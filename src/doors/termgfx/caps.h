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

#endif // TERMGFX_CAPS_H_
