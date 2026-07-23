/* termgfx_termio.c -- terminal session: fd resolution, probe burst, reply parsing,
 * hotkeys, pacing bookkeeping.  Present path arrives in Task 4.
 *
 * Ported from syncconquer/door/door_io.c (fd resolution: door_io.c:1297-1364;
 * output staging: door_io.c:~1370; probe burst: door_io.c:2738-2770; reply
 * parser: door_io.c's door_csi_final()/door_io_pump(), ~3016-3316), trimmed
 * to what this task needs -- no audio, no mouse, no kitty/evdev key modes
 * (M3's job), no pace-ring wiring (Task 4's job). */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* The door's whole Windows-vs-POSIX surface -- the monotonic clock, the sleep,
 * WSAStartup, non-blocking send()/recv(), and the isatty/exists/getpid dev
 * helpers -- lives behind termgfx_plat.h, so this file keeps no #ifdef _WIN32 and
 * pulls in no <winsock2.h>/<netinet/*>/<unistd.h> of its own. See termgfx_plat.h. */
#include "termgfx_plat.h"

#include "termgfx_termio.h"
#include "term.h"
#include "caps.h"
#include "pace.h"
#include "idle.h"
#include "door32.h"
#include "geometry.h"
#include "sixel.h"
#include "termgfx_quant.h"   /* termgfx_quant_rgb(): RGB888 -> 256-color, for
                              * present_rgbx()'s sixel tier (Task 1's shared lib) */
#include "jxl.h"
#include "apc.h"
/* SGR mouse: enable/restore strings, the DECRPM latch, and the report
 * classifier (termgfx_sgr_kind_t) -- shared with syncconquer/syncduke/
 * syncdoom. This file owns the coordinate mapping (termgfx_termio_mouse_report(),
 * below); mouse.h only knows the wire protocol. */
#include "mouse.h"
/* termgfx: key-mode negotiation (kitty CSI-u / SyncTERM evdev) + the shared
 * kitty-parameter and evdev-keycode decoders -- this file owns the byte
 * parser and the queued key events (termgfx_key_event(), below); keymode.h only
 * knows the wire protocol, same division of labor as mouse.h above. */
#include "keymode.h"
/* termgfx_audio_query/_opus_query + their reply parsers: the M4 audio
 * capability probe (audio_scan_feed(), below). No PCM is staged here -- the
 * streaming feed is its own module; this file only asks the question and
 * latches the answer for termgfx_termio_audio_available(). */
#include "audio.h"
/* The shared PCM streaming module the mixer's audio goes out through
 * (termgfx_termio_audio_stream(), below); audio_mgr.h only for the encoder's default
 * VBR quality. NOT audio_mgr's own termgfx_audio_stream_chunk()/_stop(),
 * which are a different (FMV) path this door does not use. */
#include "audio_stream.h"
#include "audio_mgr.h"
/* xpdev: iniReadFile()/iniGetInteger() for the sysop sixel_max override
 * (termgfx_read_ini(), below) and the [audio] tuning keys (audio_read_ini()) --
 * a door's own C++ glue already reads the same <door>.ini this way (e.g.
 * door/syncscumm.cpp, for "subtitles"); this is the identical house pattern,
 * just in the pure-C session layer instead of the C++ ConfMan glue. Also pulls in genwrap.h's
 * stricmp()/strnicmp(), used by the XTVERSION reply match in parse_bytes().
 *
 * No include-ORDER constraint here, unlike door/syncscumm.cpp:13-18, where
 * ini_file.h MUST precede common/scummsys.h -- scummsys pulls in
 * common/forbidden.h, which poisons strupr()/printf into unusable macros
 * that ini_file.h -> genwrap.h then trips over. This file is plain C and
 * includes no scummsys, so it has nothing to order against; the include sits
 * here purely for readability. Said out loud so the next reader doesn't have
 * to wonder why the two files disagree. */
#include "ini_file.h"
#include "dirwrap.h"   /* xpdev: isdir() -- portable directory test (termgfx_isdir) */

/* ---- module-private defines that used to live in termgfx_termio.h ------------------
 * The shared API header (termgfx_termio.h) deliberately does NOT export these
 * (see task-1-report.md): the framebuffer geometry is republished there under
 * the TERMGFX_TERMIO_FB_* names, and the audio-tuning defaults are syncscumm's
 * own door policy fed into the already-shared audio module (a future door such
 * as syncrpg would pick its own). The body below still references them by their
 * original TERMGFX_* spellings, so they are re-declared here, internal to this
 * file, with their EXACT former values -- a pure relocation, no value change.
 *
 * The door-specific half of that -- the ini filename, the env-var prefix and
 * the diagnostic paths -- is no longer hardcoded: it derives from the door's
 * own name (see g_app / termgfx_termio_set_app_name() below). The audio
 * defaults above are still fixed values every door shares. */
#define TERMGFX_FB_W TERMGFX_TERMIO_FB_W   /* 320 -- shared value, single source */
#define TERMGFX_FB_H TERMGFX_TERMIO_FB_H   /* 200 -- shared value, single source */
#define TERMGFX_AUDIO_RATE       24000     /* mixer/stream rate (see the doc comment
                                        * at termgfx_termio_audio_stream) */
#define TERMGFX_CHUNK_MS         250       /* per-shipped-chunk audio, ms */
#define TERMGFX_PREBUFFER_CHUNKS 3         /* chunks held before playback starts */
#define TERMGFX_AUDIO_HEADROOM   70        /* pre-encode PCM scale, percent (~-3 dB) */

/* The door's short name. This module has no business knowing WHICH door it is
 * driving, but each door still needs its own sysop settings file, its own
 * diagnostic env-vars and its own directory under the BBS data dir -- so all
 * three derive from this one string instead of being hardcoded:
 *
 *   settings     <name>.ini                       (sixel_max, [audio])
 *   env-vars     <NAME>_SOCK / _SIXELOUT / _AUDIODUMP / _TRACE
 *   diagnostics  <data-dir>/<name>/{stderr,trace,wirecap} touch-files, the
 *                dev fallback ./<name>-<what>, and /tmp/<name>.<pid>.<what>
 *
 * Defaults to argv[0]'s basename minus any .exe -- which is exactly the
 * "syncscumm" these were hardcoded to for that door, and the right name for
 * every other one. A door whose binary name is not its door name (and a unit
 * test, whose binary name is not a door name at all) calls
 * termgfx_termio_set_app_name() before termgfx_termio_init(). */
static char g_app[24];

void termgfx_termio_set_app_name(const char *name)
{
	size_t i = 0;

	g_app[0] = '\0';
	if (name == NULL)
		return;
	/* One name has to survive as both a filename component and an env-var
	 * name, so keep it to the character set both can carry; a path- or
	 * shell-significant byte ends it rather than escaping into either. */
	while (i + 1 < sizeof g_app && name[i] != '\0'
	       && (isalnum((unsigned char)name[i]) || name[i] == '_' || name[i] == '-')) {
		g_app[i] = name[i];
		i++;
	}
	g_app[i] = '\0';
}

/* The door's name, or "termgfx" if nothing has set one yet. */
static const char *app_name(void)
{
	return g_app[0] != '\0' ? g_app : "termgfx";
}

/* Derive the name from argv[0] unless the door set one explicitly. */
static void app_name_from_argv0(const char *argv0)
{
	const char *base, *p;
	char        buf[sizeof g_app];
	size_t      n;

	if (g_app[0] != '\0' || argv0 == NULL || *argv0 == '\0')
		return;
	base = argv0;
	for (p = argv0; *p != '\0'; p++)
		if (*p == '/' || *p == '\\')
			base = p + 1;
	n = strlen(base);
	if (n > 4 && base[n - 4] == '.'
	    && tolower((unsigned char)base[n - 3]) == 'e'
	    && tolower((unsigned char)base[n - 2]) == 'x'
	    && tolower((unsigned char)base[n - 1]) == 'e')
		n -= 4;                                  /* syncscumm.exe -> syncscumm */
	if (n >= sizeof buf)
		n = sizeof buf - 1;
	memcpy(buf, base, n);
	buf[n] = '\0';
	termgfx_termio_set_app_name(buf);
}

/* "<APPNAME>_<suffix>" -- e.g. SYNCSCUMM_TRACE. Returns a static buffer: one
 * live result at a time, which is all any caller below needs. */
static const char *app_env(const char *suffix)
{
	static char buf[sizeof g_app + 24];
	const char *name = app_name();
	size_t      i;

	for (i = 0; i + 1 < sizeof buf && name[i] != '\0'; i++)
		buf[i] = (char)toupper((unsigned char)name[i]);
	snprintf(buf + i, sizeof buf - i, "_%s", suffix);
	return buf;
}

/* "<name>.ini" -- the sysop's per-door settings file, read from the door's
 * launch directory. Static buffer, same one-at-a-time rule as app_env(). */
static const char *app_ini(void)
{
	static char buf[sizeof g_app + 8];

	snprintf(buf, sizeof buf, "%s.ini", app_name());
	return buf;
}

static int g_fd = -1, g_fd_in = -1;      /* -1 = headless (no session) */
static int g_active;
static int g_quit, g_stats, g_menu;

/* --- idle-USER detection (idle.h) -----------------------------------------
 * Lives HERE, in shared termio, so every door driven through it gets the same
 * behavior -- syncscumm today, syncrpg by inheritance.
 *
 * It exists because the BBS's own "Maximum Inactivity" cannot see a DSR-paced
 * door: that counter is reset by any socket read, and frame pacing makes the
 * terminal answer ~10x/second on its own, so the threshold is never reached.
 * Presence has to be judged on REAL input, which only the door can tell apart
 * from its own pace-acks.
 *
 * TERMGFX_IDLE_DEFAULT is the shipped policy, not merely a fallback: unset is
 * NOT off. `timeout = 0` in the door's ini disables it. Matches the sibling
 * doors (syncretro/syncconquer/syncmoo1/syncdoom/syncduke); keep them in step. */
#define TERMGFX_IDLE_DEFAULT 600      /* 10 minutes */
static termgfx_idle_t g_idle;
static int      g_idle_armed;
static int      g_idle_arg = -1;      /* -i<seconds>; -1 = not given */
static unsigned g_idle_timeout = TERMGFX_IDLE_DEFAULT;
static unsigned g_idle_warn    = 60;
static int      g_idle_showing;       /* the countdown owns the bottom row */
static char     g_idle_msg[96];
static int      g_idle_clear;         /* it just went: wipe the row once */
static int      g_idle_expired_logged;/* the exit line is logged exactly once */

/* --- session time limit (the BBS's, not ours) -----------------------------
 * The minutes the BBS says the caller has left, from DOOR32.SYS line 9 or an
 * explicit -t<seconds>. Until now termio read the drop file for its socket and
 * DISCARDED this, so a door driven through here ran until the player quit --
 * the BBS reclaimed the node only when the session itself ended.
 *
 * Distinct from the idle clock above in the way that matters to a player: this
 * one cannot be reset by pressing a key. It counts down regardless, so its
 * warning says "your BBS time", not "still there?". */
#define TERMGFX_TIMELIMIT_WARN_S 60   /* visible countdown over the last minute */
static uint32_t g_time_limit_ms;      /* 0 = no limit given */
static uint32_t g_deadline_ms;
static int      g_deadline_armed;
static int      g_deadline_logged;
static unsigned char g_menu_letter;   /* Ctrl+<letter> opens the GMM; 0 = disabled */
static int g_have_sixel, g_is_syncterm, g_jxl, g_probe_replied;
static int g_no_gfx_notified;   /* one-shot: unsupported-terminal notice sent */
static int g_cterm_ver;
#ifdef WITH_JXL
/* CTerm >= 1.329 (TERMGFX_CTERM_VER_BLOB): DrawJXLBlob draws inline, no C;S
 * cache-file round trip -- set from the DA1/CTDA reply's version field
 * (csi_final()'s 'c' case, below). Only meaningful when this build has a
 * real JXL encoder to feed it (see termgfx_tier(), piece 6). */
static int g_img_blob_ok;
#endif
static int g_canvas_w = 640, g_canvas_h = 400;   /* until the probe answers */
static int g_canvas_exact;   /* set once the ESC[4;h;wt pixel report lands */
/* XTSMGRAPHICS (ESC[?2;1S) reply: the terminal's REPORTED hard sixel raster
 * ceiling (0 = unreported -- xterm ships with window ops disabled by default
 * and never answers at all, so this stays 0 for it unless the sysop enables
 * them).
 *
 * The SIXEL tier's effective ceiling (termgfx_termio_present()'s "Fit + center"
 * block) is resolved in this precedence order:
 *
 *   1. g_sixel_max_override (sysop "sixel_max" ini key, termgfx_read_ini()) --
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
/* Sysop override, <door>.ini's "sixel_max" key (termgfx_read_ini()): 0 = no
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
static FILE *g_capture;                  /* <DOOR>_SIXELOUT mode */

/* SGR-Pixels (DEC 1016) latch: 0 = report coords are 1-based text CELLS
 * (the common case), 1 = 1-based canvas PIXELS -- confirmed by the DECRPM
 * reply (csi_final()'s 'y' case) or auto-detected the moment a report's
 * coordinate exceeds the text grid (termgfx_termio_mouse_report(), below). Session-wide,
 * never reset once latched -- termgfx_mouse_note_pixel_report()'s own
 * contract. */
static termgfx_mouse_t g_mouse;

/* The negotiated key mode (termgfx/keymode.h): kitty CSI-u flags pushed, or
 * SyncTERM evdev physical-key reports enabled, or neither. Set from the
 * kitty query reply and the CTDA cap-8 check (csi_final()'s 'u' and 'c'
 * cases, below) and undone in termgfx_termio_shutdown(). */
static termgfx_keymode_t g_km;

/* Raw PRE-ENCODE mixer PCM tap: termgfx_termio_audio_stream() appends exactly the
 * interleaved stereo S16 frames ScummVM's mixer handed us, at TERMGFX_AUDIO_RATE,
 * before termgfx/audio_stream.c resamples, Opus-encodes or drops any of it.
 * That makes the file the reference signal -- "what the game actually sounds
 * like" -- to diff an encode against when a session reports distortion.
 *
 * Ahead of the pre-encode headroom too (audio_apply_headroom(), below), which
 * is the one thing between this tap and the encoder that changes the samples
 * rather than the packaging. Deliberate, and worth knowing before diffing: the
 * dump is what the MIXER produced, not what the wire carried, so a harness
 * reproducing the wire has to apply "[audio] headroom" to it the same way the
 * door does. The alternative -- dumping the scaled PCM -- would have made the
 * file useless for the question it was added to answer, which is whether a
 * defect is in the mix or in our path.
 *
 * Gated on the <DOOR>_AUDIODUMP=<path> env var (the door's own name, upper-
 * cased -- SYNCSCUMM_AUDIODUMP for syncscumm), off by default: unset, this
 * stays NULL and the tap in termgfx_termio_audio_stream() is a single NULL test on a
 * path that already runs tens of times a second. A dev/test tool only -- like
 * <DOOR>_DUMP (door/video_dump.cpp) and <DOOR>_SIXELOUT (resolve_fd(),
 * below), an env var does NOT survive a real door launch, because the BBS
 * execvp()s the door with no shell; the touch-file gates g_trace/g_tee use
 * exist for exactly that reason. This one is deliberately env-only: it is for
 * headless test-script runs (test/boot_bass.sh), not for a live node.
 *
 * Opened in termgfx_termio_init() BEFORE resolve_fd(), and tapped BEFORE the g_active
 * check, so a HEADLESS run -- which has no session at all and streams nothing
 * -- still dumps. That is the point: the mixer's PCM does not depend on a
 * terminal being there, and a test script has no terminal.
 *
 * Decode with:  ffmpeg -f s16le -ar 24000 -ac 2 -i <path> out.wav
 * (that -ar is TERMGFX_AUDIO_RATE, termgfx_termio.h -- the tap is pre-encode, so it is
 * the MIXER's rate, never the Opus rate the wire happens to carry.) */
static FILE *g_audiodump;

/* Release audio_apply_headroom()'s scratch buffer. Forward-declared here, next
 * to the dump termgfx_termio_shutdown() closes beside it, because the buffer and the
 * function that grows it belong with the rest of the streaming-audio state much
 * further down -- and termgfx_termio_shutdown() is defined well above that. Same reason
 * termgfx_audio_underrun() is forward-declared below. */
static void audio_free_scratch(void);

/* Present-path trace: one line per present-attempt -- ms-timestamp, outcome,
 * tier, bytes emitted, in-flight, pacing depth, cumulative dropped-frame count.
 * A permanent diagnostic asset for the fade/pacing behavior (M2). Line-buffered
 * (setvbuf below), so a killed/timed-out run still leaves a complete-to-the-
 * last-line file. Gated behind EITHER an env var OR a touch-file, with the env
 * var taking precedence:
 *
 * 1. <DOOR>_TRACE=<path> env var (for test scripts): open the specified
 *    absolute path directly.
 * 2. Touch-file gate (for BBS-launched doors -- env vars don't survive execvp):
 *    enabled only when the operator creates the touch-file <data-dir>/
 *    <door>/trace (SBBSDATA -- see xtrn/CLAUDE.md's env-var table -- for a
 *    real door launch; a dev/standalone run with no SBBSDATA falls back to a
 *    relative ./<door>-trace in the cwd). When enabled, opens
 *    /tmp/<door>.<pid>.trace (the pid keeps two nodes' traces from
 *    colliding). Disabled by default: no file is opened and termgfx_trace() pays
 *    nothing for it.
 *
 * The termgfx_trace() emitter is defined further down, past the pacing globals it
 * reads (g_inflight/g_auto_depth). */
static FILE *g_trace;
/* Raw wire-byte capture: termgfx_termio_init() opens this and termgfx_termio_flush() tees
 * EXACTLY the bytes that reached the socket -- i.e. what the terminal actually
 * received, AFTER any backpressure drop -- for offline decode. Teeing here,
 * not in out_put(), is deliberate: out_put() records the intended bytes,
 * which hides a dropped/truncated frame.
 *
 * Gated behind a touch-file, not an env var: the BBS execs the door via
 * execvp with no shell, so an env var set before launch does not survive
 * into the process. Enabled only when the operator creates the touch-file
 * <data-dir>/<door>/wirecap (SBBSDATA -- see xtrn/CLAUDE.md's env-var
 * table -- for a real door launch; a dev/standalone run with no SBBSDATA
 * falls back to a relative ./<door>-wirecap in the cwd). Disabled by
 * default: no file is opened and out_put()/flush() pay nothing for it.
 *
 * When enabled, opens /tmp/<door>.<pid>.raw (the pid keeps two nodes'
 * captures from colliding) FULLY buffered (256KB), not unbuffered: a
 * palette-storm frame can be 0.5-1.5MB, and an earlier always-on+unbuffered
 * version of this tee turned every present() into a synchronous disk write
 * on the present path -- a suspected cause of mid-fade pauses (M2
 * live-retest). Buffering risks losing a capture's tail on an unclean kill,
 * an acceptable trade for an opt-in debug aid that must not itself stall a
 * real session. */
static FILE *g_tee;
static void  termgfx_trace_in(const uint8_t *buf, int n);   /* defined past pacing globals */
static void  termgfx_stats_draw(void);                      /* defined past pacing globals */
static void  idle_poll(void);                               /* idle clock; driven by the pump */
static void  deadline_poll(void);                           /* BBS session limit; likewise */

/* Set the instant the peer goes away (EOF/hard read error) or a flush hits a
 * hard write error (not EAGAIN/EINTR) -- see termgfx_termio_flush()/termgfx_termio_pump().
 * Doubles as the "session is dead" flag: termgfx_termio_flush() drops (rather than
 * retries) buffered output once this is set, so out_put()/flush() are safe
 * to keep calling from the caller's normal shutdown path without blocking or
 * erroring. g_quit is also set alongside it so termgfx_termio_quit_requested()
 * callers unwind the same way they would for an explicit q/Ctrl-C. */
static int g_hangup;

/* Pre-door DECSSDT status-line type captured from the DECRQSS reply that
 * termgfx_term_status_off provokes (-1 = not captured yet / unsupported
 * terminal); restored on termgfx_termio_shutdown() (door_io.c:1134 pattern). */
static int g_status_type = -1;

/* ---- output staging (door_io.c:~1370 pattern) ----
 *
 * ONE stage, STRICT FIFO. Video frames (a sixel DCS or a JXL APC, tens of KB)
 * and audio (a ~2KB Opus APC every 100ms, via termgfx/audio_stream.c) share
 * g_out and leave it in exactly the order they were staged. Nothing jumps the
 * queue. That is a deliberate design decision with a history -- see "WHY FIFO"
 * below -- and the accounting immediately after it exists so that sharing the
 * queue does not cost audio what it cost it once already.
 *
 * THE DEFECT. Beneath a Steel Sky's comic intro played its dialogue with
 * seconds-long gaps and audible skips for the whole ~260s comic, then was
 * flawless the moment gameplay started. The session's counters read
 * audio=1/3323/55/702: 702 of 4025 chunks (~17%, ~70 SECONDS of speech) thrown
 * away, every one of them during the comic and not one after.
 *
 * It was never bandwidth -- the comic averaged 127 KB/s and gameplay 104 KB/s,
 * and gameplay was perfect. The comic's palette storm forces the full-frame
 * path (the cheap dirty-rect path needs a stable palette), and those frames
 * are big: 899 full frames averaging 24KB against gameplay's 7.4KB dirty
 * rects, 142 of them over 48KB, the largest 82KB.
 *
 * termgfx/audio_stream.c drops a chunk once the door's backlog() reports more
 * than TERMGFX_STREAM_BACKLOG_BYTES (48KB) on two consecutive chunk boundaries
 * -- a rule meant to detect a link that cannot carry AUDIO. But the door
 * reported g_out_len, the depth of the SHARED stage, which is video-dominated.
 * So a SINGLE 82KB video frame sitting in the stage tripped a 48KB AUDIO rule,
 * and audio -- all of ~20 KB/s of that 127 -- dropped itself to make room for
 * video's bursts. Audio was punishing itself for video's mistakes. The module's
 * rule was right; the number the door fed it was not.
 *
 * THE FIX: report AUDIO's OWN share of the queue (termgfx_termio_audio_backlog(), via
 * the g_aseg ring below), so the two-strike rule can only fire when the link
 * genuinely cannot carry audio -- which is what "sustained link saturation"
 * always meant. The cushion also grew to ~800ms (see audio_stream_open()) to
 * hold audio that is late-but-correctly-ordered behind a big frame.
 *
 * WHY FIFO, AND NOT "AUDIO FIRST". The obvious alternative -- a second, PRIO
 * stage written ahead of staged video -- was written, tested, and reverted,
 * because it CORRUPTED THE WIRE. A video frame is ONE contiguous DCS or APC
 * sequence; insert an audio APC after its head has gone out and the terminal
 * decodes garbage. Guarding that with "only insert at a sequence boundary" is
 * where it died: out_put()'s stage-full guard (below) flushes from INSIDE a
 * frame, opening the DCS on the wire and draining the stage to EMPTY, so every
 * local measure of "boundary" -- including g_out_len == 0 -- reported a
 * boundary that did not exist, and audio walked into the open sequence. Four
 * flags did not close it and three candidate fixes did not either.
 *
 * REORDERING IS WHAT CREATES THE CORRUPTION SURFACE. With strict FIFO there is
 * no surface: an audio APC cannot land inside a video sequence because it
 * cannot move relative to it AT ALL. The property is preserved BY
 * CONSTRUCTION, not by guards -- and there is nothing here to re-derive wrong
 * the next time out_put() gains a new early exit. If a future change to this
 * file starts to grow a "may audio go now?" predicate, that is this same
 * mistake arriving again; stop.
 *
 * The cost of FIFO is latency, not loss: audio queued behind an 82KB frame
 * arrives up to ~650ms late at 127 KB/s. The ~750ms cushion covers it --
 * measured, not guessed: of the comic's 3673 frames, 318 (8.7%) take longer
 * than 300ms to drain and NONE exceed 800ms, the worst at 663ms. The cushion is
 * a PRODUCT, TERMGFX_CHUNK_MS x TERMGFX_PREBUFFER_CHUNKS, not either one alone; see
 * audio_stream_open() before changing either. */
static uint8_t g_out[1 << 18];
static size_t  g_out_len;

/* Monotonic stream offset of g_out[0]: the total number of bytes that have
 * ever LEFT the stage (written to the socket, or discarded on a dead peer --
 * either way, no longer pending). g_out_base + g_out_len is therefore the
 * offset of the next byte to be staged, so the two together define a stable
 * coordinate system for "where in the outbound byte stream is this?" that
 * termgfx_termio_flush()'s memmove cannot disturb. The g_aseg ring below records audio
 * in these coordinates. */
static uint64_t g_out_base;

/* The audio segments currently in the FIFO, in stream coordinates, oldest
 * first. Every staged run of audio bytes gets one, and the parts of them that
 * are still >= g_out_base are exactly what termgfx_termio_audio_backlog() reports.
 *
 * 64 is far more than the door can actually use, and the reasoning is worth
 * writing down because it is what makes the overflow path in aseg_add() cold.
 * A new segment is only created when audio is NOT contiguous with the previous
 * audio -- aseg_add() coalesces when it is, which by itself collapses the
 * entire prebuffer release (8 chunks put back to back, nothing between them)
 * into ONE segment. So growth needs VIDEO staged between two audio puts, and
 * termgfx_termio_present() only ever stages a frame when the FIFO is already empty
 * (its g_out_len gate) -- an empty FIFO means aseg_prune() has just retired
 * every segment there was. The live count therefore sits at one or two, and 64
 * is slack, not a budget.
 *
 * "Cold" is not "impossible", so aseg_add() still handles a full ring, and it
 * MERGES rather than forgets: forgetting a segment would UNDER-report the
 * backlog, which is the exact class of bug this whole change exists to fix.
 * Being wrong in the safe direction is a decision, not an accident. */
#define TERMGFX_ASEG_MAX 64
typedef struct {
	uint64_t start;   /* stream offset of the first audio byte */
	uint64_t end;     /* stream offset one past the last */
} termgfx_aseg_t;
static termgfx_aseg_t g_aseg[TERMGFX_ASEG_MAX];
static int        g_aseg_head;    /* index of the oldest live segment */
static int        g_aseg_n;       /* live segments */
static uint32_t   g_aseg_merges;  /* ring overflows survived; see aseg_add() */

static uint64_t g_tx_bytes;      /* every wire-bound byte, for the Ctrl-S KB/frame stat */
static uint32_t g_dropped_frames;   /* see out_put()'s full-buffer guard, below */

/* Audio APCs the DOOR itself refused, because out_put()'s stage-full guard
 * could not stage all of one (see termgfx_stream_put()). Distinct from the
 * module's own drop counter, which counts chunks it declined to encode.
 *
 * This has a getter AND a trace field on purpose. The reverted priority
 * attempt kept a door-side audio drop counter that was write-only -- never
 * read, never traced -- so a field failure would have printed dropped=0 and
 * sent the next investigation looking in the wrong place. That is the same
 * shape of defect as the g_out_len misreport above: a number that is not the
 * number it claims to be. If the door ever throws audio away, it says so. */
static uint32_t g_audio_dropped;

/* Record a staged run of audio bytes [start, end) in stream coordinates.
 *
 * Coalesces onto the previous segment when the run continues it exactly, which
 * is the common case (the prebuffer release puts every held chunk back to back
 * with nothing between them) and is what keeps the ring from ever filling in
 * practice.
 *
 * On a genuinely full ring, merge the two OLDEST segments into their bounding
 * span rather than dropping one. That OVER-reports the backlog by whatever
 * video sits between them -- deliberately: over-reporting can at worst cost a
 * chunk the module would have kept, while under-reporting is what silently
 * dropped 70 seconds of dialogue. The over-report is also the shortest-lived
 * one available, since the oldest bytes are the next to drain. g_aseg_merges
 * counts it so a trace can never hide it. */
static void aseg_add(uint64_t start, uint64_t end)
{
	int last;
	int slot;

	if (end <= start)
		return;
	if (g_aseg_n > 0) {
		last = (g_aseg_head + g_aseg_n - 1) % TERMGFX_ASEG_MAX;
		if (g_aseg[last].end == start) {
			g_aseg[last].end = end;
			return;
		}
	}
	if (g_aseg_n == TERMGFX_ASEG_MAX) {
		int second = (g_aseg_head + 1) % TERMGFX_ASEG_MAX;

		g_aseg[second].start = g_aseg[g_aseg_head].start;
		g_aseg_head          = second;
		g_aseg_n--;
		g_aseg_merges++;
	}
	slot               = (g_aseg_head + g_aseg_n) % TERMGFX_ASEG_MAX;
	g_aseg[slot].start = start;
	g_aseg[slot].end   = end;
	g_aseg_n++;
}

/* Retire everything the wire has taken. Segments are ordered and never
 * reordered (that is the whole point of the FIFO), so only the HEAD can
 * straddle g_out_base -- clamp that one's start and stop. */
static void aseg_prune(void)
{
	while (g_aseg_n > 0) {
		if (g_aseg[g_aseg_head].end > g_out_base) {
			if (g_aseg[g_aseg_head].start < g_out_base)
				g_aseg[g_aseg_head].start = g_out_base;
			break;
		}
		g_aseg_head = (g_aseg_head + 1) % TERMGFX_ASEG_MAX;
		g_aseg_n--;
	}
}

/* Staged-but-unwritten AUDIO bytes. Prunes first, so every live segment lies
 * wholly at or above g_out_base and the sum is a straight one. */
static size_t aseg_pending(void)
{
	uint64_t tot = 0;
	int      i;

	aseg_prune();
	for (i = 0; i < g_aseg_n; i++) {
		const termgfx_aseg_t *s = &g_aseg[(g_aseg_head + i) % TERMGFX_ASEG_MAX];

		tot += s->end - s->start;
	}
	return (size_t)tot;
}

/* door_io.c's door_out_put() grows an unbounded realloc'd buffer, so it never
 * faces this; ours is a fixed 256KB stage, and a present() call with a big
 * canvas (a large sixel_encode() can run past 256KB by itself once the
 * terminal has advertised a big pixel canvas -- present's own inflight/
 * "prior frame not flushed" gates normally never let out_put() be called
 * with a backlog already queued, but they say nothing about a SINGLE call
 * that overruns the buffer outright) can otherwise spin here forever: once
 * the stage fills, termgfx_termio_flush() is called mid-loop, and if the peer is
 * backpressured (EAGAIN) that flush makes no progress, so the loop would
 * just keep calling it. If a flush truly buys no room, drop the rest of
 * this call's bytes (counted in g_dropped_frames) instead of spinning --
 * skipping a frame under backpressure is the correct door behavior, and a
 * hard write error already gets funneled through g_hangup below (which the
 * post-flush check here returns out of immediately).
 *
 * Returns how many of `n` bytes were actually staged -- always `n` except on
 * the drop paths above. termgfx_stream_put() needs the count for two things the
 * void version could not give it: the exact extent of the audio run to record
 * in g_aseg, and whether the door just threw an audio APC away
 * (g_audio_dropped). Every other caller ignores it, exactly as before. */
static size_t out_put(const void *p, size_t n)
{
	size_t staged = 0;

	if (g_hangup)
		return 0;   /* dead session: don't bother staging bytes nobody will read */
	g_tx_bytes += n;
	while (n) {
		size_t room = sizeof(g_out) - g_out_len;
		size_t take = n < room ? n : room;
		memcpy(g_out + g_out_len, p, take);
		g_out_len += take; p = (const uint8_t *)p + take; n -= take;
		staged += take;
		if (g_out_len == sizeof(g_out)) {
			size_t before = g_out_len;
			termgfx_termio_flush();
			if (g_hangup)
				return staged;
			if (g_out_len == before) {
				g_dropped_frames++;
				return staged;   /* stage stayed full: drop the remainder, don't spin */
			}
		}
	}
	return staged;
}
static void out_puts(const char *s) { out_put(s, strlen(s)); }

/* door_io.c:1567-1577 pattern: EAGAIN/EWOULDBLOCK is backpressure, not an
 * error -- defer (stop for now, keep the unwritten tail buffered for the
 * next call) rather than busy-looping or discarding it. EINTR retries the
 * same write. Any other errno is a dead peer: mark the session hung up
 * (termgfx_termio_hung_up()) and drop the buffer -- there's no live fd left to
 * drain it to.
 *
 * Writes the stage in stage order and only in stage order: this is the FIFO
 * the staging block above describes, and the reason no audio APC can ever land
 * inside a video sequence. Do not add a "write these bytes first" step here.
 *
 * `off` counts bytes LEAVING the stage -- written, or discarded on the dead-
 * peer path -- and so is what advances g_out_base; `sent` counts only bytes a
 * socket actually took, and so is what the tee records. They are equal except
 * on the hard-error path, which moves `off` past a tail nobody will ever see:
 * teeing that tail would make the one capture worth having promise bytes that
 * were never on the wire. */
void termgfx_termio_flush(void)
{
	size_t off  = 0;
	size_t sent = 0;

	if (g_hangup || g_fd < 0 || !g_out_len) {
		/* Nothing can be delivered: the stage's contents are gone either way,
		 * so charge them to g_out_base and let aseg_prune() retire the audio
		 * segments over them. Leaving g_out_base behind would leave those
		 * segments live forever and permanently OVER-report the backlog -- the
		 * mirror of the bug this file is fixing, and just as wrong. */
		g_out_base += g_out_len;
		g_out_len   = 0;
		aseg_prune();
		return;
	}
	while (off < g_out_len) {
		int n = termgfx_plat_write(g_fd, g_out + off, g_out_len - off);
		if (n < 0) {
			if (n == TERMGFX_IO_INTR)
				continue;
			if (n == TERMGFX_IO_AGAIN)
				break;              /* backpressure: preserve the tail, try again later */
			g_hangup = 1;           /* hard write error: dead peer */
			g_quit = 1;
			off = g_out_len;        /* drop the unsent tail, nothing left to send it to */
			break;
		}
		off  += (size_t)n;
		sent += (size_t)n;
	}
	if (g_tee != NULL && sent > 0)
		fwrite(g_out, 1, sent, g_tee);   /* EXACTLY what reached the socket */
	g_out_base += off;
	if (off >= g_out_len)
		g_out_len = 0;
	else if (off > 0) {
		memmove(g_out, g_out + off, g_out_len - off);
		g_out_len -= off;
	}
	aseg_prune();
}

static uint32_t now_ms(void)
{
	/* Monotonic ms via xpdev (CLOCK_MONOTONIC on POSIX,
	 * QueryPerformanceCounter on Windows) -- see termgfx_plat.c. */
	return termgfx_plat_now_ms();
}

/* argv indices resolve_fd() consumed (the -s<fd> flag, a DOOR32.SYS path):
 * main() needs these to build the filtered argv scummvm_main() gets, since
 * it rejects options it doesn't recognize (door/syncscumm.cpp). */
#define TERMGFX_MAX_CONSUMED 8
static int g_consumed_idx[TERMGFX_MAX_CONSUMED];
static int g_consumed_n;

static void mark_consumed(int idx)
{
	if (g_consumed_n < TERMGFX_MAX_CONSUMED)
		g_consumed_idx[g_consumed_n++] = idx;
}

int termgfx_termio_consumed(int idx)
{
	int i;
	for (i = 0; i < g_consumed_n; i++)
		if (g_consumed_idx[i] == idx)
			return 1;
	return 0;
}

/* True iff s is non-empty and every character is an ASCII digit. */
static int all_digits(const char *s)
{
	if (*s == '\0')
		return 0;
	for (; *s != '\0'; ++s)
		if (*s < '0' || *s > '9')
			return 0;
	return 1;
}

/* --- fd resolution (door_io.c:1297-1364 pattern, simplified) --------------
 * Activation order: -s<fd> / a DOOR32.SYS argv (its socket, or -- comm type
 * 0, a stdio door -- default fd 1/0) / <DOOR>_SOCK env; else
 * <DOOR>_SIXELOUT capture; else a real tty on stdout; else headless. */
static int resolve_fd(int argc, char **argv)
{
	int i, door32_seen = 0, cli_sock = -1;
	const char *e;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (a[0] == '-' && a[1] == 's' && a[2] != '\0') {
			cli_sock = atoi(a + 2);                                  /* -s<fd> */
			mark_consumed(i);
		} else if (a[0] == '-' && a[1] == 't' && all_digits(a + 2)) {
			/* -t<seconds>: the session limit, as every sibling door spells it
			 * (Synchronet's %T). Wins over the drop file, which is what the
			 * siblings do too. All-digit suffix so an engine word option
			 * starting with -t passes through. */
			g_time_limit_ms = (uint32_t)atoi(a + 2) * 1000u;
			mark_consumed(i);
		} else if (a[0] == '-' && a[1] == 'i' && all_digits(a + 2)) {
			/* -i<seconds>: the idle threshold. ALL-DIGIT suffix, never a
			 * prefix match -- a game engine's own word option beginning with
			 * -i (DOOM's -iwad is the cautionary case) must pass through. */
			g_idle_arg = atoi(a + 2);
			mark_consumed(i);
		} else if (termgfx_door32_is_path(a)) {
			termgfx_door32_t d;
			door32_seen = 1;
			mark_consumed(i);
			if (termgfx_door32_read(a, &d) == 0) {
				if (d.socket >= 0)
					cli_sock = d.socket;                             /* comm type 2 */
				/* Line 9, the caller's minutes left. -t below overrides it. */
				if (d.time_limit_ms > 0 && g_time_limit_ms == 0)
					g_time_limit_ms = d.time_limit_ms;
			}
			/* comm type 0 (stdio) or a read failure: fall through to the
			 * fd 1/0 default below -- door32_seen alone still activates. */
		}
	}
	if (cli_sock < 0 && (e = getenv(app_env("SOCK"))) != NULL)
		cli_sock = atoi(e);

	/* Bring up the socket layer (WSAStartup on Windows; ignore SIGPIPE on
	 * POSIX) before touching the inherited descriptor. Idempotent. */
	termgfx_plat_net_init();

	if (cli_sock >= 0) {
		g_fd = g_fd_in = cli_sock;       /* comm type 2: DOOR32.SYS socket */
	} else if (door32_seen) {
		g_fd = 1;
		g_fd_in = 0;                     /* stdio door: BBS redirected our stdio */
	} else if ((e = getenv(app_env("SIXELOUT"))) != NULL) {
		g_capture = fopen(e, "wb");
		if (g_capture == NULL)
			fprintf(stderr, "termgfx_termio: %s=%s: %s (falling back to "
			        "headless)\n", app_env("SIXELOUT"), e, strerror(errno));
		return g_capture != NULL;        /* capture mode: no input fd at all */
	} else if (termgfx_plat_isatty(1)) {
		g_fd = 1;
		g_fd_in = 0;
	} else {
		return 0;                        /* headless: no session */
	}

	/* Non-blocking + TCP_NODELAY + large SO_SNDBUF on the live descriptor(s).
	 * On a socket g_fd == g_fd_in, so one call; a POSIX stdio/tty door has two.
	 * The TCP_NODELAY/SNDBUF half is best-effort and no-ops on a tty. */
	termgfx_plat_sock_setup(g_fd);
	if (g_fd_in != g_fd)
		termgfx_plat_sock_setup(g_fd_in);
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
#define TERMGFX_XTVER_CAP 64
static char g_xtver_buf[TERMGFX_XTVER_CAP];
static int  g_xtver_len;

/* Pacing: forward-declared here (defined with the rest of the present-path
 * pacing state, below) so csi_final()'s DSR/CPR ('R') case can fold every
 * reply -- the 999;999 grid probe's included -- into the pipeline-depth
 * bookkeeping (door_io.c:2107-2121 door_pace_ack() pattern). Harmless when
 * the ring is still empty (present() hasn't sent a paced frame yet): the
 * "any DSR outstanding" check inside just skips the RTT sample. */
static void termgfx_pace_ack(void);

/* Hand the streamed-audio module a channel's FIFO-drained notification. A
 * forward declaration because the stream itself is defined past the audio
 * capability state it is built from (which in turn is defined past this
 * parser), and csi_final() is where the notification lands -- see the 'n'
 * case below. */
static void  termgfx_audio_underrun(int ch);

/* SGR mouse report -> game-coordinate input event(s) (defined with the rest
 * of the present-path geometry state, below, since it maps through
 * termgfx_image_rect() -- the same fit+center rect termgfx_termio_present() draws the
 * frame in). csi_final()'s 'M'/'m' cases (parser, above termgfx_tier()) call
 * this as soon as a report's three params are parsed. */
static void termgfx_termio_mouse_report(int b, int col, int row, int release);

/* Keyboard decode (M3 Task 5): forward-declared for the same reason as
 * termgfx_termio_mouse_report just above -- defined with the rest of the input-event
 * FIFO state (needs termgfx_push_event()), but parse_bytes()'s P_NORMAL/P_ESC
 * byte handlers and csi_final()'s CSI/SS3/kitty/evdev dispatch, both defined
 * above that, need to queue key events. termgfx_toggle_stats() is the one
 * exception -- it needs no FIFO, just termgfx_stats_draw()/termgfx_bottom_row(), so
 * it is defined right above parse_bytes() instead; declared here only so the
 * kitty/evdev sections (also above that definition) can reach it too. */
static void termgfx_key_event(int keycode, int ascii, int mods, int down);
static void termgfx_key_press(int keycode, int ascii, int mods);
static void termgfx_key_byte(int c);
static void termgfx_evdev_report(int down);
static void termgfx_toggle_stats(void);

#define TERMGFX_AUDIO_VOLUME_PCT 50    /* default channel level, percent (-> dB via
                                    * termgfx_db_from_pct); 50 = -6 dB, trimmed
                                    * so BASS sits with the other doors */

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

/* --- key-mode state readers + the kitty parameter decode -------------------
 * termgfx (keymode.h) owns the wire protocol and the flags/state; these are
 * thin wrappers over g_km/g_csi_par so csi_final()'s CSI/SS3 dispatch below
 * reads no worse than door_input.c's own kitty_active()/kitty_parse(). */
static int termgfx_kitty_active(void) { return termgfx_keymode_kitty_active(&g_km); }
static int termgfx_evdev_active(void) { return termgfx_keymode_evdev_active(&g_km); }

static int termgfx_termio_kitty_parse(int *mod, int *ev)
{
	return termgfx_kitty_parse(g_csi_par, g_csi_len, mod, ev);
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
			for (k = 0; k < np; k++) {
				if (p[k] == 4)
					g_have_sixel = 1;              /* DA1/CTDA sixel cap */
				/* CTDA cap 8 = physical key (evdev) reports. Prefer it over
				 * kitty (a SyncTERM that advertises it doesn't speak kitty
				 * anyway): enable reports (=1h) and suppress the translated
				 * byte stream (=2h), so keys arrive only as the CSI=<code>
				 * K/k edges termgfx_evdev_report() handles, below. */
				if (p[k] == 8 && g_csi_par[0] == '<') {
					char   ks[TERMGFX_KEYMODE_SEQ_MAX];
					size_t kn = termgfx_keymode_enable_evdev(&g_km, ks, sizeof ks, now_ms());

					if (kn > 0)
						out_put(ks, kn);   /* arms the held-key settle window */
				}
			}
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
			termgfx_pace_ack();
			return;
		case 'n':   /* CTerm state report -- the Q;JXL reply ("ESC[=1;{0,1}n")
		             * and the audio caps replies are parsed separately by the
		             * raw-byte scanners (jxl_scan_feed(), audio_scan_feed()).
		             * What lands HERE is the runtime audio idle notification,
		             * which unlike those is not a probe answer and so has no
		             * accumulator to scan: it arrives whenever a channel's
		             * FIFO happens to drain, long after the scanners have
		             * latched and stopped looking. */
			g_probe_replied = 1;
			if (g_csi_len > 0 && g_csi_par[0] == '=') {
				np = csi_params(p, 8);
				/* `CSI = 7 ; <ch> ; 0 n`: that channel's FIFO drained. The
				 * audio CAPS reply shares the "=7" prefix
				 * ("ESC[=7;100;<tier>n"), and feature id 100 is reserved for
				 * it, so without the p[1] != 100 guard a caps reply would be
				 * DISPATCHED here as an underrun on channel 100. The guard
				 * stops that dispatch; even without it, the module would
				 * still reject the notification, since
				 * termgfx_stream_underrun() ignores any channel that isn't
				 * the stream's own (cfg.ch is 2). Defense in depth, not the
				 * only thing standing between this and a phantom underrun. */
				if (np >= 3 && p[0] == 7 && p[1] != 100 && p[2] == 0)
					termgfx_audio_underrun(p[1]);
			}
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
		case 'M': case 'm': {            /* xterm SGR mouse: ESC[<b;col;row M/m */
			int q[3];
			if (csi_params(q, 3) >= 3)
				termgfx_termio_mouse_report(q[0], q[1], q[2], fin == 'm');
			return;
		}
		case 'y': {                      /* DECRPM: ESC[?1016;Ps$y */
			np = csi_params(p, 4);
			termgfx_mouse_on_decrpm(&g_mouse, p, np);
			return;
		}
		/* --- keyboard: legacy CSI/SS3 nav keys, kitty CSI-u, SyncTERM
		 * evdev physical-key reports. Structure ported from
		 * syncconquer/door/door_input.c's csi_final(), trimmed to this
		 * door's neutral TERMGFX_KEY_* events. --------------------------------- */
		case 'A': case 'B': case 'C': case 'D': {   /* arrows */
			int keycode = (fin == 'A') ? TERMGFX_KEY_UP : (fin == 'B') ? TERMGFX_KEY_DOWN
			            : (fin == 'C') ? TERMGFX_KEY_RIGHT : TERMGFX_KEY_LEFT;

			if (termgfx_kitty_active()) {
				int mod, ev;
				termgfx_termio_kitty_parse(&mod, &ev);
				(void)mod;   /* arrows carry no ctrl/alt action -- only the event type matters */
				termgfx_key_event(keycode, 0, 0, ev != 3);
			} else
				termgfx_key_press(keycode, 0, 0);
			return;
		}
		case 'H': termgfx_key_press(TERMGFX_KEY_HOME, 0, 0); return;
		case 'F': termgfx_key_press(TERMGFX_KEY_END,  0, 0); return;
		case 'K':   /* evdev press report (CSI = code[;code...] K), else cterm End */
			if (termgfx_evdev_active() && g_csi_len > 0 && g_csi_par[0] == '=') {
				termgfx_evdev_report(1);
				return;
			}
			termgfx_key_press(TERMGFX_KEY_END, 0, 0);
			return;
		case 'k':   /* evdev release report */
			if (termgfx_evdev_active() && g_csi_len > 0 && g_csi_par[0] == '=')
				termgfx_evdev_report(0);
			return;
		/* F1/F2 (CSI P/Q or SS3 O P/Q). F3/F4 (SS3 O R/O S) are deliberately
		 * NOT mapped here -- 'R' and 'S' already mean this file's own
		 * CPR/DSR and XTSMGRAPHICS replies (the same collision door_input.c
		 * documents for F3); F3/F4 still work via kitty CSI-u or evdev
		 * (codes 61/62). */
		case 'P': case 'Q': {
			int keycode = (fin == 'P') ? TERMGFX_KEY_F1 : TERMGFX_KEY_F2;

			if (termgfx_kitty_active()) {
				int mod, ev;
				termgfx_termio_kitty_parse(&mod, &ev);
				(void)mod;   /* F1/F2 carry no ctrl/alt action -- only the event type matters */
				termgfx_key_event(keycode, 0, 0, ev != 3);
			} else
				termgfx_key_press(keycode, 0, 0);
			return;
		}
		case '~': {   /* nav/F-key numerics: ESC[<n>~ */
			int mod = 1, ev = 1, keycode = 0;

			if (termgfx_kitty_active())
				termgfx_termio_kitty_parse(&mod, &ev);
			(void)mod;   /* nav/F-keys carry no ctrl/alt action -- only the event type matters */
			np = csi_params(p, 1);
			if (np < 1)
				return;
			switch (p[0]) {
				case 1: case 7:  keycode = TERMGFX_KEY_HOME;     break;
				case 2:          keycode = TERMGFX_KEY_INSERT;   break;
				case 3:          keycode = TERMGFX_KEY_DELETE;   break;
				case 4: case 8:  keycode = TERMGFX_KEY_END;      break;
				case 5:          keycode = TERMGFX_KEY_PAGEUP;   break;
				case 6:          keycode = TERMGFX_KEY_PAGEDOWN; break;
				case 11:         keycode = TERMGFX_KEY_F1;       break;
				case 12:         keycode = TERMGFX_KEY_F2;       break;
				case 13:         keycode = TERMGFX_KEY_F3;       break;
				case 14:         keycode = TERMGFX_KEY_F4;       break;
				case 15:         keycode = TERMGFX_KEY_F5;       break;
				case 17:         keycode = TERMGFX_KEY_F6;       break;
				case 18:         keycode = TERMGFX_KEY_F7;       break;
				case 19:         keycode = TERMGFX_KEY_F8;       break;
				case 20:         keycode = TERMGFX_KEY_F9;       break;
				default:         return;
			}
			if (termgfx_kitty_active())
				termgfx_key_event(keycode, 0, 0, ev != 3);
			else
				termgfx_key_press(keycode, 0, 0);
			return;
		}
		case 'u':
			/* CSI?u negotiation reply: enable the protocol and push our
			 * flags (1 disambiguate | 2 event types | 8 all keys incl.
			 * modifiers). termgfx (keymode.h) owns the push itself and the
			 * evdev-wins guard. */
			if (g_csi_len > 0 && g_csi_par[0] == '?') {
				char   ks[TERMGFX_KEYMODE_SEQ_MAX];
				size_t kn = termgfx_keymode_enable_kitty(&g_km, ks, sizeof ks);

				if (kn > 0)
					out_put(ks, kn);   /* a no-op if evdev already won */
				return;
			}
			{
				int mod, ev, cp = termgfx_termio_kitty_parse(&mod, &ev);
				int down, keycode, ascii = 0, mods = 0;

				if (cp <= 0)
					return;
				down = (ev != 3);
				/* NumLock OFF: the keypad reports its navigation function
				 * set (Home/End/arrows/PgUp/PgDn/Ins/Del) -- map those to
				 * the nav keys so the keypad respects NumLock. */
				switch (cp) {
					case 57417: termgfx_key_event(TERMGFX_KEY_LEFT,     0, 0, down); return;
					case 57418: termgfx_key_event(TERMGFX_KEY_RIGHT,    0, 0, down); return;
					case 57419: termgfx_key_event(TERMGFX_KEY_UP,       0, 0, down); return;
					case 57420: termgfx_key_event(TERMGFX_KEY_DOWN,     0, 0, down); return;
					case 57421: termgfx_key_event(TERMGFX_KEY_PAGEUP,   0, 0, down); return;
					case 57422: termgfx_key_event(TERMGFX_KEY_PAGEDOWN, 0, 0, down); return;
					case 57423: termgfx_key_event(TERMGFX_KEY_HOME,     0, 0, down); return;
					case 57424: termgfx_key_event(TERMGFX_KEY_END,      0, 0, down); return;
					case 57425: termgfx_key_event(TERMGFX_KEY_INSERT,   0, 0, down); return;
					case 57426: termgfx_key_event(TERMGFX_KEY_DELETE,   0, 0, down); return;
					case 57427: termgfx_key_event(TERMGFX_KEY_KP5,      0, 0, down); return;   /* KP_BEGIN center: AGI stop */
				}
				if (cp >= 57399 && cp <= 57414) {   /* NumLock ON: keypad PUA -> ASCII */
					static const char kp[] = "0123456789./*-+\r";
					cp = (unsigned char)kp[cp - 57399];
				}
				if (cp >= 128)
					return;   /* other non-ASCII special key (incl. the
					           * modifier-key's own PUA event, 57441+): not
					           * in our map */
				/* Door-level Ctrl-S (stats) and Ctrl-Q/Ctrl-C (quit)
				 * hotkeys stay reserved under kitty too. A bare, unmodified
				 * letter -- including 'q' -- is NOT reserved here: it falls
				 * through below and is forwarded to ScummVM like any other
				 * key, since Global Main Menu save-name entry needs to be
				 * able to type a 'q' (e.g. "quicksave"). */
				if (termgfx_kitty_ctrl(mod)) {
					if ((cp | 0x20) == 's') {
						if (down)
							termgfx_toggle_stats();
						return;
					}
					if ((cp | 0x20) == 'c' || (cp | 0x20) == 'q') {
						if (down)
							g_quit = 1;
						return;
					}
					/* Ctrl+<menu_letter> (default Ctrl-G): open the ScummVM
					 * Global Main Menu -- intercepted here so it reaches the
					 * engine as EVENT_MAINMENU, not the letter. Off when
					 * g_menu_letter == 0 (sysop "[input] menu_key = off"). */
					if (g_menu_letter && (cp | 0x20) == g_menu_letter) {
						if (down)
							g_menu = 1;
						return;
					}
				}
				if (cp == 0x0d)
					keycode = TERMGFX_KEY_ENTER;
				else if (cp == 0x08 || cp == 0x7f)
					keycode = TERMGFX_KEY_BACKSPACE;
				else if (cp == 0x09)
					keycode = TERMGFX_KEY_TAB;
				else if (cp == 0x1b)
					keycode = TERMGFX_KEY_ESCAPE;
				else if (cp >= 0x20 && cp <= 0x7e) {
					keycode = cp;
					ascii   = cp;
				} else
					return;
				if (termgfx_kitty_ctrl(mod))
					mods |= TERMGFX_MOD_CTRL;
				if (termgfx_kitty_shift(mod))
					mods |= TERMGFX_MOD_SHIFT;
				if (termgfx_kitty_alt(mod))
					mods |= TERMGFX_MOD_ALT;
				termgfx_key_event(keycode, ascii, mods, down);
			}
			return;
		default:
			return;
	}
}

#define TERMGFX_ACC_CAP 4096
static uint8_t  g_acc[TERMGFX_ACC_CAP];
static size_t   g_acc_len;
static uint32_t g_probe_start_ms;
static uint32_t g_first_present_ms;   /* first present() that reached the holds */
static int      g_jxl_done;

/* ---- audio capability state (M4) ----
 * g_audio_tier mirrors termgfx_audio_parse_caps(): -1 none/unknown (the
 * terminal never answered, i.e. no audio APC at all), 0 tone-only (audio
 * APC but no libsndfile, so A;Load is a no-op and a Queue would play an
 * empty slot), 1 digital. g_audio_done latches "the question is answered"
 * so termgfx_termio_audio_available()'s bounded wait can exit the instant the
 * reply lands instead of burning its whole window -- the same trick
 * g_jxl_done plays for the graphics settle (see jxl_scan_feed()). */
static int g_audio_tier = -1;
static int g_audio_done;
static int g_opus_ok = 1;       /* -1 from parse_opus means "no reply": an
                                 * older SyncTERM that never answers, so
                                 * assume yes, as termgfx/audio.h:41-51 says */
static int g_opus_asked;
static int g_opus_seen;         /* the Opus reply actually landed (vs. the
                                 * assume-yes default above) -- read by
                                 * acc_shrink()'s guard, which must not drop
                                 * a tail the Opus parser still needs */

/* Sysop sound switch, <door>.ini's "[audio] enabled" key. THE one copy of
 * that key in this file: read once, at init (termgfx_read_ini(), below), and read
 * back by both places that care -- termgfx_termio_audio_available() right below, and
 * audio_stream_open()'s cfg.enabled. Deliberately NOT two reads of the same
 * file: both answer the same session the same question ("will sound play?"),
 * and any shape where the two could disagree is a shape where the door mutes
 * the stream while still telling the subtitle logic the player can hear it --
 * which is exactly the defect termgfx_termio_audio_available() documents.
 *
 * Defaults to 1 (sound on), so a missing file, a missing key, and a headless
 * session that never reads an ini at all all behave as they always have. */
static int g_audio_enabled = 1;

/* The graphics settle window (the JXL reply's deadline), hoisted here from
 * the present-path constants below: termgfx_termio_audio_available()'s bounded wait
 * is defined in terms of the same window and is defined above them. */
#define TERMGFX_GFX_SETTLE_MS  2000   /* JXL reply window (matches jxl_scan_feed) */

/* Append raw input to the shared scan accumulator. Split out of
 * jxl_scan_feed() for M4, when the audio replies became a second scanner
 * over the SAME growing buffer: the two scanners stop parsing at different
 * moments, so neither can own the append. Both appending would double every
 * byte, and leaving it to the JXL scanner would starve the audio scanner the
 * moment the JXL answer landed first -- the ordinary case, since the JXL
 * query precedes the audio query in the burst. termgfx_termio_pump() calls this once
 * per read, ahead of both scanners.
 *
 * Unlike jxl_scan_feed()'s own early return once it has latched, this append
 * (and acc_shrink() below) run on EVERY read for the rest of the session --
 * there is no "all scanners are done, stop touching the buffer" gate at this
 * level. That is a real behavior change from the pre-M4 shape, where the
 * scan and the append were the same function and its early return silenced
 * both; correctness is unaffected (a stray keystroke just gets memcpy'd into
 * the tail and immediately shrunk back out), and the cost is negligible next
 * to the frame path. Same story for a tier-1 terminal that never answers the
 * Opus query: parse_opus() re-scans the 256-byte window on every read for
 * the session's life, equally negligible. */
static void acc_append(const uint8_t *buf, size_t n)
{
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
}

/* Shrink the accumulator to a 256-byte tail once every scanner that reads it
 * has its answer (or the 2s deadline passes), so the buffer doesn't keep
 * growing for the rest of the session. The audio conditions are new (M4):
 * without them the accumulator could drop the tail an audio reply still
 * needed. The Opus stage counts only if it was ever asked -- a tone-only or
 * silent terminal never asks. */
static void acc_shrink(void)
{
	if ((g_jxl_done && g_probe_replied && g_audio_done
	     && (!g_opus_asked || g_opus_seen))
	    || (int32_t)(now_ms() - g_probe_start_ms) > TERMGFX_GFX_SETTLE_MS) {
		if (g_acc_len > 256) {
			memmove(g_acc, g_acc + (g_acc_len - 256), 256);
			g_acc_len = 256;
		}
	}
}

/* JXL reply via termgfx_caps_parse_jxl() over the raw accumulated buffer --
 * its reply ("ESC[=1;{0,1}-...n") carries a '-' the CSI dispatcher above
 * can't cleanly reconstruct, so this is fed the growing raw byte stream
 * instead (caps.h: "idempotent over a growing buffer"). */
static void jxl_scan_feed(void)
{
	int r;

	if (g_jxl_done && g_probe_replied)
		return;

	r = termgfx_caps_parse_jxl(g_acc, (int)g_acc_len);
	if (r >= 0) {
		g_jxl = (r == 1);
		g_is_syncterm = 1;
		g_jxl_done = 1;
	}
}

/* Scan the accumulated input for the audio capability replies. Mirrors
 * jxl_scan_feed(): termgfx_audio_parse_caps()/_parse_opus() have the same
 * idempotent-over-a-growing-buffer contract as termgfx_caps_parse_jxl().
 *
 * TWO-STAGE, deliberately: parse_caps only reports whether libsndfile is
 * present at all, not whether it decodes Ogg-Opus -- our codec, which older
 * distro libsndfile builds lack. So the Opus query goes out only once the
 * digital tier is confirmed, and a terminal that never answers it keeps the
 * historical assume-yes (termgfx/audio.h:41-51). */
static void audio_scan_feed(void)
{
	int r;

	if (g_audio_done && (!g_opus_asked || g_opus_seen))
		return;
	if (!g_audio_done) {
		r = termgfx_audio_parse_caps(g_acc, (int)g_acc_len);
		if (r >= 0) {
			g_audio_tier  = r;
			g_audio_done  = 1;
			g_is_syncterm = 1;   /* only SyncTERM answers this */
			if (r >= 1 && !g_opus_asked) {
				out_puts(termgfx_audio_opus_query);
				termgfx_termio_flush();
				g_opus_asked = 1;
			}
		}
	}
	if (g_opus_asked) {
		r = termgfx_audio_parse_opus(g_acc, (int)g_acc_len);
		if (r >= 0) {
			g_opus_ok   = r;
			g_opus_seen = 1;
		}
	}
}

/* The terminal's bottom text row: the real grid from the 999;999 CPR, else a
 * cell-height guess. Shared by the stats bar and its erase-on-hide. */
static int termgfx_bottom_row(void)
{
	if (g_term_rows > 0)
		return g_term_rows;
	if (g_canvas_h > 0)
		return g_canvas_h / (g_cell_h > 0 ? g_cell_h : 16);
	return 25;
}

/* Ctrl-S: toggle the stats bar. Draw/erase it NOW, not on the next present --
 * a static/deduped scene may not present again for a while, so waiting would
 * make the toggle appear to do nothing. Shared by the P_NORMAL byte path and
 * the kitty/evdev decode (csi_final()'s 'u'/'K' cases, above), so Ctrl-S
 * stays a door hotkey under every key mode. */
static void termgfx_toggle_stats(void)
{
	g_stats = !g_stats;
	if (g_stats) {
		termgfx_stats_draw();
		termgfx_termio_flush();
	} else {
		/* Erase the bar we last drew; it sits on the reserved bottom row,
		 * so clearing the line is enough. */
		char e[32];
		int  en = snprintf(e, sizeof e, "\x1b[%d;1H\x1b[0m\x1b[K\x1b[?25l",
		                   termgfx_bottom_row());
		out_put(e, (size_t)en);
		termgfx_termio_flush();
	}
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
				else if (c == 0x13)                     /* Ctrl-S: toggle stats bar */
					termgfx_toggle_stats();
				else if (c == 'q' || c == 0x03)          /* q / Ctrl-C: request quit */
					g_quit = 1;
				else if (g_menu_letter && c == (uint8_t)(g_menu_letter - 'a' + 1))
					g_menu = 1;                          /* Ctrl+<menu_letter>: open GMM */
				else
					termgfx_key_byte(c);
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
					/* Not a CSI/APC introducer: the pending ESC was a lone
					 * Escape key, not the start of a sequence. */
					termgfx_key_press(TERMGFX_KEY_ESCAPE, 0, 0);
					i--;                 /* reprocess c as P_NORMAL
					                      * (door_io.c:3289-3293 pattern) */
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
					if (g_apc_kind == 'P' && g_xtver_len < TERMGFX_XTVER_CAP)
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

/* Sysop sixel_max override: <door>.ini's root-section "sixel_max" key, an
 * integer pixel ceiling applied to BOTH axes (0/absent -- the default -- means
 * no override, see g_sixel_max_override's doc comment above). Read relative
 * to CWD, the door's launch dir -- the same file, same relative fopen(), and
 * same point in the startup sequence (called from termgfx_termio_init(), itself the
 * very first thing main() does -- e.g. door/syncscumm.cpp) as that door's
 * resolveSubtitles() reads its own "subtitles" key from, so no chdir has run
 * between the two reads. A missing file or missing key is not an error --
 * iniGetInteger()'s default (0) is exactly "no override". */
static void termgfx_read_ini(void)
{
	FILE *f = fopen(app_ini(), "r");
	str_list_t ini;

	if (f == NULL)
		return;
	ini = iniReadFile(f);
	fclose(f);
	if (ini == NULL)
		return;
	g_sixel_max_override = iniGetInteger(ini, ROOT_SECTION, "sixel_max", 0);
	/* [idle] -- iniGetDuration() so "15m"/"900"/"1h" all parse, and so a bare
	 * number means SECONDS here exactly as it does in the sibling doors and in
	 * the JS lobby that may pass -i. Read at init, well before the clock is
	 * armed on first input. */
	g_idle_timeout = (unsigned)iniGetDuration(ini, "idle", "timeout",
	                                          TERMGFX_IDLE_DEFAULT);
	g_idle_warn    = (unsigned)iniGetDuration(ini, "idle", "warn", 60);
	/* Read HERE, at init, rather than with the rest of the [audio] section in
	 * audio_read_ini(): that one runs on the first PCM block, and this key is
	 * needed far earlier -- termgfx_termio_audio_available() is asked during
	 * initBackend(), before the engine has produced a single sample. The
	 * other [audio] keys are tuning values the stream's config struct owns
	 * and nothing outside it reads, so they stay where the struct is built;
	 * this one is a session-wide fact two callers share, so it lives with
	 * the session state. audio_stream_open() takes its cfg.enabled from
	 * g_audio_enabled -- see the note there. */
	g_audio_enabled = iniGetBool(ini, "audio", "enabled", g_audio_enabled);
	strListFree(&ini);
}

int termgfx_termio_init(int argc, char **argv)
{
	app_name_from_argv0(argc > 0 ? argv[0] : NULL);

	/* Before resolve_fd()'s headless early-return below, deliberately: the
	 * mixer produces PCM whether or not a terminal is listening, and the only
	 * caller that wants this file is a headless test run. See g_audiodump. */
	{
		const char *ap = getenv(app_env("AUDIODUMP"));

		if (ap != NULL && *ap != '\0') {
			g_audiodump = fopen(ap, "wb");
			if (g_audiodump == NULL)
				fprintf(stderr, "termgfx_termio: %s=%s: %s (no audio "
				        "dump)\n", app_env("AUDIODUMP"), ap, strerror(errno));
		}
	}

	/* Diagnostic stderr capture (off by default): on a BBS-launched door the
	 * process's stderr is dead, so ScummVM's own diagnostics -- including a
	 * fatal error() an engine prints just before it aborts -- go nowhere.
	 * Touch-file gated like the trace/wirecap gates below: create the file
	 * <data-dir>/<door>/stderr (SBBSDATA for a real door; dev/standalone
	 * fallback ./<door>-stderr) to redirect stderr to a durable file.
	 * Placed before resolve_fd()'s headless early-return so it also covers a
	 * boot test. termgfx_plat_redirect_stderr() keeps the platform #ifdef out of
	 * this file (see termgfx_plat.h). */
	{
		const char *data_dir = getenv("SBBSDATA");
		char        touch[512];

		if (data_dir != NULL && *data_dir != '\0')
			snprintf(touch, sizeof touch, "%s/%s/stderr", data_dir, app_name());
		else
			snprintf(touch, sizeof touch, "./%s-stderr", app_name());   /* dev/standalone */

		if (termgfx_plat_file_exists(touch)) {
			char path[64];

			snprintf(path, sizeof path, "/tmp/%s.%ld.stderr", app_name(), termgfx_plat_getpid());
			termgfx_plat_redirect_stderr(path);
		}
	}

	if (!resolve_fd(argc, argv))
		return 0;
	g_active = 1;

	termgfx_read_ini();

	/* Env-var gate (test scripts): explicit path takes precedence. */
	{
		const char *tp = getenv(app_env("TRACE"));
		if (tp != NULL && *tp != '\0') {
			g_trace = fopen(tp, "w");
			if (g_trace != NULL)
				setvbuf(g_trace, NULL, _IOLBF, 0);
		}
	}

	/* Touch-file gate (BBS-launched doors): env vars don't survive execvp.
	 * Enabled when the operator creates the touch-file <data-dir>/<door>/
	 * trace (SBBSDATA for a real door; dev/standalone fallback ./<door>-
	 * trace). Env var above takes precedence if both are set. */
	if (g_trace == NULL) {
		const char *data_dir = getenv("SBBSDATA");
		char        touch[512];

		if (data_dir != NULL && *data_dir != '\0')
			snprintf(touch, sizeof touch, "%s/%s/trace", data_dir, app_name());
		else
			snprintf(touch, sizeof touch, "./%s-trace", app_name());   /* dev/standalone */

		if (termgfx_plat_file_exists(touch)) {
			char path[64];

			snprintf(path, sizeof path, "/tmp/%s.%ld.trace", app_name(), termgfx_plat_getpid());
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
			snprintf(touch, sizeof touch, "%s/%s/wirecap", data_dir, app_name());
		else
			snprintf(touch, sizeof touch, "./%s-wirecap", app_name());   /* dev/standalone */

		if (termgfx_plat_file_exists(touch)) {
			char path[64];

			snprintf(path, sizeof path, "/tmp/%s.%ld.raw", app_name(), termgfx_plat_getpid());
			g_tee = fopen(path, "wb");
			if (g_tee != NULL)
				setvbuf(g_tee, NULL, _IOFBF, 1 << 18);   /* 256KB, fully buffered */
		}
	}

	/* Once, on entry: clear+home, hide cursor, no autowrap, reset sixel
	 * scrolling, hide the status line, XTVERSION (xterm identification --
	 * see g_is_xterm's doc comment), probe the terminal's pixel canvas,
	 * DA1+CTDA (sixel/SyncTERM detect), Q;JXL (JXL detect). termgfx_termio_pump()'s
	 * parser resolves the replies (door_io.c:2738-2770, minus audio/mouse).
	 *
	 * ORDER MATTERS: the XTVERSION query goes out BEFORE term_probe's
	 * ESC[14t canvas query and 999;999 CPR. A terminal answers queries in
	 * the order it receives them, and termgfx_termio_present()'s startup holds
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
	/* SGR mouse: motion tracking (1003) + SGR encoding (1006) + SGR-Pixels
	 * (1016), the last also DECRQM-probed so csi_final()'s 'y' case can latch
	 * per-pixel precision the moment the reply lands rather than waiting on
	 * termgfx_termio_mouse_report()'s past-the-grid auto-detect (termgfx_mouse_enable
	 * itself sends the DECRQM query). Nothing here gates present() the way
	 * DA1/JXL do -- a terminal that ignores it just never sends reports. */
	out_puts(termgfx_mouse_enable);
	/* Kitty keyboard-protocol query ("does this terminal speak CSI-u?"); a
	 * '?' reply enables it (csi_final()'s 'u' case, above). SyncTERM's own
	 * evdev physical-key reports are enabled instead, when eligible, off
	 * the CTDA reply's cap 8 (csi_final()'s 'c' case) -- evdev wins over
	 * kitty (termgfx/keymode.h); a terminal that answers neither just keeps
	 * sending plain bytes, decoded in parse_bytes()'s P_NORMAL case. */
	out_puts(termgfx_keymode_query_kitty);
	/* Audio caps (SyncTERM:Q;libsndfile). Tail of the burst: nothing gates
	 * on its absence, and the XTVERSION-before-canvas invariant above
	 * constrains only those two. The Opus query is NOT sent here -- it is
	 * two-stage (audio_scan_feed(), above). */
	out_puts(termgfx_audio_query);
	termgfx_termio_flush();
	g_probe_start_ms = now_ms();
	return 1;
}

void termgfx_termio_shutdown(void)
{
	/* Before the !g_active guard, mirroring the open in termgfx_termio_init(): a
	 * headless run never set g_active, and it is the run that has a dump file
	 * to close. See g_audiodump. */
	if (g_audiodump != NULL) {
		fclose(g_audiodump);
		g_audiodump = NULL;
	}
	/* Ahead of the !g_active guard like the dump above, though for a weaker
	 * reason: only an active session ever reaches audio_apply_headroom(), so
	 * today this could sit below the guard just as well. It sits above it
	 * because freeing a buffer needs no session to be true, and the guard is
	 * the one line in this function that a future edit might make some other
	 * path skip. */
	audio_free_scratch();
	if (!g_active)
		return;
	/* Before the terminal restore, deliberately: the stop is an A;Flush with
	 * no fade, and it has to land while we are still the thing writing to
	 * this terminal. Anything left in the channel's FIFO would otherwise play
	 * on over the BBS prompt we are about to hand back to
	 * (syncretro/main.c:598). */
	termgfx_termio_audio_stop();
	if (g_tee != NULL) {
		fclose(g_tee);
		g_tee = NULL;
	}
	if (g_capture != NULL) {
		fclose(g_capture);
		g_capture = NULL;
	} else {
		/* Undo the mouse tracking modes negotiated at entry, before handing
		 * the terminal back to the BBS prompt -- it never asked to be left in
		 * SGR/SGR-Pixels motion-reporting mode. */
		out_puts(termgfx_mouse_restore);
		/* Undo whichever key mode was negotiated (kitty or evdev) at entry;
		 * a no-op (0 bytes) if neither ever engaged. */
		{
			char   ks[TERMGFX_KEYMODE_SEQ_MAX];
			size_t kn = termgfx_keymode_restore(&g_km, ks, sizeof ks);

			if (kn > 0)
				out_put(ks, kn);
		}
		/* Restore the status line hidden at entry (door_io.c:1233-1238 pattern):
		 * the captured pre-door DECSSDT type, or the usual indicator default
		 * (type 1) if the DECRQSS reply never came back. */
		char   sb[8];
		size_t sn = termgfx_term_status_set(sb, sizeof sb, g_status_type >= 0 ? g_status_type : 1);
		out_put(sb, sn);
		out_puts(termgfx_term_leave);
		termgfx_termio_flush();
	}
	if (g_trace != NULL) {
		fclose(g_trace);
		g_trace = NULL;
	}
	g_active = 0;
}

int termgfx_termio_active(void) { return g_active; }
int termgfx_termio_hung_up(void) { return g_hangup; }

void termgfx_termio_pump(void)
{
	uint8_t buf[256];

	idle_poll();       /* rendering-independent: see idle_poll()'s comment */
	deadline_poll();   /* ...and it must run second; see deadline_poll() */

	/* Deliberately NOT gated on g_active: termgfx_termio_shutdown() only retires the
	 * OUTPUT side (status-line/term_leave + "don't stage more bytes"); the fd
	 * itself is still whatever it was, and a caller that pumps once more
	 * after shutdown should still learn the peer is gone rather than get a
	 * silent no-op. Capture mode and headless (no input fd) are the only
	 * real no-read cases. */
	if (g_capture != NULL || g_fd_in < 0)
		return;   /* headless / capture mode (no client to read from) */

	for (;;) {
		int n = termgfx_plat_read(g_fd_in, buf, sizeof buf);
		if (n < 0) {
			if (n == TERMGFX_IO_AGAIN || n == TERMGFX_IO_INTR)
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

		termgfx_trace_in(buf, (int)n);
		acc_append(buf, (size_t)n);
		jxl_scan_feed();
		audio_scan_feed();
		acc_shrink();
		parse_bytes(buf, (int)n);
	}
}

int termgfx_termio_quit_requested(void) { return g_quit; }
/* One-shot: returns (and clears) true once per Ctrl+<menu_letter> press. */
int  termgfx_termio_menu_requested(void) { int m = g_menu; g_menu = 0; return m; }
/* Enable the GMM hotkey on Ctrl+<letter> (lowercase a-z); 0 disables it. */
void termgfx_termio_set_menu_key(int letter) { g_menu_letter = (letter >= 'a' && letter <= 'z') ? (unsigned char)letter : 0; }
int termgfx_termio_have_sixel(void)     { return g_have_sixel; }
int termgfx_termio_is_syncterm(void)    { return g_is_syncterm; }
int termgfx_termio_jxl_supported(void)  { return g_jxl; }
int termgfx_termio_stats_visible(void)  { return g_stats; }
unsigned termgfx_termio_frames_dropped(void) { return g_dropped_frames; }

unsigned termgfx_termio_audio_dropped(void) { return g_audio_dropped; }

/* Total staged bytes not yet written to the socket, audio and video alike.
 * Stats/test introspection ONLY -- do NOT hand this to the streaming module.
 * Doing exactly that is what dropped 70 seconds of Beneath a Steel Sky's
 * dialogue: see the staging block at the top of this file. Unlike syncretro's
 * equivalent there is no g_out_off to subtract: termgfx_termio_flush() memmoves the
 * tail down. */
size_t termgfx_termio_out_backlog(void) { return g_out_len; }

/* Staged AUDIO bytes not yet written to the socket -- audio's own share of the
 * shared FIFO, and the ONLY backlog the streaming module is entitled to see.
 * Its two-strike rule (termgfx/audio_stream.h) exists to spot a link that
 * cannot carry AUDIO, so this must not answer with video's bursts: at 20 KB/s
 * of speech, crossing the module's 48KB means ~2.5s of undelivered audio, i.e.
 * a link that genuinely cannot carry it. NOT an instantaneous socket check,
 * which SyncDOOM tried for SFX and reverted, because the frame path keeps the
 * socket busy essentially always.
 *
 * Not const-clean: aseg_pending() prunes retired segments as it sums. That is
 * a cache, not an observation -- the answer is identical either way. */
size_t termgfx_termio_audio_backlog(void) { return aseg_pending(); }

/* Does this session have working audio? Drives two decisions: the Talkie/
 * Floppy data-set pick in main() (door/syncscumm.cpp, before scummvm_main()
 * runs) and the subtitles auto-decision in resolveSubtitles() (called from
 * initBackend()): no audio -> Floppy / subtitles on.
 *
 * This is the ONE bounded wait in this file that must actually block. Every
 * other one (the JXL settle at termgfx_termio_present(), the canvas hold) is a
 * poll-and-return: ScummVM's loop calls present() again, so "hold" means
 * "return and get asked again". This question is now asked from TWO call
 * sites -- main()'s data-set selection, which runs first, right after
 * termgfx_termio_init() and before the engine starts or a single frame is drawn;
 * and resolveSubtitles() in initBackend(), which runs later during engine
 * boot. Both trust the g_audio_tier/g_audio_done/g_opus_ok latch below: the
 * first call pumps until the answer latches or the window expires, and the
 * second call finds g_audio_done already set and returns instantly with the
 * SAME value -- no second wait, and the two callers can never disagree.
 * Returning "no audio" merely because the reply had not landed yet would
 * bake subtitles on (and pick Floppy) for the whole session.
 *
 * So: pump until the answer latches or the window expires. Anchored on
 * g_probe_start_ms, which termgfx_termio_init() sets once before either caller runs
 * -- NOT on first present, unlike present()'s canvas hold, which had to move
 * its anchor because engine boot outlasts the grace. Here both callers fall
 * inside (or right after) that same init-anchored window, so init is the
 * right anchor and the wait is bounded by what is left of it.
 *
 * Costs up to the full ~2s settle window once, at boot, on a terminal that
 * never answers (Foot, xterm) -- main()'s data-set selection is the first
 * caller and runs right after termgfx_termio_init(), so g_probe_start_ms is
 * anchored right at the top of the burst and a silent terminal pays close
 * to the whole window on that first call; resolveSubtitles()'s later call
 * costs nothing, since the latch is already set. That is today's behavior
 * anyway: the wait expires, subtitles auto-on. A headless/capture session
 * has no input fd, never probes, and returns 0 immediately.
 */
int termgfx_termio_audio_available(void)
{
	if (!g_active || g_fd_in < 0 || g_hangup)
		return 0;   /* headless/capture (no input fd) or dead: never any audio */
	/* The sysop turned sound off, so nothing will play no matter what the
	 * terminal can decode -- and the honest answer to "does this session have
	 * working audio" is therefore no, before the terminal is even consulted.
	 *
	 * This question used to be read as a property of the TERMINAL, which cost
	 * a real defect: a sysop who set "[audio] enabled = false" on a SyncTERM
	 * got no sound (they asked for that) AND no subtitles (auto saw a capable
	 * terminal and switched them off), so a CD talkie's dialogue -- Beneath a
	 * Steel Sky's comic intro carries no text data at all -- was neither
	 * heard nor seen. The question means "will this player HEAR audio", and
	 * the sysop's switch is half of that answer.
	 *
	 * Returning here rather than after the wait keeps the subtitles
	 * precedence (user > sysop > auto, resolveSubtitles() in
	 * door/syncscumm.cpp) exactly as it was -- that caller is unchanged, and
	 * an explicit user or sysop "subtitles" preference still never reaches
	 * this function. Only "auto" asks.
	 *
	 * Ahead of the wait loop, deliberately: the loop below exists to collect
	 * a capability reply, and there is no reason to spend up to the whole
	 * settle window (a once-per-session boot cost the player waits through)
	 * on an answer already decided. */
	if (!g_audio_enabled)
		return 0;
	while (!g_audio_done
	       && (int32_t)(now_ms() - g_probe_start_ms) < TERMGFX_GFX_SETTLE_MS) {
		termgfx_termio_pump();
		if (g_hangup)
			return 0;
		if (g_audio_done)
			break;
		termgfx_plat_sleep_ms(2);   /* the reply is one packet away, not a poll loop */
	}
	/* Tier 0 is audio-APC-capable but libsndfile-less: A;Load is a no-op
	 * there, so a Queue would play an empty slot -- silent, and worse than
	 * silent because subtitles would be off. Only tier 1 counts. g_opus_ok
	 * defaults to 1 for an older SyncTERM that never answers the Opus query
	 * (termgfx/audio.h:41-51). */
	return (g_audio_tier >= 1 && g_opus_ok) ? 1 : 0;
}

static int termgfx_isdir(const char *p)
{
	/* xpdev's portable directory test: POSIX has S_ISDIR() but MSVC's
	 * <sys/stat.h> does not define it (only _S_IFDIR), and isdir() hides that. */
	return isdir(p) ? 1 : 0;
}

/* Talkie/Floppy data-set selection (M5): see the doc comment in termgfx_termio.h. A
 * pure directory-stat helper -- no session state -- so it is unit-tested
 * standalone (test/test_termgfx_termio_datadir.c) without a socket. */
const char *termgfx_select_datadir(const char *base, int audio, char *buf, size_t bufsz)
{
	const char *first  = audio ? "talkie" : "floppy";
	const char *second = audio ? "floppy" : "talkie";
	char        cand[600];
	const char *why;

	snprintf(cand, sizeof cand, "%s/%s", base, first);
	if (termgfx_isdir(cand)) {
		snprintf(buf, bufsz, "%s", cand);
		why = "match";
	} else {
		snprintf(cand, sizeof cand, "%s/%s", base, second);
		if (termgfx_isdir(cand)) {
			snprintf(buf, bufsz, "%s", cand);
			why = "fallback-other-build";
		} else {
			snprintf(buf, bufsz, "%s", base);   /* flat --path / single build */
			why = "fallback-base";
		}
	}
	/* Diagnostic (<DOOR>_TRACE only): which build this session got and the
	 * audio determination that drove it -- so a "no speech" report can be told
	 * apart (Floppy picked = audio probe said no vs. Talkie picked = a speech
	 * config/engine issue) from a node trace without a live debugger. */
	if (g_trace != NULL)
		fprintf(g_trace, "%u DATADIR audio=%d base=%s -> %s (%s)\n",
		        (unsigned)now_ms(), audio, base, buf, why);
	return buf;
}

/* ---- streamed audio (M4) ----
 * The mixer's PCM goes out through termgfx's shared streaming module
 * (termgfx/audio_stream.h): cushion, backlog policy, silence cache, blob
 * path and underrun re-prime all live there. This file's job is the door's
 * half -- the output seam and the session's config. */

static termgfx_stream_t *g_stream;

/* Latches "audio_stream_open() was already tried and did not produce a
 * stream", so a permanently-failing create (OOM, or any other reason
 * termgfx_stream_create() hands back NULL) gets exactly one retry instead
 * of one per PCM block for the rest of the session. g_audio_enabled's own
 * check below already covers the sysop-disabled case for free -- this flag
 * is the backstop for every OTHER way open can fail. */
static int g_stream_open_failed;

/* The pre-encode headroom, in percent of full scale -- "[audio] headroom", read
 * once alongside the module's own keys (audio_read_ini(), below). Kept here
 * rather than in the cfg the module is handed because it is not the module's
 * business: termgfx/audio_stream.c is shared with syncretro, and what a hot
 * SCUMM mix needs before an Opus encoder is this door's problem to solve on its
 * own side of the seam. See TERMGFX_AUDIO_HEADROOM (termgfx_termio.h) for what it is for
 * and why the number is 70; audio_apply_headroom() below is where it lands. */
static int g_audio_headroom = TERMGFX_AUDIO_HEADROOM;

/* Scratch for audio_apply_headroom()'s scaled copy: the mixer hands us a const
 * block it still owns, so scaling has to go somewhere else. Grown on demand and
 * kept between blocks -- tick() ships a new block tens of times a second, and
 * they are all about the same size, so this reallocs a couple of times early and
 * then never again. Freed in termgfx_termio_shutdown(). */
static int16_t *g_hr_buf;
static size_t   g_hr_cap;   /* SAMPLES (frames * 2), not frames */

static void audio_free_scratch(void)
{
	free(g_hr_buf);
	g_hr_buf = NULL;
	g_hr_cap = 0;
}

/* Stage one audio sequence -- and record WHERE it landed, so
 * termgfx_termio_audio_backlog() can tell audio's share of the FIFO from video's.
 *
 * `start` is captured before out_put() because out_put()'s stage-full guard
 * can flush from inside the copy, which moves g_out_base and memmoves the
 * stage; stream coordinates are immune to both, which is why the ring is kept
 * in them rather than in g_out indices. The staged bytes are contiguous from
 * `start` whatever happens in there, since out_put() only ever drops a
 * REMAINDER.
 *
 * Every call here is exactly one complete sequence -- termgfx/audio_stream.c
 * hands its builder's whole APC to put() in a single call -- so a short stage
 * means the door truncated an APC, and g_audio_dropped is how that becomes
 * visible instead of merely audible. (Fixing the truncation itself is the
 * separate follow-up filed against out_put()'s byte-granular drop; see
 * docs/superpowers/plans/2026-07-16-syncscumm-m4.md.) */
static void termgfx_stream_put(void *ctx, const void *buf, size_t len)
{
	uint64_t start = g_out_base + (uint64_t)g_out_len;
	size_t   n;

	(void)ctx;
	n = out_put(buf, len);
	if (n > 0)
		aseg_add(start, start + (uint64_t)n);
	if (n < len)
		g_audio_dropped++;
}

static int termgfx_stream_flush(void *ctx)
{
	(void)ctx;
	termgfx_termio_flush();
	return g_hangup ? 0 : 1;
}

/* AUDIO's backlog, never the stage's. The one-word difference between this and
 * termgfx_termio_out_backlog() is the entire comic-intro defect. */
static size_t termgfx_stream_backlog(void *ctx)
{
	(void)ctx;
	return termgfx_termio_audio_backlog();
}

static const termgfx_stream_io_t g_stream_io = {
	termgfx_stream_put, termgfx_stream_flush, termgfx_stream_backlog, NULL
};

static void termgfx_audio_underrun(int ch)
{
	if (g_stream != NULL)
		termgfx_stream_underrun(g_stream, ch);
}

/* Sysop audio settings, <door>.ini's [audio] section. Same file and same
 * lookup as termgfx_read_ini()'s "sixel_max" above and door/syncscumm.cpp's
 * resolveSubtitles() -- one <door>.ini, fopen()ed relative to CWD, the
 * door's startup_dir. (Read here rather than folded into termgfx_read_ini()
 * because these values belong to the stream's config struct, which does not
 * exist until audio_stream_open() builds it; parking them in five more
 * file-static shadows just to copy them across later would be the worse
 * trade.) A missing file or a missing key leaves every default in place, and
 * the module clamps whatever it is handed (audio_stream.h: quality
 * 0.01..1.0, volume 0..100, chunk_ms 50..250, prebuffer 2..8,
 * channels 1..2), so a nonsense
 * value cannot break a session -- do NOT re-clamp here and give the door a
 * second, silently diverging opinion of the valid ranges.
 *
 * Every default is spelled in audio_stream_open() below and passed in via
 * cfg, never restated here: these iniGet* calls hand the CURRENT value back
 * as the fallback, so there is exactly one place to read the defaults off. */
static void audio_read_ini(termgfx_stream_cfg_t *cfg)
{
	FILE      *f = fopen(app_ini(), "r");
	str_list_t ini;

	if (f == NULL)
		return;
	ini = iniReadFile(f);
	fclose(f);
	if (ini == NULL)
		return;
	/* No "enabled" here: that key is latched once at init into
	 * g_audio_enabled (termgfx_read_ini(), above) because
	 * termgfx_termio_audio_available() needs it long before this runs, and re-reading
	 * it here would give the session a second opinion of the same switch. */
	cfg->quality   = iniGetFloat(ini, "audio", "quality", cfg->quality);
	/* volume is the exception to the fallback-is-cfg rule above: the sysop knob
	 * stays a friendly 0..100 percent, but the module's field is dB, so read the
	 * percent (default TERMGFX_AUDIO_VOLUME_PCT) and convert it. */
	cfg->volume_db = termgfx_db_from_pct(iniGetInteger(ini, "audio", "volume",
	                                                   TERMGFX_AUDIO_VOLUME_PCT));
	cfg->chunk_ms  = iniGetInteger(ini, "audio", "chunk_ms", cfg->chunk_ms);
	cfg->prebuffer = iniGetInteger(ini, "audio", "prebuffer", cfg->prebuffer);
	cfg->channels  = iniGetInteger(ini, "audio", "channels", cfg->channels);
	/* Not a cfg field, and deliberately so: the headroom is applied on THIS
	 * side of the seam (audio_apply_headroom(), below), because the module it
	 * would otherwise go in is shared with syncretro. It is read here anyway so
	 * the whole [audio] section still costs exactly ONE open of <door>.ini
	 * -- on this host that file lives under a CIFS mount, and the reason
	 * termgfx_termio_audio_stream() bails early for a disabled session is precisely to
	 * keep this function from becoming an SMB round trip storm. Clamped to
	 * 1..100 rather than 0..100: 0 is not a quieter stream, it is a silent one
	 * that still pays the full Opus encode and the full uplink for every chunk,
	 * which is never what a sysop reaching for a headroom knob meant. Muting is
	 * "[audio] enabled = false". */
	g_audio_headroom = iniGetInteger(ini, "audio", "headroom", g_audio_headroom);
	if (g_audio_headroom < 1)
		g_audio_headroom = 1;
	if (g_audio_headroom > 100)
		g_audio_headroom = 100;
	strListFree(&ini);
}

/* Scale one block of the mixer's PCM to g_audio_headroom percent of full scale,
 * returning the block to hand the encoder. Returns `pcm` itself at 100 (and on
 * an allocation failure), so the default-off case and the degraded case both
 * cost one branch and no copy.
 *
 * WHY THE ATTENUATION HAS TO BE HERE, upstream of the encoder, and cannot be
 * the terminal's channel volume instead: see TERMGFX_AUDIO_HEADROOM (termgfx_termio.h). The
 * short version is that a lossy codec reconstructs this mix's hard-clipped
 * peaks with ringing that overshoots full scale, and libsndfile's S16 read
 * WRAPS that overshoot into a full-scale sign flip rather than clamping it --
 * so by the time the channel volume gets a say, the damage is a signed integer
 * that already has the wrong sign. Scaling before the encode is the only thing
 * that gives the ringing room to land.
 *
 * Deliberately NOT applied to the g_audiodump tap in termgfx_termio_audio_stream(): the
 * dump's contract is that it is the MIXER's signal, the reference to diff an
 * encode against, so it stays what ScummVM produced. */
static const int16_t *audio_apply_headroom(const int16_t *pcm, size_t frames)
{
	size_t   samples = frames * 2;   /* *2: the mixer's block is stereo */
	size_t   i;
	int16_t *grown;

	if (g_audio_headroom >= 100)
		return pcm;
	if (g_hr_cap < samples) {
		grown = (int16_t *)realloc(g_hr_buf, samples * sizeof(int16_t));
		if (grown == NULL)
			return pcm;   /* louder than we meant beats dropping the block */
		g_hr_buf = grown;
		g_hr_cap = samples;
	}
	/* Rounds to nearest, away from zero, and cannot overflow: the widest
	 * intermediate is 32767 * 100, well inside an int. Integer rather than
	 * float because this runs on every sample of every block and the error a
	 * float would save is ~90dB below a signal the codec is about to hand back
	 * at ~32dB SNR. */
	for (i = 0; i < samples; i++)
		g_hr_buf[i] = (int16_t)((pcm[i] * g_audio_headroom +
		                         (pcm[i] < 0 ? -50 : 50)) / 100);
	return g_hr_buf;
}

/* Lazily create the stream on the first PCM, once the tier is known.
 * Deliberately not in termgfx_termio_init(): the caps reply has not landed there,
 * and a stream created against an unknown tier would either drop the
 * cushion's first chunks or emit to a terminal that cannot play them. */
static void audio_stream_open(void)
{
	termgfx_stream_cfg_t cfg;

	memset(&cfg, 0, sizeof cfg);
	/* From the init-time latch, not from cfg's own default + audio_read_ini()
	 * below: g_audio_enabled is the session's single copy of the sysop switch
	 * and termgfx_termio_audio_available() has already answered the subtitle question
	 * from it. Both must speak for the same read of the same file. */
	cfg.enabled      = g_audio_enabled;
	cfg.quality      = TERMGFX_MUSIC_QUALITY_DEFAULT;
	/* 100 -- and read this WITH cfg_headroom's ~-3dB below, because the pair is
	 * one decision, not two. This was 70 for a real reason: SyncTERM's channel
	 * mixer soft-clips (a tanh knee), this stream is ALREADY mixed -- every
	 * voice, effect and music track summed by ScummVM -- and at unity the sum
	 * distorted, so ~70 bought it ~-3dB of headroom, the same fix syncconquer's
	 * FMV audio needed.
	 *
	 * That reason has not gone away; the ~-3dB has MOVED, to
	 * TERMGFX_AUDIO_HEADROOM (termgfx_termio.h), which applies it BEFORE the Opus encode
	 * instead of after the decode. End to end the terminal sees the identical
	 * signal it saw at 70 -- 70% x 100% is the same net gain as the 100% x 70%
	 * that shipped before, so the tanh knee sits at exactly the operating point
	 * it was tuned to and nothing got louder -- but the same 3dB now does a
	 * second job it could never do down here: it keeps the CODEC's ringing off
	 * the rail. A channel volume is applied post-decode, so it can only scale
	 * samples that Opus's overshoot has already wrapped to the wrong sign; that
	 * is why the battle scene crunched at volume 70 and why moving the number
	 * rather than adding another one is the whole fix.
	 *
	 * NEVER spell a mute as 0 here: SyncTERM clamps a channel volume of 0 to
	 * -60dB and bakes it into the channel's entry volume (it cost SyncDuke a
	 * debugging session); the module stops sending instead.
	 *
	 * 50 (= -6dB), not 100, so BASS sits with the other doors instead of
	 * ~9dB above them: this stream carries ScummVM's WHOLE pre-mixed output
	 * near unity, whereas SyncDOOM/SyncDuke trim music ~11-14dB under SFX and
	 * one-shots ride SyncTERM's -12dB channel base. -6dB here, atop the -3dB
	 * headroom, lands the net at ~-9dB -- close to the family, still clear for
	 * dialogue. (The player can trim it live with the + / - keys.) */
	cfg.volume_db    = termgfx_db_from_pct(TERMGFX_AUDIO_VOLUME_PCT);
	/* 250ms per chunk (TERMGFX_CHUNK_MS), the top of the module's 50..250 clamp,
	 * and the cheapest real improvement available to the pops without touching
	 * the shared module: every chunk is its OWN Ogg/Opus stream, so a boundary
	 * is a stream boundary, and this simply makes fewer of them. Measured on
	 * the 459.9s BASS capture, stereo, against the same path at 100ms:
	 *
	 *   boundaries anomalous vs interior 99th pct: 2.6% -> 0.9% (= chance)
	 *   median step at a boundary:                  395  -> 341
	 *   bitrate:                                   158.0 -> 110.2 kbps
	 *
	 * The bitrate falls because 4598 sets of Ogg headers become 1839, which is
	 * also why this beats the mono experiment outright: 110.2 kbps STEREO is
	 * cheaper than 113.7 kbps MONO was, and mono paid for that in worse pops.
	 * It does NOT fix the framing -- one continuous stream of this same PCM is
	 * 39.5 kbps -- it just stops making the boundary a 10-a-second event. */
	cfg.chunk_ms     = TERMGFX_CHUNK_MS;
	/* ~750ms of cushion: the latency budget AND the entire jitter tolerance
	 * (audio_stream.h). Read this WITH cfg.chunk_ms above -- prebuffer counts
	 * CHUNKS, so the cushion is their product, and when the chunk grew to 250ms
	 * this had to fall 8 -> 3 to hold the cushion still. Left at 8 it would be
	 * a TWO-SECOND cushion: two seconds of speech behind the picture, which is
	 * a worse defect than the pops it was bought to avoid.
	 *
	 * 3 x 250 = 750ms, and the cushion is sized this way because the FIFO is
	 * shared and strictly ordered, so audio waits out whatever frame was staged
	 * ahead of it -- see the staging block at the top of this file. That wait is
	 * MEASURED: of 3673 comic-intro frames, 318 (8.7%) take longer than 300ms to
	 * drain and NONE exceed 800ms, the worst at 663ms. 750ms still covers that
	 * worst frame; 2 chunks (500ms) would not, and an underrun re-primes, which
	 * is another gap -- the cushion has to cover the WORST frame, not the
	 * average one. That measurement is a drain time and stands on its own: it
	 * outlived the frame-congestion theory it was originally taken to support
	 * (the real defect was audio never being flushed while the screen was
	 * static, since fixed), because the FIFO being shared and ordered is
	 * structural, not a theory.
	 *
	 * The cost is ~750ms of constant speech-behind-picture offset, an offset and
	 * not drift: both ride one wall clock, so it never accumulates. End to end
	 * this run is ~100ms LONGER than the old one all the same -- a chunk cannot
	 * ship until it fills, so 250 + 750 = 1000ms replaces 100 + 800 = 900ms.
	 * "[audio] prebuffer" still overrides it (audio_read_ini(), called below);
	 * a sysop who lowers chunk_ms should raise this to keep the product. */
	cfg.prebuffer    = TERMGFX_PREBUFFER_CHUNKS;
	/* Stereo, and this default was MEASURED BACK from mono, not assumed. The
	 * case for mono was real on its face: a 459.9s capture of BASS (11,036,672
	 * frames of the mixer's own pre-encode PCM) has left and right
	 * BIT-IDENTICAL -- max |L-R| = 0, not one differing frame -- so the title's
	 * samples are mono, ScummVM duplicates them across the pair, and the wire
	 * pays for the copy. The module's accumulator downmixes by keeping the left
	 * sample, which for L==R is exactly lossless. Mono SHOULD have been free.
	 *
	 * It was not, on either count, and both numbers contradicted the
	 * expectation:
	 *
	 * 1. It saved ~28%, not the ~50% a halved channel count implies (157.7 ->
	 *    113.7 kbps). Opus already spent almost nothing on a side channel that
	 *    is identically zero.
	 * 2. It made the CHUNK-BOUNDARY pops -- the artifact a listener actually
	 *    complains about -- measurably WORSE: median boundary step 395 ->
	 *    1080, boundaries anomalous against the interior 99th pct 2.6% ->
	 *    17.2%.
	 *
	 * The mechanism behind (2), so nobody re-derives it: a MONO Opus stream
	 * comes back from libsndfile misaligned by about a sample at 24000 (and at
	 * 48000; 8/12/16k are clean, and a stereo stream is clean at every rate).
	 * Continuously that is inaudible -- one constant sub-sample offset over the
	 * whole stream. But this module ships each chunk as its OWN stream, so the
	 * skew re-lands at every boundary. Trading a 28% uplink saving for a worse
	 * version of the headline artifact is a bad trade, so the default went
	 * back.
	 *
	 * A default, though, NOT a verdict -- the key stays, and has two real
	 * users: a sysop on a thin uplink who will take the pops for the 28%, and a
	 * genuinely stereo title (an engine panning its effects) which needs the
	 * pair anyway. "[audio] channels = 1" in <door>.ini narrows the wire
	 * again. That is also why the MIXER stays stereo above (audio_term.cpp):
	 * only the wire changes width, ScummVM's mix path is untouched, and the
	 * sysop's key can move it either way without the door having to rebuild a
	 * mixer that was created long before this file was read.
	 *
	 * Neither width FIXES the pops -- they are the per-chunk stream boundary's
	 * fault, and one continuous stream of this same PCM measures 1.03x =
	 * chance, at 39.5 kbps. Stereo is the width that does not make them worse. */
	cfg.channels     = 2;
	cfg.rate         = TERMGFX_AUDIO_RATE;
	cfg.ch           = 2;
	cfg.slot         = 0;
	cfg.name         = app_name();
	cfg.cache_prefix = "s";
	/* Last, so the sysop's file overrides the defaults above rather than the
	 * other way around -- and before create(), which takes cfg BY VALUE and
	 * clamps it once (audio_stream.h -- channels included, 1..2). Deliberately
	 * does not touch rate/ch/slot/name/cache_prefix: those are properties of
	 * the door and the mixer, not preferences, and a sysop has no way to know a
	 * good value for any of them. channels IS a preference -- it depends on the
	 * game, not on the door -- so it is read like the four keys above it. */
	audio_read_ini(&cfg);
	g_stream = termgfx_stream_create(&cfg, &g_stream_io);
	if (g_stream == NULL)
		return;
	/* g_cterm_ver is already latched from the CTDA reply (csi_final()'s 'c'
	 * case, above; maj*1000+min); the JXL path next to it uses the same
	 * threshold to pick DrawJXLBlob. >= 1.329 means A;LoadBlob works, so
	 * chunks ship inline with no cache-file churn at all. */
	termgfx_stream_set_blob_ok(g_stream, g_cterm_ver >= TERMGFX_CTERM_VER_BLOB);
	termgfx_stream_caps(g_stream, g_audio_tier >= 1 && g_opus_ok ? 1 : 0);
}

void termgfx_termio_audio_stream(const int16_t *pcm, size_t frames)
{
	if (pcm == NULL || frames == 0)
		return;
	/* The diagnostic tap (g_audiodump), ahead of every gate below: this file
	 * is meant to be the mixer's signal, not the session's. A headless run
	 * (!g_active) and a disabled-audio session both discard the PCM a few
	 * lines down, and those are exactly the runs a test script makes. Inert
	 * unless <DOOR>_AUDIODUMP was set. */
	if (g_audiodump != NULL)
		fwrite(pcm, sizeof(int16_t) * 2, frames, g_audiodump);   /* *2: stereo */
	if (!g_active || g_hangup)
		return;
	/* The sysop switch, checked here BEFORE any INI work and before the
	 * open attempt below: with audio disabled, cfg.enabled is guaranteed
	 * false (audio_stream_open() takes it straight from g_audio_enabled)
	 * so termgfx_stream_create() is guaranteed to hand back NULL, and
	 * g_stream can therefore never latch. Without this check that
	 * guaranteed failure fell through to the general open-once path below,
	 * and the caller here is this door's mixer tick -- audio_term.cpp's
	 * tick() via pollEvent(), tens to hundreds of calls per second for the
	 * rest of the session -- so every one of them re-ran audio_read_ini():
	 * an fopen() + iniReadFile() + strListFree() of <door>.ini for a
	 * result already known. On this host that file lives under a
	 * CIFS-mounted /sbbs, so each retry was a real SMB open/read/close
	 * round trip; a per-tick storm of those is the same "slow disk access"
	 * pathology this box has chased before as a scraper or lock-contention
	 * problem when it was really an open/close storm. Bailing here costs
	 * nothing (g_audio_enabled is already the init-time latch) and reads
	 * the INI zero times for a disabled session, rather than the one time
	 * the general backstop below would still allow. */
	if (!g_audio_enabled)
		return;
	if (g_stream == NULL) {
		if (!g_audio_done || g_stream_open_failed)
			return;   /* tier unknown, or open already tried and failed */
		audio_stream_open();
		if (g_stream == NULL) {
			g_stream_open_failed = 1;
			return;
		}
	}
	/* The pre-encode headroom, applied here rather than inside the module
	 * (which is shared with syncretro) and AFTER the g_audiodump tap above
	 * (which is meant to be the mixer's own signal). See TERMGFX_AUDIO_HEADROOM in
	 * termgfx_termio.h: without it this door's hottest passages hand Opus a
	 * hard-clipped waveform whose reconstruction overshoots full scale, and
	 * libsndfile -- ours and SyncTERM's alike -- WRAPS that overshoot into a
	 * sign flip instead of clamping it. */
	termgfx_stream_feed(g_stream, audio_apply_headroom(pcm, frames), frames);
	/* ...and PUT IT ON THE WIRE, here, rather than leaving it staged for a
	 * present() that may never come.
	 *
	 * THE DEFECT THIS FIXES: Beneath a Steel Sky's comic intro had
	 * seconds-long gaps in its dialogue from the very first panel, while
	 * gameplay -- at a higher byte rate -- was flawless. A comic panel is a
	 * STILL: drawn once, then talked over for seconds. Two flush policies met
	 * and left a hole exactly there.
	 *
	 * termgfx/audio_stream.c calls io.flush() in exactly TWO places -- when
	 * the PRIME cushion releases (audio_stream.c:416) and at stop
	 * (audio_stream.c:470). In steady RUN it only stages chunks through
	 * io.put() and leaves the writing to the door's main loop. This door's
	 * main loop only flushed from termgfx_termio_present() (termgfx_termio.c:2350, 2627). So
	 * with nothing drawn, nothing flushed: every chunk the mixer produced sat
	 * in g_out unwritten, audio's share of the stage (termgfx_termio_audio_backlog())
	 * climbed past the module's 48KB rule, and the module began dropping
	 * chunks -- concluding "this link cannot carry audio" about a socket that
	 * was IDLE. The trace says it plainly: no present() between 34s and 42s,
	 * then 71 drops all at once at 42s, the first present after the gap (8s x
	 * 10 chunks/s ~= 71). Gameplay never showed it because constant
	 * dirty-rect redraws mean constant flushes. The link was never the
	 * bottleneck; it does multiple MB/s against 400KB/s peaks.
	 *
	 * The module is deliberately NOT the place to fix this: its flush policy
	 * is shared with syncretro, whose core runs continuously and whose main
	 * loop therefore flushes on its own. The door that can go quiet is the
	 * door that has to flush.
	 *
	 * WHY HERE IS SAFE -- this cannot split a video sequence. The only caller
	 * is audio_term.cpp's tick() (audio_term.cpp:79), reached from exactly
	 * two places, and at neither is a frame partially staged:
	 *   - syncscumm.cpp:175, in pollEvent(), before termgfx_termio_pump();
	 *   - video_term.cpp:232, at the TOP of updateScreen(), ahead of compose()
	 *     and both termgfx_termio_present() calls (video_term.cpp:272, 274).
	 * termgfx_termio_present() itself never ticks the mixer, so no tick can land
	 * inside one. Nothing is reordered and no "may audio go now?" boundary
	 * predicate is introduced: this is the same strictly-ordered FIFO write
	 * every other caller makes, just made at a moment the door would
	 * otherwise have stayed silent. An earlier attempt at this defect DID add
	 * a priority/boundary rule, spliced an A;LoadBlob into an open sixel DCS,
	 * and was reverted -- see the staging block at the top of this file.
	 *
	 * Unconditional, and per feed rather than per closed chunk. Per-chunk
	 * would be the tighter granularity but the door cannot see a chunk close
	 * -- that is inside the module, behind termgfx_stream_feed(). Per feed
	 * costs nothing to be wrong about: on the overwhelmingly common tick the
	 * module is still accumulating and staged nothing, so termgfx_termio_flush() takes
	 * its !g_out_len early return, and aseg_prune() there walks an already
	 * empty ring (an empty stage means every segment is retired by
	 * definition). The real work -- one write() -- happens only on the ticks
	 * that actually staged a chunk, which is exactly the tick that needs it. */
	termgfx_termio_flush();
}

void termgfx_termio_audio_stop(void)
{
	if (g_stream == NULL)
		return;
	/* termgfx_stream_destroy() already calls termgfx_stream_stop() internally
	 * (A;Flush, no fade -- see audio_stream.h), so an explicit stop() here
	 * would just emit the same A;Flush twice: stop() doesn't advance the
	 * stream's state, so destroy()'s own call passes the same state check
	 * and fires again. */
	termgfx_stream_destroy(g_stream);
	g_stream = NULL;
}

/* ============================================================================
 * Present path (Task 4; JXL tier added Task 7). Ported from syncconquer/
 * door/door_io.c's Phase-2 dirty-rect present loop, resized to SyncSCUMM's
 * fixed TERMGFX_FB_W x TERMGFX_FB_H (320x200) framebuffer and simplified: two image
 * tiers only, auto-selected (JXL > sixel, termgfx_tier()) -- no block/text tiers
 * (SCUMM's palette-fade-heavy frames don't suit them the way an RTS or FPS
 * door's HUD does, and the spec forbids text tiers outright), no manual
 * cycle/force (auto-select settles once, at the startup probe), and a fixed
 * 8x16 cell size (this task doesn't track the text grid, only the pixel
 * canvas -- door_cell_size()'s dynamic derivation is future work).
 * ==========================================================================*/

#define TERMGFX_TILE            16
#define TERMGFX_TX              (TERMGFX_FB_W / TERMGFX_TILE)                    /* 20 */
#define TERMGFX_TY              ((TERMGFX_FB_H + TERMGFX_TILE - 1) / TERMGFX_TILE)   /* 13: bottom row is 8px */
#define TERMGFX_MAX_BOXES       16
#define TERMGFX_MAX_COMPONENTS  128
#define TERMGFX_MERGE_GAP       2
#define TERMGFX_FALLBACK_PCT    45
#define TERMGFX_SCALE_MAX       2048
#define TERMGFX_PACE_DEADLINE_MS 750
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
#define TERMGFX_PALSTORM_MIN_MS 80
#define TERMGFX_DSR_RING        16
#define TERMGFX_PROBE_GRACE_MS  500
/* TERMGFX_GFX_SETTLE_MS is defined up with the scan accumulator: the audio
 * probe's bounded wait shares the window and is defined ahead of here. */
#define TERMGFX_AUTO_DEPTH_MAX  8
#define TERMGFX_CAPTURE_MAX_FRAMES 200

/* --- piece 1: termgfx_now_ms() -- reuse the file's existing CLOCK_MONOTONIC
 * helper (now_ms(), declared above for the probe-grace/jxl-scan timers). No
 * separate function needed; door_io.c's door_now_ms() equivalent already
 * exists here. */

/* --- piece 2: tile diff + coalesce (door_io.c:2425-2520 dr_coalesce() /
 * dr_diff_coalesce(), TERMGFX_ constants). The bottom tile row is only 8px tall
 * (200 isn't a multiple of 16), so the memcmp span per tile row is TERMGFX_TILE
 * except for ty == TERMGFX_TY-1, where it's TERMGFX_FB_H - ty*TERMGFX_TILE. ---------- */
struct termgfx_box { int x1, y1, x2, y2; };
static uint8_t termgfx_grid[TERMGFX_TY][TERMGFX_TX];

static int termgfx_coalesce(struct termgfx_box *box)
{
	static uint8_t   vis[TERMGFX_TY][TERMGFX_TX];
	static int       st[TERMGFX_TY * TERMGFX_TX];
	static const int ox[4] = { -1, 1, 0, 0 };
	static const int oy[4] = { 0, 0, -1, 1 };
	int              nb = 0, tx, ty, i, j, merged;

	memset(vis, 0, sizeof vis);
	for (ty = 0; ty < TERMGFX_TY; ty++) {
		for (tx = 0; tx < TERMGFX_TX; tx++) {
			int sp, x1, y1, x2, y2;
			if (!termgfx_grid[ty][tx] || vis[ty][tx])
				continue;
			if (nb >= TERMGFX_MAX_COMPONENTS)
				return -1;
			sp          = 0;
			st[sp++]    = ty * TERMGFX_TX + tx;
			vis[ty][tx] = 1;
			x1          = x2 = tx;
			y1          = y2 = ty;
			while (sp) {
				int idx = st[--sp], cx = idx % TERMGFX_TX, cy = idx / TERMGFX_TX, k;
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
					if (nx >= 0 && nx < TERMGFX_TX && ny >= 0 && ny < TERMGFX_TY
					    && termgfx_grid[ny][nx] && !vis[ny][nx]) {
						vis[ny][nx] = 1;
						st[sp++]    = ny * TERMGFX_TX + nx;
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
				if (box[i].x1 <= box[j].x2 + TERMGFX_MERGE_GAP && box[j].x1 <= box[i].x2 + TERMGFX_MERGE_GAP
				    && box[i].y1 <= box[j].y2 + TERMGFX_MERGE_GAP && box[j].y1 <= box[i].y2 + TERMGFX_MERGE_GAP) {
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
	return (nb > TERMGFX_MAX_BOXES) ? -1 : nb;
}

static int termgfx_diff_coalesce(const uint8_t *fb, const uint8_t *last, struct termgfx_box *box)
{
	const int tiles = TERMGFX_TX * TERMGFX_TY;
	int       tx, ty, r, nb, dirty = 0;

	for (ty = 0; ty < TERMGFX_TY; ty++) {
		int rows = (ty == TERMGFX_TY - 1) ? (TERMGFX_FB_H - ty * TERMGFX_TILE) : TERMGFX_TILE;
		for (tx = 0; tx < TERMGFX_TX; tx++) {
			int ch = 0;
			for (r = 0; r < rows && !ch; r++) {
				size_t off = (size_t)(ty * TERMGFX_TILE + r) * TERMGFX_FB_W + (size_t)tx * TERMGFX_TILE;
				if (memcmp(fb + off, last + off, TERMGFX_TILE) != 0)
					ch = 1;
			}
			termgfx_grid[ty][tx] = (uint8_t)ch;
			if (ch)
				dirty++;
		}
	}
	if (dirty == 0 || dirty * 100 / tiles >= TERMGFX_FALLBACK_PCT)
		return 0;                                   /* nothing / big change -> full frame */
	nb = termgfx_coalesce(box);
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
/* Packed RGB888 scratch, shared by the JXL tier's termgfx_pack_rgb()/_rect()
 * (below, WITH_JXL only) and present_rgbx()'s termgfx_scale_rgbx_to_rgb() (which
 * feeds BOTH the sixel and JXL truecolor tiers), so declared unconditionally
 * here rather than inside the WITH_JXL block -- a no-libjxl build still uses it
 * for the sixel truecolor path. */
static uint8_t *g_rgb_buf;   static size_t g_rgb_cap;

static const uint8_t *termgfx_scale_idx(const uint8_t *fb, int ew, int eh)
{
	int y;
	if (!ensure_cap(&g_idx_buf, &g_idx_cap, (size_t)ew * eh))
		return NULL;
	for (y = 0; y < eh; y++) {
		const uint8_t *row = fb + (size_t)(y * TERMGFX_FB_H / eh) * TERMGFX_FB_W;
		uint8_t *      o   = g_idx_buf + (size_t)y * ew;
		int            x;
		for (x = 0; x < ew; x++)
			o[x] = row[x * TERMGFX_FB_W / ew];
	}
	return g_idx_buf;
}

/* NN-scale a native w x h XRGB (R,G,B,X) frame to a packed RGB888 ew x eh
 * buffer (pad byte dropped). Backs present_rgbx's sixel + JXL tiers. Unlike
 * termgfx_scale_idx()/termgfx_pack_rgb(), the source dimensions are the caller's own
 * w/h (a truecolor source has no fixed TERMGFX_FB_W x TERMGFX_FB_H surface), so they
 * are parameters here rather than the TERMGFX_FB_* constants. */
static const uint8_t *termgfx_scale_rgbx_to_rgb(const uint8_t *xrgb, int w, int h,
                                            int ew, int eh)
{
	int y;
	if (!ensure_cap(&g_rgb_buf, &g_rgb_cap, (size_t)ew * eh * 3))
		return NULL;
	for (y = 0; y < eh; y++) {
		const uint8_t *srow = xrgb + (size_t)(y * h / eh) * w * 4;
		uint8_t *      o    = g_rgb_buf + (size_t)y * ew * 3;
		int            x;
		for (x = 0; x < ew; x++) {
			const uint8_t *p = srow + (size_t)(x * w / ew) * 4;
			*o++ = p[0]; *o++ = p[1]; *o++ = p[2];
		}
	}
	return g_rgb_buf;
}

/* Pack a display sub-rect [rx,ry,rw,rh] (in ew x eh scaled-image space) into
 * g_idx_buf, NN-sampled from fb exactly as termgfx_scale_idx() scales the full
 * frame, so the rect lines up with the sixel frame the client still holds
 * (door_io.c's dr_pack_idx_rect()). */
static const uint8_t *termgfx_pack_idx_rect(const uint8_t *fb, int ew, int eh,
                                        int rx, int ry, int rw, int rh)
{
	int      i, j;
	uint8_t *p;

	if (!ensure_cap(&g_idx_buf, &g_idx_cap, (size_t)rw * rh))
		return NULL;
	p = g_idx_buf;
	for (j = 0; j < rh; j++) {
		const uint8_t *row = fb + (size_t)((ry + j) * TERMGFX_FB_H / eh) * TERMGFX_FB_W;
		for (i = 0; i < rw; i++)
			*p++ = row[(rx + i) * TERMGFX_FB_W / ew];
	}
	return g_idx_buf;
}

/* --- piece 4: full-frame emit (door_io.c:1918-1936 door_emit_sixel()
 * pattern -- clamp to whole 6-row sixel bands, SF #258). The centering
 * math (piece 3's fit/center half, in termgfx_termio_present() below) uses the
 * UN-rounded eh; this rounds down only for the encode, same as door_io.c,
 * leaving a few pixels of slack at the bottom of the centered slot. -------*/
static size_t termgfx_emit_sixel(const uint8_t *fb, const uint8_t *pal8, int ew, int eh, int emit_pal)
{
	const uint8_t *idx;

	if (ew > TERMGFX_SCALE_MAX)
		ew = TERMGFX_SCALE_MAX;
	eh -= eh % 6;
	if (eh < 6)
		eh = 6;
	idx = termgfx_scale_idx(fb, ew, eh);
	if (idx == NULL)
		return 0;
	return sixel_encode(&g_sixel_buf, &g_sixel_cap, idx, ew, eh, pal8, emit_pal);
}

/* --- piece 5: dirty emit (door_io.c:2553-2625 door_dirty_sixel_present()
 * pattern). Cell size is the fixed 8x16 this task centers against (no text-
 * grid tracking yet), not a derived door_cell_size(). Caller guarantees
 * g_have_last and that the client holds the previous sixel frame (this
 * door only ever emits sixel, so "same tier" is implicit). ---------------*/
static int termgfx_gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }

/* cw/ch are the terminal's REAL text-cell pixels: a dirty box is placed by
 * CUP at a cell, so its display-pixel rect must snap to that cell grid, or the
 * box lands where the door THINKS the cell is (an 8x16 guess) rather than where
 * the terminal actually draws it (foot's cell is 6x13) -- which tore the comic
 * panels. Snapping on the real grid makes each box align.
 *
 * The box HEIGHT is snapped to LCM(ch, 6) (vstep) -- a whole number of BOTH
 * text cells (ch) and 6px sixel bands. Neither alone is enough on a cell-
 * anchored terminal like foot: it draws whole 6px bands and allocates whole
 * cells for them, so a box height that is a multiple of the cell but not the
 * band (e.g. 13) leaves a transparent partial band, and foot backfills the
 * rest of the cell block with the background -- a thin black strip along the
 * box's bottom edge, worst in sparse scenes. ch and 6 are coprime for ch=13,
 * so only their LCM (78) clears both.
 *
 * Shared by both the stranding pre-pass and the emit loop below (was
 * duplicated inline in each; factored out so the two can never drift). Only
 * computes geometry -- callers decide what an empty (*rw<=0) or short
 * (*ry+*rh < *ry2, i.e. stranded) result means. *ry2_out is the box's own
 * unclamped bottom row, needed by the stranding test. */
static void termgfx_box_display_rect(const struct termgfx_box *b, int ew, int ehc,
                                 int cw, int ch, int vstep,
                                 int *rx_out, int *ry_out, int *rw_out, int *rh_out,
                                 int *ry2_out)
{
	int fx1 = b->x1 * TERMGFX_TILE, fx2 = (b->x2 + 1) * TERMGFX_TILE;
	int fy1 = b->y1 * TERMGFX_TILE, fy2 = (b->y2 + 1) * TERMGFX_TILE;
	int rx, rx2, ry, ry2, cx, cy, rw, rh;

	if (fx2 > TERMGFX_FB_W)
		fx2 = TERMGFX_FB_W;
	if (fy2 > TERMGFX_FB_H)
		fy2 = TERMGFX_FB_H;

	/* display rect = the pixels whose NN source falls in [fx1,fx2)x[fy1,fy2) */
	rx  = (fx1 * ew + TERMGFX_FB_W - 1) / TERMGFX_FB_W;
	rx2 = (fx2 * ew + TERMGFX_FB_W - 1) / TERMGFX_FB_W;
	ry  = (fy1 * ehc + TERMGFX_FB_H - 1) / TERMGFX_FB_H;
	ry2 = (fy2 * ehc + TERMGFX_FB_H - 1) / TERMGFX_FB_H;
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
	 * a black strip. Clamp to the frame; the bottom-most box may then fall
	 * short of a full vstep and no longer cover this box's own changed
	 * rows [ry+rh, ry2) -- the caller must treat that as stranding and fall
	 * back to a full frame (which always encodes the whole ehc height with
	 * no partial band at the bottom) rather than ship a box that silently
	 * drops those rows. */
	rh  = (ry2 - ry + vstep - 1) / vstep * vstep;
	if (ry + rh > ehc)
		rh = (ehc - ry) / vstep * vstep;   /* stay vstep-aligned at the frame bottom */
	rw  = rx2 - rx;

	*rx_out = rx; *ry_out = ry; *rw_out = rw; *rh_out = rh; *ry2_out = ry2;
}

static size_t termgfx_dirty_sixel_present(const uint8_t *fb, const uint8_t *last,
                                      const uint8_t *pal8, int ew, int eh,
                                      int icol, int irow, int cw, int ch)
{
	struct termgfx_box box[TERMGFX_MAX_COMPONENTS];
	int             ehc, nb, k;
	int             vstep = ch / termgfx_gcd(ch, 6) * 6;   /* LCM(ch,6): cell & band */
	/* SyncTERM: registers persist, box carries no palette (NONE). Every other
	 * terminal resets per image, so the box self-describes its used colors. */
	int             emit_pal = g_is_syncterm ? SIXEL_PAL_NONE : SIXEL_PAL_USED;
	size_t          total = 0;

	ehc = eh - eh % 6;                           /* the client's actual sixel height */
	if (ehc < 6)
		return 0;

	nb = termgfx_diff_coalesce(fb, last, box);
	if (nb <= 0)
		return 0;

	/* Stranding pre-pass: a bottom-clamped box that can't cover its own
	 * changed rows forces a full-frame fallback (see termgfx_box_display_rect()'s
	 * comment). That fallback has to be decided BEFORE any box is emitted --
	 * boxes are out_put() to the wire as each one succeeds, so if the check
	 * instead lived inside the emit loop, an earlier box's bytes could
	 * already be on the wire by the time a later (bottom) box strands,
	 * leaving a partial dirty box AND a full frame both sent for the same
	 * frame. Running the identical geometry here first, before a single
	 * out_put(), keeps the fallback atomic: either nothing is sent and the
	 * caller sends a full frame, or every emitted box is complete. */
	for (k = 0; k < nb; k++) {
		int rx, ry, rw, rh, ry2;

		termgfx_box_display_rect(&box[k], ew, ehc, cw, ch, vstep, &rx, &ry, &rw, &rh, &ry2);
		if (rw <= 0)
			continue;                               /* empty box: not stranding */
		if (ry + rh < ry2)
			return 0;                               /* fall back to a full frame */
	}

	for (k = 0; k < nb; k++) {
		int            rx, ry, rw, rh, ry2, cx, cy;
		const uint8_t *idx;
		size_t         sn;
		char           wrap[24];
		int            wn;

		termgfx_box_display_rect(&box[k], ew, ehc, cw, ch, vstep, &rx, &ry, &rw, &rh, &ry2);
		if (rw <= 0)
			continue;
		/* No ry+rh < ry2 check here: the pre-pass above already guarantees
		 * no box in this frame strands, so every box reaching this point is
		 * complete. */
		cx = rx / cw;
		cy = ry / ch;

		/* Encode the box at its exact CELL-ALIGNED height (ry/ry2 were snapped
		 * to whole cells above) -- do NOT round up to a 6px sixel band. A box
		 * taller than its cell region overdraws into the next cell, which a
		 * cell-anchored terminal (foot) clears when it lands the next sixel and
		 * only partly repaints -- leaving the thin black streaks seen in sparse
		 * scenes. sixel_encode() handles a height that isn't a whole number of
		 * 6px bands (the trailing band is partial); SF #258's round-up guarded
		 * SyncTERM's sixel decoder, and SyncTERM uses the JXL tier, never this
		 * sixel-dirty path. */
		idx = termgfx_pack_idx_rect(fb, ew, ehc, rx, ry, rw, rh);
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
 * returns 0), but termgfx_tier() never routes there, so a no-libjxl build never
 * pays for a doomed RGB888 expansion + guaranteed-fail encode call every
 * frame just to fall back to sixel anyway. --------------------------------*/
#define TERMGFX_TIER_SIXEL 0
#define TERMGFX_TIER_JXL   1

static int termgfx_tier(void)
{
#ifdef WITH_JXL
	if (g_jxl)
		return TERMGFX_TIER_JXL;
#endif
	return TERMGFX_TIER_SIXEL;   /* also the pre-reply default */
}

static const char *termgfx_tier_name(int t)
{
#ifdef WITH_JXL
	if (t == TERMGFX_TIER_JXL)
		return "jxl";
#else
	(void)t;
#endif
	return "sixel";
}

#ifdef WITH_JXL
static uint8_t *g_jxl_buf; static size_t g_jxl_cap;
static uint8_t *g_apc_buf; static size_t g_apc_cap;

/* Full-frame RGB888 expansion (door_io.c's door_pack_rgb() pattern): same NN
 * source mapping termgfx_scale_idx() uses for the sixel path, but through the
 * palette so JXL gets real pixels instead of indices -- SyncTERM's decoder
 * has no palette concept for JXL/PPM. */
static const uint8_t *termgfx_pack_rgb(const uint8_t *fb, const uint8_t *pal, int ew, int eh)
{
	int y;
	if (!ensure_cap(&g_rgb_buf, &g_rgb_cap, (size_t)ew * eh * 3))
		return NULL;
	for (y = 0; y < eh; y++) {
		const uint8_t *row = fb + (size_t)(y * TERMGFX_FB_H / eh) * TERMGFX_FB_W;
		uint8_t *      p   = g_rgb_buf + (size_t)y * ew * 3;
		int            x;
		for (x = 0; x < ew; x++) {
			const uint8_t *c = pal + (size_t)row[x * TERMGFX_FB_W / ew] * 3;
			*p++ = c[0]; *p++ = c[1]; *p++ = c[2];
		}
	}
	return g_rgb_buf;
}

/* Same, but a display sub-rect [rx,ry,rw,rh] (door_io.c's dr_pack_disp_rect()
 * pattern) -- the dirty-box counterpart of termgfx_pack_rgb(), sampled with the
 * identical NN mapping so the rect lines up with the held full JXL frame.
 * JXL is pixel-addressed (unlike sixel, which snaps dirty boxes out to whole
 * 8x16 cells because it can only place at a text cursor), so no cell
 * snapping here -- rx/ry/rw/rh stay pixel-exact. */
static const uint8_t *termgfx_pack_rgb_rect(const uint8_t *fb, const uint8_t *pal,
                                        int ew, int eh, int rx, int ry, int rw, int rh)
{
	int      i, j;
	uint8_t *p;

	if (!ensure_cap(&g_rgb_buf, &g_rgb_cap, (size_t)rw * rh * 3))
		return NULL;
	p = g_rgb_buf;
	for (j = 0; j < rh; j++) {
		const uint8_t *row = fb + (size_t)((ry + j) * TERMGFX_FB_H / eh) * TERMGFX_FB_W;
		for (i = 0; i < rw; i++) {
			const uint8_t *c = pal + (size_t)row[(rx + i) * TERMGFX_FB_W / ew] * 3;
			*p++ = c[0]; *p++ = c[1]; *p++ = c[2];
		}
	}
	return g_rgb_buf;
}

/* Per-session cache-file name (SF syncterm #256): two SyncTERM windows on the
 * same dialing entry share one cache dir, so a fixed name would collide.
 * Only used on the non-blob (Store+Draw) path; a blob-capable client never
 * touches the cache at all, but the name still has to exist to pass in. */
static const char *termgfx_jxl_name(void)
{
	static char name[32];
	if (name[0] == '\0')
		snprintf(name, sizeof name, "%s_%08x.jxl", app_name(), termgfx_session_salt());
	return name;
}

static size_t termgfx_emit_jxl(const uint8_t *fb, const uint8_t *pal8, int ew, int eh, int dx, int dy)
{
	const uint8_t *rgb = termgfx_pack_rgb(fb, pal8, ew, eh);
	size_t         n;

	if (rgb == NULL)
		return 0;
	n = termgfx_jxl_encode(&g_jxl_buf, &g_jxl_cap, rgb, ew, eh, 2.0f, 1);
	if (n == 0)
		return 0;   /* encode failure (or a no-libjxl stub): caller falls back to sixel */
	return termgfx_apc_image(&g_apc_buf, &g_apc_cap, termgfx_jxl_name(), "DrawJXL",
	                         g_jxl_buf, n, dx, dy, g_img_blob_ok);
}

/* present_rgbx's JXL tier: the tail of termgfx_emit_jxl() fed already-packed
 * RGB888 (from termgfx_scale_rgbx_to_rgb()), skipping the indexed termgfx_pack_rgb()
 * round-trip a truecolor source has no need of. Bytes land in g_apc_buf, same
 * as termgfx_emit_jxl(). */
static size_t termgfx_emit_jxl_rgb(const uint8_t *rgb, int ew, int eh, int dx, int dy)
{
	size_t n = termgfx_jxl_encode(&g_jxl_buf, &g_jxl_cap, rgb, ew, eh, 2.0f, 1);

	if (n == 0)
		return 0;   /* encode failure (or a no-libjxl stub): caller falls back to sixel */
	return termgfx_apc_image(&g_apc_buf, &g_apc_cap, termgfx_jxl_name(), "DrawJXL",
	                         g_jxl_buf, n, dx, dy, g_img_blob_ok);
}

/* Present a changed frame as JXL dirty rects. Returns total bytes sent, or 0
 * to tell the caller to send a full frame instead -- but only 0 when NOTHING
 * was staged; once a box has been staged a later box's failure returns the
 * partial total (and forces a full-frame resync next present via
 * g_dropped_frames) so the caller never stacks a full frame on top of partial
 * rects. Caller guarantees
 * g_have_last, an unchanged palette (a pure palette fade can leave the INDEX
 * buffer termgfx_diff_coalesce() diffs completely unchanged while every pixel's
 * displayed color moves -- unlike sixel's separate palette registers, JXL
 * bakes the palette into each frame's RGB pixels, so a fade MUST force a
 * full re-encode, exactly like the sixel path's own pal_dirty gate), and
 * that the client currently holds the previous JXL frame in blob mode
 * (g_img_blob_ok -- a non-blob dirty box would need its own unique cache
 * file per box, which this task doesn't build). */
static size_t termgfx_dirty_jxl_present(const uint8_t *fb, const uint8_t *last,
                                    const uint8_t *pal8, int ew, int eh, int dx, int dy)
{
	struct termgfx_box box[TERMGFX_MAX_COMPONENTS];
	int             k, nb;
	size_t          total = 0;

	nb = termgfx_diff_coalesce(fb, last, box);
	if (nb <= 0)
		return 0;

	for (k = 0; k < nb; k++) {
		int            fx1 = box[k].x1 * TERMGFX_TILE, fx2 = (box[k].x2 + 1) * TERMGFX_TILE;
		int            fy1 = box[k].y1 * TERMGFX_TILE, fy2 = (box[k].y2 + 1) * TERMGFX_TILE;
		int            rx, rx2, ry, ry2, rw, rh;
		const uint8_t *rgb;
		size_t         jn, an;

		if (fx2 > TERMGFX_FB_W)
			fx2 = TERMGFX_FB_W;
		if (fy2 > TERMGFX_FB_H)
			fy2 = TERMGFX_FB_H;

		/* display rect = the pixels whose NN source falls in [fx1,fx2)x[fy1,fy2);
		 * pixel-exact, no cell-snap (contrast termgfx_dirty_sixel_present()). */
		rx  = (fx1 * ew + TERMGFX_FB_W - 1) / TERMGFX_FB_W;
		rx2 = (fx2 * ew + TERMGFX_FB_W - 1) / TERMGFX_FB_W;
		ry  = (fy1 * eh + TERMGFX_FB_H - 1) / TERMGFX_FB_H;
		ry2 = (fy2 * eh + TERMGFX_FB_H - 1) / TERMGFX_FB_H;
		if (rx2 > ew)
			rx2 = ew;
		if (ry2 > eh)
			ry2 = eh;
		rw = rx2 - rx;
		rh = ry2 - ry;
		if (rw <= 0 || rh <= 0)
			continue;

		rgb = termgfx_pack_rgb_rect(fb, pal8, ew, eh, rx, ry, rw, rh);
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
		an = termgfx_apc_image(&g_apc_buf, &g_apc_cap, termgfx_jxl_name(), "DrawJXL",
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
static uint32_t g_dsr_ts[TERMGFX_DSR_RING];
static int      g_dsr_h, g_dsr_t;
static int      g_inflight;
static uint32_t g_pace_progress_ms;
static int      g_auto_depth = 3;
static uint32_t g_auto_adj_at;
static int      g_rt_high;
static uint32_t g_rtt_ms, g_rtt_min, g_rtt_min_at;

static void termgfx_dsr_sent(uint32_t now)
{
	g_dsr_ts[g_dsr_h] = now;
	g_dsr_h = (g_dsr_h + 1) % TERMGFX_DSR_RING;
}

/* A DSR cursor-position report came back: one in-flight frame was consumed.
 * (Forward-declared near csi_final(), above, so its 'R' case can call this
 * on every CPR reply -- including the startup 999;999 grid probe, when the
 * ring is still empty and this is a harmless no-op.) */
static void termgfx_pace_ack(void)
{
	uint32_t now = now_ms();

	if (g_inflight > 0)
		g_inflight--;
	if (g_dsr_h != g_dsr_t) {
		uint32_t rtt = now - g_dsr_ts[g_dsr_t];
		g_dsr_t = (g_dsr_t + 1) % TERMGFX_DSR_RING;
		if (termgfx_rtt_sample(&g_rtt_ms, &g_rtt_min, &g_rtt_min_at, &g_rt_high, rtt, now, 0, 0))
			termgfx_aimd_update(1, &g_auto_depth, &g_auto_adj_at,
			                    g_rtt_ms, g_rtt_min, g_rt_high, TERMGFX_AUTO_DEPTH_MAX, now);
	}
	g_pace_progress_ms = now;
}

/* present()'s last computed emit geometry -- stashed here purely so
 * termgfx_trace() can log it without taking new parameters, mirroring how the
 * pacing globals just above (g_inflight/g_auto_depth, maintained by
 * termgfx_pace_ack()/present() but only READ here) feed the same emitter.
 * Diagnostic-only: nothing downstream of the fit/center block (termgfx_termio_
 * present()'s "Fit + center") reads these back. Persisting across calls
 * means an early-out trace line (gated-grace/deadline/etc., logged before a
 * given present() call reaches the fit/center block) still reports the last
 * frame's real geometry instead of zeros. */
static int     g_trace_ew, g_trace_eh;         /* effective (fit+clamped) emit size */
static int     g_trace_dx, g_trace_dy;         /* JXL pixel offset; stay 0 without WITH_JXL */
static int     g_trace_cellh;                  /* cell height used for the sixel row-reserve */
static int     g_trace_icol, g_trace_irow;     /* text-grid column/row the image is centered at */

/* Emit one present-path trace line (<DOOR>_TRACE); see g_trace, above. */
static void termgfx_trace(const char *outcome, const char *tier, size_t bytes)
{
	unsigned ch = 0, ur = 0, dr = 0;

	if (g_trace == NULL)
		return;
	/* audio=<tier>/<chunks>/<underruns>/<dropped>: the stream's own counters,
	 * on the present line because that is the one line a trace always has --
	 * audio has no event of its own worth a line, and correlating a drop
	 * against the frame it happened under is the whole point. Zeros before
	 * the stream exists (no PCM yet, or a terminal that cannot play it).
	 *
	 * abacklog=<audio backlog>/<door drops>/<ring merges> is what the comic-
	 * intro investigation did not have and needed. The trace showed audio
	 * dropping 702 chunks and no way to see WHOSE bytes the module was looking
	 * at when it decided to; the first field is that number
	 * (termgfx_termio_audio_backlog(), audio's own share of the FIFO -- compare it
	 * against the whole stage's `bytes` on a full-frame line and the old bug is
	 * visible at a glance). The second is the door's OWN audio drops
	 * (g_audio_dropped), which must never be silent the way the reverted
	 * attempt's counter was. The third is g_aseg ring overflows, which are the
	 * only condition under which the first field can be too LARGE. */
	termgfx_stream_stats(g_stream, &ch, &ur, &dr);
	/* emit=<ew>x<eh>@<dx>,<dy> cell=<cellh> rc=<irow>,<icol>: the fit+center
	 * block's actual output (termgfx_termio_present()'s "Fit + center", above) --
	 * added to attribute an unpainted screen row to either a clamp (ew/eh
	 * short of canvas) or a repaint gap (ew/eh == canvas, dx/dy == 0), which
	 * canvas=WxH alone cannot distinguish. Values are the FIT/CENTER block's
	 * last computed geometry (g_trace_ew et al, see their doc comment) --
	 * stale (last frame's) on an early-out line logged before that block
	 * runs this call. */
	fprintf(g_trace, "%u %-13s %-5s %7zu inflight=%d depth=%d dropped=%u"
	        " canvas=%dx%d%s replied=%d sixel=%d jxl=%d audio=%d/%u/%u/%u"
	        " abacklog=%zu/%u/%u emit=%dx%d@%d,%d cell=%d rc=%d,%d\n",
	        (unsigned)now_ms(), outcome, tier, bytes,
	        g_inflight, g_auto_depth, (unsigned)g_dropped_frames,
	        g_canvas_w, g_canvas_h, g_canvas_exact ? "!" : "?",
	        g_probe_replied, g_have_sixel, g_jxl,
	        g_audio_tier, ch, ur, dr,
	        termgfx_termio_audio_backlog(), (unsigned)g_audio_dropped, g_aseg_merges,
	        g_trace_ew, g_trace_eh, g_trace_dx, g_trace_dy, g_trace_cellh,
	        g_trace_irow, g_trace_icol);
}

/* Dump raw terminal input (<DOOR>_TRACE only): the exact reply bytes the
 * terminal sent, escaped, so a trace shows WHAT the terminal answered to the
 * probe and WHEN -- e.g. whether/when the ESC[4;h;wt canvas report arrives. */
static void termgfx_trace_in(const uint8_t *buf, int n)
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

static void termgfx_fps_tick(int emitted)
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

/* Arm on FIRST USE rather than at init: the door's ini is read lazily through
 * app_ini() and -i is resolved during argv parsing, so first use is the
 * earliest point where BOTH are certainly known. Arming earlier has silently
 * ignored the ini in every sibling door that tried it. */
static void idle_ensure_armed(void)
{
	if (g_idle_armed)
		return;
	g_idle_armed = 1;
	termgfx_idle_init(&g_idle,
	                  (g_idle_arg >= 0) ? (unsigned)g_idle_arg : g_idle_timeout,
	                  g_idle_warn, termgfx_plat_now_ms());
	if (g_time_limit_ms > 0) {
		g_deadline_ms    = termgfx_plat_now_ms() + g_time_limit_ms;
		g_deadline_armed = 1;
	}
}

/* The BBS session limit. Runs from the pump for the same reason the idle poll
 * does -- this door presents only on a changed frame, so a present-driven check
 * would stall on a static screen.
 *
 * Deliberately AFTER idle_poll() in the pump: over the last minute this claims
 * the shared notice row, and it must win. The idle countdown can be dismissed
 * by pressing a key; this one cannot, and telling a player "press any key" when
 * no key will help would be a lie. */
static void deadline_poll(void)
{
	static unsigned last_shown = 0;
	int32_t         left_ms;
	unsigned        left;

	if (!g_deadline_armed)
		return;
	left_ms = (int32_t)(g_deadline_ms - termgfx_plat_now_ms());
	if (left_ms <= 0) {
		if (!g_deadline_logged) {
			g_deadline_logged = 1;
			fprintf(stderr, "termgfx: BBS time limit reached -- exiting\n");
		}
		g_quit = 1;               /* clean unwind, exactly as an idle exit does */
		return;
	}
	if (left_ms > TERMGFX_TIMELIMIT_WARN_S * 1000)
		return;                   /* not yet worth interrupting the player */

	left = (unsigned)((left_ms + 999) / 1000);
	if (left != last_shown) {
		last_shown = left;
		snprintf(g_idle_msg, sizeof g_idle_msg,
		         " Your time on the BBS is up in %u second%s ",
		         left, (left == 1) ? "" : "s");
		g_idle_showing = 1;       /* shares the notice row; see the note above */
		termgfx_stats_draw();     /* paint NOW: a static scene may never present */
		termgfx_termio_flush();
	}
	g_idle_showing = 1;
}

/* Feed the clock from REAL input; 1 => this input answered an on-screen
 * countdown and the caller must CONSUME it instead of queueing it for the
 * game. "Press any key" must not also press that key at the game. */
static int idle_wake(void)
{
	int woke;

	idle_ensure_armed();
	woke = g_idle_showing;
	termgfx_idle_activity(&g_idle, termgfx_plat_now_ms());
	if (woke) {
		g_idle_showing = 0;
		g_idle_clear   = 1;
	}
	return woke;
}

/* Poll the idle clock. Driven by termgfx_termio_pump() -- the door's "call
 * every poll" service point -- and deliberately NOT by the present path: this
 * door only presents when the engine's dirty flag says the frame CHANGED, so a
 * player who walks away at a static screen would produce no frames and the
 * countdown would stall exactly when it is needed most.
 *
 * For the same reason the countdown paints itself the moment its text changes
 * rather than waiting for the next present, which is the trick
 * termgfx_toggle_stats() already uses for the stats bar.
 *
 * Expiry sets g_quit, the door's existing clean-quit flag, rather than exiting
 * here: the engine then unwinds through its normal shutdown (saving whatever
 * it saves) and termgfx_termio_leave() restores the terminal, exactly as a
 * player-initiated Ctrl-Q does. */
static void idle_poll(void)
{
	static unsigned last_shown = 0;
	unsigned        left = 0;
	int             was_showing = g_idle_showing, changed = 0;

	idle_ensure_armed();
	switch (termgfx_idle_poll(&g_idle, termgfx_plat_now_ms(), &left)) {
		case TERMGFX_IDLE_WARN:
			if (left != last_shown) {
				/* "terminating", not "disconnecting": this ends the GAME and
				 * hands the player back to the BBS -- it does not drop the
				 * call. Same wording as every sibling door. */
				snprintf(g_idle_msg, sizeof g_idle_msg,
				         " Still there? Press any key -- terminating in %u second%s ",
				         left, (left == 1) ? "" : "s");
				last_shown = left;
				changed    = 1;
			}
			if (!was_showing || changed) {
				g_idle_showing = 1;
				termgfx_stats_draw();      /* paint NOW; a static scene may never present */
				termgfx_termio_flush();
			}
			g_idle_showing = 1;
			break;
		case TERMGFX_IDLE_EXPIRED:
			/* Latched: g_quit only ASKS the engine to stop, and it takes a few
			 * pump cycles to unwind -- without this the same line lands in the
			 * log three or four times on every idle exit. */
			if (!g_idle_expired_logged) {
				g_idle_expired_logged = 1;
				fprintf(stderr, "termgfx: idle timeout -- no user input; exiting\n");
			}
			g_quit = 1;
			break;
		default:
			last_shown = 0;
			if (was_showing) {
				g_idle_showing = 0;
				g_idle_clear   = 1;
				termgfx_stats_draw();      /* wipe it now, same reasoning */
				termgfx_termio_flush();
			}
			g_idle_showing = 0;
			break;
	}
}

static void termgfx_stats_draw(void)
{
	char               buf[160];
	int                off, brow;
	unsigned long long bpf;

	/* Bottom text row from the real grid (999;999 CPR), NOT g_canvas_h/16 --
	 * the cell is often shorter than 16px (foot ~13), which put the bar rows
	 * above the true bottom. */
	brow = termgfx_bottom_row();
	if (brow < 1)
		brow = 25;

	/* The idle countdown owns this row while it is up -- an imminent
	 * termination outranks telemetry -- and it is drawn by THIS function
	 * rather than a second writer, so the two can never overwrite each other.
	 * It draws even when the player never pressed Ctrl-S, hence the g_stats
	 * check moved below rather than guarding the whole function. */
	if (g_idle_showing) {
		off = snprintf(buf, sizeof buf,
		               "\x1b[%d;1H\x1b[1;37;44m%s\x1b[K\x1b[0m\x1b[?25l",
		               brow, g_idle_msg);
		if (off > 0 && off < (int)sizeof buf)
			out_put(buf, (size_t)off);
		return;
	}
	if (g_idle_clear) {
		/* Wipe it: nothing else repaints this row, so the last countdown would
		 * otherwise sit there. If the stats bar is on it redraws just below. */
		off = snprintf(buf, sizeof buf, "\x1b[%d;1H\x1b[0m\x1b[K\x1b[?25l", brow);
		if (off > 0 && off < (int)sizeof buf)
			out_put(buf, (size_t)off);
		g_idle_clear = 0;
	}
	if (!g_stats)
		return;
	bpf = g_fps > 0 ? ((uint64_t)g_bps / 8 / (unsigned)g_fps + 512) / 1024 : 0;
	/* No present completed in the last window (e.g. a paused game, or -- before
	 * the palette-storm fix -- a fade's dark gap): show '-' rather than a
	 * misleading "0fps 0KB/f". */
	if (g_fps > 0)
		off = snprintf(buf, sizeof buf,
		              "\x1b[%d;1H\x1b[30;46m%s %dfps %lluKB/f d%d %ums\x1b[K\x1b[0m\x1b[?25l",
		              brow, termgfx_tier_name(termgfx_tier()), g_fps, bpf, g_auto_depth, (unsigned)g_rtt_ms);
	else
		off = snprintf(buf, sizeof buf,
		              "\x1b[%d;1H\x1b[30;46m%s -fps -KB/f d%d %ums\x1b[K\x1b[0m\x1b[?25l",
		              brow, termgfx_tier_name(termgfx_tier()), g_auto_depth, (unsigned)g_rtt_ms);
	if (off < 0 || off >= (int)sizeof buf)
		return;
	out_put(buf, (size_t)off);
}

/* --- input event FIFO (mouse + keyboard) -----------------------------------
 * Fixed-capacity ring, drop-oldest on overflow: a stuck/never-polled backend
 * must not grow this without bound, and input latency from dropping a stale
 * event beats unbounded memory. 64 is far more than one mouse report burst
 * can produce (at most 2 events -- a MOVE plus a DOWN/UP/WHEEL -- per
 * report); an evdev physical-key report can coalesce more codes (a fast
 * chord), but still nowhere near this ring's depth, so overflow is not
 * expected in practice either way. */
#define TERMGFX_EVQ_CAP 64
static termgfx_input_event_t g_evq[TERMGFX_EVQ_CAP];
static int               g_evq_head;   /* index of the oldest queued event */
static int               g_evq_n;      /* queued events */

static void termgfx_push_event(const termgfx_input_event_t *ev)
{
	int slot;

	if (g_evq_n == TERMGFX_EVQ_CAP) {
		g_evq_head = (g_evq_head + 1) % TERMGFX_EVQ_CAP;   /* drop the oldest */
		g_evq_n--;
	}
	slot = (g_evq_head + g_evq_n) % TERMGFX_EVQ_CAP;
	g_evq[slot] = *ev;
	g_evq_n++;
}

int termgfx_termio_next_event(termgfx_input_event_t *ev)
{
	if (g_evq_n == 0)
		return 0;
	*ev = g_evq[g_evq_head];
	g_evq_head = (g_evq_head + 1) % TERMGFX_EVQ_CAP;
	g_evq_n--;
	return 1;
}

/* --- keyboard decode: legacy bytes, kitty CSI-u, SyncTERM evdev -----------
 * Ported from syncconquer/door/door_input.c's emit_key()/emit_tap()/
 * evdev_edge()/evdev_report(), trimmed to this door's neutral TERMGFX_KEY_*
 * events: no VK_* space, no WWKeyboard Down()-poll modifier brackets --
 * ScummVM's pollEvent() (door/syncscumm.cpp) wants discrete key-down/up
 * events, so a modifier's state folds into TERMGFX_MOD_* on the key it
 * accompanies rather than a standalone press/release of its own. */

static void termgfx_key_event(int keycode, int ascii, int mods, int down)
{
	termgfx_input_event_t ev;

	/* A genuine keystroke -- the one thing DSR pacing cannot forge. Safe as a
	 * single chokepoint here (unlike the sibling doors, which had to hook two
	 * or three dispatchers) because nothing synthesizes key events on this
	 * path: a legacy key is one KEY_DOWN and no release ever follows. */
	if (down && idle_wake())
		return;                       /* it answered the countdown: consumed */

	memset(&ev, 0, sizeof ev);
	ev.type    = down ? TERMGFX_EV_KEY_DOWN : TERMGFX_EV_KEY_UP;
	ev.keycode = keycode;
	ev.ascii   = ascii;
	ev.mods    = mods;
	termgfx_push_event(&ev);
}

/* A legacy/CSI key with no true release from the wire (a plain byte, or a
 * legacy -- non-kitty -- CSI/SS3 nav key): queue just the press. Unlike
 * door_input.c's emit_tap(), which synthesizes an immediate release too
 * (WWKeyboard's Down()-poll model needs one so a key can't stick "held"),
 * ScummVM's discrete keydown/keyup event stream has no such poll to
 * satisfy, so a legacy key is a single TERMGFX_EV_KEY_DOWN and nothing else --
 * the same shape a kitty/evdev key gets on ITS press edge, just without a
 * release edge ever following. */
static void termgfx_key_press(int keycode, int ascii, int mods)
{
	termgfx_key_event(keycode, ascii, mods, 1);
}

/* A legacy P_NORMAL byte outside the ESC/CSI machinery: translate to one key
 * press. 0x13 (Ctrl-S) and 'q'/0x03 (quit) never reach here -- consumed as
 * door hotkeys in parse_bytes()'s P_NORMAL case, above them. */
static void termgfx_key_byte(int c)
{
	int keycode = 0, ascii = 0, mods = 0;

	if (c == 0x0d)
		keycode = TERMGFX_KEY_ENTER;
	else if (c == 0x08 || c == 0x7f)
		keycode = TERMGFX_KEY_BACKSPACE;
	else if (c == 0x09)
		keycode = TERMGFX_KEY_TAB;
	else if (c >= 0x01 && c <= 0x1a) {
		keycode = 'a' + (c - 1);   /* Ctrl+letter */
		mods    = TERMGFX_MOD_CTRL;
	} else if (c >= 0x20 && c <= 0x7e) {
		keycode = c;
		ascii   = c;
	} else
		return;                     /* no mapping (other control bytes,
		                            * high bit set): drop */
	termgfx_key_press(keycode, ascii, mods);
}

/* --- SyncTERM evdev physical-key reports ------------------------------------
 * Negotiated off the CTDA cap-8 reply (csi_final()'s 'c' case, above).
 * Reports arrive as "CSI = code[;code...] K" (press) / "...k" (release),
 * csi_final()'s 'K'/'k' cases below. */
static unsigned g_evdev_mods;

/* Apply one physical key edge. Unlike door_input.c's evdev_edge(), a
 * modifier code only updates g_evdev_mods -- no standalone press/release
 * event of its own; see the section doc comment above for why. */
static void termgfx_evdev_edge(int code, int down)
{
	int keycode = 0, c;
	int mod = termgfx_evdev_modifier(code);   /* L/R Ctrl, Shift, Alt */

	if (mod != 0) {
		if (down)
			g_evdev_mods |= (unsigned)mod;
		else
			g_evdev_mods &= ~(unsigned)mod;
		return;
	}

	/* SyncTERM re-reports the keys already held when physical reports are
	 * enabled -- the Enter still down from the BBS menu. Drop those press
	 * edges (termgfx/keymode.h); releases always run, so nothing sticks. */
	if (down && termgfx_keymode_evdev_settling(&g_km, now_ms()))
		return;

	switch (code) {
		case 103: case 72: keycode = TERMGFX_KEY_UP;       break;   /* + numpad KP8 */
		case 108: case 80: keycode = TERMGFX_KEY_DOWN;     break;   /* + numpad KP2 */
		case 105: case 75: keycode = TERMGFX_KEY_LEFT;     break;   /* + numpad KP4 */
		case 106: case 77: keycode = TERMGFX_KEY_RIGHT;    break;   /* + numpad KP6 */
		case 102: case 71: keycode = TERMGFX_KEY_HOME;     break;   /* + numpad KP7 */
		case 107: case 79: keycode = TERMGFX_KEY_END;      break;   /* + numpad KP1 */
		case 104: case 73: keycode = TERMGFX_KEY_PAGEUP;   break;   /* + numpad KP9 */
		case 109: case 81: keycode = TERMGFX_KEY_PAGEDOWN; break;   /* + numpad KP3 */
		case 110: case 82: keycode = TERMGFX_KEY_INSERT;   break;   /* + numpad KP0 */
		case 111: case 83: keycode = TERMGFX_KEY_DELETE;   break;   /* + numpad KP-dot */
		case 76:           keycode = TERMGFX_KEY_KP5;      break;   /* numpad KP5 (center): AGI stop */
		case 59:  keycode = TERMGFX_KEY_F1; break;
		case 60:  keycode = TERMGFX_KEY_F2; break;
		case 61:  keycode = TERMGFX_KEY_F3; break;
		case 62:  keycode = TERMGFX_KEY_F4; break;
		case 63:  keycode = TERMGFX_KEY_F5; break;
		case 64:  keycode = TERMGFX_KEY_F6; break;
		case 65:  keycode = TERMGFX_KEY_F7; break;
		case 66:  keycode = TERMGFX_KEY_F8; break;
		case 67:  keycode = TERMGFX_KEY_F9; break;
	}
	if (keycode) {
		termgfx_key_event(keycode, 0, 0, down);
		return;
	}

	c = termgfx_evdev_ascii(code, (g_evdev_mods & TERMGFX_MOD_SHIFT) != 0);
	if (c == 0)
		return;
	if ((g_evdev_mods & TERMGFX_MOD_CTRL) && (c | 0x20) >= 'a' && (c | 0x20) <= 'z') {
		/* Door-level Ctrl-S (stats) and Ctrl-Q/Ctrl-C (quit) hotkeys,
		 * consumed here too -- SyncTERM's evdev mode never reaches the
		 * P_NORMAL raw byte that catches them in legacy mode. Fire on
		 * press, swallow the release too. A bare, unmodified 'q' is NOT
		 * reserved (see below): it must reach ScummVM's text entry. */
		if ((c | 0x20) == 's') {
			if (down)
				termgfx_toggle_stats();
			return;
		}
		if ((c | 0x20) == 'c' || (c | 0x20) == 'q') {
			if (down)
				g_quit = 1;
			return;
		}
		/* Ctrl+<menu_letter> (default Ctrl-G): open the GMM. Same as the kitty
		 * and legacy-byte paths -- SyncTERM evdev reaches only here. */
		if (g_menu_letter && (c | 0x20) == g_menu_letter) {
			if (down)
				g_menu = 1;
			return;
		}
		termgfx_key_event(c | 0x20, 0, TERMGFX_MOD_CTRL, down);
		return;
	}
	termgfx_key_event(c, c, 0, down);
}

/* A physical key report ("CSI = code[;code...] K/k"): apply every coalesced
 * edge. csi_params() skips the leading '=' marker. */
static void termgfx_evdev_report(int down)
{
	int codes[16], nc, i;

	nc = csi_params(codes, 16);
	for (i = 0; i < nc; i++)
		termgfx_evdev_edge(codes[i], down);
}

/* --- SGR mouse: report coords -> TERMGFX_FB_W x TERMGFX_FB_H game coords ----------
 * Ported from syncconquer/door/door_input.c:305-417 (mouse_event), with two
 * differences: this door has only ONE coordinate space to map into (the
 * fixed 320x200 game surface -- no separate GUI-overlay space to switch
 * between), and there is no legacy modifier-synthesis-on-click block --
 * ScummVM reads discrete button-down/up events, not a held-modifier poll, so
 * a click never needs to bracket synthetic Shift/Alt/Ctrl taps around it;
 * keyboard modifiers arrive as their own key events (Task 5).
 *
 * termgfx_image_rect(), below, is the SAME fit+center math termgfx_termio_present() uses
 * to place the frame, so a report always maps against the rect the terminal
 * is actually looking at (sixel's reserved bottom row included) rather than
 * a second, potentially-stale idea of where the image is. */

/* Fit the TERMGFX_FB_W x TERMGFX_FB_H frame into the terminal's canvas and center it
 * -- extracted from termgfx_termio_present()'s pre-M3 inline "Fit + center" block
 * (same logic, unchanged) so BOTH present() and termgfx_termio_mouse_report() key off
 * one computation instead of two that could drift apart. Pixel-space only:
 * dx and dy (output via the pointers) give the offset of the image's
 * top-left corner within the canvas (the SIXEL tier's text-cell CUP origin
 * is a cheap re-derivation of this in present(), below -- col/row and dx/dy
 * are independent outputs of the same center call, so recovering one from
 * the other is exact).
 *
 * The SIXEL tier reserves the LAST text row and positions on the terminal's
 * REAL cell grid; the JXL tier keeps the full canvas. Why the sixel tier
 * differs: a sixel is drawn at the text cursor, and one drawn into the
 * bottom row scrolls the page on a ?80l terminal -- seen as the picture
 * bouncing when a scene repaints. And the cell is often shorter than the
 * hardcoded 16px (foot ~13), which both mis-sized any reserve and mis-placed
 * the cell origin. So use the real cell height (ESC[16t, else the canvas /
 * 999;999-CPR grid) to reserve exactly one row and to center on a fractional
 * cell. JXL is pixel-addressed and does not scroll at the cursor, so it
 * always uses the full canvas. */
/* Source-parameterized core of termgfx_image_rect(): identical logic, but the
 * frame's native size (sw x sh) is a parameter instead of the TERMGFX_FB_*
 * constants, so present_rgbx() (a truecolor source with its own w x h) can fit
 * against the same canvas math. termgfx_image_rect() below is the unchanged
 * TERMGFX_FB_W x TERMGFX_FB_H wrapper the indexed present()/mouse path calls -- passing
 * those two constants reproduces its former behavior exactly (the source dims
 * are used only by the termgfx_geom_fit() call; everything after keys off the
 * canvas and the resulting ew/eh). */
static void termgfx_image_rect_src(int sw, int sh, int *ew, int *eh, int *dx, int *dy)
{
	int tier  = termgfx_tier();
	int cellh = g_cell_h > 0 ? g_cell_h
	          : (g_term_rows > 0 ? (g_canvas_h + g_term_rows / 2) / g_term_rows : 16);
	int fit_h = g_canvas_h;

	if (tier == TERMGFX_TIER_SIXEL && g_term_rows > 0 && g_canvas_h > cellh)
		fit_h = g_canvas_h - cellh;   /* keep the image off the last row */

	termgfx_geom_fit(g_canvas_w, fit_h, sw, sh, TERMGFX_SCALE_MAX, ew, eh);
	/* Effective sixel ceiling -- SIXEL tier only (see g_gfx_max_w's doc
	 * comment, above, for the full precedence rationale). JXL/APC frames
	 * aren't subject to xterm's declared-raster discard (a different wire
	 * format entirely), and the terminals that reach the JXL tier are
	 * SyncTERM/CTerm builds that already answered DA1/CTDA, not a silent
	 * xterm. */
	if (tier == TERMGFX_TIER_SIXEL) {
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
		termgfx_geom_gfx_clamp(gmax_w, gmax_h, ew, eh);
	}

	if (tier == TERMGFX_TIER_SIXEL && g_term_rows > 0) {
		double cw  = g_term_cols > 0 ? (double)g_canvas_w / g_term_cols : 8.0;
		double chd = (double)g_canvas_h / g_term_rows;
		int    icol, irow;   /* unused here -- present() re-derives them from dx/dy */

		termgfx_geom_center_ex(g_canvas_w, fit_h, *ew, *eh, cw, chd, dx, dy, &icol, &irow);
	} else {
		int icol, irow;

		termgfx_geom_center(g_canvas_w, g_canvas_h, *ew, *eh, 8, 16, dx, dy, &icol, &irow);
	}
}

static void termgfx_image_rect(int *ew, int *eh, int *dx, int *dy)
{
	termgfx_image_rect_src(TERMGFX_FB_W, TERMGFX_FB_H, ew, eh, dx, dy);
}

static void termgfx_termio_mouse_report(int b, int col, int row, int release)
{
	/* Mouse activity is presence too, motion included: a point-and-click
	 * adventure can go a long while with no keystroke at all. */
	if (idle_wake())
		return;

	int                     ew = 0, eh = 0, dx = 0, dy = 0, cw, ch, px, py, gx, gy;
	termgfx_mouse_report_t  rep;
	termgfx_input_event_t       ev;

	/* Auto-detect SGR-Pixels: a reported coord past the text grid proves the
	 * terminal is sending PIXELS (mode 1016 live) even if the DECRQM
	 * handshake never confirmed it -- some terminals honor ?1016h but don't
	 * answer ?1016$p. Cell coords never exceed the grid, so this can't
	 * false-trigger on a genuinely cell-granular terminal. */
	if (!termgfx_mouse_pixels(&g_mouse)
	    && ((g_term_cols > 0 && col > g_term_cols) || (g_term_rows > 0 && row > g_term_rows)))
		termgfx_mouse_note_pixel_report(&g_mouse);

	termgfx_image_rect(&ew, &eh, &dx, &dy);
	cw = g_cell_w > 0 ? g_cell_w : 8;
	ch = g_cell_h > 0 ? g_cell_h : 16;
	if (ew <= 0)
		ew = TERMGFX_FB_W;
	if (eh <= 0)
		eh = TERMGFX_FB_H;

	/* Under SGR-Pixels the report already carries 1-based canvas pixels --
	 * use them directly. Otherwise SGR reports a 1-based text CELL: convert
	 * to that cell's center in the terminal's own canvas pixels. Either way
	 * px/py end up in canvas pixels; clamp to the displayed-image rect, then
	 * rescale into game (TERMGFX_FB_W x TERMGFX_FB_H) coords. */
	if (termgfx_mouse_pixels(&g_mouse)) {
		px = col - 1;
		py = row - 1;
	} else {
		px = (col - 1) * cw + cw / 2;
		py = (row - 1) * ch + ch / 2;
		/* A cell-granular terminal can only place the cursor at cell
		 * CENTERS, which sit cw/2 or ch/2 px inside each canvas edge -- so
		 * the extreme game rows/cols are otherwise unreachable. Snap the
		 * FIRST and LAST row/col out to the image-rect edges. Unlike
		 * syncconquer (which leaves the top short to keep Red Alert's
		 * scroll-up zone off the menu tabs), Beneath a Steel Sky NEEDS the
		 * very top reachable: its inventory bar only drops when the pointer
		 * reaches game y=0, and the first row's cell center (ch/2 px down)
		 * never maps there. Snapping all four edges makes the whole play
		 * area -- top edge included -- reachable. */
		if (g_term_cols > 0 && col <= 1)
			px = dx;
		if (g_term_cols > 0 && col >= g_term_cols)
			px = dx + ew - 1;
		if (g_term_rows > 0 && row <= 1)
			py = dy;
		if (g_term_rows > 0 && row >= g_term_rows)
			py = dy + eh - 1;
	}
	if (px < dx)
		px = dx;
	if (px >= dx + ew)
		px = dx + ew - 1;
	if (py < dy)
		py = dy;
	if (py >= dy + eh)
		py = dy + eh - 1;

	gx = (px - dx) * TERMGFX_FB_W / ew;
	gy = (py - dy) * TERMGFX_FB_H / eh;
	if (gx < 0)
		gx = 0;
	if (gx >= TERMGFX_FB_W)
		gx = TERMGFX_FB_W - 1;
	if (gy < 0)
		gy = 0;
	if (gy >= TERMGFX_FB_H)
		gy = TERMGFX_FB_H - 1;

	/* Per-report diagnostic (<DOOR>_TRACE only): the raw cell/pixel
	 * report, the mode it was read in, the image rect it mapped against, and
	 * the game coords it produced -- so a "can't reach the top edge" or a
	 * "stale cursor" report can be diagnosed from a node trace without a live
	 * capture. Gated on g_trace, so it costs nothing unless tracing is on. */
	if (g_trace != NULL)
		fprintf(g_trace, "%u MOUSE b=%d col=%d row=%d %s rect=%dx%d@%d,%d -> g=%d,%d\n",
		        (unsigned)now_ms(), b, col, row,
		        termgfx_mouse_pixels(&g_mouse) ? "px1016" : "cell",
		        ew, eh, dx, dy, gx, gy);

	/* Emit order: always a MOVE (position), then a DOWN/UP for a button
	 * (release -> UP) or a WHEEL for a notch. A plain motion report queues
	 * just the MOVE. */
	memset(&ev, 0, sizeof ev);
	ev.type = TERMGFX_EV_MOUSE_MOVE;
	ev.x    = gx;
	ev.y    = gy;
	termgfx_push_event(&ev);

	termgfx_mouse_report(&g_mouse, b, col, row, release, &rep);
	switch (rep.kind) {
		case TERMGFX_SGR_BUTTON:
			memset(&ev, 0, sizeof ev);
			ev.type   = release ? TERMGFX_EV_MOUSE_UP : TERMGFX_EV_MOUSE_DOWN;
			ev.x      = gx;
			ev.y      = gy;
			ev.button = rep.button;
			termgfx_push_event(&ev);
			break;
		case TERMGFX_SGR_WHEEL:
			memset(&ev, 0, sizeof ev);
			ev.type  = TERMGFX_EV_WHEEL;
			ev.x     = gx;
			ev.y     = gy;
			ev.wheel = rep.wheel;
			termgfx_push_event(&ev);
			break;
		case TERMGFX_SGR_MOVE:
		default:
			break;   /* motion-only: the MOVE above already covers it */
	}
}

/* --- piece 9: termgfx_termio_present() ties 1-8 together (door_io.c:2707+
 * door_io_present() shape). ------------------------------------------------*/
static uint8_t g_last_fb[TERMGFX_FB_W * TERMGFX_FB_H];
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

/* A frame that a DEFER gate (pacing or backpressure, below) held back
 * instead of sending -- retained here so termgfx_termio_tick() can retry it once the
 * gate clears, even with no further engine-side present() call. Without
 * this, the last frame of a burst on a now-static screen (the F5 panel, the
 * half-erased speech-toggle X) sits unsent until the NEXT updateScreen(),
 * which nothing guarantees will ever come (present() is edge-driven off the
 * engine's own dirty flag). Only the two pacing/backpressure gates retain --
 * NOT the startup grace/canvas-hold gates just above present()'s body, which
 * self-retry every frame of the engine's own boot loop and would just waste
 * a copy retaining a frame that is about to be superseded anyway. */
static uint8_t g_pending_idx[TERMGFX_FB_W * TERMGFX_FB_H];
static uint8_t g_pending_pal[768];
static int     g_present_pending;

/* Shared by both defer gates below: retain idx/pal768 into g_pending_idx/
 * g_pending_pal and mark a frame pending. The idx != g_pending_idx guard
 * skips a self-copy when THIS call already is a retry (tick() calls
 * present() with g_pending_idx itself). */
static void termgfx_present_retain(const uint8_t *idx, const uint8_t *pal768)
{
	if (idx != g_pending_idx) {
		memcpy(g_pending_idx, idx, TERMGFX_FB_W * TERMGFX_FB_H);
		memcpy(g_pending_pal, pal768, 768);
	}
	g_present_pending = 1;
}

void termgfx_termio_present(const uint8_t *idx, const uint8_t *pal768)
{
	int      pal_dirty, geom_changed, tier, tier_changed;
	int      ew, eh, icol, irow, emit_pal;
#ifdef WITH_JXL
	int      dx = 0, dy = 0;
#endif
	size_t   n = 0, dn;   /* n: 0 until an encoder sets it; MSVC's C4701 flow
	                       * analysis can't prove the tier if/else covers every
	                       * path to the trace calls below, and ScummVM builds
	                       * with /we4701 (warning-as-error). 0 is the truthful
	                       * "nothing emitted" byte count for the trace. */
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
		if (g_capture_frames >= TERMGFX_CAPTURE_MAX_FRAMES)
			return;
		n = sixel_encode(&g_sixel_buf, &g_sixel_cap, idx, TERMGFX_FB_W, TERMGFX_FB_H, pal768, 1);
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
	termgfx_termio_pump();
	if (g_hangup)
		return;

	/* The startup holds below are bounded from the FIRST present attempt, not
	 * from termgfx_termio_init(): the engine can boot for longer than TERMGFX_PROBE_GRACE_MS,
	 * so an init-anchored deadline would already be expired the first time we
	 * ever try to draw and would never actually hold anything (the intro then
	 * renders against the default 640x400 guess -- small, upper-left -- until
	 * the report is finally read). Anchoring here gives the terminal a real
	 * window, however long the boot took. */
	if (g_first_present_ms == 0)
		g_first_present_ms = now_ms();

	/* Startup grace: hold the first frame(s) until the capability probe
	 * answers (termgfx_termio_init()'s DA1/CTDA/JXL burst), so a silent terminal
	 * gets a bounded wait rather than an indefinite one. termgfx_tier() only
	 * ever picks JXL once the probe has actually confirmed it (g_jxl), so
	 * holding here is enough to keep the first frame from guessing sixel
	 * against a JXL-only reply that just hasn't landed yet. (The exact pixel
	 * canvas is waited on separately, below, but only for a terminal that
	 * actually has a graphics tier -- a no-graphics terminal is turned away by
	 * the gate in between and must not be held here for a canvas it will never
	 * use.) */
	if (!g_probe_replied && (int32_t)(now_ms() - g_first_present_ms) < TERMGFX_PROBE_GRACE_MS) {
		termgfx_trace("gated-grace", termgfx_tier_name(termgfx_tier()), 0);
		return;
	}

	/* Non-graphics-terminal gate (deferred here from Task 7). syncscumm renders
	 * only via sixel or JXL. If the terminal ANSWERED the capability probe but
	 * has advertised neither tier, wait out the JXL reply window (a sixel DA1
	 * would already have set g_have_sixel) and then conclude it cannot display
	 * the game: emit a one-time plain-text notice and request quit, rather than
	 * spraying unrenderable sixel DCS at it (termgfx_tier() otherwise defaults to
	 * sixel). A terminal that stayed *silent* (no DA1 at all) is left to the
	 * historical default-to-sixel path -- a graphics terminal that simply
	 * doesn't answer device-attributes shouldn't be turned away. */
	if (!g_have_sixel && !g_jxl && g_probe_replied) {
		if (!g_jxl_done
		    && (int32_t)(now_ms() - g_probe_start_ms) <= TERMGFX_GFX_SETTLE_MS) {
			termgfx_trace("gated-grace", termgfx_tier_name(termgfx_tier()), 0);
			return;   /* JXL cap may still be in flight -- keep holding */
		}
		if (!g_no_gfx_notified) {
			out_puts("\r\n\x1b[0m\r\n"
			         "  This game requires a terminal with sixel or JXL graphics"
			         " support\r\n"
			         "  (such as SyncTERM). This terminal reports neither, so the"
			         " game\r\n"
			         "  cannot be displayed.\r\n\r\n");
			termgfx_termio_flush();
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
	if (!g_canvas_exact && (int32_t)(now_ms() - g_first_present_ms) < TERMGFX_PROBE_GRACE_MS) {
		termgfx_trace("gated-grace", termgfx_tier_name(termgfx_tier()), 0);
		return;
	}

	termgfx_termio_flush();   /* drain whatever's left of the previous frame first */
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
	 * the far shorter TERMGFX_PALSTORM_MIN_MS unstick instead: the fade is
	 * monotonic so a force-sent frame just carries the newest palette, and the
	 * g_out_len backpressure gate just below still holds the next frame until
	 * this one has fully left the stage -- so a short unstick paces evenly to
	 * the link's actual rate rather than overrunning it into a drop-storm. */
	if (g_inflight >= g_auto_depth) {
		uint32_t deadline = (pal_dirty && g_have_last) ? TERMGFX_PALSTORM_MIN_MS
		                                               : TERMGFX_PACE_DEADLINE_MS;
		if ((int32_t)(now_ms() - g_pace_progress_ms) > (int32_t)deadline) {
			g_inflight = 0;
			g_dsr_t    = g_dsr_h;   /* drop the stale unacked DSR timestamps */
			termgfx_trace("deadline", termgfx_tier_name(termgfx_tier()), 0);
		} else {
			/* Retain so termgfx_termio_tick() can retry once g_inflight drains --
			 * see g_present_pending's doc comment and termgfx_present_retain(). */
			termgfx_present_retain(idx, pal768);
			termgfx_trace("gated-inflight", termgfx_tier_name(termgfx_tier()), 0);
			return;
		}
	}
	/* Backpressure gate: the FIFO did not empty, so don't stack another frame
	 * on top of it. Note this is read AFTER the unconditional termgfx_termio_flush()
	 * above, so it is not "are there bytes staged" -- it is "did a flush
	 * attempt fail to place them", i.e. the socket is in EAGAIN and the link is
	 * genuinely behind.
	 *
	 * The audio fix made g_out_len no longer purely video (audio shares this
	 * FIFO -- see the staging block at the top of this file), so it is worth
	 * being explicit that the gate still means what it always meant. It does.
	 * The question it asks is about the LINK, not about whose bytes are queued:
	 * a frame staged now would sit behind those bytes whatever they are. The
	 * only new behavior is that ~2KB of undrainable audio can now hold a frame
	 * -- but that requires the socket to be blocked at that instant, which is
	 * exactly when a frame should be held, and it costs one skipped frame
	 * (~80ms, invisible) where the converse -- letting frames pile onto a
	 * blocked link -- is what feeds out_put()'s drop path.
	 *
	 * Deliberately NOT changed to gate on audio bytes only: that would stall
	 * video behind the cushion. Deliberately NOT removed: the stage is finite
	 * and this is what bounds it. */
	if (g_out_len > 0) {
		/* Retain, same as the gated-inflight branch above -- a static screen
		 * that lands here on its last frame of a burst (the FIFO hasn't
		 * drained yet) would otherwise strand exactly like the pacing case. */
		termgfx_present_retain(idx, pal768);
		termgfx_trace("gated-outlen", termgfx_tier_name(termgfx_tier()), 0);
		return;
	}

	/* Past both defer gates above: this frame is no longer stranded -- it
	 * will either be sent below or turns out identical to what the terminal
	 * already shows (the whole-frame dedupe further down, which returns
	 * before ever reaching a commit). Either way whatever was retained for
	 * it has been handled, so clear it HERE rather than only past the
	 * dedupe return. A retry (via termgfx_termio_tick()) whose retained frame
	 * happens to equal g_last_fb -- e.g. gate A defers frame A, then while
	 * still congested a byte-identical frame B overwrites the retain, then
	 * the gate clears and the retry hits the dedupe path -- must still
	 * clear pending, or termgfx_termio_tick() re-invokes termgfx_termio_present() (a
	 * 64000-byte memcmp, plus a stats redraw + flush if the bar is on)
	 * every poll forever on an otherwise-idle static screen. If a LATER
	 * frame defers again, the gate code above re-retains -- that is
	 * correct and unaffected by clearing here. */
	g_present_pending = 0;

	/* Close a DCS a previous drop left dangling (see the drop check below)
	 * before this frame's own bytes go out. Harmless if g_need_st is unset:
	 * an ST with no open DCS is a no-op. */
	if (g_need_st) {
		out_puts("\x1b\\");
		g_need_st = 0;
	}
	dropped_before = g_dropped_frames;

	tier = termgfx_tier();

	/* Fit + center, via termgfx_image_rect() -- the SAME helper the SGR mouse
	 * mapper (termgfx_termio_mouse_report(), above) calls, so a report always maps
	 * against the exact rect the frame below is drawn in. See that helper's
	 * doc comment for the sixel-vs-JXL placement rationale (reserved bottom
	 * row, pixel-offset vs text-cell origin); this block only turns its
	 * pixel-space dx/dy back into icol/irow for the sixel CUP.
	 *
	 * Computed BEFORE geom_changed/dedupe below (not after, as originally):
	 * the gfx-ceiling clamp inside termgfx_image_rect() can shrink ew/eh on its
	 * own, with the canvas itself unchanged, whenever a late XTSMGRAPHICS
	 * reply lands after the first frame already went out -- geom_changed has
	 * to see THAT, or a static scene would dedupe/dirty-diff against a
	 * last-sent frame that was emitted at the old, unclamped size. */
	{
		int cellh = g_cell_h > 0 ? g_cell_h
		          : (g_term_rows > 0 ? (g_canvas_h + g_term_rows / 2) / g_term_rows : 16);
		int rdx, rdy;   /* pixel offset from termgfx_image_rect(); rdx/cw, rdy/chd
		                 * below recovers the SAME icol/irow
		                 * termgfx_geom_center_ex()/_center() computed inline
		                 * here before the extraction -- col/row and dx/dy are
		                 * independent outputs of that math, so re-deriving one
		                 * from the other is exact, not an approximation. */

		termgfx_image_rect(&ew, &eh, &rdx, &rdy);

		if (tier == TERMGFX_TIER_SIXEL && g_term_rows > 0) {
			double cw  = g_term_cols > 0 ? (double)g_canvas_w / g_term_cols : 8.0;
			double chd = (double)g_canvas_h / g_term_rows;
			icol = 1 + (int)(rdx / cw);
			irow = 1 + (int)(rdy / chd);
		} else {
			/* dx/dy (pixel offset) feed the pixel-addressed JXL APC; sixel
			 * (no CPR grid) falls back here and uses icol/irow only. */
			icol = 1 + rdx / 8;
			irow = 1 + rdy / 16;
#ifdef WITH_JXL
			dx = rdx;
			dy = rdy;
#endif
		}

		/* Stash for termgfx_trace() -- see g_trace_ew's doc comment, above. */
		g_trace_ew = ew;
		g_trace_eh = eh;
		g_trace_cellh = cellh;
		g_trace_icol = icol;
		g_trace_irow = irow;
#ifdef WITH_JXL
		g_trace_dx = dx;
		g_trace_dy = dy;
#endif
	}

	tier_changed = (g_last_tier != tier);
	geom_changed = (g_last_canvas_w != g_canvas_w || g_last_canvas_h != g_canvas_h
	               || g_last_ew != ew || g_last_eh != eh);

	/* Whole-frame de-dupe: fb + palette + canvas/emit geometry + tier all
	 * unchanged since the last SENT frame -> nothing new to draw. tier can
	 * in practice only change once per session (termgfx_tier() settles as soon
	 * as the startup probe answers, before any frame has gone out), but
	 * checking it here costs nothing and keeps this correct if that ever
	 * stops being true. */
	if (g_have_last && !pal_dirty && !geom_changed && !tier_changed
	    && memcmp(g_last_fb, idx, TERMGFX_FB_W * TERMGFX_FB_H) == 0) {
		termgfx_fps_tick(0);
		if (g_stats) {
			termgfx_stats_draw();
			termgfx_termio_flush();
		}
		termgfx_trace("dedupe", termgfx_tier_name(tier), 0);
		return;
	}

	/* SyncTERM persists sixel color registers across images -- (re)send them
	 * (FULL) only on a real palette change, or when (re)entering the sixel
	 * tier from something else (JXL never defined them); else NONE, reusing
	 * what the terminal already holds. Other sixel terminals reset registers
	 * per image, so non-SyncTERM always self-describes with the frame's
	 * used-colour subset instead. */
	emit_pal = (pal_dirty || g_last_tier != TERMGFX_TIER_SIXEL)
	           ? SIXEL_PAL_FULL : SIXEL_PAL_NONE;
	if (!g_is_syncterm)
		emit_pal = SIXEL_PAL_USED;

	/* g_present_pending was already cleared above, past both defer gates --
	 * see that comment. A frame that gets dropped by out_put()'s stage-full
	 * guard just below still leaves it cleared -- that is fine and
	 * deliberate: a drop already has its own resync path (g_need_st/
	 * g_have_last), and re-driving a gate's retain for a dropped frame
	 * would fight that path instead of helping it. */

	/* Stats bar BEFORE the (possibly huge) image, not after: a palette-storm
	 * full frame can run past out_put()'s 256KB stage, and once that guard
	 * pins g_out_len at exactly its cap, EVERY later out_put() call this
	 * present() makes -- including a trailing termgfx_stats_draw() -- computes
	 * zero room and silently drops (out_put()'s own "stage stayed full"
	 * path), with no resync marker the way a dropped image gets (g_need_st/
	 * g_have_last). That starved the bar during exactly the big-frame scenes
	 * (fades, palette storms) a player is most likely to have it open for --
	 * it would show once on Ctrl-S, then never again until frames shrank
	 * back under the cap. Queuing it first costs nothing (it always fits by
	 * itself) and guarantees it wins the race for stage space every time. */
	termgfx_fps_tick(1);
	termgfx_stats_draw();

	dn = 0;
	if (tier == TERMGFX_TIER_SIXEL) {
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
			dn = termgfx_dirty_sixel_present(idx, g_last_fb, pal768, ew, eh,
			                             icol, irow, cdw, cdh);

		if (dn != 0) {
			n = dn;   /* dirty rects already sent */
		} else {
			char cup[24];
			int  cn = snprintf(cup, sizeof cup, "\x1b[%d;%dH", irow, icol);
			out_put(cup, (size_t)cn);
			n = termgfx_emit_sixel(idx, pal768, ew, eh, emit_pal);
			out_put(g_sixel_buf, n);
		}
	}
#ifdef WITH_JXL
	else {   /* TERMGFX_TIER_JXL */
		if (g_have_last && !pal_dirty && !geom_changed && !tier_changed && g_img_blob_ok)
			dn = termgfx_dirty_jxl_present(idx, g_last_fb, pal768, ew, eh, dx, dy);

		if (dn != 0) {
			n = dn;   /* dirty rects already sent */
		} else {
			n = termgfx_emit_jxl(idx, pal768, ew, eh, dx, dy);
			if (n != 0) {
				out_put(g_apc_buf, n);
			} else {
				/* encode failure this frame (or a no-libjxl stub -- can't
				 * happen here since termgfx_tier() only returns JXL when
				 * WITH_JXL is compiled in and the probe confirmed it, but a
				 * transient libjxl failure is still possible): fall back to
				 * sixel for just this frame, same as door_io.c. */
				char cup[24];
				int  cn = snprintf(cup, sizeof cup, "\x1b[%d;%dH", irow, icol);
				out_put(cup, (size_t)cn);
				n = termgfx_emit_sixel(idx, pal768, ew, eh, 1);
				out_put(g_sixel_buf, n);
				tier = TERMGFX_TIER_SIXEL;
			}
		}
	}
#endif

	out_put("\x1b[6n", 4);   /* DSR: paces the next frame */
	g_inflight++;
	g_pace_progress_ms = now_ms();
	termgfx_dsr_sent(g_pace_progress_ms);

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
		memcpy(g_last_fb, idx, TERMGFX_FB_W * TERMGFX_FB_H);
		memcpy(g_last_pal, pal768, 768);
		g_last_canvas_w = g_canvas_w;
		g_last_canvas_h = g_canvas_h;
		g_last_ew       = ew;
		g_last_eh       = eh;
		g_last_tier     = tier;
		g_have_last     = 1;
	}

	if (g_dropped_frames != dropped_before)
		termgfx_trace("dropped", termgfx_tier_name(tier), n);
	else if (dn != 0)
		termgfx_trace("dirty", termgfx_tier_name(tier), n);
	else if (pal_dirty)
		termgfx_trace("full-palstorm", termgfx_tier_name(tier), n);
	else
		termgfx_trace("full", termgfx_tier_name(tier), n);

	termgfx_termio_flush();
}

/* Quantized palette scratch for present_rgbx()'s sixel tier (and its JXL->sixel
 * fallback / capture branch): termgfx_quant_rgb() writes 256 RGB triples here
 * each frame, which sixel_encode() then emits. Frame-local; the truecolor path
 * has no persistent palette to diff against (its palette is re-derived per
 * frame), so emit_pal is always SIXEL_PAL_USED below (the used-colour
 * subset). */
static uint8_t g_rgbx_pal[768];

/* Truecolor sibling of termgfx_termio_present(): the same tier / geometry /
 * pacing flow, but the encoder input is produced from a native w x h XRGB
 * (R,G,B,X) frame instead of an indexed one -- the sixel tier quantizes it, the
 * JXL tier encodes the packed RGB directly.
 *
 * Full-frame every call: the indexed path's dedupe / dirty-rect / retain
 * machinery keys off g_last_fb (an indexed TERMGFX_FB_W x TERMGFX_FB_H surface) and is
 * deliberately NOT shared here -- factoring it out would have to touch
 * present()'s tail, which must stay byte-identical for the live indexed door
 * (syncscumm). The pacing / backpressure gates ARE shared: they gate on the
 * same g_inflight / g_out_len link state, tier-agnostic. A gated frame is
 * dropped rather than retained -- there is no rgbx tick()-retry buffer, and a
 * truecolor source drives its own redraw cadence. */
void termgfx_termio_present_rgbx(const uint8_t *xrgb, int w, int h)
{
	int            tier, ew, eh, icol, irow;
#ifdef WITH_JXL
	int            dx = 0, dy = 0;
#endif
	const uint8_t *rgb;
	size_t         n = 0;
	unsigned       dropped_before;

	if (!g_active || g_hangup)
		return;

	if (g_capture != NULL) {
		/* Capture mode: a self-contained full-res frame per call, no
		 * pacing/dedupe -- mirrors present()'s capture branch, but quantizes
		 * the native XRGB frame (identity scale) before sixel_encode(). */
		if (g_capture_frames >= TERMGFX_CAPTURE_MAX_FRAMES)
			return;
		rgb = termgfx_scale_rgbx_to_rgb(xrgb, w, h, w, h);
		if (rgb == NULL || !ensure_cap(&g_idx_buf, &g_idx_cap, (size_t)w * h))
			return;
		termgfx_quant_rgb(rgb, w, h, g_idx_buf, g_rgbx_pal);
		n = sixel_encode(&g_sixel_buf, &g_sixel_cap, g_idx_buf, w, h, g_rgbx_pal, SIXEL_PAL_USED);
		if (n != 0) {
			fwrite(g_sixel_buf, 1, n, g_capture);
			g_capture_frames++;
		}
		return;
	}

	/* Read pending probe replies before choosing geometry/tier -- same reason
	 * as present() (an intro loop may never poll events). */
	termgfx_termio_pump();
	if (g_hangup)
		return;

	if (g_first_present_ms == 0)
		g_first_present_ms = now_ms();

	/* Startup grace: hold until the capability probe answers (see present()). */
	if (!g_probe_replied && (int32_t)(now_ms() - g_first_present_ms) < TERMGFX_PROBE_GRACE_MS)
		return;

	/* Non-graphics-terminal gate: neither sixel nor JXL advertised (see
	 * present()) -- notify once and quit rather than spray unrenderable DCS. */
	if (!g_have_sixel && !g_jxl && g_probe_replied) {
		if (!g_jxl_done
		    && (int32_t)(now_ms() - g_probe_start_ms) <= TERMGFX_GFX_SETTLE_MS)
			return;   /* JXL cap may still be in flight -- keep holding */
		if (!g_no_gfx_notified) {
			out_puts("\r\n\x1b[0m\r\n"
			         "  This game requires a terminal with sixel or JXL graphics"
			         " support\r\n"
			         "  (such as SyncTERM). This terminal reports neither, so the"
			         " game\r\n"
			         "  cannot be displayed.\r\n\r\n");
			termgfx_termio_flush();
			g_no_gfx_notified = 1;
		}
		g_quit = 1;
		return;
	}

	/* Canvas-size hold: wait for the real pixel canvas (see present()). */
	if (!g_canvas_exact && (int32_t)(now_ms() - g_first_present_ms) < TERMGFX_PROBE_GRACE_MS)
		return;

	termgfx_termio_flush();   /* drain the previous frame first */
	if (g_hangup)
		return;

	/* DSR-ack pacing + backpressure gates (see present()'s two gates). A gated
	 * frame is skipped, not retained: there is no rgbx retry buffer, and the
	 * source re-presents on its own. The plain TERMGFX_PACE_DEADLINE_MS unstick is
	 * used -- the shorter palette-storm deadline is an indexed-fade heuristic
	 * keyed off g_last_pal, which this path does not maintain. */
	if (g_inflight >= g_auto_depth) {
		if ((int32_t)(now_ms() - g_pace_progress_ms) > (int32_t)TERMGFX_PACE_DEADLINE_MS) {
			g_inflight = 0;
			g_dsr_t    = g_dsr_h;   /* drop the stale unacked DSR timestamps */
		} else
			return;
	}
	if (g_out_len > 0)
		return;

	/* Close a DCS/APC a previous drop left dangling (see present()). */
	if (g_need_st) {
		out_puts("\x1b\\");
		g_need_st = 0;
	}
	dropped_before = g_dropped_frames;

	tier = termgfx_tier();

	/* Fit + center against the caller's native w x h (present() fits the fixed
	 * TERMGFX_FB_W x TERMGFX_FB_H), then re-derive the sixel CUP col/row from the
	 * pixel offset exactly as present() does. */
	{
		int rdx, rdy;

		termgfx_image_rect_src(w, h, &ew, &eh, &rdx, &rdy);
		if (tier == TERMGFX_TIER_SIXEL && g_term_rows > 0) {
			double cw  = g_term_cols > 0 ? (double)g_canvas_w / g_term_cols : 8.0;
			double chd = (double)g_canvas_h / g_term_rows;
			icol = 1 + (int)(rdx / cw);
			irow = 1 + (int)(rdy / chd);
		} else {
			icol = 1 + rdx / 8;
			irow = 1 + rdy / 16;
#ifdef WITH_JXL
			dx = rdx;
			dy = rdy;
#endif
		}

		/* Stash for termgfx_trace() -- same geometry fields present() records. */
		g_trace_ew    = ew;
		g_trace_eh    = eh;
		g_trace_icol  = icol;
		g_trace_irow  = irow;
#ifdef WITH_JXL
		g_trace_dx    = dx;
		g_trace_dy    = dy;
#endif
	}

	/* Stats bar BEFORE the (possibly huge) image, so it wins stage space even
	 * when the frame runs past out_put()'s cap -- same rationale as present(). */
	termgfx_fps_tick(1);
	termgfx_stats_draw();

	if (tier == TERMGFX_TIER_SIXEL) {
		char cup[24];
		int  cn;

		/* Match termgfx_emit_sixel()'s band/scale clamps, but on the RGB scale:
		 * clamp width, round height down to whole 6px sixel bands. */
		if (ew > TERMGFX_SCALE_MAX)
			ew = TERMGFX_SCALE_MAX;
		eh -= eh % 6;
		if (eh < 6)
			eh = 6;
		rgb = termgfx_scale_rgbx_to_rgb(xrgb, w, h, ew, eh);
		if (rgb != NULL && ensure_cap(&g_idx_buf, &g_idx_cap, (size_t)ew * eh)) {
			termgfx_quant_rgb(rgb, ew, eh, g_idx_buf, g_rgbx_pal);
			cn = snprintf(cup, sizeof cup, "\x1b[%d;%dH", irow, icol);
			out_put(cup, (size_t)cn);
			n = sixel_encode(&g_sixel_buf, &g_sixel_cap, g_idx_buf, ew, eh, g_rgbx_pal, SIXEL_PAL_USED);
			out_put(g_sixel_buf, n);
		}
	}
#ifdef WITH_JXL
	else {   /* TERMGFX_TIER_JXL: encode the packed RGB directly, no indexing */
		rgb = termgfx_scale_rgbx_to_rgb(xrgb, w, h, ew, eh);
		if (rgb != NULL)
			n = termgfx_emit_jxl_rgb(rgb, ew, eh, dx, dy);
		if (n != 0) {
			out_put(g_apc_buf, n);
		} else {
			/* encode failure (or transient libjxl fail): fall back to sixel for
			 * just this frame, same as present(). */
			char cup[24];
			int  cn;

			if (ew > TERMGFX_SCALE_MAX)
				ew = TERMGFX_SCALE_MAX;
			eh -= eh % 6;
			if (eh < 6)
				eh = 6;
			rgb = termgfx_scale_rgbx_to_rgb(xrgb, w, h, ew, eh);
			if (rgb != NULL && ensure_cap(&g_idx_buf, &g_idx_cap, (size_t)ew * eh)) {
				termgfx_quant_rgb(rgb, ew, eh, g_idx_buf, g_rgbx_pal);
				cn = snprintf(cup, sizeof cup, "\x1b[%d;%dH", irow, icol);
				out_put(cup, (size_t)cn);
				n = sixel_encode(&g_sixel_buf, &g_sixel_cap, g_idx_buf, ew, eh, g_rgbx_pal, SIXEL_PAL_USED);
				out_put(g_sixel_buf, n);
			}
			tier = TERMGFX_TIER_SIXEL;
		}
	}
#endif

	out_put("\x1b[6n", 4);   /* DSR: paces the next frame */
	g_inflight++;
	g_pace_progress_ms = now_ms();
	termgfx_dsr_sent(g_pace_progress_ms);

	/* A drop mid-frame can leave a DCS/APC open with no ST -- flag the next
	 * present_rgbx() to close it (g_need_st), same resync marker present()
	 * uses. No g_have_last handling: this path keeps no last-frame state. */
	if (g_dropped_frames != dropped_before) {
		g_need_st = 1;
		termgfx_trace("dropped", termgfx_tier_name(tier), n);
	} else
		termgfx_trace("full", termgfx_tier_name(tier), n);

	termgfx_termio_flush();
}

/* Retry a frame a DEFER gate stranded (see g_present_pending's doc comment
 * above termgfx_termio_present()). Called every poll (door/syncscumm.cpp's
 * pollEvent()), including on a fully static screen where nothing else would
 * ever call present() again -- that is the whole point: a burst's last
 * frame that got gated on entry no longer needs a further engine-side
 * redraw (e.g. a mouse move) to eventually reach the wire.
 *
 * Just calls termgfx_termio_present() again with the retained bytes, so the retry
 * flows through the EXACT SAME gates as any other present() call -- it can
 * re-defer (and re-retain, via the idx == g_pending_idx guard in those gates)
 * exactly as many times as the pacing/backpressure state requires, and can
 * never send ahead of what the link has actually drained. Once the gate
 * clears, present()'s own top-of-function termgfx_termio_pump() (fresh g_inflight/
 * acks) and termgfx_termio_flush() (fresh g_out_len) mean the retry is evaluated
 * against CURRENT state, not whatever was true when the frame was first
 * gated.
 *
 * RECURSION GUARD: termgfx_termio_present() calls termgfx_termio_pump(), never
 * termgfx_termio_tick() -- so present() -> pump() -> tick() -> present() cannot
 * happen. This function itself is only ever called from pollEvent(), never
 * from present() or pump(), so there is exactly one entry point into a
 * retry and no path back into this function from within it. */
void termgfx_termio_tick(void)
{
	if (g_present_pending)
		termgfx_termio_present(g_pending_idx, g_pending_pal);
}

#ifdef TERMGFX_TEST
/* Test-only seams (test/test_termgfx_termio_mouse.c): drive termgfx_termio_mouse_report()'s
 * coordinate mapping without a real session -- no termgfx_termio_init(), no socket,
 * no probe burst. Never compiled into the shipped door (TERMGFX_TEST is not
 * defined by build.sh). Sets the geometry globals termgfx_image_rect() and
 * termgfx_termio_mouse_report() read, AND resets the event FIFO: each call starts a
 * fresh scenario, as if a probe had just landed with this geometry, rather
 * than accumulating events (or a stale g_canvas_exact/pixels latch) across
 * scenarios in the same test binary. */
int termgfx_termio_test_set_geom(int canvas_w, int canvas_h, int cell_w, int cell_h,
                         int cols, int rows, int pixels)
{
	g_canvas_w     = canvas_w;
	g_canvas_h     = canvas_h;
	g_canvas_exact = 1;   /* treat the injected canvas as a landed probe reply */
	g_cell_w       = cell_w;
	g_cell_h       = cell_h;
	g_term_cols    = cols;
	g_term_rows    = rows;
	g_mouse.pixels = pixels;
	g_evq_head = 0;
	g_evq_n    = 0;
	return 1;
}

int termgfx_termio_test_mouse_report(int b, int col, int row, int release)
{
	termgfx_termio_mouse_report(b, col, row, release);
	return 1;
}

/* Test-only seam (test/test_termgfx_termio_input.c): run raw wire bytes through the
 * exact same byte parser termgfx_termio_pump() feeds (parse_bytes()), so a keyboard
 * decode test can drive P_NORMAL/P_ESC/P_CSI/csi_final() without a socket
 * or a probe burst. Does NOT reset the event FIFO -- unlike
 * termgfx_termio_test_set_geom() above, a keyboard test scenario is a plain
 * sequence of bytes with no per-scenario geometry latch to re-seed, so
 * callers drain with termgfx_termio_next_event() between scenarios themselves. */
int termgfx_termio_test_feed(const char *bytes, size_t n)
{
	parse_bytes((const uint8_t *)bytes, (int)n);
	return 1;
}

/* Test-only seam (test/test_termgfx_termio_present_pending.c): force the DSR-ack
 * pacing gate's g_inflight without a real multi-frame DSR exchange, so the
 * strand-on-static-screen regression test can drive present() straight into
 * "gated-inflight" on demand. */
int termgfx_termio_test_set_inflight(int n)
{
	g_inflight = n;
	return 1;
}

/* Test-only seam (test/test_termgfx_termio_present_pending.c): expose g_present_pending
 * so the strand-on-static-screen regression test can assert a gated present()
 * retained its frame instead of silently dropping it, and that a successful
 * send (direct or via termgfx_termio_tick()) clears it again. */
int termgfx_termio_test_present_pending(void)
{
	return g_present_pending;
}
#endif
