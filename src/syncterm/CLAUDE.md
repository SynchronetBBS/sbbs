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

Three test suites exist:

- **cterm_test** (conio/cterm_test.c) — 275 unit tests for all emulation modes
  (ANSI-BBS, VT52, ATASCII, PETSCII, Prestel, BEEB).  Uses SDL offscreen with
  direct cterm_init/cterm_write/vmem_gettext/getpixels.  Includes response
  capture via callback, retbuf leak detection, and packet-split edge cases.
  Sets `SDL_VIDEO_EGL_DRIVER=none` internally to avoid NVIDIA EGL crashes.
  CMake: `cmake --build . --target cterm_test` then `build/ciolib/cterm_test`
  gmake: `cd conio && gmake cterm_test` then `./cterm_test`

- **termtest** (syncterm/termtest.c) — 67 ANSI-BBS integration tests using
  headless SDL + PTY.  Tests that require the full SyncTERM process: device
  queries, STS readback, SyncTERM extensions, doorway mode.
  Run via: `bash run_termtest.sh build/syncterm build/termtest`

- **termtest.js** (xtrn/termtest/termtest.js) — 148 BBS-side interactive tests.
  Runs as a Synchronet external program for visual verification on real
  terminal connections.  Includes ANSI fuzz testing mode.

## Code Style

- Tabs for indentation
- Each statement on its own line (no single-line `if (x) break;`)
- Avoid braces solely for variable scoping — hoist variables or extract functions
- When fixing bugs, discuss each individual bug with the user before making changes
- RIPv3 implementation is idealized, NOT bug-compatible with v1.54
