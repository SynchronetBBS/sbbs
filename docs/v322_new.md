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
- Windows: `system.name_servers` now includes the system's IPv6 name servers —
  on a host with IPv6-only DNS the list was empty, which broke `dns.js` (and
  so `ircd.js`, which won't start without it)
- Stability and resource-leak fixes across all servers from a
  thorough Coverity scan
- Unix-like systems: copying a file (the JavaScript `file_copy()`
  and Synchronet's internal file copies) now preserves the source
  file's permissions. Previously the copy was created with default
  permissions, so copying an executable produced a file that could
  not be run — a long-standing bug, present in v3.21 and earlier
  (issue #1202)
- Windows: `sbbs.exe`, `sbbsNTsvcs.exe`, `jsexec.exe`, and
  `sbbsctrl.exe` are now large-address-aware, so they can use up to
  ~4GB (rather than 2GB) of address space on 64-bit Windows
- The system-wide message and file totals (`@TMSG@`, `@TFILE@`,
  `system.stats.total_messages`, `system.stats.total_files`) are now
  counted at most once per new `totals_interval` (default 10 minutes)
  and shared process-wide, instead of being re-counted for every
  request. Obtaining them means examining every message base and file
  directory, which on a system with many bases — especially one whose
  data directory is on a network share, where each check is a separate
  round-trip — could take seconds. Posts and file additions or
  removals made by the same instance refresh the affected total
  immediately
- Counting the system's users no longer takes a shared byte-range lock
  on every user record: `data/user/user.tab` is now read sequentially
  in bulk. With ~1,400 user slots and the data directory on an SMB
  share, this removed ~2,900 lock round-trips per count

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
  - Available for the five "Text to search for" prompts:
    message-base scan / F)ind, mail search, file-list search, the
    file pager, and the sysop user editor's T)ext search
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
- Sysop user editor: the `~`, `+`, `*`, `$` and `/` prompts no
  longer run on the same line as the "User edit" command prompt
- More than 255 QWK network hubs are now supported
- Removed unnecessary terminal color-change codes (regression
  from v3.21)
- Fixed double-counting of terminal columns for Unicode output on
  UTF-8 terminals, which made word-wrap, right-margin truncation
  and centering wrap early (regression from v3.21, issue #1200)
- Goodbye messages and pre-login banners no longer log noisy
  "send failed" warnings when the client disconnects mid-message
- Fix line-wrap divide-by-zero crash when an SFTP-only client
  connects (no terminal dimensions negotiated, issue #1120)
- VT320 status-line control (`DECSSDT` / `DECSASD`) — the BBS can
  now write to the bottom status row of a terminal that supports
  it
- Doors are no longer told the caller has no ANSI when the caller
  uses an ANSI terminal in monochrome mode: line 20 of `DOOR.SYS`
  now reports "GR" whenever ANSI is supported (issue #1218)
- New **"Untranslated" external I/O mode** lets a door, external
  message editor, sysop page program, or global hot-key program
  emit output already encoded for the remote terminal (its own
  UTF-8 or raw graphics), with no CP437-to-UTF-8 translation or
  bare-LF-to-CRLF expansion applied by the BBS
- WWIV `CHAIN.TXT` drop file: the time-of-call and time-used fields
  (lines 24 and 25) are now populated rather than always zero
- `PCBOARD.SYS` drop file: the download-allowance, conference and
  language-extension fields are now populated
- Configuration changes now reach new callers without waiting for
  every node to disconnect: when a recycle is signaled (the
  `ctrl/recycle` semaphore, an MQTT recycle, `SIGHUP` /
  `systemctl reload`, or a console recycle command), active nodes
  are flagged to re-read their configuration on their next logon
  (the full server recycle still waits until all nodes are idle)
- Mail and message listings keep their columns aligned when a
  sender, recipient, or subject contains non-ASCII characters —
  field widths are now measured in screen columns rather than
  bytes (issue #1204)

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
- Fix: downloads via the file-area virtual path
  (`FileVPathPrefix`) ignored file credit costs entirely — for
  authenticated users as well as anonymous ones — while still
  charging the account after the transfer, so a user could go
  credit-negative and the internet at large downloaded costed
  files for free. Costs are now enforced as they are by the FTP
  server, and an unauthenticated request for a costed file is
  answered with `401 Unauthorized` (issue #1192)
- Fix: a user's free credits (renewed daily) now count toward
  file download costs, as they already did on the Terminal and
  FTP servers — the web server checked regular credits only,
  though free credits were spent first when charging
- Fix: the default `Authentication` list no longer advertises
  `TLS-PSK`, which the server has never been able to perform — a
  location configured to require it could not be authenticated by
  anyone. A warning is now logged at startup if the configured
  list still names it (issue #1206)
- Fix: the web interface's System Info sidebar evaluated each
  statistic twice — once to decide whether the row was worth showing
  and again to render it — doubling the work behind the front page

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
- New **DKIM signing of outbound mail** (RFC 6376, relaxed/relaxed,
  rsa-sha256): the SendMail thread can sign each outgoing message
  with a `DKIM-Signature` header so receivers can authenticate it as
  coming from your domain (issue #215). Enable with the `[Mail]`
  `DKIMSign` / `DKIMDomain` / `DKIMSelector` keys (or in SCFG); the
  RSA private key is read from `ctrl/dkim_<selector>.pem` and the
  matching public key is published in DNS. Supported on both *nix and
  Windows builds (signing uses OpenSSL libcrypto; it is a no-op stub if
  OpenSSL is unavailable at build time)
- **Message submission (RFC 6409) on ports 587/465**: the submission
  ports are no longer handled identically to port 25. They now always
  require SMTP authentication (a `530` reply otherwise) — point any
  unauthenticated clients at port 25 instead. An authenticated user
  sending to an external recipient via a submission port is now treated
  as a submission rather than a relay, so it no longer requires the
  `ALLOW_RELAY` option; that option continues to govern port 25
  (issue #1221)
- New **sender-address verification** on the submission ports: an
  authenticated user's `MAIL FROM` and `From:` address at one of your
  domains must resolve to that same user, using the same lookup that
  decides which account an incoming message is delivered to. Any address
  that reaches a user therefore works as a sender address for them —
  their alias, real name, `alias.cfg` entry, sub-address tag or user
  number — and nothing else does, so mail can no longer be submitted
  with a forged sender at one of your domains. An address at a domain
  that is not yours is accepted only if `ALLOW_RELAY` is enabled — the
  sysop who runs a smarthost for their users has already granted that
  permission on port 25, so submission honors it too (e.g. a BBS sysop
  submitting as their own domain); with `ALLOW_RELAY` off, it is
  rejected. A rejection names both the offending address and the address
  to use, so a mistyped client setting is self-correcting. The transfer
  port (25) does not verify sender addresses
- POP3: `USER` / `PASS` issued on an already-authenticated
  session now get a plain `-ERR` response rather than
  `!UNSUPPORTED COMMAND`, matching Dovecot / Courier behavior
  (issue #1123)
- The bundled SpamAssassin mail processor (`spamc.js`) now tells
  SpamAssassin which host actually delivered each message, so
  sender IP-based checks (DNSBLs such as Spamhaus, SPF, etc.) are
  evaluated instead of being skipped with `NO_RELAYS` — noticeably
  improving inbound spam detection. The added relay info is used
  only for scoring and is not duplicated into the delivered message
- New **`spamlearn.js`** mail processor trains SpamAssassin's Bayes
  classifier from e-mail: redirect (resend) spam to a `spamlearn`
  address and misclassified good mail to a `hamlearn` address and it
  feeds each message to `spamd` (which must be started with
  `--allow-tell`). Configure the addresses in `mailproc.ini`, gated to
  the sysop (`AccessRequirements = user equal 1`) so the learn
  addresses can't be used to poison Bayes
- `spamlearn.js` also has a `block` option for **honeypot / spam-trap**
  addresses: mail to such an address is learned as spam *and* the
  sender's IP is filtered (blocked), then the message is dropped. Use it
  in place of a `spambait.cfg` entry (which blocks but can't train Bayes)
  to get both blocking and Bayes training from trap hits
- Fixed `system.filter_ip()` ignoring a duration argument that followed
  the filename (its documented position), which had produced permanent
  (never-expiring) filter entries instead of timed ones
- Auto-detected DNS server addresses are now restricted to IPv4, since
  the MX look-up only speaks IPv4

## Services

- Adopt the shared rate-limit auto-filter (see Servers)
- Rate-limited connections now log a `NOTICE` before being
  dropped, matching the other servers (previously dropped
  silently)
- Finger: numeric requests outside the valid user-number
  range no longer throw a JavaScript error
- Finger: forwarded requests (`user@host@target`) no longer
  answer with the wrong user (the name was being truncated by
  one character)
- `websocketservice.js`: the file it writes beside each relayed
  connection now names the authenticated web session's user, not
  just the client's IP address, so the service behind the relay
  can tell who is connected
- `websocketservice.js`: that file moved from `temp/` to `data/`
  and is now named for the connection's address as well as its
  port, so a relay and its service can be on different hosts of a
  shared install
- `websocketservice.js`: new `-auth` option refuses to relay a
  connection it could not describe in that file, and new `-login`
  option refuses one with no logged-in web session

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
- New **DKIM Signing / Domain / Selector** options under Mail Server
  → SendMail Support, for outbound DKIM message signing
- New **Terminal Server → Max Concurrent Connections...**
  submenu (threshold, duration, silent variant)
- New **Message/File Total Interval** option under System → Advanced
  Options, setting how long the system-wide message and file totals
  are re-used before being counted again (default `10m`, `0` to count
  them for every request)
- **Internal MQTT Broker** toggle under Networks → MQTT, which
  auto-configures broker address, port, TLS, and protocol-version
  fields when enabled
- New import/export of message sub-board configurations via
  `subs.ini` and file directories via `dirs.ini`, the preferred
  formats going forward (legacy `subs.txt` import/export remains
  available) (issue #1128)
- Creating a new sub-board no longer copies the FidoNet area tag
  or Usenet newsgroup name from the previous sub (issue #1105)
- Help-text updates for the sub-board Semaphore File and Pointer
  Index options, and the Parent Directory import prompt
- Expanded inline help for the Servers → \* → JavaScript Settings
  menu (Heap Size, Time Limit, GC Interval, Yield Interval, Load
  Path) — corrects the wrong "ticks" wording (these are
  SpiderMonkey operation callbacks, not wall-clock time)
- New **"Translate Character Set"** prompt in the external program
  **I/O Method** dialog (`Yes` by default; `No` runs the program
  *Untranslated*, shown in the option summary) — applies to doors,
  external editors, and sysop page programs, and global hot keys
  gained their own **I/O Method** option

## Customization

- Configuration files now support a `<name>.local.<ext>` variation, preferred
  over the distributed `<name>.<ext>` wherever `file_cfgname()` is used - a
  place to keep your version of a file Synchronet distributes, separate from
  ours (and safe from the upgrade that refreshes the `exec`, `text` and `xtrn`
  directories). A per-host (`<name>.<hostname>.<ext>`) or per-platform
  variation still wins over it where one exists.
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
  - `UeditARSearchPrompt` (user editor's `/` AR-string search prompt)
- Changed text.dat strings:
  - `SeekPrompt` restyled to `<filename> (?=Help)` form (the key
    list moved into `SeekHelp`)
  - `SearchStringPrompt` now hints at the `?=help` binding
- New display file: `text/menu/textsrch.msg` — boolean-search
  quick reference, displayed when `?` is entered at a search
  prompt
- Fixed: a `mods/text` override file was shadowed by a same-named
  stock `text/` file using a higher-priority filename extension
  for the caller's terminal type (e.g. `mods/text/answer.asc`
  ignored in favor of stock `text/answer.msg`); the mods dir is
  now searched across all extensions before the stock dir, and
  mods width-variant files (e.g. `.40col.ans`) no longer require
  a plain same-extension mods file to exist (issue #1182)
- Fixed: the Unicode @-codes (`@U+XXXX@`, `@CHECKMARK@`,
  `@ELLIPSIS@`, `@COPY@`, `@SOUNDCOPY@`, `@REGISTERED@`,
  `@TRADEMARK@`, `@DEGREE_C@`, `@DEGREE_F@`, `@WIDE:text@`) now
  expand to a value instead of printing immediately, so format
  modifiers (alignment, width, truncation) work on them and
  `bbs.atcode()` / `bbs.expand_atcodes()` return them rather
  than emitting stray output (issue #1198)

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
- New: **SyncDOOM** — multiplayer DOOM as a door
  (`xtrn/syncdoom`), rendered over the terminal: JXL graphics
  (SyncTERM 1.4+), sixel (xterm, foot, WezTerm, Windows
  Terminal, …), or color-block ANSI/UTF-8 text, auto-selected
  per client. Co-op and deathmatch across the BBS (same host or
  a shared-install LAN) via a JavaScript lobby that browses,
  creates, and joins games. Needs a DOOM or Freedoom IWAD
- New: **SyncRetro** — legacy game consoles and arcade cabinets
  as a door, by hosting a libretro core (FreeIntv for
  Intellivision, FCEUmm for NES, MAME 2003-Plus for arcade) and
  rendering it over the terminal as sixel or color-block text
  graphics, with the game's audio streamed to SyncTERM. Builds
  and runs on both Linux and Windows (Win32). The core and the
  BIOS/ROMs are sysop-supplied content, not shipped
- New: **SyncArcade** (`xtrn/syncarcade`) — arcade classics via
  MAME 2003-Plus, with high scores shared by every caller the
  way a real cabinet's are
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
- New: `ircd_conf2ini.js` — converts a legacy `ctrl/ircd.conf`
  to the `ircd.ini` format, keeping the original as a backup and
  verifying that nothing was lost in translation
- IRCd config-file writing (`ircdcfg.js` "Save") no longer drops
  bans, `/RESTART` and `/DIE` operator privileges, hubs without a
  server link, or `[Allow]` passwords
- IRCd `[Class]` sections honor `Maximum=0` (unlimited) again
  instead of silently substituting a limit of 100
- An IRCd `[Operator]` section using the `S` flag (authenticate
  with the BBS system password) no longer needs a placeholder
  `Password=` value in order to load
- An operator's `/CONNECT` no longer bypasses the IRCd's
  one-outbound-link-at-a-time guard, which allowed a scheduled
  auto-connect to dial a second server at the same time
- When a remote server refuses a link, its `ERROR` reply is now
  logged and reported to opers with the reason, rather than being
  answered with "You have not registered"
- The IRCd reports itself as `SynchronetIRCd-2.1` in `/VERSION`,
  `/TRACE` and on connect, covering the changes made during the
  3.22 cycle — notably the TS-bearing JOIN burst announced by the
  `TSJOIN` capability
- The IRCd refuses a server link that uses its own server name, and
  reports (rather than dials) a `[Server]` section naming itself.
  Such a link exhausted the JavaScript stack and terminated the
  IRCd the moment it dropped
- `ircd.ini` supports the same `SYSTEM_HOST_NAME`, `SYSTEM_NAME`,
  `SYSTEM_QWKID` and `VERSION_NOTICE` macros as `ircd.conf`, and
  `ircd_conf2ini.js` and `ircdcfg.js` preserve them. Previously a
  conversion or a save replaced them with the running host's
  values, renaming the server on any host that sets its own with
  `jsexec -h`
- The stock `ctrl/ircd.ini` no longer ships with active server
  links to vert and cvs, which made a fresh install dial out to
  both every few minutes with a placeholder password. Its
  `[Allow]` example now shows the catch-all mask last, where
  first-match ordering requires it
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
- Door game-data downloads fall back to a Synchronet-hosted
  mirror when a game's own publisher can't be reached, so an
  installer no longer fails because a download site moved,
  renamed a release, or went away
- Every archive a door installer downloads is now verified
  against a checksum built into the installer, from either
  source: one that doesn't match is refused rather than
  installed. Affects the game-data fetchers for SyncDOOM,
  SyncDuke, SyncAlert, SyncDawn, SyncNES and the ScummVM-based
  doors, several of which previously checked nothing
- `dns.js`: `resolve()`, `resolveIPv4()` and `resolveIPv6()` return the
  resolved addresses again when used in synchronous mode
- HatchIT (`hatchit.js`) now exits with a meaningful status (0 =
  success, 1 = error), so a script or timed event can tell
  whether a hatch worked. Previously every run — successful
  hatch, cancelled menu, or missing option — ended in an error
  and exit status 1
- HatchIT reports unrecognized and incomplete command-line
  options instead of crashing, and its `-replace` option now
  writes a usable TIC `Replaces` line: the hatched file's own
  name, or an explicit mask with the new `-replace=<mask>` form
  (it previously wrote the literal text `Replaces true`, which
  matched nothing)

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
- Windows: scripts using socket callbacks (`Socket.on()`,
  e.g. the IRC daemon) no longer stop accepting new connections
  once the process reaches 64 open sockets
- Windows: `Socket.error_str` now reports the actual error for
  errors with a long description (e.g. a connection timeout),
  rather than "Error 122 getting error description"
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
- `File.iniGetAllObjects()` is much faster on a file with many
  sections — it no longer re-scans the whole file once per
  section (30x faster on a 4700-section `.ini`)
- `File.md5_base64` fix: was encoding 20 bytes instead of 16 (a
  28-character result with 4 bytes of uninitialized memory appended)
  since v3.19
- New SHA-256 support: `sha256_calc()` global and `File.sha256_hex`
  / `File.sha256_base64` properties, matching the existing MD5 and
  SHA-1 equivalents (the `File` properties stream the file, so
  large archives are never held in memory)
- New `load("sha256.js")` library provides `sha256_of_file()`, and
  back-fills `File.sha256_hex` (via cryptlib) on pre-3.22 builds

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

## sexyz (X/Y/ZMODEM file transfer)

- Much faster ZMODEM streaming sends. The send path now buffers
  its output instead of feeding the transmit ring one byte at a
  time, roughly a 10x throughput increase on fast/local links
  (and far less CPU) with no change to slower/real-world links,
  where the connection speed dominates either way. Windowed
  (`-w`) and segmented (`-s`) sends are unchanged
- Fixed a hang on windowed (`-w`) ZMODEM sends of files larger
  than 2 GB — the transmit-window and ACK positions were signed
  32-bit and went negative past 2 GB (issue #1196). Validated to
  2.36 GB; 4 GB remains a hard ZMODEM protocol limit
- Fixed a crash (divide-by-zero) at the start of any send where
  the transmit window is smaller than four times the block size,
  e.g. `-8 -w8192` or a comparable `MaxWindowSize` in
  `sexyz.ini` (issue #1197)
- `-w` (transmit window) now reduces the block size to a quarter of
  the window when needed, the way `lsz`/`sz` do, so a window at or
  below the block size no longer stalls the transfer (issue #1197)
- ZMODEM send-path speedups (table-driven byte classification,
  slicing-by-4 CRC-32, buffered whole-file CRC), shared with
  SyncTERM's built-in transfers

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
