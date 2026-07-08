#include "config.h"

#include <stdlib.h>

#include "game_battle_human.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_battle.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_str.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "ui.h"
#include "util.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

#define BATTLE_ROUTE_LEN    20

/* -------------------------------------------------------------------------- */

static void game_battle_with_human_init_sub1(struct battle_s *bt)
{
    /* from ui_battle_do_sub1 */
    bt->items_num = bt->s[SIDE_L].items + bt->s[SIDE_R].items;
    for (int i = 0; i <= bt->items_num; ++i) {
        struct battle_item_s *b = &(bt->item[i]);
        bool flag_no_missiles;
        flag_no_missiles = true;
        for (int j = 0; j < WEAPON_SLOT_NUM; ++j) {
            weapon_t w;
            w = b->wpn[j].t;
            if ((tbl_shiptech_weap[w].numshots > 0) && (!tbl_shiptech_weap[w].is_bomb)) {
                flag_no_missiles = false;
            }
        }
        b->missile = flag_no_missiles ? -1 : 1;
    }
    bt->antidote = bt->item[0].pulsar;  /* HACK */
    bt->item[0].pulsar = 0;
    if (bt->item[0].subspace > 0) {
        bool flag_att_have_teleporter = false;
        int att_from, att_to, def_from, def_to;
        bt->have_subspace_int = true;
        if (bt->item[0].side == SIDE_L) {
            att_from = bt->s[SIDE_L].items + 1;
            att_to = bt->items_num;
            def_from = 1;
            def_to = bt->s[SIDE_L].items;
        } else {
            att_from = 1;
            att_to = bt->s[SIDE_L].items;
            def_from = bt->s[SIDE_L].items + 1;
            def_to = bt->items_num;
        }
        for (int i = att_from; i <= att_to; ++i) {
            if (bt->item[i].subspace > 0) {
                bt->item[i].subspace = -1;
                flag_att_have_teleporter = true;
            }
        }
        if (flag_att_have_teleporter) {
            for (int i = def_from; i <= def_to; ++i) {
                if (bt->item[i].subspace > 0) {
                    bt->item[i].subspace = -1;
                }
            }
        }
    }
    for (int i = 0; i <= bt->items_num; ++i) {
        bt->item[i].hp1 = bt->item[i].hp2;
    }
    for (int i = 0; i < BATTLE_ROCK_MAX; ++i) { /* BUG? this is done only up to 6 */
        bt->rock[i].sx = -1;
        bt->rock[i].sy = -1;
    }
}

static void game_battle_place_items(struct battle_s *bt)
{
    const int8_t tbl_starty[NUM_SHIPDESIGNS] = { 3, 4, 2, 5, 1, 6 };
    {
        int x = 1, y = 3;
        switch (bt->item[0].side) {
            case SIDE_NONE:
                x = -1;
                y = -1;
                break;
            case SIDE_L:
                break;
            case SIDE_R:
                x = 8;
                break;
        }
        bt->item[0].sx = x;
        bt->item[0].sy = y;
    }
    for (int i = 1; i <= bt->s[SIDE_L].items; ++i) {
        bt->item[i].sx = 0;
        bt->item[i].sy = tbl_starty[i - 1];
    }
    for (int i = 1; i <= bt->s[SIDE_R].items; ++i) {
        bt->item[i + bt->s[SIDE_L].items].sx = 9;
        bt->item[i + bt->s[SIDE_L].items].sy = tbl_starty[i - 1];
    }
    for (int i = 0; i < bt->num_rocks; ++i) {
        struct battle_rock_s *r = &(bt->rock[i]);
        bool flag_ok;
        r->gfx = ui_gfx_get_rock(rnd_0_nm1(4, &bt->g->seed));
        flag_ok = false;
        while (!flag_ok) {
            int8_t x, y;
            flag_ok = true;
            x = rnd_0_nm1(7, &bt->g->seed) + 2;
            y = rnd_0_nm1(8, &bt->g->seed);
            {
                const struct battle_item_s *b;
                b = &(bt->item[0]);
                if ((b->side != SIDE_NONE) && (abs(b->sx - x) <= 1) && (abs(b->sy - y) <= 1)) {
                    flag_ok = false;
                }
            }
            for (int j = 0; j < i; ++j) {
                struct battle_rock_s *t = &(bt->rock[j]);
                if (0   /* FIXME BUG? overlap not checked */
                  || ((t->sy == y) && ((t->sx == (x - 1)) || (t->sx == (x + 1))))
                  || ((t->sx == x) && ((t->sy == (y - 1)) || (t->sy == (y + 1))))
                ) {
                    flag_ok = false;
                }
            }
            if (flag_ok) {
                r->sx = x;
                r->sy = y;
            }
        }
    }
}

static void game_battle_damage_planet(struct battle_s *bt, uint32_t damage)
{
    int v;
    if (bt->item[0].num > 0) {
        damage /= 2;
    }
    bt->popdamage += damage;
    v = bt->popdamage / game_num_pop_hp;
    bt->popdamage = bt->popdamage % game_num_pop_hp;
    v = bt->pop - v;
    SETMAX(v, 0);
    bt->pop = v;
    bt->factdamage += damage;
    v = bt->factdamage / game_num_fact_hp;
    bt->factdamage = bt->factdamage % game_num_fact_hp;
    v = bt->fact - v;
    SETMAX(v, 0);
    bt->fact = v;
}

static void game_battle_missile_remove_unused(struct battle_s *bt)
{
    for (int i = 0; i < bt->num_missile; ++i) {
        struct battle_missile_s *m = &(bt->missile[i]);
        if (m->target == MISSILE_TARGET_NONE) {
            util_table_remove_item_any_order(i, bt->missile, sizeof(struct battle_missile_s), bt->num_missile);
            --i;
            --bt->num_missile;
        }
    }
}

static void game_battle_item_destroy(struct battle_s *bt, int item_i)
{
    struct battle_item_s *b = &(bt->item[item_i]);
    battle_side_i_t side = b->side;
    b->selected = 0;
    if (item_i != 0) {
        if (bt->cur_item > item_i) {
            --bt->cur_item;
        }
        for (int i = 0; i <= bt->items_num2; ++i) {
            struct battle_item_s *b2 = &(bt->item[i]);
            if (bt->priority[i] == item_i) {
                bt->priority[i] = 50;
            }
            if ((bt->priority[i] > item_i) && (bt->priority[i] < 50)) {
                --bt->priority[i];
            }
            if (b2->stasisby == item_i) {
                b2->stasisby = 0;
            }
            if (b2->stasisby > item_i) {
                --b2->stasisby;
            }
        }
        for (int i = 0; i < bt->num_missile; ++i) {
            struct battle_missile_s *m = &(bt->missile[i]);
            if ((m->source == item_i) || (m->target == item_i)) {
                m->target = MISSILE_TARGET_NONE;
            }
            if (m->source > item_i) {
                --m->source;
            }
            if (m->target > item_i) {
                --m->target;
            }
        }
        --bt->s[side].items;
        util_table_remove_item_keep_order_zero(item_i, bt->item, sizeof(bt->item[0]), bt->items_num + 1);
        --bt->items_num;
    } else {
        b->wpn[0].t = WEAPON_NONE;
        for (int i = 0; i < bt->num_missile; ++i) {
            struct battle_missile_s *m = &(bt->missile[i]);
            if (m->source == item_i) {
                m->target = MISSILE_TARGET_NONE;
            }
        }
        if (bt->have_subspace_int) {
            bt->have_subspace_int = false;
            for (int i = 0; i <= bt->items_num2; ++i) {
                struct battle_item_s *b2 = &(bt->item[i]);
                if (b2->subspace == -1) {
                    b2->subspace = 1;
                }
            }
        }
    }
    game_battle_missile_remove_unused(bt);
}

static void game_battle_missile_spawn(struct battle_s *bt, int attacker_i, int target_i, int nummissiles, weapon_t wpnt, int damagemul2)
{
    struct battle_missile_s *m;
    struct shiptech_weap_s *w = &(tbl_shiptech_weap[wpnt]);
    if (bt->num_missile == BATTLE_MISSILE_MAX) {
        bt->missile[0].target = MISSILE_TARGET_NONE;
        game_battle_missile_remove_unused(bt);
    }
    m = &(bt->missile[bt->num_missile]);
    m->target = target_i;
    m->damagemul2 = damagemul2;
    m->source = attacker_i;
    {
        struct battle_item_s *b = &(bt->item[attacker_i]);
        struct firing_s *fr = &(bt->g->gaux->firing[b->look]);
        m->x = b->sx * 32 + fr->target_x + (32 - fr->target_x * 2) * b->side;
        m->y = b->sy * 24 + fr->target_y;
    }
    m->fuel = w->v24;
    if (attacker_i == 0/*planet*/) {
        m->fuel += 12;
    }
    m->wpnt = wpnt;
    m->nummissiles = nummissiles * w->nummiss;
    m->speed = w->dtbl[0];
    ++bt->num_missile;
}

static void game_battle_missile_hit(struct battle_s *bt, int missile_i, int target_x, int target_y)
{
    struct game_s *g = bt->g;
    struct battle_missile_s *m = &(bt->missile[missile_i]);
    int target_i = m->target, damage;
    struct battle_item_s *b = &(bt->item[target_i]);
    struct shiptech_weap_s *w = &(tbl_shiptech_weap[m->wpnt]);
    if (target_i == MISSILE_TARGET_NONE) {
        return;
    }
    {
        int damagediv = 1;
        if (1
          && ((target_i == 0) && (!w->is_bomb))
          && ((w->misstype > 0) || (w->damagemin != w->damagemax))
        ) {
            damagediv = 2;
        }
        if (w->damagefade) {
            damage = w->damagemax - ((w->v24 - m->fuel) * w->dtbl[0] + (w->dtbl[0] - m->speed)) / 2;
        } else {
            damage = w->damagemax;
        }
        damage /= damagediv;
        damage -= b->absorb / (w->halveshield ? 2 : 1);
        damage *= w->damagemul;
        if ((b->sbmask & (1 << SHIP_SPECIAL_BOOL_DISP)) && (rnd_1_n(100, &g->seed) < 35)) {
            damage = 0;
        }
    }
    if (damage < 0) {
        m->target = MISSILE_TARGET_NONE;
    } else {
        struct battle_item_s *bs = &(bt->item[m->source]);
        int v1c = m->damagemul2, misschance, hploss = b->hploss, totaldamage = 0, num_destroyed = 0;
        bool flag_hit_misshield = false;
        uint32_t totalhp = b->hp1 * b->num - hploss;
        misschance = 50 - (bs->complevel - b->misdefense) * 10;
        if (b->cloak == 1) {
            misschance += 50;
        }
        if (!game_num_bt_precap_tohit) {
            SETMIN(misschance, 95);
        }
        for (int i = 0; i < m->nummissiles; ++i) {
            int chance;
            chance = misschance - w->extraacc * 10;
            if (game_num_bt_precap_tohit) {
                SETMIN(chance, 95);
            }
            if (chance <= rnd_1_n(100, &g->seed)) {
                if ((b->misshield - w->tech_i) < rnd_1_n(100, &g->seed)) {
                    int v;
                    v = MIN(damage, b->hp1);
                    v *= v1c;
                    totaldamage += v;
                    hploss += v;
                } else {
                    flag_hit_misshield = true;
                }
            }
            for (int j = 0; (j < v1c) && (b->hp1 <= hploss); ++j) {
                if (b->num >= num_destroyed) {
                    ++num_destroyed;
                }
                hploss -= b->hp1;
            }
            hploss = hploss % b->hp1;
        }
        hploss = (totaldamage + b->hploss) % b->hp1;    /* FIXME should be hploss + b->hploss % hp1 ? */
        b->hploss = hploss;
        if (flag_hit_misshield && (!bt->autoresolve)) {
            ui_battle_draw_misshield(bt, target_i, target_x, target_y, missile_i);
        }
        m->target = MISSILE_TARGET_NONE;
        if (target_i == 0) {
            game_battle_damage_planet(bt, totaldamage);
        }
        SETMIN(totaldamage, totalhp);
        if (b->hploss >= b->hp1) {  /* FIXME never true to the % above? */
            ++num_destroyed;
            b->hploss = 0;
        }
        {
            int num = b->num - num_destroyed;
            SETMAX(num, 0);
            b->num = num;
            if ((totaldamage > 0) && (!bt->autoresolve)) {
                ui_battle_draw_damage(bt, target_i, target_x, target_y, totaldamage);
            }
            if (num == 0) {
                if (target_i == bt->cur_item) {
                    bt->flag_cur_item_destroyed = true;
                }
                game_battle_item_destroy(bt, target_i);
            }
        }
    }
}

static int game_battle_missile_rock_collide(struct battle_s *bt, struct battle_missile_s *m)
{
    const int tbl_chance[] = { 10, 9, 8, 7, 7, 6, 5, 4, 3, 3 };
    int collisions = 0;
    for (int i = 0; i < m->nummissiles; ++i) {
        int r;
        r = rnd_1_n(100, &bt->g->seed);
        if (r <= tbl_chance[tbl_shiptech_weap[m->wpnt].extraacc]) {
            ++collisions;
        }
    }
    return collisions;
}

static void game_battle_missile_move(struct battle_s *bt, int missile_i, int target_x, int target_y, int step)
{
    struct battle_missile_s *m = &(bt->missile[missile_i]);
    int mx = m->x, my = m->y, target_i = m->target, target_x_hit, target_y_hit;
    struct battle_item_s *b = &(bt->item[target_i]);
    {
        struct firing_s *fr = &(bt->g->gaux->firing[b->look]);
        if (b->side == SIDE_R) {
            target_x_hit = target_x + 32 - fr->target_x;
        } else {
            target_x_hit = target_x + fr->target_x;
        }
        target_y_hit = target_y + fr->target_y;
    }
    {
        int dx = target_x_hit - mx, dy = target_y_hit - my;
        if (abs(dx) > abs(dy)) {
            SETMIN(step, abs(dx));
            if (dx == 0) {
                dx = 1;
            }
            mx += (dx / abs(dx)) * step;
            my += ((dy * step * 3 + 3) / (abs(dx) * 4));
        } else {
            SETMIN(step, abs(dy));
            if (dy == 0) {
                dy = 1;
            }
            mx += (dx * step) / abs(dy);
            my += ((dy / abs(dy)) * step * 3 + 3) / 4;
        }
    }
    if (bt->num_rocks > 0) {
        int collisions = 0;
        for (int dist = 1; dist <= step; dist += 4) {
            int mx0, my0;
            mx0 = m->x;
            my0 = m->y;
            util_math_go_line_dist(&mx0, &my0, mx, my, dist);
            for (int i = 0; i < bt->num_rocks; ++i) {
                struct battle_rock_s *r = &(bt->rock[i]);
                if (1
                  && ((r->sx * 32 + 2) <= mx0)
                  && ((r->sx * 32 + 30) >= mx0)
                  && ((r->sy * 24 + 2) <= my0)
                  && ((r->sy * 24 + 22) >= my0)
                ) {
                    collisions += game_battle_missile_rock_collide(bt, m);
                }
            }
        }
        if (collisions > 0) {
            int n = m->nummissiles - collisions;
            SETMAX(n, 0);
            m->nummissiles = n;
            if (n == 0) {
                m->target = -1;
            }
            if (!bt->autoresolve) {
                ui_battle_draw_explos_small(bt, mx - 4, my - 4);
            }
        }
    }
    if (m->nummissiles > 0) {
        if (!bt->autoresolve) {
            ui_battle_draw_missile(bt, missile_i, mx, my, target_x_hit, target_y_hit);
        }
        {   /* from ui_battle_draw_missile */
            const struct shiptech_weap_s *w = &(tbl_shiptech_weap[m->wpnt]);
            if (w->misstype == 4) {
                int v;
                v = w->damagemax - (((w->v24 - m->fuel) * w->dtbl[0] + (w->dtbl[0] - m->speed))) / 2; /* FIXME check this calc */
                if (v < 0) {
                    m->target = MISSILE_TARGET_NONE;
                }
            }
        }
    }
    if (1
      && ((target_x_hit - 10) <= mx)
      && ((target_x_hit + 10) >= mx)
      && ((target_y_hit - 10) <= my)
      && ((target_y_hit + 10) >= my)
    ) {
        game_battle_missile_hit(bt, missile_i, target_x, target_y);
    } else {
        int v;
        m->x = mx;
        m->y = my;
        v = m->speed - step;
        SETMAX(v, 0);
        m->speed = v;
    }
}

static void game_battle_item_finish(struct battle_s *bt, bool flag_quick)
{
    bool flag_done = false;
    uint8_t itemi = bt->cur_item;
    struct battle_item_s *b = &(bt->item[itemi]);
    int delay = flag_quick ? 5 : 10;
    if (b->side == SIDE_NONE) {
        return;
    }
    while (!flag_done) {
        flag_done = true;
        for (int i = 0; i < bt->num_missile; ++i) {
            struct battle_missile_s *m = &(bt->missile[i]);
            if ((m->target == itemi) && (m->speed > 0)) {
                int step;
                step = tbl_shiptech_weap[m->wpnt].dtbl[0] / delay;
                SETMIN(step, m->speed);
                game_battle_missile_move(bt, i, b->sx * 32, b->sy * 24, step);
                flag_done = false;
            }
        }
        if (!bt->autoresolve) {
            ui_battle_draw_basic(bt);
        }
    }
    game_battle_missile_remove_unused(bt);
}

static void game_battle_with_human_init(struct battle_s *bt)
{
    /* from ui_battle_do */
    struct game_s *g = bt->g;
    planet_t *p = &(g->planet[bt->planet_i]);
    switch (p->rocks) {
        case PLANET_ROCKS_SOME:
            bt->num_rocks = rnd_1_n(4, &bt->g->seed);
            break;
        case PLANET_ROCKS_MANY:
            bt->num_rocks = rnd_0_nm1(3, &bt->g->seed) + 5;
            break;
        case PLANET_ROCKS_NONE:
            /* already zeroed */
            break;
    }
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        bt->s[SIDE_L].tbl_ships[i] = 0;
        bt->s[SIDE_R].tbl_ships[i] = 0;
    }
    if ((bt->planet_side != SIDE_NONE) && (bt->bases > 0)) {
        bt->s[bt->planet_side].flag_have_scan = true;
    }
    bt->item[0].side = bt->planet_side;
    game_battle_with_human_init_sub1(bt);
    game_battle_place_items(bt);
    for (int i = 1; i <= bt->items_num; ++i) {
        struct battle_item_s *b = &(bt->item[i]);
        for (int j = 0; j < WEAPON_SLOT_NUM; ++j) {
            if (b->wpn[j].n > 0) {
                weapon_t w;
                w = b->wpn[j].t;
                b->wpn[j].numshots = tbl_shiptech_weap[w].numshots;
            }
        }
    }
}

static int game_battle_get_priority(const struct battle_item_s *b, race_t race)
{
    int prio = b->complevel + b->man - b->unman;
    if (b->sbmask & (1 << SHIP_SPECIAL_BOOL_SCANNER)) {
        prio += 3;
    }
    if (b->subspace > 0) {
        prio += b->subspace * 1000;
    }
    if (race == RACE_ALKARI) {
        prio += 3;
    }
    return prio;
}

static void game_battle_build_priority(struct battle_s *bt)
{
    for (int i = 0; i <= bt->items_num; ++i) {
        bt->priority[i] = bt->items_num - i;
    }
    for (int i = 1; i <= bt->items_num; ++i) {
        const struct battle_item_s *b;
        int j, prio_j, prio_i, itemi;
        j = i - 1;
        itemi = bt->priority[i];
        b = &(bt->item[itemi]);
        prio_i = game_battle_get_priority(b, bt->s[b->side].race);
        b = &(bt->item[bt->priority[j]]);
        prio_j = game_battle_get_priority(b, bt->s[b->side].race);
        while ((j >= 0) && (prio_j < prio_i)) {
            bt->priority[j + 1] = bt->priority[j];
            --j;
            if (j >= 0) {
                b = &(bt->item[bt->priority[j]]);
                prio_j = game_battle_get_priority(b, bt->s[b->side].race);
            }
        }
        bt->priority[j + 1] = itemi;
    }
}

static void game_battle_item_done(struct battle_s *bt)
{
    struct battle_item_s *b = &(bt->item[bt->cur_item]);
    b->selected = 0;
    do {
        bt->priority[bt->prio_i] = -1;
        bt->prio_i = (bt->prio_i + 1) % (bt->items_num2 + 1);
    } while (bt->priority[bt->prio_i] == 50);
}

static void game_battle_missile_turn_done(struct battle_s *bt)
{
    for (int i = 0; i < bt->num_missile; ++i) {
        struct battle_missile_s *m = &(bt->missile[i]);
        if (m->speed <= 0) {
            m->speed = tbl_shiptech_weap[m->wpnt].dtbl[0];
            if (--m->fuel == 0) {
                m->target = MISSILE_TARGET_NONE;
            }
        }
    }
    game_battle_missile_remove_unused(bt);
}

static void game_battle_reset_specials(struct battle_s *bt)
{
    struct battle_item_s *b;
    int itemi = bt->cur_item;
    for (int i = 0; i <= bt->items_num; ++i) {
        b = &(bt->item[i]);
        if (b->repulsor > 0) {
            b->repulsor = 1;
        }
    }
    b = &(bt->item[itemi]);
    if ((b->stasis > 0) || (b->pulsar > 0)) {
        bt->special_button = 1;
    } else {
        bt->special_button = -1;
    }
    if (itemi == 0) {
        bt->special_button = -1;
    }
    {
        bool flag_no_missiles = true;
        for (int i = 0; i < WEAPON_SLOT_NUM; ++i) {
            if ((b->wpn[i].numshots > 0) && (!tbl_shiptech_weap[b->wpn[i].t].is_bomb)) {
                flag_no_missiles = false;
            }
        }
        if (flag_no_missiles) {
            b->missile = -1;
        }
        if (itemi == 0) {
            b->missile = 1;
        }
    }
    if (game_num_bt_wait_no_reload && b->can_retaliate) {   /* WASBUG MOO1 reloads specials on Wait */
        return;
    }
    b->can_retaliate = true;
    for (int i = 0; i <= bt->items_num; ++i) {
        b = &(bt->item[i]);
        if (b->stasisby == itemi) {
            b->stasisby = 0;
        }
    }
    b = &(bt->item[itemi]);
    if (b->warpdis == 2) {
        b->warpdis = 1;
    }
    if (b->stasis == 3) {
        b->stasis = 1;
    }
    if (b->pulsar > 2) {
        b->pulsar -= 2;
    }
    if (b->stream > 2) {
        b->stream -= 2;
    }
    if (b->subspace == 2) {
        b->subspace = 1;
    }
    if (b->blackhole == 2) {
        b->blackhole = 1;
    }
    if (b->technull == 2) {
        b->technull = 1;
    }
    if (b->repair > 0) {
        int repair = (b->hp2 * b->repair) / 100;
        if (b->hp1 < b->hp2) {
            b->hp1 += repair;
            SETMIN(b->hp1, b->hp2);
        } else {
            int v = b->hploss - repair;
            SETMAX(v, 0);
            b->hploss = v;
        }
    }
    if (b->cloak > 1) {
        b->cloak = 2;
    }
}

static int game_battle_get_weap_maxrange(struct battle_s *bt)
{
    struct battle_item_s *b = &(bt->item[bt->cur_item]);
    bool is_planet = (bt->cur_item == 0);
    int maxrange = 0, num_weap = is_planet ? 1 : 4;
    if (is_planet && (b->num > 0) && (b->wpn[0].numfire > 0)) {
        return 12;
    }
    if (b->blackhole == 1) {
        maxrange = 1;
    }
    if (1
      && (bt->special_button == 1)
      && ((b->stasis == 1) || (b->pulsar == 1) || (b->pulsar == 2))
    ) {
        maxrange = 1;
    }
    if ((b->stream == 1) || (b->stream == 2)) {
        maxrange = 2;
    }
    if (b->warpdis == 1) {
        maxrange = 3;
    }
    if (b->technull == 1) {
        maxrange = 4;
    }
    for (int i = 0; i < num_weap; ++i) {
        struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[i].t]);
        if ((b->wpn[i].n > 0) && (b->wpn[i].numfire > 0) && (b->wpn[i].numshots != 0) && (!w->is_bomb)) {
            int range;
            if ((b->wpn[i].numshots == -1) && (w->misstype == 0)) {
                range = w->range + b->extrarange;
            } else {
                range = w->range;
                if ((b->missile == 0) && (range > 1) && (w->misstype == 0)) {
                    range = 0;
                }
            }
            SETMAX(maxrange, range);
        }
    }
    if ((maxrange == 0) && ((b->side + bt->item[0].side) == 1)) {
        for (int i = 0; i < num_weap; ++i) {
            struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[i].t]);
            if ((b->wpn[i].n > 0) && (b->wpn[i].numfire > 0) && (b->wpn[i].numshots != 0) && w->is_bomb) {
                bt->has_attacked = false;
            }
        }
    }
    return maxrange;
}

static void game_battle_set_route_from_tbl(uint8_t *route, int *tblx, int *tbly, int len)
{
    for (int i = 0; i < BATTLE_ROUTE_LEN; ++i) {
        int x, y;
        if (i < len) {
            x = tblx[i];
            y = tbly[i];
            route[i] = BATTLE_XY_SET(x, y);
        } else {
            route[i] = BATTLE_XY_INVALID;
        }
    }
}

static void game_battle_extend_route_from_tbl(uint8_t *route, int *tblx, int *tbly, int len)
{
    int pos;
    for (pos = 0; route[pos] != BATTLE_XY_INVALID; ++pos) {
        /*nop*/
    }
    for (int i = pos; i < (pos + len); ++i) {
        int j;
        j = i - pos;
        route[i] = BATTLE_XY_SET(tblx[j], tbly[j]);
    }
}

static void game_battle_item_move_find_route(struct battle_s *bt, uint8_t *route, int itemi, int sx, int sy)
{
    struct battle_item_s *b = &(bt->item[itemi]);
    int len, tblx[BATTLE_ROUTE_LEN], tbly[BATTLE_ROUTE_LEN];
    for (int i = 0; i < BATTLE_ROUTE_LEN; ++i) {
        route[i] = BATTLE_XY_INVALID;
    }
    len = util_math_line_plot(b->sx, b->sy, sx, sy, tblx, tbly);
    if (game_battle_area_check_line_ok(bt, tblx, tbly, len) == 1) {
        game_battle_set_route_from_tbl(route, tblx, tbly, len);
    } else {
        int minrlen = 999, minlen = 999;
        for (int sx2 = 0; sx2 < BATTLE_AREA_W; ++sx2) {
            for (int sy2 = 0; sy2 < BATTLE_AREA_H; ++sy2) {
                if ((b->sx == sx2) && (b->sy == sy2)) {
                    continue;   /* WASBUG? MOO1 does not check, may use -1 as index below */
                }
                len = util_math_line_plot(b->sx, b->sy, sx2, sy2, tblx, tbly);
                if ((game_battle_area_check_line_ok(bt, tblx, tbly, len) == 1) && (b->man >= len)) {
                    int tblx2[BATTLE_ROUTE_LEN], tbly2[BATTLE_ROUTE_LEN], rlen1;
                    rlen1 = util_math_get_route_len(b->sx, b->sy, tblx, tbly, len);
                    if (rlen1 <= minrlen) {
                        int len2;
                        len2 = util_math_line_plot(sx2, sy2, sx, sy, tblx2, tbly2);
                        if ((game_battle_area_check_line_ok(bt, tblx2, tbly2, len2) == 1) && (b->man >= (len + len2))) {
                            int rlen2;
                            rlen2 = util_math_get_route_len(tblx[len - 1], tbly[len - 1], tblx2, tbly2, len2);
                            if (((len2 + len) <= minlen) && ((rlen2 + rlen1) < minrlen)) {
                                minlen = len2 + len;
                                minrlen = rlen2 + rlen1;
                                game_battle_set_route_from_tbl(route, tblx, tbly, len);
                                game_battle_extend_route_from_tbl(route, tblx2, tbly2, len2);
                            }
                        } else {
                            for (int sx3 = 0; sx3 < BATTLE_AREA_W; ++sx3) {
                                for (int sy3 = 0; sy3 < BATTLE_AREA_H; ++sy3) {
                                    if ((sx3 == sx2) && (sy3 == sy2)) {
                                        break;  /* BUG? not continue */
                                    }
                                    {
                                        int len2;
                                        len2 = util_math_line_plot(sx2, sy2, sx3, sy3, tblx2, tbly2);
                                        if ((game_battle_area_check_line_ok(bt, tblx2, tbly2, len2) == 1) && (b->man >= (len + len2))) {
                                            int tblx3[BATTLE_ROUTE_LEN], tbly3[BATTLE_ROUTE_LEN], rlen3, rlen2;
                                            rlen2 = util_math_get_route_len(tblx[len - 1], tbly[len - 1], tblx2, tbly2, len2);
                                            if ((rlen2 + rlen1) < minrlen) {
                                                int len3;
                                                len3 = util_math_line_plot(sx3, sy3, sx, sy, tblx3, tbly3);
                                                if ((game_battle_area_check_line_ok(bt, tblx3, tbly3, len3) == 1) && (b->man >= (len + len2 + len3))) {
                                                    rlen3 = util_math_get_route_len(tblx2[len2 - 1], tbly2[len2 - 1], tblx3, tbly3, len3);
                                                    if (((len3 + len + len2) <= minlen) && (rlen3 + rlen1 + rlen2) < minrlen) {
                                                        minlen = len3 + len + len2;
                                                        minrlen = rlen3 + rlen1 + rlen2;
                                                        game_battle_set_route_from_tbl(route, tblx, tbly, len);
                                                        game_battle_extend_route_from_tbl(route, tblx2, tbly2, len2);
                                                        game_battle_extend_route_from_tbl(route, tblx3, tbly3, len3);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static uint32_t game_battle_pulsar_get_dmg(struct battle_s *bt, int target_i, int v)
{
    struct battle_item_s *b = &(bt->item[target_i]);
    uint32_t dmg = 0, totalhp = b->num * b->hp1;
    if (target_i == 0/*planet*/) {
        v /= 2;
    }
    v -= b->absorb;
    if (v > 0) {
        if (b->hp1 <= v) {
            b->num = 0; /* MOO1 uses -1 */
            dmg = totalhp;
        } else {
            b->hp1 -= v;
            dmg = b->num * v;
        }
        /*4c967*/
        if (b->hploss > b->hp1) { /* FIXME should be >= ? */
            if (b->num) {   /* MOO1 does not check this */
                --b->num;
            }
            /* FIXME should reset hploss ? */
        }
    }
    return dmg;
}

static void game_battle_pulsar(struct battle_s *bt, int attacker_i, int ptype)
{
    struct battle_item_s *b = &(bt->item[attacker_i]);
    int ndiv, rbase;
    uint32_t dmgtbl[NUM_SHIPDESIGNS * 2 + 1/*planet*/];
    memset(dmgtbl, 0, sizeof(dmgtbl));
    if (ptype == 0) {
        ndiv = 2;
        rbase = 5;
    } else {
        ndiv = 1;
        rbase = 10;
    }
    for (int i = 0; i <= bt->items_num; ++i) {
        struct battle_item_s *bd = &(bt->item[i]);
        if (1
          && (bd->stasisby == 0)
          && (util_math_dist_maxabs(b->sx, b->sy, bd->sx, bd->sy) == 1)
        ) {
            int v;
            v = rnd_1_n(b->num / ndiv + rbase, &bt->g->seed);
            dmgtbl[i] = game_battle_pulsar_get_dmg(bt, i, v);
        }
    }
    if (!bt->autoresolve) {
        ui_battle_draw_pulsar(bt, attacker_i, ptype, dmgtbl);
    }
}

static bool game_battle_special(struct battle_s *bt, int attacker_i, int target_i, int dist, int *killedbelowtargetptr)
{
    /*di*/struct battle_item_s *b = &(bt->item[attacker_i]);
    /*si*/struct battle_item_s *bd = &(bt->item[target_i]);
    bool flag_use_stasis = true;
    if (b->cloak != 0) {
        if ((b->cloak == 1) && (!bt->autoresolve)) {
            ui_battle_draw_cloaking(bt, 20, 100, -1, -1);
        }
        b->cloak = 3;
    }
    /*5599e*/
    if (!bt->s[b->side].flag_human) {
        int n = 0, num = bt->s[SIDE_L].items; /* AI is always on SIDE_R */
        flag_use_stasis = false;
        for (int i = 1; i <= num; ++i) {
            if (bt->item[i].stasisby > 0) {
                ++n;
            }
        }
        if ((num - 2) >= n) {
            flag_use_stasis = true;
        }
    }
    /*559f5*/
    if ((b->stasis == 1) && (dist == 1) && (target_i != 0/*planet*/) && flag_use_stasis && (bd->cloak != 1) && (bt->special_button == 1)) {
        b->stasis = 3;
        if ((bd->side == SIDE_L) || (bt->s[SIDE_R].race != RACE_NUM/*monster*/)) {
            bt->special_button = 0;
            if (!bt->autoresolve) {
                ui_sound_play_sfx(0x15);
                ui_battle_draw_stasis(bt, attacker_i, target_i);
            }
            /* FIXME BUG? shooting target with stasis removes missiles coming for it? */
            for (int i = 0; i < bt->num_missile; ++i) {
                struct battle_missile_s *m = &(bt->missile[i]);
                if (m->target == target_i) {
                    m->target = MISSILE_TARGET_NONE;
                }
            }
            bd->stasisby = attacker_i;
            return true;
        } else {
            /*55ae9*/
            return false; /* FIXME BUG? returns attacker item offset */
        }
    }
    /*55afe*/
    if (((b->stream == 1) || (b->stream == 2)) && (dist <= 2)) {
        uint32_t damage = 0, v;
        if ((bd->side == SIDE_L) || (bt->s[SIDE_R].race != RACE_NUM/*monster*/)) {
            switch (b->stream) {
                case 1:
                    v = b->num / 2 + 20;
                    SETMIN(v, 50);
                    v = (bd->hp1 * v + 99) / 100;
                    SUBSAT0(bd->hp1, v);
                    damage = bd->num * v;
                    if (!bt->autoresolve) {
                        ui_sound_play_sfx(0x16);
                        ui_battle_draw_stream1(bt, attacker_i, target_i);
                        if (damage > 0) {
                            ui_battle_draw_damage(bt, target_i, bd->sx * 32, bd->sy * 24, damage);
                        }
                    }
                    b->stream = 3;
                    break;
                case 2:
                    v = b->num / 2 + 40;
                    SETMIN(v, 75);
                    v = (bd->hp1 * v + 99) / 100;
                    SUBSAT0(bd->hp1, v);
                    damage = bd->num * v;
                    if (!bt->autoresolve) {
                        ui_sound_play_sfx(0x18);
                        ui_battle_draw_stream2(bt, attacker_i, target_i);
                        if (damage > 0) {
                            ui_battle_draw_damage(bt, target_i, bd->sx * 32, bd->sy * 24, damage);
                        }
                    }
                    b->stream = 4;
                    break;
                default:
                    break;
            }
            if (bd->hp1 <= bd->hploss) {
                if (bd->num > 0) {
                    --bd->num;
                }
                bd->hploss = 0;
            }
            if ((bd->num <= 0) || (bd->hp1 <= 0)) {
                bd->num = 0;
                bd->hp1 = 50;
                game_battle_item_destroy(bt, target_i);
                if (target_i != 0/*planet*/) {
                    return true;
                }
            }
        } else {
            /*55fa9*/
            b->stream += 2;
        }
    }
    /*55fce*/
    if ((b->pulsar >= 1) && (dist <= 1) && (bt->special_button != 0)) {
        int n, belowtarget;
        bt->special_button = 0;
        switch (b->pulsar) {
            case 1:
            case 2:
                if (!bt->autoresolve) {
                    ui_sound_play_sfx(0x20);
                }
                game_battle_pulsar(bt, attacker_i, b->pulsar - 1);
                b->pulsar += 2;
                break;
            default:
                break;
        }
        n = bt->items_num;
        belowtarget = 0;
        while (n > -1) {    /* FIXME nothing is done on n == 0 loop */
            if ((bt->item[n].num <= 0) && (n != 0/*planet*/)) {
                game_battle_item_destroy(bt, n);
                if (target_i > n) {
                    ++belowtarget;
                }
                if ((target_i == n) && (target_i != 0/*planet*/)) { /* FIXME redundant check */
                    dist = 100;
                }
            }
            --n;
        }
        *killedbelowtargetptr = belowtarget;
        if ((dist == 100) && (target_i != 0/*planet*/)) { /* FIXME redundant check */
            return true;
        }
    }
    /*560d4*/
    if (b->warpdis == 1) {
        struct battle_item_s *bi;
        int t;
        if ((bd->unman >= bd->man) || (dist > 3) || (target_i == 0/*planet*/)) {
            t = -1;
            for (int i = 1; i <= bt->items_num; ++i) {
                bi = &(bt->item[i]);
                if (1
                  && ((b->side + bi->side) == 1)
                  && (util_math_dist_maxabs(b->sx, b->sy, bi->sx, bi->sy) <= 3)
                  && (bi->unman < bi->man)
                ) {
                    t = i;
                }
            }
        } else {
            /*561de*/
            t = target_i;
        }
        if (t > 0) {
            bi = &(bt->item[t]);
            if ((bi->side == SIDE_L) || (bt->s[SIDE_R].race != RACE_NUM/*monster*/)) {
                int v;
                if (!bt->autoresolve) {
                    ui_sound_play_sfx(0x09);
                    ui_battle_draw_bomb_attack(bt, attacker_i, t, UI_BATTLE_BOMB_WARPDIS);
                }
                b->warpdis = 2;
                v = rnd_0_nm1(2, &bt->g->seed);
                bi->unman += v;
                if ((v > 0) && (bi->man > bi->unman)) {
                    SUBSAT0(bi->defense, 2);
                    SUBSAT0(bi->misdefense, 2);
                }
                /*56318*/
                SETMIN(bi->unman, bi->man);
                SUBSAT0(bi->actman, bi->unman); /* FIXME unman sub'd earlier? */
            } else {
                /*563af*/
                b->warpdis = 2;
            }
        } else {
            /*563c3*/
            b->warpdis = 2;
        }
    }
    /*563d5*/
    if ((b->blackhole == 1) && (dist <= 1)) {
        if ((bd->side == SIDE_L) || (bt->s[SIDE_R].race != RACE_NUM/*monster*/)) {
            int v;
            if (!bt->autoresolve) {
                ui_sound_play_sfx(0x1d);
                ui_battle_draw_blackhole(bt, attacker_i, target_i);
            }
            v = rnd_1_n(75, &bt->g->seed) + 25;
            v -= bd->absorb * 2;
            if (v > 0) {
                if (bd->num > 1) {
                    bd->num = ((100 - v) * bd->num) / 100;
                } else {
                    if (rnd_1_n(100, &bt->g->seed) > v) {
                        bd->num = 0;
                    }
                }
            }
            /*5672f*/
            if (bd->num <= 0) {
                game_battle_item_destroy(bt, target_i);
                if (target_i != 0/*planet*/) {
                    return true;
                }
            }
            b->blackhole = 2;
        } else {
            /*56750*/
            b->blackhole = 2;
        }
    }
    /*56762*/
    if ((b->technull == 1) && (dist <= 4)) {
        if ((bd->side == SIDE_L) || (bt->s[SIDE_R].race != RACE_NUM/*monster*/)) {
            int v;
            v = bd->complevel - rnd_1_n(3, &bt->g->seed) - rnd_1_n(3, &bt->g->seed);
            bd->complevel = v;
            v = bd->misdefense - rnd_1_n(3, &bt->g->seed) - rnd_1_n(3, &bt->g->seed);
            SETMAX(v, bd->defense);
            bd->misdefense = v;
            if (!bt->autoresolve) {
                ui_sound_play_sfx(0x0e);
                ui_battle_draw_technull(bt, attacker_i, target_i);
            }
            b->technull = 2;
        } else {
            /*5687f*/
            b->technull = 2;
        }
    }
    return false;
}

static void game_battle_repulse_do(struct battle_s *bt, int target_i, int sx, int sy, int attacker_i)
{
    struct battle_item_s *b = &(bt->item[attacker_i]);
    struct battle_item_s *bd = &(bt->item[target_i]);
    if (!bt->autoresolve) {
        ui_battle_draw_repulse(bt, attacker_i, target_i, sx, sy);
    }
    b->repulsor = 2;
    bd->sx = sx;
    bd->sy = sy;
    ++bt->num_repulsed;
}

static void game_battle_repulse(struct battle_s *bt, int attacker_i, int target_i)
{
    struct battle_item_s *b = &(bt->item[attacker_i]);
    struct battle_item_s *bd = &(bt->item[target_i]);
    int sx, sy;
    int8_t a;
    sx = bd->sx * 2 - b->sx;
    sy = bd->sy * 2 - b->sy;
    if (1
      && (sx >= 0) && (sx < BATTLE_AREA_W)
      && (sy >= 0) && (sy < BATTLE_AREA_H)
      && (((a = bt->area[sy][sx]) == 0) || (a == 1) || (a == (target_i + 10)))
    ) {
        game_battle_repulse_do(bt, target_i, sx, sy, attacker_i);
    } else {
        for (int xo = -1; xo < 2; ++xo) {
            int x, y;
            x = sx + xo;
            if ((x < 0) || (x >= BATTLE_AREA_W)) {
                continue;
            }
            for (int yo = -1; yo < 2; ++yo) {
                y = sy + yo;
                if ((y < 0) || (y >= BATTLE_AREA_H)) {
                    continue;
                }
                a = bt->area[y][x];
                if (1
                  && ((a == 1) || (a == 0) || (a == (target_i + 10)))
                  && (util_math_dist_maxabs(x, y, b->sx, b->sy) == 2)
                ) {
                    game_battle_repulse_do(bt, target_i, x, y, attacker_i);
                    return;
                }
            }
        }
    }
}

static void game_battle_move_retaliate(struct battle_s *bt, int itemi)
{
    struct battle_item_s *b = &(bt->item[itemi]);
    uint8_t num_repulsed = 0;
    bool destroyed = false;
    for (int i = 1; i <= bt->items_num; ++i) {
        struct battle_item_s *b2 = &(bt->item[i]);
        if (b2->repulsor > 1) {
            b2->repulsor = 1;
        }
    }
    for (int i = 1; i <= bt->items_num; ++i) {
        struct battle_item_s *b2 = &(bt->item[i]);
        if ((b->side + b2->side) == 1) {
            if ((b2->stasisby == 0) && (b2->cloak != 1)) {
                if (b2->can_retaliate) {
                    destroyed = game_battle_attack(bt, i, itemi, true);
                } else if ((b2->repulsor == 1) && (util_math_dist_maxabs(b->sx, b->sy, b2->sx, b2->sy) == 1)) {
                    game_battle_repulse(bt, i, itemi);
                    if (bt->num_repulsed > num_repulsed) {
                        i = 0;
                    }
                }
            }
        }
        num_repulsed = bt->num_repulsed;
        if (destroyed) {
            break;
        }
    }
}

static void game_battle_with_human_do_turn_ai(struct battle_s *bt)
{
    int itemi = bt->cur_item;
    struct battle_item_s *b = &(bt->item[itemi]);
    if (!bt->autoresolve) {
        ui_battle_ai_pre(bt);
    }
    b->maxrange = game_battle_get_weap_maxrange(bt);
    if (b->retreat >= 2) {
        bt->s[b->side].tbl_ships[b->shiptbli] = b->num;
        if (!bt->autoresolve) {
            ui_battle_draw_retreat(bt);
        }
        game_battle_item_destroy(bt, itemi);
    } else {
        /*5a547*/
        bt->flag_cur_item_destroyed = false;    /* FIXME already done by caller */
        bt->num_repulsed = 0;    /* FIXME already done by caller */
        if ((b->stasisby == 0) && (b->num > 0)) {
            game_ai->battle_ai_turn(bt);
        } else {
            /*5a936*/
            b->retreat = 0;
        }
        /*5a949*/
        if (b->retreat == 1) {
            b->retreat = 2;
        }
    }
    /*5a96e*/
    if ((!bt->autoresolve) && ui_battle_ai_post(bt)) {
        for (battle_side_i_t i = SIDE_L; i <= SIDE_R; ++i) {
            if (bt->s[i].flag_human) {
                bt->s[i].flag_auto = 0;
            }
        }
    }
    if ((b->cloak == 2) && (b->stasisby == 0)) {
        if (!bt->autoresolve) {
            ui_battle_draw_cloaking(bt, 100, 20, -1, -1);
        }
        b->cloak = 1;
    }
}

static void game_battle_with_human_do_sub3(struct battle_s *bt)
{
    int vc = 0;
    bool flag_round_done;
    for (int i = 0; i <= bt->items_num; ++i) {
        struct battle_item_s *b = &(bt->item[i]);
        b->actman = b->man - b->unman;
        b->can_retaliate = false;
        for (int j = 0; j < WEAPON_SLOT_NUM; ++j) {
            int n, m;
            n = tbl_shiptech_weap[b->wpn[j].t].numfire;
            m = b->wpn[j].numfire;
            m += n;
            SETMIN(m, n);
            b->wpn[j].numfire = m;
        }
    }
    ++bt->num_turn;
    game_battle_build_priority(bt);
    bt->items_num2 = bt->items_num;
    flag_round_done = false;
    for (bt->prio_i = 0; (bt->priority[bt->prio_i] >= 0) && (!flag_round_done);) {
        struct battle_item_s *b;
        int itemi;
        bt->cur_item = itemi = bt->priority[bt->prio_i];
        b = &(bt->item[itemi]);
        if (itemi == 0) {
            b->retreat = 0;
            bt->bases_using_mirv = bt->s[b->side].flag_base_missile;
        }
        if ((itemi == 0) && (b->num <= 0)) {
            bt->special_button = -1;
            game_battle_item_finish(bt, bt->s[0].flag_auto && bt->s[1].flag_auto);
            game_battle_item_done(bt);
        } else {
            /*4eb6b*/
            bool flag_turn_done;
            bt->has_attacked = false;
            game_battle_reset_specials(bt);
            game_battle_area_setup(bt);
            if (/*(b->num > 0) &&*/ (b->side != -1) && (!bt->autoresolve)) {
                ui_battle_draw_basic(bt);
            }
            /*4ebbf*/
            flag_turn_done = false;
            while (!flag_turn_done) {
                ui_battle_action_t act;
                if (!bt->autoresolve) {
                    ui_battle_turn_pre(bt);
                }
                {
                    bool flag_no_missiles;
                    flag_no_missiles = true;    /* BUG? uninitialized in MOO1 if b->missile == -1 */
                    if ((b->missile == 1) || (b->missile == 0)) {
                        for (int i = 0; i < WEAPON_SLOT_NUM; ++i) {
                            if ((b->wpn[i].numshots > 0) && (!tbl_shiptech_weap[b->wpn[i].t].is_bomb)) {
                                flag_no_missiles = false;
                            }
                        }
                    }
                    if (flag_no_missiles) {
                        b->missile = -1;
                    }
                }
                if (itemi == 0) {
                    b->missile = 1;
                }
                /*4ec80*/
                b->selected = 1;
                bt->flag_cur_item_destroyed = false;
                bt->num_repulsed = 0;
                if (bt->s[b->side].flag_auto || (b->retreat > 0)) {
                    if (bt->s[b->side].flag_human && bt->autoretreat && (b->retreat == 0)) {
                        b->retreat = 1;
                    } else {
                        game_battle_with_human_do_turn_ai(bt);
                        flag_turn_done = true;
                        game_battle_item_done(bt);
                    }
                } else {
                    /*4ece2*/
                    act = ui_battle_turn(bt);
                    if ((b->stasisby > 0) || ((itemi == 0) && (b->num <= 0))) {
                        act = UI_BATTLE_ACT_DONE;
                    }
                    if (0
                      || (act == UI_BATTLE_ACT_DONE) || (bt->turn_done)
                      || ((b->missile != 0) && (b->maxrange == 0) && bt->has_attacked)
                    ) {
                        flag_turn_done = true;
                        if ((b->cloak == 2) && (b->stasisby == 0)) {
                            ui_battle_draw_cloaking(bt, 100, 20, -1, -1);
                            b->cloak = 1;
                        }
                        game_battle_item_done(bt);
                    }
                    /*4eddd*/
                    if (act == UI_BATTLE_ACT_WAIT) {
                        flag_turn_done = true;
                        b->selected = 0;
                        b->can_retaliate = true; /* XXX redundant, set by game_battle_reset_specials */
                        bt->priority[vc] = itemi;
                        if (vc != bt->prio_i) {
                            bt->priority[bt->prio_i] = -1;
                        }
                        vc = (vc + 1) % (bt->items_num2 + 1);
                        while (bt->prio_i = (bt->prio_i + 1) % (bt->items_num2 + 1), bt->priority[bt->prio_i] == 50) {
                            bt->priority[bt->prio_i] = -1;
                        }
                    }
                    /*4ee70*/
                    if (act == UI_BATTLE_ACT_AUTO) {
                        bt->s[b->side].flag_auto = 1;
                        if (b->missile == 0) {
                            b->missile = 1;
                        }
                    }
                    /*4eeb8*/
                    if (act == UI_BATTLE_ACT_MISSILE) {
                        if (itemi == 0) {
                            weapon_t t;
                            bt->bases_using_mirv = !bt->bases_using_mirv;
                            bt->s[b->side].flag_base_missile = !bt->s[b->side].flag_base_missile;
                            t = bt->item[0].wpn[0].t;
                            bt->item[0].wpn[0].t = bt->item[0].wpn[1].t;
                            bt->item[0].wpn[1].t = t;
                        } else {
                            b->missile = !b->missile;
                        }
                    }
                    /*4ef23*/
                    if (act == UI_BATTLE_ACT_PLANET) {
                        ui_battle_draw_planetinfo(bt, b->side == SIDE_R);
                    }
                    /*4ef45*/
                    if (act == UI_BATTLE_ACT_SCAN) {
                        if (bt->s[b->side ^ 1].items == 0) {
                            if ((bt->item[0].side + b->side) == 1) {
                                ui_battle_draw_planetinfo(bt, b->side == SIDE_R);
                            }
                        } else {
                            ui_battle_draw_scan(bt, b->side != SIDE_R);
                        }
                    }
                    /*4ef92*/
                    if (act == UI_BATTLE_ACT_SPECIAL) {
                        bt->special_button = !bt->special_button;
                    }
                    if (act == UI_BATTLE_ACT_RETREAT) {
                        b->retreat = 1;
                    }
                    if (act < (BATTLE_AREA_W * BATTLE_AREA_H)) {
                        int sx, sy, sa;
                        sx = UI_BATTLE_ACT_GET_X(act);
                        sy = UI_BATTLE_ACT_GET_Y(act);
                        if (sx < BATTLE_AREA_W) {
                            sa = bt->area[sy][sx];
                            switch (sa) {
                                case 1:
                                    game_battle_item_move(bt, itemi, sx, sy);
                                    break;
                                case 10:
                                    ui_battle_draw_planetinfo(bt, b->side);
                                    break;
                                case 11:
                                case 12:
                                case 13:
                                case 14:
                                case 15:
                                case 16:
                                    ui_battle_draw_scan(bt, bt->item[sa - 10].side);
                                    break;
                                case 30:
                                case 31:
                                case 32:
                                case 33:
                                case 34:
                                case 35:
                                case 36:
                                case 37: /* FIXME >36 ??? */
                                case 38:
                                case 39:
                                case 40:
                                case 41:
                                case 42:
                                case 43:
                                    game_battle_attack(bt, itemi, sa - 30, false);
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    /*4f0b6*/
                    itemi = bt->cur_item;
                    b = &(bt->item[itemi]);
                    if (!bt->flag_cur_item_destroyed) {
                        game_battle_area_setup(bt);
                        if (bt->turn_done || (bt->has_attacked && (b->maxrange == 0) && (b->missile != 0))) {
                            b->selected = 0;
                        }
                        if ((b->num > 0) && (b->side != SIDE_NONE)) {
                            ui_battle_draw_basic_copy(bt);
                        }
                    } else {
                        /*4f15a*/
                        flag_turn_done = true;
                    }
                }
                /*4f15f*/
                if (!bt->autoresolve) {
                    ui_battle_turn_post(bt);
                }
            }
            /*4f172*/
            if (!bt->flag_cur_item_destroyed) {
                game_battle_item_finish(bt, bt->s[0].flag_auto && bt->s[1].flag_auto);
            }
            if (bt->flag_cur_item_destroyed) {
                game_battle_item_done(bt);
            }
        }
        /*4f189*/
        b = &(bt->item[0/*planet*/]);
        if (((b->side != SIDE_R) || (b->num <= 0)) && (bt->s[SIDE_R].items == 0)) {
            flag_round_done = true;
        }
        if (((b->side != SIDE_L) || (b->num <= 0)) && (bt->s[SIDE_L].items == 0)) {
            flag_round_done = true;
        }
    }
    game_battle_missile_turn_done(bt);
}

typedef enum {
    BATTLE_TURN_CONTINUE, /*-1,1*/
    BATTLE_TURN_RETREAT, /*0*/
    BATTLE_TURN_TIMEOUT, /*-50*/
    BATTLE_TURN_WIN_L, /*10*/
    BATTLE_TURN_WIN_R  /*11*/
} battle_turn_start_t;

static battle_turn_start_t game_battle_with_human_do_sub2(struct battle_s *bt)
{
    struct battle_item_s *b = &(bt->item[0/*planet*/]);
    int items[2];
    items[SIDE_L] = bt->s[SIDE_L].items;
    items[SIDE_R] = bt->s[SIDE_R].items;
    if (b->num > 0) {
        ++items[b->side];
    }
    if (items[SIDE_L] == 0) {
        return BATTLE_TURN_WIN_R;
    }
    if (items[SIDE_R] == 0) {
        return BATTLE_TURN_WIN_L;
    }
    if (bt->num_turn > game_num_bt_turn_max) {
        return BATTLE_TURN_TIMEOUT;
    }
    if (bt->s[SIDE_R].race == RACE_NUM/*monster*/) {
        return BATTLE_TURN_CONTINUE;
    }
    if (!bt->s[SIDE_R].flag_auto) {
        return BATTLE_TURN_CONTINUE;
    }
    if (game_ai->battle_ai_retreat(bt)) {
        return BATTLE_TURN_RETREAT;
    }
    return BATTLE_TURN_CONTINUE;
}

static battle_side_i_t game_battle_with_human_do(struct battle_s *bt)
{
    battle_side_i_t winner = SIDE_NONE;
    while (winner == SIDE_NONE) {
        switch (game_battle_with_human_do_sub2(bt)) {
            case BATTLE_TURN_RETREAT:
                for (int i = bt->s[SIDE_L].items + 1; i <= bt->items_num; ++i) {
                    struct battle_item_s *b = &(bt->item[i]);
                    ++b->retreat;
                }
                break;
            case BATTLE_TURN_WIN_L:
                winner = SIDE_L;
                break;
            case BATTLE_TURN_WIN_R:
                winner = SIDE_R;
                break;
            case BATTLE_TURN_TIMEOUT:
                if (bt->item[0].side == SIDE_NONE) {
                    winner = SIDE_R;
                } else {
                    winner = bt->item[0].side;
                }
                break;
            default:
                break;
        }
        if (winner == SIDE_NONE) {
            game_battle_with_human_do_sub3(bt);
        }
    }
    /*SETMAX(bt->item[0].num, 0);*/
    for (int i = 1; i <= bt->items_num; ++i) {
        struct battle_item_s *b = &(bt->item[i]);
        bt->s[b->side].tbl_ships[b->shiptbli] = b->num;
    }
    return winner;
}

/* -------------------------------------------------------------------------- */

int game_battle_area_check_line_ok(struct battle_s *bt, int *tblx, int *tbly, int len)
{
    int r = 1;
    for (int i = 0; i < len; ++i) {
        int8_t v;
        v = bt->area[tbly[i]][tblx[i]];
        if (v == -100) {
            r = 0;
        }
        if ((v == 10) || (v == 30) || (v == -30)) {
            r = -1;
        }
    }
    return r;
}

bool game_battle_attack(struct battle_s *bt, int attacker_i, int target_i, bool retaliate)
{
    /*di*/struct battle_item_s *b = &(bt->item[attacker_i]);
    /*si*/struct battle_item_s *bd = &(bt->item[target_i]);
    int num_weap, hploss, numkill = 0, dist, miss_chance_beam, miss_chance_missile;
    uint32_t planetdamage = 0, totalhp;
    bool destroyed = false;
    if (attacker_i == 0) {
        if (1
          && (!bt->s[b->side].flag_auto) && (b->wpn[1].t != WEAPON_NONE)
          && (bt->s[b->side].flag_base_missile == (tbl_shiptech_weap[b->wpn[0].t].nummiss > 1))
        ) {
            weapon_t t = b->wpn[1].t; b->wpn[1].t = b->wpn[0].t; b->wpn[0].t = t;
        }
        num_weap = 1;
    } else {
        num_weap = 4;
    }
    /*571a9*/
    hploss = bd->hploss;
    if (target_i != 0) {
        totalhp = bd->hp1 * bd->num - hploss;
    } else {
        totalhp = 2000000000;   /* FIXME uint32_t max */
    }
    dist = util_math_dist_maxabs(b->sx, b->sy, bd->sx, bd->sy);
    if (!retaliate) {
        bt->has_attacked = true;
    }
    miss_chance_beam = 50 - (b->complevel - bd->defense) * 10;
    miss_chance_missile = 50 - (b->complevel - bd->misdefense) * 10;
    {
        int tblx[20], tbly[20], len;
        len = util_math_line_plot(b->sx, b->sy, bd->sx, bd->sy, tblx, tbly);
        if (game_battle_area_check_line_ok(bt, tblx, tbly, len) == 0) {
            miss_chance_beam += 30;
        }
    }
    if (bd->cloak == 1) {
        miss_chance_beam += 50;
        miss_chance_missile += 50;
    }
    if (!game_num_bt_precap_tohit) {
        SETMIN(miss_chance_beam, 95);
        SETMIN(miss_chance_missile, 95);
    }
    {
        bool flag_done1 = false;
        int killedbelowtarget = 0;
        if (!(retaliate && (bd->cloak != 1))) {
            flag_done1 = game_battle_special(bt, attacker_i, target_i, dist, &killedbelowtarget);
        }
        if (killedbelowtarget > 0) {
            bt->flag_cur_item_destroyed = false;
            target_i -= killedbelowtarget;
            bd = &(bt->item[target_i]);
            /* FIXME BUG? what about attacker_i ? */
        }
        if (flag_done1) {
            return destroyed;
        }
    }
    /*573bf*/
    for (int i = 0; i < num_weap; ++i) {
        uint32_t damage2;
        damage2 = 0;
        if ((b->wpn[i].n > 0) && (b->wpn[i].numfire > 0) && (b->wpn[i].numshots != 0)) {
            const struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[i].t]);
            int damagerange, range;
            uint32_t damagemul1, damagemul2;
            uint8_t damagediv;
            damagerange = w->damagemax - w->damagemin;
            range = (attacker_i == 0) ? 40 : 0;
            if ((!w->is_bomb) && (damagerange != 0)) {
                range += b->extrarange;
            } else {
                if (bt->item[bt->cur_item].maxrange < 0) {  /* FIXME BUG? should be [attacker_i] */
                    range += bt->item[bt->cur_item].maxrange;
                }
            }
            range += w->range;
            if ((range >= dist) && ((!w->is_bomb) || (target_i == 0/*planet*/))) {
                if (!game_num_bt_no_tohit_acc) { /* BUG? each weapon reduces miss chance */
                    miss_chance_beam -= w->extraacc * 10;
                    miss_chance_missile -= w->extraacc * 10;
                }
                if ((damagerange != 0) || w->is_bio) {
                    /*5755a*/
                    int absorbdiv;
                    if (!bt->autoresolve) {
                        ui_sound_play_sfx(w->sound);
                    }
                    if (w->is_bomb) {
                        absorbdiv = (w->halveshield ? 2 : 1);
                    } else {
                        absorbdiv = game_battle_get_absorbdiv(b, w, false);
                    }
                    if (w->is_bomb) {
                        if (w->is_bio) {
                            int dmgsum;
                            dmgsum = 0;
                            for (int j = 0; j < (b->wpn[i].n * b->num); ++j) {
                                int dmg;
                                dmg = rnd_0_nm1(w->damagemax + 1, &bt->g->seed) - bt->antidote;
                                SETMAX(dmg, 0);
                                dmgsum += dmg;
                            }
                            if (bt->pop > dmgsum) {
                                bt->pop -= dmgsum;
                            } else {
                                bt->pop = 0; /* FIXME not limited here in MOO1 */
                            }
                            bt->biodamage += dmgsum;
                        }
                        if (!bt->autoresolve) {
                            ui_battle_draw_bomb_attack(bt, attacker_i, target_i, w->is_bio ? UI_BATTLE_BOMB_BIO : UI_BATTLE_BOMB_BOMB);
                        }
                    }
                    /*5761d*/
                    damagediv = 1;
                    if ((target_i == 0/*planet*/) && (!w->is_bomb) && ((w->misstype > 0) || (w->damagemin != w->damagemax))) {
                        damagediv = 2;
                    }
                    damagemul2 = 1;
                    damagemul1 = b->num * b->wpn[i].n;
                    while (damagemul1 > 1000) {
                        damagemul1 /= 2;
                        damagemul2 *= 2;
                    }
                    /*576d0*/
                    while ((b->wpn[i].numfire > 0) && ((bd->num > 0) || (target_i == 0/*planet*/))) {
                        int miss_chance;
                        if ((!w->is_bomb) && (!bt->autoresolve)) {
                            ui_battle_draw_beam_attack(bt, attacker_i, target_i, i);
                        }
                        /*576f1*/
                        if (w->misstype >= 1) {
                            --b->wpn[i].numfire;
                        }
                        --b->wpn[i].numfire;
                        if (b->wpn[i].numshots > 0) {   /* BUG always dec'd on MOO1 */
                            --b->wpn[i].numshots;
                        }
                        miss_chance = w->is_bomb ? miss_chance_missile : miss_chance_beam;
                        if (game_num_bt_no_tohit_acc) {
                            miss_chance -= w->extraacc * 10;
                        }
                        if (game_num_bt_precap_tohit) {
                            SETMIN(miss_chance, 95);
                        }
                        for (uint32_t j = 0; j < damagemul1; ++j) {
                            /*57755*/
                            int r, dmg;
                            r = rnd_1_n(100, &bt->g->seed);
                            dmg = 0;
                            if (r >= miss_chance) {
                                dmg = ((r - miss_chance) * 2 * damagerange) / (100 - miss_chance);
                                dmg = (dmg + 1) / 2;
                                dmg += w->damagemin;
                                dmg /= damagediv;
                                dmg -= bd->absorb / absorbdiv;
                                dmg *= w->damagemul;
                                dmg *= damagemul2;
                                if ((bd->sbmask & (1 << SHIP_SPECIAL_BOOL_DISP)) && (rnd_1_n(100, &bt->g->seed) < 35)) {
                                    dmg = 0;
                                }
                            }
                            /*57893*/
                            if (dmg > 0) {
                                if ((bd->hp1 < dmg) && (w->nummiss/*streaming*/ == 0)) {
                                    planetdamage += bd->hp1;
                                    damage2 += bd->hp1;
                                } else {
                                    planetdamage += dmg;
                                    damage2 += dmg;
                                }
                                hploss += dmg;
                            }
                            /*57902*/
                            for (uint32_t k = 0; ((w->nummiss/*streaming*/ == 1) || (k < damagemul2)) && (bd->hp1 <= hploss); ++k) {
                                if (bd->hp1 <= hploss) {
                                    if (numkill < bd->num) { /* WASBUG? <= in MOO1 */
                                        ++numkill;
                                    }
                                    hploss -= bd->hp1;
                                }
                            }
                            /*5797a*/
                            hploss = hploss % bd->hp1;
                        }
                        /*579aa*/
                        bd->num -= numkill;
                        numkill = 0;
                        SETMIN(damage2, totalhp);
                        /*579e0*/
                    }
                    /*57a19*/
                    if ((damage2 > 0) && (!bt->autoresolve)) {
                        ui_battle_draw_damage(bt, target_i, bd->sx * 32, bd->sy * 24, damage2);
                    }
                } else if ((!retaliate) && (bt->item[bt->cur_item].missile != 0)) {  /* FIXME BUG? should be [attacker_i] */
                    /*57a85*/
                    if (!bt->autoresolve) {
                        ui_sound_play_sfx(w->sound);
                    }
                    if (w->misstype >= 1) {
                        --b->wpn[i].numfire;
                    }
                    if (attacker_i == 0/*planet*/) {
                        --b->wpn[1].numfire;
                    }
                    --b->wpn[i].numfire;
                    --b->wpn[i].numshots;
                    damagemul2 = 1;
                    damagemul1 = b->num * b->wpn[i].n;
                    while (damagemul1 > 1000) {
                        damagemul1 /= 2;
                        damagemul2 *= 2;
                    }
                    if ((damagemul1 > 0) && !w->is_bomb) {
                        game_battle_missile_spawn(bt, attacker_i, target_i, damagemul1, b->wpn[i].t, damagemul2);
                    }
                }
                /*57ba7*/
                numkill = 0;
            }
            /*57bac*/
            if (bd->num <= 0) {
                if (target_i == bt->cur_item) {
                    bt->flag_cur_item_destroyed = true;
                }
                game_battle_item_destroy(bt, target_i);
                destroyed = true;
                if (target_i != 0/*planet*/) {
                    break;
                }
            }
        }
    }
    /*57bef*/
    if (!destroyed) {
        bd->hploss = hploss;
    }
    if (target_i == 0/*planet*/) {
        game_battle_damage_planet(bt, planetdamage);
    }
    {
        struct battle_item_s *bc = &(bt->item[bt->cur_item]);   /* FIXME ??? */
        if (!bt->s[bc->side].flag_auto) {
            bc->maxrange = game_battle_get_weap_maxrange(bt);
        }
    }
    if ((!destroyed) && (dist == 1) && (target_i != 0/*planet*/) && (b->repulsor == 1)) {
        game_battle_repulse(bt, attacker_i, target_i);
    }
    return destroyed;
}

void game_battle_item_move(struct battle_s *bt, int itemi, int sx, int sy)
{
    struct battle_item_s *b = &(bt->item[itemi]);
    bt->num_repulsed = false;
    if (b->subspace == 1) {
        if (((b->sx != sx) || (b->sy != sy)) && (!bt->autoresolve)) {
            ui_battle_draw_cloaking(bt, (b->cloak == 1) ? 30 : 100, 0, sx, sy);
        }
        b->sx = sx;
        b->sy = sy;
        b->actman = 0;
        b->subspace = 2;
    } else {
        bool flag_quick = bt->s[SIDE_L].flag_auto && bt->s[SIDE_R].flag_auto;
        int x, y, stepdiv = flag_quick ? 2 : 8;
        uint8_t route[BATTLE_ROUTE_LEN];
        x = b->sx * 32;
        y = b->sy * 24;
        game_battle_item_move_find_route(bt, route, itemi, sx, sy);
        for (int i = 0; route[i] != BATTLE_XY_INVALID; ++i) {
            int vx, vy, dz;
            sx = BATTLE_XY_GET_X(route[i]);
            sy = BATTLE_XY_GET_Y(route[i]);
            if (bt->flag_cur_item_destroyed || bt->num_repulsed) {
                break;
            }
            /*50a89*/
            --b->actman;
            dz = sx - b->sx;
            vx = dz ? ((dz * (32 / stepdiv)) / abs(dz)) : 0;
            dz = sy - b->sy;
            vy = dz ? ((dz * (24 / stepdiv)) / abs(dz)) : 0;
            b->sx = sx;
            b->sy = sy;
            for (int f = 0; f < stepdiv; ++f) {
                x += vx;
                y += vy;
                if (!bt->autoresolve) {
                    ui_battle_draw_arena(bt, itemi, 2);
                    ui_battle_draw_bottom(bt);
                    b->selected = 2/*moving*/;
                    ui_battle_draw_item(bt, itemi, x, y);
                }
                b->selected = 1;
                for (int j = 0; j < bt->num_missile; ++j) {
                    struct battle_missile_s *m = &(bt->missile[j]);
                    if (m->target == itemi) {
                        int v;
                        v = tbl_shiptech_weap[m->wpnt].dtbl[0];
                        if ((b->man - b->unman) != 0) {
                            v /= (b->man - b->unman);
                        }
                        v = (v + stepdiv - 1) / stepdiv;
                        SETMIN(v, m->speed);
                        game_battle_missile_move(bt, j, x, y, v);
                        if (bt->flag_cur_item_destroyed || bt->num_repulsed) {
                            break;
                        }
                    }
                }
                if (!bt->autoresolve) {
                    ui_battle_draw_finish(bt);
                }
                if (bt->flag_cur_item_destroyed || bt->num_repulsed) {
                    break;
                }
            }
            game_battle_missile_remove_unused(bt);
            if ((!bt->flag_cur_item_destroyed) && (!bt->num_repulsed) && (b->cloak != 1)) {
                game_battle_move_retaliate(bt, itemi);
            }
            if (bt->flag_cur_item_destroyed || bt->num_repulsed) {
                break;
            }
        }
    }
}

int game_battle_get_xy_notsame(const struct battle_s *bt, int item1, int item2, int *x_notsame)
{
    const struct battle_item_s *b1 = &(bt->item[item1]);
    const struct battle_item_s *b2 = &(bt->item[item2]);
    int dx, dy, x_ns = 1, y_ns = 1;
    dx = b1->sx - b2->sx;
    dy = b1->sy - b2->sy;
    if (dx == 0) {
        x_ns = 0;
    }
    if (dy == 0) {
        y_ns = false;
    }
    if ((dx - dy) == 0) {
        x_ns = -1;
    }
    *x_notsame = x_ns;
    return y_ns;
}

void game_battle_area_setup(struct battle_s *bt)
{
    struct battle_item_s *b = &(bt->item[bt->cur_item]);
    int num_weap = (bt->cur_item == 0/*planet*/) ? 1 : 4;
    if (!bt->s[b->side].flag_auto) {
        b->maxrange = game_battle_get_weap_maxrange(bt);
    }
    bt->turn_done = true;
    for (int sy = 0; sy < BATTLE_AREA_H; ++sy) {
        for (int sx = 0; sx < BATTLE_AREA_W; ++sx) {
            bt->area[sy][sx] = 0;
        }
    }
    if (b->actman > 0) {
        bt->turn_done = false;
        for (int sy = 0; sy < BATTLE_AREA_H; ++sy) {
            for (int sx = 0; sx < BATTLE_AREA_W; ++sx) {
                if ((b->subspace == 1) || (b->actman >= util_math_dist_maxabs(b->sx, b->sy, sx, sy))) {
                    bt->area[sy][sx] = 1;
                }
            }
        }
    }
    for (int i = 0; i < bt->num_rocks; ++i) {
        struct battle_rock_s *r = &(bt->rock[i]);
        bt->area[r->sy][r->sx] = -100;
    }
    for (int i = 0; i <= bt->items_num; ++i) {
        struct battle_item_s *b2 = &(bt->item[i]);
        if (b2->side != SIDE_NONE) {
            int8_t v;
            if (b->side == b2->side) {
                v = 10 + i;
            } else {
                int dist, range;
                dist = util_math_dist_maxabs(b->sx, b->sy, b2->sx, b2->sy);
                if (i == 0) {
                    v = -30;
                    for (int j = 0; j < num_weap; ++j) {
                        bool is_missile;
                        const struct shiptech_weap_s *w;
                        w = &(tbl_shiptech_weap[b->wpn[j].t]);
                        is_missile = ((w->damagemax == w->damagemin) && (!w->is_bomb) && (w->misstype == 0));
                        if ((b->actman > 0) || ((w->range >= dist) && (b->wpn[j].numfire > 0) && (b->wpn[j].numshots != 0))) {
                            bt->turn_done = false;
                        }
                        if ((b->wpn[j].numshots == -1) && (w->misstype == 0)) {
                            range = w->range + b->extrarange;
                        } else {
                            range = w->range;
                        }
                        if ((range >= dist) && ((b->missile == 1) || (!is_missile)) && (b->wpn[j].numfire > 0) && (b->wpn[j].numshots != 0)) {
                            v = 30;
                        }
                    }
                    if (0
                      || ((b->blackhole == 1) && (dist == 1))
                      || (((b->pulsar == 1) || (b->pulsar == 2)) && (bt->special_button == 1) && (dist == 1))
                      || (((b->stream == 1) || (b->stream == 2)) && (dist <= 2))
                      || ((b->technull == 1) && (dist <= 4))
                    ) {
                        bt->turn_done = false;
                        v = 30;
                    }
                } else {
                    v = -i;
                    if ((b->maxrange >= dist) && (b2->stasisby == 0)) {
                        bt->turn_done = false;
                        v = 30 + i;
                    }
                }
            }
            bt->area[b2->sy][b2->sx] = v;
        }
    }
    {
        int tblx[20], tbly[20];
        int zx, zy, len;
        zx = b->sx - b->actman;
        zy = b->sy - b->actman;
        while (zx < 0) {
            ++zx;
            ++zy;
        }
        while (zy < 0) {
            ++zx;
            ++zy;
        }
        len = util_math_line_plot(b->sx, b->sy, zx, zy, tblx, tbly);
        if ((game_battle_area_check_line_ok(bt, tblx, tbly, len) < 1) && (bt->area[zy][zx] == 1)) {
            bt->area[zy][zx] = 0;
        }
        zx = b->sx + b->actman;
        zy = b->sy - b->actman;
        while (zx >= BATTLE_AREA_W) {
            --zx;
            ++zy;
        }
        while (zy < 0) {
            --zx;
            ++zy;
        }
        len = util_math_line_plot(b->sx, b->sy, zx, zy, tblx, tbly);
        if ((game_battle_area_check_line_ok(bt, tblx, tbly, len) < 1) && (bt->area[zy][zx] == 1)) {
            bt->area[zy][zx] = 0;
        }
        zx = b->sx + b->actman;
        zy = b->sy + b->actman;
        while (zx >= BATTLE_AREA_W) {
            --zx;
            --zy;
        }
        while (zy >= BATTLE_AREA_H) { /* WASBUG? check y >= _W */
            --zx;
            --zy;
        }
        len = util_math_line_plot(b->sx, b->sy, zx, zy, tblx, tbly);
        if ((game_battle_area_check_line_ok(bt, tblx, tbly, len) < 1) && (bt->area[zy][zx] == 1)) {
            bt->area[zy][zx] = 0;
        }
        zx = b->sx - b->actman;
        zy = b->sy + b->actman;
        while (zx < 0) {
            ++zx;
            --zy;
        }
        while (zy >= BATTLE_AREA_H) { /* WASBUG? check y >= _W */
            ++zx;
            --zy;
        }
        len = util_math_line_plot(b->sx, b->sy, zx, zy, tblx, tbly);
        if ((game_battle_area_check_line_ok(bt, tblx, tbly, len) < 1) && (bt->area[zy][zx] == 1)) {
            bt->area[zy][zx] = 0;
        }
    }
    if (b->missile == 0) {
        bt->turn_done = false;
    }
    if (!bt->autoresolve) {
        ui_battle_area_setup(bt);
    }
}

int game_battle_get_absorbdiv(const struct battle_item_s *b, const struct shiptech_weap_s *w, bool force_oracle_check)
{
    int v = 1;
    if ((force_oracle_check || game_num_bt_oracle_fix) && (b->sbmask & (1 << SHIP_SPECIAL_BOOL_ORACLE))) {
        /* XXX we do not check if w is a beam weapon as this is called by buggy-like-v1.3 AI code for missiles */
        v = 2;
    }
    v += w->halveshield ? 1 : 0;
    if (v == 3) {
        v = 4;
    }
    return v;
}

bool game_battle_with_human(struct battle_s *bt)
{
    planet_t *p = &(bt->g->planet[bt->planet_i]);
    battle_side_i_t winner;
    ui_battle_autoresolve_t ar;
    game_battle_with_human_init(bt);
    game_update_visibility(bt->g);
    ar = ui_battle_init(bt);
    if (ar == UI_BATTLE_AUTORESOLVE_OFF) {
        bt->autoresolve = false;
        bt->autoretreat = false;
    } else {
        bt->autoresolve = true;
        bt->autoretreat = (ar == UI_BATTLE_AUTORESOLVE_RETREAT);
        bt->s[SIDE_L].flag_auto = true;
        bt->s[SIDE_R].flag_auto = true;
    }
    winner = game_battle_with_human_do(bt);
    p->pop = bt->pop;
    p->factories = bt->fact;
    bt->bases = bt->item[0/*planet*/].num;
    game_battle_finish(bt);
    ui_battle_shutdown(bt, (bt->planet_side != SIDE_NONE) && (p->owner == PLAYER_NONE), winner);
    return winner == SIDE_R;
}
