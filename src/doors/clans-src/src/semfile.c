#include <stdio.h>
#ifdef __unix__
# include <sys/stat.h>
# include <unistd.h>
#endif

#include "defines.h"
#include "semfile.h"
#include "video.h"

bool CreateSemaphor(int16_t Node)
{
	FILE *fp;
#ifdef __unix__
	/*
	 * This is the classic O_EXCL|O_CREAT dance for NFS
	 */
	char hostname[256];
	char fname[sizeof(hostname) + 32];
	pid_t pid;
	struct stat st;

	pid = getpid();
	if (gethostname(hostname, sizeof(hostname)))
		return false;
	snprintf(fname, sizeof(fname), "online.%s.%" PRIuMAX, hostname, (uintmax_t)pid);
	fp = fopen(fname, "w+x");
	if (!fp)
		return false;
	fprintf(fp, "Node: %d\n", Node);
	fclose(fp);
	if (link(fname, "online.flg") == 0) {
		unlink(fname);
		return true;
	}
	if (stat(fname, &st)) {
		unlink(fname);
		return false;
	}
	if (st.st_nlink == 2) {
		unlink(fname);
		return true;
	}
	unlink(fname);
	return false;
#else
	fp = fopen("online.flg", "w+x");
	if (!fp)
		return false;
	fprintf(fp, "Node: %d\n", Node);
	fclose(fp);
	return true;
#endif
}

void WaitSemaphor(int16_t Node)
{
	int count = 0;
	while (!CreateSemaphor(Node)) {
		if (count++ == 0)
			DisplayStr("Waiting for online flag to clear\n");
		if (count == 60)
			count = 0;
		sleep(1);
	}
}

void RemoveSemaphor(void)
{
	unlink("online.flg");
}
