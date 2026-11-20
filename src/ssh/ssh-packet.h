/* Private packet extent arithmetic. Model builds compile this same body with
 * smaller unsigned size types; production uses size_t and SIZE_MAX. */
#ifndef DSSH_PACKET_H
#define DSSH_PACKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef DSSH_PACKET_SIZE_TYPE
 #define DSSH_PACKET_SIZE_TYPE size_t
#endif
#ifndef DSSH_PACKET_SIZE_LIMIT
 #define DSSH_PACKET_SIZE_LIMIT SIZE_MAX
#endif
typedef DSSH_PACKET_SIZE_TYPE dssh_packet_size;

/* capacity excludes the sequence prefix. Check the wire value before any
 * size conversion, then reserve the prefix in the complete MAC input size.
 * Failure leaves both outputs unchanged. Packet grammar is checked by callers. */
static inline bool
dssh_packet_extents(uint32_t wire_length, dssh_packet_size capacity,
    dssh_packet_size *packet_total, dssh_packet_size *mac_input)
{
	if (capacity < 4 || wire_length > capacity - 4)
		return false;
	dssh_packet_size length = (dssh_packet_size)wire_length;
	dssh_packet_size total = (dssh_packet_size)(length + 4);
	if (total > DSSH_PACKET_SIZE_LIMIT - 4)
		return false;
	dssh_packet_size input = (dssh_packet_size)(total + 4);
	*packet_total = total;
	*mac_input = input;
	return true;
}

/* A uint32 wire length, four length bytes and a uint16 tag fit uint64 even
 * when packet and tag together cannot be represented in size_t. */
static inline uint64_t
dssh_packet_wire_bytes(uint32_t wire_length, uint16_t tag_size)
{
	uint64_t length = wire_length;
	uint64_t bytes = (uint64_t)(length + 4 + tag_size);
	return bytes;
}

#endif
