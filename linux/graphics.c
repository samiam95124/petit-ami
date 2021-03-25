/** ****************************************************************************
*                                                                              *
*                        GRAPHICAL MODE LIBRARY FOR X                          *
*                                                                              *
*                       Copyright (C) 2019 Scott A. Franco                     *
*                                                                              *
*                            2019/05/17 S. A. Franco                           *
*                                                                              *
* Implements the graphical mode functions on X. Gralib is upward               *
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
* Gralib started in 1996 as a graphical window demonstrator as a twin to       *
* ansilib, the ANSI control character based terminal mode library.             *
* In 2003, gralib was upgraded to the graphical terminal standard.             *
* In 2005, gralib was upgraded to include the window mangement calls, and the  *
* widget calls.                                                                *
*                                                                              *
* Gralib uses three different tasks. The main task is passed on to the         *
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
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
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

/* X11 definitions */
#include <X11/Xlib.h>
#include <X11/keysym.h>

/* whitebook definitions */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* linux definitions */
#include <sys/timerfd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

/* local definitions */
#include <localdefs.h>
#include <graph.h>

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); } while (0)

#define MAXTIM 10  /* maximum number of timers available */
#define MAXBUF 10  /* maximum number of buffers available */
#define IOWIN  1   /* logical window number of input/output pair */
#define MAXCON 10  /* number of screen contexts */
#define MAXTAB 50  /* total number of tabs possible per screen */
#define MAXPIC 50  /* total number of loadable pictures */
#define MAXLIN 250 /* maximum length of input bufferred line */
#define MAXFIL 100 /* maximum open files */

/* Default terminal size sets the geometry of the terminal */

#define DEFXD 80 /* default terminal size, 80x24, this is Linux standard */
#define DEFYD 24

/* file handle numbers at the system interface level */

#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */
#define ERRFIL 2 /* handle to standard error */

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

/* screen text attribute */
typedef enum {
    sablink,     /* blinking text (foreground) */
    sarev,       /* reverse video */
    saundl,      /* underline */
    sasuper,     /* superscript */
    sasubs,      /* subscripting */
    saital,      /* italic text */
    sabold,      /* bold text */
    sastkout,    /* strikeout text */
    sacondensed, /* condensed */
    saextended,  /* extended */
    saxlight,    /* extra light */
    salight,     /* light */
    saxbold,     /* bold */
    sahollow,    /* hollow */
    saraised     /* raised */
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
   both the menu handle and the relative number 0-n of the item, neither
   the lack of tree structure nor the order of the list matters. */
typedef struct metrec {

    struct metrec* next;   /* next entry */
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
    int            id;   /* logical id of widget */
    wigtyp         typ;  /* type of widget */
    int            siz;  /* size of slider in scroll widget, in windows terms */
    int            low;  /* low limit of up/down control */
    int            high; /* high limit of up/down control */
    int            enb;  /* widget is enabled */

} wigrec, *wigptr;

typedef struct scncon { /* screen context */

    /* fields used by graph module */
    int     lwidth;      /* width of lines */
    /* note that the pixel and character dimensions and positions are kept
      in parallel for both characters and pixels */
    int     maxx;        /* maximum characters in x */
    int     maxy;        /* maximum characters in y */
    int     maxxg;       /* maximum pixels in x */
    int     maxyg;       /* maximum pixels in y */
    int     curx;        /* current cursor location x */
    int     cury;        /* current cursor location y */
    int     curxg;       /* current cursor location in pixels x */
    int     curyg;       /* current cursor location in pixels y */
    int     fcrgb;       /* current writing foreground color in rgb */
    int     bcrgb;       /* current writing background color in rgb */
    mode    fmod;        /* foreground mix mode */
    mode    bmod;        /* background mix mode */
    fontptr cfont;       /* active font entry */
    int     cspc;        /* character spacing */
    int     lspc;        /* line spacing */
    int     attr;        /* set of active attributes */
    int     autof;       /* current status of scroll and wrap */
    int     tab[MAXTAB]; /* tabbing array */
    int     curv;        /* cursor visible */
    /* note that view offsets and scaling are experimental features */
    int     offx;        /* viewport offset x */
    int     offy;        /* viewport offset y */
    int     wextx;       /* window extent x */
    int     wexty;       /* window extent y */
    int     vextx;       /* viewpor extent x */
    int     vexty;       /* viewport extent y */

    /* fields used by graphics subsystem */


} scncon, *scnptr;

typedef struct pict { /* picture tracking record */

    int     sx;  /* size in x */
    int     sy;  /* size in y */

} pict;

/* window description */
typedef struct winrec {

    /* fields used by graph module */
    int          parlfn;          /* logical parent */
    scnptr       screens[MAXCON]; /* screen contexts array */
    int          curdsp;          /* index for current display screen */
    int          curupd;          /* index for current update screen */
    /* global sets. these are the global set parameters that apply to any new
      created screen buffer */
    int          gmaxx;           /* maximum x size */
    int          gmaxy;           /* maximum y size */
    int          gmaxxg;          /* size of client area in x */
    int          gmaxyg;          /* size of client area in y */
    int          gattr;           /* current attributes */
    int          gauto;           /* state of auto */
    int          gfcrgb;          /* foreground color in rgb */
    int          gbcrgb;          /* background color in rgb */
    int          gcurv;           /* state of cursor visible */
    fontptr      gcfont;          /* current font select */
    int          gfhigh;          /* current font height */
    mode         gfmod;           /* foreground mix mode */
    mode         gbmod;           /* background mix mode */
    int          goffx;           /* viewport offset x */
    int          goffy;           /* viewport offset y */
    int          gwextx;          /* window extent x */
    int          gwexty;          /* window extent y */
    int          gvextx;          /* viewpor extent x */
    int          gvexty;          /* viewport extent y */
    fontptr      fntlst;          /* list of windows fonts */
    int          fntcnt;          /* number of fonts in font list */
    int          termfnt;         /* terminal font number */
    int          bookfnt;         /* book font number */
    int          signfnt;         /* sign font number */
    int          techfnt;         /* technical font number */
    int          mb1;             /* mouse assert status button 1 */
    int          mb2;             /* mouse assert status button 2 */
    int          mb3;             /* mouse assert status button 3 */
    int          mpx, mpy;        /* mouse current position */
    int          mpxg, mpyg;      /* mouse current position graphical */
    int          nmb1;            /* new mouse assert status button 1 */
    int          nmb2;            /* new mouse assert status button 2 */
    int          nmb3;            /* new mouse assert status button 3 */
    int          nmpx, nmpy;      /* new mouse current position */
    int          nmpxg, nmpyg;    /* new mouse current position graphical */
    int          linespace;       /* line spacing in pixels */
    int          charspace;       /* character spacing in pixels */
    int          curspace;        /* size of cursor, in pixels */
    int          baseoff;         /* font baseline offset from top */
    int          shift;           /* state of shift key */
    int          cntrl;           /* state of control key */
    int          fcurdwn;         /* cursor on screen flag */
    int          numjoy;          /* number of joysticks found */
    int          joy1cap;         /* joystick 1 is captured */
    int          joy2cap;         /* joystick 2 is captured */
    int          joy1xs;          /* last joystick position 1x */
    int          joy1ys;          /* last joystick position 1y */
    int          joy1zs;          /* last joystick position 1z */
    int          joy2xs;          /* last joystick position 2x */
    int          joy2ys;          /* last joystick position 2y */
    int          joy2zs;          /* last joystick position 2z */
    int          shsize;          /* display screen size x in millimeters */
    int          svsize;          /* display screen size y in millimeters */
    int          shres;           /* display screen pixels in x */
    int          svres;           /* display screen pixels in y */
    int          sdpmx;           /* display screen find dots per meter x */
    int          sdpmy;           /* display screen find dots per meter y */
    char         inpbuf[MAXLIN];  /* input line buffer */
    int          inpptr;          /* input line index */
    int          frmrun;          /* framing timer is running */
    struct {

       int      rep; /* timer repeat flag */

    } timers[10];
    int          focus;           /* screen in focus */
    pict         pictbl[MAXPIC];  /* loadable pictures table */
    int          bufmod;          /* buffered screen mode */
    metptr       metlst;          /* menu tracking list */
    wigptr       wiglst;          /* widget tracking list */
    int          frame;           /* frame on/off */
    int          size;            /* size bars on/off */
    int          sysbar;          /* system bar on/off */
    int          sizests;         /* last resize status save */
    int          visible;         /* window is visible */

    /* fields used by graphics subsystem */
    Window       xwhan;           /* current window */
    GC           xcxt;            /* graphics context */
    XFontStruct* xfont;           /* current font */
    Pixmap       xscnbuf;         /* pixmap for screen backing buffer */

} winrec, *winptr;

/* File tracking.
  Files can be passthrough to the OS, or can be associated with a window. If
  on a window, they can be output, or they can be input. In the case of
  input, the file has its own input queue, and will receive input from all
  windows that are attached to it. */
typedef struct filrec {

      FILE* sfp;  /* file pointer used to establish entry, or NULL */
      winptr win; /* associated window (if exists) */
      int inw;    /* entry is input linked to window */
      int inl;    /* this output file is linked to the input file, logical */

} filrec, *filptr;

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
    einvfil,  /* File is invalid */
    enotinp,  /* not input side of any window */
    esystem   /* System consistency check */

} errcod;

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static punlink_t ofpunlink;
static plseek_t  ofplseek;

/* X Windows globals */

static int fend;      /* end of program ordered flag */
static int fautohold; /* automatic hold on exit flag */

/* X windows display characteristics.
 *
 * Note that some of these are going to need to move to a per-window structure.
 */

static Display*     padisplay;      /* current display */
static int          pascreen;       /* current screen */
static int          ctrll, ctrlr;   /* control key active */
static int          shiftl, shiftr; /* shift key active */
static int          altl, altr;     /* alt key active */
static int          capslock;       /* caps lock key active */
static XEvent       evtexp;         /* expose event record */
static filptr       opnfil[MAXFIL]; /* open files table */
static int          xltwin[MAXFIL]; /* window equivalence table */
static int          filwin[MAXFIL]; /* file to window equivalence table */

/** ****************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.

*******************************************************************************/

static void error(errcod e)

{

    fprintf(stderr, "*** Error: graphics: ");
    switch (e) { /* error */

      case eftbful:  fprintf(stderr, "Too many files"); break;
      case ejoyacc:  fprintf(stderr, "No joystick access available"); break;
      case etimacc:  fprintf(stderr, "No timer access available"); break;
      case einvhan:  fprintf(stderr, "Invalid file number"); break;
      case efilopr:  fprintf(stderr, "Cannot perform operation on special file"); break;
      case einvscn:  fprintf(stderr, "Invalid screen number"); break;
      case einvtab:  fprintf(stderr, "Tab position specified off screen"); break;
      case eatopos:  fprintf(stderr, "Cannot position text by pixel with auto on"); break;
      case eatocur:  fprintf(stderr, "Cannot position outside screen with auto on"); break;
      case eatoofg:  fprintf(stderr, "Cannot reenable auto off grid"); break;
      case eatoecb:  fprintf(stderr, "Cannot reenable auto outside screen"); break;
      case einvftn:  fprintf(stderr, "Invalid font number"); break;
      case etrmfnt:  fprintf(stderr, "No valid terminal font was found"); break;
      case eatofts:  fprintf(stderr, "Cannot resize font with auto enabled"); break;
      case eatoftc:  fprintf(stderr, "Cannot change fonts with auto enabled"); break;
      case einvfnm:  fprintf(stderr, "Invalid logical font number"); break;
      case efntemp:  fprintf(stderr, "Logical font number has no assigned font"); break;
      case etrmfts:  fprintf(stderr, "Cannot size terminal font"); break;
      case etabful:  fprintf(stderr, "Too many tabs set"); break;
      case eatotab:  fprintf(stderr, "Cannot set off grid tabs with auto on"); break;
      case estrinx:  fprintf(stderr, "String index out of range"); break;
      case epicfnf:  fprintf(stderr, "Picture file not found"); break;
      case epicftl:  fprintf(stderr, "Picture filename too large"); break;
      case etimnum:  fprintf(stderr, "Invalid timer number"); break;
      case ejstsys:  fprintf(stderr, "Cannot justify system font"); break;
      case efnotwin: fprintf(stderr, "File is not attached to a window"); break;
      case ewinuse:  fprintf(stderr, "Window id in use"); break;
      case efinuse:  fprintf(stderr, "File already in use"); break;
      case einmode:  fprintf(stderr, "Input side of window in wrong mode"); break;
      case edcrel:   fprintf(stderr, "Cannot release Windows device context"); break;
      case einvsiz:  fprintf(stderr, "Invalid buffer size"); break;
      case ebufoff:  fprintf(stderr, "Buffered mode not enabled"); break;
      case edupmen:  fprintf(stderr, "Menu id was duplicated"); break;
      case emennf:   fprintf(stderr, "Menu id was not found"); break;
      case ewignf:   fprintf(stderr, "Widget id was not found"); break;
      case ewigdup:  fprintf(stderr, "Widget id was duplicated"); break;
      case einvspos: fprintf(stderr, "Invalid scroll bar slider position"); break;
      case einvssiz: fprintf(stderr, "Invalid scroll bar slider size"); break;
      case ectlfal:  fprintf(stderr, "Attempt to create control fails"); break;
      case eprgpos:  fprintf(stderr, "Invalid progress bar position"); break;
      case estrspc:  fprintf(stderr, "Out of string space"); break;
      case etabbar:  fprintf(stderr, "Unable to create tab in tab bar"); break;
      case efildlg:  fprintf(stderr, "Unable to create file dialog"); break;
      case efnddlg:  fprintf(stderr, "Unable to create find dialog"); break;
      case efntdlg:  fprintf(stderr, "Unable to create font dialog"); break;
      case efndstl:  fprintf(stderr, "Find/replace string too long"); break;
      case einvwin:  fprintf(stderr, "Invalid window number"); break;
      case einvjye:  fprintf(stderr, "Invalid joystick event"); break;
      case ejoyqry:  fprintf(stderr, "Could not get information on joystick"); break;
      case einvjoy:  fprintf(stderr, "Invalid joystick ID"); break;
      case eclsinw:  fprintf(stderr, "Cannot directly close input side of window"); break;
      case ewigsel:  fprintf(stderr, "Widget is not selectable"); break;
      case ewigptxt: fprintf(stderr, "Cannot put text in this widget"); break;
      case ewiggtxt: fprintf(stderr, "Cannot get text from this widget"); break;
      case ewigdis:  fprintf(stderr, "Cannot disable this widget"); break;
      case estrato:  fprintf(stderr, "Cannot direct write string with auto on"); break;
      case etabsel:  fprintf(stderr, "Invalid tab select"); break;
      case enomem:   fprintf(stderr, "Out of memory"); break;
      case einvfil:  fprintf(stderr, "File is invalid"); break;
      case enotinp:  fprintf(stderr, "Not input side of any window");
      case esystem:  fprintf(stderr, "System consistency check"); break;

    }
    fprintf(stderr, "\n");

    exit(1);

}

/******************************************************************************

Translate colors code

Translates an independent to a terminal specific primary color code for Windows.

******************************************************************************/

int colnum(pa_color c)

{

    int n;

    /* translate color number */
    switch (c) { /* color */

        case pa_black:     n = 0x000000; break;
        case pa_white:     n = 0xffffff; break;
        case pa_red:       n = 0xff0000; break;
        case pa_green:     n = 0x00ff00; break;
        case pa_blue:      n = 0x0000ff; break;
        case pa_cyan:      n = 0x00ffff; break;
        case pa_yellow:    n = 0xffff00; break;
        case pa_magenta:   n = 0xff00ff; break;
        case pa_backcolor: n = 0xeae9d8; break;

    }

    return (n); /* return number */

}

/** ****************************************************************************

Get file entry

Allocates and initalizes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

static void getfet(filptr* fp)

{

    *fp = malloc(sizeof(filrec)); /* get new file entry */
    if (!*fp) error(enomem); /* no more memory */
    (*fp)->win = NULL; /* set no window */
    (*fp)->inw = FALSE; /* clear input window link */
    (*fp)->inl = -1; /* set no input file linked */

}

/*******************************************************************************

Index window from logical file number

Finds the window associated with a logical file id. The file is checked
Finds the window associated with a text file. Gets the logical top level
filenumber for the file, converts this via its top to bottom alias,
validates that an alias has been established. This effectively means the file
was opened. , the window structure assigned to the file is fetched, and
validated. That means that the file was opened as a window input or output
file.

*******************************************************************************/

static winptr lfn2win(int fn)

{

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    if (!opnfil[fn]) error(einvhan); /* invalid handle */
    if (!opnfil[fn]->win)
        error(efnotwin); /* not a window file */

    return (opnfil[fn]->win); /* return windows pointer */

}

/*******************************************************************************

Index window from file

Finds the window associated with a text file. Gets the logical top level
filenumber for the file, converts this via its top to bottom alias,
validates that an alias has been established. This effectively means the file
was opened. , the window structure assigned to the file is fetched, and
validated. That means that the file was opened as a window input or output
file.

*******************************************************************************/

static winptr txt2win(FILE* f)

{

   int fn;

   fn = fileno(f); /* get file number */
   if (fn < 0) error(einvfil); /* file invalid */

   return (lfn2win(fn)); /* get logical filenumber for file */

}

/** ****************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

static int icurbnd(scnptr sc)

{

   return (sc->curx >= 1 && sc->curx <= sc->maxx &&
           sc->cury >= 1 && sc->cury <= sc->maxy);

}

/*******************************************************************************

Send expose event for current cursor character

Prepares an exposure event covering the rectangle of the current character at
the cursor. This is is necessary to keep the display and the buffer in sync.

*******************************************************************************/

static void curexp(winptr win)

{

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curupd-1]; /* index current screen */
    /* send exposure event back to window with mask over character */
    evtexp.xexpose.x = sc->curxg-1;
    evtexp.xexpose.y = sc->curyg-1;
    evtexp.xexpose.width = sc->curxg-1+win->charspace;
    evtexp.xexpose.height = sc->curyg-1+win->linespace;
    XSendEvent(padisplay, win->xwhan, FALSE, ExposureMask, &evtexp);

}

/*******************************************************************************

Draw reversing cursor

Draws a cursor rectangle in xor mode. This is used both to place and remove the
cursor.

*******************************************************************************/

static void curdrw(winptr win)

{

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curupd-1]; /* index current screen */
    XSetFunction(padisplay, win->xcxt, GXxor); /* set reverse */
    XFillRectangle(padisplay, win->xscnbuf, win->xcxt, sc->curxg, sc->curyg,
                   win->charspace, win->linespace);
    XSetFunction(padisplay, win->xcxt, GXcopy); /* set reverse */
    curexp(win); /* send expose event */

}

/*******************************************************************************

Set cursor visable

Makes the cursor visible.

*******************************************************************************/

static void curon(winptr win)

{

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curupd-1]; /* index current screen */
    if (!win->fcurdwn && win->screens[win->curdsp-1]->curv &&
        icurbnd(win->screens[win->curdsp-1]) && win->focus)  {

        /* cursor not already down, cursor visible, cursor in bounds, screen
           in focus */
        curdrw(win);
        win->fcurdwn = TRUE; /* set cursor on screen */

    }

}

/*******************************************************************************

Set cursor invisible

Makes the cursor invisible.

*******************************************************************************/

static void curoff(winptr win)

{

    int b;

    if (win->fcurdwn) { /* cursor visable */

        curdrw(win); /* remove cursor */
        win->fcurdwn = FALSE; /* set cursor not on screen */

    }

}

/*******************************************************************************

Set cursor status

Changes the current cursor status. If the cursor is out of bounds, or not
set as visible, it is set off. Otherwise, it is set on. Used to change status
of cursor after position and visible status events. Acts as a combination of
curon and curoff routines.

*******************************************************************************/

static void cursts(winptr win)

{

    int b;

    if (win->screens[win->curdsp-1]->curv &&
        icurbnd(win->screens[win->curdsp-1]) && win->focus) {

        /* cursor should be visible */
        if (!win->fcurdwn) { /* not already down */

            /* cursor not already down, cursor visible, cursor in bounds */
            curdrw(win); /* show cursor */
            win->fcurdwn = TRUE; /* set cursor on screen */

        }

    } else {

         /* cursor should not be visible */
        if (win->fcurdwn) { /* cursor visable */

            curdrw(win); /* remove cursor */
            win->fcurdwn = FALSE; /* set cursor not on screen */

        }

    }

}

/** ****************************************************************************

Restore screen

Updates all the buffer and screen parameters from the display screen to the
terminal.

*******************************************************************************/

static void restore(winptr win,   /* window to restore */
                    int    whole) /* whole or part window */

{

}

/** ****************************************************************************

Initalize screen

Clears all the parameters in the present screen context. Also, the backing
buffer bitmap is created and cleared to the present colors.

*******************************************************************************/

static void iniscn(winptr win, scnptr sc)

{

    int      i, x;
    int      r;

    sc->maxx = win->gmaxx; /* set character dimensions */
    sc->maxy = win->gmaxy;
    sc->maxxg = win->gmaxxg; /* set pixel dimensions */
    sc->maxyg = win->gmaxyg;
    sc->curx = 1; /* set cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    sc->fcrgb = win->gfcrgb; /* set colors and attributes */
    sc->bcrgb = win->gbcrgb;
    sc->attr = win->gattr;
    sc->autof = win->gauto; /* set auto scroll and wrap */
    sc->curv = win->gcurv; /* set cursor visibility */
    sc->lwidth = 1; /* set single pixel width */
    sc->cfont = win->gcfont; /* set current font */
    sc->fmod = win->gfmod; /* set mix modes */
    sc->bmod = win->gbmod;
    sc->offx = win->goffx; /* set viewport offset */
    sc->offy = win->goffy;
    sc->wextx = win->gwextx; /* set extents */
    sc->wexty = win->gwexty;
    sc->vextx = win->gvextx;
    sc->vexty = win->gvexty;
    /* set up tabbing to be on each 8th position */
    i = 9; /* set 1st tab position */
    x = 0; /* set 1st tab slot */
    while (i < sc->maxx && x < MAXTAB) {

        sc->tab[x] = (i-1)*win->charspace+1;  /* set tab */
        i = i+8; /* next tab */
        x = x+1;

    }

}

/** ****************************************************************************

Open and present window

Given a windows file id and an (optional) parent window file id opens and
presents the window associated with it. All of the screen buffer data is
cleared, and a single buffer assigned to the window.

*******************************************************************************/

static void opnwin(int fn, int pfn)

{

    int         r;    /* result holder */
    int         b;    /* int result holder */
    pa_evtrec   er;   /* event holding record */
    int         ti;   /* index for repeat array */
    int         pin;  /* index for loadable pictures array */
    int         si;   /* index for current display screen */
    winptr      win;  /* window pointer */
    winptr      pwin; /* parent window pointer */
    int         f;    /* window creation flags */
    int         depth;
    int         x, y;

    win = lfn2win(fn); /* get a pointer to the window */
    /* find parent */
    win->parlfn = pfn; /* set parent logical number */
    if (pfn >= 0) {

       pwin = lfn2win(pfn); /* index parent window */

    }
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
    win->fcurdwn = FALSE; /* set cursor is not down */
    win->focus = FALSE; /* set not in focus */
    win->joy1xs = 0; /* clear joystick saves */
    win->joy1ys = 0;
    win->joy1zs = 0;
    win->joy2xs = 0;
    win->joy2ys = 0;
    win->joy2zs = 0;
    win->numjoy = 0; /* set number of joysticks 0 */
    win->inpptr = -1; /* set buffer empty */
    win->frmrun = FALSE; /* set framing timer not running */
    win->bufmod = TRUE; /* set buffering on */
    win->metlst = NULL; /* clear menu tracking list */
    win->wiglst = NULL; /* clear widget list */
    win->frame = TRUE; /* set frame on */
    win->size = TRUE; /* set size bars on */
    win->sysbar = TRUE; /* set system bar on */
    win->sizests = 0; /* clear last size status word */
    /* clear timer repeat array */
    for (ti = 0; ti < 10; ti++) {

       win->timers[ti].rep = FALSE; /* set no repeat */

    }
    for (si = 0; si < MAXCON; si++) win->screens[si] = NULL;
    win->screens[0] = malloc(sizeof(scncon)); /* get the default screen */
    if (!win->screens[0]) error(enomem);
    win->curdsp = 1; /* set current display screen */
    win->curupd = 1; /* set current update screen */
    win->visible = FALSE; /* set not visible */

    /* get screen parameters */
    win->shsize = XDisplayWidthMM(padisplay, pascreen); /* size x in millimeters */
    win->svsize = XDisplayHeightMM(padisplay, pascreen); /* size y in millimeters */
    win->shres = DisplayWidth(padisplay, pascreen);
    win->svres = DisplayHeight(padisplay, pascreen);
    win->sdpmx = win->shres/win->shsize*1000; /* find dots per meter x */
    win->sdpmy = win->svres/win->svsize*1000; /* find dots per meter y */

#if 0
    dbg_printf(dlinfo, "Display width in pixels:  %d\n", win->shres);
    dbg_printf(dlinfo, "Display height in pixels: %d\n", win->svres);
    dbg_printf(dlinfo, "Display width in mm:      %d\n", win->shsize);
    dbg_printf(dlinfo, "Display height in mm:     %d\n", win->svsize);
#endif

    /* choose courier font based on dpi, this works best on a variety of
       display resolutions */
    win->xfont = XLoadQueryFont(padisplay,
        "-bitstream-courier 10 pitch-bold-r-normal--0-0-200-200-m-0-iso8859-1");
    if (!win->xfont) {

        fprintf(stderr, "*** No font ***\n");
        exit(1);

    }
    win->xcxt = XDefaultGC(padisplay, pascreen);
    XSetFont (padisplay, win->xcxt, win->xfont->fid);

#if 0
    dbg_printf(dlinfo, "Font min_bounds: lbearing: %d\n", win->xfont->min_bounds.lbearing);
    dbg_printf(dlinfo, "Font min_bounds: rbearing: %d\n", win->xfont->min_bounds.rbearing);
    dbg_printf(dlinfo, "Font min_bounds: width:    %d\n", win->xfont->min_bounds.width);
    dbg_printf(dlinfo, "Font min_bounds: ascent:   %d\n", win->xfont->min_bounds.ascent);
    dbg_printf(dlinfo, "Font min_bounds: descent:  %d\n", win->xfont->min_bounds.descent);

    dbg_printf(dlinfo, "Font max_bounds: lbearing: %d\n", win->xfont->max_bounds.lbearing);
    dbg_printf(dlinfo, "Font max_bounds: rbearing: %d\n", win->xfont->max_bounds.rbearing);
    dbg_printf(dlinfo, "Font max_bounds: width:    %d\n", win->xfont->max_bounds.width);
    dbg_printf(dlinfo, "Font max_bounds: ascent:   %d\n", win->xfont->max_bounds.ascent);
    dbg_printf(dlinfo, "Font max_bounds: descent:  %d\n", win->xfont->max_bounds.descent);
#endif

    /* find spacing in current font */
    win->charspace = win->xfont->max_bounds.width;
    win->linespace = win->xfont->max_bounds.ascent+win->xfont->max_bounds.descent;

    /* find base offset */
    win->baseoff = win->xfont->ascent;

    /* set buffer size required for character spacing at default character grid
       size */

    win->gmaxxg = DEFXD*win->charspace;
    win->gmaxyg = DEFYD*win->linespace;

    /* create our window */

    win->xwhan = XCreateSimpleWindow(padisplay, RootWindow(padisplay, pascreen),
                                        10, 10, win->gmaxxg, win->gmaxyg, 1,
                           BlackPixel(padisplay, pascreen),
                           WhitePixel(padisplay, pascreen));
    XSelectInput(padisplay, win->xwhan, ExposureMask | KeyPressMask | KeyReleaseMask);
    XMapWindow(padisplay, win->xwhan);

    /* set up pixmap backing buffer for text grid */
    depth = DefaultDepth(padisplay, pascreen);
    win->xscnbuf = XCreatePixmap(padisplay, win->xwhan, win->gmaxxg, win->gmaxyg, depth);
    XSetForeground(padisplay, win->xcxt, WhitePixel(padisplay, pascreen));
    XFillRectangle(padisplay, win->xscnbuf, win->xcxt, 0, 0, win->gmaxxg, win->gmaxyg);
    XSetForeground(padisplay, win->xcxt, BlackPixel(padisplay, pascreen));

    /* draw grid for character cell diagnosis */
#if 0
    XSetForeground(padisplay, win->xcxt, colnum(pa_cyan));
    for (y = 0; y < win->gmaxyg; y += win->linespace)
        XDrawLine(padisplay, win->xscnbuf, win->xcxt, 0, y, win->gmaxxg, y);
    for (x = 0; x < win->gmaxxg; x += win->charspace)
        XDrawLine(padisplay, win->xscnbuf, win->xcxt, x, 0, x, win->gmaxyg);
    XSetForeground(padisplay, win->xcxt, colnum(pa_black));
#endif

    /* reveal the background (diagnostic) */
#if 0
    XSetBackground(padisplay, win->xcxt, colnum(pa_yellow));
#endif

    /* set up global buffer parameters */
    win->gmaxx = DEFXD; /* character max dimensions */
    win->gmaxy = DEFYD;
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

}

/** ****************************************************************************

Open an input and output pair

Creates, opens and initializes an input and output pair of files.

*******************************************************************************/

static void openio(FILE* infile, FILE* outfile, int ifn, int ofn, int pfn,
                   int wid)

{

    /* if output was never opened, create it now */
    if (!opnfil[ofn]) getfet(&opnfil[ofn]);
    /* if input was never opened, create it now */
    if (!opnfil[ifn]) getfet(&opnfil[ifn]);
    opnfil[ofn]->inl = ifn; /* link output to input */
    opnfil[ifn]->inw = TRUE; /* set input is window handler */
    /* set file descriptor locations (note this is only really used for input
       files */
    opnfil[ifn]->sfp = infile;
    opnfil[ofn]->sfp = outfile;
    /* now see if it has a window attached */
    if (!opnfil[ofn]->win) {

        /* Haven't already started the main input/output window, so allocate
           and start that. We tolerate multiple opens to the output file. */
        opnfil[ofn]->win = malloc(sizeof(winrec));
        if (!opnfil[ofn]->win) error(enomem);
        opnwin(ofn, pfn); /* and start that up */

    }
    /* check if the window has been pinned to something else */
    if (xltwin[wid-1] >= 0 && xltwin[wid-1] != ofn) error(ewinuse); /* flag error */
    xltwin[wid-1] = ofn; /* pin the window to the output file */
    filwin[ofn] = wid;

}

/** ****************************************************************************

Internal versions of external interface routines

These routines both isolate lower level details from higher level details and
also use internal data structures as parameters. This allows upper level
to call down to common functions without having to find the internal data
structures each time from a file handle.

Each internal routine has the same name as an external routine (without
coining), but with a leading "i".

********************************************************************************

/** ****************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

void iclear(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    curoff(win); /* hide the cursor */
    sc->curx = 1; /* set cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    XSetForeground(padisplay, win->xcxt,
                   WhitePixel(padisplay, pascreen));
    XFillRectangle(padisplay, win->xwhan, win->xcxt, 0, 0,
                   win->gmaxxg, win->gmaxyg);
    XSetForeground(padisplay, win->xcxt,
                   BlackPixel(padisplay, pascreen));
    curon(win); /* show the cursor */

}

/** ****************************************************************************

Scroll screen

Scrolls the ANSI terminal screen by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source and destination rectangles, and use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

void iscrollg(winptr win, int x, int y)

{

    /* implement me */

}

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

void icursor(winptr win, int x, int y)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    curoff(win); /* hide the cursor */
    sc->cury = y; /* set new position */
    sc->curx = x;
    sc->curxg = (x-1)*win->charspace+1;
    sc->curyg = (y-1)*win->linespace+1;
    curon(win); /* show the cursor */

}

/** ****************************************************************************

Position cursor graphical

Moves the cursor to the specified x and y location in pixels.

*******************************************************************************/

void icursorg(winptr win, int x, int y)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    curoff(win); /* hide the cursor */
    sc->curyg = y; /* set new position */
    sc->curxg = x;
    sc->curx = x/win->charspace+1;
    sc->cury = y/win->linespace+1;
    curon(win); /* show the cursor */

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

static void ihome(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    curoff(win); /* hide the cursor */
    /* reset cursors */
    sc->curx = 1;
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    curon(win); /* show the cursor */

}

/** ****************************************************************************

Move cursor up internal

Moves the cursor position up one line. If the cursor is at screen top, and auto
is on, the screen is scrolled up, meaning that the screen contents are moved
down a line of text. If auto is off, the cursor can simply continue into
negative space as long as it stays within the bounds -INT_MAX to INT_MAX.

*******************************************************************************/

static void iup(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    curoff(win); /* hide the cursor */
    /* check not top of screen */
    if (sc->cury > 1) {

        sc->cury--; /* update position */
        sc->curyg -= win->linespace; /* go last character line */

    } else if (sc->autof)
        iscrollg(win, 0*win->charspace, -1*win->linespace); /* scroll up */
    /* check won't overflow */
    else if (sc->cury > -INT_MAX) {

        sc->cury--; /* set new position */
        sc->curyg -= win->linespace;

    }
    curon(win); /* show the cursor */

}

/** ****************************************************************************

Move cursor down internal

Moves the cursor position down one line. If the cursor is at screen bottom, and
auto is on, the screen is scrolled down, meaning that the screen contents are
moved up a line of text. If auto is off, the cursor can simply continue into
undrawn space as long as it stays within the bounds of -INT_MAX to INT_MAX.

*******************************************************************************/

static void idown(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    curoff(win); /* hide the cursor */
    /* check not bottom of screen */
    if (sc->cury < sc->maxy) {

        sc->cury++; /* update position */
        sc->curyg += win->linespace; /* move to next character line */

    } else if (sc->autof) iscrollg(win, 0*win->charspace, +1*win->linespace); /* scroll down */
    else if (sc->cury < INT_MAX) {

        sc->cury++; /* set new position */
        sc->curyg += win->linespace; /* move to next text line */

    }
    curon(win); /* show the cursor */

}

/** ****************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

static void ileft(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    curoff(win); /* hide the cursor */
    /* check not at extreme left */
    if (sc->curx > 1) {

        sc->curx--; /* update position */
        sc->curxg -= win->charspace; /* back one character */

    } else { /* wrap cursor motion */

        if (sc->autof) { /* autowrap is on */

            iup(win); /* move cursor up one line */
            sc->curx = sc->maxx; /* set cursor to extreme right */
            sc->curxg = sc->maxxg-win->charspace;

        } else {

            /* check won't overflow */
            if (sc->curx > -INT_MAX) {

                sc->curx--; /* update position */
                sc->curxg -= win->charspace;

            }

        }

    }
    curon(win); /* show the cursor */

}

/** ****************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

static void iright(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    curoff(win); /* hide the cursor */
    /* check not at extreme right */
    if (sc->curx < sc->maxx) {

        sc->curx++; /* update position */
        sc->curxg += win->charspace;

    } else { /* wrap cursor motion */

        if (sc->autof) { /* autowrap is on */

            idown(win); /* move cursor up one line */
            sc->curx = 1; /* set cursor to extreme left */
            sc->curxg = 1;

        /* check won't overflow */
        } else {

            if (sc->curx < INT_MAX) {

                sc->curx++; /* update position */
                sc->curxg += win->charspace;

            }

        }

    }
    curon(win); /* show the cursor */

}

/** ****************************************************************************

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

*******************************************************************************/

static void itab(winptr win)

{

    /* implement me */

}

/** ****************************************************************************

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

static void iauto(winptr win, int e)

{

    scnptr sc;

    sc = win->screens[win->curupd-1];
    /* check we are transitioning to auto mode */
    if (e) {

        /* check display is on grid and in bounds */
        if (sc->curxg-1%win->charspace) error(eatoofg);
        if (sc->curxg-1%win->charspace) error(eatoofg);
        if (!icurbnd(sc)) error(eatoecb);

    }
    sc->autof = e; /* set auto status */
    win->gauto = e;

}

/** ****************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attribute.

We handle some elementary control codes here, like newline, backspace and form
feed. However, the idea is not to provide a parallel set of screen controls.
That's what the API is for.

*******************************************************************************/

static void plcchr(winptr win, char c)

{

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curupd-1]; /* index current screen */
    if (c == '\r') {

        /* carriage return, position to extreme left */
        sc->curx = 1; /* set to extreme left */
        sc->curxg = 1;

    } else if (c == '\n') {

        sc->curx = 1; /* set to extreme left */
        sc->curxg = 1;
        idown(win); /* line feed, move down */

    } else if (c == '\b') ileft(win); /* back space, move left */
    else if (c == '\f') iclear(win); /* clear screen */
    else if (c == '\t') itab(win); /* process tab */
    /* only output visible characters */
    else if (c >= ' ' && c != 0x7f) {

        curoff(win); /* hide the cursor */

        /* place on buffer */
        XDrawImageString(padisplay, win->xscnbuf, win->xcxt,
                    sc->curxg-1, sc->curyg-1+win->baseoff, &c, 1);

        /* send exposure event back to window with mask over character */
        curexp(win);

        curon(win); /* show the cursor */

        /* advance to next character */
        iright(win);

    }

}

/** ****************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement stdio:

read
write
open
close
unlink
lseek

We use interdiction to filter standard I/O calls towards the terminal. The
0 (input) and 1 (output) files are interdicted. In ANSI terminal, we act as a
filter, so this does not change the user ability to redirect the file handles
elsewhere.

*******************************************************************************/

/** ****************************************************************************

Read

*******************************************************************************/

static ssize_t iread(int fd, void* buff, size_t count)

{

    return (*ofpread)(fd, buff, count);

}

/** ****************************************************************************

Write

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    ssize_t rc; /* return code */
    char*   p = (char *)buff;
    size_t  cnt = count;
    winptr  win; /* pointer to window data */

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd] && opnfil[fd]->win) { /* process window output file */

        win = opnfil[fd]->win; /* index window */
        /* send data to terminal */
        while (cnt--) plcchr(win, *p++);
        rc = count; /* set return same as count */

    } else rc = (*ofpwrite)(fd, buff, count);

    return rc;

}

/** ****************************************************************************

Open

Terminal is assumed to be opened when the system starts, and closed when it
shuts down. Thus we do nothing for this.

*******************************************************************************/

static int iopen(const char* pathname, int flags, int perm)

{

    return (*ofpopen)(pathname, flags, perm);

}

/** ****************************************************************************

Close

Does nothing but pass on.

*******************************************************************************/

static int iclose(int fd)

{

    return (*ofpclose)(fd);

}

/** ****************************************************************************

Unlink

Unlink has nothing to do with us, so we just pass it on.

*******************************************************************************/

static int iunlink(const char* pathname)

{

    return (*ofpunlink)(pathname);

}

/** ****************************************************************************

Lseek

Lseek is never possible on a terminal, so this is always an error on the stdin
or stdout handle.

*******************************************************************************/

static off_t ilseek(int fd, off_t offset, int whence)

{

    /* check seeking on terminal attached file (input or output) and error
       if so */
    if (fd == INPFIL || fd == OUTFIL) error(efilopr);

    return (*ofplseek)(fd, offset, whence);

}

/** ****************************************************************************

External interface routines

*******************************************************************************/

/** ****************************************************************************

Scroll screen

Scrolls the ANSI terminal screen by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source and destination rectangles, and use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

void pa_scrollg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iscrollg(win, x, y); /* process */

}

void pa_scroll(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    iscrollg(win, x*win->charspace, y*win->linespace); /* process scroll */

}

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

void pa_cursor(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    icursor(win, x, y); /* process */

}

/** ****************************************************************************

Position cursor graphical

Moves the cursor to the specified x and y location in pixels.

*******************************************************************************/

void pa_cursorg(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    icursorg(win, x, y); /* process */

}

/** ****************************************************************************

Find character baseline

Returns the offset, from the top of the current fonts character bounding box,
to the font baseline. The baseline is the line all characters rest on.

*******************************************************************************/

int pa_baseline(FILE* f)

{

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int pa_maxx(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxx);

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int pa_maxy(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxx);

}

/** ****************************************************************************

Return maximum x dimension graphical

Returns the maximum x dimension, which is the width of the client surface in
pixels.

*******************************************************************************/

int pa_maxxg(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxxg);

}

/** ****************************************************************************

Return maximum y dimension graphical

Returns the maximum y dimension, which is the height of the client surface in
pixels.

*******************************************************************************/

int pa_maxyg(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->gmaxyg);

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void pa_home(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    ihome(win); /* process */

}

/** ****************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

void pa_up(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iup(win); /* process */

}

/** ****************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

void pa_down(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    idown(win); /* process */

}

/** ****************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

void pa_left(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    ileft(win); /* process */

}

/** ****************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

void pa_right(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iright(win); /* process */

}

/** ****************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

Graphical mode does not implement blink mode.

*******************************************************************************/

void pa_blink(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void pa_reverse(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.
This is not implemented, but could be done by drawing a line under each
character drawn.

*******************************************************************************/

void pa_underline(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_superscript(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_subscript(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

Italic is causing problems with fixed mode on some fonts, and Windows does not
seem to want to share with me just what the true width of an italic font is
(without taking heroic measures like drawing and testing pixels). So we disable
italic on fixed fonts.

*******************************************************************************/

void pa_italic(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void pa_bold(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

void pa_strikeout(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_standout(FILE* f, int e)

{

}

/** ****************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

*******************************************************************************/

void pa_fcolor(FILE* f, pa_color c)

{

    int rgb;
    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    rgb = colnum(c); /* translate color code to RGB */
    XSetForeground(padisplay, win->xcxt, rgb);

}

/** ****************************************************************************

Set foreground color

Sets the foreground color from individual r, g, b values.

*******************************************************************************/

void pa_fcolorc(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    XSetForeground(padisplay, win->xcxt, r<<16 | g<<8 | b);

}

/** ****************************************************************************

Set foreground color graphical

Sets the foreground color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

Fcolor exists as an overload to the text version, but we also provide an
fcolorg for backward compatiblity to the days before overloads.

*******************************************************************************/

void pa_fcolorg(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    XSetForeground(padisplay, win->xcxt, r<<16 | g<<8 | b);

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void pa_bcolor(FILE* f, pa_color c)

{

    int rgb;
    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    rgb = colnum(c); /* translate color code to RGB */
    XSetForeground(padisplay, win->xcxt, rgb);

}

void pa_bcolorc(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    XSetForeground(padisplay, win->xcxt,
                   r<<16 | g<<8 | b);

}

/** ****************************************************************************

Set background color graphical

Sets the background color from RGB primaries. The RGB values are scaled from
maxint, so 255 = maxint. This means that if the color resolution ever goes
up, we will be ready.

*******************************************************************************/

void pa_bcolorg(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    XSetForeground(padisplay, win->xcxt,
                   r<<16 | g<<8 | b);

}

/** ****************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

int pa_curbnd(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (icurbnd(win->screens[win->curupd-1]));

}

/** ****************************************************************************

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

void pa_auto(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    iauto(win, e); /* execute */

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void pa_curvis(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->screens[win->curupd-1]->curv = e; /* set cursor visible status */
    win->gcurv = e;
    cursts(win); /* process any cursor status change */

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int pa_curx(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->curx); /* process */

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int pa_cury(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->cury); /* process */

}

/** ****************************************************************************

Get location of cursor in x graphical

Returns the current location of the cursor in x, in pixels.

*******************************************************************************/

int pa_curxg(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->curxg); /* process */

}

/** ****************************************************************************

Get location of cursor in y graphical

Returns the current location of the cursor in y, in pixels.

*******************************************************************************/

int pa_curyg(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */

    return (win->screens[win->curupd-1]->curyg); /* return yg */

}

/** ****************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
then a new screen is allocated and cleared.
The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

*******************************************************************************/

void pa_select(FILE* f, int u, int d)

{

}

/** ****************************************************************************

Write string to current cursor position

Writes a string to the current cursor position, then updates the cursor
position. This acts as a series of write character calls. However, it eliminates
several layers of protocol, and results in much faster write time for
applications that require it.

It is an error to call this routine with auto enabled, since it could exceed
the bounds of the screen.

No control characters or other interpretation is done, and invisible characters
such as controls are not suppressed.

*******************************************************************************/

void pa_wrtstr(FILE* f, char* s)

{

}

/** ****************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void pa_del(FILE* f)

{

    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    ileft(win); /* back up cursor */
    plcchr(win, ' '); /* blank out */
    ileft(win); /* back up again */

}

/** ****************************************************************************

Draw line

Draws a single line in the foreground color.

*******************************************************************************/

void pa_line(FILE* f, int x1, int y1, int x2, int y2)

{

}

/** ****************************************************************************

Draw rectangle

Draws a rectangle in foreground color.

*******************************************************************************/

void pa_rect(FILE* f, int x1, int y1, int x2, int y2)

{

}

/** ****************************************************************************

Draw filled rectangle

Draws a filled rectangle in foreground color.

*******************************************************************************/

void pa_frect(FILE* f, int x1, int y1, int x2, int y2)

{

}

/** ****************************************************************************

Draw rounded rectangle

Draws a rounded rectangle in foreground color.

*******************************************************************************/

void pa_rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

}

/** ****************************************************************************

Draw filled rounded rectangle

Draws a filled rounded rectangle in foreground color.

*******************************************************************************/

void pa_frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

}

/** ****************************************************************************

Draw ellipse

Draws an ellipse with the current foreground color and line width.

*******************************************************************************/

void pa_ellipse(FILE* f, int x1, int y1, int x2, int y2)

{

}

/** ****************************************************************************

Draw filled ellipse

Draws a filled ellipse with the current foreground color.

*******************************************************************************/

void pa_fellipse(FILE* f, int x1, int y1, int x2, int y2)

{

}

/** ****************************************************************************

Draw arc

Draws an arc in the current foreground color and line width. The containing
rectangle of the ellipse is given, and the start and end angles clockwise from
0 degrees delimit the arc.

Windows takes the start and end delimited by a line extending from the center
of the arc. The way we do the convertion is to project the angle upon a circle
whose radius is the precision we wish to use for the calculation. Then that
point on the circle is found by triangulation.

The larger the circle of precision, the more angles can be represented, but
the trade off is that the circle must not reach the edge of an integer
(-maxint..maxint). That means that the total logical coordinate space must be
shortened by the precision. To find out what division of the circle precis
represents, use cd := precis*2*pi. So, for example, precis = 100 means 628
divisions of the circle.

The end and start points can be negative.
Note that Windows draws arcs counterclockwise, so our start and end points are
swapped.

Negative angles are allowed.

*******************************************************************************/

void pa_arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

}

/** ****************************************************************************

Draw filled arc

Draws a filled arc in the current foreground color. The same comments apply
as for the arc function above.

*******************************************************************************/

void pa_farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

}

/** ****************************************************************************

Draw filled cord

Draws a filled cord in the current foreground color. The same comments apply
as for the arc function above.

*******************************************************************************/

void pa_fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

}

/** ****************************************************************************

Draw filled triangle

Draws a filled triangle in the current foreground color.

*******************************************************************************/

void pa_ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3)

{

}

/** ****************************************************************************

Set pixel

Sets a single logical pixel to the foreground color.

*******************************************************************************/

void pa_setpixel(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Set foreground to overwrite

Sets the foreground write mode to overwrite.

*******************************************************************************/

void pa_fover(FILE* f)

{

}

/** ****************************************************************************

Set background to overwrite

Sets the background write mode to overwrite.

*******************************************************************************/

void pa_bover(FILE* f)

{

}

/** ****************************************************************************

Set foreground to invisible

Sets the foreground write mode to invisible.

*******************************************************************************/

void pa_finvis(FILE* f)

{

}

/** ****************************************************************************

Set background to invisible

Sets the background write mode to invisible.

*******************************************************************************/

void pa_binvis(FILE* f)

{

}

/** ****************************************************************************

Set foreground to xor

Sets the foreground write mode to xor.

*******************************************************************************/

void pa_fxor(FILE* f)

{

}

/** ****************************************************************************

Set background to xor

Sets the background write mode to xor.

*******************************************************************************/

void pa_bxor(FILE* f)

{

}

/** ****************************************************************************

Set line width

Sets the width of lines and several other figures.

*******************************************************************************/

void pa_linewidth(FILE* f, int w)

{

}

/** ****************************************************************************

Find character size x

Returns the character width.

*******************************************************************************/

int pa_chrsizx(FILE* f)

{

}

/** ****************************************************************************

Find character size y

Returns the character height.

*******************************************************************************/

int pa_chrsizy(FILE* f)

{

}

/** ****************************************************************************

Find number of installed fonts

Finds the total number of installed fonts.

*******************************************************************************/

int pa_fonts(FILE* f)

{

}

/** ****************************************************************************

Change fonts

Changes the current font to the indicated logical font number.

*******************************************************************************/

void pa_font(FILE* f, int fc)

{

}

/** ****************************************************************************

Find name of font

Returns the name of a font by number.

*******************************************************************************/

void pa_fontnam(FILE* f, int fc, char* fns, int fnsl)

{

}

/** ****************************************************************************

Change font size

Changes the font sizing to match the given character height. The character
and line spacing are changed, as well as the baseline.

*******************************************************************************/

void pa_fontsiz(FILE* f, int s)

{

}

/** ****************************************************************************

Find standard font numbers

Returns the font number of one of the standard font types, terminal, book,
sign or technical.

*******************************************************************************/

int pa_termfont(FILE* f)

{

    return 1;

}

int pa_bookfont(FILE* f)

{

    return 1;

}

int pa_signfont(FILE* f)

{

    return 1;

}

int pa_techfont(FILE* f)

{

    return 1;

}

/** ****************************************************************************

Set character extra spacing y

Sets the extra character space to be added between lines, also referred to
as "ledding".

Not implemented yet.

*******************************************************************************/

void pa_chrspcy(FILE* f, int s)

{

}

/** ****************************************************************************

Sets extra character space x

Sets the extra character space to be added between characters, referred to
as "spacing".

Not implemented yet.

*******************************************************************************/

void pa_chrspcx(FILE* f, int s)

{

}

/** ****************************************************************************

Find dots per meter x

Returns the number of dots per meter resolution in x.

*******************************************************************************/

int pa_dpmx(FILE* f)

{

}

/** ****************************************************************************

Find dots per meter y

Returns the number of dots per meter resolution in y.

*******************************************************************************/

int pa_dpmy(FILE* f)

{

}

/** ****************************************************************************

Find string size in pixels

Returns the number of pixels wide the given string would be, considering
character spacing and kerning.

*******************************************************************************/

int pa_strsiz(FILE* f, const char* s)

{

}

/** ****************************************************************************

Find character position in string

Finds the pixel offset to the given character in the string.

*******************************************************************************/

int pa_chrpos(FILE* f, const char* s, int p)

{

}

/** ****************************************************************************

Write justified text

Writes a string of text with justification. The string and the width in pixels
is specified. Auto mode cannot be on for this function, nor can it be used on
the system font.

*******************************************************************************/

void pa_writejust(FILE* f, const char* s, int n)

{

}

/** ****************************************************************************

Find justified character position

Given a string, a character position in that string, and the total length
of the string in pixels, returns the offset in pixels from the start of the
string to the given character, with justification taken into account.
The model used is that the extra space needed is divided by the number of
spaces, with the fractional part lost.

*******************************************************************************/

int pa_justpos(FILE* f, const char* s, int p, int n)

{

}

/** ****************************************************************************

Turn on condensed attribute

Turns on/off the condensed attribute. Condensed is a character set with a
shorter baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void pa_condensed(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on extended attribute

Turns on/off the extended attribute. Extended is a character set with a
longer baseline than normal characters in the current font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void pa_extended(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on extra light attribute

Turns on/off the extra light attribute. Extra light is a character thiner than
light.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void pa_xlight(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on light attribute

Turns on/off the light attribute. Light is a character thiner than normal
characters in the currnet font.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void pa_light(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on extra bold attribute

Turns on/off the extra bold attribute. Extra bold is a character thicker than
bold.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void pa_xbold(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on hollow attribute

Turns on/off the hollow attribute. Hollow is an embossed or 3d effect that
makes the characters appear sunken into the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void pa_hollow(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on raised attribute

Turns on/off the raised attribute. Raised is an embossed or 3d effect that
makes the characters appear coming off the page.

Note that the attributes can only be set singly.

Not implemented yet.

*******************************************************************************/

void pa_raised(FILE* f, int e)

{

}

/** ****************************************************************************

Delete picture

Deletes a loaded picture.

*******************************************************************************/

void pa_delpict(FILE* f, int p)

{

}

/** ****************************************************************************

Load picture

Loads a picture into a slot of the loadable pictures array.

*******************************************************************************/

void pa_loadpict(FILE* f, int p, char* fn)

{

}

/** ****************************************************************************

Find size x of picture

Returns the size in x of the logical picture.

*******************************************************************************/

int pa_pictsizx(FILE* f, int p)

{

}

/** ****************************************************************************

Find size y of picture

Returns the size in y of the logical picture.

*******************************************************************************/

int pa_pictsizy(FILE* f, int p)

{

}

/** ****************************************************************************

Draw picture

Draws a picture from the given file to the rectangle. The picture is resized
to the size of the rectangle.

Images will be kept in a rotating cache to prevent repeating reloads.

*******************************************************************************/

void pa_picture(FILE* f, int p, int x1, int y1, int x2, int y2)

{

}

/** ****************************************************************************

Set viewport offset graphical

Sets the offset of the viewport in logical space, in pixels, anywhere from
-maxint to maxint.

*******************************************************************************/

void pa_viewoffg(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Set viewport scale

Sets the viewport scale in x and y. The scale is a real fraction between 0 and
1, with 1 being 1:1 scaling. Viewport scales are allways smaller than logical
scales, which means that there are more than one logical pixel to map to a
given physical pixel, but never the reverse. This means that pixels are lost
in going to the display, but the display never needs to interpolate pixels
from logical pixels.

Note:

Right now, symmetrical scaling (both x and y scales set the same) are all that
works completely, since we don't presently have a method to warp text to fit
a scaling process. However, this can be done by various means, including
painting into a buffer and transfering asymmetrically, or using outlines.

*******************************************************************************/

void pa_viewscale(FILE* f, float x, float y)

{

}

/** ****************************************************************************

Acquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle always used.

The event loop for X and the event loop for Petit-Ami are similar. Its not a
coincidence. I designed it after a description I read of the X system in 1997.
Our event loop here is like an event to event translation.

*******************************************************************************/

/* Find window file corresponding to event. Note this can be made a hash
   search if speed is required. */
static int fndevt(Window w)

{

    int fi; /* index for file table */
    int ff; /* found file */

    fi = 0; /* start index */
    ff = -1; /* set no file found */
    while (fi < MAXFIL) {

        if (opnfil[fi] && opnfil[fi]->win && opnfil[fi]->win->xwhan == w) {

            ff = fi; /* set found */
            fi = MAXFIL; /* terminate */

        } else fi++; /* next entry */

    }
    if (ff < 0) error(enotinp); /* no corresponding window file found */

    return (ff);

}

void pa_event(FILE* f, pa_evtrec* er)

{

    XEvent e;
    int evtfnd;
    KeySym ks;
    int esck;
    winptr win; /* window record pointer */
    int ofn; /* output lfn associated with window */

    evtfnd = FALSE;
    esck = FALSE; /* set no previous escape */
    do {

        XNextEvent(padisplay, &e); /* get next event */
        ofn = fndevt(e.xany.window); /* get output window lfn */
        win = lfn2win(ofn); /* get window for that */
        er->winid = filwin[ofn]; /* get window number */
        if (e.type == Expose) {

            XCopyArea(padisplay, win->xscnbuf, win->xwhan, win->xcxt, 0, 0,
                      win->gmaxxg, win->gmaxyg, 0, 0);

        } else if (e.type == KeyPress) {

            ks = XLookupKeysym(&e.xkey, 0);
            er->etype = pa_etchar; /* place default code */
            if (ks >= ' ' && ks <= 0x7e && !ctrll && !ctrlr && !altl && !altr) {

                /* issue standard key event */
                er->etype = pa_etchar; /* set event */
                /* set code, normal or shifted */
                if (shiftl || shiftr) er->echar = !capslock?toupper(ks):ks;
                else er->echar = capslock?toupper(ks):ks; /* set code */
                evtfnd = TRUE; /* set found */

            } else {

                switch (ks) {

                    /* process control characters */
                    case XK_BackSpace: er->etype = pa_etdelcb; break;
                    case XK_Tab:       er->etype = pa_ettab; break;
                    case XK_Return:    er->etype = pa_etenter; break;
                    case XK_Escape:    if (esck)
                                           { er->etype = pa_etcan; esck = FALSE; }
                                       else esck = TRUE;
                                       break;
                    case XK_Delete:    if (shiftl || shiftr) er->etype = pa_etdel;
                                       else if (ctrll || ctrlr) er->etype = pa_etdell;
                                       else er->etype = pa_etdelcf;
                                       break;

                    case XK_Home:      if (ctrll || ctrlr) er->etype = pa_ethome;
                                       else er->etype = pa_ethomel;
                                       break;
                    case XK_Left:      if (ctrll || ctrlr) er->etype = pa_etleftw;
                                       else er->etype = pa_etleft;
                                       break;
                    case XK_Up:        if (ctrll || ctrlr) er->etype = pa_etscru;
                                       else er->etype = pa_etup;
                                       break;
                    case XK_Right:     if (ctrll || ctrlr) er->etype = pa_etrightw;
                                       else er->etype = pa_etright; break;
                    case XK_Down:      if (ctrll || ctrlr) er->etype = pa_etscrd;
                                       else er->etype = pa_etdown;
                                       break;
                    case XK_Page_Up:   if (ctrll || ctrlr) er->etype = pa_etscrl;
                                       else er->etype = pa_etpagu;
                                       break;
                    case XK_Page_Down: if (ctrll || ctrlr) er->etype = pa_etscrr;
                                       else er->etype = pa_etpagd;
                                       break;
                    case XK_End:       if (ctrll || ctrlr) er->etype = pa_etend;
                                       else er->etype = pa_etendl;
                                       break;

                    case XK_Insert:    er->etype = pa_etinsertt; break;

                    case XK_F1:
                    case XK_F2:
                    case XK_F3:
                    case XK_F4:
                    case XK_F5:
                    case XK_F6:
                    case XK_F7:
                    case XK_F8:
                    case XK_F9:
                    case XK_F10:
                    case XK_F11:
                    case XK_F12:
                        /* X11 gives us all 12 function keys for our use, plus
                           are sequential */
                        er->etype = pa_etfun; /* function key */
                        er->fkey = ks-XK_F1+1;
                        break;

                    case XK_C:
                    case XK_c:         if (ctrll || ctrlr) {

                    				       er->etype = pa_etterm;
                    				       fend = TRUE;

                                       }
                                       else if (altl || altr) er->etype = pa_etcopy;
                                       break;
                    case XK_S:
                    case XK_s:         if (ctrll || ctrlr)
                                           er->etype = pa_etstop;
                                       break;
                    case XK_Q:
                    case XK_q:         if (ctrll || ctrlr)
                                           er->etype = pa_etcont;
                                       break;
                    case XK_P:
                    case XK_p:         if (ctrll || ctrlr)
                                           er->etype = pa_etprint;
                                       break;
                    case XK_H:
                    case XK_h:         if (ctrll || ctrlr)
                                           er->etype = pa_ethomes;
                                       break;
                    case XK_E:
                    case XK_e:         if (ctrll || ctrlr)
                                           er->etype = pa_etends;
                                       break;
                    case XK_V:
                    case XK_v:         if (ctrll || ctrlr)
                                           er->etype = pa_etinsert;
                                       break;

                    case XK_Shift_L:   shiftl = TRUE; break; /* Left shift */
                    case XK_Shift_R:   shiftr = TRUE; break; /* Right shift */
                    case XK_Control_L: ctrll = TRUE; break;  /* Left control */
                    case XK_Control_R: ctrlr = TRUE; break;  /* Right control */
                    case XK_Alt_L:     altl = TRUE; break;  /* Left alt */
                    case XK_Alt_R:     altr = TRUE; break;  /* Right alt */
                    case XK_Caps_Lock: capslock = !capslock; /* Caps lock */

                }
                if (er->etype != pa_etchar)
                    evtfnd = TRUE; /* a control was found */

            }

        } else if (e.type == KeyRelease) {

            /* Petit-ami does not track key releases, but we need to account for
              control and shift keys up/down */
            ks = XLookupKeysym(&e.xkey, 0); /* find code */
            switch (ks) {

                case XK_Shift_L:   shiftl = FALSE; break; /* Left shift */
                case XK_Shift_R:   shiftr = FALSE; break; /* Right shift */
                case XK_Control_L: ctrll = FALSE; break;  /* Left control */
                case XK_Control_R: ctrlr = FALSE; break;  /* Right control */
                case XK_Alt_L:     altl = FALSE; break;  /* Left alt */
                case XK_Alt_R:     altr = FALSE; break;  /* Right alt */

            }

        }

    } while (!evtfnd); /* until we have a client event */

}

/** ****************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.

Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it sends a timer mature event to
the associated input file.

*******************************************************************************/

void pa_timer(FILE* f, /* file to send event to */
              int   i, /* timer handle */
              long  t, /* number of tenth-milliseconds to run */
              int   r) /* timer is to rerun after completion */

{

}

/** ****************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

*******************************************************************************/

void pa_killtimer(FILE*  f, /* file to kill timer on */
               int i) /* handle of timer */

{

}

/** ****************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

void pa_frametimer(FILE* f, int e)

{

}

/** ****************************************************************************

Set automatic hold state

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from gralib.
This exists to allow the results of gralib unaware programs to be viewed after
termination, instead of exiting an distroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fufills the requirement of
holding gralib unaware programs.

*******************************************************************************/

void pa_autohold(int e)

{

    fautohold = e; /* set new state of autohold */

}

/** ****************************************************************************

Return number of mice

Returns the number of mice implemented. Windows supports only one mouse.

*******************************************************************************/

int pa_mouse(FILE* f)

{

}

/** ****************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

*******************************************************************************/

int pa_mousebutton(FILE* f, int m)

{

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

int pa_joystick(FILE* f)

{

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

int pa_joybutton(FILE* f, int j)

{

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

int pa_joyaxis(FILE* f, int j)

{

}

/** ****************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

*******************************************************************************/

void pa_settabg(FILE* f, int t)

{

}

/** ****************************************************************************

Set tab

Sets a tab at the indicated collumn number.

*******************************************************************************/

void pa_settab(FILE* f, int t)

{

}

/** ****************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

*******************************************************************************/

void pa_restabg(FILE* f, int t)

{

}

/** ****************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void pa_restab(FILE* f, int t)

{

}

/** ****************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void pa_clrtab(FILE* f)

{

}

/** ****************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

*******************************************************************************/

int pa_funkey(FILE* f)

{

}

/** ****************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

void pa_title(FILE* f, char* ts)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    XStoreName(padisplay, win->xwhan, ts);
    XSetIconName(padisplay, win->xwhan, ts);

}

/** ****************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.
The window id can be from 1 to ss_maxhdl, but the input and output file ids
of 1 and 2 are reserved for the input and output files, and cannot be used
directly. These ids will be be opened as a pair anytime the "_input" or
"_output" file names are seen.

*******************************************************************************/

void pa_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid)

{

}

/** ****************************************************************************

Size buffer pixel

Sets or resets the size of the buffer surface, in pixel units.

*******************************************************************************/

void pa_sizbufg(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

void pa_sizbuf(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

void pa_buffer(FILE* f, int e)

{

}

/** ****************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

*******************************************************************************/

void pa_menu(FILE* f, pa_menuptr m)

{

}

/** ****************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

*******************************************************************************/

void pa_menuena(FILE* f, int id, int onoff)

{

}

/** ****************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

*******************************************************************************/

void pa_menusel(FILE* f, int id, int select)

{

}

/** ****************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

void pa_front(FILE* f)

{

}

/** ****************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

void pa_back(FILE* f)

{

}

/** ****************************************************************************

Get window size graphical

Gets the onscreen window size.

*******************************************************************************/

void pa_getsizg(FILE* f, int* x, int* y)

{

}

/** ****************************************************************************

Get window size character

Gets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are returned. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void pa_getsiz(FILE* f, int* x, int* y)

{

}

/** ****************************************************************************

Set window size graphical

Sets the onscreen window to the given size.

*******************************************************************************/

void pa_setsizg(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Set window size character

Sets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void pa_setsiz(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Set window position graphical

Sets the onscreen window to the given position in its parent.

*******************************************************************************/

void pa_setposg(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Set window position character

Sets the onscreen window position, in character terms. If the window has a
parent, the demensions are converted to the current character size there.
Otherwise, pixel based demensions are used. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void pa_setpos(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Get screen size graphical

Gets the total screen size.

*******************************************************************************/

void pa_scnsizg(FILE* f, int* x, int* y)

{

}

/** ****************************************************************************

Find window size from client

Finds the window size, in parent terms, needed to result in a given client
window size.

Note: this routine should be able to find the minimum size of a window using
the given style, and return the minimums if the input size is lower than this.
This does not seem to be obvious under Windows.

Do we also need a menu style type ?

*******************************************************************************/

void pa_winclient(FILE* f, int cx, int cy, int* wx, int* wy,
               pa_winmodset msset)

{

}

void pa_winclientg(FILE* f, int cx, int cy, int* wx, int* wy,
                pa_winmodset ms)

{

}

/** ****************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

*******************************************************************************/

void pa_scnsiz(FILE* f, int* x, int* y)

{

}

/** ****************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

void pa_frame(FILE* f, int e)

{

}

/** ****************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

*******************************************************************************/

void pa_sizable(FILE* f, int e)

{

}

/** ****************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

*******************************************************************************/

void pa_sysbar(FILE* f, int e)

{

}

/** ****************************************************************************

Create standard menu

Creates a standard menu set. Given a set of standard items selected in a set,
and a program added menu list, creates a new standard menu.

On this windows version, the standard lists are:

file edit <program> window help

That is, all of the standard items are sorted into the lists at the start and
end of the menu, then the program selections placed in the menu.

*******************************************************************************/

void pa_stdmenu(pa_stdmenusel sms, pa_menuptr* sm, pa_menuptr pm)

{

}

/** ****************************************************************************

Kill widget

Removes the widget by id from the window.

*******************************************************************************/

void pa_killwidget(FILE* f, int id)

{

}

/** ****************************************************************************

Select/deselect widget

Selects or deselects a widget.

*******************************************************************************/

void pa_selectwidget(FILE* f, int id, int e)

{

}

/** ****************************************************************************

Enable/disable widget

Enables or disables a widget.

*******************************************************************************/

void pa_enablewidget(FILE* f, int id, int e)

{

}

/** ****************************************************************************

Get widget text

Retrives the text from a widget. The widget must be one that contains text.
It is an error if this call is used on a widget that does not contain text.
This error is currently unchecked.

*******************************************************************************/

void pa_getwidgettext(FILE* f, int id, char* s, int sl)

{

}

/** ****************************************************************************

put edit box text

Places text into an edit box.

*******************************************************************************/

void pa_putwidgettext(FILE* f, int id, char* s)

{

}

/** ****************************************************************************

Resize widget

Changes the size of a widget.

*******************************************************************************/

void pa_sizwidgetg(FILE* f, int id, int x, int y)

{

}

/** ****************************************************************************

Reposition widget

Changes the parent position of a widget.

*******************************************************************************/

void pa_poswidgetg(FILE* f, int id, int x, int y)

{

}

/** ****************************************************************************

Place widget to back of Z order

*******************************************************************************/

void pa_backwidget(FILE* f, int id)

{

}

/** ****************************************************************************

Place widget to back of Z order

*******************************************************************************/

void pa_frontwidget(FILE* f, int id)

{

}

/** ****************************************************************************

Find minimum/standard button size

Finds the minimum size for a button. Given the face string, the minimum size of
a button is calculated and returned.

*******************************************************************************/

void pa_buttonsizg(FILE* f, char* s, int* w, int* h)

{

}

void pa_buttonsiz(FILE* f, char* s, int* w, int* h)

{

}

/** ****************************************************************************

Create button

Creates a standard button within the specified rectangle, on the given window.

*******************************************************************************/

void pa_buttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

void pa_button(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

/** ****************************************************************************

Find minimum/standard checkbox size

Finds the minimum size for a checkbox. Given the face string, the minimum size of
a checkbox is calculated and returned.

*******************************************************************************/

void pa_checkboxsizg(FILE* f, char* s, int* w, int* h)

{

}

void pa_checkboxsiz(FILE* f, char* s,  int* w, int* h)

{

}

/** ****************************************************************************

Create checkbox

Creates a standard checkbox within the specified rectangle, on the given
window.

*******************************************************************************/

void pa_checkboxg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

void pa_checkbox(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

/** ****************************************************************************

Find minimum/standard radio button size

Finds the minimum size for a radio button. Given the face string, the minimum
size of a radio button is calculated and returned.

*******************************************************************************/

void pa_radiobuttonsizg(FILE* f, char* s, int* w, int* h)

{

}

void pa_radiobuttonsiz(FILE* f, char* s, int* w, int* h)

{

}

/** ****************************************************************************

Create radio button

Creates a standard radio button within the specified rectangle, on the given
window.

*******************************************************************************/

void pa_radiobuttong(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

void pa_radiobutton(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

/** ****************************************************************************

Find minimum/standard group size

Finds the minimum size for a group. Given the face string, the minimum
size of a group is calculated and returned.

*******************************************************************************/

void pa_groupsizg(FILE* f, char* s, int cw, int ch, int* w, int* h,
               int* ox, int* oy)

{

}

void pa_groupsiz(FILE* f, char* s, int cw, int ch, int* w, int* h,
              int* ox, int* oy)

{

}

/** ****************************************************************************

Create group box

Creates a group box, which is really just a decorative feature that gererates
no messages. It is used as a background for other widgets.

*******************************************************************************/

void pa_groupg(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

void pa_group(FILE* f, int x1, int y1, int x2, int y2, char* s, int id)

{

}

/** ****************************************************************************

Create background box

Creates a background box, which is really just a decorative feature that
generates no messages. It is used as a background for other widgets.

*******************************************************************************/

void pa_backgroundg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_background(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

/** ****************************************************************************

Find minimum/standard vertical scrollbar size

Finds the minimum size for a vertical scrollbar. The minimum size of a vertical
scrollbar is calculated and returned.

*******************************************************************************/

void pa_scrollvertsizg(FILE* f, int* w, int* h)

{

}

void pa_scrollvertsiz(FILE* f, int* w, int* h)

{

}

/** ****************************************************************************

Create vertical scrollbar

Creates a vertical scrollbar.

*******************************************************************************/

void pa_scrollvertg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_scrollvert(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

/** ****************************************************************************

Find minimum/standard horizontal scrollbar size

Finds the minimum size for a horizontal scrollbar. The minimum size of a
horizontal scrollbar is calculated and returned.

*******************************************************************************/

void pa_scrollhorizsizg(FILE* f, int* w, int* h)

{

}

void pa_scrollhorizsiz(FILE* f, int* w, int* h)

{

}

/** ****************************************************************************

Create horizontal scrollbar

Creates a horizontal scrollbar.

*******************************************************************************/

void pa_scrollhorizg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_scrollhoriz(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

/** ****************************************************************************

Set scrollbar position

Sets the current position of a scrollbar slider.

*******************************************************************************/

void pa_scrollpos(FILE* f, int id, int r)

{

}

/** ****************************************************************************

Set scrollbar size

Sets the current size of a scrollbar slider.

*******************************************************************************/

void pa_scrollsiz(FILE* f, int id, int r)

{

}

/** ****************************************************************************

Find minimum/standard number select box size

Finds the minimum size for a number select box. The minimum size of a number
select box is calculated and returned.

*******************************************************************************/

void pa_numselboxsizg(FILE* f, int l, int u, int* w, int* h)

{

}

void pa_numselboxsiz(FILE* f, int l, int u, int* w, int* h)

{

}

/** ****************************************************************************

Create number selector

Creates an up/down control for numeric selection.

*******************************************************************************/

void pa_numselboxg(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

}

void pa_numselbox(FILE* f, int x1, int y1, int x2, int y2, int l, int u, int id)

{

}

/** ****************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void pa_editboxsizg(FILE* f, char* s, int* w, int* h)

{

}

void pa_editboxsiz(FILE* f, char* s, int* w, int* h)

{

}

/** ****************************************************************************

Create edit box

Creates single line edit box

*******************************************************************************/

void pa_editboxg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_editbox(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

/** ****************************************************************************

Find minimum/standard edit box size

Finds the minimum size for an edit box. Given a sample face string, the minimum
size of an edit box is calculated and returned.

*******************************************************************************/

void pa_progbarsizg(FILE* f, int* w, int* h)

{

}

void pa_progbarsiz(FILE* f, int* w, int* h)

{

}

/** ****************************************************************************

Create progress bar

Creates a progress bar.

*******************************************************************************/

void pa_progbarg(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

void pa_progbar(FILE* f, int x1, int y1, int x2, int y2, int id)

{

}

/** ****************************************************************************

Set progress bar position

Sets the position of a progress bar, from 0 to maxint.

*******************************************************************************/

void pa_progbarpos(FILE* f, int id, int pos)

{

}

/** ****************************************************************************

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

void pa_listboxsizg(FILE* f, pa_strptr sp, int* w, int* h)

{

}

void pa_listboxsiz(FILE* f, pa_strptr sp, int* w, int* h)

{

}

/** ****************************************************************************

Create list box

Creates a list box. Fills it with the string list provided.

*******************************************************************************/

void pa_listboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

}

void pa_listbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

}

/** ****************************************************************************

Find minimum/standard dropbox size

Finds the minimum size for a dropbox. Given the face string, the minimum size of
a dropbox is calculated and returned, for both the "open" and "closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void pa_dropboxsizg(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

}

void pa_dropboxsiz(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

}

/** ****************************************************************************

Create dropdown box

Creates a dropdown box. Fills it with the string list provided.

*******************************************************************************/

void pa_dropboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

}

void pa_dropbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

}

/** ****************************************************************************

Find minimum/standard drop edit box size

Finds the minimum size for a drop edit box. Given the face string, the minimum
size of a drop edit box is calculated and returned, for both the "open" and
"closed" case.

The open sizing is used to create the widget. The reason for this is that the
widget can be smaller than the open size, and it will still work, because the
selections can be scrolled.

*******************************************************************************/

void pa_dropeditboxsizg(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

}

void pa_dropeditboxsiz(FILE* f, pa_strptr sp, int* cw, int* ch, int* ow, int* oh)

{

}

/** ****************************************************************************

Create dropdown edit box

Creates a dropdown edit box. Fills it with the string list provided.

We need to subclass a mode where a return selects the current contents of the
box.

*******************************************************************************/

void pa_dropeditboxg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

}

void pa_dropeditbox(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp, int id)

{

}

/** ****************************************************************************

Find minimum/standard horizontal slider size

Finds the minimum size for a horizontal slider. The minimum size of a horizontal
slider is calculated and returned.

*******************************************************************************/

void pa_slidehorizsizg(FILE* f, int * w, int* h)

{

}

void pa_slidehorizsiz(FILE* f, int* w, int* h)

{

}

/** ****************************************************************************

Create horizontal slider

Creates a horizontal slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void pa_slidehorizg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

}

void pa_slidehoriz(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

}

/** ****************************************************************************

Find minimum/standard vertical slider size

Finds the minimum size for a vertical slider. The minimum size of a vertical
slider is calculated and returned.

*******************************************************************************/

void pa_slidevertsizg(FILE* f, int* w, int* h)

{

}

void pa_slidevertsiz(FILE* f, int* w, int* h)

{

}

/** ****************************************************************************

Create vertical slider

Creates a vertical slider.

Bugs: The tick marks should be in pixel terms, not logical terms.

*******************************************************************************/

void pa_slidevertg(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

}

void pa_slidevert(FILE* f, int x1, int y1, int x2, int y2, int mark, int id)

{

}

/** ****************************************************************************

Find minimum/standard tab bar size

Finds the minimum size for a tab bar. The minimum size of a tab bar is
calculated and returned.

*******************************************************************************/

void pa_tabbarsizg(FILE* f, pa_tabori tor, int cw, int ch, int* w, int* h,
                int* ox, int* oy)

{

}

void pa_tabbarsiz(FILE* f, pa_tabori tor, int cw, int ch, int * w, int* h,
               int* ox, int* oy)

{

}

/** ****************************************************************************

Find client from tabbar size

Given a tabbar size and orientation, this routine gives the client size and
offset. This is used where the tabbar size is fixed, but the client area is
flexible.

*******************************************************************************/

void pa_tabbarclientg(FILE* f, pa_tabori tor, int w, int h, int* cw, int* ch,
                   int* ox, int* oy)

{

}

void pa_tabbarclient(FILE* f, pa_tabori tor, int w, int h, int* cw, int* ch,
                  int* ox, int* oy)

{

}

/** ****************************************************************************

Create tab bar

Creates a tab bar with the given orientation.

Bug: has strange overwrite mode where when the widget is first created, it
allows itself to be overwritten by the main window. This is worked around by
creating and distroying another widget.

*******************************************************************************/

void pa_tabbarg(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
             pa_tabori tor, int id)

{

}

void pa_tabbar(FILE* f, int x1, int y1, int x2, int y2, pa_strptr sp,
            pa_tabori tor, int id)

{

}

/** ****************************************************************************

Set tabbar current select

Sets the current tab selected in a tabbar. The select is the ordinal number
of the tab.

*******************************************************************************/

void pa_tabsel(FILE* f, int id, int tn)

{

}

/** ****************************************************************************

Output message dialog

Outputs a message dialog with the given title and message strings.

*******************************************************************************/

void pa_alert(char* title, char* message)

{

}

/** ****************************************************************************

Display choose color dialog

Presents the choose color dialog, then returns the resulting color.

Bug: does not take the input color as the default.

*******************************************************************************/

void pa_querycolor(int* r, int* g, int* b)

{

}

/** ****************************************************************************

Display choose file dialog for open

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

void pa_queryopen(char* s)

{

}

/** ****************************************************************************

Display choose file dialog for save

Presents the choose file dialog, then returns the file string as a dynamic
string. The default string passed in is presented in the dialog, and a new
string replaces it. The caller is responsible for disposing of the input
string and the output string.

If a wildcard is passed as the default, this will be used to filter the files
in the current directory into a list.

If the operation is cancelled, then a null string will be returned.

*******************************************************************************/

void pa_querysave(char* s)

{

}

/** ****************************************************************************

Display choose find text dialog

Presents the choose find text dialog, then returns the resulting string.
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
sounds very system dependent, since that could change. One method might be
to retrive the button text, but this is still fairly system dependent. We
table this issue until later.

*******************************************************************************/

void pa_queryfind(char* s, pa_qfnopts* opt)

{

}

/** ****************************************************************************

Display choose replace text dialog

Presents the choose replace text dialog, then returns the resulting string.
A find/replace option set can be specified. The parameters are "flow through",
meaning that you set them before the call, and they may or may not be changed
from these defaults after the call. In addition, the parameters are used to
set the dialog.

The string that is passed in is discarded without complaint. It is up to the
caller to dispose of it properly.

Bug: See comment, queryfind.

*******************************************************************************/

void pa_queryfindrep(char* s, char* r, pa_qfropts* opt)

{

}

/** ****************************************************************************

Display choose font dialog

Presents the choose font dialog, then returns the resulting logical font
number, size, foreground color, background color, and effects (in a special
effects set for this routine).

The parameters are "flow through", meaning that they should be set to their
defaults before the call, and changes are made, then updated to the parameters.
During the routine, the state of the parameters given are presented to the
user as the defaults.

*******************************************************************************/

void pa_queryfont(FILE* f, int* fc, int* s, int* fr, int* fg, int* fb,
               int* br, int* bg, int* bb, pa_qfteffects* effect)

{

}

/** ****************************************************************************

Gralib startup

*******************************************************************************/

static void pa_init_graphics (int argc, char *argv[]) __attribute__((constructor (102)));
static void pa_init_graphics(int argc, char *argv[])

{

    const char *title = "";
    int ofn, ifn;
    int fi;
    winptr win; /* windows record pointer */

    /* turn off I/O buffering */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
//    ovr_unlink(iunlink, &ofpunlink);
    ovr_lseek(ilseek, &ofplseek);

    /* set state of shift, control and alt keys */
    ctrll = FALSE;
    ctrlr = FALSE;
    shiftl = FALSE;
    shiftr = FALSE;
    altl = FALSE;
    altr = FALSE;
    capslock = FALSE;

    /* set internal states */
    fend = FALSE; /* set no end of program ordered */
    fautohold = TRUE; /* set automatically hold self terminators */

    /* clear open files tables */
    for (fi = 0; fi < MAXFIL; fi++) {

        opnfil[fi] = NULL; /* set unoccupied */
        /* clear window logical number translator table */
        xltwin[fi] = -1; /* set unoccupied */
        /* clear file to window logical number translator table */
        filwin[fi] = -1; /* set unoccupied */

    }

    /* find existing display */
    padisplay = XOpenDisplay(NULL);
    if (padisplay == NULL) {

        fprintf(stderr, "Cannot open display\n");
        exit(1);

    }
    pascreen = DefaultScreen(padisplay);

    /* open stdin and stdout as I/O window set */
    ifn = fileno(stdin); /* get logical id stdin */
    ofn = fileno(stdout); /* get logical id stdout */
    openio(stdin, stdout, ifn, ofn, -1, 1); /* process open */

    /* set up the expose event to full buffer by default */
    win = lfn2win(ofn); /* get window from fid */
    evtexp.type = Expose;
    evtexp.xexpose.type = Expose;
    evtexp.xexpose.serial = 0;
    evtexp.xexpose.send_event = 1;
    evtexp.xexpose.window = win->xwhan;
    evtexp.xexpose.x = 0;
    evtexp.xexpose.y = 0;
    evtexp.xexpose.width = win->gmaxxg;
    evtexp.xexpose.height = win->gmaxyg;
    evtexp.xexpose.count = 0;

}

/** ****************************************************************************

Gralib shutdown

*******************************************************************************/

static void pa_deinit_graphics (void) __attribute__((destructor (102)));
static void pa_deinit_graphics()

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    punlink_t cppunlink;
    plseek_t cpplseek;

    pa_evtrec er;

	/* if the program tries to exit when the user has not ordered an exit, it
	   is assumed to be a windows "unaware" program. We stop before we exit
	   these, so that their content may be viewed */
	if (!fend && fautohold) { /* process automatic exit sequence */

        /* wait for a formal end */
		while (!fend) pa_event(stdin, &er);

	}
    /* close X Window */
    XCloseDisplay(padisplay);

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
//    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);

    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose || cppunlink != iunlink || cpplseek != ilseek)
        error(esystem);

}
