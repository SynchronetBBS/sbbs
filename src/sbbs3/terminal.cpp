#include "terminal.h"
#include "ansi_terminal.h"
#include "petscii_term.h"
#include "link_list.h"

void Terminal::clear_hotspots(void)
{
	if (!(flags_ & MOUSE))
		return;
	int spots = listCountNodes(mouse_hotspots);
	if (spots) {
#if 0 //def _DEBUG
		sbbs->lprintf(LOG_DEBUG, "Clearing %ld mouse hot spots", spots);
#endif
		listFreeNodes(mouse_hotspots);
		if (!(sbbs->console & CON_MOUSE_SCROLL))
			set_mouse(MOUSE_MODE_OFF);
	}
}

void Terminal::scroll_hotspots(unsigned count)
{
	if (!(flags_ & MOUSE))
		return;
	unsigned spots = 0;
	unsigned remain = 0;
	list_node_t* node = mouse_hotspots->first;
	while (node != NULL) {
		struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
		list_node_t* next = node->next;
		if (spot->y >= count) {
			spot->y -= count;
			remain++;
		}
		else {
			listRemoveNode(mouse_hotspots, node, true);
		}
		spots++;
		node = next;
	}
#ifdef _DEBUG
	if (spots)
		sbbs->lprintf(LOG_DEBUG, "Scrolled %u mouse hot-spots %u rows (%u remain)", spots, count, remain);
#endif
	if (remain < 1)
		clear_hotspots();
}

struct mouse_hotspot* Terminal::add_hotspot(struct mouse_hotspot* spot)
{
	if (!(sbbs->cfg.sys_misc & SM_MOUSE_HOT) || !supports(MOUSE))
		return nullptr;
	if (spot->y == HOTSPOT_CURRENT_Y)
		spot->y = row;
	if (spot->minx == HOTSPOT_CURRENT_X)
		spot->minx = column;
	if (spot->maxx == HOTSPOT_CURRENT_X)
		spot->maxx = cols - 1;
#if 0 //def _DEBUG
	char         dbg[128];
	sbbs->lprintf(LOG_DEBUG, "Adding mouse hot spot %ld-%ld x %ld = '%s'"
		, spot->minx, spot->maxx, spot->y, c_escape_str(spot->cmd, dbg, sizeof(dbg), /* Ctrl-only? */ true));
#endif
	list_node_t* node = listInsertNodeData(mouse_hotspots, spot, sizeof(*spot));
	if (node == nullptr)
		return nullptr;
	set_mouse(MOUSE_MODE_ON);
	return (struct mouse_hotspot*)node->data;
}

struct mouse_hotspot* Terminal::add_hotspot(char cmd, bool hungry, unsigned minx, unsigned maxx, unsigned y)
{
	if (!(flags_ & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	spot.cmd[0] = cmd;
	spot.minx = minx;
	spot.maxx = maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

bool Terminal::add_pause_hotspot(char cmd)
{
	if (!(flags_ & MOUSE))
		return false;
	if (mouse_hotspots->first != nullptr)
		return false;
	struct mouse_hotspot spot = {};
	spot.cmd[0] = cmd;
	spot.minx = column;
	spot.maxx = column;
	spot.y = HOTSPOT_CURRENT_Y;
	spot.hungry = true;
	if (add_hotspot(&spot) != nullptr)
		return true;
	return false;
}

struct mouse_hotspot* Terminal::add_hotspot(int num, bool hungry, unsigned minx, unsigned maxx, unsigned y)
{
	if (!(flags_ & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	SAFEPRINTF(spot.cmd, "%d\r", num);
	spot.minx = minx;
	spot.maxx = maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

struct mouse_hotspot* Terminal::add_hotspot(uint num, bool hungry, unsigned minx, unsigned maxx, unsigned y)
{
	if (!(flags_ & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	SAFEPRINTF(spot.cmd, "%u\r", num);
	spot.minx = minx;
	spot.maxx = maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

struct mouse_hotspot* Terminal::add_hotspot(const char* cmd, bool hungry, unsigned minx, unsigned maxx, unsigned y)
{
	if (!(flags_ & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	SAFECOPY(spot.cmd, cmd);
	spot.minx = minx;
	spot.maxx = maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

void Terminal::inc_row(unsigned count) {
	row += count;
	if (row >= rows) {
		scroll_hotspots((row - rows) + 1);
		row = rows - 1;
	}
	if (lncntr || lastcrcol)
		lncntr += count;
	if (!suspend_lbuf)
		lbuflen = 0;
}

// TODO: ANSI does *not* specify what happens at the end of a line, and
//       there are at least three common behaviours (in decreasing order
//       of popularity):
//       1) Do the DEC Last Column Flag thing where it hangs out in the
//          last column until a printing character is received, then
//          decides if it should wrap or not
//       2) Wrap immediately when printed to the last column
//       3) Stay in the last column and replace the character
//
//       We assume that #2 happens here, but most terminals do #1... which
//       usually looks like #2, but is slightly different.  Generally, any
//       terminal that does #1 can be switched to do #3, and most terminals
//       that do #2 can also do #3.
//
//       It's fairly simple in ANSI to detect which happens, but is not
//       possible for dumb terminals (though I don't think I've ever seen
//       anything but #2 there).  For things like VT52, PETSCII, and Mode7,
//       the behaviour is well established.
//
//       We can easily emulate #2 if we know if #1 or #3 is currently
//       occurring.  It's possible to emulate #1 with #2 as long as
//       insert character is available, though it's trickier.
//
//       The problem with emulating #2 is that it scrolls the screen when
//       the bottom-right cell is written to, which can have undesired
//       consequences.  It also requires the suppression of CRLF after
//       column 80 is written, which breaks the "a line is a line"
//       abstraction.
//
//       The best would be to switch to #3 when possible, and emulate #1
//       The behaviour would then match the most common implementations
//       and be well controlled.  If only #1 is available, we can still
//       always work as expected (modulo some variations between exact
//       flag clearing conditions), and be able to avoid the scroll.
//       Only terminals that implement #2 (rare) and don't allow switching
//       to #3 (I'm not aware of any) would have problems.
//
//       This would resolve the long-standing issue of printing to column
//       80 which almost every sysop that customizes their BBS has ran
//       across in the past.
//
//       The sticky wicket here is what to do about doors.  The vast
//       majority of them assume #2, but are tested with #1 they also
//       generally assume 80x24.  It would be interesting to have a mode
//       that (optionally) centres the door output in the current
//       terminal.
void Terminal::inc_column(unsigned count) {
	column += count;
	if (column >= cols)
		lastcrcol = cols;
	while (column >= cols) {
		if (!suspend_lbuf)
			lbuflen = 0;
		column -= cols;
		inc_row();
	}
}

void Terminal::dec_row(unsigned count) {
	// Never allow dec_row to scroll up
	if (count > row)
		count = row;
#if 0
	// NOTE: If we do allow scrolling up, scroll_hotspots needs to get signed
	if (count > row) {
		scroll_hotspots(row - count);
	}
#endif
	row -= count;
	if (count > lncntr)
		count = lncntr;
	lncntr -= count;
	if (!suspend_lbuf)
		lbuflen = 0;
}

void Terminal::dec_column(unsigned count) {
	// Never allow dec_column() to wrap
	if (count > column)
		count = column;
	column -= count;
	if (column == 0) {
		if (!suspend_lbuf)
			lbuflen = 0;
	}
}

void Terminal::set_row(unsigned val) {
	if (val >= rows)
		val = rows - 1;
	row = val;
	lncntr = 0;
	if (!suspend_lbuf)
		lbuflen = 0;
}

void Terminal::set_column(unsigned val) {
	if (val >= cols)
		val = cols - 1;
	column = val;
}

void Terminal::cond_newline() {
	if (column > 0)
		newline(1, /* no_bg_attr */true);
}

void Terminal::cond_blankline() {
	cond_newline();
	if (lastcrcol)
		newline();
}

void Terminal::cond_contline() {
	if (column > 0 && cols < TERM_COLS_DEFAULT)
		sbbs->bputs(sbbs->text[LongLineContinuationPrefix]);
}

bool Terminal::supports(unsigned cmp_flags) {
	return (flags_ & cmp_flags) == cmp_flags;
}

bool Terminal::supports_any(unsigned cmp_flags) {
	return (flags_ & cmp_flags);
}

uint32_t Terminal::charset() {
	return (flags_ & CHARSET_FLAGS);
}

const char * Terminal::charset_str()
{
	switch(charset()) {
		case CHARSET_PETSCII:
			return "CBM-ASCII";
		case CHARSET_UTF8:
			return "UTF-8";
		case CHARSET_ASCII:
			return "US-ASCII";
		default:
			return "CP437";
	}
}

list_node_t *Terminal::find_hotspot(unsigned x, unsigned y)
{
	list_node_t *node;

	for (node = mouse_hotspots->first; node != nullptr; node = node->next) {
		struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
		if (spot->y == y && x >= spot->minx && x <= spot->maxx)
			break;
	}
	if (node == nullptr) {
		for (node = mouse_hotspots->first; node != nullptr; node = node->next) {
			struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
			if (spot->hungry && spot->y == y && x >= spot->minx)
				break;
		}
	}
	if (node == NULL) {
		for (node = mouse_hotspots->last; node != nullptr; node = node->prev) {
			struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
			if (spot->hungry && spot->y == y && x <= spot->minx)
				break;
		}
	}

	return node;
}

uint32_t Terminal::flags(bool raw)
{
	// Tuck away pointer to sbbs...
	sbbs_t *sbbs_p = sbbs;

	if (!raw) {
		uint32_t newflags = get_flags(sbbs);
		if (newflags != flags_)
			update_terminal(sbbs);
	}

	// We have potentially destructed ourselves now... use the new object.
	return sbbs_p->term->flags_;
}

void Terminal::insert_indicator() {
	// Defeat line buffer
	suspend_lbuf = true;
	if (save_cursor_pos() && gotoxy(cols, 1)) {
		const unsigned orig_atr{sbbs->curatr};
		if (sbbs->console & CON_INSERT) {
			sbbs->attr(BLINK | BLACK | (LIGHTGRAY << 4));
			sbbs->cp437_out('I');
		} else {
			sbbs->attr(LIGHTGRAY);
			sbbs->cp437_out(' ');
		}
		restore_cursor_pos();
		sbbs->attr(orig_atr);
	}
	suspend_lbuf = false;
}

char *Terminal::attrstr(unsigned newattr, char *str, size_t strsz)
{
	return attrstr(newattr, curatr, str, strsz);
}

/*
 * Increments columns appropriately for UTF-8 codepoint
 * Parses the codepoint from successive uchars
 */
bool Terminal::utf8_increment(unsigned char ch)
{
	if (flags_ & UTF8) {
		// TODO: How many errors should be logged?
		if (utf8_remain > 0) {
			// First, check if this is overlong...
			if (first_continuation
			    && ((utf8_remain == 1 && ch < 0xA0)
			    || (utf8_remain == 2 && ch < 0x90)
			    || (utf8_remain == 3 && ch >= 0x90))) {
				sbbs->lprintf(LOG_WARNING, "Sending invalid UTF-8 codepoint");
				first_continuation = false;
				utf8_remain = 0;
			}
			else if ((ch & 0xc0) != 0x80) {
				first_continuation = false;
				utf8_remain = 0;
				sbbs->lprintf(LOG_WARNING, "Sending invalid UTF-8 codepoint");
			}
			else {
				first_continuation = false;
				utf8_remain--;
				codepoint <<= 6;
				codepoint |= (ch & 0x3f);
				if (utf8_remain)
					return true;
				inc_column(unicode_width(static_cast<enum unicode_codepoint>(codepoint), 0));
				codepoint = 0;
				return true;
			}
		}
		else if ((ch & 0x80) != 0) {
			if ((ch == 0xc0) || (ch == 0xc1) || (ch > 0xF5)) {
				sbbs->lprintf(LOG_WARNING, "Sending invalid UTF-8 codepoint");
			}
			if ((ch & 0xe0) == 0xc0) {
				utf8_remain = 1;
				if (ch == 0xE0)
					first_continuation = true;
				codepoint = ch & 0x1F;
				return true;
			}
			else if ((ch & 0xf0) == 0xe0) {
				utf8_remain = 2;
				if (ch == 0xF0)
					first_continuation = true;
				codepoint = ch & 0x0F;
				return true;
			}
			else if ((ch & 0xf8) == 0xf0) {
				utf8_remain = 3;
				if (ch == 0xF4)
					first_continuation = true;
				codepoint = ch & 0x07;
				return true;
			}
			else
				sbbs->lprintf(LOG_WARNING, "Sending invalid UTF-8 codepoint");
		}
		if (utf8_remain)
			return true;
	}
	return false;
}

void update_terminal(sbbs_t *sbbsptr)
{
	uint32_t flags = Terminal::get_flags(sbbsptr);
	if (sbbsptr->term == nullptr) {
		if (flags & PETSCII)
			sbbsptr->term = new PETSCII_Terminal(sbbsptr);
		else if (flags & (ANSI))
			sbbsptr->term = new ANSI_Terminal(sbbsptr);
		else
			sbbsptr->term = new Terminal(sbbsptr);
	}
	else {
		Terminal *newTerm;
		if (flags & PETSCII)
			newTerm = new PETSCII_Terminal(sbbsptr->term);
		else if (flags & (ANSI))
			newTerm = new ANSI_Terminal(sbbsptr->term);
		else
			newTerm = new Terminal(sbbsptr->term);
		delete sbbsptr->term;
		sbbsptr->term = newTerm;
	}
	sbbsptr->term->updated();
}

void update_terminal(sbbs_t *sbbsptr, Terminal *term)
{
	uint32_t flags = term->flags(true);
	Terminal *newTerm;
	if (flags & PETSCII)
		newTerm = new PETSCII_Terminal(sbbsptr, term);
	else if (flags & (ANSI))
		newTerm = new ANSI_Terminal(sbbsptr, term);
	else
		newTerm = new Terminal(sbbsptr, term);
	if (sbbsptr->term)
		delete sbbsptr->term;
	sbbsptr->term = newTerm;
	sbbsptr->term->updated();
}

extern "C" void update_terminal(void *sbbsptr, user_t *userptr)
{
	if (sbbsptr == nullptr || userptr == nullptr)
		return;
	sbbs_t *sbbs = static_cast<sbbs_t *>(sbbsptr);
	if (sbbs->useron.number == userptr->number)
		update_terminal(sbbs);
}
