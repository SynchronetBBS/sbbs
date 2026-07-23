# Coding style in this directory

This directory mixes our own code with a vendored 3rd-party snapshot. Which
style rules apply depends on which side of that line a file is on.

## Which files are ours vs. vendored

**Ours:** `hw_term.c`, `syncmoo1_*.c`/`.h`, `syncmoo1.h`, `compat/`, `tests/`,
the build systems (`CMakeLists.txt`, `build.sh`, `deploy.js`, `build.bat`,
`jsexec deploy.js`), and the `*.md` docs (`CLAUDE.md`, `README.md`, `DESIGN.md`,
`PROVENANCE.md`).

**Vendored:** the entire `1oom/` subdirectory -- a frozen, unmodified snapshot
of [fork1oom/1oom](https://sourcecraft.dev/fork1oom/1oom) (GitHub is a mirror;
see [PROVENANCE.md](PROVENANCE.md) for the exact commit, date, and the two
frozen generated headers `1oom/src/config.h` + `1oom/src/version.inc`).

## The ours-vs-vendored contract

All door logic -- terminal I/O, input decode, DOOR32.SYS/session setup,
per-user config, the future node overlay -- lives in **our** files:
`hw_term.c` (the 1oom `hw` backend) plus `syncmoo1_*.c`/`.h`. `1oom/` supplies
only the game engine (`game/`, `os/`, `ui/`) via the pluggable `hw` backend
seam it already has (peer of its own `hw/sdl`, `hw/alleg`, `hw/nop`).

The same seam argument applies to 1oom's *other* pluggable backend directory,
`os/`: on Windows we compile **our own** `syncmoo1_os_win32.c` instead of the
vendored `1oom/src/os/win32/os.c`, because that file's `os_make_path()` omits
the "directory already exists" guard its `os/unix` sibling has -- which breaks
`cfg_save()` and, worse, game saving. Fixing it in our own backend keeps
`1oom/` frozen. `*nix` still compiles 1oom's `os/unix/os.c` unchanged.

## Prefer xpdev

This door links **xpdev** (`src/xpdev`), so reach for it before hand-rolling
platform portability -- the same directive as
[../syncconquer/CLAUDE.md](../syncconquer/CLAUDE.md), and the practice
SyncDuke/SyncDOOM follow:

- **Sockets**: `sockwrap.h` -- it normalizes `WSAGetLastError()` into the POSIX
  `errno` values, so one `EWOULDBLOCK`/`EINTR`/`ENOBUFS` test classifies a
  failed `send()`/`recv()` on both platforms. No per-call `#ifdef _WIN32`.
- **Time / sleep**: `genwrap.h`'s `xp_timer64()` (monotonic ms), `xp_timer()`,
  `SLEEP()`.
- **Strings**: `stricmp`/`strnicmp` (genwrap.h), not `_stricmp` vs `strcasecmp`.
- **Files / dirs**: `dirwrap.h`'s `mkpath()`, `isdir()`, `FULLPATH()` (note its
  `(target, path, size)` argument order -- the reverse of `realpath()`, which
  MSVC does not have at all). Also `ini_file.h`, `safe_snprintf`.

**All of the door's Windows-vs-POSIX difference is confined to
`syncmoo1_plat.c`** (clock, sleep, non-blocking descriptor I/O, Winsock
bring-up, SIGPIPE, console-less stderr capture) plus `syncmoo1_os_win32.c`. It
is the only translation unit
that includes `<winsock2.h>`/`<windows.h>` -- deliberately, since those macros
mix badly with the vendored 1oom headers. Don't scatter `#ifdef _WIN32` back
into `syncmoo1_io.c` / `_input.c` / `_door.c` / `hw_term.c`; add to the
`syncmoo1_plat.h` seam instead.

`compat/` holds three MSVC-only shims, on the include path for MSVC alone, so
that `1oom/` stays unedited: `strings.h` (the vendored `util.h` includes it),
`unistd.h` (the vendored `game_save.c` includes it; our `syncmoo1_config.c`
uses it for `getcwd`/`chdir`), and `msvc_compat.h`, force-included with `/FI`
to define away the one GCC `__attribute__((noreturn))` in `1oom/src/log.h`.

**Default: don't edit the vendored `1oom/` tree.** If the engine's own code
needs to change to *build or run* under this door, that is a signal the door
glue is missing something, not a reason to patch upstream — reach for the
`hw`/`os` backend seam first. Legacy-code compiler warnings from `1oom/` are
absorbed in the build (`-w -fcommon` on the `syncmoo1` target, see
`CMakeLists.txt`) rather than silenced by touching the vendored sources.

The one sanctioned reason to edit `1oom/` directly is a **UI-layer
customization that genuinely has no backend seam** (a hardcoded menu string, a
new-game flow that ignores a pre-fill). Those are allowed, but each such edit
MUST be minimal, tagged in-code with a `syncmoo1 door: … PROVENANCE #N`
comment, and listed in PROVENANCE.md's "Local patches" section so a re-vendor
can re-apply them. This matches how `../syncconquer/PROVENANCE.md` tracks its
vendored edits. Anything with a backend seam still goes through the glue, per
the paragraph above.

The two exceptions are `1oom/src/config.h` and `1oom/src/version.inc`:
autotools-generated headers that 1oom's own build would normally regenerate
per-host. Since this project never runs 1oom's autotools, they are frozen,
committed, hand-verified-once artifacts (see PROVENANCE.md for the exact
`configure` invocation and the one deliberate `HAVE_SAMPLERATE` edit) -- not
something to regenerate or hand-edit further.

## Our code -- house style is 4-space, NOT the repo's uncrustify tabs

**Actual, established style:** every `syncmoo1_*.c`/`.h` file and `hw_term.c`
is written with **4-space indentation** (verified: zero tab-indented lines
across all of them). This document describes the real style in the code, not
the plan's original assumption.

**Note/deviation:** the syncmoo1 design plan's Global Constraints named
[../../uncrustify.cfg](../../uncrustify.cfg) (tabs, the house style every
other door in this tree follows) as the intended style. The code as written
uses 4-space indentation throughout instead. This is flagged here as a known
deviation for a possible later reformat decision -- **do not run uncrustify
over these files** or otherwise reformat them as a side effect of unrelated
work; a deliberate reformat pass (if ever done) should be its own reviewed
change, not incidental to a feature commit.

For any new C code in this door, match the existing 4-space style rather than
introducing tabs partway through a file.

## Vendored code -- keep upstream intact

- **Don't edit `1oom/` for build/runtime problems.** No reformatting, no "just
  this one warning fix," no house-style pass. If a build or runtime problem
  seems to require a change inside `1oom/`, that need almost certainly belongs
  in `hw_term.c` or a `syncmoo1_*.c` module instead (a config define, a link
  flag, a wrapper) -- see PROVENANCE.md and DESIGN.md §2/§13 for why this
  door is a new `hw` backend rather than a source patch.
- **UI-layer customizations with no backend seam** (a hardcoded menu string, a
  new-game flow that discards a pre-fill) may be edited directly, but only as
  documented **local patches**: minimal, tagged in-code with a
  `syncmoo1 door: … PROVENANCE #N` comment, and listed in PROVENANCE.md's
  "Local patches" section. Keep that list complete -- a re-vendor re-applies
  them by hand.
- The two frozen generated headers described above are also not hand-edited
  casually -- re-generate them from a fresh upstream commit per PROVENANCE.md's
  "Updating" section rather than hand-patching the committed copy.
