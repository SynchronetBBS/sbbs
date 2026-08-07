/* help_term.cpp -- the door's key-help card. See help_term.h for why it
 * exists, why it is on a dedicated key, and why the panel avoids box glyphs.
 *
 * Laid out against an 80x25 grid and centred on whatever the client actually
 * reports, so it lands sensibly on a wider terminal too. Every row is padded to
 * the panel width and painted with an explicit background, so the panel is
 * opaque over the game picture without needing a clear.
 */
#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_stdio_h
#define FORBIDDEN_SYMBOL_EXCEPTION_snprintf
#define FORBIDDEN_SYMBOL_EXCEPTION_strlen

#include "common/scummsys.h"

#if defined(USE_TERMGFX_DRIVER)

#include <stdio.h>
#include <string.h>

#include "help_term.h"
#include "termgfx_termio.h"

static bool g_active;
static int  g_menu_key;      /* GMM hotkey letter, 0 = the sysop disabled it */

#define HELP_W 56            /* panel width in cells; the longest row + margins */

bool help_term_active() { return g_active; }

void help_term_set_menu_key(int letter) { g_menu_key = letter; }

/* One padded, coloured row of the panel at (row, col). `text` is left-aligned
 * inside HELP_W with two leading spaces. */
static void help_row(int row, int col, const char *sgr, const char *text)
{
	char line[256];
	int  n;
	int  pad = HELP_W - 2 - (int)strlen(text);

	if (pad < 0)
		pad = 0;
	n = snprintf(line, sizeof line, "\x1b[%d;%dH%s  %s%*s\x1b[0m",
	             row, col, sgr, text, pad, "");
	if (n > 0 && n < (int)sizeof line)
		termgfx_termio_write(line, (size_t)n);
}

void help_term_show()
{
	/* White on blue for the body, black on cyan for the title -- the same
	 * scheme the sibling door's card uses, so the two read as one family. */
	static const char *BODY  = "\x1b[37;44m";
	static const char *TITLE = "\x1b[30;46m";
	static const char *KEYC  = "\x1b[1;37;44m";
	char               menu[64];
	int                col = 13, row = 5; /* 80x25 default: (80-56)/2 rounded, upper third */
	int                r;

	g_active = true;

	if (g_menu_key)
		snprintf(menu, sizeof menu, "Ctrl-%c   menu: save, load, volume, QUIT",
		         g_menu_key - 'a' + 'A');
	else
		snprintf(menu, sizeof menu, "(menu key disabled by the sysop)");

	r = row;
	help_row(r++, col, TITLE, "SyncSCUMM -- keys");
	help_row(r++, col, BODY,  "");
	help_row(r++, col, KEYC,  menu);
	help_row(r++, col, BODY,  "");
	/* The blunt exit, and the one a caller hunting for the way out most needs:
	 * consumed before the engine sees it, ending the session at once with no
	 * confirmation and no save.
	 *
	 * Ctrl-Q, not a bare 'q': the letter is deliberately left alone on every
	 * key path so it can reach ScummVM's text entry, and only the CTRL form
	 * quits. */
	help_row(r++, col, KEYC,  "Ctrl-Q   quit NOW -- no prompt, no save");
	help_row(r++, col, KEYC,  "Ctrl-C   the same");
	/* Not the whole truth without this: quitting saves nothing (ScummVM's
	 * autosave runs on a timer and before a load, never on the way out), but
	 * the timer is on at its 5-minute default, so what is lost is the last few
	 * minutes rather than the session -- and there is an autosave slot to
	 * resume from. A caller told only "no save" would reasonably assume the
	 * worse of the two. */
	help_row(r++, col, BODY,  "         (autosaves every 5 min; use the menu");
	help_row(r++, col, BODY,  "          to save properly before leaving)");
	help_row(r++, col, BODY,  "");
	help_row(r++, col, KEYC,  "F5       the game's own menu, where it has one");
	help_row(r++, col, KEYC,  "F4       graphics tier (JXL / sixel)");
	help_row(r++, col, KEYC,  "Ctrl-F   letterbox or fill the screen");
	help_row(r++, col, KEYC,  "Ctrl-S   live stats on the bottom row");
	help_row(r++, col, KEYC,  "Ctrl-K   this card");
	help_row(r++, col, BODY,  "");
	help_row(r++, col, BODY,  "Mouse and keyboard go to the game otherwise.");
	help_row(r++, col, BODY,  "");
	help_row(r++, col, TITLE, "press any key");
	termgfx_termio_flush();
}

void help_term_dismiss()
{
	if (!g_active)
		return;
	g_active = false;
	/* Nothing else repaints what the panel covered: an unchanged frame
	 * de-dupes, and the dirty path only repaints what the GAME changed. */
	termgfx_termio_invalidate();
}

#endif /* USE_TERMGFX_DRIVER */
