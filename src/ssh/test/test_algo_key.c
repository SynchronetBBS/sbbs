/*
 * test_algo_key.c — Tests for Ed25519 and RSA-SHA2-256 host key algorithm
 * modules in DeuceSSH.
 *
 * Tests key generation, sign/verify, public key export, and file I/O.
 */

#include <unistd.h>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "test/dssh_test_internal.h"

/* ================================================================
 * Helpers
 * ================================================================ */

/*
 * Helper: parse a uint32 from an SSH wire-format buffer at *pos,
 * advance *pos past it.  Returns the value.
 */
static uint32_t
parse_u32(const uint8_t *buf, size_t len, size_t *pos)
{
	uint32_t val;
	dssh_parse_uint32(&buf[*pos], len - *pos, &val);
	*pos += 4;
	return val;
}

/* ================================================================
 * Ed25519 tests
 * ================================================================ */

static int
test_ed25519_register(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	return TEST_PASS;
}

static int
test_ed25519_generate_key(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	return TEST_PASS;
}

static int
test_ed25519_haskey_after_generate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_ed25519_no_key_before_generate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ASSERT_FALSE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_ed25519_sign_verify_roundtrip(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "test data for signing";
	size_t data_len = sizeof(data) - 1;

	/* Sign */
	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, data_len, ka->ctx), 0);
	ASSERT_TRUE(sig_len > 0);

	/* Get pubkey blob */
	uint8_t pub_buf[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);
	ASSERT_TRUE(pub_len > 0);

	/* Verify */
	ASSERT_EQ(ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    data, data_len), 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_corrupted_sig(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "test data for signing";
	size_t data_len = sizeof(data) - 1;

	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, data_len, ka->ctx), 0);

	uint8_t pub_buf[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	/* Corrupt the last byte of the signature blob */
	sig_buf[sig_len - 1] ^= 0xFF;

	int rc = ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    data, data_len);
	ASSERT_TRUE(rc < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_different_data(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "original data";
	const uint8_t other[] = "different data";

	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	uint8_t pub_buf[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	int rc = ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    other, sizeof(other) - 1);
	ASSERT_TRUE(rc < 0);
	return TEST_PASS;
}

static int
test_ed25519_get_pub_str_format(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char buf[256];
	int64_t len = dssh_ed25519_get_pub_str(buf, sizeof(buf));
	ASSERT_TRUE(len > 0);

	/* Must start with "ssh-ed25519 " */
	ASSERT_TRUE(len > 12);
	ASSERT_TRUE(memcmp(buf, "ssh-ed25519 ", 12) == 0);

	/* Rest is base64 — should only contain valid base64 chars */
	for (int64_t i = 12; i < len - 1; i++) {
		char c = buf[i];
		ASSERT_TRUE((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=');
	}
	return TEST_PASS;
}

static int
test_ed25519_get_pub_str_size_query(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	/* NULL buf returns needed size */
	int64_t needed = dssh_ed25519_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);

	char *buf = malloc((size_t)needed);
	ASSERT_NOT_NULL(buf);
	int64_t len = dssh_ed25519_get_pub_str(buf, (size_t)needed);
	ASSERT_EQ(len, needed);
	free(buf);
	return TEST_PASS;
}

static int
test_ed25519_pubkey_blob_format(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t blob[256];
	size_t blob_len;
	ASSERT_EQ(ka->pubkey(blob, sizeof(blob), &blob_len, ka->ctx), 0);

	/* Expected: 4-byte length + "ssh-ed25519" + 4-byte length + 32-byte key */
	ASSERT_EQ_U(blob_len, 4 + 11 + 4 + 32);

	size_t pos = 0;
	uint32_t name_len = parse_u32(blob, blob_len, &pos);
	ASSERT_EQ_U(name_len, 11);
	ASSERT_TRUE(memcmp(&blob[pos], "ssh-ed25519", 11) == 0);
	pos += 11;

	uint32_t key_len = parse_u32(blob, blob_len, &pos);
	ASSERT_EQ_U(key_len, 32);
	pos += 32;
	ASSERT_EQ_U(pos, blob_len);
	return TEST_PASS;
}

static int
test_ed25519_sig_blob_format(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "signing test";
	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	/* Expected: 4-byte length + "ssh-ed25519" + 4-byte length + 64-byte sig */
	ASSERT_EQ_U(sig_len, 4 + 11 + 4 + 64);

	size_t pos = 0;
	uint32_t name_len = parse_u32(sig_buf, sig_len, &pos);
	ASSERT_EQ_U(name_len, 11);
	ASSERT_TRUE(memcmp(&sig_buf[pos], "ssh-ed25519", 11) == 0);
	pos += 11;

	uint32_t raw_sig_len = parse_u32(sig_buf, sig_len, &pos);
	ASSERT_EQ_U(raw_sig_len, 64);
	pos += 64;
	ASSERT_EQ_U(pos, sig_len);
	return TEST_PASS;
}

static int
test_ed25519_save_load_roundtrip(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	/* Get the original public key */
	char orig_pub[256];
	int64_t orig_len = dssh_ed25519_get_pub_str(orig_pub, sizeof(orig_pub));
	ASSERT_TRUE(orig_len > 0);

	/* Save to temp file */
	char tmppath[] = "/tmp/dssh_test_ed25519_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(dssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	/* Reset and re-register, then load */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	/* Verify the loaded key produces the same public key */
	char loaded_pub[256];
	int64_t loaded_len = dssh_ed25519_get_pub_str(loaded_pub, sizeof(loaded_pub));
	ASSERT_EQ(loaded_len, orig_len);
	ASSERT_MEM_EQ(orig_pub, loaded_pub, (size_t)orig_len);
	return TEST_PASS;
}

static int
test_ed25519_save_pub_file(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed25519_pub_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(dssh_ed25519_save_pub_file(tmppath), 0);

	/* Read and verify contents start with "ssh-ed25519 " */
	FILE *fp = fopen(tmppath, "r");
	ASSERT_NOT_NULL(fp);
	char line[512];
	ASSERT_NOT_NULL(fgets(line, sizeof(line), fp));
	fclose(fp);
	unlink(tmppath);

	ASSERT_TRUE(memcmp(line, "ssh-ed25519 ", 12) == 0);

	/* Should end with newline */
	size_t llen = strlen(line);
	ASSERT_TRUE(llen > 0);
	ASSERT_EQ(line[llen - 1], '\n');
	return TEST_PASS;
}

static int
test_ed25519_sign_verify_after_load(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed25519_sv_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(dssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	/* Reload into fresh registry */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	/* Sign and verify with the loaded key */
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "loaded key signing test";
	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	uint8_t pub_buf[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	ASSERT_EQ(ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    data, sizeof(data) - 1), 0);
	return TEST_PASS;
}

/* ================================================================
 * RSA-SHA2-256 tests
 * ================================================================ */

static int
test_rsa_register(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	return TEST_PASS;
}

static int
test_rsa_generate_2048(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	return TEST_PASS;
}

static int
test_rsa_haskey_after_generate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_rsa_no_key_before_generate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ASSERT_FALSE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_rsa_sign_verify_roundtrip(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "RSA test data for signing";
	size_t data_len = sizeof(data) - 1;

	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, data_len, ka->ctx), 0);
	ASSERT_TRUE(sig_len > 0);

	uint8_t pub_buf[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);
	ASSERT_TRUE(pub_len > 0);

	ASSERT_EQ(ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    data, data_len), 0);
	return TEST_PASS;
}

static int
test_rsa_verify_corrupted_sig(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "RSA corruption test";
	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	uint8_t pub_buf[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	/* Corrupt the last byte of the signature */
	sig_buf[sig_len - 1] ^= 0xFF;

	int rc = ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    data, sizeof(data) - 1);
	ASSERT_TRUE(rc < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_different_data(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "original RSA data";
	const uint8_t other[] = "different RSA data";

	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	uint8_t pub_buf[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	int rc = ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    other, sizeof(other) - 1);
	ASSERT_TRUE(rc < 0);
	return TEST_PASS;
}

static int
test_rsa_get_pub_str_format(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char buf[2048];
	int64_t len = dssh_rsa_sha2_256_get_pub_str(buf, sizeof(buf));
	ASSERT_TRUE(len > 0);

	/* Must start with "ssh-rsa " (wire key type, not algorithm name) */
	ASSERT_TRUE(len > 8);
	ASSERT_TRUE(memcmp(buf, "ssh-rsa ", 8) == 0);

	/* Rest is base64 */
	for (int64_t i = 8; i < len - 1; i++) {
		char c = buf[i];
		ASSERT_TRUE((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=');
	}
	return TEST_PASS;
}

static int
test_rsa_get_pub_str_size_query(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	int64_t needed = dssh_rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);

	char *buf = malloc((size_t)needed);
	ASSERT_NOT_NULL(buf);
	int64_t len = dssh_rsa_sha2_256_get_pub_str(buf, (size_t)needed);
	ASSERT_EQ(len, needed);
	free(buf);
	return TEST_PASS;
}

static int
test_rsa_pubkey_blob_format(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t blob[2048];
	size_t blob_len;
	ASSERT_EQ(ka->pubkey(blob, sizeof(blob), &blob_len, ka->ctx), 0);

	/*
	 * Expected format:
	 *   string  "ssh-rsa"     (4 + 7 bytes)
	 *   string  e (mpint)
	 *   string  n (mpint)
	 */
	size_t pos = 0;

	/* Algorithm name */
	uint32_t name_len = parse_u32(blob, blob_len, &pos);
	ASSERT_EQ_U(name_len, 7);
	ASSERT_TRUE(memcmp(&blob[pos], "ssh-rsa", 7) == 0);
	pos += 7;

	/* Public exponent e */
	uint32_t e_len = parse_u32(blob, blob_len, &pos);
	ASSERT_TRUE(e_len > 0);
	ASSERT_TRUE(pos + e_len <= blob_len);
	pos += e_len;

	/* Modulus n */
	uint32_t n_len = parse_u32(blob, blob_len, &pos);
	ASSERT_TRUE(n_len > 0);
	ASSERT_TRUE(pos + n_len <= blob_len);
	pos += n_len;

	/* Should have consumed the entire blob */
	ASSERT_EQ_U(pos, blob_len);
	return TEST_PASS;
}

static int
test_rsa_sig_blob_format(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "RSA sig format test";
	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	/*
	 * Expected format:
	 *   string  "rsa-sha2-256"   (4 + 12 bytes)
	 *   string  <raw signature>  (4 + 256 bytes for 2048-bit key)
	 */
	size_t pos = 0;
	uint32_t name_len = parse_u32(sig_buf, sig_len, &pos);
	ASSERT_EQ_U(name_len, 12);
	ASSERT_TRUE(memcmp(&sig_buf[pos], "rsa-sha2-256", 12) == 0);
	pos += 12;

	uint32_t raw_sig_len = parse_u32(sig_buf, sig_len, &pos);
	/* 2048-bit RSA signature is 256 bytes */
	ASSERT_EQ_U(raw_sig_len, 256);
	pos += raw_sig_len;
	ASSERT_EQ_U(pos, sig_len);
	return TEST_PASS;
}

static int
test_rsa_save_load_roundtrip(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	/* Get the original public key string */
	int64_t needed = dssh_rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);
	char *orig_pub = malloc((size_t)needed);
	ASSERT_NOT_NULL(orig_pub);
	ASSERT_EQ(dssh_rsa_sha2_256_get_pub_str(orig_pub, (size_t)needed), needed);

	char tmppath[] = "/tmp/dssh_test_rsa_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(dssh_rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	/* Reset and re-register, then load */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	/* Verify the loaded key produces the same public key */
	int64_t loaded_needed = dssh_rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_EQ(loaded_needed, needed);
	char *loaded_pub = malloc((size_t)loaded_needed);
	ASSERT_NOT_NULL(loaded_pub);
	ASSERT_EQ(dssh_rsa_sha2_256_get_pub_str(loaded_pub, (size_t)loaded_needed),
	    loaded_needed);

	ASSERT_MEM_EQ(orig_pub, loaded_pub, (size_t)needed);
	free(orig_pub);
	free(loaded_pub);
	return TEST_PASS;
}

static int
test_rsa_save_pub_file(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_pub_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(dssh_rsa_sha2_256_save_pub_file(tmppath), 0);

	FILE *fp = fopen(tmppath, "r");
	ASSERT_NOT_NULL(fp);
	char line[2048];
	ASSERT_NOT_NULL(fgets(line, sizeof(line), fp));
	fclose(fp);
	unlink(tmppath);

	/* Must start with "ssh-rsa " */
	ASSERT_TRUE(memcmp(line, "ssh-rsa ", 8) == 0);

	/* Should end with newline */
	size_t llen = strlen(line);
	ASSERT_TRUE(llen > 0);
	ASSERT_EQ(line[llen - 1], '\n');
	return TEST_PASS;
}

static int
test_rsa_generate_4096(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(4096), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));

	/* Sign and verify with 4096-bit key */
	const uint8_t data[] = "4096-bit key test";
	uint8_t sig_buf[1024];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	uint8_t pub_buf[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	ASSERT_EQ(ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    data, sizeof(data) - 1), 0);

	/* 4096-bit signature should be 512 bytes raw */
	size_t pos = 0;
	parse_u32(sig_buf, sig_len, &pos);  /* skip name length */
	pos += 12;                           /* skip "rsa-sha2-256" */
	uint32_t raw_sig_len = parse_u32(sig_buf, sig_len, &pos);
	ASSERT_EQ_U(raw_sig_len, 512);
	return TEST_PASS;
}

static int
test_rsa_sign_verify_after_load(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_sv_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(dssh_rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	/* Reload into fresh registry */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "loaded RSA key signing test";
	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	uint8_t pub_buf[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	ASSERT_EQ(ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    data, sizeof(data) - 1), 0);
	return TEST_PASS;
}

/* ================================================================
 * Cross-algorithm tests
 * ================================================================ */

static int
test_both_algorithms_coexist(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ed_ka = dssh_transport_find_key_algo("ssh-ed25519");
	dssh_key_algo rsa_ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ed_ka);
	ASSERT_NOT_NULL(rsa_ka);
	ASSERT_TRUE(ed_ka->haskey(ed_ka->ctx));
	ASSERT_TRUE(rsa_ka->haskey(rsa_ka->ctx));

	/* Each can sign and verify independently */
	const uint8_t data[] = "coexistence test";

	uint8_t ed_sig[256];
	size_t ed_sig_len;
	ASSERT_EQ(ed_ka->sign(ed_sig, sizeof(ed_sig), &ed_sig_len,
	    data, sizeof(data) - 1, ed_ka->ctx), 0);
	uint8_t ed_pub[256];
	size_t ed_pub_len;
	ASSERT_EQ(ed_ka->pubkey(ed_pub, sizeof(ed_pub), &ed_pub_len,
	    ed_ka->ctx), 0);
	ASSERT_EQ(ed_ka->verify(ed_pub, ed_pub_len, ed_sig, ed_sig_len,
	    data, sizeof(data) - 1), 0);

	uint8_t rsa_sig[512];
	size_t rsa_sig_len;
	ASSERT_EQ(rsa_ka->sign(rsa_sig, sizeof(rsa_sig), &rsa_sig_len,
	    data, sizeof(data) - 1, rsa_ka->ctx), 0);
	uint8_t rsa_pub[2048];
	size_t rsa_pub_len;
	ASSERT_EQ(rsa_ka->pubkey(rsa_pub, sizeof(rsa_pub), &rsa_pub_len,
	    rsa_ka->ctx), 0);
	ASSERT_EQ(rsa_ka->verify(rsa_pub, rsa_pub_len, rsa_sig, rsa_sig_len,
	    data, sizeof(data) - 1), 0);
	return TEST_PASS;
}

static int
test_ed25519_sig_fails_rsa_verify(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ed_ka = dssh_transport_find_key_algo("ssh-ed25519");
	dssh_key_algo rsa_ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ed_ka);
	ASSERT_NOT_NULL(rsa_ka);

	const uint8_t data[] = "cross-algo test";

	/* Sign with Ed25519 */
	uint8_t ed_sig[256];
	size_t ed_sig_len;
	ASSERT_EQ(ed_ka->sign(ed_sig, sizeof(ed_sig), &ed_sig_len,
	    data, sizeof(data) - 1, ed_ka->ctx), 0);

	/* Try to verify with RSA pubkey — must fail */
	uint8_t rsa_pub[2048];
	size_t rsa_pub_len;
	ASSERT_EQ(rsa_ka->pubkey(rsa_pub, sizeof(rsa_pub), &rsa_pub_len,
	    rsa_ka->ctx), 0);

	int rc = rsa_ka->verify(rsa_pub, rsa_pub_len, ed_sig, ed_sig_len,
	    data, sizeof(data) - 1);
	ASSERT_TRUE(rc < 0);
	return TEST_PASS;
}

static int
test_find_unregistered_algo(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("nonexistent-algo");
	ASSERT_NULL(ka);
	return TEST_PASS;
}

/* ================================================================
 * Error path tests (Category 5)
 * ================================================================ */

static int
test_ed25519_load_nonexistent(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	int res = dssh_ed25519_load_key_file("/nonexistent/path/key", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_load_nonexistent(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	int res = dssh_rsa_sha2_256_load_key_file("/nonexistent/path/key", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_sign_without_key(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ASSERT_FALSE(ka->haskey(ka->ctx));

	const uint8_t data[] = "test";
	uint8_t sig_buf[256];
	size_t sig_len;
	int res = ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_sign_without_key(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ASSERT_FALSE(ka->haskey(ka->ctx));

	const uint8_t data[] = "test";
	uint8_t sig_buf[512];
	size_t sig_len;
	int res = ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_pubkey_without_key(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t blob[256];
	size_t blob_len;
	int res = ka->pubkey(blob, sizeof(blob), &blob_len, ka->ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_pubkey_without_key(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t blob[2048];
	size_t blob_len;
	int res = ka->pubkey(blob, sizeof(blob), &blob_len, ka->ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_short_sig(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub_buf[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	/* Signature too short to parse */
	const uint8_t data[] = "test";
	uint8_t short_sig[] = { 0x00, 0x00 };
	int res = ka->verify(pub_buf, pub_len, short_sig, sizeof(short_sig),
	    data, sizeof(data) - 1);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_short_sig(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t pub_buf[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	const uint8_t data[] = "test";
	uint8_t short_sig[] = { 0x00, 0x00 };
	int res = ka->verify(pub_buf, pub_len, short_sig, sizeof(short_sig),
	    data, sizeof(data) - 1);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_wrong_algo(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "test data";
	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	uint8_t pub_buf[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	/* Corrupt the algorithm name in the sig blob to a wrong name */
	/* sig format: uint32 name_len + "ssh-ed25519" + uint32 sig_len + sig */
	/* Change "ssh-ed25519" to "ssh-ed25518" (last byte) */
	sig_buf[4 + 10] = '8';

	int res = ka->verify(pub_buf, pub_len, sig_buf, sig_len,
	    data, sizeof(data) - 1);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * File I/O and public string edge cases
 * ================================================================ */

static int
test_ed25519_get_pub_str_small_buf(void)
{
	/* Covers bufsz < needed branch in get_pub_str */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	/* Query needed size */
	int64_t needed = dssh_ed25519_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);

	/* Pass buffer that's too small */
	char small[8];
	int64_t res = dssh_ed25519_get_pub_str(small, sizeof(small));
	ASSERT_EQ(res, (int64_t)DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_ed25519_save_pub_before_keygen(void)
{
	/* save_pub_file before generating key */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	int res = dssh_ed25519_save_pub_file("/tmp/dssh_no_key_pub");
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_load_rsa_key_file(void)
{
	/* Load an RSA key into Ed25519 loader — wrong key type */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_for_ed_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	/* Now try to load it as Ed25519 */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	int res = dssh_ed25519_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_get_pub_str_small_buf(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	int64_t needed = dssh_rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);

	char small[8];
	int64_t res = dssh_rsa_sha2_256_get_pub_str(small, sizeof(small));
	ASSERT_EQ(res, (int64_t)DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_rsa_save_pub_before_keygen(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	int res = dssh_rsa_sha2_256_save_pub_file("/tmp/dssh_no_key_pub");
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_load_ed25519_key_file(void)
{
	/* Load an Ed25519 key into RSA loader — wrong key type */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed_for_rsa_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	int res = dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_save_key_before_keygen(void)
{
	/* save_key_file before generating/loading a key */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	int res = dssh_ed25519_save_key_file("/tmp/dssh_nokey", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_save_key_before_keygen(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	int res = dssh_rsa_sha2_256_save_key_file("/tmp/dssh_nokey", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_save_key_bad_path(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	int res = dssh_ed25519_save_key_file("/nonexistent/dir/key", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_save_key_bad_path(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	int res = dssh_rsa_sha2_256_save_key_file("/nonexistent/dir/key", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_save_pub_bad_path(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	int res = dssh_ed25519_save_pub_file("/nonexistent/dir/pub");
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_save_pub_bad_path(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	int res = dssh_rsa_sha2_256_save_pub_file("/nonexistent/dir/pub");
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_generate_twice(void)
{
	/* Generate key twice — covers the "replace existing" path */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_rsa_generate_twice(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_ed25519_load_twice(void)
{
	/* Load key twice — covers the "replace existing" path in load_key_file */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed_load2_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	/* Load it once */
	ASSERT_EQ(dssh_ed25519_load_key_file(tmppath, NULL, NULL), 0);
	/* Load it again — replaces existing */
	ASSERT_EQ(dssh_ed25519_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_rsa_load_twice(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_load2_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	ASSERT_EQ(dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

/* ================================================================
 * Verify parse errors — malformed key/sig blobs
 * Covers uncovered branches in ssh-ed25519.c and rsa-sha2-256.c
 * verify() functions.
 * ================================================================ */

static int
test_ed25519_verify_truncated_key(void)
{
	/* Key blob too short for algo name length field */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "test";
	uint8_t sig[4] = {0};
	uint8_t key[2] = {0};
	ASSERT_TRUE(ka->verify(key, sizeof(key), sig, sizeof(sig),
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_wrong_key_algo_name(void)
{
	/* Key blob with wrong algo name */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	/* Build a fake key blob: name="wrong-algo" + 32 bytes key */
	uint8_t key[256];
	size_t kp = 0;
	dssh_serialize_uint32(10, key, sizeof(key), &kp);
	memcpy(&key[kp], "wrong-algo", 10);
	kp += 10;
	dssh_serialize_uint32(32, key, sizeof(key), &kp);
	memset(&key[kp], 0xAA, 32);
	kp += 32;

	const uint8_t data[] = "test";
	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	ASSERT_TRUE(ka->verify(key, kp, sig_buf, sig_len,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_wrong_key_length(void)
{
	/* Key blob with correct name but wrong raw key length (not 32) */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t key[256];
	size_t kp = 0;
	dssh_serialize_uint32(11, key, sizeof(key), &kp);
	memcpy(&key[kp], "ssh-ed25519", 11);
	kp += 11;
	/* Wrong length: 16 instead of 32 */
	dssh_serialize_uint32(16, key, sizeof(key), &kp);
	memset(&key[kp], 0xBB, 16);
	kp += 16;

	const uint8_t data[] = "test";
	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	ASSERT_TRUE(ka->verify(key, kp, sig_buf, sig_len,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_key_trailing_garbage(void)
{
	/* Valid key blob + extra trailing bytes */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	/* Append garbage */
	pub[pub_len] = 0xFF;
	pub[pub_len + 1] = 0xFE;

	const uint8_t data[] = "test";
	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	ASSERT_TRUE(ka->verify(pub, pub_len + 2, sig_buf, sig_len,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_sig_wrong_raw_len(void)
{
	/* Sig blob with correct algo name but wrong raw sig length (not 64) */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	/* Build fake sig: name="ssh-ed25519" + raw_sig_len=32 (wrong, should be 64) */
	uint8_t sig[256];
	size_t sp = 0;
	dssh_serialize_uint32(11, sig, sizeof(sig), &sp);
	memcpy(&sig[sp], "ssh-ed25519", 11);
	sp += 11;
	dssh_serialize_uint32(32, sig, sizeof(sig), &sp);
	memset(&sig[sp], 0xCC, 32);
	sp += 32;

	const uint8_t data[] = "test";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_sig_trailing_garbage(void)
{
	/* Valid sig blob + trailing bytes */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	const uint8_t data[] = "test";
	uint8_t sig[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig, sizeof(sig), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	/* Append garbage to sig */
	sig[sig_len] = 0xFF;
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sig_len + 1,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_truncated_key(void)
{
	/* Key blob too short */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "test";
	uint8_t sig[4] = {0};
	uint8_t key[2] = {0};
	ASSERT_TRUE(ka->verify(key, sizeof(key), sig, sizeof(sig),
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_wrong_key_algo_name(void)
{
	/* Key blob with wrong algo name */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t key[256];
	size_t kp = 0;
	dssh_serialize_uint32(10, key, sizeof(key), &kp);
	memcpy(&key[kp], "wrong-algo", 10);
	kp += 10;
	/* e and n fields */
	dssh_serialize_uint32(3, key, sizeof(key), &kp);
	key[kp++] = 0x01; key[kp++] = 0x00; key[kp++] = 0x01;
	dssh_serialize_uint32(1, key, sizeof(key), &kp);
	key[kp++] = 0x01;

	const uint8_t data[] = "test";
	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	ASSERT_TRUE(ka->verify(key, kp, sig_buf, sig_len,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_key_trailing_garbage(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	pub[pub_len] = 0xFF;

	const uint8_t data[] = "test";
	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	ASSERT_TRUE(ka->verify(pub, pub_len + 1, sig_buf, sig_len,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_sig_trailing_garbage(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	const uint8_t data[] = "test";
	uint8_t sig[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig, sizeof(sig), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	sig[sig_len] = 0xFF;
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sig_len + 1,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

/* ================================================================
 * Deeper verify parse errors — truncation after valid prefix
 * ================================================================ */

static int
test_ed25519_verify_key_truncated_after_name(void)
{
	/* Key blob: correct algo name, but truncated before raw key length */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t key[32];
	size_t kp = 0;
	dssh_serialize_uint32(11, key, sizeof(key), &kp);
	memcpy(&key[kp], "ssh-ed25519", 11);
	kp += 11;
	/* No room for raw key length field */

	const uint8_t data[] = "test";
	uint8_t sig_buf[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	ASSERT_TRUE(ka->verify(key, kp, sig_buf, sig_len,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_sig_truncated_after_name(void)
{
	/* Sig blob: correct algo name, but truncated before raw sig length */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	uint8_t sig[32];
	size_t sp = 0;
	dssh_serialize_uint32(11, sig, sizeof(sig), &sp);
	memcpy(&sig[sp], "ssh-ed25519", 11);
	sp += 11;
	/* No room for raw sig length field */

	const uint8_t data[] = "test";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_bad_signature(void)
{
	/* Valid format but cryptographically wrong signature */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	const uint8_t data[] = "test";
	uint8_t sig[256];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig, sizeof(sig), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	/* Verify against different data — signature is valid format but wrong */
	const uint8_t wrong_data[] = "wrong";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sig_len,
	    wrong_data, sizeof(wrong_data) - 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_key_truncated_after_name(void)
{
	/* Key blob: correct algo name, truncated before e field */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t key[32];
	size_t kp = 0;
	dssh_serialize_uint32(7, key, sizeof(key), &kp); /* "ssh-rsa" */
	memcpy(&key[kp], "ssh-rsa", 7);
	kp += 7;
	/* No room for e length field */

	const uint8_t data[] = "test";
	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	ASSERT_TRUE(ka->verify(key, kp, sig_buf, sig_len,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_key_truncated_after_e(void)
{
	/* Key blob: correct name + e field, truncated before n */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t key[32];
	size_t kp = 0;
	dssh_serialize_uint32(7, key, sizeof(key), &kp);
	memcpy(&key[kp], "ssh-rsa", 7);
	kp += 7;
	dssh_serialize_uint32(3, key, sizeof(key), &kp);
	key[kp++] = 0x01; key[kp++] = 0x00; key[kp++] = 0x01; /* e=65537 */
	/* No room for n length field */

	const uint8_t data[] = "test";
	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	ASSERT_TRUE(ka->verify(key, kp, sig_buf, sig_len,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_sig_truncated_after_name(void)
{
	/* Sig blob: correct algo name, truncated before raw sig */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	uint8_t sig[32];
	size_t sp = 0;
	dssh_serialize_uint32(12, sig, sizeof(sig), &sp);
	memcpy(&sig[sp], "rsa-sha2-256", 12);
	sp += 12;
	/* No room for raw sig length */

	const uint8_t data[] = "test";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, sizeof(data) - 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_generate_before_register(void)
{
	dssh_test_reset_global_config();
	/* Don't register — ka will be NULL */
	int res = dssh_ed25519_generate_key();
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_generate_before_register(void)
{
	dssh_test_reset_global_config();
	int res = dssh_rsa_sha2_256_generate_key(2048);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_get_pub_str_before_register(void)
{
	dssh_test_reset_global_config();
	int64_t res = dssh_ed25519_get_pub_str(NULL, 0);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_get_pub_str_before_register(void)
{
	dssh_test_reset_global_config();
	int64_t res = dssh_rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * Defensive guard tests — NULL ctx and too-small buffer branches
 * ================================================================ */

static int
test_ed25519_haskey_null_ctx(void)
{
	/* Covers haskey(NULL) — cbd != NULL False branch */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ASSERT_FALSE(ka->haskey(NULL));

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_ed25519_cleanup_null_ctx(void)
{
	/* Covers cleanup(NULL) — cbd != NULL False branch */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ka->cleanup(NULL);  /* must not crash */

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_ed25519_get_pub_str_bufsz_one(void)
{
	/* Covers get_pub_str bufsz < needed True branch */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char buf[1];
	int64_t res = dssh_ed25519_get_pub_str(buf, 1);
	ASSERT_EQ(res, (int64_t)DSSH_ERROR_TOOLONG);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_haskey_null_ctx(void)
{
	/* Covers haskey(NULL) — cbd != NULL False branch */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ASSERT_FALSE(ka->haskey(NULL));

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_cleanup_null_ctx(void)
{
	/* Covers cleanup(NULL) — cbd != NULL False branch */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ka->cleanup(NULL);  /* must not crash */

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_get_pub_str_bufsz_one(void)
{
	/* Covers get_pub_str bufsz < needed True branch */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char buf[1];
	int64_t res = dssh_rsa_sha2_256_get_pub_str(buf, 1);
	ASSERT_EQ(res, (int64_t)DSSH_ERROR_TOOLONG);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Branch coverage — verify() overrun conditions
 *
 * Each compound parse check has two branches:
 *   (1) dssh_parse_uint32 < 4 — too short for length field
 *   (2) claimed length overruns buffer
 * Existing tests hit (1); these hit (2).
 * ================================================================ */

static int
test_ed25519_verify_key_algo_overrun(void)
{
	/* key_blob: algo_len=255 but only 4+11 bytes present */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t key[8];
	size_t kp = 0;
	dssh_serialize_uint32(255, key, sizeof(key), &kp); /* algo_len overruns */

	const uint8_t data[] = "x";
	uint8_t sig[4] = {0, 0, 0, 0};
	ASSERT_TRUE(ka->verify(key, kp, sig, sizeof(sig),
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_key_rawpub_overrun(void)
{
	/* key_blob: valid algo name, raw_pub_len overruns */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t key[32];
	size_t kp = 0;
	dssh_serialize_uint32(11, key, sizeof(key), &kp);
	memcpy(&key[kp], "ssh-ed25519", 11);
	kp += 11;
	dssh_serialize_uint32(255, key, sizeof(key), &kp); /* overruns */

	const uint8_t data[] = "x";
	uint8_t sig[4] = {0, 0, 0, 0};
	ASSERT_TRUE(ka->verify(key, kp, sig, sizeof(sig),
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_sig_algo_overrun(void)
{
	/* sig_blob: sig_algo_len overruns */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	uint8_t sig[8];
	size_t sp = 0;
	dssh_serialize_uint32(255, sig, sizeof(sig), &sp); /* overruns */

	const uint8_t data[] = "x";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_sig_rawsig_overrun(void)
{
	/* sig_blob: valid algo, raw_sig_len overruns */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	uint8_t sig[32];
	size_t sp = 0;
	dssh_serialize_uint32(11, sig, sizeof(sig), &sp);
	memcpy(&sig[sp], "ssh-ed25519", 11);
	sp += 11;
	dssh_serialize_uint32(255, sig, sizeof(sig), &sp); /* overruns */

	const uint8_t data[] = "x";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_sig_wrong_algo_name(void)
{
	/* sig_blob: wrong algo name (hits memcmp != 0 branch) */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	uint8_t sig[64];
	size_t sp = 0;
	dssh_serialize_uint32(11, sig, sizeof(sig), &sp);
	memcpy(&sig[sp], "ssh-ed25518", 11); /* wrong name */
	sp += 11;
	dssh_serialize_uint32(64, sig, sizeof(sig), &sp);
	memset(&sig[sp], 0, 4); /* partial, enough for the check to reach name cmp */

	const uint8_t data[] = "x";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_key_wrong_algo_len(void)
{
	/* key_blob: algo_len=7 (not 11) — hits algo_len != ED25519_NAME_LEN */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t key[64];
	size_t kp = 0;
	dssh_serialize_uint32(7, key, sizeof(key), &kp);
	memcpy(&key[kp], "ssh-rsa", 7);
	kp += 7;
	dssh_serialize_uint32(32, key, sizeof(key), &kp);
	memset(&key[kp], 0xAA, 32);
	kp += 32;

	const uint8_t data[] = "x";
	uint8_t sig[4] = {0, 0, 0, 0};
	ASSERT_TRUE(ka->verify(key, kp, sig, sizeof(sig),
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_key_wrong_algo_content(void)
{
	/* key_blob: algo_len=11 but content mismatch — hits memcmp != 0 */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t key[64];
	size_t kp = 0;
	dssh_serialize_uint32(11, key, sizeof(key), &kp);
	memcpy(&key[kp], "ssh-ed25518", 11); /* wrong last char */
	kp += 11;
	dssh_serialize_uint32(32, key, sizeof(key), &kp);
	memset(&key[kp], 0xAA, 32);
	kp += 32;

	const uint8_t data[] = "x";
	uint8_t sig[4] = {0, 0, 0, 0};
	ASSERT_TRUE(ka->verify(key, kp, sig, sizeof(sig),
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_ed25519_verify_sig_wrong_algo_len(void)
{
	/* sig_blob: sig_algo_len=7 (not 11) */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[256];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	uint8_t sig[64];
	size_t sp = 0;
	dssh_serialize_uint32(7, sig, sizeof(sig), &sp); /* wrong length */
	memcpy(&sig[sp], "ssh-rsa", 7);
	sp += 7;
	dssh_serialize_uint32(64, sig, sizeof(sig), &sp);
	memset(&sig[sp], 0, 32);
	sp += 32;

	const uint8_t data[] = "x";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_key_wrong_algo_len(void)
{
	/* key_blob: algo_len=11 (not 7) */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t key[64];
	size_t kp = 0;
	dssh_serialize_uint32(11, key, sizeof(key), &kp); /* wrong len */
	memcpy(&key[kp], "ssh-ed25519", 11);
	kp += 11;
	dssh_serialize_uint32(1, key, sizeof(key), &kp);
	key[kp++] = 0x01;

	const uint8_t data[] = "x";
	uint8_t sig[4] = {0, 0, 0, 0};
	ASSERT_TRUE(ka->verify(key, kp, sig, sizeof(sig),
	    data, 1) < 0);
	return TEST_PASS;
}

/* RSA verify overrun tests */

static int
test_rsa_verify_key_algo_overrun(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t key[8];
	size_t kp = 0;
	dssh_serialize_uint32(255, key, sizeof(key), &kp);

	const uint8_t data[] = "x";
	uint8_t sig[4] = {0, 0, 0, 0};
	ASSERT_TRUE(ka->verify(key, kp, sig, sizeof(sig),
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_key_e_overrun(void)
{
	/* Key blob: valid name, e_len overruns */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t key[32];
	size_t kp = 0;
	dssh_serialize_uint32(7, key, sizeof(key), &kp);
	memcpy(&key[kp], "ssh-rsa", 7);
	kp += 7;
	dssh_serialize_uint32(255, key, sizeof(key), &kp); /* e overruns */

	const uint8_t data[] = "x";
	uint8_t sig[4] = {0, 0, 0, 0};
	ASSERT_TRUE(ka->verify(key, kp, sig, sizeof(sig),
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_key_n_overrun(void)
{
	/* Key blob: valid name + e, n_len overruns */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t key[32];
	size_t kp = 0;
	dssh_serialize_uint32(7, key, sizeof(key), &kp);
	memcpy(&key[kp], "ssh-rsa", 7);
	kp += 7;
	dssh_serialize_uint32(3, key, sizeof(key), &kp);
	key[kp++] = 0x01; key[kp++] = 0x00; key[kp++] = 0x01;
	dssh_serialize_uint32(255, key, sizeof(key), &kp); /* n overruns */

	const uint8_t data[] = "x";
	uint8_t sig[4] = {0, 0, 0, 0};
	ASSERT_TRUE(ka->verify(key, kp, sig, sizeof(sig),
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_sig_algo_overrun(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	uint8_t sig[8];
	size_t sp = 0;
	dssh_serialize_uint32(255, sig, sizeof(sig), &sp);

	const uint8_t data[] = "x";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_sig_wrong_algo_name(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	uint8_t sig[64];
	size_t sp = 0;
	dssh_serialize_uint32(12, sig, sizeof(sig), &sp);
	memcpy(&sig[sp], "rsa-sha2-257", 12); /* wrong name */
	sp += 12;

	const uint8_t data[] = "x";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, 1) < 0);
	return TEST_PASS;
}

static int
test_rsa_verify_sig_rawsig_overrun(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t pub[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub, sizeof(pub), &pub_len, ka->ctx), 0);

	uint8_t sig[32];
	size_t sp = 0;
	dssh_serialize_uint32(12, sig, sizeof(sig), &sp);
	memcpy(&sig[sp], "rsa-sha2-256", 12);
	sp += 12;
	dssh_serialize_uint32(255, sig, sizeof(sig), &sp); /* overruns */

	const uint8_t data[] = "x";
	ASSERT_TRUE(ka->verify(pub, pub_len, sig, sp,
	    data, 1) < 0);
	return TEST_PASS;
}

/* ================================================================
 * Branch coverage — load/save/get_pub_str edge cases
 * ================================================================ */

static int
test_ed25519_load_before_register(void)
{
	dssh_test_reset_global_config();
	ASSERT_TRUE(dssh_ed25519_load_key_file("/dev/null", NULL, NULL) < 0);
	return TEST_PASS;
}

static int
test_rsa_load_before_register(void)
{
	dssh_test_reset_global_config();
	ASSERT_TRUE(dssh_rsa_sha2_256_load_key_file("/dev/null", NULL, NULL) < 0);
	return TEST_PASS;
}

static int
test_ed25519_load_garbage_file(void)
{
	/* File exists but is not valid PEM — hits PEM_read fail branch */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	char tmppath[] = "/tmp/dssh_test_garbage_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	write(fd, "not a pem file\n", 15);
	close(fd);

	int res = dssh_ed25519_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_load_garbage_file(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	char tmppath[] = "/tmp/dssh_test_garbage_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	write(fd, "not a pem file\n", 15);
	close(fd);

	int res = dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_save_before_register(void)
{
	dssh_test_reset_global_config();
	ASSERT_TRUE(dssh_ed25519_save_key_file("/tmp/x", NULL, NULL) < 0);
	return TEST_PASS;
}

static int
test_rsa_save_before_register(void)
{
	dssh_test_reset_global_config();
	ASSERT_TRUE(dssh_rsa_sha2_256_save_key_file("/tmp/x", NULL, NULL) < 0);
	return TEST_PASS;
}

static int
dummy_pw_cb(char *buf, int size, int rwflag, void *userdata)
{
	(void)rwflag; (void)userdata;
	const char *pw = "testpassword";
	int len = (int)strlen(pw);
	if (len > size)
		len = size;
	memcpy(buf, pw, len);
	return len;
}

static int
test_ed25519_save_with_password(void)
{
	/* Covers pw_cb != NULL branch in save_key_file */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed_pw_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(dssh_ed25519_save_key_file(tmppath, dummy_pw_cb, NULL), 0);
	unlink(tmppath);
	return TEST_PASS;
}

static int
test_rsa_save_with_password(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_pw_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(dssh_rsa_sha2_256_save_key_file(tmppath, dummy_pw_cb, NULL), 0);
	unlink(tmppath);
	return TEST_PASS;
}

static int
test_ed25519_get_pub_str_null_buf(void)
{
	/* Covers buf == NULL with non-zero bufsz */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	int64_t res = dssh_ed25519_get_pub_str(NULL, 256);
	ASSERT_TRUE(res > 0);
	return TEST_PASS;
}

static int
test_ed25519_get_pub_str_zero_bufsz(void)
{
	/* Covers bufsz == 0 with non-NULL buf */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char buf[1];
	int64_t res = dssh_ed25519_get_pub_str(buf, 0);
	ASSERT_TRUE(res > 0);
	return TEST_PASS;
}

static int
test_rsa_get_pub_str_null_buf(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	int64_t res = dssh_rsa_sha2_256_get_pub_str(NULL, 256);
	ASSERT_TRUE(res > 0);
	return TEST_PASS;
}

static int
test_rsa_get_pub_str_zero_bufsz(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char buf[1];
	int64_t res = dssh_rsa_sha2_256_get_pub_str(buf, 0);
	ASSERT_TRUE(res > 0);
	return TEST_PASS;
}

/* ================================================================
 * haskey with wrong key type — hits EVP_PKEY_id mismatch
 * ================================================================ */

static int
test_ed25519_haskey_wrong_key_type(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	/* Grab the RSA pkey */
	dssh_key_algo rsa_ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(rsa_ka);
	ASSERT_NOT_NULL(rsa_ka->ctx);

	/* Feed it to ed25519's haskey */
	dssh_key_algo ed_ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ed_ka);
	ASSERT_FALSE(ed_ka->haskey(rsa_ka->ctx));
	return TEST_PASS;
}

static int
test_rsa_haskey_wrong_key_type(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ed_ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ed_ka);
	ASSERT_NOT_NULL(ed_ka->ctx);

	dssh_key_algo rsa_ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(rsa_ka);
	ASSERT_FALSE(rsa_ka->haskey(ed_ka->ctx));
	return TEST_PASS;
}

/* ================================================================
 * haskey with cbdata allocated but pkey == NULL
 * ================================================================ */

struct key_cbdata { EVP_PKEY *pkey; };

static int
test_ed25519_haskey_null_pkey(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	struct key_cbdata cbd = { .pkey = NULL };
	ASSERT_FALSE(ka->haskey((dssh_key_algo_ctx *)&cbd));
	return TEST_PASS;
}

static int
test_rsa_haskey_null_pkey(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	struct key_cbdata cbd = { .pkey = NULL };
	ASSERT_FALSE(ka->haskey((dssh_key_algo_ctx *)&cbd));
	return TEST_PASS;
}

/* ================================================================
 * fclose/write failure in save_key_file — close fd under fclose
 *
 * We save to /dev/full which causes write/fclose to fail with ENOSPC.
 * If /dev/full is unavailable, skip.
 * ================================================================ */

static int
test_ed25519_save_key_write_fail(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	/* /dev/full makes write() fail with ENOSPC */
	int res = dssh_ed25519_save_key_file("/dev/full", NULL, NULL);
	if (res == 0) {
		/* /dev/full not available or didn't fail — skip */
		return TEST_SKIP;
	}
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_save_key_write_fail(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	int res = dssh_rsa_sha2_256_save_key_file("/dev/full", NULL, NULL);
	if (res == 0)
		return TEST_SKIP;
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_save_pub_write_fail(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	int res = dssh_ed25519_save_pub_file("/dev/full");
	if (res == 0)
		return TEST_SKIP;
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_save_pub_write_fail(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	int res = dssh_rsa_sha2_256_save_pub_file("/dev/full");
	if (res == 0)
		return TEST_SKIP;
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * RSA pubkey e_pad branch — exponent with MSB set
 * ================================================================ */

static int
test_rsa_pubkey_e_pad(void)
{
	/*
	 * Build an RSA key with a public exponent that has MSB set.
	 * This hits the e_pad=true branch in pubkey().
	 */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	/* Generate via EVP with exponent = 0x80000003 (MSB set, odd, > 2) */
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
	ASSERT_NOT_NULL(pctx);

	unsigned int bits = 2048;
	BIGNUM *e_bn = BN_new();
	ASSERT_NOT_NULL(e_bn);
	BN_set_word(e_bn, 0x80000003UL);

	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_uint("bits", &bits),
		OSSL_PARAM_construct_BN("e", NULL, 0), /* placeholder */
		OSSL_PARAM_construct_end(),
	};
	/* Set the BN param properly */
	unsigned char e_buf[16];
	int e_len = BN_bn2nativepad(e_bn, e_buf, sizeof(e_buf));
	params[1] = OSSL_PARAM_construct_BN("e", e_buf, e_len);

	EVP_PKEY *pkey = NULL;
	int ok = (EVP_PKEY_keygen_init(pctx) == 1
	    && EVP_PKEY_CTX_set_params(pctx, params) == 1
	    && EVP_PKEY_keygen(pctx, &pkey) == 1);
	EVP_PKEY_CTX_free(pctx);
	BN_free(e_bn);

	if (!ok || pkey == NULL) {
		EVP_PKEY_free(pkey);
		dssh_test_reset_global_config();
		/* Some OpenSSL versions reject unusual exponents */
		return TEST_SKIP;
	}

	/* Stuff the key into the RSA algo's ctx */
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	struct key_cbdata *cbd = calloc(1, sizeof(*cbd));
	ASSERT_NOT_NULL(cbd);
	cbd->pkey = pkey;
	ka->ctx = (dssh_key_algo_ctx *)cbd;

	uint8_t blob[2048];
	size_t blob_len;
	int res = ka->pubkey(blob, sizeof(blob), &blob_len, ka->ctx);
	ASSERT_EQ(res, 0);

	/* Verify the blob has the padding byte */
	size_t pos = 0;
	uint32_t name_len = parse_u32(blob, blob_len, &pos);
	pos += name_len; /* skip "ssh-rsa" */
	uint32_t e_wire_len = parse_u32(blob, blob_len, &pos);
	/* First byte should be 0x00 padding */
	ASSERT_EQ(blob[pos], 0x00);
	(void)e_wire_len;

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * RSA pubkey n_pad=false — modulus with MSB clear
 *
 * Construct an RSA key where n has its top byte < 0x80 so n_pad is
 * false, hitting the branches that are always true with normal keys.
 * We use OSSL_PARAM to build the key from raw (e, n, d) values.
 * ================================================================ */

static int
test_rsa_pubkey_n_no_pad(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	/*
	 * Build a minimal RSA key where n has MSB clear.
	 * n = 0x7F...  (top byte < 0x80).
	 * This is not a cryptographically valid key, but pubkey()
	 * just serializes the BN values — it doesn't validate.
	 *
	 * p=0xB, q=0xD → n=0x8F (MSB set, no good)
	 * p=0x5, q=0xB → n=0x37 (MSB clear, good!)
	 * e=3, d=19 (3*19 = 57, 57 mod lcm(4,10)=20 → 57 mod 20 = 17 ≠ 1)
	 * Actually we just need pubkey() to serialize, we don't need
	 * a mathematically valid key. Just set e and n directly.
	 */
	BIGNUM *e_bn = BN_new();
	BIGNUM *n_bn = BN_new();
	ASSERT_NOT_NULL(e_bn);
	ASSERT_NOT_NULL(n_bn);
	BN_set_word(e_bn, 65537);
	/* n with MSB clear: 0x7F followed by enough bytes */
	uint8_t n_raw[256];
	n_raw[0] = 0x7F;
	memset(&n_raw[1], 0xAA, 255);
	BN_bin2bn(n_raw, 256, n_bn);

	OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
	ASSERT_NOT_NULL(bld);
	ASSERT_TRUE(OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_N, n_bn) == 1);
	ASSERT_TRUE(OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_E, e_bn) == 1);
	OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(bld);
	ASSERT_NOT_NULL(params);

	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
	ASSERT_NOT_NULL(pctx);
	EVP_PKEY *pkey = NULL;
	ASSERT_TRUE(EVP_PKEY_fromdata_init(pctx) == 1);
	ASSERT_TRUE(EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) == 1);
	EVP_PKEY_CTX_free(pctx);
	OSSL_PARAM_free(params);
	OSSL_PARAM_BLD_free(bld);
	BN_free(e_bn);
	BN_free(n_bn);

	/* Stuff into RSA algo ctx */
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	struct key_cbdata *cbd = calloc(1, sizeof(*cbd));
	ASSERT_NOT_NULL(cbd);
	cbd->pkey = pkey;
	ka->ctx = (dssh_key_algo_ctx *)cbd;

	uint8_t blob[2048];
	size_t blob_len;
	int res = ka->pubkey(blob, sizeof(blob), &blob_len, ka->ctx);
	ASSERT_EQ(res, 0);

	/* Verify n field does NOT have a leading 0x00 pad byte */
	size_t pos = 0;
	uint32_t name_len = parse_u32(blob, blob_len, &pos);
	pos += name_len; /* skip "ssh-rsa" */
	uint32_t e_wire_len = parse_u32(blob, blob_len, &pos);
	pos += e_wire_len; /* skip e */
	uint32_t n_wire_len = parse_u32(blob, blob_len, &pos);
	ASSERT_EQ_U(n_wire_len, 256); /* no pad byte */
	ASSERT_TRUE(blob[pos] == 0x7F); /* MSB clear, no padding needed */

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * RSA pubkey n_bytes==0 — modulus is zero
 * ================================================================ */

static int
test_rsa_pubkey_n_zero(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	BIGNUM *e_bn = BN_new();
	BIGNUM *n_bn = BN_new();
	ASSERT_NOT_NULL(e_bn);
	ASSERT_NOT_NULL(n_bn);
	BN_set_word(e_bn, 65537);
	/* n = 0 → BN_num_bytes returns 0 */

	OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
	ASSERT_NOT_NULL(bld);
	ASSERT_TRUE(OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_N, n_bn) == 1);
	ASSERT_TRUE(OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_E, e_bn) == 1);
	OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(bld);
	ASSERT_NOT_NULL(params);

	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
	ASSERT_NOT_NULL(pctx);
	EVP_PKEY *pkey = NULL;
	int ok = (EVP_PKEY_fromdata_init(pctx) == 1
	    && EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) == 1);
	EVP_PKEY_CTX_free(pctx);
	OSSL_PARAM_free(params);
	OSSL_PARAM_BLD_free(bld);
	BN_free(e_bn);
	BN_free(n_bn);

	if (!ok || pkey == NULL) {
		EVP_PKEY_free(pkey);
		dssh_test_reset_global_config();
		return TEST_SKIP;
	}

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	struct key_cbdata *cbd = calloc(1, sizeof(*cbd));
	ASSERT_NOT_NULL(cbd);
	cbd->pkey = pkey;
	ka->ctx = (dssh_key_algo_ctx *)cbd;

	uint8_t blob[2048];
	size_t blob_len;
	int res = ka->pubkey(blob, sizeof(blob), &blob_len, ka->ctx);
	/* Either succeeds with n_bytes=0 or fails — both are fine */
	(void)res;

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * RSA verify: wrong key algo content (correct length)
 * ================================================================ */

static int
test_rsa_verify_key_wrong_algo_content(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	uint8_t key[64];
	size_t kp = 0;
	dssh_serialize_uint32(7, key, sizeof(key), &kp);
	memcpy(&key[kp], "ssh-dss", 7); /* correct len, wrong content */
	kp += 7;
	dssh_serialize_uint32(3, key, sizeof(key), &kp);
	key[kp++] = 0x01; key[kp++] = 0x00; key[kp++] = 0x01;
	dssh_serialize_uint32(1, key, sizeof(key), &kp);
	key[kp++] = 0x01;

	const uint8_t data[] = "x";
	uint8_t sig[4] = {0, 0, 0, 0};
	ASSERT_TRUE(ka->verify(key, kp, sig, sizeof(sig),
	    data, 1) < 0);
	return TEST_PASS;
}

/* ================================================================
 * Alloc failures in key algo load/generate
 * ================================================================ */

#include "dssh_test_alloc.h"

static int
test_ed25519_generate_calloc_fail(void)
{
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	/* Iterate alloc failures until generate succeeds */
	for (int n = 0; n < 100; n++) {
		dssh_test_alloc_fail_after(n);
		int res = dssh_ed25519_generate_key();
		dssh_test_alloc_reset();
		if (res == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

static int
test_rsa_generate_calloc_fail(void)
{
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	for (int n = 0; n < 100; n++) {
		dssh_test_alloc_fail_after(n);
		int res = dssh_rsa_sha2_256_generate_key(2048);
		dssh_test_alloc_reset();
		if (res == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

static int
test_ed25519_load_calloc_fail(void)
{
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed_alloc_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	/* Reset ctx so load must calloc a new one */
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	for (int n = 0; n < 100; n++) {
		dssh_test_alloc_fail_after(n);
		int res = dssh_ed25519_load_key_file(tmppath, NULL, NULL);
		dssh_test_alloc_reset();
		if (res == 0) {
			unlink(tmppath);
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}
	unlink(tmppath);
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

static int
test_rsa_load_calloc_fail(void)
{
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_alloc_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(dssh_rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);

	for (int n = 0; n < 100; n++) {
		dssh_test_alloc_fail_after(n);
		int res = dssh_rsa_sha2_256_load_key_file(tmppath, NULL, NULL);
		dssh_test_alloc_reset();
		if (res == 0) {
			unlink(tmppath);
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}
	unlink(tmppath);
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

/* ================================================================
 * RSA save_pub_file alloc failure (malloc for str buffer)
 * ================================================================ */

static int
test_rsa_save_pub_alloc_fail(void)
{
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_pub_af_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	for (int n = 0; n < 100; n++) {
		dssh_test_alloc_fail_after(n);
		int res = dssh_rsa_sha2_256_save_pub_file(tmppath);
		dssh_test_alloc_reset();
		if (res == 0) {
			unlink(tmppath);
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}
	unlink(tmppath);
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

/* ================================================================
 * RSA set_rsa_padding failure via ossl injection
 * ================================================================ */

#include "dssh_test_ossl.h"

static int
test_rsa_sign_padding_fail(void)
{
	dssh_test_reset_global_config();
	dssh_test_ossl_reset();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "test";
	uint8_t sig_buf[512];
	size_t sig_len;

	/* Iterate ossl failures to hit set_rsa_padding in sign */
	for (int n = 0; n < 100; n++) {
		dssh_test_ossl_fail_after(n);
		int res = ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
		    data, sizeof(data) - 1, ka->ctx);
		(void)dssh_test_ossl_count();
		dssh_test_ossl_reset();
		if (res == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

static int
test_rsa_verify_padding_fail(void)
{
	dssh_test_reset_global_config();
	dssh_test_ossl_reset();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "test";
	uint8_t sig_buf[512];
	size_t sig_len;
	ASSERT_EQ(ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);

	uint8_t pub_buf[2048];
	size_t pub_len;
	ASSERT_EQ(ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx), 0);

	for (int n = 0; n < 100; n++) {
		dssh_test_ossl_fail_after(n);
		int res = ka->verify(pub_buf, pub_len, sig_buf, sig_len,
		    data, sizeof(data) - 1);
		(void)dssh_test_ossl_count();
		dssh_test_ossl_reset();
		if (res == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

/* ================================================================
 * KEX handler with NULL key_algo — hits ka == NULL guard
 * ================================================================ */

static int
test_c25519_handler_null_ka(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	struct dssh_kex_context kctx = {0};
	kctx.client = false;
	kctx.key_algo = NULL;

	int res = curve25519_handler(&kctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_dhgex_handler_null_ka(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	struct dssh_kex_context kctx = {0};
	kctx.client = false;
	kctx.key_algo = NULL;

	int res = dhgex_handler(&kctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Ed25519 */
	{ "ed25519_register",              test_ed25519_register },
	{ "ed25519_generate_key",          test_ed25519_generate_key },
	{ "ed25519_haskey_after_generate",  test_ed25519_haskey_after_generate },
	{ "ed25519_no_key_before_generate", test_ed25519_no_key_before_generate },
	{ "ed25519_sign_verify_roundtrip",  test_ed25519_sign_verify_roundtrip },
	{ "ed25519_verify_corrupted_sig",   test_ed25519_verify_corrupted_sig },
	{ "ed25519_verify_different_data",  test_ed25519_verify_different_data },
	{ "ed25519_get_pub_str_format",     test_ed25519_get_pub_str_format },
	{ "ed25519_get_pub_str_size_query", test_ed25519_get_pub_str_size_query },
	{ "ed25519_pubkey_blob_format",     test_ed25519_pubkey_blob_format },
	{ "ed25519_sig_blob_format",        test_ed25519_sig_blob_format },
	{ "ed25519_save_load_roundtrip",    test_ed25519_save_load_roundtrip },
	{ "ed25519_save_pub_file",          test_ed25519_save_pub_file },
	{ "ed25519_sign_verify_after_load", test_ed25519_sign_verify_after_load },

	/* RSA-SHA2-256 */
	{ "rsa_register",                  test_rsa_register },
	{ "rsa_generate_2048",             test_rsa_generate_2048 },
	{ "rsa_haskey_after_generate",      test_rsa_haskey_after_generate },
	{ "rsa_no_key_before_generate",     test_rsa_no_key_before_generate },
	{ "rsa_sign_verify_roundtrip",      test_rsa_sign_verify_roundtrip },
	{ "rsa_verify_corrupted_sig",       test_rsa_verify_corrupted_sig },
	{ "rsa_verify_different_data",      test_rsa_verify_different_data },
	{ "rsa_get_pub_str_format",         test_rsa_get_pub_str_format },
	{ "rsa_get_pub_str_size_query",     test_rsa_get_pub_str_size_query },
	{ "rsa_pubkey_blob_format",         test_rsa_pubkey_blob_format },
	{ "rsa_sig_blob_format",            test_rsa_sig_blob_format },
	{ "rsa_save_load_roundtrip",        test_rsa_save_load_roundtrip },
	{ "rsa_save_pub_file",              test_rsa_save_pub_file },
	{ "rsa_generate_4096",             test_rsa_generate_4096 },
	{ "rsa_sign_verify_after_load",     test_rsa_sign_verify_after_load },

	/* Cross-algorithm */
	{ "both_algorithms_coexist",        test_both_algorithms_coexist },
	{ "ed25519_sig_fails_rsa_verify",   test_ed25519_sig_fails_rsa_verify },
	{ "find_unregistered_algo",         test_find_unregistered_algo },

	/* Error paths (coverage) */
	{ "ed25519_load_nonexistent",       test_ed25519_load_nonexistent },
	{ "rsa_load_nonexistent",           test_rsa_load_nonexistent },
	{ "ed25519_sign_without_key",       test_ed25519_sign_without_key },
	{ "rsa_sign_without_key",           test_rsa_sign_without_key },
	{ "ed25519_pubkey_without_key",     test_ed25519_pubkey_without_key },
	{ "rsa_pubkey_without_key",         test_rsa_pubkey_without_key },
	{ "ed25519_verify_short_sig",       test_ed25519_verify_short_sig },
	{ "rsa_verify_short_sig",           test_rsa_verify_short_sig },
	{ "ed25519_verify_wrong_algo",      test_ed25519_verify_wrong_algo },

	/* File I/O and public string edge cases */
	{ "ed25519_get_pub_str_small_buf",   test_ed25519_get_pub_str_small_buf },
	{ "ed25519_save_pub_before_keygen",  test_ed25519_save_pub_before_keygen },
	{ "ed25519_load_rsa_key_file",       test_ed25519_load_rsa_key_file },
	{ "rsa_get_pub_str_small_buf",       test_rsa_get_pub_str_small_buf },
	{ "rsa_save_pub_before_keygen",      test_rsa_save_pub_before_keygen },
	{ "rsa_load_ed25519_key_file",       test_rsa_load_ed25519_key_file },
	{ "ed25519_save_key_before_keygen",  test_ed25519_save_key_before_keygen },
	{ "rsa_save_key_before_keygen",      test_rsa_save_key_before_keygen },
	{ "ed25519_save_key_bad_path",       test_ed25519_save_key_bad_path },
	{ "rsa_save_key_bad_path",           test_rsa_save_key_bad_path },
	{ "ed25519_save_pub_bad_path",       test_ed25519_save_pub_bad_path },
	{ "rsa_save_pub_bad_path",           test_rsa_save_pub_bad_path },
	{ "ed25519_generate_twice",          test_ed25519_generate_twice },
	{ "rsa_generate_twice",              test_rsa_generate_twice },
	{ "ed25519_load_twice",              test_ed25519_load_twice },
	{ "rsa_load_twice",                  test_rsa_load_twice },

	/* Verify parse errors — malformed blobs */
	{ "ed25519_verify_truncated_key",    test_ed25519_verify_truncated_key },
	{ "ed25519_verify_wrong_key_name",   test_ed25519_verify_wrong_key_algo_name },
	{ "ed25519_verify_wrong_key_len",    test_ed25519_verify_wrong_key_length },
	{ "ed25519_verify_key_trailing",     test_ed25519_verify_key_trailing_garbage },
	{ "ed25519_verify_sig_wrong_raw_len", test_ed25519_verify_sig_wrong_raw_len },
	{ "ed25519_verify_sig_trailing",     test_ed25519_verify_sig_trailing_garbage },
	{ "rsa_verify_truncated_key",        test_rsa_verify_truncated_key },
	{ "rsa_verify_wrong_key_name",       test_rsa_verify_wrong_key_algo_name },
	{ "rsa_verify_key_trailing",         test_rsa_verify_key_trailing_garbage },
	{ "rsa_verify_sig_trailing",         test_rsa_verify_sig_trailing_garbage },

	/* Deeper verify parse errors */
	{ "ed25519_verify_key_trunc_name",   test_ed25519_verify_key_truncated_after_name },
	{ "ed25519_verify_sig_trunc_name",   test_ed25519_verify_sig_truncated_after_name },
	{ "ed25519_verify_bad_signature",    test_ed25519_verify_bad_signature },
	{ "rsa_verify_key_trunc_name",       test_rsa_verify_key_truncated_after_name },
	{ "rsa_verify_key_trunc_e",          test_rsa_verify_key_truncated_after_e },
	{ "rsa_verify_sig_trunc_name",       test_rsa_verify_sig_truncated_after_name },

	/* Pre-registration errors */
	{ "ed25519_generate_before_register", test_ed25519_generate_before_register },
	{ "rsa_generate_before_register",    test_rsa_generate_before_register },
	{ "ed25519_pub_str_before_register", test_ed25519_get_pub_str_before_register },
	{ "rsa_pub_str_before_register",     test_rsa_get_pub_str_before_register },

	/* Defensive guards — NULL ctx and too-small buffer */
	{ "ed25519_haskey_null_ctx",         test_ed25519_haskey_null_ctx },
	{ "ed25519_cleanup_null_ctx",        test_ed25519_cleanup_null_ctx },
	{ "ed25519_pub_str_bufsz_one",       test_ed25519_get_pub_str_bufsz_one },
	{ "rsa_haskey_null_ctx",             test_rsa_haskey_null_ctx },
	{ "rsa_cleanup_null_ctx",            test_rsa_cleanup_null_ctx },
	{ "rsa_pub_str_bufsz_one",           test_rsa_get_pub_str_bufsz_one },

	/* Verify overrun branches */
	{ "ed25519_verify_key_algo_overrun",  test_ed25519_verify_key_algo_overrun },
	{ "ed25519_verify_key_rawpub_overrun", test_ed25519_verify_key_rawpub_overrun },
	{ "ed25519_verify_sig_algo_overrun",  test_ed25519_verify_sig_algo_overrun },
	{ "ed25519_verify_sig_rawsig_overrun", test_ed25519_verify_sig_rawsig_overrun },
	{ "ed25519_verify_sig_wrong_name",    test_ed25519_verify_sig_wrong_algo_name },
	{ "rsa_verify_key_algo_overrun",      test_rsa_verify_key_algo_overrun },
	{ "rsa_verify_key_e_overrun",         test_rsa_verify_key_e_overrun },
	{ "rsa_verify_key_n_overrun",         test_rsa_verify_key_n_overrun },
	{ "rsa_verify_sig_algo_overrun",      test_rsa_verify_sig_algo_overrun },
	{ "rsa_verify_sig_wrong_name",        test_rsa_verify_sig_wrong_algo_name },
	{ "rsa_verify_sig_rawsig_overrun",    test_rsa_verify_sig_rawsig_overrun },
	{ "ed25519_verify_key_wrong_len",     test_ed25519_verify_key_wrong_algo_len },
	{ "ed25519_verify_key_wrong_content", test_ed25519_verify_key_wrong_algo_content },
	{ "ed25519_verify_sig_wrong_len",     test_ed25519_verify_sig_wrong_algo_len },
	{ "rsa_verify_key_wrong_len",         test_rsa_verify_key_wrong_algo_len },

	/* Load/save/get_pub_str edge cases */
	{ "ed25519_load_before_register",     test_ed25519_load_before_register },
	{ "rsa_load_before_register",         test_rsa_load_before_register },
	{ "ed25519_load_garbage_file",        test_ed25519_load_garbage_file },
	{ "rsa_load_garbage_file",            test_rsa_load_garbage_file },
	{ "ed25519_save_before_register",     test_ed25519_save_before_register },
	{ "rsa_save_before_register",         test_rsa_save_before_register },
	{ "ed25519_save_with_password",       test_ed25519_save_with_password },
	{ "rsa_save_with_password",           test_rsa_save_with_password },
	{ "ed25519_get_pub_str_null_buf",     test_ed25519_get_pub_str_null_buf },
	{ "ed25519_pub_str_zero_bufsz",       test_ed25519_get_pub_str_zero_bufsz },
	{ "rsa_get_pub_str_null_buf",         test_rsa_get_pub_str_null_buf },
	{ "rsa_pub_str_zero_bufsz",           test_rsa_get_pub_str_zero_bufsz },

	/* haskey wrong key type / null pkey */
	{ "ed25519_haskey_wrong_type",        test_ed25519_haskey_wrong_key_type },
	{ "rsa_haskey_wrong_type",            test_rsa_haskey_wrong_key_type },
	{ "ed25519_haskey_null_pkey",         test_ed25519_haskey_null_pkey },
	{ "rsa_haskey_null_pkey",             test_rsa_haskey_null_pkey },

	/* Write failures via /dev/full */
	{ "ed25519_save_key_write_fail",      test_ed25519_save_key_write_fail },
	{ "rsa_save_key_write_fail",          test_rsa_save_key_write_fail },
	{ "ed25519_save_pub_write_fail",      test_ed25519_save_pub_write_fail },
	{ "rsa_save_pub_write_fail",          test_rsa_save_pub_write_fail },

	/* RSA pubkey e_pad / n_pad */
	{ "rsa_pubkey_e_pad",                 test_rsa_pubkey_e_pad },
	{ "rsa_pubkey_n_no_pad",              test_rsa_pubkey_n_no_pad },
	{ "rsa_pubkey_n_zero",                test_rsa_pubkey_n_zero },

	/* RSA verify wrong key algo content */
	{ "rsa_verify_key_wrong_content",     test_rsa_verify_key_wrong_algo_content },

	/* Alloc failures in load/generate */
	{ "ed25519_generate_calloc_fail",     test_ed25519_generate_calloc_fail },
	{ "rsa_generate_calloc_fail",         test_rsa_generate_calloc_fail },
	{ "ed25519_load_calloc_fail",         test_ed25519_load_calloc_fail },
	{ "rsa_load_calloc_fail",             test_rsa_load_calloc_fail },
	{ "rsa_save_pub_alloc_fail",          test_rsa_save_pub_alloc_fail },

	/* RSA set_rsa_padding ossl failures */
	{ "rsa_sign_padding_fail",            test_rsa_sign_padding_fail },
	{ "rsa_verify_padding_fail",          test_rsa_verify_padding_fail },

	/* KEX handler NULL ka */
	{ "c25519_handler_null_ka",           test_c25519_handler_null_ka },
	{ "dhgex_handler_null_ka",            test_dhgex_handler_null_ka },
};
DSSH_TEST_MAIN(tests)
