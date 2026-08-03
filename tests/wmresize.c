/*******************************************************************************
*                                                                              *
*                     WINDOW MANAGER RESIZE, NOT REPORTED                      *
*                                                                              *
* Opens a window with a child pane in it and prints every event that comes.    *
* Resize the window with the program's own call and an ami_etresize arrives.   *
* Resize it by dragging its frame and nothing arrives at all -- yet the pane   *
* has been resized and cleared by then, so the program cannot know it has to   *
* paint what the drag exposed.                                                 *
*                                                                              *
* Run it, then drag the window's right or bottom edge:                         *
*                                                                              *
*     bin/wmresize                                                             *
*                                                                              *
* Every event is printed with its size fields and what the window answers      *
* when asked its own size. What to look for is a line appearing when the       *
* drag happens. The program also draws a band down the right and along the     *
* bottom of the pane, so the exposed space shows plainly: white where the      *
* program has painted, background where it has not.                            *
*                                                                              *
* Options:                                                                     *
*                                                                              *
*     self     resize once from the program after two seconds, for contrast:   *
*              that one is reported                                            *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <string.h>

#include <localdefs.h>
#include <graphics.h>

#define PANE 2 /* the child pane */

/* paint the pane so its edges can be seen */
static void paint(FILE* f, long w, long h)

{

    fprintf(f, "\f");
    ami_fcolor(f, ami_cyan);
    ami_frect(f, 0, 0, w-1, h-1);
    ami_fcolor(f, ami_black);
    ami_cursorg(f, 10, 10);
    fprintf(f, "pane painted for %ld by %ld\n", w, h);

}

int main(int argc, char* argv[])

{

    FILE*      pane;
    ami_evtrec er;
    long       wx, wy;
    long       w, h;
    long       i;
    long       n = 0;
    int        self = FALSE;

    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "self")) self = TRUE;

    ami_autohold(FALSE);
    ami_curvis(stdout, FALSE);
    ami_auto(stdout, FALSE);
    ami_winclientg(stdout, 700, 500, &wx, &wy,
                   BIT(ami_wmframe) | BIT(ami_wmsize) | BIT(ami_wmsysbar));
    ami_setsizg(stdout, wx, wy);
    fprintf(stdout, "\f");
    ami_cursorg(stdout, 10, 10);
    fprintf(stdout, "Drag the frame. Every event is printed on stderr.\n");

    /* a pane inside it, as a program with panes has */
    ami_openwin(&stdin, &pane, stdout, PANE);
    ami_frame(pane, FALSE);
    ami_auto(pane, FALSE);
    ami_curvis(pane, FALSE);
    w = ami_maxxg(stdout)-40;
    h = ami_maxyg(stdout)-80;
    ami_setposg(pane, 20, 60);
    ami_setsizg(pane, w, h);
    paint(pane, w, h);

    if (self) { /* one resize of our own, which is reported */

        ami_setsizg(stdout, wx+150, wy+100);
        fprintf(stderr, "asked for %ld by %ld from the program\n",
                wx+150, wy+100);

    }
    do {

        ami_event(stdin, &er);
        /* everything, so that nothing can be said to have been filtered */
        fprintf(stderr, "%4ld: event %2d  window %ld  size %ldx%ld  "
                "main says %ldx%ld\n", ++n, (int)er.etype, er.winid,
                er.rszxg, er.rszyg, ami_maxxg(stdout), ami_maxyg(stdout));
        /* Only the main window's own resizes: the pane is resized here
           too, and its resize comes back as an event like any other. A
           program that acts on those resizes the pane again on the
           strength of the pane's own event, which chases itself down to
           a negative size. */
        if (er.etype == ami_etresize && er.winid == 1) {

            w = er.rszxg-40;
            h = er.rszyg-80;
            ami_setsizg(pane, w, h);
            paint(pane, w, h);

        }

    } while (er.etype != ami_etterm);

    return (0);

}
