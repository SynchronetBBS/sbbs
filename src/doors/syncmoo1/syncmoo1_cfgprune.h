/* syncmoo1_cfgprune.h -- pure line-diff over 1oom config files.
 *
 * 1oom's cfg_save() writes EVERY registered item, so a user's config pins a
 * value they never chose and a later sysop default can never reach them. We
 * prune it back to a pure diff against a base snapshot. Pure and I/O-free, so
 * tests/test_cfgprune.c can link it with no engine and no termgfx.
 *
 * Design: docs/superpowers/specs/2026-07-09-syncmoo1-music-design.md Sec 6.2
 */
#ifndef SYNCMOO1_CFGPRUNE_H_
#define SYNCMOO1_CFGPRUNE_H_

#include <stddef.h>

/* Extract the "<module>.<item>" key of a 1oom cfg line into `out`. Returns 1
 * when the line is a key=value assignment, 0 for a blank line, a comment, or
 * anything without a '='. Leading/trailing space around the key is stripped. */
int sm_cfg_key(const char *line, char *out, size_t outlen);

/* Copy `user` into a fresh malloc'd buffer at *out (NUL-terminated, length in
 * *outlen, caller frees), dropping every assignment line whose key also
 * appears in `base` WITH THE SAME VALUE. Comments, blank lines, malformed
 * lines, and keys absent from `base` are all preserved verbatim. Values are
 * compared with surrounding whitespace stripped, so "k=1" and "k = 1" match.
 *
 * `drop_keys` is an optional NULL-terminated array of keys to drop
 * UNCONDITIONALLY, whatever their value. It exists for settings the door owns
 * and re-derives every run, which have no business being persisted per user --
 * above all `opt.data_path`, which 1oom would otherwise save as an ABSOLUTE
 * path, breaking every player's config the day the sysop moves the BBS tree.
 * Pass NULL for none.
 *
 * Returns 0 on success, -1 on allocation failure (then *out is NULL). */
int sm_cfg_prune(const char *base, const char *user,
                 const char *const *drop_keys, char **out, size_t *outlen);

#endif /* SYNCMOO1_CFGPRUNE_H_ */
