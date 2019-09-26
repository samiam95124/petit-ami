/*******************************************************************************

Print device tables

Prints the available device tables.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include <sound.h>

#define BUFLEN 200

int main(int argc, char **argv)

{

    int i;
    char buff[BUFLEN];
    int max;

    printf("\nInput synthesizer devices:\n\n");
    for (i = 1; i <= pa_synthin(); i++) {

        pa_synthinname(i, buff, BUFLEN);
        printf("%2d: %s\n", i, buff);

    }


    printf("\nOutput synthesizer devices:\n\n");
    for (i = 1; i <= pa_synthout(); i++) {

        pa_synthoutname(i, buff, BUFLEN);
        printf("%2d: %s\n", i, buff);

    }

    /* sweep for max len */
    max = 0;
    for (i = 1; i <= pa_wavein(); i++) {

        pa_waveinname(i, buff, BUFLEN);
        if (strlen(buff) > max) max = strlen(buff);

    }
    printf("\nInput wave devices:\n\n");
    for (i = 1; i <= pa_wavein(); i++) {

        pa_waveinname(i, buff, BUFLEN);
        printf("%2d: %-*.*s channels: %d rate: %d len: %d sign: %d "
               "big endian: %d float: %d\n", i, max, max, buff,
               pa_chanwavein(i), pa_ratewavein(i), pa_lenwavein(i),
               pa_sgnwavein(i), pa_endwavein(i), pa_fltwavein(i));

    }

    printf("\nOutput wave devices:\n\n");
    for (i = 1; i <= pa_waveout(); i++) {

        pa_waveoutname(i, buff, BUFLEN);
        printf("%2d: %s\n", i, buff);

    }
    printf("\n");

}
