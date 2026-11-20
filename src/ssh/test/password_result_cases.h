#ifndef DSSH_TEST_PASSWORD_RESULT_CASES_H
#define DSSH_TEST_PASSWORD_RESULT_CASES_H

#include "password_result_provider.h"

struct password_release_probe {
	dssh_session session;
	unsigned calls;
	bool valid;
};

static void
password_release_probe(void *data)
{
	struct password_release_probe *probe = data;
	probe->calls++;
	probe->valid = probe->valid
	    && dssh_auth_set_banner(probe->session, "forbidden", NULL) == DSSH_ERROR_INVALID;
	int ret = mtx_trylock(&probe->session->trans.tx_mtx);
	if (ret == thrd_success) {
		int unlocked = mtx_unlock(&probe->session->trans.tx_mtx);
		probe->valid = probe->valid && unlocked == thrd_success;
	}
	else
		probe->valid = false;
	ret = mtx_trylock(&probe->session->trans.tx_queue_mtx);
	if (ret == thrd_success) {
		int unlocked = mtx_unlock(&probe->session->trans.tx_queue_mtx);
		probe->valid = probe->valid && unlocked == thrd_success;
	}
	else
		probe->valid = false;
}

static int
observed_password_result(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len, struct dssh_bytes_result *out, void *data)
{
	struct test_password_result_provider *producer = data;
	int ret = test_password_result(prompt, prompt_len, language, language_len, out, producer);
	if (auth_test_io_watch != NULL) {
		auth_test_io_watch->armed = true;
		auth_test_io_watch->secret = producer->storage;
		auth_test_io_watch->secret_len = sizeof(producer->storage);
		auth_test_io_watch->produced_results = &producer->calls;
		auth_test_io_watch->released_results = &producer->releases;
		if (producer->mode == 8) {
			auth_test_io_watch->fail_enabled = true;
			auth_test_io_watch->fail_at = auth_test_io_watch->calls;
		}
	}
	return ret;
}

static int
test_password_result_contract(void)
{
	const int expected[] = {0, -77, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID,
	    DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, 0, DSSH_ERROR_TERMINATED};
	for (unsigned mode = 0; mode < sizeof(expected) / sizeof(expected[0]); mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		ctx.client->auth_service_requested = true;
		unsigned rounds = mode == 0 ? 2 : 1;
		uint8_t request[9] = {SSH_MSG_USERAUTH_PASSWD_CHANGEREQ};
		bool valid = true;
		for (unsigned i = 0; i < rounds; i++)
			valid = valid && send_packet(ctx.server, request, sizeof(request), NULL) == 0;
		uint8_t success = SSH_MSG_USERAUTH_SUCCESS;
		valid = valid && send_packet(ctx.server, &success, 1, NULL) == 0;
		struct password_release_probe release = {.session = ctx.client, .valid = true};
		struct test_password_result_provider producer = {
		    .mode = mode, .lease_cleared = true, .previous_cleared = true,
		    .observe_release = password_release_probe, .observe_data = &release,
		};
		/* Application-owned inline storage; no producer or library allocation
		 * is needed for a repeated password-change result. */
		struct auth_test_io_watch watch = {.target = ctx.client, .clean = true};
		auth_test_io_watch = &watch;
		dssh_test_alloc_fail_after(0);
		int ret = valid ? dssh_auth_password(ctx.client, "u", "old", observed_password_result, &producer) : -999;
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		auth_test_io_watch = NULL;
		bool has_release = mode != 5 && mode != 6;
		unsigned release_count = has_release ? rounds : 0;
		valid = valid && ret == expected[mode] && producer.calls == rounds
		    && producer.releases == release_count && release.calls == release_count
		    && allocations == 0 && release.valid && producer.lease_cleared && producer.previous_cleared;
		if (ret == 0 || mode == 8)
			valid = valid && watch.checked && watch.clean;
		if (has_release)
			valid = valid && producer.cleared && auth_test_bytes_zero(producer.storage, sizeof(producer.storage));
		/* Deliberately malformed producers still own their omitted-release
		 * storage. The consumer must neither clear nor free it. */
		else
			valid = valid && producer.storage[0] == 'n';
		if (ret == 0) {
			for (unsigned i = 0; i <= rounds; i++) {
				uint8_t type;
				uint8_t *payload;
				size_t length;
				int received = recv_packet(ctx.server, &type, &payload, &length);
				valid = valid && received == 0 && type == SSH_MSG_USERAUTH_REQUEST;
				if (received == 0 && i > 0) {
					size_t tail = mode == 7 ? 4 : 7;
					valid = valid && length >= tail;
					if (length >= tail) {
						uint32_t size = DSSH_GET_U32(&payload[length - tail]);
						valid = valid && size == (mode == 7 ? 0U : 3U);
						if (mode != 7)
							valid = valid && memcmp(&payload[length - 3], "new", 3) == 0;
					}
				}
			}
		}
		dssh_cleanse(producer.storage, sizeof(producer.storage));
		handshake_cleanup(&ctx);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

#endif
