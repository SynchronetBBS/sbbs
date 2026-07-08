#include "config.h"

#include "game_aux.h"
#include "bits.h"
#include "game.h"
#include "lbx.h"
#include "lib.h"
#include "log.h"
#include "os.h"
#include "types.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

static void init_star_dist(struct game_aux_s *gaux, struct game_s *g)
{
    int stars = g->galaxy_stars;
    for (int i = 0; i < stars; ++i) {
        gaux->star_dist[i][i] = 0;
        for (int j = i + 1; j < stars; ++j) {
            uint8_t dist;
            dist = (uint8_t)util_math_dist_steps(g->planet[i].x, g->planet[i].y, g->planet[j].x, g->planet[j].y);
            gaux->star_dist[i][j] = dist;
            gaux->star_dist[j][i] = dist;
#ifdef FEATURE_MODEBUG
            if (dist == 0) {
                LOG_DEBUG((0, "%s: dist %i  (%i,%i -> %i, %i)\n", __func__, dist, g->planet[i].x, g->planet[i].y, g->planet[j].x, g->planet[j].y));
            }
#endif
        }
    }
}

static void check_lbx_t5(const uint8_t *t, const char *lbxname, uint16_t want_num, uint16_t want_size)
{
    uint16_t num, size;
    num = GET_LE_16(t);
    t += 2;
    size = GET_LE_16(t);
    if (num != want_num) {
        log_fatal_and_die("%s.lbx: expected %i entries, got %i!\n", lbxname, want_num, num);
    }
    if (size != want_size) {
        log_fatal_and_die("%s.lbx: expected size %i, got %i!\n", lbxname, want_size, size);
    }
}

/* -------------------------------------------------------------------------- */

int game_aux_init(struct game_aux_s *gaux, struct game_s *g)
{
    uint8_t *data, *t;

    memset(g, 0, sizeof(struct game_s));
    g->gaux = gaux;

    t = lbxfile_item_get(LBXFILE_RESEARCH, 0);
    gaux->research.d0 = t + 4;
    t = lbxfile_item_get(LBXFILE_RESEARCH, 1);
    gaux->research.names = (char *)t + 4;
    t = lbxfile_item_get(LBXFILE_RESEARCH, 2);
    gaux->research.descr = (char *)t + 4;
    /* TODO check num/size*/

    t = lbxfile_item_get(LBXFILE_DIPLOMAT, 1);
    check_lbx_t5(t, "diplomat", DIPLOMAT_MSG_NUM, DIPLOMAT_MSG_LEN);
    gaux->diplomat.msg = (const char *)(t + 4);

    data = t = lbxfile_item_get(LBXFILE_DIPLOMAT, 0);
    check_lbx_t5(data, "diplomat", DIPLOMAT_D0_NUM, 2);
    t += 4;
    for (int i = 0; i < DIPLOMAT_D0_NUM; ++i, t += 2) {
        gaux->diplomat.d0[i] = GET_LE_16(t); /* all values < 0x10 */
    }
    lbxfile_item_release(LBXFILE_DIPLOMAT, data);

    data = t = lbxfile_item_get(LBXFILE_FIRING, 0);
    check_lbx_t5(data, "firing", NUM_SHIPLOOKS, 0x1c);
    t += 4;
    for (int j = 0; j < NUM_SHIPLOOKS; ++j) {
        for (int i = 0; i < 12; ++i, t += 2) {
            gaux->firing[j].d0[i] = GET_LE_16(t); /* all values < 0x20 */
        }
        gaux->firing[j].target_x = GET_LE_16(t); /* all values < 0x20 */
        t += 2;
        gaux->firing[j].target_y = GET_LE_16(t); /* all values < 0x20 */
        t += 2;
    }
    lbxfile_item_release(LBXFILE_FIRING, data);

    t = lbxfile_item_get(LBXFILE_EVENTMSG, 0);
    check_lbx_t5(t, "eventmsg", EVENTMSG_NUM, EVENTMSG_LEN);
    gaux->eventmsg = (const char *)(t + 4);

    gaux->move_temp = 0;
    gaux->savenamebuflen = FSDEV_PATH_MAX;
    gaux->savenamebuf = lib_malloc(gaux->savenamebuflen);
    gaux->savebuflen = sizeof(struct game_s) + 64;
    gaux->savebuf = lib_malloc(gaux->savebuflen);
    gaux->flag_cheat_galaxy = false;
    gaux->flag_cheat_elections = false;
    gaux->flag_cheat_events = false;
    gaux->flag_cheat_spy_hint = false;
    gaux->flag_cheat_stars = false;
    gaux->flag_cheat_tech_hint = false;
    gaux->flag_cheat_news = false;
    gaux->initialized = true;
    return 0;
}

void game_aux_shutdown(struct game_aux_s *gaux)
{
    if (gaux->initialized) {
        lbxfile_item_release_file(LBXFILE_RESEARCH);
        lbxfile_item_release_file(LBXFILE_EVENTMSG);
        lbxfile_item_release_file(LBXFILE_DIPLOMAT);
        if (gaux->move_temp) {
            lib_free(gaux->move_temp);
            gaux->move_temp = 0;
        }
        lib_free(gaux->savenamebuf);
        gaux->savenamebuf = 0;
        lib_free(gaux->savebuf);
        gaux->savebuf = 0;
    }
}

uint8_t game_aux_get_firing_param_x(const struct game_aux_s *gaux, uint8_t look, uint8_t a2, bool dir)
{
    const uint8_t *f = &(gaux->firing[look].d0[0]);
    if (!dir) {
        if (a2 == 1) {
            return f[2];
        } else if (a2 == 2) {
            return f[4];
        } else /*if (a2 == 3)*/ {
            return f[0];
        }
    } else {
        if (a2 == 1) {
            return f[8];
        } else if (a2 == 2) {
            return f[10];
        } else /*if (a2 == 3)*/ {
            return f[6];
        }
    }
}

uint8_t game_aux_get_firing_param_y(const struct game_aux_s *gaux, uint8_t look, uint8_t a2, bool dir)
{
    const uint8_t *f = &(gaux->firing[look].d0[0]);
    if (!dir) {
        if (a2 == 1) {
            return f[3];
        } else if (a2 == 2) {
            return f[5];
        } else /*if (a2 == 3)*/ {
            return f[1];
        }
    } else {
        if (a2 == 1) {
            return f[9];
        } else if (a2 == 2) {
            return f[11];
        } else /*if (a2 == 3)*/ {
            return f[7];
        }
    }
}

void game_aux_start(struct game_aux_s *gaux, struct game_s *g)
{
    int n = 0;
    g->gaux = gaux;
    init_star_dist(gaux, g);
    for (int i = 0; i < g->players; ++i) {
        if (BOOLVEC_IS0(g->is_ai, i)) {
            ++n;
        }
    }
    gaux->local_players = n;
    if ((n > 1) && !gaux->move_temp) {
        gaux->move_temp = lib_malloc(sizeof(*gaux->move_temp));
    }
}
