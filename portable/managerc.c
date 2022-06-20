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

3. The default I/O surface is created maximized, as are all created windows by
default.

4. A control character is provided to cycle forward through windows, as well as
backwards. Thus managerc serves as a "screen switcher" by default.

5. By default, only standard ASCII characters are used to depict frame
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

#define MAXFIL 100 /* maximum open files */
#define MAXWIG 100 /* maximum widgets per window */
/* amount of space in pixels to add around scrollbar sliders */
#define ENDSPACE 6
#define ENDLEDSPC 10 /* space at start and end of text edit box */
/* user defined messages */
#define WMC_LGTFOC pa_etwidget+0 /* widget message code: light up focus */
#define WMC_DRKFOC pa_etwidget+1 /* widget message code: turn off focus */
#define TABHGT 2 /* tab bar tab height * char size y */

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
static pa_auto(_t auto(_vect;
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

typedef struct scncon* scnptr;
typedef struct scncon { /* screen context */

    /* fields used by graph module */
    int     lwidth;      /* width of lines */
    /* note that the pixel and character dimensions and positions are kept
      in parallel for both characters and pixels */
    int     maxx;        /* maximum characters in x */
    int     maxy;        /* maximum characters in y */
    int     curx;        /* current cursor location x */
    int     cury;        /* current cursor location y */
    int     attr;        /* set of active attributes */
    int     autof;       /* current status of scroll and wrap */
    int     tab[MAXTAB]; /* tabbing array */
    int     curv;        /* cursor visible */
    char*   buf;         /* screen buffer */

} scncon;

/* window description */
typedef struct winrec* winptr;
typedef struct winrec {

    winptr next;              /* next entry (for free list) */
    int    parlfn;            /* logical parent */
    winptr parwin;            /* link to parent (or NULL for parentless) */
    int    wid;               /* this window logical id */
    winptr childwin;          /* list of child windows */
    winptr childlst;          /* list pointer if this is a child */
    scnptr screens[MAXCON];   /* screen contexts array */
    int    curdsp;            /* index for current display screen */
    int    curupd;            /* index for current update screen */
    int    orgx;              /* window origin in root x */
    int    orgy;              /* window origin in root y */
    int    maxx;              /* maximum x size */
    int    maxy;              /* maximum y size */
    int    bufx;              /* buffer size x characters */
    int    bufy;              /* buffer size y characters */
    int    bufmod;            /* buffered screen mode */
    metptr metlst;            /* menu tracking list */
    metptr menu;              /* "faux menu" bar */
    int    frame;             /* frame on/off */
    int    size;              /* size bars on/off */
    int    sysbar;            /* system bar on/off */

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

} filrec;

static filptr        opnfil[MAXFIL];     /* open files table */

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

API calls implemented at this level

*******************************************************************************/

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

void icursor(FILE* f, int x, int y)

{

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int imaxx(FILE* f)

{

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of collumns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int imaxy(FILE* f)

{

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void ihome(FILE* f)

{

}

/** ****************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void idel(FILE* f)

/** ****************************************************************************

Move cursor up

Moves the cursor position up one line. If the cursor is at screen top, and auto
is on, the screen is scrolled up, meaning that the screen contents are moved
down a line of text. If auto is off, the cursor can simply continue into
negative space as long as it stays within the bounds -INT_MAX to INT_MAX.

*******************************************************************************/

void iup(FILE* f)

{

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

}

/** ****************************************************************************

Move cursor left

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

void ileft(FILE* f)

{

}

/** ****************************************************************************

Move cursor right

Moves the cursor one character right.

*******************************************************************************/

void iright(FILE* f)

{

}

/** ****************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

Graphical mode does not implement blink mode.

*******************************************************************************/

void iblink(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void ireverse(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.

*******************************************************************************/

void iunderline(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and elevated font.

*******************************************************************************/

void isuperscript(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

Note that subscript is implemented by a reduced size and lowered font.

*******************************************************************************/

void isubscript(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void iitalic(FILE* f, int e)

{

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void ibold(FILE* f, int e)

{

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

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void istandout(FILE* f, int e)

{

}

/** ****************************************************************************

Set foreground color

Sets the foreground color from the universal primary code.

*******************************************************************************/

void ifcolor(FILE* f, pa_color c)

{

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void ibcolor(FILE* f, pa_color c)

{

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

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void icurvis(FILE* f, int e)

{

}

/** ****************************************************************************

Scroll screen

Scrolls the terminal screen by deltas in any given direction. If the scroll
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

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int icury(FILE* f)

{

}

/** ****************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns TRUE if so.

*******************************************************************************/

int icurbnd(FILE* f)

{

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

Returns the number of mice implemented. XWindow supports only one mouse.

*******************************************************************************/

int imouse(FILE* f)

{

}

/** ****************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version. XWindow supports from 1 to 5 buttons.

*******************************************************************************/

int imousebutton(FILE* f, int m)

{

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

int ijoystick(FILE* f)

{

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

int ijoybutton(FILE* f, int j)

{

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodimensional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

int ijoyaxis(FILE* f, int j)

{

}

/** ****************************************************************************

Set tab

Sets a tab at the indicated collumn number.

*******************************************************************************/

void isettab(FILE* f, int t)

{

}

/** ****************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void irestab(FILE* f, int t)

{

}

/** ****************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void iclrtab(FILE* f)

{

}

/** ****************************************************************************

Find number of function keys

Finds the total number of function, or general assignment keys. Currently, we
just implement the 12 unshifted PC function keys. We may add control and shift
function keys as well.

*******************************************************************************/

int ifunkey(FILE* f)

{

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
termination, instead of exiting an distroying the window. This mode works for
most circumstances, but an advanced program may want to exit for other reasons
than being closed by the system bar. This call can turn automatic holding off,
and can only be used by an advanced program, so fufills the requirement of
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

Gets the onscreen window size, in character terms. If the window has a parent,
the demensions are converted to the current character size there. Otherwise,
the pixel based dementions are returned. This occurs because the desktop does
not have a fixed character aspect, so we make one up, and our logical character
is "one pixel" high and wide. It works because it can only be used as a
relative measurement.

*******************************************************************************/

void igetsiz(FILE* f, int* x, int* y)

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

void isetsiz(FILE* f, int x, int y)

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

static void plcchr(int fd, winptr win, char c)

{

    scnptr sc;    /* pointer to current screen */
    char   cb[2]; /* character send buffer */

    sc = win->screens[win->curupd-1]; /* index current screen */
    /* handle special character cases first */
    if (c == '\r')
        /* carriage return, position to extreme left */
        sc->curx = 1; /* set to extreme left */
    else if (c == '\n') {

        sc->curx = 1; /* set to extreme left */
        idown(win); /* line feed, move down */

    } else if (c == '\b') ileft(win); /* back space, move left */
    else if (c == '\f') iclear(win); /* clear screen */
    else if (c == '\t') itab(win); /* process tab */
    /* only output visible characters */
    else if (c >= ' ' && c != 0x7f) {

        if (win->bufmod) { /* buffer is active */

            /* place character to buffer */
            sc->buf[(sc->cury-1)*sc->maxx+(sc->curx-1)] = c;

        }
        if (indisp(win)) { /* do it again for the current screen */

            /* draw character to active screen */
            (*cursor_vect)(opnfil[fd]->sfp, sc->curx+win->orgx-1, sc->cury+win->orgy-1);
            cb[0] = c; /* place character in buffer */
            cb[1] = 0; /* terminate */
            (*wrtstr)(opnfil[fd]->sfp, cb); /* output */

        }
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

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
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

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd] && opnfil[fd]->win) { /* process window output file */

        win = opnfil[fd]->win; /* index window */
        /* send data to terminal */
        while (cnt--) plcchr(fd, win, *p++);
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

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
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
    if (fd == INPFIL || fd == OUTFIL) error(efilopr);

    return (*lseekdc)(fd, offset, whence);

}

static off_t ilseek(int fd, off_t offset, int whence)

{

    return ivlseek(ofplseek, fd, offset, whence);

}

/** ****************************************************************************

Widgets startup

*******************************************************************************/

static void init_widgets(void) __attribute__((constructor (102)));
static void init_widgets()

{

    int fn; /* file number */
    int wid; /* window id */

    /* override the event handler */
    pa_eventsover(widget_event, &widget_event_old);

    wigfre = NULL; /* clear widget free list */

    /* clear open files table */
    for (fn = 0; fn < MAXFIL; fn++) opnfil[fn] = NULL;

    /* clear window equivalence table */
    for (fn = 0; fn < MAXFIL*2+1; fn++)
        /* clear window logical number translator table */
        xltwig[fn] = NULL; /* set no widget entry */

    /* open "window 0" dummy window */
    pa_openwin(&stdin, &win0, NULL, pa_getwinid()); /* open window */
    pa_buffer(win0, FALSE); /* turn off buffering */
    pa_auto(win0, FALSE); /* turn off auto (for font change) */
    pa_font(win0, PA_FONT_SIGN); /* set sign font */
    pa_frame(win0, FALSE); /* turn off frame */

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
    _pa_auto(_ovr(iauto(, &auto(_vect);
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

}

/** ****************************************************************************

Widgets shutdown

*******************************************************************************/

static void deinit_widgets(void) __attribute__((destructor (102)));
static void deinit_widgets()

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
    pa_auto(_t cppauto(;
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
    _pa_auto(_ovr(auto(_vect, &cppauto();
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
        error(esystem);

}
