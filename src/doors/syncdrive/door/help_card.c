/* help_card.c -- see help_card.h. Laid out for an 80x25 grid; every row is
 * padded to the panel width with an explicit background, so the panel is
 * opaque over the picture without a clear. No box-drawing glyphs: those
 * differ between CP437 and UTF-8 terminals. */
#include <stdio.h>
#include <string.h>

#include "help_card.h"
#include "termgfx_termio.h"

#define HELP_W   56
#define HELP_COL ((80 - HELP_W) / 2 + 1)
#define HELP_ROW 3

static void help_row(int row, const char *sgr, const char *text)
{
	char line[256];
	int  pad = HELP_W - 2 - (int)strlen(text);
	int  n;

	if (pad < 0)
		pad = 0;
	n = snprintf(line, sizeof line, "\x1b[%d;%dH%s  %s%*s\x1b[0m",
	             row, HELP_COL, sgr, text, pad, "");
	if (n > 0 && n < (int)sizeof line)
		termgfx_termio_write(line, (size_t)n);
}

void help_card_show(void)
{
	static const char *body  = "\x1b[37;44m";
	static const char *title = "\x1b[30;46m";
	static const char *keyc  = "\x1b[1;37;44m";
	int                r     = HELP_ROW;

	help_row(r++, title, "Test Drive -- keys");
	help_row(r++, body,  "");
	help_row(r++, keyc,  "Up / Down      accelerate / brake");
	help_row(r++, keyc,  "Left / Right   steer");
	help_row(r++, keyc,  "A              shift up one gear");
	help_row(r++, keyc,  "Z              shift down one gear");
	help_row(r++, body,  "               One gear per press: let go between");
	help_row(r++, body,  "               shifts, and wait for the knob to");
	help_row(r++, body,  "               finish moving on the gear gate.");
	help_row(r++, body,  "");
	help_row(r++, keyc,  "Esc            end the drive");
	help_row(r++, keyc,  "Ctrl-P         pause");
	help_row(r++, keyc,  "F2             sound on / off");
	help_row(r++, keyc,  "Ctrl-F         fill the screen or true aspect");
	help_row(r++, keyc,  "Ctrl-S         live stats on the bottom row");
	help_row(r++, keyc,  "Ctrl-Q         quit to the BBS");
	help_row(r++, keyc,  "F1 / Ctrl-K    this card (the game waits)");
	help_row(r++, body,  "");
	help_row(r++, title, "press any key");
	termgfx_termio_flush();
}

void help_card_dismiss(void)
{
	termgfx_termio_invalidate();
}
