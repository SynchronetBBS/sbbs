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
		// TODO: Log an error somehow?
		// TODO: This assumes strsz is at least one...
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
		sbbs->putcom("\x1b[s\x1b[255B\x1b[255C\x1b[6n\x1b[u");
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

	sbbs->putcom("\x1b[6n");  /* Request cursor position */

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
				lprintf(LOG_DEBUG, "Unexpected ansi_getxy response: '%s'", dbg);
#endif
				sbbs->ungetkeys(str, /* insert */ false);
				rsp = 0;
				state = state_escape;
			}
		}
		if (time(NULL) - start > TIMEOUT_ANSI_GETXY) {
			lprintf(LOG_NOTICE, "!TIMEOUT in ansi_getxy");
			return false;
		}
	}

	return true;
}

bool ANSI_Terminal::gotoxy(unsigned x, unsigned y)
{
	/*
	 * TODO: Previously, this was an int and sent exactly
	 *       what was passed, leaving it up to the terminal
	 *       to decide what to do about invalid sequences with
	 *       zeros and negative numbers.  Since negatives are
	 *       invalid sequences, and zeros should be default (1),
	 *       just force known consistent behaviour.
	 */
	if (x == 0)
		x = 1;
	if (y == 0)
		y = 1;
	sbbs->comprintf("\x1b[%d;%dH", y, x);
	if (x > 0)
		column = x - 1;
	if (y > 0)
		row = y - 1;
	lncntr = 0;
	return true;
}

// Was ansi_save
bool ANSI_Terminal::save_cursor_pos()
{
	sbbs->putcom("\x1b[s");
	return true;
}

// Was ansi_restore
bool ANSI_Terminal::restore_cursor_pos()
{
	sbbs->putcom("\x1b[u");
	return true;
}

void ANSI_Terminal::clearscreen()
{
	check_clear_pause();
	clear_hotspots();
	sbbs->putcom("\x1b[2J\x1b[H");    /* clear screen, home cursor */
	row = 0;
	column = 0;
	lncntr = 0;
	lbuflen = 0;
	lastlinelen = 0;
}

void ANSI_Terminal::cleartoeos()
{
	sbbs->putcom("\x1b[J");
}

void ANSI_Terminal::cleartoeol()
{
	sbbs->putcom("\x1b[K");
}

void ANSI_Terminal::cursor_home()
{
	sbbs->putcom("\x1b[H");
	row = 0;
	column = 0;
	// TODO: Did not reset lncntr
	lncntr = 0;
	lastlinelen = 0;
}

void ANSI_Terminal::cursor_up(unsigned count = 1)
{
	if (count == 0)
		return;
	if (count > 1)
		sbbs->comprintf("\x1b[%dA", count);
	else
		sbbs->putcom("\x1b[A");
	// TODO: Old version didn't update row?
	if (count > row)
		count = row;
	row -= count;
	// TODO: Did not adjust lncntr
	if (count > lncntr)
		count = lncntr;
	lncntr -= count;
}

void ANSI_Terminal::cursor_down(unsigned count = 1)
{
	if (count == 0)
		return;
	if (count > 1)
		sbbs->comprintf("\x1b[%dB", count);
	else
		sbbs->putcom("\x1b[B");
	// TODO: Old version assumes this can scroll
	if (row + count > rows)
		count = rows - row;
	inc_row(count);
}

void ANSI_Terminal::cursor_right(unsigned count = 1)
{
	if (count == 0)
		return;
	if (count > 1)
		sbbs->comprintf("\x1b[%dC", count);
	else
		sbbs->putcom("\x1b[C");
	// TODO: Old version would move past cols
	if (column + count > cols)
		count = cols - column;
	column += count;
}

void ANSI_Terminal::cursor_left(unsigned count = 1) {
	if (count == 0)
		return;
	if (count < 4)
		sbbs->comprintf("%.*s", count, "\b\b\b");
	else
		sbbs->comprintf("\x1b[%dD", count);
	if (column > count)
		column -= count;
	else
		column = 0;
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
	sbbs->comprintf("\x1b[;%u*r", val);
	cur_output_rate = speed;
}

// TODO: backfill?
const char* ANSI_Terminal::type() {return "ANSI";}


static void ansi_mouse(sbbs_t *sbbs, enum ansi_mouse_mode mode, bool enable)
{
	char str[32] = "";
	SAFEPRINTF2(str, "\x1b[?%u%c", mode, enable ? 'h' : 'l');
	sbbs->putcom(str);
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

bool ANSI_Terminal::parse_outchar(char ch) {
	// TODO: Actually parse these so the various functions don't
	//       need to update row/column/etc.
	if (ch == ESC && outchar_esc < ansiState_string)
		outchar_esc = ansiState_esc;
	else if (outchar_esc == ansiState_esc) {
		if (ch == '[')
			outchar_esc = ansiState_csi;
		else if (ch == '_' || ch == 'P' || ch == '^' || ch == ']')
			outchar_esc = ansiState_string;
		else if (ch == 'X')
			outchar_esc = ansiState_sos;
		else if (ch >= '@' && ch <= '_')
			outchar_esc = ansiState_final;
		else
			outchar_esc = ansiState_none;
	}
	else if (outchar_esc == ansiState_csi) {
		if (ch >= '@' && ch <= '~')
			outchar_esc = ansiState_final;
	}
	else if (outchar_esc == ansiState_string) {  // APS, DCS, PM, or OSC
		if (ch == ESC)
			outchar_esc = ansiState_esc;
		if (!((ch >= '\b' && ch <= '\r') || (ch >= ' ' && ch <= '~')))
			outchar_esc = ansiState_none;
	}
	else if (outchar_esc == ansiState_sos) { // SOS
		if (ch == ESC)
			outchar_esc = ansiState_sos_esc;
	}
	else if (outchar_esc == ansiState_sos_esc) { // ESC inside SOS
		if (ch == '\\')
			outchar_esc = ansiState_esc;
		else if (ch == 'X')
			outchar_esc = ansiState_none;
		else
			outchar_esc = ansiState_sos;
	}
	else
		outchar_esc = ansiState_none;

	if (outchar_esc != ansiState_none) {
		if (outchar_esc == ansiState_final)
			outchar_esc = ansiState_none;
		sbbs->outcom(ch);
		lbuf[lbuflen++] = ch;
		return false;
	}

	if (!required_parse_outchar(ch))
		return false;

	/* Track cursor position locally */
	switch (ch) {
		case '\a':  // 7
			/* Non-printing, does not go into lbuf */
			break;
		default:
			// TODO: All kinds of CTRL charaters not handled properly
			if (!lbuflen)
				latr = curatr;
			if (lbuflen < LINE_BUFSIZE)
				lbuf[lbuflen++] = ch;
			inc_column(1);
			break;
	}

	return true;
}

bool ANSI_Terminal::parse_ctrlkey(char& ch, int mode) {
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
		// TODO: Presumably this is already set...
		sbbs->autoterm |= ANSI;
		while (sp < 10 && to < 30) {       /* up to 3 seconds */
			rc = sbbs->kbincom(100);
			if (rc == NOINP) {
				to++;
				continue;
			}
			inch = rc;
			// TODO: Previously errors in parsing a button would continue, now it breaks
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

void ANSI_Terminal::insert_indicator() {
	char str[32];
	int  col = column;
	auto row = this->row;
	save_cursor_pos();
	gotoxy(cols, 1);
	int  tmpatr;
	if (sbbs->console & CON_INSERT) {
		sbbs->putcom(attrstr(tmpatr = BLINK | BLACK | (LIGHTGRAY << 4), curatr, str, supports(COLOR)));
		sbbs->outcom('I');
	} else {
		sbbs->putcom(attrstr(tmpatr = ANSI_NORMAL));
		sbbs->outcom(' ');
	}
	sbbs->putcom(attrstr(curatr, tmpatr, str, supports(COLOR)));
	restore_cursor_pos();
	column = col;
	this->row = row;
}

struct mouse_hotspot* ANSI_Terminal::add_hotspot(struct mouse_hotspot* spot) {return nullptr;}

bool ANSI_Terminal::can_highlight() { return true; }
bool ANSI_Terminal::can_move() { return true; }
bool ANSI_Terminal::can_mouse()  { return flags & MOUSE; }
bool ANSI_Terminal::is_monochrome() { return !(flags & COLOR); }
