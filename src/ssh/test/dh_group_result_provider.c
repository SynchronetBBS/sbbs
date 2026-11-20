/* Separately compiled producer: installed public types and standard C only. */
#include <string.h>
#include "dh_group_result_provider.h"

static atomic_uint static_releases;

static bool
zero_bytes(const uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != 0)
			return false;
	}
	return true;
}

static void
release_group_slot(void *owner)
{
	struct test_dh_group_slot *slot = owner;
	struct test_dh_group_provider *provider = slot->producer;
	if (slot->lease == NULL || slot->lease->release != NULL || slot->lease->owner != NULL)
		atomic_store(&provider->valid, false);
	dssh_result_release(slot->lease);
	slot->lease = NULL;
	dssh_cleanse(slot->p, sizeof(slot->p));
	dssh_cleanse(slot->g, sizeof(slot->g));
	atomic_fetch_add(&provider->releases, 1);
	if (provider->observe_release != NULL)
		provider->observe_release(provider->observe_data);
	atomic_store(&slot->busy, false);
}

static void
release_static_group(void *owner)
{
	if (owner == NULL)
		atomic_fetch_add(&static_releases, 1);
}

unsigned
test_dh_group_static_releases(void)
{
	return atomic_load(&static_releases);
}

void
test_dh_group_init(struct test_dh_group_provider *provider, const uint8_t *prime, size_t length)
{
	provider->prime = prime;
	provider->prime_len = length;
	provider->mode = 0;
	provider->observe_release = NULL;
	provider->observe_data = NULL;
	atomic_init(&provider->calls, 0);
	atomic_init(&provider->releases, 0);
	atomic_init(&provider->valid, true);
	for (size_t i = 0; i < sizeof(provider->slots) / sizeof(provider->slots[0]); i++) {
		struct test_dh_group_slot *slot = &provider->slots[i];
		slot->producer = provider;
		slot->lease = NULL;
		atomic_init(&slot->busy, false);
		memset(slot->p, 0, sizeof(slot->p));
		memset(slot->g, 0, sizeof(slot->g));
	}
}

/* Test teardown also reclaims intentionally contract-violating omitted leases.
 * Call only after every operation/worker has finished; do not touch stale leases. */
void
test_dh_group_dispose(struct test_dh_group_provider *provider)
{
	for (size_t i = 0; i < sizeof(provider->slots) / sizeof(provider->slots[0]); i++) {
		struct test_dh_group_slot *slot = &provider->slots[i];
		dssh_cleanse(slot->p, sizeof(slot->p));
		dssh_cleanse(slot->g, sizeof(slot->g));
		slot->lease = NULL;
		atomic_store(&slot->busy, false);
	}
}

int
test_dh_group_select(uint32_t min, uint32_t preferred, uint32_t max,
    struct dssh_dh_gex_group_result *out, void *data)
{
	struct test_dh_group_provider *provider = data;
	if (out == NULL || provider == NULL || min > preferred || preferred > max
	    || provider->prime == NULL || provider->prime_len == 0
	    || provider->prime_len > sizeof(provider->slots[0].p))
		return DSSH_ERROR_INVALID;
	if (out->p.data != NULL || out->p.len != 0 || out->g.data != NULL || out->g.len != 0
	    || out->lease.release != NULL || out->lease.owner != NULL) {
		atomic_store(&provider->valid, false);
		return DSSH_ERROR_INVALID;
	}
	atomic_fetch_add(&provider->calls, 1);
	if (provider->mode == 14) {
		static const uint8_t generator = 2;
		out->p = (struct dssh_bytes){provider->prime, provider->prime_len};
		out->g = (struct dssh_bytes){&generator, 1};
		out->lease.release = release_static_group;
		return 0;
	}
	struct test_dh_group_slot *slot = NULL;
	for (size_t i = 0; i < sizeof(provider->slots) / sizeof(provider->slots[0]); i++) {
		bool expected = false;
		if (atomic_compare_exchange_strong(&provider->slots[i].busy, &expected, true)) {
			slot = &provider->slots[i];
			break;
		}
	}
	if (slot == NULL)
		return DSSH_ERROR_ALLOC;
	if (!zero_bytes(slot->p, sizeof(slot->p)) || !zero_bytes(slot->g, sizeof(slot->g)))
		atomic_store(&provider->valid, false);
	memset(slot->p, 0x6b, sizeof(slot->p));
	memset(slot->g, 0x6b, sizeof(slot->g));
	memcpy(slot->p, provider->prime, provider->prime_len);
	slot->g[0] = 2;
	slot->lease = &out->lease;
	out->p = (struct dssh_bytes){slot->p, provider->prime_len};
	out->g = (struct dssh_bytes){slot->g, 1};
	out->lease.release = release_group_slot;
	out->lease.owner = slot;
	if (provider->mode == 1 || provider->mode == 2 || provider->mode == 3)
		out->p.len = SIZE_MAX;
	if (provider->mode == 4)
		out->g.len = SIZE_MAX;
	if (provider->mode == 5)
		out->p.data = NULL;
	if (provider->mode == 6)
		out->g.data = NULL;
	if (provider->mode == 7)
		out->p.len = 0;
	if (provider->mode == 8)
		out->g.len = 0;
	if (provider->mode == 9) {
		out->lease.release = NULL;
		out->lease.owner = NULL;
	}
	if (provider->mode == 10) {
		out->p = (struct dssh_bytes){0};
		out->g = (struct dssh_bytes){0};
		out->lease.release = NULL;
	}
	if (provider->mode == 11)
		out->g = out->p;
	if (provider->mode == 12)
		out->p.len = 255;
	if (provider->mode == 13)
		memset(slot->p, 0, provider->prime_len);
	if (provider->mode == 15) {
		out->p = (struct dssh_bytes){NULL, SIZE_MAX};
		out->g = (struct dssh_bytes){NULL, SIZE_MAX};
	}
	if (provider->mode == 1 || provider->mode == 15)
		return -77;
	if (provider->mode == 2)
		return 42;
	return 0;
}
