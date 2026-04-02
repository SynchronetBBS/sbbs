# Portability Audit — Vendor Crypto Code

Audit of `kex/sntrup761.c` (SUPERCOP reference, public domain),
`kex/libcrux_mlkem768_sha3.h` (Cryspen, MIT), and `kex/mlkem768.c`
(DeuceSSH wrapper) against the Portability rule in `RULES.md`:

> Code must use only C17 standard functions and must not rely on any
> platform-specific APIs.

Both files are third-party; modifying them carries a maintenance
burden on every upstream sync.  Findings are classified as
**blocking** (prevents compilation) or **silent** (compiles but
produces wrong results).

---

## sntrup761.c (SUPERCOP)

### `__attribute__((unused))` — non-standard but harmless

102 occurrences on `static inline` function declarations.  Not C17
standard.  GCC, Clang, and ICC support it.  MSVC ignores unknown
`__attribute__` in C mode (warns in C++).

**Impact**: Compiler warnings on non-GCC/Clang.  Not blocking.

### Platform-specific inline assembly — properly guarded

78 blocks of `#if defined(__GNUC__) && defined(__x86_64__)` / `#elif
defined(__GNUC__) && defined(__aarch64__)` / `#else` for constant-time
arithmetic.  Every block has a pure-C `#else` fallback.

**Impact**: None — fallbacks are correct on all platforms.

### FINDING — `crypto_int{16,32,64}_optblocker`: undefined symbols

Lines 45–47 declare:
```c
extern volatile crypto_int16 crypto_int16_optblocker;
extern volatile crypto_int32 crypto_int32_optblocker;
extern volatile crypto_int64 crypto_int64_optblocker;
```
These are used in the `#else` (generic) fallback paths of the
constant-time helpers (e.g., line 87: `crypto_int16_x ^=
crypto_int16_optblocker`).  They are never defined anywhere in the
codebase.

On x86\_64 and aarch64, the `#if` branches take the asm path and the
fallback code is dead — the linker never resolves the symbols.  On
**any other architecture** (e.g., RISC-V, 32-bit ARM, MIPS, POWER),
the generic fallback is used and the build **fails with undefined
symbol errors**.

**Impact**: **Blocking** on non-x86\_64/aarch64 platforms.
**Fix**: Add a definition file (`kex/sntrup761_optblocker.c`) with
three zero-initialized volatile globals, or use SUPERCOP's
`cryptoint/optblocker.c`.

---

## libcrux\_mlkem768\_sha3.h (Cryspen/libcrux)

### `__attribute__` — handled portably

Lines 32–33:
```c
#if !defined(__GNUC__) || (__GNUC__ < 2)
 #define __attribute__(x)
#endif
```
Redefines `__attribute__` as nothing on non-GCC.  Correct.

### FINDING — `__builtin_popcount`: no portable fallback

Line 178:
```c
#ifdef _MSC_VER
    return __popcnt(x0);
#else
    return (uint32_t)__builtin_popcount(x0);
#endif
```
`__builtin_popcount` is GCC/Clang-specific.  The `#else` assumes all
non-MSVC compilers are GCC-compatible.  A strict C17 compiler (e.g.,
Tiny CC, CompCert, Keil) would fail.

**Impact**: **Blocking** on non-GCC/Clang/MSVC compilers.
**Fix**: Add a portable fallback:
```c
#elif !defined(__GNUC__)
    x0 = x0 - ((x0 >> 1) & 0x55);
    x0 = (x0 & 0x33) + ((x0 >> 2) & 0x33);
    return (x0 + (x0 >> 4)) & 0x0F;
#endif
```

### `#pragma once` — non-standard

Line 48.  Widely supported (GCC, Clang, MSVC, ICC) but not C17
standard.  The header is only included from `mlkem768.c`, so there
is no multiple-inclusion risk in practice.

**Impact**: Compiler warning on strict C17.  Not blocking.

---

## mlkem768.c (DeuceSSH wrapper)

### FINDING — `__BYTE_ORDER__` assumes GCC/Clang or little-endian

Lines 7–28:
```c
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    /* byte-swap macros */
#else
    #define lcx_htole64(x) (x)
    ...
#endif
```
`__BYTE_ORDER__` is a GCC/Clang predefined macro.  On a non-GCC
big-endian compiler (e.g., IBM xlc on POWER, HP aCC on PA-RISC),
`__BYTE_ORDER__` is undefined, the `#if` is false, and the identity
macros are used — producing **silently incorrect results** (wrong
byte order in SHA-3 state).

**Impact**: **Silent data corruption** on non-GCC big-endian
platforms.
**Fix**: Use a more portable detection chain:
```c
#if defined(__BYTE_ORDER__)
 #define DSSH_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
 #define DSSH_BIG_ENDIAN 1
#else
 #define DSSH_BIG_ENDIAN 0
#endif
```

---

## Summary

| File | Finding | Severity | Platforms affected |
|------|---------|----------|--------------------|
| sntrup761.c | optblocker undefined symbols | **Blocking** | All except x86\_64, aarch64 |
| libcrux\_mlkem768\_sha3.h | `__builtin_popcount` no fallback | **Blocking** | Non-GCC/Clang/MSVC |
| mlkem768.c | `__BYTE_ORDER__` assumes GCC | **Silent corruption** | Non-GCC big-endian |
| sntrup761.c | `__attribute__((unused))` | Warning | Non-GCC/Clang |
| libcrux\_mlkem768\_sha3.h | `#pragma once` | Warning | Strict C17 |

The three serious findings affect platforms outside the current
build matrix (x86\_64 Linux/FreeBSD/macOS with GCC or Clang).  They
would become blockers if DeuceSSH targets RISC-V, 32-bit ARM, POWER,
or uses a non-GCC/Clang compiler.
