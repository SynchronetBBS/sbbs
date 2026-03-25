// RFC-4251

#ifndef DSSH_ARCH_H
#define DSSH_ARCH_H

#ifndef DSSH_H
 #error Only include deucessh.h, do not directly include this file.
#endif

#include <inttypes.h>
#include <openssl/bn.h>
#include <stdbool.h>

#include "deucessh-portable.h"

// SSH Data Types (internal representations)
typedef uint8_t dssh_byte;
typedef bool dssh_boolean;
typedef uint32_t dssh_uint32_t;
typedef uint64_t dssh_uint64_t;
typedef struct dssh_string_s {
	const dssh_byte *value;
	dssh_uint32_t    length;
} *dssh_string;
typedef struct dssh_string_s *dssh_bytearray;
typedef struct dssh_string_s *dssh_mpint;
typedef struct dssh_namelist_s {
	const dssh_byte *value;
	dssh_uint32_t    length;
	dssh_uint32_t    next;
} *dssh_namelist;

#define dssh_parse(buf, bufsz, val) _Generic(val, \
	    dssh_byte: \
	    dssh_parse_byte(buf, bufsz, val), \
	    dssh_bytearray: \
	    dssh_parse_bytearray(buf, bufsz, val), \
	    dssh_boolean: \
	    dssh_parse_boolean(buf, bufsz, val), \
	    dssh_uint32_t: \
	    dssh_parse_uint32(buf, bufsz, val), \
	    dssh_uint64_t: \
	    dssh_parse_uint64(buf, bufsz, val), \
	    dssh_string: \
	    dssh_parse_string(buf, bufsz, val), \
	    dssh_mpint: \
	    dssh_parse_mpint(buf, bufsz, val), \
	    dssh_namelist: \
	    dssh_parse_namelist(buf, bufsz, val))

#define dssh_serialized_length(val) _Generic(val, \
	    dssh_byte: \
	    dssh_serialized_byte_length(val), \
	    dssh_bytearray: \
	    dssh_serialized_byte_length(val), \
	    dssh_boolean: \
	    dssh_serialized_boolean_length(val), \
	    dssh_uint32_t: \
	    dssh_serialized_uint32_length(val), \
	    dssh_uint64_t: \
	    dssh_serialized_uint64_length(val), \
	    dssh_string: \
	    dssh_serialized_string_length(val), \
	    dssh_mpint: \
	    dssh_serialized_mpint_length(val), \
	    dssh_namelist: \
	    dssh_serialized_namelist_length(val))

#define dssh_serialize(val, buf, bufsz, pos) _Generic(val, \
	    dssh_byte: \
	    dssh_serialize_byte(val, buf, bufsz, pos), \
	    dssh_bytearray: \
	    dssh_serialize_byte(val, buf, bufsz, pos), \
	    dssh_boolean: \
	    dssh_serialize_boolean(val, buf, bufsz, pos), \
	    dssh_uint32_t: \
	    dssh_serialize_uint32(val, buf, bufsz, pos), \
	    dssh_uint64_t: \
	    dssh_serialize_uint64(val, buf, bufsz, pos), \
	    dssh_string: \
	    dssh_serialize_string(val, buf, bufsz, pos), \
	    dssh_mpint: \
	    dssh_serialize_mpint(val, buf, bufsz, pos), \
	    dssh_namelist: \
	    dssh_serialize_namelist(val, buf, bufsz, pos))
int64_t dssh_parse_byte(const uint8_t *buf, size_t bufsz, dssh_byte *val);
size_t dssh_serialized_byte_length(dssh_byte val);
int dssh_serialize_byte(dssh_byte val, uint8_t *buf, size_t bufsz, size_t *pos);

// A byte array is different because val->length *must* be set before parsing
int64_t dssh_parse_bytearray(const uint8_t *buf, size_t bufsz, dssh_bytearray val);
size_t dssh_serialized_bytearray_length(dssh_bytearray val);
int dssh_serialize_bytearray(dssh_bytearray val, uint8_t *buf, size_t bufsz, size_t *pos);
int64_t dssh_parse_boolean(const uint8_t *buf, size_t bufsz, dssh_boolean *val);
size_t dssh_serialized_boolean_length(dssh_boolean val);
int dssh_serialize_boolean(dssh_boolean val, uint8_t *buf, size_t bufsz, size_t *pos);
int64_t dssh_parse_uint32(const uint8_t *buf, size_t bufsz, dssh_uint32_t *val);
size_t dssh_serialized_uint32_length(dssh_uint32_t val);
int dssh_serialize_uint32(dssh_uint32_t val, uint8_t *buf, size_t bufsz, size_t *pos);
int64_t dssh_parse_uint64(const uint8_t *buf, size_t bufsz, dssh_uint64_t *val);
size_t dssh_serialized_uint64_length(dssh_uint64_t val);
int dssh_serialize_uint64(dssh_uint64_t val, uint8_t *buf, size_t bufsz, size_t *pos);
int64_t dssh_parse_string(const uint8_t *buf, size_t bufsz, dssh_string val);
size_t dssh_serialized_string_length(dssh_string val);
int dssh_serialize_string(dssh_string val, uint8_t *buf, size_t bufsz, size_t *pos);
int64_t dssh_parse_mpint(const uint8_t *buf, size_t bufsz, dssh_mpint val);
size_t dssh_serialized_mpint_length(dssh_mpint val);
int dssh_serialize_mpint(dssh_mpint val, uint8_t *buf, size_t bufsz, size_t *pos);
int64_t dssh_parse_namelist(const uint8_t *buf, size_t bufsz, dssh_namelist val);
size_t dssh_serialized_namelist_length(dssh_namelist val);
int dssh_serialize_namelist(dssh_namelist val, uint8_t *buf, size_t bufsz, size_t *pos);
int64_t dssh_parse_namelist_next(dssh_string val, dssh_namelist nl);

#endif // ifndef DSSH_ARCH_H
