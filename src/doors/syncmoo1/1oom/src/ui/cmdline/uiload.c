#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game_save.h"
#include "types.h"
#include "uidefs.h"
#include "uiinput.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

int ui_load_game(void)
{
    struct input_list_s lg_in[] = {
        { 0, "1", NULL, NULL },
        { 1, "2", NULL, NULL },
        { 2, "3", NULL, NULL },
        { 3, "4", NULL, NULL },
        { 4, "5", NULL, NULL },
        { 5, "6", NULL, NULL },
        { 7, "7", NULL, NULL },
        { 8, "8", NULL, NULL },
        { 9, "9", NULL, NULL },
        { 0, NULL, NULL, NULL },
        { 0, NULL, NULL, NULL }
    };

    int num = 0;

    for (int i = 0; i < NUM_ALL_SAVES; ++i) {
        if (game_save_tbl_have_save[i]) {
            lg_in[num].value = i;
            lg_in[num].display = game_save_tbl_name[i];
            ++num;
        }
    }

    if (num == 0) {
        return -1;
    }

    lg_in[num].value = -1;
    lg_in[num].key = "Q";
    lg_in[num].str = "q";
    lg_in[num].display = "(quit)";
    ++num;
    lg_in[num].value = 0;
    lg_in[num].key = NULL;
    lg_in[num].str = NULL;
    lg_in[num].display = NULL;
    return ui_input_list("Load game", "> ", lg_in);
}

int ui_cmd_load(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    if (num_param == 0) {
        return ui_load_game();
    } else {
        int v;
        if (util_parse_signed_number(param->str, &v)) {
            if ((v >= 1) && (v <= NUM_ALL_SAVES)) {
                return v - 1;
            } else if ((v >= 2300) && (v <= 9999)) {
                return v;
            }
        }
    }
    return -1;
}
