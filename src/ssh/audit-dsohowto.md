# DeuceSSH — Drepper DSO Best Practices Audit

Audit of the shared library build against Ulrich Drepper's "How To
Write Shared Libraries" (2002), updated for modern toolchains.

## 1. Export Control — EXCELLENT

All functions are properly annotated:
- `DSSH_PUBLIC` (`visibility("default")`) for public API (~65 symbols)
- `DSSH_PRIVATE` (`visibility("hidden")`) for library-internal
- `static` for file-scope

No unmarked non-static symbols found in any library source file.
`-fvisibility=hidden` is set on the shared library target in CMake,
so any unannotated symbol defaults to hidden.

### Global variables

Only two file-scope globals exist:
- `gconf` (`ssh-trans.c`) — zero-initialized (BSS), `DSSH_TESTABLE`
  (hidden in production).  No public access — all manipulation is
  through setter/registration functions.
- `sw_ver` (`ssh-trans.c`) — `static const char * const`

No exported variables.  All state is per-instance behind opaque
`dssh_session` and `dssh_channel` handles.

## 2. Compiler/Linker Flags

### Implemented
- `-fPIC` via `POSITION_INDEPENDENT_CODE ON` — correct
- `-fvisibility=hidden` on the shared library target — correct
- Separate static and shared library targets — correct

### Missing — should add for production builds

| Flag | Purpose | Priority |
|------|---------|----------|
| `-Wl,-z,relro,-z,now` | Full RELRO: eager binding + read-only GOT | High |
| `-Wl,--hash-style=gnu` | `.gnu.hash` for ~50% faster symbol lookup | Medium |
| `-Wl,-Bsymbolic-functions` | Prefer local definitions for functions | Medium |
| `-fno-semantic-interposition` | Allow inlining across default-visibility defs | Low |
| `-Wl,-O2` | Linker string merging + tail optimization | Low |

Suggested CMake addition:
```cmake
if(UNIX AND NOT APPLE)
    target_link_options(deucessh_shared PRIVATE
        -Wl,-z,relro,-z,now
        -Wl,--hash-style=gnu
        -Wl,-Bsymbolic-functions
        -Wl,-O2)
    target_compile_options(deucessh_shared PRIVATE
        -fno-semantic-interposition)
endif()
```

Note: macOS uses Mach-O, not ELF — these flags are Linux/FreeBSD only.

### Version script — not yet

No linker version script.  Should be added at 1.0 release for ABI
stability.  Low priority for pre-1.0.

## 3. Data Layout — EXCELLENT

### No problematic patterns found

- No `char *str = "literal"` at file scope (would need relocation)
- No static arrays of function pointers (each element = relocation)
- No static arrays of data pointers
- Algorithm registration is dynamic (malloc + fill at runtime), not
  static initializer tables — avoids load-time relocations entirely
- All string constants are compile-time embedded in `.rodata`

### Minor: `sw_ver` pointer

```c
static const char * const sw_ver = "DeuceSSH-0.1";
```

This is a `const` pointer to a string literal.  The pointer itself
lives in writable data (one relative relocation).  Could be changed
to `static const char sw_ver[] = "DeuceSSH-0.1"` to embed the string
directly in `.rodata` with zero relocations.

### Zero-initialized globals — correct

`gconf` is zero-initialized via implicit BSS placement (no explicit
`= {0}`).  This means no file storage and no relocations until the
application calls the setter functions.

## 4. Symbol Naming

### Consistent `dssh_` prefix for core API

All session/transport/auth/conn/channel functions use the `dssh_`
prefix.  Error codes use `DSSH_ERROR_*`.  Flags use `DSSH_*`.

### Unprefixed algorithm functions — ISSUE

The algorithm registration and key management functions lack the
`dssh_` prefix and could collide with application symbols:

```
register_curve25519_sha256()
register_ssh_ed25519()
register_aes256_ctr()
register_none_enc()
ssh_ed25519_load_key_file()
ssh_ed25519_generate_key()
rsa_sha2_256_load_key_file()
rsa_sha2_256_generate_key()
```

These should be renamed to `dssh_register_curve25519_sha256()`,
`dssh_ed25519_load_key_file()`, etc.  This is an API-breaking change
best done before 1.0.

## 5. Text Relocations — NONE

No patterns that would cause text relocations:
- All code compiled with `-fPIC`
- No inline assembly with absolute addresses
- No non-PIC object files linked into the shared library

Verify with: `readelf -d libdeucessh.so | grep TEXTREL`
(should produce no output)

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
| `-fvisibility=hidden` | IMPLEMENTED |
| `-fPIC` | IMPLEMENTED |
| RELRO + BIND_NOW | MISSING |
| GNU hash style | MISSING |
| `-Bsymbolic-functions` | MISSING |
| Version script | MISSING (pre-1.0) |
| Data layout (no pointer arrays) | EXCELLENT |
| String constants in rodata | EXCELLENT |
| Zero-init globals (BSS) | EXCELLENT |
| Symbol naming prefix | PARTIAL (register/key funcs unprefixed) |
| Text relocations | NONE |
| Dependency isolation | EXCELLENT |

## Recommendations

1. Add linker hardening flags (RELRO, gnu hash, Bsymbolic-functions)
2. Add `-fno-semantic-interposition` to shared library compile flags
3. Rename unprefixed algorithm functions to use `dssh_` prefix (pre-1.0)
4. Change `sw_ver` from `const char * const` to `const char []`
5. Create version script at 1.0 release
