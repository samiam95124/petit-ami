/*******************************************************************************
*                                                                              *
*                                BREAKOUT GAME                                 *
*                                                                              *
*                       COPYRIGHT (C) 2002 S. A. FRANCO                        *
*                                                                              *
* Plays breakout in graphical mode.                                            *
*                                                                              *
*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <sound.h>
#include <graph.h>

#define   SECOND      10000                   /* one second */
#define   OSEC        (SECOND/8)              /* 1/8 second */
#define   BALMOV      50                      /* ball move timer */
#define   NEWBAL      SECOND                  /* wait for new ball time */
#define   WALL        21                      /* WALL thickness */
#define   HWALL       (WALL/2)                /* half WALL thickness */
#define   PADW        81                      /* width of paddle */
#define   PADHW       (PADW/2)                /* half paddle */
#define   PADQW       (PADW/4)                /* quarter paddle */
#define   PADH        15                      /* height of paddle */
#define   HPADW       (PADW/2)                /* half paddle width */
#define   PWDIS       5                       /* distance of paddle from bottom
                                                 wall */
#define   BALLS       21                      /* size of the ball */
#define   hBALLS      (BALLS/2)               /* half ball size */
#define   BALLCLR     pa_blue                 /* ball pa_color */
#define   WALLCLR     pa_cyan                 /* wall pa_color */
#define   PADCLR      pa_green                /* paddle pa_color */
#define   BOUNCETIME  250                     /* time to play bounce note */
#define   WALLNOTE    (PA_NOTE_D+PA_OCTAVE_6) /* note to play off wall */
#define   BRICKNOTE   (PA_NOTE_E+PA_OCTAVE_7) /* note to play off brick */
#define   FAILTIME    1500                    /* note to play on failure */
#define   FAILNOTE    (PA_NOTE_C+PA_OCTAVE_4) /* note to play on fail */
#define   BRKROW      6                       /* number of brick rows */
#define   BRKCOL      10                      /* number of brick columns */
#define   BRKH        15                      /* brick height */
#define   BRKBRD      3                       /* brick border */

typedef struct { /* rectangle */

    int x1, y1, x2, y2;

} rectangle;

int       padx;                       /* paddle position x */
int       bdx;                        /* ball direction x */
int       bdy;                        /* ball direction y */
int       bsx;                        /* ball position save x */
int       bsy;                        /* ball position save y */
int       baltim;                     /* ball start timer */
pa_evtrec er;                         /* event record */
int       jchr;                       /* number of pixels to joystick
                                         movement */
int       score;                      /* score */
int       scrsiz;                     /* score size */
int       scrchg;                     /* score has changed */
int       bac;                        /* ball accelerator */
rectangle paddle;                     /* paddle rectangle */
rectangle ball, balsav;               /* ball rectangle */
rectangle wallt, walll, wallr, wallb; /* WALL rectangles */
rectangle bricks[BRKROW][BRKCOL];     /* brick array */
int       brki;                       /* brick was intersected */
int       fldbrk;                     /* bricks hit this field */
int       bip;                        /* middle of ball intersection with
                                         paddle */

/*******************************************************************************

Write string to screen

Writes a string to the indicated position on the screen.

********************************************************************************/

void writexy(int x, int y,   /* position to write to */
             const string s) /* char* to write */

{

    pa_cursorg(stdout, x, y); /* position cursor */
    puts(s); /* output string */

}

/*******************************************************************************

Write centered string

Writes a string that is centered on the line given. Returns the
starting position of the string.

********************************************************************************/

void wrtcen(int          y, /* y position of string */
            const string s) /* string to write */

{

    int off; /* string offset */

    off = pa_maxxg(stdout)/2-pa_strsiz(stdout, s)/2;
    writexy(off, y, s); /* write out contents */

}

/*******************************************************************************

Translate color code

Translates a logical color to an RGB color. Returns the RGB color in three
variables.

*******************************************************************************/

void log2rgb(pa_color c, int* r, int* g, int* b)

{

    /* translate color number */
    switch (c) { /* color */

        case pa_black:   *r = 0;       *g= 0;        *b = 0;       break;
        case pa_white:   *r = INT_MAX; *g = INT_MAX; *b = INT_MAX; break;
        case pa_red:     *r = INT_MAX; *g = 0;       *b = 0;       break;
        case pa_green:   *r = 0;       *g = INT_MAX; *b = 0;       break;
        case pa_blue:    *r = 0;       *g = 0;       *b = INT_MAX; break;
        case pa_cyan:    *r = 0;       *g = INT_MAX; *b = INT_MAX; break;
        case pa_yellow:  *r = INT_MAX; *g = INT_MAX; *b = 0;       break;
        case pa_magenta: *r = INT_MAX; *g = 0;       *b = INT_MAX; break;

    }

}

/*******************************************************************************

Draw rectangle

Draws a filled rectangle, in the given color.

********************************************************************************/

void drwrect(rectangle* r, pa_color c)

{

    pa_fcolor(stdout, c); /* set color */
    pa_frect(stdout, r->x1, r->y1, r->x2, r->y2);

}

/*******************************************************************************

Draw bordered rectangle

Draws a filled rectangle with border, in the given color.

********************************************************************************/

void dim(float dv, int* r, int* g, int* b)

{

    *r = trunc(*r*dv);
    *g = trunc(*g*dv);
    *b = trunc(*b*dv);

}

void drwbrect(rectangle* r, pa_color c)

{

    int i;
    int hr, hg, hb; /* rgb value of highlight */
    int mr, mg, mb; /* rbg value of midlight */
    int lr, lg, lb; /* rbg value of lowlight */

    log2rgb(c, &hr, &hg, &hb); /* find actual pa_color */
    mr = hr; /* copy */
    mg = hg;
    mb = hb;
    lr = hr;
    lg = hg;
    lb = hb;
    dim(0.80, &mr, &mg, &mb); /* dim midlight to %75 */
    dim(0.60, &lr, &lg, &lb); /* dim lowlight to %50 */
    pa_fcolorg(stdout, mr, mg, mb); /* set brick body to midlight */
    pa_frect(stdout, r->x1, r->y1, r->x2, r->y2); /* draw brick */
    pa_fcolorg(stdout, hr, hg, hb); /* set hilight */
    pa_frect(stdout, r->x1, r->y1, r->x1+BRKBRD-1, r->y2); /* border left */
    pa_frect(stdout, r->x1, r->y1, r->x2, r->y1+BRKBRD-1); /* top */
    /* set lowlight border color */
    pa_fcolorg(stdout, lr, lg, lb);
    /* border right */
    for (i = 1; i <= BRKBRD; i++)
        pa_frect(stdout, r->x2-i+1, r->y1+i-1, r->x2, r->y2);
    /* border bottom */
    for (i = 1; i <= BRKBRD; i++)
        pa_frect(stdout, r->x1+i-1, r->y2-i+1, r->x2, r->y2);

}

/*******************************************************************************

Offset rectangle

Offsets a rectangle by an x and y difference.

********************************************************************************/

void offrect(rectangle* r, int x, int y)

{

    r->x1 = r->x1+x;
    r->y1 = r->y1+y;
    r->x2 = r->x2+x;
    r->y2 = r->y2+y;

}

/*******************************************************************************

Rationalize a rectangle

Rationalizes a rectangle, that is, arranges the points so that the 1st point
is lower in x and y than the second.

********************************************************************************/

void ratrect(rectangle* r)

{

    int t; /* swap temp */

    if (r->x1 > r->x2) { /* swap x */

        t = r->x1;
        r->x1 = r->x2;
        r->x2 = t;

    }
    if (r->y1 > r->y2) { /* swap y */

        t = r->y1;
        r->y1 = r->y2;
        r->y2 = t;

    }

}

/*******************************************************************************

Find intersection of rectangles

Checks if two rectangles intersect. Returns true if so.

********************************************************************************/

int intersect(rectangle* r1, rectangle* r2)

{

    /* rationalize the rectangles */
    ratrect(r1);
    ratrect(r2);

    return ((*r1).x2 >= (*r2).x1 && (*r1).x1 <= (*r2).x2 &&
            (*r1).y2 >= (*r2).y1 && (*r1).y1 <= (*r2).y2);

}

/*******************************************************************************

Set rectangle

Sets the rectangle to the given values.

********************************************************************************/

void setrect(rectangle* r, int x1, int y1, int x2, int y2)

{

    r->x1 = x1;
    r->y1 = y1;
    r->x2 = x2;
    r->y2 = y2;

}

/*******************************************************************************

Clear rectangle

Clear rectangle points to zero. Usually used to flag the rectangle invalid.

*******************************************************************************/

void clrrect(rectangle* r)

{

    r->x1 = 0;
    r->y1 = 0;
    r->x2 = 0;
    r->y2 = 0;

}

/*******************************************************************************

Draw screen

Draws a new screen, with borders.

********************************************************************************/

void drwscn(void)

{

    putchar('\n'); /* clear screen */
    /* draw walls */
    drwrect(&wallt, WALLCLR); /* top */
    drwrect(&walll, WALLCLR); /* left */
    drwrect(&wallr, WALLCLR); /* right */
    drwrect(&wallb, WALLCLR); /* bottom */
    pa_fcolor(stdout, pa_black);
    wrtcen(pa_maxyg(stdout)-WALL+1, "BREAKOUT VS. 1.0");

}

/*******************************************************************************

Draw wall

Redraws the brick wall.

********************************************************************************/

void drwwall(void)

{

    int      r, c; /* brick array indexes */
    pa_color clr;  /* brick color */

    clr = pa_red; /* set 1st pure color */
    for (r = 0; r < BRKROW; r++)
        for (c = 0; c < BRKCOL; c++) {

        drwbrect(&bricks[r][c], clr);
        if (clr < pa_magenta) clr++;
        else clr = pa_red;

    }

}

/*******************************************************************************

Set new paddle position

Places the paddle at the given position.

********************************************************************************/

void padpos(int x)

{

    if (x-HPADW <= walll.x2) x = walll.x2+HPADW+1; /* clip to ends */
    if (x+HPADW >= wallr.x1) x = wallr.x1-HPADW-1;
    /* erase old location */
    pa_fcolor(stdout, pa_white);
    pa_frect(stdout, padx-HPADW, pa_maxyg(stdout)-WALL-PADH-PWDIS,
                     padx+HPADW, pa_maxyg(stdout)-WALL-PWDIS);
    padx = x; /* set new location */
    setrect(&paddle, x-HPADW, pa_maxyg(stdout)-WALL-PADH-PWDIS,
                    x+HPADW, pa_maxyg(stdout)-WALL-PWDIS);
    drwrect(&paddle, PADCLR); /* draw paddle */

}

/*******************************************************************************

Set brick WALL

Initializes the bricks in the wall coordinates.

********************************************************************************/

void setwall(void)

{

    int r, c;   /* brick array indexes */
    int brkw;   /* brick width */
    int brkr;   /* brick remainder */
    int brkoff; /* brick WALL offset */
    int co;     /* collumn offset */
    int rd;     /* remainder distributor */

    brkw = (pa_maxxg(stdout)-2*WALL)/BRKCOL; /* find brick width */
    brkr = (pa_maxxg(stdout)-2*WALL)%BRKCOL-1; /* find brick remainder */
    brkoff = pa_maxyg(stdout)/4; /* find brick wall offset */
    for (r = 0; r < BRKROW; r++) {

        co = 0; /* clear collumn offset */
        rd = brkr; /* set remainder distributor */
        for (c = 0; c < BRKCOL; c++) {

            setrect(&bricks[r][c], 1+co+WALL, 1+(r-1)*BRKH+brkoff,
                                              1+co+brkw-1+WALL+(rd > 0),
                                              1+(r-1)*BRKH+BRKH-1+brkoff);
            co = co+brkw+(rd > 0); /* offset to next brick */
            if (brkr > 0) rd = rd-1; /* pa_reduce remainder */

        }

    }

}

/********************************************************************************

Find brick intersection

Searches for a brick that intersects with the ball, and if found, erases the
brick and returns true. Note that if more than one brick intersects, they all
disappear.

********************************************************************************/

void interbrick(void)

{

    int r, c; /* brick array indexes */

    brki = FALSE; /* set no brick intersection */
    for (r = 0; r < BRKROW; r++)
        for (c = 0; c < BRKCOL; c++) if (intersect(&ball, &bricks[r][c])) {

        brki = TRUE; /* set intersected */
        drwrect(&bricks[r][c], pa_white); /* erase from screen */
        clrrect(&bricks[r][c]); /* clear brick data */
        score++; /* count hits */
        scrchg = TRUE; /* set changed */
        fldbrk++; /* add to bricks this field */

    }

}

int main(void)

{

    pa_opensynthout(PA_SYNTH_OUT); /* open synthesizer */
    pa_instchange(PA_SYNTH_OUT, 0, 1, PA_INST_LEAD_1_SQUARE);
    pa_starttimeout(); /* start sequencer running */
    jchr = INT_MAX/((pa_maxxg(stdout)-2)/2); /* find basic joystick increment */
    pa_curvis(stdout, FALSE); /* remove drawing cursor */
    pa_auto(stdout, FALSE); /* turn off scrolling */
    pa_font(stdout, pa_signfont(stdout)); /* sign font */
    pa_bold(stdout, TRUE);
    pa_fontsiz(stdout, WALL-2); /* font fits in the WALL */
    pa_binvis(stdout); /* no background writes */
    pa_timer(stdout, 1, BALMOV, TRUE); /* enable timer */

    newgame: /* start new game */

    padx = pa_maxxg(stdout)/2; /* find initial paddle position */
    padpos(padx); /* display paddle */
    clrrect(&ball); /* set ball not on screen */
    baltim = 0; /* set ball ready to start */
    /* set up wall rectangles */
    setrect(&wallt, 1, 1, pa_maxxg(stdout), WALL); /* top */
    setrect(&walll, 1, 1, WALL, pa_maxyg(stdout)); /* left */
    /* right */
    setrect(&wallr, pa_maxxg(stdout)-WALL, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    /* bottom */
    setrect(&wallb, 1, pa_maxyg(stdout)-WALL, pa_maxxg(stdout), pa_maxyg(stdout));
    scrsiz = pa_strsiz(stdout, "SCORE 0000"); /* set nominal size of score string */
    scrchg = TRUE; /* set score changed */
    drwscn(); /* draw game screen */
    score = 0; /* clear score */
    baltim = NEWBAL/BALMOV; /* set starting ball time */
    do { /* game loop */

        setwall(); /* initialize bricks */
        drwwall(); /* redraw the wall */
        fldbrk = 0; /* clear bricks hit this field */
        do { /* fields */

            if (ball.x1 == 0 && baltim == 0) {

                /* ball not on screen, and time to wait expired, send out ball */
                setrect(&ball, WALL+1, pa_maxyg(stdout)-4*WALL-BALLS,
                               WALL+1+BALLS, pa_maxyg(stdout)-4*WALL);
                bdx = +1; /* set direction of travel */
                bdy = -2;
                /* draw the ball */
                pa_fcolor(stdout, BALLCLR);
                drwrect(&ball, BALLCLR);
                scrchg = TRUE; /* set changed */

            }
            if (scrchg) { /* process score change */

                /* erase score */
                pa_fcolor(stdout, WALLCLR);
                pa_frect(stdout, pa_maxxg(stdout)/2-scrsiz/2, 1,
                                 pa_maxxg(stdout)/2+scrsiz/2, WALL);
                /* place updated score on screen */
                pa_fcolor(stdout, pa_black);
                pa_cursorg(stdout, pa_maxxg(stdout)/2-scrsiz/2, 2);
                printf("SCORE %5d\n", score);
                scrchg = FALSE; /* reset score change flag */

            }
            do { pa_event(stdin, &er); /* wait relevant events */
            } while (er.etype != pa_etterm && er.etype != pa_etleft &&
                     er.etype != pa_etright && er.etype != pa_etfun &&
                     er.etype != pa_ettim && er.etype != pa_etjoymov);
            if (er.etype == pa_etterm) goto endgame; /* game exits */
            if (er.etype == pa_etfun) goto newgame; /* restart game */
            /* process paddle movements */
            if (er.etype == pa_etleft) padpos(padx-5); /* move left */
            else if (er.etype == pa_etright) padpos(padx+5); /* move right */
            else if (er.etype == pa_etjoymov) /* move joystick */
                padpos(pa_maxxg(stdout)/2+er.joypx/jchr);
            else if (er.etype == pa_ettim) { /* move timer */

                if (er.timnum == 1) { /* ball timer */

                    if (ball.x1 > 0) { /* ball on screen */

                        balsav = ball; /* save ball position */
                        offrect(&ball, bdx, bdy); /* move the ball */
                        /* check off screen motions */
                        if (intersect(&ball, &walll) || intersect(&ball, &wallr)) {

                            /* hit left or right wall */
                            ball = balsav; /* restore */
                            bdx = -bdx; /* change direction */
                            offrect(&ball, bdx, bdy); /* recalculate */
                            /* start bounce note */
                            pa_noteon(PA_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                            pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+BOUNCETIME, 1, WALLNOTE, INT_MAX);

                        } else if (intersect(&ball, &wallt)) { /* hits top */

                            ball = balsav; /* restore */
                            bdy = -bdy; /* change direction */
                            offrect(&ball, bdx, bdy); /* recalculate */
                            /* start bounce note */
                            pa_noteon(PA_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                            pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+BOUNCETIME, 1, WALLNOTE, INT_MAX);

                        } else if (intersect(&ball, &paddle)) {

                            ball = balsav; /* restore */
                            /* find which 5th of the paddle was struck */
                            bip = (ball.x1+hBALLS-paddle.x1)/(PADW/5);
                            /* clip to 5th */
                            if (bip < 0) bip = 0;
                            if (bip > 5) bip = 5;
                            switch (bip) {

                                case 0: bdx = -2; break; /* left hard */
                                case 1: bdx = -1; break; /* soft soft */
                                case 2: ;         break; /* center reflects */
                                case 3: bdx = +1; break; /* right soft */
                                case 4: bdx = +2; break; /* right hard */
                                case 5: bdx = +2; break; /* right hard */

                            }
                            bdy = -bdy; /* reflect y */
                            offrect(&ball, bdx, bdy); /* recalculate */
                            /* if the ball is still below the paddle plane, move
                               it up until it is not */
                            if (ball.y2 >= paddle.y1)
                                offrect(&ball, 0, -(ball.y2-paddle.y1+1));
                            /* start bounce note */
                            pa_noteon(PA_SYNTH_OUT, 0, 1, WALLNOTE, INT_MAX);
                            pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+BOUNCETIME, 1, WALLNOTE, INT_MAX);

                        } else { /* check brick hits */

                            interbrick(); /* check brick intersection */
                            if (brki) { /* there was a brick hit */

                                ball = balsav; /* restore */
                                bdy = -bdy; /* change direction */
                                offrect(&ball, bdx, bdy); /* recalculate */
                                /* start bounce note */
                                pa_noteon(PA_SYNTH_OUT, 0, 1, BRICKNOTE, INT_MAX);
                                pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+BOUNCETIME, 1, BRICKNOTE, INT_MAX);

                            }

                        };
                        if (intersect(&ball, &wallb)) { /* ball out of bounds */

                            drwrect(&balsav, pa_white);
                            clrrect(&ball); /* set ball not on screen */
                            /* start time on new ball wait */
                            baltim = NEWBAL/BALMOV;
                            /* start fail note */
                            pa_noteon(PA_SYNTH_OUT, 0, 1, FAILNOTE, INT_MAX);
                            pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+FAILTIME, 1, FAILNOTE, INT_MAX);

                        } else { /* ball in play */

                            /* erase only the pa_left(stdout)over part of the old ball */
                            pa_fcolor(stdout, pa_white);
                            if (bdx < 0) /* ball move left */
                                pa_frect(stdout, ball.x2+1, balsav.y1,
                                                 balsav.x2, balsav.y2);
                            else /* move move right */
                                pa_frect(stdout, balsav.x1, balsav.y1,
                                                 ball.x1-1, balsav.y2);
                            if (bdy < 0) /* ball move up(stdout) */
                                pa_frect(stdout, balsav.x1, ball.y2+1,
                                                 balsav.x2, balsav.y2);
                            else /* move move pa_down(stdout) */
                                pa_frect(stdout, balsav.x1, balsav.y1,
                                                 balsav.x2, ball.y1-1);
                            drwrect(&ball, BALLCLR); /* pa_redraw the ball */

                        }

                    }
                    /* if the ball timer is running, decrement it */
                    if (baltim > 0) baltim--;

                }

            }

        } while (fldbrk != BRKROW*BRKCOL); /* until bricks are cleared */
        pa_noteon(PA_SYNTH_OUT,  0,                  1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+OSEC*2,  1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        pa_noteon(PA_SYNTH_OUT,  pa_curtimeout()+OSEC*3,  1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+OSEC*4,  1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
        pa_noteon(PA_SYNTH_OUT,  pa_curtimeout()+OSEC*5,  1, PA_NOTE_E+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+OSEC*6,  1, PA_NOTE_E+PA_OCTAVE_6, INT_MAX);
        pa_noteon(PA_SYNTH_OUT,  pa_curtimeout()+OSEC*7,  1, PA_NOTE_F+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+OSEC*8,  1, PA_NOTE_F+PA_OCTAVE_6, INT_MAX);
        pa_noteon(PA_SYNTH_OUT,  pa_curtimeout()+OSEC*9,  1, PA_NOTE_E+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+OSEC*10, 1, PA_NOTE_E+PA_OCTAVE_6, INT_MAX);
        pa_noteon(PA_SYNTH_OUT,  pa_curtimeout()+OSEC*11, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(PA_SYNTH_OUT, pa_curtimeout()+OSEC*13, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
        baltim = (OSEC*13+NEWBAL)/BALMOV; /* wait fanfare */
        drwrect(&ball, pa_white); /* clear ball */
        clrrect(&ball); /* set ball not on screen */

    } while (TRUE); /* forever */

    endgame:; /* exit game */

    pa_closesynthout(PA_SYNTH_OUT); /* close synthesizer */

}
