# What's New in Synchronet Version 3.22a

## General

- Optional **internal MQTT 5.0 broker** built into Synchronet (no
  external libmosquitto dependency)
- Optional **internal MQTT 5.0 client**, used as a fallback when
  libmosquitto is not installed
- IPv6 CIDR notation supported in `ip.can`, `ip-silent.can`, and
  `host.can` filter files (issue #1145)
- Trash filter (`*.can` files): expired entries are pruned before
  any new entry is added, and an already-filtered IP is no longer
  re-added
- Per-IP `login_attempts/<IP>` and `max_concurrent/<IP>` retained
  topics are now published over MQTT from every server
- New MQTT `clear` topic (per-host and per-server scope) tells the
  server(s) to clear their in-memory login-attempts list; matching
  `ctrl/clear` and `ctrl/clear.<svc>` semaphore files also added
- `getHostNameByAddr()` now supports IPv6
- Stability and resource-leak fixes across all servers from a
  thorough Coverity scan

## Servers

- All servers (mail, FTP, services, web) now share a common
  request-rate-limit auto-filter, automatically adding repeated
  offenders to `ip.can` or `ip-silent.can` for a configurable
  duration
- The auto-filter aggregates by IPv4/IPv6 subnet, so abuse spread
  thinly across a provider's range is bucketed and filtered as a
  single CIDR. Default IPv6 prefix is `/64`. An optional subnet
  threshold avoids collateral filtering on single abusers
- All servers now de-duplicate repeated identical error log
  messages, so a single sustained fault no longer floods
  `error.log`

## Terminal Server

- New **boolean text-search engine** with PCBoard / Wildcat-style
  `AND` / `OR` / `NOT` operators (symbol or keyword form),
  parenthesised grouping, and `"quoted phrase"` whole-word match
  (issue #1139)
  - Available for the four "Text to search for" prompts:
    message-base scan / F)ind, mail search, file-list search, and
    the file pager
  - `?` at any search prompt shows quick-reference help
  - Bare-word queries keep their substring behavior, so existing
    usage is unaffected
- File pager (`P_SEEK` mode) gains a `less`-style search: `/` to
  search forward, `n` / `N` next / previous match, `?` for help
  - Pager no longer exits silently at end-of-file; a final prompt
    allows scroll-back, re-search, or quit
- `printfile()` now handles files of 2 GiB or larger
- New auto-filter for clients repeatedly hitting the per-IP
  unauthenticated concurrent-connection cap (effective against
  bots that tie up nodes idling at the login prompt) — optional
  silent variant (issue #1140)
- Inactivity timer is reset after a login-failure delay or accept
  throttle, so users with tight inactivity limits aren't kicked
  mid-retry (issue #1124)
- The `NO_EXASCII` flag is no longer permanently saved to a
  user's record when a single dumb-terminal session triggers
  auto-detection (issue #1106)
- Progress percentages no longer exceed "100%" during long
  operations like `finduserstr`
- More than 255 QWK network hubs are now supported
- Removed unnecessary terminal color-change codes (regression
  from v3.21)
- Goodbye messages and pre-login banners no longer log noisy
  "send failed" warnings when the client disconnects mid-message
- Fix line-wrap divide-by-zero crash when an SFTP-only client
  connects (no terminal dimensions negotiated, issue #1120)
- VT320 status-line control (`DECSSDT` / `DECSASD`) — the BBS can
  now write to the bottom status row of a terminal that supports
  it

## Web Server

- New **subnet-aggregated connection rate limiter**, in addition
  to the per-request rate limiter from v3.21c, enforced at accept
  time (counts even aborted TLS handshakes that never produce a
  request)
- Connection and request rate limiters share the common
  auto-filter (see Servers)
- Web access log lines for unauthenticated requests now include
  protocol, IP, requested URL, and ARS
- Anonymous (no-auth) sessions skip the user-database lookup
  (small startup latency improvement)

## FTP Server

- Adopt the shared rate-limit auto-filter (see Servers)
- Fix file-descriptor leak in directory and index handlers that,
  under heavy listing load, could exhaust descriptors and trigger
  `EMFILE` floods (leak present since 2007)
- Fix delete permissions in `MLST` / `MLSD`: the `R` and `D`
  restrictions are no longer conflated; non-`R`-restricted users
  can again delete their own uploads
- Fix `PASV` response byte order for sysops who explicitly set
  the FTP server's public IP for IPv4
- When a maintenance script locks a filebase via the new
  FileBase JS lock method, the FTP server now refuses concurrent
  client writes to it — avoids `!DATA ERROR -202 adding file`
  races
- Fix silent setsockopt failures on some platforms (issue #1137)

## Mail Server

- Adopt the shared rate-limit auto-filter (see Servers)
- POP3: `USER` / `PASS` issued on an already-authenticated
  session now get a plain `-ERR` response rather than
  `!UNSUPPORTED COMMAND`, matching Dovecot / Courier behavior
  (issue #1123)

## Services

- Adopt the shared rate-limit auto-filter (see Servers)
- Rate-limited connections now log a `NOTICE` before being
  dropped, matching the other servers (previously dropped
  silently)

## SBBSecho

- Helpful error message when passed a directory instead of a
  config or `.ini` file on the command line

## BinkIT

- `data/binkstats.ini` no longer logs successful binkp/1.1 callouts
  (and inbound sessions) as failures — a closing-handshake quirk
  caused most sessions with binkp/1.1 peers (Synchronet, binkd) to
  be recorded as "callout failure" even though mail was delivered.
  Sessions with binkp/1.0 peers (e.g. Mystic) were unaffected

## SCFG

- New **Ctrl-F option search**: type a substring at the main
  Configure menu and see every matching option label, with its
  menu path rendered as a CP437 box-drawing tree
- New per-server **Rate Limiting...** submenus under Web Server,
  FTP Server, Mail Server, and Services
- New **Terminal Server → Max Concurrent Connections...**
  submenu (threshold, duration, silent variant)
- **Internal MQTT Broker** toggle under Networks → MQTT, which
  auto-configures broker address, port, TLS, and protocol-version
  fields when enabled
- New import/export of message sub-board configurations via
  `subs.ini` and file directories via `dirs.ini`, replacing the
  legacy `subs.txt` / `dirs.txt` purposes (issue #1128)
- Creating a new sub-board no longer copies the FidoNet area tag
  or Usenet newsgroup name from the previous sub (issue #1105)
- Help-text updates for the sub-board Semaphore File and Pointer
  Index options, and the Parent Directory import prompt
- Expanded inline help for the Servers → \* → JavaScript Settings
  menu (Heap Size, Time Limit, GC Interval, Yield Interval, Load
  Path) — corrects the wrong "ticks" wording (these are
  SpiderMonkey operation callbacks, not wall-clock time)

## Customization

- New PCBoard @-code aliases (issue #940):
  - `CARRIER` (= `CONN`)
  - `PROLTR` (= `PROT`)
  - `PRODESC` (= `PROTNAME` / `PROTOCOL`)
  - `CONFNAME` (= `CONF`)
  - `CURMSGNUM` (= `MSG_NUM`)
- New Wildcat! @-code aliases (issue #941):
  - `ACCBAL` (= `CREDITS`)
  - `USERID` (= `USERNUM`)
  - `CALLID` (= `CID`)
  - `CONNECT` (= `CONN`)
  - `ID` (= `QWKID`)
  - `ENTER` (approximate alias of `PAUSE`)
- New Wildcat!-style conditional display @-codes
  (issue #941): `@IFSEC=ars@` / `@ELSE@` / `@ENDIF@`
- `FILE_NAME` and `FILE_WEB_PATH` now expand to `<sys_id>.QWK` by
  default (issue #1133)
- New text.dat strings:
  - `SeekHelp` (less-style pager help text)
  - `FindStringNotFound`
  - `InvalidSearchExpression` (boolean-search parse errors)
- Changed text.dat strings:
  - `SeekPrompt` restyled to `<filename> (?=Help)` form (the key
    list moved into `SeekHelp`)
  - `SearchStringPrompt` now hints at the `?=help` binding
- New display file: `text/menu/textsrch.msg` — boolean-search
  quick reference, displayed when `?` is entered at a search
  prompt

## Stock Modules

- New: **LLM-backed Guru** — optional JavaScript chat engine
  (`chat_llm.js`) that backs a Guru, or an IRC bot
  (`chat_llm_irc.js`), with a local or hosted LLM: tool calling,
  BM25 retrieval (RAG) over your own message/file bases, and
  per-caller memory. Enabled per-Guru via the SCFG **Module**
  field; configured in `ctrl/chat_llm.ini`
- New: **SynChess** — graphical chess game with JXL piece
  artwork
- New: **ZZT** — game port for Synchronet (`xtrn/zzt`)
- New: **Avatar Chat** — avatar-first conversations with
  optional ANSI-art send/view and MOTD support
- New: `typemd.js` and `load/md2asc.js` — Markdown viewer that
  converts to plain text with optional Ctrl-A attributes
- New: `txt_handler.js` — DOS LIST-style HTML viewer for `.txt`
  files served by the Web server
- New: `md_handler.js` — Markdown-to-HTML web content handler
  (with `?raw` for the original `.md` as `text/plain`)
- New: `uselect_rip.js` — RIP-capable "Select Item" loadable
  module; `RIPScrollbar` gains a horizontal mode
- DD File Lister gains a full RIP user interface for RIP-
  capable terminals
- DD Message Reader gains per-subboard message-attribute toggles
  for the various BBS-software color codes. Sub-board color
  settings are no longer misapplied when reading personal email
  (issue #1107)
- `ircd.js`: WEBIRC support — trusted webchat gateways can
  announce the real operator's IP/hostname so `/whois` and
  K-lines target the real user instead of the gateway. CTCP
  request and reply handling fixed
- SlyEdit: messages uploaded via `/UPLOAD` (or `/UL`) are used
  as-is, with no attribute-code interpretation. Guest users get
  default settings; per-user settings are no longer saved for
  guests
- `shell_lib.send_email()` / `send_netmail()` accept a `to` /
  `address` argument; the email-menu **A**ttachment command
  works again (regression from a v3.21 refactor)
- More strings in stock modules route through `gettext()` for
  `ctrl/text.ini` `[JS]`-section customization
- Good Time Trivia gains a `useDoveNetSyncData` option that
  auto-finds the Dove-Net data sub-board (issue #1101)
- `delfiles.js` locks the file base during deletion to prevent
  FTP-upload contention
- Stock JS modules updated for SpiderMonkey 128 compatibility
  (in preparation for v3.30): `for each` replaced with
  `for...in`, E4X removed from `funclib.js`, top-level `const`
  changed to `var`. v3.22 itself still runs SpiderMonkey 1.8.5

## JavaScript

- New **`SQLite` JavaScript class** — first-class SQLite bindings,
  available wherever Synchronet runs JavaScript (Terminal, Mail,
  and Web servers, Services, `jsexec`, `jsdoor`, and background
  JS threads). Supports parameterized queries (positional `?`
  and named `:name` placeholders), `run` / `query` / `exec` /
  `prepare` one-shots, streaming via `SQLiteStmt.step()`,
  transactions via `db.transaction(fn)`, typed-column access via
  `SQLiteValue`, table and record abstractions for schema-less
  use, WAL journal mode, configurable busy timeout, and
  foreign-key enforcement. See `exec/examples/sqlite_example.js`
  for a complete reference and the `exec/tests/sqlite/` test
  suite for usage patterns
- `MsgBase.get_all_msg_headers()` fix: `*_NULL` fields
  (`to_ext`, `from_ext`, `replyto`, `replyto_ext`,
  `replyto_list`, `to_list`, `cc_list`, `summary`, `tags`,
  `from_org`, ...) no longer return `undefined` when the same
  header object's first read of that field happened to be NULL
- New methods: `FileBase.lock()` and `FileBase.unlock()` for
  maintenance scripts that want to keep external writers (e.g.
  the FTP server) out during a lengthy operation
- `new MsgBase(path, true)` / `new FileBase(path, true)` (the
  ad-hoc path-based form) now initializes status correctly for
  fresh file bases
- Fix crash in `savemsg` / `votemsg` for ad-hoc path-based
  MsgBases when the message addressed a real local user
- `system.get_telegram(0)` now returns `null` (was previously
  returning telegrams for user #1)
- `console.gotoxy()` validates its arguments more carefully and
  reports errors usefully (issue #1107)
- `html_encode()` now supports the Ctrl-AU and Ctrl-AV
  (sysop-defined high / low attribute) codes added in v3.21c,
  and ignores Ctrl-AE (iCE colors) sequences instead of breaking
  the encode
- TraceMonkey JIT is disabled by default — addresses
  intermittent crashes (issue #1143)
- New `js.terminate_on_disconnect` property (default `true`)
  controls auto-termination of a script when its client
  disconnects — in the Terminal, Web, and Services servers —
  decoupled from `js.auto_terminate` (which now governs only
  server shutdown/recycle). The Web and Services servers gained
  this disconnect check; a script that intentionally keeps working
  past disconnect (e.g. binkit) opts out with
  `js.terminate_on_disconnect=false`. Scripts/doors that
  previously set `js.auto_terminate=false` to survive a disconnect
  should now also clear this flag
- `jsexec` and `jsdoor` now ship with auto-generated object/API
  documentation; `jsexec`-only globals no longer leak into
  `jsobjs.html`

## JSexec

- Restored the "SBBSCTRL environment variable missing" startup
  warning
- Fix `SBBSCTRL` environment clobber when running large
  `addfiles.js` invocations

## chksmb / fixsmb / smbutil

- Small (1-2 record) `.sid` / `.shd` index-vs-status divergences
  are now auto-repaired when adding messages, instead of
  hard-failing with `SMB_ERR_FILE_LEN` (-206) and producing a
  silent mail outage. Larger mismatches still fail safely; use
  `fixsmb` to repair them
- `smbutil` no longer leaks memory or leaves the message base
  locked on Ctrl-C/break abort

## SFTP Server

The Synchronet SFTP server now advertises several extensions
that SFTP clients (notably SyncTERM) can use for a richer
filebase browsing experience:

- `pubdir@syncterm.net` — advertises `/files` as the public
  filebase root, so clients land there by default
- `sha1s@syncterm.net` / `md5s@syncterm.net` — per-file digest
  carried as a file-attribute extension, enabling hash-verified
  compare instead of size+mtime
- `descs@syncterm.net` — on-demand query that returns a file's
  extended description from the filebase

## MQTT Broker

Synchronet can now run its own internal MQTT 5.0 broker, with
no libmosquitto dependency. With the broker enabled, all
servers can publish and subscribe via direct in-process calls —
no TCP, no separate broker daemon.

- In multi-host BBS setups, only the host whose name matches
  `broker_addr` actually starts the broker; the rest connect
  to it as TCP clients
- External clients (qtmonitor, jsexec, `mqtt_pub.js`,
  `mqtt_sub.js`) connect using TLS-PSK (with the sysop user
  list as the PSK table), plain TLS, cert, or mTLS
- `$SYS/broker/version`, `$SYS/broker/log/<level>`, retained-
  message expiry, will messages, session resume, keep-alive
  timeout, and QoS 0/1/2 are all supported
- The stock JS broker (`broker.js`) had several wire-protocol
  fixes (PUBLISH framing, subscription-ID handling for
  forwarded messages, subscriber registration for existing
  topics) and memory-leak fixes
- New `broker.js` service scripts for systemd, FreeBSD rc.d,
  and Linux init.d (broker starts before sbbs)

## qtmonitor

A new Qt6 native GUI monitor app at `src/sbbs3/qtmonitor`,
intended as a cross-platform (Linux/macOS/Windows) replacement
for the Windows-only Borland `sbbsctrl.exe`. Connects to the
BBS over MQTT 5.0 (TLS-PSK, TLS+CA, or mTLS).

| Pane               | What it shows                                |
|--------------------|----------------------------------------------|
| Per-server logs    | Per-level filter, incremental text filter    |
| Nodes              | Status with verbose descriptions, Set Status |
| Clients            | Connected clients (from retained topics)     |
| Failed Logins      | Per-IP and per-server clear controls         |
| Max Concurrent     | Color-coded strike counts                    |
| Statistics         | From `mqtt_stats.js` retained topics         |
| Activity           | Login/post/exec/upload/page/error feed       |
| Broker             | `$SYS/broker/#` subscription                 |

- Server control (recycle / pause / resume / clear) per server,
  per host, or across all hosts; force timed event; force
  network callout
- Multi-host auto-discovery from MQTT topics, with host-selector
  dropdown
- Sysop-page alert via taskbar flash and beep
- Configurable max log lines per pane (default 2M),
  configurable publish QoS, dark/light theme toggle with live
  recolor
- Persistent window and layout settings
- Auto-builds the Qt MQTT module from source when not packaged
  by the distro

## Synchronet Control Panel (sbbsctrl.exe) for Windows

- New "Log to Disk" option for the Terminal and Web servers —
  daily `TS<mmddyy>.LOG` / `WS<mmddyy>.LOG` files in
  `data/logs/` (issue #1108). Default off on the two new
  servers, so existing installs don't silently start writing
  additional log files after upgrade
- Disk-log files are now flushed after each line, so log
  tailing tools see output without waiting for sbbsctrl to exit
  or the date to roll over (issue #1146)
- The shutdown-summary log line ("#### thread terminated, NN
  clients served") is now captured in the disk logs
  (issue #1146)
- BBS → View → Today's / Yesterday's / Another Day's Log
  restored to the consolidated user-session daily log (no
  prefix); the new Terminal → View submenu uses the `ts`
  prefix for the per-server log
- Restored the "SBBSCTRL environment variable missing" startup
  warning
