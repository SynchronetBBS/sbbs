# DeuceSSH — Drepper DSO Best Practices Audit

Audit of the shared library build against Ulrich Drepper's "How To
Write Shared Libraries" (2002), updated for modern toolchains.

## 1. Export Control — EXCELLENT

All functions are properly annotated:
- `DSSH_PUBLIC` (`visibility("default")`) for public API (~65 symbols)
- `DSSH_PRIVATE` (`visibility("hidden")`) for library-internal
- `static` for file-scope

No unmarked non-static symbols in any library source file.
`-fvisibility=hidden` is set on the shared library target, so any
unannotated symbol defaults to hidden.

### Global variables

Only two file-scope globals exist:
- `gconf` (`ssh-trans.c`) — zero-initialized (BSS), hidden in
  production.  No public access — all manipulation through
  setter/registration functions.
- `sw_ver` (`ssh-trans.c`) — `static const char[]`, embedded
  directly in `.rodata` (zero relocations).

No exported variables.  All state is per-instance behind opaque
`dssh_session` and `dssh_channel` handles.

## 2. Compiler/Linker Flags

| Flag | Status | Notes |
|------|--------|-------|
| `-fPIC` | Yes | Via `POSITION_INDEPENDENT_CODE ON` |
| `-fvisibility=hidden` | Yes | Shared library target only |
| `-fno-semantic-interposition` | Yes | Allows inlining across default-vis defs |
| `-Wl,-z,relro,-z,now` | Yes | Full RELRO + eager binding |
| `-Wl,--hash-style=gnu` | Yes | `.gnu.hash` for ~50% faster lookup |
| `-Wl,-Bsymbolic-functions` | Yes | Local definition preference |
| `-Wl,-O2` | Yes | String merging + tail optimization |
| Version script | No | Should be added at 1.0 for ABI stability |

ELF linker flags applied on `UNIX AND NOT APPLE` (macOS uses Mach-O).
Separate static and shared library targets.

## 3. Data Layout — EXCELLENT

- No `char *str = "literal"` at file scope (would need relocation)
- No static arrays of function pointers (each element = relocation)
- No static arrays of data pointers
- Algorithm registration is dynamic (malloc + fill at runtime), not
  static initializer tables — avoids load-time relocations entirely
- All string constants are compile-time embedded in `.rodata`
- `gconf` is zero-initialized (BSS) — no file storage, no relocations
  until the application calls setter functions

## 4. Symbol Naming — EXCELLENT

All exported symbols use a consistent `dssh_` prefix:
- Core API: `dssh_session_*`, `dssh_transport_*`, `dssh_auth_*`, etc.
- Algorithm registration: `dssh_register_*`
- Key management: `dssh_ed25519_*`, `dssh_rsa_sha2_256_*`
- Error codes: `DSSH_ERROR_*`
- Flags: `DSSH_*`

## 5. Text Relocations — NONE

- All code compiled with `-fPIC`
- No inline assembly with absolute addresses
- No non-PIC object files linked into the shared library

Verify: `readelf -d libdeucessh.so | grep TEXTREL` (no output)

## 6. Dependency Isolation — EXCELLENT

- OpenSSL linked `PRIVATE` (not propagated to consumers)
- C11 threads linked via `INTERFACE` on the static lib only
- No OpenSSL types in public headers
- No `<threads.h>` or `<stdatomic.h>` in public headers

## 7. Summary

| Criterion | Status |
|-----------|--------|
| Export control (visibility) | EXCELLENT |
| No exported variables | EXCELLENT |
| Compiler flags (PIC, visibility, interposition) | COMPLETE |
| Linker hardening (RELRO, gnu hash, Bsymbolic) | COMPLETE |
| Version script | MISSING (pre-1.0) |
| Data layout (no pointer arrays) | EXCELLENT |
| String constants in rodata | EXCELLENT |
| Zero-init globals (BSS) | EXCELLENT |
| Symbol naming (`dssh_` prefix) | EXCELLENT |
| Text relocations | NONE |
| Dependency isolation | EXCELLENT |

## Remaining

1. Create linker version script at 1.0 release for ABI stability
