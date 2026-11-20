#ifndef DSSH_TEST_REGISTRATION_CASES_H
#define DSSH_TEST_REGISTRATION_CASES_H

enum registration_kind { REG_KEX, REG_KEY, REG_ENC, REG_MAC, REG_COMP, REG_KINDS };
enum registration_mode { REG_LATE, REG_EMPTY, REG_LONG, REG_LONG_EMPTY, REG_NEXT, REG_MANY, REG_ACCEPT };
enum registration_fault { REG_FAULT_NONE, REG_FAULT_BASE, REG_FAULT_ITEM, REG_FAULT_SESSION };

struct registration_view {
	void *head;
	void *tail;
	size_t count;
};

static struct registration_view
registration_view(enum registration_kind kind)
{
#define VIEW(tag, field) case tag: return (struct registration_view){gconf.field##_head, gconf.field##_tail, gconf.field##_entries}
	switch (kind) {
		VIEW(REG_KEX, kex);
		VIEW(REG_KEY, key_algo);
		VIEW(REG_ENC, enc);
		VIEW(REG_MAC, mac);
		VIEW(REG_COMP, comp);
		default:
			return (struct registration_view){0};
	}
#undef VIEW
}

/* All real users are stopped. Restore publication even after an unexpected
 * acceptance, so the private reset never owns the separately retired probe. */
static void
registration_restore(enum registration_kind kind, struct registration_view view)
{
#define RESTORE(tag, type, field) \
	case tag: { \
		type head = view.head; \
		type tail = view.tail; \
		gconf.field##_head = head; \
		gconf.field##_tail = tail; \
		gconf.field##_entries = view.count; \
		if (tail != NULL) \
			tail->next = NULL; \
		break; \
	}
	switch (kind) {
		RESTORE(REG_KEX, dssh_kex, kex)
		RESTORE(REG_KEY, dssh_key_algo, key_algo)
		RESTORE(REG_ENC, dssh_enc, enc)
		RESTORE(REG_MAC, dssh_mac, mac)
		RESTORE(REG_COMP, dssh_comp, comp)
		default:
			break;
	}
#undef RESTORE
}

/* malloc provides alignment; typed assignment establishes the descriptor's
 * effective type and proper pointer values. These are private rejected probes,
 * not copies retained by the production registry. No callback owns new state. */
static void *
registration_item(enum registration_kind kind, bool linked, char **name)
{
#define ITEM(tag, type, field, extra) \
	case tag: { \
		type item; \
		if (sizeof(*item) > SIZE_MAX - 66) \
			return NULL; \
		item = dssh_test_malloc(sizeof(*item) + 66); \
		if (item == NULL) \
			return NULL; \
		*item = *gconf.field##_head; \
		item->next = linked ? item : NULL; \
		extra; \
		memset(item->name, 0, 66); \
		memcpy(item->name, "audit-probe", sizeof("audit-probe")); \
		*name = item->name; \
		return item; \
	}
	switch (kind) {
		ITEM(REG_KEX, dssh_kex, kex, (void)0)
		ITEM(REG_KEY, dssh_key_algo, key_algo, item->ctx = NULL)
		ITEM(REG_ENC, dssh_enc, enc, (void)0)
		ITEM(REG_MAC, dssh_mac, mac, (void)0)
		ITEM(REG_COMP, dssh_comp, comp, (void)0)
		default:
			return NULL;
	}
#undef ITEM
}

static int
registration_submit(enum registration_kind kind, void *storage)
{
#define SUBMIT(tag, type, suffix) \
	case tag: { \
		type item = storage; \
		return dssh_transport_register_##suffix(item); \
	}
	switch (kind) {
		SUBMIT(REG_KEX, dssh_kex, kex)
		SUBMIT(REG_KEY, dssh_key_algo, key_algo)
		SUBMIT(REG_ENC, dssh_enc, enc)
		SUBMIT(REG_MAC, dssh_mac, mac)
		SUBMIT(REG_COMP, dssh_comp, comp)
		default:
			return DSSH_ERROR_INVALID;
	}
#undef SUBMIT
}

static void *
registration_next(enum registration_kind kind, void *storage)
{
#define NEXT(tag, type) \
	case tag: { \
		type item = storage; \
		return item->next; \
	}
	switch (kind) {
		NEXT(REG_KEX, dssh_kex)
		NEXT(REG_KEY, dssh_key_algo)
		NEXT(REG_ENC, dssh_enc)
		NEXT(REG_MAC, dssh_mac)
		NEXT(REG_COMP, dssh_comp)
		default:
			return NULL;
	}
#undef NEXT
}

struct registration_result {
	bool valid;
	enum registration_fault refused;
	int calls;
};

static struct registration_result
registration_exercise(enum registration_kind kind, enum registration_mode mode,
    enum registration_fault fault)
{
	struct registration_result result = {0};
	struct registration_view before[REG_KINDS];
	bool prepared = false;
	dssh_session sess = NULL;
	void *item = NULL;
	char *name = NULL;
	dssh_test_reset_global_config();
	if (fault == REG_FAULT_BASE)
		dssh_test_alloc_fail_after(0);
	int ret = register_all_algorithms();
	if (fault == REG_FAULT_BASE) {
		result.calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		if (ret == DSSH_ERROR_ALLOC && result.calls == 1)
			result.refused = fault;
	}
	if (ret != 0)
		goto done;
	for (unsigned i = 0; i < REG_KINDS; i++) {
		enum registration_kind current = (enum registration_kind)i;
		before[i] = registration_view(current);
		if (before[i].head == NULL || before[i].tail == NULL || before[i].count == 0)
			goto done;
	}
	prepared = true;
	if (fault == REG_FAULT_ITEM)
		dssh_test_alloc_fail_after(0);
	item = registration_item(kind, mode == REG_NEXT, &name);
	if (fault == REG_FAULT_ITEM) {
		result.calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		if (item == NULL && result.calls == 1)
			result.refused = fault;
	}
	if (item == NULL)
		goto done;
	int expected = DSSH_ERROR_INVALID;
	if (mode == REG_LATE) {
		ret = dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    mock_rxline_dispatch, mock_extra_line_cb);
		if (ret != 0)
			goto done;
		if (fault == REG_FAULT_SESSION)
			dssh_test_alloc_fail_after(0);
		sess = dssh_session_init(true, 0);
		if (fault == REG_FAULT_SESSION) {
			result.calls = dssh_test_alloc_count();
			dssh_test_alloc_reset();
			if (sess == NULL && result.calls == 1)
				result.refused = fault;
		}
		if (sess == NULL || !gconf.used)
			goto done;
		expected = DSSH_ERROR_TOOLATE;
	}
	else if (mode == REG_LONG || mode == REG_LONG_EMPTY) {
		memset(name, 'x', 65);
		name[65] = 0;
		expected = DSSH_ERROR_TOOLONG;
	}
	else if (mode == REG_EMPTY) {
		name[0] = 0;
	}
	else if (mode == REG_NEXT) {
		expected = DSSH_ERROR_MUST_BE_NULL;
	}
	else if (mode == REG_MANY) {
		struct registration_view full = before[kind];
		full.count = SIZE_MAX - 1;
		registration_restore(kind, full);
		expected = DSSH_ERROR_TOOMANY;
	}
	else if (mode == REG_ACCEPT) {
		if (before[kind].count >= SIZE_MAX - 1)
			goto done;
		expected = 0;
	}
	char original[66];
	memcpy(original, name, sizeof(original));
	ret = registration_submit(kind, item);
	result.valid = ret == expected && memcmp(name, original, sizeof(original)) == 0;
	if (mode == REG_LONG_EMPTY && result.valid) {
		name[0] = 0;
		original[0] = 0;
		result.valid = registration_submit(kind, item) == DSSH_ERROR_INVALID
		    && memcmp(name, original, sizeof(original)) == 0;
	}
	void *expected_next = mode == REG_NEXT ? item : NULL;
	result.valid = result.valid && registration_next(kind, item) == expected_next;
	for (unsigned i = 0; i < REG_KINDS; i++) {
		enum registration_kind current = (enum registration_kind)i;
		struct registration_view after = registration_view(current);
		size_t count = mode == REG_MANY && current == kind ? SIZE_MAX - 1 : before[i].count;
		void *tail = before[i].tail;
		if (mode == REG_ACCEPT && current == kind) {
			count++;
			tail = item;
		}
		result.valid = result.valid && after.head == before[i].head
		    && after.tail == tail && after.count == count;
	}
done:
	dssh_test_alloc_reset();
	if (sess != NULL)
		dssh_session_cleanup(sess);
	if (prepared) {
		for (unsigned i = 0; i < REG_KINDS; i++) {
			enum registration_kind current = (enum registration_kind)i;
			registration_restore(current, before[i]);
		}
	}
	dssh_test_reset_global_config();
	free(item);
	return result;
}

static bool
registration_case(enum registration_kind kind, enum registration_mode mode)
{
	struct registration_result result = registration_exercise(kind, mode, REG_FAULT_NONE);
	return result.valid && result.refused == REG_FAULT_NONE;
}

static int
test_registration_fixture_refusal(void)
{
	for (unsigned i = 0; i < REG_KINDS; i++) {
		enum registration_kind kind = (enum registration_kind)i;
		for (unsigned j = REG_FAULT_BASE; j <= REG_FAULT_SESSION; j++) {
			enum registration_fault fault = (enum registration_fault)j;
			struct registration_result result = registration_exercise(kind, REG_LATE, fault);
			int printed = printf("registration_refusal kind=%u fault=%u reached=%d calls=%d\n",
			    i, j, result.refused == fault, result.calls);
			ASSERT_TRUE(!result.valid && result.refused == fault && result.calls == 1
			    && printed > 0
			    && !gconf.used && gconf.kex_head == NULL && gconf.key_algo_head == NULL
			    && gconf.enc_head == NULL && gconf.mac_head == NULL && gconf.comp_head == NULL);
		}
	}
	return TEST_PASS;
}

static int
test_registration_usable_probes(void)
{
	for (unsigned i = 0; i < REG_KINDS; i++) {
		enum registration_kind kind = (enum registration_kind)i;
		ASSERT_TRUE(registration_case(kind, REG_ACCEPT));
	}
	return TEST_PASS;
}

static int
test_registration_module_allocation(void)
{
	struct {
		int (*create)(void);
		enum registration_kind kind;
	} constructors[] = {
		{dssh_register_curve25519_sha256, REG_KEX},
		{dssh_register_dh_gex_sha256, REG_KEX},
		{dssh_register_dh_group14_sha256, REG_KEX},
		{dssh_register_dh_group16_sha512, REG_KEX},
		{dssh_register_sntrup761x25519_sha512, REG_KEX},
		{dssh_register_mlkem768x25519_sha256, REG_KEX},
		{dssh_register_ssh_ed25519, REG_KEY},
		{dssh_register_rsa_sha2_256, REG_KEY},
		{dssh_register_rsa_sha2_512, REG_KEY},
		{dssh_register_aes256_ctr, REG_ENC},
		{dssh_register_aes128_cbc, REG_ENC},
		{dssh_register_none_enc, REG_ENC},
		{dssh_register_hmac_sha2_256, REG_MAC},
		{dssh_register_hmac_sha2_512, REG_MAC},
		{dssh_register_none_mac, REG_MAC},
		{dssh_register_none_comp, REG_COMP},
	};
	for (size_t i = 0; i < sizeof(constructors) / sizeof(constructors[0]); i++) {
		for (int point = 0; point < 2; point++) {
			dssh_test_reset_global_config();
			dssh_test_alloc_fail_after(point);
			int ret = constructors[i].create();
			int calls = dssh_test_alloc_count();
			dssh_test_alloc_reset();
			bool valid = calls == 1 && ret == (point == 0 ? DSSH_ERROR_ALLOC : 0);
			for (unsigned j = 0; j < REG_KINDS; j++) {
				enum registration_kind kind = (enum registration_kind)j;
				struct registration_view view = registration_view(kind);
				bool accepted = point != 0 && kind == constructors[i].kind;
				valid = valid && view.count == (accepted ? 1U : 0U)
				    && view.head == view.tail && (view.head != NULL) == accepted;
			}
			dssh_test_reset_global_config();
			int printed = printf("registration_module index=%zu point=%d calls=%d valid=%d\n",
			    i, point, calls, valid);
			ASSERT_TRUE(valid && printed > 0);
		}
	}
	return TEST_PASS;
}

#endif
