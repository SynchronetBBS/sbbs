#include "mode7_terminal.h"
#include "mode7defs.h"

// Initial work is only C64 and C128 only, not C65, X16, C116, C16, Plus/4, or VIC-20, and certainly not a PET

const char *MODE7_Terminal::attrstr(unsigned atr)
{
	switch (atr) {

		/* Special case */
		case ANSI_NORMAL:
			return "\x9C\x87\x89\x8C";	// Black Background, Alpha White, Steady, Single Height
		case BLINK:
		case BG_BRIGHT:
			return "";

		/* Foreground */
		case HIGH:
			return "";			// Only one intensity
		case BLACK:
			return "";			// Not possible in Mode7
		case RED:
			return "\x81";
		case GREEN:
			return "\x82";
		case BROWN:
			return "\x83";
		case BLUE:
			return "\x84";
		case MAGENTA:
			return "\x85";
		case CYAN:
			return "\x86";
		case LIGHTGRAY:
			return "\x87";

		/* Background */
		case BG_BLACK:
			return "\x9C";			// Reset background
		case BG_RED:
			return "";			// Not possible with static char
		case BG_GREEN:
			return "";			// Not possible with static char
		case BG_BROWN:
			return "";			// Not possible with static char
		case BG_BLUE:
			return "";			// Not possible with static char
		case BG_MAGENTA:
			return "";			// Not possible with static char
		case BG_CYAN:
			return "";			// Not possible with static char
		case BG_LIGHTGRAY:
			return "";			// Not possible with static char
	}

	return "-Invalid use of ansi()-";
}

/*
 * Convert from "full" attribute to somethig possible
 */
unsigned
MODE7_Terminal::xlat_atr(unsigned atr)
{
	unsigned ret = atr;

	if (atr & FG_UNKNOWN)
		ret |= 7;
	if (atr & BG_UNKNOWN)
		ret &= ~0x70;
	ret &= ~(FG_UNKNOWN | BG_UNKNOWN | UNDERLINE | REVERSED | BG_BRIGHT | HIGH);
	// Black foreground not possible... choose the darkest different (blue or red)
	if (!(ret & 7)) {
		if (((ret & 0x70) >> 4) != BLUE)
			ret |= (BLUE);
		else
			ret |= (RED);
	}
	return ret;
}

char MODE7_Terminal::color_char(unsigned c, bool bg)
{
	if (bg)
		c >>= 4;
	c &= 7;

	if (!c)
		sbbs->lprintf(LOG_DEBUG, "Attempting to set Mode7 black %s attribute", bg ? "background" : "foreground");
	switch (c) {
		case BLACK:
			return 0x80;
		case BLUE:
			return MODE7_ALPHA_BLUE;
		case GREEN:
			return MODE7_ALPHA_GREEN;
		case CYAN:
			return MODE7_ALPHA_CYAN;
		case RED:
			return MODE7_ALPHA_RED;
		case MAGENTA:
			return MODE7_ALPHA_MAGENTA;
		case BROWN:
			return MODE7_ALPHA_YELLOW;
		case LIGHTGRAY:
			return MODE7_ALPHA_WHITE;
	}
	sbbs->lprintf(LOG_DEBUG, "Unhandle %s colour %02x", bg ? "background" : "foreground", c);
	return 0x80;
}

size_t
MODE7_Terminal::yummy_spaces()
{
	unsigned x;
	size_t ret = 0;


	if (column == 0)
		return 0;
	for (x = column; x > 0; x--) {
		if (cell[row][x - 1] != ' ' && cell[row][x - 1] != '\xa0')
			return ret;
		ret++;
	}
	return ret;
}

char* MODE7_Terminal::attrstr(unsigned atr, unsigned curatr, char* str, size_t strsz)
{
	size_t lastret = 0;
	unsigned newatr = xlat_atr(atr);
	unsigned oldatr = xlat_atr(curatr);
	char src[2] = {0,0};

	if (strsz == 0) {
		sbbs->lprintf(LOG_DEBUG, "Zero string size in attrstr");
		return str;
	}
	str[0] = 0;

	// Blink first because we don't support hold mosaic...
	if (newatr & BLINK) {
		if (!(oldatr & BLINK))
			lastret = strlcat(str, "\x88", strsz);
	}
	else {
		if (oldatr & BLINK)
			lastret = strlcat(str, "\x89", strsz);
	}

	// Then concealed
	if (newatr & CONCEALED) {
		if (!(oldatr & CONCEALED)) {
			lastret = strlcat(str, "\x98", strsz);
			// Set concealed so we can include in FG check
			// concealed is cleared by setting foreground
			oldatr |= CONCEALED;
		}
	}

	// Background before FG so text is visible
	if ((newatr & 0x70) != (oldatr & 0x70)) {
		if (newatr & 0x70) {
			if ((newatr & 0x70) != ((oldatr & 0x07) << 4)) {
				src[0] = color_char(newatr, true);
				lastret = strlcat(str, src, strsz);
				oldatr = (oldatr & ~0x77) | (newatr & 0x70) | ((newatr & 0x70) >> 7);
			}
			src[0] = '\x9d';
			lastret = strlcat(str, src, strsz);
		}
		else
			lastret = strlcat(str, "\x9C", strsz);
	}

	// Then FG colour last
	if ((newatr & (CONCEALED | 7)) != (oldatr & (CONCEALED | 7))) {
		src[0] = color_char(newatr, false);
		lastret = strlcat(str, src, strsz);
	}

	if (lastret >= (strsz - 1))
		sbbs->lprintf(LOG_ERR, "String size (%zu) too small", strsz);
	else {
		size_t ys = yummy_spaces();
		if (ys > lastret)
			ys = lastret;
		if (ys > (strsz - lastret - 1))
			ys = strsz - lastret - 1;
		memmove(&str[ys], &str[0], lastret + 1);
		for (size_t i = 0; i < ys; i++)
			str[i] = '\x08';
	}

	return str;
}

bool MODE7_Terminal::gotoxy(unsigned x, unsigned y)
{
	char str[3] = {'\x1C'};
	str[1] = 32 + x;
	str[2] = 32 + y;
	sbbs->term_out(str, 3);
	return true;
}

// Was ansi_save
bool MODE7_Terminal::save_cursor_pos() {
	saved_x = column + 1;
	saved_y = row + 1;
	return true;
}

// Was ansi_restore
bool MODE7_Terminal::restore_cursor_pos() {
	if (saved_x && saved_y) {
		return gotoxy(saved_x, saved_y);
	}
	return false;
}

void MODE7_Terminal::carriage_return()
{
	sbbs->term_out('\x0d');	// APR
}

void MODE7_Terminal::line_feed(unsigned count)
{
	// Like cursor_down() but scrolls...
	for (unsigned i = 0; i < count; i++)
		sbbs->term_out('\x0a'); // APD
}

void MODE7_Terminal::backspace(unsigned int count)
{
	for (unsigned i = 0; i < count; i++) {
		if (column == 0)
			break;
		sbbs->term_out("\x08 \x08");
	}
}

void MODE7_Terminal::newline(unsigned count)
{
	for (unsigned i = 0; i < count; i++)
		sbbs->term_out("\0d\x0a");
}

void MODE7_Terminal::clearscreen()
{
	clear_hotspots();
	sbbs->term_out(MODE7_CLEAR);
}

void MODE7_Terminal::cleartoeos()
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

void MODE7_Terminal::cleartoeol()
{
	unsigned sc = column;
	while ((column < (cols - 1)) && sbbs->online)
		sbbs->term_out(' ');
	while (sc < column && sbbs->online)
		sbbs->term_out(MODE7_LEFT);
}

void MODE7_Terminal::clearline()
{
	int c = column;
	carriage_return();
	cleartoeol();
	cursor_right(c);
}

void MODE7_Terminal::cursor_home()
{
	sbbs->term_out(MODE7_HOME);
}

void MODE7_Terminal::cursor_up(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		if (row)
			sbbs->term_out(MODE7_UP);
	}
}

void MODE7_Terminal::cursor_down(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		if (row >= (rows - 1))
			break;
		sbbs->term_out(MODE7_DOWN);
	}
}

void MODE7_Terminal::cursor_right(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		if (column >= (cols - 1))
			break;
		sbbs->term_out(MODE7_RIGHT);
	}
}

void MODE7_Terminal::cursor_left(unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		sbbs->term_out(MODE7_LEFT);
		if (column == 0)
			break;
	}
}

const char* MODE7_Terminal::type()
{
	return "MODE7";
}

void MODE7_Terminal::set_fg(int c)
{
	curatr &= ~ALT_CHARSET1;
	curatr &= ~7;
	curatr |= c;
}

void MODE7_Terminal::update_cur()
{
	curatr = 0x07;
	if (column < 1)
		return;
	for (size_t x = 0; x < column; x++) {
		switch (cell[row][x]) {
			case '\x81':	// ANR
				set_fg(RED);
				break;
			case '\x82':	// ANG
				set_fg(GREEN);
				break;
			case '\x83':	// ANY
				set_fg(BROWN);
				break;
			case '\x84':	// ANB
				set_fg(BLUE);
				break;
			case '\x85':	// ANM
				set_fg(MAGENTA);
				break;
			case '\x86':	// ANC
				set_fg(CYAN);
				break;
			case '\x87':	// ANW
				set_fg(LIGHTGRAY);
				break;
			case '\x88':	// FSH
				curatr |= BLINK;
				break;
			case '\x89':	// STD
				curatr &= ~BLINK;
				break;
			case '\x8C':	// NSZ
				curatr &= ~DBL_HEIGHT;
				break;
			case '\x8D':	// DBH
				curatr |= DBL_HEIGHT;
				break;
			case '\x8E':	// DBW
				break;
			case '\x8F':	// DBS
				break;
			case '\x90':	// MBK
				break;
			case '\x91':	// MSR
				set_fg(RED);
				curatr |= ALT_CHARSET1;
				break;
			case '\x92':	// MSG
				set_fg(GREEN);
				curatr |= ALT_CHARSET1;
				break;
			case '\x93':	// MSY
				set_fg(YELLOW);
				curatr |= ALT_CHARSET1;
				break;
			case '\x94':	// MSB
				set_fg(BLUE);
				curatr |= ALT_CHARSET1;
				break;
			case '\x95':	// MSM
				set_fg(MAGENTA);
				curatr |= ALT_CHARSET1;
				break;
			case '\x96':	// MSC
				set_fg(CYAN);
				curatr |= ALT_CHARSET1;
				break;
			case '\x97':	// MSW
				set_fg(WHITE);
				curatr |= ALT_CHARSET1;
				break;
			case '\x98':	// CDY
				curatr |= CONCEALED;
				break;
			case '\x9C':	// BBD
				curatr &= ~0x70;
				break;
			case '\x9D':	// NBD
				curatr = (curatr & ~0x70) | ((curatr & 7) << 4);
				break;
			case '\x9E':	// HMS
				curatr |= TT_SPECIAL;
				break;
			case '\x9F':	// RMS
				curatr &= ~TT_SPECIAL;
				break;
		}
	}
}

void MODE7_Terminal::scrollup()
{
	for (size_t i = 0; i < (rows - 1); i++)
		memcpy(cell[i], cell[i+1], sizeof(cell[i]));
	memset(&cell[rows - 1], ' ', sizeof(cell[0]));
}

void MODE7_Terminal::scrolldown()
{
	for (size_t i = 1; i < rows; i++)
		memcpy(cell[i - 1], cell[i], sizeof(cell[i]));
	memset(&cell[0], ' ', sizeof(cell[0]));
}

void MODE7_Terminal::add_ch(char ch)
{
	cell[row][column] = ch;
	inc_column();
	update_cur();
}

bool MODE7_Terminal::parse_output(char ch)
{
	switch (parse_aps) {
		case 1:
			set_column((ch & 0x7F) - 32);
			parse_aps++;
			return true;
		case 2:
			set_row((ch & 0x7F) - 32);
			parse_aps = 0;
			return true;
		default:
			break;
	}
	if (last_was_esc) {
		if (ch < '@' || ch > 0x5F) {
			sbbs->lprintf(LOG_DEBUG, "Invalid code extension: ESC + \\x%02X", (uchar)ch);
			if (ch != '\x1b')
				last_was_esc = false;
			return false;
		}
		ch += 64;
	}
	if (ch == '\x1b') {
		last_was_esc = true;
		return true;
	}
	else
		last_was_esc = false;

	switch (ch) {
		// Zero-width characters we likely shouldn't send
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 14:
		case 15:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:	// Shouldn't be possible...
		case 0x1D:
		case 0x1F:
			return false;

		// Specials that affect cursor position
		case '\x08':	// APB
			// TODO: Wraps?  Scrolls up?
			if (column == 0) {
				if (row > 0) {
					dec_row();
					set_column(cols - 1);
				}
			}
			dec_column();
			return true;
		case '\x09':	// APF
			inc_column();
			return true;
		case '\x0A':	// APD
			if (row == 0)
				scrolldown();
			inc_row();
			return true;
		case '\x0B':	// APU
			if (row == 0)
				scrollup();
			dec_row();
			return true;
		case '\x0C':	// CS
			set_row();
			set_column();
			return true;
		case '\x0D':	// APR
			set_column();
			return true;
		case '\x1C':	// APS
			parse_aps = 1;
			return true;
		case '\x1E':	// APH
			set_row();
			set_column();
			return true;

		// Zero-width characters we want to pass through
		case '\x07':	// BEL
			return true;

		// Everything else is assumed one cell wide
		default:
			add_ch(ch);
			return true;
	}
	return false;
}

bool MODE7_Terminal::can_highlight() { return true; }
bool MODE7_Terminal::can_move() { return true; }
bool MODE7_Terminal::is_monochrome() { return false; }
