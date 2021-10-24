/********************************************************************************

Program to draw random lines on screen

********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <localdefs.h>
#include <graphics.h>

#define MAXLAG (200)

#define FRAMETIME   156 /* time between frames, 60 cycle refresh */
#define ACCEL       5
#define COLORCHANGE 300

typedef struct {

    int x1, y1, x2, y2;

} linrec;

static int      x1, y1, xd1, yd1, i, x2, y2, xd2, yd2, lx1, ly1, lx2, ly2;
static int      cc; /* color counter */
static pa_color clr;
static linrec   last[MAXLAG];
static int      li;

static int waitframe(void)

{

    pa_evtrec er; /* event record */
    int cancel;

    cancel = FALSE;
    pa_timer(stdout, 1, FRAMETIME, FALSE);
    do { pa_event(stdin, &er); }
    while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) cancel = TRUE;

    return (cancel);

}

/*******************************************************************************

Find random number

Find random number between 0 and N.

*******************************************************************************/

static int randn(int limit)

{

    return (long)limit*rand()/RAND_MAX;

}

static void putlast(void)

{

    int i;

    for (i = 0; i < MAXLAG-1; i++) last[i] = last[i+1];
    last[MAXLAG-1].x1 = x1;
    last[MAXLAG-1].y1 = y1;
    last[MAXLAG-1].x2 = x2;
    last[MAXLAG-1].y2 = y2;

}

int main(void)

{

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    x1 = pa_maxxg(stdout) / 4+10;
    y1 = 1;
    xd1 = -1;
    yd1 = +1;
    x2 = pa_maxxg(stdout)-(pa_maxxg(stdout) / 4);
    y2 = pa_maxyg(stdout);
    xd2 = -1;
    yd2 = -1;
    cc = 1;
    clr = randn(pa_magenta+1-pa_red)+pa_red;
    for (li = 0; li < MAXLAG; li++) last[li].x1 = 0;
    while (TRUE) {

        for (i = 1; i <= ACCEL; i++) {

            pa_fcolor(stdout, pa_white);
            if (last[0].x1 > 0)
                pa_line(stdout, last[0].x1, last[0].y1, last[0].x2, last[0].y2);
            putlast();
            x1 = x1+xd1;
            y1 = y1+yd1;
            if (x1 == 1 || x1 == pa_maxxg(stdout)) xd1 = -xd1;
            if (y1 == 1 || y1 == pa_maxyg(stdout)) yd1 = -yd1;
            x2 = x2+xd2;
            y2 = y2+yd2;
            if (x2 == 1 || x2 == pa_maxxg(stdout)) xd2 = -xd2;
            if (y2 == 1 || y2 == pa_maxyg(stdout)) yd2 = -yd2;
            pa_fcolor(stdout, clr);
            pa_line(stdout, x1, y1, x2, y2);
            cc = cc+1;
            if (cc >= COLORCHANGE) {

                cc = 1;
                clr = randn(pa_magenta+1-pa_red)+pa_red;

            }

        }
        if (waitframe()) goto terminate;

    }

    terminate:

    pa_auto(stdout, TRUE);
    pa_curvis(stdout, TRUE);

}
