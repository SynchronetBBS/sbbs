/* already included by includer */
#if 0
#include "config.h"

#include <stdio.h>

#include "hw.h"
#include "hwalleg_opt.h"
#include "options.h"
#include "types.h"
#endif

/* -------------------------------------------------------------------------- */

bool hw_opt_fullscreen = true;
bool hw_opt_relmouse = true;
int hw_opt_mouse_slowdown_x = 1;
int hw_opt_mouse_slowdown_y = 1;

/* -------------------------------------------------------------------------- */

const struct cfg_items_s hw_cfg_items[] = {
    CFG_ITEM_BOOL("fs", &hw_opt_fullscreen),
    CFG_ITEM_BOOL("relmouse", &hw_opt_relmouse),
    CFG_ITEM_INT("mouse_slowdown_x", &hw_opt_mouse_slowdown_x, NULL),
    CFG_ITEM_INT("mouse_slowdown_y", &hw_opt_mouse_slowdown_y, NULL),
    CFG_ITEM_END
};

/* -------------------------------------------------------------------------- */

const struct cmdline_options_s hw_cmdline_options[] = {
    { "-fs", 0,
      options_enable_bool_var, (void *)&hw_opt_fullscreen,
      NULL, "Enable fullscreen" },
    { "-window", 0,
      options_disable_bool_var, (void *)&hw_opt_fullscreen,
      NULL, "Use windowed mode" },
    { NULL, 0, NULL, NULL, NULL, NULL }
};
