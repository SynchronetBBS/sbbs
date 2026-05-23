---
name: text
description: Use when a sysop wants to customize the text strings/prompts Synchronet sends to remote users — the runtime `text[]` string database. Covers `ctrl/text.dat` syntax (Ctrl-A codes, @-codes, %-specifiers, mnemonics, the decimal-`\NNN`-vs-hex-`\xNN` trap, multi-line continuation, %-vs-@-code mutual exclusion, missing-line fallback to compiled default), `ctrl/text.ini` override file (three sections: by-ID overrides, `[substr]` global substitution, `[JS]` for `gettext()` strings), `ctrl/text.<lang>.ini` per-language overlays, and the recycle-required consequence. For display files see `menus`; for `gettext()`/`js.load_text()` see `javascript`; for recycling see `control`. Trigger on "change a BBS prompt", "translate strings", "override text.dat without editing it", "what's text.ini", "retheme the BBS colors", "replace \1g with \1m everywhere" (or any global Ctrl-A code substitution), or "why didn't my text.dat change take effect".
---

# Synchronet — customizing terminal-server text strings

**Reference (authoritative):**
- `text.dat` format and syntax: https://wiki.synchro.net/custom:text.dat
- `text.ini` runtime overrides: https://wiki.synchro.net/custom:text.ini
- Localization (language overlays): https://wiki.synchro.net/custom:localization
- Ctrl-A attribute codes: https://wiki.synchro.net/custom:ctrl-a_codes
- @-codes (variable substitution): https://wiki.synchro.net/custom:atcodes
- Mnemonics (`~`-prefix highlighted keys): https://wiki.synchro.net/custom:mnemonics
- `gettext()` JavaScript helper: https://wiki.synchro.net/custom:javascript:lib:gettext.js
- Related — display files (`.ans`/`.msg`/etc.): see the `menus` skill.
- Related — JS `gettext()` / `js.load_text()`: see the `javascript` skill.
- Related — making changes take effect: see the `control` skill.

This skill is the **string-database side** of Synchronet customization. The display-file side (the full-screen `.ans`/`.msg`/`.utf8` art and menus in `text/` and `text/menu/`) belongs to `menus`. Both layers share the same Ctrl-A and @-code rendering vocabulary, but the files, workflows, and traps are different.

## The two-layer model

Every Synchronet string the Terminal Server (and other servers) sends to a user resolves through a runtime `text[]` array, indexed by string ID. The array is populated in this order:

```
       ┌─────────────────────────────────────────────────────────────┐
       │  1. COMPILED-IN DEFAULTS (text_defaults.c, baked into       │
       │     sbbs.dll / libsbbs.so by the build's textgen step)      │
       └────────────────────────────┬────────────────────────────────┘
                                    │  on startup, replaced by:
       ┌────────────────────────────▼────────────────────────────────┐
       │  2. ctrl/text.dat  (sysop's file; lines present here win    │
       │     over compiled defaults; missing/commented lines fall    │
       │     back to compiled default)                               │
       └────────────────────────────┬────────────────────────────────┘
                                    │  then optionally overlaid by:
       ┌────────────────────────────▼────────────────────────────────┐
       │  3. ctrl/text.ini  (v3.20+ runtime override; by-ID and      │
       │     [substr] and [JS] sections)                             │
       └────────────────────────────┬────────────────────────────────┘
                                    │  then per-user language overlay:
       ┌────────────────────────────▼────────────────────────────────┐
       │  4. ctrl/text.<lang>.ini  (loaded when useron.lang matches  │
       │     the LANG: value in the file)                            │
       └─────────────────────────────────────────────────────────────┘
```

**As a sysop, prefer the top of the stack that does the job.** For one-off customization, use `text.ini` (your changes survive upgrades and don't get overwritten when `text.dat` ships an update). Edit `text.dat` directly only when you're maintaining a heavily-customized BBS where the override file would be unwieldy.

**Adding entirely new string IDs is developer territory**, not sysop customization — it requires editing `text.dat` *and* C/C++ source that calls `text[NewIdentifier]`, and regenerating the auto-generated headers with the `textgen` build tool. That workflow lives in the project's developer instructions, not in this skill.

## `text.dat` — the file format

The wiki page (`custom:text.dat`) is the authoritative reference. The points below are the ones sysops most often trip over.

### Line format

```
"<value>"   <ID-number>   <IdentifierName>
```

Everything after the **terminating** double-quote is comment — the wiki notes the trailing `<ID>` and `<IdentifierName>` are conventional but not required by the parser. (For consistency and so future grepping by identifier name works, leave them in place when you modify a line.)

### Escape sequences

| Sequence | Means |
|----------|-------|
| `\\`, `\?`, `\'`, `\"` | literal `\`, `?`, `'`, `"` |
| `\xNN` | byte by **hexadecimal** (`\x00`..`\xFF`) |
| `\NNN` | byte by **decimal** (`\000`..`\255`) — note: **not octal**, unlike C |
| `\r` `\n` `\t` `\b` `\a` `\f` `\v` | the usual ASCII control characters |
| `\1X` | byte `0x01` followed by `X` — this is the **Ctrl-A code** form (`\1n` = normal, `\1h` = high-intensity, `\1c` = cyan, etc., see the Ctrl-A wiki) |

**The trap.** `\NNN` is **decimal**, not octal. The wiki calls this out directly: writing `"\14"` to try to set background color 4 (`\1` is Ctrl-A, `4` should be a parameter) doesn't do what you'd guess — it's parsed as decimal `14`, which is a Ctrl-N (shift-out). Use `"\x014"` instead — hex `\x01` (Ctrl-A) followed by literal `4`. Most stock `text.dat` lines use the `\1X` shorthand for Ctrl-A; reach for `\xNN` only when you need an arbitrary byte.

### Multi-line strings

A trailing `\` *after* the closing double-quote continues the string on the next line:

```
"\1n\1h\1c\xda\xc4\xc4\xc4\xc4\xc4\xc4"\
    "\1n\xc4\xc4\xc4\xc4\xc4\xc4"\
    "\1h\1k\xc4\xc4\xc4\xc4\xc4\xc4\xc4"
```

The strings concatenate. **Max line length** is 255 chars; **max total string length** is 2000 chars.

### Mnemonics (`~`-prefix)

A `~` before a character marks it as a command key. On ANSI terminals it gets highlighted; on non-ANSI it gets parenthesised:

```
"~Yes, ~No, or ~Quit: "
```

becomes `Yes, No, or Quit: ` with `Y`/`N`/`Q` highlighted, or `(Y)es, (N)o, or (Q)uit: ` on non-ANSI. Colours come from `ctrl/attr.cfg`. See `custom:mnemonics`.

### printf %-specifiers

Many lines embed dynamic content via `%s`, `%d`, `%u`, etc. — fed by the calling C code. **Don't reorder, add, or remove them** when modifying a line; the C code expects exactly the specifiers in the order they appear.

To **suppress** a `%s` argument from display without removing it (so the C code's argument list stays balanced): change `%s` to `%.0s` (precision-zero — prints zero characters of the string).

```
"User: %s, Level: %d"     → keeps both
"User: %.0s, Level: %d"   → hides the username, still consumes the %s argument
```

### Mutually exclusive: %-specifiers and @-codes

**Lines that contain `%`-specifiers cannot also contain @-codes** (security rule, called out on the `custom:text.dat` wiki page). If you need both dynamic data and an @-code, use one of them — typically replace the `%s` with an @-code if the value is exposed via the @-code system, or vice versa.

### Suppressing a whole line

Setting a `text.dat` entry to an empty string `""` suppresses display of that line at runtime (most call sites tolerate an empty string).

### Missing / commented lines

Any string commented out with `#` (or simply missing from the file) is replaced by the compiled default from `text_defaults.c`. This means a *minimal* `text.dat` containing only your modifications is perfectly valid — you're not required to keep every stock string.

### Tooling gotcha — editors that classify `.dat` as binary

`ctrl/text.dat` is plain ASCII, but tools that classify by file extension (e.g. some AI-coding tooling's Read/Edit tools that refuse `.dat`) won't open it. Workaround:

```
cp $SBBS/ctrl/text.dat /tmp/text_dat.txt
# edit /tmp/text_dat.txt
cp /tmp/text_dat.txt $SBBS/ctrl/text.dat
```

(Some sysops keep `ctrl/text.txt` as a working copy for the same reason.)

## `text.ini` — the v3.20+ override file (recommended for sysop customization)

The wiki page (`custom:text.ini`) is the authoritative reference. The bullet form below summarises the file's three independent sections.

`text.ini` is optional. If absent or empty, the runtime uses `text.dat` (or the compiled default for missing entries) as-is. If present, its contents are layered on top of `text.dat` at startup.

**Why prefer `text.ini` over editing `text.dat`:**

- Your customizations **survive Synchronet upgrades** — `text.dat` may ship updates; `text.ini` is sysop-owned and never overwritten.
- **Order doesn't matter** — group your overrides by topic, not by stock-file order.
- **Only your changes need to live in the file** — no need to keep a full copy of the stock strings.
- Customizations also reach **other servers** (not just the Terminal Server), which Baja/JS-based runtime overrides can't.

### Section 1: by-ID overrides (no `[…]` header)

The default section at the top of the file overrides individual `text.dat` strings by **identifier name**:

```ini
; ctrl/text.ini

Email: "\1_\1?\1c\1hE-mail (User name or number): \1w"

; without quotes also works, but quotes preserve leading/trailing whitespace
NoDOS = This BBS can't run DOS doors; connect via SSH instead.
```

- The key is the **identifier name** from `text.dat` (e.g. `Email`, `NoDOS`, `NodeStatusWaitingForCall`), not the numeric ID.
- `ID: value` syntax (colon) is required if the value contains control characters (Ctrl-A, embedded escapes); `ID = value` (equals) is fine for plain text. See `config:ini_files#string_literals`.
- Double-quotes around the value are optional but **required** to preserve leading/trailing whitespace.
- **No multi-line continuation** — each override is a single line, max 1023 chars.
- The same escape sequences as `text.dat` apply (`\1X` for Ctrl-A, `\xNN` for hex bytes, `\r`/`\n`, etc.).
- Lines starting with `;` are comments.

### Section 2: `[substr]` — global substring substitution

```ini
[substr]
\1g: \1m
Group: Area
"old phrase": "new phrase"
```

Every string that goes out to a user — from any source (`text.dat`, `text.ini` overrides, JavaScript-built output) — is passed through these substitutions before being sent. The example above:

- Replaces every Ctrl-A green (`\1g`) with Ctrl-A magenta (`\1m`) — a quick way to retheme without touching individual entries.
- Replaces every occurrence of the word `Group` with `Area`.

**The trap: `[substr]` is global and unconditional.** Every output line is rewritten. Useful patterns:

- Wholesale colour retheming (e.g. green→magenta, blue→cyan).
- Renaming a term consistently (e.g. "Sysop" → "Admin", "Sub-board" → "Forum").
- Suppressing a recurring phrase (replace with empty).

Risky patterns to avoid:

- **Replacing common short tokens.** Replacing `to` with `for` will hit every "to" anywhere — including inside other words ("automatic" → "auformatic"). Use longer, more specific patterns.
- **Case-sensitive substitution.** Matches are case-sensitive; `Group` won't match `group` or `GROUP`.
- **Quoting matters for whitespace.** If you want to substitute literal whitespace, quote the value.

### Section 3: `[JS]` (also `[default.js]`) — JavaScript `gettext()` overrides

For strings emitted from JavaScript via `gettext()`:

```ini
[JS]
Find Text in Messages = Find a string in messages
You have @USERMAIL@ message(s) waiting. = You've got mail!
```

- The key is the **literal English source string** as passed to `gettext()`.
- For this to take effect, the script must `require("gettext.js")` (or `load("gettext.js")` per the older idiom) — see `custom:javascript:lib:gettext.js` and the `javascript` skill.
- A `[default.js]`-style section can scope overrides to a specific JS module (`default.js`, the main shell); other module-named sections work similarly.

## `text.<lang>.ini` — per-language overlays

For non-English users, the runtime loads `ctrl/text.<lang>.ini` when the user's `useron.lang` matches the file's `LANG:` declaration. The format is identical to `text.ini`: by-ID overrides, `[substr]`, `[JS]` — all in the target language.

Stock examples ship for German, Spanish, and French (`text.de.ini`, `text.es.ini`, `text.fr.ini`). They start with:

```ini
LANG: Deutsch
Language: Sprache
On: Auf
Off: Aus
Yes: Ja
No: Nein
...
```

The `LANG:` line is the human-readable language name that the user sees when selecting a language; the rest are per-ID overrides.

**Language selection** is per-user — see `custom:localization` for the user-side language preference flow and the parallel `text/menu/<lang>/` directory for translating *display files* (which is the `menus` skill's territory).

CP437 is the native character set; the wiki's localization page is honest about the limits for non-Western scripts and the UTF-8 work-in-progress.

## Making changes take effect

`text.ini` (and `text.<lang>.ini`) overrides are loaded **at server startup**. After editing, you need to either:

- **Recycle the server(s)** that use the strings (Terminal Server for almost everything, other servers if your overrides include their strings). The cross-platform way: `touch $SBBS/ctrl/recycle` (all servers) or `touch $SBBS/ctrl/recycle.term` (just the Terminal Server). See the **`control`** skill for the full menu of mechanisms.
- **Have users log off and back on** — minimum requirement; some text is only loaded once per server lifetime, but per-session text is re-read on each login.

`text.dat` edits work the same way — startup parses the file, so a recycle is required for changes to be visible.

## Quick recipes

**"Change the colour of the e-mail prompt from bright blue to bright cyan."**

```ini
; ctrl/text.ini  (default section, top of file)
Email: "\1_\1?\1c\1hE-mail (User name or number): \1w"
```

Then `touch $SBBS/ctrl/recycle.term`.

**"Replace every occurrence of 'Group' with 'Area' in every BBS message."**

```ini
; ctrl/text.ini
[substr]
Group: Area
```

**"Retheme green to magenta system-wide."**

```ini
[substr]
\1g: \1m
```

**"Translate a single JavaScript prompt without touching any .js file."**

```ini
; ctrl/text.de.ini   (German overlay)
LANG: Deutsch
...

[JS]
Press any key to continue = Eine Taste drücken zum Fortfahren
```

The JS code must use `gettext("Press any key to continue")` rather than the bare string for this to apply.

**"Find the identifier name for a stock prompt I want to override."**

```
grep -F 'the prompt text I see' $SBBS/ctrl/text.dat
```

The line will end with the ID and IdentifierName. (If your `text.dat` is minimal, grep `src/sbbs3/text_defaults.c` in the source tree instead — those are the compiled defaults.)

## Common mistakes

- **Editing `text.dat` for a single colour tweak when `text.ini` would do.** The next Synchronet update may overwrite your `text.dat` changes if a new line is added or fixed; `text.ini` survives. The wiki's `custom:text.ini` page is direct about this preference.
- **`"\14"` for "Ctrl-A code 4".** It's not — `\NNN` is decimal, so `\14` is the byte `0x0E` (Ctrl-N), not Ctrl-A + `4`. Use `"\x014"` or `"\1" "4"` instead.
- **Adding @-codes to a line that contains `%s`/`%d`.** The two are mutually exclusive; the @-code won't be expanded. Pick one or split the line into pieces.
- **Reordering `%`-specifiers.** Each one is consumed by the calling C code's printf arguments in order. Reorder them and you'll print the wrong values or crash.
- **`[substr]` replacements that are too short.** `to: for` will hit every "to" anywhere — inside other words, in URLs, in user names. Use distinctive multi-character tokens.
- **Forgetting to recycle.** `text.ini` is loaded at startup. After editing, `touch $SBBS/ctrl/recycle` (or the per-server variant) — see `control`.
- **Multi-line value in `text.ini`.** Values must be on one line, max 1023 chars. The `\`-continuation that works in `text.dat` does **not** work in `text.ini`.
- **Using `ID = value` for a value with control characters.** Use `ID: value` (colon) instead — see `config:ini_files#string_literals` for why.
- **Expecting `gettext()` overrides to "just work".** The script must `require('gettext.js')` and call `gettext(theString)`. A bare string literal in JS isn't routed through the override table.
- **Translating only `text.<lang>.ini` and forgetting `text/menu/<lang>/`.** The two cover different surfaces — prompts vs. display files. Translate both for a coherent non-English experience. (See `menus`.)
- **Confusing the `text.dat` identifier with the numeric ID.** `text.ini` uses the **identifier name** (e.g. `Email`), not the number (e.g. `010`). The number is a comment; the identifier is the key.

## Cross-references

- **Display files** (`.ans`, `.msg`, `.asc`, `.rip`, `.utf8` in `text/` and `text/menu/`; `data/subs/<code>.*` info files; `mods/text/` overrides; per-language `text/menu/<lang>/`): the **`menus`** skill.
- **JavaScript-emitted strings** (`gettext()`, `js.load_text()`, the JS dialect): the **`javascript`** skill.
- **Making your changes take effect** (`ctrl/recycle`, `ctrl/recycle.term`, signals, MQTT control topics): the **`control`** skill.
- **Where the runtime writes about loading these files** (look for "Loading text.ini" lines on startup): the **`logs`** skill — console stream / syslog / journal.
- **Source of truth in the codebase**: `src/sbbs3/load_cfg.c` reads `text.ini` at startup; `src/xpdev/ini_file.c` is the parser. The compiled defaults live in the auto-generated `src/sbbs3/text_defaults.c` (built from `text.dat` by `textgen.c`).
