/*******************************************************************************

Generate wave output

Generates a since or square wave of the desired frequency or 440hz by default.

Format:

genwave [--port=<port>|--p=<port>|--freq=<freq>|--f=<freq>|
         --square|--sq|--sine|-s]

The options are:

--port=<port> or --p=<port>

Set the output port to be used.

--freq=<freq> or --f=<freq>

Set the frequency of output.

--square or -s

Set square wave output (the default is sine wave).

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <sound.h>
#include <option.h>

#define SIZEBUF 2048
//#define SIZEBUF 65536

int dport = PA_SYNTH_OUT; /* set default synth out */
int freq = 440; /* set default frequency */
int square = FALSE; /* set not square wave */

optrec opttbl[] = {

    { "port",   NULL,    &dport, NULL, NULL },
    { "p",      NULL,    &dport, NULL, NULL },
    { "freq",   NULL,    &freq,  NULL, NULL },
    { "f",      NULL,    &freq,  NULL, NULL },
    { "square", &square, NULL,   NULL, NULL },
    { "s",      &square, NULL,   NULL, NULL },
    { NULL,     NULL,    NULL,    NULL, NULL }

};

int main(int argc, char **argv)
{

    int i;
    double x;
    double cost;
    unsigned int rate;
    short buf[SIZEBUF];
    int n;
    int val;
    int argi = 1;

    /* parse user options */
    options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 1) {

        fprintf(stderr,
            "Usage: genwave [--port=<port>|--p=<port>|--freq=<freq>|--f=<freq>|\n");
        fprintf(stderr,
            "                --square|--sq|--sine|-s]\n");

        exit(1);

    }

    rate = 44100; /* set sample rate */

    pa_openwaveout(dport); /* open output wave port */
    pa_chanwaveout(dport, 1);      /* one channel */
    pa_ratewaveout(dport, rate);   /* CD sample rate */
    pa_lenwaveout(dport,  16);     /* 16 bits */
    pa_sgnwaveout(dport,  TRUE);   /* signed */
    pa_endwaveout(dport,  FALSE);  /* little endian */
    pa_fltwaveout(dport,  FALSE);  /* integer */

    cost = 2.0 * M_PI * (double)freq / (double)rate;

    n = 0;
    while (1) {

        for (i = 1 ; i < SIZEBUF ; i++, n++) {

            x = sin(n * cost);
            if (square) val = (x>0)? 32767: -32767;
            else val = (short)(32767 * x);
            buf[i] = val;

        }
        pa_wrwave(dport, (byte*)buf, SIZEBUF);

    }

    return (0);

}
