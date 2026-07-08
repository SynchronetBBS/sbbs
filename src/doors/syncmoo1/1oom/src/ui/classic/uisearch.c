#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "uisearch.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_cheat.h"
#include "game_planet.h"
#include "hw.h"
#include "lbxfont.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uicursor.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uispecs.h"
#include "uistarmap.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

#define SEARCH_POS_X  6
#define SEARCH_POS_Y  169

static void search_draw_cb(void *vptr)
{
    const int x = SEARCH_POS_X, y = SEARCH_POS_Y;
    ui_draw_filled_rect(x, y, x + 100, y + 8, 0x06, ui_scale);
}

static inline bool can_see_name(const struct game_s *g, player_id_t pi, const planet_t *p)
{
    if (BOOLVEC_IS1(p->explored, pi)) {
        return true;
    }
    if (p->owner != PLAYER_NONE) {
        if (0
          || BOOLVEC_IS1(p->within_srange, pi)
          || (pi == g->evn.planet_orion_i)
          || ((BOOLVEC_IS1(g->eto[pi].contact, p->owner) || (p->within_frange[pi] == 1)))
        ) {
            return true;
        }
    }
    return false;
}

static int search_planet(const struct game_s *g, player_id_t pi, const char *str)
{
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (can_see_name(g, pi, p) && (strcasecmp(p->name, str) == 0)) {
            return i;
        }
    }
    {
        for (int i = 0; i < g->galaxy_stars; ++i) {
            const planet_t *p;
            int pli;
            pli = (g->planet_focus_i[pi] + 1 + i) % g->galaxy_stars;
            p = &(g->planet[pli]);
            if (can_see_name(g, pi, p)) {
                for (int j = 0; (j < PLANET_NAME_LEN) && p->name[j]; ++j) {
                    char cp, cs;
                    cp = p->name[j];
                    cs = str[j];
                    if (cs == '\0') {
                        return pli;
                    }
                    if (isupper(cp)) {
                        cp = tolower(cp);
                    }
                    if (isupper(cs)) {
                        cs = tolower(cs);
                    }
                    if (cp != cs) {
                        break;
                    }
                }
            }
        }
        return -1;
    }
}

/* -------------------------------------------------------------------------- */

int ui_search(struct game_s *g, player_id_t pi)
{
    const int x = SEARCH_POS_X, y = SEARCH_POS_Y;
    int pli = -1;
    char buf[PLANET_NAME_LEN];
    buf[0] = 0;

    ui_draw_copy_buf();
    hw_video_copy_back_to_page2();
    uiobj_finish_frame();
    //ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);

    uiobj_table_clear();
    uiobj_set_callback_and_delay(search_draw_cb, 0, 1);

    {
        const uint8_t ctbl[8] = { 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34 };
        lbxfont_select(0, 0, 0, 0);
        if (uiobj_read_str(x + 2, y + 2, 90, buf, PLANET_NAME_LEN - 1, 0, false, ctbl)) {
            util_trim_whitespace(buf, sizeof(buf));
            if (strcasecmp(buf, "/galaxy") == 0) {
                game_cheat_galaxy(g, pi);
            } else if (strcasecmp(buf, "/elections") == 0) {
                game_cheat_elections(g, pi);
            } else if (strcasecmp(buf, "/events") == 0) {
                game_cheat_events(g, pi);
            } else if (strcasecmp(buf, "/spyhint") == 0) {
                game_cheat_spy_hint(g, pi);
            } else if (strcasecmp(buf, "/stars") == 0) {
                game_cheat_stars(g, pi);
            } else if (strcasecmp(buf, "/techhint") == 0) {
                game_cheat_tech_hint(g, pi);
            } else if (strcasecmp(buf, "/p") == 0) {
                game_cheat_traits(g, pi);
            } else if (strcasecmp(buf, "/news") == 0) {
                game_cheat_news(g, pi);
            } else if (strcasecmp(buf, "/moola") == 0) {
                game_cheat_moola(g, pi);
            } else if (strcasecmp(buf, "/oracle0") == 0) {
                ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
                ui_specs(g, 0, true);
            } else if (strcasecmp(buf, "/oracle1") == 0) {
                ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
                ui_specs(g, 1, true);
            } else if (strcasecmp(buf, "/oracle2") == 0) {
                ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
                ui_specs(g, 2, true);
            } else if (strcasecmp(buf, "/oracle3") == 0) {
                ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
                ui_specs(g, 3, true);
            } else if (strcasecmp(buf, "/oracle4") == 0) {
                ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
                ui_specs(g, 4, true);
            } else if (strcasecmp(buf, "/oracle5") == 0) {
                ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
                ui_specs(g, 5, true);
            } else if (buf[0] != 0) {
                pli = search_planet(g, pi, buf);
            }
        }
    }

    uiobj_unset_callback();
    uiobj_table_clear();
    hw_video_copy_back_from_page2();
    uiobj_finish_frame();
    return pli;
}

bool ui_search_set_pos(struct game_s *g, player_id_t pi)
{
    int found = ui_search(g, pi);
    if (found >= 0) {
        g->planet_focus_i[pi] = found;
        ui_starmap_set_pos_focus(g, pi);
        return true;
    }
    return false;
}
