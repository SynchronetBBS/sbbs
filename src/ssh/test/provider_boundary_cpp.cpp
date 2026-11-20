/* Native ELF application allocator controls; no library header is compiled as C++. */
#include "key_file_cpp_alloc.h"
extern "C" {
void dssh_provider_cpp_arm(void)
{
	dssh_test_cpp_new_fail_after(0);
}
void dssh_provider_cpp_disable(void)
{
	dssh_test_cpp_new_disable();
}
bool dssh_provider_cpp_threw(void)
{
	return dssh_test_cpp_new_failed() && !dssh_test_cpp_new_failed_nothrow();
}
}
