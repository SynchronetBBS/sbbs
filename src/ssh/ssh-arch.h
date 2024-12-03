// RFC-4251

#ifndef DEUCE_SSH_ARCH_H
#define DEUCE_SSH_ARCH_H

#include <inttypes.h>
#include <stdbool.h>

#include <openssl/bn.h>

#include "portable.h"

// SSH Data Types (internal representations)
typedef uint8_t deuce_ssh_byte_t;
typedef bool deuce_ssh_boolean_t;
typedef uint32_t deuce_ssh_uint32_t;
typedef uint64_t deuce_ssh_uint64_t;
typedef struct deuce_ssh_string {
	deuce_ssh_uint32_t length;
	deuce_ssh_byte_t value[];
} *deuce_ssh_string_t;
typedef BIGNUM *deuce_ssh_mpint_t;
typedef struct deuce_ssh_namelist {
	size_t   names;
	uint8_t *name[];
} *deuce_ssh_namelist_t;

#define deuce_ssh_parse(buf, bufsz, val) _Generic(val,                \
	deuce_ssh_byte_t : deuce_ssh_parse_byte(buf, bufsz, val),      \
	deuce_ssh_boolean_t : deuce_ssh_parse_boolean(buf, bufsz, val), \
	deuce_ssh_uint32_t : deuce_ssh_parse_uint32(buf, bufsz, val),    \
	deuce_ssh_uint64_t : deuce_ssh_parse_uint64(buf, bufsz, val),     \
	deuce_ssh_string_t : deuce_ssh_parse_string(buf, bufsz, val),      \
	deuce_ssh_mpint_t : deuce_ssh_parse_mpint(buf, bufsz, val),         \
	deuce_ssh_namelist_t : deuce_ssh_parse_namelist(buf, bufsz, val))

#define deuce_ssh_serialized_length(val) _Generic(val,                \
	deuce_ssh_byte_t : deuce_ssh_serialized_byte_length(val),      \
	deuce_ssh_boolean_t : deuce_ssh_serialized_boolean_length(val), \
	deuce_ssh_uint32_t : deuce_ssh_serialized_uint32_length(val),    \
	deuce_ssh_uint64_t : deuce_ssh_serialized_uint64_length(val),     \
	deuce_ssh_string_t : deuce_ssh_serialized_string_length(val),      \
	deuce_ssh_mpint_t : deuce_ssh_serialized_mpint_length(val),         \
	deuce_ssh_namelist_t : deuce_ssh_serialized_namelist_length(val))

#define deuce_ssh_serialize(val, buf, bufsz, pos) _Generic(val,                \
	deuce_ssh_byte_t : deuce_ssh_serialize_byte(val, buf, bufsz, pos),      \
	deuce_ssh_boolean_t : deuce_ssh_serialize_boolean(val, buf, bufsz, pos), \
	deuce_ssh_uint32_t : deuce_ssh_serialize_uint32(val, buf, bufsz, pos),    \
	deuce_ssh_uint64_t : deuce_ssh_serialize_uint64(val, buf, bufsz, pos),     \
	deuce_ssh_string_t : deuce_ssh_serialize_string(val, buf, bufsz, pos),      \
	deuce_ssh_mpint_t : deuce_ssh_serialize_mpint(val, buf, bufsz, pos),         \
	deuce_ssh_namelist_t : deuce_ssh_serialize_namelist(val, buf, bufsz, pos))

ssize_t deuce_ssh_parse_byte(uint8_t * buf, size_t bufsz, deuce_ssh_byte_t *val);
size_t deuce_ssh_serialized_byte_length(deuce_ssh_byte_t val);
void deuce_ssh_serialize_byte(deuce_ssh_byte_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos);

ssize_t deuce_ssh_parse_boolean(uint8_t * buf, size_t bufsz, deuce_ssh_boolean_t *val);
size_t deuce_ssh_serialized_boolean_length(deuce_ssh_boolean_t val);
void deuce_ssh_serialize_boolean(deuce_ssh_boolean_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos);

ssize_t deuce_ssh_parse_uint32(uint8_t * buf, size_t bufsz, deuce_ssh_uint32_t *val);
size_t deuce_ssh_serialized_uint32_length(deuce_ssh_uint32_t val);
void deuce_ssh_serialize_uint32(deuce_ssh_uint32_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos);

ssize_t deuce_ssh_parse_uint64(uint8_t * buf, size_t bufsz, deuce_ssh_uint64_t *val);
size_t deuce_ssh_serialized_uint64_length(deuce_ssh_uint64_t val);
void deuce_ssh_serialize_uint64(deuce_ssh_uint64_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos);

ssize_t deuce_ssh_parse_string(uint8_t * buf, size_t bufsz, deuce_ssh_string_t *val);
size_t deuce_ssh_serialized_string_length(deuce_ssh_string_t val);
void deuce_ssh_serialize_string(deuce_ssh_string_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos);

ssize_t deuce_ssh_parse_mpint(uint8_t * buf, size_t bufsz, deuce_ssh_mpint_t *val);
size_t deuce_ssh_serialized_mpint_length(deuce_ssh_mpint_t val);
void deuce_ssh_serialize_mpint(deuce_ssh_mpint_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos);

ssize_t deuce_ssh_parse_namelist(uint8_t * buf, size_t bufsz, deuce_ssh_namelist_t *val);
size_t deuce_ssh_serialized_namelist_length(deuce_ssh_namelist_t val);
void deuce_ssh_serialize_namelist(deuce_ssh_namelist_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos);

#endif
