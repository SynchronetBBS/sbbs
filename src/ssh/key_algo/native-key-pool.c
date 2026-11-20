#include <stdatomic.h>
#include <stdlib.h>

#include "native-key-pool.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
#include "ssh-internal.h"
#endif

struct native_key_slot {
	struct native_key_slot *next;
	struct dssh_native_key_pool *pool;
	void *value;
};

struct dssh_native_key_pool {
	atomic_bool busy;
	struct native_key_slot *idle;
	size_t active;
	bool retired;
	bool draining;
	void *factory;
	dssh_native_key_clone clone;
	dssh_native_key_destroy destroy;
	dssh_native_key_destroy destroy_factory;
};

/* Only counter/list bookkeeping runs in this critical section. */
static void
pool_lock(struct dssh_native_key_pool *pool)
{
	while (atomic_exchange_explicit(&pool->busy, true, memory_order_acquire)) {
	}
}

static void
pool_unlock(struct dssh_native_key_pool *pool)
{
	atomic_store_explicit(&pool->busy, false, memory_order_release);
}

static void
slot_free(struct native_key_slot *slot)
{
	dssh_cleanse(slot, sizeof(*slot));
	free(slot);
}

static void
pool_free(struct dssh_native_key_pool *pool)
{
	pool->destroy_factory(pool->factory);
	dssh_cleanse(pool, sizeof(*pool));
	free(pool);
}

static void
drop_reference(struct dssh_native_key_pool *pool)
{
	pool_lock(pool);
	pool->active--;
	bool last = pool->retired && !pool->draining && pool->active == 0;
	pool_unlock(pool);
	if (last)
		pool_free(pool);
}

static void
release_key(void *owner)
{
	struct native_key_slot *slot = owner;
	struct dssh_native_key_pool *pool = slot->pool;
	pool_lock(pool);
	if (!pool->retired) {
		slot->next = pool->idle;
		pool->idle = slot;
		pool->active--;
		pool_unlock(pool);
		return;
	}
	pool_unlock(pool);
	/* Keep the active reference until destruction finishes: the native
	 * destructor may need its factory while another lease is retiring. */
	pool->destroy(slot->value);
	slot_free(slot);
	drop_reference(pool);
}

DSSH_PRIVATE int
dssh_native_key_pool_create(struct dssh_native_key_pool **out,
    void *factory, void *first, dssh_native_key_clone clone,
    dssh_native_key_destroy destroy, dssh_native_key_destroy destroy_factory)
{
	if (out == NULL || *out != NULL || factory == NULL || first == NULL
	    || clone == NULL || destroy == NULL || destroy_factory == NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_native_key_pool *pool = malloc(sizeof(*pool));
	if (pool == NULL)
		return DSSH_ERROR_ALLOC;
	struct native_key_slot *slot = malloc(sizeof(*slot));
	if (slot == NULL) {
		free(pool);
		return DSSH_ERROR_ALLOC;
	}
	atomic_init(&pool->busy, false);
	pool->idle = slot;
	pool->active = 0;
	pool->retired = false;
	pool->draining = false;
	pool->factory = factory;
	pool->clone = clone;
	pool->destroy = destroy;
	pool->destroy_factory = destroy_factory;
	*slot = (struct native_key_slot){.pool = pool, .value = first};
	*out = pool;
	return 0;
}

DSSH_PRIVATE void
dssh_native_key_pool_destroy(struct dssh_native_key_pool *pool)
{
	if (pool == NULL)
		return;
	pool_lock(pool);
	pool->retired = true;
	pool->draining = true;
	struct native_key_slot *idle = pool->idle;
	pool->idle = NULL;
	pool_unlock(pool);
	while (idle != NULL) {
		struct native_key_slot *next = idle->next;
		pool->destroy(idle->value);
		slot_free(idle);
		idle = next;
	}
	pool_lock(pool);
	pool->draining = false;
	bool last = pool->active == 0;
	pool_unlock(pool);
	if (last)
		pool_free(pool);
}

DSSH_PRIVATE int
dssh_native_key_acquire(struct dssh_native_key_pool *pool,
    struct dssh_native_key_result *result)
{
	if (pool == NULL || result == NULL || result->value != NULL
	    || result->lease.release != NULL || result->lease.owner != NULL)
		return DSSH_ERROR_INVALID;
	pool_lock(pool);
	if (pool->retired || pool->active == SIZE_MAX) {
		pool_unlock(pool);
		return DSSH_ERROR_INVALID;
	}
	struct native_key_slot *slot = pool->idle;
	if (slot != NULL)
		pool->idle = slot->next;
	/* The reservation keeps the factory live through allocation/clone,
	 * including failure cleanup or retirement during that work. */
	pool->active++;
	pool_unlock(pool);
	if (slot == NULL) {
		slot = malloc(sizeof(*slot));
		if (slot == NULL) {
			drop_reference(pool);
			return DSSH_ERROR_ALLOC;
		}
		*slot = (struct native_key_slot){.pool = pool};
		int ret = pool->clone(pool->factory, &slot->value);
		if (ret != 0 || slot->value == NULL) {
			if (slot->value != NULL)
				pool->destroy(slot->value);
			slot_free(slot);
			drop_reference(pool);
			return ret < 0 ? ret : ret > 0 ? DSSH_ERROR_INVALID : DSSH_ERROR_INIT;
		}
	}
	slot->next = NULL;
	result->value = slot->value;
	result->lease.release = release_key;
	result->lease.owner = slot;
	return 0;
}
