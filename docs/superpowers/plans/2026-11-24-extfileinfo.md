# Extended File Info Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An optional Extended File Info loadable module (`extfileinfo`) run by the built-in file information display, and a stock `exec/extfileinfo.js` that reproduces that display while highlighting what a file search found in the short and extended descriptions.

**Architecture:** C++ gains the module setting and hands `showfileinfo()` to it, passing the directory code, the quoted filename, the extended-description flag and the search expression (`%` and `"` hex-encoded). The search reaches `showfileinfo()` explicitly through `viewfile()`, `batchflagprompt()` and `bbs.view_file_info()`. The JS module reuses the `listfiles_lib.js` highlighter.

**Tech Stack:** C++ (`src/sbbs3`, `src/encode/hex.c`), C (SCFG), JavaScript (SpiderMonkey 1.8.5 and 128).

**Spec:** `docs/superpowers/specs/2026-11-24-extfileinfo-design.md`

## Global Constraints

- JS: no `let`/`const`, arrow functions, template literals, `for...of`, `Object.keys()`; runs on SM 1.8.5 and SM 128 (`exec/CLAUDE.md`).
- C/C++ compiles under GCC, Clang and MSVC; only GCC is available here. Say so when handing over.
- Module setting name **Extended File Info**, ini key `extfileinfo`, script `exec/extfileinfo.js`.
- Module arguments: `<dir code> "<filename>" <0|1> [search]`, search encoded with `hex_encode('%', search, "\"", buf, sizeof buf)`; empty search: no argument.
- The module draws only; every prompt stays with its caller.
- Never `make install`; the user rebuilds and deploys the live binaries.
- **No commits during the tasks.** Commits happen only after the user confirms the whole feature live (Task 6), batched, each with the user's OK (`~/.claude` memory: validate before commit, don't over-commit, squash feature commits). Commit messages wrap at 78 columns, end with the `Co-Authored-By` trailer.

## Review Focus

1. **A file not on disk** (size -1): `FileIsNotOnline` (+ path for sysops) as in C++; no checksum lines; no `FiFileSize` line; `FiTransferTime` omitted.
2. **A filename with spaces** (e.g. `Lady Gaga - The Queen (Stems) [WAV].zip`): reaches the module as one argument and loads.
3. **A search with quotes or `%`** (`"NOFX" | 100%`): the module decodes exactly that expression and highlights whole words only for the quoted term.
4. **An extended description with ANSI color sequences** (FILE_ID.DIZ art) and a search term that is a digit or `m`: the sequences are left intact.
5. **A long search (12+ characters, no wildcards)** in the built-in lister: the original expression, not the `*`-widened file pattern, reaches the display (and the built-in listing's own short-description highlight).

---

### Task 1: Highlighter skips ANSI escape sequences

**Files:**
- Modify: `exec/load/listfiles_lib.js` (`listfiles_highlight()`)
- Test: `exec/tests/listfiles/lib.js`

**Interfaces:**
- Produces: `listfiles_highlight(text, terms, attr)` unchanged in signature; ANSI CSI sequences (`ESC [`, parameter/intermediate bytes, one final byte `@`-`~`) are invisible to matching and copied through untouched, like Ctrl-A codes.

- [ ] **Step 1: Write the failing tests** (append before the final `if (failures.length)`, using the file's `hl()` helper):

```js
check("highlight: skips ANSI sequences", hl("\x1b[1;33mA1m", "1 | m"), "\x1b[1;33mA{[1m}");
check("highlight: match spans an ANSI sequence", hl("a\x1b[0mb", "ab"), "{[a\x1b[0mb}");
```

In the first, the `1`, `;`, `3`, `m` inside the sequence are not matched; the visible `1m` is.

- [ ] **Step 2: Run to verify they fail**

Run: `cd /sbbs && ./exec/jsexec -n tests/test.js listfiles`
Expected: `FAILED!` naming the two checks.

- [ ] **Step 3: Implement** in both passes of `listfiles_highlight()` (building `plain`, and building `result`): at `"\x1b"` followed by `"["`, treat the run through the first byte in `0x40`-`0x7e` as one code (skip it for `plain`; copy it for `result`, without ending or reopening a highlight run and without changing `cur`).

- [ ] **Step 4: Run to verify they pass**

Run: `cd /sbbs && ./exec/jsexec -n tests/test.js listfiles`
Expected: `lib.js ... passed`

---

### Task 2: extfileinfo library (pure helpers)

**Files:**
- Create: `exec/load/extfileinfo_lib.js`
- Create: `exec/tests/extfileinfo/lib.js` (load the library by path as `exec/tests/listfiles/lib.js` does)

**Interfaces:**
- Produces:
  - `extfileinfo_decode_search(args)`: `args` is an array of strings (argv from index 3); returns them joined with single spaces, then every `%XX` (hex) decoded (JS equivalent of `hex_decode('%', ...)`); `[]` returns `""`.
  - `extfileinfo_commas(n)`: integer to string with `,` every three digits (C++ `u64toac(n, buf, ',')`).
  - `extfileinfo_byte_estimate(bytes)`: C++ `byte_estimate_to_str(bytes, buf, size, 1024, 1)`: `"%1.1fP/T/G/M/K"` at each 1024 power reached, else the plain integer.

- [ ] **Step 1: Write the failing tests**

```js
check("decode none", extfileinfo_decode_search([]), "");
check("decode words", extfileinfo_decode_search(["SOUL", "DOUBT"]), "SOUL DOUBT");
check("decode quotes", extfileinfo_decode_search(["%22NO", "FX%22", "|", "100%25"]), "\"NO FX\" | 100%");
check("decode literal %41 survives", extfileinfo_decode_search(["%2541"]), "%41");
check("commas", extfileinfo_commas(825824634), "825,824,634");
check("commas small", extfileinfo_commas(0), "0");
check("estimate bytes", extfileinfo_byte_estimate(1000), "1000");
check("estimate K", extfileinfo_byte_estimate(1024), "1.0K");
check("estimate M", extfileinfo_byte_estimate(825824634), "787.6M");
check("estimate G", extfileinfo_byte_estimate(1825361100), "1.7G");
```

(The M and G values match the `NOFX` listing seen on the BBS: `787.6M`, `1.7G`.)

- [ ] **Step 2: Run to verify they fail**

Run: `cd /sbbs && ./exec/jsexec -n tests/test.js extfileinfo`
Expected: `FAILED!` (functions not defined).

- [ ] **Step 3: Implement the three functions** in `exec/load/extfileinfo_lib.js` (file header comment, `"use strict";`, ends with `this;` like other `exec/load` libraries).

- [ ] **Step 4: Run to verify they pass**

Run: `cd /sbbs && ./exec/jsexec -n tests/test.js extfileinfo`
Expected: `lib.js ... passed`

---

### Task 3: Extended File Info module setting and hook (C++)

**Files:**
- Modify: `src/sbbs3/scfgdefs.h` (after `fileinfo_mod`), `src/sbbs3/scfglib1.c:403`, `src/sbbs3/scfgsave.c:296`, `src/sbbs3/load_cfg.c:450`
- Modify: `src/sbbs3/scfg/scfgsys.c` (Loadable Modules list near line 3601, help near 3644, `case` dispatch near 3756)
- Modify: `src/sbbs3/file.cpp` (`showfileinfo()`), `src/sbbs3/sbbs.h:1270`

**Interfaces:**
- Produces: `struct loadable_module extfileinfo_mod` in `scfg_t`; `void showfileinfo(file_t*, bool show_extdesc = true, const char* search = nullptr);`

- [ ] **Step 1: Config plumbing.** `extfileinfo_mod` read with `read_loadable_mod(cfg, section, "extfileinfo", "")`, written with `write_loadable_module(&ini, "extfileinfo", ...)`, freed with `free_loadable_module()`, each next to `fileinfo_mod`.

- [ ] **Step 2: SCFG.** New list entry `"Ext File Info"` (fits the `%-18.18s` column) directly after `"View File Info"`; its `case` calls `cfg_loadable_modules("Extended File Information", &cfg.extfileinfo_mod, mod_bar, 0)`; renumber the following `case` labels (Batch Transfer onward) by one. Help lines (keep the 18-column alignment):
  - `` `View File Info`   User views information on files matching a file spec `` (replaces the current line; it is the file-by-file command)
  - `` `Ext File Info`    User views one file's extended information (display only) ``

- [ ] **Step 3: Hook.** In `showfileinfo()`, after `current_file = f;` and `getfilepath()`: encode `search` (or `""`) with `hex_encode('%', search, "\"", enc, sizeof enc)` (`#include "hex.h"`; `char enc[256]`), call `exec_mod("extended file info", cfg.extfileinfo_mod, &invoked, "%s \"%s\" %u %s", cfg.dir[f->dir]->code, f->name, show_extdesc, enc)`; when `invoked`, set `current_file = NULL` and return.

- [ ] **Step 4: Build** with GCC per the `synchronet-build` skill (load the skill first; plain `make` in `src/sbbs3`, and in `src/sbbs3/scfg`).
Expected: no new warnings in the touched files.

- [ ] **Step 5: SCFG check** (`reference_scfg_stdio_automation` memory: `scfg -id` with piped input) or ask the user: System -> Loadable Modules lists `Ext File Info`, help shows both lines, setting it to `extfileinfo` and saving writes `extfileinfo = extfileinfo` under `[module]` in a scratch `ctrl` copy (never the live `ctrl/main.ini`).

---

### Task 4: Pass the search to the display (C++ and `listfiles.js`)

**Files:**
- Modify: `src/sbbs3/viewfile.cpp` (`viewfile(file_t*, bool)`), `src/sbbs3/sbbs.h:1290,1308`
- Modify: `src/sbbs3/listfile.cpp` (`listfiles()`, `batchflagprompt()`)
- Modify: `src/sbbs3/js_bbs.cpp` (`js_view_file_info()`, its `JSDOCSTR`)
- Modify: `exec/listfiles.js` (every `bbs.view_file_info()` call: lines 415 and 529 today)

**Interfaces:**
- Consumes: `showfileinfo(file_t*, bool, const char* search)` (Task 3)
- Produces:
  - `int viewfile(file_t* f, bool extdesc, const char* search = nullptr);` passes `search` to `showfileinfo()`
  - `int batchflagprompt(smb_t*, file_t* bf[], uint row[], int total, int totalfiles, const char* search = nullptr);` passes it to its three `viewfile()` calls
  - `bbs.view_file_info(dir_code, filename [,extended=true] [,search])`

- [ ] **Step 1: `listfiles()`:** before `filespec = liberal_filepattern(...)` (line 89), save `const char* search = (mode & FL_FIND) ? filespec : nullptr;`. Pass `search` to the `viewfile()` call (line 270), to both `batchflagprompt()` calls (lines 130, 310), and to `listfile()` at line 279 in place of the widened `filespec` (fixes the built-in listing's own highlight for long searches).

- [ ] **Step 2: `js_view_file_info()`:** a 4th string argument (`JSVALUE_TO_MSTRING`, freed after the call; ignored when not a string) passed to `viewfile()`. `JSDOCSTR` arguments become `"dir_code, filename [,extended=true] [,search]"` and the description gains: "When <i>search</i> (a search expression) is specified, an Extended File Info module may highlight what it found."

- [ ] **Step 3: `listfiles.js`:** pass `(this.mode & FL_FIND) ? this.filespec : undefined` (or `listing.` equivalent in scope) as the 4th argument at each call.

- [ ] **Step 4: Build** (as Task 3 Step 4). Run `cd /sbbs && ./exec/jsexec -n tests/test.js listfiles`. Expected: passed.

---

### Task 5: `exec/extfileinfo.js`

**Files:**
- Create: `exec/extfileinfo.js`

**Interfaces:**
- Consumes: `extfileinfo_decode_search()`, `extfileinfo_commas()`, `extfileinfo_byte_estimate()` (Task 2); `listfiles_search_terms()`, `listfiles_highlight()`, `listfiles_apply_attr_codes()`, `listfiles_options()`, `listfiles_defaults()` (`exec/load/listfiles_lib.js`); `attr_option()` (`exec/load/attr_option.js`); argv from Task 3.
- Produces: exit code 0.

- [ ] **Step 1: Load and validate.** `argv[0]` dir code (lowercase-insensitive lookup in `file_area.dir`), `argv[1]` filename, `argv[2]` `"1"` to show the extended description, search = `extfileinfo_decode_search(argv.slice(3))`. Open the `FileBase`, `get(name, FileBase.DETAIL.MAX)`; disk size `get_size(name)` (-1 when missing), disk time `get_time(name)`, indexed size from `get(name, FileBase.DETAIL.MIN).size`, path `get_path(name)`. Unknown dir or file: exit 0 having printed nothing.

- [ ] **Step 2: Header**, mirroring `showfileinfo()` in `src/sbbs3/file.cpp` line by line (that function is the reference; same order, conditions and `P_TRUNCATE`): if `bbs.menu("fileinfo", P_NOCRLF | P_NOERROR)` returns true, skip to Step 3. Otherwise print with `console.print(format(bbs.text(bbs.text.<Id>), ...), P_TRUNCATE)`:
  - `FiLib`: `dir.lib_index + 1`, library `description`; `FiDir`: `dir.index + 1`, dir `description`; `FiFilename`
  - `FiFileSize` when disk size >= 0: `extfileinfo_commas(size)`, `extfileinfo_byte_estimate(size)`
  - `FiCredits`: `bbs.text(bbs.text.FREE)` when the dir has `DIR_FREE` or (size > 0 and cost <= 0), else `extfileinfo_commas(cost)`
  - when disk size > 0 and equals the indexed size: `FiChecksum` with `"CRC-32"` (`format("%08x", crc32)`), `"MD5"`, `"SHA-1"`, each only if present on the file object
  - `FiDescription` (short description, highlighted: Step 4), `FiTags`, `FiAuthor`, `FiGroup` when non-empty
  - `FiUploadedBy` with `bbs.text(bbs.text.UNKNOWN_USER)` when `anon`, else `from`, when non-empty; then `" via " + from_protocol + " "` when set
  - for `dir.is_operator`: a second `FiUploadedBy` with `"[" + from_ip_addr + "] "` and `from_host_name + " "` when either is set
  - `FiUploadedTo` (`to_list`), `FiDateUled` (`system.timestr(added)`), `FiFileDate` when disk time > 0, `FiDateDled` (`system.timestr(last_downloaded || 0)`), `FiTimesDled`
  - `FiTransferTime` (no `P_TRUNCATE`, as in C++) when the time to download is > 0: C++ `gettimetodl()` with `bbs.download_cps` (`size < 1`: 0; `size <= cps`: 1; cps < 1 means 100000; else `Math.floor(size / cps)`), shown with `system.secondstr(t, true)` and the cps
  - `console.print(bbs.text(bbs.text.FileHdrDescSeparator), P_TRUNCATE)`

- [ ] **Step 3: Extended description and status.** When asked and non-empty: strip leading CR/LF, trim trailing white space, highlight (Step 4), `console.putmsg(text, P_NOATCODES | P_CPM_EOF | P_AUTO_UTF8)`, `console.newline()`. Then, only if the header was printed (no menu file) and disk size is -1: `FileIsNotOnline` with the name, and for `user.is_sysop` the path + `"\r\n"`.

- [ ] **Step 4: Highlighting.** Terms `listfiles_search_terms(search)` (none: print as is). Color: the directory's merged List Files options (`listfiles_options(listfiles_defaults(), [listfiles], [listfiles:lib<N>], [listfiles:<code>])` loaded with `load({}, "modopts.js", section)`, as `dir_options()` in `exec/listfiles.js`), `found_attr` through `attr_option()`; empty means the toggle-bright mode: for the short description the start color is `listfiles_apply_attr_codes(<FiDescription text before its %s>, console.attributes)`, for the extended description `console.attributes`.

- [ ] **Step 5: Compile check**

Run: `cd /sbbs && ./exec/jsexec -n -r 'var f=new File(system.exec_dir+"extfileinfo.js");f.open("r");var s=f.read();f.close();new Function(s.replace(/^"use strict";/m,""));print("ok")'`
Expected: `ok`

---

### Task 6: Docs, live test, commits

**Files:**
- Modify: `docs/v322_new.md` (one line where loadable modules / file areas are listed)
- Wiki (after the user's OK, `synchronet-wiki` skill): new `module:extfileinfo`; Ext File Info added where SCFG's Loadable Modules are documented; `module:listfiles` notes `found_attr` also colors Extended File Info

- [ ] **Step 1: Release note line**, matching the neighboring bullets' style (e.g. "New Extended File Info loadable module and stock `extfileinfo.js`: highlights what a file search found in file descriptions").

- [ ] **Step 2: Hand to the user for the live test** (they rebuild `libsbbs.so` and `scfg`; enable `extfileinfo` for SYSOP only, as `listfiles` is): module unset vs set look identical for a file with every field, a missing file, a free dir, as sysop and not; search `NOFX` then **E**: both descriptions highlighted; a `fileinfo.*` menu file present: menu shown, extended description follows; `"NOFX"` highlights whole words only. Note the change was built with GCC only.

- [ ] **Step 3: Commits, after the user confirms, each with their OK:**
  1. `listfiles.js`: search-match highlighting in descriptions (the earlier uncommitted work + Task 1)
  2. C++: Extended File Info module setting, hook and search plumbing (Tasks 3-4 C++)
  3. `extfileinfo.js` + library + tests + `listfiles.js` pass-through + release note + the spec and this plan
