# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working on SyncTERM.

## Project Overview

**SyncTERM** is a BBS terminal emulator supporting Telnet, RLogin, SSH, TelnetS,
Raw, Modem/Serial, and local shell connections. It implements ANSI, RIPscrip,
SkyPix, PETSCII, ATASCII, Prestel, and Viewdata terminal emulations.

## Build

The primary build system is GNU Make (`gmake`) run from this directory.
A simple `gmake` builds everything. Do NOT use `gmake -jN` — parallel
gmake builds fail unless cryptlib has already been built, and there is
no way to test for that in advance.
See the `COMPILING` file for full details and cross-compilation options.

CMake is an alternative that supports parallel builds and is faster.
Use the CMakeLists.txt from the `syncterm/` directory — the ones higher
up will build too much. Typical CMake usage:
```sh
mkdir build && cd build
cmake ..
cmake --build . -j8
```

## Architecture

See `HACKING.md` for a comprehensive architecture guide covering SyncTERM
and all its libraries: source tree layout, rendering pipeline, connection
providers, terminal emulation, display backends, threading model, and
compile-time options.

## Testing

Automated tests require SDL (for headless offscreen rendering).

**GNU Make:**
```sh
gmake test          # build and run all tests
gmake termtest      # build just the test binary
```

**CMake:**
```sh
cd build && cmake .. && cmake --build . -j8
ctest -V            # verbose output
```

The test harness (`termtest.c`) runs as a child process of SyncTERM via
`shell:` URL over PTY. It sends escape sequences and verifies responses.
`run_termtest.sh` handles headless SDL setup and result checking.

Tests use DSR (cursor position), DECRQCRA (checksums), DECRQM (mode state),
DECRQSS (settings), OSC 4 (palette), and STS (screen content readback)
for verification.

## Code Style

- Tabs for indentation
- Each statement on its own line (no single-line `if (x) break;`)
- Avoid braces solely for variable scoping — hoist variables or extract functions
- When fixing bugs, discuss each individual bug with the user before making changes
- RIPv3 implementation is idealized, NOT bug-compatible with v1.54
