#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hash.h"   /* termgfx: termgfx_fnv1a */
#include "syncmoo1_audio.h"
#include "syncmoo1_music.h"   /* sm_music_pump prototype, for the fake below */

/* --- fake termgfx audio manager -------------------------------------------
 * syncmoo1_audio.c references these four symbols, so the test must define them
 * (a NULL-guarded call is still a call site the linker resolves). Linking the
 * real audio_mgr.c would pull in audio.c + C++ libADLMIDI and cost this target
 * its dependency-free property. Recording the calls also lets us assert the
 * Store-once behaviour directly.
 *
 * sm_audio_pump() also now calls sm_music_pump() (Task 5): this target links
 * neither syncmoo1_music.c nor termgfx's music/render machinery (same
 * dependency-free reasoning), so a no-op fake stands in and just counts
 * calls -- test_audio doesn't exercise music, only the audio module's call
 * site. */
#define FAKE_MAX 64
static char fake_stored[FAKE_MAX][16];
static int  fake_stored_n;
static char fake_played[FAKE_MAX][16];
static int  fake_played_n;
static int  fake_stop_calls;
static int  fake_tier_value = 1;   /* pretend the terminal has digital audio */

int termgfx_audio_tier(const termgfx_audio_t *m)
{
    (void)m;
    return fake_tier_value;
}

void termgfx_audio_sfx_store(termgfx_audio_t *m, const char *leaf,
                             const void *filedata, size_t filelen)
{
    (void)m; (void)filedata; (void)filelen;
    if (fake_stored_n < FAKE_MAX)
        snprintf(fake_stored[fake_stored_n++], 16, "%s", leaf);
}

void termgfx_audio_sfx_play_named(termgfx_audio_t *m, const char *leaf,
                                  int vol, int pan)
{
    (void)m; (void)vol; (void)pan;
    if (fake_played_n < FAKE_MAX)
        snprintf(fake_played[fake_played_n++], 16, "%s", leaf);
}

void termgfx_audio_sfx_stop_all(termgfx_audio_t *m)
{
    (void)m;
    ++fake_stop_calls;
}

static int fake_music_pump_calls;

void sm_music_pump(void)
{
    ++fake_music_pump_calls;
}

int main(void)
{
    /* Canonical FNV-1a 32-bit vectors (offset basis 0x811c9dc5, prime 0x01000193). */
    assert(termgfx_fnv1a("", 0)          == 0x811c9dc5u);
    assert(termgfx_fnv1a("a", 1)         == 0xe40c292cu);
    assert(termgfx_fnv1a("foobar", 6)    == 0xbf9cf968u);

    /* Binary-safe: embedded NULs are hashed, not treated as terminators. */
    assert(termgfx_fnv1a("a\0b", 3) != termgfx_fnv1a("a\0c", 3));

    /* Same bytes -> same hash (the property content-addressing depends on). */
    {
        const char v[] = "Creative Voice File\x1a";
        assert(termgfx_fnv1a(v, sizeof v - 1) == termgfx_fnv1a(v, sizeof v - 1));
    }

    /* --- syncmoo1_audio: leaf naming ------------------------------------ */
    /* Cache leaf: a bare 8-hex content hash. The door prefix ("moo1") and the
     * kind ("sfx"/"music") are already path components in termgfx's cache key,
     * so a prefix letter here would only duplicate them. */
    {
        char leaf[16];
        const char bytes[] = "hello";

        sm_audio_leaf(bytes, sizeof bytes - 1, leaf);
        assert(strlen(leaf) == 8);
        assert(strchr(leaf, '_') == NULL);          /* no "s_" prefix */
        assert(strspn(leaf, "0123456789abcdef") == 8);

        /* Content-addressed: same bytes -> same leaf, different -> different. */
        {
            char a[16], b[16];
            sm_audio_leaf("one", 3, a);
            sm_audio_leaf("one", 3, b);
            assert(strcmp(a, b) == 0);
            sm_audio_leaf("two", 3, b);
            assert(strcmp(a, b) != 0);
        }
    }

    /* --- sm_audio_payload_offset(): strip the 16-byte LBXVOC sub-header --- */
    {
        uint8_t buf[64];
        size_t  i;

        /* LBX-wrapped: magic af de 02 00 followed by a VOC body, buffer
           longer than 16 bytes -> offset is 16. */
        memset(buf, 0, sizeof buf);
        buf[0] = 0xaf; buf[1] = 0xde; buf[2] = 0x02; buf[3] = 0x00;
        memcpy(buf + 16, "Creative Voice File", 20);
        assert(sm_audio_payload_offset(buf, 16 + 20) == 16);

        /* Bare VOC, no wrapper -> offset is 0. */
        memcpy(buf, "Creative Voice File", 20);
        assert(sm_audio_payload_offset(buf, 20) == 0);

        /* Wrapper magic present but buffer too short (<= 16 bytes): must
           never underflow len - 16, must return 0. */
        buf[0] = 0xaf; buf[1] = 0xde; buf[2] = 0x02; buf[3] = 0x00;
        for (i = 0; i <= 16; ++i)
            assert(sm_audio_payload_offset(buf, i) == 0);

        /* Arbitrary non-matching bytes -> 0. */
        memset(buf, 0x42, sizeof buf);
        assert(sm_audio_payload_offset(buf, sizeof buf) == 0);
    }

    /* --- volume map: 1oom's 0..128 -> termgfx's 0..100 ------------------- */
    assert(sm_audio_vol(0)   == 0);
    assert(sm_audio_vol(128) == 100);
    assert(sm_audio_vol(64)  == 50);
    assert(sm_audio_vol(-5)  == 0);     /* clamped */
    assert(sm_audio_vol(999) == 100);   /* clamped */

    /* --- slot table + dedupe (no terminal attached: all Stores no-op) ---- */
    {
        static const char a[] = "sample-A";
        static const char b[] = "sample-B";
        char leaf_a[16], leaf_b[16];

        sm_audio_attach(NULL);                 /* no manager -> silent, still tracks */
        sm_audio_leaf(a, sizeof a - 1, leaf_a);
        sm_audio_leaf(b, sizeof b - 1, leaf_b);

        assert(sm_audio_batch_start(41) == 0);
        assert(sm_audio_init(0, (const uint8_t *)a, sizeof a - 1) == 0);
        assert(sm_audio_init(7, (const uint8_t *)b, sizeof b - 1) == 0);
        assert(sm_audio_slot(0) != NULL);
        assert(strcmp(sm_audio_slot(0), leaf_a) == 0);
        assert(sm_audio_slot(7) != NULL);
        assert(strcmp(sm_audio_slot(7), leaf_b) == 0);
        assert(sm_audio_pending() == 2);

        /* Identical bytes at another index: same leaf, queued ONCE. */
        assert(sm_audio_init(9, (const uint8_t *)a, sizeof a - 1) == 0);
        assert(sm_audio_slot(9) != NULL);
        assert(strcmp(sm_audio_slot(9), leaf_a) == 0);
        assert(sm_audio_pending() == 2);

        /* batch_start is a CAPACITY HINT, not a reset: the intro calls it
           again after ui_late_init() registered the gameplay set. Clearing
           here would silence every gameplay sound.
           NULL check MUST come first: a cleared slot returns NULL, and
           sm_audio_slot(0) must be verified non-NULL before strcmp(), else
           dereferencing NULL is undefined behavior (segfault, not a failed
           assertion). */
        assert(sm_audio_batch_start(44) == 0);
        assert(sm_audio_slot(0) != NULL);
        assert(strcmp(sm_audio_slot(0), leaf_a) == 0);
        assert(sm_audio_pending() == 2);

        /* Out-of-range indices are no-ops, never writes. */
        assert(sm_audio_init(-1, (const uint8_t *)a, sizeof a - 1) == 0);
        assert(sm_audio_init(100000, (const uint8_t *)a, sizeof a - 1) == 0);
        assert(sm_audio_slot(-1) == NULL);
        assert(sm_audio_slot(100000) == NULL);

        /* release() clears the slot, not the client's cached sample. */
        sm_audio_release(7);
        assert(sm_audio_slot(7) == NULL);

        /* play()/stop() with no manager are no-ops (must not crash, and must
           not reach termgfx: a terminal with no audio APC stays silent). */
        sm_audio_play(0);
        sm_audio_stop();
        assert(fake_played_n == 0);
        assert(fake_stop_calls == 0);
    }

    /* --- pump: drains only once the tier is known, Stores each leaf once --- */
    {
        static const char c[] = "sample-C";
        termgfx_audio_t  *fake_mgr = (termgfx_audio_t *)0x1;   /* opaque; never dereferenced */
        char leaf_c[16];

        sm_audio_leaf(c, sizeof c - 1, leaf_c);

        /* Below tier 1 the queue must NOT drain: termgfx would drop the Store. */
        fake_tier_value = -1;
        sm_audio_attach(fake_mgr);
        assert(sm_audio_init(11, (const uint8_t *)c, sizeof c - 1) == 0);
        sm_audio_pump();
        assert(fake_stored_n == 0);
        assert(sm_audio_pending() > 0);
        /* sm_audio_pump()'s early return must not skip the music poll. */
        assert(fake_music_pump_calls == 1);

        /* Tier known -> drains. Each distinct leaf Stored exactly once, and the
           duplicate registered earlier at two indices contributes one Store. */
        fake_tier_value = 1;
        sm_audio_pump();
        assert(sm_audio_pending() == 0);
        {
            int i, j, dup = 0;
            for (i = 0; i < fake_stored_n; ++i)
                for (j = i + 1; j < fake_stored_n; ++j)
                    if (strcmp(fake_stored[i], fake_stored[j]) == 0)
                        ++dup;
            assert(dup == 0);   /* no leaf Stored twice */
        }

        /* A second pump is a cheap no-op, not a re-upload. */
        {
            int before = fake_stored_n;
            sm_audio_pump();
            assert(fake_stored_n == before);
        }

        /* play() dispatches the leaf registered at that index. */
        sm_audio_play(11);
        assert(fake_played_n == 1);
        assert(strcmp(fake_played[0], leaf_c) == 0);

        /* released index no longer plays. */
        sm_audio_release(11);
        sm_audio_play(11);
        assert(fake_played_n == 1);

        sm_audio_stop();
        assert(fake_stop_calls == 1);

        sm_audio_attach(NULL);   /* leave globals inert for any later test */
    }

    /* --- LBX-wrapped vs. bare VOC payload: same content -> same leaf ----- */
    {
        static const char voc[] = "Creative Voice FileXYZ";
        uint8_t lbx[16 + sizeof voc - 1];
        char    leaf_voc[16], leaf_lbx_index[16];

        memset(lbx, 0, sizeof lbx);
        lbx[0] = 0xaf; lbx[1] = 0xde; lbx[2] = 0x02; lbx[3] = 0x00;
        memcpy(lbx + 16, voc, sizeof voc - 1);

        sm_audio_leaf(voc, sizeof voc - 1, leaf_voc);

        assert(sm_audio_init(20, lbx, (uint32_t)sizeof lbx) == 0);
        assert(sm_audio_slot(20) != NULL);
        snprintf(leaf_lbx_index, sizeof leaf_lbx_index, "%s", sm_audio_slot(20));
        assert(strcmp(leaf_lbx_index, leaf_voc) == 0);

        assert(sm_audio_init(21, (const uint8_t *)voc, sizeof voc - 1) == 0);
        assert(sm_audio_slot(21) != NULL);
        assert(strcmp(sm_audio_slot(21), leaf_voc) == 0);

        /* Both indices resolved to the same content-addressed leaf, so they
           dedupe into exactly one pending queue entry. */
        assert(sm_audio_pending() == 1);
    }

    return 0;
}
