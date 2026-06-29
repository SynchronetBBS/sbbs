// audio.c -- SyncTERM "SyncTERM:A" audio-APC transport (Store/Load/Queue/Synth/
// Flush) + the libsndfile capability probe. See audio.h. I/O-free, in the style
// of apc.c / caps.c: the door sends the bytes and owns the read loop.

#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// base64 (RFC 4648, no line breaks) -> SyncTERM's b64_decode_alloc(). Mirrors
// apc.c's encoder; kept local so the module stays self-contained.
static const char b64tab[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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

static void le16(uint8_t *p, uint16_t v)
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
}

static void le32(uint8_t *p, uint32_t v)
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
	p[2] = (v >> 16) & 0xff;
	p[3] = (v >> 24) & 0xff;
}

// Canonical 44-byte PCM WAV header into h[0..43] for `datalen` bytes of audio.
static void wav_header(uint8_t *h, size_t datalen, int bits, int ch, int rate)
{
	uint32_t byterate   = (uint32_t)rate * ch * bits / 8;
	uint16_t blockalign = (uint16_t)(ch * bits / 8);

	memcpy(h + 0, "RIFF", 4);
	le32(h + 4, (uint32_t)(36 + datalen));
	memcpy(h + 8, "WAVE", 4);
	memcpy(h + 12, "fmt ", 4);
	le32(h + 16, 16);                       // fmt chunk size
	le16(h + 20, 1);                        // PCM
	le16(h + 22, (uint16_t)ch);
	le32(h + 24, (uint32_t)rate);
	le32(h + 28, byterate);
	le16(h + 32, blockalign);
	le16(h + 34, (uint16_t)bits);
	memcpy(h + 36, "data", 4);
	le32(h + 40, (uint32_t)datalen);
}

// ---- capability probe ----------------------------------------------------

// APC: ESC _ SyncTERM:Q;libsndfile ST  (ST = ESC backslash)
const char *const termgfx_audio_query = "\x1b_SyncTERM:Q;libsndfile\x1b\\";

int termgfx_audio_parse_caps(const uint8_t *acc, int len)
{
	int j;

	// reply: ESC [ = 7 ; 1 0 0 ; {0,1} n
	for (j = 0; j + 9 < len; j++) {
		if (acc[j] == 0x1b && acc[j + 1] == '[' && acc[j + 2] == '=' &&
		    acc[j + 3] == '7' && acc[j + 4] == ';' &&
		    acc[j + 5] == '1' && acc[j + 6] == '0' && acc[j + 7] == '0' &&
		    acc[j + 8] == ';' &&
		    (acc[j + 9] == '0' || acc[j + 9] == '1'))
			return acc[j + 9] - '0';
	}
	return -1;
}

// ---- builders ------------------------------------------------------------

size_t termgfx_audio_cache_file(uint8_t **buf, size_t *cap, const char *file,
                                const void *data, size_t bytes)
{
	size_t   b64len = 4 * ((bytes + 2) / 3);
	size_t   need   = strlen(file) + b64len + 64;
	uint8_t *p;

	if (need > *cap) {
		*buf = realloc(*buf, need);
		*cap = need;
	}
	p  = *buf;
	p += sprintf((char *)p, "\x1b_SyncTERM:C;S;%s;", file);     // Store header
	p += b64_encode((char *)p, data, bytes);                   // base64 payload
	memcpy(p, "\x1b\\", 2); p += 2;                            // ST
	return (size_t)(p - *buf);
}

size_t termgfx_audio_cache_pcm(uint8_t **buf, size_t *cap, const char *file,
                               const void *data, size_t bytes,
                               int bits, int channels, int rate)
{
	size_t   wavlen = 44 + bytes;
	size_t   n;
	uint8_t *wav;

	wav = malloc(wavlen);
	if (wav == NULL)
		return 0;
	wav_header(wav, bytes, bits, channels, rate);
	memcpy(wav + 44, data, bytes);
	n = termgfx_audio_cache_file(buf, cap, file, wav, wavlen);  // wrap-then-Store
	free(wav);
	return n;
}

size_t termgfx_audio_load(uint8_t **buf, size_t *cap, int slot, const char *file)
{
	size_t need = strlen(file) + 48;

	if (need > *cap) {
		*buf = realloc(*buf, need);
		*cap = need;
	}
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Load;S=%d;%s\x1b\\", slot, file);
}

size_t termgfx_audio_queue(uint8_t **buf, size_t *cap, int ch, int slot,
                           int vol, int pan, int loop)
{
	int vl, vr;

	if (vol < 0)
		vol = 0;
	if (vol > 100)
		vol = 100;
	if (pan < -100)
		pan = -100;
	if (pan > 100)
		pan = 100;
	vl = (pan > 0) ? vol * (100 - pan) / 100 : vol;
	vr = (pan < 0) ? vol * (100 + pan) / 100 : vol;

	if (*cap < 96) {
		*buf = realloc(*buf, 96);
		*cap = 96;
	}
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Queue;C=%d;S=%d;VL=%d;VR=%d%s\x1b\\",
	                       ch, slot, vl, vr, loop ? ";L" : "");
}

size_t termgfx_audio_synth(uint8_t **buf, size_t *cap, int slot,
                           const char *shape, int freq, int ms)
{
	size_t need = strlen(shape) + 64;

	if (need > *cap) {
		*buf = realloc(*buf, need);
		*cap = need;
	}
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Synth;S=%d;W=%s;F=%d;T=%d\x1b\\",
	                       slot, shape, freq, ms);
}

size_t termgfx_audio_flush(uint8_t **buf, size_t *cap, int ch, int fade_ms)
{
	if (*cap < 64) {
		*buf = realloc(*buf, 64);
		*cap = 64;
	}
	if (fade_ms > 0)
		return (size_t)sprintf((char *)*buf,
		                       "\x1b_SyncTERM:A;Flush;C=%d;O=%d\x1b\\", ch, fade_ms);
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Flush;C=%d\x1b\\", ch);
}
