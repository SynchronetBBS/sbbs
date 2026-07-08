#include "config.h"

#include "game_event.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_design.h"
#include "game_diplo.h"
#include "game_end.h"
#include "game_fleet.h"
#include "game_misc.h"
#include "game_new.h"
#include "game_news.h"
#include "game_num.h"
#include "game_shiptech.h"
#include "game_stat.h"
#include "game_str.h"
#include "game_tech.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "ui.h"
#include "util.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

bool game_opt_skip_random_news = false;

/* -------------------------------------------------------------------------- */

static player_id_t game_event_new_get_victim(struct game_s *g)
{
    int tbl_planets[PLAYER_NUM], min_planets = 10000, r, sum = 0;
    player_id_t player;
    memset(tbl_planets, 0, sizeof(tbl_planets));
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        const planet_t *p = &(g->planet[pli]);
        if (p->owner != PLAYER_NONE) {
            ++tbl_planets[p->owner];
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if (tbl_planets[i] < 4) {
            tbl_planets[i] = 0;
        } else if (IS_HUMAN(g, i)) {
            tbl_planets[i] *= 5;
        }
    }
    for (player_id_t i = PLAYER_0; i < PLAYER_NUM; ++i) {   /* FIXME BUG? on < 6 players min is always 0 */
        SETMIN(min_planets, tbl_planets[i]);
    }
    SETMAX(min_planets, 5);
    for (player_id_t i = PLAYER_0; i < PLAYER_NUM; ++i) {
        tbl_planets[i] /= min_planets;
        if (i < g->players) {
            SETMAX(tbl_planets[i], 1);
            sum += tbl_planets[i];
        }
    }
    r = rnd_0_nm1(sum, &g->seed);
    for (player = PLAYER_0; (r >= 0) && (player < (g->players - 1));) {
        r -= tbl_planets[player];
        if (r >= 0) {
             ++player;
        }
    }
    return player;
}

static player_id_t game_event_new_get_trader(const struct game_s *g, player_id_t player)
{
    const empiretechorbit_t *e = &(g->eto[player]);
    player_id_t trader = PLAYER_NONE;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if ((i != player) && BOOLVEC_IS1(e->contact, i) && (e->trade_bc[i] != 0)) {
            trader = i;
        }
    }
    return trader;
}

static void game_monster_set_start(struct game_s *g, monster_t *m)
{
    int x, y, v;
    uint8_t dest;
    switch (rnd_0_nm1(4, &g->seed)) {
        case 0:
            v = rnd_0_nm1(g->galaxy_h, &g->seed);
            dest = v * g->galaxy_w;
            x = 0;
            y = (v - 1) * 32 + rnd_1_n(12, &g->seed) + 10;
            break;
        case 1:
            v = rnd_0_nm1(g->galaxy_w, &g->seed);
            dest = v;
            x = (v - 1) * 28 + rnd_1_n(12, &g->seed) + 30;
            y = 0;
            break;
        case 2:
            v = rnd_0_nm1(g->galaxy_h, &g->seed);
            dest = (v + 1) * g->galaxy_w - 1;
            x = g->galaxy_w * 28;
            y = (v - 1) * 32 + rnd_1_n(12, &g->seed) + 10;
            break;
        default:
            v = rnd_0_nm1(g->galaxy_w, &g->seed);
            dest = (g->galaxy_h - 1) * g->galaxy_w + v;
            x = (v - 1) * 28 + rnd_1_n(12, &g->seed) + 30;
            y = g->galaxy_h * 32;
            break;
    }
    m->x = x;
    m->y = y;
    m->dest = dest;
}

static void game_monster_set_next_dest(struct game_s *g, monster_t *m)
{
    uint8_t olddest = m->dest;
    int dest = olddest, w = g->galaxy_w, h = g->galaxy_h;
    while (dest == olddest) {
        int x;
        x = olddest % w;
        if (x == 0) {
            dest += rnd_0_nm1(2, &g->seed);
        } else {
            /*7da47*/
            dest += rnd_1_n((x == (w - 1)) ? 2 : 3, &g->seed) - 2;
        }
        /*7da6a*/
        if (olddest < w) {
            dest += rnd_0_nm1(2, &g->seed) * w;
        } else {
            /*7da86*/
            dest += rnd_1_n(((w * h - w - 1) < olddest) ? 2 : 3, &g->seed) * w - w * 2;
        }
        /*7dab3*/
        if (0
          || (dest < 0) || (dest >= g->galaxy_stars)
          || (g->planet[dest].type == PLANET_TYPE_NOT_HABITABLE)
          || (dest == g->evn.planet_orion_i)
        ) {
            dest = olddest;
        }
    }
    m->dest = dest;
}

static void game_event_kill_player(struct game_s *g, player_id_t pi)
{
    g->evn.home[pi] = PLANET_NONE;
    game_remove_player_fleets(g, pi);
    BOOLVEC_SET0(g->refuse, pi);
}

/* -------------------------------------------------------------------------- */

void game_event_new(struct game_s *g)
{
    gameevent_type_t type;
    int chance;
    player_id_t player;
    const empiretechorbit_t *e;
    uint8_t planet;
    planet_t *p;
    if (g->gaux->flag_cheat_events || (game_num_event_roll == 0)) {
        g->evn.have_plague = 0;
        g->evn.have_quake = false;
        g->evn.have_nova = 0;
        g->evn.have_accident = 0;
        g->evn.have_assassin = false;
        g->evn.have_virus = false;
        g->evn.have_comet = 0;
        g->evn.have_pirates = 0;
        g->evn.have_derelict = false;
        g->evn.crystal.exists = 0;
        g->evn.amoeba.exists = 0;
        g->evn.have_enviro = false;
        g->evn.have_rich = false;
        g->evn.have_support = false;
        g->evn.have_poor = false;
        return;
    }
    if (g->evn.year > g->year) {
        chance = 0;
    } else {
        chance = g->year - g->evn.year;
    }
    /*eb0c*/
    switch (g->difficulty) {
        case DIFFICULTY_SIMPLE:
            chance /= 2;
            break;
        case DIFFICULTY_EASY:
            chance = (chance * 2) / 3;
            break;
        case DIFFICULTY_AVERAGE:
            chance = (chance * 3) / 4;
            break;
        case DIFFICULTY_HARD:
            chance = (chance * 4) / 5;
            break;
        default:
            break;
    }
    if (rnd_1_n(game_num_event_roll, &g->seed) > chance) {
        return;
    }
    player = game_event_new_get_victim(g);
    if ((player < PLAYER_0) || (player >= g->players)) {
        return;
    }
    e = &(g->eto[player]);
    planet = game_planet_get_random(g, player);
    if ((planet == g->evn.planet_orion_i) || (planet == PLANET_NONE)) {
        return;
    }
    p = &(g->planet[planet]);
    type = GAME_EVENT_NONE;
    for (int loops = 0; (loops < 5) && (type == GAME_EVENT_NONE); ++loops) {
        type = rnd_1_n(GAME_EVENT_NUM - 1, &g->seed);
        if (0
          || BOOLVEC_IS1(g->evn.done, type)
          || ((type == GAME_EVENT_REBELLION) && (!IS_HUMAN(g, player) || (g->evn.home[player] == planet)))
          || ((type == GAME_EVENT_QUAKE) && (p->pop < 50))
          || (((type == GAME_EVENT_CRYSTAL) || (type == GAME_EVENT_AMOEBA)) && (g->year < 200))
          || (((type == GAME_EVENT_CRYSTAL) || (type == GAME_EVENT_AMOEBA)) && (g->evn.crystal.exists || g->evn.amoeba.exists))
          || ((type == GAME_EVENT_DERELICT) && IS_HUMAN(g, player))
          || ((type == GAME_EVENT_ASSASSIN) && (g->end != GAME_END_NONE))
          || ((type == GAME_EVENT_COMET) && (g->year < 100))
          || ((type == GAME_EVENT_PIRATES) && (game_event_new_get_trader(g, player) == PLAYER_NONE))
          || (((type == GAME_EVENT_ACCIDENT) || (type == GAME_EVENT_PLAGUE)) && (e->race == RACE_SILICOID))
          /* The OSG claims that the industrial accident event only happens on
           * planets with at least 30 factories. This is not true for MOO 1.3,
           * but we optionally check for it if accident_chk_factories is set in
           * a pbx file. */
          || ((type == GAME_EVENT_ACCIDENT) && game_num_accident_chk_factories && (p->factories < 30))
          || ((type == GAME_EVENT_ENVIRO) && (p->type < PLANET_TYPE_MINIMAL))
          || ((type == GAME_EVENT_ENVIRO) && ((p->growth >= PLANET_GROWTH_FERTILE) || (p->special > PLANET_SPECIAL_NORMAL)))
          || ((type == GAME_EVENT_RICH) && (p->special > PLANET_SPECIAL_NORMAL))
          || ((type == GAME_EVENT_RICH) && (p->special == PLANET_SPECIAL_ARTIFACTS))
          || ((type == GAME_EVENT_POOR) && (p->special < PLANET_SPECIAL_NORMAL))
          || ((type == GAME_EVENT_POOR) && (p->special == PLANET_SPECIAL_ARTIFACTS))
        ) {
            type = GAME_EVENT_NONE;
        }
        if (type == GAME_EVENT_ASSASSIN) {
            bool found;
            found = false;
            for (player_id_t i = PLAYER_0; i < g->players; ++i) {
                if (1
                  && (i != player) && IS_AI(g, i)
                  && (e->treaty[i] >= TREATY_NONAGGRESSION)
                  && (e->treaty[i] <= TREATY_ALLIANCE)
                ) {
                    found = true;
                }
            }
            if (!found) {
                type = GAME_EVENT_NONE;
            }
        }
    }
    /*edef*/
    if (type == GAME_EVENT_NONE) {
        return;
    }
    g->evn.year = g->year;
    BOOLVEC_SET1(g->evn.done, type);
#if 0   /* result is unused */
    switch (g->difficulty) {
        case DIFFICULTY_SIMPLE:
            va = g->year / 20;
            break;
        case DIFFICULTY_EASY:
            va = g->year / 18;
            break;
        case DIFFICULTY_AVERAGE:
            va = g->year / 15;
            break;
        case DIFFICULTY_HARD:
            va = g->year / 13;
            break;
        case DIFFICULTY_IMPOSSIBLE:
            va = g->year / 10;
            break;
        default:
            break;
    }
#endif
    switch (type) {
        case GAME_EVENT_PLAGUE:
            g->evn.have_plague = 1;
            g->evn.plague_player = player;
            g->evn.plague_planet_i = planet;
            g->evn.plague_val = (rnd_1_n(8, &g->seed) + g->difficulty * 2) * p->prod_after_maint;
            break;
        case GAME_EVENT_QUAKE:
            g->evn.have_quake = true;
            g->evn.quake_player = player;
            g->evn.quake_planet_i = planet;
            break;
        case GAME_EVENT_NOVA:
            g->evn.have_nova = 1;
            g->evn.nova_player = player;
            g->evn.nova_planet_i = planet;
            g->evn.nova_years = rnd_1_n(5, &g->seed) + 10 - g->difficulty;
            g->evn.nova_val = (rnd_1_n(5, &g->seed) + 10 - g->difficulty) * p->prod_after_maint;
            break;
        case GAME_EVENT_ACCIDENT:
            g->evn.have_accident = 1;
            g->evn.accident_player = player;
            g->evn.accident_planet_i = planet;
            break;
        case GAME_EVENT_ASSASSIN:
            g->evn.have_assassin = true;
            g->evn.assassin_player = player;
            {
                player_id_t player2;
                player2 = PLAYER_NONE;
                for (player_id_t i = PLAYER_0; i < g->players; ++i) {
                    if (1
                      && (i != player) && IS_AI(g, i)
                      && (e->treaty[i] >= TREATY_NONAGGRESSION)
                      && (e->treaty[i] <= TREATY_ALLIANCE)
                    ) {
                        player2 = i;
                    }
                }
                g->evn.assassin_player2 = player2;
            }
            break;
        case GAME_EVENT_VIRUS:
            g->evn.have_virus = true;
            g->evn.virus_player = player;
            {
                int v;
                v = rnd_0_nm1(4, &g->seed);
                switch (v) {
                    case 0: v = TECH_FIELD_COMPUTER; break;
                    case 1: v = TECH_FIELD_FORCE_FIELD; break;
                    case 2: v = TECH_FIELD_PROPULSION; break;
                    default: v = TECH_FIELD_WEAPON; break;
                }
                g->evn.virus_field = v;
            }
            break;
        case GAME_EVENT_COMET:
            g->evn.have_comet = 1;
            g->evn.comet_player = player;
            g->evn.comet_planet_i = planet;
            g->evn.comet_years = rnd_1_n(5, &g->seed) + 10 - g->difficulty;
            g->evn.comet_hp = (rnd_1_n(5, &g->seed) + 10 + g->difficulty) * 25;
            g->evn.comet_dmg = 0;
            break;
        case GAME_EVENT_PIRATES:
            g->evn.have_pirates = 1;
            g->evn.pirates_planet_i = rnd_0_nm1(g->galaxy_stars, &g->seed);
            g->evn.pirates_hp = (rnd_1_n(5, &g->seed) + 10 + g->difficulty) * 30;
            break;
        case GAME_EVENT_DERELICT:
            g->evn.have_derelict = true;
            g->evn.derelict_player = player;
            break;
        case GAME_EVENT_REBELLION:
            p->rebels = p->pop / 2;
            p->unrest = PLANET_UNREST_REBELLION;
            p->unrest_reported = false;
            break;
        case GAME_EVENT_CRYSTAL:
        case GAME_EVENT_AMOEBA:
            {
                monster_t *m = (type == GAME_EVENT_CRYSTAL) ? &(g->evn.crystal) : &(g->evn.amoeba);
                m->exists = 1;
                m->counter = 2;
                m->nuked = 0;
                m->killer = PLAYER_NONE;
                game_monster_set_start(g, m);
            }
            break;
        case GAME_EVENT_ENVIRO:
            g->evn.have_enviro = true;
            g->evn.enviro_planet_i = planet;
            break;
        case GAME_EVENT_RICH:
            g->evn.have_rich = true;
            g->evn.rich_planet_i = planet;
            break;
        case GAME_EVENT_SUPPORT:
            g->evn.have_support = true;
            g->evn.support_player = player;
            break;
        case GAME_EVENT_POOR:
            g->evn.have_poor = true;
            g->evn.poor_planet_i = planet;
            break;
        default:
            break;
    }
}

bool game_event_run(struct game_s *g, struct game_end_s *ge)
{
    struct news_s ns;
    bool any_news = false;
    ui_news_start();
    if (g->evn.have_plague) {
        uint8_t pli = g->evn.plague_planet_i;
        planet_t *p = &(g->planet[pli]);
        player_id_t player = g->evn.plague_player;
        if (p->owner == player) {
            empiretechorbit_t *e = &(g->eto[player]);
            if (g->evn.have_plague != 1) {
                int v;
                v = game_get_tech_prod(p->prod_after_maint, p->slider[PLANET_SLIDER_TECH], e->race, p->special);
                g->evn.plague_val -= v;
                v = ((rnd_1_n(6, &g->seed) + 4) * p->pop) / 100;
                SETMIN(v, p->pop);
                p->pop -= v;
                ns.num1 = v;
            } else {
                ns.num1 = 0;   /* WASBUG MOO1 uses uninitialized stack variable */
            }
            ns.num2 = g->evn.plague_val;
            ns.type = GAME_NEWS_PLAGUE;
            ns.planet_i = pli;
            ns.race = e->race;
            if (g->evn.plague_val <= 0) {
                g->evn.have_plague = 3;
            }
            switch (g->evn.have_plague) {
                case 1:
                    ns.subtype = IS_HUMAN(g, player) ? 0 : 4;
                    break;
                case 2:
                    ns.subtype = (IS_HUMAN(g, player) && (!rnd_0_nm1(10, &g->seed) || g->gaux->flag_cheat_news)) ? 1 : -1;
                    break;
                case 3:
                    ns.subtype = IS_HUMAN(g, player) ? 2 : 5;
                    break;
                default:
                    ns.subtype = -1;
                    break;
            }
            /*f48e*/
            if (g->evn.have_plague == 1) {
                g->evn.have_plague = 2;
            }
            if (g->evn.have_plague == 3) {
                g->evn.have_plague = 0;
            }
            if (ns.subtype != -1) {
                ui_news(g, &ns);
                any_news = true;
            }
            if (IS_AI(g, player)) {
                game_ai->plague(g, pli);
            }
        } else {
            g->evn.have_plague = 0;
        }
    }
    /*f4ec*/
    if (g->evn.have_quake) {
        uint8_t pli = g->evn.quake_planet_i;
        planet_t *p = &(g->planet[pli]);
        player_id_t player = g->evn.quake_player;
        int vp, vf;
        ns.planet_i = pli;
        vp = ((rnd_1_n(10, &g->seed) + 20) * p->pop) / 100;
        vf = ((rnd_1_n(50, &g->seed) + 30) * p->factories) / 100;
        SETMIN(vp, p->pop);
        SETMIN(vf, p->factories);
        ns.num1 = vp;
        ns.num2 = vf;
        ns.type = GAME_NEWS_QUAKE;
        ns.planet_i = pli;
        ns.race = g->eto[p->owner].race;
        g->evn.have_quake = false;
        p->pop -= vp;
        p->factories -= vf;
        ns.subtype = IS_HUMAN(g, player) ? 0 : 4;
        ui_news(g, &ns);
        any_news = true;
    }
    /*f662*/
    if (g->evn.have_nova) {
        uint8_t pli = g->evn.nova_planet_i;
        planet_t *p = &(g->planet[pli]);
        player_id_t player = p->owner;
        if ((g->evn.have_nova != 1) && (player != PLAYER_NONE)) {
            empiretechorbit_t *e = &(g->eto[player]);
            int v;
            v = game_get_tech_prod(p->prod_after_maint, p->slider[PLANET_SLIDER_TECH], e->race, p->special);
            g->evn.nova_val -= v;
            if (g->evn.nova_val <= 0) {
                g->evn.have_nova = 3;
            }
            --g->evn.nova_years;
        }
        /*f718*/
        if ((g->evn.nova_years <= 0) && (g->evn.have_nova != 3)) {
            /*f734*/
            int v = rnd_1_n(10, &g->seed) + 10;
            p->max_pop2 = v;
            p->max_pop3 = v;
            p->type = PLANET_TYPE_RADIATED;
            p->growth = PLANET_GROWTH_HOSTILE;
            v = rnd_1_n(v, &g->seed);
            SETMIN(p->pop, v);
            if (player == PLAYER_NONE) {
                p->pop = 0;
            }
            v = ((rnd_1_n(3, &g->seed) + 1) * p->factories) / 20;
            SETMIN(p->factories, v);
            g->evn.have_nova = 3;
        }
        /*f878*/
        ns.num1 = g->evn.nova_years;
        ns.num2 = g->evn.nova_val;
        ns.type = GAME_NEWS_NOVA;
        ns.planet_i = pli;
        ns.race = (player != PLAYER_NONE) ? g->eto[player].race : RACE_HUMAN; /* WASBUG MOO1 does not check for none, taking race from eto[-1] */
        switch (g->evn.have_nova) {
            case 1:
                ns.subtype = IS_HUMAN(g, player) ? 0 : 4;
                break;
            case 2:
                ns.subtype = (IS_HUMAN(g, player) && ((!rnd_0_nm1(10, &g->seed)) || (g->evn.nova_years < 3) || g->gaux->flag_cheat_news)) ? 1 : -1;
                break;
            case 3:
                ns.subtype = IS_HUMAN(g, player) ? 2 : 5;
                if (g->evn.nova_val > 0) {
                    ++ns.subtype;
                }
                break;
            default:
                ns.subtype = -1;
                break;
        }
        if (g->evn.have_nova == 1) {
            g->evn.have_nova = 2;
        }
        if (g->evn.have_nova == 3) {
            g->evn.have_nova = 0;
        }
        if (ns.subtype != -1) {
            ui_news(g, &ns);
            any_news = true;
        }
        if (IS_AI(g, player)) {
            game_ai->nova(g, pli);
        }
    }
    /*f986*/
    if (g->evn.have_accident) {
        uint8_t pli = g->evn.accident_planet_i;
        planet_t *p = &(g->planet[pli]);
        player_id_t player = g->evn.accident_player;
        ns.planet_i = pli;
        ns.type = GAME_NEWS_ACCIDENT;
        ns.race = g->eto[player].race;
        if (g->evn.have_accident == 1) {
            p->max_pop3 /= 2;
            SETMIN(p->max_pop2, p->max_pop3);
            p->waste = p->max_pop3 - 10;
            p->type = PLANET_TYPE_RADIATED;
            p->growth = PLANET_GROWTH_HOSTILE;
            ns.subtype = IS_HUMAN(g, player) ? 0 : 4;
            ui_news(g, &ns);
            any_news = true;
            g->evn.have_accident = 2;
        } else if (p->waste == 0) {
            /*fac7*/
            ns.subtype = IS_HUMAN(g, player) ? 1 : 5;
            g->evn.have_accident = 0;
            ui_news(g, &ns);
            any_news = true;
        }
    }
    /*fb20*/
    if (g->evn.have_assassin) {
        player_id_t player = g->evn.assassin_player;
        player_id_t player2 = g->evn.assassin_player2;
        empiretechorbit_t *e = &(g->eto[player]);
        ns.type = GAME_NEWS_ASSASSIN;
        ns.race = e->race;
        ns.num1 = player2;
        game_diplo_start_war(g, player2, player);
        if (IS_HUMAN(g, player)) {
            e->diplo_type[player2] = 13;
            e->diplo_val[player2] = 100;
        }
        g->evn.have_assassin = false;
        ns.subtype = 0;
        ui_news(g, &ns);
        any_news = true;
    }
    /*fba9*/
    if (g->evn.have_virus) {
        player_id_t player = g->evn.virus_player;
        empiretechorbit_t *e = &(g->eto[player]);
        ns.type = GAME_NEWS_VIRUS;
        ns.race = e->race;
        ns.num1 = g->evn.virus_field;
        ns.num2 = e->tech.investment[g->evn.virus_field];
        /* MOO1 limits the shown loss to 32000 and also sets it to 100 if 0 */
        e->tech.investment[g->evn.virus_field] = 0;
        g->evn.have_virus = false;
        ns.subtype = 4;
        ui_news(g, &ns);
        any_news = true;
    }
    /*fc6e*/
    if (g->evn.have_comet) {
        uint8_t pli = g->evn.comet_planet_i;
        planet_t *p = &(g->planet[pli]);
        player_id_t player = p->owner;
        int dmg = 0;
        if (player != PLAYER_NONE) {    /* BUG? only colonized planets can fight off comets */
            const empiretechorbit_t *e = &(g->eto[player]);
            const shipdesign_t *sd = &(g->srd[player].design[0]);
            for (int i = 0; i < e->shipdesigns_num; ++i) {
                dmg += rnd_1_n(game_num_tbl_hull_w[sd->hull], &g->seed) * e->orbit[pli].ships[i];
            }
            ns.race = e->race;
        } else {
            ns.race = RACE_HUMAN;   /* WASBUG MOO1 does not check for none, taking race from eto[-1] */
        }
        if ((g->evn.have_comet != 1) && (player != PLAYER_NONE)) {
            dmg += g->evn.comet_dmg;
            if (dmg > g->evn.comet_hp) {
                g->evn.comet_dmg = g->evn.comet_hp;
                g->evn.have_comet = 3;
            } else {
                g->evn.comet_dmg = dmg;
            }
            --g->evn.comet_years;
        }
        if ((g->evn.comet_years <= 0) && (g->evn.have_comet != 3)) {
            game_planet_destroy(g, pli, PLAYER_NONE);
            p->type = PLANET_TYPE_NOT_HABITABLE;
            p->growth = PLANET_GROWTH_HOSTILE;
            g->evn.have_comet = 3;
        }
        /*fde3*/
        ns.num1 = g->evn.comet_years;
        ns.num2 = (g->evn.comet_dmg * 100) / g->evn.comet_hp;
        ns.type = GAME_NEWS_COMET;
        ns.planet_i = pli;
        switch (g->evn.have_comet) {
            case 1:
                ns.subtype = IS_HUMAN(g, player) ? 0 : 4;
                break;
            case 2:
                ns.subtype = (IS_HUMAN(g, player) && ((!rnd_0_nm1(10, &g->seed)) || (g->evn.comet_years < 3) || g->gaux->flag_cheat_news)) ? 1 : -1;
                break;
            case 3:
                ns.subtype = IS_HUMAN(g, player) ? 2 : 5;
                if (g->evn.comet_dmg < g->evn.comet_hp) {
                    ++ns.subtype;
                }
                break;
            default:
                ns.subtype = -1;
                break;
        }
        if (g->evn.have_comet == 1) {
            g->evn.have_comet = 2;
        }
        if (g->evn.have_comet == 3) {
            g->evn.have_comet = 0;
        }
        if (ns.subtype != -1) {
            ui_news(g, &ns);
            any_news = true;
        }
        if (IS_AI(g, p->owner)) {
            game_ai->comet(g, pli);
        }
    }
    /*ff19*/
    if (g->evn.have_pirates) {
        uint8_t pli = g->evn.pirates_planet_i;
        planet_t *p = &(g->planet[pli]);
        int dmg = 0, setback;
        for (monster_id_t i = MONSTER_CRYSTAL; i <= MONSTER_AMOEBA; ++i) {
            monster_t *m = (i == MONSTER_CRYSTAL) ? &(g->evn.crystal) : &(g->evn.amoeba);
            if (m->exists && (m->x == p->x) && (m->y == p->y) && (m->killer == PLAYER_NONE)) {
                dmg = 32000;
            }
        }
        for (player_id_t player = PLAYER_0; player < g->players; ++player) {
            const empiretechorbit_t *e = &(g->eto[player]);
            const shipdesign_t *sd = &(g->srd[player].design[0]);
            for (int i = 0; i < e->shipdesigns_num; ++i) {
                dmg += rnd_1_n(game_num_tbl_hull_w[sd->hull], &g->seed) * e->orbit[pli].ships[i];
            }
            SETMIN(dmg, 32000);
        }
        SUBSAT0(g->evn.pirates_hp, dmg);
        ns.planet_i = pli;
        ns.num1 = setback = g->evn.pirates_hp / 6;
        if (g->evn.pirates_hp == 0) {
            g->evn.have_pirates = 3;
        }
        for (player_id_t p1 = PLAYER_0; p1 < g->players; ++p1) {
            empiretechorbit_t *e1 = &(g->eto[p1]);
            if (p->within_frange[p1] != 1) {
                continue;
            }
            for (player_id_t p2 = PLAYER_0; p2 < g->players; ++p2) {
                empiretechorbit_t *e2 = &(g->eto[p2]);
                if (p1 == p2) {
                    continue;
                }
                e1->trade_percent[p2] = (e1->trade_percent[p2] * (100 - setback)) / 100;
                e2->trade_percent[p1] = (e2->trade_percent[p1] * (100 - setback)) / 100;
            }
        }
        ns.type = GAME_NEWS_PIRATES;
        switch (g->evn.have_pirates) {
            case 1:
                ns.subtype = 0;
                break;
            case 2:
                ns.subtype = (!rnd_0_nm1(10, &g->seed)) ? 1 : -1;
                break;
            case 3:
                ns.subtype = 2;
                break;
            default:
                ns.subtype = -1;
                break;
        }
        if (g->evn.have_pirates == 1) {
            g->evn.have_pirates = 2;
        }
        if (g->evn.have_pirates == 3) {
            g->evn.have_pirates = 0;
        }
        if (ns.subtype != -1) {
            ui_news(g, &ns);
            any_news = true;
        }
        game_ai->pirates(g, pli);
    }
    /*10244*/
    if (g->evn.have_derelict) {
        player_id_t player = g->evn.derelict_player;
        empiretechorbit_t *e = &(g->eto[player]);
        const uint8_t (*rl)[TECH_TIER_NUM][3] = g->srd[player].researchlist;
        ns.type = GAME_NEWS_DERELICT;
        ns.race = e->race;
        for (int fi = 0; fi < 2; ++fi) {
            const tech_field_t ftbl[2] = { TECH_FIELD_WEAPON, TECH_FIELD_FORCE_FIELD };
            tech_field_t field;
            int tier;
            field = ftbl[fi];
            tier = e->tech.percent[field] / 5;
            for (int i = 0; (i < (tier + 2)) && (i < TECH_TIER_NUM); ++i) {
                game_tech_get_new(g, player, field, rl[field][i][0], TECHSOURCE_RESEARCH, 0, ns.race, false); /* BUG? ns.race makes no sense but is unused */
            }
        }
        ns.subtype = 0;
        ui_news(g, &ns);
        any_news = true;
        g->evn.have_derelict = false;
    }
    /*10364,10470*/
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        if ((p->owner == PLAYER_NONE) || (p->unrest_reported)) {
            continue;
        }
        if (p->unrest == PLANET_UNREST_REBELLION) {
            empiretechorbit_t *e;
            player_id_t player;
            player = p->owner;
            e = &(g->eto[player]);
            ns.planet_i = pli;
            ns.race = e->race;
            ns.type = GAME_NEWS_REBELLION;
            ns.subtype = 0;
            ns.num1 = p->rebels;
            {
                int n;
                n = 0;
                for (int i = 0; (i < g->galaxy_stars) && (n < 2); ++i) { /* WASBUG MOO1 loops while i < g->players */
                    const planet_t *p2 = &(g->planet[i]);
                    if ((p2->owner == player) && (p2->unrest == PLANET_UNREST_REBELLION)) {
                        ++n;
                    }
                }
                if (n > 1) {
                    ++ns.subtype;
                }
            }
            if (IS_AI(g, player)) {
                ns.subtype += 3;
            }
            ui_news(g, &ns);
            any_news = true;
            p->unrest_reported = true;
        } else if (p->unrest == PLANET_UNREST_RESOLVED) {
            empiretechorbit_t *e;
            player_id_t player;
            player = p->owner;
            e = &(g->eto[player]);
            ns.planet_i = pli;
            ns.race = e->race;
            ns.type = GAME_NEWS_REBELLION;
            ns.subtype = 2;
            if (IS_AI(g, player)) {
                ns.subtype += 3;
            }
            ui_news(g, &ns);
            any_news = true;
            p->unrest = PLANET_UNREST_NORMAL;
            p->unrest_reported = true;
        }
    }
    /*10530*/
    for (monster_id_t i = MONSTER_CRYSTAL; i <= MONSTER_AMOEBA; ++i) {
        monster_t *m = (i == MONSTER_CRYSTAL) ? &(g->evn.crystal) : &(g->evn.amoeba);
        if (m->exists) {
            planet_t *p = &(g->planet[m->dest]);
            player_id_t owner;
            bool flag_colony, flag_last_planet = false, flag_new_dest;
            flag_colony = false;
            owner = p->owner;
            if ((m->nuked > 4) || (m->killer != PLAYER_NONE)) {
                m->exists = 3;
            }
            if (m->killer != PLAYER_NONE) { /* WASBUG MOO1 does not check for none, taking race from eto[-1] */
                ns.race = g->eto[m->killer].race;
            } else if (owner != PLAYER_NONE) {
                ns.race = g->eto[owner].race;
            } else {
                ns.race = RACE_HUMAN;
            }
            flag_new_dest = false;
            if ((m->x == p->x) && (m->y == p->y) && (m->killer == PLAYER_NONE) && (m->counter <= 0)) {
                if ((p->pop > 0) && (owner != PLAYER_NONE)) {
                    flag_colony = true;
                }
                ns.planet_i = m->dest;
                p->pop = 0;
                if (i == MONSTER_AMOEBA) {
                    p->factories /= 10;
                    p->type = PLANET_TYPE_RADIATED;
                    p->max_pop2 = rnd_1_n(3, &g->seed) * 5 + 5;
                    p->max_pop3 = p->max_pop2;
                    game_planet_destroy(g, ns.planet_i, PLAYER_NONE);
                } else {
                    p->waste = p->max_pop3 - 10;
                }
                ++m->nuked;
                game_monster_set_next_dest(g, m);
                m->counter = 4;
                flag_new_dest = true;
                m->exists = 3;
                if (owner != PLAYER_NONE) {
                    flag_last_planet = true;
                    for (int pli = 0; (pli < g->galaxy_stars) && flag_last_planet; ++pli) {
                        planet_t *p2 = &(g->planet[pli]);
                        if ((pli != ns.planet_i) && (p2->owner == owner)) {
                            flag_last_planet = false;
                        }
                    }
                }
            } else {
                --m->counter;
            }
            switch (m->exists) {
                case 1:
                    ns.subtype = 0;
                    break;
                case 2:
                    ns.subtype = -1;
                    break;
                case 3:
                    if (flag_new_dest) {
                        ns.subtype = flag_colony ? 3 : -1;
                        m->exists = 2;
                    } else {
                        ns.subtype = (m->killer == PLAYER_NONE) ? 2 : 1;
                    }
                    break;
                default:
                    ns.subtype = -1;
                    break;
            }
            if (m->exists == 1) {
                m->exists = 2;
            }
            if (m->exists == 3) {
                m->exists = 0;
            }
            ns.type = (i == MONSTER_CRYSTAL) ? GAME_NEWS_CRYSTAL : GAME_NEWS_AMOEBA;
            if ((ns.subtype == 3) && flag_last_planet) {
                ns.subtype = 6;
            }
            if (flag_last_planet) {
                game_event_kill_player(g, owner);
            }
            if (ns.subtype != -1) {
                ui_news(g, &ns);
                any_news = true;
            }
        }
    }
    /*10c37*/
    if (g->evn.have_enviro) {
        uint8_t pli = g->evn.enviro_planet_i;
        planet_t *p = &(g->planet[pli]);
        player_id_t player = p->owner;
        empiretechorbit_t *e = 0;
        int terraf = 0;
        if (player != PLAYER_NONE) {
            e = &(g->eto[player]);
            terraf = e->have_terraform_n;
        }
        ns.planet_i = pli;
        ns.race = (player != PLAYER_NONE) ? e->race : RACE_HUMAN; /* WASBUG MOO1 does not check for none, taking race from eto[-1] */
        game_turn_soil_enrich(p, terraf, 0);
        g->evn.have_enviro = false;
        ns.type = GAME_NEWS_ENVIRO;
        ns.subtype = IS_HUMAN(g, player) ? 0 : 4;
        ui_news(g, &ns);
        any_news = true;
    }
    /*10dfb*/
    if (g->evn.have_rich) {
        uint8_t pli = g->evn.rich_planet_i;
        planet_t *p = &(g->planet[pli]);
        player_id_t player = p->owner;
        empiretechorbit_t *e = 0;
        if (player != PLAYER_NONE) {
            e = &(g->eto[player]);
        }
        ns.planet_i = pli;
        ns.race = (player != PLAYER_NONE) ? e->race : RACE_HUMAN; /* WASBUG MOO1 does not check for none, taking race from eto[-1] */
        p->special = PLANET_SPECIAL_RICH;
        g->evn.have_rich = false;
        ns.type = GAME_NEWS_RICH;
        ns.subtype = IS_HUMAN(g, player) ? 0 : 4;
        ui_news(g, &ns);
        any_news = true;
    }
    /*10e80*/
    if (g->evn.have_support) {
        player_id_t player = g->evn.support_player;
        empiretechorbit_t *e = &(g->eto[player]);
        int v;
        ns.race = e->race;
        v = (g->year * 10 / 100) * 100;
        SETMAX(v, 500);
        e->reserve_bc += v;
        ns.num1 = v;
        g->evn.have_support = false;
        ns.type = GAME_NEWS_SUPPORT;
        ns.subtype = IS_HUMAN(g, player) ? 0 : 4;
        ui_news(g, &ns);
        any_news = true;
    }
    /*10f1e*/
    if (g->evn.have_poor) {
        uint8_t pli = g->evn.poor_planet_i;
        planet_t *p = &(g->planet[pli]);
        player_id_t player = p->owner;
        empiretechorbit_t *e = 0;
        if (player != PLAYER_NONE) {
            e = &(g->eto[player]);
        }
        ns.planet_i = pli;
        ns.race = (player != PLAYER_NONE) ? e->race : RACE_HUMAN; /* WASBUG MOO1 does not check for none, taking race from eto[-1] */
        p->special = PLANET_SPECIAL_POOR;
        g->evn.have_poor = false;
        ns.type = GAME_NEWS_POOR;
        ns.subtype = IS_HUMAN(g, player) ? 0 : 4;
        ui_news(g, &ns);
        any_news = true;
    }
    /*10ff3*/
    ns.type = GAME_NEWS_GENOCIDE;
    ns.subtype = 0;
    {
        int num_humans = 0;
        uint8_t num_planets[PLAYER_NUM];
        memset(num_planets, 0, sizeof(num_planets));
        for (int pli = 0; pli < g->galaxy_stars; ++pli) {
            const planet_t *p = &(g->planet[pli]);
            if (p->owner != PLAYER_NONE) {
                ++num_planets[p->owner];
            }
        }
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (IS_HUMAN(g, i) && IS_ALIVE(g, i)) {
                ++num_humans;
            }
        }
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (1
               && (num_planets[i] == 0) && IS_ALIVE(g, i)
               && (IS_AI(g, i) || (num_humans > 1))
            ) {
                player_id_t killer;
                killer = PLAYER_NONE;
                for (player_id_t j = PLAYER_0; (j < g->players) && (killer == PLAYER_NONE); ++j) {
                    empiretechorbit_t *e = &(g->eto[j]);
                    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
                        const planet_t *p = &(g->planet[pli]);
                        if (p->claim == i) {
                            bool any_ships;
                            any_ships = false;
                            for (int k = 0; k < e->shipdesigns_num; ++k) {
                                if (e->orbit[pli].ships[k] > 0) {
                                    any_ships = true;
                                    break;
                                }
                            }
                            if (any_ships || (p->owner == j)) {
                                killer = j;
                            }
                        }
                    }
                }
                if (killer != PLAYER_NONE) {
                    ns.race = g->eto[i].race;
                    ns.num2 = killer;
                    ui_news(g, &ns);
                    any_news = true;
                    for (player_id_t j = PLAYER_0; j < g->players; ++j) {
                        if ((j != i) && (j != killer)) {
                            game_diplo_act(g, -36, killer, j, 0, 0, 0);
                        }
                    }
                }
                game_event_kill_player(g, i);
                if (IS_HUMAN(g, i)) {
                    --num_humans;
                }
            }
        }
    }
    /*1124f*/
    for (player_id_t player = PLAYER_0; player < g->players; ++player) {
        empiretechorbit_t *e;
        int num_humans;
        if (BOOLVEC_IS0(g->evn.coup, player) || (!IS_ALIVE(g, player))) {
            continue;
        }
        e = &(g->eto[player]);
        num_humans = 0;
        ns.type = GAME_NEWS_COUP;
        ns.race = e->race;
        for (int i = PLAYER_0; i < g->players; ++i) {
            if (IS_HUMAN(g, i) && IS_ALIVE(g, i)) {
                ++num_humans;
            }
        }
        if (IS_HUMAN(g, player) && (num_humans == 1)) {
            ns.subtype = 2;
            ns.num1 = player;
            ui_news(g, &ns);
            any_news = true;
            ge->type = GAME_END_LOST_FUNERAL;
            for (player_id_t i = PLAYER_0; i < g->players; ++i) {
                if ((i != player) && IS_ALIVE(g, i)) {
                    ge->race = g->eto[i].banner;
                    break;
                }
            }
            ge->banner_dead = e->banner;
            return true;
        } else {
            /*11304*/
            bool any_in_range;
            if (IS_AI(g, player)) {
                game_new_generate_other_emperor_name(g, player);
            }
            ns.subtype = rnd_0_nm1(2, &g->seed);
            ns.num1 = e->trait1;
            ns.num2 = e->trait2;
            any_in_range = IS_HUMAN(g, player);
            for (player_id_t i = PLAYER_0; (i < g->players) && (!any_in_range); ++i) {
                empiretechorbit_t *e2 = &(g->eto[i]);
                if (IS_HUMAN(g, i) && BOOLVEC_IS1(e2->contact, player) && IS_ALIVE(g, i)) {
                    any_in_range = true;
                }
            }
            if (any_in_range) {
                ui_news(g, &ns);
                any_news = true;
                /* WASBUG If multiple coups happen in one turn, MOO1 reports only one. */
            }
            /* TODO What about multiplayer? Set overthrown player to AI? */
        }
    }
    /*11383*/
    {
        player_id_t top_player = PLAYER_0;
        uint8_t half = g->galaxy_stars / 2, max_pl = 0;
        uint8_t num_planets[PLAYER_NUM];
        memset(num_planets, 0, sizeof(num_planets));
        for (int pli = 0; pli < g->galaxy_stars; ++pli) {
            const planet_t *p = &(g->planet[pli]);
            if (p->owner != PLAYER_NONE) {
                ++num_planets[p->owner];
            }
        }
        for (int i = PLAYER_0; i < g->players; ++i) {
            if (num_planets[i] > max_pl) {
                max_pl = num_planets[i];
                top_player = i;
            }
        }
        if (0
          || ((((g->evn.report_stars + 1) * half) / 4) <= max_pl)
          || ((g->evn.report_stars == 3) && ((half - 2) <= max_pl) && (max_pl < half) && (g->end == GAME_END_NONE))
        ) {
            ns.type = GAME_NEWS_STARS;
            ns.subtype = g->evn.report_stars++;
            if (ns.subtype < 4) {
                ns.num1 = max_pl;
                ns.race = g->eto[top_player].race;
                ui_news(g, &ns);
                any_news = true;
            }
        }
    }
    /*114b2*/
    if (g->guardian_killer != PLAYER_NONE) {
        player_id_t player = g->guardian_killer;
        ns.type = GAME_NEWS_GUARDIAN;
        ns.subtype = IS_HUMAN(g, player) ? 0 : 1;
        ns.race = g->eto[player].race;
        ui_news(g, &ns);
        any_news = true;
        game_tech_get_orion_loot(g, player);
        g->guardian_killer = PLAYER_NONE;
    }
    /* MOO1 has the following way before the guardian news which gives a strange news order */
    if (g->evn.have_orion_conquer && BOOLVEC_IS0(g->evn.done, 17)) {
        player_id_t player = g->evn.have_orion_conquer - 1;
        empiretechorbit_t *e = &(g->eto[player]);
        BOOLVEC_SET1(g->evn.done, 17);
        ns.race = e->race;
        ns.num1 = player;
        ns.type = GAME_NEWS_ORION;
        ns.subtype = 0; /* WASBUG MOO1 does not set this here */
        ui_news(g, &ns);
        any_news = true;
    }
    /*114fb*/
    if (1
      && (!game_opt_skip_random_news)
      && (!any_news)
      && (!rnd_0_nm1(40, &g->seed))
      && (g->year > 25)
      && (g->end == GAME_END_NONE)
    ) {
        ns.type = GAME_NEWS_STATS;
        ns.subtype = rnd_0_nm1(4, &g->seed);
        ui_news(g, &ns);
    }
    /*11538*/
    ui_news_end();
    return false;
}
