/* Deliberately public headers only; output storage belongs to this producer. */
#include <stdatomic.h>
#include <string.h>
#include "deucessh.h"
#include "signature_result_provider.h"

static atomic_uint ownerless_releases;

static bool
bytes_zero(const uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != 0)
			return false;
	}
	return true;
}

static void
release_owned(void *owner)
{
	struct test_signature_result_provider *state = owner;
	state->valid = state->valid && state->lease != NULL
	    && state->lease->release == NULL && state->lease->owner == NULL;
	dssh_result_release(state->lease);
	state->lease = NULL;
	dssh_cleanse(state->storage, sizeof(state->storage));
	state->releases++;
	if (state->observe_release != NULL)
		state->observe_release(state->observe_data);
}

static void
release_ownerless(void *owner)
{
	if (owner == NULL)
		atomic_fetch_add_explicit(&ownerless_releases, 1, memory_order_relaxed);
}

unsigned
test_signature_ownerless_releases(void)
{
	return atomic_load_explicit(&ownerless_releases, memory_order_relaxed);
}

int
test_signature_result_produce(struct dssh_bytes_result *out,
    const uint8_t *data, size_t length, dssh_key_algo_ctx *context)
{
	struct test_signature_result_provider *state = (struct test_signature_result_provider *)context;
	if (state->calls != 0)
		state->valid = state->valid && bytes_zero(state->storage, sizeof(state->storage));
	state->valid = state->valid && data != NULL && length > 0;
	state->calls++;
	memset(state->storage, 0x6b, sizeof(state->storage));
	state->storage[0] = 's';
	state->storage[1] = 0;
	state->storage[2] = 'g';
	out->bytes.data = state->storage;
	out->bytes.len = 3;
	out->lease.release = release_owned;
	out->lease.owner = state;
	state->lease = &out->lease;
	if (state->mode == 1 || state->mode == 2 || state->mode == 3)
		out->bytes.len = SIZE_MAX;
	if (state->mode == 4)
		out->bytes.data = NULL;
	if (state->mode == 5 || state->mode == 6) {
		out->lease.release = NULL;
		if (state->mode == 5)
			out->lease.owner = NULL;
	}
	if (state->mode == 6 || state->mode == 7) {
		out->bytes.data = NULL;
		out->bytes.len = 0;
	}
	if (state->mode == 8) {
		static const uint8_t signature[] = {'s', 0, 'g'};
		out->bytes.data = signature;
		out->lease.release = release_ownerless;
		out->lease.owner = NULL;
	}
	if (state->mode == 10)
		out->bytes.len = state->oversized_length;
	if (state->mode == 1)
		return -77;
	if (state->mode == 2)
		return 42;
	return 0;
}
