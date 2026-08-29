# DeuceGate

DeuceGate is a C17, SSH-native replacement for Rick Parrish's GameSrv door server. It reads an existing GameSrv directory in place and uses DeuceSSH for all remote connections.

## Build

DeuceGate uses the sibling `xpdev`, `ssh`, `conio`, and `encode` sources from the Synchronet tree.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

DeuceGate and SyncTERM use the same crypto selection policy: system Botan 3.13 or newer is used when available, and the bundled Botan source is built otherwise. OpenSSL 3.0 or newer remains available when selected explicitly with `-DCRYPTO_BACKEND=OpenSSL`; use `-DUSE_VENDORED_BOTAN=1` to force the bundled Botan build or `-DUSE_VENDORED_BOTAN=0` to opt out of it.

For a MinGW cross-build with a Botan 3.13-or-newer target installation that should not be discovered through the host's `pkg-config`, pass `-DCMAKE_SYSTEM_NAME=Windows`, the MinGW C/C++/resource compilers, and `-DDEUCEGATE_BOTAN_ROOT=/path/to/windows/botan`.

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

- `Encoding` is `CP437` (default), `UTF-8`, or `Auto`. Fixed encodings are
  independent of the caller's detected encoding. `Auto` is for adaptive doors
  that honor `DEUCEGATE_ENCODING`; DeuceGate sets it to the caller's encoding
  and relays the door session byte-for-byte without character conversion.
- `IO` is `Socket` (the GameSrv/DOOR32-compatible default) or `Stdio` for native doors.
- `Emulator` is `Auto`, `DOSBox-X`, `DOSBox`, or `DOSEMU`. Auto tries DOSBox-X, DOSBox, then DOSEMU. DOSEMU is POSIX-only.

DeuceGate sets `DEUCEGATE_ENCODING` to `CP437` or `UTF-8` in each native
door's environment. Generated DOSBox and DOSEMU environments set the same
variable. For fixed-encoding doors it describes the door-facing byte stream
after conversion; for `Encoding=Auto` it describes the unchanged caller byte
stream.

When the SSH client supplies a language preference, DeuceGate also sets
`DEUCEGATE_LANGUAGE_TAG` to a BCP 47 tag such as `en-US`. Preference order is
the same-named SSH environment variable, then `LC_ALL`, `LC_MESSAGES`, and
`LANG`. POSIX locale suffixes are removed, so for example `fr_CA.UTF-8`
becomes `fr-CA`. OpenSSH can send the explicit tag
with `SendEnv DEUCEGATE_LANGUAGE_TAG`; only these locale-related environment
variables are accepted. Language and character encoding are independent.
When the client supplies no language preference, DeuceGate uses `en`.

DOSBox and DOSBox-X run on Windows and POSIX. DeuceGate loads a root `dosbox.conf` when present, adds a per-node override, mounts the GameSrv root as `C:`, loads `dosutils/fossil.com`, `share.com`, and `ansi.com` when present, and connects COM1 to an ephemeral IPv4 loopback-only nullmodem socket with Telnet processing disabled. DOSEMU uses its PTY/FOSSIL `external.bat` path.

Native socket doors receive a new local plaintext socket handle in `DOOR32.SYS`, `*HANDLE`, and `*SOCKETHANDLE`; the encrypted SSH socket is never inherited by a door. DeuceGate also creates the UTF-8 [`BBSDEV.DRP`](https://github.com/RealDeuce/bbsdev.drp) format and exposes its absolute path through `BBSDEV_DRP` (and the optional `*BBSDEV` command macro). Its communications field is `socket` or `stdio` for native doors and `fossil` port 0 for DOS doors. Drop files and other command macros follow GameSrv's existing names and layout. `CHAIN.TXT` is also generated with the current SSH PTY columns and rows and is available through `*CHAIN`. User-controlled macros still require the legacy triple-star opt-in, such as `***ALIAS`.

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
