#include <stdlib.h>
#include <string.h>
#include "filewrap.h"

int main(int argc, char** argv)
{
	int access = O_RDWR;

	if (argc < 4) {
		fprintf(stderr, "usage: lockfile [-r] path offset length\n");
		printf("-r     open file read-only instead of read/write\n");
		return 0;
	}

	int argn = 1;
	if (strcmp(argv[argn], "-r") == 0) {
		access = O_RDONLY;
		++argn;
	}
	const char* path = argv[argn++];
	int         file = open(path, access);
	if (file < 0) {
		puts("Open failure");
		perror(path);
		return -1;
	}

	int result = lock(file, atoi(argv[argn]), atoi(argv[argn + 1]));
	if (result != 0) {
		puts("Lock failure");
		perror(path);
		return -1;
	}
	printf("Hit enter\n");
	getchar();
	close(file);
	return 0;
}

