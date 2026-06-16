#ifndef RENDER_SIXEL_H_
#define RENDER_SIXEL_H_

#include <stddef.h>
#include <stdint.h>

// Encode a w*h RGB888 image (row-major, 3 bytes/pixel) as a complete DECSIXEL
// sequence -- intro (ESC P ... q), raster attributes, an exact color palette
// (the image's own <=256 unique colors, no quantization), the pixel bands, and
// the ST terminator (ESC \). The bytes are written into *buf, which is grown via
// realloc as needed (*cap is updated to match); the encoded length is returned.
// All output is printable ASCII / C0, so it is telnet-safe with no base64 step.
size_t sixel_encode(uint8_t **buf, size_t *cap, const uint8_t *rgb, int w, int h);

#endif // RENDER_SIXEL_H_
