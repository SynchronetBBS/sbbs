#include "config.h"

#include <stdio.h>
#include <string.h>

#include "uiempirestats.h"
#include "comp.h"
#include "game.h"
#include "game_str.h"
#include "game_stat.h"
#include "lib.h"
#include "uidefs.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

int ui_cmd_empirestats(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct game_stats_s st[1];
    int maxlen = 0;
    char fmt[10];
    printf("%s | %s: %i\n", game_str_ra_stats, game_str_year, g->year + YEAR_BASE);
    game_stats_all(g, api, st);
    for (int i = 0; i < st->num; ++i) {
        int len;
        player_id_t pi;
        const empiretechorbit_t *e;
        pi = st->p[i];
        e = (&g->eto[pi]);
        len = strlen(game_str_tbl_race[e->race]);
        SETMAX(maxlen, len);
    }
    lib_sprintf(fmt, sizeof(fmt), "%%-%is", maxlen);
    printf(fmt, "");
    puts("   Fleet Tech  Prod  Pop Planets Total");
    for (int i = 0; i < st->num; ++i) {
        player_id_t pi;
        const empiretechorbit_t *e;
        pi = st->p[i];
        e = (&g->eto[pi]);
        printf(fmt, game_str_tbl_race[e->race]);
        for (int s = 0; s < 6; ++s) {
            printf("   %3u", st->v[s][i]);
        }
        putchar('\n');
    }
    return 0;
}
