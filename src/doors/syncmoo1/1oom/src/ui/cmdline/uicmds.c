#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "uicmds.h"
#include "uidefs.h"
#include "uiload.h"
#include "uiinput.h"
#include "uisave.h"

/* -------------------------------------------------------------------------- */

int ui_cmd_dummy_ret(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return (int)(intptr_t)var;
}

int ui_cmd_quit(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    if ((num_param == 1) && (param[0].str[0] == '!') && (param[0].str[1] == '\0')) {
        exit(EXIT_SUCCESS);
    }
    return 0;
}

const struct input_cmd_s ui_cmds_opts[] = {
    { "load", "[SAVENUM]", "Load game", 0, 1, 0, ui_cmd_load, 0 },
    { "save", "[SAVENUM [SAVENAME]]", "Save game", 0, 2, 0, ui_cmd_save, 0 },
    { "quit", "[!]", "Quit the game; add ! to quit without saving", 0, 1, 0, ui_cmd_quit, 0 },
    { NULL, NULL, NULL, 0, 0, 0, NULL, 0 }
};
