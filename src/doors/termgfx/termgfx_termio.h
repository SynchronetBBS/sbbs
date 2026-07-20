#ifndef TERMGFX_TERMIO_H_
#define TERMGFX_TERMIO_H_

#include <stddef.h>
#include <stdint.h>

#include "keymode.h"   // TERMGFX_MOD_* (input-event modifier bits)

// termgfx_termio.h -- the shared terminal-session engine for termgfx game
// doors: lifecycle + socket transport, the tiered present loop (sixel / JXL /
// text), input decoding (SGR mouse, kitty CSI-u, SyncTERM evdev), the audio
// output seam, and the stats/introspection probes.
//
// This is the orchestration layer that DRIVES the pieces already shared in
// libtermgfx (caps, geometry, sixel, jxl, pace, mouse/sgrmouse, keymode,
// door32, audio, sbbs_node) -- it does not re-expose them. A door owns the
// per-game part (its frame source, its key->action map) and hands frames and
// PCM in through here; the session, tier dispatch, present loop and event
// queue live behind this API.
//
// Single instance per process: the implementation keeps file-static state, so
// there is no context handle -- one door process drives one session. This
// mirrors syncscumm's sst_io_* surface verbatim (renamed), the source the body
// is extracted from; only syncscumm and the future syncrpg adopt it now. Where
// other doors need more (variable frame geometry, direct-RGB present, edge
// scroll), that stays a door-side hook until a door actually adopts this API --
// see the survey note in the extraction plan.

#ifdef __cplusplus
extern "C" {
#endif

// Fixed framebuffer the door presents through -- 320x200 indexed, syncscumm's
// current native surface (see termgfx_termio_present below). A door with a
// different or runtime-variable resolution is a later-task concern; this is the
// current byte-for-byte contract.
#define TERMGFX_TERMIO_FB_W 320
#define TERMGFX_TERMIO_FB_H 200

// ---- lifecycle / transport --------------------------------------------------

// Parse the door's argv (DOOR32.SYS path, -s<fd> socket, capture-mode flags),
// open the session, probe terminal capabilities and negotiate key/mouse modes.
// Returns nonzero on a live (or capture) session, 0 if there is nothing to
// drive. argv entries it consumes are reported by termgfx_termio_consumed().
int  termgfx_termio_init(int argc, char **argv);

// Tear the session down: restore the terminal's key/mouse modes, stop audio
// (calls termgfx_termio_audio_stop internally), flush and close the socket.
void termgfx_termio_shutdown(void);

// Is there a live terminal session? 0 for a headless / capture run. present()
// and the wire-facing calls are no-ops when this is false.
int  termgfx_termio_active(void);

// Service the session once: read and decode pending input into the event
// queue, run pacing, keep the transport moving. Call every poll.
void termgfx_termio_pump(void);

// Force everything staged for the wire out to the socket (blocking as needed).
void termgfx_termio_flush(void);

// Did the peer ask to quit (BBS-side disconnect key / door menu quit)? One-shot
// semantics as in the source engine.
int  termgfx_termio_quit_requested(void);

// Peer EOF, or a hard read/write error -- the session is gone. Once true the
// wire-facing calls become no-ops.
int  termgfx_termio_hung_up(void);

// Was Ctrl+<menu_letter> pressed (one-shot)? Drives the door's in-game menu.
int  termgfx_termio_menu_requested(void);

// Set the GMM hotkey to Ctrl+<letter>; 0 disables it.
void termgfx_termio_set_menu_key(int letter);

// Was argv[idx] one that termgfx_termio_init() resolved itself (-s<fd>, a
// DOOR32.SYS path, a capture flag)? A door's main() uses this to strip the
// door-only argv entries before handing the rest to the game engine, which
// rejects options it does not know.
int  termgfx_termio_consumed(int idx);

// ---- capability probes (unit tests + stats bar) -----------------------------

int  termgfx_termio_have_sixel(void);
int  termgfx_termio_is_syncterm(void);
int  termgfx_termio_jxl_supported(void);
int  termgfx_termio_stats_visible(void);

// Will this session's player actually HEAR audio? A door drives its
// subtitles-auto decision off this. It is a property of the SESSION, not just
// the terminal: a sysop who disabled sound ("[audio] enabled = false") reads 0
// however capable the terminal is, and answers instantly. Otherwise it answers
// from the startup audio-capability probe -- 1 only for a terminal that
// confirmed the digital (libsndfile) tier AND did not deny Ogg-Opus. A
// tone-only terminal reads 0. This one call may block briefly at boot (a few
// hundred ms) waiting for the probe reply to latch; headless/capture sessions
// return 0 immediately.
int  termgfx_termio_audio_available(void);

// ---- frame source -----------------------------------------------------------

// Present one changed frame. idx is TERMGFX_TERMIO_FB_W*TERMGFX_TERMIO_FB_H
// palette indices; pal768 is 256 RGB triples (8-bit, no <<2 scaling). This is
// the door's current native frame buffer; the exact pixel-format contract
// (indexed vs packed RGB/RGBA, stride) is pinned in the extraction's later
// task to match the body's internal surface -- do not assume a different one.
// A no-op off a terminal session (termgfx_termio_active() false) or a dead one
// (termgfx_termio_hung_up()). In capture mode it appends a self-contained
// full-res frame per call (no pacing/dedupe), capped.
void termgfx_termio_present(const uint8_t *idx, const uint8_t *pal768);

/* Truecolor present: hand a native-resolution 32-bit frame. `xrgb` is w*h
 * pixels, memory byte order R,G,B,X (the 4th byte is ignored padding), stride
 * w*4. The sixel tier quantizes internally; the JXL tier encodes the RGB
 * directly (no indexed round-trip). Parallel to termgfx_termio_present(), which
 * stays the entry point for native-indexed sources. */
void termgfx_termio_present_rgbx(const uint8_t *xrgb, int w, int h);

// Retry a frame that a pacing/backpressure gate inside present() deferred
// without sending -- present() is edge-driven off the engine's dirty flag, so a
// frame gated on the last redraw of a burst (a static panel, no further
// updates) would otherwise sit unsent. A no-op when nothing is pending. Call
// once per poll -- NEVER from within present() or pump(), which would recurse.
void termgfx_termio_tick(void);

// ---- input events (mouse + keyboard) ----------------------------------------
//
// pump()'s parser turns an SGR mouse report or a decoded key -- a legacy byte,
// a kitty CSI-u event or a SyncTERM evdev physical-key report -- into one or
// more of these and queues them; the door's poll loop drains them via
// termgfx_termio_next_event().

typedef enum {
	TERMGFX_EV_MOUSE_MOVE, TERMGFX_EV_MOUSE_DOWN, TERMGFX_EV_MOUSE_UP,
	TERMGFX_EV_WHEEL, TERMGFX_EV_KEY_DOWN, TERMGFX_EV_KEY_UP
} termgfx_ev_type_t;

// Non-ASCII keys carried in termgfx_input_event_t.keycode. Printable keys use
// their ASCII value directly (keycode == ascii); these cover the rest.
enum {
	TERMGFX_KEY_FIRST = 0x100,
	TERMGFX_KEY_UP, TERMGFX_KEY_DOWN, TERMGFX_KEY_LEFT, TERMGFX_KEY_RIGHT,
	TERMGFX_KEY_HOME, TERMGFX_KEY_END, TERMGFX_KEY_PAGEUP, TERMGFX_KEY_PAGEDOWN,
	TERMGFX_KEY_INSERT, TERMGFX_KEY_DELETE,
	TERMGFX_KEY_ENTER, TERMGFX_KEY_ESCAPE, TERMGFX_KEY_BACKSPACE, TERMGFX_KEY_TAB,
	TERMGFX_KEY_F1, TERMGFX_KEY_F2, TERMGFX_KEY_F3, TERMGFX_KEY_F4, TERMGFX_KEY_F5,
	TERMGFX_KEY_F6, TERMGFX_KEY_F7, TERMGFX_KEY_F8, TERMGFX_KEY_F9,
	TERMGFX_KEY_KP5   // numpad center (NumLock off): stationary/stop -- NOT an
	                  // ASCII key, so it must not fall through to '5'
};

// Key-event modifier bitmask lives in termgfx_input_event_t.mods, using the
// TERMGFX_MOD_SHIFT / _CTRL / _ALT bits from keymode.h (included above). The
// producer and consumer both use those symbols, so the bit values are opaque.

typedef struct {
	termgfx_ev_type_t type;
	int x, y;        // mouse: game coords, 0..TERMGFX_TERMIO_FB_W/H-1
	int button;      // MOUSE_DOWN/UP: 0 left, 1 middle, 2 right
	int wheel;       // WHEEL: -1 up, +1 down
	int keycode;     // KEY_*: a TERMGFX_KEY_* code or a raw ASCII byte
	int ascii;       // KEY_*: printable char, else 0
	int mods;        // KEY_*: TERMGFX_MOD_* bitmask
} termgfx_input_event_t;

// Pop the next queued input event. Returns 1 and fills *ev, or 0 if empty.
int termgfx_termio_next_event(termgfx_input_event_t *ev);

// ---- audio output seam ------------------------------------------------------

// Feed the session one block of the mixer's interleaved STEREO PCM (`frames`
// counts per-channel frames, at the door's configured mixer rate). Everything
// downstream -- chunking, the Opus encode, the prebuffer cushion, the
// congestion policy, the stereo-to-mono downmix -- happens in termgfx's shared
// streaming audio module; this only hands it the door's output seam. A no-op on
// any session that cannot play digital audio (tone-only, headless, dead peer):
// not one byte goes out.
void termgfx_termio_audio_stream(const int16_t *pcm, size_t frames);

// Silence the channel and drop what is queued behind it. shutdown() calls this,
// so a door that just exits needs no more.
void termgfx_termio_audio_stop(void);

// ---- stats / test introspection ---------------------------------------------

// Frames whose staged bytes were dropped because the output stage buffer stayed
// full after a flush attempt (dead-peer backpressure, not a hard error).
unsigned termgfx_termio_frames_dropped(void);

// Audio APCs the door itself refused because its output stage was full and a
// flush bought no room. Distinct from the streaming module's own drop counter
// (which counts chunks it declined to encode).
unsigned termgfx_termio_audio_dropped(void);

// Bytes staged for the wire but not yet written -- ALL of them, audio and video
// alike. NOT the number the streaming audio module wants (see below).
size_t   termgfx_termio_out_backlog(void);

// Audio's OWN share of the bytes staged but not yet written. The streaming
// audio module reads this to tell a link that genuinely cannot carry AUDIO from
// one merely busy shipping a big video frame -- video and audio share one
// strictly-ordered output FIFO, so the stage's total depth answers the wrong
// question.
size_t   termgfx_termio_audio_backlog(void);

#ifdef __cplusplus
}
#endif

#endif // TERMGFX_TERMIO_H_
