/*******************************************************************************
*                                                                              *
*                           GRAPHICAL MODE LIBRARY                             *
*                                                                              *
*                       Copyright (C) 2006 Scott A. FRANCO                     *
*                                                                              *
*                              4/96 S. A. Moore                                *
*                                                                              *
* Implements the graphical mode functions on Windows. graph is upward          *
* compatible with trmlib functions.                                            *
*                                                                              *
* Proposed improvements:                                                       *
*                                                                              *
* Move(f, d, dx, dy, s, sx1, sy1, sx2, sy2)                                    *
*                                                                              *
* Moves a block of pixels from one buffer to another, or to a different place  *
* in the same buffer. Used to implement various features like intrabuffer      *
* moves, off screen image chaching, special clipping, etc.                     *
*                                                                              *
* fand, band                                                                   *
*                                                                              *
* Used with move to implement arbitrary clips usng move, above.                *
*                                                                              *
* History:                                                                     *
*                                                                              *
* graph started in 1996 as a graphical window demonstrator as a twin to        *
* ansilib, the ANSI control character based terminal mode library.             *
* In 2003, graph was upgraded to the graphical terminal standard.              *
* In 2005, graph was upgraded to include the window mangement calls, and the  *
* widget calls.                                                                *
*                                                                              *
* graph uses three different tasks. The main task is passed on to the          *
* program, and two subthreads are created. The first one is to run the         *
* display, and the second runs widgets. The Display task both isolates the     *
* user interface from any hangs or slowdowns in the main thread, and also      *
* allows the display task to be a completely regular windows message loop      *
* with class handler, that just happens to communicate all of its results      *
* back to the main thread. This solves several small problems with adapting    *
* the X Windows/Mac OS style we use to Windows style. The main and the         *
* display thread are "joined" such that they can both access the same          *
* windows. The widget task is required because of this joining, and serves to  *
* isolate the running of widgets from the main or display threads.             *
*                                                                              *
*                            BSD LICENSE INFORMATION                           *
*                                                                              *
* Copyright (C) 2020 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*     notice, this list of conditions and the following disclaimer.            *
* 2. Redistributions in binary form must reproduce the above copyright         *
*     notice, this list of conditions and the following disclaimer in the      *
*     documentation and/or other materials provided with the distribution.     *
* 3. Neither the name of the project nor the names of its contributors         *
*     may be used to endorse or promote products derived from this software    *
*     without specific prior written permission.                               *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
*******************************************************************************/

#define WINVER 0x0A00
#define _WIN32_WINNT 0xA00

#include <sys/types.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include <commctrl.h>
#include <graph.h>

#define BIT(b) (1<<b) /* set bit from bit number */
#define BITMSK(b) (~BIT(b)) /* mask out bit number */

#define MAXXD     80    /* standard terminal, 80x25 */
#define MAXYD     25
/* the "standard character" sizes are used to form a pseudo-size for desktop
   character measurements in a graphical system. */
#define STDCHRX   8
#define STDCHRY   12
#define MAXLIN    250   /* maximum length of input bufferred line */
#define MAXCON    10    /* number of screen contexts */
#define MAXTAB    50    /* total number of tabs possible per screen */
#define MAXPIC    50    /* total number of loadable pictures */
#define FHEIGHT   15    /* default font height, matches Windows "system" default */
#define FQUALITY  NONANTIALIASED_QUALITY /* font writing quality */
#define FRMTIM    0     /* handle number of framing timer */
#define PI        3.1415926535897932 /* PI to 17 digits */
#define MAXMSG    1000  /* size of input message queue */
 /* Messages defined in this module. The system message block runs from
   0x000-0x3ff, so the user mesage area starts at 0x400. */
#define UMMAKWIN  0x404 /* create standard window */
#define UMWINSTR  0x405 /* window was created */
#define UMCLSWIN  0x406 /* close window */
#define UMWINCLS  0x407 /* window was closed */
#define UMIM      0x408 /* intratask message */
#define UMEDITCR  0x409 /* edit widget sends cr */
#define UMNUMCR   0x410 /* number select widget sends cr */
/* standard file handles */
#define INPFIL    0     /* input */
#define OUTFIL    1     /* output */
#define ERRFIL    3     /* error */
#define JOYENB    FALSE /*TRUE*/ /* enable joysticks, for debugging */
/* foreground pen style */
/* FPENSTL  ps_geometric || ps_}cap_flat || ps_solid */
#define FPENSTL  (PS_GEOMETRIC | PS_ENDCAP_FLAT | PS_SOLID | PS_JOIN_MITER)
/* foreground single pixel pen style */
#define FSPENSTL  PS_SOLID
#define PACKMSG  TRUE   /* pack paint messages in queue */
#define MAXFIL   100    /* maximum open files */
#define MAXFNM   250    /* number of filename characters in buffer */

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef int (*punlink_t)(const char*);
typedef off_t (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_unlink(punlink_t nfp, punlink_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

/* screen attribute */
typedef enum {
    sablink, /* blinking text (foreground) */
    sarev,   /* reverse video */
    saundl,  /* underline */
    sasuper, /* superscript */
    sasubs,  /* subscripting */
    saital,  /* italic text */
    sabold,  /* bold text */
    sastkout /* strikeout text */
} scnatt;

/* font description entry */
typedef struct fontrec {

    char*           fn;   /* name of font */
    int             fix;  /* fixed pitch font flag */
    int             sys;  /* font is system fixed (default) */
    struct fontrec* next; /* next font in list */

} fontrec, *fontptr;

typedef enum { mdnorm, mdinvis, mdxor } mode; /* color mix modes */

/* Menu tracking. This is a mirror image of the menu we were given by the
   user. However, we can do with less information than is in the original
   tree as passed. The menu items are a linear list, since they contain
   both the menu handle && the relative number 0-n of the item, neither
   the lack of tree structure nor the order of the list matters. */
typedef struct metrec {

    struct metrec* next;   /* next entry */
    HMENU          han;    /* handle of menu entry is attached to */
    int            inx;    /* index position, 0-n, of item */
    int            onoff;  /* the item is on-off highlighted */
    int            select; /* the current on/off state of the highlight */
    struct metrec* oneof;  /* "one of" chain pointer */
    int            id;     /* user id of item */

} metrec, *metptr;

/* widget type */
typedef enum  {
    wtbutton, wtcheckbox, wtradiobutton, wtgroup, wtbackground,
    wtscrollvert, wtscrollhoriz, wtnumselbox, wteditbox,
    wtprogressbar, wtlistbox, wtdropbox, wtdropeditbox,
    wtslidehoriz, wtslidevert, wttabbar
} wigtyp;

/* widget tracking entry */
typedef struct wigrec {

    struct wigrec* next; /* next entry in list */
    HWND           han;  /* handle to widget window */
    HWND           han2; /* handle to "buddy" window */
    int            id;   /* logical id of widget */
    wigtyp         typ;  /* type of widget */
    int            siz;  /* size of slider in scroll widget, in windows terms */
    WNDPROC        wprc; /* subclassed windows procedure, converted to int */
    int            low;  /* low limit of up/down control */
    int            high; /* high limit of up/down control */
    int            enb;  /* widget is enabled */

} wigrec, *wigptr;

typedef struct scncon { /* screen context */

    HDC     bdc;         /* handle for backing bitmap */
    HGDIOBJ bhn;         /* handle for bitmap object */
    HPEN    fpen;        /* foreground pen handle */
    HBRUSH  fbrush;      /* foreground brush handle */
    HPEN    fspen;       /* foreground single pixel pen */
    int     lwidth;      /* width of lines */
    /* note that the pixel && character dimensions && positions are kept
      in parallel for both characters && pixels */
    int     maxx;        /* maximum characters in x */
    int     maxy;        /* maximum characters in y */
    int     maxxg;       /* maximum pixels in x */
    int     maxyg;       /* maximum pixels in y */
    int     curx;        /* current cursor location x */
    int     cury;        /* current cursor location y */
    int     curxg;       /* current cursor location in pixels x */
    int     curyg;       /* current cursor location in pixels y */
    int     lcurx;       /* progressive line cursor x */
    int     lcury;       /* progressive line cursor y */
    int     tcurs;       /* progressive cursor strip flip state */
    int     tcurx1;      /* progressive triangle cursor x1 */
    int     tcury1;      /* progressive triangle cursor y1 */
    int     tcurx2;      /* progressive triangle cursor x2 */
    int     tcury2;      /* progressive triangle cursor y2 */
    int     fcrgb;       /* current writing foreground color in rgb */
    int     bcrgb;       /* current writing background color in rgb */
    mode    fmod;        /* foreground mix mode */
    mode    bmod;        /* background mix mode */
    HFONT   font;        /* current font handle */
    fontptr cfont;       /* active font entry */
    int     cspc;        /* character spacing */
    int     lspc;        /* line spacing */
    int     attr;        /* set of active attributes */
    int     autof;       /* current status of scroll && wrap */
    int     tab[MAXTAB]; /* tabbing array */
    int     curv;        /* cursor visible */
    int     offx;        /* viewport offset x */
    int     offy;        /* viewport offset y */
    int     wextx;       /* window extent x */
    int     wexty;       /* window extent y */
    int     vextx;       /* viewpor extent x */
    int     vexty;       /* viewport extent y */

} scncon, *scnptr;

typedef struct pict { /* picture tracking record */

    HBITMAP han; /* handle to bitmap */
    HDC     hdc; /* handle to DC for bitmap */
    HGDIOBJ ohn; /* handle to previous object */
    int     sx;  /* size in x */
    int     sy;  /* size in y */

} pict;

/* window description */
typedef struct winrec {

    int      parlfn;          /* logical parent */
    HWND     parhan;          /* handle to window parent */
    HWND     winhan;          /* handle to window */
    HDC      devcon;          /* device context */
    scnptr   screens[MAXCON]; /* screen contexts array */
    int      curdsp;          /* index for current display screen */
    int      curupd;          /* index for current update screen */
    /* global sets. these are the global set parameters that apply to any new
      created screen buffer */
    int      gmaxx;           /* maximum x size */
    int      gmaxy;           /* maximum y size */
    int      gmaxxg;          /* size of client area in x */
    int      gmaxyg;          /* size of client area in y */
    int      gattr;           /* current attributes */
    int      gauto;           /* state of auto */
    int      gfcrgb;          /* foreground color in rgb */
    int      gbcrgb;          /* background color in rgb */
    int      gcurv;           /* state of cursor visible */
    fontptr  gcfont;          /* current font select */
    int      gfhigh;          /* current font height */
    mode     gfmod;           /* foreground mix mode */
    mode     gbmod;           /* background mix mode */
    int      goffx;           /* viewport offset x */
    int      goffy;           /* viewport offset y */
    int      gwextx;          /* window extent x */
    int      gwexty;          /* window extent y */
    int      gvextx;          /* viewpor extent x */
    int      gvexty;          /* viewport extent y */
    fontptr  fntlst;          /* list of windows fonts */
    int      fntcnt;          /* number of fonts in font list */
    int      mb1;             /* mouse assert status button 1 */
    int      mb2;             /* mouse assert status button 2 */
    int      mb3;             /* mouse assert status button 3 */
    int      mpx, mpy;        /* mouse current position */
    int      mpxg, mpyg;      /* mouse current position graphical */
    int      nmb1;            /* new mouse assert status button 1 */
    int      nmb2;            /* new mouse assert status button 2 */
    int      nmb3;            /* new mouse assert status button 3 */
    int      nmpx, nmpy;      /* new mouse current position */
    int      nmpxg, nmpyg;    /* new mouse current position graphical */
    int      linespace;       /* line spacing in pixels */
    int      charspace;       /* character spacing in pixels */
    int      curspace;        /* size of cursor, in pixels */
    int      baseoff;         /* font baseline offset from top */
    int      shift;           /* state of shift key */
    int      cntrl;           /* state of control key */
    int      fcurdwn;         /* cursor on screen flag */
    int      numjoy;          /* number of joysticks found */
    int      joy1cap;         /* joystick 1 is captured */
    int      joy2cap;         /* joystick 2 is captured */
    int      joy1xs;          /* last joystick position 1x */
    int      joy1ys;          /* last joystick position 1y */
    int      joy1zs;          /* last joystick position 1z */
    int      joy2xs;          /* last joystick position 2x */
    int      joy2ys;          /* last joystick position 2y */
    int      joy2zs;          /* last joystick position 2z */
    int      shsize;          /* display screen size x in millimeters */
    int      svsize;          /* display screen size y in millimeters */
    int      shres;           /* display screen pixels in x */
    int      svres;           /* display screen pixels in y */
    int      sdpmx;           /* display screen find dots per meter x */
    int      sdpmy;           /* display screen find dots per meter y */
    char     inpbuf[MAXLIN];  /* input line buffer */
    int      inpptr;          /* input line index */
    int      inpend;          /* input line was terminated */
    int      frmrun;          /* framing timer is running */
    MMRESULT frmhan;          /* handle for framing timer */
    struct {

       MMRESULT han; /* handle for timer */
       int      rep; /* timer repeat flag */

    } timers[10];
    int      focus;           /* screen in focus */
    pict     pictbl[MAXPIC];  /* loadable pictures table */
    int      bufmod;          /* buffered screen mode */
    HMENU    menhan;          /* handle to (main) menu */
    metptr   metlst;          /* menu tracking list */
    wigptr   wiglst;          /* widget tracking list */
    int      frame;           /* frame on/off */
    int      size;            /* size bars on/off */
    int      sysbar;          /* system bar on/off */
    int      sizests;         /* last resize status save */
    int      visible;         /* window is visible */

} winrec, *winptr;

/* Event queuing.
   Events are kept as a double linked "bubble" to ease their FIFO status. */
typedef struct eqerec {

    pa_evtrec evt;       /* event */
    struct eqerec* last; /* last list pointer */
    struct eqerec* next; /* next list pointer */

} eqerec, *eqeptr;

/* File tracking.
  Files can be passthrough to syslib, || can be associated with a window. If
  on a window, they can be output, || they can be input. In the case of
  input, the file has its own input queue, && will receive input from all
  windows that are attached to it. */
typedef struct filrec {

      FILE* sfp;  /* file pointer used to establish entry, or NULL */
      winptr win; /* associated window (if exists) */
      int inw;    /* entry is input linked to window */
      int inl;    /* this output file is linked to the input file, logical */
      eqeptr evt; /* event queue */

} filrec, *filptr;

/* Intermessage entries. These are kept to allow communication between
   tasks. */
typedef enum {

    imalert,    /* present alert */
    imqcolor,   /* query color */
    imqopen,    /* query file open */
    imqsave,    /* query file save */
    imqfind,    /* query find */
    imqfindrep, /* query find/replace */
    imqfont,    /* query font */
    imupdown,   /* up/down control */
    imwidget,  /* general purpose widget handler */

} imcode;

typedef struct imrec { /* intermessage record */

    struct imrec* next; /* next message in list */
    imcode im; /* message type */
    union { /* intermessage type */

        struct { /* imalert */

            char* alttit; /* title string pointer */
            char* altmsg; /* message string pointer */

        };
        struct { /* imqcolor */

            int clrred;   /* colors */
            int clrgreen;
            int clrblue;

        };
        struct { /* imqopen */

            char* opnfil; /* filename to open */

        };
        struct { /* imqsave */

            char* savfil; /* filename to save */

        };
        struct { /* imqfind */

            char* fndstr; /* string to find */
            int   fndopt; /* find options */
            HWND  fndhan; /* dialog window handle */

        };
        struct { /* imqfindrep */

            char* fnrsch; /* string to search for */
            char* fnrrep; /* string to replace */
            int   fnropt; /* options */
            HWND  fnrhan; /* dialog window handle */

        };
        struct { /* imqfont */

            char* fntstr; /* font char* */
            int fnteff;   /* font effects */
            int fntfr;    /* foreground red */
            int fntfg;    /* foreground green */
            int fntfb;    /* foreground blue */
            int fntbr;    /* background red */
            int fntbg;    /* background green */
            int fntbb;    /* bakcground blue */
            int fntsiz;   /* size */

        };
        struct { /* imupdown */

            int     udflg;    /* flags */
            int     udx;      /* coordinates x */
            int     udy;      /* coordinates y */
            int     udcx;     /* width */
            int     udcy;     /* height */
            HWND    udpar;    /* parent window */
            int     udid;     /* id */
            HMODULE udinst;   /* instance */
            HWND    udbuddy;  /* buddy window handle */
            int     udup;     /* upper bound */
            int     udlow;    /* lower bound */
            int     udpos;    /* control position */
            HWND    udhan;    /* returns handle to control */

        };
        struct { /* imwidget */

            char*   wigcls; /* class char* */
            char*   wigtxt; /* text label char* */
            int     wigflg; /* flags */
            int     wigx;   /* origin x */
            int     wigy;   /* origin y */
            int     wigw;   /* width */
            int     wigh;   /* height */
            HWND    wigpar; /* parent window handle */
            int     wigid;  /* widget id */
            HMODULE wigmod; /* module */
            HWND    wigwin; /* handle to widget */

        };

    };

} imrec, *imptr;

typedef enum {

    eftbful,  /* File table full */
    ejoyacc,  /* Joystick access */
    etimacc,  /* Timer access */
    efilopr,  /* Cannot perform operation on special file */
    einvscn,  /* Invalid screen number */
    einvhan,  /* Invalid handle */
    einvtab,  /* Invalid tab position */
    eatopos,  /* Cannot position text by pixel with auto on */
    eatocur,  /* Cannot position outside screen with auto on */
    eatoofg,  /* Cannot reenable auto off grid */
    eatoecb,  /* Cannot reenable auto outside screen */
    einvftn,  /* Invalid font number */
    etrmfnt,  /* Valid terminal font not found */
    eatofts,  /* Cannot resize font with auto enabled */
    eatoftc,  /* Cannot change fonts with auto enabled */
    einvfnm,  /* Invalid logical font number */
    efntemp,  /* Empty logical font */
    etrmfts,  /* Cannot size terminal font */
    etabful,  /* Too many tabs set */
    eatotab,  /* Cannot use graphical tabs with auto on */
    estrinx,  /* String index out of range */
    epicfnf,  /* Picture file not found */
    epicftl,  /* Picture filename too large */
    etimnum,  /* Invalid timer number */
    ejstsys,  /* Cannot justify system font */
    efnotwin, /* File is not attached to a window */
    ewinuse,  /* Window id in use */
    efinuse,  /* File already in use */
    einmode,  /* Input side of window in wrong mode */
    edcrel,   /* Cannot release Windows device context */
    einvsiz,  /* Invalid buffer size */
    ebufoff,  /* buffered mode not enabled */
    edupmen,  /* Menu id was duplicated */
    emennf,   /* Meny id was not found */
    ewignf,   /* Widget id was not found */
    ewigdup,  /* Widget id was duplicated */
    einvspos, /* Invalid scroll bar slider position */
    einvssiz, /* Invalid scroll bar size */
    ectlfal,  /* Attempt to create control fails */
    eprgpos,  /* Invalid progress bar position */
    estrspc,  /* Out of string space */
    etabbar,  /* Unable to create tab in tab bar */
    efildlg,  /* Unable to create file dialog */
    efnddlg,  /* Unable to create find dialog */
    efntdlg,  /* Unable to create font dialog */
    efndstl,  /* Find/replace string too long */
    einvwin,  /* Invalid window number */
    einvjye,  /* Invalid joystick event */
    ejoyqry,  /* Could not get information on joystick */
    einvjoy,  /* Invalid joystick ID */
    eclsinw,  /* Cannot directly close input side of window */
    ewigsel,  /* Widget is not selectable */
    ewigptxt, /* Cannot put text in this widget */
    ewiggtxt, /* Cannot get text from this widget */
    ewigdis,  /* Cannot disable this widget */
    estrato,  /* Cannot direct write string with auto on */
    etabsel,  /* Invalid tab select */
    enomem,   /* Out of memory */
    enoopn,   /* Cannot open file */
    einvfil,  /* File is invalid */
    eftntl,   /* font name too large */
    estrtl,   /* string too long for destination */
    enofnt,   /* Cannot create font */
    enosel,   /* Cannot select object */
    esystem   /* System consistency check */

} errcod;

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overridden by this module.
 *
 */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static punlink_t ofpunlink;
static plseek_t  ofplseek;

filptr opnfil[MAXFIL]; /* open files table */
int xltwin[MAXFIL]; /* window equivalence table */
int filwin[MAXFIL]; /* file to window equivalence table */
/* array to translate top level file ids to syslib equivalents */
int xltfil[MAXFIL];

int       fi;           /* index for files table */
int       fend;         /* end of program ordered flag */
int       fautohold;    /* automatic hold on exit flag */
char*     pgmnam;       /* program name string */
char*     trmnam;       /* program termination string */
/* These are duplicates from the windows record. They must be here because
  Windows calls us back, && the results have to be passed via a global. */
fontptr   fntlst;       /* list of windows fonts */
int       fntcnt;       /* number of fonts in font list */
pa_evtrec er;           /* event record */
int       r;            /* result holder */
int       b;            /* int result holder */
eqeptr    eqefre;       /* free event queuing entry list */
wigptr    wigfre;       /* free widget entry list */
/* message input queue */
MSG       msgque[MAXMSG];
int       msginp;       /* input pointer */
int       msgout;       /* ouput pointer */
HANDLE    msgrdy;       /* message ready event */
/* Control message queue. We send messages around for internal controls, but
  we don"t want to discard user messages to get them. So we use a separate
  queue to store control messages. */
MSG       imsgque[MAXMSG];
int       imsginp;      /* input pointer */
int       imsgout;      /* ouput pointer */
HANDLE    imsgrdy;      /* message ready event */
/* this array stores color choices from the user in the color pick dialog */
COLORREF  gcolorsav[16];
int       i;            /* index for that */
int       fndrepmsg;    /* message assignment for find/replace */
HWND      dispwin;      /* handle to display thread window */
HWND      dialogwin;    /* handle to dialog thread window */
HANDLE    threadstart;  /* thread start event handle */
DWORD     threadid;     /* dummy thread id (unused) */
HWND      mainwin;      /* handle to main thread dummy window */
int       mainthreadid; /* main thread id */
/* This block communicates with the subthread to create standard windows. */
int       stdwinflg;    /* flags */
int       stdwinx;      /* x position */
int       stdwiny;      /* y position */
int       stdwinw;      /* width */
int       stdwinh;      /* height */
HWND      stdwinpar;    /* parent */
HWND       stdwinwin;    /* window window handle */
int       stdwinj1c;    /* joystick 1 capture */
int       stdwinj2c;    /* joystick 1 capture */
/* mainlock:    int; */ /* lock for all global structures */
LPCRITICAL_SECTION mainlock; /* main task lock */
imptr     freitm;       /* intratask message free list */
int       msgcnt;       /* counter for number of message output (diagnostic) */

/* The double fault flag is set when exiting, so if we exit again, it
  is checked,  forces an immediate exit. This keeps faults from
  looping. */
int       dblflt;       /* double fault flag */

void clswin(int fn);
LRESULT CALLBACK wndproc(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK wndprocdialog(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam);
void alert(char* title, char* message);

/******************************************************************************

Print string to debug dialog

Outputs the given string into a debug dialog. Used for debug purposes.

******************************************************************************/

void diastr(char* s)

{

    int r;

    r = MessageBox(0, s, "Debug message", MB_OK);

}

/*******************************************************************************

Print string to output

Writes a string directly to the serial error file. This is useful for
diagnostics, since it will work under any thread or callback.

*******************************************************************************/

void prtstr(char *s)

{

    HANDLE hdl; /* output file handle */
    BOOL   fr;  /* int result */
    DWORD  nbw;

    hdl = GetStdHandle(STD_ERROR_HANDLE);
    fr = WriteFile(hdl, s, strlen(s), &nbw, NULL);

}

/*******************************************************************************

Print single character to error

Writes a character directly to the serial error file. This is useful for
diagnostics, since it will work under any thread or callback.

*******************************************************************************/

void prtchr(char c)

{

    HANDLE hdl; /* output file handle */
    UINT   fr;  /* int result */
    DWORD  nbw;

    hdl = GetStdHandle(STD_ERROR_HANDLE);
    fr = WriteFile(hdl, &c, 1, &nbw, NULL);

}

/*******************************************************************************

Print number

Prints a number, with the given field and radix. Used for diagnostics, and can
be commented out.

*******************************************************************************/

void prtnum(int w,  /* value to print */
            int fd, /* field width */
            int r)  /* radix */

{

    int i, j;
    int v;
    int s;

   s = FALSE; /* set not signed */
   if (r == 10 && w < 0)  { /* is negative, && decimal */

      s = TRUE; /* set signed */
      w = -w; /* remove sign */

   }
   /* find maximum digit */
   i = 1;
   if (s)  i++; /* count sign */
   do {

      v = w; /* copy value */
      for (j = 1; j <= i; j++) v = v/r; /* move down */
      if (v)  i++; /* not found, next */

   } while (v != 0);
   if (i > fd)  fd = i; /* set minimum size of number */
   if (s) prtchr('-'); /* output sign */
   for (i = 1; i<= fd; i++) { /* output digits */

      v = w; /* save word */
      for (j = 1; j <= fd - i; j++) v = v/r; /* extract digit */
      v = v%r; /* mask */
      /* convert ascii */
      if (v >= 10)  v = v+('A'-10); else v = v+'0';
      prtchr(v); /* output */

   }

}

/*******************************************************************************

Print contents of open file table

A diagnostic. Prints out all open records from the files table.

*******************************************************************************/

void prtfil(void)

{

    int i; /* index for files table */

    for (i = 0; i < MAXFIL; i++) /* traverse table */
        if (opnfil[i]) { /* print file record */

        prtstr("File: "); prtnum(i, 1, 10);
        prtstr(" Win: ");
        if (opnfil[i]->win) prtstr("yes"); else prtstr("no");
        prtstr(" Input side of: ");
        if (opnfil[i]->inw) prtstr("yes"); else prtstr("no");
        prtstr(" link to file: "); prtnum(opnfil[i]->inl, 1, 10);
        prtstr(" Queue is: ");
        if (opnfil[i]->evt) prtstr("nonempty\r\n"); else prtstr("empty\r\n");

    }

}

/*******************************************************************************

Print menu

A diagnostic. Prints out the given menu.

*******************************************************************************/

void dooff(int offset)

{

    int i;

    for (i = 1; i <= offset; i++) prtchr(' ');

}

void prtmenuelm(pa_menuptr m, int offset)

{

    while (m) { /* list entries */

        /* print menu entries */
        dooff(offset); prtstr("Onoff:  "); prtnum(m->onoff, 1, 10);
        prtstr("\r\n");
        dooff(offset); prtstr("Oneof:  "); prtnum(m->oneof, 1, 10);
        prtstr("\r\n");
        dooff(offset); prtstr("Bar:    "); prtnum(m->bar, 1, 10);
        prtstr("\r\n");
        dooff(offset); prtstr("Id:     "); prtnum(m->id, 1, 10);
        prtstr("\r\n");
        dooff(offset); prtstr("Face:   "); prtstr(m->face);
        prtstr("\r\n");
        prtstr("\r\n");
        /* if branch exists, print that list as sublist */
        if (m->branch) prtmenuelm(m->branch, offset+3);
        m = m->next; /* next entry */

    }

}

void prtmenu(pa_menuptr m)

{

    prtstr("Menu:\r\n");
    prtstr("\r\n");
    prtmenuelm(m, 0);
    prtstr("\r\n");

}

/*******************************************************************************

Print widget

A diagnostic. Prints the contents of a widget.

*******************************************************************************/

void prtwig(wigptr wp)

{

    prtstr("Window handle: ");
    prtnum((int)wp->han, 1, 16);
    prtstr(" \"buddy\" Window handle: ");
    prtnum((int)wp->han2, 1, 16);
    prtstr(" Logical id: "); prtnum(wp->id, 1, 10);
    prtstr(" Type: ");
    switch (wp->typ) { /* widget */

        case wtbutton:      prtstr("Button");
        case wtcheckbox:    prtstr("Checkbox");
        case wtradiobutton: prtstr("Radio Button");
        case wtgroup:       prtstr("Group Box");
        case wtbackground:  prtstr("Backgroun Box");
        case wtscrollvert:  prtstr("Vertical Scroll");
        case wtscrollhoriz: prtstr("Horizontal Scroll");
        case wtnumselbox:   prtstr("Number Select Box");
        case wteditbox:     prtstr("Edit Box");
        case wtprogressbar: prtstr("Progress Bar");
        case wtlistbox:     prtstr("List Box");
        case wtdropbox:     prtstr("Drop Box");
        case wtdropeditbox: prtstr("Drop Edit Box");
        case wtslidehoriz:  prtstr("Horizontal Slider");
        case wtslidevert:   prtstr("Vertical Slider");
        case wttabbar:      prtstr("Tab Bar");

    }
    if (wp->typ == wtscrollvert || wp->typ == wtscrollhoriz)
        prtstr(" Slider size: "); prtnum(wp->siz, 1, 10);

}

/*******************************************************************************

Print widget list

A diagnostic. Prints the contents of a widget list.

*******************************************************************************/

void prtwiglst(wigptr wp)

{

    prtstr("Widget list\r\n");
    prtstr("\r\n");
    while (wp) {

        prtwig(wp);
        prtstr("\r\n");
        wp = wp->next;

    }
    prtstr("\r\n");

}

/*******************************************************************************

Find string match

Finds if the give strings match, without regard to case.

*******************************************************************************/

int comps(char* d, char* s)

{

    int r; /* result save */

    r = TRUE;
    while (*d && *s && tolower(*d) == tolower(*s)) { d++; s++; }
    if (*d || *s) r = FALSE;

    return (r);

}

/*******************************************************************************

Get decimal number from string

Parses a decimal number from the given string. Allows signed numbers. Provides
a flag return that indicates overflow or invalid digit.

*******************************************************************************/

int intv(char* s, /* string containing int */
         int* err /* error occurred */
         )

{

   int r;
   char* ep;

   r = strtol(s, &ep, 10);
   *err = !!*ep;

   return (r);

}

/*******************************************************************************

Lock mutual access

Locks multiple accesses to graph globals.

No error checking is done.

*******************************************************************************/

void lockmain(void)

{

    /* int r; */

/*;prtstr("lockmain\r\n");*/
   EnterCriticalSection(mainlock); /* start exclusive access */
   /* r = waitforsingleobject(mainlock, -1) */ /* start exclusive access */
/*;if r == -1  prtstr("Lockmain: lock operation fails\cr\lf");*/

}

/*******************************************************************************

Unlock mutual access

Unlocks multiple accesses to graph globals.

No error checking is done.

*******************************************************************************/

void unlockmain(void)

{

    /* int b; */

/*;prtstr("unlockmain\r\n");*/
   LeaveCriticalSection(mainlock); /* end exclusive access */
   /* b = releasemutex(mainlock); */ /* end exclusive access */
/*;if (!b)  prtstr("Unlockmain: lock operation fails\cr\lf");*/

}

/*******************************************************************************

Write error string

This is the hook replacement for the standard syslib routine. The syslib
serial method of outputing errors won"t work in the windowed environment,
because the standard output is not connected.

The error message is output in a dialog.

*******************************************************************************/

void wrterr(char* es)

{

   /* Output in a dialog */
   alert("Runtime Error", es);

}

/*******************************************************************************

Print graph error string

Prints a string in graph specific format.

*******************************************************************************/

void grawrterr(char* es)

{

    unlockmain(); /* end exclusive access */
    prtstr("\nError: Graph: ");
    prtstr(es);
    prtstr("\n");
    lockmain(); /* resume exclusive access */

}

/*******************************************************************************

Abort module

Close open files, unlock main and exit.

*******************************************************************************/

void abort(void)

{

    int fi; /* file index */

    /* close any open windows */
    if (!dblflt)  { /* we haven"t already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files and windows */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->win)
                clswin(fi); /* close open window */

    }
    unlockmain(); /* end exclusive access */

    exit(1); /* exit */

}

/*******************************************************************************

Print error

Prints the given error in ASCII text,  aborts the program.
This needs to go to a dialog instead of the system error trap.

*******************************************************************************/

void error(errcod e)

{

    switch (e) { /* error */

        case eftbful:  grawrterr("Too many files"); break;
        case ejoyacc:  grawrterr("No joystick access available"); break;
        case etimacc:  grawrterr("No timer access available"); break;
        case einvhan:  grawrterr("Invalid file number"); break;
        case efilopr:  grawrterr("Cannot perform operation on special file"); break;
        case einvscn:  grawrterr("Invalid screen number"); break;
        case einvtab:  grawrterr("Tab position specified off screen"); break;
        case eatopos:  grawrterr("Cannot position text by pixel with auto on"); break;
        case eatocur:  grawrterr("Cannot position outside screen with auto on"); break;
        case eatoofg:  grawrterr("Cannot reenable auto off grid"); break;
        case eatoecb:  grawrterr("Cannot reenable auto outside screen"); break;
        case einvftn:  grawrterr("Invalid font number"); break;
        case etrmfnt:  grawrterr("No valid terminal font was found"); break;
        case eatofts:  grawrterr("Cannot resize font with auto enabled"); break;
        case eatoftc:  grawrterr("Cannot change fonts with auto enabled"); break;
        case einvfnm:  grawrterr("Invalid logical font number"); break;
        case efntemp:  grawrterr("Logical font number has no assigned font"); break;
        case etrmfts:  grawrterr("Cannot size terminal font"); break;
        case etabful:  grawrterr("Too many tabs set"); break;
        case eatotab:  grawrterr("Cannot set off grid tabs with auto on"); break;
        case estrinx:  grawrterr("String index out of range"); break;
        case epicfnf:  grawrterr("Picture file ! found"); break;
        case epicftl:  grawrterr("Picture filename too large"); break;
        case etimnum:  grawrterr("Invalid timer number"); break;
        case ejstsys:  grawrterr("Cannot justify system font"); break;
        case efnotwin: grawrterr("File is not attached to a window"); break;
        case ewinuse:  grawrterr("Window id in use"); break;
        case efinuse:  grawrterr("File already in use"); break;
        case einmode:  grawrterr("Input side of window in wrong mode"); break;
        case edcrel:   grawrterr("Cannot release Windows device context"); break;
        case einvsiz:  grawrterr("Invalid buffer size"); break;
        case ebufoff:  grawrterr("Buffered mode ! enabled"); break;
        case edupmen:  grawrterr("Menu id was duplicated"); break;
        case emennf:   grawrterr("Menu id was ! found"); break;
        case ewignf:   grawrterr("Widget id was ! found"); break;
        case ewigdup:  grawrterr("Widget id was duplicated"); break;
        case einvspos: grawrterr("Invalid scroll bar slider position"); break;
        case einvssiz: grawrterr("Invalid scroll bar slider size"); break;
        case ectlfal:  grawrterr("Attempt to create control fails"); break;
        case eprgpos:  grawrterr("Invalid progress bar position"); break;
        case estrspc:  grawrterr("Out of string space"); break;
        case etabbar:  grawrterr("Unable to create tab in tab bar"); break;
        case efildlg:  grawrterr("Unable to create file dialog"); break;
        case efnddlg:  grawrterr("Unable to create find dialog"); break;
        case efntdlg:  grawrterr("Unable to create font dialog"); break;
        case efndstl:  grawrterr("Find/replace string too long"); break;
        case einvwin:  grawrterr("Invalid window number"); break;
        case einvjye:  grawrterr("Invalid joystick event"); break;
        case ejoyqry:  grawrterr("Could ! get information on joystick"); break;
        case einvjoy:  grawrterr("Invalid joystick ID"); break;
        case eclsinw:  grawrterr("Cannot directly close input side of window"); break;
        case ewigsel:  grawrterr("Widget is ! selectable"); break;
        case ewigptxt: grawrterr("Cannot put text in this widget"); break;
        case ewiggtxt: grawrterr("Cannot get text from this widget"); break;
        case ewigdis:  grawrterr("Cannot disable this widget"); break;
        case estrato:  grawrterr("Cannot direct write string with auto on"); break;
        case etabsel:  grawrterr("Invalid tab select"); break;
        case enomem:   grawrterr("Out of memory"); break;
        case enoopn:   grawrterr("Cannot open file"); break;
        case einvfil:  grawrterr("File is invalid"); break;
        case eftntl:   grawrterr("Font name too large"); break;
        case estrtl:   grawrterr("String too long for destination"); break;
        case enofnt:   grawrterr("Cannot create font"); break;
        case enosel:   grawrterr("Cannot select object"); break;
        case esystem:  grawrterr("System consistency check, please contact vendor");

    }

    abort(); /* abort module */

}

/*******************************************************************************

Handle windows error

Only called if the last error variable is set. The text string for the error
is output, &&  the program halted.

*******************************************************************************/

static void winerr(void)

{

    int e;
    char *p;
    LPVOID lpMsgBuf;

    e = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, e,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf,
                  0, NULL);
    unlockmain(); /* end exclusive access */
    prtstr("\nError: Graph: Windows error: ");
    prtstr(lpMsgBuf);
    prtstr("\n");
    lockmain(); /* resume exclusive access */

    abort(); /* abort module */

}

/*******************************************************************************

Find next queue pointer

Finds the next value of a queue pointer, which wraps around at the }.

*******************************************************************************/

int next(int mi)

{

    if (mi == MAXMSG) mi = 1; /* if at end, wrap */
    else mi = mi+1; /* increment */

    return (mi); /* return result */

}

/*******************************************************************************

Place message entry in queue

Places a message into the input queue. If the queue is full, overwrites the
oldest event.

We implement a "paint packing" option here. We track if paint messages were
put into the queue, and if so, new paint messages are merged with the existing
paint message by finding the least encompassing rectangle. This can save
a lot of redraw thrashing due to paints stacking up in the queue. Windows
implements this on the main queue, but since we manage this queue ourselves,
we have to do it here.

*******************************************************************************/

/* unpack paint message */

void upackpm(WPARAM wparam, LPARAM lparam, int* x1, int* y1, int* x2, int* y2)

{

    *x1 = wparam/0x10000;
    *y1 = wparam%0x10000;
    *x2 = lparam/0x10000;
    *y2 = lparam%0x10000;

}

/* pack paint message */

void packpm(WPARAM* wparam, LPARAM* lparam, int x1, int y1, int x2, int y2)

{

    *wparam = x1*0x10000+y1;
    *lparam = x2*0x10000+y2;

}

/* find message matching type and window in queue */

int fndmsg(HWND hwnd, UINT msg)

{

    int mi; /* message index */
    int fm; /* message found */

    fm = 0; /* set no message found */
    mi = msgout; /* index next output message */
    while (mi != msginp) { /* search forward */

        /* Check message type and window handle matches */
        if (msgque[mi].message == msg && msgque[mi].hwnd == hwnd)  {

            fm = mi; /* set found message */
            mi = msginp; /* terminate search */

        } else mi = next(mi); /* next entry */

    }

    return (fm); /* return found status */

}

/* enter new message to queue */

void enter(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)

{

    BOOL b;

    /* if the queue is full, dump the oldest entry */
    if (next(msginp) == msgout) msgout = next(msgout);
    msgque[msginp].hwnd = hwnd; /* place windows handle */
    msgque[msginp].message = msg; /* place message code */
    msgque[msginp].wParam = wparam; /* place parameters */
    msgque[msginp].lParam = lparam;
    msginp = next(msginp); /* advance input pointer */
    b = SetEvent(msgrdy); /* flag message ready */
    if (!b) winerr(); /* fails */

}

void putmsg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)

{

    int b;
    int nx1, ny1, nx2, ny2;
    int ox1, oy1, ox2, oy2;
    int fm;                 /* found message */

    lockmain(); /* start exclusive access */
/* Turning on paint compression causes lost updates */
    if (msg == WM_PAINT && PACKMSG)  {

        fm = fndmsg(hwnd, msg); /* find matching message */
        if (fm)  {

            /* There is a matching paint message in the queue, fold this message
               into the existing one by finding smallest encompassing
               rectangle. */
            upackpm(wparam, lparam, &nx1, &ny1, &nx2, &ny2); /* unpack new */
            /* unpack old */
            upackpm(msgque[fm].wParam, msgque[fm].lParam, &ox1, &oy1, &ox2, &oy2);
            /* find greater bounding */
            if (nx1 < ox1) ox1 = nx1;
            if (ny1 < oy1) oy1 = ny1;
            if (nx2 > ox2) ox2 = nx2;
            if (ny2 > oy2) oy2 = ny2;
            packpm(&msgque[fm].wParam, &msgque[fm].lParam, ox1, oy1, ox2, oy2);

        } else enter(hwnd, msg, wparam, lparam); /* enter as new message */

    } else if (msg == WM_SIZE && PACKMSG) {

        fm = fndmsg(hwnd, msg); /* find matching message */
        if (fm) {

            /* We only need the latest size, so overwrite the old with new. */
            msgque[fm].hwnd = hwnd; /* place windows handle */
            msgque[fm].wParam = wparam; /* place parameters */
            msgque[fm].lParam = lparam;

        } else enter(hwnd, msg, wparam, lparam); /* enter as new message */

    } else enter(hwnd, msg, wparam, lparam); /* enter new message */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get next message from queue

Retrives the next message from the queue. Does nothing if the queue is empty.
Queue empty should be checked before calling this routine, which is indicated
by msginp == msgout.

*******************************************************************************/

void getmsg(MSG* msg)

{

    int   f; /* found message flag */
    DWORD r; /* result */

    f = FALSE; /* set no message found */
    /* It should not happen, but if we get a FALSE signal, loop waiting for
       signal, and don"t leave until we get a TRUE message. */
    do { /* wait for message */

        if (msginp == msgout && imsginp == imsgout)  {

            /* nothing in queue */
            unlockmain(); /* end exclusive access */
            r = WaitForSingleObject(msgrdy, -1); /* wait for next event */
            if (r == -1) winerr(); /* process windows error */
            b = ResetEvent(msgrdy); /* flag message ! ready */
            lockmain(); /* start exclusive access */

        }
        /* get messages from the standard queue */
        if (msginp != msgout) { /* queue ! empty */

            memcpy(msg, &msgque[msgout], sizeof(MSG)); /* get next message */
            msgout = next(msgout); /* advance output pointer */
            f = TRUE; /* found a message */

        }

    } while (!f); /* until we have a message */

}

/*******************************************************************************

Place message entry in control queue

Places a message into the control input queue. If the queue is full, overwrites
the oldest event.

*******************************************************************************/

void iputmsg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)

{

    int b;

    lockmain(); /* start exclusive access */
    /* if the queue is full, dump the oldest entry */
    if (next(imsginp) == imsgout) imsgout = next(imsgout);
    imsgque[imsginp].hwnd = hwnd; /* place windows handle */
    imsgque[imsginp].message = msg; /* place message code */
    imsgque[imsginp].wParam = wparam; /* place parameters */
    imsgque[imsginp].lParam = lparam;
    imsginp = next(imsginp); /* advance input pointer */
    b = SetEvent(imsgrdy); /* flag message ready */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get next message from control queue

Retrives the next message from the control queue. Waits if the queue is
empty. Queue empty should be checked before calling this routine, which is
indicated by imsginp == imsgout.

*******************************************************************************/

void igetmsg(MSG* msg)

{

    int   f; /* found message flag */
    DWORD r; /* result */

    f = FALSE; /* set no message found */
    /* It should ! happen, but if we get a FALSE signal, loop waiting for
       signal, && don"t leave until we get a TRUE message. */
    do { /* wait for message */

        if (imsginp == imsgout)  {

            /* nothing in queue */
            unlockmain(); /* end exclusive access */
            r = WaitForSingleObject(imsgrdy, -1); /* wait for next event */
            if (r == -1) winerr(); /* process windows error */
            b = ResetEvent(imsgrdy); /* flag message ! ready */
            lockmain(); /* start exclusive access */

        };
        /* retrive messages from the control queue first */
        if (imsginp != imsgout)  { /* queue ! empty */

            memcpy(msg, &imsgque[imsgout], sizeof(MSG)); /* get next message */
            imsgout = next(imsgout); /* advance output pointer */
            f = TRUE; /* found a message */

        }

    } while (!f); /* until we have a message */

}

/*******************************************************************************

Get intratask message entry

Gets a new intratask message, || recycles an old one.

*******************************************************************************/

void getitm(imptr* p)

{

    if (freitm) { /* free entry available, get that */

        *p = freitm; /* index that */
        freitm = freitm->next; /* gap out of free list */

    } else {

        *p = malloc(sizeof(struct imrec)); /* else get a new one */
        if (!p) error(enomem); /* no memory */

    }
    (*p)->next = NULL; /* clear next */

}

/*******************************************************************************

Put intratask message entry

Places the given intratask message into the free list.

*******************************************************************************/

void putitm(imptr p)

{

    p->next = freitm; /* push onto list */
    freitm = p;

}

/*******************************************************************************

Get file entry

Allocates && initalizes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

void getfet(filptr* fp)

{

    *fp = malloc(sizeof(filrec)); /* get new file entry */
    if (!*fp) error(enomem); /* no more memory */
    (*fp)->win = NULL; /* set no window */
    (*fp)->inw = FALSE; /* clear input window link */
    (*fp)->inl = 0; /* set no input file linked */
    (*fp)->evt = NULL; /* set no queued events */

}

/*******************************************************************************

Make file entry

Indexes a present file entry || creates a new one. Looks for a free entry
in the files table, indicated by 0. If found, that is returned, otherwise
the file table is full.

Note that the "predefined" file slots are never allocated.

*******************************************************************************/

void makfil(int* fn) /* file handle */

{

    int fi; /* index for files table */
    int ff; /* found file entry */

    /* find idle file slot (file with closed file entry) */
    ff = 0; /* clear found file */
    for (fi = ERRFIL+1; fi < MAXFIL; fi++) { /* search all file entries */

        if (!opnfil[fi]) /* found an unallocated slot */
            ff = fi; /* set found */
        else /* check if slot is allocated, but unoccupied */
            if (!opnfil[fi]->win) ff = fi; /* set found */

    }
    if (!ff) error(einvhan); /* no empty file positions found */
    if (!opnfil[ff]) getfet(&opnfil[ff]); /* get and initalize the file entry */
    *fn = ff; /* set file id number */

}

/********************************************************************************

Trim leading and trailing spaces off string.

Removes leading and trailing spaces from a given string. Since the string will
shrink by definition, no length is needed.

********************************************************************************/

static void trim(char *s)
{

    char* p;
    char* d;

    /* trim front */
    p = s;
    while (*p == ' ') p++;
    d = s;
    if (p != s) while (*d++ = *p++);

    /* trim back */
    p = s;
    while (*p && *p == ' ') p++; /* find end of string */
    if (p > s) {

        p--;
        while (p > s && *p == ' ') p--; /* back up to first non-space */
        p++; /* reindex */
        *p = 0; /* terminate */

    }

}

/*******************************************************************************

Index window from logical file number

Finds the window associated with a logical file id. The file is checked
Finds the window associated with a text file. Gets the logical top level
filenumber for the file, converts this via its top to bottom alias,
validates that an alias has been established. This effectively means the file
was opened. , the window structure assigned to the file is fetched, and
validated. That means that the file was opened as a window input || output
file.

*******************************************************************************/

winptr lfn2win(int fn)

{

   if (fn < 0 || fn >= MAXFIL)  error(einvhan); /* invalid file handle */
   if (!opnfil[fn])  error(einvhan); /* invalid handle */
   if (!opnfil[fn]->win) error(efnotwin); /* not a window file */

   return (opnfil[fn]->win); /* return windows pointer */

}

/*******************************************************************************

Index window from file

Finds the window associated with a text file. Gets the logical top level
filenumber for the file, converts this via its top to bottom alias,
validates that an alias has been established. This effectively means the file
was opened. , the window structure assigned to the file is fetched, and
validated. That means that the file was opened as a window input || output
file.

*******************************************************************************/

winptr txt2win(FILE* f)

{

   int fn;

   fn = fileno(f); /* get file number */
   if (fn < 0) error(einvfil); /* file invalid */

   return (lfn2win(fn)); /* get logical filenumber for file */

}

/*******************************************************************************

Index window from logical window

Finds the windows context record from the logical window number, with checking.

*******************************************************************************/

winptr lwn2win(int wid)

{

    int    ofn; /* output file handle */
    winptr win; /* window context pointer */

    if (wid < 0 || wid >= MAXFIL)  error(einvhan); /* error */
    ofn = xltwin[wid]; /* get the output file handle */
    win = lfn2win(ofn); /* index window context */

    return (win); /* return result */

}

/*******************************************************************************

Find logical output file from Windows handle

Finds the logical output file that corresponds to the given window handle.
This is done the stupid way now for testing, by searching, but later can be
converted to hash methods.

*******************************************************************************/

int hwn2lfn(HWND hw)

{

    int f;  /* index for file handles */
    int fn; /* resulting file number */

    fn = -1; /* set no file found */
    for (fi = 0; fi < MAXFIL; fi++) /* search output files */
        /* has an entry, has window assigned, and matches our entry */
        if (opnfil[fi] && opnfil[fi]->win && opnfil[fi]->win->winhan == hw)
            fn = fi; /* found */

   return (fn); /* return result */

}

/*******************************************************************************

Get logical file number from file

Gets the logical translated file number from a text file, && verifies it
is valid.

*******************************************************************************/

int txt2lfn(FILE* f)

{

    int fn;

    fn = fileno(f); /* get file id */
    if (fn < 0) error(einvfil); /* invalid */

    return (fn); /* return result */

}

/*******************************************************************************

Get event queuing entry

Either gets a used event queuing entry off the free list, or allocates a new
one.

*******************************************************************************/

void geteqe(eqeptr* ep)

{

   if (eqefre) { /* get used entry */

      *ep = eqefre; /* index top of list */
      eqefre = eqefre->next; /* gap out of list */

   } else {

       *ep = malloc(sizeof(eqerec));
       if (!*ep) error(enomem);

   }
   (*ep)->last = NULL; /* clear pointers */
   (*ep)->next = NULL;

}

/*******************************************************************************

Put event queuing entry

Places the given event queuing entry onto the free list.

*******************************************************************************/

void puteqe(eqeptr ep)

{

   ep->next = eqefre; /* push onto free list */
   eqefre = ep;

}

/*******************************************************************************

Get widget

Get a widget and place into the window tracking list. If a free widget entry
is available, that will be used, otherwise a new entry is allocated.

*******************************************************************************/

void getwig(winptr win, wigptr* wp)

{

    if (wigfre) { /* used entry exists, get that */

        *wp = wigfre; /* index top entry */
        wigfre = wigfre->next; /* gap out */

   } else {

       *wp = malloc(sizeof(wigrec)); /* get entry */
       if (!*wp) error(enomem); /* no memory */

   }
   (*wp)->next = win->wiglst; /* push onto list */
   win->wiglst = *wp;
   (*wp)->han = 0; /* clear handles */
   (*wp)->han2 = 0;
   (*wp)->id = 0; /* clear id */
   (*wp)->typ = wtbutton; /* arbitrary set for type */
   (*wp)->siz = 0; /* size */
   (*wp)->enb = TRUE; /* set enabled by default */

}

/*******************************************************************************

Put widget

Removes the widget from the window list, and releases the widget entry to free
list.

*******************************************************************************/

void putwig(winptr win, wigptr wp)

{

    wigptr lp;

    /* find last entry */
    if (win->wiglst == wp)  win->wiglst = win->wiglst->next; /* gap top of list */
    else { /* mid or last in list */

        lp = win->wiglst; /* index list top */
        /* find last entry */
        while (lp->next != wp && lp->next) lp = lp->next;
        if (!lp->next) error(esystem); /* should have found it */
        lp->next = wp->next; /* gap out entry */

    }
    wp->next = wigfre; /* push onto list */
    wigfre = wp;

}

/*******************************************************************************

Find widget by number

Finds the given widget by number.

*******************************************************************************/

wigptr fndwig(winptr win, int id)

{

    wigptr wp, fp;  /* widget pointer */

    wp = win->wiglst; /* index top of list */
    fp = NULL; /* clear found */
    while (wp) { /* traverse list */

        if (wp->id == id) fp = wp; /* found */
        wp = wp->next; /* next in list */

    }

    return (fp); /* return result */

}

/*******************************************************************************

Find widget by window handle

Finds the given widget by window handle.

*******************************************************************************/

wigptr fndwighan(winptr win, HWND han)

{

    wigptr wp, fp;  /* widget pointer */

    wp = win->wiglst; /* index top of list */
    fp = NULL; /* clear found */
    while (wp) { /* traverse list */

        if (wp->han == han || wp->han2 == han) fp = wp; /* found */
        wp = wp->next; /* next in list */

    }

    return (fp); /* return result */

}

/*******************************************************************************

Translate colors code

Translates an indep}ent to a terminal specific primary color code for Windows.

*******************************************************************************/

int colnum(pa_color c)

{

    int n;

    /* translate color number */
    switch (c) { /* color */

      case pa_black:     n = 0x000000; break;
      case pa_white:     n = 0xffffff; break;
      case pa_red:       n = 0x0000ff; break;
      case pa_green:     n = 0x00ff00; break;
      case pa_blue:      n = 0xff0000; break;
      case pa_cyan:      n = 0xffff00; break;
      case pa_yellow:    n = 0x00ffff; break;
      case pa_magenta:   n = 0xff00ff; break;
      case pa_backcolor: n = 0xd8e9ea; break;

   }

   return (n); /* return number */

}

/*******************************************************************************

Translate color to rgb

Translates colors to rgb, in ratioed INT_MAX form.

*******************************************************************************/

void colrgb(pa_color c, int* r, int* g, int* b)

{

    /* translate color number */
    switch (c) { /* color */

        case pa_black:     *r = 0; *g = 0; *b = 0; break;
        case pa_white:     *r = INT_MAX; *g = INT_MAX; *b = INT_MAX; break;
        case pa_red:       *r = INT_MAX; *g = 0; *b = 0; break;
        case pa_green:     *r = 0; *g = INT_MAX; *b = 0; break;
        case pa_blue:      *r = 0; *g = 0; *b = INT_MAX; break;
        case pa_cyan:      *r = 0; *g = INT_MAX; *b = INT_MAX; break;
        case pa_yellow:    *r = INT_MAX; *g = INT_MAX; *b = 0; break;
        case pa_magenta:   *r = INT_MAX; *g = 0; *b = INT_MAX; break;
        case pa_backcolor: *r = 0xea*0x800000; *g = 0xe9*0x800000;
                           *b = 0xd8*0x800000; break;

    }

}

/*******************************************************************************

Translate rgb to color

Translates rgb to color, in ratioed INT_MAX form. Works by choosing the nearest
colors.

*******************************************************************************/

void rgbcol(int r, int g, int b, pa_color* c)

{

   if (r < INT_MAX/2 && g < INT_MAX/2 && b < INT_MAX/2) *c = pa_black;
   else if (r >= INT_MAX/2 && g < INT_MAX/2 && b < INT_MAX/2) *c = pa_red;
   else if (r < INT_MAX/2 && g >= INT_MAX/2 && b < INT_MAX/2) *c = pa_green;
   else if (r < INT_MAX/2 && g < INT_MAX/2 && b >= INT_MAX/2) *c = pa_blue;
   else if (r < INT_MAX/2 && g >= INT_MAX/2 && b >= INT_MAX/2) *c = pa_cyan;
   else if (r >= INT_MAX/2 && g >= INT_MAX/2 && b < INT_MAX/2) *c = pa_yellow;
   else if (r >= INT_MAX/2 && g < INT_MAX/2 && b >= INT_MAX/2) *c = pa_magenta;
   else if (r >= INT_MAX/2 && g >= INT_MAX/2 && b >= INT_MAX/2) *c = pa_white;
   else error(esystem); /* should have been one of those */

}

/*******************************************************************************

Translate rgb to windows color

Translates a ratioed INT_MAX graph color to the windows form, which is a 32
bit word with blue, green && red bytes.

*******************************************************************************/

int rgb2win(int r, int g, int b)

{

   return ((b/8388608)*65536+(g/8388608)*256+(r/8388608));

}

/*******************************************************************************

Translate windows color to rgb color

Translates a windows int color to our ratioed INT_MAX rgb color.

*******************************************************************************/

void win2rgb(int wc, int* r, int* g, int* b)

{

   *r = wc && 0xff * 0x800000; /* get red value */
   *g = wc / 256 && 0xff * 0x800000; /* get greeen value */
   *b = wc / 65536 && 0xff * 0x800000; /* get blue value */

}

/*******************************************************************************

Check in display mode

Checks if the current update screen is also the current display screen. Returns
TRUE if so. If the screen is in display, it means that all of the actions to
the update screen should also be reflected on the real screen.

*******************************************************************************/

int indisp(winptr win)

{

    return (win->curupd == win->curdsp);

}

/*******************************************************************************

Clear screen buffer

Clears the entire screen buffer to spaces with the current colors and
attributes.

*******************************************************************************/

void clrbuf(winptr win, scnptr sc)

{

    RECT   r;  /* rectangle */
    HBRUSH hb; /* handle to brush */
    int    b;  /* return value */

    r.left = 0; /* set all client area for rectangle clear */
    r.top = 0;
    r.right = win->gmaxxg;
    r.bottom = win->gmaxyg;
    hb = CreateSolidBrush(sc->bcrgb); /* get a brush for background */
    if (!hb) winerr(); /* process error */
    /* clear buffer surface */
    b = FillRect(sc->bdc, &r, hb);
    if (!b) winerr(); /* process error */
    b = DeleteObject(hb); /* free the brush */
    if (!b) winerr(); /* process error */

}

/*******************************************************************************

Clear window

Clears the entire window to spaces with the current colors && attributes.

*******************************************************************************/

void clrwin(winptr win)

{

    RECT   r;  /* rectangle */
    HBRUSH hb; /* handle to brush */
    int    b;   /* return value */

    r.left = 0; /* set all client area for rectangle clear */
    r.top = 0;
    r.right = win->gmaxxg;
    r.bottom = win->gmaxyg;
    hb = CreateSolidBrush(win->gbcrgb); /* get a brush for background */
    if (!hb) winerr(); /* process error */
    /* clear buffer surface */
    b = FillRect(win->devcon, &r, hb);
    if (!b) winerr(); /* process error */
    b = DeleteObject(hb); /* free the brush */
    if (!b) winerr(); /* process error */

}

/*******************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

int icurbnd(scnptr sc)

{

   return (sc->curx >= 1 && sc->curx <= sc->maxx &&
           sc->cury >= 1 && sc->cury <= sc->maxy);

}

int curbnd(FILE* f)

{

    winptr win; /* windows record pointer */
    int    cb;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    cb = icurbnd(win->screens[win->curupd]);
    unlockmain(); /* end exclusive access */

    return (cb);

}

/*******************************************************************************

Set cursor visable

Makes the cursor visible.

*******************************************************************************/

void curon(winptr win)

{

    int b;

    if (!win->fcurdwn && win->screens[win->curdsp]->curv &&
        icurbnd(win->screens[win->curdsp]) && win->focus)  {

        /* cursor not already down, cursor visible, cursor in bounds, screen
           in focus */
        b = ShowCaret(win->winhan); /* show the caret */
        if (!b) winerr(); /* process error */
        win->fcurdwn = TRUE; /* set cursor on screen */

    }

}

/*******************************************************************************

Set cursor invisible

Makes the cursor invisible.

*******************************************************************************/

void curoff(winptr win)

{

    int b;

    if (win->fcurdwn) { /* cursor visable */

        b = HideCaret(win->winhan); /* hide the caret */
        if (!b) winerr(); /* process error */
        win->fcurdwn = FALSE; /* set cursor ! on screen */

    }

}

/*******************************************************************************

Set cursor status

Changes the current cursor status. If the cursor is out of bounds, or not
set as visible, it is set off. Otherwise, it is set on. Used to change status
of cursor after position && visible status events. Acts as a combination of
curon and curoff routines.

*******************************************************************************/

void cursts(winptr win)

{

    int b;

    if (win->screens[win->curdsp]->curv && icurbnd(win->screens[win->curdsp]) &&
        win->focus) {

        /* cursor should be visible */
        if (!win->fcurdwn) { /* ! already down */

            /* cursor ! already down, cursor visible, cursor in bounds */
            b = ShowCaret(win->winhan); /* show the caret */
            if (!b) winerr(); /* process error */
            win->fcurdwn = TRUE; /* set cursor on screen */

        }

    } else {

         /* cursor should ! be visible */
        if (win->fcurdwn) { /* cursor visable */

            b = HideCaret(win->winhan); /* hide the caret */
            if (!b) winerr(); /* process error */
            win->fcurdwn = FALSE; /* set cursor ! on screen */

        }

    }

}

/*******************************************************************************

Position cursor

Positions the cursor (caret) image to the right location on screen, and handles
the visible or invisible status of that.

*******************************************************************************/

void setcur(winptr win)

{

    int b;

    /* check cursor in bounds and visible, and window has focus */
    if (icurbnd(win->screens[win->curupd]) && win->focus) {

        /* set to bottom of character bounding box */
        b = SetCaretPos(win->screens[win->curdsp]->curxg-1,
                        win->screens[win->curdsp]->curyg-1+win->linespace-3);
        /* setcaret position is always returning an error, even when correct */
        /* if (!b) winerr(); */ /* process error */

    }
    cursts(win); /* process cursor status change */

}

/*******************************************************************************

Change cursor

Changes the cursor size. If the cursor is in display,  it is destroyed
and remade to the new cursor size. Otherwise, it is left alone, && will be
created on the next focus event.

*******************************************************************************/

void chgcur(winptr win)

{

    int b; /* return value */

    if (win->focus) { /* change it */

        b = DestroyCaret(); /* remove text cursor */
        if (!b) winerr(); /* process error */
        b = CreateCaret(win->winhan, 0, win->curspace, 3); /* activate caret */
        if (!b) winerr(); /* process error */
        win->fcurdwn = FALSE; /* set cursor ! down */
        setcur(win); /* replace it */

    }

}

/*******************************************************************************

Create font from current attributes

Creates a font using the attrbutes in the current update screen, and sets the
metrics for the font.

*******************************************************************************/

void newfont(winptr win)

{

    int        w; /* weight temp */
    int        h; /* height temp */
    TEXTMETRIC tm; /* text metric structure */
    int        r; /* result holder */
    HGDIOBJ    sf; /* system fixed font object */
    int        b;
    int        attrc;
    scnptr     sc;
    HGDIOBJ    rv;


    sc = win->screens[win->curupd];
    if (sc->font) { /* there is a font */

       /* get the current font out of the DCs */
       sf = GetStockObject(SYSTEM_FIXED_FONT);
       if (!sf) winerr(); /* process windows error */
       rv = SelectObject(sc->bdc, sf);
       if (rv == HGDI_ERROR) winerr();
       if (indisp(win)) {

            rv = SelectObject(win->devcon, sf);
            if (rv == HGDI_ERROR) winerr();

       }
       /* this indicates an error when there is none */
       /* if ! DeleteObject(font)  winerr(); */ /* delete old font */
       b = DeleteObject(sc->font); /* delete old font */

    }
    if (sc->cfont->sys) { /* select as system font */

        /* select the system font */
        sf = GetStockObject(SYSTEM_FIXED_FONT);
        if (!sf) winerr(); /* process windows error */
        rv = SelectObject(sc->bdc, sf);
        if (rv == HGDI_ERROR) winerr();
        /* select to screen dc */
        if (indisp(win)) {

            rv = SelectObject(win->devcon, sf); /* process error */
            if (rv == HGDI_ERROR) winerr();

        }

    } else {

        attrc = win->screens[win->curupd]->attr; /* copy attribute */
        if (BIT(sabold) & attrc) w = FW_BOLD; else w = FW_REGULAR;
        /* set normal height || half height for subscript/superscript */
        if (BIT(sasuper) & attrc || BIT(sasubs) & attrc)
            h = trunc(win->gfhigh*0.75); else h = win->gfhigh;
        sc->font = CreateFont(h, 0, 0, 0, w, BIT(saital) & attrc,
                          BIT(saundl) & sc->attr, BIT(sastkout) & sc->attr, ANSI_CHARSET,
                          OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
                          FQUALITY, DEFAULT_PITCH,
                          sc->cfont->fn);
        if (!win->screens[win->curupd]->font) winerr(); /* process windows error */
        /* select to buffer DC */
        rv = SelectObject(win->screens[win->curupd]->bdc, sc->font);
        if (rv == HGDI_ERROR) winerr();
        /* select to screen DC */
        if (indisp(win)) {

            rv = SelectObject(win->devcon, sc->font); /* process error */
            if (rv == HGDI_ERROR) winerr();

        }

    }
    b = GetTextMetrics(win->screens[win->curupd]->bdc, &tm); /* get the standard metrics */
    if (!b) winerr(); /* process windows error */
    /* Calculate line spacing */
    win->linespace = tm.tmHeight;
    win->screens[win->curupd]->lspc = win->linespace;
    /* calculate character spacing */
    win->charspace = tm.tmMaxCharWidth;
    /* set cursor width */
    win->curspace = tm.tmAveCharWidth;
    win->screens[win->curupd]->cspc = win->charspace;
    /* calculate baseline offset */
    win->baseoff = win->linespace-tm.tmDescent-1;
    /* change cursor to match new font/size */
    if (indisp(win)) chgcur(win);

}

/*******************************************************************************

Restore screen

Updates all the buffer && screen parameters from the display screen to the
terminal.

*******************************************************************************/

void restore(winptr win,   /* window to restore */
             int    whole) /* whole or part window */

{

    int b;        /* return value */
    int r;        /* return value */
    RECT cr, cr2; /* client rectangle */
    HGDIOBJ oh;   /* old pen */
    HBRUSH hb;    /* handle to brush */
    SIZE s;       /* size holder */
    int x, y;     /* x and y coordinates */
    scnptr sc;

    sc = win->screens[win->curdsp];
    if (win->bufmod && win->visible)  { /* buffered mode is on, && visible */

        curoff(win); /* hide the cursor for drawing */
        /* set colors && attributes */
        if (BIT(sarev) & sc->attr)  { /* reverse */

            r = SetBkColor(win->devcon, sc->fcrgb);
            if (r == -1) winerr(); /* process windows error */
            r = SetTextColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */

        } else {

            r = SetBkColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */
            r = SetTextColor(win->devcon, sc->fcrgb);
            if (r == -1) winerr(); /* process windows error */

        }
        /* select any viewport offset to display */
        b = SetViewportOrgEx(win->devcon, sc->offx, sc->offy, NULL);
        if (!b)  winerr(); /* process windows error */
        /* select the extents */
        b = SetWindowExtEx(win->devcon, sc->wextx, sc->wexty, &s);
        /* if (!b) winerr(); */ /* process windows error */
        b = SetViewportExtEx(win->devcon, sc->vextx, sc->vexty, &s);
        if (!b)  winerr(); /* process windows error */
        oh = SelectObject(win->devcon, sc->font); /* select font to display */
        if (oh == HGDI_ERROR) winerr();
        oh = SelectObject(win->devcon, sc->fpen); /* select pen to display */
        if (oh == HGDI_ERROR) winerr();
        if (whole) { /* get whole client area */

            b = GetClientRect(win->winhan, &cr);
            if (!b) winerr(); /* process windows error */

        } else /* get only update area */
            b = GetUpdateRect(win->winhan, &cr, FALSE);
        /* validate it so windows won"t send multiple notifications */
        b = ValidateRgn(win->winhan, NULL); /* validate region */
        if (cr.left != 0 || cr.top != 0 || cr.right != 0 || cr.bottom != 0) {
            /* area is ! NULL */

            /* convert to device coordinates */
            cr.left = cr.left+sc->offx;
            cr.top = cr.top+sc->offy;
            cr.right = cr.right+sc->offx;
            cr.bottom = cr.bottom+sc->offy;
            /* clip update rectangle to buffer */
            if (cr.left <= win->gmaxxg || cr.bottom <= win->gmaxyg)  {

                /* It"s within the buffer. Now clip the right && bottom. */
                x = cr.right; /* copy right && bottom sides */
                y = cr.bottom;
                if (x > win->gmaxxg) x = win->gmaxxg;
                if (y > win->gmaxyg) y = win->gmaxyg;
                /* copy backing bitmap to screen */
                b = BitBlt(win->devcon, cr.left, cr.top, x-cr.left+1,
                           y-cr.top+1, sc->bdc, cr.left, cr.top, SRCCOPY);

            }
            /* Now fill the right && left sides of the client beyond the
               bitmap. */
           hb = CreateSolidBrush(sc->bcrgb); /* get a brush for background */
           if (hb == 0) winerr(); /* process windows error */
           /* check right side fill */
           cr2 = cr; /* copy update rectangle */
           /* subtract overlapping space */
           if (cr2.left <= win->gmaxxg) cr2.left = win->gmaxxg;
           if (cr2.left <= cr2.right) /* still has width */
                b = FillRect(win->devcon, &cr2, hb);
           /* check bottom side fill */
           cr2 = cr; /* copy update rectangle */
           /* subtract overlapping space */
           if (cr2.top <= win->gmaxyg) cr2.top = win->gmaxyg;
           if (cr2.top <= cr2.bottom) /* still has height */
                b = FillRect(win->devcon, &cr2, hb);
            b = DeleteObject(hb); /* free the brush */
            if (!b)  winerr(); /* process windows error */

        }
        setcur(win); /* show the cursor */

    }

}

/*******************************************************************************

Display window

Presents a window, and sends it a first paint message. Used to process the
delayed window display function.

*******************************************************************************/

void winvis(winptr win)

{

    int    b;   /* int result holder */
    winptr par; /* parent window pointer */

    /* If we are making a child window visible, we have to also force its
       parent visible. This is recursive all the way up. */
    if (win->parlfn) {

        par = lfn2win(win->parlfn); /* get parent data */
        if (!par->visible) winvis(par); /* make visible if ! */

    }
    unlockmain(); /* end exclusive access */
    /* present the window */
    b = ShowWindow(win->winhan, SW_SHOWDEFAULT);
    /* send first paint message */
    b = UpdateWindow(win->winhan);
    lockmain(); /* start exclusive access */
    win->visible = TRUE; /* set now visible */
    restore(win, TRUE); /* restore window */

}

/*******************************************************************************

Initalize screen

Clears all the parameters in the present screen context. Also, the backing
buffer bitmap is created and cleared to the present colors.

*******************************************************************************/

void iniscn(winptr win, scnptr sc)

{

    int      i, x;
    HBITMAP  hb;
    LOGBRUSH lb;
    int      r;
    HGDIOBJ  rv;

    sc->maxx = win->gmaxx; /* set character dimensions */
    sc->maxy = win->gmaxy;
    sc->maxxg = win->gmaxxg; /* set pixel dimensions */
    sc->maxyg = win->gmaxyg;
    sc->curx = 1; /* set cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    /* We set the progressive cursors to the origin, which is pretty much
       arbitrary. Nobody should do progressive figures having never done
       a previous full figure. */
    sc->lcurx = 1;
    sc->lcury = 1;
    sc->tcurs = FALSE; /* set strip flip cursor state */
    sc->tcurx1 = 1;
    sc->tcury1 = 1;
    sc->tcurx2 = 1;
    sc->tcury2 = 1;
    sc->fcrgb = win->gfcrgb; /* set colors && attributes */
    sc->bcrgb = win->gbcrgb;
    sc->attr = win->gattr;
    sc->autof = win->gauto; /* set auto scroll && wrap */
    sc->curv = win->gcurv; /* set cursor visibility */
    sc->lwidth = 1; /* set single pixel width */
    sc->font = 0; /* set no font active */
    sc->cfont = win->gcfont; /* set current font */
    sc->fmod = win->gfmod; /* set mix modes */
    sc->bmod = win->gbmod;
    sc->offx = win->goffx; /* set viewport offset */
    sc->offy = win->goffy;
    sc->wextx = win->gwextx; /* set extents */
    sc->wexty = win->gwexty;
    sc->vextx = win->gvextx;
    sc->vexty = win->gvexty;
    /* create a matching device context */
    sc->bdc = CreateCompatibleDC(win->devcon);
    if (!sc->bdc) winerr(); /* process windows error */
    /* create a bitmap for that */
    hb = CreateCompatibleBitmap(win->devcon, win->gmaxxg, win->gmaxyg);
    if (!hb) winerr(); /* process windows error */
    sc->bhn = SelectObject(sc->bdc, hb); /* select bitmap into dc */
    if (sc->bhn == HGDI_ERROR) winerr(); /* process windows error */
    newfont(win); /* create font for buffer */
    /* set non-braindamaged stretch mode */
    r = SetStretchBltMode(win->screens[win->curupd]->bdc, HALFTONE);
    if (!r) winerr(); /* process windows error */
    /* set pen to foreground */
    lb.lbStyle = BS_SOLID;
    lb.lbColor = sc->fcrgb;
    lb.lbHatch = 0;
    sc->fpen = ExtCreatePen(FPENSTL, sc->lwidth, &lb, 0, NULL);
    if (!sc->fpen) winerr(); /* process windows error */
    rv = SelectObject(sc->bdc, sc->fpen);
    if (rv == HGDI_ERROR) error(enosel);
    /* set brush to foreground */
    sc->fbrush = CreateSolidBrush(sc->fcrgb);
    if (!sc->fbrush) winerr(); /* process windows error */
    /* remove fills */
    rv = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
    if (rv == HGDI_ERROR) error(enosel);
    /* set single pixel pen to foreground */
    sc->fspen = CreatePen(FSPENSTL, 1, sc->fcrgb);
    if (!sc->fspen) winerr(); /* process windows error */
    /* set colors && attributes */
    if (BIT(sarev) & sc->attr) { /* reverse */

        r = SetBkColor(sc->bdc, sc->fcrgb);
        if (r == -1) winerr(); /* process windows error */
        r = SetTextColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */

    } else {

        r = SetBkColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */
        r = SetTextColor(sc->bdc, sc->fcrgb);
        if (r == -1) winerr(); /* process windows error */

    }
    clrbuf(win, sc); /* clear screen buffer with that */
    /* set up tabbing to be on each 8th position */
    i = 9; /* set 1st tab position */
    x = 1; /* set 1st tab slot */
    while (i < sc->maxx && x < MAXTAB) {

        sc->tab[x] = (i-1)*win->charspace+1;  /* set tab */
        i = i+8; /* next tab */
        x = x+1;

    }

}

/*******************************************************************************

Disinitalize screen

Removes all data for the given screen buffer.

Note: The major objects are cleaned, but the minor ones need to be as well.

*******************************************************************************/

void disscn(winptr win, scnptr sc)

{

    /* need to do disposals here */

}

/*******************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

void iclear(winptr win)

{

    if (win->bufmod) clrbuf(win, win->screens[win->curupd]); /* clear the screen buffer */
    win->screens[win->curupd]->curx = 1; /* set cursor at home */
    win->screens[win->curupd]->cury = 1;
    win->screens[win->curupd]->curxg = 1;
    win->screens[win->curupd]->curyg = 1;
    if (indisp(win)) { /* also process to display */

        if (win->bufmod) {

            /* in buffered mode, we just copy the now cleared buffer to the
               main screen */
            setcur(win); /* set cursor on screen */
            restore(win, TRUE); /* copy to real screen */

        } else clrwin(win); /* clear window directly */

    }

}

/*******************************************************************************

Scroll screen

Scrolls the ANSI terminal screen by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source && destination rectangles, && use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer,  restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

void iscrollg(winptr win, int x, int y)

{

    int dx, dy, dw, dh; /* destination coordinates && sizes */
    int sx, sy;         /* destination coordinates */
    int b;              /* result */
    HBRUSH hb;          /* handle to brush */
    RECT frx, fry;      /* fill rectangles */

    /* scroll would result in complete clear, do it */
    if (x <= -win->gmaxxg || x >= win->gmaxxg ||
        y <= -win->gmaxyg || y >= win->gmaxyg)
        iclear(win); /* clear the screen buffer */
    else { /* scroll */

       /* set y movement */
       if (y >= 0)  { /* move up */

          sy = y; /* from y lines down */
          dy = 0; /* move to top of screen */
          dh = win->gmaxyg-y; /* height minus lines to move */
          fry.left = 0; /* set fill to y lines at bottom */
          fry.right = win->gmaxxg;
          fry.top = win->gmaxyg-y;
          fry.bottom = win->gmaxyg;

       } else { /* move down */

          sy = 0; /* from top */
          dy = abs(y); /* move to y lines down */
          dh = win->gmaxyg-abs(y); /* height minus lines to move */
          fry.left = 0; /* set fill to y lines at top */
          fry.right = win->gmaxxg;
          fry.top = 0;
          fry.bottom = abs(y);

       }
       /* set x movement */
       if (x >= 0) { /* move text left */

          sx = x; /* from x characters to the right */
          dx = 0; /* move to left side */
          dw = win->gmaxxg-x; /* width - x characters */
          /* set fill x character collums at right */
          frx.left = win->gmaxxg-x;
          frx.right = win->gmaxxg;
          frx.top = 0;
          frx.bottom = win->gmaxyg;

       } else { /* move text right */

          sx = 0; /* from x left */
          dx = abs(x); /* move from left side */
          dw = win->gmaxxg-abs(x); /* width - x characters */
          /* set fill x character collums at left */
          frx.left = 0;
          frx.right = abs(x);
          frx.top = 0;
          frx.bottom = win->gmaxyg;

       }
       if (win->bufmod) { /* apply to buffer */

          b = BitBlt(win->screens[win->curupd]->bdc, dx, dy, dw, dh,
                     win->screens[win->curupd]->bdc, sx, sy, SRCCOPY);
          if (!b) winerr(); /* process windows error */
          /* get a brush for background */
          hb = CreateSolidBrush(win->screens[win->curupd]->bcrgb);
          if (!hb) winerr(); /* process windows error */
          /* fill vacated x */
          if (x) if (!FillRect(win->screens[win->curupd]->bdc, &frx, hb)) winerr();
          /* fill vacated y */
          if (y) if (!FillRect(win->screens[win->curupd]->bdc, &fry, hb)) winerr();
          b = DeleteObject(hb); /* free the brush */
          if (!b) winerr; /* process windows error */

       } else {

          b = BitBlt(win->devcon, dx, dy, dw, dh, win->devcon, sx, sy, SRCCOPY);
          if (!b) winerr(); /* process windows error */
          /* get a brush for background */
          hb = CreateSolidBrush(win->gbcrgb);
          if (!hb) winerr(); /* process windows error */
          /* fill vacated x */
          if (x) if (!FillRect(win->devcon, &frx, hb)) winerr();
          /* fill vacated y */
          if (y) if (!FillRect(win->devcon, &fry, hb)) winerr();
          b = DeleteObject(hb); /* free the brush */
          if (!b) winerr; /* process windows error */

       }

    }
    if (indisp(win) && win->bufmod)
        restore(win, TRUE); /* move buffer to screen */

}

void scrollg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iscrollg(win, x, y); /* process scroll */
    unlockmain(); /* end exclusive access */

}

void scroll(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iscrollg(win, x*win->charspace, y*win->linespace); /* process scroll */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Position cursor

Moves the cursor to the specified x && y location.

*******************************************************************************/

void icursor(winptr win, int x, int y)

{

    if (x != win->screens[win->curupd]->curx || y != win->screens[win->curupd]->cury) {

        win->screens[win->curupd]->cury = y; /* set new position */
        win->screens[win->curupd]->curx = x;
        win->screens[win->curupd]->curxg = (x-1)*win->charspace+1;
        win->screens[win->curupd]->curyg = (y-1)*win->linespace+1;
        if (!icurbnd(win->screens[win->curupd]) && win->screens[win->curupd]->autof)
            error(eatocur); /* bad cursor position with auto */
        if (indisp(win)) setcur(win); /* set cursor on screen */

    }

}

void cursor(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    icursor(win, x, y); /* position cursor */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Position cursor graphical

Moves the cursor to the specified x && y location in pixels.

*******************************************************************************/

void icursorg(winptr win, int x, int y)

{

    if (win->screens[win->curupd]->autof)
        error(eatopos); /* cannot perform with auto on */
    if (x != win->screens[win->curupd]->curxg || y != win->screens[win->curupd]->curyg)  {

         win->screens[win->curupd]->curyg = y; /* set new position */
         win->screens[win->curupd]->curxg = x;
         win->screens[win->curupd]->curx = x / win->charspace+1;
         win->screens[win->curupd]->cury = y / win->linespace+1;
         if (indisp(win)) setcur(win); /* set cursor on screen */

    }

}

void cursorg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    icursorg(win, x, y); /* position cursor */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find character baseline

Returns the offset, from the top of the current fonts character bounding box,
to the font baseline. The baseline is the line all characters rest on.

*******************************************************************************/

int baseline(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->baseoff; /* return current line spacing */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int maxx(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->gmaxx; /* set maximum x */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int maxy(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->gmaxy; /* set maximum y */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Return maximum x dimension graphical

Returns the maximum x dimension, which is the width of the client surface in
pixels.

*******************************************************************************/

int maxxg(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->gmaxxg; /* set maximum x */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Return maximum y dimension graphical

Returns the maximum y dimension, which is the height of the client surface in
pixels.

*******************************************************************************/

int maxyg(FILE* f)

{

    winptr win; /* windows record pointer */
    int    r;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    r = win->gmaxyg; /* set maximum y */
    unlockmain(); /* end exclusive access */

    return (r);

}

/*******************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void ihome(winptr win)

{

    win->screens[win->curdsp]->curx = 1; /* set cursor at home */
    win->screens[win->curdsp]->cury = 1;
    win->screens[win->curdsp]->curxg = 1;
    win->screens[win->curdsp]->curyg = 1;
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void home(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ihome(win);
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

void iup(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd];
    /* check ! top of screen */
    if (sc->cury > 1) {

        /* update position */
        sc->cury = sc->cury-1;
        sc->curyg = sc->curyg-win->linespace;

    /* check auto mode */
    } else if (sc->autof)
        iscrollg(win, 0*win->charspace, -1*win->linespace); /* scroll up */
    /* check won't overflow */
    else if (sc->cury > -INT_MAX) {

        /* set new position */
        sc->cury = sc->cury-1;
        sc->curyg = sc->curyg-win->linespace;

    }
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void up(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iup(win); /* move up */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

void idown(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd];
    /* check ! bottom of screen */
    if (sc->cury < sc->maxy) {

       sc->cury = sc->cury+1; /* update position */
       sc->curyg = sc->curyg+win->linespace;

    /* check auto mode */
    } else if (sc->autof)
       iscrollg(win, 0*win->charspace, +1*win->linespace); /* scroll down */
    else if (sc->cury < INT_MAX) {

       sc->cury = sc->cury+1; /* set new position */
       sc->curyg = sc->curyg+win->linespace;

    }
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void down(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    idown(win); /* move cursor down */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by INT_MAX.

*******************************************************************************/

void ileft(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd];

    /* check ! extreme left */
    if (sc->curx > 1) {

       sc->curx = sc->curx-1; /* update position */
       sc->curxg = sc->curxg-win->charspace;

    } else { /* wrap cursor motion */

       if (sc->autof) { /* autowrap is on */

          iup(win); /* move cursor up one line */
          sc->curx = sc->maxx; /* set cursor to extreme right */
          sc->curxg = sc->maxxg-win->charspace;

       } else
          /* check won"t overflow */
          if (sc->curx > -INT_MAX) {

          sc->curx = sc->curx-1; /* update position */
          sc->curxg = sc->curxg-win->charspace;

       }

    }
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void left(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ileft(win); /* move cursor left */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

void iright(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd];

    /* check not at extreme right */
    if (sc->curx < MAXXD) {

       sc->curx = sc->curx+1; /* update position */
       sc->curxg = sc->curxg+win->charspace;

    } else { /* wrap cursor motion */

       if (sc->autof)  { /* autowrap is on */

          idown(win); /* move cursor up one line */
          sc->curx = 1; /* set cursor to extreme left */
          sc->curxg = 1;

       /* check won"t overflow */
       } else if (sc->curx < INT_MAX)  {

          sc->curx = sc->curx+1; /* update position */
          sc->curxg = sc->curxg+win->charspace;

       }

    }
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void right(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iright(win); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

*******************************************************************************/

void itab(winptr win)

{

    int i;
    int x;
    scnptr sc;

    sc = win->screens[win->curupd];

    /* first, find if next tab even exists */
    x = sc->curxg+1; /* get just after the current x position */
    if (x < 1)  x = 1; /* don"t bother to search to left of screen */
    /* find tab || } of screen */
    i = 1; /* set 1st tab position */
    while (x > sc->tab[i] && sc->tab[i] && i < MAXTAB) i++;
    if (sc->tab[i] && x < sc->tab[i]) { /* not off right of tabs */

       sc->curxg = sc->tab[i]; /* set position to that tab */
       sc->curx = sc->curxg/win->charspace+1;
       if (indisp(win)) setcur(win); /* set cursor on screen */

    }

}

/*******************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

*******************************************************************************/

void blink(FILE* f, int e)

{

   /* no capability */

}

/*******************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void ireverse(winptr win, int e)

{

    int r; /* return value */
    scnptr sc;

    sc = win->screens[win->curupd];
    if (e) { /* reverse on */

        sc->attr |= BIT(sarev); /* set attribute active */
        win->gattr |= BIT(sarev);
        /* activate in buffer */
        r = SetTextColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */
        r = SetBkColor(sc->bdc, sc->fcrgb);
        if (r == -1) winerr(); /* process windows error */
        if (indisp(win)) { /* activate on screen */

            /* reverse the colors */
            r = SetTextColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */
            r = SetBkColor(win->devcon, sc->fcrgb);
            if (r == -1) winerr(); /* process windows error */

        }

    } else { /* turn it off */

        sc->attr &= ~BIT(sarev); /* set attribute inactive */
        win->gattr &= ~BIT(sarev);
        /* activate in buffer */
        r = SetTextColor(sc->bdc, sc->fcrgb);
        if (r == -1) winerr(); /* process windows error */
        r = SetBkColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */
        if (indisp(win)) { /* activate on screen */

            /* set normal colors */
            r = SetTextColor(win->devcon, sc->fcrgb);
            if (r == -1) winerr(); /* process windows error */
            r = SetBkColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */

        }

    }

}

void reverse(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ireverse(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.
This is ! implemented, but could be done by drawing a line under each
character drawn.

*******************************************************************************/

void iunderline(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd];
    if (e) { /* underline on */

        sc->attr |= BIT(saundl); /* set attribute active */
        win->gattr |= BIT(saundl);

    } else { /* turn it off */

        sc->attr &= ~BIT(saundl); /* set attribute inactive */
        win->gattr &= ~BIT(saundl);

    }
    newfont(win); /* select new font */

}

void underline(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iunderline(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void isuperscript(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd];
    if (e)  { /* superscript on */

        sc->attr |= BIT(sasuper); /* set attribute active */
        win->gattr |= BIT(sasuper);

    } else { /* turn it off */

        sc->attr &= ~BIT(sasuper); /* set attribute inactive */
        win->gattr &= ~BIT(sasuper);

    }
    newfont(win); /* select new font */

}

void superscript(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isuperscript(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void isubscript(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd];
    if (e) { /* subscript on */

        sc->attr |= BIT(sasubs); /* set attribute active */
        win->gattr |= BIT(sasubs);

    } else { /* turn it off */

        sc->attr &= ~BIT(sasubs); /* set attribute inactive */
        win->gattr &= ~BIT(sasubs);

    }
    newfont(win); /* activate new font with that */

}

void subscript(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isubscript(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

Italic is causing problems with fixed mode on some fonts, && Windows does not
seem to want to share with me just what the TRUE width of an italic font is
(without taking heroic measures like drawing && testing pixels). So we disable
italic on fixed fonts.

*******************************************************************************/

void iitalic(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd];
    if (e) { /* italic on */

        sc->attr |= BIT(saital); /* set attribute active */
        win->gattr |= BIT(saital);

    } else { /* turn it off */

        sc->attr &= ~BIT(saital); /* set attribute inactive */
        win->gattr &= ~BIT(saital);

    }
    newfont(win); /* activate new font with that */

}

void italic(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iitalic(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off,  reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void ibold(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd];
    if (e) { /* bold on */

        sc->attr |= BIT(sabold); /* set attribute active */
        win->gattr |= BIT(sabold);

    } else { /* turn it off */

        sc->attr &= ~BIT(sabold); /* set attribute inactive */
        win->gattr &= ~BIT(sabold);

    }
    newfont(win); /* activate new font with that */

}

void bold(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ibold(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

void istrikeout(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sastkout); /* set attribute active */
       win->gattr |= BIT(sastkout);

    } else { /* turn it off */

       sc->attr &= ~BIT(sastkout); /* set attribute inactive */
       win->gattr &= ~BIT(sastkout);

    }
    newfont(win); /* activate new font with that */

}

void strikeout(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    istrikeout(win, e); /* move cursor right */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void standout(FILE* f, int e)

{

   reverse(f, e); /* implement as reverse */

}

/*******************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

*******************************************************************************/

void ifcolor(winptr win, pa_color c)

{

    int      r;  /* return value */
    HGDIOBJ  oh; /* old pen */
    int      b;  /* return value */
    LOGBRUSH lb;
    scnptr   sc;

    sc = win->screens[win->curupd];
    sc->fcrgb = colnum(c); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* activate in buffer */
    if (BIT(sarev) & sc->attr) {

       r = SetBkColor(sc->bdc, sc->fcrgb);
       if (r == -1) winerr(); /* process windows error */

    } else {

       r = SetTextColor(sc->bdc, sc->fcrgb);
       if (r == -1) winerr(); /* process windows error */

    }
    /* also activate general graphics color. note that reverse does ! apply
      to graphical coloring */
    b = DeleteObject(sc->fpen); /* remove old pen */
    if (!b) winerr(); /* process windows error */
    b = DeleteObject(sc->fbrush); /* remove old brush */
    if (!b) winerr(); /* process windows error */
    b = DeleteObject(sc->fspen); /* remove old single pixel pen */
    if (!b) winerr(); /* process windows error */
    /* create new pen */
    lb.lbStyle = BS_SOLID;
    lb.lbColor = sc->fcrgb;
    lb.lbHatch = 0;
    sc->fpen = ExtCreatePen(FPENSTL, sc->lwidth, &lb, 0, NULL);
    if (!sc->fpen) winerr(); /* process windows error */
    /* create new brush */
    sc->fbrush = CreateSolidBrush(sc->fcrgb);
    if (!sc->fbrush) winerr(); /* process windows error */
    /* create new single pixel pen */
    sc->fspen = CreatePen(FSPENSTL, 1, sc->fcrgb);
    if (!sc->fspen) winerr(); /* process windows error */
    /* select to buffer dc */
    oh = SelectObject(sc->bdc, sc->fpen);
    if (oh == HGDI_ERROR) winerr();
    if (indisp(win)) { /* activate on screen */

       /* set screen color according to reverse */
       if (BIT(sarev) & sc->attr) {

          r = SetBkColor(win->devcon, sc->fcrgb);
          if (r == -1) winerr(); /* process windows error */

       } else {

          r = SetTextColor(win->devcon, sc->fcrgb);
          if (r == -1) winerr(); /* process windows error */

       }
       oh = SelectObject(win->devcon, sc->fpen); /* select pen to display */
       if (oh == HGDI_ERROR) winerr();

    }

}

void fcolor(FILE* f, pa_color c)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ifcolor(win, c); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set foreground color graphical

Sets the foreground color from RGB primaries. The RGB values are scaled from
INT_MAX, so 255 == INT_MAX. This means that if the color resolution ever goes
up, we will be ready.

*******************************************************************************/

void ifcolorg(winptr win, int r, int g, int b)

{

    int      rv; /* return value */
    HGDIOBJ  oh; /* old pen */
    int      bv; /* return value */
    LOGBRUSH lb;
    scnptr   sc;

    sc = win->screens[win->curupd];
    sc->fcrgb = rgb2win(r, g, b); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* activate in buffer */
    if (BIT(sarev) & sc->attr) {

       r = SetBkColor(sc->bdc, sc->fcrgb);
       if (r == -1) winerr(); /* process windows error */

    } else {

       rv = SetTextColor(sc->bdc, sc->fcrgb);
       if (r == -1) winerr(); /* process windows error */

    }
    /* also activate general graphics color. note that reverse does not apply
      to graphical coloring */
    bv = DeleteObject(sc->fpen); /* remove old pen */
    if (!bv) winerr(); /* process error */
    bv = DeleteObject(sc->fbrush); /* remove old brush */
    if (!bv) winerr(); /* process error */
    bv = DeleteObject(sc->fspen); /* remove old single pixel pen */
    if (!bv) winerr(); /* process error */
    /* create new pen */
    lb.lbStyle = BS_SOLID;
    lb.lbColor = sc->fcrgb;
    lb.lbHatch = 0;
    sc->fpen = ExtCreatePen(FPENSTL, sc->lwidth, &lb, 0, NULL);
    if (!sc->fpen) winerr(); /* process error */
    /* create new brush */
    sc->fbrush = CreateSolidBrush(sc->fcrgb);
    if (!sc->fbrush) winerr(); /* process error */
    /* create new single pixel pen */
    sc->fspen = CreatePen(FSPENSTL, 1, sc->fcrgb);
    if (!sc->fspen) winerr(); /* process error */
    /* select to buffer dc */
    oh = SelectObject(sc->bdc, sc->fpen);
    if (oh == HGDI_ERROR) winerr();
    if (indisp(win))  { /* activate on screen */

       /* set screen color according to reverse */
       if (BIT(sarev) & sc->attr) {

          rv = SetBkColor(win->devcon, sc->fcrgb);
          if (rv == -1) winerr(); /* process windows error */

       } else {

          rv = SetTextColor(win->devcon, sc->fcrgb);
          if (rv == -1) winerr(); /* process windows error */

       };
       oh = SelectObject(win->devcon, sc->fpen); /* select pen to display */
       if (oh == HGDI_ERROR) winerr();

    }

}

void fcolorg(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ifcolorg(win, r, g, b); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void ibcolor(winptr win, pa_color c)

{

    int r;
    scnptr   sc;

    sc = win->screens[win->curupd];
    sc->bcrgb = colnum(c); /* set color status */
    win->gbcrgb = sc->bcrgb;
    /* activate in buffer */
    if (BIT(sarev) & sc->attr) {

        r = SetTextColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */

    } else {

        r = SetBkColor(sc->bdc, sc->bcrgb);
        if (r == -1) winerr(); /* process windows error */

    }
    if (indisp(win)) { /* activate on screen */

        /* set screen color according to reverse */
        if (BIT(sarev) & sc->attr) {

            r = SetTextColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */

        } else {

            r = SetBkColor(win->devcon, sc->bcrgb);
            if (r == -1) winerr(); /* process windows error */

        }

    }

}

void bcolor(FILE* f, pa_color c)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ibcolor(win, c); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background color graphical

Sets the background color from RGB primaries. The RGB values are scaled from
INT_MAX, so 255 == INT_MAX. This means that if the color resolution ever goes
up, we will be ready.

*******************************************************************************/

void ibcolorg(winptr win, int r, int g, int b)

{

    int    rv;
    scnptr sc;

    sc = win->screens[win->curupd];
    sc->bcrgb = rgb2win(r, g, b); /* set color status */
    win->gbcrgb = sc->bcrgb;
    /* activate in buffer */
    if (BIT(sarev) & sc->attr) {

        rv = SetTextColor(sc->bdc, sc->bcrgb);
        if (rv == -1) winerr(); /* process windows error */

    } else {

        rv = SetBkColor(sc->bdc, sc->bcrgb);
        if (rv == -1) winerr(); /* process windows error */

    }
    if (indisp(win))  { /* activate on screen */

        /* set screen color according to reverse */
        if (BIT(sarev) & sc->attr)  {

            rv = SetTextColor(win->devcon, sc->bcrgb);
            if (rv == -1) winerr(); /* process windows error */

        } else {

            rv = SetBkColor(win->devcon, sc->bcrgb);
            if (rv == -1) winerr(); /* process windows error */

        }

    }

}

void bcolorg(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ibcolorg(win, r, g, b); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable/disable automatic scroll and wrap


Enables or disables automatic screen scroll and end of line wrapping. When the
cursor leaves the screen in automatic mode, the following occurs:

up       Scroll down
down     Scroll up
right    Line down, start at left
left     Line up, start at right

These movements can be combined. Leaving the screen right from the lower right
corner will both wrap and scroll up. Leaving the screen left from upper left
will wrap and scroll down.

With auto disabled, no automatic scrolling will occur, and any movement of the
cursor off screen will simply cause the cursor to be undefined. In this
package that means the cursor is off, and no characters are written. On a
real terminal, it simply means that the position is undefined, and could be
anywhere.

*******************************************************************************/

void iauto(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd];
    /* check we are transitioning to auto mode */
    if (e) {

        /* check display is on grid && in bounds */
        if (sc->curxg-1%win->charspace) error(eatoofg);
        if (sc->curxg-1%win->charspace) error(eatoofg);
        if (!icurbnd(sc)) error(eatoecb);

    }
    sc->autof = e; /* set auto status */
    win->gauto = e;

}

void pa_auto(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iauto(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable/disable cursor visibility

Enable || disable cursor visibility.

*******************************************************************************/

void icurvis(winptr win, int e)

{

    win->screens[win->curupd]->curv = e; /* set cursor visible status */
    win->gcurv = e;
    cursts(win); /* process any cursor status change */

}

void curvis(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    icurvis(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int curx(FILE* f)

{

    winptr win; /* windows record pointer */
    int    x;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    x = win->screens[win->curupd]->curx; /* return current location x */
    unlockmain(); /* end exclusive access */

    return (x);

}

/*******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int cury(FILE* f)

{

    winptr win; /* windows record pointer */
    int    y;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    y = win->screens[win->curupd]->cury; /* return current location y */
    unlockmain(); /* end exclusive access */

    return (y);

}

/*******************************************************************************

Get location of cursor in x graphical

Returns the current location of the cursor in x, in pixels.

*******************************************************************************/

int curxg(FILE* f)

{

    winptr win; /* windows record pointer */
    int    x;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    x = win->screens[win->curupd]->curxg; /* return current location x */
    unlockmain(); /* end exclusive access */

    return (x);

}

/*******************************************************************************

Get location of cursor in y graphical

Returns the current location of the cursor in y, in pixels.

*******************************************************************************/

int curyg(FILE* f)

{

    winptr win; /* windows record pointer */
    int    y;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    y = win->screens[win->curupd]->curyg; /* return current location y */
    unlockmain(); /* end exclusive access */

    return (y);

}

/*******************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
 a new screen is allocated && cleared.
The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

*******************************************************************************/

void iselect(winptr win, int u, int d)

{

    int ld; /* last display screen number save */

    if (!win->bufmod) error(ebufoff); /* error */
    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error(einvscn); /* invalid screen number */
    ld = win->curdsp; /* save the current display screen number */
    win->curupd = u; /* set the current update screen */
    if (!win->screens[win->curupd]) { /* no screen, create one */

        /* get a new screen context */
        win->screens[win->curupd] = malloc(sizeof(scncon));
        if (!win->screens[win->curupd]) error(enomem);
        iniscn(win, win->screens[win->curupd]); /* initalize that */

    }
    win->curdsp = d; /* set the current display screen */
    if (!win->screens[win->curdsp]) { /* no screen, create one */

        /* no current screen, create a new one */
        win->screens[win->curdsp] = malloc(sizeof(scncon));
        if (!win->screens[win->curdsp]) error(enomem);
        iniscn(win, win->screens[win->curdsp]); /* initalize that */

    }
    /* if the screen has changed, restore it */
    if (win->curdsp != ld) restore(win, TRUE);

}

void pa_select(FILE* f, int u, int d)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iselect(win, u, d); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors && attributes.

Note: cannot place text with foreground for background xor modes, since there
is no direct windows feature for that. The only way to add this is to write
characters into a buffer DC,  blt them back to the main buffer && display
using ROP combinations.

*******************************************************************************/

void plcchr(winptr win, char c)

{

    int    b;   /* int return */
    int    off; /* subscript offset */
    SIZE   sz;  /* size holder */
    scnptr sc;

    sc = win->screens[win->curupd];
    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* handle special character cases first */
    if (c == '\r') {

       /* carriage return, position to extreme left */
       sc->curx = 1; /* set to extreme left */
       sc->curxg = 1;
       if (indisp(win)) setcur(win); /* set cursor on screen */

    } else if (c == '\n') idown(win); /* line feed, move down */
    else if (c == '\b') ileft(win); /* back space, move left */
    else if (c == '\f') iclear(win); /* clear screen */
    else if (c == '\t') itab(win); /* process tab */
    else if (c >= ' ' && c != 0x7f) { /* character is visible */

        off = 0; /* set no subscript offset */
        if (BIT(sasubs) & sc->attr) off = trunc(win->linespace*0.35);
        /* update buffer */
        if (win->bufmod) { /* buffer is active */

           /* draw character */
           b = TextOut(sc->bdc, sc->curxg-1, sc->curyg-1+off, &c, 1);
           if (!b) winerr(); /* process windows error */

        }
        if (indisp(win)) { /* activate on screen */

           /* draw character on screen */
           curoff(win); /* hide the cursor */
           /* draw character */
           b = TextOut(win->devcon, sc->curxg-1, sc->curyg-1+off, &c, 1);
           if (!b) winerr(); /* process windows error */
           curon(win); /* show the cursor */

        }
        if (sc->cfont->sys) iright(win); /* move cursor right character */
        else { /* perform proportional version */

           b = GetTextExtentPoint32(sc->bdc, &c, 1, &sz); /* get spacing */
           if (!b) winerr(); /* process windows error */
           sc->curxg = sc->curxg+sz.cx; /* advance the character width */
           sc->curx = sc->curxg/win->charspace+1; /* recalculate character position */
           if (indisp(win)) setcur(win); /* set cursor on screen */

        }

    }

}

/*******************************************************************************

Write string to current cursor position

Writes a string to the current cursor position,  updates the cursor
position. This acts as a series of write character calls. However, it eliminates
several layers of protocol, and results in much faster write time for
applications that require it.

It is an error to call this routine with auto enabled, since it could exceed
the bounds of the screen.

No control characters or other interpretation is done, and invisible characters
such as controls are not suppressed.

*******************************************************************************/

void iwrtstr(winptr win,  char* s)

{

    int    b;   /* int return */
    int    off; /* subscript offset */
    SIZE   sz;  /* size holder */
    scnptr sc;

    sc = win->screens[win->curupd];
    if (sc->autof) error(estrato); /* autowrap is on */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    off = 0; /* set no subscript offset */
    if (BIT(sasubs) & sc->attr) off = trunc(win->linespace*0.35);
    /* update buffer */
    if (win->bufmod) { /* buffer is active */

       /* draw character */
       b = TextOut(sc->bdc, sc->curxg-1, sc->curyg-1+off, s, strlen(s));
       if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) { /* activate on screen */

       /* draw character on screen */
       curoff(win); /* hide the cursor */
       /* draw character */
       b = TextOut(win->devcon, sc->curxg-1, sc->curyg-1+off, s, strlen(s));
       if (!b) winerr(); /* process windows error */
       curon(win); /* show the cursor */

    }
    if (sc->cfont->sys) { /* perform fixed system advance */

          /* should check if this exceeds INT_MAX */
          sc->curx = sc->curx+strlen(s); /* update position */
          sc->curxg = sc->curxg+win->charspace*strlen(s);

    } else { /* perform proportional version */

       b = GetTextExtentPoint32(sc->bdc, s, strlen(s), &sz); /* get spacing */
       if (!b) winerr(); /* process windows error */
       sc->curxg = sc->curxg+sz.cx; /* advance the character width */
       sc->curx = sc->curxg/win->charspace+1; /* recalculate character position */
       if (indisp(win)) setcur(win); /* set cursor on screen */

    }

}

void wrtstr(FILE* f, char* s)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iwrtstr(win, s); /* perform string write */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Delete last character

Deletes the character to the left of the cursor, && moves the cursor one
position left.

*******************************************************************************/

void idel(winptr win)

{

    ileft(win); /* back up cursor */
    plcchr(win, ' '); /* blank out */
    ileft(win); /* back up again */

}

void del(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    idel(win); /* perform delete */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw line

Draws a single line in the foreground color.

*******************************************************************************/

void iline(winptr win, int x1, int y1, int x2, int y2)

{

    int b;      /* results */
    int tx, ty; /* temp holder */
    int dx, dy;
    scnptr sc;

    sc = win->screens[win->curupd];
    sc->lcurx = x2; /* place next progressive endpoint */
    sc->lcury = y2;
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* Try to compensate for windows not drawing line endings. */
    if (y1 == y2)  dy = 0;
    else if (y1 < y2) dy = +1;
    else dy = -1;
    if (x1 == x2) dx = 0;
    else dx = +1;
    if (win->bufmod) { /* buffer is active */

       /* set current position of origin */
       b = MoveToEx(sc->bdc, x1-1, y1-1, NULL);
       if (!b) winerr(); /* process windows error */
       b = LineTo(sc->bdc, x2-1+dx, y2-1+dy);
       if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) { /* do it again for the current screen */

       if (!win->visible) winvis(win); /* make sure we are displayed */
       curoff(win);
       b = MoveToEx(win->devcon, x1-1, y1-1, NULL);
       if (!b) winerr(); /* process windows error */
       b = LineTo(win->devcon, x2-1+dx, y2-1+dy);
       if (!b) winerr(); /* process windows error */
       curon(win);

    }

}

void line(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iline(win, x1, y1, x2, y2); /* draw line */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw rectangle

Draws a rectangle in foreground color.

*******************************************************************************/

void irect(winptr win, int x1, int y1, int x2, int y2)

{

    BOOL b; /* return value */

    if (win->bufmod) { /* buffer is active */

        /* draw to buffer */
        b = Rectangle(win->screens[win->curupd]->bdc, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) {

        /* draw to screen */
        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win);
        b = Rectangle(win->devcon, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */
        curon(win);

    }

}

void rect(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    irect(win, x1, y1, x2, y2); /* draw rectangle */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled rectangle

Draws a filled rectangle in foreground color.

*******************************************************************************/

void ifrect(winptr win, int x1, int y1, int x2, int y2)

{

    BOOL    b; /* return value */
    HGDIOBJ r; /* result holder */
    scnptr sc;

    sc = win->screens[win->curupd];
    if (win->bufmod) { /* buffer is active */

       /* for filled ellipse, the pen and brush settings are all wrong. we need
         a single pixel pen and a background brush. we set and restore these */
       r = SelectObject(sc->bdc, sc->fspen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(sc->bdc, sc->fbrush);
       if (r == HGDI_ERROR) error(enosel);
       /* draw to buffer */
       b = Rectangle(sc->bdc, x1-1, y1-1, x2, y2);
       if (!b)  winerr(); /* process windows error */
       /* restore */
       r = SelectObject(sc->bdc, sc->fpen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
       if (r == HGDI_ERROR) error(enosel);

    }
    /* draw to screen */
    if (indisp(win)) {

       if (!win->visible) winvis(win); /* make sure we are displayed */
       r = SelectObject(win->devcon, sc->fspen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(win->devcon, sc->fbrush);
       if (r == HGDI_ERROR) error(enosel);
       curoff(win);
       b = Rectangle(win->devcon, x1-1, y1-1, x2, y2);
       if (!b) winerr(); /* process windows error */
       curon(win);
       r = SelectObject(win->devcon, sc->fpen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
       if (r == HGDI_ERROR) error(enosel);

    }

}

void frect(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifrect(win, x1, y1, x2, y2); /* draw rectangle */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw rounded rectangle

Draws a rounded rectangle in foreground color.

*******************************************************************************/

void irrect(winptr win, int x1, int y1, int x2, int y2, int xs, int ys)

{


    BOOL b; /* return value */

    if (win->bufmod)  { /* buffer is active */

        /* draw to buffer */
        b = RoundRect(win->screens[win->curupd]->bdc, x1-1, y1-1, x2, y2, xs, ys);
        if (!b) winerr(); /* process windows error */

    }
    /* draw to screen */
    if (indisp(win)) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win);
        b = RoundRect(win->devcon, x1-1, y1-1, x2, y2, xs, ys);
        if (!b) winerr(); /* process windows error */
        curon(win);

    }

}

void rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    irrect(win, x1, y1, x2, y2, xs, ys); /* draw rectangle */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled rounded rectangle

Draws a filled rounded rectangle in foreground color.

*******************************************************************************/

void ifrrect(winptr win, int x1, int y1, int x2, int y2, int xs, int ys)

{

    BOOL    b; /* return value */
    HGDIOBJ r; /* result holder */
    scnptr sc;

    sc = win->screens[win->curupd];
    if (win->bufmod) { /* buffer is active */

       /* for filled ellipse, the pen && brush settings are all wrong. we need
         a single pixel pen && a background brush. we set && restore these */
       r = SelectObject(sc->bdc, sc->fspen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(sc->bdc, sc->fbrush);
       if (r == HGDI_ERROR) error(enosel);
       /* draw to buffer */
       b = RoundRect(sc->bdc, x1-1, y1-1, x2, y2, xs, ys);
       if (!b) winerr(); /* process windows error */
       /* restore */
       r = SelectObject(sc->bdc, sc->fpen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
       if (r == HGDI_ERROR) error(enosel);

    }
    /* draw to screen */
    if (indisp(win)) {

       if (!win->visible)  winvis(win); /* make sure we are displayed */
       r = SelectObject(win->devcon, sc->fspen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(win->devcon, sc->fbrush);
       if (r == HGDI_ERROR) error(enosel);
       curoff(win);
       b = RoundRect(win->devcon, x1-1, y1-1, x2, y2, xs, ys);
       if (!b) winerr(); /* process windows error */
       curon(win);
       r = SelectObject(win->devcon, sc->fpen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
       if (r == HGDI_ERROR) error(enosel);

    }

}

void frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifrrect(win, x1, y1, x2, y2, xs, ys); /* draw rectangle */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw ellipse

Draws an ellipse with the current foreground color && line width.

*******************************************************************************/

void iellipse(winptr win, int x1, int y1, int x2, int y2)

{

    BOOL b; /* return value */

    if (win->bufmod) { /* buffer is active */

       /* draw to buffer */
       b = Ellipse(win->screens[win->curupd]->bdc, x1-1, y1-1, x2, y2);
       if (!b) winerr(); /* process windows error */

    }
    /* draw to screen */
    if (indisp(win)) {

       if (!win->visible) winvis(win); /* make sure we are displayed */
       curoff(win);
       b = Ellipse(win->devcon, x1-1, y1-1, x2, y2);
       if (!b) winerr(); /* process windows error */
       curon(win);

    }

}

void pa_ellipse(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iellipse(win, x1, y1, x2, y2); /* draw ellipse */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled ellipse

Draws a filled ellipse with the current foreground color.

*******************************************************************************/

void ifellipse(winptr win, int x1, int y1, int x2, int y2)

{

    BOOL b; /* return value */
    HGDIOBJ r; /* result holder */
    scnptr sc;

    sc = win->screens[win->curupd];
    if (win->bufmod) { /* buffer is active */

        /* for filled ellipse, the pen and brush settings are all wrong. we need
           a single pixel pen && a background brush. we set && restore these */
        r = SelectObject(sc->bdc, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        /* draw to buffer */
        b = Ellipse(sc->bdc, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */
        /* restore */
        r = SelectObject(sc->bdc, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }
    /* draw to screen */
    if (indisp(win)) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        r = SelectObject(win->devcon, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        curoff(win);
        b = Ellipse(win->devcon, x1-1, y1-1, x2, y2);
        if (!b) winerr(); /* process windows error */
        curon(win);
        r = SelectObject(win->devcon, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }

}

void fellipse(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifellipse(win, x1, y1, x2, y2); /* draw ellipse */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw arc

Draws an arc in the current foreground color && line width. The containing
rectangle of the ellipse is given, && the start && } angles clockwise from
0 degrees delimit the arc.

Windows takes the start && } delimited by a line ext}ing from the center
of the arc. The way we do the convertion is to project the angle upon a circle
whose radius is the precision we wish to use for the calculation.  that
point on the circle is found by triangulation.

The larger the circle of precision, the more angles can be represented, but
the trade off is that the circle must ! reach the edge of an int
(-INT_MAX..INT_MAX). That means that the total logical coordinate space must be
shortened by the precision. To find out what division of the circle precis
represents, use cd = precis*2*PI. So, for example, precis == 100 means 628
divisions of the circle.

The } && start points can be negative.
Note that Windows draws arcs counterclockwise, so our start && } points are
swapped.

Negative angles are allowed.

*******************************************************************************/

void iarc(winptr win, int x1, int y1, int x2, int y2, int sa, int ea)

{

    const int precis = 1000; /* precision of circle calculation */

    float saf, eaf;       /* starting angles in radian float */
    int   xs, ys, xe, ye; /* start && } coordinates */
    int   xc, yc;         /* center point */
    int   t;              /* swapper */
    BOOL  b;              /* return value */

    /* rationalize rectangle for processing */
    if (x1 > x2) { t = x1; x1 = x2; x2 = t; };
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; };
    /* convert start && } to radian measure */
    saf = sa*2.0*PI/INT_MAX;
    eaf = ea*2.0*PI/INT_MAX;
    /* find center of ellipse */
    xc = (x2-x1) / 2+x1;
    yc = (y2-y1) / 2+y1;
    /* resolve start to x, y */
    xs = xc+precis*cos(PI/2-saf);
    ys = yc-precis*sin(PI/2-saf);
    /* resolve } to x, y */
    xe = xc+precis*cos(PI/2-eaf);
    ye = yc-precis*sin(PI/2-eaf);
    if (win->bufmod) { /* buffer is active */

        b = Arc(win->screens[win->curupd]->bdc, x1-1, y1-1, x2, y2, xe, ye,
                xs, ys);
        if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win);
        b = Arc(win->devcon, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        curon(win);

    }

}

void arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iarc(win, x1, y1, x2, y2, sa, ea); /* draw arc */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled arc

Draws a filled arc in the current foreground color. The same comments apply
as for the arc int above.

*******************************************************************************/

void ifarc(winptr win, int x1, int y1, int x2, int y2, int sa, int ea)

{

    const int precis = 1000; /* precision of circle calculation */

    float saf, eaf;     /* starting angles in radian float */
    int xs, ys, xe, ye; /* start && } coordinates */
    int xc, yc;         /* center point */
    int t;              /* swapper */
    BOOL b;             /* return value */
    HGDIOBJ r;          /* result holder */
    scnptr sc;

    sc = win->screens[win->curupd];
    /* rationalize rectangle for processing */
    if (x1 > x2) { t = x1; x1 = x2; x2 = t; };
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; };
    /* convert start and end to radian measure */
    saf = sa*2*PI/INT_MAX;
    eaf = ea*2*PI/INT_MAX;
    /* find center of ellipse */
    xc = (x2-x1)/2+x1;
    yc = (y2-y1)/2+y1;
    /* resolve start to x, y */
    xs = xc+precis*cos(PI/2-saf);
    ys = yc-precis*sin(PI/2-saf);
    /* resolve end to x, y */
    xe = xc+precis*cos(PI/2-eaf);
    ye = yc-precis*sin(PI/2-eaf);
    if (win->bufmod) { /* buffer is active */

        /* for filled shape, the pen && brush settings are all wrong. we need
           a single pixel pen && a background brush. we set && restore these */
        r = SelectObject(sc->bdc, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        /* draw shape */
        b = Pie(sc->bdc, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        /* restore */
        r = SelectObject(sc->bdc, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        r = SelectObject(win->devcon, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        curoff(win);
        /* draw shape */
        b = Pie(win->devcon, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        curon(win);
        r = SelectObject(win->devcon, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }

}

void farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifarc(win, x1, y1, x2, y2, sa, ea); /* draw arc */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled cord

Draws a filled cord in the current foreground color. The same comments apply
as for the arc int above.

*******************************************************************************/

void ifchord(winptr win, int x1, int y1, int x2, int y2, int sa, int ea)

{

    const int precis = 1000; /* precision of circle calculation */

    float   saf, eaf;       /* starting angles in radian float */
    int     xs, ys, xe, ye; /* start and end coordinates */
    int     xc, yc;         /* center point */
    int     t;              /* swapper */
    BOOL    b;              /* return value */
    HGDIOBJ r;              /* result holder */
    scnptr  sc;             /* screen context */

    sc = win->screens[win->curupd];
    /* rationalize rectangle for processing */
    if (x1 > x2) { t = x1; x1 = x2; x2 = t; }
    if (y1 > y2)  { t = y1; y1 = y2; y2 = t; }
    /* convert start and end to radian measure */
    saf = sa*2*PI/INT_MAX;
    eaf = ea*2*PI/INT_MAX;
    /* find center of ellipse */
    xc = (x2-x1)/2+x1;
    yc = (y2-y1)/2+y1;
    /* resolve start to x, y */
    xs = xc+precis*cos(PI/2-saf);
    ys = yc-precis*sin(PI/2-saf);
    /* resolve } to x, y */
    xe = xc+precis*cos(PI/2-eaf);
    ye = yc-precis*sin(PI/2-eaf);
    if (win->bufmod) { /* buffer is active */

        /* for filled shape, the pen && brush settings are all wrong. we need
           a single pixel pen && a background brush. we set && restore these */
        r = SelectObject(sc->bdc, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        /* draw shape */
        b = Chord(sc->bdc, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        /* restore */
        r = SelectObject(sc->bdc, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        r = SelectObject(win->devcon, sc->fspen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, sc->fbrush);
        if (r == HGDI_ERROR) error(enosel);
        curoff(win);
        /* draw shape */
        b = Chord(win->devcon, x1-1, y1-1, x2, y2, xe, ye, xs, ys);
        if (!b) winerr(); /* process windows error */
        curon(win);
        r = SelectObject(win->devcon, sc->fpen);
        if (r == HGDI_ERROR) error(enosel);
        r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
        if (r == HGDI_ERROR) error(enosel);

    }

}

void fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifchord(win, x1, y1, x2, y2, sa, ea); /* draw cord */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Draw filled triangle

Draws a filled triangle in the current foreground color.

*******************************************************************************/

void iftriangle(winptr win, int x1, int y1, int x2, int y2, int x3, int y3)

{

    POINT   pa[3]; /* points of triangle */
    BOOL    b;     /* return value */
    HGDIOBJ r;     /* result holder */
    scnptr  sc;

    sc = win->screens[win->curupd];
    /* place triangle points in array */
    pa[1].x = x1-1;
    pa[1].y = y1-1;
    pa[2].x = x2-1;
    pa[2].y = y2-1;
    pa[3].x = x3-1;
    pa[3].y = y3-1;
    if (win->bufmod) { /* buffer is active */

       /* for filled shape, the pen and brush settings are all wrong. we need
         a single pixel pen && a background brush. we set and restore these */
       r = SelectObject(sc->bdc, sc->fspen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(sc->bdc, sc->fbrush);
       if (r == HGDI_ERROR) error(enosel);
       /* draw to buffer */
       b = Polygon(sc->bdc, pa, 3);
       if (!b) winerr(); /* process windows error */
       /* restore */
       r = SelectObject(sc->bdc, sc->fpen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(sc->bdc, GetStockObject(NULL_BRUSH));
       if (r == HGDI_ERROR) error(enosel);

    }
    /* draw to screen */
    if (indisp(win)) {

       if (!win->visible) winvis(win); /* make sure we are displayed */
       r = SelectObject(win->devcon, sc->fspen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(win->devcon, sc->fbrush);
       if (r == HGDI_ERROR) error(enosel);
       curoff(win);
       b = Polygon(win->devcon, pa, 3);
       if (!b) winerr(); /* process windows error */
       curon(win);
       r = SelectObject(win->devcon, sc->fpen);
       if (r == HGDI_ERROR) error(enosel);
       r = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
       if (r == HGDI_ERROR) error(enosel);

    }
    /* The progressive points get shifted left one. This causes progressive
      single point triangles to become triangle strips. */
    if (sc->tcurs) { /* process odd strip flip */

       sc->tcurx1 = x1; /* place next progressive endpoint */
       sc->tcury1 = y1;
       sc->tcurx2 = x3;
       sc->tcury2 = y3;

    } else { /* process even strip flip */

       sc->tcurx1 = x3; /* place next progressive endpoint */
       sc->tcury1 = y3;
       sc->tcurx2 = x2;
       sc->tcury2 = y2;

    }

}

void ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iftriangle(win, x1, y1, x2, y2, x3, y3); /* draw triangle */
    win->screens[win->curupd]->tcurs = FALSE; /* set even strip flip state */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set pixel

Sets a single logical pixel to the foreground color.

*******************************************************************************/

void isetpixel(winptr win, int x, int y)

{

    COLORREF r; /* return value */

    if (win->bufmod)  { /* buffer is active */

       /* paint buffer */
       r = SetPixel(win->screens[win->curupd]->bdc, x-1, y-1,
                    win->screens[win->curupd]->fcrgb);
       if (r == -1) winerr(); /* process windows error */

    }
    /* paint screen */
    if (indisp(win)) {

       if (!win->visible) winvis(win); /* make sure we are displayed */
       curoff(win);
       r = SetPixel(win->devcon, x-1, y-1, win->screens[win->curupd]->fcrgb);
       if (r == -1) winerr(); /* process windows error */
       curon(win);

    }

}

void setpixel(FILE* f, int x, int y)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    isetpixel(win, x, y); /* set pixel */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set foreground to overwrite

Sets the foreground write mode to overwrite.

*******************************************************************************/

void ifover(winptr win)

{

    int r;

    win->gfmod = mdnorm; /* set foreground mode normal */
    win->screens[win->curupd]->fmod = mdnorm;
    r = SetROP2(win->screens[win->curupd]->bdc, R2_COPYPEN);
    if (r == 0) winerr(); /* process windows error */
    if (indisp(win)) r = SetROP2(win->devcon, R2_COPYPEN);

}

void fover(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifover(win); /* set overwrite */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background to overwrite

Sets the background write mode to overwrite.

*******************************************************************************/

void ibover(winptr win)

{

    int r;

    win->gbmod = mdnorm; /* set background mode normal */
    win->screens[win->curupd]->bmod = mdnorm;
    r = SetBkMode(win->screens[win->curupd]->bdc, OPAQUE);
    if (r == 0) winerr(); /* process windows error */
    if (indisp(win)) r = SetBkMode(win->devcon, OPAQUE);

}

void bover(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ibover(win); /* set overwrite */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set foreground to invisible

Sets the foreground write mode to invisible.

*******************************************************************************/

void ifinvis(winptr win)

{

    int r;

    win->gfmod = mdinvis; /* set foreground mode invisible */
    win->screens[win->curupd]->fmod = mdinvis;
    r = SetROP2(win->screens[win->curupd]->bdc, R2_NOP);
    if (r == 0) winerr(); /* process windows error */
    if (indisp(win)) r = SetROP2(win->devcon, R2_NOP);

}

void finvis(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifinvis(win); /* set invisible */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background to invisible

Sets the background write mode to invisible.

*******************************************************************************/

void ibinvis(winptr win)

{

    int r;

    win->gbmod = mdinvis; /* set background mode invisible */
    win->screens[win->curupd]->bmod = mdinvis;
    r = SetBkMode(win->screens[win->curupd]->bdc, TRANSPARENT);
    if (r == 0) winerr(); /* process windows error */
    if (indisp(win)) r = SetBkMode(win->devcon, TRANSPARENT);

}

void binvis(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ibinvis(win); /* set invisible */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set foreground to xor

Sets the foreground write mode to xor.

*******************************************************************************/

void ifxor(winptr win)

{

    int r;

    win->gfmod = mdxor; /* set foreground mode xor */
    win->screens[win->curupd]->fmod = mdxor;
    r = SetROP2(win->screens[win->curupd]->bdc, R2_XORPEN);
    if (!r) winerr(); /* process windows error */
    if (indisp(win)) r = SetROP2(win->devcon, R2_XORPEN);

}

void fxor(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifxor(win); /* set xor */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set background to xor

Sets the background write mode to xor.

*******************************************************************************/

void ibxor(winptr win)

{

    win->gbmod = mdxor; /* set background mode xor */
    win->screens[win->curupd]->bmod = mdxor;

}

void bxor(FILE* f)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ibxor(win); /* set xor */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set line width

Sets the width of lines && several other figures.

*******************************************************************************/

void ilinewidth(winptr win, int w)

{

    HGDIOBJ  oh; /* old pen */
    int      b;  /* return value */
    LOGBRUSH lb;
    scnptr   sc;

    sc = win->screens[win->curupd];
    sc->lwidth = w; /* set new width */
    /* create new pen with desired width */
    b = DeleteObject(sc->fpen); /* remove old pen */
    if (!b) winerr(); /* process windows error */
    /* create new pen */
    lb.lbStyle = BS_SOLID;
    lb.lbColor = sc->fcrgb;
    lb.lbHatch = 0;
    sc->fpen = ExtCreatePen(FPENSTL, sc->lwidth, &lb, 0, NULL);
    if (!sc->fpen) winerr(); /* process windows error */
    /* select to buffer dc */
    oh = SelectObject(sc->bdc, sc->fpen);
    if (oh == HGDI_ERROR) error(enosel);
    if (indisp(win)) { /* activate on screen */

        oh = SelectObject(win->devcon, sc->fpen); /* select pen to display */
        if (oh == HGDI_ERROR) error(enosel);

    }

}

void linewidth(FILE* f, int w)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ilinewidth(win, w); /* set line width */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find character size x

Returns the character width.

*******************************************************************************/

int chrsizx(FILE* f)

{

    winptr win; /* window pointer */
    int    cs;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    cs = win->charspace;
    unlockmain(); /* end exclusive access */

    return (cs);

}

/*******************************************************************************

Find character size y

Returns the character height.

*******************************************************************************/

int chrsizy(FILE* f)

{

    winptr win; /* window pointer */
    int cs;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    cs = win->linespace;
    unlockmain(); /* end exclusive access */

    return (cs);

}

/*******************************************************************************

Find number of installed fonts

Finds the total number of installed fonts.

*******************************************************************************/

int fonts(FILE* f)

{

    winptr win; /* window pointer */
    int    ft;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ft = win->fntcnt; /* return global font counter */
    unlockmain(); /* end exclusive access */

    return (ft);

}

/*******************************************************************************

Change fonts

Changes the current font to the indicated logical font number.

*******************************************************************************/

void ifont(winptr win, int fc)

{

    fontptr fp;

    if (win->screens[win->curupd]->autof) error(eatoftc); /* cannot perform with auto on */
    if (fc < 1) error(einvfnm); /* invalid font number */
    /* find indicated font */
    fp = win->fntlst;
    while (fp != NULL && fc > 1) { /* search */

       fp = fp->next; /* mext font entry */
       fc--; /* count */

    }
    if (fc > 1)  error(einvfnm); /* invalid font number */
    if (!strlen(fp->fn)) error(efntemp); /* font is not assigned */
    win->screens[win->curupd]->cfont = fp; /* place new font */
    win->gcfont = fp;
    newfont(win); /* activate font */
    chgcur(win); /* change cursors */

}

void font(FILE* f, int fc)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifont(win, fc); /* set font */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find name of font

Returns the name of a font by number.

*******************************************************************************/

void ifontnam(winptr win, int fc, char* fns, int fnsl)

{

    fontptr fp; /* pointer to font entries */
    int i; /* string index */

    if (fc <= 0) error(einvftn); /* invalid number */
    fp = win->fntlst; /* index top of list */
    while (fc > 1) { /* walk fonts */

       fp = fp->next; /* next font */
       fc = fc-1; /* count */
       if (!fp) error(einvftn); /* check null */

    }
    if (strlen(fp->fn) > fnsl+1) error(eftntl);
    strcpy(fns, fp->fn);

}

void fontnam(FILE* f, int fc, char* fns, int fnsl)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifontnam(win, fc, fns, fnsl); /* find font name */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Change font size

Changes the font sizing to match the given character height. The character
and line spacing are changed, as well as the baseline.

*******************************************************************************/

void ifontsiz(winptr win, int s)

{

    /* cannot perform with system font */
    if (win->screens[win->curupd]->cfont->sys) error(etrmfts);
    if (win->screens[win->curupd]->autof)
        error(eatofts); /* cannot perform with auto on */
    win->gfhigh = s; /* set new font height */
    newfont(win); /* activate font */

}

void fontsiz(FILE* f, int s)

{

    winptr win;  /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ifontsiz(win, s); /* set font size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set character extra spacing y

Sets the extra character space to be added between lines, also referred to
as "ledding".

Not implemented yet.

*******************************************************************************/

void chrspcy(FILE* f, int s)

{

   /* not implemented */

}

/*******************************************************************************

Sets extra character space x

Sets the extra character space to be added between characters, referred to
as "spacing".

Not implemented yet.

*******************************************************************************/

void chrspcx(FILE* f, int s)

{

   /* not implemented */

}

/*******************************************************************************

Find dots per meter x

Returns the number of dots per meter resolution in x.

*******************************************************************************/

int dpmx(FILE* f)

{

    winptr win; /* window pointer */
    int    dpm;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    dpm = win->sdpmx;
    unlockmain(); /* end exclusive access */

    return (dpm);

}

/*******************************************************************************

Find dots per meter y

Returns the number of dots per meter resolution in y.

*******************************************************************************/

int dpmy(FILE* f)

{

    winptr win; /* window pointer */
    int    dpm;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    dpm = win->sdpmy;
    unlockmain(); /* end exclusive access */

    return (dpm);

}

/*******************************************************************************

Find string size in pixels

Returns the number of pixels wide the given string would be, considering
character spacing and kerning.

*******************************************************************************/

int istrsiz(winptr win, char* s)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    int ss;

    /* get spacing */
    b = GetTextExtentPoint32(win->screens[win->curupd]->bdc, s, 1, &sz);
    if (!b) winerr(); /* process windows error */
    ss = sz.cx; /* return that */

    return (ss);

}

int strsiz(FILE* f, char* s)

{

    winptr win; /* window pointer */
    int    ss;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ss = istrsiz(win, s); /* find string size */
    unlockmain(); /* end exclusive access */

    return (ss);

}

/*******************************************************************************

Find character position in string

Finds the pixel offset to the given character in the string.

*******************************************************************************/

int ichrpos(winptr win, char* s, int p)

{

    char* sp; /* string holder */
    int   siz; /* size holder */
    SIZE  sz; /* size holder */
    BOOL  b; /* return value */
    int   i; /* index for string */

    if (p < 1 || p > strlen(s)) error(estrinx); /* out of range */
    if (p == 1) siz = 0; /* its already at the position */
    else { /* find substring length */

        /* get spacing */
        b = GetTextExtentPoint32(win->screens[win->curupd]->bdc, s, p, &sz);
        if (!b) winerr(); /* process windows error */
        siz = sz.cx; /* place size */

    }

    return (siz); /* return result */

}

int chrpos(FILE* f, char* s, int p)

{

    winptr win; /* window pointer */
    int    cp;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    cp = ichrpos(win, s, p); /* find character position */
    unlockmain(); /* end exclusive access */

    return (cp);

}

/*******************************************************************************

Write justified text

Writes a string of text with justification. The string and the width in pixels
is specified. Auto mode cannot be on for this function, nor can it be used on
the system font.

*******************************************************************************/

void iwritejust(winptr win, char* s, int n)

{

    SIZE        sz;  /* size holder */
    BOOL        b;   /* return value */
    int         off; /* subscript offset */
    GCP_RESULTS ra;  /* placement info record */
    scnptr      sc;
    DWORD       r;
    int         sa;

    sc = win->screens[win->curupd];
    if (sc->cfont->sys) error(ejstsys); /* cannot perform on system font */
    if (sc->autof) error(eatopos); /* cannot perform with auto on */
    off = 0; /* set no subscript offset */
    if (BIT(sasubs) & sc->attr) off = trunc(win->linespace*0.35);
    /* get minimum spacing for string */
    b = GetTextExtentPoint32(sc->bdc, s, strlen(s), &sz);
    if (!b) winerr(); /* process windows error */
    /* if requested less than required, force required */
    if (sz.cx > n) n = sz.cx;
    /* find justified spacing */
    ra.lStructSize = sizeof(GCP_RESULTS); /* set length of record */
    /* new(ra.lpoutstring); */
    ra.lpOutString = NULL;
    ra.lpOrder = NULL;
    ra.lpDx = &sa;
    ra.lpCaretPos = NULL;
    ra.lpClass = NULL;
    ra.lpGlyphs = NULL;
    ra.nGlyphs = strlen(s);
    ra.nMaxFit = 0;
    r = GetCharacterPlacement(win->screens[win->curupd]->bdc, s, strlen(s),
                              n, &ra, GCP_JUSTIFY | GCP_MAXEXTENT);
    if (r == 0) winerr(); /* process windows error */
    if (win->bufmod) { /* draw to buffer */

       /* draw the string to current position */
       b = ExtTextOut(sc->bdc, sc->curxg-1, sc->curyg-1+off, 0, NULL, s,
                      strlen(s), ra.lpDx);
       if (!b) winerr(); /* process windows error */

    }
    if (indisp(win)) {

       if (!win->visible) winvis(win); /* make sure we are displayed */
       /* draw character on screen */
       curoff(win); /* hide the cursor */
       /* draw the string to current position */
       b = ExtTextOut(win->devcon, sc->curxg-1, sc->curyg-1+off, 0, NULL,
                      s, strlen(s), ra.lpDx);
       if (!b) winerr(); /* process windows error */
       curon(win); /* show the cursor */

    }
    sc->curxg = sc->curxg+n; /* advance the character width */
    sc->curx = sc->curxg / win->charspace+1; /* recalculate character position */
    if (indisp(win)) setcur(win); /* set cursor on screen */

}

void writejust(FILE* f, char* s, int n)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iwritejust(win, s, n); /* write justified text */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find justified character position

Given a string, a character position in that string, and the total length
of the string in pixels, returns the offset in pixels from the start of the
string to the given character, with justification taken into account.
The model used is that the extra space needed is divided by the number of
spaces, with the fractional part lost.

*******************************************************************************/

int ijustpos(winptr win, char* s, int p, int n)

{

    int         off; /* offset to character */
    int         w;   /* minimum string size */
    GCP_RESULTS ra;  /* placement info record */
    int         i;   /* index for string */

    if (p < 1 || p > strlen(s))  error(estrinx); /* out of range */
    if (p == 1)  off = 0; /* its already at the position */
    else { /* find substring length */

       w = istrsiz(win, s); /* find minimum character spacing */
       /* if allowed is less than the min, return nominal spacing */
       if (n <= w) off = ichrpos(win, s, p); else {

          /* find justified spacing */
          ra.lStructSize = sizeof(GCP_RESULTS); /* set length of record */
          ra.lpOutString = NULL;
          ra.lpOrder = NULL;
          /* allocate size array */
          ra.lpDx = malloc(strlen(s)*sizeof(int));
          ra.lpCaretPos = NULL;
          ra.lpClass = NULL;
          ra.lpGlyphs = NULL;
          ra.nGlyphs = 0;
          ra.nMaxFit = 0;
          r = GetCharacterPlacement(win->screens[win->curupd]->bdc, s,
                                    strlen(s), n, &ra,
                                    GCP_JUSTIFY | GCP_MAXEXTENT);
          if (r == 0) winerr(); /* process windows error */
          off = 0; /* clear offset */
          /* add in all widths to the left of position to offset */
          for (i = 0; i < p; i++) off = off+ra.lpDx[i];
          free(ra.lpDx);

       }

    }

    return (off); /* return result */

}

int justpos(FILE* f, char* s, int p, int n)

{

    winptr win; /* window pointer */
    int    jp;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    jp = ijustpos(win, s, p, n); /* find justified character position */
    unlockmain(); /* end exclusive access */

    return (jp);

}

/*******************************************************************************

Turn on condensed attribute

Turns on/off the condensed attribute. Condensed is a character set with a
shorter baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void condensed(FILE* f, int e)

{

   /* not implemented */

}

/*******************************************************************************

Turn on extended attribute

Turns on/off the extended attribute. Ext}ed is a character set with a
longer baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void extended(FILE* f, int e)

{

   /* not implemented */

}

/*******************************************************************************

Turn on extra light attribute

Turns on/off the extra light attribute. Extra light is a character thiner than
light.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void xlight(FILE* f, int e)

{

   /* not implemented */

}

/*******************************************************************************

Turn on light attribute

Turns on/off the light attribute. Light is a character thiner than normal
characters in the currnet font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void light(FILE* f, int e)

{

   /* not implemented */

}

/*******************************************************************************

Turn on extra bold attribute

Turns on/off the extra bold attribute. Extra bold is a character thicker than
bold.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void xbold(FILE* f, int e)

{

   /* not implemented */

}

/*******************************************************************************

Turn on hollow attribute

Turns on/off the hollow attribute. Hollow is an embossed || 3d effect that
makes the characters appear sunken into the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void hollow(FILE* f, int e)

{

   /* not implemented */

}

/*******************************************************************************

Turn on raised attribute

Turns on/off the raised attribute. Raised is an embossed || 3d effect that
makes the characters appear coming off the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void raised(FILE* f, int e)

{

   /* not implemented */

}

/*******************************************************************************

Delete picture

Deletes a loaded picture.

*******************************************************************************/

void idelpict(winptr win, int p)

{

    HGDIOBJ r; /* result holder */
    BOOL b; /* result holder */

    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p].han) error(einvhan); /* bad picture handle */
    /* reselect old object */
    r = SelectObject(win->pictbl[p].hdc, win->pictbl[p].ohn);
    if (r == HGDI_ERROR) error(enosel);
    b = DeleteDC(win->pictbl[p].hdc); /* delete device context */
    if (!b) winerr(); /* process windows error */
    b = DeleteObject(win->pictbl[p].han); /* delete bitmap */
    if (!b) winerr(); /* process windows error */
    win->pictbl[p].han = 0; /* set this entry free */

}

void delpict(FILE* f, int p)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    idelpict(win, p); /* delete picture file */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Load picture

Loads a picture into a slot of the loadable pictures array.

*******************************************************************************/

/* place extension on filename */
void setext(char* fnh, char* ext)

{

    int i, x; /* index for string */
    int f;    /* found flag */

    f = FALSE; /* set no extention found */
    /* search for extention */
    for (i = 1; i <= MAXFNM; i++) if (fnh[i] == '.') f = TRUE; /* found */
    if (!f) { /* no extention, place one */

        i = MAXFNM; /* index last character in string */
        while (i > 1 && fnh[i] == ' ') i = i-1;
        if (MAXFNM-i < 4) error(epicftl); /* filename too large */
        for (x = 1; x <= 4; x++) fnh[i+x] = ext[x]; /* place extention */

    }

}

/* find if file exists */
int exists(char *fn)
{

    DWORD atb;

    atb = GetFileAttributes(fn);

    return (atb != INVALID_FILE_ATTRIBUTES &&
            !(atb & FILE_ATTRIBUTE_DIRECTORY));

}

void iloadpict(winptr win, int p, char* fn)

{

    int    r;           /* result holder */
    BITMAP bmi;         /* bit map information header */
    char   fnh[MAXFNM]; /* file name holder */
    int    i;           /* index for string */

    if (strlen(fn) > MAXFNM) error(epicftl); /* filename too large */
    *fnh = 0; /* clear filename holding */
    strcpy(fnh, fn); /* copy */
    setext(fnh, ".bmp"); /* try bitmap first */
    if (!exists(fnh))  { /* try dib */

       setext(fnh, ".dib");
       if (!exists(fnh)) error(epicfnf); /* no file found */

    }
    if (p < 1 || p > MAXPIC)  error(einvhan); /* bad picture handle */
    /* if the slot is already occupied, delete that picture */
    if (win->pictbl[p].han) idelpict(win, p);
    /* load the image into memory */
    win->pictbl[p].han =
       LoadImage(NULL, fnh, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!win->pictbl[p].han) winerr(); /* process windows error */
    /* put it into a device context */
    win->pictbl[p].hdc = CreateCompatibleDC(win->devcon);
    if (!win->pictbl[p].hdc) winerr(); /* process windows error */
    /* select that to device context */
    win->pictbl[p].ohn = SelectObject(win->pictbl[p].hdc, win->pictbl[p].han);
    if (win->pictbl[p].ohn == HGDI_ERROR) error(enosel);
    /* get sizes */
    r = GetObject(win->pictbl[p].han, sizeof(BITMAP), &bmi);
    if (!r) winerr(); /* process windows error */
    win->pictbl[p].sx = bmi.bmWidth; /* set size x */
    win->pictbl[p].sy = bmi.bmHeight; /* set size x */

}

void loadpict(FILE* f, int p, char* fn)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iloadpict(win, p, fn); /* load picture file */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find size x of picture

Returns the size in x of the logical picture.

*******************************************************************************/

int pictsizx(FILE* f, int p)

{

    winptr win; /* window pointer */
    int    x;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p].han) error(einvhan); /* bad picture handle */
    x = win->pictbl[p].sx; /* return x size */
    unlockmain(); /* end exclusive access */

    return (x);

}

/*******************************************************************************

Find size y of picture

Returns the size in y of the logical picture.

*******************************************************************************/

int pictsizy(FILE* f, int p)

{

    winptr win; /* window pointer */
    int    y;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC)  error(einvhan); /* bad picture handle */
    if (!win->pictbl[p].han) error(einvhan); /* bad picture handle */
    y = win->pictbl[p].sy; /* return x size */
    unlockmain(); /* end exclusive access */

    return (y);

}

/*******************************************************************************

Draw picture

Draws a picture from the given file to the rectangle. The picture is resized
to the size of the rectangle.

Images will be kept in a rotating cache to prevent repeating reloads.

*******************************************************************************/

void ipicture(winptr win, int p, int x1, int y1, int x2, int y2)

{

    BOOL  b;    /* result holder */
    DWORD rop; /* rop holder */

    if (p < 1 || p > MAXPIC)  error(einvhan); /* bad picture handle */
    if (!win->pictbl[p].han) error(einvhan); /* bad picture handle */
    switch (win->screens[win->curupd]->fmod) { /* rop */

        case mdnorm:  rop = SRCCOPY; break; /* straight */
        case mdinvis: break; /* no-op */
        case mdxor:   rop = SRCINVERT; break; /* xor */

    }
    if (win->screens[win->curupd]->fmod != mdinvis) { /* ! a no-op */

        if (win->bufmod) { /* buffer mode on */

            /* paint to buffer */
            b = StretchBlt(win->screens[win->curupd]->bdc,
                           x1-1, y1-1, x2-x1+1, y2-y1+1,
                           win->pictbl[p].hdc, 0, 0,
                           win->pictbl[p].sx, win->pictbl[p].sy,
                           rop);
            if (!b) winerr(); /* process windows error */

        }
        if (indisp(win)) { /* paint to screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win);
            b = StretchBlt(win->devcon, x1-1, y1-1, x2-x1+1, y2-y1+1,
                           win->pictbl[p].hdc, 0, 0, win->pictbl[p].sx,
                           win->pictbl[p].sy, rop);
           if (!b) winerr(); /* process windows error */
           curon(win);

        }

    }

}

void picture(FILE* f, int p, int x1, int y1, int x2, int y2)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    ipicture(win, p, x1, y1, x2, y2); /* draw picture */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set viewport offset graphical

Sets the offset of the viewport in logical space, in pixels, anywhere from
-INT_MAX to INT_MAX.

*******************************************************************************/

void iviewoffg(winptr win, int x, int y)

{

    /* check change is needed */
    if (x != win->screens[win->curupd]->offx &&
        y != win->screens[win->curupd]->offy) {

        win->screens[win->curupd]->offx = x; /* set offsets */
        win->screens[win->curupd]->offy = y;
        win->goffx = x;
        win->goffy = y;
        iclear(win); /* clear buffer */

    }

}

void viewoffg(FILE* f, int x, int y)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iviewoffg(win, x, y); /* set viewport offset */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set viewport scale

Sets the viewport scale in x and y. The scale is a real fraction between 0 and
1, with 1 being 1:1 scaling. Viewport scales are allways smaller than logical
scales, which means that there are more than one logical pixel to map to a
given physical pixel, but never the reverse. This means that pixels are lost
in going to the display, but the display never needs to interpolate pixels
from logical pixels.

Note:

Right now, symmetrical scaling (both x and y scales set the same) are all that
works completely, since we don"t presently have a method to warp text to fit
a scaling process. However, this can be done by various means, including
painting into a buffer and transfering asymmetrically, or using outlines.

*******************************************************************************/

void iviewscale(winptr win, float x, float y)

{

    /* in this starting simplistic formula, the ratio is set x*INT_MAX/INT_MAX.
      it works, but can overflow for large coordinates || scales near 1 */
    win->screens[win->curupd]->wextx = 100;
    win->screens[win->curupd]->wexty = 100;
    win->screens[win->curupd]->vextx = trunc(x*100);
    win->screens[win->curupd]->vexty = trunc(y*100);
    win->gwextx = 100;
    win->gwexty = 100;
    win->gvextx = trunc(x*100);
    win->gvexty = trunc(y*100);
    iclear(win); /* clear buffer */

}

void viewscale(FILE* f, float x, float y)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    iviewscale(win, x, y); /* set viewport scale */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Print message string

This routine is for diagnostic use. Comment it out on production builds.

*******************************************************************************/

void prtmsgstr(int mn)

{

    prtnum(mn, 4, 16);
    prtstr(": ");
    if (mn >= 0x800 && mn <= 0xbfff) prtstr("User message");
    else if (mn >= 0xc000 && mn <= 0xffff) prtstr("Registered message");
    else switch (mn) {

        case 0x0000: prtstr("WM_NULL"); break;
        case 0x0001: prtstr("WM_CREATE"); break;
        case 0x0002: prtstr("WM_DESTROY"); break;
        case 0x0003: prtstr("WM_MOVE"); break;
        case 0x0005: prtstr("WM_SIZE"); break;
        case 0x0006: prtstr("WM_ACTIVATE"); break;
        case 0x0007: prtstr("WM_SETFOCUS"); break;
        case 0x0008: prtstr("WM_KILLFOCUS"); break;
        case 0x000A: prtstr("WM_ENABLE"); break;
        case 0x000B: prtstr("WM_SETREDRAW"); break;
        case 0x000C: prtstr("WM_SETTEXT"); break;
        case 0x000D: prtstr("WM_GETTEXT"); break;
        case 0x000E: prtstr("WM_GETTEXTLENGTH"); break;
        case 0x000F: prtstr("WM_PAINT"); break;
        case 0x0010: prtstr("WM_CLOSE"); break;
        case 0x0011: prtstr("WM_QUERYENDSESSION"); break;
        case 0x0012: prtstr("WM_QUIT"); break;
        case 0x0013: prtstr("WM_QUERYOPEN"); break;
        case 0x0014: prtstr("WM_ERASEBKGND"); break;
        case 0x0015: prtstr("WM_SYSCOLORCHANGE"); break;
        case 0x0016: prtstr("WM_ENDSESSION"); break;
        case 0x0018: prtstr("WM_ShowWindow"); break;
        case 0x001A: prtstr("WM_WININICHANGE"); break;
        case 0x001B: prtstr("WM_DEVMODECHANGE"); break;
        case 0x001C: prtstr("WM_ACTIVATEAPP"); break;
        case 0x001D: prtstr("WM_FONTCHANGE"); break;
        case 0x001E: prtstr("WM_TIMECHANGE"); break;
        case 0x001F: prtstr("WM_CANCELMODE"); break;
        case 0x0020: prtstr("WM_SETCURSOR"); break;
        case 0x0021: prtstr("WM_MOUSEACTIVATE"); break;
        case 0x0022: prtstr("WM_CHILDACTIVATE"); break;
        case 0x0023: prtstr("WM_QUEUESYNC"); break;
        case 0x0024: prtstr("WM_GETMINMAXINFO"); break;
        case 0x0026: prtstr("WM_PAINTICON"); break;
        case 0x0027: prtstr("WM_ICONERASEBKGND"); break;
        case 0x0028: prtstr("WM_NEXTDLGCTL"); break;
        case 0x002A: prtstr("WM_SPOOLERSTATUS"); break;
        case 0x002B: prtstr("WM_DRAWITEM"); break;
        case 0x002C: prtstr("WM_MEASUREITEM"); break;
        case 0x002D: prtstr("WM_DELETEITEM"); break;
        case 0x002E: prtstr("WM_VKEYTOITEM"); break;
        case 0x002F: prtstr("WM_CHARTOITEM"); break;
        case 0x0030: prtstr("WM_SETFONT"); break;
        case 0x0031: prtstr("WM_GETFONT"); break;
        case 0x0032: prtstr("WM_SETHOTKEY"); break;
        case 0x0033: prtstr("WM_GETHOTKEY"); break;
        case 0x0037: prtstr("WM_QUERYDRAGICON"); break;
        case 0x0039: prtstr("WM_COMPAREITEM"); break;
        case 0x0041: prtstr("WM_COMPACTING"); break;
        case 0x0042: prtstr("WM_OTHERWINDOWCREATED"); break;
        case 0x0043: prtstr("WM_OTHERWINDOWDESTROYED"); break;
        case 0x0044: prtstr("WM_COMMNOTIFY"); break;
        case 0x0045: prtstr("WM_HOTKEYEVENT"); break;
        case 0x0046: prtstr("WM_WINDOWPOSCHANGING"); break;
        case 0x0047: prtstr("WM_WINDOWPOSCHANGED"); break;
        case 0x0048: prtstr("WM_POWER"); break;
        case 0x004A: prtstr("WM_COPYDATA"); break;
        case 0x004B: prtstr("WM_CANCELJOURNAL"); break;
        case 0x004E: prtstr("WM_NOTIFY"); break;
        case 0x0050: prtstr("WM_INPUTLANGCHANGEREQUEST"); break;
        case 0x0051: prtstr("WM_INPUTLANGCHANGE"); break;
        case 0x0052: prtstr("WM_TCARD"); break;
        case 0x0053: prtstr("WM_HELP"); break;
        case 0x0054: prtstr("WM_USERCHANGED"); break;
        case 0x0055: prtstr("WM_NOTIFYFORMAT"); break;
        case 0x007B: prtstr("WM_CONTEXTMENU"); break;
        case 0x007C: prtstr("WM_STYLECHANGING"); break;
        case 0x007D: prtstr("WM_STYLECHANGED"); break;
        case 0x007E: prtstr("WM_DISPLAYCHANGE"); break;
        case 0x007F: prtstr("WM_GETICON"); break;
        case 0x0080: prtstr("WM_SETICON"); break;
        case 0x0081: prtstr("WM_NCCREATE"); break;
        case 0x0082: prtstr("WM_NCDESTROY"); break;
        case 0x0083: prtstr("WM_NCCALCSIZE"); break;
        case 0x0084: prtstr("WM_NCHITTEST"); break;
        case 0x0085: prtstr("WM_NCPAINT"); break;
        case 0x0086: prtstr("WM_NCACTIVATE"); break;
        case 0x0087: prtstr("WM_GETDLGCODE"); break;
        case 0x00A0: prtstr("WM_NCMOUSEMOVE"); break;
        case 0x00A1: prtstr("WM_NCLBUTTONDOWN"); break;
        case 0x00A2: prtstr("WM_NCLBUTTONUP"); break;
        case 0x00A3: prtstr("WM_NCLBUTTONDBLCLK"); break;
        case 0x00A4: prtstr("WM_NCRBUTTONDOWN"); break;
        case 0x00A5: prtstr("WM_NCRBUTTONUP"); break;
        case 0x00A6: prtstr("WM_NCRBUTTONDBLCLK"); break;
        case 0x00A7: prtstr("WM_NCMBUTTONDOWN"); break;
        case 0x00A8: prtstr("WM_NCMBUTTONUP"); break;
        case 0x00A9: prtstr("WM_NCMBUTTONDBLCLK"); break;
    /*  case 0x0100: prtstr("WM_KEYFIRST"); break;*/
        case 0x0100: prtstr("WM_KEYDOWN"); break;
        case 0x0101: prtstr("WM_KEYUP"); break;
        case 0x0102: prtstr("WM_CHAR"); break;
        case 0x0103: prtstr("WM_DEADCHAR"); break;
        case 0x0104: prtstr("WM_SYSKEYDOWN"); break;
        case 0x0105: prtstr("WM_SYSKEYUP"); break;
        case 0x0106: prtstr("WM_SYSCHAR"); break;
        case 0x0107: prtstr("WM_SYSDEADCHAR"); break;
        case 0x0108: prtstr("WM_KEYLAST"); break;
        case 0x0109: prtstr("WM_UNICHAR"); break;
        case 0x0110: prtstr("WM_INITDIALOG"); break;
        case 0x0111: prtstr("WM_COMMAND"); break;
        case 0x0112: prtstr("WM_SYSCOMMAND"); break;
        case 0x0113: prtstr("WM_TIMER"); break;
        case 0x0114: prtstr("WM_HSCROLL"); break;
        case 0x0115: prtstr("WM_VSCROLL"); break;
        case 0x0116: prtstr("WM_INITMENU"); break;
        case 0x0117: prtstr("WM_INITMENUPOPUP"); break;
        case 0x011F: prtstr("WM_MENUSELECT"); break;
        case 0x0120: prtstr("WM_MENUCHAR"); break;
        case 0x0121: prtstr("WM_ENTERIDLE"); break;
        case 0x0132: prtstr("WM_CTLCOLORMSGBOX"); break;
        case 0x0133: prtstr("WM_CTLCOLOREDIT"); break;
        case 0x0134: prtstr("WM_CTLCOLORLISTBOX"); break;
        case 0x0135: prtstr("WM_CTLCOLORBTN"); break;
        case 0x0136: prtstr("WM_CTLCOLORDLG"); break;
        case 0x0137: prtstr("WM_CTLCOLORSCROLLBAR"); break;
        case 0x0138: prtstr("WM_CTLCOLORSTATIC"); break;
     /*   case 0x0200: prtstr("WM_MOUSEFIRST"); break; */
        case 0x0200: prtstr("WM_MOUSEMOVE"); break;
        case 0x0201: prtstr("WM_LBUTTONDOWN"); break;
        case 0x0202: prtstr("WM_LBUTTONUP"); break;
        case 0x0203: prtstr("WM_LBUTTONDBLCLK"); break;
        case 0x0204: prtstr("WM_RBUTTONDOWN"); break;
        case 0x0205: prtstr("WM_RBUTTONUP"); break;
        case 0x0206: prtstr("WM_RBUTTONDBLCLK"); break;
        case 0x0207: prtstr("WM_MBUTTONDOWN"); break;
        case 0x0208: prtstr("WM_MBUTTONUP"); break;
        case 0x0209: prtstr("WM_MBUTTONDBLCLK"); break;
      /*   case 0x0209: prtstr("WM_MOUSELAST"); break; */
        case 0x0210: prtstr("WM_PARENTNOTIFY"); break;
        case 0x0211: prtstr("WM_ENTERMENULOOP"); break;
        case 0x0212: prtstr("WM_EXITMENULOOP"); break;
        case 0x0220: prtstr("WM_MDICREATE"); break;
        case 0x0221: prtstr("WM_MDIDESTROY"); break;
        case 0x0222: prtstr("WM_MDIACTIVATE"); break;
        case 0x0223: prtstr("WM_MDIRESTORE"); break;
        case 0x0224: prtstr("WM_MDINEXT"); break;
        case 0x0225: prtstr("WM_MDIMAXIMIZE"); break;
        case 0x0226: prtstr("WM_MDITILE"); break;
        case 0x0227: prtstr("WM_MDICASCADE"); break;
        case 0x0228: prtstr("WM_MDIICONARRANGE"); break;
        case 0x0229: prtstr("WM_MDIGETACTIVE"); break;
        case 0x0230: prtstr("WM_MDISetMenu"); break;
        case 0x0231: prtstr("WM_ENTERSIZEMOVE"); break;
        case 0x0232: prtstr("WM_EXITSIZEMOVE"); break;
        case 0x0233: prtstr("WM_DROPFILES"); break;
        case 0x0234: prtstr("WM_MDIREFRESHMENU"); break;
        case 0x0300: prtstr("WM_CUT"); break;
        case 0x0301: prtstr("WM_COPY"); break;
        case 0x0302: prtstr("WM_PASTE"); break;
        case 0x0303: prtstr("WM_CLEAR"); break;
        case 0x0304: prtstr("WM_UNDO"); break;
        case 0x0305: prtstr("WM_RENDERFORMAT"); break;
        case 0x0306: prtstr("WM_RENDERALLFORMATS"); break;
        case 0x0307: prtstr("WM_DESTROYCLIPBOARD"); break;
        case 0x0308: prtstr("WM_DRAWCLIPBOARD"); break;
        case 0x0309: prtstr("WM_PAINTCLIPBOARD"); break;
        case 0x030A: prtstr("WM_VSCROLLCLIPBOARD"); break;
        case 0x030B: prtstr("WM_SIZECLIPBOARD"); break;
        case 0x030C: prtstr("WM_ASKCBFORMATNAME"); break;
        case 0x030D: prtstr("WM_CHANGECBCHAIN"); break;
        case 0x030E: prtstr("WM_HSCROLLCLIPBOARD"); break;
        case 0x030F: prtstr("WM_QUERYNEWPALETTE"); break;
        case 0x0310: prtstr("WM_PALETTEISCHANGING"); break;
        case 0x0311: prtstr("WM_PALETTECHANGED"); break;
        case 0x0312: prtstr("WM_HOTKEY"); break;
        case 0x0380: prtstr("WM_PENWINFIRST"); break;
        case 0x038F: prtstr("WM_PENWINLAST"); break;
        case 0x03A0: prtstr("MM_JOY1MOVE"); break;
        case 0x03A1: prtstr("MM_JOY2MOVE"); break;
        case 0x03A2: prtstr("MM_JOY1ZMOVE"); break;
        case 0x03A3: prtstr("MM_JOY2ZMOVE"); break;
        case 0x03B5: prtstr("MM_JOY1BUTTONDOWN"); break;
        case 0x03B6: prtstr("MM_JOY2BUTTONDOWN"); break;
        case 0x03B7: prtstr("MM_JOY1BUTTONUP"); break;
        case 0x03B8: prtstr("MM_JOY2BUTTONUP"); break;
        default: prtstr("???"); break;

    }

}

/*******************************************************************************

Print message diagnostic

This routine is for diagnostic use. Comment it out on production builds.

*******************************************************************************/

void prtmsg(MSG m)

{

    prtstr("handle: ");
    prtnum((int)m.hwnd, 8, 16);
    prtstr(" message: ");
    prtmsgstr(m.message);
    prtstr(" wparam: ");
    prtnum(m.wParam, 8, 16);
    prtstr(" lparam: ");
    prtnum(m.lParam, 8, 16);
    prtstr("\r\n");

}

/*******************************************************************************

Print unpacked message diagnostic

This routine is for diagnostic use. Comment it out on production builds.

*******************************************************************************/

void prtmsgu(int hwnd, int imsg, int wparam, int lparam)

{

    prtstr("handle: ");
    prtnum((int)hwnd, 8, 16);
    prtstr(" message: ");
    prtmsgstr(imsg);
    prtstr(" wparam: ");
    prtnum(wparam, 8, 16);
    prtstr(" lparam: ");
    prtnum(lparam, 8, 16);
    prtstr("\r\n");

}

/*******************************************************************************

Aquire next input event

Waits for end returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

*******************************************************************************/

/*

Process keyboard event.
The events are mapped from IBM-PC keyboard keys as follows:

etup      up arrow            cursor up one line
etdown    down arrow          down one line
etleft    left arrow          left one character
etright   right arrow         right one character
etleftw   cntrl-left arrow    left one word
etrightw  cntrl-right arrow   right one word
ethome    cntrl-home          home of document
ethomes   shift-home          home of screen
ethomel   home                home of line
etend     cntrl-end           end of document
etends    shift-end           end of screen
etendl    end                 end of line
etscrl    shift-left arrow    scroll left one character
etscrr    shift-right arrow   scroll right one character
etscru    cntrl-up arrow      scroll up one line
etscrd    cntrl-down arrow    scroll down one line
etpagd    page down           page down
etpagu    page up             page up
ettab     tab                 tab
etenter   enter               enter line
etinsert  cntrl-insert        insert block
etinsertl shift-insert        insert line
etinsertt insert              insert toggle
etdel     cntrl-del           delete block
etdell    shift-del           delete line
etdelcf   del                 delete character forward
etdelcb   backspace           delete character backward
etcopy    cntrl-f1            copy block
etcopyl   shift-f1            copy line
etcan     esc                 cancel current operation
etstop    cntrl-s             stop current operation
etcont    cntrl-q             continue current operation
etprint   shift-f2            print document
etprintb  cntrl-f2            print block
etprints  cntrl-f3            print screen
etfun     f1-f12              int keys
etmenu    alt                 display menu
etend     cntrl-c             terminate program

This is equivalent to the CUA or Common User Access keyboard mapping with
various local extentions.

*/

void keyevent(pa_evtrec* er, MSG* msg, int* keep)

{

   if (msg->wParam == '\r') er->etype = pa_etenter; /* set enter line */
   else if (msg->wParam == '\b')
      er->etype = pa_etdelcb; /* set delete character backwards */
   else if (msg->wParam == '\t') er->etype = pa_ettab; /* set tab */
   else if (msg->wParam == 0x03) /*etx*/  {

      er->etype = pa_etterm; /* set } program */
      fend = TRUE; /* set } was ordered */

   } else if (msg->wParam == 0x13) /* xoff */
      er->etype = pa_etstop; /* set stop program */
   else if (msg->wParam == 0x11) /* xon */
      er->etype = pa_etcont; /* set continue program */
   else if (msg->wParam == 0x1b) /* esc */
      er->etype = pa_etcan; /* set cancel operation */
   else { /* normal character */

      er->etype = pa_etchar; /* set character event */
      er->echar = msg->wParam;

   }
   *keep = TRUE; /* set keep event */

}

void ctlevent(winptr win, pa_evtrec* er, MSG* msg, int* keep)

{

    /* find key we process */
    switch (msg->wParam) { /* key */
        case VK_HOME: /* home */
            if (win->cntrl) er->etype = pa_ethome; /* home document */
            else if (win->shift) er->etype = pa_ethomes; /* home screen */
            else er->etype = pa_ethomel; /* home line */
            break;

        case VK_END: /* end */
            if (win->cntrl) er->etype = pa_etend; /* } document */
            else if (win->shift) er->etype = pa_etends; /* } screen */
            else er->etype = pa_etendl; /* } line */
            break;

        case VK_UP: /* up */
            if (win->cntrl) er->etype = pa_etscru; /* scroll up */
            else er->etype = pa_etup; /* up line */
            break;

        case VK_DOWN: /* down */
            if (win->cntrl) er->etype = pa_etscrd; /* scroll down */
            else er->etype = pa_etdown; /* up line */
            break;

        case VK_LEFT: /* left */
            if (win->cntrl) er->etype = pa_etleftw; /* left one word */
            else if (win->shift) er->etype = pa_etscrl; /* scroll left one character */
            else er->etype = pa_etleft; /* left one character */
            break;

        case VK_RIGHT: /* right */
            if (win->cntrl) er->etype = pa_etrightw; /* right one word */
            else if (win->shift) er->etype = pa_etscrr; /* scroll right one character */
            else er->etype = pa_etright; /* left one character */
            break;

        case VK_INSERT: /* insert */
            if (win->cntrl) er->etype = pa_etinsert; /* insert block */
            else if (win->shift) er->etype = pa_etinsertl; /* insert line */
            else er->etype = pa_etinsertt; /* insert toggle */
            break;

        case VK_DELETE: /* delete */
            if (win->cntrl) er->etype = pa_etdel; /* delete block */
            else if (win->shift) er->etype = pa_etdell; /* delete line */
            else er->etype = pa_etdelcf; /* insert toggle */
            break;

        case VK_PRIOR: er->etype = pa_etpagu; /* page up */
        case VK_NEXT: er->etype = pa_etpagd; /* page down */
        case VK_F1: /* f1 */
            if (win->cntrl) er->etype = pa_etcopy; /* copy block */
            else if (win->shift) er->etype = pa_etcopyl; /* copy line */
            else { /* f1 */

                er->etype = pa_etfun;
                er->fkey = 1;

            }
            break;

        case VK_F2: /* f2 */
            if (win->cntrl) er->etype = pa_etprintb; /* print block */
            else if (win->shift) er->etype = pa_etprint; /* print document */
            else { /* f2 */

                er->etype = pa_etfun;
                er->fkey = 2;

            }
            break;

        case VK_F3: /* f3 */
            if (win->cntrl) er->etype = pa_etprints; /* print screen */
            else { /* f3 */

                er->etype = pa_etfun;
                er->fkey = 3;

            }
            break;

        case VK_F4: /* f4 */
            er->etype = pa_etfun;
            er->fkey = 4;
            break;

        case VK_F5: /* f5 */
            er->etype = pa_etfun;
            er->fkey = 5;
            break;

        case VK_F6: /* f6 */
            er->etype = pa_etfun;
            er->fkey = 6;
            break;

        case VK_F7: /* f7 */
            er->etype = pa_etfun;
            er->fkey = 7;
            break;

        case VK_F8: /* f8 */
            er->etype = pa_etfun;
            er->fkey = 8;
            break;

        case VK_F9: /* f9 */
            er->etype = pa_etfun;
            er->fkey = 9;
            break;

        case VK_F10: /* f10 */
            er->etype = pa_etfun;
            er->fkey = 10;
            break;

        case VK_F11: /* f11 */
            er->etype = pa_etfun;
            er->fkey = 11;
            break;

        case VK_F12: /* f12 */
            er->etype = pa_etfun;
            er->fkey = 12;
            break;

        case VK_MENU: er->etype = pa_etmenu; break; /* alt */
        case VK_CANCEL: er->etype = pa_etterm; break; /* ctl-brk */

    }
    *keep = TRUE; /* set keep event */

}

/*

Process mouse event.
Buttons are assigned to Win 95 as follows:

button 1: Windows left button
button 2: Windows right button
button 3: Windows middle from left button

Button 4 is unused. Double clicks will be ignored, displaying my utter
contempt for the whole double click concept.

*/

/* update mouse parameters */

void mouseupdate(winptr win, pa_evtrec* er, int* keep)

{

    /* we prioritize events by: movements 1st, button clicks 2nd */
    if (win->nmpx != win->mpx || win->nmpy != win->mpy) {

      /* create movement event */
       er->etype = pa_etmoumov; /* set movement event */
       er->mmoun = 1; /* mouse 1 */
       er->moupx = win->nmpx; /* set new mouse position */
       er->moupy = win->nmpy;
       win->mpx = win->nmpx; /* save new position */
       win->mpy = win->nmpy;
       *keep = TRUE; /* set to keep */

    } else if (win->nmpxg != win->mpxg || win->nmpyg != win->mpyg) {

       /* create graphical movement event */
       er->etype = pa_etmoumovg; /* set movement event */
       er->mmoung = 1; /* mouse 1 */
       er->moupxg = win->nmpxg; /* set new mouse position */
       er->moupyg = win->nmpyg;
       win->mpxg = win->nmpxg; /* save new position */
       win->mpyg = win->nmpyg;
       *keep = TRUE; /* set to keep */

    } else if (win->nmb1 > win->mb1) {

       er->etype = pa_etmouba; /* button 1 assert */
       er->amoun = 1; /* mouse 1 */
       er->amoubn = 1; /* button 1 */
       win->mb1 = win->nmb1; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb2 > win->mb2) {

       er->etype = pa_etmouba; /* button 2 assert */
       er->amoun = 1; /* mouse 1 */
       er->amoubn = 2; /* button 2 */
       win->mb2 = win->nmb2; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb3 > win->mb3) {

       er->etype = pa_etmouba; /* button 3 assert */
       er->amoun = 1; /* mouse 1 */
       er->amoubn = 3; /* button 3 */
       win->mb3 = win->nmb3; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb1 < win->mb1) {

       er->etype = pa_etmoubd; /* button 1 deassert */
       er->dmoun = 1; /* mouse 1 */
       er->dmoubn = 1; /* button 1 */
       win->mb1 = win->nmb1; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb2 < win->mb2) {

       er->etype = pa_etmoubd; /* button 2 deassert */
       er->dmoun = 1; /* mouse 1 */
       er->dmoubn = 2; /* button 2 */
       win->mb2 = win->nmb2; /* update status */
       *keep = TRUE; /* set to keep */

    } else if (win->nmb3 < win->mb3) {

       er->etype = pa_etmoubd; /* button 3 deassert */
       er->dmoun = 1; /* mouse 1 */
       er->dmoubn = 3; /* button 3 */
       win->mb3 = win->nmb3; /* update status */
       *keep = TRUE; /* set to keep */

    }

}

/* register mouse status */

void mouseevent(winptr win, MSG* msg)

{

    win->nmpx = msg->lParam%65536/win->charspace+1; /* get mouse x */
    win->nmpy = msg->lParam/65536/win->linespace+1; /* get mouse y */
    win->nmpxg = msg->lParam%65536+1; /* get mouse graphical x */
    win->nmpyg = msg->lParam/65536+1; /* get mouse graphical y */
    /* set new button statuses */
    if (msg->message == WM_LBUTTONDOWN) win->nmb1 = TRUE;
    if (msg->message == WM_LBUTTONUP) win->nmb1 = FALSE;
    if (msg->message == WM_MBUTTONDOWN) win->nmb2 = TRUE;
    if (msg->message == WM_MBUTTONUP) win->nmb2 = FALSE;
    if (msg->message == WM_RBUTTONDOWN) win->nmb3 = TRUE;
    if (msg->message == WM_RBUTTONUP) win->nmb3 = FALSE;

}

/* queue event to window */

void enqueue(eqeptr* el, pa_evtrec* er)

{

    eqeptr ep; /* pointer to queue entries */

    geteqe(&ep); /* get a new event container */
    memcpy(&ep->evt, er, sizeof(pa_evtrec)); /* copy event to container */
    /* insert into bubble list */
    if (el == NULL) { /* list empty, place as first entry */

        ep->last = ep; /* tie entry to itself */
        ep->next = ep;

    } else { /* list has entries */

        ep->last = *el; /* link last to current */
        ep->next = (*el)->next; /* link next to next of current */
        (*el)->next = ep; /* link current to new */

    }
    *el = ep; /* set that as new root */
    /* ok, new entries are moving to the last direction, and we remove entries
       from the next direction. */

}

/* issue event for changed button */

void updn(pa_evtrec* er, MSG* msg, int ofn, int bn, int bm, int* keep)

{

   /* if there is already a message processed, enqueue that */
   if (*keep) enqueue(&opnfil[opnfil[ofn]->inl]->evt, er); /* queue it */
   if (msg->wParam && bm) { /* assert */

      er->etype = pa_etjoyba; /* set assert */
      if (msg->message == MM_JOY1BUTTONDOWN ||
          msg->message == MM_JOY1BUTTONUP) er->ajoyn = 1;
      else er->ajoyn = 2;
      er->ajoybn = bn; /* set number */

   } else { /* deassert */

      er->etype = pa_etjoybd; /* set deassert */
      if (msg->message == MM_JOY1BUTTONDOWN ||
          msg->message == MM_JOY1BUTTONUP) er->ajoyn = 1;
      else er->ajoyn = 2;
      er->djoybn = bn; /* set number */

   }
   *keep = TRUE; /* set keep event */

}

/* process joystick messages */

void joymes(pa_evtrec* er, MSG* msg, int ofn, int* keep)

{

   /* register changes on each button */
   if (msg->wParam & JOY_BUTTON1CHG) updn(er, msg, ofn, 1, JOY_BUTTON1, keep);
   if (msg->wParam & JOY_BUTTON2CHG) updn(er, msg, ofn, 2, JOY_BUTTON2, keep);
   if (msg->wParam & JOY_BUTTON3CHG) updn(er, msg, ofn, 3, JOY_BUTTON3, keep);
   if (msg->wParam & JOY_BUTTON4CHG) updn(er, msg, ofn, 4, JOY_BUTTON4, keep);

}

/* process windows messages to event */

void winevt(winptr win, pa_evtrec* er, MSG* msg, int ofn, int* keep)

{

    RECT   cr;         /* client rectangle */
    wigptr wp;         /* widget entry pointer */
    int    r;          /* result holder */
    int    b;          /* int result */
    int    v;          /* value */
    int    x, y, z;    /* joystick readback */
    int    dx, dy, dz; /* joystick readback differences */
    int    nm;         /* notification message */
    float  f;          /* floating point temp */
    NMHDR* nhp;        /* notification header */

    if (msg->message == WM_PAINT)  { /* window paint */

        if (!win->bufmod) { /* our client handles it"s own redraws */

            /* form redraw request */
            b = GetUpdateRect(win->winhan, &cr, FALSE);
            er->etype = pa_etredraw; /* set redraw message */
            er->rsx = msg->wParam/0x10000; /* fill the rectangle with update */
            er->rsy = msg->wParam%0x10000;
            er->rex = msg->lParam/0x10000;
            er->rey = msg->lParam%0x10000;
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == WM_SIZE) { /* resize */

        if (!win->bufmod) { /* main thread handles resizes */

            /* check if maximize, minimize, || exit from either mode */
            if (msg->wParam == SIZE_MAXIMIZED)  {

                er->etype = pa_etmax; /* set maximize event */
                /* save the event ahead of the resize */
                enqueue(&opnfil[opnfil[ofn]->inl]->evt, er); /* queue it */

            } else if (msg->wParam == SIZE_MINIMIZED)  {

                er->etype = pa_etmin; /* set minimize event */
                /* save the event ahead of the resize */
                enqueue(&opnfil[opnfil[ofn]->inl]->evt, er); /* queue it */

            } else if (msg->wParam == SIZE_RESTORED &&
                       (win->sizests == SIZE_MINIMIZED ||
                        win->sizests == SIZE_MAXIMIZED)) {

                /* window is restored, && was minimized || maximized */
                er->etype = pa_etnorm; /* set normalize event */
                /* save the event ahead of the resize */
                enqueue(&opnfil[opnfil[ofn]->inl]->evt, er); /* queue it */

            }
            win->sizests = msg->wParam; /* save size status */
            /* process resize message */
            win->gmaxxg = msg->lParam && 0xffff; /* set x size */
            win->gmaxyg = msg->lParam / 65536 && 0xffff; /* set y size */
            win->gmaxx = win->gmaxxg / win->charspace; /* find character size x */
            win->gmaxy = win->gmaxyg / win->linespace; /* find character size y */
            win->screens[win->curdsp]->maxx = win->gmaxx; /* copy to screen control */
            win->screens[win->curdsp]->maxy = win->gmaxy;
            win->screens[win->curdsp]->maxxg = win->gmaxxg;
            win->screens[win->curdsp]->maxyg = win->gmaxyg;
            /* place the resize message */
            er->etype = pa_etresize; /* set resize message */
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == WM_CHAR) keyevent(er, msg, keep); /* process characters */
    else if (msg->message == WM_KEYDOWN) {

        if (msg->wParam == VK_SHIFT) win->shift = TRUE; /* set shift active */
        if (msg->wParam == VK_CONTROL) win->cntrl = TRUE; /* set control active */
        ctlevent(win, er, msg, keep); /* process control character */

    } else if (msg->message == WM_KEYUP) {

        if (msg->wParam == VK_SHIFT) win->shift = FALSE; /* set shift inactive */
        if (msg->wParam == VK_CONTROL) win->cntrl = FALSE; /* set control inactive */

    } else if (msg->message == WM_QUIT || msg->message == WM_CLOSE) {

        er->etype = pa_etterm; /* set terminate */
        fend = TRUE; /* set } of program ordered */
        *keep = TRUE; /* set keep event */

    } else if (msg->message == WM_MOUSEMOVE || msg->message == WM_LBUTTONDOWN ||
               msg->message == WM_LBUTTONUP || msg->message == WM_MBUTTONDOWN ||
               msg->message == WM_MBUTTONUP || msg->message == WM_RBUTTONDOWN ||
               msg->message == WM_RBUTTONUP) {

        mouseevent(win, msg); /* mouse event */
        mouseupdate(win, er, keep); /* check any mouse details need processing */

    } else if (msg->message == WM_TIMER) {

        /* check its a standard timer */
        if (msg->wParam > 0 && msg->wParam <= PA_MAXTIM)  {

            er->etype = pa_ettim; /* set timer event occurred */
            er->timnum = msg->wParam; /* set what timer */
            *keep = TRUE; /* set keep event */

        } else if (msg->wParam == FRMTIM) { /* its the framing timer */

            er->etype = pa_etframe; /* set frame event occurred */
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY2MOVE ||
               msg->message == MM_JOY1ZMOVE || msg->message == MM_JOY2ZMOVE) {

        er->etype = pa_etjoymov; /* set joystick moved */
        /* set what joystick */
        if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY1ZMOVE)
            er->mjoyn = 1;
        else er->mjoyn = 2;
        /* Set all variables to default to same. This way, only the joystick
           axes that are actually set by the message are registered. */
        if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY1ZMOVE) {

            x = win->joy1xs;
            y = win->joy1ys;
            z = win->joy1zs;

        } else {

            x = win->joy2xs;
            y = win->joy2ys;
            z = win->joy2zs;

        }
        /* If it's an x/y move, split the x and y axies parts of the message
           up. */
        if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY2MOVE) {

            x = LOWORD(msg->lParam); /* get x and y for joystick */
            y = HIWORD(msg->lParam);

        /* For z axis, get a single variable. */
        } else z = msg->lParam & 0xffff;
        /* We perform thresholding on the joystick right here, which is
           limited to 255 steps (same as joystick hardware. find joystick
         diffs && update */
        if (msg->message == MM_JOY1MOVE || msg->message == MM_JOY1ZMOVE) {

            dx = abs(win->joy1xs-x); /* find differences */
            dy = abs(win->joy1ys-y);
            dz = abs(win->joy1zs-z);
            win->joy1xs = x; /* place old values */
            win->joy1ys = y;
            win->joy1zs = z;

        } else {

            dx = abs(win->joy2xs-x); /* find differences */
            dy = abs(win->joy2ys-y);
            dz = abs(win->joy2zs-z);
            win->joy2xs = x; /* place old values */
            win->joy2ys = y;
            win->joy2zs = z;

        }
        /* now reject moves below the threshold */
        if (dx > 65535 / 255 || dy > 65535 / 255 || dz > 65535 / 255) {

            /* scale axies between -INT_MAX..INT_MAX && place */
            er->joypx = (x - 32767)*(INT_MAX/32768);
            er->joypy = (y - 32767)*(INT_MAX/32768);
            er->joypz = (z - 32767)*(INT_MAX/32768);
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == MM_JOY1BUTTONDOWN ||
               msg->message == MM_JOY2BUTTONDOWN ||
               msg->message == MM_JOY1BUTTONUP ||
               msg->message == MM_JOY2BUTTONUP) joymes(er, msg, ofn, keep);
    else if (msg->message == WM_COMMAND) {

        if (msg->lParam) { /* it"s a widget */

            wp = fndwig(win, msg->wParam && 0xffff); /* find the widget */
            if (wp == NULL) error(esystem); /* should be in the list */
            nm = msg->wParam / 0x10000; /* get notification message */
            switch (wp->typ) { /* widget type */

                case wtbutton: /* button */
                    if (nm == BN_CLICKED) {

                        er->etype = pa_etbutton; /* set button assert event */
                        er->butid = wp->id; /* get widget id */
                        *keep = TRUE; /* set keep event */

                    }
                    break;

                case wtcheckbox: /* checkbox */
                    er->etype = pa_etchkbox; /* set checkbox select event */
                    er->ckbxid = wp->id; /* get widget id */
                    *keep = TRUE; /* set keep event */
                    break;

                case wtradiobutton: /* radio button */
                    er->etype = pa_etradbut; /* set checkbox select event */
                    er->radbid = wp->id; /* get widget id */
                    *keep = TRUE; /* set keep event */
                    break;

                case wtgroup: ; /* group box, gives no messages */
                case wtbackground: ; /* background box, gives no messages */
                case wtscrollvert: ; /* scrollbar, gives no messages */
                case wtscrollhoriz: ; /* scrollbar, gives no messages */
                case wteditbox: ; /* edit box, requires no messages */
                case wtlistbox: /* list box */
                    if (nm == LBN_DBLCLK) {

                        unlockmain(); /* end exclusive access */
                        r = SendMessage(wp->han, LB_GETCURSEL, 0, 0);
                        lockmain(); /* start exclusive access */
                        if (r == -1) error(esystem); /* should be a select */
                        er->etype = pa_etlstbox; /* set list box select event */
                        er->lstbid = wp->id; /* get widget id */
                        er->lstbsl = r+1; /* set selection */
                        *keep = TRUE; /* set keep event */

                    }
                    break;

                case wtdropbox: /* drop box */
                    if (nm == CBN_SELENDOK) {

                        unlockmain(); /* end exclusive access */
                        r = SendMessage(wp->han, CB_GETCURSEL, 0, 0);
                        lockmain(); /* start exclusive access */
                        if (r == -1) error(esystem); /* should be a select */
                        er->etype = pa_etdrpbox; /* set list box select event */
                        er->drpbid = wp->id; /* get widget id */
                        er->drpbsl = r+1; /* set selection */
                        *keep = TRUE; /* set keep event */

                    }
                    break;

                case wtdropeditbox: /* drop edit box */
                    if (nm == CBN_SELENDOK)  {

                        er->etype = pa_etdrebox; /* set list box select event */
                        er->drebid = wp->id; /* get widget id */
                        *keep = TRUE; /* set keep event */

                    }
                    break;

                case wtslidehoriz: break;;
                case wtslidevert: break;;
                case wtnumselbox: break;;

            }

        } else { /* it"s a menu select */

            er->etype = pa_etmenus; /* set menu select event */
            er->menuid = msg->wParam && 0xffff; /* get menu id */
            *keep = TRUE; /* set keep event */

        }

    } else if (msg->message == WM_VSCROLL)  {

        v = msg->wParam && 0xffff; /* find subcommand */
        if (v == SB_THUMBTRACK ||
            v == SB_LINEUP || v == SB_LINEDOWN ||
            v == SB_PAGEUP || v == SB_PAGEDOWN) {

            /* position request */
            wp = fndwighan(win, (HWND)msg->lParam); /* find widget tracking entry */
            if (wp == NULL) error(esystem); /* should have been found */
            if (wp->typ == wtscrollvert) { /* scroll bar */

                if (v == SB_LINEUP) {

                    er->etype = pa_etsclull; /* line up */
                    er->sclulid = wp->id;

                } else if (v == SB_LINEDOWN) {

                    er->etype = pa_etscldrl; /* line down */
                    er->scldlid = wp->id;

                } else if (v == SB_PAGEUP) {

                    er->etype = pa_etsclulp; /* page up */
                    er->sclupid = wp->id;

                } else if (v == SB_PAGEDOWN) {

                    er->etype = pa_etscldrp; /* page down */
                    er->scldpid = wp->id;

                } else {

                    er->etype = pa_etsclpos; /* set scroll position event */
                    er->sclpid = wp->id; /* set widget id */
                    f = msg->wParam/0x10000; /* get current position to float */
                    /* clamp to INT_MAX */
                    if (f*INT_MAX/(255-wp->siz) > INT_MAX) er->sclpos = INT_MAX;
                    else er->sclpos = f*INT_MAX/(255-wp->siz);
                    /*er->sclpos = msg->wParam / 65536*0x800000*/ /* get position */

                }
                *keep = TRUE; /* set keep event */

            } else if (wp->typ == wtslidevert) { /* slider */

                er->etype = pa_etsldpos; /* set scroll position event */
                er->sldpid = wp->id; /* set widget id */
                /* get position */
                if (v == SB_THUMBTRACK) /* message includes position */
                    er->sldpos = msg->wParam/65536*(INT_MAX/100);
                else { /* must retrive the position by message */

                    unlockmain(); /* end exclusive access */
                    r = SendMessage(wp->han, TBM_GETPOS, 0, 0);
                    lockmain(); /* start exclusive access */
                    er->sldpos = r*(INT_MAX/100); /* set position */

                }
                *keep = TRUE; /* set keep event */

            } else error(esystem); /* should be one of those */

        }

    } else if (msg->message == WM_HSCROLL) {

        v = msg->wParam && 0xffff; /* find subcommand */
        if (v == SB_THUMBTRACK || v == SB_LINELEFT || v == SB_LINERIGHT ||
            v == SB_PAGELEFT || v == SB_PAGERIGHT) {

            /* position request */
            wp = fndwighan(win, (HWND)msg->lParam); /* find widget tracking entry */
            if (!wp) error(esystem); /* should have been found */
            if (wp->typ == wtscrollhoriz)  { /* scroll bar */

                if (v == SB_LINELEFT) {

                    er->etype = pa_etsclull; /* line up */
                    er->sclulid = wp->id;

                } else if (v == SB_LINERIGHT) {

                    er->etype = pa_etscldrl; /* line down */
                    er->scldlid = wp->id;

                } else if (v == SB_PAGELEFT) {

                    er->etype = pa_etsclulp; /* page up */
                    er->sclupid = wp->id;

                } else if (v == SB_PAGERIGHT) {

                    er->etype = pa_etscldrp; /* page down */
                    er->scldpid = wp->id;

                } else {

                    er->etype = pa_etsclpos; /* set scroll position event */
                    er->sclpid = wp->id; /* set widget id */
                    er->sclpos = msg->wParam / 65536*0x800000; /* get position */

                }
                *keep = TRUE; /* set keep event */

            } else if (wp->typ == wtslidehoriz) { /* slider */

                er->etype = pa_etsldpos; /* set scroll position event */
                er->sldpid = wp->id; /* set widget id */
                /* get position */
                if (v == SB_THUMBTRACK) /* message includes position */
                    er->sldpos = msg->wParam/65536*(INT_MAX/100);
                else { /* must retrive the position by message */

                    unlockmain(); /* end exclusive access */
                    r = SendMessage(wp->han, TBM_GETPOS, 0, 0);
                    lockmain(); /* start exclusive access */
                    er->sldpos = r*(INT_MAX/100); /* set position */

                }
                *keep = TRUE; /* set keep event */

            } else error(esystem); /* should be one of those */

        }

    } else if (msg->message == WM_NOTIFY) {

        wp = fndwig(win, msg->wParam); /* find widget tracking entry */
        if (!wp) error(esystem); /* should have been found */
        nhp = (NMHDR*)msg->lParam; /* convert lparam to record pointer */
        v = nhp->code; /* get code */
        /* no, I don"t know why this works, or what the TCN_SELCHANGE code is.
           Tab controls are giving me multiple indications, and the
           TCN_SELCHANGE code is more reliable as a selection indicator. */
        if (v == TCN_SELCHANGE) {

            unlockmain(); /* end exclusive access */
            r = SendMessage(wp->han, TCM_GETCURSEL, 0, 0);
            lockmain(); /* start exclusive access */
            er->etype = pa_ettabbar; /* set tab bar type */
            er->tabid = wp->id; /* set id */
            er->tabsel = r+1; /* set tab number */
            *keep = TRUE; /* keep event */

        }

    } else if (msg->message == UMEDITCR) {

        wp = fndwig(win, msg->wParam); /* find widget tracking entry */
        if (!wp) error(esystem); /* should have been found */
        er->etype = pa_etedtbox; /* set edit box complete event */
        er->edtbid = wp->id; /* get widget id */
        *keep = TRUE; /* set keep event */

    } else if (msg->message == UMNUMCR) {

        wp = fndwig(win, msg->wParam); /* find widget tracking entry */
        if (!wp) error(esystem); /* should have been found */
        er->etype = pa_etnumbox; /* set number select box complete event */
        er->numbid = wp->id; /* get widget id */
        er->numbsl = msg->lParam; /* set number selected */
        *keep = TRUE; /* set keep event */

    }

}

void sigevt(pa_evtrec* er, MSG* msg, int* keep)

{

    if (msg->message == WM_QUIT || msg->message == WM_CLOSE)  {

        er->etype = pa_etterm; /* set terminate */
        fend = TRUE; /* set end of program ordered */
        *keep = TRUE; /* set keep event */

    }

}

void ievent(int ifn, pa_evtrec* er)

{

    MSG    msg;  /* windows message */
    int    keep; /* keep event flag */
    winptr win;  /* pointer to windows structure */
    int    ofn;  /* file handle from incoming message */
    eqeptr ep;   /* event queuing pointer */
    int    b;    /* return value */

    /* Windows gdi caches, which can cause written graphics to pause uncompleted
       while we await user input. This next causes a sync-up. */
    b = GdiFlush();
    /* check there are events waiting on the input queue */
    if (opnfil[ifn]->evt) {

        /* We pick one, && only one, event off the input queue. The messages are
           removed in fifo order. */
        ep = opnfil[ifn]->evt->next; /* index the entry to dequeue */
        *er = ep->evt; /* get the queued event */
        /* if this is the last entry in the queue, just empty the list */
        if (ep->next == ep) opnfil[ifn]->evt = NULL;
        else { /* ! last entry */

            ep->next->last = ep->last; /* link next to last */
            ep->last->next = ep->next; /* link last to next */
            puteqe(ep); /* release entry */

        }

    } else do {

        keep = FALSE; /* set don"t keep by default */
        /* get next message */
        getmsg(&msg);
        /* get the logical output file from Windows handle */
        ofn = hwn2lfn(msg.hwnd);
/*;prtstr("ofn: "); prtnum(ofn, 8, 16); prtstr(" "); prtmsg(msg);*/
        /* A message can have a window associated with it, or be sent anonymously.
           Anonymous messages are typically intertask housekeeping signals. */
        if (ofn > 0)  {

            win = lfn2win(ofn); /* index window from output file */
            er->winid = filwin[ofn]; /* set window id */
            winevt; /* process messsage */
            if (!keep) sigevt(er, &msg, &keep); /* if not found, try intertask signal */

        } else sigevt; /* process signal */
        if (keep && (ofn > 0))  { /* we have an event, and not main */

            /* check and error if no logical input file is linked to this output
               window */
            if (!opnfil[ofn]->inl) error(esystem);
            if (opnfil[ofn]->inl != ifn) {

                /* The event is ! for the input agent that is calling us. We will
                   queue the message up on the input file it is int}ed for. Why
                   would this happen ? Only if the program is switching between
                   input channels, since each input is locked to a task. */
                enqueue(&opnfil[opnfil[ofn]->inl]->evt, er);
                /* Now, keep receiving events until one matches the input file we
                   were looking for. */
                keep = FALSE;

            }

        }

    } while (!keep); /* until an event we want occurs */

} /* ievent */

/* external event interface */

void event(FILE* f, pa_evtrec* er)

{

    lockmain(); /* start exclusive access */
    /* get logical input file number for input, && get the event for that. */
    ievent(txt2lfn(f), er); /* process event */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Wait for intratask message

Waits for the given intratask message. Discards any other messages or
intratask messages. The im is returned back to free, which matches the common
use of im to just return the entry as acknowledgement.

*******************************************************************************/

void waitim(imcode m, imptr* ip)

{

    int done; /* done flag */
    MSG msg;  /* message */

    done = FALSE; /* set ! done */
    do {

        igetmsg(&msg); /* get next message */
        if (msg.message == UMIM) { /* receive im */

            *ip = (imptr)msg.wParam; /* get im pointer */
            if ((*ip)->im == m) done = TRUE; /* found it */
            putitm(*ip); /* release im entry */

        }

    } while (!done); /* until message found */

}

/*******************************************************************************

Timer handler procedure

Called by the callback thunk set with TimeSetEvent. We are given a user word
of data, in which we multiplex the two peices of data we need, the logical
file number for the window file, from which we get the windows handle, and
the timer number.

With that data, we  post a message back to the queue, containing the
graph number of the timer that went off.

The reason we multiplex the logical file number is because the windows handle,
which we need, has a range determined by Windows, && we have to assume it
occupies a full word. The alternatives to multiplexing were to have the timer
callback thunk be customized, which is a non-standard solution, or use one
of the other parameters Windows passes to this function, which are not
documented.

*******************************************************************************/

void CALLBACK timeout(UINT id, UINT msg, DWORD_PTR usr, DWORD_PTR dw1,
                      DWORD_PTR dw2)

{

    int fn; /* logical file number */
    HWND wh; /* window handle */

    lockmain(); /* start exclusive access */
    fn = usr/PA_MAXTIM; /* get lfn multiplexed in user data */
    /* Validate it, but do nothing if wrong. We just don"t want to crash on
       errors here. */
    if (fn >= 1 && fn <= MAXFIL)  /* valid lfn */
       if (opnfil[fn]) /* file is defined */
          if (opnfil[fn]->win) { /* file has window context */

        wh = opnfil[fn]->win->winhan; /* get window handle */
        unlockmain(); /* end exclusive access */
        putmsg(wh, WM_TIMER, usr%PA_MAXTIM /* multiplexed timer number*/, 0);

    } else unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.

Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it sends a timer mature event to
the associated input file.

*******************************************************************************/

void itimer(winptr win, /* file to send event to */
            int    lf,  /* logical file number */
            int    i,   /* timer handle */
            int    t,   /* number of tenth-milliseconds to run */
            int    r)   /* timer is to rerun after completion */

{

    int tf; /* timer flags */
    int mt; /* millisecond time */

    if (i < 1 || i > PA_MAXTIM)  error(etimnum); /* bad timer number */
    mt = t / 10; /* find millisecond time */
    if (!mt) mt = 1; /* fell below minimum, must be >= 1 */
    /* set flags for timer */
    tf = TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS;
    /* set repeat/one shot status */
    if (r) tf |= TIME_PERIODIC;
    else tf |= TIME_ONESHOT;
    /* We need both the timer number, && the window number in the handler,
      but we only have a single callback parameter available. So we mux
      them together in a word. */
    win->timers[i].han = timeSetEvent(mt, 0, timeout, lf*PA_MAXTIM+i, tf);
    if (!win->timers[i].han) error(etimacc); /* no timer available */
    win->timers[i].rep = r; /* set timer repeat flag */
    /* should check and return an error */

}

void timer(FILE* f, /* file to send event to */
           int   i, /* timer handle */
           int   t, /* number of tenth-milliseconds to run */
           int   r) /* timer is to rerun after completion */

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* index output file */
    itimer(win, txt2lfn(f), i, t, r); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Kill timer

Kills a given timer, by it"s id number. Only repeating timers should be killed.

*******************************************************************************/

void ikilltimer(winptr win, /* file to kill timer on */
                int    i)   /* handle of timer */

{

    MMRESULT r; /* return value */

    if (i < 1 || i > PA_MAXTIM) error(etimnum); /* bad timer number */
    r = timeKillEvent(win->timers[i].han); /* kill timer */
    if (r) error(etimacc); /* error */

}

void killtimer(FILE* f, /* file to kill timer on */
               int   i) /* handle of timer */

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* index output file */
    ikilltimer(win, i); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

void iframetimer(winptr win, int lf, int e)

{

    if (e) { /* enable framing timer */

        if (!win->frmrun) { /* it is ! running */

            /* set timer to run, 17ms */
            win->frmhan = timeSetEvent(17, 0, timeout, lf*PA_MAXTIM+FRMTIM,
                                       TIME_CALLBACK_FUNCTION ||
                                       TIME_KILL_SYNCHRONOUS ||
                                       TIME_PERIODIC);
            if (!win->frmhan) error(etimacc); /* error */
            win->frmrun = TRUE; /* set timer running */

        }

    } else { /* disable framing timer */

        if (win->frmrun)  { /* it is currently running */

            r = timeKillEvent(win->frmhan); /* kill timer */
            if (r) error(etimacc); /* error */
            win->frmrun = FALSE; /* set timer ! running */

        }

    }

}

void frametimer(FILE* f, int e)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* index output file */
    iframetimer(win, txt2lfn(f), e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set automatic hold state

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from graph.
This exists to allow the results of graph unaware programs to be viewed after
termination, instead of exiting an distroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fufills the requirement of
holding graph unaware programs.

*******************************************************************************/

void autohold(int e)

{

    fautohold = e; /* set new state of autohold */

}

/*******************************************************************************

Return number of mice

Returns the number of mice implemented. Windows supports only one mouse.

*******************************************************************************/

int mouse(FILE* f)

{

    int rv; /* return value */

    rv = GetSystemMetrics(SM_MOUSEPRESENT); /* find mouse present */

    return (!!rv); /* set single mouse */

}

/*******************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

*******************************************************************************/

int mousebutton(FILE* f, int m)

{

    int bn; /* number of mouse buttons */

    if (m != 1) error(einvhan); /* bad mouse number */
    bn = GetSystemMetrics(SM_CMOUSEBUTTONS); /* find mouse buttons */

    if (bn > 3) bn = 3; /* limit mouse buttons to 3*/

    return (bn);

}

/*******************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

int joystick(FILE* f)

{

    winptr win; /* window pointer */
    int    jn;  /* joystick number */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    jn = win->numjoy; /* two */
    unlockmain(); /* end exclusive access */

    return (jn);

}

/*******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

int joybutton(FILE* f, int j)

{

    JOYCAPS  jc;  /* joystick characteristics data */
    winptr   win; /* window pointer */
    int      nb;  /* number of buttons */
    MMRESULT r;   /* return value */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    if (j < 1 || j > win->numjoy) error(einvjoy); /* bad joystick id */
    r = joyGetDevCaps(j-1, &jc, sizeof(JOYCAPS));
    if (r != JOYERR_NOERROR) error(ejoyqry); /* could not access joystick */
    nb = jc.wNumButtons; /* set number of buttons */
    /* We don't support more than 4 buttons. */
    if (nb > 4) nb = 4;
    unlockmain(); /* end exclusive access */

    return (nb);

}

/*******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y,  z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

int ijoyaxis(winptr win, int j)

{

    JOYCAPS  jc; /* joystick characteristics data */
    int      na; /* number of axes */
    MMRESULT r;

    if (j < 1 || j > win->numjoy) error(einvjoy); /* bad joystick id */
    r = joyGetDevCaps(j-1, &jc, sizeof(JOYCAPS));
    if (r != JOYERR_NOERROR) error(ejoyqry); /* could not access joystick */
    na = jc.wNumAxes; /* set number of axes */
    /* We don"t support more than 3 axes. */
    if (na > 3) na = 3;

    return (na);

}

int joyaxis(FILE* f, int j)

{

    winptr win; /* window pointer */
    int    na;

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    na = ijoyaxis(win, j); /* find joystick axes */
    unlockmain(); /* end exclusive access */

    return (na);

}

/*******************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

*******************************************************************************/

void isettabg(winptr win, int t)

{

    int i, x; /* tab index */
    scncon* sc; /* screen context */

    sc = win->screens[win->curupd];
    if (sc->autof && (t-1)%win->charspace)
        error(eatotab); /* cannot perform with auto on */
    if (t < 1 || t > sc->maxxg) error(einvtab); /* bad tab position */
    /* find free location or tab beyond position */
    i = 1;
    while (i < MAXTAB && sc->tab[i] && t > sc->tab[i]) i = i+1;
    if (i == MAXTAB && t < sc->tab[i]) error(etabful); /* tab table full */
    if (t != sc->tab[i])  { /* ! the same tab yet again */

        if (sc->tab[MAXTAB]) error(etabful); /* tab table full */
        /* move tabs above us up */
        for (x = MAXTAB; x > i; x--) sc->tab[x] = sc->tab[x-1];
        sc->tab[i] = t; /* place tab in order */

    }

}

void settabg(FILE* f, int t)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, t); /* translate to graphical call */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set tab

Sets a tab at the indicated collumn number.

*******************************************************************************/

void settab(FILE* f, int t)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, (t-1)*win->charspace+1); /* translate to graphical call */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

*******************************************************************************/

void irestabg(winptr win, int t)

{

    int     i;  /* tab index */
    int     ft; /* found tab */
    scncon* sc; /* screen context */

    sc = win->screens[win->curupd];
    if (t < 1 || t > sc->maxxg) error(einvtab); /* bad tab position */
    /* search for that tab */
    ft = 0; /* set ! found */
    for (i = 1; i <= MAXTAB; i++) if (sc->tab[i] == t) ft = i; /* found */
    if (ft != 0) { /* found the tab, remove it */

       /* move all tabs down to gap out */
       for (i = ft; i <= MAXTAB-1; i++) sc->tab[i] = sc->tab[i+1];
       sc->tab[MAXTAB] = 0; /* clear any last tab */

    }

}

void restabg(FILE* f, int t)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, t); /* translate to graphical call */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void restab(FILE* f, int t)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, (t-1)*win->charspace+1); /* translate to graphical call */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void clrtab(FILE* f)

{

    int    i;
    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    for (i = 1; i <= MAXTAB; i++) win->screens[win->curupd]->tab[i] = 0;
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

*******************************************************************************/

int funkey(FILE* f)

{

    return (12); /* number of function keys */

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editting. The line is placed into the
input line buffer.

The upgrade for this is to implement a full set of editting features.

Now edits multiple buffers in multiple windows. Each event is received from
ievent, dispatched to the buffer whose window it belongs to. When a
buffer is completed by hitting "enter",  we return.

*******************************************************************************/

void readline(int fn)

{

    pa_evtrec er;  /* event record */
    winptr    win; /* window pointer */

    do { /* get line characters */

        /* get events until an "interesting" event occurs */
        do { ievent(fn, &er);
        } while (er.etype != pa_etchar && er.etype != pa_etenter &&
                 er.etype != pa_etterm && er.etype != pa_etdelcb);
        win = lfn2win(xltwin[er.winid]); /* get the window from the id */
        /* if the event is line enter, place carriage return code,
           otherwise place real character. note that we emulate a
           terminal && return cr only, which is handled as appropriate
           by a higher level. if the event is program terminate,  we
           execute an organized halt */
        switch (er.etype) {/* event */

            case pa_etterm:  abort; /* halt program */
            case pa_etenter: /* line terminate */
                win->inpbuf[win->inpptr] = '\r'; /* return cr */
                plcchr(win, '\r'); /* output newline sequence */
                plcchr(win, '\n');
                win->inpend = TRUE; /* set line was terminated */
                break;

            case pa_etchar: /* character */
                if (win->inpptr < MAXLIN) {

                    /* place real character */
                    win->inpbuf[win->inpptr] = er.echar;
                    plcchr(win, er.echar); /* echo the character */

                }
                if (win->inpptr < MAXLIN)
                    win->inpptr = win->inpptr+1; /* next character */
                break;

            case pa_etdelcb: /* delete character backwards */
                if (win->inpptr > 1) { /* ! at extreme left */

                    /* backspace, spaceout  backspace again */
                    plcchr(win, '\b');
                    plcchr(win, ' ');
                    plcchr(win, '\b');
                    win->inpptr = win->inpptr-1; /* back up pointer */

                }
                break;

        }

    } while (er.etype != pa_etenter); /* until line terminate */
    /* note we still are indexing the last window that gave us the enter */
    win->inpptr = 1; /* set 1st position on active line */

}

/*******************************************************************************

Place string in storage

Places the given string into dynamic storage, and returns that.

*******************************************************************************/

char* str(char* s)

{

    char* p;

    p = malloc(strlen(s)+1);
    if (!p) error(enomem);
    strcpy(p, s);

    return (p);

}

/*******************************************************************************

Get program name

Retrieves the program name off the Windows command line. This routine is very
dependent on the structure of a windows command line. The name is enclosed
in quotes, has a path, and is terminated by ".".

A variation on this is a program that executes us directly may not have the
quotes, and we account for this. I have also accounted for there being no
extention, just in case these kinds of things turn up.

Also sets up the termination name, used to place a title on a window that is
held after execution ends for a program with autohold set.

Notes:

1. Win98 drops the quotes.
2. Win XP/2000 drops the path and the extention.

*******************************************************************************/

void getpgm(void)

{

    int   l;    /* length of program name string */
    char* cp;   /* holds command line pointer */
    char* s;
    char* s2;
    char  fini[] = "Finished - ";

    cp = GetCommandLine(); /* get command line */
    i = 1; /* set 1st character */
    if (*cp == '"') cp++; /* skip quote */
    /* find last "\" in quoted section */
    s = NULL; /* flag not found */
    while (*s && *s != '"' && *s != ' ')
        { if (*cp == '\\') s = cp+1; cp++; }
    if (!s) error(esystem);
    /* count program name length */
    l = 0; /* clear length */
    s2 = s; /* index program name */
    while (*s2 != '.' && *s2 != ' ') { s2++; l++; }
    pgmnam = malloc(l);
    if (!pgmnam) error(enomem);
    s2 = pgmnam; /* index 1st character */
    while (*s != '.' && *s != ' ') *s2++ = *s++; /* place characters */
    /* form the name for termination */
    trmnam = malloc(strlen(pgmnam)+strlen(fini)); /* get the holding string */
    if (!trmnam) error(enomem);
    strcpy(trmnam, fini); /* place first part */
    strcmp(trmnam, pgmnam); /* place program name */

}

/*******************************************************************************

Sort font list

Sorts the font list for alphabetical order, a-z. The font list does ! need
to be in a particular order (and indeed, can't be absolutely in order, because
of the first 4 reserved entries), but sorting it makes listings neater if a
program decides to dump the font names in order.

*******************************************************************************/

void sortfont(fontptr* fp)

{

    fontptr nl, p, c, l;

    nl = NULL; /* clear destination list */
    while (fp != NULL) { /* insertion sort */

        p = *fp; /* index top */
        *fp = (*fp)->next; /* remove that */
        p->next = NULL; /* clear next */
        c = nl; /* index new list */
        l = NULL; /* clear last */
        while (c) { /* find insertion point */

            /* compare strings */
            if (strcmp(p->fn, c->fn) > 0) c = NULL; /* terminate */
            else {

                l = c; /* set last */
                c = c->next; /* advance */

            }

        }
        if (!l) {

            /* if no last, insert to start */
            p->next = nl;
            nl = p;

        } else {

            /* insert to middle/end */
            p->next = l->next; /* set next */
            l->next = p; /* link to last */

        }

    }
    *fp = nl; /* place result */

}

/*******************************************************************************

Font list callback

This routine is called by windows when we register it from getfonts. It is
called once for each installed font. We ignore non-TRUEType fonts,  place
the found fonts on the global fonts list in no particular order.

Note that OpenType fonts are also flagged as TRUEType fonts.

We remove any "bold", "italic" or "oblique" descriptor word from the end of
the font string, as these are attributes, not part of the name.

*******************************************************************************/

/* count number of space delimited words in string */

static int words(char *s)

{

    int wc;
    int ichar;
    int ispace;

    wc = 0;
    ichar = 0;
    ispace = 0;
    while (*s) {

        if (*s == ' ') {

            if (!ispace) { ispace = 1; ichar = 0; }

        } else {

            if (!ichar) { ichar = 1; ispace = 0; wc++; }

        }
        s++;

    }

    return wc;

}

/* extract a series of space delimited words from string */

static void extwords(char *d, int dl, char *s, int st, int ed)
{

    int wc;
    int ichar;
    int ispace;

    wc = 0;
    ichar = 0;
    ispace = 0;
    if (dl < 1) error(estrtl);
    dl--; /* create room for terminator */
    while (*s) {

        if (*s == ' ') {

            if (ichar) wc++; /* count end of word */
            if (!ispace) { ispace = 1; ichar = 0; }

        } else {

            if (!ichar) { ichar = 1; ispace = 0; }
            if (wc >=st && wc <= ed) {

                if (!dl) error(estrtl);
                *d++ = *s;
                dl--;

            }

        }
        s++;

    }
    *d = 0;

}

/* replace attribute word */

void repatt(char* s, int l)

{

    int wc;
    int f;
    char ts[250];

    wc = words(s); /* find number of words in string */

    do { /* try removing descriptors from string end */

        f = FALSE;
        if (!wc) error(esystem);
        extwords(ts, 250, s, wc, wc); /* get last word in string */
        if (!strcmp(ts, "bold") || !strcmp(ts, "italic") ||
            !strcmp(ts, "oblique")) { wc--; f = TRUE; }

    } while (f); /* until no more descriptors found */

    /* extract remaining string */
    extwords(s, l, ts, 1, wc);

}

int CALLBACK enumfont(const LOGFONT* lfd, const TEXTMETRIC* pfd, DWORD ft, LPARAM ad)

{

    fontptr        fp;     /* pointer to font entry */
    int            c, i;   /* indexes */
    char           ts[250];
    ENUMLOGFONTEX* lfde;

    lfde = (ENUMLOGFONTEX*)lfd;
    if ((ft & TRUETYPE_FONTTYPE) &&
        (lfde->elfLogFont.lfCharSet == ANSI_CHARSET ||
         lfde->elfLogFont.lfCharSet == SYMBOL_CHARSET ||
         lfde->elfLogFont.lfCharSet == DEFAULT_CHARSET))  {

      /* ansi character set, record it */
      strcpy(ts, lfde->elfFullName); /* make a copy of the name */
      repatt(ts, 250); /* remove any attribute word */
      fp = malloc(sizeof(fontrec)); /* get a font entry */
      if (!fp) error(enomem);
      fp->next = fntlst; /* push to list */
      fntlst = fp;
      fntcnt = fntcnt+1; /* count font */
      fp->fn = str(ts); /* create string and copy */
      fp->fix = !!(lfde->elfLogFont.lfPitchAndFamily & 3 == FIXED_PITCH);
      fp->sys = FALSE; /* set not system */

   }

   return (TRUE); /* set continue */

}

/*******************************************************************************

Get fonts list

Loads the windows font list. The list is filtered for TRUEType/OpenType
only. We also retrive the fixed pitch status.

Because of the callback arrangement, we have to pass the font list and count
through globals, which are  placed into the proper window.

*******************************************************************************/

void getfonts(winptr win)

{

    int r;
    LOGFONT lf;

    fntlst = NULL; /* clear fonts list */
    fntcnt = 0; /* clear fonts count */
    lf.lfHeight = 0; /* use default height */
    lf.lfWidth = 0; /* use default width */
    lf.lfEscapement = 0; /* no escapement */
    lf.lfOrientation = 0; /* orient to x axis */
    lf.lfWeight = FW_DONTCARE; /* default weight */
    lf.lfItalic = 0;  /* no italic */
    lf.lfUnderline = 0; /* no underline */
    lf.lfStrikeOut = 0; /* no strikeout */
    lf.lfCharSet = DEFAULT_CHARSET; /* use default characters */
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS; /* use default precision */
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS; /* use default clipping */
    lf.lfQuality = DEFAULT_QUALITY; /* use default quality */
    lf.lfPitchAndFamily = 0; /* must be zero */
    lf.lfFaceName[0] = 0; /* match all typeface names */
    r = EnumFontFamiliesEx(win->devcon, &lf, enumfont, 0, 0);
    win->fntlst = fntlst; /* place into windows record */
    win->fntcnt = fntcnt;
    sortfont(&win->fntlst); /* sort into alphabetical order */

}

/*******************************************************************************

Remove font from font list

Removes the indicated font from the font list. Does not dispose of the entry.

*******************************************************************************/

void delfnt(winptr win, fontptr fp)

{

    fontptr p;

    p = win->fntlst; /* index top of list */
    if (!win->fntlst) error(esystem); /* should ! be null */
    if (win->fntlst == fp) win->fntlst = fp->next; /* gap first entry */
    else { /* mid entry */

        /* find entry before ours */
        while (p->next != fp && p->next != NULL) p = p->next;
        if (!p->next) error(esystem); /* not found */
        p->next = fp->next; /* gap out */

    }

}

/*******************************************************************************

Search for font by name

Finds a font in the list of fonts. Also matches fixed/no fixed pitch status.

*******************************************************************************/

void fndfnt(winptr win, char* fn, int fix, fontptr* fp)

{

    fontptr p;

    *fp = NULL; /* set no font found */
    p = win->fntlst; /* index top of font list */
    while (p) { /* traverse font list */

        if (!strcmp(p->fn, fn) && p->fix == fix) *fp = p; /* found, set */
        p = p->next; /* next entry */

    }

}

/*******************************************************************************

Separate standard fonts

Finds the standard fonts, in order, and either moves these to the top of the
table or creates a blank entry.

Note: could also default to style searching for book and sign fonts.

*******************************************************************************/

/* place font entry in list */

void plcfnt(winptr win, fontptr fp)

{

    if (!fp) { /* no entry, create a dummy */

        fp = malloc(sizeof(fontrec)); /* get a new entry */
        if (!fp) error(enomem);
        fp->fn = malloc(1); /* place empty string */
        if (!fp->fn) error(enomem);
        *fp->fn = 0;
        fp->fix = FALSE; /* set for cleanlyness */
        fp->sys = FALSE; /* set ! system */

    }
    fp->next = win->fntlst; /* push onto list */
    win->fntlst = fp;

}

void stdfont(winptr win)

{

    fontptr termfp, bookfp, signfp, techfp; /* standard font slots */

    /* clear font pointers */
    termfp = NULL;
    bookfp = NULL;
    signfp = NULL;
    techfp = NULL;
    /* set up terminal font. terminal font is set to system default */
    termfp = malloc(sizeof(fontrec)); /* get a new entry */
    if (!termfp) error(enomem);
    termfp->fix = TRUE; /* set fixed */
    termfp->sys = TRUE; /* set system */
    termfp->fn = str("System Fixed");
    win->fntcnt = win->fntcnt+1; /* add to font count */
    /* find book fonts */
    fndfnt(win, "Times New Roman", FALSE, &bookfp);
    if (!bookfp) {

        fndfnt(win, "Garamond", FALSE, &bookfp);
        if (!bookfp) {

            fndfnt(win, "Book Antiqua", FALSE, &bookfp);
            if (!bookfp) {

                fndfnt(win, "Georgia", FALSE, &bookfp);
                if (!bookfp) {

                    fndfnt(win, "Palatino Linotype", FALSE, &bookfp);
                    if (!bookfp) fndfnt(win, "Verdana", FALSE, &bookfp);

                }

            }

        }

    }
    /* find sign fonts */

    fndfnt(win, "Tahoma", FALSE, &signfp);
    if (!signfp) {

       fndfnt(win, "Microsoft Sans Serif", FALSE, &signfp);
       if (!signfp) {

          fndfnt(win, "Arial", FALSE, &signfp);
          if (!signfp) {

             fndfnt(win, "News Gothic MT", FALSE, &signfp);
             if (!signfp) {

                fndfnt(win, "Century Gothic", FALSE, &signfp);
                if (!signfp) {

                   fndfnt(win, "Franklin Gothic", FALSE, &signfp);
                   if (!signfp) {

                      fndfnt(win, "Trebuchet MS", FALSE, &signfp);
                      if (!signfp) fndfnt(win, "Verdana", FALSE, &signfp);

                   }

                }

             }

          }

       }

    }
    /* delete found fonts from the list */
    if (!bookfp) delfnt(win, bookfp);
    if (!signfp) delfnt(win, signfp);
    /* now place the fonts in the list backwards */
    plcfnt(win, techfp);
    plcfnt(win, signfp);
    plcfnt(win, bookfp);
    termfp->next = win->fntlst;
    win->fntlst = termfp;

}

/*******************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

void title(FILE* f, char* ts)

{

    BOOL   b;   /* result code */
    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    /* Set title is actually done via a "wm_settext" message to the display
       window, so we have to remove the lock to allow that to be processed.
       Otherwise, setwindowtext will wait for acknoledgement of the message
       and lock us. */
    unlockmain(); /* end exclusive access */
    /* set window title text */
    b = SetWindowText(win->winhan, ts);
    lockmain();/* start exclusive access */
    if (!b) winerr(); /* process windows error */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Register standard window class

Registers the "stdwin" class. All of graph's normal windows use the same
class, which has the name "stdwin". This class only needs to be registered
once, and is thereafter referenced by name.

*******************************************************************************/

void regstd(void)

{

    WNDCLASS wc; /* windows class structure */
    ATOM     b;

    /* set windows class to a normal window without scroll bars,
       with a windows void pointing to the message mirror.
       The message mirror reflects messages that should be handled
       by the program back into the queue, sending others on to
       the windows default handler */
//    wc.style      = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = wndproc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = GetModuleHandle(NULL);
    if (!wc.hInstance) winerr(); /* process windows error */
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    if (!wc.hIcon) winerr(); /* process windows error */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    if (!wc.hCursor) winerr(); /* process windows error */
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    if (!wc.hbrBackground) winerr(); /* process windows error */
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = str("stdwin");
    /* register that class */
    b = RegisterClass(&wc);
    if (!b) winerr(); /* process windows error */

}

/*******************************************************************************

Kill window

Sends a destroy window command to the window. We can"t directly kill a window
from the main thread, so we send a message to the window to kill it for us.

*******************************************************************************/

void kilwin(HWND wh)

{

    MSG  msg; /* intertask message */
    BOOL b;

    stdwinwin = wh; /* place window handle */
    /* order window to close */
    b = PostMessage(dispwin, UMCLSWIN, 0, 0);
    if (!b) winerr(); /* process windows error */
    /* Wait for window close. */
    do { igetmsg(&msg); } while (msg.message != UMWINCLS);

}

/*******************************************************************************

Open and present window

Given a windows record, opens && presents the window associated with it. All
of the screen buffer data is cleared, && a single buffer assigned to the
window.

*******************************************************************************/

void opnwin(int fn, int pfn)

{

    RECT cr;       /* client rectangle holder */
    int        r;         /* result holder */
    int        b;         /* int result holder */
    pa_evtrec  er;  /* event holding record */
    int        ti;        /* index for repeat array */
    int        pin;       /* index for loadable pictures array */
    int        si;        /* index for current display screen */
    TEXTMETRIC tm; /* TRUE type text metric structure */
    winptr     win;    /* window pointer */
    winptr     pwin;   /* parent window pointer */
    int        f;         /* window creation flags */
    MSG        msg;       /* intertask message */
    HGDIOBJ    rv;

    win = lfn2win(fn); /* get a pointer to the window */
    /* find parent */
    win->parlfn = pfn; /* set parent logical number */
    if (pfn) {

       pwin = lfn2win(pfn); /* index parent window */
       win->parhan = pwin->winhan; /* set parent window handle */

    } else win->parhan = 0; /* set no parent */
    win->mb1 = FALSE; /* set mouse as assumed no buttons down, at origin */
    win->mb2 = FALSE;
    win->mb3 = FALSE;
    win->mpx = 1;
    win->mpy = 1;
    win->mpxg = 1;
    win->mpyg = 1;
    win->nmb1 = FALSE;
    win->nmb2 = FALSE;
    win->nmb3 = FALSE;
    win->nmpx = 1;
    win->nmpy = 1;
    win->nmpxg = 1;
    win->nmpyg = 1;
    win->shift = FALSE; /* set no shift active */
    win->cntrl = FALSE; /* set no control active */
    win->fcurdwn = FALSE; /* set cursor is ! down */
    win->focus = FALSE; /* set ! in focus */
    win->joy1xs = 0; /* clear joystick saves */
    win->joy1ys = 0;
    win->joy1zs = 0;
    win->joy2xs = 0;
    win->joy2ys = 0;
    win->joy2zs = 0;
    win->numjoy = 0; /* set number of joysticks 0 */
    win->inpptr = 1; /* set 1st character */
    win->inpend = FALSE; /* set no line }ing */
    win->frmrun = FALSE; /* set framing timer ! running */
    win->bufmod = TRUE; /* set buffering on */
    win->menhan = 0; /* set no menu */
    win->metlst = NULL; /* clear menu tracking list */
    win->wiglst = NULL; /* clear widget list */
    win->frame = TRUE; /* set frame on */
    win->size = TRUE; /* set size bars on */
    win->sysbar = TRUE; /* set system bar on */
    win->sizests = 0; /* clear last size status word */
    /* clear timer repeat array */
    for (ti = 0; ti < 10; ti++) {

       win->timers[ti].han = 0; /* set no active timer */
       win->timers[ti].rep = FALSE; /* set no repeat */

    }
    /* clear loadable pictures table */
    for (pin = 0; pin < MAXPIC; pin++) win->pictbl[pin].han = 0;
    for (si = 0; si < MAXCON; si++) win->screens[si] = NULL;
    win->screens[0] = malloc(sizeof(scncon)); /* get the default screen */
    if (!win->screens[0]) error(enomem);
    win->curdsp = 1; /* set current display screen */
    win->curupd = 1; /* set current update screen */
    win->visible = FALSE; /* set ! visible */
    /* now perform windows setup */
    /* set flags for window create */
    f = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    /* add flags for child window */
    if (win->parhan) f |= WS_CHILD | WS_CLIPSIBLINGS;
    /* Create the window, using display task. */
    stdwinflg = f;
    stdwinx = 0x80000000;
    stdwiny = 0x80000000;
    stdwinw = 0x80000000;
    stdwinh = 0x80000000;
    stdwinpar = win->parhan;
    /* order window to start */
    b = PostMessage(dispwin, UMMAKWIN, 0, 0);
    if (!b) winerr(); /* process windows error */
    /* Wait for window start. */
    do { igetmsg(&msg); } while (msg.message != UMWINSTR);
    win->winhan = stdwinwin; /* get the new handle */
    if (!win->winhan) winerr(); /* process windows error */

    /* Joysticks were captured with the window open. Set status of joysticks.

      Do we need to release && recapture the joysticks each time we gain and
      loose focus ? Windows could have easily outgrown that need by copying
      the joystick messages. This needs testing. */

    win->numjoy = 0; /* clear joystick counter */
    win->joy1cap = stdwinj1c; /* set joystick 1 capture status */
    win->numjoy = win->numjoy+win->joy1cap; /* count that */
    win->joy2cap = stdwinj2c; /* set joystick 1 capture status */
    win->numjoy = win->numjoy+win->joy2cap; /* count that */

    /* create a device context for the window */
    win->devcon = GetDC(win->winhan); /* get device context */
    if (!win->devcon) winerr(); /* process windows error */
    /* set rescalable mode */
    r = SetMapMode(win->devcon, MM_ANISOTROPIC);
    if (!r) winerr(); /* process windows error */
    /* set non-braindamaged stretch mode */
    r = SetStretchBltMode(win->devcon, HALFTONE);
    if (!r) winerr(); /* process windows error */
    /* remove fills */
    rv = SelectObject(win->devcon, GetStockObject(NULL_BRUSH));
    if (rv == HGDI_ERROR) error(enosel);
    /* because this is an "open }ed" (no feedback) emulation, we must bring
      the terminal to a known state */
    win->gfhigh = FHEIGHT; /* set default font height */
    getfonts(win); /* get the global fonts list */
    stdfont(win); /* mark the standard fonts */
    win->gcfont = fntlst; /* index top of list as terminal font */
    /* set up system default parameters */
    rv = SelectObject(win->devcon, GetStockObject(SYSTEM_FIXED_FONT));
    if (rv == HGDI_ERROR) error(enosel);
    b = GetTextMetrics(win->devcon, &tm); /* get the standard metrics */
    if (!b) winerr(); /* process windows error */
    /* calculate line spacing */
    win->linespace = tm.tmHeight;
    /* calculate character spacing */
    win->charspace = tm.tmMaxCharWidth;
    /* set cursor width */
    win->curspace = tm.tmAveCharWidth;
    /* find screen device parameters for dpm calculations */
    win->shsize = GetDeviceCaps(win->devcon, HORZSIZE); /* size x in millimeters */
    win->svsize = GetDeviceCaps(win->devcon, VERTSIZE); /* size y in millimeters */
    win->shres = GetDeviceCaps(win->devcon, HORZRES); /* pixels in x */
    win->svres = GetDeviceCaps(win->devcon, VERTRES); /* pixels in y */
    win->sdpmx = win->shres/win->shsize*1000; /* find dots per meter x */
    win->sdpmy = win->svres/win->svsize*1000; /* find dots per meter y */
    /* find client area size */
    win->gmaxxg = MAXXD*win->charspace;
    win->gmaxyg = MAXYD*win->linespace;
    cr.left = 0; /* set up desired client rectangle */
    cr.top = 0;
    cr.right = win->gmaxxg;
    cr.bottom = win->gmaxyg;
    /* find window size from client size */
    b = AdjustWindowRectEx(&cr, WS_OVERLAPPEDWINDOW, FALSE, 0);
    if (!b) winerr(); /* process windows error */
    /* now, resize the window to just fit our character mode */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0, cr.right-cr.left, cr.bottom-cr.top,
                         SWP_NOMOVE | SWP_NOZORDER);
    if (!b) winerr(); /* process windows error */
/* now handled in winvis */
#if 0
    /* present the window */
    b = ShowWindow(win->winhan, SW_SHOWDEFAULT);
    /* send first paint message */
    b = UpdateWindow(win->winhan);
#endif
    lockmain(); /* start exclusive access */
    /* set up global buffer parameters */
    win->gmaxx = MAXXD; /* character max dimensions */
    win->gmaxy = MAXYD;
    win->gattr = 0; /* no attribute */
    win->gauto = TRUE; /* auto on */
    win->gfcrgb = colnum(pa_black); /*foreground black */
    win->gbcrgb = colnum(pa_white); /* background white */
    win->gcurv = TRUE; /* cursor visible */
    win->gfmod = mdnorm; /* set mix modes */
    win->gbmod = mdnorm;
    win->goffx = 0;  /* set 0 offset */
    win->goffy = 0;
    win->gwextx = 1; /* set 1:1 extents */
    win->gwexty = 1;
    win->gvextx = 1;
    win->gvexty = 1;
    iniscn(win, win->screens[0]); /* initalize screen buffer */
    restore(win, TRUE); /* update to screen */
/* This next is taking needed messages out of the queue, and I don"t believe it
  is needed anywmore with display tasking. */
#if 0
    /* have seen problems with actions being performed before events are pulled
      from the queue, like the focus event. the answer to this is to wait a short
      delay until these messages clear. in fact, all that is really required is
      to reenter the OS so it can do the callback */
    frmhan = TimeSetEvent(10, 0, timeout, fn*MAXTIM+1,
                          TIME_CALLBACK_INT or
                          TIME_KILL_SYNCHRONOUS or
                          TIME_ONESHOT);
    if (!frmhan) error(etimacc); /* no timer available */
    do { ievent(opnfil[fn]->inl, er);
    } while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) abort();
#endif

}

/*******************************************************************************

Close window

Shuts down, removes and releases a window.

*******************************************************************************/

void clswin(int fn)

{

    int    r;   /* result holder */
    int    b;   /* int result holder */
    winptr win; /* window pointer */

    win = lfn2win(fn); /* get a pointer to the window */
    b = ReleaseDC(win->winhan, win->devcon); /* release device context */
    if (!b) winerr(); /* process error */
    /* release the joysticks */
    if (win->joy1cap) {

        r = joyReleaseCapture(JOYSTICKID1);
        if (r) error(ejoyacc); /* error */

    }
    if (win->joy2cap) {

        r = joyReleaseCapture(JOYSTICKID2);
        if (r) error(ejoyacc); /* error */

    }
    kilwin(win->winhan); /* kill window */

}

/*******************************************************************************

Close window

Closes an open window pair. Accepts an output window. The window is closed, and
the window and file handles are freed. The input file is freed only if no other
window also links it.

*******************************************************************************/

/* flush and close file */

void clsfil(int fn)

{

    eqeptr ep; /* pointer for event containers */
    int    si; /* index for screens */
    filptr fp;

    fp = opnfil[fn];
    /* release all of the screen buffers */
    for (si = 1; si <= MAXCON; si++)
        if (fp->win->screens[si]) free(fp->win->screens[si]);
    free(fp->win); /* release the window data */
    fp->win = NULL; /* set end open */
    fp->inw = FALSE;
    fp->inl = 0;
    while (fp->evt) {

        ep = fp->evt; /* index top */
        if (fp->evt->next == fp->evt)
            fp->evt = NULL; /* last entry, clear list */
        else fp->evt = fp->evt->next; /* gap out entry */
        free(ep); /* release */

    }

}

int inplnk(int fn)

{

    int fi; /* index for files */
    int fc; /* counter for files */

    fc = 0; /* clear count */
    for (fi = 1; fi <= MAXFIL; fi++) /* traverse files */
        if (opnfil[fi]) /* entry is occupied */
            if (opnfil[fi]->inl == fn) fc++; /* count the file link */

    return (fc); /* return result */

}

void closewin(int ofn)

{

    int ifn; /* input file id */
    int wid; /* window id */

    wid = filwin[ofn]; /* get window id */
    ifn = opnfil[ofn]->inl; /* get the input file link */
    clswin(ofn); /* close the window */
    clsfil(ofn); /* flush && close output file */
    /* if no remaining links exist, flush && close input file */
    if (!inplnk(ifn))  clsfil(ifn);
    filwin[ofn] = 0; /* clear file to window translation */
    xltwin[wid] = 0; /* clear window to file translation */

}

/*******************************************************************************

Open an input and output pair

Creates, opens and initalizes an input and output pair of files.

*******************************************************************************/

void openio(int ifn, int ofn, int pfn, int wid)

{

    /* if output was never opened, create it now */
    if (!opnfil[ofn]) getfet(&opnfil[ofn]);
    /* if input was never opened, create it now */
    if (!opnfil[ifn]) getfet(&opnfil[ifn]);
    opnfil[ofn]->inl = ifn; /* link output to input */
    opnfil[ifn]->inw = TRUE; /* set input is window handler */
    /* now see if it has a window attached */
    if (!opnfil[ofn]->win) {

        /* Haven't already started the main input/output window, so allocate
           and start that. We tolerate multiple opens to the output file. */
        opnfil[ofn]->win = malloc(sizeof(winrec));
        if (!opnfil[ofn]->win) error(enomem);
        opnwin(ofn, pfn); /* and start that up */

    }
    /* check if the window has been pinned to something else */
    if (xltwin[wid] && xltwin[wid] != ofn) error(ewinuse); /* flag error */
    xltwin[wid] = ofn; /* pin the window to the output file */
    filwin[ofn] = wid;

}

/*******************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.
The window id can be from 0 to MAXFIL, but the input, output and error file ids
of 0, 1 and 2 are reserved for the input, output and error files, and cannot be
used directly.

Opening a window file as either input or output from/to a window creates a
special file id that bypasses the usual file I/O (much as sockets do). However,
for several reasons we open the file anyways as "nul", which in Mingw/windows
creates a "do nothing" file. This enables the file to be opened (to nothing),
and also acts as a placeholder for the fid.

The pairing of multiple output files to a single input file is done by detecting
that the same input file is used. Since no previous state is implied by a stdio
FILE structure, we do that by matching the address of the structure.

*******************************************************************************/

/* check file is already in use */
int fndfil(FILE* fp)

{

    int fi; /* file index */
    int ff; /* found file */

    ff = -1; /* set no file found */
    for (fi = 0; fi < MAXFIL; fi++)
        if (opnfil[fi] && opnfil[fi]->sfp == fp) ff = fi; /* set found */

    return (ff);

}

void iopenwin(FILE** infile, FILE** outfile, int pfn, int wid)

{

    int ifn, ofn; /* file logical handles */

    /* check valid window handle */
    if (wid < 1 || wid > MAXFIL) error(einvwin);
    /* check if the window id is already in use */
    if (xltwin[wid]) error(ewinuse); /* error */
    ifn = fndfil(*infile); /* find previous open input side */
    if (ifn < 0) { /* no other input file, open new */

        /* open input file */
        unlockmain(); /* end exclusive access */
        *infile = fopen("nul", "r"); /* open null as read only */
        lockmain(); /* start exclusive access */
        if (!*infile) error(enoopn); /* can't open */

    }
    /* open output file */
    unlockmain(); /* end exclusive access */
    *outfile = fopen("nul", "w");
    ofn = fileno(*outfile); /* get logical file no. */
    if (ofn == -1) error(esystem);
    if (!*outfile) error(enoopn); /* can't open */

    /* check either input is unused, or is already an input side of a window */
    if (opnfil[ifn]) /* entry exists */
        if (!opnfil[ifn]->inw || opnfil[ifn]->win) error(einmode); /* wrong mode */
   /* check output file is in use for input or output from window */
   if (opnfil[ofn]) /* entry exists */
        if (opnfil[ofn]->inw || opnfil[ofn]->win)
            error(efinuse); /* file in use */
   /* establish all logical files and links, translation tables, and open window */
   openio(ifn, ofn, pfn, wid);

}

void openwin(FILE* infile, FILE* outfile, FILE* parent, int wid)

{

    winptr win; /* window context pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(parent); /* validate parent is a window file */
    iopenwin(&infile, &outfile, txt2lfn(parent), wid); /* process open */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Size buffer pixel

Sets or resets the size of the buffer surface, in pixel units.

*******************************************************************************/

void isizbufg(winptr win, int x, int y)

{

    RECT cr; /* client rectangle holder */
    int si;  /* index for current display screen */

    if (x < 1 || y < 1)  error(einvsiz); /* invalid buffer size */
    /* set buffer size */
    win->gmaxx = x/win->charspace; /* find character size x */
    win->gmaxy = y/win->linespace; /* find character size y */
    win->gmaxxg = x; /* set size in pixels x */
    win->gmaxyg = y; /* set size in pixels y */
    cr.left = 0; /* set up desired client rectangle */
    cr.top = 0;
    cr.right = win->gmaxxg;
    cr.bottom = win->gmaxyg;
    /* find window size from client size */
    b = AdjustWindowRectEx(&cr, WS_OVERLAPPEDWINDOW, FALSE, 0);
    if (!b) winerr(); /* process windows error */
    /* now, resize the window to just fit our new buffer size */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0, cr.right-cr.left, cr.bottom-cr.top,
                     SWP_NOMOVE | SWP_NOZORDER);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    /* all the screen buffers are wrong, so tear them out */
    for (si = 0; si < MAXCON; si++) disscn(win, win->screens[si]);
    win->screens[win->curdsp] = malloc(sizeof(scncon));
    if (!win->screens[win->curdsp]) error(enomem);
    iniscn(win, win->screens[win->curdsp]); /* initalize screen buffer */
    restore(win, TRUE); /* update to screen */
    if (win->curdsp != win->curupd) { /* also create the update buffer */

        win->screens[win->curupd] = malloc(sizeof(scncon)); /* get the display screen */
        if (!win->screens[win->curupd]) error(enomem);
        iniscn(win, win->screens[win->curupd]); /* initalize screen buffer */

    }

}

void sizbufg(FILE* f, int x, int y)

{

    winptr win; /* window pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window pointer from text file */
    isizbufg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

void sizbuf(FILE* f, int x, int y)

{

    winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    /* just translate from characters to pixels and do the resize in pixels. */
    isizbufg(win, x*win->charspace, y*win->linespace);
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

void ibuffer(winptr win, int e)

{

    int  si; /* index for current display screen */
    BOOL b;  /* result */
    RECT r;  /* rectangle */

    if (e) { /* perform buffer on actions */

        win->bufmod = TRUE; /* turn buffer mode on */
        /* restore size from current buffer */
        win->gmaxxg = win->screens[win->curdsp]->maxxg; /* pixel size */
        win->gmaxyg = win->screens[win->curdsp]->maxyg;
        win->gmaxx = win->screens[win->curdsp]->maxx; /* character size */
        win->gmaxy = win->screens[win->curdsp]->maxy;
        r.left = 0; /* set up desired client rectangle */
        r.top = 0;
        r.right = win->gmaxxg;
        r.bottom = win->gmaxyg;
        /* find window size from client size */
        b = AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
        if (!b) winerr(); /* process windows error */
        /* resize the window to just fit our buffer size */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(win->winhan, 0, 0, 0, r.right-r.left, r.bottom-r.top,
                         SWP_NOMOVE | SWP_NOZORDER);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */
        restore(win, TRUE); /* restore buffer to screen */

    } else if (win->bufmod) { /* perform buffer off actions */

        /* The screen buffer contains a lot of drawing information, so we have
           to keep one of them. We keep the current display, and force the
           update to point to it as well. This single buffer  serves as
           a "template" for the real pixels on screen. */
        win->bufmod = FALSE; /* turn buffer mode off */
        for (si = 0; si <MAXCON; si++)
           if (si != win->curdsp) disscn(win, win->screens[si]);
        /* dispose of screen data structures */
        for (si = 0; si < MAXCON; si++) if (si != win->curdsp-1)
           if (win->screens[si]) free(win->screens[si]);
        win->curupd = win->curdsp; /* unify the screens */
        /* get actual size of onscreen window, && set that as client space */
        b = GetClientRect(win->winhan, &r);
        if (!b) winerr(); /* process windows error */
        win->gmaxxg = r.right-r.left; /* return size */
        win->gmaxyg = r.bottom-r.top;
        win->gmaxx = win->gmaxxg / win->charspace; /* find character size x */
        win->gmaxy = win->gmaxyg / win->linespace; /* find character size y */
        /* tell the window to resize */
        b = PostMessage(win->winhan, WM_SIZE, SIZE_RESTORED,
                            win->gmaxyg*65536+win->gmaxxg);
        if (!b) winerr(); /* process windows error */
        /* tell the window to repaint */
        /*b = PostMessage(win->winhan, wm_paint, 0, 0);*/
        putmsg(win->winhan, WM_PAINT, 0, 0);
        if (!b) winerr(); /* process windows error */

    }

}

void buffer(FILE* f, int e)

{

     winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    ibuffer(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is NULL,  the active menu is
deleted.

*******************************************************************************/

/* create menu tracking entry */
void mettrk(winptr win, HMENU han, int inx, pa_menuptr m)

{

    metptr mp; /* menu tracking entry pointer */

    mp = malloc(sizeof(metrec)); /* get a new tracking entry */
    if (!mp) error(enomem);
    mp->next = win->metlst; /* push onto tracking list */
    win->metlst = mp;
    mp->han = han; /* place menu handle */
    mp->inx = inx; /* place menu index */
    mp->onoff = m->onoff; /* place on/off highlighter */
    mp->select = FALSE; /* place status of select (off) */
    mp->id = m->id; /* place id */
    mp->oneof = NULL; /* set no "one of" */
    /* We are walking backwards in the list, && we need the next list entry
      to know the "one of" chain. So we tie the entry to itself as a flag
      that it chains to the next entry. That chain will get fixed on the
      next entry. */
    if (m->oneof) mp->oneof = mp;
    /* now tie the last entry to this if indicated */
    if (mp->next) /* there is a next entry */
        if (mp->next->oneof == mp->next) mp->next->oneof = mp;

}

/* create menu list */
void createmenu(winptr win, pa_menuptr m, HMENU* mh)

{

    HMENU sm;  /* submenu handle */
    int   f;   /* menu flags */
    int   inx; /* index number for this menu */

    *mh = CreateMenu(); /* create new menu */
    if (!*mh) winerr(); /* process windows error */
    inx = 0; /* set first in sequence */
    while (m) { /* add menu item */

        f = MF_STRING | MF_ENABLED; /* set string and enable */
        if (m->branch) { /* handle submenu */

            createmenu(win, m->branch, &sm); /* create submenu */
            b = AppendMenu(*mh, f | MF_POPUP, (UINT_PTR)sm, m->face);
            if (!b) winerr(); /* process windows error */
            mettrk(win, *mh, inx, m); /* enter that into tracking */

        } else { /* handle terminal menu */

            b = AppendMenu(*mh, f, (UINT_PTR)m->id, m->face);
            if (!b) winerr(); /* process windows error */
            mettrk(win, *mh, inx, m); /* enter that into tracking */

        }
        if (m->bar) { /* add separator bar */

            /* a separator bar is a blank entry that will never be referenced */
            b = AppendMenu(*mh, MF_SEPARATOR, 0, "");
            if (!b) winerr(); /* process windows error */
            inx = inx+1; /* next in sequence */

        }
        m = m->next; /* next menu entry */
        inx++; /* next in sequence */

    }

}

void imenu(winptr win, pa_menuptr m)

{

    BOOL   b;  /* int result */
    metptr mp; /* tracking entry pointer */

    if (win->menhan) { /* distroy previous menu */

        b = DestroyMenu(win->menhan); /* destroy it */
        if (!b) winerr(); /* process windows error */
        /* dispose of menu tracking entries */
        while (win->metlst) {

            mp = win->metlst; /* remove top entry */
            win->metlst = win->metlst->next; /* gap out */
            free(mp); /* free the entry */

        }
        win->menhan = 0; /* set no menu active */

    }
    if (m) /* there is a new menu to activate */
        createmenu(win, m, &win->menhan);
    unlockmain(); /* end exclusive access */
    b = SetMenu(win->winhan, win->menhan); /* set the menu to the window */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    unlockmain(); /* end exclusive access */
    b = DrawMenuBar(win->winhan); /* display menu */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void menu(FILE* f, pa_menuptr m)

{

    winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    imenu(win, m); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find menu entry

Finds a menu entry by id. If the entry is ! found, it generates an error.
If the entry exists more than once, it generates an error.

*******************************************************************************/

metptr fndmenu(winptr win, int id)

{

    metptr mp; /* tracking entry pointer */
    metptr fp; /* found entry pointer */

    fp = NULL; /* set no entry found */
    mp = win->metlst; /* index top of list */
    while (mp) { /* traverse */

        if (mp->id == id) { /* found */

            if (fp) error(edupmen); /* menu entry is duplicated */
            fp = mp; /* set found entry */

        }
        mp = mp->next; /* next entry */

    }
    if (!fp) error(emennf); /* no menu entry found with id */

    return (fp); /* return entry */

}

/*******************************************************************************

Enable/disable menu entry

Enables || disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

*******************************************************************************/

void imenuena(winptr win, int id, int onoff)

{

    int    fl; /* flags */
    metptr mp; /* tracking pointer */
    BOOL   b;  /* result */

    mp = fndmenu(win, id); /* find the menu entry */
    fl = MF_BYPOSITION; /* place position select flag */
    if (onoff) fl |= MF_ENABLED; /* enable it */
    else fl |= MF_GRAYED; /* disable it */
    b = EnableMenuItem(mp->han, mp->inx, fl); /* perform that */
    if (b == -1) error(esystem); /* should not happen */
    unlockmain(); /* end exclusive access */
    b = DrawMenuBar(win->winhan); /* display menu */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void menuena(FILE* f, int id, int onoff)

{

    winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    imenuena(win, id, onoff); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

*******************************************************************************/

/* find top of "one of" list */
metptr fndtop(metptr mp)

{

    if (mp->next) /* there is a next */
        if (mp->next->oneof == mp) { /* next entry links this one */

        mp = mp->next; /* go to next entry */
        mp = fndtop(mp); /* try again */

   }

   return (mp); /* return result */

}

/* clear "one of" select list */
void clrlst(int* fl, metptr mp)

{

    DWORD r;

    do { /* items in list */

        *fl = MF_BYPOSITION | MF_UNCHECKED; /* place position select flag */
        r = CheckMenuItem(mp->han, mp->inx, *fl); /* perform that */
        if (r == -1) error(esystem); /* should not happen */
        mp = mp->oneof; /* link next */

    } while (mp); /* no more */

}

void imenusel(winptr win, int id, int select)

{

    int    fl; /* flags */
    metptr mp; /* tracking pointer */
    BOOL   b;  /* result */
    DWORD  r;  /* result */

    mp = fndmenu(win, id); /* find the menu entry */
    clrlst(&fl, fndtop(mp)); /* clear "one of" group */
    mp->select = select; /* set the new select */
    fl = MF_BYPOSITION; /* place position select flag */
    if (mp->select) fl |= MF_CHECKED; /* select it */
    else fl |= MF_UNCHECKED; /* deselect it */
    r = CheckMenuItem(mp->han, mp->inx, fl); /* perform that */
    if (r == -1) error(esystem); /* should ! happen */
    unlockmain(); /* end exclusive access */
    b = DrawMenuBar(win->winhan); /* display menu */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void menusel(FILE* f, int id, int select)

{

    winptr win; /* pointer to windows context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window context */
    imenusel(win, id, select); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

void ifront(winptr win)

{

    BOOL b; /* result holder */
    int fl;

    fl = 0;
    fl = ! fl;
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0/*fl*/ /*hwnd_topmost*/, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

#if 0
    fl = 1;
    fl = ! fl;
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, fl /*hwnd_notopmost*/, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
#endif

    unlockmain(); /* end exclusive access */
    b = PostMessage(win->winhan, WM_PAINT, 0, 0);
    if (!b) winerr(); /* process windows error */
    lockmain(); /* start exclusive access */

    if (win->parhan) {

        unlockmain(); /* end exclusive access */
        b = PostMessage(win->parhan, WM_PAINT, 0, 0);
        if (!b) winerr(); /* process windows error */
        lockmain(); /* start exclusive access */

    }

}

void front(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    ifront(win); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

void iback(winptr win)

{

    BOOL b; /* result holder */

    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void back(FILE* f)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iback(win); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get window size graphical

Gets the onscreen window size.

*******************************************************************************/

void igetsizg(winptr win, int* x, int* y)

{

    BOOL  b; /* result holder */
    RECT  r; /* rectangle */

    b = GetWindowRect(win->winhan, &r);
    if (!b) winerr(); /* process windows error */
    *x = r.right-r.left; /* return size */
    *y = r.bottom-r.top;

}

void getsizg(FILE* f, int* x, int* y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    igetsizg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get window size character

Gets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are returned. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void getsiz(FILE* f, int* x, int* y)

{

    winptr win, par; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    igetsizg(win, x, y); /* execute */
    if (win->parlfn) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        *x = (*x-1) / par->charspace+1;
        *y = (*y-1) / par->linespace+1;

    } else {

        /* find character based sizes */
        *x = (*x-1) / STDCHRX+1;
        *y = (*y-1) / STDCHRY+1;

    }
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set window size graphical

Sets the onscreen window to the given size.

*******************************************************************************/

void isetsizg(winptr win, int x, int y)

{

    BOOL b; /* result holder */

    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0, x, y, SWP_NOMOVE | SWP_NOZORDER);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void setsizg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isetsizg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set window size character

Sets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, && our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void setsiz(FILE* f, int x, int y)

{

    winptr win, par; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    if (win->parlfn) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        x = x*par->charspace;
        y = y*par->linespace;

    } else {

        /* find character based sizes */
        x = x*STDCHRX;
        y = y*STDCHRY;

    }
    isetsizg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set window position graphical

Sets the onscreen window to the given position in its parent.

*******************************************************************************/

void isetposg(winptr win, int x, int y)

{

    BOOL b; /* result holder */

    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, x-1, y-1, 0, 0, SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void setposg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isetposg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set window position character

Sets the onscreen window position, in character terms. If the window has a
parent, the demensions are converted to the current character size there.
Otherwise, pixel based demensions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, && our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void setpos(FILE* f, int x, int y)

{

    winptr win, par; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    if (win->parlfn) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        x = (x-1)*par->charspace+1;
        y = (y-1)*par->linespace+1;

    } else {

        /* find character based sizes */
        x = (x-1)*STDCHRX+1;
        y = (y-1)*STDCHRY+1;

    }
    isetposg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get screen size graphical

Gets the total screen size.

*******************************************************************************/

void iscnsizg(winptr win, int* x, int* y)

{

    BOOL b;      /* result holder */
    RECT r;      /* rectangle */
    HWND scnhan; /* desktop handle */

    scnhan = GetDesktopWindow();
    b = GetWindowRect(scnhan, &r);
    if (!b) winerr(); /* process windows error */
    *x = r.right-r.left; /* return size */
    *y = r.bottom-r.top;

}

void scnsizg(FILE* f, int* x, int* y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iscnsizg(win, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find window size from client

Finds the window size, in parent terms, needed to result in a given client
window size.

Note: this routine should be able to find the minimum size of a window using
the given style, and return the minimums if the input size is lower than this.
This does not seem to be obvious under Windows.

Do we also need a menu style type ?

*******************************************************************************/

void iwinclientg(winptr win, int cx, int cy, int* wx, int* wy, pa_winmodset ms)

{

    RECT cr; /* client rectangle holder */
    int fl;  /* flag */

    lockmain(); /* start exclusive access */
    cr.left = 0; /* set up desired client rectangle */
    cr.top = 0;
    cr.right = cx;
    cr.bottom = cy;
    /* set minimum style */
    fl = WS_OVERLAPPED | WS_CLIPCHILDREN;
    /* add flags for child window */
    if (win->parhan) fl |= WS_CHILD | WS_CLIPSIBLINGS;
    if (BIT(pa_wmframe) & ms) { /* add frame features */

        if (BIT(pa_wmsize) & ms) fl |= WS_THICKFRAME;
        if (BIT(pa_wmsysbar) & ms) fl |= WS_CAPTION | WS_SYSMENU |
                                         WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

    }
    /* find window size from client size */
    b = AdjustWindowRectEx(&cr, fl, FALSE, 0);
    if (!b) winerr(); /* process windows error */
    *wx = cr.right-cr.left; /* return window size */
    *wy = cr.bottom-cr.top;
    unlockmain(); /* end exclusive access */

}

void winclient(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms)

{

    winptr win, par; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    /* execute */
    iwinclientg(win, cx*win->charspace, cy*win->linespace, wx, wy, ms);
    /* find character based sizes */
    if (win->parlfn) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        *wx = (*wx-1) / par->charspace+1;
        *wy = (*wy-1) / par->linespace+1;

    } else {

        /* find character based sizes */
        *wx = (*wx-1) / STDCHRX+1;
        *wy = (*wy-1) / STDCHRY+1;

    }
    unlockmain(); /* end exclusive access */

}

void winclientg(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iwinclientg(win, cx, cy, wx, wy, ms); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

*******************************************************************************/

void scnsiz(FILE* f, int* x, int* y)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iscnsizg(win, x, y); /* execute */
    *x = *x/STDCHRX; /* convert to "standard character" size */
    *y = *y/STDCHRY;
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

void iframe(winptr win, int e)

{

    int  fl1; /* flag */
    RECT cr;  /* client rectangle holder */

    win->frame = e; /* set new status of frame */
    fl1 = WS_OVERLAPPED | WS_CLIPCHILDREN; /* set minimum style */
    /* add flags for child window */
    if (win->parhan) fl1 |= WS_CHILD | WS_CLIPSIBLINGS;
    /* if we are enabling frames, add the frame parts back */
    if (e) {

        if (win->size) fl1 |= WS_THICKFRAME;
        if (win->sysbar) fl1 |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                               WS_MAXIMIZEBOX;

    }
    unlockmain(); /* end exclusive access */
    r = SetWindowLong(win->winhan, GWL_STYLE, fl1);
    lockmain(); /* start exclusive access */
    if (!r) winerr(); /* process windows error */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(win->winhan, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
                                                 SWP_FRAMECHANGED);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    /* present the window */
    unlockmain(); /* end exclusive access */
    b = ShowWindow(win->winhan, SW_SHOWDEFAULT);
    lockmain(); /* start exclusive access */
    if (win->bufmod)  { /* in buffer mode */

        /* change window size to match new mode */
        cr.left = 0; /* set up desired client rectangle */
        cr.top = 0;
        cr.right = win->gmaxxg;
        cr.bottom = win->gmaxyg;
        /* find window size from client size */
        b = AdjustWindowRectEx(&cr, fl1, FALSE, 0);
        if (!b) winerr(); /* process windows error */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(win->winhan, 0, 0, 0,
                            cr.right-cr.left, cr.bottom-cr.top,
                            SWP_NOMOVE | SWP_NOZORDER);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void frame(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    iframe(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

*******************************************************************************/

void isizable(winptr win, int e)

{

    int  fl1; /* flag */
    RECT cr;  /* client rectangle holder */
    BOOL b;
    LONG r;

    win->size = e; /* set new status of size bars */
    /* no point in making the change of framing is off entirely */
    if (win->frame) {

        /* set minimum style */
        fl1 = WS_OVERLAPPED | WS_CLIPCHILDREN;
        /* add frame features */
        if (win->size) fl1 |= WS_THICKFRAME;
        else fl1 |= WS_BORDER;
        if (win->sysbar)
            fl1 |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        /* add flags for child window */
        if (win->parhan) fl1 |= WS_CHILD | WS_CLIPSIBLINGS;
        /* if we are enabling frames, add the frame parts back */
        if (e) fl1 |= WS_THICKFRAME;
        unlockmain(); /* end exclusive access */
        r = SetWindowLong(win->winhan, GWL_STYLE, fl1);
        lockmain(); /* start exclusive access */
        if (!r) winerr(); /* process windows error */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(win->winhan, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
                                                     SWP_FRAMECHANGED);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */
        /* present the window */
        unlockmain(); /* end exclusive access */
        b = ShowWindow(win->winhan, SW_SHOWDEFAULT);
        lockmain(); /* start exclusive access */
        if (win->bufmod) { /* in buffer mode */

            /* change window size to match new mode */
            cr.left = 0; /* set up desired client rectangle */
            cr.top = 0;
            cr.right = win->gmaxxg;
            cr.bottom = win->gmaxyg;
            /* find window size from client size */
            b = AdjustWindowRectEx(&cr, fl1, FALSE, 0);
            if (!b) winerr(); /* process windows error */
            unlockmain(); /* end exclusive access */
            b = SetWindowPos(win->winhan, 0, 0, 0,
                             cr.right-cr.left, cr.bottom-cr.top,
                             SWP_NOMOVE || SWP_NOZORDER);
            lockmain(); /* start exclusive access */
            if (!b) winerr(); /* process windows error */

        }

    }

}

void sizable(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isizable(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

*******************************************************************************/

void isysbar(winptr win, int e)

{

    int  fl1; /* flag */
    RECT cr;  /* client rectangle holder */
    LONG r;
    BOOL b;

    win->sysbar = e; /* set new status of size bars */
    /* no point in making the change of framing is off entirely */
    if (win->frame) {

        /* set minimum style */
        fl1 = WS_OVERLAPPED | WS_CLIPCHILDREN;
        /* add frame features */
        if (win->size) fl1 |= WS_THICKFRAME;
        else fl1 |= WS_BORDER;
        if (win->sysbar)
            fl1 |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        /* add flags for child window */
        if (win->parhan) fl1 |= WS_CHILD | WS_CLIPSIBLINGS;
        /* if we are enabling frames, add the frame parts back */
        if (e) fl1 |= WS_THICKFRAME;
        unlockmain(); /* end exclusive access */
        r = SetWindowLong(win->winhan, GWL_STYLE, fl1);
        lockmain(); /* start exclusive access */
        if (!r) winerr(); /* process windows error */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(win->winhan, 0, 0, 0, 0, 0,
                             SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */
        /* present the window */
        unlockmain(); /* end exclusive access */
        ShowWindow(win->winhan, SW_SHOWDEFAULT);
        lockmain(); /* start exclusive access */
        if (win->bufmod) { /* in buffer mode */

            /* change window size to match new mode */
            cr.left = 0; /* set up desired client rectangle */
            cr.top = 0;
            cr.right = win->gmaxxg;
            cr.bottom = win->gmaxyg;
            /* find window size from client size */
            b = AdjustWindowRectEx(&cr, fl1, FALSE, 0);
            if (!b) winerr(); /* process windows error */
            unlockmain(); /* end exclusive access */
            b = SetWindowPos(win->winhan, 0, 0, 0,
                                 cr.right-cr.left, cr.bottom-cr.top,
                                 SWP_NOMOVE | SWP_NOZORDER);
            lockmain(); /* start exclusive access */
            if (!b) winerr(); /* process windows error */

        }

    }

}

void sysbar(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get window from file */
    isysbar(win, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Append menu entry

Appends a new menu entry to the given list.

*******************************************************************************/

void appendmenu(pa_menuptr* list, pa_menuptr m)

{

    pa_menuptr lp;

    /* clear these links for insurance */
    m->next = NULL; /* clear next */
    m->branch = NULL; /* clear branch */
    if (!*list) *list = m; /* list empty, set as first entry */
    else { /* list non-empty */

        /* find last entry in list */
        lp = *list; /* index 1st on list */
        while (lp->next) lp = lp->next;
        lp->next = m; /* append at end */

    }

}

/*******************************************************************************

Create standard menu

Creates a standard menu set. Given a set of standard items selected in a set,
and a program added menu list, creates a new standard menu.

On this windows version, the standard lists are:

file edit <program> window help

That is, all of the standard items are sorted into the lists at the start and
end of the menu,  the program selections placed in the menu.

*******************************************************************************/

/* get menu entry */
void getmenu(pa_menuptr* m, int id, char* face)

{

    *m = malloc(sizeof(pa_menurec));
    if (!*m) error(enomem);
    (*m)->next   = NULL; /* no next */
    (*m)->branch = NULL; /* no branch */
    (*m)->onoff  = FALSE; /* ! an on/off value */
    (*m)->oneof  = FALSE; /* ! a "one of" value */
    (*m)->bar    = FALSE; /* no bar under */
    (*m)->id     = id;    /* no id */
    (*m)->face = str(face); /* place face string */

}

/* add standard list item */
void additem(pa_stdmenusel sms, int i, pa_menuptr* m, pa_menuptr* l, char* s,
             int b)

{

    if (BIT(i) & sms) { /* this item is active */

        getmenu(m, i, s); /* get menu item */
        appendmenu(l, *m); /* add to list */
        (*m)->bar = b; /* set bar status */

    }

}

void stdmenu(pa_stdmenusel sms, pa_menuptr* sm, pa_menuptr pm)

{

    pa_menuptr m, hm; /* pointer for menu entries */

    *sm = NULL; /* clear menu */

    /* check and perform "file" menu */

    if (sms & (BIT(PA_SMNEW) | BIT(PA_SMOPEN) | BIT(PA_SMCLOSE) |
               BIT(PA_SMSAVE) | BIT(PA_SMSAVEAS) | BIT(PA_SMPAGESET) |
               BIT(PA_SMPRINT) | BIT(PA_SMEXIT))) { /* file menu */

        getmenu(&hm, 0, "File"); /* get entry */
        appendmenu(sm, hm);

        additem(sms, PA_SMNEW, &m, &hm->branch, "New", FALSE);
        additem(sms, PA_SMOPEN, &m, &hm->branch, "Open", FALSE);
        additem(sms, PA_SMCLOSE, &m, &hm->branch, "Close", FALSE);
        additem(sms, PA_SMSAVE, &m, &hm->branch, "Save", FALSE);
        additem(sms, PA_SMSAVEAS, &m, &hm->branch, "Save As", TRUE);
        additem(sms, PA_SMPAGESET, &m, &hm->branch, "Page Setup", FALSE);
        additem(sms, PA_SMPRINT, &m, &hm->branch, "Print", TRUE);
        additem(sms, PA_SMEXIT, &m, &hm->branch, "Exit", FALSE);

   }

   /* check and perform "edit" menu */

   if (sms&(BIT(PA_SMUNDO) | BIT(PA_SMCUT) | BIT(PA_SMPASTE) |
            BIT(PA_SMDELETE) | BIT(PA_SMFIND) | BIT(PA_SMFINDNEXT) |
            BIT(PA_SMREPLACE) | BIT(PA_SMGOTO) | BIT(PA_SMSELECTALL))) {

        /* file menu */
        getmenu(&hm, 0, "Edit"); /* get entry */
        appendmenu(sm, hm);

        additem(sms, PA_SMUNDO, &m, &hm->branch, "Undo", TRUE);
        additem(sms, PA_SMCUT, &m, &hm->branch, "Cut", FALSE);
        additem(sms, PA_SMPASTE, &m, &hm->branch, "Paste", FALSE);
        additem(sms, PA_SMDELETE, &m, &hm->branch, "Delete", TRUE);
        additem(sms, PA_SMFIND, &m, &hm->branch, "Find", FALSE);
        additem(sms, PA_SMFINDNEXT, &m, &hm->branch, "Find Next", FALSE);
        additem(sms, PA_SMREPLACE, &m, &hm->branch, "Replace", FALSE);
        additem(sms, PA_SMGOTO, &m, &hm->branch, "Goto", TRUE);
        additem(sms, PA_SMSELECTALL, &m, &hm->branch, "Select All", FALSE);

   }

   /* insert custom menu */

   while (pm) { /* insert entries */

        m = pm; /* index top button */
        pm = pm->next; /* next button */
        appendmenu(sm, m);

   }

   /* check and perform "window" menu */

   if (sms & (BIT(PA_SMNEWWINDOW) | BIT(PA_SMTILEHORIZ) | BIT(PA_SMTILEVERT) |
              BIT(PA_SMCASCADE) | BIT(PA_SMCLOSEALL)))  { /* file menu */

        getmenu(&hm, 0, "Window"); /* get entry */
        appendmenu(sm, hm);

        additem(sms, PA_SMNEWWINDOW, &m, &hm->branch, "New Window", TRUE);
        additem(sms, PA_SMTILEHORIZ, &m, &hm->branch, "Tile Horizontally", FALSE);
        additem(sms, PA_SMTILEVERT, &m, &hm->branch, "Tile Vertically", FALSE);
        additem(sms, PA_SMCASCADE, &m, &hm->branch, "Cascade", TRUE);
        additem(sms, PA_SMCLOSEALL, &m, &hm->branch, "Close All", FALSE);

   }

   /* check && perform "help" menu */

   if (sms & (BIT(PA_SMHELPTOPIC) | BIT(PA_SMABOUT))) { /* file menu */

        getmenu(&hm, 0, "Help"); /* get entry */
        appendmenu(sm, hm);

        additem(sms, PA_SMHELPTOPIC, &m, &hm->branch, "Help Topics", TRUE);
        additem(sms, PA_SMABOUT, &m, &hm->branch, "About", FALSE);

    }

}

/*******************************************************************************

Create widget

Creates a widget within the given window, within the specified bounding box,
and using the face string && type, && the given id. The string may or may not
be used.

Widgets use the subthread to buffer them. There were various problems from
trying to start them on the main window.

*******************************************************************************/

/* create widget according to type */
HWND createwidget(winptr win, wigtyp typ, int x1, int y1, int x2, int y2,
                  char* s, int id, int exfl)

{

    HWND  wh;     /* handle to widget */
    imptr ip;     /* intertask message pointer */
    char* clsstr;
    int   fl;     /* creation flag */
    int   b;      /* return value */

    /* search previous definition with same id */
    if (fndwig(win, id)) error(ewigdup); /* duplicate widget id */
    switch (typ) { /* widget type */

        case wtbutton:
            clsstr = str("button");
            fl = BS_PUSHBUTTON | exfl;  /* button */
            break;

       case wtcheckbox:
          clsstr = str("button");
          fl = BS_CHECKBOX | exfl;    /* checkbox */
          break;

       case wtradiobutton:
          clsstr = str("button");
          fl = BS_RADIOBUTTON | exfl; /* radio button */
          break;

       case wtgroup:
          clsstr = str("button");
          fl = BS_GROUPBOX | exfl;    /* group box */
          break;

       case wtbackground:
          clsstr = str("static");
          fl = 0 | exfl;   /* background */
          break;

       case wtscrollvert: /* vertical scroll bar */
          clsstr = str("scrollbar");
          fl = SBS_VERT | exfl;
          break;

       case wtscrollhoriz: /* horizontal scrollbar */
          clsstr = str("scrollbar");
          fl = SBS_HORZ | exfl;
          break;

       case wteditbox: /* single line edit */
          clsstr = str("edit");
          fl = WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | exfl;
          break;

       case wtprogressbar: /* progress bar */
          clsstr = str("msctls_progress32");
          fl = 0 | exfl;
          break;

       case wtlistbox: /* list box */
          clsstr = str("listbox");
          fl = (LBS_STANDARD&~LBS_SORT) | exfl;
          break;

       case wtdropbox: /* list box */
          clsstr = str("combobox");
          fl = CBS_DROPDOWNLIST | exfl;
          break;

       case wtdropeditbox: /* list box */
          clsstr = str("combobox");
          fl = CBS_DROPDOWN | exfl;
          break;

       case wtslidehoriz: /* horizontal slider */
          clsstr = str("msctls_trackbar32");
          fl = TBS_HORZ | TBS_AUTOTICKS | exfl;
          break;

       case wtslidevert: /* vertical slider */
          clsstr = str("msctls_trackbar32");
          fl = TBS_VERT | TBS_AUTOTICKS | exfl;
          break;

       case wttabbar: /* tab bar */
          clsstr = str("systabcontrol32");
          fl = WS_VISIBLE | exfl;
          break;

    }
    /* create an intertask message to start the widget */
    getitm(&ip); /* get a im pointer */
    ip->im = imwidget; /* set type is widget */
    ip->wigcls = clsstr; /* place class string */
    ip->wigtxt = str(s); /* place face text */
    ip->wigflg = WS_CHILD | WS_VISIBLE | fl;
    ip->wigx = x1-1; /* place origin */
    ip->wigy = y1-1;
    ip->wigw = x2-x1+1; /* place size */
    ip->wigh = y2-y1+1;
    ip->wigpar = win->winhan; /* place parent */
    ip->wigid = id; /* place id */
    ip->wigmod = GetModuleHandle(NULL); /* place module */
    /* order widget to start */
    b = PostMessage(dispwin, UMIM, (WPARAM)ip, 0);
    if (!b) winerr(); /* process windows error */
    /* Wait for widget start, this also keeps our window going. */
    waitim(imwidget, &ip); /* wait for the return */
    wh = ip->wigwin; /* place handle to widget */
    free(ip->wigcls); /* release class string */
    free(ip->wigtxt); /* release face text string */
    putitm(ip); /* release im */

    return (wh); /* return handle */

}

void widget(winptr win, int x1, int y1, int x2, int y2, char* s, int id,
            wigtyp typ, int exfl, wigptr* wp)

{

    getwig(win, wp); /* get new widget */
    /* Group widgets don"t have a background, so we pair it up with a background
       widget. */
    if (typ == wtgroup)  /* create buddy for group */
        (*wp)->han2 = createwidget(win, wtbackground, x1, y1, x2, y2, "", id,
                                  exfl);
    /* create widget */
    (*wp)->han = createwidget(win, typ, x1, y1, x2, y2, s, id, exfl);
    (*wp)->id = id; /* place button id */
    (*wp)->typ = typ; /* place type */

}

/*******************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

void ikillwidget(winptr win, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* ! found */
    kilwin(wp->han); /* kill window */
    if (wp->han2) kilwin(wp->han2); /* distroy buddy window */
    putwig(win, wp); /* release widget entry */

}

void killwidget(FILE* f, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ikillwidget(win, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Select/deselect widget

Selects or deselects a widget.

*******************************************************************************/

void iselectwidget(winptr win, int id, int e)

{

    wigptr wp;  /* widget entry */
    int    r; /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* ! found */
    /* check this widget is selectable */
    if (wp->typ != wtcheckbox && wp->typ != wtradiobutton) error(ewigsel);
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, BM_SETCHECK, e, 0);
    lockmain();/* start exclusive access */

}

void selectwidget(FILE* f, int id, int e)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iselectwidget(win, id, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Enable/disable widget

Enables or disables a widget.

*******************************************************************************/

void ienablewidget(winptr win, int id, int e)

{

    wigptr wp; /* widget entry */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* ! found */
    /* check this widget can get text */
    if (wp->typ != wtbutton && wp->typ != wtcheckbox &&
        wp->typ != wtradiobutton && wp->typ != wtgroup &&
        wp->typ != wtscrollvert && wp->typ != wtscrollhoriz &&
        wp->typ != wtnumselbox && wp->typ != wteditbox &&
        wp->typ != wtlistbox && wp->typ != wtdropbox &&
        wp->typ != wtdropeditbox && wp->typ != wtslidehoriz &&
        wp->typ != wtslidevert && wp->typ != wttabbar) error(ewigdis);
    unlockmain(); /* end exclusive access */
    EnableWindow(wp->han, e); /* perform */
    lockmain(); /* start exclusive access */
    wp->enb = e; /* save enable/disable status */

}

void enablewidget(FILE* f, int id, int e)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ienablewidget(win, id, e); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Get widget text

Retrives the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

*******************************************************************************/

void igetwidgettext(winptr win, int id, char* s, int sl)

{

    wigptr wp; /* widget pointer */
    int    ls; /* length of text */
    char*  sp; /* pointer to string */
    int    i;  /* index for string */
    int    r;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* ! found */
    /* check this widget can get text */
    if (wp->typ != wteditbox && wp->typ != wtdropeditbox) error(ewiggtxt);
    /* There is no real way to process an error, as listed in the
      documentation, for GetWindowText. The docs define
      a zero return as being for a zero length string, but also apparently
      uses that value for errors. */
    unlockmain(); /* end exclusive access */
    r = GetWindowText(wp->han, s, sl); /* get the text */
    lockmain(); /* start exclusive access */

}

void getwidgettext(FILE* f, int id, char* s, int sl)

{

    winptr win;  /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igetwidgettext(win, id, s, sl); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Put edit box text

Places text into an edit box.

*******************************************************************************/

void iputwidgettext(winptr win, int id, char* s)

{

    wigptr wp; /* widget pointer */
    BOOL   b;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    /* check this widget can put text */
    if (wp->typ != wteditbox && wp->typ != wtdropeditbox) error(ewigptxt);
    unlockmain(); /* end exclusive access */
    b = SetWindowText(wp->han, s); /* get the text */
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */

}

void putwidgettext(FILE* f, int id, char* s)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iputwidgettext(win, id, s); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Resize widget

Changes the size of a widget.

*******************************************************************************/

void isizwidgetg(winptr win, int id,  int x, int y)

{

    wigptr wp; /* widget pointer */

    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* ! found */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(wp->han, 0, 0, 0, x, y, SWP_NOMOVE | SWP_NOZORDER);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    if (wp->han2) { /* also resize the buddy */

        /* Note, the buddy needs to be done differently for a numselbox */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(wp->han2, 0, 0, 0, x, y, SWP_NOMOVE | SWP_NOZORDER);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void sizwidgetg(FILE* f, int id, int x, int y)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    isizwidgetg(win, id, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Reposition widget

Changes the parent position of a widget.

*******************************************************************************/

void iposwidgetg(winptr win, int id, int x, int y)

{

    wigptr wp; /* widget pointer */

    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(wp->han, 0, x-1, y-1, 0, 0, SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    if (wp->han2) { /* also reposition the buddy */

        /* Note, the buddy needs to be done differently for a numselbox */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(wp->han2, 0, x-1, y-1, 0, 0, SWP_NOSIZE);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void poswidgetg(FILE* f, int id, int x, int y)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iposwidgetg(win, id, x, y); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Place widget to back of Z order

*******************************************************************************/

void ibackwidget(winptr win, int id)

{

    wigptr wp; /* widget pointer */
    BOOL   b;  /* result holder */

    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(wp->han, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    if (wp->han2) { /* also reposition the buddy */

        /* Note, the buddy needs to be done differently for a numselbox */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(wp->han2, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void backwidget(FILE* f, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
     win = txt2win(f); /* get windows context */
    ibackwidget(win, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Place widget to front of Z order

*******************************************************************************/

void ifrontwidget(winptr win, int id)

{

    wigptr wp; /* widget pointer */
    BOOL   b;  /* result holder */

    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    unlockmain(); /* end exclusive access */
    b = SetWindowPos(wp->han, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    if (wp->han2) { /* also reposition the buddy */

        /* Note, the buddy needs to be done differently for a numselbox */
        unlockmain(); /* end exclusive access */
        b = SetWindowPos(wp->han2, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE);
        lockmain(); /* start exclusive access */
        if (!b) winerr(); /* process windows error */

    }

}

void frontwidget(FILE* f, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ifrontwidget(win, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard button size

Finds the minimum size for a button. Given the face string, the minimum size of
a button is calculated and returned.

*******************************************************************************/

void ibuttonsizg(winptr win, char* s, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* add button borders to size */
    *w = sz.cx+GetSystemMetrics(SM_CXEDGE)*2;
    *h = sz.cy+GetSystemMetrics(SM_CYEDGE)*2;

}

void ibuttonsiz(winptr win, char* s, int* w, int* h)

{

    ibuttonsizg(win, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / win->charspace+1;
    *h = (*h-1) / win->linespace+1;

}

void buttonsizg(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibuttonsizg(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void buttonsiz(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibuttonsiz(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create button

Creates a standard button within the specified rectangle, on the given window.

*******************************************************************************/

void ibuttong(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, s, id, wtbutton, 0, &wp);

}

void ibutton(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    ibuttong(win, x1, y1, x2, y2, s, id); /* create button graphical */

}

void buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibuttong(win, x1, y1, x2, y2, s, id);
    unlockmain(); /* end exclusive access */

}

void button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibutton(win, x1, y1, x2, y2, s, id);
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard checkbox size

Finds the minimum size for a checkbox. Given the face string, the minimum size of
a checkbox is calculated and returned.

*******************************************************************************/

void icheckboxsizg(winptr win, char* s, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* We needed to add a fudge factor for the space between the checkbox, the
       left edge of the widget, && the left edge of the text. */
    *w = sz.cx+GetSystemMetrics(SM_CXMENUCHECK)+6; /* return size */
    *h = sz.cy;

}

void icheckboxsiz(winptr win, char* s, int* w, int* h)

{

    icheckboxsizg(win, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / win->charspace+1;
    *h = (*h-1) / win->linespace+1;

}

void checkboxsizg(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    icheckboxsizg(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void checkboxsiz(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    icheckboxsiz(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create checkbox

Creates a standard checkbox within the specified rectangle, on the given
window.

*******************************************************************************/

void icheckboxg(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, s, id, wtcheckbox, 0, &wp);

}

void icheckbox(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    icheckboxg(win, x1, y1, x2, y2, s, id); /* create button graphical */

}

void checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    icheckboxg(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    icheckbox(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard radio button size

Finds the minimum size for a radio button. Given the face string, the minimum
size of a radio button is calculated and returned.

*******************************************************************************/

void iradiobuttonsizg(winptr win, char* s, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* We needed to add a fudge factor for the space between the checkbox, the
       left edge of the widget, and the left edge of the text. */
    *w = sz.cx+GetSystemMetrics(SM_CXMENUCHECK)+6; /* return size */
    *h = sz.cy;

}

void iradiobuttonsiz(winptr win, char* s, int* w, int* h)

{

    iradiobuttonsizg(win, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

}

void radiobuttonsizg(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iradiobuttonsizg(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void radiobuttonsiz(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iradiobuttonsiz(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create radio button

Creates a standard radio button within the specified rectangle, on the given
window.

*******************************************************************************/

void iradiobuttong(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, s, id, wtradiobutton, 0, &wp);

}

void iradiobutton(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    iradiobuttong(win, x1, y1, x2, y2, s, id); /* create button graphical */

}

void radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iradiobuttong(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iradiobutton(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard group size

Finds the minimum size for a group. Given the face string, the minimum
size of a group is calculated and returned.

*******************************************************************************/

void igroupsizg(winptr win, char* s, int cw, int ch, int* w, int* h,
                int* ox, int* oy)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* Use the string sizing, and rules of thumb for the edges */
    *w = sz.cx+7*2; /* return size */
    /* if string is greater than width plus edges, use the string. */
    if (cw+7*2 > *w) *w = cw+7*2;
    *h = sz.cy+ch+5*2;
    /* set offset to client area */
    *ox = 5;
    *oy = sz.cy;

}

void igroupsiz(winptr win, char* s, int cw, int ch, int* w, int* h,
               int* ox, int* oy)

{

    /* convert client sizes to graphical */
    cw = cw*win->charspace;
    ch = ch*win->linespace;
    igroupsizg(win, s, cw, ch, w, h, ox, oy); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;
    *ox = (*ox-1)/win->charspace+1;
    *oy = (*oy-1)/win->linespace+1;

}

void groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h,
               int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igroupsizg(win, s, cw, ch, w, h, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

void groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h,
              int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igroupsiz(win, s, cw, ch, w, h, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create group box

Creates a group box, which is really just a decorative feature that gererates
no messages. It is used as a background for other widgets.

*******************************************************************************/

void igroupg(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, s, id, wtgroup, 0, &wp);

}

void igroup(winptr win, int x1, int y1, int x2, int y2, char* s, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    igroupg(win, x1, y1, x2, y2, s, id); /* create button graphical */

}

void groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igroupg(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    igroup(win, x1, y1, x2, y2, s, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create background box

Creates a background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

*******************************************************************************/

void ibackgroundg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr wp; /* widget pointer */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtbackground, 0, &wp);

}

void ibackground(winptr win, int x1, int y1, int x2, int y2, int id)

{

   /* form graphical from character coordinates */
   x1 = (x1-1)*win->charspace+1;
   y1 = (y1-1)*win->linespace+1;
   x2 = (x2)*win->charspace;
   y2 = (y2)*win->linespace;
   ibackgroundg(win, x1, y1, x2, y2, id); /* create button graphical */

};

void backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibackgroundg(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void background(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ibackground(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard vertical scrollbar size

Finds the minimum size for a vertical scrollbar. The minimum size of a vertical
scrollbar is calculated and returned.

*******************************************************************************/

void iscrollvertsizg(winptr win, int* w, int* h)

{

    /* get system values for scroll bar arrow width and height, for which there
       are two. */
    *w = GetSystemMetrics(SM_CXVSCROLL);
    *h = GetSystemMetrics(SM_CYVSCROLL)*2;

}

void iscrollvertsiz(winptr win, int* w, int* h)

{

    /* Use fixed sizes, as this looks best */
    *w = 2;
    *h = 2;

}

void scrollvertsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollvertsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void scrollvertsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollvertsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create vertical scrollbar

Creates a vertical scrollbar.

*******************************************************************************/

void iscrollvertg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr     wp; /* widget pointer */
    SCROLLINFO si; /* scroll information structure */
    BOOL       b;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtscrollvert, 0, &wp);
    /* The scroll set for windows is arbitrary. We expand that to 0..INT_MAX on
       messages. */
    unlockmain(); /* end exclusive access */
    b = SetScrollRange(wp->han, SB_CTL, 0, 255, FALSE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    /* retrieve the default size of slider */
    si.cbSize = sizeof(SCROLLINFO); /* set size */
    si.fMask = SIF_PAGE; /* set page size */
    unlockmain(); /* end exclusive access */
    b = GetScrollInfo(wp->han, SB_CTL, &si);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    wp->siz = si.nPage; /* get size */

}

void iscrollvert(winptr win, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    iscrollvertg(win, x1, y1, x2, y2, id); /* create button graphical */

}

void scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollvertg(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollvert(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard horizontal scrollbar size

Finds the minimum size for a horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated && returned.

*******************************************************************************/

void iscrollhorizsizg(winptr win, int* w, int* h)

{

    /* get system values for scroll bar arrow width && height, for which there
       are two. */
    *w = GetSystemMetrics(SM_CXHSCROLL)*2;
    *h = GetSystemMetrics(SM_CYHSCROLL);

}

void iscrollhorizsiz(winptr win, int* w, int* h)

{

    /* Use fixed sizes, as this looks best */
    *w = 2;
    *h = 1;

}

void scrollhorizsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollhorizsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void scrollhorizsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollhorizsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create horizontal scrollbar

Creates a horizontal scrollbar.

*******************************************************************************/

void iscrollhorizg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr     wp; /* widget pointer */
    SCROLLINFO si; /* scroll information structure */
    BOOL       b;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtscrollhoriz, 0, &wp);
    /* The scroll set for windows is arbitrary. We expand that to 0..INT_MAX on
       messages. */
    unlockmain(); /* end exclusive access */
    b = SetScrollRange(wp->han, SB_CTL, 0, 255, FALSE);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    /* retrieve the default size of slider */
    si.cbSize = sizeof(SCROLLINFO); /* set size */
    si.fMask = SIF_PAGE; /* set page size */
    unlockmain(); /* end exclusive access */
    b = GetScrollInfo(wp->han, SB_CTL, &si);
    lockmain(); /* start exclusive access */
    if (!b) winerr(); /* process windows error */
    wp->siz = si.nPage; /* get size */

}

void iscrollhoriz(winptr win, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    iscrollhorizg(win, x1, y1, x2, y2, id); /* create button graphical */

}

void scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollhorizg(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollhoriz(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

*******************************************************************************/

void iscrollpos(winptr win, int id, int r)

{

    wigptr wp; /* widget pointer */
    int    rv; /* return value */
    float  f;  /* floating temp */
    int    p;  /* calculated position to set */

    if (r < 0) error(einvspos); /* invalid position */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* ! found */
    f = r; /* place position in float */
    /* clamp to max */
    if (f*(255-wp->siz)/INT_MAX > 255) p = 255;
    else p = f*(255-wp->siz)/INT_MAX;
    unlockmain(); /* end exclusive access */
    rv = SetScrollPos(wp->han, SB_CTL, p, TRUE);
    lockmain();/* start exclusive access */

}

void scrollpos(FILE* f, int id, int r)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollpos(win, id, r); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

*******************************************************************************/

void iscrollsiz(winptr win, int id, int r)

{

    wigptr     wp; /* widget pointer */
    int        rv; /* return value */
    SCROLLINFO si; /* scroll information structure */

    if (r < 0) error(einvssiz); /* invalid scrollbar size */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* ! found */
    si.cbSize = sizeof(SCROLLINFO); /* set size */
    si.fMask = SIF_PAGE; /* set page size */
    si.nMin = 0; /* no min */
    si.nMax = 0; /* no max */
    si.nPage = r/0x800000; /* set size */
    si.nPos = 0; /* no position */
    si.nTrackPos = 0; /* no track position */
    unlockmain(); /* end exclusive access */
    rv = SetScrollInfo(wp->han, SB_CTL, &si, TRUE);
    lockmain(); /* start exclusive access */
    wp->siz = r/0x800000; /* set size */

}

void scrollsiz(FILE* f, int id, int r)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iscrollsiz(win, id, r); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Control window function for number edit box

This message handler is to allow us to capture the carriage return from an
number edit box, and turn that into a message. It also restricts input to the
box to numeric characters.

*******************************************************************************/

LRESULT CALLBACK wndprocnum(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam)

{

    int    r;      /* result */
    HWND   wh;     /* parent window handle */
    int    lfn;    /* logical number for parent window */
    winptr win;    /* parent window data */
    wigptr wp;     /* widget pointer */
    char   s[100]; /* buffer for edit string */
    int    v;      /* value from edit control */
    int    err;    /* error parsing value */

/*int i;*/

/*;prtstr("wndprocnum: msg: ");
;prtmsgu(hwnd, imsg, wparam, lparam);*/

    /* We need to find out who we are talking to. */
    lockmain(); /* start exclusive access */
    wh = GetParent(hwnd); /* get the widget parent handle */
    lfn = hwn2lfn(wh); /* get the logical window number */
    win = lfn2win(lfn); /* index window from logical number */
    wp = fndwighan(win, hwnd); /* find the widget from that */
    unlockmain(); /* end exclusive access */
    r = 0; /* set no error */
    /* check its a character */
    if (imsg == WM_CHAR) {

        if (wp->enb) { /* is the widget enabled ? */

            /* check control is receiving a carriage return */
            if (wparam == '\r') {

                r = GetWindowText(wp->han2, s, 100); /* get contents of edit */
                v = intv(s, &err); /* get value */
                /* Send edit sends cr message to parent window, with widget logical
                   number embedded as wparam. */
                if (!err && v >= wp->low && v <= wp->high)
                    putmsg(wh, UMNUMCR, wp->id, v); /* send select message */
                else
                    /* Send the message on to its owner, this will ring the bell in
                       Windows XP. */
                    r = CallWindowProc(wp->wprc, hwnd, imsg, wparam, lparam);

            } else {

                /* Check valid numerical character, with backspace control. If not,
                   replace with carriage return. This will cause the control to emit
                   an error, a bell in Windows XP. */
                if (!((wparam >= '0' && wparam < '9') || wparam == '+' ||
                      wparam == '-' || wparam == '\b'))
                    wparam = '\r';
                r = CallWindowProc(wp->wprc, hwnd, imsg, wparam, lparam);

            }

        }

    } else
        /* send the message on to its owner */
        r = CallWindowProc(wp->wprc, hwnd, imsg, wparam, lparam);

    return (r); /* return result */

}

/*******************************************************************************

Find minimum/standard number select box size

Finds the minimum size for a number select box. The minimum size of a number
select box is calculated && returned.

*******************************************************************************/

void inumselboxsizg(winptr win, int l, int u,  int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    /* get size of text */
    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    if (u > 9) b = GetTextExtentPoint32(dc, "00", 2, &sz); /* get sizing */
    else b = GetTextExtentPoint32(dc, "0", 1, &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* width of text, plus up/down arrows, && border && divider lines */
    *w = sz.cx+GetSystemMetrics(SM_CXVSCROLL)+4;
    *h = sz.cy+2; /* height of text plus border lines */

}

void inumselboxsiz(winptr win, int l, int u, int* w, int* h)

{

    inumselboxsizg(win, l, u, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / win->charspace+1;
    *h = (*h-1) / win->linespace+1;

}

void numselboxsizg(FILE* f, int l, int u, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    inumselboxsizg(win, l, u, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void numselboxsiz(FILE* f, int l, int u, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    inumselboxsiz(win, l, u, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create number selector

Creates an up/down control for numeric selection.

*******************************************************************************/

void inumselboxg(winptr win, int x1, int y1, int x2, int y2, int l, int u,
                 int id)

{

    imptr  ip;  /* intratask message pointer */
    wigptr wp;  /* widget pointer */
    BOOL   br;  /* result */
    int    udw; /* up/down control width */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* search previous definition with same id */
    if (fndwig(win, id)) error(ewigdup); /* duplicate widget id */
    /* Number select is a composite control, and it will send messages
      immediately after creation, as the different components talk to each
      other. Because of this, we must create a widget entry first, even if
      it is incomplete. */
    getwig(win, &wp); /* get new widget */
    wp->id = id; /* place button id */
    wp->typ = wtnumselbox; /* place type */
    wp->han = 0; /* clear handles */
    wp->han2 = 0;
    wp->low = l; /* place limits */
    wp->high = u;
    /* get width of up/down control (same as scroll arrow) */
    udw = GetSystemMetrics(SM_CXHSCROLL);
    /* If the width is ! enough for the control to appear, force it. */
    if (x2-x1+1 < udw) x2 = x1+udw-1;
    getitm(&ip); /* get a im pointer */
    ip->im = imupdown; /* set is up/down control */
    ip->udflg = WS_CHILD | WS_VISIBLE | WS_BORDER | UDS_SETBUDDYINT;
    ip->udx = x1-1;
    ip->udy = y1-1;
    ip->udcx = x2-x1+1;
    ip->udcy = y2-y1+1;
    ip->udpar = win->winhan;
    ip->udid = id;
    ip->udinst = GetModuleHandle(NULL);
    ip->udup = u;
    ip->udlow = l;
    ip->udpos = l;
    br = PostMessage(dispwin, UMIM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imupdown, &ip); /* wait for the return */
    wp->han = ip->udhan; /* place control handle */
    wp->han2 = ip->udbuddy; /* place buddy handle */
    putitm(ip); /* release im */
    /* place our subclass handler for the edit control */
    wp->wprc = (WNDPROC)GetWindowLongPtr(wp->han2, GWL_WNDPROC);
    if (!wp->wprc) winerr(); /* process windows error */
    r = SetWindowLong(wp->han2, GWL_WNDPROC, (LONG)wndprocnum);
    if (!r) winerr(); /* process windows error */

}

void inumselbox(winptr win, int x1, int y1, int x2, int y2, int l, int u,
                int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = x2*win->charspace;
    y2 = y2*win->linespace;
    inumselboxg(win, x1, y1, x2, y2, l, u, id); /* create button graphical */

}

void numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    inumselboxg(win, x1, y1, x2, y2, l, u, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    inumselbox(win, x1, y1, x2, y2, l, u, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Control window function for edit box

This message handler is to allow us to capture the carriage return from an
edit box, and turn that into a message.

*******************************************************************************/

int wndprocedit(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam)

{

    LRESULT r;   /* result */
    HWND    wh;  /* parent window handle */
    int     lfn; /* logical number for parent window */
    winptr  win; /* parent window data */
    wigptr  wp;  /* widget pointer */

/*;prtstr("wndprocedit: msg: ");
;prtmsgu(hwnd, imsg, wparam, lparam);*/

    /* We need to find out who we are talking to. */
    wh = GetParent(hwnd); /* get the widget parent handle */
    lfn = hwn2lfn(wh); /* get the logical window number */
    win = lfn2win(lfn); /* index window from logical number */
    wp = fndwighan(win, hwnd); /* find the widget from that */
    /* check control is receiving a carriage return */
    if (imsg == WM_CHAR && wparam == '\r')
        /* Send edit sends cr message to parent window, with widget logical
           number embedded as wparam. */
        putmsg(wh, UMEDITCR, wp->id, 0);
    else
        /* send the message on to its owner */
        r = CallWindowProc(wp->wprc, hwnd, imsg, wparam, lparam);

    return (r); /* return result */

}

/*******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void ieditboxsizg(winptr win, char* s, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), &sz); /* get sizing */
    if (!b) winerr(); /* process windows error */
    /* add borders to size */
    *w = sz.cx+4;
    *h = sz.cy+2;

}

void ieditboxsiz(winptr win, char* s, int* w, int* h)

{

    ieditboxsizg(win, s, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1) / win->charspace+1;
    *h = (*h-1) / win->linespace+1;

}

void editboxsizg(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ieditboxsizg(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void editboxsiz(FILE* f, char* s, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ieditboxsiz(win, s, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create edit box

Creates single line edit box

*******************************************************************************/

void ieditboxg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr wp; /* widget pointer */
    LONG   r;

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wteditbox, 0, &wp);
    /* get the windows internal void for subclassing */
    wp->wprc = (WNDPROC)GetWindowLongPtr(wp->han, GWL_WNDPROC);
    if (!wp->wprc) winerr(); /* process windows error */
    r = SetWindowLong(wp->han, GWL_WNDPROC, (LONG)wndprocedit);
    if (!r) winerr(); /* process windows error */

}

void ieditbox(winptr win, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    ieditboxg(win, x1, y1, x2, y2, id); /* create button graphical */

}

void editboxg(FILE* f,  int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ieditboxg(win, x1,y1, x2,y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void editbox(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ieditbox(win, x1,y1, x2,y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void iprogbarsizg(winptr win, int* w, int* h)

{

    /* Progress bars are arbitrary for sizing. We choose a size that allows for
       20 bar elements. Note that the size of the blocks in a Windows progress
       bar are ratioed to the height. */
    *w = 20*14+2;
    *h = 20+2;

}

void iprogbarsiz(winptr win, int* w, int* h)

{

    iprogbarsizg(win, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

};

void progbarsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbarsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void progbarsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbarsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create progress bar

Creates a progress bar.

*******************************************************************************/

void iprogbarg(winptr win, int x1, int y1, int x2, int y2, int id)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* create the progress bar */
    widget(win, x1, y1, x2, y2, "", id, wtprogressbar, 0, &wp);
    /* use 0..INT_MAX ratio */
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, PBM_SETRANGE32, 0, INT_MAX);
    lockmain(); /* start exclusive access */

}

void iprogbar(winptr win, int x1, int y1, int x2, int y2, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    iprogbarg(win, x1, y1, x2, y2, id); /* create button graphical */

}

void progbarg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win;  /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbarg(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void progbar(FILE* f, int x1, int y1, int x2, int y2, int id)

{

    winptr win;  /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbar(win, x1, y1, x2, y2, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set progress bar position

Sets the position of a progress bar, from 0 to INT_MAX.

*******************************************************************************/

void iprogbarpos(winptr win, int id, int pos)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;

   if (!win->visible) winvis(win); /* make sure we are displayed */
   if (pos < 0) error(eprgpos); /* bad position */
   wp = fndwig(win, id); /* find widget */
   if (!wp) error(ewignf); /* ! found */
   /* set the range */
   unlockmain(); /* end exclusive access */
   r = SendMessage(wp->han, PBM_SETPOS, pos, 0);
   lockmain(); /* start exclusive access */

}

void progbarpos(FILE* f, int id, int pos)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iprogbarpos(win, id, pos); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard list box size

Finds the minimum size for an list box. Given a string list, the minimum size
of an list box is calculated and returned.

Windows listboxes pretty much ignore the size given. If you allocate more space
than needed, it will only put blank lines below if enough space for an entire
line is present. If the size does not contain exactly enough to display the
whole line list, the box will colapse to a single line with an up/down
control. The only thing that is garanteed is that the box will fit within the
specified rectangle, one way or another.

*******************************************************************************/

void ilistboxsizg(winptr win, pa_strptr sp, int* w, int* h)

{

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */
    int  mw; /* max width */

    *w = 4; /* set minimum overhead */
    *h = 2;
    while (sp) { /* traverse string list */

        dc = GetWindowDC(NULL); /* get screen dc */
        if (!dc) winerr(); /* process windows error */
        b = GetTextExtentPoint32(dc, sp->str, strlen(sp->str), &sz); /* get sizing */
        if (!b) winerr(); /* process windows error */
        /* add borders to size */
        mw = sz.cx+4;
        if (mw > *w) *w = mw; /* set new maximum */
        *h = *h+sz.cy;
        sp = sp->next; /* next string */

    }

}

void ilistboxsiz(winptr win, pa_strptr sp, int* w, int* h)

{

    ilistboxsizg(win, sp, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

}

void listboxsizg(FILE* f, pa_strptr sp, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ilistboxsizg(win, sp, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void listboxsiz(FILE* f, pa_strptr sp, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ilistboxsiz(win, sp, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create list box

Creates a list box. Fills it with the string list provided.

*******************************************************************************/

void ilistboxg(winptr win, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    wigptr wp; /* widget pointer */
    LRESULT r; /* return value */

    if (!win->visible)  winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtlistbox, 0, &wp);
    while (sp) { /* add strings to list */

        unlockmain(); /* end exclusive access */
        r = SendMessage(wp->han, LB_ADDSTRING, 0, (LPARAM)sp->str); /* add string */
        lockmain(); /* start exclusive access */
        if (r == -1) error(estrspc); /* out of string space */
        sp = sp->next; /* next string */

    }

}

void ilistbox(winptr win, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    ilistboxg(win, x1, y1, x2, y2, sp, id); /* create button graphical */

}

void listboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ilistboxg(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void listbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    ilistbox(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find size for single line of text

Find the pixel size in x for the given line of text.

*******************************************************************************/

void getsizlin(char* s, LPSIZE sz)

{

    HDC dc;

    dc = GetWindowDC(NULL); /* get screen dc */
    if (!dc) winerr(); /* process windows error */
    b = GetTextExtentPoint32(dc, s, strlen(s), sz); /* get sizing */
    if (!b) winerr(); /* process windows error */

}

/*******************************************************************************

Find minimum/standard dropbox size

Finds the minimum size for a dropbox. Given the face string, the minimum size of
a dropbox is calculated and returned, for both the "open" and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void idropboxsizg(winptr win, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    /* I can't find a reasonable system metrics version of the drop arrow
       demensions, so they are hardcoded here. */
    const int darrowx = 17;
    const int darrowy = 20;

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    /* calculate first line */
    getsizlin(sp->str, &sz); /* find sizing for line */
    /* Find size of string x, drop arrow width, box edges, and add fudge factor
      to space text out. */
    *cw = sz.cx+darrowx+GetSystemMetrics(SM_CXEDGE)*2+4;
    *ow = *cw; /* open is the same */
    /* drop arrow height+shadow overhead+drop box bounding */
    *oh = darrowy+GetSystemMetrics(SM_CYEDGE)*2+2;
    /* drop arrow height+shadow overhead */
    *ch = darrowy+GetSystemMetrics(SM_CYEDGE)*2;
    /* add all lines to drop box section */
    while (sp) { /* traverse string list */

        getsizlin(sp->str, &sz); /* find sizing for this line */
        /* find open width on this string only */
        *ow = sz.cx+darrowx+GetSystemMetrics(SM_CXEDGE)*2+4;
        if (*ow > *cw) *cw = *ow; /* larger than closed width, set new max */
        *oh = *oh+sz.cy; /* add to open height */
        sp = sp->next; /* next string */

    }
    *ow = *cw; /* set maximum open width */

}

void idropboxsiz(winptr win, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    idropboxsizg(win, sp, cw, ch, ow, oh); /* get size */
    /* change graphical size to character */
    *cw = (*cw-1)/win->charspace+1;
    *ch = (*ch-1)/win->linespace+1;
    *ow = (*ow-1)/win->charspace+1;
    *oh = (*oh-1)/win->linespace+1;

}

void dropboxsizg(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropboxsizg(win, sp, cw, ch, ow, oh); /* get size */
    unlockmain(); /* end exclusive access */

}

void dropboxsiz(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropboxsiz(win, sp, cw, ch, ow, oh); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create dropdown box

Creates a dropdown box. Fills it with the string list provided.

*******************************************************************************/

void idropboxg(winptr win, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    wigptr wp;  /* widget pointer */
    pa_strptr sp1;  /* string pointer */
    LRESULT r; /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtdropbox, 0, &wp);
    sp1 = sp; /* index top of string list */
    while (sp1) { /* add strings to list */

        unlockmain(); /* end exclusive access */
        r = SendMessage(wp->han, CB_ADDSTRING, 0, (LPARAM)sp1->str); /* add string */
        lockmain(); /* start exclusive access */
        if (r == -1) error(estrspc); /* out of string space */
        sp1 = sp1->next; /* next string */

    }
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, CB_SETCURSEL, 0, 0);
    lockmain(); /* start exclusive access */
    if (r == -1)  error(esystem); /* should ! happen */

}

void idropbox(winptr win, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    idropboxg(win, x1, y1, x2, y2, sp, id); /* create button graphical */

}

void dropboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropboxg(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void dropbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropbox(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard drop edit box size

Finds the minimum size for a drop edit box. Given the face string, the minimum
size of a drop edit box is calculated and returned, for both the "open" and
"closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void idropeditboxsizg(winptr win, pa_strptr sp, int* cw, int* ch,
                      int* ow, int* oh)

{

    /* I can"t find a reasonable system metrics version of the drop arrow
       demensions, so they are hardcoded here. */
    const int darrowx = 17;
    const int darrowy = 20;

    SIZE sz; /* size holder */
    BOOL b;  /* return value */
    HDC  dc; /* dc for screen */

    /* calculate first line */
    getsizlin(sp->str, &sz); /* find sizing for line */
    /* Find size of string x, drop arrow width, box edges, and add fudge factor
       to space text out. */
//    cw = sz.cx+darrowx+GetSystemMetrics(SM_CXEDGE)*2+4;
    *ow = *cw; /* open is the same */
    /* drop arrow height+shadow overhead+drop box bounding */
    *oh = darrowy+GetSystemMetrics(SM_CYEDGE)*2+2;
    /* drop arrow height+shadow overhead */
//    ch = darrowy+GetSystemMetrics(SM_CYEDGE)*2;
    /* add all lines to drop box section */
    while (sp) { /* traverse string list */

        getsizlin(sp->str, &sz); /* find sizing for this line */
        /* find open width on this string only */
        *ow = sz.cx+darrowx+GetSystemMetrics(SM_CXEDGE)*2+4;
        if (*ow > *cw) *cw = *ow; /* larger than closed width, set new max */
        *oh = *oh+sz.cy; /* add to open height */
        sp = sp->next; /* next string */

    }
    *ow = *cw; /* set maximum open width */

}

void idropeditboxsiz(winptr win, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    idropeditboxsizg(win, sp, cw, ch, ow, oh); /* get size */
    /* change graphical size to character */
    *cw = (*cw-1)/win->charspace+1;
    *ch = (*ch-1)/win->linespace+1;
    *ow = (*ow-1)/win->charspace+1;
    *oh = (*oh-1)/win->linespace+1;

}

void dropeditboxsizg(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropeditboxsizg(win, sp, cw, ch, ow, oh); /* get size */
    unlockmain(); /* end exclusive access */

}

void dropeditboxsiz(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropeditboxsiz(win, sp, cw, ch, ow, oh); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create dropdown edit box

Creates a dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

void idropeditboxg(winptr win, int x1, int y1, int x2, int y2, pa_strptr sp,
                   int id)

{

    wigptr    wp;  /* widget pointer */
    pa_strptr sp1; /* string pointer */
    LRESULT   r;   /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    widget(win, x1, y1, x2, y2, "", id, wtdropeditbox, 0, &wp);
    sp1 = sp; /* index top of string list */
    while (sp1) { /* add strings to list */

        unlockmain(); /* end exclusive access */
        r = SendMessage(wp->han, CB_ADDSTRING, 0, (LPARAM)sp1->str); /* add string */
        lockmain(); /* start exclusive access */
        if (r == -1) error(estrspc); /* out of string space */
        sp1 = sp1->next; /* next string */

    }

}

void idropeditbox(winptr win, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    idropeditboxg(win, x1, y1, x2, y2, sp, id); /* create button graphical */

}

void dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropeditboxg(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void dropeditbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    idropeditbox(win, x1, y1, x2, y2, sp, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard horizontal slider size

Finds the minimum size for a horizontal slider. The minimum size of a horizontal
slider is calculated and returned.

*******************************************************************************/

void islidehorizsizg(winptr win, int* w, int* h)

{

    /* The width is that of an average slider. The height is what is needed to
       present the slider, tick marks, and 2 pixels of spacing around it. */
    *w = 200;
    *h = 32;

}

void islidehorizsiz(winptr win, int* w, int* h)

{

    islidehorizsizg(win, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

}

void slidehorizsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidehorizsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void slidehorizsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidehorizsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create horizontal slider

Creates a horizontal slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void islidehorizg(winptr win, int x1, int y1, int x2, int y2, int mark, int id)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    if (!mark) /* tick marks enabled */
        widget(win, x1, y1, x2, y2, "", id, wtslidehoriz, TBS_NOTICKS, &wp);
    else /* tick marks enabled */
        widget(win, x1, y1, x2, y2, "", id, wtslidehoriz, 0, &wp);
    /* set tickmark frequency */
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, TBM_SETTICFREQ, mark, 0);
    lockmain(); /* start exclusive access */

}

void islidehoriz(winptr win, int x1, int y1, int x2, int y2, int mark, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    islidehorizg(win, x1, y1, x2, y2, mark, id); /* create button graphical */

}

void slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidehorizg(win, x1, y1, x2, y2, mark, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidehoriz(win, x1, y1, x2, y2, mark, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find minimum/standard vertical slider size

Finds the minimum size for a vertical slider. The minimum size of a vertical
slider is calculated && returned.

*******************************************************************************/

void islidevertsizg(winptr win, int* w, int* h)

{

    /* The height is that of an average slider. The width is what is needed to
       present the slider, tick marks, && 2 pixels of spacing around it. */
    *w = 32;
    *h = 200;

}

void islidevertsiz(winptr win, int* w, int* h)

{

    islidevertsizg(win, w, h); /* get size */
    /* change graphical size to character */
    *w = (*w-1)/win->charspace+1;
    *h = (*h-1)/win->linespace+1;

}

void slidevertsizg(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidevertsizg(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

void slidevertsiz(FILE* f, int* w, int* h)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidevertsiz(win, w, h); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create vertical slider

Creates a vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void islidevertg(winptr win, int x1, int y1, int x2, int y2, int mark, int id)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;  /* return value */

    if (!win->visible) winvis(win); /* make sure we are displayed */
    if (!mark) /* tick marks off */
        widget(win, x1, y1, x2, y2, "", id, wtslidevert, TBS_NOTICKS, &wp);
    else /* tick marks enabled */
        widget(win, x1, y1, x2, y2, "", id, wtslidevert, 0, &wp);
    /* set tickmark frequency */
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, TBM_SETTICFREQ, mark, 0);
    lockmain(); /* start exclusive access */

}

void islidevert(winptr win, int x1, int y1, int x2, int y2, int mark, int id)

{

   /* form graphical from character coordinates */
   x1 = (x1-1)*win->charspace+1;
   y1 = (y1-1)*win->linespace+1;
   x2 = (x2)*win->charspace;
   y2 = (y2)*win->linespace;
   islidevertg(win, x1, y1, x2, y2, mark, id); /* create button graphical */

}

void slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidevertg(win, x1, y1, x2, y2, mark, id);
    unlockmain(); /* end exclusive access */

}

void slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    islidevert(win, x1, y1, x2, y2, mark, id);
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create and distroy useless widget

This appears to be a windows bug. When tab bars are created, they allow
themselves to be overritten by the parent. This only occurs on tab bars.
The workaround is to create a distroy a widget right after creating the
tab bar, since only the last widget created has this problem.

Bug: this is not getting deleted. I fixed it temporarily by making it
invisible.

*******************************************************************************/

void uselesswidget(winptr win)

{

    imptr ip; /* intratask message pointer */

    getitm(&ip); /* get a im pointer */
    ip->im = imwidget; /* set type is widget */
    strcpy(ip->wigcls, "static");
    strcpy(ip->wigtxt, "");
    ip->wigflg = WS_CHILD /*or WS_VISIBLE*/;
    ip->wigx = 50;
    ip->wigy = 50;
    ip->wigw = 50;
    ip->wigh = 50;
    ip->wigpar = win->winhan;
    ip->wigid = 0;
    ip->wigmod = GetModuleHandle(NULL);
    /* order widget to start */
    b = PostMessage(dispwin, UMIM, (WPARAM)ip, 0);
    if (!b) winerr(); /* process windows error */
    /* Wait for widget start, this also keeps our window going. */
    waitim(imwidget, &ip); /* wait for the return */
    kilwin(ip->wigwin); /* kill widget */
    free(ip->wigcls); /* release class string */
    free(ip->wigtxt); /* release face text string */
    putitm(ip); /* release im */

}

/*******************************************************************************

Find minimum/standard tab bar size

Finds the minimum size for a tab bar. The minimum size of a tab bar is
calculated and returned.

*******************************************************************************/

void itabbarsizg(winptr win, pa_tabori tor, int cw, int ch, int* w, int* h,
                 int* ox, int* oy)

{

    if (tor == pa_toright || tor == pa_toleft)  { /* vertical bar */

        *w = 32; /* set minimum width */
        *h = 2+20*2; /* set minimum height */
        *w = *w+cw; /* add client space to width */
        if (ch+4 > *h) *h = ch+4; /* set to client if greater */
        if (tor == pa_toleft) {

            *ox = 28; /* set offsets */
            *oy = 4;

        } else {

            *ox = 4; /* set offsets */
            *oy = 4;

        }

    } else { /* horizontal bar */

        *w = 2+20*2; /* set minimum width, edges, arrows */
        *h = 32; /* set minimum height */
        if (cw+4 > *w) *w = cw+4; /* set to client if greater */
        *h = *h+ch; /* add client space to height */
        if (tor == pa_totop)  {

            *ox = 4; /* set offsets */
            *oy = 28;

        } else {

            *ox = 4; /* set offsets */
            *oy = 4;

        }

    }

}

void itabbarsiz(winptr win, pa_tabori tor, int cw, int ch, int* w, int* h,
                int* ox, int* oy)

{

    int gw, gh, gox, goy;

    /* convert client sizes to graphical */
    cw = cw*win->charspace;
    ch = ch*win->linespace;
    itabbarsizg(win, tor, cw, ch, &gw, &gh, &gox, &goy); /* get size */
    /* change graphical size to character */
    *w = (gw-1) / win->charspace+1;
    *h = (gh-1) / win->linespace+1;
    *ox = (gox-1) / win->charspace+1;
    *oy = (goy-1) / win->linespace+1;
    /* must make sure that client dosen"t intrude on edges */
    if (gw-gox-4%win->charspace) *w = *w+1;
    if (gh-goy-4%win->charspace) *h = *h+1;

}

void tabbarsizg(FILE* f, pa_tabori tor, int cw, int ch, int* w, int* h,
                int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarsizg(win, tor, cw, ch, w, h, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

void tabbarsiz(FILE* f, pa_tabori tor, int cw, int ch, int* w, int* h,
               int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarsiz(win, tor, cw, ch, w, h, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Find client from tabbar size

Given a tabbar size and orientation, this routine gives the client size and
offset. This is used where the tabbar size is fixed, but the client area is
flexible.

*******************************************************************************/

void itabbarclientg(winptr win, pa_tabori tor, int w, int h, int* cw, int* ch,
                    int* ox, int* oy)

{

    if (tor == pa_toright || tor == pa_toleft)  { /* vertical bar */

        /* Find client height and width from total height and width minus
         tabbar overhead. */
        *cw = w-32;
        *ch = h-8;
        if (tor == pa_toleft) {

            *ox = 28; /* set offsets */
            *oy = 4;

        } else {

            *ox = 4; /* set offsets */
            *oy = 4;

        }

    } else { /* horizontal bar */

        /* Find client height and width from total height and width minus
           tabbar overhead. */
        *cw = w-8;
        *ch = h-32;
        if (tor == pa_totop) {

            *ox = 4; /* set offsets */
            *oy = 28;

        } else {

            *ox = 4; /* set offsets */
            *oy = 4;

        }

    }

}

void itabbarclient(winptr win, pa_tabori tor, int w, int h, int* cw, int* ch,
                   int* ox, int* oy)

{

    int gw, gh, gox, goy;

    /* convert sizes to graphical */
    w = w*win->charspace;
    h = h*win->linespace;
    itabbarsizg(win, tor, w, h, &gw, &gh, &gox, &goy); /* get size */
    /* change graphical size to character */
    *cw = (gw-1)/win->charspace+1;
    *ch = (gh-1)/win->linespace+1;
    *ox = (gox-1)/win->charspace+1;
    *oy = (goy-1)/win->linespace+1;
    /* must make sure that client dosen"t intrude on edges */
    if (gw-gox-4%win->charspace) w = w+1;
    if (gh-goy-4%win->charspace) h = h+1;

}

void tabbarclientg(FILE* f, pa_tabori tor, int w, int h, int*  cw, int* ch,
                   int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarclientg(win, tor, w, h, cw, ch, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

void tabbarclient(FILE* f, pa_tabori tor, int w, int h, int* cw, int* ch,
                  int* ox, int* oy)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarclient(win, tor, w, h, cw, ch, ox, oy); /* get size */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Create tab bar

Creates a tab bar with the given orientation.

Bug: has strange overwrite mode where when the widget is first created, it
allows itself to be overwritten by the main window. This is worked around by
creating and distroying another widget.

*******************************************************************************/

void itabbarg(winptr win, int x1, int y1, int x2, int y2, pa_strptr sp,
              pa_tabori tor, int id)

{

    wigptr  wp;  /* widget pointer */
    int     inx; /* index for tabs */
    TCITEM  tcr; /* tab attributes record */
    int     i;   /* idnex for string */
    int     m;   /* maximum length of string */
    int     fl;  /* flags */
    LRESULT r;

    if (!win->visible) winvis(win); /* make sure we are displayed */
    fl = 0; /* clear parameter flags */
    if (tor == pa_toright || tor == pa_toleft) fl |= TCS_VERTICAL;
    if (tor == pa_toright) fl |= TCS_RIGHT;
    if (tor == pa_tobottom) fl |= TCS_BOTTOM;
    widget(win, x1, y1, x2, y2, "", id, wttabbar, fl, &wp);
    inx = 0; /* set index */
    while (sp) { /* add strings to list */

        /* create a string buffer with space for terminating zero */
        tcr.mask = TCIF_TEXT; /* set record contains text label */
        tcr.dwState = 0; /* clear state */
        tcr.dwStateMask = 0; /* clear state mask */
        tcr.pszText = sp->str; /* place string */
        tcr.iImage = -1; /* no image */
        tcr.lParam = 0; /* no parameter */
        unlockmain(); /* end exclusive access */
        /* add string */
        r = SendMessage(wp->han, TCM_INSERTITEM, (WPARAM)inx, (LPARAM)&tcr);
        lockmain(); /* start exclusive access */
        if (r == -1)  error(etabbar); /* can"t create tab */
        sp = sp->next; /* next string */
        inx++; /* next index */

    }

    uselesswidget(win); /* stop overwrite bug */

}

void itabbar(winptr win, int x1, int y1, int x2, int y2, pa_strptr sp,
             pa_tabori tor, int id)

{

    /* form graphical from character coordinates */
    x1 = (x1-1)*win->charspace+1;
    y1 = (y1-1)*win->linespace+1;
    x2 = (x2)*win->charspace;
    y2 = (y2)*win->linespace;
    itabbarg(win, x1, y1, x2, y2, sp, tor, id); /* create button graphical */

}

void tabbarg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, pa_tabori tor,
             int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbarg(win, x1, y1, x2, y2, sp, tor, id); /* execute */
    unlockmain(); /* end exclusive access */

}

void tabbar(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, pa_tabori tor,
            int id)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabbar(win, x1, y1, x2, y2, sp, tor, id); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

*******************************************************************************/

void itabsel(winptr win, int id, int tn)

{

    wigptr  wp; /* widget pointer */
    LRESULT r;

    if (!win->visible) winvis(win); /* make sure we are displayed */
    if (tn < 1) error(etabsel); /* bad tab select */
    wp = fndwig(win, id); /* find widget */
    if (!wp) error(ewignf); /* not found */
    /* set the range */
    unlockmain(); /* end exclusive access */
    r = SendMessage(wp->han, TCM_SETCURSEL, tn-1, 0);
    lockmain(); /* start exclusive access */

}

void tabsel(FILE* f, int id, int tn)

{

    winptr win; /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    itabsel(win, id, tn); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Output message dialog

Outputs a message dialog with the given title and message strings.

*******************************************************************************/

void alert(char* title, char* message)

{

    imptr ip; /* intratask message pointer */
    BOOL  b;  /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imalert; /* set is alert */
    ip->alttit = title; /* copy strings */
    ip->altmsg = message;
    b = PostMessage(dialogwin, UMIM, (WPARAM)ip, 0);
    if (!b) winerr(); /* process windows error */
    waitim(imalert, &ip); /* wait for the return */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose color dialog

Presents the choose color dialog,  returns the resulting color.

Bug: does not take the input color as the default.

*******************************************************************************/

void querycolor(int* r, int* g, int* b)

{

    imptr ip; /* intratask message pointer */
    BOOL  br; /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqcolor; /* set is color query */
    ip->clrred = *r; /* set colors */
    ip->clrgreen = *g;
    ip->clrblue = *b;
    br = PostMessage(dialogwin, UMIM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imqcolor, &ip); /* wait for the return */
    *r = ip->clrred; /* set new colors */
    *g = ip->clrgreen;
    *b = ip->clrblue;
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose file dialog for open

Presents the choose file dialog,  returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, a null string will be returned.

*******************************************************************************/

void queryopen(char* s)

{

    imptr ip; /* intratask message pointer */
    BOOL  br; /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqopen; /* set is open file query */
    ip->opnfil = s; /* set input string */
    br = PostMessage(dialogwin, UMIM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imqopen, &ip); /* wait for the return */
    s = ip->opnfil; /* set output string */
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose file dialog for save

Presents the choose file dialog,  returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled,  a null string will be returned.

*******************************************************************************/

void querysave(char* s)

{

    imptr ip; /* intratask message pointer */
    BOOL br;  /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqsave; /* set is open file query */
    ip->opnfil = s; /* set input string */
    br = PostMessage(dialogwin, UMIM, (WPARAM)ip, 0);
    if (!br)  winerr(); /* process windows error */
    waitim(imqsave, &ip); /* wait for the return */
    s = ip->savfil; /* set output string */
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose find text dialog

Presents the choose find text dialog,  returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: should return null string on cancel. Unlike other dialogs, windows
provides no indication of if the cancel button was pushed. To do this, we
would need to hook (or subclass) the find dialog.

After note: tried hooking the window. The issue is that the cancel button is
just a simple button that gets pressed. Trying to rely on the button id
sounds very system dep}ent, since that could change. One method might be
to retrive the button text, but this is still fairly system dep}ent. We
table this issue until later.

*******************************************************************************/

void queryfind(char* s, int* opt)

{

    imptr ip;   /* intratask message pointer */
    BOOL  br; /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqfind; /* set is find query */
    ip->fndstr = s; /* set input string */
    ip->fndopt = *opt; /* set options */
    br = PostMessage(dialogwin, UMIM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imqfind, &ip); /* wait for the return */
    s = ip->fndstr; /* set output string */
    *opt = ip->fndopt; /* set output options */
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose replace text dialog

Presents the choose replace text dialog,  returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may ! be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: See comment, queryfind.

*******************************************************************************/

void queryfindrep(char* s, char* r, int* opt)

{

    imptr ip; /* intratask message pointer */
    BOOL  br; /* result */

    lockmain(); /* start exclusive access */
    getitm(&ip); /* get a im pointer */
    ip->im = imqfindrep; /* set is find/replace query */
    ip->fnrsch = s; /* set input find string */
    ip->fnrrep = r; /* set input replace string */
    ip->fnropt = *opt; /* set options */
    br = PostMessage(dialogwin, UMIM, (WPARAM)ip, 0);
    if (!br) winerr(); /* process windows error */
    waitim(imqfindrep, &ip); /* wait for the return */
    s = ip->fnrsch; /* set output find string */
    r = ip->fnrrep;
    *opt = ip->fnropt; /* set output options */
    putitm(ip); /* release im */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Display choose font dialog

Presents the choose font dialog,  returns the resulting logical font
number, size, foreground color, background color, and effects (in a special
effects set for this routine).

The parameters are "flow through", meaning that they should be set to their
defaults before the call, and changes are made,  updated to the parameters.
During the routine, the state of the parameters given are presented to the
user as the defaults.

*******************************************************************************/

/* find font number in fonts list */

int fndfntnum(winptr win, char* fns)

{

    fontptr fp;     /* pointer for fonts list */
    int     fc, ff; /* font counters */

    fp = win->fntlst; /* index top of fonts */
    fc = 1; /* set 1st font */
    ff = 0; /* set no font found */
    while (fp) { /* traverse */

        if (comps(fns, fp->fn)) ff = fc; /* found */
        fp = fp->next; /* next entry */
        fc++; /* count */

    }
    /* The font string should match one from the list, since that list was itself
       formed from the system font list. */
    if (!ff)  error(esystem); /* should have found matching font */

    return (ff); /* return font */

}

void iqueryfont(winptr win, int* fc, int* s, int* fr, int* fg, int* fb, int* br,
                int* bg, int* bb, int* effect)

{

    imptr ip;               /* intratask message pointer */
    BOOL  b;                /* result */
    char  fns[LF_FACESIZE]; /* name of font */

    getitm(&ip); /* get a im pointer */
    ip->im = imqfont; /* set is font query */
    ifontnam(win, *fc, fns, LF_FACESIZE); /* get the name of the font */
    ip->fntstr = fns; /* place in record */
    ip->fnteff = *effect; /* copy effects */
    ip->fntfr = *fr; /* copy colors */
    ip->fntfg = *fg;
    ip->fntfb = *fb;
    ip->fntbr = *br;
    ip->fntbg = *bg;
    ip->fntbb = *bb;
    ip->fntsiz = *s; /* place font size */
    /* send request */
    b = PostMessage(dialogwin, UMIM, (WPARAM)ip, 0);
    if (!b) winerr(); /* process windows error */
    waitim(imqfont, &ip); /* wait for the return */
    /* pull back the output parameters */
    *fc = fndfntnum(win, ip->fntstr); /* find font from list */
    *effect = ip->fnteff; /* effects */
    *fr = ip->fntfr; /* colors */
    *fg = ip->fntfg;
    *fb = ip->fntfb;
    *br = ip->fntbr;
    *bg = ip->fntbg;
    *bb = ip->fntbb;
    *s = ip->fntsiz; /* font size */
    putitm(ip); /* release im */

}

void queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb,
               int* br, int* bg, int* bb, int* effect)

{

    winptr win;  /* window context */

    lockmain(); /* start exclusive access */
    win = txt2win(f); /* get windows context */
    iqueryfont(win, fc, s, fr, fg, fb, br, bg, bb, effect); /* execute */
    unlockmain(); /* end exclusive access */

}

/*******************************************************************************

Window procedure for display thread

This is the window handler callback for all display windows.

*******************************************************************************/

LRESULT CALLBACK wndproc(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam)

{

    LRESULT r;   /* result holders */
    BOOL    b;
    int     ofn; /* output file handle */
    winptr  win; /* pointer to windows structure */
    imptr   ip;  /* intratask message pointer */
    int     udw; /* up/down control width */
    RECT    cr;  /* client rectangle */

/*prtstr("wndproc: msg: ", msgcnt); prtstr(" ");
;prtmsgu(hwnd, imsg, wparam, lparam);
;msgcnt++;*/
    if (imsg == WM_CREATE) {

        r = 0;

    } else if (imsg == WM_PAINT) {

        lockmain(); /* start exclusive access */
        /* get the logical output file from Windows handle */
        ofn = hwn2lfn(hwnd);
        if (ofn) { /* there is a window */

            win = lfn2win(ofn); /* index window from output file */
            if (win->bufmod) restore(win, FALSE); /* perform selective update */
            else { /* main task will handle it */

                /* get update region */
                b = GetUpdateRect(hwnd, &cr, FALSE);
                /* validate it so windows won"t send multiple notifications */
                b = ValidateRgn(hwnd, NULL); /* validate region */
                /* Pack the update region in the message. This limits the update
                   region to 16 bit coordinates. We need an im to fix this. */
                wparam = cr.left*0x10000+cr.top;
                lparam = cr.right*0x10000+cr.bottom;
                unlockmain(); /* end exclusive access */
                putmsg(hwnd, imsg, wparam, lparam); /* send message up */
                lockmain(); /* start exclusive access */

            }
            r = 0;

        } else r = DefWindowProc(hwnd, imsg, wparam, lparam);
        unlockmain(); /* end exclusive access */
        r = 0;

    } else if (imsg == WM_SETFOCUS)  {

        lockmain(); /* start exclusive access */
        /* get the logical output file from Windows handle */
        ofn = hwn2lfn(hwnd);
        if (ofn) { /* there is a window */

            win = lfn2win(ofn); /* index window from output file */
            /* activate caret */
            b = CreateCaret(win->winhan, 0, win->curspace, 3);
            /* set caret (text cursor) position at bottom of bounding box */
            b = SetCaretPos(win->screens[win->curdsp]->curxg-1,
                            win->screens[win->curdsp]->curyg-1+win->linespace-3);
            win->focus = TRUE; /* set screen in focus */
            curon(win); /* show the cursor */

        }
        unlockmain(); /* end exclusive access */
        putmsg(hwnd, imsg, wparam, lparam); /* copy to main thread */
        r = 0;

    } else if (imsg == WM_KILLFOCUS) {

        lockmain(); /* start exclusive access */
        /* get the logical output file from Windows handle */
        ofn = hwn2lfn(hwnd);
        if (ofn) { /* there is a window */

           win = lfn2win(ofn); /* index window from output file */
           win->focus = FALSE; /* set screen ! in focus */
           curoff(win); /* hide the cursor */
           b = DestroyCaret(); /* remove text cursor */

        }
        unlockmain(); /* end exclusive access */
        putmsg(hwnd, imsg, wparam, lparam); /* copy to main thread */
        r = 0;

    } else if (imsg == UMMAKWIN) { /* create standard window */

        /* create the window */
        stdwinwin = CreateWindow("StdWin", pgmnam, stdwinflg,
                                  stdwinx, stdwiny, stdwinw, stdwinh,
                                  stdwinpar, 0, GetModuleHandle(NULL), NULL);

        stdwinj1c = FALSE; /* set no joysticks */
        stdwinj2c = FALSE;
        if (JOYENB) {

            r = joySetCapture(stdwinwin, JOYSTICKID1, 33, FALSE);
            stdwinj1c = r == 0; /* set joystick 1 was captured */
            r = joySetCapture(stdwinwin, JOYSTICKID2, 33, FALSE);
            stdwinj2c = r == 0; /* set joystick 1 was captured */

        }

        /* signal we started window */
        iputmsg(0, UMWINSTR, 0, 0);
        r = 0;

    } else if (imsg == UMCLSWIN) { /* close standard window */

        b = DestroyWindow(stdwinwin); /* remove window from screen */

        /* signal we closed window */
        iputmsg(0, UMWINCLS, 0, 0);
        r = 0;

    } else if (imsg == WM_ERASEBKGND) {

        /* Letting windows erase the background is ! good, because it flashes, and
           is redundant in any case, because we handle that. */
        r = 1; /* say we are handling the erase */

    } else if (imsg == WM_CLOSE) {

        /* we handle our own window close, so don"t pass this on */
        putmsg(0, imsg, wparam, lparam);
        r = 0;

    } else if (imsg == WM_DESTROY) {

        /* here"s a problem. Posting quit causes the thread/process to terminate,
           not just the window. MSDN says to quit only the main window, but what
           is the main window here ? We send our terminate message based on
           wm_quit, and the window, even the main, does get closed. The postquit
           message appears to only be for closing down the program, which our
           program does by exiting. */
        /* postquitmessage(0); */
        r = 0;

    } else if (imsg == WM_LBUTTONDOWN || imsg == WM_MBUTTONDOWN ||
               imsg == WM_RBUTTONDOWN) {

        /* Windows allows child windows to capture the focus, but they don"t
           give it up (its a feature). We get around this by  returning the
           focus back to any window that is clicked by the mouse, but does
           not have the focus. */
        SetFocus(hwnd);
        putmsg(hwnd, imsg, wparam, lparam);
        r = DefWindowProc(hwnd, imsg, wparam, lparam);

    } else if (imsg == UMIM) { /* intratask message */

        ip = (imptr)wparam; /* get im pointer */
        switch (ip->im) { /* im type */

            case imupdown: /* create up/down control */
                /* get width of up/down control (same as scroll arrow) */
                udw = GetSystemMetrics(SM_CXHSCROLL);
                ip->udbuddy =
                    CreateWindow("edit", "",
                                 WS_CHILD | WS_VISIBLE | WS_BORDER |
                                 ES_LEFT | ES_AUTOHSCROLL,
                                 ip->udx, ip->udy, ip->udcx-udw-1, ip->udcy,
                                 ip->udpar, (HMENU)ip->udid, ip->udinst, NULL);
                ip->udhan =
                    CreateUpDownControl(ip->udflg, ip->udx+ip->udcx-udw-2, ip->udy, udw,
                                        ip->udcy, ip->udpar, ip->udid, ip->udinst,
                                        ip->udbuddy, ip->udup, ip->udlow,
                                        ip->udpos);
                /* signal complete */
                iputmsg(0, UMIM, wparam, 0);
                break;

            case imwidget: /* create widget */
                /* start widget window */
                ip->wigwin = CreateWindow(ip->wigcls, ip->wigtxt, ip->wigflg,
                                          ip->wigx, ip->wigy, ip->wigw,
                                          ip->wigh, ip->wigpar, (HMENU)ip->wigid,
                                          ip->wigmod, NULL);
                /* signal we started widget */
                iputmsg(0, UMIM, wparam, 0);
                break;

        }
        r = 0; /* clear result */

    } else { /* default handling */

        /* Copy messages we are interested in to the main thread. By keeping the
           messages passed down to only the interesting ones, we help prevent
           queue "flooding". This is done with a case, and ! a set, because sets
           are limited to 256 elements. */
        switch (imsg) {

            case WM_PAINT: case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_MBUTTONDOWN:
            case WM_MBUTTONUP: case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_SIZE:
            case WM_CHAR: case WM_KEYDOWN: case WM_KEYUP: case WM_QUIT: case WM_CLOSE:
            case WM_MOUSEMOVE: case WM_TIMER: case WM_COMMAND: case WM_VSCROLL:
            case WM_HSCROLL: case WM_NOTIFY:

/*prtstr("wndproc: passed to main: msg: ", msgcnt); prtstr(" ");
;prtmsgu(hwnd, imsg, wparam, lparam);
;msgcnt = msgcnt+1;*/
                putmsg(hwnd, imsg, wparam, lparam);
                break;

        }
        r = DefWindowProc(hwnd, imsg, wparam, lparam);

    }

    return (r);

}

/*******************************************************************************

Create dummy window

Create window to pass messages only. The window will have no display.

*******************************************************************************/

void createdummy(WNDPROC wndproc, char* name, HWND* dummywin)

{

    WNDCLASS wc; /* windows class structure */
    ATOM     b;  /* int return */

    /* create dummy class for message handling */
    wc.style         = 0;
    wc.lpfnWndProc   = wndproc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = 0;
    wc.hCursor       = 0;
    wc.hbrBackground = 0;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = str(name);
    /* register that class */
    b = RegisterClass(&wc);
    /* create the window */
    *dummywin =
        CreateWindow(name, "", 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                     GetModuleHandle(NULL), NULL);

}

/*******************************************************************************

Window display thread

Handles the actual display of all windows and input queues associated with
them. We create a dummy window to allow "headless" communication with the
thread, but any number of subwindows will be started in the thread.

*******************************************************************************/

DWORD WINAPI dispthread(LPVOID lpParameter)

{

    MSG     msg;
    BOOL    b;   /* int return */
    LRESULT r;   /* result holder */

    /* create dummy window for message handling */
    createdummy(wndproc, "dispthread", &dispwin);

    b = SetEvent(threadstart); /* flag subthread has started up */

    /* message handling loop */
    while (GetMessage(&msg, 0, 0, 0)) { /* not a quit message */

        b = TranslateMessage(&msg); /* translate keyboard events */
        r = DispatchMessage(&msg);

    }

}

/*******************************************************************************

Window procedure

This is the event handler for the main thread. It"s a dummy, and we handle
everything by sending it on.

*******************************************************************************/

LRESULT CALLBACK wndprocmain(HWND hwnd, UINT imsg, WPARAM wparam,
                                 LPARAM lparam)

{

    LRESULT r; /* result holder */

/*;prtstr("wndprocmain: msg: ");
;prtmsgu(hwnd, imsg, wparam, lparam);*/
    if (imsg == WM_CREATE) {

        r = 0;

    } else if (imsg == WM_DESTROY) {

        PostQuitMessage(0);
        r = 0;

    } else r = DefWindowProc(hwnd, imsg, wparam, lparam);

    return (r);

}

/*******************************************************************************

Dialog query window procedure

This message handler is to allow is to fix certain features of dialogs, like
the fact that they come up behind the main window.

*******************************************************************************/

/* Note: The Microsoft online description does not use APIENTRY, this is in
   MINGW */
UINT_PTR APIENTRY wndprocfix(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam)

{

    BOOL b; /* return value */

/*;prtstr("wndprocfix: msg: ");
;prtmsgu(hwnd, imsg, wparam, lparam);*/

    /* If dialog is focused, send it to the foreground. This solves the issue
       where dialogs are getting parked behind the main window. */
    if (imsg == WM_SETFOCUS) b = SetForegroundWindow(hwnd);

    return (0); /* tell callback to handle own messages */

}

/*******************************************************************************

Dialog procedure

Runs the various dialogs.

*******************************************************************************/

LRESULT CALLBACK wndprocdialog(HWND hwnd, UINT imsg, WPARAM wparam, LPARAM lparam)

{

    DWORD        r;    /* result holder */
    imptr        ip;   /* intratask message pointer */
    CHOOSECOLOR  cr;   /* color select structure */
    BOOL         b;    /* result holder */
    int          i;    /* index for string */
    OPENFILENAME fr;   /* file select structure */
    char*        bs;   /* filename holder */
    FINDREPLACE* frrp; /* dialog control record for find/replace */
    char*        fs;   /* pointer to finder string */
    char*        rs;   /* pointer to replacement string */
    int          fl;   /* flags */
    CHOOSEFONT   fns;  /* font select structure */
    LOGFONT*     lf;   /* logical font structure */
    int          sl;
    int          fsl, rsl;

/*;prtstr("wndprocdialog: msg: ");
;prtmsgu(hwnd, imsg, wparam, lparam);
;prtstr("");*/
    if (imsg == WM_CREATE)  {

        r = 0;

    } else if (imsg == WM_DESTROY)  {

        PostQuitMessage(0);
        r = 0;

    } else if (imsg == UMIM) { /* intratask message */

        ip = (imptr)wparam; /* get im pointer */
        switch (ip->im) { /* im type */

            case imalert: /* it"s an alert */
                r = MessageBox(0, ip->altmsg, ip->alttit,
                               MB_OK | MB_SETFOREGROUND);
                /* signal complete */
                iputmsg(0, UMIM, wparam, 0);
                break;

            case imqcolor:
                /* set starting color */
                cr.rgbResult = rgb2win(ip->clrred, ip->clrgreen, ip->clrblue);
                cr.lStructSize = 9*4; /* set size */
                cr.hwndOwner = 0; /* set no owner */
                cr.hInstance = 0; /* no instance */
                /*??? cr.rgbResult = 0;*/ /* clear color */
                cr.lpCustColors = gcolorsav; /* set global color save */
                /* set display all colors, start with initalized color */
                cr.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_ENABLEHOOK;
                cr.lCustData = 0; /* no data */
                cr.lpfnHook = wndprocfix; /* hook to force front */
                cr.lpTemplateName = NULL; /* set no template name */
                b = ChooseColor(&cr); /* perform choose color */
                /* set resulting color */
                win2rgb(cr.rgbResult, &ip->clrred, &ip->clrgreen, &ip->clrblue);
                /* signal complete */
                iputmsg(0, UMIM, wparam, 0);
                break;

            case imqopen:
            case imqsave:
                /* find length of input string */
                if (ip->im == imqopen) sl = strlen(ip->opnfil)+1;
                else sl = strlen(ip->savfil)+1;
                if (sl < 256) sl = 256;
                bs = malloc(sl); /* get string buffer */
                if (!bs) error(enomem);
                strcpy(bs, ip->opnfil); /* copy input string to buffer */
                /* now index the temp buffer */
                if (ip->im == imqopen) ip->opnfil = bs;
                else ip->savfil = bs;
                fr.lStructSize = sizeof(OPENFILENAME); /* set size */
                fr.hwndOwner = 0;
                fr.hInstance = 0;
                fr.lpstrFilter = NULL;
                fr.lpstrCustomFilter = NULL;
                fr.nFilterIndex = 0;
                fr.lpstrFile = bs;
                fr.nMaxFile = sl;
                fr.lpstrFileTitle = NULL;
                fr.lpstrInitialDir = NULL;
                fr.lpstrTitle = NULL;
                fr.Flags = OFN_HIDEREADONLY | OFN_ENABLEHOOK;
                fr.nFileOffset = 0;
                fr.nFileExtension = 0;
                fr.lpstrDefExt = NULL;
                fr.lCustData = 0;
                fr.lpfnHook = wndprocfix; /* hook to force front */
                fr.lpTemplateName = NULL;
                fr.pvReserved = 0;
                fr.dwReserved = 0;
                fr.FlagsEx = 0;
                if (ip->im == imqopen)  /* it's open */
                    b = GetOpenFileName(&fr); /* perform choose file */
                else /* it's save */
                    b = GetSaveFileName(&fr); /* perform choose file */
                if (!b) {

                    /* Check was a cancel. If the user canceled, return a null
                       string. */
                    r = CommDlgExtendedError();
                    if (!r) error(efildlg); /* error */
                    /* Since the main code is expecting us to make a new string for
                       the result, we must copy the input to the output so that it"s
                       disposal will be valid. */
                    if (ip->im == imqopen) *(ip->opnfil) = 0; /* terminate string */
                    else *(ip->savfil) = 0;

                }
                /* signal complete */
                iputmsg(0, UMIM, wparam, 0);
                break;

            case imqfind:
                sl = strlen(ip->fndstr)+1; /* find length of input */
                /* must be >= 80 characters */
                if (sl < 80) sl = 80;
                bs = malloc(sl); /* get string buffer */
                if (!bs) error(enomem);
                strcpy(bs, ip->fndstr); /* copy input string to buffer */
                ip->fndstr = bs; /* index buffer for return */
                frrp = malloc(sizeof(FINDREPLACE)); /* get find/replace entry */
                if (!frrp) error(enomem);
                frrp->lStructSize = sizeof(FINDREPLACE); /* set size */
                frrp->hwndOwner = dialogwin; /* set owner */
                frrp->hInstance = 0; /* no instance */
                /* set flags */
                fl = FR_HIDEWHOLEWORD; /* or FR_ENABLEHOOK*/;
                /* set status of down */
                if (!(BIT(pa_qfnup) & ip->fndopt)) fl |= FR_DOWN;
                /* set status of case */
                if (BIT(pa_qfncase) & ip->fndopt) fl |= FR_MATCHCASE;
                frrp->Flags = fl;
                frrp->lpstrFindWhat = fs; /* place finder string */
                frrp->lpstrReplaceWith = NULL; /* set no replacement string */
                frrp->wFindWhatLen = sl; /* set length */
                frrp->wReplaceWithLen = 0; /* set null replacement string */
                frrp->lCustData = (LPARAM)ip; /* send ip with this record */
                frrp->lpfnHook = 0; /* no callback */
                frrp->lpTemplateName = NULL; /* set no template */
                /* start find dialog */
                fndrepmsg = RegisterWindowMessage("commdlg_FindReplace");
                ip->fndhan = FindText(frrp); /* perform dialog */
                /* now bring that to the front */
                b = SetWindowPos(ip->fndhan, HWND_TOP, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE);
                b = SetForegroundWindow(ip->fndhan);
                r = 0;
                break;

            case imqfindrep:
                /* find length of search string */
                fsl =strlen(ip->fnrsch);
                if (fsl < 80) fsl = 80; /* ensure >= 80 */
                fs = malloc(fsl);
                if (!fs) error(enomem);
                strcpy(fs, ip->fnrsch);
                /* find length of replacement string */
                rsl =strlen(ip->fnrrep);
                if (rsl < 80) rsl = 80; /* ensure >= 80 */
                rs = malloc(rsl);
                if (!rs) error(enomem);
                strcpy(rs, ip->fnrrep);
                frrp = malloc(sizeof(FINDREPLACE)); /* get find/replace entry */
                if (!frrp) error(enomem);
                frrp->lStructSize = sizeof(FINDREPLACE); /* set size */
                frrp->hwndOwner = dialogwin; /* set owner */
                frrp->hInstance = 0; /* no instance */
                /* set flags */
                fl = FR_HIDEWHOLEWORD;
                if (!(BIT(pa_qfrup) & ip->fnropt)) fl |= FR_DOWN; /* set status of down */
                if (BIT(pa_qfrcase) & ip->fnropt) fl |= FR_MATCHCASE; /* set status of case */
                frrp->Flags = fl;
                frrp->lpstrFindWhat = fs; /* place finder string */
                frrp->lpstrReplaceWith = rs; /* place replacement string */
                frrp->wFindWhatLen = fsl; /* set length */
                frrp->wReplaceWithLen = rsl; /* set null replacement string */
                frrp->lCustData = (LPARAM)ip; /* send ip with this record */
                frrp->lpfnHook = 0; /* clear these */
                frrp->lpTemplateName = NULL;
                /* start find dialog */
                fndrepmsg = RegisterWindowMessage("commdlg_FindReplace");
                ip->fnrhan = ReplaceText(frrp); /* perform dialog */
                /* now bring that to the front */
                b = SetWindowPos(ip->fnrhan, HWND_TOP, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE);
                b = SetForegroundWindow(ip->fnrhan);
                r = 0;
                break;

            case imqfont:
                lf = malloc(sizeof(LOGFONT)); /* get a logical font structure */
                if (!lf) error(enomem);
                /* initalize logical font structure */
                lf->lfHeight = ip->fntsiz; /* use default height */
                lf->lfWidth = 0; /* use default width */
                lf->lfEscapement = 0; /* no escapement */
                lf->lfOrientation = 0; /* orient to x axis */
                if (BIT(pa_qftebold) & ip->fnteff) lf->lfWeight = FW_BOLD; /* set bold */
                else lf->lfWeight = FW_DONTCARE; /* default weight */
                lf->lfItalic = BIT(pa_qfteitalic) & ip->fnteff;  /* italic */
                lf->lfUnderline = BIT(pa_qfteunderline) & ip->fnteff; /* underline */
                lf->lfStrikeOut = BIT(pa_qftestrikeout) & ip->fnteff; /* strikeout */
                lf->lfCharSet = DEFAULT_CHARSET; /* use default characters */
                /* use default precision */
                lf->lfOutPrecision = OUT_DEFAULT_PRECIS;
                /* use default clipping */
                lf->lfClipPrecision = CLIP_DEFAULT_PRECIS;
                lf->lfQuality = DEFAULT_QUALITY; /* use default quality */
                lf->lfPitchAndFamily = 0; /* must be zero */
                strncpy(lf->lfFaceName, ip->fntstr, 32); /* place face name */
                /* initalize choosefont structure */
                fns.lStructSize = sizeof(CHOOSEFONT); /* set size */
                fns.hwndOwner = 0; /* set no owner */
                fns.hDC = 0; /* no device context */
                fns.lpLogFont = lf; /* logical font */
                fns.iPointSize = 0; /* no point size */
                /* set flags */
                fns.Flags = CF_SCREENFONTS | CF_EFFECTS |
                            CF_NOSCRIPTSEL | CF_FORCEFONTEXIST |
                            CF_TTONLY | CF_INITTOLOGFONTSTRUCT |
                            CF_ENABLEHOOK;
                /* color */
                fns.rgbColors = rgb2win(ip->fntfr, ip->fntfg, ip->fntfb);
                fns.lCustData = 0; /* no data */
                fns.lpfnHook = wndprocfix; /* hook to force front */
                fns.lpTemplateName = NULL; /* no template name */
                fns.hInstance = 0; /* no instance */
                fns.lpszStyle = NULL; /* no style */
                fns.nFontType = 0; /* no font type */
                fns.nSizeMin = 0; /* no minimum size */
                fns.nSizeMax = 0; /* no maximum size */
                b = ChooseFont(&fns); /* perform choose font */
                if (!b) {

                    /* Check was a cancel. If the user canceled, just return the string
                       empty. */
                    r = CommDlgExtendedError();
                    if (r) error(efnddlg); /* error */
                    /* Since the main code is expecting us to make a new string for
                       the result, we must copy the input to the output so that it"s
                       disposal will be valid. */
                    ip->fntstr = malloc(1);
                    if (!ip->fntstr) error(enomem);
                    *(ip->fntstr) = 0;

                } else { /* set what the dialog changed */

                    ip->fnteff = 0; /* clear what was set */
                    if (lf->lfItalic) ip->fnteff |= BIT(pa_qfteitalic); /* italic */
                    else ip->fnteff &= ~BIT(pa_qfteitalic);
                    if (fns.nFontType & BOLD_FONTTYPE)
                        ip->fnteff |= BIT(pa_qftebold); /* bold */
                    else
                        ip->fnteff &= ~BIT(pa_qftebold);
                    if (lf->lfUnderline)
                        ip->fnteff |= BIT(pa_qfteunderline); /* underline */
                    else ip->fnteff &= ~BIT(pa_qfteunderline);
                    if (lf->lfStrikeOut)
                        ip->fnteff |= BIT(pa_qftestrikeout); /* strikeout */
                    else ip->fnteff &= ~BIT(pa_qftestrikeout);
                    /* place foreground colors */
                    win2rgb(fns.rgbColors, &ip->fntfr, &ip->fntfg, &ip->fntfb);
                    strncpy(ip->fntstr, lf->lfFaceName, 32); /* copy font string back */
                    ip->fntsiz = abs(lf->lfHeight); /* set size */

                }
                /* signal complete */
                iputmsg(0, UMIM, wparam, 0);
                break;

        }
        r = 0; /* clear result */

    } else if (imsg == fndrepmsg) { /* find is done */

        /* Here's a series of dirty tricks. The find/replace record pointer is given
           back to us as an int by windows. We also stored the ip as "customer
           data" in int. */
        frrp = (FINDREPLACEA*) lparam; /* get find/replace data pointer */
        ip = (imptr)frrp->lCustData; /* get im pointer */
        if (ip->im == imqfind) { /* it"s a find */

            b = DestroyWindow(ip->fndhan); /* destroy the dialog */
            /* check and set case match mode */
            if (frrp->Flags & FR_MATCHCASE) ip->fndopt |= BIT(pa_qfncase);
            /* check and set cas up/down mode */
            if (frrp->Flags & FR_DOWN) ip->fndopt &= ~BIT(pa_qfnup);
            else ip->fndopt |= BIT(pa_qfnup);
            ip->fndstr = frrp->lpstrFindWhat; /* place result string */

        } else { /* it's a find/replace */

            b = DestroyWindow(ip->fnrhan); /* destroy the dialog */
            /* check and set case match mode */
            if (frrp->Flags & FR_MATCHCASE) ip->fnropt |= BIT(pa_qfrcase);
            /* check and set find mode */
            if (frrp->Flags & FR_FINDNEXT) ip->fnropt |= BIT(pa_qfrfind);
            /* check and set replace mode */
            if (frrp->Flags & FR_REPLACE)
                ip->fnropt = ip->fnropt & ~(BIT(pa_qfrfind) & ~BIT(pa_qfrallfil));
            /* check and set replace all mode */
            if (frrp->Flags & FR_REPLACEALL)
                ip->fnropt = ip->fnropt & ~BIT(pa_qfrfind) | BIT(pa_qfrallfil);
            ip->fnrsch = frrp->lpstrFindWhat;
            ip->fnrrep = frrp->lpstrReplaceWith;

        }
        free(frrp); /* release find/replace entry */
        /* signal complete */
        iputmsg(0, UMIM, (WPARAM)ip, 0);

    } else r = DefWindowProc(hwnd, imsg, wparam, lparam);

    return (r);

}

/*******************************************************************************

Dialog thread

*******************************************************************************/

DWORD WINAPI dialogthread(LPVOID lpParameter)

{

    MSG     msg;
    BOOL    b;   /* int return */
    LRESULT r;   /* result holder */

    /* create dummy window for message handling */
    createdummy(wndprocdialog, "dialogthread", &dialogwin);

    b = SetEvent(threadstart); /* flag subthread has started up */

    /* message handling loop */
    while (GetMessage(&msg, 0, 0, 0)) { /* not a quit message */

        b = TranslateMessage(&msg); /* translate keyboard events */
        r = DispatchMessage(&msg);

    }

}

/*******************************************************************************

Open

We don't do anything for this, so pass it on.

*******************************************************************************/

static int iopen(const char* pathname, int flags, int perm)

{

    return (*ofpopen)(pathname, flags, perm);

}

/*******************************************************************************

Close

Does nothing but pass on.

*******************************************************************************/

static int iclose(int fd)

{

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    /* check if the file is an output window, and close if so */
    if (opnfil[fd] && opnfil[fd]->win) closewin(fd);
    return (*ofpclose)(fd);

}

/*******************************************************************************

Unlink

Unlink has nothing to do with us, so we just pass it on.

*******************************************************************************/

static int iunlink(const char* pathname)

{

    return (*ofpunlink)(pathname);

}

/*******************************************************************************

Lseek

Lseek is passed on.

*******************************************************************************/

static off_t ilseek(int fd, off_t offset, int whence)

{

    return (*ofplseek)(fd, offset, whence);

}

/*******************************************************************************

Read file

If the file is the stdin file, we process that by reading from the event queue
and returning any characters found. Any events besides character events are
discarded, which is why reading from the stdin file is a downward compatible
operation.

The input from user is line buffered and may be edited by the user.

All other files are passed on to the system level.

*******************************************************************************/

/* find window with non-zero input buffer */

static int fndful(int fd) /* output window file */

{

    int fi; /* file index */
    int ff; /* found file */

    ff = -1; /* set no file found */
    for (fi = 0; fi < MAXFIL; fi++) if (opnfil[fi]) {

        if (opnfil[fi]->inl == fd && opnfil[fi]->win != NULL)
            /* links the input file, and has a window */
            if (opnfil[fi]->win->inpend) ff = fi; /* found one */

    }

    return (ff); /* return result */

}

static ssize_t iread(int fd, void* buff, size_t count)

{

    int            l;   /* length left on destination */
    winptr         win; /* pointer to window data */
    int            ofn; /* output file handle */
    ssize_t        rc;  /* return code */
    unsigned char* ba;

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd] && opnfil[fd]->inw) { /* process input file */

        lockmain(); /* start exclusive access */
        ba = (unsigned char*)buff; /* index start of buffer */
        l = count; /* set length of destination */
        while (l > 0) { /* while there is space left in the buffer */
            /* read input bytes */

            /* find any window with a buffer with data that points to this input
               file */
            ofn = fndful(fd);
            if (ofn == -1) readline(fd); /* none, read a buffer */
            else { /* read characters */

                win = lfn2win(ofn); /* get the window */
                while (win->inpptr > 0 && l > 0) {

                    /* there is data in the buffer, and we need that data */
                    *ba = win->inpbuf[win->inpptr]; /* get and place next character */
                    if (win->inpptr < MAXLIN) win->inpptr++; /* next */
                    /* if we have just read the last of that line,  flag buffer
                       empty */
                    if (*ba == '\r')  {

                        win->inpptr = 0; /* set 1st character */
                        win->inpend = FALSE; /* set no ending */

                    }
                    l--; /* count characters */

                }

            }

      }
      rc = count; /* set all bytes read */
      unlockmain();/* end exclusive access */

    } else
        /* passdown */
        rc = (*ofpread)(fd, buff, count);

    return rc;

}

/*******************************************************************************

Write

If the file is the stdout file, we send that output to the screen, recognizing
any control characters.

All other files are passed on to the system level.

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    int     i;   /* index for destination */
    int     l;   /* length left on destination */
    winptr  win; /* pointer to window data */
    ssize_t rc;  /* return code */
    unsigned char* ba;

    if (fd < 0 || fd > MAXFIL)  error(einvhan); /* invalid file handle */
    if (opnfil[fd] && opnfil[fd]->win) { /* process window output file */

        lockmain(); /* start exclusive access */
        win = lfn2win(fd); /* index window */
        l = count; /* set length of source */
        ba = (unsigned char*)buff; /* index buffer */
        while (l > 0) { /* write output bytes */

            plcchr(win, *ba++); /* send character to terminal emulator */
            l--; /* count characters */

        }
        rc = count; /* set number of bytes written */
        unlockmain(); /* end exclusive access */

    } else /* standard file */
        rc = (*ofpwrite)(fd, buff, count);

    return (rc); /* return read count/error */

}

/*******************************************************************************

Graph startup

*******************************************************************************/

static void pa_init_graph (void) __attribute__((constructor (103)));
static void pa_init_graph()

{

    int i;
    int fi;

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_lseek(ilseek, &ofplseek);

    fend = FALSE; /* set no end of program ordered */
    fautohold = TRUE; /* set automatically hold self terminators */
    eqefre = NULL; /* set free event queuing list empty */
    dblflt = FALSE; /* set no double fault */
    wigfre = NULL; /* set free widget tracking list empty */
    freitm = NULL; /* clear intratask message free list */
    msgcnt = 1; /* clear message counter */

    /* Set up private message queuing */

    msginp = 1; /* clear message input queue */
    msgout = 1;
    msgrdy = CreateEvent(NULL, TRUE, FALSE, NULL); /* create message event */
    imsginp = 1; /* clear control message message input queue */
    imsgout = 1;
    imsgrdy = CreateEvent(NULL, TRUE, FALSE, NULL); /* create message event */
    InitializeCriticalSection(mainlock); /* initialize the sequencer lock */
    /* mainlock = createmutex(FALSE); */ /* create mutex with no owner */
    /* if mainlock == 0  winerr(); */ /* process windows error */
    fndrepmsg = 0; /* set no find/replace message active */
    for (i = 0; i < 16; i++) gcolorsav[i] = 0xffffff; /* set all to white */
    /* clear open files table */
    for (fi = 0; i < MAXFIL; i++) {

        opnfil[fi] = NULL; /* set unoccupied */
        /* clear top level translator table */
        xltfil[fi] = 0; /* set unoccupied */
        /* clear window logical number translator table */
        xltwin[fi] = 0; /* set unoccupied */
        /* clear file to window logical number translator table */
        filwin[fi] = 0; /* set unoccupied */

    }
    /* Create dummy window for message handling. This is only required so that
      the main thread can be attached to the display thread */
    createdummy(wndprocmain, "mainthread", &mainwin);
    mainthreadid = GetCurrentThreadId();

    getpgm(); /* get the program name from the command line */
    /* Now start the display thread, which both manages all displays, and sends us
      all messages from those displays. */
    threadstart = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!threadstart) winerr(); /* process windows error */
    /* create subthread */
    ResetEvent(threadstart); /* clear event */
    CreateThread(NULL, 0, dispthread, NULL, 0, &threadid);
    WaitForSingleObject(threadstart, -1); /* wait for thread to start */
    if (r == -1) winerr(); /* process windows error */
    /* Past this point, we need to lock for access between us and the thread. */

    /* Now attach the main thread to the display thread. This is required for the
      main thread to have access to items like the display window caret. */
    b = AttachThreadInput(mainthreadid, threadid, TRUE);
    if (!b) winerr(); /* process windows error */

    /* Start widget thread */
    ResetEvent(threadstart); /* clear event */
    CreateThread(NULL, 0, dialogthread, NULL, 0, &threadid);
    r = WaitForSingleObject(threadstart, -1); /* wait for thread to start */
    if (r == -1) winerr(); /* process windows error */

    /* register the stdwin class used to create all windows */
    regstd();

}

/*******************************************************************************

Graph shutdown

*******************************************************************************/

static void pa_deinit_graph (void) __attribute__((destructor (102)));
static void pa_deinit_graph(void)

{

    winptr wp;
    int    fi;

    lockmain(); /* start exclusive access */
    /* if the program tries to exit when the user has ! ordered an exit, it
       is assumed to be a windows "unaware" program. We stop before we exit
       these, so that their content may be viewed */
    if (!fend && fautohold) { /* process automatic exit sequence */

        /* See if output is open at all */
        if (opnfil[OUTFIL]) /* output is allocated */
            if (opnfil[OUTFIL]->win) {

            wp = opnfil[OUTFIL]->win;
            /* make sure we are displayed */
            if (!wp->visible) winvis(wp);
            /* If buffering is off, turn it back on. This will cause the screen
               to come up clear, but this is better than an unrefreshed "hole"
               in the screen. */
            if (!wp->bufmod) ibuffer(wp, TRUE);
            /* Check framed. We don"t want that off, either, since the user cannot
               close the window via the system close button. */
            if (!wp->frame) iframe(wp, TRUE);
            /* Same with system bar */
            if (!wp->sysbar) isysbar(wp, TRUE);
            /* change window label to alert user */
            unlockmain(); /* end exclusive access */
            b = SetWindowText(wp->winhan, trmnam);
            lockmain(); /* start exclusive access */
            /* wait for a formal end */
            while (!fend) ievent(INPFIL, &er);

        }

    }
    /* close any open windows */
    if (!dblflt)  { /* we haven"t already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files and windows */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->win)
                clswin(fi); /* close open window */

    }
    unlockmain(); /* end exclusive access */

}
