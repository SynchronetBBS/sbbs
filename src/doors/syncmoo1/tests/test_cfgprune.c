#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "syncmoo1_cfgprune.h"

static void expect_key(const char *line, const char *want)
{
    char k[64];
    if (want == NULL) {
        assert(sm_cfg_key(line, k, sizeof k) == 0);
        return;
    }
    assert(sm_cfg_key(line, k, sizeof k) == 1);
    assert(strcmp(k, want) == 0);
}

int main(void)
{
    char  *out = NULL;
    size_t n   = 0;

    /* --- key extraction --- */
    expect_key("opta.music = true", "opta.music");
    expect_key("  ui.ui_delay_enabled=false", "ui.ui_delay_enabled");
    expect_key("game.skipintro = true   ", "game.skipintro");
    expect_key("# a comment", NULL);
    expect_key("", NULL);
    expect_key("   ", NULL);
    expect_key("no_equals_here", NULL);

    /* --- prune: identical lines dropped, diverged lines kept --- */
    {
        const char *base =
            "# 1oom configuration file\n"
            "opta.music = true\n"
            "opta.music_volume = 64\n"
            "game.skipintro = true\n";
        const char *user =
            "# 1oom configuration file\n"
            "opta.music = false\n"       /* diverged -> keep */
            "opta.music_volume = 64\n"   /* same as base -> drop */
            "game.skipintro = true\n";   /* same as base -> drop */

        assert(sm_cfg_prune(base, user, NULL, &out, &n) == 0);
        assert(strstr(out, "opta.music = false") != NULL);
        assert(strstr(out, "opta.music_volume") == NULL);
        assert(strstr(out, "game.skipintro") == NULL);
        assert(strstr(out, "# 1oom configuration file") != NULL);   /* comments kept */
        free(out); out = NULL;
    }

    /* --- key present in user but absent from base is KEPT (conditional
     *     `opta` module: cfg.c:28 skips it when audio is off) --- */
    {
        const char *base = "game.skipintro = true\n";
        const char *user = "game.skipintro = true\nopta.sfx = false\n";

        assert(sm_cfg_prune(base, user, NULL, &out, &n) == 0);
        assert(strstr(out, "opta.sfx = false") != NULL);
        assert(strstr(out, "game.skipintro") == NULL);
        free(out); out = NULL;
    }

    /* --- malformed lines are never destroyed --- */
    {
        const char *base = "opta.music = true\n";
        const char *user = "opta.music = true\ngarbage line\n";

        assert(sm_cfg_prune(base, user, NULL, &out, &n) == 0);
        assert(strstr(out, "garbage line") != NULL);
        free(out); out = NULL;
    }

    /* --- idempotent: pruning an already-pruned file changes nothing --- */
    {
        const char *base = "opta.music = true\nopta.sfx = true\n";
        const char *user = "opta.music = false\n";
        char  *once = NULL, *twice = NULL;
        size_t n1 = 0, n2 = 0;

        assert(sm_cfg_prune(base, user, NULL, &once, &n1) == 0);
        assert(sm_cfg_prune(base, once, NULL, &twice, &n2) == 0);
        assert(strcmp(once, twice) == 0);
        free(once); free(twice);
    }

    /* --- whitespace variants around '=' compare by VALUE, not by bytes --- */
    {
        const char *base = "opta.music_volume = 64\n";
        const char *user = "opta.music_volume=64\n";

        assert(sm_cfg_prune(base, user, NULL, &out, &n) == 0);
        assert(strstr(out, "music_volume") == NULL);   /* same value -> dropped */
        free(out); out = NULL;
    }

    /* --- drop_keys: door-owned keys go regardless of value ---
     *
     * opt.data_path is why this exists. 1oom's cfg_save() persists it as the
     * ABSOLUTE data directory, so a sysop who moves the BBS tree would leave
     * every player's config pointing at a path that no longer exists. The door
     * re-resolves it every run, so it must never survive. */
    {
        static const char *const drop[] = { "opt.data_path", NULL };
        const char *base = "opt.data_path = \"/old/tree/xtrn/syncmoo1\"\n";
        const char *user =
            "opt.data_path = \"/sbbs/xtrn/syncmoo1\"\n"   /* diverged -> would normally be KEPT */
            "ui.uiscale = 2\n";                            /* diverged -> kept */

        assert(sm_cfg_prune(base, user, drop, &out, &n) == 0);
        assert(strstr(out, "data_path") == NULL);   /* dropped despite diverging */
        assert(strstr(out, "/sbbs/") == NULL);      /* no absolute path survives */
        assert(strstr(out, "ui.uiscale = 2") != NULL);
        free(out); out = NULL;
    }

    /* A dropped key goes even when it MATCHES the base (the ordinary rule would
     * have dropped it too -- assert we did not double-count and lose the rest). */
    {
        static const char *const drop[] = { "opt.data_path", NULL };
        const char *base = "opt.data_path = \"/x\"\nui.uiscale = 1\n";
        const char *user = "opt.data_path = \"/x\"\nui.uiscale = 1\n";

        assert(sm_cfg_prune(base, user, drop, &out, &n) == 0);
        assert(strstr(out, "data_path") == NULL);
        assert(strstr(out, "uiscale") == NULL);
        free(out); out = NULL;
    }

    /* A key NOT in drop_keys is unaffected by the list; NULL list = old behavior. */
    {
        static const char *const drop[] = { "opt.data_path", NULL };
        const char *base = "game.skipintro = true\n";
        const char *user = "game.skipintro = false\n";   /* diverged -> kept */

        assert(sm_cfg_prune(base, user, drop, &out, &n) == 0);
        assert(strstr(out, "game.skipintro = false") != NULL);
        free(out); out = NULL;

        assert(sm_cfg_prune(base, user, NULL, &out, &n) == 0);
        assert(strstr(out, "game.skipintro = false") != NULL);
        free(out); out = NULL;
    }

    return 0;
}
