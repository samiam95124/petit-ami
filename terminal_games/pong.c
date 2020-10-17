/*******************************************************************************
*                                                                             *
*                                 PONG GAME                                   *
*                                                                             *
*                       COPYRIGHT (C) 1997 S. A. MOORE                        *
*                                                                             *
* Plays pong in text mode.                                                    *
*                                                                             *
*******************************************************************************/

#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <terminal.h>

#define MOVTIM 400    /* ball move time (1/25) sec */
#define NEWBAL (25*2) /* wait for new ball time, 1 sec (in ball move units) */

int       padx;   /* paddle position x */
int       ballx;  /* ball position x */
int       bally;  /* ball position y */
int       bdx;    /* ball direction x */
int       bdy;    /* ball direction y */
int       bsx;    /* ball position save x */
int       bsy;    /* ball position save y */
int       baltim; /* ball start timer */
pa_evtrec er;     /* event record */
int       jchr;   /* number of characters to joystick movement */
int       score;  /* score */

/*******************************************************************************

Wait time

Waits for the elapsed time, in 100 microseconds. Ignores other timers.

********************************************************************************/

void wait(int t)

{

    pa_evtrec er; /* event record */

    pa_timer(stdin, 2, t, FALSE); /* set timer */
    /* wait event */
    do {

        do { pa_event(stdin, &er); }
        while (er.etype != pa_ettim && er.etype != pa_etterm);
        if (er.etype == pa_etterm) {

            pa_curvis(stdout, TRUE); /* restore drawing cursor */
            pa_auto(stdout, TRUE); /* turn scrolling back on */
            pa_select(stdout, 1, 1); /* restore screen */
            exit(1); /* terminate */

        }

    } while (er.timnum != 2); /* this is our timer */

}

/*******************************************************************************

Write string to screen

Writes a sgtring to the indicated position on the screen.

********************************************************************************/

void writexy(int x, int y, /* position to write to */
             char* s)      /* char* to write */

{

   pa_cursor(stdout, x, y); /* position cursor */
   puts(s); /* output string */

}

/*******************************************************************************

Write centered string

Writes a string that is centered on the line given. Returns the
starting position of the string.

********************************************************************************/

void wrtcen(int   y,   /* y position of char* */
            char* s,   /* char* to write */
            int*  off) /* returns char* offset */

{

   *off = pa_maxx(stdout)/2-strlen(s)/2;
   writexy(*off, y, s); /* write out contents */

}

/*******************************************************************************

Draw screen

Draws a new screen, with borders.

********************************************************************************/

void drwscn(void)

{

    int x, y; /* screen indexes */

    putchar('\n'); /* clear screen */
    /* draw borders */
    for (x = 1; x <= pa_maxx(stdout); x++) writexy(x, 1, "*");
    for (x = 1; x <= pa_maxx(stdout); x++) writexy(x, pa_maxy(stdout), "*");
    for (y = 1; y <= pa_maxy(stdout); y++) writexy(1, y, "*");
    for (y = 1; y <= pa_maxy(stdout); y++) writexy(pa_maxx(stdout), y, "*");
    wrtcen(pa_maxy(stdout), " PONG VS. 1.0 ", &x);

}

/*******************************************************************************

Set new paddle position

Places the paddle at the given position.

********************************************************************************/

void padpos(int x)

{

    if (x < 5) x = 5; /* clip to ends */
    if (x > pa_maxx(stdout)-4) x = pa_maxx(stdout)-4;
    writexy(padx-3, pa_maxy(stdout)-1, "       "); /* blank paddle */
    padx = x; /* move right */
    writexy(padx-3, pa_maxy(stdout)-1, "=============="); /* place paddle */

}

void main(void)

{

    jchr = INT_MAX/((pa_maxx(stdout)-2) / 2); /* find basic joystick increment */
    pa_select(stdout, 2, 2); /* switch screens */
    pa_curvis(stdout, FALSE); /* remove drawing cursor */
    pa_auto(stdout, FALSE); /* turn off scrolling */
    pa_timer(stdout, 1, MOVTIM, TRUE); /* set movement timer */

    start: /* start new game */

    drwscn; /* draw game screen */
    padx = pa_maxx(stdout) / 2; /* find intial paddle position */
    writexy(padx-1, pa_maxy(stdout)-1, "=============="); /* place paddle */
    ballx = 0; /* set ball ! on screen */
    bally = 0;
    baltim = 0; /* set ball ready to start */
    do { /* game loop */

        if (!ballx && !baltim) {

            /* ball ! on screen, && time to wait expired, s} out ball */
            ballx = 2; /* place ball */
            bally = pa_maxy(stdout)-3;
            bdx = +1; /* set direction of travel */
            bdy = -1;
            writexy(ballx, bally, "*"); /* draw the ball */
            score = 0; /* clear score */

        }
        /* place updated score on screen */
        pa_cursor(stdout, pa_maxx(stdout) / 2-11 / 2, 1);
        printf("SCORE %5d\n", score);
        do { pa_event(stdin, &er); /* wait relivant events */
        } while (er.etype != pa_etterm && er.etype != pa_etleft &&
                 er.etype != pa_etright && er.etype != pa_etfun &&
                 er.etype != pa_ettim && er.etype != pa_etjoymov);
        if (er.etype == pa_etterm) goto exit; /* game exits */
        if (er.etype == pa_etfun) goto start; /* restart game */
        /* process paddle movements */
        if (er.etype == pa_etleft) padpos(padx-1); /* move left */
        else if (er.etype == pa_etright) padpos(padx+1); /* move right */
        else if (er.etype == pa_etjoymov) /* move joystick */
            padpos(pa_maxx(stdout)/2+er.joypx/jchr);
        else if (er.etype == pa_ettim) { /* move timer */

            if (er.timnum == 1) { /* ball timer */

                if (ballx > 0) { /* ball on screen */

                    writexy(ballx, bally, " "); /* erase the ball */
                    bsx = ballx; /* save ball position */
                    bsy = bally;
                    ballx = ballx+bdx; /* move the ball */
                    bally = bally+bdy;
                    /* check off screen motions */
                    if (ballx == 1 || ballx == pa_maxx(stdout)) {

                        ballx = bsx; /* restore */
                        bdx = -bdx; /* change direction */
                        ballx = ballx+bdx; /* recalculate */

                    }
                    if (bally == 1) { /* hits top */

                        bally = bsy; /* restore */
                        bdy = -bdy; /* change direction */
                        bally = bally+bdy; /* recalculate */

                    } else if (bally == pa_maxy(stdout)-1 &&
                               ballx >= padx-3 &&
                               ballx <= padx+3) {

                        /* hits paddle */
                        bally = bsy; /* restore */
                        bdy = -bdy; /* change direction */
                        bally = bally+bdy; /* recalculate */
                        score = score+1; /* count hits */

                    }
                    if (bally != pa_maxy(stdout))
                        writexy(ballx, bally, "*"); /* redraw the ball */

                }
                /* if the ball timer is running, decrement it */
                if (baltim > 0) baltim--;

            }

        }
        if (bally == pa_maxy(stdout)) { /* ball out of bounds */

            ballx = 0; /* set ball ! on screen */
            bally = 0;
            baltim = NEWBAL; /* start time on new ball wait */

        }

    } while (TRUE); /* forever */

    /* exit game */
    exit:
    pa_curvis(stdout, TRUE); /* restore drawing cursor */
    pa_auto(stdout, TRUE); /* turn scrolling back on */
    pa_select(stdout, 1, 1); /* restore screen */

}
