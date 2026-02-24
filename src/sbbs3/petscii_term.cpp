#include "petscii_term.h"
#include "petdefs.h"

// Initial work is only C64 and C128 only, not C65, X16, C116, C16, Plus/4, or VIC-20, and certainly not a PET

const char *PETSCII_Terminal::attrstr(unsigned atr)
{
	switch (atr) {

		/* Special case */
		case ANSI_NORMAL:
			if (subset == PETSCII_C128_80)
				return "\x0E\x92\x8F\x9b";	// Upper/Lower, Reverse Off, Flash Off (C128), Light Gray
			return "\x0E\x92\x9b";	// Upper/Lower, Reverse Off, Light Gray
		case BLINK:
		case BG_BRIGHT:
			if (subset == PETSCII_C128_80)
				return "\x0F";			// Flash (C128 only)
			return "";

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
unreverse_attr(unsigned atr)
{
	// This drops all background bits and REVERSED
	return (atr & BLINK)			// Blink unchanged
	    | ((atr & BG_BRIGHT) ? HIGH : 0)	// Put BG_BRIGHT bit into HIGH
	    | ((atr & 0x70) >> 4);		// Background colour to Foreground
}

static unsigned
reverse_attr(unsigned atr)
{
	// This drops all foreground bits and sets REVERSED
	return REVERSED
	    | ((atr & HIGH) ? BG_BRIGHT : 0)	// Put HIGH bit into BG_BRIGHT
	    | (atr & BLINK)			// Blink unchanged
	    | ((atr & 0x07) << 4);		// Foreground colour to Background
}

/*
 * This deals with the reverse "stuff"
 * Basically, if the background is not black, the background colour is
 * set as the foreground colour, and REVERSED is set
 */
unsigned
PETSCII_Terminal::xlat_atr(unsigned atr)
{
	if (atr == ANSI_NORMAL) {
		// But convert to "normal" atr
		atr = LIGHTGRAY;
	}
	if (atr == BG_BLACK) {
		// But convert to "normal" atr
		atr &= ~(BG_BLACK | 0x70 | BG_BRIGHT);
	}
	// If this is already reversed, clear background
	if (atr & REVERSED) {
		atr &= ~0x70;
	}
	else {
		// If there is a background colour, translate to reversed with black
		if (atr & (0x70 | BG_BRIGHT)) {
			atr = REVERSED | unreverse_attr(atr);
		}
	}
	if (subset == PETSCII_C64)
		atr &= ~(BLINK | UNDERLINE);
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

	if (newatr & REVERSED) {
		if (!(oldatr & REVERSED)) {
			str[sp++] = PETSCII_REVERSE_ON;
			oldatr = reverse_attr(oldatr);
		}
	}
	else {
		if (oldatr & REVERSED) {
			str[sp++] = '\x92';
			oldatr = unreverse_attr(oldatr);
		}
	}
	if (sp >= (strsz - 1)) {
		str[sp] = 0;
		return str;
	}
	if (newatr & BLINK) {
		if (!(oldatr & BLINK))
			str[sp++] = '\x0F';	// C128
	}
	else {
		if (oldatr & BLINK)
			str[sp++] = '\x8F';	// C128
	}
	if (newatr & UNDERLINE) {
		if (!(oldatr & UNDERLINE))
			str[sp++] = '\x02';	// C128
	}
	else {
		if (oldatr & UNDERLINE)
			str[sp++] = '\x82';	// C128
	}
	if (sp >= (strsz - 1)) {
		str[sp] = 0;
		return str;
	}
	if ((newatr & 0x0f) != (oldatr & 0x0f)) {
		switch (newatr & 0x0f) {
			case BLACK:
				str[sp++] = '\x90';
				break;
			case WHITE:
				str[sp++] = PETSCII_WHITE;
				break;
			case DARKGRAY:
				if (subset == PETSCII_C128_80)
					str[sp++] = '\x98';
				else
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
				if (subset == PETSCII_C128_80)
					str[sp++] = '\x97';
				else
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
	}
	str[sp] = 0;
	return str;
}

bool PETSCII_Terminal::gotoxy(unsigned x, unsigned y)
{
	sbbs->term_out(PETSCII_HOME);
	if (y > rows)
		y = rows;
	if (x > cols)
		x = cols;
	while (row < (y - 1) && sbbs->online)
		sbbs->term_out(PETSCII_DOWN);
	while (column < (x - 1) && sbbs->online)
		sbbs->term_out(PETSCII_RIGHT);
	lncntr = 0;
	return true;
}

// Was ansi_save
bool PETSCII_Terminal::save_cursor_pos() {
	saved_x = column + 1;
	saved_y = row + 1;
	return true;
}

// Was ansi_restore
bool PETSCII_Terminal::restore_cursor_pos() {
	if (saved_x && saved_y) {
		return gotoxy(saved_x, saved_y);
	}
	return false;
}

void PETSCII_Terminal::carriage_return()
{
	lastcrcol = column;
	cursor_left(column);
}

void PETSCII_Terminal::line_feed(unsigned count)
{
	// Like cursor_down() but scrolls...
	for (unsigned i = 0; i < count; i++)
		sbbs->term_out(PETSCII_DOWN);
}

void PETSCII_Terminal::backspace(unsigned int count)
{
	for (unsigned i = 0; i < count; i++)
		sbbs->term_out(PETSCII_DELETE);
}

void PETSCII_Terminal::newline(unsigned count, bool no_bg_attr)
{
	int saved_attr = curatr;
	if (no_bg_attr && (curatr & BG_LIGHTGRAY)) { // Don't allow background colors to bleed when scrolling
		sbbs->attr(LIGHTGRAY);
	}
	for (unsigned i = 0; i < count; i++) {
		sbbs->term_out('\r');
		sbbs->check_pause();
	}
	sbbs->attr(saved_attr);
}

void PETSCII_Terminal::clearscreen()
{
	clear_hotspots();
	sbbs->term_out(PETSCII_CLEAR);
	lastcrcol = 0;
}

void PETSCII_Terminal::cleartoeos()
{
	int x = row + 1;
	int y = column + 1;

	cleartoeol();
	while (row < rows - 1 && sbbs->online) {
		cursor_down();
		clearline();
	}
	gotoxy(x, y);
}

void PETSCII_Terminal::cleartoeol()
{
	unsigned c = column;
	unsigned s;
	s = column;
	while (++s < cols && sbbs->online)
		sbbs->term_out(" ");
	while (column > c)
		cursor_left();
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
	sbbs->term_out(PETSCII_HOME);
}

void PETSCII_Terminal::cursor_up(unsigned count)
{
	for (unsigned i = 0; i < count; i++)
		sbbs->term_out(PETSCII_UP);
}

void PETSCII_Terminal::cursor_down(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		if (row >= (rows - 1))
			break;
		sbbs->term_out(PETSCII_DOWN);
	}
}

void PETSCII_Terminal::cursor_right(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		if (column >= (cols - 1))
			break;
		sbbs->term_out(PETSCII_RIGHT);
	}
}

void PETSCII_Terminal::cursor_left(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		sbbs->term_out(PETSCII_LEFT);
		if (column == 0)
			break;
	}
}

const char* PETSCII_Terminal::type()
{
	return "PETSCII";
}

void PETSCII_Terminal::set_color(int c)
{
	if (curatr & REVERSED) {
		curatr &= ~(BG_BRIGHT | 0x70);
		curatr |= ((c & 0x07) << 4);
		if (c & HIGH)
			curatr |= BG_BRIGHT;
	}
	else {
		curatr &= ~0x0F;
		curatr |= c;
	}
}

bool PETSCII_Terminal::parse_output(char ch)
{
	switch (ch) {
		// Zero-width characters we likely shouldn't send
		case 0:
		case 1:
		case 3:  // Stop
		case 4:
		case 6:
		case 7:
		case 9:
		case 10:
		case 11:
		case 16:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27: // ESC - This one is especially troubling
		case '\x80':
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
			return false;

		// Specials that affect cursor position
		case '\x8D': // Shift-return
		case 13: // Translated as Carriage Return
			lastcrcol = column;
			inc_row();
			set_column();
			if (curatr & 0xf0)
				curatr = (curatr & ~0xff) | ((curatr & 0xf0) >> 4);
			return true;
		case 14: // Lower-case
			return true;
		case 15: // Flash on (C128 only)
			if (subset == PETSCII_C128_80) {
				curatr |= BLINK;
				return true;
			}
			return false;
		case 17: // Cursor down
			inc_row();
			return true;
		case 19: // Home
		case '\x93': // Clear
			set_row();
			set_column();
			return true;
		case 20: // Delete
			if (column == 0) {
				dec_row();
				set_column(cols - 1);
			}
			else
				dec_column();
			return true;
		case 29: // Cursor right
			inc_column();
			return true;
		case '\x91': // Cursor up
			dec_row();
			return true;
		case '\x9D': // Cursor Left
			dec_column();
			return true;

		// Zero-width characters we want to pass through
		case 2:
			if (subset == PETSCII_C128_80) {
				curatr |= UNDERLINE;
				return true;
			}
			return false;
		case 18: // Reverse on
			if (!(curatr & REVERSED))
				curatr = reverse_attr(curatr);
			return true;
		case '\x92': // Reverse off
			if (curatr & REVERSED)
				curatr = unreverse_attr(curatr);
			return true;
		case 5:  // White
			set_color(WHITE);
			return true;
		case 28: // Red
			set_color(RED);
			return true;
		case 30: // Green
			set_color(GREEN);
			return true;
		case 31: // Blue
			set_color(BLUE);
			return true;
		case '\x81': // Orange
			set_color(MAGENTA);
			return true;
		case '\x82': // Underline off
			if (subset == PETSCII_C128_80) {
				curatr &= ~UNDERLINE;
				return true;
			}
			return false;
		case '\x8F': // Flash off (C128 only)
			if (subset == PETSCII_C128_80) {
				curatr &= ~BLINK;
				return true;
			}
			return false;
		case '\x90': // Black
			set_color(BLACK);
			return true;
		case '\x95': // Brown
			set_color(BROWN);
			return true;
		case '\x96': // Pink
			set_color(LIGHTRED);
			return true;
		case '\x97': // Dark gray or Cyan
			if (subset == PETSCII_C128_80)
				set_color(CYAN);
			else
				set_color(DARKGRAY);
			return true;
		case '\x98': // Cyan or Gray
			if (subset == PETSCII_C128_80)
				set_color(DARKGRAY);
			else
				set_color(CYAN);
			return true;
		case '\x99': // Light Green
			set_color(LIGHTGREEN);
			return true;
		case '\x9A': // Light Blue
			set_color(LIGHTBLUE);
			return true;
		case '\x9B': // Light Gray
			set_color(LIGHTGRAY);
			return true;
		case '\x9C': // Purple
			set_color(LIGHTMAGENTA);
			return true;
		case '\x9E': // Yellow
			set_color(YELLOW);
			return true;
		case '\x9F': // Cyan
			set_color(LIGHTCYAN);
			return true;
		case '\x8E': // Upper case
		case '\x94': // Insert
			return true;

		// Everything else is assumed one byte wide
		default:
			inc_column();
			return true;
	}
	return false;
}

bool PETSCII_Terminal::can_highlight() { return true; }
bool PETSCII_Terminal::can_move() { return true; }
bool PETSCII_Terminal::is_monochrome() { return false; }

void PETSCII_Terminal::updated() {
	if (cols == 80)
		subset = PETSCII_C128_80;
	else
		subset = PETSCII_C64;
}
