#ifndef ANSI_TERMINAL_H
#define ANSI_TERMINAL_H

#include "terminal.h"

class ANSI_Terminal : public Terminal {
public:
	ANSI_Terminal() = delete;

	// Was ansi()
	const char *attrstr(int atr);
	// Was ansi() and ansi_attr()
	char* attrstr(int atr, int curatr, char* str, size_t strsz);
	bool getdims();
	bool getxy(int* x, int* y);
	bool gotoxy(int x, int y);
	// Was ansi_save
	bool save_cursor_pos();
	// Was ansi_restore
	bool restore_cursor_pos();
	void clearscreen();
	void cleartoeos();
	void cleartoeol();
	void cursor_home();
	void cursor_up(unsigned count);
	void cursor_down(unsigned count);
	void cursor_right(unsigned count);
	void cursor_left(unsigned count);
	void set_output_rate();
	// TODO: backfill?
	// Was term_rows
	char* rows(user_t *user, char *str, size_t *size);
	// Was term_cols
	char* cols(user_t *user, char *str, size_t *size);
	// Not a complete replacement for term_type
	char* type(char* str, size_t size);
	const char* type();
	int set_mouse(int mode);
	void parse_outchar(char ch);
	// Needs to handle C0 and C1
	bool parse_ctrlkey(char ch, int mode);
	struct mouse_hotspot* add_hotspot(struct mouse_hotspot* spot);
};

#endif
