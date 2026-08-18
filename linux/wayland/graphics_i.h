/** ****************************************************************************
*                                                                              *
*               WAYLAND GRAPHICS BACKEND PRIVATE INTERFACE                     *
*                                                                              *
* The seam between graphics.c, which carries the window and menu mechanism,    *
* and decorations.c, which draws the desktop's look: window frames and the     *
* onscreen menus. There is one decorations.c per desktop (gnome/, plasma/),    *
* each holding only what that desktop does differently. Both link into every   *
* program and the one whose desktop is running registers itself at load.       *
*                                                                              *
* This header carries the internal types the decorations see, the few          *
* graphics.c services they draw through, and the vector of entry points a      *
* decorations module registers.                                                *
*                                                                              *
* Private to linux/wayland: nothing outside the backend includes it.           *
*                                                                              *
*******************************************************************************/

#ifndef GRAPHICS_I_H
#define GRAPHICS_I_H

#include <stdio.h>
#include <sys/types.h>

#include <localdefs.h>
#include <graphics.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "pdisplay.h"

/* table limits shared across the backend */
#define MAXCON 10  /* number of screen contexts */
#define MAXTAB 50  /* total number of tabs possible per screen */
#define MAXPIC 50  /* total number of loadable pictures */
#define MAXLIN 250 /* maximum length of input bufferred line */
#define MAXFIL 1000 /* maximum open files */

/* ===========================================================================

   Backend internal types

   The decorations draw on windows and menu entries, so they see the two
   tracking structures and what those are built from.

   =========================================================================== */

typedef enum { mdnorm, mdinvis, mdxor, mdand, mdor } mode; /* color mix modes */

typedef struct xcaplst {

    struct xcaplst* next;  /* next entry */
    int             caps;  /* font capabilities set */
    char*           path;  /* font file path for this variant */
    int             index; /* face index in font collection file */

} xcaplst;

/* font description entry */
typedef struct fontrec {

    char*           fn;     /* name of font */
    int             fix;    /* fixed pitch font flag */
    int             caps;   /* set of XWindow font capabilities */
    xcaplst*        caplst; /* list of all XWindow font capabilities */
    struct fontrec* next;   /* next font in list */

} fontrec, *fontptr;

/* Menu tracking. This is a mirror image of the menu we were given by the
   user. However, we can do with less information than is in the original
   tree as passed. The menu items are a linear list, since they contain
   both the menu handle and the relative number 0-n of the item, neither
   the lack of tree structure nor the order of the list matters. */
typedef struct metrec* metptr;
typedef struct metrec {

    metptr next;               /* next entry */
    metptr branch;             /* menu branch */
    metptr frame;              /* frame for pulldown menu */
    metptr head;               /* head of menu pointer */
    int    menubar;            /* is the menu bar */
    int    frm;                /* is a frame */
    long   onoff;              /* the item is on-off highlighted */
    long   select;             /* the current on/off state of the highlight */
    metptr oneof;              /* "one of" chain pointer */
    metptr chnhd;              /* head of "one of" chain */
    long   ena;                /* enabled/disabled */
    long   bar;                /* has bar under */
    long   id;                 /* user id of item */
    int    fx1, fy1, fx2, fy2; /* subclient position of window */
    int    prime;              /* is a prime (onscreen) entry */
    int    pressed;            /* in the pressed state */
    FILE*  wf;                 /* output file for the menu window */
    char*  title;              /* title text */
    FILE*  parent;             /* parent window */
    FILE*  evtfil;             /* file to post menu events to */
    long   wid;                /* menu window id */

} metrec;

typedef struct scncon* scnptr;
typedef struct scncon { /* screen context */

    /* fields used by graph module */
    long    lwidth;      /* width of lines */
    ami_lstyle lstyle;   /* style of lines */
    /* note that the pixel and character dimensions and positions are kept
      in parallel for both characters and pixels */
    long    maxx;        /* maximum characters in x */
    long    maxy;        /* maximum characters in y */
    long    maxxg;       /* maximum pixels in x */
    long    maxyg;       /* maximum pixels in y */
    long    curx;        /* current cursor location x */
    long    cury;        /* current cursor location y */
    long    curxg;       /* current cursor location in pixels x */
    long    curyg;       /* current cursor location in pixels y */
    long    angle;       /* character drawing angle */
    long    fcrgb;       /* current writing foreground color in rgb */
    long    bcrgb;       /* current writing background color in rgb */
    mode    fmod;        /* foreground mix mode */
    mode    bmod;        /* background mix mode */
    fontptr cfont;       /* active font entry */
    long    cspc;        /* character spacing */
    long    lspc;        /* line spacing */
    int     attr;        /* set of active attributes */
    long    autof;       /* current status of scroll and wrap */
    long    tab[MAXTAB]; /* tabbing array */
    long    curv;        /* cursor visible */
    /* note that view offsets and scaling are experimental features */
    long    offx;        /* viewport offset x */
    long    offy;        /* viewport offset y */
    long    wextx;       /* window extent x */
    long    wexty;       /* window extent y */
    long    vextx;       /* viewpor extent x */
    long    vexty;       /* viewport extent y */

    /* fields used by graphics subsystem */
    pd_draw*    xcxt;    /* graphics context */
    pd_canvas*  xbuf;    /* pixmap for screen backing buffer */

} scncon;

typedef struct pict* picptr;
typedef struct pict { /* picture tracking record */

    struct pict* next; /* list of rescaled images */
    int          sx; /* size in x */
    int          sy; /* size in y */
    pd_canvas*   xi; /* pixel content */

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
    winptr       parwin;            /* link to parent (or NULL for parentless) */
    long         wid;               /* this window logical id */
    winptr       childwin;          /* list of child windows */
    winptr       childlst;          /* list pointer if this is a child */
    scnptr       screens[MAXCON];   /* screen contexts array */
    int          curdsp;            /* index for current display screen */
    int          curupd;            /* index for current update screen */
    /* global sets. these are the global set parameters that apply to any new
      created screen buffer */
    long         gmaxx;             /* maximum x size */
    long         gmaxy;             /* maximum y size */
    long         gmaxxg;            /* size of client area in x */
    long         gmaxyg;            /* size of client area in y */
    long         bufx;              /* buffer size x characters */
    long         bufy;              /* buffer size y characters */
    long         bufxg;             /* buffer size x pixels */
    long         bufyg;             /* buffer size y pixels */
    int          gattr;             /* current attributes */
    long         gauto;             /* state of auto */
    long         gfcrgb;            /* foreground color in rgb */
    long         gbcrgb;            /* background color in rgb */
    long         gcurv;             /* state of cursor visible */
    fontptr      gcfont;            /* current font select */
    int          gfhigh;            /* physical em-square pixel size y (FreeType) */
    int          gfhighx;           /* physical em-square pixel size x (asymmetric) */
    int          gfhigh_log;        /* logical em-square pixel size y (unscaled) */
    int          gfcellh;           /* target character cell height (pixels) */
    float        gfpoint;           /* current font point size */
    int          mischrx;           /* missing font character x */
    int          mischry;           /* missing font character y */
    int          misoffx;           /* missing font offset x */
    int          misoffy;           /* missing font offset y */
    mode         gfmod;             /* foreground mix mode */
    mode         gbmod;             /* background mix mode */
    long         goffx;             /* viewport offset x (physical pixels) */
    long         goffy;             /* viewport offset y (physical pixels) */
    float        vsx;               /* viewport scale x (default 1.0) */
    float        vsy;               /* viewport scale y (default 1.0) */
    long         gwextx;            /* window extent x */
    long         gwexty;            /* window extent y */
    long         gvextx;            /* viewpor extent x */
    long         gvexty;            /* viewport extent y */
    int          termfnt;           /* terminal font number */
    int          bookfnt;           /* book font number */
    int          signfnt;           /* sign font number */
    int          techfnt;           /* technical font number */
    int          mb1;               /* button 1 asserted (delivered level) */
    int          mb2;               /* button 2 asserted (delivered level) */
    int          mb3;               /* button 3 asserted (delivered level) */
    int          mb4;               /* button 4 asserted (delivered level) */
    int          mb5;               /* button 5 asserted (delivered level) */
    long         mpx, mpy;          /* mouse current position */
    long         mpxg, mpyg;        /* mouse current position graphical */
    /* Pending button presses/releases are counted, not levelled, so a press
       and its release that both arrive before either can be delivered (e.g.
       behind pending motion) are not collapsed -- every edge is preserved and
       delivered in order (see mouseevent/mouseupdate). */
    int          nmb1;              /* pending presses button 1 */
    int          nmb2;              /* pending presses button 2 */
    int          nmb3;              /* pending presses button 3 */
    int          nmb4;              /* pending presses button 4 */
    int          nmb5;              /* pending presses button 5 */
    int          rmb1;              /* pending releases button 1 */
    int          rmb2;              /* pending releases button 2 */
    int          rmb3;              /* pending releases button 3 */
    int          rmb4;              /* pending releases button 4 */
    int          rmb5;              /* pending releases button 5 */
    long         nmpx, nmpy;        /* new mouse current position */
    long         nmpxg, nmpyg;      /* new mouse current position graphical */
    int          linespace;         /* line spacing in pixels */
    int          charspace;         /* character spacing in pixels */
    long         chrspcx;           /* extra space between characters */
    long         chrspcy;           /* extra space between lines */
    int          curspace;          /* size of cursor, in pixels */
    int          baseoff;           /* font baseline offset from top */
    int          menuspcy;          /* amount of space for menu in y (if exists) */
    int          shift;             /* state of shift key */
    int          cntrl;             /* state of control key */
    int          fcurdwn;           /* cursor on screen flag */
    int          joy1cap;           /* joystick 1 is captured */
    int          joy2cap;           /* joystick 2 is captured */
    long         joy1xs;            /* last joystick position 1x */
    long         joy1ys;            /* last joystick position 1y */
    long         joy1zs;            /* last joystick position 1z */
    long         joy2xs;            /* last joystick position 2x */
    long         joy2ys;            /* last joystick position 2y */
    long         joy2zs;            /* last joystick position 2z */
    int          shsize;            /* display screen size x in millimeters */
    int          svsize;            /* display screen size y in millimeters */
    int          shres;             /* display screen pixels in x */
    int          svres;             /* display screen pixels in y */
    int          sdpmx;             /* display screen find dots per meter x */
    int          sdpmy;             /* display screen find dots per meter y */
    char         inpbuf[MAXLIN];    /* input line buffer */
    int          inpptr;            /* input line index */
    int          frmrun;            /* framing timer is running */
    int          timers[AMI_MAXTIM]; /* timer id array */
    int          frmsev;            /* frame timer system event */
    long         framecbms;         /* last callback-driven frame event, ms */
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
    /* window state, 0 = normal, 1 = maximized, 2 = minimized */
    int          winstate;
    int          lwinstate;         /* last window state */

    /* fields used by graphics subsystem */
    pd_win*      xmwhan;            /* master window */
    pd_win*      xwhan;             /* subclient window */
    xrect        xmwr;              /* master window rectangle */
    xrect        xwr;               /* subclient window rectangle */
    FT_Face      ftface;            /* current FreeType font face */
    int          pfw;               /* parent/frame width (extra) */
    int          pfh;               /* parent frame height (extra) */
    int          cwox;              /* client window offset from parent
                                       origin x */
    int          cwoy;              /* client window offset from parent
                                       origin y */
    int          childfrm;          /* TRUE if using Ami-drawn child frame */
    int          mstchild;          /* TRUE if this window is a child of the
                                       parent master (menu component), whose
                                       Ami frame offsets its origin */
    int          frmtbh;            /* frame title bar height, frozen at
                                       window creation */
    int          frmbsz;            /* frame button diameter, frozen */
    int          frmtsz;            /* frame title font size, frozen */
    int          frmgrab;           /* frame resize grab ring width, frozen */
    char*        wintitle;          /* window title string (for child frames) */
    pd_draw*     frmgc;             /* pd_draw* for drawing on xmwhan (child frame) */
    int          minimized;         /* TRUE if child frame is minimized */
    int          minslot;           /* slot index when minimized (for x pos) */
    xrect        savxmwr;           /* xmwr before minimize, for restore */
    long         savgmaxxg;         /* gmaxxg before minimize */
    long         savgmaxyg;         /* gmaxyg before minimize */

} winrec;

/* ===========================================================================

   graphics.c services the decorations draw through

   =========================================================================== */

extern pd_display* grx_padisplay; /* the display frames are flushed to */

/* FreeType text, used for the frame title */
void grx_ft_draw_string(pd_canvas* d, pd_draw* gc, FT_Face face,
                        int pixel_size_x, int pixel_size_y,
                        int x, int y, char* s, int len);
int  grx_ft_text_width(FT_Face face, const char* s, int len);

/* ===========================================================================

   The decorations interface

   A decorations module fills this vector and registers it from a load
   constructor, ahead of the backend's own (which runs at priority 102).
   Exactly one module registers: each tests the running desktop and steps
   aside if it is not its own.

   =========================================================================== */

/* what the pointer is over in the frame chrome */
typedef enum {

    dechnone,  /* nothing: the client area, or a resize border */
    dechtitle, /* the title bar, where a drag moves the window */
    dechclose, /* the close button */
    dechmax,   /* the maximize button */
    dechmin    /* the minimize button */

} dechit;

/* how a menu entry is being painted */
typedef enum {

    decmnorm,  /* at rest, or released */
    decmpress, /* just pressed */
    decmexpose /* repainting from an exposure */

} decmstate;

typedef struct {

    /* Frame metrics. frmmetrics freezes the chrome sizes in the window
       from its opening font (the chrome must hold still while the client
       changes fonts under it); frmgeom returns the space the chrome takes
       around the client for a given window mode set -- the whole chrome,
       the border alone with the system bar off, or nothing with the frame
       off. */
    void (*frmmetrics)(winptr win);
    void (*frmgeom)(winptr win, ami_winmodset ms,
                    int* pfw, int* pfh, int* cwox, int* cwoy);
    /* size of the title-bar-only bar a minimized child collapses to */
    void (*frmminsize)(winptr win, int* w, int* h);
    /* paint the chrome on the master window, which is mw by mh */
    void (*frmdraw)(winptr win, int mw, int mh);
    /* which resize edges the point (mx,my) is over, master mw by mh */
    void (*frmedges)(winptr win, int mx, int my, int mw, int mh,
                     int* left, int* right, int* top, int* bottom);
    /* which chrome button, if any, the point (mx,my) is over */
    dechit (*frmhit)(winptr win, int mx, int my);

    /* Color scheme. setscheme pins the palette from the config file;
       thememon starts following the desktop's own setting and returns a
       file descriptor to watch, or -1; themechg consumes what arrived on
       it and answers whether the palette changed. */
    void (*setscheme)(int dark);
    int  (*thememon)(void);
    int  (*themechg)(int fd);

    /* Menu metrics: the height of a menu entry, the extra width an entry
       needs around its text in a pulldown, the padding the pulldown frame
       adds around its entries, and the extra width a menu bar entry gets. */
    int  (*menuheight)(winptr win);
    void (*menumetrics)(winptr win, int* itemextra, int* frmpad,
                        int* barextra);
    /* paint a menu entry in the given state */
    void (*menupaint)(metptr mp, decmstate st);

} decvec;

/* a decorations module registers here */
void grx_decreg(const decvec* v);

#endif /* GRAPHICS_I_H */
