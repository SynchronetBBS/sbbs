/* Exercise RIP terminal replacement and input ordering through the production
 * parser and bitmap driver. SYNCVIEW omits session transfers and cache I/O. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define SYNCVIEW
#include "ripper.c"
#include <assert.h>

struct cterminal *cterm;
struct terminal term;
struct syncterm_settings settings;

struct output {
	char bytes[128];
	size_t length;
};

static struct output keys, replies, wire_text, wire_replies;
static unsigned mode_changes, audio_queries, size_changes;
static void *mode_data;
static void capture_output(const char *bytes, size_t length, void *data);

void
get_term_win_size(int *width, int *height, int *pixelw, int *pixelh, int *nostatus)
{
	if (width)
		*width = 80;
	if (height)
		*height = 43;
	if (pixelw)
		*pixelw = 640;
	if (pixelh)
		*pixelh = 350;
	if (nostatus)
		*nostatus = 1;
}

int
conn_send(const void *bytes, size_t size, unsigned timeout)
{
	(void)timeout;
	capture_output(bytes, size, &wire_replies);
	return size;
}

static void
capture_output(const char *bytes, size_t length, void *data)
{
	struct output *out = data;
	assert(out->length + length <= sizeof(out->bytes));
	memcpy(out->bytes + out->length, bytes, length);
	out->length += length;
}

static void
capture_mode(int enable, void *data)
{
	assert(enable == 1);
	assert(data == mode_data);
	mode_changes++;
}

static void
capture_audio(struct cterminal *c, int count, const uint64_t *params)
{
	assert(c == cterm && count == 1 && params[0] == 7);
	audio_queries++;
}

static void
capture_size(struct cterminal *c, int cols, int rows, int pw, int ph, void *data)
{
	assert(c == cterm && data == &size_changes);
	assert(cols == c->width && rows == c->height);
	(void)pw;
	(void)ph;
	size_changes++;
}

static void
reset(int version)
{
	if (cterm)
		cterm_end(cterm, 0);
	textmode(C80X43);
	cterm = cterm_init(43, 80, 0, 0, 0, 80, NULL, CTERM_EMULATION_ANSI_BBS);
	assert(cterm);
	init_rip_ver(version);
	cterm_write(cterm, "\x1b[H", 3, NULL);
	memset(&keys, 0, sizeof(keys));
	memset(&replies, 0, sizeof(replies));
	memset(&wire_text, 0, sizeof(wire_text));
	memset(&wire_replies, 0, sizeof(wire_replies));
	mode_changes = audio_queries = size_changes = 0;
}

static void
feed(const void *bytes, size_t length)
{
	uint8_t buffer[8192];
	unsigned calls = 0;
	assert(length + RIP_ESC_MAX < sizeof(buffer));
	memcpy(buffer, bytes, length);
	do {
		size_t n = parse_rip(buffer, length, sizeof(buffer));
		if (n) {
			capture_output((const char *)buffer, n, &wire_text);
			cterm_write(cterm, buffer, n, NULL);
		}
		length = 0;
		assert(++calls < 1000);
	} while (cb.defer_len);
}

static void
write_terminal(const char *bytes)
{
	cterm_write(cterm, bytes, strlen(bytes), NULL);
}

static void
test_reinit(void)
{
	static const char *commands[] = {
		"\r\n!|*\r\n",
		"\r\n!|w00001B0M10\r\n",
		"\r\n!|w00001B0M11\r\n",
		"\r\n!|w00001B0M12\r\n",
		"\r\n!|w00001B0M13\r\n",
		"\r\n!|w00001B0M14\r\n",
	};
	for (int version = RIP_VERSION_1; version <= RIP_VERSION_3; version++) {
		for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
			for (int muted = 0; muted <= 1; muted++) {
				reset(version);
				cterm->keystroke_cb = capture_output;
				cterm->keystroke_cbdata = &keys;
				cterm->response_cb = muted ? NULL : capture_output;
				cterm->response_cbdata = &replies;
				cterm->key_event_mode_change = capture_mode;
				/* Test both the session's self pointer and opaque data. */
				cterm->key_event_mode_change_cbdata = muted ? (void *)&keys : cterm;
				cterm->ext_state_7_cb = capture_audio;
				cterm->size_change_cb = capture_size;
				cterm->size_change_cbdata = &size_changes;
				assert(cterm_handle_key(cterm, 'a') == CTERM_KEY_HANDLED);
				/* Repeat to exercise replacing an already replaced terminal. */
				for (int repeat = 0; repeat < 2; repeat++) {
					feed(commands[i], strlen(commands[i]));
					assert(cterm_handle_key(cterm, 'b') == CTERM_KEY_HANDLED);
					/* RIP buttons and mouse regions queue host strings here. */
					handle_command_str("GO");
					while (ripbuf)
						assert(cterm_handle_key(cterm, rip_getch()) == CTERM_KEY_HANDLED);
					assert(keys.length == 1 + (repeat + 1) * 3);
					assert(memcmp(keys.bytes, "abGObGO", keys.length) == 0);
					write_terminal("\x1b[6n");
					assert(muted ? replies.length == 0 : replies.length > 0);
					mode_data = muted ? (void *)&keys : cterm;
					write_terminal("\x1b[=1h");
					assert(mode_changes == repeat + 1);
					write_terminal("\x1b[=7n");
					assert(audio_queries == repeat + 1);
					cterm_resize_rows(cterm, cterm->height - 1);
					assert(size_changes == repeat + 1);
				}
			}
		}
	}
}

static void
test_introducers(void)
{
	static const struct {
		const char *wire;
		int color;
	} cases[] = {
		{"abc !|c0F\r\n", 1},
		{"!xabc !|c0F\r\n", 1},
		{"!xabc !ydef !|c0F\r\n", 1},
		{"!x\x1b[10;1H!|c0F\r\n", 15},
		{"!x\r\n!|c0F\r\n", 15},
		{"\x1b[10;1H!|c0F\r\n", 15},
		{"\r\n\x1b[0m!|c0F\r\n", 15},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
		for (size_t split = 0; split <= strlen(cases[i].wire); split++) {
			reset(RIP_VERSION_1);
			feed(cases[i].wire, split);
			feed(cases[i].wire + split, strlen(cases[i].wire) - split);
			if (rip.color != cases[i].color)
				fprintf(stderr, "Introducer case %zu, split %zu: color %d != %d\n",
				    i, split, rip.color, cases[i].color);
			assert(rip.color == cases[i].color);
		}
	}
	/* Releasing a lone introducer from the preceding packet adds a byte
	 * to passthrough. It must not overwrite the following packet's text. */
	for (int intro = 0; intro < 3; intro++) {
		static const char introducers[] = {'!', 0x01, 0x02};
		struct vmem_cell cells[3];
		reset(RIP_VERSION_1);
		feed(&introducers[intro], 1);
		feed("XYZ", 3);
		/* ANSI-BBS renders SOH/STX as CP437 glyphs, like the literal '!'. */
		assert(vmem_gettext(2, 1, 4, 1, cells));
		assert(cells[0].ch == 'X' && cells[1].ch == 'Y' && cells[2].ch == 'Z');
	}
}

/* Literal expectations from RIPTERM.EXE 1A04:007E-0186 and 1634:0272-02FB.
 * Check every two-part split and bytewise: TCP packet boundaries must not
 * change command output, ordinary text, or the five raw history bytes. */
static void
check_wire_reply(const char *name, int version, const char *wire,
    const char *expected_host, const char *expected_text, int color, const char *expected_reply)
{
	size_t length = strlen(wire);
	for (size_t split = 0; split <= length + 1; split++) {
		struct output host = {0};
		uint8_t history[5] = {0};
		reset(version);
		if (split <= length) {
			feed(wire, split);
			feed(wire + split, length - split);
		}
		else {
			for (size_t i = 0; i < length; i++)
				feed(wire + i, 1);
		}
		while (ripbuf) {
			char c = rip_getch();
			capture_output(&c, 1, &host);
		}
		if (host.length != strlen(expected_host)
		    || memcmp(host.bytes, expected_host, host.length) != 0
		    || wire_text.length != strlen(expected_text)
		    || memcmp(wire_text.bytes, expected_text, wire_text.length) != 0
		    || wire_replies.length != strlen(expected_reply)
		    || memcmp(wire_replies.bytes, expected_reply, wire_replies.length) != 0
		    || rip.color != color) {
			fprintf(stderr, "%s (RIP %d, split %zu): host=%zu bytes, text=%zu bytes, color=%d\n",
			    name, version, split, host.length, wire_text.length, rip.color);
			assert(false);
		}
		for (size_t i = 0; i < sizeof(history) && i < length; i++)
			history[i] = (uint8_t)wire[length - 1 - i];
		assert(memcmp(cb.window, history, sizeof(history)) == 0);
	}
}

static void
check_wire(const char *name, int version, const char *wire,
    const char *expected_host, const char *expected_text, int color)
{
	check_wire_reply(name, version, wire, expected_host, expected_text, color, "");
}

static void
test_backslashes(void)
{
	/* RIP 1.54 examines two bytes before pipe and three before CR, rather
	 * than slash parity. RIP v3 retains SyncTERM's conventional escaping. */
	static const char *pipe_v1[] = {
		"Anext", "A|1\x1b" "0000next", "A\\next", "A\\next",
		"A\\\\next", "A\\\\next", "A\\\\\\next",
	};
	static const char *pipe_v3[] = {
		"Anext", "A|1\x1b" "0000next", "A\\next", "A\\|1\x1b" "0000next",
		"A\\\\next", "A\\\\|1\x1b" "0000next", "A\\\\\\next",
	};
	static const char *cr_v1[] = {"A", "AB", "A\\", "A\\B", "A\\B", "A\\\\B", "A\\\\B"};
	static const char *cr_v3[] = {"A", "AB", "A\\", "A\\B", "A\\\\", "A\\\\B", "A\\\\\\"};
	for (int version = RIP_VERSION_1; version <= RIP_VERSION_3; version++) {
		for (size_t n = 0; n < sizeof(pipe_v1) / sizeof(*pipe_v1); n++) {
			char wire[80] = "!|1\x1b" "0000A";
			size_t prefix = strlen(wire);
			memset(wire + prefix, '\\', n);
			strlcpy(wire + prefix + n, "|1\x1b" "0000next\r\nZ", sizeof(wire) - prefix - n);
			check_wire("pipe backslashes", version, wire,
			    version == RIP_VERSION_1 ? pipe_v1[n] : pipe_v3[n], "Z", 1);
			strlcpy(wire + prefix + n, "\r\nB\r\nZ", sizeof(wire) - prefix - n);
			check_wire("CR backslashes", version, wire,
			    version == RIP_VERSION_1 ? cr_v1[n] : cr_v3[n],
			    n == 0 || n == 2 || (version == RIP_VERSION_3 && n % 2 == 0)
			    ? "B\r\nZ" : "Z", 1);
		}
	}
}

static void
test_history(void)
{
	static const struct {
		const char *name, *wire, *text;
		int color;
	} cases[] = {
		{"completion ESC", "!|#\r\x1b[31mZ", "[31mZ", 1},
		{"completion printable", "!|#\rXYZ", "YZ", 1},
		{"completion LF", "!|#\r\nZ", "Z", 1},
		{"completion VT", "!|#\r\vZ", "\x1b[BZ", 1},
		{"bare pipe bang", "!|#|!XYZ\r\n", "!XYZ\r\n", 1},
		{"bare pipe SOH", "!|#|\x01XYZ\r\n", "\x01XYZ\r\n", 1},
		{"bare pipe STX", "!|#|\x02XYZ\r\n", "\x02XYZ\r\n", 1},
		{"restart bang", "!|#|!|c0F\r\n", "", 15},
		{"restart SOH", "!|#|\x01|c0F\r\n", "", 15},
		{"restart STX", "!|#|\x02|c0F\r\n", "", 15},
		{"consumed CR", "ABC\x01|#\r!|c0F\r\n", "ABC", 15},
		{"cursor after CR", "\r\x1b[1;10H!|c0F\r\n", "\r\x1b[1;10H!|c0F\r\n", 1},
		{"slash before CSI", "ABC\\\x1b[0m\x01|c0F\r\n", "ABC\\\x1b[0m", 15},
		{"slash CSI final", "ABC\x1b[\\\x01|c0F\r\n", "ABC\x1b[\\\x01|c0F\r\n", 1},
	};
	for (int version = RIP_VERSION_1; version <= RIP_VERSION_3; version++) {
		for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++)
			check_wire(cases[i].name, version, cases[i].wire, "", cases[i].text, cases[i].color);
	}
}

static void
test_autosense(void)
{
	/* 09C6:064A-06AD uses numeric parameter count/value, not spelling. */
	static const struct {
		const char *wire;
		bool query, enabled;
	} cases[] = {
		{"\x1b[!", true, true},
		{"\x1b[0!", true, true},
		{"\x1b[00!", true, true},
		{"\x1b[0000000000000000!", true, true},
		{"\x1b[1!", false, false},
		{"\x1b[01!", false, false},
		{"\x1b[0001!", false, false},
		{"\x1b[2!", false, true},
		{"\x1b[02!", false, true},
		{"\x1b[0002!", false, true},
		{"\x1b[3!", false, true},
		{"\x1b[00;0!", false, true},
		{"\x1b[0;00!", false, true},
	};
	for (int version = RIP_VERSION_1; version <= RIP_VERSION_3; version++) {
		for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
			size_t length = strlen(cases[i].wire);
			for (size_t split = 0; split <= length + 1; split++) {
				const char *expected = version == RIP_VERSION_1
				    ? "RIPSCRIP015410" : "RIPSCRIP030001";
				reset(version);
				if (split <= length) {
					feed(cases[i].wire, split);
					feed(cases[i].wire + split, length - split);
				}
				else {
					for (size_t j = 0; j < length; j++)
						feed(cases[i].wire + j, 1);
				}
				assert(wire_replies.length == (cases[i].query ? 14 : 0));
				if (cases[i].query) {
					assert(memcmp(wire_replies.bytes, expected, 14) == 0);
					assert(!skypix && !cterm->skypix);
				}
				assert(rip.enabled == cases[i].enabled);
				/* CSI handling must neither leak bytes nor eat the text
				 * immediately following its final '!'. */
				assert(wire_text.length == 0);
				feed("HELLO", 5);
				assert(wire_text.length == 5);
				assert(memcmp(wire_text.bytes, "HELLO", 5) == 0);
				if (!cases[i].enabled) {
					/* Disable remains one-way, including a padded request. */
					feed("\x1b[2!", 4);
					assert(!rip.enabled && wire_replies.length == 0);
				}
			}
		}
	}
}

static void
test_block_alias(void)
{
	/* Descriptor entries 59/60 share 1814:197D's block-mode handler.
	 * SYNCVIEW compiles out transfers and cache access, but retains command
	 * recognition and argument validation. Dispatch reports recognition. */
	static const char *commands[] = {
		"!|9\x1b" "01010000asset.icn<>",
		"!|0\x1b" "01010000asset.icn<>",
		"\x01|0\x1b" "01010000asset.icn<>",
		"!|#|0\x1b" "01010000asset.icn<>",
	};
	for (int version = RIP_VERSION_1; version <= RIP_VERSION_3; version++) {
		for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
			size_t length = strlen(commands[i]);
			for (size_t split = 0; split <= length + 1; split++) {
				reset(version);
				if (split <= length) {
					feed(commands[i], split);
					feed(commands[i] + split, length - split);
				}
				else {
					for (size_t j = 0; j < length; j++)
						feed(commands[i] + j, 1);
				}
				assert(cb.len > 0 && wire_text.length == 0);
				assert(cb_dispatch());
				assert(cb.len == 0 && cb.ebc);
				feed("\nZ", 2);
				assert(wire_text.length == 1 && wire_text.bytes[0] == 'Z');
				assert(wire_replies.length == 0);
			}
		}
	}
}

static void
test_interleaving(void)
{
	/* 1A04:01C6 branches to 01FC, where SOH/STX still start RIP.
	 * ANSI retains its prefix while the intervening RIP bytes are consumed. */
	static const struct {
		const char *name, *wire, *host, *text;
		int color;
	} cases[] = {
		{"SOH during CSI", "\x1b[31\x01|1\x1b" "0000OK\r\nmZ", "OK", "\x1b[31mZ", 1},
		{"STX during CSI", "\x1b[31\x02|1\x1b" "0000OK\r\nmZ", "OK", "\x1b[31mZ", 1},
		{"SOH after ESC", "\x1b\x01|1\x1b" "0000OK\r\n[31mZ", "OK", "\x1b[31mZ", 1},
		{"completion before CSI", "\x1b[3\x01|#\rX1mZ", "", "\x1b[31mZ", 1},
		{"two interruptions", "\x1b[3\x01|1\x1b" "0000A\r\n1\x02|1\x1b" "0000B\r\nmZ", "AB", "\x1b[31mZ", 1},
		{"RIP argument ESC", "\x1b[31\x01|1\x1b" "0000\x1b[!\r\nmZ", "\x1b[!", "\x1b[31mZ", 1},
		{"bogus SOH", "\x1b[31\x01XYZ", "", "\x1b[31\x01XYZ", 1},
		{"bogus STX", "\x1b[31\x02XYZ", "", "\x1b[31\x02XYZ", 1},
		{"escaped SOH", "\x1b[\\\x01|c0F\r\n", "", "\x1b[\\\x01|c0F\r\n", 1},
		{"escaped STX", "\x1b[\\\x02|c0F\r\n", "", "\x1b[\\\x02|c0F\r\n", 1},
		{"bang inside escape", "\x1b!|c0F\r\n", "", "\x1b!|c0F\r\n", 1},
		{"graphics after text", "A\x1b[31\x01|c0F|X2S2S\r\nmZ", "", "A\x1b[31mZ", 15},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
		check_wire(cases[i].name, RIP_VERSION_1, cases[i].wire,
		    cases[i].host, cases[i].text, cases[i].color);
		assert(cb.esc_len == 0 && cb.len == 0);
	}
	check_wire_reply("resumed autosense", RIP_VERSION_1, "\x1b[00\x01|#\r\n!Z",
	    "", "Z", 1, "RIPSCRIP015410");
	check_wire_reply("new escape after bogus SOH", RIP_VERSION_1, "\x1b[31\x01\x1b[00!Z",
	    "", "\x1b[31\x01Z", 1, "RIPSCRIP015410");
	check_wire("SkyPix string payload", RIP_VERSION_1, "\x1b[16;99!a\x01|c0F\r\n!Z",
	    "", "Z", 1);
	/* Graphics must follow the preceding text and precede the resumed SGR. */
	const char *wire = cases[sizeof(cases) / sizeof(*cases) - 1].wire;
	size_t length = strlen(wire);
	for (size_t split = 0; split <= length + 1; split++) {
		struct vmem_cell cells[2];
		struct ciolib_pixels *p;
		reset(RIP_VERSION_1);
		/* Complete the fixture's mode-change redraw before painting pixels. */
		freepixels(getpixels(0, 0, 0, 0, false));
		if (split <= length) {
			feed(wire, split);
			feed(wire + split, length - split);
		}
		else {
			for (size_t j = 0; j < length; j++)
				feed(wire + j, 1);
		}
		p = getpixels(100, 100, 100, 100, false);
		if (p && (p->pixels[0] != map_rip_color(15) || p->pixelsb[0] != map_rip_color(15)))
			fprintf(stderr, "Interleaved rendering split %zu: primary=%08x alternate=%08x expected=%08x\n",
			    split, p->pixels[0], p->pixelsb[0], map_rip_color(15));
		assert(p && p->pixels[0] == map_rip_color(15) && p->pixelsb[0] == map_rip_color(15));
		freepixels(p);
		assert(vmem_gettext(1, 1, 2, 1, cells));
		assert(cells[0].ch == 'A' && (cells[0].legacy_attr & 7) == LIGHTGRAY);
		assert(cells[1].ch == 'Z' && (cells[1].legacy_attr & 7) == RED);
	}
	/* RIPv3 keeps its existing behavior: SOH ends escape lookahead and
	 * the following pipe does not begin a RIP command. */
	check_wire("v3 escape priority", RIP_VERSION_3, "\x1b[31\x01|c0F\r\n",
	    "", "\x1b[31\x01|c0F\r\n", 1);
}

static void
test_interleaving_capacity(void)
{
	/* A saved prefix plus a bogus introducer can expand passthrough even
	 * when the next input fills the whole buffer. Check deferred drains
	 * too, including VT's extra two output bytes. */
	for (size_t capacity = RIP_ESC_MAX + 1; capacity <= 64; capacity += 31) {
		for (int intro = 1; intro <= 2; intro++) {
			for (int vt = 0; vt <= 1; vt++) {
				uint8_t guarded[66];
				uint8_t prefix[RIP_ESC_MAX], expected[100];
				size_t length = capacity, expected_length = sizeof(prefix);
				unsigned calls = 0;
				reset(RIP_VERSION_1);
				prefix[0] = 0x1b;
				prefix[1] = '[';
				memset(prefix + 2, '0', sizeof(prefix) - 3);
				prefix[sizeof(prefix) - 1] = intro;
				feed(prefix, sizeof(prefix));
				assert(cb.esc_len == RIP_ESC_MAX - 1 && cb.len == 1);
				memcpy(expected, prefix, sizeof(prefix));
				if (vt) {
					memcpy(expected + expected_length, "\x1b[B", 3);
					expected_length += 3;
				}
				memset(expected + expected_length, 'X', capacity - vt);
				expected_length += capacity - vt;
				memset(guarded, 0xa5, sizeof(guarded));
				memset(guarded + 1, 'X', capacity);
				if (vt)
					guarded[1] = 0x0b;
				do {
					size_t n = parse_rip(guarded + 1, length, capacity);
					assert(n <= capacity && guarded[0] == 0xa5 && guarded[capacity + 1] == 0xa5);
					capture_output((const char *)guarded + 1, n, &wire_text);
					if (n)
						cterm_write(cterm, guarded + 1, n, NULL);
					length = 0;
					assert(++calls < 100);
				} while (cb.defer_len);
				assert(wire_text.length == expected_length);
				assert(memcmp(wire_text.bytes, expected, expected_length) == 0);
				assert(cb.len == 0 && cb.esc_len == 0);
				for (size_t i = 0; i < sizeof(cb.window); i++)
					assert(cb.window[i] == 'X');
			}
		}
	}
}

/* Literal values below come from RIP154_PARSER.adoc, rather than from the
 * decoder under test. Exercise the same words through descriptors and wire. */
static void
test_words(void)
{
	static const struct { const char *bytes; int width, word, consumed; } cases[] = {
		{"-A", 2, 65526, 2}, {"A-", 2, 10, 2}, {"0-", 2, 0, 2},
		{"-001", 4, 65535, 4}, {"1EKF", 4, 65535, 4}, {"1EKG", 4, 0, 4},
		{"1eKf", 4, 65535, 4}, {"-", 1, 0, 1}, {"-", 2, 0, 1},
		{"A-5", 3, 10, 3}, {"--A", 3, 0, 3}, {"F", 2, 15, 1},
		{"F|", 2, 15, 1}, {"ZZZZZ", 5, 41983, 5},
	};
	static const char *bad[] = {"", "|", "?", "A?", "A-?", "\x80", "A\xff"};
	int parsed[1025], text_start, consumed;
	bool has_text;
	char many[1201];
	reset(RIP_VERSION_1);
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
		int word = parse_mega(cases[i].bytes, cases[i].width, &consumed);
		assert(word == cases[i].word && consumed == cases[i].consumed);
	}
	for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
		assert(parse_mega(bad[i], 3, &consumed) == -1);
		assert(consumed == 0);
	}
	assert(rip_signed_word(0x7fff) == 32767);
	assert(rip_signed_word(0x8000) == -32768);
	assert(rip_signed_word(0xffff) == -1);
	assert(parse_rip_args("0F", "2#2#", parsed, 1025, &text_start, &has_text) == -1);
	assert(parse_rip_args("0FF", "2#2#", parsed, 1025, &text_start, &has_text) == 2);
	assert(parsed[0] == 15 && parsed[1] == 15 && text_start == 3);
	assert(parse_rip_args("-001", "4#", parsed, 1025, &text_start, &has_text) == 1);
	assert(parsed[0] == 65535);
	/* Arbitrary suffixes of fixed descriptors are not additional fields. */
	assert(parse_rip_args("0F?", "2#", parsed, 1025, &text_start, &has_text) == 1);
	assert(text_start == 2);
	memset(many, '0', 1200);
	many[1200] = 0;
	parsed[511] = 0x12345678;
	assert(parse_rip_args(many, "+2#", parsed, 1025, &text_start, &has_text) == 511);
	assert(text_start == 1200 && parsed[511] == 0x12345678);
	/* Scan and validate even fields which cannot be retained. */
	many[1199] = '?';
	assert(parse_rip_args(many, "+2#", parsed, 1025, &text_start, &has_text) == -1);
	many[1199] = '0';
	reset(RIP_VERSION_3);
	assert(parse_mega("-A", 2, &consumed) == 10);
	assert(parse_mega("A-", 2, &consumed) == 360);
	assert(parse_mega("1EKG", 4, &consumed) == 65536);
	assert(parse_rip_args(many, "+2#", parsed, 1025, &text_start, &has_text) == 600);
}

static void
feed_split(const char *wire, size_t split)
{
	size_t length = strlen(wire);
	if (split <= length) {
		feed(wire, split);
		feed(wire + split, length - split);
	}
	else {
		for (size_t i = 0; i < length; i++)
			feed(wire + i, 1);
	}
}

static void
assert_pixel(int x, int y, int color)
{
	struct ciolib_pixels *p = getpixels(x, y, x, y, false);
	assert(p && p->pixels[0] == map_rip_color(color) && p->pixelsb[0] == map_rip_color(color));
	freepixels(p);
}

static void
test_word_consumers(void)
{
	static const char *patterns[] = {"-001", "1EKF", "1eKf", "1EKG"};
	static const char *colors[] = {"-1", "-A", "A-", "0-", "F"};
	static const int expected[] = {15, 6, 10, 0, 15};
	char wire[1200];
	for (size_t i = 0; i < sizeof(patterns) / sizeof(*patterns); i++) {
		snprintf(wire, sizeof(wire), "!|=04%s01|m-A0-\r\n", patterns[i]);
		for (size_t split = 0; split <= strlen(wire) + 1; split++) {
			reset(RIP_VERSION_1);
			feed_split(wire, split);
			assert(rip.line_style == 4 && rip.line_width == 1);
			assert(rip.line_pattern == (i == 3 ? 0 : 0xffff));
			assert(rip.x == -10 && rip.y == 0);
			assert(wire_text.length == 0);
		}
	}
	for (size_t i = 0; i < sizeof(colors) / sizeof(*colors); i++) {
		snprintf(wire, sizeof(wire), "!|c%s\r\n", colors[i]);
		check_wire("word colour", RIP_VERSION_1, wire, "", "", expected[i]);
	}
	reset(RIP_VERSION_1);
	assert(do_rip_command(0, 0, '=', "04-00101"));
	assert(do_rip_command(0, 0, '=', "-1000001"));
	assert(rip.requested.line[0] == 65535 && rip.line_style == 4 && rip.line_pattern == 65535);
	assert(do_rip_command(0, 0, '=', "000000-1"));
	assert(rip.requested.line[2] == 65535 && rip.line_style == 4 && rip.line_width == 1);
	assert(do_rip_command(0, 0, 'S', "01-1"));
	assert(rip.fill_style == 1 && rip.fill_color == 15);
	assert(do_rip_command(0, 0, 'S', "-101"));
	assert(rip.requested.fill[0] == 65535 && rip.fill_style == 1 && rip.fill_color == 15);
	assert(do_rip_command(0, 0, 's', "-1-A00FF000000000F"));
	assert(rip.fill_pattern[0] == 255 && rip.fill_pattern[1] == 246 && rip.fill_pattern[3] == 43);
	assert(!do_rip_command(0, 0, 's', "0000000000000000-1"));
	assert(rip.requested.fill[8] == 65535 && rip.fill_pattern[0] == 255 && rip.fill_color == 15);
	assert(!do_rip_command(0, 0, 'a', "-10F"));
	assert(do_rip_command(0, 0, 'a', "00-1"));
	assert(rip.requested.palette[0] == 65535 && curr_ega_palette[0] == 255);
	assert(do_rip_command(0, 0, 'a', "0000"));
	/* Invalid font falls back; unsupported metric sizes remain bounded. */
	assert(do_rip_command(0, 0, 'Y', "01000300"));
	assert(do_rip_command(0, 0, 'Y', "-1000300"));
	assert(rip.requested.font[0] == 65535 && rip.font.num == 0 && rip.font.size == 1);
	assert(do_rip_command(0, 0, 'Y', "0000-100"));
	assert(rip.requested.font[2] == 65535 && rip.font.size == 1);
	assert(!do_rip_command(1, 0, 'P', "0000-10"));
	assert(!do_rip_command(1, 0, 'I', "0000-1000file"));
	/* Reserved numeric fields are still required and validated. */
	assert(!do_rip_command(0, 0, 'Y', "000001?0"));
	memset(wire, '0', 35);
	wire[35] = '?';
	wire[36] = 0;
	assert(!do_rip_command(1, 0, 'B', wire));
	assert(!do_rip_command(0, 0, 'm', "-1"));
	assert(rip.x == 0 && rip.y == 0);

	/* Signed endpoints are clipped at the viewport; a negative flood seed
	 * must not reach bitmap indexing. Check every split and bytewise input. */
	snprintf(wire, sizeof(wire), "!|c-1|S01-1|L-10A0A0A|B-10K020M|F-1000F\r\n");
	for (size_t split = 0; split <= strlen(wire) + 1; split++) {
		reset(RIP_VERSION_1);
		freepixels(getpixels(0, 0, 0, 0, false));
		feed_split(wire, split);
		assert_pixel(0, 10, 15);
		assert_pixel(10, 10, 15);
		assert_pixel(11, 10, 0);
		assert_pixel(0, 20, 15);
		assert_pixel(2, 22, 15);
		assert_pixel(3, 22, 0);
	}
	reset(RIP_VERSION_1);
	assert(do_rip_command(0, 0, 'v', "0K0U0A0A"));
	assert(rip.viewport.sx == 10 && rip.viewport.sy == 10 && rip.viewport.ex == 20 && rip.viewport.ey == 30);
	assert(do_rip_command(0, 0, 'v', "-1000A0A"));
	assert(rip.requested.viewport[0] == 65535 && rip.viewport.sx == 10);
	assert(do_rip_command(0, 0, 'v', "05050505"));
	assert(rip.viewport.sx == 5 && rip.viewport.sy == 5 && rip.viewport.ex == 5 && rip.viewport.ey == 5);
	assert(do_rip_command(0, 0, 'v', "JZZZJZZZ"));
	assert(rip.viewport.sx == 639 && rip.viewport.sy == 349 && rip.viewport.ex == 639 && rip.viewport.ey == 349);
	reset(RIP_VERSION_1);
	assert(do_rip_command(0, 0, 'w', "-1-1-1-190"));
	assert(rip.requested.text_window[2] == 65535);
	assert(cterm->left_margin == 1 && cterm->top_margin == 1);
	assert(cterm->right_margin == 2 && cterm->bottom_margin == 2);
	assert(cterm->extattr & CTERM_EXTATTR_AUTOWRAP);
	assert(do_rip_command(0, 0, 'g', "-A-1"));
	assert(rip.requested.cursor[0] == 65526 && rip.requested.cursor[1] == 65535);
	reset(RIP_VERSION_1);
	assert(rip_circle_y_radius(65526) == 65529); /* -10 * .775 -> -7 */
	assert(bgi_trig_mul(65536, 65535) == -1);
	/* Large unsigned radii must use bounded spans and wide intermediates. */
	assert(do_rip_command(0, 0, 'o', "0000-1-1"));
	assert(do_rip_command(0, 0, 'C', "0000-A"));
	assert(do_rip_command(0, 0, 'I', "0000-A0A01"));
	assert(do_rip_command(0, 0, 'Z', "-100000000000000-1"));
	assert(rip.x == 0 && rip.y == 0);
	/* A missing vertex cannot be replaced by stale stack contents. */
	assert(!do_rip_command(0, 0, 'P', "ZZ00000000"));
	assert(!do_rip_command(0, 0, 'p', "0000000000"));
	assert(!do_rip_command(0, 0, 'l', "0300000000"));
	assert(do_rip_command(0, 0, 'l', "02-1000100"));
	/* 511 retained words hold 255 pairs. A larger declared count must
	 * not make the renderer read the scanned-but-unretained tail. */
	memset(wire, '0', 1100);
	memcpy(wire, "73", 2);
	wire[1100] = 0;
	assert(do_rip_command(0, 0, 'P', wire));
	wire[1] = '4';
	assert(!do_rip_command(0, 0, 'P', wire));
	wire[1099] = '?';
	assert(!do_rip_command(0, 0, 'P', wire));
	check_wire("bad numeric recovery", RIP_VERSION_1, "!|cA?|c-1\r\n", "", "", 15);
	for (size_t len = 0; len < 12; len++) {
		memset(wire, '0', len);
		wire[len] = 0;
		rip_poly_bezier(wire, false, false);
	}
}

static void
test_word_buttons_and_text(void)
{
	char args[1200];
	int parsed[20], offset;
	bool text;
	/* Seventeen fields, with a full word of flags and three reserved words. */
	const char *style = "-1" "-1" "02" "-001" "00" "-1" "-A" "0F" "00"
	    "0A" "-Z" "02" "0F" "0F" "00" "00" "00";
	reset(RIP_VERSION_1);
	assert(parse_rip_args(style, find_rip_desc(1, 'B'), parsed, 20, &offset, &text) == 17);
	assert(offset == 36);
	assert(do_rip_command(1, 0, 'B', style));
	assert(rip.bstyle.width == -1 && rip.bstyle.height == -1);
	assert(rip.bstyle.flags.sunken && rip.bstyle.group == 65501);
	assert(rip.bstyle.cfore == 15 && rip.bstyle.cdshadow == 6);
	/* Use a plain mouse button for deterministic field inspection. */
	rip.bstyle.button = BUTTON_TYPE_PLAIN;
	rip.bstyle.flags.bevel = false;
	rip.bstyle.flags.dropshadow = false;
	rip.bstyle.flags.autostamp = false;
	rip.bstyle.flags.mouse = true;
	assert(do_rip_command(1, 0, 'U', "0A0A0K0K-120<>Label<>GO"));
	assert(rip.mfields && rip.mfields->type == MOUSE_FIELD_BUTTON);
	assert(!rip.mfields->data.button->selected);
	assert(rip.mfields->data.button->box.x2 == 20);
	assert(rip.mfields->data.button->hotkey == 255);
	assert(!rip.mfields->data.button->highlight_hotkey);
	assert(!do_rip_command(1, 0, 'M', "-100000A0A0000000GO"));
	assert(do_rip_command(1, 0, 'M', "0000000A0A9900000GO"));
	assert(rip.mfields->type == MOUSE_FIELD_HOT);
	assert(rip.mfields->data.hot->invertable && rip.mfields->data.hot->resetafter);
	assert(strcmp(rip.mfields->data.hot->command, "GO") == 0);
	for (int i = 0; i < 130; i++)
		assert(do_rip_command(1, 0, 'M', "0000000A0A0000000GO"));
	unsigned fields = 0;
	for (struct mouse_field *field = rip.mfields; field; field = field->next)
		fields++;
	assert(fields == 128);
	/* Freeform text retains at most 1023 bytes; empty text is a space. */
	reset(RIP_VERSION_1);
	memcpy(args, "0000", 4);
	memset(args + 4, 'X', 1100);
	args[1104] = 0;
	assert(do_rip_command(1, 0, '\x1b', args));
	for (int i = 0; i < 1023; i++)
		assert(rip_getch() == 'X');
	assert(ripbuf == NULL);
	assert(do_rip_command(1, 0, '\x1b', "0000"));
	assert(rip_getch() == ' ' && ripbuf == NULL);
	/* Confirm the text cap in stream assembly, including bytewise input. */
	memcpy(args, "!|1\x1b" "0000", 8);
	memset(args + 8, 'X', 1100);
	memcpy(args + 1108, "\r\n", 3);
	static const size_t splits[] = {0, 1, 8, 512, 1031, 1108, 1110, 1111};
	for (size_t split = 0; split < sizeof(splits) / sizeof(*splits); split++) {
		reset(RIP_VERSION_1);
		feed_split(args, splits[split]);
		for (int i = 0; i < 1023; i++)
			assert(rip_getch() == 'X');
		assert(ripbuf == NULL && wire_text.length == 0);
	}
}

struct copy_expect {
	int x, y, width, height, dest;
};

static struct ciolib_pixels *
copy_fixture(int version)
{
	struct ciolib_pixels *pixels;
	reset(version);
	/* Finish the mode-change redraw before setting an independent image. */
	freepixels(getpixels(0, 0, 0, 0, false));
	pixels = getpixels(0, 0, 639, 349, false);
	assert(pixels && pixels->width == 640 && pixels->height == 350);
	for (unsigned y = 0; y < pixels->height; y++) {
		for (unsigned x = 0; x < pixels->width; x++) {
			size_t i = y * pixels->width + x;
			/* Unique RGB pixels identify the original row and column.
			 * Distinct blink buffers catch a one-buffer-only copy. */
			pixels->pixels[i] = 0x80000000u | ((y + 1) << 11) | x;
			pixels->pixelsb[i] = pixels->pixels[i] | 0x00800000u;
		}
	}
	assert(setpixels(0, 0, 639, 349, 0, 0, 0, 0, pixels, NULL));
	freepixels(pixels);
	return getpixels(0, 0, 639, 349, false);
}

static void
check_copy_image(const char *name, const struct ciolib_pixels *before,
    struct copy_expect expected)
{
	struct ciolib_pixels *after = getpixels(0, 0, 639, 349, false);
	assert(before && after);
	for (int y = 0; y < 350; y++) {
		for (int x = 0; x < 640; x++) {
			size_t dest = y * 640 + x;
			size_t source = dest;
			if (x >= expected.x && x < expected.x + expected.width
			    && y >= expected.dest && y < expected.dest + expected.height)
				source = (expected.y + y - expected.dest) * 640 + x;
			if (after->pixels[dest] != before->pixels[source]
			    || after->pixelsb[dest] != before->pixelsb[source]) {
				fprintf(stderr, "Copy %s at %d,%d: primary=%08x/%08x alternate=%08x/%08x\n",
				    name, x, y, after->pixels[dest], before->pixels[source],
				    after->pixelsb[dest], before->pixelsb[source]);
				assert(false);
			}
		}
	}
	freepixels(after);
}

static void
test_copy_region(void)
{
	/* Literal rectangles are derived from the disassembly at 0c16:0623,
	 * and the documented outward rounding for v3. Zero rectangles mean
	 * no pixels change. The source image must survive overlapping copies. */
	static const struct {
		const char *name, *args;
		struct copy_expect v1, v3;
		bool v1_only;
	} cases[] = {
		{"up", "080A0O0C0002", {8, 10, 16, 3, 2}, {8, 10, 16, 3, 2}},
		{"down overlap", "080A0O0D000B", {8, 10, 16, 4, 11}, {8, 10, 16, 4, 11}},
		{"up overlap", "080A0O0D0009", {8, 10, 16, 4, 9}, {8, 10, 16, 4, 9}},
		{"reversed", "0O0D080A0002", {8, 10, 16, 4, 2}, {8, 10, 16, 4, 2}},
		{"same line", "080A0O0C000A", {0}, {0}},
		{"one row", "080A0O0A0002", {0}, {8, 10, 16, 1, 2}},
		{"unaligned", "0E0A120C0002", {8, 10, 24, 3, 2}, {8, 10, 32, 3, 2}},
		{"reserved positive", "0E0A120C0702", {0, 10, 32, 3, 2}, {8, 10, 32, 3, 2}},
		{"reserved negative", "0E0A120C-702", {16, 10, 16, 3, 2}, {0}, true},
		{"negative left", "-A0A0O0C0002", {0, 10, 24, 3, 2}, {0}, true},
		{"negative source row", "08-10O02000A", {8, 0, 16, 3, 10}, {0}, true},
		{"negative destination", "080A0O0C00-1", {8, 10, 16, 3, 0}, {0}, true},
		{"pre-clamp height guard", "089G0OB4008C", {0}, {8, 340, 16, 10, 300}},
		{"source bottom clamp", "089G0OB40000", {8, 340, 16, 10, 0}, {8, 340, 16, 10, 0}},
		{"source right clamp", "HK0AJG0C0002", {0}, {632, 10, 8, 3, 2}},
		{"large copy", "0000HR9O0001", {0, 0, 632, 349, 1}, {0, 0, 640, 349, 1}},
		{"full width", "0000HR9O0101", {0, 0, 640, 349, 1}, {0, 0, 640, 349, 1}},
		{"loose bottom guard", "080A0O0B009P", {8, 10, 16, 1, 349}, {0}},
		{"destination overflow", "080A0O0D009O", {0}, {0}},
		{"negative source rows", "08-20O-10002", {8, 0, 16, 1, 2}, {0}, true},
		{"missing field", "080A0O0C00", {0}, {0}},
		{"invalid reserved field", "080A0O0C?002", {0}, {0}},
		{"no whole byte", "080A080C0002", {0}, {0}},
		{"negative x bounds", "-A0A-10C0002", {0}, {0}, true},
		{"resort adjusted x down", "080A090C-70K", {8, 10, 8, 3, 20}, {0}, true},
	};
	char wire[80];
	for (int version = RIP_VERSION_1; version <= RIP_VERSION_3; version++) {
		for (size_t c = 0; c < sizeof(cases) / sizeof(*cases); c++) {
			if (cases[c].v1_only && version != RIP_VERSION_1)
				continue;
			snprintf(wire, sizeof(wire), "!|1G%s\r\n", cases[c].args);
			for (int bytewise = 0; bytewise <= 1; bytewise++) {
				struct ciolib_pixels *before = copy_fixture(version);
				struct ciolib_pixels *clipboard = getpixels(300, 200, 301, 201, false);
				assert(before && clipboard);
				rip.clipboard = clipboard;
				rip.x = 31;
				rip.y = 41;
				rip.xor = true;
				rip.line_pattern = 0;
				rip.line_width = 3;
				/* Alternate a disabled viewport and a remote viewport. */
				rip.viewport.sx = rip.viewport.sy = c & 1 ? 0 : 300;
				rip.viewport.ex = rip.viewport.ey = c & 1 ? 0 : 310;
				feed_split(wire, bytewise ? strlen(wire) + 1 : 0);
				check_copy_image(cases[c].name, before,
				    version == RIP_VERSION_1 ? cases[c].v1 : cases[c].v3);
				assert(rip.clipboard == clipboard);
				assert(rip.x == 31 && rip.y == 41 && rip.xor);
				assert(rip.line_pattern == 0 && rip.line_width == 3);
				assert(rip.viewport.sx == (c & 1 ? 0 : 300));
				assert(rip.viewport.ex == (c & 1 ? 0 : 310));
				assert(wire_text.length == 0 && wire_replies.length == 0);
				freepixels(before);
			}
		}
	}
	/* Packet boundaries may fall anywhere in the numeric descriptor. */
	snprintf(wire, sizeof(wire), "!|1G080A0O0D000B\r\n");
	for (size_t split = 0; split <= strlen(wire); split++) {
		struct ciolib_pixels *before = copy_fixture(RIP_VERSION_1);
		feed_split(wire, split);
		check_copy_image("packet split", before, (struct copy_expect){8, 10, 16, 4, 11});
		freepixels(before);
	}
	/* Test the reduced-display row mapping with independently chosen pixels.
	 * Logical source rows 10,11,12 map to physical 5,6,6; destination
	 * rows 3,4,5 map to 2,2,3, so both destination rows finish with row 6. */
	struct ciolib_pixels *before = copy_fixture(RIP_VERSION_1);
	rip.x_max = 320;
	rip.y_max = 175;
	assert(do_rip_command(1, 0, 'G', "080A0O0C0003"));
	struct ciolib_pixels *after = getpixels(0, 0, 639, 349, false);
	assert(after);
	for (int y = 0; y < 350; y++) {
		for (int x = 0; x < 640; x++) {
			size_t dest = y * 640 + x;
			size_t source = x >= 4 && x <= 12 && y >= 2 && y <= 3 ? 6 * 640 + x : dest;
			assert(after->pixels[dest] == before->pixels[source]);
			assert(after->pixelsb[dest] == before->pixelsb[source]);
		}
	}
	freepixels(after);
	freepixels(before);
}


int
main(int argc, char **argv)
{
	assert(initciolib(CIOLIB_MODE_SDL) == 0);
	if (argc == 1 || strcmp(argv[1], "reinit") == 0)
		test_reinit();
	if (argc == 1 || strcmp(argv[1], "introducers") == 0)
		test_introducers();
	if (argc == 1 || strcmp(argv[1], "backslashes") == 0)
		test_backslashes();
	if (argc == 1 || strcmp(argv[1], "history") == 0)
		test_history();
	if (argc == 1 || strcmp(argv[1], "autosense") == 0)
		test_autosense();
	if (argc == 1 || strcmp(argv[1], "block-alias") == 0)
		test_block_alias();
	if (argc == 1 || strcmp(argv[1], "interleaving") == 0)
		test_interleaving();
	if (argc == 1 || strcmp(argv[1], "capacity") == 0)
		test_interleaving_capacity();
	if (argc == 1 || strcmp(argv[1], "words") == 0)
		test_words();
	if (argc == 1 || strcmp(argv[1], "word-consumers") == 0)
		test_word_consumers();
	if (argc == 1 || strcmp(argv[1], "word-buttons") == 0)
		test_word_buttons_and_text();
	if (argc == 1 || strcmp(argv[1], "copy-region") == 0)
		test_copy_region();
	puts("RIP: callbacks, ordering, escapes, history, autosense, block alias, interleaving, numeric words and screen copy passed");
	return 0;
}
