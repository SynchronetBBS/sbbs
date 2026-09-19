/* speaker.h -- the PC speaker (PIT channel 2 + port 61h gate) as PCM. */
#ifndef SYNCDRIVE_SPEAKER_H_
#define SYNCDRIVE_SPEAKER_H_

#include <stddef.h>
#include <stdint.h>

#define SPEAKER_PIT_HZ  1193182.0
#define SPEAKER_AMP     8192
#define SPEAKER_RAMP_MS 2

typedef struct {
	int rate;
	double phase;
	double freq;
	double gain;
	double target;
	double slew;
} speaker_t;

void speaker_init(speaker_t *s, int rate);
void speaker_set(speaker_t *s, uint16_t divisor, int on);
void speaker_render(speaker_t *s, int16_t *stereo, size_t frames);

#endif
