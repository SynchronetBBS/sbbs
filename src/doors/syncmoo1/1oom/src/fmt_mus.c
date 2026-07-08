#include "config.h"

#include <stdio.h>
#include <string.h>

#include "fmt_mus.h"
#include "bits.h"
#include "fmt_id.h"
#include "lib.h"
#include "log.h"
#include "options.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_FMTMUS   4

#define HDR_MIDI_LEN    0x16

typedef struct noteoff_s {
    uint32_t t;     /* time of event */
    int16_t next;    /* next event, sorted by time */
    uint8_t ch;     /* 0 if unused, otherwise 0x8Z whe Z is channel */
    uint8_t note;
} noteoff_t;

/* MOO1 has max. 15 pending noteoffs */
#define NOTEOFFBUFSIZE  32

struct noteoffs_s {
    noteoff_t tbl[NOTEOFFBUFSIZE];  /* noteoff events */
    int16_t top;    /* noteoff with nearest time */
    int16_t pos;    /* position of (likely) free tbl entry */
    int8_t num; /* number of pending events */
    int8_t max; /* max. number of pending events */
};

#define XMID_TICKSPERQ  60
/*#define XMID_USE_BANKS*/

/* -------------------------------------------------------------------------- */

static int8_t xmid_find_free_noteoff(struct noteoffs_s *s)
{
    int i = s->pos;
    int num = NOTEOFFBUFSIZE;
    while (num) {
        if (++i == NOTEOFFBUFSIZE) {
            i = 0;
        }
        if (s->tbl[i].ch == 0) {
            s->pos = i;
            return i;
        }
        --num;
    }
    return -1;
}

static bool xmid_add_pending_noteoff(struct noteoffs_s *s, const uint8_t *data, uint32_t t_now, uint32_t duration)
{
    uint32_t t = t_now + duration;
    int8_t i = xmid_find_free_noteoff(s);
    noteoff_t *n;
    if (i < 0) {
        log_error("XMID: BUG noteoff tbl full!\n");
        return false;
    }
    n = &(s->tbl[i]);
    n->next = -1;
    n->t = t;
    n->ch = data[0] & 0x8f; /* 9x -> 8x */
    n->note = data[1];

    if (s->top < 0) {
        s->top = i;
    } else {
        int j, k;
        j = s->top;
        k = -1;
        while ((j >= 0) && (t >= s->tbl[j].t)) {
            k = j;
            j = s->tbl[j].next;
        }
        if (k < 0) {
            n->next = s->top;
            s->top = i;
        } else {
            s->tbl[k].next = i;
            s->tbl[i].next = j;
        }
    }
    if (++s->num > s->max) {
        s->max = s->num;
    }
    return true;
}

static uint32_t xmid_encode_delta_time(uint8_t *buf, uint32_t delta_time)
{
    uint32_t len_event = 0, v = delta_time & 0x7f;
    while ((delta_time >>= 7) != 0) {
        v <<= 8;
        v |= (delta_time & 0x7f) | 0x80;
    }
    while (1) {
        buf[len_event++] = (uint8_t)(v & 0xff);
        if (v & 0x80) {
            v >>= 8;
        } else {
            return len_event;
        }
    }
}

static int xmid_convert_evnt(const uint8_t *data_in, uint32_t len_in, const uint8_t *timbre_tbl, uint16_t timbre_num, uint8_t *p, bool *tune_loops)
{
    struct noteoffs_s s[1];
    int rc = -1, noteons = 0, looppoint = -1;
    uint32_t len_out = 0, t_now = 0, delta_time = 0;
    bool end_found = false;
    *tune_loops = false;
    memset(s, 0, sizeof(s[0]));
    s->top = -1;

    while ((len_in > 0) && (!end_found)) {
        uint32_t len_event, add_extra_bytes, skip_extra_bytes;
        uint8_t buf_extra[4];
        bool add_event;

        add_event = true;
        add_extra_bytes = 0;
        skip_extra_bytes = 0;

        switch (*data_in & 0xf0) {
            case 0x90:
                {
                    uint32_t dt_off;
                    uint8_t b;
                    dt_off = 0;
                    skip_extra_bytes = 1;
                    while (((b = data_in[2 + skip_extra_bytes]) & 0x80) != 0) {
                        dt_off |= b & 0x7f;
                        dt_off <<= 7;
                        ++skip_extra_bytes;
                    }
                    dt_off |= b;
                    if (!xmid_add_pending_noteoff(s, data_in, t_now + delta_time, dt_off)) {
                        goto fail;
                    }
                }
                ++noteons;
                len_event = 3;
                break;

            case 0xb0:
                len_event = 3;
                {
                    uint8_t c;
                    c = data_in[1];
                    if (0
                      || ((c >= 0x20) && (c <= 0x2e))
                      || ((c >= 0x3a) && (c <= 0x3f))
                      || ((c >= 0x6e) && (c <= 0x78))
                    ) {
                        LOG_DEBUG((DEBUGLEVEL_FMTMUS, "XMID: dropping unhandled AIL CC event %02x %02x %02x, notes %i\n", *data_in, c, data_in[2], noteons));
                        add_event = false;
                        len_event = 0;
                        skip_extra_bytes = 3;
                    }
                    switch (c) {
                        case 0x74:  /* AIL loop: FOR loop = 1 to n */
                            if (looppoint >= 0) {
                                log_warning("XMID: nth FOR loop unimpl\n");
                            } else {
                                looppoint = noteons;
                                if (looppoint > 0) {
                                    log_warning("XMID: FOR loop after %i notes unimpl\n", looppoint);
                                }
                            }
                            break;
                        case 0x75:  /* AIL loop: NEXT/BREAK */
                            LOG_DEBUG((DEBUGLEVEL_FMTMUS, "XMID: NEXT/BREAK at %i after %i notes, forcing end\n", looppoint, noteons));
                            if (looppoint >= 0) {
                                *tune_loops = true;
                            } else {
                                log_warning("XMID: NEXT/BREAK without FOR\n");
                            }
                            len_event = 0;
                            end_found = true;
                            add_event = true;
                            buf_extra[0] = 0xff;
                            buf_extra[1] = 0x2f;
                            buf_extra[2] = 0x00;
                            add_extra_bytes = 3;
                            break;
                        default:
                            break;
                    }
                }
                break;

            case 0xa0:
            case 0xe0:
                len_event = 3;
                break;
            case 0x80:
                log_error("XMID: unexpected noteoff\n");
                goto fail;
            case 0xc0:
                len_event = 2;
#ifdef XMID_USE_BANKS
                {
                    int ti;
                    uint8_t patch;
                    uint8_t bank = 0;
                    patch = data_in[1];
                    for (ti = 0; ti < timbre_num; ++ti) {
                        if (timbre_tbl[ti * 2] == patch) {
                            bank = timbre_tbl[ti * 2 + 1];
                            break;
                        }
                    }
                    if (ti < timbre_num) {
                        LOG_DEBUG((DEBUGLEVEL_FMTMUS, "XMID: TIMB found bank 0x%02x for patch 0x%02x\n", bank, patch));
                        buf_extra[0] = 0xb0 | (data_in[1] & 0xf);
                        buf_extra[1] = 0;
                        buf_extra[2] = bank;
                        buf_extra[3] = 0;
                        add_extra_bytes = 4;
                    } else {
                        LOG_DEBUG((DEBUGLEVEL_FMTMUS, "XMID: TIMB no bank for patch 0x%02x\n", patch));
                    }
                }
#endif /*XMID_USE_BANKS*/
                break;
            case 0xd0:
                len_event = 2;
                break;
            case 0xf0:
                switch (*data_in & 0xf) {
                    case 0x08:
                    case 0x0a:
                    case 0x0b:
                    case 0x0c:
                        len_event = 1;
                        break;
                    case 0x0f:
                        len_event = 3 + data_in[2];
                        if (data_in[1] == 0x2f) {
                            end_found = true;
                        } else if (data_in[1] == 0x51) {
                            /* MOO1 seems to ignore the set tempo events as not dropping them results in wrong tempo in f.ex tune 0xA */
                            LOG_DEBUG((DEBUGLEVEL_FMTMUS, "XMID: dropping tempo %u event after %i notes\n", GET_BE_24(&data_in[3]), noteons));
                            add_event = false;
                            skip_extra_bytes = len_event;
                        }
                        break;
                    default:
                        log_error("XMID: unhandled event 0x%02x\n", *data_in);
                        goto fail;
                }
                break;
            default:    /* 0x00..0x7f */
                add_event = false;
                while (!(*data_in & 0x80)) {
                    delta_time += *data_in++;
                    --len_in;
                }
                break;
        }

        if (add_event) {
            uint32_t len_delta_time;
            uint8_t buf_delta_time[4];

            while ((s->top >= 0) && ((t_now + delta_time) >= s->tbl[s->top].t)) {
                noteoff_t *n = &(s->tbl[s->top]);
                uint32_t delay_noff = n->t - t_now;
                len_delta_time = xmid_encode_delta_time(buf_delta_time, delay_noff);
                for (int i = 0; i < len_delta_time; ++i) {
                    *p++ = buf_delta_time[i];
                }
                len_out += len_delta_time;
                *p++ = n->ch;
                *p++ = n->note;
                *p++ = 0x00;
                len_out += 3;
                delta_time -= delay_noff;
                t_now += delay_noff;
                n->ch = 0;
                s->top = n->next;
                --s->num;
            }
            t_now += delta_time;
            len_delta_time = xmid_encode_delta_time(buf_delta_time, delta_time);
            delta_time = 0;

            for (int i = 0; i < len_delta_time; ++i) {
                *p++ = buf_delta_time[i];
            }

            if (end_found) {
                /* last event, add remaining noteoffs */
                LOG_DEBUG((DEBUGLEVEL_FMTMUS, "XMID: %i noteoffs at end, max %i noteoffs, total %i noteons\n", s->num, s->max, noteons));
                while (s->top >= 0) {
                    noteoff_t *n = &(s->tbl[s->top]);
                    *p++ = n->ch;
                    *p++ = n->note;
                    *p++ = 0x00;
                    *p++ = 0;
                    len_out += 4;
                    n->ch = 0;
                    s->top = n->next;
                }
                s->num = 0;
            }

            for (int i = 0; i < add_extra_bytes; ++i) {
                *p++ = buf_extra[i];
            }

            for (int i = 0; i < len_event; ++i) {
                uint8_t c;
                c = *data_in++;
                --len_in;
                *p++ = c;
            }

            len_out += len_event + len_delta_time + add_extra_bytes;
        }

        if (skip_extra_bytes) {
            data_in += skip_extra_bytes;
            len_in -= skip_extra_bytes;
        }
    }

    rc = len_out;
fail:
    return rc;
}

/* -------------------------------------------------------------------------- */

mus_type_t fmt_mus_detect(const uint8_t *data, uint32_t len)
{
    uint32_t hdrid;
    if (len < 32) {
        return MUS_TYPE_UNKNOWN;
    }
    hdrid = GET_BE_32(data);
    if (hdrid == HDRID_LBXXMID) {
        return MUS_TYPE_LBXXMID;
    } else if (hdrid == HDRID_MIDI) {
        return MUS_TYPE_MIDI;
    } else if (hdrid == HDRID_WAV) {
        return MUS_TYPE_WAV;
    } else if (hdrid == HDRID_OGG) {
        return MUS_TYPE_OGG;
    } else if (hdrid == HDRID_FLAC) {
        return MUS_TYPE_FLAC;
    }
    return MUS_TYPE_UNKNOWN;
}

bool fmt_mus_convert_xmid(const uint8_t *data_in, uint32_t len_in, uint8_t **data_out_ptr, uint32_t *len_out_ptr, bool *tune_loops)
{
    uint8_t *data = NULL;
    const uint8_t *timbre_tbl;
    uint32_t len_timbre, len_evnt;
    uint16_t timbre_num;
    int len = 0;

    if (0
      || (fmt_mus_detect(data_in, len_in) != MUS_TYPE_LBXXMID)
      || (len_in < 0x4e)
      || (memcmp(&data_in[0x10], (const uint8_t *)"FORM", 4) != 0)
      || (memcmp(&data_in[0x3e], (const uint8_t *)"TIMB", 4) != 0)
      || ((len_timbre = GET_BE_32(&data_in[0x42])), (len_in < (0x4e + len_timbre)))
      || (memcmp(&data_in[0x46 + len_timbre], (const uint8_t *)"EVNT", 4) != 0)
      || ((len_evnt = GET_BE_32(&data_in[0x4a + len_timbre])), (len_in < (0x4e + len_timbre + len_evnt)))
    ) {
        log_error("XMID: invalid header\n");
        goto fail;
    }
    LOG_DEBUG((DEBUGLEVEL_FMTMUS, "XMID: lent %i lene %i\n", len_timbre, len_evnt));

    timbre_num = GET_LE_16(&data_in[0x46]);
    timbre_tbl = &data_in[0x48];

    data_in += 0x4e + len_timbre;

    /* max. len_out/len_in ratio for MOO1 data is about 1.8 */
    data = lib_malloc(HDR_MIDI_LEN + len_evnt * 2);
    {
        uint8_t hdr[] = {
            /*00*/ 'M', 'T', 'h', 'd',
            /*04*/ 0, 0, 0, 6,
            /*08*/ 0, 0, 0, 1,
            /*0c*/ (XMID_TICKSPERQ >> 8) & 0xff, XMID_TICKSPERQ & 0xff,
            /*0e*/ 'M', 'T', 'r', 'k'
            /*12*/ /* length, big endian */
        };
        memcpy(data, hdr, sizeof(hdr));
    }

    len = xmid_convert_evnt(data_in, len_evnt, timbre_tbl, timbre_num, &data[HDR_MIDI_LEN], tune_loops);
    LOG_DEBUG((DEBUGLEVEL_FMTMUS, "XMID: lene %i len %i (%f) %s\n", len_evnt, len, (double)len / (double)len_evnt, *tune_loops ? "loop" : "once"));
    if (len < 0) {
        goto fail;
    }
    SET_BE_32(&data[0x12], len);
    len += HDR_MIDI_LEN;
    data = lib_realloc(data, len);

    if (data_out_ptr) {
        *data_out_ptr = data;
    }
    if (len_out_ptr) {
        *len_out_ptr = len;
    }
    return true;

fail:
    lib_free(data);
    if (data_out_ptr) {
        *data_out_ptr = NULL;
    }
    if (len_out_ptr) {
        *len_out_ptr = 0;
    }
    return false;
}
