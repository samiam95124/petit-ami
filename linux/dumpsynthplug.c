/*******************************************************************************

Dump sequencer plug-in for Petit_Ami sound module

Dumps the input sequencer records for MIDI and reroutes them to the next device
in line (numeric order). This can be useful to see the MIDI stream, or simply
to test MIDI in plugins.

*******************************************************************************/

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <localdefs.h>
#include <sound.h>

#define MAXINST 100 /* maximum allowed device instance (unused) */

static int sport; /* input port */

/*******************************************************************************

Flag dumpmidi error

Prints an error and stops.

*******************************************************************************/

static void error(string es)

{

    fprintf(stderr, "\nError: Dumpmidi: %s\n", es);

    exit(1);

}

/*******************************************************************************

Copy string to critical output buffer

Copies the given source string to a caller supplied output buffer. Output
buffers follow the critical buffer convention: a result that fills the entire
buffer is not zero terminated, a shorter result is zero terminated, and it is
an error if the result cannot fit.

*******************************************************************************/

static void cpycrit(char* d, ami_long dl, const char* s)

{

    ami_long l;

    l = strlen(s); /* find length of source */
    if (l > dl) error("String too large for destination");
    memcpy(d, s, l); /* copy string into place */
    if (l < dl) d[l] = 0; /* terminate if shorter than buffer */

}

/*******************************************************************************

Open Liquidsynth MIDI device

Opens a Liquidsynth MIDI port for use. Does nothing at present, since we open
one MIDI out device at init time.

*******************************************************************************/

static void opendump(ami_long p)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!sport) error("No input port set to dump");

    ami_opensynthin(sport); /* open the monitored device */

}

/*******************************************************************************

Close Liquidsynth MIDI device

Closes a Liquidsynth MIDI output device for use.

*******************************************************************************/

static void closedump(ami_long p)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!sport) error("No input port set to dump");

    ami_closesynthin(sport); /* close the monitored device */

}

/*******************************************************************************

Read and dump MIDI message

Reads a sequencer message. The sequencer message is read from the next input
device in the table, then that is dumped and returned to the caller.

*******************************************************************************/

static void readdump(ami_long p, ami_seqptr sp)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!sport) error("No input port set to dump");

    ami_rdsynth(sport, sp); /* get seq record */
    /* now just dump the message */
    switch (sp->st) { /* sequencer message type */

        case st_noteon:       printf("noteon: Time: %lld Port: %lld Channel: %lld "
                                     "Note: %lld Velocity: %lld\n",
                                     AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->ntc), AMI_LONG_CAST(sp->ntn), AMI_LONG_CAST(sp->ntv));
                              break;
        case st_noteoff:      printf("noteoff: Time: %lld Port: %lld Channel: %lld "
                                     "Note: %lld Velocity: %lld\n", AMI_LONG_CAST(sp->time),
                                     AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->ntc), AMI_LONG_CAST(sp->ntn), AMI_LONG_CAST(sp->ntv));
                              break;
        case st_instchange:   printf("instchange: Time: %lld Port: %lld sp->port "
                                     "Channel: %lld Instrument: %lld\n", AMI_LONG_CAST(sp->time),
                                     AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->icc), AMI_LONG_CAST(sp->ici));
                              break;
        case st_attack:       printf("attack: Time: %lld Port: %lld Channel: %lld "
                                     "attack time: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port),
                                     AMI_LONG_CAST(sp->vsc), AMI_LONG_CAST(sp->vsv));
                              break;
        case st_release:      printf("release: Time: %lld Port: %lld Channel: %lld "
                                     "release time: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port),
                                     AMI_LONG_CAST(sp->vsc), AMI_LONG_CAST(sp->vsv));
                              break;
        case st_legato:       printf("legato: Time: %lld Port: %lld Channel: %lld "
                                     "legato on/off: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port),
                                     AMI_LONG_CAST(sp->bsc), AMI_LONG_CAST(sp->bsb));
                              break;
        case st_portamento:   printf("portamento: Time: %lld Port: %lld Channel: %lld "
                                     "portamento on/off: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port),
                                     AMI_LONG_CAST(sp->bsc), AMI_LONG_CAST(sp->bsb));
                              break;
        case st_vibrato:      printf("vibrato: Time: %lld Port: %lld Channel: %lld "
                                     "Vibrato: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_volsynthchan: printf("volsynthchan: Time: %lld Port: %lld Channel: %lld "
                                     "Volume: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_porttime:     printf("porttime: Time: %lld Port: %lld Channel: %lld "
                                     "Portamento time: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port),
                                     AMI_LONG_CAST(sp->vsc), AMI_LONG_CAST(sp->vsv));
                              break;
        case st_balance:      printf("attack: Time: %lld Port: %lld Channel: %lld "
                                     "Ballance: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_pan:          printf("pan: Time: %lld Port: %lld Channel: %lld "
                                     "Pan: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_timbre:       printf("timbre: Time: %lld Port: %lld Channel: %lld "
                                     "Timbre: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_brightness:   printf("brightness: Time: %lld Port: %lld Channel: %lld "
                                     "Brightness: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port),
                                     AMI_LONG_CAST(sp->vsc), AMI_LONG_CAST(sp->vsv));
                              break;
        case st_reverb:       printf("reverb: Time: %lld Port: %lld Channel: %lld "
                                     "Reverb: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_tremulo:      printf("tremulo: Time: %lld Port: %lld Channel: %lld "
                                     "Tremulo: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_chorus:       printf("chorus: Time: %lld Port: %lld Channel: %lld "
                                     "Chorus: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_celeste:      printf("celeste: Time: %lld Port: %lld Channel: %lld "
                                     "Celeste: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_phaser:       printf("Phaser: Time: %lld Port: %lld Channel: %lld "
                                     "Phaser: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_aftertouch:   printf("aftertouch: Time: %lld Port: %lld Channel: %lld "
                                     "Note: %lld Aftertouch: %lld\n", AMI_LONG_CAST(sp->time),
                                     AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->ntc), AMI_LONG_CAST(sp->ntn), AMI_LONG_CAST(sp->ntv));
                              break;
        case st_pressure:     printf("pressure: Time: %lld Port: %lld Channel: %lld "
                                     "Pressure: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->ntc),
                                     AMI_LONG_CAST(sp->ntv));
                              break;
        case st_pitch:        printf("pitch: Time: %lld Port: %lld Channel: %lld "
                                     "Pitch: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->vsc),
                                     AMI_LONG_CAST(sp->vsv));
                              break;
        case st_pitchrange:   printf("pitchrange: Time: %lld Port: %lld Channel: %lld "
                                     "Pitch range: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port),
                                     AMI_LONG_CAST(sp->vsc), AMI_LONG_CAST(sp->vsv));
                              break;
        case st_mono:         printf("mono: Time: %lld Port: %lld Channel: %lld "
                                     "Mono notes: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port),
                                     AMI_LONG_CAST(sp->vsc), AMI_LONG_CAST(sp->vsv));
                              break;
        case st_poly:         printf("poly: Time: %lld Port: %lld Channel: %lld\n",
                                     AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->pc));
                              break;
        case st_playsynth:    printf("playsynth: Time: %lld Port: %lld "
                                     ".mid file id: %lld\n", AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port),
                                     AMI_LONG_CAST(sp->sid));
                              break;
        case st_playwave:     printf("playwave: Time: %lld Port: %lld "
                                     ".wav file logical number: %lld\n", AMI_LONG_CAST(sp->time),
                                     AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->wt));
                              break;
        case st_volwave:      printf("volwave: Time: %lld Port: %lld Volume: %lld\n",
                                     AMI_LONG_CAST(sp->time), AMI_LONG_CAST(sp->port), AMI_LONG_CAST(sp->wv));
                              break;

    }

}

/*******************************************************************************

Set parameter

Set plug in parameter from the given name and value. Not implemented at present.
Always returns error.

*******************************************************************************/

ami_long setparamdump(ami_long p, string name, string value)

{

    ami_long r;
    string ep;

    r = 1; /* set error by default */
    if (!strcmp(name, "connect")) {

        /* set connection for output */
        sport = strtol(value, &ep, 0);
        r = !*ep; /* set good if entire string read */

    }
    return (r); /* exit with error */

}

/*******************************************************************************

Get parameter

Get plug in parameter from the given name and value. Not implemented at present.
Always returns empty string. The value is returned by the critical buffer
convention: a result that fills the entire buffer is not zero terminated, a
shorter result is zero terminated, and it is an error if the result cannot fit.

*******************************************************************************/

void getparamdump(ami_long p, string name, string value, ami_long len)

{

    cpycrit(value, len, ""); /* return empty string */

}

/*******************************************************************************

Initialize dumpmidi plug-in.

Registers dumpmidi as a plug-in device with PA sound module.

*******************************************************************************/

static void dumpmidi_plug_init (void) __attribute__((constructor (103)));
static void dumpmidi_plug_init()

{

    /* now install us as PA device at end */
    _pa_synthinplug(TRUE, "Dump MIDI", opendump, closedump, readdump,
                    setparamdump, getparamdump);
    sport = 0; /* set input port invalid until set */

}

/*******************************************************************************

Deinitialize dumpmidi plug-in.

Clean up dumpmidi instance.

*******************************************************************************/

static void dumpmidi_plug_deinit (void) __attribute__((destructor (103)));
static void dumpmidi_plug_deinit()

{

}
