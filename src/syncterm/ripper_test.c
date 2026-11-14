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
	puts("RIP: callbacks, ordering, escapes, history, autosense, block alias and interleaving passed");
	return 0;
}
