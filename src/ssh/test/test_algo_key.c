/*
 * test_algo_key.c — Tests for Ed25519 and RSA-SHA2-256 host key algorithm
 * modules in DeuceSSH.
 *
 * Tests key generation, sign/verify, public key export, and file I/O.
 */

#include <unistd.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	return TEST_PASS;
}

static int
test_ed25519_generate_key(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
	return TEST_PASS;
}

static int
test_ed25519_haskey_after_generate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_ed25519_no_key_before_generate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ASSERT_FALSE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_ed25519_sign_verify_roundtrip(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	char buf[256];
	int64_t len = ssh_ed25519_get_pub_str(buf, sizeof(buf));
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	/* NULL buf returns needed size */
	int64_t needed = ssh_ed25519_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);

	char *buf = malloc((size_t)needed);
	ASSERT_NOT_NULL(buf);
	int64_t len = ssh_ed25519_get_pub_str(buf, (size_t)needed);
	ASSERT_EQ(len, needed);
	free(buf);
	return TEST_PASS;
}

static int
test_ed25519_pubkey_blob_format(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	/* Get the original public key */
	char orig_pub[256];
	int64_t orig_len = ssh_ed25519_get_pub_str(orig_pub, sizeof(orig_pub));
	ASSERT_TRUE(orig_len > 0);

	/* Save to temp file */
	char tmppath[] = "/tmp/dssh_test_ed25519_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(ssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	/* Reset and re-register, then load */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	/* Verify the loaded key produces the same public key */
	char loaded_pub[256];
	int64_t loaded_len = ssh_ed25519_get_pub_str(loaded_pub, sizeof(loaded_pub));
	ASSERT_EQ(loaded_len, orig_len);
	ASSERT_MEM_EQ(orig_pub, loaded_pub, (size_t)orig_len);
	return TEST_PASS;
}

static int
test_ed25519_save_pub_file(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed25519_pub_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(ssh_ed25519_save_pub_file(tmppath), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed25519_sv_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(ssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	/* Reload into fresh registry */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_load_key_file(tmppath, NULL, NULL), 0);
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	return TEST_PASS;
}

static int
test_rsa_generate_2048(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);
	return TEST_PASS;
}

static int
test_rsa_haskey_after_generate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_rsa_no_key_before_generate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);
	ASSERT_FALSE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_rsa_sign_verify_roundtrip(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	char buf[2048];
	int64_t len = rsa_sha2_256_get_pub_str(buf, sizeof(buf));
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	int64_t needed = rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);

	char *buf = malloc((size_t)needed);
	ASSERT_NOT_NULL(buf);
	int64_t len = rsa_sha2_256_get_pub_str(buf, (size_t)needed);
	ASSERT_EQ(len, needed);
	free(buf);
	return TEST_PASS;
}

static int
test_rsa_pubkey_blob_format(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	/* Get the original public key string */
	int64_t needed = rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);
	char *orig_pub = malloc((size_t)needed);
	ASSERT_NOT_NULL(orig_pub);
	ASSERT_EQ(rsa_sha2_256_get_pub_str(orig_pub, (size_t)needed), needed);

	char tmppath[] = "/tmp/dssh_test_rsa_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	/* Reset and re-register, then load */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_load_key_file(tmppath, NULL, NULL), 0);
	unlink(tmppath);

	/* Verify the loaded key produces the same public key */
	int64_t loaded_needed = rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_EQ(loaded_needed, needed);
	char *loaded_pub = malloc((size_t)loaded_needed);
	ASSERT_NOT_NULL(loaded_pub);
	ASSERT_EQ(rsa_sha2_256_get_pub_str(loaded_pub, (size_t)loaded_needed),
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_pub_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(rsa_sha2_256_save_pub_file(tmppath), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(4096), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_sv_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);

	ASSERT_EQ(rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	/* Reload into fresh registry */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_load_key_file(tmppath, NULL, NULL), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("nonexistent-algo");
	ASSERT_NULL(ka);
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
};
DSSH_TEST_MAIN(tests)
