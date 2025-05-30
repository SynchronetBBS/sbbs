#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined _MSC_VER && !defined __BORLANDC__
char* itoa(int val, char* str, int radix)
{
	switch (radix) {
		case 8:
			sprintf(str, "%o", val);
			break;
		case 10:
			sprintf(str, "%u", val);
			break;
		case 16:
			sprintf(str, "%x", val);
			break;
		default:
			sprintf(str, "bad radix: %d", radix);
			break;
	}
	return str;
}

char* ltoa(long val, char* str, int radix)
{
	switch (radix) {
		case 8:
			sprintf(str, "%lo", val);
			break;
		case 10:
			sprintf(str, "%lu", val);
			break;
		case 16:
			sprintf(str, "%lx", val);
			break;
		default:
			sprintf(str, "bad radix: %d", radix);
			break;
	}
	return str;
}
#endif

#if defined(_WIN32) && !defined(_MSC_VER)
/* From FreeBSD */
size_t
strnlen(const char *s, size_t maxlen)
{
	size_t len;

	for (len = 0; len < maxlen; len++, s++) {
		if (!*s)
			break;
	}
	return len;
}
#endif

#ifdef _WIN32
/* From FreeBSD */
char *
strndup(const char *str, size_t maxlen)
{
	char * copy;
	size_t len;

	len = strnlen(str, maxlen);
	copy = malloc(len + 1);
	if (copy != NULL) {
		(void)memcpy(copy, str, len);
		copy[len] = '\0';
	}

	return copy;
}
#endif

#if defined(__EMSCRIPTEN__)
char *
strdup(const char *str)
{
	if (str == NULL)
		return NULL;
	char *ret = malloc(strlen(str) + 1);
	if (ret != NULL)
		strcpy(ret, str);
	return ret;
}

char *
strtok_r(char *str, const char *delim, char **saveptr)
{
	char *ret;
	char *end;

	if (str == NULL)
		str = *saveptr;
	if (str == NULL)
		return NULL;
	end = strchr(str, '\0');
	ret = strtok(str, delim);
	if (ret == NULL) {
		*saveptr = NULL;
	}
	else {
		*saveptr = strchr(ret, '\0') + 1;
		if (*saveptr > end)
			*saveptr = NULL;
	}
	return ret;
}
#endif
