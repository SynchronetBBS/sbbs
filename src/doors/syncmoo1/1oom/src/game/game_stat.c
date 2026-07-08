#include "config.h"

#include "game_stat.h"
#include "comp.h"
#include "game.h"
#include "game_misc.h"
#include "game_num.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

int game_stat_fleet(const struct game_s *g, player_id_t pi)
{
    const empiretechorbit_t *e = &(g->eto[pi]);
    const shipresearch_t *srd = &(g->srd[pi]);
    int v2, v4, va, ve = 0, sum;
    sum = 0;
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        sum += srd->shipcount[i] * game_num_tbl_hull_w[srd->design[i].hull];
    }
    SETMIN(sum, 0x319750);
    v4 = 0;
    va = 125;
    while (sum > va) {
        sum -= va;
        ++v4;
        ve = va;
        va <<= 1;
    }
    v2 = (sum * 10) / (va - ve) + v4 * 10;
    SETRANGE(v2, 0, 100);
    return v2;
}

int game_stat_tech(const struct game_s *g, player_id_t pi)
{
    const uint8_t *p = g->eto[pi].tech.percent;
    int sum = 0;
    for (tech_field_t f = TECH_FIELD_COMPUTER; f < TECH_FIELD_NUM; ++f) {
        sum += p[f];
    }
    return sum;
}

int game_stat_prod(const struct game_s *g, player_id_t pi)
{
    uint32_t va, ve = 0, prod = g->eto[pi].total_production_bc;
    int si = 0;
    va = 100;
    while (prod > va) {
        prod -= va;
        ++si;
        ve = va;
        va <<= 1;
    }
    return (prod * 10) / (va - ve) + si * 10;
}

int game_stat_pop(const struct game_s *g, player_id_t pi)
{
    int sum = 0, num = g->galaxy_stars;
    for (int i = 0; i < num; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (p->owner == pi) {
            sum += p->pop;
        }
    }
    return sum / ((num * 3) / 2 + 1);
}

int game_stat_planets(const struct game_s *g, player_id_t pi)
{
    int sum = 0, num = g->galaxy_stars;
    for (int i = 0; i < num; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (p->owner == pi) {
            ++sum;
        }
    }
    return (sum * 100) / (num + 1);
}

void game_stats_all(struct game_s *g, player_id_t api, struct game_stats_s *d)
{
    int sum[PLAYER_NUM];

    game_update_production(g);
    game_update_empire_contact(g);
    game_update_maint_costs(g);

    d->num = 1;
    d->p[0] = api;
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if ((pi != api) && BOOLVEC_IS1(g->eto[api].contact, pi)) {
            d->p[d->num++] = pi;
        }
    }
    for (int i = 0; i < d->num; ++i) {
        sum[i] = 0;
    }
    for (int s = 0; s < 5; ++s) {
        for (int i = 0; i < d->num; ++i) {
            player_id_t pi;
            int v;
            pi = d->p[i];
            switch (s) {
                case 0:
                    v = game_stat_fleet(g, pi);
                    break;
                case 1:
                    v = game_stat_tech(g, pi) / 3;
                    break;
                case 2:
                    v = game_stat_prod(g, pi);
                    break;
                case 3:
                    v = game_stat_pop(g, pi);
                    break;
                case 4:
                    v = game_stat_planets(g, pi);
                    break;
            }
            SETRANGE(v, 0, 100);
            d->v[s][i] = v;
            sum[i] += v;
        }
    }
    {
        int maxstats = 0;
        for (int i = 0; i < d->num; ++i) {
            SETMAX(maxstats, sum[i]);
        }
        for (int i = 0; i < d->num; ++i) {
            d->v[5][i] = maxstats ? ((sum[i] * 100) / maxstats) : 100;
        }
    }
}
