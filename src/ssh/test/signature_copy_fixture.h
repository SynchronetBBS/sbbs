#ifndef DSSH_TEST_SIGNATURE_COPY_FIXTURE_H
#define DSSH_TEST_SIGNATURE_COPY_FIXTURE_H

#include <stdlib.h>
#include <string.h>
#include "deucessh.h"
#include "deucessh-key-algo.h"

/* Verification fixtures own mutable copies for tampering/resizing. This is a
 * test consumer of the selected result API, never a module callback or a
 * library compatibility interface. Producer leases are always released here. */
struct test_signature_copy_node {
	struct test_signature_copy_node *next;
	uint8_t *bytes;
};
struct test_signature_copies {
	struct test_signature_copy_node *head;
};

static inline int
test_signature_copy(struct test_signature_copies *copies, dssh_key_algo_sign sign,
    uint8_t **bytes, size_t *length, const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
	struct dssh_bytes_result result = {0};
	*bytes = NULL;
	*length = 0;
	int ret = sign(&result, data, data_len, ctx);
	if (ret > 0)
		ret = DSSH_ERROR_INVALID;
	if (ret == 0 && (result.bytes.data == NULL || result.bytes.len == 0
	    || result.bytes.len > UINT32_MAX || result.lease.release == NULL))
		ret = DSSH_ERROR_INVALID;
	if (ret == 0) {
		struct test_signature_copy_node *node = malloc(sizeof(*node));
		if (node == NULL)
			ret = DSSH_ERROR_ALLOC;
		else {
			node->bytes = malloc(result.bytes.len);
			if (node->bytes == NULL) {
				free(node);
				ret = DSSH_ERROR_ALLOC;
			}
			else {
				memcpy(node->bytes, result.bytes.data, result.bytes.len);
				node->next = copies->head;
				copies->head = node;
				*bytes = node->bytes;
				*length = result.bytes.len;
			}
		}
	}
	dssh_result_release(&result.lease);
	return ret;
}

static inline void
test_signature_copy_free(struct test_signature_copies *copies, uint8_t *bytes)
{
	struct test_signature_copy_node **link = &copies->head;
	while (*link != NULL) {
		struct test_signature_copy_node *node = *link;
		if (node->bytes == bytes) {
			*link = node->next;
			free(node->bytes);
			free(node);
			return;
		}
		link = &node->next;
	}
}

static inline uint8_t *
test_signature_copy_resize(struct test_signature_copies *copies, uint8_t *bytes, size_t length)
{
	if (length == 0)
		return NULL;
	for (struct test_signature_copy_node *node = copies->head; node != NULL; node = node->next) {
		if (node->bytes == bytes) {
			uint8_t *replacement = realloc(bytes, length);
			if (replacement != NULL)
				node->bytes = replacement;
			return replacement;
		}
	}
	return NULL;
}

static inline int
test_signature_copies_finish(struct test_signature_copies *copies, int status)
{
	while (copies->head != NULL) {
		struct test_signature_copy_node *node = copies->head;
		copies->head = node->next;
		free(node->bytes);
		free(node);
	}
	return status;
}

#endif
