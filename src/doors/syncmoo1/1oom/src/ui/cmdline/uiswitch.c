#include "config.h"

#include <stdio.h>

#include "uiswitch.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_str.h"
#include "uicmds.h"
#include "uidefs.h"
#include "uiinput.h"

/* -------------------------------------------------------------------------- */

#define UI_SWITCH_BLANK_LINES   80

/* -------------------------------------------------------------------------- */

static bool ui_switch(const struct game_s *g, player_id_t *tbl_pi, int num_pi, bool allow_opts)
{
    for (int i = 0; i < UI_SWITCH_BLANK_LINES; ++i) {
        putchar('\n');
    }
    printf("*** %i\n", g->year + YEAR_BASE);
    for (int i = 0; i < num_pi; ++i) {
        player_id_t pi;
        pi = tbl_pi[i];
        printf("* %s %i: %s\n", game_str_player, pi + 1, g->emperor_names[pi]);
    }
    /* TODO if (allow_opts) { save, load, quit ... } */
    ui_input_wait_enter();
    return false;
}

/* -------------------------------------------------------------------------- */

bool ui_switch_1_opts(const struct game_s *g, player_id_t pi)
{
    BOOLVEC_DECLARE(viewing, PLAYER_NUM);
    if ((g->gaux->local_players <= 1) || IS_AI(g, pi)) {
        return false;
    }
    BOOLVEC_CLEAR(viewing, PLAYER_NUM);
    BOOLVEC_SET1(viewing, pi);
    if (!BOOLVEC_COMP(ui_data.players_viewing, viewing, PLAYER_NUM)) {
        BOOLVEC_COPY(ui_data.players_viewing, viewing, PLAYER_NUM);
        return ui_switch(g, &pi, 1, true);
    }
    return false;
}

void ui_switch_1(const struct game_s *g, player_id_t pi)
{
    BOOLVEC_DECLARE(viewing, PLAYER_NUM);
    if ((g->gaux->local_players <= 1) || IS_AI(g, pi)) {
        return;
    }
    BOOLVEC_CLEAR(viewing, PLAYER_NUM);
    BOOLVEC_SET1(viewing, pi);
    if (!BOOLVEC_COMP(ui_data.players_viewing, viewing, PLAYER_NUM)) {
        BOOLVEC_COPY(ui_data.players_viewing, viewing, PLAYER_NUM);
        ui_switch(g, &pi, 1, false);
    }
}

bool ui_switch_2(const struct game_s *g, player_id_t pi1, player_id_t pi2)
{
    player_id_t tbl[2];
    int n = 0;
    BOOLVEC_DECLARE(viewing, PLAYER_NUM);
    if (g->gaux->local_players <= 1) {
        return false;
    }
    BOOLVEC_CLEAR(viewing, PLAYER_NUM);
    if (IS_HUMAN(g, pi1) && IS_ALIVE(g, pi1)) {
        BOOLVEC_SET1(viewing, pi1);
        tbl[n++] = pi1;
    }
    if (IS_HUMAN(g, pi2) && IS_ALIVE(g, pi2)) {
        BOOLVEC_SET1(viewing, pi2);
        tbl[n++] = pi2;
    }
    if ((n == 2) && (pi1 > pi2)) {
        tbl[0] = pi2;
        tbl[1] = pi1;
    }
    if (!BOOLVEC_COMP(ui_data.players_viewing, viewing, PLAYER_NUM)) {
        BOOLVEC_COPY(ui_data.players_viewing, viewing, PLAYER_NUM);
        ui_switch(g, tbl, n, false);
        return true;
    }
    return false;
}

void ui_switch_all(const struct game_s *g)
{
    player_id_t tbl[PLAYER_NUM];
    int n = 0;
    BOOLVEC_DECLARE(viewing, PLAYER_NUM);
    if (g->gaux->local_players <= 1) {
        return;
    }
    BOOLVEC_CLEAR(viewing, PLAYER_NUM);
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if (IS_HUMAN(g, pi) && IS_ALIVE(g, pi)) {
            tbl[n++] = pi;
        }
    }
    if (!BOOLVEC_COMP(ui_data.players_viewing, viewing, PLAYER_NUM)) {
        BOOLVEC_COPY(ui_data.players_viewing, viewing, PLAYER_NUM);
        ui_switch(g, tbl, n, false);
    }
}

void ui_switch_wait(const struct game_s *g)
{
    if (g->gaux->local_players > 1) {
        ui_input_wait_enter();
    }
}
