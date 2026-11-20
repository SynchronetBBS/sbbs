#ifndef DSSH_TEST_RSA_CACHE_REFERENCE_H
#define DSSH_TEST_RSA_CACHE_REFERENCE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static inline bool
rsa_cache_ref_field(uint8_t *out, size_t capacity, size_t *pos,
    const uint8_t *data, size_t count, bool integer)
{
	bool pad = integer && count != 0 && data[0] >= 128;
	if (*pos > capacity || capacity - *pos < 4 || count > UINT32_MAX - (pad ? 1U : 0U)
	    || count > capacity - *pos - 4 || (pad && count == capacity - *pos - 4))
		return false;
	uint32_t wire = (uint32_t)(count + (pad ? 1U : 0U));
	for (unsigned int i = 0; i < 4; i++) {
		uint8_t byte = (uint8_t)((wire >> (24 - 8 * i)) & UINT32_C(255));
		out[(*pos)++] = byte;
	}
	if (pad)
		out[(*pos)++] = 0;
	if (count != 0)
		memcpy(out + *pos, data, count);
	*pos += count;
	return true;
}

static inline bool
rsa_cache_reference(uint8_t *out, size_t capacity, const uint8_t *e,
    size_t esize, const uint8_t *n, size_t nsize, size_t *length)
{
	const uint8_t name[] = {'s', 's', 'h', '-', 'r', 's', 'a'};
	size_t pos = 0;
	if (!rsa_cache_ref_field(out, capacity, &pos, name, sizeof(name), false)
	    || !rsa_cache_ref_field(out, capacity, &pos, e, esize, true)
	    || !rsa_cache_ref_field(out, capacity, &pos, n, nsize, true))
		return false;
	*length = pos;
	return true;
}
#endif
