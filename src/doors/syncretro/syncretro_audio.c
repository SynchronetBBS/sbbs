/* syncretro_audio.c -- syncretro's adapter onto termgfx's shared PCM
 * streaming module (termgfx/audio_stream.h). The state machine, cushion,
 * backlog policy, silence cache and blob path all live there now; what
 * remains here is the door's half: its output seam, its INI-sourced config,
 * and the rate the CORE reports (which is why creation waits for
 * rc_core_load_game() -- see sr_audio_start()). M4_AUDIO.md still describes
 * the design; the code moved, the reasoning did not. */
#include "syncretro_audio.h"
#include "audio_stream.h"
#include "syncretro.h"
#include "audio.h"        /* termgfx: the audio-APC builders */

#include <stddef.h>

static termgfx_stream_t *g_stream;
static int               g_enabled;  /* sr_audio_init() said yes; sr_audio_start()
                                      * may still bail */

/* --- the door's output seam -------------------------------------------------
 * syncretro's I/O is file-static (one door session per process), so the
 * module's `ctx` carries nothing. */

static void sr_stream_put(void *ctx, const void *buf, size_t len)
{
	(void)ctx;
	sr_out_put(buf, len);
}

static int sr_stream_flush(void *ctx)
{
	(void)ctx;
	return sr_io_out_flush();
}

static size_t sr_stream_backlog(void *ctx)
{
	(void)ctx;
	return sr_io_out_backlog();
}

static const termgfx_stream_io_t g_stream_io = {
	sr_stream_put, sr_stream_flush, sr_stream_backlog, NULL
};

/* Config only -- no allocation, no I/O, and above all no rate: the core has not
 * been loaded yet when main() calls this, so its sample rate does not exist. */
void sr_audio_init(void)
{
	if (!sr_config_audio_enabled() || !termgfx_audio_have_ogg())
		return;                    /* the stream is never created: not one audio byte */
	g_enabled = 1;
}

/* Called once from main() after rc_core_load_game(), with the rate the CORE
 * reports. That rate is why creation happens here rather than in init(): the
 * chunk's length in FRAMES depends on it, and the cushion is counted in chunks.
 * `channels = 1` -- the cores emit mono duplicated across both sides, so the
 * accumulator keeps the left sample and halves everything downstream. */
void sr_audio_start(int rate)
{
	termgfx_stream_cfg_t cfg;

	if (!g_enabled)
		return;                    /* audio off, or no Ogg encoder: stay silent */

	cfg.enabled      = 1;
	cfg.quality      = sr_config_audio_quality();
	cfg.volume_db    = termgfx_db_from_pct(sr_config_audio_volume());   /* percent knob -> dB */
	cfg.chunk_ms     = sr_config_audio_chunk_ms();
	cfg.prebuffer    = sr_config_audio_prebuffer();
	cfg.channels     = 1;
	cfg.rate         = rate;
	cfg.ch           = SR_AUDIO_CH;
	cfg.slot         = SR_AUDIO_SLOT;
	cfg.name         = "syncretro";
	cfg.cache_prefix = "s";
	g_stream         = termgfx_stream_create(&cfg, &g_stream_io);
}

void sr_audio_probe(void)
{
	termgfx_stream_probe(g_stream);
}

void sr_audio_caps(int tier)
{
	termgfx_stream_caps(g_stream, tier);
}

void sr_audio_set_blob_ok(int ok)
{
	termgfx_stream_set_blob_ok(g_stream, ok);
}

int sr_audio_blob_active(void)
{
	return termgfx_stream_blob_active(g_stream);
}

void sr_audio_underrun(int ch)
{
	termgfx_stream_underrun(g_stream, ch);
}

void sr_audio_pause(int on)
{
	termgfx_stream_pause(g_stream, on);
}

float sr_audio_volume(void)
{
	return termgfx_stream_volume(g_stream);
}

int sr_audio_muted(void)
{
	return termgfx_stream_muted(g_stream);
}

float sr_audio_volume_step(float delta_db)
{
	return termgfx_stream_volume_step(g_stream, delta_db);
}

void sr_audio_reset(void)
{
	termgfx_stream_reset(g_stream);
}

size_t sr_audio_feed(const int16_t *pcm, size_t frames)
{
	return termgfx_stream_feed(g_stream, pcm, frames);
}

/* Idempotent: destroy() no-ops on NULL, and we clear the handle. */
void sr_audio_shutdown(void)
{
	termgfx_stream_destroy(g_stream);
	g_stream = NULL;
}
