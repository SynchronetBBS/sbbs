#include "config.h"

#include <stdio.h>
#include <string.h>

#include "uiempirereport.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "uidefs.h"
#include "uiinput.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static void show_empire_report(const struct game_s *g, player_id_t api, player_id_t pi)
{
    const empiretechorbit_t *eapi = &(g->eto[api]);
    const empiretechorbit_t *e = &(g->eto[pi]);
    const shipresearch_t *srd = &(g->srd[pi]);
    char *buf = ui_data.strbuf;
    printf("Report: %s, %s, %s %s. %s ", game_str_tbl_races[e->race], g->emperor_names[pi], game_str_tbl_trait1[e->trait1], game_str_tbl_trait2[e->trait2], game_str_re_reportis);
    {
        int reportage = g->year - eapi->spyreportyear[pi] - 1;
        if (reportage < 2) {
            fputs(game_str_re_current, stdout);
        } else {
            printf("%i %s", reportage, game_str_re_yearsold);
        }
        putchar('\n');
    }
    {
        bool first = true;
        printf("%s:", game_str_re_alliance);
        for (int i = 0; i < g->players; ++i) {
            if ((i != pi) && (e->treaty[i] == TREATY_ALLIANCE) && IS_ALIVE(g, i)) {
                if (!first) {
                    putchar(',');
                }
                putchar(' ');
                first = false;
                fputs(game_str_tbl_races[g->eto[i].race], stdout);
            }
        }
        putchar('\n');
    }
    {
        bool first = true;
        printf("%s:", game_str_re_wars);
        for (int i = 0; i < g->players; ++i) {
            if ((i != pi) && (e->treaty[i] >= TREATY_WAR) && IS_ALIVE(g, i)) {
                if (!first) {
                    putchar(',');
                }
                putchar(' ');
                first = false;
                fputs(game_str_tbl_races[g->eto[i].race], stdout);
            }
        }
        putchar('\n');
    }
    for (int f = 0; f < TECH_FIELD_NUM; ++f) {
        const uint8_t *rct = &(srd->researchcompleted[f][0]);
        uint16_t tc;
        uint8_t first, num, rf;
        tc = e->tech.completed[f];
        rf = eapi->spyreportfield[pi][f];
        num = 0;
        for (int i = 0; i < tc; ++i) {
            uint8_t rc;
            rc = rct[i];
            if (rc <= rf) {
                num = i + 1;
            }
        }
        if (num > 8) {
            first = num - 8;
            num = 8;
        } else {
            first = 0;
        }
        puts(game_str_tbl_te_field[f]);
        for (int i = 0; i < num; ++i) {
            uint8_t rc;
            rc = rct[first + i];
            game_tech_get_name(g->gaux, f, rc, buf, UI_STRBUF_SIZE);
            putchar(game_tech_player_has_tech(g, f, rc, api) ? '-' : '!');
            putchar(' ');
            puts(buf);
        }
    }
}

/* -------------------------------------------------------------------------- */

int ui_cmd_empirereport(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct input_list_s rl_in[] = {
        { -1, "1", NULL, NULL },
        { -1, "2", NULL, NULL },
        { -1, "3", NULL, NULL },
        { -1, "4", NULL, NULL },
        { -1, "5", NULL, NULL },
        { 0, NULL, NULL, NULL },
        { 0, NULL, NULL, NULL }
    };
    const empiretechorbit_t *e = &(g->eto[api]);
    int num = 0;
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if ((pi != api) && BOOLVEC_IS1(e->contact, pi) && IS_ALIVE(g, pi)) {
            rl_in[num].value = pi;
            rl_in[num].display = game_str_tbl_race[g->eto[pi].race];
            ++num;
        }
    }
    if (num > 0) {
        int v;
        rl_in[num].value = -1;
        rl_in[num].key = "Q";
        rl_in[num].str = "q";
        rl_in[num].display = "(quit)";
        ++num;
        rl_in[num].value = 0;
        rl_in[num].key = NULL;
        rl_in[num].str = NULL;
        rl_in[num].display = NULL;
        v = ui_input_list("Choose race", "> ", rl_in);
        if (v >= 0) {
            show_empire_report(g, api, v);
        }
    } else {
        puts("No races to report on");
    }
    return 0;
}
