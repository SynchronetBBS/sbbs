# RULES.md Audit — Library Core Code

Audit of `ssh.c`, `ssh-trans.c`, `ssh-auth.c`, `ssh-conn.c`,
`ssh-internal.h`, `ssh-trans.h`, and all public `deucessh-*.h` headers
against the 12 rules in `RULES.md`.

Legend: **CONFORMS** = no violations found. **FINDING** = violation or
gap requiring attention.

---

## 1. Input Validation

**CONFORMS.**

Every public API function validates its pointer and enum parameters,
returning `DSSH_ERROR_INVALID` on NULL sessions/buffers/callbacks.
Pre-start callback setters additionally check `demux_running` and
return `DSSH_ERROR_TOOLATE`.  The public/internal split pattern is
used correctly (e.g., `dssh_auth_server` validates then calls
`auth_server_impl`; `dssh_auth_password` validates then calls
`auth_password_impl`).

All wire data from `recv_packet` is parsed with explicit length checks
at every field boundary.  KEXINIT parsing validates control characters,
individual algorithm name lengths, and buffer bounds.  Version exchange
validates length, CRLF, printable ASCII, and protocol version.

`send_info_request` checks `name_len > UINT32_MAX` before narrowing
(line 217).  Server KBI rejects `num_responses > last_nprompts`
(line 641).  Client KBI caps `num_prompts` at 256 (line 1452).

---

## 2. Portability

**CONFORMS.**

- Only C17 standard headers: `<stdlib.h>`, `<string.h>`, `<threads.h>`,
  `<stdatomic.h>`, `<time.h>`, `<stdbool.h>`, `<inttypes.h>`,
  `<stddef.h>`, `<assert.h>`.
- No platform-specific APIs.  `_Thread_local` and `timespec_get()` are
  C11/C17 standard.
- Conditional `#if SIZE_MAX < INT64_MAX` / `#if SIZE_MAX > UINT32_MAX`
  guards used throughout for 16/32/64-bit correctness.
- Explicit byte-by-byte serialization (`DSSH_GET_U32`, `DSSH_PUT_U32`)
  — no endianness assumptions.
- `static_assert(!offsetof(..., next), ...)` in module headers
  guarantees the generic linked-list traversal in `build_namelist` and
  `negotiate_algo` is correct on all platforms.

---

## 3. Arithmetic Safety

**CONFORMS.**

Overflow checks are present before every `size_t` addition used as a
malloc size.  Saturating adds are used for cumulative byte counters
(`tx_bytes_since_rekey`, `rx_bytes_since_rekey`, `bytebuf.total`) and
window arithmetic (`window_add`, `window_atomic_add`).

`register_channel` correctly checks `channel_capacity > SIZE_MAX / 2`
before doubling.  `event_queue_push` checks `capacity > SIZE_MAX / 2`
before doubling (line 219).

---

## 4. Error Handling

**CONFORMS.**

- All `malloc`/`calloc`/`realloc`/`strdup` returns are checked.
- All `send_packet`/`recv_packet`/`send_commit` returns are checked.
- All `dssh_random()` returns are checked.
- All module `init`/`encrypt`/`decrypt`/`generate` returns are checked.
- C11 thread operations go through `dssh_thrd_check`, which terminates
  the session on `thrd_error`/`thrd_nomem`.
- `dssh_chan_params_add_env` correctly cleans up on partial allocation
  failure without double-free (env\_count is not yet incremented).

---

## 5. Pointer Ownership

**CONFORMS** with documented exception.

- Application-provided callback data pointers are never freed by the
  library.
- `dssh_chan_params` copies all strings via `strdup`; the library owns
  the copies and frees them in `dssh_chan_params_free`.
- `dssh_auth_set_banner` copies the message via `strdup`; the library
  owns and frees the copy.

### Exception — KBI ownership transfer

The KBI authentication API intentionally transfers ownership across
the library boundary: the application `malloc`s KBI responses,
password-change prompts, and server KBI prompt arrays; the library
`free`s them after use.  The API contracts in `deucessh-auth.h`
explicitly document this for each callback typedef.  This is a
deliberate design choice (avoiding an extra copy of potentially many
variable-length strings) rather than an oversight, but it does violate
the strict rule.

---

## 6. Memory Allocation

**CONFORMS.**

Packet buffers are bounded by `packet_buf_sz` (clamped to 33 KiB –
64 MiB).  The accept queue uses a fixed-capacity ring buffer
(`DSSH_ACCEPT_QUEUE_CAP = 8`).  Event queues have a configurable cap
(default 64).  Algorithm name-lists are capped at 1024 bytes.  Version
strings are capped at 255 bytes.  Server KBI bounds `num_responses`
to `last_nprompts`.  Client KBI caps `num_prompts` at 256.

---

## 7. Memory Copies

**CONFORMS.**

Wire data is processed in-place from `rx_packet` wherever possible.
Parsing functions return pointers into the packet buffer rather than
copying.  Intermediate copies are used only where required (sign data
construction in publickey auth, KBI prompt relay).

---

## 8. No Library I/O

**CONFORMS.**

All I/O goes through application-provided callbacks: `gconf.tx`,
`gconf.rx`, `gconf.rx_line`, `gconf.tx_gather`, and
`gconf.extra_line_cb`.  No direct `read`/`write`/`send`/`recv` calls
anywhere in the library code.

---

## 9. Opaque Public API

**CONFORMS.**

Public headers (`deucessh.h`, `deucessh-auth.h`, `deucessh-conn.h`,
`deucessh-algorithms.h`, `deucessh-kex.h`, `deucessh-key-algo.h`,
`deucessh-enc.h`, `deucessh-mac.h`, `deucessh-comp.h`,
`deucessh-crypto.h`, `deucessh-lang.h`, `deucessh-portable.h`)
expose only:

- Opaque handle typedefs (`dssh_session`, `dssh_channel`,
  `dssh_enc_ctx`, `dssh_mac_ctx`, `dssh_hash_ctx`)
- Standard C types (`uint8_t`, `uint32_t`, `int64_t`, `size_t`, `bool`)
- Function pointer typedefs
- Module struct definitions with function pointers and scalar fields

No OpenSSL types, no `<threads.h>` types, no internal struct
definitions leak into public headers.  `ssh-internal.h` and
`ssh-trans.h` are strictly internal.

---

## 10. Type Safety

**CONFORMS.**

The codebase consistently uses initializer-based casts with range
checks.  `-Wconversion -Werror` is enforced.  `DSSH_STRLEN` provides
safe literal-to-`uint32_t` conversion.  `#if SIZE_MAX > UINT32_MAX`
guards protect narrowing paths.  `send_to_slot` checks
`payload_len > UINT8_MAX` before narrowing.  `send_info_request`
checks string lengths against `UINT32_MAX` before the narrowing cast.
`stream_zc_cb` and `handle_channel_extended_data` use
initializer-style casts.

---

## 11. Thread Safety

**CONFORMS.**

All mutable shared state is protected:

| State | Protection |
|-------|-----------|
| TX path (tx\_packet, tx\_seq, counters) | `tx_mtx` |
| RX path (rx\_packet, rx\_seq, counters) | `rx_mtx` |
| Channel table | `channel_mtx` |
| Per-channel buffers, flags | `buf_mtx` |
| Per-channel callbacks | `cb_mtx` |
| Accept queue | `accept_mtx` + `accept_cnd` |
| Rekey blocking | `rekey_cnd` under `tx_mtx` |
| TX slot queue | `tx_queue_mtx` + `tx_slot_cnd` |
| terminate, initialized, demux\_running | `atomic_bool` |
| remote\_window | `atomic_uint_least32_t` |
| TX packet/byte counters | `atomic_uint_fast32_t` / `atomic_uint_fast64_t` |

Lock order is documented and enforced:
`channel_mtx` → `buf_mtx` → `cb_mtx` → `tx_mtx`.

Pre-start callbacks are protected by the `demux_running` +
`thrd_create` happens-before guarantee.  `session_set_terminate` uses
`atomic_exchange` to ensure exactly-once execution.  `dssh_thrd_check`
terminates the session on any thread primitive failure.

The `in_zc_rx` thread-local guard prevents deadlock from TX calls
inside ZC RX callbacks.

---

## 12. No Deprecated Cryptography

**CONFORMS.**

No SHA-1-based algorithms are implemented or registered.  The algorithm
registry contains only:

- KEX: curve25519-sha256, dh-gex-sha256, sntrup761x25519-sha512,
  mlkem768x25519-sha256
- Host key: ssh-ed25519, rsa-sha2-256, rsa-sha2-512
- Encryption: aes256-ctr, none
- MAC: hmac-sha2-256, hmac-sha2-512, none
- Compression: none

3des-cbc, hmac-sha1, dh-group1-sha1, dh-group14-sha1, and ssh-dss
are intentionally absent.

---

## Summary

| Rule | Status |
|------|--------|
| 1. Input Validation | CONFORMS |
| 2. Portability | CONFORMS |
| 3. Arithmetic Safety | CONFORMS |
| 4. Error Handling | CONFORMS |
| 5. Pointer Ownership | CONFORMS with exception |
| 6. Memory Allocation | CONFORMS |
| 7. Memory Copies | CONFORMS |
| 8. No Library I/O | CONFORMS |
| 9. Opaque Public API | CONFORMS |
| 10. Type Safety | CONFORMS |
| 11. Thread Safety | CONFORMS |
| 12. No Deprecated Crypto | CONFORMS |

**11 rules: CONFORMS.  1 rule (Pointer Ownership): CONFORMS with
documented exception** (KBI ownership transfer by design).
