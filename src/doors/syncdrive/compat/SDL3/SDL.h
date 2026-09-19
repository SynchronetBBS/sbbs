/* SDL3/SDL.h -- the handful of SDL calls tdport/src/mem.c makes, on libc, so
 * the vendored file compiles unedited. Nothing else in the build includes
 * SDL; the platform layer is door/host_term.c. */
#ifndef SYNCDRIVE_COMPAT_SDL_H_
#define SYNCDRIVE_COMPAT_SDL_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_malloc  malloc
#define SDL_calloc  calloc
#define SDL_realloc realloc
#define SDL_free    free

static inline const char *SDL_GetError(void)
{
	return strerror(errno);
}

static inline void *SDL_LoadFile(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	void *buf;
	long  n;

	if (f == NULL)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}
	buf = malloc(n > 0 ? (size_t)n : 1);
	if (buf != NULL && fread(buf, 1, (size_t)n, f) != (size_t)n) {
		free(buf);
		buf = NULL;
	}
	fclose(f);
	if (buf != NULL)
		*len = (size_t)n;
	return buf;
}

#endif
