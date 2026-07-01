// audio_mgr.c -- stateful policy over audio.h's SyncTERM audio-APC builders.
// See audio_mgr.h. Tracks the capability tier, an upload-once sample cache, and
// round-robin SFX slot/channel cursors; emits through the door-supplied
// callback. I/O-free apart from that callback.

#include "audio_mgr.h"
#include "audio.h"
#include "audio_midi.h"   /* termgfx_midi_render: the worker renders MIDI/MUS -> PCM */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #include <process.h>   /* _getpid: unique temp-file suffix for the atomic cache write */
  #define TG_GETPID _getpid
#else
  #include <unistd.h>    /* getpid */
  #define TG_GETPID getpid
  #include <pthread.h>   /* transcode music on a worker thread so a first-play render doesn't
                          * freeze the game (POSIX; Windows falls back to the synchronous path). */
  #define TERMGFX_ASYNC_MUSIC 1
#endif

#define CL_DATA_MAX 262144u             /* cap the captured C;L reply (a runaway/unterminated
	                                     * one won't grow the buffer without bound) */

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
#define MUSNAME_MAX  32              // cache name "m/<track>_v<n>" component length
#define MUS_TRACKS   64             // distinct music tracks tracked per session
#define MUS_OGG_Q    0.3            // Vorbis VBR quality for OPL FM music (small, clean)
#define MUS_RENDER_VER 1            // bump when the synth/bank/normalisation changes:
                                    // the cache key carries it (<name>_v<n>), so every
                                    // stale door-side + client cache entry is bypassed
#define MUS_CACHE_DIR_MAX 240       // door-side OGG cache directory path

struct termgfx_audio {
	termgfx_audio_emit_fn emit;
	void *ctx;
	int tier;                              // -1 unknown/none, 0 tone, 1 digital

	uint8_t sfx_up[KEYMAX];                // id has been C;S-Stored (sfx file)
	char mus_names[MUS_TRACKS][MUSNAME_MAX];     // music tracks Stored this session
	int mus_count;
	int next_slot;                         // round-robin SFX slot cursor
	int next_ch;                           // round-robin SFX channel cursor
	char music_name[MUSNAME_MAX];          // cache key (with _v<n>) in the music slot ("" = none)
	char music_cache_dir[MUS_CACHE_DIR_MAX]; // door-side OGG disk cache ("" = none, render-only)
	char cache_prefix[24];                 // consumer/door name -> SyncTERM cache "<prefix>/music|sfx/.."

	struct { int handle; int vl, vr; } loop[NLOOP_CH]; // looping voices: loop[i] on LOOP_CH_LO+i;
	int loop_seq;                          // .handle 0 = free; loop_seq mints unique handles
	                                       // .vl/.vr = last L/R volume sent (skip redundant updates)

	uint8_t acc[32];                       // rolling window for the probe reply
	int acclen;

	// C;L client-cache query (cross-session upload skip): at tier-ready we ask the
	// client which music files it already holds on its persistent disk cache, then
	// skip the (re-)upload for any whose content-addressed name is in the reply.
	int cl_state;                          // 0 idle/done, 1 matching reply marker, 2 capturing
	int cl_match;                          // start-marker bytes matched so far (state 1)
	int cl_esc;                            // saw ESC mid-payload, awaiting ST '\' (state 2)
	int cl_done;                           // reply fully captured -> cl_data is valid
	char *cl_data;                         // captured "<path>\t<md5>" lines
	size_t cl_len, cl_cap;

	uint8_t *buf;                          // scratch for builders
	size_t cap;

#ifdef TERMGFX_ASYNC_MUSIC
	// ---- async music transcode (off the game thread) -------------------------------------------
	// A worker thread renders MIDI->PCM, encodes OGG, writes the disk cache, AND pre-builds the C;S
	// upload APC -- all the CPU -- off the game thread.  The main thread only memcpy-stages the
	// ready-to-send bytes (out_flush drains them over frames) and ships the loop.  Latest-wins: a
	// new submit supersedes an in-flight render via the generation token.
	pthread_t mus_thr;                     // worker thread
	int mus_thr_up;                        // 1 once created (join on destroy)
	pthread_mutex_t mus_lock;              // guards the job + result slots
	pthread_cond_t mus_wake;               // main -> worker: a job is pending, or stop
	int mus_stop;                          // destroy: worker should exit
	unsigned mus_gen;                      // bumped per submit (latest-wins supersede token)
	// job (main -> worker): the newest submitted track
	uint8_t * mus_job;                     // copied MIDI/MUS bytes (worker frees after render)
	size_t mus_job_len;
	int mus_job_rate, mus_job_vol;
	char mus_job_key[MUSNAME_MAX];
	int mus_job_pending;
	// result (worker -> main): the pre-built C;S upload APC for the finished track
	uint8_t * mus_res;                     // ready-to-send APC bytes (main frees after emit)
	size_t mus_res_len;
	int mus_res_vol;
	char mus_res_key[MUSNAME_MAX];
	unsigned mus_res_gen;                  // the gen this result was produced for
	int mus_res_ready;
#endif
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
#ifdef TERMGFX_ASYNC_MUSIC
	pthread_mutex_init(&m->mus_lock, NULL);
	pthread_cond_init(&m->mus_wake, NULL);        // worker is created lazily on the first submit
#endif
	return m;
}

void termgfx_audio_destroy(termgfx_audio_t *m)
{
	if (m == NULL)
		return;
#ifdef TERMGFX_ASYNC_MUSIC
	if (m->mus_thr_up) {
		pthread_mutex_lock(&m->mus_lock);
		m->mus_stop = 1;
		pthread_cond_signal(&m->mus_wake);
		pthread_mutex_unlock(&m->mus_lock);
		pthread_join(m->mus_thr, NULL);
	}
	pthread_mutex_destroy(&m->mus_lock);
	pthread_cond_destroy(&m->mus_wake);
	free(m->mus_job);
	free(m->mus_res);
#endif
	free(m->cl_data);
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

// Build a SyncTERM client-cache name "<prefix>/<sub>/<leaf>" (prefix omitted if the
// consumer didn't set one). `sub` is "music" or "sfx". Namespacing by consumer (the
// door name) keeps two doors' files distinct + attributable in SyncTERM's single
// per-BBS cache dir, and the descriptive subdir beats a terse "m"/"a".
static void cache_name(termgfx_audio_t *m, const char *sub, const char *leaf,
                       char *out, size_t sz)
{
	if (m->cache_prefix[0] != '\0')
		snprintf(out, sz, "%s/%s/%s", m->cache_prefix, sub, leaf);
	else
		snprintf(out, sz, "%s/%s", sub, leaf);
}

void termgfx_audio_set_cache_prefix(termgfx_audio_t *m, const char *name)
{
	if (m == NULL)
		return;
	if (name == NULL)
		name = "";
	strncpy(m->cache_prefix, name, sizeof(m->cache_prefix) - 1);
	m->cache_prefix[sizeof(m->cache_prefix) - 1] = '\0';
}

// ---- capability probe ----------------------------------------------------

void termgfx_audio_probe(termgfx_audio_t *m)
{
	if (m == NULL)
		return;
	m->emit(m->ctx, termgfx_audio_query, strlen(termgfx_audio_query), TERMGFX_AUDIO_PRIO);
}

// ---- C;L client-cache query (cross-session upload skip) -------------------
// SyncTERM persists C;S'd files on disk per-BBS; "SyncTERM:C;L;<glob>" lists them
// (reply: ESC _ SyncTERM:C;L \n  <path>\t<md5> ...  ESC \). At tier-ready we ask
// which music files the client already holds, then skip the (re-)upload of any
// whose content-addressed name is present -- the client Loads it from its own
// cache, no Store. Mirrors the zmachine's v6cacheList (name-presence, not MD5,
// since our names are content-addressed). Best-effort: a client that ignores C;L
// just never matches, so we upload as before.
static const char CL_MARK[] = "\x1b_SyncTERM:C;L\n";
#define CL_MARK_LEN (sizeof(CL_MARK) - 1)

static void cl_query(termgfx_audio_t *m)
{
	char q[64];
	int  n;

	if (m->cache_prefix[0] != '\0')
		n = snprintf(q, sizeof(q), "\x1b_SyncTERM:C;L;%s/music/*\x1b\\", m->cache_prefix);
	else
		n = snprintf(q, sizeof(q), "\x1b_SyncTERM:C;L;music/*\x1b\\");
	if (n > 0)
		m->emit(m->ctx, q, (size_t)n, TERMGFX_AUDIO_PRIO);
	m->cl_state = 1;                       // start scanning inbound bytes for the reply
	m->cl_match = 0;
}

static void cl_append(termgfx_audio_t *m, uint8_t b)
{
	if (m->cl_len >= CL_DATA_MAX)
		return;                        // reply larger than any real cache list -> stop growing
	if (m->cl_len + 1 > m->cl_cap) {
		size_t nc = m->cl_cap ? m->cl_cap * 2 : 256;
		char * nb = realloc(m->cl_data, nc);

		if (nb == NULL)
			return;                        // OOM: drop -> just fewer skips, never wrong
		m->cl_data = nb;
		m->cl_cap  = nc;
	}
	m->cl_data[m->cl_len++] = (char)b;
}

// Incrementally scan the inbound stream for the C;L reply frame and capture its
// payload. Tolerant of other bytes (key input, DSR acks) interleaved and of the
// frame splitting across feeds; one-shot (stops once the ST terminator arrives).
static void cl_feed(termgfx_audio_t *m, const uint8_t *buf, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		uint8_t b = buf[i];

		if (m->cl_state == 1) {                              // matching the start marker
			if (b == (uint8_t)CL_MARK[m->cl_match]) {
				if (++m->cl_match == (int)CL_MARK_LEN) {
					m->cl_state = 2;                         // marker done -> capture payload
					m->cl_esc   = 0;
				}
			}
			else {
				m->cl_match = (b == (uint8_t)CL_MARK[0]) ? 1 : 0;
			}
		}
		else if (m->cl_state == 2) {                         // capturing until ST (ESC '\')
			if (m->cl_esc) {
				if (b == '\\') {
					m->cl_done  = 1;
					m->cl_state = 0;
					return;                                  // frame complete
				}
				cl_append(m, 0x1b);                          // lone ESC in the list: keep both
				cl_append(m, b);
				m->cl_esc = 0;
			}
			else if (b == 0x1b) {
				m->cl_esc = 1;
			}
			else {
				cl_append(m, b);
			}
		}
	}
}

// 1 if the captured C;L list contains `base` as a whole filename (a path component
// at the end of an entry -- preceded by '/' or line start, followed by TAB/EOL).
static int cl_has(termgfx_audio_t *m, const char *base)
{
	size_t bl = strlen(base), i;

	if (!m->cl_done || m->cl_data == NULL || bl == 0)
		return 0;
	for (i = 0; i + bl <= m->cl_len; i++) {
		if (memcmp(m->cl_data + i, base, bl) != 0)
			continue;
		if (i != 0 && m->cl_data[i - 1] != '/' && m->cl_data[i - 1] != '\n')
			continue;
		if (i + bl == m->cl_len || m->cl_data[i + bl] == '\t' || m->cl_data[i + bl] == '\n')
			return 1;
	}
	return 0;
}

void termgfx_audio_feed(termgfx_audio_t *m, const uint8_t *buf, int len)
{
	int r;

	if (m == NULL || len <= 0)
		return;
	if (m->tier < 0) {
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
		if (r >= 0) {
			m->tier = r;
			if (r >= 1)
				cl_query(m);             // digital tier -> ask what the client already caches
		}
		return;
	}
	if (m->cl_state != 0)                    // tier known, C;L reply still expected -> capture
		cl_feed(m, buf, len);
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
	char fn[48];

	if (m == NULL || m->tier < 1 || id < 0 || id >= KEYMAX || bytes == 0)
		return;
	{
		char leaf[16];
		snprintf(leaf, sizeof(leaf), "%d", id);
		cache_name(m, "sfx", leaf, fn, sizeof(fn));   // "<prefix>/sfx/<id>"
	}

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
	char fn[48];

	if (m == NULL || m->tier < 1 || id < 0 || id >= KEYMAX || filelen == 0)
		return;
	{
		char leaf[16];
		snprintf(leaf, sizeof(leaf), "%d", id);
		cache_name(m, "sfx", leaf, fn, sizeof(fn));   // "<prefix>/sfx/<id>"
	}
	sfx_cache_file_once(m, id, filedata, filelen, fn);
	sfx_dispatch(m, fn, vol, pan);
}

// ---- looping SFX (ambience) ----------------------------------------------

int termgfx_audio_loop_start(termgfx_audio_t *m, int id,
                             const void *filedata, size_t filelen, int vol)
{
	char fn[48];
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

	{
		char leaf[16];
		snprintf(leaf, sizeof(leaf), "%d", id);
		cache_name(m, "sfx", leaf, fn, sizeof(fn));   // "<prefix>/sfx/<id>"
	}
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
	m->loop[i].vl     = vol;        // started centered; FX_Pan3D pans it next frame
	m->loop[i].vr     = vol;
	return m->loop_seq;
}

void termgfx_audio_loop_volume(termgfx_audio_t *m, int handle, int vol, int pan)
{
	int i, vl, vr;

	if (m == NULL || m->tier < 1 || handle <= 0)
		return;
	if (vol < 0)
		vol = 0;
	else if (vol > 100)
		vol = 100;
	if (pan < -100)
		pan = -100;
	else if (pan > 100)
		pan = 100;
	// Side toward the source stays at full `vol`; the opposite side attenuates --
	// matching the original audiolib pan law (and termgfx_audio_queue's VL/VR).
	vl = (pan > 0) ? vol * (100 - pan) / 100 : vol;
	vr = (pan < 0) ? vol * (100 + pan) / 100 : vol;
	for (i = 0; i < NLOOP_CH; i++) {
		if (m->loop[i].handle == handle) {
			if (vl == m->loop[i].vl && vr == m->loop[i].vr)   // unchanged -- don't flood APC
				return;
			m->loop[i].vl = vl;
			m->loop[i].vr = vr;
			send_buf(m, termgfx_audio_volume_lr(&m->buf, &m->cap, LOOP_CH_LO + i, vl, vr),
			         TERMGFX_AUDIO_PRIO);
			return;
		}
	}
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

// Build the music cache KEY from a track name: sanitise, drop the source ext, append
// the render-version tag. The key identifies the track's CONTENT + how we render it
// (the door passes a source-hash name like d_<hash>), so it's stable cross-session
// yet changes if the synth changes -- name-presence in the door-side disk cache (or,
// once wired, the client's C;L list) means "the right bytes", no MD5 needed.
static void mus_key(const char *name, char *key, size_t keysz)
{
	char  safe[MUSNAME_MAX];
	char *dot;

	mus_sanitize(safe, sizeof(safe), name);
	dot = strrchr(safe, '.');
	if (dot != NULL && dot != safe)
		*dot = '\0';
	snprintf(key, keysz, "%s_v%d", safe, MUS_RENDER_VER);
}

// Door-side OGG cache path "<dir>/<key>.ogg"; 0 if no cache dir configured.
static int mus_disk_path(termgfx_audio_t *m, const char *key, char *path, size_t sz)
{
	if (m->music_cache_dir[0] == '\0')
		return 0;
	snprintf(path, sz, "%s/%s.ogg", m->music_cache_dir, key);
	return 1;
}

// Read a whole file into a malloc'd buffer (*out, caller frees); 0 on any failure.
static size_t mus_read_file(const char *path, uint8_t **out)
{
	FILE *   f;
	long     sz;
	uint8_t *b;

	*out = NULL;
	f = fopen(path, "rb");
	if (f == NULL)
		return 0;
	if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) <= 0) {
		fclose(f);
		return 0;
	}
	rewind(f);
	b = malloc((size_t)sz);
	if (b == NULL) {
		fclose(f);
		return 0;
	}
	if (fread(b, 1, (size_t)sz, f) != (size_t)sz) {
		free(b);
		fclose(f);
		return 0;
	}
	fclose(f);
	*out = b;
	return (size_t)sz;
}

// Best-effort write of the encoded OGG to the door-side cache. Write to a per-pid
// temp then atomically move it into place: two nodes rendering the same track at
// once (the cache is content-addressed, so same file), or a write interrupted by a
// full disk / killed door, must never leave a truncated or interleaved OGG that a
// later read would ship to a client as corrupt (= a silently-silent track). A miss
// is harmless -- the track just re-renders next time.
static void mus_write_file(const char *path, const uint8_t *data, size_t len)
{
	char  tmp[MUS_CACHE_DIR_MAX + MUSNAME_MAX + 32];
	FILE *f;

	snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", path, (long)TG_GETPID());
	f = fopen(tmp, "wb");
	if (f == NULL)
		return;
	if (fwrite(data, 1, len, f) != len || fclose(f) != 0) {   // partial write -> discard
		remove(tmp);
		return;
	}
#ifdef _WIN32
	remove(path);                      // Windows rename() won't replace an existing target
#endif
	if (rename(tmp, path) != 0)        // atomic on POSIX; a complete file either way
		remove(tmp);
}

// Flush the music channel, Load the (already-Stored) cache file, loop it, set volume.
// HARD clear (fade 0), not a fade: SyncTERM's Flush with a fade-out only overlays a
// transient decay on the LOOPING head buffer (the loop survives + resumes); a no-fade
// Flush calls xp_audio_clear() and truly stops it. Queue *appends*, so the old loop
// must be cleared before the new track or the two stack. All on the BULK stream, in
// order, so the small Load/Queue stay behind the track's C;S upload (Store-before-Load)
// and never race ahead of it. Queue at unity (100) + set the channel base volume
// separately, so a live music-volume change doesn't multiply with a per-buf level.
static void mus_ship(termgfx_audio_t *m, const char *cachefn, int vol)
{
	send_buf(m, termgfx_audio_flush(&m->buf, &m->cap, MUSIC_CH, 0), TERMGFX_AUDIO_BULK);
	send_buf(m, termgfx_audio_load(&m->buf, &m->cap, MUSIC_SLOT, cachefn), TERMGFX_AUDIO_BULK);
	send_buf(m, termgfx_audio_queue(&m->buf, &m->cap, MUSIC_CH, MUSIC_SLOT, 100, 0, 1), TERMGFX_AUDIO_BULK);
	send_buf(m, termgfx_audio_volume(&m->buf, &m->cap, MUSIC_CH, vol), TERMGFX_AUDIO_BULK);
}

void termgfx_audio_set_music_cache_dir(termgfx_audio_t *m, const char *dir)
{
	if (m == NULL)
		return;
	if (dir == NULL)
		dir = "";
	strncpy(m->music_cache_dir, dir, sizeof(m->music_cache_dir) - 1);
	m->music_cache_dir[sizeof(m->music_cache_dir) - 1] = '\0';
}

// Play a track that's available WITHOUT rendering -- from the door-side OGG disk
// cache (and, once C;L is wired, the client's own persistent cache). Returns 1 if it
// shipped the track (or it's already playing); 0 if the door must render + supply via
// termgfx_audio_music(). Lets the door skip the expensive MIDI render on a cache hit.
int termgfx_audio_music_play(termgfx_audio_t *m, const char *name, int vol)
{
	char     key[MUSNAME_MAX];
	char     leaf[MUSNAME_MAX + 8];
	char     cachefn[96];
	char     path[MUS_CACHE_DIR_MAX + MUSNAME_MAX + 8];
	uint8_t *ogg;
	size_t   ogglen;

	if (m == NULL || m->tier < 1 || name == NULL || name[0] == '\0')
		return 0;
	mus_key(name, key, sizeof(key));
	if (strcmp(m->music_name, key) == 0)        // already playing this track
		return TERMGFX_MUSIC_CACHED;
	snprintf(leaf, sizeof(leaf), "%s.ogg", key);
	cache_name(m, "music", leaf, cachefn, sizeof(cachefn));   // "<prefix>/music/<key>.ogg"

	// STATE 1 -- the client's persistent cache already holds this OGG (C;L name hit):
	// Load it straight from there, NO Store/upload, NO disk read, NO render.
	if (cl_has(m, leaf)) {                       // match the "<key>.ogg" basename
		mus_remember(m, key);                   // session bookkeeping (no Store needed)
		mus_ship(m, cachefn, vol);
		strncpy(m->music_name, key, sizeof(m->music_name) - 1);
		m->music_name[sizeof(m->music_name) - 1] = '\0';
		return TERMGFX_MUSIC_CLIENT;            // played with zero upload
	}

	// STATE 2 -- the door-side disk cache has the OGG: upload it (Store) + ship, no render.
	if (!mus_disk_path(m, key, path, sizeof(path)))
		return TERMGFX_MUSIC_RENDER;            // no disk cache -> door must render
	ogglen = mus_read_file(path, &ogg);
	if (ogglen == 0)
		return TERMGFX_MUSIC_RENDER;            // not on disk -> door must render
	if (!mus_stored(m, key)) {                  // upload the cached OGG once this session
		send_buf(m, termgfx_audio_cache_file(&m->buf, &m->cap, cachefn, ogg, ogglen),
		         TERMGFX_AUDIO_BULK);
		mus_remember(m, key);
	}
	free(ogg);
	mus_ship(m, cachefn, vol);
	strncpy(m->music_name, key, sizeof(m->music_name) - 1);
	m->music_name[sizeof(m->music_name) - 1] = '\0';
	return TERMGFX_MUSIC_CACHED;                // played from the door-side disk cache
}

void termgfx_audio_music(termgfx_audio_t *m, const char *name,
                         const void *pcm, size_t bytes, int bits, int channels,
                         int rate, int vol)
{
	char key[MUSNAME_MAX];
	char leaf[MUSNAME_MAX + 8];
	char cachefn[96];
	char path[MUS_CACHE_DIR_MAX + MUSNAME_MAX + 8];
	int  use_ogg;

	if (m == NULL || m->tier < 1 || name == NULL || name[0] == '\0' || bytes == 0)
		return;
	mus_key(name, key, sizeof(key));
	if (strcmp(m->music_name, key) == 0)        // already playing this track
		return;
	// OGG/Vorbis when available (10-15x smaller upload + a door-side disk cache so the
	// next play -- any session -- skips the render); raw-PCM WAV otherwise (no caching).
	use_ogg = (bits == 16 && channels > 0 && termgfx_audio_have_ogg());
	snprintf(leaf, sizeof(leaf), "%s.%s", key, use_ogg ? "ogg" : "wav");
	cache_name(m, "music", leaf, cachefn, sizeof(cachefn));   // "<prefix>/music/<key>.<ext>"

	if (!mus_stored(m, key)) {                  // Store the track once this session
		size_t n = 0;

		if (use_ogg) {
			uint8_t *ogg    = NULL;
			size_t   ogglen = termgfx_audio_encode_ogg((const int16_t *)pcm,
			                                           bytes / ((size_t)channels * 2), channels, rate,
			                                           MUS_OGG_Q, &ogg);
			if (ogglen > 0) {
				if (mus_disk_path(m, key, path, sizeof(path)))
					mus_write_file(path, ogg, ogglen);   // populate the door-side cache
				n = termgfx_audio_cache_file(&m->buf, &m->cap, cachefn, ogg, ogglen);
			}
			free(ogg);
		}
		if (n == 0)                              // no libsndfile / encode failed -> raw WAV
			n = termgfx_audio_cache_pcm(&m->buf, &m->cap, cachefn,
			                            pcm, bytes, bits, channels, rate);
		send_buf(m, n, TERMGFX_AUDIO_BULK);
		mus_remember(m, key);
	}
	mus_ship(m, cachefn, vol);
	strncpy(m->music_name, key, sizeof(m->music_name) - 1);
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

// ---- async music transcode -----------------------------------------------------------------------
// A first-play track has to render MIDI->OPL PCM (~seconds) then encode OGG -- expensive, and if done
// inline it freezes the game.  These two calls move that work to a worker thread: the door SUBMITs
// the raw music bytes and POLLs each frame; the game keeps running and the track fades in when ready.
// Cache hits (termgfx_audio_music_play) still play instantly on the main thread -- this is only for a
// cold miss.
#ifdef TERMGFX_ASYNC_MUSIC

// Worker half (off the game thread): render -> encode -> write the disk cache -> pre-build the C;S
// upload APC into `apc`.  Touches only immutable manager fields (cache dir/prefix) + local buffers,
// never m->emit / m->buf / the socket / the session lists, so it needs no lock.  Returns APC length
// (0 = no encoder or a failure -> nothing shipped).
static size_t mus_transcode(termgfx_audio_t *m, const char *key, const void *midi, size_t len,
                            int rate, uint8_t **apc, size_t *apccap)
{
	int16_t *pcm     = NULL;
	size_t   nframes = 0, ogglen, n;
	uint8_t *ogg     = NULL;
	char     leaf[MUSNAME_MAX + 8];
	char     cachefn[96];
	char     path[MUS_CACHE_DIR_MAX + MUSNAME_MAX + 8];

	if (!termgfx_audio_have_ogg())
		return 0;
	if (!termgfx_midi_render(midi, len, rate, 0, &pcm, &nframes) || nframes == 0)
		return 0;
	ogglen = termgfx_audio_encode_ogg(pcm, nframes, 2, rate, MUS_OGG_Q, &ogg);
	free(pcm);
	if (ogglen == 0) {
		free(ogg);
		return 0;
	}
	if (mus_disk_path(m, key, path, sizeof(path)))
		mus_write_file(path, ogg, ogglen);          // populate the disk cache (atomic)
	snprintf(leaf, sizeof(leaf), "%s.ogg", key);
	cache_name(m, "music", leaf, cachefn, sizeof(cachefn));
	n = termgfx_audio_cache_file(apc, apccap, cachefn, ogg, ogglen);   // build the Store APC
	free(ogg);
	return n;
}

// Main half: stage the finished upload (once per session), ship the loop, mark it playing.
static void mus_emit(termgfx_audio_t *m, const char *key, const uint8_t *apc, size_t apclen, int vol)
{
	char leaf[MUSNAME_MAX + 8];
	char cachefn[96];

	snprintf(leaf, sizeof(leaf), "%s.ogg", key);
	cache_name(m, "music", leaf, cachefn, sizeof(cachefn));
	if (!mus_stored(m, key)) {
		if (apclen > 0)
			m->emit(m->ctx, apc, apclen, TERMGFX_AUDIO_BULK);   // staged; out_flush drains over frames
		mus_remember(m, key);
	}
	mus_ship(m, cachefn, vol);
	strncpy(m->music_name, key, sizeof(m->music_name) - 1);
	m->music_name[sizeof(m->music_name) - 1] = '\0';
}

static void *mus_worker(void *arg)
{
	termgfx_audio_t *m      = arg;
	uint8_t *        apc    = NULL;      // reused across jobs
	size_t           apccap = 0;

	pthread_mutex_lock(&m->mus_lock);
	for (;;) {
		uint8_t *job;
		size_t   joblen, apclen;
		int      rate, vol;
		char     key[MUSNAME_MAX];
		unsigned gen;

		while (!m->mus_job_pending && !m->mus_stop)
			pthread_cond_wait(&m->mus_wake, &m->mus_lock);
		if (m->mus_stop)
			break;
		job    = m->mus_job; m->mus_job = NULL;            // take the newest job
		joblen = m->mus_job_len; rate = m->mus_job_rate; vol = m->mus_job_vol;
		memcpy(key, m->mus_job_key, sizeof(key));
		gen = m->mus_gen;
		m->mus_job_pending = 0;
		pthread_mutex_unlock(&m->mus_lock);

		apclen = mus_transcode(m, key, job, joblen, rate, &apc, &apccap);   // the long work
		free(job);

		pthread_mutex_lock(&m->mus_lock);
		if (gen == m->mus_gen && apclen > 0) {            // still the current track -> hand it over
			free(m->mus_res);
			m->mus_res     = apc;   m->mus_res_len = apclen;   // main frees the buffer after emit
			m->mus_res_vol = vol;   m->mus_res_gen = gen;
			memcpy(m->mus_res_key, key, sizeof(m->mus_res_key));
			m->mus_res_ready = 1;
			apc = NULL; apccap = 0;                       // ownership passed to the result slot
		}
		// else: superseded (or failed) -> keep `apc` as scratch, drop this result
	}
	pthread_mutex_unlock(&m->mus_lock);
	free(apc);
	return NULL;
}

void termgfx_audio_music_async_submit(termgfx_audio_t *m, const char *name,
                                      const void *music, size_t len, int rate, int vol)
{
	char     key[MUSNAME_MAX];
	uint8_t *copy;

	if (m == NULL || m->tier < 1 || name == NULL || name[0] == '\0' || music == NULL || len == 0)
		return;
	mus_key(name, key, sizeof(key));
	if (strcmp(m->music_name, key) == 0)             // already playing this track
		return;
	copy = malloc(len);
	if (copy == NULL)
		return;
	memcpy(copy, music, len);

	pthread_mutex_lock(&m->mus_lock);
	if (!m->mus_thr_up) {
		if (pthread_create(&m->mus_thr, NULL, mus_worker, m) == 0)
			m->mus_thr_up = 1;
		else {
			pthread_mutex_unlock(&m->mus_lock);
			free(copy);
			return;
		}
	}
	free(m->mus_job);                                // supersede a not-yet-taken job
	m->mus_job      = copy; m->mus_job_len = len;
	m->mus_job_rate = rate; m->mus_job_vol = vol;
	memcpy(m->mus_job_key, key, sizeof(m->mus_job_key));
	m->mus_job_pending = 1;
	m->mus_gen++;
	pthread_cond_signal(&m->mus_wake);
	pthread_mutex_unlock(&m->mus_lock);
}

int termgfx_audio_music_async_poll(termgfx_audio_t *m)
{
	uint8_t *apc    = NULL;
	size_t   apclen = 0;
	int      vol    = 0, ship = 0;
	char     key[MUSNAME_MAX];

	if (m == NULL || !m->mus_thr_up)
		return TERMGFX_MUSIC_ASYNC_IDLE;
	pthread_mutex_lock(&m->mus_lock);
	if (m->mus_res_ready) {
		m->mus_res_ready = 0;
		if (m->mus_res_gen == m->mus_gen) {          // still the current track
			apc = m->mus_res; apclen = m->mus_res_len; vol = m->mus_res_vol;
			memcpy(key, m->mus_res_key, sizeof(key));
			m->mus_res = NULL;
			ship = 1;
		} else {                                      // superseded while rendering -> drop it
			free(m->mus_res); m->mus_res = NULL;
		}
	}
	pthread_mutex_unlock(&m->mus_lock);
	if (ship) {
		mus_emit(m, key, apc, apclen, vol);          // main thread: stage + ship
		free(apc);
		return TERMGFX_MUSIC_ASYNC_SHIPPED;
	}
	return TERMGFX_MUSIC_ASYNC_IDLE;
}

#else  /* no worker thread (e.g. Windows): render synchronously in submit -- the old blocking path */

void termgfx_audio_music_async_submit(termgfx_audio_t *m, const char *name,
                                      const void *music, size_t len, int rate, int vol)
{
	int16_t *pcm     = NULL;
	size_t   nframes = 0;

	if (m == NULL || m->tier < 1 || name == NULL || music == NULL || len == 0)
		return;
	if (termgfx_midi_render(music, len, rate, 0, &pcm, &nframes) && nframes > 0) {
		termgfx_audio_music(m, name, pcm, nframes * 4, 16, 2, rate, vol);
		free(pcm);
	}
}

int termgfx_audio_music_async_poll(termgfx_audio_t *m) { (void)m; return TERMGFX_MUSIC_ASYNC_IDLE; }

#endif
