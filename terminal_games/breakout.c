/*******************************************************************************
*                                                                              *
*                                BREAKOUT GAME                                 *
*                                                                              *
*                       COPYRIGHT (C) 2002 S. A. FRANCO                        *
*                                                                              *
* Plays breakout in text mode. This is the character edition of breakoutg:     *
* the same game with the same manners -- real time physics with the ball at    *
* arbitrary angles, the paddle by keyboard, joystick, or mouse capture, and    *
* the same sounds -- drawn in character cells. Cells update in place as the    *
* pieces move; character output is cheap enough that no page flipping is       *
* needed.                                                                      *
*                                                                              *
*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <sound.h>
#include <terminal.h>

/* enable sounds */
#define SOUND 1

#define   SECOND      10000                   /* one second */
#define   OSEC        (SECOND/8)              /* 1/8 second */
#define   BALMOV      75                      /* ball move timer: one step each
                                                 7.5ms, machine independent;
                                                 lower for a faster ball */
#define   NEWBAL      (SECOND/BALMOV)         /* ticks to wait for new ball */
#define   FANWAIT     ((OSEC*13+SECOND)/BALMOV) /* ticks to wait out fanfare */
#define   BALLCLR     ami_blue                 /* ball color */
#define   WALLCLR     ami_cyan                 /* wall color */
#define   PADCLR      ami_green                /* paddle color */
#define   CAPCLR      ami_yellow               /* paddle color when captured */
#define   CAPDWELL    (SECOND/BALMOV)          /* ticks of held press to capture */
#define   RELDWELL    (SECOND/BALMOV)          /* ticks of held press to release */
#define   PADEDGE     0.9                     /* how hard the strike point turns
                                                 the ball, in speed fractions */
#define   MINVERT     0.30                    /* minimum vertical speed fraction,
                                                 keeping shots off horizontal */
#define   BOUNCETIME  250                     /* time to play bounce note */
#define   WALLNOTE    (AMI_NOTE_D+AMI_OCTAVE_6) /* note to play off wall */
#define   BRICKNOTE   (AMI_NOTE_E+AMI_OCTAVE_7) /* note to play off brick */
#define   FAILTIME    1500                    /* note to play on failure */
#define   FAILNOTE    (AMI_NOTE_C+AMI_OCTAVE_4) /* note to play on fail */
#define   BRKROW      6                       /* number of brick rows */
#define   BRKCOL      10                      /* number of brick columns */

typedef struct { /* rectangle, in character cells */

    int x1, y1, x2, y2;

} rectangle;

int       padx;                       /* paddle position x */
float     bvx;                        /* ball velocity x, cells per tick */
float     bvy;                        /* ball velocity y, cells per tick */
float     bfx;                        /* ball exact position x */
float     bfy;                        /* ball exact position y; the cell is
                                         the rounding of the exact position,
                                         so any angle resolves to cells the
                                         way a line draw does */
int       baltim;                     /* ball start timer */
ami_evtrec er;                         /* event record */
ami_long  jchr;                       /* joystick units per character */
int       score;                      /* score */
int       scrchg;                     /* score has changed */
rectangle paddle;                     /* paddle rectangle */
rectangle lpaddle;                    /* paddle as last drawn */
rectangle ball;                       /* ball rectangle (one cell) */
rectangle lball;                      /* ball as last drawn, 0 for none */
rectangle wallt, walll, wallr, wallb; /* wall rectangles */
rectangle bricks[BRKROW][BRKCOL];     /* brick array */
int       brki;                       /* brick was intersected */
int       fldbrk;                     /* bricks hit this field */
int       padw;                       /* paddle width */
int       hpadw;                      /* half paddle width */
int       padstp;                     /* paddle step per key event */
ami_long  joylst;                     /* last joystick x reported */
int       joyini;                     /* joystick baseline taken */
int       mcap;                       /* paddle is captured to the mouse */
int       lstmx;                      /* last mouse position x */
int       lstmy;                      /* last mouse position y */
int       btndn;                      /* mouse button is down */
int       relarm;                     /* button released since capture: the
                                         next held press releases */
int       dwltim;                     /* held-press countdown to capture */
int       reltim;                     /* held-press countdown to release */

/*******************************************************************************

Write string to screen

Writes a string to the indicated position on the screen.

********************************************************************************/

void writexy(int x, int y,   /* position to write to */
             const char* s)  /* string to write */

{

    ami_cursor(stdout, x, y); /* position cursor */
    fputs(s, stdout); /* output string */

}

/*******************************************************************************

Write centered string

Writes a string that is centered on the line given.

********************************************************************************/

void wrtcen(int         y, /* y position of string */
            const char* s) /* string to write */

{

    writexy(ami_maxx(stdout)/2-strlen(s)/2, y, s);

}

/*******************************************************************************

Fill rectangle

Fills a cell rectangle with spaces in the given background color.

********************************************************************************/

void filrect(rectangle* r, ami_color c)

{

    int x, y;

    ami_bcolor(stdout, c); /* set color */
    for (y = r->y1; y <= r->y2; y++) {

        ami_cursor(stdout, r->x1, y);
        for (x = r->x1; x <= r->x2; x++) putchar(' ');

    }
    ami_bcolor(stdout, ami_white); /* restore background */

}

/*******************************************************************************

Rationalize a rectangle

Arranges the points so that the 1st point is lower in x and y than the second.

********************************************************************************/

void ratrect(rectangle* r)

{

    int t; /* swap temp */

    if (r->x1 > r->x2) { t = r->x1; r->x1 = r->x2; r->x2 = t; }
    if (r->y1 > r->y2) { t = r->y1; r->y1 = r->y2; r->y2 = t; }

}

/*******************************************************************************

Find intersection of rectangles

Checks if two rectangles intersect. Returns true if so.

********************************************************************************/

int intsec(rectangle* r1, rectangle* r2)

{

    ratrect(r1);
    ratrect(r2);

    return (r1->x2 >= r2->x1 && r1->x1 <= r2->x2 &&
            r1->y2 >= r2->y1 && r1->y1 <= r2->y2);

}

/*******************************************************************************

Set rectangle

Sets the rectangle to the given values.

********************************************************************************/

void setrct(rectangle* r, int x1, int y1, int x2, int y2)

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

Draw score

Places the score on the top wall.

********************************************************************************/

void drwscore(void)

{

    char sb[30];

    snprintf(sb, sizeof(sb), " SCORE %5d ", score);
    ami_bcolor(stdout, WALLCLR);
    ami_fcolor(stdout, ami_black);
    wrtcen(1, sb);
    ami_bcolor(stdout, ami_white);

}

/*******************************************************************************

Draw ball and paddle

Each is drawn where it now stands and erased where it last was, cell for
cell; only what moved is touched.

********************************************************************************/

void drwball(void)

{

    if (lball.x1 == ball.x1 && lball.y1 == ball.y1) return;
    if (lball.x1) filrect(&lball, ami_white); /* erase old */
    if (ball.x1) filrect(&ball, BALLCLR); /* draw new */
    lball = ball; /* note what is drawn */

}

void drwpad(void)

{

    filrect(&lpaddle, ami_white); /* erase old */
    filrect(&paddle, mcap? CAPCLR: PADCLR); /* draw new, showing capture */
    lpaddle = paddle; /* note what is drawn */

}

/*******************************************************************************

Draw screen

Draws a new game screen: walls, score, and title.

********************************************************************************/

void drwscn(void)

{

    putchar('\f'); /* clear screen */
    filrect(&wallt, WALLCLR); /* top */
    filrect(&walll, WALLCLR); /* left */
    filrect(&wallr, WALLCLR); /* right */
    filrect(&wallb, WALLCLR); /* bottom */
    ami_bcolor(stdout, WALLCLR);
    ami_fcolor(stdout, ami_black);
    wrtcen(ami_maxy(stdout), "BREAKOUT VS. 1.0");
    ami_bcolor(stdout, ami_white);
    drwscore();

}

/*******************************************************************************

Set brick wall

Initializes the bricks in the wall coordinates.

********************************************************************************/

void setwall(void)

{

    int r, c;   /* brick array indexes */
    int brkw;   /* brick width */
    int brkr;   /* brick remainder */
    int brkoff; /* brick wall offset */
    int co;     /* column offset */
    int rd;     /* remainder distributor */

    brkw = (ami_maxx(stdout)-2)/BRKCOL; /* find brick width */
    brkr = (ami_maxx(stdout)-2)%BRKCOL; /* find brick remainder */
    brkoff = ami_maxy(stdout)/4; /* find brick wall offset */
    for (r = 0; r < BRKROW; r++) {

        co = 0; /* clear column offset */
        rd = brkr; /* set remainder distributor */
        for (c = 0; c < BRKCOL; c++) {

            setrct(&bricks[r][c], 2+co, brkoff+r,
                                  2+co+brkw-1+(rd > 0), brkoff+r);
            co = co+brkw+(rd > 0); /* offset to next brick */
            if (rd > 0) rd--; /* reduce remainder */

        }

    }

}

/*******************************************************************************

Draw wall

Draws the standing bricks, each in its color with a single space between
bricks left by erasure on hits.

********************************************************************************/

void drwwall(void)

{

    int      r, c; /* brick array indexes */
    ami_color clr;  /* brick color */

    clr = ami_red; /* set 1st pure color */
    for (r = 0; r < BRKROW; r++)
        for (c = 0; c < BRKCOL; c++) {

        if (bricks[r][c].x1) filrect(&bricks[r][c], clr);
        if (clr < ami_magenta) clr++;
        else clr = ami_red;

    }

}

/*******************************************************************************

Set new paddle position

Places the paddle at the given position and redraws it there.

********************************************************************************/

void padpos(int x)

{

    if (x-hpadw <= walll.x2) x = walll.x2+hpadw+1; /* clip to ends */
    if (x+hpadw >= wallr.x1) x = wallr.x1-hpadw-1;
    padx = x; /* set new location */
    setrct(&paddle, x-hpadw, ami_maxy(stdout)-1,
                    x+hpadw, ami_maxy(stdout)-1);
    drwpad(); /* show it where it now stands */

}

/*******************************************************************************

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
        for (c = 0; c < BRKCOL; c++) if (intsec(&ball, &bricks[r][c])) {

        brki = TRUE; /* set intersected */
        filrect(&bricks[r][c], ami_white); /* erase from screen */
        clrrect(&bricks[r][c]); /* clear brick data */
        score++; /* count hits */
        scrchg = TRUE; /* set changed */
        fldbrk++; /* add to bricks this field */

    }

}

/*******************************************************************************

Move ball

Advances the ball one step and resolves its collisions: walls, paddle,
bricks, and the bottom. The exact position and velocity are floats, so the
ball travels at any angle, resolved to cells by rounding.

********************************************************************************/

/* set the ball cell from the exact position */

void balrect(void)

{

    setrct(&ball, round(bfx), round(bfy), round(bfx), round(bfy));

}

void movball(void)

{

    float u;   /* paddle strike offset, -1 left edge to 1 right edge */
    float spd; /* ball speed */
    float mag; /* velocity magnitude for renormalizing */

    if (ball.x1 > 0) { /* ball on screen */

        /* advance the exact position and rederive the cell */
        bfx += bvx;
        bfy += bvy;
        balrect();
        /* check off screen motions */
        if (intsec(&ball, &walll) || intsec(&ball, &wallr)) {

            /* hit left or right wall: reflect and replay the move */
            bfx -= bvx;
            bvx = -bvx;
            bfx += bvx;
            balrect();
#ifdef SOUND
            /* start bounce note */
            ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, LONG_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, WALLNOTE, LONG_MAX);
#endif

        } else if (intsec(&ball, &wallt)) { /* hits top */

            bfy -= bvy;
            bvy = -bvy;
            bfy += bvy;
            balrect();
#ifdef SOUND
            /* start bounce note */
            ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, LONG_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, WALLNOTE, LONG_MAX);
#endif

        } else if (intsec(&ball, &paddle)) {

            /* A real paddle: the strike point turns the ball. The incoming
               angle carries through as the reflected velocity, then the
               offset of the strike from paddle center adds its own turn,
               the edges throwing the sharpest angles. Speed is preserved,
               and the shot is held off horizontal so it always climbs */
            bfx -= bvx; /* stand at the strike */
            bfy -= bvy;
            balrect();
            u = (bfx-(paddle.x1+hpadw))/(float)(hpadw+1);
            if (u < -1.0) u = -1.0;
            if (u > 1.0) u = 1.0;
            spd = sqrt(bvx*bvx+bvy*bvy);
            bvy = -bvy; /* reflect vertical */
            bvx = bvx+u*spd*PADEDGE; /* the strike turns it */
            /* renormalize to the speed */
            mag = sqrt(bvx*bvx+bvy*bvy);
            bvx = bvx*spd/mag;
            bvy = bvy*spd/mag;
            /* hold the shot off horizontal */
            if (bvy > -spd*MINVERT) {

                bvy = -spd*MINVERT;
                mag = sqrt(spd*spd-bvy*bvy);
                bvx = bvx < 0.0? -mag: mag;

            }
            bfx += bvx; /* replay the move on the new heading */
            bfy += bvy;
            balrect();
            /* if the ball is still below the paddle plane, move
               it up until it is not */
            if (ball.y2 >= paddle.y1) {

                bfy -= ball.y2-paddle.y1+1;
                balrect();

            }
#ifdef SOUND
            /* start bounce note */
            ami_noteon(AMI_SYNTH_OUT, 0, 1, WALLNOTE, LONG_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, WALLNOTE, LONG_MAX);
#endif

        } else { /* check brick hits */

            interbrick(); /* check brick intersection */
            if (brki) { /* there was a brick hit */

                bfy -= bvy; /* reflect and replay */
                bvy = -bvy;
                bfy += bvy;
                balrect();
#ifdef SOUND
                /* start bounce note */
                ami_noteon(AMI_SYNTH_OUT, 0, 1, BRICKNOTE, LONG_MAX);
                ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+BOUNCETIME, 1, BRICKNOTE, LONG_MAX);
#endif

            }

        };
        if (intsec(&ball, &wallb)) { /* ball out of bounds */

            clrrect(&ball); /* set ball not on screen */
            /* start time on new ball wait */
            baltim = NEWBAL;
#ifdef SOUND
            /* start fail note */
            ami_noteon(AMI_SYNTH_OUT, 0, 1, FAILNOTE, LONG_MAX);
            ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+FAILTIME, 1, FAILNOTE, LONG_MAX);
#endif

        }
        drwball(); /* show it where it now stands */

    }

}

int main(void)

{

#ifdef SOUND
    ami_opensynthout(AMI_SYNTH_OUT); /* open synthesizer */
    ami_instchange(AMI_SYNTH_OUT, 0, 1, AMI_INST_LEAD_1_SQUARE);
    ami_starttimeout(); /* start sequencer running */
#endif
    jchr = LONG_MAX/((ami_maxx(stdout)-2)/2); /* find basic joystick increment */
    ami_curvis(stdout, FALSE); /* remove drawing cursor */
    ami_auto(stdout, FALSE); /* turn off scrolling */
    padw = ami_maxx(stdout)/8; /* set paddle width */
    hpadw = padw/2; /* half paddle width */
    padstp = ami_maxx(stdout)/50; /* paddle key step scales with the field */
    if (padstp < 1) padstp = 1;
    ami_timer(stdout, 1, BALMOV, TRUE); /* the ball physics timer */
    dwltim = CAPDWELL; /* arm the capture dwell */

    newgame: /* start new game */

    /* Set up the boundaries first; the paddle placement clips to them */
    setrct(&wallt, 1, 1, ami_maxx(stdout), 1); /* top, carrying the score */
    setrct(&walll, 1, 1, 1, ami_maxy(stdout)); /* left */
    /* right */
    setrct(&wallr, ami_maxx(stdout), 1, ami_maxx(stdout), ami_maxy(stdout));
    /* bottom */
    setrct(&wallb, 1, ami_maxy(stdout), ami_maxx(stdout), ami_maxy(stdout));
    score = 0; /* clear score */
    scrchg = FALSE;
    clrrect(&ball); /* set ball not on screen */
    clrrect(&lball); /* nothing drawn */
    drwscn(); /* draw game screen */
    padx = ami_maxx(stdout)/2; /* find initial paddle position */
    setrct(&lpaddle, padx, ami_maxy(stdout)-1, padx, ami_maxy(stdout)-1);
    padpos(padx); /* place paddle */
    baltim = NEWBAL; /* set starting ball time */
    do { /* game loop */

        setwall(); /* initialize bricks */
        drwwall(); /* draw the wall */
        fldbrk = 0; /* clear bricks hit this field */
        do { /* fields */

            if (ball.x1 == 0 && baltim == 0) {

                /* ball not on screen, and time to wait expired, send out ball */
                bfx = 3; /* launch position */
                bfy = ami_maxy(stdout)-4;
                bvx = ami_maxx(stdout)/300.0; /* set direction of travel */
                bvy = -(ami_maxy(stdout)/150.0);
                balrect();
                drwball();

            }
            if (scrchg) { /* the score updates in place */

                drwscore();
                scrchg = FALSE;

            }
            do { ami_event(stdin, &er); /* wait relevant events */
            } while (er.etype != ami_etterm && er.etype != ami_etleft &&
                     er.etype != ami_etright && er.etype != ami_etfun &&
                     er.etype != ami_ettim && er.etype != ami_etjoymov &&
                     er.etype != ami_etmoumov && er.etype != ami_etmouba &&
                     er.etype != ami_etmoubd);
            if (er.etype == ami_etterm) goto endgame; /* game exits */
            if (er.etype == ami_etfun) goto newgame; /* restart game */
            /* process paddle movements */
            if (er.etype == ami_etleft) { /* move left */

                mcap = FALSE; /* the keys take the paddle back */
                dwltim = CAPDWELL; /* and the capture dwell rearms */
                padpos(padx-padstp);

            } else if (er.etype == ami_etright) { /* move right */

                mcap = FALSE;
                dwltim = CAPDWELL;
                padpos(padx+padstp);

            } else if (er.etype == ami_etmoumov) { /* mouse moves */

                lstmx = er.moupx; /* track position */
                lstmy = er.moupy;
                if (mcap) padpos(lstmx); /* the paddle rides the mouse */

            } else if (er.etype == ami_etmouba && er.amoubn == 1)
                btndn = TRUE; /* the held press drives the dwell clocks */
            else if (er.etype == ami_etmoubd && er.dmoubn == 1)
                btndn = FALSE;
            else if (er.etype == ami_etjoymov) { /* move joystick */

                /* The paddle follows the joystick only when the stick
                   itself moves: an idle stick reports steadily, and
                   letting it hold the paddle would override the keys */
                if (!joyini) { joyini = TRUE; joylst = er.joypx; }
                else if (er.joypx != joylst) {

                    joylst = er.joypx;
                    mcap = FALSE; /* the stick takes the paddle back */
                    dwltim = CAPDWELL;
                    padpos(ami_maxx(stdout)/2+er.joypx/jchr);

                }

            }
            /* the physics beat: step the ball and the waits */
            if (er.etype == ami_ettim && er.timnum == 1) {

                movball();
                /* if the ball timer is running, decrement it */
                if (baltim > 0) baltim--;
                if (!mcap) {

                    /* the capture dwell: the button held down on the
                       paddle for the dwell time takes it; the paddle
                       then follows the mouse with the button up */
                    if (btndn &&
                        lstmx >= paddle.x1 && lstmx <= paddle.x2 &&
                        lstmy == paddle.y1) {

                        if (dwltim > 0) dwltim--;
                        if (!dwltim) {

                            mcap = TRUE; /* the mouse takes the paddle */
                            relarm = FALSE; /* this press spends itself */
                            reltim = RELDWELL;
                            drwpad(); /* show the capture */

                        }

                    } else dwltim = CAPDWELL; /* released or off rearms */

                } else {

                    /* the release dwell: a fresh press held for the
                       dwell time lets the paddle go. The press that
                       captured must lift first, or one long hold would
                       capture and release in a single stroke */
                    if (!btndn) { relarm = TRUE; reltim = RELDWELL; }
                    else if (relarm) {

                        if (reltim > 0) reltim--;
                        if (!reltim) {

                            mcap = FALSE;
                            dwltim = CAPDWELL;
                            drwpad(); /* show the release */

                        }

                    }

                }

            }

        } while (fldbrk != BRKROW*BRKCOL); /* until bricks are cleared */
#ifdef SOUND
        ami_noteon(AMI_SYNTH_OUT,  0,                  1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*2,  1, AMI_NOTE_C+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*3,  1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*4,  1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*5,  1, AMI_NOTE_E+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*6,  1, AMI_NOTE_E+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*7,  1, AMI_NOTE_F+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*8,  1, AMI_NOTE_F+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*9,  1, AMI_NOTE_E+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*10, 1, AMI_NOTE_E+AMI_OCTAVE_6, LONG_MAX);
        ami_noteon(AMI_SYNTH_OUT,  ami_curtimeout()+OSEC*11, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
        ami_noteoff(AMI_SYNTH_OUT, ami_curtimeout()+OSEC*13, 1, AMI_NOTE_D+AMI_OCTAVE_6, LONG_MAX);
#endif
        baltim = FANWAIT; /* wait fanfare */
        if (lball.x1) filrect(&lball, ami_white); /* clear ball */
        clrrect(&ball); /* set ball not on screen */
        clrrect(&lball);

    } while (TRUE); /* forever */

    endgame:; /* exit game */

    ami_curvis(stdout, TRUE);
#ifdef SOUND
    ami_closesynthout(AMI_SYNTH_OUT); /* close synthesizer */
#endif

}
