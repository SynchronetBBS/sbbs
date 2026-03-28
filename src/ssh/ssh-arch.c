// RFC-4251

#include "deucessh.h"
#include "ssh-internal.h"

DSSH_PUBLIC int64_t
dssh_parse_uint32(const uint8_t *buf, size_t bufsz, dssh_uint32_t *val)
{
	if (val == NULL || buf == NULL)
		return DSSH_ERROR_INVALID;
	if (bufsz < 4)
		return DSSH_ERROR_PARSE;
	*val = (((uint32_t)buf[0]) << 24) | (((uint32_t)buf[1]) << 16) | (((uint32_t)buf[2]) << 8) | buf[3];
	return 4;
}

DSSH_PUBLIC int
dssh_serialize_uint32(dssh_uint32_t val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (pos == NULL || buf == NULL)
		return DSSH_ERROR_INVALID;
	if (*pos > bufsz || bufsz - *pos < 4)
		return DSSH_ERROR_TOOLONG;
	buf[(*pos)++] = (uint8_t)((val >> 24) & 0xff);
	buf[(*pos)++] = (uint8_t)((val >> 16) & 0xff);
	buf[(*pos)++] = (uint8_t)((val >> 8) & 0xff);
	buf[(*pos)++] = (uint8_t)(val & 0xff);
	return 0;
}

