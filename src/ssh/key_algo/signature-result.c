#include <stdatomic.h>
#include <stdlib.h>

#include "signature-result.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

struct signature_buffer {
	struct signature_buffer *next;
	struct dssh_signature_pool *pool;
	size_t capacity;
	uint8_t bytes[];
};

struct dssh_signature_pool {
	atomic_bool busy;
	struct signature_buffer *idle;
	size_t active;
	bool retired;
};

/* The critical section only changes private counters/list links. Allocation,
 * clearing and freeing happen after unlocking. Atomic initialization and
 * layout stay in C; C++ producers see only the opaque handle. */
static void
pool_lock(struct dssh_signature_pool *pool)
{
	while (atomic_exchange_explicit(&pool->busy, true, memory_order_acquire)) {
	}
}

static void
pool_unlock(struct dssh_signature_pool *pool)
{
	atomic_store_explicit(&pool->busy, false, memory_order_release);
}

static void
buffer_free(struct signature_buffer *buffer)
{
	size_t size = sizeof(*buffer) + buffer->capacity;
	dssh_cleanse(buffer, size);
	free(buffer);
}

static void
pool_free(struct dssh_signature_pool *pool)
{
	dssh_cleanse(pool, sizeof(*pool));
	free(pool);
}

static void
release_signature(void *owner)
{
	struct signature_buffer *buffer = owner;
	struct dssh_signature_pool *pool = buffer->pool;
	dssh_cleanse(buffer->bytes, buffer->capacity);
	pool_lock(pool);
	pool->active--;
	bool retired = pool->retired;
	bool last = retired && pool->active == 0;
	if (!retired) {
		buffer->next = pool->idle;
		pool->idle = buffer;
	}
	pool_unlock(pool);
	if (retired)
		buffer_free(buffer);
	if (last)
		pool_free(pool);
}

DSSH_PRIVATE int
dssh_signature_pool_create(struct dssh_signature_pool **out)
{
	if (out == NULL || *out != NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_signature_pool *pool = malloc(sizeof(*pool));
	if (pool == NULL)
		return DSSH_ERROR_ALLOC;
	atomic_init(&pool->busy, false);
	pool->idle = NULL;
	pool->active = 0;
	pool->retired = false;
	*out = pool;
	return 0;
}

DSSH_PRIVATE void
dssh_signature_pool_destroy(struct dssh_signature_pool *pool)
{
	if (pool == NULL)
		return;
	pool_lock(pool);
	pool->retired = true;
	struct signature_buffer *idle = pool->idle;
	pool->idle = NULL;
	bool last = pool->active == 0;
	pool_unlock(pool);
	while (idle != NULL) {
		struct signature_buffer *next = idle->next;
		buffer_free(idle);
		idle = next;
	}
	if (last)
		pool_free(pool);
}

DSSH_PRIVATE int
dssh_signature_buffer_acquire(struct dssh_signature_pool *pool, size_t capacity,
    struct dssh_bytes_result *result, uint8_t **storage)
{
	if (pool == NULL || result == NULL || storage == NULL || capacity == 0
	    || result->bytes.data != NULL || result->bytes.len != 0
	    || result->lease.release != NULL || result->lease.owner != NULL)
		return DSSH_ERROR_INVALID;
	if (capacity > SIZE_MAX - sizeof(struct signature_buffer))
		return DSSH_ERROR_INVALID;
	pool_lock(pool);
	if (pool->retired || pool->active == SIZE_MAX) {
		pool_unlock(pool);
		return DSSH_ERROR_INVALID;
	}
	struct signature_buffer **link = &pool->idle;
	while (*link != NULL && (*link)->capacity < capacity)
		link = &(*link)->next;
	struct signature_buffer *buffer = *link;
	if (buffer != NULL)
		*link = buffer->next;
	/* Reserve a reference before allocating outside the lock, so retirement
	 * cannot destroy the pool until allocation rollback/result release. */
	pool->active++;
	pool_unlock(pool);
	if (buffer == NULL) {
		buffer = malloc(sizeof(*buffer) + capacity);
		if (buffer == NULL) {
			pool_lock(pool);
			pool->active--;
			bool last = pool->retired && pool->active == 0;
			pool_unlock(pool);
			if (last)
				pool_free(pool);
			return DSSH_ERROR_ALLOC;
		}
		buffer->pool = pool;
		buffer->capacity = capacity;
	}
	buffer->next = NULL;
	result->bytes.data = buffer->bytes;
	result->lease.release = release_signature;
	result->lease.owner = buffer;
	*storage = buffer->bytes;
	return 0;
}
