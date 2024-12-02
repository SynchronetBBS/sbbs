#include "filewrap.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
	int access = O_RDWR;
	char* share = "NWA";
	bool try_all = true;
	bool loop = false;
	bool rm = false;

	if(argc < 2) {
		printf("usage: sopenfile [-r] [-l] <path/filename> [share-mode]\n");
		printf("\n");
		printf("-r           open file read-only instead of read/write\n");
		printf("-c           open file with create permissions\n");
		printf("-R           remove file after open\n");
		printf("-l           loop until failure\n");
		printf("\n");
		printf("share-mode:  N (deny-none)\n");
		printf("             W (deny-write)\n");
		printf("             A (deny-all)\n");
		return EXIT_SUCCESS;
	}
	int argn = 1;
	if(strcmp(argv[argn], "-r") == 0) {
		access = O_RDONLY;
		++argn;
	}
	if(strcmp(argv[argn], "-c") == 0) {
		access |= O_CREAT;
		++argv;
	}
	if(strcmp(argv[argn], "-R") == 0) {
		rm = true;
		++argn;
	}
	if(strcmp(argv[argn], "-l") == 0) {
		loop = true;
		++argn;
	}
	const char* path = argv[argn++];
	if(argc > argn) {
		share = argv[argn++];
		try_all = false;
	}

	do {
		for(int i = 0; share[i] != '\0'; ++i) {
			int share_mode = 0;
			char share_flag = share[i];
			switch(toupper(share_flag)) {
				case 'N':
					share_mode = SH_DENYNO;
					break;
				case 'W':
					share_mode = SH_DENYWR;
					break;
				case 'A':
					share_mode = SH_DENYRW;
					break;
				default:
					fprintf(stderr, "Unrecognized share-mode: %c\n", share_flag);
					return EXIT_FAILURE;
			}
			fprintf(stderr, "%s Deny-%c (share mode %x): ", path, toupper(share_flag), share_mode);
			int file = sopen(path, access, share_mode, DEFFILEMODE);
			if(file < 0)
				fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
			else {
				printf("Success\n");
				if(rm)
					printf("remove(%s) = %d\n", path, unlink(path));
				if(!try_all) {
					fprintf(stderr, "Hit enter\n");
					getchar();
				}
				close(file);
			}
		}
	} while(loop && errno == 0);
	return errno == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

