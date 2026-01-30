#ifndef TERMINAL_H
#define TERMINAL_H

#include "sbbs.h"
#include "utf8.h"
#include "unicode.h"

#ifdef __cplusplus

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

#define HOTSPOT_CURRENT_X UINT_MAX
#define HOTSPOT_CURRENT_Y UINT_MAX

class Terminal {
public:
	unsigned row{0};                   /* Current row */
	unsigned column{80};               /* Current column counter (for line counter) */
	unsigned rows{24};                 /* Current number of Rows for User */
	unsigned cols{0};                  /* Current number of Columns for User */
	unsigned tabstop{8};               /* Current symmetric-tabstop (size) */
	unsigned lastcrcol{0};             /* Column when last CR occured (previously lastlinelen) */
	unsigned cterm_version{0};	   /* (MajorVer*1000) + MinorVer */
	unsigned lncntr{0};                /* Line Counter - for PAUSE */
	unsigned latr{ANSI_NORMAL};        /* Starting attribute of line buffer */
	uint32_t curatr{ANSI_NORMAL};      /* Current Text Attributes Always */
	unsigned lbuflen{0};               /* Number of characters in line buffer */
	char     lbuf[LINE_BUFSIZE + 1]{}; /* Temp storage for each line output */
	enum output_rate cur_output_rate{output_rate_unlimited};
	unsigned mouse_mode{MOUSE_MODE_OFF};            // Mouse reporting mode flags
	bool pause_hotspot{false};
	bool optimize_gotoxy{false};
	bool suspend_lbuf{0};
	link_list_t *mouse_hotspots{nullptr};

protected:
	sbbs_t* sbbs;
	uint32_t flags_{0};                 /* user.misc flags that impact the terminal */

private:
	link_list_t *savedlines{nullptr};
	uint8_t utf8_remain{0};
	bool first_continuation{false};
	uint32_t codepoint{0};

public:

	static uint32_t flags_fixup(uint32_t flags)
	{
		if (flags & UTF8) {
			// These bits are *never* available in UTF8 mode
			// Note that RIP is not inherently incompatible with UTF8
			flags &= ~(NO_EXASCII | PETSCII);
		}

		if (flags & RIP) {
			// ANSI is always available when RIP is
			flags |= ANSI;
		}

		if (!(flags & ANSI)) {
			// These bits are *only* available in ANSI mode
			// NOTE: COLOR is forced in PETSCII mode later
			flags &= ~(COLOR | RIP | ICE_COLOR | MOUSE);
		}
		else {
			// These bits are *never* available in ANSI mode
			flags &= ~(PETSCII);
		}

		if (flags & PETSCII) {
			// These bits are *never* available in PETSCII mode
			flags &= ~(RIP | ICE_COLOR | MOUSE | NO_EXASCII | UTF8);
			// These bits are *always* availabe in PETSCII mode
			flags |= COLOR;
		}
		return flags;
	}

	static uint32_t get_flags(sbbs_t *sbbsptr) {
		uint32_t flags;

		if (sbbsptr->sys_status & (SS_USERON | SS_NEWUSER)) {
			if (sbbsptr->useron.misc & AUTOTERM) {
				flags = sbbsptr->autoterm;
				flags |= sbbsptr->useron.misc & (NO_EXASCII | SWAP_DELETE | COLOR | ICE_COLOR | MOUSE);
			}
			else
				flags = sbbsptr->useron.misc;
		}
		else
			flags = sbbsptr->autoterm;
		flags &= TERM_FLAGS;
		// TODO: Get rows and cols
		return flags_fixup(flags);
	}

	static link_list_t *listPtrInit(int flags) {
		link_list_t *ret = new link_list_t;
		listInit(ret, flags);
		return ret;
	}

	static void listPtrFree(link_list_t *ll) {
		listFree(ll);
		delete ll;
	}

	Terminal() = delete;
	// Create from sbbs_t*, ie: "Create new"
	Terminal(sbbs_t *sbbsptr) : mouse_hotspots{listPtrInit(0)}, sbbs{sbbsptr},
	    flags_{get_flags(sbbsptr)}, savedlines{listPtrInit(0)} {}

	// Create from Terminal*, ie: Update
	Terminal(Terminal *t) : row{t->row}, column{t->column},
	    rows{t->rows}, cols{t->cols}, tabstop{t->tabstop}, lastcrcol{t->lastcrcol}, 
	    cterm_version{t->cterm_version}, lncntr{t->lncntr}, latr{t->latr}, curatr{t->curatr},
	    lbuflen{t->lbuflen}, mouse_mode{t->mouse_mode}, pause_hotspot{t->pause_hotspot},
	    suspend_lbuf{t->suspend_lbuf},
	    mouse_hotspots{t->mouse_hotspots}, sbbs{t->sbbs}, flags_{get_flags(t->sbbs)},
	    savedlines{t->savedlines} {
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
		t->flags_ = flags_;
		t->set_mouse(mouse_mode);
	}

	// Create from sbbsptr* and Terminal*, ie: Create a copy
	Terminal(sbbs_t *sbbsptr, Terminal *t) : row{t->row}, column{t->column},
	    rows{t->rows}, cols{t->cols}, tabstop{t->tabstop}, lastcrcol{t->lastcrcol}, 
	    cterm_version{t->cterm_version}, lncntr{t->lncntr}, latr{t->latr}, curatr{t->curatr},
	    lbuflen{t->lbuflen}, mouse_mode{t->mouse_mode}, pause_hotspot{t->pause_hotspot},
	    suspend_lbuf{t->suspend_lbuf},
	    mouse_hotspots{listPtrInit(0)}, sbbs{sbbsptr}, flags_{get_flags(t->sbbs)},
	    savedlines{listPtrInit(0)} {}

	virtual ~Terminal()
	{
		listPtrFree(mouse_hotspots);
		listPtrFree(savedlines);
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

	virtual bool gotoxy(unsigned x, unsigned y) {
		return false;
	}

	/*
	 * TODO: These start at zero, but gotoxy starts at 1
	 */
	virtual bool gotox(unsigned x) {
		return gotoxy(x + 1, row + 1);
	}

	virtual bool gotoy(unsigned y) {
		return gotoxy(column + 1, y + 1);
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
		sbbs->term_out('\r');
	}

	virtual void line_feed(unsigned count = 1) {
		for (unsigned i = 0; i < count; i++)
			sbbs->term_out('\n');
	}

	/*
	 * Destructive backspace.
	 */
	virtual void backspace(unsigned int count = 1) {
		for (unsigned i = 0; i < count; i++) {
			if (column > 0)
				sbbs->term_out("\b \b");
			else
				break;
		}
	}

	virtual void newline(unsigned count = 1, bool no_bg_attr = false) {
		// TODO: Original version did not increment row or lncntr
		//       It recursed through outchar()
		int saved_attr = curatr;
		if (no_bg_attr && (curatr & BG_LIGHTGRAY)) { // Don't allow background colors to bleed when scrolling
			sbbs->attr(LIGHTGRAY);
		}
		for (unsigned i = 0; i < count; i++) {
			carriage_return();
			line_feed();
			sbbs->check_pause();
		}
		sbbs->attr(saved_attr);
	}

	virtual void clearscreen() {
		clear_hotspots();
		sbbs->term_out(FF);
		lastcrcol = 0;
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

	virtual void cursor_right(unsigned count = 1) {
		for (unsigned i = 0; i < count; i++) {
			if (column < (cols - 1))
				sbbs->term_out(' ');
			else
				break;
		}
	}

	virtual void cursor_left(unsigned count = 1) {
		for (unsigned i = 0; i < count; i++) {
			if (column > 0)
				sbbs->term_out('\b');
			else
				break;
		}
	}
	virtual void set_output_rate(enum output_rate speed) {}

	virtual uint print_cols(int mode) {
		uint cols = this->cols;
		if ((mode & P_80COLS) && cols > 80)
			cols = 80;
		return cols;
	}
	virtual void center(const char *instr, int mode = 0) {
		uint cols = print_cols(mode);
		char *str = strdup(instr);
		truncsp(str);
		size_t len = bstrlen(str);
		carriage_return();
		if (len < cols)
			cursor_right((cols - len) / 2);
		sbbs->bputs(str, mode);
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
				if (*str == '[') { // CR
					size_t next = bstrlen(str + 1, mode);
					if (next > count)
						count = next;
					break;
				}
				if (*str == ']') // LF
					break;
				else if (*str == '<' && count) // ND-Backspace
					count--;
				else if (*str == '/' && count) // Conditional newline
					break;
			} else if (((*str) & 0x80) && (mode & P_UTF8)) {
				enum unicode_codepoint codepoint = UNICODE_UNDEFINED;
				len = utf8_getc(str, end - str, &codepoint);
				if (len < 1)
					break;
				count += unicode_width(codepoint, sbbs->unicode_zerowidth);
			} else if (*str == '\b') {
				if (count)
					count--;
			} else if (*str == '\r') {
				size_t next = bstrlen(str + 1, mode);
				if (next > count)
					count = next;
				break;
			} else if (*str == '\n')
				break;
			else
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
	 * this function handled it (ie: via term_out(), or stripping it)
	 */
	virtual bool parse_output(char ch) {
		if (utf8_increment(ch))
			return true;

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
			case 28: // FS
			case 29: // GS
			case 30: // RS
			case 31: // US
				return false;

			// Zero-width characters we want to pass through
			case 27: // ESC - This one is especially troubling, but we need to pass it for ANSI detection
				return true;
			case 7:  // BEL
				// Does not go into lbuf...
				return true;

			// Specials
			case 8:  // BS
				if (column)
					dec_column();
				return true;
			case 9:	// TAB
				// TODO: This makes the position unknown
				if (column < (cols - 1)) {
					inc_column();
					while ((column < (cols - 1)) && (column % 8))
						inc_column();
				}
				return true;
			case 10: // LF
				inc_row();
				return true;
			case 12: // FF
				// TODO: This makes the position unknown
				set_row();
				set_column();
				return true;
			case 13: // CR
				lastcrcol = column;
				if (sbbs->console & CON_CR_CLREOL)
					cleartoeol();
				set_column();
				return true;

			// Everything else is assumed one byte wide
			default:
				inc_column();
				return true;
		}
		return false;
	}

	/*
	 * Returns true if inkey() should return the value of ch
	 * Returns false if inkey() should parse as a ctrl character.
	 * 
	 * If ch is a sequence introducer, this should parse the whole
	 * sequence.  If the whole sequence can be replaced with a single
	 * control character, ch should be updated to that character, and
	 * the function should return true.
	 * 
	 * This can add additional characters by using ungetkeys? with
	 * insert as true.  However, to avoid infinite loops, when the
	 * first key this translates to is itself a control character,
	 * ch should be update to that one, and this should return true.
	 * 
	 * Will replace ch with a TERM_KEY_* or CTRL_*, value if it
	 * returns true.
	 * 
	 * ch is the control character that was received.
	 * mode is the mode passed to the inkey() call that received ch
	 */
	virtual bool parse_input_sequence(char& ch, int mode) { return false; }

	virtual bool saveline() {
		struct savedline line;
		line.beg_attr = latr;
		line.end_attr = curatr;
		line.column = column;
		snprintf(line.buf, sizeof(line.buf), "%.*s", lbuflen, lbuf);
		lbuflen = 0;
		return listPushNodeData(savedlines, &line, sizeof(line)) != NULL;
	}

	virtual bool restoreline() {
		struct savedline* line = (struct savedline*)listPopNode(savedlines);
		if (line == NULL)
			return false;
		// Moved insert_indicator() to first to avoid messing
		// up the line buffer with it.
		// Now behaves differently on line 1 (where we should
		// never actually see it)
		insert_indicator();
		sbbs->attr(line->beg_attr);
		// Switch from rputs to term_out()
		// This way we don't need to re-encode
		lbuflen = 0;
		sbbs->term_out(line->buf);
		curatr = line->end_attr;
		free(line);
		return true;
	}

	virtual bool can_highlight() {
		return false;
	}

	virtual bool can_move() {
		return false;
	}

	virtual bool can_mouse() {
		return false;
	}

	virtual bool is_monochrome() {
		return true;
	}

	virtual void updated() {}

	void clear_hotspots(void);
	void scroll_hotspots(unsigned count);

	struct mouse_hotspot* add_hotspot(struct mouse_hotspot* spot);
	struct mouse_hotspot* add_hotspot(char cmd, bool hungry = true, unsigned minx = HOTSPOT_CURRENT_X, unsigned maxx = HOTSPOT_CURRENT_X, unsigned y = HOTSPOT_CURRENT_Y);
	struct mouse_hotspot* add_hotspot(int num, bool hungry = true, unsigned minx = HOTSPOT_CURRENT_X, unsigned maxx = HOTSPOT_CURRENT_X, unsigned y = HOTSPOT_CURRENT_Y);
	struct mouse_hotspot* add_hotspot(uint num, bool hungry = true, unsigned minx = HOTSPOT_CURRENT_X, unsigned maxx = HOTSPOT_CURRENT_X, unsigned y = HOTSPOT_CURRENT_Y);
	struct mouse_hotspot* add_hotspot(const char* cmd, bool hungry = true, unsigned minx = HOTSPOT_CURRENT_X, unsigned maxx = HOTSPOT_CURRENT_X, unsigned y = HOTSPOT_CURRENT_Y);
	bool add_pause_hotspot(char cmd);
	void inc_row(unsigned count = 1);
	void inc_column(unsigned count = 1);
	void dec_row(unsigned count = 1);
	void dec_column(unsigned count = 1);
	void set_row(unsigned val = 0);
	void set_column(unsigned val = 0);
	void cond_newline();
	void cond_blankline();
	void cond_contline();
	bool supports(unsigned cmp_flags);
	bool supports_any(unsigned cmp_flags);
	uint32_t charset();
	const char *charset_str();
	list_node_t *find_hotspot(unsigned x, unsigned y);
	uint32_t flags(bool raw = false);
	void insert_indicator();
	char *attrstr(unsigned newattr, char *str, size_t strsz);
	bool utf8_increment(unsigned char ch);
};

void update_terminal(sbbs_t *sbbsptr, Terminal *term);
void update_terminal(sbbs_t *sbbsptr);

extern "C" {
#endif

void update_terminal(void *sbbsptr, user_t *userptr);

#ifdef __cplusplus
}
#endif

#endif
