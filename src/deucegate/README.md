# DeuceGate

DeuceGate is a C17, SSH-native replacement for Rick Parrish's GameSrv door server. It reads an existing GameSrv directory in place and uses DeuceSSH for all remote connections.

## Build

DeuceGate uses the sibling `xpdev`, `ssh`, `conio`, and `encode` sources from the Synchronet tree.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The DeuceSSH build selects Botan 3 or OpenSSL 3 automatically. It can be selected explicitly with `-DDEUCESSH_CRYPTO_BACKEND=Botan` or `OpenSSL`.

For a MinGW cross-build with a target Botan installation that should not be discovered through the host's `pkg-config`, pass `-DCMAKE_SYSTEM_NAME=Windows`, the MinGW C/C++/resource compilers, and `-DDEUCEGATE_BOTAN_ROOT=/path/to/windows/botan`.

## Run

The root is the directory containing `config/gamesrv.ini`, `menus`, `doors`, and `users`:

```sh
deucegate --root /srv/gamesrv --check-config
deucegate --root /srv/gamesrv
```

DeuceGate runs in the foreground and logs to stderr. SIGINT/SIGTERM or a Windows console shutdown stops the listener, active SSH sessions, and their doors cleanly.

### SSH settings

These optional keys belong in `[CONFIGURATION]` in `config/gamesrv.ini`:

```ini
SSHServerIP=0.0.0.0
SSHServerPort=22
SSHHostKey=config/ssh_host_ed25519.pem
DOSBoxPath=
DOSBoxXPath=
DOSEmuPath=
```

An Ed25519 server host key is generated on first start if `SSHHostKey` does not exist. Keep this file stable and backed up: callers use it to identify the server.

New aliases authenticate with an Ed25519 or RSA-SHA2 key. The first correctly signed key creates the GameSrv user and is stored as `SSHKeyAlgorithm` and `SSHKey` in that user's existing INI format. Later keys must match. Key probes do not create users.

Existing GameSrv users can continue using their legacy password over SSH. DeuceGate verifies Rick's original `PasswordSalt`/`PasswordHash`/`PasswordPepper` format, including `PasswordPepper=DISABLE`; it never creates a new password account. To enable or migrate an existing user to keys:

```sh
deucegate --root /srv/gamesrv --authorize ALIAS ~/.ssh/id_ed25519.pub
deucegate --root /srv/gamesrv --authorize ALIAS new-key.pub --replace
```

Password, public-key, and anonymous RUNBBS authentication are SSH authentication methods: no cleartext Telnet, RLogin, or WebSocket listener is opened. SSH exec, subsystems, agent/port forwarding, and extra channels are rejected.

If `doors/_runbbs.ini` exists, SSH `none` authentication is accepted and that door is launched as the anonymous `RUNBBS` user. This compatibility mode is intentionally less restrictive and should only be enabled when anonymous access is desired.

## Door settings

Existing door INIs work unchanged. Optional additions in `[DOOR]` are:

```ini
Encoding=CP437
Emulator=Auto
IO=Socket
```

- `Encoding` is `CP437` (default) or `UTF-8`, independently of the caller's detected encoding.
- `IO` is `Socket` (the GameSrv/DOOR32-compatible default) or `Stdio` for native doors.
- `Emulator` is `Auto`, `DOSBox-X`, `DOSBox`, or `DOSEMU`. Auto tries DOSBox-X, DOSBox, then DOSEMU. DOSEMU is POSIX-only.

DeuceGate sets `DEUCEGATE_ENCODING` to `CP437` or `UTF-8` in each native
door's environment. Generated DOSBox and DOSEMU environments set the same
variable. It describes the encoding of the door-facing byte stream after
DeuceGate's conversion, not the caller's encoding.

DOSBox and DOSBox-X run on Windows and POSIX. DeuceGate loads a root `dosbox.conf` when present, adds a per-node override, mounts the GameSrv root as `C:`, loads `dosutils/fossil.com`, `share.com`, and `ansi.com` when present, and connects COM1 to an ephemeral IPv4 loopback-only nullmodem socket with Telnet processing disabled. DOSEMU uses its PTY/FOSSIL `external.bat` path.

Native socket doors receive a new local plaintext socket handle in `DOOR32.SYS`, `*HANDLE`, and `*SOCKETHANDLE`; the encrypted SSH socket is never inherited by a door. Drop files and command macros follow GameSrv's existing names and layout. `CHAIN.TXT` is also generated with the current SSH PTY columns and rows and is available through `*CHAIN`. User-controlled macros still require the legacy triple-star opt-in, such as `***ALIAS`.

## Terminal handling

The caller is classified with Synchronet's CPR/BOM terminal probe. CP437 content is converted with `conio/utf8_codepages.c`. A UTF-8 stream is used internally, so all four caller/door combinations work:

- CP437 caller and CP437 door
- CP437 caller and UTF-8 door
- UTF-8 caller and CP437 door
- UTF-8 caller and UTF-8 door

Invalid UTF-8 or Unicode that CP437 cannot represent is replaced with `?`. ANSI control sequences remain byte-compatible.

## Compatibility notes

- Menus, access-specific screens, ANSI/ASCII/RIP selection, MCI fields, logon/logoff processes, drop files, pagination, timed events, node limits, multiple-login policy, and `whoisonline.txt` use the existing GameSrv files.
- `newuser.ini` prompts are not run for key-TOFU accounts; additional-information values start empty.
- `Telnet` menu actions remain parseable but display an unavailable message and are logged.
- `banned-ips.txt`, `ignored-ips.txt`, `ignored-ips-combined.txt`, and `banned-users.txt` remain effective.
- A timed event with `GoOffline=True` stops admitting callers, disconnects active nodes, runs the configured command, and resumes afterward.
