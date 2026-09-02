/*******************************************************************************
*                                                                              *
*                                BREAKOUT GAME                                 *
*                                                                              *
*                       COPYRIGHT (C) 2002 S. A. FRANCO                        *
*                                                                              *
* Plays breakout in windowed text mode: the character edition of breakoutwg.   *
* The window manager carries the frame and title, the menu bar carries the     *
* game controls and the help, and the game -- real time physics, the ball at   *
* arbitrary angles, the paddle by keyboard, joystick, or mouse capture, and    *
* the sounds -- is drawn in character cells, updated in place. The help is a   *
* window of its own, read from the topic file breakout.md.                     *
*                                                                              *
*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <sound.h>
#include <terminalw.h>

/* enable sounds */
#define SOUND 1

#define   SECOND      10000                   /* one second */
#define   OSEC        (SECOND/8)              /* 1/8 second */
#define   BALMOV      75                      /* ball move timer: one step each
                                                 7.5ms, machine independent;
                                                 lower for a faster ball */
#define   NEWBAL      (SECOND/BALMOV)         /* ticks to wait for new ball */
#define   FANWAIT     ((OSEC*13+SECOND)/BALMOV) /* ticks to wait out fanfare */
#define   BALLCLR     ami_blue                 /* ball color */
#define   WALLCLR     ami_cyan                 /* wall color */
#define   PADCLR      ami_green                /* paddle color */
#define   CAPCLR      ami_yellow               /* paddle color when captured */
#define   CAPDWELL    (SECOND/BALMOV)          /* ticks of held press to capture */
#define   RELDWELL    (SECOND/BALMOV)          /* ticks of held press to release */
#define   PADEDGE     0.9                     /* how hard the strike point turns
                                                 the ball, in speed fractions */
#define   MINVERT     0.30                    /* minimum vertical speed fraction,
                                                 keeping shots off horizontal */
#define   BOUNCETIME  250                     /* time to play bounce note */
#define   WALLNOTE    (AMI_NOTE_D+AMI_OCTAVE_6) /* note to play off wall */
#define   BRICKNOTE   (AMI_NOTE_E+AMI_OCTAVE_7) /* note to play off brick */
#define   FAILTIME    1500                    /* note to play on failure */
#define   FAILNOTE    (AMI_NOTE_C+AMI_OCTAVE_4) /* note to play on fail */
#define   BRKROW      6                       /* number of brick rows */
#define   BRKCOL      10                      /* number of brick columns */

/* menu selections */
#define   MENU_NEW    1                       /* Game: New Game */
#define   MENU_EXIT   2                       /* Game: Exit */
#define   MENU_PLAY   3                       /* Help: Game Play */
#define   MENU_ABOUT  4                       /* Help: About */

/* the help window and its widgets */
#define   HELPWIN     7                       /* the help window */
#define   HELPFIND    1                       /* the search entry */
#define   HELPLIST    2                       /* the topic list */
#define   HELPCLOSE   3                       /* the close button */
#define   HELPFILE    "breakout.md"           /* the topic file */
#define   WHEELROWS   3                       /* help lines per wheel notch */

/* the about window */
#define   ABOUTWIN    8                       /* the about window */
#define   ABOUTOK     1                       /* its OK button */

typedef struct { /* rectangle, in character cells */

    int x1, y1, x2, y2;

} rectangle;

int       padx;                       /* paddle position x */
float     bvx;                        /* ball velocity x, cells per tick */
float     bvy;                        /* ball velocity y, cells per tick */
float     bfx;                        /* ball exact position x */
float     bfy;                        /* ball exact position y; the cell is
                                         the rounding of the exact position,
                                         so any angle resolves to cells the
                                         way a line draw does */
int       baltim;                     /* ball start timer */
ami_evtrec er;                         /* event record */
ami_long  jchr;                       /* joystick units per character */
int       score;                      /* score */
int       scrchg;                     /* score has changed */
rectangle paddle;                     /* paddle rectangle */
rectangle lpaddle;                    /* paddle as last drawn */
rectangle ball;                       /* ball rectangle (one cell) */
rectangle lball;                      /* ball as last drawn, 0 for none */
rectangle wallt, walll, wallr, wallb; /* wall rectangles */
rectangle bricks[BRKROW][BRKCOL];     /* brick array */
int       brki;                       /* brick was intersected */
int       fldbrk;                     /* bricks hit this field */
int       padw;                       /* paddle width */
int       hpadw;                      /* half paddle width */
int       padstp;                     /* paddle step per key event */
ami_long  joylst;                     /* last joystick x reported */
int       joyini;                     /* joystick baseline taken */
int       mcap;                       /* paddle is captured to the mouse */
int       lstmx;                      /* last mouse position x */
int       lstmy;                      /* last mouse position y */
int       btndn;                      /* mouse button is down */
int       relarm;                     /* button released since capture: the
                                         next held press releases */
int       dwltim;                     /* held-press countdown to capture */
int       reltim;                     /* held-press countdown to release */
char*     progpath;                   /* the program path, to find the help
                                         file beside it */
FILE*     aboutwf;                    /* the about window, NULL when closed */

/*******************************************************************************

Write string to screen

Writes a string to the indicated position on the screen.

********************************************************************************/

void writexy(int x, int y,   /* position to write to */
             const char* s)  /* string to write */

{

    ami_cursor(stdout, x, y); /* position cursor */
    fputs(s, stdout); /* output string */

}

/*******************************************************************************

Write centered string

Writes a string that is centered on the line given.

********************************************************************************/

void wrtcen(int         y, /* y position of string */
            const char* s) /* string to write */

{

    writexy(ami_maxx(stdout)/2-strlen(s)/2, y, s);

}

/*******************************************************************************

Fill rectangle

Fills a cell rectangle with spaces in the given background color.

********************************************************************************/

void filrect(rectangle* r, ami_color c)

{

    int x, y;

    ami_bcolor(stdout, c); /* set color */
    for (y = r->y1; y <= r->y2; y++) {

        ami_cursor(stdout, r->x1, y);
        for (x = r->x1; x <= r->x2; x++) putchar(' ');

    }
    ami_bcolor(stdout, ami_white); /* restore background */

}

/*******************************************************************************

Rationalize a rectangle

Arranges the points so that the 1st point is lower in x and y than the second.

********************************************************************************/

void ratrect(rectangle* r)

{

    int t; /* swap temp */

    if (r->x1 > r->x2) { t = r->x1; r->x1 = r->x2; r->x2 = t; }
    if (r->y1 > r->y2) { t = r->y1; r->y1 = r->y2; r->y2 = t; }

}

/*******************************************************************************

Find intersection of rectangles

Checks if two rectangles intersect. Returns true if so.

********************************************************************************/

int intsec(rectangle* r1, rectangle* r2)

{

    ratrect(r1);
    ratrect(r2);

    return (r1->x2 >= r2->x1 && r1->x1 <= r2->x2 &&
            r1->y2 >= r2->y1 && r1->y1 <= r2->y2);

}

/*******************************************************************************

Set rectangle

Sets the rectangle to the given values.

********************************************************************************/

void setrct(rectangle* r, int x1, int y1, int x2, int y2)

{

    r->x1 = x1;
    r->y1 = y1;
    r->x2 = x2;
    r->y2 = y2;

}

/*******************************************************************************

Clear rectangle

Clear rectangle points to zero. Usually used to flag the rectangle invalid.

*******************************************************************************/

void clrrect(rectangle* r)

{

    r->x1 = 0;
    r->y1 = 0;
    r->x2 = 0;
    r->y2 = 0;

}

/*******************************************************************************

Help topics

A window of the program's own, not a dialog. A dialog stops the program
and runs a loop of its own until it is answered; help is not a question,
so it opens beside the mail and stays up while the mail is worked on.

That takes no machinery. There is one event queue for the program, and
every event names the window it came from, so the main loop tells the
help window's events from the mail's by their window id and hands them
here. Nothing is nested, and no thread is needed -- though one could be
put in charge of this window just as well, since its state is all here.

Widgets are numbered within their window, so the search entry, the topic
list and the close button are 1, 2 and 3 here even though the mail's
scroll bars are 1 and 2 there.

*******************************************************************************/

/* A topic is a title and the text under it, both pointing into the
   block the help file was read into. */
typedef struct { char* title; char* text; } helprec;

/* The wrapped text, one entry per line as it appears on the screen. The
   text is wrapped once, when the topic is picked or the window resized,
   and drawn from there, which is what makes it scrollable. */
typedef struct { char* s; int bold; ami_long ind; } helpline;

static FILE*     helpwf;      /* the help window, NULL when closed */
static char*     helpbuf;     /* the help file, read whole */
static helprec*  helptopics;  /* the topics in it */
static ami_long  helptopicct;
static ami_long*     helpmatch;   /* the topics the search matched */
static ami_long  helpmatches; /* how many of them */
static ami_long  helpsel;     /* the topic shown, -1 for none */
static ami_long  helpx0, helpy0; /* the topic list, in pixels */
static ami_long  helpx1, helpy1;
static int       helplistup;  /* the list box has been made */
static helpline* helplines;   /* the topic, wrapped to the pane */
static ami_long  helplinect;
static ami_long  helplinemax;
static ami_long  helptop;     /* first wrapped line shown */
static ami_long  helppage;    /* wrapped lines the pane holds */

static void helpout(const char* s, int bold, ami_long ind); /* forward */

/*******************************************************************************

Reading the help file

The file is markdown, of the plain kind: a line beginning with a single
# names a topic and everything after it belongs to that topic until the
next one or the end of the file. Within a topic a blank line separates
paragraphs, ## is a heading inside the topic and - is a list item.

Keeping the text in a file rather than in the program means the help can
be rewritten, corrected or translated without a compiler, and it can be
as long as it deserves to be. The file is read whole and the titles and
texts point into it, so a topic costs two pointers.

*******************************************************************************/

/* is this the start of a topic: one #, then a space, then a title? */
static int helphead(const char* p)

{

    return (*p == '#' && p[1] != '#');

}

/* Find and read the help file. It is looked for beside the program
   first, since that is where an installed program's own files belong,
   then in the source directory it is kept in, then where the program
   was run from. The first one found is the one used. */
static int helpread(void)

{

    char  path[600];
    char  dir[500];
    char* e;
    FILE* f = NULL;
    ami_long  i, n;

    /* the directory the program was run from, with its slash */
    dir[0] = 0;
    if (progpath) {

        snprintf(dir, sizeof(dir), "%s", progpath);
        e = strrchr(dir, '/');
        if (e) e[1] = 0; else dir[0] = 0;

    }
    for (i = 0; i < 4 && !f; i++) {

        switch (i) {

            /* beside the program, then the source beside that, which is
               where it sits in a build tree, then the two the same way
               from wherever the program was run */
            case 0: snprintf(path, sizeof(path), "%s%s", dir, HELPFILE);
                    break;
            case 1: snprintf(path, sizeof(path), "%s../graph_games/%s",
                             dir, HELPFILE); break;
            case 2: snprintf(path, sizeof(path), "%s", HELPFILE); break;
            default: snprintf(path, sizeof(path), "graph_games/%s",
                              HELPFILE); break;

        }
        f = fopen(path, "r");

    }
    if (!f) return (FALSE);
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    helpbuf = malloc(n+1);
    if (!helpbuf) { ami_alert("Mail", "Out of memory"); exit(1); }
    n = fread(helpbuf, 1, n, f);
    helpbuf[n] = 0;
    fclose(f);

    return (TRUE);

}

/* Break the file into topics. The heads are found first and the block
   cut afterwards, in that order: cutting as it went would overwrite the
   newline that the test for the next head stands on, and every other
   topic would be passed over. */
static void helpsplit(void)

{

    char*  p;
    char** head; /* the # of each topic */
    ami_long   n, i;

    /* count the heads, then take them, walking by lines both times */
    n = 0;
    for (p = helpbuf; *p; ) {

        if (helphead(p)) n++;
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }
    head = malloc((n+1)*sizeof(char*));
    helptopics = malloc((n+1)*sizeof(helprec));
    helpmatch = malloc((n+1)*sizeof(ami_long));
    if (!head || !helptopics || !helpmatch)
        { ami_alert("Mail", "Out of memory"); exit(1); }
    n = 0;
    for (p = helpbuf; *p; ) {

        if (helphead(p)) head[n++] = p;
        while (*p && *p != '\n') p++;
        if (*p) p++;

    }
    helptopicct = n;
    /* the title is the rest of the head line, the text is what follows */
    for (i = 0; i < n; i++) {

        char* t = head[i]+1;
        char* e;

        while (*t == ' ') t++;
        helptopics[i].title = t;
        e = t;
        while (*e && *e != '\n') e++;
        if (*e) *e++ = 0; /* end the title */
        while (*e == '\n') e++; /* and the blank line under it */
        helptopics[i].text = e;

    }
    /* Now the cutting: each text ends where the next topic begins. The
       last ends at the end of the file, which is already a zero. */
    for (i = 0; i+1 < n; i++) *head[i+1] = 0;

}

/* load the help, once, and say so if there is none */
static void helpload(void)

{

    static char nofile[400];

    if (helptopics) return; /* already loaded */
    if (helpread()) helpsplit();
    if (!helptopicct) { /* no file, or a file with no topics in it */

        snprintf(nofile, sizeof(nofile),
                 "The help file %s was not found, or holds no topics.\n"
                 "\n"
                 "It is looked for beside the program and in the "
                 "graph_programs directory of the source. Help is kept in "
                 "a file so that it can be changed without rebuilding the "
                 "program; if the file is missing, only the help is.",
                 HELPFILE);
        helptopics = malloc(sizeof(helprec));
        helpmatch = malloc(sizeof(ami_long));
        if (!helptopics || !helpmatch)
            { ami_alert("Mail", "Out of memory"); exit(1); }
        helptopics[0].title = "No help file";
        helptopics[0].text = nofile;
        helptopicct = 1;

    }

}

/*******************************************************************************

The topic list

*******************************************************************************/

/* How many times does the topic hold the text, in either case? A count
   rather than a yes or no, because the list shows it: a topic the word
   is the subject of holds it many times, and one that merely mentions
   it in passing holds it once, and the reader can tell them apart
   without opening either. */
static ami_long helpcount(const helprec* h, const char* what)

{

    const char* p;
    ami_long    n = strlen(what);
    ami_long    c = 0;

    if (!n) return (0); /* an empty search matches everything, uncounted */
    for (p = h->title; *p; p++)
        if (!strncasecmp(p, what, n)) c++;
    for (p = h->text; *p; p++)
        if (!strncasecmp(p, what, n)) c++;

    return (c);

}

/* Build the list of topics matching the search and put it in the list
   box. The list box is made again rather than changed, since a list box
   is given its contents when it is made; it copies them, so the list
   built here is ours to free. */
static void helpfill(const char* what)

{

    ami_strptr sl = NULL, sp, lp = NULL;
    ami_long   i, c;
    char       lab[300];

    /* the strings are ours until the list box has them; it copies */
    helpmatches = 0;
    for (i = 0; i < helptopicct; i++) {

        c = helpcount(&helptopics[i], what);
        if (*what && !c) continue; /* not this one */
        /* the count goes beside the title, so that a topic the word is
           the subject of can be told from one that mentions it once */
        if (*what) snprintf(lab, sizeof(lab), "%s (%lld)",
                            helptopics[i].title, AMI_LONG_CAST(c));
        else snprintf(lab, sizeof(lab), "%s", helptopics[i].title);
        sp = malloc(sizeof(ami_strrec));
        if (!sp) { ami_alert("Mail", "Out of memory"); exit(1); }
        sp->str = strdup(lab);
        if (!sp->str) { ami_alert("Mail", "Out of memory"); exit(1); }
        sp->next = NULL;
        if (lp) lp->next = sp; else sl = sp;
        lp = sp;
        helpmatch[helpmatches++] = i;

    }
    /* a search that matches nothing still needs a list, or there would
       be no box to type the next search against */
    if (!sl) {

        sl = malloc(sizeof(ami_strrec));
        if (!sl) { ami_alert("Mail", "Out of memory"); exit(1); }
        sl->str = strdup("(no topic matches)");
        if (!sl->str) { ami_alert("Mail", "Out of memory"); exit(1); }
        sl->next = NULL;

    }
    if (helplistup) ami_killwidget(helpwf, HELPLIST);
    ami_listbox(helpwf, helpx0, helpy0, helpx1, helpy1, sl, HELPLIST);
    helplistup = TRUE;
    while (sl) { sp = sl->next; free(sl->str); free(sl); sl = sp; }
    /* the topic shown is only still shown if the search kept it */
    if (helpsel >= 0) {

        for (i = 0; i < helpmatches; i++) if (helpmatch[i] == helpsel) break;
        if (i >= helpmatches) helpsel = -1;

    }

}

/*******************************************************************************

Laying the topic out

The text is wrapped to the pane once and kept as lines, so that drawing
it, scrolling it and knowing how much of it there is are all the same
small piece of work. It is wrapped again when the window is resized,
which is the only thing that can change the answer.

*******************************************************************************/

/* keep one finished line */
static void helpout(const char* s, int bold, ami_long ind)

{

    if (helplinect >= helplinemax) {

        helplinemax = helplinemax? helplinemax*2: 100;
        helplines = realloc(helplines, helplinemax*sizeof(helpline));
        if (!helplines) { ami_alert("Mail", "Out of memory"); exit(1); }

    }
    helplines[helplinect].s = strdup(s);
    if (!helplines[helplinect].s)
        { ami_alert("Mail", "Out of memory"); exit(1); }
    helplines[helplinect].bold = bold;
    helplines[helplinect].ind = ind;
    helplinect++;

}

/* Wrap one paragraph, which arrives as a single string with its line
   breaks already turned into spaces. The break goes at the last word
   that still fits, and fitting is measured with the font rather than
   counted in characters, since the font is not fixed pitch. */
static void helpwrap(const char* s, int bold, ami_long ind, ami_long w)

{

    char line[500];
    char try[500];
    ami_long n;

    ami_bold(helpwf, bold);
    while (*s) {

        const char* q = s;

        n = 0;
        line[0] = 0;
        while (*q) { /* as many whole words as fit */

            const char* e = q;
            ami_long    m;

            while (*e && *e != ' ') e++; /* the next word */
            m = e-s;
            if (m >= (ami_long)sizeof(try)) break;
            memcpy(try, s, m);
            try[m] = 0;
            if (n && (ami_long)strlen(try) > w-ind) break;
            strcpy(line, try);
            n = m;
            q = e;
            while (*q == ' ') q++;
            if (!*e) break;

        }
        if (!n) { /* one word wider than the pane: put it down anyway */

            while (*q && *q != ' ') q++;
            n = q-s;
            if (n >= (ami_long)sizeof(line)) n = sizeof(line)-1;
            memcpy(line, s, n);
            line[n] = 0;

        }
        helpout(line, bold, ind);
        s += n;
        while (*s == ' ') s++;

    }
    ami_bold(helpwf, FALSE);

}

/* Wrap the topic being shown to the pane. The markdown is read here: a
   blank line ends a paragraph, ## is a heading within the topic, and -
   is a list item, which is wrapped with its later lines lined up under
   the first word rather than under the dash. */
static void helplay1(ami_long w)

{

    const char* p;
    char        para[4000];
    ami_long    pl = 0;
    int         bold = FALSE;
    ami_long    ind = 0;
    ami_long    i;

    for (i = 0; i < helplinect; i++) free(helplines[i].s);
    helplinect = 0;
    helptop = 0;
    if (helpsel < 0) { helpout("Pick a topic.", FALSE, 0); return; }
    /* the title of the topic, in bold, and a line under it */
    helpout(helptopics[helpsel].title, TRUE, 0);
    helpout("", FALSE, 0);
    p = helptopics[helpsel].text;
    while (1) {

        const char* e = p;
        ami_long    n;

        while (*e && *e != '\n') e++;
        n = e-p;
        while (n && (p[n-1] == ' ' || p[n-1] == '\r')) n--; /* trailing */
        if (!n || *p == '#' || *p == '-' || *p == '*' || !*p) {

            /* the line before is finished, whatever this one is */
            if (pl) { para[pl] = 0; helpwrap(para, bold, ind, w); pl = 0; }
            bold = FALSE;
            ind = 0;

        }
        if (!*p) break;
        if (!n) helpout("", FALSE, 0); /* a blank line stays blank */
        else if (*p == '#') { /* a heading inside the topic */

            const char* t = p;

            while (*t == '#') t++;
            while (*t == ' ') t++;
            bold = TRUE;
            n -= t-p;
            if (n > (ami_long)sizeof(para)-1) n = sizeof(para)-1;
            memcpy(para, t, n);
            pl = n;

        } else if (*p == '-' || *p == '*') { /* a list item */

            ind = (ami_long)strlen("00");
            if (n > (ami_long)sizeof(para)-1) n = sizeof(para)-1;
            memcpy(para, p, n);
            pl = n;

        } else { /* ordinary text, joined to the line before it */

            if (pl && pl < (ami_long)sizeof(para)-1) para[pl++] = ' ';
            if (pl+n > (ami_long)sizeof(para)-1) n = sizeof(para)-1-pl;
            memcpy(para+pl, p, n);
            pl += n;

        }
        p = *e? e+1: e;

    }

}

/* draw the topic from the line it is scrolled to */
static void helpdraw(void)

{

    ami_long chrh = 1;
    ami_long x = helpx1+(ami_long)strlen("00");
    ami_long y = helpy0;
    ami_long i;

    /* the pane, cleared a row at a time down to and including the line
       the count is written on */
    {

        ami_long r, k;

        for (r = helpy0; r <= helpy1; r++) {

            ami_cursor(helpwf, x, r);
            for (k = x; k <= ami_maxx(helpwf); k++) fputc(' ', helpwf);

        }

    }
    helppage = (helpy1-helpy0)/chrh;
    if (helppage < 1) helppage = 1;
    if (helptop > helplinect-helppage) helptop = helplinect-helppage;
    if (helptop < 0) helptop = 0;
    for (i = helptop; i < helplinect && y+chrh <= helpy1; i++) {

        if (*helplines[i].s) {

            ami_bold(helpwf, helplines[i].bold);
            ami_cursor(helpwf, x+helplines[i].ind, y);
            fprintf(helpwf, "%s", helplines[i].s);
            ami_bold(helpwf, FALSE);

        }
        y += chrh;

    }
    /* say there is more, since a pane with no bar gives no other sign */
    if (helplinect > helppage) {

        char more[80];

        if (helptop+helppage >= helplinect) strcpy(more, "-- end --");
        else sprintf(more, "-- %lld more line%s, wheel or page keys --",
                     AMI_LONG_CAST(helplinect-helptop-helppage),
                     helplinect-helptop-helppage == 1? "": "s");
        ami_fcolor(helpwf, ami_blue);
        ami_cursor(helpwf, x, helpy1);
        fprintf(helpwf, "%s", more);
        ami_fcolor(helpwf, ami_black);

    }

}

/* wrap and draw, which is what everything that changes the topic wants */
static void helptext(void)

{

    helplay1(ami_maxx(helpwf)-(ami_long)strlen("00")-
             (helpx1+(ami_long)strlen("00")));
    helpdraw();

}

/* scroll the topic by so many lines */
static void helpscroll(ami_long by)

{

    ami_long was = helptop;

    helptop += by;
    if (helptop > helplinect-helppage) helptop = helplinect-helppage;
    if (helptop < 0) helptop = 0;
    if (helptop != was) helpdraw();

}

/* place the widgets and work out the panes, on opening and on resize */
static void helplay(void)

{

    ami_long chrh = 1;
    ami_long chrw = (ami_long)strlen("0");
    ami_long lw   = chrw*30;  /* the topic list */
    ami_long bw, bh, ew, eh;

    ami_buttonsiz(helpwf, "Close", &bw, &bh);
    ami_editboxsiz(helpwf, "0", &ew, &eh);
    /* the search entry along the top of the list column */
    helpx0 = chrw*2;
    helpy0 = chrh*2+eh;
    helpx1 = helpx0+lw;
    helpy1 = ami_maxy(helpwf)-bh-chrh*2;
    if (helpy1 < helpy0+chrh) helpy1 = helpy0+chrh;
    ami_poswidget(helpwf, HELPFIND, helpx0+(ami_long)strlen("Search: "),
                   chrh);
    ami_sizwidget(helpwf, HELPFIND,
                   lw-(ami_long)strlen("Search: "), eh);
    ami_poswidget(helpwf, HELPCLOSE, ami_maxx(helpwf)-bw-chrw*2,
                   ami_maxy(helpwf)-bh-chrh/2);
    ami_sizwidget(helpwf, HELPCLOSE, bw, bh);
    /* The list is moved and sized rather than made again, so that the
       topic picked in it stays picked across a resize. Only a change
       of contents needs it made again, which is what helpfill is for. */
    if (helplistup) {

        ami_poswidget(helpwf, HELPLIST, helpx0, helpy0);
        ami_sizwidget(helpwf, HELPLIST, helpx1-helpx0, helpy1-helpy0);

    } else helpfill("");
    /* the frame, the label, and the topic */
    fprintf(helpwf, "\f");
    ami_fcolor(helpwf, ami_black);
    ami_cursor(helpwf, helpx0, chrh);
    fprintf(helpwf, "Search:");
    helptext();

}

/* close the help window, if it is open */
static void helpclose(void)

{

    ami_long i;

    if (!helpwf) return;
    ami_killwidget(helpwf, HELPFIND);
    if (helplistup) ami_killwidget(helpwf, HELPLIST);
    ami_killwidget(helpwf, HELPCLOSE);
    fclose(helpwf);
    helpwf = NULL;
    helplistup = FALSE;
    for (i = 0; i < helplinect; i++) free(helplines[i].s);
    helplinect = 0;

}

/* Open the help window. A second open just brings the one already up to
   the front, as help does everywhere. */
static void helpopen(void)

{

    ami_long wx, wy;

    if (helpwf) { ami_front(helpwf); return; }
    helpload();
    helpsel = -1;
    helptop = 0;
    ami_openwin(&stdin, &helpwf, NULL, HELPWIN);
    ami_title(helpwf, "Breakout help");
    /* Unbuffered, so the window's measurements are the window's. A
       buffered window answers maxxg with the buffer, which is not what
       the layout wants here: this window has nothing to keep. */
    ami_buffer(helpwf, FALSE);
    ami_auto(helpwf, FALSE);
    ami_curvis(helpwf, FALSE);
    ami_scnsiz(helpwf, &wx, &wy);
    wx = wx-6 < 88? wx-6: 88;
    wy = wy-2 < 28? wy-2: 28;
    ami_setsiz(helpwf, wx, wy);
    /* The entry and the button are made once here and moved by the
       layout after. The list is made by the layout, which is where its
       rectangle is known and where it is made again on every search. */
    ami_editbox(helpwf, 1, 1, 2, 2, HELPFIND);
    ami_button(helpwf, 1, 1, 2, 2, "Close", HELPCLOSE);
    helplay();

}

/* An event with the help window's id on it. The main loop hands them
   here and goes on; nothing about the mail is touched. */
static void helpevent(ami_evtrec* er)

{

    char srch[100];

    switch (er->etype) {

        case ami_etterm:   /* the window was closed, not the program */
        case ami_etbutton: helpclose(); break;

        case ami_etresize:
        case ami_etredraw: helplay(); break;

        case ami_etmouba: /* the wheel, as buttons 4 and 5 */
            if (er->amoubn == 4) helpscroll(-WHEELROWS);
            else if (er->amoubn == 5) helpscroll(WHEELROWS);
            break;

        case ami_etpagu: helpscroll(-(helppage-1)); break;
        case ami_etpagd: helpscroll(helppage-1); break;
        case ami_etup:   helpscroll(-1); break;
        case ami_etdown: helpscroll(1); break;
        case ami_ethome: helpscroll(-helplinect); break;
        case ami_etend:  helpscroll(helplinect); break;

        case ami_etlstbox: /* a topic was picked */
            if (er->lstbsl >= 1 && er->lstbsl <= helpmatches)
                helpsel = helpmatch[er->lstbsl-1];
            helptext();
            break;

        case ami_etedtbox: /* the search was entered */
            ami_getwidgettext(helpwf, HELPFIND, srch, sizeof(srch));
            helpfill(srch);
            helptext();
            break;

        default: break;

    }

}


/*******************************************************************************

About

A small window of the program's own: a few centered lines of text and an
OK button at the bottom. Like the help, it is not a dialog -- it stays up
beside the game, its events told from the game's by their window id.

*******************************************************************************/

static const char* abouttxt[] = {

    "Breakout for Petit-Ami",
    "The character windowed edition",
    "Copyright (C) 2002 S. A. Franco",
    NULL

};

static void aboutopen(void)

{

    ami_long i, w, mw;

    if (aboutwf) { ami_front(aboutwf); return; }
    ami_openwin(&stdin, &aboutwf, NULL, ABOUTWIN);
    ami_title(aboutwf, "About Breakout");
    ami_buffer(aboutwf, FALSE);
    ami_auto(aboutwf, FALSE);
    ami_curvis(aboutwf, FALSE);
    /* size to the widest line, with the button row below the text */
    mw = 0;
    for (i = 0; abouttxt[i]; i++) {

        w = strlen(abouttxt[i]);
        if (w > mw) mw = w;

    }
    ami_setsiz(aboutwf, mw+6, i+6);
    ami_button(aboutwf, ami_maxx(aboutwf)/2-3, ami_maxy(aboutwf)-2,
                        ami_maxx(aboutwf)/2+3, ami_maxy(aboutwf)-1,
                        "OK", ABOUTOK);

}

static void aboutclose(void)

{

    if (!aboutwf) return;
    ami_killwidget(aboutwf, ABOUTOK);
    fclose(aboutwf);
    aboutwf = NULL;

}

static void aboutevent(ami_evtrec* er)

{

    ami_long i, w;

    switch (er->etype) {

        case ami_etterm:   /* the window was closed, not the program */
        case ami_etbutton: aboutclose(); break;

        case ami_etredraw: /* the lines, centered */
            for (i = 0; abouttxt[i]; i++) {

                w = strlen(abouttxt[i]);
                ami_cursor(aboutwf, ami_maxx(aboutwf)/2-w/2, i+2);
                fputs(abouttxt[i], aboutwf);

            }
            break;

        default: ;

    }

}

/*******************************************************************************

Menus

The menu bar carries the game controls and the help, the way chess lays
its own out.

*******************************************************************************/

ami_menuptr newmenuitem(int onoff, int oneof, int bar, int id,
                        const char* face)

{

    ami_menuptr mp;

    mp = malloc(sizeof(ami_menurec));
    mp->next = NULL;
    mp->branch = NULL;
    mp->onoff = onoff;
    mp->oneof = oneof;
    mp->bar = bar;
    mp->id = id;
    mp->face = malloc(strlen(face)+1);
    strcpy(mp->face, face);

    return (mp);

}

void appendmenu(ami_menuptr* list, ami_menuptr item)

{

    ami_menuptr p;

    if (!*list) *list = item;
    else {

        p = *list;
        while (p->next) p = p->next;
        p->next = item;

    }

}

void makmenu(void)

{

    ami_menuptr menu = NULL;
    ami_menuptr game_menu, game_items;
    ami_menuptr help_menu, help_items;

    /* Game menu */
    game_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Game");
    appendmenu(&menu, game_menu);
    game_items = NULL;
    appendmenu(&game_items,
        newmenuitem(FALSE, FALSE, TRUE, MENU_NEW, "New Game"));
    appendmenu(&game_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_EXIT, "Exit"));
    game_menu->branch = game_items;

    /* Help menu */
    help_menu = newmenuitem(FALSE, FALSE, FALSE, 0, "Help");
    appendmenu(&menu, help_menu);
    help_items = NULL;
    appendmenu(&help_items,
        newmenuitem(FALSE, FALSE, TRUE, MENU_PLAY, "Game Play"));
    appendmenu(&help_items,
        newmenuitem(FALSE, FALSE, FALSE, MENU_ABOUT, "About Breakout"));
    help_menu->branch = help_items;

    ami_menu(stdout, menu);

}

/*******************************************************************************

Draw score

Places the score on the top wall.

********************************************************************************/

void drwscore(void)

{

    char sb[30];

    snprintf(sb, sizeof(sb), " SCORE %5d ", score);
    ami_fcolor(stdout, ami_black);
    wrtcen(1, sb);

}

/*******************************************************************************

Draw ball and paddle

Each is drawn where it now stands and erased where it last was, cell for
cell; only what moved is touched.

********************************************************************************/

void drwball(void)

{

    if (lball.x1 == ball.x1 && lball.y1 == ball.y1) return;
    if (lball.x1) filrect(&lball, ami_white); /* erase old */
    if (ball.x1) filrect(&ball, BALLCLR); /* draw new */
    lball = ball; /* note what is drawn */

}

void drwpad(void)

{

    filrect(&lpaddle, ami_white); /* erase old */
    filrect(&paddle, mcap? CAPCLR: PADCLR); /* draw new, showing capture */
    lpaddle = paddle; /* note what is drawn */

}

/*******************************************************************************

Draw screen

Draws a new game screen: walls, score, and title.

********************************************************************************/

void drwscn(void)

{

    int x;

    putchar('\f'); /* clear screen */
    /* the rule below the score, which the ball bounces at */
    ami_fcolor(stdout, ami_black);
    ami_cursor(stdout, 1, 2);
    for (x = 1; x <= ami_maxx(stdout); x++) putchar('-');
    drwscore();

}

/*******************************************************************************

Set brick wall

Initializes the bricks in the wall coordinates.

********************************************************************************/

void setwall(void)

{

    int r, c;   /* brick array indexes */
    int brkw;   /* brick width */
    int brkr;   /* brick remainder */
    int brkoff; /* brick wall offset */
    int co;     /* column offset */
    int rd;     /* remainder distributor */

    brkw = (ami_maxx(stdout)-2)/BRKCOL; /* find brick width */
    brkr = (ami_maxx(stdout)-2)%BRKCOL; /* find brick remainder */
    brkoff = ami_maxy(stdout)/4; /* find brick wall offset */
    for (r = 0; r < BRKROW; r++) {

        co = 0; /* clear column offset */
        rd = brkr; /* set remainder distributor */
        for (c = 0; c < BRKCOL; c++) {

            setrct(&bricks[r][c], 2+co, brkoff+r,
                                  2+co+brkw-1+(rd > 0), brkoff+r);
            co = co+brkw+(rd > 0); /* offset to next brick */
            if (rd > 0) rd--; /* reduce remainder */

        }

    }

}

/*******************************************************************************

Draw wall

Draws the standing bricks, each in its color with a single space between
bricks left by erasure on hits.

********************************************************************************/

void drwwall(void)

{

    int      r, c; /* brick array indexes */
    ami_color clr;  /* brick color */

    clr = ami_red; /* set 1st pure color */
    for (r = 0; r < BRKROW; r++)
        for (c = 0; c < BRKCOL; c++) {

        if (bricks[r][c].x1) filrect(&bricks[r][c], clr);
        if (clr < ami_magenta) clr++;
        else clr = ami_red;

    }

}

/*******************************************************************************

Set new paddle position

Places the paddle at the given position and redraws it there.

********************************************************************************/

void padpos(int x)

{

    if (x-hpadw <= walll.x2) x = walll.x2+hpadw+1; /* clip to ends */
    if (x+hpadw >= wallr.x1) x = wallr.x1-hpadw-1;
    padx = x; /* set new location */
    setrct(&paddle, x-hpadw, ami_maxy(stdout)-1,
                    x+hpadw, ami_maxy(stdout)-1);
    drwpad(); /* show it where it now stands */

}

/*******************************************************************************

Find brick intersection

Searches for a brick that intersects with the ball, and if found, erases the
brick and returns true. Note that if more than one brick intersects, they all
disappear.

********************************************************************************/

void interbrick(void)

{

    int r, c; /* brick array indexes */

    brki = FALSE; /* set no brick intersection */
    for (r = 0; r < BRKROW; r++)
        for (c = 0; c < BRKCOL; c++) if (intsec(&ball, &bricks[r][c])) {

        brki = TRUE; /* set intersected */
        filrect(&bricks[r][c], ami_white); /* erase from screen */
        clrrect(&bricks[r][c]); /* clear brick data */
        score++; /* count hits */
        scrchg = TRUE; /* set changed */
        fldbrk++; /* add to bricks this field */

    }

}

/*******************************************************************************

Move ball

Advances the ball one step and resolves its collisions: walls, paddle,
bricks, and the bottom. The exact position and velocity are floats, so the
ball travels at any angle, resolved to cells by rounding.

********************************************************************************/

/* set the ball cell from the exact position */

void balrect(void)

{

    setrct(&ball, round(bfx), round(bfy), round(bfx), round(bfy));

}

void movball(void)

{

    float u;   /* paddle strike offset, -1 left edge to 1 right edge */
    float spd; /* ball speed */
    float mag; /* velocity magnitude for renormalizing */

    if (ball.x1 > 0) { /* ball on screen */

        /* advance the exact position and rederive the cell */
        bfx += bvx;
        bfy += bvy;
        balrect();
        /* check off screen motions */
        if (intsec(&ball, &walll) || intsec(&ball, &wallr)) {

            /* hit left or right wall: reflect and replay the move */
            bfx -= bvx;
            bvx = -bvx;
            bfx += bvx;
            balrect();
#ifdef SOUND
            /* start bounce note */
            ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, LONG_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, WALLNOTE, LONG_MAX);
#endif

        } else if (intsec(&ball, &wallt)) { /* hits top */

            bfy -= bvy;
            bvy = -bvy;
            bfy += bvy;
            balrect();
#ifdef SOUND
            /* start bounce note */
            ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, LONG_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, WALLNOTE, LONG_MAX);
#endif

        } else if (intsec(&ball, &paddle)) {

            /* A real paddle: the strike point turns the ball. The incoming
               angle carries through as the reflected velocity, then the
               offset of the strike from paddle center adds its own turn,
               the edges throwing the sharpest angles. Speed is preserved,
               and the shot is held off horizontal so it always climbs */
            bfx -= bvx; /* stand at the strike */
            bfy -= bvy;
            balrect();
            u = (bfx-(paddle.x1+hpadw))/(float)(hpadw+1);
            if (u < -1.0) u = -1.0;
            if (u > 1.0) u = 1.0;
            spd = sqrt(bvx*bvx+bvy*bvy);
            bvy = -bvy; /* reflect vertical */
            bvx = bvx+u*spd*PADEDGE; /* the strike turns it */
            /* renormalize to the speed */
            mag = sqrt(bvx*bvx+bvy*bvy);
            bvx = bvx*spd/mag;
            bvy = bvy*spd/mag;
            /* hold the shot off horizontal */
            if (bvy > -spd*MINVERT) {

                bvy = -spd*MINVERT;
                mag = sqrt(spd*spd-bvy*bvy);
                bvx = bvx < 0.0? -mag: mag;

            }
            bfx += bvx; /* replay the move on the new heading */
            bfy += bvy;
            balrect();
            /* if the ball is still below the paddle plane, move
               it up until it is not */
            if (ball.y2 >= paddle.y1) {

                bfy -= ball.y2-paddle.y1+1;
                balrect();

            }
#ifdef SOUND
            /* start bounce note */
            ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, LONG_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, WALLNOTE, LONG_MAX);
#endif

        } else { /* check brick hits */

            interbrick(); /* check brick intersection */
            if (brki) { /* there was a brick hit */

                bfy -= bvy; /* reflect and replay */
                bvy = -bvy;
                bfy += bvy;
                balrect();
#ifdef SOUND
                /* start bounce note */
                ami_noteon(AMI_SYNTH_OUT, 0, 1, BRICKNOTE, LONG_MAX);
                ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, BRICKNOTE, LONG_MAX);
#endif

            }

        };
        if (intsec(&ball, &wallb)) { /* ball out of bounds */

            clrrect(&ball); /* set ball not on screen */
            /* start time on new ball wait */
            baltim = NEWBAL;
#ifdef SOUND
            /* start fail note */
            ami_noteon(AMI_SYNTH_OUT, 0, 1, FAILNOTE, LONG_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+FAILTIME, 1, FAILNOTE, LONG_MAX);
#endif

        }
        drwball(); /* show it where it now stands */

    }

}

int main(int argc, char* argv[])

{

    progpath = argc > 0? argv[0]: NULL; /* the help file lives beside us */
    ami_autohold(FALSE); /* a game leaves nothing to read: exit means exit */
    ami_title(stdout, "Breakout"); /* the window carries the name */
    makmenu(); /* the menu bar; the field sizes below account for it */
#ifdef SOUND
    ami_opensynthout(AMI_SYNTH_OUT); /* open synthesizer */
    ami_instchange(AMI_SYNTH_OUT, 0, 1, AMI_INST_LEAD_1_SQUARE);
    ami_starttimeout(); /* start sequencer running */
#endif
    jchr = LONG_MAX/((ami_maxx(stdout)-2)/2); /* find basic joystick increment */
    ami_curvis(stdout, FALSE); /* remove drawing cursor */
    ami_auto(stdout, FALSE); /* turn off scrolling */
    padw = ami_maxx(stdout)/8; /* set paddle width */
    hpadw = padw/2; /* half paddle width */
    padstp = ami_maxx(stdout)/50; /* paddle key step scales with the field */
    if (padstp < 1) padstp = 1;
    ami_timer(stdout, 1, BALMOV, TRUE); /* the ball physics timer */
    dwltim = CAPDWELL; /* arm the capture dwell */

    newgame: /* start new game */

    /* Set up the boundaries first; the paddle placement clips to them.
       The window frames the game: the ball bounces at the rule below the
       score, the side edges, and is lost past the bottom line */
    setrct(&wallt, 1, 1, ami_maxx(stdout), 2); /* top, the score strip */
    setrct(&walll, 1, 1, 1, ami_maxy(stdout)); /* left */
    /* right */
    setrct(&wallr, ami_maxx(stdout), 1, ami_maxx(stdout), ami_maxy(stdout));
    /* bottom */
    setrct(&wallb, 1, ami_maxy(stdout), ami_maxx(stdout), ami_maxy(stdout));
    score = 0; /* clear score */
    scrchg = FALSE;
    clrrect(&ball); /* set ball not on screen */
    clrrect(&lball); /* nothing drawn */
    drwscn(); /* draw game screen */
    padx = ami_maxx(stdout)/2; /* find initial paddle position */
    setrct(&lpaddle, padx, ami_maxy(stdout)-1, padx, ami_maxy(stdout)-1);
    padpos(padx); /* place paddle */
    baltim = NEWBAL; /* set starting ball time */
    do { /* game loop */

        setwall(); /* initialize bricks */
        drwwall(); /* draw the wall */
        fldbrk = 0; /* clear bricks hit this field */
        do { /* fields */

            if (ball.x1 == 0 && baltim == 0) {

                /* ball not on screen, and time to wait expired, send out ball */
                bfx = 3; /* launch position */
                bfy = ami_maxy(stdout)-4;
                bvx = ami_maxx(stdout)/300.0; /* set direction of travel */
                bvy = -(ami_maxy(stdout)/150.0);
                balrect();
                drwball();

            }
            if (scrchg) { /* the score updates in place */

                drwscore();
                scrchg = FALSE;

            }
            do { /* wait relevant events; other windows take their own */

                int handled = FALSE;

                ami_event(stdin, &er);
                if (helpwf && er.winid == HELPWIN) {

                    helpevent(&er);
                    handled = TRUE;

                }
                if (aboutwf && er.winid == ABOUTWIN) {

                    aboutevent(&er);
                    handled = TRUE;

                }
                if (!handled &&
                    (er.etype == ami_etterm || er.etype == ami_etleft ||
                     er.etype == ami_etright || er.etype == ami_etfun ||
                     er.etype == ami_ettim || er.etype == ami_etjoymov ||
                     er.etype == ami_etmoumov || er.etype == ami_etmouba ||
                     er.etype == ami_etmoubd || er.etype == ami_etmenus)) break;

            } while (TRUE);
            if (er.etype == ami_etterm) goto endgame; /* game exits */
            if (er.etype == ami_etfun) goto newgame; /* restart game */
            if (er.etype == ami_etmenus) { /* menu selections */

                switch (er.menuid) {

                    case MENU_NEW:   goto newgame; /* start over */
                    case MENU_EXIT:  goto endgame; /* leave */
                    case MENU_PLAY:  helpopen(); break; /* the help window */
                    case MENU_ABOUT: aboutopen(); break;

                }

            }
            /* process paddle movements */
            if (er.etype == ami_etleft) { /* move left */

                mcap = FALSE; /* the keys take the paddle back */
                dwltim = CAPDWELL; /* and the capture dwell rearms */
                padpos(padx-padstp);

            } else if (er.etype == ami_etright) { /* move right */

                mcap = FALSE;
                dwltim = CAPDWELL;
                padpos(padx+padstp);

            } else if (er.etype == ami_etmoumov) { /* mouse moves */

                lstmx = er.moupx; /* track position */
                lstmy = er.moupy;
                if (mcap) padpos(lstmx); /* the paddle rides the mouse */

            } else if (er.etype == ami_etmouba && er.amoubn == 1)
                btndn = TRUE; /* the held press drives the dwell clocks */
            else if (er.etype == ami_etmoubd && er.dmoubn == 1)
                btndn = FALSE;
            else if (er.etype == ami_etjoymov) { /* move joystick */

                /* The paddle follows the joystick only when the stick
                   itself moves: an idle stick reports steadily, and
                   letting it hold the paddle would override the keys */
                if (!joyini) { joyini = TRUE; joylst = er.joypx; }
                else if (er.joypx != joylst) {

                    joylst = er.joypx;
                    mcap = FALSE; /* the stick takes the paddle back */
                    dwltim = CAPDWELL;
                    padpos(ami_maxx(stdout)/2+er.joypx/jchr);

                }

            }
            /* the physics beat: step the ball and the waits */
            if (er.etype == ami_ettim && er.timnum == 1) {

                movball();
                /* if the ball timer is running, decrement it */
                if (baltim > 0) baltim--;
                if (!mcap) {

                    /* the capture dwell: the button held down on the
                       paddle for the dwell time takes it; the paddle
                       then follows the mouse with the button up */
                    if (btndn &&
                        lstmx >= paddle.x1 && lstmx <= paddle.x2 &&
                        lstmy == paddle.y1) {

                        if (dwltim > 0) dwltim--;
                        if (!dwltim) {

                            mcap = TRUE; /* the mouse takes the paddle */
                            relarm = FALSE; /* this press spends itself */
                            reltim = RELDWELL;
                            drwpad(); /* show the capture */

                        }

                    } else dwltim = CAPDWELL; /* released or off rearms */

                } else {

                    /* the release dwell: a fresh press held for the
                       dwell time lets the paddle go. The press that
                       captured must lift first, or one long hold would
                       capture and release in a single stroke */
                    if (!btndn) { relarm = TRUE; reltim = RELDWELL; }
                    else if (relarm) {

                        if (reltim > 0) reltim--;
                        if (!reltim) {

                            mcap = FALSE;
                            dwltim = CAPDWELL;
                            drwpad(); /* show the release */

                        }

                    }

                }

            }

        } while (fldbrk != BRKROW*BRKCOL); /* until bricks are cleared */
#ifdef SOUND
        ami_noteon(AMI_SYNTH_OUT,  0,                  1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*2,  1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*3,  1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*4,  1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*5,  1, AMI_NOTE_E+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*6,  1, AMI_NOTE_E+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*7,  1, AMI_NOTE_F+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*8,  1, AMI_NOTE_F+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*9,  1, AMI_NOTE_E+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*10, 1, AMI_NOTE_E+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*11, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*13, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
#endif
        baltim = FANWAIT; /* wait fanfare */
        if (lball.x1) filrect(&lball, ami_white); /* clear ball */
        clrrect(&ball); /* set ball not on screen */
        clrrect(&lball);

    } while (TRUE); /* forever */

    endgame:; /* exit game */

    ami_curvis(stdout, TRUE);
#ifdef SOUND
    ami_closesynthout(AMI_SYNTH_OUT); /* close synthesizer */
#endif

}
