#include "config.h"

#include "game_parsed.h"
#include "comp.h"
#include "game.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_shipdesign.h"
#include "game_shiptech.h"
#include "game_tech.h"
#include "game_techtypes.h"
#include "lib.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

#define COPY_PROP(sp_, sd_, xyz_) sp_->xyz_ = sd->xyz_
#define COPY_PROP_TBL(sp_, sd_, xyz_, num_) do { for (int i_ = 0; i_ < num_; ++i_) { sp_->xyz_[i_] = sd->xyz_[i_]; } } while (0)
#define COPY_MAX_SPECIAL(res_, sd_, xyz_) \
    do { \
        int smax_; smax_ = 0; \
        for (int i_ = 0; i_ < SPECIAL_SLOT_NUM; ++i_) { \
            int v_; v_ = tbl_shiptech_special[sd_->special[i_]].xyz_; \
            SETMAX(smax_, v_); \
        } \
        res_ = smax_; \
    } while (0)

/* -------------------------------------------------------------------------- */

shipparsed_t tbl_monster[MONSTER_NUM][DIFFICULTY_NUM] = {
    { /* crystal */
        { /* simple */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_CLASS_V, 0, 0,
            { SHIP_SPECIAL_BLACK_HOLE_GENERATOR, SHIP_SPECIAL_LIGHTNING_SHIELD, 0 },
            { WEAPON_CRYSTAL_RAY, 0, 0, 0 },
            { 10, 0, 0, 0 },
            3000, 2, 10, 1, 1, 5, 0, 100,
            0, 0,
            1/*num*/, 0x90, 0, 0, (1 << SHIP_SPECIAL_BOOL_BLACKHOLE)
        },
        { /* easy */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_CLASS_V, 0, 0,
            { SHIP_SPECIAL_BLACK_HOLE_GENERATOR, SHIP_SPECIAL_LIGHTNING_SHIELD, 0 },
            { WEAPON_CRYSTAL_RAY, 0, 0, 0 },
            { 10, 0, 0, 0 },
            4000, 2, 10, 2, 2, 5, 0, 100,
            0, 0,
            1/*num*/, 0x90, 0, 0, (1 << SHIP_SPECIAL_BOOL_BLACKHOLE)
        },
        { /* medium */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_CLASS_V, 0, 0,
            { SHIP_SPECIAL_BLACK_HOLE_GENERATOR, SHIP_SPECIAL_LIGHTNING_SHIELD, 0 },
            { WEAPON_CRYSTAL_RAY, 0, 0, 0 },
            { 10, 0, 0, 0 },
            5000, 2, 10, 3, 3, 5, 0, 100,
            0, 0,
            1/*num*/, 0x90, 0, 0, (1 << SHIP_SPECIAL_BOOL_BLACKHOLE)
        },
        { /* hard */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_CLASS_V, 0, 0,
            { SHIP_SPECIAL_BLACK_HOLE_GENERATOR, SHIP_SPECIAL_LIGHTNING_SHIELD, 0 },
            { WEAPON_CRYSTAL_RAY, 0, 0, 0 },
            { 10, 0, 0, 0 },
            6000, 2, 10, 4, 4, 5, 0, 100,
            0, 0,
            1/*num*/, 0x90, 0, 0, (1 << SHIP_SPECIAL_BOOL_BLACKHOLE)
        },
        { /* impossible */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_CLASS_V, 0, 0,
            { SHIP_SPECIAL_BLACK_HOLE_GENERATOR, SHIP_SPECIAL_LIGHTNING_SHIELD, 0 },
            { WEAPON_CRYSTAL_RAY, 0, 0, 0 },
            { 10, 0, 0, 0 },
            7000, 2, 10, 5, 5, 5, 0, 100,
            0, 0,
            1/*num*/, 0x90, 0, 0, (1 << SHIP_SPECIAL_BOOL_BLACKHOLE)
        }
    },
    { /* amoeba */
        { /* simple */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_NONE, 0, 0,
            { SHIP_SPECIAL_ADV_DAMAGE_CONTROL, 0, 0 },
            { WEAPON_AMEOBA_STREAM, 0, 0, 0 },
            { 1, 0, 0, 0 },
            1000, 2, 10, 1, 1, 0, 50, 0,
            0, 0,
            1/*num*/, 0x91, 0, 0, 0
        },
        { /* easy */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_NONE, 0, 0,
            { SHIP_SPECIAL_ADV_DAMAGE_CONTROL, 0, 0 },
            { WEAPON_AMEOBA_STREAM, 0, 0, 0 },
            { 1, 0, 0, 0 },
            2000, 2, 10, 2, 2, 0, 50, 0,
            0, 0,
            1/*num*/, 0x91, 0, 0, 0
        },
        { /* medium */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_NONE, 0, 0,
            { SHIP_SPECIAL_ADV_DAMAGE_CONTROL, 0, 0 },
            { WEAPON_AMEOBA_STREAM, 0, 0, 0 },
            { 1, 0, 0, 0 },
            3000, 2, 10, 3, 3, 0, 50, 0,
            0, 0,
            1/*num*/, 0x91, 0, 0, 0
        },
        { /* hard */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_NONE, 0, 0,
            { SHIP_SPECIAL_ADV_DAMAGE_CONTROL, 0, 0 },
            { WEAPON_AMEOBA_STREAM, 0, 0, 0 },
            { 1, 0, 0, 0 },
            4000, 2, 10, 4, 4, 0, 50, 0,
            0, 0,
            1/*num*/, 0x91, 0, 0, 0
        },
        { /* impossible */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, 0, SHIP_SHIELD_NONE, 0, 0,
            { SHIP_SPECIAL_ADV_DAMAGE_CONTROL, 0, 0 },
            { WEAPON_AMEOBA_STREAM, 0, 0, 0 },
            { 1, 0, 0, 0 },
            5000, 2, 10, 5, 5, 0, 50, 0,
            0, 0,
            1/*num*/, 0x91, 0, 0, 0
        }
    },
    { /* guardian */
        { /* simple */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, SHIP_JAMMER_JAMMER_X, SHIP_SHIELD_CLASS_V, 0, 0,
            { SHIP_SPECIAL_HIGH_ENERGY_FOCUS, 0, SHIP_SPECIAL_LIGHTNING_SHIELD },
            { WEAPON_SCATTER_PACK_X_5, WEAPON_DEATH_RAY, WEAPON_STELLAR_CONVERTER, WEAPON_PLASMA_TORPEDO },
            { 5, 1, 5, 6 },
            2000, 2, 10, 1, 1, 5, 0, 100,
            0, 3,
            1/*num*/, 0x92, 0, 0, 0
        },
        { /* easy */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, SHIP_JAMMER_JAMMER_X, SHIP_SHIELD_CLASS_VI, 0, 0,
            { SHIP_SPECIAL_HIGH_ENERGY_FOCUS, 0, SHIP_SPECIAL_LIGHTNING_SHIELD },
            { WEAPON_SCATTER_PACK_X_5, WEAPON_DEATH_RAY, WEAPON_STELLAR_CONVERTER, WEAPON_PLASMA_TORPEDO },
            { 25, 1, 15, 9 },
            4000, 2, 10, 3, 3, 6, 0, 100,
            0, 3,
            1/*num*/, 0x92, 0, 0, 0
        },
        { /* medium */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, SHIP_JAMMER_JAMMER_X, SHIP_SHIELD_CLASS_VII, 0, 0,
            { SHIP_SPECIAL_HIGH_ENERGY_FOCUS, 0, SHIP_SPECIAL_LIGHTNING_SHIELD },
            { WEAPON_SCATTER_PACK_X_5, WEAPON_DEATH_RAY, WEAPON_STELLAR_CONVERTER, WEAPON_PLASMA_TORPEDO },
            { 45, 1, 25, 12 },
            6000, 2, 10, 5, 5, 7, 0, 100,
            0, 3,
            1/*num*/, 0x92, 0, 0, 0
        },
        { /* hard */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, SHIP_JAMMER_JAMMER_X, SHIP_SHIELD_CLASS_IX, 0, 0,
            { SHIP_SPECIAL_HIGH_ENERGY_FOCUS, SHIP_SPECIAL_ADV_DAMAGE_CONTROL, SHIP_SPECIAL_LIGHTNING_SHIELD },
            { WEAPON_SCATTER_PACK_X_5, WEAPON_DEATH_RAY, WEAPON_STELLAR_CONVERTER, WEAPON_PLASMA_TORPEDO },
            { 65, 1, 35, 15 },
            8000, 2, 10, 7, 7, 8, 15, 100,
            0, 3,
            1/*num*/, 0x92, 0, 0, 0
        },
        { /* impossible */
            "", SHIP_HULL_HUGE, SHIP_COMP_MARK_X, SHIP_JAMMER_JAMMER_X, SHIP_SHIELD_CLASS_XI, 0, 0,
            { SHIP_SPECIAL_HIGH_ENERGY_FOCUS, 0, SHIP_SPECIAL_LIGHTNING_SHIELD },
            { WEAPON_SCATTER_PACK_X_5, WEAPON_DEATH_RAY, WEAPON_STELLAR_CONVERTER, WEAPON_PLASMA_TORPEDO },
            { 85, 1, 45, 18 },
            10000, 2, 10, 9, 9, 9, 0, 100,
            0, 3,
            1/*num*/, 0x92, 0, 0, 0
        }
    }
};

static uint8_t get_best_armor(const struct game_s *g, player_id_t owner)
{
    const empiretechorbit_t *e = &(g->eto[owner]);
    const shipresearch_t *srd = &(g->srd[owner]);
    uint8_t best = 0;
    for (int i = 0; i <= e->tech.completed[TECH_FIELD_CONSTRUCTION]; ++i) {
        switch (srd->researchcompleted[TECH_FIELD_CONSTRUCTION][i]) {
            case TECH_CONS_DURALLOY_ARMOR:
                best = 1;
                break;
            case TECH_CONS_ZORTIUM_ARMOR:
                best = 2;
                break;
            case TECH_CONS_ANDRIUM_ARMOR:
                best = 3;
                break;
            case TECH_CONS_TRITANIUM_ARMOR:
                best = 4;
                break;
            case TECH_CONS_ADAMANTIUM_ARMOR:
                best = 5;
                break;
            case TECH_CONS_NEUTRONIUM_ARMOR:
                best = 6;
                break;
            default:
                break;
        }
    }
    return best;
}

/* -------------------------------------------------------------------------- */

void game_parsed_from_design(shipparsed_t *sp, const shipdesign_t *sd, int num)
{
    uint8_t extraman;
    lib_strcpy(sp->name, sd->name, SHIP_NAME_LEN);
    COPY_PROP(sp, sd, hull);
    COPY_PROP(sp, sd, comp);
    COPY_PROP(sp, sd, jammer);
    COPY_PROP(sp, sd, shield);
    COPY_PROP(sp, sd, armor);
    COPY_PROP(sp, sd, engine);
    COPY_PROP_TBL(sp, sd, special, SPECIAL_SLOT_NUM);
    sp->pshield = 0;
    COPY_PROP_TBL(sp, sd, wpnt, WEAPON_SLOT_NUM);
    COPY_PROP_TBL(sp, sd, wpnn, WEAPON_SLOT_NUM);
    COPY_PROP(sp, sd, hp);
    COPY_MAX_SPECIAL(sp->repair, sd, repair);
    COPY_MAX_SPECIAL(extraman, sd, extraman);
    sp->man = (sd->man + 3) / 2 + extraman / 2;
    sp->complevel = tbl_shiptech_comp[sd->comp].level;
    sp->defense = sd->man + tbl_shiptech_hull[sd->hull].defense + extraman + 1;
    sp->misdefense = sp->defense + tbl_shiptech_jammer[sd->jammer].level;
    sp->absorb = tbl_shiptech_shield[sd->shield].absorb;
    COPY_MAX_SPECIAL(sp->misshield, sd, misshield);
    COPY_MAX_SPECIAL(sp->extrarange, sd, extrarange);
    sp->num = num;
    COPY_PROP(sp, sd, look);
    COPY_MAX_SPECIAL(sp->pulsar, sd, pulsar);
    COPY_MAX_SPECIAL(sp->stream, sd, stream);
    {
        uint16_t m = 0;
        for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
            m |= tbl_shiptech_special[sd->special[i]].boolmask;
        }
        sp->sbmask = m;
        if (m & (1 << SHIP_SPECIAL_BOOL_SCANNER)) {
            ++sp->complevel;
        }
    }
}

void game_parsed_from_planet(shipparsed_t *sp, const struct game_s *g, const struct planet_s *p)
{
    const empiretechorbit_t *e = &(g->eto[p->owner]);
    memset(sp, 0, sizeof(*sp));
    lib_strcpy(sp->name, p->name, SHIP_NAME_LEN);
    sp->comp = e->base_comp + 1;
    sp->complevel = sp->comp;
    if (e->race == RACE_MRRSHAN) {
        sp->complevel += 4;
    }
    sp->shield = e->base_shield;
    sp->absorb = tbl_shiptech_shield[e->base_shield].absorb;
    sp->hp = game_num_base_hp[get_best_armor(g, p->owner)];
    sp->defense = 1;
    sp->misdefense = game_get_best_jammer(g, p->owner, e->tech.percent[TECH_FIELD_COMPUTER]) + 1;
    if (p->missile_bases > 0) {
        uint16_t m = (1 << SHIP_SPECIAL_BOOL_SCANNER);
        sp->num = p->missile_bases;
        if (e->have_sub_space_int) {
            m |= (1 << SHIP_SPECIAL_BOOL_SUBSPACE);
        }
        sp->sbmask = m;
    }
    sp->pulsar = e->antidote;   /* HACK */
    sp->look = p->infogfx;
    sp->pshield = p->shield;
    sp->wpnt[0] = e->base_weapon;
    sp->wpnn[0] = 3;
    sp->wpnt[1] = game_get_base_weapon_2(g, p->owner, e->tech.percent[TECH_FIELD_WEAPON], e->base_weapon);
    sp->wpnn[1] = 3;
    if (p->battlebg == 0) {
        sp->pshield = 0;
        sp->absorb = 0;
        sp->shield = 0;
    }
}
