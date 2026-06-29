// audio_mgr.c -- stateful policy over audio.h's SyncTERM audio-APC builders.
// See audio_mgr.h. Tracks the capability tier, an upload-once sample cache, and
// round-robin SFX slot/channel cursors; emits through the door-supplied
// callback. I/O-free apart from that callback.

#include "audio_mgr.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEYMAX       1024            // id space (Duke ~450, Doom ~110)
#define NSLOT        256             // SyncTERM patch slots
#define MUSIC_SLOT   0               // slot 0 reserved for the music loop
#define SFX_SLOT_LO  1               // SFX slots 1..255 (transient load->queue buffers)
#define MUSIC_CH     2               // channel 2 reserved for music
#define SFX_CH_LO    3               // SFX channels 3..15
#define SFX_CH_HI    15
#define NSFX_CH      (SFX_CH_HI - SFX_CH_LO + 1)

struct termgfx_audio {
	termgfx_audio_emit_fn emit;
	void *ctx;
	int tier;                              // -1 unknown/none, 0 tone, 1 digital

	uint8_t sfx_up[KEYMAX];                // id has been C;S-Stored (sfx file)
	uint8_t mus_up[KEYMAX];                // id has been C;S-Stored (music file)
	int next_slot;                         // round-robin SFX slot cursor
	int next_ch;                           // round-robin SFX channel cursor
	int music_id;                          // id in the music slot, -1 none

	uint8_t acc[32];                       // rolling window for the probe reply
	int acclen;

	uint8_t *buf;                          // scratch for builders
	size_t cap;
};

termgfx_audio_t *termgfx_audio_create(termgfx_audio_emit_fn emit, void *ctx)
{
	termgfx_audio_t *m = calloc(1, sizeof(*m));

	if (m == NULL)
		return NULL;
	m->emit      = emit;
	m->ctx       = ctx;
	m->tier      = -1;
	m->next_slot = SFX_SLOT_LO;
	m->next_ch   = SFX_CH_LO;
	m->music_id  = -1;
	return m;
}

void termgfx_audio_destroy(termgfx_audio_t *m)
{
	if (m == NULL)
		return;
	free(m->buf);
	free(m);
}

// Send one builder's output (n bytes already in m->buf) to the door.
static void send_buf(termgfx_audio_t *m, size_t n)
{
	if (n > 0)
		m->emit(m->ctx, m->buf, n);
}

// ---- capability probe ----------------------------------------------------

void termgfx_audio_probe(termgfx_audio_t *m)
{
	if (m == NULL)
		return;
	m->emit(m->ctx, termgfx_audio_query, strlen(termgfx_audio_query));
}

void termgfx_audio_feed(termgfx_audio_t *m, const uint8_t *buf, int len)
{
	int r;

	if (m == NULL || m->tier >= 0 || len <= 0)
		return;
	// Keep the last (sizeof acc) bytes of the stream so a reply split across
	// feeds still parses. The DSR is ~12 contiguous bytes; 32 is ample.
	if (len >= (int)sizeof(m->acc)) {
		memcpy(m->acc, buf + (len - (int)sizeof(m->acc)), sizeof(m->acc));
		m->acclen = (int)sizeof(m->acc);
	}
	else {
		int keep = (int)sizeof(m->acc) - len;
		if (m->acclen > keep) {
			memmove(m->acc, m->acc + (m->acclen - keep), keep);
			m->acclen = keep;
		}
		memcpy(m->acc + m->acclen, buf, len);
		m->acclen += len;
	}
	r = termgfx_audio_parse_caps(m->acc, m->acclen);
	if (r >= 0)
		m->tier = r;
}

int termgfx_audio_tier(const termgfx_audio_t *m)
{
	return m == NULL ? -1 : m->tier;
}

void termgfx_audio_set_tier(termgfx_audio_t *m, int tier)
{
	if (m != NULL)
		m->tier = tier;
}

// ---- SFX -----------------------------------------------------------------

// Load the cached `fn` into the next rotating slot and Queue it on the next
// pooled channel. A Queue EMPTIES its slot (the buffer moves into the channel
// FIFO), so every play must re-Load first -- the slot is a transient handoff
// buffer, not a persistent cache. The sample FILE stays in SyncTERM's cache
// (uploaded once), so the Load just re-decodes it -- no re-transfer.
static void sfx_dispatch(termgfx_audio_t *m, const char *fn, int vol, int pan)
{
	int slot = m->next_slot;
	int ch   = m->next_ch;

	m->next_slot = SFX_SLOT_LO + ((m->next_slot - SFX_SLOT_LO + 1) % (NSLOT - SFX_SLOT_LO));
	m->next_ch   = SFX_CH_LO + ((m->next_ch - SFX_CH_LO + 1) % NSFX_CH);
	send_buf(m, termgfx_audio_load(&m->buf, &m->cap, slot, fn));
	send_buf(m, termgfx_audio_queue(&m->buf, &m->cap, ch, slot, vol, pan, 0));
}

void termgfx_audio_sfx(termgfx_audio_t *m, int id,
                       const void *pcm, size_t bytes, int bits, int channels,
                       int rate, int vol, int pan)
{
	char fn[24];

	if (m == NULL || m->tier < 1 || id < 0 || id >= KEYMAX || bytes == 0)
		return;
	snprintf(fn, sizeof(fn), "a/%d", id);

	if (!m->sfx_up[id]) {                  // wrap raw PCM in a WAV + Store, once
		send_buf(m, termgfx_audio_cache_pcm(&m->buf, &m->cap, fn,
		                                    pcm, bytes, bits, channels, rate));
		m->sfx_up[id] = 1;
	}
	sfx_dispatch(m, fn, vol, pan);
}

void termgfx_audio_sfx_file(termgfx_audio_t *m, int id,
                            const void *filedata, size_t filelen, int vol, int pan)
{
	char fn[24];

	if (m == NULL || m->tier < 1 || id < 0 || id >= KEYMAX || filelen == 0)
		return;
	snprintf(fn, sizeof(fn), "a/%d", id);

	if (!m->sfx_up[id]) {                  // Store the sound file verbatim, once
		send_buf(m, termgfx_audio_cache_file(&m->buf, &m->cap, fn, filedata, filelen));
		m->sfx_up[id] = 1;
	}
	sfx_dispatch(m, fn, vol, pan);
}

// ---- music ---------------------------------------------------------------

void termgfx_audio_music(termgfx_audio_t *m, int id,
                         const void *pcm, size_t bytes, int bits, int channels,
                         int rate, int vol)
{
	char fn[24];

	if (m == NULL || m->tier < 1 || id < 0 || id >= KEYMAX || bytes == 0)
		return;
	if (m->music_id == id)                 // already playing this track
		return;
	snprintf(fn, sizeof(fn), "m/%d", id);

	if (!m->mus_up[id]) {
		send_buf(m, termgfx_audio_cache_pcm(&m->buf, &m->cap, fn,
		                                    pcm, bytes, bits, channels, rate));
		m->mus_up[id] = 1;
	}
	send_buf(m, termgfx_audio_flush(&m->buf, &m->cap, MUSIC_CH, 200));
	send_buf(m, termgfx_audio_load(&m->buf, &m->cap, MUSIC_SLOT, fn));
	send_buf(m, termgfx_audio_queue(&m->buf, &m->cap, MUSIC_CH, MUSIC_SLOT, vol, 0, 1));
	m->music_id = id;
}

void termgfx_audio_music_stop(termgfx_audio_t *m)
{
	if (m == NULL || m->tier < 1)
		return;
	send_buf(m, termgfx_audio_flush(&m->buf, &m->cap, MUSIC_CH, 200));
	m->music_id = -1;
}
