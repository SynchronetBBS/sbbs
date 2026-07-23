/* SyncRPG -- the door's help/about overlay. See help_term.h for why it is
 * door-side and how it is drawn. GPLv3+, like the EasyRPG tree.
 */
#include "help_term.h"

#include <string>
#include <vector>

#include "bitmap.h"
#include "color.h"
#include "rect.h"
#include "text.h"

#include "syncrpg_version.h"

static bool g_active;

bool help_term_active()  { return g_active; }
void help_term_show()    { g_active = true; }
void help_term_dismiss() { g_active = false; }

/* The page. Reconstructed from Yume Nikki's own README plus EasyRPG's default
 * desktop button table (easyrpg/src/input_buttons_desktop.cpp) and what the
 * door actually forwards (door/input_term.cpp) -- the three have to agree, and
 * only this file states the result in one place for the caller.
 *
 * The Cancel row lists Insert first, not Escape: Escape is the documented key
 * and now works, but Insert is the one that survives every terminal and
 * keyboard-protocol combination the door supports, so it is the safe advice.
 * 'q' is deliberately absent -- see the note at the bottom of this file. */
static const char *const help_lines[] = {
	"MOVE           Arrow keys",
	"CONFIRM        Enter, Space or Z",
	"CANCEL / MENU  Insert, Esc, or X C V B N",
	"",
	"WAKE UP        Number 9  (pinch cheek)",
	"EFFECT ACTION  Number 1 or 3",
	"DROP EFFECT    Number 5",
	"",
	"THIS PAGE      F1",
	"STATS BAR      Ctrl-S",
	"QUIT TO BBS    Ctrl-Q or Ctrl-C",
};

void help_term_draw(Bitmap &surface)
{
	if (!g_active)
		return;

	const int sw = surface.GetWidth();
	const int sh = surface.GetHeight();
	const int nlines = (int)(sizeof(help_lines) / sizeof(help_lines[0]));

	/* Font metrics are fixed for the built-in font: 12px tall, 6px per
	 * half-width glyph (easyrpg/src/font.cpp -- HEIGHT 12, HALF_WIDTH 6). The
	 * box is sized from the line count rather than a magic number so editing
	 * help_lines[] above cannot silently overflow it. */
	const int line_h = 12;
	const int pad    = 8;
	const int title_h = line_h + 4;
	const int box_h  = pad * 2 + title_h + nlines * line_h + line_h;
	const int box_w  = (sw > 40) ? sw - 24 : sw;
	const int box_x  = (sw - box_w) / 2;
	int       box_y  = (sh - box_h) / 2;

	if (box_y < 0)
		box_y = 0;

	/* Opaque panel, not a wash: the game keeps animating underneath, and text
	 * over a moving dream sequence is unreadable. */
	surface.FillRect(Rect(box_x, box_y, box_w, box_h), Color(0, 0, 0, 255));
	surface.FillRect(Rect(box_x, box_y, box_w, 1), Color(185, 199, 173, 255));
	surface.FillRect(Rect(box_x, box_y + box_h - 1, box_w, 1), Color(185, 199, 173, 255));

	const Color title_col(255, 255, 255, 255);
	const Color body_col(185, 199, 173, 255);
	const Color dim_col(130, 140, 125, 255);

	int y = box_y + pad;
	const int tx = box_x + pad;
	const int tw = box_w - pad * 2;

	surface.TextDraw(Rect(tx, y, tw, line_h), title_col, "SyncRPG -- Yume Nikki", Text::AlignLeft);
	y += title_h;

	for (int i = 0; i < nlines; ++i) {
		if (help_lines[i][0] != '\0')
			surface.TextDraw(Rect(tx, y, tw, line_h), body_col, help_lines[i], Text::AlignLeft);
		y += line_h;
	}

	/* The build stamp -- the reason this page carries an "about" role at all.
	 * Same string the startup splash shows, which flashes past too fast to
	 * read. Right-aligned, dim: it is reference, not instruction. */
	surface.TextDraw(Rect(tx, y, tw, line_h), dim_col, syncrpg_version_line(), Text::AlignRight);
	surface.TextDraw(Rect(tx, y, tw, line_h), dim_col, "any key to close", Text::AlignLeft);
}

/* Not listed on the page: a bare 'q'. termgfx's LEGACY byte path reserves it
 * as a quit hotkey (termgfx_termio.c's P_NORMAL case), but the kitty CSI-u and
 * SyncTERM evdev paths deliberately do NOT -- there it reaches the game as an
 * ordinary letter. Advertising a key that quits on some clients and types on
 * others would be worse than saying nothing; Ctrl-Q and Ctrl-C are reserved on
 * all three paths and are what the page recommends. Worth resolving in termgfx
 * one day so the three agree.
 */
