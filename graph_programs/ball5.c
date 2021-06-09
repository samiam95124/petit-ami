/********************************************************************************

Program to bounce animated balls around screen

********************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <graphics.h>

#define MAXBALL   10
#define REPRATE   5   /* number of moves per frame, should be low */

typedef struct balrec { /* ball data record */

    int      x, y;   /* current position */
    int      lx, ly; /* last position */
    int      xd, yd; /* deltas */
    pa_color c;      /* pa_color */
   
} balrec;
   
int    cd;              /* current display flip select */
balrec baltbl[MAXBALL]; /* ball data table */
int    i;               /* index for table */
int    nx, ny;          /* temp coordinates holders */
int    rc;              /* repetition counter */
int    ballsize;        /* size of ball onscreen */
int    halfball;        /* half size of ball */

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
   
/* Find random number between 0 and N. */

static int randn(int limit)

{

    return ((long)limit+1)*rand()/RAND_MAX;

}

int main(void)

{

    ballsize = pa_maxyg(stdout)/5; /* set ball size */
    halfball = ballsize/2; /* set half ball size */
    /* initialize ball data */
    for (i = 0; i < MAXBALL; i++) {

        baltbl[i].x = randn(pa_maxxg(stdout)-1-ballsize)+halfball+1;
        baltbl[i].y = randn(pa_maxyg(stdout)-1-ballsize)+halfball+1;
        if (randn(1)) baltbl[i].xd = +1; else baltbl[i].xd = -1;
        if (randn(1)) baltbl[i].yd = +1; else baltbl[i].yd = -1;
        baltbl[i].lx = baltbl[i].x; /* set last position to same */
        baltbl[i].ly = baltbl[i].y;
        /* set random pa_color */
        baltbl[i].c = randn(pa_magenta-pa_red)+pa_red;

    }
    pa_curvis(stdout, FALSE); /* turn off cursor */
    cd = FALSE; /* set 1st display */
    rc = 0; /* count reps */
        /* start frame timer for 60 cycle refresh */
    pa_frametimer(stdout, TRUE);
    while (TRUE) {

        /* select display and update surfaces */
        pa_select(stdout, !cd+1, cd+1);
        pa_fover(stdout); /* set overwrite */
        /* erase balls at old positions */
        for (i = 0; i < MAXBALL; i++)
            drawball(pa_white, baltbl[i].lx, baltbl[i].ly);
        pa_fxor(stdout); /* set xor mode */
        for (i = 0; i < MAXBALL; i++) { /* process balls */

            baltbl[i].lx = baltbl[i].x; /* save last position */
            baltbl[i].ly = baltbl[i].y;
            nx = baltbl[i].x+baltbl[i].xd; /* trial move ball */
            ny = baltbl[i].y+baltbl[i].yd;
            /* check out of bounds and reverse direction */
            if (nx < halfball || nx > pa_maxxg(stdout)-halfball+1)
                baltbl[i].xd = -baltbl[i].xd;
            if (ny < halfball || ny > pa_maxyg(stdout)-halfball+1)
                baltbl[i].yd = -baltbl[i].yd;
            baltbl[i].x = baltbl[i].x+baltbl[i].xd; /* move ball */
            baltbl[i].y = baltbl[i].y+baltbl[i].yd;
            /* place ball at new position */
            drawball(baltbl[i].c, baltbl[i].x, baltbl[i].y);

        }
        cd = !cd; /* flip display and update surfaces */
        rc = rc+1; /* count reps */
        if (rc >= REPRATE) {

            if (chkbrk()) goto terminate; /* check complete */
            rc = 0; /* clear rep counter */

        }

    }

    terminate:

    pa_curvis(stdout, TRUE);

}
