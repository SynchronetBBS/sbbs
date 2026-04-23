/* Copyright (C), 2026 by Stephen Hurd */
/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * IDEA — decrypt-only reference implementation.
 *
 * Used to migrate bbslist.ini files that were encrypted with IDEA
 * under the Cryptlib era.  128-bit key, 8-byte block.  Operations
 * live in three groups: XOR on 16-bit words, addition mod 2^16, and
 * multiplication in (Z/(2^16+1))^* with 0 encoding 2^16.
 *
 * The cipher algorithm itself is unpatented as of 2012.  This file
 * is deliberately naive: one-time decrypt speed doesn't matter.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "xp_crypt.h"

/* Multiplication in (Z/65537)^*; 0 denotes 2^16. */
static inline uint16_t
idea_mul(uint16_t a, uint16_t b)
{
	if (a == 0)
		return (uint16_t)((0x10001u - b) & 0xffff);
	if (b == 0)
		return (uint16_t)((0x10001u - a) & 0xffff);
	uint32_t p  = (uint32_t)a * b;
	uint32_t hi = p >> 16;
	uint32_t lo = p & 0xffff;
	uint32_t r  = (lo >= hi) ? (lo - hi) : (lo - hi + 0x10001u);
	return (uint16_t)(r & 0xffff);		/* 2^16 encoded as 0 */
}

/* Multiplicative inverse in (Z/65537)^*. */
static uint16_t
idea_mul_inv(uint16_t x)
{
	if (x <= 1)
		return x;		/* inv(0)=0 (2^16 self-inverse since -1*-1=1), inv(1)=1 */
	int64_t t0 = 1, t1 = 0;
	int64_t a = (int64_t)x, b = 0x10001;
	while (a != 0) {
		int64_t q = b / a;
		int64_t r = b - q * a;
		int64_t t = t1 - q * t0;
		b = a; a = r;
		t1 = t0; t0 = t;
	}
	if (t1 < 0)
		t1 += 0x10001;
	return (uint16_t)(t1 & 0xffff);		/* 2^16 encoded as 0 */
}

/*
 * Expand 128-bit user key into 52 16-bit encryption subkeys.
 * First 8 from the key; each subsequent batch of 8 from a 25-bit
 * left rotation of the 128-bit state.
 */
static void
idea_expand_encrypt(const uint8_t key[16], uint16_t EK[52])
{
	uint16_t K[8];
	for (int i = 0; i < 8; i++)
		K[i] = ((uint16_t)key[2 * i] << 8) | key[2 * i + 1];

	int ek = 0;
	for (;;) {
		for (int j = 0; j < 8 && ek < 52; j++)
			EK[ek++] = K[j];
		if (ek >= 52)
			break;
		/* Rotate 128-bit K left by 25 bits = rotate by 16 words, then 9 bits. */
		uint16_t tmp[8];
		for (int j = 0; j < 8; j++)
			tmp[j] = K[(j + 1) & 7];
		for (int j = 0; j < 8; j++)
			K[j] = (uint16_t)((tmp[j] << 9) | (tmp[(j + 1) & 7] >> 7));
	}
}

/*
 * Invert encryption subkeys to produce 52 decryption subkeys DK such
 * that running the encryption routine with DK decrypts.  Follows the
 * classical PGP-era derivation (Plumb): mul keys become mul_inv, add
 * keys negate, MA keys are reused as-is; middle rounds swap the two
 * add keys, first and last rounds do not.
 */
static void
idea_invert_schedule(const uint16_t EK[52], uint16_t DK[52])
{
	uint16_t t1, t2, t3;
	int p  = 52;		/* write cursor, grows downward */
	int ek = 0;

#define PUT(v) do { DK[--p] = (uint16_t)(v); } while (0)

	/* First (topmost) decryption output transform: inverts enc round 1's initial mul/add. */
	t1 = idea_mul_inv(EK[ek++]);
	t2 = (uint16_t)(-EK[ek++]);
	t3 = (uint16_t)(-EK[ek++]);
	PUT(idea_mul_inv(EK[ek++]));
	PUT(t3);
	PUT(t2);
	PUT(t1);

	/* 7 middle decryption rounds — add keys swap. */
	for (int i = 0; i < 7; i++) {
		t1 = EK[ek++];
		PUT(EK[ek++]);		/* MA2 */
		PUT(t1);		/* MA1 */

		t1 = idea_mul_inv(EK[ek++]);
		t2 = (uint16_t)(-EK[ek++]);
		t3 = (uint16_t)(-EK[ek++]);
		PUT(idea_mul_inv(EK[ek++]));
		PUT(t2);		/* swapped */
		PUT(t3);
		PUT(t1);
	}

	/* Final (bottommost) decryption round — add keys do NOT swap. */
	t1 = EK[ek++];
	PUT(EK[ek++]);
	PUT(t1);

	t1 = idea_mul_inv(EK[ek++]);
	t2 = (uint16_t)(-EK[ek++]);
	t3 = (uint16_t)(-EK[ek++]);
	PUT(idea_mul_inv(EK[ek++]));
	PUT(t3);
	PUT(t2);
	PUT(t1);

#undef PUT
}

/*
 * Core round engine: 8 rounds + output transform, consuming 52 subkeys.
 * Invoked with decryption subkeys to decrypt.
 */
static void
idea_crypt_block(const uint16_t K[52], uint8_t buf[8])
{
	uint16_t x1 = ((uint16_t)buf[0] << 8) | buf[1];
	uint16_t x2 = ((uint16_t)buf[2] << 8) | buf[3];
	uint16_t x3 = ((uint16_t)buf[4] << 8) | buf[5];
	uint16_t x4 = ((uint16_t)buf[6] << 8) | buf[7];

	const uint16_t *k = K;
	for (int r = 0; r < 8; r++) {
		uint16_t s2, s3;
		x1 = idea_mul(x1, *k++);
		x2 = (uint16_t)(x2 + *k++);
		x3 = (uint16_t)(x3 + *k++);
		x4 = idea_mul(x4, *k++);

		s3 = x3;
		x3 = idea_mul((uint16_t)(x3 ^ x1), *k++);		/* t1_internal */
		s2 = x2;
		x2 = idea_mul((uint16_t)((x2 ^ x4) + x3), *k++);	/* t2 */
		x3 = (uint16_t)(x3 + x2);				/* t1 */

		x1 = (uint16_t)(x1 ^ x2);
		x4 = (uint16_t)(x4 ^ x3);
		x2 = (uint16_t)(x2 ^ s3);
		x3 = (uint16_t)(x3 ^ s2);
	}

	/* Output transform; note x3/x2 swap in the add-key application is implicit
	 * via writing x3 to the 2nd slot and x2 to the 3rd. */
	x1 = idea_mul(x1, *k++);
	x3 = (uint16_t)(x3 + *k++);
	x2 = (uint16_t)(x2 + *k++);
	x4 = idea_mul(x4, *k);

	buf[0] = (uint8_t)(x1 >> 8); buf[1] = (uint8_t)(x1 & 0xff);
	buf[2] = (uint8_t)(x3 >> 8); buf[3] = (uint8_t)(x3 & 0xff);
	buf[4] = (uint8_t)(x2 >> 8); buf[5] = (uint8_t)(x2 & 0xff);
	buf[6] = (uint8_t)(x4 >> 8); buf[7] = (uint8_t)(x4 & 0xff);
}

/* xp_crypt legacy entry point for IDEA-CBC decrypt. */
static int
legacy_idea_decrypt_cbc(int key_bits, const void *key, size_t key_len,
                        const void *iv_in, size_t iv_len,
                        void *buf, size_t n)
{
	(void)key_bits;
	if (key == NULL || iv_in == NULL || buf == NULL)
		return -1;
	if (key_len != 16 || iv_len != 8)
		return -1;
	if (n % 8 != 0)
		return -1;

	uint16_t EK[52], DK[52];
	idea_expand_encrypt((const uint8_t *)key, EK);
	idea_invert_schedule(EK, DK);

	uint8_t prev[8];
	uint8_t save[8];
	memcpy(prev, iv_in, 8);
	uint8_t *p = (uint8_t *)buf;
	for (size_t off = 0; off < n; off += 8) {
		memcpy(save, &p[off], 8);
		idea_crypt_block(DK, &p[off]);
		for (int i = 0; i < 8; i++)
			p[off + i] ^= prev[i];
		memcpy(prev, save, 8);
	}
	return 0;
}

void
legacy_idea_register(void)
{
	xp_crypt_register_legacy_decrypt(XP_CRYPT_ALGO_IDEA, legacy_idea_decrypt_cbc);
}
