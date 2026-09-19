/* frame.h -- the engine's composed XRGB frame -> termgfx indexed frame.
 *
 * The EGA picture only ever holds the 16 colors a 200-line monitor can show,
 * so the palette is fixed (see frame.c). */
#ifndef SYNCDRIVE_FRAME_H_
#define SYNCDRIVE_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#define FRAME_W      320
#define FRAME_H      200
#define FRAME_COLORS 16

uint32_t frame_color(int i);
void     frame_palette(uint8_t pal768[768]);
int      frame_index(uint32_t xrgb);
void     frame_convert(const uint32_t *xrgb, uint8_t *idx, size_t n);

#endif
