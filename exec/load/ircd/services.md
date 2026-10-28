# Embedded IRC services

`ircd.js` loads these services itself. No additional process or listening port
is needed. Local BBS services are disabled by default. Enable them in `ircd.ini`:

```ini
[BBSServices]
NickServ=true
MemoServ=true
RequireTLS=false
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
NickExpireDays=0
ChannelExpireDays=0
MaxNicks=10
RequireTLS=false
```

Among enabled instances, the one whose listening addresses include an address
of `Host` automatically hosts the authority. It must already trust that
hostname through its ordinary services U-line configuration. `Host` defaults
to `services.synchro.net`; `Host` and `Database` are optional. Omitting `Enabled`
or setting it to `false` keeps central services inactive. A rehash applies
changes to these settings.

Every BBS using the embedded authority must configure the same `Host`, even
when its own `Enabled` is `false`. Trusting a server in `[Services:...]` alone
does not select it as the embedded authority. A leaf authenticates the helper
transport independently, then accepts authority requests only from its
configured `Host`. A wrong host can therefore leave local identification
working while remote IDENTIFY times out and identity updates are ignored.

When the INI contains any `[Services:...]` sections, those sections define the
entire services trust list. The default `services.synchro.net` and
`stats.synchro.net` entries are added only when no services sections exist.
Empty or disabled services sections also suppress the defaults.

`NickExpireDays` and `ChannelExpireDays` independently expire registrations
after the specified number of unused days. Both default to `0`, which disables
expiry. For example, `NickExpireDays=30` and `ChannelExpireDays=90` expire
nicknames after 30 days and channels after 90 days. Set these on the central
services host; leaf settings do not control the master registration list.

Last-used timestamps are tracked even with expiry disabled and appear in
NickServ/ChanServ `INFO`. A nickname is used while its authenticated owner is
using that specific nickname; identifying on another nickname does not renew
all of the account's registrations. A channel is used while an authenticated
founder or access-list member is present, including voice-level access. Idle
connections count. Services presence, unrelated visitors, failed identification,
and outsider `INFO` queries do not renew registrations.

Usage writes and expiry checks run once per minute, with a one-minute startup
allowance for network snapshots and account identification. Brief visits and
departures are recorded between checks. Pending usage is flushed when services
stop cleanly; an abrupt shutdown can lose activity since the last checkpoint. Last-used
times survive restarts, and downtime counts toward expiry. Expiry drops the
registration; channel expiry also removes its access entries and managed exceptions.
Nickname expiry leaves the BBS account and its channel permissions intact.
The registration database uses schema version 4. Schema 3 is the deployed
compatibility baseline and upgrades automatically when services next open the
database. The upgrade accepts schema-3 databases with or without the early
managed-exception table and preserves accounts, linked identities, nickname and
channel registrations, memo destinations, usage timestamps, access lists,
settings, and existing exception grants. Schema changes, data validation, and
the version update commit together; a failure rolls back the upgrade. Future
schema changes require ordered migrations from this baseline. Newer unsupported
schemas and unversioned populated databases are refused without being rebuilt.
Older builds that accept only schema 3 cannot reopen an upgraded database;
retain a pre-upgrade database backup if a software rollback is needed.

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

## Requiring TLS

Version 2.8 adds independent, optional `RequireTLS` settings in `[BBSServices]`
and `[EmbeddedServices]`. Both default to `false`; existing configurations and
external services keep working. Rehashing applies changes.

Set `[BBSServices] RequireTLS=true` on each participating BBS to protect its
NickServ authentication and private MemoServ operations. This also requires a
TLS client socket for BBS `PASS` authentication. A plaintext client may still
connect anonymously, retaining its `~` username prefix, but its password is
not checked.

Set `[EmbeddedServices] RequireTLS=true` on the authority to protect nickname
and channel registration, account linking, GHOST, access lists, configuration
changes, remote authentication, and private account/mailbox commands. Enable
both settings to protect both the local BBS services and the central authority.
Public HELP and INFO, and NickServ LOGOUT, remain available without TLS.

The requirement covers the client's actual socket and every server link to
the service handling the request. Remote BBS requests also check the authority's
path to that BBS before forwarding passwords or accessing telegrams. An unknown
hop fails the requirement, as does a known plaintext hop. Server links must
therefore support security reporting, and the client's owning IRCd must support
the version 2.8 `CLIENTTLS` capability. Legacy servers can remain connected;
users on unreportable paths cannot use protected commands while this option is
enabled. An authenticated session cannot grant central services permissions
while its path is unencrypted or unknown.

These checks use observed TLS transport. They do not imply that every client
validated its server's certificate. Configure validated server links as
described in [tls.md](tls.md); WHOIS and TLSROUTE help diagnose missing protection.

## Commands

`STATS U [server-mask]` reports services trust entries, the effective
`BBSServices` and `EmbeddedServices` settings, and runtime authority discovery
and RPC transport status. It is available without operator privileges. It
does not expose database paths, account details or transport secrets.
`STATS F` reports general server/network, clock, capability and TLS trust-store
configuration; `C` reports server links and their TLS settings; `P` reports
listeners and TLS; `O` includes operator flags/classes; `Y` includes class
comments; `B` reports RBL settings; `W` reports WebIRC host masks. Existing
`H`, `I`, `K` and `Q` reports cover hubs, client access, bans and nickname
restrictions. Passwords are omitted. `STATS ?` lists the reports, and each
accepts a remote server mask. Lowercase `u` remains the uptime report.

Each service provides a command index with `/cs HELP`, `/ns HELP`, or `/ms HELP`.
Use `HELP <command>` for syntax, permissions, behavior, limitations, and examples:

```text
/cs HELP REGISTER
/cs HELP ACCESS
/cs HELP EXCEPT ADD
/cs HELP SET MLOCK
/ns HELP IDENTIFY
/ns HELP LINK CONFIRM
/ns HELP SET MEMOACCOUNT
/ms HELP READ
/msg MemoServ@home.synchro.net HELP SEND
```

Help topics and command words are case-insensitive. HELP is public and does not
require a login or TLS, including when private services commands require TLS.
Server-qualified help includes the selected BBS in its examples and reply
instructions. All help replies use NOTICE and fit the IRC message limit.

With local NickServ enabled, `/NS` and `/msg NickServ` address the local BBS.
`/msg NickServ@home.synchro.net IDENTIFY Some.User password` selects another
participating BBS through the central authority. Use a protected client link;
remote identification carries the BBS password through trusted server links.

User-facing replies use `NOTICE`. When a BBS reply comes from a different BBS
than the user's connection server, it names the service and BBS (for example,
`[NickServ@home.synchro.net]`) and includes `/msg NickServ@home.synchro.net
<command>` or `/msg MemoServ@home.synchro.net <command>` reply instructions.
This also applies to errors and telegram text. Unqualified MemoServ READ/STATUS
check all accounts associated with the current owned nickname's network identity,
starting with its configured MEMOACCOUNT. Without an owned nickname, they use the
active account's group. Qualified reads select accounts on the named BBS from
your authenticated identities. Identifying to any group member authorizes access
to every mailbox in that group; separate BBS passwords are not required.
Both services' HELP replies include a concrete server-qualified address.
Central nickname/channel registration replies remain network-wide notices.

- `IDENTIFY <escaped-account> <password>` authenticates a BBS account.
- `LOGOUT` removes the connection server's BBS login; `NickServ@server LOGOUT`
  removes that BBS login. `LOGOUT ALL` removes every retained BBS login.
  Logging out of the active BBS selects the most recently identified remaining
  account, or restores the anonymous `~` prefix when none remain.
- `REGISTER` (or `GROUP`) registers the current nickname explicitly.
- `DROP [nickname]`, `INFO [nickname]`, and `GHOST <nickname>` manage ownership.
  GHOST disconnects another session using a nickname owned by any of your authenticated network identities.

### Linking BBS accounts

You may retain one verified account per BBS in the same IRC connection.
Identifying to a different BBS retains previous BBS logins; identifying again
at the same BBS replaces that BBS login. The most recent successful login sets
the visible username and the fallback network identity for unqualified MemoServ
commands when you are not using an owned nickname. Permissions from all retained
logins apply. Registration uses the active account's network identity.

To share registrations between independently authenticated accounts:

```text
/msg NickServ@vert.synchro.net IDENTIFY Deuce <password>
/msg NickServ@eotl.synchro.net IDENTIFY RealDeuce <password>
/msg NickServ LINK Deuce+VERT RealDeuce+EOTL
/msg NickServ LINK CONFIRM <code-from-preview>
```

`LINK` previews both complete identities: BBS accounts, nickname registrations,
telegram destinations, and channel permissions. Confirmation merges the whole
identities, not just the two named accounts. **Every member has equal control**
over all combined nicknames and channel rights, including linking or removing
other members, and may read telegrams for every BBS account in the group.
There is no primary account or BBS. Authenticate both exact accounts on this IRC connection; knowing an alias is insufficient.

The random confirmation is single-use, expires after five minutes, and is
bound to the IRC session, authentication revision, and previewed ownership,
access, and destination state. Confirmation revalidates BBS logins and checks
that the preview still applies. Changed state requires another preview.
The SQLite merge is atomic. It preserves last-used times, nickname telegram
destinations, and channel settings; duplicate access takes the higher level,
with founder rights taking precedence. Any authenticated linked member using a
registered nickname or present with channel access counts toward last use.

`[EmbeddedServices] MaxNicks=10` limits nickname registrations per network
identity. Set a positive integer on the central host. The limit applies to
REGISTER/GROUP and to the combined identities in LINK. An over-limit merge
changes nothing; drop excess registrations first. Lowering the setting retains
existing registrations but prevents additions above the new limit.

- `ACCOUNTS` lists your verified BBS logins and your linked identities, members,
  nicknames, destinations, and channel rights. Membership details are private
  to authenticated members. Public INFO describes nickname ownership without
  exposing the member BBS accounts.
- `SET <nickname> MEMOACCOUNT <escaped-account+QWKID>` selects a telegram
  destination within that nickname's identity. New registrations initially use
  the account performing REGISTER. A merge preserves each destination. This
  setting also selects the first mailbox checked by unqualified READ/STATUS
  and automatic notifications; other linked accounts are checked as well.
- `UNLINK <escaped-account+QWKID>` detaches that account into an empty identity.
  Registrations stay with the other members; the final member cannot unlink.
  If the account is a configured telegram destination, UNLINK lists the affected
  nicknames and refuses until their destinations have been explicitly changed.

Membership changes immediately affect services authorization in every session.
Existing channel op/voice modes follow the existing logout behavior and are not
automatically removed by logout or unlinking. MemoServ access follows current
group membership: unlinking immediately removes that mailbox from the remaining
group's authority, unless the session separately authenticated its new identity.

Someone using another account's registered nickname gets a 60-second grace
period to identify or change nicknames. On updated connection servers, services
rename that session to an unused Guest nickname. Legacy connection servers
lack a safe forced-rename command; services reclaim the nickname with KILL.

ChanServ permissions belong to network identities and follow every linked BBS
account across nickname changes:

- `REGISTER #channel <description>` requires channel ops and a current nickname
  registered to one of your authenticated network identities. Authenticate with
  NickServ IDENTIFY or BBS USER/PASS, then use NickServ REGISTER if your current
  nickname is not registered yet. Channel ownership is assigned to that
  nickname's identity, even if another retained BBS login is currently active.
  Existing channel founders and access rights remain unchanged by this rule.
- `ACCESS #channel ADD <registered-nick|escaped-account+QWKID> VOICE|OP|SOP`
  grants network identity access. `DEL <account>` removes it; `LIST` lists entries.
  Qualified accounts must previously have identified to central services.
- `OP`, `DEOP`, `VOICE`, `DEVOICE`, `INVITE`, and `UNBAN` take
  `#channel [nickname]`. Access grants automatic ops or voice on joining.
- `TOPIC #channel <text>` changes the topic.
- `SET #channel MLOCK <+/-imnpstkl> [key/limit]` enforces channel modes.
  `MLOCK NONE` clears the lock. `SET #channel KEEPTOPIC ON|OFF` controls
  topic restoration. Only the founder changes settings or drops registration.
- `INFO #channel` and `DROP #channel` inspect or remove registration.

ChanServ stays outside channels and observes their state directly from the
IRCd. Its trusted services server applies channel actions without membership
or channel ops. Registered channels disappear when their last member leaves;
the registration remains in SQLite. On recreation, ChanServ restores the saved
mode lock, topic (when KEEPTOPIC is enabled), managed exceptions, and automatic
ops/voice for authenticated access-list members. Restoration runs through the
normal services snapshot and action queue, so MODE and TOPIC updates may follow
the initial JOIN replies. Manual bans, exceptions, and other unsaved state do
not survive an empty channel. ChanServ removes automatic creator ops on a
registered channel unless the creator is authenticated as its founder or has
OP/SOP access. A creator with only VOICE access receives voice instead. This
correction also follows the services queue; later explicit operator grants
remain intact. Registration ownership remains with the saved founder.
Services startup does not create empty registered channels.
Invites from an outside services user require updated IRC relays and the
destination IRCd; older INVITE parsers can still require services to have ops.

### Managed channel exceptions (2.9)

```text
/msg ChanServ EXCEPT #channel ADD ~msgbypass:external:Bot!*@*
/msg ChanServ EXCEPT #channel ADD ~msgbypass:moderated:Bot!*@*
/msg ChanServ EXCEPT #channel DEL ~msgbypass:external:Bot!*@*
/msg ChanServ EXCEPT #channel LIST
```

Authenticated SOPs and the founder may ADD or DEL; OP access is sufficient for
LIST. The authority's `RequireTLS` policy protects all three commands. Entries
use the same hostmask syntax as channel `+e`: plain ban exemptions, or the
`external` and `moderated` message bypasses described in
[exceptions.md](exceptions.md). Unsupported selectors are rejected without
being stored. These are hostmask grants, even though the permission to manage
them belongs to an authenticated network identity.

ChanServ stores only entries explicitly added through EXCEPT. Adding an entry
already present in the live list adopts it without emitting a redundant MODE.
Direct MODE entries remain independent. DEL removes both the saved grant and
the matching live entry. Removing a managed entry with MODE alone causes
ChanServ to restore it; use EXCEPT DEL to revoke the saved policy.

Managed entries survive authority restarts and are restored after channel
recreation or removal from the live list. There may be at most 25 managed
entries. ADD refuses a new entry when the live list is full, but can adopt an
entry already present there. If a saved entry cannot later be restored because
manual entries fill the list, ChanServ keeps it pending, logs the full-list
condition once, and retries on subsequent channel changes. It never evicts a
manual entry to make room. Dropping or expiring a channel registration also
deletes its saved exception grants.

MemoServ is an interface to BBS telegrams, not a separate mailbox:

- `SEND <registered-nick|escaped-account+QWKID> <text>` delivers through the
  recipient BBS's native `system.put_telegram()` API. Qualified recipients need
  not have identified to IRC services before. The recipient BBS must be online
  with MemoServ enabled. Delivery is acknowledged only after storage succeeds.
  Sending to a registered nickname uses its configured MEMOACCOUNT. To override
  that destination, use `/msg MemoServ@server SEND <nickname> <text>`; this selects
  the nickname's linked account on that server. If none exists, or multiple
  linked accounts on that BBS make the selection ambiguous, delivery fails.
  An explicit `escaped-account+QWKID` always names that account; a conflicting
  service server is rejected.
- `STATUS` and automatic checks after identification report waiting telegrams
  across the nickname's group without consuming them. The configured MEMOACCOUNT
  is checked first. Automatic checks omit empty mailboxes and the completion
  summary; explicit `STATUS` still reports empty mailboxes. Waiting-mail notices
  and errors remain visible. File snapshots use native-compatible read locking.
- `READ` explicitly consumes telegram spools using `system.get_telegram()`.
  It reads all mailboxes in the current owned nickname's group, or the active
  account's group when no owned nickname is in use. `MemoServ@server READ` limits
  the read to accounts on that BBS within your authenticated identities.
  Identifying to any group member grants access to all the group's telegrams.
  Native BBS eligibility and first-on dates are still checked for every mailbox.
- Replies identify each mailbox. If one BBS is unavailable, MemoServ reports the
  error and continues checking the others. Logout, replacement authentication,
  and unlinking revoke access before further requests or replies are processed.

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

Debug messages beginning `Services RPC` summarize each complete sent or received
message, including its operation, peer BBS/helper, affected nickname and
connection server, request ID when present, and frame count. They do not log
individual data fragments. Reply matching reports the original operation,
elapsed time and a bounded result category. Authentication setup, discarded
frames, ignored authority requests, rejected identity updates, forced renames
and request timeouts have explicit reasons. Passwords, account proofs, session
cookies, command arguments, memo bodies and raw error text are excluded.

An authenticated transport does not imply the selected authority is available;
check `STATS U server.name` for the effective `Host`, authority presence/trust
and transport status. Identity refreshes normally run every 30 seconds. A
`rename` request instead enforces a registered nickname after its grace period;
its diagnostic identifies the target and whether the connection server applied
or ignored it.

Multi-login support is negotiated over the authenticated helper transport.
LINK requires updated central services, the connection server, and both BBS
helpers. Older services helpers keep single-login behavior and cannot authorize
LINK. Reconnecting the authority restores retained logins after their BBSes
revalidate them. Failed validation revokes that BBS login; other verified logins
remain usable. This does not require updates to intermediate IRC relays.
Group mailbox access and server-qualified nickname delivery are
negotiated with the authority. Older authorities retain their active-login
read behavior. Explicit server overrides are refused when unsupported, so a
message cannot silently be delivered to a different BBS.

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
/path/to/current/jsexec -n -c /tmp/ircd-services-bbs-fixture/ctrl -i load tests/ircd/services-link.js
/path/to/current/jsexec -n -c /tmp/ircd-services-bbs-fixture/ctrl -i load tests/ircd/services-expiry.js
/path/to/current/jsexec -n -c /tmp/ircd-services-bbs-fixture/ctrl -i load tests/ircd/services-notices.js
/path/to/current/jsexec -n -c /tmp/ircd-services-bbs-fixture/ctrl -i load tests/ircd/services-logging.js
/path/to/current/jsexec -n -c /tmp/ircd-services-bbs-fixture/ctrl -i load tests/ircd/services-bbs.js --bbs-fixture
python3 tests/ircd/services-network.py --jsexec /path/to/current/jsexec --legacy-source /path/to/old/exec
```

The native BBS locking test uses a test-only background helper while its parent
continues executing JavaScript. The network test launches real child IRCds,
checks their exit status independently of assertions, and exercises IPv6 clients
and actual older relays. It never contacts the live IRC network.
