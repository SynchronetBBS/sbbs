#include "config.h"

#include <stdio.h>
#include <string.h>

#include "uitech.h"
#include "comp.h"
#include "game.h"
#include "game_misc.h"
#include "game_str.h"
#include "game_tech.h"
#include "uidefs.h"

/* -------------------------------------------------------------------------- */

static const char slchars[] = "cofprw";

static tech_field_t ui_tech_slider_from_param(struct input_token_s *param)
{
    char c = param->str[0];
    char *p = strchr(slchars, c);
    if (p) {
        return (p - slchars);
    } else {
        return TECH_FIELD_NUM;
    }
}

static void ui_tech_look_sliders(const struct game_s *g, int api)
{
    const empiretechorbit_t *e = &(g->eto[api]);
    const techdata_t *t = &(e->tech);
    for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
        printf("%c%c %12s: %3i ", t->slider_lock[f] ? '*' : ' ', slchars[f], game_str_tbl_te_field[f], t->slider[f]);
        if ((t->percent[f] < 99) || (t->project[f] != 0)) {
            int complpercent;
            complpercent = game_tech_current_research_percent2(g, api, f);
            if (complpercent > 0) {
                if (complpercent < 10) {
                    putchar(' ');
                }
                printf("%i%%", complpercent);
            } else {
                #define LIGHTBULB_CHARS 8
                const char lightbulb[LIGHTBULB_CHARS] = { ' ', '.', ',', '_', 'o', 'O', '?', '!' };
                int pos;
                complpercent = game_tech_current_research_percent1(g, api, f);
                pos = (complpercent * LIGHTBULB_CHARS) / 100;
                putchar(' ');
                putchar(lightbulb[pos]);
                putchar(' ');
            }
            putchar(' ');
            putchar(game_tech_current_research_has_max_bonus(g, api, f) ? 'X' : ' ');
        } else {
            fputs(game_str_te_max, stdout);
        }
        if (t->project[f]) {
            putchar(' ');
            putchar(' ');
            game_tech_get_name(g->gaux, f, t->project[f], ui_data.strbuf, UI_STRBUF_SIZE);
            fputs(ui_data.strbuf, stdout);
        }
        putchar('\n');
    }
}

static void ui_tech_look_field(const struct game_s *g, int api, tech_field_t f)
{
    const techdata_t *t = &(g->eto[api].tech);
    const uint8_t *q = g->srd[api].researchcompleted[f];
    int8_t completed[TECH_PER_FIELD + 3];
    int num = t->completed[f];
    int8_t *p = completed;
    if (f == TECH_FIELD_WEAPON) {
        *p++ = -2;
        *p++ = -1;
    }
    for (int i = 0; i < num; ++i) {
        *p++ = *q++;
    }
    if (f == TECH_FIELD_WEAPON) {
        num += 2;
    }
    if (t->project[f]) {
        *p++ = t->project[f];
        ++num;
    }
    puts(game_str_tbl_te_field[f]);
    for (int i = 0; i < num; ++i) {
        game_tech_get_name(g->gaux, f, completed[i], ui_data.strbuf, UI_STRBUF_SIZE);
        printf("- ");
        fputs(ui_data.strbuf, stdout);
        printf(": ");
        game_tech_get_descr(g->gaux, f, completed[i], ui_data.strbuf, UI_STRBUF_SIZE);
        puts(ui_data.strbuf);
    }
}

/* -------------------------------------------------------------------------- */

int ui_cmd_tech_look(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    if (num_param == 0) {
        ui_tech_look_sliders(g, api);
    } else {
        tech_field_t f = ui_tech_slider_from_param(param);
        if (f == TECH_FIELD_NUM) {
            return -1;
        }
        ui_tech_look_field(g, api, f);
    }
    return 0;
}

int ui_cmd_tech_slider(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    techdata_t *t = &(g->eto[api].tech);
    tech_field_t f = ui_tech_slider_from_param(param);
    int v;
    if (f == TECH_FIELD_NUM) {
        return -1;
    }
    if (param[1].type == INPUT_TOKEN_NUMBER) {
        v = param[1].data.num;
    } else if (param[1].type == INPUT_TOKEN_RELNUMBER) {
        v = t->slider[f] + param[1].data.num;
    } else {
        return -1;
    }
    SETRANGE(v, 0, 100);
    if (!t->slider_lock[f]) {
        t->slider[f] = v;
        game_adjust_slider_group(t->slider, f, t->slider[f], TECH_FIELD_NUM, t->slider_lock);
    }
    return 0;
}

int ui_cmd_tech_slider_lock(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    techdata_t *t = &(g->eto[api].tech);
    tech_field_t f = ui_tech_slider_from_param(param);
    if (f == TECH_FIELD_NUM) {
        return -1;
    }
    t->slider_lock[f] = !t->slider_lock[f];
    return 0;
}

int ui_cmd_tech_equals(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    techdata_t *t = &(g->eto[api].tech);
    game_equalize_slider_group(t->slider, TECH_FIELD_NUM, t->slider_lock);
    return 0;
}
