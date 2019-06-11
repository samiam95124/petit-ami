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

#ifndef __SOUND_H__
#define __SOUND_H__

#define PA_CHAN_DRUM 10 /* the GM drum channel */

#define PA_SYNTH_OUT 1 /* the default output synth for host */
#define PA_SYNTH_EXT 2 /* the default output to external synth */

/* the notes in the lowest octave */

#define PA_NOTE_C       1
#define PA_NOTE_C_SHARP 2
#define PA_NOTE_D_FLAT  2
#define PA_NOTE_D       3
#define PA_NOTE_D_SHARP 4
#define PA_NOTE_E_FLAT  4
#define PA_NOTE_E       5
#define PA_NOTE_F       6
#define PA_NOTE_F_SHARP 7
#define PA_NOTE_G_FLAT  7
#define PA_NOTE_G       8
#define PA_NOTE_G_SHARP 9
#define PA_NOTE_A_FLAT  9
#define PA_NOTE_A       10
#define PA_NOTE_A_SHARP 11
#define PA_NOTE_B_FLAT  11
#define PA_NOTE_B       12

/* the octaves of midi, add to note to place in that octave */

#define PA_OCTAVE_1  0
#define PA_OCTAVE_2  12
#define PA_OCTAVE_3  24
#define PA_OCTAVE_4  36
#define PA_OCTAVE_5  48
#define PA_OCTAVE_6  60
#define PA_OCTAVE_7  72
#define PA_OCTAVE_8  84
#define PA_OCTAVE_9  96
#define PA_OCTAVE_10 108
#define PA_OCTAVE_11 120

/* Standard GM instruments */

/* Piano */

#define PA_INST_ACOUSTIC_GRAND        1
#define PA_INST_BRIGHT_ACOUSTIC       2
#define PA_INST_ELECTRIC_GRAND        3
#define PA_INST_HONKY_TONK            4
#define PA_INST_ELECTRIC_PIANO_1      5
#define PA_INST_ELECTRIC_PIANO_2      6
#define PA_INST_HARPSICHORD           7
#define PA_INST_CLAVINET              8

/* Chromatic percussion */

#define PA_INST_CELESTA               9
#define PA_INST_GLOCKENSPIEL          10
#define PA_INST_MUSIC_BOX             11
#define PA_INST_VIBRAPHONE            12
#define PA_INST_MARIMBA               13
#define PA_INST_XYLOPHONE             14
#define PA_INST_TUBULAR_BELLS         15
#define PA_INST_DULCIMER              16

/* Organ */
                         
#define PA_INST_DRAWBAR_ORGAN         17
#define PA_INST_PERCUSSIVE_ORGAN      18
#define PA_INST_ROCK_ORGAN            19
#define PA_INST_CHURCH_ORGAN          20
#define PA_INST_REED_ORGAN            21
#define PA_INST_ACCORIDAN             22
#define PA_INST_HARMONICA             23
#define PA_INST_TANGO_ACCORDIAN       24

/* Guitar */

#define PA_INST_NYLON_STRING_GUITAR   25
#define PA_INST_STEEL_STRING_GUITAR   26
#define PA_INST_ELECTRIC_JAZZ_GUITAR  27
#define PA_INST_ELECTRIC_CLEAN_GUITAR 28
#define PA_INST_ELECTRIC_MUTED_GUITAR 29
#define PA_INST_OVERDRIVEN_GUITAR     30
#define PA_INST_DISTORTION_GUITAR     31
#define PA_INST_GUITAR_HARMONICS      32

/* Bass */

#define PA_INST_ACOUSTIC_BASS         33
#define PA_INST_ELECTRIC_BASS_FINGER  34
#define PA_INST_ELECTRIC_BASS_PICK    35
#define PA_INST_FRETLESS_BASS         36
#define PA_INST_SLAP_BASS_1           37
#define PA_INST_SLAP_BASS_2           38
#define PA_INST_SYNTH_BASS_1          39
#define PA_INST_SYNTH_BASS_2          40

/* Solo strings */

#define PA_INST_VIOLIN                41
#define PA_INST_VIOLA                 42
#define PA_INST_CELLO                 43
#define PA_INST_CONTRABASS            44
#define PA_INST_TREMOLO_STRINGS       45
#define PA_INST_PIZZICATO_STRINGS     46
#define PA_INST_ORCHESTRAL_STRINGS    47
#define PA_INST_TIMPANI               48

/* Ensemble */

#define PA_INST_STRING_ENSEMBLE_1     49
#define PA_INST_STRING_ENSEMBLE_2     50
#define PA_INST_SYNTHSTRINGS_1        51
#define PA_INST_SYNTHSTRINGS_2        52
#define PA_INST_CHOIR_AAHS            53
#define PA_INST_VOICE_OOHS            54
#define PA_INST_SYNTH_VOICE           55
#define PA_INST_ORCHESTRA_HIT         56

/* Brass */

#define PA_INST_TRUMPET               57
#define PA_INST_TROMBONE              58
#define PA_INST_TUBA                  59
#define PA_INST_MUTED_TRUMPET         60
#define PA_INST_FRENCH_HORN           61
#define PA_INST_BRASS_SECTION         62
#define PA_INST_SYNTHBRASS_1          63
#define PA_INST_SYNTHBRASS_2          64

/* Reed */
                        
#define PA_INST_SOPRANO_SAX           65
#define PA_INST_ALTO_SAX              66
#define PA_INST_TENOR_SAX             67
#define PA_INST_BARITONE_SAX          68
#define PA_INST_OBOE                  69
#define PA_INST_ENGLISH_HORN          70
#define PA_INST_BASSOON               71
#define PA_INST_CLARINET              72

/* Pipe */

#define PA_INST_PICCOLO               73
#define PA_INST_FLUTE                 74
#define PA_INST_RECORDER              75
#define PA_INST_PAN_FLUTE             76
#define PA_INST_BLOWN_BOTTLE          77
#define PA_INST_SKAKUHACHI            78
#define PA_INST_WHISTLE               79
#define PA_INST_OCARINA               80

/* Synth lead */
                   
#define PA_INST_LEAD_1_SQUARE         81
#define PA_INST_LEAD_2_SAWTOOTH       82
#define PA_INST_LEAD_3_CALLIOPE       83
#define PA_INST_LEAD_4_CHIFF          84
#define PA_INST_LEAD_5_CHARANG        85
#define PA_INST_LEAD_6_VOICE          86
#define PA_INST_LEAD_7_FIFTHS         87
#define PA_INST_LEAD_8_BASS_LEAD      88

/* Synth pad */

#define PA_INST_PAD_1_NEW_AGE         89
#define PA_INST_PAD_2_WARM            90
#define PA_INST_PAD_3_POLYSYNTH       91
#define PA_INST_PAD_4_CHOIR           92
#define PA_INST_PAD_5_BOWED           93
#define PA_INST_PAD_6_METALLIC        94
#define PA_INST_PAD_7_HALO            95
#define PA_INST_PAD_8_SWEEP           96

/* Synth effects */

#define PA_INST_FX_1_RAIN             97
#define PA_INST_FX_2_SOUNDTRACK       98
#define PA_INST_FX_3_CRYSTAL          99
#define PA_INST_FX_4_ATMOSPHERE       100
#define PA_INST_FX_5_BRIGHTNESS       101
#define PA_INST_FX_6_GOBLINS          102
#define PA_INST_FX_7_ECHOES           103
#define PA_INST_FX_8_SCI_FI           104

/* Ethnic */

#define PA_INST_SITAR                 105
#define PA_INST_BANJO                 106
#define PA_INST_SHAMISEN              107
#define PA_INST_KOTO                  108
#define PA_INST_KALIMBA               109
#define PA_INST_BAGPIPE               110
#define PA_INST_FIDDLE                111
#define PA_INST_SHANAI                112

/* Percussive */

#define PA_INST_TINKLE_BELL           113
#define PA_INST_AGOGO                 114
#define PA_INST_STEEL_DRUMS           115
#define PA_INST_WOODBLOCK             116
#define PA_INST_TAIKO_DRUM            117
#define PA_INST_MELODIC_TOM           118
#define PA_INST_SYNTH_DRUM            119
#define PA_INST_REVERSE_CYMBAL        120

/* Sound effects */

#define PA_INST_GUITAR_FRET_NOISE     121
#define PA_INST_BREATH_NOISE          122
#define PA_INST_SEASHORE              123
#define PA_INST_BIRD_TWEET            124
#define PA_INST_TELEPHONE_RING        125
#define PA_INST_HELICOPTER            126
#define PA_INST_APPLAUSE              127
#define PA_INST_GUNSHOT               128

/* Drum sounds, activated as notes to drum instruments */

#define PA_NOTE_ACOUSTIC_BASS_DRUM 35
#define PA_NOTE_BASS_DRUM_1        36
#define PA_NOTE_SIDE_STICK         37
#define PA_NOTE_ACOUSTIC_SNARE     38
#define PA_NOTE_HAND_CLAP          39
#define PA_NOTE_ELECTRIC_SNARE     40
#define PA_NOTE_LOW_FLOOR_TOM      41
#define PA_NOTE_CLOSED_HI_HAT      42
#define PA_NOTE_HIGH_FLOOR_TOM     43
#define PA_NOTE_PEDAL_HI_HAT       44
#define PA_NOTE_LOW_TOM            45
#define PA_NOTE_OPEN_HI_HAT        46
#define PA_NOTE_LOW_MID_TOM        47
#define PA_NOTE_HI_MID_TOM         48
#define PA_NOTE_CRASH_CYMBAL_1     49
#define PA_NOTE_HIGH_TOM           50
#define PA_NOTE_RIDE_CYMBAL_1      51
#define PA_NOTE_CHINESE_CYMBAL     52
#define PA_NOTE_RIDE_BELL          53
#define PA_NOTE_TAMBOURINE         54
#define PA_NOTE_SPLASH_CYMBAL      55
#define PA_NOTE_COWBELL            56
#define PA_NOTE_CRASH_CYMBAL_2     57
#define PA_NOTE_VIBRASLAP          58
#define PA_NOTE_RIDE_CYMBAL_2      59
#define PA_NOTE_HI_BONGO           60
#define PA_NOTE_LOW_BONGO          61
#define PA_NOTE_MUTE_HI_CONGA      62
#define PA_NOTE_OPEN_HI_CONGA      63
#define PA_NOTE_LOW_CONGA          64
#define PA_NOTE_HIGH_TIMBALE       65
#define PA_NOTE_LOW_TIMBALE        66
#define PA_NOTE_HIGH_AGOGO         67
#define PA_NOTE_LOW_AGOGO          68
#define PA_NOTE_CABASA             69
#define PA_NOTE_MARACAS            70
#define PA_NOTE_SHORT_WHISTLE      71
#define PA_NOTE_LONG_WHISTLE       72
#define PA_NOTE_SHORT_GUIRO        73
#define PA_NOTE_LONG_GUIRO         74
#define PA_NOTE_CLAVES             75
#define PA_NOTE_HI_WOOD_BLOCK      76
#define PA_NOTE_LOW_WOOD_BLOCK     77
#define PA_NOTE_MUTE_CUICA         78
#define PA_NOTE_OPEN_CUICA         79
#define PA_NOTE_MUTE_TRIANGLE      80
#define PA_NOTE_OPEN_TRIANGLE      81

typedef char* string;  /* general string type */
typedef enum { false, true } boolean; /* boolean */
typedef unsigned char byte; /* byte */
typedef int note;       /* 1..128  note number for midi */
typedef int channel;    /* 1..16   channel number */
typedef int instrument; /* 1..128  instrument number */

/* functions at this level */

void pa_starttime(void);
void pa_stoptime(void);
int pa_curtime(void);
int pa_synthout(void);
void pa_opensynthout(int p);
void pa_closesynthout(int p);
void pa_noteon(int p, int t, channel c, note n, int v);
void pa_noteoff(int p, int t, channel c, note n, int v);
void pa_instchange(int p, int t, channel c, instrument i);
void pa_attack(int p, int t, channel c, int at);
void pa_release(int p, int t, channel c, int rt);
void pa_legato(int p, int t, channel c, boolean b);
void pa_portamento(int p, int t, channel c, boolean b);
void pa_vibrato(int p, int t, channel c, int v);
void pa_volsynthchan(int p, int t, channel c, int v);
void pa_porttime(int p, int t, channel c, int v);
void pa_balance(int p, int t, channel c, int b);
void pa_pan(int p, int t, channel c, int b);
void pa_timbre(int p, int t, channel c, int tb);
void pa_brightness(int p, int t, channel c, int b);
void pa_reverb(int p, int t, channel c, int r);
void pa_tremulo(int p, int t, channel c, int tr);
void pa_chorus(int p, int t, channel c, int cr);
void pa_celeste(int p, int t, channel c, int ce);
void pa_phaser(int p, int t, channel c, int ph);
void pa_aftertouch(int p, int t, channel c, note n, int at);
void pa_pressure(int p, int t, channel c, note n, int pr);
void pa_pitch(int p, int t, channel c, int pt);
void pa_pitchrange(int p, int t, channel c, int v);
void pa_mono(int p, int t, channel c, int ch);
void pa_poly(int p, int t, channel c);
void pa_playsynth(int p, int t, string sf);
int pa_waveout(void);
void pa_openwaveout(int p);
void pa_closewaveout(int p);
void pa_playwave(int p, int t, string sf);
void pa_volwave(int p, int t, int v);

#endif /* __SOUND_H__ */
