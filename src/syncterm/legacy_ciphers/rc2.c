/* Copyright (C), 2026 by Stephen Hurd */
/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * RC2 (RFC 2268) — decrypt-only reference implementation.
 *
 * Used to migrate bbslist.ini files that were encrypted with RC2
 * under the Cryptlib era.  Deliberately naive (table-driven, no
 * unrolling, no SIMD): one-time decrypt speed doesn't matter, and
 * avoiding a real crypto library keeps the shim minimal.
 *
 * Cryptlib set effective-key-bits = keySize * 8 (full strength), so
 * the decrypt entry point derives T1 from the supplied key length.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "xp_crypt.h"

/* PITABLE: 256-entry byte permutation from RFC 2268 s2. */
static const uint8_t PITABLE[256] = {
	0xd9, 0x78, 0xf9, 0xc4, 0x19, 0xdd, 0xb5, 0xed, 0x28, 0xe9, 0xfd, 0x79, 0x4a, 0xa0, 0xd8, 0x9d,
	0xc6, 0x7e, 0x37, 0x83, 0x2b, 0x76, 0x53, 0x8e, 0x62, 0x4c, 0x64, 0x88, 0x44, 0x8b, 0xfb, 0xa2,
	0x17, 0x9a, 0x59, 0xf5, 0x87, 0xb3, 0x4f, 0x13, 0x61, 0x45, 0x6d, 0x8d, 0x09, 0x81, 0x7d, 0x32,
	0xbd, 0x8f, 0x40, 0xeb, 0x86, 0xb7, 0x7b, 0x0b, 0xf0, 0x95, 0x21, 0x22, 0x5c, 0x6b, 0x4e, 0x82,
	0x54, 0xd6, 0x65, 0x93, 0xce, 0x60, 0xb2, 0x1c, 0x73, 0x56, 0xc0, 0x14, 0xa7, 0x8c, 0xf1, 0xdc,
	0x12, 0x75, 0xca, 0x1f, 0x3b, 0xbe, 0xe4, 0xd1, 0x42, 0x3d, 0xd4, 0x30, 0xa3, 0x3c, 0xb6, 0x26,
	0x6f, 0xbf, 0x0e, 0xda, 0x46, 0x69, 0x07, 0x57, 0x27, 0xf2, 0x1d, 0x9b, 0xbc, 0x94, 0x43, 0x03,
	0xf8, 0x11, 0xc7, 0xf6, 0x90, 0xef, 0x3e, 0xe7, 0x06, 0xc3, 0xd5, 0x2f, 0xc8, 0x66, 0x1e, 0xd7,
	0x08, 0xe8, 0xea, 0xde, 0x80, 0x52, 0xee, 0xf7, 0x84, 0xaa, 0x72, 0xac, 0x35, 0x4d, 0x6a, 0x2a,
	0x96, 0x1a, 0xd2, 0x71, 0x5a, 0x15, 0x49, 0x74, 0x4b, 0x9f, 0xd0, 0x5e, 0x04, 0x18, 0xa4, 0xec,
	0xc2, 0xe0, 0x41, 0x6e, 0x0f, 0x51, 0xcb, 0xcc, 0x24, 0x91, 0xaf, 0x50, 0xa1, 0xf4, 0x70, 0x39,
	0x99, 0x7c, 0x3a, 0x85, 0x23, 0xb8, 0xb4, 0x7a, 0xfc, 0x02, 0x36, 0x5b, 0x25, 0x55, 0x97, 0x31,
	0x2d, 0x5d, 0xfa, 0x98, 0xe3, 0x8a, 0x92, 0xae, 0x05, 0xdf, 0x29, 0x10, 0x67, 0x6c, 0xba, 0xc9,
	0xd3, 0x00, 0xe6, 0xcf, 0xe1, 0x9e, 0xa8, 0x2c, 0x63, 0x16, 0x01, 0x3f, 0x58, 0xe2, 0x89, 0xa9,
	0x0d, 0x38, 0x34, 0x1b, 0xab, 0x33, 0xff, 0xb0, 0xbb, 0x48, 0x0c, 0x5f, 0xb9, 0xb1, 0xcd, 0x2e,
	0xc5, 0xf3, 0xdb, 0x47, 0xe5, 0xa5, 0x9c, 0x77, 0x0a, 0xa6, 0x20, 0x68, 0xfe, 0x7f, 0xc1, 0xad,
};

/*
 * Expand an RC2 user key (1..128 bytes) with effective-key-bits T1 into
 * the 64-word (128-byte) mixing key K.  Per RFC 2268 s2.
 */
static void
rc2_expand(const uint8_t *key, size_t T, int T1, uint16_t K[64])
{
	uint8_t L[128];
	int     T8 = (T1 + 7) / 8;
	int     TM = 0xff >> (7 & (-T1));	/* ((1 << (T1 % 8)) - 1) | ((T1 % 8) == 0 ? 0xff : 0) */

	/* RFC: TM = 255 mod 2^(8 + T1 - 8*T8).  Simpler form below. */
	if (T1 % 8 == 0)
		TM = 0xff;
	else
		TM = (1 << (T1 % 8)) - 1;

	for (size_t i = 0; i < T; i++)
		L[i] = key[i];
	for (size_t i = T; i < 128; i++)
		L[i] = PITABLE[(L[i - 1] + L[i - T]) & 0xff];
	L[128 - T8] = PITABLE[L[128 - T8] & TM];
	for (int i = 127 - T8; i >= 0; i--)
		L[i] = PITABLE[L[i + 1] ^ L[i + T8]];

	for (int i = 0; i < 64; i++)
		K[i] = (uint16_t)L[2 * i] | ((uint16_t)L[2 * i + 1] << 8);
}

static inline uint16_t
rotr16(uint16_t v, int n)
{
	return (uint16_t)((v >> n) | (v << (16 - n)));
}

/*
 * RC2 decrypt of one 8-byte block in place.  Matches RFC 2268 s3's
 * reverse schedule: 5 r-mixing, r-mashing, 6 r-mixing, r-mashing,
 * 5 r-mixing.
 */
static void
rc2_decrypt_block(const uint16_t K[64], uint8_t buf[8])
{
	uint16_t R[4];
	for (int i = 0; i < 4; i++)
		R[i] = (uint16_t)buf[2 * i] | ((uint16_t)buf[2 * i + 1] << 8);

	int j = 63;
	for (int stage = 0; stage < 3; stage++) {
		int nrounds = (stage == 1) ? 6 : 5;
		for (int r = 0; r < nrounds; r++) {
			R[3] = rotr16(R[3], 5);
			R[3] = R[3] - K[j--] - (R[2] & R[1]) - ((~R[2]) & R[0]);
			R[2] = rotr16(R[2], 3);
			R[2] = R[2] - K[j--] - (R[1] & R[0]) - ((~R[1]) & R[3]);
			R[1] = rotr16(R[1], 2);
			R[1] = R[1] - K[j--] - (R[0] & R[3]) - ((~R[0]) & R[2]);
			R[0] = rotr16(R[0], 1);
			R[0] = R[0] - K[j--] - (R[3] & R[2]) - ((~R[3]) & R[1]);
		}
		if (stage < 2) {
			/* r-mashing */
			R[3] = R[3] - K[R[2] & 63];
			R[2] = R[2] - K[R[1] & 63];
			R[1] = R[1] - K[R[0] & 63];
			R[0] = R[0] - K[R[3] & 63];
		}
	}

	for (int i = 0; i < 4; i++) {
		buf[2 * i]     = (uint8_t)(R[i] & 0xff);
		buf[2 * i + 1] = (uint8_t)(R[i] >> 8);
	}
}

/*
 * xp_crypt legacy entry point for RC2-CBC decrypt.  Cryptlib stored
 * RC2-encrypted bbslist.ini files with effective_bits = 8 * keylen.
 */
static int
legacy_rc2_decrypt_cbc(int key_bits, const void *key, size_t key_len,
                      const void *iv_in, size_t iv_len,
                      void *buf, size_t n)
{
	(void)key_bits;
	if (key == NULL || iv_in == NULL || buf == NULL)
		return -1;
	if (key_len == 0 || key_len > 128 || iv_len != 8)
		return -1;
	if (n % 8 != 0)
		return -1;

	uint16_t K[64];
	int      T1 = (int)(key_len * 8);
	rc2_expand((const uint8_t *)key, key_len, T1, K);

	uint8_t prev[8];
	uint8_t save[8];
	memcpy(prev, iv_in, 8);
	uint8_t *p = (uint8_t *)buf;
	for (size_t off = 0; off < n; off += 8) {
		memcpy(save, &p[off], 8);
		rc2_decrypt_block(K, &p[off]);
		for (int i = 0; i < 8; i++)
			p[off + i] ^= prev[i];
		memcpy(prev, save, 8);
	}
	return 0;
}

void
legacy_rc2_register(void)
{
	xp_crypt_register_legacy_decrypt(XP_CRYPT_ALGO_RC2, legacy_rc2_decrypt_cbc);
}
