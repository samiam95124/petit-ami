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

    pa_instrument inst;

    if (argc != 1 && argc != 2) {

        fprintf(stderr, "Usage: random [<instrument>]\n");
        exit(1);

    }

    if (argc == 1) inst = 1; else inst = atoi(argv[1]);

    pa_opensynthout(PA_SYNTH_OUT);
    pa_instchange(PA_SYNTH_OUT, 0, 1, inst);
    srand(42);
    for( i = 0; i < 1200; i++) {

        /* Generate a random key */
        key = 60 + (int)(12.0f * rand() / (float) RAND_MAX)-1;
        /* Play a note */
        pa_noteon(PA_SYNTH_OUT, 0, 1, key, INT_MAX);
        /* Sleep for 1 second */
        usleep(100000);
        /* Stop the note */
        pa_noteoff(PA_SYNTH_OUT, 0, 1, key, 0);

    }
    pa_closesynthout(PA_SYNTH_OUT);

}
