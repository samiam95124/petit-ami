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

        fprintf(stderr, "Usage: playwave <.wav file>\n");
        exit(1);

    }

    pa_loadwave(1, argv[1]);
    pa_opensynthout(PA_WAVE_OUT);
    pa_playwave(PA_WAVE_OUT, 0, 1);
    pa_waitwave(PA_WAVE_OUT);
    pa_closewaveout(PA_WAVE_OUT);
    pa_delwave(1);

}
