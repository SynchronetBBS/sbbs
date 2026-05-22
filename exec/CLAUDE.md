# CLAUDE.md

This file provides guidance to Claude Code when working on Synchronet JavaScript files.

## JavaScript Engine Constraints

Scripts must work on both **SpiderMonkey 1.8.5** (current master) and
**SpiderMonkey 128** (next-js branch). Write to the intersection of both engines.

### Do NOT use (not in SM 1.8.5)
- `let`, `const` — use `var` (top-level `const` also causes redeclaration errors in SM128)
- Arrow functions (`=>`), template literals, destructuring, spread/rest
- `for...of`, `Object.values()`, `Object.keys()`, `Object.entries()`
- `Promise`, `async`/`await`, `class`, default parameters
- `Map`, `Set`, `WeakMap`, `WeakSet`
- Lookbehind assertions in regex

### Do NOT use (removed in SM128)
- `for each(var x in obj)` — replace with `for(var k in obj) { var x = obj[k]; ... }`
- `.toSource()` — use `JSON.stringify()` for debug/logging, `.toString()` for functions
- Deep copy via `eval(x.toSource())` — use `JSON.parse(JSON.stringify(x))`
- E4X syntax (`XMLList`, `<tag>` XML literals) — build XML strings manually
- Expression closures (`function() expr` without braces/return) — use `function() { return expr; }`
- Bare `eval("function(...) { ... }")` — wrap in parens: `eval("(function(...) { ... })")`

### Safe to use on both engines
- `var`, `for(;;)`, `for(var k in obj)`, `function` declarations
- `JSON.stringify()`, `JSON.parse()`
- `Array.isArray()`, `.indexOf()`, `.map()`, `.filter()`, `.forEach()`
- `.replace()` with regex and function callbacks
- `load()`, `require()`

## File Layout

- `exec/` — command scripts (entry points executed by the BBS)
- `exec/load/` — libraries loaded via `load("name.js")` or `require("name.js", "symbol")`

## Ctrl-A Attribute Codes

See `exec/load/cga_defs.js` (`from_attr_code` map) for the full set.
Example: `"\x01H\x01Y"` = bright yellow, `"\x01N"` = reset to normal.

## Conventions

- Tabs for indentation (tab size 4)
- The `typehtml.js` / `html2asc.js` pair is the model for text-format converters:
  a library in `load/` does the conversion, a thin script in `exec/` handles file I/O
- See `docs/jsobjs.html` for the full Synchronet JS object/function reference

## Command Shells

Two kinds of stock command shells live here. A feature touching one shell
usually needs the equivalent change in the others (check all of them).

### Inventory
- **Baja** (`*.src` compiled to `*.bin`): `obv-2`, `wwiv`, `sdos`, `wildcat`,
  `major`, `simple`, `matrix`, `ftp`. Structured as labeled sections (`:main`,
  `:file_transfers`, `:main_info`, `:file_info`, …) with `cmdkey X` handlers
  gated by a `getcmd "?...keys..."` allowed-key string.
- **JavaScript**:
  - `default.js` — **Synchronet Classic, the primary default shell.** Its
    command tables map keys to eval strings that call functions in
    `load/shell_lib.js` (e.g. `main_info()`, `file_info()`), gated by a
    `console.getkeys("...")` allowed-key string.
  - `lbshell.js` — lightbar shell. **No modal info sub-sections**; it builds
    inline lightbar `typemenu`s on the fly. Don't try to mirror the
    Baja/Classic `:file_info` structure here.
  - `menushell.js` / `menushell-lb.js` — data-driven; commands come from the
    catalog in `load/menu-commands.js` (e.g. `Commands.System.Version`).
    Which command appears on which menu is sysop menu-data config, not code.

### Editing checklist
1. Add the key in **two** places: the allowed-key string (`getcmd`/`getkeys`/
   command table) **and** the handler (`cmdkey`/`case`/eval entry). Missing the
   key string means the handler never fires.
2. Update the matching **menu display file** if one exists. `text/menu/
   maininfo.*` and `xferinfo.*` are shared by both the WWIV and Classic shells.
   These hold raw CP437 + Ctrl-A (`0x01`) bytes — edit byte-safely (e.g. a
   small Python script), NOT the Edit tool, which corrupts high bytes.

### After editing a Baja `.src`
- Recompile: `baja <shell>.src` → `<shell>.bin` (in this dir).
- The running BBS loads a shell's `.bin` **per session at logon** — an active
  session keeps the old one. Log off and back on to pick up the new `.bin`
  (no server restart needed).
- `.bin` files are gitignored build artifacts; commit only the `.src`.

### C-side command backing
Baja info commands (`info_version`, `info_system`, …) dispatch through
`CS_INFO_*` in `src/sbbs3/execfunc.cpp`; the JS equivalents are `bbs.*`
methods. Watch for default-argument drift between the two paths (e.g.
`info_version` vs `bbs.ver()` and the `verbose` flag). Changing the C side
needs a `libsbbs.so` rebuild — and do NOT `make install`/`symlinks`; the
user deploys.
