#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_news.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_stat.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

void game_news_get_msg(const struct game_s *g, struct news_s *ns, char *buf, size_t buf_size)
{
    const char *msg = EVENTMSG_PTR(g->gaux, ns->type, ns->subtype);
    struct strbuild_s strbuild = strbuild_init(buf, buf_size);
    for (int i = 0; (i < EVENTMSG_LEN) && (msg[i] != 0); ++i) {
        char c;
        c = msg[i];
        /* Fill in placeholders. */
        if (c & 0x80) {
            char techname[64];
            switch (c & 0x7f) {
                case 0:
                    strbuild_catf(&strbuild, "%s", g->planet[ns->planet_i].name);
                    break;
                case 1:
                    strbuild_catf(&strbuild, "%i", ns->num1);
                    break;
                case 2:
                    strbuild_catf(&strbuild, "%i", ns->num2);
                    break;
                case 3:
                    if (ns->num1 != 1) {
                        strbuild_append_char(&strbuild, 's');
                    }
                    break;
                case 0xe:
                    if ((ns->num1 == 2) || (ns->num1 == 3)) {
                        strbuild_append_char(&strbuild, 'n');
                    }
                    strbuild_append_char(&strbuild, ' ');
                    break;
                case 0xd:
                    if (ns->num1 == 1) {
                        strbuild_append_char(&strbuild, 's');
                    }
                    break;
                case 4:
                    strbuild_catf(&strbuild, "%s", game_str_tbl_race[ns->race]);
                    break;
                case 5:
                    strbuild_catf(&strbuild, "%s", game_str_tbl_race[g->eto[ns->num2].race]);
                    break;
                case 6:
                    strbuild_catf(&strbuild, "%s", game_str_tbl_te_field[ns->num1]);
                    break;
                case 7:
                    game_tech_get_name(g->gaux, ns->num2 / 50, ns->num2 % 50, techname, sizeof(techname));
                    strbuild_catf(&strbuild, "%s", techname);
                    break;
                case 8:
                    strbuild_catf(&strbuild, "%s", game_str_tbl_trait1[ns->num1]);
                    break;
                case 9:
                    strbuild_catf(&strbuild, "%s", game_str_tbl_trait2[ns->num2]);
                    break;
                case 0xc:
                    strbuild_catf(&strbuild, "%s", g->emperor_names[ns->num1]);
                    break;
                default:
                    strbuild_append_char(&strbuild, c);
                    break;
            }
        } else {
            strbuild_append_char(&strbuild, c);
        }
    }
    if (ns->type == GAME_NEWS_STATS) {
        int num = 0;
        int tbl_player[PLAYER_NUM];
        int tbl_stat[PLAYER_NUM];
        for (int i = 0; i < PLAYER_NUM; ++i) {
            ns->stats[i] = 0;
        }
        for (int i = 0; i < g->players; ++i) {
            if (g->evn.home[i] != PLANET_NONE) {
                int v;
                tbl_player[num] = i;
                switch (ns->subtype) {
                    case 0:
                        v = game_stat_prod(g, i);
                        SETMAX(v, 1);
                        break;
                    case 1:
                        v = game_stat_pop(g, i);
                        SETMAX(v, 1);
                        break;
                    case 2:
                        v = game_stat_tech(g, i);
                        SETMAX(v, 1);
                        break;
                    case 3:
                        v = game_stat_fleet(g, i);
                        SETMAX(v, 1);
                        break;
                    default:
                        v = 0;
                        break;
                }
                tbl_stat[num] = v;
                if (v != 0) {
                    ++num;
                }
            }
        }
        ns->statsnum = num;
        for (int loops = 0; loops < num; ++loops) {
            for (int i = 0; i < (num - 1); ++i) {
                int v0, v1;
                v0 = tbl_stat[i];
                v1 = tbl_stat[i + 1];
                if (v0 < v1) {
                    tbl_stat[i] = v1;
                    tbl_stat[i + 1] = v0;
                    v0 = tbl_player[i]; tbl_player[i] = tbl_player[i + 1]; tbl_player[i + 1] = v0;
                }
            }
        }
        for (int i = 0; i < num; ++i) {
            ns->stats[i] = game_str_tbl_race[g->eto[tbl_player[i]].race];
        }
    } else {
        ns->statsnum = 0;
    }
}
