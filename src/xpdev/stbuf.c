#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "stbuf.h"

static bool stbuf_realloc(stbuf *buf, size_t newsz);

stbuf
stbuf_malloc(size_t bufsz)
{
	size_t asz;
	stbuf ret;

	assert(bufsz <= STBUF_MAX);
	if (bufsz > STBUF_MAX)
		return NULL;
	asz = STBUF_OFFSET + bufsz + 1;
	ret = malloc(asz);
	if (ret) {
		struct stbuf_raw *stb = (struct stbuf_raw *)ret;

		stb->sz = bufsz;
		stb->len = 0;
		stb->dynamic = true;
		stb->buf[bufsz] = 0;
	}
	return ret;
}

stbuf
stbuf_zalloc(size_t bufsz)
{
	size_t asz;
	stbuf ret;

	assert(bufsz <= STBUF_MAX);
	if (bufsz > STBUF_MAX)
		return NULL;

	asz = STBUF_OFFSET + bufsz + 1;
	ret = calloc(1, asz);
	if (ret) {
		struct stbuf_raw *stb = (struct stbuf_raw *)ret;

		stb->sz = bufsz;
		stb->dynamic = true;
	}
	return ret;
}


stbuf
stbuf_frommem(void *mem, size_t sz, bool allocated)
{
	stbuf ret = (stbuf)mem;
	struct stbuf_raw *stb = (struct stbuf_raw *)mem;

	assert(mem);
	if (mem == NULL)
		return NULL;
	if (sz < STBUF_OFFSET + 1)
		return NULL;
	stb->sz = sz - STBUF_OFFSET - 1;
	stb->len = 0;
	stb->buf[0] = 0;
	stb->dynamic = allocated;
	return ret;
}

bool
stbuf_lipo(stbuf *buf)
{
	// assert()ing twice is fine, run-time checking twice is silly
	assert(buf);
	assert(*buf);
	if (!buf)
		return false;
	if (!(*buf))
		return false;
	return stbuf_realloc(buf, (*buf)->len + STBUF_OFFSET + 1);
}

void
stbuf_free(stbuf buf)
{
	if (!buf)
		return;
	assert(buf->dynamic);
	if (buf->dynamic)
		free(buf);
}

/*
 * This takes the new allocation size, not the buffer size
 */
static bool
stbuf_realloc(stbuf *buf, size_t newsz)
{
	struct stbuf_raw *nbuf;
	struct stbuf_raw **stb = (struct stbuf_raw **)buf;

	assert(buf);
	if (!buf)
		return false;
	assert(newsz >= STBUF_OFFSET + 1);
	if (newsz < STBUF_OFFSET + 1)
		return false;
	if (*buf) {
		// Ensure we're not shrinking smaller than the current length
		assert(newsz >= (*buf)->len + STBUF_OFFSET + 1);
		if (newsz < (*buf)->len + STBUF_OFFSET + 1)
			return false;
		// Ensure we're allowed to realloc()
		assert((*buf)->dynamic);
		if (!(*buf)->dynamic)
			return false;
	}

	nbuf = realloc(*buf, newsz);
	if (!nbuf)
		return false;
	if (*buf == NULL)
		nbuf->dynamic = true;
	nbuf->sz = newsz - STBUF_OFFSET - 1;

	*buf = (stbuf)nbuf;
	return true;
}

size_t
stbuf_msz(stbuf buf)
{
	assert(buf);
	if (!buf)
		return 0;
	return STBUF_SIZE(buf->sz);
}

bool
stbuf_truncate(stbuf buf, size_t len)
{
	struct stbuf_raw *stb = (struct stbuf_raw *)buf;

	assert(buf);
	assert(len <= STBUF_MAX);
	if (!buf)
		return false;
	if (buf->len > len) {
		stb->len = len;
		stb->buf[buf->len] = 0;
		return true;
	}
	return false;
}

bool
stbuf_atleast(stbuf *buf, size_t newsz)
{
	struct stbuf_raw **stb = (struct stbuf_raw **)buf;

	assert(buf);
	if (!buf)
		return false;
	assert(newsz <= STBUF_MAX);
	if (newsz > STBUF_MAX)
		return false;

	if (*buf) {
		if (newsz <= (*buf)->sz)
			return true;
	}

	// Convert to new allocation size
	newsz += STBUF_OFFSET + 1;
	return stbuf_realloc(buf, newsz);
}

bool
stbuf_memrepl(stbuf *buf, size_t start, size_t rlen, const void *mem, size_t ilen)
{
	struct stbuf_raw **dstb = (struct stbuf_raw **)buf;
	size_t remain;
	size_t olen;	// Original length
	size_t nlen;	// New length at end
	size_t end;	// End of inserted data

	assert(buf);
	if (!buf)
		return false;
	if (*buf)
		olen = (*buf)->len;
	else
		olen = 0;
	assert(start <= olen);
	if (start > olen)
		return false;
	if (rlen > (olen - start))
		rlen = olen - start;
	assert(mem || ilen == 0);
	if (ilen && !mem)
		return false;
	// First, remain is the maximum number of bytes we can still fit in the buffer
	remain = STBUF_MAX - olen;
	remain += rlen;
	assert(ilen <= remain);
	if (ilen > remain)
		return false;
	nlen = olen - rlen + ilen;
	if (!stbuf_atleast(buf, nlen))
		return false;
	// Then it's how many bytes we're moving to make room
	remain = olen - start - rlen;
	if (remain) {
		memmove(&(*dstb)->buf[start + ilen], &(*buf)->buf[start + rlen], remain + 1);
	}
	else {
		(*dstb)->buf[nlen] = 0;
	}
	if (ilen) {
		memcpy(&(*dstb)->buf[start], mem, ilen);
	}
	(*dstb)->len = nlen;
	return true;
}
