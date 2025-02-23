#include "ansi_terminal.h"

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

// Was ansi() and ansi_attr()
char* ANSI_Terminal::attrstr(unsigned atr, unsigned curatr, char* str, size_t strsz)
{
	if (curatr & 0x100) {
		if (curatr != ANSI_NORMAL)
			sbbs->lprintf(LOG_WARNING, "Invalid current attribute %04x", curatr);
		curatr = 0x07;
	}
	bool color = supports(COLOR);
	size_t lastret;
	if (supports(ICE_COLOR)) {
		switch (atr & (BG_BRIGHT | BLINK)) {
			case BG_BRIGHT:
			case BLINK:
				atr ^= BLINK;
				break;
		}
	}

	if (!color) {  /* eliminate colors if terminal doesn't support them */
		if (atr & LIGHTGRAY)       /* if any foreground bits set, set all */
			atr |= LIGHTGRAY;
		if (atr & BG_LIGHTGRAY)  /* if any background bits set, set all */
			atr |= BG_LIGHTGRAY;
		if ((atr & LIGHTGRAY) && (atr & BG_LIGHTGRAY))
			atr &= ~LIGHTGRAY;  /* if background is solid, foreground is black */
		if (!atr)
			atr |= LIGHTGRAY;   /* don't allow black on black */
	}
	if (curatr == atr) { /* text hasn't changed. no sequence needed */
		*str = 0;
		return str;
	}

	lastret = strlcpy(str, "\033[", strsz);
	if ((!(atr & HIGH) && curatr & HIGH) || (!(atr & BLINK) && curatr & BLINK)
	    || atr == LIGHTGRAY) {
		lastret = strlcat(str, "0;", strsz);
		curatr = LIGHTGRAY;
	}
	if (atr & HIGH) {                     /* special attributes */
		if (!(curatr & HIGH))
			lastret = strlcat(str, "1;", strsz);
	}
	if (atr & BLINK) {
		if (!(curatr & BLINK))
			lastret = strlcat(str, "5;", strsz);
	}
	if ((atr & 0x07) != (curatr & 0x07)) {
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
	if ((atr & 0x70) != (curatr & 0x70)) {
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
		sbbs->term_out("\x1b[s\x1b[255B\x1b[255C\x1b[6n\x1b[u");
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
	sbbs->sys_status &= ~SS_ABORT;
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
	sbbs->term_printf("\x1b[%d;%dH", y, x);
	return true;
}

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

// TODO: backfill?
const char* ANSI_Terminal::type() {return "ANSI";}


static void ansi_mouse(sbbs_t *sbbs, enum ansi_mouse_mode mode, bool enable)
{
	char str[32] = "";
	SAFEPRINTF2(str, "\x1b[?%u%c", mode, enable ? 'h' : 'l');
	sbbs->term_out(str);
}

void ANSI_Terminal::set_mouse(unsigned flags) {
	// TODO: This is new... disable mouse if not enabled.
	if (!supports(MOUSE))
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

void ANSI_Terminal::handle_SGR_sequence() {
	unsigned cnt = ansiParser.count_params();
	unsigned pval;

	for (unsigned i = 0; i < cnt; i++) {
		pval = ansiParser.get_pval(i, 0);
		switch (pval) {
			case 0:
				// Don't use ANSI_NORMAL, it's only for the const thing
				curatr = 0x07;
				break;
			case 1:
				curatr &= ~0x100;
				curatr |= 0x08;
				break;
			case 2:
				curatr &= ~0x108;
				break;
			case 5:
			case 6:
				curatr &= ~0x100;
				if (flags_ & ICE_COLOR)
					curatr |= BG_BRIGHT;
				else
					curatr |= BLINK;
				break;
			case 7:
				curatr &= ~0x100;
				if (!is_negative) {
					curatr = (curatr & ~0x77) | ((curatr & 0x70) >> 4) | ((curatr & 0x07) << 4);
					is_negative = true;
				}
				break;
			case 8:
				curatr &= ~0x100;
				curatr = (curatr & ~0x07) | ((curatr & 0x70) >> 4);
				break;
			case 22:
				curatr &= ~0x108;
				break;
			case 25:
				curatr &= ~0x100;
				if (flags_ & ICE_COLOR)
					curatr &= ~BG_BRIGHT;
				else
					curatr &= ~BLINK;
				break;
			case 27:
				if (is_negative) {
					curatr = (curatr & ~0x77) | ((curatr & 0x70) >> 4) | ((curatr & 0x07) << 4);
					is_negative = false;
				}
				break;
			case 30:
				curatr &= ~0x107;
				curatr |= BLACK;
				break;
			case 31:
				curatr &= ~0x107;
				curatr |= RED;
				break;
			case 32:
				curatr &= ~0x107;
				curatr |= GREEN;
				break;
			case 33:
				curatr &= ~0x107;
				curatr |= BROWN;
				break;
			case 34:
				curatr &= ~0x107;
				curatr |= BLUE;
				break;
			case 35:
				curatr &= ~0x107;
				curatr |= MAGENTA;
				break;
			case 36:
				curatr &= ~0x107;
				curatr |= CYAN;
				break;
			case 37:
				curatr &= ~0x107;
				curatr |= LIGHTGRAY;
				break;
			case 40:
				curatr &= ~0x370;
				// Don't use BG_BLACK, it's only for the const thing.
				//curatr |= BG_BLACK;
				break;
			case 41:
				curatr &= ~0x370;
				curatr |= BG_RED;
				break;
			case 42:
				curatr &= ~0x370;
				curatr |= BG_GREEN;
				break;
			case 43:
				curatr &= ~0x370;
				curatr |= BG_BROWN;
				break;
			case 44:
				curatr &= ~0x370;
				curatr |= BG_BLUE;
				break;
			case 45:
				curatr &= ~0x370;
				curatr |= BG_MAGENTA;
				break;
			case 46:
				curatr &= ~0x370;
				curatr |= BG_CYAN;
				break;
			case 47:
				curatr &= ~0x370;
				curatr |= BG_LIGHTGRAY;
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
	if (flags_ & UTF8) {
		// TODO: How many errors should be logged?
		if (utf8_remain > 0) {
			// First, check if this is overlong...
			if (first_continuation
			    && ((utf8_remain == 1 && ch < 0xA0)
			    || (utf8_remain == 2 && ch < 0x90)
			    || (utf8_remain == 3 && ch >= 0x90))) {
				sbbs->lprintf(LOG_WARNING, "Sending invalid UTF-8 codepoint");
				first_continuation = false;
				utf8_remain = 0;
			}
			else if ((ch & 0xc0) != 0x80) {
				first_continuation = false;
				utf8_remain = 0;
				sbbs->lprintf(LOG_WARNING, "Sending invalid UTF-8 codepoint");
			}
			else {
				first_continuation = false;
				utf8_remain--;
				codepoint <<= 6;
				codepoint |= (ch & 0x3f);
				if (utf8_remain)
					return true;
				inc_column(unicode_width(static_cast<enum unicode_codepoint>(codepoint), 0));
				codepoint = 0;
				return true;
			}
		}
		else if ((ch & 0x80) != 0) {
			if ((ch == 0xc0) || (ch == 0xc1) || (ch > 0xF5)) {
				sbbs->lprintf(LOG_WARNING, "Sending invalid UTF-8 codepoint");
			}
			if ((ch & 0xe0) == 0xc0) {
				utf8_remain = 1;
				if (ch == 0xE0)
					first_continuation = true;
				codepoint = ch & 0x1F;
				return true;
			}
			else if ((ch & 0xf0) == 0xe0) {
				utf8_remain = 2;
				if (ch == 0xF0)
					first_continuation = true;
				codepoint = ch & 0x0F;
				return true;
			}
			else if ((ch & 0xf8) == 0xf0) {
				utf8_remain = 3;
				if (ch == 0xF4)
					first_continuation = true;
				codepoint = ch & 0x07;
				return true;
			}
			else
				sbbs->lprintf(LOG_WARNING, "Sending invalid UTF-8 codepoint");
		}
		if (utf8_remain)
			return true;
	}

	switch (ansiParser.parse(ch)) {
		case ansiState_final:
			if (ansiParser.ansi_was_cc)
				handle_control_code();
			else if (!ansiParser.ansi_was_string)
				handle_control_sequence();
			ansiParser.reset();
			return true;
		case ansiState_broken:
			sbbs->lprintf(LOG_WARNING, "Sent broken ANSI sequence '%s'", ansiParser.ansi_sequence.c_str());
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

bool ANSI_Terminal::parse_input(char& ch, int mode) {
	char inch;
	char str[512];

	if (ch == ESC) {
		int rc = sbbs->kbincom((mode & K_GETSTR) ? 3000:1000);
		if (rc == NOINP)        // timed-out waiting for '['
			return true;
		inch = rc;
		if (inch != '[') {
			sbbs->ungetkey(inch, true);
			return true;
		}
		int sp = 0; // String position
		int to = 0; // Number of input timeouts
		while (sp < 10 && to < 30) {       /* up to 3 seconds */
			rc = sbbs->kbincom(100);
			if (rc == NOINP) {
				to++;
				continue;
			}
			inch = rc;
			if (sp == 0 && inch == 'M' && mouse_mode != MOUSE_MODE_OFF) {
				if (sp == 0)
					str[sp++] = ch;
				int button = sbbs->kbincom(100);
				if (button == NOINP) {
					sbbs->lprintf(LOG_DEBUG, "Timeout waiting for mouse button value");
					break;
				}
				str[sp++] = button;
				inch = sbbs->kbincom(100);
				if (inch < '!') {
					sbbs->lprintf(LOG_DEBUG, "Unexpected mouse-button (0x%02X) tracking char: 0x%02X < '!'"
						, button, ch);
					break;
				}
				str[sp++] = ch;
				int x = ch - '!';
				inch = sbbs->kbincom(100);
				if (ch < '!') {
					sbbs->lprintf(LOG_DEBUG, "Unexpected mouse-button (0x%02X) tracking char: 0x%02X < '!'"
						, button, ch);
					break;
				}
				str[sp++] = ch;
				int y = ch - '!';
				sbbs->lprintf(LOG_DEBUG, "X10 Mouse button-click (0x%02X) reported at: %u x %u", button, x, y);
				if (button == 0x20) { // Left-click
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
							ch = TERM_KEY_ABORT;
							sbbs->ungetkeys(spot->cmd, true);
						}
						else {
							if (spot->cmd[0] < 32 || spot->cmd[0] == 127) {
								ch = spot->cmd[0];
								if (spot->cmd[1])
									sbbs->ungetkeys(&spot->cmd[1]);
							}
							else {
								sbbs->ungetkeys(spot->cmd);
							}
						}
						return (ch < 32 || ch == 127);
					}
					if (sbbs->pause_inside && y == rows - 1) {
						ch = '\r';
						return true;
					}
				} else if (button == '`' && sbbs->console & CON_MOUSE_SCROLL) {
					ch = TERM_KEY_UP;
					return true;
				} else if (button == 'a' && sbbs->console & CON_MOUSE_SCROLL) {
					ch = TERM_KEY_DOWN;
					return true;
				}
				if ((button != 0x23 && sbbs->console & CON_MOUSE_CLK_PASSTHRU)
				    || (button == 0x23 && sbbs->console & CON_MOUSE_REL_PASSTHRU)) {
					for (size_t j = sp; j > 0; j--)
						sbbs->ungetkey(str[j - 1], /* insert: */ true);
					sbbs->ungetkey('[', /* insert: */ true);
					ch = ESC;
					return true;
				}
				if (button == 0x22) {  // Right-click
					ch = TERM_KEY_ABORT;
					return true;
				}
				ch = TERM_KEY_IGNORE;
				return true;
			}

			if (sp == 0 && inch == '<' && mouse_mode != MOUSE_MODE_OFF) {
				while (sp < (int)sizeof(str) - 1) {
					int byte = sbbs->kbincom(100);
					if (byte == NOINP) {
						sbbs->lprintf(LOG_DEBUG, "Timeout waiting for mouse report character (%d)", sp);
						break;;
					}
					str[sp++] = byte;
					if (IS_ALPHA(byte))
						break;
				}
				str[sp] = 0;
				int button = -1, x = 0, y = 0;
				if (sscanf(str, "%d;%d;%d%c", &button, &x, &y, &inch) != 4
				    || button < 0 || x < 1 || y < 1 || toupper(inch) != 'M') {
					sbbs->lprintf(LOG_DEBUG, "Invalid SGR mouse report sequence: '%s'", str);
					return 0;
				}
				--x;
				--y;
				sbbs->lprintf(LOG_DEBUG, "SGR Mouse button (0x%02X) %s reported at: %u x %u"
					, button, inch == 'M' ? "PRESS" : "RELEASE", x, y);
				if (button == 0 && inch == 'm') { // Left-button release
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
							ch = TERM_KEY_ABORT;
							sbbs->ungetkeys(spot->cmd, true);
						}
						else {
							if (spot->cmd[0] < 32 || spot->cmd[0] == 127) {
								ch = spot->cmd[0];
								if (spot->cmd[1])
									sbbs->ungetkeys(&spot->cmd[1]);
							}
							else {
								sbbs->ungetkeys(spot->cmd);
							}
						}
						return (ch < 32 || ch == 127);
					}
					if (sbbs->pause_inside && y == rows - 1) {
						ch = '\r';
						return true;
					}
				} else if (button == 0x40 && sbbs->console & CON_MOUSE_SCROLL) {
					ch = TERM_KEY_UP;
					return true;
				} else if (button == 0x41 && sbbs->console & CON_MOUSE_SCROLL) {
					ch = TERM_KEY_DOWN;
					return true;
				}
				if ((inch == 'M' && sbbs->console & CON_MOUSE_CLK_PASSTHRU)
				    || (ch == 'm' && sbbs->console & CON_MOUSE_REL_PASSTHRU)) {
					sbbs->lprintf(LOG_DEBUG, "Passing-through SGR mouse report: 'ESC[<%s'", str);
					for (size_t j = sp; j > 0; j--)
						sbbs->ungetkey(str[j - 1], /* insert: */ true);
					sbbs->ungetkey('<', /* insert: */ true);
					sbbs->ungetkey('[', /* insert: */ true);
					ch = ESC;
					return true;
				}
				if (inch == 'M' && button == 2) {  // Right-click
					ch = TERM_KEY_ABORT;
					return true;
				}
#ifdef _DEBUG
				sbbs->lprintf(LOG_DEBUG, "Eating SGR mouse report: 'ESC[<%s'", str);
#endif
				return 0;
			}
			if (inch != ';' && !IS_DIGIT(inch) && inch != 'R') {    /* other ANSI */
				str[sp] = 0;
				switch (inch) {
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
					case 'H':   /* ANSI:  home cursor */
						ch = TERM_KEY_HOME;
						return true;
					case 'V':
						ch = TERM_KEY_PAGEUP;
						return true;
					case 'U':
						ch = TERM_KEY_PAGEDN;
						return true;
					case 'F':   /* Xterm: cursor preceding line */
					case 'K':   /* ANSI:  clear-to-end-of-line */
						ch = TERM_KEY_END;
						return true;
					case '@':   /* ANSI/ECMA-048 INSERT */
						ch = TERM_KEY_INSERT;
						return true;
					case '~':   /* VT-220 (XP telnet.exe) */
						switch (atoi(str)) {
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
						break;
				}
				sbbs->ungetkey(inch, /* insert: */ true);
				for (size_t j = sp; j > 0; j--)
					sbbs->ungetkey(str[j - 1], /* insert: */ true);
				sbbs->ungetkey('[', /* insert: */ true);
				ch = ESC;
				return true;
			}
			if (inch == 'R') {       /* cursor position report */
				if (mode & K_ANSI_CPR && sp) {  /* auto-detect rows */
					int x, y;
					str[sp] = 0;
					if (sscanf(str, "%u;%u", &y, &x) == 2) {
						sbbs->lprintf(LOG_DEBUG, "received ANSI cursor position report: %ux%u"
							, x, y);
						/* Sanity check the coordinates in the response: */
						if (sbbs->useron.cols == TERM_COLS_AUTO && x >= TERM_COLS_MIN && x <= TERM_COLS_MAX)
							cols = x;
						if (sbbs->useron.rows == TERM_ROWS_AUTO && y >= TERM_ROWS_MIN && y <= TERM_ROWS_MAX)
							rows = y;
						if (sbbs->useron.cols == TERM_COLS_AUTO || sbbs->useron.rows == TERM_ROWS_AUTO)
							sbbs->update_nodeterm();
					}
				}
				ch = TERM_KEY_IGNORE;
				return true;
			}
			str[sp++] = ch;
		}

		for (size_t j = sp; j > 0; j--)
			sbbs->ungetkey(str[j - 1], /* insert: */ true);
		sbbs->ungetkey('[', /* insert: */ true);
		ch = ESC;
		return true;
	}
	if (ch < 32 || ch == 127)
		return true;
	return false;
}

struct mouse_hotspot* ANSI_Terminal::add_hotspot(struct mouse_hotspot* spot) {return nullptr;}

bool ANSI_Terminal::can_highlight() { return true; }
bool ANSI_Terminal::can_move() { return true; }
bool ANSI_Terminal::can_mouse()  { return flags_ & MOUSE; }
bool ANSI_Terminal::is_monochrome() { return !(flags_ & COLOR); }
