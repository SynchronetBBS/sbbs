#include "config.h"

#include "game_bomb.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_num.h"
#include "game_shiptech.h"
#include "game_tech.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */

static void game_turn_bomb_damage(struct game_s *g, uint8_t pli, player_id_t attacker, int *popdmgptr, int *factdmgptr, int *biodmgptr)
{
    const planet_t *p = &(g->planet[pli]);
    const empiretechorbit_t *ea = &(g->eto[attacker]);
    const empiretechorbit_t *ed = &(g->eto[p->owner]);
    uint32_t tbl[WEAPON_NUM];
    uint8_t pshield = p->shield, antidote = ed->antidote;
    int totaldmg = 0, totalbio = 0, maxcomp = 0, complevel;
    memset(tbl, 0, sizeof(tbl));
    for (int i = 0; i < ea->shipdesigns_num; ++i) {
        const shipdesign_t *sd = &(g->srd[attacker].design[i]);
        if ((!game_num_orbital_comp_fix) || (ea->orbit[pli].ships[i] != 0)) { /* WASBUG bonus from non-existing ships */
            SETMAX(maxcomp, sd->comp);
        }
        for (int j = 0; j < (game_num_orbital_weap_4 ? WEAPON_SLOT_NUM : (WEAPON_SLOT_NUM - 1)); ++j) { /* WASBUG? last weapon not used */
            weapon_t wpnt = sd->wpnt[j];
            uint32_t v;
            v = ea->orbit[pli].ships[i] * sd->wpnn[j];
            if (v != 0) {
                int ns;
                ns = tbl_shiptech_weap[wpnt].numshots;
                if (ns == -1) {
                    ns = 30;
                }
                v *= ns;
                tbl[wpnt] += v;
            }
        }
    }
    complevel = (maxcomp - 1) * 2 + 6;
    SETRANGE(complevel, 1, 20);
    for (int i = 0; i < (game_num_orbital_weap_any ? WEAPON_NUM : WEAPON_CRYSTAL_RAY); ++i) {  /* WASBUG? excludes death ray and amoeba stream too */
        uint32_t vcur = tbl[i];
        if (vcur != 0) {
            const struct shiptech_weap_s *w = &(tbl_shiptech_weap[i]);
            uint32_t v;
            int dmgmin, dmgmax;
            SETMIN(vcur, game_num_max_bomb_dmg);
            if (vcur < 10) {
                v = 1;
            } else {
                v = vcur / 10;
                vcur = 10;
            }
            dmgmin = w->damagemin;
            dmgmax = w->damagemax;
            if (dmgmin == dmgmax) {
                if (w->is_bio) {
                    for (uint32_t n = 0; n < vcur; ++n) {
                        if (rnd_1_n(20, &g->seed) <= complevel) {
                            int dmg;
                            dmg = rnd_0_nm1(dmgmin + 1, &g->seed) - antidote;
                            SETMAX(dmg, 0);
                            totalbio += v * dmg;
                            SETMIN(totalbio, game_num_max_bio_dmg);
                        }
                    }
                } else {
                    if (game_num_orbital_torpedo == (w->misstype != 0)) { /* WASBUG damage halving for torpedo affected missiles instead */
                        dmgmax /= 2;
                    }
                    dmgmax *= w->nummiss;
                    if (dmgmax > pshield) {
                        for (uint32_t n = 0; n < vcur; ++n) {
                            if (rnd_1_n(20, &g->seed) <= complevel) {
                                int dmg;
                                dmg = dmgmax - pshield;
                                totaldmg += v * dmg;
                            }
                        }
                    }
                }
            } else {
                /*dd3d*/
                if (!w->is_bomb) {
                    dmgmin /= 2;
                    dmgmax /= 2;
                }
                if (dmgmax > pshield) {
                    int dmgrange;
                    dmgrange = dmgmax - dmgmin + 1;
                    dmgmin = dmgmin - 1 - pshield;
                    vcur = (complevel * vcur * v) / 20;
                    for (uint32_t n = 0; n < vcur; ++n) {
                        int dmg;
                        dmg = rnd_1_n(dmgrange, &g->seed) + dmgmin;
                        if (dmg > 0) {
                            totaldmg += dmg;
                        }
                    }
                }
            }
        }
    }
    /*de00*/
    {
        int v, h, hr, hb;
        h = game_num_fact_hp / 5;
        hr = h * 2;
        hb = game_num_fact_hp - h;
        v = totaldmg / (rnd_1_n(hr, &g->seed) + hb);
        SETMIN(v, p->factories);
        *factdmgptr = v;
    }
    {
        int v, h, hr, hb;
        h = game_num_pop_hp / 5;
        hr = h * 2;
        hb = game_num_pop_hp - h;
        v = totaldmg / (rnd_1_n(hr, &g->seed) + hb);
        v += totalbio;
        SETMIN(v, p->pop);
        *popdmgptr = v;
    }
    {
        *biodmgptr = totalbio;
    }
}

/* -------------------------------------------------------------------------- */

void game_turn_bomb(struct game_s *g)
{
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        player_id_t owner;
        owner = p->owner;
        if (owner == PLAYER_NONE) {
            continue;
        }
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            empiretechorbit_t *ea = &(g->eto[i]);
            bool flag_treaty;
            int v4;
            if (i == owner) {
                continue;
            }
            v4 = 0;
            for (int j = 0; j < ea->shipdesigns_num; ++j) {
                if (ea->orbit[pli].ships[j] > 0) {
                    v4 += 2;
                }
            }
            flag_treaty = false;
            if ((ea->treaty[owner] == TREATY_ALLIANCE) || ((ea->treaty[owner] == TREATY_NONAGGRESSION) && IS_AI(g, i))) {
                flag_treaty = true;
            }
            if ((v4 > 0) && (!flag_treaty)) {
                int pop_inbound, popdmg, factdmg, biodmg;
                bool flag_do_bomb, flag_play_music;
                pop_inbound = 0;
                for (int j = 0; j < g->transport_num; ++j) {
                    transport_t *r = &(g->transport[j]);
                    if ((r->owner == i) && (r->dest == pli)) {
                        pop_inbound += r->pop;
                    }
                }
                /*cf52*/
                flag_play_music = true;
                game_turn_bomb_damage(g, pli, i, &popdmg, &factdmg, &biodmg);
                if ((popdmg == 0) && (factdmg == 0)) {
                    flag_do_bomb = false;
                } else if (IS_HUMAN(g, i)) {
                    flag_do_bomb = ui_bomb_ask(g, i, pli, pop_inbound);
                    flag_play_music = false;
                } else {
                    flag_do_bomb = game_ai->bomb(g, i, pli, pop_inbound);
                }
                /*d004*/
                if (flag_do_bomb || !game_num_orbital_bio_fix) {
                    if (biodmg) {
                        int v;
                        v = p->max_pop3 - biodmg;
                        SETMAX(v, 10);
                        p->max_pop3 = v;
                    }
                }
                if (flag_do_bomb && ((popdmg > 0) || (factdmg > 0))) {
                    p->pop -= popdmg;
                    p->factories -= factdmg;
                    SUBSAT0(p->rebels, popdmg / 2 + 1);
                    if (p->pop == 0) {
                        game_planet_destroy(g, pli, i);
                    }
                    if (IS_HUMAN(g, i) || IS_HUMAN(g, owner)) {
                        player_id_t human;
                        bool hide_other;
                        human = IS_HUMAN(g, i) ? i : owner;
                        hide_other = (IS_HUMAN(g, i) && IS_HUMAN(g, owner));
                        ui_bomb_show(g, human, i, owner, pli, popdmg, factdmg, flag_play_music, hide_other);
                    }
                    if ((p->pop == 0) && IS_HUMAN(g, i)) {
                        int dv;
                        if (ea->treaty[owner] < TREATY_WAR) {
                            game_diplo_start_war_swap(g, owner, i);
                            dv = 13;
                        } else {
                            dv = 10;
                        }
                        game_diplo_act(g, -50 - rnd_1_n(50, &g->seed), i, owner, dv, pli, 0);
                    } else {
                        /*d118*/
                        game_diplo_battle_finish(g, owner, i, popdmg, 0, biodmg, 0, pli);
                    }
                }
            }
        }
    }
}
