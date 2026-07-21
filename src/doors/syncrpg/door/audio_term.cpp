/* audio_term.cpp -- see audio_term.h. */
#include "audio_term.h"

extern "C" {
#include "termgfx_termio.h"
}

/* One tick's ceiling, mirroring syncscumm's audio_term.cpp: bounds a
 * pathological gap (a long blocking load, a stopped debugger) to one bounded
 * burst instead of a multi-second pull that would arrive as a wall of stale
 * audio anyway. */
#define MAX_TICK_FRAMES (SYNCRPG_AUDIO_RATE / 2)   /* 500ms */

SyncrpgAudio::SyncrpgAudio(const Game_ConfigAudio& cfg)
	: GenericAudio(cfg)
{
	/* Stereo S16 at termgfx's rate -- see the header's rate/format note.
	 * Every BGM/SE decoder resamples/converts to this the moment it's
	 * opened (audio_generic.cpp's PlayOnChannel -> decoder->SetFormat()),
	 * so Decode()'s mixed output already matches termgfx_stream's contract. */
	SetFormat(SYNCRPG_AUDIO_RATE, AudioDecoder::Format::S16, 2);
	_start = std::chrono::steady_clock::now();
}

void SyncrpgAudio::tick()
{
	/* No termgfx_termio_audio_available() gate here -- see audio_term.h's
	 * file doc for why: that call always answers 0 for this door's own
	 * headless/capture test runs, and termgfx_termio_audio_stream() already
	 * does its own cheap session checks before any real work. */
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	uint64_t elapsed_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - _start).count();
	uint64_t due = elapsed_ms * SYNCRPG_AUDIO_RATE / 1000;
	if (due <= _framesPulled)
		return;

	uint64_t frames = due - _framesPulled;
	if (frames > MAX_TICK_FRAMES) {
		/* A gap this large means we were not running; shipping every missed
		 * frame would just queue stale audio behind real time. Ship one
		 * bounded burst and re-anchor the clock to now. */
		frames = MAX_TICK_FRAMES;
		_framesPulled = due - frames;
	}

	_buf.resize((size_t)frames * 2);   /* stereo */
	Decode(reinterpret_cast<uint8_t *>(_buf.data()), (int)(frames * 2 * sizeof(int16_t)));
	_framesPulled += frames;

	termgfx_termio_audio_stream(_buf.data(), (size_t)frames);
}
