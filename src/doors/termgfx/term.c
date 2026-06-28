// term.c -- canonical terminal control strings for sixel game doors. See term.h
// for the rationale (notably DECSDM ?80 sixel-scrolling).

#include "term.h"

//             clear+home  hide cursor  no autowrap  no sixel scroll (DECSDM)
//             ----------  -----------  -----------  -----------------------
const char *const termgfx_term_enter = "\x1b[2J\x1b[H" "\x1b[?25l" "\x1b[?7l" "\x1b[?80l";

//             ESC[14t (text px)  ESC[16t (cell px)  ESC[?2;1S (gfx canvas)  save  corner  DSR  restore
//             -----------------  -----------------  ---------------------   ----  ------  ---  -------
// ESC[16t reports the cell pixel size (reply ESC[6;CH;CWt), letting a door convert
// a centered pixel offset into a text-cursor cell.  ESC[?2;1S (XTSMGRAPHICS) reports
// the graphics-canvas pixels (reply ESC[?2;0;W;HS) -- SyncTERM answers this instead
// of ESC[14t, and it's how a door learns the real canvas to scale/center an image
// into.  Terminals that don't support a query simply don't reply -- harmless.
const char *const termgfx_term_probe = "\x1b[14t" "\x1b[16t" "\x1b[?2;1S" "\x1b" "7" "\x1b[999;999H" "\x1b[6n" "\x1b" "8";

//             sixel scroll  autowrap  cursor
//             ------------  --------  ------
const char *const termgfx_term_leave = "\x1b[?80h" "\x1b[?7h" "\x1b[?25h";
