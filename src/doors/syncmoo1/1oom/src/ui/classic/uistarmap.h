#ifndef INC_1OOM_UISTARMAP_H
#define INC_1OOM_UISTARMAP_H

#include "game_types.h"
#include "game_planet.h"

struct game_s;

extern void ui_starmap_set_pos_focus(const struct game_s *g, player_id_t pi);
extern void ui_starmap_set_pos(const struct game_s *g, int x, int y);
extern bool ui_starmap_remove_build_finished(struct game_s *g, player_id_t api, planet_t *p);

extern void ui_starmap_do(struct game_s *g, player_id_t pi);
extern void ui_starmap_orbit_own(struct game_s *g, player_id_t pi);
extern void ui_starmap_orbit_en(struct game_s *g, player_id_t pi);
extern void ui_starmap_enroute(struct game_s *g, player_id_t pi);
extern void ui_starmap_reloc(struct game_s *g, player_id_t pi);
extern void ui_starmap_ships(struct game_s *g, player_id_t pi);
extern void ui_starmap_trans(struct game_s *g, player_id_t pi); /* "trans" button pressed */
extern void ui_starmap_transport(struct game_s *g, player_id_t pi); /* transport selected */

#endif
