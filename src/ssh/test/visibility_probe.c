#include "visibility_probe.h"
#ifdef DSSH_TEST_ELF_ARCHIVE
DSSH_PUBLIC int dssh_visibility_archive(void);
#endif

DSSH_PRIVATE int
dssh_visibility_private(void)
{
#ifdef DSSH_TEST_ELF_ARCHIVE
	return dssh_visibility_archive();
#else
	return 42;
#endif
}

DSSH_PUBLIC int
dssh_visibility_probe(void)
{
	return dssh_visibility_private();
}
