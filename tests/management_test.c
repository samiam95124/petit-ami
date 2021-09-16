/*******************************************************************************
*                                                                             *
*                     WINDOW MANAGEMENT TEST PROGRAM                          *
*                                                                             *
*                    Copyright (C) 2005 Scott A. Moore                        *
*                                                                             *
* Tests text and graphical windows management calls.                          *
*                                                                             *
*******************************************************************************/

/* base C defines */
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Petit-ami defines */
#include <localdefs.h>
#include <services.h>
#include <graphics.h>

#define OFF 0
#define ON 1

static jmp_buf terminate_buf;
static FILE*      win2;
static FILE*      win3;
static FILE*      win4;
static int        x, x2, y, y2;
static int        ox, oy;       /* original size of window */
static int        fb;           /* front/back flipper */
static pa_evtrec  er;
static pa_menuptr mp;           /* menu pointer */
static pa_menuptr ml;           /* menu list */
static pa_menuptr sm;           /* submenu list */
static int        sred;         /* state variables */
static int        sgreen;
static int        sblue;
static int        mincnt;       /* minimize counter */
static int        maxcnt;       /* maximize counter */
static int        nrmcnt;       /* normalize counter */
static int        i;
static int        xs, ys;
static int        cs;

/* wait return to be pressed, or handle terminate */

static void waitnext(void)

{

    pa_evtrec er; /* event record */

    do { pa_event(stdin, &er); }
    while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* wait return to be pressed, or handle terminate, while printing characters */

static void waitnextprint(void)

{

    pa_evtrec er; /* event record */

    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etchar)
            printf("Window: %d char: %c\n", er.winid, er.echar);

    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* print centered string */

static void prtcen(int y, const char* s)

{

   pa_cursor(stdout, (pa_maxx(stdout)/2)-(strlen(s)/2), y);
   printf("%s", s);

}

/* print centered string graphical */

static void prtceng(int y, const char* s)

{

   pa_cursorg(stdout, (pa_maxxg(stdout)/2)-(pa_strsiz(stdout, s)/2), y);
   printf("%s", s);

}

/* wait time in 100 microseconds */

static void wait(int t)

{

    pa_evtrec er;

    pa_timer(stdout, 1, t, FALSE);
    do { pa_event(stdin, &er);
    } while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* append a new menu entry to the given list */

static void appendmenu(pa_menuptr* list, pa_menuptr m)

{

    pa_menuptr lp;

    /* clear these links for insurance */
    m->next = NULL; /* clear next */
    m->branch = NULL; /* clear branch */
    if (!*list) *list = m; /* list empty, set as first entry */
    else { /* list non-empty */

        /* find last entry in list */
        lp = *list; /* index 1st on list */
        while (lp->next) lp = lp->next;
        lp->next = m; /* append at end */

    }

}

/* create menu entry */

static void newmenu(pa_menuptr* mp, int onoff, int oneof, int bar,
             int id, const string face)

{

    *mp = malloc(sizeof(pa_menurec));
    if (!*mp) pa_alert("mantst", "Out of memory");
    (*mp)->onoff = onoff;
    (*mp)->oneof = oneof;
    (*mp)->bar = bar;
    (*mp)->id = id;
    (*mp)->face = malloc(strlen(face));
    if (!*mp) pa_alert("mantst", "Out of memory");
    strcpy((*mp)->face, face);

}

/* draw a character grid */

static void chrgrid(void)

{

    int x, y;

    pa_fcolor(stdout, pa_yellow);
    y = 1;
    while (y < pa_maxyg(stdout)) {

        pa_line(stdout, 1, y, pa_maxxg(stdout), y);
        y = y+pa_chrsizy(stdout);

    }
    x = 1;
    while (x < pa_maxxg(stdout)) {

        pa_line(stdout, x, 1, x, pa_maxyg(stdout));
        x = x+pa_chrsizx(stdout);

    }
    pa_fcolor(stdout, pa_black);

}

/* display frame test */

static void frametest(const string s)

{

    pa_evtrec er;
    int       x, y;

    x = pa_maxxg(stdout); /* set size */
    y = pa_maxyg(stdout);
    do {

        pa_event(stdin, &er); /* get next event */
        if (er.etype == pa_etredraw) {

            putchar('\f');
            pa_fcolor(stdout, pa_cyan);
            pa_rect(stdout, 1, 1, x, y);
            pa_line(stdout, 1, 1, x, y);
            pa_line(stdout, 1, y, x, 1);
            pa_fcolor(stdout, pa_black);
            pa_binvis(stdout);
            puts(s);
            pa_bover(stdout);

        }
        if (er.etype == pa_etresize) {

            /* Save the new dimensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = pa_maxxg(stdout);
            y = pa_maxyg(stdout);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);

}

int main(void)

{

    if (setjmp(terminate_buf)) goto terminate;

    pa_auto(stdout, OFF);
    pa_curvis(stdout, OFF);
#if 0
    printf("Managed screen test vs. 0.1\n");
    printf("\n");
    pa_scnsiz(stdout, &x, &y);
    printf("Screen size character: x: %d y: %d\n", x, y);
    pa_scnsizg(stdout, &x, &y);
    printf("Screen size pixel: x: %d y: %d\n", x, y);
    printf("\n");
    pa_getsiz(stdout, &x, &y);
    printf("Window size character: x: %d y: %d\n", x, y);
    pa_getsizg(stdout, &ox, &oy);
    printf("Window size graphical: x: %d y: %d\n", ox, oy);
    printf("\n");
    printf("Client size character: x: %d y: %d\n", pa_maxx(stdout), pa_maxy(stdout));
    printf("Client size graphical: x: %d y: %d\n", pa_maxxg(stdout), pa_maxyg(stdout));
    printf("\n");
    printf("Hit return in any window to continue for each test\n");
    waitnext();

    /* ************************** Window titling test ************************** */

    pa_title(stdout, "This is a mangement test window");
    printf("The title bar of this window should read: This is a mangement test window\n");
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Window title test");
    waitnext();

    /* ************************** Multiple windows ************************** */

    putchar('\f');
    pa_curvis(stdout, ON);
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Multiple window test");
    pa_home(stdout);
    pa_auto(stdout, ON);
    printf("This is the main window");
    printf("\n");
    printf("Select back and forth between each window, and make sure the\n");
    printf("cursor follows\n");
    printf("\n");
    printf("Here is the cursor->");
    pa_openwin(&stdin, &win2, NULL, 2);
    fprintf(win2, "This is the second window\n");
    fprintf(win2, "\n");
    fprintf(win2, "Here is the cursor->");
    waitnext();
    printf("\n");
    printf("Now enter characters to each window, then end with return\n");
    waitnextprint();
    fclose(win2);
    putchar('\f');
    printf("Second window now closed\n");
    waitnext();
    pa_curvis(stdout, OFF);
    pa_auto(stdout, OFF);

    /* ********************* Resize buffer window character ******************** */

    ox = pa_maxx(stdout);
    oy = pa_maxy(stdout);
    pa_bcolor(stdout, pa_cyan);
    pa_sizbuf(stdout, 50, 50);
    putchar('\f');
    for (x = 1; x <= pa_maxx(stdout); x++) printf("*");
    pa_cursor(stdout, 1, pa_maxy(stdout));
    for (x = 1; x <= pa_maxx(stdout); x++) printf("*");
    for (y = 1; y <= pa_maxy(stdout); y++) { pa_cursor(stdout, 1, y); printf("*"); }
    for (y = 1; y <= pa_maxy(stdout); y++) { pa_cursor(stdout, pa_maxx(stdout), y); printf("*"); }
    pa_home(stdout);
    printf("Buffer should now be 50 by 50 characters, and\n");
    printf("painted blue\n");
    printf("maxx: %d maxy: %d\n", pa_maxx(stdout), pa_maxy(stdout));
    printf("Open up window to verify this\n");
    prtcen(pa_maxy(stdout), "Buffer resize character test\n");
    pa_bcolor(stdout, pa_white);
    waitnext();
    pa_sizbuf(stdout, ox, oy);

    /* *********************** Resize buffer window pixel ********************** */

    ox = pa_maxxg(stdout);
    oy = pa_maxyg(stdout);
    /* ratio by screen smallest x-y, then square it up */
    pa_scnsizg(stdout, &xs, &ys);
    if (xs > ys) { ys /= 4; xs = ys; } /* square */
    else { xs /= 4; ys = xs; }
    pa_bcolor(stdout, pa_cyan);
    pa_sizbufg(stdout, xs, ys);
    putchar('\f');
    pa_linewidth(stdout, 20);
    pa_line(stdout, 1, 1, pa_maxxg(stdout), 1);
    pa_line(stdout, 1, 1, 1, pa_maxyg(stdout));
    pa_line(stdout, 1, pa_maxyg(stdout), pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, pa_maxxg(stdout), 1, pa_maxxg(stdout), pa_maxyg(stdout));
    printf("Buffer should now be %d by %d pixels, and\n", xs, ys);
    printf("painted blue\n");
    printf("maxxg: %d maxyg: %d\n", pa_maxxg(stdout), pa_maxyg(stdout));
    printf("Open up window to verify this\n");
    prtcen(pa_maxy(stdout), "Buffer resize graphical test");
    pa_bcolor(stdout, pa_white);
    waitnext();
    pa_sizbufg(stdout, ox, oy);

    /* ****************** Resize screen with buffer on character *************** */

    ox = pa_maxxg(stdout);
    oy = pa_maxyg(stdout);
    for (x = 20; x <= 80; x++) {

        pa_setsiz(stdout, x, 25);
        pa_getsiz(stdout, &x2, &y2);
        if (x2 != x || y2 != 25) {

            pa_setsiz(stdout, 80, 25);
            putchar('\f');
            printf("*** Getsiz does not match setsiz, x: %d y: %d vs. x: %d y: %d\n",
                   x2, y2, x, 25);
            waitnext();
            longjmp(terminate_buf, 1);

        };
        putchar('\f');
        printf("Resize screen buffered character\n");
        printf("\n");
        printf("Moving in x\n");
        wait(1000);

    }
    printf("\n");
    printf("Complete");
    waitnext();
    for (y = 10; y <= 50; y++) {

        pa_setsiz(stdout, 80, y);
        pa_getsiz(stdout, &x2, &y2);
        if (x2 != 80 || y2 != y) {

            pa_setsiz(stdout, 80, 25);
            putchar('\f');
            printf("*** Getsiz does not match setsiz, x: %d y: %d vs. x: %d y: %d\n",
                   x2, y2, 80, y);
            printf("*** Getsiz does not match setsiz\n");
            waitnext();
            longjmp(terminate_buf, 1);

        }
        putchar('\f');
        printf("Resize screen buffered character\n");
        printf("\n");
        printf("Moving in y\n");
        wait(1000);

    }
    printf("\n");
    printf("Complete\n");
    waitnext();
    pa_winclientg(stdout, ox, oy, &ox, &oy, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    pa_setsizg(stdout, ox, oy);

    /* ******************** Resize screen with buffer on pixel ***************** */

    ox = pa_maxxg(stdout);
    oy = pa_maxyg(stdout);
    /* ratio by screen smallest x-y, then square it up */
    pa_scnsizg(stdout, &xs, &ys);
    if (xs > ys) { ys /= 8; xs = ys; } /* square */
    else { xs /= 8; ys = xs; }
    for (x = xs; x <= xs*4; x += xs/64) {

        pa_setsizg(stdout, x, ys);
        pa_getsizg(stdout, &x2, &y2);
        if (x2 != x || y2 != ys) {

            pa_setsiz(stdout, 80, 25);
            putchar('\f');
            printf("*** Getsiz does not match setsiz, x: %d y: %d vs. x: %d y: %d\n",
                   x2, y2, x, ys);
            printf("*** Getsiz does ! match setsiz\n");
            waitnext();
            longjmp(terminate_buf, 1);

        }
        putchar('\f');
        printf("Resize screen buffered graphical\n");
        printf("\n");
        printf("Moving in x\n");
        wait(100);

    }
    printf("\n");
    printf("Complete\n");
    waitnext();
    for (y = ys; y <= ys*4; y += ys/64) {

        pa_setsizg(stdout, xs, y);
        pa_getsizg(stdout, &x2, &y2);
        if (x2 != xs || y2 != y) {

            pa_setsiz(stdout, 80, 25);
            putchar('\f');
            printf("*** Getsiz does not match setsiz, x: %d y: %d vs. x: %d y: %d\n",
                   x2, y2, 300, y);
            printf("*** Getsiz does ! match setsiz\n");
            waitnext();
            longjmp(terminate_buf, 1);

        }
        putchar('\f');
        printf("Resize screen buffered graphical\n");
        printf("\n");
        printf("Moving in y\n");
        wait(100);

    }
    printf("\n");
    printf("Complete\n");
    waitnext();
    pa_winclientg(stdout, ox, oy, &ox, &oy, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    pa_setsizg(stdout, ox, oy);

    /* ********************************* Front/back test *********************** */

    /* ratio by screen smallest x-y, then square it up */
    pa_scnsizg(stdout, &xs, &ys);
    if (xs > ys) { ys /= 32; xs = ys; } /* square */
    else { xs /= 32; ys = xs; }
    cs = pa_chrsizy(stdout); /* save the character size */
    putchar('\f');
    pa_auto(stdout, OFF);
    printf("Position window for font/back test\n");
    printf("Then hit space to flip font/back status, or return to stop\n");
    fb = FALSE; /* clear front/back status */
    pa_font(stdout, PA_FONT_SIGN);
    pa_fontsiz(stdout, ys);
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etchar) if (er.echar == ' ') { /* flip front/back */

            fb = !fb;
            if (fb) {

                pa_front(stdout);
                pa_fcolor(stdout, pa_white);
                prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Back");
                pa_fcolor(stdout, pa_black);
                prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Front");

            } else {

                pa_back(stdout);
                pa_fcolor(stdout, pa_white);
                prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Front");
                pa_fcolor(stdout, pa_black);
                prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Back");

            }

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_home(stdout);
    pa_font(stdout, PA_FONT_TERM);
    pa_fontsiz(stdout, cs);
    pa_auto(stdout, ON);

    /* ************************* Frame controls test buffered ****************** */

    putchar('\f');
    pa_fcolor(stdout, pa_cyan);
    pa_rect(stdout, 1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, 1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, 1, pa_maxyg(stdout), pa_maxxg(stdout), 1);
    pa_fcolor(stdout, pa_black);
    pa_binvis(stdout);
    printf("Ready for frame controls buffered\n");
    waitnext();
    pa_frame(stdout, OFF);
    printf("Entire frame off\n");
    waitnext();
    pa_frame(stdout, ON);
    printf("Entire frame on\n");
    waitnext();
    pa_sysbar(stdout, OFF);
    printf("System bar off\n");
    waitnext();
    pa_sysbar(stdout, ON);
    printf("System bar on\n");
    waitnext();
    pa_sizable(stdout, OFF);
    printf("Size bars off\n");
    waitnext();
    pa_sizable(stdout, ON);
    printf("Size bars on\n");
    waitnext();
    pa_bover(stdout);

#endif
    /* ************************* Frame controls test unbuffered ****************** */

    pa_buffer(stdout, OFF);
    frametest("Ready for frame controls unbuffered - Resize me!");
    pa_frame(stdout, OFF);
    frametest("Entire frame off");
    pa_frame(stdout, ON);
    frametest("Entire frame on");
    pa_sysbar(stdout, OFF);
    frametest("System bar off");
    pa_sysbar(stdout, ON);
    frametest("System bar on");
    pa_sizable(stdout, OFF);
    frametest("Size bars off");
    pa_sizable(stdout, ON);
    frametest("Size bars on");
    pa_buffer(stdout, ON);

    /* ********************************* Menu test ***************************** */

    putchar('\f');
    pa_fcolor(stdout, pa_cyan);
    pa_rect(stdout, 1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, 1, 1, pa_maxxg(stdout), pa_maxyg(stdout));
    pa_line(stdout, 1, pa_maxyg(stdout), pa_maxxg(stdout), 1);
    pa_fcolor(stdout, pa_black);
    ml = NULL; /* clear menu list */
    newmenu(&mp, FALSE, FALSE, OFF, 1, "Say hello");
    appendmenu(&ml, mp);
    newmenu(&mp, TRUE, FALSE,  ON, 2, "Bark");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, 3, "Walk");
    appendmenu(&ml, mp);
    newmenu(&sm, FALSE, FALSE, OFF, 4, "Sublist");
    appendmenu(&ml, sm);
    /* these are one/of buttons */
    newmenu(&mp, FALSE, TRUE,  OFF, 5, "slow");
    appendmenu(&sm->branch, mp);
    newmenu(&mp, FALSE, TRUE,  OFF, 6, "medium");
    appendmenu(&sm->branch, mp);
    newmenu(&mp, FALSE, FALSE, ON, 7, "fast");
    appendmenu(&sm->branch, mp);
    /* these are on/off buttons */
    newmenu(&mp, TRUE, FALSE,  OFF, 8, "red");
    appendmenu(&sm->branch, mp);
    newmenu(&mp, TRUE, FALSE,  OFF, 9, "green");
    appendmenu(&sm->branch, mp);
    newmenu(&mp, TRUE, FALSE,  OFF, 10, "blue");
    appendmenu(&sm->branch, mp);
    pa_menu(stdout, ml);
    pa_menuena(stdout, 3, OFF); /* disable "Walk" */
    pa_menusel(stdout, 5, ON); /* turn on "slow" */
    pa_menusel(stdout, 8, ON); /* turn on "red" */

    pa_home(stdout);
    printf("Use sample menu above\n");
    printf("'Walk' is disabled\n");
    printf("'Sublist' is a dropdown\n");
    printf("'slow', 'medium' and 'fast' are a one/of list\n");
    printf("'red', 'green' and 'blue' are on/off\n");
    printf("There should be a bar between slow-medium-fast groups and\n");
    printf("red-green-blue groups.\n");
    sred = ON; /* set states */
    sgreen = OFF;
    sblue = OFF;
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);
        if (er.etype == pa_etmenus) {

            printf("Menu select: ");
            switch (er.menuid) {

                case 1:  printf("Say hello\n"); break;
                case 2:  printf("Bark\n"); break;
                case 3:  printf("Walk\n"); break;
                case 4:  printf("Sublist\n"); break;
                case 5:  printf("slow\n"); pa_menusel(stdout, 5, ON); break;
                case 6:  printf("medium\n"); pa_menusel(stdout, 6, ON); break;
                case 7:  printf("fast\n"); pa_menusel(stdout, 7, ON); break;
                case 8:  printf("red\n"); sred = !sred;
                         pa_menusel(stdout, 8, sred); break;
                case 9:  printf("green\n"); sgreen = !sgreen;
                         pa_menusel(stdout, 9, sgreen); break;
                case 10: printf("blue\n"); sblue = !sblue;
                         pa_menusel(stdout, 10, sblue); break;

            }

        }

    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    pa_menu(stdout, NULL);

    /* ****************************** Standard menu test ******************** */

    putchar('\f');
    pa_auto(stdout, ON);
    ml = NULL; /* clear menu list */
    newmenu(&mp, FALSE, FALSE, OFF, PA_SMMAX+1, "one");
    appendmenu(&ml, mp);
    newmenu(&mp, TRUE, FALSE,  ON, PA_SMMAX+2, "two");
    appendmenu(&ml, mp);
    newmenu(&mp, FALSE, FALSE, OFF, PA_SMMAX+3, "three");
    appendmenu(&ml, mp);
    pa_stdmenu(BIT(PA_SMNEW) | BIT(PA_SMOPEN) | BIT(PA_SMCLOSE) |
               BIT(PA_SMSAVE) | BIT(PA_SMSAVEAS) | BIT(PA_SMPAGESET) |
               BIT(PA_SMPRINT) | BIT(PA_SMEXIT) | BIT(PA_SMUNDO) |
               BIT(PA_SMCUT) | BIT(PA_SMPASTE) | BIT(PA_SMDELETE) |
               BIT(PA_SMFIND) | BIT(PA_SMFINDNEXT) | BIT(PA_SMREPLACE) |
               BIT(PA_SMGOTO) | BIT(PA_SMSELECTALL) | BIT(PA_SMNEWWINDOW) |
               BIT(PA_SMTILEHORIZ) | BIT(PA_SMTILEVERT) | BIT(PA_SMCASCADE) |
               BIT(PA_SMCLOSEALL) | BIT(PA_SMHELPTOPIC) | BIT(PA_SMABOUT),
               &mp, ml);
    pa_menu(stdout, mp);
    printf("Standard menu appears above\n");
    printf("Check our 'one', 'two', 'three' buttons are in the program\n");
    printf("defined position\n");
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);
        if (er.etype == pa_etmenus) {

            printf("Menu select: ");
            switch (er.menuid) {

                case PA_SMNEW:       printf("new\n"); break;
                case PA_SMOPEN:      printf("open\n"); break;
                case PA_SMCLOSE:     printf("close\n"); break;
                case PA_SMSAVE:      printf("save\n"); break;
                case PA_SMSAVEAS:    printf("saveas\n"); break;
                case PA_SMPAGESET:   printf("pageset\n"); break;
                case PA_SMPRINT:     printf("print\n"); break;
                case PA_SMEXIT:      printf("exit\n"); break;
                case PA_SMUNDO:      printf("undo\n"); break;
                case PA_SMCUT:       printf("cut\n"); break;
                case PA_SMPASTE:     printf("paste\n"); break;
                case PA_SMDELETE:    printf("delete\n"); break;
                case PA_SMFIND:      printf("find\n"); break;
                case PA_SMFINDNEXT:  printf("findnext\n"); break;
                case PA_SMREPLACE:   printf("replace\n"); break;
                case PA_SMGOTO:      printf("goto\n"); break;
                case PA_SMSELECTALL: printf("selectall\n"); break;
                case PA_SMNEWWINDOW: printf("newwindow\n"); break;
                case PA_SMTILEHORIZ: printf("tilehoriz\n"); break;
                case PA_SMTILEVERT:  printf("tilevert\n"); break;
                case PA_SMCASCADE:   printf("cascade\n"); break;
                case PA_SMCLOSEALL:  printf("closeall\n"); break;
                case PA_SMHELPTOPIC: printf("helptopic\n"); break;
                case PA_SMABOUT:     printf("about\n"); break;
                case PA_SMMAX+1:     printf("one\n"); break;
                case PA_SMMAX+2:     printf("two\n"); break;
                case PA_SMMAX+3:     printf("three\n"); break;

            }

        }

    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    pa_menu(stdout, NULL);

    /* ************************* Child windows test character ****************** */

    putchar('\f');
    chrgrid();
    prtcen(pa_maxy(stdout), "Child windows test character");
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_setpos(win2, 1, 10);
    pa_sizbuf(win2, 20, 10);
    pa_setsiz(win2, 20, 10);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_setpos(win3, 21, 10);
    pa_sizbuf(win3, 20, 10);
    pa_setsiz(win3, 20, 10);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_setpos(win4, 41, 10);
    pa_sizbuf(win4, 20, 10);
    pa_setsiz(win4, 20, 10);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    pa_home(stdout);
    printf("There should be 3 labeled child windows below, with frames   \n");
    waitnext();
    pa_frame(win2, OFF);
    pa_frame(win3, OFF);
    pa_frame(win4, OFF);
    pa_home(stdout);
    printf("There should be 3 labeled child windows below, without frames\n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_home(stdout);
    printf("Child windows should all be closed                           \n");
    waitnext();

    /* *************************** Child windows test pixel ******************** */

    putchar('\f');
    prtcen(pa_maxy(stdout), "Child windows test pixel");
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_setposg(win2, 1, 100);
    pa_sizbufg(win2, 200, 200);
    pa_setsizg(win2, 200, 200);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_setposg(win3, 201, 100);
    pa_sizbufg(win3, 200, 200);
    pa_setsizg(win3, 200, 200);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_setposg(win4, 401, 100);
    pa_sizbufg(win4, 200, 200);
    pa_setsizg(win4, 200, 200);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    pa_home(stdout);
    printf("There should be 3 labled child windows below, with frames   \n");
    waitnext();
    pa_frame(win2, OFF);
    pa_frame(win3, OFF);
    pa_frame(win4, OFF);
    pa_home(stdout);
    printf("There should be 3 labled child windows below, without frames\n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_home(stdout);
    printf("Child windows should all be closed                          \n");
    waitnext();

    /* ******************* Child windows stacking test pixel ******************* */

    putchar('\f');
    prtcen(pa_maxy(stdout), "Child windows stacking test pixel");
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_setposg(win2, 50, 50);
    pa_sizbufg(win2, 200, 200);
    pa_setsizg(win2, 200, 200);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_setposg(win3, 150, 100);
    pa_sizbufg(win3, 200, 200);
    pa_setsizg(win3, 200, 200);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_setposg(win4, 250, 150);
    pa_sizbufg(win4, 200, 200);
    pa_setsizg(win4, 200, 200);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    pa_home(stdout);
    printf("There should be 3 labled child windows below, overlapped,   \n");
    printf("with child 1 on the bottom, child 2 middle, and child 3 top.\n");
    waitnext();
    pa_back(win2);
    pa_back(win3);
    pa_back(win4);
    pa_home(stdout);
    printf("Now the windows are reordered, with child 1 on top, child 2 \n");
    printf("below that, and child 3 on the bottom.                      \n");
    waitnext();
    pa_front(win2);
    pa_front(win3);
    pa_front(win4);
    pa_home(stdout);
    printf("Now the windows are reordered, with child 3 on top, child 2 \n");
    printf("below that, and child 1 on the bottom.                      \n");
    waitnext();
    fclose(win2);
    fclose(win3);
    fclose(win4);
    putchar('\f');
    printf("Child windows should all be closed                          \n");
    waitnext();

    /* ************** Child windows stacking resize test pixel 1 *************** */

    pa_buffer(stdout, OFF);
    pa_auto(stdout, OFF);
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_setposg(win2, 50-25, 50-25);
    pa_sizbufg(win2, 200, 200);
    pa_setsizg(win2, pa_maxxg(stdout)-150, pa_maxyg(stdout)-150);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_setposg(win3, 100-25, 100-25);
    pa_sizbufg(win3, 200, 200);
    pa_setsizg(win3, pa_maxxg(stdout)-150, pa_maxyg(stdout)-150);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_setposg(win4, 150-25, 150-25);
    pa_sizbufg(win4, 200, 200);
    pa_setsizg(win4, pa_maxxg(stdout)-150, pa_maxyg(stdout)-150);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etredraw || er.etype == pa_etresize) {

            putchar('\f');
            prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout),
                    "Child windows stacking resize test pixel 1");
            prtceng(1, "move and resize");
            pa_setsizg(win3, pa_maxxg(stdout)-150, pa_maxyg(stdout)-150);
            pa_setsizg(win4, pa_maxxg(stdout)-150, pa_maxyg(stdout)-150);
            pa_setsizg(win2, pa_maxxg(stdout)-150, pa_maxyg(stdout)-150);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    fclose(win2);
    fclose(win3);
    fclose(win4);
/*  ???????? There is a hole in the buffer after this gets enabled */
    pa_buffer(stdout, ON);
    putchar('\f');
    printf("Child windows should all be closed                          \n");
    waitnext();

    /* ************** Child windows stacking resize test pixel 2 *************** */

    pa_buffer(stdout, OFF);
    pa_openwin(&stdin, &win2, stdout, 2);
    pa_setposg(win2, 50, 50);
    pa_sizbufg(win2, 200, 200);
    pa_setsizg(win2, pa_maxxg(stdout)-100, pa_maxyg(stdout)-100);
    pa_openwin(&stdin, &win3, stdout, 3);
    pa_setposg(win3, 100, 100);
    pa_sizbufg(win3, 200, 200);
    pa_setsizg(win3, pa_maxxg(stdout)-200, pa_maxyg(stdout)-200);
    pa_openwin(&stdin, &win4, stdout, 4);
    pa_setposg(win4, 150, 150);
    pa_sizbufg(win4, 200, 200);
    pa_setsizg(win4, pa_maxxg(stdout)-300, pa_maxyg(stdout)-300);
    pa_bcolor(win2, pa_cyan);
    putc('\f', win2);
    fprintf(win2, "I am child window 1\n");
    pa_bcolor(win3, pa_yellow);
    putc('\f', win3);
    fprintf(win3, "I am child window 2\n");
    pa_bcolor(win4, pa_magenta);
    putc('\f', win4);
    fprintf(win4, "I am child window 3\n");
    do {

        pa_event(stdin, &er);
        if (er.etype == pa_etredraw  || er.etype == pa_etresize) {

            putchar('\f');
            prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout),
                    "Child windows stacking resize test pixel 2");
            prtceng(1, "move and resize");
            pa_setsizg(win3, pa_maxxg(stdout)-200, pa_maxyg(stdout)-200);
            pa_setsizg(win4, pa_maxxg(stdout)-300, pa_maxyg(stdout)-300);
            pa_setsizg(win2, pa_maxxg(stdout)-100, pa_maxyg(stdout)-100);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    fclose(win2);
    fclose(win3);
    fclose(win4);
    pa_buffer(stdout, ON);
    putchar('\f');
    printf("Child windows should all be closed                          \n");
    waitnext();

    /* ******************************* Buffer off test *********************** */

    putchar('\f');
    pa_auto(stdout, OFF);
    pa_buffer(stdout, OFF);
    /* initialize prime size information */
    x = pa_maxxg(stdout);
    y = pa_maxyg(stdout);
    pa_linewidth(stdout, 5); /* set large lines */
    pa_font(stdout, PA_FONT_SIGN);
    pa_binvis(stdout);
    do {

        pa_event(stdin, &er); /* get next event */
        if (er.etype == pa_etredraw || er.etype == pa_etresize) {

            /* clear screen without overwriting frame */
            pa_fcolor(stdout, pa_white);
            pa_frect(stdout, 1+5, 1+5, x-5, y-5);
            pa_fcolor(stdout, pa_black);
            pa_fontsiz(stdout, y / 10);
            prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2,
                    "SIZE AND COVER ME !");
            pa_rect(stdout, 1+2, 1+2, x-2, y-2); /* frame the window */

        }
        if (er.etype == pa_etresize) {

            /* Save the new demensions, even if not required. This way we must
               get a resize notification for this test to work. */
            x = pa_maxxg(stdout);
            y = pa_maxyg(stdout);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_buffer(stdout, ON);

    /* ****************************** min/max/norm test ********************* */

    putchar('\f');
    pa_auto(stdout, OFF);
    pa_buffer(stdout, OFF);
    pa_font(stdout, PA_FONT_TERM);
    mincnt = 0; /* clear minimize counter */
    maxcnt = 0; /* clear maximize counter */
    nrmcnt = 0; /* clear normalize counter */
    do {

        pa_event(stdin, &er); /* get next event */
        /* count minimize, maximize, normalize */
        if (er.etype == pa_etmax) maxcnt = maxcnt+1;
        if (er.etype == pa_etmin) mincnt = mincnt+1;
        if (er.etype == pa_etnorm) nrmcnt = nrmcnt+1;
        if (er.etype == pa_etredraw || er.etype == pa_etmax ||
            er.etype == pa_etmin || er.etype == pa_etnorm) {

            putchar('\f');
            printf("Minimize, maximize and restore this window\n");
            printf("\n");
            printf("Minimize count:  %d\n", mincnt);
            printf("Maximize count:  %d\n", maxcnt);
            printf("Normalize count: %d\n", nrmcnt);

        }

        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_buffer(stdout, ON);

    /* ******************** Window size calculate character ***************** */

    putchar('\f');
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Window size calculate character");
    pa_home(stdout);
    pa_openwin(&stdin, &win2, NULL, 2);
    pa_linewidth(stdout, 1);

    pa_winclient(stdout, 20, 10, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (20, 10) client, full frame, window size is: %d,%d\n", x, y);
    pa_setsiz(win2, x, y);
    putc('\f', win2);
    pa_fcolor(win2, pa_black);
    fprintf(win2, "12345678901234567890\n");
    fprintf(win2, "2\n");
    fprintf(win2, "3\n");
    fprintf(win2, "4\n");
    fprintf(win2, "5\n");
    fprintf(win2, "6\n");
    fprintf(win2, "7\n");
    fprintf(win2, "8\n");
    fprintf(win2, "9\n");
    fprintf(win2, "0\n");
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    printf("System bar off\n");
    pa_sysbar(win2, OFF);
    pa_winclient(stdout, 20, 10, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize));
    printf("For (20, 10) client, no system bar, window size is: %d,%d\n", x, y);
    pa_setsiz(win2, x, y);
    putc('\f', win2);
    pa_fcolor(win2, pa_black);
    fprintf(win2, "12345678901234567890\n");
    fprintf(win2, "2\n");
    fprintf(win2, "3\n");
    fprintf(win2, "4\n");
    fprintf(win2, "5\n");
    fprintf(win2, "6\n");
    fprintf(win2, "7\n");
    fprintf(win2, "8\n");
    fprintf(win2, "9\n");
    fprintf(win2, "0\n");
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    printf("Sizing bars off\n");
    pa_sysbar(win2, ON);
    pa_sizable(win2, OFF);
    pa_winclient(stdout, 20, 10, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsysbar));
    printf("For (20, 10) client, no size bars, window size is: %d,%d\n", x, y);
    pa_setsiz(win2, x, y);
    putc('\f', win2);
    pa_fcolor(win2, pa_black);
    fprintf(win2, "12345678901234567890\n");
    fprintf(win2, "2\n");
    fprintf(win2, "3\n");
    fprintf(win2, "4\n");
    fprintf(win2, "5\n");
    fprintf(win2, "6\n");
    fprintf(win2, "7\n");
    fprintf(win2, "8\n");
    fprintf(win2, "9\n");
    fprintf(win2, "0\n");
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    printf("frame off\n");
    pa_sysbar(win2, ON);
    pa_sizable(win2, ON);
    pa_frame(win2, OFF);
    pa_winclient(stdout, 20, 10, &x, &y, BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (20, 10) client, no frame, window size is: %d,%d\n", x, y);
    pa_setsiz(win2, x, y);
    putc('\f', win2);
    pa_fcolor(win2, pa_black);
    fprintf(win2, "12345678901234567890\n");
    fprintf(win2, "2\n");
    fprintf(win2, "3\n");
    fprintf(win2, "4\n");
    fprintf(win2, "5\n");
    fprintf(win2, "6\n");
    fprintf(win2, "7\n");
    fprintf(win2, "8\n");
    fprintf(win2, "9\n");
    fprintf(win2, "0\n");
    pa_fcolor(win2, pa_cyan);
    pa_rect(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 1, 20*pa_chrsizx(win2), 10*pa_chrsizy(win2));
    pa_line(win2, 1, 10*pa_chrsizy(win2), 20*pa_chrsizx(win2), 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (20, 10) surface\n");
    waitnext();

    fclose(win2);

    /* ************************ Window size calculate pixel ******************** */

    putchar('\f');
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Window size calculate pixel");
    pa_home(stdout);
    pa_openwin(&stdin, &win2, NULL, 2);
    pa_linewidth(stdout, 1);
    pa_fcolor(win2, pa_cyan);
    pa_winclientg(stdout, 200, 200, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (200, 200) client, full frame, window size is: %d,%d\n", x, y);
    pa_setsizg(win2, x, y);
    pa_rect(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 200, 200, 1);
    pa_curvis(win2, OFF);
    printf("Check client window has (200, 200) surface\n");
    waitnext();

    printf("System bar off\n");
    pa_sysbar(win2, OFF);
    pa_winclientg(stdout, 200, 200, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize));
    printf("For (200, 200) client, no system bar, window size is: %d,%d\n", x, y);
    pa_setsizg(win2, x, y);
    putc('\f', win2);
    pa_rect(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 200, 200, 1);
    printf("Check client window has (200, 200) surface\n");
    waitnext();

    printf("Sizing bars off\n");
    pa_sysbar(win2, ON);
    pa_sizable(win2, OFF);
    pa_winclientg(stdout, 200, 200, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsysbar));
    printf("For (200, 200) client, no sizing, window size is: %d,%d\n", x, y);
    pa_setsizg(win2, x, y);
    putc('\f', win2);
    pa_rect(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 200, 200, 1);
    printf("Check client window has (200, 200) surface\n");
    waitnext();

    printf("frame off\n");
    pa_sysbar(win2, ON);
    pa_sizable(win2, ON);
    pa_frame(win2, OFF);
    pa_winclientg(stdout, 200, 200, &x, &y, BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (200, 200) client, no frame, window size is: %d,%d\n", x, y);
    pa_setsizg(win2, x, y);
    putc('\f', win2);
    pa_rect(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 1, 200, 200);
    pa_line(win2, 1, 200, 200, 1);
    printf("Check client window has (200, 200) surface\n");
    waitnext();

    fclose(win2);

    /* ******************* Window size calculate minimums pixel *************** */

    /* this test does not work, winclient needs to return the minimums */

#if 0
    putchar('\f');
    prtceng(pa_maxyg(stdout)-pa_chrsizy(stdout), "Window size calculate minimum pixel");
    pa_home(stdout);
    pa_openwin(&stdin, &win2, NULL, 2);
    pa_linewidth(stdout, 1);
    pa_fcolor(win2, pa_cyan);
    pa_winclientg(stdout, 1, 1, &x, &y, BIT(pa_wmframe) | BIT(pa_wmsize) | BIT(pa_wmsysbar));
    printf("For (200, 200) client, full frame, window size minimum is: %d,%d\n", x, y);
    pa_setsizg(win2, 1, 1);
    pa_getsizg(win2, &x2, &y2);
    waitnext();

    fclose(win2);
#endif

   /* ********************** Child windows torture test pixel ***************** */

    putchar('\f');
    printf("Child windows torture test pixel\n");
    for (i = 1; i <= 100; i++) {

        pa_openwin(&stdin, &win2, stdout, 2);
        pa_setposg(win2, 1, 100);
        pa_sizbufg(win2, 200, 200);
        pa_setsizg(win2, 200, 200);
        pa_openwin(&stdin, &win3, stdout, 3);
        pa_setposg(win3, 201, 100);
        pa_sizbufg(win3, 200, 200);
        pa_setsizg(win3, 200, 200);
        pa_openwin(&stdin, &win4, stdout, 4);
        pa_setposg(win4, 401, 100);
        pa_sizbufg(win4, 200, 200);
        pa_setsizg(win4, 200, 200);
        pa_bcolor(win2, pa_cyan);
        putc('\f', win2);
        fprintf(win2, "I am child window 1\n");
        pa_bcolor(win3, pa_yellow);
        putc('\f', win3);
        fprintf(win3, "I am child window 2\n");
        pa_bcolor(win4, pa_magenta);
        putc('\f', win4);
        fprintf(win4, "I am child window 3\n");
        fclose(win2);
        fclose(win3);
        fclose(win4);

    }
    pa_home(stdout);
    pa_bover(stdout);
    printf("Child windows should all be closed\n");
    waitnext();

    terminate: /* terminate */

    putchar('\f');
    pa_auto(stdout, OFF);
    pa_font(stdout, PA_FONT_SIGN);
    pa_fontsiz(stdout, 50);
    prtceng(pa_maxyg(stdout)/2-pa_chrsizy(stdout)/2, "Test complete");

}
