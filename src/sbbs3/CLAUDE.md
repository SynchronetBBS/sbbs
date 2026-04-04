# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Synchronet BBS** is a cross-platform bulletin board system written in C/C++. It supports multiple access protocols (Telnet, SSH, FTP, SMTP/POP3, HTTP) and includes a full JavaScript scripting engine (Mozilla JS/SpiderMonkey). The `next-js` branch is focused on JavaScript engine modernization.

The repository root (`C:/next-js/`) contains the full Synchronet tree. This working directory (`src/sbbs3/`) is where the main C/C++ sources live.

## Build

### Windows (MSVC, 32-bit only)

Open `sbbs3.sln` in Visual Studio 2022 and build, or from a Developer Command Prompt:
```bat
build.bat
```

Output goes to `msvc.win32.exe.debug/` (debug) or `msvc.win32.exe.release/` (release). Copy `*.exe` and `*.dll` to your Synchronet `exec` directory.

### UNIX/Linux

```sh
# Build and install in one step
make -f path/to/install/install-sbbs.mk SYMLINK=1

# Build only (from this directory)
make -C path/to/src/sbbs3

# Release build
# Add RELEASE=1 to src/build/localdefs.mk for persistent option

# GTK GUI utilities
make -C path/to/src/sbbs3 gtkutils
```

Output goes to platform-specific subdirectories, e.g. `gcc.linux.x64.exe.release/`. Run `make help` in this directory for all supported targets and options.

### Generated source files

These are auto-generated during build — do not edit manually:
- `git_branch.h`, `git_hash.h`
- `text.h`, `text_defaults.c`

### Ignored build files

CMake and CodeBlocks files in the tree are **not used or supported** — ignore them.

## Architecture

### Core structure

The BBS is structured as a main shared library (`sbbs`) loaded by front-end executables, with additional protocol servers as separate loadable modules:

- **`sbbs` (DLL/shared lib)** — The core BBS engine. The central class is `sbbs_t` (declared in `sbbs.h`). Connection handling is in `main.cpp`. All terminal interaction, message handling, file transfers, and user sessions live here.
- **`ftpsrvr`** — FTP server (`ftpsrvr.cpp`)
- **`mailsrvr`** — SMTP/POP3 mail server (`mailsrvr.cpp`)
- **`websrvr`** — HTTP/HTTPS web server (`websrvr.cpp`)
- **`services`** — Additional service framework (`services.cpp`)

Front-end executables (`sbbscon`, `ntsvcs.c`) load these modules and manage the server lifecycle.

### JavaScript subsystem

`jsexec` (from `jsexec.cpp`) is a standalone JS executor used for door games and scripts. The JS engine is Mozilla SpiderMonkey (libmozjs185), exposed to scripts through a large set of binding modules:

- `js_bbs.cpp` — BBS object (menus, I/O, ARS)
- `js_console.cpp` — Console/terminal I/O
- `js_socket.cpp` — Network sockets
- `js_file.cpp` — File operations
- `js_msgbase.cpp` — Message base (SMB) access
- `js_user.cpp` — User account object
- `js_system.cpp` — System info
- `js_global.cpp` — Global functions and startup

The `next-js` branch is modernizing this from mozjs185 to a newer SpiderMonkey API. Changes to `jsexec.cpp` and the `js_*.cpp` files are the primary focus.

### Key supporting libraries (in `src/`)

- `xpdev/` — Cross-platform abstraction layer; always prefer its macros over standard C functions in sbbs3 code
- `smblib/` — Synchronet Message Base library
- `comio/` — Communication I/O
- `ciolib/` — Console I/O
- `uifc/` — User Interface Framework for C

### Utilities and tools

`scfg/`, `uedit/`, `umonitor/` are built as part of the default UNIX make. Additional standalone tools (`addfiles`, `makeuser`, `sbbsecho`, `sexyz`, etc.) are individual build targets.

## Testing

Unit tests are JavaScript files located in `exec/tests/`, organized into subdirectories by subsystem (`global/`, `system/`, `file/`, `crypt/`, `user/`, `regex/`). The test runner is `exec/tests/test.js`.

Run all tests using `jsexec` (after building):
```sh
jsexec exec/tests/test.js
```

Run a single test file directly:
```sh
jsexec exec/tests/global/format.js
```

Subdirectories may contain a `skipif` script — if it returns true, that directory's tests are skipped (used for platform/feature guards). Tests return an `Error` object on failure or exit normally on success.

## Code Style

- **Tabs** for indentation (4-char tab width)
- **No `ctype.h`** functions (`isprint()`, `isspace()`, `isdigit()`, etc.) — use the `IS_*` macros from `xpdev/gen_defs.h` instead
- **Safe string functions**: use `SAFECOPY()` instead of `strcpy()`, `SAFEPRINTF()` or `snprintf()` instead of `sprintf()`
- **Booleans**: use `bool` (not `BOOL` unless Win32 API required); use `JSBool` for libmozjs API compatibility
- **Avoid `long int`** — size is inconsistent across platforms; use `int` or sized types (`int32_t`, `uint64_t`, etc.)
- **Lowercase filenames** with no spaces or special characters when adding new files
- **JavaScript files**: use `"use strict"` in new files; avoid unnecessary 3rd-party libraries

## Merge Requests

Submit merge requests to [gitlab.synchro.net](https://gitlab.synchro.net) (not GitHub mirrors). Keep MRs to a single topical change. Commit titles should be ≤70 characters.
