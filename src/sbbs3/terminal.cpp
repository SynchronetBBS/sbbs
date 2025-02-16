#include "terminal.h"

void Terminal::clear_hotspots(void)
{
	if (!(supports & MOUSE))
		return;
	int spots = listCountNodes(&mouse_hotspots);
	if (spots) {
#if 0 //def _DEBUG
		lprintf(LOG_DEBUG, "Clearing %ld mouse hot spots", spots);
#endif
		listFreeNodes(&mouse_hotspots);
		if (!(sbbs->console & CON_MOUSE_SCROLL))
			set_mouse(MOUSE_MODE_OFF);
	}
}

void Terminal::scroll_hotspots(unsigned count)
{
	if (!(supports & MOUSE))
		return;
	unsigned spots = 0;
	unsigned remain = 0;
	for (list_node_t* node = mouse_hotspots.first; node != NULL; node = node->next) {
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

struct mouse_hotspot* sbbs_t::add_hotspot(struct mouse_hotspot* spot)
{
	if (!(sbbs->cfg.sys_misc & SM_MOUSE_HOT) || !supports(MOUSE))
		return nullptr;
	if (spot->y < 0)
		spot->y = row;
	if (spot->minx < 0)
		spot->minx = column;
	if (spot->maxx < 0)
		spot->maxx = cols - 1;
#if 0 //def _DEBUG
	char         dbg[128];
	sbbs->lprintf(LOG_DEBUG, "Adding mouse hot spot %ld-%ld x %ld = '%s'"
		, spot->minx, spot->maxx, spot->y, c_escape_str(spot->cmd, dbg, sizeof(dbg), /* Ctrl-only? */ true));
#endif
	list_node_t* node = listInsertNodeData(&mouse_hotspots, spot, sizeof(*spot));
	if (node == nullptr)
		return nullptr;
	set_mouse(MOUSE_MODE_ON);
	return (struct mouse_hotspot*)node->data;
}

struct mouse_hotspot* Terminal::add_hotspot(char cmd, bool hungry, int minx, int maxx, int y)
{
	if (!(supports & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	spot.cmd[0] = cmd;
	// TODO: This was the only one that supported negative values
	spot.minx = minx < 0 ? column : minx;
	spot.maxx = maxx < 0 ? column : maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

bool Terminal::add_pause_hotspot(char cmd)
{
	if (!(supports & MOUSE))
		return false;
	if (mouse_hotspots->first != nullptr)
		return false;
	struct mouse_hotspot spot = {};
	spot.cmd[0] = cmd;
	spot.minx = column;
	spot.maxx = column;
	spot.y = -1;
	spot.hungry = true;
	if (add_hotspot(&spot) != nullptr)
		return true;
	return false;
}

struct mouse_hotspot* Terminal::add_hotspot(int num, bool hungry, int minx, int maxx, int y)
{
	if (!(supports & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	SAFEPRINTF(spot.cmd, "%d\r", num);
	spot.minx = minx < 0 ? column : minx;
	spot.maxx = maxx < 0 ? column : maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

struct mouse_hotspot* Terminal::add_hotspot(uint num, bool hungry, int minx, int maxx, int y)
{
	if (!(supports & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	SAFEPRINTF(spot.cmd, "%u\r", num);
	spot.minx = minx < 0 ? column : minx;
	spot.maxx = maxx < 0 ? column : maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

struct mouse_hotspot* Terminal::add_hotspot(const char* cmd, bool hungry, int minx, int maxx, int y)
{
	if (!(supports & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	SAFECOPY(spot.cmd, cmd);
	spot.minx = minx < 0 ? column : minx;
	spot.maxx = maxx < 0 ? column : maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

void Terminal::inc_row(int count)
{
        row += count;
        if (row >= rows) {
                scroll_hotspots((row - rows) + 1);
                row = rows - 1;
        }
}

void update_terminal(sbbs *sbbsptr)
{
	uint32_t flags = Terminal::get_flags(sbbsptr);
	if (sbbs->term == nullptr) {
		if (flags & PETSCII)
			sbbs->term = new PETSCII_Terminal(sbbsptr);
		else if (flags & (RIP | ANSI))
			sbbs->term = new ANSI_Terminal(sbbsptr);
		else
			sbbs->term = new Terminal(sbbs_ref);
	}
	else {
		Terminal *newTerm;
		if (flags & PETSCII)
			newTerm = new PETSCII_Terminal(sbbs->term);
		else if (flags & (RIP | ANSI))
			newTerm = new ANSI_Terminal(sbbs->term);
		else
			newTerm = new Terminal(sbbs->term);
		delete sbbs->term;
		sbbs->term = newTerm;
	}
	return ret;
}
