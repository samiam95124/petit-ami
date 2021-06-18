/*******************************************************************************
*                                                                             *
*                           GRAPHICS TEST PROGRAM                             *
*                                                                             *
*                    Copyright (C) 2005 Scott A. Franco                       *
*                                                                             *
* Tests various single window, unmanaged graphics.                            *
*                                                                             *
* Benchmark results, Athlon 64 3200+, BFG 6800 overclock:                     *
*                                                                             *
* Type                        Seconds     Per fig                             *
* --------------------------------------------------                          *
* line width 1                     7.484    7.484e-5                          *
* line width 10                   10.906   .00010906                          *
* rectangle width 1                7.313    7.313e-5                          *
* rectangle width 10               8.219    8.219e-5                          *
* rounded rectangle width 1       12.781   .00012781                          *
* rounded rectangle width 10      15.953   .00015953                          *
* filled rectangle                15.516   1.5516e-5                          *
* filled rounded rectangle         8.906    8.906e-5                          *
* ellipse width 1                 17.437   .00017437                          *
* ellipse width 10                22.078   .00022078                          *
* filled ellipse                  13.297   .00013297                          *
* arc width 1                      9.719    9.719e-5                          *
* arc width 10                    12.125   .00012125                          *
* filled arc                      10.422   .00010422                          *
* filled chord                      8.89     8.89e-5                          *
* filled triangle                 19.172   1.9172e-5                          *
* text                            10.922   .00010922                          *
* background invisible text       10.703   .00010703                          *
*                                                                             *
* Benchmark results, AMD Phenom, 2.51 GHZ, Nvidia GeForce 9800 GT             *
*                                                                             *
* Type                     Seconds   Per fig                                  *
* --------------------------------------------------                          *
* line width 1                 6.71    0.000067                               *
* line width 10                7.37    0.000073                               *
* rectangle width 1           10.45    0.000104                               *
* rectangle width 10          11.98    0.000119                               *
* rounded rectangle width 1   13.35    0.000133                               *
* rounded rectangle width 10  16.00    0.000160                               *
* filled rectangle             8.20    0.000082                               *
* filled rounded rectangle    20.98    0.000209                               *
* ellipse width 1             14.96    0.000149                               *
* ellipse width 10            17.62    0.000176                               *
* filled ellipse              22.76    0.000227                               *
* arc width 1                 11.03    0.000110                               *
* arc width 10                12.78    0.000127                               *
* filled arc                  16.15    0.000161                               *
* filled chord                14.53    0.000145                               *
* filled triangle             24.14    0.000241                               *
* text                        10.89    0.000435                               *
* background invisible text   10.95    0.000438                               *
* Picture draw                23.82    0.002382                               *
* No scaling picture draw     13.82    0.001382                               *
*                                                                             *
*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <services.h>
#include <graphics.h>

#define S1     "Moving string"
#define S2     "Variable size string"
#define S3     "Sizing test string"
#define S4     "Justify test string"
#define S5     "Invisible body text"
#define S6     "Example text"
#define COLDIV 6 /* number of color divisions */
#define COLSQR 20 /* size of color square */
#define OFF    FALSE
#define ON     TRUE
#define DEGREE (INT_MAX/360)

typedef enum {

    bnline1,     /* line width 1 */
    bnline10,    /* line width 10 */
    bnrect1,     /* rectangle width 1 */
    bnrect10,    /* rectangle width 10 */
    bnrrect1,    /* rounded rectangle width 1 */
    bnrrect10,   /* rounded rectangle width 10 */
    bnfrect,     /* filled rectangle */
    bnfrrect,    /* filled rounded rectangle */
    bnellipse1,  /* ellipse width 1 */
    bnellipse10, /* ellipse width 10 */
    bnfellipse,  /* filled ellipse */
    bnarc1,      /* arc width 1 */
    bnarc10,     /* arc width 10 */
    bnfarc,      /* filled arc */
    bnfchord,    /* filled chord */
    bnftriangle, /* filled triangle */
    bntext,      /* text */
    bntextbi,    /* background invisible text */
    bnpict,      /* picture draw */
    bnpictns     /* no scale picture draw */

} bench;

static jmp_buf   terminate_buf;
static char      fns[100];
static int       x, y;
static int       xs, ys;
static int       i;
static int       dx, dy;
static int       ln;
static int       term;
static int       w;
static int       l;
static int       a;
static int       r, g, b;
static pa_color  c, c1, c2;
static int       lx, ly;
static int       tx1, ty1, tx2, ty2, tx3, ty3;
static int       h;
static int       cnt;
static pa_evtrec er;
static int       fsiz;
static int       aa, ab;
static int       s;
static struct { /* benchmark stats records */

    int iter; /* number of iterations performed */
    int time; /* time in 100us for test */

} benchtab[bnpictns+1];
static bench bi;

/* Find random number between 0 and N. */

static int randn(int limit)

{

    return (long)limit*rand()/RAND_MAX;

}

/* Find random number in given range */

static int randr(int s, int e)

{

    return (randn(e-s)+s);

}

/* wait time in 100 microseconds */

static void wait(long t)

{

    pa_evtrec er;

    pa_timer(stdout, 1, t, FALSE);
    do { pa_event(stdin, &er); }
    while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* wait time in 100 microseconds, with space terminate */

static void waitchar(long t, int* st)

{

    pa_evtrec er;

    *st = FALSE; /* set no space terminate */
    pa_timer(stdout, 1, t, FALSE);
    do { pa_event(stdin, &er); }
    while (er.etype != pa_ettim && er.etype != pa_etterm &&
           er.etype != pa_etchar && er.etype != pa_etenter);
   if (er.etype == pa_etchar && er.echar == ' ') *st = TRUE;
   if (er.etype == pa_etenter) *st = TRUE;
   if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* wait return to be pressed, or handle terminate */

static void waitnext(void)

{

    pa_evtrec er; /* event record */

    do { pa_event(stdin, &er); }
    while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* print centered string */

static void prtcen(int y, const char* s)

{

   pa_cursor(stdout, (pa_maxx(stdout)/2)-(strlen(s)/2), y);
   printf("%s", s);

}

/* print centered string graphical */

static void prtceng(int y, const char* s)

{

   pa_cursorg(stdout, (pa_maxxg(stdout)/2)-(pa_strsiz(stdout, s)/2), y);
   printf("%s", s);

}

/* print all printable characters */

static void prtall(void)

{

    char c;
    char s[2];

    s[1] = 0;
    for (c = ' '; c <= '}'; c++) {

        s[0] = 'c';
        if (pa_curxg(stdout)+pa_strsiz(stdout, s) >
            pa_maxxg(stdout)) pa_cursorg(stdout, 1, pa_curyg(stdout)+pa_chrsizy(stdout));
        putchar(c);

    }
    putchar('\n');

}

/* draw a character grid */

static void chrgrid(void)

{

    int x, y;

    pa_fcolor(stdout, pa_yellow);
    y = 1;
    while (y < pa_maxyg(stdout)) {

        pa_line(stdout, 1, y, pa_maxxg(stdout), y);
        y = y+pa_chrsizy(stdout);

    }
    x = 1;
    while (x < pa_maxxg(stdout)) {

        pa_line(stdout, x, 1, x, pa_maxyg(stdout));
        x = x+pa_chrsizx(stdout);

    }
    pa_fcolor(stdout, pa_black);

}

/* find rectangular coordinates from polar, relative to center of circle,
  with given diameter */

static void rectcord(int  a, /* angle, 0-359 */
              int  r, /* radius of circle */
              int* x,
              int* y  /* returns rectangular coordinate */
             )

{

    float angle; /* angle in radian measure */

    angle = a*0.01745329; /* find radian measure */
    *x = round(sin(angle)*r); /* find distance x */
    *y = round(cos(angle)*r); /* find distance y */

}

/* draw polar coordinate line */

static void pline(int a,  /* angle of line */
                  int o,  /* length of line */
                  int cx, /* center of circle in x and y */
                  int cy,
                  int w   /* width of line */
                 )

{

    int ex, ey; /* line start and end */

    rectcord(a, o, &ex, &ey); /* find endpoint of line */
    pa_linewidth(stdout, w); /* set width */
    pa_line(stdout, cx, cy, cx+ex, cy-ey); /* draw line */

}

/* draw centered justified text */

static void justcenter(const char* s, int l)

{

    int i, x;

    x = pa_maxxg(stdout)/2-l/2;
    pa_cursorg(stdout, x, pa_curyg(stdout));
    pa_writejust(stdout, s, l);
    putchar('\n');
    pa_fcolor(stdout, pa_white);
    pa_frect(stdout, x, pa_curyg(stdout), x+l-1, pa_curyg(stdout)+pa_chrsizy(stdout)-1);
    pa_fcolor(stdout, pa_black);
    pa_rect(stdout, x, pa_curyg(stdout), x+l-1, pa_curyg(stdout)+pa_chrsizy(stdout)-1);
    for (i = 0; i < strlen(s); i++)
      pa_line(stdout, x+pa_justpos(stdout, s, i, l), pa_curyg(stdout),
              x+pa_justpos(stdout, s, i, l), pa_curyg(stdout)+pa_chrsizy(stdout)-1);
    putchar('\n');

}

/* draw graphics grid */

static void grid(void)

{

    int   x, y;
    int   xspace;
    int   yspace;
    float yrat;

    yrat = pa_dpmy(stdout)/pa_dpmx(stdout); /* find aspect ratio */
    xspace = pa_maxxg(stdout)/60;
    yspace = xspace*yrat;
    pa_linewidth(stdout, 1);
    pa_fcolor(stdout, pa_cyan);
    x = 10;
    while (x <= pa_maxxg(stdout)) {

        pa_line(stdout, x, 1, x, pa_maxyg(stdout));
        x = x+xspace;

    }
    y = 10;
    while (y <= pa_maxyg(stdout)) {

        pa_line(stdout, 1, y, pa_maxxg(stdout), y);
        y = y+yspace;

    }
    pa_fcolor(stdout, pa_black);

}

/* This is the square2 program */

#define SQUARESIZE (81)
#define HALFSQUARE (SQUARESIZE/2)
#define MAXSQUARE  (10)
#define REPRATE    (1) /* number of moves per frame, should be low */

typedef struct { /* square data record */

    int      x, y;   /* current position */
    int      lx, ly; /* last position */
    int      xd, yd; /* deltas */
    pa_color c;      /* color */

} balrec;

static balrec baltbl[MAXSQUARE];

static int chkbrk(void)

{

    pa_evtrec er; /* event record */
    int done;

    done = FALSE;
    do { pa_event(stdin, &er); }
    while (er.etype != pa_etframe && er.etype != pa_etterm &&
           er.etype != pa_etchar && er.etype != pa_etenter);
fprintf(stderr, "er.etype: %d\n", er.etype); fflush(stderr);
    if (er.etype == pa_etterm) { longjmp(terminate_buf, 1); }
    if (er.etype == pa_etchar || er.etype == pa_etenter)
        done = TRUE; /* terminate */

    return (done);

}

static void drawsquare(pa_color c, int x, int y)

{

    pa_fcolor(stdout, c); /* set color */
    pa_frect(stdout, x-HALFSQUARE+1, y-HALFSQUARE+1, x+HALFSQUARE-1, y+HALFSQUARE-1);

}

static void movesquare(int s)

{

    balrec* bt;
    int nx, ny; /* temp coordinates holders */

    bt = &baltbl[s];
    nx = bt->x+bt->xd; /* trial move square */
    ny = bt->y+bt->yd;
    /* check out of bounds and reverse direction */
    if (nx < HALFSQUARE || nx > pa_maxxg(stdout)-HALFSQUARE+1) bt->xd = -bt->xd;
    if (ny < HALFSQUARE || ny > pa_maxyg(stdout)-HALFSQUARE+1) bt->yd = -bt->yd;
    bt->x = bt->x+bt->xd; /* move square */
    bt->y = bt->y+bt->yd;

}

static void squares(void)

{

    int     cd; /* current display flip select */
    int     i; /* index for table */
    int     rc; /* repetition counter */
    int     done; /* done flag */
    balrec* bt;

    /* initalize square data */
    for (i = 0; i < MAXSQUARE; i++) {

        bt = &baltbl[i];
        bt->x = randn(pa_maxxg(stdout)-SQUARESIZE)+HALFSQUARE;
        bt->y = randn(pa_maxyg(stdout)-SQUARESIZE)+HALFSQUARE;
        if (!randn(TRUE)) bt->xd = +1; else bt->xd = -1;
        if (!randn(TRUE)) bt->yd = +1; else bt->yd = -1;
        bt->lx = bt->x; /* set last position to same */
        bt->ly = bt->y;
        bt->c = randr(pa_red, pa_magenta); /* set random color */

    }
    pa_curvis(stdout, FALSE); /* turn off cursor */
    cd = FALSE; /* set 1st display */
    /* place squares on display */
    for (i = 0; i < MAXSQUARE; i++)
        drawsquare(baltbl[i].c, baltbl[i].x, baltbl[i].y);
    pa_frametimer(stdout, TRUE); /* start frame timer */
    done = FALSE; /* set ! done */
    while (!done) {

        /* select display and update surfaces */
        pa_select(stdout, !cd+1, cd+1);
        putchar('\f');
        pa_fover(stdout);
        pa_fcolor(stdout, pa_black);
        prtcen(pa_maxy(stdout), "Animation test");
        pa_fxor(stdout);
        /* save old positions */
        for (i = 0; i < MAXSQUARE; i++) {

            baltbl[i].lx = baltbl[i].x; /* save last position */
            baltbl[i].ly = baltbl[i].y;

        }
        /* move squares */
        for (rc = 1; rc <= REPRATE; rc++) /* repeats per frame */
            for (i = 0; i < MAXSQUARE; i++) movesquare(i); /* process squares */
        /* draw squares */
        for (i = 0; i < MAXSQUARE; i++)
            drawsquare(baltbl[i].c, baltbl[i].x, baltbl[i].y);
        cd = !cd; /* flip display and update surfaces */
        //done = chkbrk(); /* check complete */
waitnext();
fprintf(stderr, "after chkbrk\n"); fflush(stderr);

    }
    pa_select(stdout, 1, 1); /* restore buffer surfaces */

}

/* draw standard graphical test, which is all the figures possible
  arranged on the screen */

static void graphtest(int lw) /* line width */

{

    int fsiz;
    int x, y;
    int xsize;   /* total space x for figures */
    int xspace;  /* x space between figures */
    int yspace;  /* y space between figures */
    int xfigsiz; /* size of figure x */
    int yfigsiz; /* size of figure y */
    float yrat;  /* aspect ratio of y to x */

    pa_auto(stdout, OFF);
    pa_font(stdout, PA_FONT_SIGN);
    fsiz = pa_chrsizy(stdout); /* save character size to restore */
    pa_fontsiz(stdout, pa_maxyg(stdout)/20);
    pa_bcolor(stdout, pa_yellow);
    pa_cursorg(stdout, pa_maxxg(stdout)/2-pa_strsiz(stdout, S6)/2, pa_curyg(stdout));
    printf("%s\n", S6);
    putchar('\n');
    /* note: after this we can't use text spacing */

    /* we assume x > y */
    yrat = pa_dpmy(stdout)/pa_dpmx(stdout); /* find aspect ratio */
    xsize = pa_maxxg(stdout)/5; /* set spacing of figures */
    xspace = xsize/5; /* set x space between figures */
    xfigsiz = xsize-xspace; /* net y size of figure */
    yfigsiz = xfigsiz*yrat; /* net y size of figure */
    yspace = xspace*yrat; /* set y space between figures */

    /* first row of figures */
    pa_fcolor(stdout, pa_magenta);
    pa_linewidth(stdout, lw);
    y = pa_curyg(stdout); /* set y location to current */
    x = xspace/2; /* set x location to after space left */
    pa_rect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    pa_fcolor(stdout, pa_green);
    x = x+xfigsiz+xspace;
    pa_frect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    pa_fcolor(stdout, pa_yellow);
    x = x+xfigsiz+xspace;
    pa_ftriangle(stdout, x, y+yfigsiz-1, x+xfigsiz/2-1, y,
                         x+xfigsiz-1, y+yfigsiz-1);
    pa_fcolor(stdout, pa_red);
    x = x+xfigsiz+xspace;
    pa_rrect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1, 20, 20);
    pa_fcolor(stdout, pa_magenta);
    x = x+xfigsiz+xspace;
    pa_arc(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1, 0, INT_MAX/4);
    pa_fcolor(stdout, pa_green);
    pa_farc(stdout, x, y, x+xfigsiz-1, y+xfigsiz-1,
                          INT_MAX/2, INT_MAX/2+INT_MAX/4);
    y = y+yfigsiz+yspace;
    x = xspace/2;

    /* second row of figures */
    pa_fcolor(stdout, pa_blue);
    pa_frect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    x = x+xfigsiz+xspace;
    pa_fcolor(stdout, pa_magenta);
    pa_frrect(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1, 20, 20);
    x = x+xfigsiz+xspace;
    pa_fcolor(stdout, pa_green);
    pa_ellipse(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    x = x+xfigsiz+xspace;
    pa_fcolor(stdout, pa_yellow);
    pa_fellipse(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1);
    x = x+xfigsiz+xspace;
    pa_fcolor(stdout, pa_blue);
    pa_fchord(stdout, x, y, x+xfigsiz-1, y+yfigsiz-1, 0, INT_MAX/2);
    y = y+xfigsiz+xspace;

    /* third row of figures (lines) */
    pa_fcolor(stdout, pa_red);
    pa_linewidth(stdout, 1);
    pa_line(stdout, 20, y, pa_maxxg(stdout)-20, y);
    y = y+10;
    pa_fcolor(stdout, pa_green);
    pa_linewidth(stdout, 3);
    pa_line(stdout, 20, y, pa_maxxg(stdout)-20, y);
    y = y+10;
    pa_fcolor(stdout, pa_blue);
    pa_linewidth(stdout, 7);
    pa_line(stdout, 20, y, pa_maxxg(stdout)-20, y);
    y = y+20;
    pa_fcolor(stdout, pa_magenta);
    pa_linewidth(stdout, 15);
    pa_line(stdout, 20, y, pa_maxxg(stdout)-20, y);
    pa_linewidth(stdout, 1);
    pa_fontsiz(stdout, fsiz); /* restore font size */
    pa_fcolor(stdout, pa_black);
    pa_bcolor(stdout, pa_white);
    pa_font(stdout, PA_FONT_TERM);

}

/* test line speed */

static void linespeed(int w, int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_linewidth(stdout, w);
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_line(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                        randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test rectangle speed */

static void rectspeed(int w, int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_linewidth(stdout, w);
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_rect(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                        randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test rounded rectangle speed */

static void rrectspeed(int w, int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_linewidth(stdout, w);
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_rrect(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                         randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                         randn(100-1)+1, randn(100-1)+1);

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test filled rectangle speed */

static void frectspeed(int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_frect(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                         randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test filled rounded rectangle speed */

static void frrectspeed(int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_frrect(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                          randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                          randn(100-1)+1, randn(100-1)+1);

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test ellipse speed */

static void ellipsespeed(int w, int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_linewidth(stdout, w);
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_ellipse(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                           randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test filled ellipse speed */

static void fellipsespeed(int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_fellipse(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                            randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test arc speed */

static void arcspeed(int w, int t, int* s)

{

    int i;
    int c;
    int sa, ea;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_linewidth(stdout, w);
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        do {

            sa = randn(INT_MAX);
            ea = randn(INT_MAX);

        } while (ea <= sa);
        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_arc(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                       randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                       sa, ea);

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test filled arc speed */

static void farcspeed(int t, int* s)

{

    int i;
    int c;
    int sa, ea;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        do {

            sa = randn(INT_MAX);
            ea = randn(INT_MAX);

        } while (ea <= sa);
        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_farc(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                        randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                        sa, ea);

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test filled chord speed */

static void fchordspeed(int t, int* s)

{

    int i;
    int c;
    int sa, ea;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        do {

            sa = randn(INT_MAX);
            ea = randn(INT_MAX);

        } while (ea <= sa);
        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_fchord(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                          randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                          sa, ea);

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test filled triangle speed */

static void ftrianglespeed(int t,  int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_ftriangle(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                             randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                             randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test text speed */

static void ftextspeed(int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_bcolor(stdout, randr(pa_red, pa_magenta));
        pa_cursorg(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));
        printf("Test text");

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);
    pa_bcolor(stdout, pa_white);

}

/* test picture draw speed */

static void fpictspeed(int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_loadpict(stdout, 1, "tests/mypic");
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_picture(stdout, 1, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                              randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

   }
   *s = pa_elapsed(c);
   pa_fcolor(stdout, pa_black);

}

/* test picture draw speed, no scaling */

static void fpictnsspeed(int t, int* s)

{

    int i;
    int c;
    int x, y;
    int xs, ys;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_loadpict(stdout, 1, "tests/mypic");
    xs = pa_pictsizx(stdout, 1);
    ys = pa_pictsizy(stdout, 1);
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        x = randr(1, pa_maxxg(stdout));
        y = randr(1, pa_maxyg(stdout));
        pa_picture(stdout, 1, x, y, x+xs-1, y+ys-1);

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

int main(void)

{

    int yspace;
    int xspace;
    int xsize;
    int ysize;
    int x1, y1, x2, y2;
    char fn[100];
    int x, y;

    if (setjmp(terminate_buf)) goto terminate;
    pa_curvis(stdout, FALSE);
    pa_binvis(stdout);
#if 0
    printf("Graphics screen test vs. 0.1\n");
    printf("\n");
    printf("Screen size in characters: x -> %d y -> %d\n", pa_maxx(stdout),
                                                           pa_maxy(stdout));
    printf("            in pixels:     x -> %d y -> %d\n",
           pa_maxxg(stdout), pa_maxyg(stdout));
    printf("Size of character in default font: x -> %d y -> %d\n",
           pa_chrsizx(stdout), pa_chrsizy(stdout));
    printf("Dots per meter: dpmx: %d dpmy: %d\n", pa_dpmx(stdout), pa_dpmy(stdout));
    printf("Aspect ratio: %f\n", (double)pa_dpmx(stdout)/pa_dpmy(stdout));
    prtcen(pa_maxy(stdout),
           "Press return to start test (and to pass each pattern)");
    waitnext();

    /* ************************ Graphical figures test ************************* */

    putchar('\f');

    grid();
    printf("\n");
    pa_bover(stdout);
    graphtest(1);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Graphical figures test, linewidth == 1");
    waitnext();

    putchar('\f');
    grid();
    printf("\n");
    graphtest(2);
    prtcen(pa_maxy(stdout), "Graphical figures test, linewidth == 2");
    waitnext();

    putchar('\f');
    grid();
    printf("\n");
    graphtest(3);
    prtcen(pa_maxy(stdout), "Graphical figures test, linewidth == 3");
    waitnext();

    putchar('\f');
    grid();
    printf("\n");
    graphtest(5);
    prtcen(pa_maxy(stdout), "Graphical figures test, linewidth == 5");
    waitnext();

    putchar('\f');
    grid();
    printf("\n");
    graphtest(11);
    prtcen(pa_maxy(stdout), "Graphical figures test, linewidth == 11");
    waitnext();

   /* ***************************** Standard Fonts test *********************** */

    putchar('\f');
    chrgrid();
    prtcen(pa_maxy(stdout), "Standard fonts test");
    pa_auto(stdout, FALSE);
    pa_home(stdout);
    pa_binvis(stdout);
    pa_fontnam(stdout, PA_FONT_TERM, fns, 100);
    if (strlen(fns) > 0) {

        pa_font(stdout, PA_FONT_TERM);
        printf("This is the terminal font: System name: \"%s\"\n", fns);
        printf("Size x -> %d y -> %d\n", pa_chrsizx(stdout), pa_chrsizy(stdout));
        prtall();
        printf("\n");

    } else {

        printf("There is no terminal font\n");
        printf("\n");

    }
    pa_fontnam(stdout, PA_FONT_BOOK, fns, 100);
    if (strlen(fns) > 0) {

        pa_font(stdout, PA_FONT_BOOK);
        printf("This is the book font: System name: \"%s\"\n", fns);
        printf("Size x -> %d y -> %d\n", pa_chrsizx(stdout), pa_chrsizy(stdout));
        prtall();
        printf("\n");

    } else {

        printf("There is no book font\n");
        printf("\n");

    }
    pa_fontnam(stdout, PA_FONT_SIGN, fns, 100);
    if (strlen(fns) > 0) {

        pa_font(stdout, PA_FONT_SIGN);
        printf("This is the sign font: System name: \"%s\"\n", fns);
        printf("Size x -> %d y -> %d\n", pa_chrsizx(stdout), pa_chrsizy(stdout));
        prtall();
        printf("\n");

    } else {

        printf("There is no sign font\n");
        printf("\n");

    }
    pa_fontnam(stdout, PA_FONT_TECH, fns, 100);
    if (strlen(fns) > 0) {

        pa_font(stdout, PA_FONT_TECH);
        printf("This is the technical font: System name: \"%s\"\n", fns);
        printf("Size x -> %d y -> %d\n", pa_chrsizx(stdout), pa_chrsizy(stdout));
        prtall();
        printf("\n");

    } else {

        printf("There is no technical font\n");
        printf("\n");

    }
    pa_font(stdout, PA_FONT_TERM);
    printf("Complete\n");
    waitnext();

   /* ********************** Graphical cursor movement test ******************* */

    putchar('\f');
    pa_auto(stdout, FALSE);
    prtcen(pa_maxy(stdout), "Graphical cursor movement test");
    x = 1;
    y = 1;
    i = 10000;
    dx = +1;
    dy = +1;
    ln = pa_strsiz(stdout, S1);
    term = FALSE;
    while (!term) {

        pa_cursorg(stdout, x, y);
        printf("%s", S1);
        xs = x;
        ys = y;
        x = x+dx;
        y = y+dy;
        if (x < 1 || x+ln-1 > pa_maxxg(stdout)) {

            x = xs;
            dx = -dx;

        }
        if (y < 1 || y+pa_chrsizy(stdout)*2 > pa_maxyg(stdout)) {

            y = ys;
            dy = -dy;

        }
        waitchar(100, &term);
        pa_cursorg(stdout, xs, ys);
        pa_fcolor(stdout, pa_white);
        printf("%s", S1);
        pa_fcolor(stdout, pa_black);

    }

    /* *************************** Vertical lines test ************************* */

    putchar('\f');
    grid();
    prtcen(pa_maxy(stdout), "Vertical lines test");
    yspace = pa_maxyg(stdout)/20;
    xspace = pa_maxxg(stdout)/50;
    y = yspace;
    w = 1;
    while (y+w/2 < pa_maxyg(stdout)-pa_chrsizy(stdout)) {

        pa_linewidth(stdout, w);
        pa_line(stdout, xspace, y, pa_maxxg(stdout)-xspace, y);
        y = y+yspace;
        w = w+1;

    }
    pa_linewidth(stdout, 1);
    waitnext();

    /* ************************* Horizontal lines test ************************* */

    putchar('\f');
    grid();
    prtcen(pa_maxy(stdout), "Horizontal lines test");
    yspace = pa_maxyg(stdout)/20;
    xspace = pa_maxxg(stdout)/20;
    x = xspace;
    w = 1;
    while (x+w/2 < pa_maxxg(stdout)-20) {

        pa_linewidth(stdout, w);
        pa_line(stdout, x, yspace, x, pa_maxyg(stdout)-pa_chrsizy(stdout));
        x = x+xspace;
        w = w+1;

    }
    pa_linewidth(stdout, 1);
    waitnext();

    /* **************************** Polar lines test *************************** */

    putchar('\f');
    grid();
    prtcen(pa_maxy(stdout), "Polar lines test");
    x = pa_maxxg(stdout)/2;
    y = pa_maxyg(stdout)/2;
    if (pa_maxxg(stdout) > pa_maxyg(stdout)) l = pa_maxyg(stdout)/2-pa_chrsizy(stdout);
    else l = pa_maxxg(stdout)/2-pa_chrsizy(stdout);
    w = 1;
    pa_fcolor(stdout, pa_blue);
    pa_ellipse(stdout, x-l, y-l, x+l, y+l);
    pa_fcolor(stdout, pa_black);
    pa_bover(stdout);
    while (w < 10) {

        a = 0; /* set angle */
        while (a < 360) {

            pline(a, l, x, y, w);
            a = a+10;

        };
        pa_home(stdout);
        printf("Line width: %d\n", w);
        w = w+1;
        waitnext();

    }
    pa_binvis(stdout);
    pa_linewidth(stdout, 1);

    /* ******************************* Color test 1 ****************************** */

    putchar('\f');
    y = 1; /* set 1st row */
    r = 0; /* set colors */
    g = 0;
    b = 0;
    while (y < pa_maxyg(stdout)) {

        x = 1;
        while (x < pa_maxxg(stdout)) {

            pa_fcolorg(stdout, r, g, b);
            pa_frect(stdout, x, y, x+COLSQR-1, y+COLSQR-1);
            x = x+COLSQR;
            if (r <= INT_MAX-INT_MAX/COLDIV) r = r+INT_MAX/COLDIV;
            else {

                r = 0;
                if (g <= INT_MAX-INT_MAX/COLDIV) g = g+INT_MAX/COLDIV;
                else {

                    g = 0;
                    if (b <= INT_MAX-INT_MAX/COLDIV) b = b+INT_MAX/COLDIV;
                    else b = 0;

                }

            }

        }
        y = y+COLSQR;

    }
    pa_fcolor(stdout, pa_black);
    pa_bcolor(stdout, pa_white);
    pa_bover(stdout);
    prtcen(pa_maxy(stdout), "Color test 1");
    pa_binvis(stdout);
    waitnext();

    /* ******************************* Color test 2 ****************************** */

    putchar('\f');
    x = 1; /* set 2st collumn */
    while (x < pa_maxxg(stdout)) {

        pa_fcolorg(stdout, INT_MAX/pa_maxxg(stdout)*x, 0, 0);
        pa_line(stdout, x, 1, x, pa_maxyg(stdout));
        x = x+1;

    }
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    pa_bcolor(stdout, pa_white);
    pa_bover(stdout);
    prtcen(pa_maxy(stdout), "Color test 2");
    pa_binvis(stdout);
    waitnext();

    /* ******************************* Color test 3 ****************************** */

    putchar('\f');
    x = 1; /* set 2st collumn */
    while (x < pa_maxxg(stdout)) {

        pa_fcolorg(stdout, 0, INT_MAX/pa_maxxg(stdout)*x, 0);
        pa_line(stdout, x, 1, x, pa_maxyg(stdout));
        x = x+1;

    }
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    pa_bcolor(stdout, pa_white);
    pa_bover(stdout);
    prtcen(pa_maxy(stdout), "Color test 3");
    pa_binvis(stdout);
    waitnext();

    /* ******************************* Color test 4 ****************************** */

    putchar('\f');
    x = 1; /* set 2st collumn */
    while (x < pa_maxxg(stdout)) {

        pa_fcolorg(stdout, 0, 0, INT_MAX/pa_maxxg(stdout)*x);
        pa_line(stdout, x, 1, x, pa_maxyg(stdout));
        x = x+1;

    }
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    pa_bcolor(stdout, pa_white);
    pa_bover(stdout);
    prtcen(pa_maxy(stdout), "Color test 4");
    pa_binvis(stdout);
    waitnext();

    /* ***************************** Rectangle test **************************** */

    putchar('\f');
    grid();
    l = 10;
    x = pa_maxxg(stdout)/2; /* find center */
    y = pa_maxyg(stdout)/2;
    w = 1;
    c = pa_black;
    while (l < pa_maxxg(stdout)/2 && l < pa_maxyg(stdout)/2-pa_chrsizy(stdout)) {

        pa_fcolor(stdout, c);
        pa_linewidth(stdout, w);
        pa_rect(stdout, x-l, y-l, x+l, y+l);
        l = l+20;
        w = w+1;
        if (c < pa_magenta) c++;
        else c = pa_black;
        if (c == pa_white) c++;

    };
    pa_linewidth(stdout, 1);
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Rectangle test");
    waitnext();

    /* ************************ Filled rectangle test 1 ************************ */

    putchar('\f');
    grid();
    if (pa_maxxg(stdout) > pa_maxyg(stdout)) l = pa_maxyg(stdout)/2-pa_chrsizy(stdout);
    else l = pa_maxxg(stdout)/2-pa_chrsizy(stdout);
    x = pa_maxxg(stdout)/2; /* find center */
    y = pa_maxyg(stdout)/2;
    c = pa_black;
    while (l >= 10) {

        pa_fcolor(stdout, c);
        pa_frect(stdout, x-l, y-l, x+l, y+l);
        l = l-20;
        if (c < pa_magenta) c++;
        else c = pa_black;
        if (c == pa_white) c++;

    }
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled rectangle test 1");
    waitnext();

    /* ************************ Filled rectangle test 2 ************************ */

    putchar('\f');
    grid();
    l = 10;
    x = 20;
    y = 20;
    c = pa_black;
    while (y+l*2 < pa_maxyg(stdout)-pa_chrsizy(stdout)) {

        while (x+l*2 < pa_maxxg(stdout)-pa_chrsizy(stdout)) {

            pa_fcolor(stdout, c);
            pa_frect(stdout, x, y, x+l*2, y+l*2);
            x = x+l*2+20;
            l = l+5;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = 10;
        y = y+l*2+10;

    }
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled rectangle test 2");
    waitnext();

    /* ************************* Rounded rectangle test ************************ */

    pa_binvis(stdout);
    r = 1;
    while (r < 100) {

        putchar('\f');
        grid();
        l = 10;
        x = pa_maxxg(stdout)/2; /* find center */
        y = pa_maxyg(stdout)/2;
        w = 1;
        c = pa_black;
        printf("r: %d\n", r);
        while (l+w/2 < pa_maxxg(stdout)/2 && l < pa_maxyg(stdout)/2-pa_chrsizy(stdout)) {

            pa_fcolor(stdout, c);
            pa_linewidth(stdout, w);
            pa_rrect(stdout, x-l, y-l, x+l, y+l, r, r);
            l = l+w;
            w = w+1;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        pa_linewidth(stdout, 1);
        pa_fcolor(stdout, pa_black);
        prtcen(pa_maxy(stdout), "Rounded rectangle test");
        waitnext();
        r = r+10;

    }
    /* ******************* rounded rectangle minimums test **************** */

    putchar('\f');
    xsize = pa_maxxg(stdout)/20;
    ysize = pa_maxyg(stdout)/20;

    /* paint the grid */
    pa_fcolor(stdout, pa_cyan);
    x1 = 1;
    while (x1 < pa_maxxg(stdout)) {

        pa_line(stdout, x1, 1, x1, pa_maxyg(stdout));
        x1 += xsize;

    }
    y1 = 1;
    while (y1 < pa_maxyg(stdout)) {

        pa_line(stdout, 1, y1, pa_maxxg(stdout), y1);
        y1 += ysize;

    }
    pa_fcolor(stdout, pa_black);

    /* draw vertical */
    x1 = 1+xsize;
    y1 = 1+ysize;
    x2 = x1+xsize*2;
    y2 = y1;
    while (y2+ysize < pa_maxyg(stdout)) {

        pa_rrect(stdout, x1, y1, x2, y2, 10, 10);
        y1 = y1+ysize;
        y2 = y2+ysize+1;

    }

    /* draw horizontal */
    x1 = 1+xsize*4;
    y1 = 1+ysize;
    x2 = x1;
    y2 = ysize*4;
    while (x2+xsize < pa_maxxg(stdout)) {

        pa_rrect(stdout, x1, y1, x2, y2, 10, 10);
        x1 = x1+xsize;
        x2 = x2+xsize+1;

    }

    /* draw boxes */
    x1 = 1+xsize*4;
    y1 = 1+ysize*6;
    x2 = x1;
    y2 = y1;
    while (x2 < pa_maxxg(stdout)) {

        pa_rrect(stdout, x1, y1, x2, y2, 10, 10);
        x1 += xsize;
        x2 += xsize+1;
        y2++;

    }
    prtcen(pa_maxy(stdout), "Rounded Rectangle Minimums Test");
    waitnext();

    /* ******************** Filled rounded rectangle test 1 ******************** */

    pa_binvis(stdout);
    r = 1;
    while (r < 100) {

        putchar('\f');
        grid();
        if (pa_maxxg(stdout) > pa_maxyg(stdout)) l = pa_maxyg(stdout)/2-pa_chrsizy(stdout);
        else l = pa_maxxg(stdout)/2-pa_chrsizy(stdout);
        x = pa_maxxg(stdout)/2; /* find center */
        y = pa_maxyg(stdout)/2;
        c = pa_black;
        printf("r: %d\n", r);
        while (l >= 10) {

            pa_fcolor(stdout, c);
            pa_frrect(stdout, x-l, y-l, x+l, y+l, r, r);
            l = l-20;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        pa_fcolor(stdout, pa_black);
        prtcen(pa_maxy(stdout), "Filled rounded rectangle test 1");
        waitnext();
        r = r+10;

    }

    /* ******************** Filled rounded rectangle test 2 ******************** */

    pa_binvis(stdout);
    r = 1;
    while (r < 100) {

        putchar('\f');
        grid();
        printf("r: %d\n", r);
        l = 10;
        x = 20;
        y = pa_curyg(stdout);
        c = pa_black;
        while (y+l*2 < pa_maxyg(stdout)-20) {

            while (x+l*2 < pa_maxxg(stdout)-20) {

                pa_fcolor(stdout, c);
                pa_frrect(stdout, x, y, x+l*2, y+l*2, r, r);
                x = x+l*2+20;
                l = l+5;
                if (c < pa_magenta) c++; else c = pa_black;
                if (c == pa_white) c++;

            }
            x = 10;
            y = y+l*2+10;

        }
        pa_fcolor(stdout, pa_black);
        pa_binvis(stdout);
        prtcen(pa_maxy(stdout), "Filled rounded rectangle test 2");
        waitnext();
        r = r+10;

    }

    /* ******************* filled rounded rectangle minimums test **************** */

    putchar('\f');
    xsize = pa_maxxg(stdout)/20;
    ysize = pa_maxyg(stdout)/20;

    /* paint the grid */
    pa_fcolor(stdout, pa_cyan);
    x1 = 1;
    while (x1 < pa_maxxg(stdout)) {

        pa_line(stdout, x1, 1, x1, pa_maxyg(stdout));
        x1 += xsize;

    }
    y1 = 1;
    while (y1 < pa_maxyg(stdout)) {

        pa_line(stdout, 1, y1, pa_maxxg(stdout), y1);
        y1 += ysize;

    }
    pa_fcolor(stdout, pa_black);

    /* draw vertical */
    x1 = 1+xsize;
    y1 = 1+ysize;
    x2 = x1+xsize*2;
    y2 = y1;
    while (y2+ysize < pa_maxyg(stdout)) {

        pa_frrect(stdout, x1, y1, x2, y2, 10, 10);
        y1 = y1+ysize;
        y2 = y2+ysize+1;

    }

    /* draw horizontal */
    x1 = 1+xsize*4;
    y1 = 1+ysize;
    x2 = x1;
    y2 = ysize*4;
    while (x2+xsize < pa_maxxg(stdout)) {

        pa_frrect(stdout, x1, y1, x2, y2, 10, 10);
        x1 = x1+xsize;
        x2 = x2+xsize+1;

    }

    /* draw boxes */
    x1 = 1+xsize*4;
    y1 = 1+ysize*6;
    x2 = x1;
    y2 = y1;
    while (x2 < pa_maxxg(stdout)) {

        pa_frrect(stdout, x1, y1, x2, y2, 10, 10);
        x1 += xsize;
        x2 += xsize+1;
        y2++;

    }
    prtcen(pa_maxy(stdout), "Filled Rectangle Minimums Test");
    waitnext();

    /* ****************************** Ellipse test ***************************** */

    pa_binvis(stdout);
    w = 1;
    while (w < 10) {

        putchar('\f');
        grid();
        lx = pa_maxxg(stdout)/2-10;
        lx = lx-lx%10;
        ly = pa_maxyg(stdout)/2-10-pa_chrsizy(stdout);
        ly = ly-ly%10;
        x = pa_maxxg(stdout)/2; /* find center */
        y = pa_maxyg(stdout)/2;
        x = x-x%10;
        y = y-y%10;
        c = pa_black;
        printf("width: %d\n", w);
        while (lx >= 10 && ly >= 10) {

            pa_fcolor(stdout, c);
            pa_linewidth(stdout, w);
            pa_ellipse(stdout, x-lx, y-ly, x+lx, y+ly);
            lx = lx-20;
            ly = ly-20;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        pa_fcolor(stdout, pa_black);
        prtcen(pa_maxy(stdout), "Ellipse test");
        waitnext();
        w = w+1;

    }
    pa_linewidth(stdout, 1);

    /* ************************** Filled ellipse test 1 ************************** */

    putchar('\f');
    grid();
    lx = pa_maxxg(stdout)/2-10;
    lx = lx-lx%10;
    ly = pa_maxyg(stdout)/2-10-pa_chrsizy(stdout);
    ly = ly-ly%10;
    x = pa_maxxg(stdout)/2; /* find center */
    y = pa_maxyg(stdout)/2;
    x = x-x%10;
    y = y-y%10;
    c = pa_black;
    while (lx >= 10 && ly >= 10) {

        pa_fcolor(stdout, c);
        pa_fellipse(stdout, x-lx, y-ly, x+lx, y+ly);
        lx = lx-20;
        ly = ly-20;
        if (c < pa_magenta) c++; else c = pa_black;
        if (c == pa_white) c++;

    }
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Filled ellipse test 1");
    waitnext();

    /* ************************ Filled ellipse test 2 ************************ */

    putchar('\f');
    grid();
    l = 10;
    x = 20;
    y = 20;
    c = pa_black;
    while (y+l*2 < pa_maxyg(stdout)-20) {

        while (x+l*2 < pa_maxxg(stdout)-20) {

            pa_fcolor(stdout, c);
            pa_fellipse(stdout, x, y, x+l*2, y+l*2);
            x = x+l*2+20;
            l = l+5;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = 10;
        y = y+l*2+10;

    }
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled ellipse test 2");
    waitnext();

    /* ******************************* Arc test 1 ******************************** */

    pa_binvis(stdout);
    w = 1;
    while (w < 10) {

        putchar('\f');
        grid();
        a = 0;
        c = pa_black;
        i = 10;
        printf("Linewidth: %d", w);
        while (i < pa_maxxg(stdout)/2 && i < (pa_maxyg(stdout)-pa_chrsizy(stdout))/2) {

            a = 0;
            while (a <= INT_MAX-INT_MAX/10) {

                pa_fcolor(stdout, c);
                pa_linewidth(stdout, w);
                pa_arc(stdout, i, i, pa_maxxg(stdout)-i,
                       pa_maxyg(stdout)-pa_chrsizy(stdout)-i, a, a+INT_MAX/10);
                a = a+INT_MAX/5;
                if (c < pa_magenta) c++; else c = pa_black;
                if (c == pa_white) c++;

            }
            i = i+20;

        }
        pa_fcolor(stdout, pa_black);
        prtcen(pa_maxy(stdout), "Arc test 1");
        waitnext();
        w = w+1;

    }

    /* ************************ Arc test 2 ************************ */

    pa_binvis(stdout);
    w = 1;
    xspace = pa_maxxg(stdout)/40;
    yspace = pa_maxyg(stdout)/40;
    while (w < 10) {

        putchar('\f');
        grid();
        printf("Linewidth: %d\n", w);
        l = pa_maxxg(stdout)/40;
        x = xspace;
        y = pa_curyg(stdout);
        c = pa_black;
        aa = 0;
        ab = INT_MAX / 360*90;
        while (y+l*2 < pa_maxyg(stdout)-yspace) {

            while (x+l*2 < pa_maxxg(stdout)-xspace) {

                pa_fcolor(stdout, pa_red);
                pa_linewidth(stdout, 1);
                pa_rect(stdout, x, y, x+l*2, y+l*2);
                pa_fcolor(stdout, pa_black);
                pa_linewidth(stdout, w);
                pa_arc(stdout, x, y, x+l*2, y+l*2, aa, ab);
                x = x+l*2+xspace;
                l = l+pa_maxxg(stdout)/60;

            }
            x = xspace;
            y = y+l*2+yspace;

        }
        pa_binvis(stdout);
        prtcen(pa_maxy(stdout), "Arc test 2");
        waitnext();
        w = w+1;

    }

    /* ************************ Arc test 3 ************************ */

    pa_binvis(stdout);
    w = 1;
    xspace = pa_maxxg(stdout)/25;
    yspace = xspace;
    while (w < 10) {

        putchar('\f');
        grid();
        printf("Linewidth: %d\n", w);
        l = xspace;
        x = xspace;
        y = pa_curyg(stdout);
        c = pa_black;
        aa = 0;
        ab = 10;
        while (y+l*2 < pa_maxyg(stdout)-yspace && ab <= 360) {

            while (x+l*2 < pa_maxxg(stdout)-xspace && ab <= 360) {

                pa_fcolor(stdout, pa_red);
                pa_linewidth(stdout, 1);
                pa_rect(stdout, x, y, x+l*2, y+l*2);
                pa_fcolor(stdout, pa_black);
                pa_linewidth(stdout, w);
                pa_arc(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
                x = x+l*2+xspace;
                ab = ab+10;

            }
            x = xspace;
            y = y+l*2+yspace;

        }
        pa_binvis(stdout);
        prtcen(pa_maxy(stdout), "Arc test 3");
        waitnext();
        w = w+1;

    }

    /* ************************ Arc test 4 ************************ */

    pa_binvis(stdout);
    w = 1;
    xspace = pa_maxxg(stdout)/25;
    yspace = xspace;
    while (w < 10) {

        putchar('\f');
        grid();
        printf("Linewidth: %d\n", w);
        l = xspace;
        x = xspace;
        y = pa_curyg(stdout);
        c = pa_black;
        aa = 0;
        ab = 360;
        while (y+l*2 < pa_maxyg(stdout)-yspace && aa < 360) {

            while (x+l*2 < pa_maxxg(stdout)-xspace && aa < 360) {

                pa_fcolor(stdout, pa_red);
                pa_linewidth(stdout, 1);
                pa_rect(stdout, x, y, x+l*2, y+l*2);
                pa_fcolor(stdout, pa_black);
                pa_linewidth(stdout, w);
                pa_arc(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
                x = x+l*2+xspace;
                aa = aa+10;

            }
            x = xspace;
            y = y+l*2+yspace;

        }
        pa_binvis(stdout);
        prtcen(pa_maxy(stdout), "Arc test 4");
        waitnext();
        w = w+1;

    }

    /* **************************** Filled arc test 1 **************************** */

    putchar('\f');
    grid();
    a = 0;
    c = pa_black;
    a = 0;
    x = pa_maxxg(stdout)-10;
    x = x-x % 10;
    y = pa_maxyg(stdout)-pa_chrsizy(stdout)-10;
    y = y-y % 10;
    while (a <= INT_MAX-INT_MAX/10) {

        pa_fcolor(stdout, c);
        pa_farc(stdout, 10, 10, x, y, a, a+INT_MAX/10);
        a = a+INT_MAX / 5;
        if (c < pa_magenta) c++; else c = pa_black;
        if (c == pa_white) c++;

    };
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Filled arc test 1");
    waitnext();

    /* ************************ filled arc test 2 ************************ */

    putchar('\f');
    xspace = pa_maxxg(stdout)/40;
    yspace = pa_maxyg(stdout)/40;
    grid();
    l = pa_maxxg(stdout)/50;
    x = xspace;
    y = yspace;
    c = pa_black;
    aa = 0;
    ab = INT_MAX / 360*90;
    while (y+l*2 < pa_maxyg(stdout)-yspace) {

        while (x+l*2 < pa_maxxg(stdout)-xspace) {

            pa_fcolor(stdout, pa_red);
            pa_linewidth(stdout, 1);
            pa_rect(stdout, x, y, x+l*2, y+l*2);
            pa_fcolor(stdout, c);
            pa_farc(stdout, x, y, x+l*2, y+l*2, aa, ab);
            x = x+l*2+xspace;
            l = l+pa_maxxg(stdout)/40;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Filled arc test 2");
    waitnext();

    /* ************************ Filled arc test 3 ************************ */

    putchar('\f');
    xspace = pa_maxxg(stdout)/40;
    yspace = pa_maxyg(stdout)/40;
    grid();
    l = pa_maxxg(stdout)/21;
    x = xspace;
    y = yspace;
    c = pa_black;
    aa = 0;
    ab = 10;
    while (y+l*2 < pa_maxyg(stdout)-yspace && ab <= 360) {

        while (x+l*2 < pa_maxxg(stdout)-xspace && ab <= 360) {

            pa_fcolor(stdout, c);
            pa_farc(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+xspace;
            ab = ab+10;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Arc test 3");
    waitnext();

    /* ************************ Filled arc test 4 ************************ */

    putchar('\f');
    xspace = pa_maxxg(stdout)/40;
    yspace = pa_maxyg(stdout)/40;
    grid();
    l = pa_maxxg(stdout)/21;
    x = xspace;
    y = yspace;
    c = pa_black;
    aa = 0;
    ab = 360;
    while (y+l*2 < pa_maxyg(stdout)-yspace && aa < 360) {

        while (x+l*2 < pa_maxxg(stdout)-xspace && aa < 360) {

            pa_fcolor(stdout, c);
            pa_farc(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+xspace;
            aa = aa+10;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Arc test 4");
    waitnext();

    /* *************************** Filled chord test 1 *************************** */

    putchar('\f');
    grid();
    a = 0;
    c = pa_black;
    a = 0;
    i = 8;
    x = pa_maxxg(stdout)-10;
    x = x-x%10;
    y = pa_maxyg(stdout)-pa_chrsizy(stdout)-10;
    y = y-y%10;
    while (a <= INT_MAX-INT_MAX/i) {

        pa_fcolor(stdout, c);
        pa_fchord(stdout, 10, 10, x, y, a, a+INT_MAX/i);
        a = a+INT_MAX/(i/2);
        if (c < pa_magenta) c++; else c = pa_black;
        if (c == pa_white) c++;

    }
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Filled chord test 1");
    waitnext();

    /* ************************ filled chord test 2 ************************ */

    putchar('\f');
    xspace = pa_maxxg(stdout)/50;
    yspace = xspace;
    grid();
    l = pa_maxxg(stdout)/100;
    x = xspace;
    y = yspace;
    c = pa_black;
    aa = 0;
    ab = INT_MAX/360*90;
    while (y+l*2 < pa_maxyg(stdout)-yspace) {

        while (x+l*2 < pa_maxxg(stdout)-xspace) {

            pa_fcolor(stdout, c);
            pa_fchord(stdout, x, y, x+l*2, y+l*2, aa, ab);
            x = x+l*2+xspace;
            l = l+pa_maxxg(stdout)/100;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Filled chord test 2");
    waitnext();

    /* ************************ Filled chord test 3 ************************ */

    putchar('\f');
    xspace = pa_maxxg(stdout)/50;
    yspace = xspace;
    grid();
    l = pa_maxxg(stdout)/20;
    x = xspace;
    y = yspace;
    c = pa_black;
    aa = 0;
    ab = 10;
    while (y+l*2 < pa_maxyg(stdout)-yspace && ab <= 360) {

        while (x+l*2 < pa_maxxg(stdout)-xspace && ab <= 360) {

            pa_fcolor(stdout, c);
            pa_fchord(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+xspace;
            ab = ab+10;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Filled chord test 3");
    waitnext();

    /* ************************ Filled chord test 4 ************************ */

    putchar('\f');
    xspace = pa_maxxg(stdout)/50;
    yspace = xspace;
    grid();
    l = pa_maxxg(stdout)/20;
    x = xspace;
    y = yspace;
    c = pa_black;
    aa = 0;
    ab = 360;
    while (y+l*2 < pa_maxyg(stdout)-yspace && aa < 360) {

        while (x+l*2 < pa_maxxg(stdout)-xspace && aa < 360) {

            pa_fcolor(stdout, c);
            pa_fchord(stdout, x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+xspace;
            aa = aa+10;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = xspace;
        y = y+l*2+yspace;

    }
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Filled chord test 4");
    waitnext();

    /* ************************** Filled triangle test 1 ************************* */

    putchar('\f');
    grid();
    tx1 = 10;
    ty1 = pa_maxyg(stdout)-pa_chrsizy(stdout)-10;
    ty1 = ty1-ty1%10;
    tx2 = pa_maxxg(stdout)/2;
    ty2 = 10;
    tx3 = pa_maxxg(stdout)-10;
    tx3 = tx3-tx3%10;
    ty3 = pa_maxyg(stdout)-pa_chrsizy(stdout)-10;
    ty3 = ty3-ty3%10;
    c = pa_black;
    i = 40;
    while (tx1 <= tx3-10 && ty2 <= ty3-10) {

        pa_fcolor(stdout, c);
        pa_ftriangle(stdout, tx1, ty1, tx2, ty2, tx3, ty3);
        tx1 = tx1+i;
        ty1 = ty1-i/2;
        ty2 = ty2+i;
        tx3 = tx3-i;
        ty3 = ty3-i/2;
        if (c < pa_magenta) c++; else c = pa_black;
        if (c == pa_white) c++;

    }
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled triangle test 1");
    waitnext();

    /* ************************** Filled triangle test 2 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    while (y < pa_maxyg(stdout)-20-l) {

        while (y < pa_maxyg(stdout)-20-l && x < pa_maxxg(stdout)-20-l) {

            pa_fcolor(stdout, c);
            pa_ftriangle(stdout, x, y+l, x+l / 2, y, x+l, y+l);
            x = x+l+20;
            l = l+10;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled triangle test 2");
    waitnext();

    /* ************************** Filled triangle test 3 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    while (y < pa_maxyg(stdout)-20-l) {

        while (y < pa_maxyg(stdout)-20-l && x < pa_maxxg(stdout)-20-l) {

            pa_fcolor(stdout, c);
            pa_ftriangle(stdout, x, y+l, x, y, x+l, y+l);
            x = x+l+20;
            l = l+10;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled triangle test 3");
    waitnext();

    /* ************************** Filled triangle test 4 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    while (y < pa_maxyg(stdout)-20-l) {

        while (y < pa_maxyg(stdout)-20-l && x < pa_maxxg(stdout)-20-l) {

            pa_fcolor(stdout, c);
            pa_ftriangle(stdout, x, y+l, x, y, x+l, y);
            x = x+l+20;
            l = l+10;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled triangle test 4");
    waitnext();

    /* ************************** Filled triangle test 5 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    while (y < pa_maxyg(stdout)-20-l) {

        while (y < pa_maxyg(stdout)-20-l && x < pa_maxxg(stdout)-20-l) {

            pa_fcolor(stdout, c);
            pa_ftriangle(stdout, x+l/2, y+l, x, y, x+l, y);
            x = x+l+20;
            l = l+10;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled triangle test 5");
    waitnext();

    /* ************************** Filled triangle test 6 ************************* */

    putchar('\f');
    grid();
    x = 20;
    y = 20;
    l = 20;
    c = pa_black;
    while (y < pa_maxyg(stdout)-20-l) {

        while (y < pa_maxyg(stdout)-20-l && x < pa_maxxg(stdout)-20-l) {

            pa_fcolor(stdout, c);
            pa_ftriangle(stdout, x+l, y+l, x, y, x+l, y);
            x = x+l+20;
            l = l+10;
            if (c < pa_magenta) c++; else c = pa_black;
            if (c == pa_white) c++;

        }
        x = 20;
        y = y+l+20;

    }
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled triangle test 6");
    waitnext();

    /* ************************** Filled triangle test 7 ************************* */

    putchar('\f');
    grid();
    c = pa_black;
    pa_fcolor(stdout, c);
    pa_ftriangle(stdout, 50, 50, 50, 100, 200, 50);
    if (c < pa_magenta) c++; else c = pa_black;
    if (c == pa_white) c++;
    pa_fcolor(stdout, c);
    pa_ftriangle(stdout, 50, 100, 300, 200, 200, 50);
    if (c < pa_magenta) c++; else c = pa_black;
    if (c == pa_white) c++;
    pa_fcolor(stdout, c);
    pa_ftriangle(stdout, 200, 50, 300, 200, 350, 100);
    if (c < pa_magenta) c++; else c = pa_black;
    if (c == pa_white) c++;
    pa_fcolor(stdout, c);
    pa_ftriangle(stdout, 350, 100, 400, 300, 300, 200);
    if (c < pa_magenta) c++; else c = pa_black;
    if (c == pa_white) c++;
    pa_binvis(stdout);
    pa_fcolor(stdout, pa_black);
    prtcen(pa_maxy(stdout), "Filled triangle test 7");
    waitnext();

    /* ************************** Filled triangle test 8 ************************* */

    putchar('\f');
    grid();
    pa_fcolor(stdout, pa_black);
    pa_ftriangle(stdout, 50, 50, 50, 100, 200, 50);
    pa_ftriangle(stdout, 50, 100, 300, 200, 200, 50);
    pa_ftriangle(stdout, 200, 50, 300, 200, 350, 100);
    pa_ftriangle(stdout, 350, 100, 400, 300, 300, 200);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Filled triangle test 8");
    waitnext();

    /* **************************** Font sizing test *************************** */

    putchar('\f');
    grid();
    fsiz = pa_chrsizy(stdout); /* save character size to restore */
    h = 10;
    pa_auto(stdout, OFF);
    pa_font(stdout, PA_FONT_SIGN);
    c1 = pa_black;
    c2 = pa_blue;
    pa_bover(stdout);
    while (pa_curyg(stdout)+pa_chrsizy(stdout) <= pa_maxyg(stdout)-20) {

        pa_fcolor(stdout, c1);
        pa_bcolor(stdout, c2);
        pa_fontsiz(stdout, h);
        puts(S2);
        h = h+5;
        if (c1 < pa_magenta) c1++; else c1 = pa_black;
        if (c1 == pa_white) c1++;
        if (c2 < pa_magenta) c2++; else c2 = pa_black;
        if (c2 == pa_white) c2++;

    }
    pa_fontsiz(stdout, fsiz); /* restore font size */
    pa_fcolor(stdout, pa_black);
    pa_bcolor(stdout, pa_white);
    pa_font(stdout, PA_FONT_TERM);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Font sizing test");
    waitnext();

    /* ***************************** Font list test **************************** */

    putchar('\f');
    grid();
    printf("Number of fonts: %d\n", pa_fonts(stdout));
    printf("\n");
    i = 1;
    cnt = pa_fonts(stdout);
    while (cnt > 0) {

        /* find defined font code */
        do {

            pa_fontnam(stdout, i, fns, 100);
            if (!strlen(fns)) i++;

        } while (!strlen(fns));
        printf("%d: %s\n", i, fns);
        if (pa_cury(stdout) >= pa_maxy (stdout)) { /* screen overflows */

            printf("Press return to continue");
            waitnext();
            putchar('\f');
            grid();

        }
        i = i+1; /* next font code */
        cnt = cnt-1; /* count fonts */

    }
    printf("\n");
    printf("List complete\n");
    waitnext();

    /* *************************** Font examples test ************************** */

    putchar('\f');
    grid();
    pa_auto(stdout, OFF);
    pa_bcolor(stdout, pa_cyan);
    pa_bover(stdout);
    i = 1;
    cnt = pa_fonts(stdout);
    while (cnt) {

        /* find defined font code */
        do {

            pa_fontnam(stdout, i, fns, 100);
            if (!strlen(fns)) i++;

        } while (!strlen(fns));
        pa_font(stdout, i);
        printf("%d: %s\n", i, fns);
        if (pa_cury(stdout) >= pa_maxy(stdout)) { /* screen overflows */

            pa_font(stdout, PA_FONT_TERM);
            printf("Press return to continue");
            waitnext();
            pa_bcolor(stdout, pa_white);
            putchar('\f');
            grid();
            pa_bcolor(stdout, pa_cyan);

        }
        i++; /* next font code */
        cnt--; /* count fonts */

    }
    pa_bcolor(stdout, pa_white);
    pa_font(stdout, PA_FONT_TERM);
    pa_binvis(stdout);
    printf("\n");
    printf("List complete\n");
    waitnext();

    /* ************************** Extended effects test ************************ */

    putchar('\f');
    grid();
    pa_auto(stdout, OFF);
    pa_font(stdout, PA_FONT_SIGN);
    pa_condensed(stdout, ON);
    printf("Condensed\n");
    pa_condensed(stdout, OFF);
    pa_extended(stdout, ON);
    printf("Extended\n");
    pa_extended(stdout, OFF);
    pa_xlight(stdout, ON);
    printf("Extra light\n");
    pa_xlight(stdout, OFF);
    pa_light(stdout, ON);
    printf("Light\n");
    pa_light(stdout, OFF);
    pa_xbold(stdout, ON);
    printf("Extra bold\n");
    pa_xbold(stdout, OFF);
    pa_hollow(stdout, ON);
    printf("Hollow\n");
    pa_hollow(stdout, OFF);
    pa_raised(stdout, ON);
    printf("Raised\n");
    pa_raised(stdout, OFF);
    pa_font(stdout, PA_FONT_TERM);
    prtcen(pa_maxy(stdout), "Extended effects test");
    waitnext();

    /* ****************** Character sizes and positions test ******************* */

    putchar('\f');
    grid();
    pa_auto(stdout, OFF);
    fsiz = pa_chrsizy(stdout); /* save character size to restore */
    pa_font(stdout, PA_FONT_SIGN);
    pa_fontsiz(stdout, pa_maxyg(stdout)/12);
    printf("Size of test string: %d\n", pa_strsiz(stdout, S3));
    printf("\n");
    x = (pa_maxxg(stdout)/2)-(pa_strsiz(stdout, S3)/2);
    pa_cursorg(stdout, x, pa_curyg(stdout)); /* go to centered */
    pa_bcolor(stdout, pa_cyan);
    pa_bover(stdout);
    printf("%s\n", S3);
    pa_fcolor(stdout, pa_white);
    pa_frect(stdout, x, pa_curyg(stdout), x+pa_strsiz(stdout, S3)-1,
            pa_curyg(stdout)+pa_chrsizy(stdout)-1);
    pa_fcolor(stdout, pa_black);
    pa_rect(stdout, x, pa_curyg(stdout), x+pa_strsiz(stdout, S3)-1,
            pa_curyg(stdout)+pa_chrsizy(stdout)-1);
    for (i = 0; i <= strlen(S3); i++)
        pa_line(stdout, x+pa_chrpos(stdout, S3, i), pa_curyg(stdout),
                x+pa_chrpos(stdout, S3, i),
                pa_curyg(stdout)+pa_chrsizy(stdout)-1);
    printf("\n");

    l = pa_strsiz(stdout, S4); /* get minimum sizing for string */
    justcenter(S4, l);
    justcenter(S4, l+40);
    justcenter(S4, l+80);

    pa_fontsiz(stdout, fsiz); /* restore font size */
    pa_font(stdout, PA_FONT_TERM);
    pa_binvis(stdout);
    prtcen(pa_maxy(stdout), "Character sizes and positions");
    waitnext();
    pa_bcolor(stdout, pa_white);

    /* ************************* Graphical tabbing test ************************ */

    putchar('\f');
    grid();
    pa_auto(stdout, OFF);
    pa_font(stdout, PA_FONT_TERM);
    for (i = 1; i <= 5; i++) {

        for (x = 1; x <= i; x++) putchar('\t');
        printf("Terminal tab: %d\n", i);

    }
    pa_clrtab(stdout);
    for (i = 1; i <= 5; i++) pa_settabg(stdout, i*43);
    for (i = 1; i <= 5; i++) {

        for (x = 1; x <= i; x++) putchar('\t');
        printf("Graphical tab number: %d position: %d\n", i, i*43);

    }
    pa_restabg(stdout, 2*43);
    pa_restabg(stdout, 4*43);
    printf("\n");
    printf("After removing tabs %d and %d\n", 2*43, 4*43);
    printf("\n");
    for (i = 1; i <= 5; i++) {

        for (x = 1; x <= i; x++) putchar('\t');
        printf("Graphical tab number: %d\n", i);

    }
    prtcen(pa_maxy(stdout), "Graphical tabbing test");
    waitnext();

    /* ************************** Picture draw test **************************** */

    putchar('\f');
    grid();
    pa_maknam(fn, 100, "tests", "mypic", "");
    pa_loadpict(stdout, 1, fn);
    printf("Picture size for 1: x: %d y: %d\n", pa_pictsizx(stdout, 1),
           pa_pictsizy(stdout, 1));
    pa_maknam(fn, 100, "tests", "mypic1", "bmp");
    pa_loadpict(stdout, 2, fn);
    printf("Picture size for 2: x: %d y: %d\n", pa_pictsizx(stdout, 2),
           pa_pictsizy(stdout, 2));
    printf("\n");
    y = pa_curyg(stdout);
    xspace = pa_maxxg(stdout)/20;
    xsize = pa_maxxg(stdout)/6;
    yspace = xspace;
    ysize = xsize;
    pa_picture(stdout, 1, xspace, y, xspace+xsize, y+ysize);
    pa_picture(stdout, 1, xspace+xsize, y+ysize, xspace+xsize*2, y+ysize*2);
    pa_picture(stdout, 1, xspace, y+ysize*2, xspace+xsize, y+ysize*3);
    pa_picture(stdout, 2, xspace+pa_maxxg(stdout)/2, y,
                          xspace+xsize+pa_maxxg(stdout)/2, y+ysize);
    pa_picture(stdout, 2, xspace+xsize+pa_maxxg(stdout)/2, y+ysize,
                          xspace+xsize*2+pa_maxxg(stdout)/2, y+ysize+ysize/2);

    pa_picture(stdout, 2, xspace+pa_maxxg(stdout)/2, y+ysize*2,
                          xspace+xsize/2+pa_maxxg(stdout)/2, y+ysize*3);
    pa_delpict(stdout, 1);
    pa_delpict(stdout, 2);
    prtcen(pa_maxy(stdout), "Picture draw test");
    waitnext();

    /* ********************** Invisible foreground test ************************ */

    putchar('\f');
    grid();
    printf("\n");
    pa_bover(stdout);
    pa_finvis(stdout);
    graphtest(1);
    pa_binvis(stdout);
    pa_fover(stdout);
    prtcen(pa_maxy(stdout), "Invisible foreground test");
    waitnext();
    pa_fover(stdout);

    /* ********************** Invisible background test ************************ */

    putchar('\f');
    grid();
    printf("\n");
    pa_binvis(stdout);
    pa_fover(stdout);
    graphtest(1);
    pa_binvis(stdout);
    pa_fover(stdout);
    prtcen(pa_maxy(stdout), "Invisible background test");
    waitnext();
    pa_bover(stdout);

    /* ************************** Xor foreground test ************************** */

    putchar('\f');
    grid();
    printf("\n");
    pa_bover(stdout);
    pa_fxor(stdout);
    graphtest(1);
    pa_binvis(stdout);
    pa_fover(stdout);
    prtcen(pa_maxy(stdout), "Xor foreground test");
    waitnext();
    pa_fover(stdout);

    /* ************************* Xor background test *************************** */

    putchar('\f');
    grid();
    printf("\n");
    pa_bxor(stdout);
    pa_fover(stdout);
    graphtest(1);
    pa_binvis(stdout);
    pa_fover(stdout);
    prtcen(pa_maxy(stdout), "Xor background test");
    waitnext();
    pa_bover(stdout);

    /* ************************** Graphical scrolling test **************************** */

    putchar('\f');
    grid();
    pa_binvis(stdout);
    prtcen(1, "Use up, down, right && left keys to scroll by pixel");
    prtcen(2, "Hit enter to continue");
    prtcen(3, "Note that edges will clear to green as screen moves");
    prtcen(pa_maxy(stdout), "Graphical scrolling test");
    pa_bcolor(stdout, pa_green);
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etup) pa_scrollg(stdout, 0, -1);
        if (er.etype == pa_etdown) pa_scrollg(stdout, 0, 1);
        if (er.etype == pa_etright) pa_scrollg(stdout, 1, 0);
        if (er.etype == pa_etleft) pa_scrollg(stdout, -1, 0);
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_bover(stdout);
    pa_bcolor(stdout, pa_white);

    /* ************************** Graphical mouse movement test **************************** */

    putchar('\f');
    prtcen(1, "Move the mouse around");
    prtcen(3, "Hit Enter to continue");
    prtcen(pa_maxy(stdout), "Graphical mouse movement test");
    x = -1;
    y = -1;
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etmoumovg) {

            if (x > 0 && y > 0) pa_line(stdout, x, y, er.moupxg, er.moupyg);
            x = er.moupxg;
            y = er.moupyg;

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);

    /* ************************** Animation test **************************** */

#endif
    squares();

    /* ************************** View offset test **************************** */

#if 0 /* view offsets are not completely working */
    putchar('\f');
    pa_auto(stdout, OFF);
    pa_viewoffg(stdout, -(pa_maxxg(stdout)/2), -(pa_maxyg(stdout)/2));
    grid();
    pa_fcolor(stdout, PA_GREEN);
    pa_frect(stdout, 0, 0, 100, 100);
    pa_cursorg(stdout, 1, -(maxyg(stdout)/2));
    pa_fcolor(stdout, pa_black);
    printf("View offset test\n");
    printf("\n");
    printf("The 1,1 origin is now at screen center\n");
    waitnext();
    pa_viewoffg(stdout, 0, 0);
#endif

   /* ************************** View scale test **************************** */

#if 0 /* view scales are not completely working */
    putchar('\f');
    pa_auto(stdout, OFF);
    pa_viewscale(stdout, 0.5);
    grid();
    pa_fcolor(stdout, PA_GREEN);
    pa_frect(stdout, 0, 0, 100, 100);
    prtcen(1, "Logical coordinates are now 1/2 size");
    prtcen(pa_maxy(stdout), "View scale text");
    waitnext();
#endif

    /* ************************** Benchmarks **************************** */

    i = 100000;
    linespeed(1, i, &s);
    benchtab[bnline1].iter = i;
    benchtab[bnline1].time = s;
    printf("Line speed for width: 1, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per line %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    linespeed(10, i, &s);
    benchtab[bnline10].iter = i;
    benchtab[bnline10].time = s;
    printf("Line speed for width: 10, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per line %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    rectspeed(1, i, &s);
    benchtab[bnrect1].iter = i;
    benchtab[bnrect1].time = s;
    printf("Rectangle speed for width: 1, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per rectangle %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    rectspeed(10, i, &s);
    benchtab[bnrect10].iter = i;
    benchtab[bnrect10].time = s;
    printf("Rectangle speed for width: 10, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per rectangle %f", s*0.0001/i);
    waitnext();

    i = 100000;
    rrectspeed(1, i, &s);
    benchtab[bnrrect1].iter = i;
    benchtab[bnrrect1].time = s;
    printf("Rounded rectangle speed for width: 1, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per rounded rectangle %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    rrectspeed(10, i, &s);
    benchtab[bnrrect10].iter = i;
    benchtab[bnrrect10].time = s;
    printf("Rounded rectangle speed for width: 10, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per rounded rectangle %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    frectspeed(i, &s);
    benchtab[bnfrect].iter = i;
    benchtab[bnfrect].time = s;
    printf("Filled rectangle speed, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per filled rectangle %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    frrectspeed(i, &s);
    benchtab[bnfrrect].iter = i;
    benchtab[bnfrrect].time = s;
    printf("Filled rounded rectangle speed, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per filled rounded rectangle %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    ellipsespeed(1, i, &s);
    benchtab[bnellipse1].iter = i;
    benchtab[bnellipse1].time = s;
    printf("Ellipse speed for width: 1, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per ellipse %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    ellipsespeed(10, i, &s);
    benchtab[bnellipse10].iter = i;
    benchtab[bnellipse10].time = s;
    printf("Ellipse speed for width: 10, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per ellipse %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    fellipsespeed(i, &s);
    benchtab[bnfellipse].iter = i;
    benchtab[bnfellipse].time = s;
    printf("Filled ellipse speed, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per filled ellipse %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    arcspeed(1, i, &s);
    benchtab[bnarc1].iter = i;
    benchtab[bnarc1].time = s;
    printf("Arc speed for width: 1, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per arc %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    arcspeed(10, i, &s);
    benchtab[bnarc10].iter = i;
    benchtab[bnarc10].time = s;
    printf("Arc speed for width: 10, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per arc %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    farcspeed(i, &s);
    benchtab[bnfarc].iter = i;
    benchtab[bnfarc].time = s;
    printf("Filled arc speed, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per filled arc %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    fchordspeed(i, &s);
    benchtab[bnfchord].iter = i;
    benchtab[bnfchord].time = s;
    printf("Filled chord speed, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per filled chord %f\n", s*0.0001/i);
    waitnext();

    i = 100000;
    ftrianglespeed(i, &s);
    benchtab[bnftriangle].iter = i;
    benchtab[bnftriangle].time = s;
    printf("Filled triangle speed, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per filled triangle %f\n", s*0.0001/i);
    waitnext();

    pa_bover(stdout);
    pa_fover(stdout);
    i = 100000/4;
    ftextspeed(i, &s);
    benchtab[bntext].iter = i;
    benchtab[bntext].time = s;
    pa_home(stdout);
    printf("Text speed, with overwrite, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per write %f\n", s*0.0001/i);
    waitnext();

    pa_binvis(stdout);
    pa_fover(stdout);
    i = 100000/4;
    ftextspeed(i, &s);
    benchtab[bntextbi].iter = i;
    benchtab[bntextbi].time = s;
    pa_home(stdout);
    pa_bover(stdout);
    printf("Text speed, invisible background, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per write %f\n", s*0.0001/i);
    waitnext();

    i = 10000;
    fpictspeed(i, &s);
    benchtab[bnpict].iter = i;
    benchtab[bnpict].time = s;
    printf("Picture draw speed, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per picture %f\n", s*0.0001/i);
    waitnext();

    i = 10000;
    fpictnsspeed(i, &s);
    benchtab[bnpictns].iter = i;
    benchtab[bnpictns].time = s;
    printf("No scale picture draw speed, %d lines %f seconds\n", i, s*0.0001);
    printf("Seconds per picture %f\n", s*0.0001/i);
    waitnext();

    /* stdout table */

    fprintf(stderr, "\n");
    fprintf(stderr, "Benchmark table\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Type                        Seconds  Per fig\n");
    fprintf(stderr, "--------------------------------------------------\n");
    for (bi = bnline1; bi <= bnpictns; bi++) {

        switch (bi) { /* benchmark type */

            case bnline1:     fprintf(stderr, "line width 1                "); break;
            case bnline10:    fprintf(stderr, "line width 10               "); break;
            case bnrect1:     fprintf(stderr, "rectangle width 1           "); break;
            case bnrect10:    fprintf(stderr, "rectangle width 10          "); break;
            case bnrrect1:    fprintf(stderr, "rounded rectangle width 1   "); break;
            case bnrrect10:   fprintf(stderr, "rounded rectangle width 10  "); break;
            case bnfrect:     fprintf(stderr, "filled rectangle            "); break;
            case bnfrrect:    fprintf(stderr, "filled rounded rectangle    "); break;
            case bnellipse1:  fprintf(stderr, "ellipse width 1             "); break;
            case bnellipse10: fprintf(stderr, "ellipse width 10            "); break;
            case bnfellipse:  fprintf(stderr, "filled ellipse              "); break;
            case bnarc1:      fprintf(stderr, "arc width 1                 "); break;
            case bnarc10:     fprintf(stderr, "arc width 10                "); break;
            case bnfarc:      fprintf(stderr, "filled arc                  "); break;
            case bnfchord:    fprintf(stderr, "filled chord                "); break;
            case bnftriangle: fprintf(stderr, "filled triangle             "); break;
            case bntext:      fprintf(stderr, "text                        "); break;
            case bntextbi:    fprintf(stderr, "background invisible text   "); break;
            case bnpict:      fprintf(stderr, "Picture draw                "); break;
            case bnpictns:    fprintf(stderr, "No scaling picture draw     "); break;

        };
        fprintf(stderr, "%5.2f", benchtab[bi].time*0.0001);
        fprintf(stderr, "    ");
        fprintf(stderr, "%f", benchtab[bi].time*0.0001/benchtab[bi].iter);
        fprintf(stderr, "\n");

    }

    terminate: /* terminate */

    putchar('\f');
    pa_auto(stdout, OFF);
    pa_font(stdout, PA_FONT_SIGN);
    pa_fontsiz(stdout, 50);
    prtceng(pa_maxy(stdout)/2, "Test complete");

    return (0);

}
