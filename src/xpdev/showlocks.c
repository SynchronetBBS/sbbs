#include <stdlib.h>
#include <string.h>
#include "filewrap.h"

int main(int argc, char** argv)
{
	int access = O_RDWR;
	int argn = 1;
	if (strcmp(argv[argn], "-r") == 0) {
		access = O_RDONLY;
		++argn;
	}
	const char* path = argv[argn++];

	int         file = open(path, access);

	if (file == -1) {
		perror(path);
		return EXIT_FAILURE;
	}

	off_t len = filelength(file);
	for (off_t o = 0; o < len; ++o) {
		if (lock(file, o, 1) != 0)
			printf("Byte 0x%" PRIx64 " is locked\n", o);
		else
			unlock(file, o, 1);
	}
	close(file);
	return EXIT_SUCCESS;
}
