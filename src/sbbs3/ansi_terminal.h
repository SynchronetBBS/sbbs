#ifndef ANSI_TERMINAL_H
#define ANSI_TERMINAL_H

#include <string>
#include "ansi_parser.h"
#include "terminal.h"

class ANSI_Terminal : public Terminal {
public:
	ANSI_Terminal() = delete;
	using Terminal::Terminal;

	// Was ansi()
	virtual const char *attrstr(unsigned atr);
	// Was ansi() and ansi_attr()
	virtual char* attrstr(unsigned atr, unsigned curatr, char* str, size_t strsz);
	virtual bool getdims();
	virtual bool getxy(unsigned* x, unsigned* y);
	virtual bool gotoxy(unsigned x, unsigned y);
	// Was ansi_save
	virtual bool save_cursor_pos();
	// Was ansi_restore
	virtual bool restore_cursor_pos();
	virtual void clearscreen();
	virtual void cleartoeos();
	virtual void cleartoeol();
	virtual void cursor_home();
	virtual void cursor_up(unsigned count);
	virtual void cursor_down(unsigned count);
	virtual void cursor_right(unsigned count);
	virtual void cursor_left(unsigned count);
	virtual void set_output_rate(enum output_rate speed);
	// TODO: backfill?
	virtual const char* type();
	virtual void set_mouse(unsigned mode);
	virtual bool parse_output(char ch);
	// Needs to handle C0 and C1
	virtual bool parse_input_sequence(char& ch, int mode);
	virtual struct mouse_hotspot* add_hotspot(struct mouse_hotspot* spot);
	virtual bool can_highlight();
	virtual bool can_move();
	virtual bool can_mouse();
	virtual bool is_monochrome();

private:
	void handle_control_code();
	void handle_control_sequence();
	void handle_SGR_sequence();
	bool stuff_unhandled(char &ch, ANSI_Parser& ansi);
	bool stuff_str(char& ch, const char *str, bool skipctlcheck = false);
	bool handle_non_SGR_mouse_sequence(char& ch, ANSI_Parser& ansi);
	bool handle_SGR_mouse_sequence(char& ch, ANSI_Parser& ansi, bool release);
	bool handle_left_press(unsigned x, unsigned y, char& ch, bool& retval);

	unsigned saved_row{0};
	unsigned saved_column{0};
	bool is_negative{0};
	uint8_t utf8_remain{0};
	bool first_continuation{false};
	uint32_t codepoint{0};
	ANSI_Parser ansiParser{};
};

#endif
