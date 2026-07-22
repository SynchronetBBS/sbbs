#include "theme.h"
#include "theme_cloud.h"

#include "syncterm.h"

#include <dirwrap.h>
#include <sha256.h>
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
static char system_cache_directory[128];

char *
get_syncterm_filename(char *fn, int fnlen, int type, bool shared)
{
	(void)shared;
	const char *directory;
	const char *suffix;
	if (type == SYNCTERM_PATH_THEMES) {
		directory = themes_directory;
		suffix = "/";
	}
	else if (type == SYNCTERM_PATH_SYSTEM_CACHE) {
		directory = system_cache_directory;
		suffix = "";
	}
	else
		return NULL;
	if (snprintf(fn, fnlen, "%s%s", directory, suffix) >= fnlen)
		return NULL;
	return fn;
}

bool
init_webget_file_req(struct webget_request *req, const char *cache_root,
    const char *name, const char *uri, const char *cache_key,
    const char *output_name)
{
	(void)req;
	(void)cache_root;
	(void)name;
	(void)uri;
	(void)cache_key;
	(void)output_name;
	return false;
}

bool
iniReadHttp(struct webget_request *req)
{
	(void)req;
	return false;
}

bool
webget_fetch(struct webget_request *req, bool force)
{
	(void)req;
	(void)force;
	return false;
}

void
destroy_webget_req(struct webget_request *req)
{
	(void)req;
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
	CHECK(syncterm_theme_valid_filename("manifest.ini"));
	CHECK(!syncterm_theme_valid_filename("../custom.ini"));
	CHECK(!syncterm_theme_valid_filename("dir/custom.ini"));
	CHECK(!syncterm_theme_valid_filename("dir\\custom.ini"));
	CHECK(syncterm_theme_valid_package("official/custom"));
	CHECK(syncterm_theme_valid_package("official/custom-theme_2.0"));
	CHECK(!syncterm_theme_valid_package("custom"));
	CHECK(!syncterm_theme_valid_package("official/"));
	CHECK(!syncterm_theme_valid_package("official/Custom"));
	CHECK(syncterm_theme_valid_package("official/manifest"));
	CHECK(!syncterm_theme_valid_package("official/../custom"));
	CHECK(!syncterm_theme_valid_package("other/custom"));
}

static void
test_delete_theme(void)
{
	static const char text[] = "Name = Delete Me\n";
	char path[256];
	char error[256] = "";
	CHECK(write_theme_file("delete.ini", text));
	snprintf(path, sizeof(path), "%s/delete.ini", themes_directory);
	CHECK(fexist(path));
	CHECK(syncterm_theme_delete("delete.ini", error, sizeof(error)));
	CHECK(!fexist(path));
	CHECK(write_theme_file("delete.ini", text));
	safe_mode = 1;
	CHECK(!syncterm_theme_delete("delete.ini", error, sizeof(error)));
	CHECK(fexist(path));
	safe_mode = 0;
	CHECK(syncterm_theme_delete("delete.ini", error, sizeof(error)));
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
	syncterm_theme_shutdown();
	settings.theme_file[0] = '\0';
	strcpy(settings.theme_package, "official/missing");
	CHECK(syncterm_theme_init(&settings));
	theme = syncterm_theme_active();
	CHECK(theme != NULL && theme->package == NULL);
	CHECK(strcmp(settings.theme_package, "official/missing") == 0);
	settings.theme_package[0] = '\0';
}

static void
test_official_themes(void)
{
	static const struct {
		const char *filename;
		const char *name;
	} themes[] = {
		{ "classic-extended.ini", "Classic Extended" },
		{ "DarkMode.ini", "Dark Mode" },
		{ "HotDogStand.ini", "Hot Dog Stand" },
		{ "emberworn.ini", "Emberworn" },
		{ "emberless.ini", "Emberless" },
		{ "verdantworn.ini", "Verdantworn" },
		{ "verdantless.ini", "Verdantless" },
		{ "azureworn.ini", "Azureworn" },
		{ "azureless.ini", "Azureless" },
		{ "crimsonbound.ini", "Crimsonbound" },
		{ "viridianbound.ini", "Viridianbound" },
		{ "ceruleanbound.ini", "Ceruleanbound" },
		{ "ashen.ini", "Ashen" },
	};
	for (size_t i = 0; i < sizeof(themes) / sizeof(themes[0]); i++) {
		char path[512];
		CHECK(snprintf(path, sizeof(path), "%s/%s",
		    SYNCTERM_THEME_SOURCE_DIR, themes[i].filename) <
		    (int)sizeof(path));
		char error[256] = "";
		struct syncterm_theme *theme = NULL;
		CHECK(syncterm_theme_load_path(path, themes[i].filename, &theme,
		    error, sizeof(error)));
		CHECK(theme != NULL);
		if (theme == NULL)
			continue;
		CHECK(strcmp(theme->name, themes[i].name) == 0);
		CHECK(theme->style_count >= 54);
		for (size_t j = 0; j < theme->style_count; j++) {
			CHECK(theme->styles[j].legacy_attr >= -1);
			CHECK(theme->styles[j].legacy_attr < 0 ||
			    theme->styles[j].legacy_attr <= 0x7f);
			CHECK(theme->styles[j].foreground >= 0);
			CHECK(theme->styles[j].background >= 0);
		}
		if (strcmp(themes[i].filename, "classic-extended.ini") == 0) {
			const struct syncterm_theme *classic =
			    syncterm_theme_active();
			CHECK(classic != NULL);
			CHECK(classic != NULL &&
			    theme->style_count == classic->style_count);
			for (size_t j = 0; classic != NULL &&
			    j < theme->style_count; j++) {
				const struct syncterm_theme_style *classic_style =
				    find_style(classic, theme->styles[j].role);
				CHECK(classic_style != NULL);
				CHECK(classic_style != NULL &&
				    theme->styles[j].legacy_attr ==
				    classic_style->legacy_attr);
			}
		}
		syncterm_theme_free(theme);
	}
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

static bool
write_cloud_file(const char *filename, const char *text)
{
	char directory[256];
	snprintf(directory, sizeof(directory), "%s/themes/official",
	    system_cache_directory);
	if (!isdir(directory) && mkpath(directory) != 0)
		return false;
	char path[320];
	snprintf(path, sizeof(path), "%s/%s", directory, filename);
	FILE *fp = fopen(path, "w");
	if (fp == NULL)
		return false;
	bool result = fputs(text, fp) >= 0 && fclose(fp) == 0;
	if (!result)
		remove(path);
	return result;
}

static bool
hash_file_hex(const char *path, char hash[SHA256_DIGEST_SIZE * 2 + 1])
{
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
		return false;
	SHA256_CTX context;
	SHA256Init(&context);
	uint8_t buffer[4096];
	bool success = true;
	for (;;) {
		size_t length = fread(buffer, 1, sizeof(buffer), fp);
		if (length > 0)
			SHA256Update(&context, buffer, length);
		if (length < sizeof(buffer)) {
			if (ferror(fp))
				success = false;
			break;
		}
	}
	if (fclose(fp) != 0)
		success = false;
	if (success) {
		uint8_t digest[SHA256_DIGEST_SIZE];
		SHA256Final(&context, digest);
		SHA256_hex(hash, digest);
	}
	return success;
}

static size_t
manifest_theme_count(const char *path)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return 0;
	size_t count = 0;
	char line[256];
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (strnicmp(line, "[Theme:", 7) == 0)
			count++;
	}
	fclose(fp);
	return count;
}

static void
test_cloud_catalog(void)
{
	static const char cloud_theme[] =
	    "Name = Cached Cloud Theme\n"
	    "Author = Theme Test\n";
	uint8_t digest[SHA256_DIGEST_SIZE];
	char hash[SHA256_DIGEST_SIZE * 2 + 1];
	SHA256_calc(digest, cloud_theme, strlen(cloud_theme));
	SHA256_hex(hash, digest);
	char source_manifest[512];
	char cached_manifest[320];
	char cached_directory[256];
	snprintf(source_manifest, sizeof(source_manifest), "%s/manifest.lst",
	    SYNCTERM_THEME_SOURCE_DIR);
	snprintf(cached_directory, sizeof(cached_directory),
	    "%s/themes/official", system_cache_directory);
	CHECK(isdir(cached_directory) || mkpath(cached_directory) == 0);
	snprintf(cached_manifest, sizeof(cached_manifest), "%s/manifest.lst",
	    cached_directory);
	CHECK(CopyFile(source_manifest, cached_manifest, false));
	char error[256] = "";
	CHECK(syncterm_cloud_themes_refresh(false, NULL, error, sizeof(error)));
	CHECK(syncterm_cloud_theme_count() ==
	    manifest_theme_count(source_manifest));
	for (size_t i = 0; i < syncterm_cloud_theme_count(); i++) {
		const struct syncterm_cloud_theme_entry *official =
		    syncterm_cloud_theme_entry(i);
		const char *filename = strrchr(official->uri, '/');
		CHECK(filename != NULL && filename[1] != '\0');
		if (filename == NULL || filename[1] == '\0')
			continue;
		char source[512];
		char actual[SHA256_DIGEST_SIZE * 2 + 1];
		snprintf(source, sizeof(source), "%s/%s",
		    SYNCTERM_THEME_SOURCE_DIR, filename + 1);
		CHECK(hash_file_hex(source, actual));
		CHECK(strcmp(actual, official->sha256) == 0);
	}
	syncterm_cloud_themes_shutdown();
	CHECK(write_cloud_file("alpha.ini", cloud_theme));
	char manifest[2048];
	snprintf(manifest, sizeof(manifest),
	    "Repository = official\n"
	    "Name = Test Catalog\n"
	    "\n"
	    "[Theme:alpha]\n"
	    "Name = Alpha Theme\n"
	    "Author = Test Author\n"
	    "Description = Cached theme test\n"
	    "Version = 1.2\n"
	    "URI = http://example.invalid/alpha.ini\n"
	    "SHA256 = %s\n", hash);
	CHECK(write_cloud_file("manifest.lst", manifest));
	CHECK(syncterm_cloud_themes_refresh(false, "official/alpha", error,
	    sizeof(error)));
	CHECK(syncterm_cloud_theme_count() == 1);
	const struct syncterm_cloud_theme_entry *entry =
	    syncterm_cloud_theme_entry(0);
	CHECK(entry != NULL);
	if (entry != NULL) {
		CHECK(strcmp(entry->package, "official/alpha") == 0);
		CHECK(strcmp(entry->name, "Alpha Theme") == 0);
		CHECK(entry->cached);
		CHECK(!entry->update_available);
		CHECK(entry->theme != NULL);
	}
	CHECK(syncterm_cloud_theme_preview("official/alpha") == NULL);
	syncterm_theme_cancel_preview();
	struct syncterm_theme_document *copy = syncterm_cloud_theme_copy(
	    "official/alpha", error, sizeof(error));
	CHECK(copy != NULL);
	if (copy != NULL) {
		CHECK(syncterm_theme_document_filename(copy)[0] == '\0');
		syncterm_theme_document_free(copy);
	}
	struct syncterm_theme *loaded = NULL;
	CHECK(syncterm_theme_load_package("official/alpha", &loaded, error,
	    sizeof(error)));
	if (loaded != NULL) {
		CHECK(strcmp(loaded->package, "official/alpha") == 0);
		syncterm_theme_free(loaded);
	}
	CHECK(syncterm_cloud_theme_delete("official/alpha", error,
	    sizeof(error)));
	entry = syncterm_cloud_theme_entry(0);
	CHECK(entry != NULL && !entry->cached && entry->theme == NULL);
	char cached_theme[320];
	snprintf(cached_theme, sizeof(cached_theme), "%s/alpha.ini",
	    cached_directory);
	CHECK(!fexist(cached_theme));
	CHECK(write_cloud_file("manifest.lst",
	    "Repository = unofficial\n[Theme:bad]\nName = Bad\n"));
	CHECK(!syncterm_cloud_themes_refresh(false, NULL, error,
	    sizeof(error)));
	CHECK(syncterm_cloud_theme_count() == 1);
	CHECK(write_cloud_file("manifest.lst", manifest));
	CHECK(syncterm_cloud_themes_refresh(false, "official/missing", error,
	    sizeof(error)));
	CHECK(syncterm_cloud_theme_count() == 2);
	entry = syncterm_cloud_theme_entry(1);
	CHECK(entry != NULL && entry->error != NULL);
	syncterm_cloud_themes_shutdown();
}

int
main(void)
{
	snprintf(themes_directory, sizeof(themes_directory),
	    "theme-test-%ld", (long)getpid());
	snprintf(system_cache_directory, sizeof(system_cache_directory),
	    "theme-cloud-test-%ld", (long)getpid());
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
	test_delete_theme();
	test_official_themes();
	test_document_edit_and_save();
	test_document_preservation_and_conflict();
	test_document_import();
	test_cloud_catalog();
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
	static const char *cloud_files[] = {"alpha.ini", "manifest.lst"};
	for (size_t i = 0; i < sizeof(cloud_files) / sizeof(cloud_files[0]); i++) {
		char path[320];
		snprintf(path, sizeof(path), "%s/themes/official/%s",
		    system_cache_directory, cloud_files[i]);
		remove(path);
	}
	char path[320];
	snprintf(path, sizeof(path), "%s/themes/official", system_cache_directory);
	remove_directory(path);
	snprintf(path, sizeof(path), "%s/themes", system_cache_directory);
	remove_directory(path);
	remove_directory(system_cache_directory);
	if (failures != 0)
		fprintf(stderr, "%d theme test(s) failed\n", failures);
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
