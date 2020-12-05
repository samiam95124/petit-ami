{******************************************************************************
*                                                                             *
*                               SOUND LIBRARY                                 *
*                                                                             *
*                              11/02 S. A. Moore                              *
*                                                                             *
* Sndlib is a combination of wave file and midi output and control functions. *
* Implements a set of midi controls and wave controls. Also includes a "flow  *
* through sequencer" function. Each event has a timestamp, and if zero, it    *
* is performed immediately, otherwise scheduled. This allows any mix of       *
* immediate vs. sequenced events.                                             *
*                                                                             *
* Notes:                                                                      *
*                                                                             *
* 1. The parameter convertion work is being performed when a sequenced item   *
* is performed. This could be moved back to the entry of the event to save    *
* time in the timer handler.                                                  *
*                                                                             *
* 2. The validation of parameters happens both at entry time and at sequence  *
* time, need not happen on both.                                              *
*                                                                             *
* 3. The model for running MIDI and waveform files might have problems. Its   *
* supposed to be an accurate event, but its going to have file lookup time    *
* built into it, which could affect start time. A logical preload/cache       *
* model would give this package the ability to do something about that.       *
*                                                                             *
******************************************************************************}

module sndlib(output);

uses windows, { uses windows system call wrapper }
     strlib,  { string library }
     extlib,  { extention library }
     syslib;  { system library, for error message output }

const

chan_drum = 10; { the GM drum channel }

synth_out = 1; { the default output synth for host }
synth_ext = 2; { the default output to external synth }

{ the notes in the lowest octave }

note_c       = 1;
note_c_sharp = 2;
note_d_flat  = 2;
note_d       = 3;
note_d_sharp = 4;
note_e_flat  = 4;
note_e       = 5;
note_f       = 6;
note_f_sharp = 7;
note_g_flat  = 7;
note_g       = 8;
note_g_sharp = 9;
note_a_flat  = 9;
note_a       = 10;
note_a_sharp = 11;
note_b_flat  = 11;
note_b       = 12;

{ the octaves of midi, add to note to place in that octave }

octave_1  = 0;   
octave_2  = 12;  
octave_3  = 24;  
octave_4  = 36;  
octave_5  = 48;  
octave_6  = 60;  
octave_7  = 72;  
octave_8  = 84;  
octave_9  = 96;  
octave_10 = 108; 
octave_11 = 120;

{ Standard GM instruments }

{ Piano }

inst_acoustic_grand        = 1;
inst_bright_acoustic       = 2;
inst_electric_grand        = 3;
inst_honky_tonk            = 4;
inst_electric_piano_1      = 5;
inst_electric_piano_2      = 6;
inst_harpsichord           = 7;
inst_clavinet              = 8;

{ Chromatic percussion }

inst_celesta               = 9;
inst_glockenspiel          = 10;
inst_music_box             = 11;
inst_vibraphone            = 12;
inst_marimba               = 13;
inst_xylophone             = 14;
inst_tubular_bells         = 15;
inst_dulcimer              = 16;

{ Organ }
                         
inst_drawbar_organ         = 17;
inst_percussive_organ      = 18;
inst_rock_organ            = 19;
inst_church_organ          = 20;
inst_reed_organ            = 21;
inst_accoridan             = 22;
inst_harmonica             = 23;
inst_tango_accordian       = 24;

{ Guitar }

inst_nylon_string_guitar   = 25;
inst_steel_string_guitar   = 26;
inst_electric_jazz_guitar  = 27;
inst_electric_clean_guitar = 28;
inst_electric_muted_guitar = 29;
inst_overdriven_guitar     = 30;
inst_distortion_guitar     = 31;
inst_guitar_harmonics      = 32;

{ Bass }

inst_acoustic_bass         = 33;
inst_electric_bass_finger  = 34;
inst_electric_bass_pick    = 35;
inst_fretless_bass         = 36;
inst_slap_bass_1           = 37;
inst_slap_bass_2           = 38;
inst_synth_bass_1          = 39;
inst_synth_bass_2          = 40;

{ Solo strings }

inst_violin                = 41;
inst_viola                 = 42;
inst_cello                 = 43;
inst_contrabass            = 44;
inst_tremolo_strings       = 45;
inst_pizzicato_strings     = 46;
inst_orchestral_strings    = 47;
inst_timpani               = 48;

{ Ensemble }

inst_string_ensemble_1     = 49;
inst_string_ensemble_2     = 50;
inst_synthstrings_1        = 51;
inst_synthstrings_2        = 52;
inst_choir_aahs            = 53;
inst_voice_oohs            = 54;
inst_synth_voice           = 55;
inst_orchestra_hit         = 56;

{ Brass }

inst_trumpet               = 57;
inst_trombone              = 58;
inst_tuba                  = 59;
inst_muted_trumpet         = 60;
inst_french_horn           = 61;
inst_brass_section         = 62;
inst_synthbrass_1          = 63;
inst_synthbrass_2          = 64;

{ Reed }
                        
inst_soprano_sax           = 65;
inst_alto_sax              = 66;
inst_tenor_sax             = 67;
inst_baritone_sax          = 68;
inst_oboe                  = 69;
inst_english_horn          = 70;
inst_bassoon               = 71;
inst_clarinet              = 72;

{ Pipe }

inst_piccolo               = 73;
inst_flute                 = 74;
inst_recorder              = 75;
inst_pan_flute             = 76;
inst_blown_bottle          = 77;
inst_skakuhachi            = 78;
inst_whistle               = 79;
inst_ocarina               = 80;

{ Synth lead }
                   
inst_lead_1_square         = 81;
inst_lead_2_sawtooth       = 82;
inst_lead_3_calliope       = 83;
inst_lead_4_chiff          = 84;
inst_lead_5_charang        = 85;
inst_lead_6_voice          = 86;
inst_lead_7_fifths         = 87;
inst_lead_8_bass_lead      = 88;

{ Synth pad }

inst_pad_1_new_age         = 89;
inst_pad_2_warm            = 90;
inst_pad_3_polysynth       = 91;
inst_pad_4_choir           = 92;
inst_pad_5_bowed           = 93;
inst_pad_6_metallic        = 94;
inst_pad_7_halo            = 95;
inst_pad_8_sweep           = 96;

{ Synth effects }

inst_fx_1_rain             = 97;
inst_fx_2_soundtrack       = 98;
inst_fx_3_crystal          = 99;
inst_fx_4_atmosphere       = 100;
inst_fx_5_brightness       = 101;
inst_fx_6_goblins          = 102;
inst_fx_7_echoes           = 103;
inst_fx_8_sci_fi           = 104;

{ Ethnic }

inst_sitar                 = 105;
inst_banjo                 = 106;
inst_shamisen              = 107;
inst_koto                  = 108;
inst_kalimba               = 109;
inst_bagpipe               = 110;
inst_fiddle                = 111;
inst_shanai                = 112;

{ Percussive }

inst_tinkle_bell           = 113;
inst_agogo                 = 114;
inst_steel_drums           = 115;
inst_woodblock             = 116;
inst_taiko_drum            = 117;
inst_melodic_tom           = 118;
inst_synth_drum            = 119;
inst_reverse_cymbal        = 120;

{ Sound effects }

inst_guitar_fret_noise     = 121;
inst_breath_noise          = 122;
inst_seashore              = 123;
inst_bird_tweet            = 124;
inst_telephone_ring        = 125;
inst_helicopter            = 126;
inst_applause              = 127;
inst_gunshot               = 128;

{ Drum sounds, activated as notes to drum instruments }

note_acoustic_bass_drum = 35;      
note_bass_drum_1        = 36;     
note_side_stick         = 37;      
note_acoustic_snare     = 38;      
note_hand_clap          = 39;     
note_electric_snare     = 40;      
note_low_floor_tom      = 41;     
note_closed_hi_hat      = 42;     
note_high_floor_tom     = 43;     
note_pedal_hi_hat       = 44;     
note_low_tom            = 45;     
note_open_hi_hat        = 46;     
note_low_mid_tom        = 47;     
note_hi_mid_tom         = 48;     
note_crash_cymbal_1     = 49;     
note_high_tom           = 50;     
note_ride_cymbal_1      = 51;     
note_chinese_cymbal     = 52;     
note_ride_bell          = 53;     
note_tambourine         = 54;     
note_splash_cymbal      = 55;     
note_cowbell            = 56;     
note_crash_cymbal_2     = 57;     
note_vibraslap          = 58;   
note_ride_cymbal_2      = 59;   
note_hi_bongo           = 60;   
note_low_bongo          = 61;   
note_mute_hi_conga      = 62;   
note_open_hi_conga      = 63;   
note_low_conga          = 64;   
note_high_timbale       = 65;   
note_low_timbale        = 66;   
note_high_agogo         = 67;   
note_low_agogo          = 68;   
note_cabasa             = 69;   
note_maracas            = 70;   
note_short_whistle      = 71;   
note_long_whistle       = 72;   
note_short_guiro        = 73;   
note_long_guiro         = 74;   
note_claves             = 75;   
note_hi_wood_block      = 76;   
note_low_wood_block     = 77;   
note_mute_cuica         = 78;   
note_open_cuica         = 79;   
note_mute_triangle      = 80;   
note_open_triangle      = 81;   

type

note       = 1..128; { note number for midi }
channel    = 1..16;  { channel number }
instrument = 1..128; { instrument number }

{ functions at this level }

procedure starttime; forward;
procedure stoptime; forward;
function curtime: integer; forward;
function synthout: integer; forward;
procedure opensynthout(p: integer); forward;
procedure closesynthout(p: integer); forward;
procedure noteon(p, t: integer; c: channel; n: note; v: integer); forward;
procedure noteoff(p, t: integer; c: channel; n: note; v: integer); forward;
procedure instchange(p, t: integer; c: channel; i: instrument); forward;
procedure attack(p, t: integer; c: channel; at: integer); forward;
procedure release(p, t: integer; c: channel; rt: integer); forward;
procedure legato(p, t: integer; c: channel; b: boolean); forward;
procedure portamento(p, t: integer; c: channel; b: boolean); forward;
procedure vibrato(p, t: integer; c: channel; v: integer); forward;
procedure volsynthchan(p, t: integer; c: channel; v: integer); forward;
procedure porttime(p, t: integer; c: channel; v: integer); forward;
procedure balance(p, t: integer; c: channel; b: integer); forward;
procedure pan(p, t: integer; c: channel; b: integer); forward;
procedure timbre(p, t: integer; c: channel; tb: integer); forward;
procedure brightness(p, t: integer; c: channel; b: integer); forward;
procedure reverb(p, t: integer; c: channel; r: integer); forward;
procedure tremulo(p, t: integer; c: channel; tr: integer); forward;
procedure chorus(p, t: integer; c: channel; cr: integer); forward;
procedure celeste(p, t: integer; c: channel; ce: integer); forward;
procedure phaser(p, t: integer; c: channel; ph: integer); forward;
procedure aftertouch(p, t: integer; c: channel; n: note; at: integer); forward;
procedure pressure(p, t: integer; c: channel; n: note; pr: integer); forward;
procedure pitch(p, t: integer; c: channel; pt: integer); forward;
procedure pitchrange(p, t: integer; c: channel; v: integer); forward;
procedure mono(p, t: integer; c: channel; ch: integer); forward;
procedure poly(p, t: integer; c: channel); forward;
procedure playsynth(p, t: integer; view sf: string); forward;
function waveout: integer; forward;
procedure openwaveout(p: integer); forward;
procedure closewaveout(p: integer); forward;
procedure playwave(p, t: integer; view sf: string); forward;
procedure volwave(p, t, v: integer); forward;

private

const

maxmid = 10; { maximum midi input/output devices }

{ midi status messages, high nybble }

mess_note_off = $80;
mess_note_on  = $90;
mess_afttch   = $a0;
mess_ctrl_chg = $b0;
mess_pgm_chg  = $c0;
mess_chn_pres = $d0;
mess_ptch_whl = $e0;

{ midi controller numbers }

ctlr_Bank_Select_coarse              = 0;   
ctlr_Modulation_Wheel_coarse         = 1;   
ctlr_Breath_controller_coarse        = 2;   
ctlr_Foot_Pedal_coarse               = 4;   
ctlr_Portamento_Time_coarse          = 5;   
ctlr_Data_Entry_coarse               = 6;   
ctlr_Volume_coarse                   = 7;   
ctlr_Balance_coarse                  = 8;   
ctlr_Pan_position_coarse             = 10;  
ctlr_Expression_coarse               = 11;  
ctlr_Effect_Control_1_coarse         = 12;  
ctlr_Effect_Control_2_coarse         = 13;  
ctlr_General_Purpose_Slider_1        = 16;  
ctlr_General_Purpose_Slider_2        = 17;  
ctlr_General_Purpose_Slider_3        = 18;  
ctlr_General_Purpose_Slider_4        = 19;  
ctlr_Bank_Select_fine                = 32;  
ctlr_Modulation_Wheel_fine           = 33;  
ctlr_Breath_controller_fine          = 34;  
ctlr_Foot_Pedal_fine                 = 36;  
ctlr_Portamento_Time_fine            = 37;  
ctlr_Data_Entry_fine                 = 38;  
ctlr_Volume_fine                     = 39;  
ctlr_Balance_fine                    = 40;  
ctlr_Pan_position_fine               = 42;  
ctlr_Expression_fine                 = 43;  
ctlr_Effect_Control_1_fine           = 44;  
ctlr_Effect_Control_2_fine           = 45;  
ctlr_Hold_Pedal                      = 64;  
ctlr_Portamento                      = 65;  
ctlr_Sustenuto_Pedal                 = 66;  
ctlr_Soft_Pedal                      = 67;  
ctlr_Legato_Pedal                    = 68;  
ctlr_Hold_2_Pedal                    = 69;  
ctlr_Sound_Variation                 = 70;  
ctlr_Sound_Timbre                    = 71;  
ctlr_Sound_Release_Time              = 72;  
ctlr_Sound_Attack_Time               = 73;  
ctlr_Sound_Brightness                = 74;  
ctlr_Sound_Control_6                 = 75;  
ctlr_Sound_Control_7                 = 76;  
ctlr_Sound_Control_8                 = 77;  
ctlr_Sound_Control_9                 = 78;  
ctlr_Sound_Control_10                = 79;  
ctlr_General_Purpose_Button_1        = 80;  
ctlr_General_Purpose_Button_2        = 81;  
ctlr_General_Purpose_Button_3        = 82;  
ctlr_General_Purpose_Button_4        = 83;  
ctlr_Effects_Level                   = 91;  
ctlr_Tremulo_Level                   = 92;  
ctlr_Chorus_Level                    = 93;  
ctlr_Celeste_Level                   = 94;  
ctlr_Phaser_Level                    = 95;  
ctlr_Data_Button_increment           = 96;  
ctlr_Data_Button_decrement           = 97;  
ctlr_Non_registered_Parameter_fine   = 98;  
ctlr_Non_registered_Parameter_coarse = 99;  
ctlr_Registered_Parameter_fine       = 100; 
ctlr_Registered_Parameter_coarse     = 101; 
ctlr_All_Sound_Off                   = 120; 
ctlr_All_Controllers_Off             = 121; 
ctlr_Local_Keyboard                  = 122; 
ctlr_All_Notes_Off                   = 123; 
ctlr_Omni_Mode_Off                   = 124; 
ctlr_Omni_Mode_On                    = 125; 
ctlr_Mono_Operation                  = 126; 
ctlr_Poly_Operation                  = 127; 

type

{ sequencer message types. each routine with a sequenced option has a
  sequencer message assocated with it }

seqtyp = (st_noteon, st_noteoff, st_instchange, st_attack, st_release,
          st_legato, st_portamento, st_vibrato, st_volsynthchan, st_porttime,
          st_balance, st_pan, st_timbre, st_brightness, st_reverb, st_tremulo,
          st_chorus, st_celeste, st_phaser, st_aftertouch, st_pressure,
          st_pitch, st_pitchrange, st_mono, st_poly, st_playsynth,
          st_playwave, st_volwave);

{ pointer to message }

seqptr = ^seqmsg;

{ sequencer message }

seqmsg = record

   port: integer; { port to which message applies }
   time: integer; { time to execute message }
   next: seqptr; { next message in list }
   case st: seqtyp of { sequencer message type }

      st_noteon, st_noteoff, st_aftertouch, st_pressure:
         (ntc: channel; ntn: note; ntv: integer);

      st_instchange: (icc: channel; ici: instrument);

      st_attack, st_release, st_vibrato, st_volsynthchan, st_porttime,
      st_balance, st_pan, st_timbre, st_brightness, st_reverb, st_tremulo,
      st_chorus, st_celeste, st_phaser, st_pitch, st_pitchrange,
      st_mono: (vsc: channel; vsv: integer);

      st_poly: (pc: channel);

      st_legato, st_portamento: (bsc: channel; bsb: boolean);

      st_playsynth, st_playwave: (ps: pstring);

      st_volwave: (wv: integer)

   { end }

end;

var 

midtab: array [1..maxmid] of sc_hmidiout; { midi output device table }
i:      1..maxmid; { index for midi tables }
seqlst: seqptr; { active sequencer entries }
seqfre: seqptr; { free sequencer entries }
seqrun: boolean; { sequencer running }
strtim: integer; { start time for sequencer, in raw windows time }
timhan: sc_mmresult; { handle for running timer }
seqlock: sc_critical_section; { sequencer task lock }

{******************************************************************************

Process sound library error

Outputs an error message using the special syslib function, then halts.

******************************************************************************}

procedure error(view s: string);

var i:     integer; { index for string }
    pream: packed array [1..8] of char; { preamble string }
    p:     pstring; { pointer to string }

begin

   pream := 'Sndlib: '; { set preamble }
   new(p, max(s)+8); { get string to hold }
   for i := 1 to 8 do p^[i] := pream[i]; { copy preamble }
   for i := 1 to max(s) do p^[i+8] := s[i]; { copy string }
   ss_wrterr(p^); { output string }
   dispose(p); { release storage }
   halt { end the run }

end;

{******************************************************************************

Get sequencer message entry

Gets a sequencer message entry, either from the used list, or new.

******************************************************************************}

procedure getseq(var p: seqptr);

begin

   if seqfre <> nil then begin { get a previous sequencer message }

      p := seqfre; { index top entry }
      seqfre := seqfre^.next { gap out }

   end else new(p); { else get a new entry, with full allocation }
   p^.next := nil { clear next }

end;

{******************************************************************************

Put sequencer message entry

Puts a sequencer message entry to the free list for reuse.

******************************************************************************}

procedure putseq(p: seqptr);

begin

   { dispose of any string }
   if (p^.st = st_playsynth) or (p^.st = st_playwave) then dispose(p^.ps);
   p^.next := seqfre; { link to top of list }
   seqfre := p { push onto list }

end;

{******************************************************************************

Insert sequencer message

Inserts a sequencer message into the list, in ascending time order.

******************************************************************************}

procedure insseq(p: seqptr);

var lp, l: seqptr;

begin

   sc_entercriticalsection(seqlock); { start exclusive access }
   { check sequencer list empty }
   if seqlst = nil then seqlst := p { place as first if so }
   { check insert to start }
   else if p^.time < seqlst^.time then begin

      p^.next := seqlst; { push onto list }
      seqlst := p

   end else begin { must search list }

      lp := seqlst; { index top of list }
      while (p^.time >= lp^.time) and (lp^.next <> nil) do begin

         { skip to key entry or end }
         l := lp; { set last }
         lp := lp^.next { index next }

      end;
      if p^.time < lp^.time then begin { insert here }

         l^.next := p; { link last to this }
         p^.next := lp { link this to next }

      end else begin { insert after }

         p^.next := lp^.next;
         lp^.next := p

      end

   end;
   sc_leavecriticalsection(seqlock) { end exclusive access }

end;

{******************************************************************************

Execute sequencer message

Executes the call referenced by the message. Each call is performed with
sequencer bypass, which means its ok to loop back on the call.

******************************************************************************}

procedure excseq(p: seqptr);

begin

   case p^.st of { sequencer message type }

      st_noteon:       noteon(p^.port, 0, p^.ntc, p^.ntn, p^.ntv);
      st_noteoff:      noteoff(p^.port, 0, p^.ntc, p^.ntn, p^.ntv);
      st_instchange:   instchange(p^.port, 0, p^.icc, p^.ici);
      st_attack:       attack(p^.port, 0, p^.vsc, p^.vsv);
      st_release:      release(p^.port, 0, p^.vsc, p^.vsv);
      st_legato:       legato(p^.port, 0, p^.bsc, p^.bsb);
      st_portamento:   portamento(p^.port, 0, p^.bsc, p^.bsb);
      st_vibrato:      vibrato(p^.port, 0, p^.vsc, p^.vsv);
      st_volsynthchan: volsynthchan(p^.port, 0, p^.vsc, p^.vsv);
      st_porttime:     porttime(p^.port, 0, p^.vsc, p^.vsv);
      st_balance:      balance(p^.port, 0, p^.vsc, p^.vsv);
      st_pan:          pan(p^.port, 0, p^.vsc, p^.vsv);
      st_timbre:       timbre(p^.port, 0, p^.vsc, p^.vsv);
      st_brightness:   brightness(p^.port, 0, p^.vsc, p^.vsv);
      st_reverb:       reverb(p^.port, 0, p^.vsc, p^.vsv);
      st_tremulo:      tremulo(p^.port, 0, p^.vsc, p^.vsv);
      st_chorus:       chorus(p^.port, 0, p^.vsc, p^.vsv);
      st_celeste:      celeste(p^.port, 0, p^.vsc, p^.vsv);
      st_phaser:       phaser(p^.port, 0, p^.vsc, p^.vsv);
      st_aftertouch:   aftertouch(p^.port, 0, p^.ntc, p^.ntn, p^.ntv);
      st_pressure:     pressure(p^.port, 0, p^.ntc, p^.ntn, p^.ntv);
      st_pitch:        pitch(p^.port, 0, p^.vsc, p^.vsv);
      st_pitchrange:   pitchrange(p^.port, 0, p^.vsc, p^.vsv);
      st_mono:         mono(p^.port, 0, p^.vsc, p^.vsv);
      st_poly:         poly(p^.port, 0, p^.pc);
      st_playsynth:    playsynth(p^.port, 0, p^.ps^);
      st_playwave:     playwave(p^.port, 0, p^.ps^);
      st_volwave:      volwave(p^.port, 0, p^.wv)

   end

end;

{******************************************************************************

Find elapsed millisecond time corrected

Finds the elapsed time on the Windows millisecond time, then corrects that
for 100us time.
Windows time is kept as a wrapping unsigned timer. Because and and subtract
are the same regardless of signed/unsigned, we do trial subtracts and examine
the results, instead of using comparisions, and -1 becomes the top value.

******************************************************************************}

function difftime(rt: integer): integer;

var ct, et: integer;

begin

   ct := sc_timegettime; { get current time }
   et := ct-rt; { find difference assuming rt > ct }
   if et < 0 then et := -1-rt+ct;
   difftime := et*10 { correct 1 milliseconds to 100us }

end;

{******************************************************************************

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

******************************************************************************}

procedure nextseq(id, msg, usr, dw1, dw2: integer);

var p:    seqptr;  { message entry pointer }
    elap: integer; { elapsed time }

begin

   if seqrun then begin { sequencer is still running }

      sc_entercriticalsection(seqlock); { start exclusive access }
      p := seqlst; { index top of list }
      while p <> nil do begin { process all past due messages }

         elap := difftime(strtim); { find elapsed time }
         if p^.time <= elap then begin { execute this message }

            seqlst := seqlst^.next; { gap out }
            excseq(p); { execute it }
            putseq(p); { release entry }
            p := seqlst { index top of list again }

         end else p := nil { stop search }

      end;
      if seqlst <> nil then { start another timer }
         timhan := sc_timesetevent((seqlst^.time-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);
      sc_leavecriticalsection(seqlock) { end exclusive access }

   end

end;

{*******************************************************************************

Find number of output midi ports

Returns the total number of output midi ports.

*******************************************************************************}

function synthout: integer;

begin

   synthout := sc_midioutgetnumdevs

end;

{*******************************************************************************

Open synthesiser output port

Opens a syth output port. The output ports have their own separate logical
numbers separate from input numbers, and by convention, port 1 will be the
main synthesizer for the computer, and port 2 will be an output port to any
midi chained devices outside the computer.

*******************************************************************************}

procedure opensynthout(p: integer);

var r: sc_mmresult;

begin

   r := sc_midioutopen_nnn(midtab[p], p-1) { open midi output device }

end;

{*******************************************************************************

Close midi sythesiser output port

Closes a previously opened midi output port.

*******************************************************************************}

procedure closesynthout(p: integer);

var r: sc_mmresult;

begin

   r := sc_midioutclose(midtab[p]); { close port }
   midtab[p] := -1 { set closed }

end;

{*******************************************************************************

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

*******************************************************************************}

procedure starttime;

begin

   strtim := sc_timegettime; { place current system time }
   seqrun := true { set sequencer running }

end;

{*******************************************************************************

Stop time

Stops midi sequencer function. Any timers and buffers in use by the sequencer
are cleared, and all pending events dropped.

*******************************************************************************}

procedure stoptime;

var r: sc_mmresult; { result value }
    p: seqptr; { message pointer }

begin

   strtim := 0; { clear start time }
   seqrun := false; { set sequencer not running }
   { if there is a pending sequencer timer, kill it }
   if timhan <> 0 then r := sc_timekillevent(timhan);
   { now clear all pending events }
   while seqlst <> nil do begin { clear }

     p := seqlst; { index top of list }
     seqlst := seqlst^.next; { gap out }
     putseq(p) { release entry }

   end

end;

{*******************************************************************************

Get current time

Finds the current time for the sequencer, which is the elapsed time since the
sequencer started.

*******************************************************************************}

function curtime: integer;

begin

   if not seqrun then error('Sequencer not running');
   curtime := difftime(strtim) { return difference time }

end;

{*******************************************************************************

Note on

Turns on a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to maxint.

*******************************************************************************}

procedure noteon(p, t: integer; c: channel; n: note; v: integer);

var msg:  integer;     { message holder }
    r:    sc_mmresult; { return value }
    elap: integer;     { current elapsed time }
    tact: boolean;     { timer active }
    sp:   seqptr;      { message pointer }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   if (n < 1) or (n > 128) then error('Bad note number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      { construct midi message }
      msg := (v div $01000000)*65536+(n-1)*256+mess_note_on+(c-1);
      r := sc_midioutshortmsg(midtab[p], msg)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_noteon; { set type }
      sp^.ntc := c; { set channel }
      sp^.ntn := n; { set note }
      sp^.ntv := v; { set velocity }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Note off

Turns off a single note by note number, 0..127, same as midi note mapping.
The time specified allows use of a sequencer. If the time is 0, then the note
is self timed. If the time is past, then the note is dropped. Otherwise, the
note is scheduled for the future by placing it in a sorted queue.
The velocity is set as 0 to maxint.

*******************************************************************************}

procedure noteoff(p, t: integer; c: channel; n: note; v: integer);

var msg:  integer;
    r:    sc_mmresult;
    elap: integer;     { current elapsed time }
    tact: boolean;     { timer active }
    sp:   seqptr;      { message pointer }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   if (n < 1) or (n > 128) then error('Bad note number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      { construct midi message }
      msg := (v div $01000000)*65536+(n-1)*256+mess_note_off+(c-1);
      r := sc_midioutshortmsg(midtab[p], msg)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_noteoff; { set type }
      sp^.ntc := c; { set channel }
      sp^.ntn := n; { set note }
      sp^.ntv := v; { set velocity }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Instrument change

Selects a new instrument for the given channel. Tbe new instrument is specified
by Midi GM encoding, 1 to 128. Takes a time for sequencing.

*******************************************************************************}

procedure instchange(p, t: integer; c: channel; i: instrument);

var msg:  integer;
    r:    sc_mmresult;
    elap: integer;     { current elapsed time }
    tact: boolean;     { timer active }
    sp:   seqptr;      { message pointer }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   if (i < 1) or (i > 128) then error('Bad instrument number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      msg := (i-1)*256+mess_pgm_chg+(c-1); { construct midi message }
      r := sc_midioutshortmsg(midtab[p], msg);

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_instchange; { set type }
      sp^.icc := c; { set channel }
      sp^.ici := i; { set instrument }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Controller change

Processes a controller value set, from 0 to 127.

*******************************************************************************}

procedure ctlchg(p, t: integer; c: channel; cn: integer; v: integer);

var msg: integer;
    r:   sc_mmresult;

begin

   { construct midi message }
   msg := v*65536+cn*256+mess_ctrl_chg+(c-1);
   r := sc_midioutshortmsg(midtab[p], msg);

end;

{*******************************************************************************

Set attack time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

*******************************************************************************}

procedure attack(p, t: integer; c: channel; at: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_sound_attack_time, at div $01000000)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_attack; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := at; { set attack }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set release time

Sets the time of attack on a note, ie., how long it takes for the note to go
full on.

*******************************************************************************}

procedure release(p, t: integer; c: channel; rt: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_sound_release_time, rt div $01000000)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_release; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := rt; { set release }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Legato pedal on/off

Sets the legato mode on/off.

*******************************************************************************}

procedure legato(p, t: integer; c: channel; b: boolean);

var msg: integer;
    r:   sc_mmresult;
    elap: integer;    { current elapsed time }
    tact: boolean;    { timer active }
    sp:  seqptr;      { message pointer }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_legato_pedal, ord(b)*127)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_legato; { set type }
      sp^.bsc := c; { set channel }
      sp^.bsb := b; { set on/off }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Portamento pedal on/off

Sets the portamento mode on/off.

*******************************************************************************}

procedure portamento(p, t: integer; c: channel; b: boolean);

var msg:  integer;
    r:    sc_mmresult;
    elap: integer;     { current elapsed time }
    tact: boolean;     { timer active }
    sp:   seqptr;      { message pointer }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_portamento, ord(b)*127)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_portamento; { set type }
      sp^.bsc := c; { set channel }
      sp^.bsb := b; { set on/off }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set volume

Sets synthesizer volume, 0 to maxint.

*******************************************************************************}

procedure volsynthchan(p, t: integer; c: channel; v: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_volume_coarse, v div $01000000); { set high }
      ctlchg(p, t, c, ctlr_volume_fine, v div $00020000 and $7f) { set low }

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_volsynthchan; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := v; { set volume }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set left right channel balance

Sets sets the left right channel balance. -maxint is all left, 0 is center,
maxint is all right.

*******************************************************************************}

procedure balance(p, t: integer; c: channel; b: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      b := b div $00040000+$2000; { reduce to 14 bits, positive only }
      ctlchg(p, t, c, ctlr_balance_coarse, b div $80); { set high }
      ctlchg(p, t, c, ctlr_balance_fine, b and $7f) { set low }

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_balance; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := b; { set balance }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set portamento time

Sets portamento time, 0 to maxint.

*******************************************************************************}

procedure porttime(p, t: integer; c: channel; v: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_portamento_time_coarse, v div $01000000); { set high }
      { set low }
      ctlchg(p, t, c, ctlr_portamento_time_fine, v div $00020000 and $7f)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_porttime; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := v; { set time }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set vibrato

Sets modulaton value, 0 to maxint.

*******************************************************************************}

procedure vibrato(p, t: integer; c: channel; v: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_modulation_wheel_coarse, v div $01000000); { set high }
      { set low }
      ctlchg(p, t, c, ctlr_modulation_wheel_fine, v div $00020000 and $7f)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_vibrato; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := v; { set vibrato }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set left/right pan position

Sets sets the left/right pan position. -maxint is hard left, 0 is center,
maxint is hard right.

*******************************************************************************}

procedure pan(p, t: integer; c: channel; b: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      b := b div $00040000+$2000; { reduce to 14 bits, positive only }
      ctlchg(p, t, c, ctlr_pan_position_coarse, b div $80); { set high }
      ctlchg(p, t, c, ctlr_pan_position_fine, b and $7f) { set low }

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_pan; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := b; { set pan }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set sound timbre

Sets the sound timbre, 0 to maxint.

*******************************************************************************}

procedure timbre(p, t: integer; c: channel; tb: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_sound_timbre, tb div $01000000)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_timbre; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := tb; { set timbre }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set sound brightness

Sets the sound brightness, 0 to maxint.

*******************************************************************************}

procedure brightness(p, t: integer; c: channel; b: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_sound_brightness, b div $01000000)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_brightness; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := b; { set brightness }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set sound reverb

Sets the sound reverb, 0 to maxint.

*******************************************************************************}

procedure reverb(p, t: integer; c: channel; r: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_effects_level, r div $01000000)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_reverb; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := r; { set reverb }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set sound tremulo

Sets the sound tremulo, 0 to maxint.

*******************************************************************************}

procedure tremulo(p, t: integer; c: channel; tr: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_tremulo_level, tr div $01000000)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_tremulo; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := tr; { set tremulo }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set sound chorus

Sets the sound chorus, 0 to maxint.

*******************************************************************************}

procedure chorus(p, t: integer; c: channel; cr: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_chorus_level, cr div $01000000)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_chorus; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := cr; { set chorus }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set sound celeste

Sets the sound celeste, 0 to maxint.

*******************************************************************************}

procedure celeste(p, t: integer; c: channel; ce: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_celeste_level, ce div $01000000)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_celeste; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := ce; { set celeste }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set sound phaser

Sets the sound phaser, 0 to maxint.

*******************************************************************************}

procedure phaser(p, t: integer; c: channel; ph: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_phaser_level, ph div $01000000)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_phaser; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := ph; { set phaser }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set pitch range

Sets the range of pitch that can be reached by the pitch adjustment. The range
is from 0 to maxint, and represents from from 0 to 127 semitones. This means
that a setting of maxint means that every note in the midi scale can be reached,
with an accuracy of 128 of the space from C to C#. At this setting, any note
could be reached with a slide, for example.

*******************************************************************************}

procedure pitchrange(p, t: integer; c: channel; v: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      { set up data entry }
      ctlchg(p, t, c, ctlr_registered_parameter_coarse, 0); { set high }
      ctlchg(p, t, c, ctlr_registered_parameter_fine, 0); { set low }
      ctlchg(p, t, c, ctlr_data_entry_coarse, v div $01000000); { set high }
      ctlchg(p, t, c, ctlr_data_entry_fine, v div $00020000 and $7f) { set low }

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_pitchrange; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := v; { set pitchrange }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set monophonic mode

If ommni is off, this sets how many channels to respond to. If omni is on,
then only once note at a time will be played. The select is from 0 to 16,
with 0 being "allways select single note mode".

*******************************************************************************}

procedure mono(p, t: integer; c: channel; ch: integer);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   if (ch < 0) or (c > 16) then error('Bad mono mode number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_mono_operation, ch)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_mono; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := ch; { set mono mode }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set polyphonic mode

Reenables polyphonic mode after a monophonic operation.

*******************************************************************************}

procedure poly(p, t: integer; c: channel);

var sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }

begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      ctlchg(p, t, c, ctlr_poly_operation, 0) { value dosen't matter }

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_poly; { set type }
      sp^.pc := c; { set channel }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Aftertouch

Controls aftertouch, 0 to maxint, on a note.

*******************************************************************************}

procedure aftertouch(p, t: integer; c: channel; n: note; at: integer);

var msg: integer;
    r:   sc_mmresult;
    sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }


begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   if (n < 1) or (n > 128) then error('Bad note number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      { construct midi message }
      msg := (at div $01000000)*65536+(n-1)*256+mess_afttch+(c-1);
      r := sc_midioutshortmsg(midtab[p], msg);

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_aftertouch; { set type }
      sp^.ntc := c; { set channel }
      sp^.ntn := n; { set note }
      sp^.ntv := at; { set aftertouch }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Channel pressure

Controls channel pressure, 0 to maxint, on a note.

*******************************************************************************}

procedure pressure(p, t: integer; c: channel; n: note; pr: integer);

var msg: integer;
    r:   sc_mmresult;
    sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }


begin


   if (c < 1) or (c > 16) then error('Bad channel number');
   if (n < 1) or (n > 128) then error('Bad note number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      { construct midi message }
      msg := (pr div $01000000)*65536+(n-1)*256+mess_chn_pres+(c-1);
      r := sc_midioutshortmsg(midtab[p], msg)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_pressure; { set type }
      sp^.ntc := c; { set channel }
      sp^.ntn := n; { set note }
      sp^.ntv := pr; { set pressure }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Set pitch wheel

Sets the pitch wheel value, from 0 to maxint. This is the amount off the note
in the channel. The GM standard is to adjust for a whole step up and down, which
is 4 half steps total. A "half step" is the difference between, say, C and C#.

*******************************************************************************}

procedure pitch(p, t: integer; c: channel; pt: integer);

var msg: integer;
    r:   sc_mmresult;
    sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }


begin

   if (c < 1) or (c > 16) then error('Bad channel number');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      pt := pt div $00040000+$2000; { reduce to 14 bits, positive only }
      { construct midi message }
      msg := (pt div $80)*65536+(pt and $7f)*256+mess_ptch_whl+(c-1);
      r := sc_midioutshortmsg(midtab[p], msg)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_pitch; { set type }
      sp^.vsc := c; { set channel }
      sp^.vsv := pt; { set pitch }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

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

*******************************************************************************}

procedure playsynth(p, t: integer; view sf: string);

var sfb, b: packed array [1..100] of char;
    x: integer;
    r: sc_mcierror; { return }
    fp, fn, fe: packed array [1..100] of char; { filename components }
    sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }


procedure plcstr(view s: string; sp: boolean);

var i: integer;

begin

   { traverse string }
   for i := 1 to max(s) do
      { check not space, or spaces allowed }
      if sp or (s[i] <> ' ') then begin { transfer characters }

      b[x] := s[i]; { move character }
      x := x+1 { next }

   end

end;

procedure send;

var p: pstring; { buffer for final string }
    i: integer;

begin

   new(p, x-1); { allocate string }
   for i := 1 to x-1 do p^[i] := b[i];
   r := sc_mcisendstring_nnn(p^);
   dispose(p) { release string }

end;

begin

   if p <> 1 then error('Must execute play on default output channel');
   if midtab[p] < 0 then error('Synth output channel not open');
   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      closesynthout(1); { close default output }
      brknam(sf, fp, fn, fe); { break filename components }
      if fe[1] = ' ' then copy(fe, 'mid'); { if no extention, place one }
      maknam(sfb, fp, fn, fe);
      x := 1; { set 1st string position }
      plcstr('close midi', true);
      send;
      x := 1; { set 1st string position }
      plcstr('open ', true);
      plcstr(sfb, false);
      plcstr(' alias midi', true);
      send;
      x := 1; { set 1st string position }
      plcstr('play midi', true);
      send;
      opensynthout(1) { reopen default midi }

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_playsynth; { set type }
      copy(sp^.ps, sf); { make copy of file string to play }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Find number of wave devices.

Returns the number of wave output devices available. This is hardwared to 1 for
the one windows waveform device.

*******************************************************************************}

function waveout: integer;

begin

   waveout := 1

end;

{*******************************************************************************

Open wave output device

Opens a wave output device by number. By convention, wave out 1 is the default
output device. This is presently a no-op for windows.

*******************************************************************************}

procedure openwaveout(p: integer);

begin

end;

{*******************************************************************************

CLose wave output device

Closes a wave output device by number. This is presently a no-op for windows.

*******************************************************************************}

procedure closewaveout(p: integer);

begin

end;

{*******************************************************************************

Play waveform file

Plays the waveform file to the indicated wave device. A sequencer time can also
be indicated, in which case the play will be stored as a sequencer event. This
allows wave files to be sequenced against other wave files and midi files.
The file is specified by file name, and the file type is system dependent.

*******************************************************************************}

procedure playwave(p, t: integer; view sf: string);

var b: boolean; { result }
    sp:   seqptr;  { message pointer }
    elap: integer; { current elapsed time }
    tact: boolean; { timer active }


begin

   elap := difftime(strtim); { find elapsed time }
   { execute immediate if 0 or sequencer running and time past }
   if (t = 0) or ((t <= elap) and seqrun) then begin

      b :=sc_playsound(sf, 0, sc_snd_filename or sc_snd_nodefault or sc_snd_async)

   end else begin { sequence }

      { check sequencer running }
      if not seqrun then error('Sequencer not running');
      tact := seqlst <> nil; { flag if pending timers }
      getseq(sp); { get a sequencer message }
      sp^.port := p; { set port }
      sp^.time := t; { set time }
      sp^.st := st_playwave; { set type }
      copy(sp^.ps, sf); { make copy of file string to play }
      insseq(sp); { insert to sequencer list }
      if not tact then { activate timer }
         timhan := sc_timesetevent((t-elap) div 10, 0, nextseq, 0,
                                   sc_time_callback_function or
                                   sc_time_kill_synchronous or
                                   sc_time_oneshot);

   end

end;

{*******************************************************************************

Adjust waveform volume

Adjusts the volume on waveform playback. The volume value is from 0 to maxint.

*******************************************************************************}

procedure volwave(p, t, v: integer);

begin

end;

begin

   seqlst := nil; { clear active sequencer list }
   seqfre := nil; { clear free sequencer messages }
   seqrun := false; { set sequencer not running }
   strtim := 0; { clear start time }
   timhan := 0; { set no timer active }
   for i := 1 to maxmid do midtab[i] := -1; { set no midi output ports open }
   sc_initializecriticalsection(seqlock); { initialize the sequencer lock }

end;

begin
end.
