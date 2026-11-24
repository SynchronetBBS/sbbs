# Extended File Info module (`extfileinfo`) design

GitLab #1421.

## Intent

After a file search, the text that matched is often only in a file's
extended description (e.g. "NOFX" inside a stem track name such as
`BGAcapellaNoFX_03.L`). `listfiles.js` now highlights matches in its
listings, but the extended information screen the user goes to next (the
**E** key, the file information command) is drawn by C++
(`sbbs_t::showfileinfo()`, `src/sbbs3/file.cpp`) and highlights nothing.

Add an optional loadable module that draws that screen, and a stock JS
implementation, `exec/extfileinfo.js`, so it can highlight what the search
found. The module is also the starting point for later sysop customization
of that screen.

## Goals

- A new loadable module setting, **Extended File Info** (`extfileinfo` in
  `main.ini` `[module]`), run by `showfileinfo()` when configured; the
  built-in display is used when it isn't (or the user fails its ARS).
- `exec/extfileinfo.js` reproduces the built-in display: same `text.dat`
  strings, same fields and conditions, same `text/menu/fileinfo.*` handling,
  same extended description output, same "not online" notice.
- It highlights search matches in the **short description** (the
  `FiDescription` line) and the **extended description**.
- The search expression reaches the display explicitly, as an argument
  (the user picked this over remembering it for the session).

## Non-goals (for now)

- No new customization: no `modopts.ini` options, no layout changes, no
  paging or scrolling of long extended descriptions, no media/SAUCE details.
- No highlighting in the built-in C++ display.
- No highlighting inside a `fileinfo` menu file (its `@FILE_DESC@` is
  expanded by the menu code, as today).
- The module draws the information only. Every prompt that follows it stays
  with its caller (decided in #1421):
  - **E** key / file information command (`viewfile()`): `FileInfoPrompt` or
    `FileInfoEditPrompt`
  - remove command and the sysop's old / old-upload / offline scans:
    `UserRemoveFilePrompt`, `RExemptRemoveFilePrompt`, `SysopRemoveFilePrompt`
  - download and user-to-user download: `ProtocolBatchQuitOrNext` /
    `QuitOrNext` (the download command shows no extended description)

## Anti-goals

- The screen must not look different from the built-in one when nothing was
  searched for. A sysop who configures the module should see no change
  except highlighting.
- No session-wide "current search" state.

## Constraints

- JS must run on SpiderMonkey 1.8.5 and 128 (`exec/CLAUDE.md`).
- C++ builds under GCC, Clang and MSVC. I can only build on Linux (GCC); the
  Windows build (VERT shares `main.ini`) needs the user's MSVC build.
- New loadable modules follow the existing pattern: `struct loadable_module`
  in `scfg_t` (`scfgdefs.h`), read in `scfglib1.c`, written in
  `scfgsave.c`, freed in `load_cfg.c`, listed with help text in
  `scfg/scfgsys.c`.
- Module command lines are split on spaces, runs of spaces collapse, and an
  argument that starts with `"` runs to the next `"` with the quotes
  stripped (`js_execfile()`, `src/sbbs3/exec.cpp`). `"` is illegal in
  filenames (`ILLEGAL_FILENAME_CHARS`), so a quoted filename is safe. A
  search expression is not: an argument starting with `"` loses its quotes
  (so a `"WORD"` whole-word term becomes a substring term). So `"` and `%`
  in the search are passed encoded as `%22` and `%25`, and the module joins
  the remaining arguments with single spaces and decodes them; nothing
  else is encoded, so a search without quotes or `%` passes unchanged
  (repeated spaces still collapse, which doesn't matter). This is the scheme
  proposed for the List Files module's argument in #1423 (DD File Lister
  compatibility discussed there); `extfileinfo` uses it from the start. An
  empty search is passed as no argument.
- `exec_mod()` formats its arguments into a 512-byte buffer.

## Approaches considered

- **A. Pass the search explicitly (chosen).** An optional search argument on
  `showfileinfo()`, `viewfile()` and `bbs.view_file_info()`; the listers
  pass their expression. No hidden state; more signatures change (all with
  defaults, so existing callers and scripts are unaffected).
- **B. Remember the search for the session.** Listers store the expression
  on `sbbs_t` while listing; the display reads it. Fewer signature changes,
  but the stored value can go stale (an aborted listing, a script showing
  file info later) and highlight the wrong words.

## Design

### C++

1. `scfg_t` gains `struct loadable_module extfileinfo_mod;` read/written as
   `extfileinfo`, freed with the rest. SCFG lists it as **Extended File
   Info** in System -> Loadable Modules, next to View File Info, with help
   text: the user views one file's extended information (after the **E**
   key, and in the file information, download and remove commands); the
   module is passed the directory's internal code, the filename, whether to
   show the extended description, and the search expression, if any.
   The existing View File Info help line should make the difference clear
   (it replaces the whole file-by-file command, not the display).
2. `void showfileinfo(file_t*, bool show_extdesc = true, const char* search = nullptr);`
   After setting `current_file` (so `@FILE_*@` codes work in the module and
   in a `fileinfo` menu file it shows), it runs
   `exec_mod("extended file info", cfg.extfileinfo_mod, &invoked, "%s \"%s\" %u %s", dir code, f->name, show_extdesc, <encoded search or "">)`,
   with the search encoded by the existing `hex_encode('%', search, "\"",
   buf, sizeof buf)` (`src/encode/hex.c`, already linked into `libsbbs`
   and used by the web server), which escapes the escape character itself
   and the listed characters: exactly `%` and `"`. The module decodes every
   `%XX` (the JS equivalent of `hex_decode('%', ...)`): since every `%` was
   encoded, that's exact.
   If invoked, it clears `current_file` and returns; otherwise it carries on
   with the built-in display.
3. `int viewfile(file_t* f, bool extdesc, const char* search = nullptr);`
   passes `search` to `showfileinfo()`.
4. The built-in lister (`listfile.cpp`) passes its search expression to
   `viewfile()` when listing in `FL_FIND` mode (the `FL_EXT` listing and the
   **E**/**V** commands at the batch-flag prompt); otherwise `nullptr`.
   `listfiles()` overwrites `filespec` with `liberal_filepattern()` (which
   appends `*` to a long search), so the original expression is saved
   before that and passed instead. `listfileinfo()`
   passes nothing (its filespec is a file name pattern, not a search).
5. `bbs.view_file_info(dir_code, filename [,extended=true] [,search])`:
   optional 4th argument passed to `viewfile()`. Update its `JSDOCSTR`
   (and the version number field, as for other changed methods).

### JS

6. `listfiles.js` passes `this.filespec` as the 4th argument to
   `bbs.view_file_info()` when listing in `FL_FIND` mode.
7. `exec/extfileinfo.js`, `argv`: `<dir code> <filename> <0|1 show extdesc> [search...]` (the search's
   arguments joined with spaces, then `%22` and `%25` decoded).
   - Loads the file from `FileBase` with the detail level that includes the
     extended description, hashes and uploader details (so edits made
     through `viewfile()` are seen). A FileBase file object has the indexed
     size and date unless the directory checks files on disk, while
     `showfileinfo()` always checks the disk: the module gets the size and
     date from the file on disk itself (missing: size -1) for the size line,
     the checksum condition, `FiFileDate` and the "not online" notice.
   - If `bbs.menu("fileinfo", P_NOCRLF | P_NOERROR)` shows a menu file, skips
     the header; otherwise prints, each with `P_TRUNCATE` and under the same
     conditions as `showfileinfo()`: `FiLib`, `FiDir`, `FiFilename`,
     `FiFileSize` (comma-separated bytes and a 1024-based estimate with one
     decimal), `FiCredits` (`FREE` for free dirs or a positive size with no
     cost), `FiChecksum` for CRC-32 / MD5 / SHA-1 when the file is present at
     its indexed size, `FiDescription` (highlighted), `FiTags`, `FiAuthor`,
     `FiGroup`, `FiUploadedBy` (`UNKNOWN_USER` when anonymous, plus
     " via <protocol>"), for directory operators a second `FiUploadedBy` with
     `[ip] host`, `FiUploadedTo`, `FiDateUled`, `FiFileDate` (when the file
     has a date), `FiDateDled`, `FiTimesDled`, `FiTransferTime` (when the
     time to download is nonzero, with the current CPS), then
     `FileHdrDescSeparator`.
   - When asked to and the file has one: the extended description, leading
     line breaks skipped and trailing white space trimmed, highlighted,
     shown with `P_NOATCODES | P_CPM_EOF | P_AUTO_UTF8`, then a newline.
   - When the file is missing: `FileIsNotOnline`, plus the path for sysops,
     but (as in C++, where only the header code finds the size) not when a
     `fileinfo` menu file was shown.
   - Highlighting reuses `listfiles_search_terms()` / `listfiles_highlight()`
     from `exec/load/listfiles_lib.js`. The color is the List Files
     module's `found_attr` option for the file's directory (`[listfiles]`,
     `[listfiles:lib<N>]`, `[listfiles:<code>]` in `modopts.ini`), so the
     info screen matches the listing the user just left (*my call*). When it
     is empty (the default), the toggle-bright mode: each match is shown in
     the color it's in, with the bright attribute toggled, starting from the
     console's current attribute (after the `FiDescription` string's Ctrl-A
     codes before its `%s`, for the short description).
   - `listfiles_highlight()` skips ANSI escape sequences (`ESC [` ... final
     byte) as it skips Ctrl-A codes, so a match never splits one; color
     tracking ignores them (*my call*; this also fixes the lister's inline
     extended descriptions, which use the same helper). New tests in
     `exec/tests/listfiles/lib.js`.

### Docs

8. `ctrl/modopts.ini`: nothing (no options yet).
9. `docs/v322_new.md`: one line under the existing file transfer / loadable
   module entries.
10. Wiki: a short `module:extfileinfo` page, and the Extended File Info
    entry wherever the loadable modules are listed (SCFG system page).
    Change notices only with the user's OK.

## Testing

- `exec/tests/listfiles/lib.js` already covers term extraction and
  highlighting.
- Build with GCC (`synchronet-build` skill); the user rebuilds the live
  binaries (`libsbbs.so`, `scfg`).
- At a terminal (the user): with the module unset, the screen is the
  built-in one; set in SCFG, the same screen comes from `extfileinfo.js`
  (compare side by side for a file with every field: hashes, tags, author,
  uploader, downloads, extended description; a missing file; a free dir; as
  a sysop and not); search "NOFX" and press **E**: matches highlighted in
  both descriptions; with a `fileinfo` menu file present, the menu file
  shows and the extended description follows.

## Left to the builder

- Exact SCFG help wording, and where the new entry goes in the module list
  (next to View File Info).
- JS helpers for the comma-separated byte count and the 1024-based size
  estimate (match the C++ output exactly).
- Whether `exec_mod`'s return value is used (the module only displays;
  `invoked` decides the fallback).
