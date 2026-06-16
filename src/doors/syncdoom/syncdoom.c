// doomgeneric backend for Synchronet / SyncTERM
//
// Renders each frame as a raster image pushed to SyncTERM via its APC cache
// protocol (see src/syncterm/term.c apc_handler):
//     Store: ESC _ SyncTERM:C;S;<file>;<base64> ST
//     Draw : ESC _ SyncTERM:C;DrawPPM;DX=..;DY=..;<file> ST
// PPM (P6) is the first-light encoder; the emit_frame() seam is where JXL
// drops in later (DrawJXL, same Store mechanism).
//
// Geometry contract: the launching BBS passes the user's screen LINE count on
// the command line as -l<N> (== DOOR.SYS line 21 == term->rows). We derive the
// pixel canvas from that the same way cterm_lib.js charheight() does, so the
// SyncTERM status-bar case (24 lines -> 640x384) is handled without any
// in-band terminal query.
//
// Input: stdin is read raw/non-blocking; terminal key escape sequences are
// parsed and mapped to Doom keys. Terminals deliver key-DOWN only, so key-UP
// is synthesized after an idle interval (terminal auto-repeat keeps a held key
// "down"); standard trick for keyboard-less Doom.

#include "doomkeys.h"
#include "doomgeneric.h"
#include "render_text.h"
#include "render_sixel.h"
#include "m_argv.h"             // myargc/myargv (set directly for the dedicated path)
#include "mp_server.h"          // mp_dedicated_main() headless server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdarg.h>

#include "sockwrap.h"           // xpdev: SOCKET, sendsocket, socket_readable/writable, ioctlsocket, closesocket
#include "dirwrap.h"            // xpdev: mkpath() (recursive mkdir), MKDIR, getdirname/getfname
#include "genwrap.h"            // xpdev: stricmp (+ cross-platform helpers)
#include "ini_file.h"          // xpdev: iniReadFile/iniGetString/iniGetInteger (terminal.ini)

#ifndef _WIN32                  // dev stdio/tty fallback (no socket handle) is *nix-only
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#endif

#ifdef WITH_JXL
#include <jxl/encode.h>
#endif

// ---------------------------------------------------------------------------
// Connection: all client I/O goes through one descriptor, not stdout/stdin.
//
// The launching BBS passes the client's comm handle (DOOR32.SYS line 2 / the
// telnet socket) as -s<fd>; we send frames and recv keystrokes on it. This
// keeps stdout free for Doom's own diagnostics, and is the portable model on
// Windows (a SOCKET handle, used via send/recv). With no -s we fall back to
// STDOUT/STDIN so a quick `| tee` pipe test still works.
//
// Windows TODO: treat the handle as SOCKET, WSAStartup() once; send/recv are
// already the portable calls.
// ---------------------------------------------------------------------------

static SOCKET       g_iosock = INVALID_SOCKET;   // client comm socket (DOOR32.SYS handle / -s)
static bool         g_sock   = false;   // using a socket (else the *nix stdio fallback)
static int          g_rfd    = 0;       // stdin  (fallback only)
static int          g_wfd    = 1;       // stdout (fallback only)
static volatile int g_hangup = 0;       // client dropped -> exit the door

// All I/O is non-blocking and cross-platform via xpdev sockwrap. The socket is
// set non-blocking once (tune_socket); reads are gated by socket_readable() and
// writes by socket_writable(), so we never need platform errno semantics.
// conn_read: >0 = bytes; 0 = EOF/dead (hangup); -1 = no data available now.
static ssize_t conn_read(void *buf, size_t len)
{
	if (g_sock) {
		int n;
		if (!socket_readable(g_iosock, 0))
			return -1;                                  // nothing right now
		n = recv(g_iosock, buf, len, 0);
		return (n == SOCKET_ERROR) ? 0 : n;             // error or 0 -> hangup
	}
#ifndef _WIN32
	return read(g_rfd, buf, len);                        // O_NONBLOCK: -1/EAGAIN = no data
#else
	return -1;
#endif
}

// ---------------------------------------------------------------------------
// Geometry (set from -l<lines> at startup)
// ---------------------------------------------------------------------------

static int      s_lines  = 25;   // user screen lines (DOOR.SYS line 21)
static int      s_pxW    = 640;  // graphics canvas width  (cols * 8)
static int      s_pxH    = 400;  // graphics canvas height (lines * cellH), capped to RESY

static uint32_t g_time_limit_ms = 0;   // user's BBS time at launch (-t<seconds>); 0 = no limit
static uint32_t g_start_ms      = 0;   // door start time (set in DG_Init)

static uint64_t g_tx_bytes  = 0;       // telemetry: total frame bytes put on the wire
static uint32_t g_tx_frames = 0;       // telemetry: frames actually emitted (not dropped)

static char     g_home[PATH_MAX] = ""; // -home: full path to the user's storage dir (empty = cwd)
static char     g_term_path[PATH_MAX] = ""; // -term: terminal.ini file or its dir (override)
static char     g_player_name[64] = ""; // -name: network player handle (multiplayer)
extern char *   net_player_name;       // net_client.c: defaulted unless we set it from -name
static char     g_door32_path[PATH_MAX] = ""; // drop-file path (for the terminal.ini node-dir fallback)
static int      g_cols = 80;           // text-tier columns (from terminal.ini, else 80)

// Graphics scaling (syncdoom.ini [video]). The bitmap tiers have no inherent
// size -- we emit whatever pixel dimensions we choose. "fit" sizes the image to
// the terminal's cell-pixel viewport (cols*8 x rows*cellH) at Doom's 8:5 aspect,
// so a wider terminal gets a bigger image; g_scale_max caps the emitted width as
// a bandwidth guard. Detail still tops out at Doom's 320x200 render -> larger is
// upscaled, not sharper. "off" emits the plain 640x400.
static int g_scale_fit = 1;            // 1 = fit terminal viewport, 0 = native 640x400
static int g_scale_max = 1280;         // max emitted image width (px); 0 = uncapped

// Real terminal PIXEL size, probed at startup. xterm/Windows Terminal answer
// ESC[14t (window px) + ESC[16t (cell px); SyncTERM answers ESC[?2;1S (graphics
// canvas px, XTSMGRAPHICS). Cell-based math (cols*8) is wrong on terminals whose
// cells aren't 8x16, so for "fit" we want the true window pixels. 0 = not reported
// (fall back to the cell model). The image is centered: g_img_{col,row} for the
// sixel text cursor, g_img_{x,y} (pixels) for the SyncTERM APC DX/DY.
static int g_win_w = 0, g_win_h = 0;   // terminal text-area / graphics-canvas pixels
static int g_cell_w = 0, g_cell_h = 0; // terminal cell pixels
static int g_img_col = 1, g_img_row = 1; // 1-based cell to place the centered image (sixel cursor)
static int g_img_x = 0, g_img_y = 0;   // pixel offset of the centered image (APC DX/DY)

static void compute_geometry(void);    // defined later; called from DG_Init after the probe

// Startup diagnostics: what terminal.ini we resolved and what we read from it.
static char g_diag_term[PATH_MAX] = "(no node dir / -term)"; // path we tried
static int  g_diag_term_ok = 0;        // did we actually open+read it?
static char g_diag_desc[64] = "";      // desc= (TERM string) read from it

// Emit one diagnostic line on stderr. Synchronet's external() reads a door's
// stderr line-by-line and logs each at LOG_NOTICE ("<fname>: <line>", xtrn.cpp),
// even in EX_BIN mode (where stderr is logged but not echoed to the user), so
// these land in the BBS log (syslog/journalctl/console) without touching the
// display. One newline-terminated line per call = one log entry.
static void dlog(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
}

// Mirror of cterm_lib.js charheight(rows): graphics cell height for a row count.
static int cell_height(int lines)
{
	switch (lines) {
		case 27: case 28: case 33: case 34: return 14;
	}
	if (lines <= 30)
		return 16;
	return 8;
}

// ---------------------------------------------------------------------------
// base64 (RFC 4648, no line breaks) -> SyncTERM's b64_decode_alloc()
// ---------------------------------------------------------------------------

static const char b64tab[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Encode len bytes of in into out; returns number of base64 chars written.
static size_t b64_encode(char *out, const uint8_t *in, size_t len)
{
	size_t i, o = 0;
	for (i = 0; i + 2 < len; i += 3) {
		uint32_t v = (in[i] << 16) | (in[i + 1] << 8) | in[i + 2];
		out[o++] = b64tab[(v >> 18) & 0x3f];
		out[o++] = b64tab[(v >> 12) & 0x3f];
		out[o++] = b64tab[(v >>  6) & 0x3f];
		out[o++] = b64tab[ v        & 0x3f];
	}
	if (i < len) {
		uint32_t v = in[i] << 16;
		if (i + 1 < len)
			v |= in[i + 1] << 8;
		out[o++] = b64tab[(v >> 18) & 0x3f];
		out[o++] = b64tab[(v >> 12) & 0x3f];
		out[o++] = (i + 1 < len) ? b64tab[(v >> 6) & 0x3f] : '=';
		out[o++] = '=';
	}
	return o;
}

// ---------------------------------------------------------------------------
// Frame encoder: PPM always, JXL when SyncTERM supports it (auto-probed). Both
// go through the same C;S Store + Draw{PPM,JXL} cache path, and the payload is
// base64 either way -> telnet-safe.
// ---------------------------------------------------------------------------

enum frame_mode { MODE_PPM = 0, MODE_JXL = 1, MODE_TEXT = 2, MODE_SIXEL = 3 };
static enum frame_mode g_mode = MODE_PPM;
static int             g_force_text = 0; // -text: force the block-char tier
static rt_charset_t    g_rt_charset = RT_CP437; // -charset (terminal.ini will set this)
static rt_mode_t       g_rt_mode    = RT_HALF; // -mode override
static int             g_rt_mode_set = 0;      // was -mode given?
static rt_colors_t     g_colors     = RT_4BIT; // color depth (terminal.ini desc / -colors)

static uint8_t *       s_rgb = NULL;   static size_t s_rgb_cap = 0;// packed RGB888
static uint8_t *       s_img = NULL;   static size_t s_img_cap = 0;// encoded bytes (PPM/JXL)
static char *          s_b64 = NULL;   static size_t s_b64_cap = 0;// base64 of s_img

// Send a small control string in full (cursor hide, JXL probe) at startup.
static void emit_all(const char *buf, size_t len)
{
	size_t off = 0;
	while (off < len) {
		if (g_sock) {
			int n;
			if (!socket_writable(g_iosock, 1000)) { g_hangup = 1; break; }
			n = sendsocket(g_iosock, buf + off, len - off);
			if (n == SOCKET_ERROR || n == 0) { g_hangup = 1; break; }
			off += (size_t)n;
		} else {
#ifndef _WIN32
			ssize_t n = write(g_wfd, buf + off, len - off);
			if (n <= 0) {
				if (n < 0 && errno == EINTR)
					continue;
				if (n == 0)
					g_hangup = 1;
				break;
			}
			off += (size_t)n;
#else
			break;
#endif
		}
	}
}

// ---------------------------------------------------------------------------
// Staged, drop-on-behind frame output. A frame is built into g_out and drained
// NON-blocking; a new frame is only built once the previous one has fully gone
// out (see emit_frame). This caps in-flight data at ~1 frame, so the game loop
// never blocks in send() and input->display latency stays ~1 frame instead of
// growing without bound (the "decent fps but laggy input" buffer-bloat).
// ---------------------------------------------------------------------------

static uint8_t *g_out = NULL;
static size_t   g_out_cap = 0, g_out_len = 0, g_out_off = 0;

// End-to-end frame pacing: after a frame we emit a DSR (ESC[6n); SyncTERM's
// report (ESC[r;cR) only comes back once it has rendered that frame, so we hold
// the next frame until then. This paces us to the terminal's real render rate
// and bounds the in-flight backlog to ~1 frame (the input-lag fix). The deadline
// is a safety valve if a report is ever lost.
static int      g_awaiting_ack = 0;
static uint32_t g_ack_deadline = 0;

static uint32_t now_ms(void);   // defined below

static int out_pending(void) { return g_out_off < g_out_len; }

static void out_put(const void *buf, size_t len)
{
	if (g_out_len + len > g_out_cap) {
		g_out_cap = g_out_len + len;
		g_out = realloc(g_out, g_out_cap);
	}
	memcpy(g_out + g_out_len, buf, len);
	g_out_len += len;
}

static void out_flush(void)
{
	while (out_pending()) {
		if (g_sock) {
			int n;
			if (!socket_writable(g_iosock, 0))
				break;                                         // can't write now: keep pending
			n = sendsocket(g_iosock, (char *)g_out + g_out_off, g_out_len - g_out_off);
			if (n == SOCKET_ERROR || n == 0) { g_hangup = 1; break; }  // writable but failed -> dead
			g_out_off += (size_t)n;
		} else {
#ifndef _WIN32
			ssize_t n = write(g_wfd, g_out + g_out_off, g_out_len - g_out_off);
			if (n > 0) { g_out_off += (size_t)n; continue; }
			if (n < 0 && errno == EINTR)
				continue;
			if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
				break;                                                      // pipe full: keep pending
			g_hangup = 1; break;
#else
			break;
#endif
		}
	}
	if (!out_pending()) { g_out_len = 0; g_out_off = 0; }
}

static void ensure(uint8_t **buf, size_t *cap, size_t need)
{
	if (need > *cap) { *buf = realloc(*buf, need); *cap = need; }
}

// Pack the rendered frame to RGB888 in s_rgb, vertically scaled to fit the
// h-row canvas. When the terminal is a row short (status-bar-on -> 384), the
// full 400-row frame (3D view + status bar) is squished into h rows so the HUD
// stays fully visible rather than cropped. Unlike a literal shift-the-HUD-up
// this is correct in menus/title/intermission too (no status bar to special-
// case there) -- the ~4% vertical squish is imperceptible.
static void pack_rgb(const uint32_t *fb, int w, int h)
{
	int      x, y, src_w = DOOMGENERIC_RESX, src_h = DOOMGENERIC_RESY;
	uint8_t *p;
	ensure(&s_rgb, &s_rgb_cap, (size_t)w * h * 3);
	p = s_rgb;
	for (y = 0; y < h; y++) {
		int             sy = (h == src_h) ? y : (y * src_h) / h; // nearest-sample scale (up or down)
		const uint32_t *row = fb + (size_t)sy * DOOMGENERIC_RESX;
		for (x = 0; x < w; x++) {
			int      sx = (w == src_w) ? x : (x * src_w) / w;  // horizontal too (was assuming w<=640)
			uint32_t px = row[sx];
			*p++ = (px >> 16) & 0xff;   // R
			*p++ = (px >>  8) & 0xff;   // G
			*p++ =  px        & 0xff;   // B
		}
	}
}

// base64 `bytes` and push: Store(<file>) then <drawverb>(<file>).
static void emit_cached_image(const char *file, const char *drawverb,
                              const uint8_t *bytes, size_t n)
{
	char   apc[64];
	int    l;
	size_t b64max = 4 * ((n + 2) / 3) + 1;
	size_t b64len;

	if (b64max > s_b64_cap) { s_b64 = realloc(s_b64, b64max); s_b64_cap = b64max; }
	b64len = b64_encode(s_b64, bytes, n);

	l = snprintf(apc, sizeof(apc), "\x1b_SyncTERM:C;S;%s;", file);
	out_put(apc, l);
	out_put(s_b64, b64len);
	out_put("\x1b\\", 2);

	l = snprintf(apc, sizeof(apc), "\x1b_SyncTERM:C;%s;DX=%d;DY=%d;%s\x1b\\",
	             drawverb, g_img_x, g_img_y, file);   // DX/DY center the image in the canvas
	out_put(apc, l);
}

static void emit_frame_ppm(int w, int h)
{
	char   hdr[32];
	int    hlen = snprintf(hdr, sizeof(hdr), "P6\n%d %d\n255\n", w, h);
	size_t need = (size_t)hlen + (size_t)w * h * 3;

	ensure(&s_img, &s_img_cap, need);
	memcpy(s_img, hdr, hlen);
	memcpy(s_img + hlen, s_rgb, (size_t)w * h * 3);
	emit_cached_image("d.ppm", "DrawPPM", s_img, need);
}

// Sixel tier: a complete DECSIXEL image drawn at the home position so each frame
// overdraws the last in place. Unlike PPM/JXL this isn't a SyncTERM APC cache
// entry -- it's standard sixel that any sixel-capable terminal renders directly.
static void emit_frame_sixel(int w, int h)
{
	char   pos[24];
	int    l = snprintf(pos, sizeof(pos), "\x1b" "7\x1b[%d;%dH", g_img_row, g_img_col);
	size_t n = sixel_encode(&s_img, &s_img_cap, s_rgb, w, h);
	out_put(pos, l);               // save cursor + position (centered) -> overdraw in place
	out_put(s_img, n);
	out_put("\x1b" "8", 2);        // restore cursor (terminals differ post-sixel)
}

#ifdef WITH_JXL
static float g_jxl_distance = 2.0f;   // lossy Butteraugli distance (smaller=better/bigger)
static int   g_jxl_effort   = 1;      // 1=lightning (fastest encode, lowest latency)

// Encode s_rgb (w*h RGB888) as JXL into s_img; returns false on any failure
// (caller falls back to PPM).
static bool emit_frame_jxl(int w, int h)
{
	JxlEncoder *             enc;
	JxlEncoderFrameSettings *fs;
	JxlBasicInfo             info;
	JxlColorEncoding         color;
	JxlPixelFormat           pf = { 3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };
	JxlEncoderStatus         st;
	size_t                   written = 0;

	enc = JxlEncoderCreate(NULL);
	if (enc == NULL)
		return false;
	fs = JxlEncoderFrameSettingsCreate(enc, NULL);
	JxlEncoderFrameSettingsSetOption(fs, JXL_ENC_FRAME_SETTING_EFFORT, g_jxl_effort);
	JxlEncoderSetFrameLossless(fs, JXL_FALSE);
	JxlEncoderSetFrameDistance(fs, g_jxl_distance);

	JxlEncoderInitBasicInfo(&info);
	info.xsize = w;  info.ysize = h;
	info.bits_per_sample = 8;
	info.num_color_channels = 3;
	info.alpha_bits = 0;
	info.uses_original_profile = JXL_FALSE;
	if (JxlEncoderSetBasicInfo(enc, &info) != JXL_ENC_SUCCESS) { JxlEncoderDestroy(enc); return false; }

	JxlColorEncodingSetToSRGB(&color, JXL_FALSE);
	JxlEncoderSetColorEncoding(enc, &color);

	if (JxlEncoderAddImageFrame(fs, &pf, s_rgb, (size_t)w * h * 3) != JXL_ENC_SUCCESS) {
		JxlEncoderDestroy(enc); return false;
	}
	JxlEncoderCloseInput(enc);

	ensure(&s_img, &s_img_cap, s_img_cap < 4096 ? 4096 : s_img_cap);
	do {
		uint8_t *next  = s_img + written;
		size_t   avail = s_img_cap - written;
		st = JxlEncoderProcessOutput(enc, &next, &avail);
		written = (size_t)(next - s_img);
		if (st == JXL_ENC_NEED_MORE_OUTPUT)
			ensure(&s_img, &s_img_cap, s_img_cap * 2);
	} while (st == JXL_ENC_NEED_MORE_OUTPUT);
	JxlEncoderDestroy(enc);
	if (st != JXL_ENC_SUCCESS)
		return false;

	emit_cached_image("d.jxl", "DrawJXL", s_img, written);
	return true;
}
#endif // WITH_JXL

static void emit_frame(const uint32_t *fb, int w, int h)
{
	bool built = false;

	if (out_pending())
		return;                         // our buffer still draining -> drop this one
	if (g_awaiting_ack && (int32_t)(g_ack_deadline - now_ms()) > 0)
		return;                         // terminal hasn't reported rendering the last frame yet

	g_out_len = 0; g_out_off = 0;       // begin a fresh frame buffer
	if (g_mode == MODE_TEXT) {          // ANSI/CP437 block-char tier (render_text)
		size_t      tlen;
		const char *tb = rt_render_frame(&tlen);
		out_put(tb, tlen);
	} else {
		pack_rgb(fb, w, h);
		if (g_mode == MODE_SIXEL) { emit_frame_sixel(w, h); built = true; }
#ifdef WITH_JXL
		else if (g_mode == MODE_JXL)
			built = emit_frame_jxl(w, h);
#endif
		if (!built)
			emit_frame_ppm(w, h);
	}

	out_put("\x1b[6n", 4);              // DSR -> paces us to the terminal's render rate
	g_awaiting_ack = 1;
	g_ack_deadline = now_ms() + 250;    // safety: proceed if the report is lost
	g_tx_bytes += g_out_len;            // telemetry: this frame's wire bytes (incl. the DSR)
	g_tx_frames++;
	out_flush();
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

static uint32_t now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

// ---------------------------------------------------------------------------
// Input: stdin escape-sequence parser + synthesized key-up
// ---------------------------------------------------------------------------

#define KEYQ_SIZE 64
static uint16_t s_keyq[KEYQ_SIZE];   // (pressed<<8)|doomkey
static int      s_keyq_r = 0, s_keyq_w = 0;

static void keyq_push(int pressed, unsigned char key)
{
	s_keyq[s_keyq_w] = (uint16_t)((pressed << 8) | key);
	s_keyq_w = (s_keyq_w + 1) % KEYQ_SIZE;
	if (s_keyq_w == s_keyq_r)                 // overflow: drop oldest
		s_keyq_r = (s_keyq_r + 1) % KEYQ_SIZE;
}

// Active (held) keys, for key-up synthesis. A terminal never sends key-up, so we
// release a held key on a timeout after its last byte. Like doom-cli we use *two*
// graces driven by how many times we've seen the key: the OS auto-repeats a held
// key as a single byte, a long initial *delay* (~250-500ms), then a faster *rate*
// (~100ms). So while a press is still "fresh" (one sighting) we wait g_grace_fresh
// (-kpdelay) -- long enough to bridge that initial delay so held movement doesn't
// stutter at the start; once the key has clearly repeated we wait only
// g_keyup_idle_ms (-kpsmooth) so releasing it stops movement promptly. The cost is
// that a single tap lingers ~g_grace_fresh; lower -kpdelay if taps creep too much.
//
// Turn keys (LEFT/RIGHT arrows) are the exception: turning is precision work and
// Doom ramps turn *acceleration* the longer the key is held, so a long fresh grace
// makes a quick tap over-rotate. They get a much shorter fresh grace (g_turn_grace,
// -kpturn) for tight aim taps -- the tradeoff is a brief hitch at the start of a
// sustained spin, which is far less noticeable than tap overshoot. Sustained
// forward/back/strafe movement keeps the long grace (smooth, no start stutter).
#define ACTIVE_MAX 16
static uint32_t g_keyup_idle_ms = 150;        // repeat grace (key seen >= 2 times)
static uint32_t g_grace_fresh   = 500;        // fresh grace (key seen once)
static uint32_t g_turn_grace    = 180;        // fresh grace for turn keys (tight taps)
static struct { unsigned char key; uint32_t last; int presses; } s_active[ACTIVE_MAX];
static int      s_active_n = 0;

// doom m_menu.c: nonzero while a menu is open (boolean == unsigned int). The input
// model switches on it. IN A MENU each key byte is a discrete press (down+up) so
// rapid same-key taps never merge -- a hold-with-timeout fundamentally can't tell a
// fast re-tap from terminal auto-repeat, so any grace short enough to separate taps
// is too short to bridge a hold. IN-GAME we keep the hold-synthesis below (movement
// needs the key to stay "down" between the terminal's auto-repeat bytes).
extern unsigned int menuactive;

static void key_seen(unsigned char key)
{
	int      i;
	uint32_t t = now_ms();

	if (menuactive) {                         // menu: one discrete tap per byte
		keyq_push(1, key);
		keyq_push(0, key);
		return;
	}
	for (i = 0; i < s_active_n; i++) {
		if (s_active[i].key == key) {         // repeat sighting: refresh + confirm hold
			s_active[i].last = t;
			s_active[i].presses++;
			return;
		}
	}
	if (s_active_n < ACTIVE_MAX) {
		s_active[s_active_n].key = key;
		s_active[s_active_n].last = t;
		s_active[s_active_n].presses = 1;
		s_active_n++;
		keyq_push(1, key);                    // key-down on first sighting
	}
}

static void expire_keys(void)
{
	uint32_t t = now_ms();
	int      i = 0;
	while (i < s_active_n) {
		unsigned char k = s_active[i].key;
		uint32_t      fresh = menuactive ? g_keyup_idle_ms                                  // menus: discrete taps
		                      : (k == KEY_LEFTARROW || k == KEY_RIGHTARROW) ? g_turn_grace   // turning: tight taps
		                      : g_grace_fresh;                                               // movement: smooth holds
		uint32_t      grace = (s_active[i].presses >= 2) ? g_keyup_idle_ms : fresh;
		if (t - s_active[i].last >= grace) {
			keyq_push(0, s_active[i].key);     // synthesized key-up
			s_active[i] = s_active[--s_active_n];
		} else
			i++;
	}
}

// Map a single terminal byte (non-escape) to a Doom key, 0 = ignore.
static unsigned char map_ascii(unsigned char c)
{
	if (c == '\r' || c == '\n')
		return KEY_ENTER;
	if (c == 0x1b)
		return KEY_ESCAPE;
	if (c == '\t')
		return KEY_TAB;
	if (c == 0x7f || c == 0x08)
		return KEY_BACKSPACE;
	// Terminal bindings matching the doom-ascii port (Doom's Ctrl/Alt/Shift
	// defaults are modifiers a terminal can't send alone). The only action on a
	// letter is USE ('e'); its UPPERCASE form falls through to a literal 'e'
	// below, so cheat codes / save-names still work when typed shifted (CAPS).
	if (c == ' ')
		return KEY_FIRE;                               // fire
	if (c == 'e')
		return KEY_USE;                                // use (lowercase only)
	if (c == ',')
		return KEY_STRAFE_L;                           // strafe left
	if (c == '.')
		return KEY_STRAFE_R;                           // strafe right
	if (c == ']')
		return KEY_RSHIFT;                             // run / speed
	if (c >= 'A' && c <= 'Z')
		return (unsigned char)(c - 'A' + 'a');                          // CAPS -> literal lowercase (cheats/text)
	if (c >= ' ' && c < 0x7f)
		return c;                               // lowercase letters / other printables pass through
	return 0;
}

// Incremental escape-sequence parser state (bytes may split across reads).
static enum { ST_NORMAL, ST_ESC, ST_CSI } s_pstate = ST_NORMAL;
static char s_csi_intro = 0;            // '[' or 'O' for the current CSI/SS3

static void parse_byte(unsigned char c)
{
	switch (s_pstate) {
		case ST_NORMAL:
			if (c == 0x1b) { s_pstate = ST_ESC; return; }
			{ unsigned char k = map_ascii(c); if (k)
				  key_seen(k); }
			return;
		case ST_ESC:
			if (c == '[' || c == 'O') { s_csi_intro = c; s_pstate = ST_CSI; return; }
			// lone ESC then a byte: treat ESC as Escape, reprocess the byte
			key_seen(KEY_ESCAPE);
			s_pstate = ST_NORMAL;
			parse_byte(c);
			return;
		case ST_CSI:
			switch (c) {
				case 'A': key_seen(KEY_UPARROW);    break;
				case 'B': key_seen(KEY_DOWNARROW);  break;
				case 'C': key_seen(KEY_RIGHTARROW); break;
				case 'D': key_seen(KEY_LEFTARROW);  break;
				case 'R':                           // CSI r;c R = DSR report = frame rendered
					if (s_csi_intro == '[')
						g_awaiting_ack = 0;
					break;
				default: break;   // ignore params / other finals for now
			}
			// final byte is a letter (0x40-0x7e and not a param/intermediate)
			if (c >= 0x40 && c <= 0x7e)
				s_pstate = ST_NORMAL;
			return;
	}
}

static void pump_input(void)
{
	unsigned char buf[256];
	for (;;) {
		ssize_t n = conn_read(buf, sizeof(buf));
		if (n > 0) {
			ssize_t i;
			for (i = 0; i < n; i++) parse_byte(buf[i]);
			continue;
		}
		if (n == 0)
			g_hangup = 1;                    // EOF / dead socket
		break;                               // n<0: no data right now
	}
	// A dangling lone ESC at end-of-input resolves to Escape after a beat;
	// KEYUP_IDLE handling and the next read will sort timing out.
	expire_keys();
}

// Exit the door (cleanly, via atexit) once the client has dropped.
static void check_hangup(void)
{
	if (g_hangup)
		exit(0);
}

// Honor the user's BBS time limit (-t<seconds>, from %T): exit when it's spent.
static void check_timelimit(void)
{
	if (g_time_limit_ms &&
	    (int32_t)(now_ms() - (g_start_ms + g_time_limit_ms)) >= 0) {
		static const char msg[] = "\x1b[?25h\r\nYour time on the BBS has expired.\r\n";
		emit_all(msg, sizeof(msg) - 1);
		exit(0);
	}
}

// ---------------------------------------------------------------------------
// Terminal raw mode (matters for standalone testing in a tty; harmless on a
// socket where tcgetattr fails)
// ---------------------------------------------------------------------------

#ifndef _WIN32
static struct termios s_oldtio;
static int            s_tio_saved = 0;
#endif

// *nix stdio-fallback only (dev pipe/tty testing). The real socket path is set
// non-blocking in tune_socket(); nothing to do here for it.
static void raw_input_on(void)
{
#ifndef _WIN32
	struct termios t;
	int            fl;
	if (g_sock)
		return;
	if (tcgetattr(g_rfd, &t) == 0) {           // only succeeds on a tty
		s_oldtio = t; s_tio_saved = 1;
		t.c_lflag &= ~(ICANON | ECHO | ISIG);
		t.c_iflag &= ~(ICRNL | INLCR | IXON);
		t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
		tcsetattr(g_rfd, TCSANOW, &t);
	}
	fl = fcntl(g_rfd, F_GETFL, 0);
	if (fl != -1)
		fcntl(g_rfd, F_SETFL, fl | O_NONBLOCK);
#endif
}

static void raw_input_off(void)
{
#ifndef _WIN32
	if (s_tio_saved)
		tcsetattr(g_rfd, TCSANOW, &s_oldtio);
#endif
}

// ---------------------------------------------------------------------------
// JXL support: -1 auto-probe, 0 force PPM, 1 force JXL.
// ---------------------------------------------------------------------------

static int g_jxl_pref = -1;
static int g_is_syncterm = 0;   // SyncTERM detected (Q;JXL reply, or CTerm version >= 1.2)
static int g_cterm_version = 0; // CTerm/SyncTERM version maj*1000+min (DA1 reply); 0 = unknown

#ifdef WITH_JXL
// One-time startup handshake: ask SyncTERM whether it can decode JXL.
// Reply is the CTerm state report "ESC [ = 1 ; {0,1} - n". Bytes seen during
// the window are also fed to the input parser so a keypress isn't lost (the
// CSI reply is inert there: it ends on the 'n' final and yields no key).
static int probe_jxl(void)
{
	static const char q[] = "\x1b_SyncTERM:Q;JXL\x1b\\";
	unsigned char     acc[256];
	int               al = 0, result = -1;
	uint32_t          deadline = now_ms() + 600;

	emit_all(q, sizeof(q) - 1);
	while (result < 0) {
		int32_t       rem = (int32_t)(deadline - now_ms());
		unsigned char buf[256];
		ssize_t       n, i;
		int           j;

		if (rem <= 0)
			break;
		if (g_sock) {
			if (!socket_readable(g_iosock, rem))
				break;
		}
#ifndef _WIN32
		else {
			struct pollfd pfd;
			pfd.fd = g_rfd; pfd.events = POLLIN; pfd.revents = 0;
			if (poll(&pfd, 1, rem) <= 0)
				break;
		}
#endif
		n = conn_read(buf, sizeof(buf));
		if (n == 0) { g_hangup = 1; break; }   // hangup
		if (n < 0)
			continue;                           // spurious wake; retry within deadline
		for (i = 0; i < n; i++) {
			parse_byte(buf[i]);
			if (al < (int)sizeof(acc))
				acc[al++] = buf[i];
		}
		for (j = 0; j + 6 < al; j++) {
			if (acc[j] == 0x1b && acc[j + 1] == '[' && acc[j + 2] == '=' &&
			    acc[j + 3] == '1' && acc[j + 4] == ';' &&
			    (acc[j + 5] == '0' || acc[j + 5] == '1') && acc[j + 6] == '-') {
				result = acc[j + 5] - '0';
				break;
			}
		}
	}
	g_is_syncterm = (result >= 0);   // any reply (0 or 1) means it's SyncTERM
	return result == 1;
}
#endif

// ---------------------------------------------------------------------------
// Sixel support: -1 auto-probe (DA1), 0 force off, 1 force on. Independent of
// JXL -- a build without libjxl can still serve the sixel tier.
// ---------------------------------------------------------------------------

static int g_sixel_pref = -1;

// One-time DA1 (Primary Device Attributes) probe: send ESC[c and look for the
// sixel capability (parameter "4") in the reply "ESC [ ? <params> c". Bytes are
// also fed to the input parser; the reply is inert there (ends on 'c', no key).
static int probe_sixel(void)
{
	static const char q[] = "\x1b[c";
	unsigned char     acc[256];
	int               al = 0, done = 0, found = 0;
	uint32_t          deadline = now_ms() + 600;

	emit_all(q, sizeof(q) - 1);
	while (!done) {
		int32_t       rem = (int32_t)(deadline - now_ms());
		unsigned char buf[256];
		ssize_t       n, i;
		int           j;

		if (rem <= 0)
			break;
		if (g_sock) {
			if (!socket_readable(g_iosock, rem))
				break;
		}
#ifndef _WIN32
		else {
			struct pollfd pfd;
			pfd.fd = g_rfd; pfd.events = POLLIN; pfd.revents = 0;
			if (poll(&pfd, 1, rem) <= 0)
				break;
		}
#endif
		n = conn_read(buf, sizeof(buf));
		if (n == 0) { g_hangup = 1; break; }   // hangup
		if (n < 0)
			continue;                           // spurious wake; retry within deadline
		for (i = 0; i < n; i++) {
			parse_byte(buf[i]);
			if (al < (int)sizeof(acc))
				acc[al++] = buf[i];
		}
		// DA1 reply: ESC[?<params>c (xterm/WT: param 4 = sixel) OR SyncTERM's CTerm
		// version ESC[=67;84;101;114;109;<maj>;<min>c (the params spell "CTerm").
		for (j = 0; j + 2 < al; j++) {
			unsigned char marker = acc[j + 2];
			int           p[8], np = 0, num = -1, k;
			if (acc[j] != 0x1b || acc[j + 1] != '[' || (marker != '?' && marker != '='))
				continue;
			for (k = j + 3; k < al; k++) {
				unsigned char ch = acc[k];
				if (ch >= '0' && ch <= '9') {
					num = (num < 0 ? 0 : num * 10) + (ch - '0');
				} else if (ch == ';') {
					if (np < 8)
						p[np++] = (num < 0 ? 0 : num);
					num = -1;
				} else {                         // final byte
					if (np < 8)
						p[np++] = (num < 0 ? 0 : num);
					if (ch == 'c') {
						int m;
						if (marker == '?') {
							for (m = 0; m < np; m++)
								if (p[m] == 4)
									found = 1;                       // sixel
						} else if (np >= 7 && p[0] == 67 && p[1] == 84 &&
						           p[2] == 101 && p[3] == 114 && p[4] == 109) {
							g_cterm_version = p[5] * 1000 + p[6];    // "CTerm" maj.min
							if (g_cterm_version >= 1002)             // SyncTERM >= 1.2 has APC PPM
								g_is_syncterm = 1;
						}
						done = 1;
					}
					break;
				}
			}
			break;
		}
	}
	return found;
}

// ---------------------------------------------------------------------------
// doomgeneric backend hooks
// ---------------------------------------------------------------------------

// Keep end-to-end latency low: small send buffer so frames can't pile up deep
// in the kernel queue, and TCP_NODELAY so frame bytes aren't held by Nagle.
static void tune_socket(void)
{
	int one = 1, sz = 96 * 1024;     // ~2 JXL frames' worth
	setsockopt(g_iosock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
	setsockopt(g_iosock, SOL_SOCKET, SO_SNDBUF, (char *)&sz, sizeof sz);
	// Non-blocking: writes return partial (staged out_flush), reads gate on
	// socket_readable(). fcntl on *nix, ioctlsocket on Windows.
#ifdef _WIN32
	{ ulong nb = 1; ioctlsocket(g_iosock, FIONBIO, &nb); }
#else
	{ int fl = fcntl(g_iosock, F_GETFL, 0); if (fl != -1)
		  fcntl(g_iosock, F_SETFL, fl | O_NONBLOCK); }
#endif
}

// Restore auto-wrap + cursor on exit (so the BBS prompt behaves after Doom quits).
static void terminal_restore(void) { emit_all("\x1b[?7h\x1b[?25h", 11); }

// Configure the block-char text tier: mode from -mode, else by terminal charset
// (UTF-8 -> hi-res sextant, CP437 -> half-block). Used by -text and as the
// no-JXL fallback.
static void setup_text_mode(void)
{
	// Default: pick the tier from the client charset (terminal.ini) -- UTF-8 gets
	// the hi-res sextant, CP437 the half-block. This assumes the door is set
	// "Translate Character Set: No" (EX_BIN) so our native UTF-8 reaches the
	// terminal untranslated. A sysop whose terminal/font can't do sextants (or who
	// leaves the door translated) forces CP437 with -charset cp437 (or -mode half).
	rt_mode_t mode = g_rt_mode_set ? g_rt_mode
	               : (g_rt_charset == RT_UTF8 ? RT_SEXTANT : RT_HALF);
	rt_config(g_cols, s_lines, mode, g_colors, g_rt_charset);
	g_mode = MODE_TEXT;
}

static const char *mode_name(enum frame_mode m)
{
	switch (m) {
		case MODE_JXL:   return "jxl";
		case MODE_SIXEL: return "sixel";
		case MODE_TEXT:  return "text";
		default:         return "ppm";
	}
}

// At-exit telemetry -> BBS log: how much we actually sent and how fast.
static void emit_telemetry(void)
{
	uint32_t elapsed = now_ms() - g_start_ms;
	double   secs = elapsed / 1000.0;

	if (g_tx_frames == 0 || secs <= 0.0)
		return;
	dlog("telemetry: %s %dx%d | %u frames in %.1fs = %.1f fps | %llu bytes "
	     "(%.0f/frame avg) | %.1f KB/s avg",
	     mode_name(g_mode), s_pxW, s_pxH, g_tx_frames, secs, g_tx_frames / secs,
	     (unsigned long long)g_tx_bytes, (double)g_tx_bytes / g_tx_frames,
	     g_tx_bytes / secs / 1024.0);
}

// Probe the terminal for its real PIXEL size so "fit" can fill a real window
// whose cells aren't 8x16. ESC[14t -> text-area pixels (reply ESC[4;H;W t),
// ESC[16t -> cell pixels (reply ESC[6;CH;CW t). SyncTERM doesn't do those but
// answers ESC[?2;1S (XTSMGRAPHICS) -> ESC[?2;0;W;H S = graphics-canvas pixels.
// A trailing DSR (ESC[6n -> ESC[r;cR) is the sentinel every terminal answers, so
// we stop as soon as it arrives. Terminals without any of these -> g_win_* stay 0
// and we fall back to the cell model.
static void probe_geometry(void)
{
	static const char q[] = "\x1b[14t\x1b[16t\x1b[?2;1S\x1b[6n";
	unsigned char     acc[256];
	int               al = 0, done = 0;
	uint32_t          deadline = now_ms() + 600;

	emit_all(q, sizeof(q) - 1);
	while (!done) {
		int32_t       rem = (int32_t)(deadline - now_ms());
		unsigned char buf[256];
		ssize_t       n, i;
		int           j;

		if (rem <= 0)
			break;
		if (g_sock) {
			if (!socket_readable(g_iosock, rem))
				break;
		}
#ifndef _WIN32
		else {
			struct pollfd pfd;
			pfd.fd = g_rfd; pfd.events = POLLIN; pfd.revents = 0;
			if (poll(&pfd, 1, rem) <= 0)
				break;
		}
#endif
		n = conn_read(buf, sizeof(buf));
		if (n == 0) { g_hangup = 1; break; }
		if (n < 0)
			continue;
		for (i = 0; i < n; i++) {
			parse_byte(buf[i]);
			if (al < (int)sizeof(acc))
				acc[al++] = buf[i];
		}
		for (j = 0; j + 2 < al; j++) {       // scan for ESC [ [?|=] <params> <final>
			int           p[4] = { 0, 0, 0, 0 }, np = 0, num = -1, k;
			unsigned char marker = 0;
			if (acc[j] != 0x1b || acc[j + 1] != '[')
				continue;
			k = j + 2;
			if (k < al && (acc[k] == '?' || acc[k] == '=' || acc[k] == '<' || acc[k] == '>'))
				marker = acc[k++];
			for (; k < al; k++) {
				unsigned char ch = acc[k];
				if (ch >= '0' && ch <= '9') {
					num = (num < 0 ? 0 : num * 10) + (ch - '0');
				} else if (ch == ';') {
					if (np < 4)
						p[np++] = (num < 0 ? 0 : num);
					num = -1;
				} else {                     // final byte
					if (np < 4)
						p[np++] = (num < 0 ? 0 : num);
					if (marker == 0 && ch == 't' && np >= 3 && p[0] == 4)      { g_win_h = p[1];  g_win_w = p[2]; }
					else if (marker == 0 && ch == 't' && np >= 3 && p[0] == 6) { g_cell_h = p[1]; g_cell_w = p[2]; }
					else if (marker == '?' && ch == 'S' && np >= 4 && p[0] == 2 && p[1] == 0 && g_win_w == 0) {
						g_win_w = p[2];      // SyncTERM graphics canvas (XTSMGRAPHICS), if ESC[14t didn't answer
						g_win_h = p[3];
					}
					else if (marker == 0 && ch == 'R')
						done = 1;                                            // DSR sentinel
					break;
				}
			}
		}
	}
	// Only the cell size came back? Derive the window from cols/rows * cell.
	if (g_win_w == 0 && g_cell_w > 0) {
		g_win_w = g_cols * g_cell_w;
		g_win_h = s_lines * g_cell_h;
	}
}

void DG_Init(void)
{
	raw_input_on();
	atexit(raw_input_off);
	atexit(terminal_restore);
	atexit(emit_telemetry);
	if (g_sock)
		tune_socket();
	g_start_ms = now_ms();
	// Clear (so leftover BBS text doesn't show through a letterboxed image) + hide
	// cursor (DECTCEM) + disable auto-wrap (DECAWM): a full-width text row would
	// otherwise wrap *and* take our newline -> a blank line every other row.
	emit_all("\x1b[2J\x1b[?25l\x1b[?7l", 15);
	probe_geometry();                // learn the terminal's real pixel size (ESC[14t/16t/?2;1S)

	// Graphics-tier auto-selection order: JXL (SyncTERM, cached, tiny) > Sixel
	// (any sixel-capable terminal) > PPM (SyncTERM APC when it has neither) > text.
	// Explicit -jxl/-sixel flags short-circuit the probes.
	if (g_force_text)
		setup_text_mode();                                      // -text: forced block-char tier
#ifdef WITH_JXL
	else if (g_jxl_pref == 1)
		g_mode = MODE_JXL;                                      // force JXL
#endif
	else if (g_sixel_pref == 1)
		g_mode = MODE_SIXEL;                                   // force sixel
#ifdef WITH_JXL
	else if (g_jxl_pref == 0)
		g_mode = MODE_PPM;                                     // explicit opt-in to slow PPM
	else if (probe_jxl())
		g_mode = MODE_JXL;                                     // auto: JXL (sets g_is_syncterm)
#endif
	else if (g_sixel_pref != 0 && probe_sixel())
		g_mode = MODE_SIXEL;                                           // auto: sixel
	else
		setup_text_mode();              // otherwise text -- PPM is never auto-selected (it's
	                                    // unusable over a real link, even at 320x200; a SyncTERM
	                                    // 1.2-1.3 lands here -> playable text. PPM is opt-in via
	                                    // -jxl 0 above, for the localhost/LAN case only.

	compute_geometry();              // emitted image size + centering (now that the tier is known)

	// Startup diagnostic -> BBS log (syslog/journalctl). Confirms the running
	// build, which terminal.ini (if any) was read, the size taken from it, and
	// the resulting emitted-image geometry + tier.
	dlog("build " __DATE__ " " __TIME__ " | term=%s (%s) | cols=%d rows=%d desc=\"%s\" cterm=%d"
	     " | win=%dx%d cell=%dx%d | image %dx%d @%d,%d scale=%s mode=%s",
	     g_diag_term, g_diag_term_ok ? "read" : "NOT FOUND",
	     g_cols, s_lines, g_diag_desc, g_cterm_version, g_win_w, g_win_h, g_cell_w, g_cell_h,
	     s_pxW, s_pxH, g_img_x, g_img_y, g_scale_fit ? "fit" : "off", mode_name(g_mode));
}

void DG_DrawFrame(void)
{
	int h = s_pxH;                // emitted dims (may exceed 640x400 when scaling up)
	int w = s_pxW;
	pump_input();                 // service input first, every tick
	out_flush();                  // keep draining any in-flight frame
	emit_frame(DG_ScreenBuffer, w, h);   // builds+sends only if drained, else drops
	pump_input();
	check_hangup();
	check_timelimit();
}

void DG_SleepMs(uint32_t ms)
{
	struct timespec ts;
	ts.tv_sec  = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}

uint32_t DG_GetTicksMs(void) { return now_ms(); }

int DG_GetKey(int *pressed, unsigned char *key)
{
	pump_input();
	check_hangup();
	if (s_keyq_r == s_keyq_w)
		return 0;
	uint16_t d = s_keyq[s_keyq_r];
	s_keyq_r = (s_keyq_r + 1) % KEYQ_SIZE;
	*pressed = d >> 8;
	*key     = d & 0xff;
	return 1;
}

void DG_SetWindowTitle(const char *title) { (void)title; }

// ---------------------------------------------------------------------------
// DOOR32.SYS drop file (the portable door interface; standard on Windows BBSes
// and written by Synchronet too). We read line 1 = comm type (2 = telnet
// socket), line 2 = comm/socket handle, line 9 = time left in MINUTES. Screen
// rows are NOT in DOOR32.SYS, so keep -l%R for that (else the 25-line default).
// ---------------------------------------------------------------------------
// True if `s` is a path whose basename is "door32.sys" (case-insensitive), so a
// bare drop-file path (Synchronet's %f) self-triggers the read -- no -door32
// flag needed. The path need not be in cwd.
static int is_door32_path(const char *s)
{
	static const char want[] = "door32.sys";
	const char *      base = s, *p;
	int               i;

	for (p = s; *p; p++)
		if (*p == '/' || *p == '\\')
			base = p + 1;
	for (i = 0; want[i] != '\0'; i++) {
		char c = base[i];
		if (c >= 'A' && c <= 'Z')
			c += 32;
		if (c != want[i])
			return 0;
	}
	return base[i] == '\0';
}

static void read_door32(const char *path)
{
	FILE *f = fopen(path, "r");
	char  line[256];
	int   ln = 0, commtype = -1, handle = -1, tmin = -1;

	if (f == NULL)
		return;
	while (fgets(line, sizeof(line), f) != NULL) {
		switch (ln) {
			case 0: commtype = atoi(line); break;   // 0=local 1=serial 2=telnet
			case 1: handle   = atoi(line); break;   // comm/socket descriptor
			case 8: tmin     = atoi(line); break;   // time left, minutes
		}
		if (++ln > 8)
			break;
	}
	fclose(f);

	if (commtype == 2 && handle >= 0) { g_iosock = (SOCKET)handle; g_sock = true; }
	if (tmin > 0)
		g_time_limit_ms = (uint32_t)tmin * 60000u;
}

// ---------------------------------------------------------------------------
// Per-user storage: chdir into the full path given by -home so Doom's config,
// savegames, screenshots and the shared temp.dsg (all default to cwd ".") land
// in that user's directory -- no inter-node collisions, and the user keeps
// saves/keybinds across sessions. The caller passes the COMPLETE per-user path
// (e.g. Synchronet's auto-cleaned data/user/####/ area); we use it verbatim and
// never append anything. No -home -> use cwd as-is.
// ---------------------------------------------------------------------------

// (mkpath() comes from xpdev/dirwrap.h -- recursive mkdir of every component.)

// Resolve a (possibly relative) path to absolute -- called BEFORE the chdir so
// -iwad/-file still load afterwards.
static char *abscopy(const char *p)
{
	char buf[PATH_MAX], cwd[PATH_MAX];
	if (p[0] == '/')
		return strdup(p);
	if (realpath(p, buf) != NULL)
		return strdup(buf);
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		snprintf(buf, sizeof(buf), "%s/%s", cwd, p);
		return strdup(buf);
	}
	return strdup(p);
}

static void setup_sandbox(void)
{
	if (g_home[0] == '\0')
		return;                          // no -home: leave cwd as-is
	mkpath(g_home);
	if (chdir(g_home) != 0)
		fprintf(stderr, "syncdoom: cannot enter storage dir '%s': %s\n", g_home, strerror(errno));
}

// ---------------------------------------------------------------------------
// Auto-detect cols/rows/charset from Synchronet's <node_dir>/terminal.ini.
// Located via -term (file or dir) > $SBBSNODE > the drop-file's directory.
// Sets the baseline; explicit -l / -charset (parsed after) still override.
// Absent (non-Synchronet BBS) -> keep defaults (80 cols, -l rows, CP437).
// ---------------------------------------------------------------------------
static void read_terminal_ini(void)
{
	char        path[PATH_MAX] = "";
	const char *node;
	FILE *      f;
	str_list_t  ini;
	char        cs[INI_MAX_VALUE_LEN];
	char        dsc[INI_MAX_VALUE_LEN];
	int         cols, rows;

	if (g_term_path[0]) {
		if (isdir(g_term_path))
			snprintf(path, sizeof(path), "%s/terminal.ini", g_term_path);
		else
			snprintf(path, sizeof(path), "%s", g_term_path);
	} else if ((node = getenv("SBBSNODE")) != NULL && *node) {
		snprintf(path, sizeof(path), "%s/terminal.ini", node);
	} else if (g_door32_path[0]) {           // terminal.ini sits beside the drop file
		char *base;
		snprintf(path, sizeof(path), "%s", g_door32_path);
		base = getfname(path);               // -> filename within path
		strcpy(base, "terminal.ini");
	}
	if (path[0]) {
		strncpy(g_diag_term, path, sizeof(g_diag_term) - 1);
		g_diag_term[sizeof(g_diag_term) - 1] = '\0';
	}
	if (!path[0])
		return;

	f = fopen(path, "r");
	if (f == NULL)
		return;
	g_diag_term_ok = 1;
	ini = iniReadFile(f);
	fclose(f);
	cols = iniGetInteger(ini, ROOT_SECTION, "cols", 0);
	rows = iniGetInteger(ini, ROOT_SECTION, "rows", 0);
	iniGetString(ini, ROOT_SECTION, "chars", "", cs);
	iniGetString(ini, ROOT_SECTION, "desc", "", dsc);   // TERM string, e.g. "xterm-256color"
	strncpy(g_diag_desc, dsc, sizeof(g_diag_desc) - 1);
	strListFree(&ini);

	if (cols > 0)
		g_cols  = cols;
	if (rows > 0)
		s_lines = rows;
	// Charset drives the default text tier (UTF-8 -> sextant, else half). Assumes
	// the door is set "Translate Character Set: No" so our native UTF-8 passes
	// through untranslated; an explicit -charset (parsed later) overrides this.
	if (cs[0]) g_rt_charset = (stricmp(cs, "utf-8") == 0 || stricmp(cs, "utf8") == 0)
		                     ? RT_UTF8 : RT_CP437;
	// Synchronet doesn't track color depth directly, so infer it from the TERM
	// string (desc): "...256color" -> 256, "truecolor"/"direct" -> 24-bit, else 16.
	if (strstr(dsc, "truecolor") != NULL || strstr(dsc, "direct") != NULL)
		g_colors = RT_24BIT;
	else if (strstr(dsc, "256color") != NULL)
		g_colors = RT_8BIT;
}

// ---------------------------------------------------------------------------
// syncdoom.ini -- the door's own config, beside the executable (portable to any
// BBS; not Synchronet's ctrl/). Located from argv[0]'s directory, cwd fallback.
// [video] scale = fit|off, scale_max = <px>.  Absent -> built-in defaults.
// ---------------------------------------------------------------------------
static void read_syncdoom_ini(const char *argv0)
{
	char       dir[PATH_MAX], path[PATH_MAX], val[INI_MAX_VALUE_LEN];
	char *     base;
	FILE *     f;
	str_list_t ini;

	strncpy(dir, argv0, sizeof(dir) - 1); dir[sizeof(dir) - 1] = '\0';
	base = getfname(dir);                          // -> filename within dir
	if (base != dir) { *base = '\0'; snprintf(path, sizeof(path), "%ssyncdoom.ini", dir); }
	else             { snprintf(path, sizeof(path), "syncdoom.ini"); }   // bare argv0 -> cwd

	f = fopen(path, "r");
	if (f == NULL)
		return;                                     // no ini -> keep defaults
	ini = iniReadFile(f);
	fclose(f);

	// All [video]/[input] keys below are optional; an absent key keeps the value
	// already set (terminal.ini auto-detect or built-in default). The command-line
	// flags are parsed after this, so they override anything set here.
	iniGetString(ini, "video", "scale", "", val);
	if (val[0]) g_scale_fit = !(stricmp(val, "off")    == 0 || stricmp(val, "0") == 0 ||
		                        stricmp(val, "native") == 0 || stricmp(val, "no") == 0);
	g_scale_max = iniGetInteger(ini, "video", "scale_max", g_scale_max);

	// charset = cp437|utf8 -- force the text-tier charset (else terminal.ini auto)
	iniGetString(ini, "video", "charset", "", val);
	if (val[0])
		g_rt_charset = (stricmp(val, "utf8") == 0 || stricmp(val, "utf-8") == 0) ? RT_UTF8 : RT_CP437;

	// colors = 16|256|true -- force color depth (else terminal.ini desc auto)
	iniGetString(ini, "video", "colors", "", val);
	if (val[0]) {
		if (stricmp(val, "true") == 0 || stricmp(val, "truecolor") == 0 || strcmp(val, "24") == 0)
			g_colors = RT_24BIT;
		else if (strcmp(val, "256") == 0)
			g_colors = RT_8BIT;
		else if (strcmp(val, "8") == 0)
			g_colors = RT_3BIT;
		else
			g_colors = RT_4BIT;
	}

	// mode = half|quadrant|sextant|space -- force the text sub-cell glyph tier
	iniGetString(ini, "video", "mode", "", val);
	if (val[0]) {
		if (stricmp(val, "sextant") == 0)
			g_rt_mode = RT_SEXTANT;
		else if (stricmp(val, "quadrant") == 0)
			g_rt_mode = RT_QUADRANT;
		else if (stricmp(val, "space") == 0)
			g_rt_mode = RT_SPACE;
		else
			g_rt_mode = RT_HALF;
		g_rt_mode_set = 1;
	}

	// tier = auto|text|jxl|sixel|ppm -- force the render tier (else auto-probe)
	iniGetString(ini, "video", "tier", "", val);
	if (val[0]) {
		if (stricmp(val, "text") == 0)
			g_force_text = 1;
		else if (stricmp(val, "jxl") == 0)
			g_jxl_pref = 1;
		else if (stricmp(val, "sixel") == 0)
			g_sixel_pref = 1;
		else if (stricmp(val, "ppm") == 0)
			g_jxl_pref = 0;
		/* "auto" -> leave the auto-probe defaults */
	}

	// [input] key-up-synthesis graces, in ms (see -kpsmooth / -kpdelay / -kpturn)
	{
		int v;
		if ((v = iniGetInteger(ini, "input", "kpsmooth", -1)) >= 0)
			g_keyup_idle_ms = (uint32_t)v;
		if ((v = iniGetInteger(ini, "input", "kpdelay",  -1)) >= 0)
			g_grace_fresh   = (uint32_t)v;
		if ((v = iniGetInteger(ini, "input", "kpturn",   -1)) >= 0)
			g_turn_grace    = (uint32_t)v;
	}

	strListFree(&ini);
}

// Compute the emitted bitmap dimensions (s_pxW x s_pxH) from the terminal's
// cell-pixel viewport and the scale config. Doom's frame is 8:5 (640:400); we fit
// the largest 8:5 rectangle into the viewport, capped at g_scale_max width.
static void compute_geometry(void)
{
	int vw, vh, cw, ch;

	if (g_win_w > 0) {                  // real terminal pixels (probed) -- preferred
		vw = g_win_w;
		vh = g_win_h;
	} else {                            // fallback: assume SyncTERM 8x<cellH> cells
		vw = g_cols * 8;
		vh = s_lines * cell_height(s_lines);
	}
	cw = g_cell_w > 0 ? g_cell_w : 8;                  // cell pixels, for centering math
	ch = g_cell_h > 0 ? g_cell_h : cell_height(s_lines);

	if (g_mode == MODE_PPM) {           // PPM is uncompressed, so emit Doom's native
		s_pxW = 320;                    // 320x200 -- the 640x400 framebuffer is a lossless
		s_pxH = 200;                    // 2x of it, so this is ~1/4 the bytes (and centered,
	} else if (!g_scale_fit) {          // drawn small, since SyncTERM can't scale on draw).
		s_pxW = 640;                    // native: doomgeneric's 640x400 (vh-squished if shorter)
		s_pxH = (vh < 400) ? vh : 400;
	} else {
		int wmax = (g_scale_max > 0 && vw > g_scale_max) ? g_scale_max : vw;
		int w = wmax;
		int h = w * 400 / 640;          // 8:5
		if (h > vh) { h = vh; w = h * 640 / 400; }   // height-limited
		s_pxW = w < 1 ? 1 : w;
		s_pxH = h < 1 ? 1 : h;

		// Drop a SMALL letterbox by stretching to fill: when a leftover bar is
		// <=8% of the image, fill it (e.g. the 640x384 status-bar canvas -> full
		// 640 wide with a ~4% squish, nicer than side bars). Bigger gaps keep the
		// letterbox so a near-square window doesn't stretch Doom fat.
		if (vw > s_pxW && (vw - s_pxW) * 100 <= s_pxW * 8)
			s_pxW = vw;
		if (vh > s_pxH && (vh - s_pxH) * 100 <= s_pxH * 8)
			s_pxH = vh;
	}

	// Center the image in the window: pixel offset for the APC DX/DY (SyncTERM
	// JXL/PPM), cell offset for the sixel text cursor.
	g_img_x = (vw - s_pxW) / 2;
	g_img_y = (vh - s_pxH) / 2;
	if (g_img_x < 0)
		g_img_x = 0;
	if (g_img_y < 0)
		g_img_y = 0;
	g_img_col = 1 + g_img_x / cw;
	g_img_row = 1 + g_img_y / ch;
}

// ---------------------------------------------------------------------------
// main: read connection/session params (-door32 drop file and/or -s/-l/-t),
// set up the per-user sandbox (-home), inject -scaling 2 so Doom's 320x200
// fills 640x400, and pass everything else (e.g. -iwad) through to doomgeneric.
// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
	char **child;
	int    cn = 0, i;

	signal(SIGPIPE, SIG_IGN);    // a dead client -> EPIPE return, not signal death

	// Headless dedicated-server mode: a match's tic relay, with no terminal,
	// no DOOR32 user, and no per-user sandbox. Branch out before any terminal
	// or render setup. The lobby spawns these via mp_spawn_server(); they can
	// also be launched by hand for testing (syncdoom -dedicated -port <n>).
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-dedicated") == 0) {
			myargc = argc;       // net_udp InitServer reads -port from these
			myargv = argv;
			return mp_dedicated_main(60);   // TODO: idle timeout from [net]
		}
		// -spawnserver: daemonize a detached -dedicated server and exit, printing
		// "<pid> <port>" -- so the JS lobby can launch a match server with one
		// synchronous call. With no -port we allocate a free one ourselves.
		if (strcmp(argv[i], "-spawnserver") == 0) {
			int  j, port = 0;
			long pid;
			for (j = 1; j < argc; j++)
				if (strcmp(argv[j], "-port") == 0 && j + 1 < argc)
					port = atoi(argv[j + 1]);
			if (port <= 0)
				port = mp_alloc_port(20000, 20063);  // TODO: range from [net]
			if (port <= 0) {
				fprintf(stderr, "syncdoom: no free server port\n");
				return 1;
			}
			pid = mp_spawn_server(argv[0], port);
			printf("%ld %d\n", pid, port);   // lobby reads pid (liveness) + port
			return pid > 0 ? 0 : 1;
		}
	}

	// Drop file first, so an explicit -s/-t below can still override it. Either
	// an explicit -door32 <path> or a bare "...door32.sys" path (Synchronet's %f).
	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "-door32", 7) == 0) {
			const char *v = argv[i] + 7;
			if (*v == '\0' && i + 1 < argc)
				v = argv[i + 1];
			if (*v) { read_door32(v); strncpy(g_door32_path, v, sizeof(g_door32_path) - 1); }
		} else if (strncmp(argv[i], "-term", 5) == 0) {
			const char *v = argv[i] + 5;
			if (*v == '\0' && i + 1 < argc)
				v = argv[i + 1];
			if (*v)
				strncpy(g_term_path, v, sizeof(g_term_path) - 1);
		} else if (argv[i][0] != '-' && is_door32_path(argv[i])) {
			read_door32(argv[i]); strncpy(g_door32_path, argv[i], sizeof(g_door32_path) - 1);
		}
	}
	read_terminal_ini();             // baseline cols/rows/charset; explicit args below override
	read_syncdoom_ini(argv[0]);      // beside-exe config (graphics scaling, ...)

	// -l / -s / -t are prefix-matched: the BBS substitutes specifiers with NO space
	// (-l%R -> "-l66", -s%H -> "-s7", -t%T -> "-t1800"), so the value can be glued to
	// the flag. -term/-door32 (paths, read in the pre-scan) are prefix-matched too.
	// Every other flag is exact-matched, taking its value (if any) as the next argv
	// -- no glued form, so no prefix-collision traps for them.
	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "-l", 2) == 0) {
			const char *v = argv[i] + 2;
			if (*v == '\0' && i + 1 < argc)
				v = argv[++i];                               // "-l 24"
			if (*v)
				s_lines = atoi(v);
		} else if (strcmp(argv[i], "-jxl") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if      (!strcmp(v, "0") || !strcmp(v, "off") || !strcmp(v, "ppm"))
				g_jxl_pref = 0;
			else if (!strcmp(v, "1") || !strcmp(v, "on")  || !strcmp(v, "jxl"))
				g_jxl_pref = 1;
			else
				g_jxl_pref = -1;    // auto
			g_force_text = 0;       // an explicit -jxl overrides an ini "tier=text"
		} else if (strcmp(argv[i], "-sixel") == 0) {        // before -s (which is prefix-matched)
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if      (!strcmp(v, "0") || !strcmp(v, "off"))
				g_sixel_pref = 0;
			else if (!strcmp(v, "1") || !strcmp(v, "on") || !strcmp(v, "sixel")) {
				g_sixel_pref = 1;
				g_force_text = 0; g_jxl_pref = -1;   // -sixel 1 overrides an ini tier=text/jxl
			}
			else
				g_sixel_pref = -1;    // auto
		} else if (strncmp(argv[i], "-s", 2) == 0) {
			const char *v = argv[i] + 2;
			if (*v == '\0' && i + 1 < argc)
				v = argv[++i];                               // "-s 7"
			if (*v) { g_iosock = (SOCKET)atoi(v); g_sock = true; }
		} else if (strcmp(argv[i], "-text") == 0) {         // before -t (which is prefix-matched)
			g_force_text = 1;                                // force the block-char tier
		} else if (strncmp(argv[i], "-term", 5) == 0) {     // before -t; read in pre-scan, consume value here
			if (argv[i][5] == '\0')
				i++;
		} else if (strcmp(argv[i], "-charset") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			g_rt_charset = (stricmp(v, "utf8") == 0 || stricmp(v, "utf-8") == 0)
			               ? RT_UTF8 : RT_CP437;
		} else if (strcmp(argv[i], "-mode") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if      (strcmp(v, "sextant")  == 0)
				g_rt_mode = RT_SEXTANT;
			else if (strcmp(v, "quadrant") == 0)
				g_rt_mode = RT_QUADRANT;
			else if (strcmp(v, "space")    == 0)
				g_rt_mode = RT_SPACE;
			else
				g_rt_mode = RT_HALF;
			g_rt_mode_set = 1;
		} else if (strcmp(argv[i], "-colors") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if      (stricmp(v, "true") == 0 || stricmp(v, "truecolor") == 0 || strcmp(v, "24") == 0)
				g_colors = RT_24BIT;
			else if (strcmp(v, "256") == 0)
				g_colors = RT_8BIT;
			else if (strcmp(v, "8")   == 0)
				g_colors = RT_3BIT;
			else
				g_colors = RT_4BIT;                               /* "16" */
		} else if (strncmp(argv[i], "-t", 2) == 0) {
			const char *v = argv[i] + 2;
			if (*v == '\0' && i + 1 < argc)
				v = argv[++i];                               // "-t 1800"
			if (*v)
				g_time_limit_ms = (uint32_t)strtoul(v, NULL, 10) * 1000u;
		} else if (strcmp(argv[i], "-kpturn") == 0) {       // turn-key fresh grace (tight taps)
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v)
				g_turn_grace = (uint32_t)strtoul(v, NULL, 10);
		} else if (strcmp(argv[i], "-kpdelay") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v)
				g_grace_fresh = (uint32_t)strtoul(v, NULL, 10);
		} else if (strcmp(argv[i], "-kpsmooth") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v)
				g_keyup_idle_ms = (uint32_t)strtoul(v, NULL, 10);
		} else if (strcmp(argv[i], "-home") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v) { strncpy(g_home, v, sizeof(g_home) - 1); g_home[sizeof(g_home) - 1] = '\0'; }
		} else if (strcmp(argv[i], "-name") == 0) {       // multiplayer player handle
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v) { strncpy(g_player_name, v, sizeof(g_player_name) - 1); g_player_name[sizeof(g_player_name) - 1] = '\0'; }
		}
	}
	// Hand the BBS handle to the net layer. It must be non-NULL: the net layer
	// sends it at connect time (NET_CL_SendSYN) and would strlen(NULL)-crash
	// otherwise, and its own default doesn't run before then. The lobby always
	// passes -name; default for direct/manual launches.
	if (g_player_name[0] == '\0')
		strcpy(g_player_name, "Player");
	net_player_name = g_player_name;
	if (s_lines < 1)
		s_lines = 25;
	compute_geometry();              // s_pxW/s_pxH from viewport + scale config

	// child argv: prog -scaling 2 [pass-through args except our -l]
	child = calloc((size_t)argc + 3, sizeof(char *));
	child[cn++] = argv[0];
	child[cn++] = "-scaling";
	child[cn++] = "2";
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-iwad") == 0 || strcmp(argv[i], "-file") == 0 ||
		    strcmp(argv[i], "-merge") == 0) {
			child[cn++] = argv[i];                       // resolve WAD paths to absolute
			while (i + 1 < argc && argv[i + 1][0] != '-')  // before we chdir into the sandbox
				child[cn++] = abscopy(argv[++i]);
			continue;
		}
		if (strncmp(argv[i], "-door32", 7) == 0) {
			if (argv[i][7] == '\0')
				i++;
			continue;
		}
		if (argv[i][0] != '-' && is_door32_path(argv[i]))
			continue;                          // drop-file path, not for doomgeneric
		if (strcmp(argv[i], "-text") == 0)
			continue;                                      // boolean: no value to skip
		if (strcmp(argv[i], "-jxl")      == 0 || strcmp(argv[i], "-sixel")   == 0 ||
		    strcmp(argv[i], "-charset")  == 0 || strcmp(argv[i], "-mode")    == 0 ||
		    strcmp(argv[i], "-colors")   == 0 || strcmp(argv[i], "-home")    == 0 ||
		    strcmp(argv[i], "-kpturn")   == 0 || strcmp(argv[i], "-kpdelay") == 0 ||
		    strcmp(argv[i], "-name")     == 0 ||
		    strcmp(argv[i], "-kpsmooth") == 0) {
			i++;                                           // exact flags: skip the flag + its value
			continue;
		}
		if (strncmp(argv[i], "-term", 5) == 0) {           // prefix (matches pre-scan); may be glued
			if (argv[i][5] == '\0')
				i++;
			continue;
		}
		if (strncmp(argv[i], "-l", 2) == 0 || strncmp(argv[i], "-s", 2) == 0 ||
		    strncmp(argv[i], "-t", 2) == 0) {
			if (argv[i][2] == '\0')
				i++;
			continue;
		}
		child[cn++] = argv[i];
	}

	setup_sandbox();                 // chdir into the per-user dir; WAD paths above are absolute
	doomgeneric_Create(cn, child);
	for (;;) doomgeneric_Tick();
	return 0;
}
