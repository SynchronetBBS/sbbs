/* Private integration snapshot validation. Refusal preserves both outputs. */
#ifndef SELFTEST_METADATA_H
#define SELFTEST_METADATA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static bool
selftest_snapshot_id(const uint8_t *source, size_t length,
    uint8_t *destination, size_t capacity, size_t *used)
{
	if (source == NULL || destination == NULL || used == NULL
	    || length == 0 || length > capacity)
		return false;
	memcpy(destination, source, length);
	*used = length;
	return true;
}
#endif
