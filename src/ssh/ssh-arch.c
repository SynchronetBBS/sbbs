// RFC-4251

#include <string.h>

#include "deucessh.h"

/*
 * Required by parse functions
 */

int64_t
dssh_parse_byte(const uint8_t *buf, size_t bufsz, dssh_byte *val)
{
	if (bufsz < 1)
		return DSSH_ERROR_PARSE;
	*val = buf[0];
	return 1;
}

size_t
dssh_serialized_byte_length(dssh_byte val)
{
	return 1;
}

int
dssh_serialize_byte(dssh_byte val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos + 1 > bufsz)
		return DSSH_ERROR_TOOLONG;
	buf[*pos] = val;
	*pos += 1;
	return 0;
}

int64_t
dssh_parse_bytearray(const uint8_t *buf, size_t bufsz, dssh_bytearray val)
{
	if (val->length < 2)
		return DSSH_ERROR_PARSE;
	if (bufsz < val->length)
		return DSSH_ERROR_PARSE;
	val->value = buf;
	return val->length;
}

size_t
dssh_serialized_bytearray_length(dssh_bytearray val)
{
	return val->length + 4;
}

int
dssh_serialize_bytearray(dssh_bytearray val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos + val->length > bufsz)
		return DSSH_ERROR_TOOLONG;
	memcpy(&buf[*pos], val->value, val->length);
	*pos += val->length;
	return 0;
}

int64_t
dssh_parse_boolean(const uint8_t *buf, size_t bufsz, dssh_boolean *val)
{
	if (bufsz < 1)
		return DSSH_ERROR_PARSE;
	*val = buf[0];
	return 1;
}

size_t
dssh_serialized_boolean_length(dssh_boolean val)
{
	return 1;
}

int
dssh_serialize_boolean(dssh_boolean val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos + 1 > bufsz)
		return DSSH_ERROR_TOOLONG;
	buf[*pos] = val;
	*pos += 1;
	return 0;
}

int64_t
dssh_parse_uint32(const uint8_t *buf, size_t bufsz, dssh_uint32_t *val)
{
	if (bufsz < 4)
		return DSSH_ERROR_PARSE;
	*val = (((uint32_t)buf[0]) << 24) | (((uint32_t)buf[1]) << 16) | (((uint32_t)buf[2]) << 8) | buf[3];
	return 4;
}

size_t
dssh_serialized_uint32_length(dssh_uint32_t val)
{
	return 4;
}

int
dssh_serialize_uint32(dssh_uint32_t val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos + 4 > bufsz)
		return DSSH_ERROR_TOOLONG;
	buf[(*pos)++] = (val >> 24) & 0xff;
	buf[(*pos)++] = (val >> 16) & 0xff;
	buf[(*pos)++] = (val >> 8) & 0xff;
	buf[(*pos)++] = (val) & 0xff;
	return 0;
}

int64_t
dssh_parse_uint64(const uint8_t *buf, size_t bufsz, dssh_uint64_t *val)
{
	if (bufsz < 8)
		return DSSH_ERROR_PARSE;
	*val = (((uint64_t)buf[0]) << 56) | (((uint64_t)buf[1]) << 48) | (((uint64_t)buf[2]) << 40) |
	    (((uint64_t)buf[3]) << 32) | (((uint64_t)buf[4]) << 24) | (((uint64_t)buf[5]) << 16) |
	    (((uint64_t)buf[6]) << 8) | buf[7];
	return 8;
}

size_t
dssh_serialized_uint64_length(dssh_uint64_t val)
{
	return 8;
}

int
dssh_serialize_uint64(dssh_uint64_t val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos + 8 > bufsz)
		return DSSH_ERROR_TOOLONG;
	buf[(*pos)++] = (val >> 56) & 0xff;
	buf[(*pos)++] = (val >> 48) & 0xff;
	buf[(*pos)++] = (val >> 40) & 0xff;
	buf[(*pos)++] = (val >> 32) & 0xff;
	buf[(*pos)++] = (val >> 24) & 0xff;
	buf[(*pos)++] = (val >> 16) & 0xff;
	buf[(*pos)++] = (val >> 8) & 0xff;
	buf[(*pos)++] = (val) & 0xff;
	return 0;
}

int64_t
dssh_parse_string(const uint8_t *buf, size_t bufsz, dssh_string val)
{
	if (bufsz < 4)
		return DSSH_ERROR_PARSE;
	uint32_t len;
	int64_t ret = dssh_parse_uint32(buf, bufsz, &len);
	if (ret < 4)
		return ret;
	size_t sz = ret + len;
	if (bufsz < sz)
		return DSSH_ERROR_PARSE;
	val->length = len;
	val->value = &buf[ret];
	return sz;
}

size_t
dssh_serialized_string_length(dssh_string val)
{
	return val->length + 4;
}

int
dssh_serialize_string(dssh_string val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos + 4 + val->length > bufsz)
		return DSSH_ERROR_TOOLONG;
	dssh_serialize_uint32(val->length, buf, bufsz, pos);
	memcpy(&buf[*pos], val->value, val->length);
	*pos += val->length;
	return 0;
}

int64_t
dssh_parse_mpint(const uint8_t *buf, size_t bufsz, dssh_mpint val)
{
	int64_t ret = dssh_parse_string(buf, bufsz, val);
	if (ret < 4)
		return ret;
	if (val->length >= 2) {
		if ((val->value[0] == 0) && ((val->value[1] & 0x80) == 0))
			return DSSH_ERROR_INVALID;
		if ((val->value[0] == 0xff) && (val->value[1] & 0x80))
			return DSSH_ERROR_INVALID;
	}
	return ret;
}

size_t
dssh_serialized_mpint_length(dssh_mpint val)
{
	return val->length + 4;
}

int
dssh_serialize_mpint(dssh_mpint val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	return dssh_serialize_string(val, buf, bufsz, pos);
}

int64_t
dssh_parse_namelist(const uint8_t *buf, size_t bufsz, dssh_namelist val)
{
	struct dssh_string_s str;

	int64_t ret = dssh_parse_string(buf, bufsz, &str);
	if (ret < 4)
		return ret;
	if (str.length > 0) {
		for (uint32_t i = 0; i < str.length; i++) {
			if (str.value[i] == ',') {
				if (i == 0 || str.value[i - 1] == ',')
					return DSSH_ERROR_PARSE;
			}
			if (str.value[i] <= ' ' || str.value[i] >= 127)
				return DSSH_ERROR_PARSE;
		}
		/* Trailing comma would produce a zero-length final name */
		if (str.value[str.length - 1] == ',')
			return DSSH_ERROR_PARSE;
	}
	val->value = str.value;
	val->length = str.length;
	val->next = 0;
	return ret;
}

size_t
dssh_serialized_namelist_length(dssh_namelist val)
{
	return val->length + 4;
}

int
dssh_serialize_namelist(dssh_namelist val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	struct dssh_string_s str = {
		.value = val->value,
		.length = val->length,
	};
	return dssh_serialize_string(&str, buf, bufsz, pos);
}

int64_t
dssh_parse_namelist_next(dssh_string val, dssh_namelist nl)
{
	if (nl->next >= nl->length)
		return DSSH_ERROR_NOMORE;
	val->value = &(nl->value[nl->next]);
	for (val->length = 0; nl->next < nl->length; nl->next++, val->length++) {
		if (nl->value[nl->next] == ',') {
			nl->next++;
			break;
		}
	}
	/* RFC 4251 s6: names MUST NOT be longer than 64 characters */
	if (val->length > 64)
		return DSSH_ERROR_PARSE;
	return val->length + 4;
}
