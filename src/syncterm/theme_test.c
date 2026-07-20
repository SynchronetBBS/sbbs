#include "theme.h"

#include "syncterm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

struct syncterm_settings settings;

static unsigned test_number;
static int failures;

char *
get_syncterm_filename(char *fn, int fnlen, int type, bool shared)
{
	(void)fn;
	(void)fnlen;
	(void)type;
	(void)shared;
	return NULL;
}

#define CHECK(expression) do {                                         \
	if (!(expression)) {                                             \
		fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__,   \
		    __LINE__, #expression);                                  \
		failures++;                                                \
	}                                                                \
} while (0)

static bool
load_text(const char *text, struct syncterm_theme **theme, char *error,
    size_t error_size)
{
	char path[128];
	snprintf(path, sizeof(path), "theme-test-%ld-%u.ini", (long)getpid(),
	    test_number++);
	FILE *fp = fopen(path, "w");
	if (fp == NULL)
		return false;
	bool wrote = fputs(text, fp) >= 0;
	if (fclose(fp) != 0)
		wrote = false;
	if (!wrote) {
		remove(path);
		return false;
	}
	bool result = syncterm_theme_load_path(path, "test.ini", theme, error,
	    error_size);
	remove(path);
	return result;
}

static const struct syncterm_theme_style *
find_style(const struct syncterm_theme *theme, const char *role)
{
	for (size_t i = 0; i < theme->style_count; i++) {
		if (strcmp(theme->styles[i].role, role) == 0)
			return &theme->styles[i];
	}
	return NULL;
}

static const struct syncterm_theme_glyph *
find_glyph(const struct syncterm_theme *theme, const char *name)
{
	for (size_t i = 0; i < theme->glyph_count; i++) {
		if (strcmp(theme->glyphs[i].name, name) == 0)
			return &theme->glyphs[i];
	}
	return NULL;
}

static void
test_valid_theme(void)
{
	static const char text[] =
	    "Name = Test Theme\n"
	    "Author = Test Author\n"
	    "Unknown = ignored\n"
	    "\n"
	    "[Style:button.focused]\n"
	    "Foreground = 0x123456\n"
	    "LegacyAttr = inherit\n"
	    "Ignored = value\n"
	    "\n"
	    "[Style:test.role]\n"
	    "Background = 255\n"
	    "\n"
	    "[Glyph:scrollbar.thumb]\n"
	    "ASCII = 42\n"
	    "\n"
	    "[Glyph:test.glyph]\n"
	    "CP437 = 219\n"
	    "ASCII = 35\n"
	    "\n"
	    "[Unknown:Section]\n"
	    "Bad = data\n";
	char error[256] = "";
	struct syncterm_theme *theme = NULL;
	CHECK(load_text(text, &theme, error, sizeof(error)));
	CHECK(theme != NULL);
	if (theme == NULL)
		return;
	CHECK(strcmp(theme->name, "Test Theme") == 0);
	CHECK(strcmp(theme->author, "Test Author") == 0);
	const struct syncterm_theme_style *style =
	    find_style(theme, "button.focused");
	CHECK(style != NULL);
	if (style != NULL) {
		CHECK(style->foreground == 0x123456);
		CHECK(style->legacy_attr == -1);
		CHECK(style->background == 0xaaaaaa);
	}
	style = find_style(theme, "test.role");
	CHECK(style != NULL);
	if (style != NULL) {
		CHECK(style->font == -1);
		CHECK(style->background == 255);
	}
	const struct syncterm_theme_glyph *glyph =
	    find_glyph(theme, "scrollbar.thumb");
	CHECK(glyph != NULL && glyph->cp437 == 0xdb && glyph->ascii == '*');
	glyph = find_glyph(theme, "test.glyph");
	CHECK(glyph != NULL && glyph->cp437 == 0xdb && glyph->ascii == '#');
	syncterm_theme_free(theme);
}

static void
test_invalid_theme(const char *text, const char *expected)
{
	char error[256] = "";
	struct syncterm_theme *theme = NULL;
	CHECK(!load_text(text, &theme, error, sizeof(error)));
	CHECK(theme == NULL);
	CHECK(strstr(error, expected) != NULL);
}

static void
test_invalid_themes(void)
{
	test_invalid_theme("Author=x\n", "Name");
	test_invalid_theme("Name=one\nName=two\n", "duplicate metadata");
	test_invalid_theme(
	    "Name=x\n[Style:button]\nForeground=nope\n", "invalid Foreground");
	test_invalid_theme(
	    "Name=x\n[Style:button]\nFont=1\nFont=2\n", "duplicate Font");
	test_invalid_theme(
	    "Name=x\n[Style:button]\nForeground=1\n"
	    "[STYLE:BUTTON]\nBackground=2\n", "duplicate section");
	test_invalid_theme(
	    "Name=x\n[Glyph:new]\nCP437=1\n", "requires CP437 and ASCII");
	test_invalid_theme(
	    "Name=x\n[Glyph:new]\nCP437=1\nASCII=1\n", "invalid ASCII");
	test_invalid_theme(
	    "Name=x\n[Style:default]\nFont=inherit\n", "must be complete");
}

static void
test_filenames(void)
{
	CHECK(syncterm_theme_valid_filename("custom.ini"));
	CHECK(syncterm_theme_valid_filename("CUSTOM.INI"));
	CHECK(!syncterm_theme_valid_filename(""));
	CHECK(!syncterm_theme_valid_filename("custom"));
	CHECK(!syncterm_theme_valid_filename("custom.ini.bak"));
	CHECK(!syncterm_theme_valid_filename("../custom.ini"));
	CHECK(!syncterm_theme_valid_filename("dir/custom.ini"));
	CHECK(!syncterm_theme_valid_filename("dir\\custom.ini"));
}

static void
test_missing_startup_theme(void)
{
	syncterm_theme_shutdown();
	strcpy(settings.theme_file, "missing.ini");
	CHECK(syncterm_theme_init(&settings));
	const struct syncterm_theme *theme = syncterm_theme_active();
	CHECK(theme != NULL);
	CHECK(theme != NULL && theme->filename != NULL &&
	    theme->filename[0] == '\0');
	CHECK(strcmp(settings.theme_file, "missing.ini") == 0);
}

int
main(void)
{
	memset(&settings, 0, sizeof(settings));
	settings.theme_frame_color = 16;
	settings.theme_text_color = 16;
	settings.theme_background_color = 8;
	settings.theme_inverse_color = 8;
	settings.theme_lightbar_color = 16;
	settings.theme_lightbar_background_color = 8;
	if (!syncterm_theme_init(&settings)) {
		fprintf(stderr, "unable to initialize theme tests\n");
		return EXIT_FAILURE;
	}
	test_valid_theme();
	test_invalid_themes();
	test_filenames();
	test_missing_startup_theme();
	syncterm_theme_shutdown();
	if (failures != 0)
		fprintf(stderr, "%d theme test(s) failed\n", failures);
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
