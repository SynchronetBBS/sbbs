/* syncmoo1_os_win32.c -- the door's 1oom `os` backend for Windows.
 *
 * 1oom's os/ is a pluggable backend directory: os/{unix,win32,msdos}/ each
 * supply an os.c + osdefs.h implementing the contract in 1oom/src/os.h. This
 * file is a fourth such backend, living in OUR tree -- exactly as hw_sbbs.c is
 * our own `hw` backend, a peer of the vendored hw/{sdl,alleg,nop}. It replaces
 * 1oom/src/os/win32/os.c for this door; the vendored osdefs.h from that
 * directory is still used (it is only FSDEV_* path-separator macros).
 *
 * WHY we don't just compile the vendored win32 os.c (CLAUDE.md: a need to
 * change 1oom/ is a signal the door glue is missing something):
 *
 *   Its os_make_path() is
 *       if (path == NULL || (path[0]=='.' && path[1]=='\0')) return 0;
 *       return mkdir(path);
 *   -- with no "already exists" test, unlike the unix backend, which guards
 *   with access(path, F_OK). So mkdir() returns EEXIST for any directory that
 *   is already there, os_make_path() reports failure, and every caller that
 *   asks it to ensure a path bails out:
 *
 *       cfg.c:219,241        cfg_save()/cfg_clear()  -> settings never persist
 *       game/game_save.c:882 game_save_do()          -> SAVING A GAME FAILS
 *       game/game_save.c:1064,1086                   -> save-slot enumeration
 *
 *   The door would run and draw but silently lose every save and every option
 *   change. (Upstream doesn't hit this because its Windows builds are MinGW +
 *   SDL and the save path is exercised from a different working directory
 *   layout; the bug is real either way, and belongs upstream.) Also, mkdir()
 *   there is single-level, so a brand-new per-user sandbox like
 *   data/user/0042/moo1/ -- several missing components deep -- could not be
 *   created even once.
 *
 * Both are fixed here by delegating to xpdev's mkpath(), which is recursive,
 * returns 0 when the directory already exists, and understands drive letters
 * and either separator. The rest of the contract mirrors the vendored backend.
 *
 * Only the Windows build uses this; *nix keeps 1oom's own os/unix/os.c, whose
 * os_make_path() is already correct. See CMakeLists.txt.
 */
#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "os.h"
#include "options.h"
#include "lib.h"
#include "util.h"

#include "dirwrap.h"   /* xpdev: mkpath() -- recursive, idempotent mkdir */

/* -------------------------------------------------------------------------- */

const struct cmdline_options_s os_cmdline_options[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

/* -------------------------------------------------------------------------- */

static char *data_path = NULL;
static char *user_path = NULL;
static char *all_data_paths[] = { NULL, NULL, NULL, NULL };

/* -------------------------------------------------------------------------- */

/* Unchanged from the vendored win32 backend: 1oom bakes this into the config
 * file name (cfg.c's cfg_cfgname -> "1oom_config_<main>_<ui>_<hw>.txt" uses the
 * hw id, but other paths key off the os id), so it is not ours to rename. */
const char *idstr_os = "win32";

int os_early_init(void)
{
    return 0;
}

int os_init(void)
{
    return 0;
}

void os_shutdown(void)
{
    lib_free(data_path);
    data_path = NULL;
    lib_free(user_path);
    user_path = NULL;
}

const char **os_get_paths_data(void)
{
    int i = 0;

    if (data_path) {
        all_data_paths[i++] = data_path;
    }
    all_data_paths[i++] = ".";
    all_data_paths[i++] = ".\\data";
    all_data_paths[i] = NULL;
    return (const char **)all_data_paths;
}

const char *os_get_path_data(void)
{
    return data_path;
}

void os_set_path_data(const char *path)
{
    /* Free first: the vendored win32 backend leaks here on a second call (the
     * unix one does not). syncmoo1_config.c calls this once, but 1oom's own
     * "data_path" cfg item can call it again on cfg_load(). */
    if (data_path) {
        lib_free(data_path);
        data_path = NULL;
    }
    data_path = lib_stralloc(path);
}

const char *os_get_path_user(void)
{
    if (user_path == NULL) {
        user_path = lib_stralloc(".");
    }
    return user_path;
}

void os_set_path_user(const char *path)
{
    if (user_path) {
        lib_free(user_path);
        user_path = NULL;
    }
    user_path = lib_stralloc(path);
}

/* 0 = the directory exists (or was created); non-zero = it does not.
 *
 * mkpath() is xpdev's: recursive, and a no-op returning 0 when the path is
 * already a directory. That "already exists is success" semantic is the whole
 * point of this backend -- see the file header. */
int os_make_path(const char *path)
{
    if ((path == NULL) || ((path[0] == '.') && (path[1] == '\0'))) {
        return 0;
    }
    return mkpath(path);
}

int os_make_path_user(void)
{
    return os_make_path(os_get_path_user());
}

int os_make_path_for(const char *filename)
{
    int   res = 0;
    char *path;

    /* util_fname_split() splits on osdefs.h's FSDEV_DIR_SEP_CHR ('\\') and, on
     * win32, FSDEV_DIR_SEP_ALT ('/') too -- so a forward-slash path built
     * elsewhere in the door still yields the right directory here. `path` is
     * NULL when `filename` has no directory component (nothing to create). */
    util_fname_split(filename, &path, NULL);
    if (path != NULL) {
        res = os_make_path(path);
        lib_free(path);
    }
    return res;
}

/* NULL: no OS-specific override, so 1oom composes its own default names
 * (cfg.c's cfg_cfgname(), game_save.c's slot/year names) from the user path.
 * Matches both vendored backends. */
const char *os_get_fname_save_slot(char *buf, size_t bufsize, int savei/*1..9*/)
{
    return NULL;
}

const char *os_get_fname_save_year(char *buf, size_t bufsize, int year/*2300..*/)
{
    return NULL;
}

const char *os_get_fname_cfg(char *buf, size_t bufsize, const char *gamestr, const char *uistr, const char *hwstr)
{
    return NULL;
}

const char *os_get_fname_log(char *buf, size_t bufsize)
{
    if (buf) {
        lib_strcpy(buf, "1oom_log.txt", bufsize);
        return buf;
    }
    return "1oom_log.txt";
}
