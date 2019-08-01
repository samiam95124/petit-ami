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

Many functions don't have equivalents in Fluidsynth. This is not serious, I
found that many of these functions do nothing on most synthesizers.

Some of the questionable codes should be compared against what happens when the
midi codes are fed directly to Fluidsynth (source code analysis?).

*******************************************************************************/

static void writeliquid(int p, seqptr sp)

{


    switch (sp->st) { /* sequencer message type */

        case st_noteon:
            fluid_synth_noteon(synth, sp->ntc-1, sp->ntn-1, sp->ntv/0x01000000);
            break;
        case st_noteoff:
            /* Note fluidsynth has no velocity parameter */
            fluid_synth_noteoff(synth, sp->ntc-1, sp->ntn-1);
            break;
        case st_instchange:
            fluid_synth_program_change(synth, sp->icc-1, sp->ici-1);
            break;
        case st_attack: break; /* no equivalent function */
        case st_release: break; /* no equivalent function */
        case st_legato:
            /* this call does not appear to be in the current Fluidsynth API */
            /*
            fluid_synth_set_legato_mode(synth, sp->bsc,
                sp->bsb?FLUID_CHANNEL_LEGATO_MODE_MULTI_RETRIGGER:
                        FLUID_CHANNEL_LEGATO_MODE_RETRIGGER);
            */
            break;
        case st_portamento:
            /* midi says on/off, but fluidsynth has three options, no apparent off */
            break;
        case st_vibrato:      break; /* no equivalent function */
        case st_volsynthchan: break; /* no equivalent function */
        case st_porttime:     break; /* no equivalent function */
        case st_balance:      break; /* no equivalent function */
        case st_pan:          break; /* no equivalent function */
        case st_timbre:       break; /* no equivalent function */
        case st_brightness:   break; /* no equivalent function */
        case st_reverb:       break; /* fluidsynth gives many options for this,
                                        needs research */
        case st_tremulo:      break; /* no equivalent function */
        case st_chorus:       break; /* fluidsynth gives many options for this,
                                        needs research */
        case st_celeste:      break; /* no equivalent function */
        case st_phaser:       break; /* no equivalent function */
        case st_aftertouch:   break; /* no equivalent function */
        case st_pressure:
            fluid_synth_channel_pressure(synth, sp->ntc-1, sp->ntv/0x01000000);
            break;
        case st_pitch:
            fluid_synth_pitch_bend(synth, sp->vsc-1, sp->vsv/0x00040000+0x2000);
            break;
        case st_pitchrange:
            /* this one is open for interpretation: what is a "semitone"? */
            fluid_synth_pitch_wheel_sens(synth, sp->vsc, sp->vsv/0x00020000);
            break;
        case st_mono:         break; /* no equivalent function */
        case st_poly:         break; /* no equivalent function */
        case st_playsynth:
        case st_playwave:
            /* not a midi instruction, we send this back to sound.c */
            excseq(p, sp);
            break;
        case st_volwave:      break;

    }

}

/*******************************************************************************

Initialize Fluidsynth plug-in.

Registers Fluidsynth as a plug-in device with PA sound module.

*******************************************************************************/

static int stderr_file;

static void quiet(void)

{

    /* run alsa open in quiet */
    fflush(stderr);
    stderr_file = dup(fileno(stderr));
    freopen("/dev/null", "w", stderr);

}

static void unquiet(void)

{

    fflush(stderr);
    dup2(stderr_file, fileno(stderr));
    close(stderr_file);
    clearerr(stderr);

}

static void fluidsynth_plug_init (void) __attribute__((constructor (103)));
static void fluidsynth_plug_init()

{

    char buff[200];

    /* turn off liquidsynth error messages */
    quiet();
    /* create the settings */
    settings = new_fluid_settings();
    /* create the synthesizer */
    synth = new_fluid_synth(settings);
    /* create the audio driver as alsa type */
    fluid_settings_setstr(settings, "audio.driver", "alsa");
    adriver = new_fluid_audio_driver(settings, synth);
    /* load a SoundFont and reset presets */
    sfont_id = fluid_synth_sfload(synth, "/usr/share/sounds/sf2/FluidR3_GM.sf2", 1);
    /* fluidsynth default volume is very low. Turn up to reasonable value */
    fluid_settings_setnum(settings, "synth.gain", 0.5);
    /* re-enable error messages */
    unquiet();

    /* show the device fluidsynth connects to (usually "default") */
    /*
    r = fluid_settings_copystr(settings, "audio.alsa.device", buff, 200);
    printf("The alsa PCM device for Fluidsynth is: %s\n", buff);
    */

    /* now install us as PA device */
    synthoutplug("Liquidsynth", openliquid, closeliquid, writeliquid);

}

/*******************************************************************************

Deinitialize Fluidsynth plug-in.

Clean up Fluidsynth instance.

*******************************************************************************/

static void fluidsynth_plug_deinit (void) __attribute__((destructor (103)));
static void fluidsynth_plug_deinit()

{

    /* Clean up */
    delete_fluid_audio_driver(adriver);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);

}
