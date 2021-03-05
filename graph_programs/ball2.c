/********************************************************************************

Program to bounce animated ball around screen

Same as ball1.c, but this one draws to odd/even buffers and flips them to
demonstrate smooth animation.

********************************************************************************/

#include <stdio.h>
#include <localdefs.h>
#include <graph.h>

#define BALLSIZE  21
#define HALFBALL  (BALLSIZE/2)
#define FRAMETIME 156          /* time between frames, 60 cycle refresh */
#define BALLACCEL 5            /* ball acceleration */

int       x, y;
int       nx, ny;
int       lx, ly;
int       xd, yd;
pa_evtrec er;
int       tc;
int       cd; /* current display flip select */


/* Wait time in 100 microseconds. Returns true if terminate. */

int wait(int t)

{

    int cancel;

    cancel = FALSE;
    pa_timer(stdout, 1, t, FALSE);
    do { pa_event(stdin, &er); }
    while (er.etype != pa_ettim && er.etype == pa_etterm);
    if (er.etype == pa_etterm) cancel = TRUE;

    return (cancel);

}

void drawball(pa_color c, int x, int y)

{

    pa_fcolor(stdout, c); /* set color */
    pa_fellipse(stdout, x-HALFBALL+1, y-HALFBALL+1, x+HALFBALL-1, y+HALFBALL-1);

}

int main(void)

{

    pa_curvis(stdout, FALSE); /* turn off cursor */
    x = HALFBALL; /* set inital ball location */
    y = HALFBALL;
    xd = +1; /* set movements */
    yd = +1;
    lx = x; /* set last position to same */
    ly = y;
    cd = FALSE; /* set 1st display */
    drawball(pa_green, x, y); /* place ball at fisrt position */
    while (TRUE) {

        /* select display and update surfaces */
        pa_select(stdout, !cd+1, cd+1);
        drawball(pa_white, lx, ly); /* erase ball at old position */
        lx = x; /* save last position */
        ly = y;
        for (tc = 1; tc <= BALLACCEL; tc++) { /* move ball */

            nx = x+xd; /* trial move ball */
            ny = y+yd;
            /* check out of bounds and reverse direction */
            if (nx < HALFBALL || nx > pa_maxxg(stdout)-HALFBALL+1) xd = -xd;
            if (ny < HALFBALL || ny > pa_maxyg(stdout)-HALFBALL+1) yd = -yd;
            x = x+xd; /* move ball */
            y = y+yd;

        }
        drawball(pa_green, x, y); /* place ball at new position */
        cd = !cd; /* flip display and update surfaces */
        if (wait(FRAMETIME)) goto terminate; /* wait */

    }

    terminate:

    pa_curvis(stdout, TRUE); /* turn on cursor */

}
