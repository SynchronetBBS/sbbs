# Embedded IRC services (IRCd 2.6)

`ircd.js` loads these services itself. No additional process or listening port
is needed. Local BBS services are disabled by default. Enable them in `ircd.ini`:

```ini
[BBSServices]
NickServ=true
MemoServ=true
```

NickServ uses the BBS login lookup and password, including the normal
case-insensitive password comparison. Deleted, inactive, guest/shared, and
passwordless accounts cannot identify. `PASS` with a matching `USER` or nickname
uses the same authentication. A wrong password leaves the `~` username prefix.

Identification changes the username to `escaped-alias+QWKID`, for example
`Some.User+HOME`. Spaces appear as dots; Synchronet's alias matching accepts
dots in place of spaces. Other delimiters use `%HH` escapes (`+` is `%2B`).
Chat handles are not account identifiers. Ownership uses the BBS QWK ID,
user record number, and first-on date, so changing an alias preserves ownership
and a replacement user record with a new first-on date cannot inherit it.
Account restores must preserve the original first-on date.

## Central authority

Central NickServ and ChanServ are disabled by default, independently of the
local BBS services. Existing configurations never activate a replacement,
including while external services are disconnected. To enable the authority:

```ini
[EmbeddedServices]
Enabled=true
Host=services.synchro.net
Database=/path/to/ircd-services.sqlite
```

Among enabled instances, the one whose listening addresses include an address
of `Host` automatically hosts the authority. It must already trust that
hostname through its ordinary services U-line configuration. `Host` defaults
to `services.synchro.net`; `Host` and `Database` are optional. Omitting `Enabled`
or setting it to `false` keeps central services inactive. A rehash applies
changes to these settings.

The default database is `system.data_dir + "ircd-services.sqlite"`. It requires
a Synchronet build providing the native `SQLite` API, native `CryptContext`
HMAC-SHA2, and `User.stats.firston_date`. SQLite owns the registrations,
account identities, channel settings, and access lists; no BBS passwords are
stored. The database is locked against simultaneous owners. Back up this file
with the BBS configuration and account databases.

Use one authority and consistent services U-lines across the network. Its
uplink must permit it to introduce the virtual services server, using the
existing hub configuration. DNS address ownership does not grant an arbitrary
leaf permission to introduce services. If the services server or service nicks
already exist, the embedded authority stays inactive. Existing external
services data is not imported; coordinate cutover before enabling a replacement.

Each participating BBS server's first hostname label must equal its QWK ID
(for example `home.synchro.net` for QWK ID `HOME`), and those IDs must be unique.
The existing authenticated server links establish which BBS owns an identity.
An account with the same alias on another BBS is a different account.

## Commands

With local NickServ enabled, `/NS` and `/msg NickServ` address the local BBS.
`/msg NickServ@home.synchro.net IDENTIFY Some.User password` selects another
participating BBS through the central authority. Use a protected client link;
remote identification carries the BBS password through trusted server links.

- `IDENTIFY <escaped-account> <password>` authenticates a BBS account.
- `LOGOUT` removes the authenticated identity.
- `REGISTER` (or `GROUP`) registers the current nickname explicitly.
- `DROP [nickname]`, `INFO [nickname]`, and `GHOST <nickname>` manage ownership.
  GHOST disconnects another session using a nickname owned by your BBS account.

Someone using another account's registered nickname gets a 60-second grace
period to identify or change nicknames. On updated connection servers, services
rename that session to an unused Guest nickname. Legacy connection servers
lack a safe forced-rename command; services reclaim the nickname with KILL.

ChanServ permissions follow authenticated BBS accounts across nickname changes:

- `REGISTER #channel <description>` requires identification and channel ops.
- `ACCESS #channel ADD <registered-nick|escaped-account+QWKID> VOICE|OP|SOP`
  grants account access. `DEL <account>` removes it; `LIST` lists entries.
  Qualified accounts must previously have identified to central services.
- `OP`, `DEOP`, `VOICE`, `DEVOICE`, `INVITE`, and `UNBAN` take
  `#channel [nickname]`. Access grants automatic ops or voice on joining.
- `TOPIC #channel <text>` changes the topic.
- `SET #channel MLOCK <+/-imnpstkl> [key/limit]` enforces channel modes.
  `MLOCK NONE` clears the lock. `SET #channel KEEPTOPIC ON|OFF` controls
  topic restoration. Only the founder changes settings or drops registration.
- `INFO #channel` and `DROP #channel` inspect or remove registration.

MemoServ is an interface to BBS telegrams, not a separate mailbox:

- `SEND <registered-nick|escaped-account+QWKID> <text>` delivers through the
  recipient BBS's native `system.put_telegram()` API. Qualified recipients need
  not have identified to IRC services before. The recipient BBS must be online
  with MemoServ enabled. Delivery is acknowledged only after storage succeeds.
- `STATUS` and automatic checks after identification report waiting telegrams
  without consuming them. File snapshots use native-compatible read locking.
- `READ` explicitly consumes the telegram spool using `system.get_telegram()`.
  Only the authenticated account may read its telegrams.

## Compatibility and runtime details

Ordinary private messages between uniquely named `BBSsvc_*` helper users carry
services traffic across older relays. Helper identities include a daemon-only
username marker that ordinary USER/PASS or NickServ logins cannot construct;
taking a helper nickname on an old server does not grant services privileges.
Both routes are challenged and messages
are authenticated, sequenced, and bounded. Connection-server session cookies
protect delayed requests and tolerate old hubs replacing nickname timestamps.
Trusted hubs retain their existing authority: this transport is not encrypted
end to end, and older debug logs can contain its payloads. Updated daemons
redact services commands and RPC payloads from protocol logs.

Older relays and leaves continue to use standard IRC messages for channel
operations and ghost kills. They do not gain local BBS services or live username
refresh. Updated clients sharing channels receive a presentation refresh when
an identity changes; network membership and channel timestamps are preserved.
An old relay that rewrites nick timestamps can leave a remote WHOIS view with
its initial username; the connection server and account permissions remain
authoritative. BBS account eligibility is rechecked while central services
are available; an inactive or replaced account loses its authenticated session.

The current JS embedding crashes when timer callbacks capture local variables
([work item 1294](https://gitlab.synchro.net/main/sbbs/-/work_items/1294)). The
services timer uses a top-level callback with an explicit receiver. Persistent
`load(true)` workers also end when the parent enters the native callback loop,
so production services use a bounded queue processed between socket callbacks.
DNS uses the asynchronous resolver and SQLite lock waits are short. Native BBS
file operations remain synchronous and may delay a callback under contention.

## Tests

Use a current `jsexec` that supplies SQLite, and an isolated BBS fixture. The
fixture copies configuration into a private `/tmp` directory and redirects
its account and telegram files there; it does not modify live BBS accounts.

```sh
python3 tests/ircd/services-fixture.py --template-ctrl /path/to/ctrl --jsexec /path/to/current/jsexec
/path/to/current/jsexec -n -c /tmp/ircd-services-bbs-fixture/ctrl -i load tests/ircd/services.js
/path/to/current/jsexec -n -c /tmp/ircd-services-bbs-fixture/ctrl -i load tests/ircd/services-bbs.js --bbs-fixture
python3 tests/ircd/services-network.py --jsexec /path/to/current/jsexec --legacy-source /path/to/old/exec
```

The native BBS locking test uses a test-only background helper while its parent
continues executing JavaScript. The network test launches real child IRCds,
checks their exit status independently of assertions, and exercises IPv6 clients
and actual older relays. It never contacts the live IRC network.
