/* syncmoo1_io.c -- terminal out-buffer, enter/probe/leave, sixel present path.
 *
 * Structure and proven patterns are ported from src/doors/syncduke/
 * syncduke_io.c and src/doors/syncconquer/door/door_io.c: the non-blocking
 * staged output buffer, the palette-emit-only-on-change rule (SyncTERM
 * garbles its sixel decoder if the color registers are redefined every
 * frame), the SYNCMOO1_SIXELOUT capture-mode sink (mirrors syncconquer's
 * SYNCALERT_SIXELOUT -- an offline verification path that needs no live
 * socket), and the term enter/probe/leave control strings from termgfx.
 *
 * Task 9 (M1, sixel tier only, DESIGN.md §3/§9): whole-frame de-dupe (memcmp
 * the native framebuffer + palette + draw-position geometry) and DSR-ACK
 * backpressure (via termgfx/pace.c's shared AIMD depth controller) now gate
 * sm_io_present() -- see its doc comment in syncmoo1.h. The image rect is
 * still computed against a fixed 640x400/80x25 default canvas until the real
 * probe reply narrows it (Task 6's job); sm_io_geom() already returns a sane
 * default so the mouse mapper has something to divide by from frame 1.
 *
 * Sink selection (env, checked once in sm_io_init()):
 *   SYNCMOO1_SIXELOUT=<path>  capture mode -- each sm_io_present() OVERWRITES
 *                             the file with one self-contained sixel frame
 *                             (palette always included, so an independent
 *                             ImageMagick decode of the file needs no
 *                             carried-over terminal register state). For
 *                             offline verification with no live socket.
 *   (else)                    the sockfd sm_io_init() was given, written
 *                             non-blocking. When none resolved, POSIX falls
 *                             back to fd 1 (stdout, dev/tty use) and Windows
 *                             has no live sink at all -- see
 *                             sm_plat_fallback_fd().
 *
 * Portable: every Windows-vs-POSIX difference this module once carried (the
 * write() call, O_NONBLOCK, SIGPIPE, the monotonic clock, the sleep) now lives
 * behind syncmoo1_plat.h, which implements them over xpdev.
 */
#include "syncmoo1_io.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "dirwrap.h"   /* xpdev: mkpath() -- recursive mkdir for the capture dir */
#include "syncmoo1_plat.h"   /* clock, sleep, non-blocking descriptor I/O */

#include "sixel.h"      /* termgfx: sixel_encode_aspect */
#include "term.h"       /* termgfx: termgfx_term_enter/probe/leave */
#include "caps.h"       /* termgfx: termgfx_query_jxl */
#include "geometry.h"   /* termgfx: termgfx_geom_center_ex */
#include "syncmoo1_geom.h"   /* sm_geom_fit_page / sm_geom_encode_dims (+ SM_FB_*, SM_SIXEL_*) */
#include "sbbs_node.h" /* termgfx: sbbs_my_node() -- node-tag the capture */
#include "pace.h"       /* termgfx: shared AIMD pipeline-depth controller (termgfx_rtt_sample/termgfx_aimd_update) */
#include "audio_mgr.h"       /* termgfx: termgfx_audio_create/_probe/_set_cache_prefix */
#include "syncmoo1_audio.h"  /* sm_audio_attach */
#include "syncmoo1_music.h"  /* sm_music_attach */
#include "syncmoo1_config.h" /* sm_config_wire_enabled() -- syncmoo1.ini [debug] wire gate; sm_config_music_quality() */

/* The native framebuffer size, the sixel pan/pad, and the fit math itself all
 * live in syncmoo1_geom.h/.c -- pure, and unit-tested by tests/test_geom.c. */

/* Default terminal canvas assumed until a later task's probe-reply parser
 * narrows it: an 80x25 SyncTERM session at the usual 8x16 font is 640x400
 * pixels -- exactly 2x the native frame, so the default geometry fills the
 * canvas edge to edge with no letterbox bars. */
#define SM_CANVAS_W_DEF 640
#define SM_CANVAS_H_DEF 400
#define SM_CELL_W_DEF   8
#define SM_CELL_H_DEF   16

/* Bounded blocking drain on exit (sm_io_leave): never hang the door's exit
 * path indefinitely on a dead/wedged peer. */
#define SM_LEAVE_DRAIN_MS 2000

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* One audio manager per session. Its emit callback stages bytes through the
 * same out-buffer as the sixel stream, so audio and video share one ordered
 * write path (mirrors syncduke_io.c's sd_audio_emit). */
static termgfx_audio_t *g_audio;

static void sm_audio_emit(void *ctx, const void *buf, size_t len, int stream)
{
    (void)ctx;
    (void)stream;
    sm_out_put(buf, len);
}

termgfx_audio_t *sm_io_audio(void)
{
    return g_audio;
}

/* --- staged output buffer + sink (Step 4) ---------------------------------- */
static uint8_t *   g_out;
static size_t      g_out_len, g_out_cap, g_out_off;

static int         g_inited;
static int         g_file_mode;    /* SYNCMOO1_SIXELOUT capture mode */
static const char *g_file;
static int         g_fd = -1;      /* the fd we WRITE to: socket, or stdout; < 0 = none */
/* The fd we READ from. Same as g_fd for a socket (bidirectional). NOT the same
 * for a STDIO door -- the BBS gave us its pipes, so stdin is 0 and stdout is 1,
 * and reading fd 1 reads nothing at all. That is what this door did: it read
 * sm_io_get_fd(), the WRITE fd, which works on a socket and at a dev tty (a tty
 * descriptor is bidirectional) and silently ignores every keystroke down a
 * pipe. */
static int         g_fd_in = -1;

/* Monotonic millisecond clock (syncmoo1_plat.h, xpdev's xp_timer64). The
 * bounded blocking exit-drain, Task 9's DSR-ACK pace timing (deadline reclaim,
 * RTT sampling) and the wire dump all share this one clock domain. */
#define sm_io_now_ms() sm_plat_now_ms()

/* --- wire dump (debug aid, off by default) --------------------------------
 * Records BOTH directions of the terminal conversation, in the order the door
 * produced/consumed them, so a rendering bug on a terminal we can't run
 * locally can be diagnosed from a capture instead of guessed at. The
 * interleaving is the point: "which frame did this DSR ack" or "did a probe
 * reply land before or after the first present" is exactly what a raw
 * one-direction log loses.
 *
 * Controlled by either SYNCMOO1_WIREDUMP (env var, dev runs) or
 * syncmoo1.ini [debug] wire = true (sysop configuration). The ini is the
 * primary switch because Synchronet has no per-door environment setting in
 * xtrn.ini: a capture that depended only on an env var would silently never
 * happen for a door launched from the BBS menu. The ini provides a persistent
 * sysop-facing switch that survives across restarts.
 *
 * When enabled, the default path is RESOLVED, not configured:
 *
 *     $SBBSDATA/syncmoo1/syncmoo1_n<node>.wire
 *
 * SBBSDATA and SBBSNNUM are both setenv()'d for a native door by xtrn.cpp's
 * external() (xtrn.cpp:1206/1209), so a BBS launch always resolves this with
 * no configuration at all; sbbs_my_node() (termgfx) reads the latter, so
 * concurrent nodes never share a file.
 *
 * With no SBBSDATA (a dev run outside the BBS) it falls back to the RELATIVE
 * "syncmoo1.wire", i.e. the cwd as of sm_io_init() -- which is the per-user
 * -home sandbox when -home was given (sm_config_apply() has already chdir'd
 * by then), not the launch dir. Fine for a dev run; the BBS path above never
 * takes this branch.
 *
 * SYNCMOO1_WIREDUMP=<path> overrides the whole resolution for a dev run that
 * wants the capture somewhere specific; SYNCMOO1_WIREDUMP=- forces it off.
 *
 * Record format, repeated: an ASCII header line
 *     <ms-since-first-record> <I|O> <len>\n
 * followed by exactly <len> raw payload bytes (which may contain anything,
 * newlines included -- always use the length, never scan for a delimiter).
 *
 * Off and free when disabled. When enabled, appends, flushes per record so a
 * hangup/crash keeps everything written so far, and never touches the door's
 * behavior: sm_io_wiredump() only observes (verified: a captured session emits
 * byte-for-byte the same stream as an uncaptured one).
 *
 * ONE GAP, on purpose: the pre-connect splash is a direct sm_plat_write() in
 * sm_door_setup() (syncmoo1_door.c), which runs before sm_io_init() opens this
 * file, so those ~39 bytes are not in the capture. Everything from
 * termgfx_term_enter onward is.
 *
 * Decode a capture with tools/wiredump.py. */
/* Cap the capture: a sixel frame is ~44KB, so an unbounded always-on dump
 * would put hundreds of MB in data/ over a long session. Past the cap we stop
 * writing (once, loudly) rather than truncate or wrap -- the interesting part
 * of a rendering bug is the beginning, and a partial capture beats a filled
 * disk. Opened "wb" (truncate), not "ab": each session replaces the previous
 * one for that node, so the file is always the session you just ran. */
#define SM_WIRE_MAX_BYTES (64u * 1024u * 1024u)

static FILE    *g_wire;
static uint32_t g_wire_t0;
static size_t   g_wire_bytes;

static void sm_io_wiredump(char dir, const void *buf, size_t len)
{
    uint32_t now;

    if (g_wire == NULL || len == 0)
        return;
    if (g_wire_bytes >= SM_WIRE_MAX_BYTES) {
        if (g_wire_bytes != (size_t)-1) {
            fprintf(stderr, "syncmoo1: wire dump hit its %u MB cap -- capture stops here\n",
                    SM_WIRE_MAX_BYTES / (1024u * 1024u));
            g_wire_bytes = (size_t)-1;   /* latch: warn once, keep the file closed to writes */
        }
        return;
    }
    now = sm_io_now_ms();
    if (g_wire_t0 == 0)
        g_wire_t0 = now;
    fprintf(g_wire, "%u %c %zu\n", (unsigned)(now - g_wire_t0), dir, len);
    fwrite(buf, 1, len, g_wire);
    fflush(g_wire);
    g_wire_bytes += len;
}

/* Called by syncmoo1_input.c for every byte it reads off the socket. */
void sm_io_wiredump_in(const void *buf, size_t len)
{
    sm_io_wiredump('I', buf, len);
}

/* Resolve the capture path (see above) and open it. Best-effort throughout: a
 * capture we cannot create is never a reason to fail a player's session, so
 * every failure just leaves g_wire NULL and the dump silently off. */
static void sm_io_wiredump_open(void)
{
    const char *env  = getenv("SYNCMOO1_WIREDUMP");
    const char *data = getenv("SBBSDATA");
    char        path[PATH_MAX];
    int         node = sbbs_my_node();

    if (env != NULL && strcmp(env, "-") == 0)
        return;                                  /* explicitly disabled */

    /* Off unless the sysop asked for it. The dump costs multi-MB per session
     * per node; it used to be unconditional. SYNCMOO1_WIREDUMP still forces it
     * on for a one-off debug run without touching the ini. */
    if ((env == NULL || env[0] == '\0') && !sm_config_wire_enabled())
        return;

    if (env != NULL && env[0] != '\0') {
        snprintf(path, sizeof path, "%s", env);
    } else if (data != NULL && data[0] != '\0') {
        size_t dl = strlen(data);
        char   dir[PATH_MAX];

        snprintf(dir, sizeof dir, "%s%ssyncmoo1", data,
                 (dl && (data[dl - 1] == '/' || data[dl - 1] == '\\')) ? "" : "/");
        mkpath(dir);                             /* xpdev: recursive mkdir */
        if (node > 0)
            snprintf(path, sizeof path, "%s/syncmoo1_n%d.wire", dir, node);
        else
            snprintf(path, sizeof path, "%s/syncmoo1.wire", dir);
    } else {
        /* Dev run outside the BBS: beside the binary. */
        snprintf(path, sizeof path, "syncmoo1.wire");
    }

    g_wire = fopen(path, "wb");
    if (g_wire == NULL)
        fprintf(stderr, "syncmoo1: cannot open wire dump '%s' -- continuing without it\n", path);
}

void sm_out_put(const void *buf, size_t len)
{
    if (!g_inited)
        sm_io_init(-1);
    if (len == 0)
        return;
    sm_io_wiredump('O', buf, len);
    if (g_out_len + len > g_out_cap) {
        uint8_t *nb;
        size_t   ncap = g_out_len + len;
        nb = (uint8_t *)realloc(g_out, ncap);
        if (nb == NULL)
            return;   /* OOM here would already be fatal elsewhere; drop silently */
        g_out = nb;
        g_out_cap = ncap;
    }
    memcpy(g_out + g_out_len, buf, len);
    g_out_len += len;
}

/* Emit a NUL-terminated control string without hand-counting its length. */
static void sm_out_puts(const char *s) { sm_out_put(s, strlen(s)); }

int sm_io_out_flush(void)
{
    if (!g_inited)
        sm_io_init(-1);

    if (g_file_mode) {
        /* Truncate-write: the capture file always holds exactly the latest
         * frame (sm_io_present() already made it self-contained). */
        FILE *fp = fopen(g_file, "wb");
        if (fp != NULL) {
            fwrite(g_out, 1, g_out_len, fp);
            fclose(fp);
        }
        g_out_len = g_out_off = 0;
        return 0;
    }

    if (g_fd < 0) {
        /* No live sink (a Windows dev run with no door socket, and not in
         * SYNCMOO1_SIXELOUT capture mode): discard rather than hang up -- there
         * was never a client to lose. */
        g_out_len = g_out_off = 0;
        return 0;
    }

    while (g_out_off < g_out_len) {
        int n = sm_plat_write(g_fd, g_out + g_out_off, g_out_len - g_out_off);
        if (n > 0) {
            g_out_off += (size_t)n;
            continue;
        }
        if (n == SM_IO_INTR)
            continue;
        if (n == SM_IO_AGAIN)
            break;      /* slow client: keep the remainder pending, not an error */
        sm_door_hangup("write error");   /* the single canonical hangup path */
        break;
    }
    if (g_out_off >= g_out_len)
        g_out_len = g_out_off = 0;
    return 0;
}

#define sm_io_sleep_ms(ms) sm_plat_sleep_ms(ms)

/* Retry sm_io_out_flush() until the staged buffer is fully sent or
 * `timeout_ms` elapses. The fd is non-blocking, so a single flush call only
 * ever writes what fits in the kernel send buffer right now -- this is what
 * makes sm_io_leave()'s terminal-restore bytes (mouse-off, term_leave)
 * actually reach a slow/backpressured client instead of being silently
 * dropped at exit. */
static void sm_io_drain_blocking(int timeout_ms)
{
    uint32_t start = sm_io_now_ms();
    for (;;) {
        sm_io_out_flush();
        if (g_out_off >= g_out_len)
            return;
        if ((int32_t)(sm_io_now_ms() - start) >= timeout_ms)
            return;
        sm_io_sleep_ms(15);
    }
}

/* --- DSR-ACK frame pacing (Task 9; termgfx/pace.c shared AIMD) ------------
 *
 * After each frame sm_io_present() ACTUALLY sends (a de-duped frame sends
 * nothing, so it needs no ack), it appends ESC[6n; the terminal's ESC[r;cR
 * reply -- parsed by syncmoo1_input.c's 'R' CSI case -- reports once the
 * terminal has CONSUMED that frame. g_pace_inflight counts unacked DSRs, so
 * it's how many sent frames the terminal hasn't drawn yet; sm_io_present()
 * DROPS a new frame (skip encode+emit) rather than queuing while
 * g_pace_inflight >= the effective depth. This paces to the terminal's
 * RENDER rate, not just link bandwidth -- a plain socket-buffer backpressure
 * still floods SyncTERM's sixel decoder, the real bottleneck; an overrun
 * decoder lags, garbles, drops the connection. A stale-progress deadline
 * (SM_PACE_DEADLINE_MS) reclaims the pipeline so a terminal that never
 * replies to DSR can't wedge the door into a permanent freeze (degrades to a
 * slideshow instead).
 *
 * GRID-PROBE-vs-PACE-ACK (DESIGN.md §9 / Task 9 brief): the startup grid probe
 * (termgfx_term_probe's own trailing ESC[6n, sent once from sm_io_enter())
 * gets its OWN ESC[r;cR reply, which syncmoo1_input.c's 'R' case routes to
 * sm_io_set_grid() -- that reply is NOT pushed through sm_io_pace_ack(): it
 * isn't a reply to any present()-emitted DSR (g_dsr_ts below only ever records
 * DSRs THIS module appended after a real frame), so there's nothing in the
 * ring for it to match, and no in-flight count to decrement. The split is
 * driven by the sm_io_take_grid_probe() one-shot flag (armed in sm_io_enter at
 * probe-SEND time, see there): the FIRST R after the probe is the grid, every
 * R after that is a pace-ack. Anchoring on probe-send-time -- rather than "the
 * first R we ever see" -- is what makes a malformed/lost grid reply safe: the
 * flag is consumed by that first R regardless of whether it parses, so a later
 * genuine pace-ack can never be mis-latched into sm_io_set_grid() (which would
 * corrupt the grid AND permanently leak one in-flight slot). This deliberately
 * diverges from syncduke_input.c (which pre-loads the probe's own DSR into its
 * ring so its first R also acks one "frame") -- simpler here because that
 * first reply genuinely has no present()-side counterpart yet at term-enter
 * time.
 *
 * Always-auto (no fixed/manual Ctrl-key depth override like SyncDuke's
 * Ctrl-T): M1 has no stats-overlay/tuning UI, and 1oom's keyboard is fully
 * claimed by the game, so the AIMD controller alone settles the depth from
 * the measured round-trip; g_pace_depth's initial value (3) is just the
 * starting point before the first RTT sample lands (termgfx_aimd_update()
 * is a no-op until then, pace.h). No CAP_FPS-style frame-rate ceiling is
 * added anywhere -- 1oom is event-driven (DESIGN.md §3), so this ack-driven
 * gate is the only throttle. */
#define SM_PACE_DEADLINE_MS 750    /* reclaim the pipeline if no DSR progress for this long */
#define SM_PACE_DEPTH_MAX   8      /* beyond this just buffers display lag, no more responsiveness */
#define SM_DSR_RING         16

static int      g_pace_inflight;
static int      g_pace_depth = 3;
static uint32_t g_pace_adj_at;
static uint32_t g_pace_progress_ms;
static uint32_t g_rtt_ms, g_rtt_min, g_rtt_min_at;
static int      g_rtt_high;

static uint32_t g_dsr_ts[SM_DSR_RING];
static int      g_dsr_h, g_dsr_t;

/* Record a DSR we just emitted (a real, present()-sent frame's ESC[6n) --
 * NOT called for the startup grid probe's own ESC[6n; see the block comment
 * above. */
static void sm_pace_dsr_sent(uint32_t now)
{
    g_dsr_ts[g_dsr_h] = now;
    g_dsr_h = (g_dsr_h + 1) % SM_DSR_RING;
}

int sm_io_pace_inflight(void) { return g_pace_inflight; }
int sm_io_pace_depth(void)    { return g_pace_depth; }

/* "Startup grid probe outstanding" flag: armed by sm_io_enter() at the moment
 * it emits termgfx_term_probe (whose ESC[999;999H + trailing ESC[6n is the
 * grid query), consumed once by sm_io_take_grid_probe() below. This is the
 * anchor for the first-R-vs-pace-ack split (see that accessor's header doc):
 * armed at probe-SEND time, NOT inferred from "the first R we ever see", so a
 * malformed/lost grid reply can't cause a later genuine pace-ack to be
 * mis-latched as the grid. */
static int g_grid_probe_pending;

int sm_io_take_grid_probe(void)
{
    if (!g_grid_probe_pending)
        return 0;
    g_grid_probe_pending = 0;   /* one-shot: the first R after the probe consumes it */
    return 1;
}

void sm_io_pace_ack(void)
{
    uint32_t now = sm_io_now_ms();

    if (g_pace_inflight > 0)
        g_pace_inflight--;
    if (g_dsr_h != g_dsr_t) {                     /* match the oldest outstanding DSR */
        uint32_t rtt = now - g_dsr_ts[g_dsr_t];
        g_dsr_t = (g_dsr_t + 1) % SM_DSR_RING;
        /* Fold the round-trip into the smoothed RTT + baseline (shared,
         * termgfx/pace.c) -- no stale-reject / no min re-seed window, same
         * as syncduke_pace_ack()'s call -- then re-settle the AIMD depth. */
        if (termgfx_rtt_sample(&g_rtt_ms, &g_rtt_min, &g_rtt_min_at, &g_rtt_high,
                               rtt, now, 0, 0))
            termgfx_aimd_update(1, &g_pace_depth, &g_pace_adj_at,
                                g_rtt_ms, g_rtt_min, g_rtt_high, SM_PACE_DEPTH_MAX, now);
    }
    g_pace_progress_ms = now;
}

/* --- image geometry (Step 2's "compute the image rect once") -------------- */
static sm_geom_t g_geom;
static int       g_geom_ready;
static int       g_icol = 1, g_irow = 1;   /* 1-based text-cell origin, cursor-positioned present */

/* Probe-driven canvas/grid (Task 6): start at the sane 640x400/80x25 default
 * (SM_CANVAS_W_DEF/H_DEF above) so sm_io_geom() always has something to divide
 * by from frame 1, and narrow to the terminal's REAL values once the input
 * module's probe-reply parser calls the setters below (ESC[14t canvas px,
 * the ESC[6n->ESC[r;cR grid). g_grid_rows/cols stay 0 (unknown) until then,
 * in which case the recompute below falls back to the assumed 8x16 cell. */
static int g_canvas_w = SM_CANVAS_W_DEF;
static int g_canvas_h = SM_CANVAS_H_DEF;
static int g_grid_rows, g_grid_cols;

/* Recompute g_geom (image rect + real cell size) from the current
 * g_canvas_w/h + g_grid_rows/cols -- called once lazily (sm_io_ensure_geom)
 * and again every time a probe-reply setter narrows one of those inputs, so
 * sm_io_geom() (and the SGR-mouse mapper that reads it) is always current. */
static void sm_io_recompute_geom(void)
{
    int ew, eh, dx, dy, icol, irow, cw, ch, fith, pagew, pageh;

    /* Real probed cell size (canvas/grid) when the grid is known; the 8x16
     * default otherwise (matches syncconquer's door_cell_size() fallback). */
    cw = (g_grid_cols > 0) ? g_canvas_w / g_grid_cols : SM_CELL_W_DEF;
    ch = (g_grid_rows > 0) ? g_canvas_h / g_grid_rows : SM_CELL_H_DEF;
    if (cw <= 0)
        cw = SM_CELL_W_DEF;
    if (ch <= 0)
        ch = SM_CELL_H_DEF;

    /* Derive the page from the GRID (grid_rows * ch), not from g_canvas_w/h.
     * The two differ, and assuming they don't is what let the image keep
     * landing on the last row whenever SyncTERM shows its status line:
     * SyncTERM never answers the ESC[14t canvas probe at all (its CSI 't' is
     * the CTerm-private palette setter, cterm_cterm.c's cterm_handle_csi_t,
     * which ignores anything with < 4 params), so g_canvas_h stays at the
     * 640x400 DEFAULT while the grid probe correctly reports 24 rows. The
     * page is then 24*16 = 384px, and reserving a row from the assumed 400
     * yields exactly 384 -- the whole usable page, last row included. Deriving
     * the page from the grid makes the reserve real on both: 25 rows -> 400px
     * -> fit 384 (unchanged), 24 rows -> 384px -> fit 368. */
    pagew = (g_grid_cols > 0) ? g_grid_cols * cw : g_canvas_w;
    pageh = (g_grid_rows > 0) ? g_grid_rows * ch : g_canvas_h;
    if (pagew <= 0 || pagew > g_canvas_w)
        pagew = g_canvas_w;
    if (pageh <= 0 || pageh > g_canvas_h)
        pageh = g_canvas_h;

    /* Reserve the bottom row and fit the native frame into what's left; the
     * fit keeps the encode at the lossless native width whenever the page is
     * wide enough for it. See sm_geom_fit_page(). */
    sm_geom_fit_page(pagew, pageh, ch, &ew, &eh, &fith);

    /* Center with the FRACTIONAL cell size, not the integer cw/ch above. On a
     * maximized terminal whose pixels-per-cell isn't an exact 8x16 (Windows
     * Terminal), int truncation of canvas/grid loses a fraction of a cell, and
     * the centered image lands a cell or two off ("kind of centered"). The
     * _ex form divides the pixel offset by the true fractional cell so the col/
     * row round correctly. (termgfx_geom_center_ex, adopted from the shared
     * fix.) */
    {
        double cwf = (g_grid_cols > 0) ? (double)g_canvas_w / g_grid_cols : cw;
        double chf = (g_grid_rows > 0) ? (double)g_canvas_h / g_grid_rows : ch;
        termgfx_geom_center_ex(pagew, fith, ew, eh, cwf, chf,
                               &dx, &dy, &icol, &irow);
    }

    g_geom.ew = ew;
    g_geom.eh = eh;
    g_geom.dx = dx;
    g_geom.dy = dy;
    g_geom.cw = cw;
    g_geom.ch = ch;
    g_icol = icol;
    g_irow = irow;
}

static void sm_io_ensure_geom(void)
{
    if (g_geom_ready)
        return;
    g_geom_ready = 1;
    g_geom.pixel_mode = 0;
    sm_io_recompute_geom();
}

const sm_geom_t *sm_io_geom(void)
{
    sm_io_ensure_geom();
    return &g_geom;
}

/* --- probe-reply setters (Task 6: fed by syncmoo1_input.c's CSI handler) --- */

static int g_canvas_is_gfx;   /* the canvas came from XTSMGRAPHICS: a hard ceiling */

void sm_io_set_canvas(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;   /* malformed/partial reply: keep the current canvas */
    if (g_canvas_is_gfx)
        return;   /* the graphics geometry outranks the ESC[14t window: see below */
    sm_io_ensure_geom();
    g_canvas_w = w;
    g_canvas_h = h;
    sm_io_recompute_geom();
}

/* The terminal's sixel graphics geometry -- the biggest image it will DRAW, which
 * is not the same thing as the window it will FIT. xterm answers ESC[14t with the
 * window and this with min(window, maxGraphicSize) -- 1000x1000 by default -- and
 * silently discards an entire sixel whose declared raster exceeds it. Fitting to
 * the window therefore paints a large xterm black. Authoritative: once this
 * arrives, the ESC[14t reply can no longer widen the canvas past it. */
void sm_io_set_gfx_canvas(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    sm_io_ensure_geom();
    g_canvas_w      = w;
    g_canvas_h      = h;
    g_canvas_is_gfx = 1;
    sm_io_recompute_geom();
}

void sm_io_set_grid(int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        return;
    sm_io_ensure_geom();
    g_grid_rows = rows;
    g_grid_cols = cols;
    sm_io_recompute_geom();
}

void sm_io_set_pixel_mode(int on)
{
    sm_io_ensure_geom();
    g_geom.pixel_mode = on ? 1 : 0;
}

/* The I/O descriptor sm_io adopted (socket, or fd 1 in dev/tty use) --
 * syncmoo1_input.c's sm_input_pump() reads from this, so the input module
 * never needs its own copy of "which fd is the door on". */
int sm_io_get_fd(void)
{
    if (!g_inited)
        sm_io_init(-1);
    return g_fd;
}

/* The fd to READ from. Its only consumer is the input pump. */
int sm_io_in_fd(void)
{
    if (!g_inited)
        sm_io_init(-1);
    return g_fd_in;
}

/* --- terminal setup / teardown (Step 3) ------------------------------------ */
static int g_entered;
static int g_left;

void sm_io_enter(void)
{
    if (g_entered)
        return;
    g_entered = 1;

    sm_io_ensure_geom();

    /* clear+home, hide cursor, no autowrap, DECSDM ?80l. That ?80l is termgfx's
     * (shared with SyncDOOM/SyncDuke, so not ours to change here): on a current
     * SyncTERM it ENABLES sixel scrolling rather than disabling it -- see the
     * rev-1.328 polarity note in sm_io_present(). Harmless for us only because
     * the bottom-row reserve keeps the image off the last row either way. */
    sm_out_puts(termgfx_term_enter);

    /* Audio: create the manager and probe before any sample is registered.
     * The reply lands ~50 ms later; sm_audio_pump() drains the pending Stores
     * when it does. Cache prefix "moo1" -> client-cache names moo1/sfx/s_... */
    g_audio = termgfx_audio_create(sm_audio_emit, NULL);
    termgfx_audio_set_cache_prefix(g_audio, "moo1");
    sm_audio_attach(g_audio);
    sm_music_attach(g_audio);
    termgfx_audio_set_music_quality(g_audio, sm_config_music_quality());
    {
        /* Door-side OGG cache: a track rendered once is shipped from disk on
         * every later session, by any user. Generated runtime data -> data/,
         * never exec/ (see the repo's directory-hierarchy rules). */
        const char *data = getenv("SBBSDATA");
        char        dir[PATH_MAX];

        if (data != NULL && data[0] != '\0') {
            size_t dl = strlen(data);
            snprintf(dir, sizeof dir, "%s%ssyncmoo1/audio", data,
                     (dl && (data[dl - 1] == '/' || data[dl - 1] == '\\')) ? "" : "/");
            mkpath(dir);
            termgfx_audio_set_music_cache_dir(g_audio, dir);
        }
    }
    termgfx_audio_probe(g_audio);

    sm_out_puts(termgfx_term_probe);    /* learn the terminal's pixel canvas; its ESC[999;999H+ESC[6n is the grid query */
    g_grid_probe_pending = 1;           /* arm the first-R-is-grid flag at probe-SEND time (Task 9 fix; see sm_io_take_grid_probe) */
    sm_out_puts("\x1b[c");              /* DA1: sixel support (param 4) */
    sm_out_puts("\x1b[<c");             /* CTDA: SyncTERM detect */
    sm_out_puts(termgfx_query_jxl);     /* Q;JXL: JXL-tier detect (a later task consumes the reply) */
    /* xterm mouse tracking (1003 any-motion, 1006 SGR coords) always on --
     * MoO1 is mouse-driven (DESIGN.md §1/§6), no steering-toggle gate like
     * SyncDuke's. 1016 (SGR-Pixels) + its DECRQM query: light up per-pixel
     * mouse precision when the terminal supports it (§6); SyncTERM ignores
     * both today and stays cell-granular. */
    sm_out_puts("\x1b[?1003h\x1b[?1006h\x1b[?1016h\x1b[?1016$p");
    sm_io_out_flush();
}

void sm_io_leave(void)
{
    if (g_left)
        return;
    g_left = 1;

    if (g_file_mode)
        return;   /* capture mode: no terminal on the other end */

    sm_out_puts("\x1b[?1003l\x1b[?1006l\x1b[?1016l");   /* mouse tracking off */
    sm_out_puts(termgfx_term_leave);                    /* restore ?80h/?7h/?25h for the BBS */
    sm_io_drain_blocking(SM_LEAVE_DRAIN_MS);             /* bounded: never hang the exit path */
}

/* --- init (Step 4) ---------------------------------------------------------- */
int sm_io_init(int sockfd)
{
    return sm_io_init_fds(sockfd, sockfd);
}

/* A STDIO door (the BBS redirected our stdin/stdout instead of handing us a
 * socket -- Synchronet's XTRN_STDIO, Mystic on *nix) passes 0 and 1: two
 * DIFFERENT descriptors, each half-duplex. A socket door passes the one socket
 * twice, which is what it has always done. */
int sm_io_init_fds(int in_fd, int out_fd)
{
    const char *s;

    if (g_inited)
        return 0;
    g_inited = 1;
    /* Windows has no stdout fallback (fd 1 is not a Winsock SOCKET), so an
     * unresolved socket leaves g_fd < 0 = "no live sink". */
    g_fd    = (out_fd >= 0) ? out_fd : sm_plat_fallback_fd();
    g_fd_in = in_fd;

    /* Idempotent, and normally already done by sm_door_setup(): Winsock up,
     * SIGPIPE off. Repeated here because sm_out_put() lazily self-inits this
     * module, so a future caller could reach the socket through here first. */
    sm_plat_net_init();

    /* Opened before anything is emitted, so the very first bytes (term_enter,
     * the capability probes) land in the capture too. */
    sm_io_wiredump_open();

    if ((s = getenv("SYNCMOO1_SIXELOUT")) != NULL && *s != '\0') {
        g_file = s;
        g_file_mode = 1;
    } else {
        /* non-blocking: a wedged client never stalls the game loop */
        (void)sm_plat_sock_setup(g_fd);
        /* The READ fd too. On a socket door it is the same descriptor, so this is
         * a no-op -- but on a STDIO door it is fd 0, a DIFFERENT descriptor, and a
         * BLOCKING stdin parks the whole game inside read() until the player
         * happens to press a key. The game does not render, does not tick, and
         * looks hung; feed it a stream of bytes and it springs to life, which is
         * exactly the symptom that found this. */
        if (g_fd_in >= 0 && g_fd_in != g_fd)
            (void)sm_plat_sock_setup(g_fd_in);
    }

    sm_io_ensure_geom();
    atexit(sm_io_leave);
    return 0;
}

/* Scratch buffer for the scaled (encoded) frame; grown on demand. */
static uint8_t *g_scaled;
static size_t   g_scaled_cap;

static int sm_io_ensure_scaled(int w, int h)
{
    size_t need = (size_t)w * (size_t)h;

    if (need > g_scaled_cap) {
        uint8_t *p = realloc(g_scaled, need);

        if (p == NULL)
            return -1;
        g_scaled     = p;
        g_scaled_cap = need;
    }
    return 0;
}

/* Encoded sixel dimensions, and the pixel-aspect the terminal applies to them,
 * for the current geometry. The per-axis aspect choice, the band clamp and the
 * degenerate floors all live in sm_geom_encode_dims(); this just feeds it the
 * geometry the last probe reply produced. */
static void sm_io_encode_dims(int *sxw, int *sxh, int *pad, int *pan)
{
    /* Three-way, and all three are MEASURED rather than assumed: SyncTERM scales
     * both axes (encode small on both); a terminal the vertical-scaling probe
     * caught honoring the raster pan -- foot, Contour, Windows Terminal -- scales
     * the height only (halve the height, keep the width 1:1); and one that scales
     * neither (xterm, WezTerm) needs a full 1:1 encode or the picture comes out
     * half-size and the mouse mapping goes with it. Unknown-yet reads as neither,
     * the conservative full-size encode, until a probe reply proves otherwise --
     * same as the palette-persistence guard below. */
    sm_geom_encode_dims(g_geom.ew, g_geom.eh, sm_input_is_syncterm(),
                        sm_input_sixel_vscale(), sxw, sxh, pad, pan);
}

/* Nearest-neighbour scale of the native 320x200 indexed frame into dst
 * (dw x dh). Purely index-domain: no palette involvement, so it can never
 * introduce a colour that isn't already in the frame.
 *
 * Nearest-neighbour only ever DUPLICATES a source pixel when it upsamples, but
 * it DROPS one outright when it downsamples -- and a dropped row or column
 * takes MoO1's 1-pixel font strokes with it. So dw >= SM_FB_W and dh >= SM_FB_H
 * hold by construction here, courtesy of sm_geom_fit_page()'s native-width
 * guarantee and sm_geom_encode_dims()'s per-axis pan/pad choice. Don't weaken
 * either without re-reading tests/test_geom.c. dw == SM_FB_W (the common case)
 * makes the horizontal term the identity, degenerating this into a row-select. */
static void sm_io_scale_indices(uint8_t *dst, int dw, int dh,
                                const uint8_t *src)
{
    int x, y;

    for (y = 0; y < dh; ++y) {
        const uint8_t *srow = src + (size_t)(y * SM_FB_H / dh) * SM_FB_W;
        uint8_t       *drow = dst + (size_t)y * dw;

        if (dw == SM_FB_W) {
            memcpy(drow, srow, SM_FB_W);
        } else {
            for (x = 0; x < dw; ++x)
                drow[x] = srow[x * SM_FB_W / dw];
        }
    }
}

/* --- present (Step 2; de-dupe + DSR-ACK backpressure added Task 9) --------- */
/* Ask a non-SyncTERM sixel terminal, once, whether it honors the raster pan --
 * SyncTERM scales both axes regardless, and a terminal with no sixel must never be
 * sent one. Held off until the DA reply names the terminal, and answered before the
 * first frame: the probe's cursor reports ack no frame, and the input module routes
 * them away from the DSR pacing (see sm_input_vscale_collect). No answer within the
 * grace = "does not scale", the safe read (a full 1:1 encode is right everywhere). */
#define SM_VSCALE_GRACE_MS 400

static void sm_io_vscale_probe(void)
{
    static int      sent;
    static uint32_t sent_ms;
    char            pb[192];
    size_t          pn;

    if (sent || !sm_input_have_sixel() || sm_input_is_syncterm())
        return;
    pn = termgfx_sixel_vscale_probe(pb, sizeof(pb));
    if (pn == 0)
        return;
    sm_input_vscale_arm();
    sm_out_put(pb, pn);
    sm_io_out_flush();
    sent    = 1;
    sent_ms = sm_io_now_ms();

    /* MoO1 is turn-based: a short synchronous wait here costs nothing a player can
     * feel, and it keeps the verdict out of the frame path entirely. */
    while (!sm_input_vscale_done()
           && (int32_t)(sm_io_now_ms() - sent_ms) < SM_VSCALE_GRACE_MS)
        sm_input_pump(sm_io_get_fd());
}

void sm_io_present(const uint8_t *idx320x200, const uint8_t *pal768)
{
    sm_io_vscale_probe();
    static uint8_t *sx;
    static size_t   sxcap;
    static uint8_t  last_pal[768];
    static int      have_pal;
    /* De-dupe cache (Task 9): the native framebuffer + everything that decides
     * what the encode produces from it -- the draw position (g_icol/g_irow) and
     * the emitted size (g_geom.ew/eh, which sets the scaled encode dims). A
     * resize can change the emitted size while leaving the centered cell at the
     * same 1;1, so ew/eh must be in the key too, not just icol/irow. */
    static uint8_t  last_fb[SM_FB_W * SM_FB_H];
    static int      have_fb;
    static int      last_icol = -1, last_irow = -1;
    static int      last_ew = -1, last_eh = -1;
    int             pal_changed, emit_pal;
    size_t          n;

    if (!g_inited)
        sm_io_init(-1);

    sm_io_enter();

    if (g_file_mode) {
        /* Capture mode: always a self-contained frame (palette included),
         * no cursor wrap (nothing to preserve -- there's no live terminal
         * session), so the file decodes standalone with ImageMagick. Bypasses
         * BOTH the de-dupe cache and the DSR-ACK pacing below -- there's no
         * live terminal to backpressure against, and each captured frame is
         * meant to stand alone, not be skipped as a duplicate of the last. */
        n = sixel_encode_aspect(&sx, &sxcap, idx320x200, SM_FB_W, SM_FB_H,
                                SM_SIXEL_PAN_MAX, SM_SIXEL_PAD_MAX, pal768, 1);
        sm_out_put(sx, n);
        sm_io_out_flush();
        memcpy(last_pal, pal768, sizeof last_pal);
        have_pal = 1;
        return;
    }

    /* Drain whatever's still pending from the last frame before deciding
     * whether to send a new one. */
    sm_io_out_flush();

    /* DSR-ACK backpressure (Task 9, see the block comment above
     * sm_io_pace_ack()): don't outrun what the terminal has actually drawn.
     * While `depth` frames are already in flight, DROP this present (no
     * encode, no emit, no queuing) -- unless the terminal's gone quiet past
     * the deadline, in which case reclaim the pipeline so a silent/dead
     * client can't wedge the door into a permanent freeze. */
    if (g_pace_inflight >= g_pace_depth) {
        if ((int32_t)(sm_io_now_ms() - g_pace_progress_ms) > SM_PACE_DEADLINE_MS) {
            g_pace_inflight = 0;
            g_dsr_t = g_dsr_h;              /* drop the stale unacked DSR timestamps */
        } else
            return;                          /* still behind: drop, don't queue */
    }
    if (g_out_off < g_out_len)
        return;                              /* prior frame's bytes not all out yet: don't pile up */

    /* Palette-changed test computed once, reused by both the de-dupe check
     * and the emit_pal decision below. */
    pal_changed = !have_pal || memcmp(last_pal, pal768, sizeof last_pal) != 0;

    /* De-dupe (memcmp the native framebuffer + palette + draw-position
     * geometry, SyncDOOM/SyncDuke's model, syncduke_io.c:1081-1119): if none
     * of those changed since the last frame actually SENT, there's nothing
     * new to draw -- skip the encode+emit entirely. Static menus then cost
     * ~zero bandwidth; a real change (keypress, mouse-cursor repaint) still
     * goes out at once instead of queuing behind identical resends. */
    if (have_fb && !pal_changed
        && last_icol == g_icol && last_irow == g_irow
        && last_ew == g_geom.ew && last_eh == g_geom.eh
        && memcmp(last_fb, idx320x200, sizeof last_fb) == 0)
        return;

    /* Palette (re)definition. SyncTERM persists sixel colour registers across
     * images, and re-defining all 256 every frame is what garbles its decoder
     * (sixel.h's emit_palette doc) -- so there, define-on-change only.
     *
     * EVERYWHERE ELSE, re-send the palette every frame. Register persistence is
     * not a given: foot defaults to PRIVATE colour registers, allocating a
     * fresh palette per image seeded from just 16 default colours, so a
     * palette-less frame's indices 16-255 land on transparent entries and the
     * text grid shows through -- the picture appears to flicker against it.
     * term_enter now asks for shared registers (?1070l, termgfx/term.h), which
     * fixes foot and xterm, but a terminal that ignores mode 1070 (Windows
     * Terminal) would still be wrong, so don't rely on that alone. This is the
     * same rule SyncDOOM (syncdoom.c:754) and SyncDuke (syncduke_io.c:1136)
     * already apply; its absence here was the actual defect.
     *
     * Unknown terminal (no probe reply yet) counts as not-SyncTERM: a few early
     * frames then carry a redundant palette, which is never WRONG. */
    emit_pal = pal_changed || !sm_input_is_syncterm();

    /* Geometry changed since the last frame we SENT -- a probe reply narrowed
     * the canvas/grid, or the terminal was resized. The previous frame was
     * drawn at a different cell/size and nothing has wiped its footprint, so a
     * stale rectangle of the old image (and 1oom's pre-sixel "Loading..." text)
     * would sit in the background behind the new, smaller or moved frame. Clear
     * the screen first. have_fb guards the very first frame, already cleared by
     * term_enter. (Same fix the sibling doors' tier-change path applies with
     * ESC[2J; here the trigger is the probe-driven geometry change.) */
    if (have_fb && (last_icol != g_icol || last_irow != g_irow
                    || last_ew != g_geom.ew || last_eh != g_geom.eh))
        sm_out_puts("\x1b[2J\x1b[H");

    {   /* Save cursor, position at the centered cell, restore after -- the sixel
         * is drawn at the text cursor, so this is what centers it without
         * disturbing whatever the real cursor position was.
         *
         * Do NOT reach for DECSDM (?80) to suppress the bottom-row scroll.
         * cterm does implement mode 80 (cterm_cterm.c's CTerm private-mode
         * handler, which chains to the DEC/XTerm one), but it REVERSED THE
         * MODE'S POLARITY in cterm rev 1.328 -- commit 117de27530, 2026-06-28,
         * "Reverse DECSDM meaning" -- which is master-only, in no release tag.
         * So one ?80l means "scrolling disabled, sixel anchored top-left" on a
         * released SyncTERM and "scrolling ENABLED, sixel drawn at the cursor"
         * on a current one, with no way to tell them apart short of sniffing
         * cterm's revision. The bottom-row reserve in sm_io_recompute_geom()
         * holds either way (and on non-SyncTERM sixel terminals besides). */
        char wrap[24];
        int  wn = snprintf(wrap, sizeof wrap, "\x1b" "7\x1b[%d;%dH", g_irow, g_icol);
        if (wn > 0)
            sm_out_put(wrap, (size_t)wn);
    }
    {
        int sxw, sxh, pad, pan;

        sm_io_encode_dims(&sxw, &sxh, &pad, &pan);
        if (sm_io_ensure_scaled(sxw, sxh) != 0)
            return;                       /* OOM: skip this frame, keep running */
        sm_io_scale_indices(g_scaled, sxw, sxh, idx320x200);
        n = sixel_encode_aspect(&sx, &sxcap, g_scaled, sxw, sxh,
                                pan, pad, pal768, emit_pal);
    }
    sm_out_put(sx, n);
    sm_out_put("\x1b" "8", 2);   /* restore cursor */

    if (emit_pal) {
        memcpy(last_pal, pal768, sizeof last_pal);
        have_pal = 1;
    }
    memcpy(last_fb, idx320x200, sizeof last_fb);
    have_fb   = 1;
    last_icol = g_icol;
    last_irow = g_irow;
    last_ew   = g_geom.ew;
    last_eh   = g_geom.eh;

    sm_out_put("\x1b[6n", 4);    /* DSR: terminal reports once it has CONSUMED this frame (paces) */
    g_pace_inflight++;
    /* g_pace_progress_ms (the deadline-reclaim clock) is refreshed on BOTH a
     * send (here) and an ack (sm_io_pace_ack). Harmless: once backpressure
     * engages, sends STOP until an ack frees a slot, so the deadline then
     * effectively measures time-since-last-ACK -- exactly what the reclaim
     * wants (a terminal that stopped answering DSR). */
    g_pace_progress_ms = sm_io_now_ms();
    sm_pace_dsr_sent(g_pace_progress_ms);

    sm_io_out_flush();
}
