/* syncmoo1_audio.c -- see syncmoo1_audio.h.
 *
 * Two facts from the engine shape this module, both verified in 1oom's source
 * rather than assumed:
 *
 *   - hw_audio_sfx_batch_start(max) is a CAPACITY HINT, not a reset. SDL's
 *     parallel backend only sizes a table with it (hwsdl_audio.c:433) and its
 *     serial backend ignores it entirely. ui_late_init() registers the 41
 *     gameplay sounds BEFORE the intro registers its three, so a backend that
 *     cleared here would silence the game the moment the intro ran.
 *
 *   - The audio tier is UNKNOWN while the engine registers sounds.
 *     ui_late_init() calls batch_start right after hw_video_init(), before the
 *     first present and so before the capability probe is even sent. Every
 *     termgfx Store is dropped below tier 1. So samples are copied into a
 *     pending queue here and drained from sm_audio_pump() once the tier reply
 *     lands -- inside the engine's own "Preparing sounds" pause.
 *
 * We never call fmt_sfx_convert(): termgfx's Store path already transcodes VOC.
 * The raw LBX bytes go straight through.
 */
#include "syncmoo1_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"   /* termgfx: termgfx_fnv1a */

/* 1oom's global SFX id space: NUM_SOUNDS (0x29) gameplay sounds, plus the
 * intro's three at NUM_SOUNDS+0..2 (uiintro.c). Sized generously and
 * range-checked rather than assuming the intro keeps using exactly three. */
#define SM_SFX_MAX      256
#define SM_LEAF_LEN     16
#define SM_PENDING_MAX  64

struct sm_pending {
    char     leaf[SM_LEAF_LEN];
    uint8_t *data;
    size_t   len;
};

static termgfx_audio_t   *g_mgr;
static char               g_slot[SM_SFX_MAX][SM_LEAF_LEN];
static struct sm_pending  g_pending[SM_PENDING_MAX];
static int                g_pending_n;
static int                g_vol = 128;   /* 1oom's default full scale */

void sm_audio_leaf(const void *data, size_t len, char out[16])
{
    snprintf(out, SM_LEAF_LEN, "s_%08x", termgfx_fnv1a(data, len));
}

int sm_audio_vol(int moo_vol)
{
    if (moo_vol <= 0)
        return 0;
    if (moo_vol >= 128)
        return 100;
    return (moo_vol * 100) / 128;
}

void sm_audio_attach(termgfx_audio_t *m)
{
    g_mgr = m;
}

static int sm_audio_valid(int index)
{
    return index >= 0 && index < SM_SFX_MAX;
}

int sm_audio_batch_start(int max)
{
    (void)max;   /* capacity hint only -- see the file header. Never clears. */
    return 0;    /* 0 = "prepared synchronously": our work is a hash, not a decode */
}

/* Queue `len` bytes under `leaf` unless that leaf is already queued. Returns 0
 * whether or not it queued: a full queue or a failed copy costs that one sample
 * its sound, never the session. */
static int sm_audio_queue(const char *leaf, const uint8_t *data, size_t len)
{
    int i;

    for (i = 0; i < g_pending_n; ++i) {
        if (strcmp(g_pending[i].leaf, leaf) == 0)
            return 0;   /* dedupe: same bytes registered at another index */
    }
    if (g_pending_n >= SM_PENDING_MAX)
        return 0;

    g_pending[g_pending_n].data = (uint8_t *)malloc(len);
    if (g_pending[g_pending_n].data == NULL)
        return 0;
    memcpy(g_pending[g_pending_n].data, data, len);
    g_pending[g_pending_n].len = len;
    snprintf(g_pending[g_pending_n].leaf, SM_LEAF_LEN, "%s", leaf);
    ++g_pending_n;
    return 0;
}

int sm_audio_init(int index, const uint8_t *data, uint32_t len)
{
    char leaf[SM_LEAF_LEN];

    if (!sm_audio_valid(index) || data == NULL || len == 0)
        return 0;   /* the engine already logs a bad index (uisound.c) */

    sm_audio_leaf(data, len, leaf);
    snprintf(g_slot[index], SM_LEAF_LEN, "%s", leaf);

    /* Copy the bytes: 1oom frees the intro's items after use, and the whole
     * set is ~440 KB -- a tenth of one sixel frame's working memory. */
    return sm_audio_queue(leaf, data, len);
}

void sm_audio_pump(void)
{
    int i;

    if (g_pending_n == 0 || g_mgr == NULL || termgfx_audio_tier(g_mgr) < 1)
        return;

    for (i = 0; i < g_pending_n; ++i) {
        termgfx_audio_sfx_store(g_mgr, g_pending[i].leaf,
                                g_pending[i].data, g_pending[i].len);
        free(g_pending[i].data);
        g_pending[i].data = NULL;
    }
    g_pending_n = 0;
}

void sm_audio_play(int index)
{
    if (g_mgr == NULL || !sm_audio_valid(index) || g_slot[index][0] == '\0')
        return;
    termgfx_audio_sfx_play_named(g_mgr, g_slot[index], sm_audio_vol(g_vol), 0);
}

void sm_audio_stop(void)
{
    if (g_mgr != NULL)
        termgfx_audio_sfx_stop_all(g_mgr);
}

void sm_audio_release(int index)
{
    if (sm_audio_valid(index))
        g_slot[index][0] = '\0';   /* the client keeps the cached sample */
}

void sm_audio_set_volume(int moo_vol)
{
    g_vol = moo_vol;
}

const char *sm_audio_slot(int index)
{
    if (!sm_audio_valid(index) || g_slot[index][0] == '\0')
        return NULL;
    return g_slot[index];
}

int sm_audio_pending(void)
{
    return g_pending_n;
}
