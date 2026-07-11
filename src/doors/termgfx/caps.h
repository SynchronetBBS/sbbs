#ifndef TERMGFX_CAPS_H_
#define TERMGFX_CAPS_H_

#include <stddef.h>
#include <stdint.h>

// caps.h -- terminal capability cap-probe parsing for SyncTERM APC graphics
// doors. I/O-free: the door emits the query string through its own output path
// and runs its own bounded read loop; these helpers only build the query and
// inspect the bytes that come back, so the socket/poll/timeout plumbing stays
// in the door.

// Query asking SyncTERM whether it can decode JPEG XL: the APC
// "ESC _ SyncTERM:Q;JXL ESC \". A NUL-terminated string -- emit it once at
// startup (e.g. out_put(s, strlen(s))) and feed the reply to the parser below.
extern const char *const termgfx_query_jxl;

// Scan `len` accumulated input bytes for SyncTERM's JXL-capability reply, the
// CTerm state report "ESC [ = 1 ; {0,1} - n". Returns 1 if JXL is supported,
// 0 if the terminal answered "no", or -1 if no complete reply is present yet
// (keep reading until the deadline). Any 0-or-1 answer also identifies the peer
// as SyncTERM. Idempotent over a growing buffer -- call it after each read.
int termgfx_caps_parse_jxl(const uint8_t *acc, int len);

// SyncTERM CTerm version (major*1000 + minor) thresholds for version-gated
// features -- compare against termgfx_caps_cterm_version(). Add more here as
// features land so the magic numbers live in one place. (exec/load/cterm_lib.js
// has the fuller list: fonts 1155, sixel 1189, CTDA 1207, copy-buffers 1316,
// JPEG XL 1318, status-query 1327 -- add the ones the doors actually gate on.)
#define TERMGFX_CTERM_VER_PPM  1002   // APC PPM pixel-media verbs (DrawPPM etc.)
#define TERMGFX_CTERM_VER_BLOB 1329   // APC blob media verbs: A;LoadBlob, Draw*Blob, Load*Blob

// Extract the CTerm version (major*1000 + minor, so 1.329 -> 1329) from a
// DA1 reply whose numeric params the door has already split out. `intro` is the
// CSI intro byte ('=' for this reply). Returns -1 if `params` are not a CTerm
// DA1 report. Use it to gate revision-guarded features (e.g. the >= 1329 APC
// blob media verbs). The door sends the query as part of its DA1 probe
// ("ESC [ c") and feeds the parsed reply here.
int termgfx_caps_cterm_version(const int *params, int nparams, char intro);

#endif // TERMGFX_CAPS_H_
