/*******************************************************************************
*                                                                             *
*                           SCREEN TEST PROGRAM                               *
*                                                                             *
*                    Copyright (C) 1997 Scott A. Moore                        *
*                                                                             *
* This program performs a reasonably complete test of common features in the  *
* terminal level standard.                                                    *
*                                                                             *
* Tests performed:                                                            *
*                                                                             *
* 1. Row id - number each row with a digit in turn. This test uncovers        *
* positioning errors.                                                         *
* 2. Collumn id - Same for collums.                                           *
* 3. Fill test - fills the screen with the printable ascii characters, and    *
* "elided" control characters. Tests ability to print standard ASCII set.     *
* 4. Sidewinder - Fills the screen starting from the edges in. Tests          *
* positioning.                                                                *
* 5. Bounce - A ball bounces off the walls for a while. Tests positioning.    *
* 6. Scroll - A pattern that is recognizable if shifted is written, then the  *
* display successively scrolled until blank, in each of four directions.      *
* Tests the scrolling ability.                                                *
*                                                                             *
* Notes:                                                                      *
*                                                                             *
* Should have speed tests adjust their length according to actual process     *
* time to prevent tests from taking too int on slow cpu/display.             *
*                                                                             *
* Benchmark results:                                                          *
*                                                                             *
* Windows console library (conlib):                                           *
*                                                                             *
* Character write speed: 0.000031 Sec. Per character.                         *
* Scrolling speed:       0.00144 Sec. Per scroll.                             *
* Buffer switch speed:   0.00143 Sec. per switch.                             *
*                                                                             *
* Windows graphical library (gralib):                                         *
*                                                                             *
* Character write speed: 0.0000075 Sec. Per character.                        *
* Scrolling speed:       0.000192 Sec. Per scroll.                            *
* Buffer switch speed:   0.000126 Sec. per switch.                            *
*                                                                             *
*******************************************************************************/

#include "terminal.h"
#include "services.h"

#include <setjmp.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int x, y, lx, ly, tx, ty, dx, dy;
static char c;
static int top, bottom, lside, rside; /* borders */
static enum { dup, ddown, dleft, dright } direction; /* writting direction */
static int count, t1, t2, delay, minlen;   /* minimum direction, x or y */
static pa_evtrec er;   /* event record */
static int i, b, tc, clk, cnt;
static FILE *tf;   /* test file */
static char tf_NAME[10/*_FNSIZE*/] = "testfile";


/* draw box */

static void box(int sx, int sy, int ex, int ey, char c)
{

    int x, y;

    /* top */
    pa_cursor(stdout, sx, sy);
    for (x = sx; x <= ex; x++) putchar(c);
    /* bottom */
    pa_cursor(stdout, sx, ey);
    for (x = sx; x <= ex; x++) putchar(c);
    /* left */
    for (y = sy; y <= ey; y++) { pa_cursor(stdout, sx, y); putchar(c); }
    /* right */
    for (y = sy; y <= ey; y++) { pa_cursor(stdout, ex, y); putchar(c); }

}


static jmp_buf _JL99;


/* wait time in 100 microseconds */

static void wait(int t)
{

    pa_evtrec er; /* event record */

    pa_timer(stdout, 1, t, 0);
    do { pa_event(stdin, &er);
    } while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) { longjmp(_JL99, 1); }

}


/* wait return to be pressed, or handle terminate */

static void waitnext(void)
{

    pa_evtrec er; /* event record */

    do { pa_event(stdin, &er);
    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) { longjmp(_JL99, 1); }

}


static void timetest(void)
{

    int i, t, et, total, max, min;
    pa_evtrec er;

    printf("Timer test, measuring minimum timer resolution, 100 samples\n\n");
    max = 0;
    min = INT_MAX;
    for (i = 1; i <= 100; i++) {

        t = pa_clock();
        pa_timer(stdout, 1, 1, 0);
        do { putchar('*'); pa_event(stdin, &er); } while (er.etype != pa_ettim);
        et = pa_elapsed(t);
        total += pa_elapsed(t);
        if (et > max) max = et;
        if (et < min) min = et;

    }
    printf("\n");
    printf("\n");
    printf("Average time was: %d00 Microseconds\n", total / 100);
    printf("Minimum time was: %d00 Microseconds\n", min);
    printf("Maximum time was: %d00 Microseconds\n", max);
    printf("This timer supports frame rates up to %d", 10000 / (total / 100));
    printf(" frames per second\n");
    t = pa_clock();
    pa_timer(stdout, 1, 10000, 0);
    do { pa_event(stdin, &er); } while (er.etype != pa_ettim);
    printf("1 second time, was: %d00 Microseconds\n", pa_elapsed(t));
    printf("\n");
    printf("30 seconds of 1 second ticks:\n");
    printf("\n");
    for (i = 1; i <= 30; i++) {

        pa_timer(stdout, 1, 10000, 0);
        do { pa_event(stdin, &er);
        } while (er.etype != pa_ettim && er.etype != pa_etterm);
        if (er.etype == pa_etterm) longjmp(_JL99, 1);
        putchar('.');

    }

}

/* plot joystick on screen */

static void plotjoy(int line, int joy)

{

    int i, x;
    double r;

    pa_cursor(stdout, 1, line);
    for (i = 1; i <= pa_maxx(stdout); i++) putchar(' '); /* clear line */
    if (joy < 0) {  /* plot left */

        r = labs(joy);
        x = pa_maxx(stdout)/2-floor(r*(pa_maxx(stdout)/2)/INT_MAX+0.5);
        pa_cursor(stdout, x, line);
        while (x <= pa_maxx(stdout) / 2) {

            putchar('*');
            x++;

        }

    } else { /* plot right */

        r = joy;
        x = (int)floor(r * (pa_maxx(stdout) / 2) / INT_MAX + pa_maxx(stdout) / 2 + 0.5);
        i = pa_maxx(stdout) / 2;
        pa_cursor(stdout, i, line);
        while (i <= x) {

            putchar('*');
            i++;

        }

    }

}

/* print centered string */

static void prtcen(int y, char *s)

{

    pa_cursor(stdout, pa_maxx(stdout)/2-strlen(s)/2, y);
    fputs(s, stdout);

}

/* print center banner string */

static void prtban(char *s)

{

    int i;

    pa_cursor(stdout, pa_maxx(stdout)/2-strlen(s)/2-1, pa_maxy(stdout)/2-1);
    for (i = 1; i <= strlen(s)+2; i++) putchar(' ');
    pa_cursor(stdout, pa_maxx(stdout)/2-strlen(s)/2-1, pa_maxy(stdout)/2);
    putchar(' ');
    prtcen(pa_maxy(stdout) / 2, s);
    putchar(' ');
    pa_cursor(stdout, pa_maxx(stdout)/2-strlen(s)/2-1, pa_maxy(stdout)/2+1);
    for (i = 1; i <= strlen(s); i++) putchar(' ');

}

int main(int argc, char *argv[])
{

    if (setjmp(_JL99))
    goto _L99;

    pa_select(stdout, 2, 2);   /* move off the display buffer */
    /* set black on white text */
    pa_fcolor(stdout, pa_black);
    pa_bcolor(stdout, pa_white);
    printf("\f");
    pa_curvis(stdout, 0);
    prtban("Terminal mode screen test vs. 1.0");
    prtcen(pa_maxy(stdout), "Press return to continue");
    waitnext();
    printf("\f");   /* clear screen */
    printf("Screen size: x -> %d y -> %d\n\n", pa_maxx(stdout), pa_maxy(stdout));
    printf("Number of joysticks: %d\n", pa_joystick(stdout));
    for (i = 1; i <= pa_joystick(stdout); i++) {

        printf("\n");
        printf("Number of axes on joystick: %d is: %d\n", i,
            pa_joyaxis(stdout, i));
        printf("Number of buttons on joystick: %d is: %d\n", i,
            pa_joybutton(stdout, i));

    }
    printf("\n");
    printf("Number of mice: %d\n", pa_mouse(stdout));
    for (i = 1; i <= pa_mouse(stdout); i++) {

        printf("\n");
        printf("\nNumber of buttons on mouse: %d is: %d\n", i,
            pa_mousebutton(stdout, i));

    }
    prtcen(pa_maxy(stdout), "Press return to continue");
    waitnext();
    printf("\f");
    timetest();
    prtcen(pa_maxy(stdout), "Press return to continue");
    waitnext();
    printf("\f");
    pa_curvis(stdout, 1);
    printf("Cursor should be [on ], press return ->");
    waitnext();
    pa_curvis(stdout, 0);
    printf("\rCursor should be [off], press return ->");
    waitnext();
    pa_curvis(stdout, 1);
    printf("\rCursor should be [on ], press return ->");
    waitnext();
    pa_curvis(stdout, 0);
    printf("\n");
    printf("\n");
    prtcen(pa_maxy(stdout),
           "Press return to start test (and to pass each pattern)");
    waitnext();

    /* ************************* Test last line problem ************************ */

    printf("\f");
    pa_curvis(stdout, 0); /* remove cursor */
    pa_auto(stdout, 0); /* turn off auto scroll */
    prtcen(1, "Last line blank out test");
    pa_cursor(stdout, 1, 3);
    printf("If this terminal is not capable of showing the last character on\n");
    printf("the last line, the \"*\" character pointed to by the arrow below\n");
    printf("will not appear (probally blank). This should be noted for each\n");
    printf("of the following test patterns.\n");
    pa_cursor(stdout, 1, pa_maxy(stdout));
    for (i = 1; i <= pa_maxx(stdout)-2; i++) putchar('-');
    printf(">*");
    waitnext();

    /* ************************** Cursor movements test ************************ */

    /* First, do it with automatic scrolling on. The pattern will rely on scroll
       up, down, left wrap and right wrap working correctly. */

    printf("\f");
    pa_auto(stdout, 1);   /* set auto on */
    pa_curvis(stdout, 0);   /* remove cursor */
    /* top of left lower */
    pa_cursor(stdout, 1, pa_maxy(stdout));

    printf("\\/");
    /* top of right lower, bottom of left lower, and move it all up */
    pa_cursor(stdout, pa_maxx(stdout) - 1, pa_maxy(stdout));
    printf("\\//\\");
    /* finish right lower */
    pa_up(stdout);
    pa_left(stdout);
    pa_left(stdout);
    pa_left(stdout);
    pa_left(stdout);
    pa_down(stdout);
    pa_down(stdout);
    printf("/\\");
    /* now move it back down */
    pa_home(stdout);
    pa_left(stdout);
    /* upper left hand cross */
    pa_cursor(stdout, 1, 1);
    printf("\\/");
    pa_cursor(stdout, pa_maxx(stdout), 1);
    pa_right(stdout);
    printf("/\\");
    /* upper right hand cross */
    pa_cursor(stdout, pa_maxx(stdout) - 1, 2);
    printf("/\\");
    pa_cursor(stdout, 1, 2);
    pa_left(stdout);
    pa_left(stdout);
    printf("\\/");
    /* test delete works */
    prtcen(1, "BARK!");
    pa_del(stdout);
    pa_del(stdout);
    pa_del(stdout);
    pa_del(stdout);
    pa_del(stdout);
    prtcen(pa_maxy(stdout)/2-1, "Cursor movements test, automatic scroll ON");
    prtcen(pa_maxy(stdout)/2+1, "Should be a double line X in each corner");
    waitnext();

    /* Now do it with automatic scrolling off. The pattern will rely on the
       ability of the cursor to go into "negative" space. */

    printf("\f");
    pa_auto(stdout, 0);   /* disable automatic screen scroll/wrap */
    /* upper left */
    pa_home(stdout);
    printf("\\/");
    pa_up(stdout);
    pa_left(stdout);
    pa_left(stdout);
    pa_left(stdout);
    pa_left(stdout);
    pa_down(stdout);
    pa_down(stdout);
    pa_right(stdout);
    pa_right(stdout);
    printf("/\\");
    /* upper right */
    pa_cursor(stdout, pa_maxx(stdout) - 1, 1);
    printf("\\/");
    pa_down(stdout);
    pa_del(stdout);
    pa_del(stdout);
    printf("/\\");
    /* lower left */
    pa_cursor(stdout, 1, pa_maxy(stdout));
    printf("/\\");
    pa_down(stdout);
    pa_left(stdout);
    pa_left(stdout);
    pa_left(stdout);
    pa_up(stdout);
    pa_up(stdout);
    pa_right(stdout);
    printf("\\/");
    /* lower right */
    pa_cursor(stdout, pa_maxx(stdout), pa_maxy(stdout) - 1);
    putchar('/');
    pa_left(stdout);
    pa_left(stdout);
    printf("\\\\");
    pa_down(stdout);
    pa_del(stdout);
    printf("/\\");
    prtcen(pa_maxy(stdout)/2-1,
           "Cursor movements test, automatic scroll OFF");
    prtcen(pa_maxy(stdout)/2+1, "Should be a double line X in each corner");
    waitnext();

    /* **************************** Scroll cursor test ************************* */

    printf("\f");
    pa_curvis(stdout, 1);
    prtcen(pa_maxy(stdout)/2, "Scroll cursor test, cursor should be here ->");
    pa_up(stdout);
    pa_scroll(stdout, 0, 1);
    waitnext();
    pa_curvis(stdout, 0);

    /* ******************************* Row ID test ***************************** */

    printf("\f");
    /* perform row id test */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++)   /* output characters */
        putchar(c);
        if (c != '9') c++;   /* next character */
        else c = '0';   /* start over */

    }
    prtban("Row ID test, all rows should be numbered");
    waitnext();

    /* *************************** Collumn ID test ***************************** */

    printf("\f");
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    prtban("Collumn ID test, all collumns should be numbered");
    waitnext();

    /* ****************************** Fill test ******************************** */

    printf("\f");
    c = '\0';   /* initalize character value */
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= pa_maxy(stdout); x++) {

            if (c >= ' ' && c != '\177') putchar(c);
            else printf("\\");
            if (c != '\177') c++;   /* next character */
            else c = '\0';   /* start over */

        }

    }
    prtban("Fill test, all printable characters should appear");
    waitnext();

    /* **************************** Sidewinder test **************************** */

    printf("\f");
    /* perform sidewinder */
    x = 1;   /* set origin */
    y = 1;
    top = 1;   /* set borders */
    bottom = pa_maxy(stdout);
    lside = 2;
    rside = pa_maxx(stdout);
    direction = ddown;   /* start down */
    t1 = pa_maxx(stdout);
    t2 = pa_maxy(stdout);
    tc = 0;
    for (count = 1; count <= t1*t2; count++) {

        /* for all screen characters */
        pa_cursor(stdout, x, y); /* place character */
        putchar('*');
        tc++;
        if (tc >= 10) {

            wait(50); /* 50 milliseconds */
            tc = 0;

        }
        switch (direction) {

            case ddown:
                y++;   /* next */
                if (y == bottom) {  /* change */

                    direction = dright;
                    bottom--;

                }
                break;

            case dright:
                x++;   /* next */
                if (x == rside) {

                    direction = dup;
                    rside--;

                }
                break;


            case dup:
                y--;
                if (y == top) {

                    direction = dleft;
                    top++;

                }
                break;


            case dleft:
                x--;
                if (x == lside) {

                    direction = ddown;
                    lside++;

                }
                break;


        }

    }
    prtcen(pa_maxy(stdout) - 1, "                 ");
    prtcen(pa_maxy(stdout), " Sidewinder test ");
    waitnext();

    /* *************************** Bouncing ball test ************************** */

    printf("\f");
    x = 10;   /* set origin */
    y = 20;
    lx = 10;   /* set last */
    ly = 20;
    dx = -1;   /* set initial directions */
    dy = -1;
    for (count = 1; count <= 1000; count++) {

        pa_cursor(stdout, x, y);   /* place character */
        putchar('*');
        pa_cursor(stdout, lx, ly);   /* place character */
        putchar(' ');
        lx = x;   /* set last */
        ly = y;
        x += dx;   /* find next x */
        y += dy;   /* find next y */
        tx = x;
        ty = y;
        if (x == 1 || tx == pa_maxx(stdout))   /* find new dir x */
        dx = -dx;
        if (y == 1 || ty == pa_maxy(stdout))   /* find new dir y */
        dy = -dy;
        /* slow this down */
        wait(100);

    }
    prtcen(pa_maxy(stdout)-1, "                    ");
    prtcen(pa_maxy(stdout), " Bouncing ball test ");
    waitnext();

    /* *************************** Attributes test ************************** */

    printf("\f");
    if (pa_maxy(stdout) < 20)
    printf("Not enough lines for attributes test");
    else {

        pa_blink(stdout, 1);
        printf("Blinking text\n");
        pa_blink(stdout, 0);
        pa_reverse(stdout, 1);
        printf("Reversed text\n");
        pa_reverse(stdout, 0);
        pa_underline(stdout, 1);
        printf("Underlined text\n");
        pa_underline(stdout, 0);
        printf("Superscript ");
        pa_superscript(stdout, 1);
        printf("text\n");
        pa_superscript(stdout, 0);
        printf("Subscript ");
        pa_subscript(stdout, 1);
        printf("text\n");
        pa_subscript(stdout, 0);
        pa_italic(stdout, 1);
        printf("Italic text\n");
        pa_italic(stdout, 0);
        pa_bold(stdout, 1);
        printf("Bold text\n");
        pa_bold(stdout, 0);
        pa_standout(stdout, 1);
        printf("Standout text\n");
        pa_standout(stdout, 0);
        pa_fcolor(stdout, pa_red);
        printf("Red text\n");
        pa_fcolor(stdout, pa_green);
        printf("Green text\n");
        pa_fcolor(stdout, pa_blue);
        printf("Blue text\n");
        pa_fcolor(stdout, pa_cyan);
        printf("Cyan text\n");
        pa_fcolor(stdout, pa_yellow);
        printf("Yellow text\n");
        pa_fcolor(stdout, pa_magenta);
        printf("Magenta text\n");
        pa_fcolor(stdout, pa_black);
        pa_bcolor(stdout, pa_red);
        printf("Red background text\n");
        pa_bcolor(stdout, pa_green);
        printf("Green background text\n");
        pa_bcolor(stdout, pa_blue);
        printf("Blue background text\n");
        pa_bcolor(stdout, pa_cyan);
        printf("Cyan background text\n");
        pa_bcolor(stdout, pa_yellow);
        printf("Yellow background text\n");
        pa_bcolor(stdout, pa_magenta);
        printf("Magenta background text\n");
        pa_bcolor(stdout, pa_white);
        prtcen(pa_maxy(stdout), "Attributes test");

    }
    waitnext();

    /* ***************************** Scrolling test **************************** */

    printf("\f");
    /* fill screen with row order data */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) putchar(c); /* output characters */
        if (c != '9') c++; /* next character */
        else c = '0'; /* start over */

    }
    for (y = 1; y <= pa_maxy(stdout); y++) { wait(200); pa_scroll(stdout, 0, 1); }
    prtcen(pa_maxy(stdout), "Scroll up");
    waitnext();
    printf("\f");
    /* fill screen with row order data */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) putchar(c); /* output characters */
        if (c != '9') c++; /* next character */
        else c = '0';   /* start over */

    }
    for (y = 1; y <= pa_maxy(stdout); y++) { wait(200); pa_scroll(stdout, 0, -1); }
    prtcen(pa_maxy(stdout), "Scroll down");
    waitnext();
    printf("\f");
    /* fill screen with collumn order data */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    for (x = 1; x <= pa_maxx(stdout); x++) { wait(200); pa_scroll(stdout, 1, 0); }
    prtcen(pa_maxy(stdout), "Scroll left");
    waitnext();
    printf("\f");
    /* fill screen with collumn order data */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++;   /* next character */
            else c = '0';   /* start over */

        }

    }
    for (x = 1; x <= pa_maxx(stdout); x++) { wait(200); pa_scroll(stdout, -1, 0); }
    /* find minimum direction, x or y */
    if (x < y) minlen = x; else minlen = y;
    prtcen(pa_maxy(stdout), "Scroll right");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) {

            putchar(c);   /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    for (i = 1; i <= minlen; i++) { wait(200); pa_scroll(stdout, 1, 1); }
    prtcen(pa_maxy(stdout), "Scroll up/left");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    for (i = 1; i <= minlen; i++) { wait(200); pa_scroll(stdout, 1, -1); }
    prtcen(pa_maxy(stdout), "Scroll down/left");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) {

            putchar(c); /* output characters */
            if (c != '9') c++; /* next character */
            else c = '0'; /* start over */

        }

    }
    for (i = 1; i <= minlen; i++) { wait(200); pa_scroll(stdout, -1, 1); }
    prtcen(pa_maxy(stdout), "Scroll up/right");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) {

            putchar(c);   /* output characters */
            if (c != '9') c++;   /* next character */
            else c = '0';   /* start over */

         }

    }
    for (i = 1; i <= minlen; i++) { wait(200); pa_scroll(stdout, -1, -1); }
    prtcen(pa_maxy(stdout), "Scroll down/right");
    waitnext();

    /* ******************************** Tab test ******************************* */

    printf("\f");
    for (y = 1; y <= pa_maxy(stdout); y++) {

        for (i = 1; i <= y-1; i++) printf("\t");
        printf(">Tab %3d\n", y-1);

    }
    prtcen(pa_maxy(stdout), "Tabbing test");
    waitnext();

    /* ************************** Buffer switching test ************************ */

    printf("\f");
    for (b = 2; b <= 10; b++) {  /* prepare buffers */

        pa_select(stdout, b, 2);   /* select buffer */
        /* write a shinking box pattern */
        box(b - 1, b-1, pa_maxx(stdout)-(b- 2), pa_maxy(stdout)-(b-2), '*');
        prtcen(pa_maxy(stdout), "Buffer switching test");

    }
    for (i = 1; i <= 30; i++) /* flip buffers */
        for (b = 2; b <= 10; b++) { wait(300); pa_select(stdout, 2, b); }
    pa_select(stdout, 2, 2);   /* restore buffer select */

    /* **************************** Writethrough test ************************** */

    printf("\f");
    prtcen(pa_maxy(stdout), "File writethrough test");
    pa_home(stdout);
    if (tf != NULL) tf = freopen(tf_NAME, "w", tf);
    else tf = fopen(tf_NAME, "w");
    if (tf == NULL) {

        fprintf(stderr, "*** File not found: %s\n", tf_NAME);
        exit(1);

    }
    fprintf(tf, "This is a test file\n");
    tf = freopen(tf_NAME, "r", tf);
    if (tf == NULL) {

        fprintf(stderr, "*** File not found: %s\n", tf_NAME);
        exit(1);

    }
    while ((c = getc(tf)) != '\n') {

        putchar(c);

    }
    while ((c = getc(tf)) != '\n');
    getc(tf);
    printf("s/b");
    printf("This is a test file\n");
    waitnext();

    /* ****************************** Joystick test **************************** */

    if (pa_joystick(stdout) > 0) {  /* joystick test */

        printf("\f");
        prtcen(1, "Move the joystick(s) X, Y and Z, and hit buttons");
        prtcen(pa_maxy(stdout), "Joystick test test");
        do {   /* gather joystick events */

            /* we do up to 4 joysticks */
            pa_event(stdin, &er);
            if (er.etype == pa_etjoymov) {  /* joystick movement */

                if (er.mjoyn == 1) {  /* joystick 1 */

                    pa_cursor(stdout, 1, 3);
                    printf("joystick: %d x: %d y: %d z: %d",
                           er.mjoyn, er.joypx, er.joypy, er.joypz);
                    plotjoy(4, er.joypx);
                    plotjoy(5, er.joypy);
                    plotjoy(6, er.joypz);

                } else if (er.mjoyn == 2) {  /* joystick 2 */

                    pa_cursor(stdout, 1, 7);
                    printf("joystick: %d x: %d y: %d z: %d",
                           er.mjoyn, er.joypx, er.joypy, er.joypz);
                    plotjoy(8, er.joypx);
                    plotjoy(9, er.joypy);
                    plotjoy(10, er.joypz);

                } else if (er.mjoyn == 3) {  /* joystick 3 */

                    pa_cursor(stdout, 1, 11);
                    printf("joystick: %d x: %d y: %d z: %d",
                           er.mjoyn, er.joypx, er.joypy, er.joypz);
                    plotjoy(11, er.joypx);
                    plotjoy(12, er.joypy);
                    plotjoy(13, er.joypz);

                } else if (er.mjoyn == 4) {  /* joystick 4 */

                    pa_cursor(stdout, 1, 14);
                    printf("joystick: %d x: %d y: %d z: %d",
                           er.mjoyn, er.joypx, er.joypy, er.joypz);
                    plotjoy(15, er.joypx);
                    plotjoy(16, er.joypy);
                    plotjoy(17, er.joypz);

                }

            } else if (er.etype == pa_etjoyba) {  /* joystick button assert */

                if (er.ajoyn == 1) {  /* joystick 1 */

                    pa_cursor(stdout, 1, 18);
                    printf("joystick: %d button assert:   %d",
                           er.ajoyn, er.ajoybn);

                } else if (er.ajoyn == 2) {  /* joystick 2 */

                    pa_cursor(stdout, 1, 19);
                    printf("joystick: %d button assert:   %d",
                           er.ajoyn, er.ajoybn);

                } else if (er.ajoyn == 3) {  /* joystick 3 */

                    pa_cursor(stdout, 1, 20);
                    printf("joystick: %d button assert:   %d",
                           er.ajoyn, er.ajoybn);

                } else if (er.ajoyn == 4) {  /* joystick 4 */

                    pa_cursor(stdout, 1, 21);
                    printf("joystick: %d button assert:   %d",
                           er.ajoyn, er.ajoybn);

                }

            } else if (er.etype == pa_etjoybd) {  /* joystick button deassert */

                if (er.djoyn == 1) {  /* joystick 1 */

                    pa_cursor(stdout, 1, 18);
                    printf("joystick: %d button deassert: %d",
                           er.djoyn, er.djoybn);

                } else if (er.djoyn == 2) {  /* joystick 2 */

                    pa_cursor(stdout, 1, 19);
                    printf("joystick: %d button deassert: %d",
                           er.djoyn, er.djoybn);

                } else if (er.djoyn == 3) {  /* joystick 3 */

                    pa_cursor(stdout, 1, 20);
                    printf("joystick: %d button deassert: %d",
                           er.djoyn, er.djoybn);

                } else if (er.djoyn == 4) {  /* joystick 4 */

                    pa_cursor(stdout, 1, 21);
                    printf("joystick: %d button deassert: %d",
                           er.djoyn, er.djoybn);

                }

            }

        } while (er.etype != pa_etenter && er.etype != pa_etterm);

    }

    /* **************************** Mouse test ********************************* */

    if (pa_mouse(stdin) > 0) {  /* mouse test */

        printf("\f");
        prtcen(1, "Move the mouse, and hit buttons");
        prtcen(pa_maxy(stdout), "Mouse test");
        do { /* gather mouse events */

            /* we only one mouse, all mice equate to that (multiple controls) */
            pa_event(stdin, &er);
            if (er.etype == pa_etmoumov) {

                pa_cursor(stdout, x, y);
                printf("          \n");
                pa_cursor(stdout, er.moupx, er.moupy);
                x = pa_curx(stdout);
                y = pa_cury(stdout);
                printf("<- Mouse %d\n", er.mmoun);

            }
            /* blank out button status line */
            pa_cursor(stdout, 1, pa_maxy(stdout)-2);
            for (i = 1; i <= pa_maxx(stdout); i++) putchar(' ');
            if (er.etype == pa_etmouba) {  /* mouse button assert */

                pa_cursor(stdout, 1, pa_maxy(stdout)-2);
                printf("Mouse button assert, mouse: %d button: %d\n",
                       er.amoun, er.amoubn);

            }
            if (er.etype == pa_etmoubd) {  /* mouse button assert */

                pa_cursor(stdout, 1, pa_maxy(stdout) - 2);
                printf("Mouse button deassert, mouse: %d button: %d\n",
                       er.dmoun, er.dmoubn);

            }


        } while (er.etype != pa_etenter && er.etype != pa_etterm);
        if (er.etype == pa_etterm) goto _L99;

    }


    /* ********************** Character write speed test *********************** */

    printf("\f");
    clk = pa_clock();   /* get reference time */
    c = '\0';   /* initalize character value */
    cnt = 0;   /* clear character count */
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y); /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++) {

            if (c >= ' ' && c != '\177')
            putchar(c);
            else
            printf("\\");
            if (c != '\177') c++; /* next character */
            else c = '\0'; /* start over */
            cnt++;   /* count characters */

        }


    }
    clk = pa_elapsed(clk);   /* find elapsed time */
    printf("\f");
    printf("Character write speed: % .5E average seconds per character\n",
           clk/cnt*0.0001);
    waitnext();

    /* ************************** Scrolling speed test ************************* */

    printf("\f");
    /* fill screen so we aren't moving blanks (could be optimized) */
    c = '1';
    for (y = 1; y <= pa_maxy(stdout); y++) {

        pa_cursor(stdout, 1, y);   /* index start of line */
        for (x = 1; x <= pa_maxx(stdout); x++)   /* output characters */
        putchar(c);
        if (c != '9') c++; /* next character */
        else c = '0'; /* start over */

    }
    prtban("Scrolling speed test");
    clk = pa_clock(); /* get reference time */
    cnt = 0; /* clear count */
    for (i = 1; i <= 1000; i++) { /* scroll various directions */

        pa_scroll(stdout, 0, -1); /* up */
        pa_scroll(stdout, -1, 0); /* left */
        pa_scroll(stdout, 0, 1); /* down */
        pa_scroll(stdout, 0, 1); /* down */
        pa_scroll(stdout, 1, 0); /* right */
        pa_scroll(stdout, 1, 0); /* right */
        pa_scroll(stdout, 0, -1); /* up */
        pa_scroll(stdout, 0, -1); /* up */
        pa_scroll(stdout, -1, 0); /* left */
        pa_scroll(stdout, 0, 1); /* down */
        pa_scroll(stdout, -1, -1); /* up/left */
        pa_scroll(stdout, 1, 1); /* down/right */
        pa_scroll(stdout, 1, 1); /* down/right */
        pa_scroll(stdout, -1, -1); /* up/left */
        pa_scroll(stdout, 1, -1); /* up/right */
        pa_scroll(stdout, -1, 1); /* down/left */
        pa_scroll(stdout, -1, 1); /* down/left */
        pa_scroll(stdout, 1, -1); /* up/right */
        cnt += 19; /* count all scrolls */

    }
    clk = pa_elapsed(clk);   /* find elapsed time */
    printf("\f");
    printf("Scrolling speed: % .5E average seconds per scroll\n", clk/cnt*0.0001);
    waitnext();

    /* ************************** Buffer flip speed test ************************* */

    printf("\f");
    cnt = 0;   /* clear count */

    for (b = 2; b <= 10; b++) {  /* prepare buffers */

        pa_select(stdout, b, 2);   /* select buffer */
        /* write a shinking box pattern */
        box(b - 1, b - 1, pa_maxx(stdout) - b + 2, pa_maxy(stdout) - b + 2, '*');

    }

    clk = pa_clock();   /* get reference time */
    for (i = 1; i <= 1000; i++) /* flip buffers */
    for (b = 2; b <= 10; b++) {

        pa_select(stdout, 2, b);
        cnt++;

    }
    clk = pa_elapsed(clk);   /* find elapsed time */
    pa_select(stdout, 2, 2);   /* restore buffer select */
    printf("\f");
    printf("Buffer switch speed: % .5E average seconds per switch\n",
           clk/cnt*0.0001);
    waitnext();

_L99: /* terminate */

    /* test complete */
    pa_select(stdout, 1, 1); /* back to display buffer */
    pa_curvis(stdout, 1);     /* restore cursor */
    pa_auto(stdout, 1);   /* enable automatic screen wrap */
    printf("\n");
    printf("Test complete\n");
    if (tf != NULL) fclose(tf);

}
