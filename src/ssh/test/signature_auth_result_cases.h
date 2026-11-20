#ifndef DSSH_TEST_SIGNATURE_AUTH_RESULT_CASES_H
#define DSSH_TEST_SIGNATURE_AUTH_RESULT_CASES_H

#include "signature_result_provider.h"

/* Only the signing boundary is substituted, after the real handshake completes.
 * Native key/public-blob ownership and all transport state stay real. */
static struct test_signature_result_provider *signature_producer;

static int
signature_producer_adapter(struct dssh_bytes_result *out, const uint8_t *data,
    size_t length, dssh_key_algo_ctx *context)
{
	(void)context;
	dssh_key_algo_ctx *opaque = (dssh_key_algo_ctx *)signature_producer;
	return test_signature_result_produce(out, data, length, opaque);
}

static int
test_auth_signature_result_contract(void)
{
	const int expected[] = {0, -77, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID,
	    DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID,
	    0, DSSH_ERROR_TERMINATED, DSSH_ERROR_TOOLONG};
	for (unsigned mode = 0; mode < sizeof(expected) / sizeof(expected[0]); mode++) {
		struct test_signature_result_provider producer = {.mode = mode, .valid = true};
		unsigned rounds = mode == 0 ? 2 : 1;
		for (unsigned round = 0; round < rounds; round++) {
			struct handshake_ctx ctx;
			if (handshake_setup(&ctx) < 0) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			ctx.client->auth_service_requested = true;
			producer.oversized_length = tx_payload_capacity(ctx.client);
			uint8_t success = SSH_MSG_USERAUTH_SUCCESS;
			bool valid = send_packet(ctx.server, &success, 1, NULL) == 0;
			struct password_release_probe release = {.session = ctx.client, .valid = true};
			producer.observe_release = password_release_probe;
			producer.observe_data = &release;
			dssh_key_algo key = find_key_algo(test_key_algo_name());
			if (key == NULL) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			dssh_key_algo_sign saved_sign = key->sign;
			key->sign = signature_producer_adapter;
			signature_producer = &producer;
			struct auth_test_io_watch watch = {.target = ctx.client, .secret = producer.storage,
			    .secret_len = sizeof(producer.storage), .armed = mode != 8, .clean = true,
			    .fail_enabled = mode == 9, .fail_at = 0};
			auth_test_io_watch = &watch;
			unsigned ownerless_before = test_signature_ownerless_releases();
			dssh_test_alloc_fail_after(1); /* Only the owned signing transcript. */
			int ret = valid ? dssh_auth_publickey(ctx.client, "u", test_key_algo_name()) : -999;
			int allocations = dssh_test_alloc_count();
			dssh_test_alloc_reset();
			auth_test_io_watch = NULL;
			signature_producer = NULL;
			key->sign = saved_sign;
			bool owned_release = mode != 5 && mode != 6 && mode != 8;
			unsigned per_round = owned_release ? 1 : 0;
			valid = valid && ret == expected[mode] && allocations == 1
			    && producer.calls == round + 1 && producer.releases == (owned_release ? round + 1 : 0)
			    && release.calls == per_round && release.valid && producer.valid
			    && !tx_owner_is_current(ctx.client);
			if (owned_release)
				valid = valid && auth_test_bytes_zero(producer.storage, sizeof(producer.storage));
			else
				valid = valid && producer.storage[0] == 's';
			if (mode == 8)
				valid = valid && test_signature_ownerless_releases() == ownerless_before + 1;
			if (ret == 0) {
				uint8_t type;
				uint8_t *payload;
				size_t length;
				int received = recv_packet(ctx.server, &type, &payload, &length);
				valid = valid && received == 0 && type == SSH_MSG_USERAUTH_REQUEST && length >= 7;
				if (received == 0 && length >= 7)
					valid = valid && DSSH_GET_U32(&payload[length - 7]) == 3
					    && payload[length - 3] == 's' && payload[length - 2] == 0 && payload[length - 1] == 'g';
			}
			if (ret == 0 || mode == 9)
				valid = valid && (mode == 8 || (watch.checked && watch.clean));
			handshake_cleanup(&ctx);
			if (!valid)
				fprintf(stderr, "signature producer mode=%u round=%u ret=%d expected=%d alloc=%d\n",
				    mode, round, ret, expected[mode], allocations);
			ASSERT_TRUE(valid);
		}
		dssh_cleanse(&producer, sizeof(producer));
	}
	return TEST_PASS;
}

#endif
