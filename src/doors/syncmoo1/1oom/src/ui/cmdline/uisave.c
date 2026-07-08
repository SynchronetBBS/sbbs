#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game_save.h"
#include "types.h"
#include "uidefs.h"
#include "uiinput.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static int ui_save_game(const struct game_s *g)
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
        { 0, NULL, NULL, NULL },
        { 0, NULL, NULL, NULL }
    };

    int v, num = 0;

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
    lg_in[num].value = -1;
    lg_in[num].key = NULL;
    lg_in[num].str = NULL;
    lg_in[num].display = NULL;
    v = ui_input_list("Save game", "> ", lg_in);
    if ((v >= 0) && (v < NUM_ALL_SAVES)) {
        const char *savename;
        savename = ui_input_line("Save name > ");
        if ((!savename) || (savename[0] == '\0')) {
            savename = game_save_tbl_have_save[v] ? game_save_tbl_name[v] : "(unnamed)";
        }
        return game_save_do_save_i(v, savename, g);
    }
    return 0;
}

/* -------------------------------------------------------------------------- */

int ui_cmd_save(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    if (num_param == 0) {
        return ui_save_game(g);
    } else {
        int v;
        if (util_parse_signed_number(param->str, &v) && (v >= 1) && (v <= NUM_ALL_SAVES)) {
            const char *savename;
            --v;
            if (num_param == 2) {
                savename = param[1].str;
            } else {
                savename = game_save_tbl_have_save[v] ? game_save_tbl_name[v] : "(unnamed)";
            }
            return game_save_do_save_i(v, savename, g);
        }
    }
    return -1;
}
