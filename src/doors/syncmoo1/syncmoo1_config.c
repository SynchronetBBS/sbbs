/* syncmoo1_config.c -- per-user -home sandbox + shared MoO1 LBX data-path
 * resolution for the syncmoo1 Synchronet door.
 *
 * DESIGN.md §8 (per-user sandbox), §15 (asset caveat). Wired into hw_term.c's
 * main() as:
 *   1. sm_door_setup()          -- resolve socket/time/alias/-home, splash.
 *   2. sm_door_sanitize_argv()  -- strip door args (incl. -home) from argv
 *                                   before 1oom's own options_parse() sees it.
 *   3. sm_config_apply()        -- THIS module: resolve the shared LBX data
 *                                   path, then do the per-user chdir. Must run
 *                                   before that chdir moves cwd, hence before
 *                                   sm_io_init()/main_1oom().
 *   4. sm_io_init(sm_door_socket())
 *   5. main_1oom(argc, argv)
 *        -> main_init() -> hw_init() -> sm_config_apply_data_path()
 *           -- THIS module too: the resolved data path is handed to 1oom from
 *              there, not from step 3. main_1oom() clobbers it in between.
 *
 * -home's value does NOT come from argv here -- sm_door_sanitize_argv() (step
 * 2) has already stripped it by the time this runs. It was captured earlier,
 * in sm_door_resolve() (the same argv pass that resolves -s<fd>/-name),
 * stashed in syncmoo1_door.c, and read back via sm_door_home() (syncmoo1.h).
 *
 * The shared LBX dir is resolved from the SYNCMOO1_LBX env var only -- no new
 * "-home"-style command-line flag was added for it. This keeps the change
 * minimal: no new argv contract to sanitize/strip/capture, and 1oom already
 * has its own native "-data <dir>" option (options.c's options_set_datadir(),
 * which is nothing more than os_set_path_data(argv[1])) for anyone who wants
 * to pass it straight through on the door's command line instead.
 */
#include "syncmoo1_config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dirwrap.h"   /* xpdev: mkpath() -- recursive mkdir */
#include "os.h"        /* 1oom: os_set_path_data() (shared LBX dir; same call
                        * lbxfile_find_dir()/lbx.c + options.c's
                        * options_set_datadir() make), os_set_path_user() (per-
                        * user config/save dir; 1oom's own -user PATH option,
                        * options.c, is nothing but this call), and
                        * os_get_path_user() (sm_config_seed_1oom() below) */
#include "cfg.h"       /* 1oom: cfg_load / cfg_save / cfg_cfgname */
#include "lib.h"       /* 1oom: lib_free -- cfg_cfgname()'s buffer is
                        * lib_malloc'd (util_concat()); free it accordingly */
#include "ini_file.h"    /* xpdev: iniReadFile / iniGetBool / iniGetFloat */
#include "audio_mgr.h"   /* termgfx: TERMGFX_MUSIC_QUALITY_DEFAULT */
#include "syncmoo1_cfgprune.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* The shared LBX data dir, resolved to an absolute path by sm_config_apply()
 * and applied later by sm_config_apply_data_path(). Empty => leave 1oom's own
 * data-path handling entirely alone. See both functions for why the resolve
 * and the apply have to happen at different points in the startup sequence. */
static char sm_lbx_dir[PATH_MAX];

/* syncmoo1.ini-backed config, resolved by sm_config_read_ini() below. */
static int    sm_wire_enabled;
static int    sm_hand_cursor;   /* draw 1oom's own hand cursor? default off -- the
                                 * terminal already shows a mouse pointer, and the
                                 * game's hand doesn't line up with it (see hw_term.c) */
static double sm_music_quality = TERMGFX_MUSIC_QUALITY_DEFAULT;

int sm_config_wire_enabled(void)
{
    return sm_wire_enabled;
}

int sm_config_hand_cursor(void)
{
    return sm_hand_cursor;
}

double sm_config_music_quality(void)
{
    return sm_music_quality;
}

/* The [1oom] section, flattened to "module.item = value\n" lines, captured
 * BEFORE the chdir (syncmoo1.ini lives in the launch dir, and by the time
 * sm_config_seed_1oom() runs, cwd is the per-user home). NULL = no section. */
static char *sm_seed_text;

/* Flatten ini's [1oom] section into sm_seed_text, one "key = value" line per
 * entry, verbatim (the keys are literal 1oom cfg names, module.item). Must
 * run while `ini` is still open, i.e. from inside sm_config_read_ini(). */
static void sm_config_capture_1oom(str_list_t ini)
{
    str_list_t keys = iniGetKeyList(ini, "1oom");
    size_t     cap = 0, used = 0, i;

    if (keys == NULL)
        return;
    for (i = 0; keys[i] != NULL; ++i)
        cap += strlen(keys[i]) + INI_MAX_VALUE_LEN + 8;
    if (cap == 0) {
        iniFreeStringList(keys);
        return;
    }
    sm_seed_text = (char *)malloc(cap);
    if (sm_seed_text == NULL) {
        iniFreeStringList(keys);
        return;
    }
    sm_seed_text[0] = '\0';
    for (i = 0; keys[i] != NULL; ++i) {
        char v[INI_MAX_VALUE_LEN];

        iniGetString(ini, "1oom", keys[i], "", v);
        used += (size_t)snprintf(sm_seed_text + used, cap - used,
                                 "%s = %s\n", keys[i], v);
        if (used >= cap)          /* truncated: refuse a half-written seed */
            break;
    }
    iniFreeStringList(keys);
}

/* Read syncmoo1.ini from the LAUNCH directory -- cwd is still the door's
 * startup_dir here, and sm_config_apply()'s chdir into -home has not happened
 * yet. Same relative-fopen approach as syncduke_config.c. A missing ini is not
 * an error: every key has a default. */
static void sm_config_read_ini(void)
{
    FILE *f = fopen("syncmoo1.ini", "r");
    str_list_t ini;

    if (f == NULL)
        return;
    ini = iniReadFile(f);
    fclose(f);
    if (ini == NULL)
        return;
    sm_wire_enabled  = iniGetBool(ini, "debug", "wire", FALSE) ? 1 : 0;
    sm_hand_cursor   = iniGetBool(ini, "video", "hand_cursor", FALSE) ? 1 : 0;
    sm_music_quality = iniGetFloat(ini, "audio", "music_quality",
                                   TERMGFX_MUSIC_QUALITY_DEFAULT);
    sm_config_capture_1oom(ini);
    strListFree(&ini);
}

/* The base snapshot lives in MEMORY, not on disk: its temp file is written,
 * read back, and deleted inside sm_config_seed_1oom(), so no exit path -- not
 * even sm_door_hangup()'s _exit(), which skips atexit() -- can leave it behind.
 * NULL = no base taken, so the prune is skipped rather than guessing. */
static char *sm_base_text;
static char sm_user_cfg[PATH_MAX];   /* user's own cfg path; "" = none taken */

/* Slurp `path` into a malloc'd NUL-terminated buffer, or NULL. */
static char *sm_slurp(const char *path)
{
    FILE  *f = fopen(path, "rb");
    char  *buf;
    long   n;

    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0
        || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = (char *)malloc((size_t)n + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[n] = '\0';
    fclose(f);
    return buf;
}

void sm_config_seed_1oom(void)
{
    FILE       *f;
    char        tmp[PATH_MAX];
    const char *user = os_get_path_user();

    /* sm_config_apply() (syncmoo1.h step 2) always calls os_set_path_user()
     * before this runs -- to -home when given, otherwise to cwd -- so `user`
     * is never NULL/"" in practice (os_set_path_user()'s lib_stralloc() would
     * itself abort the process on a NULL/OOM string, see 1oom/src/lib.c).
     * This guard is just defense against that invariant somehow not holding
     * (e.g. a future caller of this function that skips sm_config_apply()):
     * without a real user path there is nowhere to put the temps and nothing
     * to prune, so bail rather than seed/snapshot against a path we didn't
     * choose.
     *
     * Residual limitation: without -home, every session on this door shares
     * one cwd, hence one user path -- so they also share
     * 1oom_config_*.txt and these very temp-file names (syncmoo1_seed.tmp,
     * syncmoo1_base.tmp), and concurrent sessions can race each other. That
     * is inherent to not having per-user storage; it is exactly why -home
     * exists and why the live xtrn.ini passes it, not something to work
     * around here. */
    if (user == NULL || user[0] == '\0')
        return;

    /* 1. [1oom] -> a temp cfg file -> 1oom's own cfg_load(). Reusing the
     *    engine's parser means its per-item range checks validate the sysop's
     *    values, and every option it has (now or later) is settable with no
     *    code here. */
    if (sm_seed_text != NULL) {
        snprintf(tmp, sizeof tmp, "%s/syncmoo1_seed.tmp", user);
        f = fopen(tmp, "w");
        if (f != NULL) {
            fputs(sm_seed_text, f);
            fclose(f);
            cfg_load(tmp);       /* 1oom validates + assigns */
            remove(tmp);
        }
        free(sm_seed_text);
        sm_seed_text = NULL;
    }

    /* 2. Snapshot every option at its sysop-resolved value. cfg_save() rather
     *    than an enumeration, so the base needs no knowledge of the item
     *    table -- and stays correct as 1oom gains options.
     *
     *    cfg_save() only writes files, so the snapshot has to touch the disk.
     *    Read it straight back into memory and delete it HERE, in the same
     *    breath: the prune that consumes it runs from atexit(), and a player
     *    who drops carrier exits through sm_door_hangup()'s _exit(), which
     *    skips atexit entirely. A temp deleted at prune time would therefore
     *    survive every disconnected session -- the common way a BBS door ends
     *    -- and litter each player's directory with a ~3KB file. Holding the
     *    text costs nothing next to a 44KB sixel frame, and it also removes
     *    any window in which the base could go stale or be clobbered. */
    snprintf(tmp, sizeof tmp, "%s/syncmoo1_base.tmp", user);
    if (cfg_save(tmp) == 0)
        sm_base_text = sm_slurp(tmp);   /* NULL on failure -> prune is skipped */
    remove(tmp);                        /* cfg_save() can create then fail; either
                                         * way nothing survives this function */

    /* 3. Cache the user's own cfg path NOW, while os_get_path_user() still
     *    resolves to our sandbox. atexit() is LIFO: hw_term.c's main()
     *    registers sm_config_prune_user_cfg() BEFORE calling main_1oom(),
     *    which itself registers atexit(main_shutdown) (main.c). So at actual
     *    process exit, main_shutdown() (the LATER registration) fires FIRST
     *    -- running options_shutdown()'s cfg_save() while the sandbox is
     *    still in effect -- and only THEN sm_config_prune_user_cfg(). By that
     *    point main_shutdown()'s own os_shutdown() call has already
     *    lib_free()'d and NULLed 1oom's cached user_path (os/unix/os.c), so a
     *    fresh cfg_cfgname() call from inside the prune function would
     *    re-derive a path from $HOME/$XDG_CONFIG_HOME instead of the sandbox
     *    -- silently pruning (and even creating) the wrong file. Grab the
     *    path here instead, while it is still valid, and never call
     *    cfg_cfgname() again after shutdown. */
    {
        char *cfgname = cfg_cfgname();

        if (cfgname != NULL) {
            snprintf(sm_user_cfg, sizeof sm_user_cfg, "%s", cfgname);
            lib_free(cfgname);
        } else {
            sm_user_cfg[0] = '\0';
        }
    }
}

/* Keys the DOOR owns and re-resolves on every run, so persisting them per user
 * is at best noise and at worst a trap. opt.data_path is the trap: 1oom's
 * cfg_save() writes it as the ABSOLUTE directory we handed it from hw_init(),
 * so a sysop who moves the BBS tree leaves every player's config pointing at a
 * path that no longer exists. We resolve the data dir from SYNCMOO1_LBX (or the
 * launch dir) on every run and apply it AFTER cfg_load() precisely so a saved
 * value can never win -- there is nothing for it to do but rot. Drop it. */
static const char *const sm_prune_always[] = {
    "opt.data_path",
    NULL
};

void sm_config_prune_user_cfg(void)
{
    char  *user, *pruned = NULL;
    size_t n = 0;
    FILE  *f;

    if (sm_base_text == NULL || sm_user_cfg[0] == '\0')
        return;

    /* Use the path cached by sm_config_seed_1oom(), NOT a fresh cfg_cfgname()
     * call -- by the time this runs (atexit-registered, firing after
     * main_shutdown()'s cfg_save()), 1oom's os_shutdown() has already cleared
     * its cached user_path, and a second cfg_cfgname() would silently re-
     * derive a $HOME/XDG path instead of the sandbox (see the comment in
     * sm_config_seed_1oom()). The base itself was slurped there too, and its
     * temp file deleted; nothing on disk has to survive until now. */
    user = sm_slurp(sm_user_cfg);

    if (user != NULL
        && sm_cfg_prune(sm_base_text, user, sm_prune_always, &pruned, &n) == 0) {
        f = fopen(sm_user_cfg, "wb");
        if (f != NULL) {
            fwrite(pruned, 1, n, f);   /* short write -> file as-is next run */
            fclose(f);
        }
    }
    free(pruned);
    free(user);
    free(sm_base_text);
    sm_base_text = NULL;
    sm_user_cfg[0] = '\0';
}

void sm_config_apply_data_path(void)
{
    if (sm_lbx_dir[0] != '\0')
        os_set_path_data(sm_lbx_dir);
}

int sm_config_apply(void)
{
    const char *lbx  = getenv("SYNCMOO1_LBX");
    const char *home = sm_door_home();
    int         rc   = 0;
    char        cwd[PATH_MAX];

    sm_config_read_ini();   /* before the chdir: cwd is still the launch dir */

    /* --- 1. shared, read-only LBX data dir --------------------------------
     * RESOLVED here (before the step-2 chdir), APPLIED later, from hw_init()
     * via sm_config_apply_data_path().
     *
     * Resolve-now, because both candidates below are relative to the launch
     * directory, and step 2's chdir is about to move cwd into the per-user
     * sandbox: an absolute path (FULLPATH()) keeps resolving afterwards.
     *
     * Apply-later, because os_set_path_data() called from here would not
     * survive main_1oom(): its options_parse_early() runs cfg_load(), whose
     * "data_path" cfg item (options.c's opt_cfg_set_datapath) calls
     * os_set_path_data() with whatever the user's 1oom config file remembers
     * from a previous run -- overwriting ours before lbxfile_find_dir() ever
     * looks. That stale value can name a directory that no longer exists (or
     * a bare "."), which then silently defeats both SYNCMOO1_LBX and the
     * launch-dir default. hw_init() is the seam that fixes it: main_1oom()
     * calls it (via main_init()) AFTER options_parse_early()/cfg_load and
     * BEFORE options_parse(), which is where 1oom's own "-data" lands (it is
     * a late option -- cmdline_options_lbx, not cmdline_options_early). So
     * the resulting precedence is:
     *
     *     -data  >  SYNCMOO1_LBX  >  launch dir  >  1oom's own search
     *
     * with the remembered cfg "data_path" demoted to what it always was: a
     * cache of a previous search, not a configuration input.
     *
     * A SYNCMOO1_LBX that doesn't name an existing directory is reported and
     * ignored, falling through to the launch-dir default.
     *
     * isdir() + FULLPATH() (both xpdev) rather than POSIX realpath(), which
     * MSVC does not have. realpath() rolled the existence check and the
     * absolutize into one call; FULLPATH() canonicalizes lexically and does not
     * touch the filesystem, so the isdir() guard is what preserves the
     * "not found -- ignoring" behavior. It also tightens it: realpath()
     * accepted a SYNCMOO1_LBX naming a regular FILE, which 1oom would then have
     * searched as a data directory. */
    sm_lbx_dir[0] = '\0';
    if (lbx != NULL && lbx[0] != '\0') {
        if (!isdir(lbx) || FULLPATH(sm_lbx_dir, lbx, sizeof sm_lbx_dir) == NULL) {
            sm_lbx_dir[0] = '\0';
            fprintf(stderr, "syncmoo1: SYNCMOO1_LBX '%s' not found -- ignoring, "
                             "1oom will use its own default data-dir search\n", lbx);
        }
    }

    /* No usable SYNCMOO1_LBX: fall back to the door's launch directory, so LBX
     * files placed beside the door binary are found even after the step-2
     * sandbox chdir. When the program's SCFG start-up directory is set,
     * xtrn.cpp's external() chdir()s the forked child there before exec, so
     * that IS our cwd here -- conventionally the door's own dir, e.g.
     * xtrn/syncmoo1, the natural place for a sysop to drop the MoO1 data (and
     * what this door's install-xtrn.ini arranges, by leaving start-up dir
     * unset for install-xtrn.js to default to the .ini's own dir). With no
     * start-up dir configured, a NATIVE door gets no chdir at all and simply
     * inherits the sbbs process's cwd, i.e. ctrl_dir (main.cpp, sbbscon.c) --
     * xtrn.cpp's other chdir(), into cfg.node_dir, is #ifdef __FreeBSD__ AND
     * non-native, so it can never apply to this door.
     *
     * 1oom's own search does list '.' and './data', but only as evaluated
     * AFTER the step-2 chdir has pointed them at the sandbox, so the real
     * launch dir has to be captured here and registered explicitly. Harmless
     * wherever it lands if that dir holds no fonts.lbx: lbxfile_find_dir()
     * (lbx.c) just falls through to the next candidate os_get_paths_data()
     * offers (XDG dirs, /usr/share/1oom, ...). */
    if (sm_lbx_dir[0] == '\0' && getcwd(cwd, sizeof cwd) != NULL)
        snprintf(sm_lbx_dir, sizeof sm_lbx_dir, "%s", cwd);

    /* --- 2. per-user sandbox, and the $HOME guarantee ----------------------
     * 1oom builds its config + save file paths by prefixing os_get_path_user()
     * (1oom/src/os/unix/os.c) -- an ABSOLUTE path, NOT a cwd-relative one:
     * cfg_cfgname() (cfg.c) and game_save_get_slot_fname()/year_fname()
     * (game/game_save.c) all prefix it. Nothing in this door sets
     * HOME/XDG_CONFIG_HOME, so a bare chdir does NOT isolate anything -- every
     * node would still share the one default dir. The actual isolation is
     * os_set_path_user(): it stores into the same user_path var
     * os_get_path_user() lazily caches (os.c).
     *
     * GUARANTEE: this door never writes anything under $HOME. Left alone,
     * os_get_path_user() resolves to $XDG_CONFIG_HOME/1oom, else
     * $HOME/.config/1oom, else "." -- and caches whichever into that static
     * on its FIRST call. So os_set_path_user() is called HERE,
     * UNCONDITIONALLY, in every branch below, before anything else in this
     * process (including main_1oom()'s own os_early_init/os_init) can call
     * os_get_path_user() and let that fallback run: to the absolute -home
     * directory when one was given, otherwise to the door's absolute current
     * working directory. 1oom's own $HOME/XDG fallback never gets a chance to
     * fire.
     *
     * mkpath() FIRST (creates any missing components, e.g. a brand-new user's
     * nested data/user/0042/moo1/), so FULLPATH() below has something to
     * resolve against -- the same absolutize-before-anything-moves discipline
     * step 1 uses, and so the value stays valid past the chdir. The chdir is
     * kept too (harmless, and keeps any stray cwd-relative engine I/O inside
     * the sandbox), but os_set_path_user() is what does the real isolation. */
    if (home != NULL) {
        char abs[PATH_MAX];

        if (mkpath(home) != 0) {
            fprintf(stderr, "syncmoo1: cannot create per-user home '%s'\n", home);
            rc = -1;   /* logged, not fatal -- see the doc comment in syncmoo1.h */
        }
        /* Absolutize for os_set_path_user() so the per-user config/save prefix
         * can't be broken by the chdir below (or by a relative -home value).
         *
         * xpdev's FULLPATH(), not POSIX realpath(): MSVC has no realpath, and
         * this door already links xpdev. Note the argument order is _fullpath's
         * (target, path, size), the reverse of realpath's. It canonicalizes
         * lexically rather than resolving symlinks -- which is all that's
         * wanted here (an absolute path that survives the chdir), and unlike
         * realpath it doesn't fail when the directory couldn't be created. */
        os_set_path_user(FULLPATH(abs, home, sizeof abs) != NULL ? abs : home);
        if (chdir(home) != 0) {
            fprintf(stderr, "syncmoo1: cannot enter per-user home '%s'\n", home);
            rc = -1;   /* logged, not fatal -- see the doc comment in syncmoo1.h */
        }
    } else {
        /* No -home: storage goes in cwd instead (never $HOME/XDG). No chdir
         * needed here -- cwd is already where we are, and nothing above this
         * point moves it (the step-1 LBX resolve only reads cwd). */
        if (getcwd(cwd, sizeof cwd) != NULL) {
            os_set_path_user(cwd);
        } else {
            /* Can't even name cwd. Fall back to the literal string "." --
             * still not $HOME/XDG, and safe here specifically because this
             * branch never chdirs, so cwd cannot move out from under it. */
            fprintf(stderr, "syncmoo1: cannot resolve cwd for per-user config "
                             "path -- falling back to '.'\n");
            os_set_path_user(".");
            rc = -1;   /* logged, not fatal -- see the doc comment in syncmoo1.h */
        }
    }

    sm_config_seed_1oom();   /* needs os_set_path_user(); must precede main_1oom() */
    return rc;
}
