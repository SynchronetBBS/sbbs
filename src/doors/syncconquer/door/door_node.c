/*
 * door_node.c -- door-native Synchronet node integration: who's-online
 * (Ctrl-U), a NODE_EXT status broadcast, and incoming inter-node pages
 * (Ctrl-P to send). All BBS file I/O runs from door_node_tick() (the present
 * loop), never the input path. Everything no-ops when sbbs_node_available()
 * is false. Cloned at a minimal v1 level from src/doors/syncduke/
 * syncduke_node.c -- see that file for the fuller design this trims (no
 * per-map game-state status string; door_io.c's staged output buffer via
 * door_io_send() stands in for syncduke_out_put()).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

#include "sbbs_node.h"   /* termgfx: node listing / status / nmsg */
#include "door_io.h"
#include "door_node.h"

static int  g_node_ok;                 /* sbbs_node_available() cached at init */
static int  g_want_userlist;           /* Ctrl-U: build the who's-online banner on the next tick */
static char g_alias[64];               /* our own player name, for the page "from" field */

static uint32_t node_now_ms(void)
{
#ifdef _WIN32
	return (uint32_t)GetTickCount();
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)((uint64_t)t.tv_sec * 1000ULL + (uint64_t)t.tv_nsec / 1000000ULL);
#endif
}

/* --- cross-tier banner overlay: who's-online (Ctrl-U) / page compose (Ctrl-P)
 * strip, painted by door_io_present() over sixel/JXL/text alike. */
#define SA_NODE_OVROWS 8
#define SA_NODE_OVCOLS 96
static char     g_ov[SA_NODE_OVROWS][SA_NODE_OVCOLS];
static int      g_ov_rows;
static uint32_t g_ov_until;      /* ms deadline (same clock as pacing) */
/* Anchor the overlay to the BOTTOM row instead of the top. Set only by
 * door_node_notice(): the page and who's-online banners are multi-row and stay
 * top-anchored, but a one-line door notice belongs on the bottom row -- that is
 * where this door already puts its Ctrl-S stats strip, and where the sibling
 * doors put the same idle countdown. One feature, one place on screen. */
static int      g_ov_bottom;
/* What an expiring banner left on screen, so the next draw can wipe it. The
 * overlay used to just stop drawing, which leaves its last text sitting there:
 * the game image does not reach the margins, so nothing repaints those cells
 * and the message persists until something else happens to overwrite them. */
static int      g_ov_erase;        /* rows still needing an erase */
static int      g_ov_erase_bottom; /* ...and which anchor they used */

static void banner_set(int nrows, int ms)
{
	if (nrows > SA_NODE_OVROWS)
		nrows = SA_NODE_OVROWS;
	g_ov_rows   = nrows;
	g_ov_until  = node_now_ms() + (uint32_t)ms;
	g_ov_bottom = 0;                 /* top-anchored unless the caller says otherwise */
}

static int banner_live(void)
{
	if (g_ov_rows > 0 && (int32_t)(node_now_ms() - g_ov_until) >= 0) {
		g_ov_erase        = g_ov_rows;      /* remember what to wipe */
		g_ov_erase_bottom = g_ov_bottom;
		g_ov_rows         = 0;              /* expired */
	}
	return g_ov_rows > 0;
}

int door_node_overlay_active(void)
{
	/* A PENDING ERASE counts as active. door_io_present() skips the whole
	 * overlay draw on a deduped frame unless this says otherwise, so a banner
	 * that expires over a static game screen (sitting at a menu, or paused)
	 * would have its erase queued and never emitted -- leaving the last message
	 * stranded on screen indefinitely. Staying "active" for the one extra frame
	 * it takes to wipe the row is what closes that hole. */
	return banner_live() || g_ov_erase > 0;
}

void door_node_draw(int cols, int rows)
{
	int i;
	if (cols <= 0)
		cols = 80;
	if (rows <= 0)
		rows = 25;
	if (!banner_live()) {
		/* Just expired: ERASE the rows it occupied. Nothing else repaints
		 * them, so without this the last message stays on screen -- which is
		 * how a dismissed idle countdown was left stranded in the margin.
		 *
		 * The erase clears the WHOLE row, deliberately: that is the only way
		 * to reach the left/right margins the game picture never covers. But a
		 * top-anchored banner sits ON the picture, so the same wipe punches a
		 * black band through the UI -- hence the repaint request below, which
		 * makes the next present redraw the image over these rows instead of
		 * deduping against a frame that no longer matches the screen. */
		for (i = 0; i < g_ov_erase; i++) {
			char ob[32];
			int  n = snprintf(ob, sizeof ob, "\x1b[%d;1H\x1b[0m\x1b[K",
			                  g_ov_erase_bottom ? rows : i + 1);
			if (n > 0)
				door_io_send(ob, (size_t)n);
		}
		g_ov_erase = 0;
		door_io_force_repaint();
		return;
	}
	for (i = 0; i < g_ov_rows; i++) {
		char ob[SA_NODE_OVCOLS + 32];
		int  len = (int)strlen(g_ov[i]);
		int  n;
		if (len > cols)
			len = cols;
		/* White-on-red strip, one line per row: position, set attr,
		 * ERASE-TO-EOL, then the text (mirrors syncduke_node.c's overlay).
		 * Top-anchored by default; a one-line notice sits on the bottom row
		 * instead (see g_ov_bottom). */
		n = snprintf(ob, sizeof ob, "\x1b[%d;1H\x1b[1;37;41m\x1b[K%.*s\x1b[0m",
		             g_ov_bottom ? rows : i + 1, len, g_ov[i]);
		if (n > 0)
			door_io_send(ob, (size_t)n);
	}
}

void door_node_init(const char *home)
{
	static int  inited;
	const char *a;

	if (inited)
		return;
	inited = 1;
	sbbs_node_init(home != NULL ? home : "");
	g_node_ok = sbbs_node_available();
	/* Get alias from DOOR32.SYS parsing (door_io_alias), fall back to
	 * SBBSALIAS env var (set by some BBS installs), then default to "player" */
	a = door_io_alias();
	if (a != NULL && a[0] != '\0')
		snprintf(g_alias, sizeof(g_alias), "%s", a);
	else {
		a = getenv("SBBSALIAS");
		if (a != NULL && a[0] != '\0')
			snprintf(g_alias, sizeof(g_alias), "%s", a);
		else
			snprintf(g_alias, sizeof(g_alias), "%s", "player");
	}
}

/* Publish our who's-online free-text status -- fixed v1 string (no per-game
 * state to report yet), only written when it changes so it doesn't churn
 * node.exb every frame (sbbs_node_set_ext() writes node.dab every call). */
static void door_node_status(void)
{
	static int done;
	if (done || !g_node_ok)
		return;
	sbbs_node_set_ext("playing Red Alert");
	done = 1;
}

void door_node_userlist_request(void) { g_want_userlist = 1; }

/* One-line transient banner, for a door-level message that is neither a page
 * nor a who's-online list (today: the idle countdown). Deliberately the SAME
 * overlay the other two use rather than a second drawing path, so it inherits
 * their draw, expiry and redraw behavior for free. */
void door_node_notice(const char *text, int ms)
{
	snprintf(g_ov[0], SA_NODE_OVCOLS, " %s", (text != NULL) ? text : "");
	banner_set(1, ms);
	g_ov_bottom = 1;      /* after banner_set(), which clears it */
}

/* Is a bottom-anchored notice on screen? The stats strip owns that same row and
 * asks before repainting, so the two cannot fight over it. */
int door_node_notice_active(void)
{
	return g_ov_bottom && banner_live();
}

/* Retire a notice NOW rather than waiting out its dwell -- the idle countdown
 * calls this the moment the player proves they are there. Expiring rather than
 * clearing directly means the erase still runs through banner_live()'s one
 * path, so the row is wiped exactly as it would be on a natural timeout. */
void door_node_notice_expire(void)
{
	if (g_ov_bottom && g_ov_rows > 0)
		g_ov_until = node_now_ms();
}

static void door_node_userlist(void)
{
	sbbs_node_info_t nodes[16];
	char             alias[26], act[80];
	int              n, i, r, max = SA_NODE_OVROWS;
	n = sbbs_list_nodes(nodes, (int)(sizeof(nodes) / sizeof(nodes[0])), sbbs_my_node());
	if (n == 0) {
		snprintf(g_ov[0], SA_NODE_OVCOLS, " No one else is online.");
		banner_set(1, 6000);
		return;
	}
	snprintf(g_ov[0], SA_NODE_OVCOLS, " Who's online (%d):", n);
	r = 1;
	for (i = 0; i < n && r < max; i++) {
		const char *who, *activity;
		if (r == max - 1 && n - i > 1) {
			snprintf(g_ov[r], SA_NODE_OVCOLS, "   ...and %d more", n - i);
			r++; break;
		}
		who = nodes[i].anon ? "UNKNOWN" : sbbs_username(nodes[i].useron, alias, sizeof(alias));
		activity = nodes[i].ext ? sbbs_node_ext(nodes[i].number, act, sizeof(act))
		           : sbbs_action_str(&nodes[i], act, sizeof(act));
		snprintf(g_ov[r], SA_NODE_OVCOLS, "   %-20.20s  %.50s", who, activity);
		r++;
	}
	banner_set(r, 9000);
}

/* Poll ~1 Hz for an incoming inter-node page; display verbatim + bell. */
static void door_node_recv(void)
{
	static uint32_t last;
	char            raw[600], clean[600];
	const char *    s; char *d;
	uint32_t        now = node_now_ms();
	int             me, len, pos, r, w;
	if (now - last < 1000)
		return;
	last = now;
	me = sbbs_my_node();
	if (me < 1 || sbbs_recv_nmsg(me, raw, sizeof(raw)) <= 0)
		return;
	/* strip Ctrl-A codes + BEL, fold whitespace runs to one space */
	for (s = raw, d = clean; *s && d < clean + sizeof(clean) - 1; s++) {
		if (*s == '\x01') { if (s[1])
								s++; continue; }
		if (*s == '\x07')
			continue;
		if (*s == '\r' || *s == '\n' || *s == ' ') {
			if (d > clean && d[-1] != ' ')
				*d++ = ' ';
			continue;
		}
		*d++ = *s;
	}
	while (d > clean && d[-1] == ' ') d--;
	*d = '\0';
	w = SA_NODE_OVCOLS - 2;
	if (w > 80)
		w = 80;
	if (w < 20)
		w = 20;
	len = (int)strlen(clean); pos = 0; r = 0;
	while (pos < len && r < SA_NODE_OVROWS) {
		int take = len - pos;
		if (take > w) { int b = w; while (b > 0 && clean[pos + b] != ' ') b--; take = (b > w / 2) ? b : w; }
		memcpy(g_ov[r], clean + pos, (size_t)take); g_ov[r][take] = '\0';
		r++; pos += take; while (pos < len && clean[pos] == ' ') pos++;
	}
	banner_set(r, 9000);
	door_io_send("\x07", 1);   /* audible alert */
}

/* --- Ctrl-P: non-blocking page compose, mirrors syncduke_node.c --------- */
static int  g_want_page;
static int  g_compose;
static char g_msgbuf[128];
static int  g_list_rows;

int  door_node_composing(void) { return g_compose; }
void door_node_page_request(void) { g_want_page = 1; }

static void compose_show(void)
{
	snprintf(g_ov[g_list_rows], SA_NODE_OVCOLS, "  > %s_", g_msgbuf);
	banner_set(g_list_rows + 1, 3600000);   /* hold while composing */
}

static void compose_end(void)
{
	g_compose = 0;
	g_ov_rows = 0;
}

static void compose_send(void)
{
	char *msg = g_msgbuf;
	int   me  = sbbs_my_node(), target = 0;

	while (*msg == ' ')
		msg++;
	if (*msg >= '0' && *msg <= '9') {
		target = atoi(msg);
		while (*msg >= '0' && *msg <= '9')
			msg++;
		if (*msg == ':')
			msg++;
		while (*msg == ' ')
			msg++;
	}
	if (*msg == '\0') {
		compose_end();
		return;
	}
	if (target > 0) {
		int ok = sbbs_page_node(target, me, g_alias, msg);
		snprintf(g_ov[0], SA_NODE_OVCOLS, ok
		         ? " Paged node %d." : " Couldn't page node %d (offline / paging off).", target);
	} else {
		sbbs_node_info_t nodes[16];
		int              n = sbbs_list_nodes(nodes, (int)(sizeof(nodes) / sizeof(nodes[0])), me), i, sent = 0;
		for (i = 0; i < n; i++)
			if (sbbs_page_node(nodes[i].number, me, g_alias, msg))
				sent++;
		snprintf(g_ov[0], SA_NODE_OVCOLS, sent
		         ? " Paged %d node%s." : " No one could be paged.", sent, sent == 1 ? "" : "s");
	}
	g_compose = 0;
	banner_set(1, 5000);
}

void door_node_compose_key(int c)
{
	size_t len = strlen(g_msgbuf);
	if (c == 0x1b)               { compose_end();  return; }   /* Esc */
	if (c == '\r' || c == '\n') { compose_send(); return; }   /* Enter */
	if (c == 0x08 || c == 0x7f) { if (len > 0)
									  g_msgbuf[len - 1] = '\0'; }            /* Backspace */
	else if (c >= 0x20 && c < 0x7f) {
		if (len + 1 < sizeof(g_msgbuf)) {
			g_msgbuf[len]     = (char)c;
			g_msgbuf[len + 1] = '\0';
		}
	} else
		return;
	compose_show();
}

static void door_node_page(void)
{
	sbbs_node_info_t nodes[8];
	char             alias[26], act[80];
	int              n, i, r, me = sbbs_my_node();

	n = sbbs_list_nodes(nodes, (int)(sizeof(nodes) / sizeof(nodes[0])), me);
	if (n == 0) {
		snprintf(g_ov[0], SA_NODE_OVCOLS, " No one else is online.");
		banner_set(1, 6000);
		return;
	}
	snprintf(g_ov[0], SA_NODE_OVCOLS,
	         " Page (Esc/blank cancels) -- prefix a node #, or leave blank to page all:");
	r = 1;
	for (i = 0; i < n && r < SA_NODE_OVROWS - 1; i++) {
		const char *who = nodes[i].anon ? "UNKNOWN"
		                  : sbbs_username(nodes[i].useron, alias, sizeof(alias));
		const char *activity = nodes[i].ext
		                       ? sbbs_node_ext(nodes[i].number, act, sizeof(act))
		                       : sbbs_action_str(&nodes[i], act, sizeof(act));
		snprintf(g_ov[r], SA_NODE_OVCOLS, "   %d) %-16.16s %.40s", nodes[i].number, who, activity);
		r++;
	}
	g_list_rows = r;
	g_msgbuf[0] = '\0';
	g_compose   = 1;
	compose_show();
}

void door_node_tick(void)
{
	if (!g_node_ok)
		return;
	door_node_status();
	if (g_want_userlist) { g_want_userlist = 0; door_node_userlist(); }
	if (g_want_page)     { g_want_page = 0;     door_node_page(); }
	door_node_recv();
}
