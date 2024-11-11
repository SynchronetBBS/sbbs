#include <stdlib.h>
#include "filewrap.h"

int main(int argc, char** argv)
{
	int access = O_RDWR;
	const char* path = argv[1];

	if(argc < 4) {
		fprintf(stderr, "usage: lockfile path offset length\n");
		return 0;
	}

	int file = open(path, access);
	if(file < 0) {
		puts("Open failure");
		perror(path);
		return -1;
	}

	int result = lock(file, atoi(argv[2]), atoi(argv[3]));
	if(result != 0) {
		puts("Lock failure");
		perror(path);
		return -1;
	}
	printf("Hit enter\n");
	getchar();
	close(file);
	return 0;
}

