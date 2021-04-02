/********************************************************************************

Program to draw random lines on screen

********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <localdefs.h>
#include <graphics.h>

#define FRAMETIME 156 /* time between frames, 60 cycle refresh */

static int wait(void)

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

int main(void)

{

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    pa_linewidth(stdout, 5);
    while (TRUE) {

        pa_fcolor(stdout, randn(pa_magenta+1-pa_red)+pa_red);
        pa_line(stdout, randn(pa_maxxg(stdout)-1)+1, randn(pa_maxyg(stdout)-1)+1,
                        randn(pa_maxxg(stdout)-1)+1, randn(pa_maxyg(stdout)-1)+1);
        if (wait()) goto terminate;

    }

    terminate:

    pa_auto(stdout, TRUE);
    pa_curvis(stdout, TRUE);

}
