#ifndef TERMINAL_H
#define TERMINAL_H

#include "sbbs.h"
#include "utf8.h"
#include "unicode.h"

struct mouse_hotspot {          // Mouse hot-spot
	char     cmd[128];
	unsigned y;
	unsigned minx;
	unsigned maxx;
	bool     hungry;
};

struct savedline {
	char buf[LINE_BUFSIZE + 1];     /* Line buffer (i.e. ANSI-encoded) */
	uint beg_attr;                  /* Starting attribute of each line */
	uint end_attr;                  /* Ending attribute of each line */
	int column;                     /* Current column number */
};

enum output_rate {
	output_rate_unlimited,
	output_rate_300 = 300,
	output_rate_600 = 600,
	output_rate_1200 = 1200,
	output_rate_2400 = 2400,
	output_rate_4800 = 4800,
	output_rate_9600 = 9600,
	output_rate_19200 = 19200,
	output_rate_38400 = 38400,
	output_rate_57600 = 57600,
	output_rate_76800 = 76800,
	output_rate_115200 = 115200,
};

// Terminal mouse reporting mode (mouse_mode)
#define MOUSE_MODE_OFF  0       // No terminal mouse reporting enabled/expected
#define MOUSE_MODE_X10  (1<<0)  // X10 compatible mouse reporting enabled
#define MOUSE_MODE_NORM (1<<1)  // Normal tracking mode mouse reporting
#define MOUSE_MODE_BTN  (1<<2)  // Button-event tracking mode mouse reporting
#define MOUSE_MODE_ANY  (1<<3)  // Any-event tracking mode mouse reporting
#define MOUSE_MODE_EXT  (1<<4)  // SGR-encoded extended coordinate mouse reporting
#define MOUSE_MODE_ON   (MOUSE_MODE_NORM | MOUSE_MODE_EXT) // Default mouse "enabled" mode flags

class Terminal {
public:
	uint32_t flags{0};                 /* user.misc flags that impact the terminal */
	unsigned row{0};                   /* Current row */
	unsigned column{0};                /* Current column counter (for line counter) */
	unsigned rows{0};                  /* Current number of Rows for User */
	unsigned cols{0};                  /* Current number of Columns for User */
	unsigned tabstop{8};               /* Current symmetric-tabstop (size) */
	unsigned lastlinelen{0};           /* The previously displayed line length */
	unsigned cterm_version{0};	   /* (MajorVer*1000) + MinorVer */
	unsigned lncntr{0};                /* Line Counter - for PAUSE */
	unsigned latr{LIGHTGRAY};          /* Starting attribute of line buffer */
	unsigned curatr{LIGHTGRAY};        /* Current Text Attributes Always */
	unsigned lbuflen{0};               /* Number of characters in line buffer */
	char     lbuf[LINE_BUFSIZE + 1]{}; /* Temp storage for each line output */
	enum output_rate cur_output_rate{output_rate_unlimited};
	unsigned mouse_mode{MOUSE_MODE_OFF};            // Mouse reporting mode flags
	bool pause_hotspot{false};
	link_list_t *mouse_hotspots{nullptr};

protected:
	sbbs_t* sbbs;

private:
	link_list_t *savedlines{nullptr};

public:

	static uint32_t get_flags(sbbs_t *sbbsptr) {
		uint32_t flags;

		if ((sbbsptr->sys_status & (SS_USERON | SS_NEWUSER)) && (sbbsptr->useron.misc & AUTOTERM)) {
			flags = sbbsptr->autoterm;
			flags |= sbbsptr->useron.misc & (NO_EXASCII | SWAP_DELETE | COLOR | ICE_COLOR | MOUSE);
		}
		else {
			flags = sbbsptr->useron.misc;
		}
		flags &= TERM_FLAGS;
		// TODO: Get rows and cols
		return flags;
	}

	Terminal() = delete;
	Terminal(sbbs_t *sbbsptr) : flags{get_flags(sbbsptr)}, mouse_hotspots{listInit(nullptr, 0)}, sbbs{sbbsptr},
	    savedlines{listInit(nullptr, 0)} {}

	Terminal(Terminal *t) : flags{get_flags(t->sbbs)}, row{t->row}, column{t->column},
	    rows{t->rows}, cols{t->cols}, tabstop{t->tabstop}, lastlinelen{t->lastlinelen}, 
	    cterm_version{t->cterm_version}, lncntr{t->lncntr}, latr{t->latr}, curatr{t->curatr},
	    lbuflen{t->lbuflen}, mouse_mode{t->mouse_mode}, pause_hotspot{t->pause_hotspot},
	    mouse_hotspots{t->mouse_hotspots}, sbbs{t->sbbs}, savedlines{t->savedlines} {
		// Take ownership of lists so they're not destroyed
		t->mouse_hotspots = nullptr;
		t->savedlines = nullptr;
		// Copy line buffer
		memcpy(lbuf, t->lbuf, sizeof(lbuf));
		// TODO: This is pretty hacky...
		//       Calls the old set_mouse() with the current flags
		//       and mode.  We can't call the new one because it's
		//       virtual and we aren't constructed yet.
		//       Ideally this would disable the mouse if !supports(MOUSE)
		t->flags = flags;
		t->set_mouse(mouse_mode);
	}

	virtual ~Terminal()
	{
		listFree(mouse_hotspots);
		listFree(savedlines);
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
		if (x)
			*x = column + 1;
		if (y)
			*y = row + 1;
		return true;
	}

	// TODO: Pick one.
	virtual bool cursor_getxy(unsigned *x, unsigned *y) {
		return getxy(x, y);
	}

	virtual bool gotoxy(unsigned x, unsigned y) {
		return false;
	}

	// TODO: Pick one. :D
	virtual bool cursor_xy(unsigned x, unsigned y) {
		return gotoxy(x, y);
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
		sbbs->outcom('\r');
		column = 0;
	}

	virtual void line_feed(unsigned count = 1) {
		for (unsigned i = 0; i < count; i++)
			sbbs->outcom('\n');
		inc_row(count);
	}

	/*
	 * Destructive backspace.
	 * TODO: This seems to be the only one of this family that
	 *       checks CON_ECHO_OFF itself.  Figure out why and either
	 *       remove it, or add it to all the rest that call outcom()
	 */
	virtual void backspace(unsigned int count = 1) {
		if (sbbs->console & CON_ECHO_OFF)
			return;
		for (unsigned i = 0; i < count; i++) {
			if (column > 0) {
				sbbs->outcom('\b');
				sbbs->outcom(' ');
				sbbs->outcom('\b');
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
		sbbs->outcom(FF);
		row = 0;
		column = 0;
		lncntr = 0;
		clear_hotspots();
	}

	virtual void cleartoeos() {}
	virtual void cleartoeol() {}
	virtual void clearline() {
		carriage_return();
		cleartoeol();
	}

	virtual void cursor_home() {}
	virtual void cursor_up(unsigned count = 1) {}
	virtual void cursor_down(unsigned count = 1) {
		line_feed(count);
	}

	virtual void cursor_right(unsigned count = 1) {}
	virtual void cursor_left(unsigned count = 1) {
		for (unsigned i = 0; i < count; i++) {
			if (column > 0) {
				sbbs->outcom('\b');
				column--;
			}
			else
				break;
		}
	}
	virtual void set_output_rate(enum output_rate speed) {}
	virtual void center(const char *instr, bool msg = false, unsigned columns = 0) {
		if (columns == 0)
			columns = cols;
		char *str = strdup(str);
		truncsp(str);
		size_t len = bstrlen(str);
		carriage_return();
		if (len < columns)
			cursor_right((columns - len) / 2);
		if (msg)
			sbbs->putmsg(str, P_NONE);
		else
			sbbs->bputs(str);
		free(str);
		newline();
	}

	/****************************************************************************/
	/* Returns the printed columns from 'str' accounting for Ctrl-A codes       */
	/****************************************************************************/
	virtual size_t bstrlen(const char *str, int mode = 0)
	{
		str = sbbs->auto_utf8(str, mode);
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
				count += unicode_width(codepoint, sbbs->unicode_zerowidth);
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

	virtual void set_mouse(unsigned flags) {}

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
					while ((column < (cols - 1)) && (column % tabstop)) {
						sbbs->outcom(' ');
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

	virtual void insert_indicator() {};

	virtual bool saveline() {
		struct savedline line;
		line.beg_attr = latr;
		line.end_attr = curatr;
		line.column = column;
		snprintf(line.buf, sizeof(line.buf), "%.*s", lbuflen, lbuf);
		TERMINATE(line.buf);
		lbuflen = 0;
		return listPushNodeData(savedlines, &line, sizeof(line)) != NULL;
	}

	virtual bool restoreline() {
		struct savedline* line = (struct savedline*)listPopNode(savedlines);
		if (line == NULL)
			return false;
		lbuflen = 0;
		attrstr(line->beg_attr);
		sbbs->rputs(line->buf);
		// TODO: Why would this do anything?
		//if (supports(PETSCII))
		//	column = strlen(line->buf);
		curatr = line->end_attr;
		carriage_return();
		cursor_right(line->column);
		free(line);
		insert_indicator();
		return true;
	}

	void clear_hotspots(void);
	void scroll_hotspots(unsigned count);

	struct mouse_hotspot* add_hotspot(struct mouse_hotspot* spot);
	struct mouse_hotspot* add_hotspot(char cmd, bool hungry = true, int minx = -1, int maxx = -1, int y = -1);
	struct mouse_hotspot* add_hotspot(int num, bool hungry = true, int minx = -1, int maxx = -1, int y = -1);
	struct mouse_hotspot* add_hotspot(uint num, bool hungry = true, int minx = -1, int maxx = -1, int y = -1);
	struct mouse_hotspot* add_hotspot(const char* cmd, bool hungry = true, int minx = -1, int maxx = -1, int y = -1);
	bool add_pause_hotspot(char cmd);
	void inc_row(unsigned count = 1);
	void inc_column(unsigned count = 1);
	void cond_newline();
	void cond_blankline();
	void cond_contline();
	bool supports(unsigned cmp_flags);
	list_node_t *find_hotspot(int x, int y);
};

void update_terminal(sbbs_t *sbbsptr);

#endif
