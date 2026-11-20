#ifndef DSSH_TRANSPORT_IDENTIFICATION_CASES_H
#define DSSH_TRANSPORT_IDENTIFICATION_CASES_H

struct identification_io {
	uint8_t input[512];
	uint8_t emitted[255];
	size_t emitted_length;
	size_t length;
	size_t line_length;
	size_t consumed;
	unsigned tx_calls;
	unsigned line_calls;
	unsigned extra_calls;
	bool callback_input_valid;
};

static int
identification_tx(uint8_t *buf, size_t size, dssh_session sess, void *data)
{
	(void)sess;
	struct identification_io *io = data;
	io->tx_calls++;
	io->callback_input_valid = io->callback_input_valid && size >= 10 && size <= 255
	    && memcmp(buf, "SSH-2.0-", 8) == 0 && buf[size - 2] == '\r' && buf[size - 1] == '\n';
	if (size <= sizeof(io->emitted)) {
		memcpy(io->emitted, buf, size);
		io->emitted_length = size;
	}
	return 0;
}

static int
identification_rx(uint8_t *buf, size_t size, dssh_session sess, void *data)
{
	(void)sess;
	struct identification_io *io = data;
	if (io->consumed > io->length || size > io->length - io->consumed)
		return DSSH_ERROR_TERMINATED;
	memcpy(buf, &io->input[io->consumed], size);
	io->consumed += size;
	return 0;
}

static int
identification_line(uint8_t *buf, size_t size, size_t *received, dssh_session sess, void *data)
{
	struct identification_io *io = data;
	io->line_calls++;
	if (io->line_length > size)
		return DSSH_ERROR_TOOLONG;
	int ret = identification_rx(buf, io->line_length, sess, data);
	if (ret == 0)
		*received = io->line_length;
	return ret;
}

static int
identification_extra(uint8_t *buf, size_t size, void *data)
{
	(void)buf;
	(void)size;
	struct identification_io *io = data;
	io->extra_calls++;
	return 0;
}

/* Exercise the actual receive operation and transcript publication. The binary
 * packet immediately after a valid line must survive either line-reader path. */
static bool
identification_case(const uint8_t *line, size_t length, bool client, bool supplied_line,
    bool accepted)
{
	struct identification_io io = { .callback_input_valid = true };
	if (length > sizeof(io.input) - 16)
		return false;
	memcpy(io.input, line, length);
	io.line_length = length;
	io.length = length + 16;
	uint8_t *packet = &io.input[length];
	memset(packet, 0, 16);
	packet[3] = 12;
	packet[4] = 10;
	packet[5] = SSH_MSG_USERAUTH_SUCCESS;
	dssh_test_reset_global_config();
	dssh_transport_rxline_cb reader = supplied_line ? identification_line : NULL;
	if (dssh_transport_set_callbacks(identification_tx, identification_rx, reader, identification_extra) < 0)
		return false;
	dssh_session sess = dssh_session_init(client, 0);
	if (sess == NULL) {
		dssh_test_reset_global_config();
		return false;
	}
	bool valid = dssh_session_set_cbdata(sess, &io, &io, &io, &io) == 0;
	int ret = valid ? version_exchange(sess) : DSSH_ERROR_INIT;
	valid = valid && io.callback_input_valid && io.tx_calls == 1 && io.extra_calls == 0;
	if (accepted) {
		valid = valid && ret == 0 && !dssh_session_is_terminated(sess)
		    && io.consumed == length && sess->trans.remote_id_str_sz == length - 2
		    && memcmp(sess->trans.remote_id_str, line, length - 2) == 0
		    && sess->trans.remote_id_str[length - 2] == 0;
		uint8_t type = 0;
		uint8_t *payload = NULL;
		size_t payload_len = 0;
		if (valid) {
			ret = recv_packet(sess, &type, &payload, &payload_len);
			valid = ret == 0 && type == SSH_MSG_USERAUTH_SUCCESS && payload_len == 1
			    && payload[0] == SSH_MSG_USERAUTH_SUCCESS && io.consumed == length + 16;
		}
	}
	else {
		valid = valid && ret < 0 && dssh_session_is_terminated(sess)
		    && sess->trans.remote_id_str_sz == 0 && sess->trans.remote_id_str[0] == 0
		    && io.consumed <= length;
	}
	if (supplied_line)
		valid = valid && io.line_calls == 1;
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return valid;
}

static int
test_identification_grammar(void)
{
	static const struct {
		const char *line;
		bool accepted;
	} cases[] = {
		{ "SSH-2.0-x\r\n", true },
		{ "SSH-1.99-x\r\n", true },
		{ "SSH-2.0-MySSH_1.0\r\n", true },
		{ "SSH-1.99-MySSH_1.0\r\n", true },
		{ "SSH-2.0-MySSH-1.0\r\n", false },
		{ "SSH-1.99-MySSH-1.0\r\n", false },
		{ "SSH-2.0--x\r\n", false },
		{ "SSH-1.99--x\r\n", false },
		{ "SSH-2.0-x-\r\n", false },
		{ "SSH-1.99-x-\r\n", false },
		{ "SSH-2.0-x comment-with-minus\r\n", true },
		{ "SSH-1.99-x comment-with-minus\r\n", true },
		{ "SSH-2.0-x comment with spaces\r\n", true },
		{ "SSH-1.99-x compatible server\r\n", true },
		{ "SSH-2.0-x \r\n", true },
		{ "SSH-2.0-\r\n", false },
		{ "SSH-1.99-\r\n", false },
		{ "SSH-2.0- comment\r\n", false },
		{ "SSH-1.99- comment\r\n", false },
		{ "SSH-2.0-   \r\n", false },
		{ "SSH-2.0-x\tcomment\r\n", false },
		{ "SSH-2.0-x\rcomment\r\n", false },
		{ "SSH-2.0-x\ncomment\r\n", false },
		{ "SSH-2.0-x\177comment\r\n", false },
		{ "SSH-2.0-x\200comment\r\n", false },
		{ "SSH-1.0-x\r\n", false },
		{ "SSH-2.1-x\r\n", false },
		{ "SSH-2.0\r\n", false },
		{ "SSH-1.99\r\n", false },
		{ "SSH-\r\n", false },
	};
	for (unsigned client = 0; client < 2; client++) {
		for (unsigned supplied = 0; supplied < 2; supplied++) {
			for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
				const uint8_t *line = (const uint8_t *)cases[i].line;
				ASSERT_TRUE(identification_case(line, strlen(cases[i].line), client != 0,
				    supplied != 0, cases[i].accepted));
			}
		}
	}
	return TEST_PASS;
}

static int
test_identification_bytes(void)
{
	for (unsigned client = 0; client < 2; client++) {
		for (unsigned supplied = 0; supplied < 2; supplied++) {
			for (unsigned value = 0; value <= UINT8_MAX; value++) {
				uint8_t line[] = "SSH-2.0-Xtail\r\n";
				uint8_t byte = (uint8_t)value;
				line[8] = byte;
				bool accepted = value > 32 && value < 127 && value != '-';
				ASSERT_TRUE(identification_case(line, sizeof(line) - 1, client != 0,
				    supplied != 0, accepted));
			}
		}
	}
	return TEST_PASS;
}

static int
test_identification_bounds(void)
{
	static const char *prefixes[] = { "SSH-2.0-", "SSH-1.99-" };
	for (unsigned client = 0; client < 2; client++) {
		for (unsigned supplied = 0; supplied < 2; supplied++) {
			for (size_t p = 0; p < sizeof(prefixes) / sizeof(prefixes[0]); p++) {
				for (size_t length = 254; length <= 256; length++) {
					uint8_t line[256];
					memset(line, 'x', sizeof(line));
					memcpy(line, prefixes[p], strlen(prefixes[p]));
					line[length - 2] = '\r';
					line[length - 1] = '\n';
					ASSERT_TRUE(identification_case(line, length, client != 0,
					    supplied != 0, length <= 255));
					line[strlen(prefixes[p]) + 1] = ' ';
					ASSERT_TRUE(identification_case(line, length, client != 0,
					    supplied != 0, length <= 255));
				}
			}
		}
	}
	return TEST_PASS;
}

static int
test_identification_setters(void)
{
	static const char selected[] = "Selected_1.0";
	static const char kept[] = "kept-comment";
	static const char replacement[] = "new-comment";
	for (unsigned value = 0; value <= UINT8_MAX; value++) {
		dssh_test_reset_global_config();
		ASSERT_OK(dssh_transport_set_version(selected, kept));
		uint8_t byte = (uint8_t)value;
		uint8_t token[] = { byte, 0 };
		const char *software = (const char *)token;
		bool accepted = value >= 0x21 && value <= 0x7E && value != '-';
		int expected = value == 0 ? DSSH_ERROR_PARSE : accepted ? 0 : DSSH_ERROR_INVALID;
		int ret = dssh_transport_set_version(software, replacement);
		bool valid = ret == expected;
		if (accepted)
			valid = valid && gconf.software_version == software && gconf.version_comment == replacement;
		else
			valid = valid && gconf.software_version == selected && gconf.version_comment == kept;
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static bool
identification_emit_case(const char *software, const char *comment, size_t expected_length)
{
	struct identification_io io = { .callback_input_valid = true };
	dssh_test_reset_global_config();
	int ret = dssh_transport_set_callbacks(identification_tx, identification_rx, NULL, NULL);
	if (ret == 0)
		ret = dssh_transport_set_version(software, comment);
	if (ret != 0) {
		dssh_test_reset_global_config();
		return false;
	}
	uint8_t expected[255];
	size_t pos = 8;
	memcpy(expected, "SSH-2.0-", pos);
	const char *selected = gconf.software_version;
	size_t length = strlen(selected);
	bool valid = dssh_buffer_has(sizeof(expected), pos, length);
	if (software == NULL)
		valid = valid && length >= 9 && memcmp(selected, "DeuceSSH_", 9) == 0;
	if (valid) {
		memcpy(&expected[pos], selected, length);
		pos += length;
	}
	if (comment != NULL && valid) {
		length = strlen(comment);
		valid = dssh_buffer_has(sizeof(expected), pos, 1)
		    && dssh_buffer_has(sizeof(expected), pos + 1, length);
		if (valid) {
			expected[pos++] = ' ';
			memcpy(&expected[pos], comment, length);
			pos += length;
		}
	}
	valid = valid && dssh_buffer_has(sizeof(expected), pos, 2);
	if (valid) {
		expected[pos++] = '\r';
		expected[pos++] = '\n';
	}
	dssh_session sess = valid ? dssh_session_init(true, 0) : NULL;
	if (sess == NULL) {
		dssh_test_reset_global_config();
		return false;
	}
	ret = dssh_session_set_cbdata(sess, &io, &io, &io, &io);
	if (ret == 0)
		ret = version_tx(sess);
	valid = valid && ret == 0 && io.callback_input_valid && io.tx_calls == 1
	    && io.emitted_length == pos && (expected_length == 0 || pos == expected_length)
	    && memcmp(io.emitted, expected, pos) == 0 && sess->trans.id_str_sz == pos - 2
	    && memcmp(sess->trans.id_str, expected, pos - 2) == 0 && sess->trans.id_str[pos - 2] == 0;
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return valid;
}

static int
test_identification_output(void)
{
	static const char *invalid[] = { "", "bad-token", "bad token",
	    "bad\001token", "bad\177token", "bad\200token" };
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		struct identification_io io = { .callback_input_valid = true };
		dssh_test_reset_global_config();
		ASSERT_OK(dssh_transport_set_callbacks(identification_tx, identification_rx, NULL, NULL));
		dssh_session sess = dssh_session_init(true, 0);
		if (sess == NULL) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		int ret = dssh_session_set_cbdata(sess, &io, &io, &io, &io);
		bool configured = ret == 0;
		/* Exercise the sender's independent admission check for an invalid
		 * compiled default/configuration, without using the validated setter. */
		dssh_test_set_sw_version(invalid[i]);
		if (ret == 0)
			ret = version_tx(sess);
		bool valid = configured && ret == DSSH_ERROR_INVALID && io.tx_calls == 0 && sess->trans.id_str_sz == 0;
		dssh_session_cleanup(sess);
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
	}
	ASSERT_TRUE(identification_emit_case(NULL, NULL, 0));
	ASSERT_TRUE(identification_emit_case(NULL, "default-comment", 0));
	ASSERT_TRUE(identification_emit_case("MySSH_1.0", "release-candidate", 0));
	char version[247];
	memset(version, 'v', sizeof(version));
	version[245] = 0;
	ASSERT_TRUE(identification_emit_case(version, NULL, 255));
	version[245] = 'v';
	version[246] = 0;
	dssh_test_reset_global_config();
	ASSERT_OK(dssh_transport_set_version("Kept_1.0", NULL));
	const char *kept = gconf.software_version;
	int ret = dssh_transport_set_version(version, NULL);
	bool valid = ret == DSSH_ERROR_TOOLONG && gconf.software_version == kept;
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	char comment[245];
	memset(comment, 'c', sizeof(comment));
	comment[1] = '-';
	comment[243] = 0;
	ASSERT_TRUE(identification_emit_case("x", comment, 255));
	comment[243] = 'c';
	comment[244] = 0;
	ASSERT_OK(dssh_transport_set_version("Kept_1.0", NULL));
	kept = gconf.software_version;
	ret = dssh_transport_set_version("x", comment);
	valid = ret == DSSH_ERROR_TOOLONG && gconf.software_version == kept;
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#endif
