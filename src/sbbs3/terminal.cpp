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
		if (!(sbbs.console & CON_MOUSE_SCROLL))
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

struct mouse_hotspot* Terminal::add_hotspot(char cmd, bool hungry, int minx, int maxx, int y)
{
	if (!(supports & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	spot.cmd[0] = cmd;
	// TODO: This was the only one that supported negative values
	spot.minx = minx < 0 ? sbbs.column : minx;
	spot.maxx = maxx < 0 ? sbbs.column : maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

struct mouse_hotspot* Terminal::add_hotspot(int num, bool hungry, int minx, int maxx, int y)
{
	if (!(supports & MOUSE))
		return nullptr;
	struct mouse_hotspot spot = {};
	SAFEPRINTF(spot.cmd, "%d\r", num);
	spot.minx = minx < 0 ? sbbs.column : minx;
	spot.maxx = maxx < 0 ? sbbs.column : maxx;
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
	spot.minx = minx < 0 ? sbbs.column : minx;
	spot.maxx = maxx < 0 ? sbbs.column : maxx;
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
	spot.minx = minx < 0 ? sbbs.column : minx;
	spot.maxx = maxx < 0 ? sbbs.column : maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

void Terminal::inc_row(int count)
{
        sbbs.row += sbbs.count;
        if (sbbs.row >= sbbs.rows) {
                scroll_hotspots((sbbs.row - sbbs.rows) + 1);
                sbbs.row = sbbs.rows - 1;
        }
}
