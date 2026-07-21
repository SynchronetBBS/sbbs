/* SyncRPG -- EasyRPG's GenericAudio mixer -> termgfx_stream (M3 / Task 6).
 *
 * GenericAudio (easyrpg's audio_generic.h) is EasyRPG's software mixer: BGM/SE
 * decode, MIDI (routed to the built-in FmMidi sequencer -- native MIDI-out is
 * off, see build.sh's -DPLAYER_WITH_NATIVE_MIDI=OFF) and volume/fade/looping
 * all happen inside it, producing one interleaved PCM buffer per Decode()
 * call. It has no platform audio device of its own: every concrete backend
 * (Sdl2Audio, etc.) just supplies LockMutex/UnlockMutex plus a callback
 * thread that calls Decode() on a fixed schedule. This door has neither a
 * device nor a thread -- SyncrpgAudio calls Decode() itself, synchronously,
 * from tick() -- so there is no concurrent writer to guard against and
 * LockMutex/UnlockMutex are no-ops.
 *
 * RATE/FORMAT. The constructor's SetFormat() call pins the mixer's own output
 * to termgfx_termio_audio_stream()'s exact contract (interleaved S16 stereo
 * at SYNCRPG_AUDIO_RATE). Every BGM/SE decoder resamples/converts to
 * whatever format GenericAudio configures it for at open time
 * (audio_generic.cpp's PlayOnChannel -> decoder->SetFormat()), so Decode()'s
 * mixed output already matches termgfx_stream's contract -- no conversion
 * happens (or is needed) in this module.
 *
 * WALL-CLOCK DRIVEN, mirroring syncscumm's audio_term: each tick() call pulls
 * exactly the frames real time is owed since the last pull, so the game's own
 * (possibly irregular -- Player::MainLoop can run zero, one or several
 * logical steps per real poll) frame cadence can neither starve nor flood
 * the stream, and audio cannot drift against video/real time because both
 * ride the same wall clock.
 *
 * NOT gated on termgfx_termio_audio_available() -- deliberately, matching
 * syncscumm's audio_term.cpp, which ticks its mixer and calls
 * termgfx_termio_audio_stream() unconditionally too. Two reasons: (1) that
 * call already does its own cheap availability/session checks before doing
 * any real work (g_active/g_hangup/g_audio_enabled, ahead of the expensive
 * open-and-encode path), so a second gate here buys nothing termgfx doesn't
 * already give for free; (2) termgfx_termio_audio_available() answers 0
 * unconditionally for a headless/capture session (no input fd) -- exactly
 * the run mode this door's own tests use -- so gating the PULL on it here
 * would make the diagnostic PCM tap (SYNCSCUMM_AUDIODUMP, read inside
 * termgfx_termio_audio_stream() ahead of every one of its own gates) never
 * fire in the one run mode able to observe it headlessly. Tests target that
 * tap directly; see test/boot_yumenikki.sh.
 *
 * THE audio.h COLLISION (see syncrpg/CMakeLists.txt's include-order comment):
 * this file bare-includes "audio_generic.h", which itself bare-includes
 * "audio.h" from ITS OWN directory (easyrpg/src) -- a quote-include always
 * searches the including file's own directory first, so that resolves to
 * EasyRPG's audio.h regardless of door-wide -I order. This file never
 * bare-includes "audio.h" directly, so the ambiguous name never appears in a
 * context where door/CMakeLists.txt's easyrpg-src-before-termgfx ordering
 * would matter. termgfx_termio.h -- the header this module actually needs
 * from termgfx, for termgfx_termio_audio_stream()/_available() -- has no
 * short/colliding name, so it resolves correctly however it's included;
 * confirmed by a clean build (see audio_term.cpp's include and the M3 build
 * log). GPLv3+, like the EasyRPG tree this links into.
 */
#ifndef SYNCRPG_AUDIO_TERM_H_
#define SYNCRPG_AUDIO_TERM_H_

#include <chrono>
#include <cstdint>
#include <vector>

#include "audio_generic.h"

/* The mixer's rate. Must equal termgfx_termio.c's own (file-private since the
 * M4 extraction into termgfx, so no longer a shared symbol) SST_AUDIO_RATE --
 * 24000 as of this writing; see that file's doc comment on
 * termgfx_termio_audio_stream() if it ever changes. Same value, same
 * reasoning as syncscumm's audio_term.h: an Opus-legal rate with no content
 * above it in retro game audio, so termgfx's encoder resamples nothing. */
#define SYNCRPG_AUDIO_RATE 24000

class SyncrpgAudio : public GenericAudio {
public:
	explicit SyncrpgAudio(const Game_ConfigAudio& cfg);

	void LockMutex() const override {}
	void UnlockMutex() const override {}

	/* Pull whatever real time is owed since the last call and hand it to
	 * termgfx_termio_audio_stream(). Cheap and safe to call every poll --
	 * internally rate-limited by the wall clock. Always calls through (see
	 * the file doc above for why this isn't gated here); a session that
	 * can't play audio at all -- sysop-disabled sound, a tone-only
	 * terminal, a dead peer -- costs one quick termgfx-side check per call
	 * and not one byte on the wire. */
	void tick();

private:
	std::chrono::steady_clock::time_point _start;
	uint64_t _framesPulled = 0;
	std::vector<int16_t> _buf;   /* scratch, stereo interleaved */
};

#endif /* SYNCRPG_AUDIO_TERM_H_ */
