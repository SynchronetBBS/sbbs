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
#include "text.h"                 // termgfx: text/block render tiers (rt_*)
#include "sixel.h"
#include "apc.h"                 // termgfx: SyncTERM APC cached-image transport (JXL/PPM)
#include "jxl.h"                 // termgfx: JPEG XL frame encoder (RGB888 -> JXL)
#include "caps.h"                // termgfx: cap-probe query + reply parsing (JXL)
#include "geometry.h"            // termgfx: shared image fit/center (shared with SyncDuke)
#include "pace.h"                // termgfx: shared AIMD pipeline-depth controller
#include "audio_mgr.h"           // termgfx: SyncTERM audio-APC manager (digital SFX)
#include "m_argv.h"             // myargc/myargv (set directly for the dedicated path)
#include "mp_server.h"          // mp_dedicated_main() headless server
#include "git_hash.h"           // generated: GIT_HASH / GIT_DATE / GIT_TIME (build info)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef _WIN32
#include <unistd.h>
#endif
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

#include "i_system.h"           // I_Error, I_Quit
#include "i_timer.h"            // I_Sleep
#include "m_config.h"           // M_SaveDefaults (persist Doom options on exit)
#include "net_defs.h"           // net_waitdata_t (the waiting room)
#include "net_client.h"         // net_client_wait_data, NET_CL_*
#include "net_server.h"         // NET_SV_Run
#include "unicode.h"            // xpdev: cp437_unicode_tbl (CP437 -> Unicode for UTF-8 terminals)
#include "sd_splash.h"          // sd_splash[] -- the bespoke waiting-room splash (80x25 char+attr)
#include "sbbs_node.h"          // door-native who's-online + page (Ctrl-U / Ctrl-P)

#ifndef _WIN32                  // dev stdio/tty fallback (no socket handle) is *nix-only
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#else                           // sockwrap.h has already pulled in <winsock2.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>           // timeGetTime (monotonic ms clock)
#include <direct.h>             // _chdir / _getcwd
#endif


#ifndef PATH_MAX                // MSVC's <limits.h> doesn't define it
#define PATH_MAX 1024
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
static char     g_wadname[64] = "";     // -wadname: friendly WAD-set name for the who's-online
                                        // status (defaults to the -iwad basename, e.g. "freedoom1")
extern char *   net_player_name;       // net_client.c: defaulted unless we set it from -name
static char     g_door32_path[PATH_MAX] = ""; // drop-file path (for the terminal.ini node-dir fallback)
static int      g_cols = 80;           // text-tier columns (from terminal.ini, else 80)
static int      g_text_cols = 80;      // effective text-render width (g_cols capped by text_max_cols)

// Graphics scaling (syncdoom.ini [video]). The bitmap tiers have no inherent
// size -- we emit whatever pixel dimensions we choose. "fit" sizes the image to
// the terminal's cell-pixel viewport (cols*8 x rows*cellH) at Doom's 8:5 aspect,
// so a wider terminal gets a bigger image; g_scale_max caps the emitted width as
// a bandwidth guard. Detail still tops out at Doom's 320x200 render -> larger is
// upscaled, not sharper. "off" emits the plain 640x400.
static int g_scale_fit = 1;            // 1 = fit terminal viewport, 0 = native 640x400
static int g_scale_max = 1280;         // max emitted image width (px); 0 = uncapped
// Sixel client-side scaling: encode at 1/SIXEL_SCALE of the display size and emit a
// "pan;pad raster aspect so the terminal scales it back up (SyncTERM integer-doubles;
// lossless since Doom is natively 320x200). ~1/4 the sixel bytes on SyncTERM.
#define SIXEL_SCALE 2
static int g_text_max_cols = 200;      // text tier: cap render columns (0 = uncapped) -- a
static int g_text_max_rows = 80;       // maximized terminal otherwise floods the link

// Real terminal PIXEL size, probed at startup. xterm/Windows Terminal answer
// ESC[14t (window px) + ESC[16t (cell px); SyncTERM answers ESC[?2;1S (graphics
// canvas px, XTSMGRAPHICS). Cell-based math (cols*8) is wrong on terminals whose
// cells aren't 8x16, so for "fit" we want the true window pixels. 0 = not reported
// (fall back to the cell model). The image is centered: g_img_{col,row} for the
// sixel text cursor, g_img_{x,y} (pixels) for the SyncTERM APC DX/DY.
static int g_win_w = 0, g_win_h = 0;   // terminal text-area / graphics-canvas pixels
static int g_cell_w = 0, g_cell_h = 0; // terminal cell pixels
static int g_meas_rows = 0, g_meas_cols = 0; // live size measured via cursor-extreme + DSR (0 = unmeasured)
static int g_cfg_cell_w = 0, g_cfg_cell_h = 0; // syncdoom.ini [video] cell_width/height
// override (for terminals that hide their pixels)
static int g_img_col = 1, g_img_row = 1; // 1-based cell to place the centered image (sixel cursor)
static int g_img_x = 0, g_img_y = 0;   // pixel offset of the centered image (APC DX/DY)

static void compute_geometry(void);    // defined later; called from DG_Init after the probe

// Startup diagnostics: what terminal.ini we resolved and what we read from it.
static char g_diag_term[PATH_MAX] = "(no node dir / -term)"; // path we tried
static int  g_diag_term_ok = 0;        // did we actually open+read it?
static char g_diag_desc[64] = "";      // desc= (TERM string) read from it

static uint32_t now_ms(void);          // defined later (monotonic ms; only deltas used)

// Emit one diagnostic line on stderr. Synchronet's external() reads a door's
// stderr line-by-line and logs each at LOG_NOTICE ("<fname>: <line>", xtrn.cpp),
// even in EX_BIN mode (where stderr is logged but not echoed to the user), so
// these land in the BBS log (syslog/journalctl/console) without touching the
// display. One newline-terminated line per call = one log entry.
void dlog(const char *fmt, ...)   // non-static: i_termmusic.c logs the cache path too
{
	static uint32_t start;             // baseline: first dlog call
	uint32_t        now = now_ms();
	va_list         ap;

	if (!start)
		start = now ? now : 1;
	// [+ms] since the first line: the BBS log stamps each line only to the second, so this
	// gives sub-second timing (e.g. to bracket a music render=/xfer= or a frame stall).
	fprintf(stderr, "[+%6ums] ", (unsigned)(now - start));
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
}

uint32_t sd_now_ms(void) { return now_ms(); }   // public monotonic-ms for i_termmusic.c timing

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
// base64 + the SyncTERM APC cached-image transport now live in termgfx/apc.c
// (termgfx_apc_image); emit_cached_image() below is a thin wrapper around it.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Frame encoder: PPM always, JXL when SyncTERM supports it (auto-probed). Both
// go through the same C;S Store + Draw{PPM,JXL} cache path, and the payload is
// base64 either way -> telnet-safe.
// ---------------------------------------------------------------------------

enum frame_mode { MODE_PPM = 0, MODE_JXL = 1, MODE_TEXT = 2, MODE_SIXEL = 3 };
static enum frame_mode g_mode = MODE_PPM;
static int             g_sixel_fullres = 0; // full-res sixel (vsc=1, no terminal scaling) for pan-
                                            // ignoring terminals (WezTerm); per-user persisted
static int             g_force_text = 0; // -text: force the block-char tier
static rt_charset_t    g_rt_charset = RT_CP437; // -charset (terminal.ini will set this)
static rt_mode_t       g_rt_mode    = RT_HALF; // -mode override
static int             g_rt_mode_set = 0;      // was -mode given?
static rt_colors_t     g_colors     = RT_4BIT; // color depth (terminal.ini desc / -colors)

// Always-run: a terminal can't reliably hold a run modifier, so the 'R' key
// toggles Doom's permanent always-run instead (handled in G_Responder). The
// startup default comes from syncdoom.ini [input] always_run (default on) and is
// applied in DG_Init; joybspeed is the Doom global (m_controls.c).
extern int  joybspeed;
static bool g_always_run = true;

// Live render-tier cycle (the in-game F4 hotkey steps through graphics <-> the
// three text glyph tiers, so a user can find which their terminal actually
// renders -- e.g. SyncTERM is CP437-only and shows boxes for the UTF-8 quadrant
// /sextant tiers). The state list is built lazily at first cycle from whatever
// tier startup selected.
struct vstate { enum frame_mode mode; rt_mode_t rt; rt_charset_t cs; const char *label; int fullres; };
static struct vstate g_vstates[8];
static int           g_vstate_n = 0;
static int           g_vstate_i = 0;
static uint32_t      g_label_until = 0;  // suppress frame output until then (label dwell)
static int           g_clear_row1  = 0;  // erase the top row next frame (a dismissed label/overlay
                                         // strands there in a letterboxed window -- frame won't repaint it)

static void cycle_video(void);
void        sd_save_user_prefs(void); // persist per-user prefs (cycle_video uses it before its defn)
static void toggle_pipeline(void); // Ctrl-T: cycle frame-pipeline depth + persist
static void toggle_stats(void);    // Ctrl-S: toggle the live stats overlay (session-only)
static void toggle_mouse(void);    // Ctrl-O: toggle mouse steering on/off + persist
static int g_stats_overlay = 0;       // show fps/RTT/depth overlaid on each frame
// Kitty keyboard protocol negotiated (terminal answered our CSI?u query): keys arrive as CSI-u
// events with real press/repeat/release, so the door gets true key-up -- crisp hold-to-move and
// Doom's native turn ramp, no key-up-synthesis grace needed. Non-static: m_menu.c greys the (now
// moot) TAP/HOLD/TURN/FAST-TURN sliders when this is set. (Declared up here: emit_overlay reads it.)
int                  g_kitty_active = 0;
static unsigned char g_key_down[256]; // Doom keys we've sent key-down for (dedupe + release); used by
                                      // any path with a real key-up (kitty protocol, SyncTERM evdev)

// SyncTERM (CTerm) "physical key event reporting": the CTDA probe advertised cap 8, so we enabled
// it (CSI=1h reports + CSI=2h suppress-translation) and keys arrive as CSI = <evdev-code>[;...] K
// (press) / k (release) -- LAYOUT-INDEPENDENT physical scancodes with a true key-up, like kitty
// but for SyncTERM (which doesn't speak kitty; kitty terminals don't advertise CTDA cap 8, so the
// two paths are mutually exclusive). Decoded edges reuse map_ascii()+key_dispatch() verbatim.
int             g_evdev_active = 0;     // non-static: m_menu.c greys the key-feel sliders when set
static unsigned g_evdev_mods = 0;        // held modifiers: bit0 Shift, bit1 Ctrl, bit2 Alt
#define EVDEV_MOD_SHIFT 1
#define EVDEV_MOD_CTRL  2
#define EVDEV_MOD_ALT   4
// Enable-time settle: SyncTERM resyncs the keys held at enable (the key still down from selecting
// the door) as PRESS reports; drop non-modifier presses for this long so they can't skip the
// opening. The resync lands within ~1 RTT and evdev sends no auto-repeat, so one drop suffices.
#define SD_EVDEV_SETTLE_MS 500
static uint32_t  g_evdev_enabled_ms = 0;
static int       g_evdev_settling = 0;
static const char *mode_name(enum frame_mode m);   // defined later; used by the overlay

static uint8_t * s_rgb = NULL;   static size_t s_rgb_cap = 0;      // packed RGB888
static uint8_t * s_img = NULL;   static size_t s_img_cap = 0;      // encoded bytes (PPM/JXL)
static uint8_t * s_apc = NULL;   static size_t s_apc_cap = 0;      // built APC Store+Draw sequence

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

// End-to-end frame pacing: after each frame we emit a DSR (ESC[6n); the terminal's
// report (ESC[r;cR) comes back only once it has consumed that frame, so we cap the
// number of frames "in flight" (sent but not yet reported). Depth 1 paces strictly
// to the terminal's render rate -- ideal for a LOCAL SyncTERM whose JXL decode is
// the bottleneck. But for a REMOTE user the DSR round-trip IS the link RTT, so
// depth 1 caps the frame rate at ~1/RTT (a slideshow). A pipeline (depth 2-5)
// lets several frames traverse the link at once, lifting the remote frame rate
// toward Doom's 35fps sim rate; socket backpressure (out_pending) still guards
// bandwidth. g_max_inflight is the depth ([video] frames_in_flight); "auto" adapts
// it to the measured RTT.
//
// DSR sends are timestamped in a small ring (reports return in send order, so FIFO
// match), which doubles as the in-flight count and the round-trip estimator. The
// deadline reclaims the pipeline if reports ever go silent.
#define DEPTH_MAX 8                     // max pipeline depth (fixed or auto ceiling)
#define FRAME_STALL_MS 500              // emit_frame over this -> log a frame-stall note (slow encode;
                                        // out_flush is non-blocking, so this is build/encode time)
#define DSR_RING  16                    // > DEPTH_MAX so the in-flight ring can never fill (holds DSR_RING-1)
static uint32_t g_dsr_ts[DSR_RING];     // send time of each unacked DSR
static int      g_dsr_head = 0;         // next slot to write (newest)
static int      g_dsr_tail = 0;         // oldest unacked (matches the next report)
static uint32_t g_ack_deadline = 0;
static int      g_dsr_stale = 0;        // reports owed by reclaimed frames -> skip them (don't measure RTT)
static int      g_rt_high    = 0;        // latched once the frame round-trip is non-trivial (floor depth >= 2)
static int      g_max_inflight = 1;     // fixed pipeline depth when not auto (1 = strict pacing)
static int      g_inflight_auto = 1;    // [video] frames_in_flight = auto: adapt depth to the link (DEFAULT)
static uint32_t g_rtt_ms = 0;           // EMA of the DSR round-trip (ms); 0 = not yet measured
static uint32_t g_rtt_min = 0;          // baseline (unloaded) RTT: windowed min of recent samples
static uint32_t g_rtt_min_at = 0;       // when g_rtt_min was last set (for the window reset)

// RTT-adaptive turn (mirrors SyncDuke): the native hold-to-turn (true key-up + Doom's turn) is
// non-deterministic/overshoots on a laggy link -- the release edge is RTT-delayed and applied in
// tic bursts.  Past the threshold we inject a CONSTANT synthetic turn (sd_synth_turn ->
// cmd->angleturn in G_BuildTiccmd) instead, which is deterministic.  +1 = right, -1 = left, 0 =
// none.  Non-static: g_game.c reads it.
static uint32_t now_ms(void);           // defined later (monotonic ms; only deltas used)
int sd_synth_turn = 0;
// Hysteresis + dwell so the model doesn't flap as the RTT EMA jitters: switch to synthetic only
// when RTT clearly exceeds *_SYNTH, back to native only below *_NATIVE, and never more than once
// per *_DWELL_MS.  rtt==0 (unmeasured) holds the current latch.
#define SD_TURN_SYNTH_RTT   150u
#define SD_TURN_NATIVE_RTT  100u
#define SD_TURN_DWELL_MS    1200u
static int sd_turn_native(void)
{
	static int      native = 1;     // latch: 1 = native hold, 0 = synthetic
	static uint32_t last_switch = 0;
	uint32_t        now = now_ms();
	uint32_t        rtt = g_rtt_ms;

	if (rtt != 0 && (uint32_t)(now - last_switch) >= SD_TURN_DWELL_MS) {
		if (native && rtt > SD_TURN_SYNTH_RTT) {
			native = 0;
			last_switch = now;
		} else if (!native && rtt < SD_TURN_NATIVE_RTT) {
			native = 1;
			last_switch = now;
		}
	}
	return (g_kitty_active || g_evdev_active) && native;
}
#define RTT_MIN_WINDOW 8000u            // re-seed the baseline min if no lower sample in this long
static uint32_t g_recent_fps = 0;       // frames/s over a recent window (the bandwidth signal for auto)
static uint32_t g_recent_kbps = 0;      // transmit throughput (KB/s) over the same window (overlay)
static uint32_t g_fps_win_at = 0;       // current fps-window start time
static uint32_t g_fps_win_n = 0;        // frames emitted in the current fps window
static uint64_t g_fps_win_bytes = 0;    // wire bytes emitted in the current fps window

// Frame de-duplication: Doom re-renders the SAME game state several times between
// its 35Hz sim tics (TryRunTics returns early to keep menus responsive), so back-
// to-back frames are often byte-identical. We keep a copy of the framebuffer last
// sent and skip emitting an identical one -- capping the WIRE frame rate at the
// real ~35fps visual rate and saving the redundant bytes (most valuable on a thin
// remote link). g_last_fb is invalidated (NULL) whenever a label or geometry
// change disturbs the terminal so the next frame always repaints.
#define FB_BYTES ((size_t)DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(uint32_t))
static uint32_t *g_last_fb   = NULL;     // copy of the framebuffer last emitted (allocated lazily)
static int       g_last_fb_ok = 0;       // is g_last_fb a valid match target? (cleared on label/resize)
static uint32_t  g_dedup_skipped = 0;    // telemetry: identical frames skipped (not re-sent)
static char      g_ov_last[80] = "";     // last stats-overlay text drawn (to skip redundant redraws)
static int       g_ov_prev_tn = 0;       // width of the last overlay -> blank the gap when it shrinks
static int       g_ov_cell_col = 0;      // overlay's cell-grid column (0-based) -> text cell-diff exclusion
static int       g_ov_cell_w   = 0;      // overlay's width in cells (0 = not drawn yet)
static char      g_page_ov[10][82];      // shared top banner: wrapped lines (full text) --
                                         // used by both the incoming-page notice (caps at 5
                                         // rows) and the Ctrl-U who's-online list
static int       g_page_ov_rows = 0;     // active banner line count (0 = not shown)
static uint32_t  g_page_ov_until = 0;    // banner expiry (now_ms); auto-clears after

// Doom HU strings (hu_stuff.c) for the text-tier legible HUD. sd_text_hud suppresses
// Doom's framebuffer draw of the message line + chat so only our terminal-character
// version shows (real chars = legible; the rasterized font downsamples to blocks).
extern int sd_text_hud;
// d_net.c: remove a departed player's marine (vanilla leaves it standing). Set
// from [game] remove_quit_bodies; default on.
// d_net.c: fate of a departed player's marine -- 0 keep / 1 vanish / 2 teleport-fog.
extern int sd_quit_effect;
// g_game.c: defeat Doom's turn-accel ramp (Options > FAST TURN). Per-user.
extern int sd_instant_turn;
extern const char *HU_message_text(void);
extern void sd_post_message(const char *msg);   // hu_stuff.c: post a one-line HUD message
extern const char *HU_chat_text(void);
#define SD_CHAT_ROW 1                    // cell row for the chat line (one below the message)

static int dsr_inflight(void) { return (g_dsr_head - g_dsr_tail + DSR_RING) % DSR_RING; }

static uint32_t now_ms(void);           // defined later (monotonic ms; only deltas used)
static int      g_auto_depth  = 2;      // current auto pipeline depth (AIMD-adjusted)
static uint32_t g_auto_adj_at = 0;      // last gentle (probe/ease) adjustment time

// Re-evaluate the auto pipeline depth from the latest RTT. Called once per DSR
// round-trip (i.e. per DELIVERED frame; an idle/de-duped lull sends no DSR, so the
// depth simply holds). It's a delay-based AIMD controller with a dead-band and a
// rate limit so it SETTLES at the link's sustainable depth instead of oscillating
// (the bang-bang 4/5<->1 a fixed base-minus-penalty produced on a jittery VPN):
//   - heavy queuing (current EMA > 2x baseline): ease down at once to drain;
//   - clean (< 1.25x baseline): probe up one, but at most ~2.5x/sec so each step's
//     effect is observed before the next (no overshoot);
//   - dead-band (1.25x..1.5x): hold.
// Bounded to [floor, ceil]: floor 2 once the frame round-trip is non-trivial (the
// g_rt_high latch -- network latency OR client decode time) so it never face-plants
// to the depth-1 slideshow; ceil ~one frame per 40ms of baseline round-trip (abs 5)
// so a fast-rendering local link isn't pointlessly over-pipelined (extra depth there
// only buffers, adding display lag, without adding frames past the 35fps sim). The
// abs cap is DEPTH_MAX (8 -> reaches the 35fps sim at ~230ms round-trip).
static void auto_depth_update(void)   // AIMD shared with SyncDuke (termgfx/pace.c)
{
	termgfx_aimd_update(g_inflight_auto, &g_auto_depth, &g_auto_adj_at,
	                    g_rtt_ms, g_rtt_min, g_rt_high, DEPTH_MAX, now_ms());
}

// Effective pipeline depth: the fixed [video] frames_in_flight, or the AIMD-tracked
// auto depth (updated in the DSR handler; a pure getter here so it's side-effect-free
// to read from the gate, the overlay and the telemetry).
static int max_inflight(void)
{
	return g_inflight_auto ? g_auto_depth : g_max_inflight;
}

// Set the pipeline depth from a config/toggle string: "auto" or a number 1..N.
static void set_frames_in_flight(const char *val)
{
	if (stricmp(val, "auto") == 0) {
		g_inflight_auto = 1;
	} else {
		int fif = atoi(val);
		if (fif >= 1 && fif <= DEPTH_MAX) {
			g_max_inflight  = fif;
			g_inflight_auto = 0;
		}
	}
}

// The current depth setting as a string ("auto" or "N"), for saving/logging.
static const char *frames_in_flight_str(void)
{
	static char buf[8];
	if (g_inflight_auto)
		return "auto";
	snprintf(buf, sizeof(buf), "%d", g_max_inflight);
	return buf;
}

// Dead-client detection for the frame path. A client that vanishes without a
// clean FIN/RST never makes recv() return EOF, and its undrained frame just
// backs up in the kernel send buffer under TCP retransmission (~15 min default)
// -- so out_flush can neither push bytes nor see a send error, and the game loop
// spins forever (the "orphan door" bug). We stamp the last time a send made
// progress; if a frame can't drain at all for OUT_STALL_MS, the peer is gone. A
// slow but live client still drains some bytes within the window (resets it).
#define OUT_STALL_MS 30000u
static uint32_t g_tx_progress_ms = 0;   // last send progress (seeded in DG_Init)

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
			g_tx_progress_ms = now_ms();                   // bytes accepted -> client is alive
		} else {
#ifndef _WIN32
			ssize_t n = write(g_wfd, g_out + g_out_off, g_out_len - g_out_off);
			if (n > 0) { g_out_off += (size_t)n; g_tx_progress_ms = now_ms(); continue; }
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
	else if ((int32_t)(now_ms() - g_tx_progress_ms) > (int32_t)OUT_STALL_MS)
		g_hangup = 1;          // frame can't drain at all -> client silently vanished
}

// Terminal audio: the SyncTERM audio-APC manager (termgfx). i_termsound.c's
// sound_module externs sd_audio and feeds SFX into it; we stage its bytes
// through out_put (flushed with the frame) and feed it inbound replies from
// pump_input so the capability probe resolves. NULL/tier<1 => silently no sound.
termgfx_audio_t *sd_audio = NULL;

static void sd_audio_emit(void *ctx, const void *buf, size_t len, int stream)
{
	(void)ctx;
	(void)stream;   /* SyncDOOM has SFX only (no music upload yet) -> single stream */
	out_put(buf, len);
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

// base64 `bytes` and push: Store(<file>) then <drawverb>(<file>) -- via termgfx/apc.c.
static void emit_cached_image(const char *file, const char *drawverb,
                              const uint8_t *bytes, size_t n)
{
	size_t len = termgfx_apc_image(&s_apc, &s_apc_cap, file, drawverb,
	                               bytes, n, g_img_x, g_img_y);   // DX/DY center in the canvas
	out_put(s_apc, len);
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
//
// We encode from Doom's native 8-bit palette indices (scaled to the emit size)
// against a stable 1:1 register<->index palette, and (re)define the 256 sixel
// registers ONLY when Doom changes the palette -- so a steady scene sends band
// data alone (no ~4KB palette block per frame), which both shrinks the per-frame
// sixel string and keeps the palette identical frame to frame.
extern int      sd_palette_seq;                  // i_video.c: bumps on palette change
extern void sd_palette_rgb(unsigned char *out);  // i_video.c: live palette -> out[768]
extern void sd_scale_indices(unsigned char *out, int w, int h);  // native indices -> w*h

static uint8_t *s_sx_idx = NULL; static size_t s_sx_idx_cap = 0; // scaled index buffer
static int      g_sx_pal_seq = -1;     // palette seq last written to the sixel stream (-1 = none)
static int      g_is_syncterm = 0;     // SyncTERM detected (Q;JXL reply, or CTerm version >= 1.2);
                                       // only SyncTERM persists sixel registers -> define-once there

static void emit_frame_sixel(int w, int h)
{
	char                 pos[24];
	int                  l = snprintf(pos, sizeof(pos), "\x1b" "7\x1b[%d;%dH", g_img_row, g_img_col);
	static unsigned char pal[768];
	// SyncTERM persists sixel color registers across images, so we define them only
	// on a real palette change (redefining every frame is what garbled its decoder).
	// Other terminals (Windows Terminal, xterm) do NOT reliably persist registers
	// between images -- omitting the palette there leaves later frames referencing
	// undefined registers (wrong/default colors). So only SyncTERM gets define-once;
	// elsewhere re-send the palette every frame (it's now the same stable 1:1 set).
	int emit_pal = !g_is_syncterm || (g_sx_pal_seq != sd_palette_seq);
	// Client-side scaling (like SyncDuke): encode at 1/scale of the display size and
	// let the terminal scale it back up via the "pan;pad raster aspect.  SyncTERM does
	// the integer nearest-neighbor double (pad=2) -> ~1/4 the sixel bytes, and it's
	// LOSSLESS here because Doom's indexed buffer is natively 320x200, so a SyncTERM
	// frame encodes at native res (not a downscale); other sixel terminals get pad=1
	// (full width, half the bands, the vertical via the 2:1 pixel aspect).  Clamp the
	// encoded height to whole 6-row bands -- a partial final band garbles under pan>1.
	int    vsc = (g_is_syncterm || !g_sixel_fullres) ? SIXEL_SCALE : 1; // full-res (1:1) only when opted in
	int    hsc = g_is_syncterm ? SIXEL_SCALE : 1;  // horizontal: halve only on SyncTERM
	int    sxw = w / hsc;
	int    sxh = h / vsc;
	size_t n;

	sxh -= sxh % 6;
	ensure(&s_sx_idx, &s_sx_idx_cap, (size_t)sxw * sxh);
	sd_scale_indices(s_sx_idx, sxw, sxh);
	if (emit_pal) {                          // palette changed (or first frame) -> (re)define
		sd_palette_rgb(pal);
		g_sx_pal_seq = sd_palette_seq;
	}
	n = sixel_encode_aspect(&s_img, &s_img_cap, s_sx_idx, sxw, sxh, vsc, hsc, pal, emit_pal);
	out_put(pos, l);               // save cursor + position (centered) -> overdraw in place
	out_put(s_img, n);
	out_put("\x1b" "8", 2);        // restore cursor (terminals differ post-sixel)
}

// DECSDM (DEC private mode 80, "sixel scrolling") defaults to SET, which scrolls
// the page up and appends a newline whenever a sixel reaches the bottom -- so a
// full-screen sixel every frame visibly scrolls/stutters (the old SyncTERM sixel
// problem). Reset it while the sixel tier is active: the sixel origin pins to the
// page's top-left, frames overdraw in place, and nothing scrolls. Restore the
// default when leaving sixel or on exit. A no-op on terminals without mode 80.
// (Under reset-80 cterm ignores the cursor and draws at 0,0, so the sixel image
// anchors top-left rather than at emit_frame_sixel's centered cell.)
static int g_sixel_scroll_off = 0;
static void apply_sixel_scroll(void)
{
	int want = (g_mode == MODE_SIXEL);
	if (want == g_sixel_scroll_off)
		return;
	emit_all(want ? "\x1b[?80l" : "\x1b[?80h", 6);
	g_sixel_scroll_off = want;
	if (want)
		g_sx_pal_seq = -1;   // (re)entering sixel: the cleared screen drops the color
	                         // registers, so force a palette re-definition next frame
}

#ifdef WITH_JXL
// JXL frame compression. distance = lossy Butteraugli target (1.0 ~ visually
// lossless; higher = smaller + softer; NO encode-time cost) -- the size lever,
// tunable via [video] jxl_distance or -jxldistance (measured -21% at 3.0, -36%
// at 4.0 on Doom frames). effort is fixed at 1 (lightning, lowest latency): on
// this noisy game content raising it barely shrinks frames and effort >=5 makes
// them bigger AND drops fps, so there's nothing to gain by exposing it.
static float g_jxl_distance = 2.0f;
static int   g_jxl_effort   = 1;

// Encode s_rgb (w*h RGB888) as JXL via termgfx; emit as a DrawJXL. Returns false on any
// failure (caller falls back to PPM).
static bool emit_frame_jxl(int w, int h)
{
	size_t n = termgfx_jxl_encode(&s_img, &s_img_cap, s_rgb, w, h, g_jxl_distance, g_jxl_effort);
	if (n == 0)
		return false;
	emit_cached_image("d.jxl", "DrawJXL", s_img, n);
	return true;
}
#endif // WITH_JXL

// Roll the ~2s frame-rate / throughput window. Called once per emit_frame with
// this iteration's wire bytes (0 when the frame was de-duped) so a static scene
// decays toward 0 fps / 0 KB/s instead of freezing the readout at the last value.
static void fps_window_tick(uint32_t frame_bytes)
{
	uint32_t nm = now_ms();

	if (frame_bytes)
		g_fps_win_n++;
	g_fps_win_bytes += frame_bytes;
	if (g_fps_win_at == 0)
		g_fps_win_at = nm;
	else if (nm - g_fps_win_at >= 2000) {
		uint32_t span = nm - g_fps_win_at;
		g_recent_fps  = g_fps_win_n * 1000 / span;
		g_recent_kbps = (uint32_t)(g_fps_win_bytes * 1000 / 1024 / span);
		g_fps_win_n     = 0;
		g_fps_win_bytes = 0;
		g_fps_win_at    = nm;
	}
}

// Width the on-screen labels/overlay center or right-justify within: the RENDERED
// width. In text mode the grid is capped (text_max_cols) narrower than a wide
// terminal, so a label centered on the full g_cols would land off to the right of
// the game; use the capped width. Graphics tiers fill to the full terminal width.
static int overlay_cols(void)
{
	return (g_mode == MODE_TEXT) ? g_text_cols : g_cols;
}

// Live stats overlay (Ctrl-S): a status strip in the top-RIGHT corner. Top-right
// keeps it clear of Doom's notices (top-left message line) and the HUD/status bar
// (bottom). Shows the render tier, recent frame rate, transmit throughput (KB/s to
// the player), round-trip (current / baseline-min) and the effective pipeline depth
// -- the signals that drive the auto pacing, so a far-away player can watch them.
// force=1 always redraws (a fresh frame just painted over it); force=0 redraws only
// when the text changed, so refreshing it over a de-duped (unchanging) frame is free.
static void emit_overlay(int force)
{
	char txt[80], ov[160], bw[16];
	int  tn, col, ovn, aw;

	// Throughput: KB/s up to 999, then fractional MB/s (KiB/MiB of wire BYTES) so the
	// field stays narrow on a fast link.
	if (g_recent_kbps > 999)
		snprintf(bw, sizeof(bw), "%u.%uMB/s", g_recent_kbps / 1024, (g_recent_kbps % 1024) * 10 / 1024);
	else
		snprintf(bw, sizeof(bw), "%uKB/s", g_recent_kbps);
	{
		// Keyboard + turn-key model in ONE compact token: "evdev/nat" or "kitty/syn" -- the suffix
		// is native hold (low-latency true key-up) vs the synthetic constant rate (high-latency).
		char kbd[16] = "";
		if (g_evdev_active || g_kitty_active)
			snprintf(kbd, sizeof(kbd), " %s/%s",
			         g_evdev_active ? "evdev" : "kitty",
			         sd_turn_native() ? "nat" : "syn");
		tn = snprintf(txt, sizeof(txt), " %s %ufps %s lag %u/%ums depth %d%s%s ",
		              (g_mode == MODE_SIXEL && g_sixel_fullres) ? "sixel-full" : mode_name(g_mode),
		              g_recent_fps, bw, g_rtt_ms, g_rtt_min,
		              max_inflight(), g_inflight_auto ? "/auto" : "",
		              kbd);
	}

	if (!force && strcmp(txt, g_ov_last) == 0)
		return;                         // unchanged and the frame didn't repaint it -> nothing to send
	if (tn > 0 && tn < (int)sizeof(g_ov_last))
		strcpy(g_ov_last, txt);
	// Anchor to the rendered width: in text mode the grid is capped (text_max_cols),
	// so right-justifying to the full g_cols would fling the overlay off to the right
	// of the actual game view on a wide terminal. Graphics tiers use g_cols (the
	// overlay sits in the centered image's top margin).
	aw  = overlay_cols();
	col = aw - tn + 1;                  // right-justify on the top row
	if (col < 1)
		col = 1;
	g_ov_cell_col = col - 1;            // overlay's cell region (0-based) -> excluded from the
	g_ov_cell_w   = tn;                 // text cell-diff so the game never repaints under it
	if (g_ov_prev_tn > tn) {            // narrower than last time: blank the now-uncovered cells
		int prev_col = aw - g_ov_prev_tn + 1;
		if (prev_col < 1)
			prev_col = 1;
		ovn = snprintf(ov, sizeof(ov), "\x1b[1;%dH\x1b[0m%*s\x1b[1;37;44m%s\x1b[0m",
		               prev_col, col - prev_col, "", txt);
	} else {
		ovn = snprintf(ov, sizeof(ov), "\x1b[1;%dH\x1b[1;37;44m%s\x1b[0m", col, txt);
	}
	g_ov_prev_tn = tn;
	if (ovn > 0)
		out_put(ov, (size_t)ovn);
}

static int page_overlay_active(void)
{
	return g_page_ov_rows > 0 && now_ms() < g_page_ov_until;
}

// Draw the incoming-page banner over the top rows (a white-on-red strip per
// wrapped line), or -- the frame after it expires -- wipe those rows and force a
// repaint of the game underneath. Called each frame, after the frame is emitted.
static void draw_page_overlay(void)
{
	static int drawn = 0;           // banner rows currently painted on screen (high-water)
	char       ob[1400];            // up to 10 rows * (escapes + ~80 chars)
	int        i, n = 0;

	if (g_page_ov_rows <= 0)
		return;
	if (now_ms() >= g_page_ov_until) {                  // expired -> clear + repaint under
		for (i = 0; i < drawn && n < (int)sizeof(ob) - 16; i++)
			n += snprintf(ob + n, sizeof(ob) - n, "\x1b[%d;1H\x1b[0m\x1b[2K", i + 1);
		if (n > 0)
			out_put(ob, (size_t)n);
		g_page_ov_rows = 0;
		drawn          = 0;
		g_last_fb_ok   = 0;                             // dedup: repaint the frame
		rt_invalidate();                                // text: resync the cleared rows
		return;
	}
	for (i = 0; i < g_page_ov_rows && n < (int)sizeof(ob) - 100; i++)
		n += snprintf(ob + n, sizeof(ob) - n, "\x1b[%d;1H\x1b[1;37;41m\x1b[K%s\x1b[0m",
		              i + 1, g_page_ov[i]);
	// A shorter banner replaced a taller one -> wipe the rows it vacated and force a
	// repaint of the game underneath them.
	for (i = g_page_ov_rows; i < drawn && n < (int)sizeof(ob) - 16; i++)
		n += snprintf(ob + n, sizeof(ob) - n, "\x1b[%d;1H\x1b[0m\x1b[2K", i + 1);
	if (g_page_ov_rows < drawn) {
		g_last_fb_ok = 0;
		rt_invalidate();
	}
	if (n > 0)
		out_put(ob, (size_t)n);
	drawn = g_page_ov_rows;
}

static void emit_frame(const uint32_t *fb, int w, int h)
{
	bool built = false;

	sd_text_hud = (g_mode == MODE_TEXT);   // text tier -> Doom skips drawing msg/chat; we do (legibly)
	int  identical;

	if (g_label_until != 0) {
		if ((int32_t)(now_ms() - g_label_until) < 0) {
			g_last_fb_ok = 0;           // a label is overlaying the frame -> repaint once it clears
			return;                     // holding the video-cycle label on screen
		}
		g_label_until = 0;              // dwell ended -> erase the label's row + force a full repaint
		g_clear_row1 = 1;
		g_last_fb_ok = 0;
	}
	if (out_pending())
		return;                         // our buffer still draining -> drop this one
	if (dsr_inflight() > 0 && (int32_t)(g_ack_deadline - now_ms()) <= 0) {
		g_dsr_stale += dsr_inflight();   // these frames' reports may still arrive late -> ignore them
		if (g_dsr_stale > DSR_RING)
			g_dsr_stale = DSR_RING;
		g_dsr_tail = g_dsr_head;          // reports went silent past the deadline -> reclaim the pipeline
	}
	if (dsr_inflight() >= max_inflight())
		return;                          // pipeline full: drop this frame (never block the game loop)

	// De-dupe: when this frame is byte-identical to the last one we sent (a redundant
	// re-render between sim tics, or a still scene), skip the costly frame body. The
	// stats overlay is independent -- it still refreshes (cheaply) so its readout
	// stays live, and the fps/KB/s window keeps ticking so a still scene reads ~0.
	identical = (g_last_fb_ok && memcmp(fb, g_last_fb, FB_BYTES) == 0);
	if (identical) {
		g_tx_progress_ms = now_ms();    // nothing to render -> the client isn't stalled
		g_dedup_skipped++;
		fps_window_tick(0);
		if (g_stats_overlay) {
			g_out_len = 0; g_out_off = 0;
			emit_overlay(0);            // redraw only if the readout text changed
			out_flush();
		}
		return;
	}

	g_out_len = 0; g_out_off = 0;       // begin a fresh frame buffer
	if (g_clear_row1) {                 // wipe a just-dismissed label/overlay off the top row
		out_put("\x1b[1;1H\x1b[2K", 10);
		g_ov_last[0] = '\0';            // overlay text is gone -> force a redraw if it's still on
		g_ov_prev_tn = 0;
		rt_invalidate();               // we cleared the row behind the text renderer -> resync it
		g_clear_row1 = 0;
	}
	if (g_mode == MODE_TEXT) {          // ANSI/CP437 block-char tier (render_text)
		size_t      tlen;
		const char *tb;
		// Legible text-HUD: render Doom's message line + chat (and the stats overlay)
		// as REAL terminal characters, each an excluded rectangle so the game cell-diff
		// never repaints under them (no flicker). Strings fetched once -> used for both
		// the exclusion (pre-render) and the draw (post-render).
		const char *msg   = HU_message_text();
		const char *chat  = HU_chat_text();
		int         ovc0  = (g_stats_overlay && g_ov_cell_w > 0) ? g_ov_cell_col : g_text_cols;
		int         msgw  = msg  ? (int)strlen(msg)  : 0;
		int         chatw = chat ? (int)strlen(chat) : 0;
		if (msgw > ovc0)
			msgw = ovc0;                                  // keep the left message clear of the right overlay
		if (chatw > g_text_cols)
			chatw = g_text_cols;

		rt_exclude_clear();
		if (page_overlay_active()) {                      // page banner owns the top rows
			int pr;
			for (pr = 0; pr < g_page_ov_rows; pr++)
				rt_exclude_add(pr, 0, g_text_cols);
		}
		if (g_stats_overlay && g_ov_cell_w > 0)
			rt_exclude_add(0, g_ov_cell_col, g_ov_cell_col + g_ov_cell_w);
		if (msgw > 0)
			rt_exclude_add(0, 0, msgw);                   // message: cell row 0, top-left
		if (chatw > 0)
			rt_exclude_add(SD_CHAT_ROW, 0, chatw);

		pack_rgb(fb, DOOMGENERIC_RESX, DOOMGENERIC_RESY);   // native-res RGB888 for the text scaler
		tb = rt_render_frame(s_rgb, DOOMGENERIC_RESX, DOOMGENERIC_RESY, &tlen);
		out_put(tb, tlen);

		if (msgw > 0) {                                   // draw it as characters, on top
			char hb[300];
			int  n = snprintf(hb, sizeof(hb), "\x1b[1;1H\x1b[1;37;40m%.*s\x1b[0m", msgw, msg);
			if (n > 0)
				out_put(hb, (size_t)n);
		}
		if (chatw > 0) {
			char hb[300];
			int  n = snprintf(hb, sizeof(hb), "\x1b[%d;1H\x1b[1;33;44m%.*s\x1b[0m",
			                  SD_CHAT_ROW + 1, chatw, chat);
			if (n > 0)
				out_put(hb, (size_t)n);
		}
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

	if (g_stats_overlay)
		emit_overlay(1);                // the fresh frame painted over row 1 -> always redraw
	draw_page_overlay();                // incoming-page banner over the top rows (or clear it)

	out_put("\x1b[6n", 4);              // DSR -> the terminal reports when it has consumed this frame
	g_dsr_ts[g_dsr_head] = now_ms();    // timestamp it (in-flight count + RTT estimate)
	g_dsr_head = (g_dsr_head + 1) % DSR_RING;
	g_ack_deadline = now_ms() + 250;    // safety: reclaim the pipeline if no report by then
	g_tx_bytes += g_out_len;            // telemetry: this frame's wire bytes (incl. the DSR)
	g_tx_frames++;
	fps_window_tick(g_out_len);         // bandwidth signal auto's BDP cap uses (delivered frames)

	// Remember this frame so the next identical re-render can be de-duped (cache
	// allocated lazily on first send).
	if (g_last_fb == NULL)
		g_last_fb = malloc(FB_BYTES);
	if (g_last_fb != NULL) {
		memcpy(g_last_fb, fb, FB_BYTES);
		g_last_fb_ok = 1;
	}

	out_flush();
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

static uint32_t now_ms(void)
{
#ifdef _WIN32
	return (uint32_t)timeGetTime();          // monotonic ms; only deltas are used
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
#endif
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
#define KEY_PIPELINE 0xe1 // door-internal sentinel (Ctrl-T): cycle frame-pipeline depth
#define KEY_STATS    0xe2 // door-internal sentinel (Ctrl-S): toggle the live stats overlay
#define KEY_MOUSE    0xe5 // door-internal sentinel (Ctrl-O): toggle mouse steering
#define KEY_USERLIST 0xe3 // door-internal sentinel (Ctrl-U): who's online
#define KEY_PAGE     0xe4 // door-internal sentinel (Ctrl-P): page a node
// Non-static: the in-game Options > Input sliders (m_menu.c) tune these live.
// Source order, lowest-to-highest precedence: built-in -> syncdoom.ini [input]
// (house default) -> per-user saved prefs (load_input_prefs) -> -kp* CLI flags.
uint32_t   g_keyup_idle_ms = 150;             // HOLD grace (key seen >= 2 times)
uint32_t   g_grace_fresh   = 300;             // TAP grace (key seen once)
uint32_t   g_turn_grace    = 75;              // TURN grace for turn keys (tight taps; low to
                                              // suit FAST TURN's full-speed turn, default on)
static int g_kp_cli_tap    = 0;               // -kpdelay given on CLI (saved prefs won't override)
static int g_kp_cli_hold   = 0;               // -kpsmooth given on CLI
static int g_kp_cli_turn   = 0;               // -kpturn given on CLI

// Defaults captured right after read_syncdoom_ini() (built-in + house ini, before
// per-user/CLI) so a per-user save records ONLY what the player changed in-game;
// untouched keys stay out of the file and keep tracking the sysop's house default
// -- even for players who have already played.
static uint32_t g_base_grace_fresh, g_base_keyup_idle_ms, g_base_turn_grace;
static int      g_base_instant_turn;
static char     g_base_frames[32];
static char     g_user_ini_path[PATH_MAX] = ""; // per-user prefs file (<-home>/syncdoom.ini); "" = disabled
static struct { unsigned char key; uint32_t last; int presses; } s_active[ACTIVE_MAX];
static int      s_active_n = 0;

// doom m_menu.c: nonzero while a menu is open (boolean == unsigned int). The input
// model switches on it. IN A MENU each key byte is a discrete press (down+up) so
// rapid same-key taps never merge -- a hold-with-timeout fundamentally can't tell a
// fast re-tap from terminal auto-repeat, so any grace short enough to separate taps
// is too short to bridge a hold. IN-GAME we keep the hold-synthesis below (movement
// needs the key to stay "down" between the terminal's auto-repeat bytes).
extern unsigned int menuactive;
extern unsigned int chat_on;            // hu_stuff.c: nonzero while typing a netgame chat message
extern boolean      netgame;            // g_game.c: true in a multiplayer (lockstep) game
static int          g_want_userlist = 0;  // Ctrl-U pressed in-game -> run between ticks
static int          g_want_page     = 0;  // Ctrl-P pressed in-game -> run between ticks

// --- Terminal mouse control (xterm SGR mouse: SyncTERM + the xterm family) ----
// Terminals report ABSOLUTE, screen-clamped cell positions (no relative deltas,
// and the host can't warp the pointer), so classic infinite mouse-look is out.
// The model is STEERING: the pointer's offset from screen-center sets a turn RATE
// (a virtual joystick). Edge-safe -- a pointer parked off-center keeps turning
// until moved back toward center (terminals send no "still here" report, so we
// hold the last offset; an idle timeout below stops a runaway spin when the
// pointer leaves the window). Mouse turning bypasses Doom's turn-accel ramp and
// the door's key-up-synthesis grace machinery, so it's smoother than the keys.
// (A relative-delta "native feel" model was tried and dropped -- with no way to
// recenter the pointer it stalls uselessly at the window edge.)
#define MOUSE_OFF        0
#define MOUSE_ON         1
#define MOUSE_STEER_MAX  110   // turn rate at the image edge (full deflection)
#define MOUSE_IDLE_MS    600   // steer relaxes to neutral after this long with no report
                               // (stops the runaway spin when the pointer leaves the
                               // window/loses focus -- terminals send no focus-out event)
static int      g_mouse_mode      = MOUSE_ON; // on/off ([input] mouse / -mouse)
static int      g_base_mouse_mode = MOUSE_ON; // house/built-in default (for per-user save diff)
static int      g_mouse_cli       = 0;       // -mouse given on CLI (saved prefs won't override)
static int      g_mouse_enabled   = 0;       // tracking actually turned on (mode != off)
static int      g_mouse_col = 0, g_mouse_row = 0; // last reported pointer cell (1-based)
static int      g_mouse_have      = 0;       // a report has arrived (steer stays neutral until then)
static int      g_mouse_buttons   = 0;       // button bitmask: bit0 left/fire, 1 right, 2 middle
static uint32_t g_mouse_last_ms = 0;         // now_ms() of the last report (idle-timeout for steer)

static void key_seen(unsigned char key)
{
	int      i;
	uint32_t t = now_ms();

	if (key == KEY_F4) {                      // door-level hotkey: cycle render tier
		cycle_video();                        // (handled here -- never reaches Doom)
		return;
	}
	if (key == KEY_PIPELINE) {                // Ctrl-T: cycle frame-pipeline depth
		toggle_pipeline();                    // (door-level; never reaches Doom)
		return;
	}
	if (key == KEY_STATS) {                   // Ctrl-S: toggle the live stats overlay
		toggle_stats();                       // (door-level; never reaches Doom)
		return;
	}
	if (key == KEY_MOUSE) {                   // Ctrl-O: toggle mouse steering on/off
		toggle_mouse();                       // (door-level; never reaches Doom)
		return;
	}
	if (key == KEY_USERLIST) {                // Ctrl-U: who's online. Defer to the main
		g_want_userlist = 1;                  // loop (between ticks) -- the screen is a
		return;                               // blocking modal, not safe to run from here.
	}
	if (key == KEY_PAGE) {                    // Ctrl-P: page a node (deferred likewise)
		g_want_page = 1;
		return;
	}

	if (menuactive || chat_on) {              // text entry: one discrete tap per byte
		keyq_push(1, key);                    // (no hold-synthesis: a repeated char
		keyq_push(0, key);                    // must register every time, e.g. "ll")
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

// Record an xterm SGR mouse report. 'button' is the SGR button code: low 2 bits
// = which button (0 left, 1 middle, 2 right; 3 = none/free-motion); +0x20 = a
// motion event; +0x40 = wheel; 0x04/0x08/0x10 = shift/alt/ctrl. col/row are
// 1-based cells; 'release' is set for the 'm'-terminated (button-up) form.
static void mouse_seen(int button, int col, int row, int release)
{
	int b = button & 0x03;
	int bit;

	g_mouse_col = col;
	g_mouse_row = row;
	g_mouse_have = 1;
	g_mouse_last_ms = now_ms();               // mark activity (steer idle-timeout)

	// Button state comes ONLY from real press/release events, never from motion
	// reports: a motion event (0x20) carries the held-button bits, but a stale or
	// phantom button in the terminal's state can make a plain move look like a
	// left-button hold -- and since a move never has a matching release, the fire
	// bit would stick on (continuous fire while steering). Position is updated
	// above regardless; here we only touch buttons on press/release.
	if (button & 0x20)                        // motion -> position only
		return;
	if ((button & 0x40) || b == 3)            // wheel, or a "no button" report
		return;
	bit = (b == 0) ? 0 : (b == 2) ? 1 : 2;    // left->fire, right->strafe, middle->forward
	if (release)
		g_mouse_buttons &= ~(1 << bit);
	else
		g_mouse_buttons |= (1 << bit);
}

// Horizontal center + half-width (in terminal cells) of the rendered game image,
// so steering is relative to the PICTURE, not the whole (often much larger)
// window. The text tier fills the text area; graphics tiers sit at g_img_col and
// are s_pxW px wide -- the image may be letterboxed, or (xterm sixel with no
// pixel geometry) anchored top-left, where the window center is well off it.
static void mouse_view(int *center, int *halfw)
{
	int span;

	if (g_mode == MODE_TEXT) {
		span    = g_text_cols;
		*center = g_text_cols / 2;
	} else {
		int cw = g_cell_w > 0 ? g_cell_w : (g_cfg_cell_w > 0 ? g_cfg_cell_w : 8);
		span    = s_pxW / cw;                 // image width in cells
		if (span < 4)
			span = 4;
		*center = g_img_col + span / 2;       // image's horizontal center cell
	}
	*halfw = span / 2;
	if (*halfw < 2)
		*halfw = 2;
}

// Steer model: pointer offset from the IMAGE center -> turn rate, scaled so the
// image edge is full deflection (so a small, letterboxed picture is fully usable
// without dragging the pointer across the surrounding void). Edge-safe -- a parked
// off-center pointer keeps turning (we hold the last offset) until re-centered;
// the idle timeout in DG_GetMouse stops a runaway spin when the pointer is gone.
static int mouse_steer_rate(void)
{
	int center, halfw, off, mag, dz;

	mouse_view(&center, &halfw);
	off = g_mouse_col - center;
	mag = (off < 0) ? -off : off;
	dz  = halfw / 10;                         // small (~10%) center deadzone
	if (dz < 1)
		dz = 1;
	if (mag <= dz)
		return 0;
	if (mag > halfw)
		mag = halfw;                          // past the image edge -> full turn
	mag = (mag - dz) * MOUSE_STEER_MAX / (halfw - dz);
	return (off < 0) ? -mag : mag;
}

// Project the mouse state to a Doom mouse event (buttons + turn dx + fwd/back dy).
// Returns 1 if I_GetEvent should post one this tic. Suppressed in menus/chat so a
// resting pointer can't drive the menu or steer while typing. data3 (forward/back)
// is left at 0 -- vertical mouse does nothing (movement stays on the keyboard).
int DG_GetMouse(int *buttons, int *dx, int *dy)
{
	if (!g_mouse_enabled)
		return 0;
	if (menuactive || chat_on)
		return 0;
	// Idle: the pointer stopped reporting (still, or it left the window -- there's
	// no focus-out event). Relax steering to neutral and drop any held button, so a
	// pointer abandoned off-center doesn't spin (or fire) forever.
	if (g_mouse_have && (uint32_t)(now_ms() - g_mouse_last_ms) > MOUSE_IDLE_MS) {
		g_mouse_buttons = 0;
		*buttons = 0;
		*dx = *dy = 0;
		return 1;
	}
	*buttons = g_mouse_buttons;
	*dx = g_mouse_have ? mouse_steer_rate() : 0;
	*dy = 0;
	return 1;
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
	// Ctrl-A..Ctrl-F stand in for F1..F6 on terminals whose function keys are
	// broken or absent -- notably SyncTERM <=1.4, which truncates F1-F5 to an
	// ambiguous bare "ESC[1". (Same fallback convention as zmachine.js.) These
	// bytes are otherwise unused here -- FIRE is Space, not Ctrl. So Ctrl-D
	// reaches the F4 tier-cycle and Ctrl-A the F1 help on any terminal. KEY_F1..
	// KEY_F10 are consecutive in doomkeys.h, so F1+(n-1) indexes F1..F6.
	if (c >= 0x01 && c <= 0x06)
		return (unsigned char)(KEY_F1 + (c - 1));
	// Ctrl-T (0x14) -- door-level "cycle frame-pipeline depth" hotkey (for A/B-ing
	// the remote-latency frame pacing live). Intercepted in key_seen; never Doom's.
	if (c == 0x14)
		return KEY_PIPELINE;
	// Ctrl-S (0x13) -- door-level "toggle live stats overlay" hotkey. We read the
	// socket raw (no terminal XON/XOFF flow control in the path), so 0x13 is free.
	if (c == 0x13)
		return KEY_STATS;
	// Ctrl-U (0x15) who's online, Ctrl-P (0x10) page -- door-level; never Doom's.
	if (c == 0x15)
		return KEY_USERLIST;
	if (c == 0x10)
		return KEY_PAGE;
	// Ctrl-O (0x0f) -- door-level "toggle mouse steering" hotkey. Never Doom's.
	if (c == 0x0f)
		return KEY_MOUSE;
	// While the user is TYPING text -- a netgame chat message (chat_on) or a menu
	// string like a save-game name (menuactive) -- the gameplay action keys must
	// arrive as the literal characters they type: a space has to be a space (not
	// FIRE), w/a/s/d/e/r their own letters (not move/strafe/use/run), letters keep
	// case. So skip the action remaps below in those modes; the printable
	// passthrough at the bottom delivers the raw byte. (Edit keys -- Enter/Esc/
	// Backspace, handled above -- still work for sending/cancelling/erasing.)
	if (!chat_on && !menuactive) {
		// WASD movement for a terminal (no mouse): W/S move forward/back (Doom's
		// up/down-arrow movement keys), A/D strafe, and turning stays on the LEFT/
		// RIGHT arrows. FIRE on Space, USE on E, always-run toggle on R (handled in
		// G_Responder). These shadow the lowercase letters, so cheat codes and save
		// names are typed in UPPERCASE -- folded back to lowercase just below, after
		// these checks, so the shifted form bypasses the binding.
		if (c == ' ')
			return KEY_FIRE;                          // fire
		if (c == 'w')
			return KEY_UPARROW;                       // move forward
		if (c == 's')
			return KEY_DOWNARROW;                     // move back
		if (c == 'a')
			return KEY_STRAFE_L;                      // strafe left
		if (c == 'd')
			return KEY_STRAFE_R;                      // strafe right
		if (c == 'e')
			return KEY_USE;                           // use / open
		if (c == 'r')
			return KEY_RUNTOGGLE;                     // toggle always-run
		if (c >= 'A' && c <= 'Z')
			return (unsigned char)(c - 'A' + 'a');    // CAPS -> lowercase (cheats)
	}
	if (c >= ' ' && c < 0x7f)
		return c;                               // printables pass through literally
	return 0;
}

// Map a CSI "[<n>~" parameter to a Doom function key. Terminals emit the
// function/nav keys this way: F1-F5 = 11-15, F6-F10 = 17-21, F11-F12 = 23-24
// (the same table zmachine.js uses). 0 = no mapping.
static unsigned char csi_tilde_key(int n)
{
	switch (n) {
		case 11: return KEY_F1;
		case 12: return KEY_F2;
		case 13: return KEY_F3;
		case 14: return KEY_F4;
		case 15: return KEY_F5;
		case 17: return KEY_F6;
		case 18: return KEY_F7;
		case 19: return KEY_F8;
		case 20: return KEY_F9;
		case 21: return KEY_F10;
		case 23: return KEY_F11;
		case 24: return KEY_F12;
		default: return 0;
	}
}

// Incremental escape-sequence parser state (bytes may split across reads).
static enum { ST_NORMAL, ST_ESC, ST_CSI, ST_APC, ST_APC_ESC } s_pstate = ST_NORMAL;
static unsigned s_apc_len;   // bytes swallowed in the current APC/string seq (bail if unterminated)
static char     s_csi_intro = 0;        // '[' or 'O' for the current CSI/SS3
static char     s_csi_par[24];          // accumulated parameter bytes ("[<n>~", SGR mouse "[<b;c;r")
static int      s_csi_parlen = 0;

// Kitty keyboard-protocol event fields, parsed from s_csi_par ("CSI key;mods:event ...").
// kitty_event: 1=press (default), 2=repeat, 3=release. kitty_mod: 1=none, 5=ctrl, 7=ctrl+shift.
static int kitty_event(void)
{
	const char *p = s_csi_par;
	int         field = 0;
	for (; *p; p++) {
		if (*p == ';') { field++; continue; }
		if (field == 1 && *p == ':')
			return atoi(p + 1) ? atoi(p + 1) : 1;
	}
	return 1;
}

static int kitty_mod(void)
{
	const char *p = strchr(s_csi_par, ';');
	return p ? (atoi(p + 1) ? atoi(p + 1) : 1) : 1;
}

// kitty reports the numpad with two DIFFERENT codepoint sets depending on NumLock:
//   NumLock ON  -> the DIGIT keys      KP_0..KP_9 / KP_. / KP_ENTER  (57399..57414)
//   NumLock OFF -> the FUNCTION keys   KP_LEFT/RIGHT/UP/DOWN/HOME/END/PGUP/PGDN/INSERT/DELETE
//                                      (57417..57426; KP_BEGIN=57427 arrives as CSI E, handled there)
// Terminals differ on NumLock state (foot with NumLock off sends the function set; Contour was
// tested NumLock on), so fold the function codepoint back to its physical digit-key codepoint and
// let the one keypad map serve both -- numpad keys behave identically regardless of NumLock.
// (Ground truth: foot 1.62.2.)
static int kitty_kp_normalize(int cp)
{
	switch (cp) {
		case 57417: return 57403;   // KP_LEFT    -> KP_4
		case 57418: return 57405;   // KP_RIGHT   -> KP_6
		case 57419: return 57407;   // KP_UP      -> KP_8
		case 57420: return 57401;   // KP_DOWN    -> KP_2
		case 57421: return 57408;   // KP_PAGE_UP -> KP_9
		case 57422: return 57402;   // KP_PAGE_DN -> KP_3
		case 57423: return 57406;   // KP_HOME    -> KP_7
		case 57424: return 57400;   // KP_END     -> KP_1
		case 57425: return 57399;   // KP_INSERT  -> KP_0
		case 57426: return 57409;   // KP_DELETE  -> KP_.
		case 57427: return 57404;   // KP_BEGIN   -> KP_5 (defensive; foot sends CSI E for this)
	}
	return cp;
}

// Dispatch a key event from any path with a real key-up (kitty protocol, SyncTERM evdev). ev:
// 1=press, 2=repeat, 3=release. Door hotkeys fire on a fresh press (key_seen handles them); game
// keys get explicit key-down/up so movement holds and Doom's own turn ramp runs -- bypassing the
// byte-path key-up-synthesis grace machinery. Menus/text get a discrete tap per press/repeat.
static void key_dispatch(unsigned char key, int ev)
{
	if (!key)
		return;
	if (key == KEY_F4 || key == KEY_PIPELINE || key == KEY_STATS ||
	    key == KEY_MOUSE || key == KEY_USERLIST || key == KEY_PAGE) {
		if (ev == 1)                          // door hotkey: act on a fresh press only
			key_seen(key);
		return;
	}
	// Turn keys in gameplay: RTT-adaptive.  On a fast link (or while strafing -- the engine needs
	// the scancode to side-step) drive the native turn key (falls through below); on a laggy link
	// inject the constant synthetic turn (sd_synth_turn) instead -- deterministic, no overshoot.
	if ((key == KEY_LEFTARROW || key == KEY_RIGHTARROW) && !menuactive && !chat_on) {
		int dir = (key == KEY_RIGHTARROW) ? 1 : -1;
		if (ev == 3) {                        // release: clear both models for this direction
			if (sd_synth_turn == dir)
				sd_synth_turn = 0;
			if (g_key_down[key]) { keyq_push(0, key); g_key_down[key] = 0; }
			return;
		}
		if (!g_key_down[KEY_RALT] && !sd_turn_native()) {   // laggy + not strafing -> synthetic
			sd_synth_turn = dir;
			if (g_key_down[key]) { keyq_push(0, key); g_key_down[key] = 0; }   // drop any native hold
			return;
		}
		// else fall through to the native hold below (low latency, or strafing)
	}
	if (ev == 3) {                            // key-up: release a held key-down
		if (g_key_down[key]) {
			keyq_push(0, key);
			g_key_down[key] = 0;
		}
		return;
	}
	if (menuactive || chat_on) {              // menu nav / text entry: discrete tap
		keyq_push(1, key);
		keyq_push(0, key);
		return;
	}
	if (!g_key_down[key]) {                 // in-game: hold until the explicit key-up
		keyq_push(1, key);
		g_key_down[key] = 1;
	}
}

// US-QWERTY: evdev key code -> { unshifted, shifted } ASCII byte (0 = not a simple ASCII key).
// Covers the main alnum/punct block (codes 1..57); arrows/nav/F-keys/keypad are handled in
// evdev_edge(). Lets us translate a physical key edge into the byte map_ascii() expects, so the
// WASD remap, cheat/save text entry, weapon digits and Ctrl hotkeys all behave as on the byte path.
static const char evdev_ascii[88][2] = {
	{ 0, 0 },                                                                            // 0  reserved
	{ 27, 27 },                                                                          // 1  ESC
	{ '1', '!' }, { '2', '@' }, { '3', '#' }, { '4', '$' }, { '5', '%' },                // 2-6
	{ '6', '^' }, { '7', '&' }, { '8', '*' }, { '9', '(' }, { '0', ')' },                // 7-11
	{ '-', '_' }, { '=', '+' },                                                          // 12-13
	{ 8, 8 }, { 9, 9 },                                                                  // 14 BkSp, 15 Tab
	{ 'q', 'Q' }, { 'w', 'W' }, { 'e', 'E' }, { 'r', 'R' }, { 't', 'T' }, { 'y', 'Y' },  // 16-21
	{ 'u', 'U' }, { 'i', 'I' }, { 'o', 'O' }, { 'p', 'P' },                              // 22-25
	{ '[', '{' }, { ']', '}' },                                                          // 26-27
	{ '\r', '\r' },                                                                      // 28 Enter
	{ 0, 0 },                                                                            // 29 LeftCtrl
	{ 'a', 'A' }, { 's', 'S' }, { 'd', 'D' }, { 'f', 'F' }, { 'g', 'G' },                // 30-34
	{ 'h', 'H' }, { 'j', 'J' }, { 'k', 'K' }, { 'l', 'L' },                              // 35-38
	{ ';', ':' }, { '\'', '"' }, { '`', '~' },                                           // 39-41
	{ 0, 0 },                                                                            // 42 LeftShift
	{ '\\', '|' },                                                                       // 43
	{ 'z', 'Z' }, { 'x', 'X' }, { 'c', 'C' }, { 'v', 'V' }, { 'b', 'B' },                // 44-48
	{ 'n', 'N' }, { 'm', 'M' },                                                          // 49-50
	{ ',', '<' }, { '.', '>' }, { '/', '?' },                                            // 51-53
	{ 0, 0 },                                                                            // 54 RightShift
	{ '*', '*' },                                                                        // 55 KP*
	{ 0, 0 },                                                                            // 56 LeftAlt
	{ ' ', ' ' },                                                                        // 57 Space
};

// Apply one physical key edge (down=1 press / 0 release). Modifiers update g_evdev_mods; arrows/
// nav/F-keys map straight to Doom keys; everything else folds to an ASCII byte (honouring Shift/
// Ctrl) and runs through map_ascii(). All of it dispatches via key_dispatch() (event 1=press,
// 3=release) so the hold/hotkey/menu logic is shared with the kitty path.
static void evdev_edge(int code, int down)
{
	int           ev = down ? 1 : 3;
	unsigned char k  = 0, c;

	// Modifiers. Shift = Run + Strafe (held): assert BOTH the engine's Run (KEY_RSHIFT/key_speed)
	// and Strafe (KEY_RALT/key_strafe) scancodes. Run speeds forward/back; Strafe turns the Left/
	// Right arrows into strafe (no-op on its own). Both are INTERNAL scancodes fed to the engine --
	// never a real Alt back to SyncTERM -- so this sidesteps SyncTERM's modifier reservation.
	// (Physical Alt is only tracked, never forwarded: SyncTERM reserves Alt+arrow [resize] and
	// Alt+letter [hangup/...] locally, and a held physical Alt would trigger those and lose its
	// key-up to the resize mode -> a latched-down "freeze".) The Shift bit also gates the fold.
	switch (code) {
		case 29: case 97:  if (down)
				g_evdev_mods |= EVDEV_MOD_CTRL; else
				g_evdev_mods &= ~EVDEV_MOD_CTRL; return;
		case 42: case 54:                                // L/R Shift = Run (KEY_RSHIFT) + Strafe (KEY_RALT)
			if (down)
				g_evdev_mods |= EVDEV_MOD_SHIFT;
			else
				g_evdev_mods &= ~EVDEV_MOD_SHIFT;
			key_dispatch(KEY_RSHIFT, down ? 1 : 3);
			key_dispatch(KEY_RALT,   down ? 1 : 3);
			return;
		case 56: case 100: if (down)                     // L/R physical Alt: track only (NOT forwarded -- see above)
				g_evdev_mods |= EVDEV_MOD_ALT; else
				g_evdev_mods &= ~EVDEV_MOD_ALT; return;
	}

	// Drop the enable-time held-key resync (see SD_EVDEV_SETTLE_MS): only press edges; releases pass.
	if (down && g_evdev_settling) {
		if ((uint32_t)(now_ms() - g_evdev_enabled_ms) < SD_EVDEV_SETTLE_MS)
			return;
		g_evdev_settling = 0;
	}

	switch (code) {                                  // arrows / navigation / function keys
		case 103: k = KEY_UPARROW;    break;
		case 108: k = KEY_DOWNARROW;  break;
		case 105: k = KEY_LEFTARROW;  break;
		case 106: k = KEY_RIGHTARROW; break;
		case 102: k = KEY_HOME;       break;
		case 107: k = KEY_END;        break;
		case 104: k = KEY_PGUP;       break;
		case 109: k = KEY_PGDN;       break;
		case 110: k = KEY_INS;        break;
		case 111: k = KEY_DEL;        break;
		case 59:  k = KEY_F1;  break;  case 60: k = KEY_F2;  break;  case 61: k = KEY_F3;  break;
		case 62:  k = KEY_F4;  break;  case 63: k = KEY_F5;  break;  case 64: k = KEY_F6;  break;
		case 65:  k = KEY_F7;  break;  case 66: k = KEY_F8;  break;  case 67: k = KEY_F9;  break;
		case 68:  k = KEY_F10; break;  case 87: k = KEY_F11; break;  case 88: k = KEY_F12; break;
	}
	if (k) {
		key_dispatch(k, ev);
		return;
	}

	// The numpad ARROWS (8/2/4/6) alias the main arrows: move/turn in gameplay AND navigate menus
	// (key_dispatch self-routes menu=tap vs gameplay=hold). Trade-off: can't type 8/2/4/6 from the
	// numpad in a save name -- rare; the top row + KP5/7/9/1/3/0 still do.
	{
		unsigned char nav = 0;
		switch (code) {
			case 72: nav = KEY_UPARROW;    break;  // KP8 -> forward / menu-up
			case 80: nav = KEY_DOWNARROW;  break;  // KP2 -> back / menu-down
			case 75: nav = KEY_LEFTARROW;  break;  // KP4 -> turn left / menu-left
			case 77: nav = KEY_RIGHTARROW; break;  // KP6 -> turn right / menu-right
		}
		if (nav) {
			key_dispatch(nav, ev);
			return;
		}
	}

	// In a MENU/text context the numpad's gray NAV labels apply: KP7/KP1/KP9/KP3 drive
	// Home/End/PgUp/PgDn like the main nav keys -- numpad Home/End jump to first/last item --
	// instead of folding to a digit.  In gameplay they stay digits (below).  (SyncTERM sends the
	// physical keypad scancode NumLock-agnostic, so this is the evdev twin of the kitty menu path.)
	if (menuactive || chat_on) {
		unsigned char msc = (code == 71) ? KEY_HOME :   // KP7
		                    (code == 79) ? KEY_END  :   // KP1
		                    (code == 73) ? KEY_PGUP :   // KP9
		                    (code == 81) ? KEY_PGDN : 0; // KP3
		if (msc) {
			key_dispatch(msc, ev);
			return;
		}
	}

	c = 0;
	switch (code) {                                  // keypad -> ASCII (assume NumLock; matches kitty PUA folding)
		case 71: c = '7'; break; case 72: c = '8'; break; case 73: c = '9'; break; case 74: c = '-'; break;
		case 75: c = '4'; break; case 76: c = '5'; break; case 77: c = '6'; break; case 78: c = '+'; break;
		case 79: c = '1'; break; case 80: c = '2'; break; case 81: c = '3'; break; case 82: c = '0'; break;
		case 83: c = '.'; break; case 96: c = '\r'; break; case 98: c = '/'; break;
	}
	if (c == 0) {                                    // simple ASCII key via the US-QWERTY table
		// Shift folds to upper for menu/text entry ONLY -- in gameplay letters stay lowercase so
		// map_ascii's WASD/action remap still fires while Shift is held for Run (Shift+W = run fwd).
		int shifted = (g_evdev_mods & EVDEV_MOD_SHIFT) && (menuactive || chat_on);
		if (code < 0 || code >= (int)(sizeof evdev_ascii / sizeof evdev_ascii[0]))
			return;
		c = evdev_ascii[code][shifted ? 1 : 0];
		if (c == 0)
			return;
	}
	// Ctrl + letter -> its control byte (Ctrl-T pipeline, Ctrl-S stats, Ctrl-O mouse, Ctrl-A..F =
	// F1..F6, ...) -- the same fold the kitty path applies, so map_ascii's hotkey layer fires.
	if ((g_evdev_mods & EVDEV_MOD_CTRL) && (c | 0x20) >= 'a' && (c | 0x20) <= 'z')
		c &= 0x1f;
	k = map_ascii(c);
	if (k)
		key_dispatch(k, ev);
}

// A physical key report (CSI = Pk[;Pk...]K press / k release): apply every coalesced edge.
static void evdev_report(int down)
{
	const char *p = s_csi_par;

	while (*p && (*p < '0' || *p > '9'))             // skip the leading '=' marker
		p++;
	while (*p) {
		int code = 0;
		while (*p >= '0' && *p <= '9') { code = code * 10 + (*p - '0'); p++; }
		evdev_edge(code, down);
		while (*p && (*p < '0' || *p > '9'))         // skip the ';' separators
			p++;
	}
}

static void parse_byte(unsigned char c)
{
	switch (s_pstate) {
		case ST_NORMAL:
			if (c == 0x1b) { s_pstate = ST_ESC; return; }
			{ unsigned char k = map_ascii(c); if (k)
				  key_seen(k); }
			return;
		case ST_ESC:
			if (c == '[' || c == 'O') {
				s_csi_intro = c; s_csi_parlen = 0; s_pstate = ST_CSI; return;
			}
			// String sequences -- APC (_), DCS (P), OSC (]), PM (^) -- are terminated
			// by ST (ESC \) and carry no keystrokes; swallow them whole. (SyncTERM's
			// C;L cache-list reply is an APC and literally contains "C;L", so leaking
			// it as keys typed an 'l' -> Load Game. Must consume, not pass through.)
			if (c == '_' || c == 'P' || c == ']' || c == '^') {
				s_pstate = ST_APC; s_apc_len = 0; return;
			}
			// lone ESC then a byte: treat ESC as Escape, reprocess the byte
			key_seen(KEY_ESCAPE);
			s_pstate = ST_NORMAL;
			parse_byte(c);
			return;
		case ST_APC:
			if (c == 0x1b) { s_pstate = ST_APC_ESC; return; }   // maybe the ST terminator
			if (c == 0x07) { s_pstate = ST_NORMAL; return; }    // BEL also ends OSC/strings
			if (++s_apc_len > 1u << 20)
				s_pstate = ST_NORMAL;                           // unterminated -> bail (no input lockup)
			return;                                             // swallow body byte
		case ST_APC_ESC:
			s_pstate = (c == '\\') ? ST_NORMAL : ST_APC;        // ESC '\' = ST -> done
			return;
		case ST_CSI:
			// Parameter/intermediate bytes (0x20-0x3f) precede the final byte;
			// gather them so "[15~" (F5) etc. survive across the final dispatch.
			if (c >= 0x20 && c <= 0x3f) {
				if (s_csi_parlen < (int)sizeof(s_csi_par) - 1)
					s_csi_par[s_csi_parlen++] = (char)c;
				return;
			}
			if (c >= 0x40 && c <= 0x7e) {           // final byte
				unsigned char k = 0;
				s_csi_par[s_csi_parlen] = '\0';
				if (s_csi_intro == 'O') {
					// SS3: F1-F4, and arrows on some terminals.
					switch (c) {
						case 'P': k = KEY_F1; break;
						case 'Q': k = KEY_F2; break;
						case 'R': k = KEY_F3; break;
						case 'S': k = KEY_F4; break;
						case 'A': k = KEY_UPARROW;    break;
						case 'B': k = KEY_DOWNARROW;  break;
						case 'C': k = KEY_RIGHTARROW; break;
						case 'D': k = KEY_LEFTARROW;  break;
						default:  break;
					}
				} else {                            // CSI '['
					switch (c) {
						case 'A': k = KEY_UPARROW;    break;
						case 'B': k = KEY_DOWNARROW;  break;
						case 'C': k = KEY_RIGHTARROW; break;
						case 'D': k = KEY_LEFTARROW;  break;
						case 'R':                            // CSI r;c R = DSR report = a frame was consumed
							if (g_dsr_stale > 0) {
								g_dsr_stale--;               // a late report for a reclaimed frame -> drop it
							} else if (dsr_inflight() > 0) {
								uint32_t nowm = now_ms();
								uint32_t rtt = nowm - g_dsr_ts[g_dsr_tail];
								g_dsr_tail = (g_dsr_tail + 1) % DSR_RING;
								// Fold the round-trip into the smoothed RTT + windowed baseline (shared,
								// termgfx/pace.c).  stale-reject is OFF: rejecting samples below EMA/3
								// ratchets the EMA -- once a slow JXL frame spikes it, every subsequent
								// good (low) sample is discarded and it can never recover (seen latched
								// ~725ms on a LAN whose true floor is ~20ms).  Reclaimed-frame replies are
								// already dropped above via g_dsr_stale, so the filter guarded nothing.
								if (termgfx_rtt_sample(&g_rtt_ms, &g_rtt_min, &g_rtt_min_at, &g_rt_high,
								                       rtt, nowm, 0, RTT_MIN_WINDOW))
									auto_depth_update();     // re-evaluate the auto pipeline depth
							}
							g_ack_deadline = now_ms() + 250; // progress -> refresh the safety deadline
							break;
						case '~': k = csi_tilde_key(atoi(s_csi_par)); break;
						// Kitty keyboard protocol: F1/F2/F4 arrive as CSI P/Q/S, Home/End as CSI H/F
						// (only once negotiated). F4 = the tier-cycle sentinel, like SS3 S.
						case 'P': if (g_kitty_active)
								k = KEY_F1; break;
						case 'Q': if (g_kitty_active)
								k = KEY_F2; break;
						case 'S': if (g_kitty_active && s_csi_par[0] != '?')
								k = KEY_F4; break;                                              // '?' = XTSMGRAPHICS reply
						case 'H': if (g_kitty_active)
								k = KEY_HOME; break;
						case 'F': if (g_kitty_active)
								k = KEY_END; break;
						case 'E':                            // numpad-5, NumLock off = KP_Begin -> '5' (matches NumLock-on KP5)
							if (g_kitty_active)
								k = map_ascii('5');
							break;
						case 'K':                            // SyncTERM physical key PRESS report (CSI = <code> K)
							if (g_evdev_active && s_csi_par[0] == '=')
								evdev_report(1);
							break;
						case 'k':                            // SyncTERM physical key RELEASE report
							if (g_evdev_active && s_csi_par[0] == '=')
								evdev_report(0);
							break;
						case 'u':                            // kitty key event, or the CSI?u support reply
							if (s_csi_par[0] == '?') {       // progressive-flags reply -> enable + push our flags
								if (!g_kitty_active && !g_evdev_active) {   // evdev (if negotiated) takes precedence
									g_kitty_active = 1;
									emit_all("\x1b[>11u", 6); // disambiguate | report-events | report-all-keys
								}
							} else {
								int cp  = atoi(s_csi_par);
								int mod = kitty_mod();
								int ev  = kitty_event();
								if (cp > 0) {
									if (cp == 57441 || cp == 57447) {    // kitty L/R Shift (PUA) = Run + Strafe
										key_dispatch(KEY_RSHIFT, ev);    // (KEY_RSHIFT + KEY_RALT, both internal;
										key_dispatch(KEY_RALT, ev);      // see evdev note)
										break;
									}
									// Lone Alt is NOT forwarded -- SyncTERM reserves Alt+key (see evdev note).
									// In a MENU/text context the NumLock-off nav cluster does its NAV function
									// (KEY_HOME/END/PGUP/PGDN) like the main nav keys -- numpad Home/End jump to the
									// first/last item -- instead of folding to a digit.  In gameplay it falls through
									// to the digit fold below (matches the evdev numpad, always its digit).
									if (menuactive || chat_on) {
										unsigned char msc = (cp == 57423) ? KEY_HOME :   // numpad-7
										                    (cp == 57424) ? KEY_END  :   // numpad-1
										                    (cp == 57421) ? KEY_PGUP :   // numpad-9
										                    (cp == 57422) ? KEY_PGDN : 0; // numpad-3
										if (msc) {
											key_dispatch(msc, ev);
											break;
										}
									}
									cp = kitty_kp_normalize(cp);   // NumLock-OFF numpad function -> physical digit-key codepoint
									// Numpad ARROWS alias the main arrows: move/turn in gameplay AND navigate
									// menus (KP8=57407, KP2=57401, KP4=57403, KP6=57405).
									{
										unsigned char nav = (cp == 57407) ? KEY_UPARROW :
										                    (cp == 57401) ? KEY_DOWNARROW :
										                    (cp == 57403) ? KEY_LEFTARROW :
										                    (cp == 57405) ? KEY_RIGHTARROW : 0;
										if (nav) {
											key_dispatch(nav, ev);
											break;
										}
									}
									if (cp >= 57399 && cp <= 57414) {    // numpad PUA -> ASCII
										static const char kp[] = "0123456789./*-+\r";
										cp = (unsigned char)kp[cp - 57399];
									}
									if ((mod == 5 || mod == 7) &&
									    ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z')))
										key_dispatch(map_ascii((unsigned char)(cp & 0x1f)), ev); // ctrl+letter
									else if (cp < 128)
										key_dispatch(map_ascii((unsigned char)cp), ev);
								}
							}
							break;
						case 'M':                            // xterm SGR mouse: "[<b;col;row" + M(press/motion)
						case 'm':                            //                              + m(release)
							if (s_csi_par[0] == '<') {
								int b = 0, x = 0, y = 0;
								sscanf(s_csi_par + 1, "%d;%d;%d", &b, &x, &y);
								mouse_seen(b, x, y, c == 'm');
							}
							break;
						default:  break;
					}
				}
				if (k) {
					if (g_kitty_active)
						key_dispatch(k, kitty_event());   // real key-up: hold-to-move + native turn
					else
						key_seen(k);
				}
				s_pstate = ST_NORMAL;
			}
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
			termgfx_audio_feed(sd_audio, buf, (int)n);   // resolve the audio cap probe
			{
				static int sd_prev_tier = -2;            // log the negotiated tier once
				int        t = termgfx_audio_tier(sd_audio);
				if (t != sd_prev_tier) {
					if (t >= 1 && sd_prev_tier < 1) {
						extern void sd_music_tier_ready(void);   // i_termmusic.c
						sd_music_tier_ready();                   // play deferred title song
					}
					sd_prev_tier = t;
					dlog("audio: tier=%d (%s)", t,
					     t == 1 ? "digital -- SFX + music should play" :
					     t == 0 ? "audio APC but no libsndfile -- silent" :
					     "no audio APC reply");
				}
			}
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
	if (g_hangup) {
		M_SaveDefaults();        // persist Doom's options (screen size, messages,
		exit(0);                 // detail...) -- I_AtExit's saver only runs via
	}                            // I_Quit, which a hangup/time-limit exit skips
}

// Honor the user's BBS time limit (-t<seconds>, from %T): exit when it's spent.
static void check_timelimit(void)
{
	if (g_time_limit_ms &&
	    (int32_t)(now_ms() - (g_start_ms + g_time_limit_ms)) >= 0) {
		static const char msg[] = "\x1b[?25h\r\nYour time on the BBS has expired.\r\n";
		emit_all(msg, sizeof(msg) - 1);
		M_SaveDefaults();        // persist Doom's options before the timed exit
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
static int g_cterm_version = 0; // CTerm/SyncTERM version maj*1000+min (DA1 reply); 0 = unknown

#ifdef WITH_JXL
// One-time startup handshake: ask SyncTERM whether it can decode JXL.
// Reply is the CTerm state report "ESC [ = 1 ; {0,1} - n". Bytes seen during
// the window are also fed to the input parser so a keypress isn't lost (the
// CSI reply is inert there: it ends on the 'n' final and yields no key).
static int probe_jxl(void)
{
	unsigned char acc[256];
	int           al = 0, result = -1;
	uint32_t      deadline = now_ms() + 600;

	emit_all(termgfx_query_jxl, strlen(termgfx_query_jxl));
	while (result < 0) {
		int32_t       rem = (int32_t)(deadline - now_ms());
		unsigned char buf[256];
		ssize_t       n, i;

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
		result = termgfx_caps_parse_jxl(acc, al);
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
static int g_have_sixel = 0;    // sixel available (probed) -- for the F4 graphics-tier cycle

// One-time graphics-capability probe. Two device-attribute queries detect sixel
// support two ways:
//   * DA1 "ESC[c" -> xterm/WT reply "ESC[?<params>c" with parameter 4, and
//   * CTDA "ESC[<c" -> SyncTERM reply "ESC[<0;<caps>c" with capability 4 (pixel
//     ops = sixel/PPM; advertised since SyncTERM 1.1).
// The CTerm *version* reply "ESC[=67;84;...c" (params spell "CTerm") is captured
// too, for g_cterm_version / g_is_syncterm. Probe bytes are also fed to the input
// parser (the CSI replies are inert there, ending on 'c'). Returns nonzero if
// sixel is supported.
//
// SyncTERM renders sixel fine once DECSDM (private mode 80) scrolling is disabled
// (see apply_sixel_scroll) -- before that fix the per-frame page-scroll made it
// look broken, which is why this CTDA probe used to be left out. SyncTERM 1.4+
// still prefers JXL: the tier ladder probes JXL first, so cap-4 here only
// auto-selects sixel on the older, no-JXL SyncTERM versions (and on xterm/WT).
static int probe_sixel(void)
{
	static const char q[] = "\x1b[c\x1b[<c";    // DA1 (xterm) + CTDA (SyncTERM caps)
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
		// Replies (any may appear; a SyncTERM sends both '=' and '<'):
		//   ESC[?<params>c  xterm/WT DA1   -- parameter 4 = sixel
		//   ESC[<0;<caps>c  SyncTERM CTDA  -- capability 4 = pixel ops (sixel/PPM)
		//   ESC[=67;84;..c  SyncTERM CTerm version (params spell "CTerm")
		// Scan the whole buffer and process every complete reply (idempotent): the
		// '?'/'<' replies are definitive (set done); the '=' version reply is not,
		// so we keep reading for the '<' CTDA reply that carries the sixel cap.
		for (j = 0; j + 2 < al; j++) {
			unsigned char marker = acc[j + 2];
			int           p[16], np = 0, num = -1, k;
			if (acc[j] != 0x1b || acc[j + 1] != '['
			    || (marker != '?' && marker != '=' && marker != '<'))
				continue;
			for (k = j + 3; k < al; k++) {
				unsigned char ch = acc[k];
				if (ch >= '0' && ch <= '9') {
					num = (num < 0 ? 0 : num * 10) + (ch - '0');
				} else if (ch == ';') {
					if (np < 16)
						p[np++] = (num < 0 ? 0 : num);
					num = -1;
				} else {                         // final byte
					if (np < 16)
						p[np++] = (num < 0 ? 0 : num);
					if (ch == 'c') {
						int m;
						if (marker == '?' || marker == '<') {
							for (m = 0; m < np; m++) {
								if (p[m] == 4)
									found = 1;       // sixel: DA1 param 4 / CTDA cap 4
								// CTDA cap 8 = physical key (evdev) reports. Prefer over kitty (a
								// SyncTERM advertising it doesn't speak kitty): enable reports (=1h)
								// + suppress translated bytes (=2h) so keys arrive only as the
								// CSI = <code> K/k edges decoded in evdev_report().
								if (p[m] == 8 && marker == '<' && !g_evdev_active && !g_kitty_active) {
									g_evdev_active     = 1;
									g_evdev_enabled_ms = now_ms();
									g_evdev_settling   = 1;
									emit_all("\x1b[=1h\x1b[=2h", 10);
								}
							}
							done = 1;                // definitive answer
						} else if (np >= 7 && p[0] == 67 && p[1] == 84 &&
						           p[2] == 101 && p[3] == 114 && p[4] == 109) {
							g_cterm_version = p[5] * 1000 + p[6];    // "CTerm" maj.min
							if (g_cterm_version >= 1002)             // SyncTERM >= 1.2 has APC PPM
								g_is_syncterm = 1;
							// version only -- keep reading for the '<' CTDA reply
						}
					}
					break;
				}
			}
			// no break: a SyncTERM emits the '=' reply AND the '<' reply; reach both.
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

// Restore auto-wrap + cursor on exit (so the BBS prompt behaves after Doom quits),
// and turn off mouse reporting if we enabled it (else the BBS sees stray reports).
// Blocking-push whatever is staged in g_out (used only at exit, where the normal
// drop-on-behind out_flush would discard it). emit_all waits on the socket.
static void out_drain_blocking(void)
{
	if (out_pending())
		emit_all((const char *)g_out + g_out_off, g_out_len - g_out_off);
	g_out_len = 0;
	g_out_off = 0;
}

static void terminal_restore(void)
{
	// Stop the looping music BEFORE we leave, or SyncTERM keeps playing the track
	// after the door exits. termgfx_audio_music_stop is a hard-clear (fade 0) Flush;
	// it's staged in g_out like everything else, and the staged buffer is dropped at
	// exit -- so blocking-drain it now (the Flush is tiny; any pending frame ahead of
	// it goes too). Mirrors SyncDuke's syncduke_term_restore.
	if (sd_audio != NULL) {
		termgfx_audio_music_stop(sd_audio);
		out_drain_blocking();
	}
	if (g_mouse_enabled)
		emit_all("\x1b[?1003l\x1b[?1006l", 16);
	if (g_sixel_scroll_off)
		emit_all("\x1b[?80h", 6);        // restore default sixel scrolling for the BBS
	if (g_kitty_active)
		emit_all("\x1b[<u", 4);          // pop the kitty keyboard flags we pushed
	if (g_evdev_active)
		emit_all("\x1b[=2l\x1b[=1l", 10);   // restore translation, disable physical key reports
	emit_all("\x1b[?7h\x1b[?25h", 11);
}

// Configure the block-char text tier: mode from -mode, else by terminal charset
// (UTF-8 -> blocks+shades, CP437 -> half-block). Used by -text and as the
// no-JXL fallback.
static void setup_text_mode(void)
{
	// Default: pick the tier from the client charset (terminal.ini). UTF-8 gets
	// blocks+shades -- 2x2 detail using only Unicode-1.0 block/shade glyphs that
	// every font (incl. Windows conhost's Consolas) carries; the hi-res sextant
	// tier needs U+1FB00 glyphs most fonts lack, so it's opt-in (F4 / -mode sextant)
	// rather than the default. CP437 terminals get the half-block. Assumes the door
	// is "Translate Character Set: No" (EX_BIN) so native UTF-8 reaches the terminal.
	rt_mode_t mode = g_rt_mode_set ? g_rt_mode
	               : (g_rt_charset == RT_UTF8 ? RT_BLOCKS : RT_HALF);
	// Cap the text grid: a maximized terminal (e.g. 561x105) otherwise makes each
	// frame hundreds of KB (1.5+ MB/s), which floods the link and trips the dead-
	// client watchdog. Beyond Doom's own detail, extra cells add only bytes -- so
	// bound them (configurable via [video] text_max_cols/text_max_rows; 0 = off).
	int tcols = g_cols, trows = s_lines;
	if (g_text_max_cols > 0 && tcols > g_text_max_cols)
		tcols = g_text_max_cols;
	if (g_text_max_rows > 0 && trows > g_text_max_rows)
		trows = g_text_max_rows;
	g_text_cols = tcols;                 // the stats overlay anchors to this, not the full g_cols
	rt_config(tcols, trows, mode, g_colors, g_rt_charset);
	g_mode = MODE_TEXT;
}

// Build the render-tier cycle list from the startup tier: if startup chose a
// graphics tier, the cycle is [graphics, CP437 half, quadrant, sextant]; if it
// fell back to text, just the three text tiers. Charset only matters for the
// half-block glyph (quadrant/sextant carry their own UTF-8 tables).
static void build_video_states(void)
{
	g_vstate_n = 0;
	if (g_mode != MODE_TEXT) {
		g_vstates[g_vstate_n].mode    = g_mode;
		g_vstates[g_vstate_n].fullres = (g_mode == MODE_SIXEL) ? g_sixel_fullres : 0;
		g_vstates[g_vstate_n].label   = (g_mode == MODE_SIXEL && g_sixel_fullres) ? "sixel-full"
		                                : mode_name(g_mode);   // "jxl" / "sixel" / "ppm"
		g_vstate_n++;
		// Non-SyncTERM sixel: offer the OTHER vertical-scaling variant as an F4 stop. The
		// default (pan=2 half-res) renders right on terminals that honor the sixel raster
		// aspect ratio; "sixel-full" (1:1, no scaling) is for those that ignore it (WezTerm).
		if (g_mode == MODE_SIXEL && !g_is_syncterm) {
			g_vstates[g_vstate_n].mode    = MODE_SIXEL;
			g_vstates[g_vstate_n].fullres = !g_sixel_fullres;
			g_vstates[g_vstate_n].label   = g_sixel_fullres ? "sixel" : "sixel-full";
			g_vstate_n++;
		}
#ifdef WITH_JXL
		// Offer sixel as a second graphics tier when JXL is the startup tier and the
		// terminal also speaks sixel, so the player can A/B the two with F4.
		if (g_mode == MODE_JXL && g_have_sixel) {
			g_vstates[g_vstate_n].mode    = MODE_SIXEL;
			g_vstates[g_vstate_n].fullres = 0;
			g_vstates[g_vstate_n].label   = "sixel";
			g_vstate_n++;
		}
#endif
	}
	// Half-block renders in either charset (CP437 0xDF / UTF-8 U+2580), so use the
	// terminal's own charset -- a raw 0xDF is invalid UTF-8 on a UTF-8 terminal.
	g_vstates[g_vstate_n].mode = MODE_TEXT; g_vstates[g_vstate_n].rt = RT_HALF;
	g_vstates[g_vstate_n].cs = g_rt_charset; g_vstates[g_vstate_n].label = "half-block"; g_vstate_n++;
	// Blocks+shades: 2x2 detail using only CP437-safe glyphs -- works in either
	// charset, and (unlike quadrant) renders on font-limited terminals like conhost.
	g_vstates[g_vstate_n].mode = MODE_TEXT; g_vstates[g_vstate_n].rt = RT_BLOCKS;
	g_vstates[g_vstate_n].cs = g_rt_charset; g_vstates[g_vstate_n].label = "blocks+shades"; g_vstate_n++;
	// Quadrant/sextant glyphs are UTF-8 only -- no point cycling a CP437 terminal
	// (e.g. SyncTERM) through them; they'd just render as missing-glyph boxes.
	if (g_rt_charset == RT_UTF8) {
		g_vstates[g_vstate_n].mode = MODE_TEXT; g_vstates[g_vstate_n].rt = RT_QUADRANT;
		g_vstates[g_vstate_n].cs = RT_UTF8; g_vstates[g_vstate_n].label = "quadrant"; g_vstate_n++;
		g_vstates[g_vstate_n].mode = MODE_TEXT; g_vstates[g_vstate_n].rt = RT_SEXTANT;
		g_vstates[g_vstate_n].cs = RT_UTF8; g_vstates[g_vstate_n].label = "sextant";  g_vstate_n++;
	}
	g_vstate_i = 0;                       // index 0 == the startup tier
}

// Advance to the next render tier and show a brief centered label so the user
// can see (and read) what they switched to. Driven by the F4 hotkey.
static void cycle_video(void)
{
	struct vstate *v;
	char           line[80], buf[160];
	int            pad, n;

	if (g_vstate_n == 0)
		build_video_states();
	g_vstate_i = (g_vstate_i + 1) % g_vstate_n;
	v = &g_vstates[g_vstate_i];

	if (v->mode == MODE_TEXT) {
		g_rt_mode     = v->rt;
		g_rt_mode_set = 1;
		g_rt_charset  = v->cs;
		setup_text_mode();               // (re)configures render_text + sets g_mode
	} else {
		g_mode = v->mode;
		if (g_mode == MODE_SIXEL && g_sixel_fullres != v->fullres) {
			g_sixel_fullres = v->fullres;   // persist the vertical-scaling choice per-user
			sd_save_user_prefs();
		}
	}
	compute_geometry();
	apply_sixel_scroll();                // toggle DECSDM (mode 80) for the sixel tier
	g_dsr_tail = g_dsr_head;             // clear the pipeline -> let the next frame emit immediately

	// Clear (drop the prior tier's bitmap/glyphs) and dwell on a readable label.
	emit_all("\x1b[2J\x1b[H", 7);
	snprintf(line, sizeof(line), "  Video: %s  ", v->label);
	pad = (overlay_cols() - (int)strlen(line)) / 2;
	if (pad < 0)
		pad = 0;
	n = snprintf(buf, sizeof(buf), "\x1b[1;%dH\x1b[1;37;44m%s\x1b[0m", pad + 1, line);
	emit_all(buf, n);
	g_label_until = now_ms() + 900;
	dlog("video cycle -> %s", v->label);
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
	     "(%.0f/frame avg) | %.1f KB/s avg | %u deduped | lag~%ums (min %ums) depth=%d%s",
	     mode_name(g_mode), s_pxW, s_pxH, g_tx_frames, secs, g_tx_frames / secs,
	     (unsigned long long)g_tx_bytes, (double)g_tx_bytes / g_tx_frames,
	     g_tx_bytes / secs / 1024.0, g_dedup_skipped, g_rtt_ms, g_rtt_min, max_inflight(),
	     g_inflight_auto ? " (auto)" : "");
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
	// Also measure the REAL terminal size: park the cursor in the far corner
	// (terminals clamp the move to the actual extent), then read it back with DSR
	// -- so a stale -l (%R) or terminal.ini, which a mid-session client resize
	// never updates, can't size us wrong. ESC 7 / ESC 8 save+restore the cursor.
	static const char q[] = "\x1b[14t\x1b[16t\x1b[?2;1S\x1b[?u\x1b" "7\x1b[999;999H\x1b[6n";
	// (\x1b[?u above queries kitty keyboard-protocol support; a CSI?<flags>u reply -> parse_byte
	//  enables it and pushes our flags. The reply arrives before the \x1b[6n DSR that ends this loop.)
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
					else if (marker == 0 && ch == 'R') {
						if (np >= 2 && p[0] > 0 && p[1] > 0) {   // CPR from the far corner = real size
							g_meas_rows = p[0];
							g_meas_cols = p[1];
						}
						done = 1;                                            // DSR sentinel
					}
					break;
				}
			}
		}
	}
	emit_all("\x1b" "8", 2);             // restore the cursor we parked in the far corner

	// Prefer the freshly-measured size over the possibly-stale -l (%R) and
	// terminal.ini cols/rows (a mid-session client resize updates neither). The
	// bounds reject a non-clamping terminal's bogus 999;999-style reply.
	if (g_meas_rows >= 2 && g_meas_rows <= 200)
		s_lines = g_meas_rows;
	if (g_meas_cols >= 10 && g_meas_cols <= 600)
		g_cols = g_meas_cols;

	// Only the cell size came back? Derive the window from cols/rows * cell.
	if (g_win_w == 0 && g_cell_w > 0) {
		g_win_w = g_cols * g_cell_w;
		g_win_h = s_lines * g_cell_h;
	}
}

static int resolve_music_cache_dir(char *buf, size_t sz);   // defined below (needs g_wads_dir)
void DG_Init(void)
{
	raw_input_on();
	atexit(raw_input_off);
	atexit(terminal_restore);
	atexit(emit_telemetry);
	if (g_sock)
		tune_socket();
	g_start_ms = now_ms();
	g_tx_progress_ms = g_start_ms;   // seed the output-stall (dead-client) timer
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

	// F4 cycle: when JXL is the chosen tier, also learn whether the terminal speaks
	// sixel so the player can A/B JXL vs sixel with F4. The JXL ladder short-circuits
	// before probe_sixel, so probe it here -- a JXL-capable SyncTERM answers the sixel
	// CTDA query quickly (skip when sixel is force-disabled via -sixel 0).
	g_have_sixel = (g_mode == MODE_SIXEL);
#ifdef WITH_JXL
	if (g_mode == MODE_JXL && g_sixel_pref != 0)
		g_have_sixel = probe_sixel();
#endif

	compute_geometry();              // emitted image size + centering (now that the tier is known)

	// Terminal audio: create the SyncTERM audio-APC manager and fire the
	// libsndfile capability probe. pump_input parses the reply over the first
	// frames; until then -- and on terminals without the audio APC -- the
	// manager stays at tier -1 and every SFX is a silent no-op.
	sd_audio = termgfx_audio_create(sd_audio_emit, NULL);
	termgfx_audio_set_cache_prefix(sd_audio, "syncdoom");   // SyncTERM cache: syncdoom/music|sfx/..
	termgfx_audio_probe(sd_audio);
	// Door-side music cache: shared OGG transcodes -> the MIDI->OPL render happens once
	// globally; every later play (any session / door relaunch) ships the cached OGG with
	// no re-render. resolve_music_cache_dir() prefers the BBS data dir (Synchronet's
	// SBBSDATA) and falls back to the WADs dir on a non-Synchronet host; absent/unwritable
	// -> render-only (graceful). Content-addressed names keep WAD sets from colliding.
	{
		char mcdir[PATH_MAX];

		if (resolve_music_cache_dir(mcdir, sizeof(mcdir))) {
			mkpath(mcdir);
			termgfx_audio_set_music_cache_dir(sd_audio, mcdir);
		}
	}
	out_flush();                     // push the probe query now

	build_video_states();            // seed the F4 cycle now so the startup tier (incl.
	                                 // graphics) is always one of the cyclable states
	joybspeed = g_always_run ? 31 : 2;   // apply always-run default ('R' toggles at runtime;
	                                     // 31 >= MAX_JOY_BUTTONS, the always-run hack)

	apply_sixel_scroll();                // disable DECSDM scrolling if we start in the sixel tier

	// Enable xterm mouse reporting (any-event tracking + SGR coordinates) when a
	// mouse model is selected. Harmless on terminals that don't support it -- they
	// ignore the private-mode set and never send reports. SyncTERM implements it.
	if (g_mouse_mode != MOUSE_OFF) {
		g_mouse_enabled = 1;
		emit_all("\x1b[?1003h\x1b[?1006h", 16);
	}

	// Startup diagnostic -> BBS log (syslog/journalctl). Confirms the running
	// build, which terminal.ini (if any) was read, the size taken from it, and
	// the resulting emitted-image geometry + tier.
	dlog("build %s (%s) | term=%s (%s) | cols=%d rows=%d desc=\"%s\" cterm=%d"
	     " | win=%dx%d cell=%dx%d | image %dx%d @%d,%d scale=%s mode=%s"
#ifdef WITH_JXL
	     " jxl_dist=%.1f"
#endif
	     ,
	     GIT_HASH, GIT_DATE,
	     g_diag_term, g_diag_term_ok ? "read" : "NOT FOUND",
	     g_cols, s_lines, g_diag_desc, g_cterm_version, g_win_w, g_win_h, g_cell_w, g_cell_h,
	     s_pxW, s_pxH, g_img_x, g_img_y, g_scale_fit ? "fit" : "off", mode_name(g_mode)
#ifdef WITH_JXL
	     , g_jxl_distance
#endif
	     );
}

void DG_DrawFrame(void)
{
	int h = s_pxH;                // emitted dims (may exceed 640x400 when scaling up)
	int w = s_pxW;
	pump_input();                 // service input first, every tick
	out_flush();                  // keep draining any in-flight frame
	if (sd_audio != NULL && termgfx_audio_music_async_poll(sd_audio) == TERMGFX_MUSIC_ASYNC_SHIPPED)
		dlog("music: async render ready -> shipped");   // a cold-miss track finished on the worker
	{
		// Frame-stall watchdog: emit_frame drops fast when the pipe isn't drained, so a long one
		// is a slow encode.  (An audio transcode/upload blocks elsewhere and logs render=/xfer=.)
		uint32_t fs = now_ms();
		emit_frame(DG_ScreenBuffer, w, h);   // builds+sends only if drained, else drops
		fs = now_ms() - fs;
		if (fs >= FRAME_STALL_MS)
			dlog("pace: frame stall %ums (inflight=%d depth=%d)", (unsigned)fs,
			     dsr_inflight(), g_inflight_auto ? g_auto_depth : g_max_inflight);
	}
	pump_input();
	check_hangup();
	check_timelimit();
}

void DG_SleepMs(uint32_t ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	struct timespec ts;
	ts.tv_sec  = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
#endif
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

// Absolute path of the configured [wads] dir (same value exported as DOOMWADDIR),
// captured by read_syncdoom_ini() for wadcopy() below. Empty until then.
static char g_wads_dir[PATH_MAX] = "";

// Where to keep the door-side transcoded-audio cache (rendered MUS/MIDI -> OGG).
// Prefer the BBS data dir -- Synchronet exports SBBSDATA = the SCFG-configured
// data_dir to every door, so we honour a relocated data_dir without hardcoding.
// Fall back to the WADs dir on a non-Synchronet host (writable + persistent, and
// where the content already lives). Returns 0 if neither is known -> render-only.
static int resolve_music_cache_dir(char *buf, size_t sz)
{
	const char *data = getenv("SBBSDATA");

	if (data != NULL && *data) {
		size_t      n   = strlen(data);
		const char *sep = (n && (data[n - 1] == '/' || data[n - 1] == '\\')) ? "" : "/";

		snprintf(buf, sz, "%s%ssyncdoom/audio", data, sep);
		return 1;
	}
	if (g_wads_dir[0]) {                      // non-Synchronet: alongside the WADs
		snprintf(buf, sz, "%s/audio", g_wads_dir);
		return 1;
	}
	return 0;
}

// Resolve a (possibly relative) path to absolute -- called BEFORE the chdir so
// -iwad/-file still load afterwards.
static char *abscopy(const char *p)
{
	char buf[PATH_MAX], cwd[PATH_MAX];
#ifdef _WIN32
	if (p[0] == '/' || p[0] == '\\' || (p[0] && p[1] == ':'))
		return strdup(p);                                   // already absolute
	if (_fullpath(buf, p, sizeof(buf)) != NULL)
		return strdup(buf);
	if (_getcwd(cwd, sizeof(cwd)) != NULL) {
		snprintf(buf, sizeof(buf), "%s\\%s", cwd, p);
		return strdup(buf);
	}
#else
	if (p[0] == '/')
		return strdup(p);
	if (realpath(p, buf) != NULL)
		return strdup(buf);
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		snprintf(buf, sizeof(buf), "%s/%s", cwd, p);
		return strdup(buf);
	}
#endif
	return strdup(p);
}

// Resolve a -iwad/-file/-merge value to an absolute path BEFORE we chdir into the
// per-user sandbox. The lobby passes absolute WAD paths (kept as-is). A bare or
// relative WAD name -- e.g. the direct-exec install's "-iwad freedoom1.wad" -- is
// resolved against the configured [wads] dir when the WAD is found there, so it
// locates the file in the SAME place the lobby does (and matches DOOMWADDIR),
// rather than against the door's CWD. Falls back to CWD-relative (abscopy).
//
// fexistcase() (xpdev dirwrap) resolves CASE-INSENSITIVELY and rewrites buf to the
// real on-disk case: a DOS WAD named DOOM2.WAD still satisfies a configured
// "doom2.wad" on a case-sensitive (Linux) filesystem. It's a no-op correction on
// case-insensitive (Windows) filesystems.
static char *wadcopy(const char *p)
{
	char buf[PATH_MAX];
	int  absolute =
#ifdef _WIN32
		(p[0] == '/' || p[0] == '\\' || (p[0] && p[1] == ':'));
#else
		(p[0] == '/');
#endif
	if (!absolute && g_wads_dir[0]) {
		snprintf(buf, sizeof(buf), "%s/%s", g_wads_dir, p);
		if (fexistcase(buf))
			return abscopy(buf);
	}
	// The path as given (absolute, or CWD-relative): case-correct it too.
	snprintf(buf, sizeof(buf), "%s", p);
	if (fexistcase(buf))
		return abscopy(buf);
	return abscopy(p);
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
	// Charset drives the default text tier (UTF-8 -> blocks+shades, else half). Assumes
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

// External waiting-room splash, so a sysop can edit the art without a rebuild. An
// 80x25 raw "binary text" file -- one {CP437 char, CGA attribute} pair per cell,
// row-major (4000 bytes; the format PabloDraw/Moebius/TheDraw save as .bin, and
// the same the door renders). Any trailing SAUCE record is ignored. Falls back to
// the baked-in sd_splash[] when the file is absent or short.
static unsigned char g_splash_cells[80 * 25 * 2];
static int           g_have_splash = 0;
static char          g_door_dir[PATH_MAX];            // the door's own dir (trailing sep)
static char          g_splash_cfg[INI_MAX_VALUE_LEN]; // [game] splash override, if any

static void load_splash(const char *path)
{
	FILE * f = fopen(path, "rb");
	size_t rd;

	if (f == NULL)
		return;
	memset(g_splash_cells, 0, sizeof(g_splash_cells));   // unfilled cells -> black
	rd = fread(g_splash_cells, 1, sizeof(g_splash_cells), f);
	fclose(f);
	// Accept any non-empty file: a full 80x25 fills the screen; a shorter one
	// (e.g. PabloDraw's 80x23) fills the top rows and the rest stays black; a file
	// with a trailing SAUCE record just has the extra bytes ignored.
	if (rd >= 2)
		g_have_splash = 1;
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
	snprintf(g_door_dir, sizeof(g_door_dir), "%s", (base != dir) ? dir : "");  // for main()'s splash load

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
	g_text_max_cols = iniGetInteger(ini, "video", "text_max_cols", g_text_max_cols);
	g_text_max_rows = iniGetInteger(ini, "video", "text_max_rows", g_text_max_rows);

	// frames_in_flight -- DSR frame-pacing pipeline depth. 1 (default) paces
	// strictly to the terminal (best for a local SyncTERM); higher lifts the frame
	// rate over a high-latency (remote) link. "auto" adapts the depth to the
	// measured round-trip time. Also settable live in-game via Ctrl-T.
	iniGetString(ini, "video", "frames_in_flight", "", val);
	if (val[0])
		set_frames_in_flight(val);

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

#ifdef WITH_JXL
	// jxl_distance -- JXL lossy distance, the frame-size lever (higher = smaller
	// + softer, at no encode-time cost). Encode effort is fixed at 1 (see globals).
	g_jxl_distance = (float)iniGetFloat(ini, "video", "jxl_distance", g_jxl_distance);
#endif

	// cell_width / cell_height -- the terminal's character-cell size in pixels.
	// Only needed for "scale = fit" on a terminal that doesn't answer the pixel
	// probes (ESC[14t/16t/XTSMGRAPHICS) -- e.g. xterm without allowWindowOps. The
	// fit viewport is then cols*cell_width x rows*cell_height. 0 = auto-estimate.
	g_cfg_cell_w = iniGetInteger(ini, "video", "cell_width", g_cfg_cell_w);
	g_cfg_cell_h = iniGetInteger(ini, "video", "cell_height", g_cfg_cell_h);

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
	g_always_run = iniGetBool(ini, "input", "always_run", TRUE);
	// instant_turn -- house default for FAST TURN (defeat the turn-accel ramp).
	// Default ON: the ramp fights terminal key-repeat (laggy turning); off = the
	// vanilla ramp. A per-user in-game toggle overrides this.
	sd_instant_turn = iniGetBool(ini, "input", "instant_turn", TRUE);

	// [input] mouse -- terminal mouse steering on (default) or off. On a mouse-
	// capable client (SyncTERM, xterm-family) the pointer's offset from center
	// turns the player and the left button fires; inert on terminals without it.
	iniGetString(ini, "input", "mouse", "", val);
	if (stricmp(val, "off") == 0 || stricmp(val, "none") == 0 ||
	    stricmp(val, "false") == 0 || strcmp(val, "0") == 0)
		g_mouse_mode = MOUSE_OFF;
	else if (val[0])                          // on/true/yes/anything non-empty -> on
		g_mouse_mode = MOUSE_ON;
	// empty -> keep the built-in default (on)

	// [game] quit_effect -- fate of a departed player's marine: keep (vanilla,
	// stays frozen), vanish (remove at once), or fog (teleport-out puff + sound,
	// default). Changes lockstep game state, so it must match across players in a
	// match -- a house setting, not a per-user toggle.
	iniGetString(ini, "game", "quit_effect", "", val);
	if (stricmp(val, "keep") == 0)
		sd_quit_effect = 0;
	else if (stricmp(val, "vanish") == 0)
		sd_quit_effect = 1;
	else
		sd_quit_effect = 2;          // "fog" or unset -> default

	// [game] splash -- external waiting-room art (80x25 .bin; editable, no rebuild).
	// Just captured here; resolved + loaded in main() so it works with NO ini too
	// (this function returns early when syncdoom.ini is absent).
	iniGetString(ini, "game", "splash", "", g_splash_cfg);

	// [wads] dir -- point Doom's IWAD search at the configured WAD directory so a
	// bare "-iwad freedoom1.wad" (e.g. the direct-exec install) resolves there,
	// the same dir the lobby uses. Resolve to ABSOLUTE (we chdir into -home before
	// the WAD is opened, so a relative dir would break) and export DOOMWADDIR.
	// Blank = the door's own dir; a relative dir is under it; absolute used as-is.
	iniGetString(ini, "wads", "dir", "", val);
	{
		char        wads[PATH_MAX], abs[PATH_MAX];
		const char *resolved;
		if (val[0] == '/' || val[0] == '\\' || (val[0] && val[1] == ':'))
			snprintf(wads, sizeof(wads), "%s", val);                 // absolute
		else if (val[0])
			snprintf(wads, sizeof(wads), "%s%s", dir, val);          // under the door dir
		else
			snprintf(wads, sizeof(wads), "%s", dir[0] ? dir : ".");  // the door dir itself
#ifndef _WIN32
		resolved = realpath(wads, abs) ? abs : wads;
		setenv("DOOMWADDIR", resolved, 1);
#else
		resolved = _fullpath(abs, wads, sizeof(abs)) ? abs : wads;
		_putenv_s("DOOMWADDIR", resolved);
#endif
		// Keep the absolute wads dir for wadcopy(): a bare "-iwad <name>" resolves
		// here (matching DOOMWADDIR + the lobby), not against the door's CWD.
		strncpy(g_wads_dir, resolved, sizeof(g_wads_dir) - 1);
		g_wads_dir[sizeof(g_wads_dir) - 1] = '\0';
	}

	strListFree(&ini);
}

// Per-user preferences: a sectioned syncdoom.ini in the user's -home dir,
// mirroring the house-default syncdoom.ini (beside the exe) but holding only this
// user's overrides -- the in-game Options sliders ([input] kp* graces) and the
// Ctrl-T frame-pacing depth ([video] frames_in_flight). Saved on change, loaded
// after the CLI scan; precedence: built-in -> house ini -> per-user -> CLI. No-op
// without -home.

// Build g_user_ini_path = <-home>/syncdoom.ini (once). Safe to call repeatedly.
static void user_ini_path(void)
{
	size_t n;

	if (g_user_ini_path[0] != '\0' || g_home[0] == '\0')
		return;
	n = strlen(g_home);
	snprintf(g_user_ini_path, sizeof(g_user_ini_path), "%s%ssyncdoom.ini",
	         g_home, (n > 0 && (g_home[n - 1] == '/' || g_home[n - 1] == '\\')) ? "" : "/");
}

// Save a pref only if it differs from the captured default; otherwise REMOVE the
// key, so an untouched setting stays out of the per-user file and keeps following
// the sysop's house default (even for a player who has already played).
static void save_pref_int(str_list_t *list, const char *sec, const char *key, int val, int def)
{
	if (val == def)
		iniRemoveKey(list, sec, key);
	else
		iniSetInteger(list, sec, key, val, NULL);
}

static void save_pref_bool(str_list_t *list, const char *sec, const char *key, int val, int def)
{
	if (!val == !def)
		iniRemoveKey(list, sec, key);
	else
		iniSetBool(list, sec, key, val, NULL);
}

static void save_pref_str(str_list_t *list, const char *sec, const char *key,
                          const char *val, const char *def)
{
	if (strcmp(val, def) == 0)
		iniRemoveKey(list, sec, key);
	else
		iniSetString(list, sec, key, val, NULL);
}

static const char *mouse_mode_str(int m)
{
	return m == MOUSE_OFF ? "off" : "on";
}

// Write the user's prefs. Called by the Options sliders, the Ctrl-T pacing toggle,
// and the Ctrl-O mouse toggle after each change. Only settings the player has
// CHANGED from the default are written (the rest are skipped, see save_pref_*
// above) so house-ini defaults still reach this user.
//
// We write the file FRESH each time rather than read-modify-write. RMW tripped an
// xpdev empty-section bug: once "remove key == default" emptied the [input]
// section (leaving its bare header above [video]), iniSetString misfiled every
// later [input] key at end-of-file under [video], where it was unreadable -- so
// per-user input prefs silently stopped saving/loading and accumulated duplicates.
// A fresh list builds each section in order, so the bug can't trigger; it also
// self-heals an already-corrupted file. (These are generated prefs in data/, not
// hand-edited, so there's nothing to preserve by reading the old file.)
void sd_save_user_prefs(void)
{
	str_list_t list;
	FILE *     f;

	user_ini_path();
	if (g_user_ini_path[0] == '\0')
		return;                          // no -home: nowhere to persist
	list = strListInit();
	save_pref_int (&list, "input", "kpdelay",  (int)g_grace_fresh,   (int)g_base_grace_fresh);
	save_pref_int (&list, "input", "kpsmooth", (int)g_keyup_idle_ms, (int)g_base_keyup_idle_ms);
	save_pref_int (&list, "input", "kpturn",   (int)g_turn_grace,    (int)g_base_turn_grace);
	save_pref_bool(&list, "input", "instant_turn", sd_instant_turn,  g_base_instant_turn);
	save_pref_str (&list, "input", "mouse", mouse_mode_str(g_mouse_mode), mouse_mode_str(g_base_mouse_mode));
	save_pref_str (&list, "video", "frames_in_flight", frames_in_flight_str(), g_base_frames);
	save_pref_bool(&list, "video", "sixel_fullres", g_sixel_fullres, 0);
	f = fopen(g_user_ini_path, "w");
	if (f != NULL) {
		iniWriteFile(f, list);
		fclose(f);
	}
	strListFree(&list);
}

// Apply per-user prefs over the house-ini/built-in defaults. A grace pinned by a
// -kp* CLI flag is left alone (CLI wins). Call after the CLI scan.
static void load_user_prefs(void)
{
	FILE *     f;
	str_list_t ini;
	char       val[INI_MAX_VALUE_LEN];

	user_ini_path();                     // also sets the path so a later save works
	if (g_user_ini_path[0] == '\0')
		return;
	f = fopen(g_user_ini_path, "r");
	if (f == NULL)
		return;                          // none saved yet -> keep house ini/built-in
	ini = iniReadFile(f);
	fclose(f);
	if (!g_kp_cli_tap)
		g_grace_fresh   = (uint32_t)iniGetInteger(ini, "input", "kpdelay",  (int)g_grace_fresh);
	if (!g_kp_cli_hold)
		g_keyup_idle_ms = (uint32_t)iniGetInteger(ini, "input", "kpsmooth", (int)g_keyup_idle_ms);
	if (!g_kp_cli_turn)
		g_turn_grace    = (uint32_t)iniGetInteger(ini, "input", "kpturn",   (int)g_turn_grace);
	sd_instant_turn = iniGetBool(ini, "input", "instant_turn", sd_instant_turn);
	if (!g_mouse_cli) {
		iniGetString(ini, "input", "mouse", "", val);
		if (stricmp(val, "off") == 0 || stricmp(val, "none") == 0 ||
		    stricmp(val, "false") == 0 || strcmp(val, "0") == 0)
			g_mouse_mode = MOUSE_OFF;
		else if (val[0])
			g_mouse_mode = MOUSE_ON;
	}
	iniGetString(ini, "video", "frames_in_flight", "", val);
	if (val[0])
		set_frames_in_flight(val);
	g_sixel_fullres = iniGetBool(ini, "video", "sixel_fullres", g_sixel_fullres);
	strListFree(&ini);
}

// Ctrl-T: cycle the frame-pipeline depth (1..DEPTH_MAX -> auto -> 1) live, persist
// it per-user, and flash a label showing the depth + measured lag -- so a far-away
// player can A/B the remote-latency pacing and feel the difference immediately.
static void toggle_pipeline(void)
{
	char line[80], buf[160];
	int  pad, n;

	if (g_inflight_auto)
		g_inflight_auto = 0, g_max_inflight = 1;                               // auto -> 1
	else if (g_max_inflight >= DEPTH_MAX)
		g_inflight_auto = 1;                                                   // DEPTH_MAX -> auto
	else
		g_max_inflight++;                                                      // 1->2 ... ->DEPTH_MAX
	sd_save_user_prefs();

	// Skip the centered popup when the stats overlay is already up -- it shows the
	// live depth/lag, so the label would just be redundant (and obscure the game).
	if (!g_stats_overlay) {
		if (g_inflight_auto)
			snprintf(line, sizeof(line), "  DEPTH: auto  (%d, lag ~%ums)  ",
			         max_inflight(), g_rtt_ms);
		else
			snprintf(line, sizeof(line), "  DEPTH: %d  (lag ~%ums)  ", g_max_inflight, g_rtt_ms);
		pad = (overlay_cols() - (int)strlen(line)) / 2;
		if (pad < 0)
			pad = 0;
		n = snprintf(buf, sizeof(buf), "\x1b[1;%dH\x1b[1;37;44m%s\x1b[0m", pad + 1, line);
		emit_all(buf, n);
		g_label_until = now_ms() + 1200;
	}
	dlog("pipeline toggle -> frames_in_flight=%s lag~%ums", frames_in_flight_str(), g_rtt_ms);
}

// Ctrl-S: toggle the live stats overlay (a top-row strip drawn over each frame --
// fps/RTT/depth). Session-only (not persisted; it's a diagnostic, default off). A
// brief centered label confirms the toggle.
static void toggle_stats(void)
{
	char line[48], buf[160];
	int  pad, n;

	g_stats_overlay = !g_stats_overlay;
	snprintf(line, sizeof(line), "  STATS %s  ", g_stats_overlay ? "ON" : "OFF");
	pad = (overlay_cols() - (int)strlen(line)) / 2;
	if (pad < 0)
		pad = 0;
	n = snprintf(buf, sizeof(buf), "\x1b[1;%dH\x1b[1;37;44m%s\x1b[0m", pad + 1, line);
	emit_all(buf, n);
	g_label_until = now_ms() + 700;
	dlog("stats overlay -> %s", g_stats_overlay ? "on" : "off");
}

// Ctrl-O: toggle terminal mouse steering on/off live, persisted per-user. Flips the
// xterm tracking modes to match; when turning off it also drops any held button and
// the last pointer offset so the player stops cleanly (no residual turn/fire). A
// brief centered label confirms it. (Inert flag-flip on a non-mouse terminal.)
static void toggle_mouse(void)
{
	extern void sd_hud_message(const char *msg);   // g_game.c: Doom's in-game message widget

	if (g_mouse_mode == MOUSE_OFF) {
		g_mouse_mode    = MOUSE_ON;
		g_mouse_enabled = 1;
		emit_all("\x1b[?1003h\x1b[?1006h", 16);
	} else {
		g_mouse_mode    = MOUSE_OFF;
		g_mouse_enabled = 0;
		g_mouse_buttons = 0;
		g_mouse_have    = 0;
		emit_all("\x1b[?1003l\x1b[?1006l", 16);
	}
	sd_save_user_prefs();
	// Confirm via Doom's own HUD message (game font, no frame pause) rather than a terminal overlay,
	// so it matches "you got the shotgun" and doesn't stall the graphics like the old label popup.
	sd_hud_message(g_mouse_mode == MOUSE_OFF ? "MOUSE OFF" : "MOUSE ON");
	dlog("mouse steering -> %s", g_mouse_mode == MOUSE_OFF ? "off" : "on");
}

// Compute the emitted bitmap dimensions (s_pxW x s_pxH) from the terminal's
// cell-pixel viewport and the scale config. Doom's frame is 8:5 (640:400); we fit
// the largest 8:5 rectangle into the viewport, capped at g_scale_max width.
static void compute_geometry(void)
{
	int vw, vh, cw, ch, fitvh;

	g_last_fb_ok = 0;                   // geometry changed: same framebuffer now renders differently
	// Did we actually MEASURE the window, or are we guessing from cols*rows?
	// True only if the terminal answered the pixel probe, or the sysop pinned a
	// cell size in syncdoom.ini. When false the viewport below is an estimate.
	int geom_known = (g_win_w > 0) || (g_cfg_cell_w > 0 && g_cfg_cell_h > 0);

	if (g_win_w > 0) {                  // real terminal pixels (probed) -- preferred
		vw = g_win_w;
		vh = g_win_h;
	} else {                            // fallback: no real pixels probed -- estimate
		// Prefer an explicit syncdoom.ini cell size; else a SyncTERM canvas maps
		// rows to a glyph height, and a generic terminal that didn't answer the
		// probes gets a normal ~8x16 cell (cell_height() returns 8 for >30 rows,
		// badly under-sizing it). Set [video] cell_width/cell_height to fit a
		// terminal that hides its geometry (e.g. xterm without allowWindowOps).
		int cellw = g_cfg_cell_w > 0 ? g_cfg_cell_w : 8;
		int cellh = g_cfg_cell_h > 0 ? g_cfg_cell_h
		            : (g_is_syncterm ? cell_height(s_lines) : 16);

		vw = g_cols * cellw;
		vh = s_lines * cellh;
	}
	cw = g_cell_w > 0 ? g_cell_w
	     : (g_cfg_cell_w > 0 ? g_cfg_cell_w : 8);       // cell pixels, for centering math
	ch = g_cell_h > 0 ? g_cell_h
	     : (g_cfg_cell_h > 0 ? g_cfg_cell_h
	        : (g_is_syncterm ? cell_height(s_lines) : 16));

	fitvh = vh;                         // height the image is fit+centered into; the sixel-fill
	                                    // branch reserves a bottom row below (see there)
	if (g_mode == MODE_PPM) {           // PPM is uncompressed, so emit Doom's native
		s_pxW = 320;                    // 320x200 -- the 640x400 framebuffer is a lossless
		s_pxH = 200;                    // 2x of it, so this is ~1/4 the bytes (and centered,
	} else if (!g_scale_fit) {          // drawn small, since SyncTERM can't scale on draw).
		s_pxW = 640;                    // native: doomgeneric's 640x400 (vh-squished if shorter)
		s_pxH = (vh < 400) ? vh : 400;
	} else {
		// Sixel fill: scale up to fill the window, capped at 1024 wide (matching SyncDuke)
		// instead of the old hard 640. Sixel is near-raw RLE so a window-filling frame is a
		// big payload, but the de-dupe + DSR pacing handle the rate, and the bottom-row
		// reserve below keeps the terminal from mis-rendering a sixel that reaches its LAST
		// text row -- Windows Terminal (and others that ignore ?80l) scroll white lines in
		// below such a sixel. The old hard 640 cap sidestepped that only by never filling.
		// JXL/PPM keep the full g_scale_max (real compression / drawn small via APC -- they
		// position by pixel offset, not a text cell, so they have no last-row quirk).
		int cap  = (g_mode == MODE_SIXEL && (g_scale_max == 0 || g_scale_max > 1024))
		           ? 1024 : g_scale_max;
		// Reserve ONE cell row at the bottom for the sixel tier so the image never reaches the
		// last text row; centering within fitvh (below) then lands that row at the very bottom.
		if (g_mode == MODE_SIXEL && vh > ch * 4)
			fitvh = vh - ch;
		// Fit Doom's native 640x400 into the canvas, capped, aspect-preserving, with the
		// <=8% letterbox stretch -- shared with SyncDuke (termgfx/geometry.c).
		termgfx_geom_fit(vw, fitvh, 640, 400, cap, &s_pxW, &s_pxH);
	}

	// Center the image in the window: pixel offset for the APC DX/DY (SyncTERM
	// JXL/PPM), cell offset for the sixel text cursor -- shared with SyncDuke.
	termgfx_geom_center(vw, fitvh, s_pxW, s_pxH, cw, ch,
	                    &g_img_x, &g_img_y, &g_img_col, &g_img_row);

	// Sixel is positioned by TEXT CELL, but the image is sized in pixels. Without
	// the terminal's real cell-pixel size (geom_known) the centered cell is a
	// guess -- the assumed 16px cell height rarely matches (xterm's is shorter),
	// so the centered row lands too low and the image looks bottom-anchored.
	// Anchor it top-left instead: predictable, and what a user expects. JXL/PPM
	// (real pixel geometry, centered via the APC DX/DY above) are unaffected.
	if (g_mode == MODE_SIXEL && !geom_known) {
		g_img_col = 1;
		g_img_row = 1;
	}
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Multiplayer waiting room. Replaces the headless NET_WaitForLaunch (net_gui.c
// calls sd_waitroom_run): draws the joined players by name over the client
// socket while the match fills, and lets the host start it (or anyone cancel)
// before the game begins.
// ---------------------------------------------------------------------------

// Network player names by player number (filled from the waiting data, which
// the server indexes by player number). Empty slot -> hu_stuff falls back to
// the color name. See sd_player_chat_name().
static char sd_chat_player_names[NET_MAXPLAYERS][MAXPLAYERNAME];

// Raw network name for player number 'player', or NULL if unknown (e.g. the
// "<name> left the game" message). Used by d_net.c.
const char *sd_player_name(int player)
{
	if (player >= 0 && player < NET_MAXPLAYERS
	    && sd_chat_player_names[player][0] != '\0')
		return sd_chat_player_names[player];
	return NULL;
}

// Called by hu_stuff.c: the chat prefix ("Name: ") for player number 'player',
// or NULL to use Doom's color name.
const char *sd_player_chat_name(int player)
{
	static char buf[MAXPLAYERNAME + 4];
	const char *nm = sd_player_name(player);

	if (nm != NULL) {
		snprintf(buf, sizeof(buf), "%s: ", nm);
		return buf;
	}
	return NULL;
}

// Render an 80x25 "ENDOOM-format" screen -- cells of {CP437 char, CGA attribute}
// (the embedded sd_splash[]) -- to the terminal as ANSI, the waiting-room
// backdrop. CGA attribute -> SGR (fg/bg via the CGA->ANSI color swap, bit3 =
// bright fg, bit7 = blink); the char is emitted raw on a CP437 terminal, or as
// UTF-8 (cp437_unicode_tbl) on a UTF-8 one. Capped to the terminal's line count
// so a <25-row terminal doesn't scroll.
static void sd_render_screen(const unsigned char *e)
{
	static const int cga2ansi[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };  // CGA b/g/r -> ANSI r/g/b
	int              rows, row, col;

	rows = (s_lines < 25) ? s_lines : 25;
	for (row = 0; row < rows; row++) {
		char rb[2048];
		int  rl = 0, last_attr = -1;
		rl += snprintf(rb + rl, sizeof(rb) - rl, "\x1b[%d;1H", row + 1);
		for (col = 0; col < 80; col++) {
			uint8_t ch   = e[(row * 80 + col) * 2];
			uint8_t attr = e[(row * 80 + col) * 2 + 1];
			if (attr != last_attr) {
				rl += snprintf(rb + rl, sizeof(rb) - rl, "\x1b[0%s%s;3%d;4%dm",
				               (attr & 8) ? ";1" : "", (attr & 0x80) ? ";5" : "",
				               cga2ansi[attr & 7], cga2ansi[(attr >> 4) & 7]);
				last_attr = attr;
			}
			if (g_rt_charset == RT_UTF8) {
				uint32_t cp = (uint32_t)cp437_unicode_tbl[ch];
				if (cp == 0)
					cp = ch;
				if (cp < 0x80) {
					rb[rl++] = (char)cp;
				} else if (cp < 0x800) {
					rb[rl++] = (char)(0xC0 | (cp >> 6));
					rb[rl++] = (char)(0x80 | (cp & 0x3F));
				} else {
					rb[rl++] = (char)(0xE0 | (cp >> 12));
					rb[rl++] = (char)(0x80 | ((cp >> 6) & 0x3F));
					rb[rl++] = (char)(0x80 | (cp & 0x3F));
				}
			} else {                     // raw CP437; map true control codes to space
				rb[rl++] = (ch == 0 || ch == 7 || ch == 8 || ch == 9 || ch == 10
				            || ch == 12 || ch == 13 || ch == 27) ? ' ' : (char)ch;
			}
		}
		rl += snprintf(rb + rl, sizeof(rb) - rl, "\x1b[0m");
		emit_all(rb, (size_t)rl);
	}
}

static void sd_waitroom_draw(void)
{
	char            buf[2048];
	int             len = 0, i, r;
	net_waitdata_t *w = &net_client_wait_data;
	int             disp = w->num_players;
	int             twoline, panelrows, bottom, top, prow;
	int             hint = sbbs_node_available();   // show the Ctrl-U/Ctrl-P hint row

	if (disp < 1)
		disp = 1;
	if (disp > NET_MAXPLAYERS)
		disp = NET_MAXPLAYERS;
	// A lone controller gets a two-line waiting/solo hint; everyone else a single
	// prompt row. List one player PER ROW so a long alias (up to 25 chars, x4)
	// can't overrun 80 cols; the panel grows upward over the splash as players
	// join (4 rows at 1-2 players .. 6 at a full 4).
	twoline   = (w->is_controller && w->num_players < 2 && w->max_players > 1);
	panelrows = 1 /*title*/ + disp + (twoline ? 2 : 1) + (hint ? 1 : 0);
	bottom    = (s_lines < 25) ? s_lines : 25;
	top       = bottom - (panelrows - 1);
	if (top < 1)
		top = 1;

	emit_all("\x1b[2J", 4);
	sd_render_screen(g_have_splash ? g_splash_cells : sd_splash);   // external waiting.bin or baked-in

	for (r = top; r <= bottom; r++)      // solid blue panel (BCE clears each line to bg)
		len += snprintf(buf + len, sizeof(buf) - len, "\x1b[%d;1H\x1b[1;37;44m\x1b[K", r);
	len += snprintf(buf + len, sizeof(buf) - len,
	                "\x1b[%d;3HS y n c \x1b[1;31mD O O M\x1b[1;37m   waiting room   Players %d/%d",
	                top, w->num_players, w->max_players);
	for (i = 0; i < disp; i++)           // one player per row -- no overflow
		len += snprintf(buf + len, sizeof(buf) - len,
		                "\x1b[%d;3H\x1b[1;33m%d.\x1b[1;37m %s%s",
		                top + 1 + i, i + 1, w->player_names[i],
		                (i == w->consoleplayer) ? " \x1b[1;33m(you)\x1b[1;37m" : "");
	prow = top + 1 + disp;               // first prompt row, just below the list
	if (!w->is_controller)
		len += snprintf(buf + len, sizeof(buf) - len,
		                "\x1b[%d;3HWaiting for the host to start...   \x1b[1;33mQ\x1b[1;37m to cancel.",
		                prow);
	else if (twoline) {
		len += snprintf(buf + len, sizeof(buf) - len,
		                "\x1b[%d;3H\x1b[1;33mWaiting for another player...\x1b[1;37m  Auto-starts when full.",
		                prow);
		len += snprintf(buf + len, sizeof(buf) - len,
		                "\x1b[%d;3H\x1b[1;33mQ\x1b[1;37m to cancel.  To play solo, cancel and pick Play single-player.",
		                prow + 1);
	} else
		len += snprintf(buf + len, sizeof(buf) - len,
		                "\x1b[%d;3HPress \x1b[1;33mS\x1b[1;37m or \x1b[1;33mEnter\x1b[1;37m to start now, "
		                "\x1b[1;33mQ\x1b[1;37m to cancel.", prow);
	if (hint)
		len += snprintf(buf + len, sizeof(buf) - len,
		                "\x1b[%d;3H\x1b[1;33mCtrl-U\x1b[1;37m who's online   \x1b[1;33mCtrl-P\x1b[1;37m page a user",
		                prow + (twoline ? 2 : 1));
	len += snprintf(buf + len, sizeof(buf) - len, "\x1b[0m");

	if (len > 0)
		emit_all(buf, (size_t)len);
}

// ---------------------------------------------------------------------------
// Who's-online (Ctrl-U) + paging (Ctrl-P), via sbbs_node.{c,h}. Full-screen text
// drawn straight to the terminal; while reading input these keep pumping the net
// so the waiting-room connection to the dedicated server stays alive.
// ---------------------------------------------------------------------------

static void emit_str(const char *s) { emit_all(s, strlen(s)); }

// Block for one keystroke, pumping the net meanwhile. Returns the byte, -1 hangup.
static int bbs_pump_key(void)
{
	for (;;) {
		unsigned char c;
		ssize_t       n;
		NET_CL_Run();
		NET_SV_Run();
		n = conn_read(&c, 1);
		if (n == 0)
			return -1;
		if (n > 0)
			return (int)c;
		I_Sleep(10);
	}
}

// Read an echoed line into buf. Returns 1 on Enter, 0 on Esc/hangup. Backspace ok.
static int bbs_pump_line(char *buf, int max)
{
	int  len = 0;
	char ec;

	buf[0] = '\0';
	for (;;) {
		int c = bbs_pump_key();
		if (c < 0 || c == 27)
			return 0;
		if (c == '\r' || c == '\n') {
			emit_all("\r\n", 2);
			return 1;
		}
		if (c == 8 || c == 127) {
			if (len > 0) {
				buf[--len] = '\0';
				emit_all("\b \b", 3);
			}
			continue;
		}
		if (c >= 32 && c < 127 && len < max - 1) {
			buf[len++] = (char)c;
			buf[len]   = '\0';
			ec = (char)c;
			emit_all(&ec, 1);
		}
	}
}

// Clear the screen and draw the online-node list. Returns the node count.
static int draw_nodelist(void)
{
	sbbs_node_info_t nodes[64];
	char             alias[26], act[80], line[200];
	int              n, i, me = sbbs_my_node();

	emit_all("\x1b[2J\x1b[H", 7);
	emit_str("\x1b[1;36m  Who's online\x1b[0m\r\n\r\n");
	n = sbbs_list_nodes(nodes, (int)(sizeof(nodes) / sizeof(nodes[0])), 0);
	if (n == 0)
		emit_str("  \x1b[1;30m(nobody else is online)\x1b[0m\r\n");
	for (i = 0; i < n; i++) {
		const char *who = nodes[i].anon ? "UNKNOWN USER"
		                  : sbbs_username(nodes[i].useron, alias, sizeof(alias));
		const char *activity = nodes[i].ext                          // free-text status
		                       ? sbbs_node_ext(nodes[i].number, act, sizeof(act))
		                       : sbbs_action_str(&nodes[i], act, sizeof(act));
		snprintf(line, sizeof(line),
		         "  \x1b[1;33mNode %-2d\x1b[0m %-25s \x1b[36m%s\x1b[0m%s\r\n",
		         nodes[i].number, who, activity,
		         nodes[i].number == me ? "  \x1b[1;32m(you)\x1b[0m" : "");
		emit_str(line);
	}
	return n;
}

// Ctrl-U: show the online list, hold until a key.
static void sd_show_nodelist(void)
{
	if (!sbbs_node_available())
		return;
	draw_nodelist();
	emit_str("\r\n  \x1b[1;30mPress any key to return...\x1b[0m");
	bbs_pump_key();
}

// Ctrl-P: list -> pick a node -> type a message -> send (carrying the bell).
static void sd_page_flow(void)
{
	char numbuf[8], msg[200], line[256];
	int  target, ok;

	if (!sbbs_node_available())
		return;
	draw_nodelist();
	emit_str("\r\n  \x1b[1;37mPage which node #? \x1b[1;30m(Enter cancels)\x1b[0m ");
	if (!bbs_pump_line(numbuf, sizeof(numbuf)) || numbuf[0] == '\0')
		return;
	if ((target = atoi(numbuf)) < 1)
		return;
	emit_str("  \x1b[1;37mMessage: \x1b[0m");
	if (!bbs_pump_line(msg, sizeof(msg)) || msg[0] == '\0')
		return;
	ok = sbbs_page_node(target, sbbs_my_node(), g_player_name, msg);
	snprintf(line, sizeof(line), ok
	         ? "\r\n  \x1b[1;32mPaged node %d.\x1b[0m"
	         : "\r\n  \x1b[1;31mCouldn't page node %d (offline, or paging off).\x1b[0m", target);
	emit_str(line);
	emit_str("\r\n  \x1b[1;30mPress any key to return...\x1b[0m");
	bbs_pump_key();
}

// Poll for an incoming page (throttled ~1/sec) and, if one arrived, display +
// acknowledge it. Returns 1 if it showed something (caller should redraw). Strips
// Synchronet Ctrl-A attribute codes -- the door emits raw, with no BBS attribute
// translation, so they'd otherwise show as garbage.
static int sd_check_pages(void)
{
	static int last = 0;
	char       buf[600], *s, *d;
	int        now, me;

	if (!sbbs_node_available())
		return 0;
	now = I_GetTimeMS();
	if (now - last < 1000)
		return 0;
	last = now;
	me = sbbs_my_node();
	if (me < 1 || sbbs_recv_nmsg(me, buf, sizeof(buf)) <= 0)
		return 0;
	for (s = d = buf; *s; ) {
		if (*s == '\x01') { s += s[1] ? 2 : 1; continue; }   // drop Ctrl-A code
		*d++ = *s++;
	}
	*d = '\0';
	emit_all("\x1b[2J\x1b[H", 7);
	emit_str("\x1b[1;33m  >> You have a page <<\x1b[0m\r\n\r\n  ");
	emit_str(buf);
	emit_str("\r\n\r\n  \x1b[1;30mPress any key to return...\x1b[0m");
	bbs_pump_key();
	return 1;
}

// In-game who's-online: a one-line summary posted as a HUD message -- non-blocking
// and MP-safe (no screen takeover, so the frame pacing is untouched, unlike the
// waiting-room's full-screen sd_show_nodelist).
// --- node.exb free-text who's-online status (waiting room / in game) -------
// Reflect the SyncDOOM sub-state in the node's who's-online line (and Ctrl-U). The
// JS lobby keeps the default "running SyncDOOM"; the door overrides it while the
// player is in the waiting room or actually playing (with the current map name).
extern boolean usergame;        // g_game.c: a real user game is in progress (set at game
                                // start, false at the menu/title and during attract demos)
extern boolean demoplayback;    // g_game.c: true during the title-screen attract demos
extern int     gameepisode, gamemap;
extern int     gamemode;        // GameMode_t: commercial(2) = Doom II / Final Doom (MAPxx)
extern int     gamestate;       // gamestate_t: GS_LEVEL(0) GS_INTERMISSION(1) GS_FINALE(2)
extern int     deathmatch;      // 0=co-op, 1=deathmatch, 2=altdeath
extern int     gameskill;       // skill_t 0..4 (-> displayed skill 1..5)
extern int     leveltime;       // tics elapsed in the current level (/35 = seconds)
extern int     sd_next_frag(int *victim);        // g_game.c: a new frag by us -> victim player #
extern int     sd_check_death(int *cause, int *by); // g_game.c: we just died (cause/by player #)
extern int     sd_total_frags(void);             // g_game.c: our frag total this game

// Push the current in-game status. Cheap to call every tic -- it only writes
// node.exb when the text actually changes (a map change or game start/end).
static void sd_set_game_status(void)
{
	static char last[128] = "";
	char        status[128], map[16];

	if (!sbbs_node_available())
		return;
	// "playing SyncDOOM: <wadset>" plus the current map while in a level. <wadset>
	// is the -wadname the lobby passed (else the -iwad basename); omit if neither.
	const char *sep = g_wadname[0] ? ": " : "";
	if (usergame) {
		if (gamemode == 2)                       // commercial: Doom II / Final Doom
			snprintf(map, sizeof(map), "MAP%02d", gamemap);
		else
			snprintf(map, sizeof(map), "E%dM%d", gameepisode, gamemap);
		snprintf(status, sizeof(status), "playing SyncDOOM%s%s (%s)", sep, g_wadname, map);
	} else {
		snprintf(status, sizeof(status), "playing SyncDOOM%s%s", sep, g_wadname);
	}
	if (strcmp(status, last) != 0) {             // write only on change (no per-tic churn)
		sbbs_node_set_ext(status);
		snprintf(last, sizeof(last), "%s", status);
	}
}

// --- game-event log (jsonl, for the lobby's activity feed + debugging) -----
// The lobby passes -eventlog <data>/syncdoom/events.jsonl; each player's door
// appends one compact JSON line per event (it logs only events the LOCAL player
// is the actor of, so a frag is recorded once -- no central writer, no dedup).
// Append-mode writes of one short line are atomic, and the lobby reads with the
// json_lines "recover" flag, so no locking is needed here.
static char g_eventlog[PATH_MAX] = "";   // -eventlog path; "" = logging disabled

// Minimal JSON string escaper (for aliases / WAD names / TERM strings).
static const char *sd_json_esc(const char *s, char *out, size_t sz)
{
	size_t o = 0;

	if (s == NULL)
		s = "";
	for (; *s && o + 7 < sz; s++) {
		unsigned char c = (unsigned char)*s;
		if (c == '"' || c == '\\') {
			out[o++] = '\\'; out[o++] = (char)c;
		} else if (c < 0x20) {
			o += (size_t)snprintf(out + o, sz - o, "\\u%04x", c);
		} else {
			out[o++] = (char)c;
		}
	}
	out[o] = '\0';
	return out;
}

static void sd_event_emit(const char *json)
{
	FILE *f;
	char  line[640];
	int   n;

	if (g_eventlog[0] == '\0')
		return;
	n = snprintf(line, sizeof(line), "%s\n", json);
	if (n < 1 || (f = fopen(g_eventlog, "ab")) == NULL)
		return;
	fwrite(line, 1, (size_t)n, f);   // single append-mode write -> atomic line
	fclose(f);
}

static const char *sd_event_mode(void)
{
	if (deathmatch)
		return deathmatch >= 2 ? "altdeath" : "deathmatch";
	return netgame ? "co-op" : "single";
}

static void sd_event_map(char *buf, size_t sz)
{
	if (gamemode == 2)
		snprintf(buf, sz, "MAP%02d", gamemap);
	else
		snprintf(buf, sz, "E%dM%d", gameepisode, gamemap);
}

// Once-per-session debug record: terminal, user, node, build, tier, wad, mode.
static void sd_event_start(void)
{
	char        j[640], du[80], dw[80], dd[160];
	const char *u   = strstr(g_home, "user/");
	int         usernum = u ? atoi(u + 5) : 0;

	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"start\",\"node\":%d,\"usernum\":%d,\"user\":\"%s\","
	         "\"term\":{\"desc\":\"%s\",\"cols\":%d,\"rows\":%d,\"cterm\":%d},\"tier\":\"%s\","
	         "\"build\":{\"hash\":\"%s\",\"date\":\"%s\"},\"wad\":\"%s\",\"mode\":\"%s\",\"skill\":%d}",
	         (long)time(NULL), sbbs_my_node(), usernum, sd_json_esc(g_player_name, du, sizeof(du)),
	         sd_json_esc(g_diag_desc, dd, sizeof(dd)), g_cols, s_lines, g_cterm_version,
	         mode_name(g_mode), GIT_HASH, GIT_DATE,
	         sd_json_esc(g_wadname, dw, sizeof(dw)), sd_event_mode(), gameskill + 1);
	sd_event_emit(j);
}

static void sd_event_level(void)
{
	char j[400], du[80], dw[80], map[16];

	sd_event_map(map, sizeof(map));
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"level\",\"node\":%d,\"user\":\"%s\",\"wad\":\"%s\","
	         "\"map\":\"%s\",\"secs\":%d,\"skill\":%d}",
	         (long)time(NULL), sbbs_my_node(), sd_json_esc(g_player_name, du, sizeof(du)),
	         sd_json_esc(g_wadname, dw, sizeof(dw)), map, leveltime / 35, gameskill + 1);
	sd_event_emit(j);
}

static void sd_event_frag(int victim)
{
	char        j[400], dk[80], dv[80], dw[80], map[16];
	const char *vn = sd_player_name(victim);

	sd_event_map(map, sizeof(map));
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"frag\",\"node\":%d,\"killer\":\"%s\",\"victim\":\"%s\","
	         "\"wad\":\"%s\",\"map\":\"%s\"}",
	         (long)time(NULL), sbbs_my_node(), sd_json_esc(g_player_name, dk, sizeof(dk)),
	         sd_json_esc(vn ? vn : "?", dv, sizeof(dv)), sd_json_esc(g_wadname, dw, sizeof(dw)), map);
	sd_event_emit(j);
}

static void sd_event_death(int cause, int by)
{
	static const char *cs[] = { "environment", "suicide", "monster", "player" };
	char               j[400], du[80], db[80], dw[80], map[16];
	const char *       bn = (cause == 3) ? sd_player_name(by) : NULL;

	sd_event_map(map, sizeof(map));
	if (bn != NULL)
		snprintf(j, sizeof(j),
		         "{\"time\":%ld,\"type\":\"death\",\"node\":%d,\"user\":\"%s\",\"cause\":\"%s\","
		         "\"by\":\"%s\",\"wad\":\"%s\",\"map\":\"%s\"}",
		         (long)time(NULL), sbbs_my_node(), sd_json_esc(g_player_name, du, sizeof(du)),
		         cs[cause], sd_json_esc(bn, db, sizeof(db)), sd_json_esc(g_wadname, dw, sizeof(dw)), map);
	else
		snprintf(j, sizeof(j),
		         "{\"time\":%ld,\"type\":\"death\",\"node\":%d,\"user\":\"%s\",\"cause\":\"%s\","
		         "\"wad\":\"%s\",\"map\":\"%s\"}",
		         (long)time(NULL), sbbs_my_node(), sd_json_esc(g_player_name, du, sizeof(du)),
		         cs[cause], sd_json_esc(g_wadname, dw, sizeof(dw)), map);
	sd_event_emit(j);
}

static void sd_event_end(void)
{
	char j[300], du[80], dw[80];

	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"end\",\"node\":%d,\"user\":\"%s\",\"wad\":\"%s\","
	         "\"mode\":\"%s\",\"frags\":%d,\"secs\":%d}",
	         (long)time(NULL), sbbs_my_node(), sd_json_esc(g_player_name, du, sizeof(du)),
	         sd_json_esc(g_wadname, dw, sizeof(dw)), sd_event_mode(), sd_total_frags(), leveltime / 35);
	sd_event_emit(j);
}

// Detect game/level transitions + frags/deaths each tic and log events. Driven by
// `usergame` (true only in a real game -- false at the menu/title and during the
// attract demos), so demos never log. Called every tic from the main loop.
static void sd_events_tick(void)
{
	static int last_gs = -1, in_game = 0;
	int        victim, cause, by;

	if (g_eventlog[0] == '\0')
		return;

	// Game start/end edge: the per-game debug `start` record and the final tally.
	if (usergame && !in_game) {
		in_game = 1;
		sd_event_start();
	} else if (!usergame && in_game) {
		in_game = 0;
		sd_event_end();
	}

	// Level cleared: GS_LEVEL(0) -> intermission(1)/finale(2) while in a game.
	if (gamestate != last_gs) {
		if (in_game && last_gs == 0 && (gamestate == 1 || gamestate == 2))
			sd_event_level();
		last_gs = gamestate;
	}

	// Our frags and our deaths, while actually playing.
	if (in_game) {
		while (sd_next_frag(&victim))
			sd_event_frag(victim);
		if (sd_check_death(&cause, &by))
			sd_event_death(cause, by);
	}
}

static void sd_node_status_atexit(void) { sbbs_node_set_ext(""); }

// In-game who's-online (Ctrl-U): a multi-line, non-blocking banner over the top
// rows -- the same shared top-of-screen overlay (white-on-red strip, ~9s, then
// auto-clears) the incoming-page notice uses. One node per row (alias + activity),
// so nothing is clipped at the right margin like the old single HU message line.
// MP-safe: no screen takeover, frame pacing untouched.
static void sd_ingame_userlist(void)
{
	sbbs_node_info_t nodes[16];
	char             alias[26], act[80];
	int              n, i, rows, max;

	if (!sbbs_node_available())
		return;
	n = sbbs_list_nodes(nodes, (int)(sizeof(nodes) / sizeof(nodes[0])), sbbs_my_node());
	if (n == 0) {
		snprintf(g_page_ov[0], sizeof(g_page_ov[0]), " No one else is online.");
		g_page_ov_rows  = 1;
		g_page_ov_until = (uint32_t)now_ms() + 6000;
		return;
	}
	max = (int)(sizeof(g_page_ov) / sizeof(g_page_ov[0]));
	snprintf(g_page_ov[0], sizeof(g_page_ov[0]), " Who's online (%d):", n);
	rows = 1;
	for (i = 0; i < n && rows < max; i++) {
		const char *who;
		const char *activity;
		if (rows == max - 1 && n - i > 1) {            // out of rows, >1 left -> summarize
			snprintf(g_page_ov[rows], sizeof(g_page_ov[0]), "   ...and %d more", n - i);
			rows++;
			break;
		}
		who = nodes[i].anon ? "UNKNOWN"
		      : sbbs_username(nodes[i].useron, alias, sizeof(alias));
		activity = nodes[i].ext                        // free-text status (waiting/in-game)
		           ? sbbs_node_ext(nodes[i].number, act, sizeof(act))
		           : sbbs_action_str(&nodes[i], act, sizeof(act));   // full phrase (room now)
		snprintf(g_page_ov[rows], sizeof(g_page_ov[0]), "   %-20.20s  %.50s", who, activity);
		rows++;
	}
	g_page_ov_rows  = rows;
	g_page_ov_until = (uint32_t)now_ms() + 9000;
}

// In-game incoming page -> a one-line HUD message + a bell (non-blocking; polled
// ~1/sec between frames). Collapses the page's CRLF/Ctrl-A formatting to one line.
static void sd_ingame_recv(void)
{
	static uint32_t last = 0;
	char            raw[600], clean[600];
	const char *    s;
	char *          d;
	uint32_t        now = now_ms();          // same clock the banner expiry compares against
	int             me, w, len, pos;

	if (!sbbs_node_available())
		return;
	if (now - last < 1000)
		return;
	last = now;
	me = sbbs_my_node();
	if (me < 1 || sbbs_recv_nmsg(me, raw, sizeof(raw)) <= 0)
		return;
	// Strip Ctrl-A codes + BEL, fold CR/LF to spaces -> one flowing string. The page
	// can exceed Doom's 80-char HU line, so it goes to the door's own wrapped banner
	// (sd_post_message/HU would chop it at the right margin).
	for (s = raw, d = clean; *s && d < clean + sizeof(clean) - 1; s++) {
		if (*s == '\x01') { if (s[1])
								s++; continue; }         // drop Ctrl-A attribute code
		if (*s == '\x07')
			continue;                                    // drop bell (we beep below)
		if (*s == '\r' || *s == '\n' || *s == ' ') {     // fold whitespace runs to one space
			if (d > clean && d[-1] != ' ')
				*d++ = ' ';
			continue;
		}
		*d++ = *s;
	}
	while (d > clean && d[-1] == ' ')                    // trim any trailing space
		d--;
	*d = '\0';
	// Word-wrap into the banner (up to 5 rows of <= w chars; break at spaces).
	w = g_text_cols - 2;
	if (w < 20)
		w = 20;
	if (w > 80)
		w = 80;
	len = (int)strlen(clean);
	pos = 0;
	g_page_ov_rows = 0;
	while (pos < len && g_page_ov_rows < 5) {
		int take = len - pos;
		if (take > w) {
			int b = w;
			while (b > 0 && clean[pos + b] != ' ')
				b--;
			take = (b > w / 2) ? b : w;
		}
		memcpy(g_page_ov[g_page_ov_rows], clean + pos, (size_t)take);
		g_page_ov[g_page_ov_rows][take] = '\0';
		g_page_ov_rows++;
		pos += take;
		while (pos < len && clean[pos] == ' ')
			pos++;
	}
	g_page_ov_until = (uint32_t)now + 9000;              // ~9 s on screen
	emit_all("\x07", 1);                                 // audible alert
}

void sd_waitroom_run(void)
{
	int     last_drawn = -2;
	boolean launched = false;
	boolean self_started = false;   // true if THIS player pressed Start (so don't beep them)

	// Drain keystrokes buffered before we got here -- type-ahead from the lobby
	// menus that the BBS console didn't consume. Otherwise a stale Enter/'S' left
	// in the socket auto-starts the match the instant the room opens, before the
	// player can read it or press Q to cancel.
	{
		unsigned char drain[64];
		while (conn_read(drain, sizeof(drain)) > 0)
			;
	}

	sbbs_node_set_ext("in the SyncDOOM waiting room");   // who's-online status

	while (net_waiting_for_launch) {
		unsigned char kb[8];
		ssize_t       n;

		NET_CL_Run();
		NET_SV_Run();

		if (!net_client_connected)
			I_Error("NET_WaitForLaunch: Lost connection to server");

		if (sd_check_pages())          // an incoming page was shown -> redraw
			last_drawn = -2;

		if (net_client_received_wait_data) {
			net_waitdata_t *w = &net_client_wait_data;

			// Keep the network names (by player number) for in-game chat.
			memcpy(sd_chat_player_names, w->player_names,
			       sizeof(sd_chat_player_names));

			if (w->num_players != last_drawn) {
				sd_waitroom_draw();
				last_drawn = w->num_players;
			}
			// Auto-start once every expected slot is filled.
			if (!launched && w->is_controller
			    && w->num_players >= w->max_players) {
				NET_CL_LaunchGame();
				launched = true;
			}
		}

		// Poll one keystroke: Q cancels; the host can start with S/Enter.
		n = conn_read(kb, sizeof(kb));
		if (n == 0) {
			I_Quit();                          // hangup
		} else if (n > 0) {
			boolean want_start = false;
			ssize_t j;
			// Scan the whole read: Q (cancel) wins over a Start in the same burst,
			// so a buffered Enter can't override the player's deliberate cancel.
			for (j = 0; j < n; j++) {
				unsigned char c = kb[j];
				if (c == 'q' || c == 'Q') {
					NET_CL_Disconnect();
					emit_all("\x1b[2J\x1b[H", 7);
					I_Quit();              // back to the lobby/BBS
				}
				if (c == 0x15) {          // Ctrl-U: who's online
					sd_show_nodelist();
					last_drawn = -2;       // force a waiting-room redraw
					break;
				}
				if (c == 0x10) {          // Ctrl-P: page a node
					sd_page_flow();
					last_drawn = -2;
					break;
				}
				if (c == 's' || c == 'S' || c == '\r')
					want_start = true;
			}
			if (want_start && !launched && net_client_received_wait_data
			    && net_client_wait_data.is_controller) {
				net_waitdata_t *w = &net_client_wait_data;
				// Don't let the controller start a multi-player match alone -- that
				// strands them in a solo game thinking it's co-op (the trap from the
				// live test). A match created for 1 (explicit solo/test) may start.
				if (w->num_players >= 2 || w->max_players <= 1) {
					NET_CL_LaunchGame();
					launched = true;
					self_started = true;   // this player started it -- they're watching
				} else {
					emit_all("\a", 1);     // nudge: the on-screen text says why
				}
			}
		}

		I_Sleep(50);
	}

	// The game is starting. Cue a player who was waiting (and may have looked
	// away) with a BEL -- but not the one who just pressed Start, who is plainly
	// at the keyboard. Then wipe the waiting-room text so it doesn't linger beside
	// the first rendered frame (the bitmap tiers only overdraw their own centered
	// region, not the whole screen).
	if (!self_started)
		emit_all("\a", 1);
	emit_all("\x1b[2J\x1b[H", 7);
}

// main: read connection/session params (-door32 drop file and/or -s/-l/-t),
// set up the per-user sandbox (-home), inject -scaling 2 so Doom's 320x200
// fills 640x400, and pass everything else (e.g. -iwad) through to doomgeneric.
// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
	char ** child;
	int     cn = 0, i;

#ifdef _WIN32
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);   // xpdev sockwrap doesn't self-init Winsock
#else
	signal(SIGPIPE, SIG_IGN);    // a dead client -> EPIPE return, not signal death
#endif

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
			int   j, port = 0, en = 0;
			char *extra[64];
			long  pid;
			for (j = 1; j < argc; j++) {
				if (strcmp(argv[j], "-port") == 0 && j + 1 < argc) {
					port = atoi(argv[j + 1]);
					j++;                         // consume the value
					continue;
				}
				if (strcmp(argv[j], "-spawnserver") == 0)
					continue;
				if (en < 62)                     // forward metadata (-maxplayers, ...)
					extra[en++] = argv[j];
			}
			extra[en] = NULL;
			if (port <= 0)
				port = mp_alloc_port(20000, 20063);  // TODO: range from [net]
			if (port <= 0) {
				fprintf(stderr, "syncdoom: no free server port\n");
				return 1;
			}
			pid = mp_spawn_server(argv[0], port, extra);
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
	// Snapshot the sysop/built-in defaults now (before CLI + per-user) so a save
	// only records what the player changes in-game (sd_save_user_prefs).
	g_base_grace_fresh   = g_grace_fresh;
	g_base_keyup_idle_ms = g_keyup_idle_ms;
	g_base_turn_grace    = g_turn_grace;
	g_base_instant_turn  = sd_instant_turn;
	g_base_mouse_mode    = g_mouse_mode;
	snprintf(g_base_frames, sizeof(g_base_frames), "%s", frames_in_flight_str());

	// External waiting-room splash: [game] splash if set, else <door dir>/waiting.bin.
	// Here (not in read_syncdoom_ini) so it loads even when there's no syncdoom.ini.
	{
		char sp[PATH_MAX];
		if (g_splash_cfg[0] == '/' || g_splash_cfg[0] == '\\'
		    || (g_splash_cfg[0] && g_splash_cfg[1] == ':'))
			snprintf(sp, sizeof(sp), "%s", g_splash_cfg);                 // absolute
		else if (g_splash_cfg[0])
			snprintf(sp, sizeof(sp), "%s%s", g_door_dir, g_splash_cfg);   // under door dir
		else
			snprintf(sp, sizeof(sp), "%swaiting.bin", g_door_dir);        // default
		load_splash(sp);
	}

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
		} else if (strcmp(argv[i], "-skill") == 0) {        // before -s (prefix-matched): a
			if (i + 1 < argc && argv[i + 1][0] != '-')      // doomgeneric flag the parent
				i++;                                         // ignores -- consume its value so
			                                                 // "-skill" isn't misread as "-s kill"
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
		} else if (strcmp(argv[i], "-mouse") == 0) {        // terminal mouse: on | off
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (stricmp(v, "off") == 0 || stricmp(v, "none") == 0 || stricmp(v, "false") == 0)
				g_mouse_mode = MOUSE_OFF;
			else
				g_mouse_mode = MOUSE_ON;                    // "on" / "true" / empty
			g_mouse_cli = 1;
		} else if (strcmp(argv[i], "-kpturn") == 0) {       // turn-key fresh grace (tight taps)
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v) {
				g_turn_grace = (uint32_t)strtoul(v, NULL, 10);
				g_kp_cli_turn = 1;
			}
		} else if (strcmp(argv[i], "-kpdelay") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v) {
				g_grace_fresh = (uint32_t)strtoul(v, NULL, 10);
				g_kp_cli_tap = 1;
			}
		} else if (strcmp(argv[i], "-kpsmooth") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v) {
				g_keyup_idle_ms = (uint32_t)strtoul(v, NULL, 10);
				g_kp_cli_hold = 1;
			}
		} else if (strcmp(argv[i], "-home") == 0) {
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v) { strncpy(g_home, v, sizeof(g_home) - 1); g_home[sizeof(g_home) - 1] = '\0'; }
		} else if (strcmp(argv[i], "-name") == 0) {       // multiplayer player handle
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v) { strncpy(g_player_name, v, sizeof(g_player_name) - 1); g_player_name[sizeof(g_player_name) - 1] = '\0'; }
		} else if (strcmp(argv[i], "-wadname") == 0) {    // friendly WAD-set name (who's-online status)
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v) { strncpy(g_wadname, v, sizeof(g_wadname) - 1); g_wadname[sizeof(g_wadname) - 1] = '\0'; }
		} else if (strcmp(argv[i], "-eventlog") == 0) {   // jsonl game-event log (lobby activity feed)
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v) {
				char  dir[PATH_MAX];
				char *fn;
				strncpy(g_eventlog, v, sizeof(g_eventlog) - 1); g_eventlog[sizeof(g_eventlog) - 1] = '\0';
				strncpy(dir, v, sizeof(dir) - 1); dir[sizeof(dir) - 1] = '\0';
				if ((fn = getfname(dir)) != dir) { *fn = '\0'; mkpath(dir); }   // ensure its dir exists
			}
#ifdef WITH_JXL
		} else if (strcmp(argv[i], "-jxldistance") == 0) {   // JXL lossy distance (size lever)
			const char *v = (i + 1 < argc) ? argv[++i] : "";
			if (*v)
				g_jxl_distance = (float)atof(v);
#endif
		}
	}
	// Per-user input-feel overrides (saved by the in-game Options > Input sliders)
	// layer over the [input] ini defaults now that -home and any -kp* flags are known.
	load_user_prefs();
	// Locate node.dab (ctrl) + user/name.dat (data) for Ctrl-U/Ctrl-P. A no-op
	// (feature disabled) when not launched inside a BBS (no $SBBSCTRL).
	sbbs_node_init(g_home);
	atexit(sd_node_status_atexit);   // clear our who's-online free-text status on exit
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
			int is_iwad = (strcmp(argv[i], "-iwad") == 0);
			child[cn++] = argv[i];                       // resolve WAD paths to absolute
			while (i + 1 < argc && argv[i + 1][0] != '-') {  // before we chdir into the sandbox
				const char *wadarg = argv[++i];            // (bare names resolve in [wads] dir)
				if (is_iwad && g_wadname[0] == '\0') {     // default the WAD-set name to the IWAD base
					char *dot;
					strncpy(g_wadname, getfname(wadarg), sizeof(g_wadname) - 1);
					g_wadname[sizeof(g_wadname) - 1] = '\0';
					if ((dot = strrchr(g_wadname, '.')) != NULL)
						*dot = '\0';                       // strip ".wad"
				}
				child[cn++] = wadcopy(wadarg);
			}
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
		if (strcmp(argv[i], "-jxl")         == 0 || strcmp(argv[i], "-sixel")   == 0 ||
		    strcmp(argv[i], "-charset")     == 0 || strcmp(argv[i], "-mode")    == 0 ||
		    strcmp(argv[i], "-colors")      == 0 || strcmp(argv[i], "-home")    == 0 ||
		    strcmp(argv[i], "-kpturn")      == 0 || strcmp(argv[i], "-kpdelay") == 0 ||
		    strcmp(argv[i], "-name")        == 0 || strcmp(argv[i], "-mouse")   == 0 ||
		    strcmp(argv[i], "-wadname")     == 0 || strcmp(argv[i], "-jxldistance") == 0 ||
		    strcmp(argv[i], "-eventlog")    == 0 ||
		    strcmp(argv[i], "-kpsmooth")    == 0) {
			i++;                                           // exact flags: skip the flag + its value
			continue;
		}
		if (strncmp(argv[i], "-term", 5) == 0) {           // prefix (matches pre-scan); may be glued
			if (argv[i][5] == '\0')
				i++;
			continue;
		}
		if (strcmp(argv[i], "-skill") == 0) {              // a real doomgeneric flag: pass it
			child[cn++] = argv[i];                         // (and its value) through -- must be
			if (i + 1 < argc && argv[i + 1][0] != '-')     // before the "-s" prefix-skip below,
				child[cn++] = argv[++i];                   // which would otherwise eat "-skill"
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
	for (;;) {
		doomgeneric_Tick();

		// Ctrl-U / Ctrl-P and incoming pages are shown as non-blocking one-line HUD
		// messages (no screen takeover, so the frame pacing is undisturbed -- the
		// blocking full-screen version stalled the loop and wedged the DSR pacing,
		// fixed only by an F4 tier re-init). MP-safe too. Text-entry paging stays in
		// the waiting room; in-game Ctrl-P just points there.
		if (g_want_userlist) {
			g_want_userlist = 0;
			sd_ingame_userlist();
		}
		if (g_want_page) {
			g_want_page = 0;
			sd_post_message("To page a user, use Ctrl-P in the multiplayer waiting room.");
		}
		sd_ingame_recv();
		sd_set_game_status();      // keep the who's-online status current (map changes)
		sd_events_tick();          // log level/game events to the jsonl activity log
	}
	return 0;
}
