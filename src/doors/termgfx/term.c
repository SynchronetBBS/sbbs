// term.c -- canonical terminal control strings for sixel game doors. See term.h
// for the rationale (notably DECSDM ?80 sixel-scrolling).

#include "term.h"

//             clear+home  hide cursor  no autowrap  no sixel scroll (DECSDM)
//             ----------  -----------  -----------  -----------------------
const char *const termgfx_term_enter = "\x1b[2J\x1b[H" "\x1b[?25l" "\x1b[?7l" "\x1b[?80l";

//             ESC[14t (text-area px)   save  park far corner   DSR     restore
//             ----------------------   ----  ---------------   -----   -------
const char *const termgfx_term_probe = "\x1b[14t" "\x1b" "7" "\x1b[999;999H" "\x1b[6n" "\x1b" "8";

//             sixel scroll  autowrap  cursor
//             ------------  --------  ------
const char *const termgfx_term_leave = "\x1b[?80h" "\x1b[?7h" "\x1b[?25h";
