#ifndef DSSH_TEST_KEY_PUBLICATION_CPP_H
#define DSSH_TEST_KEY_PUBLICATION_CPP_H
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct dssh_test_cpp_operation_result {
	int status;
	size_t calls, failed_size;
	bool injected, escaped;
};
/* The actual C caller/function is invoked with ordinary calling-thread C++
 * new interception active. SIZE_MAX observes without a reachable fault index.
 * No native objects are fabricated and no exceptions escape this bridge. */
void dssh_test_cpp_operation(int (*operation)(void *), void *opaque, size_t fault,
    struct dssh_test_cpp_operation_result *out);
void dssh_test_cpp_operation_arm(size_t fault);
size_t dssh_test_cpp_password_allocation_size(void);
void dssh_test_cpp_context_operation(int (*operation)(void *), void *opaque,
    struct dssh_test_cpp_operation_result *out);
#ifdef __cplusplus
}
#endif
#endif
