#include "theme.h"

#include "syncterm.h"

#include <dirwrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define getpid _getpid
#define remove_directory _rmdir
#else
#include <unistd.h>
#define remove_directory rmdir
#endif

struct syncterm_settings settings;
int safe_mode;

static unsigned test_number;
static int failures;
static char themes_directory[128];

char *
get_syncterm_filename(char *fn, int fnlen, int type, bool shared)
{
	(void)shared;
	if (type != SYNCTERM_PATH_THEMES ||
	    snprintf(fn, fnlen, "%s/", themes_directory) >= fnlen)
		return NULL;
	return fn;
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

static bool
write_theme_file(const char *filename, const char *text)
{
	char path[256];
	snprintf(path, sizeof(path), "%s/%s", themes_directory, filename);
	FILE *fp = fopen(path, "w");
	if (fp == NULL)
		return false;
	bool result = fputs(text, fp) >= 0 && fclose(fp) == 0;
	if (!result)
		remove(path);
	return result;
}

static char *
read_theme_file(const char *filename)
{
	char path[256];
	snprintf(path, sizeof(path), "%s/%s", themes_directory, filename);
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
		return NULL;
	CHECK(fseek(fp, 0, SEEK_END) == 0);
	long length = ftell(fp);
	CHECK(length >= 0);
	CHECK(fseek(fp, 0, SEEK_SET) == 0);
	if (length < 0) {
		fclose(fp);
		return NULL;
	}
	char *text = malloc((size_t)length + 1);
	if (text == NULL) {
		fclose(fp);
		return NULL;
	}
	if (fread(text, 1, (size_t)length, fp) != (size_t)length) {
		free(text);
		fclose(fp);
		return NULL;
	}
	text[length] = '\0';
	fclose(fp);
	return text;
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

static bool
document_has_style(struct syncterm_theme_document *document,
    const char *role)
{
	struct syncterm_theme_document_style style;
	for (size_t i = 0; i < syncterm_theme_document_style_count(document);
	    i++) {
		if (syncterm_theme_document_style(document, i, &style) &&
		    strcmp(style.role, role) == 0)
			return true;
	}
	return false;
}

static bool
document_has_glyph(struct syncterm_theme_document *document,
    const char *name)
{
	struct syncterm_theme_document_glyph glyph;
	for (size_t i = 0; i < syncterm_theme_document_glyph_count(document);
	    i++) {
		if (syncterm_theme_document_glyph(document, i, &glyph) &&
		    strcmp(glyph.name, name) == 0)
			return true;
	}
	return false;
}

static void
test_document_edit_and_save(void)
{
	char error[256] = "";
	struct syncterm_theme_document *document =
	    syncterm_theme_document_new(error, sizeof(error));
	CHECK(document != NULL);
	if (document == NULL)
		return;
	CHECK(syncterm_theme_document_dirty(document));
	CHECK(syncterm_theme_document_set_metadata(document,
	    SYNCTERM_THEME_METADATA_NAME, "Document Test", error,
	    sizeof(error)));
	CHECK(syncterm_theme_document_set_style(document, "button.focused",
	    SYNCTERM_THEME_STYLE_FOREGROUND, SYNCTERM_THEME_VALUE_EXPLICIT,
	    0x123456, error, sizeof(error)));
	CHECK(syncterm_theme_document_add_style(document, "custom.test", error,
	    sizeof(error)));
	CHECK(syncterm_theme_document_add_glyph(document, "custom.marker", 219,
	    '#', error, sizeof(error)));
	CHECK(document_has_style(document, "custom.test"));
	CHECK(document_has_glyph(document, "custom.marker"));
	CHECK(syncterm_theme_document_undo(document));
	CHECK(!document_has_glyph(document, "custom.marker"));
	CHECK(syncterm_theme_document_redo(document));
	CHECK(document_has_glyph(document, "custom.marker"));
	CHECK(syncterm_theme_document_save(document, "document.ini", false,
	    error, sizeof(error)) == SYNCTERM_THEME_DOCUMENT_OK);
	CHECK(!syncterm_theme_document_dirty(document));
	CHECK(strcmp(syncterm_theme_document_filename(document),
	    "document.ini") == 0);
	struct syncterm_theme *loaded = NULL;
	char path[256];
	snprintf(path, sizeof(path), "%s/document.ini", themes_directory);
	CHECK(syncterm_theme_load_path(path, "document.ini", &loaded, error,
	    sizeof(error)));
	const struct syncterm_theme_style *style = loaded != NULL ?
	    find_style(loaded, "button.focused") : NULL;
	CHECK(style != NULL && style->foreground == 0x123456);
	CHECK(loaded != NULL && find_style(loaded, "custom.test") != NULL);
	CHECK(loaded != NULL && find_glyph(loaded, "custom.marker") != NULL);
	syncterm_theme_free(loaded);
	syncterm_theme_document_free(document);
}

static void
test_document_preservation_and_conflict(void)
{
	static const char source[] =
	    "; root comment\n"
	    "Name = Preserve Test\n"
	    "Private = retained\n"
	    "\n"
	    "[Style:button]\n"
	    "; style comment\n"
	    "Unknown = retained\n"
	    "Foreground = 7\n"
	    "\n"
	    "[Private]\n"
	    "Value = retained\n";
	CHECK(write_theme_file("preserve.ini", source));
	char error[256] = "";
	struct syncterm_theme_document *document =
	    syncterm_theme_document_open("preserve.ini", error, sizeof(error));
	CHECK(document != NULL);
	if (document == NULL)
		return;
	CHECK(syncterm_theme_document_set_style(document, "button",
	    SYNCTERM_THEME_STYLE_BACKGROUND, SYNCTERM_THEME_VALUE_EXPLICIT,
	    0x010203, error, sizeof(error)));
	CHECK(syncterm_theme_document_save(document, NULL, false, error,
	    sizeof(error)) == SYNCTERM_THEME_DOCUMENT_OK);
	char *saved = read_theme_file("preserve.ini");
	CHECK(saved != NULL);
	if (saved != NULL) {
		CHECK(strstr(saved, "; root comment") != NULL);
		CHECK(strstr(saved, "; style comment") != NULL);
		CHECK(strstr(saved, "Unknown = retained") != NULL);
		CHECK(strstr(saved, "[Private]\nValue = retained") != NULL);
		free(saved);
	}
	CHECK(syncterm_theme_document_set_metadata(document,
	    SYNCTERM_THEME_METADATA_AUTHOR, "Changed", error, sizeof(error)));
	char path[256];
	snprintf(path, sizeof(path), "%s/preserve.ini", themes_directory);
	FILE *fp = fopen(path, "a");
	CHECK(fp != NULL);
	if (fp != NULL) {
		CHECK(fputs("; external change\n", fp) >= 0);
		CHECK(fclose(fp) == 0);
	}
	CHECK(syncterm_theme_document_save(document, NULL, false, error,
	    sizeof(error)) == SYNCTERM_THEME_DOCUMENT_CONFLICT);
	CHECK(syncterm_theme_document_save(document, NULL, true, error,
	    sizeof(error)) == SYNCTERM_THEME_DOCUMENT_OK);
	syncterm_theme_document_free(document);
}

static void
test_document_import(void)
{
	static const char source[] =
	    "Name = Import Source\n"
	    "\n"
	    "[Style:custom.imported]\n"
	    "; imported comment\n"
	    "Foreground = 0xabcdef\n"
	    "Private = retained\n"
	    "\n"
	    "[Glyph:custom.imported]\n"
	    "CP437 = 177\n"
	    "ASCII = 43\n";
	CHECK(write_theme_file("import.ini", source));
	char error[256] = "";
	struct syncterm_theme_document *document =
	    syncterm_theme_document_new(error, sizeof(error));
	CHECK(document != NULL);
	if (document == NULL)
		return;
	CHECK(syncterm_theme_document_set_metadata(document,
	    SYNCTERM_THEME_METADATA_NAME, "Import Target", error,
	    sizeof(error)));
	CHECK(syncterm_theme_document_import(document, "import.ini", error,
	    sizeof(error)));
	CHECK(document_has_style(document, "custom.imported"));
	CHECK(document_has_glyph(document, "custom.imported"));
	CHECK(strcmp(syncterm_theme_document_metadata(document,
	    SYNCTERM_THEME_METADATA_NAME), "Import Target") == 0);
	CHECK(syncterm_theme_document_undo(document));
	CHECK(!document_has_style(document, "custom.imported"));
	CHECK(syncterm_theme_document_redo(document));
	CHECK(document_has_style(document, "custom.imported"));
	CHECK(syncterm_theme_document_save(document, "imported.ini", false,
	    error, sizeof(error)) == SYNCTERM_THEME_DOCUMENT_OK);
	char *saved = read_theme_file("imported.ini");
	CHECK(saved != NULL);
	if (saved != NULL) {
		CHECK(strstr(saved, "; imported comment") != NULL);
		CHECK(strstr(saved, "Private = retained") != NULL);
		CHECK(strstr(saved, "Import Target") != NULL);
		free(saved);
	}
	syncterm_theme_document_free(document);
}

int
main(void)
{
	snprintf(themes_directory, sizeof(themes_directory),
	    "theme-test-%ld", (long)getpid());
	if (MKDIR(themes_directory) != 0) {
		perror(themes_directory);
		return EXIT_FAILURE;
	}
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
	test_document_edit_and_save();
	test_document_preservation_and_conflict();
	test_document_import();
	test_missing_startup_theme();
	syncterm_theme_shutdown();
	static const char *files[] = {
		"document.ini", "preserve.ini", "import.ini", "imported.ini",
	};
	for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
		char path[256];
		snprintf(path, sizeof(path), "%s/%s", themes_directory, files[i]);
		remove(path);
	}
	remove_directory(themes_directory);
	if (failures != 0)
		fprintf(stderr, "%d theme test(s) failed\n", failures);
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
