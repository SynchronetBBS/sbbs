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
 * Error path tests (Category 5)
 * ================================================================ */

static int
test_ed25519_load_nonexistent(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);

	int res = ssh_ed25519_load_key_file("/nonexistent/path/key", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_load_nonexistent(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);

	int res = rsa_sha2_256_load_key_file("/nonexistent/path/key", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_sign_without_key(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	/* Query needed size */
	int64_t needed = ssh_ed25519_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);

	/* Pass buffer that's too small */
	char small[8];
	int64_t res = ssh_ed25519_get_pub_str(small, sizeof(small));
	ASSERT_EQ(res, (int64_t)DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_ed25519_save_pub_before_keygen(void)
{
	/* save_pub_file before generating key */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);

	int res = ssh_ed25519_save_pub_file("/tmp/dssh_no_key_pub");
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_load_rsa_key_file(void)
{
	/* Load an RSA key into Ed25519 loader — wrong key type */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_for_ed_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	/* Now try to load it as Ed25519 */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);

	int res = ssh_ed25519_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_get_pub_str_small_buf(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	int64_t needed = rsa_sha2_256_get_pub_str(NULL, 0);
	ASSERT_TRUE(needed > 0);

	char small[8];
	int64_t res = rsa_sha2_256_get_pub_str(small, sizeof(small));
	ASSERT_EQ(res, (int64_t)DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_rsa_save_pub_before_keygen(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);

	int res = rsa_sha2_256_save_pub_file("/tmp/dssh_no_key_pub");
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_load_ed25519_key_file(void)
{
	/* Load an Ed25519 key into RSA loader — wrong key type */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed_for_rsa_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(ssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);

	int res = rsa_sha2_256_load_key_file(tmppath, NULL, NULL);
	unlink(tmppath);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_save_key_before_keygen(void)
{
	/* save_key_file before generating/loading a key */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);

	int res = ssh_ed25519_save_key_file("/tmp/dssh_nokey", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_save_key_before_keygen(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);

	int res = rsa_sha2_256_save_key_file("/tmp/dssh_nokey", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_save_key_bad_path(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	int res = ssh_ed25519_save_key_file("/nonexistent/dir/key", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_save_key_bad_path(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	int res = rsa_sha2_256_save_key_file("/nonexistent/dir/key", NULL, NULL);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_save_pub_bad_path(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	int res = ssh_ed25519_save_pub_file("/nonexistent/dir/pub");
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_save_pub_bad_path(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	int res = rsa_sha2_256_save_pub_file("/nonexistent/dir/pub");
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_generate_twice(void)
{
	/* Generate key twice — covers the "replace existing" path */
	dssh_test_reset_global_config();
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = dssh_transport_find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);
	ASSERT_TRUE(ka->haskey(ka->ctx));
	return TEST_PASS;
}

static int
test_rsa_generate_twice(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);

	char tmppath[] = "/tmp/dssh_test_ed_load2_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(ssh_ed25519_save_key_file(tmppath, NULL, NULL), 0);

	/* Load it once */
	ASSERT_EQ(ssh_ed25519_load_key_file(tmppath, NULL, NULL), 0);
	/* Load it again — replaces existing */
	ASSERT_EQ(ssh_ed25519_load_key_file(tmppath, NULL, NULL), 0);
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);

	char tmppath[] = "/tmp/dssh_test_rsa_load2_XXXXXX";
	int fd = mkstemp(tmppath);
	ASSERT_TRUE(fd >= 0);
	close(fd);
	ASSERT_EQ(rsa_sha2_256_save_key_file(tmppath, NULL, NULL), 0);

	ASSERT_EQ(rsa_sha2_256_load_key_file(tmppath, NULL, NULL), 0);
	ASSERT_EQ(rsa_sha2_256_load_key_file(tmppath, NULL, NULL), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
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
	ASSERT_EQ(register_ssh_ed25519(), 0);
	ASSERT_EQ(ssh_ed25519_generate_key(), 0);
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);
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
	ASSERT_EQ(register_rsa_sha2_256(), 0);
	ASSERT_EQ(rsa_sha2_256_generate_key(2048), 0);
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
	int res = ssh_ed25519_generate_key();
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_generate_before_register(void)
{
	dssh_test_reset_global_config();
	int res = rsa_sha2_256_generate_key(2048);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_ed25519_get_pub_str_before_register(void)
{
	dssh_test_reset_global_config();
	int64_t res = ssh_ed25519_get_pub_str(NULL, 0);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_rsa_get_pub_str_before_register(void)
{
	dssh_test_reset_global_config();
	int64_t res = rsa_sha2_256_get_pub_str(NULL, 0);
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
};
DSSH_TEST_MAIN(tests)
