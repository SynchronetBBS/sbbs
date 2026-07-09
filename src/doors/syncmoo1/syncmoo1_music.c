/* syncmoo1_music.c -- see syncmoo1_music.h.
 *
 * Three things this module knows that its callers do not:
 *
 *   - 1oom hands hw_audio_music_init() an LBX-WRAPPED XMI, not MIDI. It must
 *     go through fmt_mus_convert_xmid(), which also reports whether the tune
 *     loops (the XMI carries a loop-start controller). MoO1's intro/ending
 *     stingers do NOT loop; termgfx used to loop everything.
 *
 *   - The cache leaf hashes the CONVERTED MIDI, not the source XMI. A change
 *     to fmt_mus_convert_xmid() then renames every track and invalidates both
 *     the client cache and the door-side OGG cache. Hashing the source would
 *     serve stale audio under an unchanged name; termgfx's own render-version
 *     tag cannot see 1oom's converter.
 *
 *   - The audio tier is UNKNOWN while the intro plays its title music (the
 *     capability probe reply lands ~50ms in). termgfx drops a play below tier
 *     1, so the request is stashed and replayed from sm_music_pump(). Same
 *     trap, same fix, as the SFX pending queue in syncmoo1_audio.c.
 */
#include "syncmoo1_music.h"
#include "syncmoo1_audio.h"   /* sm_audio_leaf, sm_audio_vol */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fmt_mus.h"   /* 1oom: fmt_mus_detect, fmt_mus_convert_xmid */
#include "lib.h"       /* 1oom: lib_free -- fmt_mus_convert_xmid allocates */
#include "options.h"   /* 1oom: opt_music_enabled, opt_music_volume */

#define SM_MUSIC_MAX  8    /* uisound.c uses slot 0; the contract is indexed */
#define SM_LEAF_LEN   16
#define SM_MUSIC_RATE 48000

struct sm_track {
    char     leaf[SM_LEAF_LEN];
    uint8_t *midi;
    size_t   len;
    int      loop;
};

static termgfx_audio_t *g_mgr;
static struct sm_track  g_track[SM_MUSIC_MAX];
static int              g_vol = 64;   /* 1oom's opt_music_volume default */
static int              g_pending = -1;   /* slot requested below tier 1 */
static int              g_playing = -1;   /* slot handed to termgfx */
static int              g_current = -1;   /* slot 1oom last asked us to play,
                                            * even if volume 0 silenced it --
                                            * lets a later volume raise resume
                                            * it. Cleared by an explicit stop
                                            * (the track is over), not by a
                                            * volume-0 mute. */
static bool             g_warned_not_xmid;   /* log the format refusal once */

static int sm_music_valid(int index)
{
    return index >= 0 && index < SM_MUSIC_MAX;
}

void sm_music_attach(termgfx_audio_t *m)
{
    g_mgr = m;
}

void sm_music_release(int index)
{
    if (!sm_music_valid(index))
        return;
    free(g_track[index].midi);
    g_track[index].midi = NULL;
    g_track[index].len  = 0;
    g_track[index].leaf[0] = '\0';
    if (g_playing == index)
        g_playing = -1;
    if (g_pending == index)
        g_pending = -1;
    if (g_current == index)
        g_current = -1;
}

int sm_music_init(int index, const uint8_t *data, uint32_t len)
{
    uint8_t *midi = NULL;
    uint32_t mlen = 0;
    bool     loops = false;

    if (!sm_music_valid(index) || data == NULL || len == 0)
        return 0;

    if (!opt_music_enabled)
        return 0;   /* music off: no hashing/conversion/upload work at all */

    if (fmt_mus_detect(data, len) != MUS_TYPE_LBXXMID) {
        /* MUSIC.LBX is XMI throughout, and 1oom's own SDL backend has no
         * plain-MIDI case either. Refuse rather than invent a passthrough. */
        if (!g_warned_not_xmid) {
            fprintf(stderr, "syncmoo1: music slot %d is not LBXXMID -- silent\n", index);
            g_warned_not_xmid = true;
        }
        return 0;
    }
    if (!fmt_mus_convert_xmid(data, len, &midi, &mlen, &loops) || midi == NULL)
        return 0;

    sm_music_release(index);   /* re-init of a live slot, as the SDL backend does */

    g_track[index].midi = (uint8_t *)malloc(mlen);
    if (g_track[index].midi == NULL) {
        lib_free(midi);
        return 0;
    }
    memcpy(g_track[index].midi, midi, mlen);
    g_track[index].len  = mlen;
    g_track[index].loop = loops ? 1 : 0;
    /* Hash the CONVERTED MIDI: the bytes that determine the audio. */
    sm_audio_leaf(g_track[index].midi, mlen, g_track[index].leaf);
    lib_free(midi);
    return 0;   /* 1oom ignores the return; 0 matches the SDL backend */
}

/* The volume termgfx should use right now, or 0 when music is off. */
static int sm_music_vol(void)
{
    if (!opt_music_enabled)
        return 0;
    return sm_audio_vol(g_vol);
}

static void sm_music_start(int index)
{
    struct sm_track *t = &g_track[index];
    int vol = sm_music_vol();

    if (vol == 0) {                       /* 0% = OFF: stop, never a silent upload */
        termgfx_audio_music_stop(g_mgr);
        g_playing = -1;
        return;
    }
    if (termgfx_audio_music_play(g_mgr, t->leaf, vol, t->loop)
        == TERMGFX_MUSIC_RENDER)
        termgfx_audio_music_async_submit(g_mgr, t->leaf, t->midi, t->len,
                                         SM_MUSIC_RATE, vol, t->loop);
    g_playing = index;
}

void sm_music_play(int index)
{
    if (g_mgr == NULL || !sm_music_valid(index) || g_track[index].leaf[0] == '\0')
        return;
    g_current = index;   /* the track 1oom last asked us to play, regardless
                           * of volume or tier -- lets a later volume raise
                           * off 0 resume it. */
    if (!opt_music_enabled) {             /* before any upload or render */
        termgfx_audio_music_stop(g_mgr);
        return;
    }
    if (termgfx_audio_tier(g_mgr) < 1) {
        g_pending = index;                /* replayed from sm_music_pump() */
        return;
    }
    sm_music_start(index);
}

void sm_music_stop(void)
{
    if (g_mgr == NULL)
        return;
    termgfx_audio_music_stop(g_mgr);      /* fades; serves fadeout() too */
    g_playing = -1;
    g_pending = -1;
    g_current = -1;   /* explicit stop: the track is over, not muted */
}

void sm_music_set_volume(int moo_vol)
{
    int was = g_vol;

    g_vol = moo_vol;
    if (g_mgr == NULL)
        return;
    if (sm_music_vol() == 0) {
        termgfx_audio_music_stop(g_mgr);
        g_playing = -1;
        return;                           /* g_current stays: a mute, not a stop */
    }
    if (sm_audio_vol(was) == 0 && g_playing < 0 && g_current >= 0)
        sm_music_play(g_current);         /* raised off 0: resume, through the
                                            * same tier gate sm_music_play() uses */
    else if (g_playing >= 0)
        termgfx_audio_music_volume(g_mgr, sm_music_vol());   /* live, no restart */
}

void sm_music_pump(void)
{
    if (g_mgr == NULL)
        return;
    termgfx_audio_music_async_poll(g_mgr);
    if (g_pending >= 0 && termgfx_audio_tier(g_mgr) >= 1) {
        int idx = g_pending;
        g_pending = -1;
        sm_music_start(idx);
    }
}

const char *sm_music_slot(int index)
{
    if (!sm_music_valid(index) || g_track[index].leaf[0] == '\0')
        return NULL;
    return g_track[index].leaf;
}
