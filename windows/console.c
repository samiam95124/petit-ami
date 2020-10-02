/*******************************************************************************
*                                                                              *
*                     TRANSPARENT SCREEN CONTROL MODULE                        *
*                          FOR WINDOWS CONSOLE MODE                            *
*                                                                              *
*                             2020 S. A. Franco                                *
*                                                                              *
* This module implements the standard terminal calls for windows console       *
* mode.                                                                        *
*                                                                              *
* Windows console mode is fully buffered, with multiple buffering and buffer   *
* to display switching, with buffer parameters stored in each buffer. Because  *
* of this, we let Windows manage the buffer operations, and mostly just        *
* reformat our calls to console mode.                                          *
*                                                                              *
* 2005/04/04 When running other tasks in the same console session from an      *
* exec, the other program moves the console position, but we don't see that,   *
* because we keep our own position. This is a side consequence of using the    *
* Windows direct console calls, which all require the write position to be     *
* specified. One way might be to return to using file I/O calls. However, we   *
* use the call "getpos" to reload Windows idea of the console cursor location  *
* any time certain events occur. These events are:                             *
*                                                                              *
* 1. Writing of characters to the console buffer.                              *
*                                                                              *
* 2. Relative positioning (up/down/left/right).                                *
*                                                                              *
* 3. Reading the position.                                                     *
*                                                                              *
* This should keep conlib in sync with any changes in the Windows console,     *
* with the price that it slows down the execution. However, in character mode  *
* console, this is deemed acceptable. Note that we ignore any other tasks      *
* changes of attributes and colors.                                            *
*                                                                              *
* Its certainly possible that our output, mixed with others, can be chaotic,   *
* but this happens in serial implementations as well. Most dangerous is our    *
* separation of character placement and move right after it. A character       *
* could come in the middle of that sequence.                                   *
*                                                                              *
* Remaining to be done:                                                        *
*                                                                              *
* 1. Make sure new buffers get proper coloring.                                *
*                                                                              *
* 2. We are getting "button 0" messages from the joystick on deassert during   *
*     fast repeated pushes.                                                    *
*                                                                              *
*                            BSD LICENSE INFORMATION                           *
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

#include <sys/types.h>
#include <stdio.h>
#include <windows.h>
#include <terminal.h>

/* standard file handles */
#define INPFIL 1   /* _input */
#define OUTFIL 2   /* _output */
#define MAXLIN 250 /* maximum length of input buffered line */
#define MAXCON 10  /* number of screen contexts */
#define MAXTAB 250 /* maximum number of tabs (length of buffer in x) */
#define FRMTIM 11  /* handle number of framing timer */

/* special user events */
#define UIV_TIM            0x8000 /* timer matures */
#define UIV_JOY1MOVE       0x4001 /* joystick 1 move */
#define UIV_JOY1ZMOVE      0x4002 /* joystick 1 z move */
#define UIV_JOY2MOVE       0x2002 /* joystick 2 move */
#define UIV_JOY2ZMOVE      0x2004 /* joystick 2 z move */
#define UIV_JOY1BUTTONDOWN 0x1000 /* joystick 1 button down */
#define UIV_JOY2BUTTONDOWN 0x0800 /* joystick 2 button down */
#define UIV_JOY1BUTTONUP   0x0400 /* joystick 1 button up */
#define UIV_JOY2BUTTONUP   0x0200 /* joystick 2 button up */
#define UIV_TERM           0x0100 /* terminate program */

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
    /* bold text */                  sabold,
    /* strikeout text */             sastkout

} scnatt;

typedef struct { /* screen context */

    HANDLE   han;         /* screen buffer handle */
    int      maxx;        /* maximum x */
    int      maxy;        /* maximum y */
    int      curx;        /* current cursor location x */
    int      cury;        /* current cursor location y */
    int      conx;        /* windows console version of x */
    int      cony;        /* windows console version of y */
    int      curv;        /* cursor visible */
    pa_color forec;       /* current writing foreground color */
    pa_color backc;       /* current writing background color */
    scnatt   attr;        /* current writing attribute */
    int      autof;       /* current status of scroll and wrap */
    int      tab[MAXTAB]; /* tabbing array */
    int      sattr;       /* current character attributes */

} scncon, *scnptr;

/* error code this module */
typedef enum {

    eftbful, /* file table full */
    ejoyacc, /* joystick access */
    etimacc, /* timer access */
    efilopr, /* cannot perform operation on special file */
    efilzer, /* filename is empty */
    einvscn, /* invalid screen number */
    einvhan, /* invalid handle */
    einvtab, /* invalid tab position */
    esbfcrt, /* cannot create screen buffer */
    ejoyqry, /* Could not get information on joystick */
    einvjoy, /* Invalid joystick ID */
    enomem,  /* insufficient memory */
    esystem  /* System consistency check */

} errcod;

HANDLE inphdl;          /* "input" file handle */
int     mb1;             /* mouse assert status button 1 */
int     mb2;             /* mouse assert status button 2 */
int     mb3;             /* mouse assert status button 3 */
int     mb4;             /* mouse assert status button 4 */
int     mpx, mpy;        /* mouse current position */
int     nmb1;            /* new mouse assert status button 1 */
int     nmb2;            /* new mouse assert status button 2 */
int     nmb3;            /* new mouse assert status button 3 */
int     nmb4;            /* new mouse assert status button 4 */
int     nmpx, nmpy;      /* new mouse current position */
/* we must open and process the _output file on our own, else we would
   recurse */
char    inpbuf[MAXLIN];  /* input line buffer */
int     inpptr;          /* input line index */
scnptr  screens[MAXCON]; /* screen contexts array */
int     curdsp;          /* index for current display screen */
int     curupd;          /* index for current update screen */
struct {

    int han; /* handle for timer */
    int rep; /* timer repeat flag */

} timers[10];


CONSOLE_SCREEN_BUFFER_INFO bi; /* screen buffer info structure */
CONSOLE_CURSOR_INFO        ci; /* console cursor info structure */
int      ti;          /* index for repeat array */
int      mode;        /* console mode */
int      b;           /* int return */
int      i;           /* tab index */
int      winhan;      /* main window id */
int      r;           /* result holder */
int      threadid;    /* dummy thread id (unused) */
int      threadstart; /* thread starts */
int      joy1xs;      /* last joystick position 1x */
int      joy1ys;      /* last joystick position 1y */
int      joy1zs;      /* last joystick position 1z */
int      joy2xs;      /* last joystick position 2x */
int      joy2ys;      /* last joystick position 2y */
int      joy2zs;      /* last joystick position 2z */
int      numjoy;      /* number of joysticks found */
/* global sets. these are the global set parameters that apply to any new
   created screen buffer */
int      gmaxx;       /* maximum x size */
int      gmaxy;       /* maximum y size */
scnatt   gattr;       /* current attribute */
int      gautof;      /* state of auto */
pa_color gforec;      /* forground color */
pa_color gbackc;      /* background color */
int      gcurv;       /* state of cursor visible */
int      cix;         /* index for display screens */
int      frmrun;      /* framing timer is running */
int      frmhan;      /* framing timer handle */

/*******************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.
This needs to go to a dialog instead of the system error trap.

*******************************************************************************/

void error(int e)

{

    fprintf(stderr, "*** Error: console: ");
    switch (e) { /* error */

        case eftbful: fprintf(stderr, "Too many files"); break;
        case ejoyacc: fprintf(stderr, "No joystick access available"); break;
        case etimacc: fprintf(stderr, "No timer access available"); break;
        case einvhan: fprintf(stderr, "Invalid handle"); break;
        case efilopr: fprintf(stderr, "Cannot perform operation on special file");
                      break;
        case efilzer: fprintf(stderr, "Filename is empty"); break;
        case einvscn: fprintf(stderr, "Invalid screen number"); break;
        case einvtab: fprintf(stderr, "Tab position specified off screen"); break;
        case esbfcrt: fprintf(stderr, "Cannot create screen buffer"); break;
        case einvjoy: fprintf(stderr, "Invalid joystick ID"); break;
        case ejoyqry: fprintf(stderr, "Could not get information on joystick");
                      break;
        case enomem:  fprintf(stderr, "Insufficient memory"); break;
        case esystem: fprintf(stderr, "System fault"); break;

    }
    fprintf(stderr, "\n");

    exit(1);

}

/*******************************************************************************

Make file entry

Indexes a present file entry or creates a new one. Looks for a free entry
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
    for (fi = OUTFIL+1; fi <= ss_maxhdl; fi++) /* search all file entries */
        if (!opnfil[fi]) ff = fi; /* found a slot */
    if (!ff) error(einvhan); /* no empty file positions found */
    fn = ff; /* set file id number */

}

/*******************************************************************************

Remove leading and trailing spaces

Given a string, removes any leading and trailing spaces in the string. The
result is allocated and returned as an indexed buffer.

*******************************************************************************/

void remspc(char* nm, /* string */
            char* rs) /* result string */

{

    int i1, i2; /* string indexes */
    int n;      /* string is null */
    char* s, e; /* string start and end */

    s = nm; /* set start of string */
    while (!*s && *s == ' ') s++; /* skip leading spaces */
    e = strchr(nm, 0)-1; /* set end of string */
    while (e > nm && *e == ' ') e--; /* find non-space */
    while (s <= e) *rs++ = *s++; /* copy body of string */
    *rs = 0; /* terminate */

}

/*******************************************************************************

Load console status

Loads the current console status. Updates the size, cursor location, and
current attributes.

*******************************************************************************/

void getpos(void)

{

    CONSOLE_SCREEN_BUFFER_INFO bi; /* screen buffer info structure */
    scnptr cp; /* screen pointer */
    int    b;  /* int return */

    if (curdsp == curupd) { /* buffer is in display */

        cp = screens[curupd]; /* index screen context */
        b = GetConsoleScreenBufferInfo(cp->han, &bi);
        if (cp->conx != bi.dwCursorPosition.x ||
            (cp->cony != bi.dwCursorPosition.y) {

            conx = bi.dwCursorPosition.x; /* get new cursor position */
            cony = bi.dwCursorPosition.y;
            cp->curx = cp->conx+1; /* place cursor position */
            cp->cury = cp->cony+1;

        }

    }

}

/*******************************************************************************

Construct packed coordinate word

Creates a Windows console coordinate word based on an X and Y value.

*******************************************************************************/

int pcoord(int x, y)

{

    return (y*65536+x);

}

/*******************************************************************************

Set colors

Sets the current background and foreground colors in windows attribute format
from the coded colors and the "reverse" attribute.
Despite the name, also sets the attributes. We obey reverse coloring, and
set the following "substitute" attributes:

italics      Foreground half intensity.

underline    Background half intensity.

*******************************************************************************/

int colnum(pa_color c, int i)

{

    int r;

    switch (c) { /* color */

        case pa_black:  r = 0x0000 | i*FOREGROUND_INTENSITY; break;
        case pa_white:  r = FOREGROUND_BLUE | FOREGROUND_GREEN |
                            FOREGROUND_RED | !i*FOREGROUND_INTENSITY; break;
        pa_red:      r = foreground_red | !i*foreground_intensity; break;
        pa_green:    r = foreground_green | !i*foreground_intensity; break;
        pa_blue:     r = foreground_blue | !i*foreground_intensity; break;
        pa_cyan:     r = foreground_blue | foreground_green |
                         !i*foreground_intensity; break;
        pa_yellow:  r = foreground_red | foreground_green |
                        !i*foreground_intensity; break;
        pa_magenta: r = foreground_red | foreground_blue !
                        !i*foreground_intensity; break;

    }

    return (r); /* return result */

}

void setcolor(scnptr sc)

{

    if (sc->attr == sarev) /* set reverse colors */
        sc->sattr = colnum(sc->forec,
                           (sc->attr == saital || sc->attr == sabold))*16+
                            colnum(sc->backc, (sc->attr == saundl ||
                                               sc->attr == sabold));
    else /* set normal colors */
        sc->sattr = colnum(sc->backc,
                           (sc->attr == saundl || sc->attr == sabold))*16+
                            colnum(sc->forec,
                                   (sc->attr == saital || sc->attr == sabold));

    }

}

/*******************************************************************************

Find colors

Find colors from attribute word.

*******************************************************************************/


pa_color numcol(int a)

{

    pa_color c;

    switch (a%8) { /* color */

         case 0: c = pa_black; break;
         case 1: c = pa_blue; break;
         case 2: c = pa_green; break;
         case 3: c = pa_cyan; break;
         case 4: c = pa_red; break;
         case 5: c = pa_magenta; break;
         case 6: c = pa_yellow; break;
         case 7: c = pa_white; break;

    }
    numcol = c

}

void fndcolor(int a)

{

    screens[curupd]->forec = numcol(a); /* set foreground color */
    screens[curupd]->backc = numcol(a/16); /* set background color */

}

/*******************************************************************************

Find if cursor is in screen bounds

Checks if the cursor lies in the current bounds, and returns true if so.

*******************************************************************************/

int icurbnd(scnptr sc)

{

    return (sc->curx >== 1 && sc->curx <== sc->maxx && sc->cury >== 1 &&
            sc->cury <== sc->maxy);

}

/*******************************************************************************

Set cursor status

Sets the cursor visible or invisible. If the cursor is out of bounds, it is
invisible regardless. Otherwise, it is visible according to the state of the
current buffer's visible status.

*******************************************************************************/

void cursts(scnptr sc)

{

    int b;
    CONSOLE_CURSOR_INFO ci;
    int cv;

    cv = sc->curv; /* set current buffer status */
    if (!icurbnd(sc)) cv = 0; /* not in bounds, force off */
    /* get current console information */
    b = GetConsoleCursorInfo(sc->han, ci);
    ci.bVisible = cv; /* set cursor status */
    b = SetConsoleCursorInfo(sc->han, ci);

}

/*******************************************************************************

Position cursor

Positions the cursor (caret) image to the right location on screen, and handles
the visible or invisible status of that.

Windows has a nasty bug that setting the cursor position of a buffer that
isn't in display causes a cursor mark to be made at that position on the
active display. So we don't position if not in display.

*******************************************************************************/

void setcur(scnptr sc)

{

    int b;

    /* check cursor in bounds, and buffer in display */
    if (icurbnd(sc) && sc == screens[curdsp]) {

        /* set cursor position */
        b = SetConsoleCursorPosition(han, pcoord(sc->curx-1, sc->cury-1));
        sc->conx = sc->curx-1; /* set our copy of that */
        sc->cony = sc->cury-1;

    }
    cursts(sc); /* set new cursor status */

}

/*******************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

void iclear(scnptr sc)

{

    int  x, y;
    char cb; /* character output buffer */
    int  ab; /* attribute output buffer */
    int  b;

    cb = ' '; /* set space */
    ab = sc->sattr; /* set attributes */
    for (y = 0; y < sc->maxy; y++) {

        for (x = 0; x < sc->maxx; x++) {

            b = WriteConsoleOutputCharacter(han, &cb, pcoord(x, y), 1);
            b = writeconsoleoutputattribute(han, &ab, pcoord(x, y), 1)

        }

    }
    sc->cury = 1; /* set cursor at home */
    sc->curx = 1;
    setcur(sc);

}

/*******************************************************************************

Initalize screen

Clears all the parameters in the present screen context.

*******************************************************************************/

void iniscn(scnptr sc)

{

    int i;

    sc->maxx = gmaxx; /* set size */
    sc->maxy = gmaxy;
    b = SetConsoleScreenBufferSize(sc->han, pcoord(sc->maxx, sc->maxy));
    sc->forec = gforec; /* set colors and attributes */
    sc->backc = gbackc;
    sc->attr = gattr;
    sc->autof = gautof; /* set auto scroll and wrap */
    sc->curv = gcurv; /* set cursor visibility */
    setcolor(sc); /* set current color */
    iclear(sc); /* clear screen buffer with that */
    /* set up tabbing to be on each 8th position */
    for (i = 1; i <= sc->maxx; i++) sc->tab[i] = (i-1) % 8 == 0;

}

/*******************************************************************************

Scroll screen

Scrolls the terminal screen by deltas in any given direction. Windows performs
scrolls as source->destination rectangle moves. This can be easily converted
from our differentials, but there is additional complexity in that both the
source and destination must be on screen, and not in negative space. So we
take each case of up, down, left right and break it out into moves that will
remain on the screen.
Because Windows (oddly) fills based on the source rectangle, we perform each
scroll as a separate sequence in each x and y direction. This makes the code
simpler, and lets Windows perform the fills for us.

*******************************************************************************/

void iscroll(int x, y)

{

    SMALL_RECT sr; /* scroll rectangle */
    CHAR_INFO  f;  /* fill character info */
    int        b;  /* return value */
    scnptr     sc; /* screen context */

    sc = screens[curupd]; /* index screen context */
    f.asciichar = ' '; /* set fill values */
    f.attributes = sc->sattr;
    if (x <== -sc->maxx || x >== sc->maxx) ||
        y <== -sc->maxy) || y >== sc->maxy)
        /* scroll would result in complete clear, do it */
        iclear(screens[curupd]); /* clear the screen buffer */
    else { /* scroll */

        /* perform y moves */
        if (y >== 0) { /* move text up */

            sr.left = 0;
            sr.right = sc->maxx-1;
            sr.top = y;
            sr.bottom = sc->maxy-1;
            b = ScrollConsoleScreenbuffer(sc->han, sr, pcoord(0, 0), f);

        } else { /* move text down */

            sr.left = 0;
            sr.right = sc->maxx-1;
            sr.top = 0;
            sr.bottom = sc->maxy-1;
            b = ScrollConsoleScreenBuffer(sc->han, sr, pcoord(0, abs(y)), f);

        }
        /* perform x moves */
        if (x >== 0) then { /* move text left */

            sr.left = x;
            sr.right = sc->maxx-1;
            sr.top = 0;
            sr.bottom = sc->maxy-1;
            b = ScrollConsoleScreenbuffer(sc->han, sr, pcoord(0, 0), f);

        } else { /* move text right */

            sr.left = 0;
            sr.right = sc->maxx-1;
            sr.top = 0;
            sr.bottom = sc->maxy-1;
            b = ScrollConsoleScreenBuffer(sc->han, sr, pcoord(abs(x), 0), f);

        }

    }

}

void scroll(FILE* f, int x, int y)

{

    iscroll(x, y); /* process scroll */

}

/*******************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

void icursor(int x, int y)

{

    screens[curupd]->cury = y; /* set new position */
    screens[curupd]->curx = x;
    setcur(screens[curupd]); /* set cursor on screen */

}

void cursor(FILE* f: text, int x, int y)

{

    icursor(x, y); /* position cursor */

}

/*******************************************************************************

Find if cursor is in screen bounds

This is the external interface to curbnd.

*******************************************************************************/

int curbnd(FILE* f)

{

    curbnd = icurbnd(screens[curupd]);

}

/*******************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int maxx(FILE* f)

{

    maxx = screens[curupd]->maxx; /* set maximum x */

}

/*******************************************************************************

Return maximum y dimension

Returns the maximum y demension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int maxy(FILE* f)

{

    maxy = screens[curupd]->maxy; /* set maximum y */

}

/*******************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void home(FILE* f)

{

    screens[curupd]->cury = 1; /* set cursor at home */
    screens[curupd]->curx = 1;
    setcur(screens[curupd]); /* set cursor on screen */

}

/*******************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

void iup(void)

{

    scnptr sc;

    getpos(); /* update status */
    sc = screens[curupd];
    /* check not top of screen */
    if (sc->cury > 1) sc->cury++; /* update position */
    /* check auto mode */
    else if (autof) iscroll(0, -1); /* scroll up */
    /* check won't overflow */
    else if (sc->cury > -INT_MAX) sc->cury--; /* set new position */
    setcur(sc); /* set cursor on screen */

}

void up(FILE* f)

{

    iup(); /* move up */

}

/*******************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

void idown(void)

{

    scnptr sc;

    getpos(); /* update status */
    sc = screens[curupd];
    /* check not bottom of screen */
    if (sc->cury < sc->maxy) sc->cury++; /* update position */
    /* check auto mode */
    else if (sc->autof) iscroll(0, +1) /* scroll down */
    else if (sc->cury < INT_MAX) sc->cury++; /* set new position */
    setcur(screens[curupd]); /* set cursor on screen */

}

void down(FILE* f)

{

    idown(); /* move cursor down */

}

/*******************************************************************************

Move cursor left internal

Moves the cursor one character left. If the cursor is at the extreme left and
auto mode is on, the cursor will wrap to the right, up one line, otherwise
the cursor will move into negative space, limited only by maxint.

*******************************************************************************/

void ileft(void)

{

    scnptr sc;

    getpos(); /* update status */
    sc = screens[curupd];
    /* check not extreme left */
    if (sc->curx > 1) sc->curx--; /* update position */
    else { /* wrap cursor motion */

        if (sc->autof) { /* autowrap is on */

            iup(); /* move cursor up one line */
            sc->curx = sc->maxx /* set cursor to extreme right */

        } else
            /* check won't overflow */
            if (sc->curx > -INT_MAX) curx--; /* update position */

    }
    setcur(screens[curupd]); /* set cursor on screen */

}

void left(FILE* f)

{

    ileft(); /* move cursor left */

}

/*******************************************************************************

Move cursor right internal

Moves the cursor one character right.

*******************************************************************************/

void iright(void)

{

    scnptr sc;

    getpos; /* update status */
    sc = screens[curupd];
    /* check not at extreme right */
    if (sc->curx < sc->maxx) sc->curx++; /* update position */
    else { /* wrap cursor motion */

        if (sc->autof) { /* autowrap is on */

            idown(); /* move cursor up one line */
            sc->curx = 1; /* set cursor to extreme left */

        } else
            /* check won't overflow */
            if (sc->curx < INT_MAX) sc->curx++; /* update position */

    }
    setcur(sc); /* set cursor on screen */

}

void right(FILE* f)

{

    iright(); /* move cursor right */

}

/*******************************************************************************

Process tab

Process a single tab. We search to the right of the current cursor collumn to
find the next tab. If there is no tab, no action is taken, otherwise, the
cursor is moved to the tab stop.

*******************************************************************************/

void itab(void)

{

    int i;

    getpos(); /* update status */
    /* first, find if next tab even exists */
    i = screens[curupd]->curx+1; /* get the current x position +1 */
    if (i < 1) i = 1; /* don't bother to search to left of screen */
    /* find tab or } of screen */
    while (i < screens[curupd]->maxx && !screens[curupd]->tab[i]) i++;
    if (screens[curupd]->tab[i]) /* we found a tab */
        while (screens[curupd]->curx < i) iright(); /* go to the tab */

}

/*******************************************************************************

Turn on blink attribute

Turns on/off the blink attribute.

Note that the attributes can only be set singly.

*******************************************************************************/

void blink(FILE* f, int e)

{

    /* no capability */
    screens[curupd]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd]); /* set those colors active */

}

/*******************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute. Reverse is done by swapping the background
and foreground writing colors.
Note that the attributes can only be set singly.

*******************************************************************************/

void reverse(FILE* f, int e)

{

    if (e) screens[curupd]->attr = sarev; /* set reverse status */
    else screens[curupd]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd]); /* set those colors active */

}

/*******************************************************************************

Turn on underline attribute

Turns on/off the underline attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void underline(FILE* f, int e)

{

    /* substituted by background half intensity */
    if (e) screens[curupd]->attr = saundl; /* set underline status */
    else screens[curupd]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd]); /* set those colors active */

}

/*******************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void superscript(FILE* f, int e)

{

    /* no capability */
    screens[curupd]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd]); /* set those colors active */

}

/*******************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void subscript(FILE* f, int e)

{

    /* no capability */
    screens[curupd]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd]); /* set those colors active */

}

/*******************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void italic(FILE* f, int e)

{

    /* substituted by foreground half intensity */
    if (e) screens[curupd]->attr = saital; /* set italic status */
    else screens[curupd]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd]); /* set those colors active */

}

/*******************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void bold(FILE* f, int e)

{

    /* substituted by foreground and background half intensity */
    if (e) screens[curupd]->attr = sabold; /* set bold status */
    else screens[curupd]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd]); /* set those colors active */

}

/*******************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.
Note that the attributes can only be set singly.
Not implemented, but strikeout can be done by drawing a line through characters
just placed.

*******************************************************************************/

void strikeout(FILE* f, int e)

{

    /* no capability */
    screens[curupd]->attr = sanone; /* set attribute inactive */
    setcolor(screens[curupd]); /* set those colors active */

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

Sets the foreground (text) color from the universal primary code.

*******************************************************************************/

void fcolor(FILE* f, pa_color c)

{

    screens[curupd]->forec = c; /* set color status */
    setcolor(screens[curupd]); /* activate */

}

/*******************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void bcolor(FILE* f, pa_color c)

{

    screens[curupd]->backc = c; /* set color status */
    setcolor(screens[curupd]); /* activate */

}

/*******************************************************************************

Enable/disable automatic scroll and wrap


Enables or disables automatic screen scroll and } of line wrapping. When the
cursor leaves the screen in automatic mode, the following occurs:

up         Scroll down
down      Scroll up
right     Line down, start at left
left      Line up, start at right

These movements can be combined. Leaving the screen right from the lower right
corner will both wrap and scroll up. Leaving the screen left from upper left
will wrap and scroll down.

With auto disabled, no automatic scrolling will occur, and any movement of the
cursor off screen will simply cause the cursor to be undefined. In this
package that means the cursor is off, and no characters are written. On a
real terminal, it simply means that the position is undefined, and could be
anywhere.

*******************************************************************************/

void auto(FILE* f, int e)

{

    screens[curupd]->autof = e; /* set line wrap status */

}

/*******************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility.

*******************************************************************************/

void curvis(FILE* f, int e)

{

    screens[curupd]->curv = e; /* set cursor visible status */
    cursts(screens[curupd]); /* update cursor status */

}

/*******************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int curx(FILE* f)

{

    getpos(); /* update status */

    return (screens[curupd]->curx); /* return current location x */

}

/*******************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int cury(FILE* f)

{

    getpos(); /* update status */

    return (screens[curupd]->cury); /* return current location y */

}

/*******************************************************************************

Select current screen

Selects one of the screens to set active. If the screen has never been used,
then a new screen is allocated and cleared.
The most common use of the screen selection system is to be able to save the
initial screen to be restored on exit. This is a moot point in this
application, since we cannot save the entry screen in any case.
We allow the screen that is currently active to be reselected. This effectively
forces a screen refresh, which can be important when working on terminals.

*******************************************************************************/

void iselect(int u, int d)

{

    if (u < 1 || u > MAXCON || d < 1 || d > MAXCON)
        error(einvscn); /* invalid screen number */
    curupd = u; /* set current update screen */
    if (!screens[curupd]) then { /* no screen, create one */

        screens[curupd] = malloc(sizeof(scncon)); /* get a new screen context */
        if (!screens[curupd]) error(enomem); /* failed to allocate */
        /* create the screen */
        screens[curupd]->han =
            CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      CONSOLE_TEXTMODE_BUFFER);
        /* check handle is valid */
        if (screens[curupd]->han == INVALID_HANDLE_VALUE) error(esbfcrt);
        iniscn(screens[curupd]); /* initalize that */

    }
    curdsp = d; /* set the current display screen */
    if (!screens[curdsp]) { /* no screen, create one */

        screens[curdsp] = malloc(sizeof(scncon)); /* get a new screen context */
        if (!screens[curdsp]) error(enomem); /* failed to allocate */
        /* create the screen */
        screens[curdsp]->han =
            CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         CONSOLE_TEXTMODE_BUFFER);
        /* check handle is valid */
        if screens[curdsp]->han == INVALID_HANDLE_VALUE then error(esbfcrt);
        iniscn(screens[curdsp]); /* initalize that */

    }
    /* set display buffer as active display console */
    b = SetConsoleActiveScreenbuffer(screens[curdsp]->han);
    getpos(); /* make sure we are synced with Windows */
    setcur(screens[curdsp]); /* make sure the cursor is at correct point */

}

void select(FILE* f, int u, int d)

{

    iselect(u, d); /* perform select */

}

/*******************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attributes. Obeys a series of elementary control characters.

We don't try to perform a terminal emulation here. The terminal interface does
that, and in any case an emulator would be layered above that.

*******************************************************************************/

void plcchr(char c)

{

    int    b;   /* int return */
    char   cb;  /* character output buffer */
    int    ab;  /* attribute output buffer */
    int    len; /* length dummy */
    scnptr sc;  /* screen context pointer */

    getpos(); /* update status */
    sc = screens[curupd];
    /* handle special character cases first */
    if (c == '\r') /* carriage return, position to extreme left */
        icursor(1, sc->cury);
    else if (c == '\n') idown(); /* line feed, move down */
    else if c == '\b') ileft(); /* back space, move left */
    else if c == '\f') iclear(sc); /* clear screen */
    else if c == '\t') itab(); /* process tab */
    else if (c >== ' ' && c != 0x7f) { /* character is visible */

        if (icurbnd(sc)) { /* cursor in bounds */

            cb = c; /* place character in buffer */
            ab = sc->sattr; /* place attribute in buffer */
            /* write character */
            b = writeconsoleoutputcharacter(sc->han, &cb, 1,
                                            pcoord(sc->curx-1, sc->cury-1),
                                            len);
            b = writeconsoleoutputattribute(han, &ab, 1,
                                            pcoord(sc->curx-1, sc->cury-1),
                                            len);

        }
        iright(); /* move cursor right */

    }

}

/*******************************************************************************

Delete last character

Deletes the character to the left of the cursor, and moves the cursor one
position left.

*******************************************************************************/

void del(FILE* f)

{

    left(f); /* back up cursor */
    plcchr(' '); /* blank out */
    left(f); /* back up again */

}

/*******************************************************************************

Aquire next input event

Waits for and returns the next event. For now, the input file is ignored, and
the standard input handle allways used.

*******************************************************************************/

/*

Process keyboard event.
The events are mapped from IBM-PC keyboard keys as follows:

etup        up arrow            cursor up one line
etdown     down arrow           down one line
etleft     left arrow           left one character
etright    right arrow          right one character
etleftw    shift-left arrow     left one word
etrightw   shift-right arrow    right one word
ethome     cntrl-home           home of document
ethomes    shift-home           home of screen
ethomel    home                 home of line
etend      cntrl-end            end of document
etends     shift-end            end of screen
etendl     end                  end of line
etscrl     cntrl-left arrow     scroll left one character
etscrr     cntrl-right arrow    scroll right one character
etscru     cntrl-up arrow       scroll up one line
etscrd     cntrl-down arrow     scroll down one line
etpagd     page down            page down
etpagu     page up              page up
ettab      tab                  tab
etenter    enter                enter line
etinsert   cntrl-insert         insert block
etinsertl  shift-insert         insert line
etinsertt  insert               insert toggle
etdel      cntrl-del            delete block
etdell     shift-del            delete line
etdelcf    del                  delete character forward
etdelcb    backspace            delete character backward
etcopy     cntrl-f1             copy block
etcopyl    shift-f1             copy line
etcan      esc                  cancel current operation
etstop     cntrl-s              stop current operation
etcont     cntrl-q              continue current operation
etprint    shift-f2             print document
etprintb   cntrl-f2             print block
etprints   cntrl-f3             print screen
etf1       f1                   int key 1
etf2       f2                   int key 2
etf3       f3                   int key 3
etf4       f4                   int key 4
etf5       f5                   int key 5
etf6       f6                   int key 6
etf7       f7                   int key 7
etf8       f8                   int key 8
etf9       f9                   int key 9
etf10      10                   int key 10
etmenu     alt                  display menu
etend      cntrl-c              terminate program

*/

/* check control key pressed */

int cntrl(INPUT_RECORD* inpevt)

{

    return (*inpevt.keyevent.dwcontrolkeystate &&
            (right_ctrl_pressed || left_ctrl_pressed) != 0);

}

/* check shift key pressed */

int shift(INPUT_RECORD* inpevt)

{

    return (*inpevt.keyevent.dwcontrolkeystate && shift_pressed != 0);

}

void keyevent(pa_evtptr er, INPUT_RECORD* inpevt, int* keep);

{

    /* we only take key down (pressed) events */
    if (*inpevt.keyevent.bkeydown) {

        /* Check character is non-zero. The zero character occurs
          whenever a control feature is hit. */
        if (*inpevt.keyevent.uchar.asciichar) {

            if (*inpevt.keyevent.uchar.asciichar == '\r')
                *er.etype = etenter /* set enter line */
            else if (*inpevt.keyevent.uchar.asciichar == '\b')
                *er.etype = etdelcb /* set delete character backwards */
            else if (*inpevt.keyevent.uchar.asciichar == '\t')
                *er.etype = ettab /* set tab */
            else if (*inpevt.keyevent.uchar.asciichar == 'C'-64)
                *er.etype = etterm /* set } program */
            else if (*inpevt.keyevent.uchar.asciichar == 'S'-64)
                *er.etype = etstop /* set stop program */
            else if (*inpevt.keyevent.uchar.asciichar == 'Q'-64)
                *er.etype = etcont /* set continue program */
            else { /* normal character */

                *er.etype = etchar; /* set character event */
                *er.char = *inpevt.keyevent.uchar.asciichar

            }
            *keep = true /* set keep event */

        } else switch (*inpevt.keyevent.wVirtualKeyCode) { /* key */

            case VK_HOME: /* home */
                if (cntrl(inpevt)) *er.etype = ethome /* home document */
                else if (shift(inpevt)) *er.etype = ethomes /* home screen */
                else *er.etype = ethomel; /* home line */
                *keep = true; /* set keep event */
                break;

            case VK_END: /* } */
                if (cntrl(inpevt)) *er.etype = et} /* end document */
                else if (shift(inpevt)) *er.etype = et}s /* end screen */
                else *er.etype = etendl; /* end line */
                *keep = true; /* set keep event */
                break;

            case VK_UP: /* up */
                if (cntrl(inpevt)) *er.etype = etscru /* scroll up */
                else *er.etype = etup; /* up line */
                *keep = true; /* set keep event */
                break;

            case VK_DOWN: /* down */
                if (cntrl(inpevt)) *er.etype = etscrd; /* scroll down */
                else *er.etype = etdown; /* up line */
                *keep = true; /* set keep event */
                break;

            case vk_left: /* left */
                if (cntrl(inpevt)) *er.etype = etscrl; /* scroll left one character */
                else if (shift(inpevt)) *er.etype = etleftw; /* left one word */
                else *er.etype = etleft; /* left one character */
                *keep = true; /* set keep event */
                break;

            case vk_right: /* right */
                if (cntrl(inpevt)) *er.etype = etscrr; /* scroll right one character */
                else if (shift(inpevt)) *er.etype = etrightw; /* right one word */
                else *er.etype = etright; /* left one character */
                *keep = true; /* set keep event */
                break;

            case vk_insert: /* insert */
                if (cntrl(inpevt)) *er.etype = etinsert; /* insert block */
                else if (shift(inpevt)) *er.etype = etinsertl; /* insert line */
                else *er.etype = etinsertt; /* insert toggle */
                *keep = true; /* set keep event */
                break;

            case vk_delete: /* delete */
                if (cntrl(inpevt)) *er.etype = etdel; /* delete block */
                else if (shift(inpevt)) *er.etype = etdell; /* delete line */
                else *er.etype = etdelcf; /* insert toggle */
                *keep = true; /* set keep event */
                break;

            case vk_prior:
                *er.etype = etpagu; /* page up */
                *keep = true; /* set keep event */
                break;

            case vk_next:
                *er.etype = etpagd; /* page down */
                *keep = true; /* set keep event */
                break;

            case vk_f1: /* f1 */
                if (cntrl(inpevt)) *er.etype = etcopy /* copy block */
                else if (shift(inpevt)) *er.etype = etcopyl; /* copy line */
                else { /* f1 */

                    *er.etype = etfun;
                    *er.fkey = 1;

                }
                *keep = true; /* set keep event */
                break;

            case vk_f2: /* f2 */
                if (cntrl(inpevt)) *er.etype = etprintb; /* print block */
                else if (shift(inpevt)) *er.etype = etprint; /* print document */
                else { /* f2 */

                    *er.etype = etfun;
                    *er.fkey = 2;

                }
                *keep = true; /* set keep event */
                break;

            case vk_f3: /* f3 */
                if (cntrl(inpevt)) *er.etype = etprints; /* print screen */
                else { /* f3 */

                    *er.etype = etfun;
                    *er.fkey = 3;

                }
                *keep = true; /* set keep event */
                break;

            case vk_f4: /* f4 */
                *er.etype = etfun;
                *er.fkey = 4;
                *keep = true; /* set keep event */
                break;

            case vk_f5: /* f5 */
                *er.etype = etfun;
                *er.fkey = 5;
                *keep = true; /* set keep event */
                break;

            case vk_f6: /* f6 */
                *er.etype = etfun;
                *er.fkey = 6;
                *keep = true; /* set keep event */
                break;

            case vk_f7: /* f7 */
                *er.etype = etfun;
                *er.fkey = 7;
                *keep = true; /* set keep event */
                break;

            case vk_f8: /* f8 */
                *er.etype = etfun;
                *er.fkey = 8;
                *keep = true; /* set keep event */
                break;

            case vk_f9: /* f9 */
                *er.etype = etfun;
                *er.fkey = 9;
                *keep = true; /* set keep event */
                break;

            case vk_f10: /* f10 */
                *er.etype = etfun;
                *er.fkey = 10;
                *keep = true; /* set keep event */
                break;

            case vk_f11: /* f11 */
                *er.etype = etfun;
                *er.fkey = 11;
                *keep = true; /* set keep event */
                break;

            case vk_f12: /* f12 */
                *er.etype = etfun;
                *er.fkey = 12;
                *keep = true; /* set keep event */
                break;

            case vk_menu:
                *er.etype = etmenu; /* alt */
                *keep = true; /* set keep event */
                break;

            case vk_cancel:
                *er.etype = etterm; /* ctl-brk */
                *keep = true; /* set keep event */
                break;

        }

    }

}

/*

Process mouse event.
Buttons are assigned to Win 95 as follows:

button 1: Windows left button
button 2: Windows right button
button 3: Windows 2nd from left button
button 4: Windows 3rd from left button

The Windows 4th from left button is unused. The double click events will
} up, as, well, two clicks of a single button, displaying my utter
contempt for the whole double click concept.

*/

/* update mouse parameters */

void mouseupdate(pa_evtptr er, int* keep)

{

    /* we prioritize events by: movements 1st, button clicks 2nd */
    if (nmpx != mpx || nmpy != mpy) { /* create movement event */

        *er.etype = etmoumov; /* set movement event */
        *er.mmoun = 1; /* mouse 1 */
        *er.moupx = nmpx; /* set new mouse position */
        *er.moupy = nmpy;
        mpx = nmpx; /* save new position */
        mpy = nmpy;
        *keep = true; /* set to keep */

    } else if nmb1 > mb1 {

        *er.etype = etmouba; /* button 1 assert */
        *er.amoun = 1; /* mouse 1 */
        *er.amoubn = 1; /* button 1 */
        mb1 = nmb1; /* update status */
        *keep = true; /* set to keep */

    } else if nmb2 > mb2 {

        *er.etype = etmouba; /* button 2 assert */
        *er.amoun = 1; /* mouse 1 */
        *er.amoubn = 2; /* button 2 */
        mb2 = nmb2; /* update status */
        *keep = true; /* set to keep */

    } else if nmb3 > mb3 {

        *er.etype = etmouba; /* button 3 assert */
        *er.amoun = 1; /* mouse 1 */
        *er.amoubn = 3; /* button 3 */
        mb3 = nmb3; /* update status */
        *keep = true; /* set to keep */

    } else if nmb4 > mb4 {

        *er.etype = etmouba; /* button 4 assert */
        *er.amoun = 1; /* mouse 1 */
        *er.amoubn = 4; /* button 4 */
        mb4 = nmb4; /* update status */
        *keep = true; /* set to keep */

    } else if nmb1 < mb1 {

        *er.etype = etmoubd; /* button 1 deassert */
        *er.dmoun = 1; /* mouse 1 */
        *er.dmoubn = 1; /* button 1 */
        mb1 = nmb1; /* update status */
        *keep = true; /* set to keep */

    } else if nmb2 < mb2 {

        *er.etype = etmoubd; /* button 2 deassert */
        *er.dmoun = 1; /* mouse 1 */
        *er.dmoubn = 2; /* button 2 */
        mb2 = nmb2; /* update status */
        *keep = true; /* set to keep */

    } else if nmb3 < mb3 {

        *er.etype = etmoubd; /* button 3 deassert */
        *er.dmoun = 1; /* mouse 1 */
        *er.dmoubn = 3; /* button 3 */
        mb3 = nmb3; /* update status */
        *keep = true; /* set to keep */

    } else if nmb4 < mb4 {

        *er.etype = etmoubd; /* button 4 deassert */
        *er.dmoun = 1; /* mouse 1 */
        *er.dmoubn = 4; /* button 4 */
        mb4 = nmb4; /* update status */
        *keep = true; /* set to keep */

    }

}

/* register mouse status */

void mouseevent(INPUT_RECORD* inpevt)

{

    /* gather a new mouse status */
    nmpx = *inpevt.MouseEvent.dwMousePosition.x+1; /* get mouse position */
    nmpy = *inpevt.MouseEvent.dwMousePosition.y+1;
    nmb1 = *inpevt.MouseEvent.dwButtonState && FROM_LEFT_1ST_BUTTON_PRESSED != 0;
    nmb2 = *inpevt.MouseEvent.dwButtonState && RIGHTMOST_BUTTON_PRESSED != 0;
    nmb3 = *inpevt.MouseEvent.dwButtonState && FROM_LEFT_2ND_BUTTON_PRESSED != 0;
    nmb4 = *inpevt.MouseEvent.dwButtonState && FROM_LEFT_3RD_BUTTON_PRESSED != 0

}

/* issue event for changed button */

void updn(pa_evtptr er, INPUT_RECORD* inpevt, int* keep, int bn, int bm)

{

    /* Note that if there are multiple up/down bits, we only register the last
      one. Windows is ambiguous as to if it will combine such events */
    if (!(*inpevt.KeyEvent.wVirtualKeyCode & bm)) { /* assert */

        *er.etype = etjoyba; /* set assert */
        if (*inpevt.EventType == UIV_JOY1BUTTONDOWN ||
            inpevt.EventType == UIV_JOY1BUTTONUP) *er.ajoyn = 1;
        else *er.ajoyn = 2;
        *er.ajoybn = bn; /* set number */

    } else { /* deassert */

        *er.etype = etjoybd; /* set deassert */
        if (inpevt.EventType == UIV_JOY1BUTTONDOWN ||
            inpevt.EventType == UIV_JOY1BUTTONUP) er.ajoyn = 1;
        else er.ajoyn = 2;
        er.djoybn = bn; /* set number */

    }
    *keep = true; /* set keep event */

}

/* process joystick messages */

void joymes(INPUT_RECORD* inpevt)

{

    /* register changes on each button */
    if (!(*inpevt.KeyEvent.wVirtualKeyCode & joy_button1chg))
        updn(1, joy_button1);
    if (!(*inpevt.KeyEvent.wVirtualKeyCode & joy_button2chg))
        updn(2, joy_button2);
    if (!(*inpevt.KeyEvent.wVirtualKeyCode & joy_button3chg))
        updn(3, joy_button3);
    if (!(*inpevt.KeyEvent.wVirtualKeyCode & joy_button4chg))
        updn(4, joy_button4);

}

void ievent(pa_evtptr er)

{

    int keep;            /* event keep flag */
    int b;               /* int return value */
    int ne;              /* number of events */
    int x, y, z;         /* joystick readback */
    int dx, dy, dz;      /* joystick readback differences */
    INPUT_RECORD inpevt; /* event read buffer */

/*msg:  msg;*/

    do {

        keep = 0; /* set don't keep by default */
        mouseupdate(er, &keep); /* check any mouse details need processing */
        if (!keep) { /* no, go ahead with event read */

            b = ReadConsoleInput(inphdl, inpevt, 1, ne); /* get the next event */
            if b then { /* process valid event */

                /* decode by event */
                if (inpevt.EventType == KEY_EVENT)
                    keyevent(er, inpevt, &keep); /* key event */
                else if (inpevt.EventType == MOUSE_EVENT)
                    mouseevent(er, &keep); /* mouse event */
                else if (inpevt.EventType == UIV_TIM) { /* timer event */

                    er.etype = ettim; /* set timer event occurred */
                    /* set what timer */
                    er.timnum = inpevt.KeyEvent.wVirtualKeyCode;
                    keep = true; /* set to keep */

                } else if (inpevt.EventType == UIV_JOY1MOVE ||
                           (inpevt.EventType == UIV_JOY1ZMOVE ||
                           (inpevt.EventType == UIV_JOY2MOVE ||
                           (inpevt.EventType == UIV_JOY2ZMOVE) {

                    /* joystick move */
                    er.etype = etjoymov; /* set joystick moved */
                    /* set what joystick */
                    if (inpevt.EventType == UIV_JOY1MOVE ||
                        (inpevt.EventType == UIV_JOY1ZMOVE) er.mjoyn = 1
                    else er.mjoyn = 2;
                    /* Set all variables to default to same. This way, only the joystick
                      axes that are actually set by the message are registered. */
                    if (inpevt.EventType == UIV_JOY1MOVE ||
                        (inpevt.EventType == UIV_JOY1ZMOVE) {

                        x = joy1xs;
                        y = joy1ys;
                        z = joy1zs;

                    } else {

                        x = joy2xs;
                        y = joy2ys;
                        z = joy2zs;

                    }
                    /* If it's an x/y move, split the x and y axies parts of the message
                      up. */
                    if (inpevt.EventType == UIV_JOY1MOVE ||
                        (inpevt.EventType == UIV_JOY2MOVE) {

                        /* get x and y positions */
                        x = inpevt.KeyEvent.wVirtualKeyCode;
                        y = inpevt.KeyEvent.wVirtualScanCode

                    } else /* get z position */
                        z = inpevt.KeyEvent.wVirtualKeyCode;
                    /* We perform thresholding on the joystick right here, which is
                      limited to 255 steps (same as joystick hardware. find joystick
                      diffs and update */
                    if (inpevt.EventType == UIV_JOY1MOVE ||
                        (inpevt.EventType == UIV_JOY1ZMOVE) {

                        dx = abs(joy1xs-x); /* find differences */
                        dy = abs(joy1ys-y);
                        dz = abs(joy1zs-z);
                        joy1xs = x; /* place old values */
                        joy1ys = y;
                        joy1zs = z;

                    } else {

                        dx = abs(joy2xs-x); /* find differences */
                        dy = abs(joy2ys-y);
                        dz = abs(joy2zs-z);
                        joy2xs = x; /* place old values */
                        joy2ys = y;
                        joy2zs = z;

                    }
                    /* now reject moves below the threshold */
                    if (dx > 65535 / 255 || dy > 65535 / 255 ||
                        dz > 65535 / 255) {

                        /* scale axies between -maxint..maxint and place */
                        er.joypx = (x - 32767)*(maxint / 32768);
                        er.joypy = (y - 32767)*(maxint / 32768);
                        er.joypz = (z - 32767)*(maxint / 32768);
                        keep = 1; /* set keep event */

                    }

                } else if (inpevt.EventType == UIV_JOY1BUTTONDOWN ||
                                (inpevt.EventType == UIV_JOY2BUTTONDOWN ||
                                (inpevt.EventType == UIV_JOY1BUTTONUP) or
                                (inpevt.EventType == UIV_JOY2BUTTONUP) then joymes(inpevt);
                else if inpevt.EventType == UIV_TERM then {

                    er.etype = etterm; /* set } program */
                    keep = true; /* set keep event */

                }

            }

        }

    } while (!keep); /* until an event we want occurs */

} /* event */

void event(FILE* f, pa_evtptrr er);

{

    ievent(er) /* process event */

}

/*******************************************************************************

Timer handler procedure

Called when the windows multimedia event timer expires. We prepare a message
to s} up to the console even handler. Since the console event system does not
have user defined messages, this is done by using a key event with an invalid
event code.

*******************************************************************************/

void timeout(int id, int msg, int usr, int dw1, int dw2)

{

    INPUT_RECORD inpevt; /* windows event record array */
    int ne;  /* number of events written */
    int b;

    inpevt.EventType = UIV_TIM; /* set key event type */
    inpevt.KeyEvent.wVirtualKeyCode = usr; /* set timer handle */
    b = WriteConsoleInput(inphdl, inpevt, 1, ne); /* send */

}

/*******************************************************************************

Set timer

Sets an elapsed timer to run, as identified by a timer handle. From 1 to 10
timers can be used. The elapsed time is 32 bit signed, in tenth milliseconds.
This means that a bit more than 24 hours can be measured without using the
sign.
Timers can be set to repeat, in which case the timer will automatically repeat
after completion. When the timer matures, it s}s a timer mature event to
the associated input file.

*******************************************************************************/

void itimer(int i, /* timer handle */
            int t, /* number of tenth-milliseconds to run */
            int r) /* timer is to rerun after completion */

{

    int tf; /* timer flags */
    int mt; /* millisecond time */

    mt = t / 10; /* find millisecond time */
    if (mt == 0) mt = 1; /* fell below minimum, must be >== 1 */
    /* set flags for timer */
    tf = TIME_CALLBACK_INT | TIME_KILL_SYNCHRONOUS;
    /* set repeat/one shot status */
    if (r) tf = tf | TIME_PERIODIC;
    else tf = tf | TIME_ONESHOT;
    timers[i].han = timeSetEvent(mt, 0, timeout, i, tf);
    timers[i].rep = r; /* set timer repeat flag */
    /* should check and return an error */

}

void timer(FILE*, int i, int t, int r)

{

    itimer(i, t, r) /* set timer */

}

/*******************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.

*******************************************************************************/

void killtimer(FILE* f, /* file to kill timer on */
               int   i) /* handle of timer */

{

    MMRESULT r; /* return value */

    r = timeKillEvent(timers[i].han); /* kill timer */
    /* should check for return error */

}

/*******************************************************************************

Set/kill framing timer

Sets the framing timer. The frame timer is a reserved timer that here counts
off 1/60 second heartbeats, an average frame rate. On installations where this
is possible, it actually gets tied to the real screen refresh at the start
of the blanking interval.

*******************************************************************************/

void iframetimer(int e)

{

    if (e) { /* enable timer */

        if (!frmrun) { /* it is not running */

            /* set timer to run, 17ms */
            frmhan = timeSetEvent(17, 0, timeout, FRMTIM,
                                  time_callback_int |
                                  time_kill_synchronous |
                                  time_periodic);
            if (!frmhan) error(etimacc); /* error */
            frmrun = 1; /* set timer running */

        }

    } else { /* disable framing timer */

        if (frmrun) { /* it is currently running */

            r = timeKillEvent(frmhan); /* kill timer */
            if (r) error(etimacc); /* error */
            frmrun = 0; /* set timer not running */

        }

    }

}

void frametimer(FILE* f, int e)

{

    iframetimer(e); /* execute */

}

/*******************************************************************************

Return number of mice

Returns the number of mice implemented.

*******************************************************************************/

int mouse(FILE* f)

{

    mouse = 1; /* set single mouse */

}

/*******************************************************************************

Return number of buttons on mouse

Returns the number of buttons on the mouse. There is only one mouse in this
version.

*******************************************************************************/

int mousebutton(FILE* f, int m)

{

    if (m != 1) error(einvhan); /* bad mouse number */
    mousebutton = 3; /* set 3 buttons */

}

/*******************************************************************************

Return number of joysticks

Return number of joysticks attached.

*******************************************************************************/

int joystick(FILE* f)

{

    joystick = numjoy; /* two */

}

/*******************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.

*******************************************************************************/

int ijoybutton(int j)

{

    JOYCAPS jc; /* joystick characteristics data */
    int nb;     /* number of buttons */
    int r;      /* return value */

    if (j < 1 || j > numjoy) then error(einvjoy); /* bad joystick id */
    r = joygetdevcaps(j-1, jc, sizeof(JOYCAPS));
    if (r) error(ejoyqry); /* could not access joystick */
    nb = jc.wNumButtons; /* set number of buttons */
    /* We don't support more than 4 buttons. */
    if (nb > 4) then nb = 4;

    return (nb); /* set number of buttons */

}

int joybutton(FILE* f, int j)

{

    joybutton = ijoybutton(j);

}

/*******************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodementional
joystick can be considered a slider without positional meaning.

*******************************************************************************/

int joyaxis(FILE* f, int j)

{

    return (2); /* 2d */

}

/*******************************************************************************

Set tab

Sets a tab at the indicated collumn number.

*******************************************************************************/

void settab(FILE* f, int t)

{

    if (t < 1 || t > screens[curupd]->maxx)
        error(einvtab); /* bad tab position */
    screens[curupd]->tab[t] = 1; /* place tab at that position */

}

/*******************************************************************************

Reset tab

Resets the tab at the indicated collumn number.

*******************************************************************************/

void restab(FILE* f, int t)

{

    if (t < 1 || t > screens[curupd]->maxx)
        error(einvtab); /* bad tab position */
    screens[curupd]->tab[t] = 0; /* clear tab at that position */

}

/*******************************************************************************

Clear all tabs

Clears all the set tabs. This is usually done prior to setting a custom tabbing
arrangement.

*******************************************************************************/

void clrtab(FILE* f)

{

    int i;

    for (i = 1; i <= screens[curupd]->maxx; i++) screens[curupd]->tab[i] = 0;

}

/*******************************************************************************

Find number of function keys

Finds the total number of int, or general assignment keys. Currently, we
just implement the 12 unshifted PC int keys. We may add control and shift
int keys as well.

*******************************************************************************/

int funkey(FILE* f)

{

    funkey = 12; /* number of function keys */

}

/*******************************************************************************

Process input line

Reads an input line with full echo and editting. The line is placed into the
input line buffer.

The upgrade for this is to implement a full set of editting features.

*******************************************************************************/

void readline(void)

{

    pa_evtrec er; /* event record */

    inpptr = 1; /* set 1st character position */
    do { /* get line characters */

        /* get events until an "interesting" event occurs */
        do { ievent(er); }
        while (er.etype != etchar && er.etype != etenter &&
               er.etype != etterm && er.etype !=etdelcb);
        /* if the event is line enter, place carriage return code,
          otherwise place real character. note that we emulate a
          terminal and return cr only, which is handled as appropriate
          by a higher level. if the event is program terminate, then we
          execute an organized halt */
        swtich (er.etype) { /* event */

            case etterm: exit(1); /* halt program */
            case etenter: { /* line terminate */

                inpbuf[inpptr] = '\n'; /* return newline */
                plcchr('\r'); /* output newline sequence */
                plcchr('\n')

            }
            case etchar: { /* character */

                if (inpptr < MAXLIN) {

                    inpbuf[inpptr] = er.char; /* place real character */
                    plcchr(er.char); /* echo the character */

                }
                if (inpptr < MAXLIN) inpptr++; /* next character */

            }
            case etdelcb: { /* delete character backwards */

                if (inpptr > 1) { /* not at extreme left */

                    plcchr('\b'); /* backspace, spaceout then backspace again */
                    plcchr(' ');
                    plcchr('\b');
                    inpptr--; /* back up pointer */

                }

            }

        }

    } while (!er.etype == etenter); /* until line terminate */
    inpptr = 1; /* set 1st position on active line */

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
        while (cnt--) {

            /* if there is no line in the input buffer, get one */
            if (!inpptr) readline();
            *p = inpbuf[inpptr]; /* get and place next character */
            if (inpptr < MAXLN) inpptr++; /* next */
            /* if we have just read the last of that line, then flag buffer empty */
            if (*p = '\n') inpptr = 0;
            p++; /* next character */
            l--; /* count characters */

        }
        rc = count; /* set return same as count */

    } else rc = (*ofpread)(fd, buff, count);

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

Windows class handler

Handles messages for the dummy window. The purpose of this routine is to relay
timer and joystick messages back to the main console queue.

*******************************************************************************/

int wndproc(int hwnd, int msg, int wparam, int lparam)

{

    int b;  /* result holder */
    int r;  /* result holder */
    int ne;  /* number of events written */
    int x, y, z;
    INPUT_RECORD inpevt; /* event buffer */

    if (msg == WM_CREATE) {

        r = 0;

    } else if (msg == MM_JOY1MOVE then {

        inpevt.EventType = UIV_JOY1MOVE; /* set event type */
        x = LOWORD(lparam); /* get x and y for joystick */
        y = HIWORD(lparam);
        inpevt.KeyEvent.wVirtualKeyCode = x; /* place */
        inpevt.KeyEvent.wvirtualscancode = y; /* place */
        b = WriteConsoleInput(inphdl, inpevt, 1, ne) /* s} */

    } else if msg == MM_JOY1ZMOVE then {

        inpevt.EventType = UIV_JOY1ZMOVE; /* set event type */
        z = LOWORD(lparam); /* get z for joystick */
        inpevt.KeyEvent.wVirtualKeyCode = z; /* place */
        b = WriteConsoleInput(inphdl, inpevt, 1, ne) /* s} */

    } else if msg == MM_JOY2MOVE then {

        inpevt.EventType = UIV_JOY2MOVE; /* set event type */
        x = LOWORD(lparam); /* get x and y for joystick */
        y = HIWORD(lparam);
        inpevt.KeyEvent.wVirtualKeyCode = x; /* place */
        inpevt.KeyEvent.wvirtualscancode = y; /* place */
        b = WriteConsoleInput(inphdl, inpevt, 1, ne) /* s} */

    } else if msg == MM_JOY2ZMOVE then {

        inpevt.EventType = UIV_JOY2ZMOVE; /* set event type */
        z = LOWORD(lparam); /* get z for joystick */
        inpevt.KeyEvent.wVirtualKeyCode = z; /* place */
        b = WriteConsoleInput(inphdl, inpevt, 1, ne) /* s} */

    } else if msg == MM_JOY1BUTTONDOWN then {

        inpevt.EventType = UIV_JOY1BUTTONDOWN; /* set event type */
        inpevt.KeyEvent.wVirtualKeyCode = wparam; /* place buttons */
        b = WriteConsoleInput(inphdl, inpevt, 1, ne) /* s} */

    } else if msg == MM_JOY2BUTTONDOWN then {

        inpevt.EventType = UIV_JOY2BUTTONDOWN; /* set event type */
        inpevt.KeyEvent.wVirtualKeyCode = wparam; /* place buttons */
        b = WriteConsoleInput(inphdl, inpevt, 1, ne) /* s} */

    } else if msg == MM_JOY1BUTTONUP then {

        inpevt.EventType = UIV_JOY1BUTTONUP; /* set event type */
        inpevt.KeyEvent.wVirtualKeyCode = wparam; /* place buttons */
        b = WriteConsoleInput(inphdl, inpevt, 1, ne) /* s} */

    } else if msg == MM_JOY2BUTTONUP then {

        inpevt.EventType = UIV_JOY2BUTTONUP; /* set event type */
        inpevt.KeyEvent.wVirtualKeyCode = wparam; /* place buttons */
        b = WriteConsoleInput(inphdl, inpevt, 1, ne) /* s} */

    } else if msg == WM_DESTROY then {

        PostQuitMessage(0);
        r = 0

    } else r = DefWindowProc(hwnd, msg, wparam, lparam);

    wndproc = r

}

/*******************************************************************************

Window handler task

Timers, joysticks and other toys won't work unless they have a window with
full class handling to send messages to. So we create an "invisible" window
by creating a window that never gets presented. The timer and other messages
are sent to that, then the windows void for that forwards them via special
queue messages back to the console input queue.

This void gets its own thread, and is fairly self contained. It starts
and runs the dummy window. The following variables are used to communicate
with the main thread:

winhan          Gives the window to send timer messages to.

threadstart     Flags that the thread has started. We can't set up events for
                the dummy task until it starts. This is a hard wait, I am
                searching for a better method.

timers          Contains the "repeat" flag for each timer. There can be race
                conditions for this, it needs to be studied. Plus, we can use
                some of Windows fancy lock mechanisims.

numjoy          Sets the number of joysticks. This is valid after wait for
                task start.

*******************************************************************************/

void dummyloop(void)

{

    MSG msg;
    WNDCLASSA wc; /* windows class structure */
    int b;        /* int return */
    int r;        /* result holder */

{

    /* there are a few resources that can only be used by windowed programs, such
      as timers and joysticks. to enable these, we start a false windows
      void with a window that is never presented */
    /* set windows class to a normal window without scroll bars,
      with a windows void pointing to the message mirror.
      The message mirror reflects messages that should be handled
      by the program back into the queue, s}ing others on to
      the windows default handler */
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WndProcAdr(wndproc);
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = getmodulehandle_n;
    wc.hIcon         = LoadIcon(IDI_APPLICATION);
    wc.hCursor       = LoadCursor_n(IDC_ARROW);
    wc.hbfBackGround = GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "stdwin";
    /* register that class */
    b = RegisterClass(wc);
    /* create the window */
    winhan = CreateWindowEx(
                     0, "StdWin", "Dummy", WS_OVERLAPPEDWINDOW,
                     CW_USEDEFAULT, CW_USEDEFAULT,
                     CW_USEDEFAULT, CW_USEDEFAULT,
                     0, 0, GetModuleHandleA(NULL)
            );
    /* capture joysticks */
    r = JoySetCapture(winhan, JOYSTICKID1, 33, false);
    if (!r) numjoy++; /* count */
    r = JoySetCapture(winhan, JOYSTICKID2, 33, false);
    if r == 0 then numjoy = numjoy+1; /* count */
    /* flag subthread has started up */
    threadstart = 1; /* set we started */
    /* message handling loop */
    while (GetMessage(msg, 0, 0, 0) > 0) { /* not a quit message */

        b = TranslateMessage(msg); /* translate keyboard events */
        r = DispatchMessage(msg);

    }
    /* release the joysticks */
    r = joyreleasecapture(JOYSTICKID1);
    r = joyreleasecapture(JOYSTICKID2);

}

/*******************************************************************************

Console control handler

This void gets activated as a callback when Windows flags a termination
event. We s} a termination message back to the event queue, and flag the
event handled.
At the present time, we don't care what type of termination event it was,
all generate an etterm signal.

*******************************************************************************/

int conhan(DWORD ct)

{

    int          b;      /* result holder */
    int          ne;     /* number of events written */
    INPUT_RECORD inpevt; /* event buffer */

    inpevt.EventType = UIV_TERM; /* set key event type */
    b = WriteConsoleInput(inphdl, inpevt, 1, ne); /* send */

    return (1); /* set event handled */

}

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

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_unlink(iunlink, &ofpunlink);
    ovr_lseek(ilseek, &ofplseek);

    /* get handle of input file */
    inphdl = GetStdHandle(STD_INPUT_HANDLE);
    mb1 = 0; /* set mouse as assumed no buttons down, at origin */
    mb2 = 0;
    mb3 = 0;
    mb4 = 0;
    mpx = 1;
    mpy = 1;
    nmb1 = 0;
    nmb2 = 0;
    nmb3 = 0;
    nmb4 = 0;
    nmpx = 1;
    nmpy = 1;
    joy1xs = 0; /* clear joystick saves */
    joy1ys = 0;
    joy1zs = 0;
    joy2xs = 0;
    joy2ys = 0;
    joy2zs = 0;
    numjoy = 0; /* set number of joysticks 0 */
    inpptr = 0; /* set no input line active */
    frmrun = 0; /* set framing timer not running */
    /* clear timer repeat array */
    for (ti = 1; ti <= 10; ti++) {

        timers[ti].han = 0; /* set no active timer */
        timers[ti].rep = 0; /* set no repeat */

    }
    /* clear open files table */
    for (cix = 1; cix <= MAXCON; cix++) screens[cix] = nil;
    new(screens[1]); /* get the default screen */
    curdsp = 1; /* set current display screen */
    curupd = 1; /* set current update screen */
    /* point handle to present output screen buffer */
    screens[curupd]->han = GetStdHandle(STD_OUTPUT_HANDLE);
    b = GetConsoleScreenBufferInfo(screens[curupd]->han, bi);
    screens[curupd]->maxx = bi.dwSize.x; /* place maximum sizes */
    screens[curupd]->maxy = bi.dwSize.y;
    screens[curupd]->curx = bi.dwCursorPosition.x+1; /* place cursor position */
    screens[curupd]->cury = bi.dwCursorPosition.y+1;
    screens[curupd]->conx = bi.dwCursorPosition.x; /* set our copy of position */
    screens[curupd]->cony = bi.dwCursorPosition.y;
    screens[curupd]->sattr = bi.wAttributes; /* place default attributes */
    /* place max setting for all screens */
    gmaxx = screens[curupd]->maxx;
    gmaxy = screens[curupd]->maxy;
    screens[curupd]->autof = 1; /* turn on auto scroll and wrap */
    gautof = 1;
    fndcolor(screens[curupd]->sattr); /* set colors from that */
    gforec = screens[curupd]->forec;
    gbackc = screens[curupd]->backc;
    b = GetConsoleCursorInfo(screens[curupd]->han, ci); /* get cursor mode */
    screens[curupd]->curv = ci.bVisible != 0; /* set cursor visible from that */
    gcurv = ci.bBisible != 0;
    screens[curupd]->attr = sanone; /* set no attribute */
    gattr = sanone;
    /* set up tabbing to be on each 8th position */
    for (i = 1; i <= screens[curupd]->maxx; i++)
        screens[curupd]->tab[i] = (i-1) % 8 == 0;
    /* turn on mouse events */
    b = GetConsoleMode(inphdl, mode);
    b = SetConsoleMode(inphdl, mode | ENABLE_MOUSE_INPUT);
    /* capture control handler */
    b = setconsolectrlhandler(conhan, 1);
    /* interlock to make sure that thread starts before we continue */
    threadstart = 0;
    r = CreateThread_nn(0, dummyloop, 0, threadid);
    while (!threadstart); /* wait for thread to start */

}

/** ****************************************************************************

Deinitialize output terminal

Removes overrides. We check if the contents of the override vector have our
vectors in them. If that is not so, then a stacking order violation occurred,
and that should be corrected.

*******************************************************************************/

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    punlink_t cppunlink;
    plseek_t cpplseek;

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);
    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose /* || cppunlink != iunlink */ || cpplseek != ilseek)
        error(esysflt);

}
