/* Ticket 299: exercise the production parser and bitmap driver without session
 * I/O. SYNCVIEW omits transfers and cache I/O. Include ripper to inspect font
 * lifetime and to check that tests have drained all deferred input. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define SYNCVIEW
#include "ripper.c"
#include <assert.h>

struct cterminal *cterm;
struct terminal term;
struct syncterm_settings settings;
static size_t sent, passthrough;
void get_term_win_size(int *width, int *height, int *pixelw, int *pixelh, int *nostatus)
{
	if (width) *width = 80;
	if (height) *height = 25;
	if (pixelw) *pixelw = 640;
	if (pixelh) *pixelh = 400;
	if (nostatus) *nostatus = 1;
}

int conn_send(const void *bytes, size_t size, unsigned timeout)
{
	(void)bytes;
	(void)timeout;
	sent += size;
	return size;
}

static void reset(int version)
{
	if (cterm)
		cterm_end(cterm, 0);
	cterm = cterm_init(25, 80, 0, 0, 0, 80, NULL, CTERM_EMULATION_ANSI_BBS);
	assert(cterm);
	init_rip_ver(version);
	textattr(7);
	const char *clear = "\x1b[0m\x1b[2J\x1b[H\x1b[?25l";
	cterm_write(cterm, clear, strlen(clear), NULL);
	sent = passthrough = 0;
}

static void feed(const void *bytes, size_t length)
{
	uint8_t buffer[8192];
	assert(length + RIP_ESC_MAX < sizeof(buffer));
	memcpy(buffer, bytes, length);
	unsigned calls = 0;
	do {
		size_t n = parse_rip(buffer, length, sizeof(buffer));
		passthrough += n;
		if (n)
			cterm_write(cterm, buffer, n, NULL);
		length = 0;
		assert(++calls < 1000);
	} while (cb.defer_len);
}

static void text(const char *bytes) { feed(bytes, strlen(bytes)); }

static struct ciolib_pixels *capture(void)
{
	struct ciolib_pixels *p = getpixels(0, 0, 639, 349, false);
	assert(p);
	for (size_t i = 0; i < (size_t)p->width * p->height; i++) {
		p->pixels[i] &= 0x80ffffff;
		p->pixelsb[i] &= 0x80ffffff;
	}
	return p;
}

static uint32_t pixel(int x, int y)
{
	struct ciolib_pixels *p = getpixels(x, y, x, y, false);
	assert(p);
	uint32_t value = p->pixels[0] & 0x80ffffff;
	assert((p->pixelsb[0] & 0x80ffffff) == value);
	freepixels(p);
	return value;
}

static void test_stream_order(int version)
{
	/* Preceding ordinary text forces the deferred-input path. Colour changes,
	 * two positions and a font reset must all take effect in stream order. */
	const char wire[] = "prefix\x1b[10;15!Times.font!\x1b[15;4!"
	    "\x1b[19;85;95!JoinLink - Messages - Games"
	    "\x1b[31m\x1b[19;75;127!CommodoreNet 64:500/1"
	    "\x1b[10;0!\x1b[HXY\bZ\x1b[?25l";
	reset(version);
	for (size_t i = 0; i < strlen(wire); i++)
		feed(wire + i, 1);
	assert(amiga_font == NULL && cterm->font_render == NULL);
	struct vmem_cell cells[2];
	assert(vmem_gettext(1, 1, 2, 1, cells));
	assert(cells[0].ch == 'X' && cells[1].ch == 'Z');
	struct ciolib_pixels *expected = capture();
	for (size_t split = 0; split <= strlen(wire); split++) {
		reset(version);
		feed(wire, split);
		feed(wire + split, strlen(wire) - split);
		assert(amiga_font == NULL && cterm->font_render == NULL);
		struct ciolib_pixels *actual = capture();
		if (memcmp(actual->pixels, expected->pixels, 640 * 350 * sizeof(uint32_t))) {
			for (size_t p = 0; p < 640 * 350; p++)
				if (actual->pixels[p] != expected->pixels[p]) {
					fprintf(stderr, "version %d split %zu pixel %zu,%zu: %u != %u\n", version, split, p % 640, p / 640, actual->pixels[p], expected->pixels[p]);
					break;
				}
			abort();
		}
		assert(memcmp(actual->pixelsb, expected->pixelsb, 640 * 350 * sizeof(uint32_t)) == 0);
		freepixels(actual);
	}
	freepixels(expected);
}

static void test_strings(void)
{
	const char wire[] = "\x1b[10;36!Courier.font!\x1b[10;0!\x1b[16;99!never-download.txt!";
	for (size_t split = 0; split <= strlen(wire); split++) {
		reset(RIP_VERSION_SKYPIX);
		feed(wire, split);
		feed(wire + split, strlen(wire) - split);
		assert(passthrough == 0 && cb.esc_len == 0 && !cb.sky_pending);
	}
	reset(RIP_VERSION_SKYPIX);
	text("\x1b[10;36!Courier.font!");
	assert(amiga_font && amiga_font->height == 36 && cterm->font_render);
	text("\x1b[10;8!missing.font!");
	assert(amiga_font == NULL && cterm->font_render == NULL && passthrough == 0);
	text("\x1b[10;8!");
	for (unsigned i = 0; i < 300; i++)
		text("a");
	text("!\x1b[15;6!");
	assert(!cb.sky_pending && cb.esc_len == 0 && passthrough == 0);
	assert((cterm->attr & 15) == 6);
	text("\x1b[10;8!unfinished\x1b[15;3!");
	assert(!cb.sky_pending && (cterm->attr & 15) == 3);
}

static void test_fonts(void)
{
	/* Every descriptor entry shipped with SyncTERM must load. */
	static const char *names[] = {"courier", "diamond", "emerald", "garnet",
	    "helvetica", "opal", "pearl", "ruby", "sapphire", "times", "topaz"};
	for (size_t n = 0; n < sizeof(names) / sizeof(*names); n++) {
		char path[MAX_PATH + 1], command[128];
		snprintf(path, sizeof(path), "%s/%s.font", SKYPIX_FONT_DIR, names[n]);
		FILE *f = fopen(path, "rb");
		assert(f);
		struct FontContentsHeader header;
		assert(fread(&header, sizeof(header), 1, f) == 1);
		for (unsigned i = 0; i < htons(header.count); i++) {
			struct StandardFontEntry entry;
			assert(fread(&entry, sizeof(entry), 1, f) == 1);
			reset(RIP_VERSION_SKYPIX);
			snprintf(command, sizeof(command), "\x1b[10;%u!%s.font!", htons(entry.height), names[n]);
			text(command);
			if (!amiga_font || !cterm->font_render)
				fprintf(stderr, "Font failed: %s/%u\n", names[n], htons(entry.height));
			assert(amiga_font && cterm->font_render);
			assert((amiga_font->height << doubled) == htons(entry.height));
		}
		fclose(f);
	}
	reset(RIP_VERSION_NONE);
	assert(!skypix && !amiga_font && !cterm->font_render && !cterm->skypix);
}

static void test_fill(void)
{
	reset(RIP_VERSION_SKYPIX);
	text("\x1b[15;3!\x1b[4;10;10;600;300!");
	uint32_t original = cterm->fg_color;
	/* A different-colour island stays unchanged; the surrounding fill exceeds
	 * one quarter of the viewport and must still complete. */
	text("\x1b[15;5!\x1b[4;30;30;40;40!\x1b[15;6!");
	uint32_t replacement = cterm->fg_color;
	uint32_t island = pixel(35, 35), outside = pixel(0, 0);
	text("\x1b[3;0;20;20!");
	assert(pixel(20, 20) == replacement && pixel(590, 290) == replacement);
	assert(pixel(35, 35) == island && pixel(0, 0) == outside);
	assert(original != replacement);
	text("\x1b[3;1;20;20!"); /* Same-colour fill is a no-op. */
	assert(pixel(20, 20) == replacement);
	text("\x1b[3;1;65535;65535!");
	assert(pixel(0, 0) == outside);
	/* Four neighbours isolate the seed; diagonal contact doesn't connect. */
	text("\x1b[15;3!\x1b[1;4;5!\x1b[1;6;5!\x1b[1;5;4!\x1b[1;5;6!"
	    "\x1b[15;6!\x1b[3;1;5;5!");
	assert(pixel(5, 5) == replacement && pixel(4, 4) == outside);
	/* Filled ellipses use the foreground pen, not the background pen. */
	text("\x1b[18;2!\x1b[15;7!\x1b[13;300;100;20;10!");
	assert(pixel(300, 100) == cterm->fg_color);
}

int main(void)
{
	assert(initciolib(CIOLIB_MODE_SDL) == 0);
	textmode(C80);
	SLEEP(50);
	test_strings();
	test_fonts();
	test_stream_order(RIP_VERSION_SKYPIX);
	test_stream_order(RIP_VERSION_3);
	test_fill();
	reset(RIP_VERSION_SKYPIX);
	text("\x1b[!\x1b[0!");
	assert(sent == 0);
	text("\x1b[1!");
	assert(rip.enabled);
	text("!|c0F");
	assert(passthrough == 5 && cb.len == 0);
	reset(RIP_VERSION_3);
	text("\x1b[!");
	assert(sent == 14);
	puts("SkyPix: fonts, strings, all stream splits, fill, reset and RIP compatibility passed");
	return 0;
}
