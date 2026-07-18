/* sst_io.h -- SyncSCUMM's terminal-session layer (pure C, no ScummVM).
 * Modeled on syncconquer/door/door_io.c; termgfx supplies the encoders,
 * probe parsers, pacing primitives and control strings -- the session,
 * tier dispatch and present loop live here. */
#ifndef SYNCSCUMM_SST_IO_H_
#define SYNCSCUMM_SST_IO_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SST_FB_W 320
#define SST_FB_H 200

/* The rate the door hands the mixer, and so the rate the stream is cut at.
 * Retro-game audio has nothing above ~12kHz to lose (SCUMM's own samples are
 * 8-22kHz), so a rate this low halves every PCM cost -- accumulate, encode,
 * wire -- versus 48000 for no audible difference.
 *
 * 24000 rather than the more obvious 22050 because 24000 is one of the five
 * rates Opus itself speaks (8/12/16/24/48kHz) and 22050 is not. termgfx's
 * encoder will happily take an illegal rate, but it pays for it: it picks the
 * nearest legal rate above and linear-interpolation-resamples to it
 * (termgfx/audio.c) -- and it does that INDEPENDENTLY PER CHUNK, so the
 * resampler's phase restarts every 100ms and each chunk boundary gains a
 * sub-sample step of its own. Asking the mixer for 24000 deletes the
 * resample entirely: there is nothing left to resample, the encoder's
 * fast path takes the PCM as-is, and the door pays no interpolation loss.
 * ScummVM's mixer rate is ours to pick and its rate converter feeds it any
 * game's native rate regardless, so this costs nothing on the mix side.
 *
 * ONE definition site: the C++ mixer (audio_term.cpp, via audio_term.h)
 * creates its mixer at this rate and sst_io.c encodes at this rate below --
 * they are the same symbol, not two literals kept in sync by hand. */
#define SST_AUDIO_RATE 24000

int  sst_io_init(int argc, char **argv);
void sst_io_shutdown(void);
int  sst_io_active(void);
void sst_io_pump(void);
void sst_io_flush(void);
int  sst_io_quit_requested(void);
int  sst_io_hung_up(void);   /* peer EOF, or a hard read/write error -- see sst_io.c */

/* Was argv[idx] one sst_io_init() resolved itself (-s<fd>, a DOOR32.SYS
 * path)? main() uses this to strip door-only argv entries before handing
 * the rest to scummvm_main(), which rejects options it doesn't know. */
int  sst_io_consumed(int idx);

/* parser-state probes (unit test + stats bar) */
int  sst_io_have_sixel(void);
int  sst_io_is_syncterm(void);
int  sst_io_jxl_supported(void);
int  sst_io_stats_visible(void);

/* Will this session's player actually HEAR audio? Drives the subtitles
 * auto-decision (door/syncscumm.cpp): no audio -> subtitles on. Note the
 * question is about the SESSION, not the terminal: a sysop who turned sound
 * off ("[audio] enabled = false") gets 0 here however capable the terminal
 * is, and answers instantly without waiting on the probe -- reading this as a
 * terminal property once left a talkie's dialogue neither heard nor seen (see
 * sst_io.c). Otherwise it answers from the startup audio-capability probe:
 * 1 only for a terminal that confirmed the digital
 * (libsndfile) tier AND did not deny the Ogg-Opus codec. A tone-only
 * terminal reads as 0 -- A;Load is a no-op without libsndfile, so a queued
 * slot would play empty. Unlike the other bounded waits here this one really
 * blocks (it is asked once, during initBackend(), and nobody re-asks): it
 * pumps until the reply latches or the probe window expires, so a terminal
 * that never answers costs a few hundred ms once, at boot. Headless/capture
 * sessions never probe and return 0 immediately. See sst_io.c. */
int  sst_io_audio_available(void);

/* How much audio each shipped chunk holds -- the door's default for
 * "[audio] chunk_ms", which a sysop's syncscumm.ini still overrides, and which
 * the module clamps to 50..250 (termgfx/audio_stream.h).
 *
 * 250, the top of that clamp, and MEASURED: each chunk is its own complete
 * Ogg/Opus stream, so a chunk boundary is a stream boundary and every one of
 * them is a chance to pop. Longer chunks buy two things at once -- 4 boundaries
 * a second instead of 10, and one set of Ogg headers per 250ms instead of per
 * 100ms. On a 459.9s BASS capture that is 158.0 -> 110.2 kbps AND boundaries
 * anomalous against the interior 99th pct 2.6% -> 0.9%, which is chance: at
 * this length the boundaries stop standing out from the signal at all.
 *
 * ONE definition site, for the same reason SST_AUDIO_RATE above is: the tests
 * size a chunk's frames off this symbol rather than respelling "a tenth of a
 * second", which is exactly the hardcoded literal that broke them when the rate
 * moved. See audio_stream_open() in sst_io.c. */
#define SST_CHUNK_MS 250

/* Chunks the streaming module holds before playback starts -- the door's
 * default for "[audio] prebuffer", which a sysop's syncscumm.ini still
 * overrides. This is both the latency budget and the entire jitter tolerance
 * (termgfx/audio_stream.h). It counts CHUNKS, not milliseconds, so it must be
 * read together with SST_CHUNK_MS above: 3 x 250ms is a ~750ms cushion, which
 * is the SAME cushion the old 8 x 100ms bought. The number moved to keep the
 * cushion still -- see audio_stream_open() in sst_io.c for the measurement it
 * is sized against. Spelled here rather than only at its use so the tests can
 * key off the cushion itself instead of re-guessing the number. */
#define SST_PREBUFFER_CHUNKS 3

/* Percent of full scale the mixer's PCM is scaled to BEFORE it is Opus-encoded
 * -- the door's default for "[audio] headroom", which a sysop's syncscumm.ini
 * still overrides. 100 = feed the mixer's PCM through untouched.
 *
 * 70, i.e. ~-3dB, and it is the ONLY headroom in the path that can do this job.
 * THE DEFECT IT FIXES: Beneath a Steel Sky's battle scene distorted -- a
 * crunch the raw mixer PCM does not have -- while the rest of the intro was
 * clean. The mix is hot there: 0.55% of the window's samples are pinned to the
 * rail (417 flat-topped runs, up to 26 samples long, in 14 seconds), because
 * ScummVM's mixer sums voice + effects + AdLib music and clamps the sum. A
 * flat top is a discontinuity, a discontinuity is broadband, and a lossy codec
 * reconstructs it with ringing that OVERSHOOTS the rail it was clipped to:
 * measured, the decoded float peaks at 1.1430 of full scale where the source
 * peaks at exactly 1.0. Hot-but-unclipped passages of the same capture do not
 * do this (peak 0.96 in, 0.9676 out), so the flat tops are the trigger, not
 * loudness as such.
 *
 * The overshoot is then fatal rather than merely loud, because libsndfile's
 * S16 read of an Opus stream WRAPS instead of clamping: +1.00044 comes back as
 * -32754. Every overshooting sample becomes a full-scale SIGN FLIP -- 2300 of
 * them in that 14s window, ~164 a second. That is the crunch. SyncTERM decodes
 * with the same sf_readf_short() and does not set SFC_SET_CLIPPING either, so
 * the wire behaves exactly like the local round-trip does.
 *
 * WHY HERE AND NOT THE TERMINAL'S CHANNEL VOLUME: "[audio] volume" is
 * SyncTERM's mixer-channel volume, applied AFTER the decode. By then the sign
 * flips are already baked into the samples, and scaling a wrapped sample just
 * makes a quieter wrapped sample. Only attenuation applied BEFORE the encoder
 * gives the codec's ringing somewhere to land. That is why this knob exists as
 * well as that one, and why the ~-3dB that used to live in the channel volume
 * now lives here (audio_stream_open() in sst_io.c has the end-to-end sum).
 *
 * 70 rather than the 85 the measurement strictly needs (overshoot reaches zero
 * at 0.85 for this capture): 70 leaves the decoded peak at 0.8029, ~1.9dB
 * clear of the rail, and it is the value that makes the loudness math exact --
 * 70% here x 100% channel volume is the same net gain as the 100% x 70% that
 * shipped before, so nothing got quieter in exchange for the fix. A title that
 * mixes hotter than BASS (or a sysop who raises the game's own volumes) can
 * lower it; one that never clips can raise it to 100 and pay nothing. */
#define SST_AUDIO_HEADROOM 70

/* Feed the session one block of the mixer's interleaved STEREO PCM (`frames`
 * counts per-channel frames, at SST_AUDIO_RATE). Everything the terminal
 * needs -- chunking, the Opus encode, the prebuffer cushion, the congestion
 * policy, and the stereo-to-mono downmix ("[audio] channels", see
 * audio_stream_open() in sst_io.c) -- happens downstream in termgfx's
 * streaming module; this only hands it the door's output seam. The PCM here
 * is stereo whatever the wire carries: the mixer stays stereo on purpose.
 * A no-op on any session that cannot play digital
 * audio (tone-only terminal, headless, dead peer): NOT one byte goes out.
 * sst_io_audio_stop() silences the channel and drops what is queued behind
 * it; sst_io_shutdown() calls it, so a door that just exits needs no more. */
void sst_io_audio_stream(const int16_t *pcm, size_t frames);
void sst_io_audio_stop(void);

/* Present one changed frame: idx is SST_FB_W*SST_FB_H palette indices,
 * pal768 is 256 RGB triples (8-bit, no <<2 scaling). A no-op off a terminal
 * session (sst_io_active() false) or a dead one (sst_io_hung_up()). In
 * SYNCSCUMM_SIXELOUT capture mode, appends a self-contained full-res sixel
 * frame per call (no pacing/dedupe), capped at 200 frames. */
void sst_io_present(const uint8_t *idx, const uint8_t *pal768);

/* ---- input events (mouse + keyboard) ----
 * sst_io_pump()'s parser turns an SGR mouse report (csi_final()'s 'M'/'m'
 * cases) or a decoded key -- a legacy byte, a kitty CSI-u event or a
 * SyncTERM evdev physical-key report -- into one or more of these and
 * queues them here; the ScummVM backend's pollEvent() drains them via
 * sst_io_next_event(). */
typedef enum {
	SST_EV_MOUSE_MOVE, SST_EV_MOUSE_DOWN, SST_EV_MOUSE_UP,
	SST_EV_WHEEL, SST_EV_KEY_DOWN, SST_EV_KEY_UP
} sst_ev_type_t;

/* Modifier bitmask (key events). */
#define SST_MOD_SHIFT 1
#define SST_MOD_ALT   2
#define SST_MOD_CTRL  4

/* Non-ASCII keys carried in sst_input_event_t.keycode. Printable keys use
 * their ASCII value directly (keycode == ascii); these cover the rest. */
enum {
	SST_KEY_FIRST = 0x100,
	SST_KEY_UP, SST_KEY_DOWN, SST_KEY_LEFT, SST_KEY_RIGHT,
	SST_KEY_HOME, SST_KEY_END, SST_KEY_PAGEUP, SST_KEY_PAGEDOWN,
	SST_KEY_INSERT, SST_KEY_DELETE,
	SST_KEY_ENTER, SST_KEY_ESCAPE, SST_KEY_BACKSPACE, SST_KEY_TAB,
	SST_KEY_F1, SST_KEY_F2, SST_KEY_F3, SST_KEY_F4, SST_KEY_F5,
	SST_KEY_F6, SST_KEY_F7, SST_KEY_F8, SST_KEY_F9
};

typedef struct {
	sst_ev_type_t type;
	int x, y;        /* mouse: game coords, 0..SST_FB_W/H-1 */
	int button;      /* MOUSE_DOWN/UP: 0 left 1 middle 2 right */
	int wheel;       /* WHEEL: -1 up, +1 down */
	int keycode;     /* KEY_*: an SST_KEY_* code or a raw ASCII byte */
	int ascii;       /* KEY_*: printable char, else 0 */
	int mods;        /* KEY_*: SST_MOD_* bitmask */
} sst_input_event_t;

/* Pop the next queued input event. Returns 1 and fills *ev, or 0 if empty.
 * Drained by the ScummVM backend's pollEvent(). */
int sst_io_next_event(sst_input_event_t *ev);

/* Frames whose staged bytes were dropped because out_put()'s 256KB stage
 * buffer stayed full after a flush attempt (dead-peer backpressure, not a
 * hard error) -- see sst_io.c's out_put(). Test-introspection / stats. */
unsigned sst_io_frames_dropped(void);

/* Audio APCs the door itself refused because its output stage was full and a
 * flush bought no room -- see sst_io.c's sst_stream_put(). Distinct from the
 * streaming module's own drop counter (termgfx/audio_stream.h), which counts
 * chunks it declined to encode. Both appear on every SYNCSCUMM_TRACE present
 * line: if audio is ever thrown away, the trace says who threw it.
 * Test-introspection / stats. */
unsigned sst_io_audio_dropped(void);

/* Bytes staged for the wire but not yet written to the socket -- ALL of them,
 * audio and video alike. Stats / test-introspection only: this is NOT the
 * number the streaming audio module wants (see sst_io_audio_backlog() below,
 * and the staging block in sst_io.c for the defect that distinction cost). */
size_t sst_io_out_backlog(void);

/* Audio's OWN share of the bytes staged but not yet written. The streaming
 * audio module reads this to tell a link that genuinely cannot carry AUDIO
 * from one that is merely busy shipping a big video frame -- video and audio
 * share one strictly-ordered output FIFO, so the stage's total depth answers
 * the wrong question entirely. See sst_io.c. */
size_t sst_io_audio_backlog(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNCSCUMM_SST_IO_H_ */
