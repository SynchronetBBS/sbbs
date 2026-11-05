---
name: filebase
description: Use for Synchronet file bases (file transfer directories) from JavaScript — the FileBase class (open, get, get_list, add, update, remove, renew, hash, lock), file metadata objects, FILE_ID.DIZ import, filename rules and length limits, directory settings flags, and the stock addfiles.js importer. Trigger whenever you add, remove, rename, list, describe or bulk-import files in a Synchronet file area; when asked "why was this file skipped/rejected by addfiles", "how do I set a file's description", "what filenames does Synchronet allow", "how long can a filename or description be"; when writing a script that touches file_area.dir, FileBase, or data/dirs/*.shd; or before planning any batch job that puts files into a file directory. Read it before the first FileBase.add() call — add() has traps (no existence check, commas are illegal in filenames) that only show up after a bad import.
---

# Synchronet file bases — the `FileBase` API

A Synchronet *file directory* (SCFG → File Areas → library → directory) has
two halves:

- **the storage path** — an ordinary folder holding the files themselves
  (`file_area.dir[code].path`);
- **the file base** — an SMB-format database of metadata (names,
  descriptions, uploader, hashes, download counts) kept under the
  directory's `data_dir` as `<code>.shd`, `.sid`, `.sdt`, `.sda`, `.sha` and
  `.ini`.

A file sitting in the storage path is invisible to users until it has an
entry in the file base. `FileBase` is the JavaScript class that reads and
writes those entries (Synchronet v3.19+). Run it under `jsexec` or inside the
BBS; see the `jsexec` skill for invocation and the `javascript` skill for the
dialect.

Change a file base through `FileBase` (or `addfiles.js`, which uses it), not
by copying, editing or deleting the `<code>.s*` files. The class takes the
SMB locks that every other accessor — terminal, FTP and web servers, other
hosts sharing the install — relies on.

## Open, use, close

```javascript
var fb = new FileBase("mycode");        // internal code, lower-case
if (!fb.open())
    throw new Error("open: " + fb.error);
try {
    print(fb.files + " files");
} finally {
    fb.close();
}
```

- The constructor **throws** on an unknown code:
  `Unrecognized file area (directory) internal code: 'x'`.
- `file_area.dir` is keyed by the **lower-case** internal code, whatever case
  SCFG shows.
- Properties read before `open()` are placeholders (`files` is `0`). Open
  first.
- `open()` on a directory that has no base yet creates an empty one, so a
  directory just added in SCFG needs no separate initialization.
- After any `false` return, `fb.error` (alias `fb.last_error`) has the reason
  and `fb.status` the SMB library result code.
- `fb.lock()` / `fb.unlock()` (v3.21+) take the base for maintenance: while
  locked, other opens fail. `close()` unlocks.

Other properties: `file` (base path without extension), `is_open`, `dirnum`,
`first_file`, `last_file`, `last_file_time`, `update_time`, `max_files`,
`max_age`, `attributes`, `retry_time`, `retry_delay`.

## Reading

```javascript
var f    = fb.get("name.zip", FileBase.DETAIL.EXTENDED);   // null if absent
var list = fb.get_list("*.zip", FileBase.DETAIL.NORM);     // array of objects
var names = fb.get_names("*");                             // array of strings
```

`get_list([filespec] [,detail] [,since_time] [,sort=true [,order]])` and
`get_names([filespec] [,since_time] [,sort=true [,order]])` default to the
sysop-configured sort order.

| `FileBase.DETAIL` | value | adds |
|---|---|---|
| `MIN` | 0 | index record only: `name`, `vpath`, `size`, `time`, hashes |
| `NORM` | 1 | header fields (`desc`, `from`, `cost`, `added`, download counts) — the default |
| `EXTENDED` | 2 | `extdesc` |
| `AUXDATA` | 3 | `auxdata` |
| `MAX` | 4 | everything |

`FileBase.SORT`: `NAME_AI` 0, `NAME_DI` 1 (case-insensitive), `NAME_AS` 4,
`NAME_DS` 5 (case-sensitive), `DATE_A` 2, `DATE_D` 3, `SIZE_A` 6, `SIZE_D` 7,
`NATURAL` 8.

### The file metadata object

| property | meaning |
|---|---|
| `name` | filename (the only required property) |
| `vpath` | virtual path, e.g. `Music/MOGG/name.zip` — read-only |
| `desc` | one-line summary, 58 characters or fewer |
| `extdesc` | multi-line extended description (e.g. from a DIZ) |
| `from` | uploader's user name |
| `from_ip_addr`, `from_host_name`, `from_protocol`, `from_port` | uploader's connection, when known |
| `author`, `author_org` | e.g. from a SAUCE record |
| `to_list` | comma-separated recipient user numbers (user-to-user transfers) |
| `tags` | space-separated tags |
| `anon` | uploaded anonymously |
| `size` | bytes, as of upload |
| `cost` | credit value (0 = free) |
| `time` | file modification time (time_t) |
| `added` | upload/import time (time_t) |
| `last_downloaded`, `times_downloaded` | download history |
| `crc16`, `crc32`, `md5`, `sha1` | content hashes |
| `auxdata` | auxiliary JSON, as a string |

- **`desc` can be `undefined`.** An entry added without a summary has no
  `desc` property at all; guard before calling string methods on it. Below
  `MAX`, every property with no value is omitted the same way; at `MAX`
  the string ones come back as `""`.
- **`auxdata` is a JSON string, not an object.** Its `archive_contents` key
  caches an archive's member listing (see `exec/load/filecontents_lib.js`),
  including for the first volume of a multi-part RAR.

Path helpers that need no open base: `fb.get_path(name)`, `fb.get_size(name)`
(-1 if the file is missing), `fb.get_time(name)`, `fb.hash(name_or_path)`
(returns `name`, `vpath`, `size`, `time`, `crc16`, `crc32`, `md5`, `sha1`),
`fb.format_name(name [,size=12] [,pad=false])` (a display form: the default
turns `Queen - One Vision.mogg` into `Queen -.mogg`), and `fb.dump(name)`
for a diagnostic header dump.

## Adding files

```javascript
var dir  = file_area.dir["mycode"];
var name = "Artist - Title.zip";
if (system.illegal_filename(name) || !system.allowed_filename(name))
    throw new Error("bad filename: " + name);
if (!file_exists(dir.path + name))
    throw new Error("not in storage path: " + name);
var ok = fb.add({
    name:    name,
    desc:    "Artist - Title",                 // <= 58 chars
    extdesc: lfexpand("line one\nline two"),   // CRLF line ends
    from:    "Sysop",
    cost:    file_size(dir.path + name)
}, false);                                     // false: do not import a DIZ
if (!ok)
    alert("add " + name + ": " + fb.error);
```

`add(file-meta-object [,use_diz] [,client])` returns `true` or `false`.

- **Put the file in the storage path first.** `add()` reads the size,
  modification time and (unless the directory disables hashing) content
  hashes from `dir.path + name`.
- ⚠️ **`add()` does not check that the file exists.** It records whatever the
  size lookup returns. `addfiles.js` checks before calling it; a script of
  your own has to do the same (`file_exists()` or `file_getcase()`).
- ⚠️ **`add()` does not apply the filename rules below either.** Call
  `system.illegal_filename()` and `system.allowed_filename()` yourself, as
  `addfiles.js` does.
- A name already in the base fails with `duplicate name found` in
  `fb.error`. Use `update()` to change an existing entry.
- **`use_diz` has three states.** Omitted: a description file inside the
  archive is imported only when the object has no `extdesc`. `true`: always
  imported, replacing `extdesc`. `false`: never. See
  [Description files](#description-files-file_iddiz) for what an import does.
- Extended descriptions are stored with CRLF line endings; pass text through
  `lfexpand()`.
- `cost` is yours to set. `addfiles.js` sets it to the file's size.

For a large import, open the base once, add serially from a single process,
and close at the end. On an install shared between hosts, run the script on
the same path every other accessor uses for the data directory, so the locks
meet.

## Changing and removing entries

- `update(filename, file-meta-object [,use_diz_always=false]
  [,readd_always=false])` — updates metadata. If the object's `name` differs
  from `filename` it **renames the file on disk** as well, and throws if the
  new name already exists in the base or the rename fails.
- `remove(filename [,delete=false])` — removes the entry. With `delete=true`
  it also deletes the file, and throws if that fails. With the default the
  file stays in the storage path, unlisted. A script that calls `remove()`
  with one argument and expects the disk to be cleaned up leaves orphans that
  no listing shows; compare the storage path against `get_names()` to find
  them. The default is also what makes a controlled rename possible: remove
  the entry, rename the file yourself, `add()` it under the new name.
- `renew(filename)` — removes and re-adds the entry so it shows as newly
  uploaded.

## Description files (`FILE_ID.DIZ`)

A `FILE_ID.DIZ` ("description in zip") is a small text file stored inside an
archive so the description travels with the download and any BBS can import
it. The convention from the BBS era:

- named `FILE_ID.DIZ`, at the root of the archive;
- at most **10 lines of 45 characters**;
- plain ASCII, CRLF line ends, no blank lines at the top;
- the first line says what the thing is, since that is what a one-line
  listing shows.

Synchronet enforces none of that shape. It does look for two variants as
well: `FILE_ID.ANS` (an ANSI-colored version, preferred when present) and
`DESC.SDI` (an older single-line description).

### What an import does

The same code runs for terminal and FTP uploads, `fb.add()`,
`fb.update(name, obj, /* use_diz_always: */ true)` and `addfiles -diz`:

1. Nothing happens unless the directory has DIZ support on (`DIR_DIZ`).
2. The archive is searched for `FILE_ID.ANS`, `FILE_ID.DIZ`, `DESC.SDI`,
   case-insensitively; the first of those with any content wins. The built-in
   reader handles every format libarchive reads (zip, 7z, rar, tar and
   compressed tars, ...), looking at the top level first and then inside
   subdirectories. If that finds nothing, the Extractable File Types command
   configured in SCFG for the file's extension is tried.
3. At most 5,000 bytes are read (`LEN_EXTDESC`), stopping at a Ctrl-Z.
4. ANSI color sequences are converted to Ctrl-A codes. If the file carries a
   SAUCE record, its width and iCE-color flag guide the conversion and its
   author and group become the entry's `author` and `author_org`.
5. The result becomes `extdesc`, replacing one you supplied if the import was
   forced.
6. **If the entry has no `desc`, one is derived from the text:** newlines
   become spaces, control characters and CP437 line-drawing characters are
   dropped (UTF-8 is converted to CP437 first), leading non-alphanumerics are
   skipped, runs of whitespace and of the same punctuation character collapse
   to one, and the result is cut at 58 characters.

Step 6 is why a DIZ that opens with a banner or a long program name gives a
poor one-line listing. Either make the first line the summary, or pass your
own `desc`.

### Writing one for an archive you build

- Keep to 10 x 45, ASCII, CRLF, with the summary as the first line. Wider or
  longer text is stored, but listings are laid out for that shape.
- Add it as `FILE_ID.DIZ` at the archive root. If the source material already
  has one, keep the original under another name rather than overwriting it.
- For a bulk import you can pass the same text as `extdesc` with
  `use_diz=false`. The listing is identical and no extraction runs per file;
  the DIZ inside the archive is then only for whoever downloads it.
- A file that is not an archive cannot carry a DIZ. Its description exists
  only in the file base, so set `desc` and `extdesc` at add time.
- To read one from a script: `new Archive(path).read("FILE_ID.DIZ")`.
- A loose `FILE_ID.DIZ` sitting in a storage path is on the default
  exclusion list of `addfiles.js`, so it is not added as a file.

## Filename rules

Two checks apply to uploads and to `addfiles.js`; both are exposed on
`system`.

`system.illegal_filename(name)` is `true` (reject) when the name:

- contains any of `\ / | < > : " ; , % ? *`
- contains `..`
- starts with `-`, `.` or a space
- ends with `.` or a space

`system.allowed_filename(name)` is `false` (reject) when the name:

- is longer than `filename_maxlen` (root key of `ctrl/file.ini`; default 64,
  minimum 8);
- contains `$` or a backtick;
- contains a space, unless SCFG allows spaces in filenames (`FM_SPACES`);
- contains a byte above 0x7F, unless extended ASCII / UTF-8 names are
  allowed (`FM_EXASCII`);
- or, when "safest filenames only" is on (`FM_SAFEST`), contains anything
  outside `A-Z a-z 0-9 - . _`.

`system.safest_filename(name)` tests that last, strictest set directly.

⚠️ **Commas, semicolons and percent signs are illegal**, which catches names
derived from titles (`Get Up, Stand Up`, `100% Pure`), as does an ellipsis
(`Wait...`). Strip or replace them before the file reaches the storage path,
then validate with the two calls above rather than a regex of your own.

⚠️ **The index holds 64 characters of name.** `fb.get_name(path)` returns the
index form; a longer name comes back shortened, with the extension kept. Two
long names that shorten to the same index form collide, so keep names at or
under 64 characters even when `filename_maxlen` has been raised.

## Limits

| what | limit | source constant |
|---|---|---|
| filename in the index | 64 | `SMB_FILEIDX_NAMELEN`, `MAX_FILENAME_LEN` |
| summary (`desc`) | 58 | `LEN_FDESC` |
| extended description | 5,000 | `LEN_EXTDESC` |
| directory internal code | 16 | `LEN_CODE` |
| directory short name | 25 | `LEN_SSNAME` |
| directory long name | 60 | `LEN_SLNAME` |

## The directory object

`file_area.dir[code]` describes the directory: `code`, `name` (short),
`description` (long), `lib_name`, `path` (storage), `data_dir`, `settings`,
`sort`, `max_files`, `max_age`, `vdir`, `vpath`, `ars`, `upload_ars`,
`download_ars`, `files`, `update_time`, and per-user `can_access`,
`can_upload`, `can_download`.

`settings` bits (the `DIR_*` names exist only after `load("sbbsdefs.js")`):

| bit | name | meaning |
|---|---|---|
| 0 | `DIR_FCHK` | check that listed files exist |
| 1 | `DIR_RATE` | force uploads to be rated |
| 2 | `DIR_MULT` | ask for multi-disk numbering |
| 3 | `DIR_DUPES` | search this directory for upload duplicates |
| 4 | `DIR_FREE` | free downloads |
| 5 | `DIR_TFREE` | download time is free |
| 6 | `DIR_CDTUL` | credit uploads |
| 7 | `DIR_CDTDL` | credit downloads |
| 8 | `DIR_ANON` | anonymous uploads |
| 9 | `DIR_AONLY` | anonymous only |
| 10 | `DIR_ULDATE` | show upload date in listings |
| 11 | `DIR_DIZ` | FILE_ID.DIZ / DESC.SDI support |
| 12 | `DIR_NOSCAN` | exclude from new-file scans |
| 13 | `DIR_NOAUTO` | exclude from `addfiles -auto` |
| 14 | `DIR_ULTIME` | deduct time during uploads |
| 15 | `DIR_CDTMIN` | award minutes instead of credits |
| 16 | `DIR_SINCEDL` | purge by days since last download |
| 17 | `DIR_MOVENEW` | moved files are marked new |
| 18 | `DIR_QUIET` | do not notify the uploader of downloads |
| 19 | `DIR_NOSTAT` | exclude transfers from system statistics |
| 22 | `DIR_NOHASH` | do not calculate or store content hashes |
| 23 | `DIR_FILETAGS` | allow user-specified tags |

`sort` uses the same numbers as `FileBase.SORT`.

Data files: `<data_dir>/<code>.shd` (headers), `.sid` (index), `.sdt`
(descriptions and auxdata), `.sda` / `.sha` (allocation), `.ini`. Files named
`<code>.dab`, `.dat`, `.exb` or `.ixb` are the pre-v3.19 format, left behind
by the upgrade.

### Adding a directory without SCFG

Directories live in `ctrl/file.ini`, one section each, and the file can be
edited by hand. The section name is `[dir:<library name>:<code suffix>]`; the
internal code is the library's `code_prefix` followed by that suffix, and
`file_area.dir` is keyed by it in lower case.

```ini
[dir:Music:MULTITRACK]
	description=Multitrack audio: Rock Band .mogg files and stem archives
	name=Multitrack Audio
	ars=
	upload_ars=REST NOT G
	download_ars=
	operator_ars=
	exempt_ars=
	area_tag=
	path=/xfer/multitrack/
	vdir=
	vshortcut=
	data_dir=
	upload_sem=
	extensions=
	settings=0x8dd
	seq_dev=0
	sort=0
	max_age=0
	max_files=0
	upload_credit_pct=0
	download_credit_pct=0
```

- `description` is the long name and `name` the short one (`LEN_SLNAME` 60,
  `LEN_SSNAME` 25).
- Copy a sibling directory's section and change `description`, `name` and
  `path`; that carries over the toggles and access strings in one step. Put
  the new section among the same library's directories. Keys are indented
  with a tab and the file uses LF line endings.
- Back the file up first, and write it through the live install path
  (`/sbbs/ctrl/file.ini`), not around it.
- Create the storage directory yourself, writable by the BBS user.
- `jsexec` reads the configuration fresh, so this verifies the edit at once:

  ```
  jsexec -r "var d=file_area.dir['multitrack']; print([d.lib_name, d.name, d.path, '0x'+d.settings.toString(16), d.upload_ars].join(' | '));"
  ```

- **Running servers do not see the directory until they recycle.** SCFG
  touches `ctrl/recycle` when it saves; after a hand edit, do it yourself
  (`touch ctrl/recycle`). The terminal, FTP, mail and services servers
  recycle within about a minute. **The web server checks the semaphore only
  while it has no session threads running** (`websrvr.cpp`: `thread_count`
  must be down to the server's own two or three), so a web server that is
  never idle keeps the old configuration for hours and `/files/<lib>/<dir>/`
  returns 404 the whole time. Look for `Recycle semaphore file ... detected`
  in its log; if it never appears, restart the web server service.

## `addfiles.js` — the stock importer

When the files are already in the storage path and one description source
fits them all, use the stock script instead of writing one:

```
jsexec addfiles.js <dir-code> [-options] [listfile] [desc-off]
```

| option | effect |
|---|---|
| `-all`, `-lib=<name>`, `-dir=<code>,...` | choose directories (`-all` and `-lib` imply `-auto`) |
| `-auto` | only directories with Auto-ADDFILES enabled |
| `-from=<name>` | uploader name |
| `-file=<spec>` | files to add (wildcards; default `*`) |
| `-ex=<name>` | add to the excluded list (default `FILES.BBS`, `DESCRIPT.ION`, `00_INDEX.TXT`, `SFFILES.BBS`, `FILE_ID.DIZ`) |
| `-diz` | always use the description inside the archive |
| `-update` | update existing entries instead of skipping them |
| `-readd` | re-add existing entries so they appear new |
| `-date`, `-fdate`, `-adate` | prefix today's, the file's, or the newest archived file's date to the description |
| `-delete` | delete the list file after import |
| `-v`, `-debug` | more output |

It skips, with a message only at `-v`, any name that is excluded, illegal, or
not allowed — the usual answer to "why didn't my file get added". Its source
is the reference for a correct add loop. Related stock scripts: `delfiles.js`,
`filelist.js`, `fileman.js`.

Write your own loop when each file needs its own description, uploader or
cost, or when the batch must be resumable and logged per file.

## Cross-references

- **The JavaScript dialect and other host classes** → `javascript`.
- **Running scripts, `jsexec` flags** → `jsexec`.
- **Checking or repairing the SMB files underneath** → `smbutils`.
- **API source of truth** → `src/sbbs3/js_filebase.cpp` (the `JSDOCSTR`
  text), `src/sbbs3/filedat.c` (filename rules, DIZ import),
  `src/smblib/smbfile.c` (`smb_addfile`), and the generated object model at
  https://synchro.net/docs/jsobjs.html.
