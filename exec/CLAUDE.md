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
