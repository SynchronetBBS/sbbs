/* syncretro_io.c -- terminal out-buffer, enter/probe/leave, sixel present path.
 *
 * Mirrors ../syncmoo1/syncmoo1_io.c: SyncRetro owns its present function because
 * termgfx is a compositional toolkit (encode + geometry + pace), not a single
 * present() call. Ported from there: the non-blocking staged output buffer, the
 * palette-emit-only-on-change rule, the whole-frame de-dupe, the DSR-ACK
 * backpressure over termgfx/pace.c's shared AIMD controller, the bounded exit
 * drain, and the SIXELOUT capture sink. See DESIGN.md sec 6.
 *
 * TWO THINGS DIFFER from every sibling door, both because a libretro core is not
 * a fixed-format engine:
 *
 *   1. The frame is TRUECOLOR, not indexed. syncretro_quant.c reduces it to 256
 *      colors + a palette before the sixel encoder ever sees it. For a legacy
 *      console (Intellivision: 16 colors) that reduction is exact and the
 *      palette is stable, so the sixel color registers are defined once.
 *
 *   2. The frame's SIZE is not a compile-time constant, and a core may change it
 *      mid-session (RETRO_ENVIRONMENT_SET_GEOMETRY). Every buffer here grows on
 *      demand, and the image rect is recomputed whenever the source dimensions
 *      change -- not just when a probe reply narrows the canvas.
 *
 * Sink selection (env, checked once in sr_io_init()):
 *   SYNCRETRO_SIXELOUT=<path>  capture mode -- each sr_io_present() OVERWRITES
 *                              the file with one self-contained sixel frame
 *                              (palette always included, so an independent
 *                              ImageMagick decode needs no carried-over terminal
 *                              register state). For offline verification with no
 *                              live socket.
 *   (else)                     the sockfd sr_io_init() was given (or fd 1,
 *                              stdout, if it was < 0 -- dev/tty use), non-blocking.
 *
 * POSIX only for now, matching DESIGN.md sec 12's Windows-is-later scope: plain
 * write()/fcntl(), no Winsock.
 */
#include "syncretro.h"
#include "syncretro_quant.h"
#include "syncretro_audio.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "term.h"       /* termgfx: termgfx_term_enter/probe/leave */
#include "sixel.h"      /* termgfx: sixel_encode */
#include "geometry.h"   /* termgfx: termgfx_geom_fit / _center */
#include "pace.h"       /* termgfx: termgfx_aimd_update / _rtt_sample */
#include "caps.h"       /* termgfx: termgfx_query_jxl */

/* Default terminal canvas assumed until the probe reply narrows it: an 80x25
 * session at the usual 8x16 font is 640x400 pixels. */
#define SR_CANVAS_W_DEF 640
#define SR_CANVAS_H_DEF 400
#define SR_CELL_W_DEF   8
#define SR_CELL_H_DEF   16

/* Bounded blocking drain on exit: never hang the door's exit path on a wedged
 * peer, but do give the terminal-restore bytes a real chance to land. */
#define SR_LEAVE_DRAIN_MS 2000
/* How long to keep swallowing input after the restore. Long enough to catch the
 * key-release reports of whatever was held when the player quit, short enough
 * that nobody notices it on the way back to the BBS. */
#define SR_LEAVE_INPUT_MS 120

/* DSR-ACK pacing (see the block comment above sr_io_pace_ack). */
#define SR_PACE_DEADLINE_MS 750   /* reclaim the pipeline if no DSR progress for this long */
#define SR_PACE_DEPTH_MAX   8     /* beyond this we'd only buffer display lag */
#define SR_DSR_RING         16

#define SR_PAL_BYTES 768

/* --- staged output buffer + sink ------------------------------------------- */
static uint8_t *   g_out;
static size_t      g_out_len, g_out_cap, g_out_off;

static int         g_inited;
static int         g_file_mode;    /* SYNCRETRO_SIXELOUT capture mode */
static const char *g_file;
static int         g_fd = -1;      /* socket / stdout fd */

void sr_out_put(const void *buf, size_t len)
{
	if (!g_inited)
		sr_io_init(-1);
	if (len == 0)
		return;
	if (g_out_len + len > g_out_cap) {
		size_t   ncap = g_out_len + len;
		uint8_t *nb   = (uint8_t *)realloc(g_out, ncap);

		if (nb == NULL)
			return;   /* OOM here would already be fatal elsewhere; drop silently */
		g_out     = nb;
		g_out_cap = ncap;
	}
	memcpy(g_out + g_out_len, buf, len);
	g_out_len += len;
}

/* Emit a NUL-terminated control string without hand-counting its length. */
static void sr_out_puts(const char *s) { sr_out_put(s, strlen(s)); }

int sr_io_out_flush(void)
{
	if (!g_inited)
		sr_io_init(-1);

	if (g_file_mode) {
		/* Truncate-write: the capture file always holds exactly the latest
		 * frame (sr_io_present() already made it self-contained). */
		FILE *fp = fopen(g_file, "wb");

		if (fp != NULL) {
			fwrite(g_out, 1, g_out_len, fp);
			fclose(fp);
		}
		g_out_len = g_out_off = 0;
		return 0;
	}

	while (g_out_off < g_out_len) {
		ssize_t n = write(g_fd, g_out + g_out_off, g_out_len - g_out_off);

		if (n > 0) {
			g_out_off += (size_t)n;
			continue;
		}
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			break;      /* slow client: keep the remainder pending, not an error */
		sr_door_hangup("write error");   /* the single canonical hangup path */
		break;
	}
	if (g_out_off >= g_out_len)
		g_out_len = g_out_off = 0;
	return 0;
}

size_t sr_io_out_backlog(void)
{
	return g_out_len - g_out_off;
}

/* Monotonic millisecond clock: the exit drain and the DSR-ACK pace timing share
 * this one clock domain (and syncretro_door.c's deadline uses the same source). */
static uint32_t sr_io_now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)((uint64_t)t.tv_sec * 1000ULL + (uint64_t)t.tv_nsec / 1000000ULL);
}

static void sr_io_sleep_ms(int ms)
{
	struct timespec t;

	t.tv_sec  = ms / 1000;
	t.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&t, NULL);
}

/* Retry sr_io_out_flush() until the staged buffer is fully sent or `timeout_ms`
 * elapses. The fd is non-blocking, so a single flush only writes what fits in
 * the kernel send buffer right now -- this is what makes sr_io_leave()'s
 * terminal-restore bytes actually reach a slow client instead of being dropped. */
static void sr_io_drain_blocking(int timeout_ms)
{
	uint32_t start = sr_io_now_ms();

	for (;;) {
		sr_io_out_flush();
		if (g_out_off >= g_out_len)
			return;
		if ((int32_t)(sr_io_now_ms() - start) >= timeout_ms)
			return;
		sr_io_sleep_ms(15);
	}
}

/* Swallow whatever the player typed while the door had the terminal.
 *
 * The BBS shares this socket with us: every byte left unread in the receive
 * queue is read -- and echoed -- at its prompt the moment we exit. A player who
 * was building forts in Utopia otherwise returns to the BBS and watches it type
 * "3141" at them. Runs AFTER the key-mode restore, so the terminal has already
 * stopped sending physical-key reports and nothing new arrives mid-drain.
 *
 * Bounded twice: a wall-clock deadline, and a byte cap, so a terminal that
 * streams forever cannot wedge the exit path. */
static void sr_io_drain_input(int timeout_ms)
{
	uint32_t start = sr_io_now_ms();
	size_t   total = 0;
	uint8_t  buf[512];

	if (g_file_mode || g_fd < 0)
		return;                      /* capture mode: no client, and g_fd is stdout */

	for (;;) {
		ssize_t n = read(g_fd, buf, sizeof buf);

		if (n > 0) {
			total += (size_t)n;
			if (total >= (64u * 1024u))
				return;              /* absurd: stop reading, let the BBS have it */
			continue;                /* more may be pending right now */
		}
		if (n == 0)
			return;                  /* peer closed: nothing left to swallow */
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			return;                  /* a dead socket needs no draining */
		if ((int32_t)(sr_io_now_ms() - start) >= timeout_ms)
			return;
		sr_io_sleep_ms(5);           /* nothing pending: give late bytes a moment */
	}
}

/* The descriptor the input module reads from. -1 in capture mode: there is no
 * client there, and g_fd is stdout -- which, when redirected, is open write-only,
 * so a read() would fail EBADF and be mistaken for a carrier drop. */
int sr_io_get_fd(void)
{
	if (!g_inited)
		sr_io_init(-1);
	return g_file_mode ? -1 : g_fd;
}

/* --- DSR-ACK frame pacing --------------------------------------------------
 *
 * After each frame sr_io_present() ACTUALLY sends (a de-duped frame sends
 * nothing, so it needs no ack), it appends ESC[6n; the terminal's ESC[r;cR reply
 * -- parsed by syncretro_input.c's 'R' case -- reports once the terminal has
 * CONSUMED that frame. g_pace_inflight counts unacked DSRs, i.e. how many sent
 * frames the terminal hasn't drawn yet; sr_io_present() DROPS a new frame (no
 * encode, no emit, no queuing) while g_pace_inflight >= the effective depth.
 *
 * This paces to the terminal's RENDER rate, not just link bandwidth. Plain
 * socket-buffer backpressure still floods SyncTERM's sixel decoder, which is the
 * real bottleneck; an overrun decoder lags, garbles, drops the connection. That
 * matters more here than in the engine doors: a libretro core runs at its native
 * 60 fps and hands us EVERY frame, where a game engine only redraws on change.
 *
 * A stale-progress deadline (SR_PACE_DEADLINE_MS) reclaims the pipeline so a
 * terminal that never answers DSR degrades to a slideshow rather than wedging
 * the door into a permanent freeze.
 *
 * GRID-PROBE-vs-PACE-ACK: the startup grid probe (termgfx_term_probe's own
 * trailing ESC[6n, sent once from sr_io_enter()) gets its OWN ESC[r;cR reply,
 * which is NOT a pace-ack -- there is no present()-sent frame for it to retire.
 * The split is driven by sr_io_take_grid_probe(), a one-shot armed at probe-SEND
 * time: the FIRST R after the probe is the grid, every R after that is an ack.
 * Anchoring on send-time (rather than "the first R we ever see") is what makes a
 * malformed or lost grid reply safe -- the flag is consumed by that first R
 * regardless of whether it parses, so a later genuine ack can never be
 * mis-latched into sr_io_set_grid() (which would corrupt the grid AND
 * permanently leak one in-flight slot). Same reasoning as syncmoo1_io.c. */
static int      g_pace_inflight;
static int      g_pace_depth = 3;   /* starting point until the first RTT sample lands */
static uint32_t g_pace_adj_at;
static uint32_t g_pace_progress_ms;
static uint32_t g_rtt_ms, g_rtt_min, g_rtt_min_at;
static int      g_rtt_high;

static uint32_t g_dsr_ts[SR_DSR_RING];
static int      g_dsr_h, g_dsr_t;

static int      g_grid_probe_pending;

static void sr_pace_dsr_sent(uint32_t now)
{
	g_dsr_ts[g_dsr_h] = now;
	g_dsr_h = (g_dsr_h + 1) % SR_DSR_RING;
}

int sr_io_take_grid_probe(void)
{
	if (!g_grid_probe_pending)
		return 0;
	g_grid_probe_pending = 0;   /* one-shot: the first R after the probe consumes it */
	return 1;
}

void sr_io_pace_ack(void)
{
	uint32_t now = sr_io_now_ms();

	sr_trace("pace ack inflight=%d->%d", g_pace_inflight,
	         g_pace_inflight > 0 ? g_pace_inflight - 1 : 0);
	if (g_pace_inflight > 0)
		g_pace_inflight--;
	if (g_dsr_h != g_dsr_t) {                     /* match the oldest outstanding DSR */
		uint32_t rtt = now - g_dsr_ts[g_dsr_t];

		g_dsr_t = (g_dsr_t + 1) % SR_DSR_RING;
		if (termgfx_rtt_sample(&g_rtt_ms, &g_rtt_min, &g_rtt_min_at, &g_rtt_high,
		                       rtt, now, 0, 0))
			termgfx_aimd_update(1, &g_pace_depth, &g_pace_adj_at,
			                    g_rtt_ms, g_rtt_min, g_rtt_high, SR_PACE_DEPTH_MAX, now);
	}
	g_pace_progress_ms = now;
}

/* --- image geometry --------------------------------------------------------
 * Probe-driven: start at the sane 640x400 / 8x16 default so frame 1 has
 * something to fit into, and narrow to the terminal's real values once
 * syncretro_input.c's probe-reply parser calls the setters below. g_grid_* stay
 * 0 (unknown) until then, in which case the assumed 8x16 cell is used. */
static int g_canvas_w = SR_CANVAS_W_DEF;
static int g_canvas_h = SR_CANVAS_H_DEF;
static int g_grid_rows, g_grid_cols;

static int g_src_w, g_src_h;               /* the core's current frame size */
static int g_ew, g_eh;                     /* emitted (scaled) image size */
static int g_icol = 1, g_irow = 1;         /* 1-based text-cell origin of the image */
static int g_geom_ready;

/* Recompute the image rect from the current canvas/grid and the core's frame
 * size. Called when any of those change. */
static void sr_io_recompute_geom(void)
{
	int cw, ch, pagew, pageh, fith, dx, dy;

	if (g_src_w <= 0 || g_src_h <= 0)
		return;   /* no frame seen yet: nothing to fit */

	/* Real probed cell size (canvas/grid) when the grid is known; the 8x16
	 * default otherwise. */
	cw = (g_grid_cols > 0) ? g_canvas_w / g_grid_cols : SR_CELL_W_DEF;
	ch = (g_grid_rows > 0) ? g_canvas_h / g_grid_rows : SR_CELL_H_DEF;
	if (cw <= 0)
		cw = SR_CELL_W_DEF;
	if (ch <= 0)
		ch = SR_CELL_H_DEF;

	/* Derive the page from the GRID (rows * ch), not from the canvas. The two
	 * differ, and assuming they don't is what let syncmoo1's image keep landing
	 * on the last row whenever SyncTERM showed its status line: SyncTERM never
	 * answers the ESC[14t canvas probe (its CSI 't' is the CTerm-private palette
	 * setter), so g_canvas_h stays at the 400 default while the grid probe
	 * correctly reports 24 rows. Deriving the page from the grid makes the
	 * bottom-row reserve real on both. */
	pagew = (g_grid_cols > 0) ? g_grid_cols * cw : g_canvas_w;
	pageh = (g_grid_rows > 0) ? g_grid_rows * ch : g_canvas_h;
	if (pagew <= 0 || pagew > g_canvas_w)
		pagew = g_canvas_w;
	if (pageh <= 0 || pageh > g_canvas_h)
		pageh = g_canvas_h;

	/* Reserve the bottom text row: a sixel that reaches the last row scrolls the
	 * page on most terminals. Keeping the image off it is what prevents that --
	 * NOT DECSDM (?80), whose set/reset sense cterm reversed in rev 1.328, so one
	 * ?80l means opposite things on a released vs. current SyncTERM. */
	fith = pageh - ch;
	if (fith < ch)
		fith = pageh;   /* absurdly short page: don't reserve ourselves to nothing */

	termgfx_geom_fit(pagew, fith, g_src_w, g_src_h, 0, &g_ew, &g_eh);
	termgfx_geom_center(pagew, fith, g_ew, g_eh, cw, ch, &dx, &dy, &g_icol, &g_irow);
	g_geom_ready = 1;
}

void sr_io_set_canvas(int w, int h)
{
	if (w <= 0 || h <= 0)
		return;   /* malformed/partial reply: keep the current canvas */
	g_canvas_w = w;
	g_canvas_h = h;
	sr_io_recompute_geom();
}

void sr_io_set_grid(int rows, int cols)
{
	if (rows <= 0 || cols <= 0)
		return;
	g_grid_rows = rows;
	g_grid_cols = cols;
	sr_io_recompute_geom();
}

/* --- terminal setup / teardown ---------------------------------------------- */
static int g_entered;
static int g_left;

static void sr_io_enter(void)
{
	if (g_entered || g_file_mode)
		return;
	g_entered = 1;

	/* clear+home, hide cursor, no autowrap, DECSDM ?80l, shared sixel color
	 * registers (?1070l -- without which foot/xterm reset the palette per image
	 * and our define-on-change frames render transparent). */
	sr_out_puts(termgfx_term_enter);

	sr_out_puts(termgfx_term_probe);   /* pixel canvas; its ESC[999;999H+ESC[6n is the grid query */
	g_grid_probe_pending = 1;          /* arm the first-R-is-grid flag at probe-SEND time */
	sr_out_puts("\x1b[c");             /* DA1: sixel support (param 4) */
	sr_out_puts("\x1b[<c");            /* CTDA: SyncTERM detect, and cap 8 -> physical key reports */
	sr_out_puts(termgfx_query_jxl);    /* Q;JXL: JXL-tier detect (M3 consumes the reply) */
	sr_out_puts("\x1b[?u");            /* kitty keyboard query: a CSI?<flags>u reply -> true key-up */
	sr_audio_probe();                  /* SyncTERM:Q;libsndfile -> sr_audio_caps() */
	sr_io_out_flush();
}

void sr_io_leave(void)
{
	if (g_left)
		return;
	g_left = 1;

	if (g_file_mode)
		return;   /* capture mode: no terminal on the other end */

	sr_input_restore_keys();                  /* undo kitty flags / physical key reports */
	sr_out_puts(termgfx_term_leave);          /* restore ?1070h/?80h/?7h/?25h for the BBS */
	sr_io_drain_blocking(SR_LEAVE_DRAIN_MS);  /* bounded: never hang the exit path */
	sr_io_drain_input(SR_LEAVE_INPUT_MS);     /* keystrokes must not echo at the BBS prompt */
}

int sr_io_init(int sockfd)
{
	const char *s;

	if (g_inited)
		return 0;
	g_inited = 1;
	g_fd = (sockfd >= 0) ? sockfd : 1;   /* fall back to stdout (dev/tty use) */

	signal(SIGPIPE, SIG_IGN);   /* a write to a closed socket returns EPIPE, not a signal */

	if ((s = getenv("SYNCRETRO_SIXELOUT")) != NULL && *s != '\0') {
		g_file = s;
		g_file_mode = 1;
	} else {
		int fl = fcntl(g_fd, F_GETFL, 0);   /* non-blocking: a wedged client never stalls the core */

		if (fl != -1)
			fcntl(g_fd, F_SETFL, fl | O_NONBLOCK);
	}

	atexit(sr_io_leave);
	return 0;
}

/* --- scratch buffers (grown on demand: the core's frame size is not fixed) --- */
static uint8_t *g_idx;      /* quantized native frame, g_src_w * g_src_h */
static size_t   g_idx_cap;
static uint8_t *g_scaled;   /* NN-scaled indices, g_ew * g_eh */
static size_t   g_scaled_cap;
static uint8_t *g_last_rgb; /* de-dupe cache of the last SENT frame */
static size_t   g_last_rgb_cap;
static uint8_t *g_sx;       /* encoded sixel bytes */
static size_t   g_sx_cap;
static int      g_force_repaint;   /* one-shot: skip the de-dupe, re-emit the palette */

void sr_io_invalidate(void)
{
	g_force_repaint = 1;
}

static int sr_io_ensure(uint8_t **buf, size_t *cap, size_t need)
{
	if (*cap < need) {
		uint8_t *p = (uint8_t *)realloc(*buf, need);

		if (p == NULL)
			return -1;
		*buf = p;
		*cap = need;
	}
	return 0;
}

/* Nearest-neighbour scale of the quantized native frame into dst (dw x dh).
 * Purely index-domain: no palette involvement, so it can never introduce a color
 * that isn't already in the frame.
 *
 * NN only ever DUPLICATES a source pixel when it upsamples, but DROPS one
 * outright when it downsamples -- and a dropped row or column takes a 1-pixel
 * font stroke with it (this is what mangled MoO1's text in syncmoo1 before its
 * fit was constrained). Here the fit is an upscale in the ordinary case: a
 * 352x224 Intellivision frame into a 640x384 page scales up on both axes. On a
 * terminal too small for the native frame it does downsample, which is a
 * legibility cost we accept rather than refusing to draw at all. */
static void sr_io_scale_indices(uint8_t *dst, int dw, int dh,
                                const uint8_t *src, int sw, int sh)
{
	int x, y;

	for (y = 0; y < dh; ++y) {
		const uint8_t *srow = src + (size_t)(y * sh / dh) * (size_t)sw;
		uint8_t *      drow = dst + (size_t)y * (size_t)dw;

		if (dw == sw)
			memcpy(drow, srow, (size_t)sw);
		else
			for (x = 0; x < dw; ++x)
				drow[x] = srow[x * sw / dw];
	}
}

/* --- present ---------------------------------------------------------------- */
void sr_io_present(const uint8_t *rgb, int w, int h)
{
	static uint8_t last_pal[SR_PAL_BYTES];
	static int     have_pal;
	static int     have_fb;
	static int     last_icol = -1, last_irow = -1;
	static int     last_ew = -1, last_eh = -1;
	uint8_t        pal[SR_PAL_BYTES];
	size_t         npx, nrgb, n;
	int            pal_changed, emit_pal, force;

	if (w <= 0 || h <= 0)
		return;
	if (!g_inited)
		sr_io_init(-1);
	sr_io_enter();

	/* one-shot: a door screen overwrote the game area. NOT cleared here --
	 * several paths below DROP this frame without sending anything (pace
	 * backpressure, pending output, OOM), and if we cleared on read the
	 * repaint would be lost with the dropped frame, leaving the door screen
	 * on-terminal until a future frame happens to differ from the de-dupe
	 * cache. It is cleared exactly once, at each point below where a frame
	 * is ACTUALLY emitted. */
	force = g_force_repaint;

	if (w != g_src_w || h != g_src_h) {
		g_src_w = w;                 /* a core may resize mid-session (SET_GEOMETRY) */
		g_src_h = h;
		have_fb = 0;                 /* the de-dupe cache is a different shape now */
		sr_io_recompute_geom();
	}
	if (!g_geom_ready)
		return;

	npx  = (size_t)w * (size_t)h;
	nrgb = npx * 3;

	if (!g_file_mode) {
		/* Drain whatever's still pending from the last frame before deciding
		 * whether to send a new one. */
		sr_io_out_flush();

		/* Backpressure: don't outrun what the terminal has actually drawn. While
		 * `depth` frames are in flight, DROP this present -- unless the terminal
		 * has gone quiet past the deadline, in which case reclaim the pipeline so
		 * a silent client can't wedge the door into a permanent freeze. */
		if (g_pace_inflight >= g_pace_depth) {
			if ((int32_t)(sr_io_now_ms() - g_pace_progress_ms) > SR_PACE_DEADLINE_MS) {
				sr_trace("pace reclaim inflight=%d depth=%d", g_pace_inflight, g_pace_depth);
				g_pace_inflight = 0;
				g_dsr_t = g_dsr_h;   /* drop the stale unacked DSR timestamps */
			} else {
				sr_trace("pace drop inflight=%d depth=%d", g_pace_inflight, g_pace_depth);
				return;              /* still behind: drop, don't queue */
			}
		}
		if (g_out_off < g_out_len) {
			sr_trace("pace draining %zu/%zu", g_out_off, g_out_len);
			return;                  /* prior frame's bytes not all out yet */
		}

		/* De-dupe on the SOURCE frame, before paying for quantization: at 60 fps
		 * a paused game hands us the same pixels over and over. Compared against
		 * the last frame actually SENT, plus the draw position and emitted size
		 * (a resize can change the encode while leaving the cell origin at 1;1). */
		if (!force
		    && have_fb && last_icol == g_icol && last_irow == g_irow
		    && last_ew == g_ew && last_eh == g_eh
		    && memcmp(g_last_rgb, rgb, nrgb) == 0) {
			sr_trace("dedupe identical-frame");
			return;
		}
	}

	if (sr_io_ensure(&g_idx, &g_idx_cap, npx) != 0)
		return;                      /* OOM: skip this frame, keep running */
	sr_quant_rgb_to_indexed(rgb, w, h, g_idx, pal);

	if (sr_io_ensure(&g_scaled, &g_scaled_cap, (size_t)g_ew * (size_t)g_eh) != 0)
		return;
	sr_io_scale_indices(g_scaled, g_ew, g_eh, g_idx, w, h);

	if (g_file_mode) {
		/* Capture mode: always a self-contained frame (palette included), no
		 * cursor wrap and no DSR (there is no live terminal session), so the file
		 * decodes standalone. Bypasses the de-dupe and the pacing above -- each
		 * captured frame is meant to stand alone, not be skipped as a duplicate. */
		n = sixel_encode(&g_sx, &g_sx_cap, g_scaled, g_ew, g_eh, pal, 1);
		sr_out_put(g_sx, n);
		g_force_repaint = 0;   /* frame actually emitted (to the capture file) */
		sr_io_out_flush();
		return;
	}

	pal_changed = force || !have_pal || memcmp(last_pal, pal, sizeof last_pal) != 0;

	/* Palette (re)definition. SyncTERM persists sixel color registers across
	 * images, and re-defining all 256 every frame is what garbles its decoder
	 * (sixel.h's emit_palette doc) -- so there, define-on-change only.
	 *
	 * EVERYWHERE ELSE, re-send the palette every frame. Register persistence is
	 * not a given: foot defaults to PRIVATE color registers, seeding each image's
	 * palette from just 16 default colors, so a palette-less frame's indices
	 * 16-255 land on transparent entries and the text grid shows through.
	 * term_enter asks for shared registers (?1070l), which fixes foot and xterm,
	 * but a terminal that ignores mode 1070 would still be wrong.
	 *
	 * Unknown terminal (no probe reply yet) counts as not-SyncTERM: a few early
	 * frames then carry a redundant palette, which is never WRONG. */
	emit_pal = pal_changed || !sr_input_is_syncterm();

	{   /* Save cursor, position at the centered cell, restore after -- the sixel
		 * is drawn at the text cursor, so this is what centers it without
		 * disturbing wherever the real cursor was. */
		char wrap[24];
		int  wn = snprintf(wrap, sizeof wrap, "\x1b" "7\x1b[%d;%dH", g_irow, g_icol);

		/* A forced repaint means something painted OVER the game -- the door's
		 * pause and help screens write text across the whole grid. The sixel
		 * covers only the centered image rect, so the margin cells around it
		 * would keep the leftovers (at 80x25 with a 352-wide frame, the first
		 * and last few columns). Erase first, in the SAME write as the repaint,
		 * so the clear and the frame reach the terminal together. */
		if (force)
			sr_out_puts("\x1b[2J");
		if (wn > 0)
			sr_out_put(wrap, (size_t)wn);
	}

	n = sixel_encode(&g_sx, &g_sx_cap, g_scaled, g_ew, g_eh, pal, emit_pal);
	sr_out_put(g_sx, n);
	sr_out_put("\x1b" "8", 2);   /* restore cursor */
	g_force_repaint = 0;   /* frame actually emitted: the one-shot is spent */

	if (emit_pal) {
		memcpy(last_pal, pal, sizeof last_pal);
		have_pal = 1;
	}
	if (sr_io_ensure(&g_last_rgb, &g_last_rgb_cap, nrgb) == 0) {
		memcpy(g_last_rgb, rgb, nrgb);
		have_fb = 1;
	}
	last_icol = g_icol;
	last_irow = g_irow;
	last_ew   = g_ew;
	last_eh   = g_eh;

	sr_out_put("\x1b[6n", 4);    /* DSR: the terminal reports once it has CONSUMED this frame */
	g_pace_inflight++;
	sr_trace("sent frame inflight=%d depth=%d", g_pace_inflight, g_pace_depth);
	/* g_pace_progress_ms (the deadline-reclaim clock) is refreshed on BOTH a send
	 * (here) and an ack. Harmless: once backpressure engages, sends STOP until an
	 * ack frees a slot, so the deadline then measures time-since-last-ACK --
	 * exactly what the reclaim wants (a terminal that stopped answering DSR). */
	g_pace_progress_ms = sr_io_now_ms();
	sr_pace_dsr_sent(g_pace_progress_ms);

	sr_io_out_flush();
}
