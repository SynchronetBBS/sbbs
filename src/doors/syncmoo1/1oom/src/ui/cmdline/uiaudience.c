#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_audience.h"
#include "game_str.h"
#include "types.h"
#include "uidefs.h"
#include "uiinput.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

static void ui_audience_show(struct audience_s *au)
{
    const struct game_s *g = au->g;
    const char *mood[] = { " :)", ">:(", " :V" };
    int moodi;
    switch (au->gfxi) {
        default:
        case 0:
            moodi = 2;
            break;
        case 3:
            moodi = 0;
            break;
        case 4:
            moodi = 1;
            break;
    }
    printf("Audience | %s | %s | %s\n", game_str_tbl_race[g->eto[au->pa].race], mood[moodi], au->buf);
}

static int16_t ui_audience_ask(struct audience_s *au)
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
    int num = 0;
    for (int i = 0; i < (AUDIENCE_STR_MAX - 1); ++i) {
        if (au->strtbl[i]) {
            if ((!au->condtbl) || au->condtbl[i]) {
                rl_in[num].value = i;
                rl_in[num].display = au->strtbl[i] + 2 /* skip "[ " */;
                ++num;
            }
        } else {
            break;
        }
    }
    rl_in[num].value = 0;
    rl_in[num].key = NULL;
    rl_in[num].str = NULL;
    rl_in[num].display = NULL;
    ui_audience_show(au);
    return ui_input_list(NULL, "> ", rl_in);
}

/* -------------------------------------------------------------------------- */

void ui_audience_start(struct audience_s *au)
{
    ui_switch_2(au->g, au->ph, au->pa);
}

void ui_audience_show1(struct audience_s *au)
{
    ui_audience_show(au);
}

void ui_audience_show2(struct audience_s *au)
{
    ui_audience_show(au);
}

void ui_audience_show3(struct audience_s *au)
{
    ui_audience_show(au);
}

int16_t ui_audience_ask2a(struct audience_s *au)
{
    return ui_audience_ask(au);
}

int16_t ui_audience_ask2b(struct audience_s *au)
{
    return ui_audience_ask(au);
}

int16_t ui_audience_ask3(struct audience_s *au)
{
    return ui_audience_ask(au);
}

int16_t ui_audience_ask4(struct audience_s *au)
{
    return ui_audience_ask(au);
}

void ui_audience_newtech(struct audience_s *au, int pi)
{
    if (pi == PLAYER_NONE) {
        ui_newtech(au->g, au->ph);
        ui_newtech(au->g, au->pa);
    } else {
        ui_newtech(au->g, pi);
    }
    ui_switch_2(au->g, au->ph, au->pa);
}

void ui_audience_end(struct audience_s *au)
{
    ui_switch_1(au->g, au->ph);
}
