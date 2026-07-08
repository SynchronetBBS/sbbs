#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "uiraces.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_audience.h"
#include "game_diplo.h"
#include "game_str.h"
#include "lib.h"
#include "uidefs.h"
#include "uiinput.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

player_id_t ui_player_from_param(const struct game_s *g, const struct input_token_s *param)
{
    char *shortcuts[PLAYER_NUM * 2];
    player_id_t player = PLAYER_NONE;
    for (int i = 0; i < g->players; ++i) {
        const char *s;
        char *p;
        char c;
        s = game_str_tbl_race[g->eto[i].race];
        p = shortcuts[i * 2 + 0] = lib_stralloc(s);
        while ((c = *p) != '\0') {
            *p++ = tolower(c);
        }
        s = g->emperor_names[i];
        p = shortcuts[i * 2 + 1] = lib_stralloc(s);
        while ((c = *p) != '\0') {
            *p++ = tolower(c);
        }
    }
    for (int i = 0; i < (g->players * 2); ++i) {
        int len;
        bool is_unique;
        is_unique = false;
        len = 0;
        while (!is_unique && shortcuts[i][++len]) {
            is_unique = true;
            for (int j = 0; j < (g->players * 2); ++j) {
                if ((i != j) && (strncmp(shortcuts[i], shortcuts[j], len) == 0)) {
                    is_unique = false;
                    break;
                }
            }
        }
        shortcuts[i][len] = '\0';
    }
    /* TODO check if over 1 len shortcuts point to same player, for example Klackon Klaquan */
    for (int i = 0; i < (g->players * 2); ++i) {
        if (ui_input_match_input(param->str, NULL, shortcuts[i])) {
            player = i / 2;
            break;
        }
    }
    for (int i = 0; i < (g->players * 2); ++i) {
        lib_free(shortcuts[i]);
    }
    return player;
}

int ui_cmd_audience(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
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
    if (g->end != GAME_END_NONE) {
        return 0;
    }
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if ((pi != api) && BOOLVEC_IS1(e->contact, pi) && IS_ALIVE(g, pi)) {
            if (!game_diplo_is_gone(g, api, pi)) {
                rl_in[num].value = pi;
                rl_in[num].display = game_str_tbl_race[g->eto[pi].race];
                ++num;
            }
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
        v = ui_input_list("Choose race to have an audience with", "> ", rl_in);
        if (v >= 0) {
            game_audience(g, api, v);
        }
    } else {
        printf("No one to have an audience with\n");
    }
    return 0;
}

int ui_cmd_races(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    const empiretechorbit_t *e = &(g->eto[api]);
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if ((pi != api) && BOOLVEC_IS1(e->contact, pi) && IS_ALIVE(g, pi)) {
            const empiretechorbit_t *eo = &(g->eto[pi]);
            printf("- %s, %s", game_str_tbl_race[eo->race], g->emperor_names[pi]);
            {
                int rel, reltxti;
                rel = e->relation1[pi];
                reltxti = rel / 12 + 8;
                SETRANGE(reltxti, 0, 16);
                SETRANGE(rel, -100, 100);
                printf(", %+3i %s, %s, ", rel, game_str_tbl_ra_relat[reltxti], game_str_tbl_ra_treaty[e->treaty[pi]]);
            }
            if (e->trade_bc[pi] != 0) {
                printf("%s: %i %s/%s", game_str_ra_trade, e->trade_bc[pi], game_str_bc, game_str_year);
            } else {
                fputs(game_str_ra_notrade, stdout);
            }
            if (game_diplo_is_gone(g, api, pi)) {
                printf(", %s %s", game_str_ra_diplo, game_str_ra_gone);
            }
            {
                int spying, spyprod, spyspend, spies, spycost;
                spies = e->spies[pi];
                if (spies == 0) {
                    printf(", %s", game_str_ra_nospies);
                } else {
                    printf(", %i %s", spies, (spies == 1) ? game_str_ra_spy : game_str_ra_spies);
                }
                spying = e->spying[pi];
                printf(", %i.%i%%, ", spying / 10, spying % 10);
                spyprod = (e->total_production_bc * spying) / 1000;
                spyspend = spyprod + e->spyfund[pi];
                spycost = e->tech.percent[TECH_FIELD_COMPUTER] * 2 + 25;
                if (e->race == RACE_DARLOK) {
                    spycost /= 2;
                }
                if (spycost <= spyspend) {
                    spies = 0;
                    while (spycost <= spyspend) {
                        ++spies;
                        spyspend -= spycost;
                        spycost *= 2;
                    }
                    printf("%i%c%s", spies, (spies == 1) ? ' ' : '/', game_str_y);
                } else {
                    if (spyprod == 0) {
                        fputs(game_str_st_none, stdout);
                    } else {
                        int left, years;
                        left = spycost - spyspend;
                        years = left / spyprod;
                        if (left % spyprod) {
                            ++years;
                        }
                        printf("%i %s", years, game_str_y);
                    }
                }
            }
            switch (e->spymode[pi]) {
                case SPYMODE_HIDE:
                    puts(", hide.");
                    break;
                case SPYMODE_ESPIONAGE:
                    puts(", espionage.");
                    break;
                case SPYMODE_SABOTAGE:
                    puts(", sabotage.");
                    break;
            }
        }
    }
    return 0;
}

int ui_cmd_spy(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    empiretechorbit_t *e = &(g->eto[api]);
    player_id_t pi = ui_player_from_param(g, param);
    if (pi == PLAYER_NONE) {
        printf("Unknown player\n");
        return -1;
    }
    if (pi == api) {
        printf("Will not spy self\n");
        return -1;
    }
    if (param[1].type == INPUT_TOKEN_NUMBER) {
        int v = param[1].data.num;
        SETRANGE(v, 0, 100);
        e->spying[pi] = v;
    } else if (param[1].type == INPUT_TOKEN_RELNUMBER) {
        int v = param[1].data.num;
        v += e->spying[pi];
        SETRANGE(v, 0, 100);
        e->spying[pi] = v;
    } else {
        char c;
        c = tolower(param[1].str[0]);
        if (c == 'h') {
            e->spymode[pi] = SPYMODE_HIDE;
        } else if (c == 'e') {
            e->spymode[pi] = SPYMODE_ESPIONAGE;
        } else if (c == 's') {
            e->spymode[pi] = SPYMODE_SABOTAGE;
        } else {
            printf("Invalid spy mode\n");
            return -1;
        }
    }
    return 0;
}
