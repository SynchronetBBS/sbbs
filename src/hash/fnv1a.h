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

#ifndef FNV1A_H_
#define FNV1A_H_

#include <stddef.h>     /* size_t */
#include <stdint.h>     /* uint32_t, uint64_t */

/* The FNV-1a offset bases: the value an empty input hashes to, and the seed a
 * fnv1a*_update() chain starts from. */
#define FNV1A32_INIT 0x811c9dc5u
#define FNV1A64_INIT 0xcbf29ce484222325ull

#ifdef __cplusplus
extern "C" {
#endif

/* Hash `len` bytes. Binary-safe: an embedded NUL is data, not a terminator.
 *
 * FNV-1a is small, table-free and needs no setup, which makes it a good fit for
 * short keys and for content-addressing where the cost of the hash matters more
 * than its strength. It is NOT cryptographic and NOT collision-resistant: use it
 * for cache keys, change-detection signatures and hash-table indexes, and use
 * sha1/sha256 (or at least md5) where an adversary might choose the input.
 *
 * Note that FNV's multiply carries bits upward only, so its LOW bits are its
 * weakest. To index a power-of-2 table, xor-fold rather than masking:
 * `(h >> n) ^ (h & ((1 << n) - 1))` distributes far better than `h & mask`. */
uint32_t fnv1a32(const void* buf, size_t len);

/* Same hash, resumable: folds `len` more bytes into a running `hash`. Start a
 * chain at FNV1A32_INIT. Chaining is exact -- hashing a buffer in any number of
 * pieces equals fnv1a32() over the whole thing -- so a caller whose input is not
 * one contiguous buffer need not marshal it into a scratch buffer first. */
uint32_t fnv1a32_update(uint32_t hash, const void* buf, size_t len);

/* Same hash over a NUL-terminated string. The terminator is not hashed, so this
 * equals fnv1a32(str, strlen(str)) exactly -- in a single pass. */
uint32_t fnv1a32_str(const char* str);

/* The 64-bit width: identical construction, wider accumulator and its own pair
 * of FNV constants (the two widths are NOT related -- fnv1a32() is not fnv1a64()
 * truncated).
 *
 * Which to use is a question of how many distinct keys you expect. By the
 * birthday bound a 32-bit hash is more likely than not to collide somewhere once
 * you pass ~77,000 keys, and is already at ~1-in-2000 odds at 3,000. Reach for
 * the 64-bit width whenever the key set is open-ended (a file base, a message
 * base, a user-supplied corpus) or a collision would be silent rather than
 * merely inefficient; 32 bits is fine for a bounded, small set whose collisions
 * are detected or harmless. */
uint64_t fnv1a64(const void* buf, size_t len);
uint64_t fnv1a64_update(uint64_t hash, const void* buf, size_t len);
uint64_t fnv1a64_str(const char* str);

#ifdef __cplusplus
}
#endif

#endif  /* Don't add anything after this line */
