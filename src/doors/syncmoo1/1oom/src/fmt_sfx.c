#include "config.h"

#include <stdio.h>
#include <string.h>
#ifdef HAVE_SAMPLERATE
#include <samplerate.h>
#endif

#include "fmt_sfx.h"
#include "bits.h"
#include "fmt_id.h"
#include "lib.h"
#include "log.h"
#include "options.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define LOW_PASS_FILTER

#define DEBUGLEVEL_FMTSFX   4

struct sfx_conv_s {
    int16_t *data;
    uint32_t num;   /* number of stereo samples */
    uint32_t samplerate;
};

/* -------------------------------------------------------------------------- */

#ifdef HAVE_SAMPLERATE
static struct sfx_conv_s fmt_sfx_resample_libsamplerate(struct sfx_conv_s *s_in, int audiorate)
{
    struct sfx_conv_s s_out = { NULL, 0, audiorate };

    /* code from chocolate-doom_2.3.0 */

    SRC_DATA src_data;
    float *data_in;
    uint32_t i, clipped = 0;
    float libsamplerate_scale = opt_libsamplerate_scale / 100.0;

    src_data.input_frames = s_in->num;
    data_in = lib_malloc(s_in->num * 2 * sizeof(float));
    src_data.data_in = data_in;
    src_data.src_ratio = (double)audiorate / s_in->samplerate;

    /* We include some extra space here in case of rounding-up. */
    src_data.output_frames = src_data.src_ratio * s_in->num + (audiorate / 4);
    src_data.data_out = lib_malloc(src_data.output_frames * 2 * sizeof(float));

    /* Convert input data to floats */
    for (i = 0; i < (s_in->num * 2); ++i) {
        data_in[i] = s_in->data[i] / 32768.0;
    }

    /* Do the sound conversion */
    {
        int retn = src_simple(&src_data, opt_libsamplerate_mode, 2);
        if (retn) {
            log_fatal_and_die("%s: libsamplerate returned %i\n", __func__, retn);
        }
    }

    /* Allocate a chunk in which to expand the sound */
    s_out.num = src_data.output_frames_gen;
    s_out.data = lib_malloc(s_out.num * 2/*16b*/ * 2/*stereo*/);

    /* Convert the result back into 16-bit integers. */

    for (i = 0; i < src_data.output_frames_gen * 2; ++i) {
        /*
           libsamplerate does not limit itself to the -1.0 .. 1.0 range on
           output, so a multiplier less than INT16_MAX (32767) is required
           to avoid overflows or clipping.  However, the smaller the
           multiplier, the quieter the sound effects get, and the more you
           have to turn down the music to keep it in balance.
        */
        float cvtval_f = src_data.data_out[i] * libsamplerate_scale * INT16_MAX;
        int32_t cvtval_i = cvtval_f + (cvtval_f < 0 ? -0.5 : 0.5);

        /* Asymmetrical sound worries me, so we won't use -32768. */
        if (cvtval_i < -INT16_MAX) {
            cvtval_i = -INT16_MAX;
            ++clipped;
        } else if (cvtval_i > INT16_MAX) {
            cvtval_i = INT16_MAX;
            ++clipped;
        }
        s_out.data[i] = cvtval_i;
    }
    if (clipped) {
        log_warning("libsamplerate clipped %i samples with scale %i\n", clipped, opt_libsamplerate_scale);
    }
    lib_free(data_in);
    lib_free(src_data.data_out);
    return s_out;
}
#endif /* HAVE_SAMPLERATE */

static struct sfx_conv_s fmt_sfx_resample_simple(struct sfx_conv_s *s_in, int audiorate)
{
    struct sfx_conv_s s_out = { NULL, 0, audiorate };

    /* code from chocolate-doom_2.3.0 */

    uint32_t expanded_length;
    int expand_ratio;
    int i;

    /* Calculate the length of the expanded version of the sample. */
    expanded_length = (uint32_t) ((((uint64_t) s_in->num) * audiorate) / s_in->samplerate);
    s_out.num = expanded_length;
    /* Allocate a chunk in which to expand the sound */
    s_out.data = lib_malloc(expanded_length * 2/*16b*/ * 2/*stereo*/);

    expand_ratio = (s_in->num << 8) / s_out.num;

    for (i = 0; i < s_out.num; ++i) {
        int src;
        src = (i * expand_ratio) >> 8;
        s_out.data[i * 2 + 0] = s_in->data[src * 2 + 0];
        s_out.data[i * 2 + 1] = s_in->data[src * 2 + 1];
    }

#ifdef LOW_PASS_FILTER
    /* Perform a low-pass filter on the upscaled sound to filter
       out high-frequency noise from the conversion process. */
    if (s_in->samplerate < audiorate) {
        float rc, dt, alpha;
        /*
           Low-pass filter for cutoff frequency f:

           For sampling rate r, dt = 1 / r
           rc = 1 / 2*pi*f
           alpha = dt / (rc + dt)

           Filter to the half sample rate of the original sound effect
           (maximum frequency, by nyquist)
        */
        dt = 1.0f / audiorate;
        rc = 1.0f / (3.14f * s_in->samplerate);
        alpha = dt / (rc + dt);

        /* Both channels are processed in parallel, hence [i - 2]: */
        for (i = 2; i < s_out.num * 2; ++i) {
            s_out.data[i] = (int16_t) (alpha * s_out.data[i] + (1 - alpha) * s_out.data[i - 2]);
        }
    }
#endif /* #ifdef LOW_PASS_FILTER */

    return s_out;
}

static struct sfx_conv_s fmt_sfx_convert_voc(const uint8_t *data_in, uint32_t len_in)
{
    struct sfx_conv_s res = { NULL, 0, 0 };
    uint32_t cursize = 0;
    int16_t *data = NULL, *q;
    const uint8_t *p = data_in;
    if ((len_in < HDR_VOC_LEN) || (memcmp(p, HDR_VOC, HDR_VOC_LEN) != 0)) {
        log_error("VOC: invalid header\n");
        return res;
    }
    p += HDR_VOC_LEN;
    len_in -= HDR_VOC_LEN;
    cursize = len_in * 2/*16b*/ * 2/*stereo*/;
    q = data = lib_malloc(cursize);
    while (len_in) {
        uint8_t block_type, ctype = 0, stereo = 0, sr;
        uint32_t block_size, newrate;
        block_type = *p++;
        --len_in;
        LOG_DEBUG((DEBUGLEVEL_FMTSFX, "VOC: block %02x ", block_type));
        if (block_type == 0x00) {    /* Terminator */
            LOG_DEBUG((DEBUGLEVEL_FMTSFX, "\n"));
            break;
        }
        if (block_type > 0x08) {
            log_error("VOC: invalid block %02x\n", block_type);
            goto fail;
        }
        if (len_in < 3) {
            log_error("VOC: no size for block %02x\n", block_type);
            goto fail;
        }
        block_size = GET_LE_24(p);
        LOG_DEBUG((DEBUGLEVEL_FMTSFX, "sz %x ", block_size));
        p += 3;
        len_in -= 3;
        if (len_in < block_size) {
            log_error("VOC: block %02x sz %i but only %i left\n", block_type, block_size, len_in);
            goto fail;
        }
        switch (block_type) {
            case 0x01:  /* Sound data */
                sr = *p++;
                --len_in;
                newrate = 1000000 / (256 - sr);
                ctype = *p++;
                --len_in;
                LOG_DEBUG((DEBUGLEVEL_FMTSFX, " sr %i->%iHz ct %i  ", sr, newrate, ctype));
                block_size -= 2;
                if ((res.samplerate != 0) && (res.samplerate != newrate)) {
                    log_error("VOC: multiple sample rates unimpl (%i -> %i)\n", res.samplerate, newrate);
                    goto fail;
                }
                res.samplerate = newrate;
                /* fall through */
            case 0x02:  /* Sound continue */
                LOG_DEBUG((DEBUGLEVEL_FMTSFX, " \n"));
                len_in -= block_size;
                if (((res.num + block_size) * 4) > cursize) {
                    uint32_t newsize;
                    newsize = cursize + ((block_size * 4) | 0xff) + 1;
                    LOG_DEBUG((DEBUGLEVEL_FMTSFX, " realloc %i->%i ", cursize, newsize));
                    data = lib_realloc(data, newsize);
                    cursize = newsize;
                    q = &data[res.num * 2/*stereo*/];
                }
                /* TODO handle ctype!=0 and stereo */
                if ((ctype != 0/*8-bit*/)) {
                    log_error("VOC: non-8-bit compression type %u unimpl\n", ctype);
                    goto fail;
                }
                res.num += block_size;
                while (block_size--) {
                    int16_t s;
                    s = (((int16_t)*p++) - 128) << 8;
                    *q++ = s;
                    *q++ = s;
                }
                break;
            case 0x03:  /* Silence */
                {
                    uint16_t len_silence;
                    len_silence = GET_LE_16(p) + 1;
                    p += 2;
                    len_in -= 2;
                    sr = *p++;
                    --len_in;
                    newrate = 1000000 / (256 - sr);
                    LOG_DEBUG((DEBUGLEVEL_FMTSFX, " silence %i sr %i->%iHz\n", len_silence, sr, newrate));
                    if (((res.num + len_silence) * 4) > cursize) {
                        uint32_t newsize;
                        newsize = cursize + ((len_silence * 4) | 0xff) + 1;
                        LOG_DEBUG((DEBUGLEVEL_FMTSFX, " realloc %i->%i ", cursize, newsize));
                        data = lib_realloc(data, newsize);
                        cursize = newsize;
                        q = &data[res.num * 2/*stereo*/];
                    }
                    res.num += len_silence;
                    while (len_silence--) {
                        *q++ = 0;
                        *q++ = 0;
                    }
                }
                break;
            case 0x04:  /* Marker */
                {
                    IF_DEBUG(uint16_t marker; marker = GET_LE_16(p);)
                    p += 2;
                    len_in -= 2;
                    LOG_DEBUG((DEBUGLEVEL_FMTSFX, " marker %i at %i\n", marker, p - data_in));
                }
                break;
            case 0x05:  /* ASCII */
                LOG_DEBUG((DEBUGLEVEL_FMTSFX, " '%s'\n", p));
                p += block_size;
                len_in -= block_size;
                break;
            case 0x06:  /* Repeat */
                {
                    IF_DEBUG(uint16_t repeatnum; repeatnum = GET_LE_16(p);)
                    p += 2;
                    len_in -= 2;
                    LOG_DEBUG((DEBUGLEVEL_FMTSFX, " repeat %i\n", repeatnum));
                    /* TODO */
                }
                break;
            case 0x07:  /* End repeat */
                /* TODO */
                LOG_DEBUG((DEBUGLEVEL_FMTSFX, "end repeat\n"));
                break;
            case 0x08:  /* Extended */
                {
                    uint16_t tc;
                    IF_DEBUG(uint8_t pack;)
                    tc = GET_LE_16(p);
                    p += 2;
                    len_in -= 2;
                    IF_DEBUG(pack = *p;)
                    ++p;
                    --len_in;
                    stereo = *p++;
                    --len_in;
                    newrate = stereo ? (128000000 / (65536 - tc)) : (256000000 / (65536 - tc));
                    if ((res.samplerate != 0) && (res.samplerate != newrate)) {
                        log_error("VOC: multiple sample rates unimpl (%i -> %i)\n", res.samplerate, newrate);
                        goto fail;
                    }
                    res.samplerate = newrate;
                    LOG_DEBUG((DEBUGLEVEL_FMTSFX, "tc %i->%iHz pack:%02x st:%02x\n", tc, newrate, pack, stereo));
                }
                break;
            default:
                break;
        }
    }
    if (len_in > 3) {
        log_warning("VOC: got terminator with %i bytes left\n", len_in);
    }
    res.data = data;
    return res;
fail:
    lib_free(data);
    return res;
}

static struct sfx_conv_s fmt_sfx_convert_wav(const uint8_t *data_in, uint32_t len_in)
{
    struct sfx_conv_s res = { NULL, 0, 0 };
    uint32_t cursize, len = 0;
    int16_t *data = NULL, *q, s;
    const uint8_t *p = data_in;
    int ch_in, bps_in;
    if (0
      || (len_in < HDR_WAV_LEN)
      || (memcmp(&p[0x00], (const uint8_t *)"RIFF", 0) != 0)
      || (memcmp(&p[0x0c], (const uint8_t *)"fmt ", 0) != 0)
      || (GET_LE_32(&p[0x10]) != 0x10)
      || (GET_LE_16(&p[0x14]) != 1)
      || (((ch_in = GET_LE_16(&p[0x16])) != 1) && (ch_in != 2))
      || (((bps_in = GET_LE_16(&p[0x22])) != 8) && (bps_in != 16))
      || (memcmp(&p[0x24], (const uint8_t *)"data", 0) != 0)
      || ((len = GET_LE_32(&p[0x28])) > (len_in - HDR_WAV_LEN))
    ) {
        log_error("WAV: invalid header\n");
        return res;
    }
    res.samplerate = GET_LE_32(&p[0x18]);
    p += HDR_WAV_LEN;
    len_in -= HDR_WAV_LEN;
    cursize = len;
    if (ch_in == 1) {
        cursize <<= 1;
    }
    if (bps_in == 8) {
        cursize <<= 1;
    }
    q = data = lib_malloc(cursize);

    if (bps_in == 8) {
        if (ch_in == 1) {
            res.num = len;
            for (int i = 0; i < len; ++i) {
                s = *p++ << 8;
                *q++ = s;
                *q++ = s;
            }
        } else {
            res.num = len / 2;
            for (int i = 0; i < (len / 2); ++i) {
                s = *p++ << 8;
                *q++ = s;
                s = *p++ << 8;
                *q++ = s;
            }
        }
    } else /*(bps_in == 16)*/{
        const int16_t *d = (const int16_t *)p;
        if (ch_in == 1) {
            res.num = len / 2;
            for (int i = 0; i < (len / 2); ++i) {
                s = GET_LE_16(d);
                ++d;
                *q++ = s;
                *q++ = s;
            }
        } else {
            res.num = len / 4;
            for (int i = 0; i < (len / 4); ++i) {
                s = GET_LE_16(d);
                ++d;
                *q++ = s;
                s = GET_LE_16(d);
                ++d;
                *q++ = s;
            }
        }
    }

    res.data = data;
    return res;
}

/* -------------------------------------------------------------------------- */

sfx_type_t fmt_sfx_detect(const uint8_t *data, uint32_t len)
{
    uint32_t hdrid;
    if (len < 32) {
        return SFX_TYPE_UNKNOWN;
    }
    hdrid = GET_BE_32(data);
    if (hdrid == HDRID_LBXVOC) {
        return SFX_TYPE_LBXVOC;
    } else if (hdrid == HDRID_VOC) {
        return SFX_TYPE_VOC;
    } else if (hdrid == HDRID_WAV) {
        return SFX_TYPE_WAV;
    }
    return SFX_TYPE_UNKNOWN;
}

bool fmt_sfx_convert(const uint8_t *data_in, uint32_t len_in, uint8_t **data_out_ptr, uint32_t *len_out_ptr, sfx_type_t *type_out, int audiorate, bool add_wav_header)
{
    sfx_type_t type = fmt_sfx_detect(data_in, len_in);
    struct sfx_conv_s conv_res = { NULL, 0, 0 };
    uint8_t *data;
    uint32_t len;
    if (type_out) {
        *type_out = type;
    }
    switch (type) {
        case SFX_TYPE_LBXVOC:
            data_in += HDR_LBXVOC_LEN;
            len_in -= HDR_LBXVOC_LEN;
            /* fall through */
        case SFX_TYPE_VOC:
            conv_res = fmt_sfx_convert_voc(data_in, len_in);
            break;
        case SFX_TYPE_WAV:
            conv_res = fmt_sfx_convert_wav(data_in, len_in);
            break;
        default:
            break;
    }
    if (!conv_res.data) {
        goto fail;
    }
    if (audiorate && (audiorate != conv_res.samplerate)) {
        struct sfx_conv_s conv_old;
        conv_old = conv_res;
#ifdef HAVE_SAMPLERATE
        if (opt_use_libsamplerate) {
            conv_res = fmt_sfx_resample_libsamplerate(&conv_old, audiorate);
        } else
#endif
        {
            conv_res = fmt_sfx_resample_simple(&conv_old, audiorate);
        }
        lib_free(conv_old.data);
    }
    data = (uint8_t *)conv_res.data;
    len = conv_res.num * 2/*16b*/ * 2/*stereo*/;
    if (add_wav_header) {
        uint8_t wav_header[] = {
            /*00*/ 'R', 'I', 'F', 'F',
            /*04*/ 0, 0, 0, 0,  /* filesize - 8 */
            /*08*/ 'W', 'A', 'V', 'E',
            /*0c*/ 'f', 'm', 't', ' ',
            /*10*/ 16, 0, 0, 0, /* length of fmt chunk */
            /*14*/ 1, 0,        /* format */
            /*16*/ 2, 0,        /* channels */
            /*18*/ 0, 0, 0, 0,  /* Hz */
            /*1c*/ 0, 0, 0, 0,  /* Hz * 2B/sample * stereo */
            /*20*/ 4, 0,        /* block align */
            /*22*/ 16, 0,       /* bits per sample */
            /*24*/ 'd', 'a', 't', 'a',
            /*28*/ 0, 0, 0, 0   /* sample data size in bytes */
        };
        SET_LE_32(&wav_header[0x4], len + HDR_WAV_LEN - 8);
        SET_LE_32(&wav_header[0x18], audiorate);
        SET_LE_32(&wav_header[0x1c], audiorate * 4);
        SET_LE_32(&wav_header[0x28], len);
        data = lib_realloc(data, len + HDR_WAV_LEN);
#ifdef WORDS_BIGENDIAN
        /* WAV file is little endian so swap bytes while relocating */
        /* TODO maybe use the big endian variant RIFX? */
        {
            uint16_t *q = (uint16_t *)&data[len - 2];
            uint16_t *p = (uint16_t *)&data[len + HDR_WAV_LEN - 2];
            int n = len / 2;
            while (n--) {
                uint16_t v = *q--;
                *p-- = BSWAP_16(v);
            }
        }
#else
        memmove(&data[HDR_WAV_LEN], data, len);
#endif
        memcpy(data, wav_header, HDR_WAV_LEN);
        len += HDR_WAV_LEN;
    }
    if (data_out_ptr) {
        *data_out_ptr = data;
    }
    if (len_out_ptr) {
        *len_out_ptr = len;
    }
    return true;
fail:
    lib_free(conv_res.data);
    if (data_out_ptr) {
        *data_out_ptr = NULL;
    }
    if (len_out_ptr) {
        *len_out_ptr = 0;
    }
    return false;
}
