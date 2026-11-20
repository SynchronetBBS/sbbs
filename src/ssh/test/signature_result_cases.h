#ifndef DSSH_TEST_SIGNATURE_RESULT_CASES_H
#define DSSH_TEST_SIGNATURE_RESULT_CASES_H

#include <stdatomic.h>
#include <threads.h>
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "key_context_fixture.h"

static bool
signature_bytes_zero(const uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != 0)
			return false;
	}
	return true;
}

static int
signature_native_case(unsigned algorithm)
{
	const char *names[] = {"ssh-ed25519", "rsa-sha2-256", "rsa-sha2-512"};
	dssh_test_reset_global_config();
	int ret = algorithm == 0 ? dssh_register_ssh_ed25519() :
	    algorithm == 1 ? dssh_register_rsa_sha2_256() : dssh_register_rsa_sha2_512();
	if (ret == 0) {
		const char *path = getenv(algorithm == 0 ? "DSSH_TEST_ED25519_KEY" : "DSSH_TEST_RSA_KEY");
		if (path != NULL)
			ret = algorithm == 0 ? dssh_ed25519_load_key_file(path, NULL, NULL) :
			    algorithm == 1 ? dssh_rsa_sha2_256_load_key_file(path, NULL, NULL) :
			    dssh_rsa_sha2_512_load_key_file(path, NULL, NULL);
		else
			ret = algorithm == 0 ? dssh_ed25519_generate_key() :
			    algorithm == 1 ? dssh_rsa_sha2_256_generate_key(2048) : dssh_rsa_sha2_512_generate_key(2048);
	}
	dssh_key_algo key = ret == 0 ? find_key_algo(names[algorithm]) : NULL;
	if (key == NULL) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	const uint8_t first[] = "first";
	const uint8_t second[] = "second";
	const uint8_t third[] = "third";
	struct dssh_bytes_result a = {0};
	struct dssh_bytes_result b = {0};
	struct dssh_bytes_result c = {0};
	struct dssh_bytes_result failed = {0};
	/* Cold buffer allocation fails inside the actual module; partial output
	 * is released even on error. A subsequent successful call must work. */
	dssh_test_alloc_fail_after(0);
	ret = key->sign(&failed, first, sizeof(first), key->ctx);
	dssh_test_alloc_reset();
	bool valid = ret == DSSH_ERROR_ALLOC && failed.lease.release == NULL && failed.bytes.data == NULL;
	dssh_result_release(&failed.lease);
	ret = key->sign(&a, first, sizeof(first), key->ctx);
	valid = valid && ret == 0 && a.bytes.data != NULL && a.bytes.len > 0 && a.lease.release != NULL;
	ret = key->sign(&b, second, sizeof(second), key->ctx);
	valid = valid && ret == 0 && b.bytes.data != NULL && b.bytes.len > 0 && b.lease.release != NULL;
	const uint8_t *public_blob = NULL;
	size_t public_length = 0;
	ret = key->pubkey(&public_blob, &public_length, key->ctx);
	valid = valid && ret == 0 && public_blob != NULL && public_length > 0 && public_length <= 16384;
	uint8_t *public_copy = valid ? malloc(public_length) : NULL;
	if (public_copy == NULL)
		valid = false;
	if (valid) {
		memcpy(public_copy, public_blob, public_length);
		valid = a.bytes.data != b.bytes.data
		    && key->verify(public_copy, public_length, a.bytes.data, a.bytes.len, first, sizeof(first)) == 0
		    && key->verify(public_copy, public_length, b.bytes.data, b.bytes.len, second, sizeof(second)) == 0;
	}
	const uint8_t *a_storage = a.bytes.data;
	size_t a_length = a.bytes.len;
	dssh_result_release(&a.lease);
	if (valid)
		valid = signature_bytes_zero(a_storage, a_length);
	/* No library-owned result allocation is needed after this capacity is warm.
	 * Provider-internal EVP/Botan allocations are a separate measured domain. */
	dssh_test_alloc_fail_after(0);
	ret = key->sign(&c, third, sizeof(third), key->ctx);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	valid = valid && ret == 0 && allocations == 0 && c.bytes.data == a_storage;
	if (valid)
		valid = key->verify(public_copy, public_length, b.bytes.data, b.bytes.len, second, sizeof(second)) == 0
		    && key->verify(public_copy, public_length, c.bytes.data, c.bytes.len, third, sizeof(third)) == 0;
	struct dssh_bytes_result invalid = {0};
	valid = valid && key->sign(&invalid, NULL, 1, key->ctx) == DSSH_ERROR_INVALID
	    && invalid.lease.release == NULL;
	valid = valid && key->sign(NULL, first, sizeof(first), key->ctx) == DSSH_ERROR_INVALID;
#ifdef DSSH_CRYPTO_OPENSSL
	/* Actual provider failure after storage acquisition must leave a releasable
	 * partial result. Iterate real wrapper failure points, never assume an index. */
	bool saw_partial = false;
	for (int n = 0; n < 16; n++) {
		struct dssh_bytes_result partial = {0};
		dssh_test_ossl_fail_after(n);
		ret = key->sign(&partial, first, sizeof(first), key->ctx);
		dssh_test_ossl_reset();
		if (ret < 0 && partial.lease.release != NULL)
			saw_partial = true;
		dssh_result_release(&partial.lease);
		if (ret == 0)
			break;
	}
	valid = valid && saw_partial;
#endif
	dssh_key_algo_verify verify = key->verify;
	/* Retiring the actual key context must not end either outstanding result. */
	dssh_test_reset_global_config();
	if (valid)
		valid = verify(public_copy, public_length, b.bytes.data, b.bytes.len, second, sizeof(second)) == 0
		    && verify(public_copy, public_length, c.bytes.data, c.bytes.len, third, sizeof(third)) == 0;
	dssh_result_release(&b.lease);
	dssh_result_release(&c.lease);
	dssh_result_release(&c.lease);
	free(public_copy);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_signature_result_ed25519(void)
{
	return signature_native_case(0);
}

static int
test_signature_result_rsa256(void)
{
	return signature_native_case(1);
}

static int
test_signature_result_rsa512(void)
{
	return signature_native_case(2);
}

#define SIGNATURE_WORKERS 8
struct signature_concurrent_state {
	atomic_uint ready;
	atomic_bool go;
};
struct signature_worker {
	struct signature_concurrent_state *shared;
	dssh_key_algo_sign sign;
	dssh_key_algo_verify verify;
	dssh_key_algo_ctx *context;
	const uint8_t *public_blob;
	size_t public_length;
	unsigned index;
	struct dssh_bytes_result result;
	bool release_on_main;
	bool valid;
};

static int
signature_worker_run(void *data)
{
	struct signature_worker *worker = data;
	uint8_t id = (uint8_t)worker->index; /* bounded by SIGNATURE_WORKERS */
	const uint8_t message[] = {id, 0};
	int ret = worker->sign(&worker->result, message, sizeof(message), worker->context);
	worker->valid = ret == 0 && worker->result.bytes.data != NULL
	    && worker->result.bytes.len > 0 && worker->result.lease.release != NULL;
	atomic_fetch_add_explicit(&worker->shared->ready, 1, memory_order_acq_rel);
	while (!atomic_load_explicit(&worker->shared->go, memory_order_acquire))
		thrd_yield();
	if (worker->valid)
		worker->valid = worker->verify(worker->public_blob, worker->public_length,
		    worker->result.bytes.data, worker->result.bytes.len, message, sizeof(message)) == 0;
	if (!worker->release_on_main)
		dssh_result_release(&worker->result.lease);
	return 0;
}

static bool
signature_concurrent_round(dssh_key_algo key, const uint8_t *public_blob,
    size_t public_length, bool retire)
{
	struct signature_concurrent_state shared;
	atomic_init(&shared.ready, 0);
	atomic_init(&shared.go, false);
	struct signature_worker workers[SIGNATURE_WORKERS] = {0};
	thrd_t threads[SIGNATURE_WORKERS];
	unsigned count = 0;
	bool valid = true;
	for (unsigned i = 0; i < SIGNATURE_WORKERS; i++) {
		workers[i].shared = &shared;
		workers[i].sign = key->sign;
		workers[i].verify = key->verify;
		workers[i].context = key->ctx;
		workers[i].public_blob = public_blob;
		workers[i].public_length = public_length;
		workers[i].index = i;
		workers[i].release_on_main = retire;
		if (thrd_create(&threads[i], signature_worker_run, &workers[i]) != thrd_success) {
			valid = false;
			break;
		}
		count++;
	}
	while (atomic_load_explicit(&shared.ready, memory_order_acquire) < count)
		thrd_yield();
	for (unsigned i = 0; i < count; i++) {
		valid = valid && workers[i].valid;
		for (unsigned j = 0; j < i; j++)
			valid = valid && workers[i].result.bytes.data != workers[j].result.bytes.data;
	}
	/* No producer is still running. Only independent result leases remain. */
	if (retire)
		dssh_test_reset_global_config();
	atomic_store_explicit(&shared.go, true, memory_order_release);
	for (unsigned i = 0; i < count; i++) {
		if (thrd_join(threads[i], NULL) != thrd_success)
			abort();
		valid = valid && workers[i].valid;
	}
	/* The retirement round also proves release from another thread after
	 * the producer threads and their temporary crypto objects are gone. */
	for (unsigned i = 0; i < count; i++)
		dssh_result_release(&workers[i].result.lease);
	return valid;
}

static int
test_signature_result_concurrent(void)
{
	const char *names[] = {"ssh-ed25519", "rsa-sha2-256", "rsa-sha2-512"};
	bool valid = true;
	for (unsigned algorithm = 0; algorithm < 3; algorithm++) {
		dssh_test_reset_global_config();
		int ret = algorithm == 0 ? dssh_register_ssh_ed25519() :
		    algorithm == 1 ? dssh_register_rsa_sha2_256() : dssh_register_rsa_sha2_512();
		const char *path = getenv(algorithm == 0 ? "DSSH_TEST_ED25519_KEY" : "DSSH_TEST_RSA_KEY");
		if (ret == 0 && path != NULL)
			ret = algorithm == 0 ? dssh_ed25519_load_key_file(path, NULL, NULL) :
			    algorithm == 1 ? dssh_rsa_sha2_256_load_key_file(path, NULL, NULL) :
			    dssh_rsa_sha2_512_load_key_file(path, NULL, NULL);
		else if (ret == 0)
			ret = algorithm == 0 ? dssh_ed25519_generate_key() :
			    algorithm == 1 ? dssh_rsa_sha2_256_generate_key(2048) : dssh_rsa_sha2_512_generate_key(2048);
		dssh_key_algo key = ret == 0 ? find_key_algo(names[algorithm]) : NULL;
		const uint8_t *public_blob = NULL;
		size_t public_length = 0;
		ret = key != NULL ? key->pubkey(&public_blob, &public_length, key->ctx) : DSSH_ERROR_INIT;
		bool ready = ret == 0 && public_blob != NULL && public_length > 0 && public_length <= 16384;
		uint8_t *public_copy = ready ? malloc(public_length) : NULL;
		if (public_copy == NULL) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		memcpy(public_copy, public_blob, public_length);
		bool first = signature_concurrent_round(key, public_copy, public_length, false);
#ifdef DSSH_CRYPTO_BOTAN
		/* The first round retains eight result buffers, but scheduling may
		 * use fewer simultaneous native objects. Warm those independently. */
		first = test_warm_native_key_context(key->ctx, SIGNATURE_WORKERS) == 0 && first;
#endif
		dssh_test_alloc_fail_after(0);
		bool reused = signature_concurrent_round(key, public_copy, public_length, false);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		bool retired = signature_concurrent_round(key, public_copy, public_length, true);
		valid = valid && first && reused && retired && allocations == 0;
		printf("signature concurrency: family=%u first=%u reused=%u retired=%u allocations=%d\n",
		    algorithm, first ? 1U : 0U, reused ? 1U : 0U, retired ? 1U : 0U, allocations);
		free(public_copy);
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#endif
