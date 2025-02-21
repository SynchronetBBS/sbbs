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
	for (list_node_t* node = mouse_hotspots->first; node != NULL; node = node->next) {
		struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
		spot->y -= count;
		spots++;
		if (spot->y >= 0)
			remain++;
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
	if (column)
		lastlinelen = column;
	row += count;
	if (row >= rows) {
		scroll_hotspots((row - rows) + 1);
		row = rows - 1;
	}
	if (lncntr || lastlinelen)
		lncntr++;
	lbuflen = 0;
}

// TODO: ANSI does *not* specify what happens at the end of a line, and
//       there are at least three common behaviours (in decresing order
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
//       occuring.  It's possible to emulate #1 with #2 as long as
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
	if (column >= cols) {
		// TODO: The "line" needs to be able to be wider than the screen?
		lastlinelen = column;
	}
	while (column >= cols) {
		lbuflen = 0;
		// TODO: This left column at 0 before...
		column -= cols;
		inc_row();
	}
}

void Terminal::dec_row(unsigned count) {
	if (column)
		lastlinelen = column;
	// TODO: Never allow dec_row to scroll up
	if (count > row)
		count = row;
#if 0
	// TODO: If we do allow scrolling up, scroll_hotspots needs to get signed
	if (count > row) {
		scroll_hotspots(row - count);
	}
#endif
	row -= count;
	if (count > lncntr)
		count = lncntr;
	lncntr -= count;
	lbuflen = 0;
}

void Terminal::dec_column(unsigned count) {
	// TODO: Never allow dec_column() to wrap
	if (count > column)
		count = column;
	column -= count;
	if (column == 0)
		lbuflen = 0;
}

void Terminal::set_row(unsigned val) {
	if (val >= rows)
		val = rows - 1;
	if (column)
		lastlinelen = column;
	row = val;
	lncntr = 0;
	lbuflen = 0;
}

void Terminal::set_column(unsigned val) {
	if (val >= cols)
		val = cols - 1;
	column = val;
}

void Terminal::cond_newline() {
	if (column > 0)
		newline();
}

void Terminal::cond_blankline() {
	cond_newline();
	if (lastlinelen)
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
	if (!raw) {
		uint32_t newflags = get_flags(sbbs);
		if (newflags != flags_)
			update_terminal(sbbs);
	}
	// TODO: We have potentially destructed ourselves now...
	return sbbs->term->flags_;
}

void update_terminal(sbbs_t *sbbsptr)
{
	uint32_t flags = Terminal::get_flags(sbbsptr);
	if (sbbsptr->term == nullptr) {
		if (flags & PETSCII)
			sbbsptr->term = new PETSCII_Terminal(sbbsptr);
		else if (flags & (RIP | ANSI))
			sbbsptr->term = new ANSI_Terminal(sbbsptr);
		else
			sbbsptr->term = new Terminal(sbbsptr);
	}
	else {
		Terminal *newTerm;
		if (flags & PETSCII)
			newTerm = new PETSCII_Terminal(sbbsptr->term);
		else if (flags & (RIP | ANSI))
			newTerm = new ANSI_Terminal(sbbsptr->term);
		else
			newTerm = new Terminal(sbbsptr->term);
		delete sbbsptr->term;
		sbbsptr->term = newTerm;
	}
}

void update_terminal(sbbs_t *sbbsptr, Terminal *term)
{
	uint32_t flags = term->flags(true);
	Terminal *newTerm;
	if (flags & PETSCII)
		newTerm = new PETSCII_Terminal(sbbsptr, term);
	else if (flags & (RIP | ANSI))
		newTerm = new ANSI_Terminal(sbbsptr, term);
	else
		newTerm = new Terminal(sbbsptr, term);
	if (sbbsptr->term)
		delete sbbsptr->term;
	sbbsptr->term = newTerm;
}
