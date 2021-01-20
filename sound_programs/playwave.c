/*******************************************************************************

Play midi file

playmidi <.mid file>

Plays the given midi file.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <sound.h>
#include <option.h>

int dport = PA_WAVE_OUT; /* set default wave out */

pa_optrec opttbl[] = {

    { "port",  NULL, &dport,  NULL, NULL },
    { "p",     NULL, &dport,  NULL, NULL },
    { NULL,    NULL, NULL,    NULL, NULL }

};

int main(int argc, char **argv)

{

    int argi = 1;

    /* parse user options */
    pa_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 2) {

        fprintf(stderr, "Usage: playwave [--port=<port>|-p=<port>] <.wav file>\n");
        exit(1);

    }

    pa_loadwave(1, argv[argi]);
    pa_openwaveout(dport);
    pa_playwave(dport, 0, 1);
    pa_waitwave(dport);
    pa_closewaveout(dport);
    pa_delwave(1);

}
