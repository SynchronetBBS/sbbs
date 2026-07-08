#include "config.h"

#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "game_ai.h"
#include "game_new.h"
#include "game_str.h"
#include "lib.h"
#include "types.h"
#include "uidefs.h"
#include "uihelp.h"
#include "uiinput.h"
#include "uiload.h"

/* -------------------------------------------------------------------------- */

static int ui_new_game_choose_race(struct game_new_options_s *newopts, player_id_t pi)
{
    int v;
    struct input_list_s ng_race_in[] = {
        { RACE_HUMAN, "1", NULL, game_str_tbl_race[RACE_HUMAN] },
        { RACE_MRRSHAN, "2", NULL, game_str_tbl_race[RACE_MRRSHAN] },
        { RACE_SILICOID, "3", NULL, game_str_tbl_race[RACE_SILICOID] },
        { RACE_SAKKRA, "4", NULL, game_str_tbl_race[RACE_SAKKRA] },
        { RACE_PSILON, "5", NULL, game_str_tbl_race[RACE_PSILON] },
        { RACE_ALKARI, "6", NULL, game_str_tbl_race[RACE_ALKARI] },
        { RACE_KLACKON, "7", NULL, game_str_tbl_race[RACE_KLACKON] },
        { RACE_BULRATHI, "8", NULL, game_str_tbl_race[RACE_BULRATHI] },
        { RACE_MEKLAR, "9", NULL, game_str_tbl_race[RACE_MEKLAR] },
        { RACE_DARLOK, "0", NULL, game_str_tbl_race[RACE_DARLOK] },
        { RACE_RANDOM, "R", NULL, game_str_tbl_race[RACE_RANDOM] },
        { -1, "Q", NULL, "(quit)" },
        { 0, NULL, NULL, NULL },
    };
    if ((v = ui_input_list("Race", "> ", ng_race_in)) < 0) {
        return -1;
    }
    newopts->pdata[pi].race = v;
    return v;
}

static int ui_new_game_choose_banner(struct game_new_options_s *newopts, player_id_t pi)
{
    int v;
    struct input_list_s ng_banner_in[] = {
        { BANNER_BLUE, "1", NULL, game_str_tbl_banner[BANNER_BLUE] },
        { BANNER_GREEN, "2", NULL, game_str_tbl_banner[BANNER_GREEN] },
        { BANNER_PURPLE, "3", NULL, game_str_tbl_banner[BANNER_PURPLE] },
        { BANNER_RED, "4", NULL, game_str_tbl_banner[BANNER_RED] },
        { BANNER_WHITE, "5", NULL, game_str_tbl_banner[BANNER_WHITE] },
        { BANNER_YELLOW, "6", NULL, game_str_tbl_banner[BANNER_YELLOW] },
        { BANNER_RANDOM, "7", NULL, game_str_tbl_banner[BANNER_RANDOM] },
        { -1, "Q", NULL, "(quit)" },
        { 0, NULL, NULL, NULL },
    };
    if ((v = ui_input_list("Banner", "> ", ng_banner_in)) < 0) {
        return -1;
    }
    newopts->pdata[pi].banner = v;
    return v;
}

static int ui_new_game_pname(struct game_new_options_s *newopts, player_id_t pi)
{
    char *p;
    fputs("Your name\n", stdout);
    p = ui_input_line_len_trim("> ", EMPEROR_NAME_LEN);
    if (p[0] == '\0') {
        race_t race = newopts->pdata[pi].race;
        if (race < RACE_NUM) {
            game_new_generate_emperor_name(race, newopts->pdata[pi].playername, EMPEROR_NAME_LEN);
        } else {
            newopts->pdata[pi].playername[0] = '\0';
        }
    } else {
        lib_strcpy(newopts->pdata[pi].playername, p, EMPEROR_NAME_LEN);
    }
    return 0;
}

static int ui_new_game_hname(struct game_new_options_s *newopts, player_id_t pi)
{
    char *p;
    fputs("Home world\n", stdout);
    p = ui_input_line_len_trim("> ", PLANET_NAME_LEN);
    if (p[0] == '\0') {
        race_t race = newopts->pdata[pi].race;
        if (race < RACE_NUM) {
            game_new_generate_home_name(race, newopts->pdata[pi].homename, PLANET_NAME_LEN);
        } else {
            newopts->pdata[pi].homename[0] = '\0';
        }
    } else {
        lib_strcpy(newopts->pdata[pi].homename, p, PLANET_NAME_LEN);
    }
    return 0;
}

static bool have_human(const struct game_new_options_s *newopts)
{
    for (int j = 0; j < newopts->players; ++j) {
        if (!newopts->pdata[j].is_ai) {
            return true;
        }
    }
    return false;
}

static void ui_new_game_display(struct game_new_options_s *newopts)
{
    for (int i = 0; i < newopts->players; ++i) {
        printf("%s %i: %s, %s", newopts->pdata[i].is_ai ? game_str_ng_computer : game_str_ng_player, i + 1, game_str_tbl_race[newopts->pdata[i].race], game_str_tbl_banner[newopts->pdata[i].banner]);
        if (newopts->pdata[i].playername[0]) {
            printf(", Emperor %s", newopts->pdata[i].playername);
        }
        if (newopts->pdata[i].homename[0]) {
            printf(", Home world %s", newopts->pdata[i].homename);
        }
        putchar('\n');
    }
    printf("%s: %s\n", game_str_ng_ai, game_ais[newopts->ai_id]->name);
    if (!have_human(newopts)) {
        puts(game_str_ng_allai);
    }
}

static int cmd_exit(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct game_new_options_s *newopts = (struct game_new_options_s *)g;
    int v;
    v = (int)(intptr_t)var;
    if ((v == 2/*start*/) && (!have_human(newopts))) {
        v = 0;
    }
    return v;
}

static int cmd_toggle_computer(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct game_new_options_s *newopts = (struct game_new_options_s *)g;
    int n;
    if (param->type != INPUT_TOKEN_NUMBER) {
        return -1;
    }
    n = param->data.num;
    if ((n < 1) || (n > newopts->players)) {
        return -1;
    }
    --n;
    newopts->pdata[n].is_ai = !newopts->pdata[n].is_ai;
    return 0;
}

static int cmd_race(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct game_new_options_s *newopts = (struct game_new_options_s *)g;
    int n;
    race_t old_race, new_race;
    if (param->type != INPUT_TOKEN_NUMBER) {
        return -1;
    }
    n = param->data.num;
    if ((n < 1) || (n > newopts->players)) {
        return -1;
    }
    --n;
    old_race = newopts->pdata[n].race;
    if (ui_new_game_choose_race(newopts, n) < 0) {
        return -1;
    }
    new_race = newopts->pdata[n].race;
    if (new_race != old_race) {
        if (new_race < RACE_NUM) {
            game_new_generate_emperor_name(new_race, newopts->pdata[n].playername, EMPEROR_NAME_LEN);
            game_new_generate_home_name(new_race, newopts->pdata[n].homename, PLANET_NAME_LEN);
        } else {
            newopts->pdata[n].playername[0] = '\0';
            newopts->pdata[n].homename[0] = '\0';
        }
    }
    return 0;
}

static int cmd_choose(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct game_new_options_s *newopts = (struct game_new_options_s *)g;
    int (*func)(struct game_new_options_s *newopts, player_id_t pi) = var;
    int n;
    if (param->type != INPUT_TOKEN_NUMBER) {
        return -1;
    }
    n = param->data.num;
    if ((n < 1) || (n > newopts->players)) {
        return -1;
    }
    --n;
    if (func(newopts, n) < 0) {
        return -1;
    }
    return 0;
}

static int cmd_ai(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct input_list_s rl_in[] = {
        { 0, "1", NULL, NULL },
        { 1, "2", NULL, NULL },
        { 2, "3", NULL, NULL },
        { 3, "4", NULL, NULL },
        { 4, "5", NULL, NULL },
        { 5, "6", NULL, NULL },
        { 0, NULL, NULL, NULL },
        { 0, NULL, NULL, NULL }
    };
    struct game_new_options_s *newopts = (struct game_new_options_s *)g;
    int num;
    for (num = 0; num < GAME_AI_NUM; ++num) {
        rl_in[num].value = num;
        rl_in[num].display = game_ais[num]->name;
    }
    rl_in[num].value = 0;
    rl_in[num].key = NULL;
    rl_in[num].str = NULL;
    rl_in[num].display = NULL;
    newopts->ai_id = ui_input_list(game_str_ng_ai, "> ", rl_in);
    return 0;
}

static bool ui_new_game_extra(struct game_new_options_s *newopts)
{
    struct input_cmd_s cmds_newgame[] = {
        { "?", NULL, "Help", 0, 0, 0, ui_cmd_help, 0 },
        { "q", NULL, game_str_ng_cancel, 0, 0, 0, cmd_exit, (void *)1/*cancel*/ },
        { "s", NULL, game_str_ng_ok, 0, 0, 0, cmd_exit, (void *)2/*start*/ },
        { "a", NULL, game_str_ng_ai, 0, 0, 0, cmd_ai, 0 },
        { "c", "[PLAYER]", game_str_ng_computer, 1, 1, 0, cmd_toggle_computer, 0 },
        { "r", "[PLAYER]", "Race", 1, 1, 0, cmd_race, 0 },
        { "b", "[PLAYER]", "Banner", 1, 1, 0, cmd_choose, ui_new_game_choose_banner },
        { "n", "[PLAYER]", "Emperor name", 1, 1, 0, cmd_choose, ui_new_game_pname },
        { "h", "[PLAYER]", "Home word name", 1, 1, 0, cmd_choose, ui_new_game_hname },
        { NULL, NULL, NULL, 0, 0, 0, NULL, 0 }
    };

    const struct input_cmd_s * const cmdsptr_newgame[] = {
        cmds_newgame,
        NULL
    };

    cmds_newgame[0].var = (void *)cmdsptr_newgame;
    ui_new_game_display(newopts);
    while (1) {
        char *input;
        input = ui_input_line("> ");
        if ((ui_input_tokenize(input, cmdsptr_newgame) == 0) && (ui_data.input.num > 0)) {
            if (ui_data.input.tok[0].type == INPUT_TOKEN_COMMAND) {
                const struct input_cmd_s *cmd;
                int v;
                cmd = ui_data.input.tok[0].data.cmd;
                v = cmd->handle((struct game_s *)newopts, 0, &ui_data.input.tok[1], ui_data.input.num - 1, cmd->var);
                if (cmd->handle == cmd_exit) {
                    if (v == 1/*cancel*/) {
                        return false;
                    } else if (v == 2/*start*/) {
                        return true;
                    }
                } else {
                    ui_new_game_display(newopts);
                }
            }
        }
    }
}

static bool ui_new_game(struct game_new_options_s *newopts)
{
    int v;

    struct input_list_s ng_galaxy_in[] = {
        { 0, "1", NULL, game_str_tbl_gsize[0] },
        { 1, "2", NULL, game_str_tbl_gsize[1] },
        { 2, "3", NULL, game_str_tbl_gsize[2] },
        { 3, "4", NULL, game_str_tbl_gsize[3] },
        { -1, "Q", NULL, "(quit)" },
        { 0, NULL, NULL, NULL },
    };

    struct input_list_s ng_diffic_in[] = {
        { 0, "1", NULL, game_str_tbl_diffic[0] },
        { 1, "2", NULL, game_str_tbl_diffic[1] },
        { 2, "3", NULL, game_str_tbl_diffic[2] },
        { 3, "4", NULL, game_str_tbl_diffic[3] },
        { 4, "5", NULL, game_str_tbl_diffic[4] },
        { -1, "Q", NULL, "(quit)" },
        { 0, NULL, NULL, NULL },
    };

    struct input_list_s ng_oppon_in[] = {
        { 2, "1", NULL, game_str_tbl_oppon[0] },
        { 3, "2", NULL, game_str_tbl_oppon[1] },
        { 4, "3", NULL, game_str_tbl_oppon[2] },
        { 5, "4", NULL, game_str_tbl_oppon[3] },
        { 6, "5", NULL, game_str_tbl_oppon[4] },
        { -1, "Q", NULL, "(quit)" },
        { 0, NULL, NULL, NULL },
    };

    if ((v = ui_input_list("Galaxy size", "> ", ng_galaxy_in)) < 0) {
        return false;
    }
    newopts->galaxy_size = v;
    if ((v = ui_input_list("Difficulty", "> ", ng_diffic_in)) < 0) {
        return false;
    }
    newopts->difficulty = v;
    if ((v = ui_input_list("Opponents", "> ", ng_oppon_in)) < 0) {
        return false;
    }
    newopts->players = v;
    if (ui_new_game_choose_race(newopts, PLAYER_0) < 0) {
        return false;
    }
    if (ui_new_game_choose_banner(newopts, PLAYER_0) < 0) {
        return false;
    }
    ui_new_game_pname(newopts, PLAYER_0);
    ui_new_game_hname(newopts, PLAYER_0);
    return ui_new_game_extra(newopts);
}

/* -------------------------------------------------------------------------- */

main_menu_action_t ui_main_menu(struct game_new_options_s *newopts, struct game_new_options_s *customopts, struct game_new_options_s *challengeopts, int *load_game_i_ptr)
{
    struct input_list_s main_menu_in[] = {
        { MAIN_MENU_ACT_CONTINUE_GAME, "C", NULL, game_str_mm_continue },
        { MAIN_MENU_ACT_LOAD_GAME, "L", NULL, game_str_mm_load },
        { MAIN_MENU_ACT_NEW_GAME, "N", NULL, game_str_mm_new },
        { MAIN_MENU_ACT_QUIT_GAME, "Q", NULL, game_str_mm_quit },
        { MAIN_MENU_ACT_TUTOR, NULL, "tutor", NULL },
        { MAIN_MENU_ACT_CUSTOM_GAME, "U", NULL, NULL },
        { 0, NULL, NULL, NULL },
    };

    main_menu_action_t ret = MAIN_MENU_ACT_QUIT_GAME;
    bool flag_done = false;
    while (!flag_done) {
        ret = ui_input_list("Main menu", "> ", main_menu_in);
        switch (ret) {
            case MAIN_MENU_ACT_NEW_GAME:
                flag_done = ui_new_game(newopts);
                break;
            case MAIN_MENU_ACT_CUSTOM_GAME:
                flag_done = ui_new_game(customopts);
                break;
            case MAIN_MENU_ACT_LOAD_GAME:
                {
                    int i;
                    i = ui_load_game();
                    if (i >= 0) {
                        *load_game_i_ptr = i;
                        flag_done = true;
                    }
                }
                break;
            default:
                flag_done = true;
                break;
        }
    }
    return ret;
}
