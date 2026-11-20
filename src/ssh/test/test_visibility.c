/* Actual C17 shared-library import and private-symbol lookup. */
#include <stdio.h>
#include "visibility_probe.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

int
main(int argc, char **argv)
{
	if (argc != 2)
		return 2;
#ifdef _WIN32
	HMODULE module = GetModuleHandleA(argv[1]);
	if (module == NULL || GetProcAddress(module, "dssh_visibility_probe") == NULL
	    || GetProcAddress(module, "dssh_visibility_private") != NULL)
		return 1;
#else
	(void)argv;
#ifdef DSSH_TEST_ELF_ARCHIVE
	if (dlsym(RTLD_DEFAULT, "dssh_visibility_archive") != NULL)
		return 1;
#endif
	if (dlsym(RTLD_DEFAULT, "dssh_visibility_probe") == NULL
	    || dlsym(RTLD_DEFAULT, "dssh_visibility_private") != NULL)
		return 1;
#endif
	int value = dssh_visibility_probe();
	if (printf("visibility_probe public=%d private=0 valid=%d\n", value, value == 42 ? 1 : 0) < 0
	    || fflush(stdout) != 0)
		return 2;
	return value == 42 ? 0 : 1;
}
