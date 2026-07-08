#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

void *lib_malloc(size_t size)
{
    void *ptr = malloc(size);

    if ((ptr == NULL) && (size > 0)) {
        log_fatal_and_die("couldn't malloc %lu, out of memory!\n", size);
    }

    if (ptr) {
        memset(ptr, 0, size);
    }

    return ptr;
}

void *lib_realloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);

    if (new_ptr == NULL) {
        log_fatal_and_die("couldn't realloc %lu, out of memory!\n", size);
    }

    return new_ptr;
}

void lib_free(void *ptr)
{
    free(ptr);
}

char *lib_stralloc(const char *str)
{
    size_t size;
    char *ptr;

    if (str == NULL) {
        exit(EXIT_FAILURE);
    }

    size = strlen(str) + 1;
    ptr = lib_malloc(size);

    memcpy(ptr, str, size);
    return ptr;
}

char *lib_strcpy(char *dst, const char *src, size_t dst_bufsize)
{
    if (strlen(src) >= dst_bufsize) {
        log_fatal_and_die("lib_strcpy: destination buffer too small, need %lu, have %lu\n", strlen(src) + 1, dst_bufsize);
    }
    return strcpy(dst, src);
}

char *lib_strcat(char *dst, const char *src, size_t dst_bufsize)
{
    size_t combined_len = strlen(src) + strlen(dst);
    /* Check for wrap-around. */
    if (combined_len < strlen(src)) {
        log_fatal_and_die("lib_strcat: Combined string length too large.");
    }
    if (combined_len >= dst_bufsize) {
        log_fatal_and_die("lib_strcat: destination buffer too small, need %lu, have %lu\n", combined_len + 1, dst_bufsize);
    }
    return strcat(dst, src);
}

int lib_sprintf(char *buf, size_t bufsize, const char *fmt, ...)
{
    va_list args;
    int bytes_to_print;
    va_start(args, fmt);
    bytes_to_print = vsnprintf(buf, bufsize, fmt, args);
    if (bytes_to_print < 0) {
        /* Error */
        log_fatal_and_die("lib_sprintf: vsnprintf: %s", strerror(errno));
    }
    else if (bytes_to_print >= bufsize) {
        /* Truncated */
        log_fatal_and_die("lib_sprintf: buffer too small, need %d, have %lu", bytes_to_print, bufsize);
    }
    return bytes_to_print;
}

/* strbuild_*: build up strings piece by piece, checking the buffer size. */

static void strbuild_fatal_too_small(struct strbuild_s *str)
{
    log_fatal_and_die("string buffer too small for: \"%s...\"\n", str->str_start);
}

struct strbuild_s strbuild_init(char *buf, size_t bufsize)
{
    if (bufsize > 0) {
        buf[0] = '\0';
    }
    struct strbuild_s output = { buf, buf, bufsize };
    return output;
}

/* Return the string being built and start a new one after it, storing multiple
 * strings in the same buffer. */
const char *strbuild_finish(struct strbuild_s *str)
{
    const char *old_str = str->str_start;
    if (str->remaining > 0) {
        *++(str->str_end) = '\0';
        str->remaining -= 1;
    }
    str->str_start = str->str_end;
    /* If str->remaining was 0, we will log_fatal_and_die when anything
     * more is written to this string. */
    return old_str;
}

void strbuild_append_char(struct strbuild_s *str, char c)
{
    if (str->remaining > 0) {
        *str->str_end++ = c;
        *str->str_end = '\0';
        str->remaining -= 1;
    }
    else {
        strbuild_fatal_too_small(str);
    }
}

/* Print text at the end of the current string. */
void strbuild_catf(struct strbuild_s *str, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int bytes_added = vsnprintf(str->str_end, str->remaining, fmt, args);
    if (bytes_added < 0) {
        /* Error */
        log_fatal_and_die("str_catf: vsnprintf: %s", strerror(errno));
    }
    else if (bytes_added >= str->remaining) {
        /* Truncated */
        strbuild_fatal_too_small(str);
    }
    str->str_end += bytes_added;
    str->remaining -= bytes_added;
}
