/* sst_io.c -- terminal session: fd resolution, probe burst, reply parsing,
 * hotkeys, pacing bookkeeping.  Present path arrives in Task 4.
 *
 * Ported from syncconquer/door/door_io.c (fd resolution: door_io.c:1297-1364;
 * output staging: door_io.c:~1370; probe burst: door_io.c:2738-2770; reply
 * parser: door_io.c's door_csi_final()/door_io_pump(), ~3016-3316), trimmed
 * to what this task needs -- no audio, no mouse, no kitty/evdev key modes
 * (M3's job), no pace-ring wiring (Task 4's job). */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "sst_io.h"
#include "term.h"
#include "caps.h"
#include "pace.h"
#include "door32.h"
#include "geometry.h"
#include "sixel.h"
#include "jxl.h"
#include "apc.h"
/* xpdev: iniReadFile()/iniGetInteger() for the sysop sixel_max override
 * (sst_read_ini(), below) -- door/syncscumm.cpp already reads the same
 * syncscumm.ini this way for "subtitles"; this is the identical house
 * pattern, just in the pure-C session layer instead of the C++ ConfMan
 * glue. Also pulls in genwrap.h's stricmp()/strnicmp(), used by the
 * XTVERSION reply match in parse_bytes(). */
#include "ini_file.h"

static int g_fd = -1, g_fd_in = -1;      /* -1 = headless (no session) */
static int g_active;
static int g_quit, g_stats;
static int g_have_sixel, g_is_syncterm, g_jxl, g_probe_replied;
static int g_no_gfx_notified;   /* one-shot: unsupported-terminal notice sent */
static int g_cterm_ver;
#ifdef WITH_JXL
/* CTerm >= 1.329 (TERMGFX_CTERM_VER_BLOB): DrawJXLBlob draws inline, no C;S
 * cache-file round trip -- set from the DA1/CTDA reply's version field
 * (csi_final()'s 'c' case, below). Only meaningful when this build has a
 * real JXL encoder to feed it (see sst_tier(), piece 6). */
static int g_img_blob_ok;
#endif
static int g_canvas_w = 640, g_canvas_h = 400;   /* until the probe answers */
static int g_canvas_exact;   /* set once the ESC[4;h;wt pixel report lands */
/* XTSMGRAPHICS (ESC[?2;1S) reply: the terminal's REPORTED hard sixel raster
 * ceiling (0 = unreported -- xterm ships with window ops disabled by default
 * and never answers at all, so this stays 0 for it unless the sysop enables
 * them).
 *
 * The SIXEL tier's effective ceiling (sst_io_present()'s "Fit + center"
 * block) is resolved in this precedence order:
 *
 *   1. g_sixel_max_override (sysop "sixel_max" ini key, sst_read_ini()) --
 *      authoritative if the sysop set one; clamps both axes to the same
 *      value.
 *   2. g_gfx_max_w/h (this reply) -- the terminal told us its real ceiling,
 *      believe it.
 *   3. g_canvas_w/h (g_canvas_exact, i.e. the ESC[4;h;wt report landed) --
 *      but ONLY when the terminal has not identified itself as xterm
 *      (g_is_xterm, from the XTVERSION reply below). A terminal that
 *      reports an exact pixel canvas without ever answering XTSMGRAPHICS
 *      (Windows Terminal, Foot) is trusted at that size: it is the terminal
 *      that just told us how big its own drawing surface is, and Foot
 *      (1200+ px wide) and Windows Terminal (2048 px wide) both render fine
 *      well past TERMGFX_SIXEL_SAFE_MAX.
 *   4. TERMGFX_SIXEL_SAFE_MAX (xterm's assumed ~1000x1000 default) --
 *      whenever neither 2 nor 3 apply, which is also where a
 *      positively-identified xterm always lands even though IT answers
 *      ESC[14t (unlike a window-ops-disabled default xterm, which answers
 *      neither probe and falls here anyway): a live xterm session had full
 *      frames emitted at 1368x906, over its real ~1000x1000 ceiling, and
 *      xterm discards an oversized declared raster WHOLE rather than
 *      clipping it, so trusting its own canvas report the way WT/Foot are
 *      trusted would still show nothing. A wrong-but-safe default that
 *      shows a (slightly undersized) picture beats a right-looking one that
 *      shows nothing. */
static int g_gfx_max_w, g_gfx_max_h;
/* Sysop override, syncscumm.ini's "sixel_max" key (sst_read_ini()): 0 = no
 * override (the default), else the pixel ceiling applied to BOTH axes,
 * ahead of every other source above. */
static int g_sixel_max_override;
/* Set once the DCS XTVERSION reply (ESC[>0q -> DCS >|<name>(<ver>) ST) has
 * been parsed and its payload starts with "xterm" (case-insensitive; parsed
 * in parse_bytes()'s P_APC_ESC handling, below). A terminal that stays
 * silent -- including a real xterm with window ops left off, and Windows
 * Terminal/Foot builds that don't answer this query -- leaves this 0, which
 * is deliberate: only a POSITIVE xterm identification demotes an exact
 * canvas report to the SAFE_MAX fallback above; silence alone does not. */
static int g_is_xterm;
static int g_term_rows, g_term_cols;   /* from the 999;999 CPR (real grid size) */
static int g_cell_w, g_cell_h;         /* from ESC[16t (real cell pixels), 0=unknown */
static FILE *g_capture;                  /* SYNCSCUMM_SIXELOUT mode */

/* Present-path trace: one line per present-attempt -- ms-timestamp, outcome,
 * tier, bytes emitted, in-flight, pacing depth, cumulative dropped-frame count.
 * A permanent diagnostic asset for the fade/pacing behavior (M2). Line-buffered
 * (setvbuf below), so a killed/timed-out run still leaves a complete-to-the-
 * last-line file. Gated behind EITHER an env var OR a touch-file, with the env
 * var taking precedence:
 *
 * 1. SYNCSCUMM_TRACE=<path> env var (for test scripts): open the specified
 *    absolute path directly.
 * 2. Touch-file gate (for BBS-launched doors -- env vars don't survive execvp):
 *    enabled only when the operator creates the touch-file <data-dir>/
 *    syncscumm/trace (SBBSDATA -- see xtrn/CLAUDE.md's env-var table -- for a
 *    real door launch; a dev/standalone run with no SBBSDATA falls back to a
 *    relative ./syncscumm-trace in the cwd). When enabled, opens
 *    /tmp/syncscumm.<pid>.trace (the pid keeps two nodes' traces from
 *    colliding). Disabled by default: no file is opened and sst_trace() pays
 *    nothing for it.
 *
 * The sst_trace() emitter is defined further down, past the pacing globals it
 * reads (g_inflight/g_auto_depth). */
static FILE *g_trace;
/* Raw wire-byte capture: sst_io_init() opens this and sst_io_flush() tees
 * EXACTLY the bytes that reached the socket -- i.e. what the terminal actually
 * received, AFTER any backpressure drop -- for offline decode. Teeing here,
 * not in out_put(), is deliberate: out_put() records the intended bytes,
 * which hides a dropped/truncated frame.
 *
 * Gated behind a touch-file, not an env var: the BBS execs the door via
 * execvp with no shell, so an env var set before launch does not survive
 * into the process. Enabled only when the operator creates the touch-file
 * <data-dir>/syncscumm/wirecap (SBBSDATA -- see xtrn/CLAUDE.md's env-var
 * table -- for a real door launch; a dev/standalone run with no SBBSDATA
 * falls back to a relative ./syncscumm-wirecap in the cwd). Disabled by
 * default: no file is opened and out_put()/flush() pay nothing for it.
 *
 * When enabled, opens /tmp/syncscumm.<pid>.raw (the pid keeps two nodes'
 * captures from colliding) FULLY buffered (256KB), not unbuffered: a
 * palette-storm frame can be 0.5-1.5MB, and an earlier always-on+unbuffered
 * version of this tee turned every present() into a synchronous disk write
 * on the present path -- a suspected cause of mid-fade pauses (M2
 * live-retest). Buffering risks losing a capture's tail on an unclean kill,
 * an acceptable trade for an opt-in debug aid that must not itself stall a
 * real session. */
static FILE *g_tee;
static void  sst_trace_in(const uint8_t *buf, int n);   /* defined past pacing globals */
static void  sst_stats_draw(void);                      /* defined past pacing globals */

/* Set the instant the peer goes away (EOF/hard read error) or a flush hits a
 * hard write error (not EAGAIN/EINTR) -- see sst_io_flush()/sst_io_pump().
 * Doubles as the "session is dead" flag: sst_io_flush() drops (rather than
 * retries) buffered output once this is set, so out_put()/flush() are safe
 * to keep calling from the caller's normal shutdown path without blocking or
 * erroring. g_quit is also set alongside it so sst_io_quit_requested()
 * callers unwind the same way they would for an explicit q/Ctrl-C. */
static int g_hangup;

/* Pre-door DECSSDT status-line type captured from the DECRQSS reply that
 * termgfx_term_status_off provokes (-1 = not captured yet / unsupported
 * terminal); restored on sst_io_shutdown() (door_io.c:1134 pattern). */
static int g_status_type = -1;

/* ---- output staging (door_io.c:~1370 pattern) ---- */
static uint8_t g_out[1 << 18];
static size_t  g_out_len;
static uint64_t g_tx_bytes;      /* every wire-bound byte, for the Ctrl-S KB/frame stat */
static uint32_t g_dropped_frames;   /* see out_put()'s full-buffer guard, below */

/* door_io.c's door_out_put() grows an unbounded realloc'd buffer, so it never
 * faces this; ours is a fixed 256KB stage, and a present() call with a big
 * canvas (a large sixel_encode() can run past 256KB by itself once the
 * terminal has advertised a big pixel canvas -- present's own inflight/
 * "prior frame not flushed" gates normally never let out_put() be called
 * with a backlog already queued, but they say nothing about a SINGLE call
 * that overruns the buffer outright) can otherwise spin here forever: once
 * the stage fills, sst_io_flush() is called mid-loop, and if the peer is
 * backpressured (EAGAIN) that flush makes no progress, so the loop would
 * just keep calling it. If a flush truly buys no room, drop the rest of
 * this call's bytes (counted in g_dropped_frames) instead of spinning --
 * skipping a frame under backpressure is the correct door behavior, and a
 * hard write error already gets funneled through g_hangup below (which the
 * post-flush check here returns out of immediately). */
static void out_put(const void *p, size_t n)
{
	if (g_hangup)
		return;   /* dead session: don't bother staging bytes nobody will read */
	g_tx_bytes += n;
	while (n) {
		size_t room = sizeof(g_out) - g_out_len;
		size_t take = n < room ? n : room;
		memcpy(g_out + g_out_len, p, take);
		g_out_len += take; p = (const uint8_t *)p + take; n -= take;
		if (g_out_len == sizeof(g_out)) {
			size_t before = g_out_len;
			sst_io_flush();
			if (g_hangup)
				return;
			if (g_out_len == before) {
				g_dropped_frames++;
				return;   /* stage stayed full: drop the remainder, don't spin */
			}
		}
	}
}
static void out_puts(const char *s) { out_put(s, strlen(s)); }

/* door_io.c:1567-1577 pattern: EAGAIN/EWOULDBLOCK is backpressure, not an
 * error -- defer (stop for now, keep the unwritten tail buffered for the
 * next call) rather than busy-looping or discarding it. EINTR retries the
 * same write. Any other errno is a dead peer: mark the session hung up
 * (sst_io_hung_up()) and drop the buffer -- there's no live fd left to
 * drain it to. */
void sst_io_flush(void)
{
	size_t off = 0;

	if (g_hangup) { g_out_len = 0; return; }
	if (g_fd < 0 || !g_out_len) { g_out_len = 0; return; }
	while (off < g_out_len) {
		ssize_t n = write(g_fd, g_out + off, g_out_len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;              /* backpressure: preserve the tail, try again later */
			g_hangup = 1;           /* hard write error: dead peer */
			g_quit = 1;
			off = g_out_len;        /* drop the unsent tail, nothing left to send it to */
			break;
		}
		off += (size_t)n;
	}
	if (g_tee != NULL && off > 0)
		fwrite(g_out, 1, off, g_tee);   /* EXACTLY what reached the socket */
	if (off >= g_out_len)
		g_out_len = 0;
	else if (off > 0) {
		memmove(g_out, g_out + off, g_out_len - off);
		g_out_len -= off;
	}
}

static uint32_t now_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)((uint64_t)t.tv_sec * 1000ULL + (uint64_t)t.tv_nsec / 1000000ULL);
}

/* argv indices resolve_fd() consumed (the -s<fd> flag, a DOOR32.SYS path):
 * main() needs these to build the filtered argv scummvm_main() gets, since
 * it rejects options it doesn't recognize (door/syncscumm.cpp). */
#define SST_MAX_CONSUMED 8
static int g_consumed_idx[SST_MAX_CONSUMED];
static int g_consumed_n;

static void mark_consumed(int idx)
{
	if (g_consumed_n < SST_MAX_CONSUMED)
		g_consumed_idx[g_consumed_n++] = idx;
}

int sst_io_consumed(int idx)
{
	int i;
	for (i = 0; i < g_consumed_n; i++)
		if (g_consumed_idx[i] == idx)
			return 1;
	return 0;
}

/* --- fd resolution (door_io.c:1297-1364 pattern, simplified) --------------
 * Activation order: -s<fd> / a DOOR32.SYS argv (its socket, or -- comm type
 * 0, a stdio door -- default fd 1/0) / SYNCSCUMM_SOCK env; else
 * SYNCSCUMM_SIXELOUT capture; else a real tty on stdout; else headless. */
static int resolve_fd(int argc, char **argv)
{
	int i, fl, door32_seen = 0, cli_sock = -1;
	const char *e;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (a[0] == '-' && a[1] == 's' && a[2] != '\0') {
			cli_sock = atoi(a + 2);                                  /* -s<fd> */
			mark_consumed(i);
		} else if (termgfx_door32_is_path(a)) {
			termgfx_door32_t d;
			door32_seen = 1;
			mark_consumed(i);
			if (termgfx_door32_read(a, &d) == 0 && d.socket >= 0)
				cli_sock = d.socket;                                 /* comm type 2 */
			/* comm type 0 (stdio) or a read failure: fall through to the
			 * fd 1/0 default below -- door32_seen alone still activates. */
		}
	}
	if (cli_sock < 0 && (e = getenv("SYNCSCUMM_SOCK")) != NULL)
		cli_sock = atoi(e);

	if (cli_sock >= 0) {
		int one = 1, sz = 96 * 1024;
		g_fd = g_fd_in = cli_sock;
		setsockopt(g_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
		setsockopt(g_fd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof sz);
	} else if (door32_seen) {
		g_fd = 1;
		g_fd_in = 0;                     /* stdio door: BBS redirected our stdio */
	} else if ((e = getenv("SYNCSCUMM_SIXELOUT")) != NULL) {
		g_capture = fopen(e, "wb");
		if (g_capture == NULL)
			fprintf(stderr, "sst_io: SYNCSCUMM_SIXELOUT=%s: %s (falling back to "
			        "headless)\n", e, strerror(errno));
		return g_capture != NULL;        /* capture mode: no input fd at all */
	} else if (isatty(1)) {
		g_fd = 1;
		g_fd_in = 0;
	} else {
		return 0;                        /* headless: no session */
	}

	if ((fl = fcntl(g_fd, F_GETFL, 0)) != -1)
		fcntl(g_fd, F_SETFL, fl | O_NONBLOCK);
	if (g_fd_in != g_fd && (fl = fcntl(g_fd_in, F_GETFL, 0)) != -1)
		fcntl(g_fd_in, F_SETFL, fl | O_NONBLOCK);
	return 1;
}

/* --- minimal CSI/ESC parser (door_io.c's door_csi_final()/door_io_pump()
 * pattern), plus an APC/DCS swallow so the DECRQSS reply to
 * termgfx_term_status_off doesn't leak stray bytes into the hotkey path. --- */
static enum { P_NORMAL, P_ESC, P_CSI, P_APC, P_APC_ESC } g_pstate;
static char g_csi_par[40];
static int  g_csi_len;
static int  g_apc_len;
/* The introducer byte that opened the current APC/DCS/OSC/PM string ('_',
 * 'P', ']', or '^' -- see the P_ESC case, below): only a DCS ('P') string
 * can be the XTVERSION reply, and only that introducer's payload is worth
 * capturing rather than just swallowed. */
static int  g_apc_kind;
/* XTVERSION (ESC[>0q) reply capture: DCS > | <name>(<version>) ST. Bounded
 * well past any real terminal name+version ("XTerm(397)", "foot(1.19.1)",
 * "Windows Terminal 1.22.11141.0" all fit easily); a longer/garbage payload
 * is simply truncated, not a parse error. Reset on every DCS entry (P_ESC's
 * 'P' case) so a stale payload from an earlier DCS string is never matched. */
#define SST_XTVER_CAP 64
static char g_xtver_buf[SST_XTVER_CAP];
static int  g_xtver_len;

/* Pacing: forward-declared here (defined with the rest of the present-path
 * pacing state, below) so csi_final()'s DSR/CPR ('R') case can fold every
 * reply -- the 999;999 grid probe's included -- into the pipeline-depth
 * bookkeeping (door_io.c:2107-2121 door_pace_ack() pattern). Harmless when
 * the ring is still empty (present() hasn't sent a paced frame yet): the
 * "any DSR outstanding" check inside just skips the RTT sample. */
static void sst_pace_ack(void);

/* Parse up to `max` ';'-separated decimal params from g_csi_par (a leading
 * non-digit marker byte like '<'/'='/'?' is simply skipped). */
static int csi_params(int *out, int max)
{
	int n = 0, i, have = 0, v = 0;
	for (i = 0; i <= g_csi_len && n < max; i++) {
		char c = (i < g_csi_len) ? g_csi_par[i] : ';';
		if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); have = 1; }
		else if (c == ';') {
			if (have)
				out[n++] = v;
			v = 0;
			have = 0;
		}
	}
	return n;
}

static void csi_final(char fin)
{
	int p[16], np, k;

	switch (fin) {
		case 'c':   /* DA1 (ESC[c) / CTDA (ESC[<c) device-attributes reply */
			g_probe_replied = 1;
			if (g_csi_len > 0 && (g_csi_par[0] == '<' || g_csi_par[0] == '='))
				g_is_syncterm = 1;
			np = csi_params(p, 16);
			for (k = 0; k < np; k++)
				if (p[k] == 4)
					g_have_sixel = 1;              /* DA1/CTDA sixel cap */
			k = termgfx_caps_cterm_version(p, np, g_csi_par[0]);
			if (k > 0)
				g_cterm_ver = k;                   /* CTDA: maj*1000+min */
#ifdef WITH_JXL
			if (k >= TERMGFX_CTERM_VER_BLOB)
				g_img_blob_ok = 1;   /* DrawJXLBlob: inline JXL frames, no cache file */
#endif
			return;
		case 'R':   /* cursor-position report: the 999;999 grid probe, and
		             * every DSR ack */
			g_probe_replied = 1;
			/* The FIRST CPR is the reply to term_probe's ESC[999;999H+DSR:
			 * parked at the extreme, the terminal clamps to its real size, so
			 * this reports the actual row;col grid. Later CPRs are DSR pacing
			 * acks (cursor wherever a frame left it), so capture once only. */
			if (g_term_rows == 0) {
				np = csi_params(p, 2);
				if (np >= 2 && p[0] > 0 && p[1] > 0) {
					g_term_rows = p[0];
					g_term_cols = p[1];
				}
			}
			sst_pace_ack();
			return;
		case 'n':   /* CTerm state report -- the Q;JXL reply ("ESC[=1;{0,1}n")
		             * is parsed separately by jxl_scan_feed()'s raw-byte scan;
		             * just note a reply arrived. */
			g_probe_replied = 1;
			return;
		case 't':   /* window report: ESC[4;h;wt text-area px, ESC[6;h;wt cell px */
			np = csi_params(p, 4);
			if (np >= 3 && p[0] == 4) {
				g_canvas_h = p[1];
				g_canvas_w = p[2];
				g_canvas_exact = 1;   /* real canvas known; release held frames */
			} else if (np >= 3 && p[0] == 6 && p[1] > 0 && p[2] > 0) {
				g_cell_h = p[1];   /* cell pixel height (ESC[16t reply) */
				g_cell_w = p[2];
			}
			return;
		case 'S':   /* XTSMGRAPHICS reply: ESC[?2;<status>;<W>;<H>S -- Pi=2 is
		             * the sixel graphics geometry item; status 0 = success,
		             * <W>/<H> the terminal's hard raster ceiling in pixels
		             * (xterm's unadvertised default is ~1000x1000; an image
		             * whose DECLARED raster exceeds it is discarded WHOLE, not
		             * clipped). Private '?' marker required -- a bare CSI...S
		             * with no marker is ANSI "scroll up", not this reply. */
			if (g_csi_len > 0 && g_csi_par[0] == '?') {
				np = csi_params(p, 4);
				if (np >= 4 && p[0] == 2 && p[1] == 0 && p[2] > 0 && p[3] > 0) {
					g_gfx_max_w = p[2];
					g_gfx_max_h = p[3];
				}
			}
			return;
		default:
			return;
	}
}

#define SST_ACC_CAP 4096
static uint8_t  g_acc[SST_ACC_CAP];
static size_t   g_acc_len;
static uint32_t g_probe_start_ms;
static uint32_t g_first_present_ms;   /* first present() that reached the holds */
static int      g_jxl_done;

/* JXL reply via termgfx_caps_parse_jxl() over the raw accumulated buffer --
 * its reply ("ESC[=1;{0,1}-...n") carries a '-' the CSI dispatcher above
 * can't cleanly reconstruct, so this is fed the growing raw byte stream
 * instead (caps.h: "idempotent over a growing buffer"). Keep accumulating
 * until both DA1 and JXL have answered, or a 2s deadline passes, then
 * shrink to a 256-byte tail so the buffer doesn't grow for the rest of the
 * session. */
static void jxl_scan_feed(const uint8_t *buf, size_t n)
{
	int r;

	if (g_jxl_done && g_probe_replied)
		return;
	if (g_probe_start_ms == 0)
		g_probe_start_ms = now_ms();

	if (g_acc_len + n > sizeof(g_acc)) {
		size_t keep = g_acc_len > 256 ? 256 : g_acc_len;
		memmove(g_acc, g_acc + (g_acc_len - keep), keep);
		g_acc_len = keep;
	}
	if (n > sizeof(g_acc) - g_acc_len)
		n = sizeof(g_acc) - g_acc_len;
	memcpy(g_acc + g_acc_len, buf, n);
	g_acc_len += n;

	r = termgfx_caps_parse_jxl(g_acc, (int)g_acc_len);
	if (r >= 0) {
		g_jxl = (r == 1);
		g_is_syncterm = 1;
		g_jxl_done = 1;
	}

	if ((g_jxl_done && g_probe_replied)
	    || (int32_t)(now_ms() - g_probe_start_ms) > 2000) {
		if (g_acc_len > 256) {
			memmove(g_acc, g_acc + (g_acc_len - 256), 256);
			g_acc_len = 256;
		}
	}
}

/* The terminal's bottom text row: the real grid from the 999;999 CPR, else a
 * cell-height guess. Shared by the stats bar and its erase-on-hide. */
static int sst_bottom_row(void)
{
	if (g_term_rows > 0)
		return g_term_rows;
	if (g_canvas_h > 0)
		return g_canvas_h / (g_cell_h > 0 ? g_cell_h : 16);
	return 25;
}

static void parse_bytes(const uint8_t *buf, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		uint8_t c = buf[i];
		switch (g_pstate) {
			case P_NORMAL:
				if (c == 0x1b)
					g_pstate = P_ESC;
				else if (c == 0x13) {                   /* Ctrl-S: toggle stats bar */
					g_stats = !g_stats;
					/* Draw/erase the bar NOW, not on the next present -- a
					 * static/deduped scene may not present again for a while, so
					 * waiting would make the toggle appear to do nothing. */
					if (g_stats) {
						sst_stats_draw();
						sst_io_flush();
					} else {
						/* Erase the bar we last drew; it sits on the reserved
						 * bottom row, so clearing the line is enough. */
						char e[32];
						int  en = snprintf(e, sizeof e,
						                   "\x1b[%d;1H\x1b[0m\x1b[K\x1b[?25l",
						                   sst_bottom_row());
						out_put(e, (size_t)en);
						sst_io_flush();
					}
				}
				else if (c == 'q' || c == 0x03)          /* q / Ctrl-C: request quit */
					g_quit = 1;
				break;
			case P_ESC:
				if (c == '[' || c == 'O') {
					g_pstate  = P_CSI;
					g_csi_len = 0;
				} else if (c == '_' || c == 'P' || c == ']' || c == '^') {
					g_pstate    = P_APC;   /* APC/DCS/OSC/PM string: swallow to ST/BEL */
					g_apc_len   = 0;
					g_apc_kind  = c;
					g_xtver_len = 0;
				} else {
					g_pstate = P_NORMAL;
					i--;                 /* not a CSI/APC introducer: reprocess c as
					                      * P_NORMAL (door_io.c:3289-3293 pattern) */
				}
				break;
			case P_APC:
				if (c == 0x1b)
					g_pstate = P_APC_ESC;
				else if (c == 0x07)
					g_pstate = P_NORMAL;
				else {
					/* Capture a DCS string's payload (bounded) so P_APC_ESC's ST
					 * can check it for the XTVERSION reply "> | <name>(<ver>)".
					 * Every other APC/DCS/OSC/PM string this door ever sees (the
					 * DECRQSS status reply included) is still just swallowed --
					 * only a 'P' introducer whose captured payload starts ">|"
					 * is matched, below. */
					if (g_apc_kind == 'P' && g_xtver_len < SST_XTVER_CAP)
						g_xtver_buf[g_xtver_len++] = (char)c;
					if (++g_apc_len > (1 << 16))
						/* Deliberately tighter than door_io.c's 1<<20: this task
						 * has no legitimately-huge inbound APC/DCS payload to
						 * accommodate (just the DECRQSS status reply and the
						 * short XTVERSION reply), so bail sooner on garbage. */
						g_pstate = P_NORMAL;   /* unterminated: bail rather than lock up */
				}
				break;
			case P_APC_ESC:
				if (c == '\\') {
					g_pstate = P_NORMAL;
					/* XTVERSION reply: DCS > | <name>(<version>) ST. Match the
					 * payload PREFIX only (right after ">|"), case-insensitively,
					 * against "xterm" -- real xterm answers "XTerm(<patch>)";
					 * Windows Terminal answers "Windows Terminal <ver>"; foot
					 * answers "foot(<ver>)". A prefix match, not "contains", is
					 * deliberate: some other terminal's own name/compat string
					 * mentioning "xterm" elsewhere (e.g. a TERM-style
					 * "xterm-256color" ad) must not be misidentified. */
					if (g_apc_kind == 'P' && g_xtver_len >= 2
					    && g_xtver_buf[0] == '>' && g_xtver_buf[1] == '|'
					    && g_xtver_len - 2 >= 5
					    && strnicmp(g_xtver_buf + 2, "xterm", 5) == 0)
						g_is_xterm = 1;
				} else {
					g_pstate = P_APC;
				}
				break;
			case P_CSI:
				if (c >= 0x40 && c <= 0x7e) {
					csi_final((char)c);
					g_pstate = P_NORMAL;
				} else if (g_csi_len < (int)sizeof(g_csi_par)) {
					g_csi_par[g_csi_len++] = (char)c;
				}
				break;
		}
	}
}

/* Sysop sixel_max override: syncscumm.ini's root-section "sixel_max" key, an
 * integer pixel ceiling applied to BOTH axes (0/absent -- the default -- means
 * no override, see g_sixel_max_override's doc comment above). Read relative
 * to CWD, the door's launch dir -- the same file, same relative fopen(), and
 * same point in the startup sequence (called from sst_io_init(), itself the
 * very first thing main() does -- door/syncscumm.cpp) as door/syncscumm.cpp's
 * resolveSubtitles() reads its own "subtitles" key from, so no chdir has run
 * between the two reads. A missing file or missing key is not an error --
 * iniGetInteger()'s default (0) is exactly "no override". */
static void sst_read_ini(void)
{
	FILE *f = fopen("syncscumm.ini", "r");
	str_list_t ini;

	if (f == NULL)
		return;
	ini = iniReadFile(f);
	fclose(f);
	if (ini == NULL)
		return;
	g_sixel_max_override = iniGetInteger(ini, ROOT_SECTION, "sixel_max", 0);
	strListFree(&ini);
}

int sst_io_init(int argc, char **argv)
{
	if (!resolve_fd(argc, argv))
		return 0;
	g_active = 1;

	sst_read_ini();

	/* Env-var gate (test scripts): explicit path takes precedence. */
	{
		const char *tp = getenv("SYNCSCUMM_TRACE");
		if (tp != NULL && *tp != '\0') {
			g_trace = fopen(tp, "w");
			if (g_trace != NULL)
				setvbuf(g_trace, NULL, _IOLBF, 0);
		}
	}

	/* Touch-file gate (BBS-launched doors): env vars don't survive execvp.
	 * Enabled when the operator creates the touch-file <data-dir>/syncscumm/
	 * trace (SBBSDATA for a real door; dev/standalone fallback ./syncscumm-
	 * trace). Env var above takes precedence if both are set. */
	if (g_trace == NULL) {
		const char *data_dir = getenv("SBBSDATA");
		char        touch[512];

		if (data_dir != NULL && *data_dir != '\0')
			snprintf(touch, sizeof touch, "%s/syncscumm/trace", data_dir);
		else
			snprintf(touch, sizeof touch, "./syncscumm-trace");   /* dev/standalone fallback */

		if (access(touch, F_OK) == 0) {
			char path[64];

			snprintf(path, sizeof path, "/tmp/syncscumm.%ld.trace", (long)getpid());
			g_trace = fopen(path, "w");
			if (g_trace != NULL)
				setvbuf(g_trace, NULL, _IOLBF, 0);   /* line-buffered */
		}
	}

	if (g_capture != NULL)
		return 1;   /* capture mode: no terminal to probe, no input to read */

	/* Debug wire-byte capture: opt in via a touch-file (see g_tee, above).
	 * Absent -- the default -- this opens nothing and costs nothing. */
	{
		const char *data_dir = getenv("SBBSDATA");
		char        touch[512];

		if (data_dir != NULL && *data_dir != '\0')
			snprintf(touch, sizeof touch, "%s/syncscumm/wirecap", data_dir);
		else
			snprintf(touch, sizeof touch, "./syncscumm-wirecap");   /* dev/standalone fallback */

		if (access(touch, F_OK) == 0) {
			char path[64];

			snprintf(path, sizeof path, "/tmp/syncscumm.%ld.raw", (long)getpid());
			g_tee = fopen(path, "wb");
			if (g_tee != NULL)
				setvbuf(g_tee, NULL, _IOFBF, 1 << 18);   /* 256KB, fully buffered */
		}
	}

	/* Once, on entry: clear+home, hide cursor, no autowrap, reset sixel
	 * scrolling, hide the status line, XTVERSION (xterm identification --
	 * see g_is_xterm's doc comment), probe the terminal's pixel canvas,
	 * DA1+CTDA (sixel/SyncTERM detect), Q;JXL (JXL detect). sst_io_pump()'s
	 * parser resolves the replies (door_io.c:2738-2770, minus audio/mouse).
	 *
	 * ORDER MATTERS: the XTVERSION query goes out BEFORE term_probe's
	 * ESC[14t canvas query and 999;999 CPR. A terminal answers queries in
	 * the order it receives them, and sst_io_present()'s startup holds
	 * release on the canvas/CPR replies alone -- so if XTVERSION trailed
	 * the canvas query, a real xterm's first present() would run with
	 * g_canvas_exact already set but g_is_xterm structurally still 0 (its
	 * reply not yet even solicited), trust the big canvas via precedence
	 * step 3, and emit an over-ceiling first frame xterm discards whole --
	 * the very defect the XTVERSION gate exists to prevent. Sent first,
	 * an answering terminal's identification always lands ahead of the
	 * canvas report it demotes; a terminal that never answers (WT, Foot)
	 * is unaffected either way. */
	out_puts(termgfx_term_enter);
	out_puts(termgfx_term_status_off);
	out_puts("\x1b[>0q");   /* XTVERSION; DCS >|<name>(<ver>) ST, or silence */
	out_puts(termgfx_term_probe);
	out_puts("\x1b[c\x1b[<c");
	out_puts(termgfx_query_jxl);
	sst_io_flush();
	g_probe_start_ms = now_ms();
	return 1;
}

void sst_io_shutdown(void)
{
	if (!g_active)
		return;
	if (g_tee != NULL) {
		fclose(g_tee);
		g_tee = NULL;
	}
	if (g_capture != NULL) {
		fclose(g_capture);
		g_capture = NULL;
	} else {
		/* Restore the status line hidden at entry (door_io.c:1233-1238 pattern):
		 * the captured pre-door DECSSDT type, or the usual indicator default
		 * (type 1) if the DECRQSS reply never came back. */
		char   sb[8];
		size_t sn = termgfx_term_status_set(sb, sizeof sb, g_status_type >= 0 ? g_status_type : 1);
		out_put(sb, sn);
		out_puts(termgfx_term_leave);
		sst_io_flush();
	}
	if (g_trace != NULL) {
		fclose(g_trace);
		g_trace = NULL;
	}
	g_active = 0;
}

int sst_io_active(void) { return g_active; }
int sst_io_hung_up(void) { return g_hangup; }

void sst_io_pump(void)
{
	uint8_t buf[256];

	/* Deliberately NOT gated on g_active: sst_io_shutdown() only retires the
	 * OUTPUT side (status-line/term_leave + "don't stage more bytes"); the fd
	 * itself is still whatever it was, and a caller that pumps once more
	 * after shutdown should still learn the peer is gone rather than get a
	 * silent no-op. Capture mode and headless (no input fd) are the only
	 * real no-read cases. */
	if (g_capture != NULL || g_fd_in < 0)
		return;   /* headless / capture mode (no client to read from) */

	for (;;) {
		ssize_t n = read(g_fd_in, buf, sizeof buf);
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				return;         /* nothing more to read this pump */
			g_hangup = 1;        /* hard read error: treat like a dropped peer */
			g_quit   = 1;
			return;
		}
		if (n == 0) {
			g_hangup = 1;            /* peer closed its end (EOF) */
			g_quit   = 1;
			return;
		}

		/* Capture the DECRQSS reply to our DECSSDT status-line query (the
		 * pre-door status type, for restore on shutdown). Raw-byte scan --
		 * the DCS reply is otherwise swallowed by the P_APC state below --
		 * with a small rolling window to bridge a reply split across reads
		 * (door_io.c:3209-3235 pattern). */
		if (g_status_type < 0) {
			static uint8_t sacc[32];
			static int     sacclen;
			int            r = termgfx_term_parse_status(buf, (int)n);

			if (r < 0) {
				if (n >= (ssize_t)sizeof sacc) {
					memcpy(sacc, buf + (n - (ssize_t)sizeof sacc), sizeof sacc);
					sacclen = (int)sizeof sacc;
				} else {
					int keep = (int)sizeof sacc - (int)n;
					if (sacclen > keep) {
						memmove(sacc, sacc + (sacclen - keep), keep);
						sacclen = keep;
					}
					memcpy(sacc + sacclen, buf, (size_t)n);
					sacclen += (int)n;
				}
				r = termgfx_term_parse_status(sacc, sacclen);
			}
			if (r >= 0)
				g_status_type = r;
		}

		sst_trace_in(buf, (int)n);
		jxl_scan_feed(buf, (size_t)n);
		parse_bytes(buf, (int)n);
	}
}

int sst_io_quit_requested(void) { return g_quit; }
int sst_io_have_sixel(void)     { return g_have_sixel; }
int sst_io_is_syncterm(void)    { return g_is_syncterm; }
int sst_io_jxl_supported(void)  { return g_jxl; }
int sst_io_stats_visible(void)  { return g_stats; }
unsigned sst_io_frames_dropped(void) { return g_dropped_frames; }

/* M2 has no audio path at all (see DESIGN.md's Audio path section -- that's
 * M4 work), so there is nothing to probe yet: always report "no audio".
 * M4 replaces this with real per-session audio-tier detection (mirroring
 * the sixel/JXL capability probes above). */
int sst_io_audio_available(void) { return 0; }

/* ============================================================================
 * Present path (Task 4; JXL tier added Task 7). Ported from syncconquer/
 * door/door_io.c's Phase-2 dirty-rect present loop, resized to SyncSCUMM's
 * fixed SST_FB_W x SST_FB_H (320x200) framebuffer and simplified: two image
 * tiers only, auto-selected (JXL > sixel, sst_tier()) -- no block/text tiers
 * (SCUMM's palette-fade-heavy frames don't suit them the way an RTS or FPS
 * door's HUD does, and the spec forbids text tiers outright), no manual
 * cycle/force (auto-select settles once, at the startup probe), and a fixed
 * 8x16 cell size (this task doesn't track the text grid, only the pixel
 * canvas -- door_cell_size()'s dynamic derivation is future work).
 * ==========================================================================*/

#define SST_TILE            16
#define SST_TX              (SST_FB_W / SST_TILE)                    /* 20 */
#define SST_TY              ((SST_FB_H + SST_TILE - 1) / SST_TILE)   /* 13: bottom row is 8px */
#define SST_MAX_BOXES       16
#define SST_MAX_COMPONENTS  128
#define SST_MERGE_GAP       2
#define SST_FALLBACK_PCT    45
#define SST_SCALE_MAX       2048
#define SST_PACE_DEADLINE_MS 750
/* Palette-storm unstick deadline: during a fade the SCUMM engine repaints the
 * palette every frame, which forces the full-frame path (the cheap dirty-rect
 * path needs a stable palette -- a pal-only change moves every displayed
 * pixel's color, and both tiers bake the palette into each frame). Those big
 * full frames make the DSR acks lag, so the pipeline pins at g_auto_depth and
 * only the pacing deadline unsticks it -- at the normal 750ms that is ~3 frames
 * then a 750ms dark gap (the "stays dark for a long time" defect). While a
 * palette storm is in progress the gate uses this far shorter unstick instead,
 * giving an even ~12fps cadence with no long dark gap; the g_out_len
 * backpressure gate still holds the next frame until this one has left the
 * stage, so this can't overrun a slow link into a drop-storm. */
#define SST_PALSTORM_MIN_MS 80
#define SST_DSR_RING        16
#define SST_PROBE_GRACE_MS  500
#define SST_GFX_SETTLE_MS  2000   /* JXL reply window (matches jxl_scan_feed) */
#define SST_AUTO_DEPTH_MAX  8
#define SST_CAPTURE_MAX_FRAMES 200

/* --- piece 1: sst_now_ms() -- reuse the file's existing CLOCK_MONOTONIC
 * helper (now_ms(), declared above for the probe-grace/jxl-scan timers). No
 * separate function needed; door_io.c's door_now_ms() equivalent already
 * exists here. */

/* --- piece 2: tile diff + coalesce (door_io.c:2425-2520 dr_coalesce() /
 * dr_diff_coalesce(), SST_ constants). The bottom tile row is only 8px tall
 * (200 isn't a multiple of 16), so the memcmp span per tile row is SST_TILE
 * except for ty == SST_TY-1, where it's SST_FB_H - ty*SST_TILE. ---------- */
struct sst_box { int x1, y1, x2, y2; };
static uint8_t sst_grid[SST_TY][SST_TX];

static int sst_coalesce(struct sst_box *box)
{
	static uint8_t   vis[SST_TY][SST_TX];
	static int       st[SST_TY * SST_TX];
	static const int ox[4] = { -1, 1, 0, 0 };
	static const int oy[4] = { 0, 0, -1, 1 };
	int              nb = 0, tx, ty, i, j, merged;

	memset(vis, 0, sizeof vis);
	for (ty = 0; ty < SST_TY; ty++) {
		for (tx = 0; tx < SST_TX; tx++) {
			int sp, x1, y1, x2, y2;
			if (!sst_grid[ty][tx] || vis[ty][tx])
				continue;
			if (nb >= SST_MAX_COMPONENTS)
				return -1;
			sp          = 0;
			st[sp++]    = ty * SST_TX + tx;
			vis[ty][tx] = 1;
			x1          = x2 = tx;
			y1          = y2 = ty;
			while (sp) {
				int idx = st[--sp], cx = idx % SST_TX, cy = idx / SST_TX, k;
				if (cx < x1)
					x1 = cx;
				if (cx > x2)
					x2 = cx;
				if (cy < y1)
					y1 = cy;
				if (cy > y2)
					y2 = cy;
				for (k = 0; k < 4; k++) {
					int nx = cx + ox[k], ny = cy + oy[k];
					if (nx >= 0 && nx < SST_TX && ny >= 0 && ny < SST_TY
					    && sst_grid[ny][nx] && !vis[ny][nx]) {
						vis[ny][nx] = 1;
						st[sp++]    = ny * SST_TX + nx;
					}
				}
			}
			box[nb].x1 = x1; box[nb].y1 = y1;
			box[nb].x2 = x2; box[nb].y2 = y2;
			nb++;
		}
	}
	merged = 1;
	while (merged) {
		merged = 0;
		for (i = 0; i < nb; i++)
			for (j = i + 1; j < nb; j++)
				if (box[i].x1 <= box[j].x2 + SST_MERGE_GAP && box[j].x1 <= box[i].x2 + SST_MERGE_GAP
				    && box[i].y1 <= box[j].y2 + SST_MERGE_GAP && box[j].y1 <= box[i].y2 + SST_MERGE_GAP) {
					if (box[j].x1 < box[i].x1)
						box[i].x1 = box[j].x1;
					if (box[j].y1 < box[i].y1)
						box[i].y1 = box[j].y1;
					if (box[j].x2 > box[i].x2)
						box[i].x2 = box[j].x2;
					if (box[j].y2 > box[i].y2)
						box[i].y2 = box[j].y2;
					box[j] = box[nb - 1];
					nb--;
					j--;
					merged = 1;
				}
	}
	return (nb > SST_MAX_BOXES) ? -1 : nb;
}

static int sst_diff_coalesce(const uint8_t *fb, const uint8_t *last, struct sst_box *box)
{
	const int tiles = SST_TX * SST_TY;
	int       tx, ty, r, nb, dirty = 0;

	for (ty = 0; ty < SST_TY; ty++) {
		int rows = (ty == SST_TY - 1) ? (SST_FB_H - ty * SST_TILE) : SST_TILE;
		for (tx = 0; tx < SST_TX; tx++) {
			int ch = 0;
			for (r = 0; r < rows && !ch; r++) {
				size_t off = (size_t)(ty * SST_TILE + r) * SST_FB_W + (size_t)tx * SST_TILE;
				if (memcmp(fb + off, last + off, SST_TILE) != 0)
					ch = 1;
			}
			sst_grid[ty][tx] = (uint8_t)ch;
			if (ch)
				dirty++;
		}
	}
	if (dirty == 0 || dirty * 100 / tiles >= SST_FALLBACK_PCT)
		return 0;                                   /* nothing / big change -> full frame */
	nb = sst_coalesce(box);
	return (nb <= 0) ? 0 : nb;                       /* too fragmented -> full frame */
}

/* --- piece 3 (scale half): nearest-neighbor scale/pack helpers
 * (door_io.c:1858-1889 ensure_cap()/door_scale_idx() pattern). One shared
 * dynamic buffer: door_io.c's g_idx_buf backs both the full-frame scale and
 * the per-box dirty-rect pack, since only one runs per present() call. ---- */
static int ensure_cap(uint8_t **buf, size_t *cap, size_t need)
{
	uint8_t *nb;
	if (need <= *cap)
		return 1;
	nb = realloc(*buf, need);
	if (nb == NULL)
		return 0;
	*buf = nb;
	*cap = need;
	return 1;
}

static uint8_t *g_idx_buf;   static size_t g_idx_cap;
static uint8_t *g_sixel_buf; static size_t g_sixel_cap;

static const uint8_t *sst_scale_idx(const uint8_t *fb, int ew, int eh)
{
	int y;
	if (!ensure_cap(&g_idx_buf, &g_idx_cap, (size_t)ew * eh))
		return NULL;
	for (y = 0; y < eh; y++) {
		const uint8_t *row = fb + (size_t)(y * SST_FB_H / eh) * SST_FB_W;
		uint8_t *      o   = g_idx_buf + (size_t)y * ew;
		int            x;
		for (x = 0; x < ew; x++)
			o[x] = row[x * SST_FB_W / ew];
	}
	return g_idx_buf;
}

/* Pack a display sub-rect [rx,ry,rw,rh] (in ew x eh scaled-image space) into
 * g_idx_buf, NN-sampled from fb exactly as sst_scale_idx() scales the full
 * frame, so the rect lines up with the sixel frame the client still holds
 * (door_io.c's dr_pack_idx_rect()). */
static const uint8_t *sst_pack_idx_rect(const uint8_t *fb, int ew, int eh,
                                        int rx, int ry, int rw, int rh)
{
	int      i, j;
	uint8_t *p;

	if (!ensure_cap(&g_idx_buf, &g_idx_cap, (size_t)rw * rh))
		return NULL;
	p = g_idx_buf;
	for (j = 0; j < rh; j++) {
		const uint8_t *row = fb + (size_t)((ry + j) * SST_FB_H / eh) * SST_FB_W;
		for (i = 0; i < rw; i++)
			*p++ = row[(rx + i) * SST_FB_W / ew];
	}
	return g_idx_buf;
}

/* --- piece 4: full-frame emit (door_io.c:1918-1936 door_emit_sixel()
 * pattern -- clamp to whole 6-row sixel bands, SF #258). The centering
 * math (piece 3's fit/center half, in sst_io_present() below) uses the
 * UN-rounded eh; this rounds down only for the encode, same as door_io.c,
 * leaving a few pixels of slack at the bottom of the centered slot. -------*/
static size_t sst_emit_sixel(const uint8_t *fb, const uint8_t *pal8, int ew, int eh, int emit_pal)
{
	const uint8_t *idx;

	if (ew > SST_SCALE_MAX)
		ew = SST_SCALE_MAX;
	eh -= eh % 6;
	if (eh < 6)
		eh = 6;
	idx = sst_scale_idx(fb, ew, eh);
	if (idx == NULL)
		return 0;
	return sixel_encode(&g_sixel_buf, &g_sixel_cap, idx, ew, eh, pal8, emit_pal);
}

/* --- piece 5: dirty emit (door_io.c:2553-2625 door_dirty_sixel_present()
 * pattern). Cell size is the fixed 8x16 this task centers against (no text-
 * grid tracking yet), not a derived door_cell_size(). Caller guarantees
 * g_have_last and that the client holds the previous sixel frame (this
 * door only ever emits sixel, so "same tier" is implicit). ---------------*/
static int sst_gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }

/* cw/ch are the terminal's REAL text-cell pixels: a dirty box is placed by
 * CUP at a cell, so its display-pixel rect must snap to that cell grid, or the
 * box lands where the door THINKS the cell is (an 8x16 guess) rather than where
 * the terminal actually draws it (foot's cell is 6x13) -- which tore the comic
 * panels. Snapping on the real grid makes each box align.
 *
 * The box HEIGHT is snapped to LCM(ch, 6) -- a whole number of BOTH text cells
 * (ch) and 6px sixel bands. Neither alone is enough on a cell-anchored terminal
 * like foot: it draws whole 6px bands and allocates whole cells for them, so a
 * box height that is a multiple of the cell but not the band (e.g. 13) leaves a
 * transparent partial band, and foot backfills the rest of the cell block with
 * the background -- a thin black strip along the box's bottom edge, worst in
 * sparse scenes. ch and 6 are coprime for ch=13, so only their LCM (78) clears
 * both. */
static size_t sst_dirty_sixel_present(const uint8_t *fb, const uint8_t *last,
                                      const uint8_t *pal8, int ew, int eh,
                                      int icol, int irow, int cw, int ch)
{
	struct sst_box box[SST_MAX_COMPONENTS];
	int             ehc, nb, k;
	int             vstep = ch / sst_gcd(ch, 6) * 6;   /* LCM(ch,6): cell & band */
	int             emit_pal = !g_is_syncterm;   /* non-SyncTERM resets registers per image */
	size_t          total = 0;

	ehc = eh - eh % 6;                           /* the client's actual sixel height */
	if (ehc < 6)
		return 0;

	nb = sst_diff_coalesce(fb, last, box);
	if (nb <= 0)
		return 0;

	for (k = 0; k < nb; k++) {
		int            fx1 = box[k].x1 * SST_TILE, fx2 = (box[k].x2 + 1) * SST_TILE;
		int            fy1 = box[k].y1 * SST_TILE, fy2 = (box[k].y2 + 1) * SST_TILE;
		int            rx, rx2, ry, ry2, cx, cy, rw, rh;
		const uint8_t *idx;
		size_t         sn;
		char           wrap[24];
		int            wn;

		if (fx2 > SST_FB_W)
			fx2 = SST_FB_W;
		if (fy2 > SST_FB_H)
			fy2 = SST_FB_H;

		/* display rect = the pixels whose NN source falls in [fx1,fx2)x[fy1,fy2) */
		rx  = (fx1 * ew + SST_FB_W - 1) / SST_FB_W;
		rx2 = (fx2 * ew + SST_FB_W - 1) / SST_FB_W;
		ry  = (fy1 * ehc + SST_FB_H - 1) / SST_FB_H;
		ry2 = (fy2 * ehc + SST_FB_H - 1) / SST_FB_H;
		if (rx2 > ew)
			rx2 = ew;
		if (ry2 > ehc)
			ry2 = ehc;

		/* snap OUT to character cells -- sixel places only at a cell corner */
		cx  = rx / cw;
		rx  = cx * cw;
		rx2 = (rx2 + cw - 1) / cw * cw;
		if (rx2 > ew)
			rx2 = ew;
		cy  = ry / ch;
		ry  = cy * ch;                              /* box top at a cell boundary */
		/* Height up to a whole number of BOTH cells and bands (vstep), so foot
		 * fills the box's cell block completely -- no partial band/cell left as
		 * a black strip. Clamp to the frame; the bottom-most box may fall short
		 * of a full vstep (a small partial near the reserved row, not visible). */
		rh  = (ry2 - ry + vstep - 1) / vstep * vstep;
		if (ry + rh > ehc)
			rh = (ehc - ry) / vstep * vstep;   /* stay vstep-aligned at the frame
			                                    * bottom; a <vstep sliver there is
			                                    * left to the next full frame */
		rw  = rx2 - rx;
		if (rw <= 0 || rh <= 0)
			continue;

		/* Encode the box at its exact CELL-ALIGNED height (ry/ry2 were snapped
		 * to whole cells above) -- do NOT round up to a 6px sixel band. A box
		 * taller than its cell region overdraws into the next cell, which a
		 * cell-anchored terminal (foot) clears when it lands the next sixel and
		 * only partly repaints -- leaving the thin black streaks seen in sparse
		 * scenes. sixel_encode() handles a height that isn't a whole number of
		 * 6px bands (the trailing band is partial); SF #258's round-up guarded
		 * SyncTERM's sixel decoder, and SyncTERM uses the JXL tier, never this
		 * sixel-dirty path. */
		idx = sst_pack_idx_rect(fb, ew, ehc, rx, ry, rw, rh);
		if (idx == NULL)
			return 0;                               /* fall back to a full frame */
		sn = sixel_encode(&g_sixel_buf, &g_sixel_cap, idx, rw, rh, pal8, emit_pal);
		if (sn == 0)
			return 0;
		wn = snprintf(wrap, sizeof wrap, "\x1b" "7\x1b[%d;%dH", irow + cy, icol + cx);
		out_put(wrap, (size_t)wn);
		out_put(g_sixel_buf, sn);
		out_puts("\x1b" "8");
		total += sn;
	}
	return total;
}

/* --- piece 6: JXL tier select + emit (Task 7; door_io.c:1938-1957
 * door_emit_jxl() and door_io.c:2627-2705 door_dirty_jxl_present() pattern).
 * WITH_JXL is a compile-time gate, not a runtime one: it's set for THIS
 * translation unit only when door/CMakeLists.txt's libjxl probe found the
 * library (build.sh appends "DEFINES += -DWITH_JXL" to ScummVM's config.mk
 * in that case, which Makefile.common folds into CPPFLAGS for both the %.c
 * and %.cpp generic rules -- see build.sh's comment). Without it,
 * termgfx_jxl_encode() is still linkable (jxl.c ships a stub that always
 * returns 0), but sst_tier() never routes there, so a no-libjxl build never
 * pays for a doomed RGB888 expansion + guaranteed-fail encode call every
 * frame just to fall back to sixel anyway. --------------------------------*/
#define SST_TIER_SIXEL 0
#define SST_TIER_JXL   1

static int sst_tier(void)
{
#ifdef WITH_JXL
	if (g_jxl)
		return SST_TIER_JXL;
#endif
	return SST_TIER_SIXEL;   /* also the pre-reply default */
}

static const char *sst_tier_name(int t)
{
#ifdef WITH_JXL
	if (t == SST_TIER_JXL)
		return "jxl";
#else
	(void)t;
#endif
	return "sixel";
}

#ifdef WITH_JXL
static uint8_t *g_rgb_buf; static size_t g_rgb_cap;
static uint8_t *g_jxl_buf; static size_t g_jxl_cap;
static uint8_t *g_apc_buf; static size_t g_apc_cap;

/* Full-frame RGB888 expansion (door_io.c's door_pack_rgb() pattern): same NN
 * source mapping sst_scale_idx() uses for the sixel path, but through the
 * palette so JXL gets real pixels instead of indices -- SyncTERM's decoder
 * has no palette concept for JXL/PPM. */
static const uint8_t *sst_pack_rgb(const uint8_t *fb, const uint8_t *pal, int ew, int eh)
{
	int y;
	if (!ensure_cap(&g_rgb_buf, &g_rgb_cap, (size_t)ew * eh * 3))
		return NULL;
	for (y = 0; y < eh; y++) {
		const uint8_t *row = fb + (size_t)(y * SST_FB_H / eh) * SST_FB_W;
		uint8_t *      p   = g_rgb_buf + (size_t)y * ew * 3;
		int            x;
		for (x = 0; x < ew; x++) {
			const uint8_t *c = pal + (size_t)row[x * SST_FB_W / ew] * 3;
			*p++ = c[0]; *p++ = c[1]; *p++ = c[2];
		}
	}
	return g_rgb_buf;
}

/* Same, but a display sub-rect [rx,ry,rw,rh] (door_io.c's dr_pack_disp_rect()
 * pattern) -- the dirty-box counterpart of sst_pack_rgb(), sampled with the
 * identical NN mapping so the rect lines up with the held full JXL frame.
 * JXL is pixel-addressed (unlike sixel, which snaps dirty boxes out to whole
 * 8x16 cells because it can only place at a text cursor), so no cell
 * snapping here -- rx/ry/rw/rh stay pixel-exact. */
static const uint8_t *sst_pack_rgb_rect(const uint8_t *fb, const uint8_t *pal,
                                        int ew, int eh, int rx, int ry, int rw, int rh)
{
	int      i, j;
	uint8_t *p;

	if (!ensure_cap(&g_rgb_buf, &g_rgb_cap, (size_t)rw * rh * 3))
		return NULL;
	p = g_rgb_buf;
	for (j = 0; j < rh; j++) {
		const uint8_t *row = fb + (size_t)((ry + j) * SST_FB_H / eh) * SST_FB_W;
		for (i = 0; i < rw; i++) {
			const uint8_t *c = pal + (size_t)row[(rx + i) * SST_FB_W / ew] * 3;
			*p++ = c[0]; *p++ = c[1]; *p++ = c[2];
		}
	}
	return g_rgb_buf;
}

/* Per-session cache-file name (SF syncterm #256): two SyncTERM windows on the
 * same dialing entry share one cache dir, so a fixed name would collide.
 * Only used on the non-blob (Store+Draw) path; a blob-capable client never
 * touches the cache at all, but the name still has to exist to pass in. */
static const char *sst_jxl_name(void)
{
	static char name[32];
	if (name[0] == '\0')
		snprintf(name, sizeof name, "syncscumm_%08x.jxl", termgfx_session_salt());
	return name;
}

static size_t sst_emit_jxl(const uint8_t *fb, const uint8_t *pal8, int ew, int eh, int dx, int dy)
{
	const uint8_t *rgb = sst_pack_rgb(fb, pal8, ew, eh);
	size_t         n;

	if (rgb == NULL)
		return 0;
	n = termgfx_jxl_encode(&g_jxl_buf, &g_jxl_cap, rgb, ew, eh, 2.0f, 1);
	if (n == 0)
		return 0;   /* encode failure (or a no-libjxl stub): caller falls back to sixel */
	return termgfx_apc_image(&g_apc_buf, &g_apc_cap, sst_jxl_name(), "DrawJXL",
	                         g_jxl_buf, n, dx, dy, g_img_blob_ok);
}

/* Present a changed frame as JXL dirty rects. Returns total bytes sent, or 0
 * to tell the caller to send a full frame instead -- but only 0 when NOTHING
 * was staged; once a box has been staged a later box's failure returns the
 * partial total (and forces a full-frame resync next present via
 * g_dropped_frames) so the caller never stacks a full frame on top of partial
 * rects. Caller guarantees
 * g_have_last, an unchanged palette (a pure palette fade can leave the INDEX
 * buffer sst_diff_coalesce() diffs completely unchanged while every pixel's
 * displayed color moves -- unlike sixel's separate palette registers, JXL
 * bakes the palette into each frame's RGB pixels, so a fade MUST force a
 * full re-encode, exactly like the sixel path's own pal_dirty gate), and
 * that the client currently holds the previous JXL frame in blob mode
 * (g_img_blob_ok -- a non-blob dirty box would need its own unique cache
 * file per box, which this task doesn't build). */
static size_t sst_dirty_jxl_present(const uint8_t *fb, const uint8_t *last,
                                    const uint8_t *pal8, int ew, int eh, int dx, int dy)
{
	struct sst_box box[SST_MAX_COMPONENTS];
	int             k, nb;
	size_t          total = 0;

	nb = sst_diff_coalesce(fb, last, box);
	if (nb <= 0)
		return 0;

	for (k = 0; k < nb; k++) {
		int            fx1 = box[k].x1 * SST_TILE, fx2 = (box[k].x2 + 1) * SST_TILE;
		int            fy1 = box[k].y1 * SST_TILE, fy2 = (box[k].y2 + 1) * SST_TILE;
		int            rx, rx2, ry, ry2, rw, rh;
		const uint8_t *rgb;
		size_t         jn, an;

		if (fx2 > SST_FB_W)
			fx2 = SST_FB_W;
		if (fy2 > SST_FB_H)
			fy2 = SST_FB_H;

		/* display rect = the pixels whose NN source falls in [fx1,fx2)x[fy1,fy2);
		 * pixel-exact, no cell-snap (contrast sst_dirty_sixel_present()). */
		rx  = (fx1 * ew + SST_FB_W - 1) / SST_FB_W;
		rx2 = (fx2 * ew + SST_FB_W - 1) / SST_FB_W;
		ry  = (fy1 * eh + SST_FB_H - 1) / SST_FB_H;
		ry2 = (fy2 * eh + SST_FB_H - 1) / SST_FB_H;
		if (rx2 > ew)
			rx2 = ew;
		if (ry2 > eh)
			ry2 = eh;
		rw = rx2 - rx;
		rh = ry2 - ry;
		if (rw <= 0 || rh <= 0)
			continue;

		rgb = sst_pack_rgb_rect(fb, pal8, ew, eh, rx, ry, rw, rh);
		jn = (rgb != NULL)
		   ? termgfx_jxl_encode(&g_jxl_buf, &g_jxl_cap, rgb, rw, rh, 2.0f, 1)
		   : 0;
		if (jn == 0) {
			/* This box failed to pack/encode. If no earlier box has been
			 * staged yet, bail cleanly (return 0) and let the caller send one
			 * full frame instead. But once an earlier box IS staged, a caller
			 * full frame would stack ON TOP of those partial rects (wasted
			 * bytes that can also push out_put()'s stage over its cap and drop
			 * the full frame): deliver the partial update and force a full-frame
			 * resync on the NEXT present instead -- bumping g_dropped_frames is
			 * exactly the signal present()'s post-emit check turns into
			 * g_have_last=0 + g_need_st=1, so this frame's gap is repainted in
			 * full one frame later. */
			if (total == 0)
				return 0;
			g_dropped_frames++;
			return total;
		}
		/* Blob mode only (caller-guaranteed g_img_blob_ok): each box is its
		 * own self-terminated APC sequence (ST-closed by termgfx_apc_image()
		 * itself, door_io.c:2696-2697 pattern) -- no cache-file name needed. */
		an = termgfx_apc_image(&g_apc_buf, &g_apc_cap, sst_jxl_name(), "DrawJXL",
		                       g_jxl_buf, jn, dx + rx, dy + ry, 1 /* blob */);
		if (an > 0) {
			out_put(g_apc_buf, an);
			total += an;
		}
	}
	return total;
}
#endif /* WITH_JXL */

/* --- piece 7 (pacing half): DSR ring + AIMD depth (door_io.c:2075-2121
 * door_dsr_sent()/door_pace_ack() pattern, termgfx/pace.h). ---------------*/
static uint32_t g_dsr_ts[SST_DSR_RING];
static int      g_dsr_h, g_dsr_t;
static int      g_inflight;
static uint32_t g_pace_progress_ms;
static int      g_auto_depth = 3;
static uint32_t g_auto_adj_at;
static int      g_rt_high;
static uint32_t g_rtt_ms, g_rtt_min, g_rtt_min_at;

static void sst_dsr_sent(uint32_t now)
{
	g_dsr_ts[g_dsr_h] = now;
	g_dsr_h = (g_dsr_h + 1) % SST_DSR_RING;
}

/* A DSR cursor-position report came back: one in-flight frame was consumed.
 * (Forward-declared near csi_final(), above, so its 'R' case can call this
 * on every CPR reply -- including the startup 999;999 grid probe, when the
 * ring is still empty and this is a harmless no-op.) */
static void sst_pace_ack(void)
{
	uint32_t now = now_ms();

	if (g_inflight > 0)
		g_inflight--;
	if (g_dsr_h != g_dsr_t) {
		uint32_t rtt = now - g_dsr_ts[g_dsr_t];
		g_dsr_t = (g_dsr_t + 1) % SST_DSR_RING;
		if (termgfx_rtt_sample(&g_rtt_ms, &g_rtt_min, &g_rtt_min_at, &g_rt_high, rtt, now, 0, 0))
			termgfx_aimd_update(1, &g_auto_depth, &g_auto_adj_at,
			                    g_rtt_ms, g_rtt_min, g_rt_high, SST_AUTO_DEPTH_MAX, now);
	}
	g_pace_progress_ms = now;
}

/* Emit one present-path trace line (SYNCSCUMM_TRACE); see g_trace, above. */
static void sst_trace(const char *outcome, const char *tier, size_t bytes)
{
	if (g_trace == NULL)
		return;
	fprintf(g_trace, "%u %-13s %-5s %7zu inflight=%d depth=%d dropped=%u"
	        " canvas=%dx%d%s replied=%d sixel=%d jxl=%d\n",
	        (unsigned)now_ms(), outcome, tier, bytes,
	        g_inflight, g_auto_depth, (unsigned)g_dropped_frames,
	        g_canvas_w, g_canvas_h, g_canvas_exact ? "!" : "?",
	        g_probe_replied, g_have_sixel, g_jxl);
}

/* Dump raw terminal input (SYNCSCUMM_TRACE only): the exact reply bytes the
 * terminal sent, escaped, so a trace shows WHAT the terminal answered to the
 * probe and WHEN -- e.g. whether/when the ESC[4;h;wt canvas report arrives. */
static void sst_trace_in(const uint8_t *buf, int n)
{
	int i;
	if (g_trace == NULL)
		return;
	fprintf(g_trace, "%u IN[%d] ", (unsigned)now_ms(), n);
	for (i = 0; i < n; i++) {
		uint8_t c = buf[i];
		if (c == 0x1b)
			fputs("\\e", g_trace);
		else if (c >= 0x20 && c < 0x7f)
			fputc(c, g_trace);
		else
			fprintf(g_trace, "\\x%02x", c);
	}
	fputc('\n', g_trace);
}

/* --- piece 8: Ctrl-S stats bar (door_io.c:2280-2384 door_fps_tick()/
 * door_stats_draw() pattern, simplified to one row: tier (always "sixel"
 * this task), fps, KB/frame, pipeline depth, RTT). Placed at a row derived
 * from the fixed 16px cell height (this task has no text-grid probe), and
 * re-hides the cursor afterward -- CUP itself doesn't toggle DECTCEM, but
 * some terminals' row-write repaints show a blinking cursor at the new
 * position, and term_enter's initial ?25l only ran once at session start. */
static int      g_fps, g_fps_frames;
static uint32_t g_fps_win_ms, g_bps;
static uint64_t g_tx_bytes_at;

static void sst_fps_tick(int emitted)
{
	uint32_t fnow = now_ms();

	g_fps_frames += emitted;
	if (g_fps_win_ms == 0) {
		g_fps_win_ms  = fnow;
		g_tx_bytes_at = g_tx_bytes;
		return;
	}
	if (fnow - g_fps_win_ms >= 1000) {
		uint32_t el = fnow - g_fps_win_ms;
		/* Only refresh the readout on a window that actually drew a frame; a
		 * fully idle second (an adventure game sits on a static, deduped scene
		 * most of the time) HOLDS the last real rate instead of zeroing it,
		 * which otherwise flashed "-fps -KB/f" constantly. So "-" now means
		 * only "no frame has been drawn yet" (startup), not "idle right now". */
		if (g_fps_frames > 0) {
			g_fps = (int)((uint64_t)g_fps_frames * 1000 / el);
			g_bps = (uint32_t)((g_tx_bytes - g_tx_bytes_at) * 8000 / el);
		}
		g_fps_frames  = 0;
		g_fps_win_ms  = fnow;
		g_tx_bytes_at = g_tx_bytes;
	}
}

static void sst_stats_draw(void)
{
	char               buf[160];
	int                off, brow;
	unsigned long long bpf;

	if (!g_stats)
		return;
	/* Bottom text row from the real grid (999;999 CPR), NOT g_canvas_h/16 --
	 * the cell is often shorter than 16px (foot ~13), which put the bar rows
	 * above the true bottom. */
	brow = sst_bottom_row();
	if (brow < 1)
		brow = 25;
	bpf = g_fps > 0 ? ((uint64_t)g_bps / 8 / (unsigned)g_fps + 512) / 1024 : 0;
	/* No present completed in the last window (e.g. a paused game, or -- before
	 * the palette-storm fix -- a fade's dark gap): show '-' rather than a
	 * misleading "0fps 0KB/f". */
	if (g_fps > 0)
		off = snprintf(buf, sizeof buf,
		              "\x1b[%d;1H\x1b[30;46m%s %dfps %lluKB/f d%d %ums\x1b[K\x1b[0m\x1b[?25l",
		              brow, sst_tier_name(sst_tier()), g_fps, bpf, g_auto_depth, (unsigned)g_rtt_ms);
	else
		off = snprintf(buf, sizeof buf,
		              "\x1b[%d;1H\x1b[30;46m%s -fps -KB/f d%d %ums\x1b[K\x1b[0m\x1b[?25l",
		              brow, sst_tier_name(sst_tier()), g_auto_depth, (unsigned)g_rtt_ms);
	if (off < 0 || off >= (int)sizeof buf)
		return;
	out_put(buf, (size_t)off);
}

/* --- piece 9: sst_io_present() ties 1-8 together (door_io.c:2707+
 * door_io_present() shape). ------------------------------------------------*/
static uint8_t g_last_fb[SST_FB_W * SST_FB_H];
static uint8_t g_last_pal[768];
static int     g_have_last;
static int     g_last_canvas_w, g_last_canvas_h;
static int     g_last_ew, g_last_eh;   /* last EMITTED (fit+gfx-clamped) size --
                                        * a late XTSMGRAPHICS reply can change
                                        * this with the canvas itself unchanged */
static int     g_last_tier = -1;   /* -1: no frame sent yet -- never equals a real tier */
static unsigned g_capture_frames;

/* Set when out_put()'s stage-full guard drops part of a frame's bytes (the
 * dirty path's box wrap or the full path's sixel image) -- see the drop
 * check at the end of this function. A dropped tail can leave a DCS open
 * with no ST, and the client never got a complete image, so the NEXT
 * present() must both close the dangling DCS (this flag) and skip the dirty
 * path (g_have_last is cleared alongside it, below). */
static int g_need_st;

void sst_io_present(const uint8_t *idx, const uint8_t *pal768)
{
	int      pal_dirty, geom_changed, tier, tier_changed;
	int      ew, eh, icol, irow, emit_pal;
#ifdef WITH_JXL
	int      dx = 0, dy = 0;
#endif
	size_t   n, dn;
	unsigned dropped_before;

	if (!g_active || g_hangup)
		return;

	if (g_capture != NULL) {
		/* Capture mode: a self-contained full-res frame per call, no
		 * pacing/dedupe -- SyncscummTermGraphicsManager only calls present()
		 * when the SCUMM engine's own _dirty flag says the frame changed, so
		 * every call here already IS a changed frame; the offline capture
		 * test just wants real DCS sixel frames to grep, capped so the file
		 * can't grow unbounded on a long boot. */
		if (g_capture_frames >= SST_CAPTURE_MAX_FRAMES)
			return;
		n = sixel_encode(&g_sixel_buf, &g_sixel_cap, idx, SST_FB_W, SST_FB_H, pal768, 1);
		if (n != 0) {
			fwrite(g_sixel_buf, 1, n, g_capture);
			g_capture_frames++;
		}
		return;
	}

	/* Pump terminal input HERE, not only from pollEvent(): ScummVM drives an
	 * intro cutscene's updateScreen() -> present() in a tight loop that need
	 * not poll events, so the probe replies -- including the ESC[4;h;wt canvas
	 * report, which has been sitting in the socket since boot -- can go unread
	 * right when the first frames are being drawn. Reading here makes
	 * g_probe_replied / g_have_sixel / g_jxl / g_canvas_exact current before
	 * the geometry and tier are chosen below. */
	sst_io_pump();
	if (g_hangup)
		return;

	/* The startup holds below are bounded from the FIRST present attempt, not
	 * from sst_io_init(): the engine can boot for longer than SST_PROBE_GRACE_MS,
	 * so an init-anchored deadline would already be expired the first time we
	 * ever try to draw and would never actually hold anything (the intro then
	 * renders against the default 640x400 guess -- small, upper-left -- until
	 * the report is finally read). Anchoring here gives the terminal a real
	 * window, however long the boot took. */
	if (g_first_present_ms == 0)
		g_first_present_ms = now_ms();

	/* Startup grace: hold the first frame(s) until the capability probe
	 * answers (sst_io_init()'s DA1/CTDA/JXL burst), so a silent terminal
	 * gets a bounded wait rather than an indefinite one. sst_tier() only
	 * ever picks JXL once the probe has actually confirmed it (g_jxl), so
	 * holding here is enough to keep the first frame from guessing sixel
	 * against a JXL-only reply that just hasn't landed yet. (The exact pixel
	 * canvas is waited on separately, below, but only for a terminal that
	 * actually has a graphics tier -- a no-graphics terminal is turned away by
	 * the gate in between and must not be held here for a canvas it will never
	 * use.) */
	if (!g_probe_replied && (int32_t)(now_ms() - g_first_present_ms) < SST_PROBE_GRACE_MS) {
		sst_trace("gated-grace", sst_tier_name(sst_tier()), 0);
		return;
	}

	/* Non-graphics-terminal gate (deferred here from Task 7). syncscumm renders
	 * only via sixel or JXL. If the terminal ANSWERED the capability probe but
	 * has advertised neither tier, wait out the JXL reply window (a sixel DA1
	 * would already have set g_have_sixel) and then conclude it cannot display
	 * the game: emit a one-time plain-text notice and request quit, rather than
	 * spraying unrenderable sixel DCS at it (sst_tier() otherwise defaults to
	 * sixel). A terminal that stayed *silent* (no DA1 at all) is left to the
	 * historical default-to-sixel path -- a graphics terminal that simply
	 * doesn't answer device-attributes shouldn't be turned away. */
	if (!g_have_sixel && !g_jxl && g_probe_replied) {
		if (!g_jxl_done
		    && (int32_t)(now_ms() - g_probe_start_ms) <= SST_GFX_SETTLE_MS) {
			sst_trace("gated-grace", sst_tier_name(sst_tier()), 0);
			return;   /* JXL cap may still be in flight -- keep holding */
		}
		if (!g_no_gfx_notified) {
			out_puts("\r\n\x1b[0m\r\n"
			         "  This game requires a terminal with sixel or JXL graphics"
			         " support\r\n"
			         "  (such as SyncTERM). This terminal reports neither, so the"
			         " game\r\n"
			         "  cannot be displayed.\r\n\r\n");
			sst_io_flush();
			g_no_gfx_notified = 1;
		}
		g_quit = 1;
		return;
	}

	/* Canvas-size hold (reached only with a graphics tier -- the gate above
	 * turned away a no-graphics terminal). term_probe's ESC[14t answer
	 * (ESC[4;h;wt -> g_canvas_exact) sets the REAL pixel canvas, but it does
	 * NOT set g_probe_replied, so the startup grace above can release the first
	 * frame before it lands. Fit-to-the-default-640x400-guess then renders the
	 * first frame small in the upper-left until the report arrives (plainly
	 * visible on a non-SyncTERM sixel client like Foot; SyncTERM's JXL path had
	 * masked it). Hold until the real canvas is known, bounded by the same
	 * grace -- a terminal that never reports its size just proceeds at the
	 * default once the grace elapses, exactly as before. */
	if (!g_canvas_exact && (int32_t)(now_ms() - g_first_present_ms) < SST_PROBE_GRACE_MS) {
		sst_trace("gated-grace", sst_tier_name(sst_tier()), 0);
		return;
	}

	sst_io_flush();   /* drain whatever's left of the previous frame first */
	if (g_hangup)
		return;

	/* Palette dirty since the last SENT frame? Computed up here (not with the
	 * geometry/tier flags below) because the pacing deadline that follows keys
	 * off it: a pal change forces a full frame (the dirty-rect path needs a
	 * stable palette), and a storm of those is what this fix paces. */
	pal_dirty = !g_have_last || memcmp(g_last_pal, pal768, 768) != 0;

	/* DSR-ACK pacing gate: don't get ahead of what the terminal has drawn.
	 *
	 * The unstick deadline is the crux of the fade fix. A palette fade repaints
	 * the palette every frame, forcing the big full-frame path (the cheap
	 * dirty-rect path needs a stable palette); those large frames make the DSR
	 * acks lag, so the pipeline pins at g_auto_depth and only the deadline
	 * unsticks it. At the normal 750ms that is ~3 frames then a 750ms dark gap
	 * -- the "stays dark for a long time" defect. During a palette storm we use
	 * the far shorter SST_PALSTORM_MIN_MS unstick instead: the fade is
	 * monotonic so a force-sent frame just carries the newest palette, and the
	 * g_out_len backpressure gate just below still holds the next frame until
	 * this one has fully left the stage -- so a short unstick paces evenly to
	 * the link's actual rate rather than overrunning it into a drop-storm. */
	if (g_inflight >= g_auto_depth) {
		uint32_t deadline = (pal_dirty && g_have_last) ? SST_PALSTORM_MIN_MS
		                                               : SST_PACE_DEADLINE_MS;
		if ((int32_t)(now_ms() - g_pace_progress_ms) > (int32_t)deadline) {
			g_inflight = 0;
			g_dsr_t    = g_dsr_h;   /* drop the stale unacked DSR timestamps */
			sst_trace("deadline", sst_tier_name(sst_tier()), 0);
		} else {
			sst_trace("gated-inflight", sst_tier_name(sst_tier()), 0);
			return;
		}
	}
	if (g_out_len > 0) {   /* prior frame's bytes not all out yet */
		sst_trace("gated-outlen", sst_tier_name(sst_tier()), 0);
		return;
	}

	/* Close a DCS a previous drop left dangling (see the drop check below)
	 * before this frame's own bytes go out. Harmless if g_need_st is unset:
	 * an ST with no open DCS is a no-op. */
	if (g_need_st) {
		out_puts("\x1b\\");
		g_need_st = 0;
	}
	dropped_before = g_dropped_frames;

	tier = sst_tier();

	/* Fit + center. The SIXEL tier reserves the LAST text row and positions on
	 * the terminal's REAL cell grid; the JXL tier keeps the full canvas and the
	 * pixel-offset (DX/DY) placement SyncTERM was validated with.
	 *
	 * Why the sixel tier differs: a sixel is drawn at the text cursor, and one
	 * drawn into the bottom row scrolls the page on a ?80l terminal -- seen as
	 * the picture bouncing when a scene repaints. And the cell is often shorter
	 * than the hardcoded 16px (foot ~13), which both mis-sized any reserve and
	 * mis-placed the cell origin. So use the real cell height (ESC[16t, else the
	 * canvas / 999;999-CPR grid) to reserve exactly one row and to center on a
	 * fractional cell. JXL is pixel-addressed and does not scroll at the cursor,
	 * so it is left exactly as before.
	 *
	 * Computed BEFORE geom_changed/dedupe below (not after, as originally):
	 * the gfx-ceiling clamp just below can shrink ew/eh on its own, with the
	 * canvas itself unchanged, whenever a late XTSMGRAPHICS reply lands after
	 * the first frame already went out -- geom_changed has to see THAT, or a
	 * static scene would dedupe/dirty-diff against a last-sent frame that was
	 * emitted at the old, unclamped size. */
	{
		int cellh = g_cell_h > 0 ? g_cell_h
		          : (g_term_rows > 0 ? (g_canvas_h + g_term_rows / 2) / g_term_rows : 16);
		int fit_h = g_canvas_h;

		if (tier == SST_TIER_SIXEL && g_term_rows > 0 && g_canvas_h > cellh)
			fit_h = g_canvas_h - cellh;   /* keep the image off the last row */

		termgfx_geom_fit(g_canvas_w, fit_h, SST_FB_W, SST_FB_H, SST_SCALE_MAX, &ew, &eh);
		/* Effective sixel ceiling -- SIXEL tier only (see g_gfx_max_w's doc
		 * comment, above, for the full precedence rationale). JXL/APC frames
		 * aren't subject to xterm's declared-raster discard (a different wire
		 * format entirely), and the terminals that reach the JXL tier are
		 * SyncTERM/CTerm builds that already answered DA1/CTDA, not a silent
		 * xterm. */
		if (tier == SST_TIER_SIXEL) {
			int gmax_w, gmax_h;

			if (g_sixel_max_override > 0) {
				gmax_w = gmax_h = g_sixel_max_override;      /* 1. sysop override */
			} else if (g_gfx_max_w > 0 && g_gfx_max_h > 0) {
				gmax_w = g_gfx_max_w;                         /* 2. XTSMGRAPHICS reply */
				gmax_h = g_gfx_max_h;
			} else if (g_canvas_exact && !g_is_xterm) {
				gmax_w = g_canvas_w;                          /* 3. trusted exact canvas */
				gmax_h = g_canvas_h;
			} else {
				gmax_w = gmax_h = TERMGFX_SIXEL_SAFE_MAX;      /* 4. xterm's assumed ceiling */
			}
			termgfx_geom_gfx_clamp(gmax_w, gmax_h, &ew, &eh);
		}

		if (tier == SST_TIER_SIXEL && g_term_rows > 0) {
			double cw  = g_term_cols > 0 ? (double)g_canvas_w / g_term_cols : 8.0;
			double chd = (double)g_canvas_h / g_term_rows;
			termgfx_geom_center_ex(g_canvas_w, fit_h, ew, eh, cw, chd,
			                       NULL, NULL, &icol, &irow);
		} else {
			/* dx/dy (pixel offset) feed the pixel-addressed JXL APC; sixel
			 * (no CPR grid) falls back here and uses icol/irow only. */
#ifdef WITH_JXL
			termgfx_geom_center(g_canvas_w, g_canvas_h, ew, eh, 8, 16, &dx, &dy, &icol, &irow);
#else
			termgfx_geom_center(g_canvas_w, g_canvas_h, ew, eh, 8, 16, NULL, NULL, &icol, &irow);
#endif
		}
	}

	tier_changed = (g_last_tier != tier);
	geom_changed = (g_last_canvas_w != g_canvas_w || g_last_canvas_h != g_canvas_h
	               || g_last_ew != ew || g_last_eh != eh);

	/* Whole-frame de-dupe: fb + palette + canvas/emit geometry + tier all
	 * unchanged since the last SENT frame -> nothing new to draw. tier can
	 * in practice only change once per session (sst_tier() settles as soon
	 * as the startup probe answers, before any frame has gone out), but
	 * checking it here costs nothing and keeps this correct if that ever
	 * stops being true. */
	if (g_have_last && !pal_dirty && !geom_changed && !tier_changed
	    && memcmp(g_last_fb, idx, SST_FB_W * SST_FB_H) == 0) {
		sst_fps_tick(0);
		if (g_stats) {
			sst_stats_draw();
			sst_io_flush();
		}
		sst_trace("dedupe", sst_tier_name(tier), 0);
		return;
	}

	/* SyncTERM persists sixel color registers across images -- (re)send them
	 * only on a real palette change, or when (re)entering the sixel tier
	 * from something else (JXL never defined them); other sixel terminals
	 * reset registers per image, so send every frame there. */
	emit_pal = pal_dirty || !g_is_syncterm || g_last_tier != SST_TIER_SIXEL;

	/* Stats bar BEFORE the (possibly huge) image, not after: a palette-storm
	 * full frame can run past out_put()'s 256KB stage, and once that guard
	 * pins g_out_len at exactly its cap, EVERY later out_put() call this
	 * present() makes -- including a trailing sst_stats_draw() -- computes
	 * zero room and silently drops (out_put()'s own "stage stayed full"
	 * path), with no resync marker the way a dropped image gets (g_need_st/
	 * g_have_last). That starved the bar during exactly the big-frame scenes
	 * (fades, palette storms) a player is most likely to have it open for --
	 * it would show once on Ctrl-S, then never again until frames shrank
	 * back under the cap. Queuing it first costs nothing (it always fits by
	 * itself) and guarantees it wins the race for stage space every time. */
	sst_fps_tick(1);
	sst_stats_draw();

	dn = 0;
	if (tier == SST_TIER_SIXEL) {
		/* Dirty-rect sixel: each box is CUP-positioned at a text cell, so it
		 * only lands correctly when its display-pixel rect is snapped to the
		 * terminal's REAL cell grid. With that grid known (999;999 CPR /
		 * ESC[16t) the boxes align and a static-scene repaint costs a few KB
		 * instead of a ~half-MB full frame; without it, fall back to a full
		 * frame rather than mis-place boxes on an 8x16 guess (which tore the
		 * comic panels on foot's 6x13 cell). */
		int cdw = g_cell_w > 0 ? g_cell_w
		        : (g_term_cols > 0 ? (g_canvas_w + g_term_cols / 2) / g_term_cols : 0);
		int cdh = g_cell_h > 0 ? g_cell_h
		        : (g_term_rows > 0 ? (g_canvas_h + g_term_rows / 2) / g_term_rows : 0);

		if (cdw > 0 && cdh > 0 && g_have_last && !pal_dirty && !geom_changed && !tier_changed)
			dn = sst_dirty_sixel_present(idx, g_last_fb, pal768, ew, eh,
			                             icol, irow, cdw, cdh);

		if (dn != 0) {
			n = dn;   /* dirty rects already sent */
		} else {
			char cup[24];
			int  cn = snprintf(cup, sizeof cup, "\x1b[%d;%dH", irow, icol);
			out_put(cup, (size_t)cn);
			n = sst_emit_sixel(idx, pal768, ew, eh, emit_pal);
			out_put(g_sixel_buf, n);
		}
	}
#ifdef WITH_JXL
	else {   /* SST_TIER_JXL */
		if (g_have_last && !pal_dirty && !geom_changed && !tier_changed && g_img_blob_ok)
			dn = sst_dirty_jxl_present(idx, g_last_fb, pal768, ew, eh, dx, dy);

		if (dn != 0) {
			n = dn;   /* dirty rects already sent */
		} else {
			n = sst_emit_jxl(idx, pal768, ew, eh, dx, dy);
			if (n != 0) {
				out_put(g_apc_buf, n);
			} else {
				/* encode failure this frame (or a no-libjxl stub -- can't
				 * happen here since sst_tier() only returns JXL when
				 * WITH_JXL is compiled in and the probe confirmed it, but a
				 * transient libjxl failure is still possible): fall back to
				 * sixel for just this frame, same as door_io.c. */
				char cup[24];
				int  cn = snprintf(cup, sizeof cup, "\x1b[%d;%dH", irow, icol);
				out_put(cup, (size_t)cn);
				n = sst_emit_sixel(idx, pal768, ew, eh, 1);
				out_put(g_sixel_buf, n);
				tier = SST_TIER_SIXEL;
			}
		}
	}
#endif

	out_put("\x1b[6n", 4);   /* DSR: paces the next frame */
	g_inflight++;
	g_pace_progress_ms = now_ms();
	sst_dsr_sent(g_pace_progress_ms);

	if (g_dropped_frames != dropped_before) {
		/* out_put()'s stage-full guard dropped part of this frame (sixel
		 * dirty path: a box's wrap/sixel bytes; sixel full path: the sixel
		 * image itself, possibly mid-DCS with no ST; JXL: an APC sequence,
		 * whole-frame or dirty-box, possibly cut before its own trailing ST
		 * -- termgfx_apc_image() always appends one, but out_put()'s guard
		 * can still truncate mid-call same as sixel's raw DCS stream). The
		 * client did not receive a complete image either way, so don't
		 * record it as "what the client has": diffing the next frame
		 * against it would only repaint what changed since this incomplete
		 * frame, leaving whatever it was meant to fix unpainted. Force the
		 * next present() to send a full frame (g_have_last = 0) and to
		 * first close any dangling DCS/APC (g_need_st) -- ST (ESC \\) is
		 * the same two bytes for both, so the one resync path covers both
		 * tiers with no tier-specific handling needed. */
		g_have_last = 0;
		g_need_st   = 1;
	} else {
		memcpy(g_last_fb, idx, SST_FB_W * SST_FB_H);
		memcpy(g_last_pal, pal768, 768);
		g_last_canvas_w = g_canvas_w;
		g_last_canvas_h = g_canvas_h;
		g_last_ew       = ew;
		g_last_eh       = eh;
		g_last_tier     = tier;
		g_have_last     = 1;
	}

	if (g_dropped_frames != dropped_before)
		sst_trace("dropped", sst_tier_name(tier), n);
	else if (dn != 0)
		sst_trace("dirty", sst_tier_name(tier), n);
	else if (pal_dirty)
		sst_trace("full-palstorm", sst_tier_name(tier), n);
	else
		sst_trace("full", sst_tier_name(tier), n);

	sst_io_flush();
}
