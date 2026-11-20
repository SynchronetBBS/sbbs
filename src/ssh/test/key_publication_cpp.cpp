#include <new>
#include <string>
#include "key_publication_cpp.h"
#include "key_file_cpp_alloc.h"
#include "deucessh.h"
#include "key_algo/key-context-botan.h"

extern "C" void
dssh_test_cpp_operation(int (*operation)(void *), void *opaque, size_t fault,
    struct dssh_test_cpp_operation_result *out)
{
	*out = {DSSH_ERROR_INIT, 0, 0, false, false};
	dssh_test_cpp_new_fail_after(fault);
	try {
		out->status = operation(opaque);
	}
	catch (...) {
		out->escaped = true;
	}
	dssh_test_cpp_new_disable();
	out->calls = dssh_test_cpp_new_count();
	out->injected = dssh_test_cpp_new_failed();
	out->failed_size = dssh_test_cpp_new_failed_size();
}

extern "C" void
dssh_test_cpp_operation_arm(size_t fault)
{
	dssh_test_cpp_new_fail_after(fault);
}

extern "C" void
dssh_test_cpp_context_operation(int (*operation)(void *), void *opaque,
    struct dssh_test_cpp_operation_result *out)
{
	*out = {DSSH_ERROR_INIT, 0, 0, false, false};
	/* sizeof is taken from the actual C++ context, never a C layout guess.
	 * Only matching scalar nothrow allocations count in this control. */
	dssh_test_cpp_new_fail_nothrow_size(sizeof(dssh_botan_key_context));
	try {
		out->status = operation(opaque);
	}
	catch (...) {
		out->escaped = true;
	}
	dssh_test_cpp_new_disable();
	out->calls = dssh_test_cpp_new_count();
	out->injected = dssh_test_cpp_new_failed();
	out->failed_size = dssh_test_cpp_new_failed_size();
}

extern "C" size_t
dssh_test_cpp_password_allocation_size(void)
{
	/* Nonsecret input exercises the exact runtime string constructor used
	 * by the password bridge; do not guess its allocation rounding/SSO. */
	const char sample[256] = {0};
	size_t result = 0;
	dssh_test_cpp_new_fail_after(0);
	try {
		std::string value(sample, sizeof(sample));
		(void)value.size();
	}
	catch (const std::bad_alloc &) {
		if (dssh_test_cpp_new_failed() && dssh_test_cpp_new_count() == 1)
			result = dssh_test_cpp_new_failed_size();
	}
	catch (...) {
	}
	dssh_test_cpp_new_disable();
	return result;
}
