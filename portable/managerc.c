/** ****************************************************************************

\file

\brief CHARACTER MODE WINDOW MANAGER

Copyright (C) 2022 Scott A. Franco

This is a character mode window submanager for Petit-Ami. It takes a character
based surface as provided by terminal or compatible, and subdivides it into
windows.

It is portable, meaning that it only relies on the terminal level API as
defined in terminal.h. It works by overriding the base calls and giving a window
view to the client program.

The use case of managerc is to subdivide a surface like an xterm that
normally cannot present subwindows. Thus it can provide windowing to terminal
packages such as xterm or a DOS window under Windows.

There are a few important differences between this window manager and a built-in
window manager such as XWindows or Windows.

1. Since the root window is the original terminal surface, it cannot create
independent windows on the desktop, only within the parent terminal window. This
has effects on the following points.

2. When windows are maximized, no system bar or frame edges are presented. That
is, a maximized windows is the same as the original window of the terminal
surface.

3. The default I/O surface is created maximized.

4. By default, only standard ASCII characters are used to depict frame
components.

The reason for these rules is that managerc is "transparent" by default. A
non-manager aware program will run full screen, and behave as if it has the
the terminal window to itself. managerc will be entirely in the background.

                          BSD LICENSE INFORMATION

Copyright (C) 2019 - Scott A. Franco

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the project nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/* whitebook definitions */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* linux definitions */
#include <limits.h>

/* local definitions */
#include <localdefs.h>
#include <config.h>
#include <graphics.h>

/* external definitions */
#ifndef __MACH__ /* Mac OS X */
extern char *program_invocation_short_name;
#endif

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

/* select dialog/command line error */
#define USEDLG

#ifndef __MACH__ /* Mac OS X */
#define NOCANCEL /* include nocancel overrides */
#endif

/* The maximum dimensions are used to set the size of the holding arrays.
   These should be reasonable values to prevent becoming a space hog. */
#define MAXXD 250     /**< Maximum terminal size x */
#define MAXYD 250     /**< Maximum terminal size y */

#define MAXFIL 100 /* maximum open files */
#define MAXCON 10  /* number of screen contexts */
#define MAXTAB 50  /* total number of tabs possible per window */
#define MAXLIN 250 /* maximum length of input bufferred line */

/* file handle numbers at the system interface level */
#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */
#define ERRFIL 2 /* handle to standard error */

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef off_t (*plseek_t)(int, off_t, int);

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

} scnatt;

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

/* saved vectors for API entry calls */
static pa_cursor_t cursor_vect;
static pa_maxx_t maxx_vect;
static pa_maxy_t maxy_vect;
static pa_home_t home_vect;
static pa_del_t del_vect;
static pa_up_t up_vect;
static pa_down_t down_vect;
static pa_left_t left_vect;
static pa_right_t right_vect;
static pa_blink_t blink_vect;
static pa_reverse_t reverse_vect;
static pa_underline_t underline_vect;
static pa_superscript_t superscript_vect;
static pa_subscript_t subscript_vect;
static pa_italic_t italic_vect;
static pa_bold_t bold_vect;
static pa_strikeout_t strikeout_vect;
static pa_standout_t standout_vect;
static pa_fcolor_t fcolor_vect;
static pa_bcolor_t bcolor_vect;
static pa_auto_t auto_vect;
static pa_curvis_t curvis_vect;
static pa_scroll_t scroll_vect;
static pa_curx_t curx_vect;
static pa_cury_t cury_vect;
static pa_curbnd_t curbnd_vect;
static pa_select_t select_vect;
static pa_event_t event_vect;
static pa_timer_t timer_vect;
static pa_killtimer_t killtimer_vect;
static pa_mouse_t mouse_vect;
static pa_mousebutton_t mousebutton_vect;
static pa_joystick_t joystick_vect;
static pa_joybutton_t joybutton_vect;
static pa_joyaxis_t joyaxis_vect;
static pa_settab_t settab_vect;
static pa_restab_t restab_vect;
static pa_clrtab_t clrtab_vect;
static pa_funkey_t funkey_vect;
static pa_frametimer_t frametimer_vect;
static pa_autohold_t autohold_vect;
static pa_wrtstr_t wrtstr_vect;
static pa_eventover_t eventover_vect;
static pa_eventsover_t eventsover_vect;
static pa_sendevent_t sendevent_vect;
static pa_title_t title_vect;
static pa_openwin_t openwin_vect;
static pa_buffer_t buffer_vect;
static pa_sizbuf_t sizbuf_vect;
static pa_getsiz_t getsiz_vect;
static pa_setsiz_t setsiz_vect;
static pa_setpos_t setpos_vect;
static pa_scnsiz_t scnsiz_vect;
static pa_scncen_t scncen_vect;
static pa_winclient_t winclient_vect;
static pa_front_t front_vect;
static pa_back_t back_vect;
static pa_frame_t frame_vect;
static pa_sizable_t sizable_vect;
static pa_sysbar_t sysbar_vect;
static pa_menu_t menu_vect;
static pa_menuena_t menuena_vect;
static pa_menusel_t menusel_vect;
static pa_stdmenu_t stdmenu_vect;
static pa_getwinid_t getwinid_vect;
static pa_focus_t focus_vect;

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
    int    onoff;              /* the item is on-off highlighted */
    int    select;             /* the current on/off state of the highlight */
    metptr oneof;              /* "one of" chain pointer */
    metptr chnhd;              /* head of "one of" chain */
    int    ena;                /* enabled/disabled */
    int    bar;                /* has bar under */
    int    id;                 /* user id of item */
    int    fx1, fy1, fx2, fy2; /* subclient position of window */
    int    prime;              /* is a prime (onscreen) entry */
    int    pressed;            /* in the pressed state */
    FILE*  wf;                 /* output file for the menu window */
    char*  title;              /* title text */
    FILE*  parent;             /* parent window */
    FILE*  evtfil;             /* file to post menu events to */
    int    wid;                /* menu window id */

} metrec;

/** single character on screen container. note that not all the attributes
   that appear here can be changed */
typedef struct {

      /* character at location */        char ch;
      /* foreground color at location */ pa_color forec;
      /* background color at location */ pa_color backc;
      /* active attribute at location */ scnatt attr;

} scnrec;

/* window description */
typedef struct winrec* winptr;
typedef struct winrec {

    winptr   next;            /* next entry (for free list) */
    int      parlfn;          /* logical parent */
    winptr   parwin;          /* link to parent (or NULL for parentless) */
    int      wid;             /* this window logical id */
    winptr   childwin;        /* list of child windows */
    winptr   childlst;        /* list pointer if this is a child */
    winptr   winlst;          /* master list of all windows */
    winptr   rootlst;         /* master list of all roots */
    scnrec*  screens[MAXCON]; /* screen contexts array */
    int      curdsp;          /* index for current display screen */
    int      curupd;          /* index for current update screen */
    int      orgx;            /* window origin in root x */
    int      orgy;            /* window origin in root y */
    int      coffx;           /* client offset x */
    int      coffy;           /* client offset y */
    int      maxx;            /* maximum x size */
    int      maxy;            /* maximum y size */
    int      pmaxx;           /* parent maximum x */
    int      pmaxy;           /* parent maximum y */
    int      curx;            /* current cursor location x */
    int      cury;            /* current cursor location y */
    int      attr;            /* set of active attributes */
    pa_color fcolor;          /* foreground color */
    pa_color bcolor;          /* background color */
    int      curv;            /* cursor visible */
    int      autof;           /* current status of scroll and wrap */
    int      bufmod;          /* buffered screen mode */
    int      tab[MAXTAB];     /* tabbing array */
    metptr   metlst;          /* menu tracking list */
    metptr   menu;            /* "faux menu" bar */
    int      frame;           /* frame on/off */
    int      size;            /* size bars on/off */
    int      sysbar;          /* system bar on/off */
    char     inpbuf[MAXLIN];  /* input line buffer */
    int      inpptr;          /* input line index */
    int      visible;         /* window is visible */
    char*    title;           /* window title */
    int      focus;           /* window has focus */
    int      zorder;          /* Z ordering of window, 0 = bottom, N = top */

} winrec;

/* File tracking.
  Files can be passthrough to the OS, or can be associated with a window. If
  on a window, they can be output, or they can be input. In the case of
  input, the file has its own input queue, and will receive input from all
  windows that are attached to it. */
typedef struct filrec* filptr;
typedef struct filrec {

    FILE*  sfp; /* file pointer used to establish entry, or NULL */
    winptr win; /* associated window (if exists) */
    int    inl; /* this output file is linked to the input file, logical */
    int    inw; /* entry is input linked to window */

} filrec;

/* Characters for window "dressing" (frame components) */
typedef enum {

    horzlin,   /* horizontal line */
    vertlin,   /* vertical line */
    sysudl,    /* system bar underline */
    toplftcnr, /* top left corner*/
    toprgtcnr, /* top right corner */
    btmlftcnr, /* bottom left corner */
    btmrgtcnr, /* bottom right corner */
    intlft,    /* left intersection */
    intrgt,    /* right intersection */
    minbtn,    /* minimize button */
    maxbtn,    /* maximize button */
    canbtn    /* cancel button */

} framecomp;

char* frmchrs[] = {

    "-", /* horizontal line */
    "|", /* vertical line */
    "=", /* system bar underline */
    "+", /* top left corner*/
    "+", /* top right corner */
    "+", /* bottom left corner */
    "+", /* bottom right corner */
    "|", /* left intersection */
    "|", /* right intersection */
    "_", /* minimize button */
    "^", /* maximize button */
    "X", /* cancel button */

};

static filptr opnfil[MAXFIL];     /* open files table */
static int    xltwin[MAXFIL*2+1]; /* window equivalence table, includes
                                     negatives and 0 */
static int    filwin[MAXFIL];     /* file to window equivalence table */

/* colors and attributes for root window */
static int      attr;         /* set of active attributes */
static pa_color fcolor;       /* foreground color */
static pa_color bcolor;       /* background color */
static int      curx;         /* cursor x */
static int      cury;         /* cursor y */
static int      curon;        /* current on/off visible state of cursor */

static winptr   winfre;       /* free windows structure list */
static winptr   winlst;       /* master list of all windows */
static winptr   rootlst;      /* master list of all roots */
static int      ztop;         /* current maximum/front Z order */
static int      mousex;       /* mouse tracking x */
static int      mousey;       /* mouse tracking y */

/* forwards */
static void plcchr(FILE* f, char c);

/** ****************************************************************************

Process error

*******************************************************************************/

static void error(
    /** Error string */ char* es
)


{

    fprintf(stderr, "Error: Managerc: %s\n", es);
    fflush(stderr);

    exit(1);

}

/** ***************************************************************************

Print event type

A diagnostic, print the given event code as a symbol to the error file.

******************************************************************************/

static void prtevtt(
    /** Error code */ pa_evtcod e
)

{

    switch (e) {

        case pa_etchar:    fprintf(stderr, "etchar   "); break;
        case pa_etup:      fprintf(stderr, "etup     "); break;
        case pa_etdown:    fprintf(stderr, "etdown   "); break;
        case pa_etleft:    fprintf(stderr, "etleft   "); break;
        case pa_etright:   fprintf(stderr, "etright  "); break;
        case pa_etleftw:   fprintf(stderr, "etleftw  "); break;
        case pa_etrightw:  fprintf(stderr, "etrightw "); break;
        case pa_ethome:    fprintf(stderr, "ethome   "); break;
        case pa_ethomes:   fprintf(stderr, "ethomes  "); break;
        case pa_ethomel:   fprintf(stderr, "ethomel  "); break;
        case pa_etend:     fprintf(stderr, "etend    "); break;
        case pa_etends:    fprintf(stderr, "etends   "); break;
        case pa_etendl:    fprintf(stderr, "etendl   "); break;
        case pa_etscrl:    fprintf(stderr, "etscrl   "); break;
        case pa_etscrr:    fprintf(stderr, "etscrr   "); break;
        case pa_etscru:    fprintf(stderr, "etscru   "); break;
        case pa_etscrd:    fprintf(stderr, "etscrd   "); break;
        case pa_etpagd:    fprintf(stderr, "etpagd   "); break;
        case pa_etpagu:    fprintf(stderr, "etpagu   "); break;
        case pa_ettab:     fprintf(stderr, "ettab    "); break;
        case pa_etenter:   fprintf(stderr, "etenter  "); break;
        case pa_etinsert:  fprintf(stderr, "etinsert "); break;
        case pa_etinsertl: fprintf(stderr, "etinsertl"); break;
        case pa_etinsertt: fprintf(stderr, "etinsertt"); break;
        case pa_etdel:     fprintf(stderr, "etdel    "); break;
        case pa_etdell:    fprintf(stderr, "etdell   "); break;
        case pa_etdelcf:   fprintf(stderr, "etdelcf  "); break;
        case pa_etdelcb:   fprintf(stderr, "etdelcb  "); break;
        case pa_etcopy:    fprintf(stderr, "etcopy   "); break;
        case pa_etcopyl:   fprintf(stderr, "etcopyl  "); break;
        case pa_etcan:     fprintf(stderr, "etcan    "); break;
        case pa_etstop:    fprintf(stderr, "etstop   "); break;
        case pa_etcont:    fprintf(stderr, "etcont   "); break;
        case pa_etprint:   fprintf(stderr, "etprint  "); break;
        case pa_etprintb:  fprintf(stderr, "etprintb "); break;
        case pa_etprints:  fprintf(stderr, "etprints "); break;
        case pa_etfun:     fprintf(stderr, "etfun    "); break;
        case pa_etmenu:    fprintf(stderr, "etmenu   "); break;
        case pa_etmouba:   fprintf(stderr, "etmouba  "); break;
        case pa_etmoubd:   fprintf(stderr, "etmoubd  "); break;
        case pa_etmoumov:  fprintf(stderr, "etmoumov "); break;
        case pa_ettim:     fprintf(stderr, "ettim    "); break;
        case pa_etjoyba:   fprintf(stderr, "etjoyba  "); break;
        case pa_etjoybd:   fprintf(stderr, "etjoybd  "); break;
        case pa_etjoymov:  fprintf(stderr, "etjoymov "); break;
        case pa_etresize:  fprintf(stderr, "etresize "); break;
        case pa_etterm:    fprintf(stderr, "etterm   "); break;
        case pa_etmoumovg: fprintf(stderr, "etmoumovg"); break;
        case pa_etframe:   fprintf(stderr, "etframe  "); break;
        case pa_etredraw:  fprintf(stderr, "etredraw "); break;
        case pa_etmin:     fprintf(stderr, "etmin    "); break;
        case pa_etmax:     fprintf(stderr, "etmax    "); break;
        case pa_etnorm:    fprintf(stderr, "etnorm   "); break;
        case pa_etfocus:   fprintf(stderr, "etfocus  "); break;
        case pa_etnofocus: fprintf(stderr, "etnofocus"); break;
        case pa_ethover:   fprintf(stderr, "ethover  "); break;
        case pa_etnohover: fprintf(stderr, "etnohover"); break;
        case pa_etmenus:   fprintf(stderr, "etmenus  "); break;
        case pa_etbutton:  fprintf(stderr, "etbutton "); break;
        case pa_etchkbox:  fprintf(stderr, "etchkbox "); break;
        case pa_etradbut:  fprintf(stderr, "etradbut "); break;
        case pa_etsclull:  fprintf(stderr, "etsclull "); break;
        case pa_etscldrl:  fprintf(stderr, "etscldrl "); break;
        case pa_etsclulp:  fprintf(stderr, "etsclulp "); break;
        case pa_etscldrp:  fprintf(stderr, "etscldrp "); break;
        case pa_etsclpos:  fprintf(stderr, "etsclpos "); break;
        case pa_etedtbox:  fprintf(stderr, "etedtbox "); break;
        case pa_etnumbox:  fprintf(stderr, "etnumbox "); break;
        case pa_etlstbox:  fprintf(stderr, "etlstbox "); break;
        case pa_etdrpbox:  fprintf(stderr, "etdrpbox "); break;
        case pa_etdrebox:  fprintf(stderr, "etdrebox "); break;
        case pa_etsldpos:  fprintf(stderr, "etsldpos "); break;
        case pa_ettabbar:  fprintf(stderr, "ettabbar "); break;

        default: fprintf(stderr, "???");

    }

}

/** ***************************************************************************

Print Petit-Ami event diagnostic

Prints a decoded version of PA events on one line, including paraemters. Only
prints if the dump PA event flag is true. Does not terminate the line.

Note: does not output a debugging preamble. If that is required, print it
before calling this routine.

******************************************************************************/

static void prtevt(
    /** Event record */ pa_evtptr er
)

{

    fprintf(stderr, "PA Event: Window: %d ", er->winid);
    prtevtt(er->etype);
    switch (er->etype) {

        case pa_etchar: fprintf(stderr, ": char: %c", er->echar); break;
        case pa_ettim: fprintf(stderr, ": timer: %d", er->timnum); break;
        case pa_etmoumov: fprintf(stderr, ": mouse: %d x: %4d y: %4d",
                                  er->mmoun, er->moupx, er->moupy); break;
        case pa_etmouba: fprintf(stderr, ": mouse: %d button: %d",
                                 er->amoun, er->amoubn); break;
        case pa_etmoubd: fprintf(stderr, ": mouse: %d button: %d",
                                 er->dmoun, er->dmoubn); break;
        case pa_etjoyba: fprintf(stderr, ": joystick: %d button: %d",
                                 er->ajoyn, er->ajoybn); break;
        case pa_etjoybd: fprintf(stderr, ": joystick: %d button: %d",
                                 er->djoyn, er->djoybn); break;
        case pa_etjoymov: fprintf(stderr, ": joystick: %d x: %4d y: %4d z: %4d "
                                  "a4: %4d a5: %4d a6: %4d", er->mjoyn,
                                  er->joypx, er->joypy, er->joypz,
                                  er->joyp4, er->joyp5, er->joyp6); break;
        case pa_etresize: fprintf(stderr, ": x: %d y: %d xg: %d yg: %d",
                                  er->rszx, er->rszy,
                                  er->rszxg, er->rszyg); break;
        case pa_etfun: fprintf(stderr, ": key: %d", er->fkey); break;
        case pa_etmoumovg: fprintf(stderr, ": mouse: %d x: %4d y: %4d",
                                   er->mmoung, er->moupxg, er->moupyg); break;
        case pa_etredraw: fprintf(stderr, ": sx: %4d sy: %4d ex: %4d ey: %4d",
                                  er->rsx, er->rsy, er->rex, er->rey); break;
        case pa_etmenus: fprintf(stderr, ": id: %d", er->menuid); break;
        case pa_etbutton: fprintf(stderr, ": id: %d", er->butid); break;
        case pa_etchkbox: fprintf(stderr, ": id: %d", er->ckbxid); break;
        case pa_etradbut: fprintf(stderr, ": id: %d", er->radbid); break;
        case pa_etsclull: fprintf(stderr, ": id: %d", er->sclulid); break;
        case pa_etscldrl: fprintf(stderr, ": id: %d", er->scldrid); break;
        case pa_etsclulp: fprintf(stderr, ": id: %d", er->sclupid); break;
        case pa_etscldrp: fprintf(stderr, ": id: %d", er->scldpid); break;
        case pa_etsclpos: fprintf(stderr, ": id: %d position: %d",
                                  er->sclpid, er->sclpos); break;
        case pa_etedtbox: fprintf(stderr, ": id: %d", er->edtbid); break;
        case pa_etnumbox: fprintf(stderr, ": id: %d number: %d",
                                  er->numbid, er->numbsl); break;
        case pa_etlstbox: fprintf(stderr, ": id: %d select: %d",
                                  er->lstbid, er->lstbsl); break;
        case pa_etdrpbox: fprintf(stderr, ": id: %d select: %d",
                                  er->drpbid, er->drpbsl); break;
        case pa_etdrebox: fprintf(stderr, ": id: %d", er->drebid); break;
        case pa_etsldpos: fprintf(stderr, ": id: %d postion: %d",
                                  er->sldpid, er->sldpos); break;
        case pa_ettabbar: fprintf(stderr, ": id: %d select: %d",
                                  er->tabid, er->tabsel); break;
        default: ;

    }

}

/** ****************************************************************************

Get file entry

Allocates and initalizes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

static void getfil(filptr* fp)

{

    *fp = malloc(sizeof(filrec)); /* get new file entry */
    (*fp)->sfp = NULL; /* set no file pointer */
    (*fp)->win = NULL; /* set no window */
    (*fp)->inw = FALSE; /* clear input window link */
    (*fp)->inl = -1; /* set no input file linked */

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

    } else p = malloc(sizeof(winrec));

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

    if (fn < 0 || fn >= MAXFIL) error("Invalid handle");
    if (!opnfil[fn]) error("Invalid handle");
    if (!opnfil[fn]->win) error("Not a window file");

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
    if (fn < 0) error("Invalid file");

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
    if (fn < 0) error("Invalid file");

    return (fn); /* return result */

}

/** ****************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

static void clrscn(FILE* f)

{

}

/** ****************************************************************************

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

*******************************************************************************/

static void itab(FILE* f)

{

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

Set cursor cached

Sets the root cursor if it has changed.

*******************************************************************************/

static int setcursor(int x, int y)

{

    if (x != curx || y != cury) {

        (*cursor_vect)(stdout, x, y); /* set new position */
        curx = x; /* set new location */
        cury = y;

    }

}

/*******************************************************************************

Set foreground color cached

Sets the root foreground color if it has changed.

*******************************************************************************/

static int setfcolor(pa_color c)

{

    if (c != fcolor) {

        (*fcolor_vect)(stdout, c); /* set new color */
        fcolor = c; /* cache that */

    }

}

/*******************************************************************************

Set background color cached

Sets the root background color if it has changed.

*******************************************************************************/

static int setbcolor(pa_color c)

{

    if (c != bcolor) {

        (*bcolor_vect)(stdout, c); /* set new color */
        bcolor = c; /* cache that */

    }

}

/** ****************************************************************************

Set screen attribute cached

Sets the current root screen attribute according to the current set of attributes.
Note that if multiple attributes are set, but the system only allows a single
attribute, then only the standout attribute will be set, since that is the last
attribute to be activated.

The standout attribute is a series of attributes in priority order.

The attribute change is only coped to the root window if it has changed from the
root window to cut down on chatter between the modules.

*******************************************************************************/

static void setattrs(int at)

{

    if (BIT(sasuper) & at != BIT(sasuper) & attr) { /* has changed */

        (*superscript_vect)(stdout, BIT(sasuper) & at);
        attr = attr & ~BIT(sasuper) | BIT(sasuper) & at;

    }
    if (BIT(sasubs) & at != BIT(sasubs) & attr) { /* has changed */

        (*subscript_vect)(stdout, BIT(sasubs) & at);
        attr = attr & ~BIT(sasubs) | BIT(sasubs) & at;

    }
    if (BIT(sablink) & at != BIT(sablink) & attr) { /* has changed */

        (*blink_vect)(stdout, BIT(sablink) & at);
        attr = attr & ~BIT(sablink) | BIT(sablink) & at;

    }
    if (BIT(sastkout) & at != BIT(sastkout) & attr) { /* has changed */

        (*blink_vect)(stdout, BIT(sastkout) & at);
        attr = attr & ~BIT(sastkout) | BIT(sastkout) & at;

    }
    if (BIT(saital) & at != BIT(saital) & attr) { /* has changed */

        (*blink_vect)(stdout, BIT(saital) & at);
        attr = attr & ~BIT(saital) | BIT(saital) & at;

    }
    if (BIT(sabold) & at != BIT(sabold) & attr) { /* has changed */

        (*blink_vect)(stdout, BIT(sabold) & at);
        attr = attr & ~BIT(sabold) | BIT(sabold) & at;

    }
    if (BIT(saundl) & at != BIT(saundl) & attr) { /* has changed */

        (*blink_vect)(stdout, BIT(saundl) & at);
        attr = attr & ~BIT(saundl) | BIT(saundl) & at;

    }
    if (BIT(sarev) & at != BIT(sarev) & attr) { /* has changed */

        (*blink_vect)(stdout, BIT(sarev) & at);
        attr = attr & ~BIT(sarev) | BIT(sarev) & at;

    }

}

/*******************************************************************************

Set cursor on/off cached

Sets the root cursor visible if it has changed.

*******************************************************************************/

static void setcurvis(int e)

{

    if (e != curon) {

        (*curvis_vect)(stdout, e); /* set new visible state */
        curon = e; /* set cache */

    }

}

/*******************************************************************************

Output character to root window

Outputs a single character to the root window and advances the x cursor. Note
we assume auto is off and x can climb to infinity (INT_MAX).

*******************************************************************************/

static void wrtchr(char c)

{

    (*ofpwrite)(OUTFIL, &c, 1);
    curx++;

}

/*******************************************************************************

Output string to root window

Outputs a string to the root window and advances the x cursor. Note we assume
auto is off and x can climb to infinity (INT_MAX).

*******************************************************************************/

static void wrtstr(char* s)

{

    while (*s) { wrtchr(*s); s++; }

}

/*******************************************************************************

Remove all focus windows

Removes all windows that show focus. Usually used before marking a window as
having focus. Note that normally there should only be a single window with
focus.

*******************************************************************************/

void remfocus(void)

{

    winptr win; /* pointer to windows list */

    win = winlst; /* get the master list */
    while (win) { /* traverse the windows list */

        win->focus = FALSE;
        win = win->winlst; /* next window */

    }

}

/*******************************************************************************

Find focus window

Finds the window currently holding focus. Returns the window pointer, or NULL if
no window is found to have focus. This would happen if the user clicked on an
unoccupied area of the root window.

*******************************************************************************/

winptr fndfocus(void)

{

    winptr win; /* pointer to windows list */
    winptr fp;  /* found window pointer */

    win = winlst; /* get the master list */
    fp = NULL; /* set no window found */
    while (win) { /* traverse the windows list */

        /* if in focus, find and terminate */
        if (win->focus) { fp = win; win = NULL; }
        else win = win->winlst; /* next window */

    }

    return (fp);

}

/*******************************************************************************

Find Z order top window from point

Given an X-Y point in the root surface, finds the topmost window containing that
point. If there is no containing window, NULL is returned.

*******************************************************************************/

winptr fndtop(int x, int y)

{

    winptr win; /* pointer to windows list */
    winptr fp;  /* found window pointer */
    int    z;   /* z order of last found */

    win = winlst; /* get the master list */
    fp = NULL; /* set no window found */
    z = -1; /* set invalid z order */
    while (win) { /* traverse the windows list */

        if (win->orgx <= x && x <= win->orgx+win->pmaxx-1 &&
            win->orgy <= y && y <= win->orgy+win->pmaxy-1 &&
            win->zorder > z) {

            /* found inclusion, Z order above previous */
            fp = win; /* set candidate */
            z = win->zorder;

        }
        win = win->winlst; /* next window */

    }

    return (win); /* exit wth container or NULL */

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editing. The line is placed into the
input line buffer.

*******************************************************************************/

static void readline(int fd)

{

}

/** ****************************************************************************

Draw frame on window

Draws a frame around the indicated window. This is one of the following:

(ASCII)

    +------------------+
    |            _ ^ X |
    |==================|
    |                  |
    |                  |
    +------------------+

(UTF-8)
    ╔══════════════════╗
    ║           - ▢ Ⓧ ║
    ╠══════════════════╣
    ║                  ║
    ║                  ║
    ╚══════════════════╝

(is seamless in xterm, here it depends on the editor).

*******************************************************************************/

static void drwfrm(winptr win)

{

    int x, y, l;

    if (win->frame) { /* draw window frame */

        if (win->size) { /* draw size bars */

            /* draw top and bottom */
            setcursor(win->orgx, win->orgy);
            wrtstr(frmchrs[toplftcnr]);
            for (x = 2; x <= win->pmaxx-1; x++) wrtstr(frmchrs[horzlin]);
            wrtstr(frmchrs[toprgtcnr]);

            setcursor(win->orgx, win->orgy+win->pmaxy-1);
            wrtstr(frmchrs[btmlftcnr]);
            for (x = 2; x <= win->pmaxx-1; x++) wrtstr(frmchrs[horzlin]);
            wrtstr(frmchrs[btmrgtcnr]);

            /* draw sides */
            for (y = win->orgy+1; y < win->orgy+win->pmaxy-1; y++) {

                setcursor(win->orgx, y);
                wrtstr(frmchrs[vertlin]);
                setcursor(win->orgx+win->pmaxx-1, y);
                wrtstr(frmchrs[vertlin]);

            }

        }
        if (win->sysbar) { /* draw system bar */

            y = win->size; /* offset to system bar */
            x = win->pmaxx-6; /* start of system bar buttons */
            /* set draw location */
            setcursor(win->orgx+x-1, win->orgy+y);
            wrtstr(frmchrs[minbtn]);
            wrtchr(' ');
            wrtstr(frmchrs[maxbtn]);
            wrtchr(' ');
            wrtstr(frmchrs[canbtn]);
            wrtchr(' ');

            /* draw title, if exists */
            if (win->title) {

                l = strlen(win->title); /* get length */
                /* limit string length to available space */
                if (win->pmaxx-6 < l) l = win->pmaxx-6;
                setcursor(win->orgx+(win->pmaxx-6)/2-(l/2), win->orgy+y);
                wrtstr(win->title);

            }

            /* draw underbar */
            y++;
            setcursor(win->orgx+1, win->orgy+y);
            for (x = 2; x <= win->pmaxx-1; x++) wrtstr(frmchrs[sysudl]);

        }

    }

}

/** ****************************************************************************

Initalize screen

Clears all the parameters in the present screen context. Also, the backing
buffer bitmap is created and cleared to the present colors.

*******************************************************************************/

static void iniscn(winptr win, scnrec* sc)

{

    int x, y;
    scnrec* scp;   /* pointer to screen location */

    /* clear buffer */
    for (y = 1; y < MAXYD; y++)
        for (x = 1; x < MAXXD; x++) {

        /* index screen character location */
        scp = &(sc[(win->cury-1)*win->maxx+(win->curx-1)*sizeof(scnrec)]);
        /* place character to buffer */
        scp->ch = ' ';
        scp->forec = pa_black;
        scp->backc = pa_white;
        scp->attr = 0;

    }

}

/** ****************************************************************************

Restore screen

Updates all the buffer and screen parameters from the display screen to the
terminal.

*******************************************************************************/

static void restore(winptr win) /* window to restore */

{

    int x, y;
    scnrec* scp;   /* pointer to screen location */

    if (win->bufmod && win->visible)  { /* buffered mode is on, and visible */

        if (win->frame) drwfrm(win); /* draw window frame */

        /* restore window from buffer */
        for (y = 1; y <= win->maxy; y++) {

            /* Reset cursor at the start of each line. Note frame offsets. */
            setcursor(win->orgx+win->coffx, win->orgy+win->coffy+y-1);
            /* draw each line */
            for (x = 1; x <= win->maxx; x++) {

                /* index screen character location */
                scp = &(win->screens[win->curdsp-1]
                    [(win->cury-1)*win->maxx+(win->curx-1)*sizeof(scnrec)]);
                setfcolor(scp->forec); /* set colors */
                setbcolor(scp->backc);
                setattrs(scp->attr); /* set attributes */
                wrtchr(scp->ch); /* output character */

            }

        }

    }

}

/*******************************************************************************

Display window

Presents a window, and sends it a first paint message. Used to process the
delayed window display function.

*******************************************************************************/

static void winvis(winptr win)

{

   if (!win->visible) { /* not already visible */

        /* first make all parents visible */
        if (win->parwin) winvis(win->parwin);

        /* now display this one */
        win->visible = TRUE; /* set now visible */
        restore(win); /* restore window */

    }

}

/** ****************************************************************************

Open and present window

Given a windows file id and an (optional) parent window file id opens and
presents the window associated with it. All of the screen buffer data is
cleared, and a single buffer assigned to the window.

*******************************************************************************/

static void opnwin(int fn, int pfn, int wid, int subclient, int root)

{

    int     si;   /* index for current display screen */
    winptr  win;  /* window pointer */
    winptr  pwin; /* parent window pointer */
    int     t;

    win = lfn2win(fn); /* get a pointer to the window */
    /* find parent */
    win->parlfn = pfn; /* set parent logical number */
    win->wid = wid; /* set window id */
    pwin = NULL; /* set no parent */
    if (pfn >= 0) pwin = lfn2win(pfn); /* index parent window */
    win->parwin = pwin; /* copy link to windows structure */
    win->childwin = NULL; /* clear the child window list */
    win->childlst = NULL; /* clear child member list pointer */
    /* push window to master list */
    win->winlst = winlst;
    winlst = win;
    /* push window to root list */
    win->rootlst = rootlst;
    rootlst = win;
    if (pwin) { /* we have a parent, enter this child to the parent list */

        win->childlst = pwin->childwin; /* push to parent's child list */
        pwin->childwin = win;

    }
    ztop++; /* increase Z ordering */
    remfocus(); /* remove any existing focus */
    win->focus = TRUE; /* last window in gets focus */
    win->zorder = ztop; /* set Z order for this window */
    win->inpptr = -1; /* set buffer empty */
    win->inpbuf[0] = 0;
    win->bufmod = TRUE; /* set buffering on */
    win->metlst = NULL; /* clear menu tracking list */
    win->menu = NULL; /* set menu bar not active */
    if (root) { /* set up root without frame */

        win->frame = FALSE; /* set frame off */
        win->size = FALSE; /* set size bars off */
        win->sysbar = FALSE; /* set system bar off */

    } else {

        win->frame = TRUE; /* set frame on */
        win->size = TRUE; /* set size bars on */
        win->sysbar = TRUE; /* set system bar on */

    }

    /* set up global buffer parameters */
    win->pmaxx = (*maxx_vect)(stdout); /* character max dimensions */
    win->pmaxy = (*maxy_vect)(stdout);
    win->maxx = win->pmaxx; /* copy to client dimensions */
    win->maxy = win->pmaxy;
    /* subtract frame from client if enabled */
    win->maxx -= (win->frame && win->size)*2;
    win->maxy -= win->frame*2+win->size*2;
    win->attr = attr; /* no attribute */
    win->autof = TRUE; /* auto on */
    win->fcolor = pa_black; /*foreground black */
    win->bcolor = pa_white; /* background white */
    win->curv = TRUE; /* cursor visible */
    win->orgx = 1;  /* set origin to root */
    win->orgy = 1;
    /* set client offset considering framing characteristics */
    win->coffx = 0+(win->frame && win->size);
    win->coffy = 0+(win->frame && win->size)+win->sysbar*2;
    win->curx = 1; /* set cursor at home */
    win->cury = 1;
    for (t = 0; t < MAXTAB; t++) win->tab[t] = 0; /* clear tab array */

    /* clear the screen array */
    for (si = 0; si < MAXCON; si++) win->screens[si] = NULL;
    /* get the default screen */
    win->screens[0] = malloc(sizeof(scnrec)*win->maxy*win->maxx);
    win->curdsp = 1; /* set current display screen */
    win->curupd = 1; /* set current update screen */
    win->visible = FALSE; /* set not visible */
    win->title = NULL; /* set no title */
    if (root) {

#ifndef __MACH__ /* Mac OS X */
    win->title = malloc(strlen(program_invocation_short_name)+1);
    if (!win->title) error("Out of memory");
    /* set title to invoking program */
    strcpy(win->title, program_invocation_short_name);
#endif

    }

    iniscn(win, win->screens[0]); /* initalize screen buffer */
    restore(win); /* update to screen */

}

/** ****************************************************************************

Open an input and output pair

Creates, opens and initializes an input and output pair of files.

*******************************************************************************/

static void openio(FILE* infile, FILE* outfile, int ifn, int ofn, int pfn,
                   int wid, int subclient, int root)

{

    /* if output was never opened, create it now */
    if (!opnfil[ofn]) getfil(&opnfil[ofn]);
    /* if input was never opened, create it now */
    if (!opnfil[ifn]) getfil(&opnfil[ifn]);
    opnfil[ofn]->inl = ifn; /* link output to input */
    opnfil[ifn]->inw = TRUE; /* set input is window handler */
    /* set file descriptor locations */
    opnfil[ifn]->sfp = infile;
    opnfil[ofn]->sfp = outfile;
    /* now see if it has a window attached */
    if (!opnfil[ofn]->win) {

        /* Haven't already started the main input/output window, so allocate
           and start that. We tolerate multiple opens to the output file. */
        opnfil[ofn]->win = getwin();
        opnwin(ofn, pfn, wid, subclient, root); /* and start that up */

    }
    /* check if the window has been pinned to something else */
    if (xltwin[wid+MAXFIL] >= 0 && xltwin[wid+MAXFIL] != ofn)
        error("Window in use"); /* flag error */
    xltwin[wid+MAXFIL] = ofn; /* pin the window to the output file */
    filwin[ofn] = wid;

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

void intopenwin(FILE** infile, FILE** outfile, FILE* parent, int wid)

{

    int ifn, ofn, pfn; /* file logical handles */

    /* check valid window handle */
    if (!wid || wid < -MAXFIL || wid > MAXFIL) error("Invalid window ID");
    /* check if the window id is already in use */
    if (xltwin[wid+MAXFIL] >= 0) error("Window ID already in use");
    if (parent) {

        txt2win(parent); /* validate parent is a window file */
        pfn = txt2lfn(parent); /* get logical parent */

    } else pfn = -1; /* set no parent */
    ifn = fndfil(*infile); /* find previous open input side */
    if (ifn < 0) { /* no other input file, open new */

        /* open input file */
        *infile = fopen("/dev/null", "r"); /* open null as read only */
        if (!*infile) error("Can't open file"); /* can't open */
        setvbuf(*infile, NULL, _IONBF, 0); /* turn off buffering */
        ifn = fileno(*infile); /* get logical file no */

    }
    /* open output file */
    *outfile = fopen("/dev/null", "w");
    ofn = fileno(*outfile); /* get logical file no. */
    if (ofn == -1) error("System consistency error");
    if (!*outfile) error("Can't open file");
    setvbuf(*outfile, NULL, _IONBF, 0); /* turn off buffering */

    /* check either input is unused, or is already an input side of a window */
    if (opnfil[ifn]) /* entry exists */
        if (!opnfil[ifn]->inw || opnfil[ifn]->win)
            error("File in incorrect mode");
    /* check output file is in use for input or output from window */
    if (opnfil[ofn]) /* entry exists */
        if (opnfil[ofn]->inw || opnfil[ofn]->win)
            error("File in use"); /* file in use */
    /* establish all logical files and links, translation tables, and open
       window */
    openio(*infile, *outfile, ifn, ofn, pfn, wid, TRUE, FALSE);

}

/*******************************************************************************

Close window

Closes an open window pair. Accepts an output window. The window is closed, and
the window and file handles are freed. The input file is freed only if no other
window also links it.

*******************************************************************************/

static void closewin(int ofn)

{

}

/** ****************************************************************************

Find if cursor is in screen bounds internal

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

int intcurbnd(winptr win)

{

    return (win->curx >= 1 && win->curx <= win->maxx &&
            win->cury >= 1 && win->cury <= win->maxy);

}

/*******************************************************************************

Position cursor in window

Positions the cursor (caret) image to the right location on screen, and handles
the visible or invisible status of that. We consider the current position and
visible/invisible status, and try to output only the minimum terminal controls
to bring the old state of the display to the same state as the new display.

*******************************************************************************/

void setcur(winptr win)

{

    if (indisp(win)) { /* in display */

        /* check cursor in bounds and visible */
        if (intcurbnd(win) && win->curv) {

            if (!curon) { /* cursor not on */

                (*curvis_vect)(stdout, TRUE); /* set cursor on */
                curon = TRUE;

            }
            /* position actual cursor */
            curx = win->curx+win->orgx-1+win->coffx;
            cury = win->cury+win->orgy-1+win->coffy;
            (*cursor_vect)(stdout, curx, cury);

        }

    }

}

/** ****************************************************************************

Scroll screen

Scrolls the screen by deltas in any given direction. If the scroll would move
all content off the screen, the screen is simply blanked. Otherwise, we find the
section of the screen that would remain after the scroll, determine its source
and destination rectangles, and use a move.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

static void intscroll(winptr win, int x, int y)

{

}

/** ****************************************************************************

API calls implemented at this level

*******************************************************************************/

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

void icursor(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->cury = y; /* set new position */
    win->curx = x;
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int imaxx(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->maxx);

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int imaxy(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return (win->maxy);

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void ihome(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* reset cursors */
    win->curx = 1;
    win->cury = 1;
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Move cursor up

Moves the cursor position up one line. If the cursor is at screen top, and auto
is on, the screen is scrolled up, meaning that the screen contents are moved
down a line of text. If auto is off, the cursor can simply continue into
negative space as long as it stays within the bounds -INT_MAX to INT_MAX.

*******************************************************************************/

void iup(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* check not top of screen */
    if (win->cury > 1) win->cury--; /* update position */
    else if (win->autof) intscroll(win, 0, -1); /* scroll up */
    /* check won't overflow */
    else if (win->cury > -INT_MAX) win->cury--; /* set new position */
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Move cursor down

Moves the cursor position down one line. If the cursor is at screen bottom, and
auto is on, the screen is scrolled down, meaning that the screen contents are
moved up a line of text. If auto is off, the cursor can simply continue into
undrawn space as long as it stays within the bounds of -INT_MAX to INT_MAX.

*******************************************************************************/

void idown(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    /* check not bottom of screen */
    if (win->cury < win->maxy) win->cury++; /* update position */
    else if (win->autof) intscroll(win, 0, +1); /* scroll down */
    /* check won't overflow */
    else if (win->cury < INT_MAX) win->cury++; /* set new position */
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Move cursor left

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

void ileft(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* check not at extreme left */
    if (win->curx > 1) win->curx--; /* update position */
    else { /* wrap cursor motion */

        if (win->autof) { /* autowrap is on */

            iup(f); /* move cursor up one line */
            win->curx = win->maxx; /* set cursor to extreme right */

        } else
            /* check won't overflow */
            if (win->curx > -INT_MAX) win->curx--; /* update position */

    }
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

void iright(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    /* check not at extreme right */
    if (win->curx < win->maxx) win->curx++; /* update position */
    else { /* wrap cursor motion */

        if (win->autof) { /* autowrap is on */

            idown(f); /* move cursor up one line */
            win->curx = 1; /* set cursor to extreme left */

        } else
            /* check won't overflow */
            if (win->curx < INT_MAX) win->curx++; /* update position */

    }
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void idel(FILE* f)

{

    ileft(f); /* back up cursor */
    plcchr(f, ' '); /* blank out */
    ileft(f); /* back up again */

}

/** ****************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

Graphical mode does not implement blink mode.

*******************************************************************************/

void iblink(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sablink); /* turn on */
    else win->attr &= ~BIT(sablink); /* turn off */

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void ireverse(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sarev); /* turn on */
    else win->attr &= ~BIT(sarev); /* turn off */

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void iunderline(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(saundl); /* turn on */
    else win->attr &= ~BIT(saundl); /* turn off */

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and elevated font.

*******************************************************************************/

void isuperscript(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sasuper); /* turn on */
    else win->attr &= ~BIT(sasuper); /* turn off */

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and lowered font.

*******************************************************************************/

void isubscript(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sasubs); /* turn on */
    else win->attr &= ~BIT(sasubs); /* turn off */

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void iitalic(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(saital); /* turn on */
    else win->attr &= ~BIT(saital); /* turn off */

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void ibold(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sabold); /* turn on */
    else win->attr &= ~BIT(sabold); /* turn off */

}

/** ****************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

void istrikeout(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sastkout); /* turn on */
    else win->attr &= ~BIT(sastkout); /* turn off */

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void istandout(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (e) win->attr |= BIT(sarev); /* turn on */
    else win->attr &= ~BIT(sarev); /* turn off */

}

/** ****************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

*******************************************************************************/

void ifcolor(FILE* f, pa_color c)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->fcolor = c; /* set color */

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void ibcolor(FILE* f, pa_color c)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->bcolor = c; /* set color */

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

void iauto(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->autof = e; /* set auto state */

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void icurvis(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->curv = e; /* set/reset cursor visibility */
    setcur(win); /* activate cursor onscreen as required */

}

/** ****************************************************************************

Scroll screen

Scrolls the window by deltas in any given direction. If the scroll
would move all content off the screen, the screen is simply blanked. Otherwise,
we find the section of the screen that would remain after the scroll, determine
its source and destination rectangles, and use a bitblt to move it.
One speedup for the code would be to use non-overlapping fills for the x-y
fill after the bitblt.

In buffered mode, this routine works by scrolling the buffer, then restoring
it to the current window. In non-buffered mode, the scroll is applied directly
to the window.

*******************************************************************************/

void iscroll(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int icurx(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return win->curx; /* return cursor x */

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int icury(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return win->cury; /* return cursor y */

}

/** ****************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

int icurbnd(FILE* f)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */

    return intcurbnd(win); /* return cursor bound status */

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

void iselect(FILE* f, int u, int d)

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

void ievent(FILE* f, pa_evtrec* er)

{

    pa_evtrec ev;    /* local event record */
    int       valid; /* event is for this window and complete */
    winptr    win;   /* windows record pointer */

    valid = FALSE; /* set no valid event */
    (*event_vect)(stdin, &ev); /* get root event */
    while (!valid)
        switch (ev.etype) { /* process root events */

        case pa_etchar: /* input character ready */

            win = fndfocus(); /* find focus window (if any) */
            if (win) { /* found focus window */

                er->etype = pa_etchar; /* place character code */
                er->echar = ev.echar; /* place character */
                er->winid = win->wid; /* send keys to focus window */
                valid = TRUE; /* set as valid event */

            }
            break;
        case pa_etmouba:  /* mouse button assertion */
            win = fndtop(mousex, mousey); /* find the enclosing window */
            /* first click with no focus gives focus, next click gives message */
            if (win) {

                if (win->focus) {

                    er->etype = pa_etmouba; /* set mouse button asserts */
                    er->amoun = ev.amoun; /* set mouse number */
                    er->amoubn = ev.amoubn; /* set button number */
                    valid = TRUE; /* set as valid event */

                } else if (ev.mmoun == 1) { /* button 1 click */

                    remfocus(); /* remove previous focus */
                    win->focus = TRUE;

                }

            }
            break;
        case pa_etmoubd:  /* mouse button deassertion */
        case pa_etmoumov: /* mouse move */
            mousex = ev.moupx; /* set current mouse position */
            mousey = ev.moupy;
            win = fndtop(mousex, mousey); /* see if in a window */
            if (win && win->focus) { /* in window and in focus */

                /* check in client area */
                if (win->orgx+win->coffx <= mousex &&
                    mousex <= win->orgx+win->coffx+win->maxx-1 &&
                    win->orgy+win->coffy <= mousey &&
                    mousey <= win->orgy+win->coffy+win->maxy-1) {

                    er->etype = pa_etmoumov; /* set mouse move event */
                    /* calculate relative location in client area */
                    er->moupx = mousex-(win->orgx+win->coffx);
                    er->moupy = mousey-(win->orgy+win->coffy);

                }

            }
            break;
        default: ; /* ignore the rest */

    }


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

void itimer(FILE* f, int i, long t, int r)

{

}

/** ****************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

*******************************************************************************/

void ikilltimer(FILE* f, int i)

{

}

/** ****************************************************************************

Return number of mice

Returns the number of mice implemented. This is a pure passthrough function.

*******************************************************************************/

int imouse(FILE* f)

{

    return (*mouse_vect)(f); /* find number of mice */

}

/** ****************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the given mouse. This is a pure passthrough
function.

*******************************************************************************/

int imousebutton(FILE* f, int m)

{

    return (*mousebutton_vect)(f, m); /* find number of buttons */

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached. This is a pure passthrough function.

*******************************************************************************/

int ijoystick(FILE* f)

{

    return (*joystick_vect)(f); /* find number of joysticks */

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick. This is a pure passthrough
function.

*******************************************************************************/

int ijoybutton(FILE* f, int j)

{

    return (*joybutton_vect)(f, j); /* find number of buttons */

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning. This is a pure
passthrough function.

*******************************************************************************/

int ijoyaxis(FILE* f, int j)

{

    return (*joyaxis_vect)(f, j); /* find number of axies */

}

/** ****************************************************************************

Set tab

Sets a tab at the indicated column number.

*******************************************************************************/

void isettab(FILE* f, int t)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (t < 1 || t > MAXTAB) error("Invalid tab position");
    win->tab[t-1] = TRUE; /* set tab position */

}

/** ****************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void irestab(FILE* f, int t)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (t < 1 || t > MAXTAB) error("Invalid tab position");
    win->tab[t-1] = FALSE; /* set tab position */

}

/** ****************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void iclrtab(FILE* f)

{

    winptr win; /* windows record pointer */
    int t;

    win = txt2win(f); /* get window from file */
    for (t = 0; t < MAXTAB; t++) win->tab[t] = 0; /* clear all tab stops */

}

/** ****************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. This is a pure
passthrough function.

*******************************************************************************/

int ifunkey(FILE* f)

{

    return (*funkey_vect)(f); /* find number of function keys */

}

/** ****************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

void iframetimer(FILE* f, int e)

{

}

/** ****************************************************************************

Set automatic hold state

Sets the state of the automatic hold flag. Automatic hold is used to hold
programs that exit without having received a "terminate" signal from gralib.
This exists to allow the results of gralib unaware programs to be viewed after
termination, instead of exiting an destroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fulfills the requirement of
holding gralib unaware programs.

*******************************************************************************/

void iautohold(int e)

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

Attributes are performed, such as foreground/background coloring, modes, and
character attributes.

Character kerning is only available via this routine, and strsiz() is only
accurate for this routine, and not direct character placement, if kerning is
enabled.

Note: If an off angle (non-90 degree) text path is selected, this routine just
passes the characters on to plcchr(). This basically negates any speed
advantage.

*******************************************************************************/

void iwrtstr(FILE* f, char* s)

{

}

/** ****************************************************************************

Override event handler

Overrides or "hooks" the indicated event handler. The existing event handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

void ieventover(pa_evtcod e, pa_pevthan eh,  pa_pevthan* oeh)

{

}

/** ****************************************************************************

Override master event handler

Overrides or "hooks" the master event handler. The existing event handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

void ieventsover(pa_pevthan eh,  pa_pevthan* oeh)

{

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

void isendevent(FILE* f, pa_evtrec* er)

{

}

/** ****************************************************************************

Set window title

Sets the title of the current window.

*******************************************************************************/

void ititle(FILE* f, char* ts)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    if (win->title) free(win->title); /* free previous string */
    win->title = malloc(strlen(ts)+1);
    if (!win->title) error("Out of memory");
    /* set title to invoking program */
    strcpy(win->title, ts);

}

/** ****************************************************************************

Open window

Opens a window to an input/output pair. The window is opened and initalized.
If a parent is provided, the window becomes a child window of the parent.

The window id can be from 1 to MAXFIL, but 1 is reserved for the main I/O
window.

*******************************************************************************/

void iopenwin(FILE** infile, FILE** outfile, FILE* parent, int wid)

{

    /* open as child of client window */
    intopenwin(infile, outfile, parent, wid);

}

/** ****************************************************************************

Enable/disable buffered mode

Enables or disables surface buffering. If screen buffers are active, they are
freed.

*******************************************************************************/

void ibuffer(FILE* f, int e)

{

}

/** ****************************************************************************

Size buffer in characters

Sets or resets the size of the buffer surface, in character counts.

*******************************************************************************/

void isizbuf(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Get window size character

Gets the onscreen window size, in character terms.

*******************************************************************************/

void igetsiz(FILE* f, int* x, int* y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    *x = win->maxx; /* set size */
    *y = win->maxy;

}

/** ****************************************************************************

Set window size character

Sets the onscreen window size, in character terms.

*******************************************************************************/

void isetsiz(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->pmaxx = x; /* set size */
    win->pmaxy = y;
    win->maxx = win->pmaxx; /* copy to client dimensions */
    win->maxy = win->pmaxy;
    /* subtract frame from client if enabled */
    win->maxx -= (win->frame && win->size)*2;
    win->maxy -= win->frame*2+win->size*2;

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

void isetpos(FILE* f, int x, int y)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->orgx = x; /* set position in parent */
    win->orgy = y;

}

/** ****************************************************************************

Get screen size character

Gets the desktopsize, in character terms. Returns the pixel size of the screen
This occurs because the desktop does not have a fixed character aspect, so we
make one up, and our logical character is "one pixel" high and wide. It works
because it can only be used as a relative measurement.

*******************************************************************************/

void iscnsiz(FILE* f, int* x, int* y)

{

}

/** ****************************************************************************

Get screen center character

Finds the center of the screen which contains the given window. This is usually
used to locate where dialogs will go. It could be another location depending on
the system.

The old system was to simply locate the dialogs in the middle of the screen,
which fails (or at least is not the optimal placement) in the case of multiple
screens that are joined at one or more sides.

*******************************************************************************/

void iscncen(FILE* f, int* x, int* y)

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

void iwinclient(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms)

{

}

/** ****************************************************************************

Bring window to front of the Z order

Brings the indicated window to the front of the Z order.

*******************************************************************************/

void ifront(FILE* f)

{

}

/** ****************************************************************************

Puts window to the back of the Z order

Puts the indicated window to the back of the Z order.

*******************************************************************************/

void iback(FILE* f)

{

}

/** ****************************************************************************

Enable or disable window frame

Turns the window frame on and off.

*******************************************************************************/

void iframe(FILE* f, int e)

{

    winptr win; /* windows record pointer */

    win = txt2win(f); /* get window from file */
    win->frame = e; /* set frame state */

}

/** ****************************************************************************

Enable or disable window sizing

Turns the window sizing on and off.

On GNOME/Ubuntu 20.04 with GDM3 window manager, we are not capable of turning
off the size bars alone, so this is a no-op. It may work on other window
managers.

*******************************************************************************/

void isizable(FILE* f, int e)

{

}

/** ****************************************************************************

Enable or disable window system bar

Turns the system bar on and off.

I don't think XWindow can do this separately. Instead, the frame() function is
used to create component windows.

*******************************************************************************/

void isysbar(FILE* f, int e)

{

}

/** ****************************************************************************

Activate/distroy menu

Accepts a menu list, and sets the menu active. If there is already a menu
active, that is replaced. If the menu list is nil, then the active menu is
deleted.

*******************************************************************************/

void imenu(FILE* f, pa_menuptr m)

{

}

/** ****************************************************************************

Enable/disable menu entry

Enables or disables a menu entry by id. The entry is set to grey if disabled,
and will no longer send messages.

*******************************************************************************/

void imenuena(FILE* f, int id, int onoff)

{

}

/** ****************************************************************************

select/deselect menu entry

Selects or deselects a menu entry by id. The entry is set to checked if
selected, with no check if not.

*******************************************************************************/

void imenusel(FILE* f, int id, int select)

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

void istdmenu(pa_stdmenusel sms, pa_menuptr* sm, pa_menuptr pm)

{

}

/** ****************************************************************************

Allocate anonymous window id

Allocates and returns an "anonymous" window id. The window id numbers are assigned
by the client program. However, there a an alternative set of ids that are
allocated as needed. Graphics keeps track of which anonymous ids have been
allocated and which have been freed.

The implementation here is to assign anonymous window ids negative numbers,
starting with -1 and proceeding downwards. 0 is never assigned. The use of
negative ids insure that the normal window ids will never overlap any anonyous
window ids.

Note that the wid entry will actually be opened by openwin(), and will be closed
by closewin(), so there is no need to deallocate this wid. Once an anonymous id
is allocated, it is reserved until it is used and removed by killwidget().

*******************************************************************************/

int igetwinid(void)

{

}

/** ****************************************************************************

Set window focus

Sends the focus, or which window gets input characters, to a given window.

*******************************************************************************/

void ifocus(FILE* f)

{

}

/** ****************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attribute.

We handle some elementary control codes here, like newline, backspace and form
feed. However, the idea is not to provide a parallel set of screen controls.
That's what the API is for.

*******************************************************************************/

static void plcchr(FILE* f, char c)

{

    winptr  win;   /* windows record pointer */
    scnrec* scp;   /* pointer to screenlocation */

    win = txt2win(f); /* get window from file */
    if (!win->visible) winvis(win); /* make sure we are displayed */
    /* handle special character cases first */
    if (c == '\r')
        /* carriage return, position to extreme left */
        win->curx = 1; /* set to extreme left */
    else if (c == '\n') {

        win->curx = 1; /* set to extreme left */
        idown(f); /* line feed, move down */

    } else if (c == '\b') ileft(f); /* back space, move left */
    else if (c == '\f') clrscn(f); /* clear screen */
    else if (c == '\t') itab(f); /* process tab */
    /* only output visible characters */
    else if (c >= ' ' && c != 0x7f) {

        if (icurbnd(f)) { /* cursor is in bounds */

            if (win->bufmod) { /* buffer is active */

                /* index screen character location */
                scp = &(win->screens[win->curdsp-1]
                    [(win->cury-1)*win->maxx+(win->curx-1)*sizeof(scnrec)]);
                /* place character to buffer */
                scp->ch = c;
                scp->forec = win->fcolor;
                scp->backc = win->bcolor;
                scp->attr = win->attr;

            }
            if (indisp(win)) { /* do it again for the current screen */


                setattrs(win->attr); /* set attributes */
                setfcolor(win->fcolor); /* set colors */
                setbcolor(win->bcolor);
                /* draw character to active screen */
                setcursor(win->curx+win->orgx-1+win->coffx,
                          win->cury+win->orgy-1+win->coffy);
                wrtchr(c); /* output */

            }

        }
        /* advance to next character */
        iright(f);

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

/* find window with non-zero input buffer */

static int fndful(int fd) /* output window file */

{

    int fi; /* file index */
    int ff; /* found file */

    ff = -1; /* set no file found */
    for (fi = 0; fi < MAXFIL; fi++) if (opnfil[fi])
        if (opnfil[fi]->inl == fd && opnfil[fi]->win != NULL)
            /* links the input file, and has a window */
            if (strlen(opnfil[fi]->win->inpbuf) > 0) ff = fi; /* found one */

    return (ff); /* return result */

}

static ssize_t ivread(pread_t readdc, int fd, void* buff, size_t count)

{

    int            l;   /* length left on destination */
    winptr         win; /* pointer to window data */
    int            ofn; /* output file handle */
    ssize_t        rc;  /* return code */
    unsigned char* ba;

    if (fd < 0 || fd >= MAXFIL) error("Invalid handle");
    if (opnfil[fd] && opnfil[fd]->inw) { /* process input file */

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
                while (win->inpbuf[win->inpptr] && l) {

                    /* there is data in the buffer, and we need that data */
                    *ba = win->inpbuf[win->inpptr]; /* get and place next character */
                    if (win->inpptr < MAXLIN) win->inpptr++; /* next */
                    /* if we have just read the last of that line, flag buffer
                       empty */
                    if (*ba == '\n') {

                        win->inpptr = -1;
                        win->inpbuf[0] = 0;

                    }
                    l--; /* count characters */

                }

            }

        }
        rc = count; /* set all bytes read */

    } else rc = (*readdc)(fd, buff, count);

    return rc;

}

static ssize_t iread(int fd, void* buff, size_t count)

{

    return ivread(ofpread, fd, buff, count);

}

static ssize_t iread_nocancel(int fd, void* buff, size_t count)

{

    return ivread(ofpread_nocancel, fd, buff, count);

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

    if (fd < 0 || fd >= MAXFIL) error("Invalid handle");
    if (opnfil[fd] && opnfil[fd]->win) { /* process window output file */

        win = opnfil[fd]->win; /* index window */
        /* send data to terminal */
        while (cnt--) plcchr(opnfil[fd]->sfp, *p++);
        rc = count; /* set return same as count */

    } else rc = (*writedc)(fd, buff, count);

    return rc;

}

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    return ivwrite(ofpwrite, fd, buff, count);

}

static ssize_t iwrite_nocancel(int fd, const void* buff, size_t count)

{

    return ivwrite(ofpwrite_nocancel, fd, buff, count);

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

    return ivopen(ofpopen, pathname, flags, perm);

}

static int iopen_nocancel(const char* pathname, int flags, int perm)

{

    return ivopen(ofpopen_nocancel, pathname, flags, perm);

}

/** ****************************************************************************

Close

If the file is attached to an output window, closes the window file. Otherwise,
the close is just passed on.

*******************************************************************************/

static int ivclose(pclose_t closedc, int fd)

{

    if (fd < 0 || fd >= MAXFIL) error("Invalid handle");
    /* check if the file is an output window, and close if so */
    if (opnfil[fd] && opnfil[fd]->win) closewin(fd);

    return (*closedc)(fd);

}

static int iclose(int fd)

{

    return ivclose(ofpclose, fd);

}

static int iclose_nocancel(int fd)

{

    return ivclose(ofpclose_nocancel, fd);

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
    if (fd == INPFIL || fd == OUTFIL)
        error("Cannot perform operation on special file");

    return (*lseekdc)(fd, offset, whence);

}

static off_t ilseek(int fd, off_t offset, int whence)

{

    return ivlseek(ofplseek, fd, offset, whence);

}

/** ****************************************************************************

Widgets startup

*******************************************************************************/

static void init_managerc(void) __attribute__((constructor (104)));
static void init_managerc()

{

    int fn;  /* file number */
    int wid; /* window id */
    int ofn; /* standard output file number */
    int ifn; /* standard input file number */

    winfre = NULL; /* clear free windows structure list */
    winlst = NULL; /* clear master window list */
    rootlst = NULL; /* clear root window list */
    ztop = -1; /* clear Z order top (none) */

    /* clear open files tables */
    for (fn = 0; fn < MAXFIL; fn++) {

        opnfil[fn] = NULL; /* set unoccupied */
        /* clear file to window logical number translator table */
        filwin[fn] = -1; /* set unoccupied */

    }

    /* clear window equivalence table */
    for (fn = 0; fn < MAXFIL*2+1; fn++) {

        /* clear window logical number translator table */
        xltwin[fn] = -1; /* set unoccupied */

    }

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

    /* override entry calls for API */
    _pa_cursor_ovr(icursor, &cursor_vect);
    _pa_maxx_ovr(imaxx, &maxx_vect);
    _pa_maxy_ovr(imaxy, &maxy_vect);
    _pa_home_ovr(ihome, &home_vect);
    _pa_del_ovr(idel, &del_vect);
    _pa_up_ovr(iup, &up_vect);
    _pa_down_ovr(idown, &down_vect);
    _pa_left_ovr(ileft, &left_vect);
    _pa_right_ovr(iright, &right_vect);
    _pa_blink_ovr(iblink, &blink_vect);
    _pa_reverse_ovr(ireverse, &reverse_vect);
    _pa_underline_ovr(iunderline, &underline_vect);
    _pa_superscript_ovr(isuperscript, &superscript_vect);
    _pa_subscript_ovr(isubscript, &subscript_vect);
    _pa_italic_ovr(iitalic, &italic_vect);
    _pa_bold_ovr(ibold, &bold_vect);
    _pa_strikeout_ovr(istrikeout, &strikeout_vect);
    _pa_standout_ovr(istandout, &standout_vect);
    _pa_fcolor_ovr(ifcolor, &fcolor_vect);
    _pa_bcolor_ovr(ibcolor, &bcolor_vect);
    _pa_auto_ovr(iauto, &auto_vect);
    _pa_curvis_ovr(icurvis, &curvis_vect);
    _pa_scroll_ovr(iscroll, &scroll_vect);
    _pa_curx_ovr(icurx, &curx_vect);
    _pa_cury_ovr(icury, &cury_vect);
    _pa_curbnd_ovr(icurbnd, &curbnd_vect);
    _pa_select_ovr(iselect, &select_vect);
    _pa_event_ovr(ievent, &event_vect);
    _pa_timer_ovr(itimer, &timer_vect);
    _pa_killtimer_ovr(ikilltimer, &killtimer_vect);
    _pa_mouse_ovr(imouse, &mouse_vect);
    _pa_mousebutton_ovr(imousebutton, &mousebutton_vect);
    _pa_joystick_ovr(ijoystick, &joystick_vect);
    _pa_joybutton_ovr(ijoybutton, &joybutton_vect);
    _pa_joyaxis_ovr(ijoyaxis, &joyaxis_vect);
    _pa_settab_ovr(isettab, &settab_vect);
    _pa_restab_ovr(irestab, &restab_vect);
    _pa_clrtab_ovr(iclrtab, &clrtab_vect);
    _pa_funkey_ovr(ifunkey, &funkey_vect);
    _pa_frametimer_ovr(iframetimer, &frametimer_vect);
    _pa_autohold_ovr(iautohold, &autohold_vect);
    _pa_wrtstr_ovr(iwrtstr, &wrtstr_vect);
    _pa_eventover_ovr(ieventover, &eventover_vect);
    _pa_eventsover_ovr(ieventsover, &eventsover_vect);
    _pa_sendevent_ovr(isendevent, &sendevent_vect);
    _pa_title_ovr(ititle, &title_vect);
    _pa_openwin_ovr(iopenwin, &openwin_vect);
    _pa_buffer_ovr(ibuffer, &buffer_vect);
    _pa_sizbuf_ovr(isizbuf, &sizbuf_vect);
    _pa_getsiz_ovr(igetsiz, &getsiz_vect);
    _pa_setsiz_ovr(isetsiz, &setsiz_vect);
    _pa_setpos_ovr(isetpos, &setpos_vect);
    _pa_scnsiz_ovr(iscnsiz, &scnsiz_vect);
    _pa_scncen_ovr(iscncen, &scncen_vect);
    _pa_winclient_ovr(iwinclient, &winclient_vect);
    _pa_front_ovr(ifront, &front_vect);
    _pa_back_ovr(iback, &back_vect);
    _pa_frame_ovr(iframe, &frame_vect);
    _pa_sizable_ovr(isizable, &sizable_vect);
    _pa_sysbar_ovr(isysbar, &sysbar_vect);
    _pa_menu_ovr(imenu, &menu_vect);
    _pa_menuena_ovr(imenuena, &menuena_vect);
    _pa_menusel_ovr(imenusel, &menusel_vect);
    _pa_stdmenu_ovr(istdmenu, &stdmenu_vect);
    _pa_getwinid_ovr(igetwinid, &getwinid_vect);
    _pa_focus_ovr(ifocus, &focus_vect);

    /* reset all attributes */
    (*superscript_vect)(stdout, FALSE);
    (*subscript_vect)(stdout, FALSE);
    (*blink_vect)(stdout, FALSE);
    (*strikeout_vect)(stdout, FALSE);
    (*italic_vect)(stdout, FALSE);
    (*bold_vect)(stdout, FALSE);
    (*underline_vect)(stdout, FALSE);
    (*reverse_vect)(stdout, FALSE);
    attr = 0;

    /* set default colors */
    fcolor = pa_black;
    bcolor = pa_white;
    (*fcolor_vect)(stdout, fcolor);
    (*bcolor_vect)(stdout, bcolor);

    /* home cursor */
    (*home_vect)(stdout);
    curx = 1;
    cury = 1;

    /* set cursor on */
    (*curvis_vect)(stdout, TRUE);
    curon = TRUE;

    /* set auto off */
    (*auto_vect)(stdout, FALSE);

    /* set mouse tracking invalid */
    mousex = -1;
    mousey = -1;

    /* open stdin and stdout as I/O window set */
    ifn = fileno(stdin); /* get logical id stdin */
    ofn = fileno(stdout); /* get logical id stdout */
    openio(stdin, stdout, ifn, ofn, -1, 1, FALSE, TRUE); /* process open */

}

/** ****************************************************************************

Widgets shutdown

*******************************************************************************/

static void deinit_managerc(void) __attribute__((destructor (104)));
static void deinit_managerc()

{

    int fn; /* file number */
    int i;

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

    /* holding copies of API override vectors */
    pa_cursor_t cppcursor;
    pa_maxx_t cppmaxx;
    pa_maxy_t cppmaxy;
    pa_home_t cpphome;
    pa_del_t cppdel;
    pa_up_t cppup;
    pa_down_t cppdown;
    pa_left_t cppleft;
    pa_right_t cppright;
    pa_blink_t cppblink;
    pa_reverse_t cppreverse;
    pa_underline_t cppunderline;
    pa_superscript_t cppsuperscript;
    pa_subscript_t cppsubscript;
    pa_italic_t cppitalic;
    pa_bold_t cppbold;
    pa_strikeout_t cppstrikeout;
    pa_standout_t cppstandout;
    pa_fcolor_t cppfcolor;
    pa_bcolor_t cppbcolor;
    pa_auto_t cppauto;
    pa_curvis_t cppcurvis;
    pa_scroll_t cppscroll;
    pa_curx_t cppcurx;
    pa_cury_t cppcury;
    pa_curbnd_t cppcurbnd;
    pa_select_t cppselect;
    pa_event_t cppevent;
    pa_timer_t cpptimer;
    pa_killtimer_t cppkilltimer;
    pa_mouse_t cppmouse;
    pa_mousebutton_t cppmousebutton;
    pa_joystick_t cppjoystick;
    pa_joybutton_t cppjoybutton;
    pa_joyaxis_t cppjoyaxis;
    pa_settab_t cppsettab;
    pa_restab_t cpprestab;
    pa_clrtab_t cppclrtab;
    pa_funkey_t cppfunkey;
    pa_frametimer_t cppframetimer;
    pa_autohold_t cppautohold;
    pa_wrtstr_t cppwrtstr;
    pa_eventover_t cppeventover;
    pa_eventsover_t cppeventsover;
    pa_sendevent_t cppsendevent;
    pa_title_t cpptitle;
    pa_openwin_t cppopenwin;
    pa_buffer_t cppbuffer;
    pa_sizbuf_t cppsizbuf;
    pa_getsiz_t cppgetsiz;
    pa_setsiz_t cppsetsiz;
    pa_setpos_t cppsetpos;
    pa_scnsiz_t cppscnsiz;
    pa_scncen_t cppscncen;
    pa_winclient_t cppwinclient;
    pa_front_t cppfront;
    pa_back_t cppback;
    pa_frame_t cppframe;
    pa_sizable_t cppsizable;
    pa_sysbar_t cppsysbar;
    pa_menu_t cppmenu;
    pa_menuena_t cppmenuena;
    pa_menusel_t cppmenusel;
    pa_stdmenu_t cppstdmenu;
    pa_getwinid_t cppgetwinid;
    pa_focus_t cppfocus;


    _pa_cursor_ovr(cursor_vect, &cppcursor);
    _pa_maxx_ovr(maxx_vect, &cppmaxx);
    _pa_maxy_ovr(maxy_vect, &cppmaxy);
    _pa_home_ovr(home_vect, &cpphome);
    _pa_del_ovr(del_vect, &cppdel);
    _pa_up_ovr(up_vect, &cppup);
    _pa_down_ovr(down_vect, &cppdown);
    _pa_left_ovr(left_vect, &cppleft);
    _pa_right_ovr(right_vect, &cppright);
    _pa_blink_ovr(blink_vect, &cppblink);
    _pa_reverse_ovr(reverse_vect, &cppreverse);
    _pa_underline_ovr(underline_vect, &cppunderline);
    _pa_superscript_ovr(superscript_vect, &cppsuperscript);
    _pa_subscript_ovr(subscript_vect, &cppsubscript);
    _pa_italic_ovr(italic_vect, &cppitalic);
    _pa_bold_ovr(bold_vect, &cppbold);
    _pa_strikeout_ovr(strikeout_vect, &cppstrikeout);
    _pa_standout_ovr(standout_vect, &cppstandout);
    _pa_fcolor_ovr(fcolor_vect, &cppfcolor);
    _pa_bcolor_ovr(bcolor_vect, &cppbcolor);
    _pa_auto_ovr(auto_vect, &cppauto);
    _pa_curvis_ovr(curvis_vect, &cppcurvis);
    _pa_scroll_ovr(scroll_vect, &cppscroll);
    _pa_curx_ovr(curx_vect, &cppcurx);
    _pa_cury_ovr(cury_vect, &cppcury);
    _pa_curbnd_ovr(curbnd_vect, &cppcurbnd);
    _pa_select_ovr(select_vect, &cppselect);
    _pa_event_ovr(event_vect, &cppevent);
    _pa_timer_ovr(timer_vect, &cpptimer);
    _pa_killtimer_ovr(killtimer_vect, &cppkilltimer);
    _pa_mouse_ovr(mouse_vect, &cppmouse);
    _pa_mousebutton_ovr(mousebutton_vect, &cppmousebutton);
    _pa_joystick_ovr(joystick_vect, &cppjoystick);
    _pa_joybutton_ovr(joybutton_vect, &cppjoybutton);
    _pa_joyaxis_ovr(joyaxis_vect, &cppjoyaxis);
    _pa_settab_ovr(settab_vect, &cppsettab);
    _pa_restab_ovr(restab_vect, &cpprestab);
    _pa_clrtab_ovr(clrtab_vect, &cppclrtab);
    _pa_funkey_ovr(funkey_vect, &cppfunkey);
    _pa_frametimer_ovr(frametimer_vect, &cppframetimer);
    _pa_autohold_ovr(autohold_vect, &cppautohold);
    _pa_wrtstr_ovr(wrtstr_vect, &cppwrtstr);
    _pa_eventover_ovr(eventover_vect, &cppeventover);
    _pa_eventsover_ovr(eventsover_vect, &cppeventsover);
    _pa_sendevent_ovr(sendevent_vect, &cppsendevent);
    _pa_title_ovr(title_vect, &cpptitle);
    _pa_openwin_ovr(openwin_vect, &cppopenwin);
    _pa_buffer_ovr(buffer_vect, &cppbuffer);
    _pa_sizbuf_ovr(sizbuf_vect, &cppsizbuf);
    _pa_getsiz_ovr(getsiz_vect, &cppgetsiz);
    _pa_setsiz_ovr(setsiz_vect, &cppsetsiz);
    _pa_setpos_ovr(setpos_vect, &cppsetpos);
    _pa_scnsiz_ovr(scnsiz_vect, &cppscnsiz);
    _pa_scncen_ovr(scncen_vect, &cppscncen);
    _pa_winclient_ovr(winclient_vect, &cppwinclient);
    _pa_front_ovr(front_vect, &cppfront);
    _pa_back_ovr(back_vect, &cppback);
    _pa_frame_ovr(frame_vect, &cppframe);
    _pa_sizable_ovr(sizable_vect, &cppsizable);
    _pa_sysbar_ovr(sysbar_vect, &cppsysbar);
    _pa_menu_ovr(menu_vect, &cppmenu);
    _pa_menuena_ovr(menuena_vect, &cppmenuena);
    _pa_menusel_ovr(menusel_vect, &cppmenusel);
    _pa_stdmenu_ovr(stdmenu_vect, &cppstdmenu);
    _pa_getwinid_ovr(getwinid_vect, &cppgetwinid);
    _pa_focus_ovr(focus_vect, &cppfocus);

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
        error("System consistency check");

}
