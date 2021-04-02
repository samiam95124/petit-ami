/**//***************************************************************************

                                 Pixel dazzler

Continually xors a pixel onto the screen, eventually filling it with black, then
reversing to white, and cycles forever.

*******************************************************************************/

#include <setjmp.h>
#include <stdio.h>
#include <localdefs.h>
#include <graphics.h>

#define ACCEL 20

static int x, y, xd, yd, i;

static jmp_buf terminate_buf;

static void wait(void)

{

    pa_evtrec er; /* event record */

#if 0
    do { pa_event(stdin, &er); }
    while (er.etype != pa_etframe && er.etype != pa_etterm);
    if (er.etype == pa_etterm) { longjmp(terminate_buf, 1); }
#endif

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
