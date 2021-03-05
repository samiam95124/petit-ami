/********************************************************************************

Program to bounce animated ball around screen

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
int       xd, yd;
pa_evtrec er;
int       tc;

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

int main(void)

{

    pa_curvis(stdout, FALSE); /* turn off cursor */
    x = HALFBALL; /* set inital ball location */
    y = HALFBALL;
    xd = +1; /* set movements */
    yd = +1;
    while (TRUE) {

        /* place ball */
        pa_fcolor(stdout, pa_green);
        pa_fellipse(stdout, x-HALFBALL+1, y-HALFBALL+1, x+HALFBALL-1, y+HALFBALL-1);
        if (wait(FRAMETIME)) goto terminate; /* wait */
        /* erase ball */
        pa_fcolor(stdout, pa_white);
        pa_fellipse(stdout, x-HALFBALL+1, y-HALFBALL+1, x+HALFBALL-1, y+HALFBALL-1);
        for (tc = 1; tc <= BALLACCEL; tc++) { /* move ball */

            nx = x+xd; /* trial move ball */
            ny = y+yd;
            /* check out of bounds && reverse direction */
            if (nx < HALFBALL || nx > pa_maxxg(stdout)-HALFBALL+1) xd = -xd;
            if (ny < HALFBALL || ny > pa_maxyg(stdout)-HALFBALL+1) yd = -yd;
            x = x+xd; /* move ball */
            y = y+yd;

        }

    }

    terminate: /* terminate */

    pa_curvis(stdout, TRUE); /* turn on cursor */

}
