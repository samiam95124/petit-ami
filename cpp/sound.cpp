/** ****************************************************************************
 *
 * Sound library interface C++ wrapper
 *
 * Wraps the calls in sound with C++ conventions, the same way
 * cpp/terminal.cpp wraps the terminal calls. This brings:
 *
 * 1. The functions, types and constants do not need an "ami_" prefix:
 * the sound namespace does the isolation. The constants of sound.h
 * appear in lower case, note_c to inst_gunshot.
 *
 * 2. Every call that takes a port has an overload without it, meaning
 * the default port.
 *
 * 3. The synth and wave objects hold ports. Opening sets the port the
 * object speaks for -- s.opensynthout() or s.opensynthout(2) -- and
 * every call after that leaves the port off: s.noteon(0, 1, note_c+
 * octave_6, v). An object holds one port of each direction, so one
 * synth can serve a keyboard in and a synthesizer out. The destructor
 * closes whatever is still open.
 *
 * Unlike term and graph, sound has no event callbacks to invert, so
 * these objects hook nothing and any number of them may exist.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

extern "C" {

#include <stdio.h>

#include <localdefs.h>
#include <sound.h>

}

#include "sound.hpp"

namespace sound {

/* procedures and functions */
void starttimeout(void) { ami_starttimeout(); }
void stoptimeout(void) { ami_stoptimeout(); }
long curtimeout(void) { return ami_curtimeout(); }
void starttimein(void) { ami_starttimein(); }
void stoptimein(void) { ami_stoptimein(); }
long curtimein(void) { return ami_curtimein(); }
long synthout(void) { return ami_synthout(); }
long synthin(void) { return ami_synthin(); }
long waveout(void) { return ami_waveout(); }
long wavein(void) { return ami_wavein(); }
void loadsynth(long s, const char* sf) { ami_loadsynth(s, (char*)sf); }
void delsynth(long s) { ami_delsynth(s); }
void loadwave(long w, const char* fn) { ami_loadwave(w, (char*)fn); }
void delwave(long w) { ami_delwave(w); }
void opensynthout(long p) { ami_opensynthout(p); }
void opensynthout(void) { ami_opensynthout(1); }
void closesynthout(long p) { ami_closesynthout(p); }
void closesynthout(void) { ami_closesynthout(1); }
void opensynthin(long p) { ami_opensynthin(p); }
void opensynthin(void) { ami_opensynthin(1); }
void closesynthin(long p) { ami_closesynthin(p); }
void closesynthin(void) { ami_closesynthin(1); }
void noteon(long p, long t, channel c, note n, long v) { ami_noteon(p, t, c, n, v); }
void noteon(long t, channel c, note n, long v) { ami_noteon(1, t, c, n, v); }
void noteoff(long p, long t, channel c, note n, long v) { ami_noteoff(p, t, c, n, v); }
void noteoff(long t, channel c, note n, long v) { ami_noteoff(1, t, c, n, v); }
void instchange(long p, long t, channel c, instrument i) { ami_instchange(p, t, c, i); }
void instchange(long t, channel c, instrument i) { ami_instchange(1, t, c, i); }
void attack(long p, long t, channel c, long at) { ami_attack(p, t, c, at); }
void attack(long t, channel c, long at) { ami_attack(1, t, c, at); }
void release(long p, long t, channel c, long rt) { ami_release(p, t, c, rt); }
void release(long t, channel c, long rt) { ami_release(1, t, c, rt); }
void legato(long p, long t, channel c, long b) { ami_legato(p, t, c, b); }
void legato(long t, channel c, long b) { ami_legato(1, t, c, b); }
void portamento(long p, long t, channel c, long b) { ami_portamento(p, t, c, b); }
void portamento(long t, channel c, long b) { ami_portamento(1, t, c, b); }
void vibrato(long p, long t, channel c, long v) { ami_vibrato(p, t, c, v); }
void vibrato(long t, channel c, long v) { ami_vibrato(1, t, c, v); }
void volsynthchan(long p, long t, channel c, long v) { ami_volsynthchan(p, t, c, v); }
void volsynthchan(long t, channel c, long v) { ami_volsynthchan(1, t, c, v); }
void porttime(long p, long t, channel c, long v) { ami_porttime(p, t, c, v); }
void porttime(long t, channel c, long v) { ami_porttime(1, t, c, v); }
void balance(long p, long t, channel c, long b) { ami_balance(p, t, c, b); }
void balance(long t, channel c, long b) { ami_balance(1, t, c, b); }
void pan(long p, long t, channel c, long b) { ami_pan(p, t, c, b); }
void pan(long t, channel c, long b) { ami_pan(1, t, c, b); }
void timbre(long p, long t, channel c, long tb) { ami_timbre(p, t, c, tb); }
void timbre(long t, channel c, long tb) { ami_timbre(1, t, c, tb); }
void brightness(long p, long t, channel c, long b) { ami_brightness(p, t, c, b); }
void brightness(long t, channel c, long b) { ami_brightness(1, t, c, b); }
void reverb(long p, long t, channel c, long r) { ami_reverb(p, t, c, r); }
void reverb(long t, channel c, long r) { ami_reverb(1, t, c, r); }
void tremulo(long p, long t, channel c, long tr) { ami_tremulo(p, t, c, tr); }
void tremulo(long t, channel c, long tr) { ami_tremulo(1, t, c, tr); }
void chorus(long p, long t, channel c, long cr) { ami_chorus(p, t, c, cr); }
void chorus(long t, channel c, long cr) { ami_chorus(1, t, c, cr); }
void celeste(long p, long t, channel c, long ce) { ami_celeste(p, t, c, ce); }
void celeste(long t, channel c, long ce) { ami_celeste(1, t, c, ce); }
void phaser(long p, long t, channel c, long ph) { ami_phaser(p, t, c, ph); }
void phaser(long t, channel c, long ph) { ami_phaser(1, t, c, ph); }
void aftertouch(long p, long t, channel c, note n, long at) { ami_aftertouch(p, t, c, n, at); }
void aftertouch(long t, channel c, note n, long at) { ami_aftertouch(1, t, c, n, at); }
void pressure(long p, long t, channel c, long pr) { ami_pressure(p, t, c, pr); }
void pressure(long t, channel c, long pr) { ami_pressure(1, t, c, pr); }
void pitch(long p, long t, channel c, long pt) { ami_pitch(p, t, c, pt); }
void pitch(long t, channel c, long pt) { ami_pitch(1, t, c, pt); }
void pitchrange(long p, long t, channel c, long v) { ami_pitchrange(p, t, c, v); }
void pitchrange(long t, channel c, long v) { ami_pitchrange(1, t, c, v); }
void mono(long p, long t, channel c, long ch) { ami_mono(p, t, c, ch); }
void mono(long t, channel c, long ch) { ami_mono(1, t, c, ch); }
void poly(long p, long t, channel c) { ami_poly(p, t, c); }
void poly(long t, channel c) { ami_poly(1, t, c); }
void playsynth(long p, long t, long s) { ami_playsynth(p, t, s); }
void playsynth(long t, long s) { ami_playsynth(1, t, s); }
void waitsynth(long p) { ami_waitsynth(p); }
void waitsynth(void) { ami_waitsynth(1); }
void wrsynth(long p, seqptr sp) { ami_wrsynth(p, (ami_seqptr)sp); }
void wrsynth(seqptr sp) { ami_wrsynth(1, (ami_seqptr)sp); }
void rdsynth(long p, seqptr sp) { ami_rdsynth(p, (ami_seqptr)sp); }
void rdsynth(seqptr sp) { ami_rdsynth(1, (ami_seqptr)sp); }
void synthoutname(long p, char* name, long len) { ami_synthoutname(p, name, len); }
void synthoutname(char* name, long len) { ami_synthoutname(1, name, len); }
void synthinname(long p, char* name, long len) { ami_synthinname(p, name, len); }
void synthinname(char* name, long len) { ami_synthinname(1, name, len); }
long setparamsynthout(long p, const char* name, const char* value) { return ami_setparamsynthout(p, (char*)name, (char*)value); }
long setparamsynthout(const char* name, const char* value) { return ami_setparamsynthout(1, (char*)name, (char*)value); }
long setparamsynthin(long p, const char* name, const char* value) { return ami_setparamsynthin(p, (char*)name, (char*)value); }
long setparamsynthin(const char* name, const char* value) { return ami_setparamsynthin(1, (char*)name, (char*)value); }
void getparamsynthout(long p, const char* name, char* value, long len) { ami_getparamsynthout(p, (char*)name, value, len); }
void getparamsynthout(const char* name, char* value, long len) { ami_getparamsynthout(1, (char*)name, value, len); }
void getparamsynthin(long p, const char* name, char* value, long len) { ami_getparamsynthin(p, (char*)name, value, len); }
void getparamsynthin(const char* name, char* value, long len) { ami_getparamsynthin(1, (char*)name, value, len); }
void openwaveout(long p) { ami_openwaveout(p); }
void openwaveout(void) { ami_openwaveout(1); }
void closewaveout(long p) { ami_closewaveout(p); }
void closewaveout(void) { ami_closewaveout(1); }
void openwavein(long p) { ami_openwavein(p); }
void openwavein(void) { ami_openwavein(1); }
void closewavein(long p) { ami_closewavein(p); }
void closewavein(void) { ami_closewavein(1); }
void playwave(long p, long t, long w) { ami_playwave(p, t, w); }
void playwave(long t, long w) { ami_playwave(1, t, w); }
void volwave(long p, long t, long v) { ami_volwave(p, t, v); }
void volwave(long t, long v) { ami_volwave(1, t, v); }
void waitwave(long p) { ami_waitwave(p); }
void waitwave(void) { ami_waitwave(1); }
void chanwaveout(long p, long c) { ami_chanwaveout(p, c); }
void chanwaveout(long c) { ami_chanwaveout(1, c); }
void ratewaveout(long p, long r) { ami_ratewaveout(p, r); }
void ratewaveout(long r) { ami_ratewaveout(1, r); }
void lenwaveout(long p, long l) { ami_lenwaveout(p, l); }
void lenwaveout(long l) { ami_lenwaveout(1, l); }
void sgnwaveout(long p, long s) { ami_sgnwaveout(p, s); }
void sgnwaveout(long s) { ami_sgnwaveout(1, s); }
void fltwaveout(long p, long f) { ami_fltwaveout(p, f); }
void fltwaveout(long f) { ami_fltwaveout(1, f); }
void endwaveout(long p, long e) { ami_endwaveout(p, e); }
void endwaveout(long e) { ami_endwaveout(1, e); }
void wrwave(long p, unsigned char* buff, long len) { ami_wrwave(p, (byte*)buff, len); }
void wrwave(unsigned char* buff, long len) { ami_wrwave(1, (byte*)buff, len); }
long chanwavein(long p) { return ami_chanwavein(p); }
long chanwavein(void) { return ami_chanwavein(1); }
long ratewavein(long p) { return ami_ratewavein(p); }
long ratewavein(void) { return ami_ratewavein(1); }
long lenwavein(long p) { return ami_lenwavein(p); }
long lenwavein(void) { return ami_lenwavein(1); }
long sgnwavein(long p) { return ami_sgnwavein(p); }
long sgnwavein(void) { return ami_sgnwavein(1); }
long endwavein(long p) { return ami_endwavein(p); }
long endwavein(void) { return ami_endwavein(1); }
long fltwavein(long p) { return ami_fltwavein(p); }
long fltwavein(void) { return ami_fltwavein(1); }
long rdwave(long p, unsigned char* buff, long len) { return ami_rdwave(p, (byte*)buff, len); }
long rdwave(unsigned char* buff, long len) { return ami_rdwave(1, (byte*)buff, len); }
void waveoutname(long p, char* name, long len) { ami_waveoutname(p, name, len); }
void waveoutname(char* name, long len) { ami_waveoutname(1, name, len); }
void waveinname(long p, char* name, long len) { ami_waveinname(p, name, len); }
void waveinname(char* name, long len) { ami_waveinname(1, name, len); }
long setparamwaveout(long p, const char* name, const char* value) { return ami_setparamwaveout(p, (char*)name, (char*)value); }
long setparamwaveout(const char* name, const char* value) { return ami_setparamwaveout(1, (char*)name, (char*)value); }
long setparamwavein(long p, const char* name, const char* value) { return ami_setparamwavein(p, (char*)name, (char*)value); }
long setparamwavein(const char* name, const char* value) { return ami_setparamwavein(1, (char*)name, (char*)value); }
void getparamwaveout(long p, const char* name, char* value, long len) { ami_getparamwaveout(p, (char*)name, value, len); }
void getparamwaveout(const char* name, char* value, long len) { ami_getparamwaveout(1, (char*)name, value, len); }
void getparamwavein(long p, const char* name, char* value, long len) { ami_getparamwavein(p, (char*)name, value, len); }
void getparamwavein(const char* name, char* value, long len) { ami_getparamwavein(1, (char*)name, value, len); }

/* methods */
synth::synth(void)

{

    outport = 0; /* nothing held yet */
    inport = 0;

}

synth::~synth(void)

{

    /* close whatever the object still holds */
    if (outport) ami_closesynthout(outport);
    if (inport) ami_closesynthin(inport);

}

void synth::opensynthout(long p)

{

    /* an object holds one port a direction: opening over a held port
       lets go of the old one first */
    if (outport) ami_closesynthout(outport);
    ami_opensynthout(p);
    outport = p;

}

void synth::closesynthout(void)

{

    if (outport) { ami_closesynthout(outport); outport = 0; }

}

void synth::opensynthin(long p)

{

    if (inport) ami_closesynthin(inport);
    ami_opensynthin(p);
    inport = p;

}

void synth::closesynthin(void)

{

    if (inport) { ami_closesynthin(inport); inport = 0; }

}

void synth::noteon(long t, channel c, note n, long v) { ami_noteon(outport, t, c, n, v); }
void synth::noteoff(long t, channel c, note n, long v) { ami_noteoff(outport, t, c, n, v); }
void synth::instchange(long t, channel c, instrument i) { ami_instchange(outport, t, c, i); }
void synth::attack(long t, channel c, long at) { ami_attack(outport, t, c, at); }
void synth::release(long t, channel c, long rt) { ami_release(outport, t, c, rt); }
void synth::legato(long t, channel c, long b) { ami_legato(outport, t, c, b); }
void synth::portamento(long t, channel c, long b) { ami_portamento(outport, t, c, b); }
void synth::vibrato(long t, channel c, long v) { ami_vibrato(outport, t, c, v); }
void synth::volsynthchan(long t, channel c, long v) { ami_volsynthchan(outport, t, c, v); }
void synth::porttime(long t, channel c, long v) { ami_porttime(outport, t, c, v); }
void synth::balance(long t, channel c, long b) { ami_balance(outport, t, c, b); }
void synth::pan(long t, channel c, long b) { ami_pan(outport, t, c, b); }
void synth::timbre(long t, channel c, long tb) { ami_timbre(outport, t, c, tb); }
void synth::brightness(long t, channel c, long b) { ami_brightness(outport, t, c, b); }
void synth::reverb(long t, channel c, long r) { ami_reverb(outport, t, c, r); }
void synth::tremulo(long t, channel c, long tr) { ami_tremulo(outport, t, c, tr); }
void synth::chorus(long t, channel c, long cr) { ami_chorus(outport, t, c, cr); }
void synth::celeste(long t, channel c, long ce) { ami_celeste(outport, t, c, ce); }
void synth::phaser(long t, channel c, long ph) { ami_phaser(outport, t, c, ph); }
void synth::aftertouch(long t, channel c, note n, long at) { ami_aftertouch(outport, t, c, n, at); }
void synth::pressure(long t, channel c, long pr) { ami_pressure(outport, t, c, pr); }
void synth::pitch(long t, channel c, long pt) { ami_pitch(outport, t, c, pt); }
void synth::pitchrange(long t, channel c, long v) { ami_pitchrange(outport, t, c, v); }
void synth::mono(long t, channel c, long ch) { ami_mono(outport, t, c, ch); }
void synth::poly(long t, channel c) { ami_poly(outport, t, c); }
void synth::playsynth(long t, long s) { ami_playsynth(outport, t, s); }
void synth::waitsynth(void) { ami_waitsynth(outport); }
void synth::wrsynth(seqptr sp) { ami_wrsynth(outport, (ami_seqptr)sp); }
void synth::rdsynth(seqptr sp) { ami_rdsynth(inport, (ami_seqptr)sp); }
void synth::synthoutname(char* name, long len) { ami_synthoutname(outport, name, len); }
void synth::synthinname(char* name, long len) { ami_synthinname(inport, name, len); }
long synth::setparamsynthout(const char* name, const char* value) { return ami_setparamsynthout(outport, (char*)name, (char*)value); }
long synth::setparamsynthin(const char* name, const char* value) { return ami_setparamsynthin(inport, (char*)name, (char*)value); }
void synth::getparamsynthout(const char* name, char* value, long len) { ami_getparamsynthout(outport, (char*)name, value, len); }
void synth::getparamsynthin(const char* name, char* value, long len) { ami_getparamsynthin(inport, (char*)name, value, len); }

wave::wave(void)

{

    outport = 0; /* nothing held yet */
    inport = 0;

}

wave::~wave(void)

{

    /* close whatever the object still holds */
    if (outport) ami_closewaveout(outport);
    if (inport) ami_closewavein(inport);

}

void wave::openwaveout(long p)

{

    /* an object holds one port a direction: opening over a held port
       lets go of the old one first */
    if (outport) ami_closewaveout(outport);
    ami_openwaveout(p);
    outport = p;

}

void wave::closewaveout(void)

{

    if (outport) { ami_closewaveout(outport); outport = 0; }

}

void wave::openwavein(long p)

{

    if (inport) ami_closewavein(inport);
    ami_openwavein(p);
    inport = p;

}

void wave::closewavein(void)

{

    if (inport) { ami_closewavein(inport); inport = 0; }

}

void wave::playwave(long t, long w) { ami_playwave(outport, t, w); }
void wave::volwave(long t, long v) { ami_volwave(outport, t, v); }
void wave::waitwave(void) { ami_waitwave(outport); }
void wave::chanwaveout(long c) { ami_chanwaveout(outport, c); }
void wave::ratewaveout(long r) { ami_ratewaveout(outport, r); }
void wave::lenwaveout(long l) { ami_lenwaveout(outport, l); }
void wave::sgnwaveout(long s) { ami_sgnwaveout(outport, s); }
void wave::fltwaveout(long f) { ami_fltwaveout(outport, f); }
void wave::endwaveout(long e) { ami_endwaveout(outport, e); }
void wave::wrwave(unsigned char* buff, long len) { ami_wrwave(outport, (byte*)buff, len); }
long wave::chanwavein(void) { return ami_chanwavein(inport); }
long wave::ratewavein(void) { return ami_ratewavein(inport); }
long wave::lenwavein(void) { return ami_lenwavein(inport); }
long wave::sgnwavein(void) { return ami_sgnwavein(inport); }
long wave::endwavein(void) { return ami_endwavein(inport); }
long wave::fltwavein(void) { return ami_fltwavein(inport); }
long wave::rdwave(unsigned char* buff, long len) { return ami_rdwave(inport, (byte*)buff, len); }
void wave::waveoutname(char* name, long len) { ami_waveoutname(outport, name, len); }
void wave::waveinname(char* name, long len) { ami_waveinname(inport, name, len); }
long wave::setparamwaveout(const char* name, const char* value) { return ami_setparamwaveout(outport, (char*)name, (char*)value); }
long wave::setparamwavein(const char* name, const char* value) { return ami_setparamwavein(inport, (char*)name, (char*)value); }
void wave::getparamwaveout(const char* name, char* value, long len) { ami_getparamwaveout(outport, (char*)name, value, len); }
void wave::getparamwavein(const char* name, char* value, long len) { ami_getparamwavein(inport, (char*)name, value, len); }

} /* namespace sound */
