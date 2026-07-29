/*******************************************************************************
*                                                                              *
*                    MANAGERC CHARACTER WIDGETS TEST                           *
*                                                                              *
* Places one of each implemented widget type directly in the main window,     *
* the surface that occupies the whole terminal. Widget notifications are       *
* reported in the root window as they arrive, so each widget can be exercised *
* by hand and its events watched: click the widgets, or focus one (click it)   *
* and use the keyboard: space activates buttons and toggles, arrows work the   *
* number select and list boxes, and the edit box takes full line editing      *
* with enter to complete.                                                      *
*                                                                              *
* The program keeps the widget states current itself: the checkbox and radio   *
* button select when their events arrive, which is the standard Petit-Ami      *
* division of labor (widgets notify, the program sets state). The slider and   *
* scroll bar positions are echoed into the progress bar, so dragging value     *
* widgets shows visible effect.                                                *
*                                                                              *
* Build (managerc is off in the default build):                                *
*                                                                              *
*     make lib/petit_ami_term.so USEMANAGERC=1                                 *
*     make test_manager2                                                       *
*     ./test_manager2                                                          *
*                                                                              *
* Press q in the main window to quit.                                          *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <localdefs.h>
#include <terminal.h>

/* widget ids */
#define WBUTTON   1
#define WCHECK    2
#define WRADIO1   3
#define WRADIO2   4
#define WGROUP    5
#define WEDIT     6
#define WNUMSEL   7
#define WPROG     8
#define WLIST     9
#define WSCROLLV  10
#define WSCROLLH  11
#define WSLIDEH   12

static FILE* win;          /* where the widgets live: the main window */
static long  evcnt;        /* number of reports made */

/* Report a widget notification in the root window. Reports flow like
   terminal output: when they reach the bottom of the terminal the root
   window scrolls, and the widgets, which are free floating windows, stay
   where they are while the text passes beneath them. */
static void report(const char* msg)

{

    printf("%4ld: %s\n", ++evcnt, msg);

}

int main(void)

{

    ami_evtrec er;
    ami_strrec s1, s2, s3, s4;
    char       buff[250];
    char       text[250];
    int        done;

    ami_curvis(stdout, FALSE);
    /* auto stays on: the root window behaves as a terminal, scrolling at
       the bottom */
    printf("\f");
    printf("Managerc widget test\n");
    printf("\n");
    printf("One of each widget is presented at the right. Exercise\n");
    printf("them and watch the notifications below. Click focuses a\n");
    printf("widget; space, arrows and editing keys work on the focus.\n");
    printf("The scroll bars and slider echo their position into the\n");
    printf("progress bar.\n");
    printf("\n");
    printf("Press q in the main window to quit.\n");
    printf("\n");
    printf("--- widget notifications ---\n");
    evcnt = 0;

    /* the widgets live directly in the main window, to the right of the
       instructions and report area */
    win = stdout;

    /* left column of the widget area: the click widgets */
    ami_button(win, 56, 3, 69, 3, "Press me", WBUTTON);
    ami_checkbox(win, 56, 5, 70, 5, "Check me", WCHECK);
    ami_radiobutton(win, 56, 7, 70, 7, "Radio one", WRADIO1);
    ami_radiobutton(win, 56, 8, 70, 8, "Radio two", WRADIO2);
    ami_progbar(win, 56, 10, 70, 10, WPROG);

    /* a group holding the value widgets */
    ami_group(win, 73, 2, 98, 9, "Values", WGROUP);
    ami_editbox(win, 75, 4, 96, 4, WEDIT);
    ami_putwidgettext(win, WEDIT, "edit me");
    ami_numselbox(win, 75, 6, 83, 6, 1, 99, WNUMSEL);
    s1.str = "apple";  s1.next = &s2;
    s2.str = "banana"; s2.next = &s3;
    s3.str = "cherry"; s3.next = &s4;
    s4.str = "damson"; s4.next = NULL;
    ami_listbox(win, 56, 12, 68, 15, &s1, WLIST);

    /* bars */
    ami_scrollvert(win, 100, 2, 100, 15, WSCROLLV);
    ami_scrollsiz(win, WSCROLLV, LONG_MAX/4);
    ami_scrollhoriz(win, 56, 17, 98, 17, WSCROLLH);
    ami_scrollsiz(win, WSCROLLH, LONG_MAX/4);
    ami_slidehoriz(win, 56, 19, 98, 19, 0, WSLIDEH);

    done = FALSE;
    do {

        ami_event(stdin, &er);
        switch (er.etype) {

            case ami_etterm: done = TRUE; break;

            case ami_etchar:
                if (er.echar == 'q' || er.echar == 'Q') done = TRUE;
                break;

            case ami_etbutton:
                snprintf(buff, sizeof(buff), "button %ld pressed", er.butid);
                report(buff);
                break;

            case ami_etchkbox:
                /* the program owns the state: toggle it */
                ami_selectwidget(win, er.ckbxid, evcnt%2 == 0);
                snprintf(buff, sizeof(buff), "checkbox %ld clicked", er.ckbxid);
                report(buff);
                break;

            case ami_etradbut:
                /* select the clicked one, clear the other */
                ami_selectwidget(win, WRADIO1, er.radbid == WRADIO1);
                ami_selectwidget(win, WRADIO2, er.radbid == WRADIO2);
                snprintf(buff, sizeof(buff), "radio button %ld selected",
                         er.radbid);
                report(buff);
                break;

            case ami_etsclull:
                snprintf(buff, sizeof(buff), "scroll %ld up/left line",
                         er.sclulid);
                report(buff);
                break;

            case ami_etscldrl:
                snprintf(buff, sizeof(buff), "scroll %ld down/right line",
                         er.scldrid);
                report(buff);
                break;

            case ami_etsclulp:
                snprintf(buff, sizeof(buff), "scroll %ld up/left page",
                         er.sclupid);
                report(buff);
                break;

            case ami_etscldrp:
                snprintf(buff, sizeof(buff), "scroll %ld down/right page",
                         er.scldpid);
                report(buff);
                break;

            case ami_etsclpos:
                snprintf(buff, sizeof(buff), "scroll %ld position %ld%%",
                         er.sclpid, er.sclpos/(LONG_MAX/100+1));
                report(buff);
                ami_progbarpos(win, WPROG, er.sclpos);
                break;

            case ami_etedtbox:
                ami_getwidgettext(win, er.edtbid, text, sizeof(text));
                snprintf(buff, sizeof(buff), "edit box %ld: \"%s\"",
                         er.edtbid, text);
                report(buff);
                break;

            case ami_etnumbox:
                snprintf(buff, sizeof(buff), "number box %ld value %ld",
                         er.numbid, er.numbsl);
                report(buff);
                break;

            case ami_etlstbox:
                snprintf(buff, sizeof(buff), "list box %ld entry %ld",
                         er.lstbid, er.lstbsl);
                report(buff);
                break;

            case ami_etsldpos:
                snprintf(buff, sizeof(buff), "slider %ld position %ld%%",
                         er.sldpid, er.sldpos/(LONG_MAX/100+1));
                report(buff);
                ami_progbarpos(win, WPROG, er.sldpos);
                break;

            default: ;

        }

    } while (!done);

    return (0);

}
