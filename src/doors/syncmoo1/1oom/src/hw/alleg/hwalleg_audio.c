#include "config.h"

#include <stdio.h>
#include <unistd.h>

#include "types.h"
#include <allegro.h>

#include "hw.h"
#include "hwalleg_audio.h"
#include "fmt_mus.h"
#include "fmt_sfx.h"
#include "hwalleg_opt.h"
#include "lib.h"
#include "log.h"
#include "options.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static bool audio_initialized = false;
static int audio_rate = 0, audio_bits = 0, audio_channels = 0;

struct sfx_s {
    struct SAMPLE *s;
};
static int sfx_num = 0;
static struct sfx_s *sfxtbl = NULL;
static int sfx_playing;

struct mus_s {
    mus_type_t type;
    struct MIDI *music;
    bool loops;
};
static int mus_num = 0;
static struct mus_s *mustbl = NULL;
static int mus_playing;

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

int hw_audio_init(void)
{
    if (opt_audio_enabled) {
        reserve_voices(1, -1);
        set_volume_per_voice(0);
        if (install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL) != 0) {
            log_error("initialising Allegro sound: %s\n", allegro_error);
            return -1;
        }
        audio_rate = get_mixer_frequency();
        audio_bits = get_mixer_bits();
        audio_channels = get_mixer_channels();
        log_message("Audio: Sound %s: %s\n", digi_driver->name, digi_driver->desc);
        log_message("Audio: Sound %i bit, %i Hz, %i ch\n", audio_bits, audio_rate, audio_channels);
        log_message("Audio: Music %s: %s\n", midi_driver->name, midi_driver->desc);
        if ((audio_bits == 8) && (audio_channels == 1)) {
            log_error("8 bit 1 ch sound currently not supported! Set sbtype=sb16 in your dosbox-*.conf, buy some new hardware, or start with -noaudio.\n");
            return -1;
        }
        sfx_playing = -1;
        mus_playing = -1;
        audio_initialized = true;
        {
            int volume;
            volume = opt_sfx_volume;
            opt_sfx_volume = -1;
            hw_audio_sfx_volume(volume);
            volume = opt_music_volume;
            opt_music_volume = -1;
            hw_audio_music_volume(volume);
        }
    }
    return 0;
}

void hw_audio_shutdown(void)
{
    if (audio_initialized) {
        log_message("Audio: shutdown\n");
        remove_sound();
        for (int i = 0; i < sfx_num; ++i) {
            hw_audio_sfx_release(i);
        }
        lib_free(sfxtbl);
        sfxtbl = NULL;
        audio_initialized = false;
    }
}

int hw_audio_music_init(int mus_index, const uint8_t *data_in, uint32_t len_in)
{
    const uint8_t *data = NULL;
    uint8_t *buf = NULL;
    uint32_t len = 0;
    struct mus_s *m;
    int res;

    if (!audio_initialized) {
        return 0;
    }

    if (mus_index >= mus_num) {
        int old_mus_num = mus_num;
        mus_num = (mus_index + 1);
        mustbl = lib_realloc(mustbl, mus_num * sizeof(struct mus_s));
        for (int i = old_mus_num; i < mus_num; ++i) {
            mustbl[i].type = MUS_TYPE_UNKNOWN;
            mustbl[i].music = NULL;
        }
    }

    m = &mustbl[mus_index];

    if (m->type != MUS_TYPE_UNKNOWN) {
        hw_audio_music_release(mus_index);
    }

    m->type = fmt_mus_detect(data_in, len_in);
    switch (m->type) {
        case MUS_TYPE_LBXXMID:
            if (fmt_mus_convert_xmid(data_in, len_in, &buf, &len, &m->loops)) {
                data = buf;
            } else {
                m->type = MUS_TYPE_UNKNOWN;
            }
            break;
        case MUS_TYPE_MIDI:
            break;
        case MUS_TYPE_UNKNOWN:
            break;
        default:
            log_error("Audio: unsupported music type %i\n", m->type);
            m->type = MUS_TYPE_UNKNOWN;
            break;
    }

    if (m->type == MUS_TYPE_UNKNOWN) {
        log_error("Audio: failed to init music %i\n", mus_index);
        return -1;
    }

    /* HACK Allegro provides no way to read a MIDI file in memory. Use a temp file. */
    res = util_file_save("1tempmus.mid", data, len);
    lib_free(buf);
    buf = 0;
    data = 0;
    if (res) {
        log_error("Audio: failed to create temp file\n");
        m->type = MUS_TYPE_UNKNOWN;
        return -1;
    }
    m->music = load_midi("1tempmus.mid");
    unlink("1tempmus.mid");
    if (!m->music) {
        log_error("Audio: load_midi failed on music %i (type %i)\n", mus_index, m->type);
        m->type = MUS_TYPE_UNKNOWN;
        return -1;
    }
    return 0;
}

void hw_audio_music_release(int mus_index)
{
    if (mus_index < mus_num) {
        if (mustbl[mus_index].music) {
            destroy_midi(mustbl[mus_index].music);
            mustbl[mus_index].music = NULL;
        }
        mustbl[mus_index].type = MUS_TYPE_UNKNOWN;
    }
}

void hw_audio_music_play(int mus_index)
{
    if (audio_initialized && opt_music_enabled && (mus_index < mus_num)) {
        stop_midi();
        play_midi(mustbl[mus_index].music, mustbl[mus_index].loops ? 1 : 0);
        mus_playing = mus_index;
    }
}

void hw_audio_music_fadeout(void)
{
    /* TODO */
    hw_audio_music_stop();
}

void hw_audio_music_stop(void)
{
    if (audio_initialized && opt_music_enabled && (mus_playing >= 0)) {
        stop_midi();
        mus_playing = -1;
    }
}

bool hw_audio_music_volume(int volume)
{
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 128) {
        volume = 128;
    }
    if (audio_initialized && opt_music_enabled) {
        set_volume(-1, volume * 2);
    }
    if (opt_music_volume != volume) {
        log_message("Audio: music volume %i\n", volume);
        opt_music_volume = volume;
    }
    return true;
}

/* -------------------------------------------------------------------------- */

int hw_audio_sfx_batch_start(int sfx_index_max)
{
    return 0;
}

int hw_audio_sfx_batch_end(void)
{
    return 0;
}

int hw_audio_sfx_init(int sfx_index, const uint8_t *data_in, uint32_t len_in)
{
    uint8_t *data = NULL;
    uint32_t len = 0;

    if (!audio_initialized) {
        return 0;
    }

    if (sfx_index >= sfx_num) {
        int old_sfx_num = sfx_num;
        sfx_num = (sfx_index + 1);
        sfxtbl = lib_realloc(sfxtbl, sfx_num * sizeof(struct sfx_s));
        for (int i = old_sfx_num; i < sfx_num; ++i) {
            sfxtbl[i].s = NULL;
        }
    }

    if (sfxtbl[sfx_index].s) {
        hw_audio_sfx_release(sfx_index);
    }

    if (fmt_sfx_convert(data_in, len_in, &data, &len, NULL, audio_rate, false)) {
        SAMPLE *s;
        const int16_t *q;
        s = create_sample(audio_bits, audio_channels, audio_rate, len / 4);
        if (!s) {
            log_error("Audio: failed to init sound %i\n", sfx_index);
            return -1;
        }
        q = (const int16_t *)data;
        /* convert signed to unsigned, (if needed) stereo to mono and 16 to 8 bits */
        if (audio_bits == 16) {
            uint16_t *p;
            p = s->data;
            if (audio_channels == 2) {
                for (int i = 0; i < (len / 2); ++i) {
                    *p++ = *q++ ^ 0x8000;
                }
            } else {
                for (int i = 0; i < (len / 4); ++i) {
                    int v;
                    v = *q++;
                    v += *q++;
                    v = ((v / 2) & 0xffff);
                    *p++ = v ^ 0x8000;
                }
            }
        } else {
            uint8_t *p;
            p = s->data;
            if (audio_channels == 2) {
                for (int i = 0; i < (len / 2); ++i) {
                    *p++ = ((((uint16_t)*q++) >> 9) ^ 0x40) | 0x80;
                }
            } else {
                for (int i = 0; i < (len / 4); ++i) {
                    int v;
                    v = *q++;
                    v += *q++;
                    v = ((v / 2) & 0xffff);
                    *p++ = ((((uint16_t)v) >> 9) ^ 0x40) | 0x80;
                }
            }
        }
        sfxtbl[sfx_index].s = s;
        lib_free(data);
    } else {
        log_error("Audio: failed to init sound %i\n", sfx_index);
        return -1;
    }
    return 0;
}

void hw_audio_sfx_release(int sfx_index)
{
    if (sfx_index < sfx_num) {
        if (sfxtbl[sfx_index].s) {
            destroy_sample(sfxtbl[sfx_index].s);
            sfxtbl[sfx_index].s = NULL;
        }
    }
}

void hw_audio_sfx_play(int sfx_index)
{
    if (audio_initialized && opt_sfx_enabled && (sfx_index < sfx_num)) {
        play_sample(sfxtbl[sfx_index].s, 255, 128, 1000, 0);
        sfx_playing = sfx_index;
    }
}

void hw_audio_sfx_stop(void)
{
    if (audio_initialized && opt_sfx_enabled && (sfx_playing >= 0)) {
        stop_sample(sfxtbl[sfx_playing].s);
        sfx_playing = -1;
    }
}

bool hw_audio_sfx_volume(int volume)
{
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 128) {
        volume = 128;
    }
    if (audio_initialized && opt_sfx_enabled) {
        set_volume(volume * 2, -1);
    }
    if (opt_sfx_volume != volume) {
        log_message("Audio: sfx volume %i\n", volume);
        opt_sfx_volume = volume;
    }
    return true;
}
