#ifndef INC_1OOM_LIB_H
#define INC_1OOM_LIB_H

#include <stdlib.h>

extern void *lib_malloc(size_t size);
extern void *lib_realloc(void *p, size_t size);
extern void lib_free(void *ptr);
extern char *lib_stralloc(const char *str);
extern char *lib_strcpy(char *dst, const char *src, size_t dst_bufsize);
extern char *lib_strcat(char *dst, const char *src, size_t dst_bufsize);
extern int lib_sprintf(char *buf, size_t bufsize, const char *fmt, ...);

/* strbuild_*: build up strings piece by piece, checking the buffer size. */

struct strbuild_s {
    char *str_start;
    char *str_end;     /* Points to the terminating '\0' byte. */
    size_t remaining;  /* Buffer size after str_end. */
};

struct strbuild_s strbuild_init(char *buf, size_t bufsize);
const char *strbuild_finish(struct strbuild_s *str);
void strbuild_append_char(struct strbuild_s *str, char c);
void strbuild_catf(struct strbuild_s *str, const char *fmt, ...);

#endif
