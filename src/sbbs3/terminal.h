#ifndef TERMINAL_H
#define TERMINAL_H

#include "sbbs.h"

class Terminal {
public:
	unsigned supports;

private:
	sbbs_t& sbbs;
	link_list_t mouse_hotspots{};

public:
	Terminal() = delete;
	Terminal(sbbs_t &sbbs_ref) : sbbs{sbbs_ref}
	{
		listInit(&mouse_hotspots, 0);
	}

	virtual ~Terminal()
	{
		listFree(&mouse_hotspots);
	}

	// Was ansi()
	virtual const char *attrstr(int atr);
	// Was ansi() and ansi_attr()
	virtual char* attrstr(int atr, int curatr, char* str, size_t strsz);
	virtual bool getdims();
	virtual bool getxy(int* x, int* y);
	virtual bool gotoxy(int x, int y);
	// Was ansi_save
	virtual bool save_cursor_pos();
	// Was ansi_restore
	virtual bool restore_cursor_pos();
	virtual void clearscreen();
	virtual void cleartoeos();
	virtual void cleartoeol();
	virtual void cursor_home();
	virtual void cursor_up();
	virtual void cursor_down();
	virtual void cursor_right();
	virtual void cursor_left();
	virtual void set_output_rate();
	// TODO: backfill?
	// Was term_rows
	virtual char* rows(user_t *user, char *str, size_t *size);
	// Was term_cols
	virtual char* cols(user_t *user, char *str, size_t *size);
	// Not a complete replacement for term_type
	virtual char* type(char* str, size_t size);
	virtual const char* type();
	virtual int set_mouse(int mode);
	virtual void parse_outchar(char ch);
	// Needs to handle C0 and C1
	virtual bool parse_ctrlkey(char ch, int mode);
	virtual struct mouse_hotspot* add_hotspot(struct mouse_hotspot* spot);

	void clear_hotspots(void);
	void scroll_hotspots(int count);
	struct mouse_hotspot* add_hotspot(char cmd, bool hungry, int minx, int maxx, int y);
	struct mouse_hotspot* add_hotspot(int num, bool hungry, int minx, int maxx, int y);
	struct mouse_hotspot* add_hotspot(uint num, bool hungry, int minx, int maxx, int y);
	struct mouse_hotspot* add_hotspot(const char* cmd, bool hungry, int minx, int maxx, int y);
};

#endif
