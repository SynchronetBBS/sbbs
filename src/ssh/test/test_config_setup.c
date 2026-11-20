/* Accepted setup contract A: permanent freeze and legal constructor retries. */
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "deucessh-algorithms.h"

static int
unused_io(uint8_t *buf, size_t length, dssh_session session, void *data)
{
	(void)buf;
	(void)length;
	(void)session;
	(void)data;
	return 0;
}

static bool
late_calls(unsigned int *rejected)
{
	int results[] = {
		dssh_transport_set_version("Later", NULL),
		dssh_transport_set_callbacks(unused_io, unused_io, NULL, NULL),
		dssh_transport_set_tx_gather(NULL),
		dssh_key_algo_set_ctx("fixture", NULL),
		dssh_kex_set_ctx("fixture", NULL),
		dssh_register_curve25519_sha256(),
		dssh_register_ssh_ed25519(),
		dssh_register_none_enc(),
		dssh_register_none_mac(),
		dssh_register_none_comp(),
		dssh_register_aes128_cbc(),
		dssh_ed25519_generate_key(),
		dssh_rsa_sha2_256_generate_key(2048),
		dssh_rsa_sha2_512_generate_key(2048),
		dssh_ed25519_load_key_file("/dssh-setup-no-file", NULL, NULL),
		dssh_rsa_sha2_256_load_key_file("/dssh-setup-no-file", NULL, NULL),
		dssh_rsa_sha2_512_load_key_file("/dssh-setup-no-file", NULL, NULL),
	};
	bool valid = true;
	for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
		bool one = results[i] == DSSH_ERROR_TOOLATE;
		*rejected += one ? 1U : 0U;
		valid = one && valid;
	}
	return valid;
}

static void
reset_controls(void)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_clock_reset();
}

static bool
failure_case(unsigned int kind, int point, unsigned int *rejected)
{
	reset_controls();
	dssh_test_reset_global_config();
	if (dssh_transport_set_version("SetupFreeze", NULL) != 0)
		return false;
	if (kind == 0)
		dssh_test_alloc_fail_after(point);
	else if (kind == 1)
		dssh_test_ossl_fail_after(point);
	else
		dssh_test_clock_fail_after(point);
	dssh_session session = dssh_session_init(true, 0);
	int count = kind == 0 ? dssh_test_alloc_count()
	    : kind == 1 ? dssh_test_ossl_count() : dssh_test_clock_count();
	reset_controls();
	bool valid = session == NULL && count == point + 1;
	if (session != NULL)
		dssh_session_cleanup(session);
	valid = late_calls(rejected) && valid;
	/* A failed accepted attempt keeps setup frozen, but session construction
	 * itself can be retried once the external/resource failure is removed. */
	session = dssh_session_init(false, 0);
	valid = session != NULL && valid;
	dssh_session_cleanup(session);
	valid = late_calls(rejected) && valid;
	dssh_test_reset_global_config();
	return valid;
}

static int
test_allocation_failures(void)
{
	bool valid = true;
	unsigned int rejected = 0;
	for (int point = 0; point < 3; point++)
		valid = failure_case(0, point, &rejected) && valid;
	printf("setup allocation freeze: cases=3 rejected=%u retries=3 valid=%u\n", rejected, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 102);
	return TEST_PASS;
}

static int
test_native_failures(void)
{
	bool valid = true;
	unsigned int rejected = 0;
	for (int point = 0; point < 5; point++)
		valid = failure_case(1, point, &rejected) && valid;
	printf("setup native freeze: cases=5 rejected=%u retries=5 valid=%u\n", rejected, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 170);
	return TEST_PASS;
}

static int
test_clock_failure(void)
{
	unsigned int rejected = 0;
	bool valid = failure_case(2, 0, &rejected);
	printf("setup clock freeze: cases=1 rejected=%u retries=1 valid=%u\n", rejected, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 34);
	return TEST_PASS;
}

static int
test_success(void)
{
	reset_controls();
	dssh_test_reset_global_config();
	dssh_session session = dssh_session_init(true, 0);
	unsigned int rejected = 0;
	bool valid = session != NULL && late_calls(&rejected);
	dssh_session_cleanup(session);
	valid = late_calls(&rejected) && valid;
	dssh_test_reset_global_config();
	printf("setup successful freeze: rejected=%u cleanup-thaw=0 valid=%u\n", rejected, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 34);
	return TEST_PASS;
}

struct rejected_entry {
	dssh_session session;
	bool called;
};

static void
release_attempt(void *opaque)
{
	struct rejected_entry *entry = opaque;
	entry->called = true;
	entry->session = dssh_session_init(true, 0);
}

static int
test_callback_entry(void)
{
	reset_controls();
	dssh_test_reset_global_config();
	struct rejected_entry entry = {0};
	struct dssh_result_lease lease = {.release = release_attempt, .owner = &entry};
	dssh_test_alloc_fail_after(0);
	dssh_result_release(&lease);
	int count = dssh_test_alloc_count();
	reset_controls();
	bool valid = entry.called && entry.session == NULL && count == 0
	    && lease.release == NULL && dssh_transport_set_version("StillMutable", NULL) == 0;
	dssh_session_cleanup(entry.session);
	dssh_test_reset_global_config();
	printf("setup rejected entry: callback=1 allocated=%d frozen=0 valid=%u\n", count, valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{"allocation_failures", test_allocation_failures},
	{"native_failures", test_native_failures},
	{"clock_failure", test_clock_failure},
	{"success", test_success},
	{"callback_entry", test_callback_entry},
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
