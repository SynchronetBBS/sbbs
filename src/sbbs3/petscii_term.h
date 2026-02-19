#ifndef PETSCII_TERMINAL_H
#define PETSCII_TERMINAL_H

#include "sbbs.h"

class PETSCII_Terminal : public Terminal {
private:
	void set_color(int c);
	unsigned xlat_atr(unsigned atr);

	unsigned saved_x{0};
	unsigned saved_y{0};
	// https://www.pagetable.com/c64ref/charset/
	enum {
		PETSCII_C64,
		PETSCII_C128_80
	} subset{PETSCII_C64};

public:

	PETSCII_Terminal() = delete;
	using Terminal::Terminal;

	virtual const char *attrstr(unsigned atr);
	virtual char* attrstr(unsigned atr, unsigned curatr, char* str, size_t strsz);
	virtual bool gotoxy(unsigned x, unsigned y);
	virtual bool save_cursor_pos();
	virtual bool restore_cursor_pos();
	virtual void carriage_return();
	virtual void line_feed(unsigned count = 1);
	virtual void backspace(unsigned int count = 1);
	virtual void newline(unsigned count = 1, bool no_bg_attr = false);
	virtual void clearscreen();
	virtual void cleartoeos();
	virtual void cleartoeol();
	virtual void clearline();
	virtual void cursor_home();
	virtual void cursor_up(unsigned count = 1);
	virtual void cursor_down(unsigned count = 1);
	virtual void cursor_right(unsigned count = 1);
	virtual void cursor_left(unsigned count = 1);
	virtual const char* type();
	virtual bool parse_output(char ch);
	virtual bool can_highlight();
	virtual bool can_move();
	virtual bool is_monochrome();
	virtual void updated();
};

#endif
