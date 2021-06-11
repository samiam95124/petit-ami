/**//***************************************************************************

                                 Pixel dazzler

Continually xors a pixel onto the screen, eventually filling it with black, then
reversing to white, and cycles forever.

*******************************************************************************/

#include <setjmp.h>
#include <stdio.h>
#include <localdefs.h>
#include <graphics.h>

#define ACCEL 1000

static int x, y, xd, yd, i;

static jmp_buf terminate_buf;

static void wait(void)

{

    pa_evtrec er; /* event record */

    /* This is a dirty trick with PA. We set minimum time and check for
       user break because we want as much CPU time as possible to draw.
       A better solution would be to use another thread and set a flag for
       cancel. */
    pa_timer(stdout, 1, 1, FALSE);
    do { pa_event(stdin, &er); }
    while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) { longjmp(terminate_buf, 1); }

}

int main(void)

{

    if (setjmp(terminate_buf)) goto terminate;

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    pa_fcolor(stdout, pa_white);
    pa_fxor(stdout);
    x = 1;
    y = 1;
    xd = +1;
    yd = +1;
    while (TRUE) {

        for (i = 1; i <= ACCEL; i++) {

            pa_setpixel(stdout, x, y);
            x = x+xd;
            y = y+yd;
            if (x == 1 || x == pa_maxxg(stdout)) xd = -xd;
            if (y == 1 || y == pa_maxyg(stdout)) yd = -yd;

        }
        wait();

    }

terminate: /* terminate */

    pa_auto(stdout, TRUE);
    pa_curvis(stdout, TRUE);

}
