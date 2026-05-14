## C / C++ coding style

For any C or C++ source modifications in this tree, follow the dominant coding style defined in [../uncrustify.cfg](../uncrustify.cfg). When in doubt about formatting (indentation, brace placement, spacing, alignment, line wrapping, etc.), defer to that configuration rather than inventing a new convention or matching nearby code that may already be out of compliance.

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
