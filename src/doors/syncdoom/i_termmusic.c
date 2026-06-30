// i_termmusic.c -- SyncDOOM terminal music module.
//
// A Doom music_module_t that renders the level's MUS/MIDI music lump to OPL3
// FM PCM (termgfx_midi_render / vendored libADLMIDI) and loops it on the user's
// terminal through SyncTERM's audio APC (termgfx_audio_music -- OGG-cached and
// content-addressed). It's wired into i_sound.c's InitMusicModule(), so the
// engine's ordinary S_ChangeMusic -> I_RegisterSong/I_PlaySong path drives it
// with no game-code changes. Mirrors SyncDuke's PlayMusic() wiring.
//
// libADLMIDI's adl_openData auto-detects MUS/MIDI/XMI, so Doom's raw MUS lumps
// feed straight in -- no mus2mid step. If the manager's capability tier is < 1
// (terminal can't do digital audio) every call is a harmless no-op; the title
// song, registered before the cap probe resolves, is stashed and replayed on
// the tier-ready edge (sd_music_tier_ready, called from syncdoom.c).

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_sound.h"

#include "audio_mgr.h"      // termgfx audio manager
#include "audio_midi.h"     // termgfx_midi_render (MIDI/MUS/XMI -> PCM)

// Owned by syncdoom.c: the per-session audio manager (NULL until created).
extern termgfx_audio_t *sd_audio;

// Music-vs-SFX balance: SFX are loud and transient, OPL music is sustained and
// peak-normalised, so at equal gain the music perceptually dominates. Cap the
// music channel well below unity (Doom's 0..127 slider maps onto 0..MAXV).
// Tunable by ear -- SyncDuke settled near 20; start a touch higher for Doom.
#define TERM_MUS_MAXV 28

static int term_music_vol = TERM_MUS_MAXV * 127 / 100;   // Doom 0..127 (pre-slider default)

static int term_music_v(void)
{
	return term_music_vol * TERM_MUS_MAXV / 127;         // -> termgfx 0..100 (balanced)
}

// A registered song: a private copy of the lump bytes plus a content-addressed
// cache name (same music -> same SyncTERM cache entry across sessions).
typedef struct {
	unsigned char *data;
	int len;
	char name[24];
} term_song_t;

static term_song_t *term_current;   // song handed to PlaySong (for volume/resume/query)
static term_song_t *term_pending;   // registered+played while tier < 1 (replay when ready)

// 32-bit FNV-1a over the lump -> a stable "d_<hex>" cache name.
static unsigned term_hash(const unsigned char *p, int n)
{
	unsigned h = 2166136261u;
	int      i;

	for (i = 0; i < n; i++) {
		h ^= p[i];
		h *= 16777619u;
	}
	return h;
}

// Render the song to PCM and loop it on the music channel (tier known >= 1).
static void term_emit(term_song_t *s)
{
	int16_t *pcm;
	size_t   nframes;

	extern void dlog(const char *fmt, ...);     // syncdoom.c: BBS-log line (stderr)
	int      hit;

	if (sd_audio == NULL || s == NULL)
		return;
	// Cache hit (the client's own persistent cache, or the door-side OGG on disk):
	// ship it without rendering. Only render the MIDI/MUS on a full miss.
	hit = termgfx_audio_music_play(sd_audio, s->name, term_music_v());
	if (hit != TERMGFX_MUSIC_RENDER) {
		dlog("music: %s -- %s", s->name,
		     hit == TERMGFX_MUSIC_CLIENT ? "client-cached (no render, no upload)"
		                                 : "disk-cached (no render)");
		return;
	}
	if (termgfx_midi_render(s->data, (size_t)s->len, 44100, 0, &pcm, &nframes)) {
		dlog("music: %s rendered %zu frames -> encode+upload+cache", s->name, nframes);
		termgfx_audio_music(sd_audio, s->name, pcm, nframes * 4, 16, 2, 44100,
		                    term_music_v());     // encodes, uploads, AND caches the OGG
		free(pcm);
	} else {
		dlog("music: %s render FAILED (len=%d)", s->name, s->len);
	}
}

static boolean I_Term_InitMusic(void)
{
	return true;                    // real readiness is the manager's tier, checked per play
}

static void I_Term_ShutdownMusic(void)
{
	if (sd_audio != NULL)
		termgfx_audio_music_stop(sd_audio);
	term_current = NULL;
	term_pending = NULL;
}

static void I_Term_SetMusicVolume(int volume)
{
	term_music_vol = volume < 0 ? 0 : volume > 127 ? 127 : volume;
	if (sd_audio != NULL)
		termgfx_audio_music_volume(sd_audio, term_music_v());   // live, no restart
}

// Doom pauses music on the pause menu. For a looping terminal track a stop/replay
// would re-upload the whole song, so let it keep looping under the pause -- the
// player barely notices and the bandwidth is saved.
static void I_Term_PauseSong(void)  { }
static void I_Term_ResumeSong(void) { }

static void *I_Term_RegisterSong(void *data, int len)
{
	term_song_t *s;

	if (data == NULL || len <= 0)
		return NULL;
	s = malloc(sizeof(*s));
	if (s == NULL)
		return NULL;
	s->data = malloc((size_t)len);
	if (s->data == NULL) {
		free(s);
		return NULL;
	}
	memcpy(s->data, data, (size_t)len);
	s->len = len;
	snprintf(s->name, sizeof(s->name), "d_%08x", term_hash(s->data, len));
	return s;
}

static void I_Term_UnRegisterSong(void *handle)
{
	term_song_t *s = handle;

	if (s == NULL)
		return;
	if (s == term_current)
		term_current = NULL;
	if (s == term_pending)
		term_pending = NULL;
	free(s->data);
	free(s);
}

static void I_Term_PlaySong(void *handle, boolean looping)
{
	term_song_t *s = handle;

	(void)looping;                  // Doom music loops; termgfx loops the buffer regardless
	if (s == NULL)
		return;
	term_current = s;
	if (sd_audio == NULL || termgfx_audio_tier(sd_audio) < 1) {
		term_pending = s;           // cap not negotiated yet -> replay on the tier-ready edge
		return;
	}
	term_pending = NULL;
	term_emit(s);
}

static void I_Term_StopSong(void)
{
	term_current = NULL;
	term_pending = NULL;
	if (sd_audio != NULL)
		termgfx_audio_music_stop(sd_audio);
}

static boolean I_Term_MusicIsPlaying(void)
{
	return term_current != NULL;
}

static void I_Term_PollMusic(void) { }

// Called from syncdoom.c when the audio capability tier first resolves to >= 1,
// to start a song that was registered/played before the terminal answered the
// probe (the startup title music).
void sd_music_tier_ready(void)
{
	term_song_t *s = term_pending;

	term_pending = NULL;
	if (s != NULL && sd_audio != NULL && termgfx_audio_tier(sd_audio) >= 1)
		term_emit(s);
}

static snddevice_t term_music_devices[] =
{
	SNDDEVICE_PAS,
	SNDDEVICE_GUS,
	SNDDEVICE_WAVEBLASTER,
	SNDDEVICE_SOUNDCANVAS,
	SNDDEVICE_GENMIDI,
	SNDDEVICE_AWE32,
	SNDDEVICE_SB,
};

music_module_t     sd_term_music_module =
{
	term_music_devices,
	sizeof(term_music_devices) / sizeof(*term_music_devices),
	I_Term_InitMusic,
	I_Term_ShutdownMusic,
	I_Term_SetMusicVolume,
	I_Term_PauseSong,
	I_Term_ResumeSong,
	I_Term_RegisterSong,
	I_Term_UnRegisterSong,
	I_Term_PlaySong,
	I_Term_StopSong,
	I_Term_MusicIsPlaying,
	I_Term_PollMusic,
};
