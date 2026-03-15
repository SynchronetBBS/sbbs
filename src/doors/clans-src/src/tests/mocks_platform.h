/*
 * mocks_platform.h -- stubs for platform wrapper functions (plat_*)
 *
 * Include BEFORE the .c file under test.  Provides no-op or minimal
 * implementations so test binaries link without the real platform layer.
 */
#ifndef MOCKS_PLATFORM_H
#define MOCKS_PLATFORM_H

#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifndef MOCK_PLAT_STRICMP_DEFINED
#define MOCK_PLAT_STRICMP_DEFINED
int plat_stricmp(const char *s1, const char *s2)
{
	return strcasecmp(s1, s2);
}
#endif

#ifndef MOCK_PLAT_DELETEFILE_DEFINED
#define MOCK_PLAT_DELETEFILE_DEFINED
bool plat_DeleteFile(const char *fname)
{
	return (unlink(fname) == 0);
}
#endif

#ifndef MOCK_PLAT_DELAY_DEFINED
#define MOCK_PLAT_DELAY_DEFINED
void plat_Delay(unsigned msec)
{
	(void)msec;
}
#endif

#ifndef MOCK_PLAT_CREATESEMFILE_DEFINED
#define MOCK_PLAT_CREATESEMFILE_DEFINED
bool plat_CreateSemfile(const char *filename, const char *content)
{
	(void)filename;
	(void)content;
	return true;
}
#endif

#ifndef MOCK_PLAT_FSOPEN_DEFINED
#define MOCK_PLAT_FSOPEN_DEFINED
FILE *plat_fsopen(const char *pathname, const char *mode, int shflag)
{
	(void)shflag;
	return fopen(pathname, mode);
}
#endif

#endif /* MOCKS_PLATFORM_H */
