# DeuceSSH — Drepper DSO Best Practices Audit

Audit of the shared library build against Ulrich Drepper's
[How To Write Shared Libraries](https://www.akkadia.org/drepper/dsohowto.pdf).
The symbol-versioning policy below was revised after reading the complete
47-page version 4.1.2, dated December 10, 2011.

The audit below is a historical build snapshot. The
[symbol-versioning plan](#symbol-versioning-plan) records the implemented v1.0
policy. Native FreeBSD export/import and installed-consumer checks pass with
both providers; native Linux, macOS and Windows execution remains for CI.

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

1. Run the committed ABI export/import and production-consumer tests in native
   Linux, macOS and Windows CI before release.

## Symbol-versioning plan

Status: **approved and implemented for the initial 1.0 interface**.
`deucessh-abi.h` assigns all 138 public functions. CMake checks those assignments
against installed declarations and generates explicit ELF, Mach-O and PE export
controls; public declarations select matching native names. The ordinary C API
spelling is unchanged. `dssh_abi_bindings` inspects production exports and
executable/consumer-DSO imports, and `dssh_abi_static`, `dssh_abi_shared` and
`dssh_abi_dso` exercise normal C17 calls. No later interface or compatibility
wrapper is introduced in this initial release.

### Required effect on every shared-library platform

1. A previously linked application or consumer DSO keeps the interface version
   it selected, including its signature, data representation and documented
   behavior, when using an upgraded DeuceSSH library.
2. Newly compiled and linked code selects the interface described by its current
   headers. A provider missing that interface must fail to satisfy the required
   import; it must not silently bind the caller to an older ABI.
3. Multiple interface versions share one coordinated implementation/state where
   appropriate. Library filename changes are a fallback for an ABI that cannot
   be retained, rather than the only strategy on a non-ELF platform.

These effects are the requirement. Native ELF version nodes, Mach-O symbol
aliases and PE export names are different ways to encode the binding. A version
resource or library-wide compatibility number alone does not pin an individual
call to an older function ABI. Normal load-time imports must resolve before
execution; explicitly delayed or runtime lookups enforce the requirement when
the requested entry is resolved.

### Initial v1.0 assignment

- Assign every public, externally linked DeuceSSH function the logical interface
  version 1.0. This includes the consumer APIs, crypto/result utilities,
  algorithm registration/key helpers and exported module-support APIs from all
  installed public headers.
- Keep application-written C calls unchanged. Headers and link artifacts bind
  those names to the selected platform representation described below.
- Keep one explicit public-symbol/interface assignment list, used to produce
  the native export maps and bindings. On ELF, use `DEUCESSH_1.0` and explicit
  global names, with `local: *;` for the rest. Do not use a broad `dssh_*`
  wildcard that silently assigns future additions to the original interface.
  Keep private functions, bundled primitives and embedded-provider symbols out
  of the public export set on every platform.
- Keep imported dependency symbols under their providers' versioning. Public
  macros, constants, inline functions, typedefs and struct layouts have no dynamic
  symbol entry; their compatibility is part of the overall public ABI contract.
- Start with one current v1.0 definition per public function. No pre-1.0 symbol
  versions or compatibility aliases are introduced.

### Subsequent releases

| Change | Required treatment |
| --- | --- |
| Implementation fix preserving documented behavior for valid uses | Keep existing symbol assignments. Undocumented behavior does not define the supported ABI. |
| New public function | Assign it the interface version of the release introducing it, such as 1.1. Add the corresponding native binding and retain existing assignments and definitions. |
| Backward-compatible extension of an existing function | When callers need to distinguish the extended interface from an older library, select a new interface version for new links and retain the old binding. Both versions may use the same implementation if it satisfies both contracts. |
| Incompatible function signature or documented behavior | Require API review. Where both ABIs can be implemented safely, retain the old entry and select the new ABI for new links, using coordinated implementations or wrappers as needed. |
| Change whose required old ABI cannot be retained | Require API review and a new library ABI identity/SONAME. Public struct layouts, callbacks, ownership and state shared between entry points must be considered together. |

Retain ELF SONAME `libdeucessh.so.1` while the library continues to provide its
published ABIs. A release/API version increase does not itself require a SONAME
change. CMake sets `DEUCESSH_ABI_MAJOR` separately from `PROJECT_VERSION_MAJOR`.
The full library filename can continue to follow the package release version.

For an existing function with two ABIs, the old entry has a nondefault version
such as `function@DEUCESSH_1.0`; new links select the single default, such as
`function@@DEUCESSH_1.1`. Existing binaries retain their old binding. GNU-style
versioning requires the actual old definition to remain available: declaring
node inheritance alone does not supply it. Keep predecessor relationships in
the map for clarity, and never remove a published node while retaining that ABI.

Interface assignments record availability, including extensions to existing
functions. They are not mechanically added for every release or automatically
excluded from patch releases. An implementation-only patch keeps its existing
assignments. These versions also do not describe SSH wire protocol or algorithm
negotiation versions.

An exposed struct change cannot be handled merely by attaching a new label to
one function. Review every entry point and callback that exchanges the affected
representation before claiming that old and new ABIs can coexist.

### Platform and build scope

| Build | Mechanism |
| --- | --- |
| ELF shared libraries, including Linux and FreeBSD | Apply the explicit version map through the actual linker driver. Versioning is required for these shared builds; an unsupported linker must produce a clear configuration failure rather than silently omit it. |
| macOS shared libraries | Use ABI-qualified Mach-O symbol names and declaration aliases, following Apple's symbol-versioning approach. For example, the C spelling `dssh_example()` can bind to `_dssh_example_v1_0`. A later declaration selects a new qualified name while the dylib retains the old definition. Keep controlled exports and appropriate dylib identity/version metadata as well. |
| Windows DLLs | Use ABI-qualified PE export names, for example `dssh_example_v1_0`, with public-header aliases selecting the current declaration and its matching import-library symbol. A later header selects the new export while the same DLL retains the old export/adapter. A `.def` file controls export names and mappings; the import library records the selected name. UCRT compatibility and DLL version resources do not provide this binding themselves. |
| Static libraries | There is no dynamic-loader version table. Any header-selected external name must match an archive definition; applications and external modules must use compatible headers, library and provider ABIs. |

The macOS/Windows bindings preserve source-level function spelling;
binary export names are platform-specific. Compiler declaration aliases and
ordinary C header aliases belong behind the public portability layer. They
must not expose provider types or alter public C types. For static Windows
builds, any such header alias must match the archive's definitions as well.

Apple's `__DARWIN_ALIAS` family deliberately selects alternate symbols while
preserving source-level names, including suffixes such as `$UNIX2003` and
`$INODE64`. This supplies the relevant macOS precedent; restricting the plan to
dylib-wide version numbers would miss the required effect. DeuceSSH uses the
portable-identifier suffix `_v1_0` with the same declaration-alias mechanism. See
[Apple's declarations and explanation](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/cdefs.h#L788).
Windows named-export and import-library mapping is documented in Microsoft's
[EXPORTS reference](https://learn.microsoft.com/en-us/cpp/build/reference/exports)
and [import-library reference](https://learn.microsoft.com/en-us/cpp/build/reference/building-an-import-library-and-export-file).

Runtime symbol lookups must request an ABI-qualified entry, or explicitly use
a documented current-default lookup with the matching declaration. On ELF a
version-aware lookup can select a node; on macOS/Windows the qualified export
name selects it. Do not silently fall back to an older definition when a
requested version is unavailable.

### Consumer link contract

Headers and the library used for the final link must describe the same current
API. Consumers, including other DSOs, must link against DeuceSSH explicitly so
the linker can record symbol-version requirements. Do not leave DeuceSSH calls
unresolved in the expectation that a later load will establish their versions.
Normal relocatable consumer objects acquire these requirements at final link;
reusing objects compiled for an older API with a newer default definition is
not a substitute for rebuilding against matching headers and libraries.

### Implementation and acceptance

1. Reconcile the public-symbol assignment list with header declarations and
   actual shared-library definitions. Make generated export/binding artifacts
   proper build dependencies. Preserve visibility and dependency isolation.
2. Check every platform's actual exports and consumer imports. ELF definitions
   must have the expected node; Mach-O and PE names must encode the selected
   interface. No public definition may be missing or accidentally unqualified,
   and no private definition may be exported. Exclude undefined provider imports.
3. Build and run normal installed C17 consumers against each native shared
   library. Inspect their recorded bindings. Retain static-consumer checks,
   including consistency of any header aliases with archive definitions.
4. Check that a consumer DSO also records its DeuceSSH dependency and versioned
   references. When first introducing a second version of an interface, verify
   that an already-linked consumer keeps its original binding and a newly
   compiled/linked consumer selects the new default. Versioned extensions must
   also reject a provider that lacks the required interface version.
5. Apply the same old-consumer/new-consumer/provider-missing-interface cases on
   Linux/FreeBSD, macOS and Windows using their native mechanisms. Run affected
   existing consumer tests on both crypto backends. Cross-generated object/import
   tables establish binding syntax only; native CI must establish execution.
   Introduce later-interface acceptance when the first such change is proposed.

The primary design reference is Drepper's paper, sections 3.1–3.8 (pages 34–39),
especially the compatible-extension and incompatible-interface examples in
sections 3.5 and 3.7. Export maps supplement compiler visibility annotations as
described in sections 2.2.2–2.2.5. The
[GNU linker VERSION documentation](https://sourceware.org/binutils/docs/ld/VERSION.html)
provides the linker syntax; it does not determine this project's compatibility
policy. The paper's historical platform/compiler observations require checking
against the toolchains actually supported when implementing the plan.
