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

/*******************************************************************************

Play note

Just plays a test note, with 1/4 on and off times. Plays middle C.

*******************************************************************************/

void playnote(int port, pa_note n)

{

    pa_noteon(port, 0, 1, n, INT_MAX); /* play middle C */
    wait(SECOND/4);
    pa_noteoff(port, 0, 1, n, 0);
    wait(SECOND/4);

}


/*******************************************************************************

Play scale

Plays a simple scale with on time.

*******************************************************************************/

void playscale(int port, int t)

{

    pa_noteon(port, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    wait(t);
    pa_noteoff(port, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
    wait(SECOND/4);
    pa_noteon(port, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
    wait(t);
    pa_noteoff(port, 0, 1, PA_NOTE_D+PA_OCTAVE_6, 0);
    wait(SECOND/4);
    pa_noteon(port, 0, 1, PA_NOTE_E+PA_OCTAVE_6, INT_MAX);
    wait(t);
    pa_noteoff(port, 0, 1, PA_NOTE_E+PA_OCTAVE_6, 0);
    wait(SECOND/4);
    pa_noteon(port, 0, 1, PA_NOTE_F+PA_OCTAVE_6, INT_MAX);
    wait(t);
    pa_noteoff(port, 0, 1, PA_NOTE_F+PA_OCTAVE_6, 0);
    wait(SECOND/4);
    pa_noteon(port, 0, 1, PA_NOTE_G+PA_OCTAVE_6, INT_MAX);
    wait(t);
    pa_noteoff(port, 0, 1, PA_NOTE_G+PA_OCTAVE_6, 0);
    wait(SECOND/4);
    pa_noteon(port, 0, 1, PA_NOTE_A+PA_OCTAVE_6, INT_MAX);
    wait(t);
    pa_noteoff(port, 0, 1, PA_NOTE_A+PA_OCTAVE_6, 0);
    wait(SECOND/4);
    pa_noteon(port, 0, 1, PA_NOTE_B+PA_OCTAVE_6, INT_MAX);
    wait(t);
    pa_noteoff(port, 0, 1, PA_NOTE_B+PA_OCTAVE_6, 0);
    wait(SECOND/4);

}


int main(int argc, char *argv[])

{

    int           dport; /* output port */
    pa_note       n; /* note */
    int           o; /* octave */
    pa_instrument ins; /* instrument */
    int           i, x, j;

    if (argc != 1 && argc != 2) {

        fprintf(stderr, "Usage: sndtst [<port>]\n");
        exit(1);

    }

    dport = 6/*PA_SYNTH_OUT*/; /* index standard synth output port */
    if (argc == 2) dport = atoi(argv[1]);

    if (setjmp(terminate_buf)) goto terminate;

    /***************************************************************************

    MIDI TESTS

    ***************************************************************************/

    pa_opensynthout(dport);

    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);

    printf("Sound library test\n\n");
    printf("Runs through various sound tests and gives you a chance to\n");
    printf("evaluate if the sound produced matches the description.\n\n");
    printf("\n");
    printf("Note that this test can also serve as a test of the output synthesizer.\n");
    printf("Not all synths implement all modes or instruments. In fact, it is common\n");
    printf("to leave many features unimplemented.\n");
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

    printf("Chop test, play note series and repeat with the envelope time\n");
    printf("limited by noteoff\n");
    printf("First piano, then organ\n");
    printf("Note that some synths appear to set a minimum on note length\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    for (i = 10; i >= 1; i--) playscale(dport, i*(SECOND/30));
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    for (i = 10; i >= 1; i--) playscale(dport, i*(SECOND/30));
    printf("Complete\n");
    waitret();

    printf("Note volume test\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    for (i = 0; i < 20; i++) {

        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, i*(INT_MAX/20));
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
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
        playnote(dport, PA_NOTE_C+PA_OCTAVE_6);

    }
    pa_attack(dport, 0, 1, i*(INT_MAX/2)); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Set step attack times on organ\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    for (i = 0; i <= 10; i++) {

        printf("Attack: %d\n", i*(INT_MAX/10));
        pa_attack(dport, 0, 1, i*(INT_MAX/10));
        playnote(dport, PA_NOTE_C+PA_OCTAVE_6);

    }
    pa_attack(dport, 0, 1, i*(INT_MAX/2)); /* reset normal */
    printf("Complete\n");
    waitret();

    /* set release times */
    printf("Set step release times on piano\n");
    waitret();
    for (i = 0; i <= 10; i++) {

        printf("Release: %d\n", i*(INT_MAX/10));
        pa_release(dport, 0, 1, i*(INT_MAX/10));
        playnote(dport, PA_NOTE_C+PA_OCTAVE_6);

    }
    pa_release(dport, 0, 1, i*(INT_MAX/2)); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Set step release times on organ\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    for (i = 0; i <= 10; i++) {

        printf("Release: %d\n", i*(INT_MAX/10));
        pa_release(dport, 0, 1, i*(INT_MAX/10));
        playnote(dport, PA_NOTE_C+PA_OCTAVE_6);

    }
    pa_release(dport, 0, 1, i*(INT_MAX/2)); /* reset normal */
    printf("Complete\n");
    waitret();

    /* set legato */
    printf("Set legato on piano, first normal, then legato\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    pa_legato(dport, 0, 1, false);
    pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
    wait(SECOND/4);
    pa_noteon(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX); /* play D */
    wait(SECOND/4);
    /* turn off both */
    pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    pa_noteoff(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
    /* now repeat with legato on */
    pa_legato(dport, 0, 1, true);
    pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
    wait(SECOND/4);
    pa_noteon(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX); /* play D */
    wait(SECOND/4);
    /* turn off both */
    pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    pa_noteoff(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
    pa_legato(dport, 0, 1, false); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Set legato on organ, first normal, then legato\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    pa_legato(dport, 0, 1, false);
    pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
    wait(SECOND/4);
    pa_noteon(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX); /* play D */
    wait(SECOND/4);
    /* turn off both */
    pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    pa_noteoff(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
    /* now repeat with legato on */
    pa_legato(dport, 0, 1, true);
    pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
    wait(SECOND/4);
    pa_noteon(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX); /* play D */
    wait(SECOND/4);
    /* turn off both */
    pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    pa_noteoff(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
    pa_legato(dport, 0, 1, false); /* reset normal */
    printf("Complete\n");
    waitret();

    /* set portamento */
    printf("Set portamento on piano, first normal, then portamento, through\n");
    printf("various portamento times\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    for (i = 0; i < 10; i++) {

        printf("Portamento time: %d\n", i*(INT_MAX/10));
        pa_porttime(dport, 0, 1, i*(INT_MAX/10));
        pa_portamento(dport, 0, 1, false);
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
        wait(SECOND/4);
        pa_noteon(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX); /* play D */
        wait(SECOND/4);
        /* turn off both */
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
        /* now repeat with portamento on */
        pa_portamento(dport, 0, 1, true);
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
        wait(SECOND/4);
        pa_noteon(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX); /* play D */
        wait(SECOND/4);
        /* turn off both */
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);

    }
    pa_portamento(dport, 0, 1, false); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Set portamento on organ, first normal, then portamento\n");
    waitret();
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    for (i = 0; i < 10; i++) {

        printf("Portamento time: %d\n", i*(INT_MAX/10));
        pa_portamento(dport, 0, 1, false);
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
        wait(SECOND/4);
        pa_noteon(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX); /* play D */
        wait(SECOND/4);
        /* turn off both */
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);
        /* now repeat with portamento on */
        pa_portamento(dport, 0, 1, true);
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX); /* play middle C */
        wait(SECOND/4);
        pa_noteon(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX); /* play D */
        wait(SECOND/4);
        /* turn off both */
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        pa_noteoff(dport, 0, 1, PA_NOTE_D+PA_OCTAVE_6, INT_MAX);

    }
    pa_portamento(dport, 0, 1, false); /* reset normal */
    printf("Complete\n");
    waitret();

    printf("Channel volume test. Play note continuously while advancing volume\n");
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    /* advance volume sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Volume: %d\n", i*(INT_MAX/20));
        pa_volsynthchan(dport, 0, 1, i*(INT_MAX/20));
        wait(SECOND/4);

    }
    pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    /* reset channel vol to midline */
    pa_volsynthchan(dport, 0, 1, INT_MAX/2);
    printf("Complete\n");
    waitret();

    printf("Channel balance test. Play note continuously while changing\n");
    printf("from to right\n");
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    /* advance volume sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Balance: %d\n", -INT_MAX+(i*(INT_MAX/20*2)));
        pa_balance(dport, 0, 1, -INT_MAX+(i*(INT_MAX/20*2)));
        wait(SECOND/4);

    }
    pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    /* reset channel balance to midline */
    pa_balance(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel vibrato test. Play note continuously while advancing vibrato\n");
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    /* advance vibrato sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Vibrato: %d\n", i*(INT_MAX/20));
        pa_vibrato(dport, 0, 1, i*(INT_MAX/20));
        wait(SECOND);

    }
    pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    /* reset channel vibrato to midline */
    pa_vibrato(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel pan test. Play note continuously while changing\n");
    printf("pan from to right\n");
    pa_instchange(dport, 0, 1, PA_INST_DRAWBAR_ORGAN);
    pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    /* advance pan sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Pan: %d\n", -INT_MAX+(i*(INT_MAX/20*2)));
        pa_pan(dport, 0, 1, -INT_MAX+(i*(INT_MAX/20*2)));
        wait(SECOND/4);

    }
    pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    /* reset channel pan to midline */
    pa_pan(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel timbre test. Play notes while advancing timbre\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    /* advance timbre sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Timbre: %d\n", i*(INT_MAX/20));
        pa_timbre(dport, 0, 1, i*(INT_MAX/20));
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
    /* reset channel timbre */
    pa_timbre(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel brightness test. Play notes while advancing brightness\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    /* advance brightness sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Brightness: %d\n", i*(INT_MAX/20));
        pa_brightness(dport, 0, 1, i*(INT_MAX/20));
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
    /* reset channel brightness */
    pa_brightness(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel reverb test. Play notes while advancing reverb\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    /* advance reverb sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Reverb: %d\n", i*(INT_MAX/20));
        pa_reverb(dport, 0, 1, i*(INT_MAX/20));
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
    /* reset channel reverb */
    pa_reverb(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel tremulo test. Play notes while advancing tremulo\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);

    /* advance tremulo sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Tremulo: %d\n", i*(INT_MAX/20));
        pa_tremulo(dport, 0, 1, i*(INT_MAX/20));
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
    /* reset channel tremulo */
    pa_tremulo(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel chorus test. Play notes while advancing chorus\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);

    /* advance chorus sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Chorus: %d\n", i*(INT_MAX/20));
        pa_chorus(dport, 0, 1, i*(INT_MAX/20));
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
    /* reset channel chorus */
    pa_chorus(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel celeste test. Play notes while advancing celeste\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    /* advance celeste sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Celeste: %d\n", i*(INT_MAX/20));
        pa_celeste(dport, 0, 1, i*(INT_MAX/20));
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
    /* reset channel celeste */
    pa_celeste(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    printf("Channel phaser test. Play notes while advancing phaser\n");
    pa_instchange(dport, 0, 1, PA_INST_ACOUSTIC_GRAND);
    /* advance phaser sets on channel while playing */
    for (i = 0; i < 20; i++) {

        printf("Phaser: %d\n", i*(INT_MAX/20));
        pa_phaser(dport, 0, 1, i*(INT_MAX/20));
        pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
        wait(SECOND/4);
        pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
        wait(SECOND/4);

    }
    /* reset channel phaser */
    pa_phaser(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

    /* don't know about this test, it seems to limit the total pitch wheel range,
       which is not right */
    printf("pitch wheel. Vary pitch wheel while playing continuously\n");
    pa_instchange(dport, 0, 1, PA_INST_LEAD_1_SQUARE);
    pa_noteon(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, INT_MAX);
    for (j = 0; j < 10; j++) {

        printf("Pitchrange: %d\n", j*(INT_MAX/10));
        pa_pitchrange(dport, 0, 1, j*(INT_MAX/10));
        for (x = 0; x < 10; x++)
            for (i = 0; i < 10; i++) {

            printf("Pitch: %d\n", -INT_MAX+(i*(INT_MAX/10*2)));
            pa_pitch(dport, 0, 1, -INT_MAX+(i*(INT_MAX/10*2)));
            wait(SECOND/100);

        }

    }
    pa_noteoff(dport, 0, 1, PA_NOTE_C+PA_OCTAVE_6, 0);
    pa_pitch(dport, 0, 1, 0);
    printf("Complete\n");
    waitret();

terminate: /* terminate */
    pa_closesynthout(dport);
    printf("\n");

    return (0); /* return no error */

}
