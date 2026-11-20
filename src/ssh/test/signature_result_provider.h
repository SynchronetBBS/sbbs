#ifndef DSSH_TEST_SIGNATURE_RESULT_PROVIDER_H
#define DSSH_TEST_SIGNATURE_RESULT_PROVIDER_H

#include "deucessh-key-algo.h"

struct test_signature_result_provider {
	uint8_t storage[64];
	size_t oversized_length;
	unsigned mode;
	unsigned calls;
	unsigned releases;
	bool valid;
	struct dssh_result_lease *lease;
	void (*observe_release)(void *data);
	void *observe_data;
};

int test_signature_result_produce(struct dssh_bytes_result *out,
    const uint8_t *data, size_t length, dssh_key_algo_ctx *context);
unsigned test_signature_ownerless_releases(void);

#endif
