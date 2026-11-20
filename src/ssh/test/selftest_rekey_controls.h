/* Caller-only omission controls; the native exchange path is never forged. */
static unsigned owner_rekey_arms;
static unsigned owner_rekey_reads;
static unsigned owner_rekey_initial[2];
static unsigned owner_rekey_maximum[2];
static bool owner_rekey_omitted;

static bool
owner_rekey_selected(void)
{
	return strncmp(owner_mode, "rekey-", 6) == 0;
}

static void
owner_rekey_arm(dssh_session session, uint64_t value)
{
	if (!owner_rekey_selected()) {
		dssh_test_arm_rx_bytes_before_count(session, value);
		return;
	}
	if (!owner_main || owner_rekey_arms != 0)
		abort();
	owner_rekey_arms++;
	owner_rekey_omitted = strstr(owner_mode, "-omitted") != NULL;
	if (!owner_rekey_omitted)
		dssh_test_arm_rx_bytes_before_count(session, value);
}

static unsigned
owner_rekey_count(dssh_session session)
{
	unsigned value = dssh_test_rekeys_completed(session);

	if (owner_rekey_selected()) {
		if (!owner_main || owner_rekey_reads == UINT_MAX)
			abort();
		/* The snapshot and predicate query both peers. Only
		 * scalars survive host teardown; retain no borrowed handles. */
		unsigned index = session->trans.client ? 0U : 1U;
		if (owner_rekey_reads < 2)
			owner_rekey_initial[index] = value;
		if (value > owner_rekey_maximum[index])
			owner_rekey_maximum[index] = value;
		owner_rekey_reads++;
	}
	return value;
}

static bool
owner_rekey_boundaries(void)
{
	static const struct {
		unsigned before[2], client, server;
		bool advanced;
	} cases[] = {
		{ { 0, 0 }, 0, 0, false }, { { 0, 0 }, 1, 0, false },
		{ { 0, 0 }, 0, 1, false }, { { 0, 0 }, 1, 1, true },
		{ { 2, 3 }, 2, 4, false }, { { 2, 3 }, 3, 3, false },
		{ { 2, 3 }, 1, 4, false }, { { 2, 3 }, 3, 2, false },
		{ { 2, 3 }, 3, 4, true },
		{ { UINT_MAX - 1, UINT_MAX - 1 }, UINT_MAX, UINT_MAX, true },
		{ { UINT_MAX, 0 }, UINT_MAX, 1, false },
		{ { 0, UINT_MAX }, 1, UINT_MAX, false },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		if (selftest_rekey_advanced(cases[i].client, cases[i].server,
		    cases[i].before) != cases[i].advanced)
			return false;
	}
	owner_note("SELFTEST_REKEY_BOUNDARIES cases=12 valid=1\n");
	return true;
}

static int
owner_rekey_control(void)
{
	if (strcmp(owner_mode, "rekey-boundaries") == 0)
		return owner_rekey_boundaries() ? 0 : 2;
	int host;

	if (strncmp(owner_mode, "rekey-data-", 11) == 0)
		host = test_self_rekey_during_data();
	else if (strncmp(owner_mode, "rekey-manual-", 13) == 0)
		host = test_self_rekey_manual();
	else if (strncmp(owner_mode, "rekey-channels-", 15) == 0)
		host = test_self_rekey_preserves_channels();
	else if (strncmp(owner_mode, "rekey-inflight-", 15) == 0)
		host = test_self_rekey_inflight_data();
	else
		return 2;
	bool native = strstr(owner_mode, "-native") != NULL;
	bool valid = host == (native ? TEST_PASS : TEST_FAIL)
	    && owner_rekey_omitted == !native && owner_rekey_arms == 1
	    && owner_rekey_reads >= 4 && owner_rekey_initial[0] == 0
	    && owner_rekey_initial[1] == 0
	    && (native ? owner_rekey_maximum[0] > 0 && owner_rekey_maximum[1] > 0
	        : owner_rekey_maximum[0] == 0 && owner_rekey_maximum[1] == 0)
	    && g_active_ctx == NULL && owner_created == 5 && owner_joined == 5
	    && owner_sessions == 2 && owner_released == 2 && owner_closed == 2
	    && atomic_load(&owner_channels_created) == 2
	    && atomic_load(&owner_channels_closed) == 2 && owner_param_clean;
	owner_note("SELFTEST_REKEY mode=%s host=%d omitted=%u arms=%u client=%u server=%u created=%u joined=%u sessions=%u released=%u fds=%u channels=%u closed=%u valid=%u\n",
	    owner_mode, host, owner_rekey_omitted ? 1U : 0U, owner_rekey_arms,
	    owner_rekey_maximum[0], owner_rekey_maximum[1], owner_created, owner_joined,
	    owner_sessions, owner_released, owner_closed, atomic_load(&owner_channels_created),
	    atomic_load(&owner_channels_closed), valid ? 1U : 0U);
	return valid ? 0 : 2;
}
