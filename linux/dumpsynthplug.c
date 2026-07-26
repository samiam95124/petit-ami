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

int sport; /* input port */

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

Open Liquidsynth MIDI device

Opens a Liquidsynth MIDI port for use. Does nothing at present, since we open
one MIDI out device at init time.

*******************************************************************************/

static void opendump(long p)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!sport) error("No input port set to dump");

    ami_opensynthin(sport); /* open the monitored device */

}

/*******************************************************************************

Close Liquidsynth MIDI device

Closes a Liquidsynth MIDI output device for use.

*******************************************************************************/

static void closedump(long p)

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

static void readdump(long p, ami_seqptr sp)

{

    if (p < 1 || p > MAXINST) error("Invalid synth handle");
    if (!sport) error("No input port set to dump");

    ami_rdsynth(sport, sp); /* get seq record */
    /* now just dump the message */
    switch (sp->st) { /* sequencer message type */

        case st_noteon:       printf("noteon: Time: %ld Port: %ld Channel: %ld "
                                     "Note: %ld Velocity: %ld\n",
                                     sp->time, sp->port, sp->ntc, sp->ntn, sp->ntv);
                              break;
        case st_noteoff:      printf("noteoff: Time: %ld Port: %ld Channel: %ld "
                                     "Note: %ld Velocity: %ld\n", sp->time,
                                     sp->port, sp->ntc, sp->ntn, sp->ntv);
                              break;
        case st_instchange:   printf("instchange: Time: %ld Port: %ld sp->port "
                                     "Channel: %ld Instrument: %ld\n", sp->time,
                                     sp->port, sp->icc, sp->ici);
                              break;
        case st_attack:       printf("attack: Time: %ld Port: %ld Channel: %ld "
                                     "attack time: %ld\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_release:      printf("release: Time: %ld Port: %ld Channel: %ld "
                                     "release time: %ld\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_legato:       printf("legato: Time: %ld Port: %ld Channel: %ld "
                                     "legato on/off: %ld\n", sp->time, sp->port,
                                     sp->bsc, sp->bsb);
                              break;
        case st_portamento:   printf("portamento: Time: %ld Port: %ld Channel: %ld "
                                     "portamento on/off: %ld\n", sp->time, sp->port,
                                     sp->bsc, sp->bsb);
                              break;
        case st_vibrato:      printf("vibrato: Time: %ld Port: %ld Channel: %ld "
                                     "Vibrato: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_volsynthchan: printf("volsynthchan: Time: %ld Port: %ld Channel: %ld "
                                     "Volume: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_porttime:     printf("porttime: Time: %ld Port: %ld Channel: %ld "
                                     "Portamento time: %ld\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_balance:      printf("attack: Time: %ld Port: %ld Channel: %ld "
                                     "Ballance: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_pan:          printf("pan: Time: %ld Port: %ld Channel: %ld "
                                     "Pan: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_timbre:       printf("timbre: Time: %ld Port: %ld Channel: %ld "
                                     "Timbre: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_brightness:   printf("brightness: Time: %ld Port: %ld Channel: %ld "
                                     "Brightness: %ld\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_reverb:       printf("reverb: Time: %ld Port: %ld Channel: %ld "
                                     "Reverb: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_tremulo:      printf("tremulo: Time: %ld Port: %ld Channel: %ld "
                                     "Tremulo: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_chorus:       printf("chorus: Time: %ld Port: %ld Channel: %ld "
                                     "Chorus: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_celeste:      printf("celeste: Time: %ld Port: %ld Channel: %ld "
                                     "Celeste: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_phaser:       printf("Phaser: Time: %ld Port: %ld Channel: %ld "
                                     "Phaser: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_aftertouch:   printf("aftertouch: Time: %ld Port: %ld Channel: %ld "
                                     "Note: %ld Aftertouch: %ld\n", sp->time,
                                     sp->port, sp->ntc, sp->ntn, sp->ntv);
                              break;
        case st_pressure:     printf("pressure: Time: %ld Port: %ld Channel: %ld "
                                     "Pressure: %ld\n", sp->time, sp->port, sp->ntc,
                                     sp->ntv);
                              break;
        case st_pitch:        printf("pitch: Time: %ld Port: %ld Channel: %ld "
                                     "Pitch: %ld\n", sp->time, sp->port, sp->vsc,
                                     sp->vsv);
                              break;
        case st_pitchrange:   printf("pitchrange: Time: %ld Port: %ld Channel: %ld "
                                     "Pitch range: %ld\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_mono:         printf("mono: Time: %ld Port: %ld Channel: %ld "
                                     "Mono notes: %ld\n", sp->time, sp->port,
                                     sp->vsc, sp->vsv);
                              break;
        case st_poly:         printf("poly: Time: %ld Port: %ld Channel: %ld\n",
                                     sp->time, sp->port, sp->pc);
                              break;
        case st_playsynth:    printf("playsynth: Time: %ld Port: %ld "
                                     ".mid file id: %ld\n", sp->time, sp->port,
                                     sp->sid);
                              break;
        case st_playwave:     printf("playwave: Time: %ld Port: %ld "
                                     ".wav file logical number: %ld\n", sp->time,
                                     sp->port, sp->wt);
                              break;
        case st_volwave:      printf("volwave: Time: %ld Port: %ld Volume: %ld\n",
                                     sp->time, sp->port, sp->wv);
                              break;

    }

}

/*******************************************************************************

Set parameter

Set plug in parameter from the given name and value. Not implemented at present.
Always returns error.

*******************************************************************************/

long setparamdump(long p, string name, string value)

{

    long r;
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
Always returns empty string.

*******************************************************************************/

void getparamdump(long p, string name, string value, long len)

{

    *value = 0; /* clear output string */

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
