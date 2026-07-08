#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "game_nebula.h"
#include "game_new.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_shipdesign.h"
#include "game_str.h"
#include "game_tech.h"
#include "game_techtypes.h"
#include "lbx.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "util.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

#define EMPEROR_NAMES_PER_RACE  6
#define EMPEROR_NAME_LBX_LEN    20

/* index to planets.lbx + 1 */
/* index to starview.lbx - 6 */
static const uint8_t tbl_planet_type_infogfx[PLANET_TYPE_NUM][6] = {
    { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
    { 0x05, 0x05, 0x14, 0x14, 0x22, 0x22 },
    { 0x04, 0x04, 0x06, 0x06, 0x17, 0x17 },
    { 0x0b, 0x0b, 0x0d, 0x0d, 0x18, 0x18 },
    { 0x07, 0x07, 0x08, 0x08, 0x23, 0x23 },
    { 0x20, 0x20, 0x20, 0x1e, 0x1e, 0x1e },
    { 0x02, 0x02, 0x1a, 0x1a, 0x20, 0x20 },
    { 0x09, 0x09, 0x09, 0x19, 0x19, 0x19 },
    { 0x11, 0x11, 0x11, 0x1d, 0x1d, 0x1d },
    { 0x0f, 0x0f, 0x12, 0x12, 0x1f, 0x1f },
    { 0x15, 0x15, 0x15, 0x1c, 0x1c, 0x1c },
    { 0x0e, 0x0e, 0x0e, 0x1b, 0x1b, 0x1b },
    { 0x03, 0x03, 0x01, 0x10, 0x0c, 0x16 },
    { 0x01, 0x01, 0x0a, 0x0a, 0x13, 0x21 }
};

static const int8_t tbl_orion_race_relation[RACE_NUM][RACE_NUM] = {
    { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 0, 0,-2, 0,-3,-1,-1, 0,-1 },
    { 1, 0, 0, 0, 0, 0,-1, 0, 1,-1 },
    { 1,-2, 0, 0, 0,-1,-1, 0,-1,-1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0,-1 },
    { 1,-3, 0,-1, 0, 0,-1, 0, 0,-1 },
    { 1,-1,-1,-1, 0,-1, 0, 0, 0,-1 },
    { 1,-1, 0, 0, 0, 0, 0, 0, 0,-1 },
    { 1, 0, 1,-1, 0, 0, 0, 0, 0,-1 },
    { 1,-1,-1,-1,-1,-1,-1,-1,-1, 1 }
};

/* -------------------------------------------------------------------------- */

static void game_generate_planets(struct game_s *g)
{
    /* assumes the planet data is already 0'd by caller */
    uint16_t tblpx[PLANETS_MAX], tblpy[PLANETS_MAX];

    for (uint16_t h = 0; h < g->galaxy_h; ++h) {
        for (uint16_t w = 0; w < g->galaxy_w; ++w) {
            uint16_t x, y;
            again:
            x = rnd_1_n(0x2b, &g->seed) + w * 0x1c + 9;
            y = rnd_1_n(0x33, &g->seed) + h * 0x20 + 7;
            for (uint16_t i = 0; i < (h * g->galaxy_w + w); ++i) {
                if ((tblpx[i] < x) || (tblpy[i] < y)) {
                    if (util_math_dist_fast(tblpx[i], tblpy[i], x, y) < 20) {
                        goto again;
                    }
                } else if (util_math_dist_fast(tblpx[i], tblpy[i], x, y) < 10) {
                    goto again;
                }
            }
            tblpx[h * g->galaxy_w + w] = x;
            tblpy[h * g->galaxy_w + w] = y;
        }
    }

    for (int i = 0; i < g->galaxy_stars; ++i) {
        bool in_nebula;
        planet_t *p;

        p = &g->planet[i];
        p->x = tblpx[i] + 4;
        p->y = tblpy[i] + 4;

        switch (rnd_0_nm1(0x14, &g->seed)) {
            /*
            case 0x00:
            case 0x01:
            case 0x02:
            */
            default:
                p->star_type = STAR_TYPE_YELLOW;
                break;
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
            case 0x08:
                p->star_type = STAR_TYPE_RED;
                break;
            case 0x09:
            case 0x0a:
            case 0x0b:
            case 0x0c:
            case 0x0d:
                p->star_type = STAR_TYPE_GREEN;
                break;
            case 0x0e:
            case 0x0f:
            case 0x10:
                p->star_type = STAR_TYPE_BLUE;
                break;
            case 0x11:
            case 0x12:
                p->star_type = STAR_TYPE_WHITE;
                break;
            case 0x13:
                p->star_type = STAR_TYPE_NEUTRON;
                break;
        }

        p->look = rnd_0_nm1(2, &g->seed) * 6;
        p->frame = rnd_0_nm1(50, &g->seed);
        /* Needed for the original sequence of random numbers */
        /*p->field_16 =*/ rnd_0_nm1(4, &g->seed);

        in_nebula = false;
        for (int k = 0; k < g->nebula_num; ++k) {
            for (int j = 0; j < 4; ++j) {
                if (1
                  && (p->x >= g->nebula_x0[k][j]) && (p->x <= g->nebula_x1[k][j])
                  && (p->y >= g->nebula_y0[k][j]) && (p->y <= g->nebula_y1[k][j])
                ) {
                    in_nebula = true;
                    /* could break out here */
                }
            }
        }

        {
            planet_type_t t;
            int16_t r;
            t = PLANET_TYPE_NOT_HABITABLE;
            r = rnd_1_n(20, &g->seed) - (in_nebula ? 4 : 0);
            switch (p->star_type) {
                case STAR_TYPE_YELLOW:
                    /*if (r < -1) { t = PLANET_TYPE_NOT_HABITABLE; }*/
                    if (r == -1) { t = PLANET_TYPE_RADIATED; }
                    if (r == 0) { t = PLANET_TYPE_TOXIC; }
                    if (r == 1) { t = PLANET_TYPE_INFERNO; }
                    if (r == 2) { t = PLANET_TYPE_TUNDRA; }
                    if (r == 3) { t = PLANET_TYPE_BARREN; }
                    if (r == 4) { t = PLANET_TYPE_MINIMAL; }
                    if (r == 5) { t = PLANET_TYPE_DESERT; }
                    if (r == 6) { t = PLANET_TYPE_STEPPE; }
                    if ((r > 6) && (r < 9)) { t = PLANET_TYPE_ARID; }
                    if ((r > 8) && (r < 0xb)) { t = PLANET_TYPE_OCEAN; }
                    if ((r > 0xa) && (r < 0xd)) { t = PLANET_TYPE_JUNGLE; }
                    if (r > 0xc) { t = PLANET_TYPE_TERRAN; }
                    break;
                case STAR_TYPE_RED:
                    /*if (r < 2) { t = PLANET_TYPE_NOT_HABITABLE; }*/
                    if (r == 2) { t = PLANET_TYPE_RADIATED; }
                    if (r == 3) { t = PLANET_TYPE_TOXIC; }
                    if (r == 4) { t = PLANET_TYPE_INFERNO; }
                    if (r == 5) { t = PLANET_TYPE_DEAD; }
                    if (r == 6) { t = PLANET_TYPE_TUNDRA; }
                    if (r == 7) { t = PLANET_TYPE_BARREN; }
                    if (r == 8) { t = PLANET_TYPE_MINIMAL; }
                    if ((r > 8) && (r < 0xb)) { t = PLANET_TYPE_DESERT; }
                    if ((r > 0xa) && (r < 0xd)) { t = PLANET_TYPE_STEPPE; }
                    if ((r > 0xc) && (r < 0x10)) { t = PLANET_TYPE_ARID; }
                    if ((r > 0xf) && (r < 0x12)) { t = PLANET_TYPE_OCEAN; }
                    if ((r > 0x11) && (r < 0x14)) { t = PLANET_TYPE_JUNGLE; }
                    if (r == 0x14) { t = PLANET_TYPE_TERRAN; }
                    break;
                case STAR_TYPE_GREEN:
                    /*if (r < 2) { t = PLANET_TYPE_NOT_HABITABLE; }*/
                    if (r == 2) { t = PLANET_TYPE_RADIATED; }
                    if (r == 3) { t = PLANET_TYPE_TOXIC; }
                    if (r == 4) { t = PLANET_TYPE_INFERNO; }
                    if (r == 5) { t = PLANET_TYPE_DEAD; }
                    if (r == 6) { t = PLANET_TYPE_TUNDRA; }
                    if (r == 7) { t = PLANET_TYPE_BARREN; }
                    if (r == 8) { t = PLANET_TYPE_MINIMAL; }
                    if (r == 9) { t = PLANET_TYPE_DESERT; }
                    if ((r > 9) && (r < 0xc)) { t = PLANET_TYPE_STEPPE; }
                    if ((r > 0xb) && (r < 0xe)) { t = PLANET_TYPE_ARID; }
                    if ((r > 0xd) && (r < 0x10)) { t = PLANET_TYPE_OCEAN; }
                    if ((r > 0xf) && (r < 0x12)) { t = PLANET_TYPE_JUNGLE; }
                    if (r > 0x11) { t = PLANET_TYPE_TERRAN; }
                    break;
                case STAR_TYPE_WHITE:
                    /*if (r < 3) { t = PLANET_TYPE_NOT_HABITABLE; }*/
                    if (r == 3) { t = PLANET_TYPE_RADIATED; }
                    if ((r > 3) && (r < 6)) { t = PLANET_TYPE_TOXIC; }
                    if ((r > 5) && (r < 8)) { t = PLANET_TYPE_INFERNO; }
                    if ((r > 7) && (r < 0xa)) { t = PLANET_TYPE_DEAD; }
                    if ((r > 9) && (r < 0xc)) { t = PLANET_TYPE_TUNDRA; }
                    if ((r > 0xb) && (r < 0xe)) { t = PLANET_TYPE_BARREN; }
                    if ((r > 0xd) && (r < 0x10)) { t = PLANET_TYPE_MINIMAL; }
                    if ((r > 0xf) && (r < 0x12)) { t = PLANET_TYPE_DESERT; }
                    if (r == 0x12) { t = PLANET_TYPE_STEPPE; }
                    if (r == 0x13) { t = PLANET_TYPE_ARID; }
                    if (r == 0x14) { t = PLANET_TYPE_OCEAN; }
                    break;
                case STAR_TYPE_BLUE:
                    /*if (r < 4) { t = PLANET_TYPE_NOT_HABITABLE; }*/
                    if ((r > 3) && (r < 6)) { t = PLANET_TYPE_RADIATED; }
                    if ((r > 5) && (r < 8)) { t = PLANET_TYPE_TOXIC; }
                    if ((r > 7) && (r < 0xa)) { t = PLANET_TYPE_INFERNO; }
                    if ((r > 9) && (r < 0xc)) { t = PLANET_TYPE_DEAD; }
                    if ((r > 0xb) && (r < 0xe)) { t = PLANET_TYPE_TUNDRA; }
                    if ((r > 0xd) && (r < 0x10)) { t = PLANET_TYPE_BARREN; }
                    if ((r > 0xf) && (r < 0x12)) { t = PLANET_TYPE_MINIMAL; }
                    if (r == 0x12) { t = PLANET_TYPE_DESERT; }
                    if (r == 0x13) { t = PLANET_TYPE_STEPPE; }
                    if (r == 0x14) { t = PLANET_TYPE_ARID; }
                    break;
                case STAR_TYPE_NEUTRON:
                    /*if (r < 5) { t = PLANET_TYPE_NOT_HABITABLE; }*/
                    if ((r > 4) && (r < 0xa)) { t = PLANET_TYPE_RADIATED; }
                    if ((r > 9) && (r < 0xd)) { t = PLANET_TYPE_TOXIC; }
                    if ((r > 0xc) && (r < 0x10)) { t = PLANET_TYPE_INFERNO; }
                    if ((r > 0xf) && (r < 0x12)) { t = PLANET_TYPE_DEAD; }
                    if (r == 0x12) { t = PLANET_TYPE_TUNDRA; }
                    if (r == 0x13) { t = PLANET_TYPE_BARREN; }
                    if (r == 0x14) { t = PLANET_TYPE_MINIMAL; }
                    break;
                default:
                    break;
            }
            p->type = t;
        }

        {
            uint16_t a;
            switch (p->type) {
                case PLANET_TYPE_NOT_HABITABLE:
                    a = 10;
                    break;
                case PLANET_TYPE_RADIATED:
                case PLANET_TYPE_TOXIC:
                case PLANET_TYPE_INFERNO:
                    a = rnd_1_n(7, &g->seed) * 5 + 5;
                    break;
                case PLANET_TYPE_DEAD:
                case PLANET_TYPE_TUNDRA:
                    a = rnd_1_n(7, &g->seed) * 5 + 0xf;
                    break;
                case PLANET_TYPE_BARREN:
                case PLANET_TYPE_MINIMAL:
                    a = rnd_1_n(5, &g->seed) * 5 + 0x19;
                    break;
                /*
                case PLANET_TYPE_DESERT:
                case PLANET_TYPE_STEPPE:
                case PLANET_TYPE_ARID:
                case PLANET_TYPE_OCEAN:
                */
                default:
                    a = rnd_1_n(4, &g->seed) * 5 + (((int)(p->type)) - (int)PLANET_TYPE_DESERT) * 0xa + 0x1e;
                    break;
                case PLANET_TYPE_JUNGLE:
                    a = rnd_1_n(4, &g->seed) * 5 + 0x46;
                    break;
                case PLANET_TYPE_TERRAN:
                    a = rnd_1_n(4, &g->seed) * 5 + 0x50;
                    break;
            }
            p->max_pop2 = a;
        }

        {
            uint8_t di;
            di = rnd_1_n(10, &g->seed);
            while ((di == 1) || (di == 10)) {
                if (di == 1) {
                    p->max_pop2 -= 20;
                } else {
                    p->max_pop2 += 20;
                }
                di = rnd_1_n(10, &g->seed);
                SETMAX(p->max_pop2, 10);
            }
            SETMIN(p->max_pop2, 120);
        }

        p->max_pop3 = p->max_pop2;
        p->max_pop1 = p->max_pop2;
        p->battlebg = in_nebula ? 0 : rnd_1_n(4, &g->seed);
        p->special = PLANET_SPECIAL_NORMAL;
        {
            star_type_t star_type;
            int16_t di;
            star_type = p->star_type;
            if (p->type >= PLANET_TYPE_STEPPE) {
                di = rnd_1_n(0x14, &g->seed);
                if (star_type == STAR_TYPE_RED) { di -= 4; } else if (star_type == STAR_TYPE_GREEN) { di -= 2; }
                if (di <= 2) {
                    p->special = PLANET_SPECIAL_POOR;
                }
                di = rnd_1_n(0x14, &g->seed);
                if (star_type == STAR_TYPE_RED) { di -= 4; } else if (star_type == STAR_TYPE_GREEN) { di -= 2; }
                if (di <= 5) {
                    if (p->special == PLANET_SPECIAL_POOR) {
                        p->special = PLANET_SPECIAL_ULTRA_POOR;
                    }
                }
            }
            di = rnd_1_n(0x14, &g->seed) - (in_nebula ? 8 : 0);
            if (star_type == STAR_TYPE_BLUE) { di -= 2; } else if (star_type == STAR_TYPE_NEUTRON) { di -= 5; }
            if ((((int)PLANET_TYPE_STEPPE) - ((int)p->type)) > di) {
                p->special = PLANET_SPECIAL_RICH;
            }
            di = rnd_1_n(0x14, &g->seed) - (in_nebula ? 8 : 0);
            if (star_type == STAR_TYPE_BLUE) { di -= 2; } else if (star_type == STAR_TYPE_NEUTRON) { di -= 5; }
            if (di < 6) {
                if (p->special == PLANET_SPECIAL_RICH) {
                    p->special = PLANET_SPECIAL_ULTRA_RICH;
                }
            }

        }
        if (1
          && (p->type >= PLANET_TYPE_MINIMAL)
          && (p->type <= PLANET_TYPE_OCEAN)
          && (p->special == PLANET_SPECIAL_NORMAL)
          && (rnd_1_n(10, &g->seed) == 1)
        ) {
            p->special = PLANET_SPECIAL_ARTIFACTS;
        }
        {
            int16_t di;
            di = rnd_1_n(100, &g->seed);
            if (p->special == PLANET_SPECIAL_ULTRA_RICH) {
                di -= 20;
            } else if (p->special == PLANET_SPECIAL_RICH) {
                di -= 10;
            }
            di -= 15 - (int)(p->type);
            if (di < 15) {
                p->rocks = PLANET_ROCKS_MANY;
            } else if (di < 40) {
                p->rocks = PLANET_ROCKS_SOME;
            } else {
                p->rocks = PLANET_ROCKS_NONE;
            }
        }
        p->growth = PLANET_GROWTH_NORMAL;
        if (p->type < PLANET_TYPE_MINIMAL) {
            p->growth = PLANET_GROWTH_HOSTILE;
        } else if ((p->type > PLANET_TYPE_DESERT) && (rnd_1_n(12, &g->seed) == 1)) {
            uint16_t v;
            p->growth = PLANET_GROWTH_FERTILE;
            v = p->max_pop2 + (p->max_pop2 / 20) * 5;
            p->max_pop2 = v;
            p->max_pop3 = v;
            p->max_pop1 = v;
        }
        p->owner = PLAYER_NONE;
        p->prev_owner = PLAYER_NONE;
        p->claim = PLAYER_NONE;
        if (p->type == PLANET_TYPE_NOT_HABITABLE) {
            p->growth = PLANET_GROWTH_NORMAL;
            p->special = PLANET_SPECIAL_NORMAL;
        }
        p->pop_oper_fact = 2;
        p->infogfx = tbl_planet_type_infogfx[p->type][rnd_0_nm1(6, &g->seed)] - 1;
        p->slider[PLANET_SLIDER_IND] = 100;
        p->reloc = i;
    }

    {
        uint16_t tx = 0, ty = 0;
        for (int i = 0; i < g->galaxy_stars; ++i) {
            planet_t *p;
            p = &g->planet[i];
            if (p->x > tx) {
                tx = p->x;
            }
            if (p->y > ty) {
                ty = p->y;
            }
        }
        g->galaxy_maxx = tx + 27;
        g->galaxy_maxy = ty + 25;
    }

    for (int i = PLAYER_0; i < PLAYER_NUM; ++i) {
        g->evn.voted[i] = PLAYER_NONE;
    }

    {
        uint16_t i;
        planet_t *p;
        i = rnd_0_nm1(g->galaxy_stars, &g->seed);
        g->evn.planet_orion_i = i;
        g->evn.have_guardian = true;
        p = &g->planet[i];
        p->type = PLANET_TYPE_TERRAN;
        p->max_pop3 = 120;
        p->max_pop2 = 120;
        p->max_pop1 = 120;
        p->special = PLANET_SPECIAL_4XTECH;
        p->growth = PLANET_GROWTH_NORMAL;
        p->infogfx = tbl_planet_type_infogfx[p->type][rnd_0_nm1(6, &g->seed)] - 1;
    }
}

static void game_generate_galaxy(struct game_s *g, struct game_new_options_s *opts)
{
    {
        uint16_t a = 0, w = 0, h = 0;

        switch (g->galaxy_size) {
            default:
            case GALAXY_SIZE_SMALL:
                a = rnd_0_nm1(2, &g->seed);
                w = 6;
                h = 4;
                break;
            case GALAXY_SIZE_MEDIUM:
                a = rnd_1_n(3, &g->seed);
                w = 8;
                h = 6;
                break;
            case GALAXY_SIZE_LARGE:
                a = rnd_1_n(2, &g->seed) + 1;
                w = 10;
                h = 7;
                break;
            case GALAXY_SIZE_HUGE:
                a = rnd_1_n(3, &g->seed) + 1;
                w = 12;
                h = 9;
                break;
        }

        g->nebula_num = opts->nebulae ? a : 0;
        g->galaxy_w = w;
        g->galaxy_h = h;
        g->galaxy_maxx = (w - 1) * 0x1c + 0x14;
        g->galaxy_maxy = (h - 1) * 0x20 + 0x10;
        g->galaxy_stars = w * h;
    }

    g->guardian_killer = PLAYER_NONE;

    {
        uint16_t gw, gh;
        uint16_t v8 = 0, i = 0;

        gw = g->galaxy_maxx - 70;
        gh = g->galaxy_maxy - 70;

        while (i < g->nebula_num) {
            uint16_t found, j;

            g->nebula_type[i] = rnd_0_nm1(NEBULA_TYPE_NUM, &g->seed);
            g->nebula_x[i] = rnd_1_n(gw, &g->seed) + 4;
            g->nebula_y[i] = rnd_1_n(gh, &g->seed) + 4;

            found = j = 0;

            while (j < i) {
                if (0
                  || (g->nebula_type[i] == g->nebula_type[j])
                  || ((abs(g->nebula_x[i] - g->nebula_x[j]) < 0x3c) && (abs(g->nebula_y[i] - g->nebula_y[j]) < 0x3c))
                ) {
                    found = 1;
                }
                ++j;
            }
            if (found == 0) {
                ++i;
            }
            if (++v8 > 200) {
                g->nebula_num = i;
                i = 0;
            }
        }
    }

    for (int i = 0; i < g->nebula_num; ++i) {
        uint16_t type = g->nebula_type[i];
        for (int j = 0; j < 4; ++j) {
            g->nebula_x0[i][j] = tbl_nebula_data[type][0][j] + g->nebula_x[i];
            g->nebula_x1[i][j] = tbl_nebula_data[type][2][j] + g->nebula_x[i];
            g->nebula_y0[i][j] = tbl_nebula_data[type][1][j] + g->nebula_y[i];
            g->nebula_y1[i][j] = tbl_nebula_data[type][3][j] + g->nebula_y[i];
        }
    }

    game_generate_planets(g);
}

static void game_generate_planet_names(struct game_s *g)
{
    BOOLVEC_DECLARE(in_use, PLANETS_MAX);
    BOOLVEC_CLEAR(in_use, PLANETS_MAX);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        uint16_t j;
        do {
            j = rnd_0_nm1(PLANETS_MAX, &g->seed);
        } while (BOOLVEC_IS1(in_use, j));
        BOOLVEC_SET1(in_use, j);
        lib_strcpy(g->planet[i].name, game_str_tbl_planet_names[j], PLANET_NAME_LEN);
    }
    lib_strcpy(g->planet[g->evn.planet_orion_i].name, game_str_planet_name_orion, PLANET_NAME_LEN);
}

static void game_generate_race_banner(struct game_s *g)
{
    const uint16_t race_tbl[] = {7, 1, 5, 0, 3, 9, 4, 6, 2, 8};
    BOOLVEC_DECLARE(in_use, (int)BANNER_NUM);
    uint16_t loops;
    loops = 0;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        race_t race;
        bool in_use = false;
        race = g->eto[i].race;
        if (race != RACE_RANDOM) {
            continue;
        }
        race = race_tbl[rnd_0_nm1(RACE_NUM, &g->seed)];
        for (player_id_t j = PLAYER_0; j < i; ++j) {
            if (g->eto[j].race == race) {
                in_use = true;
                --i;
                break;
            }
        }
        if (!in_use) {
            g->eto[i].race = race;
        }
    }
    loops = 0;
    BOOLVEC_CLEAR(in_use, (int)BANNER_NUM);
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        banner_t banner;
        banner = g->eto[i].banner;
        if (banner != BANNER_RANDOM) {
            BOOLVEC_SET1(in_use, banner);
            continue;
        }
        banner = rnd_0_nm1(BANNER_NUM, &g->seed);
        if (BOOLVEC_IS0(in_use, banner)) {
            BOOLVEC_SET1(in_use, banner);
            g->eto[i].banner = banner;
        } else {
            --i; /* try again */
            if (++loops == 100) {
                for (banner = 0; banner < BANNER_NUM; ++banner) {
                    if (BOOLVEC_IS0(in_use, banner)) {
                        BOOLVEC_SET1(in_use, banner);
                        g->eto[i].banner = banner;
                        break;
                    }
                }
                loops = 0;
            }
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if (IS_AI(g, i)) {
            race_t r;
            r = g->eto[i].race;
            g->eto[i].trait2 = game_num_tbl_trait2[r][rnd_0_nm1(TRAIT2_TBL_NUM, &g->seed)];
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        race_t r;
        r = g->eto[i].race;
        g->eto[i].trait1 = game_num_tbl_trait1[r][rnd_0_nm1(TRAIT1_TBL_NUM, &g->seed)];
    }
}

static bool game_generate_home_improved_do_i(struct game_s *g, struct game_new_options_s *opts, uint8_t tblhome[], player_id_t i)
{   //NOTE: Bottleneck for SMALL galaxy and 6 PLAYERS
    static const int mindist = 80;
    uint8_t ok_home_i[PLANETS_MAX];
    uint16_t ok_home_count = 0;
    int safe_mindist = (g->galaxy_size > GALAXY_SIZE_SMALL || g->players < 6) ? mindist : mindist - 10;
    for (uint8_t pi = 0; pi < g->galaxy_stars; ++pi) {
        int x = g->planet[pi].x;
        int y = g->planet[pi].y;
        {
            bool next = false;
            int num_checks = opts->homeworlds.num_dist_checks;
            for (int j = 0; j < i; ++j) {
                planet_t *p;
                p = &g->planet[tblhome[j]];
                if (tblhome[j] == pi) {
                    next = true;
                    break;
                }
                if ((j < num_checks) && (util_math_dist_fast(x, y, p->x, p->y) < safe_mindist)) {
                    next = true;
                    break;
                }
            }
            if (next) continue;
        }
        if ((i < opts->homeworlds.num_ok_planet_checks) && (i < 5 || g->galaxy_size > GALAXY_SIZE_SMALL)) {
            bool next = true;
            for (int j = 0; j < g->galaxy_stars; ++j) {
                planet_t *p;
                p = &g->planet[j];
                if (1
                    && (p->type >= PLANET_TYPE_MINIMAL)
                    && (j != g->evn.planet_orion_i)
                    && (j != pi)
                    ) {
                    if (util_math_dist_fast(x, y, p->x, p->y) <= 29) {
                        next = false;
                        break;
                    }
                }
            }
            if (next) continue;
        }
        if (pi == g->evn.planet_orion_i) {
            continue;
        }
        ok_home_i[ok_home_count] = pi;
        ++ok_home_count;
    }
    while (ok_home_count > 0) {
        int k = rnd_0_nm1(ok_home_count, &g->seed);
        tblhome[i] = ok_home_i[k];
        --ok_home_count;
        ok_home_i[k] = ok_home_i[ok_home_count];
        if ((i + 1) >= g->players) {
            return true;
        }
        if ((i < (PLAYER_NUM - 1)) && game_generate_home_improved_do_i(g, opts, tblhome, i + 1)) {
            return true;
        }
    }
    return false;
}

static bool game_generate_home_improved_do(struct game_s *g, struct game_new_options_s *opts, uint16_t tblhome[])
{
    uint8_t homes[PLAYER_NUM] = { PLANET_NONE , PLANET_NONE , PLANET_NONE , PLANET_NONE , PLANET_NONE , PLANET_NONE };
    game_generate_race_banner(g);
    bool result = game_generate_home_improved_do_i(g, opts, homes, PLAYER_0);
    for (int i = 0; i < PLAYER_NUM; ++i) {
        tblhome[i] = homes[i];
    }
    return result;
}

static bool game_generate_home_do(struct game_s *g, uint16_t tblhome[])
{
    uint16_t loops;
    bool flag_all_ok;
    flag_all_ok = false;
    loops = 0;
    game_generate_race_banner(g);
    while ((!flag_all_ok) && (loops < 200)) {
        flag_all_ok = true;
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            uint16_t pi;
            bool flag_again2;
            flag_again2 = true;
            while (flag_again2) {
                if (g->galaxy_stars > 24) {
                    pi = rnd_1_n(g->galaxy_w - 2, &g->seed) + rnd_1_n(g->galaxy_h - 2, &g->seed) * g->galaxy_w;
                } else if (g->players > 3) {
                    pi = rnd_0_nm1(g->galaxy_stars, &g->seed);
                } else {
                    pi = rnd_1_n(g->galaxy_w - 2, &g->seed) + rnd_1_n(g->galaxy_h - 2, &g->seed) * g->galaxy_w;
                }
                flag_again2 = false;
                for (int j = 0; j < i; ++j) {
                    if (tblhome[j] == pi) {
                        flag_again2 = true;
                    }
                }
                if (pi == g->evn.planet_orion_i) {
                    flag_again2 = true;
                }
            }
            tblhome[i] = pi;
        }
        {
            uint16_t dist, mindist;
            mindist = 10000;
            for (player_id_t i = PLAYER_0; (i < g->players) && (i < 2); ++i) {
                for (int j = 0; j < g->players; ++j) {
                    if (i == j) { continue; }
                    dist = util_math_dist_fast(g->planet[tblhome[i]].x, g->planet[tblhome[i]].y, g->planet[tblhome[j]].x, g->planet[tblhome[j]].y);
                    SETMIN(mindist, dist);
                }
            }
            if (mindist < 80) {
                /* homeworlds too close */
                flag_all_ok = false;    /* could break out here */
            }
            for (player_id_t i = PLAYER_0; (i < g->players) && (i < 2); ++i) {
                mindist = 10000;
                for (int j = 0; j < g->galaxy_stars; ++j) {
                    planet_t *p;
                    p = &g->planet[j];
                    if (1
                      && (p->type >= PLANET_TYPE_MINIMAL)
                      && (j != g->evn.planet_orion_i)
                      && (j != tblhome[i])
                    ) {
                        dist = util_math_dist_fast(g->planet[tblhome[i]].x, g->planet[tblhome[i]].y, p->x, p->y);
                        SETMIN(mindist, dist);
                    }
                }
                if (mindist > 29) {
                    /* ok planet too far away */
                    flag_all_ok = false;    /* could break out here */
                }
            }
            for (player_id_t i = PLAYER_0; (i < g->players) && (i < 2); ++i) {
                mindist = 10000;
                for (int j = 0; j < g->galaxy_stars; ++j) {
                    planet_t *p;
                    p = &g->planet[j];
                    if (1
                      && (j != g->evn.planet_orion_i)
                      && (j != tblhome[i])
                    ) {
                        dist = util_math_dist_fast(g->planet[tblhome[i]].x, g->planet[tblhome[i]].y, p->x, p->y);
                        SETMIN(mindist, dist);
                    }
                }
                if (mindist > 29) {
                    /* regular planet too far away */
                    flag_all_ok = false;    /* could break out here */
                }
            }
        }
        ++loops;
    }
#if 0
    /* FIXME in MOO1 this is actually after the if (!flag_all_ok) test, making it ineffective */
    for (int i = 0; (i < g->players) && (i <= g->difficulty); ++i) {
        uint16_t dist, mindist;
        mindist = 10000;
        for (int j = 0; j < g->galaxy_stars; ++j) {
            planet_t *p;
            p = &g->planet[j];
            if (1
              && (p->type >= PLANET_TYPE_MINIMAL)
              && (j != g->evn.planet_orion_i)
              && (j != tblhome[i])
            ) {
                dist = util_math_dist_fast(g->planet[tblhome[i]].x, g->planet[tblhome[i]].y, p->x, p->y);
                SETMIN(mindist, dist);
            }
        }
        if (mindist > "\x1d\x23\x27\x3c\x50"[g->difficulty]) {
            /* ok planet too far away */
            flag_all_ok = false;    /* could break out here */
        }
    }
#endif
    return flag_all_ok;
}

static void game_generate_home_etc(struct game_s *g, struct game_new_options_s *opts)
{
    uint16_t tblhome[PLAYER_NUM];
    uint16_t homei;
    bool flag_all_ok;
    do {
        game_generate_galaxy(g, opts);
        game_generate_planet_names(g);
        if (opts->improved_galaxy_generator) {
            flag_all_ok = game_generate_home_improved_do(g, opts, tblhome);
        } else {
            flag_all_ok = game_generate_home_do(g, tblhome);
        }
    } while (!flag_all_ok);
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        planet_t *p;
        homei = tblhome[i];
        g->evn.home[i] = homei;
        g->planet_focus_i[i] = homei;
        p = &g->planet[homei];
        p->owner = i;
        p->claim = i;
        p->type = PLANET_TYPE_TERRAN;
        p->star_type = STAR_TYPE_YELLOW;
        if ((opts->homeworlds.max_pop < 50) || (opts->homeworlds.max_pop > 120)) {
            opts->homeworlds.max_pop = 100;
        }
        p->max_pop2 = opts->homeworlds.max_pop;
        p->max_pop3 = opts->homeworlds.max_pop;
        p->max_pop1 = opts->homeworlds.max_pop;
        p->infogfx = tbl_planet_type_infogfx[p->type][rnd_0_nm1(6, &g->seed)] - 1;
        p->growth = PLANET_GROWTH_NORMAL;
        if ((opts->homeworlds.special < PLANET_SPECIAL_ULTRA_POOR) || (opts->homeworlds.special >= PLANET_SPECIAL_NUM)) {
            opts->homeworlds.special = PLANET_SPECIAL_NORMAL;
        }
        p->special = opts->homeworlds.special;
        BOOLVEC_SET1(p->explored, i);
        BOOLVEC_SET1(p->within_srange, i);
        p->within_frange[i] = 1;

        switch (g->difficulty) {
            case DIFFICULTY_SIMPLE:
            case DIFFICULTY_EASY:
            case DIFFICULTY_AVERAGE:
                p->pop = 50;
                p->factories = 30;
                p->total_prod = 55;
                break;
            case DIFFICULTY_HARD:
            case DIFFICULTY_IMPOSSIBLE:
                p->pop = 40;
                p->factories = 30;
                p->total_prod = 50;
                break;
            default:
                break;
        }
        p->pop_prev = p->pop;
        game_new_generate_home_name(g->eto[i].race, p->name, PLANET_NAME_LEN);
    }
    for (int j = 0; j < g->galaxy_stars; ++j) {
        player_id_t owner;
        owner = g->planet[j].owner;
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            g->seen[i][j].owner = (owner == i) ? i : PLAYER_NONE;
        }
        /* seen_pop.._factories cleared here */
    }
    if ((opts->homeworlds.num_scouts < 0) || (opts->homeworlds.num_scouts > 5)) {
        opts->homeworlds.num_scouts = 2;
    }
    startfleet_ships[0] = opts->homeworlds.num_scouts;
    if ((opts->homeworlds.num_fighters < 0) || (opts->homeworlds.num_fighters > 10)) {
        opts->homeworlds.num_fighters = 0;
    }
    startfleet_ships[1] = opts->homeworlds.num_fighters;
    if ((opts->homeworlds.num_colony_ships < 0) || (opts->homeworlds.num_colony_ships > 2)) {
        opts->homeworlds.num_colony_ships = 1;
    }
    startfleet_ships[4] = opts->homeworlds.num_colony_ships;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e;
        e = &g->eto[i];
        e->have_colony_for = PLANET_TYPE_MINIMAL;
        e->factory_adj_cost = 10;
        e->factory_cost = 10;
        e->have_engine = 1;
        e->colonist_oper_factories = 2;
        for (tech_field_t field = 0; field < TECH_FIELD_NUM; ++field) {
            e->tech.percent[field] = 1;
        }
        {
            shipresearch_t *srd;
            shipdesign_t *sd;
            srd = &g->srd[i];
            sd = &srd->design[0];
            e->shipdesigns_num = startship_num;
            for (int j = 0; j < startship_num; ++j, ++sd) {
                *sd = tbl_startship[j];
                lib_strcpy(sd->name, game_str_tbl_stship_names[j], SHIP_NAME_LEN);
                sd->look += e->banner * SHIP_LOOK_PER_BANNER;
            }
            memcpy(&g->current_design[i], &srd->design[0], sizeof(shipdesign_t));
            for (int j = 0; j < NUM_SHIPDESIGNS; ++j) {
                shipcount_t n;
                n = startfleet_ships[j];
                e->orbit[tblhome[i]].ships[j] = n;
                srd->shipcount[j] = n;
                if ((j == 4) && opts->homeworlds.armed_colony_ships && !srd->design[4].wpnt[0]) {
                    srd->design[4].wpnn[0] = 1;
                    srd->design[4].wpnt[0] = 2;
                }
            }
        }
        e->fuel_range = 3;
        if (IS_AI(g, i)) {
            game_ai->new_game_init(g, i, tblhome[i]);
        } else {
            e->tech.slider[TECH_FIELD_COMPUTER] = 20;
            e->tech.slider[TECH_FIELD_CONSTRUCTION] = 10;
            e->tech.slider[TECH_FIELD_FORCE_FIELD] = 15;
            e->tech.slider[TECH_FIELD_PLANETOLOGY] = 15;
            e->tech.slider[TECH_FIELD_PROPULSION] = 20;
            e->tech.slider[TECH_FIELD_WEAPON] = 20;
        }
    }
}

static void game_generate_relation_etc(struct game_s *g)
{
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e;
        e = &g->eto[i];
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            uint16_t v;
            v = tbl_orion_race_relation[e->race][g->eto[j].race] * 12;
            e->relation1[j] = v;
            e->relation2[j] = v;
            for (int k = 0; k < TECH_FIELD_NUM; ++k) {
                e->spyreportfield[j][k] = 1;
            }
        }
    }
}

static bool game_generate_research_check_field(const uint8_t (*rl)[TECH_TIER_NUM][3], tech_field_t field, const uint8_t *rr)
{
    uint8_t rmask = 0, got = 0;
    for (int i = 1; i < 50 + 1; ++i) {
        rmask |= rr[i];
    }
    rmask &= ~(GAME_NG_TECH_NEVER | GAME_NG_TECH_ALWAYS);
    if (!rmask) {
        return true;
    }
    for (int tier = 0; tier < TECH_TIER_NUM; ++tier) {
        for (int l = 0; l < 3; ++l) {
            uint8_t v;
            v = rl[field][tier][l];
            got |= rr[v];
        }
    }
    return (rmask & got) == rmask;
}

static void game_generate_research(struct game_s *g, const uint8_t *rflag)
{
    bool flag_got_essentials = false;
    while (!flag_got_essentials) {
        flag_got_essentials = true;
        for (player_id_t pli = PLAYER_0; pli < g->players; ++pli) {
            uint8_t rmax;
            uint8_t (*rl)[TECH_TIER_NUM][3];
            race_t race;
            race = g->eto[pli].race;
            rl = g->srd[pli].researchlist;
            rmax = (g->eto[pli].race == RACE_PSILON) ? 3 : 2;
            for (tech_field_t field = TECH_FIELD_COMPUTER; field < TECH_FIELD_NUM; ++field) {
                const uint8_t *rr;
                rr = game_num_ng_tech[race][field];
                for (int tier = 0; tier < TECH_TIER_NUM; ++tier) {
                    int num_taken;
                    num_taken = 0;
                    for (int l = 0; l < 3; ++l) {
                        rl[field][tier][l] = 0;
                    }
                    while (num_taken == 0) {
                        for (int ti = tier * 5 + 4; (ti >= (tier * 5)) && (num_taken < 3); --ti) {
                            if (rr[ti + 1] & GAME_NG_TECH_ALWAYS) {
                                rl[field][tier][num_taken++] = ti + 1;
                            }
                        }
                        for (int ti = tier * 5 + 4; ti >= (tier * 5); --ti) {
                            bool flag_skip;
                            flag_skip = false;
                            if (0
                              || (rr[ti + 1] & (GAME_NG_TECH_NEVER | GAME_NG_TECH_ALWAYS))
                              || (rflag[field * 50 + ti] == 0)
                            ) {
                                flag_skip = true;
                            }
                            if ((!flag_skip) && (rnd_1_n(4, &g->seed) <= rmax) && (num_taken < 3)) {
                                rl[field][tier][num_taken++] = ti + 1;
                            }
                        }
                        for (int loops = 0; loops < 3; ++loops) {
                            for (int l = 0; l < 2; ++l) {
                                uint8_t v0, v1;
                                v1 = rl[field][tier][l + 1];
                                if (v1 != 0) {
                                    v0 = rl[field][tier][l];
                                    if (v0 > v1) {
                                        rl[field][tier][l] = v1;
                                        rl[field][tier][l + 1] = v0;
                                    }
                                }
                            }
                        }
                    }
                }
                if (!game_generate_research_check_field(rl, field, rr)) {
                    flag_got_essentials = false;
                }
            }
        }
    }

    for (player_id_t pli = PLAYER_0; pli < g->players; ++pli) {
        empiretechorbit_t *e;
        shipresearch_t *srd;
        e = &g->eto[pli];
        srd = &g->srd[pli];
        e->base_shield = SHIP_SHIELD_CLASS_I;
        e->base_comp = SHIP_COMP_MARK_I;
        e->base_weapon = WEAPON_NUCLEAR_MISSILE_2;
        for (tech_field_t field = TECH_FIELD_COMPUTER; field < TECH_FIELD_NUM; ++field) {
            e->tech.completed[field] = 1;
            srd->researchcompleted[field][0] = 1;
        }
    }

    game_ai->new_game_tech(g);
}

static void game_generate_misc(struct game_s *g)
{
    g->year = 1;
    g->evn.year = 40;
    for (player_id_t pli = PLAYER_0; pli < g->players; ++pli) {
        empiretechorbit_t *e;
        e = &g->eto[pli];
        for (player_id_t pli2 = PLAYER_0; pli2 < g->players; ++pli2) {
            e->attack_bounty[pli2] = PLAYER_NONE;
            e->bounty_collect[pli2] = PLAYER_NONE;
        }
    }
}

static void game_generate_load(struct game_s *g)
{
    /* MOO1 does these when loading a game; moved here to maintain slider state after save/load */
    game_update_production(g);
    game_update_tech_util(g);
    for (player_id_t pli = PLAYER_0; pli < g->players; ++pli) {
        game_update_eco_on_waste(g, pli, false);
        game_update_seen_by_orbit(g, pli);
    }
    game_update_within_range(g);
    game_update_visibility(g);
    game_update_have_reserve_fuel(g);
}

static void game_generate_emperor_names(struct game_s *g, const uint8_t *namedata)
{
    for (player_id_t pli = PLAYER_0; pli < g->players; ++pli) {
        uint16_t base;
        if (g->emperor_names[pli][0] != '\0') {
            continue;
        }
        base = ((uint16_t)g->eto[pli].race) * EMPEROR_NAMES_PER_RACE;
        for (int loops = 0; loops < 200; ++loops) {
            char buf[EMPEROR_NAME_LBX_LEN];
            const char *str;
            bool flag_name_used;
            str = (const char *)&namedata[4 + (base + rnd_0_nm1(EMPEROR_NAMES_PER_RACE, &g->seed)) * EMPEROR_NAME_LBX_LEN];
            lib_strcpy(buf, str, sizeof(buf));
            util_trim_whitespace(buf, sizeof(buf)); /* fix "Zygot  " */
            flag_name_used = false;
            for (player_id_t i = PLAYER_0; i < g->players; ++i) {
                if (strcasecmp(buf, g->emperor_names[i]) == 0) {
                    flag_name_used = true;
                    break;
                }
            }
            if (!flag_name_used) {
                lib_strcpy(g->emperor_names[pli], buf, EMPEROR_NAME_LEN);
                break;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

int game_new(struct game_s *g, struct game_aux_s *gaux, struct game_new_options_s *opt)
{
    uint8_t researchflag[6 * 50];
    memset(g, 0, sizeof(struct game_s));
    g->gaux = gaux;
    if (opt->galaxy_seed == 0) {
        g->galaxy_seed = rnd_get_new_seed();
    } else {
        g->galaxy_seed = opt->galaxy_seed;
    }
    g->seed = g->galaxy_seed;
    g->ai_id = opt->ai_id;
    game_ai = game_ais[g->ai_id];
    g->players = opt->players;
    g->difficulty = opt->difficulty;
    g->galaxy_size = opt->galaxy_size;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        g->eto[i].race = opt->pdata[i].race;
        g->eto[i].banner = opt->pdata[i].banner;
        if (opt->pdata[i].is_ai) {
            BOOLVEC_SET1(g->is_ai, i);
        }
    }
    {
        const uint8_t *rawdata;
        rawdata = gaux->research.d0;
        for (int f = 0; f < 6; ++f) {
            for (int t = 0; t < 50; ++t) {
                researchflag[f * 50 + t] = (rawdata[(f * 50 + t) * 6] != 0xff) ? 1 : 0;
            }
            researchflag[f * 50] = 0;
        }
    }
    researchflag[TECH_FIELD_WEAPON * 50 + (TECH_WEAP_DEATH_RAY - 1)] = 0;
    {
        uint32_t vo, vr = 0, vb = 0, m = 1, va = 0;
        vo = g->difficulty + g->galaxy_size * 10 + g->players * 100;
        for (int i = 0; i < g->players; ++i, m *= 0x10) {
            vr += ((g->eto[i].race + 1) % (RACE_NUM + 1)) * m;
        }
        m = 1;
        for (int i = 0; i < g->players; ++i, m *= 10) {
            vb += ((g->eto[i].banner + 1) % (BANNER_NUM + 1)) * m;
            if (IS_HUMAN(g, i)) {
                va += m;
            }
        }
        log_message("Game: new game -new %u:0x%x:%u:0x%x:%u -nga %u\n", vo, vr, vb, g->galaxy_seed, va, g->ai_id);
    }
    game_generate_home_etc(g, opt);
    game_generate_relation_etc(g);
    game_generate_research(g, researchflag);
    game_generate_misc(g);
    if (!game_num_update_on_load) {
        game_generate_load(g);
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        char *b;
        const char *str;
        str = opt->pdata[i].playername;
        b = g->emperor_names[i];
        lib_strcpy(b, str, EMPEROR_NAME_LEN);
        util_trim_whitespace(b, EMPEROR_NAME_LEN);
        str = opt->pdata[i].homename;
        if (*str != '\0') {
            b = g->planet[g->planet_focus_i[i]].name;
            lib_strcpy(b, str, PLANET_NAME_LEN);
            util_trim_whitespace(b, PLANET_NAME_LEN);
        }
    }
    {
        uint8_t *namedata = lbxfile_item_get(LBXFILE_NAMES, 0);
        game_generate_emperor_names(g, namedata);
        lbxfile_item_release(LBXFILE_NAMES, namedata);
    }
    g->active_player = PLAYER_0;
    return 0;
}

int game_new_tutor(struct game_s *g, struct game_aux_s *gaux)
{
    struct game_new_options_s opt = GAME_NEW_OPTS_DEFAULT;
    opt.galaxy_size = GALAXY_SIZE_MEDIUM;
    opt.players = 4;
    opt.difficulty = DIFFICULTY_SIMPLE;
    opt.pdata[PLAYER_0].race = RACE_KLACKON;
    opt.pdata[PLAYER_0].banner = BANNER_WHITE;
    lib_strcpy(opt.pdata[PLAYER_0].playername, "Mr Tutor", EMPEROR_NAME_LEN);
    lib_strcpy(opt.pdata[PLAYER_0].homename, "SOL", PLANET_NAME_LEN);
    opt.galaxy_seed = 0xfda3f;
    return game_new(g, gaux, &opt);
}

void game_new_generate_emperor_name(race_t race, char *buf, size_t bufsize)
{
    if (race == RACE_RANDOM) {
        lib_strcpy(buf, game_str_rndempname, bufsize);
    } else {
        uint32_t seed = rnd_get_new_seed();
        uint8_t *namedata = lbxfile_item_get(LBXFILE_NAMES, 0);
        int base = race * EMPEROR_NAMES_PER_RACE;
        const char *str = (const char *)&namedata[4 + (base + rnd_0_nm1(EMPEROR_NAMES_PER_RACE, &seed)) * EMPEROR_NAME_LBX_LEN];
        /* TODO check if in use for the case of forced same races */
        lib_strcpy(buf, str, bufsize);
        util_trim_whitespace(buf, bufsize); /* fix "Zygot  " */
        lbxfile_item_release(LBXFILE_NAMES, namedata);
    }
}

void game_new_generate_home_name(race_t race, char *buf, size_t bufsize)
{
    /* TODO check if in use for the case of forced same races */
    lib_strcpy(buf, game_str_tbl_home_names[race], bufsize);
}

void game_new_generate_other_emperor_name(struct game_s *g, player_id_t player)
{
    uint8_t *namedata = lbxfile_item_get(LBXFILE_NAMES, 0);
    int base = g->eto[player].race * EMPEROR_NAMES_PER_RACE;
    for (int loops = 0; loops < 500; ++loops) {
        const char *str;
        char buf[EMPEROR_NAME_LBX_LEN + 1];
        bool flag_in_use;
        str = (const char *)&namedata[4 + (base + rnd_0_nm1(EMPEROR_NAMES_PER_RACE, &g->seed)) * EMPEROR_NAME_LBX_LEN];
        lib_strcpy(buf, str, sizeof(buf));
        util_trim_whitespace(buf, sizeof(buf)); /* fix "Zygot  " */
        flag_in_use = true;
        for (int i = 0; i < g->players; ++i) {
            if (strcasecmp(g->emperor_names[i], buf) == 0) {
                flag_in_use = false;
                break;
            }
        }
        if (!flag_in_use) {
            lib_strcpy(g->emperor_names[player], buf, EMPEROR_NAME_LEN);
            break;
        }
    }
    lbxfile_item_release(LBXFILE_NAMES, namedata);
}
