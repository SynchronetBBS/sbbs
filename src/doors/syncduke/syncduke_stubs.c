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
#include "build.h"
#include "audiolib/fx_man.h"
#include "audiolib/music.h"

/* --- FX: digital sound effects --- */
char *FX_ErrorString(int e) { (void)e; return ""; }
int  FX_Init(int sc, int nv, int nc, int sb, unsigned mr)
{ (void)sc; (void)nv; (void)nc; (void)sb; (void)mr; return FX_Ok; }
int  FX_Shutdown(void) { return FX_Ok; }
int  FX_SetCallBack(void (*function)(int32_t)) { (void)function; return FX_Ok; }
void FX_SetVolume(int volume) { (void)volume; }
void FX_SetReverseStereo(int setting) { (void)setting; }
int  FX_GetReverseStereo(void) { return 0; }
void FX_SetReverb(int reverb) { (void)reverb; }
void FX_SetReverbDelay(int delay) { (void)delay; }
int  FX_SetupSoundBlaster(fx_blaster_config blaster, int *mv, int *ms, int *mc)
{ (void)blaster; (void)mv; (void)ms; (void)mc; return FX_Ok; }
int  FX_VoiceAvailable(int priority) { (void)priority; return 1; }
int32_t FX_Pan3D(int handle, int angle, int distance)
{ (void)handle; (void)angle; (void)distance; return FX_Ok; }
int32_t FX_StopSound(int handle) { (void)handle; return FX_Ok; }
int32_t FX_StopAllSounds(void) { return FX_Ok; }
int  FX_PlayVOC3D(uint8_t *ptr, int32_t pitchoffset, int32_t angle, int32_t distance,
                  int32_t priority, uint32_t callbackval)
{ (void)ptr; (void)pitchoffset; (void)angle; (void)distance; (void)priority; (void)callbackval; return 0; }
int  FX_PlayWAV3D(uint8_t *ptr, int pitchoffset, int angle, int distance,
                  int priority, uint32_t callbackval)
{ (void)ptr; (void)pitchoffset; (void)angle; (void)distance; (void)priority; (void)callbackval; return 0; }
int  FX_PlayLoopedVOC(uint8_t *ptr, int32_t loopstart, int32_t loopend,
                      int32_t pitchoffset, int32_t vol, int32_t left, int32_t right,
                      int32_t priority, uint32_t callbackval)
{
	(void)ptr; (void)loopstart; (void)loopend; (void)pitchoffset; (void)vol; (void)left;
	(void)right; (void)priority; (void)callbackval; return 0;
}
int  FX_PlayLoopedWAV(uint8_t *ptr, int32_t loopstart, int32_t loopend,
                      int32_t pitchoffset, int32_t vol, int32_t left, int32_t right,
                      int32_t priority, uint32_t callbackval)
{
	(void)ptr; (void)loopstart; (void)loopend; (void)pitchoffset; (void)vol; (void)left;
	(void)right; (void)priority; (void)callbackval; return 0;
}

/* --- MUSIC: MIDI --- */
int  MUSIC_Init(int SoundCard, int Address) { (void)SoundCard; (void)Address; return 0; }
int  MUSIC_Shutdown(void) { return 0; }
void MUSIC_SetVolume(int volume) { (void)volume; }
void MUSIC_Continue(void) { }
void MUSIC_Pause(void) { }
int  MUSIC_StopSong(void) { return 0; }
void MUSIC_RegisterTimbreBank(unsigned char *timbres) { (void)timbres; }
void PlayMusic(char *filename) { (void)filename; }

/* --- multiplayer: the mmulti transport seam (initmultiplayers/getpacket/sendpacket/
 *     sendlogon/...) now lives in syncduke_net.c (UDP, LAN co-op).  genericmultifunction
 *     (a Build broadcast helper) is not part of that core, so it stays a no-op here. --- */
void  genericmultifunction(int32_t other, char *bufptr, int32_t messleng, int32_t command)
{ (void)other; (void)bufptr; (void)messleng; (void)command; }
