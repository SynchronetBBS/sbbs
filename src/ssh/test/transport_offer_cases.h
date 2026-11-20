#ifndef DSSH_TEST_TRANSPORT_OFFER_CASES_H
#define DSSH_TEST_TRANSPORT_OFFER_CASES_H

static const char *const offer_minimal_names[10] = {"a", "a", "a", "a", "a", "a", "a", "a", "", ""};

/* Test input may deliberately contain invalid names/booleans. Validate storage
 * and every wire extent, leaving output untouched on preflight refusal. */
static size_t
make_peer_offer(const char *const names[10], uint8_t follows, uint8_t *buf, size_t capacity)
{
	if (names == NULL || buf == NULL || capacity < 62)
		return 0;
	size_t lengths[10];
	size_t needed = 62; /* Message/cookie, ten lengths and complete trailer. */
	for (size_t i = 0; i < 10; i++) {
		if (names[i] == NULL)
			return 0;
		lengths[i] = strlen(names[i]);
#if SIZE_MAX > UINT32_MAX
		if (lengths[i] > UINT32_MAX)
			return 0;
#endif
		if (lengths[i] > SIZE_MAX - needed)
			return 0;
		needed += lengths[i];
	}
	if (needed > capacity)
		return 0;
	buf[0] = SSH_MSG_KEXINIT;
	memset(&buf[1], 0, 16);
	size_t position = 17;
	for (size_t i = 0; i < 10; i++) {
		uint32_t wire_length = (uint32_t)lengths[i];
		if (dssh_serialize_uint32(wire_length, buf, capacity, &position) != 0)
			return 0;
		if (lengths[i] != 0)
			memcpy(&buf[position], names[i], lengths[i]);
		position += lengths[i];
	}
	buf[position++] = follows;
	if (dssh_serialize_uint32(0, buf, capacity, &position) != 0 || position != needed)
		return 0;
	return position;
}

static bool
offer_prefix_matches(const uint8_t *buf, size_t length, const char *const names[10],
    const struct dssh_name_list lists[10], size_t count, size_t *end)
{
	if (count > 10 || length < 17)
		return false;
	size_t position = 17;
	for (size_t i = 0; i < count; i++) {
		if (position > length || length - position < 4)
			return false;
		position += 4;
		size_t expected = strlen(names[i]);
		if (expected > length - position || lists[i].data != &buf[position] || lists[i].len != expected
		    || memcmp(lists[i].data, names[i], expected) != 0)
			return false;
		position += expected;
	}
	*end = position;
	return true;
}

static bool
offer_parse_matches(const uint8_t *buf, size_t length, const char *const names[10], bool expected_follows)
{
	struct dssh_name_list lists[10];
	bool follows = !expected_follows;
	if (parse_peer_kexinit(buf, length, lists, &follows) != 0 || follows != expected_follows)
		return false;
	size_t end = 0;
	return offer_prefix_matches(buf, length, names, lists, 10, &end) && length - end == 5;
}

static bool
offer_parse_failure(const uint8_t *buf, size_t length, const char *const names[10], size_t published)
{
	uint8_t marker = 0;
	struct dssh_name_list lists[10];
	for (size_t i = 0; i < 10; i++)
		lists[i] = (struct dssh_name_list){.data = &marker, .len = 77};
	bool follows = false;
	if (parse_peer_kexinit(buf, length, lists, &follows) != DSSH_ERROR_PARSE || follows)
		return false;
	size_t end = 0;
	if (published != 0 && !offer_prefix_matches(buf, length, names, lists, published, &end))
		return false;
	for (size_t i = published; i < 10; i++) {
		if (lists[i].data != &marker || lists[i].len != 77)
			return false;
	}
	return true;
}

static bool
offer_field_rejected(const char *invalid, const char *companion, size_t field)
{
	if (field >= 10)
		return false;
	const char *names[10];
	for (size_t i = 0; i < 10; i++)
		names[i] = offer_minimal_names[i];
	names[field] = companion;
	uint8_t packet[512];
	size_t length = make_peer_offer(names, 1, packet, sizeof(packet));
	if (length == 0 || !offer_parse_matches(packet, length, names, true))
		return false;
	names[field] = invalid;
	length = make_peer_offer(names, 1, packet, sizeof(packet));
	return length != 0 && offer_parse_failure(packet, length, names, field);
}

static bool
offer_report(const char *kind, unsigned checked, bool valid)
{
	int printed = printf("offer_parse kind=%s checks=%u valid=%d\n", kind, checked, valid);
	return valid && printed > 0;
}

static int
test_kexinit_parse_valid(void)
{
	const char *names[10] = {"curve25519-sha256", "ssh-ed25519", "aes256-ctr", "aes256-ctr",
	    "hmac-sha2-256", "hmac-sha2-256", "none", "none", "", ""};
	uint8_t packet[512];
	size_t length = make_peer_offer(names, 0, packet, sizeof(packet));
	bool valid = length != 0 && offer_parse_matches(packet, length, names, false);
	ASSERT_TRUE(offer_report("valid", 1, valid));
	return TEST_PASS;
}

static int
test_kexinit_parse_control_char(void)
{
	bool valid = true;
	unsigned checked = 0;
	for (size_t field = 0; field < 10; field++) {
		valid = offer_field_rejected("a\001b", "abb", field) && valid;
		checked++;
	}
	ASSERT_TRUE(offer_report("control", checked, valid));
	return TEST_PASS;
}

static int
test_kexinit_parse_name_too_long(void)
{
	char invalid[66], companion[65];
	memset(invalid, 'x', sizeof(invalid) - 1);
	invalid[sizeof(invalid) - 1] = 0;
	memset(companion, 'x', sizeof(companion) - 1);
	companion[sizeof(companion) - 1] = 0;
	bool valid = true;
	unsigned checked = 0;
	for (size_t field = 0; field < 8; field++) {
		valid = offer_field_rejected(invalid, companion, field) && valid;
		checked++;
	}
	ASSERT_TRUE(offer_report("long", checked, valid));
	return TEST_PASS;
}

static int
test_kexinit_parse_truncated(void)
{
	uint8_t packet[128];
	size_t length = make_peer_offer(offer_minimal_names, 1, packet, sizeof(packet));
	bool valid = length != 0 && offer_parse_matches(packet, length, offer_minimal_names, true)
	    && offer_parse_failure(packet, 21, offer_minimal_names, 0);
	ASSERT_TRUE(offer_report("truncated", 1, valid));
	return TEST_PASS;
}

static int
test_kexinit_parse_too_short(void)
{
	uint8_t packet[128];
	size_t length = make_peer_offer(offer_minimal_names, 1, packet, sizeof(packet));
	bool valid = length != 0 && offer_parse_matches(packet, length, offer_minimal_names, true);
	unsigned checked = 0;
	for (size_t truncated = 0; truncated < 17; truncated++) {
		valid = offer_parse_failure(packet, truncated, offer_minimal_names, 0) && valid;
		checked++;
	}
	ASSERT_TRUE(offer_report("short", checked, valid));
	return TEST_PASS;
}

static int
test_kexinit_parse_first_kex_follows(void)
{
	static const uint8_t flags[] = {0, 1, 2, 255};
	uint8_t packet[128];
	bool valid = true;
	unsigned checked = 0;
	for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		size_t length = make_peer_offer(offer_minimal_names, flags[i], packet, sizeof(packet));
		valid = length != 0 && offer_parse_matches(packet, length, offer_minimal_names, flags[i] != 0) && valid;
		checked++;
	}
	ASSERT_TRUE(offer_report("follows", checked, valid));
	return TEST_PASS;
}

static int
test_kexinit_peer_parse_truncated_namelist(void)
{
	const char *names[10];
	for (size_t i = 0; i < 10; i++)
		names[i] = offer_minimal_names[i];
	names[0] = "abcde";
	uint8_t packet[128];
	size_t length = make_peer_offer(names, 1, packet, sizeof(packet));
	bool valid = length != 0 && offer_parse_matches(packet, length, names, true)
	    && offer_parse_failure(packet, 23, names, 0);
	ASSERT_TRUE(offer_report("partial-name", 1, valid));
	return TEST_PASS;
}

static int
test_kexinit_complete_fields(void)
{
	uint8_t packet[512];
	size_t length = make_peer_offer(offer_minimal_names, 1, packet, sizeof(packet));
	bool valid = length > 5 && length < sizeof(packet)
	    && offer_parse_matches(packet, length, offer_minimal_names, true);
	unsigned checked = 1;
	if (valid) {
		for (size_t missing = 1; missing <= 5; missing++) {
			valid = offer_parse_failure(packet, length - missing, offer_minimal_names, 10) && valid;
			checked++;
		}
		packet[length] = 0;
		valid = offer_parse_failure(packet, length + 1, offer_minimal_names, 10) && valid;
		checked++;
		packet[0] = SSH_MSG_NEWKEYS;
		valid = offer_parse_failure(packet, length, offer_minimal_names, 0) && valid;
		checked++;
		packet[0] = SSH_MSG_KEXINIT;
		packet[length - 4] = 0xde;
		packet[length - 3] = 0xad;
		packet[length - 2] = 0xbe;
		packet[length - 1] = 0xef;
		valid = offer_parse_matches(packet, length, offer_minimal_names, true) && valid;
		checked++;
	}
	for (size_t field = 0; field < 8; field++) {
		valid = offer_field_rejected("", "a", field) && valid;
		checked++;
	}
	static const char *const invalid[] = {",a", "a,", "a,,b", "a b", "a\001b", "a\177b"};
	for (size_t field = 0; field < 10; field++) {
		for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
			valid = offer_field_rejected(invalid[i], "a", field) && valid;
			checked++;
		}
	}
	ASSERT_TRUE(offer_report("fields", checked, valid));
	return TEST_PASS;
}

static int
test_peer_offer_builder(void)
{
	static const uint8_t expected[70] = {20,
	    [20] = 1, [21] = 'a', [25] = 1, [26] = 'a', [30] = 1, [31] = 'a',
	    [35] = 1, [36] = 'a', [40] = 1, [41] = 'a', [45] = 1, [46] = 'a',
	    [50] = 1, [51] = 'a', [55] = 1, [56] = 'a'};
	uint8_t guarded[72], original[72];
	memset(original, 0xa5, sizeof(original));
	bool valid = true;
	unsigned capacities = 0;
	for (size_t capacity = 0; capacity < sizeof(expected); capacity++) {
		memcpy(guarded, original, sizeof(guarded));
		size_t length = make_peer_offer(offer_minimal_names, 0, &guarded[1], capacity);
		valid = length == 0 && memcmp(guarded, original, sizeof(guarded)) == 0 && valid;
		capacities++;
	}
	size_t length = make_peer_offer(offer_minimal_names, 0, &guarded[1], sizeof(expected));
	valid = length == sizeof(expected) && memcmp(&guarded[1], expected, sizeof(expected)) == 0
	    && guarded[0] == 0xa5 && guarded[sizeof(guarded) - 1] == 0xa5 && valid;
	memcpy(guarded, original, sizeof(guarded));
	valid = make_peer_offer(NULL, 0, &guarded[1], sizeof(expected)) == 0
	    && make_peer_offer(offer_minimal_names, 0, NULL, sizeof(expected)) == 0 && valid;
	const char *names[10];
	for (size_t field = 0; field < 10; field++) {
		for (size_t i = 0; i < 10; i++)
			names[i] = i == field ? NULL : offer_minimal_names[i];
		valid = make_peer_offer(names, 0, &guarded[1], sizeof(expected)) == 0
		    && memcmp(guarded, original, sizeof(guarded)) == 0 && valid;
	}
	int printed = printf("offer_builder capacities=%u exact=%zu valid=%d\n", capacities, sizeof(expected), valid);
	ASSERT_TRUE(valid && printed > 0);
	return TEST_PASS;
}

#endif
