/*******************************************************************************

 alsaparams

 Gets The basic parameter ranges from a given alsa device, the number of
 channels, the rate, and the format.

 ******************************************************************************/

#include <stdio.h>

#include <alsa/asoundlib.h>

int bigend(void)

{

    union {

        int i;
        unsigned char b;

    } test;

    test.i = 1;

    return (!test.b);

}

void dumpparms(char *devs, snd_pcm_stream_t stream)

{

    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *pars;
    snd_pcm_format_mask_t *fmask;
    unsigned chanmin, chanmax;
    unsigned ratemin, ratemax;
    unsigned bitmin, bitmax;
    int bits;
    int sgn;
    int big;
    int flt;
    int fmt;
    int supp;
    int mbits;
    int msgn;
    int mbig;
    int mflt;
    int msupp;


    int r;

    r = snd_pcm_open(&pcm, devs, stream, SND_PCM_NONBLOCK);
    printf("After snd_pcm_open: r: %d devs: %s\n", r, devs);
    if (r) printf("Unable to open with this stream mode\n");

    else {

        snd_pcm_hw_params_alloca(&pars);
        r = snd_pcm_hw_params_any(pcm, pars);
        if (r < 0) {

            fprintf(stderr, "Error reading configuration space\n");
            exit(1);

        }
        snd_pcm_hw_params_get_channels_min(pars, &chanmin);
        snd_pcm_hw_params_get_channels_max(pars, &chanmax);
        snd_pcm_hw_params_get_rate_min(pars, &ratemin, NULL);
        snd_pcm_hw_params_get_rate_max(pars, &ratemax, NULL);
        if (chanmin == chanmax) printf("Channels: %u\n", chanmin);
        else printf("Channels from %u to %u\n", chanmin, chanmax);
        printf("Rates from %d to %u\n", ratemin, ratemax);
        printf("Capabilities:\n");
        snd_pcm_format_mask_alloca(&fmask);
        snd_pcm_hw_params_get_format_mask(pars, fmask);
        mbits = 0; msgn = 0; mbig = 0; mflt = 0; msupp = 0;
        for(fmt = 0; fmt < SND_PCM_FORMAT_LAST; fmt++) {

            bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
            if (snd_pcm_format_mask_test(fmask, (snd_pcm_format_t)fmt)) {

                switch (fmt) {

                    /* print code and get basic capabilities */
                    case SND_PCM_FORMAT_S8:
                        printf("S8                ");
                        bits = 8; sgn = 1; big = 0; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_U8:
                        printf("U8                ");
                        bits = 8; sgn = 0; big = 0; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_S16_LE:
                        printf("S16_LE            ");
                        bits = 16; sgn = 1; big = 0; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_S16_BE:
                        printf("S16_BE            ");
                        bits = 16; sgn = 1; big = 1; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_U16_LE:
                        printf("U16_LE            ");
                        bits = 16; sgn = 0; big = 0; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_U16_BE:
                        printf("U16_BE            ");
                        bits = 16; sgn = 0; big = 1; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_S24_LE:
                        printf("S24_LE            ");
                        bits = 24; sgn = 1; big = 0; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_S24_BE:
                        printf("S24_BE            ");
                        bits = 24; sgn = 1; big = 1; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_U24_LE:
                        printf("U24_LE            ");
                        bits = 24; sgn = 0; big = 0; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_U24_BE:
                        printf("U24_BE            ");
                        bits = 24; sgn = 0; big = 1; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_S32_LE:
                        printf("S32_LE            ");
                        bits = 32; sgn = 1; big = 0; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_S32_BE:
                        printf("S32_BE            ");
                        bits = 32; sgn = 1; big = 1; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_U32_LE:
                        printf("U32_LE            ");
                        bits = 32; sgn = 0; big = 0; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_U32_BE:
                        printf("U32_BE            ");
                        bits = 32; sgn = 0; big = 1; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_FLOAT_LE:
                        printf("FLOAT_LE          ");
                        bits = 32; sgn = 1; big = 0; flt = 1; supp = 1;
                        break;
                    case SND_PCM_FORMAT_FLOAT_BE:
                        printf("FLOAT_BE           ");
                        bits = 32; sgn = 1; big = 1; flt = 1; supp = 1;
                        break;
                    case SND_PCM_FORMAT_FLOAT64_LE:
                        printf("FLOAT64_LE         ");
                        bits = 64; sgn = 1; big = 0; flt = 1; supp = 1;
                        break;
                    case SND_PCM_FORMAT_FLOAT64_BE:
                        printf("FLOAT64_BE         ");
                        bits = 64; sgn = 1; big = 1; flt = 1; supp = 1;
                        break;
                    case SND_PCM_FORMAT_IEC958_SUBFRAME_LE:
                        printf("IEC958_SUBFRAME_LE ");
                        bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_IEC958_SUBFRAME_BE:
                        printf("IEC958_SUBFRAME_BE ");
                        bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_MU_LAW:
                        printf("MU_LAW             ");
                        bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_A_LAW:
                        printf("A_LAW              ");
                        bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_IMA_ADPCM:
                        printf("IMA_ADPCM          ");
                        bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_MPEG:
                        printf("MPEG               ");
                        bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_GSM:
                        printf("GSM                ");
                        bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_SPECIAL:
                        printf("SPECIAL            ");
                        bits = 0; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_S24_3LE:
                        printf("S24_3LE            ");
                        bits = 24; sgn = 1; big = 1; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_S24_3BE:
                        printf("S24_3BE             ");
                        bits = 24; sgn = 1; big = 1; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_U24_3LE:
                        printf("U24_3LE             ");
                        bits = 24; sgn = 0; big = 0; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_U24_3BE:
                        printf("U24_3BE             ");
                        bits = 24; sgn = 0; big = 1; flt = 0; supp = 1;
                        break;
                    case SND_PCM_FORMAT_S20_3LE:
                        printf("S20_3LE             ");
                        bits = 20; sgn = 1; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_S20_3BE:
                        printf("S20_3BE             ");
                        bits = 20; sgn = 1; big = 1; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_U20_3LE:
                        printf("U20_3LE             ");
                        bits = 20; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_U20_3BE:
                        printf("U20_3BE             ");
                        bits = 20; sgn = 0; big = 1; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_S18_3LE:
                        printf("S18_3LE             ");
                        bits = 18; sgn = 1; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_S18_3BE:
                        printf("S18_3BE             ");
                        bits = 18; sgn = 1; big = 1; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_U18_3LE:
                        printf("U18_3LE             ");
                        bits = 18; sgn = 0; big = 0; flt = 0; supp = 0;
                        break;
                    case SND_PCM_FORMAT_U18_3BE:
                        printf("U18_3BE             ");
                        bits = 18; sgn = 0; big = 1; flt = 0; supp = 0;
                        break;

                }
                printf("Bits: %2d Sgn: %d Big: %d Flt: %d Supported: %d\n", bits, sgn, big, flt, supp);
                /* find max format */
                if (supp) {

                    /* is a PA supported format */
                    if (bits > mbits || flt > mflt) {

                        /* prefer bigger bit size or float */
                        if (sgn > msgn || big == bigend()) {

                            /* prefer signed or big endian matches host */
                            mbits = bits; msgn = sgn; mbig = big; mflt = flt; msupp = supp;

                        }

                    }

                }

            }

        }
        if (msupp)
            printf("\nPreferred format: Bits: %d Sgn: %d Big: %d Flt: %d\n",
                   mbits, msgn, mbig, mflt);
        snd_pcm_close(pcm);

    }

}

int main(int argc,char** argv)

{

    char devname[100];

    if (argc != 2) {

        fprintf(stderr, "Usage: alsaparm <device name>\n");
        exit(1);

    }
    strcpy(devname, argv[1]);

    printf("\nCapture mode:\n\n");
    dumpparms(argv[1], SND_PCM_STREAM_PLAYBACK);
    printf("\nPlayback mode:\n\n");
    dumpparms(argv[1], SND_PCM_STREAM_CAPTURE);

    return 0;

}
