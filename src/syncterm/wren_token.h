/*
 * Picker-sourced File consent tokens for the Wren scripting host.
 *
 * Wren scripts must not synthesize File foreigns from arbitrary
 * paths — that's the whole point of the Cache / Upload / Download
 * sandbox roots and the relaxed-name predicate.  The escape hatch
 * is Host.pickFile / Host.pickFiles, where the UIFC widget itself
 * is the user's signal of consent.
 *
 * To let queued picker-sourced uploads survive a disconnect, we
 * mint a capability token at consent time: an HMAC-SHA256
 * binding (path, file content SHA-256) that Wren can persist
 * freely (it's opaque) but only the C side can mint or verify.
 * At resume time, Host.openLocalFile(token) verifies the HMAC
 * and the file's current SHA-256 still matches; mismatch (file
 * edited, deleted, or signing key rotated) → the token is
 * rejected and the user must re-pick to re-consent.
 *
 * SHA-256 (rather than xpdev's SHA-1) is chosen so the
 * file-content hash can route through DeuceSSH's hardware-
 * accelerated digest backend (Intel SHA-NI / ARM crypto
 * extensions, via OpenSSL or Botan) when available.  Picked
 * files can be large (multi-GB ISOs etc.); the accelerated path
 * is roughly 20× faster than the pure-C reference impl.
 * WITHOUT_DEUCESSH builds fall back to xpdev's pure-C SHA-256
 * (src/hash/sha256.c) and to xp_random for key generation.
 *
 * The signing key is a per-installation 32-byte random value
 * stored in the user's personal syncterm.lst (encrypted blob),
 * loaded only from USER_BBSLIST — shared / web lists must never
 * inject their own key.
 */

#ifndef SYNCTERM_WREN_TOKEN_H
#define SYNCTERM_WREN_TOKEN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Install the per-installation HMAC signing key.  Called from
 * bbslist.c's USER_BBSLIST load path after the encrypted INI
 * decode succeeds. */
void wren_token_set_key(const uint8_t key[32]);

/* Generate a fresh 32-byte signing key.  Returns true on success.
 * Returns false in WITHOUT_DEUCESSH builds (no random source) or
 * if the underlying RNG fails. */
bool wren_token_generate_key(uint8_t out[32]);

/* Hex-encode 32 bytes into 64 hex chars + NUL terminator.  out
 * must be ≥65 bytes.  Used to embed the key in syncterm.lst's
 * encrypted INI blob. */
void wren_token_key_to_hex(const uint8_t key[32], char out[65]);

/* Hex-decode exactly 64 chars (case-insensitive) into 32 bytes.
 * Returns true on success, false on bad chars or wrong length. */
bool wren_token_key_from_hex(const char *hex, uint8_t out[32]);

/* Wipe the in-memory key.  Called at VM shutdown / disconnect
 * so the key doesn't sit in memory across BBS-list reloads. */
void wren_token_clear_key(void);

/* True after wren_token_set_key, false before / after clear. */
bool wren_token_have_key(void);

/* Mint a base64-encoded consent token for `path`.  Computes the
 * file's SHA-1 (mmap-based, single pass) and binds it into the
 * token with HMAC-SHA1.  Returns a malloc'd NUL-terminated
 * string the caller frees, or NULL on:
 *   - no key set
 *   - file unreadable / unmappable
 *   - OOM
 *   - path NULL or pathologically long. */
char *wren_token_sign(const char *path);

/* Verify `token` and return the encoded path on success.  Steps:
 *   1. base64 decode
 *   2. extract path + stored SHA-1 + HMAC tag
 *   3. recompute HMAC-SHA1 with the installed key, constant-time
 *      compare against the stored tag
 *   4. open the file at the encoded path, recompute its SHA-1,
 *      constant-time compare against the stored SHA-1
 * Returns malloc'd NUL-terminated path bytes the caller frees,
 * or NULL on any failure (bad HMAC, malformed token, missing
 * file, content mismatch, OOM, no key set). */
char *wren_token_verify(const char *token);

#ifdef __cplusplus
}
#endif

#endif /* SYNCTERM_WREN_TOKEN_H */
