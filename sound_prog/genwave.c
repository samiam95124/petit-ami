/*******************************************************************************

Generate wave output

Generates a since or square wave of the desired frequency or 440hz by default.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <sound.h>

#define SIZEBUF 2048
//#define SIZEBUF 65536

int main(int argc, char **argv)
{

    int i;
    double x;
    double cost;
    int freq;
    unsigned int rate;
    short buf[SIZEBUF];
    int n;
    int square;
    int val;
    int dport;

    if (argc != 1 && argc != 2 && argc != 3) {

        fprintf(stderr, "Usage: genwave [<frequency>[<sine=>0|<square=>1]\n");
        exit(1);

    }

    square = 0; /* set sine wave */
    freq = 440; /* set default frequency */
    dport = 1; /* set default output port */
    rate = 44100; /* set sample rate */

    /* set frequency from command line if provided */
    if (argc >= 2) freq = atoi(argv[1]);

    /* set sine/square from command line if provided */
    if (argc == 3) square = atoi(argv[2]);

    pa_openwaveout(dport); /* open output wave port */
    pa_chanwaveout(dport, 1);      /* one channel */
    pa_ratewaveout(dport, rate);   /* CD sample rate */
    pa_lenwaveout(dport,  16);     /* 16 bits */
    pa_sgnwaveout(dport,  true);   /* signed */
    pa_endwaveout(dport,  false);  /* little endian */
    pa_fltwaveout(dport,  false);  /* integer */

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
