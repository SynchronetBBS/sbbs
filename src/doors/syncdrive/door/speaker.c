/* speaker.c -- band-limited (PolyBLEP) square wave with a gate ramp. */
#include <string.h>
#include "speaker.h"

static double polyblep(double t, double dt)
{
	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.0;
	}
	if (t > 1.0 - dt) {
		t = (t - 1.0) / dt;
		return t * t + t + t + 1.0;
	}
	return 0.0;
}

void speaker_init(speaker_t *s, int rate)
{
	memset(s, 0, sizeof *s);
	s->rate = rate;
	s->slew = 1.0 / (rate * SPEAKER_RAMP_MS / 1000.0);
}

void speaker_set(speaker_t *s, uint16_t divisor, int on)
{
	s->freq   = SPEAKER_PIT_HZ / (divisor ? divisor : 65536);
	s->target = (on && s->freq < s->rate / 2.0) ? 1.0 : 0.0;
}

void speaker_render(speaker_t *s, int16_t *stereo, size_t frames)
{
	double dt = s->freq / s->rate;
	size_t k;

	for (k = 0; k < frames; k++) {
		double v = 0.0;

		if (s->gain < s->target) {
			s->gain += s->slew;
			if (s->gain > s->target)
				s->gain = s->target;
		} else if (s->gain > s->target) {
			s->gain -= s->slew;
			if (s->gain < s->target)
				s->gain = s->target;
		}
		if (s->gain > 0.0 && dt > 0.0 && dt < 0.5) {
			double half = s->phase + 0.5;

			if (half >= 1.0)
				half -= 1.0;
			v  = s->phase < 0.5 ? 1.0 : -1.0;
			v += polyblep(s->phase, dt);
			v -= polyblep(half, dt);
			s->phase += dt;
			if (s->phase >= 1.0)
				s->phase -= 1.0;
		}
		stereo[k * 2]     = (int16_t)(v * s->gain * SPEAKER_AMP);
		stereo[k * 2 + 1] = stereo[k * 2];
	}
}
