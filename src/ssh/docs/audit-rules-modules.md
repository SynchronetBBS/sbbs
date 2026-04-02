# RULES.md Audit — Algorithm Modules

Audit of all algorithm module code against the 12 rules in `RULES.md`.

Module code uses the public module headers (`deucessh-kex.h`,
`deucessh-key-algo.h`, `deucessh-enc.h`, `deucessh-mac.h`,
`deucessh-comp.h`, `deucessh-crypto.h`) but exposes no API to the
application beyond the registration struct.  Modules receive a
`dssh_kex_context` (KEX) or opaque context pointer (enc/mac) from the
library and interact only through those interfaces.

**Scope**: 19 source files across kex/, key_algo/, enc/, mac/, comp/.
Vendor code (sntrup761.c, libcrux_mlkem768_sha3.h) is noted but not
held to RULES.md — it follows its upstream project's conventions.

Legend: **CONFORMS** = no violations found.  **FINDING** = violation
or gap requiring attention.

---

## 1. Input Validation

**CONFORMS.**

KEX handlers validate `msg_type` after every `kctx->recv()` and
return `DSSH_ERROR_PARSE` on unexpected messages.  All handlers check
`ka` (host key algorithm pointer) and its callback function pointers
(`ka->sign`, `ka->verify`, `ka->pubkey`) for NULL before use.

Peer-supplied public keys are validated by size:
curve25519-sha256.c checks `Q_C` length equals `X25519_KEY_LEN`;
hybrid-pq-kex.c checks combined PQ+X25519 key length matches expected
constants; dh-gex-sha256.c validates mpint bounds within the received
payload.

---

## 2. Portability

**CONFORMS.**

All modules use only C17 standard functions.  No platform-specific
APIs.  mlkem768.c byte-swap macros handle big-endian platforms.
sntrup761.c abstracts all crypto through `deucessh-crypto.h`.
Botan backend modules use the Botan C FFI which is portable C.

---

## 3. Arithmetic Safety

**CONFORMS** with findings.

`dh-gex-sha256.c` correctly checks each `SIZE_MAX` bound before
accumulating `reply_sz` (lines 478–492) and checks `UINT32_MAX`
before narrowing (line 470).  This is the correct pattern.

### FINDING 3.1 — `curve25519-sha256.c`: unchecked reply\_sz overflow

Line 360 — Server-side reply construction:
```c
size_t reply_sz = 1 + 4 + k_s_len + 4 + X25519_KEY_LEN + 4 + sig_len;
```
computes the sum in one expression without overflow checks.  `k_s_len`
was checked against `UINT32_MAX` during hash computation (line 30),
but `sig_len` from `ka->sign()` is never range-checked.  On a 32-bit
platform where `k_s_len + sig_len` wraps, `malloc(reply_sz)` succeeds
with a too-small buffer.

**Fix**: Check each addend against `SIZE_MAX` before accumulating, as
`dh-gex-sha256.c:478–492` already does.

### FINDING 3.2 — `hybrid-pq-kex.c`: unchecked reply\_sz overflow

Line 474 — Same pattern as 3.1:
```c
size_t reply_sz = 1 + 4 + k_s_len + 4 + p->q_s_len + 4 + sig_len;
```

**Fix**: Same as 3.1.

---

## 4. Error Handling

**CONFORMS.**

All `kctx->send()`, `kctx->recv()`, `dssh_hash_init/update/final()`,
`dssh_random()`, `ka->sign()`, `ka->verify()`, `ka->pubkey()`, and
backend crypto calls check return values and propagate errors.
Cleanup paths use `goto cleanup` with NULL-safe `free()`.

---

## 5. Pointer Ownership

**CONFORMS.**

KEX handlers `malloc` shared\_secret and exchange\_hash; ownership
transfers to `kctx` output fields (documented contract).  DH group
provider allocates p\_bytes/g\_bytes; the handler frees them after
use.  All temporary buffers freed before return.  Key algorithm
modules use stack buffers or caller-provided output pointers — no
cross-boundary allocation issues.

---

## 6. Memory Allocation

**CONFORMS.**

Packet-layer parsing bounds all recv'd payload sizes to
`packet_buf_sz`.  DH-GEX group allocations are bounded by the
received payload length (which is packet-bounded).  sntrup761 and
mlkem768 use compile-time constant sizes for all key/ciphertext
buffers.  Enc/MAC/comp modules do not allocate.

Note: `reply_sz` in curve25519-sha256.c and hybrid-pq-kex.c is
computed from `k_s_len` (host key blob — bounded by key algorithm)
and `sig_len` (signature — bounded by key algorithm).  These are not
directly peer-controlled, but the lack of overflow checks (Finding
3.1, 3.2) means a buggy key algorithm module could trigger an issue.

---

## 7. Memory Copies

**CONFORMS.**

Peer KEXINIT payloads referenced directly from the recv buffer.
Copies made only where required: exchange hash inputs, DH group
mpints (must survive next recv), reply message construction.
Hybrid-PQ modules efficiently append PQ and X25519 public keys.

---

## 8. No Library I/O

**CONFORMS.**

All I/O through `kctx->send()` and `kctx->recv()` callbacks.
Randomness through `dssh_random()`.  No direct I/O calls anywhere.

---

## 9. Opaque Public API

**CONFORMS.**

Public module headers expose only registration structs with function
pointers and scalar fields.  No Botan or OpenSSL types leak through.
Internal ops headers (`curve25519-sha256-ops.h`, etc.) are used only
by backend implementations.

---

## 10. Type Safety

**CONFORMS** with findings.

`dh-gex-sha256.c` uses the correct pattern: check `UINT32_MAX`
before narrowing, cast in initializer (lines 470, 501–502).
`compute_exchange_hash` functions check all `size_t` parameters
against `UINT32_MAX` at entry.

### FINDING 10.1 — `curve25519-sha256.c`: unchecked narrowing of sig\_len

Lines 373, 393 — `(uint32_t)k_s_len` and `(uint32_t)sig_len` cast
inline in `dssh_serialize_uint32()` arguments.  `k_s_len` was checked
in `compute_exchange_hash` (line 30) but `sig_len` is never checked
against `UINT32_MAX`.

**Fix**: Add `if (sig_len > UINT32_MAX)` check before the cast, and
move to initializer style.

### FINDING 10.2 — `hybrid-pq-kex.c`: unchecked narrowing in REPLY\_SER

Lines 495, 498, 501 — The `REPLY_SER` macro passes
`(uint32_t)k_s_len`, `(uint32_t)p->q_s_len`, `(uint32_t)sig_len`
without prior `UINT32_MAX` checks.  `k_s_len` was checked in
`compute_exchange_hash` (line 31); `p->q_s_len` is a compile-time
constant; `sig_len` is not checked.

**Fix**: Check `sig_len > UINT32_MAX` before the cast.

---

## 11. Thread Safety

**CONFORMS.**

KEX modules are stateless or store per-session state in `kex_data`
(freed by cleanup callback).  No global mutable state.  Key algorithm
modules store per-registration context in `ka->ctx`.  Enc/MAC modules
store per-session context in opaque handles.  No C11 thread primitives
in module code.

---

## 12. No Deprecated Cryptography

**CONFORMS.**

KEX: curve25519-sha256, dh-gex-sha256, sntrup761x25519-sha512,
mlkem768x25519-sha256 — all SHA-256 or SHA-512.
Host key: ssh-ed25519, rsa-sha2-256, rsa-sha2-512.
MAC: hmac-sha2-256, hmac-sha2-512.
No SHA-1, 3DES, or DSA anywhere.

---

## Vendor Code

**sntrup761.c** (SUPERCOP, public domain) — Uses `dssh_random()` and
`dssh_hash_oneshot()` for backend-neutral crypto.  Error codes
propagated.  Not audited against RULES.md (upstream conventions).

**libcrux\_mlkem768\_sha3.h** (Cryspen, MIT) — Self-contained SHA-3/
SHAKE implementation.  Only `dssh_random()` goes through DeuceSSH
(via mlkem768.c wrapper).  Not audited against RULES.md.

---

## Summary

| Rule | Status |
|------|--------|
| 1. Input Validation | CONFORMS |
| 2. Portability | CONFORMS |
| 3. Arithmetic Safety | CONFORMS with findings |
| 4. Error Handling | CONFORMS |
| 5. Pointer Ownership | CONFORMS |
| 6. Memory Allocation | CONFORMS |
| 7. Memory Copies | CONFORMS |
| 8. No Library I/O | CONFORMS |
| 9. Opaque Public API | CONFORMS |
| 10. Type Safety | CONFORMS with findings |
| 11. Thread Safety | CONFORMS |
| 12. No Deprecated Crypto | CONFORMS |

**4 findings in 2 files** (curve25519-sha256.c and hybrid-pq-kex.c).
All are the same class of bug: missing overflow/range checks before
`reply_sz` computation and `uint32_t` narrowing in server-side KEX
reply construction.  `dh-gex-sha256.c` has the correct pattern to
follow.
