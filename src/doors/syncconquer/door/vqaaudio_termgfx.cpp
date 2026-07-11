// vqaaudio_termgfx.cpp -- VQA (FMV cutscene) audio backend for the terminal
// door. Replaces the vendored vqaaudio_null.cpp, which stubbed every function
// and left the cutscenes silent.
//
// VQA decodes each frame's audio into VQAAudio::TempBuf; the player calls
// VQA_CopyAudio() once per frame to consume it. We accumulate those blocks into
// a memory buffer and ship them to the client via termgfx's streaming-audio
// path (termgfx_audio_stream_chunk() = Store->Load->Queue onto the movie
// channel's SyncTERM A;Queue FIFO), so the dialog plays back-to-back.
//
// A/V SYNC: the VQA drawer (vqadrawer.cpp) paces video off VQA_GetTime(); the
// client plays the shipped PCM at its true sample rate, i.e. real time. So the
// video MUST be paced in real time too, or it drifts against the audio. We
// canNOT use the vendored 60Hz TimerClass for this: its Get_System_Tick_Count()
// computes delta_ms / (1000/60), and 1000/60 truncates to 16, so it ticks at
// ~62.5/s -- it runs ~4.17% FAST. Pacing the video off that raced it ahead of
// the real-time audio (the "audio falls behind after a couple minutes" drift).
// Instead we drive BOTH the video clock (VQA_GetTime) and the audio-ship
// throttle off one true wall clock (door_io_now_ms()), so both advance in real
// time and stay locked. (This is what the OpenAL backend gets for free -- its
// VQA_GetTime is derived from the audio device's real playback position.) We
// never sleep the decode loop (that would stutter the video, whose display
// rides the same loop); the throttle only bounds how far the client FIFO runs
// ahead. A small fixed lead (the FIFO depth) is inherent; growing drift is not.

#include "vqaaudio.h"
#include "vqafile.h"
#include "vqaloader.h"
#include "vqatask.h"

#include <vector>

extern "C" {
#include "audio_mgr.h"   // termgfx: audio_mgr.c is a plain-C translation unit
#include "door_io.h"     // door_io_audio(), door_io_now_ms() -- true wall clock
}

// Real playback clock (ms) for movie video pacing AND audio shipping, anchored
// at movie start, off the door's true monotonic wall clock. Pause-aware so a
// paused cutscene freezes both the video-pace target and the audio throttle.
static uint32_t clk_epoch_ms;   // door_io_now_ms() at last (re)start
static uint32_t clk_base_ms;    // ms banked before the last pause
static bool     clk_running;

static uint32_t movie_ms(void)
{
	return clk_running ? clk_base_ms + (door_io_now_ms() - clk_epoch_ms) : clk_base_ms;
}
static void movie_clk_reset(void)
{
	clk_base_ms  = 0;
	clk_epoch_ms = door_io_now_ms();
	clk_running  = true;
}
static void movie_clk_pause(void)
{
	if (clk_running) {
		clk_base_ms += door_io_now_ms() - clk_epoch_ms;
		clk_running  = false;
	}
}
static void movie_clk_resume(void)
{
	clk_epoch_ms = door_io_now_ms();
	clk_running  = true;
}

// DECODE and SHIPPING are decoupled. The VQA loop is never paused (pausing it
// stutters the video, since the player's frame display rides the same loop): it
// just drops decoded PCM into `accum`. A separate throttle then ships to the
// client only as fast as the video clock advances, so the client FIFO stays
// shallow and the audio can't build a growing lag. `ship_off` is the
// already-shipped prefix of `accum` (index, not erase, to stay O(1) per ship).
static std::vector<unsigned char> accum;
static size_t                     ship_off;       // shipped prefix of accum
static size_t                     total_shipped;  // bytes handed to the client
static int                        accum_bits = 16, accum_channels = 1, accum_rate = 22050;

#define STREAM_CHUNK_BYTES 16384   // one shipped chunk (~0.37s of 22050Hz mono S16)

// Ship whole chunks, but no further ahead of the video clock than ~0.75s so the
// client FIFO stays shallow. `flush_all` ignores the throttle + chunk floor
// (end of movie: ship whatever's left).
static void ship_throttled(bool flush_all)
{
	termgfx_audio_t* m   = door_io_audio();
	int              bps = accum_rate * accum_channels * (accum_bits / 8);   // bytes/sec
	// Ship no further ahead than the real-time playback position: movie_ms() is
	// the same wall clock VQA_GetTime paces the video on, so audio and video
	// advance together and the client FIFO stays ~`ahead` deep.
	size_t           played = (bps > 0) ? (size_t)((uint64_t)movie_ms() * bps / 1000) : 0;
	size_t           ahead  = (bps > 0) ? (size_t)bps * 3 / 4 : STREAM_CHUNK_BYTES;

	while (accum.size() > ship_off) {
		size_t avail = accum.size() - ship_off;
		size_t chunk = flush_all ? avail : STREAM_CHUNK_BYTES;

		if (!flush_all && (avail < STREAM_CHUNK_BYTES || total_shipped > played + ahead))
			break;
		termgfx_audio_stream_chunk(m, accum.data() + ship_off, chunk,
		                           accum_bits, accum_channels, accum_rate, 100);
		ship_off      += chunk;
		total_shipped += chunk;
	}
	// Reclaim the shipped prefix when it's fully drained or has grown large.
	if (ship_off > 0 && (ship_off == accum.size() || ship_off > (1u << 20))) {
		accum.erase(accum.begin(), accum.begin() + ship_off);
		ship_off = 0;
	}
}

int VQA_StartTimerInt(VQAHandle* handle, int a2)
{
	return 0;
}

void VQA_StopTimerInt(VQAHandle* handle)
{
}

int VQA_OpenAudio(VQAHandle* handle, void* hwnd)
{
	VQAConfig* config = &handle->Config;
	VQAHeader* header = &handle->Header;
	VQAData*   data   = handle->VQABuf;
	VQAAudio*  audio  = &data->Audio;

	// Mirror the OpenAL backend's playback-rate derivation so a movie whose
	// FrameRate differs from its native FPS still pitches correctly.
	if (config->AudioRate == -1) {
		if (header->FPS == config->FrameRate)
			config->AudioRate = audio->SampleRate;
		else
			config->AudioRate = config->FrameRate * audio->SampleRate / header->FPS;
	}
	movie_clk_reset();
	return 0;
}

void VQA_CloseAudio(VQAHandle* handle)
{
	accum.clear();
	ship_off = total_shipped = 0;
}

int VQA_StartAudio(VQAHandle* handle)
{
	accum.clear();
	ship_off = total_shipped = 0;
	termgfx_audio_stream_stop(door_io_audio());   // start on a clean channel
	movie_clk_reset();
	return 0;
}

void VQA_StopAudio(VQAHandle* handle)
{
	ship_throttled(true);                         // ship whatever's left
	termgfx_audio_stream_stop(door_io_audio());
}

void VQA_PauseAudio()
{
	movie_clk_pause();   // freezes both the video-pace target and the throttle
}

void VQA_ResumeAudio()
{
	movie_clk_resume();
}

int VQA_CopyAudio(VQAHandle* handle)
{
	VQAData*   data  = handle->VQABuf;
	VQAConfig* config = &handle->Config;
	VQAAudio*  audio = &data->Audio;

	if (audio->TempBufSize > 0) {
		const unsigned char* p = (const unsigned char*)audio->TempBuf;

		accum_bits     = audio->BitsPerSample;
		accum_channels = audio->Channels;
		accum_rate     = (config->AudioRate > 0) ? config->AudioRate : audio->SampleRate;
		// Always accept the decoded block into the memory buffer -- never sleep
		// the loop (that stutters the video). Shipping is throttled below.
		accum.insert(accum.end(), p, p + audio->TempBufSize);
		audio->TempBufSize = 0;
	}
	ship_throttled(false);
	return VQAERR_NONE;
}

void VQA_SetTimer(VQAHandle* handle, int time, int method)
{
	movie_clk_reset();
}

unsigned VQA_GetTime(VQAHandle* handle)
{
	// Real-time 60Hz ticks (the drawer's desiredframe = DrawRate*curtime/60),
	// off the true wall clock -- NOT the vendored ~4%-fast TimerClass.
	return (unsigned)((uint64_t)movie_ms() * 60 / 1000);
}

int VQA_TimerMethod()
{
	return 0;
}
