#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <xpprintf.h>

#include "sftp.h"

static sftp_str_t
alloc_str(uint32_t len)
{
	sftp_str_t ret = (sftp_str_t)malloc(offsetof(struct sftp_string, c_str) + len + 1);
	if (ret != NULL) {
		ret->len = len;
		ret->c_str[len] = 0;
	}
	return ret;
}

sftp_str_t
sftp_strdup(const char *str)
{
	size_t len = strlen(str);
	if (len > UINT32_MAX)
		return NULL;
	sftp_str_t ret = alloc_str(len);
	if (ret == NULL)
		return ret;
	memcpy(ret->c_str, str, ret->len);
	return ret;
}

sftp_str_t
sftp_asprintf(const char *format, ...)
{
	va_list va;

	va_start(va, format);
	char *str = xp_asprintf(format, va);
	va_end(va);
	if (str == NULL)
		return NULL;
	size_t len = strlen(str);
	if (len > UINT32_MAX) {
		free(str);
		return NULL;
	}
	sftp_str_t ret = alloc_str(len);
	if (ret == NULL) {
		free(str);
		return NULL;
	}
	memcpy(ret->c_str, str, ret->len);
	free(str);
	return ret;
}

sftp_str_t
sftp_memdup(uint8_t *buf, uint32_t sz)
{
	sftp_str_t ret = alloc_str(sz);
	if (ret == NULL)
		return ret;
	memcpy(ret->c_str, buf, sz);
	return ret;
}

void
free_sftp_str(sftp_str_t str)
{
	free(str);
}
