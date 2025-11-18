#include <stdlib.h>
#include <string.h>

#include "tith-strings.h"

/*
 * Basically the same as the POSIX strdup()
 */
char *
tith_strDup(const char *str)
{
	size_t sz = strlen(str);
	char *ret = malloc(sz + 1);
	if (ret == NULL)
		return ret;
	strcpy(ret, str);
	return ret;
}

/*
 * Copies len bytes from str, and adds a terminating NUL
 */
char *
tith_memDup(const void *str, size_t len)
{
	char *ret = malloc(len + 1);
	if (ret == NULL)
		return ret;
	memcpy(ret, str, len);
	ret[len] = 0;
	return ret;
}
