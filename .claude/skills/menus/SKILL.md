---
name: menus
description: Use when authoring or modifying Synchronet Terminal Server display/menu files — anything in `text/`, `text/menu/`, `mods/text/`, `mods/text/menu/`, `data/subs/<code>.*`, `data/dirs/<code>.*`. Covers Ctrl-A (^A) attribute codes, @-code message variables, the file-extension priority by terminal type (rip/ans/mon/asc/msg/seq/utf8), .Xcol/.cX width variants, language overlays, security gating, mouse hotspots, and the C++/JS/Baja entry points that render these files. For the runtime string database (`ctrl/text.dat`, `ctrl/text.ini`, `ctrl/text.<lang>.ini`) see the `text` skill. Trigger on tasks like "add help menu file", "fix CGA brown color", "show this only to sysops", "make this prompt work on PETSCII", "what @-codes are available in this context", "why isn't my .ans file being picked up", "wire @SHOW:LEVEL40@", or any work that touches the look/content of what a remote BBS user sees.
---

# Synchronet menu and display files

**Reference (authoritative):**
- Ctrl-A codes: https://wiki.synchro.net/custom:ctrl-a_codes
- @-codes (message variables): https://wiki.synchro.net/custom:atcodes
- Text file locations: https://wiki.synchro.net/config:text_files
- Menu file conventions: https://wiki.synchro.net/custom:menu_files
- Related — the runtime string database (`ctrl/text.dat`, `ctrl/text.ini`, language overlays): see the **`text`** skill.

This skill describes the **display-file side** of customization: what a sysop sees inside `text/`, `text/menu/`, and the related `mods/text/` overrides — the full-screen `.ans` / `.msg` / `.utf8` / `.rip` files, plus the smaller `.msg` / `.asc` snippets — and how those bytes turn into pixels on a remote user's terminal. The runtime **string database** (`ctrl/text.dat` and its `text.ini` runtime overrides — the per-prompt one-liners that the BBS code emits via `text[ID]`) is covered by the **`text`** skill; `text.dat` is mentioned here only where the rendering machinery is shared (Ctrl-A codes, @-codes including `@TEXT:N@` and `@MENU:foo@`).

Building / running the BBS itself is covered by the `synchronet-build` and `jsexec` skills.

## File-system layout

```
<sbbs>/
├── ctrl/text.dat         # all hard-coded BBS prompts/strings (one record per line)
├── text/
│   ├── *.msg / *.ans     # login flow + special screens (answer, welcome, sbbs, feedback…)
│   ├── *.can             # trash-can filter files (name.can, file.can, ip.can…)
│   ├── menu/             # the meat: per-shell, per-section menus and help screens
│   │   ├── <lang>/       # optional language overlay (matches useron.lang)
│   │   ├── main.*        # main menu (default.src shell)
│   │   ├── mailread.*    # reading-mail menu
│   │   └── …
│   └── QWK/              # files packed into outgoing .qwk packets
├── data/
│   ├── subs/<code>.*     # info file for a sub-board (used by 'I' / 'IS')
│   └── dirs/<code>.*     # info file for a file directory (used by 'ID')
└── mods/
    └── text/             # sysop overrides; takes priority over <sbbs>/text/
        └── menu/
```

`text/` ships with Synchronet and is overwritten on update. **`mods/text/` and `mods/text/menu/` are checked first** when both exist — that's where a sysop puts their customizations so a `pull` / installer won't blow them away. (Configured under SCFG → System → Advanced Options → MODS directory.)

## File-extension priority by terminal type

A single base name (e.g. `main`) may have multiple variants on disk. `menu()` walks the candidates in this order and stops at the first match:

| User's terminal           | First                        | Then                                                  |
|---------------------------|------------------------------|-------------------------------------------------------|
| RIPscrip                  | `.rip`                       | → `.ans` → `.msg` → `.asc`                            |
| ANSI / CP437 colour       | `.ans`                       | → `.msg` → `.asc`                                     |
| ANSI / CP437 monochrome   | `.mon`                       | → `.ans` → `.msg` → `.asc`                            |
| ANSI / ASCII colour       | `.ans`                       | → `.asc` → `.msg`                                     |
| ANSI / ASCII monochrome   | `.mon`                       | → `.ans` → `.asc` → `.msg`                            |
| PETSCII                   | `.seq`                       | → `.msg` → `.asc`                                     |
| TTY / CP437               | `.msg`                       | → `.asc`                                              |
| TTY / ASCII               | `.asc`                       | → `.msg`                                              |
| UTF-8                     | `.utf8`                      | → (falls back through the matching above)             |

Rule of thumb when authoring **one** file for everyone: name it `foo.msg` (CP437 + Ctrl-A) or `foo.asc` (plain ASCII + Ctrl-A) and skip the rest. Ctrl-A codes are stripped automatically on terminals that don't render them. **Design that single file to display legibly on a 40-column terminal when possible** — see "Designing for narrow terminals" below. A user on a 40-col client (mobile telnet, retro hardware) will see the same file an 80-col user does unless you ship `.40col.<ext>` / `.c40.<ext>` width-specific variants alongside it, and those add maintenance cost.

### Width-specific variants

| Pattern              | Meaning                                                  |
|----------------------|----------------------------------------------------------|
| `foo.40col.ans`      | **Exact** 40-column terminal                             |
| `foo.80col.ans`      | **Exact** 80-column terminal                             |
| `foo.c80.ans`        | **Minimum** 80 columns (widest `.cX` ≤ user width wins)  |
| `foo.ans`            | Default fallback                                         |

Exact-width files are preferred over minimum-width which is preferred over the generic file.

### Language overlay

If `useron.lang` is set and the menu code doesn't already contain `/`, `menu()` tries `<lang>/<code>.<ext>` first under both `text/menu/` and `mods/text/menu/`. Localized menus live in `text/menu/<lang>/` (and the matching `text.<lang>.ini` lives in `ctrl/`).

### Random menus

`logon*.{ans,msg,…}` and a `random*` glob — `menu()` picks one at random when the code contains `*` or `?`.

## Where files actually get rendered (call sites)

| From            | Call                                              | Notes                                                              |
|-----------------|---------------------------------------------------|--------------------------------------------------------------------|
| C++ (sbbs)      | `sbbs->menu("code", mode, jsobj)`                 | Resolves under `text/menu/`, applies all the priority above        |
| C++ (sbbs)      | `sbbs->printfile(path, mode, org_cols, jsobj)`    | Lower-level — pass an absolute or full-relative path               |
| C++ (sbbs)      | `sbbs->menu_exists(code, ext, outpath)`           | Probe without rendering — useful for conditional UI                |
| JavaScript      | `bbs.menu("code", mode, scope_obj)`               | Same lookup rules as the C++ version                               |
| JavaScript      | `console.printfile("path", mode)`                 | Lower-level path-based                                             |
| JavaScript      | `console.printtail("path", lines, mode)`          | Tail-only display                                                  |
| Baja (PCMS)     | `MNU code`                                        | Compiles to `menu()` under the hood                                |
| text.dat record | `@MENU:foo@`, `@CONDMENU:foo@`                    | Inline include of a menu file from a `text.dat` string             |
| text.dat record | `@INCLUDE:path@`, `@TYPE:path@`                   | Inline include of an arbitrary file                                |
| Mode flags worth knowing on `printfile` / `menu`: `P_NOABORT`, `P_NOATCODES`, `P_NOCRLF`, `P_NOERROR`, `P_OPENCLOSE`, `P_CPM_EOF`, `P_HONOR_PAUSE`, `P_NOPAUSE` (see `sbbsdefs.js` / `sbbs.h`). | | |

The default `menu()` modes already set `P_OPENCLOSE | P_CPM_EOF`. Pass `P_NOERROR` to suppress the "file not found" error if a menu is optional.

## Ctrl-A codes (^A)

A Ctrl-A code is a 2-byte sequence: the literal byte `0x01` (Ctrl-A) followed by one operand char. In `text.dat` and JS/Baja source they appear as `\1X` or `\x01X` — the runtime stuffs the actual `0x01` byte at load time. In binary `.msg` / `.ans` files the byte is literal (see `cat -v` → `^A`).

> **For BBS-wide Ctrl-A colour substitution** — *"retheme every `\1g` to `\1m`"*, or any one-operand-to-another rewrite that should apply to every string the BBS emits without you editing files individually — **don't use `attr.ini`.** `attr.ini` only repaints named colour *slots* (`userhigh`, `nodestatus`, `chatlocal`, etc.) used by specific code paths; literal `\1G` bytes embedded in `.msg`/`.ans`/`text.dat` lines are dispatched directly by `con_out.cpp`'s `ctrl_a()` and **cannot be remapped by `attr.ini`** — which is what trips most sysops who grep their `text/` tree, find hundreds of hard-coded `^AG`s, and conclude "there's no global knob." There is — it's just in a different file.
>
> Add this to `ctrl/text.ini`:
> ```ini
> [substr]
> \1g: \1m
> ```
> Then `touch ctrl/recycle.term` to load it. The `[substr]` substitution is applied to every string going out to a user, regardless of source (`.msg` file, `.ans` file, `text.dat` line, JS-built output) — so a single line reaches everywhere `^AG` appears. Repeat for `\1G`/`\1H\1G` if you want bright green → bright magenta too.
>
> For the full `[substr]` traps (case-sensitive matches, the don't-substitute-short-tokens warning, the global-applies-to-*everything* implication) and the rest of `text.ini` — `[JS]` overrides, per-language overlays, the by-ID override section — see the **`text`** skill.

The one exception is the file-embed form: `Ctrl-A "filename` (the `"` operand, then a filename terminated by another Ctrl-A or end-of-line).

### Colors

| Color           | Fg `^A` | Bg `^A` |
|-----------------|---------|---------|
| Black           | `K`     | `0`     |
| Red             | `R`     | `1`     |
| Green           | `G`     | `2`     |
| Yellow / Brown  | `Y`     | `3`     |
| Blue            | `B`     | `4`     |
| Magenta         | `M`     | `5`     |
| Cyan            | `C`     | `6`     |
| White / Gray    | `W`     | `7`     |

### Attributes

| Code | Meaning                                                                              |
|------|--------------------------------------------------------------------------------------|
| `H`  | High intensity (sticky — stays until `N`)                                            |
| `I`  | Blink (slow), terminal-permitting                                                    |
| `E`  | Bright background (iCE colors) — v3.17c+                                             |
| `f`  | Set blink **only if** the alternate Blink-font is active (v3.17+)                    |
| `F`  | Set blink **only if** alternate High-Intensity Blink-font is active (v3.17+)         |
| `N`  | Normal — resets both High and Blink to defaults (light gray)                         |
| `-`  | Optimized normal: only resets if High/Blink/iCE bg is set (or pops attr stack)       |
| `_`  | Optimized normal: only resets if Blink or iCE bg is set                              |
| `+`  | Push current attributes onto LIFO stack                                              |

`-` does double duty: with a non-empty attribute stack it pops; with an empty stack it's optimized-normal. `_` is unconditional optimized-normal (resets only blink/iCE-bg, never pops).

| `X`  | Rainbow (wraps) — uses `attr.ini` `rainbow` key                                      |
| `x`  | Rainbow (repeat last) — `attr.ini` `rainbow` key                                     |
| `U`  | Sysop user-defined high (`userhigh` in `attr.ini`)                                   |
| `u`  | Sysop user-defined low (`userlow` in `attr.ini`)                                     |
| `V`  | Mnemonic high (`mnehigh`)                                                            |
| `v`  | Mnemonic low (`mnelow`)                                                              |

### Control

| Code   | Meaning                                                                            |
|--------|------------------------------------------------------------------------------------|
| `P`    | Pause prompt ("Hit a key")                                                         |
| `Q`    | Reset auto-pause line counter                                                      |
| `L`    | CLS + clear hotspots + home cursor                                                 |
| `'`    | Home cursor (no clear)                                                             |
| `,`    | Delay 0.1 s                                                                         |
| `;`    | Delay 0.5 s                                                                         |
| `.`    | Delay 2.0 s                                                                         |
| `J`    | Clear to end of screen                                                             |
| `>`    | Clear to end of line                                                               |
| `<`    | Non-destructive backspace (cursor left)                                            |
| `[`    | CR (cursor to column 1)                                                            |
| `]`    | LF (cursor down)                                                                   |
| `/`    | Conditional newline — emits CRLF only if not already at column 1 (v3.17+)          |
| `\`    | Conditional line-continuation prefix (`LongLineContinuationPrefix` in text.dat)    |
| `?`    | Conditional blank line — emits a blank line only if previous wasn't blank          |
| `~X`   | Hungry mouse hotspot (single-char command `X`)                                     |
| `` `X`` | Strict mouse hotspot (single-char command `X`)                                    |
| `A`    | Emit a literal Ctrl-A byte                                                         |
| `z`    | Emit a literal Ctrl-Z (v3.17c+)                                                    |
| `Z`    | Premature end-of-file (stop rendering)                                             |
| `D`    | Macro: current system date                                                         |
| `T`    | Macro: current system time                                                         |
| `S`    | Sync node status (calls `nodesync()`)                                              |
| `"name`| Embed file `text/name` inline (until next Ctrl-A or EOL)                           |
| 128-255| Move cursor right (`code - 127`) columns                                           |

### Security gating

These hide / show downstream text based on the current user. The level codes (`!@#$%^&*(`) are documented toggles — emitting the same code a second time turns visibility back. `)` re-enables display for everyone unconditionally. The `A`–`Z` flag codes show following text only to users with that Flag Set #1 flag set.

| Code              | Gate                                                              |
|-------------------|-------------------------------------------------------------------|
| `A`–`Z`           | Show following only to users with Flag Set #1 flag `A`–`Z`        |
| `!`               | Toggle off/on for users < level 10                                |
| `@`               | Hide from users < level 20                                        |
| `#`               | Hide from users < level 30                                        |
| `$`               | Hide from users < level 40                                        |
| `%`               | Hide from users < level 50                                        |
| `^`               | Hide from users < level 60                                        |
| `&`               | Hide from users < level 70                                        |
| `*`               | Hide from users < level 80                                        |
| `(`               | Hide from users < level 90                                        |
| `)`               | Restore display to ALL users                                      |

Note: `^A` followed by `A` is overloaded — in the colour table it means "emit literal Ctrl-A", but in security context it means "show only if flag A". The parser disambiguates by what's currently in flight. Don't lean on this; use `@SHOW:ars@` (below) for non-trivial gating.

### Sticky-high-intensity and the CGA-brown gotcha

- Setting `^AH` once makes **every subsequent color change bright until `^AN`** resets it. So `^AH^AY` and `^AY^AH` both produce bright yellow; bare `^AY` after a reset produces **CGA brown** (dim yellow).
- When you need a multi-color prompt without re-pairing `^AH`, set `^AH` early and then ride it: `^AH^AYText (^AW?^AY=help): ^AN`.
- Background colors `0`–`7` are not affected by `H`; use `^AE` for bright backgrounds (iCE colors).

## @-codes (message variables)

@-codes are `@NAME@` macros expanded **at display time** by the runtime. They work in:
- `text.dat` strings,
- `text/menu/*` files (every supported extension),
- `text/*.msg` login-flow files,
- email and sub-board messages **only if posted by user #1** (the sysop) — otherwise left as plain text,
- explicit `bbs.atcode("CODE")` JS calls.

The code name is **always uppercase** (e.g. `@SYSOP@`, not `@Sysop@`). Exception: text-string IDs from text.dat used as @-codes preserve their mixed-case spelling.

### Format modifiers

Append after the code name with `-` (one modifier, no params) or `|` (multiple modifiers OK, params via `:` OK):

| Modifier | Effect                                                                  |
|----------|-------------------------------------------------------------------------|
| `L`      | Pad/left-justify                                                        |
| `R`      | Pad/right-justify                                                       |
| `C`      | Pad/center (v3.17b+)                                                    |
| `W`      | Display double-wide (Unicode fullwidth or padded)                       |
| `Z`      | Zero-pad and right-justify (v3.17b+)                                    |
| `T`      | Thousands-separated number (v3.17c+)                                    |
| `U`      | Force uppercase (v3.18a+)                                               |
| `>`      | Allow terminal-wrap (don't truncate) overly-long expansions             |

Width comes after the modifier as a decimal number (`@NODE|L2@`) or as a string of non-numeric non-space placeholder characters (`@ALIAS|R##############@` for a 14-wide right-justified alias).

### Param-bearing codes (use `:` after the code)

| Suffix    | Means                                                                     |
|-----------|---------------------------------------------------------------------------|
| `[:b]`    | Byte-count format: `B`/`K`/`M`/`G`/`V`(verbal)                            |
| `[:d]`    | Duration format: `F`/`!`/`A`/`T`/`B`/`V`/`C`/`W`/`D`/`S`/`M`              |
| `:t`      | strftime format string (use `\x20` for space, `\x3A` for `:`)             |

Example: `@DATE:%Y-%m-%d\x20%H:%M:%S@` → `2026-05-17 12:34:56`.

### Frequently-used @-codes (representative, not exhaustive)

**System / dynamic:**
`@VER@` `@VER_NOTICE@` `@BBS@` `@SYSOP@` `@INETADDR@` `@LOCATION@` `@SERVED@` `@NODE@` `@TNODES@` `@DATE@` `@TIME@` `@DATETIME@` `@UPTIME[:d]@`

**Current user:**
`@ALIAS@` `@REALNAME@` `@FIRST@` `@USERNUM@` `@SEC@` `@TIMELEFT@` `@MAILW@` `@MAILU@` `@CREDITS[:b]@` `@LASTON[:t]@` `@CALLS@`

**Current message context:**
`@MSG_FROM@` `@MSG_TO@` `@MSG_SUBJECT@` `@MSG_DATE@` `@SMB_SUB@` `@SMB_AREA@` `@CONF@`

**Current file context:**
`@FILE_NAME@` `@FILE_DESC@` `@FILE_SIZE@` `@FILE_AREA@` `@LIB@` `@DIR@`

**Terminal control / formatting:**
`@CLS@` `@CLEAR@` `@PAUSE@` `@CRLF@` `@CENTER@` `@WRAP@` `@WRAPOFF@` `@TRUNCATE@` `@TRUNCOFF@` `@COLS@` `@ROWS@` `@CHARSET@` `@TERM@` `@HANGUP@`

**Cursor:**
`@HOME@` `@UP:n@` `@DOWN:n@` `@LEFT:n@` `@RIGHT:n@` `@GOTOXY:x,y@` `@POS:x@` `@PUSHXY@` `@POPXY@` `@CLR2EOL@` `@CLR2EOS@` `@CLRLINE@`

**Special characters (terminal-aware):**
`@CHECKMARK@` `@COPY@` `@TRADEMARK@` `@ELLIPSIS@` `@DEGREE_C@` `@DEGREE_F@` `@U+20AC@` (any Unicode codepoint)
`@U+code:fallback@` — explicit fallback string for non-UTF-8 terminals.

**Gating / conditional:**
`@SYSONLY@` — toggle off/on for non-sysops
`@SHOW:ars@` — show following only if user matches ARS (e.g. `@SHOW:LEVEL40@`)
`@SHOW@` — turn display back on for all users
`@ONOFF:ars@` — emit "On" or "Off" by ARS
`@YESNO:ars@` — emit "Yes" or "No" by ARS

**External content:**
`@MENU:foo@`, `@CONDMENU:foo@` — inline render another menu file
`@TYPE:path@`, `@INCLUDE:path@` — inline render an arbitrary file
`@EXEC:module arg arg@` — run a Baja/JS module in-process
`@TEXT:N@` — emit text.dat record N
`@Text-ID@` — emit a text.dat record by its symbolic ID (e.g. `@SearchStringPrompt@`)
`@!X@` — execute one or more Ctrl-A operands (e.g. `@!HY@` ≡ bright yellow)
`@AT@` — literal `@` (v3.20d+)
`@NOCODE@` — disable further @-code parsing in this file

**Hot-spots / mouse:**
`@HOT@` — colorize mouse hotspots in the following text using the @-code's own color
`@HOT:HUNGRY@` / `@HOT:STRICT@` — same, choosing hotspot type
`@HOT:OFF@` — disable hotspot generation
`@~text@` / `@~text~cmd@` — define a hungry hotspot with optional alternate command
`` @`text@ `` / `` @`text`cmd@ `` — same, strict variant

**Rainbow:**
`@RAINBOW@` / `@RAINBOW:OFF@` / `@RAINBOW:RAND@` / `@RAINBOW:index@` / `@RAINBOW:R:RH:B:BH@` (custom pattern)

The wiki page has the complete, current list — when in doubt, fetch the raw page (`?do=export_raw`) and grep.

## Common style patterns to mirror

### Framed-menu pattern (the dominant style in `text/menu/`)

Almost every "menu" file in `text/menu/` follows one structure — see `multchat.msg`, `qwk.msg`, `maincfg.msg`, `mailread.msg`, `transfer.40col.msg`:

```
^An^Ah^Ac@HOT@^An@CLEAR@
^A4^AC┌──────────────────────────────^AK┐^A0
^A4^AC│  ^AH^AYMenu Title  ^AN^A4^AK│^A0
^A4^AC└^AK──────────────────────────────┘^A0^AB▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄^AK
^AB█  ^AH^AC X ^AY ^AN^ACDescription of option^AW^A<binding>^AB█^AK
^AB█  ^AH^AC Y ^AY ^AN^ACAnother option         ^AW^A<binding>^AB█^AK
...
^AZ
```

Key bits:
- `@HOT@`/`@CLEAR@` at top (hotspot color + clear screen).
- CP437 box-drawing (┌─┐ │ └─┘ ▀▄█▌▐) for the frame; right-edge shadow via `^AB█` then `▀`/`▄` shadow row.
- Title in bright yellow (`^AH^AY`).
- Each entry: bright-cyan letter + spaces + `^AN^AC` description + a single `^A<byte>` (binding byte 128–255 stuffs that keycode into the input buffer if the user clicks the hotspot or presses a configured equivalent).
- Trailing `^AZ` — wait-for-key (premature EOF).

Use this pattern when adding a new menu under `text/menu/` that needs visible options and hotkeys.

### Narrative info-screen pattern

Free-form text with sparse Ctrl-A for emphasis. Used by `text/feedback.msg`, `text/inactive.msg`, `text/sbbs.msg`, etc. Appropriate for help screens, system info dumps, anything without single-letter hotkeys. No frame, no `@HOT@`, no trailing `^AZ`.

Pick this only for files that live in `text/` (not `text/menu/`) or for help screens explicitly invoked between prompts.

## Authoring practicalities

### Designing for narrow terminals

When only **one** menu file will exist (no `.40col` / `.c40` siblings), prefer a layout that fits in **40 columns** so PETSCII, mobile telnet clients, and retro hardware see something legible. Two specific techniques:

1. **`^A\` conditional line continuation.** This Ctrl-A code emits the
   `LongLineContinuationPrefix` string from `text.dat` **only when the user's
   terminal is < 80 columns**. Use it at line breaks in long-line content so
   the same source renders as one logical line on 80-col terminals and wraps
   cleanly on 40-col ones, without you maintaining two files. Example:

   ```
   ^AyChoose: ~A^Aw)~A^Andd, ~^AwR^Anemove, ~^AwE^Andit, ~^AwV^Aniew,^A\
   ~^AwQ^Anuit, ~^AwP^Anrev or [~^AwN^Anext]: 
   ```

2. **Don't write into or past the last column.** Synchronet emits whatever
   you wrote; if the cursor reaches column N+1 on an N-column terminal,
   nearly every terminal **auto-wraps** to the next row, throwing your
   subsequent layout off by one. Leave column N empty, or end the line with
   a `^A\` continuation, or use a `@>@` modifier on the final @-code when
   you genuinely want to allow wrap.

### Counting rows and columns correctly

When you size a menu file (e.g. picking a row count that fits in a 25-line
window, or making sure a line doesn't auto-wrap), what matters is the
**rendered width and rendered row count**, not the byte count of the source:

- **Ctrl-A codes consume zero visible columns and zero rows.** `^AH^AY` is
  4 bytes in the file, 0 columns on screen. The text editor's column ruler
  is wrong; count the visible characters only.
- **@-codes expand at display time** to a string whose width depends on the
  current user, current node, current message, etc. `@ALIAS@` could be 2
  chars or 25. To make the layout predictable, pin the width with a
  format modifier: `@ALIAS|L12@` (12 chars, left-padded) or `@CREDITS|R8@`.
- **CP437 box-drawing chars are one column each**, regardless of editor
  display width.
- **Some @-codes emit row-changing escapes** (`@CRLF@`, `@CLS@`, `@PAUSE@`,
  `@GETKEY@`, `@CENTER@`, `@CONTINUE@`, embedded `@MENU:…@` or `@INCLUDE:…@`).
  These can add (or zero out) rows you didn't write in the source. When
  the row count matters (e.g. fitting under a header), expand mentally or
  test against a real session.
- **Width-variant files (`.40col`, `.c80`)** preempt the generic file when
  the terminal width matches. If you ship `foo.40col.ans` *and* `foo.ans`,
  a 40-col user sees the narrow one, an 80-col user sees the wide one,
  and there's no `^A\` gymnastics needed. The tradeoff is two files to
  keep in sync.

### Entering Ctrl-A bytes

Most editors render Ctrl-A as `^A` (caret-A) or a happy-face glyph. Use one of:
1. A BBS-aware ANSI editor (TheDraw, ACiDDraw, Pablo, MysticDraw) — see `resource:ansi editors` on the wiki.
2. `text/menu/*.msg` files contain **literal** 0x01 bytes; in PowerShell, `[System.IO.File]::ReadAllBytes()` / `WriteAllBytes()` handles them safely. From bash, `printf '\x01H...'` writes raw bytes.
3. In `ctrl/text.dat` and JS/Baja source, use the escape form `\1X` or `\x01X` — the runtime stuffs the byte at parse time.

### `ans2asc` / `asc2ans`

Two-way converters: `ans2asc` takes a `.ans` file with ANSI X3.64 escapes and produces a Ctrl-A-encoded `.asc`/`.msg`; `asc2ans` reverses it so you can hand a file to an ANSI editor. Both ship as Synchronet utilities. Convenient when collaborating with an ANSI artist who wants real ESC[ codes.

### Verifying what a file looks like rendered

- **Live**: connect a terminal client to the BBS and trigger the path that displays the file (or use `jsexec exec/printfile.js path` if available).
- **From a script**: `bbs.menu("foo")` / `console.printfile("path")` — both work under a session, not under jsexec.
- **Quick byte inspection**: `cat -v file.msg` (renders Ctrl-A as `^A` and high-bit bytes as `M-X`), `od -c file.msg | head`, or `xxd file.msg | head`.

### Modifying a stock file safely

Don't edit `text/foo.msg` in place — a future `git pull` (or release update) will overwrite it. Copy first:

```bash
cp <sbbs>/text/menu/main.msg <sbbs>/mods/text/menu/main.msg
$EDITOR <sbbs>/mods/text/menu/main.msg
```

The runtime picks up `mods/text/menu/main.msg` before the stock one because `cfg.mods_dir` is checked first in `menu_exists()` (see `src/sbbs3/prntfile.cpp`).

### Modifying `ctrl/text.dat`

`ctrl/text.dat` is one record per line; lines that look like `"\1...string..."  NNN SymbolName` are records. The line count and ordering matter — the C side reads it by index via the `TOTAL_TEXT` enum from `src/sbbs3/text.h`, which is generated from `text.dat` by `textgen.exe`. **Don't add or remove records** unless you also regenerate `text.h` / `text_defaults.c` / `text_id.c` (and rebuild). You can freely edit the **string value** of an existing record without regenerating anything — the BBS picks it up on next reload of `text.dat`.

To rebuild after structural changes:
```bash
cd <sbbs>/src/sbbs3
./textgen           # or msbuild on the textgen vcxproj
```

If a running BBS doesn't pick up a text.dat edit, restart sbbsctrl or call `bbs.reload_text()` from a JS console.

## Localization

`useron.lang` is a short language code (matches `ctrl/text.<lang>.ini` and `text/menu/<lang>/`). When set and the menu code doesn't contain `/`, `menu()` tries the language-prefixed path first and falls back to the unprefixed one on miss. Author a localized variant by dropping it under `text/menu/<lang>/<base>.<ext>` — same naming rules apply within the lang dir.

## Pitfalls

- **CGA brown**: `^AY` without `^AH` is brown on CGA. Always pair, or rely on sticky `^AH`. (See the dedicated section above.)
- **`mods/text/` precedence**: an old stale file in `mods/text/menu/` will silently shadow a freshly-pulled stock file. When stock files change in an update, audit `mods/text/menu/`.
- **`text.dat` line drift**: adding/removing a record without regenerating `text.h` produces a `total_text != TOTAL_TEXT` startup error.
- **@-codes don't expand in non-sysop messages**: posted text shows the raw `@SYSOP@` to readers. Use the JS `bbs.atcode()` API at message-render time if you need expansion in user content.
- **Hyphen-modifier + colon-param doesn't work**: `@FOO-L:bar@` is invalid; use the pipe form (`@FOO|L:bar@`).
- **Width modifier truncates from the right**: `@ALIAS|L8@` on a 12-char alias keeps the first 8 chars. Use `>` if you'd rather wrap.
- **`^AZ` is sticky-fatal**: a `^AZ` byte in the middle of a file stops rendering at that point, no further bytes are seen. Useful, but easy to misplace.
- **Ctrl-A operand `A`** is overloaded (security flag A vs literal-Ctrl-A) — prefer `@SHOW:FLAG1A@` for flag-based gating instead of relying on `^AA`.
- **Hotspot binding bytes (128–255)**: the value above 127 in `^A<byte>` after an option is the keycode "stuffed" when the hotspot fires. Common assignments live in `sbbsdefs.js` / `key.h` as `K_*` constants.
- **`menu()` returns false silently on missing file** — pass `P_NOERROR` for optional menus, otherwise the user sees "missing menu" errors.

## Reading the source for the call site

When you don't know which prompt or menu drives a given screen:

1. `grep -rn '"PromptName"' src/sbbs3/text.dat` — find the text.dat record index/ID.
2. `grep -rn 'PromptName' src/sbbs3/*.cpp` — find the C++ call site (often `bputs(text[PromptName])`).
3. `grep -rn 'menu("foo"' src/sbbs3/ exec/` — find what triggers `menu()` with a given base name.
4. For Baja shells: `grep -rn 'MNU foo' exec/*.src`.

## When to use this skill vs. others

- **Just rendering text to a user** in a custom JS module → look at the @-codes table and Ctrl-A reference, then write the file. No skill needed unless you hit the priority / mods-dir / language-overlay logic.
- **Adding a new prompt to `text.dat`** → also pulls in `synchronet-build` (textgen + rebuild).
- **Testing what a file looks like rendered** → connect a BBS session; jsexec can't `console.printfile` standalone.
- **Updating the wiki** → use the `synchronet-wiki` skill.
