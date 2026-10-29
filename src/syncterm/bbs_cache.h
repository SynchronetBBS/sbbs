/* Copyright (c) 2026 Stephen Hurd; SPDX-License-Identifier: BSD-2-Clause */
#ifndef BBS_CACHE_H
#define BBS_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#define BBS_CACHE_KEY_MAX 70

/* CP437 input; same Unicode 18/SHA-256 keys as equivalent names in v2. */
bool bbs_cache_key(char *result, size_t size, const char *name);
bool bbs_cache_key_recognized(const char *key);
/* Produces a literal path without a trailing separator; root may be result. */
bool bbs_cache_path(char *result, size_t size, const char *root, const char *name);
/* Quote the literal directory before appending a cache-list glob pattern. */
bool bbs_cache_glob_pattern(char *result, size_t size, const char *directory, const char *pattern);
bool bbs_cache_is_directory(const char *path);
/* Remove a literal cache tree, without following symlinks/reparse points. */
bool bbs_cache_remove(const char *path);

struct bbslist;
/* Only recognized v2 keys are eligible for orphan cleanup. */
void bbs_cache_sweep(const char *root, struct bbslist **list, size_t listcount);

#endif
