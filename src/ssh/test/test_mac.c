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
#include <string.h>

#include "ssh-trans.h"
#include "test_mac.h"

#define TEST_MAC_DIGEST_SIZE 32
#define TEST_MAC_KEY_SIZE    32

/* Global failure injection */
static int init_fail_at = -1;
static int init_count = 0;
static int generate_fail_at = -1;
static int generate_count = 0;
static bool corrupt_output = false;

void
test_mac_reset(void)
{
	init_fail_at = -1;
	init_count = 0;
	generate_fail_at = -1;
	generate_count = 0;
	corrupt_output = false;
}

void test_mac_fail_init_at(int n)     { init_fail_at = n; init_count = 0; }
void test_mac_fail_generate_at(int n) { generate_fail_at = n; generate_count = 0; }
void test_mac_set_corrupt(bool on)    { corrupt_output = on; }

struct dssh_mac_ctx {
	uint8_t key[TEST_MAC_KEY_SIZE];
	uint16_t digest_size;
};

static uint16_t current_digest_size = TEST_MAC_DIGEST_SIZE;

static int
test_mac_init(const uint8_t *key, dssh_mac_ctx **out)
{
	if (init_fail_at >= 0 && init_count++ == init_fail_at)
		return DSSH_ERROR_INIT;

	struct dssh_mac_ctx *cbd = malloc(sizeof(*cbd));
	if (cbd == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(cbd->key, key, TEST_MAC_KEY_SIZE);
	cbd->digest_size = current_digest_size;
	*out = cbd;
	return 0;
}

static int
test_mac_generate(const uint8_t *buf, size_t bufsz,
    uint8_t *outbuf, dssh_mac_ctx *ctx)
{
	if (generate_fail_at >= 0 && generate_count++ == generate_fail_at)
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
	free(ctx);
}

int
register_test_mac(void)
{
	current_digest_size = TEST_MAC_DIGEST_SIZE;
	static const char name[] = "hmac-sha2-256";
	struct dssh_mac_s *mac = malloc(sizeof(*mac) + sizeof(name));
	if (mac == NULL)
		return DSSH_ERROR_ALLOC;
	mac->next = NULL;
	mac->init = test_mac_init;
	mac->generate = test_mac_generate;
	mac->cleanup = test_mac_cleanup;
	mac->digest_size = TEST_MAC_DIGEST_SIZE;
	mac->key_size = TEST_MAC_KEY_SIZE;
	memcpy(mac->name, name, sizeof(name));
	return dssh_transport_register_mac(mac);
}

#define TEST_MAC_OVERSIZED_DIGEST 128

int
register_test_mac_oversized(void)
{
	current_digest_size = TEST_MAC_OVERSIZED_DIGEST;
	static const char name[] = "hmac-sha2-256";
	struct dssh_mac_s *mac = malloc(sizeof(*mac) + sizeof(name));
	if (mac == NULL)
		return DSSH_ERROR_ALLOC;
	mac->next = NULL;
	mac->init = test_mac_init;
	mac->generate = test_mac_generate;
	mac->cleanup = test_mac_cleanup;
	mac->digest_size = TEST_MAC_OVERSIZED_DIGEST;
	mac->key_size = TEST_MAC_KEY_SIZE;
	memcpy(mac->name, name, sizeof(name));
	return dssh_transport_register_mac(mac);
}
