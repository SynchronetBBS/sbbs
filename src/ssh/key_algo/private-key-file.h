#ifndef DSSH_PRIVATE_KEY_FILE_H
#define DSSH_PRIVATE_KEY_FILE_H

#include <stdio.h>
#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

FILE *dssh_private_key_open(const char *path);
DSSH_PRIVATE FILE *dssh_key_file_open_read(const char *path);
DSSH_PRIVATE FILE *dssh_key_file_open_public(const char *path);

/* Builtin module setup eligibility only; name is a nonnull module constant.
 * Publication remains a separate successful-install operation. */
DSSH_PRIVATE int dssh_key_algo_setup_check(const char *name);

#ifdef DSSH_TESTING
/* Library-only stdio observations; provider-internal stdio is not redirected. */
FILE *dssh_test_key_file_fopen(const char *path, const char *mode);
int dssh_test_key_file_close(FILE *stream);
int dssh_test_key_file_setvbuf(FILE *stream, char *buffer, int mode, size_t capacity);
void dssh_test_key_file_cleared(const void *ptr, size_t length);
size_t dssh_test_key_file_read(void *ptr, size_t size, size_t count, FILE *stream);
size_t dssh_test_key_file_write(const void *ptr, size_t size, size_t count, FILE *stream);
int dssh_test_key_file_put(int byte, FILE *stream);
#ifdef __cplusplus
void *dssh_test_malloc(size_t size);
void dssh_test_free(void *ptr);
#define malloc(size) dssh_test_malloc(size)
#define free(ptr) dssh_test_free(ptr)
#endif
#define fopen(path, mode) dssh_test_key_file_fopen(path, mode)
#define setvbuf(stream, buffer, mode, capacity) dssh_test_key_file_setvbuf(stream, buffer, mode, capacity)
#define fclose(stream) dssh_test_key_file_close(stream)
#define fread(ptr, size, count, stream) dssh_test_key_file_read(ptr, size, count, stream)
#define fwrite(ptr, size, count, stream) dssh_test_key_file_write(ptr, size, count, stream)
#define fputc(byte, stream) dssh_test_key_file_put(byte, stream)
#endif


#ifdef __cplusplus
}
#endif

#endif
