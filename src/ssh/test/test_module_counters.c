/* Include the real C fixture producers; only colliding private identifiers and
 * the encoder's native free call are renamed. No producer implementation copy. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "dssh_test_alloc.h"

static void observe_enc_free(void *pointer);
#define init_fail_at enc_init_fail_at
#define init_count enc_init_count
#define init_failure_seen enc_init_failure_seen
#define free observe_enc_free
#include "test_enc.c"
#undef free
#undef init_failure_seen
#undef init_count
#undef init_fail_at
#include "test_mac.c"

static _Thread_local void *watched_enc;
static _Thread_local bool enc_released;
static _Thread_local bool enc_cleared;

static void
observe_enc_free(void *pointer)
{
	if (pointer != NULL && pointer == watched_enc) {
		const uint8_t *bytes = pointer;
		enc_cleared = true;
		for (size_t i = 0; i < sizeof(struct dssh_enc_ctx); i++)
			enc_cleared = bytes[i] == 0 && enc_cleared;
		enc_released = true;
		watched_enc = NULL;
	}
	free(pointer);
}

static bool
retire_enc(dssh_enc_ctx **owner)
{
	if (*owner == NULL)
		return true;
	dssh_enc_ctx *context = *owner;
	*owner = NULL;
	watched_enc = context;
	enc_released = false;
	enc_cleared = false;
	test_enc_cleanup(context);
	return watched_enc == NULL && enc_released && enc_cleared;
}

static bool
retire_mac(dssh_mac_ctx **owner)
{
	if (*owner == NULL)
		return true;
	dssh_mac_ctx *context = *owner;
	*owner = NULL;
	dssh_test_alloc_watch_free(context, sizeof(*context));
	test_mac_cleanup(context);
	bool cleared = false;
	bool released = dssh_test_alloc_watched_free(&cleared);
	dssh_test_alloc_watch_free(NULL, 0);
	return released && cleared;
}

enum operation {ENC_INIT, ENC_ENCRYPT, ENC_DECRYPT, MAC_INIT, MAC_GENERATE};
struct binding {
	const char *name;
	enum operation operation;
	dssh_test_counter *counter;
	void (*arm)(int);
};
static const struct binding bindings[] = {
	{"enc-init", ENC_INIT, &enc_init_count, test_enc_fail_init_at},
	{"enc-encrypt", ENC_ENCRYPT, &encrypt_count, test_enc_fail_encrypt_at},
	{"enc-decrypt", ENC_DECRYPT, &decrypt_count, test_enc_fail_decrypt_at},
	{"mac-init", MAC_INIT, &init_count, test_mac_fail_init_at},
	{"mac-generate", MAC_GENERATE, &generate_count, test_mac_fail_generate_at},
};

enum {WORKERS = 4, EACH = 64, CALLS = WORKERS * EACH};
_Static_assert(CALLS < INT_MAX, "native counter workload fits int");

struct contexts {
	dssh_enc_ctx *enc;
	dssh_mac_ctx *mac[2];
};

static void
reset(void)
{
	test_enc_reset();
	test_mac_reset();
	dssh_test_alloc_reset();
}

static bool
retire(struct contexts *contexts)
{
	bool valid = retire_enc(&contexts->enc);
	valid = retire_mac(&contexts->mac[0]) && valid;
	valid = retire_mac(&contexts->mac[1]) && valid;
	return valid;
}

static bool
prepare(const struct binding *binding, struct contexts *contexts)
{
	uint8_t key[32];
	uint8_t iv[16] = {0};
	memset(key, 0x5a, sizeof(key));
	if (binding->operation == ENC_ENCRYPT || binding->operation == ENC_DECRYPT)
		return test_enc_init(key, iv, true, &contexts->enc) == 0 && contexts->enc != NULL;
	if (binding->operation == MAC_GENERATE)
		return test_mac_init(key, &contexts->mac[0]) == 0 && contexts->mac[0] != NULL
		    && contexts->mac[0]->digest_size == 32
		    && test_mac_oversized_init(key, &contexts->mac[1]) == 0 && contexts->mac[1] != NULL
		    && contexts->mac[1]->digest_size == 128;
	return true;
}

static bool
invoke(const struct binding *binding, struct contexts *contexts, unsigned round, bool *failed)
{
	uint8_t key[32];
	uint8_t iv[16] = {0};
	uint8_t input[16];
	memset(key, 0x5a, sizeof(key));
	memset(input, 0x33, sizeof(input));
	int ret;
	bool valid = true;
	if (binding->operation == ENC_INIT) {
		dssh_enc_ctx *context = NULL;
		ret = test_enc_init(key, iv, true, &context);
		valid = ret == 0 ? context != NULL && memcmp(context->key, key, sizeof(key)) == 0
		    : ret == DSSH_ERROR_INIT && context == NULL;
		valid = retire_enc(&context) && valid;
	}
	else if (binding->operation == MAC_INIT) {
		dssh_mac_ctx *context = NULL;
		ret = test_mac_init(key, &context);
		valid = ret == 0 ? context != NULL && context->digest_size == 32
		    && memcmp(context->key, key, sizeof(key)) == 0 : ret == DSSH_ERROR_INIT && context == NULL;
		valid = retire_mac(&context) && valid;
	}
	else if (binding->operation == MAC_GENERATE) {
		dssh_mac_ctx *context = contexts->mac[round % 2];
		uint8_t output[130];
		memset(output, 0xa5, sizeof(output));
		ret = test_mac_generate(input, sizeof(input), output + 1, context);
		valid = ret == 0 || ret == DSSH_ERROR_INIT;
		for (size_t i = 0; i < sizeof(output); i++) {
			uint8_t expected = 0xa5;
			if (ret == 0 && i >= 1 && i <= context->digest_size) {
				if (i <= sizeof(input))
					expected = 0x69;
				else
					expected = 0;
			}
			valid = output[i] == expected && valid;
		}
		for (size_t i = 0; i < sizeof(input); i++)
			valid = input[i] == 0x33 && valid;
	}
	else {
		ret = binding->operation == ENC_ENCRYPT
		    ? test_enc_encrypt(input, sizeof(input), contexts->enc)
		    : test_enc_decrypt(input, sizeof(input), contexts->enc);
		valid = ret == 0 || ret == DSSH_ERROR_INIT;
		for (size_t i = 0; i < sizeof(input); i++)
			valid = input[i] == (ret == 0 ? 0x69 : 0x33) && valid;
	}
	*failed = ret == DSSH_ERROR_INIT;
	return valid;
}

static bool
invalid_arguments(const struct binding *binding, struct contexts *contexts, unsigned *checked)
{
	uint8_t key[32] = {0}, bytes[130] = {0};
	dssh_enc_ctx *enc = NULL;
	dssh_mac_ctx *mac = NULL;
	int first, second, third = DSSH_ERROR_INVALID;
	*checked = 2;
	if (binding->operation == ENC_INIT) {
		first = test_enc_init(NULL, key, true, &enc);
		second = test_enc_init(key, key, true, NULL);
	}
	else if (binding->operation == MAC_INIT) {
		first = test_mac_init(NULL, &mac);
		second = test_mac_init(key, NULL);
	}
	else if (binding->operation == MAC_GENERATE) {
		first = test_mac_generate(bytes, 1, NULL, contexts->mac[0]);
		second = test_mac_generate(NULL, 1, bytes, contexts->mac[0]);
		third = test_mac_generate(bytes, 1, bytes, NULL);
		*checked = 3;
	}
	else if (binding->operation == ENC_ENCRYPT) {
		first = test_enc_encrypt(bytes, 1, NULL);
		second = test_enc_encrypt(NULL, 1, contexts->enc);
	}
	else {
		first = test_enc_decrypt(bytes, 1, NULL);
		second = test_enc_decrypt(NULL, 1, contexts->enc);
	}
	bool valid = first == DSSH_ERROR_INVALID && second == DSSH_ERROR_INVALID
	    && third == DSSH_ERROR_INVALID && enc == NULL && mac == NULL;
	valid = retire_enc(&enc) && valid;
	valid = retire_mac(&mac) && valid;
	return valid;
}

struct worker {
	const struct binding *binding;
	struct contexts *contexts;
	unsigned failures;
};

static int
worker(void *opaque)
{
	struct worker *worker = opaque;
	bool valid = true;
	for (unsigned i = 0; i < EACH; i++) {
		bool failed = false;
		valid = invoke(worker->binding, worker->contexts, i, &failed) && valid;
		worker->failures += failed ? 1U : 0U; /* At most EACH. */
	}
	return valid ? 0 : 1;
}

static bool
failure_seen(const struct binding *binding)
{
	if (binding->operation == ENC_INIT)
		return test_enc_init_failure_seen();
	if (binding->operation == MAC_INIT)
		return test_mac_init_failure_seen();
	return true; /* Non-init operations expose no failure-seen flag. */
}

static bool
normal_binding(const struct binding *binding)
{
	reset();
	struct contexts contexts = {0};
	bool valid = prepare(binding, &contexts);
	bool failed = false;
	unsigned invalid = 0;
	if (valid) {
		binding->arm(0);
		valid = invalid_arguments(binding, &contexts, &invalid) && atomic_load(binding->counter) == 0;
		valid = invoke(binding, &contexts, 0, &failed) && failed && failure_seen(binding)
		    && atomic_load(binding->counter) == 1 && valid;
		binding->arm(-1);
		for (unsigned i = 0; i < 2; i++)
			valid = invoke(binding, &contexts, i, &failed) && !failed && atomic_load(binding->counter) == 0 && valid;
		binding->arm(1);
		for (unsigned i = 0; i < 3; i++)
			valid = invoke(binding, &contexts, i, &failed) && failed == (i == 1) && valid;
		valid = atomic_load(binding->counter) == 3 && failure_seen(binding) && valid;
		binding->arm(INT_MAX - 1);
		atomic_store(binding->counter, INT_MAX - 1);
		valid = invoke(binding, &contexts, 0, &failed) && failed
		    && atomic_load(binding->counter) == INT_MAX && valid;
		binding->arm(-1);
		atomic_store(binding->counter, INT_MAX);
		valid = invoke(binding, &contexts, 0, &failed) && !failed
		    && atomic_load(binding->counter) == INT_MAX && valid;
	}
	unsigned created = 0, joined = 0, failures = 0;
	struct worker workers[WORKERS];
	thrd_t threads[WORKERS];
	if (valid) {
		binding->arm(137);
		for (unsigned i = 0; i < WORKERS; i++) {
			workers[i] = (struct worker){.binding = binding, .contexts = &contexts};
			if (thrd_create(&threads[i], worker, &workers[i]) != thrd_success) {
				valid = false;
				break;
			}
			created++;
		}
	}
	for (unsigned i = 0; i < created; i++) {
		int result;
		if (thrd_join(threads[i], &result) != thrd_success)
			abort();
		joined++;
		failures += workers[i].failures; /* At most WORKERS * EACH. */
		valid = result == 0 && valid;
	}
	valid = created == WORKERS && joined == WORKERS && failures == 1
	    && atomic_load(binding->counter) == CALLS && valid;
	valid = retire(&contexts) && valid;
	reset();
	valid = atomic_load(binding->counter) == 0 && !test_enc_init_failure_seen()
	    && !test_mac_init_failure_seen() && valid;
	unsigned calls = CALLS;
	if (printf("module_counter binding=%s invalid=%u serial=3 disarmed=3 boundary=1 calls=%u failures=%u created=%u joined=%u cleared=1 valid=%u\n",
	    binding->name, invalid, calls, failures, created, joined, valid ? 1U : 0U) < 0 || fflush(stdout) != 0)
		return false;
	return valid;
}

int
main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "normal") == 0) {
		bool valid = true;
		for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++)
			valid = normal_binding(&bindings[i]) && valid;
		if (printf("module_counters families=5 calls=1280 created=20 joined=20 valid=%u\n", valid ? 1U : 0U) < 0
		    || fflush(stdout) != 0)
			return 1;
		return valid ? 0 : 1;
	}
	if (argc != 3 || (strcmp(argv[2], "negative") != 0 && strcmp(argv[2], "exhausted") != 0))
		return 2;
	const struct binding *binding = NULL;
	for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++) {
		if (strcmp(argv[1], bindings[i].name) == 0)
			binding = &bindings[i];
	}
	if (binding == NULL)
		return 2;
	reset();
	struct contexts contexts = {0};
	if (!prepare(binding, &contexts)) {
		bool released = retire(&contexts);
		(void)released; /* Setup already failed. */
		return 1;
	}
	binding->arm(0);
	atomic_store(binding->counter, strcmp(argv[2], "negative") == 0 ? -1 : INT_MAX);
	if (printf("module_counter_ready binding=%s mode=%s valid=1\n", binding->name, argv[2]) < 0 || fflush(stdout) != 0) {
		bool released = retire(&contexts);
		(void)released;
		return 1;
	}
	bool failed = false;
	bool valid = invoke(binding, &contexts, 0, &failed);
	valid = retire(&contexts) && valid;
	(void)valid; /* A return means the required arithmetic refusal did not occur. */
	return 3;
}
