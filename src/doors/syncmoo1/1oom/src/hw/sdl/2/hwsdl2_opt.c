#include "config.h"

#include <stdio.h>

#include <SDL.h>

#include "hw.h"
#include "cfg.h"
#include "hwsdl_opt.h"
#include "hwsdl_audio.h"
#include "hwsdl_video.h"
#include "hwsdl2_video.h"
#include "lib.h"
#include "menu.h"
#include "options.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define HW_DEFAULT_FULLSCREEN   false

bool hw_opt_aspect_ratio_correct = true;
bool hw_opt_int_scaling = false;
#ifdef IS_WINDOWS
    bool hw_opt_nograbmouse = true;
#else
    bool hw_opt_nograbmouse = false;
#endif
#if SDL_VERSION_ATLEAST(2, 0, 18)
    bool hw_opt_relmouse = false;
#else
    bool hw_opt_relmouse = true;
#endif
bool hw_opt_autotrim = true;
bool hw_opt_vsync = true;
bool hw_opt_allow_upscaling = true;
int hw_opt_scaling_quality = 0;

#ifdef HAVE_SDL2MIXER
#define HAVE_SDLMIXER
#endif /* HAVE_SDL2MIXER */

#if SDL_VERSION_ATLEAST(2, 0, 12)
static const char *hw_scaling_quality_str[3] = { "Nearest", "Linear", "Best" };

static const char *hw_uiopts_scaling_quality_get(void)
{
    return hw_scaling_quality_str[hw_opt_scaling_quality];
}
#endif

/* -------------------------------------------------------------------------- */

const struct cfg_items_s hw_cfg_items_extra[] = {
    CFG_ITEM_BOOL("aspect_ratio_correct", &hw_opt_aspect_ratio_correct),
    CFG_ITEM_BOOL("int_scaling", &hw_opt_int_scaling),
    CFG_ITEM_BOOL("relmouse", &hw_opt_relmouse),
    CFG_ITEM_BOOL("nograbmouse", &hw_opt_nograbmouse),
    CFG_ITEM_BOOL("autotrim", &hw_opt_autotrim),
    CFG_ITEM_BOOL("vsync", &hw_opt_vsync),
    CFG_ITEM_BOOL("allow_upscaling", &hw_opt_allow_upscaling),
    CFG_ITEM_INT("scaling_quality", &hw_opt_scaling_quality, 0),
    CFG_ITEM_END
};

#include "hwsdl_opt.c"

const struct cmdline_options_s hw_cmdline_options_extra[] = {
    { "-aspect", 0,
      options_enable_bool_var, (void *)&hw_opt_aspect_ratio_correct,
      NULL, "Enable aspect ratio correction" },
    { "-noaspect", 0,
      options_disable_bool_var, (void *)&hw_opt_aspect_ratio_correct,
      NULL, "Disable aspect ratio correction" },
    { "-intscaling", 0,
      options_enable_bool_var, (void *)&hw_opt_int_scaling,
      NULL, "Force integer scaling" },
    { "-nointscaling", 0,
      options_disable_bool_var, (void *)&hw_opt_int_scaling,
      NULL, "Do not force integer scaling" },
    { "-grabmouse", 0,
      options_disable_bool_var, (void *)&hw_opt_nograbmouse,
      NULL, "Allow grab mouse" },
    { "-nograbmouse", 0,
      options_enable_bool_var, (void *)&hw_opt_nograbmouse,
      NULL, "Do not allow grab mouse" },
    { "-relmouse", 0,
      options_enable_bool_var, (void *)&hw_opt_relmouse,
      NULL, "Use relative mouse mode (default)" },
    { "-norelmouse", 0,
      options_disable_bool_var, (void *)&hw_opt_relmouse,
      NULL, "Do not use relative mouse mode" },
    { "-autotrim", 0,
      options_enable_bool_var, &hw_opt_autotrim,
      NULL, "Enable automatic resize" },
    { "-noautotrim", 0,
      options_disable_bool_var, &hw_opt_autotrim,
      NULL, "Disable automatic resize" },
    { "-vsync", 0,
      options_enable_bool_var, &hw_opt_vsync,
      NULL, "Enable V-sync" },
    { "-novsync", 0,
      options_disable_bool_var, &hw_opt_vsync,
      NULL, "Disable V-sync" },
    { "-filt", 1,
     options_set_int_var, (void *)&hw_opt_scaling_quality,
     "FILTER", "Set scaling quality (0 = nearest, 1 = linear, 2 = best)" },
    { NULL, 0, NULL, NULL, NULL, NULL }
};

void hw_opt_menu_make_page_video(void)
{
    menu_make_bool_func(menu_allocate_item(), "Borderless", &hw_opt_borderless, hw_video_toggle_borderless, MOO_KEY_o);
    menu_make_bool_func(menu_allocate_item(), "Fullscreen", &hw_opt_fullscreen, hw_video_toggle_fullscreen, MOO_KEY_f);
    menu_make_bool_func(menu_allocate_item(), "Aspect ratio correction", &hw_opt_aspect_ratio_correct, hw_video_toggle_aspect, MOO_KEY_a);
    #if SDL_VERSION_ATLEAST(2, 0, 18)
        menu_make_bool_func(menu_allocate_item(), "V-sync", &hw_opt_vsync, hw_video_toggle_vsync, MOO_KEY_v);
    #endif
    #if SDL_VERSION_ATLEAST(2, 0, 5)
        menu_make_bool_func(menu_allocate_item(), "Integer scaling", &hw_opt_int_scaling, hw_video_toggle_int_scaling, MOO_KEY_i);
    #endif
    #if SDL_VERSION_ATLEAST(2, 0, 12)
        menu_make_str_func(menu_allocate_item(), "Filter", hw_uiopts_scaling_quality_get, hw_video_filter_next, MOO_KEY_i);
    #endif
}
