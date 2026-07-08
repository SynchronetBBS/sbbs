#include "config.h"

#include <stdio.h>

#include "hw.h"
#include "hwsdl_audio.h"
#include "hwsdl_opt.h"
#include "log.h"
#include "options.h"
#include "types.h"

#ifdef HAVE_SDLMIXER
#include "fmt_mus.h"
#include "fmt_sfx.h"
#include "lib.h"
#endif /* HAVE_SDLMIXER */

/* -------------------------------------------------------------------------- */

#ifdef HAVE_SDLMIXER

static bool audio_initialized = false;
static int audio_rate = 0;

struct sfx_s {
    Mix_Chunk *chunk;
    bool pending;
};

static int sfx_num = 0;
static struct sfx_s *sfxtbl = NULL;
static int sfx_playing;

#ifdef USE_SFX_INIT_THREAD

struct sfx_process_sfx_s {
    int sfxi;
    const uint8_t *data;
    uint32_t len;
};

struct sfx_process_s {
    volatile bool working;
    volatile bool busy;
    bool started;
    int num, sfximax, error, errori;
    uint32_t t_start;
    struct sfx_process_sfx_s *tbl;
};
static struct sfx_process_s sfx_process = { false, false, false, 0, 0, 0, 0, 0, NULL };

static SDL_Thread *sfx_process_thread = NULL;

#endif /* USE_SFX_INIT_THREAD */

struct mus_s {
    mus_type_t type;
    Mix_Music *music;
    Mix_MusicType sdlmtype;
    uint8_t *buf;   /* WAV music files need the data to be kept */
    bool loops;
};
static int mus_num = 0;
static struct mus_s *mustbl = NULL;
static int mus_playing;

/* -------------------------------------------------------------------------- */

static int get_slice_size(void)
{
    int limit;
    int n;
    limit = (opt_audiorate * opt_audioslice_ms) / 1000;
    /* Try all powers of two, not exceeding the limit. */
    for (n = 0; ; ++n) {
        /* 2^n <= limit < 2^n+1 ? */
        if ((1 << (n + 1)) > limit) {
            return (1 << n);
        }
    }
    /* Should never happen? */
    return 1024;
}

static Mix_MusicType mus_type_to_sdlm(mus_type_t type)
{
    switch (type) {
        case MUS_TYPE_LBXXMID:
        case MUS_TYPE_MIDI:
            return MUS_MID;
        case MUS_TYPE_WAV:
            return MUS_WAV;
        case MUS_TYPE_OGG:
            return MUS_OGG;
        case MUS_TYPE_FLAC:
            return MUS_FLAC;
        default:
            return MUS_NONE;
    }
}

/* -------------------------------------------------------------------------- */

int hw_audio_init(void)
{
    if (opt_audio_enabled) {
        int num_output_channels;
        uint16_t mixer_format;
        int slice = get_slice_size();
        if (Mix_OpenAudio(opt_audiorate, AUDIO_S16SYS, 2, slice) < 0) {
            log_error("initialising SDL_mixer (%i Hz, slice %i): %s\n", opt_audiorate, slice, Mix_GetError());
            goto failnoclose;
        }
        if (Mix_QuerySpec(&audio_rate, &mixer_format, &num_output_channels) == 0) {
            log_error("Failed to read SDL_mixer query spec");
            goto fail;
        }
        if (num_output_channels != 2) {
            log_warning("SDL_mixer gave %i output channels instead of 2\n", num_output_channels);
        }
        if (audio_rate != opt_audiorate) {
            log_warning("SDL_mixer gave %i Hz instead of %i Hz\n", audio_rate, opt_audiorate);
        }
        Mix_AllocateChannels(1);
        SDL_PauseAudio(0);
        sfx_playing = -1;
        mus_playing = -1;
        log_message("SDLA: init %i Hz slice %i\n", audio_rate, slice);
        {
            const char *sfstr;
            sfstr = Mix_GetSoundFonts();
            if (sfstr) {
                log_message("SDLA: soundfonts '%s'\n", sfstr);
            } else {
                log_message("SDLA: no soundfonts\n");
            }
        }
        if (hw_opt_sdlmixer_sf) {
            if (hw_audio_set_sdlmixer_sf(hw_opt_sdlmixer_sf) < 0) {
                goto fail;
            }
        }
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
fail:
    Mix_CloseAudio();
failnoclose:
    log_error("Audio init failed! Run with -noaudio to play without sound.\n");
    return -1;
}

void hw_audio_shutdown_pre(void)
{
#ifdef USE_SFX_INIT_THREAD
    sfx_process.working = false;
#endif /* USE_SFX_INIT_THREAD */
}

void hw_audio_shutdown(void)
{
    if (audio_initialized) {
        log_message("SDLA: shutdown\n");
        Mix_CloseAudio();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
#ifdef USE_SFX_INIT_THREAD
        sfx_process.working = false;
        if (sfx_process.busy) {
            int timeout = 20;
            while (sfx_process.busy && (--timeout)) {
                SDL_Delay(100);
            }
            if (!timeout) {
                log_warning("SDLA: timeout waiting for sfx process thread\n");
            }
        }
#endif /* USE_SFX_INIT_THREAD */
        for (int i = 0; i < sfx_num; ++i) {
            hw_audio_sfx_release(i);
        }
        lib_free(sfxtbl);
        sfxtbl = NULL;
        audio_initialized = false;
    }
    lib_free(hw_opt_sdlmixer_sf);
    hw_opt_sdlmixer_sf = NULL;
}

int hw_audio_set_sdlmixer_sf(const char *path)
{
    log_message("SDLA: setting soundfont to '%s'\n", path);
    if (Mix_SetSoundFonts(path) < 0) {
        log_error("SDLA: failed to set soundfonts to '%s'\n", path);
        return -1;
    }
    return 0;
}

int hw_audio_music_init(int mus_index, const uint8_t *data_in, uint32_t len_in)
{
    const uint8_t *data = NULL;
    uint8_t *buf = NULL;
    uint32_t len = 0;
    struct mus_s *m;

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
            mustbl[i].buf = NULL;
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
        case MUS_TYPE_WAV:
            if (fmt_sfx_convert(data_in, len_in, &buf, &len, NULL, audio_rate, true)) {
                data = buf;
                m->buf = buf;
                buf = NULL;
            } else {
                m->type = MUS_TYPE_UNKNOWN;
            }
            m->loops = false;   /* FIXME */
            break;
        case MUS_TYPE_UNKNOWN:
            break;
        default:
            data = data_in;
            len = len_in;
            m->loops = false;   /* FIXME */
            break;
    }

    m->sdlmtype = mus_type_to_sdlm(m->type);

    if (m->type == MUS_TYPE_UNKNOWN) {
        log_error("SDLA: failed to init music %i\n", mus_index);
        return -1;
    }
    {
        SDL_RWops *rw = SDL_RWFromConstMem(data, len);
        m->music = Mix_LoadMUSType_RW(rw, m->sdlmtype, 0);
        SDL_RWclose(rw);
    }
    lib_free(buf);
    if (!m->music) {
        log_error("SDLA: Mix_LoadMUSType_RW failed on music %i (type %i): %s\n", mus_index, m->type, Mix_GetError());
        m->type = MUS_TYPE_UNKNOWN;
        m->sdlmtype = MUS_NONE;
        return -1;
    }

    return 0;
}

void hw_audio_music_release(int mus_index)
{
    if (mus_index < mus_num) {
        if (mus_playing == mus_index) {
            hw_audio_music_stop();
        }
        if (mustbl[mus_index].music) {
            Mix_FreeMusic(mustbl[mus_index].music);
            mustbl[mus_index].music = NULL;
        }
        if (mustbl[mus_index].buf) {
            lib_free(mustbl[mus_index].buf);
            mustbl[mus_index].buf = NULL;
        }
        mustbl[mus_index].type = MUS_TYPE_UNKNOWN;
        mustbl[mus_index].sdlmtype = MUS_NONE;
    }
}

void hw_audio_music_play(int mus_index)
{
    if (audio_initialized && opt_music_enabled && (mus_index < mus_num)) {
        if (Mix_PlayingMusic()) {
            Mix_HaltMusic();
        }
        Mix_PlayMusic(mustbl[mus_index].music, mustbl[mus_index].loops ? -1 : 0);
        Mix_VolumeMusic(opt_music_volume);
        mus_playing = mus_index;
    }
}

void hw_audio_music_fadeout(void)
{
    if (audio_initialized && opt_music_enabled && Mix_PlayingMusic()) {
        Mix_FadeOutMusic(1000);
    }
}

void hw_audio_music_stop(void)
{
    if (audio_initialized && opt_music_enabled) {
        Mix_HaltMusic();
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
        Mix_VolumeMusic(volume);
    }
    if (opt_music_volume != volume) {
        log_message("SDLA: music volume %i\n", volume);
        opt_music_volume = volume;
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static void hw_audio_sfx_init_alloc(int sfx_index)
{
    if (sfx_index >= sfx_num) {
        int old_sfx_num = sfx_num;
        sfx_num = (sfx_index + 1);
        sfxtbl = lib_realloc(sfxtbl, sfx_num * sizeof(struct sfx_s));
        for (int i = old_sfx_num; i < sfx_num; ++i) {
            sfxtbl[i].chunk = NULL;
            sfxtbl[i].pending = false;
        }
    }
}

static int hw_audio_sfx_init_do(int sfx_index, const uint8_t *data_in, uint32_t len_in)
{
    uint8_t *data = NULL;
    uint32_t len = 0;

    hw_audio_sfx_init_alloc(sfx_index);

    if (sfxtbl[sfx_index].chunk) {
        hw_audio_sfx_release(sfx_index);
    }

    if (fmt_sfx_convert(data_in, len_in, &data, &len, NULL, audio_rate, true)) {
        sfxtbl[sfx_index].chunk = Mix_LoadWAV_RW(SDL_RWFromMem(data, len), 0);
        lib_free(data);
        sfxtbl[sfx_index].pending = false;
    } else {
        sfxtbl[sfx_index].pending = false;
        return -1;
    }
    return 0;
}

#ifdef USE_SFX_INIT_THREAD

static int hw_audio_sfx_init_thread(void *data)
{
    int i, res = 0;
    for (i = 0; (i < sfx_process.num) && sfx_process.working; ++i) {
        int sfxi;
        sfxi = sfx_process.tbl[i].sfxi;
        if (hw_audio_sfx_init_do(sfxi, sfx_process.tbl[i].data, sfx_process.tbl[i].len)) {
            sfx_process.error = 1;
            sfx_process.errori = sfxi;
            res = -1;
            break;
        }
    }
    for (; i < sfx_process.num; ++i) {
        int sfxi;
        sfxi = sfx_process.tbl[i].sfxi;
        sfxtbl[sfxi].pending = false;
    }
    lib_free(sfx_process.tbl);
    sfx_process.tbl = NULL;
    sfx_process.busy = false;
    sfx_process.working = false;
    sfx_process_thread = NULL;
    return res;
}

#endif /* USE_SFX_INIT_THREAD */

/* -------------------------------------------------------------------------- */

#ifdef USE_SFX_INIT_THREAD

int hw_audio_sfx_batch_start(int sfx_index_max)
{
    if ((!audio_initialized) || (!opt_sfx_init_parallel)) {
        return 0;
    }
    if (sfx_process.working) {
        while (sfx_process.busy) {
            SDL_Delay(100);
            hw_audio_check_process();
        }
    }
    sfx_process.t_start = hw_get_time_us();
    hw_audio_sfx_init_alloc(sfx_index_max);
    if (sfx_process.tbl) {
        lib_free(sfx_process.tbl);
        sfx_process.tbl = NULL;
    }
    sfx_process.tbl = lib_malloc(sfx_index_max * sizeof(struct sfx_process_sfx_s));
    sfx_process.num = 0;
    sfx_process.sfximax = sfx_index_max;
    sfx_process.error = 0;
    sfx_process.errori = 0;
    hw_audio_sfx_init_alloc(sfx_index_max);
    sfx_process.started = true;
    sfx_process.working = false;
    sfx_process.busy = false;
    return 1;
}

int hw_audio_sfx_batch_end(void)
{
    int res;
    if ((!audio_initialized) || (!opt_sfx_init_parallel)) {
        return 0;
    }
    sfx_process.busy = true;
    sfx_process.working = true;
    for (int i = 0; i < sfx_process.num; ++i) {
        sfxtbl[sfx_process.tbl[i].sfxi].pending = true;
    }
    sfx_process_thread = HWSDLX_CreateThread(hw_audio_sfx_init_thread);
    if (sfx_process_thread == NULL) {
        log_error("SDLA: Couldn't create thread: %s\n", SDL_GetError());
        hw_audio_sfx_init_thread(0);
        res = 0;
    } else {
        log_message("SDLA: created sfx processing thread\n");
        res = 1;
    }
    return res;
}

int hw_audio_sfx_init(int sfx_index, const uint8_t *data_in, uint32_t len_in)
{
    if (!audio_initialized) {
        return 0;
    }
    if (!sfx_process.started) {
        if (hw_audio_sfx_init_do(sfx_index, data_in, len_in)) {
            log_error("SDLA: failed to init sound %i\n", sfx_index);
            return -1;
        }
    } else {
        if (sfx_index >= sfx_num) {
            log_error("SDLA: BUG: sfx %i >= max %i\n", sfx_index, sfx_num);
            return -1;
        }
        for (int i = 0; i < sfx_process.num; ++i) {
            if (sfx_process.tbl[i].sfxi == sfx_index) {
                log_warning("SDLA: BUG: sfx %i already selected for init\n", sfx_index);
                return 0;
            }
        }
        if (sfxtbl[sfx_index].chunk) {
            hw_audio_sfx_release(sfx_index);
        }
        sfx_process.tbl[sfx_process.num].sfxi = sfx_index;
        sfx_process.tbl[sfx_process.num].data = data_in;
        sfx_process.tbl[sfx_process.num].len = len_in;
        ++sfx_process.num;
    }
    return 0;
}

int hw_audio_check_process(void)
{
    if (sfx_process.started) {
        if (!sfx_process.busy) {
            uint32_t t_end = hw_get_time_us();
            sfx_process.started = false;
            if (sfx_process.error) {
                log_error("SDLA: failed to init sound %i\n", sfx_process.errori);
                return -1;
            } else {
                log_message("SDLA: sfx processing took %i ms\n", (t_end - sfx_process.t_start) / 1000);
            }
        }
    }
    return 0;
}

#else /* !USE_SFX_INIT_THREAD */

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
    if (!audio_initialized) {
        return 0;
    }
    if (hw_audio_sfx_init_do(sfx_index, data_in, len_in)) {
        log_error("SDLA: failed to init sound %i\n", sfx_index);
        return -1;
    }
    return 0;
}

int hw_audio_check_process(void)
{
    return 0;
}

#endif /* USE_SFX_INIT_THREAD */

void hw_audio_sfx_release(int sfx_index)
{
    if (sfx_index < sfx_num) {
#ifdef USE_SFX_INIT_THREAD
        while (sfxtbl[sfx_index].pending) {
            SDL_Delay(10);
        }
#endif /* USE_SFX_INIT_THREAD */
        if (sfxtbl[sfx_index].chunk) {
            if (sfx_playing == sfx_index) {
                hw_audio_sfx_stop();
            }
            Mix_FreeChunk(sfxtbl[sfx_index].chunk);
            sfxtbl[sfx_index].chunk = NULL;
        }
    }
}

void hw_audio_sfx_play(int sfx_index)
{
    if (audio_initialized && opt_sfx_enabled && (sfx_index < sfx_num)) {
#ifdef USE_SFX_INIT_THREAD
        while (sfxtbl[sfx_index].pending) {
            SDL_Delay(10);
        }
#endif /* USE_SFX_INIT_THREAD */
        Mix_PlayChannel(0, sfxtbl[sfx_index].chunk, 0);
        sfx_playing = sfx_index;
    }
}

void hw_audio_sfx_stop(void)
{
    if (audio_initialized && Mix_Playing(0)) {
        Mix_HaltChannel(0);
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
        Mix_Volume(0, volume);
    }
    if (opt_sfx_volume != volume) {
        log_message("SDLA: sfx volume %i\n", volume);
        opt_sfx_volume = volume;
    }
    return true;
}

#else /* !HAVE_SDLMIXER */

int hw_audio_init(void)
{
    if (opt_audio_enabled) {
        log_warning("SDLA: no audio since this build was made with SDL_mixer explicitly disabled!\n");
    }
    return 0;
}

void hw_audio_shutdown_pre(void)
{
}
void hw_audio_shutdown(void)
{
}
int hw_audio_set_sdlmixer_sf(const char *path)
{
    return 0;
}
int hw_audio_music_init(int mus_index, const uint8_t *data, uint32_t len)
{
    return 0;
}
void hw_audio_music_release(int mus_index)
{
}
void hw_audio_music_play(int mus_index)
{
}
void hw_audio_music_fadeout(void)
{
}
void hw_audio_music_stop(void)
{
}
bool hw_audio_music_volume(int volume/*0..128*/)
{
    return true;
}
int hw_audio_sfx_batch_start(int sfx_index_max)
{
    return 0;
}
int hw_audio_sfx_batch_end(void)
{
    return 0;
}
int hw_audio_sfx_init(int sfx_index, const uint8_t *data, uint32_t len)
{
    return 0;
}
int hw_audio_check_process(void)
{
    return 0;
}
void hw_audio_sfx_release(int sfx_index)
{
}
void hw_audio_sfx_play(int sfx_index)
{
}
void hw_audio_sfx_stop(void)
{
}
bool hw_audio_sfx_volume(int volume/*0..128*/)
{
    return true;
}
#endif /* HAVE_SDLMIXER1 */
