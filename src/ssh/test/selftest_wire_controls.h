/* Observe actual authenticated receives and connection send commits.
 * No packet, session or callback borrow survives its actual owner. */
static const uint8_t owner_wire_lang[] = {
	0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x65, 0x6e, 0x76,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x4c, 0x41, 0x4e, 0x47, 0x00, 0x00, 0x00,
	0x0b, 0x65, 0x6e, 0x5f, 0x55, 0x53, 0x2e, 0x55, 0x54, 0x46, 0x2d, 0x38,
};
static const uint8_t owner_wire_term[] = {
	0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x65, 0x6e, 0x76,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x54, 0x45, 0x52, 0x4d, 0x00, 0x00, 0x00,
	0x05, 0x78, 0x74, 0x65, 0x72, 0x6d,
};
static const uint8_t owner_wire_exec[] = {
	0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x65, 0x78, 0x65,
	0x63, 0x01, 0x00, 0x00, 0x00, 0x08, 0x65, 0x6e, 0x76, 0x2d, 0x74, 0x65,
	0x73, 0x74,
};

enum owner_wire_kind {
	OWNER_WIRE_OTHER, OWNER_WIRE_LANG, OWNER_WIRE_TERM, OWNER_WIRE_EXEC,
	OWNER_WIRE_SUCCESS, OWNER_WIRE_FAILURE, OWNER_WIRE_BAD, OWNER_WIRE_KINDS
};
static atomic_bool owner_wire_enabled;
static atomic_bool owner_wire_valid;
static atomic_bool owner_wire_changed;
static atomic_uint owner_wire_sent[OWNER_WIRE_KINDS];
static atomic_uint owner_wire_received[OWNER_WIRE_KINDS];
static unsigned owner_wire_omissions;
static dssh_chan_break_cb owner_wire_break_callback;
static void *owner_wire_break_data;
static atomic_bool owner_wire_break_seen;
static atomic_bool owner_wire_break_duplicate;

int __real_send_commit(dssh_session, size_t, uint32_t *);
int __real_recv_packet(dssh_session, uint8_t *, uint8_t **, size_t *);

static enum owner_wire_kind
owner_wire_kind(const uint8_t *packet, size_t length)
{
	static const uint8_t env[] = { 0, 0, 0, 3, 'e', 'n', 'v' };
	static const uint8_t exec[] = { 0, 0, 0, 4, 'e', 'x', 'e', 'c' };
	static const uint8_t success[] = { SSH_MSG_CHANNEL_SUCCESS, 0, 0, 0, 0 };
	static const uint8_t failure[] = { SSH_MSG_CHANNEL_FAILURE, 0, 0, 0, 0 };
	if (length == 0)
		return OWNER_WIRE_OTHER;
	if (packet[0] == SSH_MSG_CHANNEL_SUCCESS)
		return length == sizeof(success) && memcmp(packet, success, length) == 0
		    ? OWNER_WIRE_SUCCESS : OWNER_WIRE_BAD;
	if (packet[0] == SSH_MSG_CHANNEL_FAILURE)
		return length == sizeof(failure) && memcmp(packet, failure, length) == 0
		    ? OWNER_WIRE_FAILURE : OWNER_WIRE_BAD;
	if (packet[0] != SSH_MSG_CHANNEL_REQUEST)
		return OWNER_WIRE_OTHER;
	if (length >= 12 && memcmp(packet + 5, env, sizeof(env)) == 0) {
		if (length == sizeof(owner_wire_lang) && memcmp(packet, owner_wire_lang, length) == 0)
			return OWNER_WIRE_LANG;
		if (length == sizeof(owner_wire_term) && memcmp(packet, owner_wire_term, length) == 0)
			return OWNER_WIRE_TERM;
		return OWNER_WIRE_BAD;
	}
	if (length >= 13 && memcmp(packet + 5, exec, sizeof(exec)) == 0)
		return length == sizeof(owner_wire_exec) && memcmp(packet, owner_wire_exec, length) == 0
		    ? OWNER_WIRE_EXEC : OWNER_WIRE_BAD;
	return OWNER_WIRE_OTHER;
}

static void
owner_wire_record(enum owner_wire_kind kind, bool client, bool sending)
{
	if (kind == OWNER_WIRE_OTHER)
		return;
	bool request = kind == OWNER_WIRE_LANG || kind == OWNER_WIRE_TERM || kind == OWNER_WIRE_EXEC;
	if (kind == OWNER_WIRE_BAD || request != (client == sending))
		atomic_store(&owner_wire_valid, false);
	atomic_uint *counts = sending ? owner_wire_sent : owner_wire_received;
	if ((kind == OWNER_WIRE_TERM && atomic_load(&counts[OWNER_WIRE_LANG]) != 1)
	    || (kind == OWNER_WIRE_EXEC && (atomic_load(&counts[OWNER_WIRE_LANG]) != 1
	        || atomic_load(&counts[OWNER_WIRE_TERM]) != 1)))
		atomic_store(&owner_wire_valid, false);
	unsigned count = atomic_load(&counts[kind]);
	for (;;) {
		if (count >= 4) {
			atomic_store(&owner_wire_valid, false);
			return;
		}
		if (atomic_compare_exchange_weak(&counts[kind], &count, count + 1))
			return;
	}
}

int
__wrap_send_commit(dssh_session sess, size_t length, uint32_t *sequence)
{
	if (!atomic_load(&owner_wire_enabled))
		return __real_send_commit(sess, length, sequence);
	enum owner_wire_kind kind = OWNER_WIRE_BAD;
	if (sess->trans.tx_packet != NULL && sess->trans.packet_buf_sz >= 5
	    && length <= sess->trans.packet_buf_sz - 5) {
		uint8_t *packet = sess->trans.tx_packet + 9;
		kind = owner_wire_kind(packet, length);
		if (sess->trans.client && kind == OWNER_WIRE_LANG
		    && strcmp(owner_mode, "wire-env-reply") == 0
		    && !atomic_exchange(&owner_wire_changed, true)) {
			/* This exact validated frame has its boolean at byte12. */
			packet[12] = 1;
			kind = owner_wire_kind(packet, length);
		}
	}
	bool client = sess->trans.client;
	int ret = __real_send_commit(sess, length, sequence);
	if (ret == 0)
		owner_wire_record(kind, client, true);
	else if (kind != OWNER_WIRE_OTHER)
		atomic_store(&owner_wire_valid, false);
	return ret;
}

int
__wrap_recv_packet(dssh_session sess, uint8_t *type, uint8_t **packet, size_t *length)
{
	int ret = __real_recv_packet(sess, type, packet, length);
	if (ret == 0 && atomic_load(&owner_wire_enabled)) {
		if (type == NULL || packet == NULL || length == NULL || *packet == NULL
		    || *length == 0 || (*packet)[0] != *type)
			atomic_store(&owner_wire_valid, false);
		else
			owner_wire_record(owner_wire_kind(*packet, *length), sess->trans.client, false);
	}
	return ret;
}

static int
owner_env(struct dssh_chan_params *params, const char *name, const char *value)
{
	if (strcmp(owner_mode, "wire-env-omitted") != 0)
		return dssh_chan_params_add_env(params, name, value);
	if (owner_wire_omissions >= 2 || name == NULL || value == NULL
	    || (owner_wire_omissions == 0
	        ? strcmp(name, "LANG") != 0 || strcmp(value, "en_US.UTF-8") != 0
	        : strcmp(name, "TERM") != 0 || strcmp(value, "xterm") != 0))
		return DSSH_ERROR_INVALID;
	owner_wire_omissions++;
	return 0;
}

static bool
owner_wire_break(dssh_channel ch, const struct dssh_chan_break_request *request, void *data)
{
	if (owner_wire_break_callback != selftest_break_action || data != owner_wire_break_data)
		abort();
	bool result = owner_wire_break_callback(ch, request, data);
	atomic_store(&owner_wire_break_seen, true);
	if (strcmp(owner_mode, "wire-break-duplicate") == 0) {
		bool second = owner_wire_break_callback(ch, request, data);
		atomic_store(&owner_wire_break_duplicate, !second);
	}
	return result;
}

static int
owner_break(dssh_session sess, dssh_chan_break_cb callback, void *data)
{
	if (strncmp(owner_mode, "wire-break-", 11) != 0)
		return dssh_session_set_break_cb(sess, callback, data);
	if (sess == NULL || sess->trans.client || callback != selftest_break_action || data == NULL)
		return DSSH_ERROR_INVALID;
	int ret = dssh_session_set_break_cb(sess, owner_wire_break, data);
	if (ret == 0) {
		owner_wire_break_callback = callback;
		owner_wire_break_data = data;
	}
	return ret;
}

static void
owner_wire_server_retired(void)
{
	/* Session cleanup has joined its demux before this stack borrow ends. */
	owner_wire_break_callback = NULL;
	owner_wire_break_data = NULL;
}

static int
owner_wire_control(void)
{
	bool env = strncmp(owner_mode, "wire-env-", 9) == 0;
	bool omitted = strcmp(owner_mode, "wire-env-omitted") == 0;
	bool changed = strcmp(owner_mode, "wire-env-reply") == 0;
	bool duplicate = strcmp(owner_mode, "wire-break-duplicate") == 0;
	if (!omitted && !changed && !duplicate && strcmp(owner_mode, "wire-env-native") != 0
	    && strcmp(owner_mode, "wire-break-native") != 0)
		return 2;
	atomic_init(&owner_wire_valid, true);
	atomic_init(&owner_wire_changed, false);
	atomic_init(&owner_wire_break_seen, false);
	atomic_init(&owner_wire_break_duplicate, false);
	for (size_t i = 0; i < OWNER_WIRE_KINDS; i++) {
		atomic_init(&owner_wire_sent[i], 0);
		atomic_init(&owner_wire_received[i], 0);
	}
	atomic_store(&owner_wire_enabled, env);
	int result = env ? test_self_chan_accept_env_reject() : test_self_chan_send_break();
	atomic_store(&owner_wire_enabled, false);
	bool oracle = atomic_load(&owner_wire_valid);
	for (size_t kind = OWNER_WIRE_LANG; kind <= OWNER_WIRE_BAD; kind++) {
		unsigned expected = kind <= OWNER_WIRE_SUCCESS ? 1 : 0;
		oracle &= atomic_load(&owner_wire_sent[kind]) == expected
		    && atomic_load(&owner_wire_received[kind]) == expected;
	}
	bool expected_result = duplicate ? result == TEST_FAIL
	    : changed ? result == TEST_PASS || result == TEST_FAIL : result == TEST_PASS;
	bool valid = expected_result
	    && g_active_ctx == NULL && owner_sessions == 2 && owner_released == 2
	    && owner_closed == 2 && owner_created == 5 && owner_joined == 5
	    && owner_join_attempts == 5 && owner_attempts == 5 && owner_param_clean
	    && owner_wire_break_callback == NULL && owner_wire_break_data == NULL
	    && atomic_load(&owner_channels_created) == atomic_load(&owner_channels_closed);
	if (env) {
		valid &= oracle == (!omitted && !changed)
		    && owner_wire_omissions == (omitted ? 2U : 0U)
		    && atomic_load(&owner_wire_changed) == changed;
		owner_note("SELFTEST_WIRE_ENV mode=%s tx_lang=%u rx_lang=%u tx_term=%u rx_term=%u tx_exec=%u rx_exec=%u tx_success=%u rx_success=%u tx_failure=%u rx_failure=%u tx_bad=%u rx_bad=%u omitted=%u changed=%u oracle=%u\n",
		    owner_mode, atomic_load(&owner_wire_sent[OWNER_WIRE_LANG]),
		    atomic_load(&owner_wire_received[OWNER_WIRE_LANG]),
		    atomic_load(&owner_wire_sent[OWNER_WIRE_TERM]), atomic_load(&owner_wire_received[OWNER_WIRE_TERM]),
		    atomic_load(&owner_wire_sent[OWNER_WIRE_EXEC]), atomic_load(&owner_wire_received[OWNER_WIRE_EXEC]),
		    atomic_load(&owner_wire_sent[OWNER_WIRE_SUCCESS]), atomic_load(&owner_wire_received[OWNER_WIRE_SUCCESS]),
		    atomic_load(&owner_wire_sent[OWNER_WIRE_FAILURE]), atomic_load(&owner_wire_received[OWNER_WIRE_FAILURE]),
		    atomic_load(&owner_wire_sent[OWNER_WIRE_BAD]), atomic_load(&owner_wire_received[OWNER_WIRE_BAD]),
		    owner_wire_omissions, atomic_load(&owner_wire_changed) ? 1U : 0U, oracle ? 1U : 0U);
	}
	else
		valid &= atomic_load(&owner_wire_break_seen) && atomic_load(&owner_wire_break_duplicate) == duplicate;
	owner_note("SELFTEST_WIRE mode=%s host=%d duplicate=%u created=%u joined=%u sessions=%u released=%u fds=%u channels=%u closed=%u borrowed=%u valid=%u\n",
	    owner_mode, result, atomic_load(&owner_wire_break_duplicate) ? 1U : 0U,
	    owner_created, owner_joined, owner_sessions, owner_released, owner_closed,
	    atomic_load(&owner_channels_created), atomic_load(&owner_channels_closed),
	    owner_wire_break_data != NULL ? 1U : 0U, valid ? 1U : 0U);
	return valid ? 0 : 2;
}
