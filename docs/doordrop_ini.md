# DOORDROP.INI: a named-value door drop file (draft)

Draft 0.7 · 2026-10-01 · Rob Swindell

**This is a draft under community review. Don't implement it, in a host or a door, except to experiment, until it reaches 1.0.** Keys, names and rules are still changing between drafts, and an implementation of a draft will need rework; the draft 0.3 implementation already does, after the file and variable were renamed. Review comments are welcome (see Open questions for where the draft is headed).

## Status and goals

DOORDROP.INI hands a door the details of a caller's session as named `KEY=value` lines, so a door reads only the keys it needs and new keys need no central registry. In this spec, the **host** is the BBS or other system that runs the door and writes the file. This is draft 0.7. Through draft 0.6 the file was called DROPFILE.INI and the variable `DROPFILE_INI`; the name changed to avoid confusion with the `DROPFILE.###` of DTS-0001 (see Why INI), and an implementation written against an earlier draft uses the old names. The key words MUST, MUST NOT, SHOULD, SHOULD NOT and MAY are used as described in BCP 14 [RFC2119] [RFC8174] when they appear in capitals.

Goals:

- **Readable by a DOS door in a dozen lines** of C, Turbo Pascal or QBasic: no quoting, escapes or nesting, and section headers a simple reader can ignore.
- **Readable by standard INI APIs** such as Win32 `GetPrivateProfileString()` and Python's `configparser`, which require section headers, within the limits given under Examples and minimal readers.
- **Order-independent and forward-compatible:** readers ignore keys they don't know, and every optional key has a stated default.
- **Extensible without asking anyone:** vendor-prefixed keys can't collide with standard keys or with each other.
- **Carries user preferences** that doors otherwise store separately per door, such as sound muted, screen pause and mouse use.
- **Carries detected terminal capabilities,** so a door doesn't have to query the terminal again.
- **Replaces the older drop files:** it carries the session details doors commonly read from DOOR.SYS, DOOR32.SYS and DORINFO1.DEF, so a door written for it needs no other drop file. Their security level field is left out on purpose (see User).

Non-goals:

- Returning data to the host. The file is read-only input. A host MAY offer its own way back, such as Synchronet's MODUSER.DAT.
- Authentication. Nothing in the file grants privileges.

## Why INI

The format is the simplest one that meets all of the goals above. Each alternative fails at least one:

- **Positional files** (DOOR.SYS, DORINFO1.DEF, DOOR32.SYS) give a value its meaning by line number. A producer must write every line, inventing placeholders for data it doesn't have. A new field can only be appended, and one missing or extra line shifts every later value. DOOR.SYS [DOOR.SYS] already exists in 52-line and 31-line forms that doors must tell apart. Named keys remove all of this.
- **JSON** needs a real parser, with string escapes, nesting and `\u` sequences, which is impractical in QBasic or Turbo Pascal on DOS. It must be UTF-8 [RFC8259], so it can't carry CP437 text for doors that can't decode UTF-8. One malformed byte makes the whole file unreadable.
- **TOML** is the nearest alternative, but its strings must be quoted and escaped, and it must be UTF-8. A DOS door would need a TOML parser to read a value that INI gives it with `Pos('=', S)`.
- **YAML** is a large specification with significant indentation and implicit typing: under YAML 1.1 an unquoted `NO` or `Y` becomes a boolean, and under any version `1.10` becomes the number `1.1`. No DOS door has a YAML parser.
- **XML** needs a parser and entity escaping, for the same reasons as JSON.
- **Environment variables alone** don't fit: a DOS environment defaults to a few hundred bytes, and emulators and shells differ in what they pass through.
- **Plain `KEY=value` lines without sections** would also serve a simple reader, and many INI libraries, such as C's inih and PHP's `parse_ini_file()`, accept keys before any section header. But no Win32 profile function (`GetPrivateProfileString()` and the rest) can read a key outside a section, and Python's standard-library `configparser` rejects one unless the reader adds a header itself. Sections cost a simple reader nothing, because it ignores them, and they group related keys for a sysop reading the file.

INI is also familiar to sysops and door authors, most BBS software already reads and writes it, and a sysop can read the file in any text editor when a door misbehaves.

A named-value drop file was proposed once before. DTS-0001 [DTS-0001], a 1992 draft, defined a `DROPFILE.###` of `LABEL = DATA` lines in any order, with no setting required, a default for each, a list of 5D network addresses, and a `CHANGES.###` file for returning edits. It never went past a beta draft or gained a host, but it reached the same conclusions as this spec, and its settings are compared under Coverage of older drop files.

## File name and discovery

The host points the door to the file with the environment variable `DOORDROP_INI` or on the door's command line, so the file can have any name. Its customary name is `DOORDROP.INI`, which a host SHOULD use unless the door expects another; it may be in lowercase (`doordrop.ini`) on a file system that keeps case. A file with any other name MUST keep the `.INI` extension, which no older drop file uses, so a door that chooses its parser by file name can recognize the format. Because the door always receives the path, it SHOULD open exactly that path rather than look for the file by name. Some doors are configured with a drop file directory and look for a known file name in it; a door that does so with this file MUST match the name without regard to case, since the name and its case are the host's choice, and still accepts a full path, since the host may have used another name.

- When more than one node can run the door at the same time, no two nodes' files may share a path, or they would overwrite each other. A host that gives every node the same file name, such as `DOORDROP.INI`, MUST therefore write each node's file in a directory specific to that node, such as a per-node directory; one that gives each node its own file name, such as `NODE1.INI`, MAY use a shared directory. A single-node host, or a host that lets only one node at a time run the door, MAY write the file in a shared directory, such as the door's own directory, under any name.
- The host MUST finish writing the file and close it before starting the door. A DOS emulator that is already running may not see a new file, because DOSBox caches directory listings, so the host starts the emulator after writing the file or mounts the directory with caching turned off.
- The host MUST give the door the file's absolute path, including the file name, in the `DOORDROP_INI` environment variable, on the door's command line, or both. It SHOULD set `DOORDROP_INI` wherever it can. Some DOS doors can't receive it: an emulator such as DOSBox doesn't pass the host's environment through, and a DOS environment of a few hundred bytes may have no room left. For those, the host can still set the variable inside the emulated DOS session, with a `SET DOORDROP_INI=C:\NODE1\DOORDROP.INI` line in the batch file it generates to start the door; otherwise the command line is the only way.
- In `DOORDROP_INI` the path has no quotes and no shell escaping, even when it contains spaces.
- The path uses the syntax of the environment the door runs in. For a DOS door run under an emulator, it is the DOS path the door sees, such as `C:\NODE1\DOORDROP.INI`, not the host path.
- A path given to a DOS door, in `DOORDROP_INI`, on its command line or in `TEMP_DIR`, MUST fit DOS's limits: every directory and file name 8.3, at most 64 characters for the directory part and 80 for the whole path. A DOS command line holds at most 126 characters, including the door's other arguments, so the host SHOULD keep the path short, such as `C:\NODE1\DOORDROP.INI`.
- A host MUST be able to pass the path on the door's command line, as many DOS doors expect. The sysop places the path on the door's configured command line where the door expects it. A door SHOULD accept the path on its command line, as a bare argument unless it documents a switch of its own, so it still works where the environment variable can't reach it, and uses `DOORDROP_INI` when it isn't given one. Given neither, it reports that and exits. A door MAY instead accept a directory and look for `DOORDROP.INI` in it, as doors configured with a drop file directory do today, but this is discouraged: the door must then match the name case-insensitively and can't be given a file with another name.
- The file SHOULD be readable only by the host and the door, and the host SHOULD remove it after the door exits.
- A file for a DOS door has an 8.3 name, which `DOORDROP.INI` is.

## File representation

Each line is blank, a comment, a section header (`[name]`), or `KEY=value`. A value runs from the character after the first `=` to the end of the line, exactly as written.

Section headers are there for readers that require them, such as Win32 `GetPrivateProfileString()` and Python's `configparser`. Key names don't depend on them: every key name is unique across the whole file, which is why most keys repeat their section in a prefix (`USER_ALIAS` in `[user]`, `TERM_COLS` in `[terminal]`). A reader can look keys up by section, or ignore section headers entirely and match key names alone, and gets the same value either way.

In this spec, **whitespace** means ASCII space (`0x20`) and tab (`0x09`). A **control character** is any byte `0x00` through `0x1F` or `0x7F`, and in UTF-8 text also any code point U+0080 through U+009F, the line and paragraph separators U+2028 and U+2029, and the bidirectional format characters U+202A through U+202E and U+2066 through U+2069. The last two groups are invisible, and the separators split a line in readers such as Python's `str.splitlines()` and a JavaScript multiline regular expression: an alias of `A`, U+2028, `USER_ROLE=sysop` is 19 bytes, fits a short name field, and would make such a reader see a second key. The definition applies to CP437 text too, where those bytes would otherwise display as symbols such as `☺` and `⌂`: a DOS reader can't tell such a symbol from a control code, CR and LF split the line, and many DOS text-mode readers stop at `0x1A` (Ctrl-Z) as end of file. Text also MUST NOT contain a non-breaking space, which nobody can see: byte `0xFF` in CP437, which on a Telnet connection is also the IAC command byte, or U+00A0 in UTF-8.

Producers MUST:

- write keys, section headers and all values other than text and path in ASCII, text values in UTF-8 if `FILE_UTF8` is `1` and in CP437 otherwise, and path values as the file system gives them, with no byte-order mark;
- end every line, including the last, with CRLF, and write nothing after the final CRLF, including no Ctrl-Z end-of-file marker;
- write a section header before the first key, with `[file]`, when written, as the first section (comment lines may come before it);
- write each key in its assigned section, and each section at most once; a section MAY be empty, with no keys between its header and the next, and a producer MAY leave out a section that has no keys;
- write section names in lowercase ASCII letters, digits and `-`;
- write keys in uppercase ASCII letters, digits and `_`, starting with a letter, at most 32 characters;
- write no spaces or tabs around the key or the `=`;
- write no control characters in a value, and no leading or trailing whitespace;
- keep every line to at most 255 bytes, not counting CRLF, so it fits a Turbo Pascal `string`. A value can therefore be at most 255 bytes minus the key's length and the `=`, and since keys are at most 32 characters, no value exceeds 222 bytes; a longer text value is cut at a character boundary, and a longer value of any other type is left out, since a cut path, address or host name is worse than none;
- write each key at most once.

A stored string can break these rules, such as a user name with a trailing space or a CP437 `☺`. The producer replaces each non-breaking space (CP437 `0xFF` or U+00A0) with a space, removes leading and trailing whitespace, and replaces each control character with `?`, as it does for characters that have no equivalent in the file's text encoding. If an optional key's value is then empty, the producer leaves the key out. If a required text key's value is then empty, the producer writes `?`.

A comment is a line whose first character is `;`. A consumer MAY also treat a line starting with `#` as a comment, as some INI parsers do. A `;` or `#` later in a line is part of the value, so `USER_ALIAS=Joe;Bob` is the alias `Joe;Bob`.

Consumers:

- MUST accept CRLF as a line terminator and SHOULD also accept LF alone as one. A reader that splits on LF alone, as many POSIX readers do, removes the CR left at the end of each line; otherwise every value ends in a CR, and a comparison such as `COMM_TYPE` against `socket` fails;
- SHOULD treat a Ctrl-Z (`0x1A`) as the end of the file, as DOS text-mode reads already do;
- MAY remove whitespace around a key or a value, which changes nothing in a conforming file;
- MAY compare keys case-sensitively, because producers write them in uppercase;
- MAY ignore section headers;
- MUST ignore keys they don't recognize, and other lines that contain no `=`;
- SHOULD use the first occurrence if a key appears twice;
- MUST treat a missing optional key as its stated default;
- MUST treat a token, or a value from a list this spec defines, that they don't recognize as the key's default, never as a more capable or more privileged value: an unknown `USER_ROLE` is `user`, an unknown `TERM_TYPE` is `dumb`, and an unknown `TERM_SIXEL_SCALE` is unknown. Bool values above `1` are the exception, under Value types;
- need to check only the values they use. A door that reads three keys doesn't have to validate the rest of the file.

There is no quoting or escaping. A value can contain `=`, `:`, `[`, `"` and `\` as literal characters, but cannot contain a line break.

A simple reader can skip comment and section handling: comment lines and section headers can't match any key, because no key starts with `;` or `[`, and headers contain no `=`.

Text values are in CP437, or in UTF-8 when `FILE_UTF8` is `1`. The choice is independent of `COMM_CHARSET`: a host that stores only CP437 text MAY write CP437 text for a door on a UTF-8 connection, and the door converts text values before sending them. A producer MUST NOT write `FILE_UTF8=1` unless it knows the door supports UTF-8, for example because the sysop configured the door that way, so a door that can't decode UTF-8 never receives it. A producer writing CP437 replaces characters that have no CP437 equivalent with `?`. ASCII text is valid either way. Keys and non-text values are always ASCII, so a reader can find `FILE_UTF8` before decoding any text.

## Value types

Every key has one of nine types, each parseable with a standard library call or a short loop.

| Type | Form | Example |
| --- | --- | --- |
| text | In CP437, or UTF-8 when `FILE_UTF8` is `1`; non-empty, no control characters or non-breaking spaces, no leading or trailing whitespace | `Jörg the Red` |
| ascii | Printable ASCII (`0x21` through `0x7E`), non-empty, no spaces; readable before the text encoding is known | `en-US` |
| int | Unsigned decimal, 0 through 2147483647 (fits Pascal `LongInt` and C `long`), with no sign and no leading zeros (other than `0` itself) | `80` |
| uint64 | Unsigned, 0 through 18446744073709551615, in decimal as for `int`, for counts too large for `int` | `5368709120` |
| handle | Unsigned decimal native socket, descriptor or handle value, up to 64 bits | `1234` |
| bool | `0` (false) or any other unsigned decimal (true), in the form of an `int`; `1` is the only true value unless the key's definition names others | `1` |
| token | Lowercase ASCII word from a list defined by this spec | `socket` |
| date | `YYYY-MM-DD` | `1970-01-01` |
| path | A file system path in the syntax of the environment the door runs in, written byte for byte as the host's file system gives it, never converted to the file's text encoding and never cut: a path that contains a control character or doesn't fit is left out | `C:\NODE1` |

A consumer reads a bool with the same decimal parser it uses for ints, or with Win32's `GetPrivateProfileInt()`, and tests the result: `0` is false and any other value is true, as in `atol(value) != 0` in C or `VAL(s$) <> 0` in QBasic. It MUST NOT compare the value against `1`. A producer writes `0` or `1` unless the key's definition names another value.

A key's definition MAY name values above `1` that mean true and carry more detail, as `TERM_MOUSE` does with `2`. Such a value MUST be a refinement of true: a consumer that knows only `0` and `1` and treats it as `1` still behaves correctly, if less capably. A consumer treats a value it doesn't know as `1`. The values are a list the key defines, not a bitmask, and there is only one false value: a refinement of false needs its own key.

Text values have no length limit of their own beyond the line limit (at most 222 bytes for any value, and 255 minus the key length and `=` for a given key), and the limits BBS packages put on user and system names differ. A door that shows a value in a fixed-width field truncates it itself, counting characters, not bytes, when the text is UTF-8. The Synchronet implementation notes list Synchronet's limits as an example.

Producers write tokens in lowercase, exactly as this spec lists them, so a consumer MAY compare a token case-sensitively, as it may a key.

Every numeric value is decimal; the file has no hexadecimal. C's `strtol()` (or `strtoull()` for a uint64 or handle) and Python's `int()` read them directly. `strtoull()` is C99, which Turbo C lacks; a DOS door never receives a handle, and one that can't hold a 64-bit value skips any uint64 key it can't use.

Values can exceed 32767, the limit of the default integer type in Turbo C (`int`), Turbo Pascal (`Integer`) and QBasic (`%`); `TIME_LEFT` does after about 9.1 hours. A DOS reader stores int values in a 32-bit type: `long`, `LongInt`, or a QBasic `&` variable.

Time limits are given as seconds remaining (`TIME_LEFT`), not as a clock time. A DOS door often has no `TZ` setting, and under emulation its clock may not match the host's, but any door can count down seconds from when it started.

## Defined keys

Five keys are always required: `SYS_SOFTWARE`, `SYS_NAME`, `SYS_OP`, `SYS_NODE_NUM` and `COMM_TYPE`. `COMM_CHARSET`, `USER_ALIAS` and `USER_NUMBER` are also required unless `COMM_TYPE` is `local`, where no caller is connected; then the user keys, if present, describe the account the door runs under. The connection keys marked "for" a type are required with that type. Every other key is optional, and the Default column says what a missing key means.

### File: `[file]`

Keys that describe the file itself. All are optional, so the section may be empty or left out. A producer SHOULD always write `FILE_TIME`, leaving out the UTC offset if it doesn't know it. A door MAY log it or show it to the sysop, but SHOULD NOT refuse to run because of it, since a DOS door's clock often doesn't match the host's.

| Key | Type | Meaning | Default |
| --- | --- | --- | --- |
| `FILE_UTF8` | bool | `1` = every text value in the file is UTF-8 | `0` (CP437) |
| `FILE_TIME` | ascii | Local date and time the host wrote the file, in ISO 8601 form with its UTC offset, such as `2026-09-25T14:30:00-07:00`, or without the offset (`2026-09-25T14:30:00`) when the host doesn't know it. It is for people and logs, to spot a stale file left by a misconfigured host or door, not for measuring time: a door uses `TIME_LEFT` for that | none |

### System: `[system]`

| Key | Type | Req. | Meaning | Default |
| --- | --- | --- | --- | --- |
| `SYS_SOFTWARE` | text | yes | Display name and version of the system's software; not for parsing (see `SYS_VENDOR` and `SYS_VERSION`) | |
| `SYS_NAME` | text | yes | Name of the system | |
| `SYS_OP` | text | yes | Name of the system operator (sysop), as the host is configured with it: an alias, handle or real name | |
| `SYS_NODE_NUM` | int | yes | Node number, starting at `1`; a single-node system writes `1` | |
| `SYS_NODE_COUNT` | int | no | Number of nodes the system is configured for, which is also its highest node number | unknown |
| `SYS_QWKID` | ascii | no | The system's short ID: 1 to 8 characters that are valid in a DOS file name, with letters in uppercase, not starting with a digit. It is the ID used for QWK packets and QWK networks, and also serves to identify the system to inter-BBS doors and games. It is unique within a network, not necessarily worldwide | unknown |
| `SYS_HOSTNAME` | ascii | no | The host's public Internet host name, such as `bbs.example.com`, for building links or telling users where to connect | unknown |
| `SYS_FTN_ADDR` | ascii | no | The host's FidoNet Technology Network (FTN) addresses, comma-separated with the primary first, each in `zone:net/node` or `zone:net/node.point` form and optionally followed by `@domain` naming its network, such as `1:103/705@fidonet,21:1/121@fsxnet`, for inter-BBS games that exchange data over FTN. A game on an othernet looks for the address in that network's zone or domain rather than taking the first | unknown |
| `SYS_LOCATION` | text | no | Where the host is, as its sysop describes it, such as `Los Angeles, California` | unknown |
| `SYS_OP_AVAILABLE` | bool | no | `1` = the sysop is available to be paged for chat | `0` |
| `SYS_DATE_FORMAT` | token | no | The order the host shows dates in: `mdy` (month, day, year), `dmy` or `ymd`, so a door can show dates the same way | unknown |
| `SYS_VENDOR` | ascii | no | The vendor name the system uses in its `X_<VENDOR>_` keys, in uppercase, such as `SBBS` | unknown |
| `SYS_VERSION` | ascii | no | Version of the system's software in the vendor's own format, such as `3.22a`; compared only by doors that know that vendor | unknown |

### Connection: `[comm]`

| Key | Type | Req. | Meaning | Default |
| --- | --- | --- | --- | --- |
| `COMM_TYPE` | token | yes | How the door talks to the caller (list below) | |
| `COMM_CHARSET` | ascii | unless `local` | Character set the door MUST use on the connection (list below) | |
| `COMM_HANDLE` | handle | for `socket`, `telnet`, `serial` | Inherited socket or serial handle, native to the door's platform. It MUST be an open handle: a host without one uses another type, such as `stdio`, never a placeholder such as `-1` | |
| `COMM_PORT` | int | for `fossil`, `uart` | COM port number, 1-based (`1` = COM1; FOSSIL `DX` = `COMM_PORT` - 1) | |
| `UART_BASE` | int | for `uart` | UART I/O base address, in decimal, such as `1016` for COM1 | |
| `UART_IRQ` | int | for `uart` | UART IRQ, 0 through 15 | |
| `COMM_RATE` | int | no | DTE rate in bits per second, for doors that must set it | door leaves the port as configured |

`COMM_TYPE` values:

| Token | Meaning |
| --- | --- |
| `local` | The door uses its local console. No caller is connected; a host doesn't use this type for a caller's session. |
| `stdio` | Caller input on standard input (`stdin`, descriptor 0; Win32 `STD_INPUT_HANDLE`) and output on standard output (`stdout`, descriptor 1; Win32 `STD_OUTPUT_HANDLE`), carrying only terminal bytes: the host has done any Telnet, SSH or WebSocket processing, passes each input byte on as it arrives, and does no echo or line editing. Standard error (`stderr`, descriptor 2; Win32 `STD_ERROR_HANDLE`) doesn't reach the caller. A DOS door that talks to the caller through DOS's standard handles, as WWIV chains do, is `stdio` too: under an emulator whose standard handles the host connects to the session, the door can't tell it from a native standard-I/O program. |
| `socket` | A connected socket carrying only terminal bytes; the host has already done any Telnet, SSH or WebSocket processing. |
| `telnet` | A connected socket carrying the caller's Telnet stream. The door handles the Telnet protocol itself: it answers option negotiations, sends a `0xFF` data byte as `IAC IAC`, and handles CR NUL. The host uses this type only for a caller that is actually on Telnet. The host has usually negotiated ECHO, SUPPRESS-GO-AHEAD and BINARY already; the door MAY negotiate them again. |
| `serial` | An open, configured serial port, as a handle native to the door's platform: a POSIX file descriptor, a Win32 COM handle or an OS/2 handle. |
| `fossil` | An initialized FOSSIL driver [FSC-0015]. |
| `uart` | Direct DOS UART access. |

DOOR32.SYS's [DOOR32] comm type `0` (local) corresponds to `local`. Its type `1` (serial) corresponds to `serial`; the Linux door kits checked (OpenDoors, d32 and termgfx) implement no serial I/O at all. A DOS DOOR32.SYS door typically ignores the type and uses a FOSSIL driver, which corresponds to `fossil`. Its type `2` (telnet), a socket on which the door does its own Telnet processing, corresponds to `telnet`. `socket` and `telnet` can't stand in for each other: a door that treats a plain socket as Telnet garbles every `0xFF` byte, and one that treats a Telnet stream as plain shows the negotiation bytes as garbage. A door MUST NOT fall back to another type if it doesn't support the one given.

A missing `COMM_RATE` doesn't mean a local session. A tool that converts this file to DOOR.SYS or DORINFO1.DEF writes a nonzero rate, such as `38400`, because several door kits treat a rate of 0 as local.

For `socket`, `telnet` and `serial`, the door inherits a handle to a socket or port that the host also holds. The door's copy is its own handle, but the blocking mode (`O_NONBLOCK` on POSIX, `FIONBIO` on Windows) and the socket or port options belong to the shared socket or port, so a setting the door changes is still in effect after it exits. The rules below keep the door and the host from breaking each other.

The host:

- MUST make the handle inheritable, keep it valid in the door's process for the whole session, and keep its own reference open, so the door closing its copy doesn't end the connection;
- MUST keep other sessions' handles out of the door: a Windows host that runs its nodes as threads passes only the intended handle, with `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` or by making every other handle non-inheritable, or a door started at the same moment inherits another node's socket; a POSIX host marks every other descriptor close-on-exec;
- MUST pass a socket in blocking mode, because Winsock has no documented call that reports a socket's blocking mode, so a Windows door can't find out otherwise;
- MUST pass a serial handle with its speed and framing already configured;
- MAY set socket options, such as `TCP_NODELAY`, as it chooses;
- MUST re-apply the blocking mode, and SHOULD re-apply the options it relies on, after the door exits, rather than depend on the door to restore them; doors that leave a socket non-blocking are common.

The door:

- MAY change the blocking mode and the socket or port options it needs, and SHOULD restore the original values before it exits;
- MUST NOT call `shutdown()` on the socket, which ends the connection for the host too; closing its own handle is enough;
- MUST tolerate a failed option call: a `socket` may be one end of a local socket pair, where TCP options don't apply;
- SHOULD keep the handle from being inherited by programs it starts (close-on-exec on POSIX, not inheritable on Windows), unless it deliberately hands the session to one;
- on POSIX, SHOULD ignore `SIGPIPE` or write with `MSG_NOSIGNAL`, so a caller hanging up doesn't kill the door, and SHOULD exit promptly when a read returns end of file or an error;
- on Windows, calls `WSAStartup()` before using an inherited socket, and remembers that `WSAEventSelect()` and `WSAAsyncSelect()` switch the socket to non-blocking mode: to restore blocking mode it first clears that association, such as with `WSAEventSelect(s, NULL, 0)`.

`COMM_CHARSET` is the character set the door must send and expect on the connection, which isn't necessarily the caller's terminal character set (`TERM_CHARSET`). When the host translates the door's output, for example from CP437 to UTF-8, the value is the encoding the door writes (`CP437`). When the host doesn't translate, the value is the caller's actual encoding, even one the door can't produce. A door that can't use the given character set SHOULD tell the user and exit rather than send bytes the terminal will misdisplay. It writes that message in printable US-ASCII, which CP437, UTF-8 and US-ASCII terminals all display the same way, using only uppercase letters, digits, spaces and common punctuation, ending each line with CR LF: on a PETSCII terminal, lowercase ASCII letters can show as graphics characters.

| Name | Character set |
| --- | --- |
| `CP437` | IBM PC code page 437 (IANA `IBM437`, alias `cp437`) |
| `UTF-8` | UTF-8 |
| `US-ASCII` | 7-bit ASCII |
| `PETSCII` | Commodore PETSCII |

These are this spec's own names, used by both `COMM_CHARSET` and `TERM_CHARSET`: three match IANA names or aliases, and `PETSCII` has no IANA registration. Producers write them exactly as listed, so a consumer MAY compare them case-sensitively, as it may a token. For a character set not listed, a producer MAY use its IANA name; a consumer compares such a name case-insensitively, as IANA names are.

### User: `[user]`

| Key | Type | Req. | Meaning | Default |
| --- | --- | --- | --- | --- |
| `USER_ALIAS` | text | unless `local` | The user's alias | |
| `USER_NUMBER` | int | unless `local` | The user's number on this host; may be reused after the account is deleted | |
| `USER_KEY` | ascii | no | Opaque key that never changes for the account and is never reused on this host; ASCII letters, digits, `-`, `_` and `.` only, starting with a letter or digit, at most 64 characters | none |
| `USER_ROLE` | token | no | `user`, `cosysop`, `sysop` or `guest`. `guest` is an account shared by callers who haven't registered, so a door SHOULD NOT save progress or rankings for it as one person's | `user` |
| `USER_LANG` | ascii | no | BCP 47 [BCP47] language tag, such as `en-US` | unknown |
| `USER_REALNAME` | text | no | Real name (see Security and privacy) | none |
| `USER_LOCATION` | text | no | Location as the user entered it | none |
| `USER_BIRTHDATE` | date | no | Date of birth | none |
| `USER_GENDER` | text | no | As the host stores it, often a single character such as `M` or `F` whose meanings the host defines | none |
| `USER_IP` | ascii | no | The IP address the user connected from, in IPv4 dotted-decimal or IPv6 text form (see Security and privacy) | none |
| `USER_HOSTNAME` | ascii | no | The host name of `USER_IP`, from a reverse DNS lookup | none |
| `USER_CALLER_ID` | ascii | no | The calling phone number from Caller ID, for a dial-up caller | none |
| `USER_PROTOCOL` | token | no | How the caller reached the host: `telnet`, `rlogin`, `ssh`, `raw` (a plain TCP connection), `websocket` or `serial` (a serial line, through a modem or a direct cable). | unknown |
| `USER_HANDLE` | text | no | The user's short nickname, such as for multi-node or inter-BBS chat doors. It may be an amateur radio call sign or another short name the user is known by | none |
| `USER_EMAIL` | ascii | no | The user's e-mail address on this host, such as `Joe.Bob@bbs.example.com`, where the host accepts mail for its users | none |
| `USER_NETMAIL` | ascii | no | The address the user's mail is forwarded to: an Internet e-mail address, or a FidoNet address such as `1:103/705` (see Security and privacy) | none |

A host that can't guarantee a key that is never reused, for example because it reuses deleted users' numbers internally, leaves `USER_KEY` out. A door that keeps per-user data SHOULD key it on `USER_KEY`, not on the alias or number, and falls back to `USER_NUMBER` when `USER_KEY` is missing. Its characters are safe in file names on DOS, Windows and POSIX, but the value as a whole may not be: it could be a Windows device name such as `CON` or `COM1`, with or without an extension. A door that uses it in a path puts a constant in front of it, such as `u-`, which rules those out, or hashes it; a DOS door that needs an 8.3 name hashes it in any case.

A door with translations matches `USER_LANG` against the tags it has by BCP 47 lookup [RFC4647]: it compares tags case-insensitively, and when there is no exact match it drops subtags from the end and tries again, so `de-DE` or `de-AT` finds a `de` translation. If nothing matches, it uses its default language.

Behind a proxy or web gateway, `USER_IP` is the address the host trusts as the caller's: the address the gateway forwarded, when the host is configured to trust that gateway, and otherwise the gateway's own.

There is no standard security level key. A level's range, its ordering (whether higher means more access) and even whether it is numeric differ between BBS packages, so a standard key whose meaning depends on the host that wrote it would mislead doors. A host MAY write its levels in vendor keys, such as Synchronet's `X_SBBS_LEVEL`, and a door that needs access control can use `USER_ROLE`.

### Terminal: `[terminal]`

`TERM_` keys describe the caller's terminal: what it is and what it can do. The host may have detected a value or taken it from the user's manual terminal settings, for example when the user has turned off automatic terminal detection. `PREF_` keys (next section) are the user's choices about how doors should behave, whatever the terminal can do. All keys in this section are optional. A missing `TERM_` key reads as its default. In the capability table below, the default `0` means the host didn't detect the capability, not that the terminal lacks it, so a door MAY query the terminal itself before relying on it. Producers SHOULD always write `TERM_TYPE`, since a missing one means `dumb`.

| Key | Type | Meaning | Default |
| --- | --- | --- | --- |
| `TERM_COLS` | int | Width in character cells | `80` |
| `TERM_ROWS` | int | Usable height in character cells, excluding any status line the host adds | `24` |
| `TERM_TYPE` | token | The kind of terminal: `dumb` (plain text, no cursor control), `ansi` (ANSI escape sequences), `avatar` (AVATAR/0+ [FSC-0025], which includes ANSI, so a door without AVATAR output treats it as `ansi`) or `petscii` (Commodore PETSCII control codes). A RIPscrip terminal is `ansi`, with `TERM_RIP` set | `dumb` |
| `TERM_RIP` | ascii | RIPscrip version the terminal reported, as `<major>.<minor>`, such as `1.54` from a `RIPSCRIP015400` reply to `CSI !`; or `unknown` when the terminal supports RIPscrip but reported no version, such as when the user set RIP manually. Written only when `TERM_TYPE` is `ansi`; missing means no RIPscrip | none |
| `TERM_CHARSET` | ascii | The caller's terminal character set, from the names listed under `COMM_CHARSET`. It equals `COMM_CHARSET` when the host passes the door's bytes through untranslated, and may differ when the host translates them | unknown |
| `TERM_MONO` | bool | `1` = no color, because the terminal can't show it or the user turned it off: the door may send ANSI sequences but no color changes, whatever `TERM_COLORS` says | `0` |
| `TERM_BACKSPACE` | int | The byte the terminal's Backspace key sends: `8` (BS) or `127` (DEL). A door treats the other of the two as Delete, as it does `CSI 3 ~`. A host may learn it from the user's settings, or from a DECRQM query for mode 67 (DECBKM) on a terminal that answers one | `8` |
| `TERM_NAME` | text | The terminal program's name as the program itself reported it, from a query that asks a terminal to identify itself, such as XTVERSION (`CSI > 0 q`) [XTERM] or SyncTERM's `APC SyncTERM:VER`; not the terminfo name, which is `TERM_TERMINFO` | unknown |
| `TERM_TERMINFO` | ascii | The terminal type name the caller's client sent, from Telnet TERMINAL-TYPE [RFC1091], the SSH pty request [RFC4254] or the RLogin connection, such as `xterm-256color` or `ansi`, for doors that use curses or terminfo. The host passes it on as received, without checking it; a door that doesn't find it in its terminfo database uses one that matches `TERM_TYPE` | unknown |
| `TERM_CTERM` | ascii | CTerm revision from `CSI c`, with the reply's `;` separators turned into dots: `<major>.<minor>`, such as `1.332`, or `<major>.<minor>.<fork>` from a forked CTerm, such as `1.332.4` (see below) | not CTerm |

`TERM_COLS` and `TERM_ROWS` are the size when the host wrote the file. The host, not the door, receives a later Telnet NAWS or SSH window-change message, so a door on a `socket`, `stdio` or other connection hears nothing of a resize during the session. A door that cares queries the terminal itself, such as with `CSI 18 t` [XTERM] or a cursor position report after moving the cursor to row 999, column 999.

A forked CTerm adds its own revision as a third field and leaves the first two as the CTerm revision it was forked from. A door checking for a CTerm feature compares only `<major>.<minor>`; the third field means something only to a door that knows that fork. Each field is a decimal number, so compare them numerically, not as text: `1.40` is older than `1.332`.

Capabilities the host detected. The source column names the query each comes from; several are CTerm-only (SyncTERM and its forks). The device attributes are the reply to `CSI c`, and the CTerm device attributes [CTERM] the reply to `CSI < c`; the source column refers to the numbered values in those replies. All are optional: a missing bool reads as `0`, a missing `TERM_COLORS` as `16`, and any other missing int or token as unknown, and either way it means the host didn't detect the capability.

| Key | Type | Meaning | Source |
| --- | --- | --- | --- |
| `TERM_FONTS_LOADABLE` | bool | Fonts can be loaded with device control strings | CTerm device attributes, 1 |
| `TERM_BRIGHT_BG` | bool | Bright background colors (iCE color) | CTerm device attributes, 2 |
| `TERM_PALETTE` | bool | Palette entries can be changed | CTerm device attributes, 3 |
| `TERM_FONT_SELECT` | bool | The current font can be selected | CTerm device attributes, 5 |
| `TERM_PALETTE_EXT` | bool | Extended palette | CTerm device attributes, 6 |
| `TERM_COLORS` | int | Number of colors the terminal can show: `16`, `256` or `16777216` (24-bit). Missing means `16`, the CGA palette every ANSI terminal has. It applies only to an `ansi` terminal without `TERM_MONO=1`; otherwise the door sends no color, whatever this key says | Terminal probe, such as a DECRQSS request for the SGR state after setting a 256-color or 24-bit color, or the user's terminal settings |
| `TERM_MOUSE` | bool | Mouse reporting: `1` = the terminal reports mouse events in character cells, `2` = it can also report them in pixels (SGR-Pixels, mode 1016). This is what the terminal can do, not what the user wants; see `PREF_MOUSE` | CTerm device attributes, 7; on other terminals, DECRQM requests for modes 1006 and 1016 (`CSI ? 1006 $ p`, `CSI ? 1016 $ p`), which report whether a mode is recognized without changing it; or the user's terminal settings |
| `TERM_SIXEL` | bool | Sixel graphics | Device attributes, 4; for CTerm, whose device attributes reply carries its revision instead, CTerm device attributes, 4 |
| `TERM_SIXEL_SCALE` | token | How the terminal applies the pixel aspect in a sixel's raster attributes (`"pan;pad`): `none` = draws at the encoded size, `vertical` = honors `pan` only (the DEC pixel aspect), `both` = honors `pan` and `pad` as integer scales (a CTerm extension) | Measured (see below) |
| `TERM_PPM` | bool | PPM images through SyncTERM's APC commands | CTerm device attributes, 4, with CTerm revision 1.316 or later |
| `TERM_KEYS_EVDEV` | bool | Physical key press and release reports (`CSI = 1 h`), as layout-independent evdev key codes | CTerm device attributes, 8 |
| `TERM_KEYS_KITTY` | bool | Kitty keyboard protocol: press, repeat and release events by codepoint | Reply to `CSI ? u` [KITTY-KEYS] |
| `TERM_DOORWAY` | bool | DoorWay mode (`CSI = 255 h`) | No query exists: the terminal's identity (every CTerm supports it) or a user setting |
| `TERM_JXL` | bool | JPEG XL images | `APC SyncTERM:Q;JXL` |
| `TERM_SNDFILE` | bool | Audio files can be played (libsndfile present); doors query specific formats with `APC SyncTERM:Q;libsndfileFormat` | `APC SyncTERM:Q;libsndfile` |
| `TERM_STATUS_CONTROL` | bool | The terminal's status line can be hidden or given to the host | Status display type (`DECSSDT`) request |
| `TERM_PIXEL_COLS` | int | Width of the sixel graphics area in pixels, which isn't necessarily the character area | `CSI ? 2 ; 1 S` (XTSMGRAPHICS) [XTERM] |
| `TERM_PIXEL_ROWS` | int | Height of the sixel graphics area in pixels | `CSI ? 2 ; 1 S` (XTSMGRAPHICS) |
| `TERM_CELL_WIDTH` | int | Character cell width in pixels | `CSI = 3 n` |
| `TERM_CELL_HEIGHT` | int | Character cell height in pixels | `CSI = 3 n` |

`TERM_SIXEL_SCALE` can't be inferred from the terminal's name or identity, so a host writes it only after measuring it: it draws the same small sixel with `pan` set to 1 and then to 2, requests a cursor position report after each, and compares how far each moved the cursor, a technique from [VT340TEST]. A missing key means it wasn't measured. A door that needs it can run the same probe, or encode sixels at 1:1, which displays correctly on every terminal.

Without `TERM_PIXEL_COLS` and `TERM_PIXEL_ROWS`, a door should keep sixel images within 1000 by 1000 pixels: xterm doesn't answer `CSI ? 2 ; 1 S` by default and discards a larger sixel entirely.

Other CTerm features, such as SyncTERM's APC image zoom and which DECSDM (mode 80) setting draws a sixel at the cursor, depend only on the CTerm revision, so a door gets them from `TERM_CTERM`.

### Session: `[session]`

All keys in this section are optional.

| Key | Type | Meaning | Default |
| --- | --- | --- | --- |
| `TIME_LEFT` | int | Seconds the user has left, measured when the host writes the file and already shortened for any scheduled host event, so a door needs no separate event time | no limit |
| `TEMP_DIR` | path | A directory only this node uses, which the door may write to during the session, in the path syntax the door sees (the DOS path under emulation), with no trailing separator. The host MAY empty it after the door exits, so it isn't for data that must last | none |
| `LOCAL_DISPLAY` | bool | `0` = don't show the session on the host's own screen: the door doesn't mirror its output to a local console or window. Doesn't apply when `COMM_TYPE` is `local`, where the local console is the session itself | `1` |
| `IDLE_LIMIT` | int | Seconds without input from the caller after which the host ends the session. A door uses it to warn the user before that happens, or to run its own idle timer when it exchanges terminal queries that the host may count as input | no limit |

A door MUST exit before `TIME_LEFT` seconds have passed since it started. The time between the host writing the file and the door starting, such as an emulator booting, isn't counted, so the host SHOULD enforce the limit independently. A door that counts time in minutes rounds up, so a positive `TIME_LEFT` never becomes zero minutes, which some door kits treat as no time left or as no limit.

### Door: `[door]`

The configured entry the host launched. Both keys are optional. They matter to a program installed under more than one entry, such as a game engine with one entry per title, which otherwise has no way to learn which entry started it.

| Key | Type | Meaning | Default |
| --- | --- | --- | --- |
| `DOOR_CODE` | ascii | The host's identifier for this entry, as the sysop configured it, such as `SYNCRPG`; unique among the host's entries | none |
| `DOOR_NAME` | text | The entry's display name, as the sysop configured it, such as `Legend of the Red Dragon` | none |

### User preferences: `[preferences]`

All keys in this section are optional.

| Key | Type | Meaning | Default |
| --- | --- | --- | --- |
| `PREF_SOUND` | bool | `0` = the user has muted sound: the door MUST NOT send audio, such as ANSI music, CTerm audio or BEL (Ctrl-G) characters | `1` |
| `PREF_MOUSE` | bool | `0` = the user has turned mouse use off: the door MUST NOT enable mouse reporting for hot-spots, clickable menus, scrolling or other uses that stand in for keys. A door whose play is pointer-driven, such as steering or aiming, MAY still enable it, and then gives the user its own way to turn it off | `1` |
| `PREF_PAUSE` | bool | `0` = the user has turned off screen pausing | `1` |
| `PREF_EXPERT` | bool | `1` = the user prefers short prompts without menus | `0` |
| `PREF_PAGEABLE` | bool | `0` = the user doesn't want pages, chat requests or messages from other nodes | `1` |
| `PREF_ALERTS` | bool | `0` = the user doesn't want notices of other users' activity, such as another user entering the door | `1` |
| `PREF_QUIET` | bool | `1` = don't announce this user's activity, such as entering or leaving the door, to other users | `0` |

A door that honors a preference SHOULD NOT also ask the user for it, so the user sets it once on the host for all doors.

## Extensions and versioning

Anyone can add keys without a spec change by using a vendor prefix, and standard keys are added to this spec without changing the format, so the file has no version key.

- **Vendor keys** have the form `X_<VENDOR>_<NAME>` and go in a section named `[x-<vendor>]`, such as `X_SBBS_LEVEL` in `[x-sbbs]`. `<VENDOR>` is a short name for the product that defines the key, in uppercase ASCII letters and digits only, with no `_`, so the first `_` after `X_` ends it. The section name is `x-` followed by the vendor name in lowercase. The product documents its keys. A host names its own vendor in `SYS_VENDOR`. A list of vendor names can be kept with this spec to avoid clashes, but using a prefix doesn't require listing it.
- **Key names are unique across all sections.** A new standard key never reuses a name from another section, and a vendor key is kept unique by its `X_<VENDOR>_` prefix.
- **New standard keys** are added to this spec with a stated section, type and default. Because readers ignore unknown keys and treat missing ones as their default, adding a key doesn't break existing doors.
- **A key's meaning never changes.** A change of meaning or type needs a new key name. A producer MAY write both the old and new keys during a transition.
- **A bool key can gain values above `1`** without a new name, under the rule in Value types: `0` and `1` keep their meanings, and each new value means true plus something more.
- **A vendor key can become standard** under a new unprefixed name. Producers MAY write both names until doors move to the standard one.
- **An incompatible change to the file format itself,** such as adding quoting, would use a new environment variable and a new customary file name, so an existing door never reads a file it can't parse.

## Security and privacy

The file carries no secrets and grants no privileges, and personal details beyond the alias are the sysop's choice.

- Producers MUST NOT write passwords, authentication tokens or other secrets.
- The file is only as trustworthy as whoever can write it, since `USER_KEY` and `USER_ROLE` decide whose saved game loads and who gets sysop functions inside the door. A host SHOULD make the file writable only by itself. A door that can also be started outside a host, such as directly over SSH or by a door server, and takes the path on its command line is trusting whoever gave it that path as the host. A door that opens the path it was given can't pick up a stale file from another session; one that looks for the file by name in a directory can, which is another reason to prefer the path.
- `USER_ROLE=sysop` tells the door who the sysop is. It is not authentication and grants no operating-system privileges.
- `USER_REALNAME`, `USER_LOCATION`, `USER_BIRTHDATE`, `USER_GENDER`, `USER_IP`, `USER_HOSTNAME`, `USER_CALLER_ID`, `USER_EMAIL` and `USER_NETMAIL` are optional, so a producer MAY leave any of them out, for example because the sysop chose not to share it. A host SHOULD let the sysop decide per door whether it writes them, since a door may be closed source or send what it reads to other systems. A door MUST NOT treat the last three as authentication.
- Text values come from users. A door MUST NOT pass them to a shell or use them as a format string, and MUST handle non-ASCII characters and 255-byte lines.
- The file is read-only input. A door MUST NOT use it to return changes to the host.

## Examples and minimal readers

A native door on a socket, with a muted user, CTerm capabilities and one vendor key:

```ini
; Written by Synchronet 3.22a
[file]
FILE_UTF8=1
FILE_TIME=2026-09-23T15:15:00-07:00

[system]
SYS_SOFTWARE=Synchronet 3.22a
SYS_VENDOR=SBBS
SYS_VERSION=3.22a
SYS_NAME=Example host
SYS_OP=Example Sysop
SYS_NODE_NUM=3
SYS_NODE_COUNT=8
SYS_QWKID=EXAMPLE
SYS_HOSTNAME=bbs.example.com

[comm]
COMM_TYPE=socket
COMM_HANDLE=1234
COMM_CHARSET=UTF-8

[user]
USER_ALIAS=Jörg the Red
USER_NUMBER=42
USER_KEY=42-1693526400
USER_ROLE=user
USER_LANG=de
USER_PROTOCOL=ssh

[terminal]
TERM_COLS=132
TERM_ROWS=37
TERM_TYPE=ansi
TERM_NAME=SyncTERM
TERM_CTERM=1.332
TERM_FONTS_LOADABLE=1
TERM_BRIGHT_BG=1
TERM_PALETTE=1
TERM_SIXEL=1
TERM_SIXEL_SCALE=both
TERM_PPM=1
TERM_MOUSE=2
TERM_KEYS_EVDEV=1
TERM_DOORWAY=1
TERM_SNDFILE=1
TERM_PIXEL_COLS=1056
TERM_PIXEL_ROWS=592
TERM_CELL_WIDTH=8
TERM_CELL_HEIGHT=16

[session]
TIME_LEFT=2700
IDLE_LIMIT=300

[door]
DOOR_CODE=SYNCRPG
DOOR_NAME=SyncRPG

[preferences]
PREF_SOUND=0

[x-sbbs]
X_SBBS_LEVEL=50
X_SBBS_SECTION=GAMES
```

A DOS door on a FOSSIL driver, with only the required keys plus two optional ones; the screen size is the default 80 by 24:

```ini
[system]
SYS_SOFTWARE=Example BBS 2.1
SYS_NAME=Retro Board
SYS_OP=Sysop
SYS_NODE_NUM=1

[comm]
COMM_TYPE=fossil
COMM_PORT=1
COMM_CHARSET=CP437

[user]
USER_ALIAS=Guest
USER_NUMBER=7

[terminal]
TERM_TYPE=ansi

[session]
TIME_LEFT=1800
```

Each reader below returns one key's value. It opens the file named by `DOORDROP_INI` (`getenv()`, `GetEnv()` or `ENVIRON$()`). A real door would read the file once into a table.

C (C89, builds with Turbo C through current compilers):

```c
#include <stdio.h>
#include <string.h>

/* Copy the value of key into val; return 1 if found, 0 if not */
int doordrop_get(const char* path, const char* key, char* val, size_t size)
{
	char line[260];
	FILE* fp;
	int found = 0;

	if (size == 0 || (fp = fopen(path, "r")) == NULL)
		return 0;
	while (!found && fgets(line, sizeof line, fp) != NULL) {
		char* eq;
		line[strcspn(line, "\r\n")] = '\0';
		eq = strchr(line, '=');
		if (eq == NULL)
			continue;
		*eq = '\0';
		if (strcmp(line, key) != 0)
			continue;
		strncpy(val, eq + 1, size - 1);
		val[size - 1] = '\0';
		found = 1;
	}
	fclose(fp);
	return found;
}
```

Turbo Pascal 7:

```pascal
function DropGet(const Path, Key: string; var Value: string): Boolean;
var
  F: Text;
  S: string;
  P: Integer;
  Found: Boolean;
begin
  Found := False;
  Assign(F, Path);
  {$I-} Reset(F); {$I+}
  if IOResult = 0 then
  begin
    while not Found and not Eof(F) do
    begin
      ReadLn(F, S);
      P := Pos('=', S);
      if (P > 1) and (Copy(S, 1, P - 1) = Key) then
      begin
        Value := Copy(S, P + 1, 255);
        Found := True;
      end;
    end;
    Close(F);
  end;
  DropGet := Found;
end;
```

QBasic (returns an empty string for a missing key; the file must exist, because `OPEN` on a missing file stops the program with a runtime error unless an `ON ERROR GOTO` handler is active, and that handler's label must be in module-level code, so a door first opens the file once at module level under such a handler):

```basic
FUNCTION DropGet$ (path$, key$)
  DropGet$ = ""
  f = FREEFILE
  OPEN path$ FOR INPUT AS #f
  DO WHILE NOT EOF(f)
    LINE INPUT #f, s$
    p = INSTR(s$, "=")
    IF p > 1 THEN
      IF LEFT$(s$, p - 1) = key$ THEN
        DropGet$ = MID$(s$, p + 1)
        EXIT DO
      END IF
    END IF
  LOOP
  CLOSE #f
END FUNCTION
```

None of the three handles comments or section headers: neither can match a key, because keys never start with `;` or `[`, and headers contain no `=`. The three guarantee only the CRLF line ending: the C reader also accepts LF alone, while the Pascal and QBasic readers depend on their runtime's `ReadLn` and `LINE INPUT`. In the C reader, a line longer than its buffer, which only a nonconforming file can contain, is read in pieces, and a later piece could be mistaken for a key.

A native door reads the file as bytes and decodes text values only after reading `FILE_UTF8`: a reader that decodes the whole file as UTF-8 fails on a CP437 value.

A native door can use its platform's INI API instead, within these limits:

- **Win32 `GetPrivateProfileString()`** removes a pair of quotes around a whole value, so the alias `"Joe"` comes back as `Joe`. It also reads a file with no byte-order mark in the system ANSI code page, so it doesn't decode UTF-8 text values. A Windows door that needs text values exactly reads them with a simple reader like the ones above.
- **Python `configparser`** expands `%` in values by default, and fails on an alias such as `100%Joe`. Create it with `configparser.ConfigParser(interpolation=None)` and read the file as UTF-8 or CP437 according to `FILE_UTF8` (Python codecs `utf-8` and `cp437`).

## Coverage of older drop files

This section isn't part of the specification. It compares the keys above with the fields of the drop files this format is meant to replace, so a converter from one of them, or a door moving from one, can see what has no key and why. The formats checked are the ones Synchronet writes: DOOR.SYS [DOOR.SYS], DOOR32.SYS [DOOR32], DORINFO1.DEF, WWIV's CHAIN.TXT, Wildcat's CALLINFO.BBS, Spitfire's SFDOORS.DAT, TriBBS's TRIBBS.SYS, PCBOARD.SYS, Solar Realms' DOORFILE.SR, BBSDEV.DRP [BBSDEV.DRP] and Synchronet's own XTRN.DAT, plus Phenom Dropfile [PHENOM], a Mystic BBS script that writes the same 13 values as JSON and as one value per line.

Everything DOOR32.SYS, DORINFO1.DEF, DOORFILE.SR, BBSDEV.DRP and Phenom Dropfile carry has a key, except as noted below for the password, security level, AVATAR, host operating system and host directories. Phenom's two capability flags, loadable fonts and extended palette, are `TERM_FONTS_LOADABLE` and `TERM_PALETTE_EXT`; Phenom infers them from the terminal's Telnet type name (`SYNCTERM`), where this file carries what the host detected. The fields of the other formats that have no key:

| Field | In | Disposition |
| --- | --- | --- |
| Password | DOOR.SYS 14, CALLINFO.BBS, SFDOORS.DAT, TRIBBS.SYS | Left out on purpose (see Security and privacy). A converter writes a blank or placeholder |
| Security level | every format | Left out on purpose (see User); a host writes it as a vendor key, and a converter to an older format takes it from there |
| AVATAR terminal | DORINFO1.DEF graphics `2` | Added in draft 0.5 as `TERM_TYPE=avatar`, for converters; no host writes it yet |
| Home and work phone numbers | DOOR.SYS 12 and 13, CALLINFO.BBS, SFDOORS.DAT, TRIBBS.SYS, XTRN.DAT | No key. Proposed: optional `USER_PHONE` (text), personal like `USER_CALLER_ID` |
| Total logons | DOOR.SYS 16, CALLINFO.BBS | No key. Proposed: optional `USER_LOGONS` (int) |
| Last call date and time | DOOR.SYS 17 and 45, CHAIN.TXT, CALLINFO.BBS | No key. Proposed: optional `USER_LAST_ON` (date) |
| Upload and download totals, files and kilobytes | DOOR.SYS 28 to 31 and 46 to 49, CHAIN.TXT, CALLINFO.BBS, SFDOORS.DAT | No key. Proposed, if file-area doors want them: `USER_UPLOADS`, `USER_DOWNLOADS` (int) and `USER_UPLOAD_BYTES`, `USER_DOWNLOAD_BYTES` (uint64); the daily limits stay host-specific |
| Time used this call, logon time | DOOR.SYS 44, CHAIN.TXT, SFDOORS.DAT, PCBOARD.SYS | No key. Proposed: optional `TIME_USED` (int, seconds), for doors that show it |
| Credits, gold, time credits | XTRN.DAT, CHAIN.TXT, DOOR.SYS 42 | Vendor keys (Synchronet's `X_SBBS_CREDITS`, `X_SBBS_MINUTES`); no common meaning |
| Expiration date, flags, exemptions, restrictions | XTRN.DAT, DOOR.SYS 25, TRIBBS.SYS | Vendor keys, as the security level |
| Default transfer protocol | DOOR.SYS 27, CALLINFO.BBS | Out of scope: a letter whose meaning is the host's |
| Conferences, current conference or file area, highest message read, file new-scan date | DOOR.SYS 23, 24 and 43, CALLINFO.BBS, SFDOORS.DAT | Out of scope: host-specific identifiers |
| Host directories: MAIN and GEN, gfiles, data, ctrl, log file, BBS root | DOOR.SYS 33 and 34, CHAIN.TXT, XTRN.DAT, Phenom `bbsdir` | Out of scope; a door's own data directories are an open question |
| Host operating system | Phenom `ostype` | Out of scope: a door's own binary or runtime knows what it runs on |
| Serial line details: data bits, parity, error correction, locked rate, flow control, modem strings | DOOR.SYS 3 and 38, CHAIN.TXT, SFDOORS.DAT, TRIBBS.SYS, XTRN.DAT | Out of scope: a `serial`, `fossil` or `uart` port is handed over configured |
| Printer toggle, page bell, caller alarm, default color, record locking | DOOR.SYS 7 to 9, 40 and 41, PCBOARD.SYS | Out of scope: the host's local settings |
| User age | CHAIN.TXT | Derived from `USER_BIRTHDATE` |
| Call sign | CHAIN.TXT | `USER_HANDLE` |
| User comment, doors opened, message left | DOOR.SYS 50 to 52 | Out of scope |
| Street address and postal code | XTRN.DAT | Left out: more personal than `USER_LOCATION`, and no door is known to use them |
| Door number | CALLINFO.BBS | `DOOR_CODE` |
| Guru name, network type, sysop next, from front-end | XTRN.DAT, DORINFO1.DEF, SFDOORS.DAT | Out of scope: host-specific |

A converter to an older format also needs values this file never carries, such as a nonzero rate (see `COMM_RATE`), and writes the older format's placeholders for them.

DTS-0001 [DTS-0001], the 1992 named-value proposal, overlaps this spec closely: its `BOARD`, `SOFTWARE`, `NETADDRESS`, `SYSOP`, `SYSOPAKA`, `PORT`, `PORTADDR`, `PORTIRQ`, `USER`, `USERAKA`, `ORIGIN`, `WIDTH`, `HEIGHT`, `MODES`, `BIRTHDAY` and `TIMELEFT` all have keys here, with `NEXTEVENT` folded into `TIME_LEFT`. Its settings without a key fall into the rows above (password, security level and `MAXSECURITY`, phone numbers, calls, last and current call times, protocol, transfer totals and limit, serial line details) or are its own: `TIMEBANKED`, `TIMEPERMSG`, `CHATSLEFT` and `PAGESLEFT` are host policy, better left to vendor keys; `REQFILE` and `REQFILES` are file requests from the user to the door; and `MSGAREA`, `REPLYINGTO`, `REPLYTOPIC`, `QUOTEFILE` and `REPLYFILE` serve a message editor, which this spec has no keys for (an open question).

## Synchronet implementation notes

This section isn't part of the specification. It records how Synchronet would write the file, as one worked example; authors of other BBS software can skip it. It uses Synchronet's own terms: SCFG is Synchronet's configuration program, the `XTRN_*` names are per-door option flags set there, and file names such as `xtrn_sec.cpp` are Synchronet source files.

DOORDROP.INI would be one more drop file type that the sysop selects in SCFG for each door that reads it, like DOOR.SYS or DOOR32.SYS. Synchronet writes it only for those doors.

- **Type:** a new `XTRN_INI` value appended to the drop file type enum in `sbbsdefs.h`, offered in SCFG alongside DOOR.SYS, DOOR32.SYS and the others, and given its own case in the drop file name switch in `xtrn_sec.cpp` so the file is named `DOORDROP.INI`, not the `XTRN.DAT` default or anything like `ctrl/xtrn.ini`, so existing `xtrn.ini` values keep their meaning and no migration is needed. It is offered for message editors too, since `writemsg.cpp` also calls `xtrndat()`.
- **Where:** a new block in `sbbs_t::xtrndat()` (`src/sbbs3/xtrn_sec.cpp`), written to the directory the door's drop file location option names: the node directory, the node's temp directory (`XTRN_TEMP_DIR`) or the door's start-up directory (`XTRN_STARTUPDIR`). The node and temp directories are both per-node, so they're always safe. The start-up directory is shared by every node running the door, so Synchronet uses it only when one node at a time can run the door: the door's "Supports Multiple Users" option (`XTRN_MULTIUSER`) is off, or the system has one node. For a multiuser door on a multinode system, the start-up directory option is ignored and the file goes in the node directory. In every case, the door finds the file through `DOORDROP_INI` or `%F`. The name is `DOORDROP.INI`, or `doordrop.ini` when the door's lowercase file name option is set, and the path must be absolute.
- **Environment:** `xtrn.cpp`, which launches doors, sets `DOORDROP_INI` on every door launch path: native programs, DOS programs run under DOSEMU on Linux, and DOS programs run under emulation on Windows.
- **Command line:** a door's command line in SCFG can contain `%` placeholders that Synchronet replaces at launch. `%F` becomes the full path of the door's drop file, and `%f` the same path in quotes. For a DOS door run under emulation, it is the DOS path the door sees. Both already exist, so they give the DOORDROP.INI path with no change.
- **Doors that use Windows console interception** (`XTRN_CONIO`), where Synchronet relays a door's Windows console to the caller, can't use this type: the door would see its local console while a caller is connected, which `local` doesn't allow. Standard-I/O doors use `stdio`.
- **Standard-I/O echo:** `stdio` requires that the host echo nothing, but Synchronet echoes a standard-I/O door's input back to the caller unless the door's no-echo option (`XTRN_NOECHO`) is set. For a door of this drop file type, Synchronet treats that option as set.
- **Doors written in JavaScript** that Synchronet runs inside its own process can't use this type; they already have the `user`, `console` and `system` objects.
- **Mapping from Synchronet data:**
  - `SYS_VENDOR` = `SBBS`, and `SYS_VERSION` = Synchronet's version number followed by its revision letter, such as `3.22a`.
  - `SYS_NODE_COUNT` = the configured number of nodes, and `SYS_QWKID` = the system's QWK ID.
  - `SYS_HOSTNAME` = the system's Internet host name (`system.host_name`), `SYS_LOCATION` = its configured location, `SYS_FTN_ADDR` = all of its FidoNet addresses, primary first, without `@domain`, since the domain names live in SBBSecho's configuration rather than the main one, `SYS_OP_AVAILABLE` from whether the sysop is set as available for chat, and `SYS_DATE_FORMAT` from the configured date format (`MMDDYY`, `DDMMYY` or `YYMMDD` as `mdy`, `dmy` or `ymd`).
  - `USER_PROTOCOL` from the client's protocol (`Telnet`, `RLogin` or `SSH`, in lowercase), or `serial` for a dial-up call through SEXPOTS. A caller reaching the Terminal Server through the WebSocket service arrives as `telnet`.
  - `USER_EMAIL` = the user's Internet address on the system (`user.email`), `USER_NETMAIL` = the user's forwarding address (`user.netmail`), and `USER_ROLE=guest` for a guest account (`user.is_guest`).
  - `USER_KEY` = `<number>-<firston>`: the user number and the account's creation time (as a Unix time), which together are never reused.
  - `USER_ROLE` = `sysop` when the user has sysop access; otherwise `user`.
  - `X_SBBS_LEVEL` = the user's security level; see Vendor keys and MODUSER.DAT below for the other `X_SBBS_` keys.
  - `FILE_UTF8` and `COMM_CHARSET` as described under Encodings below, and `TERM_CHARSET` from the user's terminal character set, the same value Synchronet writes as `chars` in the node's `terminal.ini`, with `PETSCII` in place of its `CBM-ASCII`.
  - `TERM_TERMINFO` from the terminal type the client sent, the same string Synchronet logs at logon from Telnet TERMINAL-TYPE, the SSH pty request or RLogin; left out when the client sent none. `TERM_NAME` needs a query Synchronet doesn't send yet (a to-do below).
  - `TERM_CTERM` from the CTerm revision Synchronet detected, including a fork's third field (`console.cterm_fork`, #1250).
  - `TERM_TYPE` from the user's terminal type, the same value Synchronet writes as `type` in the node's `terminal.ini`, in lowercase and with `RIP` written as `ansi`; never `avatar`, which Synchronet doesn't detect or support; `TERM_RIP` when that type is `RIP`, with the version from the logon detection reply, or `unknown` when RIP was set manually; and `TERM_BRIGHT_BG` from `ICE_COLOR`, whether auto-detected or set manually, and `TERM_MONO=1` when the settings have `ANSI` without `COLOR` (XTRN.DAT's `Mono`).
  - `TERM_DOORWAY` when the terminal identified itself as CTerm.
  - `TERM_SIXEL` for CTerm from its device attribute 4, as Synchronet's JavaScript library `cterm_lib.js` already does. Synchronet sends `CSI c` at logon but parses only CTerm's reply, so other terminals get the key only after a to-do below.
  - `TERM_MOUSE` from the mouse capability detected for the session: `2` when the terminal answered the DECRQM request for mode 1016, or is a CTerm recent enough to report pixels; `1` when it answered for mode 1006 or set CTerm device attribute 7, or when detection was silent and the user's mouse setting is on; otherwise left out. `PREF_MOUSE=0` when the user's mouse setting (`MOUSE`) is off. Today that setting is Synchronet's only record of mouse support, answering "Does your terminal support mouse reporting"; the to-do below separates capability from consent.
  - `TIME_LEFT` from the user's remaining time in this session, which Synchronet already shortens for an upcoming timed event, plus the door's extra time, and capped at the door's maximum time; omitted for the sysop, who has no time limit. For a door with free time (`XTRN_FREETIME`), where the user's clock stops while in it, the value is the lesser of the door's maximum time and the time until the next timed event, and is omitted when neither applies, since the user's remaining time would make the door exit when Synchronet wouldn't have.
  - `IDLE_LIMIT` from the door's maximum inactivity setting when nonzero, otherwise the Terminal Server's; omitted when both are zero.
  - `DOOR_CODE` from the program's internal code, `DOOR_NAME` from its configured name, and `X_SBBS_SECTION` from the internal code of the section it belongs to.
  - `USER_IP` and `USER_HOSTNAME` from the client's address and host name, and `USER_CALLER_ID` from the Caller ID number SEXPOTS passes for a dial-up call. For a dial-up call Synchronet stores that number in the client's address field in place of an IP address, so the writer puts it in `USER_CALLER_ID` and leaves `USER_IP` out. A host name Synchronet couldn't resolve is left out too.
  - `USER_LANG` from the user's language code, which names the `ctrl/text.<code>.ini` file the user's text comes from. The stock codes (`de`, `es`, `fr`) are already valid tags. The writer changes any `_` to `-`, so a code such as `pt_BR` becomes `pt-BR`, then checks the result against BCP 47 syntax, and leaves the key out when the code is blank (the default language, which Synchronet doesn't record) or still isn't a valid tag. Sysops SHOULD name language files with ISO 639-1 codes, since a well-formed code that isn't a real language, such as `sp`, can't be detected.
  - `LOCAL_DISPLAY=0` when the door's "Disable Local Screen Display" option (`XTRN_NODISPLAY`) is set, the same option that sets the screen field in DOOR.SYS and PCBOARD.SYS.
  - `COMM_TYPE=local` only when the door runs as a timed event, with no caller; never for a caller's session, since Synchronet has no local logon.
  - `FILE_TIME` = the local time the file is written, with the system's UTC offset.
  - `TEMP_DIR` = the node's temp directory, without its trailing separator. Synchronet already empties it at logoff and before other work that uses it, such as QWK packets, batch file transfers and launching a door configured to get its drop files there.
  - `PREF_PAGEABLE=0` when the user has turned paging off on the node, the setting PCBOARD.SYS reports as the node's chat status.
  - `PREF_SOUND=0` when the user's "no sound" setting (`NO_SOUND`) is on, which Synchronet already defines as suppressing both BEL and terminal audio.
  - `USER_HANDLE` from the user's chat handle.
  - `PREF_ALERTS=0` when activity alerts are off (`CHAT_NOACT`), and `PREF_QUIET=1` in quiet mode (`QUIET`).
  - `TERM_BACKSPACE=127` when the user's terminal settings swap Delete and Backspace (`SWAP_DELETE`); otherwise it's left out, meaning `8`.
  - The optional personal keys are written when the user's record has them, as Synchronet already does for the same details in DOOR.SYS and other drop files, unless a new per-door option (a to-do below) withholds them.
- **Text lengths:** Synchronet's limits, in bytes as stored. A CP437 string holds that many characters. A string stored as UTF-8 may hold fewer, and a CP437 string written with `FILE_UTF8=1` can take up to 3 bytes per character.

  | Key | Synchronet limit |
  | --- | --- |
  | `USER_ALIAS` | 25 |
  | `USER_REALNAME` | 25 |
  | `USER_LOCATION` | 30 |
  | `USER_GENDER` | 1 (a single character, `M` or `F` by default; the sysop can configure others) |
  | `SYS_NAME` | 40 |
  | `SYS_OP` | 40 |
  | `USER_HANDLE` | 8 |

- **Handles:** a `socket` door gets one end of a loopback TCP connection that Synchronet bridges to the caller, whatever the caller's protocol. After the door exits, Synchronet already sets the socket back to blocking mode and re-applies its socket options (`main.cpp`), which covers the host side of the handle rules.
- **Terminal capability queries** need a round trip each, and an unanswered one can stall for up to 3 seconds. Synchronet would run them once per session, before the first door launch, and cache the results, as `exec/load/cterm_lib.js` already does for the CTerm device attributes. This adds the kitty keyboard query (`CSI ? u`), the DECRQM mouse requests for `TERM_MOUSE` and, optionally, the `TERM_SIXEL_SCALE` probe, which paints two small slivers on screen that the door's first screen covers. Capabilities that weren't detected are left out.

### Vendor keys and MODUSER.DAT

Synchronet can apply changes a door makes to the user's account: when a door's "Modify User Data" option (`XTRN_MODUSERDAT`) is set, Synchronet reads a MODUSER.DAT file the door leaves in the drop file directory, whatever the drop file type. The door can't change DOORDROP.INI itself, so these `[x-sbbs]` keys give it the current values MODUSER.DAT adjusts:

| Key | Type | Meaning |
| --- | --- | --- |
| `X_SBBS_LEVEL` | int | Security level, 0 through 99 |
| `X_SBBS_FLAGS1` through `X_SBBS_FLAGS4` | ascii | The letters `A` through `Z` set in each flag set, such as `ABX`; left out when none are set |
| `X_SBBS_EXEMPT` | ascii | Exemption flags, in the same form |
| `X_SBBS_REST` | ascii | Restriction flags, in the same form |
| `X_SBBS_EXPIRE` | date | Account expiration date; left out when the account doesn't expire. MODUSER.DAT takes a new date as a hexadecimal Unix time, so the door converts it |
| `X_SBBS_MINUTES` | int | Minutes in the user's time bank |
| `X_SBBS_CREDITS` | uint64 | Credits, in bytes |
| `X_SBBS_FREE_CREDITS` | uint64 | Free credits, renewed daily and spent before `X_SBBS_CREDITS` |

MODUSER.DAT is Synchronet-specific and outside this specification.

One more `[x-sbbs]` key describes the door rather than the user:

| Key | Type | Meaning |
| --- | --- | --- |
| `X_SBBS_SECTION` | ascii | The internal code of the external program section (the menu grouping) the door entry belongs to, such as `GAMES`; left out for a door run outside a section, such as a timed event |

Sections are Synchronet's own way of grouping doors, which is why this isn't a standard `DOOR_` key: most hosts have no equivalent to write.

### Encodings

Two independent rules set the encoding keys:

- **`FILE_UTF8` depends only on the door:** `1` if the door is configured as supporting UTF-8, otherwise left out, so text is CP437. Stored strings may be CP437 or UTF-8, so the writer converts each one to the file's text encoding.
- **`COMM_CHARSET` depends on the door's binary-mode option (`XTRN_BIN`) and the user's terminal:** `CP437` without `XTRN_BIN`, because Synchronet then translates the door's output for the terminal. With `XTRN_BIN`, Synchronet passes the door's bytes through untranslated, and the value is the terminal encoding Synchronet has for the user (UTF-8, CP437, US-ASCII or PETSCII). That encoding may have been detected or set manually by the user.

| `FILE_UTF8` (door supports UTF-8) | `XTRN_BIN` | User's terminal | `COMM_CHARSET` | Result |
| --- | --- | --- | --- | --- |
| `0` | no | any | `CP437` | Synchronet translates the door's output for the terminal |
| `1` | no | any | `CP437` | Same; the door converts UTF-8 text values to CP437 before sending them |
| `0` | yes | CP437 | `CP437` | No translation needed |
| `0` | yes | UTF-8 | `UTF-8` | The door can't produce UTF-8, so it should tell the user and exit |
| `0` | yes | US-ASCII | `US-ASCII` | The door must limit itself to 7-bit ASCII |
| `0` | yes | PETSCII | `PETSCII` | The door must send PETSCII itself (e.g. a Commodore-specific door) |
| `1` | yes | CP437 | `CP437` | The door converts its text to CP437 |
| `1` | yes | UTF-8 | `UTF-8` | UTF-8 from the file straight to the terminal |
| `1` | yes | US-ASCII | `US-ASCII` | The door converts to ASCII |
| `1` | yes | PETSCII | `PETSCII` | The door converts to PETSCII |

Doors have no UTF-8 setting yet. The "Support UTF-8 Encoding" toggle (`XTRN_UTF8`) exists only for message editors in SCFG, and would need to be added for doors. Editors get drop files too (`writemsg.cpp` calls `xtrndat()`), so for them the rule works today.

### Synchronet to-dos

Work Synchronet needs beyond writing the file itself, before every key above can be filled in:

- [ ] **`TERM_SIXEL` for terminals other than CTerm:** parse the standard device attributes reply (`CSI ? <params> c`) in `answer.cpp`, which today drops it, and store whether it lists 4 on `Terminal`.
- [x] **`TERM_CTERM` for forked CTerms:** store a fork's third revision field (#1250).
- [ ] **`TERM_MOUSE` and `PREF_MOUSE`:** detect mouse capability once per session (CTerm device attribute 7; DECRQM requests for modes 1006 and 1016 on other terminals) and keep it on the `Terminal` object, like the audio capabilities, never in `autoterm`: `Terminal::get_flags()` merges the user's manual flags into the detected ones with OR, so a detected `MOUSE` flag would override a user's "no". The user's `MOUSE` setting then means consent ("use the mouse"), not capability: reword the new-user and Terminal Settings question to match, default the answer from detection, and skip it when detection says the terminal can't. Hot-spots and mouse scrolling need both the setting and the capability.
- [ ] **`TERM_RIP`:** keep the RIPscrip version from the logon detection reply (`RIPSCRIP015400`), which Synchronet logs today but doesn't store.
- [ ] **`TERM_COLORS`:** detect the terminal's color depth, which Synchronet doesn't record today.
- [ ] **`TERM_NAME`:** send `APC SyncTERM:VER` to a CTerm once per session and keep the program name from its reply (`SyncTERM 1.7rc1` gives `SyncTERM`); other terminals would need XTVERSION (`CSI > 0 q`).
- [ ] **Personal details per door:** a door option in SCFG that leaves out `USER_REALNAME` and the other personal keys listed under Security and privacy.
- [ ] **`TERM_KEYS_KITTY`:** add the kitty keyboard query (`CSI ? u`) to the once-per-session capability queries.
- [ ] **`TERM_SIXEL_SCALE` (optional):** add the sixel scale probe to the once-per-session capability queries.
- [ ] **`FILE_UTF8`:** add the "Support UTF-8 Encoding" option (`XTRN_UTF8`) for doors in SCFG; today it exists only for message editors.
- [ ] **Standard-I/O echo:** imply the no-echo option for a door of this drop file type, so `stdio` means what the spec says.
- [ ] **"Use Real Name" option (`XTRN_REALNAME`):** in the older formats it swaps the single name field to the real name. This file has separate keys, so it must never alter `USER_ALIAS`; use it as the per-door gate for writing `USER_REALNAME`, alongside or instead of the personal-details option above. Decide separately whether a door should be told to address the user by real name, which would be a policy key rather than this bit.
- [ ] **WWIV color codes (`XTRN_WWIVCOLOR`):** Synchronet rewrites WWIV color codes in a standard-I/O door's output when this option is set, the same kind of fact about the connection as `COMM_CHARSET`. Decide whether a door needs an `[x-sbbs]` key saying so.
- [ ] **`USER_LANG`:** move the BCP 47 syntax check that the BBSDEV.DRP writer uses (`bbsdev_language_tag_valid()` in `xtrn_sec.cpp`) into a shared helper, so both writers use it.

## Open questions

- [x] **Name:** `DOORDROP.INI` and `DOORDROP_INI`, from draft 0.7; `DROPFILE.INI` was too close to DTS-0001's `DROPFILE.###`. (Case needs no decision: the door gets the full path, and a door that looks the file up by name anyway must ignore case.)
- [ ] **ATASCII:** add `atascii` to the character sets and `TERM_TYPE` once a door or host author wants to consume it; no definition without a consumer and a tester. (`avatar` was added for converters from DORINFO1.DEF; see Coverage of older drop files.)
- [ ] **Keys the older drop files have and this one doesn't:** the proposals under Coverage of older drop files (`USER_PHONE`, `USER_LOGONS`, `USER_LAST_ON`, the transfer totals, `TIME_USED`): add them, or leave them to vendor keys?
- [ ] **Other file encodings:** a `FILE_CHARSET` key for CP866, CP850, CP865 or Amiga Latin-1 doors was declined as unneeded; revisit if a host or door that can't move to UTF-8 asks for it.
- [ ] **`SESSION_ID`:** a value unique per launch, so a door can tell a fresh file from one it has already seen, and host and door logs can be matched?
- [ ] **Connect-back doors:** a door, or an emulator's virtual COM port, that connects to a host TCP port has no `COMM_TYPE`. If one is added, it needs a loopback-only listener and a per-session token the door sends first, or whoever connects first gets the caller's session.
- [ ] **Message editor keys:** this file is offered to message editors too, but has no keys for the message being written: the area, the recipient, the subject, the quote file and the result file. DTS-0001 defined them. Add a `[message]` section, or leave editors to their host's own files?
- [ ] **Test files:** publish sample files with their expected key values, since the Win32 profile API, Python's `configparser` and plain line readers already disagree on quoted values and on a key in the wrong section.
- [ ] **Door data directories (shelved):** add `USER_DATA_DIR` (a door's per-user data, such as Synchronet's `data/user/<####>/<door>/`) and `DATA_DIR` (a door's shared data, such as `data/<door>/`)? The termgfx doors take these on the command line today (`-home`, `--data-dir`).
- [ ] **Reviewers:** ask these people to review this draft before Synchronet ships it:
  - BBS authors: ENiGMA½ (NuSkooler), Icy Board (mkrueger) and Deuce.
  - Door and door kit authors with recent releases (GitHub handles):
    - binary-knight: Usurper Reborn, which reads DOOR32.SYS
    - codefenix-dev: uMRC, a DOOR32.SYS MRC chat door
    - andy5995: Immortal Barons, a strategy door playable on a BBS or over SSH
    - hmderdoc: Lameboy and Lamegear, console emulator doors with graphics and sound
    - HeliosBBS: HeliosDoorKit, a multi-language door SDK
    - ZL4KJ: d32, a Free Pascal door kit
    - thewebexpert: bbs-door-server, a multi-node DOSBox door server for Synchronet
    - CryptoJones: the AdmiralBBS door specification
    - Smooth (Phenom Productions): Phenom Dropfile, a JSON drop file for Mystic BBS [PHENOM]
  - The r/bbs community, with a post linking to the draft.

## References

- [BCP47] Phillips, A. and M. Davis, "Tags for Identifying Languages", BCP 47. <https://www.rfc-editor.org/info/bcp47>
- [RFC1091] VanBokkelen, J., "Telnet Terminal-Type Option", RFC 1091, February 1989. <https://www.rfc-editor.org/info/rfc1091>
- [RFC2119] Bradner, S., "Key words for use in RFCs to Indicate Requirement Levels", BCP 14, RFC 2119, March 1997. <https://www.rfc-editor.org/info/bcp14>
- [RFC4647] Phillips, A. and M. Davis, "Matching of Language Tags", BCP 47, RFC 4647, September 2006. <https://www.rfc-editor.org/info/rfc4647>
- [RFC4254] Ylonen, T. and C. Lonvick, "The Secure Shell (SSH) Connection Protocol", RFC 4254, January 2006. <https://www.rfc-editor.org/info/rfc4254>
- [RFC8174] Leiba, B., "Ambiguity of Uppercase vs Lowercase in RFC 2119 Key Words", BCP 14, RFC 8174, May 2017. <https://www.rfc-editor.org/info/bcp14>
- [RFC8259] Bray, T., "The JavaScript Object Notation (JSON) Data Interchange Format", RFC 8259, December 2017. <https://www.rfc-editor.org/info/rfc8259>
- [DOOR32] "DOOR32 Revision 1 Specifications", February 23, 2001. <https://github.com/NuSkooler/ansi-bbs/blob/master/docs/dropfile_formats/door32_sys.txt>
- [DOOR.SYS] "DOOR.SYS", Synchronet wiki, documenting the 31-line and 52-line forms of the file Ricky Greer introduced in 1988. <https://wiki.synchro.net/ref:door.sys>
- [DTS-0001] Mecklenburg, D., "Doorware Technical Standard 0001", a proposed named-value `DROPFILE.###`, version 0.07 beta, January 14, 1992. <https://github.com/NuSkooler/ansi-bbs/blob/master/docs/dropfile_formats/dts_0001_0_07.txt>
- [BBSDEV.DRP] "BBSDEV.DRP 1.0". <https://realdeuce.github.io/bbsdev.drp/>
- [PHENOM] Smooth <PHENOM>, "Phenom Dropfile" v1.2 (JSON and line-based drop files for Mystic BBS), Phenom Productions, May 11, 2022. <https://web.archive.org/web/20250725102445/http://phenomprod.com/releases/mystic/pn-dropfile12.zip>
- [FSC-0015] "FOSSIL" specification, FidoNet Technical Standards Committee. <http://ftsc.org/docs/fsc-0015.001>
- [FSC-0025] Stanislav, G., "AVATAR video terminal specification" (AVATAR/0+), FidoNet Technical Standards Committee. <http://ftsc.org/docs/fsc-0025.001>
- [CTERM] "CTerm terminal emulation" (SyncTERM), in the Synchronet source tree. <https://gitlab.synchro.net/main/sbbs/-/blob/master/src/conio/cterm.adoc>
- [XTERM] Dickey, T., "XTerm Control Sequences". <https://invisible-island.net/xterm/ctlseqs/ctlseqs.html>
- [KITTY-KEYS] Goyal, K., "Comprehensive keyboard handling in terminals" (kitty keyboard protocol). <https://sw.kovidgoyal.net/kitty/keyboard-protocol/>
- [VT340TEST] hackerb9, "vt340test", including `testdecsdm.sh`. <https://github.com/hackerb9/vt340test>
