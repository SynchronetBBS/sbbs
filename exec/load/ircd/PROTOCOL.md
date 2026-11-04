# SynchronetIRCd 2.5 synchronization

Configured hubs remain authoritative over untrusted leaves. Clock agreement
is required only between peers that both configure the other as a hub.
The default limit is 30 seconds, configurable with `[Clock] MaxSkew` in the
IRCd INI file. Mutually configured upgraded hubs check clocks before state
exchange and every 15 seconds afterward. Stale samples after a callback stall,
a wall-clock step, or excessive round-trip delay are retried rather than
interpreted as skew.

Leaves can have drifting clocks. Their current clock and their asserted
channel ages never grant authority over an established hub channel. A new
leaf channel receives a hub timestamp, and the hub explicitly sends the
canonical channel state back to the leaf.

## Rolling upgrades and services

The required compatibility topology runs the latest IRCd on every hub, with
1.3a and other older versions connected as untrusted leaves. Preserve their
supported operations while preventing leaf assertions from corrupting canonical
hub state, claiming authority over other routes or disrupting the network.
Old-hub interoperability observations below are supplemental rollout evidence.

The `CLOCK`, `CSYNC`, and `TSMAP` capabilities identify the new leaf protocol.
Older hubs and untrusted leaves remain linkable, including with
`[Capab] Enforce=true`; missing extensions produce warnings. Mandatory
capability enforcement is deferred to a later version.
An old hub cannot relay the new mapping metadata. Mapping continuity across
hub changes requires an upgraded path between those hubs.

U:lined services retain JOIN/MODE bursts, authoritative MODE handling and
TS-zero commands. They are exempt from the new capability checks, clock
probes and corrective commands. TSJOIN-capable legacy hubs receive a founder
SJOIN when a new channel is created, avoiding their rejection of a separate
server-prefixed founder MODE.

An SJOIN from a U:lined services origin may name known users on any route,
including users local to the receiver. Hubs retain that origin and member
status when forwarding it, and apply the services origin's timestamp
authority even when it arrives through an ordinary hub. The origin itself
must still be routed through the sending link; unknown members are rejected.
SJOINs from ordinary hubs and leaves may name only users behind their link.

Services MODE, TOPIC, JOIN, PART and KICK corrections continue through hubs
whose local state already matches the request. MODE relays retain the full
requested change, including modes that were already set locally. Services
topic locks/restores may carry an older saved timestamp; a zero topic
timestamp is assigned a positive timestamp at the first receiver. Relays
preserve the services source so its authority survives each hop.

A services connection need not be configured as a hub to send cross-route
KILLs, global notices, or the SQUIT/SERVER sequence used for JUPE. SQUIT
retires the old server subtree before its replacement is introduced. The
replacement receives trust only from its own configured U:line. Global
services notices do not require an operator user mode. Services may also
introduce their reserved bot nicknames, subject to syntax and collision
checks. These privileges belong to the configured services origin, and the
claimed source must be reachable through the incoming link; an ordinary user or server on a
trusted connection does not inherit them.

Shutdown retires embedded services before the final synchronous queue
flush, preserving their QUIT/SQUIT messages along with queued channel state
and local users' QUITs. DIE, RESTART and the exit backstop share this
idempotent shutdown path.

Untrusted older leaves receive ordinary JOIN/MODE corrections. MODE omits
the timestamp, so an older local channel age cannot block hub authority.
Pre-2.2 daemons misparse a literal zero as the mode string; the timestamp-free
form is unconditional in both those daemons and later releases.
An old leaf can retain a different local channel age; its assertions cannot
replace an established hub channel. Its initial server MODE/TOPIC burst is
accepted only for channels it first introduces, while all members are routed
through that leaf. That exemption ends at BURST 0 or when another route joins.
Established channels receive immediate member/status introductions and a
complete correction after the leaf's burst completes.
Legacy topic delivery uses a per-channel, per-leaf presentation counter to
outpace timestamps received from that leaf, without changing network time.

The legacy path also covers 2.0 and 1.3a leaves, which advertise TSMODE
without TSJOIN. NICK introductions use numeric IPv4 addresses for peers
without NICKIPSTR and IP strings for peers advertising it, in both live
relays and reconnect bursts. Numeric incoming IPs are decoded even through
a newer hub that forwarded the old representation. IPv6 addresses have no
representation in old NICKIP and become its unknown-address value, zero;
this does not affect routing users by nickname.

Mixed networks retain limitations in the old implementation. In particular,
old daemons discard empty TOPIC commands, so a topic clear cannot reliably
reach their clients. Old hubs also retain their original timestamp merging
and same-second topic behavior; full convergence and cached timestamp handoff
require upgrading the hubs on the affected path. Compatibility keeps links
and supported operations working; it cannot repair code executing on old
peers. Use `tests/ircd/rolling.js --legacy-source=/previous/exec/` with a
separate previous-version source tree to test actual mixed processes.

## Services additions in 2.11

The channel-mode grammar includes Bahamut's parameterized modes `I` and `j`.
When a recipient cannot represent a mode, filtering consumes its arguments before omitting it,
preserving subsequent key, limit and status arguments in MODE and SJOIN.
An unknown argument grammar rejects the whole network request before any
channel mutation. Invite masks and extended policies use the capabilities below.

Set `Protocol=bahamut` in the existing `[Server:...]` entry on the daemon
directly connected to IRCServices' Bahamut module. The default is
`synchronet`. The profile supports ordinary `+e`, `+I`, and channel
`r/R/M/O/c/j/S` changes and reconnect lists;
extended message-bypass exceptions still require `CHANEXCEPT`. Configuration
rehash updates the connected peer's profile. It grants no services authority;
U-lines remain independent. The configurator and STATS C expose this setting.
Other Synchronet links negotiate these extensions through capabilities.

### Services identity and TLS presentation

`SVSMODE <nick> [timestamp] <modes> [stamp]` accepts services' `r`, `d` and
`a` operations only from a U:lined origin on its actual route. A nonzero
optional timestamp must match the current nickname incarnation. `d` consumes
an unsigned 32-bit stamp even with a minus sign; `r` records identification
for the current nickname, and `a` marks an identified IRC operator as a
services administrator. None grants IRC operator/server-admin privileges or
proves a BBS account. Synchronet's native user modes `r` and `d` remain operator
notice subscriptions. Bahamut NICK/MODE input cannot alias these meanings.

An untrusted leaf's original nickname timestamp is kept separately from the
hub's canonical timestamp. Conditional SVSMODE and SVCSTATE output toward that
owning leaf uses its original timestamp; other routes use canonical time.
The mapping belongs to the current user, nickname and exact link object and
is refreshed on nickname changes. A missing, stale or invalid mapping rejects
a conditional grant before mutation; it never becomes timestamp zero.
Omitted or zero SVSMODE timestamps retain their existing unconditional meaning.
This mapping supplies neither timestamp authority nor a BBS session identity.

Native links advertise `SVCSTATE`. Live grants retain their original services
prefix and pass through matching intermediate caches. Reconnect NICKs use
`stamp@authority` in the previously unused services-stamp field; older native
peers receive zero. `SVCSTATE nick created authority stamp r|a|ra|-` restores
observed grants only from a directly configured hub, for the matching user
incarnation and a configured services authority. NICK fields alone grant
nothing. Leaf-supplied stamp hints are ignored, since an external backend's
split recovery may treat them as authentication input. After a leaf reconnect,
fresh services authentication is required. Identification is never inferred
from a BBS-looking username or an ordinary user mode.

Real nickname changes clear identification/admin markers while retaining the
per-connection stamp; case-only changes preserve identification. Deoper clears
the admin marker. Services disconnects retain already observed identity;
REHASH trust revocation invalidates the owner's state. A Bahamut authority
receives its own numeric stamp and `r`/`a` in reconnect NICK introductions.
WHOIS 307 and the services-admin description come from the local canonical
state, including when the requested WHOIS is routed to another server.

Bahamut `+S` user presentation is derived from the existing first-party
`CLIENTTLS` observation. Bare NICK/MODE `+S` never establishes TLS. Unknown or
conflicting observations cannot upgrade a plaintext result. This describes
the client socket, independently of inter-server encryption and full-path
WHOIS/TLSROUTE results.

Native FileServ helpers retain the new channel-policy exemptions across server
links using their reserved `FileServ_` nickname (up to eight ASCII letters,
digits, underscores or hyphens) and exact `bbs+ircd+files` username. Ordinary
USER/PASS or BBS account presentation cannot supply that daemon username.
The owner must exist on the user's validated route through an admitted link.
This role grants no U:line, identification, account, operator or TLS privilege.
An owning-server CLIENTTLS INTERNAL report is accepted for this role. A legacy
gap still reports UNKNOWN transport while preserving the role; contradictory
transport observations remove its policy exemption. Ordinary moderation and
ban rules retain their existing behavior.

Embedded NickServ asserts the same nickname marker only when the authenticated
BBS identity owns that registered nickname, and withdraws it on logout, DROP
or ownership loss. The marker remains separate from its BBS proof and mailbox
authorization. Embedded ChanServ manages channel `r` from registration state
and supports the extended mode locks below.

### Invite masks and channel policies

Native `INVMASK` links exchange ordinary `+I` masks in MODE, SJOIN and reconnect
bursts. CSYNC adds `INVITES :<masks>` and can reconcile or clear the whole
list without churning matching entries' setter/time metadata. Older snapshots
cannot erase this list. Masks use ordinary nick!user@host matching, have their
own 25-entry local limit, and survive TS resets like bans. List numerics are
346/347; clients receive `INVEX=I` and the corresponding CHANMODES/MAXLIST.
A mask bypasses `+i` only; keys, limits, bans and other policies still apply.
Services CLEAR INVITES works through its ordinary `-I` operations.

Native `CHANPOLICY` links exchange these channel modes:

| Mode | Policy |
| --- | --- |
| `r` | Registered-channel marker; only services or authoritative server state can change it |
| `R` | New joins require services identification for the current nickname |
| `M` | Unidentified users cannot speak; channel ops/voices remain exempt |
| `O` | New joins require IRC operator status; changing the mode also requires operator status |
| `c` | Reject ASCII control/format/color codes, including ANSI escapes; allow CTCP framing and ISO-2022 charset shift prefixes |
| `j count:seconds` | Replenishing join bucket with that burst capacity and sustained rate; `-j` takes no argument; `+j 0` disables it |
| `S` | New joins require a known TLS client socket; unknown transport fails closed |

Join-rate components are positive integers up to 2147483647. The bucket uses
monotonic time, counts admitted remote joins too, and bounds distributed debt.
Changing the rate resets the bucket; matching MLOCK corrections do not. Equal-TS
bursts converge on the lower sustained rate, then smaller burst. This is
Synchronet's limiter; it does not reproduce Bahamut's local range clamps or
slow-start behavior. Services/internal bots are exempt from these policies.

Upgraded gateways check policies on JOIN/SJOIN from untrusted leaves or an
incapable intermediate hub. A rejected membership/status never enters canonical
state; a KICK corrects its source branch. For private/secret channels hidden
from an old leaf, that exact corrective KICK still reaches the rejected user's
branch without introducing a channel snapshot,
member list or status. This exception ends as soon as the correction is queued.
Burst membership is checked against identity/operator/TLS policy, but not the
live join-rate limit. `M` and `c` are also enforced on received messages.
Setting a join policy does not evict existing
members. A personal invitation or `+I` cannot bypass these policies.

Unsupported flags and their arguments are omitted for old native peers, including
MODE, SJOIN and CSYNC output. Old snapshots cannot erase unknown policy flags.
An old daemon still cannot enforce new policy inside its own subtree, carry
identity or TLS metadata through a hub, or admit `+I`-exempt users locally through
`+i`. Upgrade the path for complete behavior; explicit INVITE remains available.
Configured hubs retain their existing timestamp authority. These additions do
not allow a leaf to backdate or replace an established channel incarnation.

### Native S-lines

`SGLINE <mask-length> :<realname-mask>:<reason>` and
`SQLINE <nickname-mask> :<reason>` require a routed U:lined origin. Native links
advertise `SLINES`; other peers receive no unsupported S-line commands.
SGLINE splits at the declared length, so masks/reasons may contain colons.
Storage/removal identity is type + services-server owner + canonical exact mask.
Canonicalization collapses repeated stars and folds ASCII A-Z only; non-ASCII
wire bytes, including UTF-8 octets, are preserved in storage and replay.
`UNSGLINE :mask` and `UNSQLINE mask` remove only that entry; overlapping local
Q-lines and other owners' entries remain. Wildcard bulk-removal forms and
channel-name SQLINEs are outside this implementation.

S-lines check local admission, nickname changes and remote NICK introductions,
including users arriving through old hubs/leaves. Existing sessions are not
retroactively killed when a line is added. Service bots are exempt. Rejected
remote introductions receive an ordinary KILL before entering canonical state.
Capable reconnect bursts preserve the actual services owner and avoid returning
a forged authority prefix toward its own route.

These formats contain no expiry or complete-inventory marker. Entries are
runtime-only: full REHASH preserves entries while the authority is present and
trusted; trust revocation or authority departure clears its entries. Restart
also clears them. IRCServices must republish active policy on reconnect (its
`ImmediatelySendSline` setting), or supply entries reactively. Services owns
expiry and sends exact removals. No expiry is inferred from absent wire data.

### AKILL and forced nicknames

AKILL identity is the literal canonical mask plus its services-server owner.
Overlapping local bans and independent services bans coexist. RAKILL removes
only the matching owner's exact entry. Creation time and duration determine
absolute expiry; zero duration is indefinite. Malformed and already expired
requests are not relayed. Reconnect bursts preserve the original expiry.
Expiry is enforced on lookup and by the daemon timer. While the authority is
known and trusted, relays send RAKILL away from its route to clear old peers
that ignore duration. Full REHASH preserves unexpired entries whose owners
remain trusted; removing that trust discards their entries. REHASH AKILLS
explicitly clears runtime services bans. These entries are held in memory;
after restart, services must supply its policy again.

`SVSNICK <nick> <newnick> :<timestamp>` requires a validated services origin
and a positive wire timestamp. New links advertise `SVSNICK`. Capable relays
route the request toward the owning server and retain the old nickname until
its ordinary NICK acknowledgement returns. The owner validates the name,
notifies the client and channel peers, updates its nickname map/history and
broadcasts the new name. Collisions use at most ten bounded suffix attempts,
then release the enforcement target with KILL if none succeeds. This follows
the bounded collision policy in [Bahamut's SVSNICK handler](https://github.com/DALnet/Bahamut/blob/master/src/m_services.c).

For a next hop without SVSNICK support, nickname enforcement uses ordinary
services KILL rather than inventing a nickname unknown to the old owner.
Toward the services source, a locally decided KILL carries the deciding
server's prefix; other branches retain services authority. The mode profile
does not imply that a peer can execute SVSNICK. Requests are never returned
toward their incoming link. Upgrading every hop is required for an actual forced
rename rather than the legacy disconnect fallback.

Use `tests/ircd/services-legacy.js --legacy-source=/previous/exec/` with an
actual old source tree to test legacy services behavior. Add `--legacy-hub`
to put that version at the intermediate hub as well. `tests/ircd/rolling.js`
checks ordinary modes, membership, visibility, reconnects and hub authority
with real old hubs and leaves. Old implementations retain their own local
limitations; no new capability or profile grants them authority to replace
established state on a newer hub.

## Clock admission

`CLOCK ROLE HUB|LEAF` tells the receiver how the sender's configuration
classifies that receiver. This assertion cannot grant hub trust: the local
configuration supplies that independently. Only mutual hub links compare
their clocks. `CLOCK <nonce>` requests a fresh wall-clock sample, answered by
`CLOCK <nonce> <seconds.milliseconds>`. Each direction sends `CLOCK READY`
after its check. Neither direction sends its state burst before reciprocal
readiness. A hub withholds readiness from a returning leaf until incoming
hub mapping bursts have completed.

After admission, mutual hubs advertising `CLOCKPUSH` both send
`CLOCK -1 <seconds.milliseconds>` every 15 seconds, without requesting or
answering periodic probes. Each receiver compares the announcement with its
current wall clock; a difference beyond `[Clock] MaxSkew` (30 seconds by
default), including queue delay, closes the link. Announcements in both
directions are required: delay can conceal an ahead clock in one direction,
but increases its discrepancy in the reverse direction. Missing valid
announcements for more than 30 seconds also closes the link. Local callback
stalls restart the announcement timeout, as they do for probes.

Admission and links to peers without `CLOCKPUSH` retain numbered
request/response probes. Their skew bounds include the full round-trip
uncertainty; a reply is accepted only when the entire range fits within the
configured skew limit. Replies are not discarded merely for exceeding two
seconds. Services and leaf links remain exempt from skew enforcement.

## Corrective channel snapshots

Only a directly connected configured hub can send an authoritative
`CSYNC <network-ts> <channel>` transaction. Its phases are:

- `BEGIN <channel-modes-and-arguments>`
- `MORE :<member-list-with-@-and-+-status>` (repeatable)
- `BANS :<ban-mask-list>` (repeatable)
- `TOPIC <setter> <topic-ts> :<text>`
- `END`

The leaf applies the transaction at END. It replaces modes, status, bans and
topic state, notifying local clients of removals and additions. It retains
additional members pending in-flight JOINs. The hub echoes accepted leaf
MODE and KICK changes after previously queued corrections so those changes
are not lost. A leaf PART carries an opaque local user/action token. The hub
returns `TSMAP PARTACK <channel> <token>` after processing it; until then,
earlier snapshots cannot re-add the departing local user. PART and QUIT
precede retirement of an ended channel's mapping.

A standalone TOPIC correction acknowledges the hub's canonical
topic timestamp. TOPIC corrections use the implicit direct-server prefix to
fit maximum-length topics and channel names within the protocol line limit.

Hubs, services and leaves advertising `FULLCHAN` receive complete channel
snapshots, including secret and private channels with no local members.
`FULLCHAN` identifies the 2.5 client reply filtering; released 2.4 peers do not
advertise it. Hub authority over leaf assertions remains unchanged. Client
replies enforce channel privacy: outsiders do not receive secret/private NAMES, WHO, WHOIS
channel lists, modes or topics. LIST omits secret channels and anonymizes
private ones. Members and IRC operators retain the corresponding visibility.

For older untrusted leaves, hubs withhold hidden channels without local
members from both bursts and runtime JOIN/MODE/TOPIC/message traffic. The
first local JOIN triggers a complete authoritative snapshot. When the last
local member leaves, synthetic PARTs remove the cached remote members from
that leaf only. Public/hidden mode transitions use the same policy. This
prevents partial channel records and spurious operator errors without exposing
hidden state through old client reply handlers. An old leaf sends its initial
JOIN/NAMES replies before receiving hub correction; the full list follows
through the snapshot. Hubs serving older leaves must run the updated code.

## Replicated leaf timestamp mappings

Every process creates a random process identity. Each channel object gets a
process-local sequence identity and keeps its original raw timestamp identity
through corrections. The identities distinguish successive channel objects
created within the same clock second. Raw identity values are kept within
the protocol epoch range even when the leaf clock falls outside it.

A leaf sends `TSMAP BEGIN <process-id>` at the start of its burst. Before
each JOIN/SJOIN it sends `TSMAP CLAIM <process-id> <object-id> <raw-ts> <channel>`.
The claim identifies an object; it cannot supply the network timestamp.
Only a claim consumed by a valid channel introduction allocates a mapping.
One pending claim is stored per connection. `TSMAP END <process-id>`
reconciles the surviving objects, and `TSMAP DROP <process-id> <object-id>`
retires an ended object. Replacing a process retires its previous incarnation.

Hubs replicate the mappings using:

- `RESET <leaf> <process-id> <revision> <issuer>`
- `SET <leaf> <process-id> <object-id> <raw-ts> <network-ts> <channel> <revision> <issuer>`
- `END <leaf> <process-id> <revision> <issuer>`
- `DROP <leaf> <process-id> <object-id> <revision> <issuer>`

RESET/SET/END form an atomic inventory snapshot. Live changes use versioned
SET/DROP commands. Revisions are advanced by hubs, independent of leaf clocks.
Newer inventories prevent stale hub bursts from resurrecting ended objects.
Common surviving objects with concurrent mappings choose the earlier issued
network timestamp. A leaf cannot forge a replicated mapping or reuse an
active identity for another object.

Caches retain one process incarnation per leaf name and its surviving object
records across SQUIT. They are independent of the Server and Channel objects
that are destroyed on a split. A new hub receives cached records in its burst,
even for an offline leaf whose last channel is now absent. Thus a returning
leaf can move to that hub without receiving a fresh channel age. An existing
hub channel remains authoritative regardless of a returning leaf's mapping.

These caches are in memory. Handoff requires a surviving upgraded hub to hold
the mapping; simultaneous loss of all hub caches cannot preserve it.

## Trusted channel merges

Trusted hub JOIN/SJOIN timestamps are preserved through relays. Older channel
incarnations win. Equal-timestamp snapshots union flags and status, choose the
lexicographically greater key and the greater limit, and let secret mode
dominate private mode. Ban lists are unioned. Runtime MODE commands remain
normal setters. TS resets explicitly remove losing modes/status from local
clients before applying the winning state.

Sequential topic changes advance the channel's logical topic timestamp, even
within one second. Equal timestamps choose a stable setter/text ordering.
Empty topics retain a timestamp and are included in reconnect bursts.
