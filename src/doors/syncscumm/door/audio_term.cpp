/* audio_term.cpp -- see audio_term.h. */
#include "common/scummsys.h"

#if defined(USE_SYNCHRONET_DRIVER)

#include <stdlib.h>

#include "audio_term.h"
#include "audio/mixer_intern.h"
#include "common/system.h"

extern "C" {
#include "sst_io.h"
}

/* One tick's ceiling. A pull is normally ~10-20ms of frames; this only bounds
 * a pathological gap (a long blocking load, a stopped debugger) so we ship one
 * bounded burst and re-anchor rather than allocating a multi-second pull that
 * would arrive as a wall of stale audio anyway. */
#define MAX_TICK_FRAMES (SST_AUDIO_RATE / 2)   /* 500ms */

SyncscummMixerManager *g_syncscumm_mixer;

SyncscummMixerManager::SyncscummMixerManager()
	: _startMs(0), _framesPulled(0), _buf(0), _bufFrames(0) {
}

SyncscummMixerManager::~SyncscummMixerManager() {
	free(_buf);
	_buf = 0;
	if (g_syncscumm_mixer == this)
		g_syncscumm_mixer = 0;   /* never leave the present path a dangling tick() */
}

void SyncscummMixerManager::init() {
	/* MixerImpl(uint sampleRate, bool stereo = true, uint outBufSize = 0) --
	 * same shape NullMixerManager::init() uses.
	 *
	 * Stereo, even though the wire is mono by default ("[audio] channels",
	 * sst_io.c): the downmix belongs downstream, not here. A mono mixer would
	 * halve the PCM we copy, but it also flips getOutputStereo(), and ScummVM's
	 * engines branch on that -- digital iMUSE (engines/scumm/imuse_digi) builds
	 * its waveout and its internal mixer's streams around the answer. That is a
	 * mix path desktop backends only take when the sound DEVICE is mono, i.e.
	 * one this door would be alone in exercising, and the saving is a memcpy on
	 * a 320x200 door that spends its budget on sixels. Worse, it can't be
	 * configured: the mixer is built during initBackend(), long before
	 * syncscumm.ini's [audio] section is read on the first PCM block, so a
	 * sysop asking for stereo could not get it back. Mix stereo, narrow the
	 * wire. */
	_mixer = new Audio::MixerImpl(SST_AUDIO_RATE, true, 4096);
	assert(_mixer);
	_mixer->setReady(true);
	_bufFrames = MAX_TICK_FRAMES;
	/* Deliberately unchecked, unlike _mixer above: _mixer is structural (the
	 * engine needs a mixer object regardless of whether audio ever plays),
	 * while this buffer only serves playback. tick() already guards on
	 * _buf == 0 and simply skips the pull, so an allocation failure here
	 * degrades to a silent session instead of crashing one. */
	_buf = (int16 *)calloc(_bufFrames * 2, sizeof(int16));   /* *2: stereo */
	_startMs = g_system->getMillis();
	_framesPulled = 0;
	g_syncscumm_mixer = this;
}

void SyncscummMixerManager::tick() {
	if (_audioSuspended || _mixer == 0 || _buf == 0)
		return;

	uint32 elapsed = g_system->getMillis() - _startMs;
	uint64 due = (uint64)elapsed * SST_AUDIO_RATE / 1000;
	if (due <= _framesPulled)
		return;

	uint64 frames = due - _framesPulled;
	if (frames > _bufFrames) {
		/* A gap this large means we were not running; shipping every missed
		 * frame would just queue stale audio behind real time. Ship one
		 * bounded burst and re-anchor the clock to now. */
		frames = _bufFrames;
		_framesPulled = due - frames;
	}

	/* mixCallback()'s `len` is a BYTE count, not a frame count: mixer.cpp
	 * does memset(buf, 0, len) and only then len >>= 2 for stereo, with an
	 * assert(len % 4 == 0). The vendored NullMixerManager passes a frame
	 * count here -- a latent upstream quirk this backend must not copy. */
	_mixer->mixCallback((byte *)_buf, (uint)(frames * 4));
	_framesPulled += frames;

	sst_io_audio_stream(_buf, (size_t)frames);
}

void SyncscummMixerManager::suspendAudio() {
	/* Stop pulling. Anything already queued in the terminal's FIFO plays
	 * out -- the cushion is by design, and a suspend is not a stop. */
	_audioSuspended = true;
}

int SyncscummMixerManager::resumeAudio() {
	if (!_audioSuspended)
		return -1;
	/* Re-anchor: real time advanced while suspended, but no PCM was
	 * produced for it. Without this, the first tick back would compute a
	 * huge frames_due and ship a burst of stale audio. */
	_startMs = g_system->getMillis();
	_framesPulled = 0;
	_audioSuspended = false;
	return 0;
}

bool SyncscummMixerManager::isNullDevice() const {
	return false;
}

#endif /* USE_SYNCHRONET_DRIVER */
