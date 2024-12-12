// RFC-4251

#include <assert.h>
#include <string.h>

#include "deucessh.h"

/*
 * Required by parse functions
 */
_Static_assert(sizeof(ssize_t) > sizeof(uint32_t), "ssize_t must be larger than uint32_t");

ssize_t
deuce_ssh_parse_byte(uint8_t * buf, size_t bufsz, deuce_ssh_byte_t *val)
{
	assert(bufsz >= 1);
	if (bufsz < 1)
		return DEUCE_SSH_ERROR_PARSE;
	*val = buf[0];
	return 1;
}

size_t
deuce_ssh_serialized_byte_length(deuce_ssh_byte_t val)
{
	return 1;
}

void
deuce_ssh_serialize_byte(deuce_ssh_byte_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos)
{
	assert(*pos + 1 <= bufsz);
	buf[*pos] = val;
	*pos += 1;
}

ssize_t
deuce_ssh_parse_boolean(uint8_t * buf, size_t bufsz, deuce_ssh_boolean_t *val)
{
	assert(bufsz >= 1);
	if (bufsz < 1)
		return DEUCE_SSH_ERROR_PARSE;
#ifdef PARANOID_WRONG
	if (buf[0] != 0 && buf[0] != 1)
		return DEUCE_SSH_ERROR_INVALID;
#endif
	*val = buf[0];
	return 1;
}

size_t
deuce_ssh_serialized_boolean_length(deuce_ssh_boolean_t val)
{
	return 1;
}

void
deuce_ssh_serialize_boolean(deuce_ssh_boolean_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos)
{
	assert(*pos + 1 <= bufsz);
	buf[*pos] = val;
	*pos += 1;
}

ssize_t
deuce_ssh_parse_uint32(uint8_t * buf, size_t bufsz, deuce_ssh_uint32_t *val)
{
	assert(bufsz >= 4);
	if (bufsz < 4)
		return DEUCE_SSH_ERROR_PARSE;
	*val = (((uint32_t)buf[0]) << 24) | (((uint32_t)buf[1]) << 16) | (((uint32_t)buf[2]) << 8) | buf[3];
	return 4;
}

size_t
deuce_ssh_serialized_uint32_length(deuce_ssh_uint32_t val)
{
	return 4;
}

void
deuce_ssh_serialize_uint32(deuce_ssh_uint32_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos)
{
	assert(*pos + 4 <= bufsz);
	buf[(*pos)++] = (val >> 24) & 0xff;
	buf[(*pos)++] = (val >> 16) & 0xff;
	buf[(*pos)++] = (val >> 8) & 0xff;
	buf[(*pos)++] = (val) & 0xff;
}

ssize_t
deuce_ssh_parse_uint64(uint8_t * buf, size_t bufsz, deuce_ssh_uint64_t *val)
{
	assert(bufsz >= 8);
	if (bufsz < 8)
		return DEUCE_SSH_ERROR_PARSE;
	*val = (((uint64_t)buf[0]) << 56) | (((uint64_t)buf[1]) << 48) | (((uint64_t)buf[2]) << 40) |
	    (((uint64_t)buf[3]) << 32) | (((uint64_t)buf[4]) << 24) | (((uint64_t)buf[5]) << 16) |
	    (((uint64_t)buf[6]) << 8) | buf[7];
	return 8;
}

size_t
deuce_ssh_serialized_uint64_length(deuce_ssh_uint64_t val)
{
	return 8;
}

void
deuce_ssh_serialize_uint64(deuce_ssh_uint64_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos)
{
	assert(*pos + 8 <= bufsz);
	buf[(*pos)++] = (val >> 56) & 0xff;
	buf[(*pos)++] = (val >> 48) & 0xff;
	buf[(*pos)++] = (val >> 40) & 0xff;
	buf[(*pos)++] = (val >> 32) & 0xff;
	buf[(*pos)++] = (val >> 24) & 0xff;
	buf[(*pos)++] = (val >> 16) & 0xff;
	buf[(*pos)++] = (val >> 8) & 0xff;
	buf[(*pos)++] = (val) & 0xff;
}

ssize_t
deuce_ssh_parse_string(uint8_t * buf, size_t bufsz, deuce_ssh_string_t val)
{
	ssize_t ret;
	uint32_t len;
	size_t sz;
	size_t pos = 0;

	assert(bufsz >= 4);
	if (bufsz < 4)
		return DEUCE_SSH_ERROR_PARSE;
	ret = deuce_ssh_parse_uint32(buf, bufsz, &len);
	if (ret < 4)
		return ret;
	assert(ret == 4);
	sz = ret + len;
	assert(bufsz >= sz);
	if (bufsz < sz)
		return DEUCE_SSH_ERROR_PARSE;
	val->length = len;
	memcpy(&val->value, &buf[ret], len);
	return sz;
}

size_t
deuce_ssh_serialized_string_length(deuce_ssh_string_t val)
{
	return val->length + 4;
}

void
deuce_ssh_serialize_string(deuce_ssh_string_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos)
{
	assert(*pos + 4 + val->length <= bufsz);
	deuce_ssh_serialize_uint32(val->length, buf, bufsz, pos);
	memcpy(&buf[4], val->value, val->length);
	*pos += val->length;
}

ssize_t
deuce_ssh_parse_mpint(uint8_t * buf, size_t bufsz, deuce_ssh_mpint_t val)
{
	ssize_t ret = deuce_ssh_parse_string(buf, bufsz, val);
	if (val->value[0] == val->value[1])
		return DEUCE_SSH_ERROR_INVALID;
	if ((val->value[0] == 0) && ((val->value[0] & 0x80) == 0))
		return DEUCE_SSH_ERROR_INVALID;
	if ((val->value[0] == 255) && (val->value[0] & 0x80))
		return DEUCE_SSH_ERROR_INVALID;
	return ret;
}

size_t
deuce_ssh_serialized_mpint_length(deuce_ssh_mpint_t val)
{
	return val->length + 4;
}

void
deuce_ssh_serialize_mpint(deuce_ssh_mpint_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos)
{
	deuce_ssh_serialize_string(val, buf, bufsz, pos);
}

ssize_t
deuce_ssh_parse_namelist(uint8_t * buf, size_t bufsz, deuce_ssh_namelist_t val)
{
	struct deuce_ssh_string str;

	ssize_t ret = deuce_ssh_parse_string(buf, bufsz, &str);
	if (ret < 4)
		return ret;
	if (str.length > 0) {
		for (uint32_t i = 0; i < str.length; i++) {
			if (str.value[i] == ',') {
				if (i == 0 || str.value[i - 1] == ',')
					return DEUCE_SSH_ERROR_PARSE;
			}
			if (str.value[i] < ' ')
				return DEUCE_SSH_ERROR_PARSE;
			if (str.value[i] > '~')
				return DEUCE_SSH_ERROR_PARSE;
		}
	}
	val->value = str.value;
	val->length = str.length;
	val->next = 0;
	return ret;
}

size_t
deuce_ssh_serialized_namelist_length(deuce_ssh_namelist_t val)
{
	return val->length + 4;
}

void
deuce_ssh_serialize_namelist(deuce_ssh_namelist_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos)
{
	struct deuce_ssh_string str = {
		.value = val->value,
		.length = val->length,
	};
	deuce_ssh_serialize_string(&str, buf, bufsz, pos);
}

ssize_t
deuce_ssh_parse_namelist_next(deuce_ssh_string_t val, deuce_ssh_namelist_t nl)
{
	if (nl->next >= nl->length)
		return DEUCE_SSH_ERROR_NOMORE;
	val->value = &(nl->value[nl->next]);
	for (val->length = 0; nl->next < nl->length; nl->next++, val->length++) {
		if (nl->value[nl->next] == ',') {
			nl->next++;
			break;
		}
	}
	return val->length + 4;
}
