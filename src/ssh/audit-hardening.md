# DeuceSSH — OpenSSF Compiler Hardening Audit

Audit against the OpenSSF "Compiler Options Hardening Guide for C and C++"
(https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html).

DeuceSSH is evaluated as a **green-field C17 shared library** project.
It is pure C (no C++), uses OpenSSL 3.0+ and C11 threads, and targets
Linux and FreeBSD on x86_64 and aarch64.

## Baseline Compile-Time Warnings

| Flag | Status | Notes |
|------|--------|-------|
| `-Wall` | YES | In `DEUCESSH_COMPILE_OPTIONS` |
| `-Wformat -Wformat=2` | NO | Should add |
| `-Wconversion` | NO | Should add; may require code fixes |
| `-Wimplicit-fallthrough` | NO | Should add; library uses explicit breaks |
| `-Werror=format-security` | NO | Should add |
| `-Werror` | YES | In `DEUCESSH_COMPILE_OPTIONS` |
| `-Wpedantic` | YES | In `DEUCESSH_COMPILE_OPTIONS` |

### GCC-only contextual

| Flag | Status | Notes |
|------|--------|-------|
| `-Wtrampolines` | NO | Should add (GCC only, no nested functions used) |
| `-Wbidi-chars=any` | NO | Should add (GCC 12+, Trojan Source defense) |
| `-fzero-init-padding-bits=all` | NO | Should add (GCC 15+) |

### C obsolete construct errors

| Flag | Status | Notes |
|------|--------|-------|
| `-Werror=implicit` | NO | Should add; C17 code should already comply |
| `-Werror=incompatible-pointer-types` | NO | Should add |
| `-Werror=int-conversion` | NO | Should add |

## Runtime Protection

| Flag | Status | Notes |
|------|--------|-------|
| `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3` | NO | Should add for release builds; requires `-O2` (already used) |
| `-fstrict-flex-arrays=3` | NO | Should add; all flex arrays already use C99 `[]` |
| `-fstack-clash-protection` | NO | Should add |
| `-fstack-protector-strong` | NO | Should add |
| `-ftrivial-auto-var-init=zero` | NO | Should add for production builds |
| `-fno-delete-null-pointer-checks` | NO | Should add |
| `-fno-strict-overflow` | NO | Should add |
| `-fno-strict-aliasing` | NO | Should add; library does some pointer casts |
| `-fexceptions` | NO | Not needed — library uses C11 threads, not pthreads |

### Architecture-specific

| Flag | Status | Notes |
|------|--------|-------|
| `-fcf-protection=full` | NO | Should add for x86_64 (Intel CET) |
| `-mbranch-protection=standard` | NO | Should add for aarch64 (ARMv8.3+ PAC/BTI) |

## Linker Flags

| Flag | Status | Notes |
|------|--------|-------|
| `-Wl,-z,relro` | YES | Shared library target |
| `-Wl,-z,now` | YES | Shared library target |
| `-Wl,-z,nodlopen` | NO | Should add for shared library (not a plugin) |
| `-Wl,-z,noexecstack` | NO | Should add |
| `-Wl,--as-needed` | NO | Should add |
| `-Wl,--no-copy-dt-needed-entries` | NO | Should add |
| `-fPIC` | YES | Via `POSITION_INDEPENDENT_CODE` |

## C++ Flags (not applicable)

| Flag | Status | Notes |
|------|--------|-------|
| `-D_GLIBCXX_ASSERTIONS` | N/A | Pure C project |

## Current vs Recommended

### Currently set (compile)
```
-Wall -Wpedantic -pedantic -Werror
-fvisibility=hidden                  (shared only)
-fno-semantic-interposition          (shared only)
```

### Currently set (link, shared library, UNIX only)
```
-Wl,-z,relro,-z,now
-Wl,--hash-style=gnu
-Wl,-Bsymbolic-functions
-Wl,-O2
```

### Recommended additions (compile, both targets)
```
-Wformat -Wformat=2
-Wconversion -Wimplicit-fallthrough
-Werror=format-security
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
-fstrict-flex-arrays=3
-fstack-clash-protection
-fstack-protector-strong
-fno-delete-null-pointer-checks
-fno-strict-overflow
-fno-strict-aliasing
-ftrivial-auto-var-init=zero
```

### Recommended additions (compile, GCC only)
```
-Wtrampolines
-Wbidi-chars=any
```

### Recommended additions (compile, C only)
```
-Werror=implicit
-Werror=incompatible-pointer-types
-Werror=int-conversion
```

### Recommended additions (compile, architecture-specific)
```
-fcf-protection=full          (x86_64 only)
-mbranch-protection=standard  (aarch64 only)
```

### Recommended additions (link, shared library)
```
-Wl,-z,nodlopen
-Wl,-z,noexecstack
-Wl,--as-needed
-Wl,--no-copy-dt-needed-entries
```

## Compatibility Notes

- `-D_FORTIFY_SOURCE=3`: requires GCC 12+ or Clang 9+.  Incompatible
  with sanitizers (ASan, MSan).  Should be disabled in coverage/test
  builds.  Must prepend `-U_FORTIFY_SOURCE` to override distro defaults.
- `-fstrict-flex-arrays=3`: requires GCC 13+ or Clang 16+.  All
  DeuceSSH flex arrays already use C99 `[]` notation — no code changes
  needed.
- `-fstack-clash-protection`: performance impact scales with stack
  allocation size.  DeuceSSH uses small stack frames — negligible.
- `-Wconversion`: may produce warnings for implicit integer conversions
  in existing code.  Staged rollout recommended.
- `-ftrivial-auto-var-init=zero`: masks uninitialized variable bugs in
  test code.  Consider disabling in test builds or using `=pattern`
  for debug builds.
- `-fcf-protection=full` requires Linux 6.6+ and glibc 2.39+ at
  runtime for full CET enforcement.
- `-mbranch-protection=standard` requires ARMv8.3+ for PAC, ARMv8.5+
  for BTI.  Uses hint instructions for backward compatibility.

## Summary

DeuceSSH has good baseline warning flags (`-Wall -Wpedantic -Werror`)
and strong linker hardening.  The main gaps are:
- Missing runtime protection flags (FORTIFY_SOURCE, stack protectors,
  trivial auto var init, UB mitigations)
- Missing format string and conversion warnings
- Missing architecture-specific control-flow protection
- Missing `noexecstack`, `nodlopen`, `as-needed` linker flags

All recommended flags are compatible with the existing codebase (C17,
no nested functions, no struct hack arrays, no executable stack needs).
