#include "ansi_terminal.h"

/*
 * This requires the remote to support the following commands.  All of
 * these commands are supported by ANSI.SYS and are expected to be the
 * minimal set for correct behaviour.
 * 
 * Note that SCOSC and SCORC are not standard and conflict with the DEC
 * left/right margin settings (DECLRMM).  If this becomes an issue, they
 * can be emulated via cusros position query.  There's also the VT100
 * DECSC/DECRC (ESC8/ESC8) that may work when DECLRMM works.
 * 
 * Note that "Clear to end of screen" (ESC [ J) is not supported by
 * ANSI.SYS from MS-DOS 3.3 and is a recent addition.
 * 
 * CUU   - ESC[A Cursor Up (With count)
 * CUD   - ESC[B Cursor Down (With count)
 * CUF   - ESC[C Cursor Right (With count)
 * CUB   - ESC[D Cursor Left (With count)
 * CUP   - ESC[H Cursor Position (With Y and X specified and with no parameter specified)
 * ED    - ESC[J Erase in Page (With parameter 2 specified, and with no parameter ie: 0)
 * EL    - ESC[K Erase in Line (No parameter used currently)
 * DSR   - ESC[n Device Status Report (Parameter 6, request active cursor position)
 * SCOSC - ESC[s Save Current Position
 * SCORC - ESC[u Restore Cursor Position
 * 
 * The following aren't required, but may be sent, so need to be ignored
 * properly:
 * SGR    - ESC[m Set Graphic Rendition
 * DECSET - ESC[?h Set Mode
 * DECRST - ESC[?l Reset Mode
 * DECSCS - ESC[*r Select Communication Speed
 * 
 * In general, other commands may be used if one of the following is true:
 * 1) If they are ignored, there is not an impact on the presentation.
 *    This is where ESC[m, ESC[*r, and the ESC[?h/l commands we use fall
 *    into.
 * 2) They are reliably detected via sequences, terminal type, or a user
 *    setting.
 * 
 * The following may be used if support is detected for them:
 * (Detection can be via terminal type or via sending commands and
 *  reading the current position)
 * CHA - ESC[G Cursor Character Absolute
 * VPA - ESC[d Line Position Absolute
 */

enum ansi_mouse_mode {
	ANSI_MOUSE_X10  = 9,
	ANSI_MOUSE_NORM = 1000,
	ANSI_MOUSE_BTN  = 1002,
	ANSI_MOUSE_ANY  = 1003,
	ANSI_MOUSE_EXT  = 1006
};

// Was ansi()
const char *ANSI_Terminal::attrstr(unsigned atr)
{
	switch (atr) {

		/* Special case */
		case ANSI_NORMAL:
			return "\x1b[0m";
		case BLINK:
		case BG_BRIGHT:
			return "\x1b[5m";

		/* Foreground */
		case HIGH:
			return "\x1b[1m";
		case BLACK:
			return "\x1b[30m";
		case RED:
			return "\x1b[31m";
		case GREEN:
			return "\x1b[32m";
		case BROWN:
			return "\x1b[33m";
		case BLUE:
			return "\x1b[34m";
		case MAGENTA:
			return "\x1b[35m";
		case CYAN:
			return "\x1b[36m";
		case LIGHTGRAY:
			return "\x1b[37m";

		/* Background */
		case BG_BLACK:
			return "\x1b[40m";
		case BG_RED:
			return "\x1b[41m";
		case BG_GREEN:
			return "\x1b[42m";
		case BG_BROWN:
			return "\x1b[43m";
		case BG_BLUE:
			return "\x1b[44m";
		case BG_MAGENTA:
			return "\x1b[45m";
		case BG_CYAN:
			return "\x1b[46m";
		case BG_LIGHTGRAY:
			return "\x1b[47m";
	}

	return "-Invalid use of ansi()-";
}

#if 0 // Used by disabled telemate/qmodem code below
static uint32_t
popcnt(const uint32_t val)
{
	uint32_t i = val;

	// Clang optimizes this to popcnt on my system.
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	i = (i + (i >> 4)) & 0x0F0F0F0F;
	i *= 0x01010101;
	return  i >> 24;
}
#endif

// Was ansi() and ansi_attr()
char* ANSI_Terminal::attrstr(unsigned atr, unsigned curatr, char* str, size_t strsz)
{
	if (!supports(COLOR)) {  /* eliminate colors if terminal doesn't support them */
		if (atr & LIGHTGRAY)       /* if any foreground bits set, set all */
			atr |= LIGHTGRAY;
		if (atr & BG_LIGHTGRAY)  /* if any background bits set, set all */
			atr |= BG_LIGHTGRAY;
		if ((atr & LIGHTGRAY) && (atr & BG_LIGHTGRAY))
			atr &= ~LIGHTGRAY;  /* if background is solid, foreground is black */
		if (!atr)
			atr |= LIGHTGRAY;   /* don't allow black on black */
	}

	if (atr & FG_UNKNOWN)
		atr &= ~0x07;
	if (atr & BG_UNKNOWN)
		atr &= ~0x70;
	if (curatr & FG_UNKNOWN)
		curatr &= ~0x07;
	if (curatr & BG_UNKNOWN)
		curatr &= ~0x70;

	size_t lastret;
	if (supports(ICE_COLOR)) {
		switch (atr & (BG_BRIGHT | BLINK)) {
			case BG_BRIGHT:
			case BLINK:
				atr ^= BLINK;
				break;
		}
		switch (curatr & (BG_BRIGHT | BLINK)) {
			case BG_BRIGHT:
			case BLINK:
				curatr ^= BLINK;
				break;
		}
	}

	if (curatr == atr) { /* text hasn't changed. no sequence needed */
		*str = 0;
		return str;
	}

	lastret = strlcpy(str, "\033[", strsz);

// Not supported by Telemate or Qmodem, which we do support
#if 0
	uint32_t changed_mask = (curatr ^ atr) & (HIGH | BLINK | REVERSED | UNDERLINE | CONCEALED);
	if (changed_mask) {
		uint32_t set = popcnt(changed_mask & atr);
		uint32_t clear = popcnt(changed_mask & ~atr);
		uint32_t set_only_weight = set * 2 + 8;
		uint32_t set_clear_weight = set * 2 + clear * 3;

		if (atr & 0x70)
			set_only_weight += 3;
		if (!(atr & FG_UNKNOWN))
			set_only_weight += 3;
		if (!(atr & BG_UNKNOWN))
			set_only_weight += 3;

		if ((atr & (BG_UNKNOWN | 0x70)) != (curatr & (BG_UNKNOWN |0x70)))
			set_clear_weight += 3;
		if ((atr & (FG_UNKNOWN | 0x07)) != (curatr & (FG_UNKNOWN | 0x07)))
			set_clear_weight += 3;

		if (set_only_weight < set_clear_weight) {
			lastret = strlcat(str, "0;", strsz);
			curatr &= ~(0x77 | HIGH | BLINK | REVERSED | UNDERLINE | CONCEALED);
			curatr |= ANSI_NORMAL;
		}
	}
#else
	// TODO: Detect or configure this.
	// If turning off any of the special bits, reset
	// to normal rather than use the turn off code.
	if (((!(atr & HIGH)) && (curatr & HIGH)) ||
	    ((!(atr & BLINK)) && (curatr & BLINK)) ||
	    ((!(atr & REVERSED)) && (curatr & REVERSED)) ||
	    ((!(atr & UNDERLINE)) && (curatr & UNDERLINE)) ||
	    ((!(atr & CONCEALED)) && (curatr & CONCEALED))) {
		lastret = strlcat(str, "0;", strsz);
		curatr &= ~(0x77 | HIGH | BLINK | REVERSED | UNDERLINE | CONCEALED);
		curatr |= ANSI_NORMAL;
	}
#endif
	if (atr & HIGH) {                     /* special attributes */
		if (!(curatr & HIGH))
			lastret = strlcat(str, "1;", strsz);
	}
	else {
		if (curatr & HIGH)
			lastret = strlcat(str, "22;", strsz);
	}
	if (atr & BLINK) {
		if (!(curatr & BLINK))
			lastret = strlcat(str, "5;", strsz);
	}
	else {
		if (curatr & BLINK)
			lastret = strlcat(str, "25;", strsz);
	}
	if (atr & REVERSED) {
		if (!(curatr & REVERSED))
			lastret = strlcat(str, "7;", strsz);
	}
	else {
		if (curatr & REVERSED)
			lastret = strlcat(str, "27;", strsz);
	}
	if (atr & UNDERLINE) {
		if (!(curatr & UNDERLINE))
			lastret = strlcat(str, "4;", strsz);
	}
	else {
		if (curatr & UNDERLINE)
			lastret = strlcat(str, "24;", strsz);
	}
	if (atr & CONCEALED) {
		if (!(curatr & CONCEALED))
			lastret = strlcat(str, "8;", strsz);
	}
	else {
		if (curatr & CONCEALED)
			lastret = strlcat(str, "28;", strsz);
	}
	if ((atr & FG_UNKNOWN) && !(curatr & FG_UNKNOWN)) {
		lastret = strlcat(str, "39;", strsz);
		curatr &= ~0x07;
		curatr |= FG_UNKNOWN;
	}
	if ((atr & (FG_UNKNOWN | 0x07)) != (curatr & (FG_UNKNOWN | 0x07))) {
		switch (atr & 0x07) {
			case BLACK:
				lastret = strlcat(str, "30;", strsz);
				break;
			case RED:
				lastret = strlcat(str, "31;", strsz);
				break;
			case GREEN:
				lastret = strlcat(str, "32;", strsz);
				break;
			case BROWN:
				lastret = strlcat(str, "33;", strsz);
				break;
			case BLUE:
				lastret = strlcat(str, "34;", strsz);
				break;
			case MAGENTA:
				lastret = strlcat(str, "35;", strsz);
				break;
			case CYAN:
				lastret = strlcat(str, "36;", strsz);
				break;
			case LIGHTGRAY:
				lastret = strlcat(str, "37;", strsz);
				break;
		}
	}
	if ((atr & BG_UNKNOWN) && !(curatr & BG_UNKNOWN)) {
		lastret = strlcat(str, "39;", strsz);
		curatr &= ~0x70;
		curatr |= BG_UNKNOWN;
	}
	if ((atr & (BG_UNKNOWN | 0x70)) != (curatr & (BG_UNKNOWN | 0x70))) {
		switch (atr & 0x70) {
			/* The BG_BLACK macro is 0x200, so isn't in the mask */
			case 0 /* BG_BLACK */:
				lastret = strlcat(str, "40;", strsz);
				break;
			case BG_RED:
				lastret = strlcat(str, "41;", strsz);
				break;
			case BG_GREEN:
				lastret = strlcat(str, "42;", strsz);
				break;
			case BG_BROWN:
				lastret = strlcat(str, "43;", strsz);
				break;
			case BG_BLUE:
				lastret = strlcat(str, "44;", strsz);
				break;
			case BG_MAGENTA:
				lastret = strlcat(str, "45;", strsz);
				break;
			case BG_CYAN:
				lastret = strlcat(str, "46;", strsz);
				break;
			case BG_LIGHTGRAY:
				lastret = strlcat(str, "47;", strsz);
				break;
		}
	}
	if (lastret == 2) {  /* Convert <ESC>[ to blank */
		lastret = 0;
		if (strsz > 0) {
			*str = 0;
		}
	}
	else {
		// Replace ; with m
		if (strsz > (lastret)) {
			str[lastret - 1] = 'm';
			str[lastret] = 0;
		}
		lastret++;
	}

	if (lastret >= strsz) {
		sbbs->lprintf(LOG_ERR, "ANSI sequence attr %02X to %02X, strsz %zu too small", curatr, atr, strsz);
		if (strsz)
			str[0] = 0;
	}

	return str;
}

#define TIMEOUT_ANSI_GETXY  5   // Seconds
bool ANSI_Terminal::getdims()
{
	if (sbbs->sys_status & SS_USERON
	    && (sbbs->useron.rows == TERM_ROWS_AUTO || sbbs->useron.cols == TERM_COLS_AUTO)
	    && sbbs->online == ON_REMOTE) {                                 /* Remote */
		unsigned saved_line_counter = sbbs->term->lncntr;
		sbbs->term_out("\x1b[s\x1b[255B\x1b[255C\x1b[6n\x1b[u");
		sbbs->term->lncntr = saved_line_counter;
		return sbbs->inkey(K_ANSI_CPR, TIMEOUT_ANSI_GETXY * 1000) == 0;
	}
	return false;
}

bool ANSI_Terminal::getxy(unsigned* x, unsigned* y)
{
	size_t rsp = 0;
	int    ch;
	char   str[128];
	enum { state_escape, state_open, state_y, state_x } state = state_escape;

	if (x != NULL)
		*x = 0;
	if (y != NULL)
		*y = 0;

	sbbs->term_out("\x1b[6n");  /* Request cursor position */

	time_t start = time(NULL);
	sbbs->clearabort();
	while (sbbs->online && !(sbbs->sys_status & SS_ABORT) && rsp < sizeof(str) - 1) {
		if ((ch = sbbs->incom(1000)) != NOINP) {
			str[rsp++] = ch;
			if (ch == ESC && state == state_escape) {
				state = state_open;
				start = time(NULL);
			}
			else if (ch == '[' && state == state_open) {
				state = state_y;
				start = time(NULL);
			}
			else if (IS_DIGIT(ch) && state == state_y) {
				if (y != NULL) {
					(*y) *= 10;
					(*y) += (ch & 0xf);
				}
				start = time(NULL);
			}
			else if (ch == ';' && state == state_y) {
				state = state_x;
				start = time(NULL);
			}
			else if (IS_DIGIT(ch) && state == state_x) {
				if (x != NULL) {
					(*x) *= 10;
					(*x) += (ch & 0xf);
				}
				start = time(NULL);
			}
			else if (ch == 'R' && state == state_x)
				break;
			else {
				str[rsp] = '\0';
#ifdef _DEBUG
				char dbg[128];
				c_escape_str(str, dbg, sizeof(dbg), /* Ctrl-only? */ true);
				sbbs->lprintf(LOG_DEBUG, "Unexpected ansi_getxy response: '%s'", dbg);
#endif
				sbbs->ungetkeys(str, /* insert */ false);
				rsp = 0;
				state = state_escape;
			}
		}
		if (time(NULL) - start > TIMEOUT_ANSI_GETXY) {
			sbbs->lprintf(LOG_NOTICE, "!TIMEOUT in ansi_getxy");
			return false;
		}
	}

	return true;
}

bool ANSI_Terminal::gotoxy(unsigned x, unsigned y)
{
	if (x == 0)
		x = 1;
	if (y == 0)
		y = 1;
	if (optimize_gotoxy) {
		if (y == row + 1 && x == column + 1) {
			return true;
		}
		if (y == row + 1 && x == 1) {
			carriage_return();
			return true;
		}
		if (x == column + 1 && y > row + 1 && y - (row + 1) <= 6) {
			line_feed(y - (row + 1));
			return true;
		}
		if (x == 1 && y == 1) {
			cursor_home();
			return true;
		}
	}
	sbbs->term_printf("\x1b[%d;%dH", y, x);
	return true;
}

/*
 * TODO: gotox()/gotoy() can be done but aren't part of ANSI.SYS
 *       We need to auto-detect support before we can use them.
 */

// Was ansi_save
bool ANSI_Terminal::save_cursor_pos()
{
	sbbs->term_out("\x1b[s");
	return true;
}

// Was ansi_restore
bool ANSI_Terminal::restore_cursor_pos()
{
	sbbs->term_out("\x1b[u");
	return true;
}

void ANSI_Terminal::clearscreen()
{
	clear_hotspots();
	sbbs->term_out("\x1b[2J\x1b[H");    /* clear screen, home cursor */
	lastcrcol = 0;
}

void ANSI_Terminal::cleartoeos()
{
	sbbs->term_out("\x1b[J");
}

void ANSI_Terminal::cleartoeol()
{
	sbbs->term_out("\x1b[K");
}

void ANSI_Terminal::cursor_home()
{
	sbbs->term_out("\x1b[H");
}

void ANSI_Terminal::cursor_up(unsigned count = 1)
{
	if (count == 0)
		return;
	if (count > 1)
		sbbs->term_printf("\x1b[%dA", count);
	else
		sbbs->term_out("\x1b[A");
}

void ANSI_Terminal::cursor_down(unsigned count = 1)
{
	if (count == 0)
		return;
	if (count > 1)
		sbbs->term_printf("\x1b[%dB", count);
	else
		sbbs->term_out("\x1b[B");
}

void ANSI_Terminal::cursor_right(unsigned count = 1)
{
	if (count == 0)
		return;
	if (count > 1)
		sbbs->term_printf("\x1b[%dC", count);
	else
		sbbs->term_out("\x1b[C");
}

void ANSI_Terminal::cursor_left(unsigned count = 1) {
	if (count == 0)
		return;
	if (count < 4)
		sbbs->term_printf("%.*s", count, "\b\b\b");
	else
		sbbs->term_printf("\x1b[%dD", count);
}

void ANSI_Terminal::set_output_rate(enum output_rate speed) {
	unsigned int val = speed;
	switch (val) {
		case 0:     val = 0; break;
		case 600:   val = 2; break;
		case 1200:  val = 3; break;
		case 2400:  val = 4; break;
		case 4800:  val = 5; break;
		case 9600:  val = 6; break;
		case 19200: val = 7; break;
		case 38400: val = 8; break;
		case 57600: val = 9; break;
		case 76800: val = 10; break;
		default:
			if (val <= 300)
				val = 1;
			else if (val > 76800)
				val = 11;
			break;
	}
	sbbs->term_printf("\x1b[;%u*r", val);
	cur_output_rate = speed;
}

const char* ANSI_Terminal::type() {return "ANSI";}

static void ansi_mouse(sbbs_t *sbbs, enum ansi_mouse_mode mode, bool enable)
{
	char str[32] = "";
	SAFEPRINTF2(str, "\x1b[?%u%c", mode, enable ? 'h' : 'l');
	sbbs->term_out(str);
}

void ANSI_Terminal::set_mouse(unsigned flags) {
	if ((!supports(MOUSE)) && (mouse_mode != MOUSE_MODE_OFF))
		flags = MOUSE_MODE_OFF;
	if (supports(MOUSE) || flags == MOUSE_MODE_OFF) {
		unsigned mode = mouse_mode & ~flags;
		if (mode & MOUSE_MODE_X10)
			ansi_mouse(sbbs, ANSI_MOUSE_X10, false);
		if (mode & MOUSE_MODE_NORM)
			ansi_mouse(sbbs, ANSI_MOUSE_NORM, false);
		if (mode & MOUSE_MODE_BTN)
			ansi_mouse(sbbs, ANSI_MOUSE_BTN, false);
		if (mode & MOUSE_MODE_ANY)
			ansi_mouse(sbbs, ANSI_MOUSE_ANY, false);
		if (mode & MOUSE_MODE_EXT)
			ansi_mouse(sbbs, ANSI_MOUSE_EXT, false);

		mode = flags & ~mouse_mode;
		if (mode & MOUSE_MODE_X10)
			ansi_mouse(sbbs, ANSI_MOUSE_X10, true);
		if (mode & MOUSE_MODE_NORM)
			ansi_mouse(sbbs, ANSI_MOUSE_NORM, true);
		if (mode & MOUSE_MODE_BTN)
			ansi_mouse(sbbs, ANSI_MOUSE_BTN, true);
		if (mode & MOUSE_MODE_ANY)
			ansi_mouse(sbbs, ANSI_MOUSE_ANY, true);
		if (mode & MOUSE_MODE_EXT)
			ansi_mouse(sbbs, ANSI_MOUSE_EXT, true);

		if (mouse_mode != flags) {
			mouse_mode = flags;
		}
	}
}

void ANSI_Terminal::handle_control_code() {
	if (ansiParser.ansi_ibs == "") {
		switch (ansiParser.ansi_final_byte) {
			case 'E':	// NEL - Next Line
				set_column();
				inc_row();
				break;
			case 'M':	// RI - Reverse Line Feed
				dec_row();
				break;
			case 'c':	// RIS - Reset to Initial State.. homes
				set_column();
				set_row();
				break;
		}
	}
}

void ANSI_Terminal::set_color(int c, bool bg)
{
	if (curatr & REVERSED)
		bg = !bg;
	if (bg) {
		curatr &= ~(BG_BLACK | BG_UNKNOWN | 0x70);
		if (c == FG_UNKNOWN)
			curatr |= BG_UNKNOWN;
		else
			curatr |= c << 4;
	}
	else {
		curatr &= ~(FG_UNKNOWN | 0x07);
		curatr |= c;
	}
}

void ANSI_Terminal::handle_SGR_sequence() {
	unsigned cnt = ansiParser.count_params();
	unsigned pval;

	for (unsigned i = 0; i < cnt; i++) {
		pval = ansiParser.get_pval(i, 0);
		switch (pval) {
			case 0:
				curatr = ANSI_NORMAL;
				break;
			case 1:
				curatr |= HIGH;
				break;
			case 2:
				curatr &= ~HIGH;
				break;
			case 4:
				curatr |= UNDERLINE;
				break;
			case 5:
			case 6:
				if (flags_ & ICE_COLOR)
					curatr |= BG_BRIGHT;
				else
					curatr |= BLINK;
				break;
			case 7:
				if (!(curatr & REVERSED))
					curatr = REVERSED | (curatr & ~0x77) | ((curatr & 0x70) >> 4) | ((curatr & 0x07) << 4);
				break;
			case 8:
				curatr |= CONCEALED;
				break;
			case 22:
				curatr &= ~HIGH;
				break;
			case 24:
				curatr &= ~UNDERLINE;
				break;
			case 25:
				// Turn both off...
				curatr &= ~(BG_BRIGHT|BLINK);
				break;
			case 27:
				if (curatr & REVERSED)
					curatr = (curatr & ~(REVERSED | 0x77)) | ((curatr & 0x70) >> 4) | ((curatr & 0x07) << 4);
				break;
			case 28:
				curatr &= ~CONCEALED;
				break;
			case 30:
				set_color(BLACK, false);
				break;
			case 31:
				set_color(RED, false);
				break;
			case 32:
				set_color(GREEN, false);
				break;
			case 33:
				set_color(BROWN, false);
				break;
			case 34:
				set_color(BLUE, false);
				break;
			case 35:
				set_color(MAGENTA, false);
				break;
			case 36:
				set_color(CYAN, false);
				break;
			case 37:
				set_color(LIGHTGRAY, false);
				break;
			case 39:
				set_color(FG_UNKNOWN, false);
				break;
			case 40:
				set_color(BLACK, true);
				break;
			case 41:
				set_color(RED, true);
				break;
			case 42:
				set_color(GREEN, true);
				break;
			case 43:
				set_color(BROWN, true);
				break;
			case 44:
				set_color(BLUE, true);
				break;
			case 45:
				set_color(MAGENTA, true);
				break;
			case 46:
				set_color(CYAN, true);
				break;
			case 47:
				set_color(LIGHTGRAY, true);
				break;
			case 49:
				set_color(FG_UNKNOWN, true);
				break;
		}
	}
}

void ANSI_Terminal::handle_control_sequence() {
	unsigned pval;

	if (ansiParser.ansi_was_private) {
		// TODO: Track things like origin mode, auto wrap, etc.
	}
	else {
		if (ansiParser.ansi_ibs == "") {
			switch (ansiParser.ansi_final_byte) {
				case 'A':	// Cursor up
				case 'F':	// Cursor Preceding Line
				case 'k':	// Line Position Backward
					// Single parameter, default 1
					pval = ansiParser.get_pval(0, 1);
					if (pval > row)
						pval = row;
					dec_row(pval);
					break;
				case 'B':	// Cursor Down
				case 'E':	// Cursor Next Line
				case 'e':	// Line position Forward
					// Single parameter, default 1
					pval = ansiParser.get_pval(0, 1);
					if (pval >= (rows - row))
						pval = rows - row - 1;
					inc_row(pval);
					break;
				case 'C':	// Cursor Right
				case 'a':	// Cursor Position Forward
					// Single parameter, default 1
					pval = ansiParser.get_pval(0, 1);
					if (pval >= (cols - column))
						pval = cols - column - 1;
					inc_column(pval);
					break;
				case 'D':	// Cursor Left
				case 'j':	// Character Position Backward
					// Single parameter, default 1
					pval = ansiParser.get_pval(0, 1);
					if (pval > column)
						pval = column;
					dec_column(pval);
					break;
				case 'G':	// Cursor Character Absolute
				case '`':	// Character Position Absolute
					pval = ansiParser.get_pval(0, 1);
					if (pval > cols)
						pval = cols;
					if (pval == 0)
						pval = 1;
					set_column(pval - 1);
					break;
				case 'H':	// Cursor Position
				case 'f':	// Character and Line Position
					pval = ansiParser.get_pval(0, 1);
					if (pval > rows)
						pval = rows;
					if (pval == 0)
						pval = 1;
					set_row(pval - 1);
					pval = ansiParser.get_pval(1, 1);
					if (pval > cols)
						pval = cols;
					if (pval == 0)
						pval = 1;
					set_column(pval - 1);
					break;
				case 'I':	// Cursor Forward Tabulation
					// TODO
					break;
				case 'J':	// Clear screen
					// TODO: ANSI does not move cursor, ANSI-BBS does.
					set_row();
					set_column();
					break;
				case 'Y':	// Line tab
					// TODO: Track tabs
					break;
				case 'Z':	// Back tab
					// TODO: Track tabs
					break;
				case 'b':	// Repeat
					// TODO: Can't repeat ESC
					break;
				case 'd':	// Line Position Abolute
					pval = ansiParser.get_pval(0, 1);
					if (pval > rows)
						pval = rows;
					if (pval == 0)
						pval = 1;
					set_row(pval - 1);
					break;
				case 'm':	// Set Graphic Rendidtion
					handle_SGR_sequence();
					break;
				case 'r':	// Set Top and Bottom Margins
					// TODO
					break;
				case 's':	// Save Current Position (also set left/right margins)
					saved_row = row;
					saved_column = column;
					break;
				case 'u':	// Restore Cursor Position
					set_row(saved_row);
					set_column(saved_column);
					break;
				case 'z':	// Invoke Macro
					// TODO
					break;
				case 'N':	// "ANSI" Music
				case '|':	// SyncTERM Music
					// TODO
					break;
			}
		}
	}
}

bool ANSI_Terminal::parse_output(char ich) {
	unsigned char ch = static_cast<unsigned char>(ich);

	if (utf8_increment(ch))
		return true;

	switch (ansiParser.parse(ch)) {
		case ansiState_final:
			if (ansiParser.ansi_was_cc)
				handle_control_code();
			else if (!ansiParser.ansi_was_string)
				handle_control_sequence();
			ansiParser.reset();
			return true;
		case ansiState_broken:
			sbbs->lprintf(LOG_WARNING, "Sent %zu-character broken ANSI sequence '%s'"
				, ansiParser.ansi_sequence.length()
				, ansiParser.ansi_sequence.c_str());
			ansiParser.reset();
			return true;
		case ansiState_none:
			break;
		default:
			return true;
	}

	/* Track cursor position locally */
	switch (ch) {
		case '\a':  // 7
			/* Non-printing */
			break;
		case 8:  // BS
			dec_column();
			break;
		case 9:	// TAB
			if (column < (cols - 1)) {
				inc_column();
				while ((column < (cols - 1)) && (column % 8))
					inc_column();
			}
			break;
		case 10: // LF
			inc_row();
			break;
		case 12: // FF
			set_row();
			set_column();
			break;
		case 13: // CR
			lastcrcol = column;
			if (sbbs->console & CON_CR_CLREOL)
				cleartoeol();
			set_column();
			break;
		default:
			if ((charset() == CHARSET_UTF8) && (ch < 32)) {
				// Assume all other UTF-8 control characters do nothing.
				// TODO: Some might (ie: VT) but we can't really know
				//       what they'll do.
				break;
			}
			// Assume other CP427 control characters print a glyph.
			inc_column();
			break;
	}

	return true;
}

bool ANSI_Terminal::stuff_unhandled(char &ch, ANSI_Parser& ansi)
{
	if (ansi.ansi_sequence == "")
		return false;
	ch = ESC;
	std::string remain = ansi.ansi_sequence.substr(1);
	for (auto uch = remain.crbegin(); uch != remain.crend(); uch++) {
		sbbs->ungetkey(*uch, true);
	}
	return true;
}

bool ANSI_Terminal::stuff_str(char& ch, const char *str, bool skipctlcheck)
{
	const char *end = str;
	bool ret = false;
	if (!skipctlcheck) {
		if (str[0] < 32) {
			ch = str[0];
			ret = true;
			end = &str[1];
		}
	}
	for (const char *p = strchr(str, 0) - 1; p >= end; p--) {
		sbbs->ungetkey(*p, true);
	}
	return ret;
}

bool ANSI_Terminal::handle_left_press(unsigned x, unsigned y, char& ch, bool& retval)
{
	list_node_t* node = find_hotspot(x, y);
	if (node != NULL) {
		struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
#ifdef _DEBUG
		{
			char dbg[128];
			c_escape_str(spot->cmd, dbg, sizeof(dbg), /* Ctrl-only? */ true);
			sbbs->lprintf(LOG_DEBUG, "Stuffing hot spot command into keybuf: '%s'", dbg);
		}
#endif
		if (sbbs->pause_inside && !pause_hotspot) {
			// Abort the pause then send command
			ch = TERM_KEY_ABORT;
			stuff_str(ch, spot->cmd);
			retval = true;
			return true;
		}
		else {
			retval = stuff_str(ch, spot->cmd);
			return retval;
		}
	}
	if (sbbs->pause_inside && y == rows - 1) {
		ch = '\r';
		retval = true;
		return true;
	}
	return false;
}

bool ANSI_Terminal::handle_non_SGR_mouse_sequence(char& ch, ANSI_Parser& ansi)
{
	int button = sbbs->kbincom(100);
	if (button == NOINP) {
		sbbs->lprintf(LOG_DEBUG, "Timeout waiting for mouse button value");
		return false;
	}
	if (button < ' ') {
		sbbs->lprintf(LOG_DEBUG, "Unexpected mouse-button (0x%02X) tracking char: 0x%02X < ' '"
			, button, ch);
		return false;
	}
	button -= ' ';
	bool move{false};
	bool release{false};
	if (button == 3) {
		release = true;
		button &= ~0x20;
	}
	if (button & 0x20) {
		move = true;
		button &= ~0x20;
	}
	if (button >= 128)
		button -= 121;
	else if (button >= 64)
		button -= 61;
	if (button >= 11) {
		sbbs->lprintf(LOG_DEBUG, "Unexpected mouse-button (0x%02X) decoded", button);
		return false;
	}
	int inch = sbbs->kbincom(100);
	if (inch == NOINP) {
		sbbs->lprintf(LOG_DEBUG, "Timeout waiting for mouse X value");
		return false;
	}
	if (inch < '!') {
		sbbs->lprintf(LOG_DEBUG, "Unexpected mouse-button (0x%02X) tracking char: 0x%02X < '!'"
			, button, ch);
		return false;
	}
	int x = ch - '!';
	inch = sbbs->kbincom(100);
	if (inch == NOINP) {
		sbbs->lprintf(LOG_DEBUG, "Timeout waiting for mouse Y value");
		return false;
	}
	if (ch < '!') {
		sbbs->lprintf(LOG_DEBUG, "Unexpected mouse-button (0x%02X) tracking char: 0x%02X < '!'"
			, button, ch);
		return false;
	}
	int y = ch - '!';
	sbbs->lprintf(LOG_DEBUG, "X10 Mouse button-%s (0x%02X) reported at: %u x %u", release ? "release" : (move ? "move" : "press"), button, x, y);
	if (button == 0 && release == false && move == false) {
		bool retval{false};
		if (handle_left_press(x, y, ch, retval))
			return retval;
	}
	else if (button == 3 && release == false && move == false) {
		ch = TERM_KEY_UP;
		return true;
	}
	else if (button == 4 && release == false && move == false) {
		ch = TERM_KEY_DOWN;
		return true;
	}
	if ((sbbs->console & CON_MOUSE_CLK_PASSTHRU) && (!release))
		return stuff_unhandled(ch, ansi);
	if ((sbbs->console & CON_MOUSE_REL_PASSTHRU) && release)
		return stuff_unhandled(ch, ansi);
	if (button == 2) {
		ch = TERM_KEY_ABORT;
		return true;
	}
	return false;
}

bool ANSI_Terminal::handle_SGR_mouse_sequence(char& ch, ANSI_Parser& ansi, bool release)
{
	if (ansi.count_params() != 3) {
		sbbs->lprintf(LOG_DEBUG, "Invalid SGR mouse report sequence: '%s'", ansi.ansi_params.c_str());
		return false;
	}
	unsigned button = ansi.get_pval(0, 256);
	bool move{false};
	if (button & 0x20) {
		move = true;
		button &= ~0x20;
	}
	if (button >= 128)
		button -= 121;
	else if (button >= 64)
		button -= 61;
	if (button >= 11) {
		sbbs->lprintf(LOG_DEBUG, "Unexpected SGR mouse-button (0x%02X) decoded", button);
		return false;
	}
	unsigned x = ansi.get_pval(1, 0);
	unsigned y = ansi.get_pval(2, 0);
	if (x < 1 || y < 1) {
		sbbs->lprintf(LOG_DEBUG, "Invalid SGR mouse report position: '%s'", ansi.ansi_params.c_str());
		return false;
	}
	x--;
	y--;
	if (button == 0 && release == true && move == false) {
		bool retval{false};
		if (handle_left_press(x, y, ch, retval))
			return retval;
	}
	else if (button == 3 && release == false && move == false) {
		ch = TERM_KEY_UP;
		return true;
	}
	else if (button == 4 && release == false && move == false) {
		ch = TERM_KEY_DOWN;
		return true;
	}
	if ((sbbs->console & CON_MOUSE_CLK_PASSTHRU) && (!release))
		return stuff_unhandled(ch, ansi);
	if ((sbbs->console & CON_MOUSE_REL_PASSTHRU) && release)
		return stuff_unhandled(ch, ansi);
	if (button == 2) {
		ch = TERM_KEY_ABORT;
		return true;
	}
	return false;
}

bool ANSI_Terminal::parse_input_sequence(char& ch, int mode) {
	if (ch == ESC) {
		ANSI_Parser ansi{};
		ansi.parse(ESC);
		bool done = false;
		unsigned timeouts = 0;
		while (!done) {
			int rc = sbbs->kbincom(100);
			if (rc == NOINP) {	// Timed out
				if (++timeouts >= 30)
					break;
			}
			else if (rc & (~0xff)) {
				sbbs->lprintf(LOG_DEBUG, "Unhandled kbincom return %04x'", rc);
			}
			else {
				switch(ansi.parse(rc)) {
					case ansiState_final:
						done = 1;
						break;
					case ansiState_broken:
						sbbs->lprintf(LOG_DEBUG, "Invalid ANSI sequence: '\\e%s'", &(ansi.ansi_sequence.c_str()[1]));
						done = 1;
						break;
					default:
						break;
				}
			}
		}
		switch (ansi.current_state()) {
			case ansiState_final:
				if ((!ansi.ansi_was_private)
				    && ansi.ansi_ibs == ""
				    && (!ansi.ansi_was_cc)) {
					if (ansi.ansi_params == "") {
						switch (ansi.ansi_final_byte) {
							case 'A':
								ch = TERM_KEY_UP;
								return true;
							case 'B':
								ch = TERM_KEY_DOWN;
								return true;
							case 'C':
								ch = TERM_KEY_RIGHT;
								return true;
							case 'D':
								ch = TERM_KEY_LEFT;
								return true;
							case 'H':
								ch = TERM_KEY_HOME;
								return true;
							case 'V':
								ch = TERM_KEY_PAGEUP;
								return true;
							case 'U':
								ch = TERM_KEY_PAGEDN;
								return true;
							case 'F':
								ch = TERM_KEY_END;
								return true;
							case 'K':
								ch = TERM_KEY_END;
								return true;
							case 'M':
								return handle_non_SGR_mouse_sequence(ch, ansi);
							case '@':
								ch = TERM_KEY_INSERT;
								return true;
						}
					}
					else if (ansi.ansi_final_byte == '~' && ansi.count_params() == 1) {
						switch (ansi.get_pval(0, 0)) {
							case 1:
								ch = TERM_KEY_HOME;
								return true;
							case 2:
								ch = TERM_KEY_INSERT;
								return true;
							case 3:
								ch = TERM_KEY_DELETE;
								return true;
							case 4:
								ch = TERM_KEY_END;
								return true;
							case 5:
								ch = TERM_KEY_PAGEUP;
								return true;
							case 6:
								ch = TERM_KEY_PAGEDN;
								return true;
						}
					}
					else if (ansi.ansi_final_byte == 'R') {
						if (mode & K_ANSI_CPR) {  /* auto-detect rows */
							unsigned x = ansi.get_pval(1, 1);
							unsigned y = ansi.get_pval(0, 1);
							sbbs->lprintf(LOG_DEBUG, "received ANSI cursor position report: %ux%u"
								, x, y);
							/* Sanity check the coordinates in the response: */
							bool update{false};
							if (sbbs->useron.cols == TERM_COLS_AUTO && x >= TERM_COLS_MIN && x <= TERM_COLS_MAX) {
								cols = x;
								update = true;
							}
							if (sbbs->useron.rows == TERM_ROWS_AUTO && y >= TERM_ROWS_MIN && y <= TERM_ROWS_MAX) {
								rows = y;
								update = true;
							}
							if (update)
								sbbs->update_nodeterm();
						}
						// TODO: This was suppressed before the terminal-abstration
						//       branch.  I can't think of a good reason to keep doing
						//       that though, and feel it should be more like the rest.
						ch = 0;
						return true;
					}
				}
				else if (ansi.ansi_was_private
				    && ansi.ansi_ibs == ""
				    && (!ansi.ansi_was_cc)) {
					if (ansi.ansi_sequence[2] == '<') {
						switch (ansi.ansi_final_byte) {
							case 'm':
								if (handle_SGR_mouse_sequence(ch, ansi, true))
									return true;
								ch = 0;
								return false;
							case 'M':
								if (handle_SGR_mouse_sequence(ch, ansi, false))
									return true;
								ch = 0;
								return false;
						}
					}
				}
				// Fall through
			default:
				return stuff_unhandled(ch, ansi);
		}
	}
	return false;
}

struct mouse_hotspot* ANSI_Terminal::add_hotspot(struct mouse_hotspot* spot) {return nullptr;}

bool ANSI_Terminal::can_highlight() { return true; }
bool ANSI_Terminal::can_move() { return true; }
bool ANSI_Terminal::can_mouse()  { return flags_ & MOUSE; }
bool ANSI_Terminal::is_monochrome() { return !(flags_ & COLOR); }
