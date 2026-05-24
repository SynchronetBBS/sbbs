---
name: smbutils
description: Use when inspecting, validating, repairing, importing into, or maintaining a Synchronet Message Base (SMB) file — mail (mail.shd), message sub-boards (data/subs/<code>.shd), or file bases (data/file/<code>.shd) — via the smbutil, chksmb, or fixsmb command-line tools. Trigger whenever the user mentions smbutil, chksmb, fixsmb, SMB files, .shd / .sdt / .sid / .sha files, "the mail base", "rebuild/renumber the message base", "pack the sub", "check the message base for corruption", "repair an SMB", "dump a hash file", "view headers", "import a message", or any low-level inspection, validation, or repair of a Synchronet messaging file that isn't doable from inside the BBS or MsgBase JS API. Prefer this skill over guessing flags or shelling out blind.
---

# Synchronet SMB Utilities — smbutil, chksmb, fixsmb

Three command-line tools work on Synchronet Message Base (SMB) files. They
overlap in scope but each has a specific job:

- **`smbutil`** — the Swiss-army knife. List/read/view messages, import,
  pack, run maintenance, lock, light repair (re-init headers).
- **`chksmb`** — **read-only** integrity checker. Walks the base and reports
  corruption. Run this first when something looks wrong.
- **`fixsmb`** — targeted repair: rebuild the index, undelete, renumber,
  rehash. Use after `chksmb` tells you what's broken.

SMB is the common on-disk format for:

- Local email (`data/mail.shd`)
- Message sub-boards (`data/subs/<subcode>.shd`)
- File bases (`data/file/<dircode>.shd`)

If the task can be done from inside Synchronet (SCFG, the BBS itself, or a
`MsgBase` JS script via `jsexec`), prefer that. Reach for `smbutil` when:

- The BBS is offline / a base is corrupt and JS can't open it
- You need a quick listing/dump that doesn't justify a script
- You're importing FidoNet / netmail / e-mail from a text file
- You're packing/repairing a base from cron or a shell wrapper

## Locating the install

The path conventions in this skill are relative to the Synchronet install
root. Use the **`$SBBSCTRL`** environment variable to find a live install
— it points to the `ctrl/` subdirectory, and the install root is its
parent. So on any properly-configured host:

```bash
SBBS="$(dirname "$SBBSCTRL")"        # install root
"<sbbs>/exec/smbutil"  ...            # binary
"<sbbs>/data/mail"                    # mail base
"<sbbs>/docs/smb.html"                # local SMB spec (historical)
```

Recipes below use shorthand: bare command names (`smbutil`, `chksmb`,
`fixsmb`) and install-relative paths (`exec/...`, `data/...`, `docs/...`).
On a host where `<sbbs>/exec` isn't on `PATH`, prepend `"<sbbs>/"` or `cd
"<sbbs>"` first.

The `.exe` variants of each binary in `exec/` are for Windows — don't run
them on Linux. Conversely on **Windows** (incl. Git Bash / MSYS / WSL talking
to a Windows install), you must call the `.exe` explicitly — `./smbutil` will
attempt to exec the Linux ELF and fail with `Exec format error`. Use
`./exec/smbutil.exe` (or `cd <sbbs> && exec/smbutil.exe ...`). Also note
`$SBBSCTRL` on Windows is a Windows-style path like `C:\sbbs\ctrl` (or
whichever drive the install lives on); from Git Bash / MSYS the same path
is reachable via the `/c/sbbs/ctrl` mount-point form.

## SMB format reference

Before doing anything non-trivial — interpreting a `chksmb` error, deciding
whether `-renumber` is safe, writing a script that reads SMB directly —
consult the format spec. Knowing what each on-disk file does demystifies
most of these tools' commands.

- **Wiki spec (current, recommended):** https://wiki.synchro.net/ref:smb
  — full on-disk format, header field semantics, hash-file design,
  attachment and voting records, the SMBLIB C API surface.
- **Local copy of the original 1993 spec (v1.21):** `docs/smb.html` in the
  install (also at https://synchro.net/docs/smb.html) — historical baseline,
  superseded by the wiki page for current behavior.
- **Reference implementation:** the SMBLIB C library these utilities link
  against, in `src/smblib/` of the Synchronet source tree
  (https://gitlab.synchro.net/main/sbbs/-/tree/master/src/smblib).
  `SMB_VERSION` in `smblib.c` is the authoritative format version.

Quick file-role cheat-sheet (the per-base on-disk set):

| File | Role |
|------|------|
| `.shd` | **Status + headers** — base-level status block followed by per-message header records. The "main" file. |
| `.sdt` | **Data** — message bodies and binary attachment payloads. |
| `.sid` | **Index** — per-message index entries for fast lookup by index/number. |
| `.sha` | **Hash file** — content/from/to/subject hashes for dedupe and lookup. (Filename `.hash` is also seen on some bases — same role.) |
| `.sda` | **Data allocation** map (Fast/Hyper allocation modes). |
| `.ini` | **Per-base configuration** — max messages, age limits, attribute defaults, etc. |

## Filespec convention (all three tools)

Each tool takes one or more SMB base paths. The path is the base name
**without** the per-file extension — the tool opens the full file set
(`.shd`, `.sdt`, `.sid`, `.sha`, `.sda`, `.ini`). Passing the `.shd`
extension explicitly also works; it's stripped.

```
data/mail                          # local e-mail
data/subs/syncdata.synchronet      # sub-board
data/file/<dircode>                # file base
```

All three tools accept one or more bases — shell globs (e.g. `*.shd`) work
because each tool iterates over the expanded list. For example,
`smbutil m data/subs/*.shd` runs maintenance on every sub.

# smbutil

## Invocation form

```
smbutil [opts] <cmd>[<cmd>...] <filespec>[.shd] [<filespec> ...]
```

`<cmd>` is a **single letter** (case-sensitive — `D` is destructive, `d` is
reversible). Some commands accept an immediate suffix (e.g. `l10`, `r#42`,
`v-7`).

**Commands can be concatenated.** `smbutil mp data/subs/*.shd` runs
maintenance (`m`) and then pack (`p`) across every sub. The corruption-repair
recipe `smbutil -C Rcs <base>` chains re-init, change-status, and show-status
in one invocation.

**The mail base is auto-maintained** by the Synchronet Terminal Server (a
nightly internal pass). You don't normally need to run `smbutil m` on
`data/mail` — reach for it only when investigating corruption or running a
manual import.

Examples:

```bash
smbutil s data/mail                       # mail base status
smbutil l data/subs/syncdata.synchronet   # list sub messages
smbutil V10 data/mail                     # verbose view, start at index 10
smbutil v#1234 data/mail                  # view by msg number 1234
smbutil v-30 data/mail                    # view messages from last 30 days
```

## Commands

Group by what you actually want to do:

### Inspect / read

| Cmd | What it does |
|-----|--------------|
| `s` | Show message base status (totals, header info) — start here for any base |
| `l[n]` | List msgs/files starting at index `n` |
| `v[n]` | View msg headers from index `n` |
| `V[n]` | Verbose view (all header fields) |
| `r[n]` | Read full msgs (header + body) from index `n` |
| `x[n]` | Dump the raw msg index (`.sid`) |
| `h`  | Dump the hash file (`.sha`) — useful for duplicate-detection debugging |

`[n]` accepts three forms:

- `n` — 1-based **index offset** into the base
- `#n` — actual **message number** (the persistent ID stored in the header)
- `-n` — **age in days** (e.g. `v-7` = messages from the last week)

Use `-#<N>` to limit how many messages a `l`/`v`/`V`/`r` prints (e.g.
`-#10 l` → list first 10).

### Import

| Cmd | What it does |
|-----|--------------|
| `i[file]` | Import a generic message (RFC822-ish text); omit `file` to read stdin |
| `e[file]` | Import as e-mail (mail base) |
| `n[file]` | Import as netmail |

These pair with the `-t`, `-n`, `-u`, `-f`, `-e`, `-s` options to set
to/from/subject when the input doesn't already have headers. Use `-d` to skip
the prompts.

### Maintain / pack / repair

| Cmd | What it does |
|-----|--------------|
| `m`  | Run base maintenance — purge expired and over-max messages per `.ini` rules |
| `p[k]` | Pack the base (reclaim space). `k` = minimum reclaimable Kbytes to bother |
| `-a` | (option) Always pack — skip the compression-worthwhile check |
| `R`  | **Re-initialize / repair** SMB and status headers — use after corruption |
| `L`  | Lock the base for exclusive access (backup window) |
| `U`  | Unlock |
| `c`  | Change msg base status fields interactively |
| `Z`  | Hold an SMB header lock until keypress (lock-testing) |

### Destructive

| Cmd | What it does |
|-----|--------------|
| `d` | Flag **all** messages for deletion (reversible with `u` until next pack) |
| `u` | Unflag — clears the delete flag on all messages |
| `D` | **Delete and remove** all messages immediately — **not reversible** |

Confirm with the user before running `D`, `R`, or `p -a` on a live base.
For `R`, copy the base files aside first — re-init touches header structure.

## Verbose-dump (`V`) field names

`smbutil V[n]` prints the full per-message header. The fields aren't
documented in `--help`; the ones you'll actually filter on:

| Field | What it is |
|-------|------------|
| `index record` | 1-based index offset of this message in the base |
| `Subject` | Decoded subject line |
| `RFC822From` | Original `From:` header (raw, before SBBS rewrites) |
| `Sender` / `SenderNetType` / `SenderNetAddr` | SBBS-canonical sender (name, type, address) |
| `SenderTime` | UTC timestamp the sender stamped, format `YYYYMMDDHHMMSSZ` |
| `SenderIpAddr` / `SenderHostName` / `SenderProtocol` | Connection metadata |
| `To` | Recipient address (name or email) |
| `ToExt` | **Recipient user number** — `1` is the sysop, others are the user `.dat` index |
| `ToList` | Original RFC822 To/Cc list |

Records are separated by a blank line in `V` output, which makes them awk-friendly.

## "Show all mail for one user" recipe (smbutil-only)

`smbutil` doesn't take a "filter by recipient" flag, but you can awk over
verbose output. To list every message in `data/mail` addressed to user #1:

```bash
smbutil V data/mail | awk '
  /^index record/ { idx=$3; subj=""; from=""; date=""; toext=""; next }
  /^Subject /     { sub(/^Subject *[ \t]+/,""); subj=$0; next }
  /^RFC822From /  { sub(/^RFC822From *[ \t]+/,""); from=$0; next }
  /^Sender /      { if (from=="") { sub(/^Sender *[ \t]+/,""); from=$0 } next }
  /^SenderTime /  { sub(/^SenderTime *[ \t]+/,""); date=$0; next }
  /^ToExt /       { sub(/^ToExt *[ \t]+/,""); toext=$0; next }
  /^$/            { if (toext=="1") printf "%-15s [#%-6s] %.45s :: %s\n", date, idx, from, subj
                    idx=""; subj=""; from=""; date=""; toext="" }'
```

Swap `V` for `V-30` to limit to the last 30 days, or `V-7` for a week.
For very busy mail bases (~6k+ messages) prefer the windowed forms — full
`V` produces megabytes of output. The official answer for repeated /
filtered reads of one user's mail is still a `MsgBase` JS script via
`jsexec`; this awk recipe is the one-liner equivalent.

## `[n]` pitfalls

- `-#N` (limit count) is **not** supported by `smbutil` (`-#` is rejected:
  `Unknown opt '#'`). The `[n]` argument-suffix is a *starting position*,
  with three forms documented earlier (`n` index, `#n` msg number, `-n`
  age in days). To bound how much you read, use `V-7`/`V-30` to bound by
  age, or `V<startIndex>` to start partway through.
- The starting index `[n]` form is a **1-based** offset into the base, not
  a message number. Use `#n` if you have the persistent message ID.

## Frequently useful options

| Opt | Effect |
|-----|--------|
| `-c[m]` | Create the base if it doesn't exist (`m` = max msgs) |
| `-d` | Use defaults for to/from/subject prompts (good for scripted imports) |
| `-l` | LZH-compress imported message text |
| `-i` | Ignore dupes — don't write CRCs, don't check hash file |
| `-r` | Show raw body bytes (skip MIME decoding) — debugging only |
| `-v` | Increase console verbosity (repeatable) |
| `-o` | Send errors to stdout instead of stderr (easier to pipe/log) |
| `-C` | Continue past normally-fatal errors |
| `-z<tz>` | Set timezone offset for the imported message timestamp |
| `-t / -n / -u / -f / -e / -s` | to-name / to-addr / to-user# / from-name / from-user# / subject |

## Common recipes

**Inspect mail base health:**
```bash
smbutil s data/mail
```

**View the last week of mail with full headers:**
```bash
smbutil V-7 data/mail
```

**Find a specific message by number and dump its body:**
```bash
smbutil r#12345 data/mail
```

**Repair a corrupt sub-board** (after backing it up):
```bash
cp data/subs/syncdata.synchronet.* /tmp/smb-backup/
smbutil R data/subs/syncdata.synchronet
smbutil s data/subs/syncdata.synchronet   # verify
```

**Severely corrupted base** (`fixsmb` can't open it for index repair):
```bash
smbutil -C Rcs data/subs/badbase
```
`-C` continues past normally-fatal errors; `R` re-inits SMB/status headers,
`c` lets you reconfigure status values (max msgs, CRCs, age), `s` prints
the resulting status. After this, run `chksmb`, then `fixsmb` for any
remaining errors.

**Pack a sub to reclaim space** (only if >1MB recoverable):
```bash
smbutil p1024 data/subs/syncdata.synchronet
```

**Run scheduled maintenance** (drop expired + over-max messages):
```bash
smbutil m data/subs/syncdata.synchronet
```

**Import a netmail message from a text file:**
```bash
smbutil -t"SysOp" -s"Hello" n/tmp/msg.txt data/mail
```

For automated/scripted **posting**, the official docs recommend the
`postmsg.js` module over `smbutil i` — it understands sub-board codes,
applies the usual posting hooks, and is friendlier to maintain:

```bash
jsexec postmsg.js <subcode> "<from>" "<to>" "<subject>" </tmp/body.txt
```

(See the `jsexec` skill for details.)

# chksmb

`chksmb` walks an SMB base and **reports** problems — it's read-only. It
does **not** modify anything. Use it first when you suspect corruption,
when something fails to open from the BBS, or as a periodic health check.

## Invocation form

```
chksmb [-opts] <filespec>[.shd] [<filespec> ...]
```

Multiple bases can be checked in one run. Exit code is non-zero if errors
were found, so it's cron-friendly.

## Options

| Opt | Effect |
|-----|--------|
| `-b` | Beep on error |
| `-s` | Stop after the first errored base (useful when checking many) |
| `-p` | Pause after each errored base |
| `-h` | Skip the hash file check |
| `-a` | Skip the allocation file check |
| `-t` | Skip translation-string checks |
| `-i` | Skip message-ID checks |
| `-S` | Skip subject-CRC checks |
| `-N` | Skip to/from name-CRC checks |
| `-e` | Display extended info on each corrupted message |

The skip flags exist for cases where one file is so damaged that checking
it spams output and masks problems elsewhere — skip it and come back after
running `fixsmb` on that piece.

## Recipes

**Check the mail base:**
```bash
chksmb data/mail
```

**Check every sub-board, stop on first error, show extended info:**
```bash
chksmb -se data/subs/*.shd
```

**Cron-style nightly health check (exit code drives alerting):**
```bash
chksmb data/mail data/subs/*.shd || echo "SMB ERRORS" | mail sysop
```

# fixsmb

`fixsmb` performs **targeted repair** on an SMB base. Unlike `smbutil R`
(which re-initializes header structure), each `fixsmb` flag fixes one
specific kind of problem. Use it after `chksmb` tells you what's wrong.

## Invocation form

```
fixsmb [-renumber] [-undelete] [-fixnums] [-rehash] <smb_file> [<smb_file> ...]
```

Accepts multiple bases. The base name follows the same filespec rules.

## Repair flags

| Flag | What it does |
|------|--------------|
| `-renumber` | Rebuild the index (`.sid`), assigning fresh sequential numbers. **Use with caution** — message numbers are persistent identifiers; renumbering breaks any outside reference (FidoNet seen-by, archived URLs, user pointers) that mentions the old number. Prefer `-fixnums` when you only need to reconcile mismatches. |
| `-undelete` | Clear the "deleted" flag on every message in the base. Recovery option — pairs with an accidental `smbutil d` or a botched maintenance run, **as long as the base hasn't been packed yet**. |
| `-fixnums` | Reconcile the per-message numbers stored in headers with the index. Use when `chksmb` reports mismatched message numbers but the index is otherwise sane. |
| `-rehash` | Rebuild the hash file (`.sha`) from current message content. Use when `chksmb -h` reports hash mismatches, or after manual edits to message bodies. |

With no flags, `fixsmb` performs a generic index rebuild — equivalent in
spirit to `-renumber` plus integrity passes. Pass specific flags when you
know what you're targeting.

## Caution

`fixsmb` writes to the base. **Always back up first:**

```bash
cp data/subs/syncdata.synchronet.* /tmp/smb-backup/
fixsmb -renumber data/subs/syncdata.synchronet
chksmb data/subs/syncdata.synchronet     # verify
```

If the base is in active use, lock it first (`smbutil L`), repair, then
unlock (`smbutil U`). Better: stop the BBS, or at least the relevant
service, before repair.

# Investigating a corrupt base — full workflow

When something looks wrong (e.g. a sub fails to open, mail counts look
off, `chksmb` was triggered by cron):

1. **Identify** which base, from logs or the user's report.
2. **Back up** the full file set:
   `cp data/subs/<code>.* /tmp/smb-backup/`
3. **Run `chksmb -e`** to see exactly what's broken.
4. **Pick the right `fixsmb` flag** based on what chksmb reported:
   - "msg index out of order" → `-renumber`
   - "hash mismatch" → `-rehash`
   - "msg number doesn't match index" → `-fixnums`
   - Messages disappeared but base wasn't packed → `-undelete`
5. **Re-run `chksmb`** to confirm clean.
6. **`smbutil s`** to verify status totals look right.
7. **`smbutil p`** if there's reclaimable space after repair.

For end-of-life cleanup or scheduled trimming, prefer `smbutil m`
(maintenance honors the `.ini` retention rules) over manual deletes.

# When *not* to use these tools

- Bulk read/edit across many messages → write a JS script using `MsgBase`
  (see the `jsexec` skill). Iterating with
  `MsgBase.get_all_msg_headers()` is faster and more flexible.
- Reading a single user's mail → `MsgBase` in JS, filtering by
  `to_number` / `to_ext`.
- Anything reachable through SCFG → use SCFG. These utilities bypass
  configuration validation.

# Cross-references

Official wiki pages — consult these first for current option lists,
edge-case behavior, and any changes since this skill was written:

- https://wiki.synchro.net/util:smbutil
- https://wiki.synchro.net/util:chksmb
- https://wiki.synchro.net/util:fixsmb

Also:

- **SMB on-disk format spec:** https://wiki.synchro.net/ref:smb (and the
  brief cheat-sheet earlier in this file).
- `MsgBase` JS API (preferred for programmatic work): see the
  `jsexec` skill, and the stock JS modules in `exec/` of the
  Synchronet install for usage examples.
- Source code (authoritative when this skill, wiki, and help text disagree):
  https://gitlab.synchro.net/main/sbbs — see `src/sbbs3/smbutil.c`,
  `src/sbbs3/chksmb.c`, `src/sbbs3/fixsmb.c`, and the `src/smblib/`
  reference library.
