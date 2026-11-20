#ifndef DSSH_TEST_KBI_RESULT_PROVIDER_H
#define DSSH_TEST_KBI_RESULT_PROVIDER_H

#include "deucessh-auth.h"

struct test_kbi_result_provider {
	uint8_t storage[64];
	struct dssh_bytes answers[2];
	struct dssh_auth_prompt prompts[2];
	unsigned mode;
	unsigned calls;
	unsigned releases;
	size_t oversized_length;
	bool valid;
	bool leased;
	bool server;
	struct dssh_result_lease *lease;
	const uint8_t **input_text;
	const size_t *input_lengths;
	const bool *input_echo;
	void (*observe_release)(void *data);
	void *observe_data;
};

int test_kbi_result_client(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len, uint32_t count,
    const uint8_t **prompts, const size_t *lengths, const bool *echo,
    struct dssh_auth_responses *out, void *data);
int test_kbi_result_server(const uint8_t *user, size_t user_len, uint32_t count,
    const uint8_t **responses, const size_t *lengths,
    struct dssh_auth_challenge *out, void *data);

#endif
