/*******************************************************************************

TERMINAL EMULATOR

This is a simple program written to the Petit Ami terminal standard that just
allows the user to scribble text on the screen. Implements the basic positioning
keys, etc.

This is used mostly for testing.

*******************************************************************************/

#include "terminal.h" /* terminal level functions */

main(int argc, char *argv[])

{

    evtrec er;     /* record for returned events */
    int autostate; /* state of automatic wrap and scroll */
    long buf;      /* current terminal buffer */
    int fbold;     /* bold active flag */
    int fundl;     /* underline active flag */
    int fstko;     /* strikeout active flag */
    int fital;     /* italic active flag */
    int fsubs;     /* subscript active flag */
    int fsups;     /* superscript active flag */

    buf = 1;       /* set normal buffer */
    fbold = 0;     /* set bold off */
    fundl = 0;     /* set underline off */
    fstko = 0;     /* set strikeout off */
    fital = 0;     /* set italic off */
    fsubs = 0;     /* set subscript off */
    fsups = 0;     /* set superscript off */
    autostate = 1; /* set auto on */
    do { /* event loop */

        event(stdin, &er); /* get the next event */
        switch (er.etype) {   /* event */

             /* pass character to output */
             case etchar: putchar(er.echar); break;
             /* enter line */
             case etenter: cursor(stdout, 1, cury(stdout)); down(stdout); break;
             case etup:    up(stdout); break; /* up one line */
             case etdown:  down(stdout); break; /* down one line */
             case etleft:  left(stdout); break; /* left one character */
             case etright: right(stdout); break; /* right one character */
             case ethomes: home(stdout); break; /* home screen */
             /* home line */
             case ethomel: cursor(stdout, 1, cury(stdout)); break;
             /* end screen */
             case etends:  cursor(stdout, maxx(stdout), maxy(stdout)); break;
             /* end line */
             case etendl:  cursor(stdout, maxx(stdout), cury(stdout)); break;
             case etscrl:  scroll(stdout, -1, 0); break; /* scroll left */
             case etscrr:  scroll(stdout, 1, 0); break; /* scroll right */
             case etscru:  scroll(stdout, 0, -1); break; /* scroll up */
             case etscrd:  scroll(stdout, 0, 1); break; /* scroll down */
             case etdelcb: del(stdout); break;  /* delete left character */
             case ettab:   printf("\t"); break;/* tab */
             case etinsertt: autostate = !autostate;
                 automode(stdout, autostate); break;
             case etfun:  /* function key */

                 if (er.fkey == 1) {  /* function 1: swap screens */

                     if (buf == 10) buf = 1;  /* wrap buffer back to zero */
                     else buf++; /* next buffer */
                     select(stdout, buf, buf);

                 } else if (er.fkey == 2) {  /* function 2: bold toggle */

                     fbold = !fbold;   /* toggle */
                     bold(stdout, fbold); /* apply */

                 } else if (er.fkey == 3) {  /* function 3: underline toggle */

                     fundl = !fundl; /* toggle */
                     underline(stdout, fundl); /* apply */

                 } else if (er.fkey == 4) {  /* function 4: strikeout toggle */

                     fstko = !fstko; /* toggle */
                     strikeout(stdout, fstko); /* apply */


                 } else if (er.fkey == 5) {  /* function 5: italic toggle */

                     fital = !fital;   /* toggle */
                     italic(stdout, fital); /* apply */

                 } else if (er.fkey == 6) {  /* function 6: subscript toggle */

                     fsubs = !fsubs;   /* toggle */
                     subscript(stdout, fsubs); /* apply */

                 } else if (er.fkey == 7) {  /* function 7: superscript toggle */

                     fsups = !fsups;   /* toggle */
                     superscript(stdout, fsups); /* apply */

                 } else if (er.fkey == 8) bcolor(stdout, cyan);
                 break;

        }

    } while (er.etype != etterm); /* until termination signal */

}
