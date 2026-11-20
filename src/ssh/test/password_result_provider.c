/* Deliberately public headers only: producer owns all returned storage. */
#include <string.h>
#include "password_result_provider.h"

static bool
bytes_zero(const uint8_t *data, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		if (data[i] != 0)
			return false;
	}
	return true;
}

static void
release_password(void *owner)
{
	struct test_password_result_provider *state = owner;
	state->lease_cleared = state->lease_cleared && state->lease != NULL
	    && state->lease->release == NULL && state->lease->owner == NULL;
	/* Repeated cleanup of the cleared lease must not call us again. */
	dssh_result_release(state->lease);
	state->lease = NULL;
	dssh_cleanse(state->storage, sizeof(state->storage));
	state->cleared = bytes_zero(state->storage, sizeof(state->storage));
	state->releases++;
	if (state->observe_release != NULL)
		state->observe_release(state->observe_data);
}

int
test_password_result(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len, struct dssh_bytes_result *out,
    void *cbdata)
{
	(void)prompt;
	(void)prompt_len;
	(void)language;
	(void)language_len;
	struct test_password_result_provider *state = cbdata;
	if (state->calls != 0)
		state->previous_cleared = state->previous_cleared
		    && bytes_zero(state->storage, sizeof(state->storage));
	state->calls++;
	memset(state->storage, 0x69, sizeof(state->storage));
	memcpy(state->storage, "new", 3);
	out->bytes.data = state->storage;
	out->bytes.len = 3;
	out->lease.release = release_password;
	out->lease.owner = state;
	state->lease = &out->lease;
	if (state->mode == 1 || state->mode == 2 || state->mode == 3)
		out->bytes.len = SIZE_MAX;
	if (state->mode == 4)
		out->bytes.data = NULL;
	if (state->mode == 5) {
		out->lease.release = NULL;
		out->lease.owner = NULL;
	}
	if (state->mode == 6) {
		out->bytes.data = NULL;
		out->bytes.len = 0;
		out->lease.release = NULL;
	}
	if (state->mode == 7) {
		out->bytes.data = NULL;
		out->bytes.len = 0;
	}
	if (state->mode == 1)
		return -77;
	if (state->mode == 2)
		return 42;
	return 0;
}
