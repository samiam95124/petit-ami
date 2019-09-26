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

    { "port",  NULL, &dport,  NULL, NULL },
    { "p",     NULL, &dport,  NULL, NULL },
    { NULL,    NULL, NULL,    NULL, NULL }

};

int main(int argc, char **argv)

{

    int argi = 1;

    /* parse user options */
    options(&argi, &argc, argv, opttbl, true);

    if (argc < 2) {

        fprintf(stderr, "Usage: playmidi [--port=<port>|-p=<port>] <.mid file>\n");
        exit(1);

    }

    pa_loadsynth(1, argv[argi]);
    pa_opensynthout(dport);
    pa_playsynth(dport, 0, 1);
    pa_waitsynth(dport);
    pa_closesynthout(dport);
    pa_delsynth(1);

}
