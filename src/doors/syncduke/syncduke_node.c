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
