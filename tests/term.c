/*******************************************************************************

TERMINAL EMULATOR

This is a simple program written to the Petit Ami terminal standard that just
allows the user to scribble text on the screen. Implements the basic positioning
keys, etc.

This is used mostly for testing.

*******************************************************************************/

#include "terminal.h" /* terminal level functions */

int main(int argc, char *argv[])

{

    pa_evtrec er;   /* record for returned events */
    int autostate;  /* state of automatic wrap and scroll */
    long buf;       /* current terminal buffer */
    int fbold;      /* bold active flag */
    int fundl;      /* underline active flag */
    int fstko;      /* strikeout active flag */
    int fital;      /* italic active flag */
    int fsubs;      /* subscript active flag */
    int fsups;      /* superscript active flag */
    pa_color color; /* current color */

    buf = 1;          /* set normal buffer */
    fbold = 0;        /* set bold off */
    fundl = 0;        /* set underline off */
    fstko = 0;        /* set strikeout off */
    fital = 0;        /* set italic off */
    fsubs = 0;        /* set subscript off */
    fsups = 0;        /* set superscript off */
    autostate = 1;    /* set auto on */
    color = pa_black; /* set black foreground */
    printf("Terminal emulator test program 1.0\n");
    do { /* event loop */

        pa_event(stdin, &er); /* get the next event */
        switch (er.etype) {   /* event */

             /* pass character to output */
             case pa_etchar: putchar(er.echar); break;
             /* enter line */
             case pa_etenter: pa_cursor(stdout, 1, pa_cury(stdout)); pa_down(stdout); break;
             case pa_etup:    pa_up(stdout); break; /* up one line */
             case pa_etdown:  pa_down(stdout); break; /* down one line */
             case pa_etleft:  pa_left(stdout); break; /* left one character */
             case pa_etright: pa_right(stdout); break; /* right one character */
             case pa_ethomes: pa_home(stdout); break; /* home screen */
             /* home line */
             case pa_ethomel: pa_cursor(stdout, 1, pa_cury(stdout)); break;
             /* end screen */
             case pa_etends:  pa_cursor(stdout, pa_maxx(stdout), pa_maxy(stdout)); break;
             /* end line */
             case pa_etendl:  pa_cursor(stdout, pa_maxx(stdout), pa_cury(stdout)); break;
             case pa_etscrl:  pa_scroll(stdout, -1, 0); break; /* scroll left */
             case pa_etscrr:  pa_scroll(stdout, 1, 0); break; /* scroll right */
             case pa_etscru:  pa_scroll(stdout, 0, -1); break; /* scroll up */
             case pa_etscrd:  pa_scroll(stdout, 0, 1); break; /* scroll down */
             case pa_etdelcb: pa_del(stdout); break;  /* delete left character */
             case pa_ettab:   printf("\t"); break;/* tab */
             case pa_etinsertt: autostate = !autostate;
                 pa_auto(stdout, autostate); break;
             case pa_etfun:  /* function key */

                 if (er.fkey == 1) {  /* function 1: swap screens */

                     if (buf == 10) buf = 1;  /* wrap buffer back to zero */
                     else buf++; /* next buffer */
                     pa_select(stdout, buf, buf);

                 } else if (er.fkey == 2) {  /* function 2: bold toggle */

                     fbold = !fbold;   /* toggle */
                     pa_bold(stdout, fbold); /* apply */

                 } else if (er.fkey == 3) {  /* function 3: underline toggle */

                     fundl = !fundl; /* toggle */
                     pa_underline(stdout, fundl); /* apply */

                 } else if (er.fkey == 4) {  /* function 4: strikeout toggle */

                     fstko = !fstko; /* toggle */
                     pa_strikeout(stdout, fstko); /* apply */


                 } else if (er.fkey == 5) {  /* function 5: italic toggle */

                     fital = !fital;   /* toggle */
                     pa_italic(stdout, fital); /* apply */

                 } else if (er.fkey == 6) {  /* function 6: subscript toggle */

                     fsubs = !fsubs;   /* toggle */
                     pa_subscript(stdout, fsubs); /* apply */

                 } else if (er.fkey == 7) {  /* function 7: superscript toggle */

                     fsups = !fsups;   /* toggle */
                     pa_superscript(stdout, fsups); /* apply */

                 } else if (er.fkey == 8) {

                     color++; /* next color */
                     /* wrap color */
                     if (color > pa_magenta) color = pa_black;
                     pa_bcolor(stdout, color);

                 }
                 break;
             default: ;

        }

    } while (er.etype != pa_etterm); /* until termination signal */

    return (0); /* return no error */

}
