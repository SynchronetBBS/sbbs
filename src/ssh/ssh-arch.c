// RFC-4251

#include <string.h>

#include "deucessh.h"
#include "ssh-internal.h"

/*
 * Required by parse functions
 */
DSSH_PRIVATE int64_t
dssh_parse_byte(const uint8_t *buf, size_t bufsz, dssh_byte *val)
{
	if (bufsz < 1)
		return DSSH_ERROR_PARSE;
	*val = buf[0];
	return 1;
}

DSSH_PRIVATE int
dssh_serialize_byte(dssh_byte val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos >= bufsz)
		return DSSH_ERROR_TOOLONG;
	buf[*pos] = val;
	*pos += 1;
	return 0;
}

DSSH_PRIVATE int64_t
dssh_parse_boolean(const uint8_t *buf, size_t bufsz, dssh_boolean *val)
{
	if (bufsz < 1)
		return DSSH_ERROR_PARSE;
	*val = buf[0];
	return 1;
}

DSSH_PRIVATE int
dssh_serialize_boolean(dssh_boolean val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos >= bufsz)
		return DSSH_ERROR_TOOLONG;
	buf[*pos] = val;
	*pos += 1;
	return 0;
}

DSSH_PUBLIC int64_t
dssh_parse_uint32(const uint8_t *buf, size_t bufsz, dssh_uint32_t *val)
{
	if (buf == NULL || val == NULL)
		return DSSH_ERROR_INVALID;
	if (bufsz < 4)
		return DSSH_ERROR_PARSE;
	*val = (((uint32_t)buf[0]) << 24) | (((uint32_t)buf[1]) << 16) | (((uint32_t)buf[2]) << 8) | buf[3];
	return 4;
}

DSSH_PUBLIC int
dssh_serialize_uint32(dssh_uint32_t val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (buf == NULL || pos == NULL)
		return DSSH_ERROR_INVALID;
	if (*pos > bufsz || bufsz - *pos < 4)
		return DSSH_ERROR_TOOLONG;
	buf[(*pos)++] = (val >> 24) & 0xff;
	buf[(*pos)++] = (val >> 16) & 0xff;
	buf[(*pos)++] = (val >> 8) & 0xff;
	buf[(*pos)++] = (val) & 0xff;
	return 0;
}

DSSH_PRIVATE int64_t
dssh_parse_uint64(const uint8_t *buf, size_t bufsz, dssh_uint64_t *val)
{
	if (bufsz < 8)
		return DSSH_ERROR_PARSE;
	*val = (((uint64_t)buf[0]) << 56) | (((uint64_t)buf[1]) << 48) | (((uint64_t)buf[2]) << 40)
	    | (((uint64_t)buf[3]) << 32) | (((uint64_t)buf[4]) << 24) | (((uint64_t)buf[5]) << 16)
	    | (((uint64_t)buf[6]) << 8) | buf[7];
	return 8;
}

DSSH_PRIVATE int
dssh_serialize_uint64(dssh_uint64_t val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos > bufsz || bufsz - *pos < 8)
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

DSSH_PRIVATE int64_t
dssh_parse_string(const uint8_t *buf, size_t bufsz, dssh_string val)
{
	if (bufsz < 4)
		return DSSH_ERROR_PARSE;

	uint32_t len;

        /* dssh_parse_uint32 cannot fail here: it only fails when
         * bufsz < 4, which is already ruled out above. */
	int64_t ret = dssh_parse_uint32(buf, bufsz, &len);

	if (ret < 4)
		return ret;
#if SIZE_MAX < INT64_MAX
	if (ret > SIZE_MAX)
		return DSSH_ERROR_INVALID;
#endif
	size_t hdr = (size_t)ret;

	if (len > SIZE_MAX - hdr)
		return DSSH_ERROR_INVALID;
	if (hdr + len > bufsz)
		return DSSH_ERROR_PARSE;
	size_t sz = hdr + len;
	val->length = len;
	val->value = &buf[hdr];
#if SIZE_MAX > INT64_MAX
	if (sz > INT64_MAX)
		return DSSH_ERROR_INVALID;
#endif
	return (int64_t)sz;
}

DSSH_PRIVATE int
dssh_serialize_string(dssh_string val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (*pos > bufsz || bufsz - *pos < 4 || val->length > bufsz - *pos - 4)
		return DSSH_ERROR_TOOLONG;
	int ret = dssh_serialize_uint32(val->length, buf, bufsz, pos);
	if (ret < 0)
		return ret;
	memcpy(&buf[*pos], val->value, val->length);
	*pos += val->length;
	return 0;
}

DSSH_PRIVATE int64_t
dssh_parse_mpint(const uint8_t *buf, size_t bufsz, dssh_mpint val)
{
	int64_t ret = dssh_parse_string(buf, bufsz, val);

	if (ret < 4)
		return ret;
	if (val->length >= 2) {
		if ((val->value[0] == 0) && ((val->value[1] & DSSH_MPINT_SIGN_BIT) == 0))
			return DSSH_ERROR_INVALID;
		if ((val->value[0] == 0xff) && (val->value[1] & DSSH_MPINT_SIGN_BIT))
			return DSSH_ERROR_INVALID;
	}
	return ret;
}

DSSH_PRIVATE int
dssh_serialize_mpint(dssh_mpint val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	return dssh_serialize_string(val, buf, bufsz, pos);
}

DSSH_PRIVATE int64_t
dssh_parse_namelist(const uint8_t *buf, size_t bufsz, dssh_namelist val)
{
	struct dssh_string_s str;
	int64_t              ret = dssh_parse_string(buf, bufsz, &str);

	if (ret < 4)
		return ret;
	if (str.length > 0) {
		for (uint32_t i = 0; i < str.length; i++) {
			if (str.value[i] == ',') {
				if ((i == 0) || (str.value[i - 1] == ','))
					return DSSH_ERROR_PARSE;
			}
			if ((str.value[i] <= ' ') || (str.value[i] >= DSSH_ASCII_DEL))
				return DSSH_ERROR_PARSE;
		}

                /* Trailing comma would produce a zero-length final name */
		if (str.value[str.length - 1] == ',')
			return DSSH_ERROR_PARSE;
	}
	val->value = str.value;
	val->length = str.length;
	return ret;
}

DSSH_PRIVATE int
dssh_serialize_namelist(dssh_namelist val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	struct dssh_string_s str = {
		.value = val->value,
		.length = val->length,
	};

	return dssh_serialize_string(&str, buf, bufsz, pos);
}
