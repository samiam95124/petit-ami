/*******************************************************************************
*                                                                              *
*                    MANAGERC SUBWINDOW BEHAVIOR TEST                          *
*                                                                              *
* Opens two subwindows over the character mode window manager (managerc),      *
* labels them, and fills each with this program's own source text, so the      *
* behavior of subwindows can be examined by hand: moving, resizing,            *
* minimize/maximize from the system bar, focus changes, occlusion between     *
* the two overlapping windows, and the performance of the surface updates      *
* while doing all of that. Filling the windows scrolls the text through        *
* them, which exercises the scroll path as well; the windows are buffered,     *
* so the manager keeps the text present over any rearrangement.                *
*                                                                              *
* Build:                                                                       *
*                                                                              *
*     make test_manager                                                        *
*     ./test_manager                                                           *
*                                                                              *
* The main (root) window holds instructions and a running event trace at the   *
* bottom, so window management events can be watched as they arrive. Press     *
* "f" to refill both windows with the source text, repeating the scroll load   *
* on demand; press "q" in any window to quit.                                  *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <string.h>

#include <localdefs.h>
#include <terminalw.h>

#define OFF 0
#define ON  1

/* event names for the trace, indexed by ami_evtcod */
static const char* evtnam(ami_evtcod e)

{

    switch (e) {

        case ami_etchar:   return "char";
        case ami_etenter:  return "enter";
        case ami_etterm:   return "term";
        case ami_etmoumov: return "moumov";
        case ami_etmouba:  return "mouba";
        case ami_etmoubd:  return "moubd";
        case ami_etredraw: return "redraw";
        case ami_etresize: return "resize";
        case ami_etfocus:  return "focus";
        case ami_etnofocus: return "nofocus";
        case ami_ethover:  return "hover";
        case ami_etnohover: return "nohover";
        case ami_etmax:    return "max";
        case ami_etmin:    return "min";
        case ami_etnorm:   return "norm";
        default:           return NULL; /* don't trace the rest */

    }

}

/* fill both windows with this program's own source text */
static void fillwin(FILE* win1, FILE* win2)

{

    FILE* fp;
    char  buff[250];

    fp = fopen(__FILE__, "r");
    if (fp) {

        while (fgets(buff, sizeof(buff), fp)) {

            fputs(buff, win1);
            fputs(buff, win2);

        }
        fclose(fp);

    } else {

        fprintf(win1, "cannot open %s: run from the repo root\n", __FILE__);
        fprintf(win2, "cannot open %s: run from the repo root\n", __FILE__);

    }

}

int main(void)

{

    FILE*      win1;  /* subwindow 1 */
    FILE*      win2;  /* subwindow 2 */
    ami_evtrec er;    /* event record */
    long       done;
    long       evline; /* trace line in root window */
    long       evcnt;  /* events traced */
    const char* en;
    char       buff[250];

    /* the root window: instructions, and it stays behind the subwindows */
    ami_curvis(stdout, OFF);
    ami_auto(stdout, OFF);
    printf("\f");
    printf("Managerc subwindow test\n");
    printf("\n");
    printf("Two subwindows should be present, each labeled.\n");
    printf("Try: dragging by the system bar, resizing by the edges,\n");
    printf("minimize/maximize/close from the system bar buttons,\n");
    printf("clicking between windows for focus.\n");
    printf("\n");
    printf("Press f to refill both windows (repeat the scroll load).\n");
    printf("Press q in any window to quit.\n");
    evline = ami_maxy(stdout)-10; /* trace area at the bottom */
    ami_cursor(stdout, 1, evline-1);
    printf("--- window management events ---");
    evcnt = 0;

    /* first subwindow */
    ami_openwin(&stdin, &win1, stdout, 2);
    ami_setsiz(win1, 60, 16);
    /* Match the buffer to the client area (window less frame and system
       bar). The default buffer is near full screen, and managerc's scroll
       and restore paths misdraw when the buffer is larger than the client:
       they paint the entire buffer at the window position. */
    ami_sizbuf(win1, 58, 12);
    ami_setpos(win1, 4, 3);
    fprintf(win1, "I am window 1\n");

    /* second subwindow, overlapping the first so the occlusion redraw is
       exercised when they move over each other */
    ami_openwin(&stdin, &win2, stdout, 3);
    ami_setsiz(win2, 60, 16);
    ami_sizbuf(win2, 58, 12);
    ami_setpos(win2, 40, 10);
    fprintf(win2, "I am window 2\n");

    /* Fill both windows with this program's own source. The text scrolls
       through each window, which exercises the scroll path on the way in,
       and the windows are buffered, so the text stays put over moves,
       resizes, and occlusion by the other window. */
    fillwin(win1, win2);

    done = FALSE;
    do {

        ami_event(stdin, &er);
        if (er.etype == ami_etterm) done = TRUE;
        else if (er.etype == ami_etchar && (er.echar == 'q' || er.echar == 'Q'))
            done = TRUE;
        else if (er.etype == ami_etchar && (er.echar == 'f' || er.echar == 'F'))
            /* refill on demand, so the scroll load can be repeated while
               watching (and timed) */
            fillwin(win1, win2);
        else {

            /* trace interesting events in the root window */
            en = evtnam(er.etype);
            if (en) {

                sprintf(buff, "%4ld: window %ld: %-8s", ++evcnt, er.winid, en);
                ami_cursor(stdout, 1, evline+(evcnt%10));
                printf("%-40s", buff);

            }

        }

    } while (!done);

    fclose(win2);
    fclose(win1);

    return (0);

}
