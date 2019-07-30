/*******************************************************************************

Play midi file

playmidi <.mid file>

Plays the given midi file.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <sound.h>

int main(int argc, char **argv)

{

    if (argc != 2) {

        fprintf(stderr, "Usage: play <.mid file>\n");
        exit(1);

    }

    pa_loadsynth(1, argv[1]);
    pa_opensynthout(PA_SYNTH_OUT);
    pa_playsynth(PA_SYNTH_OUT, 0, 1);
    pa_waitsynth(PA_SYNTH_OUT);
    pa_closesynthout(PA_SYNTH_OUT);
    pa_delsynth(1);

}
