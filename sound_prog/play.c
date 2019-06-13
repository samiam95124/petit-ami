/*******************************************************************************

Player example file

Implements a Qbasic compatible "play" statement, and feeds it a sample song.
The theory here is that using an existing music notation will give us lots
of test material.

This website gives and overview of the notation. I don't think we implement
all of it.

https://www.qbasic.net/en/reference/qb11/Statement/PLAY-006.htm

*******************************************************************************/

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

#include "terminal.h" /* terminal level API */
#include "sound.h"    /* sound API */

#define SECOND 10000

int  ntime;  /* normal beat time in quarter notes */
note octave; /* current octave */
int  deftim; /* default note time */
int  i;

void wait(int t)

{

    pa_evtrec er; /* event record */

    pa_timer(stdin, 1, t, false);
    do { pa_event(stdin, &er); } while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) exit(0);

}

void playnote(note n, int nt)

{

/*printf("Note: %d Time: %d\n", n, nt);*/
   pa_noteon(1, 0, 1, n, INT_MAX); /* turn on the note */
   wait(nt); /* wait time */
   pa_noteoff(1, 0, 1, n, INT_MAX); /* turn off the note */

}

void settim(int* t, int ln)

{

    while (ln > 4) { /* process tempo levels */
   
        *t = *t/2;
        ln = ln/2;

    }
    if (ln == 1) *t = *t*4; /* set whole note */
    else if (ln == 2) *t = *t*2; /* 1/2 note */

}

void setoct(int on)

{

    if (on < 0 || on > 6) { /* bad octave */

        printf("*** Play: bad octave number\n");
        exit(1);

    }
    /* we place Plays' 6 7 octaves in the middle of midis' 11 octaves */
    switch (on) { /* octave */

        case 0: octave = PA_OCTAVE_2; break;
        case 1: octave = PA_OCTAVE_3; break;
        case 2: octave = PA_OCTAVE_4; break;
        case 3: octave = PA_OCTAVE_5; break;
        case 4: octave = PA_OCTAVE_6; break;
        case 5: octave = PA_OCTAVE_7; break;
        case 6: octave = PA_OCTAVE_8; break;

    }

}

/* Qbasic compatible "play" string command */

void play(string ms)

{

    note n;
    int nt;
    int x;
    int i;

    while (*ms) { /* interpret commands*/

        /* process single notes */
        if (tolower(*ms) >= 'a' && tolower(*ms) <= 'g') {

            printf("%c ", *ms);
            /* its a note */
            switch (tolower(*ms)) { /* note */

                case 'c': n = PA_NOTE_C; break;
                case 'd': n = PA_NOTE_D; break;
                case 'e': n = PA_NOTE_E; break;
                case 'f': n = PA_NOTE_F; break;
                case 'g': n = PA_NOTE_G; break;
                case 'a': n = PA_NOTE_A; break;
                case 'b': n = PA_NOTE_B;  break;

            }
            ms++; /* next character */
            if (*ms == '+' || *ms == '#') {

                n++; /* sharpen it */
                ms++;

            } else if (*ms == '-') {

                n--; /* flatten it */
                ms++;

            }
            nt = deftim; /* set default whole note */
            /* check length follows, and set time from that */
            if (*ms >= '0' && *ms <= '9') {

                nt = ntime; /* reset to main time */
                settim(&nt, strtol(ms, &ms, 10));

            }
            if (*ms == '.') { /* dotted length */

                nt = nt+nt/2; /* extend by 1/2 */
                ms++;

            }
            /* note is fully prepared, send it */
            playnote(n+octave, nt);

        } else if (tolower(*ms) == 'o') { /* set octave */

            ms++; /* advance */
            setoct(strtol(ms, &ms, 10)); /* set octave */

        } else if (tolower(*ms) == 'l') { /* set note lengths */

            ms++; /* advance */
            deftim = ntime; /* reset default */
            settim(&deftim, strtol(ms, &ms, 10)); /* set default time */

        } else if (*ms == '>') { /* up octave */

            if (octave < PA_OCTAVE_8) octave = octave+12;
            ms++;

        } else if (*ms == '<') { /* down octave */

            if (octave > PA_OCTAVE_2) octave = octave-12;
            ms++;

        } else if (tolower(*ms) == 'n') { /* numbered note */

            ms++; /* advance */
            i = strtol(ms, &ms, 10); /* get the note */
            if (i < 0 && i > 84) {

                printf("*** Play: Invalid note number\n");
                exit(1);

            }
            if (i == 0) wait(ntime); /* rest */
            else playnote(i-1+PA_OCTAVE_2, deftim); /* play note */

        } else if (tolower(*ms) == 'p') { /* pause */

            ms++; /* advance */
            settim(&x, strtol(ms, &ms, 10)); /* get time */
            wait(x); /* wait for that time */
         
        } else if (tolower(*ms) == 't') { /* tempo */

            /* not implemented, just skip */
            ms++; /* advance */
            x = strtol(ms, &ms, 10); /* get time */
         
        } else if (tolower(*ms) == 'm') { /* various commands */

            ms++; /* advance */
            if (tolower(*ms) != 'n' && tolower(*ms) != 'l' &&
                tolower(*ms) != 's' && tolower(*ms) != 'f' &&
                tolower(*ms) != 'b') {

                printf("*** Play: command syntax error\n");
                exit(1);

            }
            ms++; /* advance */

        } else if (*ms == ' ') ms++; /* skip spaces */
        else {

            printf("*** Play: command syntax error\n");
            exit(1);

        }

    }

}

int main(void)

{

    /* set up "play" parameters */
    ntime = SECOND/2; /* set default tempo for quarter notes */
    octave = PA_OCTAVE_5; /* start in middle octave */
    deftim = ntime; /* set whole notes default */

    printf("Synthesisers: %d\n", pa_synthout());
    pa_opensynthout(1); /* open main synthesiser */

    pa_instchange(1, 0, 1, PA_INST_ACOUSTIC_GRAND);

    printf("Mozart''s Sonata in C\n");
    play("c2 l4 e g < b. > l16 c d l2 c");
    play("> a l4 g > c < g l16 g f e f l2 e");
    play("< a8 l16 b > c d e f g a g f e d c < b a");
    play("g8 a b > c d e f g f e d c < b a g f8 g a b > c d e");
    play("f e d c < b a g f e8 f g a b > c d e d c < b a g f e");
    play("d8 e f g a b > c# d < a b > c# d e f g");
    play("a b > c < b a g f e f g a g f e d c");
    play("< l8 b ms > g e c ml d g ms e c");
    play("d4 g4 < g2 g2 > c4 e4 g2");
    play("l16 a g f e f e d c e d e d e d e d e d e d e d c d");
    play("c4 c < g > c e g e c e f d < b > d");
    play("c4 < c < g > c e g e c e f d < b > d c4 > c4 c2");
    printf("\n");

   return 0;

}
