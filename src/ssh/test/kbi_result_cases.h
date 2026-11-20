#ifndef DSSH_TEST_KBI_RESULT_CASES_H
#define DSSH_TEST_KBI_RESULT_CASES_H

#include "kbi_result_provider.h"

static bool
kbi_result_read_answer(const uint8_t *packet, size_t length, size_t *pos,
    const uint8_t **answer, size_t *answer_len)
{
	if (!dssh_buffer_has(length, *pos, 4))
		return false;
	uint32_t size = DSSH_GET_U32(&packet[*pos]);
	*pos += 4;
	if (size > length - *pos)
		return false;
	size_t safe_size = (size_t)size;
	*answer_len = safe_size;
	*answer = &packet[*pos];
	*pos += *answer_len;
	return true;
}

static size_t
kbi_result_request(uint8_t *packet, size_t capacity, uint32_t count)
{
	const uint8_t name[] = {0, 'N'};
	const uint8_t instruction[] = {'I', 0};
	const uint8_t prompt[] = {'p', 0};
	size_t pos = 1;
	packet[0] = SSH_MSG_USERAUTH_INFO_REQUEST;
	if (!auth_frame_string(packet, capacity, &pos, name, sizeof(name))
	    || !auth_frame_string(packet, capacity, &pos, instruction, sizeof(instruction))
	    || !auth_frame_string(packet, capacity, &pos, "", 0)
	    || !dssh_buffer_has(capacity, pos, 4))
		return 0;
	DSSH_PUT_U32(count, packet, &pos);
	for (uint32_t i = 0; i < count; i++) {
		if (!auth_frame_string(packet, capacity, &pos, prompt, sizeof(prompt))
		    || !dssh_buffer_has(capacity, pos, 1))
			return 0;
		packet[pos++] = i != 0;
	}
	return pos;
}

static int
test_kbi_result_client_contract(void)
{
	const int expected[] = {0, -77, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID,
	    DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, 0,
	    DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_TOOLONG,
	    0, DSSH_ERROR_TOOLONG, 0, DSSH_ERROR_ALLOC, DSSH_ERROR_ALLOC, DSSH_ERROR_ALLOC};
	for (unsigned mode = 0; mode < sizeof(expected) / sizeof(expected[0]); mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		ctx.client->auth_service_requested = true;
		unsigned rounds = mode == 14 ? 3 : mode == 0 || mode == 12 ? 2 : 1;
		bool valid = true;
		for (unsigned i = 0; i < rounds; i++) {
			uint8_t request[64];
			uint32_t count = mode == 12 ? 0 : mode == 14 && i != 1 ? 1 : 2;
			size_t length = kbi_result_request(request, sizeof(request), count);
			if (length == 0 || send_packet(ctx.server, request, length, NULL) != 0)
				valid = false;
		}
		uint8_t success = SSH_MSG_USERAUTH_SUCCESS;
		if (send_packet(ctx.server, &success, 1, NULL) != 0)
			valid = false;
		struct password_release_probe release = {.session = ctx.client, .valid = true};
		struct test_kbi_result_provider producer = {.mode = mode, .valid = true,
		    .oversized_length = tx_payload_capacity(ctx.client),
		    .observe_release = password_release_probe, .observe_data = &release};
		struct auth_test_io_watch watch = {.target = ctx.client, .secret = producer.storage,
		    .secret_len = sizeof(producer.storage), .armed = true, .clean = true};
		auth_test_io_watch = &watch;
		/* The first nonempty round creates three input-view arrays. Mode 14
		 * grows those once. All result storage is inline and reused. */
		int expected_allocations = mode == 12 ? 0 : mode == 14 ? 6 : 3;
		int fail_after = expected_allocations;
		if (mode >= 15) {
			int failure_index = (int)(mode - 15);
			fail_after = failure_index;
			expected_allocations = 3;
		}
		dssh_test_alloc_fail_after(fail_after);
		int ret = valid ? dssh_auth_keyboard_interactive(ctx.client, "u", test_kbi_result_client, &producer) : -999;
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		auth_test_io_watch = NULL;
		unsigned release_count = mode == 5 || mode == 6 || mode >= 15 ? 0 : rounds;
		unsigned calls = mode >= 15 ? 0 : rounds;
		valid = valid && ret == expected[mode] && allocations == expected_allocations
		    && producer.calls == calls && producer.valid && producer.releases == release_count
		    && release.calls == release_count && release.valid && !tx_owner_is_current(ctx.client);
		if (release_count > 0) {
			const uint8_t *answer_bytes = (const uint8_t *)producer.answers;
			const uint8_t *prompt_bytes = (const uint8_t *)producer.prompts;
			valid = valid && !producer.leased && auth_test_bytes_zero(producer.storage, sizeof(producer.storage))
			    && auth_test_bytes_zero(answer_bytes, sizeof(producer.answers))
			    && auth_test_bytes_zero(prompt_bytes, sizeof(producer.prompts));
		}
		else if (calls > 0)
			valid = valid && producer.leased && producer.storage[1] == 'N';
		else
			valid = valid && !producer.leased && auth_test_bytes_zero(producer.storage, sizeof(producer.storage));
		if (ret == 0) {
			valid = valid && watch.checked && watch.clean;
			for (unsigned i = 0; i <= rounds; i++) {
				uint8_t type;
				uint8_t *payload;
				size_t length;
				int received = recv_packet(ctx.server, &type, &payload, &length);
				valid = valid && received == 0;
				if (received != 0)
					break;
				if (i == 0) {
					valid = valid && type == SSH_MSG_USERAUTH_REQUEST;
					continue;
				}
				uint32_t count = mode == 12 ? 0 : mode == 14 && i != 2 ? 1 : 2;
				valid = valid && type == SSH_MSG_USERAUTH_INFO_RESPONSE && length >= 5;
				if (length < 5)
					continue;
				valid = valid && DSSH_GET_U32(&payload[1]) == count;
				size_t pos = 5;
				for (uint32_t j = 0; j < count; j++) {
					const uint8_t *answer;
					size_t answer_len;
					bool parsed = kbi_result_read_answer(payload, length, &pos, &answer, &answer_len);
					valid = valid && parsed;
					if (!parsed)
						break;
					valid = valid && answer_len == (mode == 7 ? 0U : 2U);
					if (answer_len == 2)
						valid = valid && answer[0] == 'r' && answer[1] == 0;
				}
				valid = valid && pos == length;
			}
		}
		dssh_cleanse(&producer, sizeof(producer));
		handshake_cleanup(&ctx);
		if (!valid)
			fprintf(stderr, "KBI client mode=%u ret=%d expected=%d alloc=%d expected-alloc=%d\n",
			    mode, ret, expected[mode], allocations, expected_allocations);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

struct kbi_result_peer {
	struct handshake_ctx *ctx;
	unsigned mode;
	unsigned requests;
	bool valid;
};

static int
kbi_result_peer_thread(void *data)
{
	struct kbi_result_peer *peer = data;
	dssh_test_alloc_exclude_thread();
	for (;;) {
		uint8_t type;
		uint8_t *payload;
		size_t length;
		int ret = recv_packet(peer->ctx->client, &type, &payload, &length);
		if (ret < 0)
			break;
		if (type == SSH_MSG_SERVICE_ACCEPT)
			continue;
		if (type != SSH_MSG_USERAUTH_INFO_REQUEST)
			break;
		peer->requests++;
		uint8_t expected[64];
		uint32_t count = peer->mode == 12 ? 0 : 2;
		size_t expected_len = kbi_result_request(expected, sizeof(expected), count);
		peer->valid = peer->valid && length == expected_len;
		if (length == expected_len)
			peer->valid = peer->valid && memcmp(payload, expected, length) == 0;
		uint8_t response[32] = {SSH_MSG_USERAUTH_INFO_RESPONSE};
		size_t pos = 1;
		DSSH_PUT_U32(count, response, &pos);
		const uint8_t answer[] = {'r', 0};
		for (uint32_t i = 0; i < count; i++) {
			if (!auth_frame_string(response, sizeof(response), &pos, answer, sizeof(answer)))
				peer->valid = false;
		}
		if (!peer->valid || send_packet(peer->ctx->client, response, pos, NULL) < 0)
			break;
	}
	mock_io_close_c2s_write(&peer->ctx->io);
	return 0;
}

static int
test_kbi_result_server_contract(void)
{
	const int expected[] = {0, DSSH_ERROR_TERMINATED, DSSH_ERROR_INVALID, DSSH_ERROR_TOOLONG,
	    DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID,
	    DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_TOOLONG, DSSH_ERROR_TOOLONG,
	    0, DSSH_ERROR_TERMINATED, DSSH_ERROR_ALLOC, DSSH_ERROR_ALLOC};
	for (unsigned mode = 0; mode < sizeof(expected) / sizeof(expected[0]); mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		uint8_t service[17] = {SSH_MSG_SERVICE_REQUEST};
		size_t pos = 1;
		bool valid = auth_frame_string(service, sizeof(service), &pos, "ssh-userauth", 12);
		if (send_packet(ctx.client, service, pos, NULL) != 0)
			valid = false;
		uint8_t extra[8] = {0};
		uint8_t request[128];
		size_t length = build_auth_request("keyboard-interactive", 20, extra, sizeof(extra), request, sizeof(request));
		if (send_packet(ctx.client, request, length, NULL) != 0)
			valid = false;
		struct password_release_probe release = {.session = ctx.server, .valid = true};
		struct test_kbi_result_provider producer = {.mode = mode, .valid = true, .server = true,
		    .oversized_length = tx_payload_capacity(ctx.server),
		    .observe_release = password_release_probe, .observe_data = &release};
		struct dssh_auth_server_cbs cbs = {.methods_str = "keyboard-interactive",
		    .keyboard_interactive_cb = test_kbi_result_server, .cbdata = &producer};
		struct kbi_result_peer peer = {.ctx = &ctx, .mode = mode, .valid = true};
		struct auth_test_io_watch watch = {.target = ctx.server, .secret = producer.storage,
		    .secret_len = sizeof(producer.storage), .armed = true, .clean = true};
		auth_test_io_watch = &watch;
		thrd_t thread;
		int created = thrd_create(&thread, kbi_result_peer_thread, &peer);
		if (created != thrd_success) {
			auth_test_io_watch = NULL;
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		int expected_allocations = mode == 0 || mode >= 14 ? 3 : 1;
		int fail_after = expected_allocations;
		if (mode >= 14) {
			int failure_index = (int)(mode - 13);
			fail_after = failure_index;
		}
		dssh_test_alloc_fail_after(fail_after);
		int ret = valid ? dssh_auth_server(ctx.server, &cbs, NULL, NULL) : -999;
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		if (thrd_join(thread, NULL) != thrd_success)
			abort();
		auth_test_io_watch = NULL;
		unsigned calls = mode == 0 || mode == 12 ? 3 : 1;
		unsigned releases = mode == 5 || mode == 6 ? 0 : calls;
		valid = valid && ret == expected[mode]
		    && allocations == expected_allocations && producer.calls == calls && producer.valid
		    && producer.releases == releases && release.calls == releases && release.valid && peer.valid
		    && peer.requests == (mode == 0 || mode == 12 ? 2U : mode >= 14 ? 1U : 0U)
		    && !tx_owner_is_current(ctx.server) && watch.checked && watch.clean;
		if (releases > 0)
			valid = valid && !producer.leased && auth_test_bytes_zero(producer.storage, sizeof(producer.storage));
		else
			valid = valid && producer.leased && producer.storage[1] == 'N';
		dssh_cleanse(&producer, sizeof(producer));
		handshake_cleanup(&ctx);
		if (!valid)
			fprintf(stderr, "KBI server mode=%u ret=%d expected=%d alloc=%d expected-alloc=%d calls=%u releases=%u peer=%u\n",
			    mode, ret, expected[mode], allocations, expected_allocations, calls, releases, peer.requests);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

#endif
