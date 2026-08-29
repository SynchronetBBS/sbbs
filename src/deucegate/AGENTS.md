# DeuceGate contributor guide

DeuceGate is a C17, SSH-only replacement for Rick Parrish's GameSrv. It is
intended to run directly against an existing GameSrv root without migrating its
configuration, menus, doors, users, or screen files.

The source directory may have a historical or temporary name. The product,
executable, CMake targets, log messages, documentation, and new identifiers must
always use **DeuceGate**. Do not derive product names from the directory name.

## Repository relationships

This project lives beside Synchronet's shared libraries and intentionally uses
their source trees:

- `../xpdev` supplies INI parsing, sockets, threads, filesystem wrappers, and
  other portability support.
- `../ssh` is DeuceSSH and is the only remote transport implementation.
- `../conio/utf8_codepages.c` supplies the CP437 conversion tables. Do not add a
  dependency on `../encode` for character-set conversion.
- `../encode/base64.c` is used only for Base64 encoding and decoding.
- `../sbbs3/answer.cpp` is the reference for terminal/UTF-8 detection behavior.
- `../sbbs3/xtrn_sec.cpp` is the reference for drop-file field ordering,
  especially `CHAIN.TXT`.

Keep changes in those sibling projects out of DeuceGate work unless the task
explicitly requires them.

## Compatibility invariants

- Preserve the existing GameSrv `config/gamesrv.ini`, menu INIs, door INIs,
  user INIs, MCI codes, actions, macros, and screen-selection behavior.
- Optional DeuceGate configuration belongs in `[CONFIGURATION]`; existing files
  must continue to work when the new keys are absent.
- Treat menu and door names case-insensitively even on case-sensitive hosts.
- The internal text representation is UTF-8. Caller and door encodings are
  independent, and all four CP437/UTF-8 combinations must remain supported.
- Conversion is streaming: never assume a UTF-8 sequence arrives in one read.
  Preserve ANSI/control bytes and replace unrepresentable text with `?`.
- PTY columns and rows come from SSH and must be propagated to `CHAIN.TXT`.
- Maintain the established drop files and command macros. A native socket door
  receives a new local plaintext socket; never expose or inherit the encrypted
  SSH transport socket.
- DOS doors must remain available on both Windows and POSIX through DOSBox-X or
  DOSBox. DOSEMU is POSIX-only. Use an IPv4 loopback-only nullmodem listener with
  DOSBox Telnet processing disabled.
- If a root `dosbox.conf` exists, load it before the generated per-node override.
  Per-node temporary files must be removed when the door exits.

## Authentication and security invariants

- DeuceSSH is the only network server. Do not add cleartext Telnet, RLogin,
  WebSocket, or a separate password listener.
- New accounts use signed public-key TOFU. A key probe must not create or modify
  a user; creation occurs only after DeuceSSH has verified the signature.
- Once stored, a user's key must match exactly. Key replacement is an explicit
  administrator action through `--authorize ... --replace`.
- Existing GameSrv password accounts may authenticate with SSH password auth.
  Preserve Rick's exact legacy scheme: ASCII bytes of
  `PasswordSalt + password + PasswordPepper`, SHA-512 once, then SHA-512 of the
  digest 1024 more times, and Base64. `PasswordPepper=DISABLE` means plaintext
  comparison. Use DeuceSSH's crypto abstraction rather than adding a hash
  implementation.
- Compare secrets in constant time where applicable and cleanse temporary secret
  buffers.
- Host keys must be stable. On POSIX, reject host-key files accessible by group
  or others and create new keys with owner-only permissions.
- Reject SSH exec, subsystem, forwarding, and additional-channel requests. Only
  the interactive shell used by the menu/door session is supported.
- User-controlled command substitutions retain GameSrv's triple-star opt-in.
  Do not silently make ordinary macros attacker-controlled.
- Timed-event commands and door definitions are administrator-controlled input;
  caller input is not.

## Source map

- `main.c`: command-line handling.
- `config.c`: GameSrv configuration loading and validation.
- `server.c`: listener, DeuceSSH callbacks, authentication, node ownership,
  shutdown, online-user state, and timed events.
- `user.c`: user lookup, TOFU/key authorization, atomic INI updates, and legacy
  password verification.
- `terminal.c`: Synchronet-compatible terminal detection and SSH channel I/O.
- `charset.c`: streaming CP437/UTF-8 conversion.
- `menu.c`: GameSrv processes, menus, actions, screens, and pagination.
- `mci.c`: MCI expansion.
- `door.c`: door INIs, macros, drop files, native processes, emulators, relays,
  and process-tree cleanup.
- `util.c`: paths, atomic files, logging, and small shared helpers.
- `tests/test_deucegate.c`: unit and compatibility regression tests.
- `tests/fixture`: minimal in-place GameSrv root used by CTest.

## Editing conventions

- Use C17 and the existing style: tabs for C indentation, braces on their own
  line for functions, and small file-local helpers.
- Prefer xpdev wrappers over new platform-specific abstractions. When native OS
  APIs are necessary, keep `_WIN32` and POSIX branches adjacent and equivalent.
- Keep bounded strings NUL-terminated. Check truncation where it changes
  behavior, especially command lines, paths, keys, and MCI output.
- Use atomic replacement for persistent INI and status files. Do not leave a
  partially written user or `whoisonline.txt` after interruption.
- Signal handlers may only perform async-signal-safe operations. Let the normal
  server loop terminate sessions and doors.
- Do not edit generated build files or commit build output.

## Required verification

Run the native build and tests after every functional change:

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For SSH/session changes, also run a live OpenSSH-client check against a temporary
copy of `tests/fixture`: verify signed-key TOFU enrollment, a subsequent login,
menu display, and clean logoff. Add a legacy-password live check when changing
password authentication.

Changes touching `_WIN32`, process creation, drop files, sockets, or DOS support
also require a 64-bit MinGW compile and final link. In this Synchronet checkout,
the existing Windows Botan package can be selected without leaking host
`pkg-config` flags:

```sh
cmake -S . -B /tmp/deucegate-mingw \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=/home/admin/mingw-w64/bin/x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=/home/admin/mingw-w64/bin/x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=/home/admin/mingw-w64/bin/x86_64-w64-mingw32-windres \
  -DDEUCEGATE_BOTAN_ROOT=../../3rdp/gcc.win64.x64.release/botan \
  -DDEUCEGATE_BUILD_TESTS=OFF -DBUILD_TESTING=OFF
cmake --build /tmp/deucegate-mingw --parallel
```

Toolchain locations vary; use equivalent paths when they are not installed at
the locations above. A successful object build is insufficient: verify that
`deucegate.exe` links successfully. Run it under Wine or on Windows when the
behavior being changed can be exercised there.

Before handoff, also check:

```sh
rg -n -i '[T]ODO|[F]IXME' . -g '!build/**'
git diff --check
```

Warnings emitted solely by sibling xpdev/conio sources may be reported, but new
warnings from DeuceGate sources should be fixed.
