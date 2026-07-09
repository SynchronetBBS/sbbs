#include "hash.h"

uint32_t
termgfx_fnv1a(const void *data, size_t len)
{
	const uint8_t *p = (const uint8_t *)data;
	uint32_t       h = 0x811c9dc5u;          // FNV offset basis
	size_t         i;

	for (i = 0; i < len; ++i) {
		h ^= p[i];
		h *= 0x01000193u;                    // FNV prime
	}
	return h;
}
