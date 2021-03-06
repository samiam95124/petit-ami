/*******************************************************************************

Play random notes

random <instrument>

Plays notes at random from the given instrument, or default 1 (piano).

I have a bit of nostalgia about the random note generators. Synth chips were a
very early addition to computers (before the IBM-PC in fact). When I went to
computer fairs, playing random notes was a common demo left playing. This was
late 1970's-early 1980's.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <terminal.h>
#include <sound.h>
#include <option.h>

#define SECOND 10000

int dport = PA_SYNTH_OUT; /* set default synth out */
pa_instrument inst = PA_INST_ACOUSTIC_GRAND; /* set default instrument */

pa_optrec opttbl[] = {

    { "port", NULL, &dport,  NULL, NULL },
    { "p",    NULL, &dport,  NULL, NULL },
    { "inst", NULL, &inst,   NULL, NULL },
    { "i",    NULL, &inst,   NULL, NULL },
    { NULL,   NULL, NULL,    NULL, NULL }

};

void waittime(int t)

{

    pa_evtrec er; /* event record */

    pa_timer(stdin, 1, t, FALSE);
    do { pa_event(stdin, &er); } while (er.etype != pa_ettim && er.etype != pa_etterm);
    if (er.etype == pa_etterm) exit(0);

}

int main(int argc, char **argv)

{

    int i;
    int key;
    int argi = 1;


    /* parse user options */
    pa_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 1) {

        fprintf(stderr, "Usage: random [--port=<port>|--p=<port>|--inst=<instrument>|\n");
        fprintf(stderr, "               --i=<instrument>]\n");

        exit(1);

    }

    pa_opensynthout(dport);
    pa_instchange(dport, 0, 1, inst);
    srand(42);
    for( i = 0; i < 1000; i++) {

        /* Generate a random key */
        key = 60 + (int)(12.0f * rand() / (float) RAND_MAX)-1;
        /* Play a note */
        pa_noteon(dport, 0, 1, key, INT_MAX);
        /* Sleep for .1 second */
        waittime(SECOND/10);
        /* Stop the note */
        pa_noteoff(dport, 0, 1, key, 0);

    }
    pa_closesynthout(dport);

}
