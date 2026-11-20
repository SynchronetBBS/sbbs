#ifndef DSSH_TEST_PASSWORD_RESULT_PROVIDER_H
#define DSSH_TEST_PASSWORD_RESULT_PROVIDER_H

#include <stdbool.h>
#include "deucessh-auth.h"

struct test_password_result_provider {
	uint8_t storage[32];
	unsigned calls;
	unsigned releases;
	unsigned mode;
	bool cleared;
	bool lease_cleared;
	bool previous_cleared;
	struct dssh_result_lease *lease;
	void (*observe_release)(void *data);
	void *observe_data;
};

int test_password_result(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len, struct dssh_bytes_result *out,
    void *cbdata);

#endif
