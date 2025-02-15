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
	uint32_t supports{0};
	unsigned row{0};
	unsigned column{0};
	unsigned rows{0};
	unsigned cols{0};
	unsigned tabstop{8};
	unsigned lastlinelen{0};
	unsigned cterm_version{0};	/* (MajorVer*1000) + MinorVer */
	unsigned lncntr{0};

protected:
	sbbs_t& sbbs;

private:
	link_list_t *mouse_hotspots{nullptr};

public:
	Terminal() = delete;
	Terminal(sbbs_t &sbbs_ref) : sbbs{sbbs_ref}, mouse_hotspots{listInit(nullptr, 0)} {}

	Terminal(Terminal& t) : supports{t.supports}, row{t.row}, column{t.column},
	    rows{t.rows}, cols{t.cols}, tabstop{t.tabstop}, lastlinelen{t.lastlinelen}, 
	    cterm_version{t.cterm_version}, lncntr{t.lncntr}, sbbs{t.sbbs},
	    mouse_hotspots{t.mouse_hotspots} {
		// Take ownership of mouse_hotspots so they're not destroyed
		t.mouse_hotspots = nullptr;
	}

	virtual ~Terminal()
	{
		listFree(mouse_hotspots);
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

	virtual void carriage_return() {
		sbbs.outcom('\r');
		column = 0;
	}

	virtual void line_feed(unsigned count = 1) {
		for (unsigned i = 0; i < count; i++)
			sbbs.outcom('\n');
		inc_row(count);
	}

	/*
	 * Destructive backspace.
	 * TODO: This seems to be the only one of this family that
	 *       checks CON_ECHO_OFF itself.  Figure out why and either
	 *       remove it, or add it to all the rest that call outcom()
	 */
	virtual void backspace(unsigned int count = 1) {
		if (sbbs.console & CON_ECHO_OFF)
			return;
		for (unsigned i = 0; i < count; i++) {
			if (column > 0) {
				sbbs.outcom('\b');
				sbbs.outcom(' ');
				sbbs.outcom('\b');
				column--;
			}
			else
				break;
		}
	}

	virtual void newline(unsigned count = 1) {
		// TODO: Original version did not increment row or lncntr
		//       It recursed through outchar()
		for (unsigned i = 0; i < count; i++) {
			carriage_return();
			line_feed();
		}
	}

	virtual void clearscreen() {
		sbbs.outcom(FF);
		row = 0;
		column = 0;
		lncntr = 0;
		clear_hotspots();
	}

	virtual void cleartoeos() {}
	virtual void cleartoeol() {}
	virtual void cursor_home() {}
	virtual void cursor_up(unsigned count = 1) {}
	virtual void cursor_down(unsigned count = 1) {
		line_feed(count);
	}

	virtual void cursor_right(unsigned count = 1) {}
	virtual void cursor_left(unsigned count = 1) {
		for (unsigned i = 0; i < count; i++) {
			if (column > 0) {
				sbbs.outcom('\b');
				column--;
			}
			else
				break;
		}
	}
	virtual void set_output_rate() {}
	virtual void center(const char *instr, bool msg, unsigned columns) {
		char *str = strdup(str);
		truncsp(str);
		len = bstrlen(str);
		
	}

	/****************************************************************************/
	/* Returns the printed columns from 'str' accounting for Ctrl-A codes       */
	/****************************************************************************/
	virtual size_t bstrlen(const char *str, int mode)
	{
		str = auto_utf8(str, mode);
		size_t      count = 0;
		const char* end = str + strlen(str);
		while (str < end) {
			int len = 1;
			if (*str == CTRL_A) {
				str++;
				if (*str == 0 || *str == 'Z')    // EOF
					break;
				if (*str == '[') // CR
					count = 0;
				else if (*str == '<' && count) // ND-Backspace
					count--;
			} else if (((*str) & 0x80) && (mode & P_UTF8)) {
				enum unicode_codepoint codepoint = UNICODE_UNDEFINED;
				len = utf8_getc(str, end - str, &codepoint);
				if (len < 1)
					break;
				count += unicode_width(codepoint, unicode_zerowidth);
			} else
				count++;
			str += len;
		}
		return count;
	}


	// TODO: backfill?
	virtual const char* type() {
		return "DUMB";
	}

	virtual void set_mouse(int flags) {}

	/*
	 * Returns true if the caller should send the char, false if
	 * this function handled it (ie: via outcom(), or stripping it)
	 */
	virtual bool parse_outchar(char ch) {
		switch (ch) {
			// Zero-width characters we likely shouldn't send
			case 0:  // NUL
			case 1:  // SOH
			case 2:  // STX
			case 3:  // ETX
			case 4:  // EOT
			case 5:  // ENQ
			case 6:  // ACK
			case 11: // VT - May be supported... TODO
			case 14: // SO (7-bit) or LS1 (8-bit)
			case 15: // SI (7-bit) or LS0 (8-bit)
			case 16: // DLE
			case 17: // DC1 / XON
			case 18: // DC2
			case 19: // DC3 / XOFF
			case 20: // DC4
			case 21: // NAK
			case 22: // SYN
			case 23: // STB
			case 24: // CAN
			case 25: // EM
			case 26: // SUB
			case 27: // ESC - This one is especially troubling
			case 28: // FS
			case 29: // GS
			case 30: // RS
			case 31: // US
				return false;

			// Zero-width characters we want to pass through
			case 7:  // BEL
				return true;

			// Special values
			case 8:  // BS
				cursor_left();
				return false;
			case 9:  // TAB - Copy pasta... TODO
				// TODO: Original would wrap, this one (hopefully) doesn't.
				if (column < (cols - 1)) {
					column++;
					while ((column < (cols < 1)) && (column % tabstop)) {
						sbbs.outcom(' ');
						inc_column();
					}
				}
				return false;
			case 10: // LF
				line_feed();
				return false;
			case 12: // FF
				clearscreen();
				return false;
			case 13: // CR
				carriage_return();
				return false;

			// Everything else is assumed one byte wide
			default:
				inc_column();
				return true;
		}
		return false;
	}

	/*
	 * Returns true if ch was a control key.  For terminals like
	 * ANSI where characters like ESC are potentially just the start
	 * of a sequence, reads the extra characters via kbincom()
	 * and ungetkey(x, true) if it needs to back out.
	 * 
	 * Will replace ch with a TERM_KEY_* or CTRL_*, value if it
	 * returns true.
	 * 
	 * Note that DEL is considered a control key by this function.
	 */
	virtual bool parse_ctrlkey(char& ch, int mode) {
		if (ch < 32 || ch == 127)
			return true;
		return false;
	}

	virtual struct mouse_hotspot* add_hotspot(struct mouse_hotspot* spot) {return nullptr;}

	void clear_hotspots(void) {}
	void scroll_hotspots(int count) {}
	struct mouse_hotspot* add_hotspot(char cmd, bool hungry, int minx, int maxx, int y) {return nullptr;}
	struct mouse_hotspot* add_hotspot(int num, bool hungry, int minx, int maxx, int y) {return nullptr;}
	struct mouse_hotspot* add_hotspot(uint num, bool hungry, int minx, int maxx, int y) {return nullptr;}
	struct mouse_hotspot* add_hotspot(const char* cmd, bool hungry, int minx, int maxx, int y) {return nullptr;}

	void inc_row(unsigned count = 1) {
		row += count;
		if (row >= rows) {
			scroll_hotspots((row - rows) + 1);
			row = rows - 1;
		}
		lncntr++;
	}

	void inc_column(unsigned count = 1) {
		column += count;
		if (column >= cols) {
			// TODO: The "line" needs to be able to be wider than the screen?
			lastlinelen = column;
		}
		while (column >= cols) {
			sbbs.lbuflen = 0;
			// TODO: This left column at 0 before...
			column -= cols;
			inc_row();
		}
	}
};

#endif
