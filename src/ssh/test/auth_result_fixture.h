#ifndef DSSH_TEST_AUTH_RESULT_FIXTURE_H
#define DSSH_TEST_AUTH_RESULT_FIXTURE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-result.h"
#include "deucessh-auth.h"

/* Fixture producer allocation, independent of the reported output length. */
struct test_auth_output {
	struct test_auth_output *next;
	size_t capacity;
	void (*observe_release)(const void *storage, size_t size, void *data);
	void *observe_data;
	_Alignas(max_align_t) uint8_t bytes[];
};

static inline void
test_auth_output_release(void *data)
{
	struct test_auth_output *owner = data;
	while (owner != NULL) {
		struct test_auth_output *next = owner->next;
		size_t size = sizeof(*owner) + owner->capacity;
		void (*observe)(const void *, size_t, void *) = owner->observe_release;
		void *observe_data = owner->observe_data;
		dssh_cleanse(owner, size);
		if (observe != NULL)
			observe(owner, size, observe_data);
		free(owner);
		owner = next;
	}
}

static inline void *
test_auth_lease_alloc(struct dssh_result_lease *lease, size_t capacity)
{
	if (capacity > SIZE_MAX - sizeof(struct test_auth_output))
		return NULL;
	struct test_auth_output *owner = malloc(sizeof(*owner) + capacity);
	if (owner == NULL)
		return NULL;
	owner->capacity = capacity;
	owner->next = lease->owner;
	owner->observe_release = NULL;
	owner->observe_data = NULL;
	lease->release = test_auth_output_release;
	lease->owner = owner;
	return owner->bytes;
}

static inline uint8_t *
test_auth_output_alloc(struct dssh_bytes_result *result, size_t capacity)
{
	return test_auth_lease_alloc(&result->lease, capacity);
}

static inline void *
test_auth_lease_calloc(struct dssh_result_lease *lease, size_t count, size_t size)
{
	if (size != 0 && count > SIZE_MAX / size)
		return NULL;
	size_t capacity = count * size;
	void *storage = test_auth_lease_alloc(lease, capacity);
	if (storage != NULL)
		memset(storage, 0, capacity);
	return storage;
}

static inline struct dssh_bytes
test_auth_lease_text(struct dssh_result_lease *lease, const char *text)
{
	size_t length = strlen(text);
	uint8_t *storage = test_auth_lease_alloc(lease, length);
	struct dssh_bytes result = { storage, length };
	if (storage != NULL && length > 0)
		memcpy(storage, text, length);
	return result;
}

static inline int
test_auth_output_finish(struct dssh_bytes_result *result, uint8_t *data, size_t length, int status)
{
	result->bytes.data = data;
	result->bytes.len = length;
	return status;
}

#endif
