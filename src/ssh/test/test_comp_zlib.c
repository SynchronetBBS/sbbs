/* Optional example-provider adapter. */
#include "test_comp.h"
#ifdef DSSH_TEST_HAVE_ZLIB
#include "../examples/zlib-comp.h"
unsigned
dssh_test_zlib_resets(void)
{
	return dssh_example_zlib_resets();
}
void
dssh_test_zlib_fail_after(int count)
{
	dssh_example_zlib_fail_after(count);
}
bool
dssh_test_zlib_available(void)
{
	return true;
}
int
dssh_test_register_zlib_comp(void)
{
	return dssh_example_register_zlib_comp();
}
void
dssh_test_zlib_stats(unsigned *allocated, unsigned *remaining, unsigned *errors)
{
	dssh_example_zlib_stats(allocated, remaining, errors);
}
#else
unsigned
dssh_test_zlib_resets(void)
{
	return 0;
}
void
dssh_test_zlib_fail_after(int count)
{
	(void)count;
}
bool
dssh_test_zlib_available(void)
{
	return false;
}
int
dssh_test_register_zlib_comp(void)
{
	return -1;
}
void
dssh_test_zlib_stats(unsigned *allocated, unsigned *remaining, unsigned *errors)
{
	*allocated = 0;
	*remaining = 0;
	*errors = 0;
}
#endif
