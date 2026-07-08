#include "config.h"

#include <stdio.h>

#include "hw.h"
#include "cfg.h"
#include "hwalleg_opt.h"
#include "lib.h"
#include "options.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

const struct cfg_items_s hw_cfg_items_extra[] = {
    CFG_ITEM_END
};

#include "hwalleg_opt.c"

const struct cmdline_options_s hw_cmdline_options_extra[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

void hw_opt_menu_make_page_video(void) {}
