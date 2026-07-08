#ifndef INC_1OOM_OPTIONS_H
#define INC_1OOM_OPTIONS_H

#include "cfg.h"
#include "types.h"

extern const struct cfg_items_s opt_cfg_items[];
extern const struct cfg_items_s opt_cfg_items_audio[];

#ifdef FEATURE_MODEBUG
extern int opt_modebug;
#endif
extern bool opt_audio_enabled;
extern bool opt_music_enabled;
extern bool opt_sfx_enabled;
extern bool opt_sfx_init_parallel;
extern int opt_music_volume;
extern int opt_sfx_volume;
extern int opt_audiorate;
extern int opt_audioslice_ms;
#ifdef HAVE_SAMPLERATE
extern bool opt_use_libsamplerate;
extern int opt_libsamplerate_scale;
extern int opt_libsamplerate_mode;
#endif
extern bool opt_cfg_reset;

struct cmdline_options_s {
    const char *str;
    int num_param;
    int (*handle)(char **argv, void *var);
    void *var;
    const char *str_param;
    const char *str_help;
};

extern int options_enable_int_var(char **argv, void *var);
extern int options_disable_int_var(char **argv, void *var);
extern int options_set_int_var(char **argv, void *var);
extern int options_set_str_var(char **argv, void *var);
extern int options_unset_str_var(char **argv, void *var);
extern int options_empty_str_var(char **argv, void *var);
extern int options_enable_bool_var(char **argv, void *var);
extern int options_disable_bool_var(char **argv, void *var);

extern int options_parse_early(int argc, char **argv);
extern int options_parse(int argc, char **argv);
extern void options_show_usage(void);

extern void options_finish(void);
extern void options_shutdown(bool save_config);

#endif
