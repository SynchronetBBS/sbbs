#include "config.h"

#include <stdio.h>
#include <string.h>

#include "uiplanet.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_misc.h"
#include "game_planet.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "uidefs.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static void ui_planet_print_orbit_if_visible(const struct game_s *g, player_id_t api, uint8_t planet_i, player_id_t owner)
{
    const empiretechorbit_t *e = &(g->eto[owner]);
    const fleet_orbit_t *o = &(e->orbit[planet_i]);
    if (BOOLVEC_IS1(o->visible, api)) {
        bool any_ships = false;
        for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
            if (o->ships[i] != 0) {
                any_ships = true;
                break;
            }
        }
        if (any_ships) {
            printf("  - %s fleet at orbit:", game_str_tbl_race[e->race]);
            for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
                printf(" %5i", o->ships[i]);
            }
            putchar('\n');
        }
    }
}

static void ui_planet_print_visible_fleets(const struct game_s *g, player_id_t api, uint8_t planet_i)
{
    const empiretechorbit_t *e = &(g->eto[api]);
    const planet_t *p = &(g->planet[planet_i]);
    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &(g->enroute[i]);
        if (BOOLVEC_IS1(r->visible, api) && (r->dest == planet_i) && ((r->owner == api) || e->have_ia_scanner)) {
            printf("  - %s fleet ETA %i turns:", game_str_tbl_race[e->race], game_calc_eta_ship(g, r->speed, p->x, p->y, r->x, r->y));
            for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
                printf(" %5i", r->ships[i]);
            }
            putchar('\n');
        }
    }
}

static void ui_planet_print_visible_transports(const struct game_s *g, player_id_t api, uint8_t planet_i)
{
    const empiretechorbit_t *e = &(g->eto[api]);
    const planet_t *p = &(g->planet[planet_i]);
    for (int i = 0; i < g->transport_num; ++i) {
        const transport_t *r = &(g->transport[i]);
        if (BOOLVEC_IS1(r->visible, api) && (r->dest == planet_i) && ((r->owner == api) || e->have_ia_scanner)) {
            printf("  - %s transport %i ETA %i turns\n", game_str_tbl_race[e->race], r->pop, game_calc_eta_trans(g, r->speed, p->x, p->y, r->x, r->y));
        }
    }
}

planet_slider_i_t ui_planet_slider_from_param(struct input_token_s *param)
{
    char slchars[] = "sdiet";
    char c = param->str[0];
    char *p = strchr(slchars, c);
    if (p) {
        return (p - slchars);
    } else {
        return PLANET_SLIDER_NUM;
    }
}

/* -------------------------------------------------------------------------- */

const char *ui_planet_str(const struct game_s *g, int api, uint8_t planet_i, char *buf, size_t bufsize)
{
    const planet_t *p = &(g->planet[planet_i]);
    if (BOOLVEC_IS1(p->explored, api)) {
        return p->name;
    } else if ((p->owner != PLAYER_NONE) && (0
      || BOOLVEC_IS1(p->within_srange, api)
      || (p->within_frange[api] == 1)
      || (planet_i == g->evn.planet_orion_i)
    )) {
        return p->name;
    } else {
        lib_sprintf(buf, bufsize, "#%c%i", game_str_tbl_sm_stinfo[p->star_type][0]/*HACK*/, planet_i);
        return buf;
    }
}

uint8_t ui_planet_from_param(struct game_s *g, int api, struct input_token_s *param)
{
    int n = PLANET_NONE;
    if (param->type == INPUT_TOKEN_NUMBER) {
        n = param->data.num;
        if ((n < 0) || (n >= g->galaxy_stars)) {
            n = PLANET_NONE;
        }
    } if (param->type == INPUT_TOKEN_RELNUMBER) {
        int dir, num = param->data.num;
        n = g->planet_focus_i[api];
        if (num < 0) {
            dir = -1;
            num = -num;
        } else {
            dir = 1;
        }
        while (num) {
            do {
                n += dir;
                if (n < 0) {
                    n = g->galaxy_stars - 1;
                } else if (n >= g->galaxy_stars) {
                    n = 0;
                }
            } while (g->planet[n].owner != api);
            --num;
        }
    } else if ((param->str[0] == '#') && (param->str[1] != '\0') && (param->str[2] != '\0'))  {
        if (0
          || (!util_parse_signed_number(&param->str[2], &n))
          || (n < 0) || (n >= g->galaxy_stars)
          || (param->str[1] != game_str_tbl_sm_stinfo[g->planet[n].star_type][0]/*HACK*/)
        ) {
            n = PLANET_NONE;
        }
    }
    if (n == PLANET_NONE) {
        for (n = 0; n < g->galaxy_stars; ++n) {
            const planet_t *p = &(g->planet[n]);
            if (BOOLVEC_IS1(p->explored, api) && (strcasecmp(p->name, param->str) == 0)) {
                break;
            }
        }
    }
    if ((n >= 0) && (n < g->galaxy_stars)) {
        return n;
    }
    return PLANET_NONE;
}

void ui_planet_look(const struct game_s *g, int api, uint8_t planet_i, bool show_full)
{
    const planet_t *p = &(g->planet[planet_i]);
    const empiretechorbit_t *e = &(g->eto[api]);
    int dist = game_get_min_dist(g, api, planet_i);
    {
        char buf[16];
        printf("[%i] (%i,%i) %s: ", planet_i, p->x, p->y, ui_planet_str(g, api, planet_i, buf, sizeof(buf)));
        if (game_xy_is_in_nebula(g, p->x, p->y)) {
            printf("(In nebula)\n");
        }
    }
    if (BOOLVEC_IS1(p->explored, api)) {
        if (p->type == PLANET_TYPE_NOT_HABITABLE) {
            printf("%s %s\n", game_str_sm_nohabit, game_str_sm__planets);
        } else {
            player_id_t owner = p->owner;
            const char *str = NULL;
            printf("%s, %s %i %s", game_str_tbl_sm_pltype[p->type], game_str_sm_pop, p->max_pop3, game_str_sm_max);
            if (p->special != PLANET_SPECIAL_NORMAL) {
                printf(", %s", game_str_tbl_sm_pspecial[p->special]);
            }
            if (p->growth != PLANET_GROWTH_NORMAL) {
                printf(", %s", game_str_tbl_sm_pgrowth[p->growth]);
            }
            if (g->evn.have_plague && (g->evn.plague_planet_i == planet_i)) {
                str = game_str_sm_plague;
            } else if (g->evn.have_nova && (g->evn.nova_planet_i == planet_i)) {
                str = game_str_sm_nova;
            } else if (g->evn.have_comet && (g->evn.comet_planet_i == planet_i)) {
                str = game_str_sm_comet;
            } else if (g->evn.have_pirates && (g->evn.pirates_planet_i == planet_i)) {
                str = game_str_sm_pirates;
            } else if (p->unrest == PLANET_UNREST_REBELLION) {
                str = game_str_sm_rebellion;
            } else if (p->unrest == PLANET_UNREST_UNREST) {
                str = game_str_sm_unrest;
            } else if (g->evn.have_accident && (g->evn.accident_planet_i == planet_i)) {
                str = game_str_sm_accident;
            }
            if (str) {
                printf(", %s", str);
            }
            if (BOOLVEC_IS0(p->within_srange, api) && ((owner == PLAYER_NONE) || BOOLVEC_IS0(e->contact, owner))) {
                owner = g->seen[api][planet_i].owner;
            }
            if (owner == PLAYER_NONE) {
                printf(". No colony. %s %i %s\n", game_str_sm_range, dist, (dist == 1) ? game_str_sm_parsec : game_str_sm_parsecs);
            } else if ((owner != api) || (p->unrest == PLANET_UNREST_REBELLION)) {
                int pop, bases;
                if (BOOLVEC_IS0(p->within_srange, api)) {
                    printf("%s ", game_str_sm_lastrep);
                    pop = g->seen[api][planet_i].pop;
                    bases = g->seen[api][planet_i].bases;
                } else {
                    pop = p->pop;
                    bases = p->missile_bases;
                }
                printf(". %s %s. Pop %i, bases %i. %s %i %s\n", game_str_tbl_race[g->eto[owner].race], game_str_sm_colony, pop, bases, game_str_sm_range, dist, (dist == 1) ? game_str_sm_parsec : game_str_sm_parsecs);
            } else {
                printf(", pop %i, bases %i, factories %i, prod %i (%i), waste %i\n", p->pop, p->missile_bases, p->factories, p->prod_after_maint, p->total_prod, p->waste);
                if (show_full) {
                    char buf[10];
                    int v;
                    v = game_planet_get_slider_text(g, p, PLANET_SLIDER_SHIP, buf, sizeof(buf));
                    printf("  - Build ");
                    if (p->buildship == BUILDSHIP_STARGATE) {
                        printf("%s\n", game_str_sm_stargate);
                    } else {
                        printf("%i %s\n", v, g->srd[api].design[p->buildship].name);
                    }
                    printf("  - Slider %cSHIP %3i  %s\n", p->slider_lock[PLANET_SLIDER_SHIP] ? '*' : ' ', p->slider[PLANET_SLIDER_SHIP], buf);
                    game_planet_get_slider_text(g, p, PLANET_SLIDER_DEF, buf, sizeof(buf));
                    printf("  - Slider %c DEF %3i  %s\n", p->slider_lock[PLANET_SLIDER_DEF] ? '*' : ' ', p->slider[PLANET_SLIDER_DEF], buf);
                    game_planet_get_slider_text(g, p, PLANET_SLIDER_IND, buf, sizeof(buf));
                    printf("  - Slider %c IND %3i  %s\n", p->slider_lock[PLANET_SLIDER_IND] ? '*' : ' ', p->slider[PLANET_SLIDER_IND], buf);
                    v = game_planet_get_slider_text_eco(g, p, true, buf, sizeof(buf));
                    printf("  - Slider %c ECO %3i  ", p->slider_lock[PLANET_SLIDER_ECO] ? '*' : ' ', p->slider[PLANET_SLIDER_ECO]);
                    if (v >= 0) {
                        if (v < 100) {
                            printf("+%i.%i ", v / 10, v % 10);
                        } else {
                            printf("+%i ", v / 10);
                        }
                    }
                    printf("%s\n", buf);
                    game_planet_get_slider_text(g, p, PLANET_SLIDER_TECH, buf, sizeof(buf));
                    printf("  - Slider %cTECH %3i  %s RP\n", p->slider_lock[PLANET_SLIDER_TECH] ? '*' : ' ', p->slider[PLANET_SLIDER_TECH], buf);
                    if (p->reloc != planet_i) {
                        printf("  - Reloc to %s\n", g->planet[p->reloc].name);
                    }
                    if ((p->trans_num != 0) && (p->trans_dest != PLANET_NONE)) {
                        const planet_t *pt = &(g->planet[p->trans_dest]);
                        printf("  - Transport %i to %s", p->trans_num, pt->name);
                        if (p->have_stargate && pt->have_stargate && (p->owner == pt->owner)) {
                            printf(" via %s.\n", game_str_sm_stargate);
                        } else {
                            int eta, engine = e->have_engine;
                            eta = game_calc_eta_trans(g, engine, p->x, p->y, pt->x, pt->y);
                            printf(". %s %i %s.\n", game_str_sm_eta, eta, (eta == 1) ? game_str_sm_turn : game_str_sm_turns);
                        }
                    }
                }
            }
        }
    } else {
        printf("%s. %s %i %s.", game_str_sm_unexplored, game_str_sm_range, dist, (dist == 1) ? game_str_sm_parsec : game_str_sm_parsecs);
        if ((p->owner != PLAYER_NONE) && (0
          || BOOLVEC_IS1(p->within_srange, api)
          || (p->within_frange[api] == 1)
          || (planet_i == g->evn.planet_orion_i)
        )) {
            printf(" %s %s", game_str_tbl_race[g->eto[p->owner].race], game_str_sm_colony);
        }
        putchar('\n');
    }
    ui_planet_print_orbit_if_visible(g, api, planet_i, api);
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if (pi != api) {
            ui_planet_print_orbit_if_visible(g, api, planet_i, pi);
        }
    }
    ui_planet_print_visible_fleets(g, api, planet_i);
    ui_planet_print_visible_transports(g, api, planet_i);
}

int ui_cmd_planet_look(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    uint8_t pi;
    if (num_param == 0) {
        pi = g->planet_focus_i[api];
    } else {
        pi = ui_planet_from_param(g, api, param);
        if (pi == PLANET_NONE) {
            if (param->str[0] == '*') {
                if (param->str[1] == '\0') {
                    for (pi = 0; pi < g->galaxy_stars; ++pi) {
                        ui_planet_look(g, api, pi, true);
                    }
                    return 0;
                }
            }
        }
    }
    if (pi != PLANET_NONE) {
        ui_planet_look(g, api, pi, true);
        return 0;
    } else {
        return -1;
    }
}

int ui_cmd_planet_go(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    uint8_t planet_i = ui_planet_from_param(g, api, param);
    if (planet_i != PLANET_NONE) {
        g->planet_focus_i[api] = planet_i;
        return 0;
    } else {
        return -1;
    }
}

int ui_cmd_planet_slider(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    planet_t *p = &(g->planet[g->planet_focus_i[api]]);
    planet_slider_i_t si = ui_planet_slider_from_param(param);
    int v;
    if (si == PLANET_SLIDER_NUM) {
        return -1;
    }
    if (p->owner != api) {
        return -1;
    }
    if (param[1].type == INPUT_TOKEN_NUMBER) {
        v = param[1].data.num;
    } else if (param[1].type == INPUT_TOKEN_RELNUMBER) {
        v = p->slider[si] + param[1].data.num;
    } else {
        return -1;
    }
    SETRANGE(v, 0, 100);
    if (p->slider_lock[si] == 0) {
        p->slider[si] = v;
        game_adjust_slider_group(p->slider, si, p->slider[si], PLANET_SLIDER_NUM, p->slider_lock);
    }
    return 0;
}

int ui_cmd_planet_slider_lock(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    planet_t *p = &(g->planet[g->planet_focus_i[api]]);
    planet_slider_i_t si = ui_planet_slider_from_param(param);
    if (p->owner != api) {
        return -1;
    }
    p->slider_lock[si] = !p->slider_lock[si];
    return 0;
}

int ui_cmd_planet_build(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    planet_t *p = &(g->planet[g->planet_focus_i[api]]);
    const empiretechorbit_t *e = &(g->eto[api]);
    if (p->owner != api) {
        return -1;
    }
    if (num_param == 0) {
        int n;
        n = p->buildship + 1;
        if (n >= e->shipdesigns_num) {
            if (n >= (NUM_SHIPDESIGNS + 1)) {
                n = 0;
            } else if (e->have_stargates && !p->have_stargate) {
                n = BUILDSHIP_STARGATE;
            } else {
                n = 0;
            }
        }
        p->buildship = n;
    } else {
        /* TODO handle param */
        return -1;
    }
    if (p->buildship == BUILDSHIP_STARGATE) {
        printf("Building %s\n", game_str_sm_stargate);
    } else {
        printf("Building %s\n", g->srd[api].design[p->buildship].name);
    }
    return 0;
}

int ui_cmd_planet_reloc(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    uint8_t pfi, pti;
    planet_t *pf;
    pfi = g->planet_focus_i[api];
    pf = &(g->planet[pfi]);
    if (pf->owner != api) {
        return -1;
    }
    if (num_param == 0) {
        pti = pfi;
    } else {
        pti = ui_planet_from_param(g, api, param);
        if ((pti == PLANET_NONE) || !game_reloc_dest_ok(g, pti, api)) {
            return -1;
        }
    }
    pf->reloc = pti;
    return 0;
}

int ui_cmd_planet_trans(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    uint8_t pfi, pti;
    planet_t *pf;
    char buf[0x80];
    int v = 0;
    pfi = g->planet_focus_i[api];
    pf = &(g->planet[pfi]);
    if (pf->owner != api) {
        return -1;
    }
    if (num_param == 0) {
        pti = pfi;
    } else if (num_param == 1) {
        return -1;
    } else {
        const planet_t *pt;
        const empiretechorbit_t *e = &(g->eto[api]);
        int trans_max = pf->pop / 2;
        pti = ui_planet_from_param(g, api, param);
        if (pti == PLANET_NONE) {
            return -1;
        }
        pt = &(g->planet[pti]);
        if (param[1].type != INPUT_TOKEN_NUMBER) {
            return -1;
        }
        v = param[1].data.num;
        if (v > trans_max) {
            printf("Can transport at most %i from %s.\n", trans_max, pf->name);
            return -1;
        }
        if (pt->within_frange[api] != 1) {
            int mindist = game_get_min_dist(g, api, pti);
            printf("%s %i %s %i %s\n", game_str_sm_notrange1, mindist, game_str_sm_notrange2, e->fuel_range, game_str_sm_notrange3);
            return -1;
        } else if (BOOLVEC_IS0(pt->explored, api)) {
            puts(game_str_sm_trfirste);
            return -1;
        } else if (pt->type < e->have_colony_for) {
            int pos;
            pos = lib_sprintf(buf, sizeof(buf), "%s ", game_str_sm_trcontr1);
            lib_sprintf(&buf[pos], sizeof(buf) - pos, "%s ", game_str_tbl_sm_pltype[pt->type]);
            util_str_tolower(&buf[pos], sizeof(buf));
            printf("%s %s\n", buf, game_str_sm_trcontr2);
            return -1;
        } else if (pt->owner == PLAYER_NONE) {
            puts(game_str_sm_trfirstc);
            return -1;
        } else {
            if (pf->have_stargate && pt->have_stargate && (pf->owner == pt->owner)) {
                lib_strcpy(buf, game_str_sm_stargate, sizeof(buf));
            } else {
                int eta, engine = e->have_engine;
                eta = game_calc_eta_trans(g, engine, pf->x, pf->y, pt->x, pt->y);
                lib_sprintf(buf, sizeof(buf), "%s %i %s", game_str_sm_eta, eta, (eta == 1) ? game_str_sm_turn : game_str_sm_turns);
            }
            puts(buf);
        }
    }
    pf->trans_dest = pti;
    pf->trans_num = v;
    return 0;
}

int ui_cmd_planet_reserve(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    planet_t *p = &(g->planet[g->planet_focus_i[api]]);
    empiretechorbit_t *e = &(g->eto[api]);
    int v;
    if (p->owner != api) {
        return -1;
    }
    if (num_param == 0) {
        int prod = p->prod_after_maint - p->reserve;
        SETMAX(prod, 0);
        v = e->reserve_bc;
        SETMIN(v, prod);
    } else if (param[0].type == INPUT_TOKEN_NUMBER) {
        v = param[0].data.num;
        if (v > e->reserve_bc) {
            return -1;
        }
    } else {
        return -1;
    }
    p->reserve += v;
    e->reserve_bc -= v;
    game_update_production(g);
    return 0;
}

int ui_cmd_planet_scrap_bases(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    planet_t *p = &(g->planet[g->planet_focus_i[api]]);
    int v;
    if (p->owner != api) {
        return -1;
    }
    if (param[0].type == INPUT_TOKEN_NUMBER) {
        v = param[0].data.num;
        if (v > p->missile_bases) {
            return -1;
        }
    } else {
        return -1;
    }
    p->missile_bases -= v;
    g->eto[api].reserve_bc += (v * game_get_base_cost(g, api)) / 4;
    return 0;
}
