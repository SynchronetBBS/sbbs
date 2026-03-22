// RFC-4251

#ifndef DEUCE_SSH_ARCH_H
#define DEUCE_SSH_ARCH_H

#ifndef DEUCE_SSH_H
#error Only include deucessh.h, do not directly include this file.
#endif

#include <inttypes.h>
#include <stdbool.h>

#include <openssl/bn.h>

#include "portable.h"

// SSH Data Types (internal representations)
typedef uint8_t deuce_ssh_byte;
typedef bool deuce_ssh_boolean;
typedef uint32_t deuce_ssh_uint32_t;
typedef uint64_t deuce_ssh_uint64_t;
typedef struct deuce_ssh_string_s {
	const deuce_ssh_byte *value;
	deuce_ssh_uint32_t length;
} *deuce_ssh_string;
typedef struct deuce_ssh_string_s *deuce_ssh_bytearray;
typedef struct deuce_ssh_string_s *deuce_ssh_mpint;
typedef struct deuce_ssh_namelist_s {
	const deuce_ssh_byte *value;
	deuce_ssh_uint32_t length;
	deuce_ssh_uint32_t next;
} *deuce_ssh_namelist;

#define deuce_ssh_parse(buf, bufsz, val) _Generic(val,                \
	deuce_ssh_byte : deuce_ssh_parse_byte(buf, bufsz, val),      \
	deuce_ssh_bytearray : deuce_ssh_parse_bytearray(buf, bufsz, val),      \
	deuce_ssh_boolean : deuce_ssh_parse_boolean(buf, bufsz, val), \
	deuce_ssh_uint32_t : deuce_ssh_parse_uint32(buf, bufsz, val),    \
	deuce_ssh_uint64_t : deuce_ssh_parse_uint64(buf, bufsz, val),     \
	deuce_ssh_string : deuce_ssh_parse_string(buf, bufsz, val),      \
	deuce_ssh_mpint : deuce_ssh_parse_mpint(buf, bufsz, val),         \
	deuce_ssh_namelist : deuce_ssh_parse_namelist(buf, bufsz, val))

#define deuce_ssh_serialized_length(val) _Generic(val,                \
	deuce_ssh_byte : deuce_ssh_serialized_byte_length(val),      \
	deuce_ssh_bytearray : deuce_ssh_serialized_byte_length(val),      \
	deuce_ssh_boolean : deuce_ssh_serialized_boolean_length(val), \
	deuce_ssh_uint32_t : deuce_ssh_serialized_uint32_length(val),    \
	deuce_ssh_uint64_t : deuce_ssh_serialized_uint64_length(val),     \
	deuce_ssh_string : deuce_ssh_serialized_string_length(val),      \
	deuce_ssh_mpint : deuce_ssh_serialized_mpint_length(val),         \
	deuce_ssh_namelist : deuce_ssh_serialized_namelist_length(val))

#define deuce_ssh_serialize(val, buf, bufsz, pos) _Generic(val,                \
	deuce_ssh_byte : deuce_ssh_serialize_byte(val, buf, bufsz, pos),      \
	deuce_ssh_bytearray : deuce_ssh_serialize_byte(val, buf, bufsz, pos),      \
	deuce_ssh_boolean : deuce_ssh_serialize_boolean(val, buf, bufsz, pos), \
	deuce_ssh_uint32_t : deuce_ssh_serialize_uint32(val, buf, bufsz, pos),    \
	deuce_ssh_uint64_t : deuce_ssh_serialize_uint64(val, buf, bufsz, pos),     \
	deuce_ssh_string : deuce_ssh_serialize_string(val, buf, bufsz, pos),      \
	deuce_ssh_mpint : deuce_ssh_serialize_mpint(val, buf, bufsz, pos),         \
	deuce_ssh_namelist : deuce_ssh_serialize_namelist(val, buf, bufsz, pos))

int64_t deuce_ssh_parse_byte(const uint8_t *buf, size_t bufsz, deuce_ssh_byte *val);
size_t deuce_ssh_serialized_byte_length(deuce_ssh_byte val);
int deuce_ssh_serialize_byte(deuce_ssh_byte val, uint8_t *buf, size_t bufsz, size_t *pos);

// A byte array is different because val->length *must* be set before parsing
int64_t deuce_ssh_parse_bytearray(const uint8_t *buf, size_t bufsz, deuce_ssh_bytearray val);
size_t deuce_ssh_serialized_bytearray_length(deuce_ssh_bytearray val);
int deuce_ssh_serialize_bytearray(deuce_ssh_bytearray val, uint8_t *buf, size_t bufsz, size_t *pos);

int64_t deuce_ssh_parse_boolean(const uint8_t *buf, size_t bufsz, deuce_ssh_boolean *val);
size_t deuce_ssh_serialized_boolean_length(deuce_ssh_boolean val);
int deuce_ssh_serialize_boolean(deuce_ssh_boolean val, uint8_t *buf, size_t bufsz, size_t *pos);

int64_t deuce_ssh_parse_uint32(const uint8_t *buf, size_t bufsz, deuce_ssh_uint32_t *val);
size_t deuce_ssh_serialized_uint32_length(deuce_ssh_uint32_t val);
int deuce_ssh_serialize_uint32(deuce_ssh_uint32_t val, uint8_t *buf, size_t bufsz, size_t *pos);

int64_t deuce_ssh_parse_uint64(const uint8_t *buf, size_t bufsz, deuce_ssh_uint64_t *val);
size_t deuce_ssh_serialized_uint64_length(deuce_ssh_uint64_t val);
int deuce_ssh_serialize_uint64(deuce_ssh_uint64_t val, uint8_t *buf, size_t bufsz, size_t *pos);

int64_t deuce_ssh_parse_string(const uint8_t *buf, size_t bufsz, deuce_ssh_string val);
size_t deuce_ssh_serialized_string_length(deuce_ssh_string val);
int deuce_ssh_serialize_string(deuce_ssh_string val, uint8_t *buf, size_t bufsz, size_t *pos);

int64_t deuce_ssh_parse_mpint(const uint8_t *buf, size_t bufsz, deuce_ssh_mpint val);
size_t deuce_ssh_serialized_mpint_length(deuce_ssh_mpint val);
int deuce_ssh_serialize_mpint(deuce_ssh_mpint val, uint8_t *buf, size_t bufsz, size_t *pos);

int64_t deuce_ssh_parse_namelist(const uint8_t *buf, size_t bufsz, deuce_ssh_namelist val);
size_t deuce_ssh_serialized_namelist_length(deuce_ssh_namelist val);
int deuce_ssh_serialize_namelist(deuce_ssh_namelist val, uint8_t *buf, size_t bufsz, size_t *pos);
int64_t deuce_ssh_parse_namelist_next(deuce_ssh_string val, deuce_ssh_namelist nl);

#endif
