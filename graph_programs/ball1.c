/********************************************************************************

Program to bounce animated ball around screen

********************************************************************************/

#include <stdio.h>
#include <localdefs.h>
#include <graph.h>

const int ballsize = 21;
const int halfball = ballsize/2;
const int frametime = 156; /* time between frames, 60 cycle refresh */
const int ballaccel = 5; /* ball acceleration */

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
    x = halfball; /* set inital ball location */
    y = halfball;
    xd = +1; /* set movements */
    yd = +1;
    while (TRUE) {

        /* place ball */
        pa_fcolor(stdout, pa_green);
        pa_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);
        if (wait(frametime)) goto terminate; /* wait */
        /* erase ball */
        pa_fcolor(stdout, pa_white);
        pa_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);
        for (tc = 1; tc <= ballaccel; tc++) { /* move ball */

            nx = x+xd; /* trial move ball */
            ny = y+yd;
            /* check out of bounds && reverse direction */
            if (nx < halfball || nx > pa_maxxg(stdout)-halfball+1) xd = -xd;
            if (ny < halfball || ny > pa_maxyg(stdout)-halfball+1) yd = -yd;
            x = x+xd; /* move ball */
            y = y+yd;

        }

    }

    terminate: /* terminate */

    pa_curvis(stdout, TRUE); /* turn on cursor */

}
