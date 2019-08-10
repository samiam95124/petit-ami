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

    int dport;

    if (argc != 2) {

        fprintf(stderr, "Usage: play <.mid file>\n");
        exit(1);

    }

    dport = 1;

    pa_loadsynth(1, argv[1]);
    pa_opensynthout(dport);
    pa_playsynth(dport, 0, 1);
    pa_waitsynth(dport);
    pa_closesynthout(dport);
    pa_delsynth(1);

}
