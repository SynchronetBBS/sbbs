#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "gameapi.h"
#include "hw.h"
#include "lbx.h"
#include "log.h"
#include "options.h"
#include "os.h"
#include "pbx.h"
#include "ui.h"
#include "version.h"

/* -------------------------------------------------------------------------- */

static bool main_startup_ok = false;

/* -------------------------------------------------------------------------- */

static int main_early_init(void)
{
    if (0
      || os_early_init()
      || hw_early_init()
      || ui_early_init()
      || lbxfile_init()
    ) {
        return 1;
    }
    return 0;
}

static int main_init(void)
{
    if (0
      || os_init()
      || hw_init()
      || ui_init()
    ) {
        return 1;
    }
    return 0;
}

static void main_shutdown(void)
{
    options_shutdown(main_startup_ok);
    main_do_shutdown();
    lbxfile_shutdown();
    ui_shutdown();
    hw_shutdown();
    os_shutdown();
    log_file_close();
    log_message("Thanks for playing 1oom.\n");
}

/* -------------------------------------------------------------------------- */

int main_1oom(int argc, char **argv)
{
    if (main_early_init()) {
        return 1;
    }
    if (options_parse_early(argc, argv)) {
        return 1;
    }
    log_message(PACKAGE_NAME " " VERSION_STR ": main:%s ui:%s hw:%s os:%s\n", idstr_main, idstr_ui, idstr_hw, idstr_os);
    atexit(main_shutdown);
    if (main_init()) {
        return 2;
    }
    if (options_parse(argc, argv)) {
        return 3;
    }
    if (lbxfile_find_dir()) {
        ui_early_show_message_box("Error: could not find\nthe LBX files!\n"
                            "1oom requires a copy\nof the Master of Orion\n(v1.3) LBX files.\n"
                            "Simply copy the EXE and\nDLLs to your MOO1\ndirectory.\n"
                            "Alternatively copy your\nMOO1 LBX files to your\n1oom directory.");
        return 4;
    }
    game_apply_rules();
    if (pbx_apply_queued_files()) {
        return 5;
    }

    log_message("USER: '%s'\n", os_get_path_user());
    main_startup_ok = true;
    options_finish();
    return main_do();
}
