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
static int g_want_userlist;            /* Ctrl-U: build the who's-online banner on the next tick */

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
		/* White-on-red top strip, one node per row: position, set attr, ERASE-TO-EOL
		 * (fills the row red to the true terminal edge -- SyncDOOM's draw_page_overlay
		 * pattern), then the text.  \x1b[K bounds the byte count (no `cols`-wide pad), so
		 * `ob` can't be overrun on a wide terminal, and the strip fills regardless of `cols`. */
		n = snprintf(ob, sizeof ob, "\x1b[%d;1H\x1b[1;37;41m\x1b[K%.*s\x1b[0m",
		             i + 1, len, g_ov[i]);
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

/* Publish our who's-online free-text -- "playing SyncDuke", plus the current E#L#
 * while in a real game (syncduke_game_status, which reads the engine state) -- only
 * when it changes, so a map change updates it without churning node.exb every frame. */
static void syncduke_node_status(void)
{
	static char last[64];
	char        status[64];

	syncduke_game_status(status, sizeof status);
	if (strcmp(status, last) != 0) {
		sbbs_node_set_ext(status);
		snprintf(last, sizeof(last), "%s", status);
	}
}

/* Ctrl-U: flag the next tick to (re)build the who's-online banner. The BBS file I/O
 * itself (sbbs_list_nodes) only ever runs from syncduke_node_tick(), never here. */
void syncduke_node_userlist_request(void)
{
	g_want_userlist = 1;
}

/* Build the who's-online banner from the live node list. */
static void syncduke_node_userlist(void)
{
	sbbs_node_info_t nodes[16];
	char             alias[26], act[80];
	int              n, i, r, max = SD_NODE_OVROWS;
	n = sbbs_list_nodes(nodes, (int)(sizeof(nodes) / sizeof(nodes[0])), sbbs_my_node());
	if (n == 0) {
		snprintf(g_ov[0], SD_NODE_OVCOLS, " No one else is online.");
		banner_set(1, 6000);
		return;
	}
	snprintf(g_ov[0], SD_NODE_OVCOLS, " Who's online (%d):", n);
	r = 1;
	for (i = 0; i < n && r < max; i++) {
		const char *who, *activity;
		if (r == max - 1 && n - i > 1) {
			snprintf(g_ov[r], SD_NODE_OVCOLS, "   ...and %d more", n - i);
			r++; break;
		}
		who = nodes[i].anon ? "UNKNOWN" : sbbs_username(nodes[i].useron, alias, sizeof(alias));
		activity = nodes[i].ext ? sbbs_node_ext(nodes[i].number, act, sizeof(act))
		           : sbbs_action_str(&nodes[i], act, sizeof(act));
		snprintf(g_ov[r], SD_NODE_OVCOLS, "   %-20.20s  %.50s", who, activity);
		r++;
	}
	banner_set(r, 9000);
}

/* Poll ~1 Hz for incoming inter-node messages; display verbatim + bell. */
static void syncduke_node_recv(void)
{
	static uint32_t last;
	char            raw[600], clean[600];
	const char *    s; char *d;
	uint32_t        now = syncduke_clock_ms();
	int             me, w, len, pos, r;
	if (now - last < 1000)
		return;
	last = now;
	me = sbbs_my_node();
	if (me < 1 || sbbs_recv_nmsg(me, raw, sizeof(raw)) <= 0)
		return;
	/* strip Ctrl-A codes + BEL, fold whitespace runs to one space -> one flowing string */
	for (s = raw, d = clean; *s && d < clean + sizeof(clean) - 1; s++) {
		if (*s == '\x01') { if (s[1])
								s++; continue; }
		if (*s == '\x07')
			continue;
		if (*s == '\r' || *s == '\n' || *s == ' ') { if (d > clean && d[-1] != ' ')
														 *d++ = ' '; continue; }
		*d++ = *s;
	}
	while (d > clean && d[-1] == ' ') d--;
	*d = '\0';
	/* word-wrap into the banner (verbatim -- NO "paging" label) */
	w = SD_NODE_OVCOLS - 2; if (w > 80)
		w = 80; if (w < 20)
		w = 20;
	len = (int)strlen(clean); pos = 0; r = 0;
	while (pos < len && r < SD_NODE_OVROWS) {
		int take = len - pos;
		if (take > w) { int b = w; while (b > 0 && clean[pos + b] != ' ') b--; take = (b > w / 2) ? b : w; }
		memcpy(g_ov[r], clean + pos, (size_t)take); g_ov[r][take] = '\0';
		r++; pos += take; while (pos < len && clean[pos] == ' ') pos++;
	}
	banner_set(r, 9000);
	syncduke_out_put("\x07", 1);   /* audible alert */
}

/* --- In-game Ctrl-P: non-blocking page compose (SP + MP), mirrors SyncDOOM -------
 * Ctrl-P opens a compose line over the running game -- the game keeps ticking, so in
 * a netgame nobody stalls (your Duke just stands still, like the 'T' chat). The
 * who's-online list is shown above; type a message, Enter sends. A leading node
 * number ("5 hi" / "5: hi") targets that node, else it broadcasts to all others.
 * Esc, or a blank message, cancels. The input path (syncduke_input.c) routes keys
 * here via syncduke_node_compose_key() while syncduke_node_composing() is true. */
static int  g_want_page;               /* Ctrl-P: open the compose on the next tick */
static int  g_compose;                 /* compose overlay active */
static char g_msgbuf[128];             /* message being typed */
static int  g_list_rows;               /* rows above the compose line (prompt + node list) */

int  syncduke_node_composing(void) { return g_compose; }
void syncduke_node_page_request(void) { g_want_page = 1; }

static void compose_show(void)         /* refresh the compose line; re-arm + mark dirty */
{
	snprintf(g_ov[g_list_rows], SD_NODE_OVCOLS, "  > %s_", g_msgbuf);
	banner_set(g_list_rows + 1, 3600000);   /* hold while composing (bumps the dirty sig) */
}

static void compose_end(void)          /* leave compose, clear the banner */
{
	g_compose  = 0;
	g_ov_rows  = 0;
	g_ov_sig++;
}

static void compose_send(void)
{
	char *msg = g_msgbuf;
	int   me  = sbbs_my_node(), target = 0;

	while (*msg == ' ')
		msg++;
	if (*msg >= '0' && *msg <= '9') {            /* optional leading node number */
		target = atoi(msg);
		while (*msg >= '0' && *msg <= '9')
			msg++;
		if (*msg == ':')
			msg++;
		while (*msg == ' ')
			msg++;
	}
	if (*msg == '\0') {                           /* blank message -> cancel */
		compose_end();
		return;
	}
	if (target > 0) {
		int ok = sbbs_page_node(target, me, syncduke_door_alias(), msg);
		snprintf(g_ov[0], SD_NODE_OVCOLS, ok
		         ? " Paged node %d." : " Couldn't page node %d (offline / paging off).", target);
	} else {
		sbbs_node_info_t nodes[16];
		int n = sbbs_list_nodes(nodes, (int)(sizeof(nodes) / sizeof(nodes[0])), me), i, sent = 0;
		for (i = 0; i < n; i++)
			if (sbbs_page_node(nodes[i].number, me, syncduke_door_alias(), msg))
				sent++;
		snprintf(g_ov[0], SD_NODE_OVCOLS, sent
		         ? " Paged %d node%s." : " No one could be paged.", sent, sent == 1 ? "" : "s");
	}
	g_compose = 0;
	banner_set(1, 5000);
}

/* Called from the input path for each typed key while composing. */
void syncduke_node_compose_key(int c)
{
	size_t len = strlen(g_msgbuf);
	if (c == 0x1b)              { compose_end();  return; }   /* Esc */
	if (c == '\r' || c == '\n'){ compose_send(); return; }   /* Enter */
	if (c == 0x08 || c == 0x7f){ if (len > 0) g_msgbuf[len - 1] = '\0'; }   /* Backspace */
	else if (c >= 0x20 && c < 0x7f) {
		if (len + 1 < sizeof(g_msgbuf)) {
			g_msgbuf[len]     = (char)c;
			g_msgbuf[len + 1] = '\0';
		}
	} else
		return;                                              /* ignore other keys */
	compose_show();
}

/* Open the compose from the tick: no one else online -> a notice (like Ctrl-U);
 * otherwise draw the node list and open the compose line. */
static void syncduke_node_page(void)
{
	sbbs_node_info_t nodes[8];
	char alias[26], act[80];
	int  n, i, r, me = sbbs_my_node();

	n = sbbs_list_nodes(nodes, (int)(sizeof(nodes) / sizeof(nodes[0])), me);
	if (n == 0) {
		snprintf(g_ov[0], SD_NODE_OVCOLS, " No one else is online.");
		banner_set(1, 6000);
		return;
	}
	snprintf(g_ov[0], SD_NODE_OVCOLS,
	         " Page (Esc/blank cancels) -- prefix a node #, or leave blank to page all:");
	r = 1;
	for (i = 0; i < n && r < SD_NODE_OVROWS - 1; i++) {       /* leave a row for the compose line */
		const char *who = nodes[i].anon ? "UNKNOWN"
		                  : sbbs_username(nodes[i].useron, alias, sizeof(alias));
		const char *activity = nodes[i].ext
		                       ? sbbs_node_ext(nodes[i].number, act, sizeof(act))
		                       : sbbs_action_str(&nodes[i], act, sizeof(act));
		snprintf(g_ov[r], SD_NODE_OVCOLS, "   %d) %-16.16s %.40s", nodes[i].number, who, activity);
		r++;
	}
	g_list_rows = r;
	g_msgbuf[0] = '\0';
	g_compose   = 1;
	compose_show();
}

void syncduke_node_tick(void)
{
	if (!g_node_ok)
		return;
	syncduke_node_status();
	if (g_want_userlist) { g_want_userlist = 0; syncduke_node_userlist(); }
	if (g_want_page)     { g_want_page = 0;     syncduke_node_page(); }
	syncduke_node_recv();
}
