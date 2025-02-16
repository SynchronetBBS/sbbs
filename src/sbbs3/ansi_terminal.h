#ifndef ANSI_TERMINAL_H
#define ANSI_TERMINAL_H

#include "terminal.h"

enum ansiState {
	 ansiState_none         // No sequence
	,ansiState_esc          // Escape
	,ansiState_csi          // CSI
	,ansiState_final        // Final byte
	,ansiState_string       // APS, DCS, PM, or OSC
	,ansiState_sos          // SOS
	,ansiState_sos_esc      // ESC inside SOS
};

class ANSI_Terminal : public Terminal {
public:
	ANSI_Terminal() = delete;

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
	virtual struct mouse_hotspot* add_hotspot(struct mouse_hotspot* spot);

private:
	enum ansiState outchar_esc{ansiState_none}; // track ANSI escape seq output
};

#endif
