/* syncmoo1_config.c -- per-user -home sandbox + shared MoO1 LBX data-path
 * resolution for the syncmoo1 Synchronet door.
 *
 * DESIGN.md §8 (per-user sandbox), §15 (asset caveat). Wired into hw_sbbs.c's
 * main() as:
 *   1. sm_door_setup()          -- resolve socket/time/alias/-home, splash.
 *   2. sm_door_sanitize_argv()  -- strip door args (incl. -home) from argv
 *                                   before 1oom's own options_parse() sees it.
 *   3. sm_config_apply()        -- THIS module: shared LBX data path, then
 *                                   the per-user chdir. Must run before
 *                                   sm_io_init()/main_1oom(), specifically
 *                                   before main_1oom() reaches
 *                                   lbxfile_find_dir() (1oom/src/main.c).
 *   4. sm_io_init(sm_door_socket())
 *   5. main_1oom(argc, argv)
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

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int sm_config_apply(void)
{
    const char *lbx  = getenv("SYNCMOO1_LBX");
    const char *home = sm_door_home();
    int         rc   = 0;

    /* --- 1. shared, read-only LBX data dir --------------------------------
     * Resolved and applied FIRST, before the per-user chdir below, and
     * absolutized via realpath() so it keeps resolving no matter what the
     * chdir changes cwd to next -- a relative SYNCMOO1_LBX must resolve
     * against the door's actual launch dir, not (whatever's left of it
     * after) the per-user sandbox. Unset/empty, or a path realpath() can't
     * resolve (e.g. it doesn't exist), leaves 1oom's own os_get_paths_data()
     * search completely alone -- no reimplementation of that search here. */
    if (lbx != NULL && lbx[0] != '\0') {
        char abs[PATH_MAX];

        if (realpath(lbx, abs) != NULL)
            os_set_path_data(abs);
        else
            fprintf(stderr, "syncmoo1: SYNCMOO1_LBX '%s' not found -- ignoring, "
                             "1oom will use its own default data-dir search\n", lbx);
    }

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
