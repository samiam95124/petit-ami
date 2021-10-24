/********************************************************************************

Program to bounce animated balls around screen

********************************************************************************/

/* posix definitions */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

/* Petit-Ami definitions */
#include <localdefs.h>
#include <graphics.h>
#include <sound.h>

#define MAXBALL   10
#define REPRATE   5   /* number of moves per frame, should be low */

#define WAVSTR 90 /* starting noise wave time */
#define WAVCNT 10 /* number of frames to wait for wave output */

typedef struct balrec { /* ball data record */

    int      x, y;   /* current position */
    int      lx, ly; /* last position */
    int      xd, yd; /* deltas */
    pa_color c;      /* pa_color */

} balrec;

int      cd;              /* current display flip select */
balrec   baltbl[MAXBALL]; /* ball data table */
int      i;               /* index for table */
int      nx, ny;          /* temp coordinates holders */
int      rc;              /* repetition counter */
int      ballsize;        /* size of ball onscreen */
int      halfball;        /* half size of ball */
pa_color cc;              /* pa_color assignment counter */
pa_note  n;               /* note variable */
int      bounce;          /* a bounce took place */
int      wavtim;          /* wave output timer */

int chkbrk(void)

{

    pa_evtrec er;
    int cancel;

    cancel = FALSE;
    do { pa_event(stdin, &er); }
    while (er.etype != pa_etframe && er.etype != pa_etterm);
    if (er.etype == pa_etterm) cancel = TRUE;

    return (cancel);

}

/* Find random number between 0 and N. */

static int randn(int limit)

{

    return ((long)limit+1)*rand()/RAND_MAX;

}

/********************************************************************************

Draw centered ball

Draws a ball with the given center and size. If the size is not odd, it is
rounded up a pixel.

********************************************************************************/

void drawball(int x, int y, int s)

{

    int hs;

    hs = s / 2;
    pa_fellipse(stdout, x-hs, y-hs, x+hs, y+hs);

}

/********************************************************************************

Draw shaded ball

Draws a shaded ball with highlighting from upper left lighting. The center
and size of the ball is specified. The offset of the highlight is expressed
as a percentage from the center to edge of the ball, and the number of shading
steps is specified. The color is specified as RGB.

Note that the more steps specified, the more drawing time, so only as many steps
as needed should be used. Steps will be more apparent on larger balls.

********************************************************************************/

/* subtract from level without allowing negative */

int level(int c, int steps, int shad, int i)

{

    c = c-(steps-i)*shad;
    if (c < 0) c = 0;

    return (c);

}

void drawsball(int x, int y, int size, int o, int steps, int r, int g, int b)

{

    int   i;
    int   k, q;
    float offs;
    int   shad;

    offs = o*(size/2)/100; /* find offset from percentage */
    shad = INT_MAX/2/steps; /* find shading steps */
    for (i = 1; i <= steps; i++) {

        pa_fcolorg(stdout, level(r, steps, shad, i), level(g, steps, shad, i),
                   level(b, steps, shad, i));
        k = round((i-1)*((float)size/steps));
        q = round((i-1)*(offs/steps));
        drawball(x-q, y-q, size-k);

    }

}

int redv(pa_color c)

{

    int cv;

    if (c == pa_red || c == pa_magenta || c == pa_yellow) cv = INT_MAX;
    else cv = 0;

    return (cv);

}

int greenv(pa_color c)

{

    int cv;

    if (c == pa_green || c == pa_yellow || c == pa_cyan) cv = INT_MAX;
    else cv = 0;

    return (cv);

}

int bluev(pa_color c)

{

    int cv;

    if (c == pa_blue || c == pa_cyan || c == pa_magenta) cv = INT_MAX;
    else cv = 0;

    return (cv);

}

void movbal(int b)

{

    int nx, ny; /* temp coordinates holders */

    nx = baltbl[b].x+baltbl[b].xd; /* trial move ball */
    ny = baltbl[b].y+baltbl[b].yd;
    /* check out of bounds and reverse direction */
    if (nx < halfball || nx > pa_maxxg(stdout)-halfball+1) {

       bounce = TRUE; /* set bounce occurred */
       baltbl[b].xd = -baltbl[b].xd;

    }
    if (ny < halfball || ny > pa_maxyg(stdout)-halfball+1) {

       bounce = TRUE; /* set bounce occurred */
       baltbl[b].yd = -baltbl[b].yd;

    }
    baltbl[b].x += baltbl[b].xd; /* move ball */
    baltbl[b].y += baltbl[b].yd;

}

int main(void)

{

    pa_openwaveout(1); /* open main wave output */
    /* load wave files to use */
    pa_loadwave(1, "graph_programs/car_rev");
    pa_loadwave(2, "graph_programs/pong");
    pa_playwave(1, 0, 1);
    wavtim = WAVSTR; /* place starting wave time (60 seconds) */
    ballsize = pa_maxyg(stdout)/10; /* set ball size */
    halfball = ballsize/2; /* set half ball size */
    /* initialize ball data */
    cc = pa_red; /* start colors */
    for (i = 0; i < MAXBALL; i++) {

        baltbl[i].x = randn(pa_maxxg(stdout)-1-ballsize)+halfball+1;
        baltbl[i].y = randn(pa_maxyg(stdout)-1-ballsize)+halfball+1;
        if (randn(1)) baltbl[i].xd = +1; else baltbl[i].xd = -1;
        if (randn(1)) baltbl[i].yd = +1; else baltbl[i].yd = -1;
        baltbl[i].lx = baltbl[i].x; /* set last position to same */
        baltbl[i].ly = baltbl[i].y;
        baltbl[i].c = cc; /* set pa_color */
        if (cc < pa_magenta) cc++; else cc = pa_red; /* next pa_color */

    }
    pa_curvis(stdout, FALSE); /* turn off cursor */
    cd = FALSE; /* set 1st display */
    rc = 0; /* count reps */
    bounce = FALSE; /* set no bounce */
    pa_frametimer(stdout, TRUE); /* turn on the framing timer */
    while (TRUE) {

        /* select display and update surfaces */
        pa_select(stdout, !cd+1, cd+1);
        /* erase old balls */
        pa_fcolor(stdout, pa_white);
        for (i = 0; i < MAXBALL; i++)
            drawball(baltbl[i].lx, baltbl[i].ly, ballsize);
        pa_fcolor(stdout, pa_black);
        /* save last position */
        for (i = 0; i < MAXBALL; i++) {

            baltbl[i].lx = baltbl[i].x; /* save last position */
            baltbl[i].ly = baltbl[i].y;

        }
        /* move balls */
        for (rc = 0; rc < REPRATE; rc++) /* repeat per frame */
            for (i = 0; i < MAXBALL; i++) movbal(i);
        /* draw new balls */
        for (i = 0; i < MAXBALL; i++)
            drawsball(baltbl[i].x, baltbl[i].y, ballsize, 30, 30,
                      redv(baltbl[i].c), greenv(baltbl[i].c),
                      bluev(baltbl[i].c));
        cd = !cd; /* flip display and update surfaces */
        if (chkbrk()) goto terminate; /* wait frame and check for break */
        if (bounce && wavtim == 0) { /* a bounce occurred in cycle */

            pa_playwave(1, 0, 2); /* start pong sound */
            wavtim = WAVCNT; /* start timer */

        }
        bounce = FALSE; /* set no bounce */
        if (wavtim) wavtim--; /* count down wave timer */

    }

    terminate:

    pa_curvis(stdout, TRUE);

}
