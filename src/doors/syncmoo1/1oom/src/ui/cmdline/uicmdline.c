#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "cfg.h"
#include "game.h"
#include "game_end.h"
#include "game_turn.h"
#include "log.h"
#include "options.h"
#include "types.h"
#include "uidefs.h"

/* -------------------------------------------------------------------------- */

const struct cfg_items_s ui_cfg_items[] = {
    CFG_ITEM_END
};

const struct cmdline_options_s ui_cmdline_options[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

/* -------------------------------------------------------------------------- */

const char *idstr_ui = "cmdline";

struct ui_data_s ui_data = { 0 };

bool ui_use_audio = false;

/* -------------------------------------------------------------------------- */

void ui_early_show_message_box(const char *msg)
{

}

int ui_early_init(void)
{
    return 0;
}

int ui_init(void)
{
    return 0;
}

int ui_late_init(void)
{
    return 0;
}

void ui_shutdown(void)
{
}

char *ui_get_strbuf(void)
{
    return ui_data.strbuf;
}

void ui_sound_play_sfx(int sfxi)
{
}

void ui_landing(struct game_s *g, int pi, uint8_t planet_i)
{
}

void ui_turn_pre(const struct game_s *g)
{
}

void ui_turn_msg(struct game_s *g, int pi, const char *str)
{
    printf("%s | %i | Message: %s\n", g->emperor_names[pi], g->year + YEAR_BASE, str);
}

void ui_copyprotection_check(struct game_s *g) {
    copyprot_status = -99;
}

void ui_copyprotection_lose(struct game_s *g, struct game_end_s *ge) {
}

void ui_newships(struct game_s *g, int pi)
{
    bool first = true;
    printf("%s | %i | New ships:", g->emperor_names[pi], g->year + YEAR_BASE);
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        shipsum_t n;
        n = g->evn.new_ships[pi][i];
        if (n != 0) {
            const shipdesign_t *sd = &(g->srd[pi].design[i]);
            if (!first) {
                fputs(",", stdout);
            } else {
                first = false;
            }
            printf(" %i * %s", n, sd->name);
        }
    }
    fputs("\n", stdout);
}
