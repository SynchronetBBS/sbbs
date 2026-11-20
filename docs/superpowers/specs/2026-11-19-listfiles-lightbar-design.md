# List Files module: a lightbar style with a preview pane

Design for a second interactive style ("lightbar") of the stock List Files
loadable module (`exec/listfiles.js`, #1353), selectable by the sysop next to
the existing interactive style ("classic").

## Intent

The module's current interactive mode (shipped 2026-11-18) is deliberately
true to the built-in file lister: the directory header file, a commands bar,
file lines like the built-in's, a prompt under the list. It's a good
"interactive but legacy" listing and stays as it is.

The sysop wants a second personality that resembles `exec/msglist.js`:
area details on the top line, command/key tips on the bottom line, a true
full-width lightbar on the current file, and msglist's preview pane, here
showing the current file's extended information. A sysop
chooses either style through documented modopts settings.

## Goals

- A `lightbar` style, chosen with one `[listfiles]` modopts key, overridable
  per library and per directory like every other option.
- The classic interactive style unchanged in look and behavior.
- A preview pane showing the current file's information and extended
  description.
- Everything visible in the new style customizable with modopts format
  strings (as the classic style's commands bar and prompt are), and
  documented on the wiki page `module:listfiles`.

## Non-goals

- No change to who gets an interactive listing (see "Who gets which style").
- No preview pane in the classic style.
- No replacement of the V command's full-screen viewer, which runs the
  sysop's configured viewers.
- No file contents (archive listings, text) in the preview pane (decision
  9); V remains the full-screen viewer.
- No new C++ beyond one small `bbs` method, `bbs.can_view_file()` (kept for
  other modules; see decision 9); otherwise the module uses existing JS
  classes and the `bbs` methods added for #1353.

## Anti-goals

- Any visible or behavioral change to the classic style.
- Output that scrolls the screen or wraps lines in the lightbar layout
  (everything is positioned and truncated, as in classic).

## Constraints

- SpiderMonkey 1.8.5 and 128 intersection (`exec/CLAUDE.md`): no `let`,
  `const`, arrow functions, `Object.keys()` etc.
- ANSI and PETSCII terminals (the existing `cursor_capable()` test).
  Screens narrower than 80 columns and shorter than 24 rows must still work.
- Mouse: hotspots as in classic (click selects, double-click tags).

## Approaches considered

- **A. One shared engine, two layouts (chosen).** Split the classic mode's
  `list_pages()` into an engine (current file, tags, scrolling window, keys,
  commands) and layout objects (classic, lightbar). Fixes to navigation and
  commands apply to both styles; classic is rebuilt on the engine and must be
  re-verified at a terminal.
- **B. A separate `list_lightbar()` beside `list_pages()`.** Classic
  untouched, but ~150 lines of navigation/tagging/window code duplicated;
  each later fix made twice.
- **C. A separate module file.** Needs the `Listing` class moved to a library
  first (a larger restructure than A) and gives sysops two module names
  instead of one setting.

The sysop chose A.

## Decisions (the sysop's)

1. **Style setting:** `style = classic | lightbar` in `[listfiles]`,
   `[listfiles:lib<N>]` or `[listfiles:<code>]`.
2. **Who gets which style:** unchanged gating. The chosen style replaces the
   classic interactive mode only for users with batch flagging on and a
   cursor-capable terminal. Batch flagging off: the plain scrolling list.
   Dumb terminals: the letter flow (batch flagging on) or the plain list.
3. **Preview pane visibility is the user's extended-descriptions setting**
   (`USER_EXTDESC`, toggled with T and saved, as today). In the lightbar
   style, extended descriptions are never shown inline; every file is one
   row.
4. **Tab** shows the pane (if hidden) and moves the focus between the list
   and the pane; with the focus in the pane, the navigation keys scroll the
   pane. Showing the pane with Tab turns `USER_EXTDESC` on (saved), as T and
   P do, so the pane's visibility and the saved setting never disagree
   (proposed by Claude in the conversation, not objected to).
5. **E shows or hides the pane**, as T does (P is the previous page, as in
   classic) (saving `USER_EXTDESC`,
   decision 3). **I** is the full-screen extended information (with the
   uploader's avatar) for the tagged files or the current file: the classic
   style's E. **V** is the full-screen viewer, as in classic.
6. **Enter** is next page, then the next directory (as in classic, which scan
   users are used to). **N** is a non-control-key alternative to PgDn.
7. Custom header/footer menu files are not shown in the lightbar style; the
   area line and key-tips line take their place.
8. In multi-directory scans each directory with files gets a fresh screen.
9. **No contents in the pane** (revised during the build, 2026-11-19): an
   archive's file list is what V already shows full-screen, so the pane
   would mostly duplicate it, at the cost of reading archives as the bar
   moves. The pane shows only the file's information. An earlier version of
   this decision (contents only where V could show them) added
   `bbs.can_view_file(dir_code, filename)`: whether the file exists and a
   configured file viewer for it is available to the user (V's rule). The
   sysop kept the method for future use by other modules.

## Design

### Engine and layouts

`Listing.prototype.list_pages()` becomes the engine. It owns:

- the files, the current index, the tagged set, the window (`top`, row
  heights, `listfiles_window_count()`/`listfiles_window_top()`);
- the key loop and its navigation keys, mouse hotspot handling, tagging,
  the Delete key, and the dispatch of commands to
  `Listing.prototype.command()`;
- the results returned to `list()` (CMD_* handling, counts, `leave()`
  behavior as fixed for review finding 7).

A layout object supplies what differs. The engine calls it to:

- lay out the screen for a directory (clear, draw what's above the list) and
  report the first list row and how many rows the list has;
- report each file's height in rows (classic: 1 plus extended description
  lines; lightbar: always 1);
- draw a file row, current or not, tagged or not;
- draw what's below the list (classic: position line and prompt; lightbar:
  the pane and the key-tips line), given the window state;
- handle layout-specific keys before the engine does (lightbar: Tab, P, T,
  E, V, and the navigation keys while the pane has the focus), returning
  whether it consumed the key;
- tidy up on leaving a directory.

The classic layout is today's drawing code moved, not rewritten. The
interface above is a starting point; the builder may adjust it, provided both
layouts sit on one engine.

Style selection in `list()`: batch flagging + `cursor_capable()` +
`opts.style` decide between classic layout, lightbar layout, the letter flow
and the plain list. An unknown `style` value falls back to classic.

### Lightbar screen

```
 Main / Synchronet Files                              [file 3 of 99]
 Name          Size  Description
 sbbs322.zip   4.1M  Synchronet v3.22 for Windows
 sbbs322s.tgz  9.8M  Synchronet v3.22 source                        <- bar
 ...
-- Info ---------------------------------------------- Lines 1-6 of 9
 Uploaded by Digital Man on 11/18/26, downloaded 12 times
 ...
 Download Batch View Ext Help Quit  Space:Tag Tab:Pane   (key letters lit)
```

- **Area line** (row 1): `lightbar_area_fmt` with the library and directory
  names (and numbers), the right part `lightbar_position_fmt`
  (`[file %u of %u]`) right-justified; the position is omitted under 80
  columns, as msglist does. Long names are shortened with
  `listfiles_shorten_to_fit()`.
- **Column headings** (row 2): `lightbar_headings`; `""` for none (the list
  then starts on row 2).
- **List rows:** `line_fmt_lightbar`, default the classic interactive line
  (tag column, no letters). The current row is the **bar**: the row's text
  stripped of its colors, padded to the screen width minus one, in
  `lightbar_attr`.
- **Key-tips line** (last row): `lightbar_keys` (`lightbar_keys_operator`
  for users who may remove files from the directory, adding Remove and
  Move), a mnemonics string displayed with `console.mnemonics()`: `~` marks
  each key letter within its word (`~Download`), highlighted in the
  mnemonic colors, each a mouse hotspot, comma-separated and ending at the
  cursor like msglist's: `~Download, ~Batch, ~View, ~Ext, ~Info, Space=Tag,
  ~Quit or ~?` (the operator variant adds `~Remove, ~Move`). Truncated to
  the screen width, never wrapped.
- **Pane** (when shown): the list gets the upper half of the rows between
  the headings and the key-tips line; then a separator line; then the pane.
  The separator shows its label (Info), the line range
  (`Lines %u-%u of %u`) and is highlighted while the pane has the focus
  (formats and colors as options, modeled on msglist's `preview_*` options).
- Scrolling redraws rows in place, as classic does; the screen is cleared
  only for a new directory or after a command's full-screen output.

### Preview pane

The current file's information: `preview_info_fmt`, a template expanded with
`bbs.expand_atcodes(fmt, file, dir_code)` (the `@FILE_*@` codes added for
#1353), followed by the extended description split by
`listfiles_extdesc_lines()`. The default template gives what the E screen
gives: uploader, upload date, size, downloads, last download, cost.

### Keys (lightbar)

| Key | List focus | Pane focus |
|---|---|---|
| Up/Down, PgUp/PgDn, Home/End | move the bar | scroll the pane |
| Backspace | PgUp | PgUp in the pane |
| N | PgDn | PgDn |
| Enter | next page, then next directory | same |
| Space | tag and move down | (ignored) |
| Tab | show pane (saved setting), focus pane | focus list |
| T, E | toggle pane (saved setting) | hide pane, focus list |
| P | previous page (as in classic) | same |
| I | full-screen extended information (with the uploader's avatar), for the tagged files or the current file: the classic style's E | same |
| Ctrl-A | tag all files, or none if the first is tagged (both styles) | same |
| B D V R M Del S Q ? H | as in classic (H: help) | as in classic |

Mouse: click selects a row, double-click tags (as classic); the wheel scrolls
whichever has the focus (msglist sets `CON_MOUSE_SCROLL`).

Help: `?` shows `text/menu/listfiles_lightbar_help.msg` (a new file, like
`listfiles_help.msg`, listing the keys above).

### Options (new, in `listfiles_defaults()`)

`style`, `line_fmt_lightbar`, `lightbar_area_fmt`, `lightbar_position_fmt`,
`lightbar_headings`, `lightbar_keys`, `lightbar_keys_operator`,
`lightbar_attr`, `preview_info_fmt`, and the separator's label/format/color
options. Names may be refined by the builder; defaults must reproduce the
screen above. The stock `ctrl/modopts.ini` `[listfiles]` section lists
`style` (commented, with both values).

## Testing

- Lib tests (`exec/tests/listfiles/lib.js`) for every pure function added:
  style selection, pane/list row split, pane scrolling, option defaults.
- The sysop at a terminal: classic re-verified after the engine split (the
  same checks as for #1353: scrolling, tags, commands, scans, Delete, exits);
  lightbar checked in the throwaway directory `TEST_RM` and the `test`
  directory (whose ANSI-art extended descriptions exercise the pane).

## Documentation

- Wiki `module:listfiles`: a "Styles" section (classic vs lightbar, how to
  choose, per-lib/dir overrides), the new options, the lightbar keys,
  marked "(new in v3.22)".
- `docs/v322_new.md`: extend the existing listfiles bullet, briefly.

## Left to the builder

- The exact engine/layout interface, and how classic's drawing moves into
  its layout without visible change.
- Option names beyond `style`, and default colors (taken from msglist's
  where there's an equivalent).
- Behavior on very short screens (e.g. pane unavailable below a minimum
  number of rows), provided nothing scrolls or wraps.
- The task breakdown and commit structure.
