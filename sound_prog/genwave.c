/*******************************************************************************

Generate square wave output

Generates a square wave of the desired frequency or 440hz by default.

*******************************************************************************/

/* general system definitins */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

/* Petit_ami definitions */
#include <localdefs.h> /* PA local defines */
#include <terminal.h>  /* terminal level API */
#include <sound.h>     /* sound API */

/* length of sample buffer */
#define BUFLEN 1024

/* timer value for 1 second */
#define SECOND 10000

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

    int       sport;        /* source port */
    int       dport;        /* destination port */
    int       ssize;        /* sample size in bytes */
    int       sbuf;         /* number of samples per buffer */
    byte      buff[BUFLEN]; /* buffer for samples */
    int       rate;         /* rate of samples */
    int       freq;         /* frequency of output */
    float     stime;        /* sample time in seconds */
    pa_evtrec er;           /* event record */
    float     ftime;        /* 1/2 cycle time */
    int       val;          /* output value */
    int       i;

    if (argc != 1 && argc != 2) {

        fprintf(stderr, "Usage: genwave [<frequency>]\n");
        exit(1);

    }

    /* set default ports */
    freq = 440; /* set default frequency */
    dport = 1; /* set default output port */
    rate = 44100; /* set sample rate */

    /* set ports from command line if provided */
    if (argc == 2) {

        freq = atoi(argv[1]);

    }

    /* open target ports */
    pa_openwaveout(dport);

    /* set output parameters */
    pa_chanwaveout(dport, 1);      /* one channel */
    pa_ratewaveout(dport, rate);   /* CD sample rate */
    pa_lenwaveout(dport,  16);     /* 16 bits */
    pa_sgnwaveout(dport,  true);   /* signed */
    pa_endwaveout(dport,  false);  /* little endian */
    pa_fltwaveout(dport,  false);  /* integer */

    /* find size of basic sample */
    ssize = splsiz(1, 16);

    /* find number of samples per buffer */
    sbuf = BUFLEN/ssize;

    /* output data continuously */
    val = SHRT_MAX/2; /* set output volume */
    stime = 1.0/rate; /* find time per sample */
    ftime = stime*(freq/2); /* find half cycle time */
    /* set timer at buffer output time and repeat */
    pa_timer(stdin, 1, sbuf*stime*10000, true);
    while (1) {

        for (i = 0; i < sbuf; i += ssize) {

            /* fill samples */
            buff[i] = val&0xff; /* set sample in buffer */
            buff[i+1] = (val>>8)&0xff;
            ftime -= stime; /* count off time to 1/2 cycle */
            if (ftime <= 0) {

                /* 1/2 cycle done, flip value and reset time remaining */
                val = -val;
                ftime = stime*(freq/2); /* find half cycle time */

            }

        }
        /* wait for buffer framing time */
        do { pa_event(stdin, &er); } while (er.etype != pa_ettim && er.etype != pa_etterm);
        if (er.etype == pa_etterm) exit(0);
        pa_wrwave(dport, buff, sbuf);

    }

}
