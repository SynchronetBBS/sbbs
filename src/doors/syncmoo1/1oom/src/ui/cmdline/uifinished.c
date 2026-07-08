#include "config.h"

#include <stdio.h>

#include "uifinished.h"
#include "game.h"
#include "game_planet.h"
#include "ui.h"
#include "uidefs.h"

/* -------------------------------------------------------------------------- */

void ui_finished_print_all(struct game_s *g, int api)
{
    if (g->evn.build_finished_num[api] > 0) {
        g->evn.build_finished_num[api] = 0;
        for (uint8_t pli = 0; pli < g->galaxy_stars; ++pli) {
            planet_t *p = &(g->planet[pli]);
            if ((p->owner == api) && (p->finished[0] & (~(1 << FINISHED_SHIP)))) {
                for (planet_finished_t type = 0; type < FINISHED_NUM; ++type) {
                    if ((type != FINISHED_SHIP) && BOOLVEC_IS1(p->finished, type)) {
                        BOOLVEC_SET0(p->finished, type);
                        printf("- %s\n", game_planet_get_finished_text(g, p, type, ui_data.strbuf, UI_STRBUF_SIZE));
                    }
                }
            }
        }
    }
}
