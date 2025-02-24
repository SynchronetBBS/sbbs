#include "petscii_term.h"
#include "petdefs.h"

// Initial work is only C64, not C128, C116, C16, Plus/4, or VIC-20, and certainly not a PET

const char *PETSCII_Terminal::attrstr(unsigned atr)
{
	switch (atr) {

		/* Special case */
		case ANSI_NORMAL:
			return "\x0E\x92\x8F\x9b";	// Upper/Lower, Reverse Off, Flash Off (C128), Light Gray
		case BLINK:
		case BG_BRIGHT:
			return "\x0F";			// Flash (C128 only)

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
	// This drops all background bits
	return (atr & BLINK)			// Blink unchanged
	    | ((atr & BG_BRIGHT) ? HIGH : 0)	// Put BG_BRIGHT bit into HIGH
	    | ((atr & 0x70) >> 4);		// Background colour to Foreground
}

static unsigned
reverse_attr(unsigned atr)
{
	// This drops all foreground bits
	return ((atr & HIGH) ? BG_BRIGHT : 0)	// Put HIGH bit into BG_BRIGHT
	    | (atr & BLINK)			// Blink unchanged
	    | ((atr & 0x07) << 4);		// Foreground colour to Background
}

/*
 * This deals with the reverse "stuff"
 * Basically, if the background is not black, the background colour is
 * set as the foreground colour, and BG_BLACK is set
 * 
 * TODO: This hackery around BG_BLACK is terrible.
 */
static unsigned
xlat_atr(unsigned atr)
{
	// BG_BLACK bit trumps all
	if (atr & BG_BLACK) {
		// But convert to "normal" atr
		atr &= ~(BG_BLACK | 0x70 | BG_BRIGHT);
	}
	// If there is a background colour...
	if (atr & (0x70 | BG_BRIGHT)) {
		atr = BG_BLACK | unreverse_attr(atr);
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

	if (newatr & BG_BLACK) {	// Reversed
		if (!(oldatr & BG_BLACK)) {
			str[sp++] = PETSCII_REVERSE_ON;
			oldatr = reverse_attr(oldatr);
		}
	}
	else {
		if (oldatr & BG_BLACK) {
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
	}
	str[sp] = 0;
	return str;
}

bool PETSCII_Terminal::gotoxy(unsigned x, unsigned y)
{
	sbbs->term_out(PETSCII_HOME);
	while (row < (y - 1) && sbbs->online)
		sbbs->term_out(PETSCII_DOWN);
	while (column < (x - 1) && sbbs->online)
		sbbs->term_out(PETSCII_RIGHT);
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

void PETSCII_Terminal::newline(unsigned count)
{
	sbbs->term_out('\r');
}

void PETSCII_Terminal::clearscreen()
{
	clear_hotspots();
	sbbs->term_out('\x93');
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
	unsigned s;
	s = column;
	while (++s <= cols && sbbs->online)
		sbbs->term_out(" \x14");
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
		sbbs->term_out('\x91');
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
		sbbs->term_out('\x9d');
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
	if (reverse_on) {
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
		case 2:
		case 3:  // Stop
		case 4:
		case 6:
		case 7:
		case 9:
		case 10:
		case 11:
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

		// Specials that affect cursor position
		case '\x8D': // Shift-return
		case 13: // Translated as Carriage Return
			inc_row();
			set_column(0);
			if (curatr & 0xf0)
				curatr = (curatr & ~0xff) | ((curatr & 0xf0) >> 4);
			return true;
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
		case 18: // Reverse on
			if (!reverse_on) {
				reverse_on = true;
				curatr = reverse_attr(curatr);
			}
			return true;
		case '\x92': // Reverse off
			if (reverse_on) {
				reverse_on = false;
				curatr = unreverse_attr(curatr);
			}
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
		case '\x90': // Black
			set_color(BLACK);
			return true;
		case '\x95': // Brown
			set_color(BROWN);
			return true;
		case '\x96': // Pink
			set_color(LIGHTRED);
			return true;
		case '\x97': // Dark gray
			set_color(DARKGRAY);
			return true;
		case '\x98': // Gray
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
