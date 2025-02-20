#ifndef ANSI_TERMINAL_H
#define ANSI_TERMINAL_H

#include <string>
#include "terminal.h"

enum ansiState {
	 ansiState_none         // No sequence
	,ansiState_esc          // Escape
	,ansiState_csi          // CSI
	,ansiState_intermediate // Intermediate byte
	,ansiState_final        // Final byte
	,ansiState_string       // APS, DCS, PM, or OSC
	,ansiState_sos          // SOS
	,ansiState_sos_esc      // ESC inside SOS
};

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
	virtual bool parse_outchar(char ch);
	// Needs to handle C0 and C1
	virtual bool parse_ctrlkey(char& ch, int mode);
	virtual void insert_indicator();
	virtual struct mouse_hotspot* add_hotspot(struct mouse_hotspot* spot);
	virtual bool can_highlight();
	virtual bool can_move();
	virtual bool can_mouse();
	virtual bool is_monochrome();

private:
	enum ansiState outchar_esc{ansiState_none}; // track ANSI escape seq output
	std::string ansi_params{""};
	std::string ansi_ibs{""};
	std::string ansi_sequence{""};
	bool ansi_was_cc{false};
	bool ansi_was_string{false};
	char ansi_final_byte{0};
	bool ansi_was_private{false};
	unsigned saved_row{0};
	unsigned saved_column{0};
	bool is_negative{0};
	uint8_t utf8_remain{0};
	bool first_continuation{false};
};

#endif
