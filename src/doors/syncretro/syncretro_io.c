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
 *   (else)                     the sockfd sr_io_init() was given (or the plat
 *                              fallback -- fd 1/stdout on POSIX, none on
 *                              Windows -- if it was < 0), non-blocking.
 *
 * All descriptor I/O, the monotonic clock, and the sleep go through
 * syncretro_plat.h, so this file carries no #ifdef and no direct syscall: on
 * Windows the DOOR32.SYS handle is a Winsock SOCKET (send/recv, ioctlsocket),
 * on POSIX a plain fd (write/read, fcntl) -- the seam hides which.
 */
#include "syncretro.h"
#include "syncretro_quant.h"
#include "syncretro_audio.h"
#include "syncretro_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "term.h"       /* termgfx: termgfx_term_enter/probe/leave */
#include "sixel.h"      /* termgfx: sixel_encode */
#include "geometry.h"   /* termgfx: termgfx_geom_fit / _center */
#include "pace.h"       /* termgfx: termgfx_aimd_update / _rtt_sample */
#include "caps.h"       /* termgfx: termgfx_query_jxl */
#include "text.h"       /* termgfx: rt_config / rt_render_frame -- the text tiers */
#include "charset.h"    /* termgfx: the client's charset (CP437 vs UTF-8) */

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
static int         g_fd = -1;      /* the fd we WRITE to: socket, stdout, or tty */
/* The fd we READ from. Identical to g_fd for a socket door -- a socket is
 * bidirectional -- but NOT for a stdio door, where the BBS gives us its pipes:
 * stdin is 0 and stdout is 1, and reading fd 1 would read nothing forever. */
static int         g_fd_in = -1;

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
	sr_trace_wire(buf, len);   /* diagnostic: capture exactly what we will send */
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
		int n = sr_plat_write(g_fd, g_out + g_out_off, g_out_len - g_out_off);

		if (n > 0) {
			g_out_off += (size_t)n;
			continue;
		}
		if (n == SR_IO_INTR)
			continue;
		if (n == SR_IO_AGAIN)
			break;      /* slow client: keep the remainder pending, not an error */
		sr_door_hangup("write error");   /* n < 0 (dead socket), or a 0-byte write
		                                  * that can't happen with len > 0 anyway */
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
 * this one clock domain (and syncretro_door.c's deadline uses the same source,
 * sr_plat_now_ms). */
#define sr_io_now_ms()    sr_plat_now_ms()
#define sr_io_sleep_ms(ms) sr_plat_sleep_ms(ms)

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
		int n = sr_plat_read(g_fd, buf, sizeof buf);

		if (n > 0) {
			total += (size_t)n;
			if (total >= (64u * 1024u))
				return;              /* absurd: stop reading, let the BBS have it */
			continue;                /* more may be pending right now */
		}
		if (n == 0)
			return;                  /* peer closed: nothing left to swallow */
		if (n == SR_IO_ERROR)
			return;                  /* a dead socket needs no draining */
		if ((int32_t)(sr_io_now_ms() - start) >= timeout_ms)
			return;
		sr_io_sleep_ms(5);           /* nothing pending: give late bytes a moment */
	}
}

/* The descriptor the input module reads from. -1 in capture mode: there is no
 * client there, and g_fd is stdout -- which, when redirected, is open write-only,
 * so a read() would fail EBADF and be mistaken for a carrier drop. */
/* The fd to READ from. The only consumer is sr_input_pump(). */
int sr_io_in_fd(void)
{
	if (!g_inited)
		sr_io_init(-1);
	return g_fd_in;
}

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
/* The DISPLAY aspect (width/height) the core asks for, or 0 = "assume square
 * pixels". A console's pixels are not square: the NES's 256x240 framebuffer was
 * shown on a 4:3 television, and fceumm reports 1.219 (the 8:7 pixel-aspect
 * convention) -- fit it as 256:240 = 1.067 and the picture is a quarter too
 * narrow, which is exactly what a player sees. FreeIntv hid this bug for a whole
 * milestone: its 352x224 IS 1.5714 square, the same number it reports, so
 * ignoring the aspect happened to be right on the only console we had. */
static double g_par;
static int    g_canvas_known;              /* the terminal ANSWERED ESC[14t (vs. our guess) */
static int    g_ew, g_eh;                  /* emitted (scaled) image size */
static int    g_icol = 1, g_irow = 1;      /* 1-based text-cell origin of the image */
static int    g_geom_ready;


/* --- render tiers ------------------------------------------------------------
 *
 * SIXEL is the picture; the TEXT tiers are the fallback that always works. A
 * terminal with no sixel (and there are plenty -- conhost, PuTTY, an ancient
 * client on a real serial line) can still play the game rendered out of block
 * characters, in color, at whatever detail its font can carry.
 *
 * The tiers are built from what the CLIENT can actually do, not from a menu of
 * everything termgfx knows:
 *
 *   sixel           only if the terminal answered DA1 param 4. Otherwise the door
 *                   starts on text rather than drawing into the void.
 *   half-block      one glyph, two pixels (upper/lower). Renders in EITHER charset
 *                   -- CP437 0xDF, UTF-8 U+2580 -- so every client gets it.
 *   blocks+shades   2x2 detail from the classic CP437 block/shade glyphs only, so
 *                   it renders on font-limited terminals (conhost) where the
 *                   quadrant glyphs come out as missing-glyph boxes.
 *   quadrant        2x2 detail, exact, via U+2596-259F.   UTF-8 ONLY.
 *   sextant         2x3 detail via U+1FB00.               UTF-8 ONLY.
 *
 * The last two are offered only to a UTF-8 client: cycling a CP437 SyncTERM
 * through them would step it through two screens of missing-glyph boxes.
 */
void sr_io_toast(const char *text);   /* defined below; the tier cycle announces itself */

typedef struct {
	int text;               /* 0 = sixel (pixels); 1 = one of the text tiers */
	rt_mode_t rt;
	rt_charset_t cs;
	const char * label;
} sr_tier_t;

static sr_tier_t g_tiers[6];
static int       g_ntiers;
static int       g_tier_i;
static int       g_tier_chosen;          /* the PLAYER picked this one (F4) -- do not override */
static int       g_text_cols, g_text_rows;   /* what rt_config() was last given */

static void sr_tier_add(int text, rt_mode_t rt, rt_charset_t cs, const char *label)
{
	if (g_ntiers >= (int)(sizeof g_tiers / sizeof g_tiers[0]))
		return;
	g_tiers[g_ntiers].text  = text;
	g_tiers[g_ntiers].rt    = rt;
	g_tiers[g_ntiers].cs    = cs;
	g_tiers[g_ntiers].label = label;
	g_ntiers++;
}

/* Built once the probe replies have had their say (so g_have_sixel and the charset
 * are known) -- and REBUILT if sixel turns up afterwards.
 *
 * A terminal that misses the settle window (a slow link, a client that answers
 * DA1 in its own time) would otherwise be stuck on a text tier forever, with no
 * sixel in the cycle to find: the list is built once, and it was built from an
 * answer that had not arrived. So if we later learn the terminal CAN draw, the
 * list is rebuilt with sixel in it -- and the player is left on the tier he is
 * already looking at, not yanked to a different one behind his back. */
static void sr_tiers_build(void)
{
	rt_charset_t cs = (termgfx_client_charset() == TERMGFX_UTF8) ? RT_UTF8 : RT_CP437;
	const char * had = NULL;

	if (g_ntiers > 0) {
		if (g_tiers[0].text == 0 || !sr_input_has_sixel())
			return;                      /* sixel is already offered, or still absent */
		had     = g_tiers[g_tier_i].label;   /* the tier the player is on right now */
		g_ntiers = 0;                        /* rebuild: sixel arrived late */
	}

	if (sr_input_has_sixel())
		sr_tier_add(0, 0, cs, "sixel");
	sr_tier_add(1, RT_HALF,   cs, "half-block");
	sr_tier_add(1, RT_BLOCKS, cs, "blocks+shades");
	if (cs == RT_UTF8) {
		sr_tier_add(1, RT_QUADRANT, RT_UTF8, "quadrant");
		sr_tier_add(1, RT_SEXTANT,  RT_UTF8, "sextant");
	}

	g_tier_i = 0;                        /* index 0 = the best the client can take */

	/* A REBUILD -- sixel turned up late, past the settle window (a slow link, a
	 * client that answers in its own time). Two cases, and they are not the same:
	 *
	 *   the player has not touched F4  ->  he is on the fallback only because we
	 *                                      did not know better yet. Give him the
	 *                                      picture: adopt the best tier (index 0).
	 *   the player HAS pressed F4      ->  he chose this tier. Leave him on it, and
	 *                                      let him find sixel in the cycle himself.
	 */
	if (had != NULL && g_tier_chosen) {
		int i;

		for (i = 0; i < g_ntiers; i++) {
			if (strcmp(g_tiers[i].label, had) == 0) {
				g_tier_i = i;
				break;
			}
		}
	}
	if (had != NULL && strcmp(g_tiers[g_tier_i].label, had) != 0) {
		g_text_cols = g_text_rows = 0;   /* the renderer changed under us */
		rt_invalidate();
		sr_io_invalidate();
	}
}

const char *sr_io_tier_name(void)
{
	return (g_ntiers > 0) ? g_tiers[g_tier_i].label : "sixel";
}

/* F4. Step to the next tier and repaint from scratch: the two kinds of output do
 * not overwrite one another -- a sixel is pixels laid over the cell grid, and the
 * text tiers write cells -- so whichever was on screen has to be erased, and the
 * cell-diff shadow (which believes it knows what is on the terminal) has to be
 * thrown away with it. */
void sr_io_cycle_tier(void)
{
	if (g_ntiers < 2)
		return;                          /* nothing to cycle to */
	g_tier_i     = (g_tier_i + 1) % g_ntiers;
	g_tier_chosen = 1;                   /* his choice now: a late probe must not undo it */
	g_text_cols  = g_text_rows = 0;      /* force an rt_config() on the next frame */
	rt_invalidate();                     /* the shadow describes a screen we are erasing */
	sr_io_invalidate();                  /* -> ESC[2J + a full frame */
	sr_io_toast(sr_io_tier_name());
}

/* Recompute the image rect from the current canvas/grid and the core's frame
 * size. Called when any of those change. */
static void sr_io_recompute_geom(void)
{
	double cw, ch;
	int    pagew, pageh, fith, fitw, dx, dy;
	int    was_col = g_icol, was_row = g_irow, was_ew = g_ew, was_eh = g_eh;

	if (g_src_w <= 0 || g_src_h <= 0)
		return;   /* no frame seen yet: nothing to fit */

	/* THE CELL IS FRACTIONAL, AND ROUNDING IT DOWN IS WHAT PUTS THE PICTURE OFF
	 * CENTER. A cell is canvas/grid: 1648 px over 164 columns is 10.05, and on a
	 * display at 125%/150% scaling the fraction is far bigger. Truncate it and the
	 * page (cols * cell) comes out up to `cols` pixels narrower THAN THE SCREEN --
	 * ~100 px on a wide terminal -- so centering inside that page lands the image
	 * half the error to the left. That is the "not quite centered" a player sees.
	 * Keep the fraction; quantize once, at the very end, where the text cursor
	 * genuinely forces a whole cell. */
	if (g_canvas_known && g_grid_cols > 0 && g_grid_rows > 0) {
		cw    = (double)g_canvas_w / g_grid_cols;
		ch    = (double)g_canvas_h / g_grid_rows;
		pagew = g_canvas_w;          /* the terminal told us: the page IS the canvas */
		pageh = g_canvas_h;
	} else {
		/* The canvas is a GUESS -- SyncTERM never answers the ESC[14t probe (its
		 * CSI 't' is the CTerm-private palette setter), so g_canvas_* is still the
		 * 640x400 default. NEVER center against a guess. The GRID is real, so take
		 * the default cell size as truth and derive the page from that: SyncTERM's
		 * 80x24 comes out at exactly 640x384, which is what it actually is, and the
		 * bottom-row reserve below stays real. (Deriving the page from the guessed
		 * canvas is what used to land syncmoo1's image on the last row whenever
		 * SyncTERM showed its status line.) */
		cw    = SR_CELL_W_DEF;
		ch    = SR_CELL_H_DEF;
		pagew = (g_grid_cols > 0) ? (int)(g_grid_cols * cw) : g_canvas_w;
		pageh = (g_grid_rows > 0) ? (int)(g_grid_rows * ch) : g_canvas_h;
	}
	if (cw <= 0.0)
		cw = SR_CELL_W_DEF;
	if (ch <= 0.0)
		ch = SR_CELL_H_DEF;
	if (pagew <= 0)
		pagew = g_canvas_w;
	if (pageh <= 0)
		pageh = g_canvas_h;

	/* Reserve the bottom text row: a sixel that reaches the last row scrolls the
	 * page on most terminals. Keeping the image off it is what prevents that --
	 * NOT DECSDM (?80), whose set/reset sense cterm reversed in rev 1.328, so one
	 * ?80l means opposite things on a released vs. current SyncTERM. */
	fith = pageh - (int)(ch + 0.5);
	if (fith < (int)ch)
		fith = pageh;   /* absurdly short page: don't reserve ourselves to nothing */

	/* Fit the frame at its DISPLAY shape, not its pixel shape: widen (or narrow)
	 * the source to the aspect the core asks for, and let the resampler stretch
	 * into the result. The height is left alone -- a terminal has far fewer rows
	 * than columns to spend, so scaling x is the cheaper axis to distort. */
	fitw = g_src_w;
	if (g_par > 0.0) {
		int aw = (int)((double)g_src_h * g_par + 0.5);

		if (aw > 0)
			fitw = aw;
	}

	termgfx_geom_fit(pagew, fith, fitw, g_src_h, 0, &g_ew, &g_eh);
	termgfx_geom_center_ex(pagew, fith, g_ew, g_eh, cw, ch, &dx, &dy, &g_icol, &g_irow);
	g_geom_ready = 1;

	if (g_icol == was_col && g_irow == was_row && g_ew == was_ew && g_eh == was_eh)
		return;                    /* same rect: nothing moved, nothing to erase */

	/* THE IMAGE RECT MOVED, SO THE OLD ONE MUST BE ERASED -- and only a forced
	 * repaint does that (it emits ESC[2J ahead of the frame).
	 *
	 * This is not a corner case, it is EVERY SESSION. The door draws its first
	 * frames against the assumed 80x25/640x400 canvas, because the terminal's
	 * probe replies take a network round-trip to come back; when they land, the
	 * image is re-fitted to the real canvas and moves. A sixel is PIXELS, not
	 * cells: nothing about drawing the new image erases the old one, so whatever
	 * the small first image painted outside the big one's rect stays on screen for
	 * the rest of the session. On a 164-column terminal that leftover is a grey
	 * slab down the left of the picture -- the console's power-on frame, which is
	 * what the core was still showing when the first frames went out.
	 *
	 * The invalidate has to live HERE and not in the callers: sr_io_set_aspect()
	 * remembered to call it, sr_io_set_canvas() and sr_io_set_grid() -- the two
	 * that fire on every single session -- did not. A choke point cannot be
	 * forgotten. */
	sr_io_invalidate();

	fprintf(stderr, "syncretro: geometry canvas %dx%d%s grid %dx%d cell %.2fx%.2f"
	        " page %dx%d image %dx%d at cell %d,%d\n",
	        g_canvas_w, g_canvas_h, g_canvas_known ? "" : " (assumed)",
	        g_grid_cols, g_grid_rows, cw, ch, pagew, fith, g_ew, g_eh,
	        g_icol, g_irow);
}

/* The core's display aspect, from retro_get_system_av_info() (possibly overridden
 * by [video] aspect). Call once after the game loads, before the first frame. */
void sr_io_set_aspect(double aspect)
{
	if (aspect < 0.1 || aspect > 10.0)
		aspect = 0.0;              /* nonsense: fall back to square pixels */
	if (g_par == aspect)
		return;
	g_par = aspect;
	sr_io_recompute_geom();
	sr_io_invalidate();            /* the image rect moved: repaint in full */
}

void sr_io_set_canvas(int w, int h)
{
	if (w <= 0 || h <= 0)
		return;   /* malformed/partial reply: keep the current canvas */
	g_canvas_w     = w;
	g_canvas_h     = h;
	g_canvas_known = 1;   /* a real reply: the geometry may now trust the canvas */
	sr_io_recompute_geom();
}

/* The terminal's text grid, for the door's own text screens (pause/help). 0 for
 * either means the terminal never told us -- the caller falls back to 80x24. */
void sr_io_grid(int *rows, int *cols)
{
	if (rows != NULL)
		*rows = g_grid_rows;
	if (cols != NULL)
		*cols = g_grid_cols;
}

void sr_io_set_grid(int rows, int cols)
{
	if (rows <= 0 || cols <= 0)
		return;
	g_grid_rows = rows;
	g_grid_cols = cols;
	sr_io_recompute_geom();
}

/* --- transient toast (volume) ------------------------------------------------
 *
 * One line on the BOTTOM text row -- the row sr_io_recompute_geom() already
 * reserves and keeps clear of the image, so a message there can never disturb
 * the picture or scroll the page. Shown for a moment and then erased. It exists
 * because a volume key that changes nothing you can SEE is indistinguishable
 * from a volume key that does not work. */
#define SR_TOAST_MS 1500

static char     g_toast[64];
static uint32_t g_toast_until;
static int      g_toast_drawn;

static int sr_toast_row(void)
{
	return (g_grid_rows > 0) ? g_grid_rows : 24;
}

static void sr_toast_draw(const char *text)
{
	char ov[128];
	int  n = snprintf(ov, sizeof ov, "\x1b[%d;1H\x1b[1;37;44m %s \x1b[K\x1b[0m",
	                  sr_toast_row(), text);

	if (n > 0)
		sr_out_put(ov, (size_t)n);
	sr_io_out_flush();
}

static void sr_toast_clear(void)
{
	char ov[32];
	int  n = snprintf(ov, sizeof ov, "\x1b[%d;1H\x1b[0m\x1b[K", sr_toast_row());

	if (n > 0)
		sr_out_put(ov, (size_t)n);
	sr_io_out_flush();
}

void sr_io_toast(const char *text)
{
	if (g_file_mode || g_fd < 0 || text == NULL)
		return;
	snprintf(g_toast, sizeof g_toast, "%s", text);
	g_toast_until = sr_io_now_ms() + SR_TOAST_MS;
	sr_toast_draw(g_toast);
	g_toast_drawn = 1;
}

/* Erase the toast once its moment has passed. Called once per frame. */
static void sr_io_stats_emit(int force);

static void sr_toast_tick(void)
{
	if (!g_toast_drawn)
		return;
	if ((int32_t)(sr_io_now_ms() - g_toast_until) < 0)
		return;
	sr_toast_clear();
	g_toast_drawn = 0;
	sr_io_stats_emit(1);   /* the strip shares this row: give it straight back */
}

/* --- live stats overlay (Ctrl-S) -------------------------------------------
 *
 * A one-line strip on the BOTTOM text row. It shows the render tier, recent frame
 * rate, transmit throughput to the player, round-trip (current / baseline-min)
 * and the effective pipeline depth -- the very signals that drive the DSR-ACK
 * pacing -- plus the negotiated keyboard mode. Like SyncDOOM's, it redraws only
 * when the text changes, so refreshing it over a de-duped frame is nearly free.
 *
 * THE BOTTOM ROW IS THE ONLY PLACEMENT THAT WORKS. sr_io_recompute_geom() already
 * reserves that row (a sixel reaching the last row scrolls the page), so nothing
 * else ever paints there -- the strip cannot be overwritten by the image, and it
 * cannot disturb it. The overlay used to sit top-right, over the image's top
 * margin, which is exactly where ../syncconquer found it UNREADABLE: sixel shares
 * the text plane and repaints with a slow progressive pass, so a strip drawn into
 * it flickers and tears. Same conclusion, same fix.
 *
 * It shares the row with the volume toast, which takes precedence while it is up
 * (sr_toast_tick() forces a redraw when the toast expires). */
static int      g_stats_on;
static char     g_ov_last[80];             /* last strip text drawn (skip redundant redraws) */
static int      g_ov_prev_tn;              /* previous strip width (to blank cells on narrowing) */

/* Frame-rate / throughput over a rolling ~2s window (SyncDOOM's model). */
static uint32_t g_fps_win_at, g_fps_win_n;
static uint64_t g_fps_win_bytes;
static uint32_t g_recent_fps, g_recent_kbps;

/* Count a frame ACTUALLY sent, and its wire byte size. */
static void sr_stats_add_frame(uint32_t bytes)
{
	g_fps_win_n++;
	g_fps_win_bytes += bytes;
}

/* Advance the metrics window once per frame (from sr_io_stats_tick). */
static void sr_stats_window(void)
{
	uint32_t nm = sr_io_now_ms();

	if (g_fps_win_at == 0) {
		g_fps_win_at = nm;
		return;
	}
	if ((int32_t)(nm - g_fps_win_at) >= 2000) {
		uint32_t span = nm - g_fps_win_at;

		g_recent_fps  = (uint32_t)(g_fps_win_n * 1000u / span);
		g_recent_kbps = (uint32_t)(g_fps_win_bytes * 1000u / 1024u / span);
		g_fps_win_n     = 0;
		g_fps_win_bytes = 0;
		g_fps_win_at    = nm;
	}
}

/* Draw / refresh the strip. force=1 always redraws (a fresh frame just painted
 * over it); force=0 redraws only when the readout text changed. */
static void sr_io_stats_emit(int force)
{
	char txt[80], ov[192];
	int  tn, col, ovn, aw;

	/* Guard HERE, not in each caller. It used to be the callers' job, and the
	 * moment a new one appeared -- sr_toast_tick(), handing the shared bottom row
	 * back after a volume readout -- the strip painted itself over a session that
	 * had never pressed Ctrl-S. */
	if (!g_stats_on)
		return;
	if (g_file_mode || g_fd < 0)
		return;   /* no live terminal to draw on */

	/* tier fps KB/s lag rtt/min depth kbd. The tier is no longer a constant: F4
	 * cycles it, and which one you are looking at is exactly the thing you want to
	 * know while comparing them. */
	tn = snprintf(txt, sizeof txt, " %s %ufps %uKB/s lag %u/%ums depth %d %s%s ",
	              sr_io_tier_name(), g_recent_fps, g_recent_kbps, g_rtt_ms, g_rtt_min,
	              g_pace_depth, sr_input_keymode_name(),
	              sr_audio_blob_active() ? " a-blob" : "");   /* audio streaming inline (A;LoadBlob) */

	if (g_toast_drawn)
		return;   /* the volume readout owns the row for its moment */

	if (!force && strcmp(txt, g_ov_last) == 0)
		return;
	if (tn > 0 && tn < (int)sizeof g_ov_last)
		strcpy(g_ov_last, txt);

	/* Bottom row, from column 1, with an erase-to-end-of-line so a shrinking
	 * readout leaves no tail behind (no blank-the-uncovered-cells dance, and no
	 * cursor save/restore needed: nothing else draws on this row). */
	(void)col; (void)aw;
	/* ESC[K BEFORE the reset, not before the attribute. Erase-to-end-of-line paints
	 * with the CURRENT background (BCE), so running it while the cyan is set
	 * finishes the row as a clean full-width bar AND wipes the tail of a previous,
	 * longer readout in the same stroke. Erasing first (which is what this did)
	 * cleared the row to BLACK and then painted a short cyan stub on it -- the strip
	 * ended wherever the text did. The sibling doors have always done it this way
	 * (syncdoom.c, syncconquer's door_io.c). */
	ovn = snprintf(ov, sizeof ov, "\x1b[%d;1H\x1b[30;46m%s\x1b[K\x1b[0m",
	               sr_toast_row(), txt);
	g_ov_prev_tn = tn;
	if (ovn > 0) {
		sr_out_put(ov, (size_t)ovn);
		sr_io_out_flush();
	}
}

void sr_io_stats_toggle(void)
{
	g_stats_on   = !g_stats_on;
	g_ov_last[0] = '\0';
	g_ov_prev_tn = 0;
	if (g_stats_on) {
		sr_io_stats_emit(1);    /* draw it now */
		return;
	}
	/* ERASE it. The strip lives on the reserved bottom row, where the image never
	 * paints -- so invalidating the frame (what this used to do, back when the
	 * strip sat over the image's top margin) would repaint everything EXCEPT the
	 * row the strip is on, and it would sit there for the rest of the session. */
	if (!g_toast_drawn)
		sr_toast_clear();
}

/* THE FRAME JUST PAINTED OVER THE BOTTOM ROW -- give it back.
 *
 * A forced repaint emits ESC[2J, which erases the whole page, the reserved bottom
 * row included. Whatever was on that row (the stats strip, or a toast that is
 * still inside its moment) has to be redrawn on top of the fresh frame, or it
 * simply vanishes: the strip only ever repaints when its TEXT changes, so after a
 * clear it stays blank until the frame counter ticks over -- about a second of
 * nothing, which is exactly what an F4 tier switch looked like.
 *
 * The sixel path had half of this (it redrew the strip every frame); the text path
 * had none of it, and neither path ever put the toast back. One function now, and
 * both call it. */
static void sr_io_overlay_after_frame(int cleared)
{
	if (g_toast_drawn) {
		if (cleared)
			sr_toast_draw(g_toast);   /* the clear ate it; it is still its moment */
		return;                       /* the toast owns the row either way */
	}
	if (g_stats_on)
		sr_io_stats_emit(1);
}

void sr_io_stats_tick(void)
{
	sr_toast_tick();   /* erase a volume readout whose moment has passed */

	sr_stats_window();          /* keep the metrics live even while it's off */
	if (g_stats_on)
		sr_io_stats_emit(0);    /* refresh the readout (redraws only on change) */
}

/* --- terminal setup / teardown ---------------------------------------------- */
static int      g_entered;
static int      g_left;
static uint32_t g_enter_ms;

/* How long to hold the FIRST frame while the terminal's probe replies come back.
 *
 * The replies cost a network round-trip, and until the grid one lands the door is
 * guessing 80x25. Painting during that window means painting an image that is
 * about to move -- a visible jump, and (before the invalidate above) a permanent
 * smear of leftover pixels. Holding for a moment costs a few black frames that
 * nobody sees, because "Loading SyncRetro..." is still on the screen.
 *
 * It is a CEILING, not a wait: the moment the grid reply arrives the door starts
 * drawing. The timeout only bounds a terminal that never answers at all, which
 * then gets the 80x25 default it deserves. */
#define SR_GEOM_SETTLE_MS   400

static void sr_io_enter(void)
{
	if (g_entered || g_file_mode)
		return;
	g_entered = 1;
	g_enter_ms = sr_io_now_ms();

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

/* A SOCKET door: one bidirectional descriptor, read and written. */
int sr_io_init(int sockfd)
{
	return sr_io_init_fds(sockfd, sockfd);
}

/* The general form. A STDIO door (the BBS redirected our stdin/stdout instead of
 * handing us a socket -- Mystic on *nix) passes 0 and 1, which are two DIFFERENT
 * descriptors and only half-duplex each. Everything downstream of here already
 * treats reading and writing separately, so this is the whole of it. */
int sr_io_init_fds(int in_fd, int out_fd)
{
	const char *s;

	if (g_inited)
		return 0;
	g_inited = 1;
	/* Fall back to the plat's dev sink: stdout on POSIX, none (-1) on Windows,
	 * where fd 1 is not a Winsock SOCKET. */
	g_fd    = (out_fd >= 0) ? out_fd : sr_plat_fallback_fd();
	g_fd_in = in_fd;

	sr_plat_net_init();   /* WSAStartup / SIGPIPE-ignore; idempotent, defensive here */

	if ((s = getenv("SYNCRETRO_SIXELOUT")) != NULL && *s != '\0') {
		g_file = s;
		g_file_mode = 1;
	} else {
		/* non-blocking + TCP_NODELAY: a wedged client never stalls the core.
		 * Both ends: on a stdio door the read side is a different descriptor, and
		 * a BLOCKING stdin would stall the emulator for a whole frame every time
		 * the player is not typing -- which is nearly always. */
		(void)sr_plat_sock_setup(g_fd);
		if (g_fd_in >= 0 && g_fd_in != g_fd)
			(void)sr_plat_sock_setup(g_fd_in);
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
	size_t         npx, nrgb, n, frame_start = 0;
	int            pal_changed, emit_pal, force;

	if (w <= 0 || h <= 0)
		return;
	if (!g_inited)
		sr_io_init(-1);
	sr_io_enter();

	/* Hold the first frame until the terminal has told us TWO things -- how big it
	 * is, and whether it can draw a picture -- or until SR_GEOM_SETTLE_MS says it
	 * never will. The core keeps running; we just don't paint yet.
	 *
	 * BOTH answers, not just the grid. The door asks for the grid FIRST (the
	 * ESC[999;999H + ESC[6n inside termgfx_term_probe) and for the device
	 * attributes second, so the grid reply comes back first -- and this gate used
	 * to open on it alone. The very next frame then built the tier list while the
	 * DA1 reply ("I do sixel") was still in flight, read "no sixel", and started
	 * the player on a TEXT tier with no sixel in the cycle at all. Whether the two
	 * replies landed in the same read was a matter of packet boundaries, which is
	 * why sixel would appear once and then never again. */
	if (!g_file_mode && (g_grid_cols <= 0 || !sr_input_probe_replied())
	    && (uint32_t)(sr_io_now_ms() - g_enter_ms) < SR_GEOM_SETTLE_MS)
		return;

	sr_tiers_build();

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

	/* A TEXT TIER: termgfx's block-character renderer owns the whole path from
	 * here -- it scales, quantizes to the cell grid, and emits only the cells that
	 * CHANGED (its own shadow buffer), so none of the sixel machinery below (quant,
	 * scale, palette, the centered-cursor wrap) applies. */
	if (g_ntiers > 0 && g_tiers[g_tier_i].text) {
		const char *cells;
		size_t      cn;
		int         tcols = (g_grid_cols > 0) ? g_grid_cols : 80;
		int         trows = (g_grid_rows > 0) ? g_grid_rows : 24;

		/* Reserve the bottom row, exactly as the sixel path does: it belongs to the
		 * stats strip and the volume toast, and a renderer that owned it would
		 * repaint over them every frame. */
		if (trows > 1)
			trows--;

		if (tcols != g_text_cols || trows != g_text_rows) {
			/* 24-bit on SyncTERM (cterm speaks truecolor), 256 elsewhere -- the same
			 * call the sibling doors make. */
			rt_config(tcols, trows, g_tiers[g_tier_i].rt,
			          sr_input_is_syncterm() ? RT_24BIT : RT_8BIT,
			          g_tiers[g_tier_i].cs);
			g_text_cols = tcols;
			g_text_rows = trows;
			force       = 1;             /* new grid: the shadow is meaningless */
		}
		if (force)
			rt_invalidate();             /* something painted over us: repaint every cell */

		frame_start = g_out_len;
		if (force)
			sr_out_puts("\x1b[2J");
		cells = rt_render_frame(rgb, w, h, &cn);
		if (cells != NULL && cn > 0)
			sr_out_put(cells, cn);
		g_force_repaint = 0;

		if (sr_io_ensure(&g_last_rgb, &g_last_rgb_cap, nrgb) == 0) {
			memcpy(g_last_rgb, rgb, nrgb);
			have_fb = 1;
		}
		last_icol = g_icol;   /* keep the de-dupe key coherent across a tier switch */
		last_irow = g_irow;
		last_ew   = g_ew;
		last_eh   = g_eh;

		/* The same DSR-ACK pacing and the same accounting as the sixel path: a text
		 * frame is a payload like any other, and the backpressure is about what the
		 * TERMINAL has drawn, not about what we drew it with. */
		sr_out_put("\x1b[6n", 4);
		g_pace_inflight++;
		g_pace_progress_ms = sr_io_now_ms();
		sr_pace_dsr_sent(g_pace_progress_ms);
		sr_stats_add_frame((uint32_t)(g_out_len - frame_start));
		sr_io_overlay_after_frame(force);   /* the bottom row is ours again */
		sr_io_out_flush();
		return;
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

	frame_start = g_out_len;   /* wire bytes for this frame, for the stats overlay */

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

	sr_stats_add_frame((uint32_t)(g_out_len - frame_start));
	sr_io_overlay_after_frame(force);   /* the bottom row is ours again */

	sr_io_out_flush();
}
