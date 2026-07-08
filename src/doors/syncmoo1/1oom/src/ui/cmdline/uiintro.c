#include "config.h"

#include <stdio.h>

#include "ui.h"

/* -------------------------------------------------------------------------- */

void ui_play_intro(void)
{
    fputs("\n\n(imagine a view of a planet from orbit...\n"
          " names of dead companies...\n"
          " $GAME_TITLE\n"
          " ships shooting at missile bases...\n"
          " missile base shooting back...\n"
          " *BOOM*\n"
          " remaining ships warping out...\n"
          " credits roll by... intro over!)\n", stdout);
}
