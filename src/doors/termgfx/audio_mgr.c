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
#define SFX_CH_LO    3               // one-shot SFX: round-robin channels 3..11
#define SFX_CH_HI    11
#define NSFX_CH      (SFX_CH_HI - SFX_CH_LO + 1)
#define LOOP_CH_LO   12              // looping SFX (ambience): dedicated channels 12..15
#define LOOP_CH_HI   15
#define NLOOP_CH     (LOOP_CH_HI - LOOP_CH_LO + 1)
#define MUSNAME_MAX  24              // cache name "m/<track>" component length
#define MUS_TRACKS   64             // distinct music tracks tracked per session
#define MUS_OGG_Q    0.3            // Vorbis VBR quality for OPL FM music (small, clean)

struct termgfx_audio {
	termgfx_audio_emit_fn emit;
	void *ctx;
	int tier;                              // -1 unknown/none, 0 tone, 1 digital

	uint8_t sfx_up[KEYMAX];                // id has been C;S-Stored (sfx file)
	char mus_names[MUS_TRACKS][MUSNAME_MAX];     // music tracks Stored this session
	int mus_count;
	int next_slot;                         // round-robin SFX slot cursor
	int next_ch;                           // round-robin SFX channel cursor
	char music_name[MUSNAME_MAX];          // track in the music slot ("" = none)

	struct { int handle; } loop[NLOOP_CH]; // looping voices: loop[i] is on LOOP_CH_LO+i,
	int loop_seq;                          // .handle 0 = free; loop_seq mints unique handles

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
	return m;
}

void termgfx_audio_destroy(termgfx_audio_t *m)
{
	if (m == NULL)
		return;
	free(m->buf);
	free(m);
}

// Send one builder's output (n bytes already in m->buf) to the door on `stream`
// (TERMGFX_AUDIO_PRIO for SFX/control, TERMGFX_AUDIO_BULK for a music upload).
static void send_buf(termgfx_audio_t *m, size_t n, int stream)
{
	if (n > 0)
		m->emit(m->ctx, m->buf, n, stream);
}

// ---- capability probe ----------------------------------------------------

void termgfx_audio_probe(termgfx_audio_t *m)
{
	if (m == NULL)
		return;
	m->emit(m->ctx, termgfx_audio_query, strlen(termgfx_audio_query), TERMGFX_AUDIO_PRIO);
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
	send_buf(m, termgfx_audio_load(&m->buf, &m->cap, slot, fn), TERMGFX_AUDIO_PRIO);
	send_buf(m, termgfx_audio_queue(&m->buf, &m->cap, ch, slot, vol, pan, 0), TERMGFX_AUDIO_PRIO);
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
		                                    pcm, bytes, bits, channels, rate),
		         TERMGFX_AUDIO_PRIO);
		m->sfx_up[id] = 1;
	}
	sfx_dispatch(m, fn, vol, pan);
}

// C;S-Store the sound file `filedata` under `fn` once per id. libsndfile rejects
// some of Duke's multi-block VOCs ("incompatible VOC sections") -> silent SFX, so a
// VOC is transcoded to a clean 8-bit WAV; anything else (WAV/OGG/...) ships verbatim
// (content-sniffed). Shared by one-shot SFX and looping SFX.
static void sfx_cache_file_once(termgfx_audio_t *m, int id,
                                const void *filedata, size_t filelen, const char *fn)
{
	uint8_t *pcm = NULL;
	int      rate = 0;
	size_t   n;

	if (m->sfx_up[id])
		return;
	n = termgfx_audio_voc_to_pcm(filedata, filelen, &pcm, &rate);
	if (n > 0) {
		send_buf(m, termgfx_audio_cache_pcm(&m->buf, &m->cap, fn, pcm, n, 8, 1, rate),
		         TERMGFX_AUDIO_PRIO);
		free(pcm);
	}
	else {
		send_buf(m, termgfx_audio_cache_file(&m->buf, &m->cap, fn, filedata, filelen),
		         TERMGFX_AUDIO_PRIO);
	}
	m->sfx_up[id] = 1;
}

void termgfx_audio_sfx_file(termgfx_audio_t *m, int id,
                            const void *filedata, size_t filelen, int vol, int pan)
{
	char fn[24];

	if (m == NULL || m->tier < 1 || id < 0 || id >= KEYMAX || filelen == 0)
		return;
	snprintf(fn, sizeof(fn), "a/%d", id);
	sfx_cache_file_once(m, id, filedata, filelen, fn);
	sfx_dispatch(m, fn, vol, pan);
}

// ---- looping SFX (ambience) ----------------------------------------------

int termgfx_audio_loop_start(termgfx_audio_t *m, int id,
                             const void *filedata, size_t filelen, int vol)
{
	char fn[24];
	int  i, slot;

	if (m == NULL || m->tier < 1 || id < 0 || id >= KEYMAX || filelen == 0)
		return 0;

	// Pick a free looping channel; if all NLOOP_CH are busy, steal slot 0.
	for (i = 0; i < NLOOP_CH && m->loop[i].handle != 0; i++)
		;
	if (i == NLOOP_CH) {
		send_buf(m, termgfx_audio_flush(&m->buf, &m->cap, LOOP_CH_LO, 0), TERMGFX_AUDIO_PRIO);
		i = 0;
	}

	snprintf(fn, sizeof(fn), "a/%d", id);
	sfx_cache_file_once(m, id, filedata, filelen, fn);

	// Load a transient slot, then Queue it LOOPED on this channel's dedicated slot.
	slot         = m->next_slot;
	m->next_slot = SFX_SLOT_LO + ((m->next_slot - SFX_SLOT_LO + 1) % (NSLOT - SFX_SLOT_LO));
	send_buf(m, termgfx_audio_load(&m->buf, &m->cap, slot, fn), TERMGFX_AUDIO_PRIO);
	send_buf(m, termgfx_audio_queue(&m->buf, &m->cap, LOOP_CH_LO + i, slot, vol, 0, 1),
	         TERMGFX_AUDIO_PRIO);

	if (++m->loop_seq <= 0)
		m->loop_seq = 1;
	m->loop[i].handle = m->loop_seq;
	return m->loop_seq;
}

void termgfx_audio_loop_stop(termgfx_audio_t *m, int handle)
{
	int i;

	if (m == NULL || m->tier < 1 || handle <= 0)
		return;
	for (i = 0; i < NLOOP_CH; i++) {
		if (m->loop[i].handle == handle) {
			send_buf(m, termgfx_audio_flush(&m->buf, &m->cap, LOOP_CH_LO + i, 0),
			         TERMGFX_AUDIO_PRIO);
			m->loop[i].handle = 0;
			return;
		}
	}
}

// ---- music ---------------------------------------------------------------

// Track name -> a cache-safe component: keep [A-Za-z0-9._-], map the rest to '_'.
// The name comes from the game (a DOS 8.3 MIDI name / Doom MUS lump) so this is
// belt-and-suspenders against an odd byte landing in a cache path.
static void mus_sanitize(char *dst, size_t dsz, const char *src)
{
	size_t i;

	for (i = 0; i + 1 < dsz && src[i] != '\0'; i++) {
		char c = src[i];
		dst[i] = ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		          (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_')
		         ? c : '_';
	}
	dst[i] = '\0';
}

// Has track `safe` been C;S-Stored this session? (upload-once)
static int mus_stored(termgfx_audio_t *m, const char *safe)
{
	int i;

	for (i = 0; i < m->mus_count; i++)
		if (strcmp(m->mus_names[i], safe) == 0)
			return 1;
	return 0;
}

static void mus_remember(termgfx_audio_t *m, const char *safe)
{
	if (m->mus_count < MUS_TRACKS) {
		strncpy(m->mus_names[m->mus_count], safe, MUSNAME_MAX - 1);
		m->mus_names[m->mus_count][MUSNAME_MAX - 1] = '\0';
		m->mus_count++;
	}
}

void termgfx_audio_music(termgfx_audio_t *m, const char *name,
                         const void *pcm, size_t bytes, int bits, int channels,
                         int rate, int vol)
{
	char  safe[MUSNAME_MAX];
	char  fn[MUSNAME_MAX + 8];
	char *dot;
	int   use_ogg;

	if (m == NULL || m->tier < 1 || name == NULL || name[0] == '\0' || bytes == 0)
		return;
	mus_sanitize(safe, sizeof(safe), name);
	// Drop the SOURCE extension (".MID"/".MUS"/...) -- the cache name carries the
	// ENCODER's format instead (below). That makes each entry self-describing AND
	// changes the name whenever the encoder changes, so a format switch writes a
	// fresh cache entry rather than silently reusing a stale one of the old format
	// (and a future C;L re-upload skip stays format-correct). SyncTERM decodes by
	// content, not extension, so the name is for our bookkeeping only.
	dot = strrchr(safe, '.');
	if (dot != NULL && dot != safe)
		*dot = '\0';
	if (strcmp(m->music_name, safe) == 0)  // already playing this track
		return;
	// OGG/Vorbis when available (10-15x smaller upload + on-disk cache); raw-PCM
	// WAV otherwise. SyncTERM decodes both via libsndfile.
	use_ogg = (bits == 16 && channels > 0 && termgfx_audio_have_ogg());
	snprintf(fn, sizeof(fn), "m/%s.%s", safe, use_ogg ? "ogg" : "wav");

	if (!mus_stored(m, safe)) {            // Store the track once (content-addressed)
		size_t n = 0;

		if (use_ogg)
			n = termgfx_audio_cache_ogg(&m->buf, &m->cap, fn, (const int16_t *)pcm,
			                            bytes / ((size_t)channels * 2), channels,
			                            rate, MUS_OGG_Q);
		if (n == 0)
			n = termgfx_audio_cache_pcm(&m->buf, &m->cap, fn,
			                            pcm, bytes, bits, channels, rate);
		send_buf(m, n, TERMGFX_AUDIO_BULK);
		mus_remember(m, safe);
	}
	// HARD clear (fade 0), not a fade: SyncTERM's Flush with a fade-out only
	// overlays a transient decay on the LOOPING head buffer -- the loop survives
	// and resumes -- whereas a no-fade Flush calls xp_audio_clear() and truly stops
	// it. Queue *appends*, so the old loop must be cleared before the new track or
	// the two stack.
	// All on the BULK stream, in order, so the small Load/Queue stay behind the
	// track's own C;S upload (the file must be Stored before it's Loaded) and never
	// race ahead of it on the priority stream. Queue at unity (100) and set the
	// channel's base volume separately, so a live music-volume change (Volume verb,
	// channel base) doesn't multiply with a per-buf Queue level.
	send_buf(m, termgfx_audio_flush(&m->buf, &m->cap, MUSIC_CH, 0), TERMGFX_AUDIO_BULK);
	send_buf(m, termgfx_audio_load(&m->buf, &m->cap, MUSIC_SLOT, fn), TERMGFX_AUDIO_BULK);
	send_buf(m, termgfx_audio_queue(&m->buf, &m->cap, MUSIC_CH, MUSIC_SLOT, 100, 0, 1), TERMGFX_AUDIO_BULK);
	send_buf(m, termgfx_audio_volume(&m->buf, &m->cap, MUSIC_CH, vol), TERMGFX_AUDIO_BULK);
	strncpy(m->music_name, safe, sizeof(m->music_name) - 1);
	m->music_name[sizeof(m->music_name) - 1] = '\0';
}

void termgfx_audio_music_stop(termgfx_audio_t *m)
{
	if (m == NULL || m->tier < 1)
		return;
	// Hard clear -- a faded Flush leaves a looping head playing (see the note in
	// termgfx_audio_music); on door exit we want the loop definitively gone. On the
	// PRIO stream so it stops promptly (menu MUSIC:OFF / exit), ahead of any frames.
	send_buf(m, termgfx_audio_flush(&m->buf, &m->cap, MUSIC_CH, 0), TERMGFX_AUDIO_PRIO);
	m->music_name[0] = '\0';
}

void termgfx_audio_music_volume(termgfx_audio_t *m, int vol)
{
	if (m == NULL || m->tier < 1)
		return;
	send_buf(m, termgfx_audio_volume(&m->buf, &m->cap, MUSIC_CH, vol), TERMGFX_AUDIO_PRIO);
}

void termgfx_audio_sfx_stop_all(termgfx_audio_t *m)
{
	int ch, i;

	if (m == NULL || m->tier < 1)
		return;
	for (ch = SFX_CH_LO; ch <= LOOP_CH_HI; ch++)   // one-shot SFX + looping channels
		send_buf(m, termgfx_audio_flush(&m->buf, &m->cap, ch, 0), TERMGFX_AUDIO_PRIO);
	for (i = 0; i < NLOOP_CH; i++)                  // free the looping voices
		m->loop[i].handle = 0;
}
