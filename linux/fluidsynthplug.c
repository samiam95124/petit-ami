/*******************************************************************************

Fluidsynth Plug-In for Petit_Ami sound module

Allows Fluidsynth to serve as a plug-in for MIDI command stream devices under
Petit_ami sound system.

*******************************************************************************/

#include <sys/types.h>
#include <unistd.h>

#include <fluidsynth.h>
#include <stdlib.h>
#include <sound.h>

static fluid_settings_t *settings;
static fluid_synth_t *synth;
static fluid_audio_driver_t *adriver;
static int sfont_id;

/*******************************************************************************

Open Liquidsynth MIDI device

Opens a Liquidsynth MIDI port for use. Does nothing at present, since we open
one MIDI out device at init time.

*******************************************************************************/

static void openliquid(int p)

{

}

/*******************************************************************************

Close Liquidsynth MIDI device

Closes a Liquidsynth MIDI output device for use.

*******************************************************************************/

static void closeliquid(int p)

{

}

/*******************************************************************************

Write liquidsynth MIDI message

Accepts a MIDI message in PA sequencer format and outputs that.

*******************************************************************************/

static void writeliquid(int p, seqptr sp)

{

    switch (sp->st) { /* sequencer message type */

        case st_noteon:
            fluid_synth_noteon(synth, sp->ntc-1, sp->ntn-1, sp->ntv/0x01000000);
            break;
        case st_noteoff:
            fluid_synth_noteoff(synth, sp->ntc-1, sp->ntn-1/*,
                                sp->ntv/0x01000000*/);
            break;
        case st_instchange:   break;
        case st_attack:       break;
        case st_release:      break;
        case st_legato:       break;
        case st_portamento:   break;
        case st_vibrato:      break;
        case st_volsynthchan: break;
        case st_porttime:     break;
        case st_balance:      break;
        case st_pan:          break;
        case st_timbre:       break;
        case st_brightness:   break;
        case st_reverb:       break;
        case st_tremulo:      break;
        case st_chorus:       break;
        case st_celeste:      break;
        case st_phaser:       break;
        case st_aftertouch:   break;
        case st_pressure:     break;
        case st_pitch:        break;
        case st_pitchrange:   break;
        case st_mono:         break;
        case st_poly:         break;
        case st_playsynth:    break;
        case st_playwave:     break;
        case st_volwave:      break;

    }

}

/*******************************************************************************

Initialize Fluidsynth plug-in.

Registers Fluidsynth as a plug-in device with PA sound module.

*******************************************************************************/

static void fluidsynth_plug_init (void) __attribute__((constructor (103)));
static void fluidsynth_plug_init()

{

    /* create the settings */
    settings = new_fluid_settings();
    /* create the synthesizer */
    synth = new_fluid_synth(settings);
    /* create the audio driver as alsa type */
    fluid_settings_setstr(settings, "audio.driver", "alsa");
    adriver = new_fluid_audio_driver(settings, synth);
    /* load a SoundFont and reset presets */
    sfont_id = fluid_synth_sfload(synth, "/usr/share/sounds/sf2/FluidR3_GM.sf2", 1);

    /* now install us as PA device */
    synthoutplug("Liquidsynth", openliquid, closeliquid, writeliquid);

}

/*******************************************************************************

Deinitialize Fluidsynth plug-in.

Clean up Fluidsynth instance.

*******************************************************************************/

static void fluidsynth_plug_deinit (void) __attribute__((constructor (103)));
static void fluidsynth_plug_deinit()

{

    /* Clean up */
    delete_fluid_audio_driver(adriver);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);

}
