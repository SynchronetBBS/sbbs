/* Unit tests for speaker.c. */
#include <assert.h>
#include <stdlib.h>
#include "speaker.h"

#define RATE 24000

static int16_t buf[RATE * 2];

static int rising_crossings(const int16_t *st, size_t frames)
{
	size_t k;
	int    n = 0;

	for (k = 1; k < frames; k++) {
		if (st[(k - 1) * 2] < 0 && st[k * 2] >= 0)
			n++;
	}
	return n;
}

int main(void)
{
	speaker_t s;
	size_t    k;
	int       n;

	/* Gated off from the start: pure silence. */
	speaker_init(&s, RATE);
	speaker_render(&s, buf, RATE);
	for (k = 0; k < RATE * 2; k++)
		assert(buf[k] == 0);

	/* 440 Hz: PIT divisor 1193182 / 440 = 2711.8 -> 2712 (439.97 Hz). */
	speaker_set(&s, 2712, 1);
	speaker_render(&s, buf, RATE);
	n = rising_crossings(buf, RATE);
	assert(n >= 438 && n <= 441);

	/* Stereo: both channels identical. Peak within the amplitude. */
	for (k = 0; k < RATE; k++) {
		assert(buf[k * 2] == buf[k * 2 + 1]);
		assert(abs(buf[k * 2]) <= SPEAKER_AMP + SPEAKER_AMP / 4);
	}

	/* Gate on from silence: the first sample is near zero (the ramp). */
	speaker_init(&s, RATE);
	speaker_set(&s, 2712, 1);
	speaker_render(&s, buf, 4);
	assert(abs(buf[0]) <= SPEAKER_AMP / 16);

	/* Gate off: silent once the ramp is over. */
	speaker_render(&s, buf, RATE / 10);
	speaker_set(&s, 2712, 0);
	speaker_render(&s, buf, RATE / 10);
	for (k = (size_t)(RATE * SPEAKER_RAMP_MS / 1000 + 1); k < RATE / 10; k++)
		assert(buf[k * 2] == 0);

	/* Divisor 1 (1.19 MHz, above Nyquist): silence. */
	speaker_init(&s, RATE);
	speaker_set(&s, 1, 1);
	speaker_render(&s, buf, RATE / 10);
	for (k = 0; k < RATE / 10 * 2; k++)
		assert(buf[k] == 0);

	/* Divisor 0 means 65536: 18.2 Hz. */
	speaker_init(&s, RATE);
	speaker_set(&s, 0, 1);
	speaker_render(&s, buf, RATE);
	n = rising_crossings(buf, RATE);
	assert(n >= 17 && n <= 19);
	return 0;
}
