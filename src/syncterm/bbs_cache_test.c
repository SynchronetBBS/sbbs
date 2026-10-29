/* Copyright (c) 2026 Stephen Hurd; SPDX-License-Identifier: BSD-2-Clause */

#include "bbs_cache.h"
#include "bbslist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define CHECK(e) do { if (!(e)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #e); \
	return 1; \
} } while (0)

static int
key_tests(void)
{
	/* The first six vectors are also fixed expectations in v2's cache test. */
	static const struct { const char *name; const char *digest; } vectors[] = {
		{"Normal Name", "f0f4db0c76b5d4f68d5c58b47d15de497777f1a7812044728507ee6457fb8ec6"},
		{"NORMAL NAME", "f0f4db0c76b5d4f68d5c58b47d15de497777f1a7812044728507ee6457fb8ec6"},
		{"../Outside", "62ca1d92c4a3fc44a5fa30d1ddc593be1a9945ca21c0821af53d4f2b604075e7"},
		{"CON", "1143da2bc54c495c4be31d3868785d39ffdfd56df5668f0645d8f14d47647952"},
		{"trail.", "cedd4b9ff43e695b31d896bebded15445818ddbec673abc6baa2e22661864b9d"},
		{"One\\Two", "11b8e8b4c13a882a72dd9b86344147d13b63f5bbef78e343e63699db5c46e28a"},
		{"Test*BBS", "6a031d7f31d5e11a60906b2884ef583d784050b41a8927efdf140b9e63146f8a"},
		{"..", "5ec1f7e700f37c3d0b2981d04855fc34b94aaa15457b05ca571817442d228f81"},
		{"trail ", "59413e908f3b72e4ce859bf153fffc8054615bc3e445f7ac5e7b2adb9b8bbe70"},
		{"Stra\xc3\x9f" "e", "16d96952087774fee069b7585d3991b24d90c181c09b2129b4908c35baa7f0c0"},
		{"\xc3\x85", "56bb553c77d36738947bd6a15549ade795b2c49047509632d90af5ff889cf66e"},
		{"A\xcc\x8a", "56bb553c77d36738947bd6a15549ade795b2c49047509632d90af5ff889cf66e"},
		{"\xc4\xb0", "fbbdb347060082cf4e72cdc5f02679dd809ebf4e6c601933e858d954a65f5e73"},
		{"\xea\xb0\x80", "a0d2271e0fe8929d025eb859cde49251ba70c86abb76123f03f8d8eda1c98939"},
		{"A\xcc\x95\xcc\x80", "918c0a9dec292c2a45d18cd248c2dae41091a23146b3874af24d6ee81c9d7ed1"},
	};
	char key[BBS_CACHE_KEY_MAX + 1];
	char expected[BBS_CACHE_KEY_MAX + 1];
	char path[256];

	for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
		snprintf(expected, sizeof(expected), "[u18-%s]", vectors[i].digest);
		CHECK(bbs_cache_key(key, sizeof(key), vectors[i].name));
		CHECK(strcmp(key, expected) == 0);
		CHECK(bbs_cache_key_recognized(key));
		CHECK(bbs_cache_path(path, sizeof(path), "cache", vectors[i].name));
		CHECK(strcmp(path + 6, key) == 0);
	}
	CHECK(!bbs_cache_key(key, sizeof(key) - 1, "Normal Name"));
	CHECK(!bbs_cache_key(key, sizeof(key), ""));
	CHECK(!bbs_cache_key(key, sizeof(key), "\xc0\x80"));
	CHECK(!bbs_cache_key(key, sizeof(key), "\xed\xa0\x80"));
	CHECK(!bbs_cache_key(key, sizeof(key), "\xf4\x90\x80\x80"));
	CHECK(!bbs_cache_key_recognized("Normal Name"));
	CHECK(!bbs_cache_key_recognized("[636f6e]"));
	CHECK(!bbs_cache_key_recognized("syncterm-system-cache"));
	CHECK(!bbs_cache_key_recognized("[u18-F0f4db0c76b5d4f68d5c58b47d15de497777f1a7812044728507ee6457fb8ec6]"));
	memset(path, 'x', sizeof(path));
	CHECK(!bbs_cache_path(path, 76, "cache", "Normal Name"));
	CHECK(path[0] == 'x');
	CHECK(bbs_cache_path(path, 77, "cache", "Normal Name"));
	CHECK(strlen(path) == 76);
	CHECK(!bbs_cache_path(path, sizeof(path), "", "Normal Name"));
	strcpy(path, "cache");
	CHECK(bbs_cache_path(path, sizeof(path), path, "Normal Name"));
	CHECK(strlen(path) == 76);
	return 0;
}

static bool
write_text(const char *path)
{
	FILE *file = fopen(path, "wb");
	if (file == NULL)
		return false;
	bool success = fputs("keep", file) >= 0;
	return fclose(file) == 0 && success;
}

static int
lifecycle_tests(void)
{
	const char *root = BBS_CACHE_TEST_DIRECTORY;
	char cache[MAX_PATH + 1];
	char renamed[MAX_PATH + 1];
	char orphan[MAX_PATH + 1];
	char file[MAX_PATH + 1];
	char outside[MAX_PATH + 1];

#ifndef _WIN32
	char link[MAX_PATH + 1];
#endif
	char pattern[MAX_PATH * 2 + 1];
	struct bbslist bbs = {0};
	struct bbslist *list[] = {NULL, &bbs};
	static const char *const unusual_names[] = {
		"Test*BBS", "..", ".", "../Outside", "/", "One/Two", "One\\Two",
		"NUL", "CON", "AUX", "COM1", "trail.", "trail ", ":?*\"<>|",
	};

	CHECK(bbs_cache_remove(root));
	CHECK(mkpath(root) == 0);
	for (size_t i = 0; i < sizeof(unusual_names) / sizeof(unusual_names[0]); ++i) {
		CHECK(bbs_cache_path(cache, sizeof(cache), root, unusual_names[i]));
		CHECK(mkpath(cache) == 0);
		CHECK(bbs_cache_is_directory(cache));
		CHECK(snprintf(file, sizeof(file), "%s/audio.wav", cache) < (int)sizeof(file));
		CHECK(write_text(file));
		CHECK(snprintf(pattern, sizeof(pattern), "%s%c", cache, PATH_DELIM) < (int)sizeof(pattern));
		strcpy(file, pattern);
		CHECK(bbs_cache_glob_pattern(pattern, sizeof(pattern), file, "*.wav"));
		glob_t matches;
		CHECK(glob(pattern, 0, NULL, &matches) == 0);
		CHECK(matches.gl_pathc == 1);
		CHECK(strcmp(getfname(matches.gl_pathv[0]), "audio.wav") == 0);
		globfree(&matches);
		CHECK(snprintf(file, sizeof(file), "%s/audio.wav", cache) < (int)sizeof(file));
		strcpy(bbs.name, unusual_names[i]);
		bbs_cache_sweep(root, list, 2);
		CHECK(fexist(file));
		CHECK(bbs_cache_remove(cache));
		CHECK(!isdir(cache));
	}

	CHECK(bbs_cache_path(cache, sizeof(cache), root, "Old Name"));
	CHECK(bbs_cache_path(renamed, sizeof(renamed), root, "New/Name"));
	CHECK(mkpath(cache) == 0);
	CHECK(snprintf(file, sizeof(file), "%s/icon.rip", cache) < (int)sizeof(file));
	CHECK(write_text(file));
	CHECK(rename(cache, renamed) == 0);
	CHECK(snprintf(file, sizeof(file), "%s/icon.rip", renamed) < (int)sizeof(file));
	CHECK(fexist(file));
	strcpy(bbs.name, "NEW/NAME");
	CHECK(bbs_cache_path(cache, sizeof(cache), root, bbs.name));
	CHECK(strcmp(cache, renamed) == 0);

	CHECK(bbs_cache_path(orphan, sizeof(orphan), root, "Orphan"));
	CHECK(mkpath(orphan) == 0);
	CHECK(snprintf(file, sizeof(file), "%s/[icon]*.rip", orphan) < (int)sizeof(file));
#ifndef _WIN32
	CHECK(write_text(file));
#endif
	CHECK(snprintf(outside, sizeof(outside), "%s/legacy", root) < (int)sizeof(outside));
	CHECK(mkpath(outside) == 0);
	CHECK(snprintf(file, sizeof(file), "%s/keep", outside) < (int)sizeof(file));
	CHECK(write_text(file));
#ifndef _WIN32
	/* A child symlink must be removed without touching its target. */
	CHECK(snprintf(link, sizeof(link), "%s/link", orphan) < (int)sizeof(link));
	CHECK(symlink(outside, link) == 0);
#endif
	bbs_cache_sweep(root, list, 2);
	CHECK(!isdir(orphan));
	CHECK(bbs_cache_is_directory(renamed));
	CHECK(fexist(file));
#ifndef _WIN32
	/* The sweep also ignores a recognized key that is a directory symlink. */
	CHECK(symlink(outside, orphan) == 0);
	bbs_cache_sweep(root, list, 2);
	CHECK(isdir(orphan));
	CHECK(!bbs_cache_is_directory(orphan));
	CHECK(bbs_cache_remove(orphan));
	CHECK(fexist(file));
#endif
	CHECK(snprintf(cache, sizeof(cache), "%s/syncterm-system-cache", root) < (int)sizeof(cache));
	CHECK(mkpath(cache) == 0);
	bbs_cache_sweep(root, NULL, 0);
	CHECK(!isdir(renamed));
	CHECK(isdir(cache));
	CHECK(fexist(file));
	CHECK(bbs_cache_remove(root));
	return 0;
}

int
main(void)
{
	CHECK(key_tests() == 0);
	CHECK(lifecycle_tests() == 0);
	return 0;
}
