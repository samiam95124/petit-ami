/*******************************************************************************

Print device tables

Prints the available device tables.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sound.h>
#include <localdefs.h>
#include <option.h>

#define BUFLEN 200

ami_long is = FALSE; /* print input synth ports */
ami_long os = FALSE;
ami_long iw = FALSE;
ami_long ow = FALSE;

ami_optrec opttbl[] = {

    { "is", &is,  NULL,  NULL, NULL },
    { "os", &os,  NULL,  NULL, NULL },
    { "iw", &iw,  NULL,  NULL, NULL },
    { "ow", &ow,  NULL,  NULL, NULL },
    { NULL, NULL, NULL,  NULL, NULL }

};

int main(int argc, char **argv)

{

    int i;
    char buff[BUFLEN];
    int max;
    ami_long argi = 1;
    ami_long argcl = argc;

    /* parse user options */
    ami_options(&argi, &argcl, argv, opttbl, TRUE);

    if (argcl != 1) {

        fprintf(stderr, "Usage: printdev [--is|--os|--iw|--ow\n");
        exit(1);

    }

    /* the default is print all */
    if (!is && !os && !iw && !ow) { is = os = iw = ow = TRUE; }

    printf("\n");

    if (is) {

        printf("Input synthesizer devices:\n\n");
        for (i = 1; i <= ami_synthin(); i++) {

            ami_synthinname(i, buff, BUFLEN);
            printf("%2d: %s\n", i, buff);

        }
        printf("\n");

    }

    if (os) {

        printf("Output synthesizer devices:\n\n");
        for (i = 1; i <= ami_synthout(); i++) {

            ami_synthoutname(i, buff, BUFLEN);
            printf("%2d: %s\n", i, buff);

        }
        printf("\n");

    }

    if (iw) {

        /* sweep for max len */
        max = 0;
        for (i = 1; i <= ami_wavein(); i++) {

            ami_waveinname(i, buff, BUFLEN);
            if (strlen(buff) > max) max = strlen(buff);

        }
        printf("Input wave devices:\n\n");
        for (i = 1; i <= ami_wavein(); i++) {

            ami_waveinname(i, buff, BUFLEN);
            printf("%2d: %-*.*s channels: %lld rate: %5lld len: %2lld sign: %lld "
                   "big endian: %lld float: %lld\n", i, max, max, buff,
                   AMI_LONG_CAST(ami_chanwavein(i)), AMI_LONG_CAST(ami_ratewavein(i)), AMI_LONG_CAST(ami_lenwavein(i)),
                   AMI_LONG_CAST(ami_sgnwavein(i)), AMI_LONG_CAST(ami_endwavein(i)), AMI_LONG_CAST(ami_fltwavein(i)));

        }
        printf("\n");

    }

    if (ow) {

        printf("Output wave devices:\n\n");
        for (i = 1; i <= ami_waveout(); i++) {

            ami_waveoutname(i, buff, BUFLEN);
            printf("%2d: %s\n", i, buff);

        }
        printf("\n");

    }

}
