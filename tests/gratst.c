/*******************************************************************************
*                                                                             *
*                           GRAPHICS TEST PROGRAM                             *
*                                                                             *
*                    Copyright (C) 2005 Scott A. Moore                        *
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
*******************************************************************************/

#include <stdio.h>
#include <services.h>
#include <graph.h>

#define S1     "Moving char*";
#define S2     "Variable size char*";
#define S3     "Sizing test char*";
#define S4     "Justify test char*";
#define S5     "Invisible body text";
#define S6     "Example text";
#define COLDIV 6; /* number of color divisions */
#define COLSQR 20; /* size of color square */
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

char      fns[100];
int       x, y;
int       xs, ys;
int       i;
int       dx, dy;
int       ln;
int       term;
int       w;
int       l;
int       a;
int       r, g, b;
pa_color  c, c1, c2;
int       lx, ly;
int       x1, y1, x2, y2, x3, y3;
int       h;
int       cnt;
pa_evtrec er;
int       fsiz;
int       aa, ab;
int       s;
struct { /* benchmark stats records */

    int iter; /* number of iterations performed */
    int time; /* time in 100us for test */

} benchtab[bnpictns+1];
bench bi;

/* Find random number between 0 and N. */

int randn(int limit)

{

    return (long)limit*rand()/RAND_MAX;

}

/* Find random number in given range */

int randr(int s, int e)

{

    return (randn(e-s)+s);

}

/* wait time in 100 microseconds */

void wait(long t)

{

    pa_evtrec er;

    pa_timer(stdout, 1, t, FALSE);
    do { event(stdin, er); }
    while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) { longjmp(terminate_buf, 1); }

}

/* wait time in 100 microseconds, with space terminate */

void waitchar(long t, int* st)

{

    pa_evtrecr er;

    *st = FALSE; /* set no space terminate */
    pa_timer(stdout, 1, t, FALSE);
    do { event(input, er); }
    while (er.etype != pa_ettim && er.etype != pa_etterm &&
           er.etype != pa_etchar && er.etype != pa_etenter);
   if (er.etype == pa_etchar && er.char == " ") *st = TRUE;
   if (er.etype == pa_etenter) *st = TRUE;
   if (er.etype == pa_etterm) { longjmp(terminate_buf, 1); }

}

/* wait return to be pressed, or handle terminate */

void waitnext(void)

{

    pa_evtrec er; /* event record */

    do { repeat event(input, er); }
    while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) { longjmp(terminate_buf, 1); }

}

/* print centered char* */

void prtcen(int y, const char* s);

{

   cursor(stdout, (pa_maxx(stdout)/2)-(strlen(s)/2), y);
   puts(s);

}

/* print centered char* graphical */

void prtceng(int y, const char* s)

{

   pa_cursorg(stdout, (pa_maxxg(stdout)/2)-(pa_strsiz(stdout, s)/2), y);
   puts(s);

}

/* print all printable characters */

void prtall(void)

{

    char c;
    char s[2];

    s[1] = 0;
    for (c = " "; c <= "}"; c++) {

      s[0] = "c";
      if (pa_curxg(stdout)+pa_strsiz(s) > maxxg then cursorg(1, curyg+chrsizy);
      putchar(c);

   }
   putchar("\n");

};

/* draw a character grid */

void chrgrid(void)

{

    int x, y;

    pa_fcolor(pa_yellow);
    y = 1;
    while (y < pa_maxyg(stdout)) {

        pa_line(1, y, pa_maxxg(stdout, y);
        y = y+pa_chrsizy(stdout);

    }
    x = 1;
    while (x < pa_maxxg(stdout) {

        pa_line(x, 1, x, maxyg(stdout));
        x = x+chrsizx(stdout);

    }
    pa_fcolor(pa_black);

}

/* find rectangular coordinates from polar, relative to center of circle,
  with given diameter */

void rectcord(int  a, /* angle, 0-359 */
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

void pline(int a,      /* angle of line */
           int o,      /* length of line */
           int cx, cy, /* center of circle in x && y */
           int w       /* width of line */
          )

{

    int ex, ey; /* line start and end */

    rectcord(a, o, &ex, &ey); /* find endpoint of line */
    pa_linewidth(stdout, w); /* set width */
    pa_line(stdout, cx, cy, cx+ex, cy-ey); /* draw line */

}

/* draw centered justified text */

void justcenter(const char* s, int l)

{

    int i, x;

    x = pa_maxxg(stdout)/2-l/2;
    pa_cursorg(x, pa_curyg(stdout));
    pa_writejust(s, l);
    putchar('\n');
    pa_rect(x, pa_curyg(stdout), x+l-1, pa_curyg(stdout)+pa_chrsizy(stdout)-1);
    for (i = 2; i <= strlen(s); i++)
      pa_line(x+pa_justpos(stdout, s, i, l), pa_curyg(stdout),
              x+pa_justpos(stdout, s, i, l), pa_curyg(stdout)+pa_chrsizy(stdout)-1);
   putchar("\n");

}

/* draw 10"s grid */

void grid(void)

{

    int x, y;

    pa_linewidth(stdout, 1);
    pa_fcolor(stdout, cyan);
    x = 10;
    while (x <= pa_maxxg(stdout)) {

        pa_line(x, 1, x, pa_maxyg(stdout));
        x = x+10;

    }
    y = 10;
    while (y <= pa_maxyg(stdout)) {

        pa_line(stdout, 1, y, pa_maxxg(stdout), y);
        y = y+10;

    }
    pa_fcolor(stdout, pa_black);

}

/* This is the square2 program */

const int squaresize = 81;
const int halfsquare = squaresize / 2;
const int maxsquare = 10;
const int reprate = 1; /* number of moves per frame, should be low */

typedef struct { /* square data record */

    int      x, y;   /* current position */
    int      lx, ly; /* last position */
    int      xd, yd; /* deltas */
    pa_color c;      /* color */

} balrec;

balrec baltbl[maxsquare];

int chkbrk(void)

{

    pa_evtrec er; /* event record */
    int done;

    done = FALSE;
    do { event(input, er); }
    while (er.etype != etframe && er.etype != etterm) &&
           er.etype != etchar && er.etype != etenter);
    if (er.etype == etterm) { longjmp(terminate_buf, 1); }
    if (er.etype == etchar || er.etype == etenter)
        done = TRUE; /* terminate */

    return (done);

}

void drawsquare(pa_color c, int x, int y)

{

    pa_fcolor(stdout, c); /* set color */
    pa_frect(stdout, x-halfsquare+1, y-halfsquare+1, x+halfsquare-1, y+halfsquare-1)

}

void movesquare(int s)

{

    balrec* bt;
    int nx, ny; /* temp coordinates holders */

    bt = &baltbl[s];
    nx = bt->x+bt->xd; /* trial move square */
    ny = bt->y+bt->yd;
    /* check out of bounds and reverse direction */
    if (nx < halfsquare || nx > pa_maxxg(stdout)-halfsquare+1) bt->xd = -bt->xd;
    if (ny < halfsquare || (ny > pa_maxyg(stdout)-halfsquare+1) bt->yd = -bt->yd;
    bt->x = bt->x+bt->xd; /* move square */
    bt->y = bt->y+bt->yd

}

void squares(void);

{

    int     cd; /* current display flip select */
    int     i; /* index for table */
    int     rc; /* repetition counter */
    int     done; /* done flag */
    balrec* bt;

    /* initalize square data */
    for (i = 1; i <= maxsquare; i++) do with baltbl[i] do {

        bt = &baltbl[i];
        bt->x = rand % (pa_maxxg(stdout)-squaresize)+halfsquare;
        bt->y = rand % (pa_maxyg(stdout)-squaresize)+halfsquare;
        if (!(rand % 2)) bt->xd = +1; else bt->xd = -1;
        if (!(rand % 2)) bt->yd = +1; else bt->yd = -1;
        bt->lx = bt->x; /* set last position to same */
        bt->ly = bt->y;
        bt->c = rand%6+pa_red; /* set random color */

    }
    pa_curvis(stdout, false); /* turn off cursor */
    cd = FALSE; /* set 1st display */
    /* place squares on display */
    for (i = 0; i < maxsquare; i++) drawsquare(baltbl[i].c, baltbl[i].x, baltbl[i].y);
    pa_frametimer(stdout, true); /* start frame timer */
    done = FALSE; /* set ! done */
    while (!done) {

        /* select display and update surfaces */
        select(stdout, ord(not cd)+1, ord(cd)+1);
        putchar("\n");
        pa_fover(stdout);
        pa_fcolor(stdout, pa_black);
        prtcen(pa_maxy(stdout), "Animation test");
        pa_fxor(stdout);
        /* save old positions */
        for (i = 0; i < maxsquare; i++) {

            baltbl[i].lx = baltbl[i].x; /* save last position */
            baltbl[i].ly = baltbl[i].y;

        }
        /* move squares */
        for (rc = 1; rc <= reprate; rc++) /* repeats per frame */
            for (i = 1; i <= maxsquare; i++) movesquare(i); /* process squares */
        /* draw squares */
        for (i = 0; i < maxsquare; i++)
            drawsquare(baltbl[i].c, baltbl[i].x, baltbl[i].y);
        cd = !cd; /* flip display && update surfaces */
        done = chkbrk(); /* check complete */

    }
    pa_select(stdout, 1, 1); /* restore buffer surfaces */

}

/* draw standard graphical test, which is all the figures possible
  arranged on the screen */

void graphtest(int lw); /* line width */

{

    int fsiz;
    int x, y;

    pa_auto(stdout, OFF);
    pa_font(stdout, PA_FONT_SIGN);
    fsiz = chrsizy(stdout); /* save character size to restore */
    pa_fontsiz(stdout, 30);
    pa_bcolor(stdout, pa_yellow);
    pa_cursorg(stdout, pa_maxxg(stdout) / 2-pa_strsiz(stdout, S6) / 2, pa_curyg(stdout));
    printf("%s\n", S6);
    putchar("\n");
    pa_fcolor(stdout, pa_magenta);
    pa_linewidth(stdout, lw);
    y = 70;
    x = 20;
    pa_rect(stdout, x, y, x+100, y+100);
    pa_fcolor(stdout, pa_green);
    x = x+120;
    pa_frect(stdout, x, y, x+100, y+100);
    pa_fcolor(pa_yellow);
    x = x+120;
    pa_ftriangle(stdout, x, y+100, x+50, y, x+100, y+100);
    pa_fcolor(stdout, pa_red);
    x = x+120;
    pa_rrect(stdout, x, y, x+100, y+100, 20, 20);
    pa_fcolor(stdout, pa_magenta);
    x = x+120;
    pa_arc(stdout, x, y, x+100, y+100, 0, INT_MAX / 4);
    pa_fcolor(stdout, pa_green);
    pa_farc(stdout, x, y, x+100, y+100, INT_MAX / 2, INT_MAX / 2+INT_MAX / 4);
    y = y+120;
    x = 20;
    pa_fcolor(stdout, pa_blue);
    pa_frect(stdout, x, y, x+100, y+100);
    x = x+120;
    pa_fcolor(stdout, pa_magenta);
    pa_frrect(stdout, x, y, x+100, y+100, 20, 20);
    x = x+120;
    pa_fcolor(stdout, pa_green);
    pa_ellipse(stdout, x, y, x+100, y+100);
    x = x+120;
    pa_fcolor(stdout, pa_yellow);
    pa_fellipse(x, y, x+100, y+100);
    x = x+120;
    pa_fcolor(stdout, pa_blue);
    pa_fchord(x, y, x+100, y+100, 0, INT_MAX / 2);
    y = y+120;
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
    pa_line(stdout, 20, y, maxxg-20, y);
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

void linespeed(int w, int t, int* s)

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

void rectspeed(int w, int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_linewidth(stdout, w);
    c = pa_clock();
    for i = 1 to t do {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_rect(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                        randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test rounded rectangle speed */

void rrectspeed(int w, int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    ps_linewidth(stdout, w);
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_rrect(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                         randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                         randn(100-1)+1, randn(100-1)+1);

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black)

}

/* test filled rectangle speed */

void frectspeed(int t, int* s)

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

void frrectspeed(int t, int* s)

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
    pa_fcolor(stdout, pa_black)

}

/* test ellipse speed */

void ellipsespeed(int w, int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_linewidth(stdout, w);
    c = pa_clock;
    for i = 1 to t do {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_ellipse(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                           randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test filled ellipse speed */

void fellipsespeed(int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f')
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_fellipse(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                            randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    fcolor(stdout, pa_black);

}

/* test arc speed */

void arcspeed(int w, int t, int* s)

{

    int i;
    int c;
    int sa, ea;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_linewidth(stdout, w);
    c = pa_clock();
    for (i = 1; i <= to; i++) {

        do {

            sa = randn(INT_MAX);
            ea = randn(INT_MAX);

        } while (ea <= sa);
        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_arc(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                       randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                       sa, ea);

    }
    s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

/* test filled arc speed */

void farcspeed(int t, int* s)

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

void fchordspeed(int t, int* s)

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
    pa_fcolor(stdout, pa_black)

}

/* test filled triangle speed */

void ftrianglespeed(int t,  int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    c = pa_clock;
    for i = 1 to t do {

        pa_fcolor(stdout, randr(pa_red, pa_magenta));
        pa_ftriangle(stdout, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                             randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                             randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black)

}

/* test text speed */

void ftextspeed(int t, int* s)

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
        puts("Test text");

    }
    *s = pa_elapsed(c);
    pa_fcolor(pa_black);
    pa_bcolor(pa_white);

}

/* test picture draw speed */

void fpictspeed(int t, int* s)

{

    int i;
    int c;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    putchar('\f');
    pa_loadpict(1, "mypic");
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        pa_picture(stdout, 1, randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)),
                              randr(1, pa_maxxg(stdout)), randr(1, pa_maxyg(stdout)));

   }
   *s = pa_elapsed(c);
   pa_fcolor(stdout, pa_black);

}

/* test picture draw speed, no scaling */

void fpictnsspeed(int t, int* s)

{

    int i;
    int c;
    int x, y;
    int xs, ys;

    pa_auto(stdout, false);
    pa_curvis(stdout, false);
    putchar('\f');
    pa_loadpict(stdout, 1, "mypic");
    xs = pa_pictsizx(stdout, 1);
    ys = pa_pictsizy(stdout, 1);
    c = pa_clock();
    for (i = 1; i <= t; i++) {

        x = randr(1, pa_maxxg(stdout);
        y = randr(1, pa_maxyg(stdout));
        pa_picture(stdout, 1, x, y, x+xs-1, y+ys-1);

    }
    *s = pa_elapsed(c);
    pa_fcolor(stdout, pa_black);

}

int main(void)

{

   pa_curvis(false);
   printf("Graphics screen test vs. 0.1\n");
   printf("\n");
   printf("Screen size in characters: x -> %d y -> %d\n", pa_maxx(stdout),
                                                          pa_maxy(stdout));
   printf("            in pixels:     x -> %d y -> %d\n",
          pa_maxxg(stdout), pa_maxyg(stdout));
   printf("Size of character in default font: x -> %d y -> %d\n",
          pa_chrsizx(stdout), pa_chrsizy(stdout));
   printf("Dots per meter: dpmx: %d dpmy: %d\n", pa_dpmx, pa_dpmy);
   printf("Aspect ratio: %f", pa_dpmx/pa_dpmy);
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
   pa_bover(stdout);
   graphtest(2);
   pa_binvis(stdout);
   prtcen(pa_maxy(stdout), "Graphical figures test, linewidth == 2");
   waitnext();

   putchar('\f');
   grid();
   printf("\n");
   pa_bover(stdout);
   graphtest(3);
   pa_binvis(stdout);
   prtcen(pa_maxy(stdout), "Graphical figures test, linewidth == 3");
   waitnext();

   putchar('\f');
   grid();
   printf("\n");
   pa_bover(stdout);
   graphtest(5);
   pa_binvis(stdout);
   prtcen(pa_maxy(stdout), "Graphical figures test, linewidth == 5");
   waitnext();

   putchar('\f');
   grid();
   printf("\n");
   pa_bover(stdout);
   graphtest(11);
   pa_binvis(stdout);
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
        printf("This is the terminal font: System name: \"%s\" Size x -> %d y -> %d\n",
               fns, pa_chrsizx(stdout), pa_chrsizy(stdout));
        prtall();
        printf("\n");

    } else {

        printf("There is no terminal font\n");
        printf("\n");

    }
    pa_fontnam(PA_FONT_BOOK, fns);
    if (strlen(fns) > 0) {

        pa_font(PA_FONT_BOOK);
        pa_fontsiz(20);
        printf("This is the book font: System name: \"%s\" Size x -> %d y -> %d\n",
               fns, pa_chrsizx(stdout), pa_chrsizy(stdout));
        prtall();
        printf("\n");

    } else {

        printf("There is no book font\n");
        printf("\n");

    }
   fontnam(font_sign, fns);
   if len(fns) > 0 then {

      font(font_sign);
      fontsiz(20);
      writeln("This is the sign font: System name: "", fns:0,
              "" Size x -> ", chrsizx:1, " y -> ", chrsizy:1);
      prtall;
      writeln

   } else {

      writeln("There is no sign font");
      writeln

   };
   fontnam(font_tech, fns);
   if len(fns) > 0 then {

      font(font_tech);
      fontsiz(20);
      writeln("This is the technical font: System name: "", fns:0,
              "" Size x -> ", chrsizx:1, " y -> ", chrsizy:1);
      prtall;
      writeln

   } else {

      writeln("There is no technical font");
      writeln

   };
   font(font_term);
   writeln("Complete");
   waitnext();

   /* ********************** Graphical cursor movement test ******************* */

   putchar('\f');
   prtcen(maxy(stdout), "Graphical cursor movement test");
   x = 1;
   y = 1;
   i = 10000;
   dx = +1;
   dy = +1;
   ln = strsiz(S1);
   term = false;
   while ! term do {

      cursorg(x, y);
      write(S1);
      xs = x;
      ys = y;
      x = x+dx;
      y = y+dy;
      if (x < 1) || (x+ln-1 > maxxg) then {

         x = xs;
         dx = -dx

      };
      if (y < 1) || (y+chrsizy-1 > maxyg) then {

         y = ys;
         dy = -dy

      };
      waitchar(100, term);
      cursorg(xs, ys);
      fcolor(white);
      write(S1);
      fcolor(black)

   };

   /* *************************** Vertical lines test ************************* */

   putchar('\f');
   grid();
   prtcen(maxy(stdout), "Vertical lines test");
   y = 20;
   w = 1;
   while (y < maxyg-30) && (w < 15) do {

      linewidth(w);
      line(20, y, maxxg-20, y);
      y = y+30;
      w = w+1

   };
   linewidth(1);
   waitnext();

   /* ************************* Horizontal lines test ************************* */

   putchar('\f');
   grid();
   prtcen(maxy(stdout), "Horizontal lines test");
   x = 20;
   w = 1;
   y = maxyg-20;
   y = y-y % 10;
   while (x < maxxg-20) && (w < 30) do {

      linewidth(w);
      line(x, 20, x, y);
      x = x+30;
      w = w+1

   };
   linewidth(1);
   waitnext();

   /* **************************** Polar lines test *************************** */

   putchar('\f');
   grid();
   binvis(stdout);
   prtcen(maxy(stdout), "Polar lines test");
   bover(stdout);
   x = maxxg / 2;
   x = x-(x % 10);
   y = maxyg / 2;
   y = y-(y % 10);
   if maxxg > maxyg then l = maxyg / 2-40
                    else l = maxxg / 2-40;
   l = l-(l % 10);
   w = 1;
   fcolor(blue);
   ellipse(x-l, y-l, x+l, y+l);
   fcolor(black);
   while w < 10 do {

      a = 0; /* set angle */
      while a < 360 do {

         pline(a, l, x, y, w);
         a = a+10;

      };
      pa_home(stdout);
      writeln("Line width: ", w:1);
      w = w+1;
      waitnext

   };
   linewidth(1);

   /* ************************* Progressive lines test ************************ */

   putchar('\f');
   grid();
   line(10, 10, 100, 100);
   line(100, 10);
   line(200, 50);
   line(10, 100);
   line(50, 230);
   line(20, 130);
   line(250, 80);
   line(100, 40);
   line(160, 180);
   line(80, 160);
   line(120, 30);
   line(90, 90);
   line(20, 50);
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Progressive lines test");
   waitnext();

   /* ******************************* Color test 1 ****************************** */

   putchar('\f');
   y = 1; /* set 1st row */
   r = 0; /* set colors */
   g = 0;
   b = 0;
   while y < maxyg do {

      x = 1;
      while x < maxxg do {

         fcolor(r, g, b);
         frect(x, y, x+COLSQR-1, y+COLSQR-1);
         x = x+COLSQR;
         if r <== INT_MAX-INT_MAX / COLDIV then r = r+INT_MAX / COLDIV
         else {

            r = 0;
            if g <== INT_MAX-INT_MAX / COLDIV then g = g+INT_MAX / COLDIV
            else {

               g = 0;
               if b <== INT_MAX-INT_MAX / COLDIV then b = b+INT_MAX / COLDIV
               else b = 0

            }

         }

      };
      y = y+COLSQR

   };
   fcolor(black);
   prtcen(maxy(stdout), "Color test 1");
   waitnext();

   /* ******************************* Color test 2 ****************************** */

   putchar('\f');
   x = 1; /* set 2st collumn */
   while x < maxxg do {

      fcolor(INT_MAX / maxxg*x, 0, 0);
      line(x, 1, x, maxyg);
      x = x+1

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Color test 2");
   waitnext();

   /* ******************************* Color test 3 ****************************** */

   putchar('\f');
   x = 1; /* set 2st collumn */
   while x < maxxg do {

      fcolor(0, INT_MAX / maxxg*x, 0);
      line(x, 1, x, maxyg);
      x = x+1

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Color test 3");
   waitnext();

   /* ******************************* Color test 4 ****************************** */

   putchar('\f');
   x = 1; /* set 2st collumn */
   while x < maxxg do {

      fcolor(0, 0, INT_MAX / maxxg*x);
      line(x, 1, x, maxyg);
      x = x+1

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Color test 4");
   waitnext();

   /* ***************************** Rectangle test **************************** */

   putchar('\f');
   grid();
   l = 10;
   x = maxxg / 2; /* find center */
   y = maxyg / 2;
   x = x-x % 10;
   y = y-y % 10;
   w = 1;
   c = black;
   while (l < maxxg / 2) && (l < maxyg / 2) do {

      fcolor(c);
      linewidth(w);
      rect(x-l, y-l, x+l, y+l);
      l = l+20;
      w = w+1;
      if c < magenta then c = succ(c)
      else c = black;
      if c == white then c = succ(c)

   };
   linewidth(1);
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Rectangle test");
   waitnext();

   /* ************************ Filled rectangle test 1 ************************ */

   putchar('\f');
   grid();
   if maxxg > maxyg then l = maxyg / 2-10 else l = maxxg / 2-10;
   l = l-l % 10;
   x = maxxg / 2; /* find center */
   y = maxyg / 2;
   x = x-x % 10;
   y = y-y % 10;
   c = black;
   while (l >== 10) && (l < maxyg / 2) do {

      fcolor(c);
      frect(x-l, y-l, x+l, y+l);
      l = l-20;
      if c < magenta then c = succ(c)
      else c = black;
      if c == white then c = succ(c)

   };
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled rectangle test 1");
   waitnext();

   /* ************************ Filled rectangle test 2 ************************ */

   putchar('\f');
   grid();
   l = 10;
   x = 20;
   y = 20;
   c = black;
   while y+l*2 < maxyg-20 do {

      while x+l*2 < maxxg-20 do {

         fcolor(c);
         frect(x, y, x+l*2, y+l*2);
         x = x+l*2+20;
         l = l+5;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 10;
      y = y+l*2+10

   };
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled rectangle test 2");
   waitnext();

   /* ************************* Rounded rectangle test ************************ */

   binvis(stdout);
   r = 1;
   while r < 100 do {

      putchar('\f');
      grid();
      l = 10;
      x = maxxg / 2; /* find center */
      y = maxyg / 2;
      x = x-x % 10;
      y = y-y % 10;
      w = 1;
      c = black;
      writeln("r: ", r:1);
      while (l < maxxg / 2) && (l < maxyg / 2) do {

         fcolor(c);
         linewidth(w);
         rrect(x-l, y-l, x+l, y+l, r, r);
         l = l+20;
         w = w+1;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      linewidth(1);
      fcolor(black);
      prtcen(maxy(stdout), "Rounded rectangle test");
      waitnext();
      r = r+10

   };

   /* ******************** Filled rounded rectangle test 1 ******************** */

   binvis(stdout);
   r = 1;
   while r < 100 do {

      putchar('\f');
      grid();
      if maxxg > maxyg then l = maxyg / 2-10 else l = maxxg / 2-10;
      l = l-l % 10;
      x = maxxg / 2; /* find center */
      y = maxyg / 2;
      x = x-x % 10;
      y = y-y % 10;
      c = black;
      writeln("r: ", r:1);
      while (l >== 10) && (l < maxyg / 2) do {

         fcolor(c);
         frrect(x-l, y-l, x+l, y+l, r, r);
         l = l-20;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      fcolor(black);
      prtcen(maxy(stdout), "Filled rounded rectangle test 1");
      waitnext();
      r = r+10

   };

   /* ******************** Filled rounded rectangle test 2 ******************** */

   binvis(stdout);
   r = 1;
   while r < 100 do {

      putchar('\f');
      grid();
      l = 10;
      x = 20;
      y = 20;
      c = black;
      writeln("r: ", r:1);
      while y+l*2 < maxyg-20 do {

         while x+l*2 < maxxg-20 do {

            fcolor(c);
            frrect(x, y, x+l*2, y+l*2, r, r);
            x = x+l*2+20;
            l = l+5;
            if c < magenta then c = succ(c)
            else c = black;
            if c == white then c = succ(c)

         };
         x = 10;
         y = y+l*2+10

      };
      fcolor(black);
      binvis(stdout);
      prtcen(maxy(stdout), "Filled rounded rectangle test 2");
      waitnext();
      r = r+10

   };

   /* ****************************** Ellipse test ***************************** */

   binvis(stdout);
   w = 1;
   while w < 10 do {

      putchar('\f');
      grid();
      lx = maxxg / 2-10;
      lx = lx-lx % 10;
      ly = maxyg / 2-10;
      ly = ly-ly % 10;
      x = maxxg / 2; /* find center */
      y = maxyg / 2;
      x = x-x % 10;
      y = y-y % 10;
      c = black;
      writeln("width: ", w:1);
      while (lx >== 10) && (ly >== 10) do {

         fcolor(c);
         linewidth(w);
         ellipse(x-lx, y-ly, x+lx, y+ly);
         lx = lx-20;
         ly = ly-20;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      fcolor(black);
      prtcen(maxy(stdout), "Ellipse test");
      waitnext();
      w = w+1

   };
   linewidth(1);

   /* ************************** Filled ellipse test 1 ************************** */

   putchar('\f');
   grid();
   lx = maxxg / 2-10;
   lx = lx-lx % 10;
   ly = maxyg / 2-10;
   ly = ly-ly % 10;
   x = maxxg / 2; /* find center */
   y = maxyg / 2;
   x = x-x % 10;
   y = y-y % 10;
   c = black;
   while (lx >== 10) && (ly >== 10) do {

      fcolor(c);
      fellipse(x-lx, y-ly, x+lx, y+ly);
      lx = lx-20;
      ly = ly-20;
      if c < magenta then c = succ(c)
      else c = black;
      if c == white then c = succ(c)

   };
   fcolor(black);
   prtcen(maxy(stdout), "Filled ellipse test 1");
   waitnext();

   /* ************************ Filled ellipse test 2 ************************ */

   putchar('\f');
   grid();
   l = 10;
   x = 20;
   y = 20;
   c = black;
   while y+l*2 < maxyg-20 do {

      while x+l*2 < maxxg-20 do {

         fcolor(c);
         fellipse(x, y, x+l*2, y+l*2);
         x = x+l*2+20;
         l = l+5;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 10;
      y = y+l*2+10

   };
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled ellipse test 2");
   waitnext();

   /* ******************************* Arc test 1 ******************************** */

   binvis(stdout);
   w = 1;
   while w < 10 do {

      putchar('\f');
      grid();
      a = 0;
      c = black;
      i = 10;
      write("Linewidth: ", w:1);
      while (i < maxxg / 2) && (i < maxyg / 2) do {

         a = 0;
         while a <== INT_MAX-INT_MAX / 10 do {

            fcolor(c);
            linewidth(w);
            arc(i, i, maxxg-i, maxyg-i, a, a+INT_MAX / 10);
            a = a+INT_MAX / 5;
            if c < magenta then c = succ(c)
            else c = black;
            if c == white then c = succ(c)

         };
         i = i+20

      };
      fcolor(black);
      prtcen(maxy(stdout), "Arc test 1");
      waitnext();
      w = w+1

   };

   /* ************************ Arc test 2 ************************ */

   binvis(stdout);
   w = 1;
   while w < 10 do {

      putchar('\f');
      grid();
      l = 10;
      x = 20;
      y = 20;
      c = black;
      aa = 0;
      ab = INT_MAX / 360*90;
      write("Linewidth: ", w:1);
      while y+l*2 < maxyg-20 do {

         while x+l*2 < maxxg-20 do {

            linewidth(w);
            arc(x, y, x+l*2, y+l*2, aa, ab);
            x = x+l*2+20;
            l = l+10;

         };
         x = 10;
         y = y+l*2+10

      };
      binvis(stdout);
      prtcen(maxy(stdout), "Arc test 2");
      waitnext();
      w = w+1

   };

   /* ************************ Arc test 3 ************************ */

   binvis(stdout);
   w = 1;
   while w < 10 do {

      putchar('\f');
      grid();
      l = 30;
      x = 20;
      y = 20;
      c = black;
      aa = 0;
      ab = 10;
      write("Linewidth: ", w:1);
      while (y+l*2 < maxyg-20) && (ab <== 360) do {

         while (x+l*2 < maxxg-20) && (ab <== 360) do {

            linewidth(w);
            arc(x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+20;
            ab = ab+10

         };
         x = 10;
         y = y+l*2+20

      };
      binvis(stdout);
      prtcen(maxy(stdout), "Arc test 3");
      waitnext();
      w = w+1

   };

   /* ************************ Arc test 4 ************************ */

   binvis(stdout);
   w = 1;
   while w < 10 do {

      putchar('\f');
      grid();
      l = 30;
      x = 20;
      y = 20;
      c = black;
      aa = 0;
      ab = 360;
      write("Linewidth: ", w:1);
      while (y+l*2 < maxyg-20) && (ab <== 360) do {

         while (x+l*2 < maxxg-20) && (ab <== 360) do {

            linewidth(w);
            arc(x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
            x = x+l*2+20;
            aa = aa+10

         };
         x = 10;
         y = y+l*2+20

      };
      binvis(stdout);
      prtcen(maxy(stdout), "Arc test 4");
      waitnext();
      w = w+1

   };

   /* **************************** Filled arc test 1 **************************** */

   putchar('\f');
   grid();
   a = 0;
   c = black;
   a = 0;
   x = maxxg-10;
   x = x-x % 10;
   y = maxyg-10;
   y = y-y % 10;
   while a <== INT_MAX-INT_MAX / 10 do {

      fcolor(c);
      farc(10, 10, x, y, a, a+INT_MAX / 10);
      a = a+INT_MAX / 5;
      if c < magenta then c = succ(c)
      else c = black;
      if c == white then c = succ(c)

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Filled arc test 1");
   waitnext();

   /* ************************ filled arc test 2 ************************ */

   putchar('\f');
   grid();
   l = 10;
   x = 20;
   y = 20;
   c = black;
   aa = 0;
   ab = INT_MAX / 360*90;
   while y+l*2 < maxyg-20 do {

      while x+l*2 < maxxg-20 do {

         fcolor(c);
         farc(x, y, x+l*2, y+l*2, aa, ab);
         x = x+l*2+20;
         l = l+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l*2+10

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Filled arc test 2");
   waitnext();

   /* ************************ Filled arc test 3 ************************ */

   putchar('\f');
   grid();
   l = 30;
   x = 20;
   y = 20;
   c = black;
   aa = 0;
   ab = 10;
   while (y+l*2 < maxyg-20) && (ab <== 360) do {

      while (x+l*2 < maxxg-20) && (ab <== 360) do {

         fcolor(c);
         farc(x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
         x = x+l*2+20;
         ab = ab+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l*2+20

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Arc test 3");
   waitnext();

   /* ************************ Filled arc test 4 ************************ */

   putchar('\f');
   grid();
   l = 30;
   x = 20;
   y = 20;
   c = black;
   aa = 0;
   ab = 360;
   while (y+l*2 < maxyg-20) && (ab <== 360) do {

      while (x+l*2 < maxxg-20) && (ab <== 360) do {

         fcolor(c);
         farc(x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
         x = x+l*2+20;
         aa = aa+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l*2+20

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Arc test 3");
   waitnext();

   /* *************************** Filled chord test 1 *************************** */

   putchar('\f');
   grid();
   a = 0;
   c = black;
   a = 0;
   i = 8;
   x = maxxg-10;
   x = x-x % 10;
   y = maxyg-10;
   y = y-y % 10;
   while a <== INT_MAX-INT_MAX / i do {

      fcolor(c);
      fchord(10, 10, x, y, a, a+INT_MAX / i);
      a = a+INT_MAX / (i / 2);
      if c < magenta then c = succ(c)
      else c = black;
      if c == white then c = succ(c)

   };
   fcolor(black);
   prtcen(maxy(stdout), "Filled chord test 1");
   waitnext();

   /* ************************ filled chord test 2 ************************ */

   putchar('\f');
   grid();
   l = 10;
   x = 20;
   y = 20;
   c = black;
   aa = 0;
   ab = INT_MAX / 360*90;
   while y+l*2 < maxyg-20 do {

      while x+l*2 < maxxg-20 do {

         fcolor(c);
         fchord(x, y, x+l*2, y+l*2, aa, ab);
         x = x+l*2+20;
         l = l+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l*2+10

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Filled chord test 2");
   waitnext();

   /* ************************ Filled chord test 3 ************************ */

   putchar('\f');
   grid();
   l = 30;
   x = 20;
   y = 20;
   c = black;
   aa = 0;
   ab = 10;
   while (y+l*2 < maxyg-20) && (ab <== 360) do {

      while (x+l*2 < maxxg-20) && (ab <== 360) do {

         fcolor(c);
         fchord(x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
         x = x+l*2+20;
         ab = ab+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l*2+20

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Filled chord test 3");
   waitnext();

   /* ************************ Filled chord test 4 ************************ */

   putchar('\f');
   grid();
   l = 30;
   x = 20;
   y = 20;
   c = black;
   aa = 0;
   ab = 360;
   while (y+l*2 < maxyg-20) && (ab <== 360) do {

      while (x+l*2 < maxxg-20) && (ab <== 360) do {

         fcolor(c);
         fchord(x, y, x+l*2, y+l*2, aa*DEGREE, ab*DEGREE);
         x = x+l*2+20;
         aa = aa+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l*2+20

   };
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Filled chord test 3");
   waitnext();

   /* ************************** Filled triangle test 1 ************************* */

   putchar('\f');
   grid();
   x1 = 10;
   y1 = maxyg-10;
   y1 = y1-y1 % 10;
   x2 = maxxg / 2;
   y2 = 10;
   x3 = maxxg-10;
   x3 = x3-x3 % 10;
   y3 = maxyg-10;
   y3 = y3-y3 % 10;
   c = black;
   i = 40;
   while (x1 <== x3-10) && (y2 <== y3-10) do {

      fcolor(c);
      ftriangle(x1, y1, x2, y2, x3, y3);
      x1 = x1+i;
      y1 = y1-i / 2;
      y2 = y2+i;
      x3 = x3-i;
      y3 = y3-i / 2;
      if c < magenta then c = succ(c)
      else c = black;
      if c == white then c = succ(c)

   };
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled triangle test 1");
   waitnext();

   /* ************************** Filled triangle test 2 ************************* */

   putchar('\f');
   grid();
   x = 20;
   y = 20;
   l = 20;
   while y < maxyg-20-l do {

      while (y < maxyg-20-l) && (x < maxxg-20-l) do {

         fcolor(c);
         ftriangle(x, y+l, x+l / 2, y, x+l, y+l);
         x = x+l+20;
         l = l+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l+20

   };
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled triangle test 2");
   waitnext();

   /* ************************** Filled triangle test 3 ************************* */

   putchar('\f');
   grid();
   x = 20;
   y = 20;
   l = 20;
   while y < maxyg-20-l do {

      while (y < maxyg-20-l) && (x < maxxg-20-l) do {

         fcolor(c);
         ftriangle(x, y+l, x, y, x+l, y+l);
         x = x+l+20;
         l = l+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l+20

   };
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled triangle test 3");
   waitnext();

   /* ************************** Filled triangle test 4 ************************* */

   putchar('\f');
   grid();
   x = 20;
   y = 20;
   l = 20;
   while y < maxyg-20-l do {

      while (y < maxyg-20-l) && (x < maxxg-20-l) do {

         fcolor(c);
         ftriangle(x, y+l, x, y, x+l, y);
         x = x+l+20;
         l = l+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l+20

   };
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled triangle test 4");
   waitnext();

   /* ************************** Filled triangle test 5 ************************* */

   putchar('\f');
   grid();
   x = 20;
   y = 20;
   l = 20;
   while y < maxyg-20-l do {

      while (y < maxyg-20-l) && (x < maxxg-20-l) do {

         fcolor(c);
         ftriangle(x+l / 2, y+l, x, y, x+l, y);
         x = x+l+20;
         l = l+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l+20

   };
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled triangle test 5");
   waitnext();

   /* ************************** Filled triangle test 6 ************************* */

   putchar('\f');
   grid();
   x = 20;
   y = 20;
   l = 20;
   c = black;
   while y < maxyg-20-l do {

      while (y < maxyg-20-l) && (x < maxxg-20-l) do {

         fcolor(c);
         ftriangle(x+l, y+l, x, y, x+l, y);
         x = x+l+20;
         l = l+10;
         if c < magenta then c = succ(c)
         else c = black;
         if c == white then c = succ(c)

      };
      x = 20;
      y = y+l+20

   };
   fcolor(black);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled triangle test 6");
   waitnext();

   /* ************************** Filled triangle test 7 ************************* */

   putchar('\f');
   grid();
   c = black;
   fcolor(c);
   ftriangle(50, 50, 50, 100, 200, 50);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(50, 100, 300, 200, 200, 50);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(200, 50, 300, 200, 350, 100);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(350, 100, 400, 300, 300, 200);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Filled triangle test 7");
   waitnext();

   /* ************************** Filled triangle test 8 ************************* */

   putchar('\f');
   grid();
   fcolor(black);
   ftriangle(50, 50, 50, 100, 200, 50);
   ftriangle(50, 100, 300, 200, 200, 50);
   ftriangle(200, 50, 300, 200, 350, 100);
   ftriangle(350, 100, 400, 300, 300, 200);
   binvis(stdout);
   prtcen(maxy(stdout), "Filled triangle test 8");
   waitnext();

   /* ************************** Filled triangle test 9 ************************* */

   putchar('\f');
   grid();
   fcolor(black);
   c = black;
   ftriangle(50, 50, 100, 50, 100, 100);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(200, 100, 200, 200);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(250, 100, 300, 200);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(200, 200, 250, 250);
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Filled triangle test 9, progressive singles");
   waitnext();

   /* ************************** Filled triangle test 9 ************************* */

   putchar('\f');
   grid();
   fcolor(black);
   c = black;
   ftriangle(50, 100, 50, 50, 100, 100);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(150, 50);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(200, 160);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(250, 100);
   if c < magenta then c = succ(c)
   else c = black;
   if c == white then c = succ(c);
   fcolor(c);
   ftriangle(300, 100);
   binvis(stdout);
   fcolor(black);
   prtcen(maxy(stdout), "Filled triangle test 10, progressive strips");
   waitnext();

   /* **************************** Font sizing test *************************** */

   putchar('\f');
   grid();
   fsiz = chrsizy; /* save character size to restore */
   h = 10;
   auto(OFF);
   font(font_sign);
   c1 = black;
   c2 = blue;
   bover(stdout);
   while curyg+chrsizy <== maxyg-20 do {

      fcolor(c1);
      bcolor(c2);
      fontsiz(h);
      writeln(S2);
      h = h+5;
      if c1 < magenta then c1 = succ(c1)
      else c1 = black;
      if c1 == white then c1 = succ(c1);
      if c2 < magenta then c2 = succ(c2)
      else c2 = black;
      if c2 == white then c2 = succ(c2)

   };
   fontsiz(fsiz); /* restore font size */
   fcolor(black);
   bcolor(white);
   font(font_term);
   binvis(stdout);
   prtcen(maxy(stdout), "Font sizing test");
   waitnext();

   /* ***************************** Font list test **************************** */

   putchar('\f');
   grid();
   writeln("Number of fonts: ", fonts);
   printf("\n");
   i = 1;
   cnt = fonts;
   while cnt > 0 do {

      /* find defined font code */
      repeat

         fontnam(i, fns);
         if len(fns) == 0 then i = i+1

      until len(fns) > 0;
      writeln(i:1, ": ", fns:0);
      if cury >== maxy then { /* screen overflows */

         write("Press return to continue");
         waitnext();
         putchar('\f');
         grid

      };
      i = i+1; /* next font code */
      cnt = cnt-1 /* count fonts */

   };
   printf("\n");
   writeln("List complete");
   waitnext();

   /* *************************** Font examples test ************************** */

   putchar('\f');
   grid();
   auto(OFF);
   bcolor(cyan);
   bover(stdout);
   i = 1;
   cnt = fonts;
   while cnt > 0 do {

      /* find defined font code */
      repeat

         fontnam(i, fns);
         if len(fns) == 0 then i = i+1

      until len(fns) > 0;
      font(i);
      writeln(i:1, ": ", fns:0);
      if cury >== maxy then { /* screen overflows */

         font(font_term);
         write("Press return to continue");
         waitnext();
         bcolor(white);
         putchar('\f');
         grid();
         bcolor(cyan)

      };
      i = i+1; /* next font code */
      cnt = cnt-1 /* count fonts */

   };
   bcolor(white);
   font(font_term);
   binvis(stdout);
   printf("\n");
   writeln("List complete");
   waitnext();

   /* ************************** Ext}ed effects test ************************ */

   putchar('\f');
   grid();
   auto(OFF);
   font(font_sign);
   condensed(ON);
   writeln("Condensed");
   ext}ed(ON);
   writeln("Ext}ed");
   ext}ed(OFF);
   xlight(ON);
   writeln("Extra light");
   xlight(OFF);
   xbold(ON);
   writeln("Extra bold");
   xbold(OFF);
   hollow(ON);
   writeln("Hollow");
   hollow(OFF);
   raised(ON);
   writeln("Raised");
   raised(OFF);
   font(font_term);
   prtcen(maxy(stdout), "Ext}ed effects test");
   waitnext();

   /* ****************** Character sizes && positions test ******************* */

   putchar('\f');
   grid();
   auto(OFF);
   fsiz = chrsizy; /* save character size to restore */
   font(font_sign);
   fontsiz(30);
   writeln("Size of test char*: ", strsiz(S3));
   printf("\n");
   x = (maxxg(stdout) / 2)-(strsiz(S3) / 2);
   cursorg(x, curyg); /* go to centered */
   bcolor(cyan);
   bover(stdout);
   writeln(S3);
   rect(x, curyg, x+strsiz(S3)-1, curyg+chrsizy-1);
   for i = 2 to len(S3) do
      line(x+chrpos(S3, i), curyg, x+chrpos(S3, i), curyg+chrsizy-1);
   printf("\n");

   l = strsiz(S4); /* get minimum sizing for char* */
   justcenter(S4, l);
   justcenter(S4, l+40);
   justcenter(S4, l+80);

   fontsiz(fsiz); /* restore font size */
   font(font_term);
   binvis(stdout);
   prtcen(maxy(stdout), "Character sizes && positions");
   waitnext();
   bcolor(white);

   /* ************************* Graphical tabbing test ************************ */

   putchar('\f');
   grid();
   auto(OFF);
   font(font_term);
   for i = 1 to 5 do {

      for x = 1 to i do write("\ht");
      writeln("Terminal tab: ", i:1)

   };
   clrtab;
   for i = 1 to 5 do settabg(i*43);
   for i = 1 to 5 do {

      for x = 1 to i do write("\ht");
      writeln("Graphical tab number: ", i:1, " position: ", i*43:1)

   };
   restabg(2*43);
   restabg(4*43);
   printf("\n");
   writeln("After removing tabs ", 2*43, " && ", 4*43);
   printf("\n");
   for i = 1 to 5 do {

      for x = 1 to i do write("\ht");
      writeln("Graphical tab number: ", i:1)

   };
   prtcen(maxy(stdout), "Graphical tabbing test");
   waitnext();

   /* ************************** Picture draw test **************************** */

   putchar('\f');
   grid();
   loadpict(1, "mypic");
   writeln("Picture size for 1: x: ", pictsizx(1):1, " y: ", pictsizy(1):1);
   loadpict(2, "mypic1.bmp");
   writeln("Picture size for 2: x: ", pictsizx(2):1, " y: ", pictsizy(2):1);
   picture(1, 50, 50, 100, 100);
   picture(1, 100, 100, 200, 200);
   picture(1, 50, 200, 100, 350);
   picture(2, 200, 50, 250, 100);
   picture(2, 250, 100, 350, 200);
   picture(2, 250, 250, 450, 300);
   delpict(1);
   delpict(2);
   prtcen(maxy(stdout), "Picture draw test");
   waitnext();

   /* ********************** Invisible foreground test ************************ */

   putchar('\f');
   grid();
   printf("\n");
   bover(stdout);
   finvis;
   graphtest(1);
   binvis(stdout);
   prtcen(maxy(stdout), "Invisible foreground test");
   waitnext();
   fover;

   /* ********************** Invisible background test ************************ */

   putchar('\f');
   grid();
   printf("\n");
   binvis(stdout);
   fover;
   graphtest(1);
   binvis(stdout);
   prtcen(maxy(stdout), "Invisible background test");
   waitnext();
   bover(stdout);

   /* ************************** Xor foreground test ************************** */

   putchar('\f');
   grid();
   printf("\n");
   bover(stdout);
   fxor;
   graphtest(1);
   binvis(stdout);
   prtcen(maxy(stdout), "Xor foreground test");
   waitnext();
   fover;

   /* ************************* Xor background test *************************** */

   putchar('\f');
   grid();
   printf("\n");
   bxor;
   fover;
   graphtest(1);
   binvis(stdout);
   prtcen(maxy(stdout), "Xor background test");
   waitnext();
   bover(stdout);

   /* ************************** Graphical scrolling test **************************** */

   putchar('\f');
   grid();
   binvis(stdout);
   prtcen(1, "Use up, down, right && left keys to scroll by pixel");
   prtcen(2, "Hit enter to continue");
   prtcen(3, "Note that edges will clear to green as screen moves");
   prtcen(maxy(stdout), "Graphical scrolling test");
   bcolor(green);
   repeat

      event(er);
      if er.etype == etup then scrollg(0, -1);
      if er.etype == etdown then scrollg(0, 1);
      if er.etype == etright then scrollg(1, 0);
      if er.etype == etleft then scrollg(-1, 0);
      if er.etype == etterm then goto 99

   until er.etype == etenter;
   bover(stdout);
   bcolor(white);

   /* ************************** Graphical mouse movement test **************************** */

   putchar('\f');
   prtcen(1, "Move the mouse around");
   prtcen(3, "Hit Enter to continue");
   prtcen(maxy(stdout), "Graphical mouse movement test");
   x = -1;
   y = -1;
   repeat

      event(er);
      if er.etype == etmoumovg then {

         if (x > 0) && (y > 0) then line(x, y, er.moupxg, er.moupyg);
         x = er.moupxg;
         y = er.moupyg

      };
      if er.etype == etterm then goto 99

   until er.etype == etenter;

   /* ************************** Animation test **************************** */

   squares;

   /* ************************** View offset test **************************** */

if false then { /* view offsets are ! completely working */
   putchar('\f');
   auto(OFF);
   viewoffg(-(maxxg / 2), -(maxyg / 2));
   grid();
   fcolor(green);
   frect(0, 0, 100, 100);
   cursorg(1, -(maxyg / 2));
   fcolor(black);
   writeln("View offset test");
   printf("\n");
   writeln("The 1,1 origin is now at screen center");
   waitnext();
   viewoffg(0, 0);
};

   /* ************************** View scale test **************************** */

if false then { /* view scales are ! completely working */
   putchar('\f');
   auto(OFF);
   viewscale(0.5);
   grid();
   fcolor(green);
   frect(0, 0, 100, 100);
   prtcen(1, "Logical coordinates are now 1/2 size");
   prtcen(maxy(stdout), "View scale text");
   waitnext();
};

   /* ************************** Benchmarks **************************** */

   i = 100000;
   linespeed(1, i, s);
   with benchtab[bnline1] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Line speed for width: 1, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per line", s*0.0001/i);
   waitnext();

   i = 100000;
   linespeed(10, i, s);
   with benchtab[bnline10] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Line speed for width: 10, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per line", s*0.0001/i);
   waitnext();

   i = 100000;
   rectspeed(1, i, s);
   with benchtab[bnrect1] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Rectangle speed for width: 1, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per rectangle", s*0.0001/i);
   waitnext();

   i = 100000;
   rectspeed(10, i, s);
   with benchtab[bnrect10] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Rectangle speed for width: 10, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per rectangle", s*0.0001/i);
   waitnext();

   i = 100000;
   rrectspeed(1, i, s);
   with benchtab[bnrrect1] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Rounded rectangle speed for width: 1, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per rounded rectangle", s*0.0001/i);
   waitnext();

   i = 100000;
   rrectspeed(10, i, s);
   with benchtab[bnrrect10] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Rounded rectangle speed for width: 10, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per rounded rectangle", s*0.0001/i);
   waitnext();

   i = 1000000;
   frectspeed(i, s);
   with benchtab[bnfrect] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Filled rectangle speed, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per filled rectangle", s*0.0001/i);
   waitnext();

   i = 100000;
   frrectspeed(i, s);
   with benchtab[bnfrrect] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Filled rounded rectangle speed, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per filled rounded rectangle", s*0.0001/i);
   waitnext();

   i = 100000;
   ellipsespeed(1, i, s);
   with benchtab[bnellipse1] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Ellipse speed for width: 1, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per ellipse", s*0.0001/i);
   waitnext();

   i = 100000;
   ellipsespeed(10, i, s);
   with benchtab[bnellipse10] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Ellipse speed for width: 10, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per ellipse", s*0.0001/i);
   waitnext();

   i = 100000;
   fellipsespeed(i, s);
   with benchtab[bnfellipse] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Filled ellipse speed, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per filled ellipse", s*0.0001/i);
   waitnext();

   i = 100000;
   arcspeed(1, i, s);
   with benchtab[bnarc1] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Arc speed for width: 1, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per arc", s*0.0001/i);
   waitnext();

   i = 100000;
   arcspeed(10, i, s);
   with benchtab[bnarc10] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Arc speed for width: 10, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per arc", s*0.0001/i);
   waitnext();

   i = 100000;
   farcspeed(i, s);
   with benchtab[bnfarc] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Filled arc speed, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per filled arc", s*0.0001/i);
   waitnext();

   i = 100000;
   fchordspeed(i, s);
   with benchtab[bnfchord] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Filled chord speed, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per filled chord", s*0.0001/i);
   waitnext();

   i = 1000000;
   ftrianglespeed(i, s);
   with benchtab[bnftriangle] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Filled triangle speed, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per filled triangle", s*0.0001/i);
   waitnext();

   bover(stdout);
   fover;
   i = 100000;
   ftextspeed(i, s);
   with benchtab[bntext] do { /* place stats */

      iter = i;
      time = s

   };
   pa_home(stdout);
   writeln("Text speed, with overwrite, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per write", s*0.0001/i);
   waitnext();

   binvis(stdout);
   fover;
   i = 100000;
   ftextspeed(i, s);
   with benchtab[bntextbi] do { /* place stats */

      iter = i;
      time = s

   };
   pa_home(stdout);
   bover(stdout);
   writeln("Text speed, invisible background, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per write", s*0.0001/i);
   waitnext();

   i = 1000;
   fpictspeed(i, s);
   with benchtab[bnpict] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("Picture draw speed, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per picture", s*0.0001/i);
   waitnext();

   i = 1000;
   fpictnsspeed(i, s);
   with benchtab[bnpictns] do { /* place stats */

      iter = i;
      time = s

   };
   writeln("No scale picture draw speed, ", i:1, " lines: ", s*0.0001,
           " seconds");
   writeln("Seconds per picture", s*0.0001/i);
   waitnext();

   /* stdout table */

   writeln(error);
   writeln(error, "Benchmark table");
   writeln(error);
   writeln(error, "Type                        Seconds     Per fig");
   writeln(error, "--------------------------------------------------");
   for bi = bnline1 to bnpictns do with benchtab[bi] do {

      case bi of /* benchmark type */

         bnline1:     write(error, "line width 1                ");
         bnline10:    write(error, "line width 10               ");
         bnrect1:     write(error, "rectangle width 1           ");
         bnrect10:    write(error, "rectangle width 10          ");
         bnrrect1:    write(error, "rounded rectangle width 1   ");
         bnrrect10:   write(error, "rounded rectangle width 10  ");
         bnfrect:     write(error, "filled rectangle            ");
         bnfrrect:    write(error, "filled rounded rectangle    ");
         bnellipse1:  write(error, "ellipse width 1             ");
         bnellipse10: write(error, "ellipse width 10            ");
         bnfellipse:  write(error, "filled ellipse              ");
         bnarc1:      write(error, "arc width 1                 ");
         bnarc10:     write(error, "arc width 10                ");
         bnfarc:      write(error, "filled arc                  ");
         bnfchord:    write(error, "filled chord                ");
         bnftriangle: write(error, "filled triangle             ");
         bntext:      write(error, "text                        ");
         bntextbi:    write(error, "background invisible text   ");
         bnpict:      write(error, "Picture draw                ");
         bnpictns:    write(error, "No scaling picture draw     ");

      };
      writere(error, time*0.0001, 10);
      write(error, "  ");
      writere(error, time*0.0001/iter, 10);
      writeln(error)

   };

   99: /* terminate */

   putchar('\f');
   auto(OFF);
   font(font_sign);
   fontsiz(50);
   prtceng(maxy / 2, "Test complete");

   return (0);

}
