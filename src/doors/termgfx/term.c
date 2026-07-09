// term.c -- canonical terminal control strings for sixel game doors. See term.h
// for the rationale (notably DECSDM ?80 sixel-scrolling and DECSET 1070
// colour-register sharing).

#include "term.h"
#include <stddef.h>

//             clear+home  hide cursor  no autowrap  sixel-at-cursor  shared colour regs
//             ----------  -----------  -----------  ---------------  ------------------
//                                                   (DECSDM ?80l)    (DECSET 1070 reset)
const char *const termgfx_term_enter = "\x1b[2J\x1b[H" "\x1b[?25l" "\x1b[?7l" "\x1b[?80l" "\x1b[?1070l";

//             ESC[14t (text px)  ESC[16t (cell px)  ESC[?2;1S (gfx canvas)  save  corner  DSR  restore
//             -----------------  -----------------  ---------------------   ----  ------  ---  -------
// ESC[16t reports the cell pixel size (reply ESC[6;CH;CWt), letting a door convert
// a centered pixel offset into a text-cursor cell.  ESC[?2;1S (XTSMGRAPHICS) reports
// the graphics-canvas pixels (reply ESC[?2;0;W;HS) -- SyncTERM answers this instead
// of ESC[14t, and it's how a door learns the real canvas to scale/center an image
// into.  Terminals that don't support a query simply don't reply -- harmless.
const char *const termgfx_term_probe = "\x1b[14t" "\x1b[16t" "\x1b[?2;1S" "\x1b" "7" "\x1b[999;999H" "\x1b[6n" "\x1b" "8";

//             private colour regs  sixel scroll  autowrap  cursor
//             ------------------   ------------  --------  ------
const char *const termgfx_term_leave = "\x1b[?1070h" "\x1b[?80h" "\x1b[?7h" "\x1b[?25h";

// --- status line (DECSSDT) --------------------------------------------------
// A terminal with a status line (SyncTERM's default) reserves its bottom text
// row for it, shrinking the drawing area -- an 80x25 SyncTERM becomes an 80x24
// / 640x384 canvas, so a 640x400 game fractionally downscales and loses
// single-pixel detail. termgfx_term_status_off hides the status line (DECSSDT
// Ps=0 "no status display"; the terminal grows the row back) so the door draws
// the full canvas 1:1. It is PREFIXED with a DECRQSS query of the current
// setting (DCS $q $~ ST) so a door can capture the reply with
// termgfx_term_parse_status() and restore the pre-door state on exit via
// termgfx_term_status_set(). Send it BEFORE the canvas probe so the probe
// reports the reclaimed size. Terminals without a status line / DECSSDT ignore
// both the query and the set -- harmless.
const char *const termgfx_term_status_off = "\x1bP$q$~\x1b\\" "\x1b[0$~";

// Build "CSI Ps $ ~" (DECSSDT) to set the status-line type: 0 none, 1 indicator
// (the usual default), 2 host-writable. Used to restore the pre-door state. out
// gets a NUL-terminated string; returns its length (0 if out is too small).
size_t termgfx_term_status_set(char *out, size_t sz, int type)
{
	if (out == NULL || sz < 6)
		return 0;
	if (type < 0 || type > 2)
		type = 1;
	out[0] = 0x1b; out[1] = '['; out[2] = (char)('0' + type);
	out[3] = '$';  out[4] = '~'; out[5] = '\0';
	return 5;
}

// Scan `acc` (len bytes) for a DECRQSS reply to a DECSSDT query --
// ESC P 1 $ r <0..2> $ ~ ESC \ -- and return the reported status-display type
// (0..2), or -1 if it isn't present or the terminal answered "unsupported"
// (p1=0). A raw-byte scan tolerant of interleaved input, like
// termgfx_caps_parse_jxl(); a door feeds it a rolling window of inbound bytes.
int termgfx_term_parse_status(const uint8_t *acc, int len)
{
	int j;

	for (j = 0; j + 7 < len; j++) {
		if (acc[j] == 0x1b && acc[j + 1] == 'P' && acc[j + 2] == '1'
		    && acc[j + 3] == '$' && acc[j + 4] == 'r'
		    && acc[j + 5] >= '0' && acc[j + 5] <= '2'
		    && acc[j + 6] == '$' && acc[j + 7] == '~')
			return acc[j + 5] - '0';
	}
	return -1;
}
