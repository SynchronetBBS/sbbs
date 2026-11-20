/* Native exec control: only a nonsensitive test file is identified. */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	if (argc != 4)
		return 2;
	char *end = NULL;
	errno = 0;
	long value = strtol(argv[1], &end, 10);
	if (errno != 0 || end == argv[1] || *end != 0 || value < 0 || value > INT_MAX)
		return 2;
	int fd = (int)value;
	bool expected_open = strcmp(argv[3], "open") == 0;
	if (!expected_open && strcmp(argv[3], "closed") != 0)
		return 2;
	struct stat original, observed;
	if (stat(argv[2], &original) != 0)
		return 2;
	errno = 0;
	int ret = fstat(fd, &observed);
	bool valid_query = ret == 0 || errno == EBADF;
	bool same = ret == 0 && original.st_dev == observed.st_dev && original.st_ino == observed.st_ino;
	bool valid = valid_query && same == expected_open;
	printf("key fd exec: expected=%s observed-original=%u valid=%u\n", argv[3], same ? 1U : 0U, valid ? 1U : 0U);
	return valid ? 0 : 1;
}
