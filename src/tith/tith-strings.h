#ifndef TITH_STRINGS_HEADER
#define TITH_STRINGS_HEADER

/*
 * Basically a clone of the POSIX strdup()
 */
char *tith_strDup(const char *str);

/*
 * Copies len bytes from str, and adds a terminating NUL
 */
char *tith_memDup(const void *str, size_t len);

#endif
