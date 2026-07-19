/* See wren_token.h for the consent-token model rationale. */

#include "wren_token.h"

#include "sha256.h"        /* xpdev: pure-C reference SHA-256 */
#include "xpmap.h"
#include "base64.h"
#include "dirwrap.h"       /* flength */
#include "genwrap.h"       /* xp_randomize / xp_random — fallback RNG */

#ifndef WITHOUT_DEUCESSH
#include "deucessh-crypto.h"   /* dssh_random + dssh_hash_oneshot
                                * for hardware-accelerated SHA-256 */
#endif

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* SHA-256 digest length, used for both the file-content
 * fingerprint and the HMAC tag.  Defined here so the token-format
 * constants don't have to drag the hash header into every TU. */
#define WREN_TOKEN_HASH_LEN SHA256_DIGEST_SIZE

/* Module-static signing key.  Touched only from the main thread
 * (bbslist load) at startup and the Wren VM thread thereafter;
 * single-threaded for practical purposes. */
static bool    g_key_set;
static uint8_t g_key[32];

void
wren_token_set_key(const uint8_t k[32])
{
	memcpy(g_key, k, sizeof(g_key));
	g_key_set = true;
}

bool
wren_token_generate_key(uint8_t out[32])
{
#ifndef WITHOUT_DEUCESSH
	/* Cryptographic source from the SSH backend (OpenSSL or Botan). */
	if (dssh_random(out, 32) == 0)
		return true;
#endif
	/* Best-effort fallback: xpdev's PRNG seeded from /dev/urandom
	 * (when present) or time/PID otherwise.  Not cryptographically
	 * strong against a determined attacker, but the WITHOUT_DEUCESSH
	 * configuration has no SSH/SFTP and the picker tokens are only
	 * a defense against malicious in-VM Wren scripts (which can't
	 * read this key anyway).  The threat we're hedging against is
	 * "trivial token forgery"; xp_random clears that bar. */
	xp_randomize();
	for (int i = 0; i < 32; i++)
		out[i] = (uint8_t)xp_random(256);
	return true;
}

void
wren_token_key_to_hex(const uint8_t key[32], char out[65])
{
	static const char hexdigits[] = "0123456789abcdef";
	for (int i = 0; i < 32; i++) {
		out[i * 2]     = hexdigits[(key[i] >> 4) & 0x0F];
		out[i * 2 + 1] = hexdigits[key[i] & 0x0F];
	}
	out[64] = '\0';
}

static int
hex_nibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

bool
wren_token_key_from_hex(const char *hex, uint8_t out[32])
{
	if (hex == NULL || strlen(hex) != 64)
		return false;
	for (int i = 0; i < 32; i++) {
		int hi = hex_nibble(hex[i * 2]);
		int lo = hex_nibble(hex[i * 2 + 1]);
		if (hi < 0 || lo < 0)
			return false;
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	return true;
}

/* RFC 2104 HMAC-SHA256 over a contiguous data buffer.  SHA-256's
 * block size is 64 bytes (RFC 6234); keys longer than that are
 * pre-hashed, shorter ones are zero-padded.  Always uses xpdev's
 * pure-C SHA-256 — the inputs are ~100 bytes (key + path + file
 * hash), so backend acceleration would buy nothing here.  The
 * file-content hash path below is where DeuceSSH's hardware-
 * accelerated digest matters. */
static void
hmac_sha256(const uint8_t *key, size_t klen,
            const uint8_t *data, size_t dlen,
            uint8_t out[WREN_TOKEN_HASH_LEN])
{
	uint8_t kblock[SHA256_BLOCK_SIZE];
	if (klen > sizeof(kblock)) {
		uint8_t khash[WREN_TOKEN_HASH_LEN];
		SHA256_calc(khash, key, klen);
		memcpy(kblock, khash, WREN_TOKEN_HASH_LEN);
		memset(kblock + WREN_TOKEN_HASH_LEN, 0,
		    sizeof(kblock) - WREN_TOKEN_HASH_LEN);
	}
	else {
		memcpy(kblock, key, klen);
		if (klen < sizeof(kblock))
			memset(kblock + klen, 0, sizeof(kblock) - klen);
	}
	uint8_t ipad[SHA256_BLOCK_SIZE], opad[SHA256_BLOCK_SIZE];
	for (size_t i = 0; i < sizeof(kblock); i++) {
		ipad[i] = kblock[i] ^ 0x36;
		opad[i] = kblock[i] ^ 0x5c;
	}
	SHA256_CTX ctx;
	uint8_t    inner[WREN_TOKEN_HASH_LEN];
	SHA256Init(&ctx);
	SHA256Update(&ctx, ipad, sizeof(ipad));
	SHA256Update(&ctx, data, dlen);
	SHA256Final(&ctx, inner);
	SHA256Init(&ctx);
	SHA256Update(&ctx, opad, sizeof(opad));
	SHA256Update(&ctx, inner, sizeof(inner));
	SHA256Final(&ctx, out);
}

/* Compute SHA-256 of the file at `path` via mmap.  Routes through
 * DeuceSSH's digest backend when available (Intel SHA-NI / ARM
 * crypto extensions, via OpenSSL or Botan) — typical 10-50×
 * speedup over the pure-C path on hardware with crypto extensions.
 * Falls back to xpdev's reference SHA-256 in WITHOUT_DEUCESSH
 * builds (correct, just slow on multi-GB files). */
static bool
file_sha256(const char *path, uint8_t out[WREN_TOKEN_HASH_LEN])
{
	off_t sz = flength(path);
	if (sz < 0)
		return false;
	if (sz == 0) {
		SHA256_calc(out, "", 0);
		return true;
	}
	struct xpmapping *map = xpmap(path, XPMAP_READ);
	if (map == NULL)
		return false;
#ifndef WITHOUT_DEUCESSH
	int rc = dssh_hash_oneshot("SHA-256", map->addr, map->size,
	    out, WREN_TOKEN_HASH_LEN);
	xpunmap(map);
	return rc == 0;
#else
	SHA256_calc(out, map->addr, map->size);
	xpunmap(map);
	return true;
#endif
}

/* Constant-time byte compare.  Returns 0 on match. */
static int
ct_memcmp(const uint8_t *a, const uint8_t *b, size_t n)
{
	uint8_t diff = 0;
	for (size_t i = 0; i < n; i++)
		diff |= a[i] ^ b[i];
	return diff;
}

/* Token binary layout:
 *   |  4 bytes BE: path_len (P)               |
 *   |  P bytes:    path (UTF-8, no NUL)       |
 *   | 32 bytes:    file SHA-256 at sign time  |
 *   | 32 bytes:    HMAC-SHA256 over the above |
 * Then base64-encoded for Wren-side opacity. */
#define TOKEN_HEADER_LEN  4
#define TOKEN_FHASH_LEN   WREN_TOKEN_HASH_LEN
#define TOKEN_HMAC_LEN    WREN_TOKEN_HASH_LEN
#define TOKEN_FIXED_LEN   (TOKEN_HEADER_LEN + TOKEN_FHASH_LEN + TOKEN_HMAC_LEN)

char *
wren_token_sign(const char *path)
{
	if (!g_key_set || path == NULL)
		return NULL;
	size_t plen = strlen(path);
	/* Cap path length so the BE-encoded length fits 32 bits and
	 * the resulting binary token doesn't overflow size_t in the
	 * base64 sizing math. */
	if (plen == 0 || plen > 65535)
		return NULL;
	uint8_t fhash[TOKEN_FHASH_LEN];
	if (!file_sha256(path, fhash))
		return NULL;
	size_t binlen = TOKEN_FIXED_LEN + plen;
	uint8_t *bin = malloc(binlen);
	if (bin == NULL)
		return NULL;
	uint32_t plen32 = (uint32_t)plen;
	bin[0] = (uint8_t)(plen32 >> 24);
	bin[1] = (uint8_t)(plen32 >> 16);
	bin[2] = (uint8_t)(plen32 >> 8);
	bin[3] = (uint8_t)plen32;
	memcpy(bin + TOKEN_HEADER_LEN, path, plen);
	memcpy(bin + TOKEN_HEADER_LEN + plen, fhash, TOKEN_FHASH_LEN);
	hmac_sha256(g_key, sizeof(g_key),
	    bin, TOKEN_HEADER_LEN + plen + TOKEN_FHASH_LEN,
	    bin + TOKEN_HEADER_LEN + plen + TOKEN_FHASH_LEN);
	/* base64 size: 4 chars per 3 input bytes (rounded up), +1 NUL. */
	size_t b64len = 4 * ((binlen + 2) / 3) + 1;
	char *out = malloc(b64len);
	if (out == NULL) {
		free(bin);
		return NULL;
	}
	ssize_t enc = b64_encode(out, b64len, (const char *)bin, binlen);
	free(bin);
	if (enc < 0) {
		free(out);
		return NULL;
	}
	return out;
}

char *
wren_token_verify(const char *token)
{
	if (!g_key_set || token == NULL)
		return NULL;
	size_t tlen = strlen(token);
	if (tlen == 0)
		return NULL;
	/* Decoded size is at most ceil(tlen * 3 / 4); pad a bit. */
	size_t binmax = (tlen / 4) * 3 + 3;
	uint8_t *bin = malloc(binmax);
	if (bin == NULL)
		return NULL;
	ssize_t dec = b64_decode((char *)bin, binmax, token, tlen);
	if (dec < (ssize_t)TOKEN_FIXED_LEN) {
		free(bin);
		return NULL;
	}
	uint32_t plen32 = ((uint32_t)bin[0] << 24) |
	                  ((uint32_t)bin[1] << 16) |
	                  ((uint32_t)bin[2] << 8) |
	                  (uint32_t)bin[3];
	if ((size_t)dec != (size_t)TOKEN_FIXED_LEN + plen32) {
		free(bin);
		return NULL;
	}
	/* HMAC verify (constant-time). */
	uint8_t expected[TOKEN_HMAC_LEN];
	hmac_sha256(g_key, sizeof(g_key),
	    bin, TOKEN_HEADER_LEN + plen32 + TOKEN_FHASH_LEN, expected);
	if (ct_memcmp(expected,
	        bin + TOKEN_HEADER_LEN + plen32 + TOKEN_FHASH_LEN,
	        TOKEN_HMAC_LEN) != 0) {
		free(bin);
		return NULL;
	}
	/* Reject embedded NULs in the encoded path — those have no
	 * legitimate filesystem meaning and could confuse callers. */
	for (uint32_t i = 0; i < plen32; i++) {
		if (bin[TOKEN_HEADER_LEN + i] == 0) {
			free(bin);
			return NULL;
		}
	}
	char *path = malloc((size_t)plen32 + 1);
	if (path == NULL) {
		free(bin);
		return NULL;
	}
	memcpy(path, bin + TOKEN_HEADER_LEN, plen32);
	path[plen32] = '\0';
	/* File-content SHA-256 must still match — this is the
	 * "consent was for this content" check. */
	uint8_t cur_hash[TOKEN_FHASH_LEN];
	if (!file_sha256(path, cur_hash)) {
		free(path);
		free(bin);
		return NULL;
	}
	if (ct_memcmp(cur_hash, bin + TOKEN_HEADER_LEN + plen32,
	        TOKEN_FHASH_LEN) != 0) {
		free(path);
		free(bin);
		return NULL;
	}
	free(bin);
	return path;
}
