/* Same production arithmetic with unsigned size models; this is not a
 * different ABI or a claim to execute the whole library on a narrow host. */
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "ssh-packet.h"

#ifdef DSSH_PACKET_MODEL_BITS
_Static_assert(sizeof(dssh_packet_size) * CHAR_BIT == DSSH_PACKET_MODEL_BITS,
    "packet model must use the requested unsigned width");
#endif

static uint64_t checks;
static uint64_t failed;

static void
check_extent(uint32_t wire, dssh_packet_size capacity)
{
	/* The oracle adds in uintmax_t before comparing; the helper bounds the
	 * wire value before conversion/addition in the selected size type. */
	uintmax_t expected_total = wire;
	expected_total += 4;
	uintmax_t expected_input = expected_total + 4;
	uintmax_t maximum = DSSH_PACKET_SIZE_LIMIT;
	bool fits = expected_total <= capacity && expected_input <= maximum;
	dssh_packet_size total = DSSH_PACKET_SIZE_LIMIT;
	dssh_packet_size input = DSSH_PACKET_SIZE_LIMIT;
	bool result = dssh_packet_extents(wire, capacity, &total, &input);
	bool valid = result == fits;
	if (fits)
		valid = valid && total == expected_total && input == expected_input;
	else
		valid = valid && total == DSSH_PACKET_SIZE_LIMIT && input == DSSH_PACKET_SIZE_LIMIT;
	/* Fixed tables and the exhaustive 16-bit loop make fewer than 2^32 calls. */
	checks++;
	if (!valid) {
		if (failed == 0) {
			uintmax_t reported_capacity = capacity;
			fprintf(stderr, "extent wire=%" PRIu32 " capacity=%" PRIuMAX " result=%u expected=%u\n",
			    wire, reported_capacity, result ? 1U : 0U, fits ? 1U : 0U);
		}
		failed++;
	}
}

int
main(void)
{
	const uintmax_t capacities[] = {
		0, 1, 3, 4, 5, 7, 8, 15, 16, 17, 33280,
		UINT16_MAX - 8U, UINT16_MAX - 4U, UINT16_MAX,
		UINT32_C(65536), UINT32_MAX - 8U, UINT32_MAX - 4U, UINT32_MAX,
		UINT64_C(4294967296), DSSH_PACKET_SIZE_LIMIT - 4U, DSSH_PACKET_SIZE_LIMIT
	};
	const uint32_t wire[] = {
		0, 1, 2, 3, 4, 7, 8, 11, 12, 13, 33275, 33276, 33277,
		UINT16_MAX - 12U, UINT16_MAX - 8U, UINT16_MAX - 7U,
		UINT16_MAX - 4U, UINT16_MAX - 3U, UINT16_MAX, UINT32_C(65536),
		UINT32_C(65548), UINT32_MAX - 8U, UINT32_MAX - 4U, UINT32_MAX
	};
	const uint16_t tags[] = {0, 1, 16, 32, 64, 128, UINT16_MAX};
	for (size_t c = 0; c < sizeof(capacities) / sizeof(capacities[0]); c++) {
		if (capacities[c] > DSSH_PACKET_SIZE_LIMIT)
			continue;
		dssh_packet_size capacity = (dssh_packet_size)capacities[c];
		for (size_t i = 0; i < sizeof(wire) / sizeof(wire[0]); i++)
			check_extent(wire[i], capacity);
	}
	/* Every 16-bit wire value at the actual narrow session capacity, plus
	 * over-width values in the fixed table. The loop index cannot wrap. */
	for (uint32_t value = 0; value <= UINT16_MAX; value++)
		check_extent(value, UINT16_MAX - 4U);
	for (size_t i = 0; i < sizeof(wire) / sizeof(wire[0]); i++) {
		for (size_t j = 0; j < sizeof(tags) / sizeof(tags[0]); j++) {
			uintmax_t expected = wire[i];
			expected += 4;
			expected += tags[j];
			uint64_t actual = dssh_packet_wire_bytes(wire[i], tags[j]);
			checks++;
			if (actual != expected)
				failed++;
		}
	}
	unsigned width = (unsigned)(sizeof(dssh_packet_size) * CHAR_BIT);
	int printed = printf("packet_sizes model=%s width=%u checks=%" PRIu64 " failed=%" PRIu64 "\n",
	    DSSH_PACKET_MODEL_NAME, width, checks, failed);
	return failed == 0 && checks != 0 && printed >= 0 ? 0 : 1;
}
