/*******************************************************************************

SNAKE GAME PROGRAM

1984 S. A. MOORE

Plays a moving target game were the player is a snake, winding it's body around
the screen, eating score producing digit 'targets' and trying to avoid the wall
and itself. The snake's movements are dictated by up, down, east and west
keys. for play details examine the program or simply activate the game (it has
instruction banners). This game is a fairly literal copy (functionality wise)
of the unix 'worm' program.

Adjustments; the following may be adjusted:

     Maximum size of snake: Change maxsn if the snake needs more or less
     possible positions.

     Size of score: adjust scrnum.

     Time between moves: adjust maxtim.

     If accumulated score overflows: adjust maxlft.

History:

The date reads 1984, but frankly I have no idea what the original form of the
program was, since that predates most of my efforts with terminal management.
The most likely would be direct cursor manipulations for a memory mapped screen
such as I used then (in fact the line lengths of the orignal source were within
64 characters, the size of that screen).

I believe it was inspired by my time using Unisoft Unix on the Motorola 68000,
which was before the virtual memory management chip came out. From there it
went to the screen handling library for a WYSE 80 terminal, and that was changed
to ANSI terminals on 1989, likely on DOS, and then to terminal mode on IP Pascal
sometime around 1997.

If still bears some of the oddities of those changes. The routine writescreen()
is a good example of that (Petit-ami does not need that). You will forgive some
of the comments, it was written by a 27 year old (me).

Translated to C, 2019/04/28.

Translated to C+, 2022/08/19.

Translated to C++, 2022/08/19.

*******************************************************************************/

#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#include <iostream>

#include <localdefs.h>
#include <terminal.hpp>

using namespace std;
using namespace terminal;

#define MAXSN  1000  /* total snake positions */
#define TIMMAX 5000  /* time between forced moves (1 second) */
#define BLNTIM 1000  /* delay time for blinker */
#define MAXLFT 100   /* maximum amount of score achievable before
                        being registered without overflow */
/* instruction message at top of screen; s/b maxx characters long */
#define SCRNUM 4     /* number of score digits */
#define SCROFF 45    /* location of first (high) digit of score
                        (should correspond with 'msgstr' above */
#define MAXSCN 250   /* maximum screen dimension */

typedef int                  scrinx;  /* index for score save */
typedef int                  sninx;   /* index for snake array */
/* index set for screen */
typedef struct {

    int scnx;
    int scny;

} scnpos;

/*******************************************************************************

Game terminal object

Adds to term:

1. Stored screen image, and methods to read and write that.
2. An event structure and the method event() to refresh it.
3. Write screen at x,y location.
4. Write string centered at a given line.
5. Handling for the terminate event.

*******************************************************************************/

class gameterm: public term

{

    public:

    /* members */
    evtrec er;
    char   image[MAXSCN][MAXSCN]; /* screen image */

    /* methods */
    int evterm(void);
    void event(void);
    void writeimage(int x, int y, char c);
    char readimage(int x, int y);
    void writescreen(int x, int y, char c);
    void wrtcen(int y, const char* s, int* off);

};

/*******************************************************************************

Get next event

Retrieves the next event to the gameterm er event record.
  
*******************************************************************************/

void gameterm::event(void)

{

    term::event(&er);

}

/*******************************************************************************

Terminate program

Called on etterm event, restores the cursor, automatic mode, and changes the
screen back to 1, then exits the program with no error.

*******************************************************************************/

int gameterm::evterm(void)

{

    curvis(TRUE); /* restore drawing cursor */
    autom(TRUE); /* restore automatic scrolling */
    select(1, 1); /* back to original screen */

    exit(0);

}

/*******************************************************************************

Write single character to screen image

Writes the given character to the given X and Y point on the screen image. 

********************************************************************************/

void gameterm::writeimage(int x, int y, /* position to place character */
                        char c)       /* write screen */

{

    image[x][y] = c;

}

/*******************************************************************************

Read single character from screen image

Reads a character from the given X and Y point on the screen image and returns
it. 

********************************************************************************/

char gameterm::readimage(int x, int y) /* position to place character */

{

    return (image[x][y]);

}

/*******************************************************************************

Write single character to screen

Writes the given character to the given X and Y point on the screen. Also saves
a copy to our screen image.

********************************************************************************/

void gameterm::writescreen(int x, int y, /* position to place character */
                         char c)       /* write screen */

{

    cursor(x, y); /* position to the given location */
    if (c != image[x][y]) { /* filter redundant placements */

        cout << c; /* write the character */
        image[x][y] = c; /* place character in image */

    }

}

/*******************************************************************************

Write centered string

Writes a string that is centered on the line given. Returns the starting
position of the string.

*******************************************************************************/

void gameterm::wrtcen(int          y,   /* y position of string */
                    const char*  s,   /* string to write */
                    int*         off) /* returns string offset */

{

    int i; /* index for string */

    *off = maxx()/2-strlen(s) / 2;
    /* write out contents */
    for (i = 1; i < strlen(s); i++) writescreen(i+*off, y, s[i]);

}

/*******************************************************************************

Game object

Contains data and methods for the game.

*******************************************************************************/

class game: public gameterm 

{

    public:

    int    timcnt;         /* move countdown */
    scnpos snakel[MAXSN];  /* snake's positions */
    int    sntop;          /* current snake array top */
    evtcod lstmov;         /* player move type */
    char   scrsav[SCRNUM]; /* screen score counter */
    int    scrlft;         /* units of score left to add */
    int    scrloc;         /* location of score digits */
    int    fblink;         /* crash blinker */
    int    crash;          /* crash occurred flag */

    game();                        /* constructor */
    ~game();                       /* destuctor */

    /* event callbacks */
    int evleft(void);              /* left arrow */
    int evright(void);             /* right arrow */
    int evup(void);                /* up arrow */
    int evdown(void);              /* down arrow */
    int evjoymov(int j, int x, int y, int z); /* joystick move */
    int evtim(int t);              /* timer fires */

    /* additional methods */
    void clrscn(void);             /* clear and format game screen */
    int randn(int limit);          /* find random number */
    void plctrg(void);             /* place game goals */
    void nxtscr(void);             /* increment score counter */
    void movesnake(evtcod usrmov); /* move snake to location */
    void restart(void);            /* restart new game */
    void blink(void);              /* blink crashed snake head */
    int evtrst(void);              /* check event is restart */

};

/*******************************************************************************

Event direction handlers

Called on arrow keys, moves the snake in the direction indicated.

*******************************************************************************/

int game::evleft(void) { movesnake(etleft); return (1); }
int game::evright(void) { movesnake(etright); return (1); }
int game::evup(void) { movesnake(etup); return (1); }
int game::evdown(void) { movesnake(etdown); return (1); }

/*******************************************************************************

Joystick handler

Called on joystick events, sets up the automatic move so that the next move
goes in the joystick indicated direction.

*******************************************************************************/

int game::evjoymov(int j, int x, int y, int z)

{

    /* change joystick to default move directions */
    if (x > INT_MAX/10) lstmov = etright;
    else if (x < -INT_MAX/10) lstmov = etleft;
    else if (y > INT_MAX/10) lstmov = etdown;
    else if (y < -INT_MAX/10) lstmov = etup;

    return (TRUE); /* set handled */

}

/*******************************************************************************

Timer handler

Called on timer events. We handle only timer 1 here, the automatic move timer.

*******************************************************************************/

int game::evtim(int t)

{

    int handled;

    handled = FALSE; /* set not handled */
    if (t == 1) { /* timer 1 */

        movesnake(lstmov); /* move the same as last */
        handled = TRUE; /* set we handled the event */

    }

    return (handled); /* return state of handled */

}

/*******************************************************************************

Check current event is restart

Restart is ordered by function key 1.

*******************************************************************************/

int game::evtrst(void)

{

    return (er.etype == etfun && er.fkey == 1);

}

/*******************************************************************************

Clear screen

Places the banner at the top of screen, then clears and sets the border on the
screen below. This is done in top to bottom order (no skipping about) to avoid
any text mixing with characters already on the screen (looks cleaner). This is
a concern because the screen clear is not quite instantaineous.

*******************************************************************************/

void game::clrscn(void)

{

    int y; /* index y */
    int x; /* index x */

    cout << '\f'; /* clear display screen */
    for (x = 1; x <= maxx(); x++) /* clear image */
        for (y = 1; y <= maxy(); y++) writeimage(x, y, ' ');
    /* place top */
    for (x = 1; x <= maxx(); x++) writescreen(x, 1, '*');
    /* place sides */
    for (y = 2; y <= maxy()-1; y++) { /* lines */

        writescreen(1, y, '*'); /* place left border */
        writescreen(maxx(), y, '*'); /* place right border */

    }
    /* place bottom */
    for (x = 1; x <= maxx(); x++) writescreen(x, maxy(), '*');
    /* size and place banners */
    wrtcen(1, " -> FUNCTION 1 RESTARTS <-   SCORE - 0000 ", &x);
    scrloc = x+38;
    wrtcen(maxy(), " SNAKE VS. 2.0 ", &x);

}

/*******************************************************************************

Find random number

Find random number between 0 and N.

*******************************************************************************/

int game::randn(int limit)

{

    return (long)limit*rand()/RAND_MAX;

}

/*******************************************************************************

Place target

Places a digit on the screen for use as a target. Both the location of the
target and it's value (0-9) are picked at random. Multiple tries are used to
avoid colisions.

*******************************************************************************/

void game::plctrg(void)

{

    int x; /* index x */
    int y; /* index y */
    char c;

    do { /* pick postions and check if the're free */

        /* find x, y locations, not on a border using
           a zero - n random function */
        y = randn(maxy()-2)+2;
        x = randn(maxx()-2)+2;
        c = readimage(x, y); /* get character at position */

    } while (c != ' '); /* area is unoccupied */
    /* place target integer */
    writescreen(x, y, randn(9)+'1');

}

/*******************************************************************************

Next score

Increments the displayed score counter. This overflow is not checked. Note that
the 'scrloc' global constant tells us where to place the score on screen, and
scrnum indicates the number of score digits.

********************************************************************************/

void game::nxtscr(void)

{

    scrinx  i;     /* score save index */
    int carry; /* carry out for addition */

    i = SCRNUM-1; /* index LSD */
    carry = TRUE; /* initalize carry */
    do { /* process digit add */

        if (scrsav[i] == '9') scrsav[i] = '0'; /* carry out digit */
        else {

            scrsav[i]++; /* add single turnover */
            carry = FALSE; /* stop */

        }
        i--; /*next digit */

   } while (i >= 0 && carry); /* last digit is processed, no digit carry */
   /* place score on screen */
   for (i = 0; i < SCRNUM; i++) writescreen(scrloc+i, 1, scrsav[i]);

}

/*******************************************************************************

Move snake

Since this game is pretty much solitary, the movement of the snake (activated by
a player or automatically) evokes most game behavor.

A move character is accepted, the new position calculated, and the following may
happen:

    1. The new position is inside a wall or border (game terminates, user loss).

    2. The new position crosses the snake itself (same result).

    3. A score tolken is found. The score value is added to the 'bank' of
       accumulate score. The score is later removed from the bank one value at a
       time.

After the new position is found, the decision is make to 'grow' the snake (make
it longer by the new position), or 'move' the snake (eliminate the last position
opposite the new one).

*******************************************************************************/

void game::movesnake(evtcod usrmov)

{

    sninx sn; /* index for snake array */
    char  c;
    int   x;  /* index x */
    int   y;  /* index y */

    if ((usrmov == etdown || usrmov == etup || usrmov == etleft ||
         usrmov == etright) && !crash) {

        x = snakel[sntop].scnx; /* save present top */
        y = snakel[sntop].scny;
        switch (usrmov) {

            case etdown:  y++; break; /* move down */
            case etup:    y--; break; /* move up */
            case etleft:  x--; break; /* move left */
            case etright: x++; break; /* move right */
            default:;

        }
        /* if we are directly backing up into ourselves, ignore the move */
        if (sntop == 0 || x != snakel[sntop-1].scnx ||
                          y != snakel[sntop-1].scny) {

            c = readimage(x, y); /* load new character */
            /* check terminate */
            if (y == 1 || y == maxy() || x == 1 || x == maxx() ||
                (c != ' ' && !isdigit(c))) {

                crash = TRUE; /* set crash occurred */
                goto terminate; /* exit */

            }
            writescreen(x, y, '@'); /* place new head */
            if (isdigit(c)) {

                plctrg(); /* place new target */
                /* set digit score */
                scrlft += c-'0';

            }
            if (scrlft != 0) {

                sntop++; /* 'grow' snake */
                if (sntop >= MAXSN) { /* snake to big */

                    crash = TRUE; /* set crash occurred */
                    goto terminate; /* exit */

                }
                nxtscr(); /* increment score */
                scrlft--; /* decrement score to add */

            } else {

                writescreen(snakel[0].scnx, snakel[0].scny, ' ');
                for (sn = 0; sn < sntop; sn++) /* copy old positions */
                    snakel[sn] = snakel[sn+1];

            }
            snakel[sntop].scnx = x; /* update coordinates */
            snakel[sntop].scny = y;
            lstmov = usrmov; /* set the last move */

        }

    }

    terminate: ; /* terminate move */

}

/*******************************************************************************

Restart game

Restarts the game. Clears the screen and redraws. Clears score, places starting
snake,

*******************************************************************************/

void game::restart(void)

{

    int i; /* index */

    do {

        scrlft = 0; /* clear score add count */
        clrscn();
        snakel[0].scnx = maxx()/2; /* set snake position middle */
        snakel[0].scny = maxy()/2;
        sntop = 0; /* set top snake character */
        writescreen(maxx()/2, maxy()/2, '@'); /* place snake */
        timcnt = TIMMAX;
        for (i = 0; i < SCRNUM; i++) scrsav[i] = '0'; /* zero score */
        nxtscr();
        lstmov = etchar; /* set move to invalid */
        /* now wait for the user to hit a key */
        event(); /* get the next event, without timers */

    } while (evtrst()); /* user hits restart */
    plctrg(); /* place starting target */
    crash = FALSE; /* set no crash occurred */

}

/*******************************************************************************

Blink snake head

When the snake crashes, it blinks until the user exits or hits restart.

*******************************************************************************/

void game::blink(void)

{

    int  tx, ty;  /* location of snake */
    int  restart; /* restart requested flag */

    tx = snakel[sntop].scnx;
    ty = snakel[sntop].scny;
    /* blink the head off and on (so that snakes behind us won't run into
       us) */
    fblink = FALSE; /* clear crash blinker */
    restart = FALSE; /* set no restart */
    do { /* blink cycles */

        /* wait for an interesting event */
        do { event(); }
        while (er.etype != ettim && er.etype != etterm && !evtrst());
        if (evtrst()) restart = TRUE; /* restart game */
        else
            /* must be timer */
            if (er.timnum == 2) { /* blink cycle */

            if (fblink) /* turn back on */
                writescreen(tx, ty, '@');
            else /* turn off */
                writescreen(tx, ty, ' ');
            fblink = !fblink; /* invert blinker status */

        }

    } while (!restart); /* forever */

}

/*******************************************************************************

Initialize game

Checks the screen size is within limits, moves to screen buffer 2, then removes
the cursor, automatic mode, places the background color, and starts the move and
blink timers.

*******************************************************************************/

game::game()

{

    if (maxx() > MAXSCN || maxy() > MAXSCN) {

        clog << "*** Error: Screen exceeds maximum size" << endl;
        exit(1);

    }
    select(2, 2); /* switch screens */
    curvis(FALSE); /* remove drawing cursor */
    autom(FALSE); /* remove automatic scrolling */
    bcolor(cyan); /* on cyan background */
    timer(1, TIMMAX, TRUE); /* set move timer */
    timer(2, BLNTIM, TRUE); /* set blinker timer */

}

/*******************************************************************************

Deinitialize game

Restores the drawing cursor, automatic mode, and flips the screen back to screen
1.

*******************************************************************************/

game::~game()

{

    curvis(TRUE); /* restore drawing cursor */
    autom(TRUE); /* restore automatic scrolling */
    select(1, 1); /* back to original screen */

}

/*******************************************************************************

Main program

Various set-ups are performed, then the move loop is activated. The user is
given n chances in the loop to enter a move character (and therefore a certain
time), after which the snake moves automatically in the same direction as it
last moved. This, of course, requires that the user have moved before the
game starts! This problem is handled by requiring a user move to start the play.
Besides the direction keys, the user has avalible restart and cancel game keys.

*******************************************************************************/

int main(void) /* snake */

{

    game gi;      /* game object */
    int  restart; /* restart flag */

    do { /* game */

        do { /* restarts */

            gi.restart(); /* restart the game */
            restart = FALSE; /* set no restart */
            do { /* game loop */

                gi.event(); /* get next event */
                if (gi.evtrst()) restart = TRUE; /* start new game */

            } while (!gi.crash && !restart); /* we crash into an object */
            /* not a voluntary cancel, must have *** crashed *** */
            if (gi.crash) gi.blink(); /* blink snake head */

        } while (restart); /* loop restarts */

    } while (1); /* until etterm event */

}