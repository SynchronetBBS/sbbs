// apc.c -- SyncTERM APC cached-image transport (C;S Store + C;Draw{JXL,PPM}), base64 payload.
// Carved out of syncdoom.c's b64_encode()/emit_cached_image() so JXL/PPM (and any future
// cached-image tier) share one transport. I/O-free: builds the bytes, the door sends them.

#include "apc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "genwrap.h"   /* xpdev: GetCurrentProcessId() -- getpid() on POSIX */

// base64 (RFC 4648, no line breaks) -> SyncTERM's b64_decode_alloc()
static const char b64tab[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Encode len bytes of in into out; returns number of base64 chars written.
static size_t b64_encode(char *out, const uint8_t *in, size_t len)
{
	size_t i, o = 0;
	for (i = 0; i + 2 < len; i += 3) {
		uint32_t v = (in[i] << 16) | (in[i + 1] << 8) | in[i + 2];
		out[o++] = b64tab[(v >> 18) & 0x3f];
		out[o++] = b64tab[(v >> 12) & 0x3f];
		out[o++] = b64tab[(v >>  6) & 0x3f];
		out[o++] = b64tab[ v        & 0x3f];
	}
	if (i < len) {
		uint32_t v = in[i] << 16;
		if (i + 1 < len)
			v |= in[i + 1] << 8;
		out[o++] = b64tab[(v >> 18) & 0x3f];
		out[o++] = b64tab[(v >> 12) & 0x3f];
		out[o++] = (i + 1 < len) ? b64tab[(v >> 6) & 0x3f] : '=';
		out[o++] = '=';
	}
	return o;
}

unsigned termgfx_session_salt(void)
{
	static unsigned salt;

	if (salt == 0) {
		salt = (unsigned)GetCurrentProcessId();
		if (salt == 0)
			salt = 1;   /* 0 is the "not computed yet" sentinel */
	}
	return salt;
}

size_t termgfx_apc_image(uint8_t **buf, size_t *cap,
                         const char *file, const char *drawverb,
                         const uint8_t *payload, size_t n, int dx, int dy, int blob)
{
	return termgfx_apc_image_zoom(buf, cap, file, drawverb, payload, n, dx, dy, 1, 1, blob);
}

size_t termgfx_apc_image_zoom(uint8_t **buf, size_t *cap,
                              const char *file, const char *drawverb,
                              const uint8_t *payload, size_t n, int dx, int dy,
                              int zx, int zy, int blob)
{
	size_t   b64len = 4 * ((n + 2) / 3);                        // base64 chars (no terminator)
	size_t   need   = strlen(file) * 2 + strlen(drawverb) + 96 + b64len;
	char     zoom[32];
	uint8_t *p;

	// ZX/ZY are integer replication factors; 1 is the default, so leave them out
	// and stay byte-identical to what every door emitted before.
	zoom[0] = '\0';
	if (zx > 1 || zy > 1)
		snprintf(zoom, sizeof zoom, "ZX=%d;ZY=%d;", zx > 1 ? zx : 1, zy > 1 ? zy : 1);
	need += strlen(zoom);

	if (need > *cap) {
		*buf = realloc(*buf, need);
		*cap = need;
	}
	p = *buf;
	if (blob) {
		// CTerm >= 1.329: draw inline, no C;S Store / cache-file write.
		p += sprintf((char *)p, "\x1b_SyncTERM:C;%sBlob;DX=%d;DY=%d;%s", drawverb, dx, dy, zoom);
		p += b64_encode((char *)p, payload, n);                // base64 payload
		memcpy(p, "\x1b\\", 2); p += 2;                        // ST
	} else {
		p += sprintf((char *)p, "\x1b_SyncTERM:C;S;%s;", file);     // Store header
		p += b64_encode((char *)p, payload, n);                    // base64 payload
		memcpy(p, "\x1b\\", 2); p += 2;                            // ST
		p += sprintf((char *)p, "\x1b_SyncTERM:C;%s;DX=%d;DY=%d;%s%s\x1b\\",
		             drawverb, dx, dy, zoom, file);
	}
	return (size_t)(p - *buf);
}
