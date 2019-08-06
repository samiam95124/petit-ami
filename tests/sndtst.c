/*******************************************************************************

Sound library test program

Goes through various test cases on the sound library.

Notes:

1. The MIDI tests not only test sound.c, but also the synthesizer
implementation.

*******************************************************************************/

#include <limits.h>
#include <setjmp.h>

#include <terminal.h> /* terminal level functions */
#include <sound.h>    /* sound library */

#define SECOND 10000 /* one second */

/* global variables */
static jmp_buf terminate_buf;

/*******************************************************************************

Wait time

wait time in 100 microseconds.

*******************************************************************************/

static void wait(int t)

{

    pa_evtrec er; /* event record */

    pa_timer(stdout, 1, t, 0);
    do { pa_event(stdin, &er);
    } while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) { longjmp(terminate_buf, 1); }

}

/*******************************************************************************

Wait user interaction

Wait return to be pressed, or handle terminate.

*******************************************************************************/

static void waitnext(void)

{

    pa_evtrec er; /* event record */

    do { pa_event(stdin, &er);
    } while (er.etype != pa_etenter && er.etype != pa_etterm);
    if (er.etype == pa_etterm) { longjmp(terminate_buf, 1); }

}

/*******************************************************************************

Wait return

Prints a message and waits for return to be pressed, or handle terminate.

*******************************************************************************/

static void waitret(void)

{

    printf("Hit return to continue\n");
    waitnext();

}

/*******************************************************************************

Play random notes

Plays random notes on the current instrument, for a given number of notes, to
the given port.

*******************************************************************************/

void playrand(int port, int notes)

{

    int key;
    int i;

    srand(42);
    for( i = 0; i < notes; i++) {

        /* Generate a random key */
        key = 60 + (int)(12.0f * rand() / (float) RAND_MAX)-1;
        /* Play a note */
        pa_noteon(port, 0, 1, key, INT_MAX);
        /* Sleep for 1/10 second */
        wait(SECOND/20);
        /* Stop the note */
        pa_noteoff(port, 0, 1, key, 0);
        /* Sleep for 1/10 second */
        wait(SECOND/20);

    }

}

int main(int argc, char *argv[])


{

    int           dport; /* output port */
    pa_note       n; /* note */
    int           o; /* octave */
    pa_instrument ins; /* instrument */
    int           i;

    if (setjmp(terminate_buf)) goto terminate;

    dport = PA_SYNTH_OUT; /* index standard synth output port */
    pa_opensynthout(dport);

    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);

    printf("Sound library test\n\n");
    printf("Runs through various sound tests and gives you a chance to\n");
    printf("evaluate if the sound produced matches the description.\n\n");
    waitret();
    printf("Run through the entire scale of notes available\n");
    for (n = PA_NOTE_C+PA_OCTAVE_1; n <= PA_NOTE_G+PA_OCTAVE_11; n++) {

        printf("%d ", n);
        pa_noteon(dport, 0, 1, n+o, INT_MAX);
        wait(SECOND/10);
        pa_noteoff(dport, 0, 1, n+o, 0);

    }
    printf("\n");
    printf("Complete\n");
    waitret();

    printf("Run through all instruments with middle C\n");
    printf("Note that not all syths implement all instruments\n");
    printf("Instruments: ");
    for (ins = PA_INST_ACOUSTIC_GRAND; ins <= PA_INST_GUNSHOT; ins++) {

        printf("%d ", ins);
        pa_instchange(dport, 0, 1, ins);
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        wait(SECOND/10);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/10);

    }
    printf("\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    printf("Complete\n");
    waitret();

    printf("Run though all percussive instruments\n");
        printf("Note that not all syths implement all instruments\n");
    printf("Instruments: ");
    for (n = PA_NOTE_ACOUSTIC_BASS_DRUM; n <= PA_NOTE_OPEN_TRIANGLE; n++) {

        printf("%d ", n);
        pa_noteon(dport, 0, 10, n, INT_MAX);
        wait(SECOND/10);
        pa_noteoff(dport, 0, 10, n, 0);
        wait(SECOND/10);

    }
    printf("\n");
    printf("Complete\n");
    waitret();

    printf("Random note programming piano:\n");
    waitret();
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    printf("Random note programming harpsichord:\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_HARPSICHORD);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    printf("Random note programming organ:\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    printf("Random note programming soprando sax:\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_SOPRANO_SAX);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    printf("Random note programming telephone:\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_TELEPHONE_RING);
    playrand(dport, 100);
    printf("Complete\n");
    waitret();

    /* restore piano */
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);

    /* set attack times */
    printf("Set step attack times on piano\n");
    waitret();
    for (i = 0; i <= 10; i++) {

        printf("Attack: %d\n", i*(INT_MAX/10));
        pa_attack(dport, 0, 1, i*(INT_MAX/10));
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
    printf("Complete\n");
    waitret();

    printf("Set step attack times on organ\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    for (i = 0; i <= 10; i++) {

        printf("Attack: %d\n", i*(INT_MAX/10));
        pa_attack(dport, 0, 1, i*(INT_MAX/10));
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
    printf("Complete\n");
    waitret();

//#endif

terminate: /* terminate */
    pa_closesynthout(dport);
    printf("\n");
    printf("Test complete\n");
    waitret();

    return (0); /* return no error */

}
