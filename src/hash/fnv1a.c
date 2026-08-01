/* Fowler/Noll/Vo (FNV-1a) 32-bit non-cryptographic hash */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* FNV-1a, not FNV-1: the byte is xor'd into the accumulator BEFORE the multiply,
 * not after. That ordering is the whole difference between the two variants, and
 * it is why 1a is the one to use -- in FNV-1 the final byte only ever reaches the
 * low 8 bits of the result, so short low-entropy keys (i.e. text) diffuse poorly.
 * Reference: http://www.isthe.com/chongo/tech/comp/fnv/ */

#include "fnv1a.h"

#define FNV1A32_PRIME 0x01000193u           /* 16777619 == 2^24 + 2^8 + 0x93 */
#define FNV1A64_PRIME 0x100000001b3ull      /* 1099511628211 == 2^40 + 2^8 + 0xb3 */

uint32_t fnv1a32_update(uint32_t hash, const void* buf, size_t len)
{
	const uint8_t* p = (const uint8_t*)buf;
	size_t         i;

	for (i = 0; i < len; i++) {
		hash ^= p[i];
		hash *= FNV1A32_PRIME;
	}
	return hash;
}

uint32_t fnv1a32(const void* buf, size_t len)
{
	return fnv1a32_update(FNV1A32_INIT, buf, len);
}

/* Not fnv1a32(str, strlen(str)): one pass over the string instead of two, which
 * is worth having where this is called per short string in a hot loop (e.g. the
 * doors' text renderer signs every character cell with it, every frame). */
uint32_t fnv1a32_str(const char* str)
{
	uint32_t hash = FNV1A32_INIT;

	while (*str != '\0') {
		hash ^= (uint8_t)*str++;
		hash *= FNV1A32_PRIME;
	}
	return hash;
}

uint64_t fnv1a64_update(uint64_t hash, const void* buf, size_t len)
{
	const uint8_t* p = (const uint8_t*)buf;
	size_t         i;

	for (i = 0; i < len; i++) {
		hash ^= p[i];
		hash *= FNV1A64_PRIME;
	}
	return hash;
}

uint64_t fnv1a64(const void* buf, size_t len)
{
	return fnv1a64_update(FNV1A64_INIT, buf, len);
}

uint64_t fnv1a64_str(const char* str)
{
	uint64_t hash = FNV1A64_INIT;

	while (*str != '\0') {
		hash ^= (uint8_t)*str++;
		hash *= FNV1A64_PRIME;
	}
	return hash;
}
