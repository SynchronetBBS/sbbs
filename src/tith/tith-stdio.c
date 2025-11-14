#include <inttypes.h>
#include <stdio.h>

#include "tith-interface.h"

int
getByte(void *handle)
{
	(void)handle;
	int ret = fgetc(stdin);
	if (ret < 0 || ret > UINT8_MAX)
		return -1;
	return ret;
}

bool
getBytes(void *handle, uint8_t *buf, size_t bufsz)
{
	(void)handle;
	return fread(buf, bufsz, 1, stdin) == 1;
}

void shutdownRead(void *handle)
{
	(void)handle;
	fclose(stdin);
}

bool
sendByte(void *handle, uint8_t ch)
{
	(void)handle;
	return fputc(ch, stdout) != EOF;
}

bool
sendBytes(void *handle, uint8_t *buf, size_t bufsz)
{
	(void)handle;
	return fwrite(buf, bufsz, 1, stdout) == 1;
}

bool
flushWrite(void *handle)
{
	(void)handle;
	return !fflush(stdout);
}

void
shutdownWrite(void *handle)
{
	(void)handle;
	fclose(stdout);
}

void
closeConnection(void *handle)
{
	(void)handle;
	return;
}

void
logString(const char *str)
{
	fprintf(stderr, "%s\n", str);
	fflush(stderr);
}

int
main(int argc, char **argv)
{
	return TITH_main(argc, argv, NULL);
}
