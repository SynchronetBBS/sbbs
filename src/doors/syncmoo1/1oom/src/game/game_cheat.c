#include "config.h"

#include "game_cheat.h"
#include "boolvec.h"
#include "game.h"
#include "game_aux.h"
#include "game_misc.h"
#include "game_tech.h"
#include "rnd.h"

/* -------------------------------------------------------------------------- */
/* TODO disable for multiplayer and return false */

bool game_cheat_galaxy(struct game_s *g, player_id_t pi)
{
    g->gaux->flag_cheat_galaxy = !g->gaux->flag_cheat_galaxy;
    game_update_tech_util(g);
    game_update_within_range(g);
    game_update_visibility(g);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        BOOLVEC_SET1(g->planet[i].explored, pi);
    }
    return true;
}

bool game_cheat_elections(struct game_s *g, player_id_t pi)
{
    g->gaux->flag_cheat_elections = !g->gaux->flag_cheat_elections;
    return true;
}

bool game_cheat_events(struct game_s *g, player_id_t pi)
{
    g->gaux->flag_cheat_events = !g->gaux->flag_cheat_events;
    return true;
}

bool game_cheat_spy_hint(struct game_s *g, player_id_t pi)
{
    g->gaux->flag_cheat_spy_hint = !g->gaux->flag_cheat_spy_hint;
    return true;
}

bool game_cheat_stars(struct game_s *g, player_id_t pi)
{
    g->gaux->flag_cheat_stars = !g->gaux->flag_cheat_stars;
    return true;
}

bool game_cheat_tech_hint(struct game_s *g, player_id_t pi)
{
    g->gaux->flag_cheat_tech_hint = !g->gaux->flag_cheat_tech_hint;
    return true;
}

bool game_cheat_moola(struct game_s *g, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    e->reserve_bc += 100;
    game_update_production(g);
    return true;
}

bool game_cheat_traits(struct game_s *g, player_id_t pi)
{
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        g->eto[i].trait2 = rnd_0_nm1(TRAIT2_NUM, &g->seed);
        g->eto[i].trait1 = rnd_0_nm1(TRAIT1_NUM, &g->seed);
    }
    return true;
}

bool game_cheat_news(struct game_s *g, player_id_t pi)
{
    g->gaux->flag_cheat_news = !g->gaux->flag_cheat_news;
    return true;
}
