/* audio_term.h -- ScummVM's mixer -> the door's audio stream.
 *
 * Replaces NullMixerManager. ScummVM sums every voice, sound effect and
 * synthesized music track (the AdLib/MT-32 emulators register on this same
 * mixer via playStream()) into ONE interleaved S16 stereo stream. That single
 * pull is the whole audio path: there is no useful per-sound boundary to hook,
 * and music arrives already rendered to PCM, so a separate MIDI path would
 * re-do work the engine just did.
 *
 * WALL-CLOCK DRIVEN. Each tick pulls exactly the samples real time is owed:
 * frames_due = elapsed_ms * rate / 1000 - frames_pulled. The game's own pacing
 * can then neither starve nor flood the stream, and audio cannot drift against
 * video, because both ride one clock. Pacing the pull off the game loop instead
 * is what made syncconquer's FMV audio drift ~4% and fall behind after a couple
 * of minutes.
 *
 * The manager is created unconditionally -- engines need a mixer object however
 * the terminal answered -- and whether the pulled PCM is streamed or discarded
 * is sst_io's decision, not ours.
 */
#ifndef SYNCSCUMM_AUDIO_TERM_H_
#define SYNCSCUMM_AUDIO_TERM_H_

#include "backends/mixer/mixer.h"
#include "sst_io.h"

/* The mixer's rate is SST_AUDIO_RATE (sst_io.h), not a rate of our own: the
 * mixer is CREATED at that rate below (init()) and sst_io.c ENCODES at that
 * same rate, and sst_io_audio_stream() takes a frame count, not a rate --
 * the two ends only agree because they read one shared symbol, not because
 * two literals happen to match. Retro adventure audio has no content above
 * ~12kHz, so that rate halves the PCM we handle versus 48000 and costs
 * nothing audible -- and it is deliberately an Opus-legal one, so termgfx's
 * encoder resamples nothing at all. See sst_io.h for why that matters. */

/* MixerManager's pure virtuals are exactly init()/suspendAudio()/resumeAudio()
 * (backends/mixer/mixer.h); isNullDevice() is virtual with a false default.
 * NOTE there is NO update() in the base -- that is NullMixerManager's own
 * invention, a call-counter that shoots the callback every Nth call. We are
 * wall-clock driven and have no period to honor, so tick() replaces it
 * outright. _mixer and _audioSuspended are the base's protected members;
 * don't shadow them. */
class SyncscummMixerManager : public MixerManager {
public:
	SyncscummMixerManager();
	virtual ~SyncscummMixerManager();

	void init() override;
	void suspendAudio() override;
	int  resumeAudio() override;
	bool isNullDevice() const override;

	/* Pull whatever real time is owed and ship it. Cheap and internally
	 * rate-limited by the wall clock, so calling it from more than one site
	 * is safe -- and necessary: see the call sites in syncscumm.cpp and
	 * video_term.cpp. */
	void tick();

private:
	uint32  _startMs;
	uint64  _framesPulled;
	int16  *_buf;
	size_t  _bufFrames;
};

/* The present path must tick too (see the tick() call site in
 * video_term.cpp), and it cannot get here through g_system:
 * getMixerManager() lives on ModularBackend, not OSystem. So the manager
 * publishes itself for that one caller. NULL until initBackend() runs. */
extern SyncscummMixerManager *g_syncscumm_mixer;

#endif /* SYNCSCUMM_AUDIO_TERM_H_ */
