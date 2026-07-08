#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"
#include "cfg.h"
#include "hw.h"
#include "lib.h"
#include "log.h"
#include "main.h"
#include "os.h"
#include "pbx.h"
#include "ui.h"
#include "util.h"
#include "version.h"

/* -------------------------------------------------------------------------- */
/* local options */

static const char *opt_logfilename_in = 0;
static const char *opt_configfilename_in = 0;   /* used only for the -c option */
static char *opt_configfilename = 0;
static bool opt_config_ro = false;
static char *opt_datapath = 0;
static bool opt_default = false;

/* -------------------------------------------------------------------------- */
/* global options */

#ifdef FEATURE_MODEBUG
int opt_modebug = 0;
#endif
bool opt_audio_enabled = true;
bool opt_music_enabled = true;
bool opt_sfx_enabled = true;
bool opt_sfx_init_parallel = true;
int opt_music_volume = 64;
int opt_sfx_volume = 100;
int opt_audiorate = 48000;
int opt_audioslice_ms = 50;
#ifdef HAVE_SAMPLERATE
bool opt_use_libsamplerate = true;
int opt_libsamplerate_scale = 65;
int opt_libsamplerate_mode = 1;
#endif
bool opt_cfg_reset = false;

/* -------------------------------------------------------------------------- */

static bool opt_cfg_set_datapath(void *var)
{
    LOG_DEBUG((1, "%s: '%s'\n", __func__, (const char *)var));
    os_set_path_data((const char *)var);
    return true;
}

const struct cfg_items_s opt_cfg_items[] = {
    CFG_ITEM_STR("data_path", &opt_datapath, opt_cfg_set_datapath),
    CFG_ITEM_END
};

const struct cfg_items_s opt_cfg_items_audio[] = {
    CFG_ITEM_BOOL("audio", &opt_audio_enabled),
    CFG_ITEM_BOOL("music", &opt_music_enabled),
    CFG_ITEM_BOOL("sfx", &opt_sfx_enabled),
    CFG_ITEM_BOOL("sfxinitpar", &opt_sfx_init_parallel),
    CFG_ITEM_INT("music_volume", &opt_music_volume, 0),
    CFG_ITEM_INT("sfx_volume", &opt_sfx_volume, 0),
    CFG_ITEM_INT("audiorate", &opt_audiorate, 0),
    CFG_ITEM_INT("audioslice_ms", &opt_audioslice_ms, 0),
#ifdef HAVE_SAMPLERATE
    CFG_ITEM_BOOL("use_libsamplerate", &opt_use_libsamplerate),
    CFG_ITEM_INT("libsamplerate_scale", &opt_libsamplerate_scale, 0),
    CFG_ITEM_INT("libsamplerate_mode", &opt_libsamplerate_mode, 0),
#endif
    CFG_ITEM_END
};

/* -------------------------------------------------------------------------- */

int options_enable_int_var(char **argv, void *var)
{
    *((int *)var) = 1;
    return 0;
}

int options_disable_int_var(char **argv, void *var)
{
    *((int *)var) = 0;
    return 0;
}

int options_set_int_var(char **argv, void *var)
{
    *((int *)var) = atoi(argv[1]);
    return 0;
}

int options_set_str_var(char **argv, void *var)
{
    *(const char **)var = argv[1];
    return 0;
}

int options_unset_str_var(char **argv, void *var)
{
    *(const char **)var = 0;
    return 0;
}

int options_empty_str_var(char **argv, void *var)
{
    *(const char **)var = "";
    return 0;
}

int options_enable_bool_var(char **argv, void *var)
{
    *((bool *)var) = true;
    return 0;
}

int options_disable_bool_var(char **argv, void *var)
{
    *((bool *)var) = false;
    return 0;
}

int options_notimpl(char **argv, void *var)
{
    log_error("option '%s' is not implemented (yet)", argv[0]);
    return -1;
}

int options_nop(char **argv, void *var)
{
    return 0;
}

/* -------------------------------------------------------------------------- */

static int options_set_userdir(char **argv, void *var)
{
    log_message("Setting user directory to '%s'\n", argv[1]);
    os_set_path_user(argv[1]);
    return 0;
}

static int options_set_datadir(char **argv, void *var)
{
    log_message("Setting data directory to '%s'\n", argv[1]);
    os_set_path_data(argv[1]);
    return 0;
}

static int options_add_patchfile(char **argv, void *var)
{
    if (pbx_queue_file(argv[1]) < 0) {
        log_error("too many patchfiles");
        return -1;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */

static int show_usage(char **argv, void *var);
static int load_default(char **argv, void *var);

static const struct cmdline_options_s cmdline_options_early[] = {
    { "-?", 0,
      show_usage, NULL,
      NULL, "Show command line options" },
    { "-default", 0,
      load_default, NULL,
      NULL, "Use default configuration" },
#ifdef FEATURE_MODEBUG
    { "-modebug", 1,
      options_set_int_var, (void *)&opt_modebug,
      "LEVEL", "Set debug level" },
#endif
    { NULL, 0, NULL, NULL, NULL, NULL }
};

static const struct cmdline_options_s cmdline_options_cfg_early[] = {
    { "-c", 1,
      options_set_str_var, (void *)&opt_configfilename_in,
      "FILE.TXT", "Set config filename" },
    { "-cro", 0,
      options_enable_bool_var, (void *)&opt_config_ro,
      NULL, "Do not write a config file" },
    { "-user", 1,
      options_set_userdir, NULL,
      "PATH", "Set user directory" },
    { "-log", 1,
      options_set_str_var, (void *)&opt_logfilename_in,
      "FILE.TXT", "Set log filename" },
    { "-nolog", 0,
      options_empty_str_var, (void *)&opt_logfilename_in,
      NULL, "Do not create a log file" },
    { NULL, 0, NULL, NULL, NULL, NULL }
};

static const struct cmdline_options_s cmdline_options_lbx[] = {
    { "-data", 1,
      options_set_datadir, NULL,
      "PATH", "Set data directory" },
    { NULL, 0, NULL, NULL, NULL, NULL }
};

static const struct cmdline_options_s cmdline_options_pbxfile[] = {
    { "-file", 1,
      options_add_patchfile, NULL,
      "FILE.PBX", "Add PBX file" },
    { NULL, 0, NULL, NULL, NULL, NULL }
};

static const struct cmdline_options_s cmdline_options_audio_early[] = {
    { "-audio", 0,
      options_enable_bool_var, (void *)&opt_audio_enabled,
      NULL, "Enable audio" },
    { "-noaudio", 0,
      options_disable_bool_var, (void *)&opt_audio_enabled,
      NULL, "Disable audio" },
    { NULL, 0, NULL, NULL, NULL, NULL }
};

static const struct cmdline_options_s cmdline_options_audio[] = {
    { "-music", 0,
      options_enable_bool_var, (void *)&opt_music_enabled,
      NULL, "Enable music" },
    { "-nomusic", 0,
      options_disable_bool_var, (void *)&opt_music_enabled,
      NULL, "Disable music" },
    { "-sfx", 0,
      options_enable_bool_var, (void *)&opt_sfx_enabled,
      NULL, "Enable SFX" },
    { "-nosfx", 0,
      options_disable_bool_var, (void *)&opt_sfx_enabled,
      NULL, "Disable SFX" },
    { "-sfxinitpar", 0,
      options_enable_bool_var, (void *)&opt_sfx_init_parallel,
      NULL, "Init SFX in parallel (if possible)" },
    { "-nosfxinitpar", 0,
      options_disable_bool_var, (void *)&opt_sfx_init_parallel,
      NULL, "Do not init SFX in parallel" },
    { "-musicvol", 1,
      options_set_int_var, (void *)&opt_music_volume,
      "VOLUME", "Set music volume (0..128)" },
    { "-sfxvol", 1,
      options_set_int_var, (void *)&opt_sfx_volume,
      "VOLUME", "Set SFX volume (0..128)" },
    { "-audiohz", 1,
      options_set_int_var, (void *)&opt_audiorate,
      "HZ", "Set audio sample rate (Hz)" },
    { "-audioms", 1,
      options_set_int_var, (void *)&opt_audioslice_ms,
      "MS", "Set max audio slice size (ms)" },
#ifdef HAVE_SAMPLERATE
    { "-libsr", 0,
      options_enable_bool_var, (void *)&opt_use_libsamplerate,
      NULL, "Use libsamplerate" },
    { "-nolibsr", 0,
      options_disable_bool_var, (void *)&opt_use_libsamplerate,
      NULL, "Do not use libsamplerate" },
    { "-libsrscale", 1,
      options_set_int_var, (void *)&opt_libsamplerate_scale,
      "PERCENT", "libsamplerate scaling %" },
    { "-libsrmode", 1,
      options_set_int_var, (void *)&opt_libsamplerate_mode,
      "MODE", "libsamplerate mode (0 = best, 4 = worst)" },
#endif
    { NULL, 0, NULL, NULL, NULL, NULL }
};

/* -------------------------------------------------------------------------- */

static int get_options_w(const struct cmdline_options_s *opts, int lmax)
{
    int i = 0;
    while (opts[i].str != NULL) {
        int l;
        l = strlen(opts[i].str) + 1;
        if (opts[i].str_param) {
            l += strlen(opts[i].str_param);
        }
        if (l > lmax) {
            lmax = l;
        }
        ++i;
    }
    return lmax;
}

static void show_options(const struct cmdline_options_s *opts, int lmax)
{
    int i = 0;
    char fmt1[16];
    char fmt2[16];
    lib_sprintf(fmt1, sizeof(fmt1), "    %%-%is ", lmax);
    lib_sprintf(fmt2, sizeof(fmt2), "%%s\n%%-%is", lmax + 5);
    while (opts[i].str != NULL) {
        char buf[128];
        if (opts[i].str_help) {
            const char *p, *q;
            lib_sprintf(buf, sizeof(buf), "%s %s", opts[i].str, opts[i].str_param ? opts[i].str_param : "");
            log_message(fmt1, buf);
            p = opts[i].str_help;
            while ((q = strchr(p, '\n')) != NULL) {
                int len;
                len = q - p;
                memcpy(buf, p, len);
                buf[len] = '\0';
                log_message(fmt2, buf, "");
                p = q + 1;
            }
            log_message("%s\n", p);
        }
        ++i;
    }
}

static int show_usage(char **argv, void *var)
{
    options_show_usage();
    return -1;
}

static int load_default(char **argv, void *var)
{
    opt_default = true;
    return 0;
}

/* -------------------------------------------------------------------------- */

static const struct cmdline_options_s *find_option_do(const char *name, const struct cmdline_options_s *cmds)
{
    while (cmds->str != NULL) {
        if (strcmp(name, cmds->str) == 0) {
            return cmds;
        }
        ++cmds;
    }

    return NULL;
}

static const struct cmdline_options_s *find_option(const char *name, bool early, bool *was_early)
{
    const struct cmdline_options_s *o = NULL;

    if (0
      || (o = find_option_do(name, cmdline_options_early))
      || (main_use_cfg && (o = find_option_do(name, cmdline_options_cfg_early)))
      || (ui_use_audio && (o = find_option_do(name, cmdline_options_audio_early)))
      || (o = find_option_do(name, main_cmdline_options_early))
    ) {
        *was_early = true;
        return o;
    }
    if (0
      || (main_use_lbx && (o = find_option_do(name, cmdline_options_lbx)))
      || (main_use_lbx && (o = find_option_do(name, cmdline_options_pbxfile)))
      || (ui_use_audio && (o = find_option_do(name, cmdline_options_audio)))
      || (o = find_option_do(name, os_cmdline_options))
      || (o = find_option_do(name, hw_cmdline_options))
      || (o = find_option_do(name, hw_cmdline_options_extra))
      || (o = find_option_do(name, ui_cmdline_options))
      || (o = find_option_do(name, main_cmdline_options))
    ) {
        *was_early = false;
        return o;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int options_parse_do(int argc, char **argv, bool early)
{
    int i = 1;
    int num;
    bool was_early;
    const struct cmdline_options_s *o;

    while (i < argc) {
        switch (argv[i][0]) {
            case '+':
            case '-':
                o = find_option(argv[i], early, &was_early);

                if (!o) {
                    log_error("unknown option '%s'\n", argv[i]);
                    show_usage(NULL, NULL);
                    return -1;
                }

                num = o->num_param;

                if (num >= (argc - i)) {
                    log_error("option '%s' is missing the parameter\n", argv[i]);
                    return -1;
                }

                if (early == was_early) {
                    if (o->handle(&argv[i], o->var) < 0) {
                        return -1;
                    }
                }

                i += num;
                break;

            default:
                if (!early) {
                    if (main_handle_option(argv[i]) < 0) {
                        log_warning("ignoring unhandled parameter '%s'\n", argv[i]);
                    }
                }
                break;
        }

        ++i;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */

int options_parse_early(int argc, char **argv)
{
    int res;
    /* parse options first to exit early on "-?" and open the log file */
    res = options_parse_do(argc, argv, true);
    if (!res) {
        const char *filename = 0;
        char *filenamea = 0;
        if (opt_logfilename_in) {
            filename = opt_logfilename_in;
        } else {
            char namebuf[128];
            if (os_get_fname_log(namebuf, sizeof(namebuf))) {
                const char *path = os_get_path_user();
                filename = filenamea = util_concat(path, FSDEV_DIR_SEP_STR, namebuf, NULL);
            }
        }
        if (filename) {
            log_file_open(filename);
            if (filenamea) {
                lib_free(filenamea);
                filenamea = 0;
            }
            filename = 0;
        }
    }
    if ((!res) && main_use_cfg) {
        if (opt_configfilename_in != 0) {
            opt_configfilename = lib_stralloc(opt_configfilename_in);
        } else {
            opt_configfilename = cfg_cfgname();
        }
        if (!opt_default && cfg_load(opt_configfilename)) {
            log_warning("Opt: problems loading config file '%s'\n", opt_configfilename);
        }
        /* parse options again to override configuration */
        res = options_parse_do(argc, argv, true);
    }
    return res;
}

int options_parse(int argc, char **argv)
{
    return options_parse_do(argc, argv, false);
}

void options_show_usage(void)
{
    int lmax = 0;
    log_message_direct(PACKAGE_NAME " " VERSION_STR "\n");
    if (main_usage) {
        main_usage();
    }
    log_message_direct("Options:\n");

    lmax = get_options_w(cmdline_options_early, lmax);
    if (main_use_cfg) {
        lmax = get_options_w(cmdline_options_cfg_early, lmax);
    }
    if (main_use_lbx) {
        lmax = get_options_w(cmdline_options_lbx, lmax);
        lmax = get_options_w(cmdline_options_pbxfile, lmax);
    }
    if (ui_use_audio) {
        lmax = get_options_w(cmdline_options_audio_early, lmax);
        lmax = get_options_w(cmdline_options_audio, lmax);
    }
    lmax = get_options_w(os_cmdline_options, lmax);
    lmax = get_options_w(hw_cmdline_options, lmax);
    lmax = get_options_w(hw_cmdline_options_extra, lmax);
    lmax = get_options_w(ui_cmdline_options, lmax);
    lmax = get_options_w(main_cmdline_options_early, lmax);
    lmax = get_options_w(main_cmdline_options, lmax);

    show_options(cmdline_options_early, lmax);
    if (main_use_cfg) {
        show_options(cmdline_options_cfg_early, lmax);
    }
    if (main_use_lbx) {
        show_options(cmdline_options_lbx, lmax);
        show_options(cmdline_options_pbxfile, lmax);
    }
    if (ui_use_audio) {
        show_options(cmdline_options_audio_early, lmax);
        show_options(cmdline_options_audio, lmax);
    }
    show_options(os_cmdline_options, lmax);
    show_options(hw_cmdline_options, lmax);
    show_options(hw_cmdline_options_extra, lmax);
    show_options(ui_cmdline_options, lmax);
    show_options(main_cmdline_options_early, lmax);
    show_options(main_cmdline_options, lmax);
}

void options_finish(void)
{
    if (opt_datapath) { /* from config file */
        lib_free(opt_datapath);
    }
    opt_datapath = lib_stralloc(os_get_path_data());
}

void options_shutdown(bool save_config)
{
    if (main_use_cfg && save_config && opt_configfilename && (!opt_config_ro)) {
        if (cfg_save(opt_configfilename) != 0) {
            log_error("Opt: problems saving config file '%s'\n", opt_configfilename);
        }
    }
    lib_free(opt_configfilename);
    opt_configfilename = 0;
    lib_free(opt_datapath);
    opt_datapath = 0;
}
