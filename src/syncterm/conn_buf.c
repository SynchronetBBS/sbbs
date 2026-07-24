/* Copyright (C), 2007 by Stephen Hurd */

#include <stdlib.h>

#include "conn.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "term.h"
#include "threadwrap.h"

struct conn_buffer *
create_conn_buf(struct conn_buffer *buf, size_t size)
{
	buf->buf = (unsigned char *)malloc(size);
	if (buf->buf == NULL)
		return NULL;
	buf->bufsize = size;
	atomic_init(&buf->head, 0);
	atomic_init(&buf->tail, 0);
	buf->data_event = NULL;
	buf->space_event = NULL;
	if (pthread_mutex_init(&(buf->read_mutex), NULL)) {
		FREE_AND_NULL(buf->buf);
		return NULL;
	}
	if (pthread_mutex_init(&(buf->write_mutex), NULL)) {
		FREE_AND_NULL(buf->buf);
		pthread_mutex_destroy(&(buf->read_mutex));
		return NULL;
	}
	buf->data_event = CreateEvent(NULL, /* manual reset */ FALSE,
	    /* initial state */ FALSE, NULL);
	if (buf->data_event == NULL) {
		FREE_AND_NULL(buf->buf);
		pthread_mutex_destroy(&(buf->write_mutex));
		pthread_mutex_destroy(&(buf->read_mutex));
		return NULL;
	}
	buf->space_event = CreateEvent(NULL, /* manual reset */ FALSE,
	    /* initial state */ FALSE, NULL);
	if (buf->space_event == NULL) {
		FREE_AND_NULL(buf->buf);
		CloseEvent(buf->data_event);
		buf->data_event = NULL;
		pthread_mutex_destroy(&(buf->write_mutex));
		pthread_mutex_destroy(&(buf->read_mutex));
		return NULL;
	}
	return buf;
}

void
destroy_conn_buf(struct conn_buffer *buf)
{
	if (buf->buf != NULL) {
		FREE_AND_NULL(buf->buf);
		CloseEvent(buf->data_event);
		CloseEvent(buf->space_event);
		buf->data_event = NULL;
		buf->space_event = NULL;
		pthread_mutex_destroy(&(buf->write_mutex));
		pthread_mutex_destroy(&(buf->read_mutex));
	}
}

/* Discard all pending bytes in the buffer.  Both sides are locked so
 * reset remains synchronous even when called by a third thread. */
void
conn_buf_reset(struct conn_buffer *buf)
{
	size_t head;

	assert_pthread_mutex_lock(&(buf->write_mutex));
	assert_pthread_mutex_lock(&(buf->read_mutex));
	head = atomic_load_explicit(&buf->head, memory_order_acquire);
	atomic_store_explicit(&buf->tail, head, memory_order_release);
	SetEvent(buf->space_event);
	assert_pthread_mutex_unlock(&(buf->read_mutex));
	assert_pthread_mutex_unlock(&(buf->write_mutex));
}

/*
 * The caller holds read_mutex for get/peek/wait-bytes, and write_mutex
 * for put/wait-free.  Atomic indices publish data between the two lock
 * domains without making the payload atomic.
 */
size_t
conn_buf_bytes(struct conn_buffer *buf)
{
	size_t tail = atomic_load_explicit(&buf->tail, memory_order_acquire);
	size_t head = atomic_load_explicit(&buf->head, memory_order_acquire);
	return head - tail;
}

size_t
conn_buf_free(struct conn_buffer *buf)
{
	return buf->bufsize - conn_buf_bytes(buf);
}

/*
 * Copies up to outlen bytes from the buffer into outbuf,
 * leaving them in the buffer.  Returns the number of bytes
 * copied out of the buffer
 */
size_t
conn_buf_peek(struct conn_buffer *buf, void *voutbuf, size_t outlen)
{
	unsigned char *outbuf = (unsigned char *)voutbuf;
	size_t         copy_bytes;
	size_t         chunk;
	size_t         tail;

	tail = atomic_load_explicit(&buf->tail, memory_order_relaxed);
	copy_bytes = atomic_load_explicit(&buf->head, memory_order_acquire) - tail;
	if (copy_bytes > outlen)
		copy_bytes = outlen;
	chunk = buf->bufsize - (tail % buf->bufsize);
	if (chunk > copy_bytes)
		chunk = copy_bytes;

	if (chunk)
		memcpy(outbuf, buf->buf + (tail % buf->bufsize), chunk);
	if (chunk < copy_bytes)
		memcpy(outbuf + chunk, buf->buf, copy_bytes - chunk);

	return copy_bytes;
}

/*
 * Copies up to outlen bytes from the buffer into outbuf,
 * removing them from the buffer.  Returns the number of
 * bytes removed from the buffer.
 */
size_t
conn_buf_get(struct conn_buffer *buf, void *voutbuf, size_t outlen)
{
	unsigned char *outbuf = (unsigned char *)voutbuf;
	size_t         ret;
	size_t         tail;

	tail = atomic_load_explicit(&buf->tail, memory_order_relaxed);
	ret = conn_buf_peek(buf, outbuf, outlen);
	if (ret) {
		atomic_store_explicit(&buf->tail, tail + ret, memory_order_release);
		SetEvent(buf->space_event);
		/* A second reader may already be asleep after releasing
		 * read_mutex in conn_buf_wait_cond(). */
		if (conn_buf_bytes(buf) != 0)
			SetEvent(buf->data_event);
	}
	return ret;
}

/*
 * Places up to outlen bytes from outbuf into the buffer
 * returns the number of bytes written into the buffer
 */
size_t
conn_buf_put(struct conn_buffer *buf, const void *voutbuf, size_t outlen)
{
	const unsigned char *outbuf = (unsigned char *)voutbuf;
	size_t               write_bytes;
	size_t               chunk;
	size_t               head;

	head = atomic_load_explicit(&buf->head, memory_order_relaxed);
	write_bytes = buf->bufsize
	    - (head - atomic_load_explicit(&buf->tail, memory_order_acquire));
	if (write_bytes > outlen)
		write_bytes = outlen;
	if (write_bytes) {
		chunk = buf->bufsize - (head % buf->bufsize);
		if (chunk > write_bytes)
			chunk = write_bytes;
		if (chunk)
			memcpy(buf->buf + (head % buf->bufsize), outbuf, chunk);
		if (chunk < write_bytes)
			memcpy(buf->buf, outbuf + chunk, write_bytes - chunk);
		atomic_store_explicit(&buf->head, head + write_bytes, memory_order_release);
		SetEvent(buf->data_event);
		/* A second writer may already be asleep after releasing
		 * write_mutex in conn_buf_wait_cond(). */
		if (conn_buf_free(buf) != 0)
			SetEvent(buf->space_event);
		/* Wake the doterm() main loop on remote-data arrival.  The
		 * outbuf gets put-to from the main thread itself; waking
		 * there is harmless (next WaitForEvent returns immediately,
		 * costs one extra loop iteration) but inelegant -- gate on
		 * the in-direction buffer.  Pointer-compare is safe because
		 * conn_inbuf / conn_outbuf are file-scope globals. */
		if (buf == &conn_inbuf)
			doterm_wake();
	}
	return write_bytes;
}

/*
 * Waits up to timeout milliseconds for bcount bytes to be available/free
 * in the buffer.  Events are only wakeup hints: their state may be stale
 * or coalesced, and xpevent tracks its own waiters.  The atomic buffer
 * predicate is authoritative after every wake.
 */
size_t
conn_buf_wait_cond(struct conn_buffer *buf, size_t bcount, unsigned long timeout, int do_free)
{
	long double      now;
	long double      end;
	size_t           found;
	unsigned long    timeleft;
	DWORD            wait_result;
	xpevent_t        event;
	pthread_mutex_t *mutex;
	size_t         (*cond)(struct conn_buffer *buf);

	if (do_free) {
		event = buf->space_event;
		mutex = &buf->write_mutex;
		cond = conn_buf_free;
	}
	else {
		event = buf->data_event;
		mutex = &buf->read_mutex;
		cond = conn_buf_bytes;
	}

	found = cond(buf);
	if (found > bcount)
		found = bcount;

	if ((found == bcount) || (timeout == 0))
		return found;

	end = timeout;
	end /= 1000;
	now = xp_timer();
	end += now;

	for (;;) {
		assert_pthread_mutex_unlock(mutex);

		found = cond(buf);
		if (found > bcount)
			found = bcount;

		now = xp_timer();
		if (end <= now) {
			timeleft = 0;
		}
		else {
			timeleft = (end - now) * 1000;
			if ((timeleft < 1) || (timeleft > timeout))
				timeleft = 1;
		}
		if (found == bcount)
			wait_result = WAIT_OBJECT_0;
		else
			wait_result = WaitForEvent(event, timeleft);

		assert_pthread_mutex_lock(mutex);
		found = cond(buf);
		if (found > bcount)
			found = bcount;

		if ((found == bcount) || wait_result != WAIT_OBJECT_0)
			return found;
	}
}
