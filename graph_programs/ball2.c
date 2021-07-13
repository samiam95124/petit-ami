/********************************************************************************

Program to bounce animated ball around screen

Same as ball1.c, but this one draws to odd/even buffers and flips them to
demonstrate smooth animation.

********************************************************************************/

#include <stdio.h>
#include <localdefs.h>
#include <graphics.h>

#define BALLACCEL 5 /* ball acceleration */

int       x, y;
int       nx, ny;
int       lx, ly;
int       xd, yd;
pa_evtrec er;
int       tc;
int       cd; /* current display flip select */
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

void drawball(pa_color c, int x, int y)

{

    pa_fcolor(stdout, c); /* set color */
    pa_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);

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
    lx = x; /* set last position to same */
    ly = y;
    cd = FALSE; /* set 1st display */
    pa_frametimer(stdout, TRUE); /* set frame timer */
    drawball(pa_green, x, y); /* place ball at first position */
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
            if (nx < halfball || nx > pa_maxxg(stdout)-halfball+1) xd = -xd;
            if (ny < halfball || ny > pa_maxyg(stdout)-halfball+1) yd = -yd;
            x = x+xd; /* move ball */
            y = y+yd;

        }
        drawball(pa_green, x, y); /* place ball at new position */
        cd = !cd; /* flip display and update surfaces */
        if (chkbrk()) goto terminate; /* wait */

    }

    terminate:

    pa_curvis(stdout, TRUE); /* turn on cursor */

}
