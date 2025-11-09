#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int8_t ato8(const char *str, const char *desc)
{
	int ret = atoi(str);

	if (ret < INT8_MIN || ret > INT8_MAX) {
		printf("%s out of range\n", desc);
		exit(EXIT_FAILURE);
	}
	return (int8_t)ret;
}

char atoc(const char *str, const char *desc)
{
	int ret = atoi(str);

	if (ret < CHAR_MIN || ret > CHAR_MAX) {
		printf("%s out of range\n", desc);
		exit(EXIT_FAILURE);
	}
	return (char)ret;
}

int16_t ato16(const char *str, const char *desc)
{
	int ret = atoi(str);

	if (ret < INT16_MIN || ret > INT16_MAX) {
		printf("%s out of range\n", desc);
		exit(EXIT_FAILURE);
	}
	return (int16_t)ret;
}

int32_t ato32(const char *str, const char *desc)
{
	long ret = atol(str);

	if (ret < INT32_MIN || ret > INT32_MAX) {
		printf("%s out of range\n", desc);
		exit(EXIT_FAILURE);
	}
	return (int32_t)ret;
}
