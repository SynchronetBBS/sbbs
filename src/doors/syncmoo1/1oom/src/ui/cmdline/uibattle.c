#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_aux.h"
#include "game_battle.h"
#include "game_battle_human.h"
#include "game_str.h"
#include "lib.h"
#include "uicmds.h"
#include "uidefs.h"
#include "uihelp.h"
#include "uiinput.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

static const char battlegfx[][3][5] = {
    {
        "    ",
        "  > ",
        "    "
    },
    {
        "    ",
        " |> ",
        "    "
    },
    {
        " \\  ",
        " >> ",
        " /  "
    },
    {
        "=\\  ",
        "|O> ",
        "=/  "
    },
    {
        "    ",
        " <  ",
        "    "
    },
    {
        "    ",
        " <| ",
        "    "
    },
    {
        "  / ",
        " << ",
        "  \\ "
    },
    {
        "  /=",
        " <O|",
        "  \\="
    },
    {
        "/--\\",
        "|  |",
        "\\--/"
    },
    {
        "+  +",
        "    ",
        "+   "
    },
    {
        "  * ",
        " *  ",
        "*  *"
    }
};

/* -------------------------------------------------------------------------- */

static int cmd_battle_look(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return 0;
}

static const struct input_cmd_s * const cmdsptr_battle[2];

static const struct input_cmd_s cmds_battle[] = {
    { "?", NULL, "Help", 0, 0, 0, ui_cmd_help, (void *)cmdsptr_battle },
    { "[1-9][a-j]", NULL, "Move to / shoot coordinates", 0, 0, 0, NULL, 0 },
    { "[a-j][1-9]", NULL, "Move to / shoot coordinates", 0, 0, 0, NULL, 0 },
    { "w", NULL, "Wait", 0, 0, 0, ui_cmd_dummy_ret, (void *)UI_BATTLE_ACT_WAIT },
    { "d", NULL, "Done", 0, 0, 0, ui_cmd_dummy_ret, (void *)UI_BATTLE_ACT_DONE },
    { "m", NULL, "Missile", 0, 0, 0, ui_cmd_dummy_ret, (void *)UI_BATTLE_ACT_MISSILE },
    { "a", NULL, "Auto", 0, 0, 0, ui_cmd_dummy_ret, (void *)UI_BATTLE_ACT_AUTO },
    { "r", NULL, "Retreat", 0, 0, 0, ui_cmd_dummy_ret, (void *)UI_BATTLE_ACT_RETREAT },
    { "s", NULL, "Scan", 0, 0, 0, ui_cmd_dummy_ret, (void *)UI_BATTLE_ACT_SCAN },
    { "p", NULL, "Planet", 0, 0, 0, ui_cmd_dummy_ret, (void *)UI_BATTLE_ACT_PLANET },
    { "l", NULL, "Look area, highlight clickable", 0, 0, 0, cmd_battle_look, 0 },
    { NULL, NULL, NULL, 0, 0, 0, NULL, 0 }
};

static const struct input_cmd_s * const cmdsptr_battle[2] = {
    cmds_battle,
    0
};

/* -------------------------------------------------------------------------- */

static void ui_battle_draw_reach(const struct battle_s *bt, int itemi)
{
    for (int sy = 0; sy < BATTLE_AREA_H; ++sy) {
        for (int sx = 0; sx < BATTLE_AREA_W; ++sx) {
            int8_t sv;
            sv = bt->area[sy][sx];
            if ((sv == 1) || (sv >= 30)) {
                int tx, ty;
                tx = sx * 4 + 3;
                ty = sy * 3;
                for (int dy = 0; dy < 3; ++dy) {
                    for (int dx = 0; dx < 4; ++dx) {
                        char c;
                        c = ui_data.battle.screen[ty + dy][tx + dx];
                        if (c == ' ') {
                            ui_data.battle.screen[ty + dy][tx + dx] = '.';
                        }
                    }
                }
            }
        }
    }
}

static void ui_battle_prepost(const struct battle_s *bt, int winner)
{
    shipsum_t force[2][SHIP_HULL_NUM];
    game_battle_count_hulls(bt, force);
    for (int i = 0; i < SHIP_HULL_NUM; ++i) {
        printf("- %5u %s %u\n", force[SIDE_L][i], game_str_tbl_st_hull[i], force[SIDE_R][i]);
    }
    if (bt->bases) {
        printf("- %s %u\n", game_str_sb_bases, bt->bases);
    }
    if (winner != SIDE_NONE) {
        int party_winner = (winner != SIDE_NONE) ? bt->s[winner].party : -1;
        const char *str;
        if (party_winner >= PLAYER_NUM) {
            str = game_str_tbl_mon_names[party_winner - PLAYER_NUM];
        } else {
            race_t race = bt->g->eto[party_winner].race;
            str = game_str_tbl_races[race];
        }
        printf("%s %s\n", str, game_str_bp_won);
    }
}

static void ui_battle_draw_scan_weap(const struct battle_item_s *b, int wi, char *buf, size_t bufsize)
{
    if (b->wpn[wi].n != 0) {
        const struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[wi].t]);
        struct strbuild_s str = strbuild_init(buf, bufsize);
        strbuild_catf(&str, "%i x %s", b->wpn[wi].n, *w->nameptr);
        if (b->wpn[wi].numshots >= 0) {
            strbuild_catf(&str, " (x %i)", b->wpn[wi].numshots);
        }
    } else {
        buf[0] = 0;
    }
}

/* -------------------------------------------------------------------------- */

ui_battle_autoresolve_t ui_battle_init(struct battle_s *bt)
{
    struct game_s *g = bt->g;
    const planet_t *p = &(g->planet[bt->planet_i]);
    int party_u = bt->s[SIDE_L].party, party_d = bt->s[SIDE_R].party;
    const char *s1, *s2, *s3;
    ui_switch_2(bt->g, party_u, party_d);
    if (party_d >= PLAYER_NUM) {
        s1 = game_str_tbl_mon_names[party_d - PLAYER_NUM];
    } else {
        race_t race = g->eto[bt->flag_human_att ? party_u : party_d].race;
        s1 = game_str_tbl_races[race];
    }
    s2 = (party_d >= PLAYER_NUM) ? game_str_bp_attacks : game_str_bp_attack;
    {
        race_t race = g->eto[bt->flag_human_att ? party_d : party_u].race;
        s3 = game_str_tbl_races[race];
    }
    printf("%s | %s | %s %s %s\n", game_str_bp_scombat, p->name, s1, s2, s3);
    ui_battle_prepost(bt, SIDE_NONE);
    {
        const struct input_list_s cl_cont_auto[] = {
            { UI_BATTLE_AUTORESOLVE_OFF, "C", NULL, "Continue" },
            { UI_BATTLE_AUTORESOLVE_AUTO, "A", NULL, "Autoresolve" },
            { UI_BATTLE_AUTORESOLVE_RETREAT, "R", NULL, "Retreat" },
            { 0, NULL, NULL, NULL }
        };
        return ui_input_list("Fight?", "> ", cl_cont_auto);
    }

}

void ui_battle_shutdown(struct battle_s *bt, bool colony_destroyed, int winner)
{
    ui_battle_prepost(bt, winner);
    ui_switch_wait(bt->g);
}

void ui_battle_draw_planetinfo(const struct battle_s *bt, bool side_r)
{
    const struct battle_item_s *b = &(bt->item[0/*planet*/]);
    if (b->side == SIDE_NONE) {
        return;
    }
    printf("%s, %s %i, %s %i\n", b->name, game_str_bt_pop, bt->pop, game_str_bt_ind, bt->fact);
    if (bt->s[bt->item[bt->cur_item].side].flag_have_scan || (side_r == (b->side == SIDE_R))) {
        printf("%s:\n", game_str_bt_bases);
        if (bt->have_subspace_int) {
            printf("- %s\n", game_str_bt_subint);
        }
        {
            const struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[0].t]);
            if ((!bt->s[b->side].flag_base_missile) && (w->nummiss == 1)) {
                w = &(tbl_shiptech_weap[b->wpn[1].t]);
            }
            printf("- 3 %s %s\n", *w->nameptr, game_str_bt_launch);
        }
        printf("- bdef %i, mdef %i, att %i, hits %i, dam %i, shield %i\n", b->defense, b->misdefense, b->complevel, b->hp1, b->hploss, b->absorb);
    }
}

void ui_battle_draw_scan(const struct battle_s *bt, bool side_r)
{
    battle_side_i_t scan_side = side_r ? SIDE_R : SIDE_L;
    int itembase, itemnum;
    itembase = (scan_side == SIDE_L) ? 1 : (bt->s[SIDE_L].items + 1);
    itemnum = bt->s[scan_side].items;
    if (!bt->s[scan_side].flag_have_scan) {
        printf("No scanners available.\n");
        return;
    }
    for (int i = 0; i < itemnum; ++i) {
        const struct battle_item_s *b = &(bt->item[itembase + i]);
        char gfx[3][5];
        char buf0[40];
        char buf1[40];
        {
            int tgfx;
            tgfx = (int)(intptr_t)(b->gfx);
            if (b->side == SIDE_R) {
                tgfx += 4;
            }
            for (int dy = 0; dy < 3; ++dy) {
                for (int dx = 0; dx <= 4; ++dx) {
                    gfx[dy][dx] = battlegfx[tgfx][dy][dx];
                }
            }
        }
        gfx[0][((b->side == SIDE_R) ? 0 : 3)] = '1' + b->shiptbli;
        if (b->num > 0) {
            char buf[8];
            if (b->num >= 1000) {
                lib_sprintf(buf, sizeof(buf), "%ik", b->num / 1000);
            } else {
                lib_sprintf(buf, sizeof(buf), "%i", b->num);
            }
            for (int j = 0; buf[j]; ++j) {
                gfx[2][j + 1] = buf[j];
            }
        }
        lib_sprintf(buf0, sizeof(buf0), "%s", b->name);
        ui_battle_draw_scan_weap(b, 0, buf1, sizeof(buf1));
        printf("%-30s %-30s %s\n", buf0, buf1, game_str_tbl_st_specsh[b->special[0]]);
        lib_sprintf(buf0, sizeof(buf0), "%s  bdef %i, mdef %i, att %i", gfx[0], b->defense, b->misdefense, b->complevel);
        ui_battle_draw_scan_weap(b, 1, buf1, sizeof(buf1));
        printf("%-30s %-30s %s\n", buf0, buf1, b->special[1] ? game_str_tbl_st_specsh[b->special[1]] : "");
        lib_sprintf(buf0, sizeof(buf0), "%s  hits %i, dmg %i", gfx[1], b->hp1, b->hploss);
        ui_battle_draw_scan_weap(b, 2, buf1, sizeof(buf1));
        printf("%-30s %-30s %s\n", buf0, buf1, b->special[2] ? game_str_tbl_st_specsh[b->special[2]] : "");
        lib_sprintf(buf0, sizeof(buf0), "%s  shield %i, speed %i", gfx[2], b->absorb, b->man - b->unman);
        ui_battle_draw_scan_weap(b, 3, buf1, sizeof(buf1));
        printf("%-30s %-30s\n", buf0, buf1);
    }
}

void ui_battle_draw_misshield(const struct battle_s *bt, int target_i, int target_x, int target_y, int missile_i)
{
}

void ui_battle_draw_damage(const struct battle_s *bt, int target_i, int target_x, int target_y, uint32_t damage)
{
    char loc[3];
    loc[0] = bt->item[target_i].sx + 'a';
    loc[1] = bt->item[target_i].sy + '1';
    loc[2] = '\0';

    printf("%s: Hit, %d damage\n", loc, damage);
}

void ui_battle_draw_explos_small(const struct battle_s *bt, int x, int y)
{
}

void ui_battle_draw_basic(const struct battle_s *bt)
{
    ui_battle_draw_arena(bt, 0, 0);
}

void ui_battle_draw_basic_copy(const struct battle_s *bt)
{
    ui_battle_draw_arena(bt, 0, 0);
}

void ui_battle_draw_missile(const struct battle_s *bt, int missilei, int x, int y, int tx, int ty)
{
    /* incoming tx, ty unused */
    tx = (x * 4) / 32 + 3;
    ty = (y * 3) / 24;
    ui_data.battle.screen[ty][tx] = 'X';
}

void ui_battle_draw_cloaking(const struct battle_s *bt, int from, int to, int sx, int sy)
{
}

void ui_battle_draw_arena(const struct battle_s *bt, int itemi, int dmode)
{
    memset(ui_data.battle.screen, ' ', sizeof(ui_data.battle.screen));
    for (int l = 0; l < (8 * 3); ++l) {
        ui_data.battle.screen[l][2] = '|';
        ui_data.battle.screen[l][3 + BATTLE_AREA_W * 4 + 1] = '|';
        ui_data.battle.screen[l][3 + BATTLE_AREA_W * 4 + 2] = '\n';
        ui_data.battle.screen[l][3 + BATTLE_AREA_W * 4 + 3] = '\0';
        if ((l % 3) == 1) {
            ui_data.battle.screen[l][1] = '1' + (l / 3);
        }
    }
    for (int i = 1; i <= bt->items_num; ++i) {
        const struct battle_item_s *b;
        size_t battle_screen_write_pos = 3 + BATTLE_AREA_W * 4 + 2;
        b = &(bt->item[i]);
        int l;
        l = (b->side == SIDE_L) ? 0 : 12;
        l += b->shiptbli * 2;
        lib_sprintf(&ui_data.battle.screen[l][battle_screen_write_pos], BATTLE_SCREEN_WIDTH - battle_screen_write_pos, " %i%c %i/%i\n", b->shiptbli + 1, (b->side == SIDE_L) ? '>' : '<', b->hp1 - b->hploss, b->hp1);
        ++l;
        lib_sprintf(&ui_data.battle.screen[l][battle_screen_write_pos], BATTLE_SCREEN_WIDTH - battle_screen_write_pos, " %s\n", b->name);
    }
    for (int i = 0; i <= bt->items_num; ++i) {
        if ((i != itemi) || (dmode == 0/*normal*/)) {
            const struct battle_item_s *b;
            b = &(bt->item[i]);
            if (b->side != SIDE_NONE) {
                ui_battle_draw_item(bt, i, b->sx * 32, b->sy * 24);
            }
        }
    }
    for (int i = 0; i < bt->num_rocks; ++i) {
        const struct battle_rock_s *r;
        int tx, ty;
        r = &(bt->rock[i]);
        tx = (r->sx * 4) + 3;
        ty = (r->sy * 3);
        for (int dy = 0; dy < 3; ++dy) {
            for (int dx = 0; dx < 4; ++dx) {
                char c;
                c = battlegfx[10][dy][dx];
                ui_data.battle.screen[ty + dy][tx + dx] = c;
            }
        }
    }
    for (int i = 0; i < bt->num_missile; ++i) {
        const struct battle_missile_s *m = &(bt->missile[i]);
        int8_t target;
        target = m->target;
        if ((target != MISSILE_TARGET_NONE) && ((target != itemi) || (dmode != 2/*hide target missile*/))) {
            const struct battle_item_s *b;
            b = &(bt->item[target]);
            ui_battle_draw_missile(bt, i, m->x, m->y, b->sx * 32 + 16, b->sy * 24 + 12);
        }
    }
}

void ui_battle_draw_item(const struct battle_s *bt, int itemi, int x, int y)
{
    const struct battle_item_s *b;
    int tx, ty, tgfx;
    b = &(bt->item[itemi]);
    tx = (x * 4) / 32 + 3;
    ty = (y * 3) / 24;
    tgfx = (int)(intptr_t)(b->gfx);
    if ((itemi != 0) && (b->side == SIDE_R)) {
        tgfx += 4;
    }
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = 0; dx < 4; ++dx) {
            char c;
            c = battlegfx[tgfx][dy][dx];
            ui_data.battle.screen[ty + dy][tx + dx] = c;
        }
    }
    if (b->selected && (!bt->s[b->side].flag_auto)) {
        if (b->selected != 2/*moving*/) {
            for (int dy = 0; dy < 3; ++dy) {
                for (int dx = 0; dx < 4; ++dx) {
                    char c;
                    c = battlegfx[9][dy][dx];
                    if (c != ' ') {
                        ui_data.battle.screen[ty + dy][tx + dx] = c;
                    }
                }
            }
        }
    }
    if (itemi != 0) {
        ui_data.battle.screen[ty][tx + ((b->side == SIDE_R) ? 0 : 3)] = '1' + b->shiptbli;
    }
    if (b->num > 0) {
        char buf[8];
        if (b->num >= 1000) {
            lib_sprintf(buf, sizeof(buf), "%ik", b->num / 1000);
        } else {
            lib_sprintf(buf, sizeof(buf), "%i", b->num);
        }
        for (int i = 0; buf[i]; ++i) {
            ui_data.battle.screen[ty + 2][tx + i + 1] = buf[i];
        }
    }
}

void ui_battle_draw_bomb_attack(const struct battle_s *bt, int attacker_i, int target_i, ui_battle_bomb_t bombtype)
{
}
void ui_battle_draw_beam_attack(const struct battle_s *bt, int attacker_i, int target_i, int wpni)
{
}
void ui_battle_draw_stasis(const struct battle_s *bt, int attacker_i, int target_i)
{
}
void ui_battle_draw_pulsar(const struct battle_s *bt, int attacker_i, int ptype, const uint32_t *dmgtbl)
{
}
void ui_battle_draw_stream1(const struct battle_s *bt, int attacker_i, int target_i)
{
}
void ui_battle_draw_stream2(const struct battle_s *bt, int attacker_i, int target_i)
{
}
void ui_battle_draw_blackhole(const struct battle_s *bt, int attacker_i, int target_i)
{
}
void ui_battle_draw_technull(const struct battle_s *bt, int attacker_i, int target_i)
{
}
void ui_battle_draw_repulse(const struct battle_s *bt, int attacker_i, int target_i, int sx, int sy)
{
}
void ui_battle_draw_retreat(const struct battle_s *bt)
{
    int tx, ty, itemi = bt->cur_item;
    const struct battle_item_s *b = &(bt->item[itemi]);
    b = &(bt->item[itemi]);
    tx = (b->sx * 4) + 3;
    ty = (b->sy * 3);
    ui_battle_draw_arena(bt, itemi, 1);
    ui_battle_draw_bottom(bt);
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = 0; dx < 4; ++dx) {
            ui_data.battle.screen[ty + dy][tx + dx] = 'R';
        }
    }
    ui_battle_draw_finish(bt);
}

void ui_battle_draw_bottom(const struct battle_s *bt)
{
}

void ui_battle_draw_finish(const struct battle_s *bt)
{
    fputs("  +-A---B---C---D---E---F---G---H---I---J---+\n", stdout);
    for (int l = 0; l < (8 * 3); ++l) {
        fputs(ui_data.battle.screen[l], stdout);
    }
    fputs("  +-A---B---C---D---E---F---G---H---I---J---+\n", stdout);
}

void ui_battle_area_setup(const struct battle_s *bt)
{
}
void ui_battle_turn_pre(const struct battle_s *bt)
{
}

void ui_battle_turn_post(const struct battle_s *bt)
{
}

ui_battle_action_t ui_battle_turn(const struct battle_s *bt)
{
    int itemi = bt->cur_item;
    const struct battle_item_s *b = &(bt->item[itemi]);
    char prompt[EMPEROR_NAME_LEN + SHIP_NAME_LEN + 20];
    ui_battle_draw_arena(bt, 0, 0);
    ui_battle_draw_finish(bt);
    if (0
      || (b->stasisby > 0)
      || ((itemi == 0) && (b->num <= 0))
      || (bt->turn_done)
      || ((b->missile != 0) && (b->maxrange == 0) && bt->has_attacked)
    ) {
        return UI_BATTLE_ACT_DONE;
    }
    lib_sprintf(prompt, sizeof(prompt), "%s: %d %s > ", bt->g->emperor_names[bt->s[b->side].party], b->num, b->name);
    while (1) {
        char *input;
        input = ui_input_line(prompt);
        if ((ui_input_tokenize(input, cmdsptr_battle) == 0) && (ui_data.input.num > 0)) {
            if (ui_data.input.tok[0].type == INPUT_TOKEN_COMMAND) {
                const struct input_cmd_s *cmd;
                int v;
                cmd = ui_data.input.tok[0].data.cmd;
                v = cmd->handle(bt->g, 0, &ui_data.input.tok[1], ui_data.input.num - 1, cmd->var);
                if (v >= 0) {
                    if (cmd->handle == ui_cmd_dummy_ret) {
                        return v;
                    } else if (cmd->handle == cmd_battle_look) {
                        ui_battle_draw_arena(bt, 0, 0);
                        ui_battle_draw_reach(bt, itemi);
                        ui_battle_draw_finish(bt);
                    }
                }
            } else {
                int x, y;
                const char *p;
                char c;
                x = y = -1;
                p = ui_data.input.tok[0].str;
                c = *p++;
                if ((c >= '1') && (c <= '8')) {
                    y = c - '1';
                    c = *p++;
                    if ((c >= 'a') && (c <= 'j')) {
                        x = c - 'a';
                        if (*p != '\0') {
                            x = -1;
                        }
                    }
                } else if ((c >= 'a') && (c <= 'j')) {
                    x = c - 'a';
                    c = *p++;
                    if ((c >= '1') && (c <= '8')) {
                        y = c - '1';
                        if (*p != '\0') {
                            y = -1;
                        }
                    }
                }
                if ((x >= 0) && (y >= 0)) {
                    return UI_BATTLE_ACT_CLICK(x, y);
                }
            }
        }
    }
}

void ui_battle_ai_pre(const struct battle_s *bt)
{
}

bool ui_battle_ai_post(const struct battle_s *bt)
{
    return false;
}

uint8_t *ui_gfx_get_ship(int look)
{
    if (look >= (SHIP_LOOK_PER_BANNER * BANNER_NUM)) {
        look = 3;
    } else {
        look = (look % SHIP_LOOK_PER_BANNER) / SHIP_LOOK_PER_HULL;
    }
    return (void *)(intptr_t)look;
}

uint8_t *ui_gfx_get_planet(int look)
{
    return (void *)8;
}

uint8_t *ui_gfx_get_rock(int look)
{
    return (void *)10;
}
