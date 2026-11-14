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

static struct output keys, replies;
static unsigned mode_changes, audio_queries, size_changes;
static void *mode_data;

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
	(void)bytes;
	(void)timeout;
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
		if (n)
			cterm_write(cterm, buffer, n, NULL);
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

int
main(int argc, char **argv)
{
	assert(initciolib(CIOLIB_MODE_SDL) == 0);
	if (argc == 1 || strcmp(argv[1], "reinit") == 0)
		test_reinit();
	if (argc == 1 || strcmp(argv[1], "introducers") == 0)
		test_introducers();
	puts("RIP: terminal callbacks and introducer stream ordering passed");
	return 0;
}
