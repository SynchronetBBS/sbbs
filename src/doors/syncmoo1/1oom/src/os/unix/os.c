#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "os.h"
#include "options.h"
#include "lib.h"
#include "util.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

const struct cmdline_options_s os_cmdline_options[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

/* -------------------------------------------------------------------------- */

#define NUM_ALL_DATA_PATHS 64

static char *data_path = NULL;
static char *user_path = NULL;
static char *all_data_paths[NUM_ALL_DATA_PATHS] = {NULL};
static int num_data_paths = 0;

/* -------------------------------------------------------------------------- */

const char *idstr_os = "unix";

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
    lib_free(user_path);
    user_path = NULL;
    lib_free(data_path);
    data_path = NULL;
    for (int i = 0; i < num_data_paths; ++i) {
        lib_free(all_data_paths[i]);
        all_data_paths[i] = NULL;
    }
    num_data_paths = 0;
}

const char **os_get_paths_data(void)
{
    if (num_data_paths == 0) {
        const char *p;
        int i = 0;
        bool got_xdg = false;

        if (data_path) {
            all_data_paths[i++] = lib_stralloc(data_path);
        }
        p = getenv("XDG_DATA_HOME");
        if (p) {
            all_data_paths[i++] = util_concat(p, "/1oom", NULL);
            got_xdg = true;
        }
        p = getenv("XDG_DATA_DIRS");
        if (p) {
            char *data_dirs = lib_stralloc(p);
            char *q = data_dirs;
            bool flag_done = false;
            while (!flag_done && *q) {
                char *r = strchr(q, ':');
                if (r == NULL) {
                    flag_done = true;
                } else {
                    *r = '\0';
                }
                r = strchr(q, '\0');
                if (!r || (r == q)) {
                    flag_done = true;
                } else {
                    if (*(r - 1) == '/') {
                        *(r - 1) = '\0';
                    }
                    all_data_paths[i++] = util_concat(q, "/1oom", NULL);
                }
                if (!flag_done) {
                    q = r + 1;
                }
            }
            got_xdg = true;
            lib_free(data_dirs);
        }
        if (!got_xdg) {
            p = getenv("HOME");
            if (p) {
                all_data_paths[i++] = util_concat(p, "/.local/share/1oom", NULL);;
            }
        }
        all_data_paths[i++] = lib_stralloc("/usr/share/1oom");
        all_data_paths[i++] = lib_stralloc("/usr/local/share/1oom");
        all_data_paths[i++] = lib_stralloc(".");
        all_data_paths[i++] = lib_stralloc("./data");
        all_data_paths[i] = NULL;
        num_data_paths = i;
    }
    return (const char **)all_data_paths;
}

const char *os_get_path_data(void)
{
    return data_path;
}

void os_set_path_data(const char *path)
{
    if (data_path) {
        lib_free(data_path);
        data_path = NULL;
    }
    data_path = lib_stralloc(path);
}


const char *os_get_path_user(void)
{
    if (user_path == NULL) {
        char *xdg_config_home = getenv("XDG_CONFIG_HOME");
        if (xdg_config_home != NULL) {
            user_path = util_concat(xdg_config_home, "/1oom", NULL);
        } else {
            char *home = getenv("HOME");
            if (home != NULL) {
                user_path = util_concat(home, "/.config/1oom", NULL);
            } else {
                user_path = lib_stralloc(".");
            }
        }
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

int os_make_path(const char *path)
{
    if ((path == NULL) || ((path[0] == '.') && (path[1] == '\0'))) {
        return 0;
    }
    if (access(path, F_OK)) {
        return mkdir(path, 0700);
    }
    return 0;
}

int os_make_path_user(void)
{
    return os_make_path(os_get_path_user());
}

int os_make_path_for(const char *filename)
{
    int res = 0;
    char *path;
    util_fname_split(filename, &path, NULL);
    if (path != NULL) {
        res = os_make_path(path);
        lib_free(path);
    }
    return res;
}

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
