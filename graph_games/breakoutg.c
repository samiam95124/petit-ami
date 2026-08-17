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
#include <graphics.h>

/* enable sounds */
#define SOUND 1

#define   SECOND      10000                   /* one second */
#define   OSEC        (SECOND/8)              /* 1/8 second */
#define   BALMOV      75                      /* ball move timer: one step each
                                                 7.5ms, machine independent;
                                                 lower for a faster ball */
#define   NEWBAL      (SECOND/BALMOV)         /* ticks to wait for new ball */
#define   FANWAIT     ((OSEC*13+SECOND)/BALMOV) /* ticks to wait out fanfare */
#define   BALLCLR     ami_blue                 /* ball ami_color */
#define   WALLCLR     ami_cyan                 /* wall ami_color */
#define   PADCLR      ami_green                /* paddle ami_color */
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

typedef struct { /* rectangle */

    int x1, y1, x2, y2;

} rectangle;

int       padx;                       /* paddle position x */
float     bvx;                        /* ball velocity x, pixels per tick */
float     bvy;                        /* ball velocity y, pixels per tick */
float     bfx;                        /* ball exact position x */
float     bfy;                        /* ball exact position y; the rectangle
                                         is the rounding of the exact position,
                                         so any angle resolves to pixels the
                                         way a line draw does */
int       baltim;                     /* ball start timer */
ami_evtrec er;                         /* event record */
long      jchr;                       /* number of pixels to joystick
                                         movement */
int       score;                      /* score */
int       scrsiz;                     /* score size */
int       bac;                        /* ball accelerator */
rectangle paddle;                     /* paddle rectangle */
rectangle ball;                       /* ball rectangle */
rectangle wallt, walll, wallr, wallb; /* WALL rectangles */
rectangle bricks[BRKROW][BRKCOL];     /* brick array */
int       brki;                       /* brick was intersected */
int       fldbrk;                     /* bricks hit this field */
int       wall;                       /* wall thickness */
int       brkh;                       /* brick height */
int       brkbrd;                     /* brick border */
int       balls;                      /* ball size */
int       hballs;                     /* half ball size */
int       padh;                       /* height of paddle */
int       pwdis;                      /* distance of paddle from bottom wall */
int       padw;                       /* paddle width */
int       hpadw;                      /* half paddle width */
int       padstp;                     /* paddle step per key event */
int       curpag;                     /* current display page for the flip */
long      joylst;                     /* last joystick x reported */
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
             const string s) /* char* to write */

{

    ami_cursorg(stdout, x, y); /* position cursor */
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

    off = ami_maxxg(stdout)/2-ami_strsiz(stdout, s)/2;
    writexy(off, y, s); /* write out contents */

}

/*******************************************************************************

Translate color code

Translates a logical color to an RGB color. Returns the RGB color in three
variables.

*******************************************************************************/

void log2rgb(ami_color c, long* r, long* g, long* b)

{

    /* translate color number */
    switch (c) { /* color */

        case ami_black:   *r = 0;        *g= 0;         *b = 0;        break;
        case ami_white:   *r = LONG_MAX; *g = LONG_MAX; *b = LONG_MAX; break;
        case ami_red:     *r = LONG_MAX; *g = 0;        *b = 0;        break;
        case ami_green:   *r = 0;        *g = LONG_MAX; *b = 0;        break;
        case ami_blue:    *r = 0;        *g = 0;        *b = LONG_MAX; break;
        case ami_cyan:    *r = 0;        *g = LONG_MAX; *b = LONG_MAX; break;
        case ami_yellow:  *r = LONG_MAX; *g = LONG_MAX; *b = 0;        break;
        case ami_magenta: *r = LONG_MAX; *g = 0;        *b = LONG_MAX; break;
        default: ;

    }

}

/*******************************************************************************

Draw rectangle

Draws a filled rectangle, in the given color.

********************************************************************************/

void drwrect(rectangle* r, ami_color c)

{

    ami_fcolor(stdout, c); /* set color */
    ami_frect(stdout, r->x1, r->y1, r->x2, r->y2);

}

/*******************************************************************************

Draw bordered rectangle

Draws a filled rectangle with border, in the given color.

********************************************************************************/

void dim(float dv, long* r, long* g, long* b)

{

    *r = trunc(*r*dv);
    *g = trunc(*g*dv);
    *b = trunc(*b*dv);

}

void drwbrect(rectangle* r, ami_color c)

{

    int i;
    long hr, hg, hb; /* rgb value of highlight */
    long mr, mg, mb; /* rbg value of midlight */
    long lr, lg, lb; /* rbg value of lowlight */

    log2rgb(c, &hr, &hg, &hb); /* find actual ami_color */
    mr = hr; /* copy */
    mg = hg;
    mb = hb;
    lr = hr;
    lg = hg;
    lb = hb;
    dim(0.80, &mr, &mg, &mb); /* dim midlight to %75 */
    dim(0.60, &lr, &lg, &lb); /* dim lowlight to %50 */
    ami_fcolorg(stdout, mr, mg, mb); /* set brick body to midlight */
    ami_frect(stdout, r->x1, r->y1, r->x2, r->y2); /* draw brick */
    ami_fcolorg(stdout, hr, hg, hb); /* set hilight */
    ami_frect(stdout, r->x1, r->y1, r->x1+brkbrd-1, r->y2); /* border left */
    ami_frect(stdout, r->x1, r->y1, r->x2, r->y1+brkbrd-1); /* top */
    /* set lowlight border color */
    ami_fcolorg(stdout, lr, lg, lb);
    /* border right */
    for (i = 1; i <= brkbrd; i++)
        ami_frect(stdout, r->x2-i+1, r->y1+i-1, r->x2, r->y2);
    /* border bottom */
    for (i = 1; i <= brkbrd; i++)
        ami_frect(stdout, r->x1+i-1, r->y2-i+1, r->x2, r->y2);

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

int intsec(rectangle* r1, rectangle* r2)

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

Draw wall

Redraws the brick wall. A brick already knocked out (its rectangle cleared)
leaves its place empty; the color sequence steps by position so the standing
bricks keep their colors as neighbors disappear.

********************************************************************************/

void drwwall(void)

{

    int      r, c; /* brick array indexes */
    ami_color clr;  /* brick color */

    clr = ami_red; /* set 1st pure color */
    for (r = 0; r < BRKROW; r++)
        for (c = 0; c < BRKCOL; c++) {

        if (bricks[r][c].x1) drwbrect(&bricks[r][c], clr);
        if (clr < ami_magenta) clr++;
        else clr = ami_red;

    }

}

/*******************************************************************************

Draw frame

Draws the complete game scene into the hidden page, then flips it to the
display. This is double buffered animation on the ball6 model: two screen
surfaces alternate, the update surface always the one not shown, and the
select at the top of each frame both reveals the page drawn last frame and
turns the stale one into this frame's canvas. Every frame is drawn whole --
no erasing on a visible surface, so nothing flickers, however fast or slow
the machine.

********************************************************************************/

void drwframe(void)

{

    /* reveal the page drawn last frame; the other becomes the canvas */
    ami_select(stdout, !curpag+1, curpag+1);
    putchar('\f'); /* clear the canvas page */
    /* each surface carries its own attributes; dress this one */
    ami_font(stdout, AMI_FONT_SIGN);
    ami_bold(stdout, TRUE);
    ami_fontsiz(stdout, wall-2);
    ami_binvis(stdout);
    /* draw walls */
    drwrect(&wallt, WALLCLR); /* top */
    drwrect(&walll, WALLCLR); /* left */
    drwrect(&wallr, WALLCLR); /* right */
    drwrect(&wallb, WALLCLR); /* bottom */
    ami_fcolor(stdout, ami_black);
    wrtcen(ami_maxyg(stdout)-wall+1, "BREAKOUT VS. 1.0");
    /* the score rides the top wall */
    ami_fcolor(stdout, ami_black);
    ami_cursorg(stdout, ami_maxxg(stdout)/2-scrsiz/2, 2);
    printf("SCORE %5d\n", score);
    drwwall(); /* the standing bricks */
    drwrect(&paddle, mcap? CAPCLR: PADCLR); /* the paddle, showing capture */
    if (ball.x1 > 0) drwrect(&ball, BALLCLR); /* the ball, when in play */
    curpag = !curpag; /* the page just drawn shows at the next frame */

}

/*******************************************************************************

Set new paddle position

Places the paddle at the given position.

********************************************************************************/

void padpos(int x)

{

    if (x-hpadw <= walll.x2) x = walll.x2+hpadw+1; /* clip to ends */
    if (x+hpadw >= wallr.x1) x = wallr.x1-hpadw-1;
    padx = x; /* set new location */
    setrct(&paddle, x-hpadw, ami_maxyg(stdout)-wall-padh-pwdis,
                    x+hpadw, ami_maxyg(stdout)-wall-pwdis);
    /* the next frame draws it where it now stands */

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

    brkw = (ami_maxxg(stdout)-2*wall)/BRKCOL; /* find brick width */
    brkr = (ami_maxxg(stdout)-2*wall)%BRKCOL-1; /* find brick remainder */
    brkoff = ami_maxyg(stdout)/4; /* find brick wall offset */
    for (r = 0; r < BRKROW; r++) {

        co = 0; /* clear collumn offset */
        rd = brkr; /* set remainder distributor */
        for (c = 0; c < BRKCOL; c++) {

            setrct(&bricks[r][c], 1+co+wall, 1+(r-1)*brkh+brkoff,
                                              1+co+brkw-1+wall+(rd > 0),
                                              1+(r-1)*brkh+brkh-1+brkoff);
            co = co+brkw+(rd > 0); /* offset to next brick */
            if (brkr > 0) rd = rd-1; /* ami_reduce remainder */

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
        for (c = 0; c < BRKCOL; c++) if (intsec(&ball, &bricks[r][c])) {

        brki = TRUE; /* set intersected */
        clrrect(&bricks[r][c]); /* clear brick data; it stops being drawn */
        score++; /* count hits */
        fldbrk++; /* add to bricks this field */

    }

}

/*******************************************************************************

Move ball

Advances the ball one step and resolves its collisions: walls, paddle,
bricks, and the bottom. One step is small -- the frame beat calls this
several times per frame, so the ball cannot tunnel through what it should
bounce from, the same way ball6 repeats its moves per frame.

********************************************************************************/

/* set the ball rectangle from the exact position */

void balrect(void)

{

    setrct(&ball, round(bfx), round(bfy), round(bfx)+balls, round(bfy)+balls);

}

void movball(void)

{

    float u;   /* paddle strike offset, -1 left edge to 1 right edge */
    float spd; /* ball speed */
    float mag; /* velocity magnitude for renormalizing */

    if (ball.x1 > 0) { /* ball on screen */

        /* advance the exact position and rederive the rectangle */
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
            u = ((bfx+hballs)-(paddle.x1+hpadw))/(float)(hpadw+hballs);
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
        /* in play, the next frame draws the ball where it
           now stands */

    }

}

int main(void)

{

#ifdef SOUND
    ami_opensynthout(AMI_SYNTH_OUT); /* open synthesizer */
    ami_instchange(AMI_SYNTH_OUT, 0, 1, AMI_INST_LEAD_1_SQUARE);
    ami_starttimeout(); /* start sequencer running */
#endif
    jchr = LONG_MAX/((ami_maxxg(stdout)-2)/2); /* find basic joystick increment */
    ami_curvis(stdout, FALSE); /* remove drawing cursor */
    ami_auto(stdout, FALSE); /* turn off scrolling */
    ami_font(stdout, AMI_FONT_SIGN); /* sign font */
    wall = ami_maxyg(stdout)/20; /* set wall thickness */
    brkh = ami_maxyg(stdout)/25; /* set brick thickness */
    brkbrd = brkh/5; /* set brick border */
    balls = ami_maxyg(stdout)/20; /* set ball size */
    hballs = balls/2; /* set half ball size */
    padh = ami_maxyg(stdout)/22; /* set paddle thickness */
    pwdis = padh/4; /* set distance of paddle to wall */
    padw = ami_maxxg(stdout)/8; /* set paddle width */
    hpadw = padw/2; /* half paddle width */
    padstp = ami_maxxg(stdout)/50; /* paddle key step scales with the field */
    dwltim = CAPDWELL; /* arm the capture dwell */
    ami_bold(stdout, TRUE);
    ami_fontsiz(stdout, wall-2); /* font fits in the wall */
    ami_binvis(stdout); /* no background writes */
    ami_timer(stdout, 1, BALMOV, TRUE); /* the ball physics timer */
    curpag = FALSE; /* start the page flip on the first surface */
    ami_frametimer(stdout, TRUE); /* frame events pace the drawing */

    newgame: /* start new game */

    /* set up wall rectangles first; the paddle placement clips to them */
    setrct(&wallt, 1, 1, ami_maxxg(stdout), wall); /* top */
    setrct(&walll, 1, 1, wall, ami_maxyg(stdout)); /* left */
    /* right */
    setrct(&wallr, ami_maxxg(stdout)-wall, 1, ami_maxxg(stdout), ami_maxyg(stdout));
    /* bottom */
    setrct(&wallb, 1, ami_maxyg(stdout)-wall, ami_maxxg(stdout), ami_maxyg(stdout));
    padx = ami_maxxg(stdout)/2; /* find initial paddle position */
    padpos(padx); /* place paddle */
    clrrect(&ball); /* set ball not on screen */
    scrsiz = ami_strsiz(stdout, "SCORE 0000"); /* set nominal size of score string */
    score = 0; /* clear score */
    baltim = NEWBAL; /* set starting ball time */
    do { /* game loop */

        setwall(); /* initialize bricks */
        fldbrk = 0; /* clear bricks hit this field */
        do { /* fields */

            if (ball.x1 == 0 && baltim == 0) {

                /* ball not on screen, and time to wait expired, send out ball */
                bfx = wall+1; /* launch position */
                bfy = ami_maxyg(stdout)-4*wall-balls;
                bvx = ami_maxxg(stdout)/300.0; /* set direction of travel */
                bvy = -(ami_maxyg(stdout)/150.0);
                balrect();

            }
            do { ami_event(stdin, &er); /* wait relevant events */
            } while (er.etype != ami_etterm && er.etype != ami_etleft &&
                     er.etype != ami_etright && er.etype != ami_etfun &&
                     er.etype != ami_ettim && er.etype != ami_etjoymov &&
                     er.etype != ami_etframe && er.etype != ami_etmoumovg &&
                     er.etype != ami_etmouba && er.etype != ami_etmoubd);
            if (er.etype == ami_etterm) goto endgame; /* game exits */
            if (er.etype == ami_etfun) goto newgame; /* restart game */
            /* the frame beat: draw the scene as it now stands and flip */
            if (er.etype == ami_etframe) drwframe();
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
                        lstmy >= paddle.y1 && lstmy <= paddle.y2) {

                        if (dwltim > 0) dwltim--;
                        if (!dwltim) {

                            mcap = TRUE; /* the mouse takes the paddle */
                            relarm = FALSE; /* this press spends itself */
                            reltim = RELDWELL;

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

                        }

                    }

                }

            }
            /* process paddle movements */
            if (er.etype == ami_etleft) { /* move left */

                mcap = FALSE; /* the keys take the paddle back */
                dwltim = CAPDWELL; /* and the capture dwell rearms */
                padpos(padx-padstp);

            } else if (er.etype == ami_etright) { /* move right */

                mcap = FALSE;
                dwltim = CAPDWELL;
                padpos(padx+padstp);

            } else if (er.etype == ami_etmoumovg) { /* mouse moves */

                lstmx = er.moupxg; /* track position */
                lstmy = er.moupyg;
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
                    padpos(ami_maxxg(stdout)/2+er.joypx/jchr);

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
        clrrect(&ball); /* set ball not on screen */

    } while (TRUE); /* forever */

    endgame:; /* exit game */

#ifdef SOUND
    ami_closesynthout(AMI_SYNTH_OUT); /* close synthesizer */
#endif

}
