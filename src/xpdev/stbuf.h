/*
 * Library for buffers.
 * The buffer can hold any bytes and is dynamically resized as necessary
 * There is always a NUL after the last byte in the buffer
 */

#ifndef STBUF_H
#define STBUF_H

#if __STDC_VERSION__ >= 199901L
# define STBUF_INLINE inline
# define STBUF_RESTRICT restrict
#else
# define STBUF_INLINE
# define STBUF_RESTRICT
#endif

#include <stddef.h>
#include <string.h>

#include "gen_defs.h"

struct stbuf_raw {
	size_t   sz;      // Size of the buffer
	size_t   len;     // Length of the buffer contents
	bool     dynamic; // Can be resized via realloc()
	char     buf[];   // Buffer data
};

typedef struct stbuf_s {
	const size_t sz;      // Size of the buffer (number of bytes it can hold)
	const size_t len;     // Length of the buffer contents
	const bool   dynamic; // Can be resized via realloc()
	const char   buf[];   // Buffer data
} *STBUF_RESTRICT stbuf;

// Offset from the start of the buffer to the data
#define STBUF_OFFSET offsetof(struct stbuf_raw, buf)

// Maximum number of bytes any buffer can hold
#define STBUF_MAX (SIZE_MAX - STBUF_OFFSET - 1)

// Size of a buffer object that can hold the specified length
#define STBUF_SIZE(x) (STBUF_OFFSET + (x) + 1)

/*
 * Allocates a new buffer that will hold bufsz bytes
 * Returns NULL on failure
 */
stbuf stbuf_malloc(size_t bufsz);

/*
 * Allocates and zeros a new buffer that will hold bufsz bytes
 * Returns NULL on failure
 */
stbuf stbuf_zalloc(size_t bufsz);

/*
 * Ensures the current length is less than or equal to len.
 * Returns true if the length was adjusted or false if it was already
 * small enough buf is NULL.
 */
bool stbuf_truncate(stbuf buf, size_t len);

/*
 * Returns the current memory size of the buffer
 */
size_t stbuf_msz(stbuf buf);

/*
 * Resizes the buffer is possible to be able to contain newsz bytes
 * Returns true if the buffer can hold newsz bytes, false otherwise.
 */
bool stbuf_atleast(stbuf *buf, size_t newsz);

/*
 * Attempts to shrink the allocation of buf as small as possible
 * using realloc()
 */
bool stbuf_lipo(stbuf *buf);

/*
 * Creates an empty stbuf in arbitrary memory mem where the memory block
 * size is sz.
 * If allocated is true, mem can be passed to realloc() by the functions,
 * and the address of mem changed. If allocated is false, the memory will
 * not be changed and mem will remain valid.
 */
stbuf stbuf_frommem(void *mem, size_t sz, bool allocated);

/*
 * free()s an stbuf
 */
void stbuf_free(stbuf buf);

/*
 * The main function
 * Replaces rlen bytes in buf starting at offset start with ilen bytes from mem
 * It will resize buf as needed and return false on failure.
 */
bool stbuf_memrepl(stbuf *buf, size_t start, size_t rlen, const void *mem, size_t ilen);

static STBUF_INLINE bool
stbuf_strrepl(stbuf *buf, size_t start, size_t rlen, const char *str)
{
	const size_t len = str ? strlen(str) : 0;
	return stbuf_memrepl(buf, start, rlen, str, len);
}

static STBUF_INLINE bool
stbuf_repl(stbuf *buf, size_t start, size_t rlen, const stbuf stb)
{
	return stbuf_memrepl(buf, start, rlen, stb ? stb->buf : NULL, stb ? stb->len : 0);
}

static STBUF_INLINE bool
stbuf_strcpy(stbuf *buf, const char *str)
{
	return stbuf_strrepl(buf, 0, STBUF_MAX, str);
}

static STBUF_INLINE bool
stbuf_memcpy(stbuf *buf, const void *mem, size_t len)
{
	return stbuf_memrepl(buf, 0, STBUF_MAX, mem, len);
}

static STBUF_INLINE bool
stbuf_cpy(stbuf *buf, const stbuf src)
{
	return stbuf_repl(buf, 0, STBUF_MAX, src);
}

static STBUF_INLINE bool
stbuf_strcat(stbuf *buf, const char *str)
{
	const size_t blen = (buf && *buf) ? (*buf)->len : 0;
	return stbuf_strrepl(buf, blen, 0, str);
}

static STBUF_INLINE bool
stbuf_memcat(stbuf *buf, const void *mem, size_t len)
{
	const size_t blen = (buf && *buf) ? (*buf)->len : 0;
	return stbuf_memrepl(buf, blen, STBUF_MAX, mem, len);
}

static STBUF_INLINE bool
stbuf_cat(stbuf *buf, const stbuf sbuf)
{
	const size_t blen = (buf && *buf) ? (*buf)->len : 0;
	return stbuf_repl(buf, blen, STBUF_MAX, sbuf);
}

static STBUF_INLINE bool
stbuf_memins(stbuf *buf, size_t start, const void *mem, size_t len)
{
	return stbuf_memrepl(buf, start, 0, mem, len);
}

static STBUF_INLINE bool
stbuf_strins(stbuf *buf, size_t start, const char *str)
{
	return stbuf_strrepl(buf, start, 0, str);
}

static STBUF_INLINE bool
stbuf_ins(stbuf *buf, size_t start, const stbuf sbuf)
{
	return stbuf_repl(buf, start, 0, sbuf);
}

static STBUF_INLINE bool
stbuf_rem(stbuf *buf, size_t start, size_t len)
{
	return stbuf_memrepl(buf, start, len, NULL, 0);
}

#undef STBUF_INLINE

#endif
