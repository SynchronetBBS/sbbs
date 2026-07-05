// i_termsound.c -- SyncDOOM terminal SFX module.
//
// A Doom sound_module_t that, instead of opening a sound card, ships each
// digital sound effect to the user's terminal through SyncTERM's audio APC
// (the termgfx audio manager). It's registered in i_sound.c's sound_modules[]
// table, so the engine's ordinary S_StartSound -> I_StartSound path drives it
// with no changes to game code. Music is a separate module_t (i_termmusic.c --
// the MUS/MIDI lumps OPL-rendered through the same audio manager).
//
// Selection: snd_sfxdevice defaults to SNDDEVICE_SB, which is in our device
// list, so InitSfxModule() picks this module when no SDL module is compiled in.
// If the manager's capability tier is < 1 (terminal can't do digital audio),
// every StartSound is a harmless no-op.

#include <stdio.h>
#include <stddef.h>

#include "doomtype.h"
#include "i_sound.h"
#include "sounds.h"        // S_sfx[], sfxinfo_t
#include "w_wad.h"
#include "z_zone.h"        // PU_STATIC

#include "audio_mgr.h"     // termgfx audio manager

// Owned by syncdoom.c: the per-session audio manager (NULL until created;
// stays usable but silent until the capability probe sets tier == 1).
extern termgfx_audio_t *sd_audio;

// Mirror the SDL module's device coverage (plus PC-speaker/AdLib) so this
// module is selected for whatever snd_sfxdevice the user/config picks.
static snddevice_t      term_sound_devices[] =
{
	SNDDEVICE_SB,
	SNDDEVICE_PAS,
	SNDDEVICE_GUS,
	SNDDEVICE_WAVEBLASTER,
	SNDDEVICE_SOUNDCANVAS,
	SNDDEVICE_AWE32,
	SNDDEVICE_ADLIB,
	SNDDEVICE_PCSPEAKER,
};

static boolean I_Term_InitSound(boolean use_sfx_prefix)
{
	(void)use_sfx_prefix;
	return true;            // real readiness is the manager's tier, checked per play
}

static void I_Term_ShutdownSound(void)
{
}

// Doom SFX lumps are "DS"<name>; follow a link to the referenced sound.
static int I_Term_GetSfxLumpNum(sfxinfo_t *sfx)
{
	char namebuf[16];

	if (sfx->link != NULL)
		sfx = sfx->link;
	snprintf(namebuf, sizeof(namebuf), "ds%s", sfx->name);
	return W_CheckNumForName(namebuf);
}

static void I_Term_Update(void)
{
}

static void I_Term_UpdateSoundParams(int channel, int vol, int sep)
{
	(void)channel; (void)vol; (void)sep;
}

extern uint32_t sd_now_ms(void);           // syncdoom.c: monotonic ms clock

// Collapse the same sfx re-triggered within this window into a single play. Doom has no
// terminal-side sound coalescing (unlike SyncDuke's FX path), so a firefight floods
// identical hitscan/imp/explosion triggers; near-simultaneous duplicates are inaudible
// as separate hits yet each still costs a Load+Queue on the shared socket. (This burst
// dedup is the ONLY drop: an output-backpressure drop was tried here and removed -- it
// randomly ate one-time sounds during combat, since frames keep the socket busy most of
// the time; the channel voice-stealing in termgfx bounds audio latency instead.)
#define SD_SFX_COALESCE_MS 40

static int sd_sfx_drop(int id)
{
	static uint32_t last_ms[NUMSFX];
	static uint8_t  seen[NUMSFX];
	uint32_t        now;

	if (id < 0 || id >= NUMSFX)
		return 0;
	now = sd_now_ms();
	if (seen[id] && (uint32_t)(now - last_ms[id]) <= SD_SFX_COALESCE_MS)
		return 1;
	last_ms[id] = now;
	seen[id]    = 1;
	return 0;
}

// channel doubles as the engine's handle (matches the SDL module); the engine
// passes it back to StopSound / SoundIsPlaying.
static int I_Term_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
	int   lumpnum, lumplen, id, rate, length, pan;
	byte *data;

	if (sd_audio == NULL)
		return channel;

	id = (int)(sfxinfo - S_sfx);            // event's sfx enum index
	if (sd_sfx_drop(id))                    // backed-up pipe or a same-sfx burst -> skip
		return channel;

	lumpnum = sfxinfo->lumpnum;
	if (lumpnum < 0)
		lumpnum = I_Term_GetSfxLumpNum(sfxinfo);
	if (lumpnum < 0)
		return channel;

	data    = W_CacheLumpNum(lumpnum, PU_STATIC);
	lumplen = W_LumpLength(lumpnum);
	// DMX header: 0x0003, u16 rate, u32 length, then 8-bit unsigned PCM.
	if (lumplen < 8 || data[0] != 0x03 || data[1] != 0x00) {
		W_ReleaseLumpNum(lumpnum);
		return channel;
	}
	rate   = (data[3] << 8) | data[2];
	length = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];
	if (length < 0 || length + 8 > lumplen)
		length = lumplen - 8;

	pan = (sep - 128) * 100 / 128;          // sep 0..255 (128=center) -> -100..+100
	termgfx_audio_sfx(sd_audio, id, data + 8, (size_t)length,
	                  8, 1, rate, vol * 100 / 127, pan);

	W_ReleaseLumpNum(lumpnum);
	return channel;
}

static void I_Term_StopSound(int channel)
{
	(void)channel;          // fire-and-forget: SyncTERM plays the burst out
}

static boolean I_Term_SoundIsPlaying(int channel)
{
	(void)channel;
	return false;           // engine treats each one-shot as already finished
}

static void I_Term_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
	(void)sounds; (void)num_sounds;
}

sound_module_t sd_term_sound_module =
{
	term_sound_devices,
	sizeof(term_sound_devices) / sizeof(*term_sound_devices),
	I_Term_InitSound,
	I_Term_ShutdownSound,
	I_Term_GetSfxLumpNum,
	I_Term_Update,
	I_Term_UpdateSoundParams,
	I_Term_StartSound,
	I_Term_StopSound,
	I_Term_SoundIsPlaying,
	I_Term_PrecacheSounds,
};
