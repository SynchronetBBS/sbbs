/*
 * SHA-256 in C
 * Reference implementation of FIPS PUB 180-4.
 * 100% Public Domain.
 *
 * Test Vectors (from FIPS PUB 180-2):
 *   "abc"
 *     BA7816BF 8F01CFEA 414140DE 5DAE2223
 *     B00361A3 96177A9C B410FF61 F20015AD
 *   "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 *     248D6A61 D20638B8 E5C02693 0C3E6039
 *     A33CE459 64FF2167 F6ECEDD4 19DB06C1
 *   one million 'a's:
 *     CDC76E5C 9914FB92 81A1C7E2 84D73E67
 *     F1809A48 A497200E 046D39CC C7112CD0
 */

#include "sha256.h"

#include <string.h>

/* Round constants K[0..63] — first 32 bits of the fractional parts
 * of the cube roots of the first 64 primes (FIPS 180-4 §4.2.2). */
static const uint32_t K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n)   (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x)     (ROTR(x, 2)  ^ ROTR(x, 13) ^ ROTR(x, 22))
#define BSIG1(x)     (ROTR(x, 6)  ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SSIG0(x)     (ROTR(x, 7)  ^ ROTR(x, 18) ^ ((x) >> 3))
#define SSIG1(x)     (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static void
SHA256Transform(uint32_t state[8], const uint8_t block[SHA256_BLOCK_SIZE])
{
	uint32_t W[64];
	uint32_t a, b, c, d, e, f, g, h, t1, t2;
	int      t;

	for (t = 0; t < 16; t++) {
		W[t] = ((uint32_t)block[t * 4] << 24)
		     | ((uint32_t)block[t * 4 + 1] << 16)
		     | ((uint32_t)block[t * 4 + 2] << 8)
		     | (uint32_t)block[t * 4 + 3];
	}
	for (t = 16; t < 64; t++)
		W[t] = SSIG1(W[t - 2]) + W[t - 7]
		     + SSIG0(W[t - 15]) + W[t - 16];

	a = state[0]; b = state[1]; c = state[2]; d = state[3];
	e = state[4]; f = state[5]; g = state[6]; h = state[7];

	for (t = 0; t < 64; t++) {
		t1 = h + BSIG1(e) + CH(e, f, g) + K[t] + W[t];
		t2 = BSIG0(a) + MAJ(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void
SHA256Init(SHA256_CTX *ctx)
{
	/* Initial hash values H0..H7 — first 32 bits of the fractional
	 * parts of the square roots of the first 8 primes (FIPS 180-4 §5.3.3). */
	ctx->state[0] = 0x6a09e667;
	ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372;
	ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f;
	ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab;
	ctx->state[7] = 0x5be0cd19;
	ctx->count    = 0;
}

void
SHA256Update(SHA256_CTX *ctx, const void *data, size_t len)
{
	const uint8_t *in      = (const uint8_t *)data;
	size_t         buf_off = (size_t)(ctx->count % SHA256_BLOCK_SIZE);

	ctx->count += len;
	if (buf_off > 0) {
		size_t fill = SHA256_BLOCK_SIZE - buf_off;
		if (len < fill) {
			memcpy(ctx->buffer + buf_off, in, len);
			return;
		}
		memcpy(ctx->buffer + buf_off, in, fill);
		SHA256Transform(ctx->state, ctx->buffer);
		in  += fill;
		len -= fill;
	}
	while (len >= SHA256_BLOCK_SIZE) {
		SHA256Transform(ctx->state, in);
		in  += SHA256_BLOCK_SIZE;
		len -= SHA256_BLOCK_SIZE;
	}
	if (len > 0)
		memcpy(ctx->buffer, in, len);
}

void
SHA256Final(SHA256_CTX *ctx, uint8_t digest[SHA256_DIGEST_SIZE])
{
	uint64_t bit_count = ctx->count * 8;
	size_t   buf_off   = (size_t)(ctx->count % SHA256_BLOCK_SIZE);
	int      i;

	/* Append 0x80, pad with zeros to leave 8 bytes for the length. */
	ctx->buffer[buf_off++] = 0x80;
	if (buf_off > SHA256_BLOCK_SIZE - 8) {
		if (buf_off < SHA256_BLOCK_SIZE)
			memset(ctx->buffer + buf_off, 0, SHA256_BLOCK_SIZE - buf_off);
		SHA256Transform(ctx->state, ctx->buffer);
		buf_off = 0;
	}
	memset(ctx->buffer + buf_off, 0, (SHA256_BLOCK_SIZE - 8) - buf_off);
	/* 64-bit BE bit count. */
	for (i = 0; i < 8; i++)
		ctx->buffer[SHA256_BLOCK_SIZE - 8 + i] =
		    (uint8_t)(bit_count >> (56 - i * 8));
	SHA256Transform(ctx->state, ctx->buffer);
	/* Emit state in BE. */
	for (i = 0; i < 8; i++) {
		digest[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
		digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
		digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
		digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
	}
	memset(ctx, 0, sizeof(*ctx));
}

void
SHA256_calc(uint8_t *hash_out, const void *data, size_t len)
{
	SHA256_CTX ctx;
	SHA256Init(&ctx);
	SHA256Update(&ctx, data, len);
	SHA256Final(&ctx, hash_out);
}

char *
SHA256_hex(char *to, const uint8_t digest[SHA256_DIGEST_SIZE])
{
	static const char hexdigits[] = "0123456789abcdef";
	char *d = to;
	int   i;
	for (i = 0; i < SHA256_DIGEST_SIZE; i++) {
		*d++ = hexdigits[(digest[i] >> 4) & 0x0F];
		*d++ = hexdigits[digest[i] & 0x0F];
	}
	*d = '\0';
	return to;
}
