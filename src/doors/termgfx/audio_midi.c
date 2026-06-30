// audio_midi.c -- MIDI/MUS/XMI -> S16 PCM via vendored libADLMIDI (OPL3/Nuked).
// See audio_midi.h. C file calling libADLMIDI's C API (adlmidi.h is extern "C").

#include "audio_midi.h"
#include "adlmidi.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_MAX_MS 120000u          // 2-minute safety cap on a rendered loop

// libADLMIDI's OPL3 output is low-amplitude -- a single chip peaks well below
// full scale (~18% on Duke's GRABBAG), so the raw PCM is barely audible once
// SyncTERM applies its own Queue volume on top. Peak-normalize the rendered
// song to ~90% full scale, but cap the boost so a near-silent passage isn't
// amplified into a noise floor.
#define NORM_TARGET_PEAK 29490          // 0.90 * 32767
#define NORM_MAX_GAIN    8              // never boost more than 8x

int termgfx_midi_render(const void *midi, size_t len, int rate,
                        uint32_t max_ms, int16_t **out, size_t *nframes)
{
	struct ADL_MIDIPlayer *dev;
	int16_t *              buf  = NULL;
	size_t                 cap  = 0;    // capacity, in samples (L+R interleaved)
	size_t                 used = 0;    // samples written so far
	size_t                 max_samples;
	short                  chunk[4096]; // 2048 stereo frames per adl_play()

	*out = NULL;
	*nframes = 0;
	if (midi == NULL || len == 0 || rate <= 0)
		return 0;
	if (max_ms == 0)
		max_ms = DEFAULT_MAX_MS;
	max_samples = (size_t)rate * 2 * max_ms / 1000;   // stereo

	dev = adl_init(rate);
	if (dev == NULL)
		return 0;
	adl_setNumChips(dev, 1);
	adl_setLoopEnabled(dev, 0);         // one pass; the player loops the buffer
	if (adl_openData(dev, midi, (unsigned long)len) < 0) {
		adl_close(dev);
		return 0;
	}

	for (;;) {
		int n = adl_play(dev, (int)(sizeof(chunk) / sizeof(chunk[0])), chunk);
		if (n <= 0)
			break;                      // end of song
		if (used + (size_t)n > cap) {
			size_t   ncap = cap ? cap * 2 : (size_t)rate * 2;   // ~1s to start
			int16_t *nb;
			while (ncap < used + (size_t)n)
				ncap *= 2;
			nb = realloc(buf, ncap * sizeof(int16_t));
			if (nb == NULL) {
				free(buf);
				adl_close(dev);
				return 0;
			}
			buf = nb;
			cap = ncap;
		}
		memcpy(buf + used, chunk, (size_t)n * sizeof(int16_t));
		used += (size_t)n;
		if (used >= max_samples)
			break;                      // safety cap
	}
	adl_close(dev);

	if (buf == NULL || used == 0) {
		free(buf);
		return 0;
	}

	// Peak-normalize (see NORM_* above): scan for the loudest sample, then scale
	// the whole buffer up so that peak lands near full scale (gain-capped).
	{
		int32_t peak = 0;
		size_t  i;

		for (i = 0; i < used; i++) {
			int32_t a = buf[i] < 0 ? -(int32_t)buf[i] : buf[i];
			if (a > peak)
				peak = a;
		}
		if (peak > 0) {
			int32_t gain_num = NORM_TARGET_PEAK;
			if ((int64_t)gain_num > (int64_t)peak * NORM_MAX_GAIN)
				gain_num = peak * NORM_MAX_GAIN;       // cap the boost
			if (gain_num > peak) {                     // only ever amplify
				for (i = 0; i < used; i++) {
					int32_t s = (int32_t)buf[i] * gain_num / peak;
					if (s > 32767)
						s = 32767;
					else if (s < -32768)
						s = -32768;
					buf[i] = (int16_t)s;
				}
			}
		}
	}

	*out     = buf;
	*nframes = used / 2;                // interleaved L+R -> stereo frames
	return 1;
}
