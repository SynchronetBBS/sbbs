/* Real key publication transactions under every library allocation rejection. */
#include <limits.h>
#include <unistd.h>
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "deucessh-algorithms.h"
#ifdef DSSH_CRYPTO_BOTAN
#include "key_publication_cpp.h"
#endif

struct publication_ops {
	int (*reg)(void);
	int (*load)(const char *, dssh_pem_password_cb, void *);
	int (*save)(const char *, dssh_pem_password_cb, void *);
	int (*generate)(unsigned int);
	int64_t (*text)(char *, size_t);
	const char *name, *cache;
};
static int
ed_generate(unsigned int unused)
{
	(void)unused;
	return dssh_ed25519_generate_key();
}
static const struct publication_ops operations[] = {
	{dssh_register_ssh_ed25519, dssh_ed25519_load_key_file, dssh_ed25519_save_key_file,
	    ed_generate, dssh_ed25519_get_pub_str, "ssh-ed25519", "DSSH_TEST_ED25519_KEY"},
	{dssh_register_rsa_sha2_256, dssh_rsa_sha2_256_load_key_file, dssh_rsa_sha2_256_save_key_file,
	    dssh_rsa_sha2_256_generate_key, dssh_rsa_sha2_256_get_pub_str, "rsa-sha2-256", "DSSH_TEST_RSA_KEY"},
	{dssh_register_rsa_sha2_512, dssh_rsa_sha2_512_load_key_file, dssh_rsa_sha2_512_save_key_file,
	    dssh_rsa_sha2_512_generate_key, dssh_rsa_sha2_512_get_pub_str, "rsa-sha2-512", "DSSH_TEST_RSA_KEY"},
};
struct snapshot {
	uint8_t public[512];
	size_t public_len;
	char text[1024];
	size_t text_len;
	const uint8_t *cache;
};
struct password_state {
	unsigned int calls, clears;
	bool valid, full, observe, watching, zero, arm_after;
	uintptr_t address;
};
static int
password(char *buf, int capacity, int writing, void *opaque)
{
	const char phrase[] = {'p', 'u', 'b', 0, 'l', 'i', 's', 'h'};
	struct password_state *state = opaque;
	int requested = state->full ? 256 : 8;
	if (buf == NULL || capacity < requested || (writing != 0 && writing != 1) || state->calls == UINT_MAX) {
		state->valid = false;
		return -1;
	}
	state->calls++;
	if (state->observe) {
		if (state->watching || capacity != 256) {
			state->valid = false;
			return -1;
		}
		uintptr_t address = (uintptr_t)buf;
		state->address = address;
		state->watching = true;
	}
	if (state->full) {
		memset(buf, 'K', 256);
		buf[128] = 0;
	}
	else
		memcpy(buf, phrase, sizeof(phrase));
#ifdef DSSH_CRYPTO_BOTAN
	if (state->arm_after) {
		state->arm_after = false;
		dssh_test_cpp_operation_arm(0);
	}
#endif
	return requested;
}
static bool
snapshot(const struct publication_ops *ops, dssh_key_algo key, struct snapshot *out)
{
	const uint8_t *blob = NULL;
	size_t length = 0;
	if (key == NULL || !key->haskey(key->ctx) || key->pubkey(&blob, &length, key->ctx) != 0
	    || blob == NULL || length == 0 || length > sizeof(out->public))
		return false;
	memcpy(out->public, blob, length);
	out->public_len = length;
	out->cache = blob;
	int64_t count = ops->text(out->text, sizeof(out->text));
	if (count < 1 || count > 1024)
		return false;
	size_t text_len = (size_t)count;
	if (out->text[text_len - 1] != 0)
		return false;
	out->text_len = text_len;
	return true;
}
static bool
same_key(const struct snapshot *a, const struct snapshot *b)
{
	return a->public_len == b->public_len && memcmp(a->public, b->public, a->public_len) == 0
	    && a->text_len == b->text_len && memcmp(a->text, b->text, a->text_len) == 0;
}
static bool
verify(dssh_key_algo key, const struct snapshot *identity, const struct dssh_bytes_result *signature)
{
	const uint8_t message[] = {'p', 'u', 'b'};
	return signature->bytes.data != NULL && signature->bytes.len != 0 && signature->lease.release != NULL
	    && key->verify(identity->public, identity->public_len, signature->bytes.data,
	        signature->bytes.len, message, sizeof(message)) == 0;
}
static int
sign(dssh_key_algo key, struct dssh_bytes_result *signature)
{
	const uint8_t message[] = {'p', 'u', 'b'};
	return key->sign(signature, message, sizeof(message), key->ctx);
}
static bool
temporary(char *path, bool *created)
{
	int fd = mkstemp(path);
	if (fd < 0)
		return false;
	*created = true;
	return close(fd) == 0;
}

/* mode0 generates; mode1 imports plain; mode2 imports encrypted PKCS8.
 * A new old-key owner is installed before every attempt, outside injection. */
static bool
family(const struct publication_ops *ops, unsigned int mode, unsigned int *failures,
    unsigned int *recoveries)
{
	char candidate_path[] = "/tmp/dssh-pub-candidate-XXXXXX";
	char old_path[] = "/tmp/dssh-pub-old-XXXXXX";
	bool a = false, b = false, valid = false, finished = false;
	struct snapshot candidate = {0}, original = {0};
	struct password_state state = {.valid = true};
	const char *cache = getenv(ops->cache);
	dssh_test_alloc_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	if (cache == NULL || ops->reg() != 0 || ops->load(cache, NULL, NULL) != 0
	    || !temporary(candidate_path, &a) || !temporary(old_path, &b))
		goto done;
	dssh_key_algo key = find_key_algo(ops->name);
	if (!snapshot(ops, key, &candidate)
	    || ops->save(candidate_path, mode == 2 ? password : NULL, mode == 2 ? &state : NULL) != 0
	    || !state.valid || ops->generate(2048) != 0 || !snapshot(ops, key, &original)
	    || same_key(&candidate, &original) || ops->save(old_path, NULL, NULL) != 0)
		goto done;
	valid = true;
	for (int point = 0; valid && point < 64; point++) {
		struct dssh_bytes_result held = {0}, next = {0};
		struct snapshot before = {0}, after = {0};
		uint8_t held_copy[1024];
		if (ops->load(old_path, NULL, NULL) != 0 || !snapshot(ops, key, &before)
		    || !same_key(&before, &original) || sign(key, &held) != 0 || !verify(key, &before, &held)
		    || held.bytes.len > sizeof(held_copy)) {
			dssh_result_release(&held.lease);
			valid = false;
			break;
		}
		memcpy(held_copy, held.bytes.data, held.bytes.len);
		dssh_key_algo_ctx *prior = key->ctx;
		state = (struct password_state){.valid = true};
		dssh_test_key_stdio_reset();
		dssh_test_alloc_reset();
#ifdef DSSH_CRYPTO_BOTAN
		if (mode != 0)
			dssh_test_key_watch_read_free();
#endif
		dssh_test_alloc_fail_after(point);
		int ret = mode == 0 ? ops->generate(2048)
		    : ops->load(candidate_path, mode == 2 ? password : NULL, mode == 2 ? &state : NULL);
		int calls = dssh_test_alloc_count();
		unsigned int closes = dssh_test_key_close_calls();
		bool input_retired = true;
#ifdef DSSH_CRYPTO_BOTAN
		/* The first owned input allocation must reject before any read or
		 * password callback. A later metadata failure is not this path. */
		if (mode != 0 && point == 0)
			input_retired = dssh_test_key_read_calls() == 0 && state.calls == 0;
		if (mode != 0 && point != 0) {
			bool zero = false;
			input_retired = dssh_test_alloc_watched_free(&zero) && zero;
		}
#endif
		dssh_test_alloc_reset();
		valid = input_retired && state.valid && snapshot(ops, key, &after)
		    && closes == (mode == 0 ? 0U : 1U)
		    && verify(key, &before, &held) && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0;
		if (ret < 0) {
			valid = ret == DSSH_ERROR_ALLOC && calls == point + 1 && key->ctx == prior
			    && before.cache == after.cache && same_key(&before, &after) && valid;
			if (valid)
				(*failures)++;
		}
		else {
			valid = ret == 0 && calls == point && !same_key(&before, &after)
			    && (mode == 0 || same_key(&candidate, &after)) && valid;
			valid = valid && (mode == 2 ? state.calls == 1 : state.calls == 0);
			finished = valid;
			if (valid)
				(*recoveries)++;
		}
		if (valid)
			valid = sign(key, &next) == 0 && verify(key, &after, &next)
			    && next.bytes.data != held.bytes.data && verify(key, &before, &held);
		dssh_result_release(&next.lease);
		dssh_result_release(&held.lease);
		printf("key publication: algorithm=%s mode=%u point=%d calls=%d result=%d valid=%u\n",
		    ops->name, mode, point, calls, ret, valid ? 1U : 0U);
		if (finished)
			break;
	}
	valid = valid && finished;
done:
	dssh_test_alloc_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	if (a && unlink(candidate_path) != 0)
		valid = false;
	if (b && unlink(old_path) != 0)
		valid = false;
	return valid;
}
static int
run(unsigned int mode)
{
	unsigned int failures = 0, recoveries = 0;
	bool valid = true;
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
		bool one = family(&operations[i], mode, &failures, &recoveries);
		valid = one && valid;
	}
	printf("key publication summary: families=3 mode=%u rejected=%u recovered=%u valid=%u\n",
	    mode, failures, recoveries, valid ? 1U : 0U);
	ASSERT_TRUE(valid && failures >= 9 && recoveries == 3);
	return TEST_PASS;
}
static int
test_generation(void)
{
	return run(0);
}
static int
test_plain(void)
{
	return run(1);
}
static int
test_encrypted(void)
{
	return run(2);
}
/* A setup eligibility check must not publish a context on a failed operation.
 * Detach through the actual public setter, without fabricated native objects. */
static bool
preflight_family(const struct publication_ops *ops)
{
	char missing[] = "/tmp/dssh-preflight-XXXXXX";
	bool created = false, valid = false;
	const char *cache = getenv(ops->cache);
	struct snapshot before = {0}, after = {0};
	struct dssh_bytes_result held = {0}, next = {0};
	uint8_t held_copy[1024];
	dssh_test_alloc_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	if (cache == NULL || ops->reg() != 0 || ops->load(cache, NULL, NULL) != 0
	    || !temporary(missing, &created) || unlink(missing) != 0)
		goto done;
	created = false;
	dssh_key_algo key = find_key_algo(ops->name);
	if (!snapshot(ops, key, &before) || sign(key, &held) != 0
	    || !verify(key, &before, &held) || held.bytes.len > sizeof(held_copy))
		goto done;
	memcpy(held_copy, held.bytes.data, held.bytes.len);
	dssh_key_algo_ctx *prior = key->ctx;
	valid = true;
	for (unsigned int action = 0; action < 3; action++) {
		if (dssh_key_algo_set_ctx(ops->name, NULL) != 0 || key->ctx != NULL
		    || key->haskey(key->ctx) != 0) {
			valid = false;
			break;
		}
		if (action != 0)
			dssh_test_alloc_fail_after(0);
		int ret = action == 0 ? ops->load(missing, NULL, NULL)
		    : action == 1 ? ops->generate(2048) : ops->load(cache, NULL, NULL);
		int calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		bool detached = key->ctx == NULL;
		int restored = dssh_key_algo_set_ctx(ops->name, prior);
		bool one = detached && ret == (action == 0 ? DSSH_ERROR_INIT : DSSH_ERROR_ALLOC)
		    && calls == (action == 0 ? 0 : 1) && restored == 0 && key->ctx == prior
		    && snapshot(ops, key, &after) && same_key(&before, &after)
		    && before.cache == after.cache && verify(key, &before, &held)
		    && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0;
		printf("key preflight: algorithm=%s case=%u status=%d calls=%d detached=%u valid=%u\n",
		    ops->name, action, ret, calls, detached ? 1U : 0U, one ? 1U : 0U);
		valid = one && valid;
	}
	/* A real successful operation still publishes after explicit detachment. */
	bool replaced = dssh_key_algo_set_ctx(ops->name, NULL) == 0
	    && ops->load(cache, NULL, NULL) == 0 && key->ctx != NULL && key->ctx != prior
	    && snapshot(ops, key, &after) && same_key(&before, &after) && before.cache != after.cache
	    && verify(key, &before, &held) && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0
	    && sign(key, &next) == 0 && verify(key, &after, &next)
	    && next.bytes.data != held.bytes.data;
	printf("key preflight recovery: algorithm=%s valid=%u\n", ops->name, replaced ? 1U : 0U);
	valid = replaced && valid;
done:
	dssh_result_release(&next.lease);
	dssh_result_release(&held.lease);
	dssh_test_alloc_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	if (created && unlink(missing) != 0)
		valid = false;
	return valid;
}
static int
test_preflight(void)
{
	unsigned int completed = 0;
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
		if (preflight_family(&operations[i]))
			completed++;
	}
	printf("key preflight summary: families=3 rejected=9 recovered=3 completed=%u\n", completed);
	ASSERT_EQ(completed, 3);
	return TEST_PASS;
}
#ifdef DSSH_CRYPTO_BOTAN
static void
cpp_password_cleared(const void *ptr, size_t length, void *opaque)
{
	struct password_state *state = opaque;
	uintptr_t address = (uintptr_t)ptr;
	if (!state->watching || state->address != address)
		return;
	if (length != 256)
		state->zero = false;
	else {
		const uint8_t *bytes = ptr;
		for (size_t i = 0; i < length; i++) {
			if (bytes[i] != 0)
				state->zero = false;
		}
	}
	state->watching = false;
	state->address = 0;
	if (state->clears == UINT_MAX)
		state->valid = false;
	else
		state->clears++;
}
struct cpp_operation {
	const struct publication_ops *ops;
	unsigned int mode;
	const char *path;
	struct password_state *password;
};
static int
cpp_operate(void *opaque)
{
	struct cpp_operation *operation = opaque;
	if (operation->mode == 0)
		return operation->ops->generate(2048);
	return operation->ops->load(operation->path, operation->mode >= 2 ? password : NULL,
	    operation->mode >= 2 ? operation->password : NULL);
}
static bool
cpp_profile(const struct publication_ops *ops, unsigned int mode)
{
	char path[] = "/tmp/dssh-cpp-profile-XXXXXX";
	bool created = false, valid = false;
	struct snapshot candidate = {0}, before = {0}, after = {0};
	struct dssh_bytes_result held = {0}, next = {0};
	uint8_t held_copy[1024];
	struct password_state state = {.valid = true, .full = mode == 3, .zero = true};
	struct dssh_test_cpp_operation_result result = {0};
	const char *cache = getenv(ops->cache);
	dssh_test_alloc_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	if (cache == NULL || ops->reg() != 0 || ops->load(cache, NULL, NULL) != 0
	    || !temporary(path, &created))
		goto done;
	dssh_key_algo key = find_key_algo(ops->name);
	if (!snapshot(ops, key, &candidate)
	    || ops->save(path, mode >= 2 ? password : NULL, mode >= 2 ? &state : NULL) != 0
	    || !state.valid || ops->generate(2048) != 0 || !snapshot(ops, key, &before)
	    || same_key(&candidate, &before) || sign(key, &held) != 0 || !verify(key, &before, &held)
	    || held.bytes.len > sizeof(held_copy))
		goto done;
	memcpy(held_copy, held.bytes.data, held.bytes.len);
	state = (struct password_state){.valid = true, .full = mode == 3, .zero = true, .observe = true};
	struct cpp_operation operation = {ops, mode, path, &state};
	dssh_test_key_observe_clear(cpp_password_cleared, &state);
	dssh_test_cpp_operation(cpp_operate, &operation, SIZE_MAX, &result);
	dssh_test_key_observe_clear(NULL, NULL);
	valid = state.zero && !state.watching && state.calls == state.clears && !result.escaped && !result.injected && result.status == 0 && result.calls > 0
	    && state.valid && snapshot(ops, key, &after) && !same_key(&before, &after)
	    && (mode == 0 || same_key(&candidate, &after))
	    && (mode >= 2 ? state.calls == 1 : state.calls == 0)
	    && verify(key, &before, &held) && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0;
	if (valid)
		valid = sign(key, &next) == 0 && verify(key, &after, &next)
		    && next.bytes.data != held.bytes.data && verify(key, &before, &held);
done:
	dssh_result_release(&next.lease);
	dssh_result_release(&held.lease);
	dssh_test_alloc_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	if (created && unlink(path) != 0)
		valid = false;
	printf("key CPP profile: algorithm=%s mode=%u calls=%zu status=%d injected=%u escaped=%u valid=%u\n",
	    ops->name, mode, result.calls, result.status, result.injected ? 1U : 0U,
	    result.escaped ? 1U : 0U, valid ? 1U : 0U);
	return valid;
}
/* Only the public-cache C allocation has this measured odd byte extent;
 * native owner/signature metadata use pointer-aligned struct sizes. No native
 * work, allocation or blocking operation runs inside this observer. */
struct cpp_cache_marker {
	size_t cache_size, fault;
	unsigned int seen;
	bool late, valid;
};
static void
cpp_cache(size_t count, size_t size, void *opaque)
{
	struct cpp_cache_marker *marker = opaque;
	if (count != 1 || size != marker->cache_size)
		return;
	if (marker->seen == UINT_MAX) {
		marker->valid = false;
		return;
	}
	marker->seen++;
	if (marker->seen != 1) {
		marker->valid = false;
		return;
	}
	if (marker->late)
		dssh_test_cpp_operation_arm(marker->fault);
}

/* scope0: whole ordinary-new prefix; scope1: after public cache allocation;
 * scope2: actual scalar nothrow allocation of the C++ key context. */
static bool
cpp_fault_family(const struct publication_ops *ops, unsigned int mode, unsigned int scope)
{
	char candidate_path[] = "/tmp/dssh-cpp-candidate-XXXXXX";
	char old_path[] = "/tmp/dssh-cpp-old-XXXXXX";
	bool a = false, b = false, valid = false, finished = false;
	struct snapshot candidate = {0}, original = {0};
	struct password_state state = {.valid = true, .full = mode == 3, .zero = true};
	const char *cache = getenv(ops->cache);
	size_t rejected = 0, recovered = 0, allocation_errors = 0, invoked = 0, complete_calls = 0;
	size_t password_size = scope == 3 ? dssh_test_cpp_password_allocation_size() : 0;
	dssh_test_alloc_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	if (cache == NULL || ops->reg() != 0 || ops->load(cache, NULL, NULL) != 0
	    || !temporary(candidate_path, &a) || !temporary(old_path, &b))
		goto done;
	dssh_key_algo key = find_key_algo(ops->name);
	if (!snapshot(ops, key, &candidate)
	    || ops->save(candidate_path, mode >= 2 ? password : NULL, mode >= 2 ? &state : NULL) != 0
	    || !state.valid || ops->generate(2048) != 0 || !snapshot(ops, key, &original)
	    || same_key(&candidate, &original) || ops->save(old_path, NULL, NULL) != 0)
		goto done;
	valid = true;
	for (size_t point = 0; valid && point < 2048; point++) {
		struct dssh_bytes_result held = {0}, next = {0};
		struct snapshot before = {0}, after = {0};
		struct dssh_test_cpp_operation_result result = {0};
		uint8_t held_copy[1024];
		if (ops->load(old_path, NULL, NULL) != 0 || !snapshot(ops, key, &before)
		    || !same_key(&before, &original) || sign(key, &held) != 0 || !verify(key, &before, &held)
		    || held.bytes.len > sizeof(held_copy)) {
			dssh_result_release(&held.lease);
			valid = false;
			break;
		}
		memcpy(held_copy, held.bytes.data, held.bytes.len);
		dssh_key_algo_ctx *prior = key->ctx;
		state = (struct password_state){.valid = true, .full = mode == 3, .zero = true, .observe = true};
		struct cpp_operation operation = {ops, mode, candidate_path, &state};
		struct cpp_cache_marker marker = {before.public_len, point, 0, scope == 1, true};
		dssh_test_alloc_reset();
		dssh_test_key_stdio_reset();
		if (mode != 0)
			dssh_test_key_watch_read_free();
		dssh_test_alloc_observe_allocations(cpp_cache, &marker);
		dssh_test_key_observe_clear(cpp_password_cleared, &state);
		state.arm_after = scope == 3;
		if (scope == 2)
			dssh_test_cpp_context_operation(cpp_operate, &operation, &result);
		else
			dssh_test_cpp_operation(cpp_operate, &operation, scope == 1 || scope == 3 ? SIZE_MAX : point, &result);
		dssh_test_alloc_observe_allocations(NULL, NULL);
		dssh_test_key_observe_clear(NULL, NULL);
		unsigned int closes = dssh_test_key_close_calls();
		bool input_retired = true;
		if (mode != 0) {
			bool zero = false;
			input_retired = dssh_test_alloc_watched_free(&zero) && zero;
		}
		dssh_test_alloc_reset();
		valid = state.zero && !state.watching && state.calls == state.clears
		    && !result.escaped && state.valid && marker.valid && input_retired
		    && closes == (mode == 0 ? 0U : 1U) && snapshot(ops, key, &after)
		    && verify(key, &before, &held) && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0;
		if (scope == 1)
			valid = valid && marker.seen == 1;
		if (result.status < 0) {
			valid = valid && result.injected && result.calls > point && key->ctx == prior
			    && before.cache == after.cache && same_key(&before, &after);
			rejected++;
			if (result.status == DSSH_ERROR_ALLOC)
				allocation_errors++;
		}
		else {
			valid = valid && result.status == 0 && !same_key(&before, &after)
			    && (mode == 0 || same_key(&candidate, &after))
			    && (mode >= 2 ? state.calls == 1 : state.calls == 0);
			if (result.injected)
				recovered++;
			else {
				valid = valid && result.calls > 0 && result.calls <= point;
				finished = valid;
				complete_calls = result.calls;
			}
		}
		if (scope == 2) {
			valid = valid && point == 0 && result.status == DSSH_ERROR_ALLOC && result.injected
			    && result.calls == 1 && marker.seen == 0;
			finished = valid;
		}
		if (scope == 3) {
			valid = valid && mode == 3 && point == 0 && password_size > 0 && result.status == DSSH_ERROR_INIT
			    && result.injected && result.failed_size == password_size && marker.seen == 0
			    && state.calls == 1 && state.clears == 1;
			finished = valid;
		}
		if (valid)
			valid = sign(key, &next) == 0 && verify(key, &after, &next)
			    && next.bytes.data != held.bytes.data && verify(key, &before, &held);
		dssh_result_release(&next.lease);
		dssh_result_release(&held.lease);
		invoked++;
		printf("key CPP fault: algorithm=%s mode=%u scope=%u point=%zu calls=%zu status=%d injected=%u escaped=%u "
		    "bytes=%zu cache=%u pw-calls=%u pw-clears=%u valid=%u\n",
		    ops->name, mode, scope, point, result.calls, result.status, result.injected ? 1U : 0U,
		    result.escaped ? 1U : 0U, result.failed_size, marker.seen, state.calls, state.clears, valid ? 1U : 0U);
		if (finished)
			break;
	}
	valid = valid && finished && rejected > 0 && (scope != 1 || allocation_errors > 0);
	if (valid && scope >= 2) {
		struct dssh_bytes_result held = {0}, next = {0};
		struct snapshot after = {0};
		struct dssh_test_cpp_operation_result result = {0};
		uint8_t held_copy[1024];
		valid = sign(key, &held) == 0 && verify(key, &original, &held)
		    && held.bytes.len <= sizeof(held_copy);
		if (valid) {
			memcpy(held_copy, held.bytes.data, held.bytes.len);
			state = (struct password_state){.valid = true, .full = mode == 3, .zero = true, .observe = true};
			struct cpp_operation operation = {ops, mode, candidate_path, &state};
			dssh_test_key_observe_clear(cpp_password_cleared, &state);
			dssh_test_cpp_operation(cpp_operate, &operation, SIZE_MAX, &result);
			dssh_test_key_observe_clear(NULL, NULL);
			valid = state.zero && !state.watching && state.calls == state.clears
			    && result.status == 0 && !result.injected && !result.escaped && result.calls > 0
			    && state.valid && snapshot(ops, key, &after) && !same_key(&original, &after)
			    && (mode == 0 || same_key(&candidate, &after))
			    && verify(key, &original, &held) && memcmp(held_copy, held.bytes.data, held.bytes.len) == 0
			    && sign(key, &next) == 0 && verify(key, &after, &next) && next.bytes.data != held.bytes.data;
			complete_calls = result.calls;
		}
		dssh_result_release(&next.lease);
		dssh_result_release(&held.lease);
		printf("key CPP recovery: algorithm=%s mode=%u scope=%u calls=%zu valid=%u\n",
		    ops->name, mode, scope, complete_calls, valid ? 1U : 0U);
	}
done:
	dssh_test_alloc_reset();
	dssh_test_key_stdio_reset();
	dssh_test_reset_global_config();
	if (a && unlink(candidate_path) != 0)
		valid = false;
	if (b && unlink(old_path) != 0)
		valid = false;
	printf("key CPP fault summary: algorithm=%s mode=%u scope=%u invoked=%zu rejected=%zu recovered=%zu alloc-errors=%zu complete-calls=%zu valid=%u\n",
	    ops->name, mode, scope, invoked, rejected, recovered, allocation_errors, complete_calls, valid ? 1U : 0U);
	return valid;
}
static int
cpp_faults(unsigned int mode, unsigned int scope)
{
	unsigned int completed = 0;
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
		if (cpp_fault_family(&operations[i], mode, scope))
			completed++;
	}
	printf("key CPP fault group: mode=%u scope=%u families=3 completed=%u\n", mode, scope, completed);
	ASSERT_EQ(completed, 3);
	return TEST_PASS;
}
#define CPP_FAULT_TEST(name, mode, scope) \
	static int name(void) \
	{ \
		return cpp_faults(mode, scope); \
	}
CPP_FAULT_TEST(cpp_prefix_generation, 0, 0)
CPP_FAULT_TEST(cpp_prefix_plain, 1, 0)
CPP_FAULT_TEST(cpp_prefix_encrypted, 2, 0)
CPP_FAULT_TEST(cpp_late_generation, 0, 1)
CPP_FAULT_TEST(cpp_late_plain, 1, 1)
CPP_FAULT_TEST(cpp_late_encrypted, 2, 1)
CPP_FAULT_TEST(cpp_context_generation, 0, 2)
CPP_FAULT_TEST(cpp_context_plain, 1, 2)
CPP_FAULT_TEST(cpp_context_encrypted, 2, 2)
CPP_FAULT_TEST(cpp_password_encrypted, 3, 3)
#undef CPP_FAULT_TEST
static int
cpp_counts(unsigned int mode)
{
	unsigned int completed = 0;
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
		if (cpp_profile(&operations[i], mode))
			completed++;
	}
	printf("key CPP profile summary: mode=%u families=3 completed=%u\n", mode, completed);
	ASSERT_EQ(completed, 3);
	return TEST_PASS;
}
static int
cpp_counts_generation(void)
{
	return cpp_counts(0);
}
static int
cpp_counts_plain(void)
{
	return cpp_counts(1);
}
static int
cpp_counts_encrypted(void)
{
	return cpp_counts(2);
}
static int
cpp_counts_full_password(void)
{
	return cpp_counts(3);
}
#endif
static int
client_context_faults(bool replacement)
{
	dssh_test_reset_global_config();
	ASSERT_OK(dssh_register_ssh_ed25519());
	unsigned modes = 1;
#ifdef DSSH_CRYPTO_OPENSSL
	/* The native countdown wraps OpenSSL calls. Botan does not use them. */
	modes = 2;
#endif
	for (unsigned mode = 0; mode < modes; mode++) {
		bool completed = false;
		unsigned rejected = 0;
		for (int allocation = 0; allocation < 64; allocation++) {
			dssh_session sess = dssh_session_init(true, 0);
			ASSERT_NOT_NULL(sess);
			char before[512], after[512];
			if (replacement) {
				ASSERT_OK(dssh_session_generate_client_key(sess, "ssh-ed25519", 256));
				ASSERT_TRUE(dssh_session_get_client_key_pub_str(sess, "ssh-ed25519", before, sizeof(before)) > 0);
			}
			if (mode == 0)
				dssh_test_alloc_fail_after(allocation);
			else
				dssh_test_ossl_fail_after(allocation);
			int ret = dssh_session_generate_client_key(sess, "ssh-ed25519", 256);
			dssh_test_alloc_reset();
			dssh_test_ossl_reset();
			if (ret < 0) {
				rejected++;
				if (replacement) {
					ASSERT_TRUE(dssh_session_get_client_key_pub_str(sess, "ssh-ed25519", after, sizeof(after)) > 0);
					ASSERT_TRUE(strcmp(before, after) == 0);
				}
				else
					ASSERT_EQ(dssh_session_get_client_key_pub_str(sess, "ssh-ed25519", after, sizeof(after)), DSSH_ERROR_INIT);
			}
			else {
				ASSERT_EQ(ret, 0);
				ASSERT_TRUE(dssh_session_get_client_key_pub_str(sess, "ssh-ed25519", after, sizeof(after)) > 0);
				completed = true;
			}
			dssh_session_cleanup(sess);
			if (completed)
				break;
		}
		ASSERT_TRUE(completed);
		ASSERT_TRUE(rejected > 0);
		printf("client context publication: replacement=%d mode=%u rejected=%u completed=%d\n",
		    replacement, mode, rejected, completed);
	}
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
client_initial(void)
{
	return client_context_faults(false);
}

static int
client_replacement(void)
{
	return client_context_faults(true);
}

static struct dssh_test_entry tests[] = {
	{"client_initial", client_initial}, {"client_replacement", client_replacement},
	{"generation", test_generation}, {"plain", test_plain}, {"encrypted", test_encrypted},
	{"preflight", test_preflight},
#ifdef DSSH_CRYPTO_BOTAN
		{"cpp_prefix_generation", cpp_prefix_generation},
	{"cpp_prefix_plain", cpp_prefix_plain},
	{"cpp_prefix_encrypted", cpp_prefix_encrypted},
	{"cpp_late_generation", cpp_late_generation},
	{"cpp_late_plain", cpp_late_plain},
	{"cpp_late_encrypted", cpp_late_encrypted},
	{"cpp_context_generation", cpp_context_generation},
	{"cpp_context_plain", cpp_context_plain},
	{"cpp_context_encrypted", cpp_context_encrypted},
	{"cpp_password_encrypted", cpp_password_encrypted},
	{"cpp_counts_full_password", cpp_counts_full_password},
	{"cpp_counts_generation", cpp_counts_generation},
	{"cpp_counts_plain", cpp_counts_plain},
	{"cpp_counts_encrypted", cpp_counts_encrypted},
#endif
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
