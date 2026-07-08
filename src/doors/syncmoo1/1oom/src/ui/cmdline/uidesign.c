#include "config.h"

#include <stdio.h>

#include "uidesign.h"
#include "comp.h"
#include "game.h"
#include "game_design.h"
#include "game_shiptech.h"
#include "game_str.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uicmds.h"
#include "uidefs.h"
#include "uifleet.h"
#include "uihelp.h"
#include "uiinput.h"
#include "uispecs.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static int cmd_exit(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return (int)(intptr_t)var;
}

static int cmd_look(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    const struct design_data_s *d = var;
    const struct game_design_s *gd = d->gd;
    const shipdesign_t *sd = &(gd->sd);
    uint8_t extraman;
    {
        uint8_t v;
        extraman = 0;
        for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
            v = tbl_shiptech_special[sd->special[i]].extraman;
            SETMAX(extraman, v);
        }
    }
    {
        uint16_t v = 0;
        for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
            v |= tbl_shiptech_special[sd->special[i]].boolmask;
        }
        v = tbl_shiptech_comp[sd->comp].level + ((v & (1 << SHIP_SPECIAL_BOOL_SCANNER)) ? 1 : 0);
        printf("%c%s: %s, %s %u\n", d->flag_disable_comp ? '-' : ' ', game_str_sd_comp, *tbl_shiptech_comp[sd->comp].nameptr, game_str_sd_att, v);
    }
    {
        printf("%c%s: %s", d->flag_disable_shield ? '-' : ' ', game_str_sd_shield, *tbl_shiptech_shield[sd->shield].nameptr);
        if (sd->shield) {
            uint8_t v = tbl_shiptech_shield[sd->shield].absorb;
            printf(", %s %i %s", game_str_sd_absorbs, v, (v == 1) ? game_str_sd_hit : game_str_sd_hits);
        }
        putchar('\n');
    }
    {
        uint8_t v = extraman + sd->man + tbl_shiptech_jammer[sd->jammer].level + tbl_shiptech_hull[sd->hull].defense + 1;
        printf("%c%s: %s, %s %u\n", d->flag_disable_jammer ? '-' : ' ', game_str_sd_ecm, *tbl_shiptech_jammer[sd->jammer].nameptr, game_str_sd_misdef, v);
    }
    printf("%c%s: %s, %s %i\n", d->flag_disable_armor ? '-' : ' ', game_str_sd_armor, *tbl_shiptech_armor[sd->armor].nameptr, game_str_sd_hp, sd->hp);
    {
        uint8_t def, warp = tbl_shiptech_engine[sd->engine].warp;
        def = extraman + sd->man + tbl_shiptech_hull[sd->hull].defense + 1;
        printf("%c%s: %s, %s %u, %s %u\n", d->flag_disable_engine ? '-' : ' ', game_str_sd_engine, *tbl_shiptech_engine[sd->engine].nameptr, game_str_sd_warp, warp, game_str_sd_def, def);
    }
    {
        uint8_t cspeed, man = extraman + sd->man + 1;
        cspeed = (extraman / 2) + (sd->man + 3) / 2;
        printf("%c%s: %u, %s %u\n", d->flag_disable_cspeed ? '-' : ' ', game_str_sd_man, man, game_str_sd_cspeed, cspeed);
    }
    for (int i = 0; i < WEAPON_SLOT_NUM; ++i) {
        weapon_t wi;
        wi = sd->wpnt[i];
        printf("%c%s:", d->flag_tbl_weapon[i] ? '-' : ' ', game_str_tbl_sd_weap[i]);
        if (wi != WEAPON_NONE) {
            const char *str;
            int dmin, dmax;
            dmin = tbl_shiptech_weap[wi].damagemin;
            dmax = tbl_shiptech_weap[wi].damagemax;
            printf(" %c%c %2i x %s, %s ", d->flag_tbl_weap_dn[i] ? ' ' : '-', d->flag_tbl_weap_up[i] ? ' ' : '+', sd->wpnn[i], *tbl_shiptech_weap[wi].nameptr, game_str_sd_damage);
            if (dmin != dmax) {
                printf("%i-", dmin);
            }
            printf("%i, %s %i", dmax, game_str_sd_rng, tbl_shiptech_weap[wi].range);
            str = *tbl_shiptech_weap[wi].extratextptr;
            if (str && *str) {
                printf(", %s", str);
            }
        }
        putchar('\n');
    }
    for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
        ship_special_t si;
        si = sd->special[i];
        printf("%c%s:", d->flag_tbl_special[i] ? '-' : ' ', game_str_tbl_sd_spec[i]);
        if (si != SHIP_SPECIAL_NONE) {
            printf(" %s", *tbl_shiptech_special[si].nameptr);
        }
        putchar('\n');
    }
    printf(" '%s', %s, %s %i %s, %s %i/%i\n", sd->name, *tbl_shiptech_hull[sd->hull].nameptr, game_str_sd_cost, sd->cost, game_str_bc, game_str_sd_space, sd->space, game_design_get_hull_space(gd));
    return 0;
}

static int cmd_clear(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct design_data_s *d = var;
    struct game_design_s *gd = d->gd;
    game_design_clear(gd);
    return 0;
}

/* -------------------------------------------------------------------------- */

struct sel_shiptech_s {
    struct design_data_s *d;
    const bool *flag_enable;
    const int8_t *havebuf;
    int oldv, slot;
};

#define SEL_LIST_MAX    33

static const char *selstrs[SEL_LIST_MAX] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", /* leave  Q for Quit */ "R", "S", "T", "U",
    "V", "W", "X" /* Y/Z for prev/next */
};

static void sel_build_il(struct input_list_s *il, bool *flag_enable, const int8_t *havebuf, int havelast, int offs)
{
    int i, n = 0;
    for (i = 0; i <= havelast; ++i) {
        flag_enable[i] = (havebuf[i] > 0);
    }
    for (i = offs; (i <= havelast) && (n < SEL_LIST_MAX); ++i) {
        if (havebuf[i] >= 0) {
            il[n].value = i;
            il[n].key = selstrs[n];
            il[n].str = 0;
            il[n].display = 0;
            ++n;
        }
    }
    if (offs > 0) {
        il[n].value = -2/*prev*/;
        il[n].key = "Y";
        il[n].str = 0;
        il[n].display = "(prev)";
        ++n;
    }
    if (i <= havelast) {
        il[n].value = -3/*next*/;
        il[n].key = "Z";
        il[n].str = 0;
        il[n].display = "(next)";
        ++n;
    }
    il[n].value = -1;
    il[n].key = "Q";
    il[n].str = 0;
    il[n].display = "(quit)";
    ++n;
    il[n].value = -4;
    il[n].key = 0;
    il[n].str = 0;
    il[n].display = 0;
}

static bool sel_is_ok(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    int v = l->value;
    return ((v < 0) || sel->flag_enable[v]);
}

static const char *sel_comp_get_display(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    struct design_data_s *d = sel->d;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    int i = l->value, space, cost, space2, power, cost2, sizei;
    if (i < 0) {
        return l->display;
    }
    sd->comp = 0;
    game_design_update_engines(sd);
    space = game_design_calc_space(gd);
    cost = game_design_calc_cost(gd);
    sd->comp = i;
    game_design_update_engines(sd);
    space2 = space - game_design_calc_space(gd);
    power = tbl_shiptech_comp[i].power[sd->hull];
    cost2 = game_design_calc_cost(gd) - cost;
    sizei = game_design_calc_space_item(gd, DESIGN_SLOT_COMP, i);
    lib_sprintf(ui_data.strbuf, UI_STRBUF_SIZE, "%s%s, %s %i, %s %i, %s %i, %s %i", (i == sel->oldv) ? "*" : "", *tbl_shiptech_comp[i].nameptr, game_str_sd_cost, cost2, game_str_sd_size, sizei, game_str_sd_power, power, game_str_sd_space, space2);
    return ui_data.strbuf;
}

static int cmd_sel_comp(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct design_data_s *d = var;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    struct input_list_s il[SHIP_COMP_NUM + 2];
    bool flag_enable[SHIP_COMP_NUM];
    int oldv = sd->comp;
    {
        int8_t havebuf[SHIP_COMP_NUM];
        int havelast;
        sd->comp = 0;
        game_design_update_engines(sd);
        havelast = game_design_build_tbl_fit_comp(d->g, gd, havebuf);
        sel_build_il(il, flag_enable, havebuf, havelast, 0);
    }
    {
        struct sel_shiptech_s sel = { .d = d, .flag_enable = flag_enable, .oldv = oldv };
        struct input_list_dyn_s ld = { .list = il, .ctx = &sel, .is_ok = sel_is_ok, .get_display = sel_comp_get_display };
        int v = ui_input_list_dynamic(game_str_sd_comp, "> ", &ld);
        sd->comp = (v < 0) ? oldv : v;
    }
    return 0;
}

static const char *sel_shield_get_display(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    struct design_data_s *d = sel->d;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    int i = l->value, space, cost, space2, power, cost2, sizei;
    if (i < 0) {
        return l->display;
    }
    sd->shield = 0;
    game_design_update_engines(sd);
    space = game_design_calc_space(gd);
    cost = game_design_calc_cost(gd);
    sd->shield = i;
    game_design_update_engines(sd);
    space2 = space - game_design_calc_space(gd);
    power = tbl_shiptech_shield[i].power[sd->hull];
    cost2 = game_design_calc_cost(gd) - cost;
    sizei = game_design_calc_space_item(gd, DESIGN_SLOT_SHIELD, i);
    lib_sprintf(ui_data.strbuf, UI_STRBUF_SIZE, "%s%s, %s %i, %s %i, %s %i, %s %i", (i == sel->oldv) ? "*" : "", *tbl_shiptech_shield[i].nameptr, game_str_sd_cost, cost2, game_str_sd_size, sizei, game_str_sd_power, power, game_str_sd_space, space2);
    return ui_data.strbuf;
}

static int cmd_sel_shield(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct design_data_s *d = var;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    struct input_list_s il[SHIP_SHIELD_NUM + 2];
    bool flag_enable[SHIP_SHIELD_NUM];
    int oldv = sd->shield;
    {
        int8_t havebuf[SHIP_SHIELD_NUM];
        int havelast;
        sd->shield = 0;
        game_design_update_engines(sd);
        havelast = game_design_build_tbl_fit_shield(d->g, gd, havebuf);
        sel_build_il(il, flag_enable, havebuf, havelast, 0);
    }
    {
        struct sel_shiptech_s sel = { .d = d, .flag_enable = flag_enable, .oldv = oldv };
        struct input_list_dyn_s ld = { .list = il, .ctx = &sel, .is_ok = sel_is_ok, .get_display = sel_shield_get_display };
        int v = ui_input_list_dynamic(game_str_sd_shield, "> ", &ld);
        sd->shield = (v < 0) ? oldv : v;
    }
    return 0;
}

static const char *sel_jammer_get_display(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    struct design_data_s *d = sel->d;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    int i = l->value, space, cost, space2, power, cost2, sizei;
    if (i < 0) {
        return (i == -1) ? "(quit)" : 0;
    }
    sd->jammer = 0;
    game_design_update_engines(sd);
    space = game_design_calc_space(gd);
    cost = game_design_calc_cost(gd);
    sd->jammer = i;
    game_design_update_engines(sd);
    space2 = space - game_design_calc_space(gd);
    power = tbl_shiptech_jammer[i].power[sd->hull];
    cost2 = game_design_calc_cost(gd) - cost;
    sizei = game_design_calc_space_item(gd, DESIGN_SLOT_JAMMER, i);
    lib_sprintf(ui_data.strbuf, UI_STRBUF_SIZE, "%s%s, %s %i, %s %i, %s %i, %s %i", (i == sel->oldv) ? "*" : "", *tbl_shiptech_jammer[i].nameptr, game_str_sd_cost, cost2, game_str_sd_size, sizei, game_str_sd_power, power, game_str_sd_space, space2);
    return ui_data.strbuf;
}

static int cmd_sel_jammer(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct design_data_s *d = var;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    struct input_list_s il[SHIP_JAMMER_NUM + 2];
    bool flag_enable[SHIP_JAMMER_NUM];
    int oldv = sd->jammer;
    {
        int8_t havebuf[SHIP_JAMMER_NUM];
        int havelast;
        sd->jammer = 0;
        game_design_update_engines(sd);
        havelast = game_design_build_tbl_fit_jammer(d->g, gd, havebuf);
        sel_build_il(il, flag_enable, havebuf, havelast, 0);
    }
    {
        struct sel_shiptech_s sel = { .d = d, .flag_enable = flag_enable, .oldv = oldv };
        struct input_list_dyn_s ld = { .list = il, .ctx = &sel, .is_ok = sel_is_ok, .get_display = sel_jammer_get_display };
        int v = ui_input_list_dynamic(game_str_sd_ecm, "> ", &ld);
        sd->jammer = (v < 0) ? oldv : v;
    }
    return 0;
}

static const char *sel_armor_get_display(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    struct design_data_s *d = sel->d;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    int i = l->value, cost2, sizei;
    if (i < 0) {
        return l->display;
    }
    sd->armor = i;
    game_design_update_engines(sd);
    cost2 = game_design_calc_cost_item(d->gd, DESIGN_SLOT_ARMOR, i);
    sizei = game_design_calc_space_item(d->gd, DESIGN_SLOT_ARMOR, i);
    lib_sprintf(ui_data.strbuf, UI_STRBUF_SIZE, "%s%s, %s %i, %s %i", (i == sel->oldv) ? "*" : "", *tbl_shiptech_armor[i].nameptr, game_str_sd_cost, cost2, game_str_sd_size, sizei);
    return ui_data.strbuf;
}

static int cmd_sel_armor(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct design_data_s *d = var;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    struct input_list_s il[SHIP_ARMOR_NUM + 2];
    bool flag_enable[SHIP_ARMOR_NUM];
    int oldv = sd->armor;
    {
        int8_t havebuf[SHIP_ARMOR_NUM];
        int havelast;
        havelast = game_design_build_tbl_fit_armor(d->g, gd, havebuf);
        sel_build_il(il, flag_enable, havebuf, havelast, 0);
    }
    {
        struct sel_shiptech_s sel = { .d = d, .flag_enable = flag_enable, .oldv = oldv };
        struct input_list_dyn_s ld = { .list = il, .ctx = &sel, .is_ok = sel_is_ok, .get_display = sel_armor_get_display };
        int v = ui_input_list_dynamic(game_str_sd_armor, "> ", &ld);
        sd->armor = (v < 0) ? oldv : v;
    }
    return 0;
}

static const char *sel_engine_get_display(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    struct design_data_s *d = sel->d;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    int i = l->value, cost2, sizei, sizet, ne;
    if (i < 0) {
        return l->display;
    }
    sd->engine = i;
    game_design_update_engines(sd);
    ne = sd->engines;
    sizei = game_design_calc_space_item(d->gd, DESIGN_SLOT_ENGINE, i);
    sizet = (sizei * ne) / 10;
    cost2 = game_design_calc_cost_item(d->gd, DESIGN_SLOT_ENGINE, i);
    sizei = game_design_calc_space_item(gd, DESIGN_SLOT_JAMMER, i);
    lib_sprintf(ui_data.strbuf, UI_STRBUF_SIZE, "%s%s, %s %i, %s %i, %s %i.%i, %s %i", (i == sel->oldv) ? "*" : "", *tbl_shiptech_engine[i].nameptr, game_str_sd_cost, cost2, game_str_sd_size, sizei, game_str_sd_numengs, ne / 10, ne % 10, game_str_sd_space, sizet);
    return ui_data.strbuf;
}

static int cmd_sel_engine(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct design_data_s *d = var;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    struct input_list_s il[SHIP_ENGINE_NUM + 2];
    bool flag_enable[SHIP_ENGINE_NUM];
    int oldv = sd->engine;
    {
        int8_t havebuf[SHIP_ENGINE_NUM];
        int havelast;
        sd->engine = 0;
        game_design_update_engines(sd);
        havelast = game_design_build_tbl_fit_engine(d->g, gd, havebuf);
        sel_build_il(il, flag_enable, havebuf, havelast, 0);
    }
    {
        struct sel_shiptech_s sel = { .d = d, .flag_enable = flag_enable, .oldv = oldv };
        struct input_list_dyn_s ld = { .list = il, .ctx = &sel, .is_ok = sel_is_ok, .get_display = sel_engine_get_display };
        int v = ui_input_list_dynamic(game_str_sd_engine, "> ", &ld);
        sd->engine = (v < 0) ? oldv : v;
    }
    return 0;
}

static const char *sel_man_get_display(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    struct design_data_s *d = sel->d;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    int i = l->value, powperwarp, space, cost, space2, spacet, cost2;
    if (i < 0) {
        return l->display;
    }
    sd->man = 0;
    game_design_update_engines(sd);
    powperwarp = tbl_shiptech_hull[sd->hull].power / tbl_shiptech_engine[sd->engine].warp;
    sd->engines = sd->engines - (powperwarp * 10) / tbl_shiptech_engine[sd->engine].power;
    space = game_design_calc_space(gd);
    cost = game_design_calc_cost(gd);
    sd->man = i;
    game_design_update_engines(sd);
    spacet = game_design_calc_space(gd);
    space2 = space - spacet;
    SETMAX(space2, 1);
    powperwarp = (tbl_shiptech_hull[sd->hull].power * (i + 1)) / tbl_shiptech_engine[sd->engine].warp;
    SETMAX(powperwarp, 1);
    cost2 = game_design_calc_cost(d->gd) - cost;
    SETMAX(cost2, 1);
    lib_sprintf(ui_data.strbuf, UI_STRBUF_SIZE, "%s%s %s, %s %i, %s %i, %s %i, %s %i", (i == sel->oldv) ? "*" : "", game_str_sd_class, game_str_tbl_roman[i + 1], game_str_sd_speed, (i + 3) / 2, game_str_sd_cost, cost2, game_str_sd_power, powperwarp, game_str_sd_space, space2);
    return ui_data.strbuf;
}

static int cmd_sel_man(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct design_data_s *d = var;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    struct input_list_s il[SHIP_ENGINE_NUM + 2];
    bool flag_enable[SHIP_ENGINE_NUM];
    int oldv = sd->man;
    {
        int8_t havebuf[SHIP_ENGINE_NUM];
        int havelast = sd->engine;
        for (int i = 0; i <= havelast; ++i) {
            int spacet;
            sd->man = i;
            game_design_update_engines(sd);
            spacet = game_design_calc_space(d->gd);
            havebuf[i] = (spacet >= 0) ? 1 : 0;
        }
        sel_build_il(il, flag_enable, havebuf, havelast, 0);
    }
    {
        struct sel_shiptech_s sel = { .d = d, .flag_enable = flag_enable, .oldv = oldv };
        struct input_list_dyn_s ld = { .list = il, .ctx = &sel, .is_ok = sel_is_ok, .get_display = sel_man_get_display };
        int v = ui_input_list_dynamic(game_str_sd_man, "> ", &ld);
        sd->man = (v < 0) ? oldv : v;
    }
    return 0;
}

static const char *sel_wpnt_get_display(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    struct design_data_s *d = sel->d;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    int i = l->value, wslot = sel->slot, space, cost, space2, power, cost2, sizei, dmin, dmax;
    if (i < 0) {
        return l->display;
    }
    if (i == 0) {
        return *tbl_shiptech_weap[0].nameptr;
    }
    sd->wpnt[wslot] = 0;
    sd->wpnn[wslot] = 0;
    game_design_update_engines(sd);
    space = game_design_calc_space(gd);
    cost = game_design_calc_cost(gd);
    sd->wpnt[wslot] = i;
    sd->wpnn[wslot] = 1;
    game_design_update_engines(sd);
    space2 = space - game_design_calc_space(gd);
    power = tbl_shiptech_weap[i].power;
    cost2 = game_design_calc_cost(gd) - cost;
    sizei = game_design_calc_space_item(gd, DESIGN_SLOT_WEAPON1, i);
    dmin = tbl_shiptech_weap[i].damagemin;
    dmax = tbl_shiptech_weap[i].damagemax;

    struct strbuild_s str = strbuild_init(ui_data.strbuf, UI_STRBUF_SIZE);
    strbuild_catf(&str, "%s%i x %s", (i == sel->oldv) ? "*" : "", sel->havebuf[i], *tbl_shiptech_weap[i].nameptr);
    if (**tbl_shiptech_weap[i].extratextptr != ' ') {
        strbuild_catf(&str, ": %s", *tbl_shiptech_weap[i].extratextptr);
    }
    strbuild_catf(&str, ", %s %i", game_str_sd_dmg, dmin);
    if (dmin != dmax) {
        strbuild_catf(&str, "-%i", dmax);
    }
    strbuild_catf(&str, ", %s %i, %s %i, %s %i, %s %i", game_str_sd_cost, cost2, game_str_sd_size, sizei, game_str_sd_power, power, game_str_sd_space, space2);
    return ui_data.strbuf;
}

static int cmd_sel_wpnt(struct design_data_s *d, int wslot)
{
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    struct input_list_s il[SEL_LIST_MAX + 4];
    int8_t havebuf[WEAPON_NUM];
    bool flag_enable[WEAPON_NUM];
    int havelast, offs = 0, oldv = sd->wpnt[wslot];
    uint8_t oldn = sd->wpnn[wslot];
    struct sel_shiptech_s sel = { .d = d, .havebuf = havebuf, .flag_enable = flag_enable, .oldv = oldv, .slot = wslot };
    struct input_list_dyn_s ld = { .list = il, .ctx = &sel, .is_ok = sel_is_ok, .get_display = sel_wpnt_get_display };
    sd->wpnt[wslot] = 0;
    game_design_update_engines(sd);
    havelast = game_design_build_tbl_fit_weapon(d->g, gd, havebuf, wslot, WEAPON_GROUP_ALL);
    while (1) {
        int v;
        sel_build_il(il, flag_enable, havebuf, havelast, offs);
        v = ui_input_list_dynamic(game_str_tbl_sd_weap[wslot], "> ", &ld);
        if (v >= 0) {
            uint8_t wn;
            sd->wpnt[wslot] = v;
            wn = havebuf[v];
            SETMIN(wn, oldn);
            SETMAX(wn, 1);
            sd->wpnn[wslot] = wn;
            return 0;
        } else if (v == -1/*quit*/) {
            sd->wpnt[wslot] = oldv;
            sd->wpnn[wslot] = oldn;
            return 0;
        } else if (v == -2/*prev*/) {
            offs -= SEL_LIST_MAX;
            SETMAX(offs, 0);
        } else {
            offs += SEL_LIST_MAX;
            SETMIN(offs, havelast);
        }
    }
}

static int cmd_sel_wpnt1(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_sel_wpnt(var, 0);
}

static int cmd_sel_wpnt2(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_sel_wpnt(var, 1);
}

static int cmd_sel_wpnt3(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_sel_wpnt(var, 2);
}

static int cmd_sel_wpnt4(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_sel_wpnt(var, 3);
}

static int cmd_set_wpnn(struct design_data_s *d, int wslot, struct input_token_s *param)
{
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    int v;
    if (sd->wpnt[wslot] == 0) {
        printf("No weapon in slot %i\n", wslot);
        return -1;
    }
    if (param[0].type == INPUT_TOKEN_NUMBER) {
        v = param[0].data.num;
    } else if (param[0].type == INPUT_TOKEN_RELNUMBER) {
        v = sd->wpnn[wslot] + param[0].data.num;
    } else {
        return -1;
    }
    SETRANGE(v, 0, 99);
    do {
        sd->wpnn[wslot] = v;
        game_design_update_engines(sd);
        --v;
    } while ((v >= 0) && (game_design_calc_space(gd) < 0));
    return 0;
}

static int cmd_set_wpnn1(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_set_wpnn(var, 0, param);
}

static int cmd_set_wpnn2(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_set_wpnn(var, 1, param);
}

static int cmd_set_wpnn3(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_set_wpnn(var, 2, param);
}

static int cmd_set_wpnn4(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_set_wpnn(var, 3, param);
}

static const char *sel_spec_get_display(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    struct design_data_s *d = sel->d;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    int i = l->value, sslot = sel->slot, space, cost, space2, power, cost2, sizei;
    if (i < 0) {
        return l->display;
    }
    sd->special[sslot] = 0;
    game_design_update_engines(sd);
    space = game_design_calc_space(gd);
    cost = game_design_calc_cost(gd);
    sd->special[sslot] = i;
    game_design_update_engines(sd);
    space2 = space - game_design_calc_space(gd);
    power = tbl_shiptech_special[i].power[sd->hull];
    cost2 = game_design_calc_cost(gd) - cost;
    sizei = game_design_calc_space_item(gd, DESIGN_SLOT_SPECIAL1, i);
    lib_sprintf(ui_data.strbuf, UI_STRBUF_SIZE, "%s%s: %s, %s %i, %s %i, %s %i, %s %i", (i == sel->oldv) ? "*" : "", *tbl_shiptech_special[i].nameptr, *tbl_shiptech_special[i].extratextptr, game_str_sd_cost, cost2, game_str_sd_size, sizei, game_str_sd_power, power, game_str_sd_space, space2);
    return ui_data.strbuf;
}

static int cmd_sel_spec(struct design_data_s *d, int sslot)
{
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    struct input_list_s il[SHIP_SPECIAL_NUM + 2];
    bool flag_enable[SHIP_SPECIAL_NUM];
    int oldv = sd->special[sslot];
    {
        int8_t havebuf[SHIP_SPECIAL_NUM];
        int havelast;
        sd->special[sslot] = 0;
        game_design_update_engines(sd);
        havelast = game_design_build_tbl_fit_special(d->g, gd, havebuf, sslot);
        sel_build_il(il, flag_enable, havebuf, havelast, 0);
    }
    {
        struct sel_shiptech_s sel = { .d = d, .flag_enable = flag_enable, .oldv = oldv, .slot = sslot };
        struct input_list_dyn_s ld = { .list = il, .ctx = &sel, .is_ok = sel_is_ok, .get_display = sel_spec_get_display };
        int v = ui_input_list_dynamic(game_str_tbl_sd_spec[sslot], "> ", &ld);
        sd->special[sslot] = (v < 0) ? oldv : v;
    }
    return 0;
}

static int cmd_sel_spec1(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_sel_spec(var, 0);
}

static int cmd_sel_spec2(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_sel_spec(var, 1);
}

static int cmd_sel_spec3(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return cmd_sel_spec(var, 2);
}

static const char *sel_hull_get_display(void *ctx, const struct input_list_s *l)
{
    struct sel_shiptech_s *sel = ctx;
    int i = l->value;
    if (i < 0) {
        return l->display;
    }
    lib_sprintf(ui_data.strbuf, UI_STRBUF_SIZE, "%s%s", (i == sel->oldv) ? "*" : "", *tbl_shiptech_hull[i].nameptr);
    return ui_data.strbuf;
}

static int cmd_sel_hull(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct design_data_s *d = var;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    struct input_list_s il[SHIP_HULL_NUM + 2];
    bool flag_enable[SHIP_HULL_NUM];
    int oldv = sd->hull;
    {
        int8_t havebuf[SHIP_HULL_NUM];
        for (int i = 0; i < SHIP_HULL_NUM; ++i) {
            bool flag_no;
            flag_no = d->flag_tbl_hull[i];
            havebuf[i] = flag_no ? 0 : 1;
            flag_enable[i] = !flag_no;
        }
        sel_build_il(il, flag_enable, havebuf, SHIP_HULL_NUM - 1, 0);
    }
    {
        struct sel_shiptech_s sel = { .d = d, .flag_enable = flag_enable, .oldv = oldv };
        struct input_list_dyn_s ld = { .list = il, .ctx = &sel, .is_ok = sel_is_ok, .get_display = sel_hull_get_display };
        int v = ui_input_list_dynamic("Ship size", "> ", &ld);
        sd->hull = (v < 0) ? oldv : v;
    }
    return 0;
}

static int cmd_set_name(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct design_data_s *d = var;
    struct game_design_s *gd = d->gd;
    shipdesign_t *sd = &(gd->sd);
    if (num_param == 0) {
        sd->name[0] = '\0';
    } else {
#define BUFLEN 64
        char name[BUFLEN];
        int len;
        strncpy(name, param[0].str, BUFLEN - 1);
        name[BUFLEN - 1] = '\0';
        util_trim_whitespace(name, sizeof(name));
        len = strlen(name);
        if (len >= SHIP_NAME_LEN) {
            printf("Name too long!\n");
            return -1;
        }
        memset(sd->name, 0, SHIP_NAME_LEN);
        memcpy(sd->name, name, len);
#undef BUFLEN
    }
    if (sd->name[0] == '\0') {
        lib_strcpy(sd->name, gd->names[sd->hull], SHIP_NAME_LEN);
    }
    return 0;
}

/* -------------------------------------------------------------------------- */

static const struct input_cmd_s * const cmdsptr_design[2];

static const struct input_cmd_s cmds_design[] = {
    { "?", NULL, "Help", 0, 0, 0, ui_cmd_help, (void *)cmdsptr_design },
    { "q", NULL, "Cancel", 0, 0, 0, cmd_exit, (void *)1/*cancel*/ },
    { "b", NULL, "Build", 0, 0, 0, cmd_exit, (void *)2/*ok*/ },
    { "l", NULL, "Look", 0, 0, 0, cmd_look, 0 },
    { "clear", NULL, "Clear", 0, 0, 0, cmd_clear, 0 },
    { "c", NULL, "Computer", 0, 0, 0, cmd_sel_comp, 0 },
    { "s", NULL, "Shield", 0, 0, 0, cmd_sel_shield, 0 },
    { "j", NULL, "Ecm", 0, 0, 0, cmd_sel_jammer, 0 },
    { "a", NULL, "Armor", 0, 0, 0, cmd_sel_armor, 0 },
    { "e", NULL, "Engine", 0, 0, 0, cmd_sel_engine, 0 },
    { "m", NULL, "Maneuver", 0, 0, 0, cmd_sel_man, 0 },
    { "w1", NULL, "Weapon 1 type", 0, 0, 0, cmd_sel_wpnt1, 0 },
    { "w2", NULL, "Weapon 2 type", 0, 0, 0, cmd_sel_wpnt2, 0 },
    { "w3", NULL, "Weapon 3 type", 0, 0, 0, cmd_sel_wpnt3, 0 },
    { "w4", NULL, "Weapon 4 type", 0, 0, 0, cmd_sel_wpnt4, 0 },
    { "a1", "NUM", "Weapon 1 amount", 1, 1, 0, cmd_set_wpnn1, 0 },
    { "a2", "NUM", "Weapon 2 amount", 1, 1, 0, cmd_set_wpnn2, 0 },
    { "a3", "NUM", "Weapon 3 amount", 1, 1, 0, cmd_set_wpnn3, 0 },
    { "a4", "NUM", "Weapon 4 amount\nNUM can be +N or -N for relative adjustment", 1, 1, 0, cmd_set_wpnn4, 0 },
    { "s1", NULL, "Special 1", 0, 0, 0, cmd_sel_spec1, 0 },
    { "s2", NULL, "Special 2", 0, 0, 0, cmd_sel_spec2, 0 },
    { "s3", NULL, "Special 3", 0, 0, 0, cmd_sel_spec3, 0 },
    { "h", NULL, "Ship size", 0, 0, 0, cmd_sel_hull, 0 },
    { "n", "[NAME]", "Name", 0, 1, 0, cmd_set_name, 0 },
    { NULL, NULL, NULL, 0, 0, 0, NULL, 0 }
};

static const struct input_cmd_s * const cmdsptr_design[2] = {
    cmds_design,
    NULL
};

static bool ui_design(struct game_s *g, struct game_design_s *gd, player_id_t pi)
{
    struct design_data_s d;
    shipdesign_t *sd = &(gd->sd);
    d.g = g;
    d.gd = gd;
    d.api = pi;
    game_design_init_maxtech_haveflags(&d);
    lib_strcpy(sd->name, gd->names[sd->hull], SHIP_NAME_LEN);
    cmd_look(g, pi, 0, 0, &d);
    while (1) {
        char *input;
        input = ui_input_line("Ship design > ");
        if ((ui_input_tokenize(input, cmdsptr_design) == 0) && (ui_data.input.num > 0)) {
            if (ui_data.input.tok[0].type == INPUT_TOKEN_COMMAND) {
                const struct input_cmd_s *cmd;
                int v;
                cmd = ui_data.input.tok[0].data.cmd;
                v = cmd->handle(g, pi, &ui_data.input.tok[1], ui_data.input.num - 1, cmd->var ? cmd->var : &d);
                if (cmd->handle == cmd_exit) {
                    return (v == 2/*ok*/);
                } else if (cmd->handle != ui_cmd_help) {
                    game_design_update_engines(sd);
                    game_design_update_haveflags(&d);
                    cmd_look(g, pi, 0, 0, &d);
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

int ui_cmd_design(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct game_design_s gd;
    bool ok;
    int sd_num;
    sd_num = g->eto[api].shipdesigns_num;
    game_design_prepare(g, &gd, api, &g->current_design[api]);
    ok = ui_design(g, &gd, api);
    if (ok && (sd_num == NUM_SHIPDESIGNS)) {
        ui_cmd_fleet_specs(g, api, 0, 0, 0);
        ui_cmd_fleet_scrap(g, api, 0, 0, 0);
        sd_num = g->eto[api].shipdesigns_num;
        ok = (sd_num < NUM_SHIPDESIGNS);
        if (ok) {
            game_design_look_fix(g, api, &gd.sd);
        }
    }
    if (ok) {
        game_design_add(g, api, &gd.sd, true);
    }
    g->current_design[api] = gd.sd;
    return 0;
}
