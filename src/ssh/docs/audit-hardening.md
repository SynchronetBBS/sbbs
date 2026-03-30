# DeuceSSH — OpenSSF Compiler Hardening Audit

Audit against the OpenSSF "Compiler Options Hardening Guide for C and C++"
(https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html).

DeuceSSH is evaluated as a **green-field C17 shared library** project.
It is pure C (no C++), uses OpenSSL 3.0+ and C11 threads, and targets
Linux and FreeBSD on x86_64 and aarch64.

All flags are feature-probed at configure time via `check_c_compiler_flag`
and `check_linker_flag`.  Flags unsupported by the compiler/linker are
silently omitted, supporting back to GCC 8 and Clang 7.

## Baseline Compile-Time Warnings

| Flag | Status | Notes |
|------|--------|-------|
| `-Wall` | YES | |
| `-Wpedantic` | YES | |
| `-Werror` | YES | |
| `-Wformat -Wformat=2` | YES | |
| `-Wimplicit-fallthrough` | YES | |
| `-Werror=format-security` | YES | |
| `-Werror=implicit` | YES | C obsolete construct |
| `-Werror=incompatible-pointer-types` | YES | C obsolete construct |
| `-Werror=int-conversion` | YES | C obsolete construct |
| `-Wconversion` | YES | Probed; all narrowing conversions use range-checked locals |

### GCC-only contextual (probed)

| Flag | Status | Notes |
|------|--------|-------|
| `-Wtrampolines` | YES | Probed; no nested functions in codebase |
| `-Wbidi-chars=any` | YES | Probed; Trojan Source defense (GCC 12+) |
| `-fzero-init-padding-bits=all` | YES | Probed (GCC 15+) |

## Runtime Protection (probed)

| Flag | Status | Notes |
|------|--------|-------|
| `-D_FORTIFY_SOURCE=3` | YES | Non-Debug builds only; prepends `-U_FORTIFY_SOURCE` |
| `-fstrict-flex-arrays=3` | YES | Probed; all flex arrays use C99 `[]` |
| `-fstack-clash-protection` | YES | Probed |
| `-fstack-protector-strong` | YES | Probed |
| `-ftrivial-auto-var-init=zero` | YES | Probed |
| `-fno-delete-null-pointer-checks` | YES | |
| `-fno-strict-overflow` | YES | |
| `-fno-strict-aliasing` | YES | |
| `-fexceptions` | N/A | Not needed — C11 threads, not pthreads |

### Architecture-specific (probed)

| Flag | Status | Notes |
|------|--------|-------|
| `-fcf-protection=full` | YES | Probed; x86_64 only (Intel CET) |
| `-mbranch-protection=standard` | YES | Probed; aarch64 only (ARMv8.3+ PAC/BTI) |

## Linker Flags

| Flag | Status | Notes |
|------|--------|-------|
| `-Wl,-z,relro` | YES | Shared library |
| `-Wl,-z,now` | YES | Shared library |
| `-Wl,-z,nodlopen` | YES | Probed; shared library |
| `-Wl,-z,noexecstack` | YES | Probed; shared library |
| `-Wl,--as-needed` | YES | Probed; shared library |
| `-Wl,--no-copy-dt-needed-entries` | YES | Probed; shared library |
| `-Wl,--hash-style=gnu` | YES | Shared library |
| `-Wl,-Bsymbolic-functions` | YES | Shared library |
| `-Wl,-O2` | YES | Shared library |
| `-fPIC` | YES | Via `POSITION_INDEPENDENT_CODE` |
| `-fvisibility=hidden` | YES | Shared library |
| `-fno-semantic-interposition` | YES | Shared library |

## C++ Flags (not applicable)

| Flag | Status | Notes |
|------|--------|-------|
| `-D_GLIBCXX_ASSERTIONS` | N/A | Pure C project |

## Compatibility Notes

- All optional flags are probed via `check_c_compiler_flag` /
  `check_linker_flag`.  Older compilers/linkers that lack a flag
  simply skip it — no configure errors.
- `-D_FORTIFY_SOURCE=3` disabled in Debug builds (incompatible with
  sanitizers).  Must prepend `-U_FORTIFY_SOURCE` to override distro
  defaults.
- `-fstrict-flex-arrays=3` requires GCC 13+ or Clang 16+.  All
  DeuceSSH flex arrays already use C99 `[]` — no code changes needed.
- `-fcf-protection=full` requires Linux 6.6+ and glibc 2.39+ at
  runtime for full CET enforcement.
- `-mbranch-protection=standard` requires ARMv8.3+ for PAC, ARMv8.5+
  for BTI.  Uses hint instructions for backward compatibility.
- `-Wconversion` enabled.  All narrowing conversions in library code
  use range-checked intermediate variables (never inline casts).
  Both upper and lower bounds are verified, with overflow checks
  before arithmetic.  Test code uses explicit casts where safe.

## Summary

DeuceSSH implements all OpenSSF recommended hardening flags.  All
flags are feature-probed for portability across compiler versions.

| Category | Coverage |
|----------|----------|
| Warnings | 13 of 13 |
| Runtime protection | 8 of 8 applicable |
| Architecture CFI | 2 of 2 (probed per arch) |
| Linker hardening | 11 of 11 |
| **Total** | **34 of 34** |
