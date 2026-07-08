#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_news.h"
#include "log.h"
#include "types.h"
#include "uidefs.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

void ui_news_start(void)
{
}

void ui_news(struct game_s *g, struct news_s *ns)
{
    ui_switch_all(g);
    game_news_get_msg(g, ns, ui_data.strbuf, UI_STRBUF_SIZE);
    printf("GNN | %s", ui_data.strbuf);
    if (ns->type == GAME_NEWS_STATS) {
        for (int i = 0; i < ns->statsnum; ++i) {
            if (i > 0) {
                putchar(',');
            }
            printf(" %i. %s", i + 1, ns->stats[i]);
        }
    }
    putchar('\n');
    ui_switch_wait(g);
}

void ui_news_end(void)
{
}
