/* already included by includer */
#if 0
#include "config.h"

#include <stdio.h>

#include "hw.h"
#include "hwsdl_opt.h"
#include "options.h"
#include "types.h"
#endif

/* -------------------------------------------------------------------------- */

bool hw_opt_borderless = false;
bool hw_opt_fullscreen = HW_DEFAULT_FULLSCREEN;
bool hw_opt_force_sw = false;
int hw_opt_screen_winw = 0;
int hw_opt_screen_winh = 0;
int hw_opt_screen_fsw = 0;
int hw_opt_screen_fsh = 0;
int hw_opt_mousespd = 100;
char *hw_opt_sdlmixer_sf = NULL;

/* -------------------------------------------------------------------------- */

const struct cfg_items_s hw_cfg_items[] = {
    CFG_ITEM_BOOL("borderless", &hw_opt_borderless),
    CFG_ITEM_BOOL("fs", &hw_opt_fullscreen),
    CFG_ITEM_BOOL("force_sw", &hw_opt_force_sw),
    CFG_ITEM_INT("winw", &hw_opt_screen_winw, 0),
    CFG_ITEM_INT("winh", &hw_opt_screen_winh, 0),
    CFG_ITEM_INT("fsw", &hw_opt_screen_fsw, 0),
    CFG_ITEM_INT("fsh", &hw_opt_screen_fsh, 0),
    CFG_ITEM_INT("mousespd", &hw_opt_mousespd, 0),
#ifdef HAVE_SDLMIXER
    CFG_ITEM_STR("sdlmixersf", &hw_opt_sdlmixer_sf, 0),
#endif
    CFG_ITEM_END
};

/* -------------------------------------------------------------------------- */

#ifdef HAVE_SDLMIXER
static int hw_opt_set_sdlmixer_sf(char **argv, void *var)
{
    hw_opt_sdlmixer_sf = lib_stralloc(argv[1]);
    return hw_audio_set_sdlmixer_sf(hw_opt_sdlmixer_sf);
}
#endif

/* -------------------------------------------------------------------------- */

const struct cmdline_options_s hw_cmdline_options[] = {
    { "-fs", 0,
      options_enable_bool_var, (void *)&hw_opt_fullscreen,
      NULL, "Enable fullscreen" },
    { "-window", 0,
      options_disable_bool_var, (void *)&hw_opt_fullscreen,
      NULL, "Use windowed mode" },
    { "-forcesw", 0,
      options_enable_bool_var, (void *)&hw_opt_force_sw,
      NULL, "Force software rendering" },
    { "-noforcesw", 0,
      options_disable_bool_var, (void *)&hw_opt_force_sw,
      NULL, "Do not force software rendering" },
    { "-winw", 1,
      options_set_int_var, (void *)&hw_opt_screen_winw,
      "WIDTH", "Set window width" },
    { "-winh", 1,
      options_set_int_var, (void *)&hw_opt_screen_winh,
      "HEIGHT", "Set window height" },
    { "-fsw", 1,
      options_set_int_var, (void *)&hw_opt_screen_fsw,
      "WIDTH", "Set fullscreen width" },
    { "-fsh", 1,
      options_set_int_var, (void *)&hw_opt_screen_fsh,
      "HEIGHT", "Set fullscreen height" },
    { "-mousespd", 1,
      options_set_int_var, (void *)&hw_opt_mousespd,
      "SPEED", "Set mouse speed (default = 100)" },
#ifdef HAVE_SDLMIXER
    { "-sdlmixersf", 1,
      hw_opt_set_sdlmixer_sf, NULL,
      "FILE.SF2", "Set SDL_mixer soundfont" },
#endif
    { NULL, 0, NULL, NULL, NULL, NULL }
};
