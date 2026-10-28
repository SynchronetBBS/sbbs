# Embedded per-BBS FileServ

FileServ is disabled by default and is independent of central NickServ and
ChanServ. Each enabled BBS introduces `FileServ_<QWKID>` as an ordinary IRC
user. It joins only the channels in `Channels`. Existing services/configurations
are unaffected. A nickname collision prevents startup; FileServ does not kill
or replace the existing nickname. No registration database change is needed.

Enable it in the IRCd's `ircd.ini`, for example:

```ini
[BBSServices]
NickServ=true
FileServ=true

[FileServ]
IncludeLibs=Main,Games
ExcludeLibs=Private*
IncludeDirs=*
ExcludeDirs=*_staff,*_incoming
PublicLibs=
PublicDirs=main_public,main_docs
Guest=0
Channels=#files
Address=files.example.org
Bind=0.0.0.0
PortFirst=40000
PortLast=40015
TLS=optional
MaxDownloads=4
MaxChats=4
MaxQueue=64
UserQueue=8
PageSize=25
Timeout=60
; Optional override when jsexec is not in the BBS exec directory:
; JSexec=/path/to/jsexec
```

Rehash applies changes, closes existing FileServ sessions and cancels its
queue. Defaults export all otherwise-accessible libraries/directories to
identified accounts, and **nothing anonymously**. Include/exclude/public
values are comma-separated, case-insensitive `*`/`?` masks. Library filters
match **library short names**, such as `Main` from `[lib:Main]`, regardless
of its code prefix. Directory filters match **full directory internal codes**.
Spaces within names are preserved; use `IncludeLibs=Main,BBS Software` without
quoting individual names. Surrounding spaces around each mask are ignored.
Exclusions win; public selections
must also satisfy both global include filters. Upload, sysop, user-to-user and offline directories
are never exported. Only registered catalog files are served. Paths and
symlinks escaping the configured native directory are refused. Quote filenames
containing spaces, for example `GET "Main Library/Documents" "read me.txt"`.

For anonymous requests, `Guest=0` selects the lowest-numbered active,
nondeleted BBS account with the **G restriction**, regardless of its alias.
An explicit positive `Guest` selects that account, which must also be an active
G-restricted account. No eligible guest means anonymous access is unavailable.
The guest must pass native directory and download ARS, restrictions, daily
limits and credit checks just like any other BBS user. Public selection grants
visibility only; it does not bypass those checks or make paid files free.

Protected exports require an actual, currently verified login to the BBS
serving the file. Local `PASS` qualifies even with NickServ disabled. Enable the local NickServ
to also permit `NickServ IDENTIFY`. Identifying to
another member of a linked nickname group **does not grant file access** to
this BBS. This intentionally differs from MemoServ's group mailbox access.
Native account number and first-on date prevent recycled accounts inheriting
permissions. Access is rechecked for queued work, before accepting a transfer,
and periodically during transfers. Logout, disconnect, loss of authorization,
and disabled/deleted accounts stop their protected sessions.

## Commands and channels

Send private commands with `/msg FileServ@<server> <command>` or use the bot's
actual `FileServ_<QWKID>` nickname. Every command has detailed `HELP <command>`.

| Command | Purpose |
| --- | --- |
| `HELP [command]` | Commands, syntax, permissions and behavior |
| `LIBS [page]` | Accessible exported library names |
| `DIRS [library-name-mask] [page]` | Accessible directories as `LIBRARY/DIRECTORY` |
| `LIST <library/directory> [filename-mask] [page]` | Paginated file catalog |
| `SEARCH <text> [page]` | Search filenames and descriptions |
| `INFO <library/directory> <filename>` | Size, description and account credit cost |
| `GET <library/directory> <filename> [TLS\|PLAIN]` | Queue an active DCC download |
| `CHAT [TLS\|PLAIN]` | Open the interactive DCC file browser |
| `ADDRESS [numeric-IP\|RESET]` | Privately override/reset the expected DCC peer IP |
| `QUEUE` | Your pending downloads and active sessions |
| `CANCEL [id\|ALL]` | Cancel your queued or active work |

In a configured channel, `!list` privately lists each participating BBS's
FileServ contact information. `!files <QWKID> <command>` addresses one BBS;
`!fserve <QWKID> [TLS|PLAIN]` opens its DCC browser. For example,
`!fserve BBSDEV PLAIN` requests ordinary unencrypted DCC CHAT when the BBS allows
plaintext. Omitting the option uses the BBS's default TLS policy. There are no
scheduled advertisements.
Channel commands are processed only after normal channel messaging permissions
pass. NOTICE input never invokes commands. Ordinary bot replies, listings,
help and errors use NOTICE to the requesting user. The sole PRIVMSG exception
is CTCP DCC protocol negotiation, which clients require to recognize offers
and `RESUME`/`ACCEPT` replies.

The chat browser has a three-level virtual hierarchy: `/`, libraries, and
directories within each library. `DIR` (or `LS`) lists libraries at the root,
directory short names inside a library, and filenames inside a directory.
`LIST` or `LIST . [mask] [page]` also lists the current location in DCC chat.
An explicit `LIST <library/directory> [filename-mask] [page]` lists that
directory without changing the current location.
For example:

```text
DIR
CD "Main Library"
DIR
CD Documents
DIR
INFO "read me.txt"
GET "read me.txt" PLAIN
CD ..
CD /
```

`CD ..` moves up one level; `CD /` returns to the root. `PWD` shows the current
virtual path. `CD /LIBRARY/DIRECTORY` also jumps directly to a directory.
Names match without regard to case. Quote names containing spaces. Separators,
quotes, percent signs and control characters within a native name are displayed
with percent escapes, so the displayed component can be used in commands.

`LIBS`, `DIRS`, `LIST`, `SEARCH`, `QUEUE`, `CANCEL`, `HELP` and `QUIT` also work
in chat. `DIR [mask] [page]` filters names at the current level. From outside
a directory, `LIST`, `INFO`, `GET` and `CD` accept `LIBRARY/DIRECTORY`; an
unqualified directory name is accepted when unique in the applicable library
or across exported directories. Full internal directory codes remain accepted
for existing commands and exact disambiguation. Library export masks use the
native library short names; directory export masks use full internal directory
codes. No OS paths are exposed, commands executed or uploads accepted.

## DCC transport and resource limits

`TLS=optional` defaults to encrypted **SSEND/SCHAT**. Clients lacking encrypted
DCC can explicitly request `PLAIN`; there is no automatic downgrade after a
TLS failure. `TLS=required` refuses plaintext. `TLS=off` defaults to plaintext
and refuses encrypted requests. Encrypted DCC uses the BBS's normal native
`ctrl/ssl.cert`; the client chooses its certificate verification policy.
The certificate is loaded when a peer connects, not checked when the listener
is created. The client still needs a reachable FileServ address and port.

`Address` is the advertised numeric IPv4/IPv6 address **or hostname**. A hostname
with both A and AAAA records lets IPv4 and IPv6 clients use the same configuration.
`Bind` also accepts a hostname, provided its addresses are assigned to this BBS.
For a directly reachable dual-stack host, both may name the same host:

```ini
[FileServ]
Address=files.example.org
Bind=files.example.org
```

Behind NAT, use a public hostname for `Address` and local addresses or wildcard
bindings for `Bind`. With the default `Bind=0.0.0.0`, IPv4 uses that wildcard and
IPv6 uses `::`. A numeric shared `Address` or `Bind` applies to its own family.
If `Address` is empty, the selected bind address is advertised; a wildcard bind
therefore requires an explicit advertised address.

Separate settings override the shared settings for each family, and can also
contain hostnames:

```ini
[FileServ]
Address4=192.0.2.10
Bind4=0.0.0.0
Address6=2001:db8::10
Bind6=::
```

FileServ selects the family from the requesting user's IRC address, or their
private numeric `ADDRESS` override. It resolves the required A or AAAA records
asynchronously through `dns.js`, keeping IRC responsive. Answers are cached
for their DNS TTL, capped at five minutes; failed lookups are cached for five
seconds. The first valid address in the selected family is used. All addresses
used by a bind hostname must be suitable local interfaces. A missing or invalid
endpoint for the selected family produces a NOTICE instead of an unusable DCC
offer; FileServ does not switch the client's address family automatically.
Client DCC support for IPv6 is required to receive IPv6 offers.

`PortFirst=0` and `PortLast=0` use ephemeral ports; otherwise configure a range
and allow/forward those ports through the IPv4 and IPv6 firewalls as appropriate.
Only active DCC SEND, SSEND, CHAT and SCHAT are supported; no reverse/passive
DCC or uploads.

The default expected peer address is the requesting IRC user's IP. Other
addresses are refused before TLS or file data. Bouncer/proxy users can privately
set `ADDRESS` to the address their DCC connection will actually use. The
setting belongs only to that IRC session and never changes the BBS user record.
Each request captures its selected bind, advertised address and expected peer
address before queuing. Changing `ADDRESS` or DNS records affects later requests;
queued and active transfers retain their captured addresses. Cancelling while
DNS is pending prevents a late offer.

Downloads are serialized per BBS account across IRC sessions. Defaults allow
four simultaneous downloads, four chats (at most one chat per IRC session),
64 queued downloads overall and eight per account. Anonymous downloads share
the configured guest's account limit. Catalog workers are also bounded, with
at most two pending catalog requests per session. Listings contain 25 entries
per page. `Timeout` bounds DCC connection/handshake and stalled transfers;
idle chats close after five times that interval. Files larger than
4,294,967,295 bytes are refused because the native JS download-accounting API
accepts a 32-bit byte count. SEND supports pre-connection RESUME/ACCEPT, with
network-order cumulative four-byte acknowledgements.

Native `User.downloaded_file()` performs BBS file/user/uploader/system updates
and credit charging exactly once per transfer. Resumed transfers account only
newly acknowledged bytes; interrupted downloads account their acknowledged
partial bytes. Native semantics can charge the full file cost and increment
the download count even for a partial transfer. Failed offers with no delivered
bytes incur no charge. A sudden process/machine failure can lose progress not
yet received by the IRCd; this is not a persistent/resumable accounting journal.

FileServ starts short-lived **jsexec child processes** for catalog scans and
DCC socket/TLS work, including blocking native handshakes. The IRC loop polls
a private loopback control listener. Private per-worker capabilities, bounded
frames, renewable authorization leases, parent-connection EOF, timeouts and
process supervision constrain those workers. Cancellation and shutdown close
sockets and reap children; native worker exit failures are logged. Use a POSIX
host with a shell and `realpath` utility for this implementation. Child workers
have a bounded 64 MiB JS heap; for larger enabled services deployments allow
at least 64 MiB with the IRC service's `JavaScriptMaxBytes` setting. No C++ API
change or separate persistent service is required.

## Remote access, TLS policy and compatibility

Anonymous FileServ access needs no central authority. Direct bot nicknames
and CTCP DCC negotiation work across legacy relays. The server-qualified
`FileServ@server` alias requires a new connection IRCd; on older connection
servers use the actual bot nickname.

Protected remote access requires the serving BBS, central authority and user's
connection server to support the new FileServ services RPCs. Capability
negotiation fails closed for older endpoints. The authority asks the connection
server which exact BBS login the session still holds, and the serving BBS checks
its own independent authentication attestation. Group membership or a presented
username cannot substitute for that proof. Older intermediate IRCds can relay
these RPCs without modification. Public access and existing NickServ, MemoServ
and ChanServ remain available while endpoints are upgraded.

`[BBSServices] RequireTLS=true` additionally requires the full IRC path to
this BBS for authenticated FileServ requests. The authority's own RequireTLS
policy also applies to remote account checks. These settings protect the IRC
control path; the separate `[FileServ] TLS` setting controls the DCC connection.
`STATS U` reports FileServ's public configuration and queue/worker counts,
without worker capabilities, executable paths or BBS filesystem paths.

Account ARS uses `User.can_access_dir()` for both visibility and download
permission, never the daemon's global `file_area.can_*` flags. Existing JS APIs
supply no current client context for constructed `User` objects. Consequently,
HOST/IP/PROT ARS evaluates the account's stored last-login context, as those
APIs currently specify. FileServ never rewrites that context to simulate an
IRC/DCC request. The optional per-request context enhancement is tracked in
[work item 1310](https://gitlab.synchro.net/main/sbbs/-/work_items/1310).

## Isolated verification

Prepare the private services fixture using `tests/ircd/services-fixture.py`,
then run `tests/ircd/fileserv-fixture.py --jsexec /path/to/jsexec`. Never point
the fixture preparation at a live user database. Run:

```
jsexec -n -m 67108864 -c /tmp/ircd-services-bbs-fixture/ctrl \
  /path/to/exec/tests/ircd/fileserv.js --bbs-fixture
python3 tests/ircd/fileserv-network.py --jsexec /path/to/jsexec
python3 tests/ircd/services-network.py --jsexec /path/to/jsexec \
  --legacy-source /path/to/older/exec --skip-grace --fileserv
```

The FileServ network fixture generates only private test TLS keys and checks
actual native worker exit statuses, including errors after protocol completion.
IPv4 and IPv6 endpoints, local A/AAAA DNS replies, missing address families,
delayed DNS and cancellation, EOF, partial downloads, resume, encryption,
missing certificates, access rules, credit accounting, chat, cancellation,
and IRC responsiveness are exercised.
The services network test additionally checks protected remote access and
logout revocation through a real legacy relay, with no group-based file access.
