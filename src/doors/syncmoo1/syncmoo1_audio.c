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
 * But 1oom's SFX items are LBXVOC, not bare VOC -- fmt_sfx.c's own detector
 * (fmt_sfx_detect(), fmt_sfx.c:406) checks for a 16-byte LBX sub-header
 * (HDRID_LBXVOC / HDR_LBXVOC_LEN, fmt_id.h) before the "Creative Voice File"
 * magic ever appears. termgfx_audio_voc_to_pcm() only recognises the bare
 * VOC magic at offset 0, so sm_audio_payload_offset() strips that 16-byte
 * wrapper (when present) before the bytes are hashed or queued, mirroring
 * fmt_sfx_convert()'s SFX_TYPE_LBXVOC case (fmt_sfx.c:427) without pulling in
 * any of fmt_sfx.c itself.
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

/* Big-endian 0xafde0200 at offset 0 (1oom/src/fmt_id.h HDRID_LBXVOC) plus the
 * 16-byte sub-header length it introduces (HDR_LBXVOC_LEN); source of truth
 * is fmt_sfx_detect()/fmt_sfx_convert() in 1oom/src/fmt_sfx.c:406-428. Kept as
 * a local byte pattern (not the engine's header) so this target links neither
 * fmt_sfx.c nor fmt_id.h. */
static const uint8_t SM_LBXVOC_MAGIC[4] = { 0xaf, 0xde, 0x02, 0x00 };
#define SM_LBXVOC_HDR_LEN  16

size_t sm_audio_payload_offset(const void *data, size_t len)
{
    if (data == NULL || len <= SM_LBXVOC_HDR_LEN)
        return 0;
    if (memcmp(data, SM_LBXVOC_MAGIC, sizeof SM_LBXVOC_MAGIC) != 0)
        return 0;
    return SM_LBXVOC_HDR_LEN;
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
    char   leaf[SM_LEAF_LEN];
    size_t off;

    if (!sm_audio_valid(index) || data == NULL || len == 0)
        return 0;   /* the engine already logs a bad index (uisound.c) */

    /* Strip the LBX sub-header (if present) BEFORE hashing/queueing: the hash
     * must be over the bytes we actually ship, and termgfx's VOC detector
     * only fires when "Creative Voice File" starts at offset 0. */
    off = sm_audio_payload_offset(data, len);
    data += off;
    len -= (uint32_t)off;

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
