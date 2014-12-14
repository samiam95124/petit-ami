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

static int x, y, lx, ly, tx, ty, dx, dy;
static char c;
static int top, bottom, lside, rside; /* borders */
static enum { dup, ddown, dleft, dright } direction; /* writting direction */
static int count, t1, t2, delay, minlen;   /* minimum direction, x or y */
static evtrec er;   /* event record */
static int i, b, tc, clk, cnt;
static FILE *tf;   /* test file */
static char tf_NAME[_FNSIZE];


/* draw box */

static void box(int sx, int sy, int ex, int ey, char c)
{

    int x, y;

    /* top */
    cursor(stdout, sx, sy);
    for (x = sx; x <= ex; x++) putchar(c);
    /* bottom */
    cursor(stdout, sx, ey);
    for (x = sx; x <= ex; x++) putchar(c);
    /* left */
    for (y = sy; y <= ey; y++) { cursor(stdout, sx, y); putchar(c); }
    /* right */
    for (y = sy; y <= ey; y++) { cursor(stdout, ex, y); putchar(c); }

}


static jmp_buf _JL99;


/* wait time in 100 microseconds */

static void wait(int t)
{

    timer(stdout, 1, t, 0);
    do { event(stdin, &er); } while (er.etype != ettim && er.etype != etterm);
    if (er.etype == etterm) { intjmp(_JL99, 1); }

}


/* wait return to be pressed, or handle terminate */

static void waitnext(void)
{

    do { event(stdin, &er); } while (er.etype != etenter && er.etype != etterm);
    if (er.etype == etterm) { intjmp(_JL99, 1); }

}


static void timetest(void)
{

    int i, t, et, total, max, min;
    evtrec er;

    printf("Timer test, measuring minimum timer resolution, 100 samples\n\n");
    max = 0;
    min = INT_MAX;
    for (i = 1; i <= 100; i++) {

        t = clock();
        timer(stdout, 1, 1, 0);
        do { putchar('*'); event(stdin, &er); } while (er.etype != ettim);
        et = elapsed(t);
        total += elapsed(t);
        if (et > max) max = et;
        if (et < min) min = et;

    }
    printf("\n");
    printf("\n");
    printf("Average time was: %ld00 Microseconds\n", total / 100);
    printf("Minimum time was: %ld00 Microseconds\n", min);
    printf("Maximum time was: %ld00 Microseconds\n", max);
    printf("This timer supports frame rates up to %ld", 10000 / (total / 100));
    printf(" frames per second\n");
    t = clock();
    timer(stdout, 1, 10000, 0);
    do { event(stdin, &er); } while (er.etype != ettim);
    printf("1 second time, was: %ld00 Microseconds\n", elapsed(t));
    printf("\n");
    printf("30 seconds of 1 second ticks:\n");
    printf("\n");
    for (i = 1; i <= 30; i++) {

        timer(stdout, 1, 10000, 0);
        do { event(stdin, &er); } while (er.etype != ettim && er.etype != etterm);
        if (er.etype == etterm) intjmp(_JL99, 1);
        putchar('.');

    }

}


/* plot joystick on screen */

static void plotjoy(int line, int joy)
{
    int i, x;
    double r;
    int FORLIM;


    cursor(stdout, 1, line);
    FORLIM = maxx(stdout);
    for (i = 1; i <= FORLIM; i++)   /* clear line */
    putchar(' ');
    if (joy < 0) {  /* plot left */
    r = labs(joy);
    x = maxx(stdout) / 2 - (int)floor(r * (maxx(stdout) / 2) / int_MAX + 0.5);
    cursor(stdout, x, line);
    while (x <= maxx(stdout) / 2) {
    putchar('*');
    x++;

    }


    return;
    }

    r = joy;
    x = (int)floor(r * (maxx(stdout) / 2) / int_MAX + maxx(stdout) / 2 + 0.5);
    i = maxx(stdout) / 2;
    cursor(stdout, i, line);
    while (i <= x) {
    putchar('*');
    i++;
    /* plot right */



    }


}


/* print centered string */

static void prtcen(int y, char *s)
{
    cursor(stdout, maxx(stdout) / 2 - s / 2, y);
    fputs(s, stdout);

}


/* print center banner string */

static void prtban(char *s)
{
    int i, FORLIM;


    cursor(stdout, maxx(stdout) / 2 - s / 2 - 1, maxy(stdout) / 2 - 1);
    FORLIM = strlen(s) + 2;
    for (i = 1; i <= FORLIM; i++)
    putchar(' ');
    cursor(stdout, maxx(stdout) / 2 - s / 2 - 1, maxy(stdout) / 2);
    putchar(' ');
    prtcen(maxy(stdout) / 2, s);
    putchar(' ');
    cursor(stdout, maxx(stdout) / 2 - s / 2 - 1, maxy(stdout) / 2 + 1);
    FORLIM = strlen(s) + 2;
    for (i = 1; i <= FORLIM; i++)
    putchar(' ');

}


main(int argc, char *argv[])
{
    int FORLIM, FORLIM1;


    if (setjmp(_JL99))
    goto _L99;
    tf = NULL;
    select(stdout, 2, 2);   /* move off the display buffer */
    /* set black on white text */
    fcolor(stdout, black);
    bcolor(stdout, white);
    printf("\f");
    curvis(stdout, 0);
    prtban("Terminal mode screen test vs. 1.0");
    prtcen(maxy(stdout), "Press return to continue");
    waitnext();
    printf("\f");   /* clear screen */
    printf("Screen size: x -> %ld y -> %ld\n\n", maxx(stdout), maxy(stdout));
    printf("Number of joysticks: %d\n", joystick(stdout));
    FORLIM = joystick(stdout);
    for (i = 1; i <= FORLIM; i++) {
    printf("\nNumber of axes on joystick: %ld is: %d\n",
           i, joyaxis(stdout, i));
    printf("Number of buttons on joystick: %ld is: %d\n",
           i, joybutton(stdout, i));

    }

    printf("\nNumber of mice: %d\n", mouse(stdout));
    FORLIM = mouse(stdout);
    for (i = 1; i <= FORLIM; i++)
    printf("\nNumber of buttons on mouse: %ld is: %d\n",
           i, mousebutton(stdout, i));


    prtcen(maxy(stdout), "Press return to continue");
    waitnext();
    printf("\f");
    timetest();
    prtcen(maxy(stdout), "Press return to continue");
    waitnext();
    printf("\f");
    curvis(stdout, 1);
    printf("Cursor should be [on ], press return ->");
    waitnext();
    curvis(stdout, 0);
    printf("\\crCursor should be [off], press return ->");
    waitnext();
    curvis(stdout, 1);
    printf("\\crCursor should be [on ], press return ->");
    waitnext();
    curvis(stdout, 0);
    printf("\n\n");
    prtcen(maxy(stdout),
           "Press return to start test (and to pass each pattern)");
    waitnext();

    /* ************************* Test last line problem ************************ */

    printf("\f");
    curvis(stdout, 0);   /* remove cursor */
    auto_(stdout, 0);   /* turn off auto scroll */
    prtcen(1, "Last line blank out test");
    cursor(stdout, 1, 3);
    printf("If this terminal is not capable of showing the last character on\n");
    printf("the last line, the \"*\" character pointed to by the arrow below\n");
    printf("will not appear (probally blank). This should be noted for each\n");
    printf("of the following test patterns.\n");
    cursor(stdout, 1, maxy(stdout));
    FORLIM = maxx(stdout) - 2;
    for (i = 1; i <= FORLIM; i++)
    putchar('-');
    printf(">*");
    waitnext();

    /* ************************** Cursor movements test ************************ */

    /* First, do it with automatic scrolling on. The pattern will rely on scroll
       up, down, left wrap and right wrap working correctly. */

    printf("\f");
    auto_(stdout, 1);   /* set auto on */
    curvis(stdout, 0);   /* remove cursor */
    /* top of left lower */
    cursor(stdout, 1, maxy(stdout));
    printf("\\\\/");
    /* top of right lower, bottom of left lower, and move it all up */
    cursor(stdout, maxx(stdout) - 1, maxy(stdout));
    printf("\\\\//\\\\");
    /* finish right lower */
    up(stdout);
    left(stdout);
    left(stdout);
    left(stdout);
    left(stdout);
    down(stdout);
    down(stdout);
    printf("/\\\\");
    /* now move it back down */
    home(stdout);
    left(stdout);
    /* upper left hand cross */
    cursor(stdout, 1, 1);
    printf("\\\\/");
    cursor(stdout, maxx(stdout), 1);
    right(stdout);
    printf("/\\\\");
    /* upper right hand cross */
    cursor(stdout, maxx(stdout) - 1, 2);
    printf("/\\\\");
    cursor(stdout, 1, 2);
    left(stdout);
    left(stdout);
    printf("\\\\/");
    /* test delete works */
    prtcen(1, "BARK!");
    del(stdout);
    del(stdout);
    del(stdout);
    del(stdout);
    del(stdout);
    prtcen(maxy(stdout) / 2 - 1, "Cursor movements test, automatic scroll ON");
    prtcen(maxy(stdout) / 2 + 1, "Should be a double line X in each corner");
    waitnext();

    /* Now do it with automatic scrolling off. The pattern will rely on the
       ability of the cursor to go into "negative" space. */

    printf("\f");
    auto_(stdout, 0);   /* disable automatic screen scroll/wrap */
    /* upper left */
    home(stdout);
    printf("\\\\/");
    up(stdout);
    left(stdout);
    left(stdout);
    left(stdout);
    left(stdout);
    down(stdout);
    down(stdout);
    right(stdout);
    right(stdout);
    printf("/\\\\");
    /* upper right */
    cursor(stdout, maxx(stdout) - 1, 1);
    printf("\\\\/");
    down(stdout);
    del(stdout);
    del(stdout);
    printf("/\\\\");
    /* lower left */
    cursor(stdout, 1, maxy(stdout));
    printf("/\\\\");
    down(stdout);
    left(stdout);
    left(stdout);
    left(stdout);
    up(stdout);
    up(stdout);
    right(stdout);
    printf("\\\\/");
    /* lower right */
    cursor(stdout, maxx(stdout), maxy(stdout) - 1);
    putchar('/');
    left(stdout);
    left(stdout);
    printf("\\\\");
    down(stdout);
    del(stdout);
    printf("/\\\\");
    prtcen(maxy(stdout) / 2 - 1,
           "Cursor movements test, automatic scroll OFF");
    prtcen(maxy(stdout) / 2 + 1, "Should be a double line X in each corner");
    waitnext();

    /* **************************** Scroll cursor test ************************* */

    printf("\f");
    curvis(stdout, 1);
    prtcen(maxy(stdout) / 2, "Scroll cursor test, cursor should be here ->");
    up(stdout);
    scroll(stdout, 0, 1);
    waitnext();
    curvis(stdout, 0);

    /* ******************************* Row ID test ***************************** */

    printf("\f");
    /* perform row id test */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++)   /* output characters */
    putchar(c);
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }

    prtban("Row ID test, all rows should be numbered");
    waitnext();

    /* *************************** Collumn ID test ***************************** */

    printf("\f");
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++) {
    putchar(c);   /* output characters */
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }


    }

    prtban("Collumn ID test, all collumns should be numbered");
    waitnext();

    /* ****************************** Fill test ******************************** */

    printf("\f");
    c = '\0';   /* initalize character value */
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++) {
    if (c >= ' ' && c != '\177')
    putchar(c);
    else
    printf("\\\\");
    if (c != '\177')
    c++;   /* next character */
    else {
    c = '\0';   /* start over */

    }
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
    bottom = maxy(stdout);
    lside = 2;
    rside = maxx(stdout);
    direction = ddown;   /* start down */
    t1 = maxx(stdout);
    t2 = maxy(stdout);
    tc = 0;
    FORLIM = t1 * t2;
    for (count = 1; count <= FORLIM; count++) {
    /* for all screen characters */
    cursor(stdout, x, y);   /* place character */
    putchar('*');
    tc++;
    if (tc >= 10) {
    wait(50);   /* 50 milliseconds */
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

    prtcen(maxy(stdout) - 1, "                 ");
    prtcen(maxy(stdout), " Sidewinder test ");
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
    cursor(stdout, x, y);   /* place character */
    putchar('*');
    cursor(stdout, lx, ly);   /* place character */
    putchar(' ');
    lx = x;   /* set last */
    ly = y;
    x += dx;   /* find next x */
    y += dy;   /* find next y */
    tx = x;
    ty = y;
    if (x == 1 || tx == maxx(stdout))   /* find new dir x */
    dx = -dx;
    if (y == 1 || ty == maxy(stdout))   /* find new dir y */
    dy = -dy;
    /* slow this down */

    wait(100);
    }

    prtcen(maxy(stdout) - 1, "                    ");
    prtcen(maxy(stdout), " Bouncing ball test ");
    waitnext();

    /* *************************** Attributes test ************************** */

    printf("\f");
    if (maxy(stdout) < 20)
    printf("Not enough lines for attributes test");
    else {
    blink(stdout, 1);
    printf("Blinking text\n");
    blink(stdout, 0);
    reverse(stdout, 1);
    printf("Reversed text\n");
    reverse(stdout, 0);
    underline(stdout, 1);
    printf("Underlined text\n");
    underline(stdout, 0);
    printf("Superscript ");
    superscript(stdout, 1);
    printf("text\n");
    superscript(stdout, 0);
    printf("Subscript ");
    subscript(stdout, 1);
    printf("text\n");
    subscript(stdout, 0);
    italic(stdout, 1);
    printf("Italic text\n");
    italic(stdout, 0);
    bold(stdout, 1);
    printf("Bold text\n");
    bold(stdout, 0);
    standout(stdout, 1);
    printf("Standout text\n");
    standout(stdout, 0);
    fcolor(stdout, red);
    printf("Red text\n");
    fcolor(stdout, green);
    printf("Green text\n");
    fcolor(stdout, blue);
    printf("Blue text\n");
    fcolor(stdout, cyan);
    printf("Cyan text\n");
    fcolor(stdout, yellow);
    printf("Yellow text\n");
    fcolor(stdout, magenta);
    printf("Magenta text\n");
    fcolor(stdout, black);
    bcolor(stdout, red);
    printf("Red background text\n");
    bcolor(stdout, green);
    printf("Green background text\n");
    bcolor(stdout, blue);
    printf("Blue background text\n");
    bcolor(stdout, cyan);
    printf("Cyan background text\n");
    bcolor(stdout, yellow);
    printf("Yellow background text\n");
    bcolor(stdout, magenta);
    printf("Magenta background text\n");
    bcolor(stdout, white);

    prtcen(maxy(stdout), "Attributes test");
    }

    waitnext();

    /* ***************************** Scrolling test **************************** */

    printf("\f");
    /* fill screen with row order data */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++)   /* output characters */
    putchar(c);
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }

    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    wait(200);
    scroll(stdout, 0, 1);
    }
    prtcen(maxy(stdout), "Scroll up");
    waitnext();
    printf("\f");
    /* fill screen with row order data */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++)   /* output characters */
    putchar(c);
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }

    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    wait(200);
    scroll(stdout, 0, -1);
    }
    prtcen(maxy(stdout), "Scroll down");
    waitnext();
    printf("\f");
    /* fill screen with collumn order data */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++) {
    putchar(c);   /* output characters */
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }


    }

    FORLIM = maxx(stdout);
    for (x = 1; x <= FORLIM; x++) {
    wait(200);
    scroll(stdout, 1, 0);
    }
    prtcen(maxy(stdout), "Scroll left");
    waitnext();
    printf("\f");
    /* fill screen with collumn order data */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++) {
    putchar(c);   /* output characters */
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }


    }

    FORLIM = maxx(stdout);
    for (x = 1; x <= FORLIM; x++) {
    wait(200);
    scroll(stdout, -1, 0);
    }
    /* find minimum direction, x or y */
    if (x < y)
    minlen = x;
    else
    minlen = y;
    prtcen(maxy(stdout), "Scroll right");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++) {
    putchar(c);   /* output characters */
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }


    }

    FORLIM = minlen;
    for (i = 1; i <= FORLIM; i++) {
    wait(200);
    scroll(stdout, 1, 1);
    }
    prtcen(maxy(stdout), "Scroll up/left");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++) {
    putchar(c);   /* output characters */
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }


    }

    FORLIM = minlen;
    for (i = 1; i <= FORLIM; i++) {
    wait(200);
    scroll(stdout, 1, -1);
    }
    prtcen(maxy(stdout), "Scroll down/left");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++) {
    putchar(c);   /* output characters */
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }


    }

    FORLIM = minlen;
    for (i = 1; i <= FORLIM; i++) {
    wait(200);
    scroll(stdout, -1, 1);
    }
    prtcen(maxy(stdout), "Scroll up/right");
    waitnext();
    printf("\f");
    /* fill screen with uni data */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++) {
    putchar(c);   /* output characters */
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }


    }

    FORLIM = minlen;
    for (i = 1; i <= FORLIM; i++) {
    wait(200);
    scroll(stdout, -1, -1);
    }
    prtcen(maxy(stdout), "Scroll down/right");
    waitnext();

    /* ******************************** Tab test ******************************* */

    printf("\f");
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    FORLIM1 = y;
    for (i = 1; i < FORLIM1; i++)
    printf("\\ht");
    printf(">Tab %3ld\n", y - 1);

    }

    prtcen(maxy(stdout), "Tabbing test");
    waitnext();

    /* ************************** Buffer switching test ************************ */

    printf("\f");
    for (b = 2; b <= 10; b++) {  /* prepare buffers */
    select(stdout, b, 2);   /* select buffer */
    /* write a shinking box pattern */
    box(b - 1, b - 1, maxx(stdout) - b + 2, maxy(stdout) - b + 2, '*');

    prtcen(maxy(stdout), "Buffer switching test");
    }

    for (i = 1; i <= 30; i++) {   /* flip buffers */
    for (b = 2; b <= 10; b++) {
    wait(300);
    select(stdout, 2, b);
    }
    }
    select(stdout, 2, 2);   /* restore buffer select */

    /* **************************** Writethrough test ************************** */

    printf("\f");
    prtcen(maxy(stdout), "File writethrough test");
    home(stdout);
    if (tf != NULL)
    tf = freopen(tf_NAME, "w", tf);
    else
    tf = fopen(tf_NAME, "w");
    if (tf == NULL)
    _EscIO2(FileNotFound, tf_NAME);
    fprintf(tf, "This is a test file\n");
    tf = freopen(tf_NAME, "r", tf);
    if (tf == NULL)
    _EscIO2(FileNotFound, tf_NAME);
    while (!P_eoln(tf)) {
    c = getc(tf);
    putchar(c);

    }

    fscanf(tf, "%*[^\n]");
    getc(tf);
    printf("\n\ns/b\n\n");
    printf("This is a test file\n");
    waitnext();

    /* ****************************** Joystick test **************************** */

    if (joystick(stdout) > 0) {  /* joystick test */
    printf("\f");
    prtcen(1, "Move the joystick(s) X, Y and Z, and hit buttons");
    prtcen(maxy(stdout), "Joystick test test");
    do {   /* gather joystick events */

    /* we do up to 4 joysticks */
    event(stdin, &er);
    if (er.etype == etjoymov) {  /* joystick movement */
    if (er.UU.U44.mjoyn == 1) {  /* joystick 1 */
    cursor(stdout, 1, 3);
    printf("joystick: %d x: %ld y: %ld z: %ld",
           er.UU.U44.mjoyn, er.UU.U44.joypx, er.UU.U44.joypy,
           er.UU.U44.joypz);
    plotjoy(4, er.UU.U44.joypx);
    plotjoy(5, er.UU.U44.joypy);
    plotjoy(6, er.UU.U44.joypz);

    }

    else if (er.UU.U44.mjoyn == 2) {  /* joystick 2 */

    cursor(stdout, 1, 7);
    printf("joystick: %d x: %ld y: %ld z: %ld",
           er.UU.U44.mjoyn, er.UU.U44.joypx, er.UU.U44.joypy,
           er.UU.U44.joypz);
    plotjoy(8, er.UU.U44.joypx);
    plotjoy(9, er.UU.U44.joypy);
    plotjoy(10, er.UU.U44.joypz);

    } else if (er.UU.U44.mjoyn == 3) {  /* joystick 3 */

    cursor(stdout, 1, 11);
    printf("joystick: %d x: %ld y: %ld z: %ld",
           er.UU.U44.mjoyn, er.UU.U44.joypx, er.UU.U44.joypy,
           er.UU.U44.joypz);
    plotjoy(11, er.UU.U44.joypx);
    plotjoy(12, er.UU.U44.joypy);
    plotjoy(13, er.UU.U44.joypz);

    } else if (er.UU.U44.mjoyn == 4) {  /* joystick 4 */


    cursor(stdout, 1, 14);
    printf("joystick: %d x: %ld y: %ld z: %ld",
           er.UU.U44.mjoyn, er.UU.U44.joypx, er.UU.U44.joypy,
           er.UU.U44.joypz);
    plotjoy(15, er.UU.U44.joypx);
    plotjoy(16, er.UU.U44.joypy);
    plotjoy(17, er.UU.U44.joypz);

    }
    }

    else if (er.etype == etjoyba) {  /* joystick button assert */

    if (er.UU.U42.ajoyn == 1) {  /* joystick 1 */
    cursor(stdout, 1, 18);
    printf("joystick: %d button assert:   %d",
           er.UU.U42.ajoyn, er.UU.U42.ajoybn);

    }

    else if (er.UU.U42.ajoyn == 2) {  /* joystick 2 */

    cursor(stdout, 1, 19);
    printf("joystick: %d button assert:   %d",
           er.UU.U42.ajoyn, er.UU.U42.ajoybn);

    } else if (er.UU.U42.ajoyn == 3) {  /* joystick 3 */

    cursor(stdout, 1, 20);
    printf("joystick: %d button assert:   %d",
           er.UU.U42.ajoyn, er.UU.U42.ajoybn);

    } else if (er.UU.U42.ajoyn == 4) {  /* joystick 4 */


    cursor(stdout, 1, 21);
    printf("joystick: %d button assert:   %d",
           er.UU.U42.ajoyn, er.UU.U42.ajoybn);

    }
    } else if (er.etype == etjoybd) {  /* joystick button deassert */


    if (er.UU.U43.djoyn == 1) {  /* joystick 1 */
    cursor(stdout, 1, 18);
    printf("joystick: %d button deassert: %d",
           er.UU.U43.djoyn, er.UU.U43.djoybn);

    }

    else if (er.UU.U43.djoyn == 2) {  /* joystick 2 */

    cursor(stdout, 1, 19);
    printf("joystick: %d button deassert: %d",
           er.UU.U43.djoyn, er.UU.U43.djoybn);

    } else if (er.UU.U43.djoyn == 3) {  /* joystick 3 */

    cursor(stdout, 1, 20);
    printf("joystick: %d button deassert: %d",
           er.UU.U43.djoyn, er.UU.U43.djoybn);

    } else if (er.UU.U43.djoyn == 4) {  /* joystick 4 */


    cursor(stdout, 1, 21);
    printf("joystick: %d button deassert: %d",
           er.UU.U43.djoyn, er.UU.U43.djoybn);

    }
    }
    } while (er.etype != etenter && er.etype != etterm);
    }



    /* **************************** Mouse test ********************************* */

    if (mouse(stdin) > 0) {  /* mouse test */
    printf("\f");
    prtcen(1, "Move the mouse, and hit buttons");
    prtcen(maxy(stdout), "Mouse test");
    do {   /* gather mouse events */

    /* we only one mouse, all mice equate to that (multiple controls) */
    event(stdin, &er);
    if (er.etype == etmoumov) {
    cursor(stdout, x, y);
    printf("          \n");
    cursor(stdout, er.UU.U40.moupx, er.UU.U40.moupy);
    x = curx(stdout);
    y = cury(stdout);
    printf("<- Mouse %d\n", er.UU.U40.mmoun);

    }

    /* blank out button status line */
    cursor(stdout, 1, maxy(stdout) - 2);
    FORLIM = maxx(stdout);
    for (i = 1; i <= FORLIM; i++)
    putchar(' ');
    if (er.etype == etmouba) {  /* mouse button assert */
    cursor(stdout, 1, maxy(stdout) - 2);
    printf("Mouse button assert, mouse: %d button: %d\n",
           er.UU.U38.amoun, er.UU.U38.amoubn);

    }

    if (er.etype == etmoubd) {  /* mouse button assert */
    cursor(stdout, 1, maxy(stdout) - 2);
    printf("Mouse button deassert, mouse: %d button: %d\n",
           er.UU.U39.dmoun, er.UU.U39.dmoubn);

    }


    } while (er.etype != etenter && er.etype != etterm);
    if (er.etype == etterm) {
    goto _L99;

    }
    }


    /* ********************** Character write speed test *********************** */

    printf("\f");
    clk = clock();   /* get reference time */
    c = '\0';   /* initalize character value */
    cnt = 0;   /* clear character count */
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++) {
    if (c >= ' ' && c != '\177')
    putchar(c);
    else
    printf("\\\\");
    if (c != '\177')
    c++;   /* next character */
    else
    c = '\0';
    /* start over */
    cnt++;   /* count characters */

    }


    }

    clk = elapsed(clk);   /* find elapsed time */
    printf("\f");
    printf("Character write speed: % .5E average seconds per character\n",
           (double)clk / cnt * 0.0001);
    waitnext();

    /* ************************** Scrolling speed test ************************* */

    printf("\f");
    /* fill screen so we aren't moving blanks (could be optimized) */
    c = '1';
    FORLIM = maxy(stdout);
    for (y = 1; y <= FORLIM; y++) {
    cursor(stdout, 1, y);   /* index start of line */
    FORLIM1 = maxx(stdout);
    for (x = 1; x <= FORLIM1; x++)   /* output characters */
    putchar(c);
    if (c != '9')
    c++;   /* next character */
    else {
    c = '0';   /* start over */

    }
    }

    prtban("Scrolling speed test");
    clk = clock();   /* get reference time */
    cnt = 0;   /* clear count */
    for (i = 1; i <= 1000; i++) {  /* scroll various directions */
    scroll(stdout, 0, -1);   /* up */
    scroll(stdout, -1, 0);   /* left */
    scroll(stdout, 0, 1);   /* down */
    scroll(stdout, 0, 1);   /* down */
    scroll(stdout, 1, 0);   /* right */
    scroll(stdout, 1, 0);   /* right */
    scroll(stdout, 0, -1);   /* up */
    scroll(stdout, 0, -1);   /* up */
    scroll(stdout, -1, 0);   /* left */
    scroll(stdout, 0, 1);   /* down */
    scroll(stdout, -1, -1);   /* up/left */
    scroll(stdout, 1, 1);   /* down/right */
    scroll(stdout, 1, 1);   /* down/right */
    scroll(stdout, -1, -1);   /* up/left */
    scroll(stdout, 1, -1);   /* up/right */
    scroll(stdout, -1, 1);   /* down/left */
    scroll(stdout, -1, 1);   /* down/left */
    scroll(stdout, 1, -1);   /* up/right */
    cnt += 19;   /* count all scrolls */

    }

    clk = elapsed(clk);   /* find elapsed time */
    printf("\f");
    printf("Scrolling speed: % .5E average seconds per scroll\n",
           (double)clk / cnt * 0.0001);
    waitnext();

    /* ************************** Buffer flip speed test ************************* */

    printf("\f");
    cnt = 0;   /* clear count */

    for (b = 2; b <= 10; b++) {  /* prepare buffers */
    select(stdout, b, 2);   /* select buffer */
    /* write a shinking box pattern */

    box(b - 1, b - 1, maxx(stdout) - b + 2, maxy(stdout) - b + 2, '*');
    }

    clk = clock();   /* get reference time */
    for (i = 1; i <= 1000; i++) {   /* flip buffers */
    for (b = 2; b <= 10; b++) {
    select(stdout, 2, b);
    cnt++;

    }

    }
    clk = elapsed(clk);   /* find elapsed time */
    select(stdout, 2, 2);   /* restore buffer select */
    printf("\f");
    printf("Buffer switch speed: % .5E average seconds per switch\n",
           (double)clk / cnt * 0.0001);
    waitnext();

_L99:   /* terminate */

    /* test complete */
    select(stdout, 1, 1);   /* back to display buffer */
    curvis(stdout, 1);   /* restore cursor */
    auto_(stdout, 1);   /* enable automatic screen wrap */
    printf("\nTest complete\n");
    if (tf != NULL)
    fclose(tf);
    exit(EXIT_SUCCESS);
}



/* End. */
