#include <stdint.h>

/*
 * This is the set of 16x16 LDR_RGB1_*.png textures from the free public domain
 * blue noise texture pack available here:
 *
 *     https://momentsingraphics.de/BlueNoise.html
 *
 * Red is the least-significant byte and blue is the most (but it makes
 * absolutely no difference as long as it's consistent.) Alpha is not included.
 *
 * It's non-const because we scale all values at startup depending on the color
 * mode.
 */
extern uint32_t noise_textures[64][256];

static const int noise_texture_count = 64;
