#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_ground.h"
#include "game_spy.h"
#include "game_str.h"
#include "game_tech.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

static void ui_ground_print_troops(struct ground_s *gr, const char **strrace)
{
    printf("Ground | %i %s %s", gr->s[1].pop1, strrace[1], game_str_gr_troops);
    for (int i = 0; i < gr->s[1].strnum; ++i) {
        printf(", %s", gr->s[1].str[i]);
    }
    putchar('\n');
    printf("Ground | %i %s %s", gr->s[0].pop1, gr->flag_rebel ? game_str_gr_rebel : strrace[0], game_str_gr_troops);
    for (int i = 0; i < gr->s[0].strnum; ++i) {
        printf(", %s", gr->s[0].str[i]);
    }
    putchar('\n');
}

/* -------------------------------------------------------------------------- */

void ui_ground(struct game_s *g, struct ground_s *gr)
{
    const char *strrace[2];
    ui_switch_2(g, gr->s[0].player, gr->s[1].player);
    for (int i = 0; i < 2; ++i) {
        strrace[i] = game_str_tbl_race[g->eto[gr->s[i].player].race];
    }
    fputs("Ground | ", stdout);
    printf("%s %s\n", game_str_gr_gcon, g->planet[gr->planet_i].name);
    fputs("Ground | ", stdout);
    if (!gr->flag_rebel) {
        printf("%i %s %i %s %s ", gr->inbound, game_str_gr_outof, gr->total_inbound, strrace[gr->flag_swap ? 1 : 0], game_str_gr_transs);
        printf("%s %s %s\n", game_str_gr_penetr, strrace[gr->flag_swap ? 0 : 1], game_str_gr_defenss);
    } else {
        printf("%i %s\n", gr->inbound, game_str_gr_reclaim);
    }
    ui_ground_print_troops(gr, strrace);
    fputs("Ground | ", stdout);
    printf("(*BANG* *ZZZAP* etc)\n");
    while ((gr->s[0].pop1 != 0) && (gr->s[1].pop1 != 0)) {
        game_ground_kill(gr);
    }
    ui_ground_print_troops(gr, strrace);
    {
        int pop = gr->s[gr->flag_swap ? 1 : 0].pop1;
        fputs("Ground | ", stdout);
        if (pop != 0) {
            if (!gr->flag_swap) {
                printf("%s%s ", strrace[0], game_str_gr_scapt);
            } else {
                if (gr->flag_rebel) {
                    printf("%s ", game_str_gr_itroops);
                } else {
                    printf("%s%s ", strrace[1], game_str_gr_scapt);
                }
            }
        } else {
            const char *s;
            if (!gr->flag_swap) {
                s = strrace[1];
            } else {
                if (gr->flag_rebel) {
                    s = game_str_gr_rebel;
                } else {
                    s = strrace[0];
                }
            }
            printf("%s%s ", s, game_str_gr_succd);
        }
        printf("%s\n", g->planet[gr->planet_i].name);
        if ((pop > 0) && (!gr->flag_rebel)) {
            if (gr->fact > 0) {
                fputs("Ground | ", stdout);
                printf("%i %s\n", gr->fact, game_str_gr_fcapt);
            }
            if (gr->techchance > 0) {
                fputs("Ground | ", stdout);
                if (gr->flag_swap == gr->s[0].human) {
                    fputs(game_str_gr_tsteal, stdout);
                    for (int i = 0; i < gr->techchance; ++i) {
                        char buf[0x80];
                        game_tech_get_name(g->gaux, gr->got[i].field, gr->got[i].tech, buf, sizeof(buf));
                        printf("%s %s", (i > 0) ? "," : ":", buf);
                    }
                } else {
                    fputs(game_str_gr_tnew, stdout);
                }
                putchar('\n');
            }
        }
    }
    ui_switch_wait(g);
}
