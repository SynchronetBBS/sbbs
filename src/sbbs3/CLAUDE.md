## C / C++ coding style

For any C or C++ source modifications in this tree, follow the dominant coding style defined in [../uncrustify.cfg](../uncrustify.cfg). When in doubt about formatting (indentation, brace placement, spacing, alignment, line wrapping, etc.), defer to that configuration rather than inventing a new convention or matching nearby code that may already be out of compliance.

An uncrustify pass (or `--check` PASS) is **not** proof of conformance — the config deliberately leaves some constructs alone that are still non-conformant. In particular, **single-line compound blocks** like `{ found = true; break; }` are not house style and must be expanded into properly-braced multi-line blocks, even though uncrustify tolerates them (its only knobs that would catch them, `nl_after_semicolon`/`nl_after_brace_open`, would also shred wanted idioms — evaluated and rejected, see below). The sanctioned exception is the compact, column-aligned **`case X: stmt; break;` switch-table idiom** (e.g. `zmodem.c`, `con_out.cpp`) — leave those tables one-row-per-case. Don't enable `nl_after_semicolon` or `nl_after_brace_open` in `uncrustify.cfg` to enforce the compound-block rule: measured across `src/sbbs3` they'd reformat ~822 lines in 57 files, almost all of it those case tables and one-line `enum {...}` declarations, and uncrustify has no case-body exemption.

## Prefer consolidation when the same lines repeat

When you notice the same cluster of fields, the same ini read/write block, or the same menu/dispatch pattern repeated across the per-server code (web/ftp/mail/services, or the various scfg dialogs, etc.), prefer to factor it into a shared form rather than leaving the duplication. Concretely:

- **Repeated field clusters in startup/config structs** → lift into a named sub-struct (e.g. `struct login_attempt_settings`, `struct max_concurrent_settings`, `struct rate_limit_settings` in `startup.h`) and embed by value. A nested struct is byte-contiguous in declared order, so this preserves the on-disk/ABI layout of the outer struct.
- **Repeated ini get/set blocks in `sbbs_ini.c`** → factor into a `get_<thing>_settings()` / `set_<thing>_settings()` helper pair, mirroring `get_login_attempt_settings` / `set_login_attempt_settings`.
- **Repeated menu logic in `scfg/`** → use a "view" struct of pointers (e.g. `struct rate_limit_cfg_view`) and one shared `<thing>_cfg()` function, plus a thin per-server wrapper that initializes the view. NULL pointers in the view can suppress menu items that don't apply to a given server.
- **Repeated request-handler logic in the server `.cpp` files** → a static-inline helper in a shared `.hpp` (e.g. `ratelimit_filter.hpp`) keeps it header-only so no shared `.cpp` needs to be added to each server's `.vcxproj`.

This is consolidation of existing repetition, not speculative abstraction. Don't invent a sub-struct or helper for a single use site, and don't reshape code that only *looks* similar but is actually doing different things. But when 3–4 servers each carry the same N-line block verbatim, collapsing it is the preference.

## Adding or modifying text strings (the `text[]` array)

The runtime `text[]` array (indexed by IDs in `text.h`, accessed as `text[SomeIdentifier]`) is fed by **`ctrl/text.dat`**, which is the **single source of truth**. The textgen tool (`src/sbbs3/textgen.c`, built as `textgen.exe`) reads `text.dat` and (re)generates four files:

- `src/sbbs3/text.h` — the `enum text { ... }` of string IDs
- `src/sbbs3/text_id.c` — the parallel `text_id[]` array of identifier names
- `src/sbbs3/text_defaults.c` — the `text_defaults[]` array of compiled-in default strings
- `exec/load/text.js` — JavaScript constants for `.js` modules

**Never hand-edit any of those four files** — your changes will be overwritten the next time `textgen` runs, and they won't propagate to `text.dat` (which is what ships and what sysops may have customized).

### Workflow

1. Edit `ctrl/text.dat`. To **add** a new string, append a new line at the end (before any sentinel):

   ```
   "string content"   NNN  IdentifierName
   ```

   - `NNN` is the next sequential ID (one greater than the last). It's a comment, but keep it accurate.
   - `IdentifierName` becomes the enum identifier (referenced as `text[IdentifierName]` in C/C++ and the global `IdentifierName` constant in JS).
   - Multi-line strings: end a line with `\` (backslash) inside the quotes and continue on the next line; subsequent fragments are concatenated.
   - Inside the string, `\1X` is the byte 0x01 followed by `X` — Synchronet's Ctrl-A control codes (e.g., `\1n` = normal attr, `\1h` = high, `\1y` = yellow, `\1.` = 2-sec delay, `\1;` = 0.5-sec delay, `\1[` = CR, `\1>` = clear-to-EOL, etc., per `con_out.cpp`'s `ctrl_a()`). Ordinary C escapes (`\r`, `\n`, `\t`, `\xNN`, `\NNN`) also work.
   - To **modify** an existing string, edit the line for that ID in place.

2. Regenerate. From `src/sbbs3` (so the generated files land in this directory):

   ```
   ./msvc.Win32.exe.Debug/textgen.exe ../../ctrl ../../exec
   ```

   The first argument is the directory containing `text.dat` (the `ctrl/` dir); the second is the `exec/` dir where `load/text.js` is written. (Pick whichever build of `textgen.exe` is freshest — Win32 Debug, Win32 Release, x64 Debug, or x64 Release; they all do the same thing.) `textgen` overwrites `text.h`, `text_id.c`, `text_defaults.c` (in cwd) and `<exec>/load/text.js`.

3. Commit `ctrl/text.dat` together with the four regenerated files in the same commit.

### Read-tool gotcha

The `Read` tool refuses `text.dat` (it has a `.dat` extension and gets classified as binary, even though it's ASCII). Workaround: copy it to a `.txt` path first (e.g., `cp ctrl/text.dat /tmp/text_dat.txt`), Read/Edit the copy, then copy it back before running `textgen`.
