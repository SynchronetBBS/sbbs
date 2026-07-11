/* syncretro_audio.c -- see syncretro_audio.h and M4_AUDIO.md. */
#include "syncretro_audio.h"
#include "syncretro_chunk.h"
#include "syncretro.h"
#include "audio.h"        /* termgfx: the audio-APC builders */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- the streaming module ---------------------------------------------------
 *
 * Chunks are produced in real time (the core runs at wall clock) and consumed
 * in real time, so the channel FIFO's depth is whatever cushion we establish at
 * the start and never grows on its own. That is why playback waits for
 * `prebuffer` chunks: a FIFO fed one chunk at a time from empty stays one deep
 * and underruns on the first jitter. Those chunks ARE the latency budget.
 */

enum { SR_AS_OFF = 0, SR_AS_WAIT_CAPS, SR_AS_PRIME, SR_AS_RUN };

static int        g_state = SR_AS_OFF;
static int        g_paused;
static sr_chunk_t g_chunk;

static double     g_quality;
static int        g_volume;
static int        g_prebuffer;

static uint8_t *  g_apc;          /* reusable builder scratch */
static size_t     g_apc_cap;

/* Encoded chunks held during PRIME, emitted together once we have `prebuffer`. */
static uint8_t *  g_held[8];
static size_t     g_held_len[8];
static int        g_held_n;

static unsigned   g_ring;         /* next cache name index */
static int        g_have_silence; /* s/z uploaded */
static int        g_blob_ok;      /* client supports A;LoadBlob (CTerm >= 1.329) */

/* Telemetry, printed at shutdown. */
static unsigned   g_underruns;
static unsigned   g_dropped;
static unsigned   g_chunks;

/* Sustained-backlog drop. NOT an instantaneous socket check: SyncDOOM tried
 * that for SFX and reverted it, because the frame path keeps the socket busy
 * essentially always (syncdoom/i_termsound.c). Two consecutive chunk boundaries
 * over the threshold is ~200 ms of real congestion, not a momentary burst. */
#define SR_AUDIO_BACKLOG_BYTES (48 * 1024)
#define SR_AUDIO_BACKLOG_STRIKES 2
static int g_strikes;

static void sr_apc(size_t n)
{
	if (n > 0)
		sr_out_put(g_apc, n);
}

static void sr_audio_name(char *out, size_t outlen, unsigned idx)
{
	snprintf(out, outlen, "s/%u", idx % SR_AUDIO_RING);
}

/* Load slot 0 from a cached name and queue it; re-arm the idle notification. */
static void sr_audio_play(const char *name)
{
	sr_apc(termgfx_audio_load(&g_apc, &g_apc_cap, SR_AUDIO_SLOT, name));
	sr_apc(termgfx_audio_queue(&g_apc, &g_apc_cap, SR_AUDIO_CH, SR_AUDIO_SLOT,
	                           100, 0, 0));
	sr_apc(termgfx_audio_update(&g_apc, &g_apc_cap, SR_AUDIO_CH));
	g_chunks++;
}

/* Enable the inline A;LoadBlob path (CTerm >= 1.329). Set from the input layer
 * once the DA1 reply's version is known; until then we stay on the cache ring. */
void sr_audio_set_blob_ok(int ok)
{
	g_blob_ok = ok ? 1 : 0;
}

/* 1 once the inline-blob path is active (streaming chunks skip the cache ring).
 * For the stats overlay -- retro's video is sixel, so blob only ever applies to
 * the audio stream here. */
int sr_audio_blob_active(void)
{
	return g_blob_ok;
}

/* Ship one encoded chunk. On a CTerm >= 1.329 client, inline it via A;LoadBlob
 * (no cache file, no ring churn); otherwise upload under the next ring name and
 * play it by name. Unique game-audio chunks never repeat, so the blob path is a
 * pure win where available. (The named "s/z" silence chunk stays cached -- it
 * IS replayed, so upload-once-by-name still beats re-sending it.) */
static void sr_audio_send(const uint8_t *ogg, size_t len)
{
	char name[16];

	if (g_blob_ok) {
		sr_apc(termgfx_audio_load_blob_file(&g_apc, &g_apc_cap, SR_AUDIO_SLOT, ogg, len));
		sr_apc(termgfx_audio_queue(&g_apc, &g_apc_cap, SR_AUDIO_CH, SR_AUDIO_SLOT, 100, 0, 0));
		sr_apc(termgfx_audio_update(&g_apc, &g_apc_cap, SR_AUDIO_CH));
		g_chunks++;
		return;
	}
	sr_audio_name(name, sizeof name, g_ring++);
	sr_apc(termgfx_audio_cache_file(&g_apc, &g_apc_cap, name, ogg, len));
	sr_audio_play(name);
}

static void sr_audio_stop_channel(void);

static void sr_audio_drop_held(void)
{
	int i;

	for (i = 0; i < g_held_n; i++)
		free(g_held[i]);
	g_held_n = 0;
}

/* The core's sample rate, from retro_get_system_av_info() -- NOT a constant.
 * FreeIntv mixes at 44100 and fceumm at 48000, and a chunk sized or encoded at
 * the wrong rate is both mis-pitched and mis-timed against the FIFO cushion the
 * whole pacing scheme rests on. Zero until sr_audio_start(). */
static int g_rate;
static int g_enabled;   /* sr_audio_init() said yes; sr_audio_start() may still bail */

/* Encode one closed chunk. Returns malloc'd Ogg bytes in *out, or 0. */
static size_t sr_audio_encode(uint8_t **out)
{
	return termgfx_audio_encode_ogg(g_chunk.buf, g_chunk.len, 1, g_rate,
	                                g_quality, out);
}

/* Upload the silent chunk once; thereafter a silent chunk costs a Load+Queue
 * and no payload at all. */
static void sr_audio_upload_silence(void)
{
	uint8_t *ogg = NULL;
	size_t   n;
	size_t   i;

	for (i = 0; i < g_chunk.cap; i++)
		g_chunk.buf[i] = 0;
	g_chunk.len = g_chunk.cap;
	n = sr_audio_encode(&ogg);
	g_chunk.len = 0;
	if (n == 0 || ogg == NULL)
		return;                    /* leave g_have_silence 0: we just pay bytes */
	sr_apc(termgfx_audio_cache_file(&g_apc, &g_apc_cap, "s/z", ogg, n));
	free(ogg);
	g_have_silence = 1;
}

/* Config only -- no allocation, no I/O, and above all no rate: the core has not
 * been loaded yet when main() calls this, so its sample rate does not exist. */
void sr_audio_init(void)
{
	if (!sr_config_audio_enabled() || !termgfx_audio_have_ogg())
		return;                    /* stays SR_AS_OFF: not one audio byte */
	g_quality   = sr_config_audio_quality();
	g_volume    = sr_config_audio_volume();
	g_prebuffer = sr_config_audio_prebuffer();
	g_enabled   = 1;
}

/* Called once from main() after rc_core_load_game(), with the rate the CORE
 * reports. This is where the chunk is finally sized, because its length in
 * FRAMES depends on that rate: chunk_ms of audio is 4410 frames at 44.1 kHz and
 * 4800 at 48 kHz, and the ~300 ms cushion is counted in chunks. */
void sr_audio_start(int rate)
{
	size_t frames;

	if (!g_enabled)
		return;                    /* audio off, or no Ogg encoder: stay silent */

	/* A core that reports a nonsense rate should be audible-but-wrong, never
	 * silent, and never a division by zero. */
	if (rate < 8000 || rate > 192000) {
		fprintf(stderr, "syncretro: core reports sample_rate %d; using %d\n",
		        rate, SR_AUDIO_RATE_FALLBACK);
		rate = SR_AUDIO_RATE_FALLBACK;
	}
	g_rate = rate;

	frames = (size_t)g_rate * (size_t)sr_config_audio_chunk_ms() / 1000;
	if (!sr_chunk_init(&g_chunk, frames))
		return;
	fprintf(stderr, "syncretro: audio %d Hz, %d ms chunks (%u frames)\n",
	        g_rate, sr_config_audio_chunk_ms(), (unsigned)frames);
	g_state = SR_AS_WAIT_CAPS;
}

void sr_audio_probe(void)
{
	if (g_state == SR_AS_OFF)
		return;
	sr_out_put(termgfx_audio_query, strlen(termgfx_audio_query));
}

void sr_audio_caps(int tier)
{
	if (g_state != SR_AS_WAIT_CAPS)
		return;
	if (tier != 1) {
		g_state = SR_AS_OFF;       /* no libsndfile: A;Load is a silent no-op */
		sr_chunk_free(&g_chunk);
		return;
	}
	sr_apc(termgfx_audio_volume(&g_apc, &g_apc_cap, SR_AUDIO_CH, g_volume));
	sr_audio_upload_silence();
	g_state = SR_AS_PRIME;
}

void sr_audio_underrun(int ch)
{
	if (ch != SR_AUDIO_CH || g_state != SR_AS_RUN)
		return;
	if (g_paused)
		return;                    /* the FIFO drained because we stopped the core */
	g_underruns++;
	g_state = SR_AS_PRIME;         /* re-prime: never dribble into an empty FIFO */
}

/* --- volume ('+' / '-') ------------------------------------------------------
 *
 * The volume is the TERMINAL's mixer-channel volume (A;Volume), not a gain we
 * apply to the PCM: the player's own client does the scaling, so turning it down
 * costs us nothing and loses no precision.
 *
 * ZERO IS NOT "VERY QUIET". At 0 we stop sending audio ENTIRELY -- no encode, no
 * upload, no Queue -- and flush what is already queued, so a muted player pays
 * ZERO bytes for sound rather than streaming ~100 ms Opus chunks at volume 0 to a
 * terminal that will silently discard them. That is the whole point of a mute on
 * a BBS door, where the audio is the bulk of the uplink after the frames. */
int sr_audio_volume(void)
{
	return g_volume;
}

int sr_audio_muted(void)
{
	return g_volume <= 0;
}

/* Step the volume by `delta` (clamped to 0..100) and return the new value. */
int sr_audio_volume_step(int delta)
{
	int was = g_volume;

	g_volume += delta;
	if (g_volume < 0)
		g_volume = 0;
	if (g_volume > 100)
		g_volume = 100;
	if (g_volume == was || g_state == SR_AS_OFF)
		return g_volume;    /* no change, or no audio pipeline to tell */

	if (g_volume == 0) {
		/* Silence NOW: the ~300 ms cushion already in the terminal's FIFO would
		 * otherwise keep playing after the player asked for quiet. */
		sr_audio_stop_channel();
		sr_audio_drop_held();
		sr_chunk_reset(&g_chunk);
		if (g_state == SR_AS_RUN)
			g_state = SR_AS_PRIME;   /* rebuild the cushion when we come back */
		return g_volume;
	}

	sr_apc(termgfx_audio_volume(&g_apc, &g_apc_cap, SR_AUDIO_CH, g_volume));
	if (was == 0) {
		/* Coming back from mute: the FIFO is empty and the half-chunk we were
		 * NOT accumulating is gone, so re-prime rather than dribbling one chunk
		 * into an empty FIFO (the underrun the whole cushion exists to avoid). */
		sr_chunk_reset(&g_chunk);
		if (g_state == SR_AS_RUN)
			g_state = SR_AS_PRIME;
	}
	return g_volume;
}

void sr_audio_pause(int on)
{
	if (g_state == SR_AS_OFF)
		return;
	g_paused = on;
	if (on) {
		/* Anything held mid-PRIME belongs to the moment before the screen went
		 * up. Queueing it on resume would replay stale audio. */
		sr_audio_drop_held();
		return;
	}
	sr_chunk_reset(&g_chunk);      /* drop the half-chunk straddling the pause */
	if (g_state == SR_AS_RUN)
		g_state = SR_AS_PRIME;     /* rebuild the cushion the drain destroyed */
}

/* One closed chunk: silence path, drop path, or encode+send. */
static void sr_audio_chunk_closed(void)
{
	uint8_t *ogg = NULL;
	size_t   n;

	if (g_chunk.len == 0)
		return;
	if (g_volume <= 0)
		return;                    /* muted: the one place audio leaves the door */

	if (sr_io_out_backlog() > SR_AUDIO_BACKLOG_BYTES)
		g_strikes++;
	else
		g_strikes = 0;
	if (g_strikes >= SR_AUDIO_BACKLOG_STRIKES) {
		g_dropped++;               /* drop at the INPUT, before encoding */
		return;
	}

	if (g_have_silence && sr_chunk_is_silent(&g_chunk)) {
		if (g_state == SR_AS_RUN)
			sr_audio_play("s/z");  /* no upload, no encode: headers alone */
		return;                    /* silence never primes: it has no cushion value */
	}

	n = sr_audio_encode(&ogg);
	if (n == 0 || ogg == NULL) {
		g_dropped++;               /* one 100 ms gap beats stalling the game */
		return;
	}

	if (g_state == SR_AS_PRIME) {
		if (g_held_n < (int)(sizeof g_held / sizeof g_held[0])) {
			g_held[g_held_n]     = ogg;
			g_held_len[g_held_n] = n;
			g_held_n++;
		} else {
			free(ogg);
			g_dropped++;
		}
		if (g_held_n >= g_prebuffer) {
			int i;

			for (i = 0; i < g_held_n; i++) {
				sr_audio_send(g_held[i], g_held_len[i]);
				free(g_held[i]);
			}
			g_held_n = 0;
			g_state  = SR_AS_RUN;
			sr_io_out_flush();
		}
		return;
	}

	sr_audio_send(ogg, n);
	free(ogg);
}

size_t sr_audio_feed(const int16_t *pcm, size_t frames)
{
	size_t left = frames;

	if (g_state == SR_AS_OFF || g_state == SR_AS_WAIT_CAPS || pcm == NULL)
		return frames;             /* the core must believe we took it all */
	if (g_volume <= 0)
		return frames;             /* MUTED: not one sample accumulated, not one
	                                * byte encoded, not one byte sent */

	while (left > 0) {
		size_t used = sr_chunk_append(&g_chunk, pcm, left);

		pcm  += used * 2;          /* interleaved stereo */
		left -= used;
		if (sr_chunk_full(&g_chunk)) {
			sr_audio_chunk_closed();
			sr_chunk_reset(&g_chunk);
		} else if (used == 0) {
			break;                 /* cannot happen: a reset chunk has room */
		}
	}
	return frames;
}

/* Stop the channel dead and drop everything queued behind it.
 *
 * NO FADE, deliberately. SyncTERM's do_flush() clears the channel only when no
 * O= is given; with a fade it queues a silent crossfade against the HEAD buffer
 * and leaves the rest of the FIFO in place (syncterm/audio_apc.c). Since we run
 * three chunks deep, `O=50` would let ~300 ms of game audio play on after the
 * door has already handed the terminal back to the BBS. An abrupt stop at the
 * moment the player quits is what they expect anyway. */
static void sr_audio_stop_channel(void)
{
	if (g_state != SR_AS_PRIME && g_state != SR_AS_RUN)
		return;                    /* the terminal never confirmed it can play */
	sr_apc(termgfx_audio_flush(&g_apc, &g_apc_cap, SR_AUDIO_CH, 0));
	sr_io_out_flush();
}

/* Ctrl-R: the console restarts, so the ~300 ms already queued is from a game
 * that no longer exists. Drop it, and rebuild the cushion from the new one. */
void sr_audio_reset(void)
{
	if (g_state == SR_AS_OFF)
		return;
	sr_audio_stop_channel();
	sr_audio_drop_held();
	sr_chunk_reset(&g_chunk);
	if (g_state == SR_AS_RUN)
		g_state = SR_AS_PRIME;
}

void sr_audio_shutdown(void)
{
	if (g_state == SR_AS_OFF)
		return;
	sr_audio_stop_channel();
	if (g_chunks || g_underruns || g_dropped)
		fprintf(stderr, "syncretro: audio %u chunks, %u underrun(s), %u drop(s)\n",
		        g_chunks, g_underruns, g_dropped);
	sr_audio_drop_held();
	sr_chunk_free(&g_chunk);
	free(g_apc);
	g_apc     = NULL;
	g_apc_cap = 0;
	g_state   = SR_AS_OFF;
}
