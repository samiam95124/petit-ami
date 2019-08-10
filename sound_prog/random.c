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

#include <sound.h>

int main(int argc, char **argv)

{

    int i;
    int key;
    int dport;

    pa_instrument inst;

    if (argc != 1 && argc != 2 && argc != 3) {

        fprintf(stderr, "Usage: random [<instrument>[<port>]]\n");
        exit(1);

    }

    inst = PA_INST_ACOUSTIC_GRAND;
    dport = PA_SYNTH_OUT;

    if (argc >= 2) inst = atoi(argv[1]);
    else if (argc >= 3) dport = atoi(argv[2]);

printf("dport: %d\n", dport);
dport = 6;
    pa_opensynthout(dport);
    pa_instchange(dport, 0, 1, inst);
    srand(42);
    for( i = 0; i < 1000; i++) {

        /* Generate a random key */
        key = 60 + (int)(12.0f * rand() / (float) RAND_MAX)-1;
        /* Play a note */
        pa_noteon(dport, 0, 1, key, INT_MAX);
        /* Sleep for 1 second */
        usleep(100000);
        /* Stop the note */
        pa_noteoff(dport, 0, 1, key, 0);

    }
    pa_closesynthout(dport);

}
