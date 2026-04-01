# RULES.md Audit — Library Core Code

Audit of `ssh.c`, `ssh-trans.c`, `ssh-auth.c`, `ssh-conn.c`,
`ssh-internal.h`, `ssh-trans.h`, and all public `deucessh-*.h` headers
against the 12 rules in `RULES.md`.

Legend: **CONFORMS** = no violations found. **FINDING** = violation or
gap requiring attention.

---

## 1. Input Validation

**CONFORMS** with exceptions noted below.

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

### FINDING 1.1 — `send_info_request`: missing UINT32\_MAX check on server KBI strings

`ssh-auth.c:213–254` — `name_len`, `inst_len`, and per-prompt `plen`
are `size_t` values from `strlen()`, cast to `uint32_t` via
`DSSH_PUT_U32((uint32_t)name_len, ...)` without a prior range check.
On 64-bit platforms where the server application's KBI callback returns
a string longer than 4 GiB, the cast silently truncates.

The overflow check on line 222 (`msg_len > SIZE_MAX - 5 - plen`)
guards against `size_t` overflow but does not catch individual fields
exceeding `UINT32_MAX`.

**Fix**: Add `#if SIZE_MAX > UINT32_MAX` guards checking each
`strlen()` result against `UINT32_MAX` before the narrowing cast, as
is already done in `flush_pending_banner` (line 326).

### FINDING 1.2 — Server KBI: no bound on remote `num_responses`

`ssh-auth.c:625–638` — In the server-side KBI handler, `num_responses`
is a `uint32_t` read directly from the client's INFO\_RESPONSE packet.
No check enforces `num_responses <= num_prompts` (or any other bound)
before `calloc(num_responses, ...)`.  A malicious client can claim
billions of responses, causing a multi-GiB allocation attempt.

See also **Finding 6.1** (Memory Allocation).

### FINDING 1.3 — Client KBI: no bound on remote `num_prompts`

`ssh-auth.c:1432–1448` — Client-side KBI reads `num_prompts` from the
server's INFO\_REQUEST and immediately `calloc`s five arrays of that
size.  A malicious server can trigger a multi-GiB allocation.

See also **Finding 6.2** (Memory Allocation).

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

**CONFORMS** with one exception.

Overflow checks are present before every `size_t` addition used as a
malloc size.  Saturating adds are used for cumulative byte counters
(`tx_bytes_since_rekey`, `rx_bytes_since_rekey`, `bytebuf.total`) and
window arithmetic (`window_add`, `window_atomic_add`).

`register_channel` correctly checks `channel_capacity > SIZE_MAX / 2`
before doubling.

### FINDING 3.1 — `event_queue_push`: unchecked capacity doubling

`ssh-conn.c:219` — `size_t new_cap = q->capacity * 2;` can overflow
to a small value if `capacity` approaches `SIZE_MAX / 2`.  The
subsequent `calloc(new_cap, sizeof(...))` might succeed with a too-small
buffer, and the copy loop would write past the allocation.

In practice, the default `max_events = 64` cap prevents unbounded
growth, and `max_events = 0` (uncapped) would require petabytes of
prior allocations to reach the overflow.  Nevertheless, the rule states
"Only IMPOSSIBLE over/underflow may be unchecked; UNLIKELY over/underflow
MUST be checked."

**Fix**: Add `if (q->capacity > SIZE_MAX / 2) return DSSH_ERROR_ALLOC;`
before the multiplication.

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

**CONFORMS.**

- Application-provided callback data pointers are never freed by the
  library.
- Cross-boundary ownership transfers are documented and consistent:
  the application `malloc`s KBI responses, password-change prompts,
  and server KBI prompt arrays; the library `free`s them after use.
  The API contracts in `deucessh-auth.h` explicitly state this for
  each callback typedef.
- `dssh_chan_params` copies all strings via `strdup`; the library owns
  the copies and frees them in `dssh_chan_params_free`.
- `dssh_auth_set_banner` copies the message via `strdup`; the library
  owns and frees the copy.

---

## 6. Memory Allocation

**CONFORMS** with exceptions noted below.

Packet buffers are bounded by `packet_buf_sz` (clamped to 33 KiB –
64 MiB).  The accept queue uses a fixed-capacity ring buffer
(`DSSH_ACCEPT_QUEUE_CAP = 8`).  Event queues have a configurable cap
(default 64).  Algorithm name-lists are capped at 1024 bytes.  Version
strings are capped at 255 bytes.

### FINDING 6.1 — Server KBI: unbounded allocation from remote `num_responses`

`ssh-auth.c:628–638` — `num_responses` is read from the wire with no
upper bound.  `calloc(num_responses, sizeof(*responses))` with
`num_responses = UINT32_MAX` requests ~32 GiB on 64-bit.

**Fix**: Reject packets where `num_responses` exceeds the
`num_prompts` that were sent in the preceding INFO\_REQUEST (the
server knows this value), or impose a hard cap (e.g., 256).

### FINDING 6.2 — Client KBI: unbounded allocation from remote `num_prompts`

`ssh-auth.c:1432–1448` — `num_prompts` from the server's
INFO\_REQUEST drives five `calloc` calls with no cap.

**Fix**: Impose a hard cap (e.g., 256 prompts) with a protocol error
return if exceeded.

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

**CONFORMS** with exceptions noted below.

The codebase consistently uses initializer-based casts with range
checks.  `-Wconversion -Werror` is enforced.  `DSSH_STRLEN` provides
safe literal-to-`uint32_t` conversion.  `#if SIZE_MAX > UINT32_MAX`
guards protect narrowing paths.

### FINDING 10.1 — `send_to_slot`: unchecked narrowing to `uint8_t`

`ssh-trans.c:1029,1054` — `slot->payload_len = (uint8_t)payload_len;`
narrows `size_t` to `uint8_t` without a range check.  All current
callers pass values ≤ 16, but the function signature accepts any
`size_t`.

**Fix**: Add `if (payload_len > UINT8_MAX) return DSSH_ERROR_INVALID;`
at the top of `send_to_slot`.

### FINDING 10.2 — `send_info_request`: inline narrowing casts

`ssh-auth.c:238,243,254` — `(uint32_t)name_len`, `(uint32_t)inst_len`,
and `(uint32_t)plen` are inline narrowing casts in `DSSH_PUT_U32`
macro arguments without prior range checks.  (See also Finding 1.1.)

### FINDING 10.3 — `stream_zc_cb`: narrowing cast in return statement

`ssh-conn.c:2160` — `return (uint32_t)written;` narrows `size_t` in a
return statement rather than an initializer.  The value is bounded by
`window_max` (uint32\_t), so it always fits, but the style rule
requires initializer placement.

**Fix**: `uint32_t ret = (uint32_t)written; return ret;`

### FINDING 10.4 — `handle_channel_extended_data`: inline cast in argument

`ssh-conn.c:1182` — `cb(ch, (int)data_type, data, dlen, cbd)` has an
inline `(int)` cast of `uint32_t data_type` in a function argument.
`data_type` is always `SSH_EXTENDED_DATA_STDERR` (1), so it fits, but
the inline cast violates the style rule.

**Fix**: `int stream = (int)data_type;` then `cb(ch, stream, ...)`.

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

| Rule | Status | Findings |
|------|--------|----------|
| 1. Input Validation | CONFORMS with findings | 1.1, 1.2, 1.3 |
| 2. Portability | CONFORMS | — |
| 3. Arithmetic Safety | CONFORMS with finding | 3.1 |
| 4. Error Handling | CONFORMS | — |
| 5. Pointer Ownership | CONFORMS | — |
| 6. Memory Allocation | CONFORMS with findings | 6.1, 6.2 |
| 7. Memory Copies | CONFORMS | — |
| 8. No Library I/O | CONFORMS | — |
| 9. Opaque Public API | CONFORMS | — |
| 10. Type Safety | CONFORMS with findings | 10.1, 10.2, 10.3, 10.4 |
| 11. Thread Safety | CONFORMS | — |
| 12. No Deprecated Crypto | CONFORMS | — |

**Total findings: 9** (3 input validation, 1 arithmetic, 2 memory
allocation, 4 type safety).  Findings 1.2/6.1 and 1.3/6.2 overlap
(same issue, two rules).  **7 unique code locations** require fixes.
