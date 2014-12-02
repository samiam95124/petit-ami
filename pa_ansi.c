/** ****************************************************************************
 *
 * Petit AMI
 *
 * ANSI console interface
 *
 * This is a standard PA/TK terminal module using ANSI control codes. It is
 * useful on any terminal that uses ANSI control codes, mainly the VT100 and
 * emulations of it.
 *
 * This is a vestigal PA/TK terminal handler. It does not meet the full
 * standard for PA/TK terminal level interface. Instead it is meant to
 * provide a starting point to implementations such as Unix/Linux that don't
 * have an API to control the console, or for serial, telnet or ssh links to
 * a VT100 terminal emulation.
 *
 * This module is completely compatible with ANSI C.
 *
 * The module works by keeping a in memory image of the output terminal and
 * its attributes, along the lines of what curses does. Because it always knows
 * what the state of the actual terminal should be, it does not need to read
 * from the terminal to determine the state of individual character cells.
 *
 * In this version, the file argument is not used.
 *
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "pa_terminal.h"
//#include "stdio.h"


#define MAXXD 80  /**< standard terminal x, 80x25 */
#define MAXYD 43 /*25*/  /**< standard terminal x, 80x25 */
#define MAXCON 10 /**< number of screen contexts */

/* file handle numbers at the system interface level */

#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */
#define ERRFIL 2 /* handle to standard error */

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int);
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
      /* foreground color at location */ color forec;
      /* background color at location */ color backc;
      /* active attribute at location */ scnatt attr;

} scnrec;
typedef scnrec scnbuf[MAXYD][MAXXD];
/** Screen context */
typedef struct { /* screen context */

      /* screen buffer */                    scnbuf buf;
      /* current cursor location x */        int curx;
      /* current cursor location y */        int cury;
      /* current writing foreground color */ color forec;
      /* current writing background color */ color backc;
      /* current writing attribute */        scnatt attr;
      /* current status of scroll */         int scroll;

} scncon;
/** pointer to screen context block */ typedef scncon* scnptr;
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
    esysflt   /* system fault */

} errcod;

/*
 * keyboard key equivalents table
 *
 * Contains equivalent strings as are returned from xterm keys
 * attached to an IBM-PC keyboard.
 *
 */

char *keytab[etterm+1] = {

    "", /** ANSI character returned */
    "\33\133\101", /** cursor up one line */
    "\33\133\102", /** down one line */
    "\33\133\104", /** left one character */
    "\33\133\103", /** right one character */
    "\33\133\61\73\65\104", /** left one word */
    "\33\133\61\73\65\103", /** right one word */
    "", /** home of document */
    "", /** home of screen */
    "", /** home of line */
    "", /** end of document */
    "", /** end of screen */
    "", /** end of line */
    "", /** scroll left one character */
    "", /** scroll right one character */
    "\33\133\61\73\65\102", /** scroll up one line */
    "\33\133\61\73\65\101", /** scroll down one line */
    "", /** page down */
    "", /** page up */
    "\11", /** tab */
    "\12", /** enter line */
    "", /** insert block */
    "", /** insert line */
    "", /** insert toggle */
    "", /** delete block */
    "", /** delete line */
    "", /** delete character forward */
    "", /** delete character backward */
    "", /** copy block */
    "", /** copy line */
    "", /** cancel current operation */
    "", /** stop current operation */
    "", /** continue current operation */
    "", /** print document */
    "", /** print block */
    "", /** print screen */
    "", /** int key */
    "", /** display menu */
    "", /** mouse button assertion */
    "", /** mouse button deassertion */
    "", /** mouse move */
    "", /** timer matures */
    "", /** joystick button assertion */
    "", /** joystick button deassertion */
    "", /** joystick move */
    "", /** terminate program */

};

/* screen contexts array */           static scnptr screens[MAXCON];
/* index for current screen */        static int curscn;
/* array of event handler routines */ static pevthan evthan[etterm+1];

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t ofpread;
static pwrite_t ofpwrite;
static popen_t ofpopen;
static pclose_t ofpclose;
static punlink_t ofpunlink;
static plseek_t ofplseek;

/** ****************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.

*******************************************************************************/

static void error(errcod e)

{

    fprintf(stderr, "*** Error: Ansi: ");
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
        case esysflt: fprintf(stderr, "System fault"); break;

    }
    exit(1);

}

/** ****************************************************************************

Write character from input file

Reads a single character to from the input file. Used to read from the input file
directly.

*******************************************************************************/

static char getchr(void)

{

    char c; /* character read */
    ssize_t rc; /* return code */

    /* receive character to the next hander in the override chain */
    rc = (*ofpread)(INPFIL, &c, 1);

    if (rc != 1) error(einpdev); /* input device error */

    return c; /* return character */

}

/** ****************************************************************************

Write character to output file

Writes a single character to the output file. Used to write to the output file
directly.

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

    p = 1; /* set maximum power of 10 */
    while (p*10/10 == p) p *= 10;
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

Get keyboard code control match

Performs a successive match to keyboard input. A keyboard character is read,
and matched against the keyboard equivalence table. If we find a match, we keep
reading in characters until we get a single unambiguous matching entry.

If the match never results in a full match, the buffered characters are simply
discarded, and matching goes on with the next input character. Such "stillborn"
matches are either the result of ill considered input key equivalences, or of
a user typing in keys manually that happen to evaluate to special keys.

*******************************************************************************/

static void getkey(evtcod *evt, char *key)

{

    /** input key match buffer. This is sized to the longest control key
        sequence possible. */
    char   buf[10];
    int    len;
    int    pmatch; /* partial match found */
    int    ematch; /* exact match found */
    int    stillborn; /* stillborn match */
    evtcod i; /* index for events */

    len = 0;
    do { /* match input keys */

        buf[len++] = getchr(); /* get next character to match buffer */
        pmatch = 0; /* set no partial matches */
        ematch = 0; /* set no exact matches */
        stillborn = 0; /* no stillborn match */
        for (i = etchar; i <= etterm && !ematch; i++) {

            if (!strncmp(buf, keytab[i], len)) {

            	pmatch = 1; /* set partial match */
                *evt = i; /* set what event */
                /* set if the match is whole key */
                ematch = strlen(keytab[i]) == len;

            }

        }
        /* if no partial match, then something went wrong, or there never was
           at match at all. For such "stillborn" matches we start over */
        if (!pmatch && len > 1) {

        	len = 0; /* clear the buffer */
        	stillborn = 1; /* set stillborn match */

        }

    } while ((pmatch && !ematch) || stillborn); /* while substring match but not whole match */
    if (!ematch) {

    	*key = buf[0]; /* get our character from buffer */
    	*evt = etchar; /* set character event */

    }

 }

/** *****************************************************************************

Translate colors code

Translates an independent to a terminal specific primary color code for an
ANSI compliant terminal..

*******************************************************************************/

static int colnum(color c)

{

    /** numeric equivalent of color */ int n;

    /* translate color number */
    switch (c) { /* color */

        case black:   n = 0;  break;
        case white:   n = 7; break;
        case red:     n = 1; break;
        case green:   n = 2; break;
        case blue:    n = 4; break;
        case cyan:    n = 6; break;
        case yellow:  n = 3; break;
        case magenta: n = 5; break;

    }

    return n; /* return number */

}

/** ****************************************************************************

Basic terminal controls

These routines control the basic terminal functions. They exist just to
encapsulate this information. All of these functionss are specific to ANSI
compliant terminals.

ANSI is able to set more than one attribute at a time, but under windows 95
there are no two attributes that you can detect together ! This is because
win95 modifies the attributes quite a bit (there is no blink). This capability
can be replaced later if needed.

Other notes: underline only works on monochrome terminals. On color, it makes
the text turn blue.

*******************************************************************************/

/** clear screen and home cursor */
static void trm_clear(void) { putstr("\33[2J"); }
/** home cursor */ static void trm_home(void) { putstr("\33[H"); }
/** move cursor up */ static void trm_up(void) { putstr("\33[A"); }
/** move cursor down */ static void trm_down(void) { putstr("\33[B"); }
/** move cursor left */ static void trm_left(void) { putstr("\33[D"); }
/** move cursor right */ static void trm_right(void) { putstr("\33[C"); }
/** turn on blink attribute */ static void trm_blink(void) { putstr("\33[5m"); }
/** turn on reverse video */ static void trm_rev(void) { putstr("\33[7m"); }
/** turn on underline */ static void trm_undl(void) { putstr("\33[4m"); }
/** turn on bold attribute */ static void trm_bold(void) { putstr("\33[1m"); }
/** turn off all attributes */
static void trm_attroff(void) { putstr("\33[0m"); }
/** turn on cursor wrap */ static void trm_wrapon(void) { putstr("\33[7h"); }
/** turn off cursor wrap */ static void trm_wrapoff(void) { putstr("\33[7l"); }

/** set foreground color */
static void trm_fcolor(color c)
    { putstr("\33["); wrtint(30+colnum(c)); putstr("m"); }

/** set background color */
static void trm_bcolor(color c)
    { putstr("\33["); wrtint(40+colnum(c)); putstr("m"); };

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

Set attribute from attribute code

Accepts a "universal" attribute code, and executes the attribute set required
to make that happen on screen.

*******************************************************************************/

static void setattr(scnatt a)

{

    switch (a) { /* attribute */

        case sanone:  trm_attroff(); break; /* no attribute */
        case sablink: trm_blink();   break; /* blinking text (foreground) */
        case sarev:   trm_rev();     break; /* reverse video */
        case saundl:  trm_undl();    break; /* underline */
        case sasuper: break;                /* superscript */
        case sasubs:  break;                /* subscripting */
        case saital:  break;                /* italic text */
        case sabold:  trm_bold();    break; /* bold text */

    }

}

/** ****************************************************************************

Clear screen buffer

Clears the entire screen buffer to spaces with the current colors and
attributes.

*******************************************************************************/

static void clrbuf(void)

{

    /** screen indexes */           int x, y;
    /** pointer to screen record */ scnrec* sp;

    /* clear the screen buffer */
    for (y = 1;  y <= MAXYD; y++)
        for (x = 1; x < MAXXD; x++) {

        sp = &screens[curscn-1]->buf[y-1][x-1];
        sp->ch = ' '; /* clear to spaces */
        sp->forec = screens[curscn-1]->forec;
        sp->backc = screens[curscn-1]->backc;
        sp->attr = screens[curscn-1]->attr;

    }

}

/** ****************************************************************************

Initialize screen

Clears all the parameters in the present screen context, and updates the
display to match.

*******************************************************************************/

static void iniscn(void)

{

    screens[curscn-1]->cury = 1; /* set cursor at home */
    screens[curscn-1]->curx = 1;
    /* these attributes and colors are pretty much windows 95 specific. The
       Bizarre setting of "blink" actually allows access to bright white */
    screens[curscn-1]->forec = black; /* set colors and attributes */
    screens[curscn-1]->backc = white;
    screens[curscn-1]->attr = sablink;
    screens[curscn-1]->scroll = 1; /* turn on autoscroll */
    clrbuf(); /* clear screen buffer with that */
    setattr(screens[curscn-1]->attr); /* set current attribute */
    trm_fcolor(screens[curscn-1]->forec); /* set current colors */
    trm_bcolor(screens[curscn-1]->backc);
    trm_clear(); /* clear screen, home cursor */

}

/** ****************************************************************************

Restore screen

Updates all the buffer and screen parameters to the terminal.

*******************************************************************************/

static void restore(void)

{

    /** screen indexes */         int xi, yi;
    /** color saves */            color fs, bs;
    /** attribute saves */        scnatt as;
    /** screen element pointer */ scnrec *p;

    trm_home(); /* restore cursor to upper left to start */
    /* set colors and attributes */
    trm_fcolor(screens[curscn-1]->forec); /* restore colors */
    trm_bcolor(screens[curscn-1]->backc);
    setattr(screens[curscn-1]->attr); /* restore attributes */
    fs = screens[curscn-1]->forec; /* save current colors and attributes */
    bs = screens[curscn-1]->backc;
    as = screens[curscn-1]->attr;
    /* copy buffer to screen */
    for (yi = 1; yi <= MAXYD; yi++) { /* lines */

        for (xi = 1; xi <= MAXXD; xi++) { /* characters */

            /* for each new character, we compare the attributes and colors
               with what is set. if a new color or attribute is called for,
               we set that, and update the saves. this technique cuts down on
               the amount of output characters */
            p = &(screens[curscn-1]->buf[yi][xi]); /* index this screen element */
            if (p->forec != fs) { /* new foreground color */

                trm_fcolor(p->forec); /* set the new color */
                fs = p->forec; /* set save */

            };
            if (p->backc != bs) { /* new foreground color */

                trm_bcolor(p->backc); /* set the new color */
                bs = p->backc; /* set save */

            };
            if (p->attr != as) { /* new attribute */

                setattr(p->attr); /* set the new attribute */
                as = p->attr; /* set save */

            };
            putchr(p->ch); /* now output the actual character */

        };
        if (yi < MAXYD)
            /* output next line sequence on all lines but the last. this is
               because the last one would cause us to scroll */
            putstr("\r\n");

   };
   /* restore cursor position */
   trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
   trm_fcolor(screens[curscn-1]->forec); /* restore colors */
   trm_bcolor(screens[curscn-1]->backc);
   setattr(screens[curscn-1]->attr); /* restore attributes */

}

/** ****************************************************************************

Default event handler

If we reach this event handler, it means none of the overriders has handled the
event, but rather passed it down. We flag the event was not handled and return,
which will cause the event to return to the event() caller.

*******************************************************************************/

static void defaultevent(evtrec* ev)

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

static void iscroll(int x, int y)

{

    int     xi, yi;   /* screen counters */
    color   fs, bs;   /* color saves */
    scnatt  as;       /* attribute saves */
    scnbuf  scnsav;   /* full screen buffer save */
    int     lx;       /* last unmatching character index */
    int     m;        /* match flag */
    scnrec* sp;       /* pointer to screen record */

    if (y > 0 && x == 0) {

        /* downward straight scroll, we can do this with native scrolling */
        trm_cursor(1, MAXYD); /* position to bottom of screen */
        /* use linefeed to scroll. linefeeds work no matter the state of
           wrap, and use whatever the current background color is */
        yi = y;   /* set line count */
        while (yi > 0) {  /* scroll down requested lines */

            putchr('n');   /* scroll down */
            yi--;   /* count lines */

        }
        /* restore cursor position */
        trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
        /* now, adjust the buffer to be the same */
        for (yi = 1; yi <= MAXYD-1; yi++) /* move any lines up */
            if (yi+y <= MAXYD) /* still within buffer */
                /* move lines up */
                    memcpy(screens[curscn-1]->buf[yi],
                           screens[curscn-1]->buf[yi+y],
                           MAXXD * sizeof(scnrec));
        for (yi = MAXYD-y+1; yi <= MAXYD; yi++) /* clear blank lines at end */
            for (xi = 1; xi <= MAXXD; xi++) {

            sp = &screens[curscn-1]->buf[yi][xi];
            sp->ch = ' ';   /* clear to blanks at colors and attributes */
            sp->forec = screens[curscn-1]->forec;
            sp->backc = screens[curscn-1]->backc;
            sp->attr = screens[curscn-1]->attr;

        }

    } else { /* odd direction scroll */

        /* when the scroll is arbitrary, we do it by completely refreshing the
           contents of the screen from the buffer */
        if (x <= -MAXXD || x >= MAXXD || y <= -MAXYD || y >= MAXYD) {

            /* scroll would result in complete clear, do it */
            trm_clear();   /* scroll would result in complete clear, do it */
            clrbuf();   /* clear the screen buffer */
            /* restore cursor positition */
            trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);

        } else { /* scroll */

            /* true scroll is done in two steps. first, the contents of the buffer
               are adjusted to read as after the scroll. then, the contents of the
               buffer are output to the terminal. before the buffer is changed,
               we perform a full save of it, which then represents the "current"
               state of the real terminal. then, the new buffer contents are
               compared to that while being output. this saves work when most of
               the screen is spaces anyways */

            /* save the entire buffer */
            memcpy(scnsav, screens[curscn-1]->buf, sizeof(scnbuf));
            if (y > 0) {  /* move text up */

                for (yi = 1; yi <= MAXYD-1; yi++) /* move any lines up */
                    if (yi + y + 1 <= MAXYD) /* still within buffer */
                        /* move lines up */
                        memcpy(screens[curscn-1]->buf[yi],
                               screens[curscn-1]->buf[yi + y],
                               MAXXD*sizeof(scnrec));
                for (yi = MAXYD-y+1; yi <= MAXYD; yi++)
                    /* clear blank lines at end */
                    for (xi = 0; xi < MAXXD; xi++) {

                    sp = &screens[curscn-1]->buf[yi][xi];
                    sp->ch = ' ';   /* clear to blanks at colors and attributes */
                    sp->forec = screens[curscn-1]->forec;
                    sp->backc = screens[curscn-1]->backc;
                    sp->attr = screens[curscn-1]->attr;

                }

            } else if (y < 0) {  /* move text down */

                for (yi = MAXYD; yi >= 2; yi--)   /* move any lines up */
                    if (yi + y >= 1) /* still within buffer */
                        /* move lines up */
                        memcpy(screens[curscn-1]->buf[yi],
                               screens[curscn-1]->buf[yi+y],
                               MAXXD * sizeof(scnrec));
                for (yi = 1; yi <= abs(y); yi++) /* clear blank lines at start */
                    for (xi = 1; xi <= MAXXD; xi++) {

                    sp = &screens[curscn-1]->buf[yi][xi];
                    /* clear to blanks at colors and attributes */
                    sp->ch = ' ';
                    sp->forec = screens[curscn-1]->forec;
                    sp->backc = screens[curscn-1]->backc;
                    sp->attr = screens[curscn-1]->attr;

                }

            }
            if (x > 0) { /* move text left */

                for (yi = 1; yi <= MAXYD; yi++) { /* move text left */

                    for (xi = 1; xi <= MAXXD-1; xi++) /* move left */
                        if (xi + x + 1 <= MAXXD) /* still within buffer */
                            /* move characters left */
                            screens[curscn-1]->buf[yi][xi] =
                                screens[curscn-1]->buf[yi][xi+x];
                    /* clear blank spaces at right */
                    for (xi = MAXXD-x+1; xi <= MAXXD; xi++) {

                        sp = &screens[curscn-1]->buf[yi][xi];
                        /* clear to blanks at colors and attributes */
                        sp->ch = ' ';
                        sp->forec = screens[curscn-1]->forec;
                        sp->backc = screens[curscn-1]->backc;
                        sp->attr = screens[curscn-1]->attr;

                    }

                }

            } else if (x < 0) { /* move text right */

                for (yi = 1; yi <= MAXYD; yi++) { /* move text right */

                    for (xi = MAXXD; xi >= 2; xi--) /* move right */
                        if (xi+x >= 1) /* still within buffer */
                            /* move characters left */
                            screens[curscn-1]->buf[yi][xi] =
                                screens[curscn-1]->buf[yi][xi + x];
                    /* clear blank spaces at left */
                    for (xi = 1; xi <= abs(x); xi++) {

                        sp = &screens[curscn-1]->buf[yi][xi];
                        sp->ch = ' ';   /* clear to blanks at colors and attributes */
                        sp->forec = screens[curscn-1]->forec;
                        sp->backc = screens[curscn-1]->backc;
                        sp->attr = screens[curscn-1]->attr;

                    }

                }

            }
            /* the buffer is adjusted. now just copy the complete buffer to the
               screen */
            trm_home(); /* restore cursor to upper left to start */
            fs = screens[curscn-1]->forec; /* save current colors and attributes */
            bs = screens[curscn-1]->backc;
            as = screens[curscn-1]->attr;
            for (yi = 1; yi <= MAXYD; yi++) { /* lines */

                /* find the last unmatching character between real and new buffers.
                   Then, we only need output the leftmost non-matching characters
                   on the line. note that it does not really help us that characters
                   WITHIN the line match, because a character output is as or more
                   efficient as a cursor movement. if, however, you want to get
                   SERIOUSLY complex, we could check runs of matching characters,
                   then check if performing a direct cursor position is less output
                   characters than just outputing data :) */
                lx = MAXXD; /* set to end */
                do { /* check matches */

                    m = 1; /* set match */
                    /* check all elements match */
                    if (screens[curscn-1]->buf[yi][lx-1].ch != scnsav[yi][lx-1].ch)
                        m = 0;
                    if (screens[curscn-1]->buf[yi][lx-1].forec !=
                        scnsav[yi][lx-1].forec)
                        m = 0;
                    if (screens[curscn-1]->buf[yi][lx-1].backc !=
                        scnsav[yi][lx-1].backc)
                        m = 0;
                    if (screens[curscn-1]->buf[yi][lx-1].attr !=
                        scnsav[yi][lx-1].attr)
                        m = 0;
                    if (m) lx--; /* next character */

                } while (m && lx); /* until match or no more */
                for (xi = 1; xi <= lx; xi++) { /* characters */

                    /* for each new character, we compare the attributes and colors
                       with what is set. if a new color or attribute is called for,
                       we set that, and update the saves. this technique cuts down on
                       the amount of output characters */
                    sp = &screens[curscn-1]->buf[yi][xi];
                    if (sp->forec != fs) { /* new foreground color */

                        trm_fcolor(sp->forec); /* set the new color */
                        fs = sp->forec; /* set save */

                    }
                    if (sp->backc != bs) { /* new background color */

                        trm_bcolor(sp->forec); /* set the new color */
                        bs = sp->backc; /* set save */

                    }

                    if (sp->attr != as)  { /* new attribute */

                        setattr(sp->attr); /* set the new attribute */
                        as = sp->attr;   /* set save */

                    }
                    putchr(sp->ch);

                }
                if (yi + 1 < MAXYD)
                    /* output next line sequence on all lines but the last. this is
                       because the last one would cause us to scroll */
                    putstr("\r\\n");

            }
            /* restore cursor position */
            trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);
            trm_fcolor(screens[curscn-1]->forec);   /* restore colors */
            trm_bcolor(screens[curscn-1]->backc);   /* restore attributes */
            setattr(screens[curscn-1]->attr);

        }

    }

}

/** ****************************************************************************

Clear screen

Clears the screen and homes the cursor. This effectively occurs by writing all
characters on the screen to spaces with the current colors and attributes.

*******************************************************************************/

static void iclear(void)

{

    trm_clear(); /* erase screen */
    clrbuf(); /* clear the screen buffer */
    screens[curscn-1]->cury = 1; /* set cursor at home */
    screens[curscn-1]->curx = 1;

}

/** ****************************************************************************

Position cursor

Moves the cursor to the specified x and y location.

*******************************************************************************/

static void icursor(int x, int y)

{

    if (x >= 1 && x <= MAXXD && y >= 1 && y <= MAXYD) {

        if (x != screens[curscn-1]->curx || y != screens[curscn-1]->cury) {

            trm_cursor(x, y); /* position cursor */
            screens[curscn-1]->cury = y; /* set new position */
            screens[curscn-1]->curx = x;

        }

    } else error(einvpos); /* invalid position */

}

/** ****************************************************************************

Move cursor up internal

Moves the cursor position up one line.

*******************************************************************************/

static void iup(void)

{

    if (screens[curscn-1]->cury > 1) { /* not at top of screen */

        trm_up(); /* move up */
        screens[curscn-1]->cury = screens[curscn-1]->cury-1; /* update position */

    } else if (screens[curscn-1]->scroll) /* scroll enabled */
        iscroll(0, -1); /* at top already, scroll up */
    else { /* wrap cursor around to screen bottom */

        screens[curscn-1]->cury = MAXYD; /* set new position */
        /* update on screen */
        trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);

    }

}

/** ****************************************************************************

Move cursor down internal

Moves the cursor position down one line.

*******************************************************************************/

static void idown()

{

    if (screens[curscn-1]->cury < MAXYD) { /* not at bottom of screen */

        trm_down(); /* move down */
        screens[curscn-1]->cury = screens[curscn-1]->cury+1; /* update position */

    } else if (screens[curscn-1]->scroll) /* wrap enabled */
        iscroll(0, +1); /* already at bottom, scroll down */
    else { /* wrap cursor around to screen top */

        screens[curscn-1]->cury = 1; /* set new position */
        /* update on screen */
        trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);

    }

}

/** ****************************************************************************

Move cursor left internal

Moves the cursor one character left.

*******************************************************************************/

static void ileft()

{

    if (screens[curscn-1]->curx > 1) { /* not at extreme left */

        trm_left(); /* move left */
        screens[curscn-1]->curx = screens[curscn-1]->curx-1; /* update position */

    } else { /* wrap cursor motion */

        iup(); /* move cursor up one line */
        screens[curscn-1]->curx = MAXXD; /* set cursor to extreme right */
        /* position on screen */
        trm_cursor(screens[curscn-1]->curx, screens[curscn-1]->cury);

    }

}

/** ****************************************************************************

Move cursor right internal

Moves the cursor one character right.

*******************************************************************************/

static void iright()

{

    if (screens[curscn-1]->curx < MAXXD) { /* not at extreme right */

        trm_right(); /* move right */
        screens[curscn-1]->curx = screens[curscn-1]->curx+1; /* update position */

    } else { /* wrap cursor motion */

        idown(); /* move cursor up one line */
        screens[curscn-1]->curx = 1; /* set cursor to extreme left */
        putchr('\r'); /* position on screen */

    }

}

/** ****************************************************************************

Place next terminal character

Places the given character to the current cursor position using the current
colors and attribute.

*******************************************************************************/

static void plcchr(char c)

{

    scnrec* p;

    /* handle special character cases first */
    if (c == '\r') /* carriage return, position to extreme left */
        icursor(1, screens[curscn-1]->cury);
    else if (c == '\n') idown(); /* line feed, move down */
    else if (c == '\b') ileft(); /* back space, move left */
    else if (c == '\f') iclear(); /* clear screen */
    else if (c >= ' ' && c != 0x7f) {

        /* normal character case, not control character */
        putchr(c); /* output character to terminal */
        /* update buffer */
        p = &screens[curscn-1]->
                buf[screens[curscn-1]->cury-1][screens[curscn-1]->curx-1];
        p->ch = c; /* place character */
        p->forec = screens[curscn-1]->forec; /* place colors */
        p->backc = screens[curscn-1]->backc;
        p->attr = screens[curscn-1]->attr; /* place attribute */
        /* finish cursor right processing */
        if (screens[curscn-1]->curx < MAXXD) /* not at extreme right */
            screens[curscn-1]->curx++; /* update position */
        else
            /* Wrap being off, ANSI left the cursor at the extreme right. So now
               we can process our own version of move right */
            iright();

   }

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

Read

*******************************************************************************/

static ssize_t iread(int fd, void* buff, size_t count)

{

    return (*ofpread)(fd, buff, count);

}

/*******************************************************************************

Write

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    ssize_t rc; /* return code */
    char *p = (char *)buff;

    if (fd == OUTFIL) {

        /* send data to terminal */
        while (count--) plcchr(*p++);
        rc = count; /* set return same as count */

    } else rc = (*ofpwrite)(fd, buff, count);

    return rc;

}

/*******************************************************************************

Open

Terminal is assumed to be opened when the system starts, and closed when it
shuts down. Thus we do nothing for this.

*******************************************************************************/

static int iopen(const char* pathname, int flags)

{

    return (*ofpopen)(pathname, flags);

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

void cursor(FILE *f, int x, int y)

{

    icursor(x, y); /* position cursor */

}

/** ****************************************************************************

Return maximum x dimension

Returns the maximum x dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int maxx(FILE *f)

{

    return MAXXD; /* set maximum x */

}

/** ****************************************************************************

Return maximum y dimension

Returns the maximum y dimension, also equal to the number of columns in the
display. Because ANSI has no information return capability, this is preset.

*******************************************************************************/

int maxy(FILE *f)

{

    return MAXYD; /* set maximum y */

}

/** ****************************************************************************

Home cursor

Moves the cursor to the home position at (1, 1), the upper right hand corner.

*******************************************************************************/

void home(FILE *f)

{

    trm_home(); /* home cursor */
    screens[curscn-1]->cury = 1; /* set cursor at home */
    screens[curscn-1]->curx = 1;

}

/** ****************************************************************************

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

/** ****************************************************************************

Move cursor up

This is the external interface to up.

*******************************************************************************/

void up(FILE *f)

{

    iup(); /* move up */

}


/** ****************************************************************************

Move cursor down

This is the external interface to down.

*******************************************************************************/

void down(FILE *f)

{

    idown(); /* move cursor down */

}

/** ****************************************************************************

Move cursor left

This is the external interface to left.

*******************************************************************************/

void left(FILE *f)

{

   ileft(); /* move cursor left */

}

/** ****************************************************************************

Move cursor right

This is the external interface to right.

*******************************************************************************/

void right(FILE *f)

{

    iright(); /* move cursor right */

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

void blink(FILE *f, int e)

{

    trm_attroff(); /* turn off attributes */
    /* either on or off leads to blink, so we just do that */
    screens[curscn-1]->attr = sablink; /* set attribute active */
    setattr(screens[curscn-1]->attr); /* set current attribute */
    trm_fcolor(screens[curscn-1]->forec); /* set current colors */
    trm_bcolor(screens[curscn-1]->backc);

}

/** ****************************************************************************

Turn on reverse attribute

Turns on/off the reverse attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void reverse(FILE *f, int e)

{

    trm_attroff(); /* turn off attributes */
    if (e) { /* reverse on */

        screens[curscn-1]->attr = sarev; /* set attribute active */
        setattr(screens[curscn-1]->attr); /* set current attribute */
        trm_fcolor(screens[curscn-1]->forec); /* set current colors */
        trm_bcolor(screens[curscn-1]->backc);

    } else { /* turn it off */

        screens[curscn-1]->attr = sablink; /* set attribute active */
        setattr(screens[curscn-1]->attr); /* set current attribute */
        trm_fcolor(screens[curscn-1]->forec); /* set current colors */
        trm_bcolor(screens[curscn-1]->backc);

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

void underline(FILE *f, int e)

{

    trm_attroff(); /* turn off attributes */
    if (e) { /* underline on */

        screens[curscn-1]->attr = saundl; /* set attribute active */
        setattr(screens[curscn-1]->attr); /* set current attribute */
        trm_fcolor(screens[curscn-1]->forec); /* set current colors */
        trm_bcolor(screens[curscn-1]->backc);

    } else { /* turn it off */

        screens[curscn-1]->attr = sablink; /* set attribute active */
        setattr(screens[curscn-1]->attr); /* set current attribute */
        trm_fcolor(screens[curscn-1]->forec); /* set current colors */
        trm_bcolor(screens[curscn-1]->backc);

    }

}

/** ****************************************************************************

Turn on superscript attribute

Turns on/off the superscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void superscript(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Turn on subscript attribute

Turns on/off the subscript attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void subscript(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Turn on italic attribute

Turns on/off the italic attribute.
Note that the attributes can only be set singly.

*******************************************************************************/

void italic(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Turn on bold attribute

Turns on/off the bold attribute.
Note that the attributes can only be set singly.
Basically, the only way that I have found to reliably change attributes
on a PC is to turn it all off, then reset everything, including the
colors, which an ATTRIBUTE command seems to mess with !

*******************************************************************************/

void bold(FILE *f, int e)

{

    trm_attroff(); /* turn off attributes */
    if (e) { /* bold on */

        screens[curscn-1]->attr = sabold; /* set attribute active */
        setattr(screens[curscn-1]->attr); /* set current attribute */
        trm_fcolor(screens[curscn-1]->forec); /* set current colors */
        trm_bcolor(screens[curscn-1]->backc);

    } else { /* turn it off */

        screens[curscn-1]->attr = sablink; /* set attribute active */
        setattr(screens[curscn-1]->attr); /* set current attribute */
        trm_fcolor(screens[curscn-1]->forec); /* set current colors */
        trm_bcolor(screens[curscn-1]->backc);

    }

}

/** ****************************************************************************

Turn on strikeout attribute

Turns on/off the strikeout attribute.

Not impelemented.

*******************************************************************************/

void strikeout(FILE *f, int e)

{

}

/** ****************************************************************************

Turn on standout attribute

Turns on/off the standout attribute. Standout is implemented as reverse video.
Note that the attributes can only be set singly.

*******************************************************************************/

void standout(FILE *f, int e)

{

    reverse(f, e); /* implement as reverse */

}

/** ****************************************************************************

Set foreground color

Sets the foreground (text) color from the universal primary code.

*******************************************************************************/

void fcolor(FILE *f, color c)

{

    trm_fcolor(c); /* set color */
    screens[curscn-1]->forec = c;

}

/** ****************************************************************************

Set background color

Sets the background color from the universal primary code.

*******************************************************************************/

void bcolor(FILE *f, color c)

{

    trm_bcolor(c); /* set color */
    screens[curscn-1]->backc = c;

}

/** ****************************************************************************

Enable/disable automatic scroll

Enables or disables automatic screen scroll. With automatic scroll on, moving
off the screen at the top or bottom will scroll up or down, respectively.

*******************************************************************************/

void automode(FILE *f, int e)

{

    screens[curscn-1]->scroll = e; /* set line wrap status */

}

/** ****************************************************************************

Enable/disable cursor visibility

Enable or disable cursor visibility. We don't have a capability for this.

*******************************************************************************/

void curvis(FILE *f, int e)

{

    /* no capability */

}

/** ****************************************************************************

Scroll screen

Process full delta scroll on screen. This is the external interface to this
int.

*******************************************************************************/

void scroll(FILE *f, int x, int y)

{

    iscroll(x, y); /* process scroll */

}

/** ****************************************************************************

Get location of cursor in x

Returns the current location of the cursor in x.

*******************************************************************************/

int curx(FILE *f)

{

    return screens[curscn-1]->curx; /* return current location x */

}

/** ****************************************************************************

Get location of cursor in y

Returns the current location of the cursor in y.

*******************************************************************************/

int cury(FILE *f)

{

    return screens[curscn-1]->cury; /* return current location y */

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

void selects(FILE *f, int u, int d)

{

    if (d < 1 || d > MAXCON) error(einvscn); /* invalid screen number */
    curscn = d; /* set the current display screen */
    if (screens[curscn-1]) restore(); /* restore current screen */
    else { /* no current screen, create a new one */

        /* get a new screen context */
        screens[curscn-1] = (scncon*)malloc(sizeof(scncon));
        iniscn(); /* initalize that */

    }

}

/** ****************************************************************************

Acquire next input event

Only character input from the standard input is supported.

*******************************************************************************/

void event(FILE* f, evtrec *er)

{

    do { /* loop handling via event vectors */

    	/* get next key event */
    	getkey(&er->etype, &er->echar);
        er->handled = 1; /* set event is handled by default */
        /* decode alternate character types */
        if (er->echar == '\n') er->etype = etenter;
        (*evthan[er->etype])(er); /* call event handler first */

    } while (er->handled);
    /* event not handled, return it to the caller */

}

/** ****************************************************************************

Set timer

No timer is implemented.

*******************************************************************************/

void timer(/* file to s} event to */                FILE *f,
           /* timer handle */                       int i,
           /* number of milliseconds to run */      int t,
           /* timer is to rerun after completion */ int r)

{

    error(etimacc); /* no timers available */

}

/** ****************************************************************************

Kill timer

Kills a given timer, by it's id number. Only repeating timers should be killed.
Note: timers are not implemented. I tried this under win95, it was
uncooperative.

*******************************************************************************/

void killtimer(/* file to kill timer on */ FILE *f,
               /* handle of timer */       int i)

{

    error(etimacc); /* no timers available */

}

/** ****************************************************************************

Returns number of mice

Returns the number of mice attached.

*******************************************************************************/

int mouse(FILE *f)

{

    return 0; /* set no mice */

}

/** ****************************************************************************

Returns number of buttons on a mouse

Returns the number of buttons implemented on a given mouse.

*******************************************************************************/

int mousebutton(FILE *f, int m)

{

    error(emouacc); /* there are no mice */

    return 0; /* shut up compiler */

}

/** ****************************************************************************

Return number of joysticks

Return number of joysticks attached.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

int joystick(FILE *f)

{

    return 0; /* none */

}

/** ****************************************************************************

Return number of buttons on a joystick

Returns the number of buttons on a given joystick.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

int joybutton(FILE *f, int j)

{

    error(ejoyacc); /* there are no joysticks */

    return 0; /* shut up compiler */

}

/** ****************************************************************************

Return number of axies on a joystick

Returns the number of axies implemented on a joystick, which can be 1 to 3.
The axies order of implementation is x, y, then z. Typically, a monodementional
joystick can be considered a slider without positional meaning.
Note that Windows 95 has no joystick capability.

*******************************************************************************/

int joyaxis(FILE *f, int j)

{

    error(ejoyacc); /* there are no joysticks */

    return 0; /* shut up compiler */

}

/** ****************************************************************************

settab

Sets a tab. The tab number t is 1 to n, and indicates the column for the tab.
Setting a tab stop means that when a tab is received, it will move to the next
tab stop that is set. If there is no next tab stop, nothing will happen.

Not implemented.

*******************************************************************************/

void settab(FILE* f, int t)

{

}

/** ****************************************************************************

restab

Resets a tab. The tab number t is 1 to n, and indicates the column for the tab.

Not implemented.

*******************************************************************************/

void restab(FILE* f, int t)

{

}

/** ****************************************************************************

clrtab

Clears all tabs.

Not implemented.

*******************************************************************************/

void clrtab(FILE* f)

{

}

/** ****************************************************************************

funkey

Return number of function keys.

Not implemented.

*******************************************************************************/

int funkey(FILE* f)

{

}

/** ****************************************************************************

Frametimer

Enables or disables the framing timer.

Not currently implemented.

*******************************************************************************/

void frametimer(FILE* f, int e)

{

}

/** ****************************************************************************

Autohold

Turns on or off automatic hold mode.

We don't implement automatic hold here.

*******************************************************************************/

void autohold(FILE* f, int e)

{

}

/** ****************************************************************************

Write string direct

Writes a string direct to the terminal, bypassing character handling.

*******************************************************************************/

void wrtstr(FILE* f, char *s)

{

    putstr(s);

}

/** ****************************************************************************

Write string direct with length

Writes a string with length direct to the terminal, bypassing character
handling.

*******************************************************************************/

void wrtstrn(FILE* f, char *s, int n)

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

void eventover(evtcod e, pevthan eh,  pevthan* oeh)

{

    *oeh = evthan[e]; /* save existing event handler */
    evthan[e] = eh; /* place new event handler */

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

static void init_terminal (void) __attribute__((constructor (102)));
static void init_terminal()

{

    /* index for events */ evtcod e;

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_unlink(iunlink, &ofpunlink);
    ovr_lseek(ilseek, &ofplseek);

    /* clear screens array */
    for (curscn = 1; curscn <= MAXCON; curscn++) screens[curscn-1] = 0;
    screens[0] = (scncon*)malloc(sizeof(scncon)); /* get the default screen */
    curscn = 1; /* set current screen */
    trm_wrapoff(); /* wrap is always off */
    iniscn(); /* initalize screen */
    for (e = etchar; e <= etterm; e++) evthan[e] = defaultevent;

}

/** ****************************************************************************

Deinitialize output terminal

Removes overrides. We check if the contents of the override vector have our
vectors in them. If that is not so, then a stacking order violation occurred,
and that should be corrected.

*******************************************************************************/

static void deinit_terminal (void) __attribute__((destructor (102)));
static void deinit_terminal()

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
        cppclose != iclose || cppunlink != iunlink || cpplseek != ilseek)
        error(esysflt);

}
