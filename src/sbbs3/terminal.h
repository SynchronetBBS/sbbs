#ifndef TERMINAL_H
#define TERMINAL_H

#include "sbbs.h"

struct mouse_hotspot {          // Mouse hot-spot
	char     cmd[128];
	unsigned y;
	unsigned minx;
	unsigned maxx;
	bool     hungry;
};

class Terminal {
public:
	uint32_t supports;
	unsigned row;
	unsigned column;
	unsigned rows;
	unsigned cols;
	unsigned tabstop;
	unsigned lastlinelen;
	unsigned cterm_version;

protected:
	sbbs_t& sbbs;

private:
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
	virtual const char *attrstr(unsigned atr) {
		return "";
	}

	// Was ansi() and ansi_attr()
	virtual char* attrstr(unsigned atr, unsigned curatr, char* str, size_t strsz) {
		if (strsz > 0)
			str[0] = 0;
		return str;
	}

	virtual bool getdims() {
		return false;
	}

	virtual bool getxy(unsigned* x, unsigned* y) {
		return false;
	}

	virtual bool gotoxy(unsigned x, unsigned y) {
		return false;
	}

	// Was ansi_save
	virtual bool save_cursor_pos() {
		return false;
	}

	// Was ansi_restore
	virtual bool restore_cursor_pos() {
		return false;
	}

	virtual void clearscreen() {}
	virtual void cleartoeos() {}
	virtual void cleartoeol() {}
	virtual void cursor_home() {}
	virtual void cursor_up(unsigned count) {}
	virtual void cursor_down(unsigned count) {}
	virtual void cursor_right(unsigned count) {}
	virtual void cursor_left(unsigned count) {}
	virtual void set_output_rate() {}

	// TODO: backfill?
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
	void inc_row(int count);
};

#endif
