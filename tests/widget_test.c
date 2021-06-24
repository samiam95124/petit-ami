/*******************************************************************************
*                                                                              *
*                          WIDGET TEST PROGRAM                                 *
*                                                                              *
*                    Copyright (C) 2005 Scott A. Franco                        *
*                                                                              *
* Tests the widgets and dialogs available.                                     *
*                                                                              *
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

#define SECOND 10000 /* one second timer */

static jmp_buf       terminate_buf;
static pa_evtrec     er;
static int           chk, chk2, chk3;
static string        s;
static string        ss, rs;
static int           prog;
static pa_strptr     sp, lp;
static int           x, y;
static int           r, g, b;
static pa_qfnopts    optf;
static pa_qfropts    optfr;
static int           fc;
static int           fs;
static int           fr, fg, fb;
static int           br, bg, bb;
static pa_qfteffects fe
static int           cx, cy;
static int           ox, oy;

static int           i, cnt;
static char          fns[100];

/* allocate memory with error checking */
static void *imalloc(size_t size)

{

    void* p;

    p = malloc(size);
    if (!p) {

        fprintf(stderr, "*** Out of memory ***\n");
        exit(1);

    }

    return (p);

}

/* get string in dynamic storage */
static char* str(char* s)

{

    char* p;

    p = malloc(strlen(s)+1);
    if (!p) error(enomem);
    strcpy(p, s);

    return (p);

}

/* wait return to be pressed, or handle terminate */
static void waitnext(void)

{

    pa_evtrec er; /* event record */

    do { pa_event(stdin, stdin, &er); }
    while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

}

/* draw a character grid */
static void chrgrid(void)

{

    int x, y;

    pa_fpa_color(stdout, pa_yellow);
    y = 1;
    while (y < pa_maxyg(stdout)) {

        pa_line(stdout, stdout, 1, y, pa_maxxg(stdout), y);
        y = y+pa_chrsizy(stdout);

    }
    x = 1;
    while (x < pa_maxxg(stdout)) {

        pa_line(stdout, x, 1, x, pa_maxyg(stdout));
        x = x+pa_chrsizx(stdout);

    }
    pa_fcolor(stdout, pa_black);

}

int main(void)

{

    if (setjmp(terminate_buf)) goto terminate;

    printf("Widget test vs. 0.1\n");
    printf("\n");
    printf("Hit return in any window to continue for each test\n");
    waitnext();

    /* ************************** Terminal Button test ************************* */

    pa_bcolor(stdout, pa_backcolor);
    printf("\f");
    printf("Background pa_color test\n");
    printf("\n");
    printf("The background color should match widgets now.\n");
    waitnext();
    pa_bcolor(stdout, pa_white);

    /* ************************** Terminal Button test ************************* */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal buttons test\n");
    printf("\n");
    pa_buttonsiz(stdout, "Hello, there", x, y);
    pa_button(stdout, 10, 7, 10+x-1, 7+y-1, "Hello, there", 1);
    pa_buttonsiz(stdout, "Bark!", x, y);
    pa_button(stdout, 10, 10, 10+x-1, 10+y-1, "Bark!", 2);
    pa_buttonsiz(stdout, "Sniff", x, y);
    pa_button(stdout, 10, 13, 10+x-1, 13+y-1, "Sniff", 3);
    printf("Hit the buttons, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etbutton) {

            if (er.butid == 1) printf("Hello to you, too");
            else if (er.butid == 2) printf("Bark bark");
            else if er.butid == 3 then printf("Sniff sniff");
            else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1)

    } while (er.etype != pa_etenter);
    pa_enablewidget(stdout, 2, false);
    printf("Now the middle button is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the buttons, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etbutton) {

            if (er.butid == 1) printf("Hello to you, too");
            else if (er.butid == 2) printf("Bark bark");
            else if (er.butid == 3) printf("Sniff sniff");
            else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);

    /* ************************* Graphical Button test ************************* */

    printf("\f");
    printf("Graphical buttons test\n");
    printf("\n");
    pa_buttonsizg(stdout, "Hello, there", x, y);
    pa_buttong(stdout, 100, 100, 100+x, 100+y, "Hello, there", 1);
    pa_buttonsizg(stdout, "Bark!", x, y);
    pa_buttong(stdout, 100, 150, 100+x, 150+y, "Bark!", 2);
    pa_buttonsizg(stdout, "Sniff", x, y);
    pa_buttong(stdout, 100, 200, 100+x, 200+y, "Sniff", 3);
    printf("Hit the buttons, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etbutton) {

            if (er.butid == 1) printf("Hello to you, too\n");
            else if (er.butid == 2) printf("Bark bark\n");
            else if (er.butid == 3) printf("Sniff sniff\n");
            else printf("!!! No button with id: %d\n !!!", er.butid);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_enablewidget(stdout, 2, false);
    printf("Now the middle button is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the buttons, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etbutton) {

            if (er.butid == 1) printf("Hello to you, too\n");
            else if (er.butid == 2) printf("Bark bark\n");
            else if (er.butid == 3) printf("Sniff sniff\n");
            else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);

    /* ************************** Terminal Checkbox test ************************** */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal checkbox test\n");
    printf("\n");
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    pa_checkboxsiz(stdout, "Pick me", x, y);
    pa_checkbox(stdout, 10, 7, 10+x-1, 7+y-1, "Pick me", 1);
    pa_checkboxsiz(stdout, "Or me", x, y);
    pa_checkbox(stdout, 10, 10, 10+x-1, 10+y-1, "Or me", 2);
    pa_checkboxsiz(stdout, "No, me", x, y);
    pa_checkbox(stdout, 10, 13, 10+x-1, 13+y-1, "No, me", 3);
    printf("Hit the checkbox, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etchkbox) {

            if (er.ckbxid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                pa_selectwidget(stdout, 1, chk);

            } else if (er.ckbxid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                pa_selectwidget(stdout, 2, chk2);

            } else if (er.ckbxid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                pa_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_enablewidget(stdout, 2, FALSE);
    printf("Now the middle checkbox is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the checkbox, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etchkbox) {

            if (er.ckbxid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                pa_selectwidget(stdout, 1, chk);

            } else if (er.ckbxid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                pa_selectwidget(stdout, 2, chk2);

            } else if (er.ckbxid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                pa_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);

    /* ************************** Graphical Checkbox test ************************** */

    printf("\f");
    printf("Graphical checkbox test\n");
    printf("\n");
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    pa_checkboxsizg(stdout, "Pick me", x, y);
    pa_checkboxg(stdout, 100, 100, 100+x, 100+y, "Pick me", 1);
    pa_checkboxsizg(stdout, "Or me", x, y);
    pa_checkboxg(stdout, 100, 150, 100+x, 150+y, "Or me", 2);
    pa_checkboxsizg(stdout, "No, me", x, y);
    pa_checkboxg(stdout, 100, 200, 100+x, 200+y, "No, me", 3);
    printf("Hit the checkbox, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etchkbox) {

            if (er.ckbxid == 1) {

                printf("You selected the top checkbox\n");
                chk = ! chk;
                pa_selectwidget(stdout, 1, chk);

            } else if (er.ckbxid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = ! chk2;
                pa_selectwidget(stdout, 2, chk2);

            } else if (er.ckbxid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = ! chk3;
                pa_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_enablewidget(stdout, 2, FALSE);
    printf("Now the middle checkbox is disabled, and should not be able to\n");
    printf("be pressed.\n");
    printf("Hit the checkbox, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etchkbox) {

            if (er.ckbxid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                pa_selectwidget(stdout, 1, chk);

            } else if (er.ckbxid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                pa_selectwidget(stdout, 2, chk2);

            } else if (er.ckbxid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                pa_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1)

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);

    /* *********************** Terminal radio button test ********************* */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal radio button test\n");
    printf("\n");
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    radiopa_buttonsiz(stdout, "Station 1", x, y);
    radiopa_button(stdout, 10, 7, 10+x-1, 7+y-1, "Station 1", 1);
    radiopa_buttonsiz(stdout, "Station 2", x, y);
    radiopa_button(stdout, 10, 10, 10+x-1, 10+y-1, "Station 2", 2);
    radiopa_buttonsiz(stdout, "Station 3", x, y);
    radiopa_button(stdout, 10, 13, 10+x-1, 13+y-1, "Station 3", 3);
    printf("Hit the radio button, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etradbut) {

            if (er.radbid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                pa_selectwidget(stdout, 1, chk);

            } else if (er.radbid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                pa_selectwidget(stdout, 2, chk2);

            } else if (er.radbid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                pa_selectwidget(stdout, 3, chk3)

            } else printf("!!! No button with id: %d !!!", er.butid);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1)

    } while (er.etype != pa_etenter);
    pa_enablewidget(stdout, 2, false);
    printf("Now the middle radio button is disabled, and should not be able\n");
    printf("to be pressed.\n");
    printf("Hit the radio button, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if er.etype == pa_etradbut then {

            if (er.radbid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                pa_selectwidget(stdout, 1, chk);

            } else if (er.radbid == 2) then {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                pa_selectwidget(stdout, 2, chk2);

            } else if (er.radbid == 3) then {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                pa_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: ", er.butid:1, " !!!\n")

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);

    /* *********************** Graphical radio button test ********************* */

    printf("\f");
    printf("Graphical radio button test\n");
    printf("\n");
    chk = FALSE;
    chk2 = FALSE;
    chk3 = FALSE;
    radiopa_buttonsizg(stdout, "Station 1", x, y);
    radiopa_buttong(stdout, 100, 100, 100+x, 100+y, "Station 1", 1);
    radiopa_buttonsizg(stdout, "Station 2", x, y);
    radiopa_buttong(stdout, 100, 150, 100+x, 150+y, "Station 2", 2);
    radiopa_buttonsizg(stdout, "Station 3", x, y);
    radiopa_buttong(stdout, 100, 200, 100+x, 200+y, "Station 3", 3);
    printf("Hit the radio button, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etradbut) {

            if (er.radbid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                pa_selectwidget(stdout, 1, chk);

            } else if (er.radbid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                pa_selectwidget(stdout, 2, chk2);

            } else if (er.radbid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                pa_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_enablewidget(stdout, 2, FALSE);
    printf("Now the middle radio button is disabled, and should not be able\n");
    printf("to be pressed.\n");
    printf("Hit the radio button, or return to continue\n");
    printf("\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etradbut) {

            if (er.radbid == 1) {

                printf("You selected the top checkbox\n");
                chk = !chk;
                pa_selectwidget(stdout, 1, chk);

            } else (if er.radbid == 2) {

                printf("You selected the middle checkbox\n");
                chk2 = !chk2;
                pa_selectwidget(stdout, 2, chk2);

            } else if (er.radbid == 3) {

                printf("You selected the bottom checkbox\n");
                chk3 = !chk3;
                pa_selectwidget(stdout, 3, chk3);

            } else printf("!!! No button with id: %d !!!\n", er.butid);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);


    /* *********************** Terminal Group box test ************************ */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal group box test\n");
    printf("\n");
    gropa_up(stdout)siz("Hello there", 0, 0, x, y, ox, oy);
    gropa_up(stdout)(10, 10, 10+x, 10+y, "Hello there", 1);
    printf("This is a group box with a null client area\n");
    printf("Hit return to continue\n");
    waitnext();
    pa_killwidget(stdout, 1);
    groupsiz("Hello there", 20, 10, x, y, ox, oy);
    group(10, 10, 10+x, 10+y, "Hello there", 1);
    printf("This is a group box with a 20,10 client area\n");
    printf("Hit return to continue\n");
    waitnext();
    pa_killwidget(stdout, 1);
    gropa_up(stdout)siz("Hello there", 20, 10, x, y, ox, oy);
    gropa_up(stdout)(10, 10, 10+x, 10+y, "Hello there", 1);
    pa_button(stdout, 10+ox, 10+oy, 10+ox+20-1, 10+oy+10-1, "Bark, bark!", 2);
    printf("This is a group box with a 20,10 layered button\n");
    printf("Hit return to continue\n");
    waitnext();
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);

    /* *********************** Graphical Group box test ************************ */

    printf("\f");
    printf("Graphical group box test\n");
    printf("\n");
    groupsizg("Hello there", 0, 0, x, y, ox, oy);
    groupg(100, 100, 100+x, 100+y, "Hello there", 1);
    printf("This is a group box with a null client area\n");
    printf("Hit return to continue\n");
    waitnext();
    pa_killwidget(stdout, 1);
    groupsizg("Hello there", 200, 200, x, y, ox, oy);
    groupg(100, 100, 100+x, 100+y, "Hello there", 1);
    printf("This is a group box with a 200,200 client area\n");
    printf("Hit return to continue\n");
    waitnext();
    pa_killwidget(stdout, 1);
    groupsizg("Hello there", 200, 200, x, y, ox, oy);
    groupg(100, 100, 100+x, 100+y, "Hello there", 1);
    pa_buttong(stdout, 100+ox, 100+oy, 100+ox+200, 100+oy+200, "Bark, bark!", 2);
    printf("This is a group box with a 200,200 layered button\n");
    printf("Hit return to continue");
    waitnext();
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);

    /* *********************** Terminal background test ************************ */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal background test\n");
    printf("\n");
    pa_background(10, 10, 40, 20, 1);
    printf("Hit return to continue\n");
    waitnext();
    pa_button(stdout, 11, 11, 39, 19, "Bark, bark!", 2);
    printf("This is a background with a layered button\n");
    printf("Hit return to continue\n");
    waitnext();
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);

    /* *********************** Graphical background test ************************ */

    printf("\f");
    printf("Graphical background test\n");
    printf("\n");
    pa_backgroundg(100, 100, 400, 200, 1);
    printf("Hit return to continue\n");
    waitnext();
    pa_buttong(stdout, 110, 110, 390, 190, "Bark, bark!", 2);
    printf("This is a background with a layered button\n");
    printf("Hit return to continue\n");
    waitnext();
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);

    /* *********************** Terminal scroll bar test *********************** */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal scroll bar test\n");
    printf("\n");
    pa_scrollvertsiz(stdout, x, y);
    pa_scrollvert(stdout, 10, 10, 10+x-1, 20, 1);
    pa_scrollhorizsiz(stdout, x, y);
    pa_scrollhoriz(stdout, 15, 10, 35, 10+y-1, 2);
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == pa_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldlid);
        if (er.etype == pa_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == pa_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid:1);
        if (er.etype == pa_etsclpos) {

            pa_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);

    /* ******************* Terminal scroll bar sizing test ******************** */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal scroll bar sizing test\n");
    printf("\n");
    pa_scrollvert(stdout, 10, 10, 12, 20, 1);
    pa_scrollsiz(stdout, 1, (INT_MAX / 4)*3);
    pa_scrollvert(stdout, 10+5, 10, 12+5, 20, 2);
    pa_scrollsiz(stdout, 2, INT_MAX / 2);
    pa_scrollvert(stdout, 10+10, 10, 12+10, 20, 3);
    pa_scrollsiz(stdout, 3, INT_MAX / 4);
    pa_scrollvert(stdout, 10+15, 10, 12+15, 20, 4);
    pa_scrollsiz(stdout, 4, INT_MAX / 8);
    printf("Now should be four scrollbars, decending in size to the right.\n");
    printf("All of the scrollbars can be manipulated.\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == pa_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldlid);
        if (er.etype == pa_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == pa_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == pa_etsclpos) {

            pa_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        };
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1)

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

    /* ****************** Terminal scroll bar minimums test ******************* */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal scroll bar minimums test\n");
    printf("\n");
    pa_scrollvertsiz(stdout, x, y);
    pa_scrollvert(stdout, 10, 10, 10+x-1, 10+y-1, 1);
    pa_scrollhorizsiz(stdout, x, y);
    pa_scrollhoriz(stdout, 15, 10, 15+x-1, 10+y-1, 2);
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etsclull)
            printf("Scrollbar: ", er.sclulid:1, " up/left line\n");
        if (er.etype == pa_etscldrl)
            printf("Scrollbar: ", er.scldlid:1, " down/right line\n");
        if (er.etype == pa_etsclulp)
            printf("Scrollbar: ", er.sclpa_up(stdout)id:1, " up/left page\n");
        if (er.etype == pa_etscldrp)
            printf("Scrollbar: ", er.scldpid:1, " down/right page\n");
        if (er.etype == pa_etsclpos) {

            pa_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);

    /* ************ Terminal scroll bar fat and skinny bars test ************** */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal scroll bar fat and skinny bars test\n");
    printf("\n");
    pa_scrollvertsiz(stdout, x, y);
    pa_scrollvert(stdout, 10, 10, 10, 10+10, 1);
    pa_scrollvert(stdout, 12, 10, 20, 10+10, 3);
    pa_scrollhorizsiz(stdout, x, y);
    pa_scrollhoriz(stdout, 30, 10, 30+20, 10, 2);
    pa_scrollhoriz(stdout, 30, 12, 30+20, 20, 4);
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid");
        if (er.etype == pa_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldlid);
        if (er.etype == pa_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == pa_etscldrp) then
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == pa_etsclpos) then {

            pa_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype == pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

    /* *********************** Graphical scroll bar test *********************** */

    printf("\f");
    printf("Graphical scroll bar test\n");
    printf("\n");
    pa_scrollvertsizg(stdout, x, y);
    pa_scrollvertg(stdout, 100, 100, 100+x, 300, 1);
    pa_scrollhorizsizg(stdout, x, y);
    pa_scrollhorizg(stdout, 150, 100, 350, 100+y, 2);
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == pa_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldlid);
        if (er.etype == pa_etsclulp)
            printf("Scrollbar: %d up/pa_left page\n", er.sclupid);
        if (er.etype == pa_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == pa_etsclpos) {

            pa_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);

    /* ******************* Graphical scroll bar sizing test ******************** */

    printf("\f");
    printf("Graphical scroll bar sizing test\n");
    printf("\n");
    pa_scrollvertg(stdout, 100, 100, 120, 300, 1);
    pa_scrollsiz(stdout, 1, (INT_MAX / 4)*3);
    pa_scrollvertg(stdout, 100+50, 100, 120+50, 300, 2);
    pa_scrollsiz(stdout, 2, INT_MAX / 2);
    pa_scrollvertg(stdout, 100+100, 100, 120+100, 300, 3);
    pa_scrollsiz(stdout, 3, INT_MAX / 4);
    pa_scrollvertg(stdout, 100+150, 100, 120+150, 300, 4);
    pa_scrollsiz(stdout, 4, INT_MAX / 8);
    printf("Now should be four scrollbars, dec}ing in size to the pa_right(stdout).\n");
    printf("All of the scrollbars can be manipulated.\n");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etsclull) then
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == pa_etscldrl) then
            printf("Scrollbar: %d down/right line\n", er.scldlid);
        if (er.etype == pa_etsclulp) then
            printf("Scrollbar: %d up/pa_left page\n", er.sclupid);
        if (er.etype == pa_etscldrp) then
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == pa_etsclpos) then {

            pa_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d", er.sclpid, er.sclpos);

        }
        if er.etype == pa_etterm longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

    /* ****************** Graphical scroll bar minimums test ******************* */

    printf("\f");
    printf("Graphical scroll bar minimums test\n");
    printf("\n");
    pa_scrollvertsizg(stdout, x, y);
    pa_scrollvertg(stdout, 100, 100, 100+x, 100+y, 1);
    pa_scrollhorizsizg(stdout, x, y);
    pa_scrollhorizg(stdout, 150, 100, 150+x, 100+y, 2);
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == pa_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldlid);
        if (er.etype == pa_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == pa_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == pa_etsclpos) {

            pa_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);

    /* ************ Graphical scroll bar fat and skinny bars test ************** */

    printf("\f");
    printf("Graphical scroll bar fat && skinny bars test\n");
    printf("\n");
    pa_scrollvertsizg(stdout, x, y);
    pa_scrollvertg(stdout, 100, 100, 100+x / 2, 100+200, 1);
    pa_scrollvertg(stdout, 120, 100, 200, 100+200, 3);
    pa_scrollhorizsizg(stdout, x, y);
    pa_scrollhorizg(stdout, 250, 100, 250+200, 100+y / 2, 2);
    pa_scrollhorizg(stdout, 250, 120, 250+200, 200, 4);
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etsclull)
            printf("Scrollbar: %d up/left line\n", er.sclulid);
        if (er.etype == pa_etscldrl)
            printf("Scrollbar: %d down/right line\n", er.scldlid);
        if (er.etype == pa_etsclulp)
            printf("Scrollbar: %d up/left page\n", er.sclupid);
        if (er.etype == pa_etscldrp)
            printf("Scrollbar: %d down/right page\n", er.scldpid);
        if (er.etype == pa_etsclpos) {

            pa_scrollpos(stdout, er.sclpid, er.sclpos); /* set new position for scrollbar */
            printf("Scrollbar: %d position set: %d\n", er.sclpid, er.sclpos);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

    /* ******************** Terminal number select box test ******************* */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal number select box test\n");
    printf("\n");
    pa_numselboxsiz(stdout, 1, 10, x, y);
    pa_numselbox(stdout, 10, 10, 10+x-1, 10+y-1, 1, 10, 1);
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etnumbox) printf("You selected: %d\n", er.numbsl);
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1)

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);

    /* ******************** Graphical number select box test ******************* */

    printf("\f");
    printf("Graphical number select box test\n");
    printf("\n");
    pa_numselboxsizg(stdout, 1, 10, x, y);
    pa_numselboxg(stdout, 100, 100, 100+x, 100+y, 1, 10, 1);
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etnumbox) printf("You selected: %d\n", er.numbsl);
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype == pa_etenter);
    pa_killwidget(stdout, 1);

    /* ************************* Terminal edit box test ************************ */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal edit box test\n");
    printf("\n");
    pa_editboxsiz(stdout, "Hi there, george", x, y);
    pa_editbox(stdout, 10, 10, 10+x-1, 10+y-1, 1);
    pa_putwidgettext(stdout, 1, "Hi there, george");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etedtbox) {

            pa_getwidgettext(stdout, 1, s);
            printf("You entered: %s\n", s);

        }
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);

    /* ************************* Graphical edit box test ************************ */

    printf("\f");
    printf("Graphical edit box test\n");
    printf("\n");
    pa_editboxsizg(stdout, "Hi there, george", x, y);
    pa_editboxg(stdout, 100, 100, 100+x-1, 100+y-1, 1);
    pa_putwidgettext(stdout, 1, "Hi there, george");
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etedtbox) {

            pa_getwidgettext(stdout, 1, s);
            printf("You entered: %s\n", s);

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);

    /* *********************** Terminal progress bar test ********************* */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal progress bar test\n");
    printf("\n");
    pa_progbarsiz(stdout, x, y);
    pa_progbar(stdout, 10, 10, 10+x-1, 10+y-1, 1);
    pa_timer(stdout, 1, SECOND, TRUE);
    prog = 1;
    do {

        pa_event(stdin, er);
        if (er.etype == pa_ettim) {

            if (prog < 20) {

                pa_progbarpos(stdout, 1, INT_MAX-((20-prog)*(INT_MAX / 20)));
                prog = prog+1; /* next progress value */

            } else if (prog == 20) {

                pa_progbarpos(stdout, 1, INT_MAX);
                printf("Done !\n");
                prog = 11;
                killpa_timer(stdout, 1);

            }

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);

    /* *********************** Graphical progress bar test ********************* */

    printf("\f");
    printf("Graphical progress bar test\n");
    printf("\n");
    pa_progbarsizg(stdout, x, y);
    pa_progbarg(stdout, 100, 100, 100+x-1, 100+y-1, 1);
    pa_timer(stdout, 1, SECOND, TRUE);
    prog = 1;
    do {

        pa_event(stdin, er);
        if (er.etype == pa_ettim) {

            if (prog < 20) {

                pa_progbarpos(stdout, 1, INT_MAX-((20-prog)*(INT_MAX / 20)));
                prog = prog+1; /* next progress value */

            } else if (prog == 20) {

                pa_progbarpos(stdout, 1, INT_MAX);
                printf("Done !\n");
                prog = 11;
                pa_killtimer(stdout, 1);

            }

        }
        if (er.etype == pa_etterm) longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);

    /* ************************* Terminal list box test ************************ */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal list box test\n");
    printf("\n");
    printf("Note that it is normal for this box to ! fill to exact\n");
    printf("character cells.\n");
    printf("\n");
    lp = (pa_strptr)imalloc(sizeof(pa_strrec));
    lp->str = str("Blue");
    lp->next = NULL;
    sp = (pa_strptr)imalloc(sizeof(pa_strrec));
    sp->str - str("Red");
    sp->next = lp;
    lp = sp;
    sp = (pa_strptr)imalloc(sizeof(pa_strrec));
    sp->str = str("Green");
    sp->next = lp;
    lp = sp;
    pa_listboxsiz(stdout, lp, x, y);
    pa_listbox(stdout, 10, 10, 10+x-1, 10+y-1, lp, 1);
    do {

        pa_event(stdin, er);
        if (er.etype == pa_etlstbox) {

            switch (er.lstbsl) {

                case 1: printf("You selected pa_green\n");
                case 2: printf("You selected pa_red\n");
                case 3: printf("You selected pa_blue\n");
                default: printf("!!! Bad select number !!!\n");

            }

        }
        if er.etype == pa_etterm longjmp(terminate_buf, 1);

    } while (er.etype != pa_etenter);
    pa_killwidget(stdout, 1);

    /* ************************* Graphical list box test ************************ */

    printf("\f");
    printf("Graphical list box test\n");
    printf("\n");
    new(lp);
    copy(lp->str, "Blue");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Red");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Green");
    sp->next = lp;
    lp = sp;
    pa_listboxsizg(stdout, lp, x, y);
    pa_listboxg(stdout, 100, 100, 100+x-1, 100+y-1, lp, 1);
    repeat

        pa_event(stdin, er);
        if er.etype == pa_etlstbox then {

            case er.lstbsl of

                1: printf("You selected pa_green\n");
                2: printf("You selected pa_red\n");
                3: printf("You selected pa_blue\n");
                else printf("!!! Bad select number !!!\n")

            }

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);

    /* ************************* Terminal dropdown box test ************************ */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal dropdown box test\n");
    printf("\n");
    printf("Note that it is normal for this box to not fill to exact\n");
    printf("character cells.\n");
    printf("\n");
    new(lp);
    copy(lp->str, "Dog");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Cat");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Bird");
    sp->next = lp;
    lp = sp;
    pa_dropboxsiz(stdout, lp, cx, cy, ox, oy);
    pa_dropbox(stdout, 10, 10, 10+ox-1, 10+oy-1, lp, 1);
    repeat

        pa_event(stdin, er);
        if er.etype == pa_etdrpbox then {

            case er.drpbsl of

                1: printf("You selected Bird\n");
                2: printf("You selected Cat\n");
                3: printf("You selected Dog\n");
                else printf("!!! Bad select number !!!\n")

            }

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);

    /* ************************* Graphical dropdown box test ************************ */

    printf("\f");
    printf("Graphical droppa_down(stdout) box test\n");
    printf("\n");
    new(lp);
    copy(lp->str, "Dog");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Cat");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Bird");
    sp->next = lp;
    lp = sp;
    pa_dropboxsizg(stdout, lp, cx, cy, ox, oy);
    pa_dropboxg(stdout, 100, 100, 100+ox-1, 100+oy-1, lp, 1);
    repeat

        pa_event(stdin, er);
        if er.etype == pa_etdrpbox then {

            case er.drpbsl of

                1: printf("You selected Bird\n");
                2: printf("You selected Cat\n");
                3: printf("You selected Dog\n");
                else printf("!!! Bad select number !!!\n")

            }

        };
       if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);

    /* ******************* Terminal dropdown edit box test ******************** */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal droppa_down(stdout) edit box test\n");
    printf("\n");
    printf("Note that it is normal for this box to not fill to exact\n");
    printf("character cells.\n");
    printf("\n");
    new(lp);
    copy(lp->str, "Corn");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Flower");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Tortillas");
    sp->next = lp;
    lp = sp;
    droppa_editboxsiz(stdout, lp, cx, cy, ox, oy);
    droppa_editbox(stdout, 10, 10, 10+ox-1, 10+oy-1, lp, 1);
    repeat

        pa_event(stdin, er);
        if er.etype == pa_etdrebox then {

            pa_getwidgettext(stdout, 1, s);
            printf("You selected: ", s^);

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);

    /* ******************* Graphical dropdown edit box test ******************** */

    printf("\f");
    printf("Graphical dropdown edit box test\n");
    printf("\n");
    new(lp);
    copy(lp->str, "Corn");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Flower");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Tortillas");
    sp->next = lp;
    lp = sp;
    droppa_editboxsizg(stdout, lp, cx, cy, ox, oy);
    droppa_editboxg(stdout, 100, 100, 100+ox-1, 100+oy-1, lp, 1);
    repeat

        pa_event(stdin, er);
        if er.etype == pa_etdrebox then {

            pa_getwidgettext(stdout, 1, s);
            printf("You selected: ", s^, "\n");

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);

    /* ************************* Terminal slider test ************************ */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal slider test\n");
    pa_slidehorizsiz(stdout, x, y);
    pa_slidehoriz(stdout, 10, 10, 10+x-1, 10+y-1, 10, 1);
    pa_slidehoriz(stdout, 10, 20, 10+x-1, 20+y-1, 0, 2);
    pa_slidevertsiz(stdout, x, y);
    pa_slidevert(stdout, 40, 10, 40+x-1, 10+y-1, 10, 3);
    pa_slidevert(stdout, 50, 10, 50+x-1, 10+y-1, 0, 4);
    printf("Bottom and right sliders should not have tick marks\n");
    repeat

        pa_event(stdin, er);
        if er.etype == pa_etsldpos then
            printf("Slider id: ", er.sldpid:1, " position: ", er.sldpos:1, "\n");
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

    /* ************************* Graphical slider test ************************ */

    printf("\f");
    printf("Graphical slider test\n");
    pa_slidehorizsizg(stdout, x, y);
    pa_slidehorizg(stdout, 100, 100, 100+x-1, 100+y-1, 10, 1);
    pa_slidehorizg(stdout, 100, 200, 100+x-1, 200+y-1, 0, 2);
    pa_slidevertsizg(stdout, x, y);
    pa_slidevertg(stdout, 400, 100, 400+x-1, 100+y-1, 10, 3);
    pa_slidevertg(stdout, 500, 100, 500+x-1, 100+y-1, 0, 4);
    printf("Bottom and right sliders should not have tick marks\n");
    repeat

        pa_event(stdin, er);
        if er.etype == pa_etsldpos then
            printf("Slider id: ", er.sldpid:1, " position: ", er.sldpos:1, "\n");
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

    /* ************************* Terminal tab bar test ************************ */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal tab bar test\n");
    printf("\n");
    new(lp);
    copy(lp->str, "Right");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Left");
    sp->next = lp;
    lp = sp;
    pa_tabbarsiz(stdout, pa_totop, 20, 2, x, y, ox, oy);
    pa_tabbar(stdout, 15, 3, 15+x-1, 3+y-1, lp, pa_totop, 1);

    new(lp);
    copy(lp->str, "Bottom");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Top");
    sp->next = lp;
    lp = sp;
    pa_tabbarsiz(stdout, pa_topa_right(stdout), 2, 12, x, y, ox, oy);
    pa_tabbar(stdout, 40, 7, 40+x-1, 7+y-1, lp, pa_topa_right(stdout), 2);

    new(lp);
    copy(lp->str, "Right");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Left");
    sp->next = lp;
    lp = sp;
    pa_tabbarsiz(stdout, pa_tobottom, 20, 2, x, y, ox, oy);
    pa_tabbar(stdout, 15, 20, 15+x-1, 20+y-1, lp, pa_tobottom, 3);

    new(lp);
    copy(lp->str, "Bottom");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Top");
    sp->next = lp;
    lp = sp;
    pa_tabbarsiz(stdout, pa_topa_left(stdout), 2, 12, x, y, ox, oy);
    pa_tabbar(stdout, 5, 7, 5+x-1, 7+y-1, lp, pa_topa_left(stdout), 4);

    repeat

        pa_event(stdin, er);
        if er.etype == pa_ettabbar then {

            if er.tabid == 1 then case er.tabsel of

                1: printf("Top bar: You selected Left\n");
                2: printf("Top bar: You selected Center\n");
                3: printf("Top bar: You selected Right\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 2 then case er.tabsel of

                1: printf("Right bar: You selected Top\n");
                2: printf("Right bar: You selected Center\n");
                3: printf("Right bar: You selected Bottom\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 3 then case er.tabsel of

                1: printf("Bottom bar: You selected Left\n");
                2: printf("Bottom bar: You selected Center\n");
                3: printf("Bottom bar: You selected right\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 4 then case er.tabsel of

                1: printf("Left bar: You selected Top\n");
                2: printf("Left bar: You selected Center\n");
                3: printf("Left bar: You selected Bottom\n");
                else printf("!!! Bad select number !!!\n")

            } else printf("!!! Bad tab id !!!\n")

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

   /* ************************* Graphical tab bar test ************************ */

pa_bcolor(stdout, pa_cyan);
    printf("\f");
    printf("Graphical tab bar test\n");
    printf("\n");
    new(lp);
    copy(lp->str, "Right");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Left");
    sp->next = lp;
    lp = sp;
    pa_tabbarsizg(stdout, pa_totop, 200, 20, x, y, ox, oy);
pa_line(stdout, 1, 50, pa_maxxg(stdout), 50);
pa_line(stdout, 150, 1, 150, pa_maxyg(stdout));
    pa_tabbarg(stdout, 150, 50, 150+x-1, 50+y-1, lp, pa_totop, 1);

    new(lp);
    copy(lp->str, "Bottom");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Top");
    sp->next = lp;
    lp = sp;
    pa_tabbarsizg(stdout, pa_toright, 20, 200, x, y, ox, oy);
    pa_tabbarg(stdout, 400, 100, 400+x-1, 100+y-1, lp, toright, 2);

    new(lp);
    copy(lp->str, "Right");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Left");
    sp->next = lp;
    lp = sp;
    pa_tabbarsizg(stdout, pa_tobottom, 200, 20, x, y, ox, oy);
    pa_tabbarg(stdout, 150, 300, 150+x-1, 300+y-1, lp, pa_tobottom, 3);

    new(lp);
    copy(lp->str, "Bottom");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Top");
    sp->next = lp;
    lp = sp;
    pa_tabbarsizg(stdout, pa_toleft, 20, 200, x, y, ox, oy);
    pa_tabbarg(stdout, 50, 100, 50+x-1, 100+y-1, lp, pa_toleft, 4);

    repeat

        pa_event(stdin, er);
        if er.etype == pa_ettabbar then {

            if er.tabid == 1 then case er.tabsel of

                1: printf("Top bar: You selected Left\n");
                2: printf("Top bar: You selected Center\n");
                3: printf("Top bar: You selected Right\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 2 then case er.tabsel of

                1: printf("Right bar: You selected Top\n");
                2: printf("Right bar: You selected Center\n");
                3: printf("Right bar: You selected Bottom\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 3 then case er.tabsel of

                1: printf("Bottom bar: You selected Left\n");
                2: printf("Bottom bar: You selected Center\n");
                3: printf("Bottom bar: You selected right\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 4 then case er.tabsel of

                1: printf("Left bar: You selected Top\n");
                2: printf("Left bar: You selected Center\n");
                3: printf("Left bar: You selected Bottom\n");
                else printf("!!! Bad select number !!!\n")

            } else printf("!!! Bad tab id !!!\n")

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

    /* ************************* Terminal overlaid tab bar test ************************ */

    printf("\f");
    chrgrid();
    pa_binvis(stdout);
    printf("Terminal overlaid tab bar test\n");
    printf("\n");

    new(lp);
    copy(lp->str, "Right");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Left");
    sp->next = lp;
    lp = sp;
    pa_tabbarsiz(stdout, pa_totop, 30, 12, x, y, ox, oy);
    pa_tabbar(stdout, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, pa_totop, 1);

    new(lp);
    copy(lp->str, "Bottom");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Top");
    sp->next = lp;
    lp = sp;
    pa_tabbarsiz(stdout, pa_toright, 30, 12, x, y, ox, oy);
    pa_tabbar(stdout, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, pa_toright, 2);

    new(lp);
    copy(lp->str, "Right");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Left");
    sp->next = lp;
    lp = sp;
    pa_tabbarsiz(stdout, pa_tobottom, 30, 12, x, y, ox, oy);
    pa_tabbar(stdout, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, pa_tobottom, 3);

    new(lp);
    copy(lp->str, "Bottom");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Top");
    sp->next = lp;
    lp = sp;
    pa_tabbarsiz(stdout, pa_toleft, 30, 12, x, y, ox, oy);
    pa_tabbar(stdout, 20-ox, 7-oy, 20+x-ox-1, 7+y-oy-1, lp, pa_toleft, 4);

    repeat

        pa_event(stdin, er);
        if er.etype == pa_ettabbar then {

            if er.tabid == 1 then case er.tabsel of

                1: printf("Top bar: You selected Left\n");
                2: printf("Top bar: You selected Center\n");
                3: printf("Top bar: You selected Right\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 2 then case er.tabsel of

                1: printf("Right bar: You selected Top\n");
                2: printf("Right bar: You selected Center\n");
                3: printf("Right bar: You selected Bottom\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 3 then case er.tabsel of

                1: printf("Bottom bar: You selected Left\n");
                2: printf("Bottom bar: You selected Center\n");
                3: printf("Bottom bar: You selected right\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 4 then case er.tabsel of

                1: printf("Left bar: You selected Top\n");
                2: printf("Left bar: You selected Center\n");
                3: printf("Left bar: You selected Bottom\n");
                else printf("!!! Bad select number !!!\n")

            } else printf("!!! Bad tab id !!!\n")

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

    /* ************************* Graphical overlaid tab bar test ************************ */

    printf("\f");
    printf("Graphical overlaid tab bar test\n");
    printf("\n");
    new(lp);
    copy(lp->str, "Right");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Left");
    sp->next = lp;
    lp = sp;
    pa_tabbarsizg(stdout, pa_totop, 200, 200, x, y, ox, oy);
    pa_tabbarg(stdout, 200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, pa_totop, 1);

    new(lp);
    copy(lp->str, "Bottom");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Top");
    sp->next = lp;
    lp = sp;
    pa_tabbarsizg(stdout, pa_toright, 200, 200, x, y, ox, oy);
    pa_tabbarg(stdout, 200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, pa_toright, 2);

    new(lp);
    copy(lp->str, "Right");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Left");
    sp->next = lp;
    lp = sp;
    pa_tabbarsizg(stdout, pa_tobottom, 200, 200, x, y, ox, oy);
    pa_tabbarg(stdout, 200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, pa_tobottom, 3);

    new(lp);
    copy(lp->str, "Bottom");
    lp->next = nil;
    new(sp);
    copy(sp->str, "Center");
    sp->next = lp;
    lp = sp;
    new(sp);
    copy(sp->str, "Top");
    sp->next = lp;
    lp = sp;
    pa_tabbarsizg(stdout, pa_toleft, 200, 200, x, y, ox, oy);
    pa_tabbarg(stdout, 200-ox, 100-oy, 200+x-ox, 100+y-oy, lp, pa_toleft, 4);

    repeat

        pa_event(stdin, er);
        if er.etype == pa_ettabbar then {

            if er.tabid == 1 then case er.tabsel of

                1: printf("Top bar: You selected Left\n");
                2: printf("Top bar: You selected Center\n");
                3: printf("Top bar: You selected Right\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 2 then case er.tabsel of

                1: printf("Right bar: You selected Top\n");
                2: printf("Right bar: You selected Center\n");
                3: printf("Right bar: You selected Bottom\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 3 then case er.tabsel of

                1: printf("Bottom bar: You selected Left\n");
                2: printf("Bottom bar: You selected Center\n");
                3: printf("Bottom bar: You selected right\n");
                else printf("!!! Bad select number !!!\n")

            } else if er.tabid == 4 then case er.tabsel of

                1: printf("Left bar: You selected Top\n");
                2: printf("Left bar: You selected Center\n");
                3: printf("Left bar: You selected Bottom\n");
                else printf("!!! Bad select number !!!\n")

            } else printf("!!! Bad tab id !!!\n")

        };
        if er.etype == pa_etterm longjmp(terminate_buf, 1)

    until er.etype == pa_etenter;
    pa_killwidget(stdout, 1);
    pa_killwidget(stdout, 2);
    pa_killwidget(stdout, 3);
    pa_killwidget(stdout, 4);

    /* ************************* Alert test ************************ */

    printf("\f");
    printf("Alert test\n");
    printf("\n");
    printf("There should be an pa_alert dialog\n");
    printf("Both the dialog && this window should be fully reactive\n");
    pa_alert("This is an important message", "There has been an event !\n");
    printf("\n");
    printf("Alert dialog should have completed now\n");
    waitnext();

    /* ************************* Color query test ************************ */

    printf("\f");
    printf("Color query test\n");
    printf("\n");
    printf("There should be an pa_color query dialog\n");
    printf("Both the dialog && this window should be fully reactive\n");
    printf("The pa_color pa_white should be the default selection\n");
    r = INT_MAX;
    g = INT_MAX;
    b = INT_MAX;
    querypa_color(r, g, b);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Colors are: pa_red: ", r:1,  " pa_green: ", g:1, " pa_blue: ", b:1, "\n");
    waitnext();

    /* ************************* Open file query test ************************ */

    printf("\f");
    printf("Open file query test\n");
    printf("\n");
    printf("There should be an open file query dialog\n");
    printf("Both the dialog && this window should be fully reactive\n");
    printf("The dialog should have "myfile.txt" as the default filename\n");
    copy(s, "myfile.txt\n");
    pa_queryopen(s);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Filename is: ", s^, "\n");
    waitnext();

    /* ************************* Save file query test ************************ */

    printf("\f");
    printf("Save file query test\n");
    printf("\n");
    printf("There should be an save file query dialog\n");
    printf("Both the dialog && this window should be fully reactive\n");
    printf("The dialog should have "myfile.txt" as the default filename\n");
    copy(s, "myfile.txt\n");
    pa_querysave(s);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Filename is: ", s^, "\n");
    waitnext();

    /* ************************* Find query test ************************ */

    printf("\f");
    printf("Find query test\n");
    printf("\n");
    printf("There should be a find query dialog\n");
    printf("Both the dialog and this window should be fully reactive\n");
    printf("The dialog should have "mystuff" as the default search string\n");
    copy(s, "mystuff");
    optf = [];
    pa_queryfind(s, optf);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Search string is: """, s^, """\n");
    if pa_qfncase in optf then printf("Case sensitive is on\n")
    else printf("Case sensitive is off\n");
    if pa_qfnpa_up(stdout) in optf then printf("Search up\n")
    else printf("Search down\n");
    if pa_qfnre in optf then printf("Use regular expression\n")
    else printf("Use literal expression\n");
    waitnext();

    /* ************************* Find/replace query test ************************ */

    printf("\f");
    printf("Find/replace query test\n");
    printf("\n");
    printf("There should be a find/replace query dialog\n");
    printf("Both the dialog && this window should be fully reactive\n");
    printf("The dialog should have "bark" as the default search string\n");
    printf("and should have "sniff" as the default replacement string\n");
    copy(ss, "bark");
    copy(rs, "sniff");
    optfr = [];
    pa_queryfindrep (ss, rs, optfr);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Search char* is: """, ss^, """\n");
    printf("Replace char* is: """, rs^, """\n");
    if qfrcase in optfr then printf("Case sensitive is on\n")
    else printf("Case sensitive is off\n");
    if qfrpa_up(stdout) in optfr then printf("Search/replace up\n")
    else printf("Search/replace down\n");
    if qfrre in optfr then printf("Regular expressions are on\n")
    else printf("Regular expressions are off\n");
    if qfrfind in optfr then printf("Mode is find\n")
    else printf("Mode is find/replace\n");
    if qfrallfil in optfr then printf("Mode is find/replace all in file\n")
    else printf("Mode is find/replace first in file\n");
    if qfralllin in optfr then printf("Mode is find/replace all on line(s)\n")
    else printf("Mode is find/replace first on line(s)\n");
    waitnext();

    /* ************************* Font query test ************************ */

    printf("\f");
    printf("Font query test\n");
    printf("\n");
    printf("There should be a font query dialog\n");
    printf("Both the dialog && this window should be fully reactive\n");
    fc = pa_font_book;
    fs = pa_chrsizy(stdout);
    fr = 0; /* set foreground to pa_black */
    fg = 0;
    fb = 0;
    br = INT_MAX; /* set pa_back(stdout)ground to pa_white */
    bg = INT_MAX;
    bb = INT_MAX;
    fe = [];
    querypa_font(stdout, output, fc, fs, fr, fg, fb, br, bg, bb, fe);
    printf("\n");
    printf("Dialog should have completed now\n");
    printf("Font code: ", fc:1, "\n");
    printf("Font size: ", fs:1, "\n");
    printf("Foreground pa_color: Red: ", fr:1, " Green: ", fg:1, " Blue: ", fb:1, "\n");
    printf("Background pa_color: Red: ", br:1, " Green: ", bg:1, " Blue: ", bb:1, "\n");
    if qfteblink in fe then printf("Blink\n");
    if qftereverse in fe then printf("Reverse\n");
    if qfteunderline in fe then printf("Underline\n");
    if qftespa_up(stdout)erscript in fe then printf("Superscript\n");
    if qftesubscript in fe then printf("Subscript\n");
    if qfteitalic in fe then printf("Italic\n");
    if qftebold in fe then printf("Bold\n");
    if qftestrikeout in fe then printf("Strikeout\n");
    if qftestandout in fe then printf("Standout\n");
    if qftecondensed in fe then printf("Condensed\n");
    if qfteext}ed in fe then printf("Extended\n");
    if qftexlight in fe then printf("Xlight\n");
    if qftelight in fe then printf("Light\n");
    if qftexbold in fe then printf("Xbold\n");
    if qftehollow in fe then printf("Hollow\n");
    if qfteraised in fe then printf("Raised\n");
    waitnext();

    terminate:;

    printf("\f");
    printf("Test complete\n");

}
