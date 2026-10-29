/* Copyright (c) 2026 Stephen Hurd; SPDX-License-Identifier: BSD-2-Clause */

#include "bbs_cache.h"
#include "bbslist.h"

#include <dirwrap.h>
#include <sha256.h>
#include <utf8_codepages.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "bbs_cache_unicode.h"

struct scalar_list {
	uint32_t *items;
	size_t count;
	size_t capacity;
};

/* Canonical ordering must span scalar and case-fold expansion boundaries. */
static bool
nfd_append(struct scalar_list *list, uint32_t codepoint)
{
	uint32_t values[8];
	size_t count = st_unicode_canonical_decompose(codepoint, values, 8);

	if (count == 0 || count > 8 || count > SIZE_MAX - list->count)
		return false;
	if (list->count + count > list->capacity) {
		size_t capacity = list->capacity;
		uint32_t *items;

		if (capacity > SIZE_MAX / 2)
			return false;
		capacity = capacity == 0 ? 32 : capacity * 2;
		if (capacity < list->count + count)
			capacity = list->count + count;
		if (capacity > SIZE_MAX / sizeof(*items))
			return false;
		items = realloc(list->items, capacity * sizeof(*items));
		if (items == NULL)
			return false;
		list->items = items;
		list->capacity = capacity;
	}
	for (size_t i = 0; i < count; ++i) {
		uint8_t ccc = st_unicode_canonical_combining_class(values[i]);
		size_t position = list->count++;

		while (ccc != 0 && position > 0) {
			uint8_t prior = st_unicode_canonical_combining_class(list->items[position - 1]);

			if (prior == 0 || prior <= ccc)
				break;
			list->items[position] = list->items[position - 1];
			--position;
		}
		list->items[position] = values[i];
	}
	return true;
}

bool
bbs_cache_key(char *result, size_t size, const char *name)
{
	struct scalar_list first = {0};
	struct scalar_list folded = {0};
	SHA256_CTX context;
	uint8_t digest[SHA256_DIGEST_SIZE];
	bool success = false;

	if (result == NULL || size <= BBS_CACHE_KEY_MAX || name == NULL || *name == 0)
		return false;
	/* Directory names are CP437; only the canonical hash input is UTF-8. */
	for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
		uint32_t cp = cpoint_from_cpchar(CIOLIB_CP437, *p);

		if (!nfd_append(&first, cp))
			goto done;
	}
	for (size_t i = 0; i < first.count; ++i) {
		uint32_t mapping[3];
		size_t count = st_unicode_casefold_mapping(first.items[i], mapping);

		for (size_t j = 0; j < count; ++j)
			if (!nfd_append(&folded, mapping[j]))
				goto done;
	}
	SHA256Init(&context);
	for (size_t i = 0; i < folded.count; ++i) {
		uint8_t bytes[4];
		int count = utf8_from_cpoint(bytes, folded.items[i]);

		if (count <= 0)
			goto done;
		SHA256Update(&context, bytes, count);
	}
	SHA256Final(&context, digest);
	memcpy(result, "[u18-", 5);
	SHA256_hex(result + 5, digest);
	result[69] = ']';
	result[70] = 0;
	success = true;
done:
	free(first.items);
	free(folded.items);
	return success;
}

bool
bbs_cache_key_recognized(const char *key)
{
	if (key == NULL || strlen(key) != BBS_CACHE_KEY_MAX ||
	    memcmp(key, "[u18-", 5) != 0 || key[69] != ']')
		return false;
	for (size_t i = 5; i < 69; ++i)
		if (!((key[i] >= '0' && key[i] <= '9') || (key[i] >= 'a' && key[i] <= 'f')))
			return false;
	return true;
}

bool
bbs_cache_path(char *result, size_t size, const char *root, const char *name)
{
	char key[BBS_CACHE_KEY_MAX + 1];
	size_t length;
	bool separator;

	if (result == NULL || root == NULL || *root == 0 ||
	    !bbs_cache_key(key, sizeof(key), name))
		return false;
	length = strlen(root);
	separator = !IS_PATH_DELIM(root[length - 1]);
	if (length >= size || size - length <= BBS_CACHE_KEY_MAX + (size_t)separator)
		return false;
	memmove(result, root, length);
	if (separator)
		result[length++] = PATH_DELIM;
	memcpy(result + length, key, sizeof(key));
	return true;
}

bool
bbs_cache_glob_pattern(char *result, size_t size, const char *directory, const char *pattern)
{
	size_t used = 0;
	size_t length = strlen(pattern);

	for (const char *p = directory; *p; ++p) {
#ifndef _WIN32
		if (strchr("*?[]\\", *p) != NULL) {
			if (used >= size)
				return false;
			result[used++] = '\\';
		}
#endif
		if (used >= size)
			return false;
		result[used++] = *p;
	}
	if (used >= size || length >= size - used)
		return false;
	memcpy(result + used, pattern, length + 1);
	return true;
}

bool
bbs_cache_is_directory(const char *path)
{
#ifdef _WIN32
	DWORD attributes = GetFileAttributesA(path);

	return attributes != INVALID_FILE_ATTRIBUTES &&
	    (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == FILE_ATTRIBUTE_DIRECTORY;
#else
	struct stat info;

	return lstat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool
bbs_cache_remove(const char *path)
{
	DIR *dir;
	struct dirent *entry;
	bool success = true;
#ifdef _WIN32
	DWORD attributes = GetFileAttributesA(path);

	if (attributes == INVALID_FILE_ATTRIBUTES) {
		DWORD error = GetLastError();

		return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
	}
	if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
		return (attributes & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryA(path) != 0 : DeleteFileA(path) != 0;
	if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
		return remove(path) == 0;
#else
	struct stat info;

	if (lstat(path, &info) != 0)
		return errno == ENOENT;
	if (!S_ISDIR(info.st_mode))
		return unlink(path) == 0;
#endif
	dir = opendir(path);
	if (dir == NULL)
		return false;
	while ((entry = readdir(dir)) != NULL) {
		char child[MAX_PATH + 1];
		size_t length = strlen(path);
		size_t component = strlen(entry->d_name);

		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
		if (length + component + 2 > sizeof(child)) {
			success = false;
			break;
		}
		memcpy(child, path, length);
		child[length++] = PATH_DELIM;
		memcpy(child + length, entry->d_name, component + 1);
		if (!bbs_cache_remove(child)) {
			success = false;
			break;
		}
	}
	if (closedir(dir) != 0)
		success = false;
	return success && rmdir(path) == 0;
}

void
bbs_cache_sweep(const char *root, struct bbslist **list, size_t listcount)
{
	char cache_root[MAX_PATH + 1];
	char          entry_path[MAX_PATH + 1];
	DIR          *dir;
	struct dirent *ent;
	bool          found;

	if (root == NULL || *root == 0 || strlen(root) + 1 >= sizeof(cache_root))
		return;
	strcpy(cache_root, root);
	backslash(cache_root);
	dir = opendir(cache_root);
	if (dir == NULL)
		return;
	while ((ent = readdir(dir)) != NULL) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
			continue;
		if (!bbs_cache_key_recognized(ent->d_name))
			continue;
		if (strlen(cache_root) + strlen(ent->d_name) >= sizeof(entry_path))
			continue;
		strcpy(entry_path, cache_root);
		strcat(entry_path, ent->d_name);
		if (!bbs_cache_is_directory(entry_path))
			continue;
		found = false;
		for (size_t i = 0; i < listcount; i++) {
			char key[BBS_CACHE_KEY_MAX + 1];
			if (list[i] == NULL)
				continue;
			/* A failed key calculation must not delete a live cache. */
			if (!bbs_cache_key(key, sizeof(key), list[i]->name)) {
				closedir(dir);
				return;
			}
			if (strcmp(key, ent->d_name) == 0) {
				found = true;
				break;
			}
		}
		if (!found)
			bbs_cache_remove(entry_path);
	}
	closedir(dir);
}
