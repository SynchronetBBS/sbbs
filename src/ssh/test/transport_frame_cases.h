#ifndef DSSH_TEST_TRANSPORT_FRAME_CASES_H
#define DSSH_TEST_TRANSPORT_FRAME_CASES_H

struct frame_registry {
	dssh_enc enc;
	dssh_mac mac[2];
};

struct frame_probe {
	uint8_t plain[20];
	uint8_t wire[80];
	size_t wire_size;
	size_t offset;
	size_t reads[3];
	unsigned read_count;
	unsigned decrypt_count;
	unsigned mac_count;
	unsigned mac_successes;
	bool valid;
	dssh_session sess;
	dssh_enc_crypt decrypt;
	dssh_mac_generate generate;
	dssh_mac_ctx *mac_context;
	size_t tag_size;
};

static struct frame_probe *frame_active;

static int
frame_tx(uint8_t *buf, size_t length, dssh_session sess, void *opaque)
{
	(void)buf;
	(void)length;
	(void)sess;
	struct frame_probe *probe = opaque;
	if (probe != NULL)
		probe->valid = false;
	return DSSH_ERROR_INIT;
}

static int
frame_line(uint8_t *buf, size_t length, size_t *received, dssh_session sess, void *opaque)
{
	if (received != NULL)
		*received = 0;
	return frame_tx(buf, length, sess, opaque);
}

static int
frame_rx(uint8_t *buf, size_t length, dssh_session sess, void *opaque)
{
	struct frame_probe *probe = opaque;
	if (probe == NULL)
		return DSSH_ERROR_INVALID;
	if (buf == NULL || sess != probe->sess || probe->read_count >= 3
	    || probe->offset > probe->wire_size || length > probe->wire_size - probe->offset) {
		probe->valid = false;
		return DSSH_ERROR_INVALID;
	}
	probe->reads[probe->read_count++] = length;
	memcpy(buf, &probe->wire[probe->offset], length);
	probe->offset += length;
	return 0;
}

static int
frame_decrypt(uint8_t *buf, size_t length, dssh_enc_ctx *ctx)
{
	struct frame_probe *probe = frame_active;
	if (probe == NULL)
		return DSSH_ERROR_INVALID;
	if (probe->decrypt_count >= 2 || length != 8 || ctx != NULL || probe->decrypt == NULL) {
		probe->valid = false;
		return DSSH_ERROR_INVALID;
	}
	size_t offset = 8 * probe->decrypt_count;
	if (buf != &probe->sess->trans.rx_packet[4 + offset]
	    || memcmp(buf, &probe->plain[4 + offset], length) != 0) {
		probe->valid = false;
		return DSSH_ERROR_INVALID;
	}
	probe->decrypt_count++;
	return probe->decrypt(buf, length, ctx);
}

static int
frame_mac(const uint8_t *buf, size_t length, uint8_t *out, dssh_mac_ctx *ctx)
{
	struct frame_probe *probe = frame_active;
	if (probe == NULL)
		return DSSH_ERROR_INVALID;
	if (probe->mac_count != 0 || buf != probe->sess->trans.rx_packet || length != sizeof(probe->plain)
	    || out != probe->sess->trans.rx_mac_computed || ctx != probe->mac_context || probe->generate == NULL
	    || memcmp(buf, probe->plain, sizeof(probe->plain)) != 0) {
		probe->valid = false;
		return DSSH_ERROR_INVALID;
	}
	probe->mac_count++;
	int ret = probe->generate(buf, length, out, ctx);
	if (ret == 0 && memcmp(out, &probe->wire[16], probe->tag_size) == 0)
		probe->mac_successes++;
	else
		probe->valid = false;
	return ret;
}

/* The same registered definitions/callbacks are reused across each root's
 * matrix. No worker is running while their private observers are installed. */
static int
frame_registry_prepare(struct frame_registry *registry)
{
	*registry = (struct frame_registry){0};
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	int ret = dssh_register_none_comp();
	if (ret == 0)
		ret = dssh_register_none_enc();
	if (ret == 0)
		ret = dssh_register_hmac_sha2_256();
	if (ret == 0)
		ret = dssh_register_hmac_sha2_512();
	if (ret == 0)
		ret = dssh_transport_set_callbacks(frame_tx, frame_rx, frame_line, NULL);
	if (ret != 0)
		return ret;
	registry->enc = gconf.enc_head;
	registry->mac[0] = gconf.mac_head;
	registry->mac[1] = registry->mac[0] == NULL ? NULL : registry->mac[0]->next;
	if (registry->enc == NULL || registry->enc->decrypt == NULL || registry->enc->blocksize != 1
	    || strcmp(registry->enc->name, "none") != 0 || registry->mac[0] == NULL || registry->mac[1] == NULL)
		return DSSH_ERROR_INIT;
	for (size_t i = 0; i < 2; i++) {
		dssh_mac mac = registry->mac[i];
		size_t expected = i == 0 ? 32 : 64;
		if (mac->init == NULL || mac->generate == NULL || mac->cleanup == NULL
		    || mac->digest_size != expected || mac->key_size != expected
		    || strcmp(mac->name, i == 0 ? "hmac-sha2-256" : "hmac-sha2-512") != 0)
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static void
frame_registry_finish(void)
{
	if (frame_active != NULL || test_pair_active != NULL)
		abort();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
}

static int
frame_session_prepare(struct test_session_pair *pair, struct frame_probe *probe,
    const struct frame_registry *registry, bool client, unsigned mode)
{
	dssh_session sess = dssh_session_init(client, 0);
	if (client)
		pair->client = sess;
	else
		pair->server = sess;
	if (sess == NULL)
		return DSSH_ERROR_ALLOC;
	probe->sess = sess;
	int ret = dssh_session_set_cbdata(sess, probe, probe, probe, NULL);
	if (ret != 0 || mode == 0)
		return ret;
	dssh_mac mac = registry->mac[mode - 1];
	static const uint8_t key[64] = {0x42};
	dssh_mac_ctx **context;
	if (client) {
		sess->trans.enc_s2c_active = registry->enc;
		sess->trans.enc_s2c_selected = registry->enc;
		sess->trans.mac_s2c_selected = mac;
		context = &sess->trans.mac_s2c_ctx;
	}
	else {
		sess->trans.enc_c2s_active = registry->enc;
		sess->trans.enc_c2s_selected = registry->enc;
		sess->trans.mac_c2s_selected = mac;
		context = &sess->trans.mac_c2s_ctx;
	}
	ret = mac->init(key, context);
	if (ret == 0 || *context != NULL) {
		if (client)
			sess->trans.mac_s2c_active = mac;
		else
			sess->trans.mac_c2s_active = mac;
	}
	if (ret != 0)
		return ret;
	if (*context == NULL)
		return DSSH_ERROR_INIT;
	probe->mac_context = *context;
	probe->tag_size = mac->digest_size;
	return reserve_mac_buffers(sess, mac->digest_size);
}

struct frame_result {
	int fixture;
	int operation;
	int allocations;
	bool ran;
	bool valid;
	bool retired;
};

static struct frame_result
frame_case(const struct frame_registry *registry, bool client, unsigned mode,
    uint8_t padding, uint32_t declared, int expected, int allocation_failure)
{
	struct frame_result result = {.fixture = DSSH_ERROR_INVALID, .operation = DSSH_ERROR_INIT};
	if (mode >= 3 || frame_active != NULL)
		return result;
	struct test_session_pair pair;
	test_pair_begin(&pair);
	struct frame_probe probe = {.valid = true};
	dssh_mac mac = mode == 0 ? NULL : registry->mac[mode - 1];
	bool observed = false;
	dssh_test_alloc_fail_after(allocation_failure);
	result.fixture = frame_session_prepare(&pair, &probe, registry, client, mode);
	result.allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	if (result.fixture != 0)
		goto done;
	dssh_session sess = probe.sess;
	sess->trans.rx_seq = UINT32_C(0x10203040);
	sess->trans.rx_since_rekey = 3;
	sess->trans.rx_bytes_since_rekey = 99;
	memset(probe.plain, 0x5a, sizeof(probe.plain));
	probe.plain[0] = 0x10;
	probe.plain[1] = 0x20;
	probe.plain[2] = 0x30;
	probe.plain[3] = 0x40;
	size_t position = 4;
	result.fixture = dssh_serialize_uint32(declared, probe.plain, sizeof(probe.plain), &position);
	if (result.fixture != 0 || position != 8) {
		result.fixture = DSSH_ERROR_INIT;
		goto done;
	}
	probe.plain[8] = padding;
	probe.plain[9] = SSH_MSG_SERVICE_ACCEPT;
	memcpy(probe.wire, &probe.plain[4], 16);
	probe.wire_size = 16 + probe.tag_size;
	if (mode != 0) {
		probe.decrypt = registry->enc->decrypt;
		probe.generate = mac->generate;
		result.fixture = probe.generate(probe.plain, sizeof(probe.plain), &probe.wire[16], probe.mac_context);
		if (result.fixture != 0)
			goto done;
		frame_active = &probe;
		registry->enc->decrypt = frame_decrypt;
		mac->generate = frame_mac;
		observed = true;
	}
	uint8_t marker = 0;
	uint8_t type = 0xa5;
	uint8_t *payload = &marker;
	size_t length = 77;
	result.ran = true;
	result.operation = recv_packet(sess, &type, &payload, &length);
	bool complete = declared == 12;
	unsigned reads = complete ? mode == 0 ? 2U : 3U : 1U;
	unsigned decrypts = mode == 0 ? 0U : complete ? 2U : 1U;
	unsigned macs = mode != 0 && complete ? 1U : 0U;
	size_t consumed = complete ? probe.wire_size : 8;
	result.valid = result.operation == expected && probe.valid && probe.read_count == reads
	    && probe.reads[0] == 8 && probe.offset == consumed && probe.decrypt_count == decrypts
	    && probe.mac_count == macs && probe.mac_successes == macs;
	if (complete)
		result.valid = probe.reads[1] == 8 && (mode == 0 || probe.reads[2] == probe.tag_size) && result.valid;
	if (expected == 0) {
		if (padding < 4 || padding > 10)
			result.valid = false;
		else {
			size_t pad = padding;
			size_t wanted = 11 - pad;
			result.valid = payload == &sess->trans.rx_packet[9] && type == SSH_MSG_SERVICE_ACCEPT
			    && length == wanted && memcmp(payload, &probe.plain[9], wanted) == 0
			    && sess->trans.rx_seq == UINT32_C(0x10203041) && sess->trans.last_rx_seq == UINT32_C(0x10203040)
			    && sess->trans.rx_since_rekey == 4 && sess->trans.rx_bytes_since_rekey == 99 + probe.wire_size
			    && !dssh_session_is_terminated(sess) && result.valid;
		}
	}
	else {
		bool empty = complete && padding == 11;
		result.valid = type == 0xa5 && (empty ? payload == &sess->trans.rx_packet[9] && length == 0
		    : payload == &marker && length == 77)
		    && sess->trans.rx_seq == UINT32_C(0x10203040) && sess->trans.last_rx_seq == 0
		    && sess->trans.rx_since_rekey == 3 && sess->trans.rx_bytes_since_rekey == 99
		    && dssh_session_is_terminated(sess) && result.valid;
		if (sess->trans.packet_buf_sz > SIZE_MAX - 4)
			result.valid = false;
		else {
			size_t extent = 4 + sess->trans.packet_buf_sz;
			for (size_t i = 0; i < extent; i++)
				result.valid = sess->trans.rx_packet[i] == 0 && result.valid;
		}
	}
done:
	if (observed) {
		registry->enc->decrypt = probe.decrypt;
		mac->generate = probe.generate;
		frame_active = NULL;
	}
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	test_pair_destroy(&pair);
	result.retired = transport_pair_retired(&pair) && frame_active == NULL;
	result.valid = result.valid && result.retired;
	return result;
}

static int
test_packet_recv_bad_padding(void)
{
	struct frame_registry registry;
	bool valid = frame_registry_prepare(&registry) == 0;
	unsigned cases = 0, successes = 0, lower = 0, upper = 0, empty = 0;
	for (unsigned mode = 0; mode < 3 && valid; mode++) {
		for (unsigned role = 0; role < 2; role++) {
			for (unsigned byte = 0; byte <= UINT8_MAX; byte++) {
				uint8_t padding = (uint8_t)byte;
				int expected = byte >= 4 && byte <= 10 ? 0 : DSSH_ERROR_PARSE;
				struct frame_result result = frame_case(&registry, role == 0, mode, padding, 12, expected, -1);
				valid = result.fixture == 0 && result.ran && result.valid && valid;
				cases++;
				if (byte < 4)
					lower++;
				else if (byte >= 12)
					upper++;
				else if (byte == 11)
					empty++;
				else
					successes++;
			}
		}
	}
	frame_registry_finish();
	int printed = printf("framing_padding cases=%u success=%u lower=%u upper=%u empty=%u modes=3 roles=2 valid=%d\n",
	    cases, successes, lower, upper, empty, valid);
	ASSERT_TRUE(valid && cases == 1536 && successes == 42 && printed > 0);
	return TEST_PASS;
}

static int
test_packet_recv_too_small(void)
{
	struct frame_registry registry;
	bool valid = frame_registry_prepare(&registry) == 0;
	unsigned cases = 0, baselines = 0;
	for (unsigned mode = 0; mode < 3 && valid; mode++) {
		for (unsigned role = 0; role < 2; role++) {
			struct frame_result baseline = frame_case(&registry, role == 0, mode, 4, 12, 0, -1);
			valid = baseline.fixture == 0 && baseline.ran && baseline.valid && valid;
			baselines++;
			for (uint32_t declared = 0; declared < 2; declared++) {
				struct frame_result result = frame_case(&registry, role == 0, mode, 4, declared, DSSH_ERROR_PARSE, -1);
				valid = result.fixture == 0 && result.ran && result.valid && valid;
				cases++;
			}
		}
	}
	frame_registry_finish();
	int printed = printf("framing_small cases=%u baseline=%u modes=3 roles=2 valid=%d\n", cases, baselines, valid);
	ASSERT_TRUE(valid && cases == 12 && baselines == 6 && printed > 0);
	return TEST_PASS;
}

static int
test_packet_framing_boundaries(void)
{
	static const uint32_t lengths[] = {2, 3, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, UINT32_MAX};
	struct frame_registry registry;
	bool valid = frame_registry_prepare(&registry) == 0;
	unsigned cases = 0, baselines = 0, too_long = 0;
	for (unsigned mode = 0; mode < 3 && valid; mode++) {
		for (unsigned role = 0; role < 2; role++) {
			struct frame_result baseline = frame_case(&registry, role == 0, mode, 4, 12, 0, -1);
			valid = baseline.fixture == 0 && baseline.ran && baseline.valid && valid;
			baselines++;
			for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
				int expected = lengths[i] == UINT32_MAX ? DSSH_ERROR_TOOLONG : DSSH_ERROR_PARSE;
				struct frame_result result = frame_case(&registry, role == 0, mode, 4, lengths[i], expected, -1);
				valid = result.fixture == 0 && result.ran && result.valid && valid;
				cases++;
				if (expected == DSSH_ERROR_TOOLONG)
					too_long++;
			}
		}
	}
	frame_registry_finish();
	int printed = printf("framing_lengths cases=%u baseline=%u toolong=%u modes=3 roles=2 valid=%d\n", cases, baselines, too_long, valid);
	ASSERT_TRUE(valid && cases == 84 && baselines == 6 && too_long == 6 && printed > 0);
	return TEST_PASS;
}

static int
test_frame_constructor_refusals(void)
{
	struct frame_registry registry;
	bool valid = frame_registry_prepare(&registry) == 0;
	unsigned refusals = 0, baselines = 0, finals = 0;
	for (unsigned mode = 0; mode < 3 && valid; mode++) {
		for (unsigned role = 0; role < 2; role++) {
			struct frame_result baseline = frame_case(&registry, role == 0, mode, 4, 12, 0, INT_MAX);
			valid = baseline.fixture == 0 && baseline.ran && baseline.valid
			    && baseline.allocations > 0 && baseline.allocations < INT_MAX && valid;
			baselines++;
			for (int i = 0; valid && i < baseline.allocations; i++) {
				struct frame_result result = frame_case(&registry, role == 0, mode, 4, 12, 0, i);
				valid = result.fixture == DSSH_ERROR_ALLOC && !result.ran && result.retired && result.allocations == i + 1;
				if (valid)
					refusals++;
			}
			struct frame_result final = frame_case(&registry, role == 0, mode, 4, 12, 0, baseline.allocations);
			valid = final.fixture == 0 && final.ran && final.valid && final.allocations == baseline.allocations && valid;
			finals++;
		}
	}
	frame_registry_finish();
	int printed = printf("framing_constructors baselines=%u refused=%u final=%u modes=3 roles=2 valid=%d\n",
	    baselines, refusals, finals, valid);
	ASSERT_TRUE(valid && baselines == 6 && finals == 6 && printed > 0);
	return TEST_PASS;
}

#endif
