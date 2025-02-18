#include "terminal.h"
#include "ansi_terminal.h"
#include "petscii_term.h"
#include "link_list.h"

/*
 * Returns true if the caller should send the char, false if
 * this function handled it (ie: via outcom(), or stripping it)
 */
bool Terminal::required_parse_outchar(char ch) {
	switch (ch) {
		// Special values
		case 8:  // BS
			if (lbuflen < LINE_BUFSIZE)
				lbuf[lbuflen++] = ch;
			cursor_left();
			return false;
		case 9:  // TAB - Copy pasta... TODO
			// TODO: Original would wrap, this one (hopefully) doesn't.
			//       Further, use outchar() instead of outcom() to get
			//       the spaces into the line buffer instead of tabs
			if (column < (cols - 1)) {
				sbbs->outchar(' ');
				while ((column < (cols - 1)) && (column % tabstop))
					sbbs->outchar(' ');
			}
			return false;
		case 10: // LF
			// Terminates lbuf
			if (sbbs->line_delay)
				SLEEP(sbbs->line_delay);
			line_feed();
			return false;
		case 12: // FF
			// Does not go into lbuf
			clearscreen();
			return false;
		case 13: // CR
			if (sbbs->console & CON_CR_CLREOL)
				cleartoeol();
			if (lbuflen < LINE_BUFSIZE)
				lbuf[lbuflen++] = ch;
			carriage_return();
			return false;
		// Everything else is assumed one byte wide
	}
	return true;
}

void Terminal::clear_hotspots(void)
{
	if (!(flags & MOUSE))
		return;
	int spots = listCountNodes(mouse_hotspots);
	if (spots) {
#if 0 //def _DEBUG
		lprintf(LOG_DEBUG, "Clearing %ld mouse hot spots", spots);
#endif
		listFreeNodes(mouse_hotspots);
		if (!(sbbs->console & CON_MOUSE_SCROLL))
			set_mouse(MOUSE_MODE_OFF);
	}
}

void Terminal::scroll_hotspots(unsigned count)
{
	if (!(flags & MOUSE))
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
		lprintf(LOG_DEBUG, "Scrolled %u mouse hot-spots %u rows (%u remain)", spots, count, remain);
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
	if (!(flags & MOUSE))
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
	if (!(flags & MOUSE))
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
	if (!(flags & MOUSE))
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
	if (!(flags & MOUSE))
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
	if (!(flags & MOUSE))
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
	// TODO: lbuflen needs some love...
	//lbuflen = 0;
}

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
	return (flags & cmp_flags) == cmp_flags;
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

void Terminal::check_clear_pause()
{
	if (lncntr > 0 && row > 0) {
		lncntr = 0;
		newline();
		if (!(sbbs->sys_status & SS_PAUSEOFF)) {
			sbbs->pause();
			while (lncntr && sbbs->online && !(sbbs->sys_status & SS_ABORT))
				pause();
		}
	}
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
