/*
 * key_algo/private-key-file-posix.c -- securely create private key files.
 */

#include "private-key-file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

FILE *
dssh_private_key_open(const char *path)
{
	int flags = O_CREAT | O_WRONLY;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
	int fd = open(path, flags, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return NULL;
	if (fchmod(fd, S_IRUSR | S_IWUSR) != 0 || ftruncate(fd, 0) != 0) {
		close(fd);
		return NULL;
	}
	FILE *fp = fdopen(fd, "wb");
	if (fp == NULL)
		close(fd);
	return fp;
}
