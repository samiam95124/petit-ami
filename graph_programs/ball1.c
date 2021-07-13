/********************************************************************************

Program to bounce animated ball around screen

********************************************************************************/

#include <stdio.h>
#include <localdefs.h>
#include <graphics.h>

#define BALLACCEL 5   /* ball acceleration */

int       x, y;
int       nx, ny;
int       xd, yd;
pa_evtrec er;
int       tc;
int       ballsize;
int       halfball;

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

int main(void)

{

    pa_curvis(stdout, FALSE); /* turn off cursor */
    ballsize = pa_maxyg(stdout)/10; /* set ball size */
    halfball = ballsize/2; /* set half ball size */
    x = halfball; /* set initial ball location */
    y = halfball;
    xd = +1; /* set movements */
    yd = +1;
    pa_frametimer(stdout, TRUE); /* start frame timer */
    while (TRUE) {

        /* place ball */
        pa_fcolor(stdout, pa_green);
        pa_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);
        if (chkbrk()) goto terminate; /* wait */
        /* erase ball */
        pa_fcolor(stdout, pa_white);
        pa_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);
        for (tc = 1; tc <= BALLACCEL; tc++) { /* move ball */

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
