/*
 * test_alloc.c -- Allocation failure tests for DeuceSSH.
 *
 * Uses the mock allocator (--wrap=malloc/calloc/realloc/free) to
 * inject NULL returns at every allocation point in the library.
 * For each function under test, we iterate N from 0 upward, failing
 * the Nth allocation. A sweep completes only after an observed fault and a
 * successful operation that did not reach its target. Some older threaded
 * controllers below still await this completion/ownership migration (R081).
 */

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "kex/dh-gex-sha256.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "mock_io.h"
#include "mock_alloc.h"
#include "dssh_test_ossl.h"
#include "dssh_test_alloc.h"
#include "test_dhgex_provider.h"
#include "../kex/sntrup761.h"

#include "deucessh-crypto.h"


/* ================================================================
 * Helpers
 * ================================================================ */

/*
 * Register all algorithms.  Must be called with the mock allocator
 * disabled (mock_alloc_reset) since registration itself allocates.
 */
#include "auth_result_fixture.h"

#include "signature_copy_fixture.h"
static int
register_all(void)
{
	int res;
	res = test_register_kex();
	if (res < 0) return res;
	res = test_register_key_algos();
	if (res < 0) return res;
	res = dssh_register_aes256_ctr();
	if (res < 0) return res;
	res = test_register_mac_algos();
	if (res < 0) return res;
	res = dssh_register_none_comp();
	if (res < 0) return res;
	return 0;
}

/* A successful operation may tolerate the selected fault. It is still a
 * fault case, not the final control. Never infer completion from a repeated
 * count, an uninjected rejection, a skipped target or invalid output. */
struct allocation_sweep {
	int next;
	unsigned faults;
	unsigned tolerated;
};

static int
allocation_sweep_step(struct allocation_sweep *sweep, const char *name,
    int point, int calls, bool completed, bool valid)
{
	if (point < 0 || point != sweep->next || calls < 0 || !valid)
		return -1;
	bool injected = calls > point;
	if (!completed && !injected)
		return -1;
	if (injected) {
		if (sweep->faults == UINT_MAX || (completed && sweep->tolerated == UINT_MAX))
			return -1;
		sweep->faults++;
		if (completed)
			sweep->tolerated++;
	}
	if (point == INT_MAX)
		return -1;
	sweep->next++;
	if (printf("allocation_sweep name=%s key=%s point=%d calls=%d injected=%d completed=%d valid=1\n",
	        name, test_key_algo_name(), point, calls, injected, completed) < 0)
		return -1;
	if (!injected && completed) {
		if (sweep->faults == 0)
			return -1;
		if (printf("allocation_complete name=%s key=%s cases=%d faults=%u tolerated=%u\n",
		        name, test_key_algo_name(), sweep->next, sweep->faults, sweep->tolerated) < 0)
			return -1;
		return 1;
	}
	return 0;
}

/* Deliberate observer controls: tolerated faults and repeated fault counts
 * must continue; an unreached failure and skipped target must fail. */
static int
test_alloc_sweep_contract(void)
{
	struct allocation_sweep sweep = {0};
	ASSERT_EQ(allocation_sweep_step(&sweep, "contract", 0, 2, true, true), 0);
	ASSERT_EQ(allocation_sweep_step(&sweep, "contract", 1, 2, false, true), 0);
	ASSERT_EQ(allocation_sweep_step(&sweep, "contract", 2, 2, true, true), 1);
	ASSERT_EQ(sweep.faults, 2);
	ASSERT_EQ(sweep.tolerated, 1);
	sweep = (struct allocation_sweep){0};
	ASSERT_EQ(allocation_sweep_step(&sweep, "contract", 0, 0, false, true), -1);
	ASSERT_EQ(allocation_sweep_step(&sweep, "contract", 1, 2, false, true), -1);
	ASSERT_EQ(allocation_sweep_step(&sweep, "contract", 0, 1, true, false), -1);
	ASSERT_EQ(allocation_sweep_step(&sweep, "contract", 0, -1, false, true), -1);
	ASSERT_EQ(allocation_sweep_step(&sweep, "contract", 0, 0, true, true), -1);
	return TEST_PASS;
}

/* All builtin constructors: exact publication, each actual allocation fault,
 * clean rejection and a final uninjected publication on a fresh registry. */
enum allocation_registry_kind { REG_KEX, REG_KEY, REG_ENC, REG_MAC, REG_COMP };

static bool
allocation_registry_empty(void)
{
	return gconf.kex_head == NULL && gconf.kex_tail == NULL && gconf.kex_entries == 0
	    && gconf.key_algo_head == NULL && gconf.key_algo_tail == NULL && gconf.key_algo_entries == 0
	    && gconf.enc_head == NULL && gconf.enc_tail == NULL && gconf.enc_entries == 0
	    && gconf.mac_head == NULL && gconf.mac_tail == NULL && gconf.mac_entries == 0
	    && gconf.comp_head == NULL && gconf.comp_tail == NULL && gconf.comp_entries == 0
	    && !atomic_load(&gconf.used);
}

static bool
allocation_registry_single(enum allocation_registry_kind kind, const char *name)
{
	if (gconf.kex_entries != (kind == REG_KEX ? 1U : 0U)
	    || gconf.key_algo_entries != (kind == REG_KEY ? 1U : 0U)
	    || gconf.enc_entries != (kind == REG_ENC ? 1U : 0U)
	    || gconf.mac_entries != (kind == REG_MAC ? 1U : 0U)
	    || gconf.comp_entries != (kind == REG_COMP ? 1U : 0U) || atomic_load(&gconf.used))
		return false;
	bool valid = true;
#define REGISTRY_NODE(field, expected_kind) \
	do { \
		if (kind == expected_kind) \
			valid = valid && gconf.field##_head != NULL && gconf.field##_head == gconf.field##_tail \
			    && gconf.field##_head->next == NULL && strcmp(gconf.field##_head->name, name) == 0; \
		else \
			valid = valid && gconf.field##_head == NULL && gconf.field##_tail == NULL; \
	} while (0)
	REGISTRY_NODE(kex, REG_KEX);
	REGISTRY_NODE(key_algo, REG_KEY);
	REGISTRY_NODE(enc, REG_ENC);
	REGISTRY_NODE(mac, REG_MAC);
	REGISTRY_NODE(comp, REG_COMP);
#undef REGISTRY_NODE
	return valid;
}

static int
allocation_registration_sweep(int (*registration)(void), enum allocation_registry_kind kind,
    const char *wire_name, const char *test_name)
{
	mock_alloc_reset();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	int ret = registration();
	bool valid = ret == 0 && allocation_registry_single(kind, wire_name);
	dssh_test_reset_global_config();
	valid = valid && allocation_registry_empty();
	if (!valid)
		return TEST_FAIL;
	struct allocation_sweep sweep = {0};
	for (int point = 0; point < 100; point++) {
		mock_alloc_fail_after(point);
		ret = registration();
		int calls = mock_alloc_count();
		mock_alloc_reset();
		bool completed = ret == 0;
		valid = completed ? allocation_registry_single(kind, wire_name)
		    : ret == DSSH_ERROR_ALLOC && allocation_registry_empty();
		dssh_test_reset_global_config();
		valid = allocation_registry_empty() && valid;
		if (printf("allocation_registration name=%s wire=%s point=%d result=%d calls=%d retired=%u valid=%u\n",
		        test_name, wire_name, point, ret, calls, allocation_registry_empty() ? 1U : 0U,
		        valid ? 1U : 0U) < 0)
			return TEST_FAIL;
		int step = allocation_sweep_step(&sweep, test_name, point, calls, completed, valid);
		if (step != 0)
			return step > 0 ? TEST_PASS : TEST_FAIL;
	}
	return TEST_FAIL;
}

#define REGISTRATION_TEST(suffix, kind, wire_name) \
	static int test_alloc_register_##suffix(void) \
	{ \
		return allocation_registration_sweep(dssh_register_##suffix, kind, wire_name, "alloc/register_" #suffix); \
	}
REGISTRATION_TEST(curve25519_sha256, REG_KEX, "curve25519-sha256")
REGISTRATION_TEST(dh_gex_sha256, REG_KEX, "diffie-hellman-group-exchange-sha256")
REGISTRATION_TEST(dh_group14_sha256, REG_KEX, "diffie-hellman-group14-sha256")
REGISTRATION_TEST(dh_group16_sha512, REG_KEX, "diffie-hellman-group16-sha512")
REGISTRATION_TEST(sntrup761x25519_sha512, REG_KEX, "sntrup761x25519-sha512")
REGISTRATION_TEST(mlkem768x25519_sha256, REG_KEX, "mlkem768x25519-sha256")
REGISTRATION_TEST(ssh_ed25519, REG_KEY, "ssh-ed25519")
REGISTRATION_TEST(rsa_sha2_256, REG_KEY, "rsa-sha2-256")
REGISTRATION_TEST(rsa_sha2_512, REG_KEY, "rsa-sha2-512")
REGISTRATION_TEST(aes256_ctr, REG_ENC, "aes256-ctr")
REGISTRATION_TEST(aes128_cbc, REG_ENC, "aes128-cbc")
REGISTRATION_TEST(none_enc, REG_ENC, "none")
REGISTRATION_TEST(hmac_sha2_256, REG_MAC, "hmac-sha2-256")
REGISTRATION_TEST(hmac_sha2_512, REG_MAC, "hmac-sha2-512")
REGISTRATION_TEST(none_mac, REG_MAC, "none")
REGISTRATION_TEST(none_comp, REG_COMP, "none")
#undef REGISTRATION_TEST

/* ================================================================
 * dssh_session_init alloc failures
 *
 * session_init does one calloc for the session struct, then two
 * mallocs for tx_packet and rx_packet (each with 4-byte seq prefix).
 * Failing any of these must return NULL.
 * ================================================================ */

static int
test_alloc_session_init(void)
{
	dssh_test_reset_global_config();
	mock_alloc_reset();
	dssh_test_ossl_reset();
	int setup = register_all();
	if (setup < 0) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	/* Require a valid normal constructor before arming a fault. */
	dssh_session baseline = dssh_session_init(true, 0);
	if (baseline == NULL) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_cleanup(baseline);
	struct allocation_sweep sweep = {0};
	for (int n = 0; n < 100; n++) {
		mock_alloc_fail_after(n);
		dssh_session sess = dssh_session_init(true, 0);
		int calls = mock_alloc_count();
		mock_alloc_reset();
		bool completed = sess != NULL;
		dssh_session_cleanup(sess);
		int step = allocation_sweep_step(&sweep, "alloc/session_init", n, calls, completed, true);
		if (step != 0) {
			dssh_test_reset_global_config();
			return step > 0 ? TEST_PASS : TEST_FAIL;
		}
	}
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

/* ================================================================
 * dssh_session_init C11 thread failures
 *
 * session_init calls mtx_init() after the calloc succeeds.  Failing
 * it must return NULL without leaking the session struct.
 * ================================================================ */


#ifdef DSSH_CRYPTO_OPENSSL
static int
test_ossl_session_init(void)
{
	dssh_test_reset_global_config();
	mock_alloc_reset();
	dssh_test_ossl_reset();
	int setup = register_all();
	if (setup < 0) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	/* Require a valid normal constructor before arming a fault. */
	dssh_session baseline = dssh_session_init(true, 0);
	if (baseline == NULL) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_cleanup(baseline);
	struct allocation_sweep sweep = {0};
	for (int n = 0; n < 100; n++) {
		dssh_test_ossl_fail_after(n);
		dssh_session sess = dssh_session_init(true, 0);
		int calls = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		bool completed = sess != NULL;
		dssh_session_cleanup(sess);
		int step = allocation_sweep_step(&sweep, "ossl/session_init", n, calls, completed, true);
		if (step != 0) {
			dssh_test_reset_global_config();
			return step > 0 ? TEST_PASS : TEST_FAIL;
		}
	}
	dssh_test_reset_global_config();
	return TEST_FAIL;
}
#endif /* DSSH_CRYPTO_OPENSSL */

/* ================================================================
 * Auth message building alloc failures (ssh-auth.c)
 *
 * Each client auth function builds one or more malloc'd messages.
 * We test dssh_auth_get_methods, dssh_auth_password, and
 * dssh_auth_publickey by failing their internal mallocs.
 *
 * These need a completed handshake, so we set up the handshake
 * with the allocator disabled, then arm it for the auth call.
 * ================================================================ */

static _Thread_local bool allocation_reject_io;
static _Thread_local unsigned allocation_argument_io_calls;

static bool
allocation_io_refused(void)
{
	if (!allocation_reject_io)
		return false;
	if (allocation_argument_io_calls < UINT_MAX)
		allocation_argument_io_calls++;
	return true;
}

/* Dispatch callbacks */
static int
mock_tx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	if (allocation_io_refused())
		return DSSH_ERROR_INIT;
	if (sess->trans.client)
		return mock_tx_client(buf, bufsz, sess, cbdata);
	else
		return mock_tx_server(buf, bufsz, sess, cbdata);
}

static int
mock_rx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	if (allocation_io_refused())
		return DSSH_ERROR_INIT;
	if (sess->trans.client)
		return mock_rx_client(buf, bufsz, sess, cbdata);
	else
		return mock_rx_server(buf, bufsz, sess, cbdata);
}

static int
mock_rxline_dispatch(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata)
{
	if (allocation_io_refused()) {
		*bytes_received = 0;
		return DSSH_ERROR_INIT;
	}
	if (sess->trans.client)
		return mock_rxline_client(buf, bufsz, bytes_received, sess, cbdata);
	else
		return mock_rxline_server(buf, bufsz, bytes_received, sess, cbdata);
}

#include "allocation_pair_fixture.h"

static void
allocation_cleanup(struct allocation_pair *owner)
{
	mock_alloc_reset();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	allocation_pair_destroy(owner);
	dssh_test_reset_global_config();
}

static int
allocation_prepare(struct allocation_pair *owner, bool synchronization)
{
	allocation_pair_begin(owner);
	mock_alloc_reset();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	int ret = register_all();
	if (ret == 0)
		ret = test_generate_host_key();
	if (ret == 0)
		ret = dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    mock_rxline_dispatch, mock_extra_line_cb);
	if (ret == 0)
		ret = test_pair_open(&owner->pair);
	if (ret == 0 && synchronization)
		ret = allocation_pair_sync_init(owner);
	if (ret < 0)
		allocation_cleanup(owner);
	return ret;
}

void
dssh_test_failure_cleanup(void)
{
	/* All actors in this executable now have registered, live-frame owners. */
	mock_alloc_reset();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	while (allocation_pair_active != NULL)
		allocation_pair_destroy(allocation_pair_active);
	test_pair_destroy_active();
	dssh_test_reset_global_config();
}

/* ================================================================
 * Connection protocol: accept queue capacity limit (ssh-conn.c)
 *
 * acceptqueue_push uses a fixed-capacity ring buffer.  Pushing
 * beyond DSSH_ACCEPT_QUEUE_CAP must return DSSH_ERROR_TOOMANY.
 * ================================================================ */

static int
test_alloc_acceptqueue_push(void)
{
	struct dssh_accept_queue queue;
	static const uint8_t type[] = "session";
	acceptqueue_init(&queue);
	bool valid = true;
	for (uint32_t number = 0; number < DSSH_ACCEPT_QUEUE_CAP; number++) {
		if (acceptqueue_push(&queue, number, 0x200000, 0x8000, type, sizeof(type) - 1) != 0) {
			valid = false;
			break;
		}
	}
	if (valid)
		valid = queue.count == DSSH_ACCEPT_QUEUE_CAP
		    && acceptqueue_push(&queue, 99, 0x200000, 0x8000, type, sizeof(type) - 1) == DSSH_ERROR_TOOMANY
		    && queue.count == DSSH_ACCEPT_QUEUE_CAP;
	struct dssh_incoming_open entry;
	if (valid)
		valid = acceptqueue_pop(&queue, &entry) == 0 && entry.protocol_id == NULL
		    && entry.peer_channel == 0 && entry.peer_window == 0x200000 && entry.peer_max_packet == 0x8000
		    && entry.channel_type_len == sizeof(type) - 1
		    && memcmp(entry.channel_type, type, sizeof(type) - 1) == 0;
	if (valid)
		valid = acceptqueue_push(&queue, 100, 0x200000, 0x8000, type, sizeof(type) - 1) == 0
		    && queue.count == DSSH_ACCEPT_QUEUE_CAP;
	acceptqueue_free(&queue);
	valid = valid && queue.head == 0 && queue.count == 0;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_kbi_null_callback(void)
{
	struct allocation_pair owner;
	int ret = allocation_prepare(&owner, false);
	bool valid = ret == 0;
	allocation_argument_io_calls = 0;
	int result = DSSH_ERROR_INIT, calls = -1;
	if (valid) {
		allocation_reject_io = true;
		dssh_test_alloc_fail_after(0);
		result = dssh_auth_keyboard_interactive(owner.pair.client, "user", NULL, NULL);
		calls = dssh_test_alloc_count();
		allocation_reject_io = false;
		dssh_test_alloc_reset();
		valid = result == DSSH_ERROR_INVALID && calls == 0 && allocation_argument_io_calls == 0
		    && owner.pair.client->trans.tx_seq == 0 && owner.pair.client->trans.rx_seq == 0
		    && !owner.pair.client->auth_service_requested && !owner.pair.client->auth_complete
		    && !dssh_session_is_terminated(owner.pair.client);
	}
	allocation_cleanup(&owner);
	valid = valid && allocation_pair_retired(&owner);
	if (printf("allocation_kbi_argument result=%d calls=%d io=%u retired=%u valid=%u\n", result,
	    calls, allocation_argument_io_calls, allocation_pair_retired(&owner) ? 1U : 0U, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
allocation_version_only(dssh_session session)
{
	return version_exchange(session);
}

static int
test_alloc_kexinit(void)
{
	struct allocation_pair owner;
	int ret = allocation_prepare(&owner, false);
	if (ret == 0)
		ret = test_pair_run(&owner.pair, allocation_version_only);
	int result = DSSH_ERROR_INIT, calls = -1;
	bool valid = ret == 0;
	if (valid) {
		uint32_t tx = owner.pair.client->trans.tx_seq;
		uint32_t rx = owner.pair.client->trans.rx_seq;
		allocation_argument_io_calls = 0;
		allocation_reject_io = true;
		dssh_test_alloc_fail_after(0);
		result = kexinit(owner.pair.client);
		calls = dssh_test_alloc_count();
		allocation_reject_io = false;
		dssh_test_alloc_reset();
		valid = result == DSSH_ERROR_ALLOC && calls == 1 && allocation_argument_io_calls == 0
		    && owner.pair.client->trans.our_kexinit == NULL && owner.pair.client->trans.our_kexinit_sz == 0
		    && owner.pair.client->trans.tx_seq == tx && owner.pair.client->trans.rx_seq == rx
		    && !dssh_session_is_terminated(owner.pair.client);
		/* No bytes were sent. Recover on these same owned sessions/streams. */
		ret = valid ? test_pair_run(&owner.pair, kexinit) : DSSH_ERROR_INVALID;
		valid = valid && ret == 0 && owner.pair.client->trans.kex_selected != NULL
		    && owner.pair.server->trans.kex_selected != NULL
		    && strcmp(owner.pair.client->trans.kex_selected->name, test_kex_algo_name()) == 0
		    && strcmp(owner.pair.server->trans.kex_selected->name, test_kex_algo_name()) == 0;
	}
	allocation_cleanup(&owner);
	allocation_cleanup(&owner);
	valid = valid && allocation_pair_retired(&owner) && owner.pair.created == 4 && owner.pair.joined == 4;
	if (printf("allocation_kexinit result=%d calls=%d io=%u recovery=%d created=%u joined=%u retired=%u valid=%u\n",
	    result, calls, allocation_argument_io_calls, ret, owner.pair.created, owner.pair.joined,
	    allocation_pair_retired(&owner) ? 1U : 0U, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* ================================================================
 * Iterative handshake alloc failures using the library-only
 * allocator (dssh_test_alloc).
 *
 * Unlike --wrap=malloc, the macro-based allocator only intercepts
 * mallocs in library code (ssh-trans.c, ssh-auth.c, etc.), not
 * OpenSSL's internal allocations.  This lets us fail every library
 * malloc in turn during a two-threaded handshake without crashing
 * OpenSSL.
 * ================================================================ */

#include "dssh_test_alloc.h"

static int
iter_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    struct dssh_bytes_result *output_result, void *cbdata)
{
	(void)username;
	(void)username_len;
	(void)password;
	(void)password_len;
	(void)cbdata;
	return test_auth_output_finish(output_result, NULL, 0, DSSH_AUTH_SUCCESS);
}

static int
allocation_handshake_operation(struct allocation_pair *owner, dssh_session sess)
{
	(void)owner;
	return dssh_transport_handshake(sess);
}

static int
allocation_auth_operation(struct allocation_pair *owner, dssh_session sess)
{
	if (sess == owner->pair.client)
		return dssh_auth_password(sess, "testuser", "testpass", NULL, NULL);
	struct dssh_auth_server_cbs cbs = {0};
	cbs.methods_str = "password";
	cbs.password_cb = iter_password_cb;
	uint8_t username[256];
	size_t length = sizeof(username);
	int ret = dssh_auth_server(sess, &cbs, username, &length);
	if (ret == 0 && (length != sizeof("testuser") - 1
	    || memcmp(username, "testuser", sizeof("testuser") - 1) != 0))
		return DSSH_ERROR_INVALID;
	return ret;
}

static const uint8_t allocation_prompt[] = "Password: ";
static const uint8_t allocation_response[] = "testpass";
static const struct dssh_auth_prompt allocation_prompts[] = {
	{{allocation_prompt, sizeof(allocation_prompt) - 1}, false},
};
static const struct dssh_bytes allocation_responses[] = {
	{allocation_response, sizeof(allocation_response) - 1},
};

static void
allocation_kbi_release(void *opaque)
{
	unsigned *releases = opaque;
	if (*releases == UINT_MAX)
		abort();
	(*releases)++;
}

static int
allocation_kbi_client(const uint8_t *name, size_t name_length,
    const uint8_t *instruction, size_t instruction_length, uint32_t count,
    const uint8_t **prompts, const size_t *lengths, const bool *echo,
    struct dssh_auth_responses *out, void *opaque)
{
	(void)name;
	(void)instruction;
	struct allocation_pair *owner = opaque;
	if (name_length != 0 || instruction_length != 0 || count != 1
	    || prompts == NULL || lengths == NULL || echo == NULL
	    || prompts[0] == NULL || lengths[0] != sizeof(allocation_prompt) - 1
	    || memcmp(prompts[0], allocation_prompt, sizeof(allocation_prompt) - 1) != 0
	    || echo[0] || owner->client_rounds != 0)
		return DSSH_ERROR_INVALID;
	owner->client_rounds = 1;
	out->items = allocation_responses;
	out->count = 1;
	out->lease = (struct dssh_result_lease){.release = allocation_kbi_release,
	    .owner = &owner->response_released};
	owner->response_published = 1;
	return 0;
}

static int
allocation_kbi_server(const uint8_t *username, size_t username_length,
    uint32_t count, const uint8_t **responses, const size_t *lengths,
    struct dssh_auth_challenge *out, void *opaque)
{
	struct allocation_pair *owner = opaque;
	if (username == NULL || username_length != sizeof("testuser") - 1
	    || memcmp(username, "testuser", sizeof("testuser") - 1) != 0)
		return DSSH_ERROR_INVALID;
	if (owner->server_rounds == 0) {
		if (count != 0 || responses != NULL)
			return DSSH_ERROR_INVALID;
		owner->server_rounds = 1;
		out->prompts = allocation_prompts;
		out->count = 1;
		out->lease = (struct dssh_result_lease){.release = allocation_kbi_release,
		    .owner = &owner->prompt_released};
		owner->prompt_published = 1;
		return DSSH_AUTH_KBI_PROMPT;
	}
	if (owner->server_rounds != 1 || count != 1 || responses == NULL
	    || lengths == NULL || responses[0] == NULL
	    || lengths[0] != sizeof(allocation_response) - 1
	    || memcmp(responses[0], allocation_response, sizeof(allocation_response) - 1) != 0)
		return DSSH_ERROR_INVALID;
	owner->server_rounds = 2;
	return DSSH_AUTH_SUCCESS;
}

static int
allocation_kbi_operation(struct allocation_pair *owner, dssh_session sess)
{
	if (sess == owner->pair.client)
		return dssh_auth_keyboard_interactive(sess, "testuser", allocation_kbi_client, owner);
	struct dssh_auth_server_cbs cbs = {0};
	cbs.methods_str = "keyboard-interactive";
	cbs.keyboard_interactive_cb = allocation_kbi_server;
	cbs.cbdata = owner;
	uint8_t username[256];
	size_t length = sizeof(username);
	int ret = dssh_auth_server(sess, &cbs, username, &length);
	if (ret == 0 && (length != sizeof("testuser") - 1
	    || memcmp(username, "testuser", sizeof("testuser") - 1) != 0))
		return DSSH_ERROR_INVALID;
	return ret;
}

static int
allocation_password_control(void)
{
	struct allocation_pair owner;
	int ret = allocation_prepare(&owner, true);
	if (ret == 0)
		ret = allocation_pair_run(&owner, allocation_handshake_operation, -1, false, false);
	if (ret == 0)
		/* The first server identity needs one retained allocation. Refuse
		 * any additional allocation during this plain password exchange. */
		ret = allocation_pair_run(&owner, allocation_auth_operation, 1, false, false);
	int calls = dssh_test_alloc_count();
	bool completed = ret == 0 && owner.pair.client_result == 0 && owner.pair.server_result == 0;
	allocation_cleanup(&owner);
	bool valid = completed && calls == 1 && allocation_pair_retired(&owner);
	if (printf("allocation_password completed=%d calls=%d retired=%d valid=%d\n",
	        completed, calls, allocation_pair_retired(&owner), valid) < 0)
		return TEST_FAIL;
	return valid ? TEST_PASS : TEST_FAIL;
}

static int
iter_accept_exec(dssh_channel channel, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)channel;
	(void)params;
	(void)result;
	(void)cbdata;
	return 0;
}

static const struct dssh_chan_accept_cbs iter_accept_cbs = {
	.exec = iter_accept_exec,
};

static int
allocation_conn_operation(struct allocation_pair *owner, dssh_session sess)
{
	int ret = dssh_session_start(sess);
	if (ret < 0)
		return ret;
	if (sess == owner->pair.client) {
		struct dssh_chan_params params = {0};
		ret = dssh_chan_params_init(&params, DSSH_CHAN_EXEC);
		if (ret == 0)
			ret = dssh_chan_params_set_command(&params, "echo test");
		if (ret == 0)
			ret = dssh_chan_params_set_pty(&params, false);
		if (ret == 0)
			owner->channels[0] = dssh_chan_open(sess, &params);
		dssh_chan_params_free(&params);
		if (ret < 0)
			return ret;
		return owner->channels[0] == NULL ? DSSH_ERROR_INIT : 0;
	}
	owner->channels[1] = dssh_chan_accept(sess, &iter_accept_cbs, 2000);
	return owner->channels[1] == NULL ? DSSH_ERROR_INIT : 0;
}

static int
test_allocation_fixture_sync(void)
{
	for (int point = 0; point < 5; point++) {
		struct allocation_pair owner;
		if (allocation_prepare(&owner, false) != 0)
			return TEST_FAIL;
		dssh_test_ossl_fail_after(point);
		int ret = allocation_pair_sync_init(&owner);
		int calls = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		bool valid = ret == (point == 4 ? 0 : DSSH_ERROR_INIT)
		    && calls == (point == 4 ? 4 : point + 1)
		    && owner.gate.mutex_ready == (point >= 1)
		    && owner.gate.condition_ready == (point >= 2)
		    && owner.watch.mutex_ready == (point >= 3)
		    && owner.watch.condition_ready == (point >= 4);
		if (ret == 0)
			valid &= allocation_pair_run(&owner, allocation_handshake_operation, -1, false, false) == 0;
		allocation_cleanup(&owner);
		allocation_cleanup(&owner);
		valid &= allocation_pair_retired(&owner);
		if (printf("allocation_sync point=%d calls=%d ready_prefix=%d created=%u joined=%u retired=%d valid=%d\n",
		        point, calls, point, owner.pair.created, owner.pair.joined,
		        allocation_pair_retired(&owner), valid) < 0)
			return TEST_FAIL;
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_allocation_fixture_watchdog(void)
{
	struct allocation_pair owner;
	if (allocation_prepare(&owner, true) != 0)
		return TEST_FAIL;
	dssh_test_clock_fail_after(0);
	int ret = allocation_watch_start(&owner, 3000);
	dssh_test_clock_reset();
	bool valid = ret == DSSH_ERROR_INIT && !owner.watch.owned && owner.watch.created == 0;
	struct timespec invalid = {.tv_nsec = 1000000000L};
	dssh_test_clock_set(invalid);
	ret = allocation_watch_start(&owner, 3000);
	dssh_test_clock_reset();
	valid &= ret == DSSH_ERROR_INIT && !owner.watch.owned && owner.watch.created == 0;
	/* Zero delay uses the observed native UTC time, so the actual native
	 * wait expires without a sleep or a guessed timing allowance. */
	ret = allocation_watch_start(&owner, 0);
	if (ret == 0)
		allocation_watch_join(&owner.watch);
	valid &= ret == 0 && owner.watch.fired && owner.watch.created == 1 && owner.watch.joined == 1
	    && dssh_session_is_terminated(owner.pair.client) && dssh_session_is_terminated(owner.pair.server);
	allocation_cleanup(&owner);
	allocation_cleanup(&owner);
	valid &= allocation_pair_retired(&owner);
	if (printf("allocation_watchdog clock_refusals=2 expired=%d created=%u joined=%u retired=%d valid=%d\n",
	        owner.watch.fired, owner.watch.created, owner.watch.joined,
	        allocation_pair_retired(&owner), valid) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_allocation_fixture_owner(void)
{
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool plain = mode == NULL || strcmp(mode, "plain") == 0;
	struct allocation_pair owner;
	int ret = allocation_prepare(&owner, true);
	if (ret == 0)
		ret = allocation_pair_run(&owner, allocation_handshake_operation, -1, false, true);
	bool valid = plain ? ret == 0 : ret < 0;
	allocation_cleanup(&owner);
	allocation_cleanup(&owner);
	valid &= allocation_pair_retired(&owner);
	unsigned created = owner.pair.created + owner.watch.created;
	unsigned joined = owner.pair.joined + owner.watch.joined;
	if (printf("allocation_fixture_owner created=%u joined=%u retired=%d valid=%d\n",
	        created, joined, allocation_pair_retired(&owner), valid) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
allocation_assertion_failure(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static int
allocation_assertion_control(unsigned phase)
{
	struct allocation_pair owner;
	if (allocation_prepare(&owner, true) != 0)
		return 2;
	int ret = phase >= 1 ? allocation_watch_start(&owner, 3000) : 0;
	owner.operation = allocation_handshake_operation;
	owner.pair.operation = allocation_pair_worker;
	for (unsigned i = 1; ret == 0 && i < phase; i++)
		ret = test_pair_start(&owner.pair, i - 1);
	if (ret != 0) {
		allocation_cleanup(&owner);
		return 2;
	}
	ret = allocation_assertion_failure();
	unsigned created = owner.pair.created + owner.watch.created;
	unsigned joined = owner.pair.joined + owner.watch.joined;
	bool valid = ret == TEST_FAIL && allocation_pair_retired(&owner)
	    && created == phase && joined == phase;
	allocation_cleanup(&owner);
	if (printf("allocation_assertion phase=%u created=%u joined=%u retired=%d valid=%d\n",
	        phase, created, joined, allocation_pair_retired(&owner), valid) < 0)
		return 2;
	return valid ? 1 : 2;
}

static int
allocation_sync_fatal(const char *path, const char *mode)
{
	enum { ENTER, READY, RELEASE, CANCEL, WATCH_ENTER, WATCH_CANCEL };
	static const struct {
		const char *path;
		int operation;
		int point;
	} sites[] = {
		{"gate.enter_lock", ENTER, 0}, {"gate.arrive_notify", ENTER, 1},
		{"gate.enter_wait", ENTER, 2}, {"gate.enter_unlock", ENTER, -1},
		{"gate.ready_lock", READY, 0}, {"gate.ready_wait", READY, 1},
		{"gate.ready_unlock", READY, -1},
		{"gate.release_lock", RELEASE, 0}, {"gate.release_notify", RELEASE, 1},
		{"gate.release_unlock", RELEASE, -1},
		{"gate.cancel_lock", CANCEL, 0}, {"gate.cancel_notify", CANCEL, 1},
		{"gate.cancel_unlock", CANCEL, -1},
		{"watch.enter_lock", WATCH_ENTER, 0}, {"watch.enter_wait", WATCH_ENTER, 1},
		{"watch.enter_unlock", WATCH_ENTER, -1},
		{"watch.cancel_lock", WATCH_CANCEL, 0}, {"watch.cancel_notify", WATCH_CANCEL, 1},
		{"watch.cancel_unlock", WATCH_CANCEL, -1},
	};
	for (size_t i = 0; i < sizeof(sites) / sizeof(sites[0]); i++) {
		if (strcmp(path, sites[i].path) != 0)
			continue;
		bool unlock = sites[i].point < 0;
		bool released = strcmp(mode, "released") == 0;
		if (unlock ? (!released && strcmp(mode, "retained") != 0) : strcmp(mode, "error") != 0)
			return 2;
		struct allocation_pair owner;
		if (allocation_prepare(&owner, true) != 0)
			return 2;
		if (unlock) {
			owner.gate.released = true;
			owner.gate.arrived = sites[i].operation == READY ? 2U : 0U;
			owner.watch.cancelled = true;
			mtx_t *mutex = sites[i].operation >= WATCH_ENTER ? &owner.watch.mutex : &owner.gate.mutex;
			dssh_test_unlock_fail_after(mutex, 0, released);
		}
		else
			dssh_test_thrd_fail_after(sites[i].point);
		switch (sites[i].operation) {
			case ENTER:
				(void)allocation_gate_enter(&owner.gate);
				break;
			case READY:
				allocation_gate_ready(&owner.gate);
				break;
			case RELEASE:
				allocation_gate_release(&owner.gate);
				break;
			case CANCEL:
				allocation_gate_cancel(&owner.gate);
				break;
			case WATCH_ENTER:
				(void)allocation_watch_worker(&owner);
				break;
			case WATCH_CANCEL:
				allocation_watch_cancel(&owner.watch);
				break;
		}
		allocation_cleanup(&owner);
		return 2;
	}
	return 2;
}

enum allocation_phase {
	ALLOCATION_HANDSHAKE,
	ALLOCATION_AUTH,
	ALLOCATION_CONNECTION
};

static int
allocation_threaded_sweep(enum allocation_phase phase, bool provider)
{
	const char *name = phase == ALLOCATION_CONNECTION ? "alloc/conn_iterate"
	    : phase == ALLOCATION_AUTH ? "alloc/auth_iterate"
	    : provider ? "ossl/handshake_iterate" : "alloc/handshake_iterate";
	struct allocation_sweep sweep = {0};
	int limit = provider ? 500 : phase == ALLOCATION_CONNECTION ? 200 : 100;
	for (int point = 0; point < limit; point++) {
		struct allocation_pair owner;
		int ret = allocation_prepare(&owner, true);
		if (ret == 0 && phase != ALLOCATION_HANDSHAKE)
			ret = allocation_pair_run(&owner, allocation_handshake_operation, -1, false, false);
		if (ret == 0 && phase == ALLOCATION_CONNECTION)
			ret = allocation_pair_run(&owner, allocation_auth_operation, -1, false, false);
		if (ret != 0) {
			allocation_cleanup(&owner);
			return TEST_FAIL;
		}
		int (*operation)(struct allocation_pair *, dssh_session) = phase == ALLOCATION_HANDSHAKE
		    ? allocation_handshake_operation : phase == ALLOCATION_AUTH
		    ? allocation_kbi_operation : allocation_conn_operation;
		ret = allocation_pair_run(&owner, operation, point, provider, phase == ALLOCATION_CONNECTION);
		int calls = provider ? dssh_test_ossl_count() : dssh_test_alloc_count();
		bool completed = ret == 0 && owner.pair.client_result == 0 && owner.pair.server_result == 0;
		bool valid = ret <= 0 && owner.pair.client_result <= 0 && owner.pair.server_result <= 0;
		if (phase == ALLOCATION_AUTH) {
			valid &= owner.prompt_published == owner.prompt_released
			    && owner.response_published == owner.response_released;
			if (completed)
				valid &= owner.server_rounds == 2 && owner.client_rounds == 1
				    && owner.prompt_published == 1 && owner.response_published == 1;
			if (printf("allocation_kbi point=%d server_rounds=%u client_rounds=%u prompts=%u prompt_releases=%u responses=%u response_releases=%u\n",
			        point, owner.server_rounds, owner.client_rounds, owner.prompt_published,
			        owner.prompt_released, owner.response_published, owner.response_released) < 0) {
				allocation_cleanup(&owner);
				return TEST_FAIL;
			}
		}
		if (completed && phase == ALLOCATION_CONNECTION)
			valid &= owner.channels[0] != NULL && owner.channels[1] != NULL && !owner.watch.fired;
		allocation_cleanup(&owner);
		allocation_cleanup(&owner);
		valid &= allocation_pair_retired(&owner);
		if (printf("allocation_owner name=%s point=%d created=%u joined=%u watch_created=%u watch_joined=%u retired=%d\n",
		        name, point, owner.pair.created, owner.pair.joined, owner.watch.created,
		        owner.watch.joined, allocation_pair_retired(&owner)) < 0)
			return TEST_FAIL;
		int step = allocation_sweep_step(&sweep, name, point, calls, completed, valid);
		if (step != 0)
			return step > 0 ? TEST_PASS : TEST_FAIL;
	}
	return TEST_FAIL;
}

static int
test_alloc_handshake_iterate(void)
{
	return allocation_threaded_sweep(ALLOCATION_HANDSHAKE, false);
}

static int
test_alloc_auth_iterate(void)
{
	if (allocation_password_control() != TEST_PASS)
		return TEST_FAIL;
	return allocation_threaded_sweep(ALLOCATION_AUTH, false);
}

static int
test_alloc_conn_iterate(void)
{
	return allocation_threaded_sweep(ALLOCATION_CONNECTION, false);
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_ossl_handshake_iterate(void)
{
	return allocation_threaded_sweep(ALLOCATION_HANDSHAKE, true);
}

/* ================================================================
 * Isolated OpenSSL failure injection for key algo operations.
 *
 * Single-threaded: generate key, then iterate ossl failures through
 * sign -> pubkey -> verify.  No handshake, no I/O -- fast.
 * ================================================================ */

static int
test_ossl_key_sign_verify_iterate(void)
{
	struct allocation_sweep sweep = {0};
	for (int n = 0; n < 500; n++) {
		dssh_test_reset_global_config();
		dssh_test_ossl_reset();
		mock_alloc_reset();
		struct test_signature_copies copies = {0};
		int ret = test_register_key_algos();
		if (ret == 0)
			ret = test_generate_host_key();
		dssh_key_algo ka = ret == 0 ? find_key_algo(test_key_algo_name()) : NULL;
		if (ka == NULL) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		const uint8_t data[] = "test data for ossl sign/verify";
		uint8_t *sig = NULL;
		size_t sig_len = 0;
		const uint8_t *pub = NULL;
		size_t pub_len = 0;
		/* Establish the exact callback sequence without injection first. */
		ret = test_signature_copy(&copies, ka->sign, &sig, &sig_len,
		    data, sizeof(data) - 1, ka->ctx);
		if (ret == 0)
			ret = ka->pubkey(&pub, &pub_len, ka->ctx);
		if (ret == 0)
			ret = ka->verify(pub, pub_len, sig, sig_len, data, sizeof(data) - 1);
		test_signature_copy_free(&copies, sig);
		if (ret != 0) {
			test_signature_copies_finish(&copies, TEST_FAIL);
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		sig = NULL;
		sig_len = 0;
		pub = NULL;
		pub_len = 0;
		dssh_test_ossl_fail_after(n);
		int sign_res = test_signature_copy(&copies, ka->sign, &sig, &sig_len,
		    data, sizeof(data) - 1, ka->ctx);
		int pub_res = ka->pubkey(&pub, &pub_len, ka->ctx);
		int verify_res = DSSH_ERROR_INIT;
		bool valid = sign_res <= 0 && pub_res <= 0;
		if (sign_res == 0 && pub_res == 0) {
			valid &= sig != NULL && sig_len > 0 && pub != NULL && pub_len > 0;
			if (valid)
				verify_res = ka->verify(pub, pub_len, sig, sig_len, data, sizeof(data) - 1);
			valid &= verify_res <= 0;
		}
		int calls = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		test_signature_copies_finish(&copies, TEST_PASS);
		bool completed = sign_res == 0 && pub_res == 0 && verify_res == 0;
		dssh_test_reset_global_config();
		int step = allocation_sweep_step(&sweep, "ossl/key_sign_verify", n, calls, completed, valid);
		if (step != 0)
			return step > 0 ? TEST_PASS : TEST_FAIL;
	}
	return TEST_FAIL;
}

/* ================================================================
 * Isolated OpenSSL failure injection for key generation.
 *
 * Single-threaded: iterate ossl failures through generate().
 * ================================================================ */

static int
generate_selected_key(void)
{
	if (test_using_rsa512())
		return dssh_rsa_sha2_512_generate_key(2048);
	if (test_using_rsa())
		return dssh_rsa_sha2_256_generate_key(2048);
	return dssh_ed25519_generate_key();
}

static bool
selected_key_ready(dssh_key_algo ka)
{
	if (ka == NULL || ka->haskey(ka->ctx) <= 0)
		return false;
	const uint8_t *pub = NULL;
	size_t length = 0;
	if (ka->pubkey(&pub, &length, ka->ctx) != 0 || pub == NULL || length == 0)
		return false;
	unsigned bits = ka->key_bits(pub, length);
	return bits == (test_using_rsa() ? 2048U : 256U);
}

static int
test_ossl_keygen_iterate(void)
{
	struct allocation_sweep sweep = {0};
	for (int n = 0; n < 500; n++) {
		dssh_test_reset_global_config();
		dssh_test_ossl_reset();
		mock_alloc_reset();
		int ret = test_register_key_algos();
		dssh_key_algo ka = ret == 0 ? find_key_algo(test_key_algo_name()) : NULL;
		if (ka == NULL) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		dssh_test_ossl_fail_after(n);
		ret = generate_selected_key();
		int calls = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		bool valid = ret <= 0 && (ret == 0 ? selected_key_ready(ka) : ka->ctx == NULL);
		/* No other registered key family may acquire this generation. */
		for (dssh_key_algo other = gconf.key_algo_head; other != NULL; other = other->next) {
			if (other != ka)
				valid &= other->ctx == NULL;
		}
		dssh_test_reset_global_config();
		int step = allocation_sweep_step(&sweep, "ossl/keygen", n, calls, ret == 0, valid);
		if (step != 0)
			return step > 0 ? TEST_PASS : TEST_FAIL;
	}
	return TEST_FAIL;
}

/* ================================================================
 * Isolated OpenSSL failure injection for enc init/encrypt.
 *
 * Single-threaded: register aes256-ctr, iterate ossl failures
 * through init -> encrypt -> cleanup.
 * ================================================================ */

static int
test_ossl_enc_iterate(void)
{
	/* First AES-256-CTR block of NIST SP 800-38A F.5.5; the complete
	 * published corpus is also covered by test_algo_enc.c. */
	static const uint8_t key[32] = {
		0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
		0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
		0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
		0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
	};
	static const uint8_t iv[16] = {
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
	};
	static const uint8_t plaintext[16] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
	};
	static const uint8_t ciphertext[16] = {
		0x60, 0x1e, 0xc3, 0x13, 0x77, 0x57, 0x89, 0xa5,
		0xb7, 0xa7, 0xf5, 0x04, 0xbb, 0xf3, 0xd2, 0x28
	};
	struct allocation_sweep sweep = {0};
	for (int n = 0; n < 500; n++) {
		dssh_test_reset_global_config();
		dssh_test_ossl_reset();
		mock_alloc_reset();
		int ret = dssh_register_aes256_ctr();
		dssh_enc enc = ret == 0 ? gconf.enc_head : NULL;
		if (enc == NULL) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		uint8_t buf[sizeof(plaintext)];
		memcpy(buf, plaintext, sizeof(buf));
		dssh_enc_ctx *ctx = NULL;
		dssh_test_ossl_fail_after(n);
		ret = enc->init(key, iv, true, &ctx);
		bool valid = ret <= 0 && (ret < 0 || ctx != NULL);
		if (ret == 0 && valid)
			ret = enc->encrypt(buf, sizeof(buf), ctx);
		int calls = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		valid &= ret <= 0 && (ret != 0 || memcmp(buf, ciphertext, sizeof(buf)) == 0);
		/* Retire a published context on either init outcome. */
		if (ctx != NULL)
			enc->cleanup(ctx);
		dssh_test_reset_global_config();
		int step = allocation_sweep_step(&sweep, "ossl/enc", n, calls, ret == 0, valid);
		if (step != 0)
			return step > 0 ? TEST_PASS : TEST_FAIL;
	}
	return TEST_FAIL;
}

/* ================================================================
 * Isolated OpenSSL failure injection for mac init/compute/verify.
 *
 * Single-threaded: register hmac-sha2-256, iterate ossl failures
 * through init -> compute -> verify -> cleanup.
 * ================================================================ */

static int
test_ossl_mac_iterate(void)
{
	/* RFC 4231 case 1: zero-extending the 20-byte key to the SSH
	 * module's fixed 32-byte key input leaves the HMAC key block exact. */
	static const uint8_t expected[32] = {
		0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
		0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
		0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
		0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7
	};
	struct allocation_sweep sweep = {0};
	for (int n = 0; n < 500; n++) {
		dssh_test_reset_global_config();
		dssh_test_ossl_reset();
		mock_alloc_reset();
		int ret = dssh_register_hmac_sha2_256();
		dssh_mac mac = ret == 0 ? gconf.mac_head : NULL;
		if (mac == NULL) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		uint8_t key[32] = {0};
		memset(key, 0x0b, 20);
		const uint8_t data[] = "Hi There";
		uint8_t tag[32] = {0};
		dssh_mac_ctx *ctx = NULL;
		dssh_test_ossl_fail_after(n);
		ret = mac->init(key, &ctx);
		bool valid = ret <= 0 && (ret < 0 || ctx != NULL);
		if (ret == 0 && valid)
			ret = mac->generate(data, sizeof(data) - 1, tag, ctx);
		int calls = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		valid &= ret <= 0 && (ret != 0 || memcmp(tag, expected, sizeof(tag)) == 0);
		if (ctx != NULL)
			mac->cleanup(ctx);
		dssh_test_reset_global_config();
		int step = allocation_sweep_step(&sweep, "ossl/mac", n, calls, ret == 0, valid);
		if (step != 0)
			return step > 0 ? TEST_PASS : TEST_FAIL;
	}
	return TEST_FAIL;
}

/* ================================================================
 * Isolated key_algo ossl iterate: verify and pubkey
 *
 * These exercise OpenSSL error paths in verify() and pubkey()
 * without any session or I/O -- just the crypto callbacks.
 * ================================================================ */

static int
test_ossl_key_verify_iterate(void)
{
	struct test_signature_copies copies = {0};
	dssh_test_reset_global_config();
	dssh_test_ossl_reset();
	mock_alloc_reset();
	int ret = test_register_key_algos();
	if (ret == 0)
		ret = test_generate_host_key();
	dssh_key_algo ka = ret == 0 ? find_key_algo(test_key_algo_name()) : NULL;
	const uint8_t data[] = "test data for verify iterate";
	uint8_t *sig = NULL;
	const uint8_t *pub = NULL;
	size_t sig_len = 0;
	size_t pub_len = 0;
	if (ka != NULL)
		ret = test_signature_copy(&copies, ka->sign, &sig, &sig_len,
		    data, sizeof(data) - 1, ka->ctx);
	if (ret == 0 && ka != NULL)
		ret = ka->pubkey(&pub, &pub_len, ka->ctx);
	if (ret == 0 && ka != NULL)
		ret = ka->verify(pub, pub_len, sig, sig_len, data, sizeof(data) - 1);
	int result = TEST_FAIL;
	if (ret == 0 && ka != NULL) {
		struct allocation_sweep sweep = {0};
		for (int n = 0; n < 500; n++) {
			dssh_test_ossl_fail_after(n);
			ret = ka->verify(pub, pub_len, sig, sig_len, data, sizeof(data) - 1);
			int calls = dssh_test_ossl_count();
			dssh_test_ossl_reset();
			int step = allocation_sweep_step(&sweep, "ossl/key_verify", n, calls, ret == 0, ret <= 0);
			if (step != 0) {
				result = step > 0 ? TEST_PASS : TEST_FAIL;
				break;
			}
		}
	}
	test_signature_copies_finish(&copies, result);
	dssh_test_reset_global_config();
	return result;
}

static int
test_ossl_key_pubkey_iterate(void)
{
	dssh_test_reset_global_config();
	dssh_test_ossl_reset();
	mock_alloc_reset();
	int ret = test_register_key_algos();
	if (ret == 0)
		ret = test_generate_host_key();
	dssh_key_algo ka = ret == 0 ? find_key_algo(test_key_algo_name()) : NULL;
	if (ka == NULL) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_key_algo_ctx *original_ctx = ka->ctx;
	const uint8_t *original_blob = NULL;
	size_t original_len = 0;
	ret = ka->pubkey(&original_blob, &original_len, ka->ctx);
	if (ret != 0 || original_blob == NULL || original_len == 0) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	/* The cached reader must not consume the armed provider fault. */
	dssh_test_ossl_fail_after(0);
	const uint8_t *blob = NULL;
	size_t len = 0;
	ret = ka->pubkey(&blob, &len, ka->ctx);
	int calls = dssh_test_ossl_count();
	dssh_test_ossl_reset();
	bool valid = ret == 0 && calls == 0 && blob == original_blob && len == original_len;
	int result = TEST_FAIL;
	struct allocation_sweep sweep = {0};
	for (int n = 0; valid && n < 500; n++) {
		dssh_test_ossl_fail_after(n);
		ret = test_generate_host_key();
		calls = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		valid = ret <= 0;
		if (ret < 0) {
			valid &= ka->ctx == original_ctx;
			if (valid)
				valid = ka->pubkey(&blob, &len, ka->ctx) == 0
				    && blob == original_blob && len == original_len;
		}
		else {
			valid &= selected_key_ready(ka);
			/* A tolerated replacement can retire the former key. Track
			 * the new live cache for the following iteration, not freed
			 * pointer values from a prior successful replacement. */
			if (valid) {
				original_ctx = ka->ctx;
				valid = ka->pubkey(&original_blob, &original_len, ka->ctx) == 0;
			}
		}
		int step = allocation_sweep_step(&sweep, "ossl/key_pubkey", n, calls, ret == 0, valid);
		if (step != 0) {
			result = step > 0 ? TEST_PASS : TEST_FAIL;
			break;
		}
	}
	dssh_test_reset_global_config();
	return result;
}

#endif /* DSSH_CRYPTO_OPENSSL -- ossl key/enc/mac iterate */

/* Direct KEX sweeps use real peers in both directions. The preparation phase
 * completes version exchange and KEXINIT before arming either injector. The
 * opposite KEX actor excludes itself before its first failable operation. */
static int
allocation_kex_negotiate(dssh_session sess)
{
	int ret = version_exchange(sess);
	if (ret == 0)
		ret = kexinit(sess);
	return ret;
}

static int
allocation_kex_excluded_peer(dssh_session sess)
{
	dssh_test_alloc_exclude_thread();
	dssh_test_ossl_exclude_thread();
	return kex(sess);
}

#ifdef DSSH_CRYPTO_OPENSSL
static bool allocation_kex_random_observe;
static unsigned allocation_kex_random_mode;
#endif

/* Observe actual hash-final calls on the target thread; copy only the selected
 * public exchange H, after any hybrid secret-combiner finalization.
 * Fixed64-byte storage covers the specified SHA256/SHA512 builtin results.
 * No result/native owner address is retained past the forwarded call. */
static _Thread_local bool allocation_guard_hash_watch;
static _Thread_local unsigned allocation_guard_hash_calls;
static _Thread_local unsigned allocation_guard_hash_target;
static _Thread_local bool allocation_guard_hash_valid;
static _Thread_local size_t allocation_guard_hash_length;
static _Thread_local uint8_t allocation_guard_hash[64];

int __real_dssh_hash_final(dssh_hash_ctx *context, uint8_t *output, size_t capacity);

int
__wrap_dssh_hash_final(dssh_hash_ctx *context, uint8_t *output, size_t capacity)
{
	int ret = __real_dssh_hash_final(context, output, capacity);
	if (allocation_guard_hash_watch) {
		if (allocation_guard_hash_calls < UINT_MAX)
			allocation_guard_hash_calls++;
		if (ret != 0 || capacity == 0 || capacity > sizeof(allocation_guard_hash))
			allocation_guard_hash_valid = false;
		else if (allocation_guard_hash_calls == allocation_guard_hash_target) {
			memcpy(allocation_guard_hash, output, capacity);
			allocation_guard_hash_length = capacity;
		}
	}
	return ret;
}

struct allocation_kex_observation {
	int target_result;
	int peer_result;
#ifdef DSSH_CRYPTO_OPENSSL
	struct dssh_test_bn_random_stats random;
#endif
	int calls;
	unsigned created;
	unsigned joined;
	bool completed;
	bool retired;
	bool valid;
	unsigned hashes;
	size_t digest;
	bool matched;
	bool restored;
};

static bool
allocation_kex_results_equal(const struct test_session_pair *pair)
{
	const struct dssh_kex_result *client = &pair->client->trans.kex_result;
	const struct dssh_kex_result *server = &pair->server->trans.kex_result;
	return client->lease.release != NULL && server->lease.release != NULL
	    && client->shared_secret.data != NULL && server->shared_secret.data != NULL
	    && client->exchange_hash.data != NULL && server->exchange_hash.data != NULL
	    && client->shared_secret.len > 0 && client->exchange_hash.len > 0
	    && client->shared_secret.len == server->shared_secret.len
	    && client->exchange_hash.len == server->exchange_hash.len
	    && memcmp(client->shared_secret.data, server->shared_secret.data,
	        client->shared_secret.len) == 0
	    && memcmp(client->exchange_hash.data, server->exchange_hash.data,
	        client->exchange_hash.len) == 0;
}

static bool
allocation_kex_result_empty(dssh_session sess)
{
	const struct dssh_kex_result *result = &sess->trans.kex_result;
	return result->shared_secret.data == NULL && result->shared_secret.len == 0
	    && result->exchange_hash.data == NULL && result->exchange_hash.len == 0
	    && result->lease.owner == NULL && result->lease.release == NULL
	    && sess->trans.kex_active == NULL && sess->trans.kex_state == NULL;
}

static int
allocation_kex_case(bool client, bool provider, int point, unsigned guard,
    struct allocation_kex_observation *observed)
{
	*observed = (struct allocation_kex_observation){
	    .target_result = DSSH_ERROR_INIT, .peer_result = DSSH_ERROR_INIT};
	struct allocation_pair owner;
	int ret = allocation_prepare(&owner, false);
	if (ret == 0)
		ret = test_pair_run(&owner.pair, allocation_kex_negotiate);
	if (ret == 0) {
		bool selected = owner.pair.client->trans.kex_selected != NULL
		    && owner.pair.server->trans.kex_selected != NULL
		    && strcmp(owner.pair.client->trans.kex_selected->name, test_kex_algo_name()) == 0
		    && strcmp(owner.pair.server->trans.kex_selected->name, test_kex_algo_name()) == 0
		    && owner.pair.client->trans.key_algo_selected != NULL
		    && owner.pair.server->trans.key_algo_selected != NULL
		    && strcmp(owner.pair.client->trans.key_algo_selected->name, test_key_algo_name()) == 0
		    && strcmp(owner.pair.server->trans.key_algo_selected->name, test_key_algo_name()) == 0;
		if (!selected)
			ret = DSSH_ERROR_INVALID;
	}
	if (ret == 0) {
		owner.pair.operation = allocation_kex_excluded_peer;
		ret = test_pair_start(&owner.pair, client ? 1U : 0U);
	}
	if (ret == 0) {
		if (point >= 0) {
			if (provider)
				dssh_test_ossl_fail_after(point);
			else
				dssh_test_alloc_fail_after(point);
		}
		dssh_session target = client ? owner.pair.client : owner.pair.server;
		dssh_key_algo saved = target->trans.key_algo_selected;
		struct dssh_key_algo_s missing_verify = {0};
		uint32_t before = target->trans.rx_seq;
		allocation_guard_hash_calls = 0;
		allocation_guard_hash_target = test_using_sntrup() || test_using_mlkem() ? 2U : 1U;
		allocation_guard_hash_length = 0;
		allocation_guard_hash_valid = true;
		allocation_guard_hash_watch = guard != 0;
		if (guard != 0)
			target->trans.key_algo_selected = guard == 1 ? NULL : &missing_verify;
#ifdef DSSH_CRYPTO_OPENSSL
		if (allocation_kex_random_observe)
			dssh_test_ossl_bn_random_begin(allocation_kex_random_mode);
#endif
		observed->target_result = kex(target);
		allocation_guard_hash_watch = false;
		target->trans.key_algo_selected = saved;
		observed->restored = target->trans.key_algo_selected == saved;
		observed->hashes = allocation_guard_hash_calls;
		observed->digest = allocation_guard_hash_length;
		bool gex = strcmp(test_kex_algo_name(), "diffie-hellman-group-exchange-sha256") == 0;
		unsigned replies = gex ? 2U : 1U;
		uint8_t reply_type = gex ? UINT8_C(33) : UINT8_C(31);
		bool guard_reply = guard != 0 && target->trans.rx_seq >= before
		    && target->trans.rx_seq - before == replies && target->trans.rx_packet != NULL
		    && target->trans.packet_buf_sz > 9 && target->trans.rx_packet[9] == reply_type;
#ifdef DSSH_CRYPTO_OPENSSL
		/* Failed methods may already have destroyed their native owner. */
		if (allocation_kex_random_observe)
			observed->random = dssh_test_ossl_bn_random_end(false);
#endif
		observed->calls = provider ? dssh_test_ossl_count() : dssh_test_alloc_count();
		dssh_test_alloc_reset();
		dssh_test_ossl_reset();
		/* A successful exchange lets its peer finish naturally; shutting down
		 * that peer's receive direction could discard its queued reply. */
		if (observed->target_result != 0 && !guard_reply)
			test_pair_cancel(&owner.pair);
		test_pair_join(&owner.pair);
		observed->peer_result = client ? owner.pair.server_result : owner.pair.client_result;
		observed->completed = observed->target_result == 0 && observed->peer_result == 0;
		observed->valid = observed->target_result <= 0 && observed->peer_result <= 0
		    && (observed->target_result == 0
		        ? observed->completed && allocation_kex_results_equal(&owner.pair)
		        : allocation_kex_result_empty(target));
		if (guard != 0) {
			const struct dssh_kex_result *peer = &owner.pair.server->trans.kex_result;
			unsigned finals = test_using_sntrup() || test_using_mlkem() ? 2U : 1U;
			observed->matched = allocation_guard_hash_valid && observed->hashes == finals
			    && observed->digest != 0 && peer->lease.release != NULL
			    && peer->exchange_hash.data != NULL && peer->exchange_hash.len == observed->digest
			    && memcmp(allocation_guard_hash, peer->exchange_hash.data, observed->digest) == 0;
			observed->valid = guard_reply && observed->matched && observed->restored
			    && observed->target_result == DSSH_ERROR_INIT && observed->peer_result == 0
			    && allocation_kex_result_empty(target) && !dssh_session_is_terminated(target);
		}
		dssh_cleanse(allocation_guard_hash, sizeof(allocation_guard_hash));
	}
	allocation_cleanup(&owner);
	allocation_cleanup(&owner);
	observed->created = owner.pair.created;
	observed->joined = owner.pair.joined;
	observed->retired = allocation_pair_retired(&owner);
	observed->valid = observed->valid && observed->retired
	    && observed->created == 3 && observed->joined == 3;
	return ret;
}

/* Private invalid-context models, not a public way to change a frozen
 * registration. The real server keeps its immutable registered descriptor. */
static int
allocation_verification_guard(unsigned guard)
{
	static const char *phases[] = {"before", "guard", "after"};
	bool valid = true;
	for (unsigned phase = 0; phase < 3; phase++) {
		unsigned mode = phase == 1 ? guard : 0;
		struct allocation_kex_observation observed;
		int ret = allocation_kex_case(true, false, -1, mode, &observed);
		bool checked = ret == 0 && observed.valid && observed.restored
		    && (mode != 0 || observed.completed);
		if (printf("allocation_verify method=%s key=%s phase=%s mode=%u target=%d peer=%d hashes=%u digest=%zu matched=%u restored=%u created=%u joined=%u retired=%u valid=%u\n",
		        test_kex_algo_name(), test_key_algo_name(), phases[phase], mode,
		        observed.target_result, observed.peer_result, observed.hashes, observed.digest,
		        observed.matched ? 1U : 0U, observed.restored ? 1U : 0U,
		        observed.created, observed.joined, observed.retired ? 1U : 0U, checked ? 1U : 0U) < 0)
			checked = false;
		valid = valid && checked;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_kex_missing_key(void)
{
	return allocation_verification_guard(1);
}

static int
test_kex_missing_verify(void)
{
	return allocation_verification_guard(2);
}

/* The controlled executable wraps actual native creates/joins. Only completed
 * negotiation precedes the third actor; every failure retires that real prefix. */
static int
allocation_direct_owner_control(void)
{
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool plain = mode == NULL || strcmp(mode, "plain") == 0;
	struct allocation_kex_observation observed;
	int ret = allocation_kex_case(true, false, -1, 0, &observed);
	bool valid = plain ? ret == 0 && observed.completed && observed.valid : ret < 0;
	valid = valid && observed.retired && observed.created == observed.joined;
	if (printf("allocation_direct_control created=%u joined=%u retired=%u completed=%u fixture=%d valid=%u\n",
	        observed.created, observed.joined, observed.retired ? 1U : 0U,
	        observed.completed ? 1U : 0U, ret, valid ? 1U : 0U) < 0)
		return 2;
	return valid ? 0 : 2;
}

static int
allocation_direct_assertion_control(unsigned phase)
{
	struct allocation_pair owner;
	int ret = allocation_prepare(&owner, false);
	owner.pair.operation = allocation_kex_negotiate;
	if (ret == 0 && phase >= 1)
		ret = test_pair_start(&owner.pair, 0);
	if (ret == 0 && phase >= 2) {
		ret = test_pair_start(&owner.pair, 1);
		if (ret == 0) {
			test_pair_join(&owner.pair);
			ret = owner.pair.client_result != 0 ? owner.pair.client_result : owner.pair.server_result;
		}
	}
	if (ret == 0 && phase >= 3) {
		owner.pair.operation = allocation_kex_excluded_peer;
		ret = test_pair_start(&owner.pair, 1);
	}
	if (ret != 0) {
		allocation_cleanup(&owner);
		return 2;
	}
	ret = allocation_assertion_failure();
	bool retired = allocation_pair_retired(&owner);
	unsigned created = owner.pair.created;
	unsigned joined = owner.pair.joined;
	bool valid = ret == TEST_FAIL && retired && created == phase && joined == phase;
	allocation_cleanup(&owner);
	if (printf("allocation_direct_assertion phase=%u created=%u joined=%u retired=%u valid=%u\n",
	        phase, created, joined, retired ? 1U : 0U, valid ? 1U : 0U) < 0)
		return 2;
	return valid ? 1 : 2;
}

static int
allocation_kex_sweep(bool client, bool provider)
{
	const char *name = provider ? (client ? "ossl/kex_client" : "ossl/kex_server")
	    : (client ? "alloc/kex_client" : "alloc/kex_server");
	struct allocation_kex_observation observed;
	int ret = allocation_kex_case(client, provider, -1, 0, &observed);
	if (ret != 0 || !observed.completed || !observed.valid)
		return TEST_FAIL;
	if (printf("allocation_kex_preflight name=%s method=%s key=%s created=3 joined=3 equal=1 retired=1 valid=1\n",
	        name, test_kex_algo_name(), test_key_algo_name()) < 0)
		return TEST_FAIL;
	struct allocation_sweep sweep = {0};
	/* This retains the earlier largest iteration bound, independently of any
	 * wall-clock timeout. Completion still requires an uninjected success. */
	while (sweep.next < 100000) {
		int point = sweep.next;
		ret = allocation_kex_case(client, provider, point, 0, &observed);
		if (ret != 0)
			return TEST_FAIL;
		if (printf("allocation_kex_owner name=%s point=%d target=%d peer=%d created=%u joined=%u equal=%u retired=1 valid=%u\n",
		        name, point, observed.target_result, observed.peer_result,
		        observed.created, observed.joined, observed.completed ? 1U : 0U,
		        observed.valid ? 1U : 0U) < 0)
			return TEST_FAIL;
		int step = allocation_sweep_step(&sweep, name, point, observed.calls,
		    observed.completed, observed.valid);
		if (step < 0)
			return TEST_FAIL;
		if (step > 0)
			return TEST_PASS;
	}
	return TEST_FAIL;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_ossl_kex_server_iterate(void)
{
	return allocation_kex_sweep(false, true);
}

#endif /* DSSH_CRYPTO_OPENSSL -- ossl kex_server iterate */

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_ossl_kex_client_iterate(void)
{
	return allocation_kex_sweep(true, true);
}
#endif /* DSSH_CRYPTO_OPENSSL -- ossl iterate tests */

/* ================================================================
 * Client-side parse/validate tests.  A "bad server" thread sends
 * crafted packets to exercise error paths in the client handler.
 * ================================================================ */

enum allocation_parser_stage {
	PARSER_CURVE_REPLY,
	PARSER_GEX_GROUP,
	PARSER_GEX_REPLY,
	PARSER_CURVE_INIT
};

struct allocation_parser_peer {
	struct mock_io_state *io;
	dssh_session session;
	enum allocation_parser_stage stage;
	const uint8_t *reply;
	size_t reply_len;
	const char *model;
	struct dssh_result_lease producer;
	unsigned requests;
	unsigned sends;
	unsigned groups;
	unsigned releases;
	int result;
	bool model_hit;
};

struct allocation_parser_observation {
	int result;
	int peer_result;
	unsigned created;
	unsigned joined;
	bool delivered;
	bool retired;
	bool model_hit;
};

static void
allocation_parser_group_release(void *opaque)
{
	struct allocation_parser_peer *peer = opaque;
	/* One selected group owns one producer lease on this worker. */
	peer->releases++;
	dssh_result_release(&peer->producer);
}

static int
allocation_parser_send(struct allocation_parser_peer *peer, const uint8_t *data, size_t length)
{
	int ret = send_packet(peer->session, data, length, NULL);
	if (ret == 0)
		peer->sends++;
	return ret;
}

static int
allocation_parser_put_mpint(struct dssh_bytes bytes, uint8_t *buffer, size_t capacity, size_t *pos)
{
	if (bytes.data == NULL || bytes.len == 0 || bytes.len > 1024 || *pos > capacity)
		return DSSH_ERROR_INVALID;
	bool pad = (bytes.data[0] & 0x80) != 0;
	size_t encoded = bytes.len + (pad ? 1U : 0U); /* At most1025. */
	if (capacity - *pos < 4 || encoded > capacity - *pos - 4)
		return DSSH_ERROR_TOOLONG;
	uint32_t count = (uint32_t)encoded;
	int ret = dssh_serialize_uint32(count, buffer, capacity, pos);
	if (ret < 0)
		return ret;
	if (pad)
		buffer[(*pos)++] = 0;
	memcpy(buffer + *pos, bytes.data, bytes.len);
	*pos += bytes.len;
	return 0;
}

static int
allocation_parser_request(struct allocation_parser_peer *peer, bool init)
{
	uint8_t type;
	uint8_t *payload;
	size_t length;
	int ret = recv_packet(peer->session, &type, &payload, &length);
	if (ret != 0)
		return ret;
	uint8_t wanted = peer->stage == PARSER_CURVE_REPLY ? UINT8_C(30)
	    : init ? UINT8_C(32) : UINT8_C(34);
	if (strcmp(peer->model, "bad-request") == 0) {
		peer->model_hit = true;
		wanted = 34;
	}
	if (payload == NULL || length == 0 || type != wanted || payload[0] != type)
		return DSSH_ERROR_PARSE;
	if (wanted == 34) {
		if (length != 13)
			return DSSH_ERROR_PARSE;
		static const uint32_t expected[] = {2048, 4096, 8192};
		for (size_t i = 0; i < 3; i++) {
			uint32_t value;
			if (dssh_parse_uint32(payload + 1 + i * 4, 4, &value) != 4 || value != expected[i])
				return DSSH_ERROR_PARSE;
		}
	}
	else {
		uint32_t count;
		if (length < 5 || dssh_parse_uint32(payload + 1, length - 1, &count) != 4
		    || count != length - 5)
			return DSSH_ERROR_PARSE;
		if (wanted == 30) {
			if (count != 32)
				return DSSH_ERROR_PARSE;
		}
		else if (count == 0 || count > 1025 || (payload[5] & 0x80) != 0
		    || (payload[5] == 0 && (count < 2 || (payload[6] & 0x80) == 0)))
			return DSSH_ERROR_PARSE;
	}
	peer->requests++;
	return 0;
}

static int
allocation_parser_worker(void *opaque)
{
	struct allocation_parser_peer *peer = opaque;
	struct dssh_dh_gex_group_result group = {0};
	dssh_test_ossl_exclude_thread();
	dssh_test_alloc_exclude_thread();
	peer->result = 0;
	if (peer->stage != PARSER_CURVE_INIT)
		peer->result = allocation_parser_request(peer, false);
	if (peer->result != 0)
		goto done;
	if (peer->stage == PARSER_GEX_REPLY) {
		dssh_kex method = peer->session->trans.kex_selected;
		struct dssh_dh_gex_provider *provider = method->config;
		if (provider == NULL || provider->select_group == NULL) {
			peer->result = DSSH_ERROR_INVALID;
			goto done;
		}
		peer->result = provider->select_group(2048, 4096, 8192, &group, provider->cbdata);
		if (group.lease.release != NULL) {
			/* Move this producer lease into a forwarding observation. The
			 * original result and both records stay live through release. */
			peer->producer = group.lease;
			group.lease = (struct dssh_result_lease){allocation_parser_group_release, peer};
			peer->groups++;
		}
		if (strcmp(peer->model, "provider-error") == 0) {
			peer->model_hit = true;
			peer->result = DSSH_ERROR_INIT;
		}
		if (peer->result != 0)
			goto done;
		if (peer->groups != 1 || group.p.data == group.g.data) {
			peer->result = DSSH_ERROR_INVALID;
			goto done;
		}
		uint8_t message[2 * 1024 + 11] = {31};
		size_t pos = 1;
		peer->result = allocation_parser_put_mpint(group.p, message, sizeof(message), &pos);
		if (peer->result == 0)
			peer->result = allocation_parser_put_mpint(group.g, message, sizeof(message), &pos);
		dssh_result_release(&group.lease);
		if (peer->result == 0)
			peer->result = allocation_parser_send(peer, message, pos);
		if (peer->result == 0)
			peer->result = allocation_parser_request(peer, true);
		if (peer->result != 0)
			goto done;
	}
	if (strcmp(peer->model, "skip-reply") == 0)
		peer->model_hit = true;
	else if (peer->reply_len != 0)
		peer->result = allocation_parser_send(peer, peer->reply, peer->reply_len);
done:
	dssh_result_release(&group.lease);
	/* Preserve queued bytes and the receiver. Full cancellation belongs to
	 * the enclosing owner, after the target's result has been observed. */
	if (peer->stage == PARSER_CURVE_INIT)
		mock_io_close_c2s_write(peer->io);
	else
		mock_io_close_s2c_write(peer->io);
	return 0;
}

static int
allocation_parser_start(struct allocation_pair *owner, struct allocation_parser_peer *peer)
{
	unsigned side = peer->stage == PARSER_CURVE_INIT ? 0U : 1U;
	if (owner->pair.owned[side] || owner->pair.created == UINT_MAX)
		return DSSH_ERROR_INVALID;
	peer->io = &owner->pair.io;
	peer->session = side == 0 ? owner->pair.client : owner->pair.server;
	if (thrd_create(&owner->pair.threads[side], allocation_parser_worker, peer) != thrd_success)
		return DSSH_ERROR_INIT;
	owner->pair.owned[side] = true;
	owner->pair.created++;
	return 0;
}

static bool
allocation_parser_payload(dssh_session target, const uint8_t *reply, size_t length)
{
	if (target->trans.rx_packet == NULL || target->trans.packet_buf_sz < 9
	    || length > target->trans.packet_buf_sz - 9)
		return false;
	uint32_t packet;
	if (dssh_parse_uint32(target->trans.rx_packet + 4, 4, &packet) != 4)
		return false;
	uint32_t padding = target->trans.rx_packet[8];
	if (packet < padding + 1 || packet - padding - 1 != length)
		return false;
	return memcmp(target->trans.rx_packet + 9, reply, length) == 0;
}

static int
allocation_parser_case(enum allocation_parser_stage stage, const uint8_t *reply,
    size_t length, int expected, const char *model, struct allocation_parser_observation *observed)
{
	*observed = (struct allocation_parser_observation){.result = DSSH_ERROR_INIT,
	    .peer_result = DSSH_ERROR_INIT};
	struct allocation_pair owner;
	struct allocation_parser_peer peer = {.stage = stage, .reply = reply,
	    .reply_len = length, .model = model, .result = DSSH_ERROR_INIT};
	int ret = allocation_prepare(&owner, false);
	if (ret == 0)
		ret = test_pair_run(&owner.pair, allocation_kex_negotiate);
	const char *method = stage == PARSER_GEX_GROUP || stage == PARSER_GEX_REPLY
	    ? "diffie-hellman-group-exchange-sha256" : "curve25519-sha256";
	if (ret == 0) {
		bool selected = owner.pair.client->trans.kex_selected != NULL
		    && owner.pair.server->trans.kex_selected != NULL
		    && strcmp(owner.pair.client->trans.kex_selected->name, method) == 0
		    && strcmp(owner.pair.server->trans.kex_selected->name, method) == 0
		    && owner.pair.client->trans.key_algo_selected != NULL
		    && owner.pair.server->trans.key_algo_selected != NULL
		    && strcmp(owner.pair.client->trans.key_algo_selected->name, test_key_algo_name()) == 0
		    && strcmp(owner.pair.server->trans.key_algo_selected->name, test_key_algo_name()) == 0;
		if (!selected)
			ret = DSSH_ERROR_INVALID;
	}
	dssh_session target = stage == PARSER_CURVE_INIT ? owner.pair.server : owner.pair.client;
	uint32_t before = ret == 0 ? target->trans.rx_seq : 0;
	if (ret == 0)
		ret = allocation_parser_start(&owner, &peer);
	if (ret == 0) {
		observed->result = kex(target);
		unsigned sends = (stage == PARSER_GEX_REPLY ? 1U : 0U) + (length != 0 ? 1U : 0U);
		bool packets = target->trans.rx_seq >= before && target->trans.rx_seq - before == sends;
		observed->delivered = packets && (length == 0
		    || allocation_parser_payload(target, reply, length));
		bool target_valid = observed->result == expected && allocation_kex_result_empty(target)
		    && dssh_session_is_terminated(target) == (length == 0);
		/* Complete receipt proves the responder has reached its final send.
		 * Let that send return before termination can alter its result. EOF
		 * responders already close only after finishing their prerequisites;
		 * incomplete/unexpected paths still need cancellation to unblock. */
		if (!observed->delivered || length == 0)
			test_pair_cancel(&owner.pair);
		test_pair_join(&owner.pair);
		observed->peer_result = peer.result;
		unsigned requests = stage == PARSER_CURVE_INIT ? 0U : stage == PARSER_GEX_REPLY ? 2U : 1U;
		unsigned groups = stage == PARSER_GEX_REPLY ? 1U : 0U;
		if (peer.result != 0 || !target_valid || !observed->delivered || peer.requests != requests
		    || peer.sends != sends || peer.groups != groups || peer.releases != groups)
			ret = DSSH_ERROR_INVALID;
	}
	allocation_cleanup(&owner);
	allocation_cleanup(&owner);
	observed->created = owner.pair.created;
	observed->joined = owner.pair.joined;
	observed->retired = allocation_pair_retired(&owner);
	observed->model_hit = peer.model_hit;
	if (ret == 0 && (!observed->retired || observed->created != 3 || observed->joined != 3))
		ret = DSSH_ERROR_INVALID;
	unsigned stage_id = (unsigned)stage; /* One of the four private stage constants. */
	if (printf("allocation_parser stage=%u method=%s key=%s bytes=%zu result=%d expected=%d peer=%d requests=%u sends=%u groups=%u releases=%u delivered=%u created=%u joined=%u retired=%u fixture=%d\n",
	        stage_id, method, test_key_algo_name(), length, observed->result, expected, peer.result,
	        peer.requests, peer.sends, peer.groups, peer.releases, observed->delivered ? 1U : 0U,
	        observed->created, observed->joined, observed->retired ? 1U : 0U, ret) < 0)
		ret = DSSH_ERROR_INIT;
	return ret;
}

static int
allocation_parser_owner_control(void)
{
	const char *mode = getenv("DSSH_PEER_FAILURE");
	if (mode == NULL)
		mode = "plain";
	bool plain = strcmp(mode, "plain") == 0;
	bool model = strcmp(mode, "skip-reply") == 0 || strcmp(mode, "bad-request") == 0
	    || strcmp(mode, "provider-error") == 0;
	enum allocation_parser_stage stage = strcmp(mode, "provider-error") == 0
	    ? PARSER_GEX_REPLY : PARSER_CURVE_REPLY;
	static const uint8_t reply[] = {31};
	struct allocation_parser_observation observed;
	int ret = allocation_parser_case(stage, reply, sizeof(reply), DSSH_ERROR_PARSE, mode, &observed);
	bool valid = plain ? ret == 0 : ret < 0;
	valid = valid && observed.retired && observed.created == observed.joined
	    && (!model || (observed.model_hit && observed.created == 3));
	if (model && printf("allocation_parser_model mode=%s hit=%u\n", mode,
	    observed.model_hit ? 1U : 0U) < 0)
		return 2;
	if (printf("allocation_parser_control created=%u joined=%u retired=%u completed=%u fixture=%d valid=%u\n",
	        observed.created, observed.joined, observed.retired ? 1U : 0U,
	        ret == 0 ? 1U : 0U, ret, valid ? 1U : 0U) < 0)
		return 2;
	return valid ? 0 : 2;
}

static int
allocation_parser_assertion_control(unsigned phase)
{
	struct allocation_pair owner;
	struct allocation_parser_peer peer = {.stage = PARSER_CURVE_REPLY, .model = "plain"};
	int ret = allocation_prepare(&owner, false);
	owner.pair.operation = allocation_kex_negotiate;
	if (ret == 0 && phase >= 1)
		ret = test_pair_start(&owner.pair, 0);
	if (ret == 0 && phase >= 2) {
		ret = test_pair_start(&owner.pair, 1);
		if (ret == 0) {
			test_pair_join(&owner.pair);
			ret = owner.pair.client_result != 0 ? owner.pair.client_result : owner.pair.server_result;
		}
	}
	if (ret == 0 && phase >= 3)
		ret = allocation_parser_start(&owner, &peer);
	if (ret != 0) {
		allocation_cleanup(&owner);
		return 2;
	}
	ret = allocation_assertion_failure();
	bool retired = allocation_pair_retired(&owner);
	unsigned created = owner.pair.created;
	unsigned joined = owner.pair.joined;
	bool valid = ret == TEST_FAIL && retired && created == phase && joined == phase;
	allocation_cleanup(&owner);
	if (printf("allocation_parser_assertion phase=%u created=%u joined=%u retired=%u valid=%u\n",
	        phase, created, joined, retired ? 1U : 0U, valid ? 1U : 0U) < 0)
		return 2;
	return valid ? 1 : 2;
}

static int
dhgex_client_parse_test(enum allocation_parser_stage stage, const uint8_t *reply, size_t length, int expected)
{
	struct allocation_parser_observation observed;
	return allocation_parser_case(stage, reply, length, expected, "plain", &observed);
}

static int
c25519_client_parse_test(const uint8_t *reply, size_t length)
{
	struct allocation_parser_observation observed;
	return allocation_parser_case(PARSER_CURVE_REPLY, reply, length, DSSH_ERROR_PARSE, "plain", &observed);
}

/* Line 220: recv GEX_GROUP fails (server closes before sending) */
static int
test_dhgex_client_recv_group_fail(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* Empty reply + close -> recv fails */
	int res = dhgex_client_parse_test(PARSER_GEX_GROUP, NULL, 0, DSSH_ERROR_PARSE);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 226: GEX_GROUP payload empty after msg_type */
static int
test_dhgex_client_group_empty(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	uint8_t group[1] = { 31 }; /* GEX_GROUP, no p or g */
	int res = dhgex_client_parse_test(PARSER_GEX_GROUP, group, 1, DSSH_ERROR_PARSE);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 230: GEX_GROUP has p but no g */
static int
test_dhgex_client_group_no_g(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_GROUP with p=2 but no g */
	uint8_t group[16];
	size_t gp = 0;
	group[gp++] = 31;
	ASSERT_EQ(dssh_serialize_uint32(1, group, sizeof(group), &gp), 0);
	group[gp++] = 0x02; /* mpint p=2 */
	int res = dhgex_client_parse_test(PARSER_GEX_GROUP, group, gp, DSSH_ERROR_PARSE);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Server returns |p| below client_min (GEX_MIN=2048) -> client must reject */
static int
test_dhgex_client_group_too_small(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* A canonical positive768-bit p needs one sign byte plus96 data bytes. */
	uint8_t group[1 + 4 + 97 + 4 + 1];
	size_t  gp = 0;
	group[gp++] = 31; /* GEX_GROUP */
	ASSERT_EQ(dssh_serialize_uint32(97, group, sizeof(group), &gp), 0);
	group[gp++] = 0;
	group[gp] = 0x80;
	memset(&group[gp + 1], 0xff, 95);
	gp += 96;
	ASSERT_EQ(dssh_serialize_uint32(1, group, sizeof(group), &gp), 0);
	group[gp++] = 0x02; /* g = 2 */
	int res = dhgex_client_parse_test(PARSER_GEX_GROUP, group, gp, DSSH_ERROR_INVALID);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 264: wrong msg_type in GEX_REPLY */
static int
test_dhgex_client_bad_reply_type(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* Send a GEX_INIT(32) instead of GEX_REPLY(33) */
	uint8_t reply[8];
	size_t rp = 0;
	reply[rp++] = 32; /* wrong type */
	ASSERT_EQ(dssh_serialize_uint32(1, reply, sizeof(reply), &rp), 0);
	reply[rp++] = 0x02;
	int res = dhgex_client_parse_test(PARSER_GEX_REPLY, reply, rp, DSSH_ERROR_PARSE);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 268: GEX_REPLY too short for K_S length field */
static int
test_dhgex_client_reply_short_ks(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY with only msg_type byte -- no K_S length field */
	uint8_t reply[1] = { 33 };
	int res = dhgex_client_parse_test(PARSER_GEX_REPLY, reply, 1, DSSH_ERROR_PARSE);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 270: GEX_REPLY with K_S length overrunning payload */
static int
test_dhgex_client_reply_truncated_ks(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY with K_S length claiming 1000 bytes but only 2 present */
	uint8_t reply[16];
	size_t rp = 0;
	reply[rp++] = 33; /* GEX_REPLY */
	ASSERT_EQ(dssh_serialize_uint32(1000, reply, sizeof(reply), &rp), 0);
	reply[rp++] = 0x01;
	reply[rp++] = 0x02;
	int res = dhgex_client_parse_test(PARSER_GEX_REPLY, reply, rp, DSSH_ERROR_PARSE);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 280: GEX_REPLY with f=0 (invalid DH value) */
static int
test_dhgex_client_reply_f_zero(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY: K_S="", f=0, sig="" */
	uint8_t reply[32];
	size_t rp = 0;
	reply[rp++] = 33; /* GEX_REPLY */
	/* K_S: empty string (len=0) */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0);
	/* f: mpint 0 (len=0) */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0);
	/* Complete signature framing precedes the mathematical f check. */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0);
	int res = dhgex_client_parse_test(PARSER_GEX_REPLY, reply, rp, DSSH_ERROR_INVALID);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 289: GEX_REPLY too short for sig length field */
static int
test_dhgex_client_reply_short_sig(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY: K_S="", f=2 (valid), but payload ends before sig len */
	uint8_t reply[16];
	size_t rp = 0;
	reply[rp++] = 33; /* GEX_REPLY */
	/* K_S: empty string (len=0) */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0);
	/* f: mpint 2 */
	ASSERT_EQ(dssh_serialize_uint32(1, reply, sizeof(reply), &rp), 0);
	reply[rp++] = 0x02;
	/* No sig at all -- payload ends here */
	int res = dhgex_client_parse_test(PARSER_GEX_REPLY, reply, rp, DSSH_ERROR_PARSE);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 291: GEX_REPLY with sig length overrunning payload */
static int
test_dhgex_client_reply_truncated_sig(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY: K_S="", f=2 (valid), sig claiming 1000 bytes */
	uint8_t reply[32];
	size_t rp = 0;
	reply[rp++] = 33; /* GEX_REPLY */
	/* K_S: empty string */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0);
	/* f: mpint 2 */
	ASSERT_EQ(dssh_serialize_uint32(1, reply, sizeof(reply), &rp), 0);
	reply[rp++] = 0x02;
	/* sig: claiming 1000 bytes but message ends here */
	ASSERT_EQ(dssh_serialize_uint32(1000, reply, sizeof(reply), &rp), 0);
	int res = dhgex_client_parse_test(PARSER_GEX_REPLY, reply, rp, DSSH_ERROR_PARSE);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* ================================================================
 * Curve25519 client parse tests
 *
 * Equivalent of the DH-GEX client parse tests above, but for the
 * curve25519-sha256 KEX.  The "bad server" reads ECDH_INIT from
 * the client, sends a crafted ECDH_REPLY, then closes.
 * ================================================================ */

/* Line 257: recv ECDH_REPLY fails (server closes before sending) */
static int
test_c25519_client_recv_reply_fail(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;
	/* Empty reply + close -> recv fails */
	int res = c25519_client_parse_test(NULL, 0);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 263: ECDH_REPLY too short for K_S length field */
static int
test_c25519_client_reply_short_ks(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;
	/* ECDH_REPLY with only msg_type byte -- no K_S length field */
	uint8_t reply[1] = { 31 }; /* SSH_MSG_KEX_ECDH_REPLY */
	int res = c25519_client_parse_test(reply, 1);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 264: ECDH_REPLY with K_S length overrunning payload */
static int
test_c25519_client_reply_trunc_ks(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;
	/* K_S length claiming 1000 bytes but only 2 present */
	uint8_t reply[16];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	ASSERT_EQ(dssh_serialize_uint32(1000, reply, sizeof(reply), &rp), 0);
	reply[rp++] = 0x01;
	reply[rp++] = 0x02;
	int res = c25519_client_parse_test(reply, rp);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 271: ECDH_REPLY with K_S="" but end before Q_S length field */
static int
test_c25519_client_reply_short_qs(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;
	/* K_S: empty string (len=0), payload ends before Q_S len */
	uint8_t reply[8];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0);
	int res = c25519_client_parse_test(reply, rp);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 277: ECDH_REPLY with Q_S length != 32 */
static int
test_c25519_client_reply_bad_qs_len(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;
	/* K_S="" + Q_S with len=16 (not 32) */
	uint8_t reply[32];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0); /* K_S: empty */
	ASSERT_EQ(dssh_serialize_uint32(16, reply, sizeof(reply), &rp), 0); /* Q_S len=16 */
	memset(&reply[rp], 0x42, 16);
	rp += 16;
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0); /* complete sig */
	int res = c25519_client_parse_test(reply, rp);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 277:35: ECDH_REPLY with K_S="" + Q_S len=32 but only 10 bytes present */
static int
test_c25519_client_reply_qs_overrun(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;
	uint8_t reply[32];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0); /* K_S: empty */
	ASSERT_EQ(dssh_serialize_uint32(32, reply, sizeof(reply), &rp), 0); /* Q_S len=32 */
	memset(&reply[rp], 0x42, 10); /* only 10 bytes of Q_S data */
	rp += 10;
	int res = c25519_client_parse_test(reply, rp);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 283: ECDH_REPLY with K_S="" + Q_S(32 bytes) but no sig */
static int
test_c25519_client_reply_short_sig(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;
	/* K_S="" + Q_S(32 bytes) + payload ends before sig len */
	uint8_t reply[48];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0); /* K_S: empty */
	ASSERT_EQ(dssh_serialize_uint32(32, reply, sizeof(reply), &rp), 0); /* Q_S len=32 */
	memset(&reply[rp], 0x42, 32);
	rp += 32;
	/* No sig at all -- payload ends here */
	int res = c25519_client_parse_test(reply, rp);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* Line 289: ECDH_REPLY with sig length overrunning payload */
static int
test_c25519_client_reply_trunc_sig(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;
	/* K_S="" + Q_S(32 bytes) + sig claiming 1000 bytes */
	uint8_t reply[64];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0); /* K_S: empty */
	ASSERT_EQ(dssh_serialize_uint32(32, reply, sizeof(reply), &rp), 0); /* Q_S len=32 */
	memset(&reply[rp], 0x42, 32);
	rp += 32;
	/* sig: claiming 1000 bytes but message ends here */
	ASSERT_EQ(dssh_serialize_uint32(1000, reply, sizeof(reply), &rp), 0);
	int res = c25519_client_parse_test(reply, rp);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

/* ================================================================
 * Library alloc injection iterates for KEX server/client.
 *
 * These cover malloc failures in serialize_bn_mpint, shared_secret,
 * reply buffer, and exchange_hash inside the DH-GEX handler.
 * The ossl iterates only inject OpenSSL failures; these inject
 * library malloc failures via dssh_test_alloc_fail_after().
 * ================================================================ */

static int
test_alloc_kex_server_iterate(void)
{
	return allocation_kex_sweep(false, false);
}

/*
 * alloc/kex_client: library alloc injection on the client during
 * a two-threaded DH-GEX handshake.  The server thread excludes
 * itself from alloc injection via dssh_test_alloc_exclude_thread().
 */

static int
test_alloc_kex_client_iterate(void)
{
	return allocation_kex_sweep(true, false);
}

static int
test_c25519_server_truncated_init(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;
	static const uint8_t init[] = {SSH_MSG_KEX_ECDH_INIT};
	struct allocation_parser_observation observed;
	int ret = allocation_parser_case(PARSER_CURVE_INIT, init, sizeof(init),
	    DSSH_ERROR_PARSE, "plain", &observed);
	ASSERT_EQ(ret, 0);
	return TEST_PASS;
}

/* ================================================================
 * curve25519: client receives ECDH_REPLY with bad Q_S length
 * ================================================================ */

static int
test_c25519_client_bad_qs_length(void)
{
	if (strcmp(test_kex_algo_name(), "curve25519-sha256") != 0)
		return TEST_SKIP;

	/* K_S="" + Q_S with len=0 (not 32), enough data present */
	uint8_t reply[16];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0); /* K_S: empty */
	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0); /* Q_S len=0 */

	ASSERT_EQ(dssh_serialize_uint32(0, reply, sizeof(reply), &rp), 0); /* complete sig */
	int res = c25519_client_parse_test(reply, rp);
	ASSERT_EQ(res, 0);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
/* The named call is observed after checked negotiation, separately from the
 * generic provider countdown. Fresh method frames own every failure case;
 * native retained-owner recovery is qualified in test_dh_rng.c. */
static int
test_dh_private_rng_fail(void)
{
	const char *method = test_kex_algo_name();
	if (strcmp(method, "diffie-hellman-group-exchange-sha256") != 0
	    && strcmp(method, "diffie-hellman-group14-sha256") != 0
	    && strcmp(method, "diffie-hellman-group16-sha512") != 0)
		return TEST_SKIP;
	static const unsigned modes[] = {0, 1, 0, 2, 0};
	bool valid = true;
	for (unsigned role = 0; role < 2; role++) {
		for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
			unsigned mode = modes[i];
			struct allocation_kex_observation observed;
			allocation_kex_random_observe = true;
			allocation_kex_random_mode = mode;
			int ret = allocation_kex_case(role == 0, true, -1, 0, &observed);
			allocation_kex_random_observe = false;
			const struct dssh_test_bn_random_stats *random = &observed.random;
			bool checked = ret == 0 && observed.valid && random->calls == 1
			    && random->strength == 256 && random->range_bits >= 2047
			    && random->context_present && random->constant_time
			    && random->native_called == (mode != 1)
			    && random->native_completed == (mode != 1)
			    && random->in_range == (mode != 1)
			    && (mode == 0 ? observed.completed : observed.target_result < 0 && !observed.completed);
			if (printf("dh_random_method method=%s key=%s role=%u phase=%zu mode=%u calls=%u strength=%u native=%u completed=%u valid=%u\n",
			        method, test_key_algo_name(), role, i, mode, random->calls,
			        random->strength, random->native_called ? 1U : 0U,
			        observed.completed ? 1U : 0U, checked ? 1U : 0U) < 0)
				checked = false;
			valid = valid && checked;
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#endif /* DSSH_CRYPTO_OPENSSL -- named DH randomness */

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	{ "alloc/fixture_sync", test_allocation_fixture_sync },
	{ "alloc/fixture_watchdog", test_allocation_fixture_watchdog },
	{ "alloc/fixture_owner", test_allocation_fixture_owner },
	/* Every builtin registration constructor. */
	{ "alloc/register_curve25519_sha256", test_alloc_register_curve25519_sha256 },
	{ "alloc/register_dh_gex_sha256", test_alloc_register_dh_gex_sha256 },
	{ "alloc/register_dh_group14_sha256", test_alloc_register_dh_group14_sha256 },
	{ "alloc/register_dh_group16_sha512", test_alloc_register_dh_group16_sha512 },
	{ "alloc/register_sntrup761x25519_sha512", test_alloc_register_sntrup761x25519_sha512 },
	{ "alloc/register_mlkem768x25519_sha256", test_alloc_register_mlkem768x25519_sha256 },
	{ "alloc/register_ssh_ed25519", test_alloc_register_ssh_ed25519 },
	{ "alloc/register_rsa_sha2_256", test_alloc_register_rsa_sha2_256 },
	{ "alloc/register_rsa_sha2_512", test_alloc_register_rsa_sha2_512 },
	{ "alloc/register_aes256_ctr", test_alloc_register_aes256_ctr },
	{ "alloc/register_aes128_cbc", test_alloc_register_aes128_cbc },
	{ "alloc/register_none_enc", test_alloc_register_none_enc },
	{ "alloc/register_hmac_sha2_256", test_alloc_register_hmac_sha2_256 },
	{ "alloc/register_hmac_sha2_512", test_alloc_register_hmac_sha2_512 },
	{ "alloc/register_none_mac", test_alloc_register_none_mac },
	{ "alloc/register_none_comp", test_alloc_register_none_comp },

	{ "alloc/sweep_contract",         test_alloc_sweep_contract },

	/* Session init */
	{ "alloc/session_init",           test_alloc_session_init },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "ossl/session_init",            test_ossl_session_init },
#endif


	/* Auth */
	{ "guard/kbi_null_callback",       test_kbi_null_callback },

	/* Server auth */

	/* Connection protocol */
	{ "guard/acceptqueue",             test_alloc_acceptqueue_push },

	/* Handshake kexinit malloc */
	{ "alloc/kexinit",                test_alloc_kexinit },

	/* Iterative handshake (library-only allocator) */
	{ "alloc/handshake_iterate",      test_alloc_handshake_iterate },

	/* Iterative auth (library-only allocator) */
	{ "alloc/auth_iterate",           test_alloc_auth_iterate },

	/* Iterative conn (library-only allocator) */
	{ "alloc/conn_iterate",           test_alloc_conn_iterate },

	/* Iterative OpenSSL/C11 failure injection */
#ifdef DSSH_CRYPTO_OPENSSL
	{ "ossl/handshake_iterate",       test_ossl_handshake_iterate },
	{ "ossl/key_sign_verify",         test_ossl_key_sign_verify_iterate },
	{ "ossl/keygen",                  test_ossl_keygen_iterate },
	{ "ossl/enc",                     test_ossl_enc_iterate },
	{ "ossl/mac",                     test_ossl_mac_iterate },
	{ "ossl/key_verify",              test_ossl_key_verify_iterate },
	{ "ossl/key_pubkey",              test_ossl_key_pubkey_iterate },
	{ "ossl/kex_server",              test_ossl_kex_server_iterate },
	{ "ossl/kex_client",              test_ossl_kex_client_iterate },
#endif
	{ "guard/kex_missing_key", test_kex_missing_key },
	{ "guard/kex_missing_verify", test_kex_missing_verify },
	{ "dhgex/client_recv_group_fail", test_dhgex_client_recv_group_fail },
	{ "dhgex/client_group_empty",     test_dhgex_client_group_empty },
	{ "dhgex/client_group_no_g",      test_dhgex_client_group_no_g },
	{ "dhgex/client_group_too_small", test_dhgex_client_group_too_small },
	{ "dhgex/client_bad_reply_type",  test_dhgex_client_bad_reply_type },
	{ "dhgex/client_short_ks",        test_dhgex_client_reply_short_ks },
	{ "dhgex/client_trunc_ks",        test_dhgex_client_reply_truncated_ks },
	{ "dhgex/client_reply_f_zero",    test_dhgex_client_reply_f_zero },
	{ "dhgex/client_short_sig",       test_dhgex_client_reply_short_sig },
	{ "dhgex/client_trunc_sig",       test_dhgex_client_reply_truncated_sig },
	{ "c25519/client_recv_reply",     test_c25519_client_recv_reply_fail },
	{ "c25519/client_short_ks",       test_c25519_client_reply_short_ks },
	{ "c25519/client_trunc_ks",       test_c25519_client_reply_trunc_ks },
	{ "c25519/client_short_qs",       test_c25519_client_reply_short_qs },
	{ "c25519/client_bad_qs_len",     test_c25519_client_reply_bad_qs_len },
	{ "c25519/client_qs_overrun",     test_c25519_client_reply_qs_overrun },
	{ "c25519/client_short_sig",      test_c25519_client_reply_short_sig },
	{ "c25519/client_trunc_sig",      test_c25519_client_reply_trunc_sig },
	{ "alloc/kex_server",             test_alloc_kex_server_iterate },
	{ "alloc/kex_client",             test_alloc_kex_client_iterate },
	{ "c25519/server_truncated_init", test_c25519_server_truncated_init },
	{ "c25519/client_bad_qs_length",  test_c25519_client_bad_qs_length },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "dh/private_rng_fail",          test_dh_private_rng_fail },
#endif
};

#define main allocation_test_main
DSSH_TEST_MAIN(tests)
#undef main

int
main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--allocation-parser-owner") == 0)
		return allocation_parser_owner_control();
	if (argc == 3 && strcmp(argv[1], "--allocation-parser-assert") == 0) {
		if (strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '3')
			return 2;
		unsigned phase = (unsigned)(argv[2][0] - '0');
		return allocation_parser_assertion_control(phase);
	}
	if (argc == 2 && strcmp(argv[1], "--allocation-direct-owner") == 0)
		return allocation_direct_owner_control();
	if (argc == 3 && strcmp(argv[1], "--allocation-direct-assert") == 0) {
		if (strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '3')
			return 2;
		unsigned phase = (unsigned)(argv[2][0] - '0');
		return allocation_direct_assertion_control(phase);
	}
	if (argc == 3 && strcmp(argv[1], "--allocation-assert") == 0) {
		if (strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '3')
			return 2;
		unsigned phase = (unsigned)(argv[2][0] - '0');
		return allocation_assertion_control(phase);
	}
	if (argc == 4 && strcmp(argv[1], "--allocation-sync-fatal") == 0)
		return allocation_sync_fatal(argv[2], argv[3]);
	return allocation_test_main(argc, argv);
}
