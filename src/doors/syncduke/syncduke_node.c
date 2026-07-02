/*
 * syncduke_node.c -- door-native Synchronet node integration: who's-online
 * (Ctrl-U), a NODE_EXT status broadcast, and neutral incoming inter-node messages.
 * All BBS file I/O runs from syncduke_node_tick() (the main loop), never the input
 * path. Everything no-ops when sbbs_node_available() is false. Mirrors the in-game
 * node features of src/doors/syncdoom/syncdoom.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sbbs_node.h"   /* termgfx: node listing / status / nmsg */
#include "syncduke.h"

static int g_node_ok;                  /* sbbs_node_available() cached at init */

/* --- cross-tier banner overlay: who's-online (Ctrl-U, Task 3) / incoming
 * message (Task 4) strip, painted by present() over sixel/JXL/text alike. */
#define SD_NODE_OVROWS 8
#define SD_NODE_OVCOLS 96
static char     g_ov[SD_NODE_OVROWS][SD_NODE_OVCOLS];
static int      g_ov_rows;
static uint32_t g_ov_until;      /* ms deadline (same clock as pacing) */
static uint32_t g_ov_sig;        /* bumped on every content/visibility change */

static void banner_set(int nrows, int ms)
{
	if (nrows > SD_NODE_OVROWS)
		nrows = SD_NODE_OVROWS;
	g_ov_rows  = nrows;
	g_ov_until = syncduke_clock_ms() + (uint32_t)ms;
	g_ov_sig++;                  /* mark dirty so present() repaints */
}

static int banner_live(void)
{
	if (g_ov_rows > 0 && (int32_t)(syncduke_clock_ms() - g_ov_until) >= 0) {
		g_ov_rows = 0;           /* expired -> clear + mark dirty once */
		g_ov_sig++;
	}
	return g_ov_rows > 0;
}

uint32_t syncduke_node_overlay_sig(void)
{
	(void)banner_live();
	return g_ov_rows ? g_ov_sig : 0;
}

void syncduke_node_draw(int cols, int rows)
{
	int i;
	(void)rows;
	if (!banner_live())
		return;
	for (i = 0; i < g_ov_rows; i++) {
		char ob[SD_NODE_OVCOLS + 32];
		int  len = (int)strlen(g_ov[i]);
		int  n;
		if (len > cols)
			len = cols;
		/* white-on-red top strip, one node per row, left-aligned */
		n = snprintf(ob, sizeof ob, "\x1b[%d;1H\x1b[1;37;41m%-*.*s\x1b[0m",
		             i + 1, cols, len, g_ov[i]);
		if (n > 0)
			syncduke_out_put(ob, (size_t)n);
	}
}

static void syncduke_node_atexit(void)
{
	if (g_node_ok)
		sbbs_node_set_ext("");         /* clear our NODE_EXT flag on exit */
}

void syncduke_node_init(void)
{
	static int inited;
	if (inited)
		return;
	inited = 1;
	sbbs_node_init("");                /* $SBBSCTRL/$SBBSNODE locate node.dab; CWD is -home */
	g_node_ok = sbbs_node_available();
	if (g_node_ok)
		atexit(syncduke_node_atexit);
}

/* Publish "playing SyncDuke" as our who's-online free-text, only when it changes. */
static void syncduke_node_status(void)
{
	static char last[64];
	const char *status = "playing SyncDuke";
	if (strcmp(status, last) != 0) {
		sbbs_node_set_ext(status);
		snprintf(last, sizeof(last), "%s", status);
	}
}

void syncduke_node_tick(void)
{
	if (!g_node_ok)
		return;
	syncduke_node_status();
	/* Task 2 adds: build who's-online banner if Ctrl-U was requested.
	 * Task 4 adds: poll sbbs_recv_nmsg ~1 Hz -> banner + bell. */
}
