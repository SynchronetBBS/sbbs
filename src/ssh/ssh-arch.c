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
	if (buf[0] != 0 && buf[0] != 1)
		return DEUCE_SSH_ERROR_INVALID;
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
deuce_ssh_parse_string(uint8_t * buf, size_t bufsz, deuce_ssh_string_t *val)
{
	ssize_t ret;
	uint32_t len;
	size_t sz;
	size_t pos = 0;

	assert(bufsz >= 4);
	assert(*val == NULL);
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
	*val = malloc(sz);
	if (*val == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	(*val)->length = len;
	deuce_ssh_serialize_uint32(len, (uint8_t *)*val, sz, &pos);
	assert(pos == 4);
	memcpy(&(*val)->value, &buf[pos], len);
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
deuce_ssh_parse_mpint(uint8_t * buf, size_t bufsz, deuce_ssh_mpint_t *val)
{
	size_t ret;
	uint32_t len;
	size_t sz;
	size_t pos = 0;

	assert(bufsz >= 4);
	assert(*val == NULL);
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
	*val = BN_mpi2bn(buf, sz, NULL);
	return sz;
}

size_t
deuce_ssh_serialized_mpint_length(deuce_ssh_mpint_t val)
{
	int ret = BN_bn2mpi(val, NULL);
	assert(ret >= 4);
	return ret;
}

void
deuce_ssh_serialize_mpint(deuce_ssh_mpint_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos)
{
	assert(*pos + 4 <= bufsz);
	*pos += BN_bn2mpi(val, &buf[*pos]);
}

ssize_t
deuce_ssh_parse_namelist(uint8_t * buf, size_t bufsz, deuce_ssh_namelist_t *val)
{
	deuce_ssh_string_t str = NULL;
	size_t names = 0;

	assert(*val == NULL);
	ssize_t ret = deuce_ssh_parse_string(buf, bufsz, &str);
	if (ret < 4)
		return ret;
	if (str->length > 0) {
		names++;
		for (size_t i = 0; i < str->length; i++) {
			if (str->value[i] == ',') {
				if (i == 0 || str->value[i - 1] == ',') {
					free(str);
					return DEUCE_SSH_ERROR_PARSE;
				}
				names++;
			}
			if (str->value[i] < ' ') {
				free(str);
				return DEUCE_SSH_ERROR_PARSE;
			}
			if (str->value[i] > '~') {
				free(str);
				return DEUCE_SSH_ERROR_PARSE;
			}
		}
	}
	size_t lsz = sizeof(uint8_t *) * names;
	*val = malloc(offsetof(struct deuce_ssh_namelist, name) + lsz + str->length + (names == 0 ? 0 : 1));
	if (*val == NULL) {
		free(str);
		return DEUCE_SSH_ERROR_ALLOC;
	}
	(*val)->names = names;
	if (names > 0) {
		uint8_t *data = (uint8_t *)&(*val)->name[names];
		memcpy(data, str->value, str->length);
		data[str->length] = ',';
		uint8_t *end = data;
		size_t remain = str->length + 1;
		for (size_t i = 0; i < names; i++) {
			(*val)->name[i] = end;
			end = memchr(end, ',', remain);
			assert(end);
			assert(*end == ',');
			*end = 0;
			end++;
			remain = (str->length + 1) - (end - data);
		}
	}
	return ret;
}

size_t
deuce_ssh_serialized_namelist_length(deuce_ssh_namelist_t val)
{
	size_t ret = 4;
	for (size_t i = 0; i < val->names; i++) {
		ret += strlen((char *)val->name[i]);
		if (i < (val->names - 1))
			ret += 1;
	}
	return ret;
}

void
deuce_ssh_serialize_namelist(deuce_ssh_namelist_t val, uint8_t *buf, MAYBE_UNUSED size_t bufsz, size_t *pos)
{
	assert(bufsz >= 4);

	uint8_t *lbuf = buf;
	size_t lpos = *pos;
	*pos += 4;
	uint8_t *data = (uint8_t *)&val->name[0];
	size_t names = 0;
	for (size_t i = 0;; i++) {
		if (data[i] == 0) {
			names++;
			if (names == val->names)
				break;
		}
		buf[(*pos)++] = data[i];
	}
	deuce_ssh_serialize_uint32(*pos - lpos - 4, lbuf, 4, &lpos);
}
