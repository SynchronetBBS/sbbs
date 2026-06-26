#ifndef JXL_H_
#define JXL_H_

#include <stddef.h>
#include <stdint.h>

// Encode a w*h RGB888 image as a complete JPEG XL codestream into *buf (grown via
// realloc, *cap updated); returns the encoded length, or 0 on failure -- or when JXL
// was not compiled in (built without WITH_JXL), so the caller can fall back to another
// tier. `distance` = lossy Butteraugli target (1.0 ~ visually lossless; higher = smaller,
// softer); `effort` = JXL encode effort (1 = lightning, lowest latency). I/O-free: the
// caller sends the bytes (typically wrapped by termgfx_apc_image as a DrawJXL).
size_t termgfx_jxl_encode(uint8_t **buf, size_t *cap, const uint8_t *rgb,
                          int w, int h, float distance, int effort);

#endif // JXL_H_
