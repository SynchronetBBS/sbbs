#include <stdlib.h>
#include <string.h>

#include "tith-strings.h"

/*
 * Basically the same as the POSIX strdup()
 */
char *tith_strDup(const char *str)
{
	size_t sz = strlen(str);
	char *ret = malloc(sz + 1);
	if (ret == NULL)
		return ret;
	strcpy(ret, str);
	return ret;
}
