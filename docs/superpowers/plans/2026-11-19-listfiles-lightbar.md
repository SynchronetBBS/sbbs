# List Files Lightbar Style Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a msglist.js-like "lightbar" style, with a preview pane, to the
List Files module, selectable with the `[listfiles]` modopts `style` option,
next to the unchanged "classic" interactive style.

**Architecture:** `Listing.prototype.list_pages()` in `exec/listfiles.js`
becomes an engine (current file, tags, scrolling window, keys, commands)
driving a layout object; today's drawing code moves into a classic layout
unchanged, and a new lightbar layout adds the area line, key tips, bar and
preview pane. Pure logic (style choice, row split, pane state, contents
formatting) goes in `exec/load/listfiles_lib.js` with unit tests. One C++
method, `bbs.can_view_file()`, applies the V command's viewer rule.

**Tech Stack:** Synchronet JavaScript (SpiderMonkey 1.8.5 and 128
intersection), Synchronet C++ (`src/sbbs3/js_bbs.cpp`).

**Spec:** `docs/superpowers/specs/2026-11-19-listfiles-lightbar-design.md`

## Global Constraints

- JS: no `let`, `const`, arrow functions, template literals, `Object.keys()`,
  `for...of`, `for each`, default parameters (`exec/CLAUDE.md`).
- Classic style: no visible or behavioral change. Its drawing code is moved,
  not rewritten.
- Who gets which style is unchanged: interactive (classic or lightbar) only
  with batch flagging on and `cursor_capable()`; otherwise the letter flow
  (batch flagging on) or the plain list.
- Nothing in the lightbar layout scrolls the screen or wraps: everything is
  positioned and printed with `P_TRUNCATE`; no printing in the last column.
- Pane visibility is `user.settings & USER_EXTDESC`; every show/hide (T, P,
  Tab, E, V) saves it. In the lightbar style files are always one row.
- Contents are read only in the contents view, after `preview_delay`
  (default 0.5 s) without a key, never for files over `preview_max_size`
  (default 50 MB, i.e. 52428800), at most `preview_max_lines` lines; cached
  per directory listing by name + modification time.
- Contents only when `bbs.can_view_file(code, name)` is true (V's rule); if
  the method doesn't exist (older build), no contents view.
- Tests: `cd /sbbs && ./exec/jsexec tests/test.js listfiles` (lib) must
  pass after every task. Terminal checks are the sysop's (`;EXEC
  ?listfiles.js <code> <mode>`).
- Live deployment: `/sbbs/exec/*.js` are symlinks into this checkout, so JS
  edits go live immediately; `make` in `src/sbbs3` would deploy C++, so
  build-check C++ in a scratch git worktree only (synchronet-build skill,
  "Isolated variant builds").
- Commits: on master; check `git diff --cached --stat` is empty before
  `git add` (shared index); message lines <= 78 columns (verify with
  `awk 'length > 78'`); end with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. No push.
- Plain ASCII punctuation in code, comments and UI text (no em/en dashes).

## Review Focus

1. A screen too short for the pane (e.g. 24 rows with headings, or 16 rows):
   the pane is unavailable (T/Tab/E/V keep it hidden) and nothing scrolls.
   Tested in Task 2 (`listfiles_lightbar_rows`).
2. A corrupt or unreadable archive, or `Archive` throwing: the pane shows a
   one-line note, the listing carries on. Tested in Task 2
   (`listfiles_contents_lines` with a throwing reader).
3. Archive entry names or text lines longer than the screen, or with ANSI
   escape sequences / control characters: truncated, ESC sequences removed.
   Tested in Task 2.
4. Removing or moving the last file, or quitting, in the lightbar style:
   leaves cleanly (shared engine, finding 7 behavior). Covered by Task 3's
   engine move; terminal check in Task 4.
5. A build without `bbs.can_view_file`: no contents view, V uses the
   full-screen viewer, no script error. Tested in Task 2
   (`listfiles_contents_allowed`).

---

### Task 1: `bbs.can_view_file()` (C++)

**Files:**
- Modify: `src/sbbs3/js_bbs.cpp` (new `js_can_view_file` beside
  `js_move_file`; entry in `js_bbs_functions[]` after `move_file`)
- Test: `exec/tests/listfiles_bridges/bridges.js`

**Interfaces:**
- Produces: `bbs.can_view_file(dir_code, filename)` -> boolean. True when
  the file exists in the directory and a configured file viewer matches its
  extension (`file_type_match(path, cfg.fview[i]->ext)`) whose access
  requirements the user passes (`chk_ar(cfg.fview[i]->ar, &useron, &client)`)
  with `curdirnum` temporarily set to the file's directory, exactly as
  `sbbs_t::viewfile(const char*)` / `viewfilecontents()` in
  `src/sbbs3/viewfile.cpp` select a viewer. Runs nothing, prints nothing.
  Invalid dir code: throws ("Invalid directory"), as the other #1353
  methods do via `js_load_dir_file()`. Missing file: false.

- [ ] **Step 1: Add bridge test assertions** in `bridges.js` (before the
  `remove_file()` check, while the test file exists). The viewer
  configuration differs between systems, so the existing file's result is
  checked for type only:
  - `check("can_view_file() returns a boolean", typeof bbs.can_view_file(CODE, name), "boolean")`
  - `check("can_view_file() nonexistent", bbs.can_view_file(CODE, "nonexistent.zzz"), false)`
  - `check("can_view_file() bad dir throws", throws(function () { bbs.can_view_file("no_such_dir", name); }), true)`
- [ ] **Step 2: Implement `js_can_view_file`** using `js_load_dir_file()`
  and a new `sbbs_t` member `bool can_view_file(file_t*)` in
  `src/sbbs3/viewfile.cpp` (declared in `sbbs.h` beside `viewfilecontents`)
  that holds the viewer loop; refactor `viewfile(const char*)` to use a
  shared `int find_fview(const char* path)` (returns index or -1) so both
  apply one rule. `JSDOCSTR`: "Whether the current user may view the
  file's contents (a file viewer is configured for it that the user may
  use)", version 322. Suspend the request around the file I/O as the
  neighbors do. Style per `src/uncrustify.cfg`, hand-formatted.
- [ ] **Step 3: Build-check in a scratch worktree** (copy the changed
  `.cpp`/`.h` in; `make gitinfo`; `make -j$(nproc)`; then `make` serially).
  Expected: final serial `make` exits 0, no warnings in the changed files.
  Remove the worktree after.
- [ ] **Step 4: Commit** `src/sbbs3/js_bbs.cpp src/sbbs3/viewfile.cpp
  src/sbbs3/sbbs.h exec/tests/listfiles_bridges/bridges.js`, subject
  "JS bbs.can_view_file(): whether the user may view a file's contents".
  Tell the sysop the bridge test (`;EXEC ?tests/test listfiles_bridges`)
  needs their `libsbbs.so` rebuild; it's verified with GCC only.

### Task 2: Library: style choice, rows, pane state, contents, defaults

**Files:**
- Modify: `exec/load/listfiles_lib.js`
- Test: `exec/tests/listfiles/lib.js`

**Interfaces:**
- Produces (all pure; later tasks call them):
  - `listfiles_style(opts_style, batch, cursor)` -> `"classic"`,
    `"lightbar"`, `"letters"` or `"plain"`. Interactive only when `batch &&
    cursor`; unknown/empty `opts_style` -> `"classic"`.
  - `listfiles_lightbar_rows(screen_rows, headings, pane)` -> `{ list_top,
    list_rows, pane_top, pane_rows }` (0-based rows). Row 0 area line, row 1
    headings if `headings`, last row key tips. Without pane: list fills the
    rest (`avail` = rows between the headings and the key tips). With
    pane: list gets `Math.ceil(avail / 2)` rows, then one separator row
    (`pane_top - 1`), pane gets `avail - list_rows - 1`. If that leaves
    `list_rows < 3` or `pane_rows < 3`, `pane_rows` is 0 (pane unavailable)
    and the list fills the rest.
  - `listfiles_pane_key(key, shown, view)` -> `{ shown, view }` for `"E"`
    and `"V"` per spec decision 5's table (`view` is `"info"` or
    `"contents"`); other keys return the input unchanged.
  - `listfiles_contents_allowed(can_view_func, opts)` -> boolean: false if
    `opts.preview_contents` is false or `can_view_func` isn't a function.
  - `listfiles_text_lines(text, max_lines, width)` -> array: split on
    CR/LF, ANSI escape sequences (`/\x1b\[[0-9;?]*[A-Za-z]/g` and lone ESC)
    and other control characters except Ctrl-A removed, each line cut to
    `width` printable columns, at most `max_lines` lines.
  - `listfiles_archive_lines(entries, max_lines, width)` -> array: one
    `format("%-*s %10s %s", name_width, name, size, date)` line per entry
    (`entries` as `Archive.list()` returns: `name`, `size`, `time`;
    directories skipped; `name_width = width - 30`, names longer cut), at most
    `max_lines`.
  - `listfiles_contents_lines(reader, max_lines, width)` -> `{ lines, note }`:
    calls `reader()`; if it throws or returns null, `lines` is `[]` and
    `note` is the error text (caller formats it).
  - `listfiles_defaults()` gains: `style: "classic"`, `line_fmt_lightbar`
    (= `line_fmt_interactive`'s value), `lightbar_area_fmt` (`"\x01n\x01k\x017 %s / %s\x01>"`-like, args lib name, dir name),
    `lightbar_position_fmt: "[file %u of %u]"`, `lightbar_headings`
    (`" Name         Size  Description"`-like, aligned to the default
    line), `lightbar_keys: "~Download ~Batch ~View ~Ext ~Help ~Quit  Space:Tag Tab:Pane"`,
    `lightbar_keys_operator: "~Download ~Batch ~View ~Ext ~Remove ~Move ~Help ~Quit  Space:Tag Tab:Pane"`,
    `lightbar_attr: "\x01n\x01h\x01w\x014"`, `preview_info_fmt` (E-screen
    details via `@FILE_*@` codes: uploader, date, size, downloads, last
    download, cost), `preview_contents: true`, `preview_text_exts:
    "txt,nfo,diz,doc,md,me,1st,asc"`, `preview_max_size: 52428800`,
    `preview_max_lines: 500`, `preview_delay: 0.5`, `preview_info_label:
    "Info"`, `preview_contents_label: "Contents"`, `preview_lines_fmt:
    " Lines %u-%u of %u"`, `preview_separator: "\xc4"`, `preview_attr`,
    `preview_focus_attr` (msglist's `preview_*` colors), and the notes
    `preview_offline_note`, `preview_unviewable_note`,
    `preview_use_viewer_note`, `preview_error_fmt`.

- [ ] **Step 1: Write the failing tests** in `lib.js`, e.g.:
  - `check("style default", listfiles_style("", true, true), "classic")`,
    `("lightbar", true, true)` -> `"lightbar"`, `("lightbar", false, true)`
    -> `"plain"`, `("lightbar", true, false)` -> `"letters"`, `("bogus",
    true, true)` -> `"classic"`.
  - `listfiles_lightbar_rows(24, true, false)` ->
    `{list_top:2, list_rows:21, pane_top:0, pane_rows:0}`;
    `(24, true, true)` -> `{list_top:2, list_rows:11, pane_top:14, pane_rows:9}`;
    `(10, true, true)` -> `{list_top:2, list_rows:7, pane_top:0, pane_rows:0}`
    (7 rows would split 4 + separator + 2: too short, the list fills);
    `(24, false, true)` -> list_top 1.
  - every cell of the E/V table via `listfiles_pane_key`.
  - `listfiles_contents_allowed(undefined, {preview_contents:true})` false;
    `(function(){}, {preview_contents:false})` false; `(function(){}, {preview_contents:true})` true.
  - `listfiles_text_lines("a\x1b[1;31mb\r\nc\x07d", 10, 79)` -> `["ab", "cd"]`;
    width cut; max_lines cut.
  - `listfiles_archive_lines([{type:"directory",name:"d/"},{type:"file",name:"a.txt",size:12,time:0}], 10, 79).length` -> 1; a 200-char name cut to fit 79 columns.
  - `listfiles_contents_lines(function () { throw new Error("bad zip"); }, 10, 79)` -> `{lines:[], note:"bad zip"}`.
  - defaults: `strip_ctrl(d.lightbar_keys).length <= 79`, same for
    `lightbar_keys_operator`; `d.style === "classic"`.
- [ ] **Step 2: Run** `cd /sbbs && ./exec/jsexec tests/test.js listfiles`.
  Expected: FAIL, `listfiles_style is not defined`.
- [ ] **Step 3: Implement** the functions and defaults above in
  `listfiles_lib.js` (with the file's comment style: one comment per
  function saying what it returns).
- [ ] **Step 4: Run the tests.** Expected: `lib.js ... passed`.
- [ ] **Step 5: Commit** `exec/load/listfiles_lib.js exec/tests/listfiles/lib.js`,
  subject "listfiles_lib.js: lightbar style and preview pane helpers".

### Task 3: Engine and classic layout (no visible change)

**Files:**
- Modify: `exec/listfiles.js` (`list_pages()` and its inner functions,
  roughly lines 670-1035 today)

**Interfaces:**
- Consumes: nothing new.
- Produces: `Listing.prototype.list_pages(layout)` (the engine) and
  `function ClassicLayout(listing)`; later tasks add `LightbarLayout`.
  A layout object has these methods (the engine passes itself as
  `engine`, exposing `engine.cur`, `engine.top`, `engine.tagged`,
  `engine.files` (= `listing.files`), `engine.visible()`, `engine.bottom()`,
  `engine.redraw(full)`):
  - `begin()` -> `{ list_row, rows }`: clear and draw what's above the
    list for a (re)drawn directory; classic: today's `draw_page(true)`
    header part.
  - `height(i)` -> rows of file i (classic: 1 + extended description
    lines, as `measure()` does today).
  - `draw_file(i, row, max_lines, current, tagged)` -> rows drawn (classic:
    today's `draw_entry()` + `line()`).
  - `draw_below()`: what's under the list (classic: position line and
    prompt with more_above/more_below arrows and its hotspots, as today).
  - `place_cursor()`: before `getkey` (classic: end of the prompt).
  - `key(ch)` -> `null` if not consumed, else one of `"done"` (handled, no
    redraw), `"redraw"` (redraw in place), `"relayout"` (begin() again);
    classic consumes nothing.
  - `leave()`: tidy up on leaving (classic: today's `leave()`).
  - `hotspots_cleared()`: re-add any fixed hotspots after the engine
    cleared them (classic: the commands bar, as `draw_page` does now).
- [ ] **Step 1: Run the lib tests** to record the baseline (expected: pass).
- [ ] **Step 2: Move the code.** Engine keeps: `cur`, `top`, `tagged`,
  `last_click`, `heights`, `rows`, window functions, `move`, `page_up`,
  `page_down`, `toggle_tag`, `chooser`, the key loop (navigation keys,
  Space, Enter/N, Delete->R, hotspot clicks, command dispatch, CMD_*
  handling, `total` counting, empty-directory exit). Classic layout gets
  `line`, `draw_entry`, `clear_to_eol`, `draw_position`, the prompt
  building (`listfiles_shorten_to_fit` call), `leave`, the header part of
  `draw_page`. Same statements, same order of output; no new behavior.
  `list()` calls `this.list_pages(new ClassicLayout(this))`.
- [ ] **Step 3: Syntax check:**
  `/sbbs/exec/jsexec -C -r 'var f=new File("/home/rswindell/sbbs/exec/listfiles.js"); f.open("r"); var s=f.read(); f.close(); new Function(s); print("syntax ok");'`
  Expected: `syntax ok`. Run the lib tests: pass.
- [ ] **Step 4: Sysop terminal check** (ask and wait): in a directory with
  more files than one screen and in `test` (ANSI-art descriptions), with
  batch flagging on: arrows, PgUp/PgDn/Backspace/N, Home/End, Space tag,
  click and double-click, T toggle, B/D/V/E, R and Delete then cancel, M
  cancel, `?` help, Q; a new-file scan (`;EXEC ?listfiles.js <code> 1`)
  across directories. Expected: identical to before.
- [ ] **Step 5: Commit** `exec/listfiles.js`, subject
  "listfiles.js: split the interactive mode into an engine and a layout".

### Task 4: Lightbar layout without the pane

**Files:**
- Modify: `exec/listfiles.js` (new `LightbarLayout`; `list()` style choice)
- Create: `text/menu/listfiles_lightbar_help.msg` (CRLF, Ctrl-A, mode 664,
  group `sbbs`; written byte-safely with Python, not the Edit tool)
- Modify: `ctrl/modopts.ini` (`[listfiles]`: commented `style` with both
  values and the lightbar options with defaults)

**Interfaces:**
- Consumes: Task 2's `listfiles_style`, `listfiles_lightbar_rows`, defaults;
  Task 3's layout interface.
- Produces: `function LightbarLayout(listing)` implementing every layout
  method; its own state `pane_shown` (from `USER_EXTDESC`), `view`, `focus`
  (`"list"` or `"pane"`) used by Tasks 5-6.

- [ ] **Step 1: Choose the style** in `list()`: `listfiles_style(opts.style,
  this.batch, cursor_capable())`; `"lightbar"` -> `list_pages(new
  LightbarLayout(this))`. Lightbar shows no header/footer menu files (skip
  `show_header()`/`show_tail()`).
- [ ] **Step 2: Implement `LightbarLayout`** for the screen in the spec's
  "Lightbar screen": area line (`lightbar_area_fmt`, names shortened with
  `listfiles_shorten_to_fit`, `lightbar_position_fmt` right-justified when
  `console.screen_columns >= 80`), headings row (`lightbar_headings`, none
  if `""`), file rows (`line_fmt_lightbar`, `height()` always 1, extended
  descriptions never inline), the bar (current row `strip_ctrl`ed, padded
  to `screen_columns - 1`, in `lightbar_attr`), key tips on the last row
  via `console.mnemonics()` (`lightbar_keys_operator` when
  `listing.can_remove()`), truncated. For now the pane is always hidden
  (`listfiles_lightbar_rows(rows, headings, false)`). Mouse:
  `console.status |= CON_MOUSE_SCROLL` while listing (restore on leave);
  wheel up/down moves the bar. `?` shows `listfiles_lightbar_help`.
- [ ] **Step 3: Help file** `text/menu/listfiles_lightbar_help.msg`: the
  key table from the spec (list focus and pane focus), same look as
  `listfiles_help.msg`.
- [ ] **Step 4: modopts.ini** comments for `style` and the lightbar options.
- [ ] **Step 5: Syntax check and lib tests** (as Task 3 Step 3). Expected:
  pass.
- [ ] **Step 6: Sysop terminal check** with `[listfiles:test] style =
  lightbar` (ask the sysop to add it, or add it after announcing the live
  config edit): bar movement and scrolling, tagging, mouse click/wheel,
  commands, Delete/R/M in `test_rm`, removing the last file, Q, Enter at
  the end, a scan; 80x24 and a narrow (e.g. 40-column) screen.
- [ ] **Step 7: Commit** `exec/listfiles.js text/menu/listfiles_lightbar_help.msg
  ctrl/modopts.ini`, subject "listfiles.js: the lightbar style".

### Task 5: Preview pane, info view

**Files:**
- Modify: `exec/listfiles.js` (`LightbarLayout`)

**Interfaces:**
- Consumes: `listfiles_lightbar_rows`, `listfiles_pane_key`,
  `listfiles_extdesc_lines`, `bbs.expand_atcodes(fmt, file, dir_code)`.
- Produces: `LightbarLayout.prototype.pane_lines(i)` -> array of display
  lines for file i in the current view (Task 6 adds the contents branch);
  view persistence in `bbs.mods.listfiles_view`.

- [ ] **Step 1: Pane drawing:** separator row (`preview_separator` fill,
  label `preview_info_label`, `preview_lines_fmt` with the range,
  `preview_focus_attr` when `focus === "pane"`), then pane rows from
  `pane_lines(cur)` starting at `pane_scroll`, each truncated and cleared
  to end of line. Info lines: `bbs.expand_atcodes(opts.preview_info_fmt,
  file, code)` split on CR/LF, then `listfiles_extdesc_lines(file.extdesc)`.
  The file object needs extended details: load with
  `FileBase.DETAIL.EXTENDED` (already the case for `extdesc`).
- [ ] **Step 2: Keys** in `key(ch)`: T and P toggle `pane_shown` (save
  `user.settings ^= USER_EXTDESC`), Tab (show + focus pane / focus list),
  E and V via `listfiles_pane_key` (V's contents branch: Task 6; until
  then V with contents selected behaves as the full-screen viewer),
  navigation keys and wheel scroll the pane while `focus === "pane"`.
  Showing/hiding returns `"relayout"`; moving the bar resets
  `pane_scroll` to 0 and redraws the pane. When `listfiles_lightbar_rows`
  says the pane is unavailable, keys that would show it do nothing.
  Initial view: `bbs.mods.listfiles_view || "info"`; store on change.
- [ ] **Step 3: Syntax check and lib tests.** Expected: pass.
- [ ] **Step 4: Sysop terminal check:** T/P/Tab/E in `test` and a large
  directory; pane scrolling with focus; setting persists (classic shows
  extended descriptions inline after turning the pane on); short screen
  (e.g. 16 rows) keeps the pane hidden.
- [ ] **Step 5: Commit** `exec/listfiles.js`, subject
  "listfiles.js: the lightbar style's preview pane".

### Task 6: Preview pane, contents view (DROPPED)

> Dropped 2026-11-19: the sysop decided the pane shows no file contents
> (spec decision 9, revised). `bbs.can_view_file()` (Task 1) stays for
> other modules' use.

**Files:**
- Modify: `exec/listfiles.js` (`LightbarLayout`)

**Interfaces:**
- Consumes: Task 1's `bbs.can_view_file`, Task 2's
  `listfiles_contents_allowed`, `listfiles_contents_lines`,
  `listfiles_archive_lines`, `listfiles_text_lines`.

- [ ] **Step 1: Contents lines** in `pane_lines(i)` when `view ===
  "contents"`: offline file (no `File` at `file_area` path) ->
  `preview_offline_note`; not `listfiles_contents_allowed(bbs.can_view_file,
  opts)` or `!bbs.can_view_file(code, name)` -> `preview_unviewable_note`;
  size > `preview_max_size` or an extension that's neither an archive nor in
  `preview_text_exts` -> `preview_use_viewer_note` (when viewable);
  archives (try `new Archive(path).list()`) via `listfiles_archive_lines`;
  text via `listfiles_text_lines` (read at most `preview_max_lines` lines
  with `File.readln`). Cache in a per-listing object keyed
  `name + "\t" + file_date(path)`.
- [ ] **Step 2: Delay:** when the bar lands on a file in the contents view
  and it isn't cached, draw the pane's first row as `"..."`, wait
  `console.inkey(K_NONE, opts.preview_delay * 1000)`; if a key arrived,
  hand it to the key loop without reading; else read, cache, draw.
- [ ] **Step 3: V full-screen fallback:** V in the contents view (or V
  selecting it) on a viewable file the pane can't show runs
  `bbs.view_file_info(code, name, false)` and returns `"relayout"`.
- [ ] **Step 4: Syntax check and lib tests.** Expected: pass.
- [ ] **Step 5: Sysop terminal check** (after their `libsbbs.so` rebuild):
  a ZIP and a .txt in `test_rm`; holding Down through a directory of
  archives stays responsive; a file over the size cap; a non-viewable
  extension; an offline file; V on an image or other configured-viewer
  type opens full screen; an older build (method missing) shows no
  contents view, without errors.
- [ ] **Step 6: Commit** `exec/listfiles.js`, subject
  "listfiles.js: archive and text contents in the preview pane".

### Task 7: Documentation

**Files:**
- Modify: wiki `module:listfiles` (`/var/www/html/wiki/data/pages/module/listfiles.txt`,
  synchronet-wiki skill's attic/changes procedure)
- Modify: `docs/v322_new.md` (extend the existing listfiles bullet, briefly)

- [ ] **Step 1: Wiki:** a "Styles" section (classic vs lightbar, `style`
  option, per-lib/dir overrides, same batch-flagging rule), the lightbar
  keys table, the new options with defaults, `bbs.can_view_file` not
  mentioned (internal); "(new in v3.22)". Verify the rendered page
  (`_export/xhtml` with `?purge=true`; no leaked `''`/`**`).
- [ ] **Step 2: Release note** wording, one line added to the bullet.
- [ ] **Step 3: Commit** `docs/v322_new.md` (only this hunk: check
  `git diff docs/v322_new.md` first; other sessions edit this file).
  Ask the sysop before posting the wiki change notice.
