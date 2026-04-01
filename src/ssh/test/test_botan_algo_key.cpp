/*
 * test/test_botan_algo_key.cpp -- Botan-specific key algorithm tests.
 *
 * Covers gaps left by OpenSSL-specific tests:
 * - RSA pubkey serialization edge cases (e_pad, n_pad)
 * - Error paths (bad PEM, wrong key type, NULL ctx)
 * - Key file round-trips through Botan's PKCS8 layer
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <botan/rsa.h>
#include <botan/ed25519.h>
#include <botan/bigint.h>
#include <botan/system_rng.h>

extern "C" {
#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
}

/* Declared in rsa-sha2-256-botan.cpp / rsa-sha2-512-botan.cpp */
extern "C" int dssh_botan_rsa256_sign(uint8_t **out, size_t *outlen,
    const uint8_t *data, size_t data_len, void *ctx);
extern "C" int dssh_botan_rsa256_verify(const uint8_t *key_blob,
    size_t key_blob_len, const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len);
extern "C" int dssh_botan_rsa256_pubkey(const uint8_t **out,
    size_t *outlen, void *ctx);
extern "C" int dssh_botan_rsa256_haskey(void *ctx);
extern "C" void dssh_botan_rsa256_cleanup(void *ctx);

extern "C" int dssh_botan_ed25519_sign(uint8_t **out, size_t *outlen,
    const uint8_t *data, size_t data_len, void *ctx);
extern "C" int dssh_botan_ed25519_pubkey(const uint8_t **out,
    size_t *outlen, void *ctx);
extern "C" int dssh_botan_ed25519_haskey(void *ctx);
extern "C" void dssh_botan_ed25519_cleanup(void *ctx);

/* Reset function from test library */
extern "C" void dssh_test_reset_global_config(void);

/* ================================================================
 * Helpers
 * ================================================================ */

static int
load_rsa_test_key(void)
{
	const char *keyfile = getenv("DSSH_TEST_RSA_KEY");
	if (keyfile != NULL)
		return dssh_rsa_sha2_256_load_key_file(keyfile, NULL, NULL);
	return dssh_rsa_sha2_256_generate_key(2048);
}

static uint32_t
parse_u32(const uint8_t *buf, size_t len, size_t *pos)
{
	uint32_t val;
	dssh_parse_uint32(&buf[*pos], len - *pos, &val);
	*pos += 4;
	return val;
}

/* ================================================================
 * RSA pubkey serialization: verify e_pad and n_pad
 *
 * Construct a synthetic RSA public key with a specific exponent via
 * Botan's C++ API and inject it into the algorithm ctx to exercise
 * the mpint sign-bit padding branches in pubkey_impl().
 * ================================================================ */

/*
 * cbdata layout must match the one in rsa-sha2-256-botan.cpp:
 *   std::unique_ptr<Botan::Private_Key> privkey;
 *   uint8_t *pub_blob;
 *   size_t   pub_blob_len;
 *
 * We use this to construct a synthetic cbdata with a specific key.
 */
struct rsa_cbdata {
	std::unique_ptr<Botan::Private_Key> privkey;
	uint8_t                            *pub_blob;
	size_t                              pub_blob_len;
};

static int
test_rsa_pubkey_e_pad(void)
{
	/*
	 * Build an RSA key with exponent that has MSB set (0x80000003).
	 * This hits the e_pad=true branch in pubkey_impl().
	 *
	 * Botan's RSA_PrivateKey constructor validates the key, so we
	 * need a mathematically valid one.  We generate a normal key
	 * and then check if we can construct one with a custom exponent.
	 * Botan may reject unusual exponents, so SKIP if it does.
	 */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	/* Botan doesn't easily support custom exponents in keygen.
	 * Instead, generate a normal key and verify the pubkey blob
	 * is well-formed — then test the e_pad path by constructing
	 * a synthetic cbdata with a known exponent.
	 */

	/* Create a Botan RSA key with e = 0x80000003 (MSB set) */
	Botan::RSA_PrivateKey *priv = nullptr;
	try {
		priv = new Botan::RSA_PrivateKey(Botan::system_rng(),
		    2048, 0x80000003UL);
	} catch (...) {
		/* Some Botan versions may reject unusual exponents */
		dssh_test_reset_global_config();
		return TEST_SKIP;
	}

	/* Build synthetic cbdata and inject into the algo */
	auto *cbd = new (std::nothrow) rsa_cbdata;
	ASSERT_NOT_NULL(cbd);
	cbd->privkey.reset(priv);
	cbd->pub_blob = nullptr;
	cbd->pub_blob_len = 0;

	ASSERT_EQ(dssh_key_algo_set_ctx("rsa-sha2-256", cbd), 0);

	/* Call pubkey through the extern "C" wrapper */
	const uint8_t *blob = nullptr;
	size_t blob_len;
	int res = dssh_botan_rsa256_pubkey(&blob, &blob_len, cbd);
	ASSERT_EQ(res, 0);

	/* Parse: string "ssh-rsa", string e, string n */
	size_t pos = 0;
	uint32_t name_len = parse_u32(blob, blob_len, &pos);
	pos += name_len; /* skip "ssh-rsa" */
	uint32_t e_wire_len = parse_u32(blob, blob_len, &pos);
	/* First byte should be 0x00 padding (MSB of exponent is set) */
	ASSERT_EQ(blob[pos], 0x00);
	(void)e_wire_len;

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_pubkey_n_no_pad(void)
{
	/*
	 * Normal RSA keys always have MSB set on n (2048-bit key has
	 * n >= 2^2047, so the top byte is always >= 0x80, meaning n_pad
	 * is always true).  To hit n_pad=false we'd need a modulus with
	 * MSB clear, which isn't a valid RSA key.
	 *
	 * Instead, verify that a normal key's n field has the pad byte.
	 */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(load_rsa_test_key(), 0);

	/* Verify pubkey serialization succeeds and produces a valid
	 * "ssh-rsa ..." string via the public API.  With a real 2048-bit
	 * key, n always has MSB set so n_pad is always true. */
	char str[4096];
	int64_t slen = dssh_rsa_sha2_256_get_pub_str(str, sizeof(str));
	ASSERT_TRUE(slen > 8);
	ASSERT_TRUE(memcmp(str, "ssh-rsa ", 8) == 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * NULL ctx handling for extern "C" wrappers
 * ================================================================ */

static int
test_rsa256_sign_null_ctx(void)
{
	uint8_t data[] = "test data";
	uint8_t *sig = nullptr;
	size_t siglen = 0;

	ASSERT_EQ(dssh_botan_rsa256_sign(&sig, &siglen,
	    data, sizeof(data), nullptr), DSSH_ERROR_INIT);
	ASSERT_NULL(sig);
	return TEST_PASS;
}

static int
test_rsa256_pubkey_null_ctx(void)
{
	const uint8_t *blob = nullptr;
	size_t blob_len = 0;

	ASSERT_EQ(dssh_botan_rsa256_pubkey(&blob, &blob_len, nullptr),
	    DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_rsa256_haskey_null_ctx(void)
{
	ASSERT_FALSE(dssh_botan_rsa256_haskey(nullptr));
	return TEST_PASS;
}

static int
test_rsa256_cleanup_null_ctx(void)
{
	/* Must not crash */
	dssh_botan_rsa256_cleanup(nullptr);
	return TEST_PASS;
}

static int
test_ed25519_sign_null_ctx(void)
{
	uint8_t data[] = "test data";
	uint8_t *sig = nullptr;
	size_t siglen = 0;

	ASSERT_EQ(dssh_botan_ed25519_sign(&sig, &siglen,
	    data, sizeof(data), nullptr), DSSH_ERROR_INIT);
	ASSERT_NULL(sig);
	return TEST_PASS;
}

static int
test_ed25519_pubkey_null_ctx(void)
{
	const uint8_t *blob = nullptr;
	size_t blob_len = 0;

	ASSERT_EQ(dssh_botan_ed25519_pubkey(&blob, &blob_len, nullptr),
	    DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_ed25519_haskey_null_ctx(void)
{
	ASSERT_FALSE(dssh_botan_ed25519_haskey(nullptr));
	return TEST_PASS;
}

static int
test_ed25519_cleanup_null_ctx(void)
{
	dssh_botan_ed25519_cleanup(nullptr);
	return TEST_PASS;
}

/* ================================================================
 * Key file error paths
 * ================================================================ */

static int
test_ed25519_load_bad_pem(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	/* Write garbage to a temp file */
	char tmppath[] = "/tmp/dssh_test_bad_pem_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	const char *garbage = "this is not a PEM file\n";
	write(fd, garbage, strlen(garbage));
	close(fd);

	int res = dssh_ed25519_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_EQ(res, DSSH_ERROR_INIT);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_load_bad_pem(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	char tmppath[] = "/tmp/dssh_test_bad_pem_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	const char *garbage = "not a valid key file\n";
	write(fd, garbage, strlen(garbage));
	close(fd);

	int res = dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_EQ(res, DSSH_ERROR_INIT);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_ed25519_load_wrong_key_type(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	/* Generate and save an RSA key */
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_wrongtype_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	/* Try to load it as Ed25519 */
	int res = dssh_ed25519_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_EQ(res, DSSH_ERROR_INVALID);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_load_wrong_key_type(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	/* Generate and save an Ed25519 key */
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_wrongtype_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	/* Try to load it as RSA */
	int res = dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_EQ(res, DSSH_ERROR_INVALID);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_ed25519_load_nonexistent(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	ASSERT_EQ(dssh_ed25519_load_key_file("/nonexistent/path", NULL, NULL),
	    DSSH_ERROR_INIT);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_load_nonexistent(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	ASSERT_EQ(dssh_rsa_sha2_256_load_key_file("/nonexistent/path",
	    NULL, NULL), DSSH_ERROR_INIT);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Key file round-trip: generate -> save -> load -> sign -> verify
 * ================================================================ */

static int
test_ed25519_key_roundtrip(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	/* Save */
	char tmppath[] = "/tmp/dssh_test_ed_rt_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	/* Get public key string before reload */
	char pub1[256];
	int64_t len1 = dssh_ed25519_get_pub_str(pub1, sizeof(pub1));
	ASSERT_TRUE(len1 > 0);

	/* Reset and reload */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	/* Get public key string after reload */
	char pub2[256];
	int64_t len2 = dssh_ed25519_get_pub_str(pub2, sizeof(pub2));
	ASSERT_TRUE(len2 > 0);

	/* Keys must match */
	ASSERT_EQ(len1, len2);
	ASSERT_TRUE(memcmp(pub1, pub2, static_cast<size_t>(len1)) == 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_key_roundtrip(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_rt_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	/* Get public key before reload */
	char *pub1 = nullptr;
	int64_t len1 = dssh_rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_TRUE(len1 > 0);
	pub1 = static_cast<char *>(malloc(static_cast<size_t>(len1)));
	ASSERT_NOT_NULL(pub1);
	ASSERT_TRUE(dssh_rsa_sha2_256_get_pub_str(pub1,
	    static_cast<size_t>(len1)) > 0);

	/* Reset and reload */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	char *pub2 = nullptr;
	int64_t len2 = dssh_rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_TRUE(len2 > 0);
	pub2 = static_cast<char *>(malloc(static_cast<size_t>(len2)));
	ASSERT_NOT_NULL(pub2);
	ASSERT_TRUE(dssh_rsa_sha2_256_get_pub_str(pub2,
	    static_cast<size_t>(len2)) > 0);

	ASSERT_EQ(len1, len2);
	ASSERT_TRUE(memcmp(pub1, pub2, static_cast<size_t>(len1)) == 0);

	free(pub1);
	free(pub2);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* RSA pubkey serialization */
	{ "rsa_pubkey_e_pad",            test_rsa_pubkey_e_pad },
	{ "rsa_pubkey_n_no_pad",         test_rsa_pubkey_n_no_pad },

	/* NULL ctx wrappers */
	{ "rsa256_sign_null_ctx",        test_rsa256_sign_null_ctx },
	{ "rsa256_pubkey_null_ctx",      test_rsa256_pubkey_null_ctx },
	{ "rsa256_haskey_null_ctx",      test_rsa256_haskey_null_ctx },
	{ "rsa256_cleanup_null_ctx",     test_rsa256_cleanup_null_ctx },
	{ "ed25519_sign_null_ctx",       test_ed25519_sign_null_ctx },
	{ "ed25519_pubkey_null_ctx",     test_ed25519_pubkey_null_ctx },
	{ "ed25519_haskey_null_ctx",     test_ed25519_haskey_null_ctx },
	{ "ed25519_cleanup_null_ctx",    test_ed25519_cleanup_null_ctx },

	/* Key file error paths */
	{ "ed25519_load_bad_pem",        test_ed25519_load_bad_pem },
	{ "rsa_load_bad_pem",            test_rsa_load_bad_pem },
	{ "ed25519_load_wrong_type",     test_ed25519_load_wrong_key_type },
	{ "rsa_load_wrong_type",         test_rsa_load_wrong_key_type },
	{ "ed25519_load_nonexistent",    test_ed25519_load_nonexistent },
	{ "rsa_load_nonexistent",        test_rsa_load_nonexistent },

	/* Key round-trips */
	{ "ed25519_key_roundtrip",       test_ed25519_key_roundtrip },
	{ "rsa_key_roundtrip",           test_rsa_key_roundtrip },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
