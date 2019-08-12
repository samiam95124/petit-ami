/*******************************************************************************

Play midi file

playmidi [--port=<port>|-p=<port>] <.mid file>

Plays the given midi file.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <sound.h>
#include <option.h>

int dport = PA_SYNTH_OUT; /* set default synth out */

optrec opttbl[] = {

    { "port",  NULL, &dport,  NULL, NULL   },
    { "p",     NULL, &dport,  NULL, NULL   },
    { NULL,    NULL, NULL,    NULL, NULL }

};

int main(int argc, char **argv)

{

    int argi = 1;

    if (argc < 2) {

        fprintf(stderr, "Usage: play [--port=<port>|-p=<port>] <.mid file>\n");
        exit(1);

    }

    /* parse user options */
printf("before options\n");
    options(&argi, &argc, argv, opttbl, true);
printf("after options\n");

    pa_loadsynth(1, argv[1]);
    pa_opensynthout(dport);
    pa_playsynth(dport, 0, 1);
    pa_waitsynth(dport);
    pa_closesynthout(dport);
    pa_delsynth(1);

}
