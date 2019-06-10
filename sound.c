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
* 3. The model for running MIDI and waveform files might have problems. Its    *
* supposed to be an accurate event, but its going to have file lookup time     *
* built into it, which could affect start time. A logical preload/cache        *
* model would give this package the ability to do something about that.        *
*                                                                              *
*******************************************************************************/

#include <pthread.h>

#include "sound.h"

#define MAXMID = 10; /* maximum midi input/output devices */

/* midi status messages, high nybble */

#define MESS_NOTE_OFF 0x80
#define MESS_NOTE_ON  0x90
#define MESS_AFTTCH   0xa0
#define MESS_CTRL_CHG 0xb0
#define MESS_PGM_CHG  0xc0
#define MESS_CHN_PRES 0xd0
#define MESS_PTCH_WHL 0xe0

/* midi controller numbers */

CTLR_BANK_SELECT_COARSE              0
CTLR_MODULATION_WHEEL_COARSE         1
CTLR_BREATH_CONTROLLER_COARSE        2
CTLR_FOOT_PEDAL_COARSE               4
CTLR_PORTAMENTO_TIME_COARSE          5
CTLR_DATA_ENTRY_COARSE               6
CTLR_VOLUME_COARSE                   7
CTLR_BALANCE_COARSE                  8
CTLR_PAN_POSITION_COARSE             10
CTLR_EXPRESSION_COARSE               11
CTLR_EFFECT_CONTROL_1_COARSE         12
CTLR_EFFECT_CONTROL_2_COARSE         13
CTLR_GENERAL_PURPOSE_SLIDER_1        16
CTLR_GENERAL_PURPOSE_SLIDER_2        17
CTLR_GENERAL_PURPOSE_SLIDER_3        18
CTLR_GENERAL_PURPOSE_SLIDER_4        19
CTLR_BANK_SELECT_FINE                32
CTLR_MODULATION_WHEEL_FINE           33
CTLR_BREATH_CONTROLLER_FINE          34
CTLR_FOOT_PEDAL_FINE                 36
CTLR_PORTAMENTO_TIME_FINE            37
CTLR_DATA_ENTRY_FINE                 38
CTLR_VOLUME_FINE                     39
CTLR_BALANCE_FINE                    40
CTLR_PAN_POSITION_FINE               42
CTLR_EXPRESSION_FINE                 43
CTLR_EFFECT_CONTROL_1_FINE           44
CTLR_EFFECT_CONTROL_2_FINE           45
CTLR_HOLD_PEDAL                      64
CTLR_PORTAMENTO                      65
CTLR_SUSTENUTO_PEDAL                 66
CTLR_SOFT_PEDAL                      67
CTLR_LEGATO_PEDAL                    68
CTLR_HOLD_2_PEDAL                    69
CTLR_SOUND_VARIATION                 70
CTLR_SOUND_TIMBRE                    71
CTLR_SOUND_RELEASE_TIME              72
CTLR_SOUND_ATTACK_TIME               73
CTLR_SOUND_BRIGHTNESS                74
CTLR_SOUND_CONTROL_6                 75
CTLR_SOUND_CONTROL_7                 76
CTLR_SOUND_CONTROL_8                 77
CTLR_SOUND_CONTROL_9                 78
CTLR_SOUND_CONTROL_10                79
CTLR_GENERAL_PURPOSE_BUTTON_1        80
CTLR_GENERAL_PURPOSE_BUTTON_2        81
CTLR_GENERAL_PURPOSE_BUTTON_3        82
CTLR_GENERAL_PURPOSE_BUTTON_4        83
CTLR_EFFECTS_LEVEL                   91
CTLR_TREMULO_LEVEL                   92
CTLR_CHORUS_LEVEL                    93
CTLR_CELESTE_LEVEL                   94
CTLR_PHASER_LEVEL                    95
CTLR_DATA_BUTTON_INCREMENT           96
CTLR_DATA_BUTTON_DECREMENT           97
CTLR_NON_REGISTERED_PARAMETER_FINE   98
CTLR_NON_REGISTERED_PARAMETER_COARSE 99
CTLR_REGISTERED_PARAMETER_FINE       100
CTLR_REGISTERED_PARAMETER_COARSE     101
CTLR_ALL_SOUND_OFF                   120
CTLR_ALL_CONTROLLERS_OFF             121
CTLR_LOCAL_KEYBOARD                  122
CTLR_ALL_NOTES_OFF                   123
CTLR_OMNI_MODE_OFF                   124
CTLR_OMNI_MODE_ON                    125
CTLR_MONO_OPERATION                  126
CTLR_POLY_OPERATION                  127

type

/* sequencer message types. each routine with a sequenced option has a
  sequencer message assocated with it */

typedef enum {
    st_noteon, st_noteoff, st_instchange, st_attack, st_release,
    st_legato, st_portamento, st_vibrato, st_volsynthchan, st_porttime,
    st_balance, st_pan, st_timbre, st_brightness, st_reverb, st_tremulo,
    st_chorus, st_celeste, st_phaser, st_aftertouch, st_pressure,
    st_pitch, st_pitchrange, st_mono, st_poly, st_playsynth,
    st_playwave, st_volwave
} seqtyp;

/* pointer to message */

typedef *segmsg seqptr;

/* sequencer message */

typedef struct seqmsg {

    seqptr next; /* next message in list */
    int port; /* port to which message applies */
    int time; /* time to execute message */
    union {

        channel ntc; note ntn; integer ntv;
        channel icc; instrument ici;
        channel vsc; int vsv;
        channel pc;
        channel bsc; boolean bsb;
        string ps;
        int wv;

    }

} seqmsg;

static snd_rawmini_t midtab[MAXMID];  /* midi output device table */
static seqptr seqlst;                 /* active sequencer entries */
static seqptr seqfre;                 /* free sequencer entries */
static boolean seqrun;                /* sequencer running */
static struct timeval strtim;         /* start time for sequencer, in raw linux
                                         time */
struct int seqhan;                    /* handle for sequencer timer */
struct pthread_mutex *seqlock;        /* sequencer task lock */
static pthread_t sequencer_thread_id; /* sequencer thread id */

/*
 * Set of input file ids for select
 */
static fd_set ifdseta; /* active sets */
static fd_set ifdsets; /* signaled set */
static int ifdmax;

/*******************************************************************************

Process sound library error

Outputs an error message using the special syslib function, then halts.

*******************************************************************************/

void error(string s)

{

    fprintf(stderr, "\nError: Sound: %s\n", s);

    exit(1);

end;

/*******************************************************************************

Issue MIDI 2 byte message

Sends a 2 byte MIDI message to the given ALSA midi port.

*******************************************************************************/

void midimsg2(snd_rawmidi_t *midiout, byte sts, byte dat1)

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

void midimsg3(snd_rawmidi_t *midiout, byte sts, byte dat1, byte dat2)

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

Activate sequencer timer

If the sequencer timer is not running, we activate it.

*******************************************************************************/

void acttim(int t)

{

    struct itimerspec ts; /* timer data */
    long tl;

    if (!seqlst) { /* nothing in sequencer list, activate timer */

        tl = t-elap; /* set next time to run */
        ts.it_value.tv_sec = tl/10000; /* set number of seconds to run */
        /* set number of nanoseconds to run */
        ts.it_value.tv_nsec = tl%10000*100000;
        ts.it_interval.tv_sec = 0; /* set does not rerun */
        ts.it_interval.tv_nsec = 0;
        timerfd_settime(seqtim, 0, &ts, NULL);

    }

}

/*******************************************************************************

Get sequencer message entry

Gets a sequencer message entry, either from the used list, or new.

*******************************************************************************/

void getseq(seqptr* p)

{

    if (seqfre) { /* get a previous sequencer message */

        p = seqfre; /* index top entry */
        seqfre = seqfre->next; /* gap out */

    } else
        /* else get a new entry, with full allocation */
        p = (seqptr) malloc(sizeof(seqmsg);
    p->next = NULL; /* clear next */

}

/*******************************************************************************

Put sequencer message entry

Puts a sequencer message entry to the free list for reuse.

*******************************************************************************/

void putseq(seqptr p)

{

   /* dispose of any string first, we don't recycle those */
   if (p->st = st_playsynth || p->st = st_playwave) free(p->ps);
   p->next = seqfre; /* link to top of list */
   seqfre = p /* push onto list */

}

/*******************************************************************************

Insert sequencer message

Inserts a sequencer message into the list, in ascending time order.

*******************************************************************************/

void insseq(seqptr p)

{

    seqptr lp, l;

    pthread_mutex_lock(seqlock); /* take sequencer data lock */
    /* check sequencer list empty */
    if (!seqlst) seqlst = p; /* place as first if so */
    /* check insert to start */
    else if (p->time < seqlst->time) {

        p->next = seqlst; /* push onto list */
        seqlst = p

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
    pthread_mutex_unlock(seqlock); /* release lock */

}

/*******************************************************************************

Execute sequencer message

Executes the call referenced by the message. Each call is performed with
sequencer bypass, which means its ok to loop back on the call.

*******************************************************************************/

void excseq(seqptr p)

{

    switch (p->st) { /* sequencer message type */

        case st_noteon:       noteon(p->port, 0, p->ntc, p->ntn, p->ntv); break;
        case st_noteoff:      noteoff(p->port, 0, p->ntc, p->ntn, p->ntv);
                              break;
        case st_instchange:   instchange(p->port, 0, p->icc, p->ici); break;
        case st_attack:       attack(p->port, 0, p->vsc, p->vsv); break;
        case st_release:      release(p->port, 0, p->vsc, p->vsv); break;
        case st_legato:       legato(p->port, 0, p->bsc, p->bsb); break;
        case st_portamento:   portamento(p->port, 0, p->bsc, p->bsb); break;
        case st_vibrato:      vibrato(p->port, 0, p->vsc, p->vsv); break;
        case st_volsynthchan: volsynthchan(p->port, 0, p->vsc, p->vsv); break;
        case st_porttime:     porttime(p->port, 0, p->vsc, p->vsv); break;
        case st_balance:      balance(p->port, 0, p->vsc, p->vsv); break;
        case st_pan:          pan(p->port, 0, p->vsc, p->vsv); break;
        case st_timbre:       timbre(p->port, 0, p->vsc, p->vsv); break;
        case st_brightness:   brightness(p->port, 0, p->vsc, p->vsv); break;
        case st_reverb:       reverb(p->port, 0, p->vsc, p->vsv); break;
        case st_tremulo:      tremulo(p->port, 0, p->vsc, p->vsv); break;
        case st_chorus:       chorus(p->port, 0, p->vsc, p->vsv); break;
        case st_celeste:      celeste(p->port, 0, p->vsc, p->vsv); break;
        case st_phaser:       phaser(p->port, 0, p->vsc, p->vsv); break;
        case st_aftertouch:   aftertouch(p->port, 0, p->ntc, p->ntn, p->ntv);
                              break;
        case st_pressure:     pressure(p->port, 0, p->ntc, p->ntn, p->ntv);
                              break;
        case st_pitch:        pitch(p->port, 0, p->vsc, p->vsv); break;
        case st_pitchrange:   pitchrange(p->port, 0, p->vsc, p->vsv); break;
        case st_mono:         mono(p->port, 0, p->vsc, p->vsv); break;
        case st_poly:         poly(p->port, 0, p->pc); break;
        case st_playsynth:    playsynth(p->port, 0, p->ps^); break;
        case st_playwave:     playwave(p->port, 0, p->ps^); break;
        case st_volwave:      volwave(p->port, 0, p->wv) break;

    }

}

/*******************************************************************************

Find elapsed millisecond time corrected

Finds the elapsed time on the linux microsecond time, then corrects that for
100us time.

Linux time is kept as seconds and a microseconds offset from that. It does not
wrap.

*******************************************************************************/

int difftime(struct timeval* rt)

{

    int ds; /* seconds difference */
    int du; /* microseconds difference */
    struct timeval tv; /* record to get time */

    gettimeofday(&tv, NULL); /* get current time */
    ds = tv.tv_sec-rt.tv_sec; /* find difference in seconds */
    du = tv.tv_usec-rt.tv_usec; /* find difference in microseconds */

    return (ds*10000+du/100); /* return difference in 100us increments */

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

    while (1) { /* until thread cancelled */

        /* no input is active, load a new signaler set */
        ifdsets = ifdseta; /* set up request set */
        rv = select(ifdmax, &ifdsets, NULL, NULL, NULL);
        /* if error, the input set won't be modified and thus will appear as
           if they were active. We clear them in this case */
        if (rv < 0) FD_ZERO(&ifdsets);
        if (FD_ISSET(timer, &ifdsets)) {

            /* the sequencer timer went off */
            if seqrun then begin /* sequencer is still running */

                pthread_mutex_lock(seqlock); /* take sequencer data lock */
                p = seqlst; /* index top of list */
                while (p) { /* process all past due messages */

                    elap = difftime(strtim); /* find elapsed time */
                    if (p->time <= elap) { /* execute this message */

                        seqlst = seqlst->next; /* gap out */
                        excseq(p); /* execute it */
                        putseq(p); /* release entry */
                        p = seqlst /* index top of list again */

                    end else p = NULL /* stop search */

                end;
                if (seqlst) { /* start sequencer timer again */

                    tl = seqlst->time-elap; /* set next time to run */
                    ts.it_value.tv_sec = tl/10000; /* set number of seconds to run */
                    ts.it_value.tv_nsec = tl%10000*100000; /* set number of nanoseconds to run */
                    ts.it_interval.tv_sec = 0; /* set does not rerun */
                    ts.it_interval.tv_nsec = 0;
                    timerfd_settime(seqhan, 0, &ts, NULL);

                }
                pthread_mutex_unlock(seqlock);

            }

        }

    }

}

/*******************************************************************************

Find number of output midi ports

Returns the total number of output midi ports.

*******************************************************************************/

int synthout(void)

{

   return 1; /* for testing */

}

/*******************************************************************************

Open synthesiser output port

Opens a syth output port. The output ports have their own separate logical
numbers separate from input numbers, and by convention, port 1 will be the
main synthesizer for the computer, and port 2 will be an output port to any
midi chained devices outside the computer.

*******************************************************************************/

void opensynthout(int p)

{

    int r;

    r = snd_rawmidi_open(NULL, &midtab[p], "default", SND_RAWMIDI_SYNC);
    if (r < 0) error("Cannot open synthethizer");

}

/*******************************************************************************

Close midi sythesiser output port

Closes a previously opened midi output port.

*******************************************************************************/

void closesynthout(int p)

{

    snd_rawmidi_close(&midtab[p]); /* close port */

}

/*******************************************************************************

Start time

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

void starttime(void)

{

    gettimeofday(&strtim, NULL); /* get current time */
    seqrun = true; /* set sequencer running */

}

/*******************************************************************************

Stop time

Stops midi sequencer function. Any timers and buffers in use by the sequencer
are cleared, and all pending events dropped.

*******************************************************************************/

void stoptime(void)

{

    seqptr p; /* message pointer */
    struct itimerspec ts;

    strtim.tv_sec = 0; /* clear start time */
    strtim.tv_usec = 0;
    seqrun = false; /* set sequencer not running */
    /* if there is a pending sequencer timer, kill it */
    if (timhan) {

        /* set timer run time to zero to kill it */
        ts.it_value.tv_sec = 0;
        ts.it_value.tv_nsec = 0;
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;
        rv = timerfd_settime(seqhan, 0, &ts, NULL);

    }
    /* now clear all pending events */
    pthread_mutex_lock(seqlock); /* take sequencer data lock */
    while (seqlst) { /* clear */

        p = seqlst; /* index top of list */
        seqlst = seqlst->next; /* gap out */
        putseq(p); /* release entry */

    }
    pthread_mutex_unlock(seqlock); /* drop lock */

}

/*******************************************************************************

Get current time

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

*******************************************************************************/

int curtime;

{

   if (!seqrun) error("Sequencer not running");
   curtime = difftime(strtim); /* return difference time */

}

/*******************************************************************************

Note on

Turns on a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to maxint.

*******************************************************************************/

void noteon(int p, int t, channel c, note n, int v)



{

    int     r;    /* return value */
    int     elap; /* current elapsed time */
    seqptr  sp;   /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        midimsg3(mess_note_on+(c-1), n-1, v/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_noteon; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntn = n; /* set note */
        sp->ntv = v; /* set velocity */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

end;

/*******************************************************************************

Note off

Turns off a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to maxint.

*******************************************************************************/

void noteoff(int p, int t, channel c, note n, int v)

{

    int r;
    int elap;  /* current elapsed time */
    seqptr sp; /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        midimsg3(mess_note_off+(c-1), n-1, v/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_noteoff; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntn = n; /* set note */
        sp->ntv = v; /* set velocity */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Instrument change

Selects a new instrument for the given channel. Tbe new instrument is specified
by Midi GM encoding, 1 to 128. Takes a time for sequencing.

*******************************************************************************/

void instchange(int p, int t, channel c, instrument i);

{

    int r;
    int elap;  /* current elapsed time */
    seqptr sp; /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    if (i < 1 || i > 128) error("Bad instrument number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        midimsg2(mess_pgm_chg+(c-1), i-1);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_instchange; /* set type */
        sp->icc = c; /* set channel */
        sp->ici = i; /* set instrument */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Controller change

Processes a controller value set, from 0 to 127.

*******************************************************************************/

static void ctlchg(int p, int t, channel c, int cn, int v)

{

    /* construct midi message */
    midimsg3(mess_ctrl_chg+(c-1), cn-1, v/0x01000000);

}

/*******************************************************************************

Set attack time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

*******************************************************************************/

void attack(int p, int t, channel c, int at)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_sound_attack_time, at div 0x01000000)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_attack; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = at; /* set attack */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set release time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

*******************************************************************************/

void release(int p, int t, channel c, int rt)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (c < 1 || c > 16) then error('Bad channel number');
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_sound_release_time, rt/0x01000000)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_release; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = rt; /* set release */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Legato pedal on/off

Sets the legato mode on/off.

*******************************************************************************/

void legato(int p, int t, channel c, boolean b)

{

    int elap;  /* current elapsed time */
    seqptr sp; /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_legato_pedal, !!b*127)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_legato; /* set type */
        sp->bsc = c; /* set channel */
        sp->bsb = b; /* set on/off */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Portamento pedal on/off

Sets the portamento mode on/off.

*******************************************************************************/

void portamento(int p, int t, channel c, boolean b)

{

    int elap;  /* current elapsed time */
    seqptr sp; /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) then begin

        ctlchg(p, t, c, ctlr_portamento, !!b*127)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_portamento; /* set type */
        sp->bsc = c; /* set channel */
        sp->bsb = b; /* set on/off */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set volume

Sets synthesizer volume, 0 to maxint.

*******************************************************************************/

void volsynthchan(int p, int t: integer, channel c, int v)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_volume_coarse, v/0x01000000); /* set high */
        ctlchg(p, t, c, ctlr_volume_fine, v/0x00020000 & 0x7f) /* set low */

    end else begin /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_volsynthchan; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set volume */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set left right channel balance

Sets sets the left right channel balance. -maxint is all left, 0 is center,
maxint is all right.

*******************************************************************************/

void balance(int p, int t, channel c, int b)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        b = b/0x00040000+0x2000; /* reduce to 14 bits, positive only */
        ctlchg(p, t, c, ctlr_balance_coarse, b/0x80); /* set high */
        ctlchg(p, t, c, ctlr_balance_fine, b & 0x7f) /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_balance; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = b; /* set balance */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set portamento time

Sets portamento time, 0 to maxint.

*******************************************************************************/

void porttime(int p, int t, channel c, int v)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* set high */
        ctlchg(p, t, c, ctlr_portamento_time_coarse, v/0x01000000);
        /* set low */
        ctlchg(p, t, c, ctlr_portamento_time_fine, v/0x00020000 & 0x7f)

    end else begin /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_porttime; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set time */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set vibrato

Sets modulaton value, 0 to maxint.

*******************************************************************************/

void vibrato(int p, int t, channel c, int v)

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* set high */
        ctlchg(p, t, c, ctlr_modulation_wheel_coarse, v/0x01000000);
        /* set low */
        ctlchg(p, t, c, ctlr_modulation_wheel_fine, v/0x00020000 & 0x7f)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_vibrato; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set vibrato */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set left/right pan position

Sets sets the left/right pan position. -maxint is hard left, 0 is center,
maxint is hard right.

*******************************************************************************/

void pan(int p, int t, channel c, int b);

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        b = b/0x00040000+0x2000; /* reduce to 14 bits, positive only */
        ctlchg(p, t, c, ctlr_pan_position_coarse, b/0x80); /* set high */
        ctlchg(p, t, c, ctlr_pan_position_fine, b & 0x7f) /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pan; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = b; /* set pan */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound timbre

Sets the sound timbre, 0 to maxint.

*******************************************************************************/

void timbre(int p, int t, channel c, int tb);

{

    seqptr sp; /* message pointer */
    int elap;  /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_sound_timbre, tb div 0x01000000)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_timbre; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = tb; /* set timbre */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound brightness

Sets the sound brightness, 0 to maxint.

*******************************************************************************/

void brightness(int p, int t, channel c, int b)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_sound_brightness, b/0x01000000)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_brightness; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = b; /* set brightness */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

   }

}

/*******************************************************************************

Set sound reverb

Sets the sound reverb, 0 to maxint.

*******************************************************************************/

void reverb(int p, int t, channel c, int r)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_effects_level, r/0x01000000)

    } else { /* sequence */

       /* check sequencer running */
       if (!seqrun) error("Sequencer not running");
       getseq(sp); /* get a sequencer message */
       sp->port = p; /* set port */
       sp->time = t; /* set time */
       sp->st = st_reverb; /* set type */
       sp->vsc = c; /* set channel */
       sp->vsv = r; /* set reverb */
       insseq(sp); /* insert to sequencer list */
       acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound tremulo

Sets the sound tremulo, 0 to maxint.

*******************************************************************************/

void tremulo(int p, int t, channel c, int tr)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) then begin

        ctlchg(p, t, c, ctlr_tremulo_level, tr/0x01000000)

    } else { /* sequence */

        /* check sequencer running */
       if (!seqrun) error("Sequencer not running");
       getseq(sp); /* get a sequencer message */
       sp->port = p; /* set port */
       sp->time = t; /* set time */
       sp->st = st_tremulo; /* set type */
       sp->vsc = c; /* set channel */
       sp->vsv = tr; /* set tremulo */
       insseq(sp); /* insert to sequencer list */
       acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound chorus

Sets the sound chorus, 0 to maxint.

*******************************************************************************/

void chorus(int p, int t, channel c, int cr)

{

    seqptr sp;  /* message pointer */
    int elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_chorus_level, cr/0x01000000)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_chorus; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = cr; /* set chorus */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound celeste

Sets the sound celeste, 0 to maxint.

*******************************************************************************/

void celeste(int p, int t, channel c, int ce);

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_celeste_level, ce/0x01000000)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_celeste; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = ce; /* set celeste */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set sound phaser

Sets the sound phaser, 0 to maxint.

*******************************************************************************/

void phaser(int p, int t, channel c, int ph)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_phaser_level, ph/0x01000000)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_phaser; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = ph; /* set phaser */
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

void pitchrange(int p, int t, channel c, int v)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* set up data entry */
        ctlchg(p, t, c, ctlr_registered_parameter_coarse, 0); /* set high */
        ctlchg(p, t, c, ctlr_registered_parameter_fine, 0); /* set low */
        ctlchg(p, t, c, ctlr_data_entry_coarse, v/0x01000000); /* set high */
        ctlchg(p, t, c, ctlr_data_entry_fine, v/0x00020000 and 0x7f) /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pitchrange; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set pitchrange */
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

void mono(int p, int t, channel c, int ch)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    if (ch < 0 || c > 16) error("Bad mono mode number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_mono_operation, ch)

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_mono; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = ch; /* set mono mode */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set polyphonic mode

Reenables polyphonic mode after a monophonic operation.

*******************************************************************************/

void poly(int p, int t, channel c)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, ctlr_poly_operation, 0) /* value dosen't matter */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_poly; /* set type */
        sp->pc = c; /* set channel */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Aftertouch

Controls aftertouch, 0 to maxint, on a note.

*******************************************************************************/

void aftertouch(int p, int t, channel c, note n, int at)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

   if (c < 1 || c > 16) error("Bad channel number");
   if (n < 1) or (n > 128) then error('Bad note number');
   elap = difftime(strtim); /* find elapsed time */
   /* execute immediate if 0 or sequencer running and time past */
   if (t == 0 || (t <= elap && seqrun)) {

      /* construct midi message */
      midimsg3(mess_afttch+(c-1), n-1, at/0x01000000);

   } else { /* sequence */

      /* check sequencer running */
      if (!seqrun) error("Sequencer not running");
      getseq(sp); /* get a sequencer message */
      sp->port = p; /* set port */
      sp->time = t; /* set time */
      sp->st = st_aftertouch; /* set type */
      sp->ntc = c; /* set channel */
      sp->ntn = n; /* set note */
      sp->ntv = at; /* set aftertouch */
      insseq(sp); /* insert to sequencer list */
      acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Channel pressure

Controls channel pressure, 0 to maxint, on a note.

*******************************************************************************/

void pressure(int p, int t, channel c, note n, int pr)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        midimsg3(mess_chn_pres+(c-1), n-1, pr/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pressure; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntn = n; /* set note */
        sp->ntv = pr; /* set pressure */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Set pitch wheel

Sets the pitch wheel value, from 0 to maxint. This is the amount off the note
in the channel. The GM standard is to adjust for a whole step up and down, which
is 4 half steps total. A "half step" is the difference between, say, C and C#.

*******************************************************************************/

void pitch(int p, int t, channel c, int pt)

{

    seqptr sp;  /* message pointer */
    int elap; /* current elapsed time */

    if (c < 1) or (c > 16) then error('Bad channel number');
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        pt = pt div 0x00040000+0x2000; /* reduce to 14 bits, positive only */
        /* construct midi message */
        midimsg3(mess_ptch_whl+(c-1), pt & 0x7f, pt/0x80);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pitch; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = pt; /* set pitch */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Play ALSA midi file

Plays the given ALSA midi file given the filename.

Not implemented.

*******************************************************************************/

static void alsaplaywave(string fn)

{

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

void playsynth(int p, int t, string sf)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    if p != 1 then error('Must execute play on default output channel');
    if midtab[p] < 0 then error('Synth output channel not open');
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        alsaplaymidi(sf); /* play the file */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_playsynth; /* set type */
        strcpy(sp->ps, sf); /* make copy of file string to play */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Find number of wave devices.

Returns the number of wave output devices available. This is hardwared to 1 for
the one Linux waveform device.

*******************************************************************************/

function waveout: integer;

begin

   waveout = 1

end;

/*******************************************************************************

Open wave output device

Opens a wave output device by number. By convention, wave out 1 is the default
output device. This is presently a no-op for linux.

*******************************************************************************/

void openwaveout(p: integer);

begin

end;

/*******************************************************************************

CLose wave output device

Closes a wave output device by number. This is presently a no-op for linux.

*******************************************************************************/

void closewaveout(int p)

{

}

/*******************************************************************************

Play ALSA sound file

Plays the given ALSA sound file given the filename.

*******************************************************************************/

static void alsaplaywave(string fn)

{

    unsigned int pcm, tmp;
    int rate, channels;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    char *buff;
    int buff_size;
    int r;
    bololean end;
    FILE* fh;

    /* open input .wav file */
    fh = fopen(fn, "r");
    if (!fh) error("Cannot open input .wav file");

    /* open pcm device */
    r = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (r < 0) error("Cannot open PCM output device");

    snd_pcm_hw_params_alloca(&params); /* get hw parameter block */
    snd_pcm_hw_params_any(pcm_handle, params);

    /* Set parameters */
    r = snd_pcm_hw_params_set_access(pcm_handle, params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
    if (r < 0) error("Cannot set interleaved mode");

    r = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    if (r < 0) error("Cannot set format");

    r = snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
    if (r < 0) error("Cannot set channels number");

    r = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);
    if (r < 0) error("Cannot set rate");

    /* Write parameters */
    r = snd_pcm_hw_params(pcm_handle, params);
    if (r < 0) error("cannot set hardware parameters");

    /* get number of channels */
    snd_pcm_hw_params_get_channels(params, &channels);

    /* get sample rate */
    snd_pcm_hw_params_get_rate(params, &rate, 0);

    /* Allocate buffer to hold single period */
    snd_pcm_hw_params_get_period_size(params, &frames, 0);

    buff_size = frames * channels * 2 /* 2 -> sample size */;
    buff = (char *) malloc(buff_size);

    snd_pcm_hw_params_get_period_time(params, &tmp, NULL);

    end = false;
    while (!end) {

        /* read input .wav file */
        r = fread(buff, buff_size, 1, fh);
        end = r == 0;
        if (!end) {

            /* write samples to PCM device */
            r = snd_pcm_writei(pcm_handle, buff, frames);
            if (r == -EPIPE) snd_pcm_prepare(pcm_handle);
            else if (r < 0) error("Cannot write to PCM device");

        }

    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    free(buff);
    fclose(fh);

}

/*******************************************************************************

Play waveform file

Plays the waveform file to the indicated wave device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows wave files to be sequenced against other wave files and midi files.
The file is specified by file name, and the file type is system dependent.

*******************************************************************************/

void playwave(int p, int t, string sf)

{

    seqptr sp;   /* message pointer */
    int    elap; /* current elapsed time */

    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        alsaplaywave(sf);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        getseq(sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_playwave; /* set type */
        strcpy(sp->ps, sf); /* make copy of file string to play */
        insseq(sp); /* insert to sequencer list */
        acttim(t); /* kick timer if needed */

    }

}

/*******************************************************************************

Adjust waveform volume

Adjusts the volume on waveform playback. The volume value is from 0 to maxint.

Not implemented at present.

*******************************************************************************/

void volwave(int p, int t, int v)

{

}

/*******************************************************************************

Initialize sound module

Clears sequencer lists, flags no timer active, clears the midi output port
table, and initalizes the sequencer task mutex.

*******************************************************************************/

static void pa_init_sound (void) __attribute__((constructor (102)));
static void pa_init_sound()

{

    int i; /* index for midi tables */

    seqlst = NULL; /* clear active sequencer list */
    seqfre = NULL; /* clear free sequencer messages */
    seqrun = false; /* set sequencer not running */
    strtim.tv_sec = 0; /* clear start time */
    strtim.tv_usec = 0;
    timhan = 0; /* set no timer active */
    /* set no midi output ports open */
    for (i = 0; i < MAXMID; i++) midtab[i] = -1;

    /* create sequencer timer */
    seqhan = timerfd_create(CLOCK_REALTIME, 0);

    /* clear input select set */
    FD_ZERO(&ifdseta);

    /* select input file */
    FD_SET(0, &ifdseta);

    /* set current max input fd */
    ifdmax = 0+1;

    pthread_mutex_init(seqlock); /* init sequencer lock */

    /* start sequencer thread */
    pthread_create(&sequencer_thread_id, NULL, sequencer_thread, NULL);

}

/*******************************************************************************

Deinitialize sound module

Nothing required at present.

*******************************************************************************/

static void pa_deinit_terminal (void) __attribute__((destructor (102)));
static void pa_deinit_terminal()

{

}
