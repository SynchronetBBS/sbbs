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
 * Task 5 (M1, sixel tier only, DESIGN.md §5/§9/§10): NO whole-frame de-dupe
 * and NO DSR-ACK pacing here -- both are a later task (§5's "de-dupe" and
 * "DSR-ACK backpressure" bullets, §9's probe-reply parsing). sm_io_present()
 * is a straight encode-and-emit every call; only the palette-register
 * decision is conditional. The image rect is computed against a fixed
 * 640x400/80x25 default canvas -- narrowing it from the real probe reply is
 * the input module's job (a later task); sm_io_geom() already returns a sane
 * default so Task 6's mouse mapper has something to divide by from frame 1.
 *
 * Sink selection (env, checked once in sm_io_init()):
 *   SYNCMOO1_SIXELOUT=<path>  capture mode -- each sm_io_present() OVERWRITES
 *                             the file with one self-contained sixel frame
 *                             (palette always included, so an independent
 *                             ImageMagick decode of the file needs no
 *                             carried-over terminal register state). For
 *                             offline verification with no live socket.
 *   (else)                    the sockfd sm_io_init() was given (or fd 1,
 *                             stdout, if it was < 0 -- dev/tty use), written
 *                             non-blocking.
 *
 * POSIX only for now: this door's Windows/MSVC build is DESIGN.md's M2+
 * ("Deferred" milestones), and hw_sbbs.c (the 1oom hw backend this module
 * plugs into) has no _WIN32 branches yet either -- matching that scope
 * rather than adding untested Windows plumbing ahead of it.
 */
#include "syncmoo1_io.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sixel.h"      /* termgfx: sixel_encode_aspect */
#include "term.h"       /* termgfx: termgfx_term_enter/probe/leave */
#include "caps.h"       /* termgfx: termgfx_query_jxl */
#include "geometry.h"   /* termgfx: termgfx_geom_fit_ex / termgfx_geom_center */

/* 1oom's native canvas (DESIGN.md: "Canvas: native 320x200, 8-bit
 * palettized"). */
#define SM_FB_W 320
#define SM_FB_H 200

/* Default terminal canvas assumed until a later task's probe-reply parser
 * narrows it: an 80x25 SyncTERM session at the usual 8x16 font is 640x400
 * pixels -- exactly 2x the native frame, so the default geometry fills the
 * canvas edge to edge with no letterbox bars. */
#define SM_CANVAS_W_DEF 640
#define SM_CANVAS_H_DEF 400
#define SM_CELL_W_DEF   8
#define SM_CELL_H_DEF   16

/* Generous upper bound on the fitted sixel width -- termgfx_geom_fit_ex()
 * just needs SOME cap (0 would mean uncapped); way above any real terminal
 * canvas this door will ever be centered in. */
#define SM_GEOM_SCALE_MAX 2048

/* pan=pad=2: SyncTERM/cterm renders each encoded sixel pixel as a 2x2 block,
 * so a 320x200 encode DISPLAYS at 640x400 (the default canvas above) while
 * sending ~1/4 the raster bytes of a pre-upscaled 640x400 sixel -- the
 * terminal does the nearest-neighbor doubling (sixel.h, termgfx_geom
 * doc). Brief-mandated for M1; a later task may add a full-res opt-in like
 * SyncDuke's. */
#define SM_SIXEL_PAN 2
#define SM_SIXEL_PAD 2

/* Bounded blocking drain on exit (sm_io_leave): never hang the door's exit
 * path indefinitely on a dead/wedged peer. */
#define SM_LEAVE_DRAIN_MS 2000

/* --- staged output buffer + sink (Step 4) ---------------------------------- */
static uint8_t *   g_out;
static size_t      g_out_len, g_out_cap, g_out_off;

static int         g_inited;
static int         g_file_mode;    /* SYNCMOO1_SIXELOUT capture mode */
static const char *g_file;
static int         g_fd = -1;      /* socket / stdout fd */

void sm_out_put(const void *buf, size_t len)
{
    if (!g_inited)
        sm_io_init(-1);
    if (len == 0)
        return;
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
        sm_door_hangup("write error");   /* the single canonical hangup path */
        break;
    }
    if (g_out_off >= g_out_len)
        g_out_len = g_out_off = 0;
    return 0;
}

/* Monotonic millisecond clock, for the bounded blocking exit-drain only. */
static uint32_t sm_io_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((uint64_t)t.tv_sec * 1000ULL + (uint64_t)t.tv_nsec / 1000000ULL);
}

static void sm_io_sleep_ms(int ms)
{
    struct timespec t;
    t.tv_sec  = ms / 1000;
    t.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&t, NULL);
}

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
    int ew, eh, dx, dy, icol, irow, cw, ch;

    /* Real probed cell size (canvas/grid) when the grid is known; the 8x16
     * default otherwise (matches syncconquer's door_cell_size() fallback). */
    cw = (g_grid_cols > 0) ? g_canvas_w / g_grid_cols : SM_CELL_W_DEF;
    ch = (g_grid_rows > 0) ? g_canvas_h / g_grid_rows : SM_CELL_H_DEF;
    if (cw <= 0)
        cw = SM_CELL_W_DEF;
    if (ch <= 0)
        ch = SM_CELL_H_DEF;

    /* Fit the native 320x200 frame into the canvas, true aspect (no stretch
     * allowance -- max_stretch_pct=0 -- since the default/probed canvas is an
     * exact (or near-exact) 2x multiple), then center it. */
    termgfx_geom_fit_ex(g_canvas_w, g_canvas_h, SM_FB_W, SM_FB_H,
                        SM_GEOM_SCALE_MAX, 0, &ew, &eh);
    termgfx_geom_center(g_canvas_w, g_canvas_h, ew, eh, cw, ch, &dx, &dy, &icol, &irow);

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

void sm_io_set_canvas(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;   /* malformed/partial reply: keep the current canvas */
    sm_io_ensure_geom();
    g_canvas_w = w;
    g_canvas_h = h;
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

/* --- terminal setup / teardown (Step 3) ------------------------------------ */
static int g_entered;
static int g_left;

void sm_io_enter(void)
{
    if (g_entered)
        return;
    g_entered = 1;

    sm_io_ensure_geom();

    sm_out_puts(termgfx_term_enter);    /* clear+home, hide cursor, no autowrap, DECSDM ?80l */
    sm_out_puts(termgfx_term_probe);    /* learn the terminal's pixel canvas (parsed by a later task) */
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
    const char *s;

    if (g_inited)
        return 0;
    g_inited = 1;
    g_fd = (sockfd >= 0) ? sockfd : 1;   /* fall back to stdout (dev/tty use) */

    signal(SIGPIPE, SIG_IGN);   /* a write to a closed socket returns EPIPE, not a fatal signal */

    if ((s = getenv("SYNCMOO1_SIXELOUT")) != NULL && *s != '\0') {
        g_file = s;
        g_file_mode = 1;
    } else {
        int fl = fcntl(g_fd, F_GETFL, 0);   /* non-blocking: a wedged client never stalls the game */
        if (fl != -1)
            fcntl(g_fd, F_SETFL, fl | O_NONBLOCK);
    }

    sm_io_ensure_geom();
    atexit(sm_io_leave);
    return 0;
}

/* --- present (Step 2) ------------------------------------------------------- */
void sm_io_present(const uint8_t *idx320x200, const uint8_t *pal768)
{
    static uint8_t *sx;
    static size_t   sxcap;
    static uint8_t  last_pal[768];
    static int      have_pal;
    int             emit_pal;
    size_t          n;

    if (!g_inited)
        sm_io_init(-1);

    sm_io_enter();

    if (g_file_mode) {
        /* Capture mode: always a self-contained frame (palette included),
         * no cursor wrap (nothing to preserve -- there's no live terminal
         * session), so the file decodes standalone with ImageMagick. */
        n = sixel_encode_aspect(&sx, &sxcap, idx320x200, SM_FB_W, SM_FB_H,
                                SM_SIXEL_PAN, SM_SIXEL_PAD, pal768, 1);
        sm_out_put(sx, n);
        sm_io_out_flush();
        memcpy(last_pal, pal768, sizeof last_pal);
        have_pal = 1;
        return;
    }

    /* Palette (re)definition only on a real change: SyncTERM persists sixel
     * color registers across images, so re-defining all 256 every frame is
     * what garbles its decoder (sixel.h's emit_palette doc; SyncDOOM/
     * SyncDuke/SyncConquer all apply this same rule). */
    emit_pal = !have_pal || memcmp(last_pal, pal768, sizeof last_pal) != 0;

    {   /* Save cursor, position at the centered cell, restore after -- so the
         * sixel (drawn at the text cursor per DECSDM ?80l) lands centered
         * without disturbing whatever the real cursor position was. */
        char wrap[24];
        int  wn = snprintf(wrap, sizeof wrap, "\x1b" "7\x1b[%d;%dH", g_irow, g_icol);
        if (wn > 0)
            sm_out_put(wrap, (size_t)wn);
    }
    n = sixel_encode_aspect(&sx, &sxcap, idx320x200, SM_FB_W, SM_FB_H,
                            SM_SIXEL_PAN, SM_SIXEL_PAD, pal768, emit_pal);
    sm_out_put(sx, n);
    sm_out_put("\x1b" "8", 2);   /* restore cursor */

    if (emit_pal) {
        memcpy(last_pal, pal768, sizeof last_pal);
        have_pal = 1;
    }

    sm_io_out_flush();
}
