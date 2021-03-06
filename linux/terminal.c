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
* program. In the latter case, the mouse and joystick position is irrelevant,  *
* and we need to determine terminal geometry via ANSI sequences (yes, it is    *
* possible!).                                                                  *
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

/* The maximum dimensions are used to set the size of the holding arrays.
   These should be reasonable values to prevent becoming a space hog. */
#define MAXXD 250     /**< Maximum terminal size x */
#define MAXYD 250     /**< Maximum terminal size y */

#define MAXCON  10    /**< number of screen contexts */
#define MAXLIN  250   /* maximum length of input buffered line */
#define MAXFKEY 10    /**< maximum number of function keys */
#define MAXJOY  10    /* number of joysticks possible */
#define DMPEVT  FALSE /* enable dump Petit-Ami messages */

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

      /* character at location */        char ch;
      /* foreground color at location */ pa_color forec;
      /* background color at location */ pa_color backc;
      /* active attribute at location */ scnatt attr;

} scnrec;
typedef scnrec scnbuf[MAXYD][MAXXD];
/** Screen context */
typedef struct { /* screen context */

      /* screen buffer */                    scnbuf buf;
      /* current cursor location x */        int curx;
      /* current cursor location y */        int cury;
      /* current writing foreground color */ pa_color forec;
      /* current writing background color */ pa_color backc;
      /* current writing attribute */        scnatt attr;
      /* current status of scroll */         int scroll;
      /* current status of cursor visible */ int curvis;
      /* tabbing array */                    int tabs[MAXXD];

} scncon;
/** pointer to screen context block */ typedef scncon* scnptr;

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
    "\33[M",                /* mouse move */
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

/* screen contexts array */               static scnptr screens[MAXCON];
/* index for current display screen */    static int curdsp;
/* index for current update screen */     static int curupd;
/* array of event handler routines */     static pa_pevthan evthan[pa_etframe+1];
/* single master event handler routine */ static pa_pevthan evtshan;

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

/**
 * Save for terminal status
 */
static struct termios trmsav;

/**
 * Active timers table
 */
static int timtbl[PA_MAXTIM];

/**
 * Key matching input buffer
 */
static char   keybuf[10]; /* buffer */
static int    keylen;      /* number of characters in buffer */
static int    tabs[MAXXD]; /* tabs set */
static int    dimx;        /* actual width of screen */
static int    dimy;        /* actual height of screen */
static int    curon;       /* current on/off state of cursor */
static int    curx;        /* cursor position on screen */
static int    cury;
static int    curval; /* physical cursor position valid */
/* global scroll enable. This does not reflect the physical state, we never
   turn on automatic scroll. */
static int    scroll;
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
/* maximum power of 10 in integer */
static int    maxpow10;
static int    numjoy;      /* number of joysticks found */
static int    joyenb;      /* enable joysticks */
static int    frmfid;      /* framing timer fid */
char          inpbuf[MAXLIN];  /* input line buffer */
int           inpptr;          /* input line index */
static joyptr joytab[MAXJOY]; /* joystick control table */
static int    dmpevt;      /* enable dump Petit-Ami messages */
static int    inpsev;      /* keyboard input system event number */
static int    frmsev;      /* frame timer system event number */
static int    winchsev;    /* windows change system event number */

/* forwards */
static void restore(scnptr sc);

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
        case esystem: fprintf(stderr, "System fault"); break;

    }
    fprintf(stderr, "\n");

    exit(1);

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
        case pa_etframe:   fprintf(stderr, "etframe"); break;

        default: fprintf(stderr, "???");

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

static void putchr(char c)

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

static void putstr(char *s)


{

    /** index for string */ int i;

    while (*s) putchr(*s++); /* output characters */

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

            ev->etype = pa_etresize;
            evtfnd = 1;
            /* save current window size */
            dimxs = dimx;
            dimys = dimy;
            /* recalculate window size */
            findsize();
            /* now find if we have exposed any new areas, then redraw if so */
            if (dimx > dimxs || dimy > dimys) restore(screens[curdsp-1]);

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

    putstr("\33[");
    /* override "bright" black, which is more like grey */
    if (c == pa_black) wrtint(ANSIFORECOLORBASE+colnum(c));
    else wrtint(FORECOLORBASE+colnum(c));
    putstr("m");

}

/** set background color */
static void trm_bcolor(pa_color c)

{

    putstr("\33[");
    /* override "bright" black, which is more like grey */
    if (c == pa_black) wrtint(ANSIBACKCOLORBASE+colnum(c));
    else wrtint(BACKCOLORBASE+colnum(c));
    putstr("m");

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

            trm_fcolor(sc->forec); /* set current colors */
            trm_bcolor(sc->backc);

        }

    }

}

/*******************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns true if so.

*******************************************************************************/

int icurbnd(scnptr sc)

{

   return (sc->curx >= 1) && (sc->curx <= dimx) &&
          (sc->cury >= 1) && (sc->cury <= dimy);

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

        cv = sc->curvis; /* set current buffer status */
        if (!icurbnd(sc)) cv = 0; /* not in bounds, force off */
        if (cv != curon) { /* not already at the desired state */

            if (cv) {

                trm_curon();
                curon = 1;

            } else {

                trm_curoff();
                curon = 0;

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
            if ((sc->curx != curx || sc->cury != cury) && curval) {

                /* Cursor position and actual don't match. Try some optimized
                   cursor positions to reduce bandwidth. Note we don't count on
                   real terminal behavior at the borders. */
                if (sc->curx == 1 && sc->cury == 1) trm_home();
                else if (sc->curx == curx && sc->cury == cury-1) trm_up();
                else if (sc->curx == curx && sc->cury == cury+1) trm_down();
                else if (sc->curx == curx-1 && sc->cury == cury) trm_left();
                else if (sc->curx == curx+1 && sc->cury == cury) trm_right();
                else if (sc->curx == 1 && sc->cury == cury) putchr('\r');
                else trm_cursor(sc->curx, sc->cury);
                curx = sc->curx;
                cury = sc->cury;
                curval = 1;

            } else {

                /* don't count on physical cursor location, just reset */
                trm_cursor(sc->curx, sc->cury);
                curx = sc->curx;
                cury = sc->cury;
                curval = 1;

            }

        }
        cursts(sc); /* set new cursor status */

    }

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
    for (y = 1;  y <= MAXYD; y++)
        for (x = 1; x <= MAXXD; x++) {

        sp = &sc->buf[y-1][x-1];
        sp->ch = ' '; /* clear to spaces */
        /* colors and attributes to the set for that screen */
        sp->forec = sc->forec;
        sp->backc = sc->backc;
        sp->attr = sc->attr;

    }

}

/** ****************************************************************************

Initialize screen

Clears all the parameters in the present screen context, and updates the
display to match.

*******************************************************************************/

static void iniscn(scnptr sc)

{

    sc->cury = 1; /* set cursor at home */
    sc->curx = 1;
    /* these attributes and colors are pretty much windows 95 specific. The
       Bizarre setting of "blink" actually allows access to bright white */
    sc->forec = pa_black; /* set colors and attributes */
    sc->backc = pa_white;
    sc->attr = sanone;
    sc->curvis = curon; /* set cursor visible from curent state */
    sc->scroll = scroll; /* set autoscroll from global state */
    clrbuf(sc); /* clear screen buffer with that */

}

/** ****************************************************************************

Restore screen

Updates all the buffer and screen parameters to the terminal.

*******************************************************************************/

static void restore(scnptr sc)

{

    /** screen indexes */         int xi, yi;
    /** color saves */            pa_color fs, bs;
    /** attribute saves */        scnatt as;
    /** screen element pointer */ scnrec *p;

    trm_home(); /* restore cursor to upper left to start */
    /* set colors and attributes */
    trm_fcolor(sc->forec); /* restore colors */
    trm_bcolor(sc->backc);
    setattr(sc, sc->attr); /* restore attributes */
    fs = sc->forec; /* save current colors and attributes */
    bs = sc->backc;
    as = sc->attr;
    /* copy buffer to screen */
    for (yi = 1; yi <= dimy; yi++) { /* lines */

        for (xi = 1; xi <= dimx; xi++) { /* characters */

            /* for each new character, we compare the attributes and colors
               with what is set. if a new color or attribute is called for,
               we set that, and update the saves. this technique cuts down on
               the amount of output characters */
            p = &(sc->buf[yi-1][xi-1]); /* index this screen element */
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
            putchr(p->ch); /* now output the actual character */

        };
        if (yi < dimy)
            /* output next line sequence on all lines but the last. this is
               because the last one would cause us to scroll */
            putstr("\r\n");

    };
    /* restore cursor position */
    trm_cursor(sc->curx, sc->cury);
    curx = sc->curx; /* set physical cursor */
    cury = sc->cury;
    curval = 1; /* set it is valid */
    trm_fcolor(sc->forec); /* restore colors */
    trm_bcolor(sc->backc);
    setattr(sc, sc->attr); /* restore attributes */
    setcur(sc); /* set cursor status */

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
        for (x = 1; x <= dimx; x++) fprintf(stderr, "%c", sc->buf[y-1][x-1].ch);
        fprintf(stderr, "\"\n");

    }

}

static void iscroll(scnptr sc, int x, int y)

{

    int     xi, yi;    /* screen counters */
    pa_color   fs, bs; /* color saves */
    scnatt  as;        /* attribute saves */
    scnbuf  scnsav;    /* full screen buffer save */
    int     lx;        /* last unmatching character index */
    int     m;         /* match flag */
    scnrec* sp;        /* pointer to screen record */

    if (y > 0 && x == 0) {

        if (indisp(sc)) { /* in display */

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
            trm_cursor(sc->curx, sc->cury);

        }
        /* now, adjust the buffer to be the same */
        for (yi = 1; yi <= dimy-1; yi++) /* move any lines up */
            if (yi+y <= dimy) /* still within buffer */
                /* move lines up */
                    memcpy(sc->buf[yi-1],
                           sc->buf[yi+y-1],
                           MAXXD * sizeof(scnrec));
        for (yi = dimy-y+1; yi <= dimy; yi++) /* clear blank lines at end */
            for (xi = 1; xi <= dimx; xi++) {

            sp = &sc->buf[yi-1][xi-1];
            sp->ch = ' ';   /* clear to blanks at colors and attributes */
            sp->forec = sc->forec;
            sp->backc = sc->backc;
            sp->attr = sc->attr;

        }

    } else { /* odd direction scroll */

        /* when the scroll is arbitrary, we do it by completely refreshing the
           contents of the screen from the buffer */
        if (x <= -dimx || x >= dimx || y <= -dimy || y >= dimy) {

            /* scroll would result in complete clear, do it */
            trm_clear();   /* scroll would result in complete clear, do it */
            clrbuf(sc);   /* clear the screen buffer */
            /* restore cursor positition */
            trm_cursor(sc->curx, sc->cury);

        } else { /* scroll */

            /* true scroll is done in two steps. first, the contents of the buffer
               are adjusted to read as after the scroll. then, the contents of the
               buffer are output to the terminal. before the buffer is changed,
               we perform a full save of it, which then represents the "current"
               state of the real terminal. then, the new buffer contents are
               compared to that while being output. this saves work when most of
               the screen is spaces anyways */

            /* save the entire buffer */
            memcpy(scnsav, sc->buf, sizeof(scnbuf));
            if (y > 0) {  /* move text up */

                for (yi = 1; yi < dimy; yi++) /* move any lines up */
                    if (yi + y <= dimy) /* still within buffer */
                        /* move lines up */
                        memcpy(sc->buf[yi-1],
                               sc->buf[yi+y-1],
                               MAXXD*sizeof(scnrec));
                for (yi = dimy-y+1; yi <= dimy; yi++)
                    /* clear blank lines at end */
                    for (xi = 1; xi <= dimx; xi++) {

                    sp = &sc->buf[yi-1][xi-1];
                    sp->ch = ' ';   /* clear to blanks at colors and attributes */
                    sp->forec = sc->forec;
                    sp->backc = sc->backc;
                    sp->attr = sc->attr;

                }

            } else if (y < 0) { /* move text down */

                for (yi = dimy; yi >= 2; yi--)   /* move any lines up */
                    if (yi + y >= 1) /* still within buffer */
                        /* move lines up */
                        memcpy(sc->buf[yi-1],
                               sc->buf[yi+y-1],
                               MAXXD * sizeof(scnrec));
                for (yi = 1; yi <= abs(y); yi++) /* clear blank lines at start */
                    for (xi = 1; xi <= dimx; xi++) {

                    sp = &sc->buf[yi-1][xi-1];
                    /* clear to blanks at colors and attributes */
                    sp->ch = ' ';
                    sp->forec = sc->forec;
                    sp->backc = sc->backc;
                    sp->attr = sc->attr;

                }

            }
            if (x > 0) { /* move text left */
                for (yi = 1; yi <= dimy; yi++) { /* move text left */

                    for (xi = 1; xi <= dimx-1; xi++) /* move left */
                        if (xi+x <= dimx) /* still within buffer */
                            /* move characters left */
                            sc->buf[yi-1][xi-1] =
                                sc->buf[yi-1][xi+x-1];
                    /* clear blank spaces at right */
                    for (xi = dimx-x+1; xi <= dimx; xi++) {

                        sp = &sc->buf[yi-1][xi-1];
                        /* clear to blanks at colors and attributes */
                        sp->ch = ' ';
                        sp->forec = sc->forec;
                        sp->backc = sc->backc;
                        sp->attr = sc->attr;

                    }

                }

            } else if (x < 0) { /* move text right */

                for (yi = 1; yi <= dimy; yi++) { /* move text right */

                    for (xi = dimx; xi >= 2; xi--) /* move right */
                        if (xi+x >= 1) /* still within buffer */
                            /* move characters left */
                            sc->buf[yi-1][xi-1] =
                                sc->buf[yi-1][xi+x-1];
                    /* clear blank spaces at left */
                    for (xi = 1; xi <= abs(x); xi++) {

                        sp = &sc->buf[yi-1][xi-1];
                        sp->ch = ' ';   /* clear to blanks at colors and attributes */
                        sp->forec = sc->forec;
                        sp->backc = sc->backc;
                        sp->attr = sc->attr;

                    }

                }

            }
            if (indisp(sc)) { /* in display */

                /* the buffer is adjusted. now just copy the complete buffer to the
                   screen */
                trm_home(); /* restore cursor to upper left to start */
                fs = sc->forec; /* save current colors and attributes */
                bs = sc->backc;
                as = sc->attr;
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
                        if (sc->buf[yi-1][lx-1].ch != scnsav[yi-1][lx-1].ch)
                            m = 0;
                        if (sc->buf[yi-1][lx-1].forec !=
                            scnsav[yi-1][lx-1].forec) m = 0;
                        if (sc->buf[yi-1][lx-1].backc !=
                            scnsav[yi-1][lx-1].backc) m = 0;
                        if (sc->buf[yi-1][lx-1].attr !=
                            scnsav[yi-1][lx-1].attr)  m = 0;
                        if (m) lx--; /* next character */

                    } while (m && lx); /* until match or no more */
                    for (xi = 1; xi <= lx; xi++) { /* characters */

                        /* for each new character, we compare the attributes and colors
                           with what is set. if a new color or attribute is called for,
                           we set that, and update the saves. this technique cuts down on
                           the amount of output characters */
                        sp = &sc->buf[yi-1][xi-1];
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
                        putchr(sp->ch);

                    }
                    if (yi < dimy)
                        /* output next line sequence on all lines but the last. this is
                           because the last one would cause us to scroll */
                        putstr("\r\n");

                }
                /* restore cursor position */
                trm_cursor(sc->curx, sc->cury);
                trm_fcolor(sc->forec);   /* restore colors */
                trm_bcolor(sc->backc);   /* restore attributes */
                setattr(sc, sc->attr);

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
    sc->cury = 1; /* set cursor at home */
    sc->curx = 1;
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

    sc->cury = y; /* set new position */
    sc->curx = x;
    setcur(sc);

}

/** ****************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

static void iup(scnptr sc)

{

    if (sc->scroll) { /* autowrap is on */

        if (sc->cury > 1) /* not at top of screen */
            sc->cury = sc->cury-1; /* update position */
        else if (sc->scroll) /* scroll enabled */
            iscroll(sc, 0, -1); /* at top already, scroll up */
        else /* wrap cursor around to screen bottom */
            sc->cury = dimy; /* set new position */

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (sc->cury > -INT_MAX) sc->cury--;
    setcur(sc);

}

/** ****************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

static void idown(scnptr sc)

{

    if (sc->scroll) { /* autowrap is on */

        if (sc->cury < dimy) /* not at bottom of screen */
            sc->cury = sc->cury+1; /* update position */
        else if (sc->scroll) /* wrap enabled */
            iscroll(sc, 0, +1); /* already at bottom, scroll down */
        else /* wrap cursor around to screen top */
            sc->cury = 1; /* set new position */

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (sc->cury < INT_MAX) sc->cury++;
    setcur(sc);

}

/** ****************************************************************************

Move cursor left internal

Moves the cursor one character left.

*******************************************************************************/

static void ileft(scnptr sc)

{

    if (sc->scroll) { /* autowrap is on */

        if (sc->curx > 1)  /* not at extreme left */
            sc->curx = screens[curupd-1]->curx-1; /* update position */
        else { /* wrap cursor motion */

            iup(sc); /* move cursor up one line */
            sc->curx = dimx; /* set cursor to extreme right */

        }

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (sc->curx > -INT_MAX) sc->curx--;
    setcur(sc);

}

/** ****************************************************************************

Move cursor right internal

Moves the cursor one character right.

*******************************************************************************/

static void iright(scnptr sc)

{

    if (sc->scroll) { /* autowrap is on */

        if (sc->curx < dimx) /* not at extreme right */
            sc->curx = sc->curx+1; /* update position */
        else { /* wrap cursor motion */

            idown(sc); /* move cursor up one line */
            sc->curx = 1; /* set cursor to extreme left */

        }

    } else /* autowrap is off */
        /* prevent overflow, but otherwise its unlimited */
        if (sc->curx < INT_MAX) sc->curx++;
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

static void plcchr(scnptr sc, char c)

{

    scnrec* p;
    int i;

    /* handle special character cases first */
    if (c == '\r') /* carriage return, position to extreme left */
        icursor(sc, 1, screens[curupd-1]->cury);
    else if (c == '\n') {

        /* line end */
        idown(sc); /* line feed, move down */
        /* position to extreme left */
        icursor(sc, 1, sc->cury);

    } else if (c == '\b') ileft(sc); /* back space, move left */
    else if (c == '\f') iclear(sc); /* clear screen */
    else if (c == '\t') {

        /* find next tab position */
        i = sc->curx+1; /* find current x +1 */
        while (i < dimx && !tabs[i-1]) i++;
        if (tabs[i-1]) /* we found a tab */
           while (sc->curx < i) iright(sc);

    } else if (c >= ' ' && c != 0x7f) {

        /* normal character case, not control character */
        if (sc->curx >= 1 && sc->curx <= MAXXD &&
            sc->cury >= 1 && sc->cury <= MAXYD) {

            /* within the buffer space, otherwise just dump */
            p = &sc->buf[sc->cury-1][sc->curx-1];
            p->ch = c; /* place character */
            p->forec = sc->forec; /* place colors */
            p->backc = sc->backc;
            p->attr = sc->attr; /* place attribute */

        }
        if (icurbnd(sc) && indisp(sc)) {

            /* This handling is from iright. We do this here because
               placement implicitly moves the cursor */
            putchr(c); /* output character to terminal */
            /* at right side, don't count on the screen wrap action */
            if (curx == dimx) curval = 0;
            else curx++; /* update physical cursor */
            if (sc->scroll) { /* autowrap is on */

                if (sc->curx < dimx) /* not at extreme right */
                    sc->curx = sc->curx+1; /* update position */
                else { /* wrap cursor motion */

                    idown(sc); /* move cursor down one line */
                    sc->curx = 1; /* set cursor to extreme left */

                }

            } else {/* autowrap is off */

                /* prevent overflow, but otherwise its unlimited */
                if (sc->curx < INT_MAX) sc->curx++;
                /* don't count on physical cursor behavior if scrolling is
                   off and we are at extreme right */
                curval = 0;

            }
            setcur(sc); /* update physical cursor */

        } else iright(sc); /* move right */

    }

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editing. The line is placed into the
input line buffer.

The upgrade for this is to implement a full set of editing features.

*******************************************************************************/

static void readline(void)

{

    pa_evtrec er; /* event record */

    inpptr = 0; /* set 1st character position */
    do { /* get line characters */

        /* get events until an "interesting" event occurs */
        do { ievent(&er); }
        while (er.etype != pa_etchar && er.etype != pa_etenter &&
               er.etype != pa_etterm && er.etype != pa_etdelcb);
        /* if the event is line enter, place newline code,
           otherwise place real character. if the event is program terminate,
           we execute an organized halt */
        switch (er.etype) { /* event */

            case pa_etterm: exit(1); /* halt program */
            case pa_etenter: /* line terminate */
                inpbuf[inpptr] = '\n'; /* return newline */
                /* terminate the line for debug prints */
                inpbuf[inpptr+1] = 0;
                plcchr(screens[curupd-1], '\r'); /* output newline sequence */
                plcchr(screens[curupd-1], '\n');
                break;
            case pa_etchar: /* character */
                if (inpptr < MAXLIN) {

                    inpbuf[inpptr++] = er.echar; /* place real character */
                    /* echo the character */
                    plcchr(screens[curupd-1], er.echar);

                }
                break;
            case pa_etdelcb: /* delete character backwards */
                if (inpptr > 0) { /* not at extreme left */

                    /* backspace, spaceout then backspace again */
                    plcchr(screens[curupd-1], '\b');
                    plcchr(screens[curupd-1], ' ');
                    plcchr(screens[curupd-1], '\b');
                    inpptr--; /* back up pointer */

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
    char *p = (char *)buff;
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

void pa_cursor(FILE *f, int x, int y)

{

    icursor(screens[curupd-1], x, y); /* position cursor */

}

/*******************************************************************************

Find if cursor is in screen bounds

This is the external interface to curbnd.

*******************************************************************************/

int pa_curbnd(FILE *f)

{

   return icurbnd(screens[curupd-1]);

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int pa_maxx(FILE *f)

{

    return dimx; /* set maximum x */

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int pa_maxy(FILE *f)

{

    return dimy; /* set maximum y */

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void pa_home(FILE *f)

{

    screens[curupd-1]->cury = 1; /* set cursor at home */
    screens[curupd-1]->curx = 1;
    setcur(screens[curupd-1]);

}

/** ****************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void pa_del(FILE* f)

{

    ileft(screens[curupd-1]); /* back up cursor */
    plcchr(screens[curupd-1], ' '); /* blank out */
    ileft(screens[curupd-1]); /* back up again */

}

/** ****************************************************************************

Move cursor up

This is the external interface to up.

*******************************************************************************/

void pa_up(FILE *f)

{

    iup(screens[curupd-1]); /* move up */

}


/** ****************************************************************************

Move cursor down

This is the external interface to down.

*******************************************************************************/

void pa_down(FILE *f)

{

    idown(screens[curupd-1]); /* move cursor down */

}

/** ****************************************************************************

Move cursor left

This is the external interface to left.

*******************************************************************************/

void pa_left(FILE *f)

{

   ileft(screens[curupd-1]); /* move cursor left */

}

/** ****************************************************************************

Move cursor right

This is the external interface to right.

*******************************************************************************/

void pa_right(FILE *f)

{

    iright(screens[curupd-1]); /* move cursor right */

}

/** ****************************************************************************

Turn on blink attribute

Turns on/off the blink attribute. Note that under windows 95 in a shell window,
blink does not mean blink, but instead "bright". We leave this alone because
we are supposed to also work over a com interface.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void pa_blink(FILE *f, int e)

{

    setattr(screens[curupd-1], sanone); /* turn off attributes */
    if (e) { /* reverse on */

        screens[curupd-1]->attr = sablink; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    } else { /* turn it off */

        screens[curupd-1]->attr = sanone; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    }

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void pa_reverse(FILE *f, int e)

{

    setattr(screens[curupd-1], sanone); /* turn off attributes */
    if (e) { /* reverse on */

        screens[curupd-1]->attr = sarev; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    } else { /* turn it off */

        screens[curupd-1]->attr = sanone; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    }

}

/** ****************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void pa_underline(FILE *f, int e)

{

    setattr(screens[curupd-1], sanone); /* turn off attributes */
    if (e) { /* underline on */

        screens[curupd-1]->attr = saundl; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    } else { /* turn it off */

        screens[curupd-1]->attr = sanone; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    }

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_superscript(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_subscript(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_italic(FILE *f, int e)

{

    setattr(screens[curupd-1], sanone); /* turn off attributes */
    if (e) { /* italic on */

        screens[curupd-1]->attr = saital; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    } else { /* turn it off */

        screens[curupd-1]->attr = sanone; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    }

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void pa_bold(FILE *f, int e)

{

    setattr(screens[curupd-1], sanone); /* turn off attributes */
    if (e) { /* bold on */

        screens[curupd-1]->attr = sabold; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    } else { /* turn it off */

        screens[curupd-1]->attr = sanone; /* set attribute active */
        /* set current attribute */
        setattr(screens[curupd-1], screens[curupd-1]->attr);
        if (curupd == curdsp) { /* in display */

            trm_fcolor(screens[curupd-1]->forec); /* set current colors */
            trm_bcolor(screens[curupd-1]->backc);

        }

    }

}

/** ****************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.

Not implemented.

*******************************************************************************/

void pa_strikeout(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void pa_standout(FILE *f, int e)

{

    pa_reverse(f, e); /* implement as reverse */

}

/** ****************************************************************************

Set foreground color

Sets the foreground (text) color from the universal primary code.

*******************************************************************************/

void pa_fcolor(FILE *f, pa_color c)

{

    if (curupd == curdsp) trm_fcolor(c); /* set color */
    screens[curupd-1]->forec = c;

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void pa_bcolor(FILE *f, pa_color c)

{

    if (curupd == curdsp) trm_bcolor(c); /* set color */
    screens[curupd-1]->backc = c;

}

/** ****************************************************************************

Enable/disable automatic scroll

Enables or disables automatic screen scroll. With automatic scroll on, moving
off the screen at the top or bottom will scroll up or down, respectively.

*******************************************************************************/

void pa_auto(FILE *f, int e)

{

    screens[curupd-1]->scroll = e; /* set line wrap status */
    if (indisp(screens[curupd-1])) scroll = e; /* also global status */

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void pa_curvis(FILE *f, int e)

{

    screens[curupd-1]->curvis = !!e; /* set cursor visible status */
    if (e) trm_curon(); else trm_curoff();

}

/** ****************************************************************************

Scroll screen

Process full delta scroll on screen. This is the external interface to this
int.

*******************************************************************************/

void pa_scroll(FILE *f, int x, int y)

{

    iscroll(screens[curupd-1], x, y); /* process scroll */

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int pa_curx(FILE *f)

{

    return screens[curupd-1]->curx; /* return current location x */

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int pa_cury(FILE *f)

{

    return screens[curupd-1]->cury; /* return current location y */

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

void pa_select(FILE *f, int u, int d)

{

    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error(einvscn); /* invalid screen number */
    if (curupd != u) { /* update screen changes */

        curupd = u; /* change to new screen */
        if (!screens[curupd-1]) { /* no screen allocated there */

            /* get a new screen context */
            screens[curupd-1] = (scncon*)malloc(sizeof(scncon));
            iniscn(screens[curupd-1]); /* initalize that */

        }

    }
    if (curdsp != d) { /* display screen changes */

        curdsp = d; /* change to new screen */
        if (screens[curdsp-1]) /* no screen allocated there */
            restore(screens[curdsp-1]); /* restore current screen */
        else { /* no current screen, create a new one */

            /* get a new screen context */
            screens[curdsp-1] = (scncon*)malloc(sizeof(scncon));
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

void pa_event(FILE* f, pa_evtrec *er)

{

    static int ecnt = 0; /* PA event counter */

    do { /* loop handling via event vectors */

        /* get next input event */
        ievent(er);
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

        dbg_printf(dlinfo, "PA Event: %5d Window: %d ", ecnt++, er->winid);
        prtevt(er->etype); fprintf(stderr, "\n"); fflush(stderr);

    }

}

/** ****************************************************************************

Set timer

*******************************************************************************/

void pa_timer(/* file to send event to */              FILE *f,
              /* timer handle */                       int i,
              /* number of 100us counts */             int t,
              /* timer is to rerun after completion */ int r)

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

void pa_killtimer(/* file to kill timer on */ FILE *f,
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

int pa_mouse(FILE *f)

{

    return 1; /* set 1 mouse */

}

/** ****************************************************************************

Returns number of buttons on a mouse

Returns the number of buttons implemented on a given mouse. With xterm we have
to assume 3 buttons.

*******************************************************************************/

int pa_mousebutton(FILE *f, int m)

{

    return 3; /* set three buttons */

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

int pa_joystick(FILE *f)

{

    return (numjoy); /* set number of joysticks */

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

int pa_joybutton(FILE *f, int j)

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

int pa_joyaxis(FILE *f, int j)

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

void pa_settab(FILE* f, int t)

{

    if (t < 1 || t > dimx) error(einvtab); /* invalid tab position */
    tabs[t-1] = 1; /* set tab position */

}

/** ****************************************************************************

restab

Resets a tab. The tab number t is 1 to n, and indicates the column for the tab.

*******************************************************************************/

void pa_restab(FILE* f, int t)

{

    if (t < 1 || t > dimx) error(einvtab); /* invalid tab position */
    tabs[t-1] = 0; /* reset tab position */

}

/** ****************************************************************************

clrtab

Clears all tabs.

*******************************************************************************/

void pa_clrtab(FILE* f)

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

int pa_funkey(FILE* f)

{

    return MAXFKEY;

}

/** ****************************************************************************

Frametimer

Enables or disables the framing timer.

Not currently implemented.

*******************************************************************************/

void pa_frametimer(FILE* f, int e)

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

void pa_autohold(FILE* f, int e)

{

}

/** ****************************************************************************

Write string direct

Writes a string direct to the terminal, bypassing character handling.

*******************************************************************************/

void pa_wrtstr(FILE* f, char *s)

{

    putstr(s);

}

/** ****************************************************************************

Write string direct with length

Writes a string with length direct to the terminal, bypassing character
handling.

*******************************************************************************/

void pa_wrtstrn(FILE* f, char *s, int n)

{

    while (n--) putchr(*s++);

}

/** ****************************************************************************

Override event handler

Overrides or "hooks" the indicated event handler. The existing even handler is
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

    dmpevt    = DMPEVT;    /* dump Petit-Ami messages */

    /* set default screen geometry */
    dimx = DEFXD;
    dimy = DEFYD;

    /* try getting size from system */
    findsize();

    /* clear screens array */
    for (curupd = 1; curupd <= MAXCON; curupd++) screens[curupd-1] = 0;
    screens[0] = (scncon*)malloc(sizeof(scncon)); /* get the default screen */
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

    /* enable windows change signal */
    winchsev = system_event_addsesig(SIGWINCH);

    /* restore terminal state after flushing */
    tcsetattr(0,TCSAFLUSH,&raw);

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
