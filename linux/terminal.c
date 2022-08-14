/** ****************************************************************************
*                                                                              *
*                                 Petit AMI                                    *
*                                                                              *
* xterm/ANSI console interface                                                 *
*                                                                              *
* This is a standard PA/TK terminal module using ANSI control codes, some      *
* of which are specific to various VT10x terminals and xterm which emulates    *
* them. Its mainly for xterm and compatibles, which means Linux and Mac OS X.  *
*                                                                              *
* Uses ANSI C and a good bit of POSIX. The stdio interface is done by a        *
* specially modified library that includes the ability to hook or override     *
* the bottom level of I/O.                                                     *
*                                                                              *
* The module works by keeping a in memory image of the output terminal and     *
* its attributes, along the lines of what curses does. Because it always knows *
* what the state of the actual terminal should be, it does not need to read    *
* from the terminal to determine the state of individual character cells.      *
*                                                                              *
* In this version, the file argument is not used.                              *
*                                                                              *
* The ANSI interface is mainly useful in Linux/BSD because the ANSI controls   *
* are standardized there, and serial connections are more widely used          *
* (like SSH). Curses is also used, but it, too, is typically just a wrapper    *
* for ANSI controls, since the wide variety of different serial terminals from *
* the 1970s and before have died off (which perhaps shows that one way to      *
* standardize the world is to get a smaller world).                            *
*                                                                              *
* The ANSI driver really has two modes: one when used as a local program, and  *
* another when used remotely via serial connection, telnet, ssh or similar     *
* program. In the latter case, the joystick position is irrelevant, because    *
* there is, at this writing, no remote method to read the joystick.            *
*                                                                              *
* Terminal can also be used on Linux that boots to the console. This is either *
* a Linux that boots up without XWindows, or by switching to an alternate      *
* console. This may or may not support a mouse.                                *
*                                                                              *
* Petit-Ami is a standard that goes back to a start in 1984 with significant   *
* improvements in the 1997 and on years. It was the library standard for       *
* Pascaline, but I translated it to C.                                         *
*                                                                              *
* The first version of this package implemented the call set on a Wyse 80      *
* terminal.                                                                    *
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

/* linux definitions */
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#ifndef __MACH__ /* Mac OS X */
#include <linux/joystick.h>
#endif
#include <fcntl.h>

/* Petit-Ami definitions */
#include "terminal.h"
#include "system_event.h"

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

#define JOYENB TRUE /* enable joysticks */

/* Default terminal size sets the geometry of the terminal if we cannot find
   out the geometry from the terminal itself. */
#define DEFXD 80 /* default terminal size, 80x24, this is Linux standard */
#define DEFYD 24

#define MAXKEY 20     /* maximum length of key sequence */
#define MAXCON  10    /**< number of screen contexts */
#define MAXLIN  250   /* maximum length of input buffered line */
#define MAXFKEY 10    /**< maximum number of function keys */
#define MAXJOY  10    /* number of joysticks possible */
#define DMPEVT  FALSE /* enable dump Petit-Ami messages */
#define ALLOWUTF8     /* enable UTF-8 encoding */

/*
 * Standard mouse decoding has a limit of about 223 in x or y. SGR mode
 * can go from 1 to 2015.
 */
#define MOUSESGR      /* use SGR mouse mode (extended positions) */

/*
 * Use 24 bit color encoding. This was used to get pure white in an Xterm, which
 * normally does not appear to be possible.
 */
#define COLOR24

/* file handle numbers at the system interface level */

#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */
#define ERRFIL 2 /* handle to standard error */

/* Foreground and background color bases.
   The "normal" ANSI base gives dull colors on Linux, Windows and probably the
   Mac as well (untested). On linux the AIX colors give bright, and on Windows
   blink gives bright (apparently since blink is not implemented there). This
   was considered a non-issue since we use the Windows console mode driver
   instead of this driver.
   Note that dull colors are mainly an issue for "paper white" background
   programs because dull white looks different from every other window on the
   system. */
#define AIXTERM 1 /* set aix terminal colors active */

#ifdef AIXTERM

#define ANSIFORECOLORBASE 30 /* ansi override base */
#define ANSIBACKCOLORBASE 40

#define FORECOLORBASE 90 /* aixterm */
#define BACKCOLORBASE 100

#else

#define ANSIFORECOLORBASE 30 /* ansi override base */
#define ANSIBACKCOLORBASE 40

#define FORECOLORBASE 30 /* ansi */
#define BACKCOLORBASE 40

#endif

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

/** screen attribute */
typedef enum {

    /* no attribute */               sanone,
    /* blinking text (foreground) */ sablink,
    /* reverse video */              sarev,
    /* underline */                  saundl,
    /* superscript */                sasuper,
    /* subscripting */               sasubs,
    /* italic text */                saital,
    /* bold text */                  sabold

} scnatt;

/** single character on screen container. note that not all the attributes
   that appear here can be changed */
typedef struct {

#ifdef ALLOWUTF8
    /* character at location */        unsigned char ch[4]; /* encoded utf-8 */
#else
    /* character at location */        unsigned char ch;
#endif
    /* foreground color at location */ pa_color forec;
    /* background color at location */ pa_color backc;
    /* active attribute at location */ scnatt attr;

} scnrec, *scnptr;

/* macro to access screen elements by y,x */
#define SCNBUF(sc, x, y) (sc[(y-1)*bufx+(x-1)])

/* Joystick tracking structure */
typedef struct joyrec* joyptr; /* pointer to joystick record */
typedef struct joyrec {

    int fid;    /* joystick file id */
    int sid;    /* system event id */
    int axis;   /* number of joystick axes */
    int button; /* number of joystick buttons */
    int ax;     /* joystick x axis save */
    int ay;     /* joystick y axis save */
    int az;     /* joystick z axis save */
    int a4;     /* joystick axis 4 save */
    int a5;     /* joystick axis 5 save */
    int a6;     /* joystick axis 6 save */
    int no;     /* logical number of joystick, 1-n */

} joyrec;

/* PA queue structure. Its a bubble list. */
typedef struct paevtque {

    struct paevtque* next; /* next in list */
    struct paevtque* last; /* last in list */
    pa_evtrec       evt;  /* event data */

} paevtque;

/** Error codes this module */
typedef enum {

    eftbful,  /* file table full */
    ejoyacc,  /* joystick access */
    etimacc,  /* timer access */
    efilopr,  /* cannot perform operation on special file */
    einvpos,  /* invalid screen position */
    efilzer,  /* filename is empty */
    einvscn,  /* invalid screen number */
    einvhan,  /* invalid handle */
    emouacc,  /* mouse access */
    eoutdev,  /* output device error */
    einpdev,  /* input device error */
    einvtab,  /* invalid tab stop */
    einvjoy,  /* Invalid joystick ID */
    esendevent_unimp, /* sendevent unimplemented */
    etitle_unimp,     /* title unimplemented */
    eopenwin_unimp,   /* openwin unimplemented */
    ebuffer_unimp,    /* buffer unimplemented */
    esizbuf_unimp,    /* sizbuf unimplemented */
    egetsiz_unimp,    /* getsiz unimplemented */
    esetsiz_unimp,    /* setsiz unimplemented */
    esetpos_unimp,    /* setpos unimplemented */
    escnsiz_unimp,    /* scnsiz unimplemented */
    escncen_unimp,    /* scncen unimplemented */
    ewinclient_unimp, /* winclient unimplemented */
    efront_unimp,     /* front unimplemented */
    eback_unimp,      /* back unimplemented */
    eframe_unimp,     /* frame unimplemented */
    esizable_unimp,   /* sizable unimplemented */
    esysbar_unimp,    /* sysbar unimplemented */
    emenu_unimp,      /* menu unimplemented */
    emenuena_unimp,   /* menuena unimplemented */
    emenusel_unimp,   /* menusel unimplemented */
    estdmenu_unimp,   /* stdmenu unimplemented */
    egetwinid_unimp,  /* getwinid unimplemented */
    efocus_unimp,     /* focus unimplemented */
    esystem   /* system fault */

} errcod;

/*
 * keyboard key equivalents table
 *
 * Contains equivalent strings as are returned from xterm keys
 * attached to an IBM-PC keyboard.
 *
 * Note these definitions are mosly CUA (common user interface). One exception
 * was the terminate key, which has a long tradition as CTRL-C, and I left it.
 *
 * In xterm the home and end keys return the same regardless of their shift,
 * control or alt status. Some of the CUA keys may not be available simply
 * because the GUI intercepts them. For example, print screen, insert and
 * similar keys. Thus we need a xterm equivalent, and we use alternative keys.
 *
 */

char *keytab[pa_etterm+1+MAXFKEY] = {

    /* Common controls are:
    Codes                   Meaning                   IBM-PC keyboard equivalents */
    "",                     /* ANSI character returned */
    "\33\133\101",          /* cursor up one line         (up arrow) */
    "\33\133\102",          /* down one line              (down arrow) */
    "\33\133\104",          /* left one character         (left arrow) */
    "\33\133\103",          /* right one character        (right arrow) */
    "\33\133\61\73\65\104", /* left one word              (ctrl-left arrow)*/
    "\33\133\61\73\65\103", /* right one word             (ctrl-right arrow) */
    "\33\133\61\73\65\110", /* home of document           (ctrl-home) */
    "\10",                  /* home of screen             (ctrl-h) */
    "\33\133\110",          /* home of line               (home) */
    "\33\133\61\73\65\106", /* end of document            (ctrl-end) */
    "\5",                   /* end of screen              (ctrl-e) */
    "\33\133\106",          /* end of line                (end) */
    "\33\133\65\73\65\176", /* scroll left one character  (ctrl-page up) */
    "\33\133\66\73\65\176", /* scroll right one character (ctrl-page down)  */
    "\33\133\61\73\65\102", /* scroll up one line         (ctrl-up arrow) */
    "\33\133\61\73\65\101", /* scroll down one line       (ctrl-down arrow) */
    "\33\133\66\176",       /* page down                  (page down) */
    "\33\133\65\176",       /* page up                    (page up) */
    "\11",                  /* tab                        (tab) */
    "\15",                  /* enter line                 (enter) */
    "\26",                  /* insert block               (ctrl-v) */
    "",                     /* insert line */
    "\33\133\62\176",       /* insert toggle              (insert) */
    "\33\133\63\73\62\176", /* delete block               (shift-del) */
    "\33\133\63\73\65\176", /* delete line                (ctrl-del) */
    "\33\133\63\176",       /* delete character forward   (del) */
    "\177",                 /* delete character backward  (backspace) */
    "\33\143",              /* copy block                 (alt-c) */
    "",                     /* copy line */
    "\33\33",               /* cancel current operation   (esc esc) */
    "\23",                  /* stop current operation     (ctrl-s) */
    "\21",                  /* continue current operation (ctrl-q) */
    "\20",                  /* print document             (ctrl-p) */
    "",                     /* print block */
    "",                     /* print screen */
    "",                     /* function key */
    "",                     /* display menu */
    "",                     /* mouse button assertion */
    "",                     /* mouse button deassertion */
    /* mouse move is just the leader for the mouse move/assert message. The
       characters are read in the input handler. */
#ifdef MOUSESGR
    "\33[<",                /* mouse move */
#else
    "\33[M",                /* mouse move */
#endif
    "",                     /* timer matures */
    "",                     /* joystick button assertion */
    "",                     /* joystick button deassertion */
    "",                     /* joystick move */
    "",                     /* window resize */
    "\3",                   /* terminate program           (ctrl-c) */
    /* we added the Fx key codes to the end here */
    "\33\117\120",          /* F1 */
    "\33\117\121",          /* F2 */
    "\33\117\122",          /* F3 */
    "\33\117\123",          /* F4 */
    "\33\133\61\65\176",    /* F5 */
    "\33\133\61\67\176",    /* F6 */
    "\33\133\61\70\176",    /* F7 */
    "\33\133\61\71\176",    /* F8 */
    "\33\133\62\60\176",    /* F9 */
    /* F12 is a "pseudo 10th" key in that I wanted to preserve the PA tradition
       of giving 10 function keys, so I reassigned the last one, since F10 is
       taken by xterm (I'm sure its a CUA thing) */
    "\33\133\62\64\176"     /* F12 */

};

#ifdef ALLOWUTF8
/*
 * bit count table for UTF-8
 */
unsigned char utf8bits[] = {

    0, /* 0000 */
    1, /* 0001 */
    1, /* 0010 */
    2, /* 0011 */
    1, /* 0100 */
    2, /* 0101 */
    2, /* 0110 */
    3, /* 0111 */
    1, /* 1000 */
    2, /* 1001 */
    2, /* 1010 */
    3, /* 1011 */
    2, /* 1100 */
    3, /* 1101 */
    3, /* 1110 */
    4  /* 1111 */

};
#endif

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

/*
 * Override vectors for calls in this package
 *
 */
static pa_cursor_t          cursor_vect;
static pa_maxx_t            maxx_vect;
static pa_maxy_t            maxy_vect;
static pa_home_t            home_vect;
static pa_del_t             del_vect;
static pa_up_t              up_vect;
static pa_down_t            down_vect;
static pa_left_t            left_vect;
static pa_right_t           right_vect;
static pa_blink_t           blink_vect;
static pa_reverse_t         reverse_vect;
static pa_underline_t       underline_vect;
static pa_superscript_t     superscript_vect;
static pa_subscript_t       subscript_vect;
static pa_italic_t          italic_vect;
static pa_bold_t            bold_vect;
static pa_strikeout_t       strikeout_vect;
static pa_standout_t        standout_vect;
static pa_fcolor_t          fcolor_vect;
static pa_bcolor_t          bcolor_vect;
static pa_auto_t            auto_vect;
static pa_curvis_t          curvis_vect;
static pa_scroll_t          scroll_vect;
static pa_curx_t            curx_vect;
static pa_cury_t            cury_vect;
static pa_curbnd_t          curbnd_vect;
static pa_select_t          select_vect;
static pa_event_t           event_vect;
static pa_timer_t           timer_vect;
static pa_killtimer_t       killtimer_vect;
static pa_mouse_t           mouse_vect;
static pa_mousebutton_t     mousebutton_vect;
static pa_joystick_t        joystick_vect;
static pa_joybutton_t       joybutton_vect;
static pa_joyaxis_t         joyaxis_vect;
static pa_settab_t          settab_vect;
static pa_restab_t          restab_vect;
static pa_clrtab_t          clrtab_vect;
static pa_funkey_t          funkey_vect;
static pa_frametimer_t      frametimer_vect;
static pa_autohold_t        autohold_vect;
static pa_wrtstr_t          wrtstr_vect;
static pa_wrtstrn_t         wrtstrn_vect;
static pa_eventover_t       eventover_vect;
static pa_eventsover_t      eventsover_vect;
static pa_sendevent_t       sendevent_vect;
static pa_title_t           title_vect;
static pa_openwin_t         openwin_vect;
static pa_buffer_t          buffer_vect;
static pa_sizbuf_t          sizbuf_vect;
static pa_getsiz_t          getsiz_vect;
static pa_setsiz_t          setsiz_vect;
static pa_setpos_t          setpos_vect;
static pa_scnsiz_t          scnsiz_vect;
static pa_scncen_t          scncen_vect;
static pa_winclient_t       winclient_vect;
static pa_front_t           front_vect;
static pa_back_t            back_vect;
static pa_frame_t           frame_vect;
static pa_sizable_t         sizable_vect;
static pa_sysbar_t          sysbar_vect;
static pa_menu_t            menu_vect;
static pa_menuena_t         menuena_vect;
static pa_menusel_t         menusel_vect;
static pa_stdmenu_t         stdmenu_vect;
static pa_getwinid_t        getwinid_vect;
static pa_focus_t           focus_vect;

/**
 * Save for terminal status
 */
static struct termios trmsav;

/**
 * Active timers table
 */
static int timtbl[PA_MAXTIM];

static scnrec* screens[MAXCON];         /* screen contexts array */ 
static int curdsp;                      /* index for current display screen */ 
static int curupd;                      /* index for current update screen */ 
static pa_pevthan evthan[pa_etframe+1]; /* array of event handler routines */ 
static pa_pevthan evtshan;              /* single master event handler routine */

/*
 * Input event parsing
 */

/*
 * Key matching input buffer.
 *
 * Note mouse also comes in as input keys.
 */
static pthread_mutex_t evtlock; /* lock for event tracking */
static unsigned char keybuf[MAXKEY]; /* buffer */
static int    keylen;      /* number of characters in buffer */
/* current tracking states of mouse */
static int    button1;     /* button 1 state: 0=assert, 1=deassert */
static int    button2;     /* button 2 state: 0=assert, 1=deassert */
static int    button3;     /* button 3 state: 0=assert, 1=deassert */
static int    mpx;         /* mouse x/y current position */
static int    mpy;
/* new, incoming states of mouse */
static int    nbutton1;    /* button 1 state: 0=assert, 1=deassert */
static int    nbutton2;    /* button 2 state: 0=assert, 1=deassert */
static int    nbutton3;    /* button 3 state: 0=assert, 1=deassert */
static int    nmpx;        /* mouse x/y current position */
static int    nmpy;
/* end evtlock area */

static int*   tabs;        /* tabs set */
static int    dimx;        /* actual width of screen */
static int    dimy;        /* actual height of screen */
static int    bufx, bufy;  /* buffer size */
static int    curon;       /* current on/off state of cursor */
static int    curx;        /* cursor position on screen */
static int    cury;
static int    ncurx;       /* new cursor position on screen */
static int    ncury;
static int    curval;      /* physical cursor position valid */
static int    curvis;      /* current status of cursor visible */
static pa_color forec;     /* current writing foreground color */
static pa_color backc;     /* current writing background color */
static scnatt   attr;      /* current writing attribute */
/* global scroll enable. This does not reflect the physical state, we never
   turn on automatic scroll. */
static int    scroll;

/* maximum power of 10 in integer */
static int    maxpow10;
static int    numjoy;         /* number of joysticks found */
static int    joyenb;         /* enable joysticks */
static int    frmfid;         /* framing timer fid */
char          inpbuf[MAXLIN]; /* input line buffer */
int           inpptr;         /* input line index */
static joyptr joytab[MAXJOY]; /* joystick control table */
static int    dmpevt;         /* enable dump Petit-Ami messages */
static int    inpsev;         /* keyboard input system event number */
static int    frmsev;         /* frame timer system event number */
static int    winchsev;       /* windows change system event number */
#ifdef ALLOWUTF8
static int    utf8cnt;        /* UTF-8 extended character count */
#endif

static paevtque*       paqfre;      /* free PA event queue entries list */
static paevtque*       paqevt;      /* PA event input save queue */
static pthread_t       eventthread; /* thread handle for event thread */
static pthread_mutex_t evtlck;      /* event queue lock */
static pthread_cond_t  evtquene;    /* semaphore: event queue not empty */
static int             evtquecnt;   /* number of entries in event queue */
static int             evtquemax;   /* high water mark for event queue */
static int             matrem;      /* matching entries removed */

/** ****************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.

*******************************************************************************/

static void error(errcod e)

{

    fprintf(stderr, "*** Error: AnsiTerm: ");
    switch (e) { /* error */

        case eftbful: fprintf(stderr, "Too many files"); break;
        case ejoyacc: fprintf(stderr, "No joystick access available"); break;
        case etimacc: fprintf(stderr, "No timer access available"); break;
        case efilopr: fprintf(stderr, "Cannot perform operation on special file");
                      break;
        case einvpos: fprintf(stderr, "Invalid screen position"); break;
        case efilzer: fprintf(stderr, "Filename is empty"); break;
        case einvscn: fprintf(stderr, "Invalid screen number"); break;
        case einvhan: fprintf(stderr, "Invalid file handle"); break;
        case emouacc: fprintf(stderr, "No mouse access available"); break;
        case eoutdev: fprintf(stderr, "Error in output device"); break;
        case einpdev: fprintf(stderr, "Error in input device"); break;
        case einvtab: fprintf(stderr, "Invalid tab stop position"); break;
        case einvjoy: fprintf(stderr, "Invalid joystick ID"); break;
        case esendevent_unimp: fprintf(stderr, "sendevent unimplemented"); break;
        case etitle_unimp:     fprintf(stderr, "title unimplemented"); break;
        case eopenwin_unimp:   fprintf(stderr, "openwin unimplemented"); break;
        case ebuffer_unimp:    fprintf(stderr, "buffer unimplemented"); break;
        case esizbuf_unimp:    fprintf(stderr, "sizbuf unimplemented"); break;
        case egetsiz_unimp:    fprintf(stderr, "getsiz unimplemented"); break;
        case esetsiz_unimp:    fprintf(stderr, "setsiz unimplemented"); break;
        case esetpos_unimp:    fprintf(stderr, "setpos unimplemented"); break;
        case escnsiz_unimp:    fprintf(stderr, "scnsiz unimplemented"); break;
        case escncen_unimp:    fprintf(stderr, "scncen unimplemented"); break;
        case ewinclient_unimp: fprintf(stderr, "winclient unimplemented"); break;
        case efront_unimp:     fprintf(stderr, "front unimplemented"); break;
        case eback_unimp:      fprintf(stderr, "back unimplemented"); break;
        case eframe_unimp:     fprintf(stderr, "frame unimplemented"); break;
        case esizable_unimp:   fprintf(stderr, "sizable unimplemented"); break;
        case esysbar_unimp:    fprintf(stderr, "sysbar unimplemented"); break;
        case emenu_unimp:      fprintf(stderr, "menu unimplemented"); break;
        case emenuena_unimp:   fprintf(stderr, "menuena unimplemented"); break;
        case emenusel_unimp:   fprintf(stderr, "menusel unimplemented"); break;
        case estdmenu_unimp:   fprintf(stderr, "stdmenu unimplemented"); break;
        case egetwinid_unimp:  fprintf(stderr, "getwinid unimplemented"); break;
        case efocus_unimp:     fprintf(stderr, "focus unimplemented"); break;
        case esystem: fprintf(stderr, "System fault"); break;

    }
    fprintf(stderr, "\n");

    exit(1);

}

/** ****************************************************************************

Print Linux error

Accepts a linux error code. Prints the error string and exits.

*******************************************************************************/

void linuxerror(int ec)

{

    fprintf(stderr, "Linux error: %s\n", strerror(ec)); fflush(stderr);

    exit(1);

}

/******************************************************************************

Print event symbol

A diagnostic, print the given event code as a symbol to the error file.

******************************************************************************/

void prtevtt(pa_evtcod e)

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
        case pa_etframe:   fprintf(stderr, "etframe"); break;

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

    fprintf(stderr, "PA Event: ");
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
        case pa_etfun: fprintf(stderr, ": key: %d", er->fkey); break;
        default: ;

    }

}

/*******************************************************************************

Get size

Finds the x-y window size from the input device. Note that if this is not
sucessful, the size remains unchanged.

*******************************************************************************/

void findsize(void)

{

    /** window size record */          struct winsize ws;
    /** result code */                 int r;

    /* attempt to determine actual screen size */
    r = ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
    if (!r) {

        dimx = ws.ws_col;
        dimy = ws.ws_row;

    }

}

/** ****************************************************************************

Read character from input file

Reads a single character to from the input file. Used to read from the input file
directly.

On the input file, we can't use the override, because the select() call bypasses
it on input, and so we must as well.

*******************************************************************************/

static char getchr(void)

{

    char    c;  /* character read */
    ssize_t rc; /* return code */

    /* receive character to the next hander in the override chain */
    rc = (*ofpread)(INPFIL, &c, 1);
    // rc = read(INPFIL, &c, 1);
    if (rc != 1) error(einpdev); /* input device error */

    return c; /* return character */

}

/** ****************************************************************************

Write character to output file

Writes a single character to the output file. Used to write to the output file
directly.

Uses the write() override.

*******************************************************************************/

static void putchr(unsigned char c)

{

    ssize_t rc; /* return code */

    /* send character to the next hander in the override chain */
    rc = (*ofpwrite)(OUTFIL, &c, 1);

    if (rc != 1) error(eoutdev); /* output device error */

}

/** ****************************************************************************

Write string to output file

Writes a string directly to the output file.

*******************************************************************************/

static void putstr(unsigned char *s)


{

    /** index for string */ int i;

    while (*s) putchr(*s++); /* output characters */

}

/** ****************************************************************************

Write n length string to output file

Writes a string directly to the output file of n length.

*******************************************************************************/

static void putnstr(unsigned char *s, int n)


{

    /** index for string */ int i;

    while (*s && n--) putchr(*s++); /* output characters */

}

/** ****************************************************************************

Write integer to output file

Writes a simple unsigned integer to the output file.

*******************************************************************************/

static void wrtint(int i)

{

    /* power holder */       int  p;
    /* digit holder */       char digit;
    /* leading digit flag */ int  leading;

    p = maxpow10; /* set maximum power of 10 */
    leading = 0; /* set no leading digit encountered */
    while (p != 0) { /* output digits */

        digit = i/p%10+'0'; /* get next digit */
        p = p/10; /* next digit */
        if (digit != '0' || p == 0) /* leading digit or beginning of number */
            leading = 1; /* set leading digit found */
        if (leading) putchr(digit); /* output digit */

    }

}

/** ****************************************************************************

Get freed/new PA queue entry

Either gets a new entry from malloc or returns a previously freed entry.

Note: This routine should be called within the event queue lock.

*******************************************************************************/

static paevtque* getpaevt(void)

{

    paevtque* p;

    if (paqfre) { /* there is a freed entry */

        p = paqfre; /* index top entry */
        paqfre = p->next; /* gap from list */

    } else p = (paevtque*)malloc(sizeof(paevtque));

    return (p);

}

/** ****************************************************************************

Get freed/new PA queue entry

Either gets a new entry from malloc or returns a previously freed entry.

Note: This routine should be called within the event queue lock.

*******************************************************************************/

static void putpaevt(paevtque* p)

{

    p->next = paqfre; /* push to list */
    paqfre = p;

}

/** ****************************************************************************

Print contents of PA queue

A diagnostic, prints the contents of the PA queue.

*******************************************************************************/

static void prtquepaevt(void)

{

    paevtque* p;
    int       r;

    r = pthread_mutex_lock(&evtlck); /* lock event queue */
    if (r) linuxerror(r);
    p = paqevt; /* index root entry */
    while (p) {

        prtevt(&p->evt); /* print this entry */
        fprintf(stderr, "\n"); fflush(stderr);
        p = p->next; /* link next */
        if (p == paqevt) p = NULL; /* end of queue, terminate */

    }
    r = pthread_mutex_unlock(&evtlck); /* release event queue */
    if (r) linuxerror(r);

}

/** ****************************************************************************

Remove queue duplicates

Removes any entries in the current queue that would be made redundant by the new
queue entry. Right now this consists only of mouse movements.

*******************************************************************************/

void remdupque(pa_evtrec* e)

{

    paevtque* p;

    if (paqevt) { /* the queue has content */

        p = paqevt; /* index first queue entry */
        do { /* run around the bubble */

            if (e->etype == pa_etmoumov && p->evt.etype == pa_etmoumov &&
                e->mmoun == p->evt.mmoun) {

                /* matching entry, remove */
                matrem++; /* count */
                if (p->next == p) {

                    paqevt = NULL; /* only one entry, clear list */
                    p = NULL; /* set no next entry */

                } else { /* other entries */

                    p->last->next = p->next; /* point last at current */
                    p->next->last = p->last; /* point current at last */
                    p = p->next; /* go next entry */

                }

            } else p = p->next; /* go next queue entry */

        } while (p && p->next != paqevt); /* not back at beginning */

    }

}

/** ****************************************************************************

Place PA event into input queue

*******************************************************************************/

static void enquepaevt(pa_evtrec* e)

{

    paevtque* p;
    int       r;

    r = pthread_mutex_lock(&evtlck); /* lock event queue */
    if (r) linuxerror(r);
    remdupque(e); /* remove any duplicates */
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
        r = pthread_cond_signal(&evtquene);
        if (r) linuxerror(r);

    }
    evtquecnt++; /* count entries in queue */
    /* set new high water mark */
    if (evtquecnt > evtquemax) evtquemax = evtquecnt;
    r = pthread_mutex_unlock(&evtlck); /* release event queue */
    if (r) linuxerror(r);

}

/** ****************************************************************************

Remove PA event from input queue

*******************************************************************************/

static void dequepaevt(pa_evtrec* e)

{

    paevtque* p;
    int       r;

    r = pthread_mutex_lock(&evtlck); /* lock event queue */
    if (r) linuxerror(r);
    /* if queue is empty, wait for not empty event */
    while (!paqevt) {

        r = pthread_cond_wait(&evtquene, &evtlck);
        if (r) linuxerror(r);

    }
    /* we push TO next (current) and take FROM last (final) */
    p = paqevt->last; /* index final entry */
    if (p->next == p) paqevt = NULL; /* only one entry, clear list */
    else { /* other entries */

        p->last->next = p->next; /* point last at current */
        p->next->last = p->last; /* point current at last */

    }
    memcpy(e, &p->evt, sizeof(pa_evtrec)); /* copy out to caller */
    putpaevt(p); /* release queue entry to free */
    evtquecnt--; /* count entries in queue */
    r = pthread_mutex_unlock(&evtlck); /* release event queue */
    if (r) linuxerror(r);

}


/******************************************************************************

Translate colors code rgb

Translates an independent to primary RGB color codes.

******************************************************************************/

void colnumrgb(pa_color c, int* r, int* g, int* b)

{

    int n;

    /* translate color number */
    switch (c) { /* color */

        case pa_black:     *r = 0x00; *g = 0x00; *b = 0x00; break;
        case pa_white:     *r = 0xff; *g = 0xff; *b = 0xff; break;
        case pa_red:       *r = 0xff; *g = 0x00; *b = 0x00; break;
        case pa_green:     *r = 0x00; *g = 0xff; *b = 0x00; break;
        case pa_blue:      *r = 0x00; *g = 0x00; *b = 0xff; break;
        case pa_cyan:      *r = 0x00; *g = 0xff; *b = 0xff; break;
        case pa_yellow:    *r = 0xff; *g = 0xff; *b = 0x00; break;
        case pa_magenta:   *r = 0xff; *g = 0x00; *b = 0xff; break;

    }

}

/** ****************************************************************************

Basic terminal controls

These routines control the basic terminal functions. They exist just to
encapsulate this information. All of these functions are specific to ANSI
compliant terminals.

ANSI is able to set more than one attribute at a time, but under windows 95
there are no two attributes that you can detect together ! This is because
win95 modifies the attributes quite a bit (there is no blink). This capability
can be replaced later if needed.

Other notes:

1. Underline only works on monochrome terminals. On color, it makes
the text turn blue.

2. On linux, gnome-terminal and xterm both do not also home the cursor on
a clear (as the ANSI spec says). We fake this by adding a specific cursor home.

*******************************************************************************/

/** clear screen and home cursor */
static void trm_clear(void) { putstr("\33[2J\33[H"); }
/** home cursor */ static void trm_home(void) { putstr("\33[H"); }
/** move cursor up */ static void trm_up(void) { putstr("\33[A"); }
/** move cursor down */ static void trm_down(void) { putstr("\33[B"); }
/** move cursor left */ static void trm_left(void) { putstr("\33[D"); }
/** move cursor right */ static void trm_right(void) { putstr("\33[C"); }
/** turn on blink attribute */ static void trm_blink(void) { putstr("\33[5m"); }
/** turn on reverse video */ static void trm_rev(void) { putstr("\33[7m"); }
/** turn on underline */ static void trm_undl(void) { putstr("\33[4m"); }
/** turn on bold attribute */ static void trm_bold(void) { putstr("\33[1m"); }
/** turn on italic attribute */ static void trm_ital(void) { putstr("\33[3m"); }
/** turn off all attributes */
static void trm_attroff(void) { putstr("\33[0m"); }
/** turn on cursor wrap */ static void trm_wrapon(void) { putstr("\33[7h"); }
/** turn off cursor wrap */ static void trm_wrapoff(void) { putstr("\33[7l"); }
/** turn off cursor */ static void trm_curoff(void) { putstr("\33[?25l"); }
/** turn on cursor */ static void trm_curon(void) { putstr("\33[?25h"); }

/** set foreground color */
static void trm_fcolor(pa_color c)

{

#ifdef COLOR24
    int r, g, b;

    colnumrgb(c, &r, &g, &b); /* get rgb equivalent color */
    putstr("\33[38;2;");
    wrtint(r);
    putstr(";");
    wrtint(g);
    putstr(";");
    wrtint(b);
    putstr("m");
#else
    putstr("\33[");
    /* override "bright" black, which is more like grey */
    if (c == pa_black) wrtint(ANSIFORECOLORBASE+colnum(c));
    else wrtint(FORECOLORBASE+colnum(c));
    putstr("m");
#endif

}

/** set background color */
static void trm_bcolor(pa_color c)

{

#ifdef COLOR24
    int r, g, b;

    colnumrgb(c, &r, &g, &b); /* get rgb equivalent color */
    putstr("\33[48;2;");
    wrtint(r);
    putstr(";");
    wrtint(g);
    putstr(";");
    wrtint(b);
    putstr("m");
#else
    putstr("\33[");
    /* override "bright" black, which is more like grey */
    if (c == pa_black) wrtint(ANSIBACKCOLORBASE+colnum(c));
    else wrtint(BACKCOLORBASE+colnum(c));
    putstr("m");
#endif

}

/** position cursor */
static void trm_cursor(int x, int y)

{

    putstr("\33[");
    wrtint(y);
    putstr(";");
    wrtint(x);
    putstr("H");

}

/** ****************************************************************************

Check in display

Check that the given screen context is currently being displayed.

*******************************************************************************/

int indisp(scnptr sc)

{

    return sc == screens[curdsp-1];

}

/** ****************************************************************************

Set attribute from attribute code

Accepts a "universal" attribute code, and executes the attribute set required
to make that happen on screen. A few of these don't work on ANSI terminals,
including superscript and subscript.

*******************************************************************************/

static void setattr(scnptr sc, scnatt a)

{

    if (indisp(sc)) { /* in display */

        switch (a) { /* attribute */

            case sanone:  trm_attroff(); break; /* no attribute */
            case sablink: trm_blink();   break; /* blinking text (foreground) */
            case sarev:   trm_rev();     break; /* reverse video */
            case saundl:  trm_undl();    break; /* underline */
            case sasuper: break;                /* superscript */
            case sasubs:  break;                /* subscripting */
            case saital:  trm_ital();    break; /* italic text */
            case sabold:  trm_bold();    break; /* bold text */

        }
        /* attribute off may change the colors back to "normal" (normal for that
           particular implementation), apparently to remove reverse video. So we
           need to restore colors in this case, since PA/TK preserves colors. */
        if (a == sanone) {

            trm_fcolor(forec); /* set current colors */
            trm_bcolor(backc);

        }

    }

}


/*******************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns true if so.

*******************************************************************************/

int icurbnd(scnptr sc)

{

   return (ncurx >= 1) && (ncurx <= dimx) && (ncury >= 1) && (ncury <= dimy);

}

/*******************************************************************************

Set cursor status

Sets the cursor visible or invisible. If the cursor is out of bounds, it is
invisible regardless. Otherwise, it is visible according to the state of the
current buffer's visible status.

Should suppress redundant visibility sets here.

*******************************************************************************/

void cursts(scnptr sc)

{

    int cv;

    if (indisp(sc)) { /* in display */

        cv = curvis; /* set current buffer status */
        if (!icurbnd(sc)) cv = 0; /* not in bounds, force off */
        if (cv != curon) { /* not already at the desired state */

            if (cv) {

                trm_curon();
                curon = TRUE;

            } else {

                trm_curoff();
                curon = FALSE;

            }

        }

    }

}

/*******************************************************************************

Position cursor

Positions the cursor (caret) image to the right location on screen, and handles
the visible or invisible status of that. We consider the current position and
visible/invisible status, and try to output only the minimum terminal controls
to bring the old state of the display to the same state as the new display.

*******************************************************************************/

void setcur(scnptr sc)

{

    if (indisp(sc)) { /* in display */

        /* check cursor in bounds */
        if (icurbnd(sc)) {

            /* set cursor position */
            if ((ncurx != curx || ncury != cury) && curval) {

                /* Cursor position and actual don't match. Try some optimized
                   cursor positions to reduce bandwidth. Note we don't count on
                   real terminal behavior at the borders. */
                if (ncurx == 1 && ncury == 1) trm_home();
                else if (ncurx == curx && ncury == cury-1) trm_up();
                else if (ncurx == curx && ncury == cury+1) trm_down();
                else if (ncurx == curx-1 && ncury == cury) trm_left();
                else if (ncurx == curx+1 && ncury == cury) trm_right();
                else if (ncurx == 1 && ncury == cury) putchr('\r');
                else trm_cursor(ncurx, ncury);
                curx = ncurx;
                cury = ncury;
                curval = 1;

            } else {

                /* don't count on physical cursor location, just reset */
                trm_cursor(ncurx, ncury);
                curx = ncurx;
                cury = ncury;
                curval = 1;

            }

        }
        cursts(sc); /* set new cursor status */

    }

}

/** ****************************************************************************

Restore screen

Updates all the buffer and screen parameters to the terminal. We specifically
write each location. A clear would be faster, but would flash.

*******************************************************************************/

static void restore(scnptr sc)

{

    /** screen indexes */         int xi, yi;
    /** color saves */            pa_color fs, bs;
    /** attribute saves */        scnatt as;
    /** screen element pointer */ scnrec *p;
    /** clipped buffer sizes */   int cbufx, cbufy;

    trm_home(); /* restore cursor to upper left to start */
    /* set colors and attributes */
    trm_fcolor(forec); /* restore colors */
    trm_bcolor(backc);
    setattr(sc, attr); /* restore attributes */
    fs = forec; /* save current colors and attributes */
    bs = backc;
    as = attr;
    /* find buffer sizes clipped by onscreen image */
    cbufx = bufx;
    cbufy = bufy;
    if (cbufx > dimx) cbufx = dimx;
    if (cbufy > dimy) cbufy = dimy;
    /* copy buffer to screen */
    for (yi = 1; yi <= cbufy; yi++) { /* lines */

        for (xi = 1; xi <= cbufx; xi++) { /* characters */

            /* for each new character, we compare the attributes and colors
               with what is set. if a new color or attribute is called for,
               we set that, and update the saves. this technique cuts down on
               the amount of output characters */
            p = &SCNBUF(sc, xi, yi); /* index this screen element */
            if (p->forec != fs) { /* new foreground color */

                trm_fcolor(p->forec); /* set the new color */
                fs = p->forec; /* set save */

            };
            if (p->backc != bs) { /* new foreground color */

                trm_bcolor(p->backc); /* set the new color */
                bs = p->backc; /* set save */

            };
            if (p->attr != as) { /* new attribute */

                setattr(sc, p->attr); /* set the new attribute */
                as = p->attr; /* set save */

            };
#ifdef ALLOWUTF8
            putnstr(p->ch, 4); /* now output the actual character */
#else
            putchr(p->ch); /* now output the actual character */
#endif

        };
        if (yi < cbufy)
            /* output next line sequence on all lines but the last. this is
               because the last one would cause us to scroll */
            putstr("\r\n");

    };
    /* color backgrounds outside of buffer */
    if (dimx > bufx) {

        /* space to right */
        trm_bcolor(backc); /* set background color */
        for (yi = 1; yi <= bufy; yi++) {

            trm_cursor(bufx+1, yi); /* locate to line start */
            for (xi = bufx+1; xi <= dimx; xi++) putchr(' '); /* fill */

        }

    }
    if (dimy > bufy) {

        /* space to bottom, we color right bottom here because it is easier */
        trm_bcolor(backc); /* set background color */
        for (yi = bufy+1; yi <= dimy; yi++) {

            trm_cursor(1, yi); /* locate to line start */
            for (xi = 1; xi <= dimx; xi++) putchr(' '); /* fill */

        }

    }
    /* restore cursor position */
    trm_cursor(ncurx, ncury);
    curx = ncurx; /* set physical cursor */
    cury = ncury;
    curval = 1; /* set it is valid */
    trm_fcolor(forec); /* restore colors */
    trm_bcolor(backc);
    setattr(sc, attr); /* restore attributes */
    setcur(sc); /* set cursor status */

}

/** *****************************************************************************

Get keyboard code control match or other event

Performs a successive match to keyboard input. A keyboard character is read,
and matched against the keyboard equivalence table. If we find a match, we keep
reading in characters until we get a single unambiguous matching entry.

If the match never results in a full match, the buffered characters are simply
discarded, and matching goes on with the next input character. Such "stillborn"
matches are either the result of ill considered input key equivalences, or of
a user typing in keys manually that happen to evaluate to special keys.

*******************************************************************************/

/* get and process a joystick event */
static void joyevt(pa_evtrec* er, int* keep, joyptr jp)

{

#ifndef __MACH__ /* Mac OS X */
    struct js_event ev;

    read(jp->fid, &ev, sizeof(ev)); /* get next joystick event */
    if (!(ev.type & JS_EVENT_INIT)) {

        if (ev.type & JS_EVENT_BUTTON) {

            /* we use Linux button numbering, because, what the heck */
            if (ev.value) { /* assert */

                er->etype = pa_etjoyba; /* set assert */
                er->ajoyn = jp->no; /* set joystick 1 */
                er->ajoybn = ev.number+1; /* set button number */

            } else { /* deassert */

                er->etype = pa_etjoybd; /* set assert */
                er->djoyn = jp->no; /* set joystick 1 */
                er->djoybn = ev.number+1; /* set button number */

            }
            *keep = TRUE; /* set keep event */

        }
        if (ev.type & JS_EVENT_AXIS) {

            /* update the axies */
            if (ev.number == 0) jp->ax = ev.value*(INT_MAX/32768);
            else if (ev.number == 1) jp->ay = ev.value*(INT_MAX/32768);
            else if (ev.number == 2) jp->az = ev.value*(INT_MAX/32768);
            else if (ev.number == 3) jp->a4 = ev.value*(INT_MAX/32768);
            else if (ev.number == 4) jp->a5 = ev.value*(INT_MAX/32768);
            else if (ev.number == 5) jp->a6 = ev.value*(INT_MAX/32768);

            /* we support up to 6 axes on a joystick. After 6, they get thrown
               out, leaving just the buttons to respond */
            if (ev.number < 6) {

                er->etype = pa_etjoymov; /* set joystick move */
                er->mjoyn = jp->no; /* set joystick number */
                er->joypx = jp->ax; /* place joystick axies */
                er->joypy = jp->ay;
                er->joypz = jp->az;
                er->joyp4 = jp->a4;
                er->joyp5 = jp->a5;
                er->joyp6 = jp->a6;
                *keep = TRUE; /* set keep event */

            }

        }

    }
#endif

}

static void ievent(pa_evtrec* ev)

{

    int       pmatch; /* partial match found */
    pa_evtcod i;      /* index for events */
    int       rv;     /* return value */
    int       evtfnd; /* found an event */
    int       evtsig; /* event signaled */
    int       ti;     /* index for timers */
    int       ji;     /* index for joysticks */
    enum { mnone, mbutton, mx, my } mousts; /* mouse state variable */
    int       dimxs;  /* save for screen size */
    int       dimys;
    sysevt    sev;    /* system event */
    joyptr    jp;
    int       bn;     /* mouse button number */
    int       ba;     /* mouse button assert */

    mousts = mnone; /* set no mouse event being processed */
    do { /* match input events */

        evtfnd = 0; /* set no event found */
        evtsig = 0; /* set no event signaled */
        system_event_getsevt(&sev); /* get the next system event */
        /* check the read file has signaled */
        if (sev.typ == se_inp && sev.lse == inpsev) {

            /* keyboard (standard input) */
            evtsig = 1; /* event signaled */
            keybuf[keylen++] = getchr(); /* get next character to match buffer */
            if (mousts == mnone) { /* do table matching */

                pmatch = 0; /* set no partial matches */
                for (i = pa_etchar; i <= pa_etterm+MAXFKEY && !evtfnd; i++)
                    if (!strncmp(keybuf, keytab[i], keylen)) {

                    pmatch = 1; /* set partial match */
                    /* set if the match is whole key */
                    if (strlen(keytab[i]) == keylen) {

                        if (i == pa_etmoumov)
                            /* mouse move leader, start state machine */
                            mousts = mbutton; /* set next is button state */
                        else {

                            /* complete match found, set as event */
                            if (i > pa_etterm) { /* it's a function key */

                                ev->etype = pa_etfun;
                                /* compensate for F12 subsitution */
                                if (i == pa_etterm+MAXFKEY) ev->fkey = 10;
                                else ev->fkey = i-pa_etterm;

                            } else ev->etype = i; /* set event */
                            evtfnd = 1; /* set event found */
                            keylen = 0; /* clear buffer */
                            pmatch = 0; /* clear partial match */

                        }

                    }

                }
                if (!pmatch) {

                    /* if no partial match and there are characters in buffer, something
                       went wrong, or there never was at match at all. For such
                       "stillborn" matches we start over */
                    if (keylen > 1) keylen = 0;
                    else if (keylen == 1) { /* have valid character */

                        ev->etype = pa_etchar; /* set event */
                        ev->echar = keybuf[0]; /* place character */
                        evtfnd = 1; /* set event found */
                        keylen = 0; /* clear buffer */

                    }

                }

            } else { /* parse mouse components */

#ifdef MOUSESGR
                /* SGR is variable length */
                if (keybuf[keylen-1] == 'm' || keybuf[keylen-1] == 'M') {

                    /* mouse message is complete, parse */
                    ba = keybuf[keylen-1] == 'm'; /* set assert */
                    keylen = 3; /* set start of sequence in buffer */
                    bn = 0; /* clear button number */
                    while (keybuf[keylen] >= '0' && keybuf[keylen] <= '9')
                        bn = bn*10+keybuf[keylen++]-'0';
                    if (keybuf[keylen] == ';') keylen++;
                    nmpx = 0; /* clear x */
                    while (keybuf[keylen] >= '0' && keybuf[keylen] <= '9')
                        nmpx = nmpx*10+keybuf[keylen++]-'0';
                    if (keybuf[keylen] == ';') keylen++;
                    nmpy = 0; /* clear y */
                    while (keybuf[keylen] >= '0' && keybuf[keylen] <= '9')
                        nmpy = nmpy*10+keybuf[keylen++]-'0';
                    if (keybuf[keylen] == 'm' || keybuf[keylen] == 'M') {

                        /* mouse sequence is correct, process */
                        switch (bn) { /* decode button */

                            case 0: nbutton1 = ba; break; /* assert button 1 */
                            case 1: nbutton2 = ba; break; /* assert button 2 */
                            case 2: nbutton3 = ba; break; /* assert button 3 */
                            default: break; /* deassert all, do nothing */

                        }
                        keylen = 0; /* clear key buffer */
                        mousts = mnone; /* reset mouse aquire */

                    }

                }
#else
                /* standard mouse encode */
                if (mousts < my) mousts++;
                else { /* mouse has matured */

                    /* the mouse event state is laid out in the buffer, we will
                       decompose it into a new mouse status */
                    nbutton1 = 1;
                    nbutton2 = 1;
                    nbutton3 = 1;
                    switch (keybuf[3] & 0x3) {

                        case 0: nbutton1 = 0; break; /* assert button 1 */
                        case 1: nbutton2 = 0; break; /* assert button 2 */
                        case 2: nbutton3 = 0; break; /* assert button 3 */
                        case 3: break; /* deassert all, do nothing */

                    }
                    /* set new mouse position */
                    nmpx = keybuf[4]-33+1;
                    nmpy = keybuf[5]-33+1;
                    keylen = 0; /* clear key buffer */
                    mousts = mnone; /* reset mouse aquire */

                }
#endif

            }

        } else if (sev.typ == se_tim) {

            /* look in timer set */
            for (ti = 0; ti < PA_MAXTIM && !evtfnd; ti++) {

                if (timtbl[ti] == sev.lse) {

                    /* timer found, set as event */
                    evtsig = 1; /* set event signaled */
                    ev->etype = pa_ettim; /* set timer type */
                    ev->timnum = ti+1; /* set timer number */
                    evtfnd = 1; /* set event found */

                }

            }
            /* could also be the frame timer */
            if (!evtfnd && sev.lse == frmsev) {

                evtsig = 1; /* set event signaled */
                ev->etype = pa_etframe; /* set frame event occurred */
                evtfnd = TRUE; /* set event found */

            }

        } else if (sev.typ == se_inp && !evtfnd && joyenb) {

            /* look in joystick set */
            for (ji = 0; ji < numjoy && !evtfnd; ji++) {

                if (joytab[ji] && joytab[ji]->sid == sev.lse) {

                    evtsig = 1; /* set event signaled */
                    joyevt(ev, &evtfnd, joytab[ji]); /* process joystick */

                }

            }

        } else if (sev.typ == se_sig && !evtfnd && sev.lse == winchsev) {

            /* save current window size */
            dimxs = dimx;
            dimys = dimy;
            /* recalculate window size */
            findsize();
            /* linux/xterm has an oddity here, if the winch contracts in y, it
               occasionally relocates the buffer contents up. This means we
               always need to refresh, and means it can flash. */
            restore(screens[curdsp-1]);
            ev->etype = pa_etresize;
            ev->rszx = dimx; /* send new size in message */
            ev->rszy = dimy;
            evtfnd = 1;

        }

        if (!evtfnd) {

            /* check any mouse states have changed, flag and remove */
            if (nbutton1 < button1) {

                ev->etype = pa_etmouba;
                ev->amoun = 1;
                ev->amoubn = 1;
                evtfnd = 1;
                button1 = nbutton1;

            } else if (nbutton1 > button1) {

                ev->etype = pa_etmoubd;
                ev->dmoun = 1;
                ev->dmoubn = 1;
                evtfnd = 1;
                button1 = nbutton1;

            } else if (nbutton2 < button2) {

                ev->etype = pa_etmouba;
                ev->amoun = 1;
                ev->amoubn = 2;
                evtfnd = 1;
                button2 = nbutton2;

            } else if (nbutton2 > button2) {

                ev->etype = pa_etmoubd;
                ev->dmoun = 1;
                ev->dmoubn = 2;
                evtfnd = 1;
                button2 = nbutton2;

            } else if (nbutton3 < button3) {

                ev->etype = pa_etmouba;
                ev->amoun = 1;
                ev->amoubn = 3;
                evtfnd = 1;
                button3 = nbutton3;

            } else if (nbutton3 > button3) {

                ev->etype = pa_etmoubd;
                ev->dmoun = 1;
                ev->dmoubn = 3;
                evtfnd = 1;
                button3 = nbutton3;

            } if (nmpx != mpx || nmpy != mpy) {

                ev->etype = pa_etmoumov;
                ev->mmoun = 1;
                ev->moupx = nmpx;
                ev->moupy = nmpy;
                evtfnd = 1;
                mpx = nmpx;
                mpy = nmpy;

            }

        }

    /* while substring match and no other event found, or buffer empty */
    } while (!evtfnd);

}

/** ****************************************************************************

Event input thread

This thread runs continuously and gets events from the lower level, then spools
them into the input queue. This allows the input queue to run ahead of the
client program.

*******************************************************************************/

static void* eventtask(void* param)

{

    pa_evtrec er; /* event record */

    while (1) { /* run continuously */

        ievent(&er); /* get next event */
        enquepaevt(&er); /* send to queue */

    }

    return (NULL);

}

/** *****************************************************************************

Translate colors code

Translates an independent to a terminal specific primary color code for an
ANSI compliant terminal..

*******************************************************************************/

static int colnum(pa_color c)

{

    /** numeric equivalent of color */ int n;

    /* translate color number */
    switch (c) { /* color */

        case pa_black:   n = 0;  break;
        case pa_white:   n = 7; break;
        case pa_red:     n = 1; break;
        case pa_green:   n = 2; break;
        case pa_blue:    n = 4; break;
        case pa_cyan:    n = 6; break;
        case pa_yellow:  n = 3; break;
        case pa_magenta: n = 5; break;

    }

    return n; /* return number */

}

/** ****************************************************************************

Place character in screen buffer

Places the next character or extension in the given screen buffer location.
Handles either ISO 8859 characters or UTF-8 characters.

For UTF-8, there are a few errors possible. Here is how they are handled:

1. Too many extension (10xxxxxx) characters. Overflowing 4 places will cause
the sequence to be reset to 0 and thus cleared.

2. Too many extension (10xxxxxx) characters for format. This happens if the
first or count character indicates fewer than the number of extension characters
received. The sequence is cleared and reset.

3. An extension (10xxxxxx) character received as the first character. The
sequence is cleared.

*******************************************************************************/

static void plcchrext(scnrec* p, unsigned char c)

{

#ifdef ALLOWUTF8
    int ci;

    if (c < 0x80 || c >= 0xc0) { /* normal ASCII or start UTF-8 character */

        /* start of character sequence, clear whole sequence */
        for (ci = 0; ci < 4; ci++) p->ch[ci] = 0;
        p->ch[0] = c; /* place start character */

    } else if ( c & 0xc0 == 0x80) { /* extension character */

        if (p->ch[0] == 0) { /* extension received as first character */

            for (ci = 0; ci < 4; ci++) p->ch[ci] = 0;

        } else {

            /* follow on character */
            ci = 0;
            while (ci < 4 && p->ch[ci]) ci++;
            /* overflow, clear out */
            if (ci >= 4) {

                for (ci = 0; ci < 4; ci++) p->ch[ci] = 0;

            } else {

                /* more extension characters than count char */
                if (ci > utf8bits[p->ch[0]])
                    for (ci = 0; ci < 4; ci++) p->ch[ci] = 0;
                else /* place next in sequence */
                    p->ch[ci] = c;

            }

        }

    }
#else
    p->ch = c; /* place character */
#endif

}

/** ****************************************************************************

Clear screen buffer

Clears the entire screen buffer to spaces with the current colors and
attributes.

*******************************************************************************/

static void clrbuf(scnptr sc)

{

    /** screen indexes */           int x, y;
    /** pointer to screen record */ scnrec* sp;

    /* clear the screen buffer */
    for (y = 1;  y <= dimy; y++)
        for (x = 1; x <= dimx; x++) {

        /* index screen character location */
        sp = &SCNBUF(sc, x, y);
        plcchrext(sp, ' '); /* clear to spaces */
        /* colors and attributes to the global set */
        sp->forec = forec;
        sp->backc = backc;
        sp->attr = attr;

    }

}

/** ****************************************************************************

Initialize screen

Clears all the parameters in the present screen context, and updates the
display to match.

*******************************************************************************/

static void iniscn(scnptr sc)

{

    ncury = 1; /* set cursor at home */
    ncurx = 1;
    /* these attributes and colors are pretty much windows 95 specific. The
       Bizarre setting of "blink" actually allows access to bright white */
    forec = pa_black; /* set colors and attributes */
    backc = pa_white;
    attr = sanone;
    curvis = curon; /* set cursor visible from curent state */
    clrbuf(sc); /* clear screen buffer with that */

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

Scroll screen

Scrolls the ANSI terminal screen by deltas in any given direction. For an ANSI
terminal, we special case any scroll that is downward only, without any
movement in x. These are then done by an arbitrary number of line feeds
executed at the bottom of the screen.
For all other scrolls, we do this by completely refreshing the contents of the
screen, including blank lines or columns for the "scrolled in" areas. The
blank areas are all given the current attributes and colors.
The cursor always remains in place for these scrolls, even though the text
is moving under it.

*******************************************************************************/

void prtbuf(scnptr sc)

{

    int x, y;

    fprintf(stderr, "Screen:\n\n");
    for (y = 1; y <= dimy; y++) {

        fprintf(stderr, "%2d\"", y);
#ifdef ALLOWUTF8
        for (x = 1; x <= dimx; x++) fprintf(stderr, "%c", SCNBUF(sc, x, y).ch[0]);
#else
        for (x = 1; x <= dimx; x++) fprintf(stderr, "%c", SCNBUF(sc, x, y).ch);
#endif
        fprintf(stderr, "\"\n");

    }

}

static void iscroll(scnptr sc, int x, int y)

{

    int      xi, yi; /* screen counters */
    pa_color fs, bs; /* color saves */
    scnatt   as;     /* attribute saves */
    scnrec*  scnsav; /* full screen buffer save */
    int      lx;     /* last unmatching character index */
    int      m;      /* match flag */
    scnrec*  sp;     /* pointer to screen record */
    scnrec*  sp2;

    if (y > 0 && x == 0) {

        if (indisp(sc)) { /* in display */

            trm_curoff(); /* turn cursor off for display */
            curon = FALSE;
            /* downward straight scroll, we can do this with native scrolling */
            trm_cursor(1, dimy); /* position to bottom of screen */
            /* use linefeed to scroll. linefeeds work no matter the state of
               wrap, and use whatever the current background color is */
            yi = y;   /* set line count */
            while (yi > 0) {  /* scroll down requested lines */

                putchr('\n');   /* scroll down */
                yi--;   /* count lines */

            }
            /* restore cursor position */
            trm_cursor(ncurx, ncury);
            cursts(sc); /* re-enable cursor */

        }
        /* now, adjust the buffer to be the same */
        for (yi = 1; yi <= dimy-1; yi++) /* move any lines up */
            if (yi+y <= dimy) /* still within buffer */
                /* move lines up */
                    memcpy(&sc[(yi-1)*dimx], &sc[(yi+y-1)*dimx],
                           dimx*sizeof(scnrec));
        for (yi = dimy-y+1; yi <= dimy; yi++) /* clear blank lines at end */
            for (xi = 1; xi <= dimx; xi++) {

            sp = &SCNBUF(sc, xi, yi);
            plcchrext(sp, ' '); /* clear to blanks at colors and attributes */
            sp->forec = forec;
            sp->backc = backc;
            sp->attr = attr;

        }

    } else { /* odd direction scroll */

        /* when the scroll is arbitrary, we do it by completely refreshing the
           contents of the screen from the buffer */
        if (x <= -dimx || x >= dimx || y <= -dimy || y >= dimy) {

            /* scroll would result in complete clear, do it */
            trm_clear();   /* scroll would result in complete clear, do it */
            clrbuf(sc);   /* clear the screen buffer */
            /* restore cursor position */
            trm_cursor(ncurx, ncury);

        } else { /* scroll */

            /* true scroll is done in two steps. first, the contents of the buffer
               are adjusted to read as after the scroll. then, the contents of the
               buffer are output to the terminal. before the buffer is changed,
               we perform a full save of it, which then represents the "current"
               state of the real terminal. then, the new buffer contents are
               compared to that while being output. this saves work when most of
               the screen is spaces anyways */

            /* save the entire buffer */
            scnsav = malloc(sizeof(scnrec)*dimy*dimx);
            memcpy(scnsav, sc, sizeof(scnrec)*dimy*dimx);
            if (y > 0) {  /* move text up */

                for (yi = 1; yi < dimy; yi++) /* move any lines up */
                    if (yi + y <= dimy) /* still within buffer */
                        /* move lines up */
                        memcpy(&sc[(yi-1)*dimx], &sc[(yi+y-1)*dimx],
                           dimx*sizeof(scnrec));
                for (yi = dimy-y+1; yi <= dimy; yi++)
                    /* clear blank lines at end */
                    for (xi = 1; xi <= dimx; xi++) {

                    sp = &SCNBUF(sc, xi, yi);
                    /* clear to blanks at colors and attributes */
                    plcchrext(sp, ' ');
                    sp->forec = forec;
                    sp->backc = backc;
                    sp->attr = attr;

                }

            } else if (y < 0) { /* move text down */

                for (yi = dimy; yi >= 2; yi--)   /* move any lines up */
                    if (yi + y >= 1) /* still within buffer */
                        /* move lines up */
                        memcpy(&sc[(yi-1)*dimx], &sc[(yi+y-1)*dimx],
                           dimx*sizeof(scnrec));
                for (yi = 1; yi <= abs(y); yi++) /* clear blank lines at start */
                    for (xi = 1; xi <= dimx; xi++) {

                    sp = &SCNBUF(sc, xi, yi);
                    /* clear to blanks at colors and attributes */
                    plcchrext(sp, ' ');
                    sp->forec = forec;
                    sp->backc = backc;
                    sp->attr = attr;

                }

            }
            if (x > 0) { /* move text left */
                for (yi = 1; yi <= dimy; yi++) { /* move text left */

                    for (xi = 1; xi <= dimx-1; xi++) /* move left */
                        if (xi+x <= dimx) /* still within buffer */
                            /* move characters left */
                            memcpy(&SCNBUF(sc, xi, yi), &SCNBUF(sc, xi+x, yi),
                               sizeof(scnrec));
                    /* clear blank spaces at right */
                    for (xi = dimx-x+1; xi <= dimx; xi++) {

                        sp = &SCNBUF(sc, xi, yi);
                        /* clear to blanks at colors and attributes */
                        plcchrext(sp, ' ');
                        sp->forec = forec;
                        sp->backc = backc;
                        sp->attr = attr;

                    }

                }

            } else if (x < 0) { /* move text right */

                for (yi = 1; yi <= dimy; yi++) { /* move text right */

                    for (xi = dimx; xi >= 2; xi--) /* move right */
                        if (xi+x >= 1) /* still within buffer */
                            /* move characters left */
                            memcpy(&SCNBUF(sc, xi, yi), &SCNBUF(sc, xi+x, yi),
                               sizeof(scnrec));
                    /* clear blank spaces at left */
                    for (xi = 1; xi <= abs(x); xi++) {

                        sp = &SCNBUF(sc, xi, yi);
                        /* clear to blanks at colors and attributes */
                        plcchrext(sp, ' ');
                        sp->forec = forec;
                        sp->backc = backc;
                        sp->attr = attr;

                    }

                }

            }
            if (indisp(sc)) { /* in display */

                trm_curoff(); /* turn cursor off for display */
                curon = FALSE;
                /* the buffer is adjusted. now just copy the complete buffer to the
                   screen */
                trm_home(); /* restore cursor to upper left to start */
                fs = forec; /* save current colors and attributes */
                bs = backc;
                as = attr;
                for (yi = 1; yi <= dimy; yi++) { /* lines */

                    /* find the last unmatching character between real and new buffers.
                       Then, we only need output the leftmost non-matching characters
                       on the line. note that it does not really help us that characters
                       WITHIN the line match, because a character output is as or more
                       efficient as a cursor movement. if, however, you want to get
                       SERIOUSLY complex, we could check runs of matching characters,
                       then check if performing a direct cursor position is less output
                       characters than just outputting data :) */
                    lx = dimx; /* set to end */
                    do { /* check matches */

                        m = 1; /* set match */
                        /* check all elements match */
                        sp = &SCNBUF(sc, lx, yi);
                        sp2 = &SCNBUF(scnsav, lx, yi);
                        if (sp->ch != sp2->ch) m = 0;
                        if (sp->forec != sp2->forec) m = 0;
                        if (sp->backc != sp2->backc) m = 0;
                        if (sp->attr != sp2->attr)  m = 0;
                        if (m) lx--; /* next character */

                    } while (m && lx); /* until match or no more */
                    for (xi = 1; xi <= lx; xi++) { /* characters */

                        /* for each new character, we compare the attributes and colors
                           with what is set. if a new color or attribute is called for,
                           we set that, and update the saves. this technique cuts down on
                           the amount of output characters */
                        sp = &SCNBUF(sc, xi, yi);
                        if (sp->forec != fs) { /* new foreground color */

                            trm_fcolor(sp->forec); /* set the new color */
                            fs = sp->forec; /* set save */

                        }
                        if (sp->backc != bs) { /* new background color */

                            trm_bcolor(sp->backc); /* set the new color */
                            bs = sp->backc; /* set save */

                        }

                        if (sp->attr != as)  { /* new attribute */

                            setattr(sc, sp->attr); /* set the new attribute */
                            as = sp->attr;   /* set save */

                        }
#ifdef ALLOWUTF8
                        putnstr(sp->ch, 4); /* now output the actual character */
#else
                        putchr(sp->ch);
#endif

                    }
                    if (yi < dimy)
                        /* output next line sequence on all lines but the last. this is
                           because the last one would cause us to scroll */
                        putstr("\r\n");

                }
                /* restore cursor position */
                trm_cursor(ncurx, ncury);
                trm_fcolor(forec);   /* restore colors */
                trm_bcolor(backc);   /* restore attributes */
                setattr(sc, attr);
                cursts(sc); /* re-enable cursor */

            }

        }

    }

}

/** ****************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

static void iclear(scnptr sc)

{

    clrbuf(sc); /* clear the screen buffer */
    ncury = 1; /* set cursor at home */
    ncurx = 1;
    if (indisp(sc)) { /* in display */

        trm_clear(); /* erase screen */
        curx = 1; /* set actual cursor location */
        cury = 1;
        curval = 1;
        setcur(sc);

    }


}

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

static void icursor(scnptr sc, int x, int y)

{

    ncury = y; /* set new position */
    ncurx = x;
    setcur(sc);

}

/** ****************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

static void iup(scnptr sc)

{

    if (scroll) { /* autowrap is on */

        if (ncury > 1) /* not at top of screen */
            ncury = ncury-1; /* update position */
        else if (scroll) /* scroll enabled */
            iscroll(sc, 0, -1); /* at top already, scroll up */
        else /* wrap cursor around to screen bottom */
            ncury = dimy; /* set new position */

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (ncury > -INT_MAX) ncury--;
    setcur(sc);

}

/** ****************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

static void idown(scnptr sc)

{

    if (scroll) { /* autowrap is on */

        if (ncury < dimy) /* not at bottom of screen */
            ncury = ncury+1; /* update position */
        else if (scroll) /* wrap enabled */
            iscroll(sc, 0, +1); /* already at bottom, scroll down */
        else /* wrap cursor around to screen top */
            ncury = 1; /* set new position */

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (ncury < INT_MAX) ncury++;
    setcur(sc);

}

/** ****************************************************************************

Move cursor left internal

Moves the cursor one character left.

*******************************************************************************/

static void ileft(scnptr sc)

{

    if (scroll) { /* autowrap is on */

        if (ncurx > 1)  /* not at extreme left */
            ncurx = ncurx-1; /* update position */
        else { /* wrap cursor motion */

            iup(sc); /* move cursor up one line */
            ncurx = dimx; /* set cursor to extreme right */

        }

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (ncurx > -INT_MAX) ncurx--;
    setcur(sc);

}

/** ****************************************************************************

Move cursor right internal

Moves the cursor one character right.

*******************************************************************************/

static void iright(scnptr sc)

{

    if (scroll) { /* autowrap is on */

        if (ncurx < dimx) /* not at extreme right */
            ncurx = ncurx+1; /* update position */
        else { /* wrap cursor motion */

            idown(sc); /* move cursor up one line */
            ncurx = 1; /* set cursor to extreme left */

        }

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (ncurx < INT_MAX) ncurx++;
    setcur(sc);

}



/** ****************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attribute.

We handle some elementary control codes here, like newline, backspace and form
feed. However, the idea is not to provide a parallel set of screen controls.
That's what the API is for.

*******************************************************************************/

static void plcchr(scnptr sc, unsigned char c)

{

    scnrec* p;
    int i;

    /* handle special character cases first */
    if (c == '\r') /* carriage return, position to extreme left */
        icursor(sc, 1, ncury);
    else if (c == '\n') {

        /* line end */
        idown(sc); /* line feed, move down */
        /* position to extreme left */
        icursor(sc, 1, ncury);

    } else if (c == '\b') ileft(sc); /* back space, move left */
    else if (c == '\f') iclear(sc); /* clear screen */
    else if (c == '\t') {

        /* find next tab position */
        i = ncurx+1; /* find current x +1 */
        while (i < dimx && !tabs[i-1]) i++;
        if (tabs[i-1]) /* we found a tab */
           while (ncurx < i) iright(sc);

    } else if (c >= ' ' && c != 0x7f) {

#ifdef ALLOWUTF8
        /* if UTF-8 leader, set character count */
        if (c >= 0xc0) utf8cnt = utf8bits[c >> 4];
        if (utf8cnt) utf8cnt--; /* count off characters */
#endif
        /* normal character case, not control character */
        if (ncurx >= 1 && ncurx <= dimx &&
            ncury >= 1 && ncury <= dimy) {

            /* within the buffer space, otherwise just dump */
            p = &SCNBUF(sc, ncurx, ncury);
            plcchrext(p, c); /* place character in buffer */
            p->forec = forec; /* place colors */
            p->backc = backc;
            p->attr = attr; /* place attribute */

        }
        /* cursor in bounds, in display, and not mid-UTF-8 */
        if (icurbnd(sc) && indisp(sc)) {

            /* This handling is from iright. We do this here because
               placement implicitly moves the cursor */
            putchr(c); /* output character to terminal */
#ifdef ALLOWUTF8
            if (!utf8cnt)
#endif
            { /* not working on partial character */

                /* at right side, don't count on the screen wrap action */
                if (curx == dimx) curval = 0;
                else curx++; /* update physical cursor */
                if (scroll) { /* autowrap is on */

                    if (ncurx < dimx) /* not at extreme right */
                        ncurx = ncurx+1; /* update position */
                    else { /* wrap cursor motion */

                        idown(sc); /* move cursor down one line */
                        ncurx = 1; /* set cursor to extreme left */

                    }

                } else {/* autowrap is off */

                    /* prevent overflow, but otherwise its unlimited */
                    if (ncurx < INT_MAX) ncurx++;
                    /* don't count on physical cursor behavior if scrolling is
                       off and we are at extreme right */
                    curval = 0;

                }
                setcur(sc); /* update physical cursor */

            }

        } else iright(sc); /* move right */

    }

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editing. The line is placed into the
input line buffer.

*******************************************************************************/

static void readline(void)

{

    pa_evtrec er;   /* event record */
    scnptr    sc;   /* pointer to current screen */
    int       ins;  /* insert/overwrite mode */
    int       xoff; /* x starting line offset */
    int       l;    /* buffer length */
    int       i;

    sc = screens[curupd-1]; /* index current screen */
    inpptr = 0; /* set 1st character position */
    inpbuf[0] = 0; /* terminate line */
    ins = 1;
    xoff = ncurx; /* save starting line offset */
    do { /* get line characters */

        ievent(&er); /* get next event */
        switch (er.etype) { /* event */

            case pa_etterm: exit(1); /* halt program */
            case pa_etenter: /* line terminate */
                while (inpbuf[inpptr]) inpptr++; /* advance to end */
                inpbuf[inpptr] = '\n'; /* return newline */
                /* terminate the line for debug prints */
                inpbuf[inpptr+1] = 0;
                plcchr(sc, '\r'); /* output newline sequence */
                plcchr(sc, '\n');
                break;
            case pa_etchar: /* character */
                if (inpptr < MAXLIN-2) {

                    if (ins) { /* insert */

                        i = inpptr; /* find end */
                        while (inpbuf[i]) i++;
                        /* move line up */
                        while (inpptr <= i) { inpbuf[i+1] = inpbuf[i]; i--; }
                        inpbuf[inpptr] = er.echar; /* place new character */
                        /* reprint line */
                        i = inpptr;
                        while (inpbuf[i]) plcchr(sc, inpbuf[i++]);
                        /* back up */
                        i = inpptr;
                        while (inpbuf[i++]) plcchr(sc, '\b');
                        /* forward and next char */
                        plcchr(sc, inpbuf[inpptr]);
                        inpptr++;

                    } else { /* overwrite */

                        /* if end, move end marker */
                        if (!inpbuf[inpptr]) inpbuf[inpptr+1] = 0;
                        inpbuf[inpptr] = er.echar; /* place new character */
                        /* forward and next char */
                        plcchr(sc, inpbuf[inpptr]);
                        inpptr++;

                    }

                }
                break;
            case pa_etdelcb: /* delete character backwards */
                if (inpptr > 0) { /* not at extreme left */

                    inpptr--; /* back up pointer */
                    /* move characters back */
                    i = inpptr;
                    while (inpbuf[i]) { inpbuf[i] = inpbuf[i+1]; i++; }
                    plcchr(sc, '\b'); /* move cursor back */
                    /* repaint line */
                    i = inpptr;
                    while (inpbuf[i]) plcchr(sc, inpbuf[i++]);
                    plcchr(sc, ' '); /* blank last */
                    /* back up */
                    plcchr(sc, '\b');
                    i = inpptr;
                    while (inpbuf[i++]) plcchr(sc, '\b');

                }
                break;
            case pa_etdelcf: /* delete character forward */
                if (inpbuf[inpptr]) { /* not at extreme right */

                    /* move characters down */
                    i = inpptr;
                    while (inpbuf[i]) { inpbuf[i] = inpbuf[i+1]; i++; }
                    /* repaint right */
                    i = inpptr;
                    while (inpbuf[i]) plcchr(sc, inpbuf[i++]);
                    plcchr(sc, ' '); /* blank last */
                    /* back up */
                    plcchr(sc, '\b');
                    i = inpptr;
                    while (inpbuf[i++]) plcchr(sc, '\b');

                }
                break;
            case pa_etright: /* right character */
                /* not at extreme right, go right */
                if (inpbuf[inpptr]) {

                    plcchr(sc, inpbuf[inpptr]);
                    inpptr++; /* advance input */

                }
                break;

            case pa_etleft: /* left character */
                /* not at extreme left, go left */
                if (inpptr > 0) {

                    plcchr(sc, '\b');
                    inpptr--; /* back up pointer */

                }
                break;

            case pa_etmoumov: /* mouse moved */
                /* we can track this internally */
                break;

            case pa_etmouba: /* mouse click */
                if (er.amoubn == 1) {

                    l = strlen(inpbuf);
                    if (ncury == nmpy && xoff <= nmpx && xoff+l >= nmpx) {

                        /* mouse position is within buffer space, set
                           position */
                        icursor(sc, nmpx, ncury);
                        inpptr = nmpx-xoff;

                    }

                }
                break;

            case pa_ethomel: /* beginning of line */
                /* back up to start of line */
                while (inpptr) {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                break;

            case pa_etendl: /* end of line */
                /* go to end of line */
                while (inpbuf[inpptr]) {

                    plcchr(sc, inpbuf[inpptr]);
                    inpptr++;

                }
                break;

            case pa_etinsertt: /* toggle insert mode */
                ins = !ins; /* toggle insert mode */
                break;

            case pa_etdell: /* delete whole line */
                /* back up to start of line */
                while (inpptr) {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                /* erase line on screen */
                while (inpbuf[inpptr]) {

                    plcchr(sc, ' ');
                    inpptr++;

                }
                /* back up again */
                while (inpptr) {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                inpbuf[inpptr] = 0; /* clear line */
                break;

            case pa_etleftw: /* left word */
                /* back over any spaces */
                while (inpptr && inpbuf[inpptr-1] == ' ') {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                /* now back over any non-space */
                while (inpptr && inpbuf[inpptr-1] != ' ') {

                    plcchr(sc, '\b');
                    inpptr--;

                }
                break;

            case pa_etrightw: /* right word */
                /* advance over any non-space */
                while (inpbuf[inpptr] && inpbuf[inpptr] != ' ') {

                    plcchr(sc, inpbuf[inpptr]);
                    inpptr++;

                }
                /* advance over any spaces */
                while (inpbuf[inpptr] && inpbuf[inpptr] == ' ') {

                    plcchr(sc, inpbuf[inpptr]);
                    inpptr++;

                }
                break;

            default: ;

        }

    } while (er.etype != pa_etenter); /* until line terminate */
    inpptr = 0; /* set 1st position on active line */

}

/*******************************************************************************

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


/*******************************************************************************

Read file

If the file is the stdin file, we process that by reading from the event queue
and returning any characters found. Any events besides character events are
 discarded, which is why reading from the stdin file is a downward compatible
operation.

The input from user is line buffered and may be edited by the user.

All other files are passed on to the system level.

*******************************************************************************/

static ssize_t iread(int fd, void* buff, size_t count)

{

    ssize_t rc; /* return code */
    char *p = (char *)buff;
    size_t cnt = count;

    if (fd == INPFIL) {

        /* get data from terminal */
        while (cnt) {

            /* if there is no line in the input buffer, get one */
            if (inpptr == -1) readline();
            *p = inpbuf[inpptr]; /* get and place next character */
            if (inpptr < MAXLIN) inpptr++; /* next */
            /* if we have just read the last of that line, then flag buffer
               empty */
            if (*p == '\n') inpptr = -1;
            p++; /* next character */
            cnt--; /* count characters */

        }
        rc = count; /* set return same as count */

    } else rc = (*ofpread)(fd, buff, count);

    return rc;

}

/*******************************************************************************

Write

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    ssize_t rc; /* return code */
    unsigned char *p = (unsigned char *)buff;
    size_t cnt = count;

    if (fd == OUTFIL) {

        /* send data to terminal */
        while (cnt--) plcchr(screens[curupd-1], *p++);
        rc = count; /* set return same as count */

    } else rc = (*ofpwrite)(fd, buff, count);

    return rc;

}

/*******************************************************************************

Open

Terminal is assumed to be opened when the system starts, and closed when it
shuts down. Thus we do nothing for this.

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

/*******************************************************************************
*
* External API routines
*
*******************************************************************************/

/** ****************************************************************************

Position cursor

This is the external interface to cursor.

*******************************************************************************/

void _pa_cursor_ovr(pa_cursor_t nfp, pa_cursor_t* ofp)
    { *ofp = cursor_vect; cursor_vect = nfp; }
void pa_cursor(FILE* f, int x, int y) { (*cursor_vect)(f, x, y); }

static void cursor_ivf(FILE *f, int x, int y)

{

    icursor(screens[curupd-1], x, y); /* position cursor */

}

/*******************************************************************************

Find if cursor is in screen bounds

This is the external interface to curbnd.

*******************************************************************************/

void _pa_curbnd_ovr(pa_curbnd_t nfp, pa_curbnd_t* ofp)
    { *ofp = curbnd_vect; curbnd_vect = nfp; }
int pa_curbnd(FILE* f) { (*curbnd_vect)(f); }

static int curbnd_ivf(FILE *f)

{

   return icurbnd(screens[curupd-1]);

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

void _pa_maxx_ovr(pa_maxx_t nfp, pa_maxx_t* ofp)
    { *ofp = maxx_vect; maxx_vect = nfp; }
int pa_maxx(FILE* f) { (*maxx_vect)(f); }

static int maxx_ivf(FILE *f)

{

    return bufx; /* set maximum x */

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

void _pa_maxy_ovr(pa_maxy_t nfp, pa_maxy_t* ofp)
    { *ofp = maxy_vect; maxy_vect = nfp; }
int pa_maxy(FILE* f) { (*maxy_vect)(f); }

static int maxy_ivf(FILE *f)

{

    return bufy; /* set maximum y */

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void _pa_home_ovr(pa_home_t nfp, pa_home_t* ofp)
    { *ofp = home_vect; home_vect = nfp; }
void pa_home(FILE* f) { (*home_vect)(f); }

static void home_ivf(FILE *f)

{

    ncury = 1; /* set cursor at home */
    ncurx = 1;
    setcur(screens[curupd-1]);

}

/** ****************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void _pa_del_ovr(pa_del_t nfp, pa_del_t* ofp)
    { *ofp = del_vect; del_vect = nfp; }
void pa_del(FILE* f) { (*del_vect)(f); }

static void del_ivf(FILE* f)

{

    ileft(screens[curupd-1]); /* back up cursor */
    plcchr(screens[curupd-1], ' '); /* blank out */
    ileft(screens[curupd-1]); /* back up again */

}

/** ****************************************************************************

Move cursor up

This is the external interface to up.

*******************************************************************************/

void _pa_up_ovr(pa_up_t nfp, pa_up_t* ofp)
    { *ofp = up_vect; up_vect = nfp; }
void pa_up(FILE* f) { (*up_vect)(f); }

static void up_ivf(FILE *f)

{

    iup(screens[curupd-1]); /* move up */

}


/** ****************************************************************************

Move cursor down

This is the external interface to down.

*******************************************************************************/

void _pa_down_ovr(pa_down_t nfp, pa_down_t* ofp)
    { *ofp = down_vect; down_vect = nfp; }
void pa_down(FILE* f) { (*down_vect)(f); }

static void down_ivf(FILE *f)

{

    idown(screens[curupd-1]); /* move cursor down */

}

/** ****************************************************************************

Move cursor left

This is the external interface to left.

*******************************************************************************/

void _pa_left_ovr(pa_left_t nfp, pa_left_t* ofp)
    { *ofp = left_vect; left_vect = nfp; }
void pa_left(FILE* f) { (*left_vect)(f); }

static void left_ivf(FILE *f)

{

   ileft(screens[curupd-1]); /* move cursor left */

}

/** ****************************************************************************

Move cursor right

This is the external interface to right.

*******************************************************************************/

void _pa_right_ovr(pa_right_t nfp, pa_right_t* ofp)
    { *ofp = right_vect; right_vect = nfp; }
void pa_right(FILE* f) { (*right_vect)(f); }

static void right_ivf(FILE *f)

{

    iright(screens[curupd-1]); /* move cursor right */

}

/** ****************************************************************************

Turn on/off attribute

Turns on or off a single attribute. The attributes can only be set singly.
If the screen is in display, refreshes the colors. This is because in Windows,
changing attributes causes the colors to change. This should be verified on
xterm.

*******************************************************************************/

static void attronoff(FILE *f, int e, scnatt nattr)

{

    setattr(screens[curupd-1], sanone); /* turn off attributes */
    if (e) { /* attribute on */

        attr = nattr; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(forec); /* set current colors */
            trm_bcolor(backc);

        }

    } else { /* turn it off */

        attr = sanone; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(forec); /* set current colors */
            trm_bcolor(backc);

        }

    }

}

/** ****************************************************************************

Turn on blink attribute

Turns on/off the blink attribute. Note that under windows 95 in a shell window,
blink does not mean blink, but instead "bright". We leave this alone because
we are supposed to also work over a com interface.

*******************************************************************************/

void _pa_blink_ovr(pa_blink_t nfp, pa_blink_t* ofp)
    { *ofp = blink_vect; blink_vect = nfp; }
void pa_blink(FILE* f, int e) { (*blink_vect)(f, e); }

static void blink_ivf(FILE *f, int e)

{

    attronoff(f, e, sablink); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute.

*******************************************************************************/

void _pa_reverse_ovr(pa_reverse_t nfp, pa_reverse_t* ofp)
    { *ofp = reverse_vect; reverse_vect = nfp; }
void pa_reverse(FILE* f, int e) { (*reverse_vect)(f, e); }

static void reverse_ivf(FILE *f, int e)

{

    attronoff(f, e, sarev); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.

*******************************************************************************/

void _pa_underline_ovr(pa_underline_t nfp, pa_underline_t* ofp)
    { *ofp = underline_vect; underline_vect = nfp; }
void pa_underline(FILE* f, int e) { (*underline_vect)(f, e); }

static void underline_ivf(FILE *f, int e)

{

    attronoff(f, e, saundl); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.

*******************************************************************************/

void _pa_superscript_ovr(pa_superscript_t nfp, pa_superscript_t* ofp)
    { *ofp = superscript_vect; superscript_vect = nfp; }
void pa_superscript(FILE* f, int e) { (*superscript_vect)(f, e); }

static void superscript_ivf(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.

*******************************************************************************/

void _pa_subscript_ovr(pa_subscript_t nfp, pa_subscript_t* ofp)
    { *ofp = subscript_vect; subscript_vect = nfp; }
void pa_subscript(FILE* f, int e) { (*subscript_vect)(f, e); }

static void subscript_ivf(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.

*******************************************************************************/

void _pa_italic_ovr(pa_italic_t nfp, pa_italic_t* ofp)
    { *ofp = italic_vect; italic_vect = nfp; }
void pa_italic(FILE* f, int e) { (*italic_vect)(f, e); }

static void italic_ivf(FILE *f, int e)

{

    attronoff(f, e, saital); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.

*******************************************************************************/

void _pa_bold_ovr(pa_bold_t nfp, pa_bold_t* ofp)
    { *ofp = bold_vect; bold_vect = nfp; }
void pa_bold(FILE* f, int e) { (*bold_vect)(f, e); }

static void bold_ivf(FILE *f, int e)

{

    attronoff(f, e, sabold); /* enable/disable attbute */

}

/** ****************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.

Not implemented.

*******************************************************************************/

void _pa_strikeout_ovr(pa_strikeout_t nfp, pa_strikeout_t* ofp)
    { *ofp = strikeout_vect; strikeout_vect = nfp; }
void pa_strikeout(FILE* f, int e) { (*strikeout_vect)(f, e); }

static void strikeout_ivf(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void _pa_standout_ovr(pa_standout_t nfp, pa_standout_t* ofp)
    { *ofp = standout_vect; standout_vect = nfp; }
void pa_standout(FILE* f, int e) { (*standout_vect)(f, e); }

static void standout_ivf(FILE *f, int e)

{

    pa_reverse(f, e); /* implement as reverse */

}

/** ****************************************************************************

Set foreground color

Sets the foreground (text) color from the universal primary code.

*******************************************************************************/

void _pa_fcolor_ovr(pa_fcolor_t nfp, pa_fcolor_t* ofp)
    { *ofp = fcolor_vect; fcolor_vect = nfp; }
void pa_fcolor(FILE* f, pa_color c) { (*fcolor_vect)(f, c); }

static void fcolor_ivf(FILE *f, pa_color c)

{

    if (curupd == curdsp) trm_fcolor(c); /* set color */
    forec = c;

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void _pa_bcolor_ovr(pa_bcolor_t nfp, pa_bcolor_t* ofp)
    { *ofp = bcolor_vect; bcolor_vect = nfp; }
void pa_bcolor(FILE* f, pa_color c) { (*bcolor_vect)(f, c); }

static void bcolor_ivf(FILE *f, pa_color c)

{

    if (curupd == curdsp) trm_bcolor(c); /* set color */
    backc = c;

}

/** ****************************************************************************

Enable/disable automatic scroll

Enables or disables automatic screen scroll. With automatic scroll on, moving
off the screen at the top or bottom will scroll up or down, respectively.

*******************************************************************************/

void _pa_auto_ovr(pa_auto_t nfp, pa_auto_t* ofp)
    { *ofp = auto_vect; auto_vect = nfp; }
void pa_auto(FILE* f, int e) { (*auto_vect)(f, e); }

static void auto_ivf(FILE *f, int e)

{

    scroll = e; /* set line wrap status */

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void _pa_curvis_ovr(pa_curvis_t nfp, pa_curvis_t* ofp)
    { *ofp = curvis_vect; curvis_vect = nfp; }
void pa_curvis(FILE* f, int e) { (*curvis_vect)(f, e); }

static void curvis_ivf(FILE *f, int e)

{

    curvis = !!e; /* set cursor visible status */
    if (e) trm_curon(); else trm_curoff();

}

/** ****************************************************************************

Scroll screen

Process full delta scroll on screen. This is the external interface to this
int.

*******************************************************************************/

void _pa_scroll_ovr(pa_scroll_t nfp, pa_scroll_t* ofp)
    { *ofp = scroll_vect; scroll_vect = nfp; }
void pa_scroll(FILE* f, int x, int y) { (*scroll_vect)(f, x, y); }

static void scroll_ivf(FILE *f, int x, int y)

{

    iscroll(screens[curupd-1], x, y); /* process scroll */

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

void _pa_curx_ovr(pa_curx_t nfp, pa_curx_t* ofp)
    { *ofp = curx_vect; curx_vect = nfp; }
int pa_curx(FILE* f) { (*curx_vect)(f); }

static int curx_ivf(FILE *f)

{

    return ncurx; /* return current location x */

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

void _pa_cury_ovr(pa_cury_t nfp, pa_cury_t* ofp)
    { *ofp = cury_vect; cury_vect = nfp; }
int pa_cury(FILE* f) { (*cury_vect)(f); }

static int cury_ivf(FILE *f)

{

    return ncury; /* return current location y */

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

Note that split update and display screens are not implemented at present.

*******************************************************************************/

void _pa_select_ovr(pa_select_t nfp, pa_select_t* ofp)
    { *ofp = select_vect; select_vect = nfp; }
void pa_select(FILE* f, int u, int d) { (*select_vect)(f, u, d); }

static void select_ivf(FILE *f, int u, int d)

{

    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error(einvscn); /* invalid screen number */
    if (curupd != u) { /* update screen changes */

        curupd = u; /* change to new screen */
        if (!screens[curupd-1]) { /* no screen allocated there */

            /* allocate screen array */
            screens[curupd-1] = malloc(sizeof(scnrec)*dimy*dimx);
            iniscn(screens[curupd-1]); /* initalize that */

        }

    }
    if (curdsp != d) { /* display screen changes */

        curdsp = d; /* change to new screen */
        if (screens[curdsp-1]) /* screen allocated there */
            restore(screens[curdsp-1]); /* restore current screen */
        else { /* no current screen, create a new one */

            /* allocate screen array */
            screens[curdsp-1] = malloc(sizeof(scnrec)*dimy*dimx);
            iniscn(screens[curdsp-1]); /* initalize that */
            restore(screens[curdsp-1]); /* place on display */

        }

    }

}

/** ****************************************************************************

Acquire next input event

Decodes the input for various events. These are sent to the override handlers
first, then if no chained handler dealt with it, we return the event to the
caller.

*******************************************************************************/

void _pa_event_ovr(pa_event_t nfp, pa_event_t* ofp)
    { *ofp = event_vect; event_vect = nfp; }
void pa_event(FILE* f, pa_evtrec* er) { (*event_vect)(f, er); }

static void event_ivf(FILE* f, pa_evtrec *er)

{

    static int ecnt = 0; /* PA event counter */

    do { /* loop handling via event vectors */

        /* get next input event */
        dequepaevt(er); /* get next queued event */
        er->handled = 1; /* set event is handled by default */
        (evtshan)(er); /* call master event handler */
        if (!er->handled) { /* send it to fanout */

            er->handled = 1; /* set event is handled by default */
            (*evthan[er->etype])(er); /* call event handler first */

        }

    } while (er->handled);
    /* event not handled, return it to the caller */

    /* do diagnostic dump of PA events */
    if (dmpevt) {

        prtevtt(er->etype); fprintf(stderr, "\n"); fflush(stderr);

    }

}

/** ****************************************************************************

Set timer

*******************************************************************************/

void _pa_timer_ovr(pa_timer_t nfp, pa_timer_t* ofp)
    { *ofp = timer_vect; timer_vect = nfp; }
void pa_timer(FILE* f, int i, long t, int r) { (*timer_vect)(f, i, t, r); }

static void timer_ivf(/* file to send event to */              FILE* f,
                      /* timer handle */                       int   i,
                      /* number of 100us counts */             long  t,
                      /* timer is to rerun after completion */ int   r)

{

    if (i < 1 || i > PA_MAXTIM) error(einvhan); /* invalid timer handle */
    timtbl[i-1] = system_event_addsetim(timtbl[i-1], t, r);

}

/** ****************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.
Killed timers are not removed. Once a timer is set active, it is always set
in reserve.

*******************************************************************************/

void _pa_killtimer_ovr(pa_killtimer_t nfp, pa_killtimer_t* ofp)
    { *ofp = killtimer_vect; killtimer_vect = nfp; }
void pa_killtimer(FILE* f, int   i ) { (*killtimer_vect)(f, i ); }

static void killtimer_ivf(/* file to kill timer on */ FILE *f,
                  /* handle of timer */       int i)

{

    if (i < 1 || i > PA_MAXTIM) error(einvhan); /* invalid timer handle */
    if (timtbl[i-1] <= 0) error(etimacc); /* no such timer */
    system_event_deasetim(timtbl[i-1]); /* deactivate timer */

}

/** ****************************************************************************

Returns number of mice

Returns the number of mice attached. In xterm, we can't actually determine if
we have a mouse or not, so we just assume we have one. It will be a dead mouse
if none is available, never changing it's state.

*******************************************************************************/

void _pa_mouse_ovr(pa_mouse_t nfp, pa_mouse_t* ofp)
    { *ofp = mouse_vect; mouse_vect = nfp; }
int pa_mouse(FILE* f) { (*mouse_vect)(f); }

static int mouse_ivf(FILE *f)

{

    return 1; /* set 1 mouse */

}

/** ****************************************************************************

Returns number of buttons on a mouse

Returns the number of buttons implemented on a given mouse. With xterm we have
to assume 3 buttons.

*******************************************************************************/

void _pa_mousebutton_ovr(pa_mousebutton_t nfp, pa_mousebutton_t* ofp)
    { *ofp = mousebutton_vect; mousebutton_vect = nfp; }
int pa_mousebutton(FILE* f, int m) { (*mousebutton_vect)(f, m); }

static int mousebutton_ivf(FILE *f, int m)

{

    return 3; /* set three buttons */

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

void _pa_joystick_ovr(pa_joystick_t nfp, pa_joystick_t* ofp)
    { *ofp = joystick_vect; joystick_vect = nfp; }
int pa_joystick(FILE* f) { (*joystick_vect)(f); }

static int joystick_ivf(FILE *f)

{

    return (numjoy); /* set number of joysticks */

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

void _pa_joybutton_ovr(pa_joybutton_t nfp, pa_joybutton_t* ofp)
    { *ofp = joybutton_vect; joybutton_vect = nfp; }
int pa_joybutton(FILE* f, int j) { (*joybutton_vect)(f, j); }

static int joybutton_ivf(FILE *f, int j)

{

    if (j < 1 || j > numjoy) error(einvjoy); /* bad joystick id */
    if (!joytab[j-1]) error(esystem); /* should be a table entry */

    return (joytab[j-1]->button); /* return button count */

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodementional
joystick can be considered a slider without positional meaning.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

void _pa_joyaxis_ovr(pa_joyaxis_t nfp, pa_joyaxis_t* ofp)
    { *ofp = joyaxis_vect; joyaxis_vect = nfp; }
int pa_joyaxis(FILE* f, int j) { (*joyaxis_vect)(f, j); }

static int joyaxis_ivf(FILE *f, int j)

{

    int ja;

    if (j < 1 || j > numjoy) error(einvjoy); /* bad joystick id */
    if (!joytab[j-1]) error(esystem); /* should be a table entry */

    ja = joytab[j-1]->axis; /* get axis number */
    if (ja > 6) ja = 6; /* limit to 6 maximum */

    return (ja); /* set axis number */

}

/** ****************************************************************************

settab

Sets a tab. The tab number t is 1 to n, and indicates the column for the tab.
Setting a tab stop means that when a tab is received, it will move to the next
tab stop that is set. If there is no next tab stop, nothing will happen.

*******************************************************************************/

void _pa_settab_ovr(pa_settab_t nfp, pa_settab_t* ofp)
    { *ofp = settab_vect; settab_vect = nfp; }
void pa_settab(FILE* f, int t) { (*settab_vect)(f, t); }

static void settab_ivf(FILE* f, int t)

{

    if (t < 1 || t > dimx) error(einvtab); /* invalid tab position */
    tabs[t-1] = 1; /* set tab position */

}

/** ****************************************************************************

restab

Resets a tab. The tab number t is 1 to n, and indicates the column for the tab.

*******************************************************************************/

void _pa_restab_ovr(pa_restab_t nfp, pa_restab_t* ofp)
    { *ofp = restab_vect; restab_vect = nfp; }
void pa_restab(FILE* f, int t) { (*restab_vect)(f, t); }

static void restab_ivf(FILE* f, int t)

{

    if (t < 1 || t > dimx) error(einvtab); /* invalid tab position */
    tabs[t-1] = 0; /* reset tab position */

}

/** ****************************************************************************

clrtab

Clears all tabs.

*******************************************************************************/

void _pa_clrtab_ovr(pa_clrtab_t nfp, pa_clrtab_t* ofp)
    { *ofp = clrtab_vect; clrtab_vect = nfp; }
void pa_clrtab(FILE* f) { (*clrtab_vect)(f); }

static void clrtab_ivf(FILE* f)

{

    int i;

    for (i = 0; i < dimx; i++) tabs[i] = 0; /* clear all tab stops */

}

/** ****************************************************************************

funkey

Return number of function keys. xterm gives us F1 to F9, takes F10 and F11,
and leaves us F12. It only reserves F10 of the shifted keys, takes most of
the ALT-Fx keys, and leaves all of the CONTROL-Fx keys.
The tradition in PA is to take the F1-F10 keys (it's a nice round number),
but more can be allocated if needed.

*******************************************************************************/

void _pa_funkey_ovr(pa_funkey_t nfp, pa_funkey_t* ofp)
    { *ofp = funkey_vect; funkey_vect = nfp; }
int pa_funkey(FILE* f) { (*funkey_vect)(f); }

static int funkey_ivf(FILE* f)

{

    return MAXFKEY;

}

/** ****************************************************************************

Frametimer

Enables or disables the framing timer.

Not currently implemented.

*******************************************************************************/

void _pa_frametimer_ovr(pa_frametimer_t nfp, pa_frametimer_t* ofp)
    { *ofp = frametimer_vect; frametimer_vect = nfp; }
void pa_frametimer(FILE* f, int e) { (*frametimer_vect)(f, e); }

static void frametimer_ivf(FILE* f, int e)

{

    if (e) { /* set framing timer to run */

        frmsev = system_event_addsetim(frmsev, 166, TRUE);

    } else {

        system_event_deasetim(frmsev);

    }

}

/** ****************************************************************************

Autohold

Turns on or off automatic hold mode.

We don't implement automatic hold here, it has no real use on a terminal, since
we abort to the same window.

*******************************************************************************/

void _pa_autohold_ovr(pa_autohold_t nfp, pa_autohold_t* ofp)
    { *ofp = autohold_vect; autohold_vect = nfp; }
void pa_autohold(int e) { (*autohold_vect)(e); }

static void autohold_ivf(int e)

{

}

/** ****************************************************************************

Write string direct

Writes a string direct to the terminal, bypassing character handling.

*******************************************************************************/

void _pa_wrtstr_ovr(pa_wrtstr_t nfp, pa_wrtstr_t* ofp)
    { *ofp = wrtstr_vect; wrtstr_vect = nfp; }
void pa_wrtstr(FILE* f, char* s) { (*wrtstr_vect)(f, s); }

static void wrtstr_ivf(FILE* f, char *s)

{

    putstr(s);

}

/** ****************************************************************************

Write string direct with length

Writes a string with length direct to the terminal, bypassing character
handling.

*******************************************************************************/

void _pa_wrtstrn_ovr(pa_wrtstrn_t nfp, pa_wrtstrn_t* ofp)
    { *ofp = wrtstrn_vect; wrtstrn_vect = nfp; }
void pa_wrtstrn(FILE* f, char* s, int n) { (*wrtstrn_vect)(f, s, n); }

static void wrtstrn_ivf(FILE* f, char *s, int n)

{

    while (n--) putchr(*s++);

}

/** ****************************************************************************

Size buffer

Sets or resets the size of the buffer surface.

*******************************************************************************/

void _pa_sizbuf_ovr(pa_sizbuf_t nfp, pa_sizbuf_t* ofp)
    { *ofp = sizbuf_vect; sizbuf_vect = nfp; }
void pa_sizbuf(FILE* f, int x, int y) { (*sizbuf_vect)(f, x, y); }

static void sizbuf_ivf(FILE* f, int x, int y)

{

    int si;

    if (bufx != x || bufy != y) {

        /* the buffer asked is not the same as present */
        for (si = 0; si < MAXCON; si++) {

            /* free up any/all present buffers */
            if (screens[si]) free(screens[si]);
            /* allocate new update screen */
            screens[curupd-1] = malloc(sizeof(scnrec)*y*x);
            /* clear it */
            clrbuf(screens[curupd-1]);
            if (curupd != curdsp) { /* display screen not the same */

                /* allocate */
                screens[curdsp-1] = malloc(sizeof(scnrec)*y*x);
                /* clear it */
                clrbuf(screens[curupd-1]);

            }

        }
        /* set new buffer size */
        bufx = x;
        bufy = y;
        /* redraw screen */
        restore(screens[curdsp-1]);

    }

}

/** ****************************************************************************

Override event handler

Overrides or "hooks" the indicated event handler. The existing even handler is
given to the caller, and the new event handler becomes effective. If the event
is called, and the overrider does not want to handle it, that overrider can
call down into the stack by executing the overridden event.

*******************************************************************************/

void _pa_eventover_ovr(pa_eventover_t nfp, pa_eventover_t* ofp)
    { *ofp = eventover_vect; eventover_vect = nfp; }
void pa_eventover(pa_evtcod e, pa_pevthan eh, pa_pevthan* oeh)
    { (*eventover_vect)(e, eh, oeh); }

static void eventover_ivf(pa_evtcod e, pa_pevthan eh,  pa_pevthan* oeh)

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

void _pa_eventsover_ovr(pa_eventsover_t nfp, pa_eventsover_t* ofp)
    { *ofp = eventsover_vect; eventsover_vect = nfp; }
void pa_eventsover(pa_pevthan eh,  pa_pevthan* oeh)
    { (*eventsover_vect)(eh, oeh); }

static void eventsover_ivf(pa_pevthan eh,  pa_pevthan* oeh)

{

    *oeh = evtshan; /* save existing event handler */
    evtshan = eh; /* place new event handler */

}

/** ****************************************************************************

Management extension package

This section is a series of override vectors for unimplemented window management
calls.

*******************************************************************************/

void _pa_sendevent_ovr(pa_sendevent_t nfp, pa_sendevent_t* ofp)
    { *ofp = sendevent_vect; sendevent_vect = nfp; }
void pa_sendevent(FILE* f, pa_evtrec* er) { (*sendevent_vect)(f, er); }

static void sendevent_ivf(FILE* f, pa_evtrec* er)
    { error(esendevent_unimp); }

void _pa_title_ovr(pa_title_t nfp, pa_title_t* ofp)
    { *ofp = title_vect; title_vect = nfp; }
void pa_title(FILE* f, char* ts) { (*title_vect)(f, ts); }

static void title_ivf(FILE* f, char* ts)
    { error(etitle_unimp); }

void _pa_openwin_ovr(pa_openwin_t nfp, pa_openwin_t* ofp)
    { *ofp = openwin_vect; openwin_vect = nfp; }
void pa_openwin(FILE** infile, FILE** outfile, FILE* parent, int wid)
    { (*openwin_vect)(infile, outfile, parent, wid); }

static void openwin_ivf(FILE** infile, FILE** outfile, FILE* parent, int wid)
    { error(eopenwin_unimp); }

void _pa_buffer_ovr(pa_buffer_t nfp, pa_buffer_t* ofp)
    { *ofp = buffer_vect; buffer_vect = nfp; }
void pa_buffer(FILE* f, int e) { (*buffer_vect)(f, e); }

static void buffer_ivf(FILE* f, int e)
    { error(ebuffer_unimp); }

void _pa_getsiz_ovr(pa_getsiz_t nfp, pa_getsiz_t* ofp)
    { *ofp = getsiz_vect; getsiz_vect = nfp; }
void pa_getsiz(FILE* f, int* x, int* y) { (*getsiz_vect)(f, x, y); }

static void getsiz_ivf(FILE* f, int* x, int* y)
    { error(egetsiz_unimp); }

void _pa_setsiz_ovr(pa_setsiz_t nfp, pa_setsiz_t* ofp)
    { *ofp = setsiz_vect; setsiz_vect = nfp; }
void pa_setsiz(FILE* f, int x, int y) { (*setsiz_vect)(f, x, y); }

static void setsiz_ivf(FILE* f, int x, int y)
    { error(esetsiz_unimp); }

void _pa_setpos_ovr(pa_setpos_t nfp, pa_setpos_t* ofp)
    { *ofp = setpos_vect; setpos_vect = nfp; }
void pa_setpos(FILE* f, int x, int y) { (*setpos_vect)(f, x, y); }

static void setpos_ivf(FILE* f, int x, int y)
    { error(esetpos_unimp); }

void _pa_scnsiz_ovr(pa_scnsiz_t nfp, pa_scnsiz_t* ofp)
    { *ofp = scnsiz_vect; scnsiz_vect = nfp; }
void pa_scnsiz(FILE* f, int* x, int* y) { (*scnsiz_vect)(f, x, y); }

static void scnsiz_ivf(FILE* f, int* x, int* y)
    { error(escnsiz_unimp); }

void _pa_scncen_ovr(pa_scncen_t nfp, pa_scncen_t* ofp)
    { *ofp = scncen_vect; scncen_vect = nfp; }
void pa_scncen(FILE* f, int* x, int* y) { (*scncen_vect)(f, x, y); }

static void scncen_ivf(FILE* f, int* x, int* y)
    { error(escncen_unimp); }

void _pa_winclient_ovr(pa_winclient_t nfp, pa_winclient_t* ofp)
    { *ofp = winclient_vect; winclient_vect = nfp; }
void pa_winclient(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms)
    { (*winclient_vect)(f, cx, cy, wx, wy, ms); }

static void winclient_ivf(FILE* f, int cx, int cy, int* wx, int* wy, pa_winmodset ms)
    { error(ewinclient_unimp); }

void _pa_front_ovr(pa_front_t nfp, pa_front_t* ofp)
    { *ofp = front_vect; front_vect = nfp; }
void pa_front(FILE* f) { (*front_vect)(f); }

static void front_ivf(FILE* f)
    { error(efront_unimp); }

void _pa_back_ovr(pa_back_t nfp, pa_back_t* ofp)
    { *ofp = back_vect; back_vect = nfp; }
void pa_back(FILE* f) { (*back_vect)(f); }

static void back_ivf(FILE* f)
    { error(eback_unimp); }

void _pa_frame_ovr(pa_frame_t nfp, pa_frame_t* ofp)
    { *ofp = frame_vect; frame_vect = nfp; }
void pa_frame(FILE* f, int e) { (*frame_vect)(f, e); }

static void frame_ivf(FILE* f, int e)
    { error(eframe_unimp); }

void _pa_sizable_ovr(pa_sizable_t nfp, pa_sizable_t* ofp)
    { *ofp = sizable_vect; sizable_vect = nfp; }
void pa_sizable(FILE* f, int e) { (*sizable_vect)(f, e); }

static void sizable_ivf(FILE* f, int e)
    { error(esizable_unimp); }

void _pa_sysbar_ovr(pa_sysbar_t nfp, pa_sysbar_t* ofp)
    { *ofp = sysbar_vect; sysbar_vect = nfp; }
void pa_sysbar(FILE* f, int e) { (*sysbar_vect)(f, e); }

static void sysbar_ivf(FILE* f, int e)
    { error(esysbar_unimp); }

void _pa_menu_ovr(pa_menu_t nfp, pa_menu_t* ofp)
    { *ofp = menu_vect; menu_vect = nfp; }
void pa_menu(FILE* f, pa_menuptr m) { (*menu_vect)(f, m); }

static void menu_ivf(FILE* f, pa_menuptr m)
    { error(emenu_unimp); }

void _pa_menuena_ovr(pa_menuena_t nfp, pa_menuena_t* ofp)
    { *ofp = menuena_vect; menuena_vect = nfp; }
void pa_menuena(FILE* f, int id, int onoff) { (*menuena_vect)(f, id, onoff); }

static void menuena_ivf(FILE* f, int id, int onoff)
    { error(emenuena_unimp); }

void _pa_menusel_ovr(pa_menusel_t nfp, pa_menusel_t* ofp)
    { *ofp = menusel_vect; menusel_vect = nfp; }
void pa_menusel(FILE* f, int id, int select) { (*menusel_vect)(f, id, select); }

static void menusel_ivf(FILE* f, int id, int select)
    { error(emenusel_unimp); }

void _pa_stdmenu_ovr(pa_stdmenu_t nfp, pa_stdmenu_t* ofp)
    { *ofp = stdmenu_vect; stdmenu_vect = nfp; }
void pa_stdmenu(pa_stdmenusel sms, pa_menuptr* sm, pa_menuptr pm)
    { (*stdmenu_vect)(sms, sm, pm); }

static void stdmenu_ivf(pa_stdmenusel sms, pa_menuptr* sm, pa_menuptr pm)
    { error(estdmenu_unimp); }

void _pa_getwinid_ovr(pa_getwinid_t nfp, pa_getwinid_t* ofp)
    { *ofp = getwinid_vect; getwinid_vect = nfp; }
int pa_getwinid(void) { (*getwinid_vect)(); }

static int getwinid_ivf(void)
    { error(egetwinid_unimp); }

void _pa_focus_ovr(pa_focus_t nfp, pa_focus_t* ofp)
    { *ofp = focus_vect; focus_vect = nfp; }
void pa_focus(FILE* f) { (*focus_vect)(f); }

static void focus_ivf(FILE* f)
    { error(efocus_unimp); }

/*******************************************************************************

Module startup/shutdown

*******************************************************************************/

/** ****************************************************************************

Initialize output terminal

We initialize all variables and tables, then clear the screen to bring it to
a known state.

This is the startup routine for terminal, and is executed automatically
before the client program runs.

*******************************************************************************/

static void pa_init_terminal (void) __attribute__((constructor (102)));
static void pa_init_terminal()

{

    /** index for events */            pa_evtcod      e;
    /** build new terminal settings */ struct termios raw;
    /** index */                       int            i;
    /** digit count integer */         int            dci;
    /** joystick index */              int            ji;
    /** joystick file id */            int            joyfid;
    /** joystick device name */        char           joyfil[] = "/dev/input/js0";
    /** joystick parameter read */     char           jc;
    /** Linux return value */          int            r;

    /* set override vectors to defaults */
    cursor_vect =          cursor_ivf;
    cursor_vect =          cursor_ivf;
    maxx_vect =            maxx_ivf;
    maxy_vect =            maxy_ivf;
    home_vect =            home_ivf;
    del_vect =             del_ivf;
    up_vect =              up_ivf;
    down_vect =            down_ivf;
    left_vect =            left_ivf;
    right_vect =           right_ivf;
    blink_vect =           blink_ivf;
    reverse_vect =         reverse_ivf;
    underline_vect =       underline_ivf;
    superscript_vect =     superscript_ivf;
    subscript_vect =       subscript_ivf;
    italic_vect =          italic_ivf;
    bold_vect =            bold_ivf;
    strikeout_vect =       strikeout_ivf;
    standout_vect =        standout_ivf;
    fcolor_vect =          fcolor_ivf;
    bcolor_vect =          bcolor_ivf;
    auto_vect =            auto_ivf;
    curvis_vect =          curvis_ivf;
    scroll_vect =          scroll_ivf;
    curx_vect =            curx_ivf;
    cury_vect =            cury_ivf;
    curbnd_vect =          curbnd_ivf;
    select_vect =          select_ivf;
    event_vect =           event_ivf;
    timer_vect =           timer_ivf;
    killtimer_vect =       killtimer_ivf;
    mouse_vect =           mouse_ivf;
    mousebutton_vect =     mousebutton_ivf;
    joystick_vect =        joystick_ivf;
    joybutton_vect =       joybutton_ivf;
    joyaxis_vect =         joyaxis_ivf;
    settab_vect =          settab_ivf;
    restab_vect =          restab_ivf;
    clrtab_vect =          clrtab_ivf;
    funkey_vect =          funkey_ivf;
    frametimer_vect =      frametimer_ivf;
    autohold_vect =        autohold_ivf;
    wrtstr_vect =          wrtstr_ivf;
    wrtstrn_vect =         wrtstrn_ivf;
    eventover_vect =       eventover_ivf;
    eventsover_vect =      eventsover_ivf;
    sendevent_vect =       sendevent_ivf;
    title_vect =           title_ivf;
    openwin_vect =         openwin_ivf;
    buffer_vect =          buffer_ivf;
    sizbuf_vect =          sizbuf_ivf;
    getsiz_vect =          getsiz_ivf;
    setsiz_vect =          setsiz_ivf;
    setpos_vect =          setpos_ivf;
    scnsiz_vect =          scnsiz_ivf;
    scncen_vect =          scncen_ivf;
    winclient_vect =       winclient_ivf;
    front_vect =           front_ivf;
    back_vect =            back_ivf;
    frame_vect =           frame_ivf;
    sizable_vect =         sizable_ivf;
    sysbar_vect =          sysbar_ivf;
    menu_vect =            menu_ivf;
    menuena_vect =         menuena_ivf;
    menusel_vect =         menusel_ivf;
    stdmenu_vect =         stdmenu_ivf;
    getwinid_vect =        getwinid_ivf;
    focus_vect =           focus_ivf;

    /* turn off I/O buffering */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* change to alternate screen/turn off wrap */
    printf("\033[?1049h\033[H"); fflush(stdout);

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
//    ovr_unlink(iunlink, &ofpunlink);
    ovr_lseek(ilseek, &ofplseek);

    dmpevt  = DMPEVT;    /* dump Petit-Ami messages */

    /* set default screen geometry */
    dimx = DEFXD;
    dimy = DEFYD;

    /* try getting size from system */
    findsize();

    /* clear screens array */
    for (curupd = 1; curupd <= MAXCON; curupd++) screens[curupd-1] = NULL;
    /* allocate screen array */
    screens[0] = malloc(sizeof(scnrec)*dimy*dimx);

    /* set buffer size */
    bufx = dimx;
    bufy = dimy;

    /* alloocate tab array */
    tabs = malloc(sizeof(int)*dimx);

    curdsp = 1; /* set display current screen */
    curupd = 1; /* set current update screen */
    trm_wrapoff(); /* physical wrap is always off */
    scroll = 1; /* turn on virtual wrap */
    curon = 1; /* set default cursor on */
    trm_curon(); /* and make sure that is so */
    iniscn(screens[curdsp-1]); /* initalize screen */
    restore(screens[curdsp-1]); /* place on display */
    joyenb = JOYENB; /* enable joystick */
    inpptr = -1; /* set no input line active */
#ifdef ALLOWUTF8
    utf8cnt = 0; /* clear utf-8 character count */
#endif
    paqfre    = NULL;  /* clear pa event free queue */
    paqevt    = NULL;  /* clear pa event input queue */
    evtquecnt = 0;     /* clear event queue counter */
    evtquemax = 0;     /* clear event queue max */
    matrem    = 0;     /* set no duplicate removal */

    /* clear event vector table */
    evtshan = defaultevent;
    for (e = pa_etchar; e <= pa_etframe; e++) evthan[e] = defaultevent;

    /* clear keyboard match buffer */
    keylen = 0;

    /* set maximum power of 10 for conversions */
    maxpow10 = INT_MAX;
    dci = 0;
    while (maxpow10) { maxpow10 /= 10; dci++; }
    /* find top power of 10 */
    maxpow10 = 1;
    while (--dci) maxpow10 = maxpow10*10;

    /*
     * Set terminal in raw mode
     */
    tcgetattr(0,&trmsav); /* save original state of terminal */
    raw = trmsav; /* copy into new state */

    /* input modes - clear indicated ones giving: no break, no CR to NL,
       no parity check, no strip char, no start/stop output (sic) control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* output modes - clear giving: no post processing such as NL to CR+NL */
    raw.c_oflag &= ~(OPOST);

    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);

    /* local modes - clear giving: echoing off, canonical off (no erase with
       backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* add input file event */
    inpsev = system_event_addseinp(0);

    /* clear the timers table */
    for (i = 0; i < PA_MAXTIM; i++) timtbl[i] = 0;

    /* clear joystick table */
    for (ji = 0; ji < MAXJOY; ji++) joytab[ji] = NULL;

    strcpy(joyfil, "/dev/input/js0"); /* set name of joystick file */

    /* open joystick if available */
    numjoy = 0; /* set no joysticks */
    if (joyenb) { /* if joystick is to be enabled */

        do { /* find joysticks */

            joyfil[13] = numjoy+'0'; /* set number of joystick to find */
            joyfid = open(joyfil, O_RDONLY);
            if (joyfid >= 0) { /* found */

                /* get a joystick table entry */
                joytab[numjoy] = malloc(sizeof(joyrec));
                joytab[numjoy]->fid = joyfid; /* set fid */
                /* enable system event */
                joytab[numjoy]->sid = system_event_addseinp(joyfid);
                joytab[numjoy]->ax = 0; /* clear joystick axis saves */
                joytab[numjoy]->ay = 0;
                joytab[numjoy]->az = 0;
                joytab[numjoy]->a4 = 0;
                joytab[numjoy]->a5 = 0;
                joytab[numjoy]->a6 = 0;
                joytab[numjoy]->no = numjoy+1; /* set logical number */
#ifndef __MACH__ /* Mac OS X */
                /* get number of axes */
                ioctl(joyfid, JSIOCGAXES, &jc);
                joytab[numjoy]->axis = jc;
                /* get number of buttons */
                ioctl(joyfid, JSIOCGBUTTONS, &jc);
                joytab[numjoy]->button = jc;
#endif
                numjoy++; /* count joysticks */

            }

        } while (numjoy < MAXJOY && joyfid >= 0); /* no more joysticks */

    }

    /* clear tabs and set to 8ths */
    for (i = 1; i <= dimx; i++) tabs[i-1] = ((i-1)%8 == 0) && (i != 1);

    /* clear mouse state. Note mouse will be all buttons deasserted, and
       location 0,0 and thus offscreen. This is important, since in xterm we
       can't know if there is or is not a mouse, thus a missing mouse is dead
       and offscreen */
    button1 = nbutton1 = 1;
    button2 = nbutton2 = 1;
    button3 = nbutton3 = 1;
    mpx = nmpx = -INT_MAX;
    mpy = nmpy = -INT_MAX;

    /* now signal xterm we want all mouse events including all movements */
    putstr("\33[?1003h");
#ifdef MOUSESGR
    putstr("\33[?1006h"); /* enable SGR mouse mode (extended) */
#endif

    /* enable windows change signal */
    winchsev = system_event_addsesig(SIGWINCH);

    /* restore terminal state after flushing */
    tcsetattr(0,TCSAFLUSH,&raw);

    /* initialize the event tracking lock */
    r = pthread_mutex_init(&evtlock, NULL);
    if (r) linuxerror(r);

    /* initialize queue not empty semaphore */
    r = pthread_cond_init(&evtquene, NULL);
    if (r) linuxerror(r);

    /* start event thread */
    r = pthread_create(&eventthread, NULL, eventtask, NULL);
    if (r) linuxerror(r); /* error, print and exit */

}

/** ****************************************************************************

Deinitialize output terminal

Removes overrides. We check if the contents of the override vector have our
vectors in them. If that is not so, then a stacking order violation occurred,
and that should be corrected.

*******************************************************************************/

static void pa_deinit_terminal (void) __attribute__((destructor (102)));
static void pa_deinit_terminal()

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    punlink_t cppunlink;
    plseek_t cpplseek;

    /* restore cursor visible */
    trm_curon();

    /* restore terminal */
    tcsetattr(0,TCSAFLUSH,&trmsav);

    /* turn off mouse tracking */
    putstr("\33[?1003l");

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
//    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);
    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose /* || cppunlink != iunlink */ || cpplseek != ilseek)
        error(esystem);

    /* back to normal buffer on xterm */
    printf("\033[?1049l"); fflush(stdout);

}
