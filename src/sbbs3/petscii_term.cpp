#include "petscii_term.h"
#include "petdefs.h"

// Initial work is only C64, not C126, C16, or VIC-20, and certainly not a PET

const char *PETSCII_Terminal::attrstr(unsigned atr)
{
	switch (atr) {

		/* Special case */
		case ANSI_NORMAL:
			return "\x0E\x92\x84\x9b";	// Upper/Lower, Reverse Off, Flash Off, Light Gray
		case BLINK:
		case BG_BRIGHT:
			return "\x82";			// Flash

		/* Foreground */
		case HIGH:
			return "\x05";			// Literally "White"
		case BLACK:
			return "\x90";
		case RED:
			return "\x1C";
		case GREEN:
			return "\x1E";
		case BROWN:
			return "\x95";
		case BLUE:
			return "\x1F";
		case MAGENTA:
			return "\x9C";
		case CYAN:
			return "\x9F";
		case LIGHTGRAY:
			return "\x9B";

		/* Background */
		case BG_BLACK:
			return "\x92";			// Reverse off
		case BG_RED:
			return "\x1C\x12";
		case BG_GREEN:
			return "\x1E\x12";
		case BG_BROWN:
			return "\x95\x12";
		case BG_BLUE:
			return "\x1F\x12";
		case BG_MAGENTA:
			return "\x9C\x12";
		case BG_CYAN:
			return "\x9F\x12";
		case BG_LIGHTGRAY:
			return "\x9B\x12";
	}

	return "-Invalid use of ansi()-";
}

static unsigned
xlat_atr(unsigned atr)
{
	if (atr & (0x70 | BG_BRIGHT)) {
		if (atr & BG_BRIGHT)
			atr |= HIGH;
		else
			atr &= ~HIGH;
		atr = (atr & (BLINK | HIGH)) | ((atr & 0x70) >> 4);
	}
	return atr;
}

char* PETSCII_Terminal::attrstr(unsigned atr, unsigned curatr, char* str, size_t strsz)
{
	unsigned newatr = xlat_atr(atr);
	unsigned oldatr = xlat_atr(curatr);
	size_t sp = 0;

	if (strsz < 2) {
		if (strsz > 0)
			str[0] = 0;
		return str;
	}

	if (newatr & (0x70 | BG_BRIGHT)) {	// Reversed
		if (!(oldatr & (0x70 | BG_BRIGHT)))
			str[sp++] = PETSCII_REVERSE_ON;
	}
	else {
		if (oldatr & (0x70 | BG_BRIGHT))
			str[sp++] = '\x92';
	}
	if (sp >= (strsz - 1)) {
		str[sp] = 0;
		return str;
	}
	if (newatr & BLINK) {
		if (!(oldatr & BLINK))
			str[sp++] = '\x82';
	}
	else {
		if (oldatr & BLINK)
			str[sp++] = '\x82';
	}
	if (sp >= (strsz - 1)) {
		str[sp] = 0;
		return str;
	}
	switch (atr & 0x0f) {
		case BLACK:
			str[sp++] = '\x90';
			break;
		case WHITE:
			str[sp++] = PETSCII_WHITE;
			break;
		case DARKGRAY:
			str[sp++] = '\x97';
			break;
		case LIGHTGRAY:
			str[sp++] = '\x9b';
			break;
		case BLUE:
			str[sp++] = PETSCII_BLUE;
			break;
		case LIGHTBLUE:
			str[sp++] = '\x9a';
			break;
		case CYAN:
			str[sp++] = '\x98';
			break;
		case LIGHTCYAN:
			str[sp++] = '\x9f';
			break;
		case YELLOW:
			str[sp++] = '\x9e';
			break;
		case BROWN:
			str[sp++] = '\x95';
			break;
		case RED:
			str[sp++] = PETSCII_RED;
			break;
		case LIGHTRED:
			str[sp++] = '\x96';
			break;
		case GREEN:
			str[sp++] = PETSCII_GREEN;
			break;
		case LIGHTGREEN:
			str[sp++] = '\x99';
			break;
		case MAGENTA:
			str[sp++] = '\x81';
			break;
		case LIGHTMAGENTA:
			str[sp++] = '\x9c';
			break;
	}
	str[sp] = 0;
	curatr = atr;
	return str;
}

bool PETSCII_Terminal::gotoxy(unsigned x, unsigned y)
{
	sbbs->outcom(PETSCII_HOME);
	column = 0;
	row = 0;
	cursor_down(y - 1);
	cursor_right(x - 1);
	return true;
}

// Was ansi_save
bool save_cursor_pos() {
	saved_x = column + 1;
	saved_y = row + 1;
	return true;
}

// Was ansi_restore
bool restore_cursor_pos() {
	if (saved_x && saved_y) {
		return gotoxy(saved_x, saved_y);
	}
	return false;
}

void PETSCII_Terminal::carriage_return()
{
	cursor_left(column);
}

void PETSCII_Terminal::line_feed(unsigned count)
{
	// Like cursor_down() but scrolls...
	for (unsigned i = 0; i < count; i++)
		sbbs->outcom(PETSCII_DOWN);
	cursor_down(count);
}

void PETSCII_Terminal::backspace(unsigned int count)
{
	sbbs->outcom(PETSCII_DELETE);
}

void PETSCII_Terminal::newline(unsigned count)
{
	sbbs->outcom('\r');
	inc_row();
	column = 0;
}

void PETSCII_Terminal::clearscreen()
{
	sbbs->outcom('\x93');
	row = 0;
	column = 0;
	lncntr = 0;
	clear_hotspots();
}

void PETSCII_Terminal::cleartoeos()
{
	int x = row + 1;
	int y = column + 1;

	cleartoeol();
	while (row < rows - 1) {
		cursor_down();
		clearline();
	}
	gotoxy(x, y);
}

void PETSCII_Terminal::cleartoeol()
{
	unsigned s, b;
	s = b = column;
	while (++s <= cols)
		sbbs->outcom(' ');
	while (++b <= cols)
		sbbs->outcom('\x9d');
}

void PETSCII_Terminal::clearline()
{
	int c = column;
	carriage_return();
	cleartoeol();
	cursor_right(c);
}

void PETSCII_Terminal::cursor_home()
{
	sbbs->outcom(PETSCII_HOME);
	row = 0;
	column = 0;
	lncntr = 0;
}

void PETSCII_Terminal::cursor_up(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		sbbs->outcom('\x91');
		if (row > 0)
			row--;
		if (lncntr > 0)
			lncntr--;
	}
}

void PETSCII_Terminal::cursor_down(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		if (row >= (rows - 1))
			break;
		sbbs->outcom(PETSCII_DOWN);
		inc_row();
	}
}

void PETSCII_Terminal::cursor_right(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		if (column >= (column - 1))
			break;
		sbbs->outcom(PETSCII_RIGHT);
		inc_column();
	}
}

void PETSCII_Terminal::cursor_left(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		sbbs->outcom('\x9d');
		if (column > 0)
			column--;
	}
}

const char* PETSCII_Terminal::type()
{
	return "PETSCII";
}

bool PETSCII_Terminal::parse_outchar(char ch)
{
	// TODO: This needs to be in here so we don't reset lncntr before checking
	if (ch == FF && lncntr > 0 && row > 0) {
		lncntr = 0;
		newline();
		if (!(sbbs->sys_status & SS_PAUSEOFF)) {
			sbbs->pause();
			while (lncntr && sbbs->online && !(sbbs->sys_status & SS_ABORT))
				pause();
		}
	}

	switch (ch) {
		// Zero-width characters we likely shouldn't send
		case 0:
		case 1:
		case 2:
		case 3:  // Stop
		case 4:
		case 6:
		case 7:
		//case 8:  // Translated as Backspace
		//case 9:  // Transpated as Tab
		//case 10: // Translated as Linefeed
		case 11:
		//case 12: // Translated as Form Feed
		//case 13: // Translated as Carriage Return
		case 14:
		case 15:
		case 16:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27: // ESC - This one is especially troubling
		case '\x80':
		case '\x82':
		case '\x83': // Run
		case '\x84':
		case '\x85': // F1
		case '\x86': // F3
		case '\x87': // F5
		case '\x88': // F7
		case '\x89': // F2
		case '\x8A': // F4
		case '\x8B': // F6
		case '\x8C': // F8
		case '\x8F':
			return false;

		// Zero-width characters we want to pass through
		case 5:  // White
		case 17: // Cursor down
		case 18: // Reverse on
		case 19: // Home
		case 20: // Delete
		case 28: // Red
		case 29: // Cursor right
		case 30: // Green
		case 31: // Blue
		case '\x81': // Orange
		case '\x8D': // Shift-return
		case '\x8E': // Upper case
		case '\x90': // Black
		case '\x91': // Cursor up
		case '\x92': // Reverse off
		case '\x93': // Clear
		case '\x94': // Insert
		case '\x95': // Brown
		case '\x96': // Pink
		case '\x97': // Dark gray
		case '\x98': // Gray
		case '\x99': // Light Green
		case '\x9A': // Light Blue
		case '\x9B': // Light Gray
		case '\x9C': // Purple
		case '\x9D': // Cursor Left
		case '\x9E': // Yellow
		case '\x9F': // Cyan
			return true;

		// Special values
		case 8:  // BS
			cursor_left();
			return false;
		case 9:  // TAB - Copy pasta... TODO
			// TODO: Original would wrap, this one (hopefully) doesn't.
			if (column < (cols - 1)) {
				column++;
				while ((column < (cols - 1)) && (column % tabstop)) {
					sbbs->outcom(' ');
					inc_column();
				}
			}
			return false;
		case 10: // LF
			if (sbbs->line_delay)
				SLEEP(sbbs->line_delay);
			line_feed();
			return false;
		case 12: // FF
			clearscreen();
			return false;
		case 13: // CR
			if (sbbs->console & CON_CR_CLREOL)
				cleartoeol();
			carriage_return();
			return false;

		// Everything else is assumed one byte wide
		default:
			inc_column();
			return true;
	}
}

bool PETSCII_Terminal::parse_ctrlkey(char& ch, int mode)
{
	switch (ch) {
		case 17:
			ch = TERM_KEY_DOWN;
			return true;
		case 19:
			ch = TERM_KEY_HOME;
			return true;
		case '\x93': // Clear (translate to End)
			ch = TERM_KEY_END;
			return true;
		case 20:
			ch = TERM_KEY_DELETE;
			return true;
		case 29:
			ch = TERM_KEY_RIGHT;
			return true;
		case '\x85': // F1
			ch = TERM_KEY_IGNORE;
			return true;
		case '\x86': // F3
			ch = TERM_KEY_IGNORE;
			return true;
		case '\x87': // F5
			ch = TERM_KEY_IGNORE;
			return true;
		case '\x88': // F7
			ch = TERM_KEY_IGNORE;
			return true;
		case '\x89': // F2
			ch = TERM_KEY_IGNORE;
			return true;
		case '\x8A': // F4
			ch = TERM_KEY_IGNORE;
			return true;
		case '\x8B': // F6
			ch = TERM_KEY_IGNORE;
			return true;
		case '\x8C': // F8
			ch = TERM_KEY_IGNORE;
			return true;
		case '\x91':
			ch = TERM_KEY_UP;
			return true;
		case '\x94':
			ch = TERM_KEY_INSERT;
			return true;
		case '\x9d':
			ch = TERM_KEY_LEFT;
			return true;
	}
	if (ch < 32)
		return true;
	return false;
}

void PETSCII_Terminal::insert_indicator()
{
	char str[32];
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
	gotoxy(column + 1, row + 1);
}
