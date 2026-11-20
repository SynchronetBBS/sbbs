/* Public headers only. All returned storage belongs to this producer. */
#include <string.h>
#include "kbi_result_provider.h"

static bool
bytes_zero(const void *storage, size_t length)
{
	const uint8_t *bytes = storage;
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != 0)
			return false;
	}
	return true;
}

static void
release_result(void *owner)
{
	struct test_kbi_result_provider *state = owner;
	state->valid = state->valid && state->leased && state->lease != NULL
	    && state->lease->release == NULL && state->lease->owner == NULL;
	dssh_result_release(state->lease);
	state->lease = NULL;
	dssh_cleanse(state->storage, sizeof(state->storage));
	dssh_cleanse(state->answers, sizeof(state->answers));
	dssh_cleanse(state->prompts, sizeof(state->prompts));
	state->leased = false;
	state->releases++;
	if (state->observe_release != NULL)
		state->observe_release(state->observe_data);
}

static void
prepare_result(struct test_kbi_result_provider *state, struct dssh_result_lease *lease)
{
	state->valid = state->valid && !state->leased;
	if (state->calls > 0)
		state->valid = state->valid && bytes_zero(state->storage, sizeof(state->storage))
		    && bytes_zero(state->answers, sizeof(state->answers))
		    && bytes_zero(state->prompts, sizeof(state->prompts));
	state->calls++;
	memset(state->storage, 0x65, sizeof(state->storage));
	state->storage[0] = 0;
	state->storage[1] = 'N';
	state->storage[2] = 'I';
	state->storage[3] = 0;
	state->storage[4] = 'p';
	state->storage[5] = 0;
	state->storage[6] = 'r';
	state->storage[7] = 0;
	lease->release = release_result;
	lease->owner = state;
	state->lease = lease;
	state->leased = true;
}

static void
observe_inputs(struct test_kbi_result_provider *state, uint32_t count,
    const uint8_t **text, const size_t *lengths, const bool *echo)
{
	if (count == 0) {
		state->valid = state->valid && text == NULL && lengths == NULL && echo == NULL;
		return;
	}
	state->valid = state->valid && count <= 2 && text != NULL && lengths != NULL;
	if (!state->server)
		state->valid = state->valid && echo != NULL;
	if (!state->valid)
		return;
	/* Same-size rounds retain metadata arrays; a growing round may replace
	 * them. The third round in mode 14 verifies retained larger capacity. */
	bool same_capacity = state->mode != 14 || state->calls == 3;
	if (same_capacity && state->input_text != NULL)
		state->valid = state->valid && text == state->input_text && lengths == state->input_lengths
		    && echo == state->input_echo;
	state->input_text = text;
	state->input_lengths = lengths;
	state->input_echo = echo;
	for (uint32_t i = 0; i < count; i++) {
		state->valid = state->valid && lengths[i] == 2;
		if (lengths[i] == 2)
			state->valid = state->valid && text[i][0] == (state->server ? 'r' : 'p') && text[i][1] == 0;
		if (!state->server)
			state->valid = state->valid && echo[i] == (i != 0);
	}
}

int
test_kbi_result_client(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len, uint32_t count,
    const uint8_t **prompts, const size_t *lengths, const bool *echo,
    struct dssh_auth_responses *out, void *data)
{
	struct test_kbi_result_provider *state = data;
	prepare_result(state, &out->lease);
	state->valid = state->valid && name_len == 2 && instruction_len == 2;
	if (name_len == 2 && instruction_len == 2)
		state->valid = state->valid && name[0] == 0 && name[1] == 'N'
		    && instruction[0] == 'I' && instruction[1] == 0;
	observe_inputs(state, count, prompts, lengths, echo);
	out->count = count;
	out->items = count > 0 ? state->answers : NULL;
	for (uint32_t i = 0; i < count && i < 2; i++) {
		state->answers[i].data = &state->storage[6];
		state->answers[i].len = 2;
	}
	if (state->mode == 1 || state->mode == 2 || state->mode == 3)
		state->answers[0].len = SIZE_MAX;
	if (state->mode == 4)
		state->answers[0].data = NULL;
	if (state->mode == 5 || state->mode == 6) {
		out->lease.release = NULL;
		if (state->mode == 5)
			out->lease.owner = NULL;
	}
	if (state->mode == 7) {
		for (uint32_t i = 0; i < count && i < 2; i++) {
			state->answers[i].data = NULL;
			state->answers[i].len = 0;
		}
	}
	if (state->mode == 8)
		out->count = 1;
	if (state->mode == 9)
		out->items = NULL;
	if (state->mode == 10)
		out->count = UINT32_MAX;
	if (state->mode == 11)
		state->answers[0].len = state->oversized_length;
	if (state->mode == 1)
		return -77;
	if (state->mode == 2)
		return 42;
	if (state->mode == 13)
		return DSSH_ERROR_TOOLONG;
	return 0;
}

int
test_kbi_result_server(const uint8_t *user, size_t user_len, uint32_t count,
    const uint8_t **responses, const size_t *lengths,
    struct dssh_auth_challenge *out, void *data)
{
	struct test_kbi_result_provider *state = data;
	prepare_result(state, &out->lease);
	state->valid = state->valid && user_len == 1 && user != NULL;
	if (user_len == 1 && user != NULL)
		state->valid = state->valid && user[0] == 'u';
	observe_inputs(state, count, responses, lengths, NULL);
	out->name = (struct dssh_bytes){state->storage, 2};
	out->instruction = (struct dssh_bytes){&state->storage[2], 2};
	out->count = state->mode == 12 ? 0 : 2;
	out->prompts = state->prompts;
	for (size_t i = 0; i < 2; i++) {
		state->prompts[i].text = (struct dssh_bytes){&state->storage[4], 2};
		state->prompts[i].echo = i != 0;
	}
	if (state->mode == 1 || state->mode == 2 || state->mode == 3)
		out->name.len = SIZE_MAX;
	if (state->mode == 4)
		out->name.data = NULL;
	if (state->mode == 5 || state->mode == 6) {
		out->lease.release = NULL;
		if (state->mode == 5)
			out->lease.owner = NULL;
	}
	if (state->mode == 7)
		state->prompts[0].text.len = 0;
	if (state->mode == 8)
		state->prompts[0].text.data = NULL;
	if (state->mode == 9)
		out->prompts = NULL;
	if (state->mode == 10)
		out->count = UINT32_MAX;
	if (state->mode == 11)
		state->prompts[0].text.len = state->oversized_length;
	if (state->mode == 1)
		return DSSH_AUTH_FAILURE;
	if (state->mode == 2)
		return 42;
	if (state->mode == 13)
		return DSSH_AUTH_DISCONNECT;
	return state->calls > 2 ? DSSH_AUTH_SUCCESS : DSSH_AUTH_KBI_PROMPT;
}
