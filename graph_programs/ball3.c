/********************************************************************************

Place random balls

********************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <graphics.h>

#define FRAMETIME 156          /* time between frames, 60 cycle refresh */

int ballsize;
int halfball;

int wait(void)

{

    pa_evtrec er;
    int cancel;

    cancel = FALSE;
    pa_timer(stdout, 1, FRAMETIME, FALSE);
    do { pa_event(stdin, &er); }
    while (er.etype != pa_ettim && er.etype == pa_etterm);
    if (er.etype == pa_etterm) cancel = TRUE;

    return (cancel);

}

void drawball(pa_color c, int  x, int y)

{

   pa_fcolor(stdout, c); /* set pa_color */
   pa_fellipse(stdout, x-halfball+1, y-halfball+1, x+halfball-1, y+halfball-1);

}
   
/* Find random number between 0 and N. */

static int randn(int limit)

{

    return (long)limit*rand()/RAND_MAX;

}

int main(void)

{

    pa_auto(stdout, FALSE);
    pa_curvis(stdout, FALSE);
    ballsize = pa_maxyg(stdout)/20; /* set ball size */
    halfball = ballsize/2; /* set half ball size */
    while (TRUE) {

        drawball(randn(pa_magenta+1-pa_red)+pa_red,
                 randn(pa_maxxg(stdout)-1)-ballsize+halfball+1,
                 randn(pa_maxyg(stdout)-1)-ballsize+halfball+1);
        if (wait()) goto terminate; /* wait for a frametime */

   }

   terminate:

   pa_auto(stdout, TRUE);
   pa_curvis(stdout, TRUE);

}
