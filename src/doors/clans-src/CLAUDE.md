# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**The Clans** is a multi-user BBS door game written in C (v0.97b2, GPL v2). It's a text-based fantasy RPG featuring clans, empires, villages, combat, and inter-BBS multiplayer. Originally created by Allen Ussher (1997-2002), now modernized for FreeBSD/Linux/Windows.

## Build Commands

```sh
gmake -j8          # Build game binaries (default target)
gmake -j8 devkit   # Build development kit tools
gmake -j8 all      # Build everything (game + devkit)
gmake clean        # Clean build artifacts
gmake deepclean    # Full clean including bin/

# Debug build (enables -Wall -Wextra -Wconversion -pedantic -g -O0)
gmake -j8 DEBUG=1

# Windows cross-builds (run from src/ directory)
gmake -j8 CC=x86_64-w64-mingw32-gcc          # Win64 game binaries
gmake -j8 CC=x86_64-w64-mingw32-gcc devkit   # Win64 devkit (includes qtest)
gmake -j8 CC=i686-w64-mingw32-gcc            # Win32 game binaries
gmake -j8 CC=i686-w64-mingw32-gcc devkit     # Win32 devkit (includes qtest)
```

Build output goes to `bin/<platform>/` (e.g., `bin/freebsd.amd64.opt/`,
`bin/win64.amd64.opt/`). The build uses GCC with C17, `-Os -flto` for release.

## Testing

Run all tests (unit + integration) from the project root:

```sh
gmake test
```

This runs both the unit tests (`src/tests/`) and integration tests (`tests/`), in that order. Individual suites can also be run separately:

```sh
gmake -C src/tests/ test   # unit tests only
gmake -C tests/ test       # integration tests (devkit tools) only
```

### Integration tests (devkit tools)

Shell-based integration tests live in `tests/`. Each devkit tool has a corresponding script.

The GNUmakefile in `tests/` includes `mk/Platform.gmake` and `mk/Paths.gmake` to resolve the correct platform-suffixed binary names, and exports `BINDIR`, `EXEFILE`, `FIXTURE_DIR`, and `CLANS_TMPDIR` to the scripts. Fixtures live in `tests/fixtures/<tool>/`. Temporary output goes to `tests/tmp/` (created automatically, cleaned by `gmake -C tests/ clean`).

The harness (`tests/test_harness.sh`) provides: `assert_succeeds`, `assert_fails`, `assert_nonempty`, `assert_contains`, `assert_not_contains`, `assert_file_larger`.

### Manual game testing

The `test/` directory is a pre-configured game instance:

```sh
cd test/
./clans     # Run main game
./reset     # Run maintenance reset
./config    # Run configuration utility
./pcedit    # Edit player characters
```

## Architecture

### Key Source Files (`src/`)

| File | Purpose |
|------|---------|
| `clans.c` | Main entry point, pre-game menu |
| `ibbs.c` (~101KB) | Inter-BBS network, packet handling — see `docs/ibbs-notes.txt` for full protocol documentation |
| `myibbs.c` | Low-level Fidonet/netmail message creation layer (originally from OpenDoors sample code by Brian Pirie) |
| `empire.c` (~100KB) | Empire management, war mechanics, inter-BBS packet processing |
| `user.c` (~68KB) | Player/clan data, player management |
| `fight.c` (~50KB) | Combat system, damage calculations |
| `serialize.c` / `deserialize.c` | Binary data serialization with XOR encryption |
| `myopen.c` | Custom file I/O with XOR encryption wrappers |
| `door.c` | Low-level BBS door interface (OpenDoors library integration) |
| `video.c` | Terminal output, ANSI color codes, UI rendering |
| `structs.h` | All major data structures |
| `mstrings.h` | Language string macros (strings loaded from `strings.xl` binary) |

### Data Structures (in `structs.h`)

Core types: `struct clan`, `struct empire`, `struct village_data`, `struct pc` (player character), `struct game_data`, `struct army`, `struct strategy`.

### Serialization System

Binary files use explicit XOR encryption constants (e.g., `XOR_VILLAGE = 9`, `XOR_IBBS = 0x8A`). Buffer sizes are hardcoded in `myopen.h` (e.g., `BUF_SIZE_clan = 2249`). When adding fields to structs, update `serialize.c`, `deserialize.c`, and the buffer size constants.

### Language/String System

User-visible strings are compiled from `strings.u8.txt` → `strings.xl` using the `langcomp` devkit tool. Strings are accessed via macros in `mstrings.h`. Do not hardcode new user-visible strings; add them to `data/strings.u8.txt` and recompile. `src/mstrings.h` is auto-generated — do not hand-edit it. To rebuild: `gmake -C data/ strings.xl` (regenerates both `data/strings.xl` and `src/mstrings.h`).

### Inter-BBS (IBBS) System

Packet-based communication via Fidonet/netmail. The full protocol is documented in `docs/ibbs-notes.txt`. Key concepts:
- Packet filenames: `clFFFTTTDDI` (FFF=source BBS, TTT=dest BBS, DD=league ID, I=sequence index)
- Packet types: `PT_CLANMOVE`, `PT_NEWUSER`, `PT_DELUSER`, `PT_ADDUSER`, `PT_SUBUSER`, `PT_ATTACK`, `PT_ATTACKRESULT`, `PT_SPY`, `PT_RESET`, `PT_SCOREDATA`, `PT_ULIST`, etc.
- `BACKUP.DAT` tracks in-flight packets; stale `PT_CLANMOVE` re-adds the clan, stale `PT_ATTACK` returns troops
- The League Controller (BBS ID 1) is authoritative: new users go to LC first (via `PT_NEWUSER`), LC deduplicates and broadcasts via `PT_ADDUSER`; LC periodically sends full `PT_ULIST` to sync all nodes
- `myibbs.c` handles low-level Fidonet netmail message creation (file attach, kludge lines, INTL/FMPT/TOPT)

### DevKit Tools

Content is compiled using tools in `data/`: `langcomp`, `mcomp` (monsters), `mitems`, `mspells`, `mclass`, `makenpc`, `ecomp` (events), `makepak` (packages all data into `clans.pak`).

### Platform Abstraction

Conditional compilation via `#ifdef UNIXY` / `#ifdef WIN`. Platform-specific wrappers in `unix_wrappers.c` and `win_wrappers.c`. Build platform detection is in `mk/Platform.gmake`.

## Data File Line Endings

`rputs()` in `door.c` handles `\n` by calling `od_putch('\n'); od_putch('\r')`. Any file
read line-by-line (via `fgets()`) and then passed through `rputs()` must use **LF-only**
line endings. With CRLF input, `fgets()` on Unix delivers `"content\r"` and `rputs()`
appends another `\r`, producing a spurious leading `\r` on every output line.

**LF only**: all `.hlp`, `.evt`, `.asc` files in `data/`; `devkit/pak.lst`; plain text
files in `docs/`, `installer/`, `installerdk/`, `release/`.

**CRLF intentionally kept**: `data/genall.bat`, `devkit/genall.bat` (Windows batch files);
`release/clanad.ans` (ANSI art — treat as binary).

**`.gitattributes`** at the repo root overrides: `*.asc text !eol` (native line endings,
overriding parent's `eol=crlf`); `*.ans -text !eol` (binary).

### CP437 encoding and UTF-8 equivalents

Seven stock data files that formerly contained raw CP437 bytes have been
converted to UTF-8 with `.u8.` in the filename (e.g., `data/menus.u8.hlp`).
The original CP437 files have been deleted (available in git history).
The build system compiles exclusively from the `.u8.` files.

These files are valid UTF-8 and safe with Read, Edit, and Write tools:

| File | Content |
|------|---------|
| `data/menus.u8.hlp` | Main menu screens, box-drawing art |
| `data/strings.u8.txt` | Language strings with dividers/bullets |
| `data/npcquote.u8.txt` | NPC quote data |
| `data/pg.u8.asc` | Page display art |
| `data/pxtit.u8.asc` | Title screen art |
| `data/spells.u8.txt` | Spell data with decorative chars |
| `data/clans.u8.txt` | NPC definitions with bullet chars |

Three installer `.ini` files also contain UTF-8 but use a first-line
`# encoding: utf-8` marker instead of the `.u8.` naming convention.
install.c detects the marker and converts UTF-8→CP437 via `u8_fgets()`:

| File | Content |
|------|---------|
| `installer/install.ini` | Game installer, ANSI art line |
| `installerdk/clandev.ini` | Devkit installer, ANSI art banner |
| `devkit/example.ini` | Example config, box-drawing dividers |

The following files have **no UTF-8 equivalent** — always use Python binary mode:

| File | Content |
|------|---------|
| `release/clanad.ans` | ANSI advertisement (treat as binary) |
| `data/orphans/*.e,*.mon,*.q` | Compiled binary data files |

### Finding CRLF files

```sh
ggrep -rlP '\r$' <dir>   # correct: \r appears at line-end before \n
# NOT: ggrep -rlP '\r\n'  # wrong: grep parses line-by-line; \n is the delimiter
# NOT: file <path>         # unreliable: only reports first line-ending type seen
```

## Platform Notes (FreeBSD)

- GNU grep is available as `/usr/local/bin/ggrep` and supports `-P` (PCRE) and other GNU extensions. The system `/bin/grep` is BSD grep and lacks these features.
- Use `ggrep` when you need `-P`, `--include`, or other GNU-specific flags.
- When searching for raw non-ASCII bytes (e.g., `[\x80-\xFF]`), prefix the entire command with `LANG=C` to prevent locale/UTF-8 interpretation from masking matches: `LANG=C find ... -exec ggrep -lP '[\x80-\xFF]' {} \;`. Do **not** use `LANG=C` for ordinary text searches — it is only needed when the pattern itself matches raw byte values.

## Task Tracking

When the user asks to add an item to the todo list, append it to `docs/todo.txt`
using the documentation format (plain ASCII, ≤79 columns, section `[X.Y]` style).
Do **not** use the TaskCreate tool for project todos — that storage is session-only
and is lost when the conversation ends.

## Bug Handling During Tasks

When a potential bug or unexpected behaviour is discovered while working on a
task, pause and ask the user which of the following options to take:

- **Ignore** — note it mentally and continue immediately without any action.
- **ToDo** — append an item to `docs/todo.txt` (documentation format, ≤79
  columns, `[X.Y]` style) and then continue.
- **Chat** — discuss the bug; only resume the original task when the user
  explicitly asks to continue.

In all cases the original task will resume at some point.

## Git Workflow

**NEVER run `git commit`, `git commit --amend`, or `git push` without the
user explicitly requesting it.**  Prepare changes, show a summary if helpful,
and wait for the user to say "commit", "amend", "push", or equivalent before
running any of these commands.  **Permission does NOT carry over between
tasks** — each commit, amend, and push requires its own explicit authorization
in the current context.

## Code Style

All C source files use **tabs** for indentation. When constructing `old_string` for the Edit tool, always use literal tab characters, not spaces. If an edit fails to match, verify whitespace with `sed -n 'Np' file | cat -t` — on FreeBSD, `cat -t` shows tabs as `^I` and also reveals other non-printing characters (it implies `-v`). Use `cat -v` alone if you only need to inspect non-printing characters without marking tabs.

## Known Issues / Technical Debt

- **XOR encryption** is trivially weak — the author explicitly acknowledges this.
- **IBBS packet system** protocol is fully documented in `docs/ibbs-notes.txt`.
- **`video.c`** is highly hardware-specific (legacy BBS serial port / terminal control).
- Game logic has no automated tests; only the devkit tools are covered by integration tests.

### Dead / unfinished features

Several features have infrastructure in the code (defines, struct fields, filtering
logic) but are never activated at runtime.  **Do not document these as working
features** — always verify gameplay claims against the source.  See `docs/todo.txt`
for full details.

- **Village types** (`V_WOODLAND`, `V_COASTAL`, `V_WASTELAND`): defined in `defines.h`; items can be tagged with a `VillageType` and `items.c` filters on it — but `Village.Data.VillageType` is never set, so it stays 0 (`V_ALL`) and type-restricted items never appear.  (todo.txt [1.7])
- **Protection period**: `DaysOfProtection` only guards against **empire attacks** (`empire.c`); `Fight_Clan()` in `fight.c` has no protection check, so clan-to-clan combat is always allowed.

## Dependencies

- **OpenDoors library** (`libODoors.a`) — required for BBS door functionality, linked statically.
- Unix builds require POSIX (`-D_POSIX_C_SOURCE=200809L`).

## Documentation Audiences

Four audiences, each building on the previous:

| Audience | Role | Main doc | Location |
|----------|------|----------|----------|
| Player | Play the game on a BBS | `release/player.txt` | `release/` |
| Sysop | Install binary, run on BBS, join IBBS leagues, install quest packs | `release/clans.txt` | `release/` |
| PAK developer | Use devkit to create/customize items, monsters, spells, events, quest packs for distribution | `devkit/clandev.txt` | `devkit/` |
| Clans developer | Modify C source code for the game itself | `docs/notes.txt` | `docs/` |

Each audience is expected to be familiar with the documentation for all prior audiences (sysops know the player docs; PAK developers know both; Clans developers know all three).

Markdown files (`.md`) are **not documentation** — they are LLM interaction files (prompts, project instructions, etc.). The sole exception is `README.md`, which is documentation.

## Documentation Format

All documentation files use the same format regardless of audience.
Plain ASCII text files, ≤79 columns, no markdown, no UTF-8 characters.
Use `--` (double hyphen) instead of em-dash.  Canonical examples:
- `release/clans.txt` — sysop documentation
- `devkit/clandev.txt` — PAK developer documentation
- `docs/notes.txt` — Clans developer documentation

### Structure

```
Title vX.XX [by Author]
-------------------------------------------------------------------------------
[Copyright / URL]

[blank lines]

[0.0]  Table of Contents
-------------------------------------------------------------------------------

Chapter Name
============
Entry description .................................................   X.Y
Another entry .....................................................   X.Y

Chapter Name
============
Entry description .................................................   X.Y


Chapter Name
===============================================================================

[X.Y]  Section Title
-------------------------------------------------------------------------------
Body text wraps at 79 columns.  Two spaces after a period.

        Indented code or examples use 8 spaces.

Example N: Subsection Title
---------------------------
Subsection body.
```

### Rules

- **Separators**: `===============================================================================` (79 `=`) between major chapters; `-------------------------------------------------------------------------------` (79 `-`) under section headers and at file top.
- **Section IDs**: `[X.Y]  Title` where X is chapter, Y is section; `.0` is always the TOC.
- **TOC entries**: all lines the same length (77 chars in `clans.txt` and `player.txt`); description padded with `.`, then spaces + section number right-aligned at the final column.  Single-digit IDs (`1.1`) get 3 leading spaces; double-digit IDs (`10.1`) or suffixed IDs (`1.1b`) get 2 leading spaces — the trailing character always lands at the same column.
- **TOC groups**: group name followed by `====` underline (equal to group name width).
- **Indentation**: 4 or 8 spaces for examples/code blocks; no tabs in documentation files.
- **File excerpts**: delimited with `--[beginning of file]--` / `--[end of file]--------`.
- **Term lists**: term left-padded to column ~20, description follows.
