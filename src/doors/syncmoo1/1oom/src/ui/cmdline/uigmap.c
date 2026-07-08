#include "config.h"

#include "game.h"
#include "game_aux.h"
#include "ui.h"

void *ui_gmap_basic_init(struct game_s *g, bool show_player_switch)
{
    return 0;
}

void ui_gmap_basic_shutdown(void *ctx)
{
}

void ui_gmap_basic_start_player(void *ctx, int pi)
{
}

void ui_gmap_basic_start_frame(void *ctx, int pi)
{
}

void ui_gmap_basic_draw_frame(void *ctx, int pi)
{
}

void ui_gmap_basic_finish_frame(void *ctx, int pi)
{
}
