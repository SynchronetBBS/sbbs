# HACKING.md — SyncTERM Architecture Guide

This document describes the architecture of SyncTERM and all the libraries
it uses. It is intended for developers who want to understand, modify, or
extend the codebase.

## Table of Contents

1. [Overview](#overview)
2. [Building](#building)
3. [Source Tree Layout](#source-tree-layout)
4. [Application Layer (syncterm/)](#application-layer)
5. [Connection Layer](#connection-layer)
6. [Terminal Emulation (cterm)](#terminal-emulation)
7. [Console I/O Library (ciolib)](#console-io-library)
8. [Bitmap Rendering Pipeline](#bitmap-rendering-pipeline)
9. [Scaling Engine](#scaling-engine)
10. [Display Backends](#display-backends)
11. [Text-Only Backends](#text-only-backends)
12. [RIPscrip Graphics (ripper)](#ripscrip-graphics)
13. [Operation Overkill II (ooii)](#operation-overkill-ii)
14. [File Transfer Protocols](#file-transfer-protocols)
15. [UI Framework (uifc)](#ui-framework)
16. [Cross-Platform Library (xpdev)](#cross-platform-library)
17. [Character Set Translation](#character-set-translation)
18. [Utility Libraries](#utility-libraries)
19. [Threading Model](#threading-model)
20. [Coding Conventions](#coding-conventions)

---

## Overview

SyncTERM is a BBS terminal emulator supporting:

- **Protocols**: Telnet, RLogin, SSH, TelnetS (TLS), Raw, Modem/Serial,
  local shell (Unix PTY, Windows ConPTY)
- **Emulations**: ANSI-BBS, RIPscrip (v1-v3), SkyPix, PETSCII (C64/C128),
  ATASCII (Atari 8-bit), Prestel/Viewdata, BBC Micro, Atari ST VT52
- **Graphics**: Sixel, RIPscrip vector graphics, pixel graphics via APC
  escape sequences (PPM/PBM/JXL)
- **Audio**: ANSI music (PLAY command), Operation Overkill II sound effects
- **File transfer**: ZMODEM, X/YMODEM, CET telesoftware (Prestel)

The codebase is roughly 53 source files in `syncterm/` plus shared libraries
totaling ~1.9M lines (most of which is embedded data: fonts, sound samples,
ANSI art, stroke font vectors).

## Building

### GNU Make (primary)

```sh
cd src/syncterm
gmake
```

A simple `gmake` from this directory builds everything including cryptlib.
**Do NOT use `gmake -jN`** — parallel gmake builds fail unless cryptlib has
already been built, and there is no way to test for that in advance.

See the `COMPILING` file for cross-compilation options (MinGW, Emscripten),
release build flags, and packaging.

### CMake (alternative)

CMake supports parallel builds and is faster for iterative development.
Use the CMakeLists.txt from the `syncterm/` directory — the ones higher
up will build too much.

```sh
mkdir build && cd build
cmake ..
cmake --build . -j8
```

### Compile-Time Options

Both gmake and CMake support the same set of feature toggles:

| Option | Default | Effect |
|--------|---------|--------|
| `WITHOUT_OOII` | OFF | Disable Operation Overkill II terminal mode. OOII is a client-side renderer for a single BBS door game. Its embedded data (41 PCM audio samples, 63 ANSI art screens) adds ~11MB of source and significantly increases binary size. Set this to reduce build times and binary size if OOII support is not needed. |
| `WITHOUT_CRYPTLIB` | OFF | Disable cryptlib. Removes SSH and TelnetS (TLS) support. Eliminates the cryptlib build dependency, which is the slowest part of a clean build. |
| `WITHOUT_JPEG_XL` | OFF | Disable JPEG XL image decoding. Removes libjxl dependency. |
| `WITHOUT_RETRO` | ON | Disable RetroArch/libretro backend. When enabled (`WITH_RETRO` for gmake), SyncTERM is built as a shared library instead of an executable. |

#### Display Backend Options (conio/)

| Option | Default | Effect |
|--------|---------|--------|
| `WITHOUT_GDI` | OFF | Disable Win32 GDI backend (Windows only). |
| `WITHOUT_QUARTZ` | OFF | Disable Quartz backend (macOS only). When Quartz is enabled, SDL video is automatically disabled. |
| `WITHOUT_SDL` | OFF | Disable SDL2 video backend. |
| `WITHOUT_WAYLAND` | OFF | Disable Wayland backend (Unix only). |
| `WITHOUT_X11` | OFF | Disable X11/Xlib backend (Unix only). |
| `WITHOUT_XRANDR` | OFF | Disable XRandR support (monitor resolution detection). Requires X11. |
| `WITHOUT_XRENDER` | OFF | Disable XRender support (external scaling). Requires X11. |
| `WITHOUT_XINERAMA` | OFF | Disable Xinerama support (multi-monitor). Requires X11. |

If both `WITHOUT_GDI` and `WITHOUT_SDL` are set on Windows, the
application is built without `WIN32` subsystem (console mode only).

#### Audio Backend Options (xpdev/)

| Option | Default | Effect |
|--------|---------|--------|
| `WITHOUT_SDL_AUDIO` | OFF | Disable SDL2 audio output. |
| `WITHOUT_COREAUDIO` | OFF | Disable CoreAudio output (macOS). |
| `WITHOUT_ALSA` | OFF | Disable ALSA audio (Linux). |
| `WITHOUT_OSS` | OFF | Disable OSS audio (BSD/Linux). |
| `WITHOUT_PORTAUDIO` | OFF | Disable PortAudio output. |
| `WITHOUT_PULSEAUDIO` | OFF | Disable PulseAudio output. |

Audio backends are used for ANSI music playback and OOII sound effects.
Multiple backends can coexist; xpbeep selects one at runtime.

#### Usage

**gmake** (prefix with variable assignment):
```sh
gmake WITHOUT_OOII=1
gmake WITHOUT_CRYPTLIB=1 WITHOUT_OOII=1
```

**CMake** (pass as cache variables):
```sh
cmake -DWITHOUT_OOII=ON -DWITHOUT_CRYPTLIB=ON ..
cmake -DWITHOUT_X11=ON -DWITHOUT_ALSA=ON ..
```

### Source Tree Requirements

The Synchronet source tree structure must be kept intact. SyncTERM depends on
sibling directories:

```
src/
  build/          — build system makefiles
  conio/          — console I/O library (ciolib, cterm, bitmap, backends)
  comio/          — serial port I/O library
  encode/         — base64, hex, uucode, yenc, lzh, utf8
  hash/           — CRC-16, CRC-32, MD5, SHA-1
  sbbs3/telnet.c  — telnet protocol helpers
  sbbs3/telnet.h
  sbbs3/zmodem.c  — ZMODEM file transfer
  sbbs3/zmodem.h
  sbbs3/xmodem.h  — X/YMODEM definitions
  sbbs3/sexyz.h   — SEXYZ mode flags
  sbbs3/saucedefs.h — SAUCE record definitions
  syncterm/       — this directory (the application)
  uifc/           — text-mode UI framework
  xpdev/          — cross-platform portability layer
3rdp/
  dist/           — cryptlib source archive and patches
```

## Source Tree Layout

### syncterm/ — Application

| File | Lines | Purpose |
|------|-------|---------|
| `syncterm.c` | ~2400 | main(), settings, URL parsing, webget, init |
| `bbslist.c` | ~5200 | BBS directory: read/write/edit, INI persistence |
| `term.c` | ~5500 | Terminal loop (doterm), file transfers, scrollback |
| `conn.c` | ~800 | Connection abstraction: circular buffers, socket connect |
| `conn_telnet.c` | ~400 | Telnet: IAC expand, rx/tx parse callbacks |
| `telnet_io.c` | ~500 | Telnet protocol: option negotiation, subneg, NAWS |
| `rlogin.c` | ~400 | RLogin, Raw, GHost protocol: I/O threads, handshake |
| `ssh.c` | ~1100 | SSH via cryptlib: session, channels, SFTP key upload |
| `telnets.c` | ~300 | TLS-wrapped telnet via cryptlib |
| `conn_pty.c` | ~500 | Unix PTY shell (forkpty, termios) |
| `conn_conpty.c` | ~300 | Windows ConPTY shell (UTF-8 transcoding) |
| `modem.c` | ~400 | Serial/modem via comio library |
| `ripper.c` | ~16700 | RIPscrip graphics interpreter (~9300 lines of code, rest is embedded data) |
| `ooii.c` | ~2200 | Operation Overkill II terminal mode |
| `ooii_sounds.c` | ~226000 | OOII embedded PCM audio samples (41 sounds, ~11MB) |
| `ooii_logons.c` | ~1840 | OOII embedded ANSI art logon screens (3 modes × 10) |
| `ooii_bmenus.c` | ~620 | OOII embedded ANSI art base menus (3 modes × 6) |
| `ooii_cmenus.c` | ~510 | OOII embedded ANSI art complex menus (3 modes × 5) |
| `menu.c` | ~300 | Online menu (Alt-Z), scrollback viewer |
| `window.c` | ~200 | Terminal window sizing, background drawing |
| `uifcinit.c` | ~200 | UIFC library init/bail with palette/font management |
| `fonts.c` | ~300 | Custom font file management (INI persistence) |
| `webget.c` | ~400 | HTTP/HTTPS download for BBS lists/cache |
| `libjxl.c` | ~200 | JPEG XL decoder (dynamic loading) |

### conio/ — Console I/O Library

| File | Lines | Purpose |
|------|-------|---------|
| `ciolib.c/h` | ~2300 | Dispatch layer: Borland-conio-compatible API |
| `cterm.c/h` | ~6800 | Terminal emulator core (ANSI, PETSCII, ATASCII, Prestel, etc.) |
| `bitmap_con.c/h` | ~2850 | Text-to-pixel rendering, double-buffering, blinker thread |
| `scale.c/h` | ~960 | Internal scaling engine (pointy, xBR, interpolation) |
| `cg_cio.m/h` | ~330 | Quartz backend: ciolib interface, key pipe, app thread |
| `cg_events.m` | ~1260 | Quartz backend: AppKit event loop, rendering, input, menus |
| `wl_cio.c/h`, `wl_events.c/h` | ~2200 | Wayland backend (event thread, rendering, input) |
| `wl_dynload.c/h` | ~200 | Wayland client library dynamic loading |
| `wl_proto.c/h` | ~9000 | Pre-generated Wayland protocol headers and code |
| `x_events.c/h`, `x_cio.c/h` | ~2000 | X11/Xlib backend |
| `win32gdi.c/h` | ~1500 | Win32 GDI backend |
| `sdl_con.c/h` | ~2000 | SDL2 backend |
| `retro.c/h` | ~500 | RetroArch/libretro backend |
| `win32cio.c/h` | ~800 | Windows Console API (text-only) |
| `curs_cio.c/h` | ~600 | ncurses (text-only) |
| `ansi_cio.c/h` | ~700 | ANSI escape output (text-only fallback) |
| `utf8_codepages.c/h` | ~2350 | 28 codepage ↔ UTF-8 translation tables |
| `vidmodes.c/h` | ~300 | Video mode definitions and parameters |
| `mouse.c/h` | ~200 | Mouse event queue |
| `sdlfuncs.c/h` | ~300 | SDL2 dynamic function loading |
| `xbr.c/h` | ~500 | xBR anti-aliasing pixel filter |
| `allfonts.c` | large | Embedded bitmap font data |
| `petscii.c` | ~300 | PETSCII-specific helper functions |

### xpdev/ — Cross-Platform Portability Library

| File | Lines | Purpose |
|------|-------|---------|
| `ini_file.c/h` | ~4360 | INI file parser: sections, typed values, !include, fast-parse |
| `xpprintf.c/h` | ~1890 | Custom printf with positional parameters, xp_asprintf |
| `dirwrap.c/h` | ~1550 | opendir/readdir on Windows, glob, path manipulation, fexistcase |
| `xpbeep.c/h` | ~1340 | Tone generation: ALSA, OSS, SDL, PortAudio, PulseAudio, Win32 |
| `genwrap.c/h` | ~1550 | SLEEP/YIELD, xp_random, xp_timer, c_escape_str, parse_byte_count |
| `link_list.c/h` | ~1190 | Doubly-linked list with mutex/semaphore protection, ref counting |
| `str_list.c/h` | ~1130 | NULL-terminated `char**` dynamic arrays, stack macros |
| `unicode.c/h` | ~840 | CP437↔Unicode table, zero-width detection, display width |
| `unicode_defs.h` | ~830 | 1000+ Unicode codepoint constants |
| `sockwrap.c/h` | ~1060 | BSD socket / WinSock unification, union xp_sockaddr |
| `multisock.c/h` | ~700 | Multi-socket binding with retry, IPv4/IPv6/Unix domain |
| `gen_defs.h` | ~570 | Foundation types, SAFECOPY/SAFEPRINTF macros, FREE_AND_NULL |
| `filewrap.c/h` | ~620 | File locking (fcntl/LockFile), sopen, 64-bit offsets |
| `threadwrap.c/h` | ~660 | _beginthread on Unix, mutexes, protected_int (C11 atomics) |
| `xpdatetime.c/h` | ~610 | Microsecond-precision date/time, ISO-8601 formatting |
| `netwrap.c/h` | ~370 | DNS utilities, hostname validation, IP address parsing |
| `dat_file.c/h` | ~380 | CSV and tab-delimited file parsing |
| `xpsem.c/h` | ~410 | POSIX semaphore via pthread_mutex+pthread_cond (fallback) |
| `rwlockwrap.c/h` | ~340 | Read-write locks (custom Windows impl with reentrant reads) |
| `xpevent.c/h` | ~280 | Win32 CreateEvent/SetEvent emulation on Unix |
| `msg_queue.c/h` | ~310 | Bidirectional FIFO message queue on link_list |
| `stbuf.c/h` | ~400 | Dynamic string buffer with NUL termination guarantee |
| `semwrap.c/h` | ~200 | POSIX sem_t wrapper (Win32 semaphore underneath) |
| `datewrap.c/h` | ~320 | Thread-safe gmtime_r/localtime_r, DOS date structs |
| `xpmap.c/h` | ~230 | Memory-mapped file I/O (mmap / CreateFileMapping) |
| `conwrap.c/h` | ~190 | kbhit/getch on Unix via termios |
| `sdlfuncs.c/h` | ~160 | SDL2 function pointer table for dynamic loading |
| `named_str_list.c/h` | ~100 | Name-value pair array with case-insensitive search |
| `xp_dl.c/h` | ~120 | Dynamic library loading with .so.N version fallback |
| `cp437defs.h` | ~160 | CP437 character mnemonics |
| `semfile.c/h` | ~180 | File-based semaphore (lock file create/check/remove) |
| `strwrap.c/h` | ~160 | Additional string utilities |
| `xp_syslog.c/h` | ~230 | Syslog wrapper with remote UDP support |
| `wrapdll.h` | ~35 | DLLEXPORT/DLLCALL macros |
| `xpendian.h` | ~80 | Byte-order macros (BE_INT32, BE_INT64) |
| `petdefs.h` | ~90 | PETSCII character constant definitions |
| `haproxy.h` | ~26 | HAProxy PROXY protocol header struct |

### uifc/ — Text-Mode UI Framework

| File | Lines | Purpose |
|------|-------|---------|
| `uifc32.c` | ~3230 | Main implementation: menus, input, popups, help viewer |
| `filepick.c/h` | ~750 | Split-pane file/directory browser |
| `uifc.h` | ~550 | API definition: uifcapi_t struct, mode flags, constants |
| `uifcx.c` | ~530 | Stdio-based fallback implementation (not used by SyncTERM) |
| `uifc_ini.c` | ~48 | INI configuration reader for UI settings |

### comio/ — Serial Port I/O Library

| File | Lines | Purpose |
|------|-------|---------|
| `comio_nix.c` | ~470 | Unix implementation: termios, baud via cfsetspeed, DTR/RTS ioctls |
| `comio_win32.c` | ~250 | Windows implementation: CreateFile `\\.\COMn`, DCB, COMMTIMEOUTS |
| `comio.c` | ~56 | Shared: comReadBuf (polling loop), comReadLine |
| `comio.h` | ~130 | API: COM_HANDLE type, 19 function prototypes, modem status flags |

### encode/ — Encoding and Compression

| File | Lines | Purpose |
|------|-------|---------|
| `lzh.c/h` | ~18400 | LZH compression: LZSS + adaptive Huffman (message base compression) |
| `utf8.c/h` | ~580 | UTF-8 encode/decode, normalization, CP437/Latin-1 conversion |
| `uucode.c/h` | ~270 | Traditional Unix-to-Unix encoding |
| `yenc.c/h` | ~220 | yEnc binary encoding (Usenet) |
| `base64.c/h` | ~220 | Standard base64 encode/decode |
| `hex.c/h` | ~100 | URL-style hex encoding/decoding |

### hash/ — CRC and Hash Functions

| File | Lines | Purpose |
|------|-------|---------|
| `sha1.c/h` | ~370 | SHA-1 (Steve Reid public domain implementation) |
| `md5.c/h` | ~380 | MD5 (RSA reference implementation) |
| `crc32.c/h` | ~130 | IEEE 802.3 CRC-32 with lookup table |
| `crc16.c/h` | ~120 | CCITT CRC-16 with lookup table |

### sftp/ — SFTP Protocol v3

| File | Lines | Purpose |
|------|-------|---------|
| `sftp_server.c` | ~630 | Server-side dispatch loop with operation callbacks |
| `sftp_pkt.c` | ~490 | Packet serialization: streaming RX reassembly, builder TX |
| `sftp_client.c` | ~420 | Client-side state machine: synchronous request/response |
| `sftp_attr.c` | ~360 | File attributes: size, uid/gid, permissions, times |
| `sftp.h` | ~290 | Public API, types, packet type enums |
| `sftp_static.h` | ~80 | Shared client/server helpers (included twice via macro) |
| `sftp_str.c` | ~70 | Length-prefixed string allocation (sftp_str_t) |

### sbbs3/ — Protocol Files (used by SyncTERM)

| File | Lines | Purpose |
|------|-------|---------|
| `zmodem.c/h` | ~2790 | ZMODEM file transfer: ZDLE encoding, CRC-16/32, batch, crash recovery |
| `xmodem.c/h` | ~730 | X/YMODEM: 128/1024-byte blocks, checksum/CRC-16, G-mode |
| `telnet.c/h` | ~340 | Telnet constants, IAC doubling, option negotiation helpers |
| `saucedefs.h` | ~110 | SAUCE record format (ANSI art metadata) |
| `sexyz.h` | ~45 | SEXYZ mode flags and error codes |

## Application Layer

### Startup (syncterm.c)

`main()` parses command-line arguments and URLs, loads settings from
`syncterm.ini` via the xpdev INI library, initializes the ciolib display
backend, and enters the BBS list UI.

#### URL Parsing

`parse_url()` handles `protocol://[user[:pass]@]host[:port][/path]` URLs,
mapping protocol schemes to connection types. Falls back to INI lookup for
bare hostnames.

#### Settings

The `syncterm_settings` struct holds global configuration: output mode,
scaling, scrollback lines, modem config, TERM string, transfer paths,
UI colors, audio modes, cursor type, etc. Persisted via INI file.

### BBS Directory (bbslist.c)

The BBS list is the main navigation interface. Each entry is a `struct
bbslist` (~168 bytes) containing: name, address, port, credentials,
connection type, screen mode, font, colors, SSH fingerprint, serial
settings, RIP version, palette, and more.

Two list types: `USER_BBSLIST` (personal, read-write) and
`SYSTEM_BBSLIST` (shared, read-only). Both stored in INI format.

`show_bbslist()` provides the main directory with sort, search,
add/edit/delete. `edit_list()` is a ~1000-line property editor with
sub-menus for all fields. Custom sort order via `sortorder[]` array.

### Terminal Loop (term.c)

`doterm()` (~1400 lines) is the central event loop:

```
conn_recv_upto()  →  optional BPS rate limiting  →  cterm_write()  →  screen
keyboard getch()  →  scancode translation  →  conn_send()
```

It also handles:
- ANSI music playback coordination
- RIP graphics mode switching
- OOII mode dispatching
- Mouse event forwarding
- Sixel/pixmap rendering via APC escape sequences
- File transfer initiation and progress display

#### File Transfers

- **ZMODEM**: `zmodem_upload/download()` with progress window
- **X/YMODEM**: `xmodem_upload/download()` with progress window
- **CET telesoftware**: `cet_telesoftware_download()` — Prestel frame-based
  file transfer with frame counting (999 = unknown count)

Transfer UI uses a dedicated window with progress bars, logging, and error
display.

### Online Menu (menu.c)

Alt-Z opens the online menu providing access to: scrollback viewer,
file transfer initiation, terminal settings, font selection, capture
toggle, disconnect, and other runtime options.

## Connection Layer

### Abstraction (conn.c, conn.h)

All connection types share a common `conn_api` struct with function
pointers for `connect()`, `close()`, and `binary_mode_on/off()`, plus
callbacks `rx_parse_cb()` and `tx_parse_cb()` for protocol-layer
processing (e.g., telnet IAC expansion).

Atomic state flags track thread lifecycle:
- `input_thread_running`, `output_thread_running`: 0=not started,
  1=running, 2=finished
- `terminate`: bool, signals threads to exit

#### Circular Buffers

`conn_inbuf` and `conn_outbuf` are mutex-protected ring buffers with
semaphores for blocking waits. API: `conn_buf_put/get/peek/bytes/free`.
`conn_buf_wait_cond()` provides timed waits using `sem_trywait_block`.

#### Connection Flow

```
conn_connect()
  → sets up dispatch based on conn_type
  → calls provider's connect()
  → provider creates buffers, starts I/O threads
  → main thread polls input/output_thread_running
```

All providers follow the same pattern: allocate `rd_buf`/`wr_buf`
(BUFFER_SIZE = 16384), create `conn_inbuf`/`conn_outbuf`, start
input + output threads.

#### Socket Connect

`conn_socket_connect()` handles DNS resolution via `getaddrinfo`,
non-blocking connect with 1-second `socket_writable` polls,
cancel-on-keypress support, and SO_KEEPALIVE.

### Telnet (conn_telnet.c, telnet_io.c)

**TX side** (`st_telnet_expand()`): IAC doubling and CR→CRLF expansion
per RFC 5198. Only expands when not in binary mode.

**RX side** (`telnet_interpret()`): Stateful parser handling IAC escaping,
CR handling in non-binary mode, option negotiation (DO/DONT/WILL/WONT)
with local/remote tracking, and subnegotiation (TERM_TYPE sends
`term_name`, NAWS sends window size).

Binary mode is auto-set when both local and remote BINARY_TX options
are active.

### SSH (ssh.c)

Uses cryptlib for SSH sessions. Two I/O threads:

- `ssh_input_thread`: Multiplexes SSH channel + SFTP channel data.
  Reads from cryptlib session, dispatches to `conn_inbuf` or SFTP
  state machine.
- `ssh_output_thread`: Reads `conn_outbuf`, pushes through cryptlib.
  Protected by `ssh_mutex` for all cryptlib calls.

**SFTP support**: Separate channel for public key upload. Background
thread opens SFTP channel, uploads key file via `sftpc_*` API.

**Fingerprint verification**: Checks stored fingerprint, prompts user
on mismatch or first connection.

### TelnetS (telnets.c)

TLS-wrapped telnet via cryptlib. Same I/O thread pattern as plain
telnet, with cryptlib session wrapping the socket.

### RLogin / Raw / GHost (rlogin.c)

RLogin sends the standard `\0username\0username\0termtype/speed\0`
handshake. GHost protocol adds a multi-step handshake with hostname
exchange. Raw mode skips all handshake.

### Modem/Serial (modem.c)

Uses the comio library for serial port access. The `com` handle is
`_Atomic(COM_HANDLE)` for thread-safe access between the I/O threads
and the DCD polling loop.

### Unix PTY (conn_pty.c)

Launches a local shell via `forkpty()`. Configures termios settings
(raw mode, no echo, control characters). Sets TERM, COLUMNS, LINES
environment variables.

### Windows ConPTY (conn_conpty.c)

Creates a Windows pseudo-console for local shell access. Includes
UTF-8 span boundary checking for multi-byte sequence handling across
read boundaries.

## Terminal Emulation

### cterm (conio/cterm.c, ~6800 lines)

The terminal emulator core processes incoming byte streams and translates
them into ciolib screen operations. It supports multiple emulations
switchable at runtime.

#### Coordinate Systems (5 layers, all 1-based)

```
pixel    → ciolib pixel coordinates (sixel graphics)
screen   → ciolib screen (puttext/gettext coordinates)
absterm  → full terminal area (origin mode OFF)
term     → scrolling region only (origin mode ON)
curr     → absterm or term depending on origin mode state
```

`coord_conv_xy()` converts between layers. Convenience macros
(`SCR_XY`, `ABS_XY`, `TERM_XY`, `CURR_XY`, etc.) simplify usage.

#### Main Entry Point: cterm_write()

```c
size_t cterm_write(struct cterminal *cterm, const void *buf, int buflen,
                   char *retbuf, size_t retsize, int *speed);
```

Processes `buflen` bytes, generates response data in `retbuf`. The main
byte loop has priority-ordered dispatch:

1. **String capture**: Accumulating DCS/APC/OSC/PM/SOS string data
2. **Font download**: Receiving raw font bitmap data
3. **Escape sequence**: Parsing and executing escape sequences
4. **Music capture**: Accumulating ANSI music strings
5. **Normal character**: Per-emulation character handling and output

Printable characters are batched into a buffer and flushed via
`uctputs()` → `ctputs()` which handles UTF-8 to codepage conversion.

#### CSI Dispatch: do_ansi() (~2200 lines)

The heart of ANSI processing. `parse_sequence()` breaks escape sequences
into `esc_seq` structs (c1_byte, parameters, intermediates, final_byte),
then a large switch dispatches on the final byte.

Major CSI sequences: cursor movement (A-H), editing (J/K/L/M/P/X/S/T),
SGR attributes (m) with 256-color and truecolor support, DEC private
modes (?h/?l), SyncTERM extensions (=h/=l), device queries (c/n/t/S).

#### Emulation-Specific Handling

- **ATASCII**: Control codes for cursor movement (28-31), screen clear
  (125), tabs. Screen code translation between ATASCII and display.
  Attribute byte used as mode flag (7=normal, 1=inverse).
- **PETSCII**: C64/C128 color codes, reverse mode, screen code
  translation. Different palettes for C64 40-col and C128 80-col modes.
- **Prestel/Viewdata**: Control characters with split before/after
  semantics for attribute application. Mosaic graphics (2x3 grid per
  cell), hold mode, double-height, conceal/reveal, programming mode.
- **BBC Micro (BEEB)**: VDU command parsing with fixed-length
  multi-byte sequences.
- **Atari ST VT52**: VT52-style escape sequences.

#### Sixel Graphics

Parsed within DCS strings. Color registers defined with HLS or RGB.
Repeat counts, carriage return, and newline within sixel bands. Renders
directly to pixel buffer via `setpixels()`.

#### ANSI Music

Captured between `ESC[M` and SO (0x0E). Supports notes (A-G with
sharps/flats), octave (0-6), tempo (32-255 BPM), note length,
normal/legato/staccato, foreground/background playback. Notes queued
to a linked list and played by a background thread.

#### Scrollback

Circular buffer of `vmem_cell` arrays. Lines added during scroll-up
operations. Viewer accessible from the online menu.

#### LCF (Last Column Flag)

Three-bit flag system matching DEC VT behavior: cursor "sticks" at the
right margin and the next character wraps to the next line, avoiding
the off-by-one ambiguity at the screen edge.

## Console I/O Library

### ciolib (conio/ciolib.c, ~2300 lines)

Provides a Borland-conio-compatible API that dispatches to one of several
backends via function pointers in the `cio_api` struct.

```
Application code (syncterm.c, term.c, etc.)
  |
  v  (ciolib_puttext, ciolib_getch, etc.)
ciolib.c  — dispatch layer
  |
  +--> Bitmap backends (via bitmap_con.c):
  |      Quartz, Wayland, X11, GDI, SDL, RetroArch
  |
  +--> Text-only backends (direct):
         win32cio.c  (Windows Console API)
         curs_cio.c  (ncurses)
         ansi_cio.c  (ANSI escape sequences)
```

The `cio_api` struct contains ~60 function pointers. Each backend
populates the pointers it implements. Functions not implemented by a
backend fall back to generic implementations in ciolib.c.

#### Initialization (AUTO mode fallback chains)

- **macOS**: Quartz → X11 → SDL → Curses → ANSI
- **Unix**: Wayland → X11 → SDL → Curses → ANSI
- **Windows**: GDI → SDL → ANSI → Win32 Console

#### macOS Main Thread Requirement

macOS Cocoa requires all UI operations on the main thread. Both the
Quartz and SDL backends handle this by redefining `main()` via macro:
the real main spawns application logic in a new thread, then the main
thread runs the event loop (NSApplication for Quartz, SDL for SDL).
The Quartz backend uses `cg_launched_sem` to synchronize — window
creation waits for `applicationDidFinishLaunching:` before proceeding.

#### Key Abstractions

- **vmem**: Virtual memory — circular buffer of `vmem_cell` structs
  (character, legacy attribute, 32-bit fg/bg colors, font index,
  hyperlink_id)
- **Hyperlink table**: 4096-entry table in ciolib.c mapping
  `hyperlink_id` → URI. Populated via OSC 8 sequences parsed by
  cterm. GC scans visible screen when table is full. APIs:
  `ciolib_add_hyperlink()`, `ciolib_get_hyperlink_url()`,
  `ciolib_open_hyperlink()`, `ciolib_set_current_hyperlink()`
- **text_info**: Current window state (coordinates, cursor, mode, attribute)
- **Screen save/restore**: Captures full state including vmem, pixels,
  fonts, palette, and video flags
- **Macro layer**: Unless `CIOLIB_NO_MACROS` is defined, standard conio
  names (`puttext`, `getch`, `gotoxy`, etc.) are macros to `ciolib_*`

## Bitmap Rendering Pipeline

### bitmap_con.c (~2850 lines)

Sits between the abstract ciolib API and platform-specific display
backends. Converts text operations into pixel data.

```
ciolib.c  (text operations)
  |
  v
bitmap_con.c  (text→pixel rendering, double-buffering, blink/cursor)
  |
  v
Backend callbacks: drawrect() + flush()
  |
  +--> cg_events.m       (macOS Quartz/AppKit)
  +--> wl_events.c       (Wayland)
  +--> x_events.c        (X11/Xlib)
  +--> win32gdi.c        (Win32 GDI)
  +--> sdl_con.c         (SDL2)
  +--> retro.c           (RetroArch libretro)
```

#### Double-Buffered Pixel Storage

Two `bitmap_screen` instances: `screena` (blink-on) and `screenb`
(blink-off). Each contains pixel data (uint32_t ARGB per pixel) in
a circular buffer with `toprow` offset for efficient scrolling.

#### Rendering Pipeline

1. `set_vmem_cell()` — write to vmem
2. `update_from_vmem()` — diff vs cached state, redraw changed cells
3. `bitmap_draw_vmem_locked()` — render each cell's scanlines
4. `get_full_rectangle_locked()` — linearize circular buffer into rectlist
5. `cb_drawrect()` — overlay cursor, send to backend
6. `cb_flush()` — signal backend to present

Two rendering paths:
- **Fast path** (`draw_char_row_fast`): 8-bit-wide chars, standard rendering
- **Slow path** (`draw_char_row_slow`): 12-wide fonts, VGA line-expand,
  Prestel double-height/separated mosaics

#### Blinker Thread

Started by `bitmap_drv_init()`. Runs forever with 10ms sleep interval.
Handles cursor blink, text blink (with mode-specific timing: Prestel
asymmetric, C64 tied, Atari no-blink), and screen updates. **The blinker
thread is the ONLY thread that calls drawrect/flush**, serializing all
rendering.

#### Locking Hierarchy

Five locks, must be acquired in this order:

1. `vstatlock` (rwlock) — vstat and vmem
2. `screenlock` (mutex) — pixel buffers
3. `vstat_chlock` (mutex) — vmem changed flag only
4. `free_rect_lock` (mutex) — free rects list and throttle state
5. `prestel_hack_lock` (mutex) — Prestel reveal/hide mutation

#### Throttling

`MAX_OUTSTANDING = 2` rects. When backends are slow (especially GDI),
the blinker thread skips frames rather than queuing unboundedly.

#### Font System

4 font slots for alternate character set support. Slot 0 is primary;
slots 1-3 are selected by bright/blink attribute bits. Supports 8x8,
8x14, 8x16, 12x20 sizes. Runtime font replacement for remote font
download.

#### Pixel Graphics

`bitmap_setpixel()` and `bitmap_setpixels()` write directly to pixel
buffers, marking affected vmem cells with `CIOLIB_BG_PIXEL_GRAPHICS`
so the text renderer doesn't overwrite them. Used for sixel and mouse
overlays.

## Scaling Engine

### scale.c (~960 lines)

When `CIOLIB_SCALING_INTERNAL` is active, bitmap_con.c renders at native
resolution and scale.c upscales to window size. The pipeline decomposes
the integer scale factor into a sequence of operations:

1. **multiply_scale** — simple pixel duplication
2. **pointy_scale_odd/3/5** — edge-aware integer upscale (sharp diagonals
   for pixel art and box drawing)
3. **xbr_filter 4x/2x** — xBR anti-aliasing filter
4. **interpolate_height/width** — fractional scaling via YCoCg color
   space blending

When `CONIO_OPT_BLOCKY_SCALING` is set, only multiply_scale +
interpolation are used (no edge detection).

Two `graphics_buffer` structs ping-pong as source and target through
the pipeline.

#### Aspect Ratio

Multiple aspect ratio functions handle different correction strategies
(fix inside box, grow to match, shrink to match). Integer scale factors
are clamped 1..14.

## Display Backends

### Wayland (wl_cio.c + wl_events.c + wl_dynload.c + wl_proto.c)

1. Init: dynamically load libwayland-client via dlopen (wl_dynload.c),
   create pipes for main↔event thread communication, spawn event thread
2. Event thread: `wl_display_connect()`, bind globals (compositor, SHM,
   xdg-shell, seat), create xdg_surface/xdg_toplevel, enter `select()`
   loop on Wayland fd + local_pipe fd
3. drawrect: write to local_pipe; event thread reads it, does scaling
   (internal via `do_scale()` or external via viewporter
   `wp_viewport_set_destination()`), copies pixels to SHM buffer,
   `wl_surface_commit()`
4. Separate mouse thread polls `mouse_wait()`
5. Keyboard: xkbcommon (dlopen'd) for layout-aware key translation;
   fallback to evdev scancode tables (US QWERTY) if unavailable
6. Clipboard: `wl_data_device_manager` protocol for copy/paste with
   self-paste deadlock avoidance
7. Window icon: xdg-toplevel-icon-v1 protocol (staging) with pixel buffer
8. Cursor shape: libwayland-cursor (dlopen'd) for pointer theme support
9. HiDPI: `wl_surface_set_buffer_scale(surf, 1)` prevents compositor
   double-scaling when internal scaling is active
10. Graceful degradation: only compositor, SHM, xdg-shell, seat, and
    keyboard are required; optional protocols (viewporter, xdg-decoration,
    xdg-toplevel-icon, clipboard, cursor theme) degrade gracefully

Protocol code is pre-generated by wayland-scanner and combined into
`wl_proto.h`/`wl_proto.c`. No wayland-scanner build-time dependency.

### X11 (x_cio.c + x_events.c)

1. Init: dynamically load X11/XRender/Xinerama/XRandr, create pipes for
   main↔event thread communication, spawn event thread
2. Event thread: XOpenDisplay, CreateWindow, GC, enter `select()` loop
   on X11 fd + local_pipe fd
3. drawrect: write to local_pipe (non-blocking); event thread reads it,
   does scaling (internal via `do_scale()` or external via XRender),
   dirty-rect detection, XPutImage
4. Separate mouse thread polls mouse_wait()

### GDI (win32gdi.c)

1. Init: DPI awareness setup, register "SyncConsole" window class
2. Message thread: GetMessage loop, signals init when window created
3. drawrect: appends to linked list; flush keeps only the last rect,
   posts WM_USER_INVALIDATE
4. WM_PAINT: CreateDIBitmap, BitBlt (internal) or StretchBlt with
   HALFTONE mode (external scaling)
5. Implicit throttling via next/last double-buffer (at most 2 rects alive)
6. Keyboard via overlapped named pipe

### Quartz (cg_cio.m + cg_events.m)

1. Init: `cg_start_app_thread()` spawns app logic thread, main thread
   enters NSApplication event loop (`cg_run_event_loop()`).
   `applicationDidFinishLaunching:` posts `cg_launched_sem` to unblock
   window creation
2. Window: NSWindow + custom SyncTermView (NSView subclass). Content
   size set in points; `vstat.winwidth/winheight` stored in backing
   pixels for correct bitmap_snap behavior on Retina displays
3. drawrect: appends to linked list; flush drains queue, keeps last.
   External scaling: raw rectlist passed to drawRect, CG stretches
   via `CGContextDrawImage`. Internal scaling: `do_scale()` at backing
   pixel resolution, 1:1 physical pixel mapping on Retina
4. Keyboard: macOS virtual keycodes → AT Set 1 scancodes via lookup
   table. Option = Alt (BBS keycodes). Cmd+Q → CIO_KEY_QUIT,
   Cmd+V → Shift-Insert (macOS conventions)
5. Mouse: Cocoa point coordinates → screen bitmap coordinates →
   1-based cell coordinates
6. Menus: Native NSMenu (SyncTERM, Edit, View) with keyboard shortcuts
7. Clipboard: NSPasteboard for copy/paste
8. Icon: NSBitmapImageRep from ABGR pixel data → NSImage
9. Audio: CoreAudio AudioQueue API (in xpdev/xpbeep.c), no SDL needed
10. Quartz and SDL are mutually exclusive (both need the main thread);
    CMake automatically disables SDL when Quartz is enabled

### SDL (sdl_con.c + sdlfuncs.c)

1. Init: dynamically load SDL2, init semaphores and mutexes
2. macOS: main thread runs event loop (Cocoa requirement); app logic in
   spawned thread
3. User function mechanism: cross-thread RPC via SDL_PeepEvents + semaphores
   (blocking and non-blocking variants)
4. Event loop: SDL_WaitEventTimeout with BIOS Alt+numpad keyboard parsing,
   SDL_TEXTINPUT, window resize/maximize handling
5. flush: dequeue all rects, keep only last, LockTexture, copy pixels,
   RenderCopy, RenderPresent

### RetroArch (retro.c)

SyncTERM compiled as a shared library. RetroArch controls frame timing
via `retro_run()`.

## Text-Only Backends

These operate on character cells directly without the bitmap rendering
layer. No pixel graphics, custom fonts, or scaling.

### Windows Console (win32cio.c)

Uses Windows Console API (ReadConsoleInput, WriteConsoleOutput). Shadow
buffer (`win32cio_buffer`) for dirty tracking. BIOS-style scancode
translation. Supports mouse and clipboard.

### ncurses (curs_cio.c)

Three sub-modes: CURSES (CP437 translation via ACS), CURSES_IBM
(assumes CP437 terminal), CURSES_ASCII (ASCII only). Translates
curses KEY_* to BIOS scancodes. Tracks font number for codepage
detection but can't change display font.

### ANSI (ansi_cio.c)

Outputs ANSI escape sequences to stdout, reads input from stdin.
Fallback when no graphical or curses backend is available. Diffs
output against internal buffer to minimize escape sequences. No
mouse support.

### Backend Capability Comparison

| Feature | win32cio | curs_cio | ansi_cio | Bitmap |
|---------|----------|----------|----------|--------|
| Pixel graphics | No | No | No | Yes |
| Custom fonts | No | Track only | No | Yes |
| Extended palette | No | No | No | Yes |
| Mouse | Yes | Yes | No | Yes |
| Clipboard | Yes | No | No | Yes |
| Window scaling | No | No | No | Yes |
| Suspend/resume | Yes | Yes | Yes | No |

## RIPscrip Graphics

### ripper.c (~16700 lines, ~9300 code)

Interprets RIPscrip graphics commands from remote BBS servers.
**Processes untrusted remote data** — highest attack surface in SyncTERM.

#### Embedded Data (lines 1-7350)

7x8 bitmap font, 10 BGI stroke fonts (Triplex, Small, Sans, Gothic,
Script, Simplex, TriplexScript, Complex, European, Bold), 64-entry
EGA palette, 256-color extended palette, 12 fill patterns, font metrics.

#### Parsing Pipeline

```
parse_rip()              — entry point, per-chunk state machine
  handle_rip_line()      — per-line RIP detection / extraction
    do_rip_string()      — parse !|level,sublevel,cmd,args sequences
      do_rip_command()   — 4250-line command dispatcher (~100 commands)
    do_skypix()          — CSI-based SkyPix extension commands
  unrip_line()           — remove RIP sequences, pass text through
```

State machine tracks parser state across chunks:
BOL → BANG → PIPE → LEVEL → SUBLEVEL → CMD → ENDED

#### Coordinate System

RIP uses logical 640x350 coordinates mapped to physical screen via
lookup tables. Viewport clipping checked per-pixel in draw/set/fill
operations.

#### Drawing

- **Pixels**: `draw_pixel()` (viewport-clipped, XOR mode),
  `set_pixel()` (direct color), `fill_pixel()` (uses fill pattern)
- **Lines**: Bresenham with line pattern, XOR, width 1 or 3
- **Fill**: `broken_flood_fill()` — intentionally bug-compatible with
  RIPterm's known fill bugs; `do_fill()` — even-odd for polygons/beziers
- **Stroke fonts**: Font 0 = bitmap 8x8, fonts 1-10 = BGI stroke fonts
- **Ellipse/arc**: McIlroy midpoint algorithm
- **Bezier curves**: Cubic with configurable step count

#### SkyPix Extensions

CSI-based graphics commands (Amiga-inspired): pixel set, line, rect,
ellipse, copy/paste regions, Amiga font loading, palette, XModem
file transfer.

#### Button System

Graphical buttons with bevel, chisel, sunken, recessed, dropshadow
styles. Highlight/underline hotkeys. Mouse field linked list for
clickable regions.

#### File Operations

ICN icon files (EGA planar format), file transfers (XModem/YModem/
ZModem), file queries (existence, size, date).

#### Variable System

~80 built-in text variables (`$VARNAME$` syntax) for date/time, mouse
state, cursor position, sound, save/restore. Command string processing
handles `^X` control chars, variable expansion, and popup menus.

#### Implementation Note

RIPv3 implementation is idealized, NOT bug-compatible with v1.54.
Three intentional compatibility bugs exist in `broken_flood_fill()`.

## Operation Overkill II

### ooii.c (~2200 lines) + embedded data files

Client-side rendering for the OOII BBS door game. Handles game-specific
terminal codes to render menus, status screens, map scanner, and sound
effects locally instead of streaming raw ANSI.

Activated only when `ooii_mode > 0` after BBS sends the `?` negotiation
code. Three display modes controlling which ANSI art sets to use.

Embedded data (~230K lines total):
- `ooii_sounds.c` — 41 PCM audio samples
- `ooii_logons.c` — 30 ANSI art logon screens (3 modes × 10)
- `ooii_bmenus.c` — 18 base menus (3 modes × 6)
- `ooii_cmenus.c` — 15 complex menus (3 modes × 5)

Static lookup tables for game items: diseases, weapons, ammo, equipment,
suits, armor.

## File Transfer Protocols

### ZMODEM (sbbs3/zmodem.c, ~2440 lines)

Full-featured ZMODEM implementation with:
- ZDLE encoding/escaping for telnet transparency
- Hex and binary (CRC-16/CRC-32) frame formats
- Streaming with adaptive window sizing and block size
- Crash recovery via ZRPOS
- Batch transfer with file metadata (name, size, timestamp)

Callback-driven I/O via `zmodem_t` struct (~50 fields).

### X/YMODEM (sbbs3/xmodem.c, ~650 lines)

- 128-byte and 1024-byte blocks with checksum or CRC-16
- YMODEM batch header (filename, size, timestamp)
- G-mode streaming (no per-block ACK)
- Automatic block size fallback on errors

### Telnet Protocol (sbbs3/telnet.c/h)

Constants and helpers: telnet command/option enums, `telnet_expand()`
for IAC doubling, option negotiation response helpers.

### SAUCE (sbbs3/saucedefs.h)

Standard Architecture for Universal Comment Extensions — metadata
format for ANSI art files (title, author, dimensions, font, iCE color
flag).

## UI Framework

### uifc (uifc/)

Novell SYSCON-inspired text-mode UI framework. Function-pointer-based
API dispatched through `uifcapi_t` struct.

#### Key Components

- `ulist()` (~1270 lines): Menu system with window save/restore stack,
  dynamic updates, keyboard/mouse handling, type-ahead search,
  clipboard, and extensive mode flags (64-bit `uifc_winmode_t`)
- `ugetstr()`: String input with insert/overwrite, clipboard paste,
  character filtering, password mode
- `showbuf()`: Scrollable text viewer with markup (STX/~ toggle inverse)
- `filepick.c`: Split-pane file/directory browser

#### Static State

Only one UIFC instance can exist per process — all state is file-scope
static.

## Cross-Platform Library

### xpdev/ (~25K lines)

Synchronet's cross-platform portability layer. Unix functions are the
"native" API; on Windows, POSIX names are provided as macros/wrappers
over Win32 API.

#### Threading & Synchronization
- `threadwrap.c/h` — `_beginthread()`, mutexes, protected_int (C11
  atomics when available, mutex fallback otherwise)
- `semwrap.c/h` — POSIX sem_t (wraps Win32 semaphores on Windows)
- `xpevent.c/h` — Win32 CreateEvent/WaitForEvent emulation on Unix
- `rwlockwrap.c/h` — Read-write locks (custom implementation on Windows
  with reentrant read locks and writer starvation prevention)

#### File & Directory I/O
- `filewrap.c/h` — File locking, shared-mode open, 64-bit offsets
- `dirwrap.c/h` — opendir/readdir on Windows, glob, path manipulation,
  `fexistcase()` case-insensitive lookup

#### Networking
- `sockwrap.c/h` — BSD socket / WinSock unification, `union
  xp_sockaddr` (IPv4/IPv6/Unix), errno mapping
- `netwrap.c/h` — DNS utilities, hostname validation

#### Configuration
- `ini_file.c/h` (~4000 lines) — Comprehensive INI file parser with
  typed values, two-pass section search, `!include` directives,
  fast-parse mode, encrypted file support

#### Audio
- `xpbeep.c/h` — Cross-platform tone generation: Win32 Beep, ALSA,
  OSS, SDL, PortAudio, PulseAudio. Sine, sawtooth, square waveforms.
  `xp_play_sample()` for raw PCM playback.

#### Other Key Modules
- `genwrap.c/h` — SLEEP/YIELD, xp_random(), xp_timer(), c_escape_str
- `xpprintf.c/h` — Custom printf with positional parameters
- `link_list.c/h` — Doubly-linked list with mutex/semaphore protection
- `str_list.c/h` — Dynamic NULL-terminated string arrays
- `unicode.c/h` — CP437↔Unicode table, zero-width detection
- `xp_dl.c/h` — Dynamic library loading with version fallback
- `sdlfuncs.c/h` — SDL2 function pointer table

## Character Set Translation

### utf8_codepages.c (~2350 lines)

Bidirectional conversion between UTF-8 and 28 legacy character encodings.
Mostly static data tables.

Two families:
- **Standard codepages** (CP437, ISO-8859-*, KOI8-*, CP850, etc.): Bytes
  0-127 are ASCII identity; bytes 128-255 use lookup tables. Unicode→CP
  uses binary search on sorted mapping tables.
- **Full-table codepages** (ATASCII, PETSCII, Prestel): All 256 byte
  values have custom mappings with no ASCII pass-through.

Conversion API: `cp_to_utf8()`, `utf8_to_cp()`, plus single-character
functions. Used by cterm.c for clipboard paste and by the ncurses backend.

## Utility Libraries

### hash/ — CRC and Hash Functions (~530 lines)
- CRC-16 (CCITT), CRC-32 (IEEE 802.3) with lookup tables
- MD5 (RSA reference), SHA-1 (Steve Reid public domain)

### comio/ — Serial Port I/O (~530 lines)
- Cross-platform COM port abstraction (termios on Unix, DCB on Windows)
- 19 functions: open/close, baud rate, flow control, modem status,
  read/write, DTR/RTS control

### encode/ — Encodings (~19K lines)
- base64, hex (URL-style), uucode, yEnc
- LZH compression (LZSS + adaptive Huffman, ~18K lines)
- UTF-8 encode/decode with normalization

### sftp/ — SFTP Protocol v3 (~1700 lines)
- Layered: packet I/O, string handling, file attributes, client, server
- Client used by SyncTERM for SSH public key upload
- Big-endian packet format with grow-on-demand buffers

## Threading Model

SyncTERM uses multiple threads with carefully structured communication:

### Main Thread
- Runs the UI (BBS list, menus) and terminal loop (`doterm()`)
- On macOS with SDL: main thread runs SDL event loop; app logic runs in
  a spawned thread

### Connection I/O Threads (per connection)
- **Input thread**: Reads from network/serial/PTY, writes to `conn_inbuf`
- **Output thread**: Reads from `conn_outbuf`, writes to network/serial/PTY
- Protected by `conn_api.terminate` for shutdown coordination

### Blinker Thread
- Single thread responsible for ALL screen rendering
- Handles cursor blink, text blink, and screen updates
- Only thread that calls `drawrect()`/`flush()` — serializes rendering

### Backend Event Threads
- Wayland: Event thread runs `select()` loop on Wayland fd + pipe
- X11: Event thread runs `select()` loop on X11 fd + pipe
- GDI: Message pump thread runs `GetMessage` loop
- SDL: Event thread runs `SDL_WaitEventTimeout`
- Each has a separate mouse thread

### ANSI Music Thread
- Background playback via `playnote_thread()`
- Reads from note queue (linked list with semaphore)

### Synchronization Primitives
- Mutexes (pthread_mutex / CRITICAL_SECTION)
- Semaphores (sem_t / Win32 semaphore)
- Read-write locks (vstatlock in bitmap_con.c)
- Atomics (C11 `_Atomic` with fallback)

## Coding Conventions

- **Indentation**: Tabs
- **Statements**: Each on its own line (no single-line `if (x) break;`)
- **Braces**: Avoid using solely for variable scoping — hoist variables
  or extract functions instead
- **Platform ifdefs**: `__unix__` for all Unix, `_WIN32` for Windows,
  `__OS2__` for OS/2
- **String safety**: `strlcpy`/`strlcat` preferred over `strcpy`/`strcat`;
  `snprintf` over `sprintf`
- **Thread safety**: File-scope statics acceptable for single-instance
  subsystems (UIFC, scaling); atomic/mutex for shared state
- **Memory**: `FREE_AND_NULL` macro for safe free+null; `SAFECOPY`/
  `SAFEPRINTF` macros for bounded string operations
