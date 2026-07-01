/*
 * syncduke_stubs.c -- no-op audio (FX_/MUSIC_) + multiplayer stubs.
 *
 * SyncDuke v1 is silent and single-player. The vendored dummy_audiolib.c has its
 * stubs block-commented out (the normal build links the real SDL audiolib), and
 * dummy_multi.c provides only the MP globals -- so we supply the function stubs
 * the game calls. Signatures match audiolib/fx_man.h + music.h (included) and the
 * mmulti API in build.h.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "build.h"
#include "audiolib/fx_man.h"
#include "audiolib/music.h"
#include "audio_mgr.h"          /* termgfx: SyncTERM audio-APC manager */
#include "audio_midi.h"         /* termgfx: MIDI/MUS -> PCM via libADLMIDI (OPL3) */

/* Engine file API (Engine/src/filesystem.h) -- read the .MID from the GRP. */
extern int32_t kopen4load(const char *filename, int openOnlyFromGRP);
extern int32_t kread(int32_t handle, void *buffer, int32_t leng);
extern int32_t kfilelength(int32_t handle);
extern void    kclose(int32_t handle);

/* SyncTERM audio: the per-session manager lives in syncduke_io.c; soundsiz[] is
 * the engine's loaded-sample byte length (global.c), indexed by sound number.
 * A NULL manager / tier < 1 makes the calls below harmless no-ops (silent). */
extern termgfx_audio_t *sd_audio;
extern int32_t          soundsiz[];

/* Master volumes from the Setup Sound menu (engine FXVolume/MusicVolume, 0..255):
 * FX_SetVolume/MUSIC_SetVolume keep these in sync; sd_fx_play scales each SFX by the
 * FX master, and the music volume rides the music channel's base level. Defaults
 * match the engine (config.c). */
static int  sd_fx_vol    = 220;
static int  sd_music_vol = 200;
/* Most-recent track requested (PlayMusic).  At 0% volume we treat music as OFF and skip the
 * render/upload entirely; this lets MUSIC_SetVolume replay the current track when the slider is
 * raised back off 0.  (SyncDuke's Setup Sound has no separate MUSIC:OFF, so 0% is the off switch.) */
static char sd_current_music[16];

/* Music sits a touch under SFX at equal slider settings (as in DOS Duke): the OPL3
 * render is peak-normalized -- a loud, SUSTAINED signal -- while SFX are un-normalized
 * and TRANSIENT, so without trimming the music perceptually dominates. SD_MUSIC_MAXV
 * is the music channel volume (0..100) at a full MusicVolume slider -- tune by ear. */
#define SD_MUSIC_MAXV 20
static int sd_music_v(void) { return sd_music_vol * SD_MUSIC_MAXV / 255; }

/* Per-sound re-dispatch rate limit. Because we return FX_Ok (fire-and-forget; see the
 * sd_fx_play note), the engine's `soundonce` gate is dead -- a CONTINUOUS source (a flying
 * trooper's jet hum, a monster's idle roam, steam/water ambience) re-fires its sound EVERY
 * game tic, flooding the audio APC stream and delaying real SFX. The DOS audiolib dropped
 * those repeats via Sound[].num; lacking that, we drop them here: if the same sound id was
 * dispatched within SD_SFX_REPEAT_GAP ticks (120Hz totalclock), skip the redundant
 * Load+Queue. A game-tic is TICSPERFRAME = 120/26 = 4 ticks, so a per-tic `soundonce` stream
 * (every 4 ticks) sits inside the gap and plays ONCE per burst; the fastest weapon, the
 * chaingun, fires every 3 game-tics = 12 ticks, well outside an 8-tick (2 game-tic) gap. */
#define SD_SFX_REPEAT_GAP 8              /* 2 game tics (~67ms): > 1-tic hum, < 3-tic chaingun */
#define SD_MAXSOUND       450            /* = NUM_SOUNDS (duke3d.h) */

/* Map an engine pan angle (the build relative-bearing `sndang>>6`, 0..31) to a
 * stereo pan -100 (left) .. +100 (right). Faithful to the original audiolib's
 * MV_CalcPanTable: a triangle with center (front) at 0, hard right at 8, center
 * (behind) at 16, hard left at 24 -- the side toward the source plays at full,
 * the opposite side ramps down. */
static int sd_angle_to_pan(int angle)
{
	int a = angle & 31;
	int bal;                                 /* -8..+8, +8 = hard right */

	if (a <= 8)
		bal = a;                         /* 0..8  : center -> right */
	else if (a <= 16)
		bal = 16 - a;                    /* 8..16 : right -> center */
	else if (a <= 24)
		bal = -(a - 16);                 /* 16..24: center -> left  */
	else
		bal = -(32 - a);                 /* 24..32: left -> center  */
	return bal * 100 / 8;
}

/* Ship a one-shot Duke SFX to the terminal. ptr/soundsiz[num] is a complete VOC/WAV
 * file (libsndfile decodes it); distance is Apogee 0..255 with 0 = closest/loudest.
 * Returns FX_Ok so the engine does NOT track a voice handle (fire-and-forget).
 *
 * Why fire-and-forget (and NOT a real voice handle): the engine's `soundonce` gate keys
 * off Sound[num].num, which only stays >0 if FX_Play* returns a handle AND the voice's
 * completion later clears it. The DOS audiolib drove that completion from an async mixer
 * interrupt; our APC-to-SyncTERM path has no such async source, and several engine code
 * paths BUSY-WAIT on a sound finishing -- e.g. newgame() spins `while(Sound[skillsound]
 * .lock>=200);` (empty body) after a skill pick, and the vol-3 ending loops on lock too.
 * Hand back a real handle and lock/num stay pinned with nothing to clear them inside those
 * spins -> hard hang. So we return FX_Ok: the engine runs its `else Sound[num].lock--`
 * path immediately, the busy-waits fall through, and SyncTERM plays the burst out on its
 * own. The repeat-flood that the dead `soundonce` gate would otherwise cause is bounded by
 * the rate limit below. (Pan/3D is a follow-up.) */
static int sd_fx_play(uint8_t *ptr, int angle, int distance, uint32_t callbackval)
{
	extern void    syncduke_log(const char *fmt, ...);
	static int32_t sd_last[SD_MAXSOUND];     /* totalclock of last dispatch ATTEMPT, per sound */
	static uint8_t sd_seen[SD_MAXSOUND];     /* sd_last[num] is valid (vs cold zero-init) */
	static int     sd_fx_logn;               /* TEMP diag: log the first several SFX */
	int            num = (int)callbackval;
	int            vol, pan;

	if (sd_audio == NULL || ptr == NULL || num < 0 || soundsiz[num] <= 0)
		return FX_Ok;

	/* Drop a same-sound re-dispatch inside the burst window (see SD_SFX_REPEAT_GAP). Track
	 * the ATTEMPT (not just sends) so a steady per-tic stream stays one burst; gap < 0 means
	 * totalclock was reset (new level) -> don't suppress. */
	if (num < SD_MAXSOUND) {
		int32_t now = totalclock;
		int32_t gap = now - sd_last[num];

		sd_last[num] = now;
		if (sd_seen[num] && gap >= 0 && gap <= SD_SFX_REPEAT_GAP)
			return FX_Ok;
		sd_seen[num] = 1;
	}

	if (distance < 0)
		distance = 0;
	if (distance > 255)
		distance = 255;
	vol = (255 - distance) * 100 / 255;
	vol = vol * sd_fx_vol / 255;             /* apply the Setup Sound FX master volume */
	pan = sd_angle_to_pan(angle);            /* place the burst on the correct side */
	if (sd_fx_logn < 60) {
		sd_fx_logn++;
		syncduke_log("sfx: #%d tier=%d vol=%d pan=%d siz=%d", num,
		             termgfx_audio_tier(sd_audio), vol, pan, (int)soundsiz[num]);
	}
	termgfx_audio_sfx_file(sd_audio, num, ptr, (size_t)soundsiz[num], vol, pan);
	return FX_Ok;
}

/* Start a LOOPING ambient SFX (wind, machinery, rushing water, helicopter idle...).
 * The engine's looped-FX calls pass the distance (sndist>>6) in the `distance` slot;
 * derive a volume from it like a one-shot, then loop the VOC on a dedicated channel.
 * Returns the manager's positive voice handle (the engine stores it in SoundOwner and
 * later FX_StopSound's it), or 0 (FX_Ok) if nothing started. */
static int sd_loop_play(uint8_t *ptr, int distance, uint32_t callbackval)
{
	extern void syncduke_log(const char *fmt, ...);
	int num = (int)callbackval;
	int vol, h;

	if (sd_audio == NULL || ptr == NULL || num < 0 || soundsiz[num] <= 0)
		return FX_Ok;
	if (distance < 0)
		distance = 0;
	if (distance > 255)
		distance = 255;
	vol = (255 - distance) * 100 / 255;
	vol = vol * sd_fx_vol / 255;
	h   = termgfx_audio_loop_start(sd_audio, num, ptr, (size_t)soundsiz[num], vol);
	syncduke_log("loop: start #%d vol=%d -> handle %d", num, vol, h);
	return h;
}

/* --- FX: digital sound effects --- */
char *FX_ErrorString(int e) { (void)e; return ""; }
int  FX_Init(int sc, int nv, int nc, int sb, unsigned mr)
{ (void)sc; (void)nv; (void)nc; (void)sb; (void)mr; return FX_Ok; }
int  FX_Shutdown(void) { return FX_Ok; }
int  FX_SetCallBack(void (*function)(int32_t)) { (void)function; return FX_Ok; }
void FX_SetVolume(int volume)
{ sd_fx_vol = volume < 0 ? 0 : volume > 255 ? 255 : volume; }
void FX_SetReverseStereo(int setting) { (void)setting; }
int  FX_GetReverseStereo(void) { return 0; }
void FX_SetReverb(int reverb) { (void)reverb; }
void FX_SetReverbDelay(int delay) { (void)delay; }
int  FX_SetupSoundBlaster(fx_blaster_config blaster, int *mv, int *ms, int *mc)
{ (void)blaster; (void)mv; (void)ms; (void)mc; return FX_Ok; }
int  FX_VoiceAvailable(int priority) { (void)priority; return 1; }
/* The engine re-pans every active positional voice each frame (sounds.c). For our
 * looping ambiences (the cinema projector, machinery, water...) `handle` is the
 * termgfx loop handle; map the updated distance to a volume and the bearing to a
 * stereo pan, exactly as the original audiolib did, so the source attenuates AND
 * shifts side as the player moves. termgfx suppresses unchanged L/R, so calling
 * this every frame is cheap. (One-shots never reach here -- fire-and-forget leaves
 * Sound[].num==0, so the engine's pan loop skips them; they're panned at queue.) */
int32_t FX_Pan3D(int handle, int angle, int distance)
{
	int vol, pan;

	if (distance < 0)
		distance = 0;
	if (distance > 255)
		distance = 255;
	vol = (255 - distance) * 100 / 255;
	vol = vol * sd_fx_vol / 255;
	pan = sd_angle_to_pan(angle);
	termgfx_audio_loop_volume(sd_audio, handle, vol, pan);
	return FX_Ok;
}
int32_t FX_StopSound(int handle) { termgfx_audio_loop_stop(sd_audio, handle); return FX_Ok; }
int32_t FX_StopAllSounds(void) { termgfx_audio_sfx_stop_all(sd_audio); return FX_Ok; }
int  FX_PlayVOC3D(uint8_t *ptr, int32_t pitchoffset, int32_t angle, int32_t distance,
                  int32_t priority, uint32_t callbackval)
{ (void)pitchoffset; (void)priority; return sd_fx_play(ptr, (int)angle, (int)distance, callbackval); }
int  FX_PlayWAV3D(uint8_t *ptr, int pitchoffset, int angle, int distance,
                  int priority, uint32_t callbackval)
{ (void)pitchoffset; (void)priority; return sd_fx_play(ptr, angle, distance, callbackval); }
int  FX_PlayLoopedVOC(uint8_t *ptr, int32_t loopstart, int32_t loopend,
                      int32_t pitchoffset, int32_t vol, int32_t left, int32_t right,
                      int32_t priority, uint32_t callbackval)
{
	(void)loopstart; (void)loopend; (void)pitchoffset; (void)left; (void)right; (void)priority;
	return sd_loop_play(ptr, (int)vol, callbackval);   /* vol arg == sndist>>6 == distance */
}
int  FX_PlayLoopedWAV(uint8_t *ptr, int32_t loopstart, int32_t loopend,
                      int32_t pitchoffset, int32_t vol, int32_t left, int32_t right,
                      int32_t priority, uint32_t callbackval)
{
	(void)loopstart; (void)loopend; (void)pitchoffset; (void)left; (void)right; (void)priority;
	return sd_loop_play(ptr, (int)vol, callbackval);   /* vol arg == sndist>>6 == distance */
}

/* --- MUSIC: MIDI --- */
int  MUSIC_Init(int SoundCard, int Address) { (void)SoundCard; (void)Address; return 0; }
int  MUSIC_Shutdown(void) { return 0; }
void MUSIC_SetVolume(int volume)
{
	int was = sd_music_vol;

	sd_music_vol = volume < 0 ? 0 : volume > 255 ? 255 : volume;
	if (sd_music_vol == 0)
		termgfx_audio_music_stop(sd_audio);          /* 0% = OFF: stop the loop (no silent transfer) */
	else {
		termgfx_audio_music_volume(sd_audio, sd_music_v());   /* live slider (balanced vs SFX) */
		if (was == 0 && sd_current_music[0] != '\0')
			PlayMusic(sd_current_music);             /* raised off 0: resume the current track */
	}
}
void MUSIC_Continue(void) { }
void MUSIC_Pause(void) { }
/* The "Setup Sound -> MUSIC: OFF" menu toggle (and Logo()) call this to stop the
 * track -- wire it to the terminal so the looping music actually stops. */
int  MUSIC_StopSong(void) { termgfx_audio_music_stop(sd_audio); return 0; }
void MUSIC_RegisterTimbreBank(unsigned char *timbres) { (void)timbres; }

/* The engine plays the title music from Logo() at startup, before the audio tier
 * has been negotiated with the terminal -- so that first request arrives at tier 0
 * and would be silently dropped (leaving the menu silent until a level loads). Stash
 * the most-recent track requested while the tier is unknown and replay it the moment
 * the terminal answers (sd_music_pending_retry, from the input pump's tier-ready edge). */
static char sd_pending_music[16];

/* Duke music is MIDI in the GRP. Read it, render to PCM via libADLMIDI (OPL3 FM
 * -- the authentic Sound Blaster sound), and loop it on SyncTERM's reserved
 * music channel. No-op unless the terminal negotiated digital audio (tier 1). */
void PlayMusic(char *filename)
{
	extern void syncduke_log(const char *fmt, ...);
	int32_t   fd, len;
	void *    mid;
	int16_t * pcm;
	size_t    nframes;
	char      id[24];           /* content-addressed cache name "<trackname>_<hash>" */
	uint32_t  h;
	int32_t   i;

	if (sd_audio == NULL || filename == NULL || filename[0] == '\0') {
		syncduke_log("music: PlayMusic('%s') ignored (null/empty filename)",
		             filename ? filename : "(null)");
		return;
	}
	strncpy(sd_current_music, filename, sizeof(sd_current_music) - 1);   /* remember for vol-raise replay */
	sd_current_music[sizeof(sd_current_music) - 1] = '\0';
	if (sd_music_vol == 0) {          /* 0% = OFF: don't render/encode/upload a silent track */
		syncduke_log("music: '%s' skipped (volume 0 = off)", filename);
		return;
	}
	if (termgfx_audio_tier(sd_audio) < 1) {
		syncduke_log("music: '%s' deferred (tier %d not ready)", filename,
		             termgfx_audio_tier(sd_audio));
		strncpy(sd_pending_music, filename, sizeof(sd_pending_music) - 1);
		sd_pending_music[sizeof(sd_pending_music) - 1] = '\0';
		return;
	}
	sd_pending_music[0] = '\0';      /* tier ready: this request supersedes any pending one */
	fd = kopen4load(filename, 1);
	syncduke_log("music: '%s' tier=%d fd=%d", filename, termgfx_audio_tier(sd_audio), fd);
	if (fd < 0)
		return;
	len = kfilelength(fd);
	if (len <= 0) {
		kclose(fd);
		return;
	}
	mid = malloc((size_t)len);
	if (mid == NULL) {
		kclose(fd);
		return;
	}
	if (kread(fd, mid, len) != len) {
		kclose(fd);
		free(mid);
		return;
	}
	kclose(fd);

	/* Content-address the track: the FNV-1a hash of the MIDI bytes, prefixed with the
	 * track's (extension-stripped) filename for legibility -> "<name>_<hash>". The hash
	 * keeps it stable across sessions and collision-free across GRPs (a reused name with
	 * different bytes still differs); the name just makes the cache files readable. Try
	 * the cache first -- a hit ships it without the expensive OPL render. */
	h = 2166136261u;
	for (i = 0; i < len; i++)
		h = (h ^ ((const uint8_t *)mid)[i]) * 16777619u;
	{
		const char *b = filename, *p, *dot;
		char        stem[14];
		size_t      sn, k;

		if ((p = strrchr(b, '/')) != NULL)
			b = p + 1;                                     /* drop any directory */
		if ((p = strrchr(b, '\\')) != NULL)
			b = p + 1;
		dot = strrchr(b, '.');                             /* and the extension */
		sn  = (dot != NULL && dot != b) ? (size_t)(dot - b) : strlen(b);
		if (sn >= sizeof(stem))
			sn = sizeof(stem) - 1;
		for (k = 0; k < sn; k++)                            /* no '.' -> termgfx keeps the hash */
			stem[k] = (b[k] == '.') ? '_' : b[k];
		stem[sn] = '\0';
		snprintf(id, sizeof(id), "%s_%08x", stem, (unsigned)h);
	}

	{
		int hit = termgfx_audio_music_play(sd_audio, id, sd_music_v());
		if (hit != TERMGFX_MUSIC_RENDER) {
			syncduke_log("music: '%s' (%s) -- %s", filename, id,
			             hit == TERMGFX_MUSIC_CLIENT ? "client-cached (no render, no upload)"
			                                         : "disk-cached (no render)");
			free(mid);
			return;
		}
	}
	{
		extern uint32_t getticks(void);           /* engine ms clock: split the two costly steps */
		uint32_t t0 = getticks(), t1;
		if (termgfx_midi_render(mid, (size_t)len, 44100, 0, &pcm, &nframes)) {
			t1 = getticks();                      /* render (OPL transcode) done */
			termgfx_audio_music(sd_audio, id, pcm, nframes * 4, 16, 2, 44100,
			                    sd_music_v());     /* encode + disk-cache + upload (transfer) */
			/* render=<transcode ms> xfer=<encode+cache+upload ms>: tells a stall's cause apart. */
			syncduke_log("music: '%s' (%s) rendered %zu frames render=%ums xfer=%ums (vol=%d)",
			             filename, id, nframes, (unsigned)(t1 - t0),
			             (unsigned)(getticks() - t1), sd_music_v());
			free(pcm);
		}
		else {
			syncduke_log("music: '%s' render FAILED (len=%d)", filename, (int)len);
		}
	}
	free(mid);
}

/* Replay the title/menu track that Logo() requested before the audio tier was
 * known. Called from the input pump when the tier first resolves to >= 1. */
void sd_music_pending_retry(void)
{
	char fn[16];

	if (sd_pending_music[0] == '\0' || sd_audio == NULL)
		return;
	if (termgfx_audio_tier(sd_audio) < 1)
		return;
	strcpy(fn, sd_pending_music);
	sd_pending_music[0] = '\0';
	PlayMusic(fn);
}

/* --- multiplayer: the mmulti transport seam (initmultiplayers/getpacket/sendpacket/
 *     sendlogon/...) now lives in syncduke_net.c (UDP, LAN co-op).  genericmultifunction
 *     (a Build broadcast helper) is not part of that core, so it stays a no-op here. --- */
void  genericmultifunction(int32_t other, char *bufptr, int32_t messleng, int32_t command)
{ (void)other; (void)bufptr; (void)messleng; (void)command; }
