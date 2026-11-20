#ifndef DSSH_TEST_KEY_FILE_CPP_ALLOC_H
#define DSSH_TEST_KEY_FILE_CPP_ALLOC_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
void dssh_test_cpp_new_fail_after(size_t index);
void dssh_test_cpp_new_disable(void);
void dssh_test_cpp_new_fail_nothrow_size(size_t size);
size_t dssh_test_cpp_new_count(void);
bool dssh_test_cpp_new_failed(void);
bool dssh_test_cpp_new_failed_nothrow(void);
size_t dssh_test_cpp_new_failed_size(void);
extern "C" {
#endif
bool dssh_test_key_ed_public(uint8_t *out, size_t capacity, size_t *length);
bool dssh_test_key_ed_signable(const uint8_t *public_blob, size_t public_length);
#ifdef __cplusplus
}
#endif
#endif
