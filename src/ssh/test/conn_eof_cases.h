/* Real descriptors and encoded packets exercise the EOF oracle directly. */
#ifndef DSSH_TEST_CONN_EOF_CASES_H
#define DSSH_TEST_CONN_EOF_CASES_H

static int
test_native_eof_probe(void)
{
	ASSERT_EQ(conn_native_eof(NULL), -1);
	const size_t lengths[] = {0, 1, 3, 8};
	const uint8_t bytes[] = {0, 1, 2, 3, 4, 5, 6, 7};
	for (unsigned direction = 0; direction < 2; direction++) {
		for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
			struct mock_io_state io;
			if (mock_io_init(&io, 0) != 0)
				return TEST_FAIL;
			struct mock_io_pipe *pipe = direction == 0 ? &io.c2s : &io.s2c;
			bool valid = conn_native_eof(pipe) == -1;
			size_t length = lengths[i];
			if (length != 0)
				valid = mock_io_inject(pipe, bytes, length) == length && valid;
			if (direction == 0)
				mock_io_close_c2s_write(&io);
			else
				mock_io_close_s2c_write(&io);
			for (size_t j = 0; j < length; j++)
				valid = conn_native_eof(pipe) == 1 && valid;
			valid = conn_native_eof(pipe) == 0 && valid;
			valid = conn_native_eof(pipe) == 0 && valid;
			if (direction == 0)
				mock_io_close_c2s(&io);
			else
				mock_io_close_s2c(&io);
			valid = conn_native_eof(pipe) == -1 && valid;
			mock_io_free(&io);
			valid = conn_native_eof(pipe) == -1 && valid;
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static int
test_native_eof_wire(void)
{
	for (unsigned kind = 0; kind < 8; kind++) {
		struct conn_ctx ctx;
		ASSERT_EQ(conn_setup_mode(&ctx, false), 0);
		ASSERT_TRUE(half_close_algorithms(&ctx));
		uint8_t packet[25];
		size_t pos = 0;
		if (kind == 7) {
			packet[pos++] = SSH_MSG_CHANNEL_REQUEST;
			DSSH_PUT_U32(42, packet, &pos);
			DSSH_PUT_U32(11, packet, &pos);
			memcpy(packet + pos, "exit-status", 11);
			pos += 11;
			packet[pos++] = 0;
			DSSH_PUT_U32(UINT32_MAX, packet, &pos);
			ASSERT_EQ(send_packet(ctx.client, packet, pos, NULL), 0);
		}
		if (kind >= 3) {
			pos = 0;
			packet[pos++] = SSH_MSG_CHANNEL_EOF;
			DSSH_PUT_U32(42, packet, &pos);
			ASSERT_EQ(send_packet(ctx.client, packet, pos, NULL), 0);
		}
		if (kind >= 5) {
			pos = 0;
			packet[pos++] = SSH_MSG_CHANNEL_CLOSE;
			DSSH_PUT_U32(42, packet, &pos);
			ASSERT_EQ(send_packet(ctx.client, packet, pos, NULL), 0);
		}
		if (kind == 1 || kind == 2 || kind == 4) {
			const uint8_t extra = 0x7f;
			ASSERT_EQ_U(mock_io_inject(&ctx.io.c2s, &extra, 1), 1);
		}
		mock_io_close_c2s_write(&ctx.io);
		unsigned expected = kind <= 1 ? 0 : kind <= 5 ? 1 : kind == 6 ? 2 : 3;
		struct close_outcome_wire wire = {0};
		bool result = close_outcome_drain(ctx.server, &wire, &ctx.io.c2s, expected);
		bool wanted = kind == 0 || kind == 3 || kind == 6 || kind == 7;
		ASSERT_EQ(result, wanted);
		if (wanted) {
			ASSERT_EQ(wire.end, 0);
			ASSERT_EQ_U(wire.eof, kind == 0 ? 0 : 1);
			ASSERT_EQ_U(wire.close, kind >= 6 ? 1 : 0);
			ASSERT_EQ_U(wire.status, kind == 7 ? 1 : 0);
			if (kind == 7)
				ASSERT_EQ_U(wire.exit_code, UINT32_MAX);
		}
		struct close_outcome_wire invalid = {0};
		ASSERT_FALSE(close_outcome_drain(NULL, &invalid, &ctx.io.c2s, 0));
		ASSERT_FALSE(close_outcome_drain(ctx.server, NULL, &ctx.io.c2s, 0));
		ASSERT_FALSE(close_outcome_drain(ctx.server, &invalid, NULL, 0));
		ASSERT_FALSE(close_outcome_drain(ctx.server, &invalid, &ctx.io.c2s, 4));
		conn_cleanup(&ctx);
	}
	return TEST_PASS;
}

#endif
