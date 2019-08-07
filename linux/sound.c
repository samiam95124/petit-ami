/*******************************************************************************
*                                                                              *
*                               SOUND LIBRARY                                  *
*                                                                              *
*                              11/02 S. A. Moore                               *
*                                                                              *
* Sndlib is a combination of wave file and midi output and control functions.  *
* Implements a set of midi controls and wave controls. Also includes a "flow   *
* through sequencer" function. Each event has a timestamp, and if zero, it     *
* is performed immediately, otherwise scheduled. This allows any mix of        *
* immediate vs. sequenced events.                                              *
*                                                                              *
* Notes:                                                                       *
*                                                                              *
* 1. The parameter convertion work is being performed when a sequenced item    *
* is performed. This could be moved back to the entry of the event to save     *
* time in the timer handler.                                                   *
*                                                                              *
* 2. The validation of parameters happens both at entry time and at sequence   *
* time, need not happen on both.                                               *
*                                                                              *
*******************************************************************************/

#include <pthread.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <stdlib.h>
#include <stdint.h>

#include <localdefs.h>
#include <sound.h>

#define SILENTALSA 1 /* silence ALSA during init */
//#define SHOWDEVTBL 1 /* show device tables after enumeration */
//#define SHOWMIDIIN 1 /* show midi in dumps */

#define MAXMIDP 100 /* maximum midi input/output devices */
#define MAXWAVP 100 /* maximum wave input/output devices */
#define MAXMIDT 100 /* maximum number of midi tracks that can be stored */
#define MAXWAVT 100 /* maximum number of wave tracks that can be stored */

#define WAVBUF (16*1024) /* size of output wave buffer */
#define MAXFIL 200 /* maximum size of wave table filename */

#define DEFMIDITIM 5000 /* default midi quarter note (.5 seconds) */

/*
 * Maximum rate number input
 *
 * The maximum rate, 44100, is derived from nyquist sampling theorem
 * that twice the frequency of the input will result in an accurate sample
 * of that input, provided that the input is band limited at the input
 * frequency. For human hearing, this is generally accepted at 20000hz.
 * The 4100hz extra over the ideal 40000 sampling rate is to accomodate
 * inaccuracies in the band pass filter. This was how CD sample rates
 * were set, and this became the standard.
 *
 * ALSA gives us effectively "infinite" sampling rates in inputs in cases
 * to be accomodating, but we limit these to the preferred rate for
 * practical reasons.
 */
#define MAXINRATE 44100 /* preferred sample rate (CD player) */

/*
 * Maximum channel number input
 *
 * ALSA sudo-devices can give any number of channels by duplicating
 * channel data. Similar to rates, these must be limited to reasonable
 * values. We use 8 channels as the standard limiting channel number,
 * since that accomodates 7.1 channel sound, currently the multichannel
 * sound premimum standard.
 *
 */
 #define MAXINCHAN 8

 /*
  * Maximum bit size input
  *
  * Although some ALSA pseudo input devices can deliver 64 bits (64 bit float),
  * most output devices cannot accomodate this format (even pseudo devices).
  * So we limit to 32 bits.
  */
#define MAXINBITS 32

/*
 * Print unknown midi event codes
 */
#define MIDIUNKNOWN 1

/* midi status messages, high nybble */

#define MESS_NOTE_OFF 0x80
#define MESS_NOTE_ON  0x90
#define MESS_AFTTCH   0xa0
#define MESS_CTRL_CHG 0xb0
#define MESS_PGM_CHG  0xc0
#define MESS_CHN_PRES 0xd0
#define MESS_PTCH_WHL 0xe0

/* midi controller numbers */

#define CTLR_BANK_SELECT_COARSE              0
#define CTLR_MODULATION_WHEEL_COARSE         1
#define CTLR_BREATH_CONTROLLER_COARSE        2
#define CTLR_FOOT_PEDAL_COARSE               4
#define CTLR_PORTAMENTO_TIME_COARSE          5
#define CTLR_DATA_ENTRY_COARSE               6
#define CTLR_VOLUME_COARSE                   7
#define CTLR_BALANCE_COARSE                  8
#define CTLR_PAN_POSITION_COARSE             10
#define CTLR_EXPRESSION_COARSE               11
#define CTLR_EFFECT_CONTROL_1_COARSE         12
#define CTLR_EFFECT_CONTROL_2_COARSE         13
#define CTLR_GENERAL_PURPOSE_SLIDER_1        16
#define CTLR_GENERAL_PURPOSE_SLIDER_2        17
#define CTLR_GENERAL_PURPOSE_SLIDER_3        18
#define CTLR_GENERAL_PURPOSE_SLIDER_4        19
#define CTLR_BANK_SELECT_FINE                32
#define CTLR_MODULATION_WHEEL_FINE           33
#define CTLR_BREATH_CONTROLLER_FINE          34
#define CTLR_FOOT_PEDAL_FINE                 36
#define CTLR_PORTAMENTO_TIME_FINE            37
#define CTLR_DATA_ENTRY_FINE                 38
#define CTLR_VOLUME_FINE                     39
#define CTLR_BALANCE_FINE                    40
#define CTLR_PAN_POSITION_FINE               42
#define CTLR_EXPRESSION_FINE                 43
#define CTLR_EFFECT_CONTROL_1_FINE           44
#define CTLR_EFFECT_CONTROL_2_FINE           45
#define CTLR_HOLD_PEDAL                      64
#define CTLR_PORTAMENTO                      65
#define CTLR_SUSTENUTO_PEDAL                 66
#define CTLR_SOFT_PEDAL                      67
#define CTLR_LEGATO_PEDAL                    68
#define CTLR_HOLD_2_PEDAL                    69
#define CTLR_SOUND_VARIATION                 70
#define CTLR_SOUND_TIMBRE                    71
#define CTLR_SOUND_RELEASE_TIME              72
#define CTLR_SOUND_ATTACK_TIME               73
#define CTLR_SOUND_BRIGHTNESS                74
#define CTLR_SOUND_CONTROL_6                 75
#define CTLR_SOUND_CONTROL_7                 76
#define CTLR_SOUND_CONTROL_8                 77
#define CTLR_SOUND_CONTROL_9                 78
#define CTLR_SOUND_CONTROL_10                79
#define CTLR_GENERAL_PURPOSE_BUTTON_1        80
#define CTLR_GENERAL_PURPOSE_BUTTON_2        81
#define CTLR_GENERAL_PURPOSE_BUTTON_3        82
#define CTLR_GENERAL_PURPOSE_BUTTON_4        83
#define CTLR_EFFECTS_LEVEL                   91
#define CTLR_TREMULO_LEVEL                   92
#define CTLR_CHORUS_LEVEL                    93
#define CTLR_CELESTE_LEVEL                   94
#define CTLR_PHASER_LEVEL                    95
#define CTLR_DATA_BUTTON_INCREMENT           96
#define CTLR_DATA_BUTTON_DECREMENT           97
#define CTLR_NON_REGISTERED_PARAMETER_FINE   98
#define CTLR_NON_REGISTERED_PARAMETER_COARSE 99
#define CTLR_REGISTERED_PARAMETER_FINE       100
#define CTLR_REGISTERED_PARAMETER_COARSE     101
#define CTLR_ALL_SOUND_OFF                   120
#define CTLR_ALL_CONTROLLERS_OFF             121
#define CTLR_LOCAL_KEYBOARD                  122
#define CTLR_ALL_NOTES_OFF                   123
#define CTLR_OMNI_MODE_OFF                   124
#define CTLR_OMNI_MODE_ON                    125
#define CTLR_MONO_OPERATION                  126
#define CTLR_POLY_OPERATION                  127

/*
 * .wav file format elements
 */

/* turn on critical packing. This allows format headers to be read intact
   from the file */
#pragma pack(1)

/* wave header */
typedef struct wavhdr {

    unsigned char id[4];   /* type of header */
    unsigned int  len;     /* length */
    unsigned char type[4]; /* type of file */

} wavhdr;

/* chunk header */
typedef struct cnkhdr {

    unsigned char id[4]; /* type of chunk */
    unsigned int  len;   /* length of chunk */

} cnkhdr;

/* wave fmt chunk */
typedef struct fmthdr {

    unsigned char   id[4];         /* type of chunk */
    unsigned int    len;           /* size of chunk */
    short           tag;           /* type (1=PCM) */
    unsigned short  channels;      /* number of channels */
    unsigned int    samplerate;    /* sample rate per second */
    unsigned int    byterate;      /* byte rate per second */
    unsigned short  blockalign;    /* number of bytes in a sample */
    unsigned short  bitspersample; /* number of bits in a sample */

} fmthdr;

/* turn off critical packing */
#pragma pack()

/* port/id structure to start player tasks */
typedef struct portid {

    int port;
    int id;

} portid;

typedef portid* portidptr;

/* alsa device descriptor */

typedef struct snddev {

    string         name;       /* alsa name of device (sufficient to open) */
    snd_rawmidi_t* midi;       /* MIDI device handle */
    snd_pcm_t*     pcm;        /* PCM device handle */
    int            chan;       /* number of channels */
    int            bits;       /* preferred format bit size */
    int            rate;       /* sample rate */
    boolean        sgn;        /* preferred format sign */
    boolean        big;        /* preferred format big endian */
    boolean        flt;        /* preferred format floating point */
    int            ssiz;       /* sample size, bits*chan in bytes */
    int            fmt;        /* alsa format code for output, -1 if not set */
    byte           last;       /* last byte on midi input */
    int            pback;      /* pushback for input */
    /* These entries support plug in devices, but are also set for intenal devices */
    void (*opnseq)(int p);           /* open sequencer port */
    void (*clsseq)(int p);           /* close sequencer port */
    void (*wrseq)(int p, seqptr sp); /* write function pointer */
    boolean        devopn;     /* device open flag */

} snddev;

typedef snddev* devptr; /* pointer to waveform device */

static seqptr seqlst;                  /* active sequencer entries */
static seqptr seqfre;                  /* free sequencer entries */
static boolean seqrun;                 /* sequencer running */
static struct timeval strtim;          /* start time for sequencer, in raw linux
                                          time */
static boolean seqtimact;              /* sequencer timer active */
static int seqhan;                     /* handle for sequencer timer */
static pthread_mutex_t seqlock;        /* sequencer task lock */
static pthread_t sequencer_thread_id;  /* sequencer thread id */

static boolean sinrun;                 /* input timer running */
static struct timeval sintim;          /* start time input midi marking, in raw
                                          linux time */

/*
 * Set of input file ids for select
 */
static fd_set ifdseta; /* active sets */
static fd_set ifdsets; /* signaled set */
static int ifdmax;

/* Wave track storage. Note this needs locking */
static pthread_mutex_t wavlck; /* sequencer task lock */
static volatile string wavfil[MAXWAVT]; /* storage for wave track files */

/* Synth track storage. Note this needs locking */
static pthread_mutex_t synlck; /* sequencer task lock */
static volatile seqptr syntab[MAXMIDT]; /* storage for synth track files */

/* The active wave track counter uses both a lock and a condition to signal
   that it has returned to zero */
static pthread_mutex_t wnmlck; /* number of outstanding wave plays */
static pthread_cond_t wnmzer; /* counter reached zero */
static volatile int numwav; /* outstanding active wave instances */

/* The active synth counter uses both a lock and a condition to signal
   that it has returned to zero */
static pthread_mutex_t snmlck; /* number of outstanding synth plays */
static pthread_cond_t snmzer; /* counter reached zero */
static volatile int numseq; /* outstanding active synth instances */
/* individual counts on sequencers for each logical syth store. These are
   by the same lock as the group */
static volatile int numsql[MAXMIDT];

/*
 * Alsa devices
 *
 * Each type, PCM in, PCM out, Midi in, midi out, has its own name table.
 * Midi is not a wave table, but uses the same naming system.
 */
static devptr alsamidiout[MAXMIDP]; /* MIDI out */
static devptr alsamidiin[MAXMIDP]; /* MIDI in */
static devptr alsapcmout[MAXWAVP]; /* PCM out */
static devptr alsapcmin[MAXWAVP]; /* PCM in */

/* number of each device */
static int alsamidioutnum; /* MIDI out */
static int alsamidiinnum; /* MIDI in */
static int alsapcmoutnum; /* PCM out */
static int alsapcminnum; /* PCM in */

/* count of plug-ins */
static int alsamidioutplug; /* MIDI out */

/* forwards */
static void alsaplaysynth_kickoff(int p, int s);
static void alsaplaywave_kickoff(int p, int w);

/*******************************************************************************

Process sound library error

Outputs an error message using the special syslib function, then halts.

*******************************************************************************/

static void error(string s)

{

    fprintf(stderr, "\nError: Sound: %s\n", s);

    exit(1);

}

/*******************************************************************************

Process alsa error

Outputs an ALSA specific error, then halts.

*******************************************************************************/

static void alsaerror(int e)

{

    fprintf(stderr, "\nALSA error: %d: %s\n", e, snd_strerror(e));

    exit(1);

}

/*******************************************************************************

Print parameters of device

Prints device parameters. A diagnostic.

*******************************************************************************/

void prtparm(devptr p, boolean wave)

{

    if (wave)
        printf("%-20s chan: %d bits: %d rate: %d sgn: %d big: %d flt: %d\n",
               p->name, p->chan, p->bits, p->rate, p->sgn, p->big, p->flt);
    else
        printf("%-20s\n", p->name);

}

/*******************************************************************************

Print device table

Prints a list of devices. A diagnostic.

*******************************************************************************/

void prtdtbl(devptr table[], int len, boolean wave)

{

    int i;

    for (i = 0; i < len; i++) if (table[i]) {

        printf("%2d: ", i+1);
        prtparm(table[i], wave);

    }

}

/*******************************************************************************

Convert ALSA format codes to parameters

Finds the net format parameters bits/sign/endian/float from a given alsa format
code. The return value is true if the format is supported by PA, otherwise
false.

*******************************************************************************/

int alsa2params(int fmt, int* rbits, int* rsgn, int* rbig, int* rflt)

{

    int supp; /* format is supported */
    int bits; /* number of bits */
    int sgn;  /* signed/unsigned */
    int big;  /* big/litte endian */
    int flt;  /* floating point/integer */

    switch (fmt) {

        case SND_PCM_FORMAT_S8:
            bits = 8; sgn = 1; big = 0; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_U8:
            bits = 8; sgn = 0; big = 0; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_S16_LE:
            bits = 16; sgn = 1; big = 0; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_S16_BE:
            bits = 16; sgn = 1; big = 1; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_U16_LE:
            bits = 16; sgn = 0; big = 0; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_U16_BE:
            bits = 16; sgn = 0; big = 1; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_S24_LE:
            bits = 24; sgn = 1; big = 0; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_S24_BE:
            bits = 24; sgn = 1; big = 1; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_U24_LE:
            bits = 24; sgn = 0; big = 0; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_U24_BE:
            bits = 24; sgn = 0; big = 1; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_S32_LE:
            bits = 32; sgn = 1; big = 0; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_S32_BE:
            bits = 32; sgn = 1; big = 1; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_U32_LE:
            bits = 32; sgn = 0; big = 0; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_U32_BE:
            bits = 32; sgn = 0; big = 1; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_FLOAT_LE:
            bits = 32; sgn = 1; big = 0; flt = 1; supp = 1; break;
        case SND_PCM_FORMAT_FLOAT_BE:
            bits = 32; sgn = 1; big = 1; flt = 1; supp = 1; break;
        case SND_PCM_FORMAT_FLOAT64_LE:
            bits = 64; sgn = 1; big = 0; flt = 1; supp = 1; break;
        case SND_PCM_FORMAT_FLOAT64_BE:
            bits = 64; sgn = 1; big = 1; flt = 1; supp = 1; break;
        case SND_PCM_FORMAT_IEC958_SUBFRAME_LE:
            bits = 0; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_IEC958_SUBFRAME_BE:
            bits = 0; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_MU_LAW:
            bits = 0; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_A_LAW:
            bits = 0; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_IMA_ADPCM:
            bits = 0; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_MPEG:
            bits = 0; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_GSM:
            bits = 0; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_SPECIAL:
            bits = 0; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_S24_3LE:
            bits = 24; sgn = 1; big = 1; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_S24_3BE:
            bits = 24; sgn = 1; big = 1; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_U24_3LE:
            bits = 24; sgn = 0; big = 0; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_U24_3BE:
            bits = 24; sgn = 0; big = 1; flt = 0; supp = 1; break;
        case SND_PCM_FORMAT_S20_3LE:
            bits = 20; sgn = 1; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_S20_3BE:
            bits = 20; sgn = 1; big = 1; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_U20_3LE:
            bits = 20; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_U20_3BE:
            bits = 20; sgn = 0; big = 1; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_S18_3LE:
            bits = 18; sgn = 1; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_S18_3BE:
            bits = 18; sgn = 1; big = 1; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_U18_3LE:
            bits = 18; sgn = 0; big = 0; flt = 0; supp = 0; break;
        case SND_PCM_FORMAT_U18_3BE:
            bits = 18; sgn = 0; big = 1; flt = 0; supp = 0; break;

    }
    if (supp) { /* valid format, copy back parameters */

        *rbits = bits;
        *rsgn = sgn;
        *rbig = big;
        *rflt = flt;

    }

    return (supp);

}

/*******************************************************************************

Convert sound device parameters to ALSA format code

Using the alsa parameters for the given device, comes up with an ALSA format
code that matches. This is required since we use bits/sign/endian/float to
identify wave sample parameters instead of a code.

Does a linear search to find the format code.

*******************************************************************************/

static int params2alsa(devptr dp)

{

    int fmt;    /* ALSA format code */
    int supp;   /* format is supported */
    int bits;   /* number of bits */
    int sgn;    /* signed/unsigned */
    int big;    /* big/litte endian */
    int flt;    /* floating point/integer */
    int fndfmt; /* found format code */

    fndfmt = -1;
    for (fmt = 0; fmt < SND_PCM_FORMAT_LAST; fmt++) {

        supp = alsa2params(fmt, &bits, &sgn, &big, &flt);
        if (supp &&
            bits == dp->bits &&
            sgn == dp->sgn &&
            big == dp->big &&
            flt == dp->flt) fndfmt = fmt;

    }
    if (fndfmt == -1)
        error("No ALSA format equivalent for current wave parameters");

    return (fndfmt);

}

/*******************************************************************************

Issue MIDI 2 byte message

Sends a 2 byte MIDI message to the given ALSA midi port.

*******************************************************************************/

static void midimsg2(snd_rawmidi_t *midiout, byte sts, byte dat1)

{

    int r; /* function result */
    byte oa[2]; /* output holding array */

    /* load the output array */
    oa[0] = sts;
    oa[1] = dat1;
    /* send it */
    r = snd_rawmidi_write(midiout, oa, 2);
    if (r < 0) error("Unable to send to MIDI channel");

}

/*******************************************************************************

Issue MIDI 3 byte message

Sends a 3 byte MIDI message to the given ALSA midi port.

*******************************************************************************/

static void midimsg3(snd_rawmidi_t *midiout, byte sts, byte dat1, byte dat2)

{

    int r; /* function result */
    byte oa[3]; /* output holding array */

    /* load the output array */
    oa[0] = sts;
    oa[1] = dat1;
    oa[2] = dat2;
    /* send it */
    r = snd_rawmidi_write(midiout, oa, 3);
    if (r < 0) error("Unable to send to MIDI channel");

}

/*******************************************************************************

Find elapsed microsecond time corrected

Finds the elapsed time on the linux microsecond time, then corrects that for
100us time.

Linux time is kept as seconds and a microseconds offset from that. It does not
wrap.

*******************************************************************************/

static int timediff(struct timeval* rt)

{

    int ds; /* seconds difference */
    int du; /* microseconds difference */
    struct timeval tv; /* record to get time */

    gettimeofday(&tv, NULL); /* get current time */
    ds = tv.tv_sec-rt->tv_sec; /* find difference in seconds */
    du = tv.tv_usec-rt->tv_usec; /* find difference in microseconds */

    return (ds*10000+du/100); /* return difference in 100us increments */

}

/*******************************************************************************

Activate sequencer timer

If the sequencer timer is not running, we activate it.

*******************************************************************************/

static void acttim(int t)

{

    struct itimerspec ts; /* timer data */
    int    elap;          /* elapsed time */
    long tl;

    if (!seqlst) { /* nothing in sequencer list, activate timer */

        elap = timediff(&strtim); /* find elapsed time since seq start */
        tl = t-elap; /* set next time to run */
        ts.it_value.tv_sec = tl/10000; /* set number of seconds to run */
        /* set number of nanoseconds to run */
        ts.it_value.tv_nsec = tl%10000*100000;
        ts.it_interval.tv_sec = 0; /* set does not rerun */
        ts.it_interval.tv_nsec = 0;
        timerfd_settime(seqhan, 0, &ts, NULL);
        seqtimact = true; /* set sequencer timer active */

        /* count active sequence instances */
        pthread_mutex_lock(&snmlck);
        numseq++; /* count active */
        pthread_mutex_unlock(&snmlck); /* release lock */

    }

}

/*******************************************************************************

Get sequencer message entry

Gets a sequencer message entry, either from the used list, or new.

Its faster to recycle these from a private list.

*******************************************************************************/

static void getseq(seqptr* p)

{

    if (seqfre) { /* get a previous sequencer message */

        *p = seqfre; /* index top entry */
        seqfre = seqfre->next; /* gap out */

    } else
        /* else get a new entry, with full allocation */
        *p = (seqptr) malloc(sizeof(seqmsg));
    (*p)->next = NULL; /* clear next */

}

/*******************************************************************************

Put sequencer message entry

Puts a sequencer message entry to the free list for reuse.

*******************************************************************************/

static void putseq(seqptr p)

{

   p->next = seqfre; /* link to top of list */
   seqfre = p; /* push onto list */

}

/*******************************************************************************

Dump sequencer list

A diagnostic, dumps the given sequencer list in ASCII.

*******************************************************************************/

static void dmpseq(seqptr p)

{

    switch (p->st) { /* sequencer message type */

        case st_noteon:       printf("noteon: Time: %d Port: %d Channel: %d "
                                     "Note: %d Velocity: %d\n",
                                     p->time, p->port, p->ntc, p->ntn, p->ntv);
                              break;
        case st_noteoff:      printf("noteoff: Time: %d Port: %d Channel: %d "
                                     "Note: %d Velocity: %d\n", p->time,
                                     p->port, p->ntc, p->ntn, p->ntv);
                              break;
        case st_instchange:   printf("instchange: Time: %d Port: %d p->port "
                                     "Channel: %d Instrument: %d\n", p->time,
                                     p->port, p->icc, p->ici);
                              break;
        case st_attack:       printf("attack: Time: %d Port: %d Channel: %d "
                                     "attack time: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_release:      printf("release: Time: %d Port: %d Channel: %d "
                                     "release time: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_legato:       printf("legato: Time: %d Port: %d Channel: %d "
                                     "legato on/off: %d\n", p->time, p->port,
                                     p->bsc, p->bsb);
                              break;
        case st_portamento:   printf("portamento: Time: %d Port: %d Channel: %d "
                                     "portamento on/off: %d\n", p->time, p->port,
                                     p->bsc, p->bsb);
                              break;
        case st_vibrato:      printf("vibrato: Time: %d Port: %d Channel: %d "
                                     "Vibrato: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_volsynthchan: printf("volsynthchan: Time: %d Port: %d Channel: %d "
                                     "Volume: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_porttime:     printf("porttime: Time: %d Port: %d Channel: %d "
                                     "Portamento time: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_balance:      printf("attack: Time: %d Port: %d Channel: %d "
                                     "Ballance: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_pan:          printf("pan: Time: %d Port: %d Channel: %d "
                                     "Pan: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_timbre:       printf("timbre: Time: %d Port: %d Channel: %d "
                                     "Timbre: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_brightness:   printf("brightness: Time: %d Port: %d Channel: %d "
                                     "Brightness: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_reverb:       printf("reverb: Time: %d Port: %d Channel: %d "
                                     "Reverb: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_tremulo:      printf("tremulo: Time: %d Port: %d Channel: %d "
                                     "Tremulo: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_chorus:       printf("chorus: Time: %d Port: %d Channel: %d "
                                     "Chorus: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_celeste:      printf("celeste: Time: %d Port: %d Channel: %d "
                                     "Celeste: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_phaser:       printf("Phaser: Time: %d Port: %d Channel: %d "
                                     "Phaser: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_aftertouch:   printf("aftertouch: Time: %d Port: %d Channel: %d "
                                     "Note: %d Aftertouch: %d\n", p->time,
                                     p->port, p->ntc, p->ntn, p->ntv);
                              break;
        case st_pressure:     printf("pressure: Time: %d Port: %d Channel: %d "
                                     "Pressure: %d\n", p->time, p->port, p->ntc,
                                     p->ntv);
                              break;
        case st_pitch:        printf("pitch: Time: %d Port: %d Channel: %d "
                                     "Pitch: %d\n", p->time, p->port, p->vsc,
                                     p->vsv);
                              break;
        case st_pitchrange:   printf("pitchrange: Time: %d Port: %d Channel: %d "
                                     "Pitch range: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_mono:         printf("mono: Time: %d Port: %d Channel: %d "
                                     "Mono notes: %d\n", p->time, p->port,
                                     p->vsc, p->vsv);
                              break;
        case st_poly:         printf("poly: Time: %d Port: %d Channel: %d\n",
                                     p->time, p->port, p->pc);
                              break;
        case st_playsynth:    printf("playsynth: Time: %d Port: %d "
                                     ".mid file id: %d\n", p->time, p->port,
                                     p->sid);
                              break;
        case st_playwave:     printf("playwave: Time: %d Port: %d "
                                     ".wav file logical number: %d\n", p->time,
                                     p->port, p->wt);
                              break;
        case st_volwave:      printf("volwave: Time: %d Port: %d Volume: %d\n",
                                     p->time, p->port, p->wv);
                              break;

    }

}

/*******************************************************************************

Dump sequencer list

A diagnostic, dumps the given sequencer list in ASCII.

*******************************************************************************/

static void dmpseqlst(seqptr p)

{

    while (p) {

        dmpseq(p);
        p = p->next;

    }

}

/*******************************************************************************

Put sequencer list

Frees a sequencer instruction list.

*******************************************************************************/

static void putseqlst(seqptr p)

{

    seqptr tp;

    while (p) {

        tp = p; /* remove top entry from list */
        p = p->next;
        putseq(tp); /* free that */

    }

}


/*******************************************************************************

Insert sequencer message

Inserts a sequencer message into the list, in ascending time order.

*******************************************************************************/

static void insseq(seqptr p)

{

    seqptr lp, l;

    pthread_mutex_lock(&seqlock); /* take sequencer data lock */
    /* check sequencer list empty */
    if (!seqlst) seqlst = p; /* place as first if so */
    /* check insert to start */
    else if (p->time < seqlst->time) {

        p->next = seqlst; /* push onto list */
        seqlst = p;

    } else { /* must search list */

        lp = seqlst; /* index top of list */
        while (p->time >= lp->time && lp->next) {

            /* skip to key entry or end */
            l = lp; /* set last */
            lp = lp->next; /* index next */

        }
        if (p->time < lp->time) { /* insert here */

            l->next = p; /* link last to this */
            p->next = lp; /* link this to next */

        } else { /* insert after */

            p->next = lp->next;
            lp->next = p;

        }

    }
    pthread_mutex_unlock(&seqlock); /* release lock */

}

/*******************************************************************************

Controller change

Processes a controller value set, from 0 to 127.

*******************************************************************************/

static void ctlchg(int p, pa_channel c, int cn, int v)

{

    /* construct midi message */
    midimsg3(alsamidiout[p-1]->midi, MESS_CTRL_CHG+(c-1), cn, v/0x01000000);

}

/*******************************************************************************

Execute sequencer message

Directly encodes the sequencer message in MIDI format, and outputs to the MIDI
device assocated with the port.

*******************************************************************************/

void _pa_excseq(int p, seqptr sp)

{

    int b;
    int pt;

    switch (sp->st) { /* sequencer message type */

        case st_noteon:
            midimsg3(alsamidiout[sp->port-1]->midi, MESS_NOTE_ON+(sp->ntc-1),
                     sp->ntn-1, sp->ntv/0x01000000);
            break;
        case st_noteoff:
            midimsg3(alsamidiout[sp->port-1]->midi, MESS_NOTE_OFF+(sp->ntc-1),
                     sp->ntn-1, sp->ntv/0x01000000);
            break;
        case st_instchange:
            midimsg2(alsamidiout[sp->port-1]->midi, MESS_PGM_CHG+(sp->icc-1),
                     sp->ici-1);
            break;
        case st_attack: ctlchg(sp->port, sp->vsc, CTLR_SOUND_ATTACK_TIME,
                               sp->vsv/0x01000000);
            break;
        case st_release: ctlchg(sp->port, sp->vsc, CTLR_SOUND_RELEASE_TIME, sp->vsv/0x01000000);
            break;
        case st_legato: ctlchg(sp->port, sp->bsc, CTLR_LEGATO_PEDAL, !!sp->bsb*0x7fffffff);
            break;
        case st_portamento: ctlchg(sp->port, sp->bsc, CTLR_PORTAMENTO, !!sp->bsb*0x7fffffff);
            break;
        case st_vibrato:
            /* set high */
            ctlchg(sp->port, sp->vsc, CTLR_MODULATION_WHEEL_COARSE, sp->vsv/0x01000000);
            /* set low */
            ctlchg(sp->port, sp->vsc, CTLR_MODULATION_WHEEL_FINE, sp->vsv/0x00020000 & 0x7f);
            break;
        case st_volsynthchan:
            ctlchg(sp->port, sp->vsc, CTLR_VOLUME_COARSE, sp->vsv/0x01000000); /* set high */
            ctlchg(sp->port, sp->vsc, CTLR_VOLUME_FINE, sp->vsv/0x00020000 & 0x7f); /* set low */
            break;
        case st_porttime:
            /* set high */
            ctlchg(sp->port, sp->vsc, CTLR_PORTAMENTO_TIME_COARSE, sp->vsv/0x01000000);
            /* set low */
            ctlchg(sp->port, sp->vsc, CTLR_PORTAMENTO_TIME_FINE, sp->vsv/0x00020000 & 0x7f);
            break;
        case st_balance:
            b = sp->vsv/0x00040000+0x2000; /* reduce to 14 bits, positive only */
            ctlchg(sp->port, sp->vsc, CTLR_BALANCE_COARSE, b/0x80); /* set high */
            ctlchg(sp->port, sp->vsc, CTLR_BALANCE_FINE, b & 0x7f); /* set low */
            break;
        case st_pan:
            b = sp->vsv/0x00040000+0x2000; /* reduce to 14 bits, positive only */
            ctlchg(sp->port, sp->vsc, CTLR_PAN_POSITION_COARSE, b/0x80); /* set high */
            ctlchg(sp->port, sp->vsc, CTLR_PAN_POSITION_FINE, b & 0x7f); /* set low */
            break;
        case st_timbre:
            ctlchg(sp->port, sp->vsc, CTLR_SOUND_TIMBRE, sp->vsv/0x01000000);
            break;
        case st_brightness:
            ctlchg(sp->port, sp->vsc, CTLR_SOUND_BRIGHTNESS, sp->vsv/0x01000000);
            break;
        case st_reverb:
            ctlchg(sp->port, sp->vsc, CTLR_EFFECTS_LEVEL, sp->vsv/0x01000000);
            break;
        case st_tremulo:
            ctlchg(sp->port, sp->vsc, CTLR_TREMULO_LEVEL, sp->vsv/0x01000000);
            break;
        case st_chorus:
            ctlchg(sp->port, sp->vsc, CTLR_CHORUS_LEVEL, sp->vsv/0x01000000);
            break;
        case st_celeste:
            ctlchg(sp->port, sp->vsc, CTLR_CELESTE_LEVEL, sp->vsv/0x01000000);
            break;
        case st_phaser:
            ctlchg(sp->port, sp->vsc, CTLR_PHASER_LEVEL, sp->vsv/0x01000000);
            break;
        case st_aftertouch:
            midimsg3(alsamidiout[sp->port-1]->midi, MESS_AFTTCH+(sp->ntc-1),
                     sp->ntn-1, sp->ntv/0x01000000);
            break;
        case st_pressure:
            midimsg2(alsamidiout[sp->port-1]->midi, MESS_CHN_PRES+(sp->ntc-1),
                     sp->ntv/0x01000000);
            break;
        case st_pitch:
            pt = sp->vsv/0x00040000+0x2000; /* reduce to 14 bits, positive only */
            /* construct midi message */
            midimsg3(alsamidiout[sp->port-1]->midi, MESS_PTCH_WHL+(sp->vsc-1),
                     pt & 0x7f, pt/0x80);
            break;
        case st_pitchrange:
            /* set up data entry */
            /* set high */
            ctlchg(sp->port, sp->vsc, CTLR_REGISTERED_PARAMETER_COARSE, 0);
            /* set low */
            ctlchg(sp->port, sp->vsc, CTLR_REGISTERED_PARAMETER_FINE, 0);
            /* set high */
            ctlchg(sp->port, sp->vsc, CTLR_DATA_ENTRY_COARSE, sp->vsv/0x01000000);
            /* set low */
            ctlchg(sp->port, sp->vsc, CTLR_DATA_ENTRY_FINE,
                   sp->vsv/0x00020000 & 0x7f);
            break;
        case st_mono:
            ctlchg(sp->port, sp->vsc, CTLR_MONO_OPERATION, sp->vsv);
            break;
        case st_poly:
            /* value dosen't matter */
            ctlchg(sp->port, sp->pc, CTLR_POLY_OPERATION, 0);
            break;
        case st_playsynth:
            alsaplaysynth_kickoff(sp->port, sp->sid); /* play the file */
            break;
        case st_playwave:
            alsaplaywave_kickoff(sp->port, sp->wt);
            break;
        case st_volwave:
            /* not implemented at present */
            break;

    }

}

/*******************************************************************************

Open ALSA MIDI device

Opens an ALSA MIDI port for use.

*******************************************************************************/

static void openalsamidi(int p)

{

    int r;

    r = snd_rawmidi_open(NULL, &alsamidiout[p-1]->midi, alsamidiout[p-1]->name,
                         SND_RAWMIDI_SYNC);
    if (r < 0) alsaerror(r);

}

/*******************************************************************************

Close sequencer output device

Closes a sequencer output device for use. We shut down the channel as well as
possible. The MIDI spec states we are to send noteoff commands for every noteon
command that was sent, but this is impractical. In practice, synths respond to
the channel messages without the "every noteon" requirement of the standard.
In fact, all notes off and/or all sound off typically affects all channels
regardless. We do the safe/complete thing there.

*******************************************************************************/

static void closealsamidi(int p)

{

    pa_channel c;

    /* send all all notes off to all channels */
    for (c = 1; c <= 16; c++) ctlchg(p, c, CTLR_ALL_NOTES_OFF, 0);
    /* send all sound off to all channels */
    for (c = 1; c <= 16; c++) ctlchg(p, c, CTLR_ALL_SOUND_OFF, 0);
    snd_rawmidi_close(alsamidiout[p-1]->midi); /* close port */

}

/*******************************************************************************

Open output device

Opens a sequencer output device for use. Somewhat misnamed, it actually opens
a midi port for output. It uses sequencer entries to specify the MIDI commands
to be output.

*******************************************************************************/

static void opnseq(seqptr sp)

{

    alsamidiout[sp->port-1]->opnseq(sp->port);

}

/*******************************************************************************

Close sequencer output device

Closes a sequencer output device for use.

*******************************************************************************/

static void clsseq(seqptr sp)

{

    alsamidiout[sp->port-1]->clsseq(sp->port);

}

/*******************************************************************************

Write sequencer entry

Writes a sequencer entry to the device given by it's port.

*******************************************************************************/

static void wrtseq(seqptr sp)

{

    alsamidiout[sp->port-1]->wrseq(sp->port, sp);

}

/*******************************************************************************

Define plug-in sequencer output device

Lets a sequencer plug-in define a device. Accepts three vectors, the open, close
and write vectors. Returns the defined device.

Plug-ins go into the MIDI output device table at position 1, meaning that the
plug-in takes over the default device.

*******************************************************************************/

void _pa_synthoutplug(
    /* name */            string name,
    /* open sequencer */  void (*opnseq)(int p),
    /* close sequencer */ void (*clsseq)(int p),
    /* write sequencer */ void (*wrseq)(int p, seqptr sp)
)

{

    int i;

    if (alsamidioutnum >= MAXMIDP) error("Device table full");
    /* move table entries above plug-ins up one */
    for (i = MAXMIDP-1; i > alsamidioutplug; i--)
        alsamidiout[i] = alsamidiout[i-1];
    alsamidiout[alsamidioutplug] = malloc(sizeof(snddev)); /* create new device entry */
    alsamidiout[alsamidioutplug]->name = malloc(strlen(name)+1); /* place name of device */
    strcpy(alsamidiout[alsamidioutplug]->name, name);
    alsamidiout[alsamidioutplug]->last = 0; /* clear last byte */
    alsamidiout[alsamidioutplug]->pback = -1; /* set no pushback */
    alsamidiout[alsamidioutplug]->opnseq = opnseq; /* set open alsa midi device */
    alsamidiout[alsamidioutplug]->clsseq = clsseq; /* set close alsa midi device */
    alsamidiout[alsamidioutplug]->wrseq = wrseq; /* set sequencer execute function */
    alsamidioutnum++; /* count total devices */
    alsamidioutplug++; /* count plug-ins */

}

/*******************************************************************************

Translate logical sythesizer out handle to ALSA handle

Validates a synth out handle and returns the assocated ALSA MIDI out handle.
This is commonly used for bypass operations, or programs that directly access
the ALSA functions even though using sound.c.

Please read the PA guide on the subject of bypass operations. Bypassing can
solve problems, adapt to specific OS issues, or extend the interface. It can
also cause difficult to debug problems when the sound.c ALSA interface conflicts
with what the bypass operations do. And sound.c can perform such operations via
a direct user call OR a background thread.

*******************************************************************************/

snd_rawmidi_t* _pa_getsythouthdl(int p)

{

    if (p < 1 || p > MAXMIDP) error("Invalid synth output port");
    if (!alsamidiout[p-1])
        error("No synth output device defined at logical number");

    return (alsamidiout[p-1]->midi);

}

/*******************************************************************************

Translate logical sythesizer in handle to ALSA handle

Validates a synth in handle and returns the assocated ALSA MIDI in handle.
This is commonly used for bypass operations, or programs that directly access
the ALSA functions even though using sound.c.

Please read the PA guide on the subject of bypass operations. Bypassing can
solve problems, adapt to specific OS issues, or extend the interface. It can
also cause difficult to debug problems when the sound.c ALSA interface conflicts
with what the bypass operations do. And sound.c can perform such operations via
a direct user call OR a background thread.

*******************************************************************************/

snd_rawmidi_t* _pa_getsythinhdl(int p)

{

    if (p < 1 || p > MAXMIDP) error("Invalid synth input port");
    if (!alsamidiin[p-1])
        error("No synth input device defined at logical number");

    return (alsamidiin[p-1]->midi);

}

/*******************************************************************************

Translate logical wave out handle to ALSA handle

Validates a wave out handle and returns the assocated ALSA PCM out handle.
This is commonly used for bypass operations, or programs that directly access
the ALSA functions even though using sound.c.

Please read the PA guide on the subject of bypass operations. Bypassing can
solve problems, adapt to specific OS issues, or extend the interface. It can
also cause difficult to debug problems when the sound.c ALSA interface conflicts
with what the bypass operations do. And sound.c can perform such operations via
a direct user call OR a background thread.

*******************************************************************************/

snd_pcm_t* _pa_getwaveouthdl(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1])
        error("No wave output device defined at logical number");

    return (alsapcmout[p-1]->pcm);

}

/*******************************************************************************

Translate logical wave in handle to ALSA handle

Validates a wave in handle and returns the assocated ALSA PCM in handle.
This is commonly used for bypass operations, or programs that directly access
the ALSA functions even though using sound.c.

Please read the PA guide on the subject of bypass operations. Bypassing can
solve problems, adapt to specific OS issues, or extend the interface. It can
also cause difficult to debug problems when the sound.c ALSA interface conflicts
with what the bypass operations do. And sound.c can perform such operations via
a direct user call OR a background thread.

*******************************************************************************/

snd_pcm_t* _pa_getwaveinhdl(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1])
        error("No wave input device defined at logical number");

    return (alsapcmin[p-1]->pcm);

}

/*******************************************************************************

Timer thread

Called when the windows event timer expires, we first check if the sequencer
is still running. If not, we do nothing, because we may have been called
while the sequencer is being shut down. If it is running, we then take all
messages off the top of the queue that have become due. Timer overruns are
handled by executing all past due events, on the idea that things like volume
changes, etc, need to be performed to stay in sync. If notes are past due,
this will cause "note scramble" for a short time, and we might have to improve
this.

After all due messages are cleared, if the queue still has active messages,
then another timer is set for that new top message. This keeps the queue moving
until clear.

*******************************************************************************/

static void* sequencer_thread(void* data)

{

    seqptr p;             /* message entry pointer */
    int    elap;          /* elapsed time */
    struct itimerspec ts; /* timer data */
    long tl;
    int r;

    while (1) { /* until thread cancelled */

        /* no input is active, load a new signaler set */
        ifdsets = ifdseta; /* set up request set */
        r = select(ifdmax, &ifdsets, NULL, NULL, NULL);
        /* if error, the input set won't be modified and thus will appear as
           if they were active. We clear them in this case */
        if (r < 0) FD_ZERO(&ifdsets);
        if (FD_ISSET(seqhan, &ifdsets)) {

            /* the sequencer timer went off */
            if (seqrun) { /* sequencer is still running */

                pthread_mutex_lock(&seqlock); /* take sequencer data lock */
                p = seqlst; /* index top of list */
                elap = timediff(&strtim); /* find elapsed time since seq start */
                while (p) { /* process all past due messages */

                    if (p->time <= elap) { /* execute this message */

                        seqlst = seqlst->next; /* gap out */
                        wrtseq(p); /* execute it */
                        putseq(p); /* release entry */
                        p = seqlst; /* index top of list again */

                    } else p = NULL; /* stop search */

                }
                if (seqlst) { /* start sequencer timer again */

                    tl = seqlst->time-elap; /* set next time to run */
                    ts.it_value.tv_sec = tl/10000; /* set number of seconds to run */
                    ts.it_value.tv_nsec = tl%10000*100000; /* set number of nanoseconds to run */
                    ts.it_interval.tv_sec = 0; /* set does not rerun */
                    ts.it_interval.tv_nsec = 0;
                    timerfd_settime(seqhan, 0, &ts, NULL);
                    seqtimact = true; /* set sequencer timer active */

                } else seqtimact = false; /* set quencer timer inactive */
                pthread_mutex_unlock(&seqlock);
                if (!seqtimact) {

                    /* count active sequencer instances */
                    pthread_mutex_lock(&snmlck);
                    numseq--; /* count active */
                    /* if now zero, signal zero crossing */
                    if (!numseq) pthread_cond_signal(&snmzer);
                    pthread_mutex_unlock(&snmlck); /* release lock */
                    if (numseq < 0) error("Wave locking imbalance");

                }

            }

        }

    }

}

/*******************************************************************************

Find number of output midi ports

Returns the total number of output midi ports.

*******************************************************************************/

int pa_synthout(void)

{

   return (alsamidioutnum);

}

/*******************************************************************************

Find number of input midi ports

Returns the total number of input midi ports.

*******************************************************************************/

int pa_synthin(void)

{

   return (alsamidiinnum);

}

/*******************************************************************************

Open synthesiser output port

Opens a syth output port. The output ports have their own separate logical
numbers separate from input numbers, and by convention, port 1 will be the
main synthesizer for the computer, and port 2 will be an output port to any
midi chained devices outside the computer.

*******************************************************************************/

void pa_opensynthout(int p)

{

    if (p < 1 || p > MAXMIDP) error("Invalid synthesizer port");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (alsamidiout[p-1]->devopn) error("Synthsizer port already open");

    alsamidiout[p-1]->opnseq(p); /* open port */
    alsamidiout[p-1]->devopn = true; /* set open */

}

/*******************************************************************************

Close midi sythesiser output port

Closes a previously opened midi output port.

*******************************************************************************/

void pa_closesynthout(int p)

{

    if (p < 1 || p > MAXMIDP) error("Invalid synthesizer port");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");

    alsamidiout[p-1]->clsseq(p); /* close port */
    alsamidiout[p-1]->devopn = false; /* set closed */

}

/*******************************************************************************

Start time output

Starts the sequencer function. The sequencer is cleared, and upcount begins
after this call. Before a sequencer start, any notes marked as "sequenced" by
having a non-zero time value would cause an error. After sequencer start,
they are either:

1. Discarded if the time has already passed.

2. Send immediately if the time is now (or very close to now).

3. Buffered and scheduled to be set out at the correct time.

We mark sequencer start by recording the start base time, which is the
windows free running time that all sequencer times will be measured from.
That counter is 32 bits unsigned millisecond, which gives 49.71 days. We 
take half that, because we store as signed, so we are 24.855 days full
sequencer time.

Note that there will be no sequencer events, because we don't allow them to
be set without the sequencer running. The first event will be kicked off by
a sequenced event.

*******************************************************************************/

void pa_starttimeout(void)

{

    gettimeofday(&strtim, NULL); /* get current time */
    seqrun = true; /* set sequencer running */

}

/*******************************************************************************

Stop time output

Stops midi sequencer function. Any timers and buffers in use by the sequencer
are cleared, and all pending events dropped.

Note that this does not stop any midi files from being sequenced.

*******************************************************************************/

void pa_stoptimeout(void)

{

    seqptr p; /* message pointer */
    struct itimerspec ts;

    strtim.tv_sec = 0; /* clear start time */
    strtim.tv_usec = 0;
    seqrun = false; /* set sequencer not running */
    /* if there is a pending sequencer timer, kill it */
    if (seqtimact) {

        /* set timer run time to zero to kill it */
        ts.it_value.tv_sec = 0;
        ts.it_value.tv_nsec = 0;
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;
        timerfd_settime(seqhan, 0, &ts, NULL);
        seqtimact = false; /* set sequencer timer inactive */

    }
    /* now clear all pending events */
    pthread_mutex_lock(&seqlock); /* take sequencer data lock */
    while (seqlst) { /* clear */

        p = seqlst; /* index top of list */
        seqlst = seqlst->next; /* gap out */
        putseq(p); /* release entry */

    }
    pthread_mutex_unlock(&seqlock); /* drop lock */

}

/*******************************************************************************

Get current time output

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

*******************************************************************************/

int pa_curtimeout(void)

{

   if (!seqrun) error("Sequencer not running");

   return (timediff(&strtim)); /* return difference time */

}






/*******************************************************************************

Start time input

Marks the time basis for input midi streams. Normally, MIDI input streams are
marked with 0 time, which means unsequenced. However, starting the input time
will mark MIDI inputs with their arrival time, which can be used to sequence
the MIDI commands. Stopping the time will return to marking 0 time.

*******************************************************************************/

void pa_starttimein(void)

{

    gettimeofday(&sintim, NULL); /* get current time */
    sinrun = true; /* set sequencer running */

}

/*******************************************************************************

Stop time input

Simply sets that we are not marking input time anymore.

*******************************************************************************/

void pa_stoptimein(void)

{

    sinrun = false;

}

/*******************************************************************************

Get current time output

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

*******************************************************************************/

int pa_curtimein(void)

{

   if (!sinrun) error("Input MIDI time is not marking");

   return (timediff(&sintim)); /* return difference time */

}

/*******************************************************************************

Note on

Turns on a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to maxint.

*******************************************************************************/

void pa_noteon(int p, int t, pa_channel c, pa_note n, int v)



{

    int     r;    /* return value */
    int     elap; /* current elapsed time */
    seqptr  sp;   /* message pointer */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_noteon; /* set type */
    sp->ntc = c; /* set channel */
    sp->ntn = n; /* set note */
    sp->ntv = v; /* set velocity */

    /* find disposition of command */
    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Note off

Turns off a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to maxint.

*******************************************************************************/

void pa_noteoff(int p, int t, pa_channel c, pa_note n, int v)

{

    int r;
    int elap;  /* current elapsed time */
    seqptr sp; /* message pointer */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_noteoff; /* set type */
    sp->ntc = c; /* set channel */
    sp->ntn = n; /* set note */
    sp->ntv = v; /* set velocity */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Instrument change

Selects a new instrument for the given channel. Tbe new instrument is specified
by Midi GM encoding, 1 to 128. Takes a time for sequencing.

*******************************************************************************/

void pa_instchange(int p, int t, pa_channel c, pa_instrument i)

{

    int r;
    int elap;  /* current elapsed time */
    seqptr sp; /* message pointer */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");
    if (i < 1 || i > 128) error("Bad instrument number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_instchange; /* set type */
    sp->icc = c; /* set channel */
    sp->ici = i; /* set instrument */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set attack time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

*******************************************************************************/

void pa_attack(int p, int t, pa_channel c, int at)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_attack; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = at; /* set attack */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set release time

Sets the time of release on a note, ie., how long it takes for the note to go
full off.

*******************************************************************************/

void pa_release(int p, int t, pa_channel c, int rt)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_release; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = rt; /* set release */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Legato pedal on/off

Sets the legato mode on/off.

*******************************************************************************/

void pa_legato(int p, int t, pa_channel c, boolean b)

{

    int elap;  /* current elapsed time */
    seqptr sp; /* message pointer */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_legato; /* set type */
    sp->bsc = c; /* set channel */
    sp->bsb = b; /* set on/off */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Portamento pedal on/off

Sets the portamento mode on/off.

*******************************************************************************/

void pa_portamento(int p, int t, pa_channel c, boolean b)

{

    int elap;  /* current elapsed time */
    seqptr sp; /* message pointer */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_portamento; /* set type */
    sp->bsc = c; /* set channel */
    sp->bsb = b; /* set on/off */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set volume

Sets synthesizer volume, 0 to maxint.

*******************************************************************************/

void pa_volsynthchan(int p, int t, pa_channel c, int v)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_volsynthchan; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = v; /* set volume */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set left right channel balance

Sets sets the left right channel balance. -maxint is all left, 0 is center,
maxint is all right.

*******************************************************************************/

void pa_balance(int p, int t, pa_channel c, int b)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_balance; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = b; /* set balance */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set portamento time

Sets portamento time, 0 to maxint.

*******************************************************************************/

void pa_porttime(int p, int t, pa_channel c, int v)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_porttime; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = v; /* set time */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set vibrato

Sets modulaton value, 0 to maxint.

*******************************************************************************/

void pa_vibrato(int p, int t, pa_channel c, int v)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_vibrato; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = v; /* set vibrato */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set left/right pan position

Sets sets the left/right pan position. -maxint is hard left, 0 is center,
maxint is hard right.

*******************************************************************************/

void pa_pan(int p, int t, pa_channel c, int b)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_pan; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = b; /* set pan */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound timbre

Sets the sound timbre, 0 to maxint.

*******************************************************************************/

void pa_timbre(int p, int t, pa_channel c, int tb)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_timbre; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = tb; /* set timbre */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound brightness

Sets the sound brightness, 0 to maxint.

*******************************************************************************/

void pa_brightness(int p, int t, pa_channel c, int b)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_brightness; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = b; /* set brightness */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

   }

}

/*******************************************************************************

Set sound reverb

Sets the sound reverb, 0 to maxint.

*******************************************************************************/

void pa_reverb(int p, int t, pa_channel c, int r)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_reverb; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = r; /* set reverb */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

       /* check sequencer running */
       if (!seqrun) error("Sequencer not running");
       insseq(sp); /* insert to sequencer list */
       acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound tremulo

Sets the sound tremulo, 0 to maxint.

*******************************************************************************/

void pa_tremulo(int p, int t, pa_channel c, int tr)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_tremulo; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = tr; /* set tremulo */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
       if (!seqrun) error("Sequencer not running");
       insseq(sp); /* insert to sequencer list */
       acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound chorus

Sets the sound chorus, 0 to maxint.

*******************************************************************************/

void pa_chorus(int p, int t, pa_channel c, int cr)

{

    seqptr sp;  /* message pointer */
    int elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    if (!seqrun) error("Sequencer not running");
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_chorus; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = cr; /* set chorus */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound celeste

Sets the sound celeste, 0 to maxint.

*******************************************************************************/

void pa_celeste(int p, int t, pa_channel c, int ce)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_celeste; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = ce; /* set celeste */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound phaser

Sets the sound phaser, 0 to maxint.

*******************************************************************************/

void pa_phaser(int p, int t, pa_channel c, int ph)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_phaser; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = ph; /* set phaser */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set pitch range

Sets the range of pitch that can be reached by the pitch adjustment. The range
is from 0 to maxint, and represents from from 0 to 127 semitones. This means
that a setting of maxint means that every note in the midi scale can be reached,
with an accuracy of 128 of the space from C to C#. At this setting, any note
could be reached with a slide, for example.

*******************************************************************************/

void pa_pitchrange(int p, int t, pa_channel c, int v)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_pitchrange; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = v; /* set pitchrange */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set monophonic mode

If ommni is off, this sets how many channels to respond to. If omni is on,
then only once note at a time will be played. The select is from 0 to 16,
with 0 being "allways select single note mode".

*******************************************************************************/

void pa_mono(int p, int t, pa_channel c, int ch)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");
    if (ch < 0 || c > 16) error("Bad mono mode number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_mono; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = ch; /* set mono mode */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set polyphonic mode

Reenables polyphonic mode after a monophonic operation.

*******************************************************************************/

void pa_poly(int p, int t, pa_channel c)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_poly; /* set type */
    sp->pc = c; /* set channel */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Aftertouch

Controls aftertouch, 0 to maxint, on a note.

*******************************************************************************/

void pa_aftertouch(int p, int t, pa_channel c, pa_note n, int at)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_aftertouch; /* set type */
    sp->ntc = c; /* set channel */
    sp->ntn = n; /* set note */
    sp->ntv = at; /* set aftertouch */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

       /* check sequencer running */
       if (!seqrun) error("Sequencer not running");
       insseq(sp); /* insert to sequencer list */
       acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Channel pressure

Controls channel pressure, 0 to maxint.

*******************************************************************************/

void pa_pressure(int p, int t, pa_channel c, int pr)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_pressure; /* set type */
    sp->ntc = c; /* set channel */
    sp->ntv = pr; /* set pressure */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set pitch wheel

Sets the pitch wheel value, from -maxint to maxint. This is the amount off the
note in the channel. The GM standard is to adjust for a whole step up and down,
which is 4 half steps total. A "half step" is the difference between, say, C and
C#.

*******************************************************************************/

void pa_pitch(int p, int t, pa_channel c, int pt)

{

    seqptr sp;  /* message pointer */
    int elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Bad port number");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");
    if (c < 1 || c > 16) error("Bad channel number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_pitch; /* set type */
    sp->vsc = c; /* set channel */
    sp->vsv = pt; /* set pitch */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Play ALSA midi file

Plays the given ALSA midi file given the filename.

*******************************************************************************/

static void *alsaplaymidi(void* data)

{

    int               curtim;  /* current time in 100us */
    int               qnote;   /* number of 100us/quarter note */
    seqptr            sp;      /* sequencer entry */
    seqptr            seqlst;  /* sorted sequencer list */
    int               tfd;     /* timer file descriptor */
    struct itimerspec ts;      /* timer data */
    uint64_t          exp;     /* timer expire value */
    struct timeval    strtim;  /* start time for sequencer */
    portidptr         pip;     /* pointer for data we need */
    int               s;       /* synthesizer file instance */
    int               p;       /* port */
    long              tl;

    pip = (portidptr) data; /* get data pointer */
    s = pip->id; /* get id */
    p = pip->port; /* get port */
    pthread_mutex_lock(&synlck); /* take wave table lock */
    seqlst = syntab[s]; /* get the existing entry */
    pthread_mutex_unlock(&synlck); /* release lock */
    if (!seqlst) return (NULL); /* entry is empty, abort */

    /* count active sequence instances */
    pthread_mutex_lock(&snmlck);
    numseq++; /* count active */
    numsql[s]++;
    pthread_mutex_unlock(&snmlck); /* release lock */

    /* sequence and destroy the master list */
    tfd = timerfd_create(CLOCK_REALTIME, 0); /* create timer */
    gettimeofday(&strtim, NULL); /* get current time */
    while (seqlst) {

        curtim = timediff(&strtim); /* find elapsed time since seq start */
        tl = seqlst->time-curtim; /* set next time to run */
        /* check event still is in the future */
        if (tl > 0) {

            ts.it_value.tv_sec = tl/10000; /* set number of seconds to run */
            ts.it_value.tv_nsec = tl%10000*100000; /* set number of nanoseconds to run */
            ts.it_interval.tv_sec = 0; /* set does not rerun */
            ts.it_interval.tv_nsec = 0;
            timerfd_settime(tfd, 0, &ts, NULL);
            read(tfd, &exp, sizeof(uint64_t)); /* wait for timer expire */

        }
        /* Change the port to be the one requested. In this case we are treating
           the sequencer list to be a form to be applied to any port */
        seqlst->port = p;
        wrtseq(seqlst); /* execute top entry */
        seqlst = seqlst->next;

    }
    close(tfd); /* release the timer */

    /* count active sequencer instances */
    pthread_mutex_lock(&snmlck);
    numseq--; /* count active */
    numsql[s]--;
    /* if now zero, signal zero crossing */
    if (!numseq) pthread_cond_signal(&snmzer);
    pthread_mutex_unlock(&snmlck); /* release lock */
    if (numseq < 0) error("Sequencer locking imbalance");
    free(pip); /* release data pointer */

    return (NULL);

}

/*******************************************************************************

Play ALSA synth file kickoff routine

Plays an ALSA .mid file. This is the kickoff routine. We play .mid files on a
thread, and this routine spawns a thread to accomplish that. The thread is
"set and forget", in that it self terminates when the play routine terminates.

*******************************************************************************/

void alsaplaysynth_kickoff(int p, int s)

{

    pthread_t tid; /* thread id (unused) */
    portidptr pip; /* port/id structure */

    pip = malloc(sizeof(portid)); /* get a port/id structure */
    pip->port = p; /* place port */
    pip->id = s; /* place synth file id */
    pthread_create(&tid, NULL, alsaplaymidi, (void*)pip);

}

/*******************************************************************************

Load synthesizer file

Loads a synthesizer control file, usually midi format, into a logical cache,
from 1 to N. These are loaded up into memory for minimum latency. The file is
specified by file name, and the file type is system dependent.

Note that we support 100 synth files loaded, but the Petit-ami "rule of thumb"
is no more than 10 synth files at a time.

*******************************************************************************/

static byte readbyt(FILE* fh)

{

    byte b;
    int v;

    v = getc(fh);
    if (v == EOF) error("Invalid .mid file format");
    b = v;
/* All but the header reads call this routine, so uncommenting this print will
   give a good diagnostic for data reads */
/*
printf("@%ld: byte: %2.2x\n", ftell(fh), b);
*/

    return (b);

}

static int readvar(FILE* fh, unsigned int* v)

{

    byte b;
    int cnt;

    *v = 0;
    cnt = 0;
    do {

        b = readbyt(fh); /* get next part */
        cnt++;
        *v = *v<<7|(b&0x7f); /* shift and place new value */

    } while (b >= 128);

    return (cnt);

}

static void prttxt(FILE* fh, unsigned int len)

{

    byte b;

    while (len) {

        b = readbyt(fh);
        putchar(b);
        len--;

    }

}

static int dcdmidi(FILE* fh, byte b, boolean* endtrk, int p, int t, int* qnote,
                   seqptr* rsp)

{

    byte p1;
    byte p2;
    byte p3;
    byte p4;
    byte p5;
    unsigned len;
    int cnt;
    seqptr sp;

    cnt = 0;
    *endtrk = false;
    sp = NULL; /* clear sequencer entry */
    switch (b>>4) { /* command nybble */

        case 0x8: /* note off */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  getseq(&sp); /* get sequencer entry */
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_noteoff; /* set type */
                  sp->ntc = (b&15)+1; /* set channel */
                  sp->ntn = p1+1; /* set note */
                  sp->ntv = p2*0x01000000; /* set velocity */
                  break;
        case 0x9: /* note on */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  getseq(&sp); /* get a sequencer message */
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_noteon; /* set type */
                  sp->ntc = (b&15)+1; /* set channel */
                  sp->ntn = p1+1; /* set note */
                  sp->ntv = p2*0x01000000; /* set velocity */
                  break;
        case 0xa: /* polyphonic key pressure */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  getseq(&sp); /* get a sequencer message */
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_aftertouch; /* set type */
                  sp->ntc = (b&15)+1; /* set channel */
                  sp->ntn = p1+1; /* set note */
                  sp->ntv = p2*0x01000000; /* set aftertouch */
                  break;
        case 0xb: /* controller change/channel mode */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  switch (p1) { /* channel mode messages */

                      /* note we don't implement all controller messages */
                      case CTLR_SOUND_ATTACK_TIME:
                      case CTLR_SOUND_RELEASE_TIME:
                      case CTLR_LEGATO_PEDAL:
                      case CTLR_PORTAMENTO:
                      case CTLR_VOLUME_COARSE:
                      case CTLR_VOLUME_FINE:
                      case CTLR_BALANCE_COARSE:
                      case CTLR_BALANCE_FINE:
                      case CTLR_PORTAMENTO_TIME_COARSE:
                      case CTLR_PORTAMENTO_TIME_FINE:
                      case CTLR_MODULATION_WHEEL_COARSE:
                      case CTLR_MODULATION_WHEEL_FINE:
                      case CTLR_PAN_POSITION_COARSE:
                      case CTLR_PAN_POSITION_FINE:
                      case CTLR_SOUND_TIMBRE:
                      case CTLR_SOUND_BRIGHTNESS:
                      case CTLR_EFFECTS_LEVEL:
                      case CTLR_TREMULO_LEVEL:
                      case CTLR_CHORUS_LEVEL:
                      case CTLR_CELESTE_LEVEL:
                      case CTLR_PHASER_LEVEL:
                      case CTLR_REGISTERED_PARAMETER_COARSE:
                      case CTLR_REGISTERED_PARAMETER_FINE:
                      case CTLR_DATA_ENTRY_COARSE:
                      case CTLR_DATA_ENTRY_FINE:
                                 break;

                      case CTLR_MONO_OPERATION: /* Mono mode on */
                                 getseq(&sp); /* get a sequencer message */
                                 sp->port = p; /* set port */
                                 sp->time = t; /* set time */
                                 sp->st = st_mono; /* set type */
                                 sp->vsc = (b&15)+1; /* set channel */
                                 sp->vsv = p2; /* set mono mode */
                                 break;
                      case CTLR_POLY_OPERATION: /* Poly mode on */
                                 getseq(&sp); /* get a sequencer message */
                                 sp->port = p; /* set port */
                                 sp->time = t; /* set time */
                                 sp->st = st_poly; /* set type */
                                 sp->pc = (b&15)+1; /* set channel */
                                 break;

                  }
                  break;
        case 0xc: /* program change */
                  p1 = readbyt(fh); cnt++;
                  getseq(&sp); /* get a sequencer message */
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_instchange; /* set type */
                  sp->icc = (b&15)+1; /* set channel */
                  sp->ici = p1+1; /* set instrument */
                  break;
        case 0xd: /* channel key pressure */
                  p1 = readbyt(fh); cnt++;
                  getseq(&sp); /* get a sequencer message */
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_pressure; /* set type */
                  sp->ntc = (b&15)+1; /* set channel */
                  sp->ntv = p1*0x01000000; /* set pressure */
                  break;
        case 0xe: /* pitch bend */
                  p1 = readbyt(fh); cnt++;
                  p2 = readbyt(fh); cnt++;
                  getseq(&sp); /* get a sequencer message */
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_pitch; /* set type */
                  sp->vsc = (b&15)+1; /* set channel */
                  sp->vsv = p2<<7|p1; /* set pitch */
                  break;
        case 0xf: /* sysex/meta */
                  switch (b) {

                      case 0xf0: /* F0 sysex event */
                      case 0xf7: /* F7 sysex event */
                                 cnt += readvar(fh, &len); /* get length */
                                 fseek(fh, len, SEEK_CUR);
                                 break;
                      case 0xff: /* meta events */
                                 p1 = readbyt(fh);  cnt++; /* get next byte */
                                 cnt += readvar(fh, &len); /* get length */
                                 switch (p1) {

                                     case 0x2f: /* End of track */
                                                if (len != 0) error("Meta event length does not match");
                                                *endtrk = true; /* set end of track */
                                                break;
                                     case 0x51: /* Set tempo */
                                                if (len != 3) error("Meta event length does not match");
                                                p1 = readbyt(fh); cnt++; /* get b1-b3 */
                                                p2 = readbyt(fh); cnt++;
                                                p3 = readbyt(fh); cnt++;
                                                *qnote = (p1<<16|p2<<8|p3)/100;
                                                break;
                                     default:   /* unknown/other */
                                                fseek(fh, len, SEEK_CUR);
                                                break;

                                 }
                                 break;

                      default: error("Invalid .mid file format");

                  }
                  break;

        default: error("Invalid .mid file format");

    }
    *rsp = sp; /* return sequencer entry (or NULL if none) */

    return (cnt);

}

unsigned int read16be(FILE* fh)

{

    byte b;
    unsigned int v;

    v = readbyt(fh) << 8;
    v |= readbyt(fh);

    return (v);

}

unsigned int read32be(FILE* fh)

{

    byte b;
    unsigned int v;

    v = readbyt(fh) << 24;
    v |= readbyt(fh) << 16;
    v |= readbyt(fh) << 8;
    v |= readbyt(fh);

    return (v);

}

unsigned int str2id(string ids)

{

    return (ids[0]<<24|ids[1]<<16|ids[2]<<8|ids[3]);

}

void prthid(FILE* fh, unsigned int id)

{

    putc(id >>24 & 0xff, fh);
    putc(id >>16 & 0xff, fh);
    putc(id >>8 & 0xff, fh);
    putc(id & 0xff, fh);

}

void pa_loadsynth(int s, string fn)

{

    FILE*             fh;           /* file handle */
    unsigned int      rem;          /* remaining track length */
    unsigned int      len;          /* length read */
    unsigned int      hlen;         /* header length */
    unsigned int      delta_time;   /* delta time */
    boolean           endtrk;       /* end of track flag */
    byte              last;         /* last command */
    unsigned short    fmt;          /* format code */
    unsigned short    tracks;       /* number of tracks */
    unsigned short    division;     /* delta time */
    int               found;        /* found our header */
    unsigned int      id;           /* id */
    int               curtim;       /* current time in 100us */
    int               qnote;        /* number of 100us/quarter note */
    seqptr            sp, sp2, sp3; /* sequencer entry */
    seqptr            seqlst;       /* sorted sequencer list */
    seqptr            trklst;       /* sorted track list */
    seqptr            trklas;       /* track last entry */
    byte              b;
    int               i;
    int               r;

    if (s < 1 || s > MAXMIDT) error("Invalid logical synthesize file number");

    fh = fopen(fn, "r");
    if (!fh) error("Cannot open input .mid file");
    id = read32be(fh); /* get header id */

    /* check RIFF prefix */
    if (id == str2id("RIFF")) {

        len = read32be(fh); /* read and discard length */
        /* a RIFF file can contain a MIDI file, so we search within it */
        id = read32be(fh); /* get header id */
        if (id != str2id("RMID")) error("Invalid .mid file header");
        do {

            id = read32be(fh); /* get next id */
            len = read32be(fh); /* get length */
            found = id == str2id("data"); /* check data header */
            if (!found) fseek(fh, len, SEEK_CUR);

        } while (!found); /* not SMF header */
        id = read32be(fh); /* get next id */

    }

    /* check a midi file header */
    if (id != str2id("MThd")) error("Invalid .mid file header");

    /* read the rest of the file header */
    hlen = read32be(fh); /* get length */
    fmt = read16be(fh); /* format */
    tracks = read16be(fh); /* tracks */
    division = read16be(fh); /* delta time */

    /* check and reject SMTPE framing */
    if (division & 0x80000000) error("Cannot handle SMTPE framing");

    /*
     * Read the midi file in and construct a sorted sequencer list.
     */
    qnote = DEFMIDITIM; /* set default quarter note time */
    seqlst = NULL; /* clear the master sequencer list */
    for (i = 0; i < tracks && !feof(fh); i++) { /* read track chunks */

        curtim = 0; /* set zero offset time */
        trklst = NULL; /* clear track sequencer list */
        trklas = NULL; /* clear track last sequencer entry */
        if (!feof(fh)) { /* not eof */

            /* Read in .mid track header */
            id = read32be(fh); /* get header id */
            hlen = read32be(fh); /* get length */

            /* check a midi header */
            if (id == str2id("MTrk")) {

                rem = hlen; /* set remainder to parse */
                last = 0; /* clear last command */
                do {

                    len = readvar(fh, &delta_time); /* get delta time */
                    rem -= len; /* count */
                    curtim += delta_time*qnote/division; /* advance with delta time */
                    b = getc(fh); /* get the command byte */
                    rem--; /* count */
                    if (b < 0x80) { /* process running status or repeat */

                        ungetc(b, fh); /* put back parameter byte */
                        rem++; /* back out count */
                        b = last; /* put back command for repeat */

                    }
                    /* decode midi instruction. Note that the output port is
                       irrelivant, since the actual port will be specified at
                       play time */
                    len = dcdmidi(fh, b, &endtrk, PA_SYNTH_OUT, curtim, &qnote, &sp);
                    rem -= len; /* count */
                    if (sp) { /* a sequencer entry was produced */

                        /* note the track list is inherently constructed in time
                           order */
                        sp->next = NULL; /* clear next */
                        if (!trklas) trklst = sp; /* list empty, place first */
                        else trklas->next = sp; /* link last to this */
                        trklas = sp; /* set new last */
                    }
                    /* if command is not meta, save as last command */
                    if (b < 0xf0) last = b;

                } while (rem && !endtrk); /* until run out of commands */
                /* merge track list with master list */
                if (trklst) {

                    sp2 = seqlst; /* save master list */
                    seqlst = NULL; /* clear new list */
                    sp3 = NULL; /* clear last */
                    while (sp2 && trklst) { /* while both lists have content */

                        if (sp2->time <= trklst->time) {

                            /* take from master */
                            sp = sp2; /* remove top entry */
                            sp2 = sp2->next;

                        } else {

                            /* take from track */
                            sp = trklst; /* remove top entry */
                            trklst = sp->next;

                        }
                        sp->next = NULL; /* clear next */
                        /* insert to new list */
                        if (sp3) sp3->next = sp; /* link last to this */
                        else seqlst = sp; /* set first entry */
                        sp3 = sp; /* set new last */

                    }
                    /* dump rest of master list */
                    while (sp2) {

                        /* take from master */
                        sp = sp2; /* remove top entry */
                        sp2 = sp2->next;
                        sp->next = NULL; /* clear next */
                        /* insert to new list */
                        if (sp3) sp3->next = sp; /* link last to this */
                        else seqlst = sp; /* set first entry */
                        sp3 = sp; /* set new last */

                    }
                    /* dump rest of track list */
                    while (trklst) {

                        /* take from track */
                        sp = trklst; /* remove top entry */
                        trklst = sp->next;
                        /* insert to new list */
                        if (sp3) sp3->next = sp; /* link last to this */
                        else seqlst = sp; /* set first entry */
                        sp3 = sp; /* set new last */

                    }

                }

            } else fseek(fh, hlen, SEEK_CUR);

        }

    }
    /* place completed synth list into the logical table */
    pthread_mutex_lock(&synlck); /* take wave table lock */
    sp = syntab[s]; /* get existing entry */
    syntab[s] = seqlst; /* place new */
    pthread_mutex_unlock(&synlck); /* release lock */
    if (sp) error("Wave file already defined for logical wave number");

}

/*******************************************************************************

Delete synthesizer file

Removes a synthesizer file from the caching table. This frees up the entry to be
redefined.

Attempting to delete a synthesizer entry that is actively being played will
result in this routine blocking until it is complete.

*******************************************************************************/

void pa_delsynth(int s)

{

    seqptr  p;
    int     n;
    boolean accessed;

    if (s < 1 || s > MAXMIDT) error("Invalid logical synth file number");

    /* check a synth for that file is active */
    do {

        /* this is a double lock like your mother warned you about. It is
           structured two deep and warrants analysis. */
        accessed = false; /* set no successful access */
        pthread_mutex_lock(&synlck); /* take synth table lock */

        pthread_mutex_lock(&snmlck);
        n = numsql[s]++;
        pthread_mutex_unlock(&snmlck); /* release lock */

        if (!n) { /* no active synths, proceed to delete */

            p = syntab[s]; /* get the existing entry */
            syntab[s] = NULL; /* clear original */
            accessed = true; /* set sucessful access */

        }
        pthread_mutex_unlock(&synlck); /* release lock */

    } while (!accessed); /* until we get a successful access */
    if (!p) error("No synth file loaded for logical wave number");
    putseqlst(p); /* free the sequencer list */

}

/*******************************************************************************

Play synthesiser file

Plays the waveform file to the indicated midi device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows midi files to be sequenced against other wave files and midi files.
The file is specified by file name, and the file type is system dependent.
This version uses the string send MCI command, one of the simpliest ways to do
that. There does not seem to be an obvious way to relate the MCI devices to the
midi devices.

Windows cannot play more than one midi file at a time (although it can layer
one wave with one midi). Also, a midi open/close sequence like we use here will
fail if the default synth is open. We handle this by closing the default if
it is open, then reopening it afterwards.

*******************************************************************************/

void pa_playsynth(int p, int t, int s)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXMIDP) error("Invalid synthesizer port");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_playsynth; /* set type */
    sp->sid = s; /* place synth file id */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Wait synthesizers complete

Waits for all running sequencers to complete before returning.
The synthesizers all play on a separate thread. Normally, if the parent program
exits before the threads all complete, the synth plays stop, and this is usually
the correct behavior. However, in some cases we want the synth sequencers to
complete. for example a "play mymidi.mid" command should wait until the synth
sequencers finish.

This routine waits until ALL synth operations complete. There is no way to
determine indivudual sequencer completions, since even the same track could have
mutiple plays active at the same time. So we keep a counter of active plays, and
wait until they all stop.

The active sequencer count includes all sequencers, including syth (midi) file
plays and individual notes/events sent to the midi "manual" interface. Thus this
call only makes sense if the calling program has stopped sending events to the
sequencer(s), including background tasks.

*******************************************************************************/

void pa_waitsynth(int p)

{

    if (p < 1 || p > MAXMIDP) error("Invalid synthesizer port");
    if (!alsamidiout[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthsizer port not open");

    pthread_mutex_lock(&snmlck); /* lock counter */
    pthread_cond_wait(&snmzer, &snmlck); /* wait zero event */
    pthread_mutex_unlock(&snmlck); /* release lock */

}

/*******************************************************************************

Find number of output wave devices.

Returns the number of wave output devices available.

*******************************************************************************/

int pa_waveout(void)

{

    return (alsapcmoutnum);

}

/*******************************************************************************

Find number of input wave devices.

Returns the number of wave output devices available.

*******************************************************************************/

int pa_wavein(void)

{

    return (alsapcminnum);

}

/*******************************************************************************

Open wave output device

Opens a wave output device by number. By convention, wave out 1 is the default
output device. This is presently a no-op for linux.

*******************************************************************************/

void pa_openwaveout(int p)

{

    int r;

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1]) error("No wave output device defined at logical number");
    if (alsapcmout[p-1]->devopn) error("Wave port already open");

    r = snd_pcm_open(&alsapcmout[p-1]->pcm, alsapcmout[p-1]->name,
                     SND_PCM_STREAM_PLAYBACK, 0);
    if (r < 0) alsaerror(r);
    alsapcmout[p-1]->devopn = true; /* set open */
    alsapcmout[p-1]->fmt = -1; /* set format undefined until read/write time */

}

/*******************************************************************************

CLose wave output device

Closes a wave output device by number. This is presently a no-op for linux.

*******************************************************************************/

void pa_closewaveout(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p]) error("No wave output device defined at logical number");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

    snd_pcm_close(alsapcmout[p-1]->pcm);
    alsapcmout[p-1]->devopn = false; /* set closed */


}

/*******************************************************************************

Play ALSA sound file

Plays the given ALSA sound file given the logical wave track file number.
Accepts a limited number of formats for the .wav file. A format header must
appear after the initial chunk, and we don't accept further format changes.

*******************************************************************************/

static void* alsaplaywave(void* data)

{

    wavhdr            whd;    /* .wav file header */
    fmthdr            fhd;    /* fmt chunk header */
    cnkhdr            chd;    /* data chunk header */
    snd_pcm_t*        pdh;    /* playback device handle */
    snd_pcm_format_t  fmt;    /* PCM format */
    snd_pcm_uframes_t frmsiz; /* frame size, sample bits*channels */
    unsigned int      remsiz; /* remaining transfer bytes */
    snd_pcm_uframes_t remfrm; /* remaining frames to transfer */
    unsigned int      xfrsiz; /* partial transfer size */
    snd_pcm_uframes_t frmbuf; /* frames in buffer */
    portidptr         pip;    /* pointer for data we need */
    int               w;      /* wave file instance */
    int               p;      /* port */
    int               fh;
    int               r;
    int               len;
    byte              buff[WAVBUF]; /* buffer for frames */
    int               i;
    byte*             buffp;
    char              fn[MAXFIL];

    pip = (portidptr) data; /* get data pointer */
    w = pip->id; /* get id */
    p = pip->port; /* get port */

    /* Have to be very careful about wavetable access, we won't call external
       routines with the lock on. We are read access, the writer is the
       delwave() routine. We have to get the filename while locked, copy it
       out of the table to fixed string (can't call malloc), then check we
       actually got it outside the lock */
    pthread_mutex_lock(&wavlck); /* take wave table lock */
    fn[0] = 0; /* terminate wave table entry */
    if (wavfil[w]) strncpy(fn, wavfil[w], MAXFIL);
    pthread_mutex_unlock(&wavlck); /* release lock */
    if (!fn[0]) return (NULL); /* entry is empty, abort */

    /* count active wave instances */
    pthread_mutex_lock(&wnmlck);
    numwav++; /* count active */
    pthread_mutex_unlock(&wnmlck); /* release lock */

    fh = open(fn, O_RDONLY);
    if (fh < 0) error("Cannot open input .wav file");

    /* Read in IFF File header */
    len = read(fh, &whd, sizeof(wavhdr));
    if (len != sizeof(wavhdr)) error(".wav file format");

    /* check a RIFF header with WAVE type */
    if (strncmp("RIFF", whd.id, 4) || strncmp("WAVE", whd.type, 4))
       error("Not a valid .wav file");

    /* read in fmt header. Note we expect that at the top of the file, and that
       there is only one such header. */
    len = read(fh, &fhd, sizeof(fmthdr));
    if (len != sizeof(fmthdr)) error(".wav file format");

    /* check a format header */
    if (strncmp("fmt ", fhd.id, 4)) error("Not a valid .wav file");

    r = snd_pcm_open(&pdh, alsapcmout[p-1]->name, SND_PCM_STREAM_PLAYBACK, 0);
    if (r < 0) error("Cannot open audio output device");

    /* set PCM format */
    switch (fhd.bitspersample) {

        case 8:  fmt = SND_PCM_FORMAT_U8;  break;
        case 16: fmt = SND_PCM_FORMAT_S16; break;
        case 24: fmt = SND_PCM_FORMAT_S24; break;
        case 32: fmt = SND_PCM_FORMAT_S32; break;
        default: error("Cannot play this PCM format");

    }
    r = snd_pcm_set_params(pdh, fmt, SND_PCM_ACCESS_RW_INTERLEAVED,
                           fhd.channels, fhd.samplerate, 1, 500000);
    if (r < 0) error("Cannot set sound parameters");

    /* calculate frame size, or minimum transfer length in bytes */
    frmsiz = fhd.blockalign*fhd.channels;
    frmbuf = WAVBUF/frmsiz; /* find number of frames in a buffer */

    /* read data chunks */
    do {

        len = read(fh, &chd, sizeof(cnkhdr));
        if (len && len != sizeof(cnkhdr)) error(".wav file format");
        if (len) { /* not EOF */

            if (!strncmp("data", chd.id, 4)) {

                /* is a "data" chunk, play */
                remsiz = chd.len; /* set remaining transfer length */
                while (remsiz) {

                    /* find number of frames remaining */
                    remfrm = remsiz/frmsiz;
                    if (remfrm > frmbuf) remfrm = frmbuf;
                    xfrsiz = remfrm*frmsiz; /* find bytes to transfer */
                    len = read(fh, buff, xfrsiz);
                    if (len != xfrsiz) error(".wav file format");
                    r = snd_pcm_writei(pdh, &buff, remfrm);
                    // If an error, try to recover from it
                    if (r < 0) r = snd_pcm_recover(pdh, r, 0);
                    if (r < 0) error("Cannot play .wav file");
                    remsiz -= xfrsiz; /* find remaining total size */

                }

            } else {

                /* skip unrecognized chunk */
                if (chd.len & 1) lseek(fh, chd.len+1, SEEK_CUR);
                else lseek(fh, chd.len, SEEK_CUR);

            }

        }

    } while (len); /* not EOF */

    snd_pcm_close(pdh); /* close playback device */
    close(fh); /* close input file */

    /* count active wave instances */
    pthread_mutex_lock(&wnmlck);
    numwav--; /* count active */
    /* if now zero, signal zero crossing */
    if (!numwav) pthread_cond_signal(&wnmzer);
    pthread_mutex_unlock(&wnmlck); /* release lock */
    if (numwav < 0) error("Wave locking imbalance");
    free(pip); /* release data pointer */

    return (NULL);

}

/*******************************************************************************

Play ALSA sound file kickoff routine

Plays an ALSA .wav file. This is the kickoff routine. We play .wav files on a
thread, and this routine spawns a thread to accomplish that. The thread is
"set and forget", in that it self terminates when the play routine terminates.

*******************************************************************************/

static void alsaplaywave_kickoff(int p, int w)

{

    pthread_t tid; /* thread id (unused) */
    portidptr pip; /* port/id structure */

    pip = malloc(sizeof(portid)); /* get a port/id structure */
    pip->port = p; /* place port */
    pip->id = w; /* place synth file id */
    pthread_create(&tid, NULL, alsaplaywave, (void*)pip);

}

/*******************************************************************************

Load waveform file

Loads a waveform file to a logical cache, from 1 to N. These are loaded up into
memory for minimum latency. The file is specified by file name, and the file
type is system dependent.

Note that we support 100 wave files loaded, but the Petit-ami "rule of thumb"
is no more than 10 wave files at a time.

Note that at present, we don't implement wave caching. This is mainly because on
the test system, the latency to play is acceptable.

*******************************************************************************/

void pa_loadwave(int w, string fn)

{

    string p, p2;

    if (w < 1 || w > MAXWAVT) error("Invalid logical wave number");

    p = malloc(strlen(fn)+1); /* allocate filename for slot */
    if (!p) error("Could not alocate wave file");
    strcpy(p, fn); /* place filename */
    pthread_mutex_lock(&wavlck); /* take wave table lock */
    p2 = wavfil[w]; /* get existing entry */
    wavfil[w] = p; /* place new */
    pthread_mutex_unlock(&wavlck); /* release lock */
    if (p2) error("Wave file already defined for logical wave number");

}

/*******************************************************************************

Delete waveform file

Removes a waveform file from the caching table. This frees up the entry to be
redefined.

*******************************************************************************/

void pa_delwave(int w)

{

    string p;

    if (w < 1 || w > MAXWAVT) error("Invalid logical wave number");

    pthread_mutex_lock(&wavlck); /* take wave table lock */
    p = wavfil[w]; /* get the existing entry */
    wavfil[w] = NULL; /* clear original */
    pthread_mutex_unlock(&wavlck); /* release lock */
    if (!p) error("No wave file loaded for logical wave number");
    free(p); /* free the entry */

}

/*******************************************************************************

Play waveform file

Plays the waveform file to the indicated wave device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows wave files to be sequenced against other wave files and midi files.
The file is specified by file name, and the file type is system dependent.

*******************************************************************************/

void pa_playwave(int p, int t, int w)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (p < 1 || p > MAXWAVP) error("Invalid wave port");
    if (!alsapcmout[p-1]) error("No wave device defined for logical port");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");
    if (w < 1 || w > MAXWAVT) error("Invalid logical wave number");
    if (!wavfil[w]) error("No wave file loaded for logical wave number");

    /* create sequencer entry */
    getseq(&sp); /* get a sequencer message */
    sp->port = p; /* set port */
    sp->time = t; /* set time */
    sp->st = st_playwave; /* set type */
    sp->wt = w; /* set logical track */

    elap = timediff(&strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        wrtseq(sp); /* execute */
        putseq(sp); /* free */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Adjust waveform volume

Adjusts the volume on waveform playback. The volume value is from 0 to maxint.

Not implemented at present.

*******************************************************************************/

void pa_volwave(int p, int t, int v)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p]) error("No wave output device defined at logical number");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

}

/*******************************************************************************

Wait waves complete

Waits for all pending wave out operations to complete before returning.
Wavefiles all play on a separate thread. Normally, if the parent program exits
before the threads all complete, the wave plays stop, and this is usually the
correct behavior. However, in some cases we want the wave to complete. for
example a "play mywave.wav" command should wait until the wave finishes.

This routine waits until ALL wave operations complete. There is no way to
determine indivudual wave completions, since even the same track could have
mutiple plays active at the same time. So we keep a counter of active plays, and
wait until they all stop.

*******************************************************************************/

void pa_waitwave(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave port");
    if (!alsapcmout[p-1]) error("No wave device defined for logical port");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

    pthread_mutex_lock(&wnmlck); /* lock counter */
    pthread_cond_wait(&wnmzer, &wnmlck); /* wait zero event */
    pthread_mutex_unlock(&wnmlck); /* release lock */

}

/*******************************************************************************

Set the number of channels for a wave output device

The given port will have its channel number set from the provided number. It
must be a wave output port, and it must be open. Wave samples are always output
in 24 bit samples, and the channels are interleaved. This means that the
presentation will have channel 1 first, followed by channel 2, etc, then repeat
for the next sample.

*******************************************************************************/

void pa_chanwaveout(int p, int c)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1]) error("No wave output device defined at logical number");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

    alsapcmout[p-1]->chan = c;
    alsapcmout[p-1]->fmt = -1; /* set format undefined until read/write time */

}

/*******************************************************************************

Set the rate for a wave output device

The given port will have its rate set from the provided number, which is the
number of samples per second that will be output. It must be a wave output port,
and it must be open. Output samples are retimed for the rate. No matter how much
data is written, each sample is timed to output at the given rate, buffering as
required.

*******************************************************************************/

void pa_ratewaveout(int p, int r)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1]) error("No wave output device defined at logical number");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

    alsapcmout[p-1]->rate = r;
    alsapcmout[p-1]->fmt = -1; /* set format undefined until read/write time */

}

/*******************************************************************************

Set bit length for output wave device

The given port has its bit length for samples set. Bit lengths are rounded up
to the nearest byte. At the present time, bit numbers that are not divisible by
8 are not supported, but the most likely would be to zero or 1 pad the msbs
depending on signed status, effectively extending the samples. Thus the bit
cound would mainly indicate precision only.

*******************************************************************************/

void pa_lenwaveout(int p, int l)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1]) error("No wave output device defined at logical number");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

    alsapcmout[p-1]->bits = l;
    alsapcmout[p-1]->fmt = -1; /* set format undefined until read/write time */

}

/*******************************************************************************

Set sign of wave output device samples

The given port has its signed/no signed status changed. Note that all floating
point formats are inherently signed.

*******************************************************************************/

void pa_sgnwaveout(int p, boolean s)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1]) error("No wave output device defined at logical number");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

    alsapcmout[p-1]->sgn = s;
    alsapcmout[p-1]->fmt = -1; /* set format undefined until read/write time */

}

/*******************************************************************************

Set floating/non-floating point format

Sets the floating point/integer format for output sound samples.

*******************************************************************************/

void pa_fltwaveout(int p, boolean f)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1]) error("No wave output device defined at logical number");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

    alsapcmout[p-1]->flt = f;
    alsapcmout[p-1]->fmt = -1; /* set format undefined until read/write time */

}

/*******************************************************************************

Set big/little endian format

Sets the big or little endian format for an output wave device. It is possible
that an installation is fixed to the endian format of the host machine, in which
case it is an error to set a format that is different.

*******************************************************************************/

void pa_endwaveout(int p, boolean e)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1]) error("No wave output device defined at logical number");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

    alsapcmout[p-1]->big = e;
    alsapcmout[p-1]->fmt = -1; /* set format undefined until read/write time */

}

/*******************************************************************************

Write wave data output

Writes a buffer of wave data to the given output wave port. The data must be
formatted according to the number of channels. It must be a wave output port,
and it must be open. Wave samples are always output in 24 bit samples, and the
channels are interleaved. This means that the presentation will have channel
1 first, followed by channel 2, etc, then repeat for the next sample. The
samples are converted as required from the 24 bit, big endian samples as
required. The can be up or down converted according to size, and may be
converted to floating point.

This package does not control buffering dept. As much buffering a needed to keep
the data is used without regard for realtime concerns. The nececessary buffering
for real time is implemented by the user of this package, which is generally
recommended to be 1ms or less (64 samples at a 44100 sample rate).

*******************************************************************************/

void pa_wrwave(int p, byte* buff, int len)

{

    int r;
    int bytes;

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1]) error("No wave output device defined at logical number");
    if (!alsapcmout[p-1]->devopn) error("Wave port not open");

    if (alsapcmout[p-1]->fmt == -1) {

        /* if format is not set yet, we set it here */
        alsapcmout[p-1]->fmt = params2alsa(alsapcmout[p-1]);
        r = snd_pcm_set_params(alsapcmout[p-1]->pcm, alsapcmout[p-1]->fmt,
                               SND_PCM_ACCESS_RW_INTERLEAVED, alsapcmout[p-1]->chan,
                               alsapcmout[p-1]->rate, 1, 500000);
        if (r < 0) alsaerror(r);
        /* find size of sample */
        bytes = alsapcmout[p-1]->bits/8; /* find bytes per */
        if (alsapcmout[p-1]->bits&8) bytes++; /* round  up */
        bytes *= alsapcmout[p-1]->chan;
        alsapcmout[p-1]->ssiz = bytes;
        snd_pcm_prepare(alsapcmout[p-1]->pcm);

    }
    r = snd_pcm_writei(alsapcmout[p-1]->pcm, buff, len);
    if (r == -EPIPE) {
        /* uncomment next for a broken pipe diagnostic */
        /* printf("Recovered from output error\n"); */
        snd_pcm_recover(alsapcmin[p-1]->pcm, r, 1);
    } else if (r < 0) alsaerror(r);

}

/*******************************************************************************

Open wave input device

Opens a wave output device by number. By convention, wave in 1 is the default
input device. The wave stream parameters are determined during enumeration,
but we assert them here on open.

*******************************************************************************/

void pa_openwavein(int p)

{

    snd_pcm_hw_params_t *hw_params;
    unsigned int rate;
    int fmt;
    int r;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1]) error("No wave input device defined at logical number");
    if (alsapcmin[p-1]->devopn) error("Wave port already open");

    r = snd_pcm_open(&alsapcmin[p-1]->pcm, alsapcmin[p-1]->name,
                     SND_PCM_STREAM_CAPTURE, 0);
    if (r < 0) alsaerror(r);
    alsapcmin[p-1]->devopn = true; /* set open */

    r = snd_pcm_hw_params_malloc(&hw_params);
    if (r < 0) alsaerror(r);

    /* fill defaults */
    r = snd_pcm_hw_params_any(alsapcmin[p-1]->pcm, hw_params);
    if (r < 0) alsaerror(r);

    r = snd_pcm_hw_params_set_access(alsapcmin[p-1]->pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (r < 0) alsaerror(r);

    fmt = params2alsa(alsapcmin[p-1]); /* find equivalent ALSA format code */
    r = snd_pcm_hw_params_set_format(alsapcmin[p-1]->pcm, hw_params, fmt);
    if (r < 0) alsaerror(r);

    rate = alsapcmin[p-1]->rate;
    r = snd_pcm_hw_params_set_rate_near(alsapcmin[p-1]->pcm, hw_params, &rate, 0);
    if (r < 0) alsaerror(r);

    r = snd_pcm_hw_params_set_channels(alsapcmin[p-1]->pcm, hw_params, alsapcmin[p-1]->chan);
    if (r < 0) alsaerror(r);

    r = snd_pcm_hw_params(alsapcmin[p-1]->pcm, hw_params);
    if (r < 0) alsaerror(r);

    snd_pcm_hw_params_free (hw_params);

    snd_pcm_prepare(alsapcmin[p-1]->pcm);
    if (r < 0) alsaerror(r);

}

/*******************************************************************************

CLose wave input device

Closes a wave input device by number. This is presently a no-op for linux.

*******************************************************************************/

void pa_closewavein(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1]) error("No wave input device defined at logical number");
    if (!alsapcmin[p-1]->devopn) error("Wave port not open");

    snd_pcm_close(alsapcmin[p-1]->pcm); /* close wave device */
    alsapcmin[p-1]->devopn = false; /* set closed */

}

/*******************************************************************************

Get the number of channels for a wave input device

The given port will have its channel number read and returned. It must be a wave
input port, and it must be open. Wave samples are always input in 24 bit
samples, and the channels are interleaved. This means that the presentation will
have channel 1 first, followed by channel 2, etc, then repeat for the next
sample.

*******************************************************************************/

int pa_chanwavein(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1]) error("No wave input device defined at logical number");
    if (!alsapcmin[p-1]->devopn) error("Wave port not open");

    return (alsapcmin[p-1]->chan); /* return channel count */

}

/*******************************************************************************

Get the rate for a wave input device

The given port will have its rate read and returned, which is the
number of samples per second that will be input. It must be a wave output port,
and it must be open. Input samples are timed at the rate.

*******************************************************************************/

int pa_ratewavein(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1]) error("No wave input device defined at logical number");
    if (!alsapcmin[p-1]->devopn) error("Wave port not open");

    return (alsapcmin[p-1]->rate); /* return channel count */

}

/*******************************************************************************

Get the bit length for a wave input device

Returns the number of bits for a given input wave device. To find the number of
bytes required for the format is:

bytes = bits/8;
if (bits%8) bytes++;

This is then multipled by the number of channels to determine the size of a
"sample", or unit of sound data transfer.

At present, only whole byte format samples are supported, IE., 8, 16, 24, 32,
etc. However, the caller should assume that partial bytes can be used and thus
round up bit lengths as shown above.

*******************************************************************************/

int pa_lenwavein(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1]) error("No wave input device defined at logical number");
    if (!alsapcmin[p-1]->devopn) error("Wave port not open");

    return (alsapcmin[p-1]->bits); /* return bit count */

}

/*******************************************************************************

Get signed status of wave input device

Returns true if the given wave input device has signed sampling. Note that
signed sampling is always true if the samples are floating point.

*******************************************************************************/

boolean pa_sgnwavein(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1]) error("No wave input device defined at logical number");
    if (!alsapcmin[p-1]->devopn) error("Wave port not open");

    return (alsapcmin[p-1]->sgn); /* return signed status */

}

/*******************************************************************************

Get big endian status of wave input device

Returns true if the given wave input device has big endian sampling.

*******************************************************************************/

boolean pa_endwavein(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1]) error("No wave input device defined at logical number");
    if (!alsapcmin[p-1]->devopn) error("Wave port not open");

    return (alsapcmin[p-1]->big); /* return big endian status */

}

/*******************************************************************************

Get floating point status of wave input device

Returns true if the given wave input device has floating point sampling.

*******************************************************************************/

boolean pa_fltwavein(int p)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1]) error("No wave input device defined at logical number");
    if (!alsapcmin[p-1]->devopn) error("Wave port not open");

    return (alsapcmin[p-1]->flt); /* return floating point status */

}

/*******************************************************************************

Read wave data input

Reads a buffer of wave data from the given input wave port. The data is
formatted according to the number of channels. It must be a wave input port,
and it must be open. Wave samples are always input in 24 bit samples, and the
channels are interleaved. This means that the presentation will have channel
1 first, followed by channel 2, etc, then repeat for the next sample. The
samples are converted as required from the 24 bit, big endian samples as
required. The can be up or down converted according to size, and may be
converted to floating point.

The input device will be buffered according to the parameters of the input
device and/or the device software. The hardware will have a given buffering
amount, and the driver many change that to be higher or lower. Generally the
buffering will be designed to keep the buffer latency below 1ms (64 samples at a
44100 sample rate). Because the exact sample rate is unknown, the caller is
recommended to provide a buffer that is greater than any possible buffer amount.
Thus (for example) 1024*channels would be an appropriate buffer size. Not
providing enough buffering will not cause an error, but will cause the read
rate to fall behind the data rate.

pa_rdwave() will return the actual number of bytes read, which will contain
3*pa_chanwavein() bytes of samples. This will then be the actual buffer content.

*******************************************************************************/

int pa_rdwave(int p, byte* buff, int len)

{

    int r;

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1]) error("No wave input device defined at logical number");
    if (!alsapcmin[p-1]->devopn) error("Wave port not open");

    do {

        r = snd_pcm_readi(alsapcmin[p-1]->pcm, buff, len);
        if (r < 0) {

            /* uncomment the next to get a channel recovery diagnostic */
            /* fprintf(stderr, "Read data recovered\n"); */
            r = snd_pcm_recover(alsapcmin[p-1]->pcm, r, 1);
            if (r < 0) alsaerror(r);

        }

    } while (!r);

    return (r);

}

/*******************************************************************************

Find device name of synthesizer output port

Returns the ALSA device name of the given synthsizer output port.

*******************************************************************************/

void pa_synthoutname(int p, string name, int len)

{

    if (p < 1 || p > MAXMIDP) error("Invalid MIDI output port");
    if (!alsamidiout[p-1])
        error("No MIDI output device defined at logical number");

    if (strlen(alsamidiout[p-1]->name)+1 > len)
        error("Device name too large for destination");
    strcpy(name, alsamidiout[p-1]->name);

}

/*******************************************************************************

Find device name of synthesizer input port

Returns the ALSA device name of the given synthsizer input port.

*******************************************************************************/

void pa_synthinname(int p, string name, int len)

{

    if (p < 1 || p > MAXMIDP) error("Invalid MIDI input port");
    if (!alsamidiin[p-1])
        error("No MIDI input device defined at logical number");

    if (strlen(alsamidiin[p-1]->name)+1 > len)
        error("Device name too large for destination");
    strcpy(name, alsamidiin[p-1]->name);

}

/*******************************************************************************

Find device name of wave output port

Returns the ALSA device name of the given wave output port.

*******************************************************************************/

void pa_waveoutname(int p, string name, int len)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave output port");
    if (!alsapcmout[p-1])
        error("No wave output device defined at logical number");

    if (strlen(alsapcmout[p-1]->name)+1 > len)
        error("Device name too large for destination");
    strcpy(name, alsapcmout[p-1]->name);

}

/*******************************************************************************

Find device name of wave input port

Returns the ALSA device name of the given wave input port.

*******************************************************************************/

void pa_waveinname(int p, string name, int len)

{

    if (p < 1 || p > MAXWAVP) error("Invalid wave input port");
    if (!alsapcmin[p-1])
        error("No wave input device defined at logical number");

    if (strlen(alsapcmin[p-1]->name)+1 > len)
        error("Device name too large for destination");
    strcpy(name, alsapcmin[p-1]->name);

}

/*******************************************************************************

Open a synthesizer input port.

The given synthesizer port is opened and ready for reading.

*******************************************************************************/

void pa_opensynthin(int p)

{

    int r;

    if (p < 1 || p > MAXMIDP) error("Invalid synthesizer port");
    if (!alsamidiin[p-1]) error("No synthsizer defined for logical port");
    if (alsamidiin[p-1]->devopn) error("Synthsizer port already open");

    r = snd_rawmidi_open(&alsamidiin[p-1]->midi, NULL, alsamidiin[p-1]->name,
                         0);
    if (r < 0) alsaerror(r);
    alsamidiin[p-1]->devopn = true; /* set open */

}

/*******************************************************************************

Close a synthesizer input port

Closes the given synthsizer port for reading.

*******************************************************************************/

void pa_closesynthin(int p)

{

    if (p < 1 || p > MAXMIDP) error("Invalid synthesizer port");
    if (!alsamidiin[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiin[p-1]->devopn) error("Synthsizer port not open");

    snd_rawmidi_close(alsamidiin[p-1]->midi); /* close port */
    alsamidiin[p-1]->devopn = false; /* set closed */

}

/*******************************************************************************

Write synthesizer port

Writes a given sequencer instruction entry the given output port. This is the
same effect as the "constructor" entries, but means the user won't have to
decode streaming MIDI devices.

Note that if the entries are sequenced (time not 0) they will be entered into
the sequencer list.

The port numbers in the input are ignored and replaced with the port given as
a parameter.

*******************************************************************************/

void pa_wrsynth(int p, seqptr sp)

{

    seqptr spp;

    if (p < 1 || p > MAXMIDP) error("Invalid synthesizer port");
    if (!alsamidiout[p-1]) error("No synthesizer defined for logical port");
    if (!alsamidiout[p-1]->devopn) error("Synthesizer port not open");

    if (sp->time) {

        /* entry to be sequenced */
        if (!seqrun) error("Sequencer not running");
        getseq(&spp); /* get a sequencer message */
        /* make a copy of the command record */
        memcpy(spp, sp, sizeof(seqmsg));
        spp->port = p; /* override the port number */
        insseq(spp); /* insert to sequencer list */
        acttim(sp->time); /* kick timer if needed */

    } else  wrtseq(sp); /* execute the synth note */

}

/*******************************************************************************

Read synthesizer port

Reads and parses a midi instruction record from the given input port. ALSA midi
input ports are assumed to be "time delimited", that is, input terminates if the
input device takes to long to deliver the next byte. This makes the input self
syncronising. This property will exist even if the traffic is burstly, IE.,
since we parse a MIDI instruction and stop, we will keep sync as long as we
can properly decode MIDI instructions.

We return the next midi instruction as a sequencer record. The port is as the
input, which is not useful except to document where the instruction came from.
If a start time is set for the input, then the input is timestamped, otherwise
the time will be set to 0 or "unsequenced".

Note that this routine will read meta-instructions, but skip them. It is not
a full MIDI decoder.

*******************************************************************************/

byte rdsynth(devptr mp)

{

    byte b;
    int r;

    if (mp->pback != -1) {

        /* get a pushback character */
        b = mp->pback;
        mp->pback = -1; /* set none */

    } else {

        r = snd_rawmidi_read(mp->midi, &b, 1);
        if (r < 0) alsaerror(r);
        /* this should never happen */
        if (r != 1) error("Error reading midi file");

    }

    return b;

}

static void rdsynthvar(devptr mp, unsigned int* v)

{

    byte b;
    int cnt;

    *v = 0;
    do {

        b = rdsynth(mp); /* get next part */
        *v = *v<<7|(b&0x7f); /* shift and place new value */

    } while (b >= 128);

}

void pa_rdsynth(int p, seqptr sp)

{

    devptr mp; /* midi input port */
    byte b;
    int t;
    byte p1;
    byte p2;
    byte p3;
    byte p4;
    byte p5;
    unsigned len;
    int i;

    if (p < 1 || p > MAXMIDP) error("Invalid synthesizer port");
    if (!alsamidiin[p-1]) error("No synthsizer defined for logical port");
    if (!alsamidiin[p-1]->devopn) error("Synthesizer port not open");

    t = 0; /* set no time */
    if (sinrun) t = (timediff(&sintim)); /* mark with current time */
    mp = alsamidiin[p-1];
    b = rdsynth(mp);
    if (b < 0x80) { /* process running status or repeat */

        mp->pback = b; /* put back parameter byte */
        b = mp->last; /* put back command for repeat */

    }
    switch (b>>4) { /* command nybble */

        case 0x8: /* note off */
                  p1 = rdsynth(mp);
                  p2 = rdsynth(mp);
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_noteoff; /* set type */
                  sp->ntc = (b&15)+1; /* set channel */
                  sp->ntn = p1+1; /* set note */
                  sp->ntv = p2*0x01000000; /* set velocity */
                  break;
        case 0x9: /* note on */
                  p1 = rdsynth(mp);
                  p2 = rdsynth(mp);
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_noteon; /* set type */
                  sp->ntc = (b&15)+1; /* set channel */
                  sp->ntn = p1+1; /* set note */
                  sp->ntv = p2*0x01000000; /* set velocity */
                  break;
        case 0xa: /* polyphonic key pressure */
                  p1 = rdsynth(mp);
                  p2 = rdsynth(mp);
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_aftertouch; /* set type */
                  sp->ntc = (b&15)+1; /* set channel */
                  sp->ntn = p1+1; /* set note */
                  sp->ntv = p2*0x01000000; /* set aftertouch */
                  break;
        case 0xb: /* controller change/channel mode */
                  p1 = rdsynth(mp);
                  p2 = rdsynth(mp);
                  switch (p1) { /* channel mode messages */

                      /* note we don't implement all controller messages */
                      case CTLR_SOUND_ATTACK_TIME:
                      case CTLR_SOUND_RELEASE_TIME:
                      case CTLR_LEGATO_PEDAL:
                      case CTLR_PORTAMENTO:
                      case CTLR_VOLUME_COARSE:
                      case CTLR_VOLUME_FINE:
                      case CTLR_BALANCE_COARSE:
                      case CTLR_BALANCE_FINE:
                      case CTLR_PORTAMENTO_TIME_COARSE:
                      case CTLR_PORTAMENTO_TIME_FINE:
                      case CTLR_MODULATION_WHEEL_COARSE:
                      case CTLR_MODULATION_WHEEL_FINE:
                      case CTLR_PAN_POSITION_COARSE:
                      case CTLR_PAN_POSITION_FINE:
                      case CTLR_SOUND_TIMBRE:
                      case CTLR_SOUND_BRIGHTNESS:
                      case CTLR_EFFECTS_LEVEL:
                      case CTLR_TREMULO_LEVEL:
                      case CTLR_CHORUS_LEVEL:
                      case CTLR_CELESTE_LEVEL:
                      case CTLR_PHASER_LEVEL:
                      case CTLR_REGISTERED_PARAMETER_COARSE:
                      case CTLR_REGISTERED_PARAMETER_FINE:
                      case CTLR_DATA_ENTRY_COARSE:
                      case CTLR_DATA_ENTRY_FINE:
                                 break;

                      case CTLR_MONO_OPERATION: /* Mono mode on */
                                 sp->port = p; /* set port */
                                 sp->time = t; /* set time */
                                 sp->st = st_mono; /* set type */
                                 sp->vsc = (b&15)+1; /* set channel */
                                 sp->vsv = p2; /* set mono mode */
                                 break;
                      case CTLR_POLY_OPERATION: /* Poly mode on */
                                 sp->port = p; /* set port */
                                 sp->time = t; /* set time */
                                 sp->st = st_poly; /* set type */
                                 sp->pc = (b&15)+1; /* set channel */
                                 break;

                  }
                  break;
        case 0xc: /* program change */
                  p1 = rdsynth(mp);
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_instchange; /* set type */
                  sp->icc = (b&15)+1; /* set channel */
                  sp->ici = p1+1; /* set instrument */
                  break;
        case 0xd: /* channel key pressure */
                  p1 = rdsynth(mp);
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_pressure; /* set type */
                  sp->ntc = (b&15)+1; /* set channel */
                  sp->ntv = p1*0x01000000; /* set pressure */
                  break;
        case 0xe: /* pitch bend */
                  p1 = rdsynth(mp);
                  p2 = rdsynth(mp);
                  sp->port = p; /* set port */
                  sp->time = t; /* set time */
                  sp->st = st_pitch; /* set type */
                  sp->vsc = (b&15)+1; /* set channel */
                  sp->vsv = p2<<7|p1; /* set pitch */
                  break;
        case 0xf: /* sysex/meta */
                  switch (b) {

                      case 0xf0: /* F0 sysex event */
                      case 0xf7: /* F7 sysex event */
                                 rdsynthvar(mp, &len); /* get length */
                                 /* skip instruction */
                                 for (i = 0; i < len; i++) rdsynth(mp);
                                 break;
                      case 0xff: /* meta events */
                                 p1 = rdsynth(mp);  /* get next byte */
                                 rdsynthvar(mp, &len); /* get length */
                                 /* skip instruction */
                                 for (i = 0; i < len; i++) rdsynth(mp);
                                 break;

                      default: error("Invalid .mid file format");

                  }
                  break;

        default: error("Invalid .mid file format");

    }
    /* if command is not meta, save as last command */
    if (b < 0xf0) mp->last = b;
#ifdef SHOWMIDIIN
    dmpseq(sp); /* dump sequencer instruction */
#endif

}

/*******************************************************************************

Find wave channel parameters

Sets the wave channel parameters based on the "preferred" channel parameters.
We find the number of bits, the sign, the big endian mode, if the channel is
floating point.

If the device supports multiple formats, we select the best according to
priorities. These are, in priority order:

1. The largest number of bits.
2. Signed (vs. Unsigned).
3. Floating point (vs. fixed).
4. Endian mode that matches the machine we are on.

The parameters are really for input devices. We assume that the input device
gives the best parameters for its capture, and the output device matches the
input device. On a single card, that will be true by definition. When crossing
hardware from one input to an unrelated device, the output device is assumed
to adapt to the input device. If that is not possible, alsa will provide a
translation layer for the I/O. Although an input device can be so adapted, its
not the best idea, since you could easily be throwing away precision. Thus the
PA rule: the output device adapts to the input source.

Accepts the wave device to find parameters for, and the stream mode (capture
or playback). The name of the device (alsa) must be present.

Returns if the device is supported or not. If not supported, then it should be
removed.

*******************************************************************************/

/* find big or little endian */

static int bigend(void)

{

    union {

        int i;
        unsigned char b;

    } test;

    test.i = 1;

    return (!test.b);

}

static int stderr_file;

static void quiet(void)

{

#ifdef SILENTALSA
    /* run alsa open in quiet */
    fflush(stderr);
    stderr_file = dup(fileno(stderr));
    freopen("/dev/null", "w", stderr);
#endif

}

static void unquiet(void)

{

#ifdef SILENTALSA
    fflush(stderr);
    dup2(stderr_file, fileno(stderr));
    close(stderr_file);
    clearerr(stderr);
#endif

}

static int findparms(devptr dev, snd_pcm_stream_t stream)

{

    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *pars;
    snd_pcm_format_mask_t *fmask;
    unsigned chanmin, chanmax;
    unsigned ratemin, ratemax;
    unsigned bitmin, bitmax;
    int bits;
    int sgn;
    int big;
    int flt;
    int fmt;
    int supp;
    int mbits;
    int msgn;
    int mbig;
    int mflt;
    int msupp;
    int r;
    int stderr_save;

    mbits = 0; msgn = 0; mbig = 0; mflt = 0; msupp = 0;
    quiet();
    r = snd_pcm_open(&pcm, dev->name, stream, SND_PCM_NONBLOCK);
    unquiet();
    if (!r) {

        snd_pcm_hw_params_alloca(&pars);
        r = snd_pcm_hw_params_any(pcm, pars);
        if (r >= 0) {

            snd_pcm_hw_params_get_channels_min(pars, &chanmin);
            snd_pcm_hw_params_get_channels_max(pars, &chanmax);
            /* limit channels to max */
            if (stream == SND_PCM_STREAM_CAPTURE && chanmax > MAXINCHAN)
                dev->chan = MAXINCHAN;
            else dev->chan = chanmax; /* set channels to max */
            snd_pcm_hw_params_get_rate_min(pars, &ratemin, NULL);
            snd_pcm_hw_params_get_rate_max(pars, &ratemax, NULL);
            /* limit rate to max */
            if (stream == SND_PCM_STREAM_CAPTURE && ratemax >= MAXINRATE)
                dev->rate = MAXINRATE;
            else dev->rate = ratemax;
            snd_pcm_format_mask_alloca(&fmask);
            snd_pcm_hw_params_get_format_mask(pars, fmask);
            for (fmt = 0; fmt < SND_PCM_FORMAT_LAST; fmt++) {

                bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
                if (snd_pcm_format_mask_test(fmask, (snd_pcm_format_t)fmt)) {

                    supp = alsa2params(fmt, &bits, &sgn, &big, &flt);
                    /* if input stream and bits > max, we drop this format */
                    if (stream == SND_PCM_STREAM_CAPTURE && bits > MAXINBITS)
                        supp = 0;
                    /* find max format */
                    if (supp) {

                        /* is a PA supported format */
                        if (bits > mbits || flt > mflt) {

                            /* prefer bigger bit size or float */
                            if (sgn > msgn || big == bigend()) {

                                /* prefer signed or big endian matches host */
                                mbits = bits;
                                msgn = sgn;
                                mbig = big;
                                mflt = flt;
                                msupp = supp;

                            }

                        }

                    }

                }

            }

        }
        snd_pcm_close(pcm);

    }
    if (msupp) {

        /* supported device, set up */
        dev->bits = mbits;
        dev->sgn = msgn;
        dev->big = mbig;
        dev->flt = mflt;

    }

    return (msupp);

}

/*******************************************************************************

Read device names

Reads alsa device names into table. Accepts the table base, device type name,
I/O type, and table maximum, and fills the table with as many devices as are
found or fit into the table.

*******************************************************************************/

static void readalsadev(devptr table[], string devt, string iotyp, int tabmax,
                        int* tblcnt)

{

    char** hint;
    char** hi;
    char*  devn;
    char*  iot;
    int    r;
    int    i;
    snd_pcm_stream_t stream;

    /* clear target device table */
    for (i = 0; i < tabmax; i++) table[i] = NULL;
    i = 0; /* set 1st table entry */
    /* Enumerate sound devices */
    r = snd_device_name_hint(-1, devt, (void***)&hint);
    if (r != 0) error("Cannot get device names");
    hi = hint;
    while (*hi != NULL && i < tabmax) {

        /* if table overflows, just keep first entries */
        devn = snd_device_name_get_hint(*hi, "NAME");
        iot = snd_device_name_get_hint(*hi, "IOID");
        /* fix up incorrectly typed ALSA devices (plugins) */
        if (!strncmp(devn, "dmix:", 5)) {

            if (iot) free(iot);
            iot = malloc(7);
            strcpy(iot, "Output");

        }
        if (!strncmp(devn, "dsnoop:", 7)) {

            if (iot) free(iot);
            iot = malloc(7);
            strcpy(iot, "Input");

        }
        if (!strncmp(devn, "front:", 6)) {

            if (iot) free(iot);
            iot = malloc(7);
            strcpy(iot, "Output");

        }
        if (!strncmp(devn, "surround", 8)) {

            if (iot) free(iot);
            iot = malloc(7);
            strcpy(iot, "Output");

        }
        if (!strncmp(devn, "iec958:", 7)) {

            if (iot) free(iot);
            iot = malloc(7);
            strcpy(iot, "Output");

        }
        /* treat null I/O types as universal for now. Alsa returns such
           types for MIDI devices. It seems to indicate "both" */
        if (!iot || !strcmp(iotyp, iot)) {

            table[i] = malloc(sizeof(snddev)); /* create new device entry */
            table[i]->name = devn; /* place name of device */
            table[i]->last = 0; /* clear last byte */
            table[i]->pback = -1; /* set no pushback */
            table[i]->opnseq = openalsamidi; /* set open alsa midi device */
            table[i]->clsseq = closealsamidi; /* set close alsa midi device */
            table[i]->wrseq = _pa_excseq; /* set sequencer execute function */
            table[i]->midi = NULL; /* clear midi handle */
            table[i]->pcm = NULL; /* clear PCM handle */
            table[i]->devopn = false; /* set device not open */
            /* if the device is not midi, get the parameters of wave */
            if (strcmp(devt, "rawmidi")) {

                stream = SND_PCM_STREAM_PLAYBACK; /* set playback type */
                /* set input type if so */
                if (!strcmp(iotyp, "Input")) stream = SND_PCM_STREAM_CAPTURE;
                /* characterize the entry and leave blank if not possible */
                if (!findparms(table[i], stream)) {

                    /* could not characterize, clear entry */
                    free(devn);
                    free(table[i]);
                    table[i] = NULL;

                }

            }

        } else free(devn); /* free up name */
        if (iot) free(iot);
        hi++; /* next hint */
        if (table[i]) i++; /* next device name (if entry defined) */

    }
    /* release hint */
    snd_device_name_free_hint((void**)hint);

    *tblcnt = i; /* return the table count */

}

/*******************************************************************************

Initialize sound module

Clears sequencer lists, flags no timer active, clears the midi output port
table, and initalizes the sequencer task mutex.

*******************************************************************************/

static void pa_init_sound (void) __attribute__((constructor (102)));
static void pa_init_sound()

{

    int    i; /* index for midi tables */

    seqlst = NULL; /* clear active sequencer list */
    seqfre = NULL; /* clear free sequencer messages */
    seqrun = false; /* set sequencer not running */
    strtim.tv_sec = 0; /* clear start time */
    strtim.tv_usec = 0;
    sinrun = false; /* set no input midi time marking */
    sintim.tv_sec = 0; /* clear input midi start time */
    sintim.tv_usec = 0;

    /* clear the wave track cache */
    for (i = 0; i < MAXWAVT; i++) wavfil[i] = NULL;

    /* clear the synth track cache */
    for (i = 0; i < MAXMIDT; i++) syntab[i] = NULL;

    /* clear the synth track active counts */
    for (i = 0; i < MAXMIDT; i++) numsql[i] = 0;

    /* create sequencer timer */
    seqhan = timerfd_create(CLOCK_REALTIME, 0);
    seqtimact = false;

    /* clear input select set */
    FD_ZERO(&ifdseta);

    /* select input file */
    FD_SET(0, &ifdseta);

    /* set current max input fd */
    ifdmax = 0+1;

    pthread_mutex_init(&seqlock, NULL); /* init sequencer lock */

    /* start sequencer thread */
    pthread_create(&sequencer_thread_id, NULL, sequencer_thread, NULL);

    /* initialize other locks */
    pthread_mutex_init(&wavlck, NULL); /* init wave output lock */
    pthread_mutex_init(&wnmlck, NULL); /* init wave count lock */
    pthread_mutex_init(&synlck, NULL); /* init synth sequencer lock */
    pthread_mutex_init(&snmlck, NULL); /* init synth count lock */

    numwav = 0; /* clear wave count */
    pthread_cond_init(&wnmzer, NULL); /* init wave count zero condition */

    numseq = 0; /* clear sequencer count */
    pthread_cond_init(&snmzer, NULL); /* init sequencer count zero condition */

    /* be warned: the diagnostics only show the device tables before
       installation of plug-ins */

    /* define the ALSA midi output devices */
    readalsadev(alsamidiout, "rawmidi", "Output", MAXMIDP, &alsamidioutnum);

#ifdef SHOWDEVTBL
    printf("\nmidi output devices:\n\n");
    prtdtbl(alsamidiout, MAXMIDP, false);
#endif

    /* define the ALSA midi input devices */
    readalsadev(alsamidiin, "rawmidi", "Input", MAXMIDP, &alsamidiinnum);

#ifdef SHOWDEVTBL
    printf("\nmidi input devices:\n\n");
    prtdtbl(alsamidiin, MAXMIDP, false);
#endif

    /* define the ALSA PCM output devices */
    readalsadev(alsapcmout, "pcm", "Output", MAXWAVP, &alsapcmoutnum);

#ifdef SHOWDEVTBL
    printf("\nPCM output devices:\n\n");
    prtdtbl(alsapcmout, MAXWAVP, true);
#endif

    /* define the ALSA PCM input devices */
    readalsadev(alsapcmin, "pcm", "Input", MAXWAVP, &alsapcminnum);

#ifdef SHOWDEVTBL
    printf("\nPCM input devices:\n\n");
    prtdtbl(alsapcmin, MAXWAVP, true);
#endif

    /* set midi out plug-in count */
    alsamidioutplug = 0;

}

/*******************************************************************************

Deinitialize sound module

Nothing required at present.

*******************************************************************************/

static void pa_deinit_sound (void) __attribute__((destructor (102)));
static void pa_deinit_sound()

{

    int i;

    /* issue close to all synth outs */
    for (i = 0; i < MAXMIDP; i++) if (alsamidiout[i])
        if (alsamidiout[i]->devopn) alsamidiout[i]->clsseq(i+1);

    /* issue close to all synth ins */
    for (i = 0; i < MAXMIDP; i++) if (alsamidiin[i])
        if (alsamidiin[i]->devopn) snd_rawmidi_close(alsamidiin[i]->midi);

    /* issue close to all wave outs */
    for (i = 0; i < MAXMIDP; i++) if (alsapcmout[i])
        if (alsapcmout[i]->devopn) snd_pcm_close(alsapcmout[i]->pcm);

    /* issue close to all wave ins */
    for (i = 0; i < MAXMIDP; i++) if (alsapcmin[i])
        if (alsapcmin[i]->devopn) snd_pcm_close(alsapcmin[i]->pcm);

}
