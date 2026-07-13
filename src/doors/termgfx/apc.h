#ifndef APC_H_
#define APC_H_

#include <stddef.h>
#include <stdint.h>

// SyncTERM APC image transport. `drawverb` is "DrawJXL" or "DrawPPM", (dx,dy) is
// the canvas pixel position, `payload` is the raw image (base64'd here).
//
// blob == 0 (any SyncTERM): cache-then-draw --
//     ESC_ SyncTERM:C;S;<file>;<base64> ST  ESC_ SyncTERM:C;<verb>;DX;DY;<file> ST
//   The client writes `payload` to its cache dir under `file`, then draws it.
//   For per-frame video that rewrites one cache file every frame.
//
// blob != 0 (CTerm >= 1.329): draw inline, no cache file --
//     ESC_ SyncTERM:C;<verb>Blob;DX;DY;<base64> ST
//   `file` is unused. Gate on termgfx_caps_cterm_version() >= TERMGFX_CTERM_VER_BLOB
//   (and, for DrawJXL, on JXL support already being confirmed).
//
// Written into *buf (grown via realloc, *cap updated); returns its length. This
// layer does no I/O, matching sixel_encode()'s buf/cap contract.
size_t termgfx_apc_image(uint8_t **buf, size_t *cap,
                         const char *file, const char *drawverb,
                         const uint8_t *payload, size_t n, int dx, int dy, int blob);

// Same, but ask the terminal to upscale the image by an integer factor per axis
// (graphics-APC ZX/ZY, CTerm >= TERMGFX_CTERM_VER_ZOOM -- gate on that, and use
// termgfx_geom_zoom() to decide the factors). The door encodes the frame at its
// NATIVE size and skips its own upscale, so the payload is far smaller; SyncTERM
// replicates the pixels, which is what the door's nearest-neighbor upscale did
// anyway. zx/zy <= 1 emit nothing extra, making this byte-for-byte identical to
// termgfx_apc_image() -- which is just this with zx = zy = 1.
size_t termgfx_apc_image_zoom(uint8_t **buf, size_t *cap,
                              const char *file, const char *drawverb,
                              const uint8_t *payload, size_t n, int dx, int dy,
                              int zx, int zy, int blob);

// A per-session token, generated once and cached: the door's process ID. A new
// door process is spawned per connection, so it's unique among the concurrent
// sessions on one machine. Mix it into cache filenames (the blob==0 path) so two
// SyncTERM instances of the SAME dialing-entry -- which share one
// ~/.cache/syncterm/<bbs>/ -- don't collide on a fixed frame name (SourceForge
// syncterm #256). Doesn't need to be random; just distinct per live session.
unsigned termgfx_session_salt(void);

#endif // APC_H_
