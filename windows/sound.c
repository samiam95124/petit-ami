/*******************************************************************************
*                                                                              *
*                               SOUND LIBRARY                                  *
*                                                                              *
*                              11/02 S. A. Franco                              *
*                                                                              *
* Sndlib is a combination of wave file and midi output and control functions.  *
* Implements a set of midi controls and wave controls. Also includes a "flow   *
* through sequencer" function. Each event has a timestamp, and if zero, it     *
* is performed immediately, otherwise scheduled. This allows any mix of        *
* immediate vs. sequenced events.                                              *
*                                                                              *
* Notes:                                                                       *
*                                                                              *
* 1. The parameter conversion work is being performed when a sequenced item    *
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
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
*******************************************************************************/

#define WINVER 0x0A00
#define _WIN32_WINNT 0xA00

#include <sys/types.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>
#include <commctrl.h>

#include <config.h>
#include <sound.h>

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); } while (0)

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

static HMIDIOUT         midtab[MAXMIDP];   /* midi output device table */
static int              i;                 /* index for midi tables */
static pa_seqptr        seqlst;            /* active sequencer entries */
static pa_seqptr        seqfre;            /* free sequencer entries */
static int              seqrun;            /* sequencer running */
static DWORD            strtim;            /* start time for sequencer, in raw windows
                                            time */
static MMRESULT         timhan;            /* handle for running timer */
static CRITICAL_SECTION seqlock;           /* sequencer task lock */
static string           synthnam[MAXMIDT]; /* midi track file names */
static string           wavenam[MAXWAVT];  /* wave track file names */

/*******************************************************************************

Process sound library error

Outputs an error message, then halts.

*******************************************************************************/

static void error(const string s)

{

    fprintf(stderr, "*** Sound: %s\n", s);

    exit(1);

}

/*******************************************************************************

Find path separator character

Returns the character used to separate filename path sections.
In windows/dos this is "\", in Unix/Linux it is '/'. One possible solution to
pathing is to accept both characters as a path separator. This means that
systems that use the '\' as a forcing character would need to represent the
separator as '\\'.

*******************************************************************************/

static char pthchr(void)
{

    return ('\\');

}

/********************************************************************************

Break file specification

Breaks a filespec down into its components, the path, name and extension.
Note that we don't validate file specifications here. Note that any part of
the file specification could be returned blank.

For Windows, we trim leading and trailing spaces, but leave any embedded spaces
or ".".

The path is straightforward, and consists of any number of /x sections. The
Presence of a trailing "\" without a name means the entire thing gets parsed
as a path, including any embedded spaces or "." characters.

Windows allows any number of "." characters, so we consider the extension to be
only the last such section, which could be null. Windows does not technically
consider "." to be a special character, but if the brknam and maknam procedures
are properly paired, it will effectively be treated the same as if the "."
were a normal character.

********************************************************************************/

static void brknam(
        /* file specification */ char *fn,
        /* path */               char *p, int pl,
        /* name */               char *n, int nl,
        /* extention */          char *e, int el
)

{

    int i, s, f, t; /* string indexes */
    char *s1, *s2;

    /* clear all strings */
    *p = 0;
    *n = 0;
    *e = 0;
    if (!strlen(fn)) error("File specification is empty");
    s1 = fn; /* index file spec */
    /* skip spaces */
    while (*s1 && *s1 == ' ') s1++;
    /* find last '/' that will mark the path */
    s2 = strrchr(s1, pthchr());
    if (s2) {

        /* there was a path, store that */
        while (s1 <= s2) {

            if (!pl) error("String to large for destination\n");
            *p++ = *s1++;
            pl--;

        }
        if (!nl) error("String to large for destination\n");
        *p = 0;
        /* now s1 points after path */

    }
    /* keep any leading '.' characters from fooling extension finder */
    s2 = s1; /* copy start point in name */
    while (*s2 == '.') s2++;
    /* find last '.' that will mark the extension */
    s2 = strrchr(s2, '.');
    if (s2) {

        /* there was a name, store that */
        while (s1 < s2) {

            if (!nl) error("String to large for destination\n");
            *n++ = *s1++;
            nl--;

        }
        if (!nl) error("String to large for destination\n");
        *n = 0;
        /* now s1 points to extension or nothing */

    } else {

        /* no extension */
        if (strlen(s1)+1 > nl) error("String to large for destination\n");
        strcpy(n, s1);
        while (*s1) s1++;

    }
    if (*s1 == '.') {

        /* there is an extension */
        if (strlen(s1)+1 > el) error("String to large for destination\n");
        s1++; /* skip '.' */
        strcpy(e, s1);

    }

}

/********************************************************************************

Make specification

Creates a file specification from its components, the path, name and extention.
We make sure that the path is properly terminated with ':' or '\' before
concatenating.

********************************************************************************/

static void maknam(
    /** file specification to build */ char *fn,
    /** file specification length */   int fnl,
    /** path */                        char *p,
    /** filename */                    char *n,
    /** extension */                   char *e
)
{

    int  i;    /* index for string */
    int  fsi;  /* index for output filename */
    char s[2]; /* path character buffer */

    if (strlen(p) > fnl) error("String too large for destination");
    strcpy(fn, p); /* place path */
    /* check path properly terminated */
    i = strlen(p);   /* find length */
    if (*p) /* not null */
        if (p[i-1] != pthchr()) {

        if (strlen(fn)+1 > fnl) error("String too large for destination");
        s[0] = pthchr(); /* set up path character as string */
        s[1] = 0;
        strcat(fn, s); /* add path separator */

    }
    /* terminate path */
    if (strlen(fn)+strlen(n) > fnl) error("String too large for destination");
    strcat(fn, n); /* place name */
    if (*e) {  /* there is an extension */

        if (strlen(fn)+1+strlen(e) > fnl)
            error("String too large for destination");
        strcat(fn, "."); /* place '.' */
        strcat(fn, e); /* place extension */

    }

}

/*******************************************************************************

Get sequencer message entry

Gets a sequencer message entry, either from the used list, or new.

*******************************************************************************/

static void getseq(pa_seqptr* p)

{

    if (seqfre) { /* get a previous sequencer message */

        *p = seqfre; /* index top entry */
        seqfre = seqfre->next; /* gap out */

    } else {

        /* else get a new entry, with full allocation */
        *p = malloc(sizeof(pa_seqmsg));
        if (!*p) error("No memory");

    }
    (*p)->next = NULL; /* clear next */

}

/*******************************************************************************

Put sequencer message entry

Puts a sequencer message entry to the free list for reuse.

*******************************************************************************/

static void putseq(pa_seqptr p)

{

    p->next = seqfre; /* link to top of list */
    seqfre = p; /* push onto list */

}

/*******************************************************************************

Insert sequencer message

Inserts a sequencer message into the list, in asc}ing time order.

*******************************************************************************/

static void insseq(pa_seqptr p)

{

    pa_seqptr lp, l;

    EnterCriticalSection(&seqlock); /* start exclusive access */
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
    LeaveCriticalSection(&seqlock); /* end exclusive access */

}

/*******************************************************************************

Execute sequencer message

Executes the call referenced by the message. Each call is performed with
sequencer bypass, which means its ok to loop back on the call.

*******************************************************************************/

static void excseq(pa_seqptr p)

{

    switch (p->st) { /* sequencer message type */

        case st_noteon:       pa_noteon(p->port, 0, p->ntc, p->ntn, p->ntv);
                              break;
        case st_noteoff:      pa_noteoff(p->port, 0, p->ntc, p->ntn, p->ntv);
                              break;
        case st_instchange:   pa_instchange(p->port, 0, p->icc, p->ici); break;
        case st_attack:       pa_attack(p->port, 0, p->vsc, p->vsv); break;
        case st_release:      pa_release(p->port, 0, p->vsc, p->vsv); break;
        case st_legato:       pa_legato(p->port, 0, p->bsc, p->bsb); break;
        case st_portamento:   pa_portamento(p->port, 0, p->bsc, p->bsb); break;
        case st_vibrato:      pa_vibrato(p->port, 0, p->vsc, p->vsv); break;
        case st_volsynthchan: pa_volsynthchan(p->port, 0, p->vsc, p->vsv); break;
        case st_porttime:     pa_porttime(p->port, 0, p->vsc, p->vsv); break;
        case st_balance:      pa_balance(p->port, 0, p->vsc, p->vsv); break;
        case st_pan:          pa_pan(p->port, 0, p->vsc, p->vsv); break;
        case st_timbre:       pa_timbre(p->port, 0, p->vsc, p->vsv); break;
        case st_brightness:   pa_brightness(p->port, 0, p->vsc, p->vsv); break;
        case st_reverb:       pa_reverb(p->port, 0, p->vsc, p->vsv); break;
        case st_tremulo:      pa_tremulo(p->port, 0, p->vsc, p->vsv); break;
        case st_chorus:       pa_chorus(p->port, 0, p->vsc, p->vsv); break;
        case st_celeste:      pa_celeste(p->port, 0, p->vsc, p->vsv); break;
        case st_phaser:       pa_phaser(p->port, 0, p->vsc, p->vsv); break;
        case st_aftertouch:   pa_aftertouch(p->port, 0, p->ntc, p->ntn, p->ntv);
                              break;
        case st_pressure:     pa_pressure(p->port, 0, p->ntc, p->ntv);
                              break;
        case st_pitch:        pa_pitch(p->port, 0, p->vsc, p->vsv); break;
        case st_pitchrange:   pa_pitchrange(p->port, 0, p->vsc, p->vsv); break;
        case st_mono:         pa_mono(p->port, 0, p->vsc, p->vsv); break;
        case st_poly:         pa_poly(p->port, 0, p->pc); break;
        case st_playsynth:    pa_playsynth(p->port, 0, p->sid); break;
        case st_playwave:     pa_playwave(p->port, 0, p->wt); break;
        case st_volwave:      pa_volwave(p->port, 0, p->wv); break;

    }

}

/*******************************************************************************

Find elapsed millisecond time corrected

Finds the elapsed time on the Windows millisecond time, then corrects that
for 100us time.

Windows time is kept as a wrapping unsigned timer. Because add and subtract
are the same regardless of signed/unsigned, we do trial subtracts and examine
the results, instead of using comparisons, and -1 becomes the top value.

*******************************************************************************/

static DWORD difftime(DWORD rt)

{

    DWORD ct, et;

    ct = timeGetTime(); /* get current time */
    et = ct-rt; /* find difference assuming rt > ct */
    if (et < 0) et = -1-rt+ct;

    return (et*10); /* correct 1 milliseconds to 100us */

}

/*******************************************************************************

Timer handler procedure

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

static void CALLBACK nextseq(UINT id, UINT msg, DWORD_PTR usr, DWORD_PTR dw1,
                             DWORD_PTR dw2)

{

    pa_seqptr p;    /* message entry pointer */
    DWORD     elap; /* elapsed time */

    if (seqrun) { /* sequencer is still running */

        EnterCriticalSection(&seqlock); /* start exclusive access */
        p = seqlst; /* index top of list */
        while (p) { /* process all past due messages */

            elap = difftime(strtim); /* find elapsed time */
            if (p->time <= elap) { /* execute this message */

                seqlst = seqlst->next; /* gap out */
                excseq(p); /* execute it */
                putseq(p); /* release entry */
                p = seqlst; /* index top of list again */

            } else p = NULL; /* stop search */

        }
        if (seqlst) /* start another timer */
            timhan = timeSetEvent((seqlst->time-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);
        LeaveCriticalSection(&seqlock); /* end exclusive access */

    }

}

/********************************************************************************

Find number of output midi ports

Returns the total number of output midi ports.

********************************************************************************/

int pa_synthout(void)

{

    return (midiOutGetNumDevs());

}

/*******************************************************************************

Find number of input midi ports

Returns the total number of input midi ports.

*******************************************************************************/

int pa_synthin(void)

{

    error("pa_synthin: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/********************************************************************************

Open synthesizer output port

Opens a syth output port. The output ports have their own separate logical
numbers separate from input numbers, and by convention, port 1 will be the
main synthesizer for the computer, and port 2 will be an output port to any
midi chained devices outside the computer.

********************************************************************************/

void pa_opensynthout(int p)

{

    /* open midi output device */
    midiOutOpen(&midtab[p], p-1, 0, 0, CALLBACK_NULL);

}

/********************************************************************************

Close midi sythesiser output port

Closes a previously opened midi output port.

********************************************************************************/

void pa_closesynthout(int p)

{

    midiOutClose(midtab[p]); /* close port */
    midtab[p] = (HMIDIOUT)-1; /* set closed */

}

/********************************************************************************

Start time output

Starts the sequencer function. The sequencer is cleared, and upcount {s
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

********************************************************************************/

void pa_starttimeout(void)

{

    strtim = timeGetTime(); /* place current system time */
    seqrun = TRUE; /* set sequencer running */

}

/********************************************************************************

Stop time output

Stops midi sequencer function. Any timers and buffers in use by the sequencer
are cleared, and all pending events dropped.

********************************************************************************/

void pa_stoptimeout(void)

{

    pa_seqptr p; /* message pointer */

    strtim = 0; /* clear start time */
    seqrun = FALSE; /* set sequencer ! running */
    /* if there is a p}ing sequencer timer, kill it */
    if (timhan) timeKillEvent(timhan);
    /* now clear all p}ing events */
    while (seqlst) { /* clear */

        p = seqlst; /* index top of list */
        seqlst = seqlst->next; /* gap out */
        putseq(p); /* release entry */

    }

}

/********************************************************************************

Get current time output

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

********************************************************************************/

int pa_curtimeout(void)

{

    if (!seqrun) error("Sequencer not running");

    return (difftime(strtim)); /* return difference time */

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

    error("pa_starttimein: Is not implemented");

}

/*******************************************************************************

Stop time input

Simply sets that we are not marking input time anymore.

*******************************************************************************/

void pa_stoptimein(void)

{

    error("pa_stoptimein: Is not implemented");

}

/*******************************************************************************

Get current time output

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

*******************************************************************************/

int pa_curtimein(void)

{

    error("pa_curtimein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/********************************************************************************

Note on

Turns on a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to INT_MAX.

********************************************************************************/

void pa_noteon(int p, int t, pa_channel c, pa_note n, int v)

{

    DWORD     msg;  /* message holder */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    pa_seqptr sp;   /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        msg = (v / 0x01000000)*65536+(n-1)*256+MESS_NOTE_ON+(c-1);
        midiOutShortMsg(midtab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_noteon; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntn = n; /* set note */
        sp->ntv = v; /* set velocity */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Note off

Turns off a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to INT_MAX.

********************************************************************************/

void pa_noteoff(int p, int t, pa_channel c, pa_note n, int v)

{

    DWORD     msg;
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    pa_seqptr sp;   /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        msg = (v / 0x01000000)*65536+(n-1)*256+MESS_NOTE_OFF+(c-1);
        midiOutShortMsg(midtab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_noteoff; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntn = n; /* set note */
        sp->ntv = v; /* set velocity */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Instrument change

Selects a new instrument for the given channel. Tbe new instrument is specified
by Midi GM encoding, 1 to 128. Takes a time for sequencing.

********************************************************************************/

void pa_instchange(int p, int t, pa_channel c, pa_instrument i)

{

    DWORD     msg;
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    pa_seqptr sp;   /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    if (i < 1 || i > 128) error("Bad instrument number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        msg = (i-1)*256+MESS_PGM_CHG+(c-1); /* construct midi message */
        midiOutShortMsg(midtab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_instchange; /* set type */
        sp->icc = c; /* set channel */
        sp->ici = i; /* set instrument */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Controller change

Processes a controller value set, from 0 to 127.

********************************************************************************/

static void ctlchg(int p, int t, pa_channel c, int cn, int v)

{

    DWORD msg;

    /* construct midi message */
    msg = v*65536+cn*256+MESS_CTRL_CHG+(c-1);
    midiOutShortMsg(midtab[p], msg);

}

/********************************************************************************

Set attack time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

********************************************************************************/

void pa_attack(int p, int t, pa_channel c, int at)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_SOUND_ATTACK_TIME, at/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_attack; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = at; /* set attack */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/ 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set release time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

********************************************************************************/

void pa_release(int p, int t, pa_channel c, int rt)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    DWORD     tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_SOUND_RELEASE_TIME, rt/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if p}ing timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_release; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = rt; /* set release */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Legato pedal on/off

Sets the legato mode on/off.

********************************************************************************/

void pa_legato(int p, int t, pa_channel c, int b)

{

    DWORD     msg;
    DWORD     elap;    /* current elapsed time */
    int       tact;    /* timer active */
    pa_seqptr sp;      /* message pointer */


    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_LEGATO_PEDAL, !!b*127);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_legato; /* set type */
        sp->bsc = c; /* set channel */
        sp->bsb = b; /* set on/off */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

   }

}

/********************************************************************************

Portamento pedal on/off

Sets the portamento mode on/off.

********************************************************************************/

void pa_portamento(int p, int t, pa_channel c, int b)

{

    DWORD     msg;
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */
    pa_seqptr sp;   /* message pointer */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_PORTAMENTO, !!b*127);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_portamento; /* set type */
        sp->bsc = c; /* set channel */
        sp->bsb = b; /* set on/off */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set volume

Sets synthesizer volume, 0 to INT_MAX.

********************************************************************************/

void pa_volsynthchan(int p, int t, pa_channel c, int v)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_VOLUME_COARSE, v/0x01000000); /* set high */
        ctlchg(p, t, c, CTLR_VOLUME_FINE, v/0x00020000 && 0x7f); /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_volsynthchan; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set volume */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

};

/********************************************************************************

Set left right channel balance

Sets sets the left right channel balance. -INT_MAX is all left, 0 is center,
INT_MAX is all right.

********************************************************************************/

void pa_balance(int p, int t, pa_channel c, int b)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */


    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        b = b/0x00040000+0x2000; /* reduce to 14 bits, positive only */
        ctlchg(p, t, c, CTLR_BALANCE_COARSE, b / 0x80); /* set high */
        ctlchg(p, t, c, CTLR_BALANCE_FINE, b && 0x7f); /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_balance; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = b; /* set balance */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set portamento time

Sets portamento time, 0 to INT_MAX.

********************************************************************************/

void pa_porttime(int p, int t, pa_channel c, int v)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_PORTAMENTO_TIME_COARSE, v/0x01000000); /* set high */
        /* set low */
        ctlchg(p, t, c, CTLR_PORTAMENTO_TIME_FINE, v/0x00020000 && 0x7f);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_porttime; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set time */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

   }

}

/********************************************************************************

Set vibrato

Sets modulaton value, 0 to INT_MAX.

********************************************************************************/

void pa_vibrato(int p, int t, pa_channel c, int v)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_MODULATION_WHEEL_COARSE, v/0x01000000); /* set high */
        /* set low */
        ctlchg(p, t, c, CTLR_MODULATION_WHEEL_FINE, v/0x00020000 && 0x7f);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_vibrato; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set vibrato */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set left/right pan position

Sets sets the left/right pan position. -INT_MAX is hard left, 0 is center,
INT_MAX is hard right.

********************************************************************************/

void pa_pan(int p, int t, pa_channel c, int b)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        b = b / 0x00040000+0x2000; /* reduce to 14 bits, positive only */
        ctlchg(p, t, c, CTLR_PAN_POSITION_COARSE, b/0x80); /* set high */
        ctlchg(p, t, c, CTLR_PAN_POSITION_FINE, b&0x7f); /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pan; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = b; /* set pan */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound timbre

Sets the sound timbre, 0 to INT_MAX.

********************************************************************************/

void pa_timbre(int p, int t, pa_channel c, int tb)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_SOUND_TIMBRE, tb/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_timbre; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = tb; /* set timbre */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                    TIME_CALLBACK_FUNCTION |
                                    TIME_KILL_SYNCHRONOUS |
                                    TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound brightness

Sets the sound brightness, 0 to INT_MAX.

********************************************************************************/

void pa_brightness(int p, int t, pa_channel c, int b)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_SOUND_BRIGHTNESS, b/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_brightness; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = b; /* set brightness */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound reverb

Sets the sound reverb, 0 to INT_MAX.

********************************************************************************/

void pa_reverb(int p, int t, pa_channel c, int r)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_EFFECTS_LEVEL, r/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_reverb; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = r; /* set reverb */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound tremulo

Sets the sound tremulo, 0 to INT_MAX.

********************************************************************************/

void pa_tremulo(int p, int t, pa_channel c, int tr)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_TREMULO_LEVEL, tr/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_tremulo; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = tr; /* set tremulo */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound chorus

Sets the sound chorus, 0 to INT_MAX.

********************************************************************************/

void pa_chorus(int p, int t, pa_channel c, int cr)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_CHORUS_LEVEL, cr/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_chorus; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = cr; /* set chorus */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound celeste

Sets the sound celeste, 0 to INT_MAX.

********************************************************************************/

void pa_celeste(int p, int t, pa_channel c, int ce)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_CELESTE_LEVEL, ce/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_celeste; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = ce; /* set celeste */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set sound phaser

Sets the sound phaser, 0 to INT_MAX.

********************************************************************************/

void pa_phaser(int p, int t, pa_channel c, int ph)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_PHASER_LEVEL, ph/0x01000000);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_phaser; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = ph; /* set phaser */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set pitch range

Sets the range of pitch that can be reached by the pitch adjustment. The range
is from 0 to INT_MAX, && represents from from 0 to 127 semitones. This means
that a setting of INT_MAX means that every note in the midi scale can be reached,
with an accuracy of 128 of the space from C to C#. At this setting, any note
could be reached with a slide, for example.

********************************************************************************/

void pa_pitchrange(int p, int t, pa_channel c, int v)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* set up data entry */
        ctlchg(p, t, c, CTLR_REGISTERED_PARAMETER_COARSE, 0); /* set high */
        ctlchg(p, t, c, CTLR_REGISTERED_PARAMETER_FINE, 0); /* set low */
        ctlchg(p, t, c, CTLR_DATA_ENTRY_COARSE, v/0x01000000); /* set high */
        ctlchg(p, t, c, CTLR_DATA_ENTRY_FINE, v/0x00020000 & 0x7f); /* set low */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pitchrange; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = v; /* set pitchrange */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set monophonic mode

If ommni is off, this sets how many channels to respond to. If omni is on,
then only once note at a time will be played. The select is from 0 to 16,
with 0 being "allways select single note mode".

********************************************************************************/

void pa_mono(int p, int t, pa_channel c, int ch)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    if (ch < 0 || c > 16) error("Bad mono mode number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_MONO_OPERATION, ch);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_mono; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = ch; /* set mono mode */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set polyphonic mode

Reenables polyphonic mode after a monophonic operation.

********************************************************************************/

void pa_poly(int p, int t, pa_channel c)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        ctlchg(p, t, c, CTLR_POLY_OPERATION, 0); /* value dosen't matter */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_poly; /* set type */
        sp->pc = c; /* set channel */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Aftertouch

Controls aftertouch, 0 to INT_MAX, on a note.

********************************************************************************/

void pa_aftertouch(int p, int t, pa_channel c, pa_note n, int at)

{

    DWORD msg;
    pa_seqptr sp; /* message pointer */
    DWORD elap;   /* current elapsed time */
    int tact;     /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    if (n < 1 || n > 128) error("Bad note number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        msg = (at/0x01000000)*65536+(n-1)*256+MESS_AFTTCH+(c-1);
        midiOutShortMsg(midtab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_aftertouch; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntn = n; /* set note */
        sp->ntv = at; /* set aftertouch */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Channel pressure

Controls channel pressure, 0 to INT_MAX, on a note.

********************************************************************************/

void pa_pressure(int p, int t, pa_channel c, int pr)

{

    DWORD     msg;
    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        /* construct midi message */
        msg = (pr/0x01000000)*256+MESS_CHN_PRES+(c-1);
        midiOutShortMsg(midtab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pressure; /* set type */
        sp->ntc = c; /* set channel */
        sp->ntv = pr; /* set pressure */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Set pitch wheel

Sets the pitch wheel value, from 0 to INT_MAX. This is the amount off the note
in the channel. The GM standard is to adjust for a whole step up && down, which
is 4 half steps total. A "half step" is the difference between, say, C && C#.

********************************************************************************/

void pa_pitch(int p, int t, pa_channel c, int pt)

{

    DWORD     msg;
    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (c < 1 || c > 16) error("Bad channel number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        pt = pt/0x00040000+0x2000; /* reduce to 14 bits, positive only */
        /* construct midi message */
        msg = (pt/0x80)*65536+(pt && 0x7f)*256+MESS_PTCH_WHL+(c-1);
        midiOutShortMsg(midtab[p], msg);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_pitch; /* set type */
        sp->vsc = c; /* set channel */
        sp->vsv = pt; /* set pitch */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/*******************************************************************************

Load synthesizer file

Loads a synthesizer control file, usually midi format, into a logical cache,
from 1 to N. These are loaded up into memory for minimum latency. The file is
specified by file name, and the file type is system dependent.

Note that we support 100 synth files loaded, but the Petit-ami "rule of thumb"
is no more than 10 synth files at a time.

Windows does not need to preload files (although we could find later that a
delay to play might require it). This implementation just saves the name for
the subsequent play operation.

*******************************************************************************/

void pa_loadsynth(int s, string fn)

{

    if (s < 1 || s > MAXMIDT) error("Invalid logical synthesizer file number");
    if (synthnam[s-1])
        error("Synthesizer file already defined for logical number");
    /* simply store the filename for later */
    synthnam[s-1] = malloc(strlen(fn)+1); /* get a name entry */
    if (!synthnam[s-1]) error("No memory");
    strcpy(synthnam[s-1], fn);

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

    if (s < 1 || s > MAXMIDT) error("Invalid logical synthesizer file number");
    if (!synthnam[s-1])
        error("No synthesizer file loaded for logical number");
    free(synthnam[s-1]);

}

/********************************************************************************

Play synthesizer file

Plays the waveform file to the indicated midi device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows midi files to be sequenced against other wave files and midi files.
The file is specified by file name, end the file type is system dependent.
This version uses the string send MCI command, one of the simplest ways to do
that. There does not seem to be an obvious way to relate the MCI devices to the
midi devices.

Windows cannot play more than one midi file at a time (although it can layer
one wave with one midi). Also, a midi open/close sequence like we use here will
fail if the default synth is open. We handle this by closing the default if
it is open, then reopening it afterwards.

********************************************************************************/

void pa_playsynth(int p, int t, int s)

{

    char      sfb[100], b[100];
    int       x;
    MCIERROR  r; /* return */
    char      fp[100], fn[100], fe[100]; /* filename components */
    pa_seqptr sp;  /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */

    if (p != 1) error("Must execute play on default output channel");
    if (midtab[p] < 0) error("Synth output channel not open");
    if (s < 1 || s > MAXMIDT) error("Invalid logical synthesizer file number");
    if (!synthnam[s-1])
        error("No synthesizer file loaded for logical wave number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        pa_closesynthout(1); /* close default output */
        /* break filename components */
        brknam(synthnam[s-1], fp, 100, fn, 100, fe, 100);
        if (fe[0] == ' ') strcpy(fe, "mid");
        maknam(sfb, 100, fp, fn, fe);
        mciSendString("close midi", NULL, 0, NULL);
        strcpy(b, "open ");
        strcat(b, sfb);
        strcat(b, " alias midi");
        mciSendString(b, NULL, 0, NULL);
        mciSendString("play midi", NULL, 0, NULL);
        pa_opensynthout(1); /* reopen default midi */

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_playsynth; /* set type */
        sp->sid = s; /* place synth file id */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap)/10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

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

    TCHAR status[100];

    /* This is a real waste of time but it works. Looking for a better
       solution */
    do { mciSendString("status midi mode", status, 100, NULL);
    } while (!strcmp(status, "playing"));

}

/********************************************************************************

Find number of wave devices.

Returns the number of wave output devices available. This is hardwared to 1 for
the one windows waveform device.

********************************************************************************/

int pa_waveout(void)

{

    return (1);

}

/*******************************************************************************

Find number of input wave devices.

Returns the number of wave output devices available.

*******************************************************************************/

int pa_wavein(void)

{

    error("pa_wavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/********************************************************************************

Open wave output device

Opens a wave output device by number. By convention, wave out 1 is the default
output device. This is presently a no-op for windows.

********************************************************************************/

void pa_openwaveout(int p)

{

}

/********************************************************************************

CLose wave output device

Closes a wave output device by number. This is presently a no-op for windows.

********************************************************************************/

void pa_closewaveout(int p)

{

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

    if (w < 1 || w > MAXWAVT) error("Invalid logical wave file number");
    if (synthnam[w-1])
        error("Wave file already defined for logical number");
    /* simply store the filename for later */
    wavenam[w-1] = malloc(strlen(fn)+1); /* get a name entry */
    if (!wavenam[w-1]) error("No memory");
    strcpy(wavenam[w-1], fn);

}

/*******************************************************************************

Delete waveform file

Removes a waveform file from the caching table. This frees up the entry to be
redefined.

*******************************************************************************/

void pa_delwave(int w)

{

    if (w < 1 || w > MAXWAVT) error("Invalid logical wave file number");
    if (!wavenam[w-1])
        error("No wave file loaded for logical number");
    free(synthnam[w-1]);

}

/********************************************************************************

Play waveform file

Plays the waveform file to the indicated wave device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows wave files to be sequenced against other wave files and midi files.
The file is specified by file name, and the file type is system dependent.

********************************************************************************/

void pa_playwave(int p, int t, int w)

{

    pa_seqptr sp;   /* message pointer */
    DWORD     elap; /* current elapsed time */
    int       tact; /* timer active */


    if (w < 1 || w > MAXWAVT) error("Invalid logical wave file number");
    if (!wavenam[w-1])
        error("No wave file loaded for logical number");
    elap = difftime(strtim); /* find elapsed time */
    /* execute immediate if 0 or sequencer running and time past */
    if (t == 0 || (t <= elap && seqrun)) {

        PlaySound(wavenam[w-1], 0, SND_FILENAME | SND_NODEFAULT | SND_ASYNC);

    } else { /* sequence */

        /* check sequencer running */
        if (!seqrun) error("Sequencer not running");
        tact = seqlst != NULL; /* flag if pending timers */
        getseq(&sp); /* get a sequencer message */
        sp->port = p; /* set port */
        sp->time = t; /* set time */
        sp->st = st_playwave; /* set type */
        sp->wt = w; /* set logical track */
        insseq(sp); /* insert to sequencer list */
        if (!tact) /* activate timer */
            timhan = timeSetEvent((t-elap) / 10, 0, nextseq, 0,
                                   TIME_CALLBACK_FUNCTION |
                                   TIME_KILL_SYNCHRONOUS |
                                   TIME_ONESHOT);

    }

}

/********************************************************************************

Adjust waveform volume

Adjusts the volume on waveform playback. The volume value is from 0 to INT_MAX.

********************************************************************************/

void pa_volwave(int p, int t, int v)

{

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

    error("pa_waitwave: Is not implemented");

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

    error("pa_chanwaveout: Is not implemented");

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

    error("pa_ratewaveout: Is not implemented");

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

    error("pa_lenwaveout: Is not implemented");

}

/*******************************************************************************

Set sign of wave output device samples

The given port has its signed/no signed status changed. Note that all floating
point formats are inherently signed.

*******************************************************************************/

void pa_sgnwaveout(int p, int s)

{

    error("pa_sgnwaveout: Is not implemented");

}

/*******************************************************************************

Set floating/non-floating point format

Sets the floating point/integer format for output sound samples.

*******************************************************************************/

void pa_fltwaveout(int p, int f)

{

    error("pa_fltwaveout: Is not implemented");

}

/*******************************************************************************

Set big/little endian format

Sets the big or little endian format for an output wave device. It is possible
that an installation is fixed to the endian format of the host machine, in which
case it is an error to set a format that is different.

*******************************************************************************/

void pa_endwaveout(int p, int e)

{

    error("pa_endwaveout: Is not implemented");

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

    error("pa_wrwave: Is not implemented");

}

/*******************************************************************************

Open wave input device

Opens a wave output device by number. By convention, wave in 1 is the default
input device. The wave stream parameters are determined during enumeration,
but we assert them here on open.

*******************************************************************************/

void pa_openwavein(int p)

{

    error("pa_openwavein: Is not implemented");

}

/*******************************************************************************

CLose wave input device

Closes a wave input device by number. This is presently a no-op for linux.

*******************************************************************************/

void pa_closewavein(int p)

{

    error("pa_closewavein: Is not implemented");

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

    error("pa_chanwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get the rate for a wave input device

The given port will have its rate read and returned, which is the
number of samples per second that will be input. It must be a wave output port,
and it must be open. Input samples are timed at the rate.

*******************************************************************************/

int pa_ratewavein(int p)

{

    error("pa_ratewavein: Is not implemented");

    return (1); /* this just shuts up compiler */

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

    error("pa_lenwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get signed status of wave input device

Returns true if the given wave input device has signed sampling. Note that
signed sampling is always true if the samples are floating point.

*******************************************************************************/

int pa_sgnwavein(int p)

{

    error("pa_sgnwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get big endian status of wave input device

Returns true if the given wave input device has big endian sampling.

*******************************************************************************/

int pa_endwavein(int p)

{

    error("pa_endwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Get floating point status of wave input device

Returns true if the given wave input device has floating point sampling.

*******************************************************************************/

int pa_fltwavein(int p)

{

    error("pa_fltwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

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

    error("pa_rdwave: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Find device name of synthesizer output port

Returns the ALSA device name of the given synthsizer output port.

*******************************************************************************/

void pa_synthoutname(int p, string name, int len)

{

    error("pa_synthoutname: Is not implemented");

}

/*******************************************************************************

Find device name of synthesizer input port

Returns the ALSA device name of the given synthsizer input port.

*******************************************************************************/

void pa_synthinname(int p, string name, int len)

{

    error("pa_synthinname: Is not implemented");

}

/*******************************************************************************

Find device name of wave output port

Returns the ALSA device name of the given wave output port.

*******************************************************************************/

void pa_waveoutname(int p, string name, int len)

{

    error("pa_waveoutname: Is not implemented");

}

/*******************************************************************************

Find device name of wave input port

Returns the ALSA device name of the given wave input port.

*******************************************************************************/

void pa_waveinname(int p, string name, int len)

{

    error("pa_waveinname: Is not implemented");

}

/*******************************************************************************

Open a synthesizer input port.

The given synthesizer port is opened and ready for reading.

*******************************************************************************/

void pa_opensynthin(int p)

{

    error("pa_opensynthin: Is not implemented");

}

/*******************************************************************************

Close a synthesizer input port

Closes the given synthsizer port for reading.

*******************************************************************************/

void pa_closesynthin(int p)

{

    error("pa_closesynthin: Is not implemented");

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

void pa_wrsynth(int p, pa_seqptr sp)

{

    error("pa_wrsynth: Is not implemented");

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

void pa_rdsynth(int p, pa_seqptr sp)

{

    error("pa_rdsynth: Is not implemented");

}

/*******************************************************************************

Get device parameter synth out

Reads a device parameter by name. Device parameters are strings indexed by name.
The device parameter is returned if it exists, otherwise an empty string is
returned.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

void pa_getparamsynthout(int p, string name, string value, int len)

{

    error("pa_getparamsynthout: Is not implemented");

}

/*******************************************************************************

Get device parameter synth in

Reads a device parameter by name. Device parameters are strings indexed by name.
The device parameter is returned if it exists, otherwise an empty string is
returned.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

void pa_getparamsynthin(int p, string name, string value, int len)

{

    error("pa_getparamsynthin: Is not implemented");

}

/*******************************************************************************

Get device parameter wave out

Reads a device parameter by name. Device parameters are strings indexed by name.
The device parameter is returned if it exists, otherwise an empty string is
returned.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

void pa_getparamwaveout(int p, string name, string value, int len)

{

    error("pa_getparamwaveout: Is not implemented");

}

/*******************************************************************************

Get device parameter wave in

Reads a device parameter by name. Device parameters are strings indexed by name.
The device parameter is returned if it exists, otherwise an empty string is
returned.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

void pa_getparamwavein(int p, string name, string value, int len)

{

    error("pa_getparamwavein: Is not implemented");

}

/*******************************************************************************

Set device parameter synth out

Sets a device parameter by name. Device parameters are strings indexed by name.
The device parameter is set if it exists, otherwise an error results.
If the parameter was successfully set, a 0 is returned, otherwise 1.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

int pa_setparamsynthout(int p, string name, string value)

{

    error("pa_setparamsynthout: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Set device parameter synth in

Sets a device parameter by name. Device parameters are strings indexed by name.
The device parameter is set if it exists, otherwise an error results.
If the parameter was successfully set, a 0 is returned, otherwise 1.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

int pa_setparamsynthin(int p, string name, string value)

{

    error("pa_setparamsynthin: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Set device parameter wave out

Sets a device parameter by name. Device parameters are strings indexed by name.
The device parameter is set if it exists, otherwise an error results.
If the parameter was successfully set, a 0 is returned, otherwise 1.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

int pa_setparamwaveout(int p, string name, string value)

{

    error("pa_setparamwaveout: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Set device parameter wave in

Sets a device parameter by name. Device parameters are strings indexed by name.
The device parameter is set if it exists, otherwise an error results.
If the parameter was successfully set, a 0 is returned, otherwise 1.

Device parameters are generally implemented for plug-ins only. The set of
parameters implemented on a particular device are dependent on that device.

*******************************************************************************/

int pa_setparamwavein(int p, string name, string value)

{

    error("pa_setparamwavein: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Initialize sound module

Clears sequencer lists, flags no timer active, clears the midi output port
table, and initializes the sequencer task mutex.

*******************************************************************************/

static void pa_init_sound (void) __attribute__((constructor (102)));
static void pa_init_sound()

{

    int i;

    seqlst = NULL; /* clear active sequencer list */
    seqfre = NULL; /* clear free sequencer messages */
    seqrun = FALSE; /* set sequencer ! running */
    strtim = 0; /* clear start time */
    timhan = 0; /* set no timer active */
    for (i = 0; i < MAXMIDP; i++) midtab[i] = (HMIDIOUT)-1; /* set no midi output ports open */
    for (i = 0; i < MAXMIDT; i++) synthnam[i] = NULL; /* clear synth track list */
    for (i = 0; i < MAXWAVT; i++) wavenam[i] = NULL; /* clear wave track list */
    InitializeCriticalSection(&seqlock); /* initialize the sequencer lock */

}
