/*******************************************************************************

Connect WAVE input port to WAVE output port

Transfers input wave data to output wave data. The format of the command is:

connectwave [<source port> <destination port>]

If the source and destination are not on the command line, they will both
default to 1.

The key parameter in the program is the buffer size. This should be big enough
to avoid buffer overruns, but not so big that considerable sound lag exists in
the system.

This program can be used as a basis for many other wave handling program types.
The sound samples are repeatedly read into a buffer, and those samples could
be copied or modified, thus making this program into a multi-output patcher,
mixer or effects plug-in.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <localdefs.h>
#include <sound.h>

#define BUFLEN 2048 /* 46ms */

/*******************************************************************************

Find length of basic sample

Uses the channel and bit length to find the size of a basic sample. The bit
length is rounded to the nearest byte, then multiplied by the channel number.
The result returned is the sample size in bytes.

*******************************************************************************/

int splsiz(int chan, int bits)

{

    int size;

    size = bits/8; /* find number of bytes basic */
    if (bits%8) size++; /* round up to nearest byte */

    return size*chan; /* return sample size */

}

int main(int argc, char **argv)

{

    int sport;         /* source port */
    int dport;         /* destination port */
    int ssize;         /* sample size in bytes */
    int sbuf;          /* number of samples per buffer */
    byte buff[BUFLEN]; /* buffer for samples */

    if (argc != 1 && argc != 3) {

        fprintf(stderr, "Usage: connectwave [<input port> <output port>]\n");
        exit(1);

    }

    /* set default ports */
    sport = 1;
    dport = 1;

    /* set ports from command line if provided */
    if (argc == 3) {

        sport = atoi(argv[1]);
        dport = atoi(argv[2]);

    }

    /* open target ports */
    pa_openwavein(sport);
    pa_openwaveout(dport);

    /* transfer input parameters to output port */
    pa_chanwaveout(dport, pa_chanwavein(sport));
    pa_ratewaveout(dport, pa_ratewavein(sport));
    pa_lenwaveout(dport, pa_lenwavein(sport));
    pa_sgnwaveout(dport, pa_sgnwavein(sport));
    pa_endwaveout(dport, pa_endwavein(sport));
    pa_fltwaveout(dport, pa_fltwavein(sport));

    /* find size of basic sample */
    ssize = splsiz(pa_chanwavein(sport), pa_lenwavein(sport));

    /* find number of samples per buffer */
    sbuf = BUFLEN/ssize;

    /* transfer data continuously */
    while (1) {

        pa_rdwave(sport, buff, sbuf);
        pa_wrwave(dport, buff, sbuf);

    }

}
