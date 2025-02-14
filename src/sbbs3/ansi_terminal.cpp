#include "ansi_terminal.h"

// Was ansi()
const char *ANSI_Terminal::attrstr(int atr)
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
char* ANSI_Terminal::attrstr(int atr, int curatr, char* str, size_t strsz)
{
	bool color = supports & COLOR;
	size_t lastret;
	if (supports & ICE_COLOR) {
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
	if (atr & BLINK) {                     /* special attributes */
		if (!(curatr & BLINK))
			lastret = strlcat(str, "5;", strsz);
	}
	if (atr & HIGH) {
		if (!(curatr & HIGH))
			lastret = strlcat(str, "1;", strsz);
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
		if (strsz > (lastret + 1)) {
			str[lastret] = 'm';
			str[lastret + 1] = 0;
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
	if (sbbs.sys_status & SS_USERON
	    && (sbbs.useron.rows == TERM_ROWS_AUTO || sbbs.useron.cols == TERM_COLS_AUTO)
	    && sbbs.online == ON_REMOTE) {                                 /* Remote */
		sbbs.putcom("\x1b[s\x1b[255B\x1b[255C\x1b[6n\x1b[u");
		return sbbs.inkey(K_ANSI_CPR, TIMEOUT_ANSI_GETXY * 1000) == 0;
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

	sbbs.putcom("\x1b[6n");  /* Request cursor position */

	time_t start = time(NULL);
	sbbs.sys_status &= ~SS_ABORT;
	while (sbbs.online && !(sbbs.sys_status & SS_ABORT) && rsp < sizeof(str) - 1) {
		if ((ch = sbbs.incom(1000)) != NOINP) {
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
				sbbs.ungetkeys(str, /* insert */ false);
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
	sbbs.comprintf("\x1b[%d;%dH", y, x);
	if (x > 0)
		sbbs.column = x - 1;
	if (y > 0)
		sbbs.row = y - 1;
	sbbs.lncntr = 0;
	return true;
}

// Was ansi_save
bool ANSI_Terminal::save_cursor_pos()
{
	sbbs.putcom("\x1b[s");
	return true;
}

// Was ansi_restore
bool ANSI_Terminal::restore_cursor_pos()
{
	sbbs.putcom("\x1b[u");
	return true;
}

void ANSI_Terminal::clearscreen()
{
	sbbs.putcom("\x1b[2J\x1b[H");    /* clear screen, home cursor */
	sbbs.row = 0;
	sbbs.column = 0;
	sbbs.lncntr = 0;
}

void ANSI_Terminal::cleartoeos()
{
	sbbs.putcom("\x1b[J");
}

void ANSI_Terminal::cleartoeol()
{
	sbbs.putcom("\x1b[K");
}

void ANSI_Terminal::cursor_home()
{
	sbbs.putcom("\x1b[H");
	sbbs.row = 0;
	sbbs.column = 0;
}

void ANSI_Terminal::cursor_up(unsigned count)
{
	if (count > 1)
		sbbs.comprintf("\x1b[%dA", count);
	else
		sbbs.putcom("\x1b[A");
	// TODO: Old version didn't update row?
	if (count > sbbs.row)
		count = sbbs.row;
	sbbs.row -= count;
}

void ANSI_Terminal::cursor_down(unsigned count)
{
	if (count > 1)
		sbbs.comprintf("\x1b[%dB", count);
	else
		sbbs.putcom("\x1b[B");
	// TODO: Old version assumes this can scroll
	if (sbbs.row + count > sbbs.rows)
		count = sbbs.rows - sbbs.row;
	inc_row(count);
}

void ANSI_Terminal::cursor_right(unsigned count)
{
	if (count > 1)
		sbbs.comprintf("\x1b[%dC", count);
	else
		sbbs.putcom("\x1b[C");
	// TODO: Old version would move past cols
	if (sbbs.column + count > sbbs.cols)
		count = sbbs.cols - sbbs.column;
	sbbs.column += count;
}

void ANSI_Terminal::cursor_left(unsigned count);
void ANSI_Terminal::set_output_rate();
// TODO: backfill?
// Not a complete replacement for term_type
char* ANSI_Terminal::type(char* str, size_t size);
const char* ANSI_Terminal::type();
int ANSI_Terminal::set_mouse(int mode);
void ANSI_Terminal::parse_outchar(char ch);
// Needs to handle C0 and C1
bool ANSI_Terminal::parse_ctrlkey(char ch, int mode);
struct mouse_hotspot* ANSI_Terminal::add_hotspot(struct mouse_hotspot* spot);
