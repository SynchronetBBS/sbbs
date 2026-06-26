#ifndef APC_H_
#define APC_H_

#include <stddef.h>
#include <stdint.h>

// SyncTERM APC cached-image transport. Build the Store + Draw escape sequence that
// caches `payload` (base64-encoded) under `file` and then draws it -- `drawverb` is
// "DrawJXL" or "DrawPPM", (dx,dy) is the canvas pixel position. The whole sequence
//
//     ESC _ SyncTERM:C;S;<file>;<base64> ST  ESC _ SyncTERM:C;<verb>;DX=<dx>;DY=<dy>;<file> ST
//
// is written into *buf (grown via realloc, *cap updated); returns its length. The
// caller sends those bytes to the terminal -- this layer does no I/O, matching
// sixel_encode()'s buf/cap contract.
size_t termgfx_apc_image(uint8_t **buf, size_t *cap,
                         const char *file, const char *drawverb,
                         const uint8_t *payload, size_t n, int dx, int dy);

#endif // APC_H_
