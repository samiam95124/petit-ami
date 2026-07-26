/*******************************************************************************
*                                                                              *
*                                PETIT-AMI                                     *
*                                                                              *
*                         MACOS SOUND MODULE                                   *
*                                                                              *
*                              Created 2026                                    *
*                                                                              *
*                               S. A. FRANCO                                   *
*                                                                              *
* macOS implementation of the Petit-Ami sound module using CoreMIDI,           *
* AudioToolbox (AUGraph for software MIDI synthesis, AudioQueue for wave       *
* streaming), and MusicPlayer for MIDI file playback.                          *
*                                                                              *
* Port 1 synth output is the built-in Apple DLS software synthesizer.          *
* Additional synth ports map to CoreMIDI destinations.                         *
* Wave output uses AudioQueue Services.                                        *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2026 - Scott A. Franco                                         *
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/event.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <CoreMIDI/CoreMIDI.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>

#include <sound.h>

/*******************************************************************************
*                                                                              *
*                              Constants                                       *
*                                                                              *
*******************************************************************************/

#define MAXMIDP  100  /* maximum MIDI ports */
#define MAXWAVP  100  /* maximum wave ports */
#define MAXMIDT  100  /* maximum loaded MIDI file cache entries */
#define MAXWAVT  100  /* maximum loaded wave file cache entries */
#define WAVBUFS    3  /* number of AudioQueue buffers */
#define WAVBUFSZ 4096 /* AudioQueue buffer size in bytes */
#define SEQTIM_IDENT 42 /* kqueue timer ident */

/* MIDI controller numbers */
#define CTLR_MODULATION_WHEEL_COARSE   1
#define CTLR_MODULATION_WHEEL_FINE    33
#define CTLR_DATA_ENTRY_COARSE         6
#define CTLR_DATA_ENTRY_FINE          38
#define CTLR_VOLUME_COARSE             7
#define CTLR_VOLUME_FINE              39
#define CTLR_BALANCE_COARSE            8
#define CTLR_BALANCE_FINE             40
#define CTLR_PAN_POSITION_COARSE      10
#define CTLR_PAN_POSITION_FINE        42
#define CTLR_PORTAMENTO_TIME_COARSE    5
#define CTLR_PORTAMENTO_TIME_FINE     37
#define CTLR_PORTAMENTO               65
#define CTLR_LEGATO_PEDAL             68
#define CTLR_SOUND_TIMBRE             71
#define CTLR_SOUND_RELEASE_TIME       72
#define CTLR_SOUND_ATTACK_TIME        73
#define CTLR_SOUND_BRIGHTNESS         74
#define CTLR_EFFECTS_LEVEL            91
#define CTLR_TREMULO_LEVEL            92
#define CTLR_CHORUS_LEVEL             93
#define CTLR_CELESTE_LEVEL            94
#define CTLR_PHASER_LEVEL             95
#define CTLR_REGISTERED_PARAMETER_COARSE  101
#define CTLR_REGISTERED_PARAMETER_FINE    100
#define CTLR_MONO_OPERATION          126
#define CTLR_POLY_OPERATION          127

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/*******************************************************************************
*                                                                              *
*                              Types                                           *
*                                                                              *
*******************************************************************************/

/* synth output port descriptor */
typedef struct {
    int          open;     /* port is open */
    int          issoft;   /* is the software DLS synth (port 1) */
    /* software synth state (port 1) */
    AUGraph      graph;
    AudioUnit    synthUnit;
    /* CoreMIDI external output (port 2+) */
    MIDIEndpointRef endpoint;
    MIDIPortRef     midiPort;
    /* plug-in overrides */
    void (*opn)(long p);
    void (*cls)(long p);
    void (*wrseq)(long p, ami_seqptr sp);
    long (*setparam)(long p, string name, string value);
    void (*getparam)(long p, string name, string value, long len);
    char name[256];
} synthoutdev;

/* synth input port descriptor */
typedef struct {
    int          open;
    MIDIEndpointRef endpoint;
    MIDIPortRef     midiPort;
    void (*opn)(long p);
    void (*cls)(long p);
    void (*rdseq)(long p, ami_seqptr sp);
    long (*setparam)(long p, string name, string value);
    void (*getparam)(long p, string name, string value, long len);
    char name[256];
} synthindev;

/* wave output port descriptor */
typedef struct {
    int           open;
    int           chan;    /* channels */
    int           rate;    /* sample rate */
    int           bits;    /* bits per sample */
    int           sgn;     /* signed */
    int           flt;     /* floating point */
    int           big;     /* big endian */
    AudioQueueRef queue;   /* AudioQueue for streaming */
    int           fmtset;  /* format needs (re-)configuration */
    void (*opn)(long p);
    void (*cls)(long p);
    void (*chanwavout)(long p, long c);
    void (*ratewavout)(long p, long r);
    void (*lenwavout)(long p, long l);
    void (*sgnwavout)(long p, long s);
    void (*fltwavout)(long p, long f);
    void (*endwaveout)(long p, long e);
    void (*wrwav)(long p, byte* buff, long len);
    long (*setparam)(long p, string name, string value);
    void (*getparam)(long p, string name, string value, long len);
    char name[256];
} waveoutdev;

/* wave input port descriptor */
typedef struct {
    int           open;
    int           chan;
    int           rate;
    int           bits;
    int           sgn;
    int           flt;
    int           big;
    AudioQueueRef queue;
    void (*opn)(long p);
    void (*cls)(long p);
    long (*chanwavin)(long p);
    long (*ratewavin)(long p);
    long (*lenwavin)(long p);
    long (*sgnwavin)(long p);
    long (*fltwavin)(long p);
    long (*endwavein)(long p);
    long (*rdwav)(long p, byte* buff, long len);
    long (*setparam)(long p, string name, string value);
    void (*getparam)(long p, string name, string value, long len);
    char name[256];
} waveindev;

/*******************************************************************************
*                                                                              *
*                           Global state                                       *
*                                                                              *
*******************************************************************************/

/* CoreMIDI client and output port (shared) */
static MIDIClientRef   midiClient;
static MIDIPortRef     midiOutPort;
static int             midiInited;

/* device tables */
static synthoutdev synthout_tab[MAXMIDP];
static int         synthout_num;
static int         synthout_plug; /* number of plug-in synth outs */

static synthindev  synthin_tab[MAXMIDP];
static int         synthin_num;
static int         synthin_plug;

static waveoutdev  waveout_tab[MAXWAVP];
static int         waveout_num;
static int         waveout_plug;

static waveindev   wavein_tab[MAXWAVP];
static int         wavein_num;
static int         wavein_plug;

/* loaded file caches */
static char* wavfil[MAXWAVT]; /* wave file name table */
static pthread_mutex_t wavlck = PTHREAD_MUTEX_INITIALIZER;

static char* synfil[MAXMIDT]; /* synth (MIDI) file name table */
static pthread_mutex_t synlck = PTHREAD_MUTEX_INITIALIZER;

/* active play counters */
static int numwav;
static pthread_mutex_t wnmlck = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  wnmzer = PTHREAD_COND_INITIALIZER;

static int numseq;
static pthread_mutex_t snmlck = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  snmzer = PTHREAD_COND_INITIALIZER;

/* sequencer state */
static ami_seqptr  seqlst;    /* pending event list (time-sorted) */
static ami_seqptr  seqfre;    /* recycled-entry free list */
static int         seqrun;    /* sequencer is active */
static struct timeval strtim; /* sequencer start time */
static int         seqtimact; /* timer is armed */
static int         seqhan;    /* kqueue fd */
static pthread_mutex_t seqlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t   seqthread;
static int         seqthread_running;

/* input timing */
static int seqrunin;
static struct timeval sintim;

/* module init */
static int inited;

/*******************************************************************************
*                                                                              *
*                          Error handling                                       *
*                                                                              *
*******************************************************************************/

static void error(const char* s)
{
    fprintf(stderr, "\nError: Sound: %s\n", s);
    exit(1);
}

/*******************************************************************************
*                                                                              *
*                          Time helpers                                         *
*                                                                              *
*******************************************************************************/

static long timediff(struct timeval* rt)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long ds = (long)(tv.tv_sec  - rt->tv_sec);
    long du = (long)(tv.tv_usec - rt->tv_usec);
    return ds * 10000 + du / 100;
}

/*******************************************************************************
*                                                                              *
*                      Sequencer infrastructure                                *
*                                                                              *
*******************************************************************************/

static void getseq(ami_seqptr* p)
{
    if (seqfre) {
        *p = seqfre;
        seqfre = seqfre->next;
    } else
        *p = (ami_seqptr)malloc(sizeof(ami_seqmsg));
    (*p)->next = NULL;
}

static void putseq(ami_seqptr p)
{
    p->next = seqfre;
    seqfre = p;
}

static void insseq(ami_seqptr sp)
{
    ami_seqptr p, l;

    pthread_mutex_lock(&seqlock);
    l = NULL;
    p = seqlst;
    while (p && p->time <= sp->time) { l = p; p = p->next; }
    sp->next = p;
    if (l) l->next = sp; else seqlst = sp;
    pthread_mutex_unlock(&seqlock);
}

/* forward declarations */
static void excseq(ami_seqptr sp);
static void playwave_kickoff(long port, long wt);
static void playsynth_kickoff(long port, long sid);

static void acttim(void)
{
    if (!seqtimact && seqlst) {
        long elap = timediff(&strtim);
        long tl = seqlst->time - elap;
        if (tl < 0) tl = 0;
        struct kevent kev;
        /* NOTE_NSECONDS: tl is in 100us units → convert to nanoseconds */
        uint64_t ns = (uint64_t)tl * 100000ULL;
        if (ns == 0) ns = 1;
        EV_SET(&kev, SEQTIM_IDENT, EVFILT_TIMER,
               EV_ADD | EV_ONESHOT, NOTE_NSECONDS, (intptr_t)ns, NULL);
        kevent(seqhan, &kev, 1, NULL, 0, NULL);
        seqtimact = TRUE;
    }
}

static void* sequencer_thread(void* arg)
{
    (void)arg;
    struct kevent kev;

    for (;;) {
        int n = kevent(seqhan, NULL, 0, &kev, 1, NULL);
        if (n < 0) break;
        if (n == 0) continue;

        pthread_mutex_lock(&seqlock);
        long elap = timediff(&strtim);
        while (seqlst && seqlst->time <= elap) {
            ami_seqptr sp = seqlst;
            seqlst = sp->next;
            sp->next = NULL;
            pthread_mutex_unlock(&seqlock);
            excseq(sp);
            putseq(sp);
            pthread_mutex_lock(&seqlock);
            elap = timediff(&strtim);
        }
        seqtimact = FALSE;
        if (seqlst)
            acttim();
        else {
            pthread_mutex_lock(&snmlck);
            numseq--;
            if (numseq <= 0) { numseq = 0; pthread_cond_broadcast(&snmzer); }
            pthread_mutex_unlock(&snmlck);
        }
        pthread_mutex_unlock(&seqlock);
    }
    return NULL;
}

static void ensure_seqthread(void)
{
    if (!seqthread_running) {
        seqhan = kqueue();
        if (seqhan < 0) error("Cannot create kqueue for sequencer");
        pthread_create(&seqthread, NULL, sequencer_thread, NULL);
        seqthread_running = TRUE;
    }
}

/* schedule or execute immediately */
static void seqorexe(ami_seqptr sp)
{
    if (sp->time == 0 || !seqrun) {
        excseq(sp);
        putseq(sp);
    } else {
        long elap = timediff(&strtim);
        if (sp->time <= elap) {
            excseq(sp);
            putseq(sp);
        } else {
            insseq(sp);
            pthread_mutex_lock(&seqlock);
            acttim();
            pthread_mutex_unlock(&seqlock);
        }
    }
}

/*******************************************************************************
*                                                                              *
*                       MIDI message helpers                                   *
*                                                                              *
*******************************************************************************/

static void midisend(long port, UInt32 status, UInt32 data1, UInt32 data2)
{
    synthoutdev* d = &synthout_tab[port - 1];
    if (!d->open) return;

    if (d->issoft) {
        MusicDeviceMIDIEvent(d->synthUnit, status, data1, data2, 0);
    } else if (d->endpoint) {
        Byte buf[3] = { (Byte)status, (Byte)data1, (Byte)data2 };
        MIDIPacketList pktList;
        MIDIPacket* pkt = MIDIPacketListInit(&pktList);
        int len = (status >= 0xC0 && status < 0xE0) ? 2 : 3;
        MIDIPacketListAdd(&pktList, sizeof(pktList), pkt, 0, len, buf);
        MIDISend(d->midiPort, d->endpoint, &pktList);
    }
}

static void ctlchg(long port, ami_channel c, int cn, int v)
{
    midisend(port, 0xB0 + (c - 1), cn, v);
}

/*******************************************************************************
*                                                                              *
*                  Execute sequencer entry                                     *
*                                                                              *
*******************************************************************************/

static void excseq(ami_seqptr sp)
{
    long b, pt;

    /* if the port has a plug-in wrseq, use it */
    if (sp->port >= 1 && sp->port <= synthout_num &&
        synthout_tab[sp->port - 1].wrseq) {
        synthout_tab[sp->port - 1].wrseq(sp->port, sp);
        return;
    }

    switch (sp->st) {
    case st_noteon:
        midisend(sp->port, 0x90 + (sp->ntc - 1),
                 sp->ntn - 1, sp->ntv / (LONG_MAX/128+1));
        break;
    case st_noteoff:
        midisend(sp->port, 0x80 + (sp->ntc - 1),
                 sp->ntn - 1, sp->ntv / (LONG_MAX/128+1));
        break;
    case st_instchange:
        midisend(sp->port, 0xC0 + (sp->icc - 1), sp->ici - 1, 0);
        break;
    case st_attack:
        ctlchg(sp->port, sp->vsc, CTLR_SOUND_ATTACK_TIME,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_release:
        ctlchg(sp->port, sp->vsc, CTLR_SOUND_RELEASE_TIME,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_legato:
        ctlchg(sp->port, sp->bsc, CTLR_LEGATO_PEDAL, !!sp->bsb * 127);
        break;
    case st_portamento:
        ctlchg(sp->port, sp->bsc, CTLR_PORTAMENTO, !!sp->bsb * 127);
        break;
    case st_vibrato:
        ctlchg(sp->port, sp->vsc, CTLR_MODULATION_WHEEL_COARSE,
               sp->vsv / (LONG_MAX/128+1));
        ctlchg(sp->port, sp->vsc, CTLR_MODULATION_WHEEL_FINE,
               sp->vsv / (LONG_MAX/16384+1) & 0x7f);
        break;
    case st_volsynthchan:
        ctlchg(sp->port, sp->vsc, CTLR_VOLUME_COARSE,
               sp->vsv / (LONG_MAX/128+1));
        ctlchg(sp->port, sp->vsc, CTLR_VOLUME_FINE,
               sp->vsv / (LONG_MAX/16384+1) & 0x7f);
        break;
    case st_porttime:
        ctlchg(sp->port, sp->vsc, CTLR_PORTAMENTO_TIME_COARSE,
               sp->vsv / (LONG_MAX/128+1));
        ctlchg(sp->port, sp->vsc, CTLR_PORTAMENTO_TIME_FINE,
               sp->vsv / (LONG_MAX/16384+1) & 0x7f);
        break;
    case st_balance:
        b = sp->vsv / (LONG_MAX/8192+1) + 0x2000;
        ctlchg(sp->port, sp->vsc, CTLR_BALANCE_COARSE, b / 0x80);
        ctlchg(sp->port, sp->vsc, CTLR_BALANCE_FINE, b & 0x7f);
        break;
    case st_pan:
        b = sp->vsv / (LONG_MAX/8192+1) + 0x2000;
        ctlchg(sp->port, sp->vsc, CTLR_PAN_POSITION_COARSE, b / 0x80);
        ctlchg(sp->port, sp->vsc, CTLR_PAN_POSITION_FINE, b & 0x7f);
        break;
    case st_timbre:
        ctlchg(sp->port, sp->vsc, CTLR_SOUND_TIMBRE,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_brightness:
        ctlchg(sp->port, sp->vsc, CTLR_SOUND_BRIGHTNESS,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_reverb:
        ctlchg(sp->port, sp->vsc, CTLR_EFFECTS_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_tremulo:
        ctlchg(sp->port, sp->vsc, CTLR_TREMULO_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_chorus:
        ctlchg(sp->port, sp->vsc, CTLR_CHORUS_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_celeste:
        ctlchg(sp->port, sp->vsc, CTLR_CELESTE_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_phaser:
        ctlchg(sp->port, sp->vsc, CTLR_PHASER_LEVEL,
               sp->vsv / (LONG_MAX/128+1));
        break;
    case st_aftertouch:
        midisend(sp->port, 0xA0 + (sp->ntc - 1),
                 sp->ntn - 1, sp->ntv / (LONG_MAX/128+1));
        break;
    case st_pressure:
        midisend(sp->port, 0xD0 + (sp->ntc - 1),
                 sp->ntv / (LONG_MAX/128+1), 0);
        break;
    case st_pitch:
        pt = sp->vsv / (LONG_MAX/8192+1) + 0x2000;
        midisend(sp->port, 0xE0 + (sp->vsc - 1), pt & 0x7f, pt / 0x80);
        break;
    case st_pitchrange:
        ctlchg(sp->port, sp->vsc, CTLR_REGISTERED_PARAMETER_COARSE, 0);
        ctlchg(sp->port, sp->vsc, CTLR_REGISTERED_PARAMETER_FINE, 0);
        ctlchg(sp->port, sp->vsc, CTLR_DATA_ENTRY_COARSE,
               sp->vsv / (LONG_MAX/128+1));
        ctlchg(sp->port, sp->vsc, CTLR_DATA_ENTRY_FINE,
               sp->vsv / (LONG_MAX/16384+1) & 0x7f);
        break;
    case st_mono:
        ctlchg(sp->port, sp->vsc, CTLR_MONO_OPERATION, sp->vsv);
        break;
    case st_poly:
        ctlchg(sp->port, sp->pc, CTLR_POLY_OPERATION, 0);
        break;
    case st_playsynth:
        playsynth_kickoff(sp->port, sp->sid);
        break;
    case st_playwave:
        playwave_kickoff(sp->port, sp->wt);
        break;
    case st_volwave:
        /* not implemented */
        break;
    }
}

void _pa_excseq(long p, ami_seqptr sp)
{
    excseq(sp);
}

/*******************************************************************************
*                                                                              *
*                    Device enumeration & init                                  *
*                                                                              *
*******************************************************************************/

static void init_coremidi(void)
{
    if (midiInited) return;
    OSStatus st = MIDIClientCreate(CFSTR("PetitAmi"), NULL, NULL, &midiClient);
    if (st != noErr) return;
    MIDIOutputPortCreate(midiClient, CFSTR("PA Output"), &midiOutPort);
    midiInited = TRUE;
}

static void enumerate_devices(void)
{
    init_coremidi();

    /* port 1 = built-in DLS software synth (always present) */
    synthout_tab[0].issoft = TRUE;
    snprintf(synthout_tab[0].name, sizeof(synthout_tab[0].name),
             "Apple DLS Music Device");
    synthout_num = 1;

    /* additional ports = CoreMIDI destinations */
    ItemCount nd = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < nd && synthout_num < MAXMIDP; i++) {
        MIDIEndpointRef ep = MIDIGetDestination(i);
        if (!ep) continue;
        synthoutdev* d = &synthout_tab[synthout_num];
        d->endpoint = ep;
        CFStringRef cfname = NULL;
        MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &cfname);
        if (cfname) {
            CFStringGetCString(cfname, d->name, sizeof(d->name),
                               kCFStringEncodingUTF8);
            CFRelease(cfname);
        } else
            snprintf(d->name, sizeof(d->name), "MIDI Out %d",
                     synthout_num + 1);
        synthout_num++;
    }

    /* synth input = CoreMIDI sources */
    ItemCount ns = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < ns && synthin_num < MAXMIDP; i++) {
        MIDIEndpointRef ep = MIDIGetSource(i);
        if (!ep) continue;
        synthindev* d = &synthin_tab[synthin_num];
        d->endpoint = ep;
        CFStringRef cfname = NULL;
        MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &cfname);
        if (cfname) {
            CFStringGetCString(cfname, d->name, sizeof(d->name),
                               kCFStringEncodingUTF8);
            CFRelease(cfname);
        } else
            snprintf(d->name, sizeof(d->name), "MIDI In %d",
                     synthin_num + 1);
        synthin_num++;
    }

    /* wave output: always 1 default device */
    snprintf(waveout_tab[0].name, sizeof(waveout_tab[0].name),
             "Default Audio Output");
    waveout_tab[0].chan = 2;
    waveout_tab[0].rate = 44100;
    waveout_tab[0].bits = 16;
    waveout_tab[0].sgn  = TRUE;
    waveout_tab[0].flt  = FALSE;
    waveout_tab[0].big  = FALSE;
    waveout_num = 1;

    /* wave input: always 1 default device */
    snprintf(wavein_tab[0].name, sizeof(wavein_tab[0].name),
             "Default Audio Input");
    wavein_tab[0].chan = 1;
    wavein_tab[0].rate = 44100;
    wavein_tab[0].bits = 16;
    wavein_tab[0].sgn  = TRUE;
    wavein_tab[0].flt  = FALSE;
    wavein_tab[0].big  = FALSE;
    wavein_num = 1;
}

__attribute__((constructor(103)))
static void ami_init_sound(void)
{
    if (inited) return;
    inited = TRUE;
    enumerate_devices();
}

/*******************************************************************************
*                                                                              *
*                        Sequencer timing                                      *
*                                                                              *
*******************************************************************************/

void ami_starttimeout(void)
{
    ensure_seqthread();
    gettimeofday(&strtim, NULL);
    seqrun = TRUE;
}

void ami_stoptimeout(void)
{
    seqrun = FALSE;
    pthread_mutex_lock(&seqlock);
    /* drain the sequencer list */
    while (seqlst) {
        ami_seqptr sp = seqlst;
        seqlst = sp->next;
        putseq(sp);
    }
    seqtimact = FALSE;
    pthread_mutex_unlock(&seqlock);
}

long ami_curtimeout(void)
{
    if (!seqrun) return 0;
    return timediff(&strtim);
}

void ami_starttimein(void)
{
    gettimeofday(&sintim, NULL);
    seqrunin = TRUE;
}

void ami_stoptimein(void)
{
    seqrunin = FALSE;
}

long ami_curtimein(void)
{
    if (!seqrunin) return 0;
    return timediff(&sintim);
}

/*******************************************************************************
*                                                                              *
*                     Port enumeration                                         *
*                                                                              *
*******************************************************************************/

long ami_synthout(void)  { return synthout_num; }
long ami_synthin(void)   { return synthin_num; }
long ami_waveout(void)   { return waveout_num; }
long ami_wavein(void)    { return wavein_num; }

void ami_synthoutname(long p, string name, long len)
{
    if (p < 1 || p > synthout_num) { if (len > 0) name[0] = 0; return; }
    strncpy(name, synthout_tab[p - 1].name, len);
    if (len > 0) name[len - 1] = 0;
}

void ami_synthinname(long p, string name, long len)
{
    if (p < 1 || p > synthin_num) { if (len > 0) name[0] = 0; return; }
    strncpy(name, synthin_tab[p - 1].name, len);
    if (len > 0) name[len - 1] = 0;
}

void ami_waveoutname(long p, string name, long len)
{
    if (p < 1 || p > waveout_num) { if (len > 0) name[0] = 0; return; }
    strncpy(name, waveout_tab[p - 1].name, len);
    if (len > 0) name[len - 1] = 0;
}

void ami_waveinname(long p, string name, long len)
{
    if (p < 1 || p > wavein_num) { if (len > 0) name[0] = 0; return; }
    strncpy(name, wavein_tab[p - 1].name, len);
    if (len > 0) name[len - 1] = 0;
}

/*******************************************************************************
*                                                                              *
*                    Synth output open/close                                   *
*                                                                              *
*******************************************************************************/

void ami_opensynthout(long p)
{
    if (p < 1 || p > synthout_num) error("Invalid synth output port");
    synthoutdev* d = &synthout_tab[p - 1];
    if (d->open) return;

    if (d->opn) { d->opn(p); d->open = TRUE; return; }

    if (d->issoft) {
        /* create AUGraph: DLSSynth → DefaultOutput */
        OSStatus st;
        st = NewAUGraph(&d->graph);
        if (st != noErr) error("Cannot create AUGraph");

        AudioComponentDescription synthDesc = {
            .componentType = kAudioUnitType_MusicDevice,
            .componentSubType = kAudioUnitSubType_DLSSynth,
            .componentManufacturer = kAudioUnitManufacturer_Apple
        };
        AudioComponentDescription outDesc = {
            .componentType = kAudioUnitType_Output,
            .componentSubType = kAudioUnitSubType_DefaultOutput,
            .componentManufacturer = kAudioUnitManufacturer_Apple
        };

        AUNode synthNode, outNode;
        AUGraphAddNode(d->graph, &synthDesc, &synthNode);
        AUGraphAddNode(d->graph, &outDesc, &outNode);
        AUGraphConnectNodeInput(d->graph, synthNode, 0, outNode, 0);
        AUGraphOpen(d->graph);
        AUGraphNodeInfo(d->graph, synthNode, NULL, &d->synthUnit);
        AUGraphInitialize(d->graph);
        AUGraphStart(d->graph);
    } else {
        /* external CoreMIDI destination: port already created in enumerate */
        d->midiPort = midiOutPort;
    }
    d->open = TRUE;
}

void ami_closesynthout(long p)
{
    if (p < 1 || p > synthout_num) return;
    synthoutdev* d = &synthout_tab[p - 1];
    if (!d->open) return;

    if (d->cls) { d->cls(p); d->open = FALSE; return; }

    if (d->issoft && d->graph) {
        AUGraphStop(d->graph);
        AUGraphUninitialize(d->graph);
        AUGraphClose(d->graph);
        DisposeAUGraph(d->graph);
        d->graph = NULL;
        d->synthUnit = NULL;
    }
    d->open = FALSE;
}

/*******************************************************************************
*                                                                              *
*                    Synth input open/close                                    *
*                                                                              *
*******************************************************************************/

void ami_opensynthin(long p)
{
    if (p < 1 || p > synthin_num) error("Invalid synth input port");
    synthindev* d = &synthin_tab[p - 1];
    if (d->open) return;
    if (d->opn) { d->opn(p); d->open = TRUE; return; }
    /* CoreMIDI input port: create if needed */
    /* TODO: implement CoreMIDI input with callback */
    d->open = TRUE;
}

void ami_closesynthin(long p)
{
    if (p < 1 || p > synthin_num) return;
    synthindev* d = &synthin_tab[p - 1];
    if (!d->open) return;
    if (d->cls) { d->cls(p); d->open = FALSE; return; }
    d->open = FALSE;
}

/*******************************************************************************
*                                                                              *
*                    MIDI note/control messages                                *
*                                                                              *
*******************************************************************************/

void ami_noteon(long p, long t, ami_channel c, ami_note n, long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_noteon;
    sp->ntc = c; sp->ntn = n; sp->ntv = v;
    seqorexe(sp);
}

void ami_noteoff(long p, long t, ami_channel c, ami_note n, long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_noteoff;
    sp->ntc = c; sp->ntn = n; sp->ntv = v;
    seqorexe(sp);
}

void ami_instchange(long p, long t, ami_channel c, ami_instrument i)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_instchange;
    sp->icc = c; sp->ici = i;
    seqorexe(sp);
}

void ami_attack(long p, long t, ami_channel c, long at)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_attack;
    sp->vsc = c; sp->vsv = at;
    seqorexe(sp);
}

void ami_release(long p, long t, ami_channel c, long rt)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_release;
    sp->vsc = c; sp->vsv = rt;
    seqorexe(sp);
}

void ami_legato(long p, long t, ami_channel c, long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_legato;
    sp->bsc = c; sp->bsb = b;
    seqorexe(sp);
}

void ami_portamento(long p, long t, ami_channel c, long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_portamento;
    sp->bsc = c; sp->bsb = b;
    seqorexe(sp);
}

void ami_vibrato(long p, long t, ami_channel c, long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_vibrato;
    sp->vsc = c; sp->vsv = v;
    seqorexe(sp);
}

void ami_volsynthchan(long p, long t, ami_channel c, long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_volsynthchan;
    sp->vsc = c; sp->vsv = v;
    seqorexe(sp);
}

void ami_porttime(long p, long t, ami_channel c, long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_porttime;
    sp->vsc = c; sp->vsv = v;
    seqorexe(sp);
}

void ami_balance(long p, long t, ami_channel c, long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_balance;
    sp->vsc = c; sp->vsv = b;
    seqorexe(sp);
}

void ami_pan(long p, long t, ami_channel c, long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_pan;
    sp->vsc = c; sp->vsv = b;
    seqorexe(sp);
}

void ami_timbre(long p, long t, ami_channel c, long tb)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_timbre;
    sp->vsc = c; sp->vsv = tb;
    seqorexe(sp);
}

void ami_brightness(long p, long t, ami_channel c, long b)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_brightness;
    sp->vsc = c; sp->vsv = b;
    seqorexe(sp);
}

void ami_reverb(long p, long t, ami_channel c, long r)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_reverb;
    sp->vsc = c; sp->vsv = r;
    seqorexe(sp);
}

void ami_tremulo(long p, long t, ami_channel c, long tr)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_tremulo;
    sp->vsc = c; sp->vsv = tr;
    seqorexe(sp);
}

void ami_chorus(long p, long t, ami_channel c, long cr)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_chorus;
    sp->vsc = c; sp->vsv = cr;
    seqorexe(sp);
}

void ami_celeste(long p, long t, ami_channel c, long ce)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_celeste;
    sp->vsc = c; sp->vsv = ce;
    seqorexe(sp);
}

void ami_phaser(long p, long t, ami_channel c, long ph)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_phaser;
    sp->vsc = c; sp->vsv = ph;
    seqorexe(sp);
}

void ami_aftertouch(long p, long t, ami_channel c, ami_note n, long at)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_aftertouch;
    sp->ntc = c; sp->ntn = n; sp->ntv = at;
    seqorexe(sp);
}

void ami_pressure(long p, long t, ami_channel c, long pr)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_pressure;
    sp->ntc = c; sp->ntn = 0; sp->ntv = pr;
    seqorexe(sp);
}

void ami_pitch(long p, long t, ami_channel c, long pt)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_pitch;
    sp->vsc = c; sp->vsv = pt;
    seqorexe(sp);
}

void ami_pitchrange(long p, long t, ami_channel c, long v)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_pitchrange;
    sp->vsc = c; sp->vsv = v;
    seqorexe(sp);
}

void ami_mono(long p, long t, ami_channel c, long ch)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_mono;
    sp->vsc = c; sp->vsv = ch;
    seqorexe(sp);
}

void ami_poly(long p, long t, ami_channel c)
{
    ami_seqptr sp;
    getseq(&sp);
    sp->port = p; sp->time = t; sp->st = st_poly;
    sp->pc = c;
    seqorexe(sp);
}

/*******************************************************************************
*                                                                              *
*                    Synth file (MIDI) playback                                *
*                                                                              *
*******************************************************************************/

void ami_loadsynth(long s, string fn)
{
    if (s < 1 || s > MAXMIDT) error("Invalid synth file slot");
    pthread_mutex_lock(&synlck);
    if (synfil[s - 1]) { free(synfil[s - 1]); synfil[s - 1] = NULL; }
    synfil[s - 1] = strdup(fn);
    pthread_mutex_unlock(&synlck);
}

void ami_delsynth(long s)
{
    if (s < 1 || s > MAXMIDT) error("Invalid synth file slot");
    pthread_mutex_lock(&synlck);
    if (synfil[s - 1]) { free(synfil[s - 1]); synfil[s - 1] = NULL; }
    pthread_mutex_unlock(&synlck);
}

/* thread function: play a MIDI file via MusicPlayer through the DLS synth */
static void* playsynth_thread(void* arg)
{
    long* args = (long*)arg;
    long port = args[0];
    long sid  = args[1];
    free(args);

    /* get the filename */
    pthread_mutex_lock(&synlck);
    if (sid < 1 || sid > MAXMIDT || !synfil[sid - 1]) {
        pthread_mutex_unlock(&synlck);
        goto done;
    }
    char* fn = strdup(synfil[sid - 1]);
    pthread_mutex_unlock(&synlck);

    /* create the MusicSequence and load the file */
    MusicSequence seq = NULL;
    MusicPlayer player = NULL;
    OSStatus st;

    st = NewMusicSequence(&seq);
    if (st != noErr) { free(fn); goto done; }

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (const UInt8*)fn, strlen(fn), false);
    free(fn);
    if (!url) { DisposeMusicSequence(seq); goto done; }

    st = MusicSequenceFileLoad(seq, url,
                               kMusicSequenceFile_MIDIType, 0);
    CFRelease(url);
    if (st != noErr) { DisposeMusicSequence(seq); goto done; }

    /* set the sequence to play through port 1's AUGraph if available */
    synthoutdev* d = &synthout_tab[port - 1];
    if (d->issoft && d->graph)
        MusicSequenceSetAUGraph(seq, d->graph);

    st = NewMusicPlayer(&player);
    if (st != noErr) { DisposeMusicSequence(seq); goto done; }

    MusicPlayerSetSequence(player, seq);
    MusicPlayerPreroll(player);
    MusicPlayerStart(player);

    /* wait for playback to finish */
    MusicTimeStamp seqLen = 0;
    UInt32 ntracks;
    MusicSequenceGetTrackCount(seq, &ntracks);
    for (UInt32 i = 0; i < ntracks; i++) {
        MusicTrack track;
        MusicSequenceGetIndTrack(seq, i, &track);
        MusicTimeStamp tlen;
        UInt32 sz = sizeof(tlen);
        MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength,
                              &tlen, &sz);
        if (tlen > seqLen) seqLen = tlen;
    }
    seqLen += 4; /* add a few beats for reverb tail */

    Boolean isPlaying = TRUE;
    while (isPlaying) {
        usleep(100000); /* 100ms poll */
        MusicTimeStamp now;
        MusicPlayerGetTime(player, &now);
        if (now >= seqLen) isPlaying = FALSE;
        MusicPlayerIsPlaying(player, &isPlaying);
    }

    MusicPlayerStop(player);
    DisposeMusicPlayer(player);
    DisposeMusicSequence(seq);

done:
    pthread_mutex_lock(&snmlck);
    numseq--;
    if (numseq <= 0) { numseq = 0; pthread_cond_broadcast(&snmzer); }
    pthread_mutex_unlock(&snmlck);
    return NULL;
}

static void playsynth_kickoff(long port, long sid)
{
    pthread_mutex_lock(&snmlck);
    numseq++;
    pthread_mutex_unlock(&snmlck);

    long* args = (long*)malloc(2 * sizeof(long));
    args[0] = port;
    args[1] = sid;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, playsynth_thread, args);
    pthread_attr_destroy(&attr);
}

void ami_playsynth(long p, long t, long s)
{
    if (p < 1 || p > synthout_num) error("Invalid synth port");
    if (s < 1 || s > MAXMIDT) error("Invalid synth file slot");
    if (t == 0 || !seqrun) {
        playsynth_kickoff(p, s);
    } else {
        ami_seqptr sp;
        getseq(&sp);
        sp->port = p; sp->time = t; sp->st = st_playsynth;
        sp->sid = s;
        seqorexe(sp);
    }
}

void ami_waitsynth(long p)
{
    pthread_mutex_lock(&snmlck);
    while (numseq > 0)
        pthread_cond_wait(&snmzer, &snmlck);
    pthread_mutex_unlock(&snmlck);
}

void ami_wrsynth(long p, ami_seqptr sp)
{
    if (p < 1 || p > synthout_num) error("Invalid synth port");
    sp->port = p;
    excseq(sp);
}

void ami_rdsynth(long p, ami_seqptr sp)
{
    if (p < 1 || p > synthin_num) error("Invalid synth input port");
    /* TODO: implement CoreMIDI input reading */
    memset(sp, 0, sizeof(ami_seqmsg));
}

/*******************************************************************************
*                                                                              *
*                    Wave output open/close                                    *
*                                                                              *
*******************************************************************************/

void ami_openwaveout(long p)
{
    if (p < 1 || p > waveout_num) error("Invalid wave output port");
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->open) return;
    if (d->opn) { d->opn(p); d->open = TRUE; return; }
    d->fmtset = TRUE; /* defer AudioQueue creation until first write */
    d->open = TRUE;
}

void ami_closewaveout(long p)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (!d->open) return;
    if (d->cls) { d->cls(p); d->open = FALSE; return; }
    if (d->queue) {
        AudioQueueStop(d->queue, true);
        AudioQueueDispose(d->queue, true);
        d->queue = NULL;
    }
    d->open = FALSE;
}

void ami_chanwaveout(long p, long c)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->chanwavout) { d->chanwavout(p, c); return; }
    d->chan = c;
    d->fmtset = TRUE;
}

void ami_ratewaveout(long p, long r)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->ratewavout) { d->ratewavout(p, r); return; }
    d->rate = r;
    d->fmtset = TRUE;
}

void ami_lenwaveout(long p, long l)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->lenwavout) { d->lenwavout(p, l); return; }
    d->bits = l;
    d->fmtset = TRUE;
}

void ami_sgnwaveout(long p, long s)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->sgnwavout) { d->sgnwavout(p, s); return; }
    d->sgn = s;
    d->fmtset = TRUE;
}

void ami_fltwaveout(long p, long f)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->fltwavout) { d->fltwavout(p, f); return; }
    d->flt = f;
    d->fmtset = TRUE;
}

void ami_endwaveout(long p, long e)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->endwaveout) { d->endwaveout(p, e); return; }
    d->big = e;
    d->fmtset = TRUE;
}

/* AudioQueue output callback: does nothing (buffer reuse managed in wrwave) */
static void aq_output_cb(void* data, AudioQueueRef q,
                          AudioQueueBufferRef buf)
{
    /* buffer is now available for reuse — nothing to do here,
       wrwave blocks until buffers are available */
    (void)data; (void)q; (void)buf;
}

static void ensure_aq_out(waveoutdev* d)
{
    if (!d->fmtset || !d->open) return;

    /* tear down old queue if format changed */
    if (d->queue) {
        AudioQueueStop(d->queue, true);
        AudioQueueDispose(d->queue, true);
        d->queue = NULL;
    }

    AudioStreamBasicDescription fmt = {0};
    fmt.mSampleRate = d->rate;
    fmt.mChannelsPerFrame = d->chan;
    int bytesPerSample = (d->bits + 7) / 8;

    if (d->flt) {
        fmt.mFormatID = kAudioFormatLinearPCM;
        fmt.mFormatFlags = kAudioFormatFlagIsFloat |
                           kAudioFormatFlagIsPacked;
        if (d->big) fmt.mFormatFlags |= kAudioFormatFlagIsBigEndian;
        bytesPerSample = sizeof(float);
        fmt.mBitsPerChannel = 32;
    } else {
        fmt.mFormatID = kAudioFormatLinearPCM;
        fmt.mFormatFlags = kAudioFormatFlagIsPacked;
        if (d->sgn) fmt.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
        if (d->big) fmt.mFormatFlags |= kAudioFormatFlagIsBigEndian;
        fmt.mBitsPerChannel = d->bits;
    }
    fmt.mBytesPerFrame = bytesPerSample * fmt.mChannelsPerFrame;
    fmt.mFramesPerPacket = 1;
    fmt.mBytesPerPacket = fmt.mBytesPerFrame;

    OSStatus st = AudioQueueNewOutput(&fmt, aq_output_cb, d,
                                       NULL, NULL, 0, &d->queue);
    if (st != noErr) error("Cannot create AudioQueue output");

    /* allocate and prime buffers */
    for (int i = 0; i < WAVBUFS; i++) {
        AudioQueueBufferRef buf;
        AudioQueueAllocateBuffer(d->queue, WAVBUFSZ, &buf);
        buf->mAudioDataByteSize = 0;
        AudioQueueEnqueueBuffer(d->queue, buf, 0, NULL);
    }
    AudioQueueStart(d->queue, NULL);
    d->fmtset = FALSE;
}

void ami_wrwave(long p, byte* buff, long len)
{
    if (p < 1 || p > waveout_num) return;
    waveoutdev* d = &waveout_tab[p - 1];
    if (!d->open) return;
    if (d->wrwav) { d->wrwav(p, buff, len); return; }

    ensure_aq_out(d);

    /* write data to AudioQueue buffer */
    AudioQueueBufferRef buf;
    AudioQueueAllocateBuffer(d->queue, len, &buf);
    memcpy(buf->mAudioData, buff, len);
    buf->mAudioDataByteSize = len;
    AudioQueueEnqueueBuffer(d->queue, buf, 0, NULL);
}

/*******************************************************************************
*                                                                              *
*                    Wave file playback                                        *
*                                                                              *
*******************************************************************************/

void ami_loadwave(long w, string fn)
{
    if (w < 1 || w > MAXWAVT) error("Invalid wave file slot");
    pthread_mutex_lock(&wavlck);
    if (wavfil[w - 1]) { free(wavfil[w - 1]); wavfil[w - 1] = NULL; }
    wavfil[w - 1] = strdup(fn);
    pthread_mutex_unlock(&wavlck);
}

void ami_delwave(long w)
{
    if (w < 1 || w > MAXWAVT) error("Invalid wave file slot");
    pthread_mutex_lock(&wavlck);
    if (wavfil[w - 1]) { free(wavfil[w - 1]); wavfil[w - 1] = NULL; }
    pthread_mutex_unlock(&wavlck);
}

/* AudioQueue callback for file playback */
typedef struct {
    AudioFileID       audioFile;
    AudioQueueRef     queue;
    SInt64            packetPos;
    UInt32            numPacketsToRead;
    AudioStreamPacketDescription* pktDescs;
    int               done;
} aq_playinfo;

static void aq_play_cb(void* data, AudioQueueRef q,
                        AudioQueueBufferRef buf)
{
    aq_playinfo* info = (aq_playinfo*)data;
    if (info->done) return;

    UInt32 numPackets = info->numPacketsToRead;
    UInt32 numBytes = buf->mAudioDataBytesCapacity;

    OSStatus st = AudioFileReadPacketData(info->audioFile, false,
                                           &numBytes, info->pktDescs,
                                           info->packetPos, &numPackets,
                                           buf->mAudioData);
    if (st != noErr || numPackets == 0) {
        info->done = TRUE;
        AudioQueueStop(q, false);
        return;
    }
    buf->mAudioDataByteSize = numBytes;
    info->packetPos += numPackets;
    AudioQueueEnqueueBuffer(q, buf, info->pktDescs ? numPackets : 0,
                            info->pktDescs);
}

static void* playwave_thread(void* arg)
{
    long* args = (long*)arg;
    long port = args[0];
    long wt   = args[1];
    free(args);
    (void)port;

    /* get filename */
    pthread_mutex_lock(&wavlck);
    if (wt < 1 || wt > MAXWAVT || !wavfil[wt - 1]) {
        pthread_mutex_unlock(&wavlck);
        goto done;
    }
    char* fn = strdup(wavfil[wt - 1]);
    pthread_mutex_unlock(&wavlck);

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL, (const UInt8*)fn, strlen(fn), false);
    free(fn);
    if (!url) goto done;

    AudioFileID audioFile = NULL;
    OSStatus st = AudioFileOpenURL(url, kAudioFileReadPermission, 0,
                                    &audioFile);
    CFRelease(url);
    if (st != noErr || !audioFile) goto done;

    AudioStreamBasicDescription fmt;
    UInt32 sz = sizeof(fmt);
    AudioFileGetProperty(audioFile, kAudioFilePropertyDataFormat, &sz, &fmt);

    aq_playinfo info = {0};
    info.audioFile = audioFile;
    info.numPacketsToRead = WAVBUFSZ / fmt.mBytesPerPacket;
    if (info.numPacketsToRead == 0) info.numPacketsToRead = 1;

    /* for VBR, allocate packet descriptions */
    if (fmt.mBytesPerPacket == 0) {
        UInt32 maxpkt;
        sz = sizeof(maxpkt);
        AudioFileGetProperty(audioFile,
                             kAudioFilePropertyPacketSizeUpperBound,
                             &sz, &maxpkt);
        info.numPacketsToRead = WAVBUFSZ / maxpkt;
        if (info.numPacketsToRead == 0) info.numPacketsToRead = 1;
        info.pktDescs = (AudioStreamPacketDescription*)
            malloc(info.numPacketsToRead * sizeof(AudioStreamPacketDescription));
    }

    st = AudioQueueNewOutput(&fmt, aq_play_cb, &info,
                              NULL, NULL, 0, &info.queue);
    if (st != noErr) {
        AudioFileClose(audioFile);
        free(info.pktDescs);
        goto done;
    }

    /* prime buffers */
    AudioQueueBufferRef bufs[WAVBUFS];
    for (int i = 0; i < WAVBUFS; i++) {
        AudioQueueAllocateBuffer(info.queue, WAVBUFSZ, &bufs[i]);
        aq_play_cb(&info, info.queue, bufs[i]);
        if (info.done) break;
    }

    AudioQueueStart(info.queue, NULL);

    /* wait until done */
    while (!info.done) usleep(50000);
    usleep(200000); /* drain tail */

    AudioQueueStop(info.queue, true);
    AudioQueueDispose(info.queue, true);
    AudioFileClose(audioFile);
    free(info.pktDescs);

done:
    pthread_mutex_lock(&wnmlck);
    numwav--;
    if (numwav <= 0) { numwav = 0; pthread_cond_broadcast(&wnmzer); }
    pthread_mutex_unlock(&wnmlck);
    return NULL;
}

static void playwave_kickoff(long port, long wt)
{
    pthread_mutex_lock(&wnmlck);
    numwav++;
    pthread_mutex_unlock(&wnmlck);

    long* args = (long*)malloc(2 * sizeof(long));
    args[0] = port;
    args[1] = wt;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, playwave_thread, args);
    pthread_attr_destroy(&attr);
}

void ami_playwave(long p, long t, long w)
{
    if (p < 1 || p > waveout_num) error("Invalid wave output port");
    if (w < 1 || w > MAXWAVT) error("Invalid wave file slot");
    if (t == 0 || !seqrun) {
        playwave_kickoff(p, w);
    } else {
        ami_seqptr sp;
        getseq(&sp);
        sp->port = p; sp->time = t; sp->st = st_playwave;
        sp->wt = w;
        seqorexe(sp);
    }
}

void ami_volwave(long p, long t, long v)
{
    /* not implemented at present */
    (void)p; (void)t; (void)v;
}

void ami_waitwave(long p)
{
    (void)p;
    pthread_mutex_lock(&wnmlck);
    while (numwav > 0)
        pthread_cond_wait(&wnmzer, &wnmlck);
    pthread_mutex_unlock(&wnmlck);
}

/*******************************************************************************
*                                                                              *
*                    Wave input                                                *
*                                                                              *
*******************************************************************************/

void ami_openwavein(long p)
{
    if (p < 1 || p > wavein_num) error("Invalid wave input port");
    waveindev* d = &wavein_tab[p - 1];
    if (d->open) return;
    if (d->opn) { d->opn(p); d->open = TRUE; return; }
    /* TODO: create AudioQueue input */
    d->open = TRUE;
}

void ami_closewavein(long p)
{
    if (p < 1 || p > wavein_num) return;
    waveindev* d = &wavein_tab[p - 1];
    if (!d->open) return;
    if (d->cls) { d->cls(p); d->open = FALSE; return; }
    if (d->queue) {
        AudioQueueStop(d->queue, true);
        AudioQueueDispose(d->queue, true);
        d->queue = NULL;
    }
    d->open = FALSE;
}

long ami_chanwavein(long p)
{
    if (p < 1 || p > wavein_num) return 1;
    waveindev* d = &wavein_tab[p - 1];
    if (d->chanwavin) return d->chanwavin(p);
    return d->chan;
}

long ami_ratewavein(long p)
{
    if (p < 1 || p > wavein_num) return 44100;
    waveindev* d = &wavein_tab[p - 1];
    if (d->ratewavin) return d->ratewavin(p);
    return d->rate;
}

long ami_lenwavein(long p)
{
    if (p < 1 || p > wavein_num) return 16;
    waveindev* d = &wavein_tab[p - 1];
    if (d->lenwavin) return d->lenwavin(p);
    return d->bits;
}

long ami_sgnwavein(long p)
{
    if (p < 1 || p > wavein_num) return TRUE;
    waveindev* d = &wavein_tab[p - 1];
    if (d->sgnwavin) return d->sgnwavin(p);
    return d->sgn;
}

long ami_endwavein(long p)
{
    if (p < 1 || p > wavein_num) return FALSE;
    waveindev* d = &wavein_tab[p - 1];
    if (d->endwavein) return d->endwavein(p);
    return d->big;
}

long ami_fltwavein(long p)
{
    if (p < 1 || p > wavein_num) return FALSE;
    waveindev* d = &wavein_tab[p - 1];
    if (d->fltwavin) return d->fltwavin(p);
    return d->flt;
}

long ami_rdwave(long p, byte* buff, long len)
{
    if (p < 1 || p > wavein_num) return 0;
    waveindev* d = &wavein_tab[p - 1];
    if (d->rdwav) return d->rdwav(p, buff, len);
    /* TODO: implement AudioQueue input */
    return 0;
}

/*******************************************************************************
*                                                                              *
*                    Device parameters                                         *
*                                                                              *
*******************************************************************************/

void ami_getparamsynthout(long p, string name, string value, long len)
{
    if (p < 1 || p > synthout_num) { if (len > 0) value[0] = 0; return; }
    synthoutdev* d = &synthout_tab[p - 1];
    if (d->getparam) { d->getparam(p, name, value, len); return; }
    if (len > 0) value[0] = 0;
}

void ami_getparamsynthin(long p, string name, string value, long len)
{
    if (p < 1 || p > synthin_num) { if (len > 0) value[0] = 0; return; }
    synthindev* d = &synthin_tab[p - 1];
    if (d->getparam) { d->getparam(p, name, value, len); return; }
    if (len > 0) value[0] = 0;
}

void ami_getparamwaveout(long p, string name, string value, long len)
{
    if (p < 1 || p > waveout_num) { if (len > 0) value[0] = 0; return; }
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->getparam) { d->getparam(p, name, value, len); return; }
    if (len > 0) value[0] = 0;
}

void ami_getparamwavein(long p, string name, string value, long len)
{
    if (p < 1 || p > wavein_num) { if (len > 0) value[0] = 0; return; }
    waveindev* d = &wavein_tab[p - 1];
    if (d->getparam) { d->getparam(p, name, value, len); return; }
    if (len > 0) value[0] = 0;
}

long ami_setparamsynthout(long p, string name, string value)
{
    if (p < 1 || p > synthout_num) return 1;
    synthoutdev* d = &synthout_tab[p - 1];
    if (d->setparam) return d->setparam(p, name, value);
    return 1;
}

long ami_setparamsynthin(long p, string name, string value)
{
    if (p < 1 || p > synthin_num) return 1;
    synthindev* d = &synthin_tab[p - 1];
    if (d->setparam) return d->setparam(p, name, value);
    return 1;
}

long ami_setparamwaveout(long p, string name, string value)
{
    if (p < 1 || p > waveout_num) return 1;
    waveoutdev* d = &waveout_tab[p - 1];
    if (d->setparam) return d->setparam(p, name, value);
    return 1;
}

long ami_setparamwavein(long p, string name, string value)
{
    if (p < 1 || p > wavein_num) return 1;
    waveindev* d = &wavein_tab[p - 1];
    if (d->setparam) return d->setparam(p, name, value);
    return 1;
}

/*******************************************************************************
*                                                                              *
*                    Plug-in registration                                      *
*                                                                              *
*******************************************************************************/

void _pa_synthoutplug(long addend, string name,
                      void (*opnseq)(long p), void (*clsseq)(long p),
                      void (*wrseq)(long p, ami_seqptr sp),
                      long (*setparam)(long p, string name, string value),
                      void (*getparam)(long p, string name, string value,
                                       long len))
{
    if (addend) {
        if (synthout_num >= MAXMIDP) return;
        synthoutdev* d = &synthout_tab[synthout_num];
        memset(d, 0, sizeof(*d));
        d->opn = opnseq; d->cls = clsseq; d->wrseq = wrseq;
        d->setparam = setparam; d->getparam = getparam;
        strncpy(d->name, name, sizeof(d->name) - 1);
        synthout_num++;
        synthout_plug++;
    } else {
        /* insert at front */
        if (synthout_num >= MAXMIDP) return;
        memmove(&synthout_tab[1], &synthout_tab[0],
                synthout_num * sizeof(synthoutdev));
        memset(&synthout_tab[0], 0, sizeof(synthoutdev));
        synthout_tab[0].opn = opnseq;
        synthout_tab[0].cls = clsseq;
        synthout_tab[0].wrseq = wrseq;
        synthout_tab[0].setparam = setparam;
        synthout_tab[0].getparam = getparam;
        strncpy(synthout_tab[0].name, name,
                sizeof(synthout_tab[0].name) - 1);
        synthout_num++;
        synthout_plug++;
    }
}

void _pa_synthinplug(long addend, string name,
                     void (*opnseq)(long p), void (*clsseq)(long p),
                     void (*rdseq)(long p, ami_seqptr sp),
                     long (*setparam)(long p, string name, string value),
                     void (*getparam)(long p, string name, string value,
                                      long len))
{
    if (addend) {
        if (synthin_num >= MAXMIDP) return;
        synthindev* d = &synthin_tab[synthin_num];
        memset(d, 0, sizeof(*d));
        d->opn = opnseq; d->cls = clsseq; d->rdseq = rdseq;
        d->setparam = setparam; d->getparam = getparam;
        strncpy(d->name, name, sizeof(d->name) - 1);
        synthin_num++;
        synthin_plug++;
    } else {
        if (synthin_num >= MAXMIDP) return;
        memmove(&synthin_tab[1], &synthin_tab[0],
                synthin_num * sizeof(synthindev));
        memset(&synthin_tab[0], 0, sizeof(synthindev));
        synthin_tab[0].opn = opnseq;
        synthin_tab[0].cls = clsseq;
        synthin_tab[0].rdseq = rdseq;
        synthin_tab[0].setparam = setparam;
        synthin_tab[0].getparam = getparam;
        strncpy(synthin_tab[0].name, name,
                sizeof(synthin_tab[0].name) - 1);
        synthin_num++;
        synthin_plug++;
    }
}

void _pa_waveoutplug(long addend, string name,
                     void (*open)(long p), void (*close)(long p),
                     void (*chanwavout)(long p, long c),
                     void (*ratewavout)(long p, long r),
                     void (*lenwavout)(long p, long l),
                     void (*sgnwavout)(long p, long s),
                     void (*fltwavout)(long p, long f),
                     void (*endwaveout)(long p, long e),
                     void (*wrwav)(long p, byte* buff, long len),
                     long (*setparam)(long p, string name, string value),
                     void (*getparam)(long p, string name, string value,
                                      long len))
{
    if (addend) {
        if (waveout_num >= MAXWAVP) return;
        waveoutdev* d = &waveout_tab[waveout_num];
        memset(d, 0, sizeof(*d));
        d->opn = open; d->cls = close;
        d->chanwavout = chanwavout; d->ratewavout = ratewavout;
        d->lenwavout = lenwavout; d->sgnwavout = sgnwavout;
        d->fltwavout = fltwavout; d->endwaveout = endwaveout;
        d->wrwav = wrwav;
        d->setparam = setparam; d->getparam = getparam;
        strncpy(d->name, name, sizeof(d->name) - 1);
        waveout_num++;
        waveout_plug++;
    } else {
        if (waveout_num >= MAXWAVP) return;
        memmove(&waveout_tab[1], &waveout_tab[0],
                waveout_num * sizeof(waveoutdev));
        memset(&waveout_tab[0], 0, sizeof(waveoutdev));
        waveout_tab[0].opn = open; waveout_tab[0].cls = close;
        waveout_tab[0].chanwavout = chanwavout;
        waveout_tab[0].ratewavout = ratewavout;
        waveout_tab[0].lenwavout = lenwavout;
        waveout_tab[0].sgnwavout = sgnwavout;
        waveout_tab[0].fltwavout = fltwavout;
        waveout_tab[0].endwaveout = endwaveout;
        waveout_tab[0].wrwav = wrwav;
        waveout_tab[0].setparam = setparam;
        waveout_tab[0].getparam = getparam;
        strncpy(waveout_tab[0].name, name,
                sizeof(waveout_tab[0].name) - 1);
        waveout_num++;
        waveout_plug++;
    }
}

void _pa_waveinplug(long addend, string name,
                    void (*open)(long p), void (*close)(long p),
                    long (*chanwavin)(long p), long (*ratewavin)(long p),
                    long (*lenwavin)(long p), long (*sgnwavin)(long p),
                    long (*fltwavin)(long p), long (*endwavein)(long p),
                    long (*rdwav)(long p, byte* buff, long len),
                    long (*setparam)(long p, string name, string value),
                    void (*getparam)(long p, string name, string value,
                                     long len))
{
    if (addend) {
        if (wavein_num >= MAXWAVP) return;
        waveindev* d = &wavein_tab[wavein_num];
        memset(d, 0, sizeof(*d));
        d->opn = open; d->cls = close;
        d->chanwavin = chanwavin; d->ratewavin = ratewavin;
        d->lenwavin = lenwavin; d->sgnwavin = sgnwavin;
        d->fltwavin = fltwavin; d->endwavein = endwavein;
        d->rdwav = rdwav;
        d->setparam = setparam; d->getparam = getparam;
        strncpy(d->name, name, sizeof(d->name) - 1);
        wavein_num++;
        wavein_plug++;
    } else {
        if (wavein_num >= MAXWAVP) return;
        memmove(&wavein_tab[1], &wavein_tab[0],
                wavein_num * sizeof(waveindev));
        memset(&wavein_tab[0], 0, sizeof(waveindev));
        wavein_tab[0].opn = open; wavein_tab[0].cls = close;
        wavein_tab[0].chanwavin = chanwavin;
        wavein_tab[0].ratewavin = ratewavin;
        wavein_tab[0].lenwavin = lenwavin;
        wavein_tab[0].sgnwavin = sgnwavin;
        wavein_tab[0].fltwavin = fltwavin;
        wavein_tab[0].endwavein = endwavein;
        wavein_tab[0].rdwav = rdwav;
        wavein_tab[0].setparam = setparam;
        wavein_tab[0].getparam = getparam;
        strncpy(wavein_tab[0].name, name,
                sizeof(wavein_tab[0].name) - 1);
        wavein_num++;
        wavein_plug++;
    }
}

#pragma clang diagnostic pop
