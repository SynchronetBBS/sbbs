#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "os.h"
#include "options.h"
#include "lib.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

const struct cmdline_options_s os_cmdline_options[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

/* -------------------------------------------------------------------------- */

static char *data_path = NULL;
static char *user_path = NULL;
static char *all_data_paths[] = { NULL, NULL, NULL, NULL };

/* -------------------------------------------------------------------------- */

const char *idstr_os = "msdos";

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

int os_make_path(const char *path)
{
    if ((path == NULL) || ((path[0] == '.') && (path[1] == '\0'))) {
        return 0;
    }
    return mkdir(path, 0700);
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
    lib_sprintf(buf, bufsize, "1oomsav%i.bin", savei);
    return buf;
}

const char *os_get_fname_save_year(char *buf, size_t bufsize, int year/*2300..*/)
{
    lib_sprintf(buf, bufsize, "1oom%i.bin", year);
    return buf;
}

const char *os_get_fname_cfg(char *buf, size_t bufsize, const char *gamestr, const char *uistr, const char *hwstr)
{
    const char *s;
    if (strcmp(gamestr, "game") == 0) {
        s = uistr;
    } else {
        s = gamestr;
    }
    lib_sprintf(buf, bufsize, "1%s", s);
    buf[8] = '\0';
    lib_strcat(buf, ".cfg", bufsize);
    return buf;
}

const char *os_get_fname_log(char *buf, size_t bufsize)
{
    if (buf) {
        lib_strcpy(buf, "1oom_log.txt", bufsize);
        return buf;
    }
    return "1oom_log.txt";
}
