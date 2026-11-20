/*
 * test_mac.c -- Test MAC module for DeuceSSH.
 *
 * A minimal MAC that XORs the input into a fixed-size digest.
 * Provides controllable failure injection and deliberate MAC
 * corruption for testing MAC mismatch handling.
 *
 * Registered as "hmac-sha2-256" so it participates in normal
 * algorithm negotiation.  Only linked into test builds.
 */

#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#include "deucessh.h"
#include "ssh-trans.h"
#include "test_mac.h"
#include "dssh_test_counter.h"
#include "dssh_test_alloc.h"

#define TEST_MAC_DIGEST_SIZE 32
#define TEST_MAC_KEY_SIZE    32

/* Global failure injection */
static atomic_int init_fail_at = -1;
static dssh_test_counter init_count;
static atomic_bool init_failure_seen;
static atomic_int generate_fail_at = -1;
static dssh_test_counter generate_count;
static atomic_bool corrupt_output;

void
test_mac_reset(void)
{
	init_fail_at = -1;
	init_count = 0;
	init_failure_seen = false;
	generate_fail_at = -1;
	generate_count = 0;
	corrupt_output = false;
}

void
test_mac_fail_init_at(int n)
{
	init_count = 0;
	init_failure_seen = false;
	init_fail_at = n;
}

bool
test_mac_init_failure_seen(void)
{
	return atomic_load(&init_failure_seen);
}

void
test_mac_fail_generate_at(int n)
{
	generate_count = 0;
	generate_fail_at = n;
}

void
test_mac_set_corrupt(bool on)
{
	corrupt_output = on;
}

struct dssh_mac_ctx {
	uint8_t key[TEST_MAC_KEY_SIZE];
	uint16_t digest_size;
};

static int
test_mac_init_size(const uint8_t *key, dssh_mac_ctx **out, uint16_t digest_size)
{
	if (key == NULL || out == NULL)
		return DSSH_ERROR_INVALID;
	int fail_at = atomic_load(&init_fail_at);
	if (fail_at >= 0 && dssh_test_counter_next(&init_count) == fail_at) {
		init_failure_seen = true;
		return DSSH_ERROR_INIT;
	}

	struct dssh_mac_ctx *cbd = dssh_test_malloc(sizeof(*cbd));
	if (cbd == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(cbd->key, key, TEST_MAC_KEY_SIZE);
	cbd->digest_size = digest_size;
	*out = cbd;
	return 0;
}

static int
test_mac_init(const uint8_t *key, dssh_mac_ctx **out)
{
	return test_mac_init_size(key, out, TEST_MAC_DIGEST_SIZE);
}

#define TEST_MAC_OVERSIZED_DIGEST 128

static int
test_mac_oversized_init(const uint8_t *key, dssh_mac_ctx **out)
{
	return test_mac_init_size(key, out, TEST_MAC_OVERSIZED_DIGEST);
}

static int
test_mac_generate(const uint8_t *buf, size_t bufsz,
    uint8_t *outbuf, dssh_mac_ctx *ctx)
{
	if (ctx == NULL || outbuf == NULL || (buf == NULL && bufsz != 0))
		return DSSH_ERROR_INVALID;
	int fail_at = atomic_load(&generate_fail_at);
	if (fail_at >= 0 && dssh_test_counter_next(&generate_count) == fail_at)
		return DSSH_ERROR_INIT;

	struct dssh_mac_ctx *cbd = ctx;

	/* Simple MAC: XOR input with key, fold into digest_size output */
	memset(outbuf, 0, cbd->digest_size);
	for (size_t i = 0; i < bufsz; i++)
		outbuf[i % cbd->digest_size] ^= buf[i] ^ cbd->key[i % TEST_MAC_KEY_SIZE];

	if (corrupt_output)
		outbuf[0] ^= 0xFF;

	return 0;
}

static void
test_mac_cleanup(dssh_mac_ctx *ctx)
{
	if (ctx != NULL)
		dssh_secure_zero(ctx, sizeof(*ctx));
	dssh_test_free(ctx);
}

static int
register_test_mac_size(dssh_mac_init init, uint16_t digest_size)
{
	static const char name[] = "hmac-sha2-256";
	if (sizeof(struct dssh_mac_s) > SIZE_MAX - sizeof(name))
		return DSSH_ERROR_INVALID;
	struct dssh_mac_s *mac = dssh_test_malloc(sizeof(*mac) + sizeof(name));
	if (mac == NULL)
		return DSSH_ERROR_ALLOC;
	mac->next = NULL;
	mac->init = init;
	mac->generate = test_mac_generate;
	mac->cleanup = test_mac_cleanup;
	mac->digest_size = digest_size;
	mac->key_size = TEST_MAC_KEY_SIZE;
	memcpy(mac->name, name, sizeof(name));
	int ret = dssh_transport_register_mac(mac);
	if (ret < 0)
		dssh_test_free(mac);
	return ret;
}

int
register_test_mac(void)
{
	return register_test_mac_size(test_mac_init, TEST_MAC_DIGEST_SIZE);
}

int
register_test_mac_oversized(void)
{
	return register_test_mac_size(test_mac_oversized_init, TEST_MAC_OVERSIZED_DIGEST);
}
