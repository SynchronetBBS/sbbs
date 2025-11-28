#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>

#include "tith-common.h"

/*
 * Generates a context for reading directory contents of the passed
 * path.  The passed path must be to a directory.
 * 
 * Returns NULL on failure.
 */
void *
openDirectory(const char *path)
{
	return opendir(path);
}

/*
 * Reads the next entry from a context returned by openDirectory()
 * 
 * The returned entry does not include the path, and must remain valid
 * until the next call to either readDirectory() or closeDirectory() on
 * the directory handle dhandle
 * 
 * returns NULL when the end of the directory is reached or an error
 * occurs
 */
const char *
readDirectory(void *dhandle)
{
	struct dirent *de = readdir((DIR *)dhandle);
	return de->d_name;
}

/*
 * Frees all resources associated with the directory handle and makes
 * future calls to readDirectory() with the specified handle return NULL
 * (unless the handle is returned from a later call to openDirectory).
 */
void
closeDirectory(void *dhandle)
{
	closedir((DIR *)dhandle);
}

bool isDir(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return false;
	if (S_ISDIR(st.st_mode))
		return true;
	return false;
}
