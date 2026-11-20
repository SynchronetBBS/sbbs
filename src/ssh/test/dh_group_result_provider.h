#ifndef DSSH_TEST_DH_GROUP_RESULT_PROVIDER_H
#define DSSH_TEST_DH_GROUP_RESULT_PROVIDER_H

#include <stdatomic.h>
#include "deucessh-kex.h"

struct test_dh_group_provider;

struct test_dh_group_slot {
	struct test_dh_group_provider *producer;
	struct dssh_result_lease *lease;
	atomic_bool busy;
	uint8_t p[1024];
	uint8_t g[16];
};

struct test_dh_group_provider {
	const uint8_t *prime;
	size_t prime_len;
	unsigned mode;
	atomic_uint calls;
	atomic_uint releases;
	atomic_bool valid;
	void (*observe_release)(void *data);
	void *observe_data;
	struct test_dh_group_slot slots[2];
};

void test_dh_group_init(struct test_dh_group_provider *provider, const uint8_t *prime, size_t length);
void test_dh_group_dispose(struct test_dh_group_provider *provider);
int test_dh_group_select(uint32_t min, uint32_t preferred, uint32_t max,
    struct dssh_dh_gex_group_result *out, void *data);
unsigned test_dh_group_static_releases(void);

#endif
