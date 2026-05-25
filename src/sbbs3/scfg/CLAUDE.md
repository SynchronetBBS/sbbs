## Menu help text (`uifc.helpbuf`)

Every `uifc.list(...)` call should be preceded by a `uifc.helpbuf = "..."`
assignment that describes the menu the sysop is about to interact with.
This is what's shown when the sysop presses **F1** on a menu.

For menus with multiple options (more than just a yes/no picker), the
helpbuf should briefly describe **each option**, not just the menu's
overall purpose. A stub like `"Settings that control X."` for a 5-option
menu leaves the sysop with no way to know what any individual option
does without trial and error.

### Format

```c
uifc.helpbuf =
    "`Menu Title:`\n"
    "\n"
    "One- or two-sentence purpose of this menu — what category of\n"
    "settings live here, when a sysop would change them.\n"
    "\n"
    "`Option Name`: short description, max ~3 lines.  Default value if\n"
    "useful.  `0` or special-value behavior if applicable.\n"
    "\n"
    "`Other Option`: same shape.\n"
;
```

### Conventions

- **Title** in backticks with trailing colon: `` "`Menu Title:`\n" ``.
- **Wrap at 72 visible columns — this is a hard limit, not a target.**
  The uifc help window is 76 chars wide minus 2 borders minus 2×1 pad,
  so 72 visible chars per line is the renderer's actual ceiling
  (`showbuf()` in `src/uifc/uifc32.c`). Lines exceeding 72 visible chars
  auto-wrap mid-word at the boundary, which looks ragged — rebalance
  the paragraph instead of pushing past 72.
- **Pack lines tightly within that 72-budget — don't leave 10+ chars of
  slack.** Wrapping at ~62 when ~70 would fit produces a column of
  short lines that reads as choppy and wastes vertical space. After
  drafting, scan each line: if the *next* word would still fit under 72
  visible chars, pull it up. Aim for most lines to land in the high
  60s to low 70s of visible width. Markup characters consumed
  by the renderer don't take screen space and should *not* be counted
  against the wrap budget; per its `WIN_HLP` rendering:
  - `` ` `` and `\x01` toggle high-intensity (highlight) color
  - `~` and `\x02` toggle inverse-video
  So a literal source line like
  `` "`Network Interfaces`: comma-separated IPv4/IPv6 ..." `` displays
  as `Network Interfaces: comma-separated IPv4/IPv6 ...` — wrap against
  that visible form, not the source-character count.
- **Backtick-quote** option names (matching their menu labels) and literal
  values (`` `Yes` ``, `` `0` ``, `` `16M` ``) — uifc renders backtick
  spans in highlight color.
- **One blank line** (`"\n"`) between paragraphs and between option blocks.
- **Sysop voice** — do not reference C source files, function names, or
  internal struct/macro identifiers in helpbuf text. Describe what the
  sysop sees, types, or configures, using the labels they see on screen.
- **Don't cross-reference `sbbsctrl`** — SCFG runs on every platform
  Synchronet supports (Windows, Linux, *BSD, macOS), but `sbbsctrl` is
  the Windows-only Control Panel GUI. Pointing a Linux/BSD sysop at it
  is a dead end. Describe the option in its own terms; if you need to
  cite an alternative configuration entry point, prefer the relevant
  `.ini` section (`sbbs.ini`, `services.ini`, etc.) or the wiki page.
  (The wiki page may list `sbbsctrl` as one of several configuration
  surfaces — the helpbuf shouldn't.)
- **State when changes take effect** — most server settings need a
  recycle; mention it.
- **Cross-reference cousin menus** sparingly if helpful (e.g. "see also
  Rate Limiting").

### Anti-patterns

- One-liner menu intros like `"Settings that control the X."` as the
  entire body for a multi-option menu.
- Mentioning C struct fields, function names, or internal symbols
  (`logfile_base`, `js_startup_t`, `WEB_OPT_*`).
- Wrapping past 80 columns — wraps look ragged in uifc.
- Embedding URLs — helpbuf is read offline in a TUI; URLs cannot be
  clicked.

### Canonical examples to template from

- `rate_limit_cfg` in `scfgsrvr.c` — best multi-option example with rich
  per-option descriptions.
- `login_attempt_cfg` in `scfgsrvr.c` — concise menu-purpose intro.
- `max_concurrent_cfg` in `scfgsrvr.c` — medium-length with per-option
  breakdown.
- `js_startup_cfg` in `scfgsrvr.c` — recent rework following this format.

### Wiki sync

When the option being described is also documented on `wiki.synchro.net`
(under the `config:` namespace — most server/system options are), update
the wiki page in lockstep so the wiki and SCFG stay in agreement.
Inaccuracies often live in both for years.

### Helpbufs are NOT in `text.dat`

The `text[]` array workflow described in `../CLAUDE.md` covers the
runtime string database fed by `ctrl/text.dat`. SCFG menu help text is
**not** part of that system — helpbufs are plain C string literals
inline in the `scfg*.c` sources, edited directly and recompiled. Do not
add new helpbuf content to `text.dat`.

### Shared menu helpers apply to every caller

Several menus are rendered by a single helper called from multiple
server contexts:

- `js_startup_cfg` — JavaScript Settings (Global, Term, Mail, FTP, Web,
  Services)
- `login_attempt_cfg` — Failed Login Attempts (same six)
- `rate_limit_cfg` — Rate Limiting (FTP, Mail, Web, Services)
- `max_concurrent_cfg` — Max Concurrent Connections (Term, FTP, Mail,
  Web, Services)

Editing the helper's helpbuf updates every caller's menu in one go.

### Validating helpbuf changes

There is no automated test for menu help text. After building scfg
(`make -C src/sbbs3/scfg` — see the synchronet-build skill), have a
sysop open SCFG, navigate to the menu, and press **F1** to confirm the
rendered help looks right. Watch for: line wraps that look ragged,
backtick spans that didn't highlight, accidental long-option-dash
substitutions in literal CLI examples, and CP437 box-drawing characters
that got mangled (see below).

### Editing CP437 / high-byte characters in scfg sources

Some scfg helpbufs contain CP437 box-drawing characters (`\xC4`, `\xB3`,
etc.) used to illustrate menu layouts. The `Edit` tool corrupts those
raw bytes to the UTF-8 replacement sequence `\xEF\xBF\xBD` when
substituting strings. If you need to edit a helpbuf containing
high-byte characters, prefer in-place edits that don't touch those
bytes, or restore the original bytes with a `bytes.replace` post-step.

### `uifc.helpbuf = NULL` resets

`uifc.helpbuf = NULL;` clears the help for the next prompt — used to
prevent stale help text from a previous menu/dialog from being shown on
an unrelated input prompt. If a new sub-dialog needs no specific help,
this is the right pattern; do not leave a parent menu's helpbuf
dangling.

## Search index (`scfgindex.h`)

`scfgindex.h` is **AUTO-GENERATED** by `gen_option_index.py` from the
`snprintf(opt[..], ...)` / `strcpy(opt[..], ...)` calls and `uifc.list(...)`
titles in the surrounding `scfg*.c` sources. Do not hand-edit it.

After adding, renaming, or removing a menu option (or restructuring the
menu's call graph so paths change), regenerate from this directory:

```
python3 gen_option_index.py
```

Commit the regenerated `scfgindex.h` alongside the source change.

The script handles the indirection-table dispatch pattern
`switch (action[sel])` (as used by `rate_limit_cfg`) and fans out helper
functions called from multiple menus (e.g. `login_attempt_cfg` from each
server's submenu) to one index entry per caller chain. It deliberately
excludes the first-install wizard subtree (`cfg_wizard` and its callees)
because those options are not reachable via SCFG's Ctrl-F search.

### When adding a new menu dispatch pattern

If you write a `uifc.list(...)` whose result is dispatched in a way
other than `switch (sel)` or `switch (table[sel])` — for example, an
`if (sel == N)` chain or a function-pointer table — the index generator
may not recognize it as a navigable menu and silently drop the menu's
options from search results. After adding the menu, re-run the
generator and check that the new options appear in `scfgindex.h`; if
they don't, extend `is_navigable_list()` in `gen_option_index.py` to
recognize the new dispatch shape.

### Out-of-tree builds skip auto-regen

The Makefile's `scfgindex.h` regeneration rule is gated on
`SBBS_OFFICIAL` (the canonical sysop tree). Builds with
`SBBS_OFFICIAL=` (the default for out-of-tree clones) skip it. Always
run `python3 gen_option_index.py` by hand after touching menu
structure, regardless of which tree you're in.
