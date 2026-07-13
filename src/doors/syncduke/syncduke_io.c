/*
 * syncduke_io.c -- SyncDuke terminal out-buffer + sixel present path.
 *
 * Stages the per-frame terminal bytes and pushes them to the client sink, and
 * turns the engine framebuffer + palette (from syncduke_plat.c) into a DECSIXEL
 * frame via libtermgfx's sixel_encode(). Mirrors the proven non-blocking
 * out_put/out_flush structure from src/doors/syncdoom/syncdoom.c, trimmed to
 * v1 essentials.
 *
 * Sink selection (env, checked once):
 *   SYNCDUKE_SIXELOUT=<path>  capture mode -- each frame OVERWRITES the file with
 *                             one self-contained sixel (palette always emitted),
 *                             so the file always holds the latest frame. For
 *                             offline verification (decode with ImageMagick).
 *   SYNCDUKE_SOCK=<fd>        write frames to this (door/telnet) socket fd.
 *   (neither)                 write to stdout (fd 1) -- dev/tty testing.
 *
 * The real DOOR32.SYS parse + xpdev sockwrap path lands with door integration
 * (Tasks 6-7); v1 uses a plain non-blocking fd so this stays dependency-light
 * and verifiable without a live BBS.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>        /* the door sink is a Winsock SOCKET (send/ioctlsocket) */
  #include <ws2tcpip.h>        /* IPPROTO_TCP / TCP_NODELAY */
  #include <windows.h>         /* QueryPerformanceCounter (monotonic clock) */
  #include <io.h>              /* _access/_unlink (per-user full-res sixel flag file) */
#else
  #include <errno.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <signal.h>
  #include <time.h>
#endif

#include "syncduke.h"
#include "sixel.h"
#include "term.h"
#include "apc.h"        /* termgfx: SyncTERM APC cached-image transport (DrawJXL) */
#include "jxl.h"        /* termgfx: JPEG XL frame encoder (RGB888 -> JXL) */
#include "caps.h"       /* termgfx: termgfx_query_jxl (the Q;JXL cap-probe string) */
#include "text.h"       /* termgfx: text/block render tiers (rt_config / rt_render_frame) */
#include "geometry.h"   /* termgfx: shared image fit/center (shared with SyncDOOM) */
#include "pace.h"       /* termgfx: shared AIMD pipeline-depth controller */
#include "audio_mgr.h"  /* termgfx: SyncTERM audio-APC manager (digital SFX) */
#include <dirwrap.h>    /* xpdev: mkpath (recursive mkdir) for the audio cache dir */

/* emit a NUL-terminated control string (no embedded NULs) without hand-counting */
static void syncduke_out_puts(const char *s) { syncduke_out_put(s, strlen(s)); }

/* SyncTERM audio: the per-session manager. syncduke_stubs.c's FX_Play* feed SFX
 * into it; syncduke_input.c feeds it inbound replies (the cap probe). Its emit
 * callback stages bytes through syncduke_out_put -- audio rides the same staged
 * buffer as the frames (FIFO), flushed with them. NULL until syncduke_io_init
 * creates it; tier < 1 => silent. */
termgfx_audio_t *  sd_audio = NULL;

/* --- staged output buffer --- */
static uint8_t *   g_out;
static size_t      g_out_len, g_out_cap, g_out_off;

/* --- sink --- */
static int         g_inited;
static int         g_file_mode;        /* SYNCDUKE_SIXELOUT capture mode */
static const char *g_file;
#ifdef _WIN32
static SOCKET      g_iosock = INVALID_SOCKET;   /* door client socket */
static int         g_use_sock;                  /* 1 = writing to the socket */
#else
static int         g_fd = 1;           /* socket / stdout fd */
#endif

/* Audio manager emit callback: stage audio (SFX + music) into the frame buffer so it
 * flushes FIFO with the frames. `stream` (SFX vs music-upload priority) is ignored --
 * one buffer, no reordering. */
static void sd_audio_emit(void *ctx, const void *buf, size_t len, int stream)
{
	(void)ctx;
	(void)stream;
	syncduke_out_put(buf, len);
}

/* Per-user "full-res sixel" preference (vsc=1 instead of the default half-res 2:1-aspect),
 * made sticky via a presence flag-file in the per-user CWD -- syncduke_config.c chdir's into
 * the -home dir before main(), so a bare relative name lands in the player's own dir. */
static int syncduke_sixel_fullres;
#ifdef _WIN32
#  define SD_FULLRES_PRESENT() (_access("syncduke.fullres", 0) == 0)
#  define SD_FULLRES_REMOVE()  _unlink("syncduke.fullres")
#else
#  define SD_FULLRES_PRESENT() (access("syncduke.fullres", F_OK) == 0)
#  define SD_FULLRES_REMOVE()  unlink("syncduke.fullres")
#endif
static void syncduke_save_fullres(void)
{
	if (syncduke_sixel_fullres) {
		FILE *f = fopen("syncduke.fullres", "w");
		if (f)
			fclose(f);
	} else
		SD_FULLRES_REMOVE();
}

/* Best-effort terminal restore: hand the BBS back a terminal with sixel-scrolling,
 * autowrap and the cursor re-enabled, the key mode undone and mouse tracking off.
 * A direct blocking write (not the non-blocking frame path) so it isn't dropped
 * at exit.
 *
 * Reached from atexit() on a normal exit AND, since the hangup-path harmonization,
 * directly from syncduke_hangup() before its _exit() -- a read-side hangup leaves
 * the WRITE direction open, so the BBS still gets its terminal back rather than a
 * session stuck in physical-key mode. Idempotent (the guard below), because
 * _exit() skips atexit but a signal path may not. */
void syncduke_term_restore(void)
{
	static int restored;

	if (restored)
		return;
	restored = 1;
	if (g_file_mode)
		return;
	/* Turn off xterm mouse tracking so the terminal stops reporting mouse events to the
	 * BBS once the door exits, then restore autowrap/sixel-scroll/cursor. */
#ifdef _WIN32
	if (!g_use_sock)
		return;
	termgfx_audio_music_stop(sd_audio);   /* stop the looping music ... */
	termgfx_audio_sfx_stop_all(sd_audio); /* ...and any looping ambient SFX (wind/machinery/etc.) */
	syncduke_out_flush();                 /* drain the staged Flush APC now -- the buffered path is dropped at exit */
	(void)send(g_iosock, "\x1b[?1003l\x1b[?1006l", 16, 0);
	{   /* undo whichever key mode was negotiated (termgfx); nothing if none was */
		char   ks[TERMGFX_KEYMODE_SEQ_MAX];
		size_t kn = termgfx_keymode_restore(syncduke_keymode(), ks, sizeof ks);

		if (kn > 0)
			(void)send(g_iosock, ks, (int)kn, 0);
	}
	{   /* restore the status line hidden at entry: captured pre-door type, else default (indicator) */
		char   sb[8];
		size_t sn = termgfx_term_status_set(sb, sizeof sb, syncduke_status_type() >= 0 ? syncduke_status_type() : 1);
		(void)send(g_iosock, sb, (int)sn, 0);
	}
	(void)send(g_iosock, termgfx_term_leave, (int)strlen(termgfx_term_leave), 0);
#else
	if (g_fd < 0)
		return;
	termgfx_audio_music_stop(sd_audio);   /* stop the looping music ... */
	termgfx_audio_sfx_stop_all(sd_audio); /* ...and any looping ambient SFX (wind/machinery/etc.) */
	syncduke_out_flush();                 /* drain the staged Flush APC now -- the buffered path is dropped at exit */
	(void)write(g_fd, "\x1b[?1003l\x1b[?1006l", 16);
	{   /* undo whichever key mode was negotiated (termgfx); nothing if none was */
		char   ks[TERMGFX_KEYMODE_SEQ_MAX];
		size_t kn = termgfx_keymode_restore(syncduke_keymode(), ks, sizeof ks);

		if (kn > 0)
			(void)write(g_fd, ks, kn);
	}
	{   /* restore the status line hidden at entry: captured pre-door type, else default (indicator) */
		char   sb[8];
		size_t sn = termgfx_term_status_set(sb, sizeof sb, syncduke_status_type() >= 0 ? syncduke_status_type() : 1);
		(void)write(g_fd, sb, sn);
	}
	(void)write(g_fd, termgfx_term_leave, strlen(termgfx_term_leave));
#endif
}

static void syncduke_io_init(void)
{
	const char *s;
	int         ds;

	g_inited = 1;
	if (sd_audio == NULL)
		sd_audio = termgfx_audio_create(sd_audio_emit, NULL);
	termgfx_audio_set_cache_prefix(sd_audio, "syncduke");   /* SyncTERM cache: syncduke/music|sfx/.. */
	termgfx_audio_set_music_quality(sd_audio, syncduke_music_quality());   /* [audio] music_quality */
	{   /* Door-side transcoded-audio cache: prefer the BBS data dir (Synchronet exports
		 * SBBSDATA = the configured data_dir), else fall back beside the GRP on a non-
		 * Synchronet host. Shared OGG cache -> the MIDI render runs once globally. */
		extern char syncduke_grpdir[];
		const char *data = getenv("SBBSDATA");
		char        mcdir[512];

		mcdir[0] = '\0';
		if (data != NULL && *data) {
			size_t      n   = strlen(data);
			const char *sep = (n && (data[n - 1] == '/' || data[n - 1] == '\\')) ? "" : "/";
			snprintf(mcdir, sizeof(mcdir), "%s%ssyncduke/audio", data, sep);
		} else if (syncduke_grpdir[0]) {
			snprintf(mcdir, sizeof(mcdir), "%s/audio", syncduke_grpdir);
		}
		if (mcdir[0] != '\0') {
			mkpath(mcdir);
			termgfx_audio_set_music_cache_dir(sd_audio, mcdir);
		}
	}
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);   /* a write to a closed socket returns EPIPE, not a fatal signal */
#endif
	atexit(syncduke_term_restore);
	syncduke_sixel_fullres = SD_FULLRES_PRESENT();   /* sticky per-user full-res sixel preference */
	if ((s = getenv("SYNCDUKE_SIXELOUT")) != NULL) {   /* capture mode (offline test) */
		g_file = s;
		g_file_mode = 1;
		return;
	}
#ifdef _WIN32
	/* On Windows the only live sink is the door's Winsock socket (Winsock was brought
	 * up in syncduke_door.c's pre-main constructor). A dev run with no socket uses the
	 * SYNCDUKE_SIXELOUT capture mode above; otherwise there's nothing to draw to. */
	if ((ds = syncduke_door_socket()) >= 0) {
		u_long nb  = 1;
		int    one = 1;
		int    sz  = 96 * 1024;
		g_iosock   = (SOCKET)ds;
		g_use_sock = 1;
		ioctlsocket(g_iosock, FIONBIO, &nb);   /* non-blocking: a wedged client never stalls us */
		setsockopt(g_iosock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
		setsockopt(g_iosock, SOL_SOCKET, SO_SNDBUF, (char *)&sz, sizeof sz);
	}
#else
	{
		int fl;
		if ((ds = syncduke_door_socket()) >= 0)            /* real BBS client socket */
			g_fd = ds;
		else if ((s = getenv("SYNCDUKE_SOCK")) != NULL)    /* explicit fd (test) */
			g_fd = atoi(s);
		else
			/* No socket: the player is on our STDOUT -- either a dev/tty run, or a
			 * STDIO door (the BBS redirected our stdin/stdout; Synchronet's
			 * XTRN_STDIO, Mystic on *nix). syncduke_stdout_fd(), not a bare 1: the
			 * engine's chatter was routed over fd 1 by then, and writing there would
			 * put every frame in the BBS log and nothing on the player's screen. */
			g_fd = syncduke_stdout_fd();
		/* non-blocking, so a wedged client never stalls the game loop */
		if ((fl = fcntl(g_fd, F_GETFL, 0)) != -1)
			fcntl(g_fd, F_SETFL, fl | O_NONBLOCK);
	}
#endif
}

void syncduke_out_put(const void *buf, size_t len)
{
	if (!g_inited)
		syncduke_io_init();
	if (g_out_len + len > g_out_cap) {
		g_out_cap = g_out_len + len;
		g_out = realloc(g_out, g_out_cap);
	}
	memcpy(g_out + g_out_len, buf, len);
	g_out_len += len;
}

void syncduke_out_flush(void)
{
	if (!g_inited)
		syncduke_io_init();

	if (g_file_mode) {
		/* truncate-write: the file always holds exactly the latest frame */
		FILE *fp = fopen(g_file, "wb");
		if (fp) {
			fwrite(g_out, 1, g_out_len, fp);
			fclose(fp);
		}
		g_out_len = g_out_off = 0;
		return;
	}

#ifdef _WIN32
	if (!g_use_sock) {            /* dev run with no socket: discard (capture mode does output) */
		g_out_len = g_out_off = 0;
		return;
	}
	while (g_out_off < g_out_len) {
		int n = send(g_iosock, (const char *)g_out + g_out_off,
		             (int)(g_out_len - g_out_off), 0);
		int e;
		if (n > 0) { g_out_off += (size_t)n; continue; }
		e = WSAGetLastError();
		/* Transient -- keep the frame pending and retry next tick, do NOT treat as a
		 * dead socket. WSAEWOULDBLOCK is normal backpressure (slow client). WSAENOBUFS
		 * can surface under a burst of large frames (e.g. holding a key, which defeats
		 * de-dupe and pushes frames out at max rate); WSAEINTR is a benign interruption.
		 * Treating these as a hangup wrongly dropped the player back to the BBS. */
		if (e == WSAEWOULDBLOCK || e == WSAENOBUFS || e == WSAEINTR) {
			if (e != WSAEWOULDBLOCK)
				syncduke_log("send transient wsa=%d (%u bytes pending)", e,
				             (unsigned)(g_out_len - g_out_off));
			break;
		}
		{
			char r[64];
			snprintf(r, sizeof r, "client closed (send error wsa=%d)", e);
			syncduke_hangup(r);   /* genuinely broken socket -> exit, free the node */
		}
		break;
	}
#else
	while (g_out_off < g_out_len) {
		ssize_t n = write(g_fd, g_out + g_out_off, g_out_len - g_out_off);
		if (n > 0) { g_out_off += (size_t)n; continue; }
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			break;          /* slow client: keep the frame pending */
		syncduke_hangup("client closed (write error)");   /* broken socket -> exit, free the node */
		break;
	}
#endif
	if (g_out_off >= g_out_len)
		g_out_len = g_out_off = 0;
}

/* The terminal output is scaled up from Build's native 320x200 so it fills the
 * graphics canvas the way syncdoom does, instead of a small image in the upper-
 * left. The target is the terminal's actual pixel canvas, learned from a startup
 * probe (syncduke_term_px_*): 640x384 on an 80x24 terminal, 640x400 on 80x25, etc. We
 * default to 640x384 (fits a 24-line terminal without overflow) until/unless the
 * probe answers, and cap to the buffer. Width is capped at 640 (80 cols x 8). */
#define SYNCDUKE_OUT_W_MAX 1024
/* Tall enough for a FULL-RES (vsc=1) encode at the width cap: sdw=1024 with the aspect/
 * <=8%-stretch fit yields sdh up to ~691, and full-res encodes sxh=sdh (no /2 like half-
 * res). 640 was the half-res-era ceiling; at full-res sxh could exceed it and overrun
 * syncduke_scaled into the adjacent globals (g_out) -> SIGSEGV. 768 covers the worst case;
 * the area clamp in the sixel path is the hard backstop. */
#define SYNCDUKE_OUT_H_MAX 768
#define SYNCDUKE_OUT_W_DEF 640
#define SYNCDUKE_OUT_H_DEF 384
static uint8_t syncduke_scaled[SYNCDUKE_OUT_W_MAX * SYNCDUKE_OUT_H_MAX];
static int     syncduke_out_w = SYNCDUKE_OUT_W_DEF, syncduke_out_h = SYNCDUKE_OUT_H_DEF;

/* Adopt the probed terminal canvas (clamped to the buffer) for this frame. */
static void syncduke_update_outsize(void)
{
	int w = syncduke_term_px_w(), h = syncduke_term_px_h();
	if (w <= 0 || h <= 0)
		return;                       /* no probe reply yet: keep the default */
	if (w > SYNCDUKE_OUT_W_MAX)
		w = SYNCDUKE_OUT_W_MAX;
	if (h > SYNCDUKE_OUT_H_MAX)
		h = SYNCDUKE_OUT_H_MAX;
	syncduke_out_w = w;
	syncduke_out_h = h;
}

/* The displayed image's horizontal placement (center column + half-width in cells),
 * recorded by present() each frame for the mouse-steer -- which maps a pointer column
 * to a turn rate around the image center.  Where the image actually lands depends on
 * the tier AND terminal (a SyncTERM sixel anchors top-left because the cursor is
 * ignored under ?80l, while JXL and non-SyncTERM sixel are centered), so present()
 * computes it where it places the image rather than re-deriving it here. */
static int g_hsteer_center = 40;   /* image center column (1-based); sane 80-col default */
static int g_hsteer_half   = 40;   /* image half-width in cells (deflection scale) */

void syncduke_hsteer(int *center_col, int *half_cols)
{
	if (center_col)
		*center_col = g_hsteer_center;
	if (half_cols)
		*half_cols  = g_hsteer_half;
}

/* We encode the sixel at 1/SCALE of the on-screen size and emit a "pan;pad pixel-
 * aspect of SCALE so SyncTERM/cterm renders each pixel as a SCALExSCALE block (it
 * does the nearest-neighbor doubling). Same picture as a pre-upscaled full-size
 * sixel, but ~1/(SCALE*SCALE)... ~1/2 the bytes in practice -- which is the whole
 * frame-rate win (matches SyncDOOM's ~88KB at 640x384 instead of our ~170KB). The
 * engine is natively 320x200, so SCALE=2 also means the sixel is ~native res (no
 * wasteful 2x upscale baked into the bytes). */
#define SYNCDUKE_SIXEL_SCALE 2

/* Nearest-neighbor scale the native 320x200 index buffer into syncduke_scaled at
 * the given sixel dimensions (= on-screen size / SCALE). */
static void syncduke_scale_fb(const uint8_t *fb, int sxw, int sxh)
{
	int y;
	for (y = 0; y < sxh; y++) {
		const uint8_t *row = fb + (y * SYNCDUKE_SCREEN_H / sxh) * SYNCDUKE_SCREEN_W;
		uint8_t *      o   = syncduke_scaled + y * sxw;
		int            x;
		for (x = 0; x < sxw; x++)
			o[x] = row[x * SYNCDUKE_SCREEN_W / sxw];
	}
}

/* Graphics tiers SyncDuke can present (F4 cycles through the available ones).
 * SIXEL and JXL are pixel tiers; HALF/BLOCKS/QUADRANT/SEXTANT are termgfx
 * text/block-character tiers -- a SyncTERM-independent low-bandwidth fallback. */
/* SD_SIXEL_FULL sits BELOW the text tiers so sd_is_text_tier() (>= SD_HALF) stays correct.
 * It's the sixel tier encoded at full vertical resolution (vsc=1) instead of the default
 * half-res-with-2:1-aspect, for non-SyncTERM terminals that ignore the sixel raster aspect. */
enum sd_tier { SD_SIXEL = 0, SD_JXL, SD_SIXEL_FULL, SD_HALF, SD_BLOCKS, SD_QUADRANT, SD_SEXTANT };

static int sd_is_text_tier(int t) { return t >= SD_HALF; }

/* Packed RGB888 of the current frame (native 320x200 fb through the 8-bit
 * palette). Shared by the JXL encoder and the text renderer. */
static uint8_t *syncduke_rgb;   static size_t syncduke_rgb_cap;

static void syncduke_pack_rgb(const uint8_t *fb, const uint8_t *pal, int w, int h)
{
	int      x, y;
	uint8_t *p;
	size_t   need = (size_t)w * h * 3;

	if (need > syncduke_rgb_cap) {
		uint8_t *nb = realloc(syncduke_rgb, need);
		if (nb == NULL)
			return;
		syncduke_rgb = nb;
		syncduke_rgb_cap = need;
	}
	p = syncduke_rgb;
	for (y = 0; y < h; y++) {
		const uint8_t *row = fb + (y * SYNCDUKE_SCREEN_H / h) * SYNCDUKE_SCREEN_W;
		for (x = 0; x < w; x++) {
			const uint8_t *c = pal + row[x * SYNCDUKE_SCREEN_W / w] * 3;
			*p++ = c[0]; *p++ = c[1]; *p++ = c[2];
		}
	}
}

#ifdef WITH_JXL
/* --- JXL/APC tier (SyncTERM only) -------------------------------------------------
 * SyncTERM that answers the Q;JXL probe gets JPEG XL frames instead of sixel: far
 * smaller on the wire, and the palette bakes into the RGB so there are no color
 * registers to (re)define. A DrawJXL is a 1:1 cache blit (no terminal scaling, unlike
 * the half-scale sixel + pan/pad trick), so the door scales the image itself: the
 * native 320x200 framebuffer is nearest-scaled up through the 8-bit palette into
 * RGB888 at a fit-to-canvas w x h, then positioned with the APC DX/DY to center it
 * (sizing/centering shared with SyncDOOM via termgfx_geom_fit/center). */
static uint8_t *syncduke_jxl;   static size_t syncduke_jxl_cap;   /* encoded JXL    */
static uint8_t *syncduke_apc;   static size_t syncduke_apc_cap;   /* APC Store+Draw */

/* Encode the frame as JXL and ship it as a DrawJXL blit at canvas pixel (dx,dy).
 * distance 2.0 / effort 1 (lightning) match SyncDOOM's emit_frame_jxl. Returns the wire
 * byte count, or 0 on encode failure (the caller then falls back to sixel this frame). */
/* Last APC zoom sent (1 = none: the door upscaled the frame itself).  Surfaced in
 * the Ctrl-S stats strip so "is the terminal doing the upscale?" is observable. */
static int syncduke_last_zoom_x = 1;
static int syncduke_last_zoom_y = 1;

static size_t syncduke_emit_jxl(const uint8_t *fb, const uint8_t *pal, int w, int h,
                                int dx, int dy)
{
	size_t n;
	int    zx = 1, zy = 1;

	/* When the fit is an exact integer multiple of the native 320x200 -- which is
	 * the usual case, 2x into a 640x400 canvas -- encode the NATIVE frame and let
	 * the terminal replicate the pixels (ZX/ZY).  Identical picture (that upscale
	 * IS nearest-neighbor), a fraction of the bytes.  Any other fit still gets the
	 * door-side upscale below: SyncTERM has no resampler. */
	if (syncduke_img_zoom_ok()
	    && termgfx_geom_zoom(SYNCDUKE_SCREEN_W, SYNCDUKE_SCREEN_H, w, h, &zx, &zy)) {
		w = SYNCDUKE_SCREEN_W;
		h = SYNCDUKE_SCREEN_H;
	}
	else {
		zx = zy = 1;
	}
	syncduke_last_zoom_x = zx;
	syncduke_last_zoom_y = zy;

	syncduke_pack_rgb(fb, pal, w, h);
	n = termgfx_jxl_encode(&syncduke_jxl, &syncduke_jxl_cap, syncduke_rgb, w, h, 2.0f, 1);
	if (n == 0)
		return 0;
	/* Per-session cache name (SF syncterm #256): avoid collision between two
	 * SyncTERM windows sharing one dialing-entry cache dir. */
	static char name[32];
	if (name[0] == '\0')
		snprintf(name, sizeof name, "syncduke_%08x.jxl", termgfx_session_salt());
	n = termgfx_apc_image_zoom(&syncduke_apc, &syncduke_apc_cap, name, "DrawJXL",
	                           syncduke_jxl, n, dx, dy, zx, zy, syncduke_img_blob_ok());
	syncduke_out_put(syncduke_apc, n);
	return n;
}
#endif /* WITH_JXL */

/* --- text/block tier (termgfx render_text) ----------------------------------------
 * Render the native framebuffer as ANSI block characters -- a SyncTERM-independent,
 * low-bandwidth fallback. cols/rows come from the terminal's pixel canvas (8x16 cells);
 * rt_render_frame cell-diffs internally, so after the first full repaint only changed
 * cells go on the wire. 24-bit color on SyncTERM (cterm truecolor), 256-color elsewhere.
 * Charset follows the client (syncduke_term_is_utf8): native UTF-8 block/sub-cell glyphs on a
 * UTF-8 terminal, CP437 bytes on a CP437 terminal -- the door runs Translate-Character-Set-No
 * (EX_BIN), so whatever we emit reaches the terminal untranslated. */
static int sd_rt_mode, sd_rt_cols, sd_rt_rows;   /* last rt_config -- reconfigure on change */
static int syncduke_stats_on;                    /* Ctrl-S live stats strip showing (suppresses the popups) */

static size_t syncduke_emit_text(const uint8_t *fb, const uint8_t *pal, rt_mode_t mode)
{
	int         cols = syncduke_term_cols();   // the REAL character grid, not px/8 -- see the
	int         rows = syncduke_term_rows();   // accessors: px/8,px/16 overran non-8x16 cells
	const char *tb;
	size_t      tlen;

	if (cols < 1)
		cols = 1;
	if (rows < 1)
		rows = 1;
	if ((int)mode != sd_rt_mode || cols != sd_rt_cols || rows != sd_rt_rows) {
		rt_config(cols, rows, mode, syncduke_is_syncterm() ? RT_24BIT : RT_8BIT,
		          syncduke_term_is_utf8() ? RT_UTF8 : RT_CP437);
		sd_rt_mode = mode; sd_rt_cols = cols; sd_rt_rows = rows;
	}
	syncduke_pack_rgb(fb, pal, SYNCDUKE_SCREEN_W, SYNCDUKE_SCREEN_H);   /* native res; rt scales to cells */

	/* Legible HUD overlay: the game's quote/chat strings, captured by operatefta() in
	 * place of the (unreadable) block font.  EXCLUDE the cells we'll overwrite with real
	 * characters so the block renderer skips them, render, then draw the lines on top --
	 * centered like Duke's gametext(160,..), on the terminal row matching the Duke y.
	 * (rt_exclude_clear() also drops last frame's rectangles when no lines are active.) */
	{
		const syncduke_hud_line_t *hud;
		int hn = syncduke_hud_lines(&hud), i;

		rt_exclude_clear();
		if (syncduke_stats_on) {                          /* stats strip owns the bottom row */
			int ovrow = syncduke_term_rows() - 1;         /* 0-based; only when within the grid */
			if (ovrow >= 0 && ovrow < rows)
				rt_exclude_add(ovrow, 0, cols);
		}
		for (i = 0; i < hn; i++) {
			int len = (int)strlen(hud[i].text);
			int row = hud[i].y * rows / SYNCDUKE_SCREEN_H;
			int c0;
			if (len > cols) len = cols;
			if (row < 0) row = 0; else if (row >= rows) row = rows - 1;
			c0 = (cols - len) / 2; if (c0 < 0) c0 = 0;
			rt_exclude_add(row, c0, c0 + len);
		}

		tb = rt_render_frame(syncduke_rgb, SYNCDUKE_SCREEN_W, SYNCDUKE_SCREEN_H, &tlen);
		syncduke_out_put(tb, tlen);

		for (i = 0; i < hn; i++) {
			char ob[SYNCDUKE_HUD_LEN + 32];
			int  len = (int)strlen(hud[i].text);
			int  row = hud[i].y * rows / SYNCDUKE_SCREEN_H;
			int  c0, n;
			if (len > cols) len = cols;
			if (row < 0) row = 0; else if (row >= rows) row = rows - 1;
			c0 = (cols - len) / 2; if (c0 < 0) c0 = 0;
			n = snprintf(ob, sizeof ob, "\x1b[%d;%dH\x1b[1;37;40m%.*s\x1b[0m",
			             row + 1, c0 + 1, len, hud[i].text);
			if (n > 0) {
				syncduke_out_put(ob, (size_t)n);
				tlen += (size_t)n;
			}
		}
	}
	return tlen;
}

/* FNV-1a over the captured HUD lines (count + each y + text).  present() compares this
 * to the last emitted signature so, in a text tier, an appearing/expiring quote is NOT
 * de-duped away (the quote isn't in the framebuffer, so the fb memcmp alone can't see
 * it), and a change forces a repaint of the rows it vacated. */
static uint32_t sd_hud_signature(void)
{
	const syncduke_hud_line_t *hud;
	int         hn = syncduke_hud_lines(&hud), i;
	uint32_t    h  = 2166136261u;
	const char *p;

	h = (h ^ (uint32_t)hn) * 16777619u;
	for (i = 0; i < hn; i++) {
		h = (h ^ (uint32_t)hud[i].y) * 16777619u;
		for (p = hud[i].text; *p != '\0'; p++)
			h = (h ^ (uint8_t)(unsigned char)*p) * 16777619u;
	}
	return h;
}
static uint32_t sd_hud_sig, sd_hud_last_sig;
static uint32_t syncduke_node_last_sig;   /* last-drawn syncduke_node_overlay_sig() (banner de-dupe) */

/* Copy of the NATIVE framebuffer last emitted (+ the geometry it was emitted at),
 * for de-dupe -- mirrors SyncDOOM's g_last_fb memcmp. Comparing the 320x200 source
 * (not the scaled output) is cheaper and lets a duplicate skip the scale too. */
static uint8_t syncduke_last_fb[SYNCDUKE_SCREEN_W * SYNCDUKE_SCREEN_H];
static int     syncduke_have_last, syncduke_last_w, syncduke_last_h, syncduke_last_hsc;
static int     syncduke_last_tier;   /* tier of the last sent frame (0=sixel, 1=jxl); a flip forces a repaint */

/* Graphics-tier override (F4 cycles it, ala SyncDOOM): -1 = auto (jxl when the
 * terminal supports it, else sixel), 0 = force sixel, 1 = force jxl. Lets the player
 * A/B the two tiers on a JXL-capable SyncTERM. */
static int      syncduke_tier_force = -1;
static uint32_t syncduke_label_until;   /* hold the F4/Ctrl-T label on screen until this ms (0 = none) */

/* --- DSR-ACK frame pacing (SyncDOOM's model) ---
 * After each frame we emit a DSR (ESC[6n); the terminal's report (ESC[r;cR) comes
 * back only once it has CONSUMED that frame. The count of unacked DSRs (g_inflight)
 * is therefore how many frames the terminal hasn't drawn yet -- so we DON'T send a
 * new frame while g_inflight >= depth. This paces to the terminal's RENDER rate, not
 * just to link bandwidth: a plain socket-buffer backpressure still floods SyncTERM's
 * sixel decoder (which is the real bottleneck), and an overrun decoder lags, garbles
 * and drops the connection. A stale-progress deadline reclaims the pipeline so a
 * terminal that never reports DSR can't freeze us (degrades to a slideshow). The
 * report is consumed by syncduke_input's CSI parser, which calls syncduke_pace_ack(). */
#define SYNCDUKE_PACE_DEADLINE_MS 750     /* reclaim the pipeline if no DSR progress for this long */
static int      syncduke_pace_depth = 3;  /* fixed depth when not auto (Ctrl-T cycles the steps) */
static int      syncduke_inflight;
static uint32_t syncduke_pace_progress_ms;

/* Auto-depth (AIMD, ala SyncDOOM): default on -- the controller settles the pipeline at
 * the link's sustainable depth from the measured RTT (see syncduke_auto_depth_update),
 * instead of a hand-tuned fixed depth. Ctrl-T cycles fixed 1..16 then back to auto. */
#define SYNCDUKE_AUTO_DEPTH_MAX 8         /* beyond this just buffers (display lag), no more fps */
static int      syncduke_inflight_auto = 1;
static int      syncduke_auto_depth    = 3;
static uint32_t syncduke_auto_adj_at;
static int      syncduke_rt_high;         /* latched once the round-trip is non-trivial (decode time): floor depth 2 */

/* Effective pipeline depth used by the pacing gate, overlay and telemetry: the AIMD auto
 * depth when auto, else the fixed Ctrl-T depth. Pure getter (no side effects). */
static int syncduke_eff_depth(void)
{
	return syncduke_inflight_auto ? syncduke_auto_depth : syncduke_pace_depth;
}

/* Pace snapshot for the frame-stall log (syncduke_plat.c): unacked frames in flight, and the
 * current effective pipeline depth.  High inflight during a stall => the terminal/link isn't
 * draining (transfer/pacing bound), not a local hitch. */
int syncduke_pace_inflight(void) { return syncduke_inflight; }
int syncduke_pace_curdepth(void) { return syncduke_eff_depth(); }

/* Monotonic microsecond clock (cross-platform): QueryPerformanceCounter on
 * Windows, clock_gettime(CLOCK_MONOTONIC) elsewhere. */
static uint64_t syncduke_now_us(void)
{
#ifdef _WIN32
	static LARGE_INTEGER freq;
	LARGE_INTEGER        c;
	if (freq.QuadPart == 0)
		QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&c);
	return (uint64_t)c.QuadPart * 1000000ULL / (uint64_t)freq.QuadPart;
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000000ULL + (uint64_t)t.tv_nsec / 1000ULL;
#endif
}

static uint32_t syncduke_now_ms(void) { return (uint32_t)(syncduke_now_us() / 1000ULL); }

/* Public accessor for the same clock domain present()/pacing use above, so a banner's
 * expiry (set/checked from syncduke_node.c) compares against the identical monotonic
 * clock instead of a second, merely-similar one (e.g. syncduke_input.c's own copy). */
uint32_t syncduke_clock_ms(void) { return syncduke_now_ms(); }

/* DSR send-time ring -> RTT of each frame's terminal round-trip (= link + the
 * terminal's sixel render time, the thing that caps remote frame rate). */
#define SYNCDUKE_DSR_RING 16
static uint32_t syncduke_dsr_ts[SYNCDUKE_DSR_RING];
static int      syncduke_dsr_h, syncduke_dsr_t;
static uint32_t syncduke_rtt_ms, syncduke_rtt_min, syncduke_rtt_min_at;

uint32_t syncduke_rtt(void) { return syncduke_rtt_ms; }   /* smoothed RTT (ms) -- drives the input-feel adaptation */

static void syncduke_dsr_sent(uint32_t now)        /* record a DSR we just emitted */
{
	syncduke_dsr_ts[syncduke_dsr_h] = now;
	syncduke_dsr_h = (syncduke_dsr_h + 1) % SYNCDUKE_DSR_RING;
}

/* Human-readable tier name (shown on the F4 switch label + the stats strip). */
static const char *sd_tier_name(int t)
{
	switch (t) {
		case SD_JXL:      return "jxl";
		case SD_SIXEL:    return "sixel";
		case SD_SIXEL_FULL: return "sixel-full";
		case SD_HALF:     return "half-block";
		case SD_BLOCKS:   return "blocks+shades";
		case SD_QUADRANT: return "quadrant";
		case SD_SEXTANT:  return "sextant";
		default:          return "?";
	}
}

/* Name of the currently active render tier (events.jsonl "tier" field). */
const char *syncduke_active_tier_name(void) { return sd_tier_name(syncduke_last_tier); }

/* How long present() holds the first auto-tier frame waiting for the capability probe
 * reply (so a no-sixel terminal like conhost never sees a sixel frame). */
#define SYNCDUKE_PROBE_GRACE_MS 500

/* The tier auto-selected when there's no F4 override: jxl on a JXL-capable SyncTERM,
 * sixel on a sixel-capable terminal, else the text/block fallback (half-block) so a
 * terminal with neither (e.g. Windows conhost) still renders the game. */
/* The sixel tier to use by default: the full-res variant when the user opted in.  On
 * SyncTERM full-res now means 1:1 encoding (no half-res + 2x upscale), so it's a real
 * quality win there too -- see the hsc/vsc opt-in in present(). */
static int sd_sixel_default(void)
{
	return syncduke_sixel_fullres ? SD_SIXEL_FULL : SD_SIXEL;
}

static int sd_auto_tier(void)
{
#ifdef WITH_JXL
	if (syncduke_jxl_supported())
		return SD_JXL;
#endif
	if (syncduke_have_sixel())
		return sd_sixel_default();
	if (syncduke_probe_replied())
		return SD_HALF;        /* terminal answered the probe but advertises no sixel/JXL */
	return SD_SIXEL;           /* no reply yet: optimistic (most BBS clients are SyncTERM) */
}

/* 1 if the ACTIVE graphics tier (F4 override, else auto) is a text/block tier.  The
 * engine calls this to skip image-only screens (e.g. the exit "order" splashes) that are
 * unreadable as block characters and would otherwise trap the user on a blocking wait. */
int syncduke_text_tier(void)
{
	int t = (syncduke_tier_force >= 0) ? syncduke_tier_force : sd_auto_tier();
	return sd_is_text_tier(t);
}

/* Brief centered popup confirming an F4/Ctrl-T change (ala SyncDOOM's labels). Held by
 * syncduke_label_until -- present() pauses frame output during the dwell so it's readable
 * with the stats overlay off. SUPPRESSED when the Ctrl-S stats strip is up (it already
 * shows tier+depth live, and the popup would just obscure the game). The label's ESC[2J
 * clears the screen, so it forces a repaint (rt_invalidate too -- the text renderer's
 * cell-diff shadow is now stale). */
static void syncduke_show_label(const char *text, int clear)
{
	int  n, cols, pad, cw;
	char buf[220];

	if (syncduke_stats_on)
		return;
	cw   = syncduke_term_cell_w() > 0 ? syncduke_term_cell_w() : 8;
	cols = (syncduke_out_w > 0 ? syncduke_out_w : 640) / cw;   /* real cell width: a wide font has
	                                                            * fewer columns than out_w/8, which
	                                                            * over-counted and pushed the centered
	                                                            * label off the right of the screen */
	pad  = (cols - (int)strlen(text)) / 2;
	if (pad < 0)
		pad = 0;
	/* A TIER switch (clear=1) wipes the old tier's image so the new one paints clean. A
	 * DEPTH change (clear=0) leaves the frozen frame up and just overlays the label -- no
	 * black-out (the present() dwell holds the frame, not a cleared screen). */
	n = snprintf(buf, sizeof(buf), "%s\x1b[1;%dH\x1b[1;37;44m%s\x1b[0m",
	             clear ? "\x1b[2J\x1b[H" : "", pad + 1, text);
	if (n > 0)
		syncduke_out_put(buf, (size_t)n);
	syncduke_out_flush();
	syncduke_have_last   = 0;     /* repaint after the dwell (overwrite the label / cleared screen) */
	rt_invalidate();
	syncduke_label_until = syncduke_now_ms() + 900;
}

/* F4: cycle the graphics tier (ala SyncDOOM's render-tier cycle): jxl (if the SyncTERM
 * supports it) -> sixel (if the terminal supports it) -> half-block -> blocks+shades,
 * wrapping. Only tiers the terminal can actually render are offered -- a terminal with no
 * sixel (e.g. Windows conhost) cycles the text tiers only, never landing on a blank sixel
 * frame. The text tiers always render (block characters). quadrant/sextant are UTF-8-only
 * so they aren't offered on a CP437 SyncTERM. Intercepted in syncduke_input (not Duke). */
void syncduke_tier_cycle(void)
{
	int  avail[8], n = 0, i, eff, cur = 0;
	char line[80];

#ifdef WITH_JXL
	if (syncduke_jxl_supported())
		avail[n++] = SD_JXL;
#endif
	if (syncduke_have_sixel() || !syncduke_probe_replied())   /* skip sixel where it can't render */
		avail[n++] = SD_SIXEL;
	/* A second sixel stop encoded full-res (hsc=vsc=1): sharper -- no half-res downscale +
	 * 2x nearest-neighbor upscale -- at ~4x the bytes.  Offered on any sixel-capable client:
	 * on SyncTERM it drops the raster-aspect scaling; elsewhere (e.g. WezTerm that ignore the
	 * 2:1 aspect) it also fixes the vertical squish. */
	if (syncduke_have_sixel() || !syncduke_probe_replied())
		avail[n++] = SD_SIXEL_FULL;
	avail[n++] = SD_HALF;
	avail[n++] = SD_BLOCKS;
	/* Quadrant/sextant use Unicode block/sextant glyphs (U+2596-259F / U+1FB00), so they're
	 * only offered when the client charset is UTF-8 -- on a CP437 terminal they'd be
	 * missing-glyph boxes.  The block tiers above emit native UTF-8 there too (see rt_config). */
	if (syncduke_term_is_utf8()) {
		avail[n++] = SD_QUADRANT;
		avail[n++] = SD_SEXTANT;
	}

	eff = (syncduke_tier_force >= 0) ? syncduke_tier_force : sd_auto_tier();
	for (i = 0; i < n; i++)
		if (avail[i] == eff)
			cur = i;
	syncduke_tier_force = avail[(cur + 1) % n];
	if (syncduke_tier_force == SD_SIXEL || syncduke_tier_force == SD_SIXEL_FULL) {
		syncduke_sixel_fullres = (syncduke_tier_force == SD_SIXEL_FULL);
		syncduke_save_fullres();   /* persist the vertical-scaling choice per-user */
	}

	/* tier switch: clear the pipeline + force a repaint so the new tier paints at once,
	 * even when the popup is suppressed (stats overlay on). */
	syncduke_inflight  = 0;
	syncduke_dsr_t     = syncduke_dsr_h;
	syncduke_have_last = 0;
	rt_invalidate();

	/* Wipe the old tier's bitmap UNCONDITIONALLY.  The popup label (and the clear it
	 * used to carry) is suppressed while the stats overlay is up, but a JXL->sixel
	 * switch leaves the old, larger JXL image showing around the smaller sixel unless
	 * the screen is actually cleared here -- so clear directly, like SyncDOOM. */
	syncduke_out_put("\x1b[2J\x1b[H", 7);
	syncduke_out_flush();
	snprintf(line, sizeof(line), "  Video: %s  ", sd_tier_name(syncduke_tier_force));
	syncduke_show_label(line, 0);   /* cosmetic label only; the screen is already cleared */
	fprintf(stderr, "syncduke: video tier -> %s\n", sd_tier_name(syncduke_tier_force));
}

/* Ctrl-T: cycle the frame-pipeline depth live (ala SyncDOOM): auto -> 1 -> 2 -> ... -> 16
 * -> auto, wrapping. "auto" is the AIMD controller (the default); the fixed steps let the
 * player A/B remote-latency pacing. Flashes a label with the depth + measured lag (unless
 * the stats strip is up). Intercepted in syncduke_input (never reaches Duke). */
void syncduke_depth_cycle(void)
{
	static const int steps[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
	int              ns = (int)(sizeof(steps) / sizeof(steps[0])), i;
	char             line[80];

	if (syncduke_inflight_auto) {                  /* auto -> 1 */
		syncduke_inflight_auto = 0;
		syncduke_pace_depth    = steps[0];
	} else {
		for (i = 0; i < ns; i++)
			if (steps[i] == syncduke_pace_depth)
				break;
		if (i >= ns - 1)                           /* last fixed step (16) -> auto */
			syncduke_inflight_auto = 1;
		else
			syncduke_pace_depth = steps[i + 1];
	}

	if (syncduke_inflight_auto)
		snprintf(line, sizeof(line), "  DEPTH: auto  (%d, lag ~%ums)  ", syncduke_eff_depth(), syncduke_rtt_ms);
	else
		snprintf(line, sizeof(line), "  DEPTH: %d  (lag ~%ums)  ", syncduke_pace_depth, syncduke_rtt_ms);
	syncduke_show_label(line, 0);   /* depth change: keep the frame up, just overlay the label (no black-out) */
	fprintf(stderr, "syncduke: pace depth -> %s (eff %d, lag %ums)\n",
	        syncduke_inflight_auto ? "auto" : "fixed", syncduke_eff_depth(), syncduke_rtt_ms);
}

/* AIMD auto-depth controller (ala SyncDOOM): settle the pipeline at the link's sustainable
 * depth from the measured RTT, instead of oscillating. Called once per DSR round-trip from
 * syncduke_pace_ack (an idle/de-duped lull sends no DSR, so the depth just holds):
 *   - heavy queuing (EMA > 2x baseline): ease down at once to drain;
 *   - clean (< 1.25x baseline): probe up one, rate-limited to ~2.5x/sec so each step
 *     settles before the next; dead-band (1.25x..1.5x): hold.
 * Bounded to [1, ceil], ceil ~one frame per 40ms of baseline round-trip (abs cap
 * SYNCDUKE_AUTO_DEPTH_MAX -- beyond it just buffers display lag without adding fps). */
static void syncduke_auto_depth_update(void)   /* AIMD shared with SyncDOOM (termgfx/pace.c) */
{
	termgfx_aimd_update(syncduke_inflight_auto, &syncduke_auto_depth, &syncduke_auto_adj_at,
	                    syncduke_rtt_ms, syncduke_rtt_min, syncduke_rt_high,
	                    SYNCDUKE_AUTO_DEPTH_MAX, syncduke_now_ms());
}

/* A DSR report came back -> the terminal consumed one in-flight frame (pacing),
 * and we can measure that frame's round-trip for the stats overlay. */
void syncduke_pace_ack(void)
{
	uint32_t now = syncduke_now_ms();

	if (syncduke_inflight > 0)
		syncduke_inflight--;
	if (syncduke_dsr_h != syncduke_dsr_t) {            /* match the oldest outstanding DSR */
		uint32_t rtt = now - syncduke_dsr_ts[syncduke_dsr_t];
		syncduke_dsr_t = (syncduke_dsr_t + 1) % SYNCDUKE_DSR_RING;
		/* fold the round-trip into the smoothed RTT + baseline (shared, termgfx/pace.c);
		 * no stale-reject / no min re-seed window -- Duke's original behavior. */
		if (termgfx_rtt_sample(&syncduke_rtt_ms, &syncduke_rtt_min, &syncduke_rtt_min_at,
		                       &syncduke_rt_high, rtt, now, 0, 0))
			syncduke_auto_depth_update();              /* re-evaluate the AIMD auto depth */
	}
	syncduke_pace_progress_ms = now;
}

/* --- live stats overlay (Ctrl-S): a status strip on the BOTTOM row for diagnosing the
 * frame rate. Shows delivered fps, wire KB/s, the DSR round-trip (lag cur/min ms
 * -- high lag = the terminal renders sixel slowly, which is what caps fps), the
 * pipeline (inflight/depth), the last frame size and encode time, and the de-dupe
 * count. Toggled by Ctrl-S (intercepted in syncduke_input, never reaches Duke). The
 * bottom row is unused in the sixel tier (recompute_geom reserves one cell so a sixel
 * never reaches the last text row -- the Windows-Terminal scroll guard), so this is
 * clear of Duke's top-row pickup/quote messages and, on average, less invasive than
 * the old top strip that overlaid the game view. */
static uint32_t syncduke_win_at, syncduke_win_frames;
static uint64_t syncduke_win_bytes;
static uint32_t syncduke_recent_fps, syncduke_recent_kbps;
static uint32_t syncduke_last_kb, syncduke_last_enc_us;
static uint32_t syncduke_ov_draw_ms;
static char     syncduke_ov_last[96];

void syncduke_stats_toggle(void)
{
	syncduke_stats_on = !syncduke_stats_on;
	syncduke_ov_last[0] = '\0';     /* force the strip to (re)draw */
	syncduke_have_last  = 0;        /* repaint the full frame: shows/erases the strip cleanly */
	if (!syncduke_stats_on) {       /* erase the strip off the bottom row -- the sixel tier never
	                                 * repaints it (reserved row), so clear it explicitly */
		char cb[24];
		int  cn = snprintf(cb, sizeof(cb), "\x1b" "7\x1b[%d;1H\x1b[0m\x1b[K\x1b" "8", syncduke_term_rows());
		if (cn > 0)
			syncduke_out_put(cb, (size_t)cn);
	}
}

/* Roll the 2-second stats window: recompute delivered fps + wire KB/s and LOG the
 * readout to stderr (-> sbbs.log). Called from present() on EVERY frame, whether or
 * not the Ctrl-S overlay is showing -- so the numbers are already live the instant
 * the overlay is toggled on (no 0fps warm-up), and the frame rate is recorded for
 * every session the way SyncDOOM does. */
static void syncduke_stats_window(void)
{
	uint32_t nm = syncduke_now_ms();
	uint32_t span;

	if (syncduke_win_at == 0) {
		syncduke_win_at = nm;
		return;
	}
	if (nm - syncduke_win_at < 2000)
		return;
	span = nm - syncduke_win_at;
	syncduke_recent_fps  = syncduke_win_frames * 1000 / span;
	syncduke_recent_kbps = (uint32_t)(syncduke_win_bytes * 1000 / 1024 / span);
	/* 0fps just means a still scene (de-duped); inflight/dim included for diagnosis. */
	fprintf(stderr, "syncduke: stats %s %ufps %uKB/s lag %u/%ums depth %d%s inflight %d %uKB enc %ums %dx%d\n",
	        sd_tier_name(syncduke_last_tier),
	        syncduke_recent_fps, syncduke_recent_kbps, syncduke_rtt_ms, syncduke_rtt_min,
	        syncduke_eff_depth(), syncduke_inflight_auto ? "/auto" : "", syncduke_inflight, syncduke_last_kb,
	        syncduke_last_enc_us / 1000, syncduke_out_w, syncduke_out_h);
	syncduke_win_at = nm; syncduke_win_frames = 0; syncduke_win_bytes = 0;
}

/* force=1: a fresh frame just painted over the strip, so always repaint it.
 * force=0: standalone refresh -- keep the live numbers moving on a static (all
 * de-duped) screen, but rate-limited to 4Hz so de-dupe churn can't spam the wire. */
static void syncduke_emit_overlay(int force)
{
	char     txt[96], ov[220], bw[20];
	uint32_t nm = syncduke_now_ms();
	int      tn, ovn;

	if (!force && nm - syncduke_ov_draw_ms < 250)
		return;

	/* Build the readout, SyncDOOM-style: spaced fields and KB/s up to 999, then
	 * fractional MB/s so the throughput field stays narrow on a fast link. */
	{
		uint32_t fps  = syncduke_recent_fps  > 9999 ? 9999 : syncduke_recent_fps;
		uint32_t kbps = syncduke_recent_kbps;
		uint32_t rtt  = syncduke_rtt_ms      > 9999 ? 9999 : syncduke_rtt_ms;
		uint32_t rmin = syncduke_rtt_min     > 9999 ? 9999 : syncduke_rtt_min;
		uint32_t kb   = syncduke_last_kb     > 9999 ? 9999 : syncduke_last_kb;
		uint32_t enc  = syncduke_last_enc_us / 1000;
		if (enc > 999)
			enc = 999;
		if (kbps > 999)
			snprintf(bw, sizeof(bw), "%u.%uMB/s", kbps / 1024, (kbps % 1024) * 10 / 1024);
		else
			snprintf(bw, sizeof(bw), "%uKB/s", kbps);
		{
			/* Keyboard + turn-key model in ONE compact token (width = the old "kbd:evdev"):
			 * "evdev/nat" or "kitty/syn" -- the suffix is native hold (low-latency true key-up)
			 * vs the synthetic constant rate (high-latency fallback).  Empty on the byte path. */
			char kbd[16] = "";
			char zm[12]  = "";                    /* terminal-side upscale (APC ZX/ZY) */
			if (syncduke_evdev_active() || syncduke_kitty_active())
				snprintf(kbd, sizeof(kbd), " %s/%s",
				         syncduke_evdev_active() ? "evdev" : "kitty",
				         syncduke_turn_native() ? "nat" : "syn");
			if (syncduke_last_tier == 1
			    && (syncduke_last_zoom_x > 1 || syncduke_last_zoom_y > 1)) {
				if (syncduke_last_zoom_x == syncduke_last_zoom_y)
					snprintf(zm, sizeof(zm), " x%d", syncduke_last_zoom_x);
				else
					snprintf(zm, sizeof(zm), " x%dx%d",
					         syncduke_last_zoom_x, syncduke_last_zoom_y);
			}
			tn = snprintf(txt, sizeof(txt), " %s %ufps %s lag %u/%ums depth %d%s %uKB enc %2ums%s%s%s ",
			              sd_tier_name(syncduke_last_tier),
			              fps, bw, rtt, rmin, syncduke_eff_depth(), syncduke_inflight_auto ? "/auto" : "", kb, enc,
			              kbd,
			              (syncduke_last_tier == 1 && syncduke_img_blob_ok()) ? " blob" : "",   /* JXL inline (DrawJXLBlob) */
			              zm);                                                                  /* " x2" = terminal upscales */
		}
	}
	if (tn < 0)
		return;
	if (!force && strcmp(txt, syncduke_ov_last) == 0)
		return;                                          /* unchanged + frame didn't repaint it */
	if (tn < (int)sizeof(syncduke_ov_last))
		strcpy(syncduke_ov_last, txt);
	syncduke_ov_draw_ms = nm;

	/* Left-justified on the bottom row. Set the strip color FIRST, then erase to
	 * end-of-line so the background attribute paints the whole row as a full-width
	 * status bar (SyncConquer's idiom) and drops any tail from a longer prior readout.
	 * Wrapped in cursor save/restore (ESC 7 / ESC 8) so the game's own cursor is left
	 * untouched. In the text tier the whole row is excluded from the cell-diff (see
	 * syncduke_emit_text). */
	ovn = snprintf(ov, sizeof(ov), "\x1b" "7\x1b[%d;1H\x1b[30;46m%s\x1b[K\x1b[0m\x1b" "8",
	               syncduke_term_rows(), txt);
	if (ovn > 0)
		syncduke_out_put(ov, (size_t)ovn);
}

/* The render loop's "may I produce another frame?" gate, mirroring present()'s
 * own send gates: the pipeline has room (an in-flight frame was acked) AND the
 * previous frame's bytes are fully out. syncduke_pace_engine() blocks on this so
 * the engine renders exactly as fast as the terminal draws -- no busy-spin, no
 * random-phase frame drops. */
int syncduke_pace_ready(void)
{
	return syncduke_inflight < syncduke_eff_depth() && g_out_off >= g_out_len;
}

void syncduke_present(void)
{
	static uint8_t *sx;
	static size_t   sxcap;
	static int      cleared;
	static uint32_t cleared_ms;          /* when term-enter probes were sent (startup grace) */
	static int      mouse_track = 0;     /* terminal's xterm mouse tracking: in sync with the Ctrl-O toggle */
	const uint8_t * pal = syncduke_palette();
	const uint8_t * fb;
	int             emit_pal, pal_dirty;
	int             sent = 0;
	int             hsc, vsc;
	int             tier = 0;            /* graphics tier this frame: 0 = sixel, 1 = JXL/APC */
	size_t          n = 0;

	if (!g_inited)
		syncduke_io_init();

	if (g_file_mode) {
		/* Capture mode: always emit a self-contained frame (palette included), no
		 * pacing/de-dupe -- the file just holds the latest frame for offline decode. */
		int sxw = syncduke_out_w / SYNCDUKE_SIXEL_SCALE;
		int sxh = syncduke_out_h / SYNCDUKE_SIXEL_SCALE;
		sxh -= sxh % 6;                            /* whole sixel bands only (see present() below) */
		syncduke_scale_fb(syncduke_fb(), sxw, sxh);
		(void)syncduke_palette_take_dirty();
		n = sixel_encode_aspect(&sx, &sxcap, syncduke_scaled, sxw, sxh,
		                        SYNCDUKE_SIXEL_SCALE, SYNCDUKE_SIXEL_SCALE, pal, 1);
		syncduke_out_put(sx, n);
		syncduke_out_flush();
		return;
	}

	/* Once, on entry: clear the screen, home, hide the cursor, disable autowrap
	 * (so the game canvas isn't drawn over leftover BBS text and the cursor doesn't
	 * blink across the graphics) and reset sixel scrolling, then probe the terminal's
	 * pixel canvas so we can scale to fit -- ESC[14t (reply ESC[4;H;Wt) with a cursor-
	 * position fallback (ESC[999;999H ESC[6n -> ESC[r;cR). syncduke_input parses the
	 * replies. */
	if (!cleared) {
		syncduke_out_puts(termgfx_term_enter);   /* clear+home, hide cursor, no autowrap, no sixel scroll (DECSDM ?80l) */
		syncduke_out_puts(termgfx_term_status_off);   /* hide the client status line -> reclaim the 25th row (640x400); BEFORE the probe so it reports the reclaimed size (syncduke_input captures the DECRQSS reply for restore) */
		syncduke_out_puts(termgfx_term_probe);   /* learn the terminal's pixel canvas */
		syncduke_out_puts("\x1b[c\x1b[<c");      /* DA1 + CTDA: detect sixel (DA1 param 4 / CTDA cap 4) + SyncTERM; a no-sixel reply (conhost) -> text tier */
		syncduke_out_puts(termgfx_query_jxl);    /* Q;JXL: SyncTERM replies ESC[=1;{0,1}-n -> JXL/APC tier when supported */
		syncduke_out_puts(termgfx_keymode_query_kitty);   /* kitty keyboard-protocol query: a CSI?<flags>u reply -> true key-up (hold-to-move) */
		termgfx_audio_probe(sd_audio);           /* Q;libsndfile: SyncTERM replies ESC[=7;100;{0,1}n -> digital-SFX tier */
		syncduke_dsr_sent(syncduke_now_ms());    /* the probe's ESC[6n is a DSR too (keeps the RTT ring aligned) */
		cleared    = 1;
		cleared_ms = syncduke_now_ms();
	}

	/* Keep the terminal's xterm mouse tracking in sync with the steering toggle (Ctrl-O):
	 * 1003 = any-motion tracking, 1006 = SGR (extended) coordinate encoding. */
	if (syncduke_mouse_enabled() != mouse_track) {
		mouse_track = syncduke_mouse_enabled();
		syncduke_out_puts(mouse_track ? "\x1b[?1003h\x1b[?1006h" : "\x1b[?1003l\x1b[?1006l");
	}

	/* Startup grace: when picking the tier automatically, hold the first frame(s) until
	 * the capability probe ('c') answers -- so the auto tier is right from frame 1 and we
	 * never blast sixel at a terminal (e.g. conhost) that can't render it. The reply lands
	 * in a few ms; cap the wait so a silent terminal still gets frames (optimistic sixel). */
	if (syncduke_tier_force < 0 && !syncduke_probe_replied()
	    && (int32_t)(syncduke_now_ms() - cleared_ms) < SYNCDUKE_PROBE_GRACE_MS) {
		syncduke_out_flush();
		return;
	}

	/* F4 tier-cycle label dwell: hold the label readable by NOT painting a frame over
	 * it until the dwell ends, then force a full repaint (SyncDOOM's g_label_until). */
	if (syncduke_label_until) {
		if ((int32_t)(syncduke_now_ms() - syncduke_label_until) < 0) {
			syncduke_out_flush();        /* keep draining; leave the label on screen */
			return;
		}
		syncduke_label_until = 0;
		syncduke_have_last   = 0;        /* dwell ended -> repaint the live frame */
	}

	/* Backpressure: drain what we can of the previous frame; the pacing below decides
	 * whether to encode a new one. */
	syncduke_out_flush();

	/* DSR-ACK pacing: don't get ahead of what the terminal has actually drawn. While
	 * depth frames are already in flight, skip -- unless the terminal has gone quiet
	 * past the deadline, in which case reclaim the pipeline so we don't freeze. */
	if (syncduke_inflight >= syncduke_eff_depth()) {
		if ((int32_t)(syncduke_now_ms() - syncduke_pace_progress_ms) > SYNCDUKE_PACE_DEADLINE_MS) {
			syncduke_inflight = 0;
			syncduke_dsr_t    = syncduke_dsr_h;   /* drop the stale unacked DSR timestamps */
		} else
			goto done;
	}
	if (g_out_off < g_out_len)                /* prior frame's bytes not all out yet: don't pile up */
		goto done;

	syncduke_update_outsize();                /* adopt the probed terminal canvas, if it answered */
	pal_dirty = syncduke_palette_take_dirty();

	/* Pixel-aspect scaling: halve vertically (pan=2 -- the DEC 2:1 pixel aspect,
	 * honored by SyncTERM, foot, Contour and Windows Terminal, but NOT by xterm or
	 * WezTerm, which render at the encoded size and so show a half-height picture
	 * until the user picks the full-res tier below). Halve horizontally too (pad=2)
	 * ONLY on SyncTERM, whose cterm alone reads pad as an integer horizontal scale;
	 * anywhere else that would draw a half-width image, so others keep pad=1. The
	 * sixel is encoded at out/hsc x out/vsc and the terminal scales it back to full.
	 * (See the terminal matrix in termgfx/README.md.) */
	/* Full-res opt-in: encode 1:1 at the display size (hsc=vsc=1) instead of half-res +
	 * the terminal's 2x nearest-neighbor upscale.  Sharper (no sub-native downscale) at
	 * ~4x the sixel bytes.  Applies on SyncTERM too now (drops the raster-aspect scaling);
	 * the default (half-res) still applies otherwise for the leaner payload. */
	vsc = syncduke_sixel_fullres ? 1 : SYNCDUKE_SIXEL_SCALE;
	hsc = (syncduke_sixel_fullres || !syncduke_is_syncterm()) ? 1 : SYNCDUKE_SIXEL_SCALE;

	/* De-dupe (SyncDOOM-style: memcmp the native framebuffer): if the game image,
	 * the palette and the output geometry are all unchanged since the last frame we
	 * sent, there's nothing new to draw -- skip it (no scale, no encode). Static
	 * screens (menus, the title hold) then cost ~zero bandwidth and stay responsive:
	 * a keypress's changed frame goes out at once instead of queuing behind identical
	 * resends. Build re-renders the same state many times between sim tics. The hscale
	 * is part of the geometry so a SyncTERM upgrade (1->2) forces a repaint. */
	/* Graphics tier: the F4 override if set, else auto (jxl when the SyncTERM answered the
	 * Q;JXL probe -- set asynchronously, so the first frame or two are sixel then it flips --
	 * else sixel). Never JXL on a terminal that can't decode it. The tier is part of the
	 * de-dupe geometry so any flip forces one repaint. */
	tier = (syncduke_tier_force >= 0) ? syncduke_tier_force : sd_auto_tier();
#ifdef WITH_JXL
	if (tier == SD_JXL && !syncduke_jxl_supported())
		tier = SD_SIXEL;
#else
	if (tier == SD_JXL)
		tier = SD_SIXEL;
#endif

	/* Legible text-tier HUD: tell the engine (next frame's operatefta) to capture the
	 * on-screen quotes for our ANSI overlay when the active tier is a text tier, and take
	 * this frame's captured-line signature (operatefta already ran for this frame). */
	syncduke_text_hud = sd_is_text_tier(tier);
	sd_hud_sig        = sd_hud_signature();

	/* Cross-tier banner (who's-online / message strip, syncduke_node.c): its own change
	 * signature, folded into the de-dupe below so a banner-only change (no game-frame
	 * change at all) still isn't skipped. */
	uint32_t node_sig = syncduke_node_overlay_sig();

	fb = syncduke_fb();
	if (!pal_dirty && syncduke_have_last
	    && syncduke_last_w == syncduke_out_w && syncduke_last_h == syncduke_out_h
	    && syncduke_last_hsc == hsc && syncduke_last_tier == tier
	    && (!sd_is_text_tier(tier) || sd_hud_sig == sd_hud_last_sig)
	    && node_sig == syncduke_node_last_sig
	    && memcmp(syncduke_last_fb, fb, SYNCDUKE_SCREEN_W * SYNCDUKE_SCREEN_H) == 0)
		goto done;

	if (node_sig != syncduke_node_last_sig) {
		syncduke_have_last = 0;   /* image tier: force a full re-emit so the strip draws/erases */
		rt_invalidate();          /* text tier: same */
	}

	/* Palette emission, per SyncDOOM's tested multi-terminal rule (syncdoom.c): SyncTERM
	 * persists sixel color registers across images, so (re)define them only on a real
	 * palette change -- re-defining every frame is what garbles SyncTERM's decoder. Other
	 * terminals (Windows Terminal, xterm) reset registers per image, so there we include
	 * the palette in every sent frame. De-dupe above still skips identical frames; when one
	 * is skipped the last (palette-bearing) frame persists, so colors stay correct. (JXL
	 * bakes the palette into the RGB, so this only matters to the sixel tier.) ALSO force a
	 * (re)definition when (re)entering the sixel tier from another tier: jxl/text never
	 * define the registers, and a session that started in jxl has none -- so the first sixel
	 * frame would reference undefined registers and render BLACK on SyncTERM. */
	emit_pal = pal_dirty || !syncduke_is_syncterm()
	           || (syncduke_last_tier != SD_SIXEL && syncduke_last_tier != SD_SIXEL_FULL);

	if (sd_is_text_tier(tier)) {
		/* text/block tier: the renderer positions each cell absolutely and cell-diffs
		 * internally, so NO save/home/restore wrap (unlike the image tiers). */
		rt_mode_t m = (tier == SD_BLOCKS)   ? RT_BLOCKS
		                  : (tier == SD_QUADRANT) ? RT_QUADRANT
		                  : (tier == SD_SEXTANT)  ? RT_SEXTANT : RT_HALF;
		uint64_t  t0 = syncduke_now_us();
		if (sd_hud_sig != sd_hud_last_sig)
			rt_invalidate();          /* HUD lines changed -> full repaint so vacated rows refresh */
		n = syncduke_emit_text(fb, pal, m);       /* may be 0 if no cells changed */
		syncduke_last_enc_us = (uint32_t)(syncduke_now_us() - t0);
		sd_hud_last_sig = sd_hud_sig;
	} else {
		/* Image tiers (JXL/sixel): fit Duke's native 320x200 into the graphics canvas
		 * preserving aspect (letter-boxed, NOT stretched to the 4:3 text canvas like
		 * before) and center it -- shared with SyncDOOM via termgfx_geom_fit/center.
		 * The sixel display is capped at 640 wide (it's RLE -- a wide one floods the
		 * wire) and positioned by the text cursor (SyncTERM ignores it under ?80l and
		 * draws at 0,0; Windows Terminal honors it).  Canvas unknown (pre-probe) ->
		 * fall back to the on-screen out size. */
		int  vw = syncduke_canvas_w(), vh = syncduke_canvas_h();
		int  sdw, sdh, irow = 1, icol = 1;
		int  dispw, displeft;                 /* displayed image width + left cell (mouse steer) */
		char wrap[24];

		if (vw < 1 || vh < 1) { vw = syncduke_out_w; vh = syncduke_out_h; }
		/* Reserve ONE cell row at the bottom so the sixel never reaches the LAST screen row:
		 * Windows Terminal (and others that ignore ?80l) scroll a sixel that touches the bottom
		 * edge, scrolling white lines in below it -- the "white bar" seen only when the image
		 * fills the window (this window is ~1.6:1, so filling the width forces full height and
		 * hits the bottom). Centering the fit within vh-cellh lands that one reserved row at the
		 * very bottom and fills everything above it -- the maximum fill that still clears the
		 * last row. (cellh unknown pre-probe -> 16; clamp so a tiny window still fits.) */
		{
			/* Fall back to a sane 8x16 cell when the terminal didn't report its cell
			 * pixel size (ESC[16t): geom_center needs the cell width to turn the centered
			 * pixel offset into a text column, and with cell_w==0 it pins the image to
			 * column 1 (left edge) instead of centering.  SyncTERM answers the graphics
			 * canvas (?2;1S) but NOT 16t, so cell==0 is the common path there -- which is
			 * why a SyncTERM sixel used to hug the left while SyncDOOM (same 8px fallback)
			 * centered.  cell_w tracks the canvas: an 8px-wide font gives cols == vw/8. */
			int cellw = syncduke_term_cell_w() > 0 ? syncduke_term_cell_w() : 8;
			int cellh = syncduke_term_cell_h() > 0 ? syncduke_term_cell_h() : 16;
			int fitvh = vh - cellh;
			if (fitvh < SYNCDUKE_SCREEN_H)
				fitvh = vh;                              /* don't over-shrink a very short window */
			termgfx_geom_fit(vw, fitvh, SYNCDUKE_SCREEN_W, SYNCDUKE_SCREEN_H, 1024, &sdw, &sdh);
			termgfx_geom_center(vw, fitvh, sdw, sdh, cellw, cellh, NULL, NULL, &icol, &irow);
		}
		syncduke_out_put(wrap, snprintf(wrap, sizeof wrap, "\x1b" "7\x1b[%d;%dH", irow, icol));
		/* Mouse-steer center: the sixel is drawn at the centered cursor cell (icol) and
		 * SyncTERM honors the cursor for a sixel (same as other terminals), so the steer
		 * maps the pointer around icol on all of them. */
		dispw = sdw; displeft = icol;
#ifdef WITH_JXL
		if (tier == SD_JXL) {
			/* A DrawJXL is a 1:1 blit (no terminal scaling), so the door scales the
			 * bitmap to fill the canvas itself (capped at [video] scale_max) and
			 * positions it with the APC DX/DY. */
			int      ew, eh, dx = 0, dy = 0;
			uint64_t t0 = syncduke_now_us();
			termgfx_geom_fit(vw, vh, SYNCDUKE_SCREEN_W, SYNCDUKE_SCREEN_H,
			                 syncduke_jxl_scale_max(), &ew, &eh);
			termgfx_geom_center(vw, vh, ew, eh, syncduke_term_cell_w(), syncduke_term_cell_h(),
			                    &dx, &dy, NULL, NULL);
			n = syncduke_emit_jxl(fb, pal, ew, eh, dx, dy);   /* 0 on encode failure */
			syncduke_last_enc_us = (uint32_t)(syncduke_now_us() - t0);
			if (n != 0) {                     /* JXL is centered via the APC DX/DY on both terminals */
				int cw = syncduke_term_cell_w() > 0 ? syncduke_term_cell_w() : 8;
				dispw = ew; displeft = 1 + dx / cw;
			}
		}
#endif
		if (n == 0) {                             /* sixel: the default tier, and the JXL-failure fallback */
			/* Encode at 1/scale of the fitted display size and let the terminal scale it
			 * back up via the "vsc;hsc raster aspect.  Clamp the encoded height to whole
			 * 6-row sixel bands: a partial final band garbles under pan>1 on SyncTERM (the
			 * <=5 dropped rows land at the bottom edge, under the HUD). */
			int      sxw = sdw / hsc;
			int      sxh = sdh / vsc;
			uint64_t t0;

			if (tier != SD_SIXEL_FULL)
				tier = SD_SIXEL;   /* normalize a JXL-failure fallback to sixel, but keep
			                        * SD_SIXEL_FULL so last_tier/stats show "sixel-full" */
			/* Hard-bound the encode to the syncduke_scaled buffer. At full-res (vsc=1) sxh==sdh,
			 * which can exceed OUT_H_MAX once sdw hits the width cap + stretch -- sxw*sxh would
			 * overrun the fixed buffer and corrupt the adjacent globals (g_out -> SIGSEGV in
			 * out_put). Clamp the area so it always fits, whatever the canvas/tier. */
			if (sxw > SYNCDUKE_OUT_W_MAX)
				sxw = SYNCDUKE_OUT_W_MAX;
			if ((long)sxw * sxh > (long)SYNCDUKE_OUT_W_MAX * SYNCDUKE_OUT_H_MAX)
				sxh = (int)((long)SYNCDUKE_OUT_W_MAX * SYNCDUKE_OUT_H_MAX / sxw);
			sxh -= sxh % 6;
			syncduke_scale_fb(fb, sxw, sxh);
			t0 = syncduke_now_us();
			n = sixel_encode_aspect(&sx, &sxcap, syncduke_scaled, sxw, sxh, vsc, hsc, pal, emit_pal);
			syncduke_last_enc_us = (uint32_t)(syncduke_now_us() - t0);
			syncduke_out_put(sx, n);
		}
		syncduke_out_put("\x1b" "8", 2);          /* restore cursor */
		{   /* record the displayed image's horizontal center + half-width (cells) so the
			 * mouse-steer maps the pointer to a turn rate around the actual image, not the
			 * canvas middle (they differ when a SyncTERM sixel anchors top-left). */
			int cw = syncduke_term_cell_w() > 0 ? syncduke_term_cell_w() : 8;
			int icols = dispw / cw;
			g_hsteer_half   = icols / 2 > 0 ? icols / 2 : 1;
			g_hsteer_center = displeft + g_hsteer_half;
		}
	}
	syncduke_out_put("\x1b[6n", 4);           /* DSR: terminal reports once it has CONSUMED this frame (paces both tiers) */
	syncduke_inflight++;
	syncduke_pace_progress_ms = syncduke_now_ms();
	syncduke_dsr_sent(syncduke_pace_progress_ms);
	syncduke_last_kb = (uint32_t)(n / 1024);
	syncduke_win_frames++;
	syncduke_win_bytes += n;
	sent = 1;

	memcpy(syncduke_last_fb, fb, SYNCDUKE_SCREEN_W * SYNCDUKE_SCREEN_H);
	syncduke_last_w    = syncduke_out_w;
	syncduke_last_h    = syncduke_out_h;
	syncduke_last_hsc  = hsc;
	syncduke_last_tier = tier;
	syncduke_have_last = 1;

	syncduke_node_draw(syncduke_out_w / 8, syncduke_out_h / 16);  /* who's-online / message strip */
	syncduke_node_last_sig = node_sig;

done:
	syncduke_hud_begin();                     /* per-frame HUD reset: next frame's operatefta refills it (or leaves
	                                             it empty in a menu, where operatefta doesn't run) -- no stale overlay */
	syncduke_stats_window();                  /* always track + log the 2s window (so Ctrl-S is live immediately) */
	if (syncduke_stats_on)
		syncduke_emit_overlay(sent);          /* force a redraw when a fresh frame just painted over it */
	syncduke_out_flush();
}
