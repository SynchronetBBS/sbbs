#ifndef _XP_CA_FILE_H
#define _XP_CA_FILE_H

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>

#include "dirwrap.h"
#include "genwrap.h"

#if defined(_WIN32)
	#include <io.h>
	#include <windows.h>
#else
	#include <fcntl.h>
	#include <unistd.h>
#endif

static inline void
xp_ca_scrub_memory(void *memory, size_t size)
{
	volatile unsigned char *byte = (volatile unsigned char *)memory;

	while (size-- != 0)
		*byte++ = 0;
}

static FILE *
xp_ca_open_private_temporary(const char *path, char *temporary,
                             size_t temporary_size)
{
	for (unsigned attempt = 0; attempt < 100; attempt++) {
		if (snprintf(temporary, temporary_size, "%s.tmp.%ld.%u", path,
		             (long)getpid(), attempt) >= (int)temporary_size) {
			errno = ENAMETOOLONG;
			return NULL;
		}
#if defined(_WIN32)
		FILE *file = fopen(temporary, "wbx");
		if (file == NULL) {
			if (errno == EEXIST)
				continue;
			return NULL;
		}
		if (CHMOD(temporary, S_IREAD | S_IWRITE) == 0)
			return file;
		fclose(file);
		remove(temporary);
		return NULL;
#else
		int descriptor = open(
			temporary, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if (descriptor < 0) {
			if (errno == EEXIST)
				continue;
			return NULL;
		}
		FILE *file = fdopen(descriptor, "wb");
		if (file != NULL)
			return file;
		int saved_errno = errno;
		close(descriptor);
		remove(temporary);
		errno = saved_errno;
		return NULL;
#endif
	}
	errno = EEXIST;
	return NULL;
}

static int
xp_ca_sync_file(FILE *file)
{
	if (fflush(file) != 0)
		return -1;
#if defined(_WIN32)
	return _commit(_fileno(file));
#else
	return fsync(fileno(file));
#endif
}

static int
xp_ca_replace_file(const char *temporary, const char *path)
{
#if defined(_WIN32)
	return MoveFileExA(temporary, path,
	                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0 ? 0 : -1;
#else
	return rename(temporary, path);
#endif
}

static int
xp_ca_commit_private_temporary(FILE *file, const char *temporary,
                               const char *path)
{
	int result = xp_ca_sync_file(file);
	if (fclose(file) != 0)
		result = -1;
	if (result == 0)
		result = xp_ca_replace_file(temporary, path);
	if (result != 0)
		remove(temporary);
	return result;
}

static void
xp_ca_discard_private_temporary(FILE *file, const char *temporary)
{
	if (file != NULL)
		fclose(file);
	remove(temporary);
}

#endif
