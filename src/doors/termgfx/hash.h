#ifndef TERMGFX_HASH_H_
#define TERMGFX_HASH_H_

#include <stddef.h>
#include <stdint.h>

// 32-bit FNV-1a over `len` bytes. Binary-safe (embedded NULs are data).
//
// This is the hash the doors already content-address their SyncTERM client-cache
// names with: syncdoom/i_termmusic.c builds "d_%08x" from it, and syncduke does
// the same for its MIDI tracks. It lives here so a third door does not copy it
// again. Not a security hash -- it is a cache key: identical bytes must yield
// identical names, across sessions and across hosts.
uint32_t termgfx_fnv1a(const void *data, size_t len);

#endif // TERMGFX_HASH_H_
