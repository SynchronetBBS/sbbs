/* syncmoo1_config.c -- per-user -home sandbox + shared MoO1 LBX data-path
 * resolution for the syncmoo1 Synchronet door.
 *
 * DESIGN.md §8 (per-user sandbox), §15 (asset caveat). Wired into hw_sbbs.c's
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
                        * options_set_datadir() make) and os_set_path_user()
                        * (per-user config/save dir; 1oom's own -user PATH
                        * option, options.c, is nothing but this call) */
#include "ini_file.h"    /* xpdev: iniReadFile / iniGetBool / iniGetFloat */
#include "audio_mgr.h"   /* termgfx: TERMGFX_MUSIC_QUALITY_DEFAULT */

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
static double sm_music_quality = TERMGFX_MUSIC_QUALITY_DEFAULT;

int sm_config_wire_enabled(void)
{
    return sm_wire_enabled;
}

double sm_config_music_quality(void)
{
    return sm_music_quality;
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
    sm_music_quality = iniGetFloat(ini, "audio", "music_quality",
                                   TERMGFX_MUSIC_QUALITY_DEFAULT);
    strListFree(&ini);
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
     * sandbox: an absolute path (realpath()) keeps resolving afterwards.
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
     * A SYNCMOO1_LBX that realpath() can't resolve (e.g. it doesn't exist) is
     * reported and ignored, falling through to the launch-dir default. */
    sm_lbx_dir[0] = '\0';
    if (lbx != NULL && lbx[0] != '\0') {
        if (realpath(lbx, sm_lbx_dir) == NULL) {
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

    /* --- 2. per-user sandbox ------------------------------------------------
     * 1oom builds its config + save file paths by prefixing os_get_path_user()
     * (1oom/src/os/unix/os.c) -- an ABSOLUTE $XDG_CONFIG_HOME/1oom or
     * $HOME/.config/1oom, NOT a cwd-relative path: cfg_cfgname() (cfg.c) and
     * game_save_get_slot_fname()/year_fname() (game/game_save.c) all prefix
     * it. Nothing in this door sets HOME/XDG_CONFIG_HOME, so a bare chdir does
     * NOT isolate anything -- every node would still share the one default
     * dir. The actual isolation is os_set_path_user(): it stores into the same
     * user_path var os_get_path_user() lazily caches (os.c), and this runs
     * before main_1oom()'s os_early_init/os_init ever call os_get_path_user(),
     * so setting it here pre-populates that cache and wins.
     *
     * mkpath() FIRST (creates any missing components, e.g. a brand-new user's
     * nested data/user/0042/moo1/), so realpath() below can resolve it to an
     * absolute path -- the same absolutize-before-anything-moves discipline
     * step 1 uses, and so the value stays valid past the chdir. The chdir is
     * kept too (harmless, and keeps any stray cwd-relative engine I/O inside
     * the sandbox), but os_set_path_user() is what does the real isolation.
     *
     * No -home => user_path is left unset, so 1oom keeps its own default
     * XDG/HOME resolution (the pre-Task-8 shared-location behavior) -- a clean
     * no-op, matching sm_door_home()'s NULL-on-absence contract. */
    if (home != NULL) {
        char abs[PATH_MAX];

        if (mkpath(home) != 0) {
            fprintf(stderr, "syncmoo1: cannot create per-user home '%s'\n", home);
            rc = -1;   /* logged, not fatal -- see the doc comment in syncmoo1.h */
        }
        /* Absolutize for os_set_path_user() so the per-user config/save prefix
         * can't be broken by the chdir below (or by a relative -home value). */
        os_set_path_user(realpath(home, abs) != NULL ? abs : home);
        if (chdir(home) != 0) {
            fprintf(stderr, "syncmoo1: cannot enter per-user home '%s'\n", home);
            rc = -1;   /* logged, not fatal -- see the doc comment in syncmoo1.h */
        }
    }

    return rc;
}
