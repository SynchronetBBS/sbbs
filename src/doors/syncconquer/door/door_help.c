/*
 * door_help.c -- Ctrl-K / F1 key-bindings help card (see door_help.h). The
 * command scheme is identical between Red Alert and Tiberian Dawn (their
 * OptionsClass key defaults match), so one card serves both doors; only the
 * title differs.
 *
 * Rendered as plain positioned ANSI (no box-drawing glyphs, which split CP437
 * vs UTF-8 across SyncTERM/Windows Terminal): a solid centered panel -- a cyan
 * title bar over white-on-blue body rows -- with the bindings laid out in two
 * fixed-width columns so they line up. door_io_send() stages the bytes into the
 * same output buffer door_io_present() flushes; the present loop freezes the
 * frame under the panel while it's up so it doesn't flicker.
 */
#include <stdio.h>
#include <string.h>

#include "door_io.h"
#include "door_help.h"

#if defined(SYNCCONQUER_TD)
  #define HELP_TITLE "SyncDawn -- Key Commands"
#else
  #define HELP_TITLE "SyncAlert -- Key Commands"
#endif

/* Inner text width of the panel (chars between the 1-space side margins). The
 * column format below sums to this. */
#define HELP_W 46

/* A key/description cell. The two game/display sections are drawn as two
 * side-by-side columns of these, zipped row for row. */
struct help_kv {
	const char *k;
	const char *d;
};

/* Left column: map / view / mouse. Right column: unit orders. */
static const struct help_kv GAME_L[] = {
	{ "Arrows",     "Scroll map"  },
	{ "Home/End",   "Jump edges"  },
	{ "Mouse edge", "Scroll map"  },
	{ "L-click",    "Select"      },
	{ "R-click",    "Deselect"    },
	{ "Alt+click",  "Force-move"  },
	{ "^Click",     "Force-fire"  },
};
static const struct help_kv GAME_R[] = {
	{ "S",     "Stop"        },
	{ "G",     "Guard"       },
	{ "X",     "Scatter"     },
	{ "F",     "Formation"   },
	{ "H",     "Go to base"  },
	{ "N/B",   "Next/prev"   },
	{ "1-0",   "Pick team"   },
	{ "^1-0",  "Assign team" },
	{ "A",     "Alliance"    },
	{ "E",     "Select all"  },
	{ "Esc",   "Options"     },
};

/* Door/session controls -- all Ctrl chords ("^") except F4. */
static const struct help_kv DISP_L[] = {
	{ "^K / F1", "This help"    },
	{ "^U",      "Who's online" },
	{ "^P",      "Page a node"  },
};
static const struct help_kv DISP_R[] = {
	{ "^S", "Debug stats"   },
	{ "^F", "Aspect / Fill" },
	{ "F4", "Graphics tier" },
};

#define NELEM(a) ((int)(sizeof(a) / sizeof((a)[0])))

static int                  g_active;

void door_help_toggle(void)
{
	g_active = !g_active;
}

int door_help_active(void)
{
	return g_active;
}

/* Emit one panel row: position at (row,col), set attr, one leading space, the
 * content padded/clipped to HELP_W, one trailing space, reset. */
static void help_row(int row, int col, const char *attr, const char *text)
{
	char ob[HELP_W + 64];
	int  n = snprintf(ob, sizeof ob, "\x1b[%d;%dH%s %-*.*s \x1b[0m",
	                  row, col, attr, HELP_W, HELP_W, text);
	if (n > 0)
		door_io_send(ob, (size_t)n);
}

/* Format one zipped two-column line (either cell may be absent). Field widths
 * sum to HELP_W: key 12, desc 13, gap 1, key 6, desc 14. Each key field is
 * wider than its longest key so a gap always separates key from description. */
static void cols_fmt(char *buf, size_t cap, const struct help_kv *l, const struct help_kv *r)
{
	snprintf(buf, cap, "%-12s%-13s %-6s%s",
	         l ? l->k : "", l ? l->d : "",
	         r ? r->k : "", r ? r->d : "");
}

void door_help_draw(int cols, int rows)
{
	const char *title_attr = "\x1b[1;30;46m";   /* black on cyan  */
	const char *body_attr  = "\x1b[0;37;44m";   /* white on blue  */
	const char *hdr_attr   = "\x1b[1;33;44m";   /* yellow on blue */
	struct {
		const char *attr;
		char text[HELP_W + 16];
	} line[40];
	int  n = 0, i, gn, dn, boxw, top, left, pad;
	char title[HELP_W + 1];

	if (!g_active)
		return;
	if (cols <= 0)
		cols = 80;
	if (rows <= 0)
		rows = 25;

	/* Title bar: center the title within the inner width. */
	pad = (HELP_W - (int)strlen(HELP_TITLE)) / 2;
	if (pad < 0)
		pad = 0;
	snprintf(title, sizeof title, "%*s%s", pad, "", HELP_TITLE);
	line[n].attr = title_attr;
	snprintf(line[n].text, sizeof line[n].text, "%s", title);
	n++;

	line[n].attr = body_attr; line[n].text[0] = '\0'; n++;
	line[n].attr = hdr_attr;  snprintf(line[n].text, sizeof line[n].text, "  GAME"); n++;

	gn = NELEM(GAME_L) > NELEM(GAME_R) ? NELEM(GAME_L) : NELEM(GAME_R);
	for (i = 0; i < gn; i++) {
		line[n].attr = body_attr;
		cols_fmt(line[n].text, sizeof line[n].text,
		         i < NELEM(GAME_L) ? &GAME_L[i] : NULL,
		         i < NELEM(GAME_R) ? &GAME_R[i] : NULL);
		n++;
	}

	line[n].attr = body_attr; line[n].text[0] = '\0'; n++;
	line[n].attr = hdr_attr;  snprintf(line[n].text, sizeof line[n].text, "  DISPLAY   (^ = Ctrl)"); n++;

	dn = NELEM(DISP_L) > NELEM(DISP_R) ? NELEM(DISP_L) : NELEM(DISP_R);
	for (i = 0; i < dn; i++) {
		line[n].attr = body_attr;
		cols_fmt(line[n].text, sizeof line[n].text,
		         i < NELEM(DISP_L) ? &DISP_L[i] : NULL,
		         i < NELEM(DISP_R) ? &DISP_R[i] : NULL);
		n++;
	}

	line[n].attr = body_attr; snprintf(line[n].text, sizeof line[n].text, "  + / -      Music volume"); n++;
	line[n].attr = body_attr; line[n].text[0] = '\0'; n++;
	line[n].attr = title_attr; snprintf(line[n].text, sizeof line[n].text, "     F1 / ^K / Esc to close"); n++;

	/* Center the assembled panel. */
	boxw = HELP_W + 2;
	left = (cols - boxw) / 2 + 1;
	if (left < 1)
		left = 1;
	top = (rows - n) / 2 + 1;
	if (top < 1)
		top = 1;

	for (i = 0; i < n; i++)
		help_row(top + i, left, line[i].attr, line[i].text);
}
