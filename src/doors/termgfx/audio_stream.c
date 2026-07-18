// audio_stream.c -- see audio_stream.h.
//
// The chunk accumulator declared alongside this state machine in
// audio_stream.h lives in chunk.c, not here -- see that file's banner for why
// it is a separate translation unit.
#include "audio_stream.h"
#include "audio.h"       // the audio-APC builders
#include "audio_mgr.h"   // TERMGFX_MUSIC_QUALITY_DEFAULT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- the streaming state machine -----------------------------------------
//
// Chunks are produced in real time (the source runs at wall clock) and consumed
// in real time, so the channel FIFO's depth is whatever cushion we establish at
// the start and never grows on its own. That is why playback waits for
// `prebuffer` chunks: a FIFO fed one chunk at a time from empty stays one deep
// and underruns on the first jitter. Those chunks ARE the latency budget.

enum { TERMGFX_STREAM_OFF = 0, TERMGFX_STREAM_WAIT_CAPS,
	   TERMGFX_STREAM_PRIME, TERMGFX_STREAM_RUN };

// A source that reports a nonsense rate should be audible-but-wrong, never
// silent, and never a division by zero.
#define TERMGFX_STREAM_RATE_FALLBACK 44100

// Sustained-backlog drop. NOT an instantaneous socket check: SyncDOOM tried
// that for SFX and reverted it, because the frame path keeps the socket busy
// essentially always (syncdoom/i_termsound.c). Two consecutive chunk boundaries
// over the threshold is ~200 ms of real congestion, not a momentary burst.
#define TERMGFX_STREAM_BACKLOG_BYTES (48 * 1024)
#define TERMGFX_STREAM_BACKLOG_STRIKES 2

struct termgfx_stream {
	termgfx_stream_cfg_t cfg;
	termgfx_stream_io_t io;
	char name[32];            // cfg.name / cfg.cache_prefix are copied in here,
	char prefix[16];          // so the door's strings need not outlive us

	int state;
	int paused;
	termgfx_chunk_t chunk;

	uint8_t *apc;             // reusable builder scratch
	size_t apc_cap;

	// Encoded chunks held during PRIME, emitted together once we have
	// `prebuffer` of them.
	uint8_t *held[8];
	size_t held_len[8];
	int held_n;

	unsigned ring;            // next cache name index
	int have_silence;         // <prefix>/z uploaded
	int blob_ok;              // client supports A;LoadBlob (CTerm >= 1.329)

	// Telemetry, printed at destroy.
	unsigned underruns;
	unsigned dropped;
	unsigned chunks;

	int strikes;
};

static void
termgfx_stream_apc(termgfx_stream_t *s, size_t n)
{
	if (n > 0)
		s->io.put(s->io.ctx, s->apc, n);
}

static void
termgfx_stream_name(termgfx_stream_t *s, char *out, size_t outlen, unsigned idx)
{
	snprintf(out, outlen, "%s/%u", s->cfg.cache_prefix, idx % TERMGFX_STREAM_RING);
}

// Load the patch slot from a cached name and queue it; re-arm the idle
// notification.
static void
termgfx_stream_play(termgfx_stream_t *s, const char *name)
{
	termgfx_stream_apc(s, termgfx_audio_load(&s->apc, &s->apc_cap, s->cfg.slot, name));
	termgfx_stream_apc(s, termgfx_audio_queue(&s->apc, &s->apc_cap, s->cfg.ch,
	                                          s->cfg.slot, termgfx_db_from_pct(100), 0, 0));
	termgfx_stream_apc(s, termgfx_audio_update(&s->apc, &s->apc_cap, s->cfg.ch));
	s->chunks++;
}

// Enable the inline A;LoadBlob path (CTerm >= 1.329). Set from the input layer
// once the DA1 reply's version is known; until then we stay on the cache ring.
void
termgfx_stream_set_blob_ok(termgfx_stream_t *s, int ok)
{
	if (s == NULL)
		return;
	s->blob_ok = ok ? 1 : 0;
}

// 1 once the inline-blob path is active (streaming chunks skip the cache ring).
// For a door's stats overlay.
int
termgfx_stream_blob_active(const termgfx_stream_t *s)
{
	return s == NULL ? 0 : s->blob_ok;
}

// Ship one encoded chunk. On a CTerm >= 1.329 client, inline it via A;LoadBlob
// (no cache file, no ring churn); otherwise upload under the next ring name and
// play it by name. Unique game-audio chunks never repeat, so the blob path is a
// pure win where available. (The named "<prefix>/z" silence chunk stays cached
// -- it IS replayed, so upload-once-by-name still beats re-sending it.)
static void
termgfx_stream_send(termgfx_stream_t *s, const uint8_t *ogg, size_t len)
{
	char name[32];

	if (s->blob_ok) {
		termgfx_stream_apc(s, termgfx_audio_load_blob_file(&s->apc, &s->apc_cap,
		                                                   s->cfg.slot, ogg, len));
		termgfx_stream_apc(s, termgfx_audio_queue(&s->apc, &s->apc_cap, s->cfg.ch,
		                                          s->cfg.slot, termgfx_db_from_pct(100), 0, 0));
		termgfx_stream_apc(s, termgfx_audio_update(&s->apc, &s->apc_cap, s->cfg.ch));
		s->chunks++;
		return;
	}
	termgfx_stream_name(s, name, sizeof name, s->ring++);
	termgfx_stream_apc(s, termgfx_audio_cache_file(&s->apc, &s->apc_cap, name, ogg, len));
	termgfx_stream_play(s, name);
}

static void
termgfx_stream_drop_held(termgfx_stream_t *s)
{
	int i;

	for (i = 0; i < s->held_n; i++)
		free(s->held[i]);
	s->held_n = 0;
}

// Encode one closed chunk. Returns malloc'd Ogg bytes in *out, or 0.
static size_t
termgfx_stream_encode(termgfx_stream_t *s, uint8_t **out)
{
	return termgfx_audio_encode_ogg(s->chunk.buf, s->chunk.len, s->cfg.channels,
	                                s->cfg.rate, s->cfg.quality, out);
}

// Upload the silent chunk once; thereafter a silent chunk costs a Load+Queue
// and no payload at all.
static void
termgfx_stream_upload_silence(termgfx_stream_t *s)
{
	uint8_t *ogg = NULL;
	char     name[32];
	size_t   n;
	size_t   i;

	for (i = 0; i < s->chunk.cap * (size_t)s->chunk.channels; i++)
		s->chunk.buf[i] = 0;
	s->chunk.len = s->chunk.cap;
	n            = termgfx_stream_encode(s, &ogg);
	s->chunk.len = 0;
	if (n == 0 || ogg == NULL)
		return;                    // leave have_silence 0: we just pay bytes
	snprintf(name, sizeof name, "%s/z", s->cfg.cache_prefix);
	termgfx_stream_apc(s, termgfx_audio_cache_file(&s->apc, &s->apc_cap, name, ogg, n));
	free(ogg);
	s->have_silence = 1;
}

termgfx_stream_t *
termgfx_stream_create(const termgfx_stream_cfg_t *cfg, const termgfx_stream_io_t *io)
{
	termgfx_stream_t *s;
	size_t            frames;

	if (cfg == NULL || io == NULL || io->put == NULL || io->flush == NULL ||
	    io->backlog == NULL)
		return NULL;
	if (!cfg->enabled)
		return NULL;               // not one audio byte
	s = calloc(1, sizeof *s);
	if (s == NULL)
		return NULL;
	s->cfg = *cfg;
	s->io  = *io;

	// Clamp ONCE, here: everything downstream reads the by-value copy, so a
	// value cannot change under the state machine between two reads of it.
	if (!(s->cfg.quality >= 0.01 && s->cfg.quality <= 1.0))
		s->cfg.quality = TERMGFX_MUSIC_QUALITY_DEFAULT;
	if (s->cfg.volume < 0)
		s->cfg.volume = 0;
	else if (s->cfg.volume > 100)
		s->cfg.volume = 100;
	if (s->cfg.chunk_ms < 50)
		s->cfg.chunk_ms = 50;
	else if (s->cfg.chunk_ms > 250)
		s->cfg.chunk_ms = 250;
	if (s->cfg.prebuffer < 2)
		s->cfg.prebuffer = 2;
	else if (s->cfg.prebuffer > 8)
		s->cfg.prebuffer = 8;
	if (s->cfg.channels < 1)
		s->cfg.channels = 1;
	else if (s->cfg.channels > 2)
		s->cfg.channels = 2;

	snprintf(s->name, sizeof s->name, "%s",
	         cfg->name == NULL ? "termgfx" : cfg->name);
	snprintf(s->prefix, sizeof s->prefix, "%s",
	         cfg->cache_prefix == NULL ? "s" : cfg->cache_prefix);
	// NEVER memcpy or assign this struct (`termgfx_stream_t tmp = *s;`): these
	// two point INTO it, at our own copies of the caller's strings, so a
	// bitwise copy would leave the clone's cfg aliasing the original's
	// buffers. Safe as written only because the type is opaque and never
	// copied -- keep it that way.
	s->cfg.name         = s->name;
	s->cfg.cache_prefix = s->prefix;

	if (s->cfg.rate < 8000 || s->cfg.rate > 192000) {
		fprintf(stderr, "%s: source reports sample_rate %d; using %d\n",
		        s->cfg.name, s->cfg.rate, TERMGFX_STREAM_RATE_FALLBACK);
		s->cfg.rate = TERMGFX_STREAM_RATE_FALLBACK;
	}

	// The chunk is sized HERE because its length in FRAMES depends on the rate:
	// chunk_ms of audio is 4410 frames at 44.1 kHz and 4800 at 48 kHz, and the
	// ~300 ms cushion is counted in chunks.
	frames = (size_t)s->cfg.rate * (size_t)s->cfg.chunk_ms / 1000;
	if (!termgfx_chunk_init(&s->chunk, frames, s->cfg.channels)) {
		free(s);
		return NULL;
	}
	fprintf(stderr, "%s: audio %d Hz, %d ms chunks (%u frames)\n",
	        s->cfg.name, s->cfg.rate, s->cfg.chunk_ms, (unsigned)frames);
	s->state = TERMGFX_STREAM_WAIT_CAPS;
	return s;
}

void
termgfx_stream_probe(termgfx_stream_t *s)
{
	if (s == NULL || s->state == TERMGFX_STREAM_OFF)
		return;
	s->io.put(s->io.ctx, termgfx_audio_query, strlen(termgfx_audio_query));
}

void
termgfx_stream_caps(termgfx_stream_t *s, int tier)
{
	if (s == NULL || s->state != TERMGFX_STREAM_WAIT_CAPS)
		return;
	if (tier != 1) {
		s->state = TERMGFX_STREAM_OFF;   // no libsndfile: A;Load is a silent no-op
		termgfx_chunk_free(&s->chunk);
		return;
	}
	termgfx_stream_apc(s, termgfx_audio_volume(&s->apc, &s->apc_cap, s->cfg.ch,
	                                           termgfx_db_from_pct(s->cfg.volume)));
	termgfx_stream_upload_silence(s);
	s->state = TERMGFX_STREAM_PRIME;
}

void
termgfx_stream_underrun(termgfx_stream_t *s, int ch)
{
	if (s == NULL || ch != s->cfg.ch || s->state != TERMGFX_STREAM_RUN)
		return;
	if (s->paused)
		return;                    // the FIFO drained because the source stopped
	s->underruns++;
	s->state = TERMGFX_STREAM_PRIME;   // re-prime: never dribble into an empty FIFO
}

// ---- volume ---------------------------------------------------------------
//
// The volume is the TERMINAL's mixer-channel volume (A;Volume), not a gain we
// apply to the PCM: the player's own client does the scaling, so turning it down
// costs us nothing and loses no precision.
//
// ZERO IS NOT "VERY QUIET". At 0 we stop sending audio ENTIRELY -- no encode, no
// upload, no Queue -- and flush what is already queued, so a muted player pays
// ZERO bytes for sound rather than streaming ~100 ms Opus chunks at volume 0 to a
// terminal that will silently discard them. That is the whole point of a mute on
// a BBS door, where the audio is the bulk of the uplink after the frames.
int
termgfx_stream_volume(const termgfx_stream_t *s)
{
	return s == NULL ? 0 : s->cfg.volume;
}

int
termgfx_stream_muted(const termgfx_stream_t *s)
{
	return s == NULL ? 1 : s->cfg.volume <= 0;
}

// Step the volume by `delta` (clamped to 0..100) and return the new value.
int
termgfx_stream_volume_step(termgfx_stream_t *s, int delta)
{
	int was;

	if (s == NULL)
		return 0;
	was = s->cfg.volume;

	s->cfg.volume += delta;
	if (s->cfg.volume < 0)
		s->cfg.volume = 0;
	if (s->cfg.volume > 100)
		s->cfg.volume = 100;
	if (s->cfg.volume == was || s->state == TERMGFX_STREAM_OFF)
		return s->cfg.volume;   // no change, or no audio pipeline to tell

	if (s->cfg.volume == 0) {
		// Silence NOW: the ~300 ms cushion already in the terminal's FIFO would
		// otherwise keep playing after the player asked for quiet.
		termgfx_stream_stop(s);
		termgfx_stream_drop_held(s);
		termgfx_chunk_reset(&s->chunk);
		if (s->state == TERMGFX_STREAM_RUN)
			s->state = TERMGFX_STREAM_PRIME;   // rebuild the cushion when we come back
		return s->cfg.volume;
	}

	termgfx_stream_apc(s, termgfx_audio_volume(&s->apc, &s->apc_cap, s->cfg.ch,
	                                           termgfx_db_from_pct(s->cfg.volume)));
	if (was == 0) {
		// Coming back from mute: the FIFO is empty and the half-chunk we were
		// NOT accumulating is gone, so re-prime rather than dribbling one chunk
		// into an empty FIFO (the underrun the whole cushion exists to avoid).
		termgfx_chunk_reset(&s->chunk);
		if (s->state == TERMGFX_STREAM_RUN)
			s->state = TERMGFX_STREAM_PRIME;
	}
	return s->cfg.volume;
}

void
termgfx_stream_pause(termgfx_stream_t *s, int on)
{
	if (s == NULL || s->state == TERMGFX_STREAM_OFF)
		return;
	s->paused = on;
	if (on) {
		// Anything held mid-PRIME belongs to the moment before the screen went
		// up. Queueing it on resume would replay stale audio.
		termgfx_stream_drop_held(s);
		return;
	}
	termgfx_chunk_reset(&s->chunk);        // drop the half-chunk straddling the pause
	if (s->state == TERMGFX_STREAM_RUN)
		s->state = TERMGFX_STREAM_PRIME;   // rebuild the cushion the drain destroyed
}

// One closed chunk: silence path, drop path, or encode+send.
static void
termgfx_stream_chunk_closed(termgfx_stream_t *s)
{
	uint8_t *ogg = NULL;
	char     name[32];
	size_t   n;

	if (s->chunk.len == 0)
		return;
	if (s->cfg.volume <= 0)
		return;                    // muted: the one place audio leaves the door

	if (s->io.backlog(s->io.ctx) > TERMGFX_STREAM_BACKLOG_BYTES)
		s->strikes++;
	else
		s->strikes = 0;
	if (s->strikes >= TERMGFX_STREAM_BACKLOG_STRIKES) {
		s->dropped++;              // drop at the INPUT, before encoding
		return;
	}

	if (s->have_silence && termgfx_chunk_is_silent(&s->chunk)) {
		if (s->state == TERMGFX_STREAM_RUN) {
			snprintf(name, sizeof name, "%s/z", s->cfg.cache_prefix);
			termgfx_stream_play(s, name);   // no upload, no encode: headers alone
		}
		return;                    // silence never primes: it has no cushion value
	}

	n = termgfx_stream_encode(s, &ogg);
	if (n == 0 || ogg == NULL) {
		s->dropped++;              // one 100 ms gap beats stalling the source
		return;
	}

	if (s->state == TERMGFX_STREAM_PRIME) {
		if (s->held_n < (int)(sizeof s->held / sizeof s->held[0])) {
			s->held[s->held_n]     = ogg;
			s->held_len[s->held_n] = n;
			s->held_n++;
		} else {
			free(ogg);
			s->dropped++;
		}
		if (s->held_n >= s->cfg.prebuffer) {
			int i;

			for (i = 0; i < s->held_n; i++) {
				termgfx_stream_send(s, s->held[i], s->held_len[i]);
				free(s->held[i]);
			}
			s->held_n = 0;
			s->state  = TERMGFX_STREAM_RUN;
			s->io.flush(s->io.ctx);
		}
		return;
	}

	termgfx_stream_send(s, ogg, n);
	free(ogg);
}

size_t
termgfx_stream_feed(termgfx_stream_t *s, const int16_t *pcm, size_t frames)
{
	size_t left = frames;

	if (s == NULL)
		return frames;
	if (s->state == TERMGFX_STREAM_OFF || s->state == TERMGFX_STREAM_WAIT_CAPS ||
	    pcm == NULL)
		return frames;             // the source must believe we took it all
	if (s->cfg.volume <= 0)
		return frames;             // MUTED: not one sample accumulated, not one
	                               // byte encoded, not one byte sent

	while (left > 0) {
		size_t used = termgfx_chunk_append(&s->chunk, pcm, left);

		pcm  += used * 2;          // interleaved stereo
		left -= used;
		if (termgfx_chunk_full(&s->chunk)) {
			termgfx_stream_chunk_closed(s);
			termgfx_chunk_reset(&s->chunk);
		} else if (used == 0) {
			break;                 // cannot happen: a reset chunk has room
		}
	}
	return frames;
}

// Stop the channel dead and drop everything queued behind it.
//
// NO FADE, deliberately. SyncTERM's do_flush() clears the channel only when no
// O= is given; with a fade it queues a silent crossfade against the HEAD buffer
// and leaves the rest of the FIFO in place (syncterm/audio_apc.c). Since we run
// several chunks deep, `O=50` would let ~300 ms of game audio play on after the
// door has already handed the terminal back to the BBS. An abrupt stop at the
// moment the player quits is what they expect anyway.
void
termgfx_stream_stop(termgfx_stream_t *s)
{
	if (s == NULL)
		return;
	if (s->state != TERMGFX_STREAM_PRIME && s->state != TERMGFX_STREAM_RUN)
		return;                    // the terminal never confirmed it can play
	termgfx_stream_apc(s, termgfx_audio_flush(&s->apc, &s->apc_cap, s->cfg.ch, 0));
	s->io.flush(s->io.ctx);
}

// The source restarted, so the ~300 ms already queued is from a session that no
// longer exists. Drop it, and rebuild the cushion from the new one.
void
termgfx_stream_reset(termgfx_stream_t *s)
{
	if (s == NULL || s->state == TERMGFX_STREAM_OFF)
		return;
	termgfx_stream_stop(s);
	termgfx_stream_drop_held(s);
	termgfx_chunk_reset(&s->chunk);
	if (s->state == TERMGFX_STREAM_RUN)
		s->state = TERMGFX_STREAM_PRIME;
}

void
termgfx_stream_stats(const termgfx_stream_t *s, unsigned *chunks,
                     unsigned *underruns, unsigned *dropped)
{
	if (chunks != NULL)
		*chunks = s == NULL ? 0 : s->chunks;
	if (underruns != NULL)
		*underruns = s == NULL ? 0 : s->underruns;
	if (dropped != NULL)
		*dropped = s == NULL ? 0 : s->dropped;
}

void
termgfx_stream_destroy(termgfx_stream_t *s)
{
	if (s == NULL)
		return;
	termgfx_stream_stop(s);
	if (s->chunks || s->underruns || s->dropped)
		fprintf(stderr, "%s: audio %u chunks, %u underrun(s), %u drop(s)\n",
		        s->cfg.name, s->chunks, s->underruns, s->dropped);
	termgfx_stream_drop_held(s);
	termgfx_chunk_free(&s->chunk);
	free(s->apc);
	free(s);
}
