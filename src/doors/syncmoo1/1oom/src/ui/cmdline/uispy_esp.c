#include "config.h"

#include <ctype.h>
#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "uiinput.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

int ui_spy_steal(struct game_s *g, int spy, int target, uint8_t flags_field)
{
    struct input_list_s rl_in[] = {
        { -1, "1", NULL, NULL },
        { -1, "2", NULL, NULL },
        { -1, "3", NULL, NULL },
        { -1, "4", NULL, NULL },
        { -1, "5", NULL, NULL },
        { -1, "6", NULL, NULL },
        { 0, NULL, NULL, NULL },
        { 0, NULL, NULL, NULL }
    };
    int num = 0, v = -1;
    ui_switch_1(g, spy);
    {
        char rbuf[0x20], *p, c;
        const empiretechorbit_t *e = &(g->eto[target]);
        bool usean = false;
        lib_strcpy(rbuf, game_str_tbl_race[e->race], sizeof(rbuf));
        p = rbuf;
        while ((c = *p) != 0) {
            if (islower(c)) {
                c = toupper(c);
                *p = c;
            }
            if ((c == 'A') || (c == 'E') || (c == 'I') || (c == 'O') || (c == 'U')) {
                usean = true;
            }
            ++p;
        }
        printf("%s%s %s %s. ", game_str_es_youresp1, usean ? "N" : "", rbuf, game_str_es_youresp2);
    }
    for (int i = 0; i < TECH_FIELD_NUM; ++i) {
        if (flags_field & (1 << i)) {
            rl_in[num].value = i;
            rl_in[num].display = game_str_tbl_te_field[i];
            ++num;
        }
    }
    if (num > 0) {
        rl_in[num].value = -1;
        rl_in[num].key = "Q";
        rl_in[num].str = "q";
        rl_in[num].display = "(quit)";
        ++num;
        rl_in[num].value = 0;
        rl_in[num].key = NULL;
        rl_in[num].str = NULL;
        rl_in[num].display = NULL;
        v = ui_input_list(game_str_es_youresp3, "> ", rl_in);
    }
    return v;
}

void ui_spy_stolen(struct game_s *g, int pi, int spy, int field, uint8_t tech)
{
    const char *s;
    char buf[0x80];
    if (spy != PLAYER_NONE) {
        s = game_str_tbl_race[g->eto[spy].race];
    } else {
        s = game_str_es_unkn;
    }
    ui_switch_1(g, pi);
    printf("%s %s | %s %s %s\n", s, game_str_es_thesp1, s, game_str_es_thesp2, game_tech_get_name(g->gaux, field, tech, buf, sizeof(buf)));
    ui_switch_wait(g);
}
