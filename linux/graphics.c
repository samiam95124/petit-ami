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
* History:                                                                     *
*                                                                              *
* Gralib started in 1996 as a graphical window demonstrator as a twin to       *
* ansilib, the ANSI control character based terminal mode library.             *
* In 2003, gralib was upgraded to the graphical terminal standard.             *
* In 2005, gralib was upgraded to include the window mangement calls, and the  *
* widget calls.                                                                *
*                                                                              *
* The XWindow version started at various times around 2018, the first try was  *
* an attempt at use of GTK.This encountered technical problems that seemed to  *
* be a dead end, but later a solution was found. Irregardless, the rule from   *
* the effort was "the shallower the depth of stacked APIs, the better".        *
*                                                                              *
* The XWindow version was created about the same time as the Windows version   *
* was translated to C. An attempt was and is made to make the structure of the *
* code to be as similar as possible between them. Never the less, there is no  *
* attempt made to produce a universal code base between them. If for no other  *
* reason, that is Petit-Ami's job description.                                 *
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
#include <X11/Xutil.h>
#include <X11/Xatom.h>

/* whitebook definitions */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* linux definitions */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <signal.h>
#include <linux/joystick.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

/* local definitions */
#include <localdefs.h>
#include <config.h>
#include <graphics.h>

/* external definitions */
extern char *program_invocation_short_name;

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
                                __func__, __LINE__, ##__VA_ARGS__); \
                                fflush(stderr); } while (0)

//#define PRTFNT /* print internal fonts list */
//#define PRTMEM /* print memory allocations at exit */
//#define PRTWPM /* print window parameters on open */
//#define NOWDELAY /* don't delay window presentation until drawn */
#define NOCANCEL /* include nocancel overrides */

/* the "standard character" sizes are used to form a pseudo-size for desktop
   character measurements in a graphical system. */
#define STDCHRX   8
#define STDCHRY   12
#define MAXBUF 10  /* maximum number of buffers available */
#define IOWIN  1   /* logical window number of input/output pair */
#define MAXCON 10  /* number of screen contexts */
#define MAXTAB 50  /* total number of tabs possible per screen */
#define MAXPIC 50  /* total number of loadable pictures */
#define MAXLIN 250 /* maximum length of input bufferred line */
#define MAXFIL 100 /* maximum open files */
#define MINJST 1   /* minimum pixels for space in justification */
#define MAXFNM 250 /* number of filename characters in buffer */

/* To properly compensate for high DPI displays, we use actual height onscreen
   to determine the character height. Note the point size was choosen to most
   closely match xterm. */
#define POINT  (0.353) /* point size in mm */
#define CONPNT 18/*11.5*/ /* height of console font */
#define STRIKE (1.5)      /* strikeout percentage (from top of cell to baseline */
#define EXTRAMENUY 10     /* extra space for menu bar y */
#define EXTRAMENUX 10     /* extra space for menu bar x */

/*
 * Configurable parameters
 *
 * These parameters can be configured here at compile time, or are overriden
 * at runtime by values of the same name in the config files.
 */
#define MAXXD     80 /* standard terminal, 80x25 */
#define MAXYD     25
#define DIALOGERR 1     /* send runtime errors to dialog */
#define MOUSEENB  TRUE  /* enable mouse */
#define JOYENB    TRUE  /* enable joysticks */
#define DMPMSG    FALSE /* enable dump messages (diagnostic) */
#define DMPEVT    FALSE /* enable dump Petit-Ami messages */
#define PRTFTM    FALSE /* print font metrics (diagnostic) */

/* file handle numbers at the system interface level */

#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */
#define ERRFIL 2 /* handle to standard error */

/* XWindows call lock/unlock */
#define XWLOCK() pthread_mutex_lock(&xwlock)
#define XWUNLOCK() pthread_mutex_unlock(&xwlock)

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef off_t (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

#ifdef NOCANCEL
extern void ovr_read_nocancel(pread_t nfp, pread_t* ofp);
extern void ovr_write_nocancel(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open_nocancel(popen_t nfp, popen_t* ofp);
extern void ovr_close_nocancel(pclose_t nfp, pclose_t* ofp);
#endif

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

/* XWindow font attributes. These are similar, but not identical to the screen
   text attributes. Its XWindow specific because we have to use these attibutes
   to automatically recreate the font specifications. They are mapped into PA
   standard text attributes by each set routine. */
typedef enum {

    /* weights */
    xcnormal,        /* normal */
    xcmedium,        /* medium */
    xcbold,          /* bold */
    xcdemibold,      /* demibold */
    xcdark,          /* dark */
    xclight,         /* light */

    /* slants */
    xcroman,         /* no slant */
    xcital,          /* italic */
    xcoblique,       /* oblique */
    xcrital,         /* reverse italic */
    xcroblique,      /* reverse oblique */

    /* widths */
    xcnormalw,       /* normal */
    xcnarrow,        /* narrow */
    xccondensed,     /* condensed */
    xcsemicondensed, /* semicondensed */
    xcexpanded,      /* expanded */

    /* spacing */
    xcproportional,  /* proportional */
    xcmonospace,     /* monospaced */
    xcchar           /* character spaced */

} xwcaps;

typedef struct xcaplst {

    struct xcaplst* next; /* next entry */
    int             caps; /* XWindow font capabilities set */

} xcaplst;

/* font description entry */
typedef struct fontrec {

    char*           fn;     /* name of font */
    int             fix;    /* fixed pitch font flag */
    int             caps;   /* set of XWindow font capabilities */
    xcaplst*        caplst; /* list of all XWindow font capabilities */
    struct fontrec* next;   /* next font in list */

} fontrec, *fontptr;

typedef enum { mdnorm, mdinvis, mdxor, mdand, mdor } mode; /* color mix modes */

/* Widget control structure */
typedef struct widget {

    int            pressed; /* in the pressed state */
    FILE*          wf;      /* output file for the widget window */
    char*          title;   /* title text */
    FILE*          parent;  /* parent window */
    FILE*          evtfil;  /* file to post menu events to */
    int            id;      /* id number */
    int            wid;     /* widget window id */

} widget;

/* Menu tracking. This is a mirror image of the menu we were given by the
   user. However, we can do with less information than is in the original
   tree as passed. The menu items are a linear list, since they contain
   both the menu handle and the relative number 0-n of the item, neither
   the lack of tree structure nor the order of the list matters. */
typedef struct metrec* metptr;
typedef struct metrec {

    metptr next;    /* next entry */
    metptr branch;  /* menu branch */
    metptr frame;   /* frame for pulldown menu */
    metptr head;    /* head of menu pointer */
    int    menubar; /* is the menu bar */
    int    frm;     /* is a frame */
    int    onoff;   /* the item is on-off highlighted */
    int    select;  /* the current on/off state of the highlight */
    metptr oneof;   /* "one of" chain pointer */
    int    bar;     /* has bar under */
    int    id;      /* user id of item */
    int    x, y;    /* subclient position of window */
    int    prime;   /* is a prime (onscreen) entry */
    int    pressed; /* in the pressed state */
    FILE*  wf;      /* output file for the menu window */
    char*  title;   /* title text */
    FILE*  parent;  /* parent window */
    FILE*  evtfil;  /* file to post menu events to */
    int    wid;     /* menu window id */

} metrec;

typedef struct scncon* scnptr;
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
    GC      xcxt;        /* graphics context */
    Pixmap  xbuf;        /* pixmap for screen backing buffer */

} scncon;

typedef struct pict* picptr;
typedef struct pict { /* picture tracking record */

    struct pict* next; /* list of rescaled images */
    int          sx; /* size in x */
    int          sy; /* size in y */
    XImage*      xi; /* Xwindows image */

} pict;

/* XWindow style rectangle */
typedef struct {

    int         x, y; /* origin */
    int         w, h; /* width/height */

} xrect;

/* window description */
typedef struct winrec* winptr;
typedef struct winrec {

    winptr       next;              /* next entry (for free list) */
    /* fields used by graph module */
    int          parlfn;            /* logical parent */
    int          wid;               /* this window logical id */
    scnptr       screens[MAXCON];   /* screen contexts array */
    int          curdsp;            /* index for current display screen */
    int          curupd;            /* index for current update screen */
    /* global sets. these are the global set parameters that apply to any new
      created screen buffer */
    int          gmaxx;             /* maximum x size */
    int          gmaxy;             /* maximum y size */
    int          gmaxxg;            /* size of client area in x */
    int          gmaxyg;            /* size of client area in y */
    int          bufx;              /* buffer size x characters */
    int          bufy;              /* buffer size y characters */
    int          bufxg;             /* buffer size x pixels */
    int          bufyg;             /* buffer size y pixels */
    int          gattr;             /* current attributes */
    int          gauto;             /* state of auto */
    int          gfcrgb;            /* foreground color in rgb */
    int          gbcrgb;            /* background color in rgb */
    int          gcurv;             /* state of cursor visible */
    fontptr      gcfont;            /* current font select */
    int          gfhigh;            /* current font height */
    mode         gfmod;             /* foreground mix mode */
    mode         gbmod;             /* background mix mode */
    int          goffx;             /* viewport offset x */
    int          goffy;             /* viewport offset y */
    int          gwextx;            /* window extent x */
    int          gwexty;            /* window extent y */
    int          gvextx;            /* viewpor extent x */
    int          gvexty;            /* viewport extent y */
    int          termfnt;           /* terminal font number */
    int          bookfnt;           /* book font number */
    int          signfnt;           /* sign font number */
    int          techfnt;           /* technical font number */
    int          mb1;               /* mouse assert status button 1 */
    int          mb2;               /* mouse assert status button 2 */
    int          mb3;               /* mouse assert status button 3 */
    int          mpx, mpy;          /* mouse current position */
    int          mpxg, mpyg;        /* mouse current position graphical */
    int          nmb1;              /* new mouse assert status button 1 */
    int          nmb2;              /* new mouse assert status button 2 */
    int          nmb3;              /* new mouse assert status button 3 */
    int          nmpx, nmpy;        /* new mouse current position */
    int          nmpxg, nmpyg;      /* new mouse current position graphical */
    int          linespace;         /* line spacing in pixels */
    int          charspace;         /* character spacing in pixels */
    int          chrspcx;           /* extra space between characters */
    int          chrspcy;           /* extra space between lines */
    int          curspace;          /* size of cursor, in pixels */
    int          baseoff;           /* font baseline offset from top */
    int          menuspcy;          /* amount of space for menu in y (if exists) */
    int          shift;             /* state of shift key */
    int          cntrl;             /* state of control key */
    int          fcurdwn;           /* cursor on screen flag */
    int          joy1cap;           /* joystick 1 is captured */
    int          joy2cap;           /* joystick 2 is captured */
    int          joy1xs;            /* last joystick position 1x */
    int          joy1ys;            /* last joystick position 1y */
    int          joy1zs;            /* last joystick position 1z */
    int          joy2xs;            /* last joystick position 2x */
    int          joy2ys;            /* last joystick position 2y */
    int          joy2zs;            /* last joystick position 2z */
    int          shsize;            /* display screen size x in millimeters */
    int          svsize;            /* display screen size y in millimeters */
    int          shres;             /* display screen pixels in x */
    int          svres;             /* display screen pixels in y */
    int          sdpmx;             /* display screen find dots per meter x */
    int          sdpmy;             /* display screen find dots per meter y */
    char         inpbuf[MAXLIN];    /* input line buffer */
    int          inpptr;            /* input line index */
    int          frmrun;            /* framing timer is running */
    int          timers[PA_MAXTIM]; /* timer id array */
    int          focus;             /* screen in focus */
    picptr       pictbl[MAXPIC];    /* loadable pictures table */
    int          bufmod;            /* buffered screen mode */
    metptr       metlst;            /* menu tracking list */
    metptr       menu;              /* "faux menu" bar */
    int          frame;             /* frame on/off */
    int          size;              /* size bars on/off */
    int          sysbar;            /* system bar on/off */
    int          sizests;           /* last resize status save */
    int          visible;           /* window is visible */

    /* fields used by graphics subsystem */
    Window       xmwhan;            /* master window */
    Window       xwhan;             /* subclient window */
    xrect        xmwr;              /* master window rectangle */
    xrect        xwr;               /* subclient window rectangle */
    XFontStruct* xfont;             /* current font */
    Atom         delmsg;            /* windows manager delete window message */
    int          pfw;               /* parent/frame width (extra) */
    int          pfh;               /* parent frame height (extra) */
    int          cwox;              /* client window offset from parent
                                       origin x */
    int          cwoy;              /* client window offset from parent
                                       origin y */

} winrec;

/* File tracking.
  Files can be passthrough to the OS, or can be associated with a window. If
  on a window, they can be output, or they can be input. In the case of
  input, the file has its own input queue, and will receive input from all
  windows that are attached to it. */
typedef struct filrec* filptr;
typedef struct filrec {

      FILE*  sfp;  /* file pointer used to establish entry, or NULL */
      winptr win;  /* associated window (if exists) */
      int    inw;  /* entry is input linked to window */
      int    inl;  /* this output file is linked to the input file, logical */
      int    tim;  /* fid has a timer associated with it */
      winptr twin; /* window associated with timer */

} filrec;

/* internal client messages */
typedef enum {

    cm_timer /* timer fires */

} clientmessagecode;

/* error codes */
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
    estdfnt,  /* Cannot find standard font */
    eftntl,   /* Font name too large */
    epicopn,  /* Cannot open picture file */
    ebadfmt,  /* Bad format of picture file */
    ecfgval,  /* invalid configuration value */
    enoopn,   /* Cannot open file */
    enoinps,  /* no input side for this window */
    enowid,   /* No more window ids available */
    esystem   /* System consistency check */

} errcod;

/* mode to function table */
int mod2fnc[mdor+1] = {

    GXcopy, /* mdnorm */
    GXnoop, /* mdinvis */
    GXxor,  /* mdxor */
    GXand,  /* mdand */
    GXor    /* mdor */

};

/* XEvent queue structure. Its a bubble list. */
typedef struct xevtque {

    struct xevtque* next; /* next in list */
    struct xevtque* last; /* last in list */
    XEvent          evt;  /* event data */

} xevtque;

/* PA queue structure. Its a bubble list. */
typedef struct paevtque {

    struct paevtque* next; /* next in list */
    struct paevtque* last; /* last in list */
    pa_evtrec       evt;  /* event data */

} paevtque;

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t   ofpread;
static pread_t   ofpread_nocancel;
static pwrite_t  ofpwrite;
static pwrite_t  ofpwrite_nocancel;
static popen_t   ofpopen;
static popen_t   ofpopen_nocancel;
static pclose_t  ofpclose;
static pclose_t  ofpclose_nocancel;
static plseek_t  ofplseek;

/* X Windows globals */

static int fend;      /* end of program ordered flag */
static int fautohold; /* automatic hold on exit flag */
static pthread_mutex_t xwlock; /* XWindow call lock */

/* X windows display characteristics.
 *
 * Note that some of these are going to need to move to a per-window structure.
 */
static Display*   padisplay;      /* current display */
static int        pascreen;       /* current screen */
static int        ctrll, ctrlr;   /* control key active */
static int        shiftl, shiftr; /* shift key active */
static int        altl, altr;     /* alt key active */
static int        capslock;       /* caps lock key active */
static filptr     opnfil[MAXFIL]; /* open files table */
static int        xltwin[MAXFIL*2+1]; /* window equivalence table, includes
                                         negatives and 0 */
static metptr     xltmnu[MAXFIL*2+1]; /* menu entry equivalence table */
static int        filwin[MAXFIL]; /* file to window equivalence table */
static int        esck;           /* previous key was escape */
static fontptr    fntlst;         /* list of XWindow fonts */
static int        fntcnt;         /* number of fonts */
static picptr     frepic;         /* free picture entries */
static int        numjoy;         /* number of joysticks found */
static int        joyfid;         /* joystick file id */
static int        joyax;          /* joystick x axis save */
static int        joyay;          /* joystick y axis save */
static int        joyaz;          /* joystick z axis save */
static int        frmfid;         /* framing timer fid */
static int        cfgcap;         /* "configuration" caps */
static pa_pevthan evthan[pa_ettabbar+1]; /* array of event handler routines */
static pa_pevthan evtshan;        /* single master event handler routine */
static xevtque*   freque;         /* free XEvent queue entries list */
static xevtque*   evtque;         /* XEvent input save queue */
static paevtque*  paqfre;         /* free XEvent queue entries list */
static paevtque*  paqevt;         /* XEvent input save queue */
static pa_pevthan menu_event_oeh; /* event callback save for menus */
static metptr     fremet;         /* free menu entrys list */
static winptr     winfre;         /* free windows structure list */

/* memory statistics/diagnostics */
static unsigned long memusd;    /* total memory in use for malloc */
static unsigned long memrty;    /* retries executed on malloc */
static unsigned long maxrty;    /* maximum retry count */
static unsigned long fontcnt;   /* font entry counter */
static unsigned long fonttot;   /* font entry total */
static unsigned long filcnt;    /* file entry counter */
static unsigned long filtot;    /* file entry total */
static unsigned long piccnt;    /* picture entry counter */
static unsigned long pictot;    /* picture entry total */
static unsigned long scncnt;    /* screen struct counter */
static unsigned long scntot;    /* screen struct total */
static unsigned long wincnt;    /* windows structure counter */
static unsigned long wintot;    /* windows structure total */
static unsigned long imgcnt;    /* image frame counter */
static unsigned long imgtot;    /* image frame total */
static unsigned long metcnt;    /* menu entries counter */
static unsigned long mettot;    /* menu entries total */

/* config settable runtime options */
static int maxxd;     /* default window dimensions */
static int maxyd;
static int dialogerr; /* send runtime errors to dialog */
static int mouseenb;  /* enable mouse */
static int joyenb;    /* enable joysticks */
static int dmpmsg;    /* enable dump messages (diagnostic, windows only) */
static int dmpevt;    /* enable dump Petit-Ami messages */
static int prtftm;    /* print font metrics (diagnostic) */

/**
 * Set of input file ids for select
 */
static fd_set ifdseta; /* active sets */
static fd_set ifdsets; /* signaled set */
static int ifdmax;     /* maximum FID for select() */

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
      case enotinp:  fprintf(stderr, "Not input side of any window"); break;
      case estdfnt:  fprintf(stderr, "Cannot find standard font"); break;
      case eftntl:   fprintf(stderr, "Font name too large"); break;
      case epicopn:  fprintf(stderr, "Cannot open picture file"); break;
      case ebadfmt:  fprintf(stderr, "Bad format of picture file"); break;
      case ecfgval:  fprintf(stderr, "Invalid configuration value"); break;
      case enoopn:   fprintf(stderr, "Cannot open file"); break;
      case enoinps:  fprintf(stderr, "No input side for this window"); break;
      case enowid:   fprintf(stderr, "No more window ids available"); break;
      case esystem:  fprintf(stderr, "System consistency check"); break;

    }
    fprintf(stderr, "\n");
    fflush(stderr); /* make sure error message is output */

    exit(1);

}

/******************************************************************************

Internal version of malloc/free

This wrapper around malloc/free both handles errors and also gives us a chance
to track memory useage. Running out of memory is a terminal event, so we don't
need to return.

Only allocated memory is tallied, no accounting is done for free(). The vast
majority of memory used in this package is never returned.

******************************************************************************/

static void *imalloc(size_t size)

{

    int rt;

    void* ptr;

    rt = 0;
    do {

        ptr = malloc(size);
        rt++;
        memrty++;
        if (memrty > maxrty) maxrty = memrty;

    } while (rt < 100);
    if (!ptr) {

#ifdef PRTMEM
        fprintf(stderr, "Malloc fail, memory used: %lu retries: %lu\n", memusd,
                        memrty);
        fprintf(stderr, "Maximum retry: %lu\n", maxrty);
        fprintf(stderr, "Font entry counter:    %lu\n", fontcnt);
        fprintf(stderr, "Font entry total:      %lu\n", fonttot);
        fprintf(stderr, "File entry counter:    %lu\n", filcnt);
        fprintf(stderr, "File entry total:      %lu\n", filtot);
        fprintf(stderr, "Picture entry counter: %lu\n", piccnt);
        fprintf(stderr, "Picture entry total:   %lu\n", pictot);
        fprintf(stderr, "Screen entry counter:  %lu\n", scncnt);
        fprintf(stderr, "Screen entry total:    %lu\n", scntot);
        fprintf(stderr, "Window entry counter:  %lu\n", wincnt);
        fprintf(stderr, "Window entry total:    %lu\n", wintot);
        fprintf(stderr, "Image frame counter:   %lu\n", imgcnt);
        fprintf(stderr, "Image frame total:     %lu\n", imgtot);
        fprintf(stderr, "Menu entries counter:  %lu\n", metcnt);
        fprintf(stderr, "Menu entries total:    %lu\n", mettot);
#endif
        error(enomem);

    }
    memusd += size;

    return ptr;

}

static void ifree(void* ptr)

{

    free(ptr);

}

/******************************************************************************

Print event symbol

A diagnostic, print the given event code as a symbol to the error file.

******************************************************************************/

void prtevt(pa_evtcod e)

{

    switch (e) {

        case pa_etchar:    fprintf(stderr, "etchar"); break;
        case pa_etup:      fprintf(stderr, "etup"); break;
        case pa_etdown:    fprintf(stderr, "etdown"); break;
        case pa_etleft:    fprintf(stderr, "etleft"); break;
        case pa_etright:   fprintf(stderr, "etright"); break;
        case pa_etleftw:   fprintf(stderr, "etleftw"); break;
        case pa_etrightw:  fprintf(stderr, "etrightw"); break;
        case pa_ethome:    fprintf(stderr, "ethome"); break;
        case pa_ethomes:   fprintf(stderr, "ethomes"); break;
        case pa_ethomel:   fprintf(stderr, "ethomel"); break;
        case pa_etend:     fprintf(stderr, "etend"); break;
        case pa_etends:    fprintf(stderr, "etends"); break;
        case pa_etendl:    fprintf(stderr, "etendl"); break;
        case pa_etscrl:    fprintf(stderr, "etscrl"); break;
        case pa_etscrr:    fprintf(stderr, "etscrr"); break;
        case pa_etscru:    fprintf(stderr, "etscru"); break;
        case pa_etscrd:    fprintf(stderr, "etscrd"); break;
        case pa_etpagd:    fprintf(stderr, "etpagd"); break;
        case pa_etpagu:    fprintf(stderr, "etpagu"); break;
        case pa_ettab:     fprintf(stderr, "ettab"); break;
        case pa_etenter:   fprintf(stderr, "etenter"); break;
        case pa_etinsert:  fprintf(stderr, "etinsert"); break;
        case pa_etinsertl: fprintf(stderr, "etinsertl"); break;
        case pa_etinsertt: fprintf(stderr, "etinsertt"); break;
        case pa_etdel:     fprintf(stderr, "etdel"); break;
        case pa_etdell:    fprintf(stderr, "etdell"); break;
        case pa_etdelcf:   fprintf(stderr, "etdelcf"); break;
        case pa_etdelcb:   fprintf(stderr, "etdelcb"); break;
        case pa_etcopy:    fprintf(stderr, "etcopy"); break;
        case pa_etcopyl:   fprintf(stderr, "etcopyl"); break;
        case pa_etcan:     fprintf(stderr, "etcan"); break;
        case pa_etstop:    fprintf(stderr, "etstop"); break;
        case pa_etcont:    fprintf(stderr, "etcont"); break;
        case pa_etprint:   fprintf(stderr, "etprint"); break;
        case pa_etprintb:  fprintf(stderr, "etprintb"); break;
        case pa_etprints:  fprintf(stderr, "etprints"); break;
        case pa_etfun:     fprintf(stderr, "etfun"); break;
        case pa_etmenu:    fprintf(stderr, "etmenu"); break;
        case pa_etmouba:   fprintf(stderr, "etmouba"); break;
        case pa_etmoubd:   fprintf(stderr, "etmoubd"); break;
        case pa_etmoumov:  fprintf(stderr, "etmoumov"); break;
        case pa_ettim:     fprintf(stderr, "ettim"); break;
        case pa_etjoyba:   fprintf(stderr, "etjoyba"); break;
        case pa_etjoybd:   fprintf(stderr, "etjoybd"); break;
        case pa_etjoymov:  fprintf(stderr, "etjoymov"); break;
        case pa_etresize:  fprintf(stderr, "etresize"); break;
        case pa_etterm:    fprintf(stderr, "etterm"); break;
        case pa_etmoumovg: fprintf(stderr, "etmoumovg"); break;
        case pa_etframe:   fprintf(stderr, "etframe"); break;
        case pa_etredraw:  fprintf(stderr, "etredraw"); break;
        case pa_etmin:     fprintf(stderr, "etmin"); break;
        case pa_etmax:     fprintf(stderr, "etmax"); break;
        case pa_etnorm:    fprintf(stderr, "etnorm"); break;
        case pa_etmenus:   fprintf(stderr, "etmenus"); break;
        case pa_etbutton:  fprintf(stderr, "etbutton"); break;
        case pa_etchkbox:  fprintf(stderr, "etchkbox"); break;
        case pa_etradbut:  fprintf(stderr, "etradbut"); break;
        case pa_etsclull:  fprintf(stderr, "etsclull"); break;
        case pa_etscldrl:  fprintf(stderr, "etscldrl"); break;
        case pa_etsclulp:  fprintf(stderr, "etsclulp"); break;
        case pa_etscldrp:  fprintf(stderr, "etscldrp"); break;
        case pa_etsclpos:  fprintf(stderr, "etsclpos"); break;
        case pa_etedtbox:  fprintf(stderr, "etedtbox"); break;
        case pa_etnumbox:  fprintf(stderr, "etnumbox"); break;
        case pa_etlstbox:  fprintf(stderr, "etlstbox"); break;
        case pa_etdrpbox:  fprintf(stderr, "etdrpbox"); break;
        case pa_etdrebox:  fprintf(stderr, "etdrebox"); break;
        case pa_etsldpos:  fprintf(stderr, "etsldpos"); break;
        case pa_ettabbar:   fprintf(stderr, "ettabbar"); break;

        default: fprintf(stderr, "???");

    }

}

/******************************************************************************

Print XWindow event type

A diagnostic. Prints the XWindow event type codes.

******************************************************************************/

void prtxevtt(int type)

{

    switch (type) {

        case 2:  fprintf(stderr, "KeyPress"); break;
        case 3:  fprintf(stderr, "KeyRelease"); break;
        case 4:  fprintf(stderr, "ButtonPress"); break;
        case 5:  fprintf(stderr, "ButtonRelease"); break;
        case 6:  fprintf(stderr, "MotionNotify"); break;
        case 7:  fprintf(stderr, "EnterNotify"); break;
        case 8:  fprintf(stderr, "LeaveNotify"); break;
        case 9:  fprintf(stderr, "FocusIn"); break;
        case 10: fprintf(stderr, "FocusOut"); break;
        case 11: fprintf(stderr, "KeymapNotify"); break;
        case 12: fprintf(stderr, "Expose"); break;
        case 13: fprintf(stderr, "GraphicsExpose"); break;
        case 14: fprintf(stderr, "NoExpose"); break;
        case 15: fprintf(stderr, "VisibilityNotify"); break;
        case 16: fprintf(stderr, "CreateNotify"); break;
        case 17: fprintf(stderr, "DestroyNotify"); break;
        case 18: fprintf(stderr, "UnmapNotify"); break;
        case 19: fprintf(stderr, "MapNotify"); break;
        case 20: fprintf(stderr, "MapRequest"); break;
        case 21: fprintf(stderr, "ReparentNotify"); break;
        case 22: fprintf(stderr, "ConfigureNotify"); break;
        case 23: fprintf(stderr, "ConfigureRequest"); break;
        case 24: fprintf(stderr, "GravityNotify"); break;
        case 25: fprintf(stderr, "ResizeRequest"); break;
        case 26: fprintf(stderr, "CirculateNotify"); break;
        case 27: fprintf(stderr, "CirculateRequest"); break;
        case 28: fprintf(stderr, "PropertyNotify"); break;
        case 29: fprintf(stderr, "SelectionClear"); break;
        case 30: fprintf(stderr, "SelectionRequest"); break;
        case 31: fprintf(stderr, "SelectionNotify"); break;
        case 32: fprintf(stderr, "ColormapNotify"); break;
        case 33: fprintf(stderr, "ClientMessage"); break;
        case 34: fprintf(stderr, "MappingNotify"); break;
        case 35: fprintf(stderr, "GenericEvent"); break;
        default: fprintf(stderr, "???"); break;

    }

}

/******************************************************************************

Print attributes set

Prints the contents of a PA attributes set.

******************************************************************************/

void prtatset(int at)

{

    if (at & BIT(sablink)) fprintf(stderr, "blink ");
    if (at & BIT(sarev)) fprintf(stderr, "rev ");
    if (at & BIT(saundl)) fprintf(stderr, "underl ");
    if (at & BIT(sasuper)) fprintf(stderr, "super ");
    if (at & BIT(sasubs)) fprintf(stderr, "subs ");
    if (at & BIT(saital)) fprintf(stderr, "italic ");
    if (at & BIT(sabold)) fprintf(stderr, "bold ");
    if (at & BIT(sastkout)) fprintf(stderr, "strkout ");
    if (at & BIT(sacondensed)) fprintf(stderr, "cond ");
    if (at & BIT(saextended)) fprintf(stderr, "ext ");
    if (at & BIT(saxlight)) fprintf(stderr, "xlight ");
    if (at & BIT(salight)) fprintf(stderr, "light ");
    if (at & BIT(saxbold)) fprintf(stderr, "xbold ");
    if (at & BIT(sahollow)) fprintf(stderr, "hollow ");
    if (at & BIT(saraised)) fprintf(stderr, "raised ");

}

/******************************************************************************

Print capabilities set

Prints the contents of a XWindows capabilities set.

******************************************************************************/

void prtxcset(int caps)

{

    if (caps & BIT(xcnormal)) fprintf(stderr, "norm ");
    if (caps & BIT(xcmedium)) fprintf(stderr, "med ");
    if (caps & BIT(xcbold)) fprintf(stderr, "bold ");
    if (caps & BIT(xcdemibold)) fprintf(stderr, "dbold ");
    if (caps & BIT(xcdark)) fprintf(stderr, "dark ");
    if (caps & BIT(xclight)) fprintf(stderr, "light ");
    if (caps & BIT(xcroman)) fprintf(stderr, "rom ");
    if (caps & BIT(xcital)) fprintf(stderr, "ital ");
    if (caps & BIT(xcoblique)) fprintf(stderr, "obliq ");
    if (caps & BIT(xcrital)) fprintf(stderr, "rital ");
    if (caps & BIT(xcroblique)) fprintf(stderr, "robliq ");
    if (caps & BIT(xcnormalw)) fprintf(stderr, "normw ");
    if (caps & BIT(xcnarrow)) fprintf(stderr, "narrw ");
    if (caps & BIT(xccondensed)) fprintf(stderr, "cond ");
    if (caps & BIT(xcsemicondensed)) fprintf(stderr, "scond ");
    if (caps & BIT(xcexpanded)) fprintf(stderr, "exp ");
    if (caps & BIT(xcproportional)) fprintf(stderr, "prop ");
    if (caps & BIT(xcmonospace)) fprintf(stderr, "mono ");
    if (caps & BIT(xcchar)) fprintf(stderr, "char ");

}

/** ****************************************************************************

Default event handler

If we reach this event handler, it means none of the overriders has handled the
event, but rather passed it down. We flag the event was not handled and return,
which will cause the event to return to the event() caller.

*******************************************************************************/

static void defaultevent(pa_evtrec* ev)

{

    /* set not handled and exit */
    ev->handled = 0;

}

/*******************************************************************************

Place string in storage

Places the given string into dynamic storage, and returns that.

*******************************************************************************/

static char* str(char* s)

{

    char* p;

    p = imalloc(strlen(s)+1);
    strcpy(p, s);

    return (p);

}

/******************************************************************************

Translate colors code

Translates an independent to a terminal specific primary RGB color code for
XWindow.

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

/*******************************************************************************

Translate rgb to XWindow color

Translates a ratioed INT_MAX graph color to the XWindow form, which is a 32
bit word with blue, green and red bytes.

*******************************************************************************/

static int rgb2xwin(int r, int g, int b)

{

   return ((r/8388608)*65536+(g/8388608)*256+(b/8388608));

}

/*******************************************************************************

Search for font by name

Finds a font in the list of fonts. Also matches fixed/no fixed pitch status.

*******************************************************************************/

static fontptr fndfnt(char* fn, int fix)

{

    fontptr p;
    fontptr fp;

    fp = NULL;
    p = fntlst; /* index top of font list */
    while (!fp && p) { /* traverse font list */

        if (!strcmp(p->fn, fn) && p->fix == fix) fp = p;
        else p = p->next; /* next entry */

    }

    return (fp); /* return found font */

}

/*******************************************************************************

Search for font by name

Finds a font in the list of fonts by name.

*******************************************************************************/

static fontptr schfnt(char* fn)

{

    fontptr p;
    fontptr fp;

    fp = NULL;
    p = fntlst; /* index top of font list */
    while (!fp && p) { /* traverse font list */

        if (!strcmp(p->fn, fn)) fp = p;
        else p = p->next; /* next entry */

    }

    return (fp); /* return found font */

}

/*******************************************************************************

Delete font

Deletes the given font entry from the global font list. Does not recycle it.

*******************************************************************************/

void delfnt(fontptr fp)

{

    fontptr flp;
    fontptr fl;

    if (fp == fntlst) fntlst = fntlst->next; /* gap from top of list */
    else { /* search whole list */

        /* find last pointer */
        flp = fntlst;
        fl = NULL; /* set no last */
        while (flp && flp != fp) { /* find last */

            fl = flp; /* set last */
            flp = flp->next; /* go next */

        }
        if (!fl) error(esystem); /* should have found it */
        fl->next = fp->next; /* gap over entry */

    }

}

/*******************************************************************************

Print font list

A diagnostic, prints the internal font list.

*******************************************************************************/

void prtfnt(void)

{

    fontptr  fp;
    int      c;
    xcaplst* cp;

    fp = fntlst;
    c = 1;
    while (fp) {

        dbg_printf(dlinfo, "Font %2d: %s Capabilities: ", c, fp->fn);
        prtxcset(fp->caps);
        fprintf(stderr, "\n");
        cp = fp->caplst;
        while (cp) {

            fprintf(stderr, "    ");
            prtxcset(cp->caps);
            fprintf(stderr, "\n");
            cp = cp->next;

        }
        fp = fp->next;
        c++;

    }

}

/*******************************************************************************

Preselect standard fonts

Selects the list of 1-4 standard fonts, and reorders the list so they are at the
top.

The standard fonts are selected by name in this version. This relies on XWindow
having a standard series of fonts that it carries with it. They could also be
selected by capabilities, but the sticking point there would be to determine
serif/sans serif, which does not have a formal capability.

We prefer 10646-1 (Unicode) fonts here, but will take 8859-1 (ISO international
8 bit).

It is an error in this version if we can't find all the standard fonts. This
means that programs don't need to account for missing fonts as in old style PA.
The programs that try to account for missing fonts still work, because the fonts
will never be missing.

In the current versions, the technical font is just a copy of the sign or sans
serif font. It used to be a vector stroke font, but that is no longer necessary.
We actually make a copy of the font entry to keep accounting correct. Its up to
the user to remove that in lists as required, and perhaps alphabetize the fonts.

Todo

1. Need to check fonts have the correct capabilities, ie., roman/no slant, etc.

*******************************************************************************/

void stdfont(void)

{

    fontptr nfl; /* new font list */
    fontptr fp; /* font pointer */
    fontptr sp; /* sign font pointer */

    /* select first 4 fonts for standard fonts */
    nfl = NULL; /* clear target list */

    /* search 1: terminal font */
    fp = fndfnt("bitstream: courier 10 pitch: iso10646-1", TRUE);
    if (fp) { /* found, enter as 1 */

        delfnt(fp); /* remove from source list */
        fp->next = nfl; /* insert to target list */
        nfl = fp;

    } else {

        fp = fndfnt("bitstream: courier 10 pitch: iso8859-1", TRUE);
        if (!fp) error(estdfnt); /* no font found */
        delfnt(fp); /* remove from source list */
        fp->next = nfl; /* insert to target list */
        nfl = fp;

    }

    /* search 2: book (serif) font */
    fp = fndfnt("bitstream: bitstream charter: iso10646-1", FALSE);
    if (fp) { /* found, enter as 2 */

        delfnt(fp); /* remove from source list */
        fp->next = nfl; /* insert to target list */
        nfl = fp;

    } else {

        fp = fndfnt("bitstream: bitstream charter: iso8859-1", FALSE);
        if (!fp) error(estdfnt); /* no font found */
        delfnt(fp); /* remove from source list */
        fp->next = nfl; /* insert to target list */
        nfl = fp;

    }

    /* search 3: sign (san serif) font */
    fp = fndfnt("unregistered: latin modern sans: iso8859-1", FALSE);
    if (!fp) error(estdfnt); /* no font found */
    delfnt(fp); /* remove from source list */
    fp->next = nfl; /* insert to target list */
    nfl = fp;
    sp = fp; /* save sign font */

    /* search 4: technical font, make copy of sign */
    fp = (fontptr)imalloc(sizeof(fontrec));
    fontcnt++;
    fonttot += sizeof(fontrec);

    /* copy sign font parameters */
    fp->fn = sp->fn;
    fp->fix = sp->fix;
    fp->caps = sp->caps;
    fp->caplst = sp->caplst;
    fp->next = nfl; /* insert to target list */
    nfl = fp;
    fntcnt++; /* add to font count */

    /* transfer all remaining entries to the font list */
    while (fntlst) {

        fp = fntlst;
        fntlst = fntlst->next; /* gap from list */
        fp->next = nfl; /* insert to new list */
        nfl = fp;

    }
    /* now insert back to master list, and reverse entries to order */
    while (nfl) {

        fp = nfl;
        nfl = nfl->next; /* gap from list */
        fp->next = fntlst; /* insert to new list */
        fntlst = fp;

    }

}

/*******************************************************************************

Load fonts list

Loads the XWindow font list. We only load scalable fonts, since PA has no
ability to adapt to fonts that don't scale. The font names are stripped to
their essential information, IE, the PA font names are not simply a copy of the
XWindow font naming system. We keep the fields needed to differentiate the
fonts. There is enough information in the font names (by defintion) to select
the active font by filling in the missing fields with wildcards or specific
sizing or attribute information.

Looking at the XWindow font name description, these are the dispositions of the
fields:

-foundry-family-weight-slant-width-pixels-points-hres-vres-spacing-width-cset-#

Foundry     Kept as part of the name.
Family      Kept as part of the name.
Weight      Selected as attribute.
Slant       Selected as attribute.
Width       Selected as attribute.
Pixels      Selected in scaling.
Points      Selected in scaling.
Hres        Selected in scaling.
Vres        Selected in scaling.
Spacing     Selected as attribute.
Width       Unused.
Cset        Kept as part of the name.
#           Kept as part of the name.

See the XWindow font name description for more.

*******************************************************************************/

/* get field by number */
string fldnum(string fp, int fn)

{

    int i;

    fp++; /* index 1st field */
    fn--;
    while (fn) {

        while (*fp && *fp != '-') fp++; /* skip to next '-' */
        fp++; /* skip over '-' */
        fn--; /* count */

    }

    return (fp); /* return with the string */

}

void getfonts(void)

{

    string*  fl;       /* font list */
    int      fc;       /* font count */
    string*  fp;       /* font pointer */
    int      ifc;      /* internal font count */
    char     buf[250]; /* buffer for string name */
    int      i;
    string   sp, dp;
    fontptr  flp;
    fontptr  nfl;
    xcaplst* xcl;

    /* load the fonts list */
    XWLOCK();
    fl = XListFonts(padisplay, "-*-*-*-*-*--0-0-0-0-?-0-*", INT_MAX, &fc);
    XWUNLOCK();

#if 0
    /* print the raw XWindow font list */
    fp = fl;
    for (i = 1; i <= fc; i++) {

        dbg_printf(dlinfo, "XWindow Font %d: %s\n", i, *fp);
        fp++;

    }
#endif

    fp = fl; /* index top of list */
    fntlst = NULL; /* clear destination list */
    ifc = 0; /* clear internal font counter */
    for (i = 1; i <= fc; i++) { /* process all fonts */

        /* reject character spaced fonts. I haven't seen reasonable metrics for
           those */
        sp = fldnum(*fp, 11); /* index spacing field */
        if (strncmp(sp, "c", 1)) {

            dp = buf; /* index result buffer */
            /* get foundry */
            sp = fldnum(*fp, 1);
            while (*sp && *sp != '-') *dp++ = *sp++; /* transfer character */
            *dp++ = ':';
            *dp++ = ' ';
            /* get font family */
            sp = fldnum(*fp, 2);
            while (*sp && *sp != '-') *dp++ = *sp++; /* transfer character */
            *dp++ = ':';
            *dp++ = ' ';
            /* get character set (2 parts) */
            sp = fldnum(*fp, 13);
            while (*sp && *sp != '-') *dp++ = *sp++; /* transfer character */
            *dp++ = '-';
            sp = fldnum(*fp, 14);
            while (*sp && *sp != '-') *dp++ = *sp++; /* transfer character */
            *dp++ = 0; /* terminate string */

            /* Search for duplicates. Since we removed the attributes, many
               entries will be duplicated. */
            flp = schfnt(buf);

            if (!flp) { /* entry is unique */

                /* create destination entry */
                flp = (fontptr)imalloc(sizeof(fontrec));
                fontcnt++;
                fonttot += sizeof(fontrec);
                flp->fn = (string)imalloc(strlen(buf)+1); /* get name string */
                strcpy(flp->fn, buf); /* copy name into place */
                flp->caps = 0; /* clear capabilities */
                flp->caplst = NULL; /* clear capabilities list */
                /* push to destination */
                flp->next = fntlst;
                fntlst = flp;
                ifc++; /* count internal fonts */

            }

            xcl = (xcaplst*)imalloc(sizeof(xcaplst));
            xcl->caps = 0; /* clear capabilities */
            xcl->next = flp->caplst; /* push to font cap list */
            flp->caplst = xcl;

            /* transfer font capabilties to flags */

            /* weight */
            sp = fldnum(*fp, 3);
            if (!strncmp(sp, "normal", 6)) xcl->caps |= BIT(xcnormal);
            if (!strncmp(sp, "medium", 6)) xcl->caps |= BIT(xcmedium);
            if (!strncmp(sp, "bold", 4)) xcl->caps |= BIT(xcbold);
            if (!strncmp(sp, "demi bold", 9)) xcl->caps |= BIT(xcdemibold);
            if (!strncmp(sp, "dark", 4)) xcl->caps |= BIT(xcdark);
            if (!strncmp(sp, "light", 5)) xcl->caps |= BIT(xclight);

            /* slants */
            sp = fldnum(*fp, 4);
            if (!strncmp(sp, "r", 1)) xcl->caps |= BIT(xcroman);
            if (!strncmp(sp, "i", 1)) xcl->caps |= BIT(xcital);
            if (!strncmp(sp, "o", 1)) xcl->caps |= BIT(xcoblique);
            if (!strncmp(sp, "ri", 2)) xcl->caps |= BIT(xcrital);
            if (!strncmp(sp, "ro", 2)) xcl->caps |= BIT(xcroblique);

            /* widths */
            sp = fldnum(*fp, 5);
            if (!strncmp(sp, "normal", 6)) xcl->caps |= BIT(xcnormalw);
            if (!strncmp(sp, "narrow", 6)) xcl->caps |= BIT(xcnarrow);
            if (!strncmp(sp, "condensed", 9)) xcl->caps |= BIT(xccondensed);
            if (!strncmp(sp, "semicondensed", 13))
                xcl->caps |= BIT(xcsemicondensed);
            if (!strncmp(sp, "expanded", 8)) xcl->caps |= BIT(xcexpanded);

            /* spacing */
            sp = fldnum(*fp, 11); /* index spacing field */
            if (!strncmp(sp, "p", 1)) xcl->caps |= BIT(xcproportional);
            if (!strncmp(sp, "m", 1)) xcl->caps |= BIT(xcmonospace);
            if (!strncmp(sp, "c", 1)) xcl->caps |= BIT(xcchar);

            /* form set of all capabilities */
            flp->caps |= xcl->caps;

            /* set our font flags based on that */
            flp->fix = flp->caps & BIT(xcmonospace) || flp->caps & BIT(xcchar);

        }
        fp++; /* next source font entry */

    }
    XWLOCK();
    XFreeFontNames(fl); /* release the font list */
    XWUNLOCK();

    fntcnt = ifc; /* set internal font count */

    /* select the standard fonts */
    stdfont();

#ifdef PRTFNT
    /* print resulting font list */
    dbg_printf(dlinfo, "Internal font list:\n");
    prtfnt();
    fflush(stderr);
#endif

}

/*******************************************************************************

Create XWindow XLFD font select string

Creates a font select string in XWindow XLFD format from the given XWindow
capabilities, and a given pixel height.

*******************************************************************************/

void selxlfd(winptr win, int caps, string buf, int ht)

{

    char* bp;       /* buffer pointer */
    char* np;       /* font name pointer */
    fontptr fp;     /* pointer to new font */

    fp = win->gcfont; /* get new font to select */
    /* (re)construct XWindow font name */
    bp = buf; /* index buffer */
    np = fp->fn; /* index name */
    *bp++ = '-'; /* start format */

    /* foundry */
    while (*np && *np != ':') *bp++ = *np++;
    np += 2;
    *bp++ = '-';

    /* family name */
    while (*np && *np != ':') *bp++ = *np++;
    np += 2;
    *bp++ = '-';

    /* weight */
    if (caps & BIT(xcnormal)) { strcpy(bp, "normal"); bp += 6; }
    else if (caps & BIT(xcmedium)) { strcpy(bp, "medium"); bp += 6; }
    else if (caps & BIT(xcbold)) { strcpy(bp, "bold"); bp += 4; }
    else if (caps & BIT(xcdemibold)) { strcpy(bp, "demi bold"); bp += 9; }
    else if (caps & BIT(xcdark)) { strcpy(bp, "dark"); bp += 4; }
    else if (caps & BIT(xclight)) { strcpy(bp, "light"); bp += 5; }
    *bp++ = '-';

    /* slant */
    if (caps & BIT(xcroman)) { strcpy(bp, "r"); bp += 1; }
    if (caps & BIT(xcital)) { strcpy(bp, "i"); bp += 1; }
    else if (caps & BIT(xcoblique)) { strcpy(bp, "o"); bp += 1; }
    else if (caps & BIT(xcrital)) { strcpy(bp, "ri"); bp += 2; }
    else if (caps & BIT(xcroblique)) { strcpy(bp, "ro"); bp += 2; }
    *bp++ = '-';

    /* widths */
    if (caps & BIT(xcnormalw)) { strcpy(bp, "normal"); bp += 6; }
    else if (caps & BIT(xcnarrow)) { strcpy(bp, "narrow"); bp += 6; }
    else if (caps & BIT(xccondensed)) { strcpy(bp, "condensed"); bp += 9; }
    else if (caps & BIT(xcsemicondensed)) { strcpy(bp, "semicondensed"); bp += 13; }
    else if (caps & BIT(xcexpanded)) { strcpy(bp, "expanded"); bp += 8; }
    *bp++ = '-';

    /* additional style (empty) */
    *bp++ = '-';

    /* pixel size */
    bp += sprintf(bp, "%d", ht);
    *bp++ = '-';

    /* point size */
    *bp++ = '*';
    *bp++ = '-';

    /* resolution X */
    *bp++ = '*';
    *bp++ = '-';

    /* resolution Y */
    *bp++ = '*';
    *bp++ = '-';

    /* spacing  */
    if (caps & BIT(xcproportional)) { strcpy(bp, "p"); bp += 1; }
    else if (caps & BIT(xcmonospace)) { strcpy(bp, "m"); bp += 1; }
    else if (caps & BIT(xcchar)) { strcpy(bp, "c"); bp += 1; }
    *bp++ = '-';

    /* average width */
    *bp++ = '*';
    *bp++ = '-';

    /* registry and encoding */
    while (*np) *bp++ = *np++;

    *bp = 0; /* terminate */

}

/*******************************************************************************

Find requested XWindows font capabilities

Finds the set of XWindows capabilities that are requested in the set of PA
attributes.

*******************************************************************************/

int fndxcap(int caps, int at)

{

    int ncaps;   /* XWindow font capabilities */

    ncaps = 0; /* clear result */

    /* weight */
    if (at & BIT(sabold) && caps & BIT(xcbold)) ncaps |= BIT(xcbold);
    else if (at & BIT(salight) && caps & BIT(xclight)) ncaps |= BIT(xclight);

    /* slant */
    if (at & BIT(saital) && caps & BIT(xcital)) ncaps |= BIT(xcital);

    /* widths */
    if (at & BIT(sacondensed) && caps & BIT(xccondensed))
        ncaps |= BIT(xccondensed);
    else if (at & BIT(saextended) && caps & BIT(xcexpanded))
        ncaps |= BIT(xcexpanded);

    return (ncaps); /* return new capabilities */

}

/*******************************************************************************

Select attributes by priority

Given a set of Petit-Ami attributes, finds a set of XWindows capabilities from
the list of possible capabilities by selecting all of the possible attributes,
then removing attributes by lowest priority until a match is found. This is
required because the XWindow capability set may have conflicting attributes. For
example, bold may have to have extended due to the new size of the font.

Returns the resulting set of XWindow capabilities as a set.

*******************************************************************************/

/* find number of bits in set */
int bitcnt(int i)

{

    int c, b;

    c = 0;
    b = 1;
    while (b >= 0) { /* this will overflow negative */

        if (i & b) c++;
        b <<= 1;

    }

}

/* find if capabilities set matches one from list */
int matchcap(int caps, xcaplst* cl, int* mc)

{

    int fnd; /* found/not found flag */
    int bn, bn2;  /* bit number */

    fnd = FALSE; /* set not found */
    bn = INT_MAX; /* set bit number maximum */
    while (cl) {

        /* all of the requested capability bits must also exist in the target
           capabilities */
        if ((cl->caps&caps) == caps) {

            /* find number of configuring bits in new set */
            bn2 = bitcnt(cl->caps & cfgcap);
            if (bn2 < bn) {

                /* fewer side effects */
                fnd = TRUE; /* set found */
                *mc = cl->caps; /* return what was matched */
                bn = bn2; /* set new minimum */

            }

        }
        cl = cl->next;

    }

    return (fnd); /* return match/no match */

}

int fndxcapp(fontptr fp, int at)

{

    /* capabilities list in lowest to highest priority */
    const int cappri[] = {

        sablink,     /* blinking text (foreground) */
        saxlight,    /* extra light */
        saxbold,     /* bold */
        salight,     /* light */
        sarev,       /* reverse video */
        saundl,      /* underline */
        sasuper,     /* superscript */
        sasubs,      /* subscripting */
        sastkout,    /* strikeout text */
        sahollow,    /* hollow */
        saraised,    /* raised */
        sacondensed, /* condensed */
        saextended,  /* extended */
        saital,      /* italic text */
        sabold,      /* bold text */
        INT_MAX      /* end */

    };

    int caps;
    int match;
    int ia, lia;
    int mc;

    ia = 0;
    match = FALSE;
//dbg_printf(dlinfo, "full caps: "); prtxcset(fp->caps); fprintf(stderr, "\n"); fflush(stderr);
    do { /* search capabilities */

//dbg_printf(dlinfo, "at: "); prtatset(at); fprintf(stderr, "\n"); fflush(stderr);
        /* find capabilities from this set of attributes */
        caps = fndxcap(fp->caps, at);
//dbg_printf(dlinfo, "caps: "); prtxcset(caps); fprintf(stderr, "\n"); fflush(stderr);
        if (matchcap(caps, fp->caplst, &mc)) match = TRUE; /* found a match */
        else { /* try again */

//dbg_printf(dlinfo, "Will remove: "); prtatset(BIT(cappri[ia])); fprintf(stderr, "\n"); fflush(stderr);
            at &= ~BIT(cappri[ia]); /* remove attribute by priority */
            lia = ia; /* save select */
            ia++; /* next attribute */

        }

    } while (!match &&
             (cappri[lia] != INT_MAX)); /* until found or no more attributes */
    /* if we still have not found anything, it has to be a system error, since
       we all attributes */
    if (!match) error(esystem);
//dbg_printf(dlinfo, "matching caps: "); prtxcset(mc); fprintf(stderr, "\n"); fflush(stderr);

    return (mc); /* return matching caps */

}

/*******************************************************************************

Select font

Sets the currently selected font active. Processes all current attributes as
applicable, if the XWindow font set has that capability. Capability must be an
exact match, that is, capability indicates an actual XWindows font with that
capability exists. If any capability is not found, we default to blank or no
characters in the field.

The translation of attributes to capabilities is done on a priority basis. For
example, bold and light are mutually exclusive, and bold is prioritized.

*******************************************************************************/

void setfnt(winptr win)

{

    char    buf[250]; /* construction buffer for X font names */
    char*   bp;       /* buffer pointer */
    char*   np;       /* font name pointer */
    fontptr fp;       /* pointer to new font */
    int     aht;      /* found height of font */
    int     ht;       /* requested height of font */
    int     caps;     /* XWindows capabilities set */

    /* release any existing font */
    if (win->xfont) {

        XWLOCK();
        XFreeFont(padisplay, win->xfont);
        XWUNLOCK();

    }

    /* Find matching XWindow capabilities set */
    caps = fndxcapp(win->gcfont, win->gattr);

    /* XWindows does not select the pixel height by the true bounding box of the
       font, defined by "a box that contains all pixels drawn by any character
       in the font", so we must search to find the actual size. */
    ht = win->gfhigh; /* set starting request size */
    do { /* try font sizes */

        selxlfd(win, caps, buf, ht); /* form XLFD selection string */
        XWLOCK();
        win->xfont = XLoadQueryFont(padisplay, buf);
        XWUNLOCK();
        if (!win->xfont) error(esystem); /* should have found it */
        aht = win->xfont->ascent+win->xfont->descent; /* find resulting height */
        ht--;
        /* if we are going to try again, free up the trial font */
        if (aht > win->gfhigh) XFreeFont(padisplay, win->xfont);

    } while (aht > win->gfhigh);

//dbg_printf(dlinfo, "XLFD font select string: %s\n", buf);
    if (prtftm) {

        dbg_printf(dlinfo, "Font ascent:  %d\n", win->xfont->ascent);
        dbg_printf(dlinfo, "Font descent: %d\n", win->xfont->descent);

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

    }

    /* find spacing in current font */
    win->charspace = win->xfont->max_bounds.width;
    /* because the search solution above could find a font smaller than the given
       bounding box, we use the requested vs. actual font height */
    win->linespace = win->gfhigh;
    win->chrspcx = 0; /* reset leading and spacing */
    win->chrspcy = 0;

    /* find base offset */
    win->baseoff = win->xfont->ascent;

    if (prtftm) {

        dbg_printf(dlinfo, "Width of character cell:  %d\n", win->charspace);
        dbg_printf(dlinfo, "Height of character cell: %d\n", win->linespace);
        dbg_printf(dlinfo, "Base offset:              %d\n", win->baseoff);

    }

}

/*******************************************************************************

Find width of character in XWindow

Finds and returns the width of a character in XWindow. Normally used for
proportional fonts.

*******************************************************************************/

int xwidth(winptr win, char c)

{

    /* only use the simple calculation */
    if (!win->xfont->per_char) error(esystem);
    if (win->xfont->min_byte1) error(esystem);
    if (win->xfont->min_char_or_byte2 != 0) error(esystem);

    return (win->xfont->per_char[c].width);

}

/*******************************************************************************

Append menu entry

Appends a new menu entry to the given list.

*******************************************************************************/

static void appendmenu(pa_menuptr* list, pa_menuptr m)

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

Check in display mode

Checks if the current update screen is also the current display screen. Returns
TRUE if so. If the screen is in display, it means that all of the actions to
the update screen should also be reflected on the real screen.

*******************************************************************************/

static int indisp(winptr win)

{

    return (win->curupd == win->curdsp);

}

/*******************************************************************************

Clear screen buffer

Clears the entire screen buffer to spaces with the current colors and
attributes.

*******************************************************************************/

static void clrbuf(scnptr sc)

{

    XWLOCK();
    XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
    XFillRectangle(padisplay, sc->xbuf, sc->xcxt, 0, 0, sc->maxxg, sc->maxyg);
    XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Get file entry

Allocates and initalizes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

static void getfil(filptr* fp)

{

    *fp = imalloc(sizeof(filrec)); /* get new file entry */
    filcnt++;
    filtot += sizeof(filrec);
    (*fp)->win = NULL; /* set no window */
    (*fp)->inw = FALSE; /* clear input window link */
    (*fp)->inl = -1; /* set no input file linked */
    (*fp)->tim = 0; /* set no timer associated with it */
    (*fp)->twin = NULL; /* set no window for timer */

}

/** ****************************************************************************

Get picture entry

Allocates and returns a new picture entry.

*******************************************************************************/

static picptr getpic(void)

{

    picptr pp; /* pointer to picture entry */

    if (frepic) { /* there is a free entry */

        pp = frepic; /* index top free */
        frepic = pp->next; /* gap out */

    } else { /* allocate new one */

        pp = imalloc(sizeof(pict)); /* get new file entry */
        piccnt++;
        pictot += sizeof(pict);

    }
    pp->xi = NULL; /* set no image */
    pp->next = NULL; /* set no next */

    return (pp); /* return entry */

}

/** ****************************************************************************

Put picture entry

Frees a picture entry.

*******************************************************************************/

static void putpic(picptr pp)

{

    pp->next = frepic; /* push to free list */
    frepic = pp;

}

/** ****************************************************************************

Delete picture entries

Deletes all the scaled copies of the picture by number. No error checking is
done.

*******************************************************************************/

static void delpic(winptr win, int p)

{

    picptr pp; /* pointer to picture entries */

    while (win->pictbl[p-1]) {

        /* remove top entry */
        pp = win->pictbl[p-1];
        win->pictbl[p-1] = pp->next;
        XWLOCK();
        XDestroyImage(pp->xi); /* release image */
        XWUNLOCK();
        putpic(pp); /* release image entry */

    }

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

/*******************************************************************************

Get logical file number from file

Gets the logical translated file number from a text file, and verifies it
is valid.

*******************************************************************************/

static int txt2lfn(FILE* f)

{

    int fn;

    fn = fileno(f); /* get file id */
    if (fn < 0) error(einvfil); /* invalid */

    return (fn); /* return result */

}

/*******************************************************************************

Get input side file from output file

Returns the input side file attached to the given output file.

*******************************************************************************/

static FILE* out2inp(FILE* f)

{

    int fn;

    fn = fileno(f); /* get file id */
    if (fn < 0) error(einvfil); /* file invalid */
    if (!opnfil[fn]) error(einvhan); /* invalid handle */
    if (!opnfil[fn]->win) error(efnotwin); /* not a window file */
    if (opnfil[fn]->inl < 0) error(enoinps); /* no input side for this window */
    fn = opnfil[fn]->inl; /* link input side */
    if (!opnfil[fn]->sfp) error(enoinps); /* no input side for this window */

    return (opnfil[fn]->sfp); /* return that */

}

/*******************************************************************************

Rescale XWindows image

Rescales an image from the source to the destination. Both scaling up and down
are possible, as well as changing the aspect ratio. Uses bilinear interpolation.

*******************************************************************************/

void rescale(XImage* dp, XImage* sp)

{

    unsigned int px1, px2, px3, px4;
    int sx, sy, dx, dy;
    float xr, yr;
    int xd, yd;
    int b, r, g;
    unsigned int* src;
    unsigned int* dest;
    int si, di;

    xr = ((float)(sp->width-1))/dp->width; /* find scaling ratio x */
    yr = ((float)(sp->height-1))/dp->height; /* find scaling ratio y */
    src = (unsigned int*)(sp->data); /* index source pixmap */
    dest = (unsigned int*)(dp->data); /* index destination pixmap */
    di = 0; /* set destination index */

    /* copy and scale source to destination */
    for (dy = 0; dy < dp->height; dy++) {

        for (dx = 0; dx < dp->width; dx++) {

            sx = xr*dx; /* find source x location */
            sy = yr*dy; /* find source y location */
            xd = (xr*dx)-sx;
            yd = (yr*dy)-sy;
            si = (sy*sp->width+sx); /* find net source index */
            px1 = src[si]; /* get this pixel */
            px2 = src[si+1]; /* get right pixel */
            px3 = src[si+sp->width]; /* get down pixel */
            px4 = src[si+sp->width+1]; /* get down/right pixel */

            b = (px1&0xff)*(1-xd)*(1-yd)+(px2&0xff)*xd*(1-yd)+
                   (px3&0xff)*yd*(1-xd)+(px4&0xff)*xd*yd;

            g = ((px1>>8)&0xff)*(1-xd)*(1-yd)+((px2>>8)&0xff)*xd*(1-yd)+
                    ((px3>>8)&0xff)*yd*(1-xd)+((px4>>8)&0xff)*xd*yd;

            r = ((px1>>16)&0xff)*(1-xd)*(1-yd)+((px2>>16)&0xff)*xd*(1-yd)+
                  ((px3>>16)&0xff)*(yd)*(1-xd)+((px4>>16)&0xff)*xd*yd;

            dest[di++] = 0xff000000|r<<16&0xff0000|g<<8&0xff00|b;

        }

    }

}

/** ****************************************************************************

Convert Petit-Ami ratioed angles to XWindow 64ths angles

PA uses 0 to INT_MAX clockwise angular measurements. Xwindows uses 64 degree
measurements, with 0 degress at the 3'oclock position, and counterclockwise
drawing direction. Converts PA angles to XWindow angles.

*******************************************************************************/

#define DEGREE (INT_MAX/360)

int rat2a64(int a)

{

    /* normalize for 90 degrees origin */
    a -= INT_MAX/4; /* normalize for 90 degrees */
    if (a < 0) a += INT_MAX;
    a /= INT_MAX/(360*64); /* convert to 64ths degrees */
    if (a) a = 360*64-a;

    return (a); /* return counterclockwise */

}


/** ****************************************************************************

Print XEvent message

A diagnostic, prints fields in an XEvent message.

*******************************************************************************/

void prtxevt(XEvent* e)

{

        fprintf(stderr, "X Event: %5ld Window: %lx ", e->xany.serial,
                e->xany.window);
        prtxevtt(e->type); fprintf(stderr, "\n"); fflush(stderr);

}

/** ****************************************************************************

Get freed/new window structure

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static winptr getwin(void)

{

    winptr p;

    if (winfre) { /* there is a freed entry */

        p = winfre; /* index top entry */
        winfre = p->next; /* gap from list */

    } else {

        p = imalloc(sizeof(winrec));
        wincnt++; /* count entries */
        wintot += sizeof(winrec); /* add to total memory used */

    }

    return (p);

}

/** ****************************************************************************

Get freed/new window structure

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static void putwin(winptr p)

{

    p->next = winfre; /* push to list */
    winfre = p;

}

/** ****************************************************************************

Get freed/new queue entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static xevtque* getxevt(void)

{

    xevtque* p;

    if (freque) { /* there is a freed entry */

        p = freque; /* index top entry */
        freque = p->next; /* gap from list */

    } else p = (xevtque*)imalloc(sizeof(xevtque));

    return (p);

}

/** ****************************************************************************

Get freed/new queue entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static void putxevt(xevtque* p)

{

    p->next = freque; /* push to list */
    freque = p;

}

/** ****************************************************************************

Place XEvent into input queue

*******************************************************************************/

static void enquexevt(XEvent* e)

{

    xevtque* p;

    p = getxevt(); /* get a queue entry */
    memcpy(&p->evt, e, sizeof(XEvent)); /* copy event to queue entry */
    if (evtque) { /* there are entries in queue */

        /* we push TO next (current) and take FROM last (final) */
        p->next = evtque; /* link next to current entry */
        p->last = evtque->last; /* link last to final entry */
        evtque->last = p; /* link current to this */
        p->last->next = p; /* link final to this */
        evtque = p; /* point to new entry */

    } else { /* queue is empty */

        p->next = p; /* link to self */
        p->last = p;
        evtque = p; /* place in list */

    }

}

/** ****************************************************************************

Remove XEvent from input queue

*******************************************************************************/

static void dequexevt(XEvent* e)

{

    xevtque* p;

    if (!evtque) error(esystem); /* should not be called empty */
    /* we push TO next (current) and take FROM last (final) */
    p = evtque->last; /* index final entry */
    if (p->next == p) evtque = NULL; /* only one entry, clear list */
    else { /* other entries */

        p->last->next = p->next; /* point last at current */
        p->next->last = p->last; /* point current at last */

    }
    memcpy(e, &p->evt, sizeof(XEvent)); /* copy out to caller */
    putxevt(p); /* release queue entry to free */

}

/** ****************************************************************************

Find window output file number from XWindow handle

Search file table for XWindow handle and returns the table index corresponding,
or -1 if not found.

*******************************************************************************/

static int fndevt(Window w)

{

    int fi; /* index for file table */
    int ff; /* found file */

    fi = 0; /* start index */
    ff = -1; /* set no file found */
    while (fi < MAXFIL) {

        if (opnfil[fi] && opnfil[fi]->win &&
            (opnfil[fi]->win->xmwhan == w || opnfil[fi]->win->xwhan == w)) {

            ff = fi; /* set found */
            fi = MAXFIL; /* terminate */

        } else fi++; /* next entry */

    }

    return (ff);

}

/** ****************************************************************************

Wait response message

Looks into the XWindows events until a given respose is found. Events are placed
back into the input via a queue so that we don't discard important events like
expose.

Should have timeouts.

*******************************************************************************/

static void peekxevt(XEvent* e)

{

    XWLOCK();
    XNextEvent(padisplay, e); /* get next event */
    XWUNLOCK();
    enquexevt(e); /* place in input queue */
    /* there is another diagnostic in pa_event(), but you might want to see
       these events immediately */
    //dbg_printf(dlinfo, ""); prtxevt(e);

}

/** ****************************************************************************

Get freed/new PA queue entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static paevtque* getpaevt(void)

{

    paevtque* p;

    if (paqfre) { /* there is a freed entry */

        p = paqfre; /* index top entry */
        paqfre = p->next; /* gap from list */

    } else p = (paevtque*)imalloc(sizeof(paevtque));

    return (p);

}

/** ****************************************************************************

Get freed/new PA queue entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static void putpaevt(paevtque* p)

{

    p->next = paqfre; /* push to list */
    paqfre = p;

}

/** ****************************************************************************

Place PA event into input queue

*******************************************************************************/

static void enquepaevt(pa_evtrec* e)

{

    paevtque* p;

    p = getpaevt(); /* get a queue entry */
    memcpy(&p->evt, e, sizeof(pa_evtrec)); /* copy event to queue entry */
    if (paqevt) { /* there are entries in queue */

        /* we push TO next (current) and take FROM last (final) */
        p->next = paqevt; /* link next to current entry */
        p->last = paqevt->last; /* link last to final entry */
        paqevt->last = p; /* link current to this */
        p->last->next = p; /* link final to this */
        paqevt = p; /* point to new entry */

    } else { /* queue is empty */

        p->next = p; /* link to self */
        p->last = p;
        paqevt = p; /* place in list */

    }

}

/** ****************************************************************************

Remove XEvent from input queue

*******************************************************************************/

static void dequepaevt(pa_evtrec* e)

{

    paevtque* p;

    if (!paqevt) error(esystem); /* should not be called empty */
    /* we push TO next (current) and take FROM last (final) */
    p = paqevt->last; /* index final entry */
    if (p->next == p) paqevt = NULL; /* only one entry, clear list */
    else { /* other entries */

        p->last->next = p->next; /* point last at current */
        p->next->last = p->last; /* point current at last */

    }
    memcpy(e, &p->evt, sizeof(pa_evtrec)); /* copy out to caller */
    putpaevt(p); /* release queue entry to free */

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

Draw reversing cursor

Draws a cursor rectangle in xor mode. This is used both to place and remove the
cursor.

*******************************************************************************/

static void curdrw(winptr win)

{

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curupd-1]; /* index current update screen */
    XWLOCK();
    XSetForeground(padisplay, sc->xcxt, colnum(pa_white));
    XSetFunction(padisplay, sc->xcxt, GXxor); /* set reverse */
    XFillRectangle(padisplay, win->xwhan, sc->xcxt, sc->curxg-1, sc->curyg-1,
                   win->charspace, win->linespace);
    XSetFunction(padisplay, sc->xcxt, GXcopy); /* set overwrite */
    if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

}

/*******************************************************************************

Set cursor visable

Makes the cursor visible.

*******************************************************************************/

static void curon(winptr win)

{

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curdsp-1]; /* index current screen */
    if (!win->fcurdwn && sc->curv && icurbnd(sc) && win->focus)  {

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

    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curdsp-1]; /* index current screen */
    if (win->fcurdwn && sc->curv && icurbnd(sc) && win->focus)  {

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

static void restore(winptr win) /* window to restore */

{

    int rgb;
    scnptr sc;

    sc = win->screens[win->curdsp-1]; /* index screen */
    if (win->bufmod && win->visible)  { /* buffered mode is on, and visible */

        curoff(win); /* hide the cursor for drawing */
        /* set colors and attributes */
        if (BIT(sarev) & sc->attr)  { /* reverse */

            XWLOCK();
            XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            XSetBackground(padisplay, sc->xcxt, sc->fcrgb);
            XWUNLOCK();

        } else {

            XWLOCK();
            XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
            XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            XWUNLOCK();

        }
        /* copy buffer to screen */
        XWLOCK();
        XCopyArea(padisplay, sc->xbuf, win->xwhan, sc->xcxt, 0, 0,
                  sc->maxxg, sc->maxyg, 0, 0);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }

}

/*******************************************************************************

Display window

Presents a window, and sends it a first paint message. Used to process the
delayed window display function.

*******************************************************************************/

static void winvis(winptr win)

{

    XEvent e; /* XWindow event */

#ifndef NOWDELAY
    /* present the master window onscreen */
    XWLOCK();
    XMapWindow(padisplay, win->xmwhan);
    XFlush(padisplay);
    XWUNLOCK();

    /* wait for the window to be displayed */
    do { peekxevt(&e);
    } while (e.type !=  MapNotify || e.xany.window != win->xmwhan);

    /* present the subclient window onscreen */
    XWLOCK();
    XMapWindow(padisplay, win->xwhan);
    XFlush(padisplay);
    XWUNLOCK();

    /* wait for the window to be displayed */
    do { peekxevt(&e); } while (e.type !=  MapNotify || e.xany.window != win->xwhan);

    win->visible = TRUE; /* set now visible */
    restore(win); /* restore window */
#endif

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
    int      depth;

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
    for (i = 0; i < MAXTAB; i++) sc->tab[i] = 0; /* clear tab array */
    /* set up tabbing to be on each 8th position */
    i = 9; /* set 1st tab position */
    x = 0; /* set 1st tab slot */
    while (i < sc->maxx && x < MAXTAB) {

        sc->tab[x] = (i-1)*win->charspace+1;  /* set tab */
        i = i+8; /* next tab */
        x = x+1;

    }

    /* create graphics context for screen */
    XWLOCK();
    sc->xcxt = XCreateGC(padisplay, win->xwhan, 0, NULL);
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();

    /* set colors && attributes */
    if (BIT(sarev) & sc->attr) { /* reverse */

        XWLOCK();
        XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        XWUNLOCK();

    } else {

        XWLOCK();
        XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        XWUNLOCK();

    }

    XWLOCK();
    /* set line attributes */
    XSetLineAttributes(padisplay, sc->xcxt, 1, LineSolid, CapButt, JoinMiter);

    /* set up pixmap backing buffer */
    depth = DefaultDepth(padisplay, pascreen);
    sc->xbuf = XCreatePixmap(padisplay, win->xwhan, sc->maxxg, sc->maxyg, depth);
    XWUNLOCK();

    /* save buffer size */
    win->bufx = win->gmaxx;
    win->bufy = win->gmaxy;
    win->bufxg = win->gmaxxg;
    win->bufyg = win->gmaxyg;

    /* clear it */
    clrbuf(sc);

#if 0
    /* draw grid for character cell diagnosis */
    XWLOCK();
    XSetForeground(padisplay, sc->xcxt, colnum(pa_cyan));
    for (y = 0; y < sc->maxyg; y += win->linespace)
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, 0, y, sc->maxxg, y);
    for (x = 0; x < sc->maxxg; x += win->charspace)
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x, 0, x, sc->maxyg);
    XSetForeground(padisplay, sc->xcxt, colnum(pa_black));
    XWUNLOCK();
#endif

#if 0
    /* reveal the background (diagnostic) */
    XWLOCK();
    XSetBackground(padisplay, sc->xcxt, colnum(pa_yellow));
    XWUNLOCK();
#endif

}

/*******************************************************************************

Disinitalize screen

Removes all data for the given screen buffer.

*******************************************************************************/

static void disscn(winptr win, scnptr sc)

{

    /* need to do disposals here */

}

/** ****************************************************************************

Open and present window

Given a windows file id and an (optional) parent window file id opens and
presents the window associated with it. All of the screen buffer data is
cleared, and a single buffer assigned to the window.

*******************************************************************************/

/* create window without background draw */

static Window createwindow(Window parent, int x, int y)

{

    Window w; /* XWindow handle */
    XEvent e; /* XWindow event */

    /* create our window with no background */
    XWLOCK();
    w = XCreateWindow(padisplay, parent, 0, 0, x, y, 0, CopyFromParent,
                      InputOutput, CopyFromParent, 0, NULL);

    /* select what events we want */
    XSelectInput(padisplay, w, ExposureMask|KeyPressMask|
                 KeyReleaseMask|PointerMotionMask|ButtonPressMask|
                 ButtonReleaseMask|StructureNotifyMask|FocusChangeMask);
    XWUNLOCK();

/* now handled in winvis */
#ifdef NOWDELAY
    /* present the window onscreen */
    XWLOCK();
    XMapWindow(padisplay, w);
    XFlush(padisplay);
    XWUNLOCK();

    /* wait for the window to be displayed */
    do { peekxevt(&e); } while (e.type !=  MapNotify || e.xany.window != w);
#endif

    return (w);

}

static void opnwin(int fn, int pfn, int wid)

{

    int                  r;     /* result holder */
    int                  b;     /* int result holder */
    pa_evtrec            er;    /* event holding record */
    int                  ti;    /* index for repeat array */
    int                  pin;   /* index for loadable pictures array */
    int                  si;    /* index for current display screen */
    winptr               win;   /* window pointer */
    winptr               pwin;  /* parent window pointer */
    XSetWindowAttributes xwsa;  /* XWindow set attributes */
    XWindowAttributes    xwga, xpwga; /* XWindow get attributes */
    XEvent               e;     /* XWindow event */
    char                 buf[250];
    Window               pw, rw;
    Window*              cwl;
    int                  ncw;

    win = lfn2win(fn); /* get a pointer to the window */
    /* find parent */
    win->parlfn = pfn; /* set parent logical number */
    win->wid = wid; /* set window id */
    pwin = NULL; /* set no parent */
    if (pfn >= 0) pwin = lfn2win(pfn); /* index parent window */
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
    win->focus = TRUE /*FALSE*/; /* set not in focus */
    win->joy1xs = 0; /* clear joystick saves */
    win->joy1ys = 0;
    win->joy1zs = 0;
    win->joy2xs = 0;
    win->joy2ys = 0;
    win->joy2zs = 0;
    win->inpptr = -1; /* set buffer empty */
    win->frmrun = FALSE; /* set framing timer not running */
    win->bufmod = TRUE; /* set buffering on */
    win->metlst = NULL; /* clear menu tracking list */
    win->menu = NULL; /* set menu bar not active */
    win->frame = TRUE; /* set frame on */
    win->size = TRUE; /* set size bars on */
    win->sysbar = TRUE; /* set system bar on */
    win->sizests = 0; /* clear last size status word */
    /* clear timer repeat array */
    for (ti = 0; ti < 10; ti++) win->timers[ti] = -1;
    /* clear loadable pictures table */
    for (pin = 0; pin < MAXPIC; pin++) win->pictbl[pin] = NULL;
    /* clear the screen array */
    for (si = 0; si < MAXCON; si++) win->screens[si] = NULL;
    win->screens[0] = imalloc(sizeof(scncon)); /* get the default screen */
    scncnt++;
    scntot += sizeof(scncon);
    win->curdsp = 1; /* set current display screen */
    win->curupd = 1; /* set current update screen */
    win->visible = FALSE; /* set not visible */

    /* set up global buffer parameters */
    win->gmaxx = maxxd; /* character max dimensions */
    win->gmaxy = maxyd;
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

    win->xmwhan = 0; /* clear the XWindow handles */
    win->xwhan = 0;

    /* get screen parameters */
    XWLOCK();
    win->shsize = DisplayWidthMM(padisplay, pascreen); /* size x in millimeters */
    win->svsize = DisplayHeightMM(padisplay, pascreen); /* size y in millimeters */
    win->shres = DisplayWidth(padisplay, pascreen);
    win->svres = DisplayHeight(padisplay, pascreen);
    XWUNLOCK();
    win->sdpmx = win->shres*1000/win->shsize; /* find dots per meter x */
    win->sdpmy = win->svres*1000/win->svsize; /* find dots per meter y */

#ifdef PRTPWM
    dbg_printf(dlinfo, "Display width in pixels:  %d\n", win->shres);
    dbg_printf(dlinfo, "Display height in pixels: %d\n", win->svres);
    dbg_printf(dlinfo, "Display width in mm:      %d\n", win->shsize);
    dbg_printf(dlinfo, "Display height in mm:     %d\n", win->svsize);
    dbg_printf(dlinfo, "Dots per meter x:         %d\n", win->sdpmx);
    dbg_printf(dlinfo, "Dots per meter y:         %d\n", win->sdpmy);
#endif

    win->gcfont = fntlst; /* index terminal font entry */
    win->gfhigh = (int)(CONPNT*POINT*win->sdpmy/1000); /* set font height */
    win->xfont = NULL; /* clear current font */
    setfnt(win); /* select font */

    /* set buffer size required for character spacing at default character grid
       size */
    win->gmaxxg = maxxd*win->charspace;
    win->gmaxyg = maxyd*win->linespace;

    /* set XWindow display origins sizes and sizes */
    win->xmwr.x = 0;
    win->xmwr.y = 0;
    win->xmwr.w = win->gmaxxg;
    win->xmwr.h = win->gmaxyg;

    win->xwr.x = 0;
    win->xwr.y = 0;
    win->xwr.w = win->gmaxxg;
    win->xwr.h = win->gmaxyg;

    /* set menu line spacing now, from our choosen font sized from the window.
       This then won't be reset by the client. */
    win->menuspcy = win->linespace+EXTRAMENUY;

    /* set parent window, either the given or the root window */
    if (pwin) pw = pwin->xmwhan; /* given */
    else pw = RootWindow(padisplay, pascreen); /* root */

    /* create master window */
    win->xmwhan = createwindow(pw, win->gmaxxg, win->gmaxyg);

    /* hook close event from windows manager */
    XWLOCK();
    win->delmsg = XInternAtom(padisplay, "WM_DELETE_WINDOW", FALSE);
    XSetWMProtocols(padisplay, win->xmwhan, &win->delmsg, 1);
    XWUNLOCK();

    /* create subclient window */
    win->xwhan = createwindow(win->xmwhan, win->gmaxxg, win->gmaxyg);

//dbg_printf(dlinfo, "master: %lx subclient: %lx\n", win->xmwhan, win->xwhan);
    /* find and save the frame parameters from the immediate/parent window.
       This may not work on some window managers */
    XWLOCK();
#ifndef NOWDELAY
    XMapWindow(padisplay, win->xmwhan);
    XMapWindow(padisplay, win->xwhan);
#endif
    XQueryTree(padisplay, win->xmwhan, &rw, &pw, &cwl, &ncw);
    XGetWindowAttributes(padisplay, pw, &xpwga);
    XGetWindowAttributes(padisplay, win->xmwhan, &xwga);
#ifndef NOWDELAY
    XUnmapWindow(padisplay, win->xwhan);
    XUnmapWindow(padisplay, win->xmwhan);
#endif
    XWUNLOCK();

    /* find net extra width of frame from client area */
    win->pfw = xpwga.width-xwga.width;
    win->pfh = xpwga.height-xwga.height;
    /* find offset from parent origin to client origin */
    win->cwox = xwga.x;
    win->cwoy = xwga.y;

#if 0
    dbg_printf(dlinfo, "Frame extra width: %d\n", win->pfw);
    dbg_printf(dlinfo, "Frame extra height: %d\n", win->pfh);
    dbg_printf(dlinfo, "Parent to client offset: x: %d y: %d\n",
               win->cwox, win->cwoy);
#endif

    /* set window title from program name */
    XWLOCK();
    XStoreName(padisplay, win->xmwhan, program_invocation_short_name);
    XWUNLOCK();

    iniscn(win, win->screens[0]); /* initalize screen buffer */
    restore(win); /* update to screen */

}

/*******************************************************************************

Close window

Shuts down, removes and releases a window.

*******************************************************************************/

static void clswin(int fn)

{

    int    r;   /* result holder */
    int    b;   /* int result holder */
    winptr win; /* window pointer */

    win = lfn2win(fn); /* get a pointer to the window */
    XWLOCK();
    /* destroy the window */
    XDestroyWindow(padisplay, win->xwhan);
    XDestroyWindow(padisplay, win->xmwhan);
    XWUNLOCK();

}

/*******************************************************************************

Close window

Closes an open window pair. Accepts an output window. The window is closed, and
the window and file handles are freed. The input file is freed only if no other
window also links it.

*******************************************************************************/

/* flush and close file */

static void clsfil(int fn)

{

    int    si; /* index for screens */
    filptr fp;

    fp = opnfil[fn];
    /* release all of the screen buffers */
    for (si = 0; si < MAXCON; si++)
        if (fp->win->screens[si]) ifree(fp->win->screens[si]);
    putwin(fp->win); /* release the window data */
    fp->win = NULL; /* set end open */
    fp->inw = FALSE;
    fp->inl = -1;

}

static int inplnk(int fn)

{

    int fi; /* index for files */
    int fc; /* counter for files */

    fc = 0; /* clear count */
    for (fi = 0; fi < MAXFIL; fi++) /* traverse files */
        if (opnfil[fi]) /* entry is occupied */
            if (opnfil[fi]->inl == fn) fc++; /* count the file link */

    return (fc); /* return result */

}

static void closewin(int ofn)

{

    int ifn; /* input file id */
    int wid; /* window id */

    wid = filwin[ofn]; /* get window id */
    ifn = opnfil[ofn]->inl; /* get the input file link */
    clswin(ofn); /* close the window */
    clsfil(ofn); /* flush and close output file */
    /* if no remaining links exist, flush and close input file */
    if (!inplnk(ifn)) clsfil(ifn);
    filwin[ofn] = -1; /* clear file to window translation */
    xltwin[wid+MAXFIL] = -1; /* clear window to file translation */

}

/** ****************************************************************************

Open an input and output pair

Creates, opens and initializes an input and output pair of files.

*******************************************************************************/

static void openio(FILE* infile, FILE* outfile, int ifn, int ofn, int pfn,
                   int wid)

{

    /* if output was never opened, create it now */
    if (!opnfil[ofn]) getfil(&opnfil[ofn]);
    /* if input was never opened, create it now */
    if (!opnfil[ifn]) getfil(&opnfil[ifn]);
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
        opnfil[ofn]->win = getwin();
        opnwin(ofn, pfn, wid); /* and start that up */

    }
    /* check if the window has been pinned to something else */
    if (xltwin[wid+MAXFIL] >= 0 && xltwin[wid+MAXFIL] != ofn) error(ewinuse); /* flag error */
    xltwin[wid+MAXFIL] = ofn; /* pin the window to the output file */
    filwin[ofn] = wid;

}

/** ****************************************************************************

Get freed/new menu tracking entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static metptr getmet(void)

{

    metptr p;

    if (fremet) { /* there is a freed entry */

        p = fremet; /* index top entry */
        fremet = p->next; /* gap from list */

    } else {

        p = (metptr)imalloc(sizeof(metrec));
        metcnt++; /* count entries */
        mettot += sizeof(metrec); /* add to total memory used */

    }

    return (p);

}

/** ****************************************************************************

Get freed/new menu entry

Either gets a new entry from malloc or returns a previously freed entry.

*******************************************************************************/

static void putmet(metptr p)

{

    p->next = fremet; /* push to list */
    fremet = p;

}

/** ****************************************************************************

Open menu item as window

The onscreen menu consists of a series of widgets representing menu items, each
in its own window. This routine opens a window at the given location on screen,
then prepares it to act as a widget. The actual drawing of the contents and
running event actions is left to the menu event handler.

The parent file can be NULL, in which case the menu entry is "free floating",
that is, an independent window on the desktop.

*******************************************************************************/

static void openmenu(
    /* input side */         FILE* f,
    /** parent */            FILE* p,
    /* rectangle */          int x1, int y1, int x2, int y2,
    /* menu tracker entry */ metptr mp
)

{

    mp->wid = pa_getwid(); /* allocate a buried wid */
    pa_openwin(&f, &mp->wf, p, mp->wid); /* open widget window */
    mp->parent = p; /* set parent file */
    xltmnu[mp->wid+MAXFIL] = mp; /* set the tracking entry for window */
    pa_buffer(mp->wf, FALSE); /* turn off buffering */
    pa_frame(mp->wf, FALSE); /* turn off frame */
    pa_auto(mp->wf, FALSE); /* turn off auto */
    pa_curvis(mp->wf, FALSE); /* turn off cursor */
    pa_font(mp->wf, PA_FONT_SIGN); /* set button font */
    pa_setposg(mp->wf, x1, y1); /* place at position */
    pa_setsizg(mp->wf, x2-x1+1, y2-y1+1); /* set size */
    pa_binvis(mp->wf); /* no background write */

}

/** ****************************************************************************

Menu event handler

This routine is called as a plug-in to the event handler chain. The events for
a menu item widget are performed.

*******************************************************************************/

/* present floating menu */
static void fltmen(FILE* f, metptr mp, int x, int y)

{

    int    mw; /* cumulative width of submenu */
    metptr p;  /* pointer to menu entries */
    int    w;
    winptr win;
    int    fx, fy;
    int    wc;

    win = txt2win(f); /* index window structure */
    /* find cumulative width of branch entries */
    p = mp->branch; /* index first in list */
    mw = 0; /* set no width */
    wc = 0; /* clear window count */
    while (p) { /* traverse */

        w = pa_strsiz(f, p->title); /* get width this entry */
        if (w > mw) mw = w; /* find maximum */
        wc++; /* count windows */
        p = p->next; /* next in list */

    }
    mw += 20; /* pad to sides */
    /* present frame */
    openmenu(out2inp(f), mp->evtfil, x, y, x+mw+4, y+wc*win->menuspcy+4+8, mp->frame);
    /* present the branch list as children of the frame */
    p = mp->branch;
    fx = 3; /* set frame coordinates, upper left+2 */
    fy = 3;
    while (p) { /* traverse */

        /* open menu item */
        openmenu(out2inp(f), mp->frame->wf, fx, fy, fx+mw, fy+win->menuspcy, p);
        p = p->next; /* next entry */
        fy += win->menuspcy+1; /* next location */

    }

}

/* remove pulldown menus */
static void remmen(metptr mp)

{

    /* close any open entry in this chain */
    while (mp) {

        /* if window file is open, close it */
        if (mp->wf && !mp->prime) {

            fclose(mp->wf); /* close */
            mp->wf = NULL; /* clear file link */
            mp->pressed = FALSE; /* remove any press status */

        }
        /* and close any submenus */
        if (mp->branch) remmen(mp->branch);
        /* close frame, if exists */
        if (mp->branch) remmen(mp->frame);
        mp = mp->next; /* next menu entry */

    }

}

static void menu_release_all(metptr mp, metptr skip);

/* handle menu button press */
static void menu_press(metptr mp)

{

    winptr par; /* window parent */
    int x, y;

    par = NULL; /* set no parent (floating menu) */
    if (mp->parent) par = txt2win(mp->parent); /* index parent window */
    /* process button press */
    mp->pressed = TRUE;
    pa_fcolorg(mp->wf, INT_MAX-INT_MAX/4, INT_MAX-INT_MAX/4, INT_MAX-INT_MAX/4);
    pa_frect(mp->wf, 1, 1, pa_maxxg(mp->wf), pa_maxyg(mp->wf));
    if (mp->title) { /* there is a title */

        pa_fcolor(mp->wf, pa_black);
        pa_cursorg(mp->wf,
                   pa_maxxg(mp->wf)/2-pa_strsiz(mp->wf, mp->title)/2,
                pa_maxyg(mp->wf)/2-pa_chrsizy(mp->wf)/2);
        fprintf(mp->wf, "%s", mp->title); /* place button title */

    }
    /* draw underbar */
    pa_fcolorg(mp->wf, INT_MAX/256*233, INT_MAX/256*84, INT_MAX/256*32);
    pa_frect(mp->wf, 1, pa_maxyg(mp->wf)-4, pa_maxxg(mp->wf), pa_maxyg(mp->wf));
    /* if it is a branch, present floating menu */
    if (mp->branch) {

        x = mp->x; /* find location of button bottom */
        y = mp->y+par->menuspcy;
        remmen(mp->head); /* remove any other pulldowns */
        menu_release_all(mp->head, mp); /* release other menus */
        fltmen(mp->wf, mp, x, y); /* present this pulldown */

    }

}

/* handle menu button release */
static void menu_release(metptr mp)

{

    mp->pressed = FALSE;
    pa_fcolor(mp->wf, pa_white);
    pa_frect(mp->wf, 1, 1, pa_maxxg(mp->wf), pa_maxyg(mp->wf));
    if (mp->title) { /* there is a title */

        pa_fcolor(mp->wf, pa_black);
        pa_cursorg(mp->wf,
                   pa_maxxg(mp->wf)/2-pa_strsiz(mp->wf, mp->title)/2,
                   pa_maxyg(mp->wf)/2-pa_chrsizy(mp->wf)/2);
        fprintf(mp->wf, "%s", mp->title); /* place button title */

    }
    pa_fcolorg(mp->wf,
               INT_MAX/256*223, INT_MAX/256*223, INT_MAX/256*223);
    pa_frect(mp->wf, 1, pa_maxyg(mp->wf)-1,
                        pa_maxxg(mp->wf), pa_maxyg(mp->wf));

}

/* remove any top level menu press states */
static void menu_release_all(metptr mp, metptr skip)

{

    /* close any open entry in this chain */
    while (mp) {

        if (mp->pressed && mp != skip) menu_release(mp); /* release this menu */
        mp = mp->next; /* next menu entry */

    }

}

static void menu_event(pa_evtrec* ev)

{

    pa_evtrec er; /* outbound menu event */
    metptr    mp; /* tracking entry for meny entries */
    winptr    par; /* window parent */
    int x, y;

    /* if not our window, send it on */
    mp = xltmnu[ev->winid+MAXFIL]; /* get possible menu entry */
    if (!mp) menu_event_oeh(ev); /* pass on if not a menu entry */
    else { /* handle it here */

        par = NULL; /* set no parent (floating menu) */
        if (mp->parent) par = txt2win(mp->parent); /* index parent window */
        if (ev->etype == pa_etredraw) { /* redraw the window */

            /* color the background */
            pa_fcolor(mp->wf, pa_white);
            pa_frect(mp->wf, 1, 1, pa_maxxg(mp->wf), pa_maxyg(mp->wf));
            if (mp->title) { /* there is a title */

                /* place the title */
                pa_fcolor(mp->wf, pa_black);
                pa_cursorg(mp->wf,
                           pa_maxxg(mp->wf)/2-pa_strsiz(mp->wf, mp->title)/2,
                           pa_maxyg(mp->wf)/2-pa_chrsizy(mp->wf)/2);
                fprintf(mp->wf, "%s", mp->title); /* place button title */

            }
            if (mp->pressed) {

                /* draw underbar */
                pa_fcolorg(mp->wf, INT_MAX/256*233, INT_MAX/256*84, INT_MAX/256*32);
                pa_frect(mp->wf, 1, pa_maxyg(mp->wf)-4, pa_maxxg(mp->wf), pa_maxyg(mp->wf));

            } else if (mp->prime) { /* draw divider line */

                pa_fcolorg(mp->wf,
                           INT_MAX/256*223, INT_MAX/256*223, INT_MAX/256*223);
                pa_frect(mp->wf, 1, pa_maxyg(mp->wf)-1,
                                    pa_maxxg(mp->wf), pa_maxyg(mp->wf));

            }
            if (mp->frm) { /* box the frame */

                pa_fcolorg(mp->wf,
                           INT_MAX/256*150, INT_MAX/256*150, INT_MAX/256*150);
                pa_rect(mp->wf, 1, 1, pa_maxxg(mp->wf), pa_maxyg(mp->wf));
                pa_rect(mp->wf, 2, 2, pa_maxxg(mp->wf)-1, pa_maxyg(mp->wf)-1);

            }
            if (mp->bar) { /* draw bar under */

                pa_fcolorg(mp->wf,
                           INT_MAX/256*150, INT_MAX/256*150, INT_MAX/256*150);
                pa_line(mp->wf, 1, pa_maxyg(mp->wf), pa_maxxg(mp->wf),
                        pa_maxyg(mp->wf));

            }

        } else if (ev->etype == pa_etmouba && ev->amoubn == 1) {

            /* mouse button 1 activation in window */
            if (!mp->menubar) { /* if not the menu bar */

                /* if button not pressed */
                if (!mp->pressed) menu_press(mp); /* process menu press */
                else if (mp->branch) {

                    /* second press on floating menu */
                    menu_release(mp); /* release the button */
                    remmen(mp->branch); /* remove floating menus */
                    remmen(mp->frame); /* remove frame */

                }

            }

            if (!mp->branch) { /* not a branch entry */

                /* send event back to parent window */
                er.etype = pa_etmenus; /* set button event */
                er.butid = mp->id; /* set id */
                pa_sendevent(mp->evtfil/*parent*/, &er); /* send the event to the parent */
                remmen(mp->head); /* remove all floating menus but bar */
                menu_release_all(mp->head, mp); /* remove all top level presses */

            }

        } else if (ev->etype == pa_etmoubd && ev->dmoubn == 1) {

            /* mouse button 1 deactivation in window */
            if (!mp->menubar && !mp->branch) /* not the menu bar and not branch */
                menu_release(mp); /* release menu button */

        }

    }

}

/** ****************************************************************************

Activate onscreen menu for window

Takes a window file. The onscreen menu for the window, if it exists, is
presented in the window. Only the top level menu items are presented. The lower
level menus are activated as needed by the menu event handler.

*******************************************************************************/

static void actmenu(FILE* f)

{

    winptr win; /* pointer to windows context */
    metptr mp;  /* menu pointer */
    int x;      /* running position of menu entries */
    int w;      /* width of menu face text */
    FILE* bf;   /* menu bar file */
    FILE* inf;  /* input file for output window */

    win = txt2win(f); /* get window context */
    inf = out2inp(f); /* get input file for window */
    /* open the menu bar */
    openmenu(inf, f, 1, 1, pa_maxxg(f), win->menuspcy, win->menu);
    bf = win->menu->wf; /* get the menu bar file */
    x = 1; /* set initial menu bar position */
    mp = win->metlst; /* index top of menu list */
    while (mp) { /* traverse top level list */

        /* find width of face text in menu bar terms */
        w = pa_strsiz(bf, mp->title);
        /* open menu item here */
        openmenu(inf, f, x, 1, x+w+EXTRAMENUX, win->menuspcy, mp);
        mp->x = x; /* save position */
        mp->y = 1;
        mp->prime = TRUE; /* set is a prime (onscreen) menu */
        x = x+w+EXTRAMENUX; /* go next menu position */
        mp = mp->next; /* next top menu item */

    }

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

static void iclear(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index current update screen */
    sc->curx = 1; /* set cursor at home */
    sc->cury = 1;
    sc->curxg = 1;
    sc->curyg = 1;
    clrbuf(sc); /* clear screen buffer */
    if (indisp(win)) { /* also process to display */

        curoff(win); /* hide the cursor */
        XWLOCK();
        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        XFillRectangle(padisplay, win->xwhan, sc->xcxt, 0, 0,
                                  sc->maxxg, sc->maxyg);
        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }

}

/** ****************************************************************************

Scroll screen

Scrolls the screen by deltas in any given direction. If the scroll would move
all content off the screen, the screen is simply blanked. Otherwise, we find the
section of the screen that would remain after the scroll, determine its source
and destination rectangles, and use a bitblt to move it.

One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

static void iscrollg(winptr win, int x, int y)

{

    int dx, dy, dw, dh; /* destination coordinates and sizes */
    int sx, sy, sw, sh; /* destination coordinates */
    struct { /* fill rectangle */

        int x, y; /* origin (left, top) */
        int w, h; /* width, height */

    } frx, fry; /* x fill, y fill */
    scnptr sc;  /* pointer to current screen */

    sc = win->screens[win->curupd-1]; /* index current screen */
    /* scroll would result in complete clear, do it */
    if (x <= -sc->maxxg || x >= sc->maxxg ||
        y <= -sc->maxyg || y >= sc->maxyg)
        iclear(win); /* clear the screen buffer */
    else { /* scroll */

        /* set y movement */
        if (y >= 0)  { /* move up */

            sy = y; /* from y lines down */
            sh = sc->maxyg-y; /* height minus lines to move */
            dy = 0; /* move to top of screen */
            fry.x = 0; /* set fill to y lines at bottom */
            fry.w = sc->maxxg-1;
            fry.y = sc->maxyg-y;
            fry.h = y;

        } else { /* move down */

            sy = 0; /* from top */
            sh = sc->maxyg-abs(y); /* height minus lines to move */
            dy = abs(y); /* move to y lines down */
            fry.x = 0; /* set fill to y lines at top */
            fry.w = sc->maxxg-1;
            fry.y = 0;
            fry.h = abs(y);

        }
        /* set x movement */
        if (x >= 0) { /* move left */

            sx = x; /* from x characters to the right */
            sw = sc->maxxg-x; /* width - x characters */
            dx = 0; /* move to left side */
            /* set fill x character collums at right */
            frx.x = sc->maxxg-x;
            frx.w = x;
            frx.y = 0;
            frx.h = sc->maxyg-1;

        } else { /* move right */

            sx = 0; /* from x left */
            sw = sc->maxxg-abs(x); /* width - x characters */
            dx = abs(x); /* move from left side */
            /* set fill x character collums at left */
            frx.x = 0;
            frx.w = abs(x);
            frx.y = 0;
            frx.h = sc->maxyg-1;

        }
        if (win->bufmod) { /* apply to buffer */

            XWLOCK();
            XCopyArea(padisplay, sc->xbuf, sc->xbuf, sc->xcxt,
                      sx, sy, sw, sh, dx, dy);
            XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            /* fill vacated x */
            if (x) XFillRectangle(padisplay, sc->xbuf, sc->xcxt, frx.x, frx.y,
                                  frx.w, frx.h);
            /* fill vacated y */
            if (y) XFillRectangle(padisplay, sc->xbuf, sc->xcxt, fry.x, fry.y,
                                  fry.w, fry.h);
            XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            XWUNLOCK();

        } else { /* scroll on screen */

            curoff(win); /* hide the cursor for drawing */
            XWLOCK();
            XCopyArea(padisplay, win->xwhan, win->xwhan, sc->xcxt,
                      sx, sy, sw, sh, dx, dy);
            XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            /* fill vacated x */
            if (x) XFillRectangle(padisplay, win->xwhan, sc->xcxt, frx.x, frx.y,
                                  frx.w, frx.h);
            /* fill vacated y */
            if (y) XFillRectangle(padisplay, win->xwhan, sc->xcxt, fry.x, fry.y,
                                  fry.w, fry.h);
            XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }

    }
    if (indisp(win) && win->bufmod)
        restore(win); /* move buffer to screen */

}

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

static void icursor(winptr win, int x, int y)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index update screen */
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

static void icursorg(winptr win, int x, int y)

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
    /* check not top of screen */
    if (sc->cury > 1) {

        curoff(win); /* hide the cursor */
        sc->cury--; /* update position */
        sc->curyg -= win->linespace; /* go last character line */
        curon(win); /* show the cursor */

    } else if (sc->autof)
        iscrollg(win, 0*win->charspace, -1*win->linespace); /* scroll up */
    /* check won't overflow */
    else if (sc->cury > -INT_MAX) {

        curoff(win); /* hide the cursor */
        sc->cury--; /* set new position */
        sc->curyg -= win->linespace;
        curon(win); /* show the cursor */

    }

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
    /* check not bottom of screen */
    if (sc->cury < sc->maxy) {

        curoff(win); /* hide the cursor */
        sc->cury++; /* update position */
        sc->curyg += win->linespace+win->chrspcy; /* move to next character line */
        curon(win); /* show the cursor */

    } else if (sc->autof)
        iscrollg(win, 0*win->charspace, +1*win->linespace); /* scroll down */
    else if (sc->cury < INT_MAX) {

        curoff(win); /* hide the cursor */
        sc->cury++; /* set new position */
        sc->curyg += win->linespace+win->chrspcy; /* move to next text line */
        curon(win); /* show the cursor */

    }

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
    /* check not at extreme left */
    if (sc->curx > 1) {

        curoff(win); /* hide the cursor */
        sc->curx--; /* update position */
        sc->curxg -= win->charspace; /* back one character */
        curon(win); /* show the cursor */

    } else { /* wrap cursor motion */

        if (sc->autof) { /* autowrap is on */

            iup(win); /* move cursor up one line */
            curoff(win); /* hide the cursor */
            sc->curx = sc->maxx; /* set cursor to extreme right */
            sc->curxg = sc->maxxg-win->charspace;
            curon(win); /* show the cursor */

        } else {

            /* check won't overflow */
            if (sc->curx > -INT_MAX) {

                curoff(win); /* hide the cursor */
                sc->curx--; /* update position */
                sc->curxg -= win->charspace;
                curon(win); /* show the cursor */

            }

        }

    }

}

/** ****************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

static void iright(winptr win)

{

    scnptr sc;

    sc = win->screens[win->curupd-1]; /* index screen */
    /* check not at extreme right */
    if (sc->curx < sc->maxx) {

        curoff(win); /* hide the cursor */
        sc->curx++; /* update position */
        sc->curxg += win->charspace;
        curon(win); /* show the cursor */

    } else { /* wrap cursor motion */

        if (sc->autof) { /* autowrap is on */

            idown(win); /* move cursor up one line */
            curoff(win); /* hide the cursor */
            sc->curx = 1; /* set cursor to extreme left */
            sc->curxg = 1;
            curon(win); /* show the cursor */

        /* check won't overflow */
        } else {

            if (sc->curx < INT_MAX) {

                curoff(win); /* hide the cursor */
                sc->curx++; /* update position */
                sc->curxg += win->charspace;
                curon(win); /* show the cursor */

            }

        }

    }

}

/** ****************************************************************************

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

*******************************************************************************/

static void itab(winptr win)

{

    int i;
    int x;
    scnptr sc;

    sc = win->screens[win->curupd-1];
    curoff(win); /* hide the cursor */
    /* first, find if next tab even exists */
    x = sc->curxg+1; /* get just after the current x position */
    if (x < 1) x = 1; /* don"t bother to search to left of screen */
    /* find tab || } of screen */
    i = 0; /* set 1st tab position */
    while (x > sc->tab[i] && sc->tab[i] && i < MAXTAB && x < sc->maxxg) i++;
    if (sc->tab[i] && x < sc->tab[i]) { /* not off right of tabs */

       sc->curxg = sc->tab[i]; /* set position to that tab */
       sc->curx = sc->curxg/win->charspace+1;

    }
    curon(win); /* show the cursor */

}

/*******************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

*******************************************************************************/

static void isettabg(winptr win, int t)

{

    int i, x; /* tab index */
    scncon* sc; /* screen context */

    sc = win->screens[win->curupd-1];
    if (sc->autof && (t-1)%win->charspace)
        error(eatotab); /* cannot perform with auto on */
    if (t < 1 || t > sc->maxxg) error(einvtab); /* bad tab position */
    /* find free location or tab beyond position */
    i = 0;
    while (i < MAXTAB && sc->tab[i] && t > sc->tab[i]) i = i+1;
    if (i == MAXTAB && t < sc->tab[i]) error(etabful); /* tab table full */
    if (t != sc->tab[i])  { /* not the same tab yet again */

        if (sc->tab[MAXTAB-1]) error(etabful); /* tab table full */
        /* move tabs above us up */
        for (x = MAXTAB-1; x > i; x--) sc->tab[x] = sc->tab[x-1];
        sc->tab[i] = t; /* place tab in order */

    }

}

/*******************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

*******************************************************************************/

static void irestabg(winptr win, int t)

{

    int     i;  /* tab index */
    int     ft; /* found tab */
    scncon* sc; /* screen context */

    sc = win->screens[win->curupd-1];
    if (t < 1 || t > sc->maxxg) error(einvtab); /* bad tab position */
    /* search for that tab */
    ft = 0; /* set not found */
    for (i = 0; i < MAXTAB; i++) if (sc->tab[i] == t) ft = i; /* found */
    if (ft != 0) { /* found the tab, remove it */

       /* move all tabs down to gap out */
       for (i = ft; i < MAXTAB-1; i++) sc->tab[i] = sc->tab[i+1];
       sc->tab[MAXTAB] = 0; /* clear any last tab */

    }

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

    scnptr sc; /* pointer to current screen */
    int    cs; /* character spacing */

    sc = win->screens[win->curupd-1]; /* index current screen */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* handle special character cases first */
    if (c == '\r') {

        /* carriage return, position to extreme left */
        curoff(win); /* hide the cursor */
        sc->curx = 1; /* set to extreme left */
        sc->curxg = 1;
        curon(win); /* show the cursor */


    } else if (c == '\n') {

        curoff(win); /* hide the cursor */
        sc->curx = 1; /* set to extreme left */
        sc->curxg = 1;
        curon(win); /* show the cursor */
        idown(win); /* line feed, move down */

    } else if (c == '\b') ileft(win); /* back space, move left */
    else if (c == '\f') iclear(win); /* clear screen */
    else if (c == '\t') itab(win); /* process tab */
    /* only output visible characters */
    else if (c >= ' ' && c != 0x7f) {

        /* find character spacing */
        if (sc->cfont->fix) cs = win->charspace;
        else cs = xwidth(win, c)+win->chrspcx;
        if (win->bufmod) { /* buffer is active */

            if (sc->bmod != mdinvis) { /* background is visible */

                XWLOCK();
                /* set background function */
                XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
                /* set background to foreground to draw character background */
                if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                XFillRectangle(padisplay, sc->xbuf, sc->xcxt,
                               sc->curxg-1, sc->curyg-1,
                               cs, win->linespace);
                /* xor is non-destructive, and we can restore it. And and or are
                   destructive, and would require a combining buffer to perform */
                if (sc->bmod == mdxor)
                    /* restore the surface under text */
                    XDrawString(padisplay, sc->xbuf, sc->xcxt,
                                sc->curxg-1, sc->curyg-1+win->baseoff, &c, 1);
                /* restore colors */
                if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                /* reset background function */
                XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
                XWUNLOCK();

            }
            if (sc->fmod != mdinvis) {

                XWLOCK();
                /* set foreground function */
                XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
                /* draw character */
                XDrawString(padisplay, sc->xbuf, sc->xcxt,
                            sc->curxg-1, sc->curyg-1+win->baseoff, &c, 1);

                /* check draw underline */
                if (sc->attr & BIT(saundl)){

                    /* double line, may need ajusting for low DP displays */
                    XDrawLine(padisplay, sc->xbuf, sc->xcxt,
                              sc->curxg-1, sc->curyg-1+win->baseoff+1,
                              sc->curxg-1+cs, sc->curyg-1+win->baseoff+1);
                    XDrawLine(padisplay, sc->xbuf, sc->xcxt,
                              sc->curxg-1, sc->curyg-1+win->baseoff+2,
                              sc->curxg-1+cs, sc->curyg-1+win->baseoff+2);

                }

                /* check draw strikeout */
                if (sc->attr & BIT(sastkout)) {

                    XDrawLine(padisplay, sc->xbuf, sc->xcxt,
                              sc->curxg-1, sc->curyg-1+win->baseoff/STRIKE,
                              sc->curxg-1+cs, sc->curyg-1+win->baseoff/STRIKE);
                    XDrawLine(padisplay, sc->xbuf, sc->xcxt,
                              sc->curxg-1, sc->curyg-1+win->baseoff/STRIKE+1,
                              sc->curxg-1+cs, sc->curyg-1+win->baseoff/STRIKE+1);

                }
                /* reset foreground function */
                XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
                XWUNLOCK();

            }

        }
        if (indisp(win)) { /* do it again for the current screen */

            curoff(win); /* hide the cursor */

            if (sc->bmod != mdinvis) { /* background is visible */

                XWLOCK();
                /* set background function */
                XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
                /* set background to foreground to draw character background */
                if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                               sc->curxg-1, sc->curyg-1,
                               cs, win->linespace);
                /* xor is non-destructive, and we can restore it. And and or are
                   destructive, and would require a combining buffer to perform */
                if (sc->bmod == mdxor)
                    /* restore the surface under text */
                    XDrawString(padisplay, win->xwhan, sc->xcxt,
                                sc->curxg-1, sc->curyg-1+win->baseoff, &c, 1);
                /* restore colors */
                if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                /* reset background function */
                XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
                XWUNLOCK();

            }
            if (sc->fmod != mdinvis) {

                XWLOCK();
                /* set foreground function */
                XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
                /* draw character */
                XDrawString(padisplay, win->xwhan, sc->xcxt,
                            sc->curxg-1, sc->curyg-1+win->baseoff, &c, 1);

                /* check draw underline */
                if (sc->attr & BIT(saundl)){

                    /* double line, may need ajusting for low DP displays */
                    XDrawLine(padisplay, win->xwhan, sc->xcxt,
                              sc->curxg-1, sc->curyg-1+win->baseoff+1,
                              sc->curxg-1+cs, sc->curyg-1+win->baseoff+1);
                    XDrawLine(padisplay, win->xwhan, sc->xcxt,
                              sc->curxg-1, sc->curyg-1+win->baseoff+2,
                              sc->curxg-1+cs, sc->curyg-1+win->baseoff+2);

                }

                /* check draw strikeout */
                if (sc->attr & BIT(sastkout)) {

                    XDrawLine(padisplay, win->xwhan, sc->xcxt,
                              sc->curxg-1, sc->curyg-1+win->baseoff/STRIKE,
                              sc->curxg-1+cs, sc->curyg-1+win->baseoff/STRIKE);
                    XDrawLine(padisplay, win->xwhan, sc->xcxt,
                              sc->curxg-1, sc->curyg-1+win->baseoff/STRIKE+1,
                             sc->curxg-1+cs, sc->curyg-1+win->baseoff/STRIKE+1);

                }
                /* reset foreground function */
                XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
                XWUNLOCK();

            }

            curon(win); /* show the cursor */

        }
        /* advance to next character */
        if (sc->cfont->fix) iright(win); /* move cursor right character */
        else { /* perform proportional version */

            if (indisp(win)) curoff(win); /* remove cursor */
            /* advance the character width */
            sc->curxg = sc->curxg+xwidth(win, c)+win->chrspcx;
            /* the cursor x position really has no meaning with proportional
               but we recalculate it using space anyways. */
            sc->curx = sc->curxg/win->charspace+1;
            if (indisp(win)) curon(win); /* set cursor on screen */

        }

    }

}

/** ****************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement stdio:

read
write
open
close
lseek

We use interdiction to filter standard I/O calls towards the terminal. The
0 (input) and 1 (output) files are interdicted. In ANSI terminal, we act as a
filter, so this does not change the user ability to redirect the file handles
elsewhere.

*******************************************************************************/

/** ****************************************************************************

Read

*******************************************************************************/

static ssize_t ivread(pread_t readdc, int fd, void* buff, size_t count)

{

    return (*readdc)(fd, buff, count);

}

static ssize_t iread(int fd, void* buff, size_t count)

{

    ivread(ofpread, fd, buff, count);

}

static ssize_t iread_nocancel(int fd, void* buff, size_t count)

{

    ivread(ofpread_nocancel, fd, buff, count);

}

/** ****************************************************************************

Write

*******************************************************************************/

static ssize_t ivwrite(pwrite_t writedc, int fd, const void* buff, size_t count)

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

    } else rc = (*writedc)(fd, buff, count);

    return rc;

}

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    ivwrite(ofpwrite, fd, buff, count);

}

static ssize_t iwrite_nocancel(int fd, const void* buff, size_t count)

{

    ivwrite(ofpwrite_nocancel, fd, buff, count);

}

/** ****************************************************************************

Open

Terminal is assumed to be opened when the system starts, and closed when it
shuts down. Thus we do nothing for this.

*******************************************************************************/

static int ivopen(popen_t opendc, const char* pathname, int flags, int perm)

{

    return (*opendc)(pathname, flags, perm);

}

static int iopen(const char* pathname, int flags, int perm)

{

    ivopen(ofpopen, pathname, flags, perm);

}

static int iopen_nocancel(const char* pathname, int flags, int perm)

{

    ivopen(ofpopen_nocancel, pathname, flags, perm);

}

/** ****************************************************************************

Close

If the file is attached to an output window, closes the window file. Otherwise,
the close is just passed on.

*******************************************************************************/

static int ivclose(pclose_t closedc, int fd)

{

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    /* check if the file is an output window, and close if so */
    if (opnfil[fd] && opnfil[fd]->win) closewin(fd);

    return (*closedc)(fd);

}

static int iclose(int fd)

{

    ivclose(ofpclose, fd);

}

static int iclose_nocancel(int fd)

{

    ivclose(ofpclose_nocancel, fd);

}

/** ****************************************************************************

Lseek

Lseek is never possible on a terminal, so this is always an error on the stdin
or stdout handle.

*******************************************************************************/

static off_t ivlseek(plseek_t lseekdc, int fd, off_t offset, int whence)

{

    /* check seeking on terminal attached file (input or output) and error
       if so */
    if (fd == INPFIL || fd == OUTFIL) error(efilopr);

    return (*lseekdc)(fd, offset, whence);

}

static off_t ilseek(int fd, off_t offset, int whence)

{

    ivlseek(ofplseek, fd, offset, whence);

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

    winptr win; /* windows record pointer */
    int    r;

    win = txt2win(f); /* get window from file */
    r = win->baseoff; /* return current line spacing */

    return (r);

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

    return (win->gmaxy);

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

   /* no capability */

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void pa_reverse(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc; /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* reverse on */

        sc->attr |= BIT(sarev); /* set attribute active */
        win->gattr |= BIT(sarev);
        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
        XSetBackground(padisplay, sc->xcxt, sc->fcrgb);

    } else { /* turn it off */

        sc->attr &= ~BIT(sarev); /* set attribute inactive */
        win->gattr &= ~BIT(sarev);
        XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);

    }

}

/** ****************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_underline(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* underline on */

        sc->attr |= BIT(saundl); /* set attribute active */
        win->gattr |= BIT(saundl);

    } else { /* turn it off */

        sc->attr &= ~BIT(saundl); /* set attribute inactive */
        win->gattr &= ~BIT(saundl);

    }

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and elevated font.

*******************************************************************************/

void pa_superscript(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sasuper); /* set attribute active */
       win->gattr |= BIT(sasuper);

    } else { /* turn it off */

       sc->attr &= ~BIT(sasuper); /* set attribute inactive */
       win->gattr &= ~BIT(sasuper);

    }

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and lowered font.

*******************************************************************************/

void pa_subscript(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sasubs); /* set attribute active */
       win->gattr |= BIT(sasubs);

    } else { /* turn it off */

       sc->attr &= ~BIT(sasubs); /* set attribute inactive */
       win->gattr &= ~BIT(sasubs);

    }

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

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saital); /* set attribute active */
       win->gattr |= BIT(saital);

    } else { /* turn it off */

       sc->attr &= ~BIT(saital); /* set attribute inactive */
       win->gattr &= ~BIT(saital);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_bold(FILE* f, int e)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sabold); /* set attribute active */
       win->gattr |= BIT(sabold);

    } else { /* turn it off */

       sc->attr &= ~BIT(sabold); /* set attribute inactive */
       win->gattr &= ~BIT(sabold);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

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

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sastkout); /* set attribute active */
       win->gattr |= BIT(sastkout);

    } else { /* turn it off */

       sc->attr &= ~BIT(sastkout); /* set attribute inactive */
       win->gattr &= ~BIT(sastkout);

    }

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_standout(FILE* f, int e)

{

   pa_reverse(f, e); /* implement as reverse */

}

/** ****************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

*******************************************************************************/

void pa_fcolor(FILE* f, pa_color c)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->fcrgb = colnum(c); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* set screen color according to reverse */
    XWLOCK();
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->fcrgb);
    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Set foreground color

Sets the foreground color from individual r, g, b values.

*******************************************************************************/

void pa_fcolorc(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->fcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* set screen color according to reverse */
    XWLOCK();
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->fcrgb);
    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

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
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->fcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gfcrgb = sc->fcrgb;
    /* set screen color according to reverse */
    XWLOCK();
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->fcrgb);
    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void pa_bcolor(FILE* f, pa_color c)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->bcrgb = colnum(c); /* set color status */
    win->gbcrgb = sc->bcrgb;
    XWLOCK();
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
    else XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    XWUNLOCK();

}

/** ****************************************************************************

Set background color

Sets the background color from individual r, g, b values.

*******************************************************************************/

void pa_bcolorc(FILE* f, int r, int g, int b)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->bcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gbcrgb = sc->bcrgb;
    XWLOCK();
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    else XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    XWUNLOCK();

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
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    sc->bcrgb = rgb2xwin(r, g, b); /* set color status */
    win->gbcrgb = sc->bcrgb; /* copy to master */
    XWLOCK();
    /* set screen color according to reverse */
    if (BIT(sarev) & sc->attr) XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    else XSetBackground(padisplay, sc->xcxt, sc->bcrgb);
    XWUNLOCK();

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

    int    ld;  /* last display screen number save */
    winptr win; /* window record pointer */

    win = txt2win(f); /* get window from file */
    if (!win->bufmod) error(ebufoff); /* error */
    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error(einvscn); /* invalid screen number */
    ld = win->curdsp; /* save the current display screen number */
    win->curupd = u; /* set the current update screen */
    if (!win->screens[win->curupd-1]) { /* no screen, create one */

        /* get a new screen context */
        win->screens[win->curupd-1] = imalloc(sizeof(scncon));
        scncnt++;
        scntot += sizeof(scncon);
        iniscn(win, win->screens[win->curupd-1]); /* initalize that */

    }
    win->curdsp = d; /* set the current display screen */
    if (!win->screens[win->curdsp-1]) { /* no screen, create one */

        /* no current screen, create a new one */
        win->screens[win->curdsp-1] = imalloc(sizeof(scncon));
        scncnt++;
        scntot += sizeof(scncon);
        iniscn(win, win->screens[win->curdsp-1]); /* initalize that */

    }
    /* if the screen has changed, restore it */
    if (win->curdsp != ld) {

        if (!win->visible) winvis(win); /* make sure we are displayed */
        else restore(win);

    }

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

Attributes are performed, such as foreground/background coloring, modes, and
character attributes.

Character kerning is only available via this routine, and strsiz() is only
accurate for this routine, and not direct character placement, if kerning is
enabled.

*******************************************************************************/

void pa_wrtstr(FILE* f, char* s)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int    l;   /* length of string */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (sc->autof) error(estrato); /* autowrap is on */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    l = strlen(s); /* get length of string */
    if (win->bufmod) { /* buffer is active */

        if (sc->bmod != mdinvis) { /* background is visible */

            XWLOCK();
            /* set background function */
            XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
            /* set background to foreground to draw character background */
            if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt,
                           sc->curxg-1, sc->curyg-1,
                           win->charspace*l, win->linespace);
            /* xor is non-destructive, and we can restore it. And and or are
               destructive, and would require a combining buffer to perform */
            if (sc->bmod == mdxor)
                /* restore the surface under text */
                XDrawString(padisplay, sc->xbuf, sc->xcxt,
                            sc->curxg-1, sc->curyg-1+win->baseoff, s, l);
            /* restore colors */
            if (BIT(sarev) & sc->attr) XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            /* reset background function */
            XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
            XWUNLOCK();

        }
        if (sc->fmod != mdinvis) {

            XWLOCK();
            /* set foreground function */
            XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
            /* draw character */
            XDrawString(padisplay, sc->xbuf, sc->xcxt,
                        sc->curxg-1, sc->curyg-1+win->baseoff, s, l);

            /* check draw underline */
            if (sc->attr & BIT(saundl)){

                /* double line, may need ajusting for low DP displays */
                XDrawLine(padisplay, sc->xbuf, sc->xcxt,
                          sc->curxg-1, sc->curyg-1+win->baseoff+1,
                          sc->curxg-1+win->charspace*l, sc->curyg-1+win->baseoff+1);
                XDrawLine(padisplay, sc->xbuf, sc->xcxt,
                          sc->curxg-1, sc->curyg-1+win->baseoff+2,
                          sc->curxg-1+win->charspace*l, sc->curyg-1+win->baseoff+2);

            }

            /* check draw strikeout */
            if (sc->attr & BIT(sastkout)) {

                XDrawLine(padisplay, sc->xbuf, sc->xcxt,
                          sc->curxg-1, sc->curyg-1+win->baseoff/STRIKE,
                          sc->curxg-1+win->charspace*l, sc->curyg-1+win->baseoff/STRIKE);
                XDrawLine(padisplay, sc->xbuf, sc->xcxt,
                          sc->curxg-1, sc->curyg-1+win->baseoff/STRIKE+1,
                          sc->curxg-1+win->charspace*l, sc->curyg-1+win->baseoff/STRIKE+1);

            }
            /* reset foreground function */
            XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
            XWUNLOCK();

        }

    }
    if (indisp(win)) { /* do it again for the current screen */

        curoff(win); /* hide the cursor */
        if (sc->bmod != mdinvis) { /* background is visible */

            XWLOCK();
            /* set background function */
            XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
            /* set background to foreground to draw character background */
            if (BIT(sarev) & sc->attr)
                XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                           sc->curxg-1, sc->curyg-1,
                           win->charspace*l, win->linespace);
            /* xor is non-destructive, and we can restore it. And and or are
               destructive, and would require a combining buffer to perform */
            if (sc->bmod == mdxor)
                /* restore the surface under text */
                XDrawString(padisplay, win->xwhan, sc->xcxt,
                            sc->curxg-1, sc->curyg-1+win->baseoff, s, l);
            /* restore colors */
            if (BIT(sarev) & sc->attr)
                XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
            else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
            /* reset background function */
            XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
            XWUNLOCK();

        }
        if (sc->fmod != mdinvis) {

            XWLOCK();
            /* set foreground function */
            XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
            /* draw character */
            XDrawString(padisplay, win->xwhan, sc->xcxt,
                        sc->curxg-1, sc->curyg-1+win->baseoff, s, l);

            /* check draw underline */
            if (sc->attr & BIT(saundl)){

                /* double line, may need ajusting for low DP displays */
                XDrawLine(padisplay, win->xwhan, sc->xcxt,
                          sc->curxg-1, sc->curyg-1+win->baseoff+1,
                          sc->curxg-1+win->charspace*l,
                          sc->curyg-1+win->baseoff+1);
                XDrawLine(padisplay, win->xwhan, sc->xcxt,
                          sc->curxg-1, sc->curyg-1+win->baseoff+2,
                          sc->curxg-1+win->charspace*l,
                          sc->curyg-1+win->baseoff+2);

            }

            /* check draw strikeout */
            if (sc->attr & BIT(sastkout)) {

                XDrawLine(padisplay, win->xwhan, sc->xcxt,
                          sc->curxg-1, sc->curyg-1+win->baseoff/STRIKE,
                          sc->curxg-1+win->charspace*l,
                          sc->curyg-1+win->baseoff/STRIKE);
                XDrawLine(padisplay, win->xwhan, sc->xcxt,
                          sc->curxg-1, sc->curyg-1+win->baseoff/STRIKE+1,
                          sc->curxg-1+win->charspace*l,
                          sc->curyg-1+win->baseoff/STRIKE+1);

            }
            /* reset foreground function */
            XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
            XWUNLOCK();

        }
        curon(win); /* show the cursor */

    }

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

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the line */
        XWLOCK();
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x1-1, y1-1, x2-1, y2-1);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        XWLOCK();
        /* draw the line */
        XDrawLine(padisplay, win->xwhan, sc->xcxt, x1-1, y1-1, x2-1, y2-1);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw rectangle

Draws a rectangle in foreground color.

*******************************************************************************/

void pa_rect(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the rectangle */
        XWLOCK();
        XDrawRectangle(padisplay, sc->xbuf, sc->xcxt, x1-1, y1-1, x2-x1, y2-y1);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the rectangle */
        XWLOCK();
        XDrawRectangle(padisplay, win->xwhan, sc->xcxt, x1-1, y1-1, x2-x1, y2-y1);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw filled rectangle

Draws a filled rectangle in foreground color.

*******************************************************************************/

void pa_frect(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the rectangle */
        XWLOCK();
        XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1-1, y1-1, x2-x1+1, y2-y1+1);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the rectangle */
        XWLOCK();
        XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1-1, y1-1, x2-x1+1, y2-y1+1);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw rounded rectangle

Draws a rounded rectangle in foreground color.

In XWindow, this has to be constructed, since there is no equivalent function.

*******************************************************************************/

void pa_rrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* adjust for X */
    x1--;
    y1--;
    x2--;
    y2--;
    /* limit the size of the corner circles if greater than the height or
        width */
    if (xs > x2-x1+1) xs = x2-x1+1; /* limit rounding elipse */
    if (ys > y2-y1+1) ys = y2-y1+1;
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        XWLOCK();
        /* stroke the sides */
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x1, y1+ys/2, x1, y2-ys/2);
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x2, y1+ys/2, x2, y2-ys/2);
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y1, x2-xs/2, y1);
        XDrawLine(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y2, x2-xs/2, y2);
        /* draw corner arcs */
        XDrawArc(padisplay, sc->xbuf, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
        XDrawArc(padisplay, sc->xbuf, sc->xcxt, x2-xs, y1, xs, ys, 0, 90*64);

        XDrawArc(padisplay, sc->xbuf, sc->xcxt, x1, y2-ys, xs, ys, 180*64,
                 90*64);

        XDrawArc(padisplay, sc->xbuf, sc->xcxt, x2-xs, y2-ys, xs, ys, 270*64,
                 90*64);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        XWLOCK();
        /* stroke the sides */
        XDrawLine(padisplay, win->xwhan, sc->xcxt, x1, y1+ys/2, x1, y2-ys/2);
        XDrawLine(padisplay, win->xwhan, sc->xcxt, x2, y1+ys/2, x2, y2-ys/2);
        XDrawLine(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y1, x2-xs/2, y1);
        XDrawLine(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y2, x2-xs/2, y2);
        /* draw corner arcs */
        XDrawArc(padisplay, win->xwhan, sc->xcxt, x1, y1, xs, ys,
                 90*64, 90*64);
        XDrawArc(padisplay, win->xwhan, sc->xcxt, x2-xs, y1, xs, ys,
                 0, 90*64);
        XDrawArc(padisplay, win->xwhan, sc->xcxt, x1, y2-ys, xs, ys, 180*64,
                 90*64);

        XDrawArc(padisplay, win->xwhan, sc->xcxt, x2-xs, y2-ys, xs, ys, 270*64,
                 90*64);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);

}

/** ****************************************************************************

Draw filled rounded rectangle

Draws a filled rounded rectangle in foreground color.

In XWindow, this has to be constructed, since there is no equivalent function.

The code compensates quite a bit for degenerative cases such as single line
x or y sizes.

*******************************************************************************/

void pa_frrect(FILE* f, int x1, int y1, int x2, int y2, int xs, int ys)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */
    int wm;     /* width of middle rectangle */
    int hm;     /* height of middle rectangle */
    int wtb;    /* width of top/bottom rectangle */
    int htb;    /* height of top/bottom rectangle */
    int wlr;    /* width of left/right rectangle */
    int hlr;    /* height of left/right rectangle */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* adjust for X */
    x1--;
    y1--;
    x2--;
    y2--;
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (x2-x1 >= y2-y1) { /* x >= y */

        /* find the widths and heights of components, and find minimums */
        wm = x2-x1+1; /* set width of middle */
        if (wm < 1) wm = 1;
        hm = y2-y1+1-ys; /* set height of middle */
        if (ys%2) hm++; /* distribute fraction to middle */
        if (hm < 1) hm = 1;
        wtb = x2-x1+1-xs; /* set width of top and bottom */
        if (xs%2) wtb++; /* distribute fraction to top and bottom width */
        if (wtb < 0) wtb = 0;
        htb = ys/2; /* set height of top and bottom */
        if (y2-y1+1-hm < htb) htb = y2-y1+1-hm;
        if (htb < 0) htb = 0;
        if (xs > x2-x1+1) xs = x2-x1+1; /* limit rounding elipse */
        if (ys > y2-y1+1) ys = y2-y1+1;
        if (win->bufmod) { /* buffer is active */

            XWLOCK();
            /* middle rectangle */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1, y1+ys/2, wm, hm);

            /* top */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y1, wtb, htb);

            /* bottom */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y2-ys/2+1, wtb, htb);

            /* draw corner arcs */
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x2-xs+1, y1, xs, ys, 0, 90*64);

            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1, y2-ys+1, xs, ys, 180*64,
                     90*64);

            XFillArc(padisplay, sc->xbuf, sc->xcxt, x2-xs+1, y2-ys+1, xs, ys, 270*64,
                     90*64);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible)  winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            XWLOCK();
            /* middle rectangle */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1, y1+ys/2, wm, hm);

            /* top */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y1, wtb, htb);

            /* bottom */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y2-ys/2+1, wtb, htb);

            /* draw corner arcs */
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1, y1, xs, ys,
                     90*64, 90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x2-xs+1, y1, xs, ys,
                     0, 90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1, y2-ys+1, xs, ys, 180*64,
                     90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x2-xs+1, y2-ys+1, xs, ys, 270*64,
                     90*64);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }

    } else { /* y > x */

        /* find the widths and heights of components, and find minimums */
        wm = x2-x1+1-xs; /* set width of middle */
        if (xs%2) wm++; /* distribute fraction to middle */
        if (wm < 1) wm = 1;
        hm = y2-y1+1; /* set height of middle */
        if (hm < 1) hm = 1;
        wlr = xs/2; /* set width of left and right */
        if (x2-x1+1-wm < wlr) wlr = x2-x1+1-wm;
        if (wlr < 0) wlr = 0;
        hlr = y2-y1+1-ys; /* set height of top and bottom */
        if (ys%2) hlr++; /* distribute fraction to left and right height */
        if (hlr < 0) hlr = 0;
        if (xs > x2-x1+1) xs = x2-x1+1; /* limit rounding elipse */
        if (ys > y2-y1+1) ys = y2-y1+1;
        if (win->bufmod) { /* buffer is active */

            XWLOCK();
            /* middle rectangle */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1+xs/2, y1, wm, hm);

            /* left */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x1, y1+ys/2, wlr, hlr);

            /* right */
            XFillRectangle(padisplay, sc->xbuf, sc->xcxt, x2-xs/2+1, y1+ys/2, wlr, hlr);

            /* draw corner arcs */
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x2-xs+1, y1, xs, ys, 0, 90*64);
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1, y2-ys+1, xs, ys, 180*64,
                     90*64);
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x2-xs-1, y2-ys, xs, ys, 270*64,
                     90*64);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible)  winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            XWLOCK();
            /* middle rectangle */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1+xs/2, y1, wm, hm);

            /* left */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x1, y1+ys/2, wlr, hlr);

            /* right */
            XFillRectangle(padisplay, win->xwhan, sc->xcxt, x2-xs/2+1, y1+ys/2, wlr, hlr);

            /* draw corner arcs */
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1, y1, xs, ys, 90*64, 90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x2-xs+1, y1, xs, ys, 0, 90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1, y2-ys+1, xs, ys, 180*64,
                     90*64);
            XFillArc(padisplay, win->xwhan, sc->xcxt, x2-xs+1, y2-ys+1, xs, ys, 270*64,
                     90*64);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw ellipse

Draws an ellipse with the current foreground color and line width.

*******************************************************************************/

void pa_ellipse(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the ellipse */
        XWLOCK();
        XDrawArc(padisplay, sc->xbuf, sc->xcxt, x1-1, y1-1, x2-x1, y2-y1,
                 0, 360*64);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the ellipse */
        XWLOCK();
        XDrawArc(padisplay, win->xwhan, sc->xcxt, x1-1, y1-1, x2-x1, y2-y1,
                 0, 360*64);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Draw filled ellipse

Draws a filled ellipse with the current foreground color.

*******************************************************************************/

void pa_fellipse(FILE* f, int x1, int y1, int x2, int y2)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the ellipse */
        XWLOCK();
        XFillArc(padisplay, sc->xbuf, sc->xcxt, x1-1, y1-1, x2-x1+1, y2-y1+1,
                 0, 360*64);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the ellipse */
        XWLOCK();
        XFillArc(padisplay, win->xwhan, sc->xcxt, x1-1, y1-1, x2-x1+1, y2-y1+1,
                 0, 360*64);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

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
Note that XWindow draws arcs counterclockwise, so our start and end points are
swapped.

Negative angles are allowed.

*******************************************************************************/

void pa_arc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */
    int a1, a2; /* XWindow angles */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    if (sa != ea) { /* not null */

        a1 = rat2a64(ea); /* convert angles */
        a2 = rat2a64(sa);
        /* find difference accounting for zero crossing */
        if (a1 >= a2) a2 = 360*64-a1+a2;
        else a2 = a2-a1;

        /* set foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
        XWUNLOCK();
        if (win->bufmod) { /* buffer is active */

            /* draw the arc */
            XWLOCK();
            XDrawArc(padisplay, sc->xbuf, sc->xcxt, x1-1, y1-1, x2-x1, y2-y1,
            		 a1, a2);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            /* draw the arc */
            XWLOCK();
            XDrawArc(padisplay, win->xwhan, sc->xcxt, x1-1, y1-1, x2-x1, y2-y1,
            		 a1, a2);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }
        /* reset foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Draw filled arc

Draws a filled arc in the current foreground color. The same comments apply
as for the arc function above.

Note: XWindows was found to be off the origin by 1, so we put corrections in for
this.

*******************************************************************************/

void pa_farc(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */
    int a1, a2; /* XWindow angles */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    if (sa != ea) { /* not null */

        a1 = rat2a64(ea); /* convert angles */
        a2 = rat2a64(sa);
        /* find difference accounting for zero crossing */
        if (a1 >= a2) a2 = 360*64-a1+a2;
        else a2 = a2-a1;

        /* set foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
        XWUNLOCK();
        if (win->bufmod) { /* buffer is active */

            /* draw the ellipse */
            XWLOCK();
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1-1, y1-1, x2-x1+1,
                     y2-y1+1, a1, a2);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            /* draw the ellipse */
            XWLOCK();
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1-1, y1-1, x2-x1+1,
                     y2-y1+1, a1, a2);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }
        /* reset foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Draw filled cord

Draws a filled cord in the current foreground color. The same comments apply
as for the arc function above.

*******************************************************************************/

void pa_fchord(FILE* f, int x1, int y1, int x2, int y2, int sa, int ea)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    int tx, ty; /* temps */
    int a1, a2; /* XWindow angles */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* rationalize the line to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    if (sa != ea) { /* not null */

        a1 = rat2a64(ea); /* convert angles */
        a2 = rat2a64(sa);
        /* find difference accounting for zero crossing */
        if (a1 >= a2) a2 = 360*64-a1+a2;
        else a2 = a2-a1;

        /* set foreground function */
        XWLOCK();
        XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
        XSetArcMode(padisplay, sc->xcxt, ArcChord); /* set chord mode */
        XWUNLOCK();
        if (win->bufmod) { /* buffer is active */

            /* draw the ellipse */
            XWLOCK();
            XFillArc(padisplay, sc->xbuf, sc->xcxt, x1-1, y1-1, x2-x1+1, y2-y1+1,
                     a1, a2);
            XWUNLOCK();

        }
        if (indisp(win)) { /* do it again for the current screen */

            if (!win->visible) winvis(win); /* make sure we are displayed */
            curoff(win); /* hide the cursor */
            /* draw the ellipse */
            XWLOCK();
            XFillArc(padisplay, win->xwhan, sc->xcxt, x1-1, y1-1, x2-x1+1, y2-y1+1,
                     a1, a2);
            XWUNLOCK();
            curon(win); /* show the cursor */

        }
        XWLOCK();
        XSetArcMode(padisplay, sc->xcxt, ArcPieSlice); /* set pie mode */
        /* reset foreground function */
        XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
        XWUNLOCK();

    }

}

/** ****************************************************************************

Draw filled triangle

Draws a filled triangle in the current foreground color.

*******************************************************************************/

void pa_ftriangle(FILE* f, int x1, int y1, int x2, int y2, int x3, int y3)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */
    XPoint pa[3]; /* XWindow points array */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* place the triangle points in the X array */
    pa[0].x = x1;
    pa[0].y = y1;
    pa[1].x = x2;
    pa[1].y = y2;
    pa[2].x = x3;
    pa[2].y = y3;

    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the triangle */
        XWLOCK();
        XFillPolygon(padisplay, sc->xbuf, sc->xcxt, pa, 3, Convex,
                     CoordModeOrigin);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the ellipse */
        XWLOCK();
        XFillPolygon(padisplay, win->xwhan, sc->xcxt, pa, 3, Convex,
                     CoordModeOrigin);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Set pixel

Sets a single logical pixel to the foreground color.

*******************************************************************************/

void pa_setpixel(FILE* f, int x, int y)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        curoff(win); /* hide the cursor */
        /* draw the pixel */
        XWLOCK();
        XDrawPoint(padisplay, sc->xbuf, sc->xcxt, x-1, y-1);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the pixel */
        XWLOCK();
        XDrawPoint(padisplay, win->xwhan, sc->xcxt, x-1, y-1);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

}

/** ****************************************************************************

Set foreground to overwrite

Sets the foreground write mode to overwrite.

*******************************************************************************/

void pa_fover(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdnorm; /* set foreground mode overwrite */
    sc->fmod = mdnorm;

}

/** ****************************************************************************

Set background to overwrite

Sets the background write mode to overwrite.

*******************************************************************************/

void pa_bover(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdnorm; /* set background mode overwrite */
    sc->bmod = mdnorm;

}

/** ****************************************************************************

Set foreground to invisible

Sets the foreground write mode to invisible.

*******************************************************************************/

void pa_finvis(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdinvis; /* set foreground mode invisible */
    sc->fmod = mdinvis;

}

/** ****************************************************************************

Set background to invisible

Sets the background write mode to invisible.

*******************************************************************************/

void pa_binvis(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdinvis; /* set background mode invisible */
    sc->bmod = mdinvis;

}

/** ****************************************************************************

Set foreground to xor

Sets the foreground write mode to xor.

*******************************************************************************/

void pa_fxor(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdxor; /* set foreground mode xor */
    sc->fmod = mdxor;

}

/** ****************************************************************************

Set background to xor

Sets the background write mode to xor.

*******************************************************************************/

void pa_bxor(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdxor; /* set background mode xor */
    sc->bmod = mdxor;

}

/** ****************************************************************************

Set foreground to and

Sets the foreground write mode to and.

*******************************************************************************/

void pa_fand(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdand; /* set foreground mode and */
    sc->fmod = mdand;

}

/** ****************************************************************************

Set background to and

Sets the background write mode to and.

*******************************************************************************/

void pa_band(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdand; /* set background mode and */
    sc->bmod = mdand;

}

/** ****************************************************************************

Set foreground to or

Sets the foreground write mode to or.

*******************************************************************************/

void pa_for(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gfmod = mdor; /* set foreground mode or */
    sc->fmod = mdor;

}

/** ****************************************************************************

Set background to or

Sets the background write mode to or.

*******************************************************************************/

void pa_bor(FILE* f)

{

    winptr win; /* window record pointer */
    scnptr sc;  /* screen buffer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    win->gbmod = mdor; /* set background mode or */
    sc->bmod = mdor;

}

/** ****************************************************************************

Set line width

Sets the width of lines and several other figures.

*******************************************************************************/

void pa_linewidth(FILE* f, int w)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    XWLOCK();
    XSetLineAttributes(padisplay, sc->xcxt, w, LineSolid, CapButt, JoinMiter);
    XWUNLOCK();

}

/** ****************************************************************************

Find character size x

Returns the character width. This only works if the font is fixed. If it is
proportional, this just returns the width of a space, which is the widest
character in the character set.

*******************************************************************************/

int pa_chrsizx(FILE* f)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */

    return (win->charspace); /* return character spacing */

}

/** ****************************************************************************

Find character size y

Returns the character height.

*******************************************************************************/

int pa_chrsizy(FILE* f)

{

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */

    return (win->linespace); /* return line spacing */

}

/** ****************************************************************************

Find number of installed fonts

Finds the total number of installed fonts.

*******************************************************************************/

int pa_fonts(FILE* f)

{

    return (fntcnt); /* just return the global font count */

}

/** ****************************************************************************

Change fonts

Changes the current font to the indicated logical font number.

*******************************************************************************/

void pa_font(FILE* f, int fc)

{

    fontptr fp;  /* font pointer */
    winptr  win; /* windows record pointer */
    scnptr  sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    if (win->screens[win->curupd-1]->autof)
        error(eatoftc); /* cannot perform with auto on */
    if (fc < 1) error(einvfnm); /* invalid font number */
    /* find indicated font */
    fp = fntlst;
    while (fp != NULL && fc > 1) { /* search */

       fp = fp->next; /* next font entry */
       fc--; /* count */

    }
    if (fc > 1)  error(einvfnm); /* invalid font number */
    if (!strlen(fp->fn)) error(efntemp); /* font is not assigned */
    curoff(win); /* remove cursor with old font characteristics */
    win->screens[win->curupd-1]->cfont = fp; /* place new font */
    win->gcfont = fp;
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Find name of font

Returns the name of a font by number.

*******************************************************************************/

void pa_fontnam(FILE* f, int fc, char* fns, int fnsl)

{

    fontptr fp; /* pointer to font entries */
    int i; /* string index */

    if (fc <= 0) error(einvftn); /* invalid number */
    fp = fntlst; /* index top of list */
    while (fc > 1) { /* walk fonts */

       fp = fp->next; /* next font */
       fc = fc-1; /* count */
       if (!fp) error(einvftn); /* check null */

    }
    if (strlen(fp->fn) > fnsl+1) error(eftntl);
    strcpy(fns, fp->fn);

}

/** ****************************************************************************

Change font size

Changes the font sizing to match the given character height. The character
and line spacing are changed, as well as the baseline.

*******************************************************************************/

void pa_fontsiz(FILE* f, int s)

{

    fontptr fp;  /* font pointer */
    winptr  win; /* windows record pointer */
    scnptr  sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    if (win->screens[win->curupd-1]->autof)
        error(eatoftc); /* cannot perform with auto on */
    curoff(win); /* remove cursor with old font characteristics */
    win->gfhigh = s; /* set new font height */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Set character extra spacing y

Sets the extra character space to be added between lines, also referred to
as "leading".

*******************************************************************************/

void pa_chrspcy(FILE* f, int s)

{

    winptr  win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->chrspcy = s; /* set leading */

}

/** ****************************************************************************

Sets extra character space x

Sets the extra character space to be added between characters, referred to
as "spacing".

*******************************************************************************/

void pa_chrspcx(FILE* f, int s)

{

    winptr  win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->chrspcx = s; /* set ledding */

}

/** ****************************************************************************

Find dots per meter x

Returns the number of dots per meter resolution in x.

*******************************************************************************/

int pa_dpmx(FILE* f)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */

    return (win->sdpmx); /* return value */

}

/** ****************************************************************************

Find dots per meter y

Returns the number of dots per meter resolution in y.

*******************************************************************************/

int pa_dpmy(FILE* f)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */

    return (win->sdpmy); /* return value */

}

/** ****************************************************************************

Find string size in pixels

Returns the number of pixels wide the given string would be, considering
character spacing and kerning.

*******************************************************************************/

int pa_strsiz(FILE* f, const char* s)

{

    winptr win; /* window pointer */
    int    rv;

    win = txt2win(f); /* get window pointer from text file */
    XWLOCK();
    rv = XTextWidth(win->xfont, s, strlen(s)); /* return value */
    XWUNLOCK();

    return (rv);

}

/** ****************************************************************************

Find character position in string

Finds the pixel offset to the given character in the string.

*******************************************************************************/

int pa_chrpos(FILE* f, const char* s, int p)

{

    winptr win; /* window pointer */
    int    rv;

    if (p < 0 || p >= strlen(s)) error(estrinx); /* out of range */
    win = txt2win(f); /* get window pointer from text file */
    XWLOCK();
    rv = XTextWidth(win->xfont, s, p); /* return value */
    XWUNLOCK();

    return (rv);

}

/** ****************************************************************************

Write justified text

Writes a string of text with justification. The string and the width in pixels
is specified. Auto mode cannot be on for this function.

XWindow does not provide justification. We implement a simple algorithim that
distributes the space amoung the spaces present in the string.

*******************************************************************************/

void pa_writejust(FILE* f, const char* s, int n)

{

    winptr win; /* window pointer */
    scnptr sc;  /* pointer to update screen */
    int    spc; /* space of spaces */
    int    ns;  /* number of spaces */
    int    ss;  /* spaces total size */
    int    sz;  /* critical size, chars+min space */
    int    cs;  /* size of characters only */
    int    cbs; /* character background spacing */
    int    i;
    int    l;

    win = txt2win(f); /* get window pointer from text file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    if (sc->autof) error(eatopos); /* cannot perform with auto on */
    l = strlen(s); /* find string length */
    /* find critical spacing, that is, spaces are minimum */
    sz = 0; /* clear size */
    ns = 0; /* clear number of spaces */
    cs = 0; /* clear space in characters */
    for (i = 0; i < l; i++) {

        if (s[i] == ' ') { sz += MINJST; ns++; }
        else {

            sz += xwidth(win, s[i]); /* calculate chars+min space */
            cs += xwidth(win, s[i]); /* calculate chars only */

        }

    }
    spc = MINJST; /* set minimum */
    /* if space provided is greater than the minimum, distribute the extra space
       amoung the existing spaces */
    ss = ns*MINJST; /* set minimum distribution of space */
    if (n > sz) { spc = (n-cs)/ns; ss = n-cs; }
    /* Output the string with our choosen spacing */
    for (i = 0; i < l; i++) {

        if (s[i] == ' ') {

            if (spc > ss) cbs = ss; /* set space to next character */
            else cbs = spc;
            /* draw in background */
            if (win->bufmod) { /* buffer is active */

                if (sc->bmod != mdinvis) { /* background is visible */

                    XWLOCK();
                    /* set background function */
                    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
                    /* set background to foreground to draw character background */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    XFillRectangle(padisplay, sc->xbuf, sc->xcxt,
                                   sc->curxg-1, sc->curyg-1,
                                   cbs, win->linespace);
                    /* restore colors */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    /* reset background function */
                    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
                    XWUNLOCK();

                }

            }

            if (indisp(win)) { /* do it again for the current screen */

                if (!win->visible) winvis(win); /* make sure we are displayed */
                curoff(win); /* hide the cursor */
                if (sc->bmod != mdinvis) { /* background is visible */

                    XWLOCK();
                    /* set background function */
                    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->bmod]);
                    /* set background to foreground to draw character background */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                                   sc->curxg-1, sc->curyg-1,
                                   cbs, win->linespace);
                    /* restore colors */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    /* reset background function */
                    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
                    XWUNLOCK();

                }
                curon(win); /* show the cursor */

            }

            /* now move forward */
            if (spc > ss) sc->curxg += ss; /* space off */
            else { sc->curxg += spc; ss -= spc; }

        } else plcchr(win, s[i]); /* print the character with natural spacing */

    }

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

    winptr win; /* window pointer */
    scnptr sc;  /* pointer to update screen */
    int    spc; /* space of spaces */
    int    ns;  /* number of spaces */
    int    ss;  /* spaces total size */
    int    sz;  /* critical size, chars+min space */
    int    cs;  /* size of characters only */
    int    cbs; /* character background spacing */
    int    cp;  /* character pixel position */
    int    crp; /* character result position */
    int    i;
    int    l;

    win = txt2win(f); /* get window pointer from text file */
    sc = win->screens[win->curupd-1]; /* index update screen */
    if (sc->autof) error(eatopos); /* cannot perform with auto on */
    l = strlen(s); /* find string length */
    if (p < 0 || p >= strlen(s))  error(estrinx); /* out of range */
    /* find critical spacing, that is, spaces are minimum */
    sz = 0; /* clear size */
    ns = 0; /* clear number of spaces */
    cs = 0; /* clear space in characters */
    for (i = 0; i < l; i++) {

        if (s[i] == ' ') { sz += MINJST; ns++; }
        else {

            sz += xwidth(win, s[i]); /* calculate chars+min space */
            cs += xwidth(win, s[i]); /* calculate chars only */

        }

    }
    spc = MINJST; /* set minimum */
    /* if space provided is greater than the minimum, distribute the extra space
       amoung the existing spaces */
    ss = ns*MINJST; /* set minimum distribution of space */
    if (n > sz) { spc = (n-cs)/ns; ss = n-cs; }
    cp = 0; /* set 0 offset to character */
    crp = 0; /* clear result position */
    /* Output the string with our choosen spacing */
    for (i = 0; i < l; i++) {

        if (i == p) crp = cp;
        if (s[i] == ' ') {

            /* now move forward */
            if (spc > ss) cp += ss; /* space off */
            else { cp += spc; ss -= spc; }

        } else cp += xwidth(win, s[i]); /* move forward character space */

    }

    return (crp); /* return result */

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

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sacondensed); /* set attribute active */
       win->gattr |= BIT(sacondensed);

    } else { /* turn it off */

       sc->attr &= ~BIT(sacondensed); /* set attribute inactive */
       win->gattr &= ~BIT(sacondensed);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

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

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saextended); /* set attribute active */
       win->gattr |= BIT(saextended);

    } else { /* turn it off */

       sc->attr &= ~BIT(saextended); /* set attribute inactive */
       win->gattr &= ~BIT(saextended);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

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

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saxlight); /* set attribute active */
       win->gattr |= BIT(saxlight);

    } else { /* turn it off */

       sc->attr &= ~BIT(saxlight); /* set attribute inactive */
       win->gattr &= ~BIT(saxlight);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

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

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(salight); /* set attribute active */
       win->gattr |= BIT(salight);

    } else { /* turn it off */

       sc->attr &= ~BIT(salight); /* set attribute inactive */
       win->gattr &= ~BIT(salight);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

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

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saxbold); /* set attribute active */
       win->gattr |= BIT(saxbold);

    } else { /* turn it off */

       sc->attr &= ~BIT(saxbold); /* set attribute inactive */
       win->gattr &= ~BIT(saxbold);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

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

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(sahollow); /* set attribute active */
       win->gattr |= BIT(sahollow);

    } else { /* turn it off */

       sc->attr &= ~BIT(sahollow); /* set attribute inactive */
       win->gattr &= ~BIT(sahollow);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

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

    winptr win; /* windows record pointer */
    scnptr sc;  /* screen pointer */

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (e) { /* strikeout on */

       sc->attr |= BIT(saraised); /* set attribute active */
       win->gattr |= BIT(saraised);

    } else { /* turn it off */

       sc->attr &= ~BIT(saraised); /* set attribute inactive */
       win->gattr &= ~BIT(saraised);

    }

    /* this is a font changing event */
    curoff(win); /* remove cursor with old font characteristics */
    setfnt(win); /* select the font */
    /* select to context */
    XWLOCK();
    XSetFont(padisplay, sc->xcxt, win->xfont->fid);
    XWUNLOCK();
    curon(win); /* replace cursor with new font characteristics */

}

/** ****************************************************************************

Delete picture

Deletes a loaded picture.

*******************************************************************************/

void pa_delpict(FILE* f, int p)

{

    winptr win; /* window pointer */
    picptr pp; /* image pointer */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1]->xi) error(einvhan); /* bad picture handle */
    delpic(win, p); /* delete all of the scaled copies */

}

/** ****************************************************************************

Load picture

Loads a picture into a slot of the loadable pictures array. In this version,
only .bmp files are loaded, and those must be in 24 bit Truecolor.

*******************************************************************************/

/* place extension on filename */
static void setext(char* fnh, char* ext)

{

    char* ec; /* extension character location */
    char* cp;

    /* find extension or end */
    cp = fnh;
    ec = NULL;
    while (*cp) {

        if (*cp == '.' || !*cp) ec = cp;
        cp++;

    }
    if (!*cp && !ec) ec = cp;
    if (cp-fnh+strlen(ext) > MAXFNM)
        error(epicftl); /* filename too large */
    strcpy(ec, ext); /* place extention */

}

byte getbyt(FILE* f)

{

    byte b;
    size_t nb;

    nb = fread(&b, sizeof(byte), 1, f);
    if (nb != 1) error(ebadfmt);

    return (b);

}

unsigned int read32(FILE* f)

{

    union {

        unsigned int i;
        byte         b[4];

    } i2b;
    int i;

    for (i = 0; i < 4; i++) i2b.b[i] = getbyt(f);

    return (i2b.i);

}

unsigned int read16(FILE* f)

{

    union {

        unsigned int i;
        byte         b[4];

    } i2b;
    int i;


    for (i = 0; i < 2; i++) i2b.b[i] = getbyt(f);

    return (i2b.i);

}

void pa_loadpict(FILE* f, int p, char* fn)

{

    winptr win; /* window pointer */
    FILE* pf; /* picture file */
    /* signature of PNG file */
    const byte signature[8] = { 0x42, 0x4d };
    unsigned int pw; /* picture width */
    unsigned int ph; /* picture height */
    byte r, g, b; /* colors */
    int pad;
    Visual* vi;
    byte* frmdat;
    byte* pp;
    int x, y;
    unsigned int t;
    int i;
    unsigned int hs;
    picptr ip; /* image pointer */
    char fnh[MAXFNM]; /* file name holder */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC)  error(einvhan); /* bad picture handle */
    /* if the slot is already occupied, delete that picture */
    delpic(win, p);
    /* copy filename and add extension if required */
    strcpy(fnh, fn); /* copy */
    setext(fnh, ".bmp"); /* set or overwrite extension */
    pf = fopen(fnh, "r"); /* open picture for read only */
    if (!pf) error(epicopn); /* cannot open picture file */
    for (i = 0; i < 2; i++) { /* read and compare signature */

        b = getbyt(pf); /* get next byte */
        if (b != signature[i]) error(ebadfmt);

    }
    read32(pf); /* size of bmp file */
    read16(pf); /* reserved */
    read16(pf); /* reserved */
    read32(pf); /* offset */
    hs = read32(pf); /* size of header */
    pw = read32(pf); /* image width */
    ph = read32(pf); /* image height */
    t = read16(pf); /* get number of planes */
    if (t != 1) error(ebadfmt); /* should be single plane */
    t = read16(pf); /* get number of bits in pixel */
    if (t != 24) error(ebadfmt); /* should be 24 bits */
    t = read32(pf); /* compression type */
    if (t != 0) error(ebadfmt); /* should be no compression */
    read32(pf); /* image size */
    read32(pf); /* pixels per meter x */
    read32(pf); /* pixels per meter y */
    t = read32(pf); /* Number of colors */
    if (t != 0) error(ebadfmt); /* should be no palette */
    read32(pf); /* important colors */
    /* read and dispose of the rest of the header */
    for (i = 0; i < hs-40; i++) getbyt(pf);

    ip = getpic(); /* get new image entry */
    ip->next = win->pictbl[p-1]; /* push to list */
    win->pictbl[p-1] = ip;
    /* set picture size */
    ip->sx = pw;
    ip->sy = ph;
    /* create image structure */
    vi = DefaultVisual(padisplay, 0); /* define direct map color */
    frmdat = (byte*)imalloc(pw*ph*4); /* allocate image frame */
    imgcnt++;
    imgtot += pw*ph*4;
    /* create truecolor image */
    XWLOCK();
    ip->xi = XCreateImage(padisplay, vi, 24, ZPixmap, 0, frmdat, pw, ph, 32, 0);
    XWUNLOCK();

    /* find end of row padding */
    pad = 0;
    if (pw*3%4) pad = 4-(pw*3%4);
    pp = frmdat+pw*ph*4-pw*4; /* index last line */
    /* fill picture with data */
    for (y = ph-1; y >= 0; y--) { /* fill bottom to top */

        for (x = 0; x < pw; x++) { /* fill left to right */

            *pp++ = getbyt(pf); /* get blue */
            *pp++ = getbyt(pf); /* get green */
            *pp++ = getbyt(pf); /* get red */
            pp++; /* skip alpha */

        }
        /* remove padding */
        for (i = 0; i < pad; i++) getbyt(pf);
        pp -= pw*4*2; /* go back one line */

    }
    fclose(pf); /* close the input file */

}

/** ****************************************************************************

Find size x of picture

Returns the size in x of the logical picture.

*******************************************************************************/

int pa_pictsizx(FILE* f, int p)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1]->xi) error(einvhan); /* bad picture handle */

    return (win->pictbl[p-1]->sx); /* return x size */

}

/** ****************************************************************************

Find size y of picture

Returns the size in y of the logical picture.

*******************************************************************************/

int pa_pictsizy(FILE* f, int p)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1]->xi) error(einvhan); /* bad picture handle */

    return (win->pictbl[p-1]->sy); /* return x size */

}

/** ****************************************************************************

Draw picture

Draws a picture from the given file to the rectangle. The picture is resized
to the size of the rectangle.

Images will be kept in a rotating cache to prevent repeating reloads.

*******************************************************************************/

void pa_picture(FILE* f, int p, int x1, int y1, int x2, int y2)

{

    winptr  win; /* window record pointer */
    scnptr  sc;  /* screen buffer */
    int     tx, ty; /* temps */
    int     pw, ph; /* picture width and height */
    picptr  pp, fp; /* picture entry pointers */
    byte*   frmdat;
    Visual* vi;

    win = txt2win(f); /* get window from file */
    sc = win->screens[win->curupd-1];
    if (p < 1 || p > MAXPIC) error(einvhan); /* bad picture handle */
    if (!win->pictbl[p-1]->xi) error(einvhan); /* bad picture handle */
    /* rationalize the rectangle to right/down */
    if (x1 > x2 || (x1 == x2 && y1 > y2)) { /* swap */

       tx = x1;
       ty = y1;
       x1 = x2;
       y1 = y2;
       x2 = tx;
       y2 = ty;

    }
    /* set picture width and height */
    pw = x2-x1+1;
    ph = y2-y1+1;
    pp = win->pictbl[p-1]; /* index top picture */
    fp = NULL; /* set none found */
    while (pp) { /* search for scale that matches */

        if (pp->sx == pw && pp->sy == ph) fp = pp; /* found matching entry */
        pp = pp->next; /* go next */

    }
    if (!fp) {

        /* New scale does not match any previous. We create a new scaled
           image. */
        pp = win->pictbl[p-1]; /* index top picture */
        while (pp->next) pp = pp->next; /* go to bottom of list */
        fp = getpic(); /* get new image entry */
        fp->next = win->pictbl[p-1]; /* push to list */
        win->pictbl[p-1] = fp;
        /* set picture size */
        fp->sx = pw;
        fp->sy = ph;
        /* create image structure */
        vi = DefaultVisual(padisplay, 0); /* define direct map color */
        frmdat = (byte*)imalloc(pw*ph*4); /* allocate image frame */
        imgcnt++;
        imgtot += pw*ph*4;
        /* create truecolor image */
        XWLOCK();
        fp->xi = XCreateImage(padisplay, vi, 24, ZPixmap, 0, frmdat, pw, ph, 32, 0);
        XWUNLOCK();
        rescale(fp->xi, pp->xi); /* rescale to new image */

    }
    /* set foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[sc->fmod]);
    XWUNLOCK();
    if (win->bufmod) { /* buffer is active */

        /* draw the picture */
        XWLOCK();
        XPutImage(padisplay, sc->xbuf, sc->xcxt, fp->xi, 0, 0, x1-1, y1-1,
                  x2-x1+1, y2-y1+1);
        XWUNLOCK();

    }
    if (indisp(win)) { /* do it again for the current screen */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        curoff(win); /* hide the cursor */
        /* draw the rectangle */
        XWLOCK();
        XPutImage(padisplay, win->xwhan, sc->xcxt, fp->xi, 0, 0,
                             x1-1, y1-1, x2-x1+1, y2-y1+1);
        XWUNLOCK();
        curon(win); /* show the cursor */

    }
    /* reset foreground function */
    XWLOCK();
    XSetFunction(padisplay, sc->xcxt, mod2fnc[mdnorm]);
    XWUNLOCK();

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

/* get and process a joystick event */
static void joyevt(pa_evtrec* er, int* keep)

{

    struct js_event ev;

    read(joyfid, &ev, sizeof(ev)); /* get next joystick event */
    if (!(ev.type & JS_EVENT_INIT)) {

        if (ev.type & JS_EVENT_BUTTON) {

            /* we use Linux button numbering, because, what the heck */
            if (ev.value) { /* assert */

                er->etype = pa_etjoyba; /* set assert */
                er->ajoyn = 1; /* set joystick 1 */
                er->ajoybn = ev.number; /* set button number */

            } else { /* deassert */

                er->etype = pa_etjoybd; /* set assert */
                er->djoyn = 1; /* set joystick 1 */
                er->djoybn = ev.number; /* set button number */

            }
            *keep = TRUE; /* set keep event */

        }
        if (ev.type & JS_EVENT_AXIS) {

            /* update the axies */
            if (ev.number == 0) joyax = ev.value*(INT_MAX/32768);
            else if (ev.number == 1) joyay = ev.value*(INT_MAX/32768);
            else if (ev.number == 2) joyaz = ev.value*(INT_MAX/32768);

            er->etype = pa_etjoymov; /* set joystick move */
            er->mjoyn = 1; /* set joystick number */
            er->joypx = joyax; /* place joystick axies */
            er->joypy = joyay;
            er->joypz = joyaz;
            *keep = TRUE; /* set keep event */

        }

    }

}

/* Update mouse parameters.
   Use state flags to create events. */

static void mouseupdate(winptr win, pa_evtrec* er, int* keep)

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

/* Register mouse status.
   Get mouse status from XWindow event to window data flags. */

static void mouseevent(winptr win, XEvent* e)

{

    if (e->type == MotionNotify) {

        win->nmpx = e->xmotion.x/win->charspace+1; /* get mouse x */
        win->nmpy = e->xmotion.y/win->linespace+1; /* get mouse y */
        win->nmpxg = e->xmotion.x+1; /* get mouse graphical x */
        win->nmpyg = e->xmotion.y+1; /* get mouse graphical y */

    } else if (e->type == ButtonPress) {

        if (e->xbutton.button == Button1) win->nmb1 = TRUE;
        else if (e->xbutton.button == Button2) win->nmb2 = TRUE;
        else if (e->xbutton.button == Button3) win->nmb3 = TRUE;

    } else if (e->type = ButtonRelease) {

        if (e->xbutton.button == Button1) win->nmb1 = FALSE;
        else if (e->xbutton.button == Button2) win->nmb2 = FALSE;
        else if (e->xbutton.button == Button3) win->nmb3 = FALSE;

    }

}

/* rectangle */
typedef struct { int x1, y1, x2, y2; } rectangle;

/* set rectangle to values */
void setrect(rectangle* r, int x1, int y1, int x2, int y2)

{

    r->x1 = x1;
    r->y1 = y1;
    r->x2 = x2;
    r->y2 = y2;

}

/* find if rectangles intersect */
int intersect(rectangle* r1, rectangle* r2)

{

    return ((*r1).x2 >= (*r2).x1 && (*r1).x1 <= (*r2).x2 &&
            (*r1).y2 >= (*r2).y1 && (*r1).y1 <= (*r2).y2);

}

/* find intersection of rectangles as a rectangle (meaningless if they don't
   intersect) */
void intersection(rectangle* ri, rectangle* r1, rectangle* r2)

{

    /* copy to destination */
    ri->x1 = r1->x1;
    ri->x2 = r1->x2;
    ri->y1 = r1->y1;
    ri->y2 = r1->y2;

    /* find intersection */
    if (r1->x1 < r2->x1) ri->x1 = r2->x1;
    if (r1->x2 > r2->x2) ri->x2 = r2->x2;
    if (r1->y1 < r2->y1) ri->y1 = r2->y1;
    if (r1->y2 > r2->y2) ri->y2 = r2->y2;

}

/* find rectangle is null */

int zerorect(rectangle* r)

{

    return (!(r->x1 | r->x2 | r->y1 | r->y2));

}

/* Subtract rectangles. Relies on the rectangles to be rational and
   intersecting. */
void subrect(rectangle* r1, rectangle* r2, rectangle* rr, rectangle* rb)

{

    /* right full */
    rr->x1 = r1->x2+1;
    rr->x2 = r2->x2;
    rr->y1 = r2->y1;
    rr->y2 = r2->y2;

    /* bottom partial */
    rb->x1 = r2->x1;
    rb->x2 = r2->x2;
    rb->y1 = r1->y2+1;
    rb->y2 = r2->y2;

    /* if null, zero out */
    if (rr->x1 > rr->x2) rr->x1 = rr->x2 = rr->y1 = rr->y2 = 0;
    if (rb->y1 > rb->y2) rb->x1 = rb->x2 = rb->y1 = rb->y2 = 0;

}

/* XWindow event process */

static void xwinevt(winptr win, pa_evtrec* er, XEvent* e, int* keep)

{

    KeySym         ks;
    scnptr         sc;  /* screen pointer */
    rectangle      r1, r2, ri, rr, rb;
    XWindowChanges xwc; /* XWindow values */
    XEvent         xe;
    winptr         mwin;

    sc = win->screens[win->curdsp-1]; /* index screen */
    if (e->type == Expose && win->xmwhan != e->xany.window) {

        if (win->bufmod) { /* use buffer to satisfy event */

            /* make expose mask into rectangle */
            setrect(&r1, e->xexpose.x, e->xexpose.y,
                    e->xexpose.x+e->xexpose.width-1,
                    e->xexpose.y+e->xexpose.height-1);
            /* make buffer into 0,0 rectangle */
            setrect(&r2, 0, r2.y1 = 0, win->gmaxxg-1, win->gmaxyg-1);
            if (intersect(&r1, &r2)) {

                intersection(&ri, &r1, &r2); /* find intersection of those */
                XWLOCK();
                XCopyArea(padisplay, sc->xbuf, win->xwhan, sc->xcxt, ri.x1, ri.y1,
                      ri.x2-ri.x1+1, ri.y2-ri.y1+1, ri.x1, ri.y1);
                subrect(&r2, &r1, &rr, &rb); /* find subtraction r1-r2 */
                /* check any result */
                if (!zerorect(&rr) || !zerorect(&rb)) {

                    /* set background color to foreground */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    /* paint right */
                    if (!zerorect(&rr))
                        XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                                       rr.x1, rr.y1,
                                       rr.x2-rr.x1+1, rr.y2-rr.y1+1);
                    /* paint bottom */
                    if (!zerorect(&rb))
                        XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                                       rb.x1, rb.y1,
                                       rb.x2-rb.x1+1, rb.y2-rb.y1+1);
                    /* restore foreground color */
                    if (BIT(sarev) & sc->attr)
                        XSetForeground(padisplay, sc->xcxt, sc->bcrgb);
                    else XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                    /* we shouldn't need to do this, but I have seen unpainted
                       of the window if not while resizing */
                    XFlush(padisplay);

                }
                XWUNLOCK();

            } else {

                /* paint right or bottom off buffer space */
                XWLOCK();
                XSetForeground(padisplay, sc->xcxt, sc->fcrgb);
                XFillRectangle(padisplay, win->xwhan, sc->xcxt,
                               e->xexpose.x, e->xexpose.y,
                               e->xexpose.width, e->xexpose.height);
                XFlush(padisplay);
                XWUNLOCK();

            }

        } else { /* let the client handle it */

            er->etype = pa_etredraw; /* set redraw event */
            er->rsx = e->xexpose.x+1; /* set redraw rectangle */
            er->rsy = e->xexpose.y+1;
            er->rex = e->xexpose.x+e->xexpose.width;
            er->rey = e->xexpose.y+e->xexpose.height;
            *keep = TRUE; /* set found */

        }

    } else if (e->type == ConfigureNotify) {

        if (win->xmwhan == e->xany.window) { /* it's the master window */

            /* find size of subclient */
            xwc.width = e->xconfigure.width; /* set frameless offset to client */
            xwc.height = e->xconfigure.height;
            if (win->menu) /* if menu is active, remove that space from subclient */
                xwc.height = e->xconfigure.height-win->menuspcy;
            if (xwc.height <= 0) xwc.height = 1; /* set minimum height */
            /* check subclient window has changed size */
            if (xwc.width != win->xwr.w || xwc.height != win->xwr.h) {

                XWLOCK();
                XConfigureWindow(padisplay, win->xwhan, CWWidth|CWHeight, &xwc);
                XWUNLOCK();
                /* wait for the configure response with correct sizes */
                do { peekxevt(&xe); /* peek next event */
                } while (xe.type != ConfigureNotify || xe.xconfigure.width != xwc.width ||
                         xe.xconfigure.height != xwc.height || xe.xany.window != win->xwhan);
                /* change saved size to match */
                win->xwr.w = xwc.width;
                win->xwr.h = xwc.height;

            }

            /* if menu bar is active, also send a configure to it */
            if (win->menu) {

                mwin = txt2win(win->menu->wf); /* index window */
                /* find resulting size of menu bar */
                xwc.width = e->xconfigure.width; /* width is client */
                xwc.height = win->menuspcy; /* height is menu text */
                /* check menu bar has changed size */
                if (xwc.width != mwin->xmwr.w || xwc.height != mwin->xmwr.h) {

                    XWLOCK();
                    XConfigureWindow(padisplay, mwin->xmwhan, CWWidth|CWHeight, &xwc);
                    XWUNLOCK();
                    /* wait for the configure response with correct sizes */
                    do { peekxevt(&xe); /* peek next event */
                    } while (xe.type != ConfigureNotify || xe.xconfigure.width != xwc.width ||
                             xe.xconfigure.height != xwc.height || xe.xany.window != mwin->xmwhan);
                    /* change saved size to match */
                    mwin->xmwr.w = xwc.width;
                    mwin->xmwr.h = xwc.height;

                }

            }

        } else { /* its the subclient window */

            /* size of window has changed, send event */
            er->etype = pa_etresize; /* set resize event */
            er->rszxg = e->xconfigure.width; /* set graphics size */
            er->rszyg = e->xconfigure.height;
            er->rszx = e->xconfigure.width/win->charspace; /* set character size */
            er->rszy = e->xconfigure.height/win->linespace;
            *keep = TRUE; /* set found */
            if (!win->bufmod) {

                /* reset tracking sizes */
                win->gmaxxg = er->rszxg; /* graphics x */
                win->gmaxyg = er->rszyg; /* graphics y */
                /* find character size x */
                win->gmaxx = win->gmaxxg/win->charspace;
                /* find character size y */
                win->gmaxy = win->gmaxyg/win->linespace;

            }

        }

    } else if (e->type == KeyPress) {

        XWLOCK();
        ks = XLookupKeysym(&e->xkey, 0);
        XWUNLOCK();
        er->etype = pa_etchar; /* place default code */
        if (ks >= ' ' && ks <= 0x7e && !ctrll && !ctrlr && !altl && !altr) {

            /* issue standard key event */
            er->etype = pa_etchar; /* set event */
            /* set code, normal or shifted */
            if (shiftl || shiftr) er->echar = !capslock?toupper(ks):ks;
            else er->echar = capslock?toupper(ks):ks; /* set code */
            *keep = TRUE; /* set found */

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
                *keep = TRUE; /* a control was found */

        }

    } else if (e->type == KeyRelease) {

        /* Petit-ami does not track key releases, but we need to account for
          control and shift keys up/down */
        XWLOCK();
        ks = XLookupKeysym(&e->xkey, 0); /* find code */
        XWUNLOCK();
        switch (ks) {

            case XK_Shift_L:   shiftl = FALSE; break; /* Left shift */
            case XK_Shift_R:   shiftr = FALSE; break; /* Right shift */
            case XK_Control_L: ctrll = FALSE; break;  /* Left control */
            case XK_Control_R: ctrlr = FALSE; break;  /* Right control */
            case XK_Alt_L:     altl = FALSE; break;  /* Left alt */
            case XK_Alt_R:     altr = FALSE; break;  /* Right alt */

        }

    } else if ((e->type == MotionNotify || e->type == ButtonPress ||
               e->type == ButtonRelease) && mouseenb) {

        mouseevent(win, e); /* process mouse event */
        /* check any mouse details need processing */
        mouseupdate(win, er, keep);

    } else if (e->type == ClientMessage){

        /* windows manager has message for us */
        if ((Atom)e->xclient.data.l[0] == win->delmsg) {

            /* terminate client window */
            er->etype = pa_etterm;
            fend = TRUE;
            *keep = TRUE;

        }

    }

}

/* prepare and process XWindow event */
static void xwinprc(XEvent* e, pa_evtrec* er, int* keep)

{

    winptr     win; /* window record pointer */
    int        ofn; /* output lfn associated with window */
    int        rv;

    if (dmpmsg) {

        /* note we don't print XEvent diagnostics until they get extracted from
           the input queue */
        prtxevt(e);

    }
    ofn = fndevt(e->xany.window); /* get output window lfn */
    if (ofn >= 0) { /* its one of our windows */

        win = lfn2win(ofn); /* get window for that */
        er->winid = filwin[ofn]; /* get window number */
        xwinevt(win, er, e, keep); /* process XWindow event */

    }

}

/* get and process XWindow event */
static void xwinget(pa_evtrec* er, int* keep)

{

    XEvent e;  /* XWindow event record */
    int    rv;

    XWLOCK();
    rv = XPending(padisplay);
    XWUNLOCK();
    if (rv) {

        XWLOCK();
        XNextEvent(padisplay, &e); /* get next event */
        XWUNLOCK();
        xwinprc(&e, er, keep); /* pass to processing */

    }

}

static void ievent(FILE* f, pa_evtrec* er)

{

    int        keep;     /* keep event flag */
    int        dfid;     /* XWindow display FID */
    int        rv;       /* return value */
    static int ecnt = 0; /* PA event counter */
    uint64_t   exp;      /* timer expiration time */
    winptr     win;      /* window record pointer */
    XEvent     e;        /* XWindow event */
    int        i;

    /* make sure all drawing is complete before we take inputs */
    XWLOCK();
    XFlush(padisplay);
    XWUNLOCK();
    keep = FALSE; /* set do not keep event */
    dfid = ConnectionNumber(padisplay); /* find XWindow display fid */
    do {

        /* check input queue has events */
        while (evtque && !keep) {

            dequexevt(&e); /* remove event from queue */
            xwinprc(&e, er, &keep); /* process */

        }

        /* search for active event */
        if (!keep)
            for (i = 0; i < ifdmax && !keep; i++)
                if (FD_ISSET(i, &ifdsets)) {

            FD_CLR(i, &ifdsets); /* remove event from input sets */
            XWLOCK();
            rv = XPending(padisplay);
            XWUNLOCK();
            if (opnfil[i] && opnfil[i]->tim) { /* do timer event */

                win = opnfil[i]->twin; /* get window containing timer */
                er->etype = pa_ettim; /* set timer event */
                er->timnum = opnfil[i]->tim; /* set timer number */
                er->winid = win->wid; /* set window number */
                keep = TRUE; /* set keep */
                /* clear the timer by reading it */
                read(i, &exp, sizeof(uint64_t));

            } else if (i == dfid && rv)
                xwinget(er, &keep);
            else if (i == joyfid && joyenb)
                joyevt(er, &keep); /* process joystick events */
            else if (i == frmfid) {

                er->etype = pa_etframe; /* set frame event occurred */
                keep = TRUE; /* set keep event */
                /* clear the timer by reading it */
                read(i, &exp, sizeof(uint64_t));

            }

        }

        /* no more active selects, get a new select set */
        if (!keep) {

            /* check the queue before select() */
            xwinget(er, &keep);
            if (!keep) { /* still no event */

                /* we found no event, get a new select set */
                ifdsets = ifdseta; /* set up request set */
                rv = select(ifdmax, &ifdsets, NULL, NULL, NULL);
                /* if error, the input set won't be modified and thus will
                   appear as if they were active. We clear them in this case */
                if (rv < 0) FD_ZERO(&ifdsets);

            }

        }

    } while (!keep); /* until we have a client event */


    /* do diagnostic dump of PA events */
    if (dmpevt) {

        dbg_printf(dlinfo, "PA Event: %5d Window: %d ", ecnt++, er->winid);
        prtevt(er->etype); fprintf(stderr, "\n"); fflush(stderr);

    }

}

/* external event interface */

void pa_event(FILE* f, pa_evtrec* er)

{

    do { /* loop handling via event vectors and queuing */

        /* check input PA queue */
        if (paqevt) dequepaevt(er);
        /* get logical input file number for input, and get the event for that. */
        else ievent(f, er); /* process event */
        er->handled = 1; /* set event is handled by default */
        (evtshan)(er); /* call master event handler */
        if (!er->handled) { /* send it to fanout */

            er->handled = 1; /* set event is handled by default */
            (*evthan[er->etype])(er); /* call event handler first */

        }

    } while (er->handled);
    /* event not handled, return it to the caller */

}

/** ****************************************************************************

Send event to window

Send an event to the given window. The event is placed into the queue for the
given window. Note that the input side of the window is found, and the event
spooled for that side. Note that any window number given the event is
overwritten with the proper window id after a copy is made.

The difference between this and inserting to the event chain is that this
routine enters to the top of the chain, and specifies the input side of the
window. Thus it is a more complete send of the event.

*******************************************************************************/

void pa_sendevent(FILE* f, pa_evtrec* er)

{

    winptr win;   /* pointer to windows context */
    int fn;       /* logical file number */
    pa_evtrec ec; /* copy of event record */

    fn = fileno(f); /* find find number */
    if (fn < 0) error(einvfil); /* file invalid */
    if (opnfil[fn]->inl < 0) error(enoinps); /* no input side for window */
    win = lfn2win(fn); /* index window for file */
    memcpy(&ec, er, sizeof(pa_evtrec));
    ec.winid = win->wid; /* overwrite window id */
    enquepaevt(&ec); /* send to queue */

}

/** ****************************************************************************

Override event handler

Overrides or "hooks" the indicated event handler. The existing event handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

void pa_eventover(pa_evtcod e, pa_pevthan eh,  pa_pevthan* oeh)

{

    *oeh = evthan[e]; /* save existing event handler */
    evthan[e] = eh; /* place new event handler */

}

/** ****************************************************************************

Override master event handler

Overrides or "hooks" the master event handler. The existing event handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

void pa_eventsover(pa_pevthan eh,  pa_pevthan* oeh)

{

    *oeh = evtshan; /* save existing event handler */
    evtshan = eh; /* place new event handler */

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

    winptr win; /* windows record pointer */
    struct itimerspec ts;
    int    rv;
    long   tl;
    int    tfid;

    if (i < 1 || i > PA_MAXTIM) error(einvhan); /* invalid timer handle */
    win = txt2win(f); /* get window from file */
    if (win->timers[i-1] < 0) { /* timer entry inactive, create a timer */

        tfid = timerfd_create(CLOCK_REALTIME, 0);
        if (tfid == -1) error(etimacc);
        win->timers[i-1] = tfid; /* place in timers equ */
        /* place new file in active select set */
        FD_SET(tfid, &ifdseta);
        /* if the new file handle is greater than  any existing, set new max */
        if (tfid+1 > ifdmax) ifdmax = tfid+1;
        /* create entry in fid table */
        if (!opnfil[tfid]) getfil(&opnfil[tfid]); /* get fet if empty */
        opnfil[tfid]->tim = i; /* place timer equ number */
        opnfil[tfid]->twin = win; /* place window containing timer */

    }

    /* set timer run time */
    tl = t;
    ts.it_value.tv_sec = tl/10000; /* set number of seconds to run */
    ts.it_value.tv_nsec = tl%10000*100000; /* set number of nanoseconds to run */

    /* set if timer does not rerun */
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    if (r) { /* timer reruns */

        ts.it_interval.tv_sec = ts.it_value.tv_sec;
        ts.it_interval.tv_nsec = ts.it_value.tv_nsec;

    }

    rv = timerfd_settime(win->timers[i-1], 0, &ts, NULL);
    if (rv < 0) error(etimacc); /* could not set time */

}

/** ****************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

*******************************************************************************/

void pa_killtimer(FILE* f, /* file to kill timer on */
                  int   i  /* handle of timer */
                 )

{

    winptr win; /* windows record pointer */
    struct itimerspec ts;
    int rv;

    if (i < 1 || i > PA_MAXTIM) error(einvhan); /* invalid timer handle */
    win = txt2win(f); /* get window from file */
    if (!win->timers[i-1] < 0) error(etimacc); /* no such timer */

    /* set timer run time to zero to kill it */
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    rv = timerfd_settime(win->timers[i-1], 0, &ts, NULL);
    if (rv < 0) error(etimacc); /* could not set time */

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

    struct itimerspec ts; /* linux timer structure */
    int               rv; /* return value */

    if (e) { /* set framing timer to run */

        /* set timer run time */
        ts.it_value.tv_sec = 0; /* set number of seconds to run */
        ts.it_value.tv_nsec = 16666667; /* set number of nanoseconds to run */

        /* set rerun time */
        ts.it_interval.tv_sec = ts.it_value.tv_sec;
        ts.it_interval.tv_nsec = ts.it_value.tv_nsec;

        rv = timerfd_settime(frmfid, 0, &ts, NULL);
        if (rv < 0) error(etimacc); /* could not set time */

    } else {

        /* set timer run time to zero to kill it */
        ts.it_value.tv_sec = 0;
        ts.it_value.tv_nsec = 0;
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;

        rv = timerfd_settime(frmfid, 0, &ts, NULL);
        if (rv < 0) error(etimacc); /* could not set time */

    }

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

Returns the number of mice implemented. XWindow supports only one mouse.

*******************************************************************************/

int pa_mouse(FILE* f)

{

    return 1;

}

/** ****************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version. XWindow supports from 1 to 5 buttons, but we limit it to 3.

*******************************************************************************/

int pa_mousebutton(FILE* f, int m)

{

    return 3;

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

int pa_joystick(FILE* f)

{

    winptr win; /* window pointer */
    int    jn;  /* joystick number */

    win = txt2win(f); /* get window pointer from text file */
    jn = numjoy; /* set number of joysticks */

    return (jn);

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

int pa_joybutton(FILE* f, int j)

{

    if (j < 1 || j > numjoy) error(einvjoy); /* bad joystick id */

    return (3); /* return button number */

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

int pa_joyaxis(FILE* f, int j)

{

    if (j < 1 || j > numjoy) error(einvjoy); /* bad joystick id */

    return (3); /* set axis number */

}

/** ****************************************************************************

Set tab graphical

Sets a tab at the indicated pixel number.

*******************************************************************************/

void pa_settabg(FILE* f, int t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, t); /* translate to graphical call */

}

/** ****************************************************************************

Set tab

Sets a tab at the indicated collumn number.

*******************************************************************************/

void pa_settab(FILE* f, int t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    isettabg(win, (t-1)*win->charspace+1); /* translate to graphical call */

}

/** ****************************************************************************

Reset tab graphical

Resets the tab at the indicated pixel number.

*******************************************************************************/

void pa_restabg(FILE* f, int t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, t); /* translate to graphical call */

}

/** ****************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void pa_restab(FILE* f, int t)

{

    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    irestabg(win, (t-1)*win->charspace+1); /* translate to graphical call */

}

/** ****************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void pa_clrtab(FILE* f)

{

    int    i;
    winptr win; /* window pointer */

    win = txt2win(f); /* get window pointer from text file */
    for (i = 0; i < MAXTAB; i++) win->screens[win->curupd-1]->tab[i] = 0;

}

/** ****************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

*******************************************************************************/

int pa_funkey(FILE* f)

{

    return (12); /* number of function keys */

}

/** ****************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

void pa_title(FILE* f, char* ts)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    XWLOCK();
    XStoreName(padisplay, win->xmwhan, ts);
    XSetIconName(padisplay, win->xwhan, ts);
    XWUNLOCK();

}

/** ****************************************************************************

Allocate buried window id

Allocates and returns a "buried" window id. The window id numbers are assigned
by the client program. However, there a an alternative set of ids that are
allocated as needed. Graphics keeps track of which buried ids have been
allocated and which have been freed.

The implementation here is to assign buried window ids negative numbers,
starting with -1 and proceeding downwards. 0 is never assigned. The calls that
take window ids will recognize those ids as special. The use of negative ids
insure that the normal wids will never overlap any buried wids.

The main use of buried wids is in widgets, the internal workings of which the
client isn't supposed to know about.

Note that the wid entry will actually be opened by openwin(), and will be closed
by closewin(), so there is no need to deallocate this wid.

Note also that this routine is not particularly multitask friendly, since
another task could swoop in and take the wid away.

*******************************************************************************/

int pa_getwid(void)

{

    int wid; /* window id */

    wid = -1; /* start at -1 */
    /* find any open entry */
    while (wid > -MAXFIL && xltwin[wid+MAXFIL] >= 0) wid--;
    if (wid == -MAXFIL) error(enowid); /* ran out of buried wids */

    return (wid); /* return the wid */

}

/** ****************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.

The window id can be from 1 to MAXFIL, but 1 is reserved for the main I/O
window.

*******************************************************************************/

/* check file is already in use */
static int fndfil(FILE* fp)

{

    int fi; /* file index */
    int ff; /* found file */

    ff = -1; /* set no file found */
    for (fi = 0; fi < MAXFIL; fi++)
        if (opnfil[fi] && opnfil[fi]->sfp == fp) ff = fi; /* set found */

    return (ff);

}

void pa_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid)

{

    int ifn, ofn, pfn; /* file logical handles */

    /* check valid window handle */
    if (!wid || wid < -MAXFIL || wid > MAXFIL) error(einvwin);
    /* check if the window id is already in use */
    if (xltwin[wid+MAXFIL] >= 0) error(ewinuse); /* error */
    if (parent) {

        txt2win(parent); /* validate parent is a window file */
        pfn = txt2lfn(parent); /* get logical parent */

    } else pfn = -1; /* set no parent */
    ifn = fndfil(*infile); /* find previous open input side */
    if (ifn < 0) { /* no other input file, open new */

        /* open input file */
        *infile = fopen("/dev/null", "r"); /* open null as read only */
        if (!*infile) error(enoopn); /* can't open */
        setvbuf(*infile, NULL, _IONBF, 0); /* turn off buffering */

    }
    /* open output file */
    *outfile = fopen("/dev/null", "w");
    ofn = fileno(*outfile); /* get logical file no. */
    if (ofn == -1) error(esystem);
    if (!*outfile) error(enoopn); /* can't open */
    setvbuf(*outfile, NULL, _IONBF, 0); /* turn off buffering */

    /* check either input is unused, or is already an input side of a window */
    if (opnfil[ifn]) /* entry exists */
        if (!opnfil[ifn]->inw || opnfil[ifn]->win) error(einmode); /* wrong mode */
    /* check output file is in use for input or output from window */
    if (opnfil[ofn]) /* entry exists */
        if (opnfil[ofn]->inw || opnfil[ofn]->win)
            error(efinuse); /* file in use */
    /* establish all logical files and links, translation tables, and open window */
    openio(*infile, *outfile, ifn, ofn, pfn, wid);

}

/** ****************************************************************************

Size buffer pixel

Sets or resets the size of the buffer surface, in pixel units.

*******************************************************************************/

void pa_sizbufg(FILE* f, int x, int y)

{

    int            si;  /* index for current display screen */
    XWindowChanges xwc; /* XWindow values */
    winptr         win; /* pointer to windows context */
    XEvent         e;   /* XWindow event */

    if (x < 1 || y < 1)  error(einvsiz); /* invalid buffer size */
    win = txt2win(f); /* get window context */
    /* set buffer size */
    win->gmaxx = x/win->charspace; /* find character size x */
    win->gmaxy = y/win->linespace; /* find character size y */
    win->gmaxxg = x; /* set size in pixels x */
    win->gmaxyg = y; /* set size in pixels y */
    /* all the screen buffers are wrong, so tear them out */
    for (si = 0; si < MAXCON; si++) {

        disscn(win, win->screens[si]);
        ifree(win->screens[si]); /* free screen data */
        win->screens[si] = NULL; /* clear screen data */

    }
    win->screens[win->curdsp-1] = imalloc(sizeof(scncon));
    iniscn(win, win->screens[win->curdsp-1]); /* initalize screen buffer */
    if (win->curdsp != win->curupd) { /* also create the update buffer */

        win->screens[win->curupd-1] = imalloc(sizeof(scncon)); /* get the display screen */
        iniscn(win, win->screens[win->curupd-1]); /* initalize screen buffer */

    }
    xwc.width = win->gmaxxg; /* set XWindow width and height */
    xwc.height = win->gmaxyg;
    XWLOCK();
    XConfigureWindow(padisplay, win->xwhan, CWWidth|CWHeight, &xwc);
    XWUNLOCK();

    /* wait for the configure response with correct sizes */
    do {

        peekxevt(&e); /* peek next event */

    } while (e.type != ConfigureNotify && e.xconfigure.width != x ||
             e.xconfigure.height != y || e.xany.window != win->xwhan);

    restore(win); /* restore buffer to screen */

}

/** ****************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

void pa_sizbuf(FILE* f, int x, int y)

{

    winptr win; /* pointer to windows context */

    win = txt2win(f); /* get window context */
    /* just translate from characters to pixels and do the resize in pixels. */
    pa_sizbufg(f, x*win->charspace, y*win->linespace);

}

/** ****************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

void pa_buffer(FILE* f, int e)

{

    winptr            win; /* pointer to windows context */
    XWindowChanges    xwc; /* XWindow values */
    XWindowAttributes xwa; /* XWindow attributes */
    XEvent            xe;  /* XWindow event */
    int               si;  /* index for screens */

    win = txt2win(f); /* get window context */
    if (e) { /* perform buffer on actions */

        win->bufmod = TRUE; /* turn buffer mode on */
        /* restore last buffer size */
        win->gmaxxg = win->bufxg; /* pixel size */
        win->gmaxyg = win->bufyg;
        win->gmaxx = win->bufx; /* character size */
        win->gmaxy = win->bufy;
        win->screens[win->curdsp-1]->maxxg = win->gmaxxg; /* pixel size */
        win->screens[win->curdsp-1]->maxyg = win->gmaxyg;
        win->screens[win->curdsp-1]->maxx = win->gmaxx; /* character size */
        win->screens[win->curdsp-1]->maxy = win->gmaxy;
        xwc.width = win->gmaxxg; /* set XWindow width and height */
        xwc.height = win->gmaxyg;
        XWLOCK();
        XConfigureWindow(padisplay, win->xwhan, CWWidth|CWHeight, &xwc);
        XWUNLOCK();
        restore(win); /* restore buffer to screen */

    } else if (win->bufmod) { /* perform buffer off actions */

        /* The screen buffer contains a lot of drawing information, so we have
           to keep one of them. We keep the current display, and force the
           update to point to it as well. This single buffer  serves as
           a "template" for the real pixels on screen. */
        win->bufmod = FALSE; /* turn buffer mode off */
        /* dispose of screen data structures */
        for (si = 0; si < MAXCON; si++) if (si != win->curdsp-1)
            if (win->screens[si]) {

            disscn(win, win->screens[si]); /* free buffer data */
            ifree(win->screens[si]); /* free screen data */
            win->screens[si] = NULL; /* clear screen data */

        }
        win->curupd = win->curdsp; /* unify the screens */
        /* get actual size of onscreen window, and set that as client space */
        XWLOCK();
        XGetWindowAttributes(padisplay, win->xwhan, &xwa);
        XWUNLOCK();
        win->gmaxxg = xwa.width; /* return size */
        win->gmaxyg = xwa.height;
        win->gmaxx = win->gmaxxg/win->charspace; /* find character size x */
        win->gmaxy = win->gmaxyg/win->linespace; /* find character size y */
        /* tell the window to resize */
        xe.type = ConfigureNotify;
        xe.xconfigure.width = win->gmaxxg;
        xe.xconfigure.height = win->gmaxyg;
        xe.xconfigure.window = win->xwhan;
        XSendEvent(padisplay, win->xwhan, FALSE, 0, &xe);
        /* tell the window to repaint */
        xe.type = Expose;
        xe.xexpose.x = 0;
        xe.xexpose.y = 0;
        xe.xexpose.width = win->gmaxxg;
        xe.xexpose.height = win->gmaxyg;
        xe.xexpose.window = win->xwhan;
        XSendEvent(padisplay, win->xwhan, FALSE, 0, &xe);

    }

}

/** ****************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

*******************************************************************************/

/* insert at list end */
static void insend(metptr* root, metptr mp)

{

    metptr lp; /* last entry */

    mp->next = NULL; /* clear next */
    if (*root) {

        /* find last entry */
        lp = *root;
        while (lp->next) lp = lp->next;
        lp->next = mp; /* set new last */

    } else *root = mp; /* insert only entry */

}

/* create menu tracking entry */
static void mettrk(
    /** window file for menu */          FILE*      f,
    /** window structure */              winptr     win,
    /** root of menu list */             metptr*    root,
    /** client side menu tree pointer */ pa_menuptr m,
    /** new entry created */             metptr*    nm)

{

    metptr mp; /* menu tracking entry pointer */

    mp = getmet(); /* get a new tracking entry */
    insend(root, mp); /* insert to end */
    mp->branch = NULL; /* clear branch */
    mp->frame = NULL; /* clear frame */
    mp->head = win->metlst; /* point back to root (used for floating menus) */
    mp->menubar = FALSE; /* set not menu bar */
    mp->frm = FALSE; /* set not frame */
    mp->onoff = FALSE; /* set no on/off highlighter */
    if (m) mp->onoff = m->onoff; /* place on/off highlighter */
    mp->select = FALSE; /* place status of select (off) */
    mp->id = 0; /* set invalid id */
    if (m) mp->id = m->id; /* place id */
    mp->oneof = NULL; /* set no "one of" */
    mp->bar = FALSE; /* set no bar under */
    if (m) mp->bar = m->bar; /* place bar state */
    /* set up the button properties */
    mp->pressed = FALSE; /* not pressed */
    mp->wf = NULL; /* set no window file attached */
    mp->title = NULL; /* set no title */
    if (m) mp->title = str(m->face); /* copy face string */
    mp->evtfil = f; /* set menu event post file */
    mp->prime = FALSE; /* set not prime menu */
    /* We are walking backwards in the list, and we need the next list entry
      to know the "one of" chain. So we tie the entry to itself as a flag
      that it chains to the next entry. That chain will get fixed on the
      next entry. */
    if (m) if (m->oneof) mp->oneof = mp;
    /* now tie the last entry to this if indicated */
    if (mp->next) /* there is a next entry */
        if (mp->next->oneof == mp->next) mp->next->oneof = mp;
    *nm = mp; /* pass back created entry */

}

/* create menu list */
static void createmenu(FILE* f, winptr win, metptr* root, pa_menuptr m)

{

    metptr mp, mp2;  /* pointer to menu tracking entry */

    while (m) { /* add menu item */

        if (m->branch) { /* handle submenu */

            mettrk(f, win, root, m, &mp); /* enter that into tracking */
            createmenu(f, win, &mp->branch, m->branch); /* create submenu */
            mettrk(f, win, &mp->frame, NULL, &mp2); /* create frame for submenu */
            mp2->frm = TRUE; /* set is frame */

        } else { /* handle terminal menu */

            mettrk(f, win, root, m, &mp); /* enter that into tracking */

        }
        m = m->next; /* next menu entry */

    }

}

void pa_menu(FILE* f, pa_menuptr m)

{

    winptr win; /* pointer to windows context */
    metptr mp;  /* pointer to menu tracking entry */
    XEvent e;   /* XWindow event */
    int wx, wy; /* window sizes */

    win = txt2win(f); /* get window context */
    if (win->metlst) { /* distroy previous menu */

        /* dispose of menu tracking entries */
        putmet(win->menu); /* release menu bar */
        while (win->metlst) {

            mp = win->metlst; /* remove top entry */
            win->metlst = win->metlst->next; /* gap out */
            putmet(mp); /* free the entry */

        }

    }
    if (m) { /* there is a new menu to activate */

        if (!win->menu) {

            /* no previous menu active, resize the master window and activate the menu
               bar */
            pa_winclientg(f, win->gmaxxg, win->gmaxyg+win->menuspcy, &wx, &wy,
                          BIT(pa_wmframe)*win->frame|BIT(pa_wmsize)*win->size|
                          BIT(pa_wmsysbar)*win->sysbar);
            pa_setsizg(f, wx, wy);
            /* move subclient window down past menu bar */
            XWLOCK();
            XMoveWindow(padisplay, win->xwhan, 0, win->menuspcy);
            XWUNLOCK();

            /* wait for the configure response */
            do { peekxevt(&e); /* peek next event */
            } while (e.type != ConfigureNotify || e.xconfigure.x != 0 ||
                     e.xconfigure.y != win->menuspcy ||
                     e.xany.window != win->xwhan);
            restore(win);

        }
        /* get an entry for the menu bar */
        win->menu = getmet();
        win->menu->next = NULL; /* clear next */
        win->menu->branch = win->menu; /* set not to generate event */
        win->menu->menubar = TRUE; /* flag is menu bar */
        win->menu->frm = FALSE; /* not frame */
        win->menu->bar = FALSE; /* no bar (underline) */
        win->menu->prime = FALSE; /* not prime */
        win->menu->title = NULL; /* set no face string */
        win->menu->pressed = FALSE; /* set not pressed */
        /* make internal copy of menu */
        createmenu(f, win, &win->metlst, m);
        /* activate top level menu */
        actmenu(f);

    }

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

Create standard menu

Creates a standard menu set. Given a set of standard items selected in a set,
and a program added menu list, creates a new standard menu.

On this windows version, the standard lists are:

file edit <program> window help

That is, all of the standard items are sorted into the lists at the start and
end of the menu, then the program selections placed in the menu.

*******************************************************************************/

/* get menu entry */
static void getmenu(pa_menuptr* m, int id, char* face)

{

    *m = imalloc(sizeof(pa_menurec));
    (*m)->next   = NULL; /* no next */
    (*m)->branch = NULL; /* no branch */
    (*m)->onoff  = FALSE; /* not an on/off value */
    (*m)->oneof  = FALSE; /* not a "one of" value */
    (*m)->bar    = FALSE; /* no bar under */
    (*m)->id     = id;    /* no id */
    (*m)->face = str(face); /* place face string */

}

/* add standard list item */
static void additem(pa_stdmenusel sms, int i, pa_menuptr* m, pa_menuptr* l,
                    char* s, int b)

{

    if (BIT(i) & sms) { /* this item is active */

        getmenu(m, i, s); /* get menu item */
        appendmenu(l, *m); /* add to list */
        (*m)->bar = b; /* set bar status */

    }

}

void pa_stdmenu(pa_stdmenusel sms, pa_menuptr* sm, pa_menuptr pm)

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

   /* check and perform "help" menu */

   if (sms & (BIT(PA_SMHELPTOPIC) | BIT(PA_SMABOUT))) { /* file menu */

        getmenu(&hm, 0, "Help"); /* get entry */
        appendmenu(sm, hm);

        additem(sms, PA_SMHELPTOPIC, &m, &hm->branch, "Help Topics", TRUE);
        additem(sms, PA_SMABOUT, &m, &hm->branch, "About", FALSE);

    }

}

/** ****************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

void pa_front(FILE* f)

{

    winptr win; /* pointer to windows context */

    win = txt2win(f); /* get window context */
    XWLOCK();
    XRaiseWindow(padisplay, win->xwhan);
    XWUNLOCK();

}

/** ****************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

void pa_back(FILE* f)

{

    winptr win; /* pointer to windows context */

    win = txt2win(f); /* get window context */
    XWLOCK();
    XLowerWindow(padisplay, win->xwhan);
    XWUNLOCK();

}

/** ****************************************************************************

Get window size graphical

Gets the onscreen window size.

*******************************************************************************/

void pa_getsizg(FILE* f, int* x, int* y)

{

    XWindowAttributes xwa; /* XWindow attributes */
    winptr            win; /* pointer to windows context */
    Window            cw, pw, rw;
    Window*           cwl;
    int               ncw;

    win = txt2win(f); /* get window context */
    XWLOCK();
    /* find parent */
    XQueryTree(padisplay, win->xwhan, &rw, &pw, &cwl, &ncw);
    /* get parent parameters */
    XGetWindowAttributes(padisplay, pw, &xwa);
    XWUNLOCK();
    *x = xwa.width;
    *y = xwa.height;

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

    winptr win; /* pointer to windows context */
    winptr par; /* pointer to parent windows context */
    int    gx, gy;

    win = txt2win(f); /* get window context */
    pa_getsizg(f, &gx, &gy); /* get graphics size */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        *x = gx/par->charspace;
        *y = gy/par->linespace;

    } else {

        /* find character based sizes */
        *x = gx/STDCHRX;
        *y = gy/STDCHRY;

    }

}

/** ****************************************************************************

Set window size graphical

Sets the onscreen window to the given size.

Note: XWindow, or at least the window manager, apparently accepts a width of 1
and height of 1 as the minimum size window.

*******************************************************************************/

void pa_setsizg(FILE* f, int x, int y)

{

    winptr win; /* pointer to windows context */
    XWindowChanges xwc; /* XWindow values */
    XEvent e; /* Xwindow event */

    win = txt2win(f); /* get window context */
    /* Check repeated sizing. This prevents hangups due to the window manager
       ignoring such sets. */
    if (x != win->xmwr.w || y != win->xmwr.h) {

        xwc.width = x; /* set frameless offset to client */
        xwc.height = y;
        if (win->frame) { /* if frame is enabled, calculate offset to client */

            /* change to client terms with zero clip */
            if (x >= win->pfw) xwc.width = x-win->pfw; else xwc.width = 1;
            if (y >= win->pfh) xwc.height = y-win->pfh; else xwc.height = 1;

        }

        /* reconfigure window */
        XWLOCK();
        XConfigureWindow(padisplay, win->xmwhan, CWWidth|CWHeight, &xwc);
        XWUNLOCK();

        /* set new size */
        win->xmwr.w = x;
        win->xmwr.h = y;

        /* wait for the configure response with correct sizes */
        do { peekxevt(&e); /* peek next event */
        } while (e.type != ConfigureNotify || e.xconfigure.width != xwc.width ||
                 e.xconfigure.height != xwc.height || e.xany.window != win->xmwhan);

        /* because this event may not reach pa_event() for some time, we have to
           set the dimensions now */
        if (!win->bufmod) {

            /* reset tracking sizes */
            win->gmaxxg = e.xconfigure.width; /* graphics x */
            win->gmaxyg = e.xconfigure.height; /* graphics y */
            /* find character size x */
            win->gmaxx = win->gmaxxg/win->charspace;
            /* find character size y */
            win->gmaxy = win->gmaxyg/win->linespace;

        }

    }

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


    winptr win, par; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        x = x*par->charspace;
        y = y*par->linespace;

    } else {

        /* find character based sizes */
        x = x*STDCHRX;
        y = y*STDCHRY;

    }
    pa_setsizg(f, x, y); /* execute */

}

/** ****************************************************************************

Set window position graphical

Sets the onscreen window to the given position in its parent.

*******************************************************************************/

void pa_setposg(FILE* f, int x, int y)

{

    winptr win;         /* pointer to windows context */
    XWindowChanges xwc; /* XWindow values */
    XEvent         e;   /* XWindow event */

    win = txt2win(f); /* get window context */

    /* don't repeat positions, it will cause a no-op in windows manager */
    if (x-1 != win->xmwr.x || y-1 != win->xmwr.y) {

        /* reconfigure window */
        XWLOCK();
        XMoveWindow(padisplay, win->xmwhan, x-1, y-1);
        XWUNLOCK();

        /* wait for the configure response */
        do { peekxevt(&e); /* peek next event */
        } while (e.type != ConfigureNotify || e.xany.window != win->xmwhan);

        /* set origin for next time */
        win->xmwr.x = x-1;
        win->xmwr.y = y-1;

    }

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

    winptr win, par; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based position */
        x = (x-1)*par->charspace+1;
        y = (y-1)*par->linespace+1;

    } else {

        /* find character based position */
        x = (x-1)*STDCHRX+1;
        y = (y-1)*STDCHRY+1;

    }
    pa_setposg(f, x, y); /* execute */

}

/** ****************************************************************************

Get screen size graphical

Gets the total screen size.

*******************************************************************************/

void pa_scnsizg(FILE* f, int* x, int* y)

{

    XWindowAttributes xwa; /* XWindow attributes */
    winptr            win; /* pointer to windows context */
    Window            cw, pw, rw;
    Window*           cwl;
    int               ncw;

    win = txt2win(f); /* get window context */
    XWLOCK();
    /* find parent */
    XQueryTree(padisplay, win->xwhan, &rw, &pw, &cwl, &ncw);
    /* get root parameters */
    XGetWindowAttributes(padisplay, rw, &xwa);
    XWUNLOCK();
    *x = xwa.width;
    *y = xwa.height;

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

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    pa_scnsizg(f, x, y); /* execute */
    *x = *x/STDCHRX; /* convert to "standard character" size */
    *y = *y/STDCHRY;

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

void pa_winclientg(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (BIT(pa_wmframe) & ms) { /* if the frame is on */

        *wx = cx+win->pfw; /* add width to frame */
        *wy = cy+win->pfh; /* add height to frame */

    } else { /* no frame, thus no edge */

        *wx = cx;
        *wy = cy;
    }

}

void pa_winclient(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms)

{

    winptr win, par; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* execute */
    pa_winclientg(f, cx*win->charspace, cy*win->linespace, wx, wy, ms);
    /* find character based sizes */
    if (win->parlfn >= 0) { /* has a parent */

        par = lfn2win(win->parlfn); /* index the parent */
        /* find character based sizes */
        *wx = (*wx-1) / par->charspace+1;
        *wy = (*wy-1) / par->linespace+1;

    } else {

        /* find character based sizes */
        *wx = (*wx-1) / STDCHRX+1;
        *wy = (*wy-1) / STDCHRY+1;

    }

}

/** ****************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

/* these were defined in motif, which no longer exists */
typedef struct
{
    unsigned long       flags;
    unsigned long       functions;
    unsigned long       decorations;
    long                inputmode;
    unsigned long       status;

} mwmhints;

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

void pa_frame(FILE* f, int e)

{

    winptr win; /* pointer to windows context */
    Atom mwmHintsProperty;
    mwmhints hints;

    win = txt2win(f); /* get window context */
    win->frame = !!e; /* set new status of frame */
    XWLOCK();
    mwmHintsProperty = XInternAtom(padisplay, "_MOTIF_WM_HINTS", 0);
    hints.flags = MWM_HINTS_DECORATIONS;
    if (e) hints.decorations = MWM_DECOR_ALL;
    else hints.decorations = 0; /* everything off */
    XChangeProperty(padisplay, win->xmwhan, mwmHintsProperty, mwmHintsProperty,
                    32, PropModeReplace, (unsigned char *)&hints, 5);
    XWUNLOCK();

}

/** ****************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

On GNOME/Ubuntu 20.04 with GDM3 window manager, we are not capable of turning
off the size bars alone, so this is a no-op. It may work on other window
managers.

*******************************************************************************/

void pa_sizable(FILE* f, int e)

{

    winptr win; /* pointer to windows context */
    Atom mwmHintsProperty;
    mwmhints hints;

    win = txt2win(f); /* get window context */
    win->size = !!e; /* set new status of size bars */
    XWLOCK();
    mwmHintsProperty = XInternAtom(padisplay, "_MOTIF_WM_HINTS", 0);
    hints.flags = MWM_HINTS_DECORATIONS;
    if (e) hints.decorations = MWM_DECOR_ALL;
    else hints.decorations = MWM_DECOR_TITLE|MWM_DECOR_MENU|MWM_DECOR_MINIMIZE|
                             MWM_DECOR_MAXIMIZE;
    XChangeProperty(padisplay, win->xmwhan, mwmHintsProperty, mwmHintsProperty,
                    32, PropModeReplace, (unsigned char *)&hints, 5);
    XWUNLOCK();

}

/** ****************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

I don't think XWindow can do this separately. Instead, the frame() function is
used to create component windows.

*******************************************************************************/

void pa_sysbar(FILE* f, int e)

{

    winptr win; /* pointer to windows context */
    Atom mwmHintsProperty;
    mwmhints hints;

    win = txt2win(f); /* get window context */
    win->sysbar = !!e; /* set new status of system bar */
    XWLOCK();
    mwmHintsProperty = XInternAtom(padisplay, "_MOTIF_WM_HINTS", 0);
    hints.flags = MWM_HINTS_DECORATIONS;
    if (e) hints.decorations = MWM_DECOR_ALL;
    else hints.decorations = MWM_DECOR_BORDER;
    XChangeProperty(padisplay, win->xmwhan, mwmHintsProperty, mwmHintsProperty,
                    32, PropModeReplace, (unsigned char *)&hints, 5);
    XWUNLOCK();

}

/** ****************************************************************************

Gralib startup

*******************************************************************************/

static void pa_init_graphics (int argc, char *argv[]) __attribute__((constructor (102)));
static void pa_init_graphics(int argc, char *argv[])

{

    int       ofn;  /* standard output file number */
    int       ifn;  /* standard input file number */
    winptr    win;  /* windows record pointer */
    int       dfid; /* XWindow display FID */
    int       f;    /* window creation flags */
    pa_valptr config_root; /* root for config block */
    pa_valptr term_root; /* root for terminal block */
    pa_valptr graph_root; /* root for graphics block */
    pa_valptr diag_root; /* root for diagnostics block */
    pa_valptr xwin_root; /* root for xwindow */
    pa_valptr vp;
    char*     errstr;
    int       fi;
    pa_evtcod e;

    /* clear malloc in use total */
    memusd = 0; /* total memory in use *
    memrty = 0; /* number of retries */
    maxrty = 0; /* retry maximum */
    fontcnt = 0; /* font entry counter */
    fonttot = 0; /* font entry total */
    filcnt = 0; /* file entry counter */
    filtot = 0; /* file entry total */
    piccnt = 0; /* picture entry counter */
    pictot = 0; /* picture entry total */
    scncnt = 0; /* screen struct counter */
    scntot = 0; /* screen struct total */
    wincnt = 0; /* windows structure counter */
    wintot = 0; /* windows structure total */
    imgcnt = 0; /* image frame counter */
    imgtot = 0; /* image frame total */
    metcnt = 0; /* menu entries counter */
    mettot = 0; /* menu entries total space */

    /* initialize the XWindow lock */
    pthread_mutex_init(&xwlock, NULL);

    /* turn off I/O buffering */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_lseek(ilseek, &ofplseek);
#ifdef NOCANCEL
    ovr_read_nocancel(iread_nocancel, &ofpread_nocancel);
    ovr_write_nocancel(iwrite_nocancel, &ofpwrite_nocancel);
    ovr_open_nocancel(iopen_nocancel, &ofpopen_nocancel);
    ovr_close_nocancel(iclose_nocancel, &ofpclose_nocancel);
#endif

    /* set internal configurable settings */
    maxxd     = MAXXD;     /* set default window dimensions */
    maxyd     = MAXYD;
    dialogerr = DIALOGERR; /* send runtime errors to dialog */
    mouseenb  = MOUSEENB;  /* enable mouse */
    joyenb    = JOYENB;    /* enable joystick */
    dmpmsg    = DMPMSG;    /* dump XWindow messages */
    dmpevt    = DMPEVT;    /* dump Petit-Ami messages */
    prtftm    = PRTFTM;    /* print font metrics on load */

    /* set state of shift, control and alt keys */
    ctrll = FALSE;
    ctrlr = FALSE;
    shiftl = FALSE;
    shiftr = FALSE;
    altl = FALSE;
    altr = FALSE;
    capslock = FALSE;

    esck = FALSE; /* set no previous escape */

    /* set "configuration" XWindow font capabilities */
    cfgcap = BIT(xcmedium) | BIT(xcbold) | BIT(xcdemibold) | BIT(xcdark) |
             BIT(xclight) | BIT(xcital) | BIT(xcoblique) | BIT(xcrital) |
             BIT(xcroblique) | BIT(xcnarrow) | BIT(xccondensed) |
             BIT(xcsemicondensed) |
             BIT(xcexpanded);

    /* set internal states */
    fend = FALSE; /* set no end of program ordered */
    fautohold = TRUE; /* set automatically hold self terminators */

    fntlst = NULL; /* clear font list */
    fntcnt = 0;
    frepic = NULL; /* clear free pictures list */
    freque = NULL; /* clear free x input queues */
    evtque = NULL; /* clear x event input queue */
    paqfre = NULL; /* clear pa event free queue */
    paqevt = NULL; /* clear pa event input queue */
    fremet = NULL; /* clear free menu tracking entries */
    winfre = NULL; /* clear free windows structure list */

    /* clear open files tables */
    for (fi = 0; fi < MAXFIL; fi++) {

        opnfil[fi] = NULL; /* set unoccupied */
        /* clear file to window logical number translator table */
        filwin[fi] = -1; /* set unoccupied */

    }

    /* clear window equivalence table */
    for (fi = 0; fi < MAXFIL*2+1; fi++) {

        /* clear window logical number translator table */
        xltwin[fi] = -1; /* set unoccupied */
        xltmnu[fi] = NULL; /* set no menu entry */

    }

    /* clear event vector table */
    evtshan = defaultevent;
    for (e = pa_etchar; e <= pa_ettabbar; e++) evthan[e] = defaultevent;

    /* get setup configuration */
    config_root = NULL;
    pa_config(&config_root);

    /* find "terminal" block */
    term_root = pa_schlst("terminal", config_root);
    if (term_root && term_root->sublist) term_root = term_root->sublist;

    /* find x an y max if they exist */
    vp = pa_schlst("maxxd", term_root);
    if (vp) maxxd = strtol(vp->value, &errstr, 10);
    if (*errstr) error(ecfgval);
    vp = pa_schlst("maxyd", term_root);
    if (vp) maxyd = strtol(vp->value, &errstr, 10);
    if (*errstr) error(ecfgval);
    vp = pa_schlst("joystick", term_root);
    if (vp) joyenb = strtol(vp->value, &errstr, 10);
    vp = pa_schlst("mouse", term_root);
    if (vp) mouseenb = strtol(vp->value, &errstr, 10);
    vp = pa_schlst("dump_event", term_root);
    if (vp) dmpevt = strtol(vp->value, &errstr, 10);

    /* find graph block */
    graph_root = pa_schlst("graphics", config_root);
    if (graph_root) {

        vp = pa_schlst("dialogerr", graph_root->sublist);
        if (vp) dialogerr = strtol(vp->value, &errstr, 10);
        if (*errstr) error(ecfgval);

        /* find windows subsection */
        xwin_root = pa_schlst("xwindow", graph_root->sublist);
        if (xwin_root) {

            /* find diagnostic subsection */
            diag_root = pa_schlst("diagnostics", xwin_root->sublist);
            if (diag_root) {

                vp = pa_schlst("dump_messages", diag_root->sublist);
                if (vp) dmpmsg = strtol(vp->value, &errstr, 10);
                if (*errstr) error(ecfgval);

                vp = pa_schlst("print_font_metrics", diag_root->sublist);
                if (vp) prtftm = strtol(vp->value, &errstr, 10);
                if (*errstr) error(ecfgval);

            }

        }

    }

    /* find existing display */
    XWLOCK();
    padisplay = XOpenDisplay(NULL);
    XWUNLOCK();
    if (padisplay == NULL) {

        fprintf(stderr, "Cannot open display\n");
        exit(1);

    }
    XWLOCK();
    pascreen = DefaultScreen(padisplay);
    XWUNLOCK();

    /* load the XWindow font set */
    getfonts();

    /* open stdin and stdout as I/O window set */
    ifn = fileno(stdin); /* get logical id stdin */
    ofn = fileno(stdout); /* get logical id stdout */
    openio(stdin, stdout, ifn, ofn, -1, 1); /* process open */

    /* clear input select set */
    FD_ZERO(&ifdseta);

    /* select XWindow display file */
    XWLOCK();
    dfid = ConnectionNumber(padisplay);
    XWUNLOCK();
    FD_SET(dfid, &ifdseta);
    ifdmax = dfid+1; /* set maximum fid for select() */

    /* open joystick if available */
    numjoy = 0; /* set no joysticks */
    if (joyenb) { /* if joystick is to be enabled */

        joyfid = open("/dev/input/js0", O_RDONLY);
        if (joyfid >= 0) { /* found */

            numjoy++; /* set joystick active */
            FD_SET(joyfid, &ifdseta);
            if (joyfid+1 > ifdmax)
                ifdmax = joyfid+1; /* set maximum fid for select() */

        }

    }

    /* create framing timer */
    frmfid = timerfd_create(CLOCK_REALTIME, 0);
    if (frmfid == -1) error(etimacc);
    FD_SET(frmfid, &ifdseta);
    if (frmfid+1 > ifdmax) ifdmax = frmfid+1; /* set maximum fid for select() */

    /* clear the signaling set */
    FD_ZERO(&ifdsets);

    /* clear joystick axis saves */
    joyax = 0;
    joyay = 0;
    joyaz = 0;

    /* override the event handler for menus */
    pa_eventsover(menu_event, &menu_event_oeh);

}

/** ****************************************************************************

Gralib shutdown

*******************************************************************************/

static void pa_deinit_graphics (void) __attribute__((destructor (102)));
static void pa_deinit_graphics()

{

    /* holding copies of system vectors */
    pread_t cppread;
    pread_t cppread_nocancel;
    pwrite_t cppwrite;
    pwrite_t cppwrite_nocancel;
    popen_t cppopen;
    popen_t cppopen_nocancel;
    pclose_t cppclose;
    pclose_t cppclose_nocancel;
    plseek_t cpplseek;

    winptr win;    /* windows record pointer */
    string trmnam; /* termination name */
    char   fini[] = "Finished - ";

    pa_evtrec er;

    win = lfn2win(fileno(stdout)); /* get window from fid */

	/* if the program tries to exit when the user has not ordered an exit, it
	   is assumed to be a windows "unaware" program. We stop before we exit
	   these, so that their content may be viewed */
	if (!fend && fautohold) { /* process automatic exit sequence */

        if (!win->visible) winvis(win); /* make sure we are displayed */
        /* construct final name for window */
        trmnam = imalloc(strlen(fini)+strlen(program_invocation_short_name)+1);
        strcpy(trmnam, fini); /* place first part */
        strcat(trmnam, program_invocation_short_name); /* place program name */
        /* set window title */
        XStoreName(padisplay, win->xmwhan, trmnam);
        /* wait for a formal end */
		while (!fend) pa_event(stdin, &er);
		ifree(trmnam); /* free up termination name */

	}
	XWLOCK();
	/* destroy the main window */
	XDestroyWindow(padisplay, win->xwhan);
    /* close X Window */
    XCloseDisplay(padisplay);
    XWUNLOCK();

    /* close joystick */
    close(joyfid);

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_lseek(ofplseek, &cpplseek);
#ifdef NOCANCEL
    ovr_read_nocancel(ofpread_nocancel, &cppread_nocancel);
    ovr_write_nocancel(ofpwrite_nocancel, &cppwrite_nocancel);
    ovr_open_nocancel(ofpopen_nocancel, &cppopen_nocancel);
    ovr_close_nocancel(ofpclose_nocancel, &cppclose_nocancel);
#endif

    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose || cpplseek != ilseek)
        error(esystem);

    /* release the XWindow call lock */
    pthread_mutex_destroy(&xwlock);

#ifdef PRTMEM
    fprintf(stderr, "Total memory used: %lu Total retries on malloc(): %lu\n",
            memusd, memrty);
    fprintf(stderr, "Maximum retry: %lu\n", maxrty);
    fprintf(stderr, "Font entry counter:    %lu\n", fontcnt);
    fprintf(stderr, "Font entry total:      %lu\n", fonttot);
    fprintf(stderr, "File entry counter:    %lu\n", filcnt);
    fprintf(stderr, "File entry total:      %lu\n", filtot);
    fprintf(stderr, "Picture entry counter: %lu\n", piccnt);
    fprintf(stderr, "Picture entry total:   %lu\n", pictot);
    fprintf(stderr, "Screen entry counter:  %lu\n", scncnt);
    fprintf(stderr, "Screen entry total:    %lu\n", scntot);
    fprintf(stderr, "Window entry counter:  %lu\n", wincnt);
    fprintf(stderr, "Window entry total:    %lu\n", wintot);
    fprintf(stderr, "Image frame counter:   %lu\n", imgcnt);
    fprintf(stderr, "Image frame total:     %lu\n", imgtot);

#endif

}
