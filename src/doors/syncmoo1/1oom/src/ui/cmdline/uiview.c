#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uiview.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_misc.h"
#include "game_str.h"
#include "uidefs.h"
#include "uifleet.h"
#include "uiplanet.h"
#include "util.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

#define UI_VIEW_FILTER_PLANET   (1 << 0)
#define UI_VIEW_FILTER_FLEET    (1 << 1)
#define UI_VIEW_FILTER_TRANS    (1 << 2)
#define UI_VIEW_FILTER_MY       (1 << 3)
#define UI_VIEW_FILTER_OPP      (1 << 4)
#define UI_VIEW_FILTER_UNEX     (1 << 5)
#define UI_VIEW_FILTER_EX       (1 << 6)
#define UI_VIEW_FILTER_UNCOLON  (1 << 7)
#define UI_VIEW_FILTER_COLON    (1 << 8)
#define UI_VIEW_FILTER_ALL      ((1 << 9) - 1)

static struct ui_filter_s {
    char c;
    uint32_t m1;
    uint32_t m0;
} ui_view_filter[] = {
    { 'p', UI_VIEW_FILTER_PLANET, 0 },
    { 'f', UI_VIEW_FILTER_FLEET, 0 },
    { 't', UI_VIEW_FILTER_TRANS, 0 },
    { 'y', UI_VIEW_FILTER_MY | UI_VIEW_FILTER_COLON | UI_VIEW_FILTER_EX, UI_VIEW_FILTER_OPP | UI_VIEW_FILTER_UNCOLON | UI_VIEW_FILTER_UNEX },
    { 'o', UI_VIEW_FILTER_OPP | UI_VIEW_FILTER_COLON | UI_VIEW_FILTER_EX, UI_VIEW_FILTER_MY | UI_VIEW_FILTER_UNCOLON | UI_VIEW_FILTER_UNEX },
    { 'u', UI_VIEW_FILTER_UNEX | UI_VIEW_FILTER_PLANET, UI_VIEW_FILTER_EX },
    { 'x', UI_VIEW_FILTER_EX | UI_VIEW_FILTER_PLANET, UI_VIEW_FILTER_UNEX },
    { 'n', UI_VIEW_FILTER_UNCOLON | UI_VIEW_FILTER_PLANET, UI_VIEW_FILTER_COLON },
    { 'c', UI_VIEW_FILTER_COLON | UI_VIEW_FILTER_PLANET, UI_VIEW_FILTER_UNCOLON },
    { 0, 0, 0 }
};

/* -------------------------------------------------------------------------- */

static int ui_cmd_view_add(int range, int dist, int i, int type, int num)
{
    uint32_t v;
    v = (dist << 24) | (type << 22) | i;
    ui_data.view.item[num] = v;
    return ++num;
}

static int ui_cmd_view_item_compare(const void *p0, const void *p1)
{
    uint32_t v0 = *((uint32_t const *)p0);
    uint32_t v1 = *((uint32_t const *)p1);
    return v0 < v1;
}

static void ui_cmd_view_display(const struct game_s *g, player_id_t api, uint32_t item, uint32_t filter)
{
    int dist = item >> 24;
    int type = (item >> 22) & 3;
    int i = item & 0xfff;
    printf("Dist %2i: ", dist);
    switch (type) {
        case 0/*transport*/:
            ui_fleet_print_transport_enroute(g, api, &g->transport[i]);
            break;
        case 1/*fleet*/:
            ui_fleet_print_fleet_enroute(g, api, &g->enroute[i], PLANET_NONE);
            break;
        case 2/*planet*/:
            ui_planet_look(g, api, i, false);
            break;
        case 3/*orbit*/:
            ui_fleet_print_fleets_orbit(g, api, i, (filter & UI_VIEW_FILTER_MY) != 0, (filter & UI_VIEW_FILTER_OPP) != 0);
            break;
        default:
            break;
    }
}

static void ui_cmd_view_do(const struct game_s *g, player_id_t api, uint32_t filter, int rangemax, int distmax)
{
    uint8_t planet_i = g->planet_focus_i[api];
    const planet_t *p = &(g->planet[planet_i]);
    int num = 0, cx = p->x, cy = p->y;
    if (filter & UI_VIEW_FILTER_PLANET) {
        for (int i = 0; i < g->galaxy_stars; ++i) {
            int dist, range;
            p = &(g->planet[i]);
            if (BOOLVEC_IS0(p->explored, api)) {
                if (!(filter & UI_VIEW_FILTER_UNEX)) {
                    continue;
                }
            } else {
                if (!(filter & UI_VIEW_FILTER_EX)) {
                    continue;
                }
	    }
	    {
                player_id_t owner;
                owner = p->owner;
                if (BOOLVEC_IS0(p->within_srange, api) && ((owner == PLAYER_NONE) || BOOLVEC_IS0(g->eto[api].contact, owner))) {
                    owner = g->seen[api][i].owner;
                }
                if (owner == PLAYER_NONE) {
                    if (!(filter & UI_VIEW_FILTER_UNCOLON)) {
                        continue;
                    }
                } else {
                    if (!(filter & UI_VIEW_FILTER_COLON)) {
                        continue;
                    }
                    if (p->owner == api) {
                        if (!(filter & UI_VIEW_FILTER_MY)) {
                            continue;
                        }
                    } else {
                        if (!(filter & UI_VIEW_FILTER_OPP)) {
                            continue;
                        }
                    }
                }
            }
            dist = g->gaux->star_dist[planet_i][i];
            if ((distmax >= 0) && (dist > distmax)) {
                continue;
            }
            range = game_get_min_dist(g, api, i);
            if ((rangemax >= 0) && (range > rangemax)) {
                continue;
            }
            num = ui_cmd_view_add(range, dist, i, 2/*planet*/, num);
        }
    }
    if (filter & UI_VIEW_FILTER_FLEET) {
        for (int i = 0; i < g->enroute_num; ++i) {
            const fleet_enroute_t *r = &(g->enroute[i]);
            int dist;
            if (BOOLVEC_IS0(r->visible, api)) {
                continue;
            }
            if (r->owner == api) {
                if (!(filter & UI_VIEW_FILTER_MY)) {
                    continue;
                }
            } else {
                if (!(filter & UI_VIEW_FILTER_OPP)) {
                    continue;
                }
            }
            dist = util_math_dist_steps(cx, cy, r->x, r->y);
            if ((distmax >= 0) && (dist > distmax)) {
                continue;
            }
            num = ui_cmd_view_add(0, dist, i, 1/*fleet*/, num);
        }
    }
    if ((filter & (UI_VIEW_FILTER_FLEET | UI_VIEW_FILTER_PLANET)) == UI_VIEW_FILTER_FLEET) {
        for (int i = 0; i < g->galaxy_stars; ++i) {
            bool got_fleet;
            int dist, range;
            dist = g->gaux->star_dist[planet_i][i];
            if ((distmax >= 0) && (dist > distmax)) {
                continue;
            }
            got_fleet = false;
            for (int j = 0; j < g->players; ++j) {
                const fleet_orbit_t *r = &(g->eto[j].orbit[i]);
                if (BOOLVEC_IS0(r->visible, api)) {
                    continue;
                }
                if (j == api) {
                    if (filter & UI_VIEW_FILTER_MY) {
                        got_fleet = true;
                        break;
                    }
                } else {
                    if (filter & UI_VIEW_FILTER_OPP) {
                        got_fleet = true;
                        break;
                    }
                }
            }
            if (!got_fleet) {
                continue;
            }
            range = game_get_min_dist(g, api, planet_i);
            if ((rangemax >= 0) && (range > rangemax)) {
                continue;
            }
            num = ui_cmd_view_add(range, dist, i, 3/*orbit*/, num);
        }
    }
    if (filter & UI_VIEW_FILTER_TRANS) {
        for (int i = 0; i < g->transport_num; ++i) {
            const transport_t *r = &(g->transport[i]);
            int dist;
            if (BOOLVEC_IS0(r->visible, api)) {
                continue;
            }
            if (r->owner == api) {
                if (!(filter & UI_VIEW_FILTER_MY)) {
                    continue;
                }
            } else {
                if (!(filter & UI_VIEW_FILTER_OPP)) {
                    continue;
                }
            }
            dist = util_math_dist_steps(cx, cy, r->x, r->y);
            if ((distmax >= 0) && (dist > distmax)) {
                continue;
            }
            num = ui_cmd_view_add(0, dist, i, 0/*transport*/, num);
        }
    }
    qsort(ui_data.view.item, num, sizeof(ui_data.view.item[0]), ui_cmd_view_item_compare);
    for (int i = 0; i < num; ++i) {
        ui_cmd_view_display(g, api, ui_data.view.item[i], filter);
    }
}

/* -------------------------------------------------------------------------- */

int ui_cmd_view(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    uint32_t filter = UI_VIEW_FILTER_ALL;
    int range = -1;
    int dist = -1;
    for (int i = 0; i < num_param; ++i) {
        if (param[i].type == INPUT_TOKEN_NUMBER) {
            range = param[i].data.num;
        } else if (param[i].type == INPUT_TOKEN_RELNUMBER) {
            dist = param[i].data.num;
        } else {
            const char *str;
            char c;
            filter = 0;
            str = param[i].str;
            while ((c = *str++) != '\0') {
                struct ui_filter_s *f;
                f = &ui_view_filter[0];
                c = tolower(c);
                while (f->c && (f->c != c)) {
                    ++f;
                }
                if (!f->c) {
                    printf("Invalid filter '%c'\n", c);
                    return -1;
                }
                filter |= f->m1;
                if (f->m0) {
                    filter &= ~(f->m0);
                }
            }
            if ((filter & (UI_VIEW_FILTER_PLANET | UI_VIEW_FILTER_FLEET | UI_VIEW_FILTER_TRANS)) == 0) {
                filter |= UI_VIEW_FILTER_PLANET | UI_VIEW_FILTER_FLEET | UI_VIEW_FILTER_TRANS;
            }
            if ((filter & (UI_VIEW_FILTER_MY | UI_VIEW_FILTER_OPP)) == 0) {
                filter |= UI_VIEW_FILTER_MY | UI_VIEW_FILTER_OPP;
            }
            if ((filter & (UI_VIEW_FILTER_EX | UI_VIEW_FILTER_UNEX)) == 0) {
                filter |= UI_VIEW_FILTER_EX | UI_VIEW_FILTER_UNEX;
            }
            if ((filter & (UI_VIEW_FILTER_COLON | UI_VIEW_FILTER_UNCOLON)) == 0) {
                filter |= UI_VIEW_FILTER_COLON | UI_VIEW_FILTER_UNCOLON;
            }
        }
    }
    ui_cmd_view_do(g, api, filter, range, dist);
    return 0;
}
