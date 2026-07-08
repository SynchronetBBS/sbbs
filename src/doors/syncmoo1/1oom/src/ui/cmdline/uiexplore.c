#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_str.h"
#include "lib.h"
#include "uiinput.h"
#include "uiplanet.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

bool ui_explore(struct game_s *g, int pi, uint8_t planet_i, bool by_scanner, bool flag_colony_ship)
{
    ui_switch_1(g, pi);
    if (!flag_colony_ship) {
        if (by_scanner) {
            printf("%s %s %s %s\n", game_str_ex_planeta, game_str_ex_scanner, game_str_ex_explore, game_str_ex_starsys);
        } else {
            printf("%s %s %s\n", game_str_ex_scout, game_str_ex_explore, game_str_ex_starsys);
        }
    }
    ui_planet_look(g, pi, planet_i, true);
    if (flag_colony_ship) {
        char buf[0x80];
        int v;
        lib_sprintf(buf, sizeof(buf), "%s %s", game_str_ex_build, game_str_ex_colony);
        v = ui_input_list(buf, "> ", il_yes_no);
        flag_colony_ship = (v == 1);
        if (flag_colony_ship) {
            char *name;
            printf("%s %i %s %s%s: (%s) ", game_str_la_inyear, g->year + YEAR_BASE, game_str_la_the, game_str_tbl_race[g->eto[pi].race], game_str_la_formnew, g->planet[planet_i].name);
            name = ui_input_line_len_trim("> ", PLANET_NAME_LEN);
            if (name[0] != '\0') {
                lib_strcpy(g->planet[planet_i].name, name, PLANET_NAME_LEN);
            }
        }
    } else {
        ui_switch_wait(g);
    }
    return flag_colony_ship;
}
