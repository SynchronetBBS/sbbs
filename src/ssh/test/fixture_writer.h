/* Bounded construction of actual test packet storage. No allocation or I/O. */
#ifndef DSSH_TEST_FIXTURE_WRITER_H
#define DSSH_TEST_FIXTURE_WRITER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "deucessh.h"

struct fixture_writer {
	uint8_t *data;
	size_t capacity;
	size_t used;
	bool valid;
};

static inline bool
fixture_writer_room(struct fixture_writer *writer, size_t size)
{
	if (!writer->valid || writer->used > writer->capacity
	    || size > writer->capacity - writer->used
	    || (writer->data == NULL && size != 0)) {
		writer->valid = false;
		return false;
	}
	return true;
}

static inline bool
fixture_writer_bytes(struct fixture_writer *writer, const uint8_t *data, size_t size)
{
	if ((data == NULL && size != 0) || !fixture_writer_room(writer, size)) {
		writer->valid = false;
		return false;
	}
	if (size != 0)
		memcpy(writer->data + writer->used, data, size);
	writer->used += size;
	return true;
}

static inline bool
fixture_writer_u32(struct fixture_writer *writer, uint32_t value)
{
	if (!fixture_writer_room(writer, 4))
		return false;
	size_t end = writer->used + 4;
	int ret = dssh_serialize_uint32(value, writer->data, writer->capacity, &writer->used);
	if (ret != 0 || writer->used != end) {
		writer->valid = false;
		return false;
	}
	return true;
}

static inline bool
fixture_writer_prefix(struct fixture_writer *writer, size_t size)
{
	if (size > UINT32_MAX || !fixture_writer_room(writer, 4)
	    || size > writer->capacity - writer->used - 4) {
		writer->valid = false;
		return false;
	}
	uint32_t length = (uint32_t)size;
	return fixture_writer_u32(writer, length);
}

static inline bool
fixture_writer_string(struct fixture_writer *writer, const uint8_t *data, size_t size)
{
	if (data == NULL && size != 0) {
		writer->valid = false;
		return false;
	}
	return fixture_writer_prefix(writer, size) && fixture_writer_bytes(writer, data, size);
}

static inline bool
fixture_writer_repeated_string(struct fixture_writer *writer, uint8_t value, size_t size)
{
	if (!fixture_writer_prefix(writer, size))
		return false;
	if (size != 0)
		memset(writer->data + writer->used, value, size);
	writer->used += size;
	return true;
}

#endif
