#include "theme_cloud.h"

#include "syncterm.h"
#include "webget.h"

#include <ctype.h>
#include <dirwrap.h>
#include <errno.h>
#include <ini_file.h>
#include <sha256.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <str_list.h>

#ifdef _WIN32
#include <windows.h>
#endif

static struct syncterm_cloud_theme_entry *catalog;
static size_t catalog_count;

static void
set_error(char *error, size_t error_size, const char *format, ...)
{
	if (error == NULL || error_size == 0)
		return;
	va_list ap;
	va_start(ap, format);
	vsnprintf(error, error_size, format, ap);
	va_end(ap);
}

static void
free_entry(struct syncterm_cloud_theme_entry *entry)
{
	free(entry->package);
	free(entry->name);
	free(entry->author);
	free(entry->description);
	free(entry->version);
	free(entry->uri);
	free(entry->sha256);
	free(entry->error);
	syncterm_theme_free(entry->theme);
}

void
syncterm_cloud_themes_shutdown(void)
{
	for (size_t i = 0; i < catalog_count; i++)
		free_entry(&catalog[i]);
	free(catalog);
	catalog = NULL;
	catalog_count = 0;
}

static bool
cloud_directory(char *path, size_t path_size, bool create)
{
	char cache[MAX_PATH + 1];
	if (get_syncterm_filename(cache, sizeof(cache),
	    SYNCTERM_PATH_SYSTEM_CACHE, false) == NULL)
		return false;
	if (snprintf(path, path_size, "%s/themes/%s", cache,
	    SYNCTERM_THEME_REPOSITORY) >= (int)path_size)
		return false;
	if (create && !isdir(path) && mkpath(path) != 0)
		return false;
	return !create || isdir(path);
}

static bool
package_id(const char *package, const char **id)
{
	if (!syncterm_theme_valid_package(package))
		return false;
	*id = strchr(package, '/') + 1;
	return true;
}

static bool
cloud_path(const char *name, char *path, size_t path_size)
{
	char directory[MAX_PATH + 1];
	if (!cloud_directory(directory, sizeof(directory), true))
		return false;
	return snprintf(path, path_size, "%s/%s", directory, name) <
	    (int)path_size;
}

static bool
file_hash(const char *path, uint8_t result[SHA256_DIGEST_SIZE])
{
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
		return false;
	SHA256_CTX context;
	SHA256Init(&context);
	uint8_t buffer[16384];
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
	if (success)
		SHA256Final(&context, result);
	return success;
}

static int
hex_value(char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	value = (char)tolower((unsigned char)value);
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	return -1;
}

static bool
valid_hash_string(const char *value)
{
	if (value == NULL || strlen(value) != SHA256_DIGEST_SIZE * 2)
		return false;
	for (size_t i = 0; value[i] != '\0'; i++) {
		if (hex_value(value[i]) < 0)
			return false;
	}
	return true;
}

static bool
hash_matches(const char *path, const char *expected)
{
	uint8_t actual[SHA256_DIGEST_SIZE];
	if (!valid_hash_string(expected) || !file_hash(path, actual))
		return false;
	for (size_t i = 0; i < sizeof(actual); i++) {
		int high = hex_value(expected[i * 2]);
		int low = hex_value(expected[i * 2 + 1]);
		if (actual[i] != (uint8_t)((high << 4) | low))
			return false;
	}
	return true;
}

static bool
replace_file(const char *source, const char *destination)
{
	char temporary[MAX_PATH + 1];
	if (snprintf(temporary, sizeof(temporary), "%s.new", destination) >=
	    (int)sizeof(temporary))
		return false;
	remove(temporary);
	if (!CopyFile(source, temporary, false))
		return false;
#ifdef _WIN32
	if (!MoveFileExA(temporary, destination,
	    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		remove(temporary);
		return false;
	}
#else
	if (rename(temporary, destination) != 0) {
		remove(temporary);
		return false;
	}
#endif
	return true;
}

static bool
append_manifest_entry(const char *id, named_string_t **values,
    struct syncterm_cloud_theme_entry **entries, size_t *count,
    char *error, size_t error_size)
{
	const char *name = NULL;
	const char *author = NULL;
	const char *description = NULL;
	const char *version = NULL;
	const char *uri = NULL;
	const char *sha256 = NULL;
	unsigned seen = 0;
	for (size_t i = 0; values != NULL && values[i] != NULL; i++) {
		const char **destination = NULL;
		unsigned bit = 0;
		if (stricmp(values[i]->name, "Name") == 0) {
			destination = &name;
			bit = 1u << 0;
		}
		else if (stricmp(values[i]->name, "Author") == 0) {
			destination = &author;
			bit = 1u << 1;
		}
		else if (stricmp(values[i]->name, "Description") == 0) {
			destination = &description;
			bit = 1u << 2;
		}
		else if (stricmp(values[i]->name, "Version") == 0) {
			destination = &version;
			bit = 1u << 3;
		}
		else if (stricmp(values[i]->name, "URI") == 0) {
			destination = &uri;
			bit = 1u << 4;
		}
		else if (stricmp(values[i]->name, "SHA256") == 0) {
			destination = &sha256;
			bit = 1u << 5;
		}
		if (destination == NULL)
			continue;
		if ((seen & bit) != 0) {
			set_error(error, error_size, "duplicate %s in Theme:%s",
			    values[i]->name, id);
			return false;
		}
		seen |= bit;
		*destination = values[i]->value;
	}
	char package[MAX_PATH + 1];
	if (snprintf(package, sizeof(package), "%s/%s",
	    SYNCTERM_THEME_REPOSITORY, id) >= (int)sizeof(package) ||
	    !syncterm_theme_valid_package(package)) {
		set_error(error, error_size, "invalid theme package id: %s", id);
		return false;
	}
	if (name == NULL || name[0] == '\0' || uri == NULL ||
	    (strncmp(uri, "http://", 7) != 0 &&
	    strncmp(uri, "https://", 8) != 0) || !valid_hash_string(sha256)) {
		set_error(error, error_size, "Theme:%s is missing valid Name, URI, or SHA256", id);
		return false;
	}
	for (size_t i = 0; i < *count; i++) {
		if (strcmp((*entries)[i].package, package) == 0) {
			set_error(error, error_size, "duplicate Theme:%s section", id);
			return false;
		}
	}
	struct syncterm_cloud_theme_entry *resized = realloc(*entries,
	    (*count + 1) * sizeof(**entries));
	if (resized == NULL) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	*entries = resized;
	struct syncterm_cloud_theme_entry *entry = &resized[*count];
	memset(entry, 0, sizeof(*entry));
	entry->package = strdup(package);
	entry->name = strdup(name);
	entry->author = author != NULL ? strdup(author) : NULL;
	entry->description = description != NULL ? strdup(description) : NULL;
	entry->version = version != NULL ? strdup(version) : NULL;
	entry->uri = strdup(uri);
	entry->sha256 = strdup(sha256);
	if (entry->package == NULL || entry->name == NULL || entry->uri == NULL ||
	    entry->sha256 == NULL || (author != NULL && entry->author == NULL) ||
	    (description != NULL && entry->description == NULL) ||
	    (version != NULL && entry->version == NULL)) {
		free_entry(entry);
		set_error(error, error_size, "out of memory");
		return false;
	}
	(*count)++;
	return true;
}

static int
compare_entries(const void *left_pointer, const void *right_pointer)
{
	const struct syncterm_cloud_theme_entry *left = left_pointer;
	const struct syncterm_cloud_theme_entry *right = right_pointer;
	int result = stricmp(left->name, right->name);
	if (result != 0)
		return result;
	return strcmp(left->package, right->package);
}

static bool
parse_manifest(const char *path,
    struct syncterm_cloud_theme_entry **entries, size_t *count,
    char *error, size_t error_size)
{
	*entries = NULL;
	*count = 0;
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		set_error(error, error_size, "unable to open theme manifest: %s",
		    strerror(errno));
		return false;
	}
	str_list_t ini = iniReadFile(fp);
	bool close_ok = fclose(fp) == 0;
	if (ini == NULL || !close_ok) {
		strListFree(&ini);
		set_error(error, error_size, "unable to read theme manifest");
		return false;
	}
	const char *repository = NULL;
	unsigned repository_count = 0;
	named_string_t **root = iniGetNamedStringList(ini, NULL);
	for (size_t i = 0; root != NULL && root[i] != NULL; i++) {
		if (stricmp(root[i]->name, "Repository") == 0) {
			repository = root[i]->value;
			repository_count++;
		}
	}
	if (repository == NULL || strcmp(repository,
	    SYNCTERM_THEME_REPOSITORY) != 0 || repository_count != 1) {
		set_error(error, error_size, "theme manifest Repository must be %s",
		    SYNCTERM_THEME_REPOSITORY);
		iniFreeNamedStringList(root);
		strListFree(&ini);
		return false;
	}
	iniFreeNamedStringList(root);
	str_list_t sections = iniGetSectionListWithDupes(ini, NULL);
	for (size_t i = 0; sections != NULL && sections[i] != NULL; i++) {
		if (strnicmp(sections[i], "Theme:", 6) != 0)
			continue;
		const char *id = sections[i] + 6;
		named_string_t **values = iniGetNamedStringList(ini, sections[i]);
		bool success = append_manifest_entry(id, values, entries, count,
		    error, error_size);
		iniFreeNamedStringList(values);
		if (!success) {
			strListFree(&sections);
			strListFree(&ini);
			for (size_t j = 0; j < *count; j++)
				free_entry(&(*entries)[j]);
			free(*entries);
			*entries = NULL;
			*count = 0;
			return false;
		}
	}
	strListFree(&sections);
	strListFree(&ini);
	if (*count > 1)
		qsort(*entries, *count, sizeof(**entries), compare_entries);
	return true;
}

static struct syncterm_cloud_theme_entry *
find_entry(const char *package)
{
	for (size_t i = 0; i < catalog_count; i++) {
		if (strcmp(catalog[i].package, package) == 0)
			return &catalog[i];
	}
	return NULL;
}

static void
set_entry_error(struct syncterm_cloud_theme_entry *entry,
    const char *message)
{
	free(entry->error);
	entry->error = message != NULL ? strdup(message) : NULL;
}

static bool
load_cached_entry(struct syncterm_cloud_theme_entry *entry)
{
	const char *id;
	if (!package_id(entry->package, &id))
		return false;
	char name[MAX_PATH + 1];
	char path[MAX_PATH + 1];
	if (snprintf(name, sizeof(name), "%s.ini", id) >= (int)sizeof(name) ||
	    !cloud_path(name, path, sizeof(path)))
		return false;
	entry->cached = fexist(path) && !isdir(path);
	entry->update_available = entry->cached &&
	    !hash_matches(path, entry->sha256);
	syncterm_theme_free(entry->theme);
	entry->theme = NULL;
	if (!entry->cached || entry->update_available)
		return false;
	char error[256];
	if (!syncterm_theme_load_path(path, "", &entry->theme, error,
	    sizeof(error))) {
		set_entry_error(entry, error);
		return false;
	}
	free(entry->theme->package);
	entry->theme->package = strdup(entry->package);
	if (entry->theme->package == NULL) {
		set_entry_error(entry, "out of memory");
		syncterm_theme_free(entry->theme);
		entry->theme = NULL;
		return false;
	}
	set_entry_error(entry, NULL);
	return true;
}

static bool
download_entry(struct syncterm_cloud_theme_entry *entry, char *error,
    size_t error_size)
{
	const char *id;
	if (!package_id(entry->package, &id)) {
		set_error(error, error_size, "invalid theme package");
		return false;
	}
	char directory[MAX_PATH + 1];
	char download_name[MAX_PATH + 1];
	char cache_key[MAX_PATH + 1];
	char download_path[MAX_PATH + 1];
	char installed_name[MAX_PATH + 1];
	char installed_path[MAX_PATH + 1];
	if (!cloud_directory(directory, sizeof(directory), true) ||
	    snprintf(download_name, sizeof(download_name), "%s.download", id) >=
	    (int)sizeof(download_name) ||
	    snprintf(cache_key, sizeof(cache_key), "theme-%s", id) >=
	    (int)sizeof(cache_key) ||
	    snprintf(download_path, sizeof(download_path), "%s/%s", directory,
	    download_name) >= (int)sizeof(download_path) ||
	    snprintf(installed_name, sizeof(installed_name), "%s.ini", id) >=
	    (int)sizeof(installed_name) ||
	    snprintf(installed_path, sizeof(installed_path), "%s/%s", directory,
	    installed_name) >= (int)sizeof(installed_path)) {
		set_error(error, error_size, "theme cache path is too long");
		return false;
	}
	struct webget_request request;
	if (!init_webget_file_req(&request, directory, entry->name, entry->uri,
	    cache_key, download_name)) {
		set_error(error, error_size, "unable to initialize theme download");
		return false;
	}
	bool fetched = webget_fetch(&request, true);
	if (!fetched) {
		set_error(error, error_size, "%s",
		    request.msg != NULL ? request.msg : "theme download failed");
		destroy_webget_req(&request);
		return false;
	}
	destroy_webget_req(&request);
	if (!hash_matches(download_path, entry->sha256)) {
		set_error(error, error_size, "downloaded theme has the wrong SHA256");
		return false;
	}
	struct syncterm_theme *theme = NULL;
	if (!syncterm_theme_load_path(download_path, "", &theme, error,
	    error_size))
		return false;
	syncterm_theme_free(theme);
	if (!replace_file(download_path, installed_path)) {
		set_error(error, error_size, "unable to install cached theme");
		return false;
	}
	return load_cached_entry(entry);
}

bool
syncterm_cloud_theme_cache(const char *package, char *error,
    size_t error_size)
{
	struct syncterm_cloud_theme_entry *entry = find_entry(package);
	if (entry == NULL) {
		set_error(error, error_size, "theme package is not in the manifest");
		return false;
	}
	if (entry->uri == NULL || entry->sha256 == NULL) {
		set_error(error, error_size, "%s", entry->error != NULL ?
		    entry->error : "theme package is unavailable");
		return false;
	}
	if (load_cached_entry(entry))
		return true;
	if (!download_entry(entry, error, error_size)) {
		set_entry_error(entry, error);
		return false;
	}
	return true;
}

bool
syncterm_cloud_theme_delete(const char *package, char *error,
    size_t error_size)
{
	if (safe_mode) {
		set_error(error, error_size, "themes cannot be deleted in safe mode");
		return false;
	}
	struct syncterm_cloud_theme_entry *entry = find_entry(package);
	const char *id;
	char directory[MAX_PATH + 1];
	char installed[MAX_PATH + 1];
	if (entry == NULL || !package_id(package, &id) ||
	    !cloud_directory(directory, sizeof(directory), false) ||
	    !syncterm_theme_package_path(package, installed, sizeof(installed))) {
		set_error(error, error_size, "theme package is not cached");
		return false;
	}
	if (remove(installed) != 0) {
		set_error(error, error_size, "unable to delete cached theme: %s",
		    strerror(errno));
		return false;
	}
	char path[MAX_PATH + 1];
	if (snprintf(path, sizeof(path), "%s/%s.download", directory, id) <
	    (int)sizeof(path))
		remove(path);
	if (snprintf(path, sizeof(path), "%s/theme-%s.cacheinfo", directory, id) <
	    (int)sizeof(path))
		remove(path);
	entry->cached = false;
	entry->update_available = false;
	syncterm_theme_free(entry->theme);
	entry->theme = NULL;
	set_entry_error(entry, NULL);
	return true;
}

static bool
refresh_manifest(char *warning, size_t warning_size)
{
	char directory[MAX_PATH + 1];
	char download[MAX_PATH + 1];
	char installed[MAX_PATH + 1];
	if (!cloud_directory(directory, sizeof(directory), true) ||
	    !cloud_path("manifest.lst.download", download, sizeof(download)) ||
	    !cloud_path("manifest.lst", installed, sizeof(installed))) {
		set_error(warning, warning_size, "theme cache path is unavailable");
		return false;
	}
	struct webget_request request;
	if (!init_webget_file_req(&request, directory, "Theme Catalog",
	    SYNCTERM_THEME_MANIFEST_URI, "manifest", "manifest.lst.download")) {
		set_error(warning, warning_size, "unable to initialize theme catalog download");
		return false;
	}
	bool fetched = webget_fetch(&request, false);
	struct syncterm_cloud_theme_entry *entries = NULL;
	size_t count = 0;
	char parse_error[256] = "";
	bool parsed = fetched && parse_manifest(download, &entries, &count,
	    parse_error, sizeof(parse_error));
	if (fetched && !parsed) {
		fetched = webget_fetch(&request, true);
		parsed = fetched && parse_manifest(download, &entries, &count,
		    parse_error, sizeof(parse_error));
	}
	if (!fetched)
		set_error(warning, warning_size, "%s", request.msg != NULL ?
		    request.msg : "theme catalog download failed");
	else if (!parsed)
		set_error(warning, warning_size, "%s", parse_error);
	destroy_webget_req(&request);
	if (!parsed)
		return false;
	for (size_t i = 0; i < count; i++)
		free_entry(&entries[i]);
	free(entries);
	if (!replace_file(download, installed)) {
		set_error(warning, warning_size, "unable to install theme catalog");
		return false;
	}
	return true;
}

bool
syncterm_cloud_themes_refresh(bool fetch, const char *selected_package,
    char *error, size_t error_size)
{
	if (error != NULL && error_size > 0)
		error[0] = '\0';
	char warning[256] = "";
	if (fetch)
		refresh_manifest(warning, sizeof(warning));
	char manifest[MAX_PATH + 1];
	if (!cloud_path("manifest.lst", manifest, sizeof(manifest))) {
		set_error(error, error_size, "theme cache path is unavailable");
		return false;
	}
	struct syncterm_cloud_theme_entry *entries;
	size_t count;
	if (!parse_manifest(manifest, &entries, &count, error, error_size)) {
		if (warning[0] != '\0')
			set_error(error, error_size, "%s", warning);
		return false;
	}
	if (selected_package != NULL && selected_package[0] != '\0') {
		bool found = false;
		for (size_t i = 0; i < count; i++) {
			if (strcmp(entries[i].package, selected_package) == 0) {
				found = true;
				break;
			}
		}
		if (!found && syncterm_theme_valid_package(selected_package)) {
			struct syncterm_cloud_theme_entry *resized = realloc(entries,
			    (count + 1) * sizeof(*entries));
			if (resized != NULL) {
				entries = resized;
				memset(&entries[count], 0, sizeof(entries[count]));
				entries[count].package = strdup(selected_package);
				entries[count].name = strdup(selected_package);
				entries[count].error = strdup(
				    "the configured theme package is not in the manifest");
				if (entries[count].package != NULL &&
				    entries[count].name != NULL && entries[count].error != NULL)
					count++;
				else
					free_entry(&entries[count]);
			}
		}
	}
	syncterm_cloud_themes_shutdown();
	catalog = entries;
	catalog_count = count;
	for (size_t i = 0; i < catalog_count; i++) {
		if (catalog[i].uri == NULL || catalog[i].sha256 == NULL)
			continue;
		bool selected = selected_package != NULL &&
		    strcmp(catalog[i].package, selected_package) == 0;
		bool was_cached = false;
		const char *id;
		char name[MAX_PATH + 1];
		char path[MAX_PATH + 1];
		if (package_id(catalog[i].package, &id) &&
		    snprintf(name, sizeof(name), "%s.ini", id) < (int)sizeof(name) &&
		    cloud_path(name, path, sizeof(path)))
			was_cached = fexist(path) && !isdir(path);
		load_cached_entry(&catalog[i]);
		if (fetch && (selected || was_cached) && catalog[i].theme == NULL) {
			char update_error[256];
			if (!download_entry(&catalog[i], update_error,
			    sizeof(update_error)))
				set_entry_error(&catalog[i], update_error);
		}
	}
	if (warning[0] != '\0')
		set_error(error, error_size, "%s", warning);
	return true;
}

bool
syncterm_cloud_themes_available(void)
{
	char directory[MAX_PATH + 1];
	char manifest[MAX_PATH + 1];
	if (!cloud_directory(directory, sizeof(directory), false) ||
	    snprintf(manifest, sizeof(manifest), "%s/manifest.lst", directory) >=
	    (int)sizeof(manifest))
		return false;
	return fexist(manifest) && !isdir(manifest);
}

size_t
syncterm_cloud_theme_count(void)
{
	return catalog_count;
}

const struct syncterm_cloud_theme_entry *
syncterm_cloud_theme_entry(size_t index)
{
	return index < catalog_count ? &catalog[index] : NULL;
}

const char *
syncterm_cloud_theme_preview(const char *package)
{
	struct syncterm_cloud_theme_entry *entry = find_entry(package);
	if (entry == NULL)
		return "theme package is not in the manifest";
	if (entry->theme == NULL)
		return entry->error != NULL ? entry->error : "theme is not downloaded";
	return syncterm_theme_preview_loaded(entry->theme);
}

struct syncterm_theme_document *
syncterm_cloud_theme_copy(const char *package, char *error,
    size_t error_size)
{
	if (!syncterm_cloud_theme_cache(package, error, error_size))
		return NULL;
	char path[MAX_PATH + 1];
	if (!syncterm_theme_package_path(package, path, sizeof(path))) {
		set_error(error, error_size, "theme cache path is unavailable");
		return NULL;
	}
	return syncterm_theme_document_copy_path(path, error, error_size);
}
