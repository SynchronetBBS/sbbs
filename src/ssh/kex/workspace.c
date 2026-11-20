#include <stdbool.h>
#include <stdlib.h>

#include "workspace.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

struct kex_buffer {
	uint8_t *data;
	size_t capacity;
	bool dirty;
};

struct dssh_kex_workspace {
	struct kex_buffer secret;
	struct kex_buffer hash;
	size_t allocation_size;
	size_t slots;
	bool leased;
	bool retired;
	struct kex_buffer scratch[];
};

static void
buffer_clear(struct kex_buffer *buffer)
{
	if (buffer->dirty) {
		dssh_cleanse(buffer->data, buffer->capacity);
		buffer->dirty = false;
	}
}

static void
buffer_free(struct kex_buffer *buffer)
{
	if (buffer->data != NULL) {
		dssh_cleanse(buffer->data, buffer->capacity);
		free(buffer->data);
	}
	*buffer = (struct kex_buffer){0};
}

static int
buffer_reserve(struct kex_buffer *buffer, size_t capacity)
{
	if (capacity > buffer->capacity) {
		uint8_t *storage = calloc(1, capacity);
		if (storage == NULL)
			return DSSH_ERROR_ALLOC;
		buffer_free(buffer);
		buffer->data = storage;
		buffer->capacity = capacity;
	}
	return 0;
}

static void
workspace_free(struct dssh_kex_workspace *workspace)
{
	buffer_free(&workspace->secret);
	buffer_free(&workspace->hash);
	for (size_t i = 0; i < workspace->slots; i++)
		buffer_free(&workspace->scratch[i]);
	size_t size = workspace->allocation_size;
	dssh_cleanse(workspace, size);
	free(workspace);
}

static void
release_result(void *owner)
{
	struct dssh_kex_workspace *workspace = owner;
	buffer_clear(&workspace->secret);
	buffer_clear(&workspace->hash);
	workspace->leased = false;
	if (workspace->retired)
		workspace_free(workspace);
}

DSSH_PRIVATE int
dssh_kex_workspace_create(size_t slots, struct dssh_kex_workspace **out)
{
	if (out == NULL || *out != NULL)
		return DSSH_ERROR_INVALID;
	if (slots > (SIZE_MAX - sizeof(struct dssh_kex_workspace)) / sizeof(struct kex_buffer))
		return DSSH_ERROR_INVALID;
	size_t size = sizeof(struct dssh_kex_workspace) + slots * sizeof(struct kex_buffer);
	struct dssh_kex_workspace *workspace = malloc(size);
	if (workspace == NULL)
		return DSSH_ERROR_ALLOC;
	/* Initialize pointer values as C values, independent of their object
	 * representation. Only the byte buffers use calloc's all-zero bytes. */
	workspace->secret = (struct kex_buffer){0};
	workspace->hash = (struct kex_buffer){0};
	workspace->allocation_size = size;
	workspace->slots = slots;
	workspace->leased = false;
	workspace->retired = false;
	for (size_t i = 0; i < slots; i++)
		workspace->scratch[i] = (struct kex_buffer){0};
	*out = workspace;
	return 0;
}

DSSH_PRIVATE int
dssh_kex_workspace_reset(struct dssh_kex_workspace *workspace)
{
	if (workspace == NULL || workspace->retired || workspace->leased)
		return DSSH_ERROR_INVALID;
	buffer_clear(&workspace->secret);
	buffer_clear(&workspace->hash);
	for (size_t i = 0; i < workspace->slots; i++)
		buffer_clear(&workspace->scratch[i]);
	return 0;
}

DSSH_PRIVATE int
dssh_kex_workspace_get(struct dssh_kex_workspace *workspace, size_t slot,
    size_t capacity, uint8_t **storage)
{
	if (workspace == NULL || workspace->retired || slot >= workspace->slots
	    || capacity == 0 || storage == NULL)
		return DSSH_ERROR_INVALID;
	struct kex_buffer *buffer = &workspace->scratch[slot];
	int ret = buffer_reserve(buffer, capacity);
	if (ret < 0)
		return ret;
	buffer_clear(buffer);
	buffer->dirty = true;
	*storage = buffer->data;
	return 0;
}

DSSH_PRIVATE int
dssh_kex_workspace_drop(struct dssh_kex_workspace *workspace, size_t slot)
{
	if (workspace == NULL || workspace->retired || slot >= workspace->slots)
		return DSSH_ERROR_INVALID;
	buffer_clear(&workspace->scratch[slot]);
	return 0;
}

DSSH_PRIVATE int
dssh_kex_workspace_result(struct dssh_kex_workspace *workspace, size_t secret_size,
    size_t hash_size, struct dssh_result_lease *lease, uint8_t **secret, uint8_t **hash)
{
	if (workspace == NULL || workspace->retired || workspace->leased
	    || secret_size == 0 || hash_size == 0 || lease == NULL || secret == NULL
	    || hash == NULL || secret == hash || lease->release != NULL || lease->owner != NULL)
		return DSSH_ERROR_INVALID;
	int ret = buffer_reserve(&workspace->secret, secret_size);
	if (ret < 0)
		return ret;
	ret = buffer_reserve(&workspace->hash, hash_size);
	if (ret < 0)
		return ret;
	buffer_clear(&workspace->secret);
	buffer_clear(&workspace->hash);
	workspace->secret.dirty = true;
	workspace->hash.dirty = true;
	workspace->leased = true;
	lease->release = release_result;
	lease->owner = workspace;
	*secret = workspace->secret.data;
	*hash = workspace->hash.data;
	return 0;
}

DSSH_PRIVATE void
dssh_kex_workspace_destroy(struct dssh_kex_workspace *workspace)
{
	if (workspace == NULL)
		return;
	workspace->retired = true;
	if (workspace->leased) {
		for (size_t i = 0; i < workspace->slots; i++)
			buffer_free(&workspace->scratch[i]);
	}
	else
		workspace_free(workspace);
}

DSSH_PRIVATE int
dssh_kex_workspace_init_slots(size_t slots, void **state)
{
	if (state == NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_kex_workspace *workspace = *state;
	if (workspace == NULL) {
		int ret = dssh_kex_workspace_create(slots, &workspace);
		if (ret < 0)
			return ret;
		*state = workspace;
		return 0;
	}
	if (workspace->slots != slots)
		return DSSH_ERROR_INVALID;
	return dssh_kex_workspace_reset(workspace);
}

DSSH_PRIVATE void
dssh_kex_workspace_cleanup(void *state)
{
	struct dssh_kex_workspace *workspace = state;
	dssh_kex_workspace_destroy(workspace);
}
