# SpiderMonkey Migration Comparison Report

## Branch Overview

| | **js-next128** (next-js branch) | **js-next78** (deuce repo) |
|---|---|---|
| **Target SM version** | SpiderMonkey 128 (mozjs-128) | SpiderMonkey 78 (mozjs-78) |
| **Base codebase** | Current master (2026) | Older fork of master (2021) |
| **Branch commits** | 1 (squashed) | 377 (275 by Deuce, 92 by Rob Swindell, 10 by Randy Sommerfeld) |
| **Active period** | 2026 (ongoing) | Apr-Sep 2021 (inactive since) |
| **JS-related diff** | 5,143+ / 6,503- | 52,581+ / 6,188- |
| **Net line change** | -1,360 lines | +46,393 lines |

## Architectural Approach

### js-next128: Shim-based incremental migration

- Preserves the original file structure and code patterns
- Introduces `js_shims.hpp` -- a compatibility header that maps ~50 SM185 macros/types (`JSVAL_TO_INT`, `JS_SET_RVAL`, `JSBool`, etc.) to SM128 equivalents
- ~2,939 legacy macro invocations remain in source via shims
- Each `js_*.cpp` file is modified in-place (edit, not rewrite)
- Property get/set converted to native SM128 accessors via `js_DefineSyncAccessors()` helper + per-class `_prop_getter`/`_prop_setter` functions
- Original `jsSyncPropertySpec` table format preserved (tinyid-based dispatch in `_set_value`/`_get_value` switch statements)

### js-next78: Full rewrite

- Most `js_*.cpp` files are deleted and recreated (git sees them as new files, not modifications)
- Server files (`mailsrvr`, `websrvr`, `services`, `jsexec`, `jsdoor`) also `.c` to `.cpp` rename + full rewrite
- Zero legacy SM185 macros remain -- all code uses native SM78 API directly (`JS::CallArgs`, `args.rval()`, `JS::RootedValue`, etc.)
- Heavy use of C preprocessor macros for boilerplate (`GET_INT`, `SET_INT`, `GET_SET_INT32`, `JS_GET_ARGS_PRIVATE`)
- Property spec changed to store `JSNative` getter/setter function pointers directly in `jsSyncPropertySpec`
- Each property has its own individual getter/setter function (generated via macros like `GET_SET_INT32(name, member)`)
- Removed `#ifdef JAVASCRIPT` guards from JS files entirely

## Code Size Impact

| File | master | js-next128 | js-next78 | js-next128 delta | js-next78 delta |
|---|---|---|---|---|---|
| js_bbs.cpp | 5,388 | 5,087 | 3,176 | -301 | **-2,212** |
| js_console.cpp | 3,084 | 2,956 | 1,998 | -128 | **-1,086** |
| js_socket.cpp | 3,810 | 3,709 | 3,500 | -101 | -310 |
| js_file.cpp | 3,222 | 3,076 | 3,244 | -146 | +22 |
| js_global.cpp | 5,515 | 5,297 | 5,319 | -218 | -196 |
| js_system.cpp | 3,186 | 3,136 | 2,280 | -50 | **-906** |
| jsexec.cpp | 1,641 | 1,615 | 1,396 | -26 | -245 |
| main.cpp | 6,206 | 6,265 | 5,861 | +59 | -345 |
| sbbs.h | 1,690 | 1,731 | 1,495 | +41 | -195 |

js-next78 achieves smaller files by eliminating tinyid enums, switch-based dispatch, and boilerplate through macro-generated per-property functions.

## Key Design Differences

| Concern | js-next128 | js-next78 |
|---|---|---|
| **Property dispatch** | Single getter/setter per class with `switch(tinyid)` | Individual `JSNative` getter/setter per property (macro-generated) |
| **SM185 compat** | `js_shims.hpp` (2,939 macro uses) | None -- fully native SM78 API |
| **GC rooting (events)** | Raw `JSObject*`/`JSFunction*` + `trace` hook in `JSClassOps` | `JS::PersistentRooted<T>` (self-rooting, type-safe) |
| **GC rooting (tls_psk)** | Raw `JSObject*` + trace hook | Field removed entirely |
| **Interrupt callback** | Trigger thread in `js_rtpool.cpp` | Dedicated `js_cxirq.cpp` module with global context list |
| **Runtime/context** | `jsrt_GetNew()` wraps `JS_NewContext()` (SM128: context = runtime) | Still references `jsrt_GetNew()` but deleted `js_rtpool` (likely broken) |
| **Error reporter** | SM128 `JS::SetWarningReporter` + `report->isWarning()` | SM78 `JS_SetErrorReporter` + `JSREPORT_IS_WARNING` (old-style still works in SM78) |
| **Request threading** | Removed (SM128 has no request model) | Removed (SM78 has no request model) |
| **`JSBool` type** | Kept via shim (`typedef bool JSBool`) | Replaced with native `bool` throughout |

## New Features in js-next78 (not in js-next128)

- **Promise/async support** -- `js_jobqueue.cpp`/`.h` implements `JS::JobQueue` for promise resolution
- **ArrayBuffer support** -- Socket and File objects can return `ArrayBuffer` (new `array_buffer` flag)
- **ICU Unicode wrappers** -- `icu_wrappers.c`/`.cpp` for proper Unicode handling
- **Package manager notes** -- `SPaMNotes.txt` design document for future module system
- **Codepage support** -- Socket struct has `codepage` field
- **Non-JS fixes** -- 377 commits include many bug fixes, hex encode/decode, IPv6 fixes, Atari terminal support, etc.

## Potential Issues

### js-next128

- Heavy reliance on compat shims means a second migration pass would be needed to reach "native" SM128 style
- GC trace hooks are less type-safe than `PersistentRooted` but lower overhead
- Still uses some SM185 patterns that may confuse future maintainers

### js-next78

- **Stale since September 2021** -- 5 years behind current master, would require massive merge effort
- **Broken build references**: `main.cpp` still calls `jsrt_GetNew()`/`jsrt_Release()` but `js_rtpool` was deleted
- **SM78 is already EOL** -- Mozilla stopped supporting SM78 years ago; SM128 is the current ESR
- Full file rewrites make it very difficult to merge upstream master changes (no common diff context)
- Based on an older fork of the codebase (missing 5 years of features and fixes)

## Summary

The two branches represent fundamentally different migration philosophies:

**js-next128** is a conservative, incremental migration -- it changes the minimum amount of code needed to compile against SM128, using a compatibility shim layer to preserve existing patterns. This makes it easy to merge with ongoing master development and reduces risk of introducing regressions, at the cost of carrying forward legacy API patterns.

**js-next78** is a ground-up rewrite -- it rewrites every JS binding file to use native SM78 API patterns with macro-generated property accessors. This produces cleaner, more idiomatic code, but at the cost of being essentially unmergeable with the current codebase (5 years diverged, full file replacements). It also targeted SM78, which is now obsolete.

The js-next128 approach is the viable path forward given the current state of both branches.
