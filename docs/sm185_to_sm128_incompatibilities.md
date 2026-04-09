# SpiderMonkey 1.8.5 to 128 Incompatibilities for Synchronet Scripts

This document describes potential incompatibilities between scripts written for
SpiderMonkey 1.8.5 (SM185) — including those that take advantage of Mozilla
proprietary extensions or bugs — and scripts that need to be compatible with
both SM185 and SM128 (SpiderMonkey 128).

## 1. Top-level `const` (and `let`) — CRITICAL

**SM185:** `const` was essentially function-scoped, like `var`. Re-executing a
script in the same global (e.g. a terminal server session running the same menu
script twice) silently re-declared the `const`.

**SM128:** `const` is ES6-compliant — block-scoped, creates non-configurable
bindings in the global lexical environment. Running the same script a second
time in the same context throws `SyntaxError: redeclaration of const`.

**Scope:** ~20 scripts use top-level `const` (`SlyEdit.js`,
`websocketservice.js`, `nntpservice.js`, `default.js`, `sbbsedit.js`, etc.).
All must use `var` at top level for dual compatibility. `const` inside functions
is fine in both engines.

**Dual-compat fix:** Use `var` for all top-level declarations. This works
identically in both engines (no block-scoping semantics, re-declarable).

## 2. `load({}, "modopts.js", ...)` — Completion value pattern — CRITICAL

**SM185:** `load({}, "script.js")` executes the script in a separate scope
object and returns the script's **completion value** (the value of the last
expression statement).

**SM128:** `load({}, "script.js")` uses `ExecuteInJSMEnvironment` which returns
`bool`, not a completion value. The `load()` function returns the **jsmEnv
scope object** itself, not the script's completion value.

**Impact:** `modopts.js` relies on this — its last line is
`get_mod_options.apply(null, argv);` which is a completion value. All ~36
callers that use `load({}, "modopts.js", "module_name")` get back the scope
object (with `get_mod_options` as a property) instead of the options
object/value.

**Dual-compat fix:** Use `load("modopts.js", ...)` (without the scope object).
`JS_ExecuteScript` returns the completion value correctly in both engines. The
tradeoff is that `get_mod_options` leaks into the global scope, but this is
harmless. Same issue applies to `nodelist_options.js`.

**General rule:** `load({}, ...)` works for **library loading** (where you
access properties of the returned object). It does NOT work for
**value-returning scripts** in SM128.

## 3. `toSource()` — REMOVED

**SM185:** Every object, array, function, string, etc. had a `.toSource()`
method that returned a parseable JS source representation (e.g.,
`({a:1, b:2})`).

**SM128:** `toSource()` is completely removed.

**Scope:** 57 occurrences across 12 files. `imapservice.js` is the heaviest
user (33 occurrences), using it extensively for serialization, debugging, and —
critically — for dynamically building `eval()`'d functions with embedded
literal values.

**Dual-compat fix:** Replace with `JSON.stringify()` where used for
serialization/debugging. For the `eval()` pattern in `imapservice.js` (e.g.
building filter functions with embedded array literals), replace
`array.toSource()` with `JSON.stringify(array)` — JSON is valid JS literal
syntax for arrays, strings, numbers, and booleans. For objects, be aware that
`JSON.stringify` produces `"key":` (quoted keys) while `toSource()` produced
`key:` (unquoted) — but `eval()` accepts both.

## 4. `for each...in` — REMOVED

**SM185:** Mozilla extension `for each (var x in obj)` iterates over **values**
instead of keys.

**SM128:** Completely removed. `SyntaxError`.

**Scope:** 68 occurrences across 20 files (IRC bots, `frame.js`, `layout.js`,
`json-service.js`, `json-chat.js`, `allusers.js`, etc.).

**Dual-compat fix:** Replace with standard `for (var k in obj) { var x = obj[k]; ... }`
or, if iterating an array, use a standard `for` loop or
`Array.prototype.forEach`. Both work in SM185 and SM128.

## 5. Expression closures — REMOVED

**SM185:** `function(x) x * 2` (no braces, implicit return). Mozilla
extension.

**SM128:** Removed. `SyntaxError`.

**Scope:** At least 1 occurrence in `broker.js` (line 2638):
```js
function() MQTT.Packet.PUBACK.prototype.serializeVariableHeader
```

**Dual-compat fix:** Add braces and `return`:
```js
function() { return MQTT.Packet.PUBACK.prototype.serializeVariableHeader; }
```

## 6. `__defineGetter__` / `__defineSetter__` — LIKELY OK BUT FRAGILE

**SM185:** Widely supported Mozilla extension.

**SM128:** Still supported (it's in Annex B of the ES spec), but deprecated.

**Scope:** 201 occurrences across 18 files (`frame.js`, `layout.js`,
`tree.js`, `inputline.js`, `mapgenerator.js`, etc.). Heavily used in the
TUI/frame library.

**Dual-compat:** These work in both engines today. However,
`Object.defineProperty()` is the standard replacement and works in both SM185
and SM128. No urgent fix needed, but worth noting for long-term maintenance.

## 7. `__proto__` direct access

**SM185 & SM128:** Both support `__proto__` (Annex B), but SM128 is stricter
about prototype chain manipulation.

**Scope:** 18 occurrences across 5 files (`fido-nodelist-browser.js`,
`broker.js`, `dorkit.js`, `agwpe.js`).

**Dual-compat:** Generally works in both, but `Object.getPrototypeOf()` /
`Object.setPrototypeOf()` are safer. Low risk.

## 8. `Iterator()` / `StopIteration` / `__iterator__` — REMOVED

**SM185:** Custom iteration protocol using `Iterator()`, `StopIteration`, and
`__iterator__`.

**SM128:** Replaced by ES6 `Symbol.iterator` / `for...of` protocol.

**Scope:** 3 occurrences in `fido-nodelist-browser.js` (but this file appears
to be a bundled polyfill, so it may self-handle).

**Dual-compat:** Tricky. SM185 doesn't support `Symbol.iterator` or
`for...of`. SM128 doesn't support `Iterator()`. Scripts using this pattern need
conditional code or must avoid the custom iterator protocol entirely, using
`for...in` with manual value access.

## 9. E4X (ECMAScript for XML) — REMOVED

**SM185:** Full E4X support (`XML`, `XMLList`, `.@attr`, `..child`,
`default xml namespace`).

**SM128:** Completely removed.

**Scope:** 219 occurrences across 6 files — but almost all are in the IRC RPG
bot (`rpgbot/`) and `callsign.js`. The `syncjslint.js` reference is just a
comment.

**Dual-compat:** No dual-compat path exists. E4X must be replaced entirely
with DOM-style XML parsing or string manipulation. SM128 has no built-in XML
object. These scripts cannot run on SM128 without a rewrite.

## 10. Strict mode behavior differences

**SM185:** `"use strict"` was partially implemented; many strict-mode
violations were silently ignored.

**SM128:** Full ES6 strict mode enforcement. Things that silently failed in
SM185 now throw:
- Assignment to undeclared variables → `ReferenceError`
- Assignment to read-only properties → `TypeError`
- `delete` on non-configurable properties → `TypeError`
- Duplicate parameter names → `SyntaxError`
- Octal literals (`0777`) → `SyntaxError` (use `0o777`)

**Dual-compat:** Write code that is actually strict-mode compliant. Both
engines accept it; only SM128 enforces it.

## 11. `exit_code` redefinition (C++ side)

**SM185:** `JS_DefineProperty` on an already-defined non-configurable property
silently succeeded or was a no-op.

**SM128:** Throws `TypeError: can't redefine non-configurable property`. This
was hit when scripts call `exit()` which sets `exit_code`.

**Dual-compat:** Fixed on the C++ side (using `JS_SetProperty` instead of
`JS_DefineProperty`). Script authors don't need to worry about this one.

## 12. Scoping differences with `js.exec()` and `exec_dir` (C++ side)

**SM185:** `JS_ExecuteScript` with a scope object honored the scope for
property lookups, so `js.exec_dir` resolved correctly in executed scripts.

**SM128:** Scope parameter to `JS_ExecuteScript` is ignored. Properties like
`js.exec_dir` must be explicitly propagated.

**Dual-compat:** Fixed on the C++ side. But script authors should be aware that
scope isolation semantics differ if they rely on `js.exec()` for sandboxing.

## Summary of risk by category

| Risk | Issue | Files affected |
|------|-------|---------------|
| **Critical** | Top-level `const`/`let` redeclaration | ~20 files |
| **Critical** | `load({}, "modopts.js")` completion value | ~36 call sites |
| **High** | `toSource()` removed | 12 files, 57 uses |
| **High** | `for each...in` removed | 20 files, 68 uses |
| **High** | E4X removed (rpgbot, callsign) | 6 files, 219 uses |
| **Moderate** | Expression closures | 1+ files |
| **Moderate** | Strict mode enforcement | Unknown (latent) |
| **Low** | `Iterator()`/`StopIteration` | 1 file (polyfill) |
| **Low** | `__defineGetter__`/`__defineSetter__` | 18 files (still works) |
| **Low** | `__proto__` | 5 files (still works) |

The first two (top-level `const` and `load({})` completion values) are the most
insidious because they don't cause syntax errors — they cause silent wrong
behavior or errors only on the second execution.
