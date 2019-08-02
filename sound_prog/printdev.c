/*******************************************************************************

Print device tables

Prints the available device tables.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <sound.h>

#define BUFLEN 200

int main(int argc, char **argv)

{

    int i;
    char buff[BUFLEN];

    printf("\nInput synthesizer devices:\n\n");
    for (i = 1; i <= pa_synthin(); i++) {

        pa_synthinname(i, buff, BUFLEN);
        printf("%d: %s\n", i, buff);

    }

    printf("\nOutput synthesizer devices:\n\n");
    for (i = 1; i <= pa_synthout(); i++) {

        pa_synthoutname(i, buff, BUFLEN);
        printf("%d: %s\n", i, buff);

    }

    printf("\nInput wave devices:\n\n");
    for (i = 1; i <= pa_wavein(); i++) {

        pa_waveinname(i, buff, BUFLEN);
        printf("%d: %s\n", i, buff);

    }

    printf("\nOutput wave devices:\n\n");
    for (i = 1; i <= pa_waveout(); i++) {

        pa_waveoutname(i, buff, BUFLEN);
        printf("%d: %s\n", i, buff);

    }
    printf("\n");

}
