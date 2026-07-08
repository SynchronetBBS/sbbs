#include "config.h"

#include <stdio.h>

#include "ui.h"

/* -------------------------------------------------------------------------- */

void ui_play_ending_good(int race, const char *name)
{
    putchar('\n');
    fputs(name, stdout);
    fputs(" won! :)\n\n", stdout);
}

void ui_play_ending_tyrant(int race, const char *name)
{
    putchar('\n');
    fputs(name, stdout);
    fputs(" won... :/\n\n", stdout);
}

void ui_play_ending_funeral(int p0, int p2)
{
    fputs("\nFuneral. :(\n\n", stdout);
}

void ui_play_ending_exile(const char *name)
{
    putchar('\n');
    fputs(name, stdout);
    fputs(" was exiled. >:I\n\n", stdout);
}
