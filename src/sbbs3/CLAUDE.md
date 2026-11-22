## C / C++ coding style

For any C or C++ source modifications in this tree, follow the dominant coding style defined in [../uncrustify.cfg](../uncrustify.cfg). When in doubt about formatting (indentation, brace placement, spacing, alignment, line wrapping, etc.), defer to that configuration rather than inventing a new convention or matching nearby code that may already be out of compliance.

**Don't run uncrustify over a whole file that predates the config.** It is a
reformatter, not a linter: pointed at `sbbs.h` it re-indents the entire
`sbbs_t` class body (~2500 lines of pure whitespace churn) and mangles the
compact one-line inline methods — `rainbow_len()`, `timeon()`, `timeused()`,
`current_subnum()` — into broken-looking multi-line forms. It also re-aligns
unrelated declaration blocks in `.cpp` files, burying a small change in noise
and making the diff unreviewable. Header files and long-lived `.cpp` files in
this tree are generally **not** uncrustify-clean.

Format the lines you actually touched to match `../uncrustify.cfg` by hand
instead. If you do run the tool, run it on a copy and transplant only the hunks
that cover your own edit; verify with `git diff -w` that the substantive change
and the total change are the same size before committing.

An uncrustify pass (or `--check` PASS) is **not** proof of conformance — the config deliberately leaves some constructs alone that are still non-conformant. In particular, **single-line compound blocks** like `{ found = true; break; }` are not house style and must be expanded into properly-braced multi-line blocks, even though uncrustify tolerates them (its only knobs that would catch them, `nl_after_semicolon`/`nl_after_brace_open`, would also shred wanted idioms — evaluated and rejected, see below). The sanctioned exception is the compact, column-aligned **`case X: stmt; break;` switch-table idiom** (e.g. `zmodem.c`, `con_out.cpp`) — leave those tables one-row-per-case. Don't enable `nl_after_semicolon` or `nl_after_brace_open` in `uncrustify.cfg` to enforce the compound-block rule: measured across `src/sbbs3` they'd reformat ~822 lines in 57 files, almost all of it those case tables and one-line `enum {...}` declarations, and uncrustify has no case-body exemption.

## Prefer consolidation when the same lines repeat

When you notice the same cluster of fields, the same ini read/write block, or the same menu/dispatch pattern repeated across the per-server code (web/ftp/mail/services, or the various scfg dialogs, etc.), prefer to factor it into a shared form rather than leaving the duplication. Concretely:

- **Repeated field clusters in startup/config structs** → lift into a named sub-struct (e.g. `struct login_attempt_settings`, `struct max_concurrent_settings`, `struct rate_limit_settings` in `startup.h`) and embed by value. A nested struct is byte-contiguous in declared order, so this preserves the on-disk/ABI layout of the outer struct.
- **Repeated ini get/set blocks in `sbbs_ini.c`** → factor into a `get_<thing>_settings()` / `set_<thing>_settings()` helper pair, mirroring `get_login_attempt_settings` / `set_login_attempt_settings`.
- **Repeated menu logic in `scfg/`** → use a "view" struct of pointers (e.g. `struct rate_limit_cfg_view`) and one shared `<thing>_cfg()` function, plus a thin per-server wrapper that initializes the view. NULL pointers in the view can suppress menu items that don't apply to a given server.
- **Repeated request-handler logic in the server `.cpp` files** → a static-inline helper in a shared `.hpp` (e.g. `ratelimit_filter.hpp`) keeps it header-only so no shared `.cpp` needs to be added to each server's `.vcxproj`. This is the ordinary way to share code between the servers here, so don't open the header with a comment explaining that it is header-only or why ("so no project files change"); that reasoning belongs in the commit message, if anywhere. Comment only on something a reader would get wrong, such as `static` state that exists once per server module rather than once per process.

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

## Log message conventions (`lprintf()` / `errprintf()` in the servers)

### Identify the client the way the rest of that server does

A server log line starts with the socket (`%04d`), then says whose connection it is. Two bracket styles, each with one meaning:

- **`[ip-address]`** — square brackets hold a client address, and nothing else.
- **`<alias>`** — angle brackets hold a user name, and nothing else. Use it once a user is authenticated.

Never print an address inside angle brackets or a name inside square brackets — `"<%s>"` fed `host_ip`, or `user.number ? user.alias : host_ip` under a single `<%s>`, is a bug. When a line can be reached both before and after login, pick the bracket along with the value (as the `client_id` string in `mailsrvr.cpp`'s `smtp_client_thread()` and `ftpsrvr.cpp`'s `ctrl_thread()` does: `"[%s]"` with the address until login, then `"<%s>"` with the alias).

The prefix differs per server; match the file you are in:

| Server | Prefix |
|--------|--------|
| FTP (`ftpsrvr.cpp`) | `%04d [ip]` before login, `%04d <alias>` after (`[ip] <alias>` on the login line itself) |
| Mail, Web, Services | `%04d %-5s [ip]`, with `<alias>` appended once known — the `%-5s` is the protocol (`SMTP`, `POP3`, `HTTPS`, ...) |
| Terminal (`main.cpp` and `sbbs_t`) | `Node %d <alias>` in a node thread; `%04d %s [ip]` (socket, protocol) before a node is assigned |

A **hostname** is printed bare, never wrapped by the format string, and goes before the address when both appear: `%s [%s]` with `host_name, host_ip` ("logged-out from port 1234 on example.com [192.0.2.1]"), or on its own `Hostname: %s` line. The one bracketed thing you will see in a hostname's place is `<no name>` — that is the *value* (`STR_NO_HOSTNAME`, `sbbsdefs.h`) a server stores when the reverse lookup is disabled or fails, not a format. So `<no name> [192.0.2.1]` in a log is a missing hostname, not a user.

A helper that logs on behalf of a session needs the client id passed in; don't log a bare socket number because the function doesn't happen to have the address.

### The text and the severity must agree (#1396)

In every server's `lprintf()`, `level <= LOG_ERR` is what makes a line an error: it is written to `data/error.log`, counted, and raised in sbbsctrl. The word in the text does none of that.

- **The `!ERROR` marker ⇒ `LOG_ERR` or higher.** Reserve it for something the sysop should act on, and prefer `errprintf(LOG_ERR, WHERE, ...)` for those.
- **Below `LOG_ERR` ⇒ no `!ERROR` marker.** The plain word is fine as description — "cryptlib write error", "error 10054 receiving on socket" — it is the `!ERROR` flag that claims a severity the line does not have.
- **The exception is a level decided at run time.** `errprintf()` demotes a repeat of the same error (`repeated_error()`, keyed on line and function) to `LOG_WARNING`, so its `!ERROR` text is sometimes logged at warning level by design. That is fine; a fixed `lprintf(LOG_WARNING, "... !ERROR ...")` is not.
- **What a peer does is not our error.** A client disconnecting, resetting, dropping a TLS session without closing it, or sending garbage is `LOG_NOTICE` (or `LOG_WARNING` when it looks hostile), not `LOG_ERR`.

Much existing code breaks this (dozens of fixed-level `!ERROR` lines at `LOG_WARNING`); don't copy it into new lines, and fix a line's wording or level when you are already changing it. Don't mass-reword: sysops and scripts match on these strings.
