/* miniFMsynth 1.1 by Matthias Nagorni    */
/* This program uses callback-based audio */
/* playback as proposed by Paul Davis on  */
/* the linux-audio-dev mailinglist.       */

#include <stdio.h>   
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <math.h>

#define POLY 10
#define GAIN 5000.0
#define BUFSIZE 512

snd_seq_t *seq_handle;
snd_pcm_t *playback_handle;
short *buf;
double phi[POLY], phi_mod[POLY], pitch, modulation, velocity[POLY], attack, decay, sustain, release, env_time[POLY], env_level[POLY];
int harmonic, subharmonic, transpose, note[POLY], gate[POLY], note_active[POLY], rate;

snd_seq_t *open_seq() {

    snd_seq_t *seq_handle;

    rate = 44100;    
    if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "Error opening ALSA sequencer.\n");
        exit(1);
    }
    snd_seq_set_client_name(seq_handle, "miniFMsynth");
    if (snd_seq_create_simple_port(seq_handle, "miniFMsynth",
        SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION) < 0) {
        fprintf(stderr, "Error creating sequencer port.\n");
        exit(1);
    }
    return(seq_handle);
}

snd_pcm_t *open_pcm(char *pcm_name) {

    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
            
    if (snd_pcm_open (&playback_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf (stderr, "cannot open audio device %s\n", pcm_name);
        exit (1);
    }
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(playback_handle, hw_params);
    snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0);
    snd_pcm_hw_params_set_channels(playback_handle, hw_params, 2);
    snd_pcm_hw_params_set_periods(playback_handle, hw_params, 2, 0);
    snd_pcm_hw_params_set_period_size(playback_handle, hw_params, BUFSIZE, 0);
    snd_pcm_hw_params(playback_handle, hw_params);
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(playback_handle, sw_params);
    snd_pcm_sw_params_set_avail_min(playback_handle, sw_params, BUFSIZE);
    snd_pcm_sw_params(playback_handle, sw_params);
    return(playback_handle);
}

double envelope(int *note_active, int gate, double *env_level, double t, double attack, double decay, double sustain, double release) {

    if (gate)  {
        if (t > attack + decay) return(*env_level = sustain);
        if (t > attack) return(*env_level = 1.0 - (1.0 - sustain) * (t - attack) / decay);
        return(*env_level = t / attack);
    } else {
        if (t > release) {
            if (note_active) *note_active = 0;
            return(*env_level = 0);
        }
        return(*env_level * (1.0 - t / release));
    }
}

int midi_callback() {

    snd_seq_event_t *ev;
    int l1;
  
    do {
        snd_seq_event_input(seq_handle, &ev);
        switch (ev->type) {
            case SND_SEQ_EVENT_PITCHBEND:
                pitch = (double)ev->data.control.value / 8192.0;
                break;
            case SND_SEQ_EVENT_CONTROLLER:
                if (ev->data.control.param == 1) {
                    modulation = (double)ev->data.control.value / 10.0;
                } 
                break;
            case SND_SEQ_EVENT_NOTEON:
                for (l1 = 0; l1 < POLY; l1++) {
                    if (!note_active[l1]) {
                        note[l1] = ev->data.note.note;
                        velocity[l1] = ev->data.note.velocity / 127.0;
                        env_time[l1] = 0;
                        gate[l1] = 1;
                        note_active[l1] = 1;
                        break;
                    }
                }
                break;        
            case SND_SEQ_EVENT_NOTEOFF:
                for (l1 = 0; l1 < POLY; l1++) {
                    if (gate[l1] && note_active[l1] && (note[l1] == ev->data.note.note)) {
                        env_time[l1] = 0;
                        gate[l1] = 0;
                    }
                }
                break;        
        }
        snd_seq_free_event(ev);
    } while (snd_seq_event_input_pending(seq_handle, 0) > 0);
    return (0);
}

int playback_callback (snd_pcm_sframes_t nframes) {

    int l1, l2;
    double dphi, dphi_mod, f1, f2, f3, freq_note, sound;
      
    memset(buf, 0, nframes * 4);
    for (l2 = 0; l2 < POLY; l2++) {
        if (note_active[l2]) {
            f1 = 8.176 * exp((double)(transpose+note[l2]-2)*log(2.0)/12.0);
            f2 = 8.176 * exp((double)(transpose+note[l2])*log(2.0)/12.0);
            f3 = 8.176 * exp((double)(transpose+note[l2]+2)*log(2.0)/12.0);
            freq_note = (pitch > 0) ? f2 + (f3-f2)*pitch : f2 + (f2-f1)*pitch;
            dphi = M_PI * freq_note / 22050.0;                                    
            dphi_mod = dphi * (double)harmonic / (double)subharmonic;
            for (l1 = 0; l1 < nframes; l1++) {
                phi[l2] += dphi;
                phi_mod[l2] += dphi_mod;
                if (phi[l2] > 2.0 * M_PI) phi[l2] -= 2.0 * M_PI;
                if (phi_mod[l2] > 2.0 * M_PI) phi_mod[l2] -= 2.0 * M_PI;
                sound = GAIN * envelope(&note_active[l2], gate[l2], &env_level[l2], env_time[l2], attack, decay, sustain, release)
                             * velocity[l2] * sin(phi[l2] + modulation * sin(phi_mod[l2]));
                env_time[l2] += 1.0 / 44100.0;
                buf[2 * l1] += sound;
                buf[2 * l1 + 1] += sound;
            }
        }    
    }
    return snd_pcm_writei (playback_handle, buf, nframes); 
}
      
int main (int argc, char *argv[]) {

    int nfds, seq_nfds, l1;
    struct pollfd *pfds;
    
    if (argc < 10) {
        fprintf(stderr, "miniFMsynth <device> <FM> <harmonic> <subharmonic> <transpose> <a> <d> <s> <r>\n"); 
        exit(1);
    }
    modulation = atof(argv[2]);
    harmonic = atoi(argv[3]);
    subharmonic = atoi(argv[4]);
    transpose = atoi(argv[5]);
    attack = atof(argv[6]);
    decay = atof(argv[7]);
    sustain = atof(argv[8]);
    release = atof(argv[9]);
    pitch = 0;
    buf = (short *) malloc (2 * sizeof (short) * BUFSIZE);
    playback_handle = open_pcm(argv[1]);
    seq_handle = open_seq();
    seq_nfds = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
    nfds = snd_pcm_poll_descriptors_count (playback_handle);
    pfds = (struct pollfd *)alloca(sizeof(struct pollfd) * (seq_nfds + nfds));
    snd_seq_poll_descriptors(seq_handle, pfds, seq_nfds, POLLIN);
    snd_pcm_poll_descriptors (playback_handle, pfds+seq_nfds, nfds);
    for (l1 = 0; l1 < POLY; note_active[l1++] = 0);
    while (1) {
        if (poll (pfds, seq_nfds + nfds, 1000) > 0) {
            for (l1 = 0; l1 < seq_nfds; l1++) {
               if (pfds[l1].revents > 0) midi_callback();
            }
            for (l1 = seq_nfds; l1 < seq_nfds + nfds; l1++) {    
                if (pfds[l1].revents > 0) { 
                    if (playback_callback(BUFSIZE) < BUFSIZE) {
                        fprintf (stderr, "xrun !\n");
                        snd_pcm_prepare(playback_handle);
                    }
                }
            }        
        }
    }
    snd_pcm_close (playback_handle);
    snd_seq_close (seq_handle);
    free(buf);
    return (0);
}
     
