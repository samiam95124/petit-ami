/** ****************************************************************************
 *
 * Sound library interface C++ wrapper declarations
 *
 * See cpp/sound.cpp for the rationale. The types and constants of the C
 * header appear here with the ami_/AMI_ prefixes stripped, the constants
 * in lower case as C++ constants rather than macros. The layout of
 * seqmsg is identical to ami_seqmsg in sound.h, and must stay so: the
 * wrapper casts between them.
 *
 ******************************************************************************/

#ifndef __SOUND_HPP__
#define __SOUND_HPP__

namespace sound {

/* MIDI note, channel and instrument */
typedef long note;
typedef long channel;
typedef long instrument;

/* sequencer message types */
typedef enum {

    st_noteon, st_noteoff, st_instchange, st_attack, st_release,
    st_legato, st_portamento, st_vibrato, st_volsynthchan,
    st_porttime, st_balance, st_pan, st_timbre, st_brightness,
    st_reverb, st_tremulo, st_chorus, st_celeste, st_phaser,
    st_aftertouch, st_pressure, st_pitch, st_pitchrange, st_mono,
    st_poly, st_playsynth, st_playwave, st_volwave

} seqtyp;

/* sequencer message */
typedef struct seqmsg {

    struct seqmsg* next; /* next message in list */
    long           port; /* port to which message applies */
    long           time; /* time to execute message */
    seqtyp         st;   /* type of message */
    union {

        /* st_noteon st_noteoff st_aftertouch st_pressure */
        struct { channel ntc; note ntn; long ntv; };
        /* st_instchange */ struct { channel icc; instrument ici; };
        /* st_attack, st_release, st_vibrato, st_volsynthchan,
           st_porttime, st_balance, st_pan, st_timbre, st_brightness,
           st_reverb, st_tremulo, st_chorus, st_celeste, st_phaser,
           st_pitch, st_pitchrange, st_mono */
        struct { channel vsc; long vsv; };
        /* st_poly */ channel pc;
        /* st_legato, st_portamento */ struct { channel bsc; long bsb; };
        /* st_playsynth */ long sid;
        /* st_playwave */ long wt;
        /* st_volwave */ long wv;

    };

} seqmsg;

/* pointer to message */
typedef seqmsg* seqptr;

/* the constants of sound.h, prefixes stripped */
const long chan_drum                    =  10;
const long synth_out                    =   1;
const long synth_in                     =   1;
const long wave_in                      =   1;
const long wave_out                     =   1;
const long note_c                       =   1;
const long note_c_sharp                 =   2;
const long note_d_flat                  =   2;
const long note_d                       =   3;
const long note_d_sharp                 =   4;
const long note_e_flat                  =   4;
const long note_e                       =   5;
const long note_f                       =   6;
const long note_f_sharp                 =   7;
const long note_g_flat                  =   7;
const long note_g                       =   8;
const long note_g_sharp                 =   9;
const long note_a_flat                  =   9;
const long note_a                       =  10;
const long note_a_sharp                 =  11;
const long note_b_flat                  =  11;
const long note_b                       =  12;
const long octave_1                     =   0;
const long octave_2                     =  12;
const long octave_3                     =  24;
const long octave_4                     =  36;
const long octave_5                     =  48;
const long octave_6                     =  60;
const long octave_7                     =  72;
const long octave_8                     =  84;
const long octave_9                     =  96;
const long octave_10                    = 108;
const long octave_11                    = 120;
const long inst_acoustic_grand          =   1;
const long inst_bright_acoustic         =   2;
const long inst_electric_grand          =   3;
const long inst_honky_tonk              =   4;
const long inst_electric_piano_1        =   5;
const long inst_electric_piano_2        =   6;
const long inst_harpsichord             =   7;
const long inst_clavinet                =   8;
const long inst_celesta                 =   9;
const long inst_glockenspiel            =  10;
const long inst_music_box               =  11;
const long inst_vibraphone              =  12;
const long inst_marimba                 =  13;
const long inst_xylophone               =  14;
const long inst_tubular_bells           =  15;
const long inst_dulcimer                =  16;
const long inst_drawbar_organ           =  17;
const long inst_percussive_organ        =  18;
const long inst_rock_organ              =  19;
const long inst_church_organ            =  20;
const long inst_reed_organ              =  21;
const long inst_accoridan               =  22;
const long inst_harmonica               =  23;
const long inst_tango_accordian         =  24;
const long inst_nylon_string_guitar     =  25;
const long inst_steel_string_guitar     =  26;
const long inst_electric_jazz_guitar    =  27;
const long inst_electric_clean_guitar   =  28;
const long inst_electric_muted_guitar   =  29;
const long inst_overdriven_guitar       =  30;
const long inst_distortion_guitar       =  31;
const long inst_guitar_harmonics        =  32;
const long inst_acoustic_bass           =  33;
const long inst_electric_bass_finger    =  34;
const long inst_electric_bass_pick      =  35;
const long inst_fretless_bass           =  36;
const long inst_slap_bass_1             =  37;
const long inst_slap_bass_2             =  38;
const long inst_synth_bass_1            =  39;
const long inst_synth_bass_2            =  40;
const long inst_violin                  =  41;
const long inst_viola                   =  42;
const long inst_cello                   =  43;
const long inst_contrabass              =  44;
const long inst_tremolo_strings         =  45;
const long inst_pizzicato_strings       =  46;
const long inst_orchestral_strings      =  47;
const long inst_timpani                 =  48;
const long inst_string_ensemble_1       =  49;
const long inst_string_ensemble_2       =  50;
const long inst_synthstrings_1          =  51;
const long inst_synthstrings_2          =  52;
const long inst_choir_aahs              =  53;
const long inst_voice_oohs              =  54;
const long inst_synth_voice             =  55;
const long inst_orchestra_hit           =  56;
const long inst_trumpet                 =  57;
const long inst_trombone                =  58;
const long inst_tuba                    =  59;
const long inst_muted_trumpet           =  60;
const long inst_french_horn             =  61;
const long inst_brass_section           =  62;
const long inst_synthbrass_1            =  63;
const long inst_synthbrass_2            =  64;
const long inst_soprano_sax             =  65;
const long inst_alto_sax                =  66;
const long inst_tenor_sax               =  67;
const long inst_baritone_sax            =  68;
const long inst_oboe                    =  69;
const long inst_english_horn            =  70;
const long inst_bassoon                 =  71;
const long inst_clarinet                =  72;
const long inst_piccolo                 =  73;
const long inst_flute                   =  74;
const long inst_recorder                =  75;
const long inst_pan_flute               =  76;
const long inst_blown_bottle            =  77;
const long inst_skakuhachi              =  78;
const long inst_whistle                 =  79;
const long inst_ocarina                 =  80;
const long inst_lead_1_square           =  81;
const long inst_lead_2_sawtooth         =  82;
const long inst_lead_3_calliope         =  83;
const long inst_lead_4_chiff            =  84;
const long inst_lead_5_charang          =  85;
const long inst_lead_6_voice            =  86;
const long inst_lead_7_fifths           =  87;
const long inst_lead_8_bass_lead        =  88;
const long inst_pad_1_new_age           =  89;
const long inst_pad_2_warm              =  90;
const long inst_pad_3_polysynth         =  91;
const long inst_pad_4_choir             =  92;
const long inst_pad_5_bowed             =  93;
const long inst_pad_6_metallic          =  94;
const long inst_pad_7_halo              =  95;
const long inst_pad_8_sweep             =  96;
const long inst_fx_1_rain               =  97;
const long inst_fx_2_soundtrack         =  98;
const long inst_fx_3_crystal            =  99;
const long inst_fx_4_atmosphere         = 100;
const long inst_fx_5_brightness         = 101;
const long inst_fx_6_goblins            = 102;
const long inst_fx_7_echoes             = 103;
const long inst_fx_8_sci_fi             = 104;
const long inst_sitar                   = 105;
const long inst_banjo                   = 106;
const long inst_shamisen                = 107;
const long inst_koto                    = 108;
const long inst_kalimba                 = 109;
const long inst_bagpipe                 = 110;
const long inst_fiddle                  = 111;
const long inst_shanai                  = 112;
const long inst_tinkle_bell             = 113;
const long inst_agogo                   = 114;
const long inst_steel_drums             = 115;
const long inst_woodblock               = 116;
const long inst_taiko_drum              = 117;
const long inst_melodic_tom             = 118;
const long inst_synth_drum              = 119;
const long inst_reverse_cymbal          = 120;
const long inst_guitar_fret_noise       = 121;
const long inst_breath_noise            = 122;
const long inst_seashore                = 123;
const long inst_bird_tweet              = 124;
const long inst_telephone_ring          = 125;
const long inst_helicopter              = 126;
const long inst_applause                = 127;
const long inst_gunshot                 = 128;
const long note_acoustic_bass_drum      =  35;
const long note_bass_drum_1             =  36;
const long note_side_stick              =  37;
const long note_acoustic_snare          =  38;
const long note_hand_clap               =  39;
const long note_electric_snare          =  40;
const long note_low_floor_tom           =  41;
const long note_closed_hi_hat           =  42;
const long note_high_floor_tom          =  43;
const long note_pedal_hi_hat            =  44;
const long note_low_tom                 =  45;
const long note_open_hi_hat             =  46;
const long note_low_mid_tom             =  47;
const long note_hi_mid_tom              =  48;
const long note_crash_cymbal_1          =  49;
const long note_high_tom                =  50;
const long note_ride_cymbal_1           =  51;
const long note_chinese_cymbal          =  52;
const long note_ride_bell               =  53;
const long note_tambourine              =  54;
const long note_splash_cymbal           =  55;
const long note_cowbell                 =  56;
const long note_crash_cymbal_2          =  57;
const long note_vibraslap               =  58;
const long note_ride_cymbal_2           =  59;
const long note_hi_bongo                =  60;
const long note_low_bongo               =  61;
const long note_mute_hi_conga           =  62;
const long note_open_hi_conga           =  63;
const long note_low_conga               =  64;
const long note_high_timbale            =  65;
const long note_low_timbale             =  66;
const long note_high_agogo              =  67;
const long note_low_agogo               =  68;
const long note_cabasa                  =  69;
const long note_maracas                 =  70;
const long note_short_whistle           =  71;
const long note_long_whistle            =  72;
const long note_short_guiro             =  73;
const long note_long_guiro              =  74;
const long note_claves                  =  75;
const long note_hi_wood_block           =  76;
const long note_low_wood_block          =  77;
const long note_mute_cuica              =  78;
const long note_open_cuica              =  79;
const long note_mute_triangle           =  80;
const long note_open_triangle           =  81;

/* procedures and functions */
void starttimeout(void);
void stoptimeout(void);
long curtimeout(void);
void starttimein(void);
void stoptimein(void);
long curtimein(void);
long synthout(void);
long synthin(void);
long waveout(void);
long wavein(void);
void loadsynth(long s, const char* sf);
void delsynth(long s);
void loadwave(long w, const char* fn);
void delwave(long w);
void opensynthout(long p);
void opensynthout(void);
void closesynthout(long p);
void closesynthout(void);
void opensynthin(long p);
void opensynthin(void);
void closesynthin(long p);
void closesynthin(void);
void noteon(long p, long t, channel c, note n, long v);
void noteon(long t, channel c, note n, long v);
void noteoff(long p, long t, channel c, note n, long v);
void noteoff(long t, channel c, note n, long v);
void instchange(long p, long t, channel c, instrument i);
void instchange(long t, channel c, instrument i);
void attack(long p, long t, channel c, long at);
void attack(long t, channel c, long at);
void release(long p, long t, channel c, long rt);
void release(long t, channel c, long rt);
void legato(long p, long t, channel c, long b);
void legato(long t, channel c, long b);
void portamento(long p, long t, channel c, long b);
void portamento(long t, channel c, long b);
void vibrato(long p, long t, channel c, long v);
void vibrato(long t, channel c, long v);
void volsynthchan(long p, long t, channel c, long v);
void volsynthchan(long t, channel c, long v);
void porttime(long p, long t, channel c, long v);
void porttime(long t, channel c, long v);
void balance(long p, long t, channel c, long b);
void balance(long t, channel c, long b);
void pan(long p, long t, channel c, long b);
void pan(long t, channel c, long b);
void timbre(long p, long t, channel c, long tb);
void timbre(long t, channel c, long tb);
void brightness(long p, long t, channel c, long b);
void brightness(long t, channel c, long b);
void reverb(long p, long t, channel c, long r);
void reverb(long t, channel c, long r);
void tremulo(long p, long t, channel c, long tr);
void tremulo(long t, channel c, long tr);
void chorus(long p, long t, channel c, long cr);
void chorus(long t, channel c, long cr);
void celeste(long p, long t, channel c, long ce);
void celeste(long t, channel c, long ce);
void phaser(long p, long t, channel c, long ph);
void phaser(long t, channel c, long ph);
void aftertouch(long p, long t, channel c, note n, long at);
void aftertouch(long t, channel c, note n, long at);
void pressure(long p, long t, channel c, long pr);
void pressure(long t, channel c, long pr);
void pitch(long p, long t, channel c, long pt);
void pitch(long t, channel c, long pt);
void pitchrange(long p, long t, channel c, long v);
void pitchrange(long t, channel c, long v);
void mono(long p, long t, channel c, long ch);
void mono(long t, channel c, long ch);
void poly(long p, long t, channel c);
void poly(long t, channel c);
void playsynth(long p, long t, long s);
void playsynth(long t, long s);
void waitsynth(long p);
void waitsynth(void);
void wrsynth(long p, seqptr sp);
void wrsynth(seqptr sp);
void rdsynth(long p, seqptr sp);
void rdsynth(seqptr sp);
void synthoutname(long p, char* name, long len);
void synthoutname(char* name, long len);
void synthinname(long p, char* name, long len);
void synthinname(char* name, long len);
long setparamsynthout(long p, const char* name, const char* value);
long setparamsynthout(const char* name, const char* value);
long setparamsynthin(long p, const char* name, const char* value);
long setparamsynthin(const char* name, const char* value);
void getparamsynthout(long p, const char* name, char* value, long len);
void getparamsynthout(const char* name, char* value, long len);
void getparamsynthin(long p, const char* name, char* value, long len);
void getparamsynthin(const char* name, char* value, long len);
void openwaveout(long p);
void openwaveout(void);
void closewaveout(long p);
void closewaveout(void);
void openwavein(long p);
void openwavein(void);
void closewavein(long p);
void closewavein(void);
void playwave(long p, long t, long w);
void playwave(long t, long w);
void volwave(long p, long t, long v);
void volwave(long t, long v);
void waitwave(long p);
void waitwave(void);
void chanwaveout(long p, long c);
void chanwaveout(long c);
void ratewaveout(long p, long r);
void ratewaveout(long r);
void lenwaveout(long p, long l);
void lenwaveout(long l);
void sgnwaveout(long p, long s);
void sgnwaveout(long s);
void fltwaveout(long p, long f);
void fltwaveout(long f);
void endwaveout(long p, long e);
void endwaveout(long e);
void wrwave(long p, unsigned char* buff, long len);
void wrwave(unsigned char* buff, long len);
long chanwavein(long p);
long chanwavein(void);
long ratewavein(long p);
long ratewavein(void);
long lenwavein(long p);
long lenwavein(void);
long sgnwavein(long p);
long sgnwavein(void);
long endwavein(long p);
long endwavein(void);
long fltwavein(long p);
long fltwavein(void);
long rdwave(long p, unsigned char* buff, long len);
long rdwave(unsigned char* buff, long len);
void waveoutname(long p, char* name, long len);
void waveoutname(char* name, long len);
void waveinname(long p, char* name, long len);
void waveinname(char* name, long len);
long setparamwaveout(long p, const char* name, const char* value);
long setparamwaveout(const char* name, const char* value);
long setparamwavein(long p, const char* name, const char* value);
long setparamwavein(const char* name, const char* value);
void getparamwaveout(long p, const char* name, char* value, long len);
void getparamwaveout(const char* name, char* value, long len);
void getparamwavein(long p, const char* name, char* value, long len);
void getparamwavein(const char* name, char* value, long len);

/*******************************************************************************

The port objects

A synth holds a synthesizer output port and a synthesizer input port; a
wave holds the same pair of waveform ports. Opening sets the port the
object speaks for, and every call after that leaves the port off. The
destructor closes whatever the object still holds open, so a port
cannot leak by an early return.

Unlike the term and graph objects of the other wrappers, these hook
nothing: any number of them can exist at once, one per port in use.

*******************************************************************************/

class synth {

long outport; /* the output port held, 0 when closed */
long inport;  /* the input port held, 0 when closed */

public:

/* constructor */
synth();

/* destructor */
~synth();

/* methods */
void opensynthout(long p = synth_out);
void closesynthout(void);
void opensynthin(long p = synth_in);
void closesynthin(void);
void noteon(long t, channel c, note n, long v);
void noteoff(long t, channel c, note n, long v);
void instchange(long t, channel c, instrument i);
void attack(long t, channel c, long at);
void release(long t, channel c, long rt);
void legato(long t, channel c, long b);
void portamento(long t, channel c, long b);
void vibrato(long t, channel c, long v);
void volsynthchan(long t, channel c, long v);
void porttime(long t, channel c, long v);
void balance(long t, channel c, long b);
void pan(long t, channel c, long b);
void timbre(long t, channel c, long tb);
void brightness(long t, channel c, long b);
void reverb(long t, channel c, long r);
void tremulo(long t, channel c, long tr);
void chorus(long t, channel c, long cr);
void celeste(long t, channel c, long ce);
void phaser(long t, channel c, long ph);
void aftertouch(long t, channel c, note n, long at);
void pressure(long t, channel c, long pr);
void pitch(long t, channel c, long pt);
void pitchrange(long t, channel c, long v);
void mono(long t, channel c, long ch);
void poly(long t, channel c);
void playsynth(long t, long s);
void waitsynth(void);
void wrsynth(seqptr sp);
void rdsynth(seqptr sp);
void synthoutname(char* name, long len);
void synthinname(char* name, long len);
long setparamsynthout(const char* name, const char* value);
long setparamsynthin(const char* name, const char* value);
void getparamsynthout(const char* name, char* value, long len);
void getparamsynthin(const char* name, char* value, long len);

}; /* class synth */

class wave {

long outport; /* the output port held, 0 when closed */
long inport;  /* the input port held, 0 when closed */

public:

/* constructor */
wave();

/* destructor */
~wave();

/* methods */
void openwaveout(long p = wave_out);
void closewaveout(void);
void openwavein(long p = wave_in);
void closewavein(void);
void playwave(long t, long w);
void volwave(long t, long v);
void waitwave(void);
void chanwaveout(long c);
void ratewaveout(long r);
void lenwaveout(long l);
void sgnwaveout(long s);
void fltwaveout(long f);
void endwaveout(long e);
void wrwave(unsigned char* buff, long len);
long chanwavein(void);
long ratewavein(void);
long lenwavein(void);
long sgnwavein(void);
long endwavein(void);
long fltwavein(void);
long rdwave(unsigned char* buff, long len);
void waveoutname(char* name, long len);
void waveinname(char* name, long len);
long setparamwaveout(const char* name, const char* value);
long setparamwavein(const char* name, const char* value);
void getparamwaveout(const char* name, char* value, long len);
void getparamwavein(const char* name, char* value, long len);

}; /* class wave */

} /* namespace sound */

#endif /* __SOUND_HPP__ */
