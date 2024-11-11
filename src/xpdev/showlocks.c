#include "filewrap.h"

void show_locks(const char* path)
{
	int file = open(path, O_RDWR);

	if(file == -1) {
		perror(path);
		return;
	}

	off_t len = filelength(file);
	for(off_t o = 0; o < len; ++o) {
		if(lock(file, o, 1) != 0)
			printf("Byte 0x%" PRIx64 " is locked\n", o);
		else
			unlock(file, o, 1);
	}
	close(file);
}

int main(int argc, char** argv)
{
	for(int i = 1; i < argc; ++i)
		show_locks(argv[i]);
	return 0;
} 
