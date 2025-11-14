#include <stdlib.h>

#include "tith-file.h"

char *
readLine(FILE *fp)
{
	if (fp == NULL)
		return NULL;
	char *ret = malloc(1);
	if (ret == NULL)
		return NULL;
	*ret = 0;
	size_t sz = 1;
	size_t len = 0;
	for (;;) {
		int ch = fgetc(fp);
		if (ch == EOF)
			break;
		if (ch < 0 || ch > 0xFF)
			break;
		if (ch < 32 && ch != '\t')
			break;
		if (ch == '\n') {
			char *small = realloc(ret, len + 1);
			if (small == NULL)
				return ret;
			return small;
		}
		if ((len + 1) >= sz) {
			size_t newsz = sz == 1 ? 4096 : sz * 2;
			char *newret = realloc(ret, newsz);
			if (newret == NULL)
				break;
			sz = newsz;
			ret = newret;
		}
		ret[len++] = (char)ch;
		ret[len] = 0;
	}

	free(ret);
	return NULL;
}
