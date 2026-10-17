/* CTest fixture: publish cached keys before any parallel reader starts. */
#include "dssh_test.h"
#include "deucessh-algorithms.h"

static int
test_prepare_keys(void)
{
	const char *ed25519 = getenv("DSSH_TEST_ED25519_KEY");
	const char *rsa = getenv("DSSH_TEST_RSA_KEY");

	ASSERT_NOT_NULL(ed25519);
	ASSERT_NOT_NULL(rsa);
	ASSERT_OK(dssh_register_ssh_ed25519());
	ASSERT_OK(dssh_register_rsa_sha2_256());
	FILE *fp = fopen(ed25519, "r");

	if (fp != NULL) {
		ASSERT_EQ(fclose(fp), 0);
		ASSERT_OK(dssh_ed25519_load_key_file(ed25519, NULL, NULL));
	}
	else {
		ASSERT_OK(dssh_ed25519_generate_key());
		ASSERT_OK(dssh_ed25519_save_key_file(ed25519, NULL, NULL));
	}
	fp = fopen(rsa, "r");
	if (fp != NULL) {
		ASSERT_EQ(fclose(fp), 0);
		ASSERT_OK(dssh_rsa_sha2_256_load_key_file(rsa, NULL, NULL));
	}
	else {
		ASSERT_OK(dssh_rsa_sha2_256_generate_key(2048));
		ASSERT_OK(dssh_rsa_sha2_256_save_key_file(rsa, NULL, NULL));
	}
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "prepare_keys", test_prepare_keys },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
