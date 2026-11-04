# External IRCServices compatibility audit

Audited 2026-11-04 against IRCd 2.10, commit `6438bb9448`, and the stock
IRCServices 5.1.24 Bahamut module and its callers. This is an interoperability
inventory and work list; it does not claim complete Bahamut support.

## 2.11 implementation status

The inventory below records the 2.10 audit baseline. Version 2.11 implements
the Fix first, Low-hanging fruit and Useful additions work, including the
follow-up identity, channel-policy and S-line scope:

- Argument-safe mode filtering, exact owned AKILL/RAKILL state and expiry,
  ordinary Bahamut `+e`, and routed SVSNICK with a legacy KILL fallback.
- Routed SVSMODE `r/d/a`, independent identification/stamp/admin state,
  trusted hub snapshots, authority-scoped reconnect stamps and first-party
  TLS presentation. Leaf-supplied recovery hints cannot authenticate a user.
- Ordinary `+I` invite masks, list numerics, matching, clear operations,
  negotiated relay/burst and authoritative snapshot reconciliation.
- Channel `r/R/M/O/c/j/S` storage and enforcement, negotiated old-peer
  filtering, and admission/message checks at newer gateways to old subtrees.
- Native owned SGLINE/UNSGLINE realname bans and SQLINE/UNSQLINE nickname
  reservations, local/remote enforcement and explicit runtime lifecycle.
- Embedded NickServ registered-nickname markers, ChanServ registration markers,
  and extended MLOCK support, preserving BBS proof and mailbox boundaries.

See [the 2.11 protocol rules](PROTOCOL.md#services-additions-in-211) for
`Protocol=bahamut`, new capabilities, lifetime/trust rules and old-peer limits.
Configured old hubs retain their existing timestamp authority. No capability
grants a leaf authority over an established channel. Unknown TLS fails closed;
leaf reconnects require fresh services identification. S-lines have no wire
expiry and end when their authority departs; IRCServices must republish active
policy after reconnect. SQLINE channel reservations, wildcard bulk S-line
deletion, deliberate empty-channel SJOIN resets, and CSSetChannelTime remain
outside this scope. The published 2.10 commit is unchanged.

### Legacy leaf compatibility requirement

The supported rollout topology has every hub running the latest IRCd. Version
1.3a and other older daemons connect as leaves, configured without hub or
services authority on their upstream hub. Acceptance requires ordinary leaf
chat, channel operations, services access and reconnects to work as well as
their protocol permits, while the current hubs protect the rest of the network.

Legacy timestamps, modes, status claims and service-looking input must not
replace hub-owned channel state, acquire authority over other routes, fabricate
identification/TLS proof, or break unrelated links. Unsupported features may
remain limited on the old leaf; upgraded hubs enforce the available policies
at that boundary. A limitation confined to the leaf, such as 1.3a's missing
RAKILL handler, is distinct from corruption of current hub state.

The default services/policy legacy tests use current hubs and an actual old
leaf. Old-hub and mixed-hub runs below are supplemental upgrade observations;
full operation through an old hub is outside this compatibility requirement.

### 2.11 validation

Validation uses isolated configuration, private BBS/SQLite files and loopback
sockets. The final named-check results are recorded below; TLS integration
scenarios additionally exercise real client/server handshakes and BBS proof.

| Coverage | Checks |
| --- | ---: |
| Thirty-one native JSexec suites, including SQLite-backed embedded services and fresh-review regressions | 558 |
| Six-server integration network | 30 |
| Services through two upgraded hubs to an upgraded leaf | 11 |
| External identity, policies, S-lines and reconnect/burst state across native relays | 9 |
| Real native FileServ across protected-channel boundaries and ordinary-client impostor rejection | 2 |
| Rolling networks with actual 1.3a, 2.0, 2.3 and 2.7 hubs/leaves | 36 |
| Services with 1.3a hub/leaf paths, a 2.0 leaf, a 2.3 hub/leaf path and a 2.7 leaf | 20 |
| New policy boundaries with actual 1.3a, 2.0, 2.3 and 2.7 leaves, through both upgraded and same-version legacy hubs | 48 |
| Embedded BBS identity, ChanServ, memos and restarts through an actual 2.3 relay | 24 |

The TLS route suite verifies embedded identification and extended mode locks
across native hubs, successful TLS-only admission, rejection of plaintext
clients, logout revocation, and unknown legacy-hop security. The embedded
services network suite separately checks BBS proof/RPC through an actual old
relay. The native runtime includes Synchronet's real SQLite API; no JavaScript
replacement database was used.

Fresh-review regressions cover exact rejected JOIN/SJOIN corrections for hidden
channels, native FileServ identity/route and conflicting-transport checks,
byte-preserving S-line canonicalization, and conditional SVSMODE/SVCSTATE
translation between canonical and original leaf nickname timestamps. They
include stale/replaced link and user objects, malformed timestamps, renames,
revocations, omitted/zero timestamps, and preservation of separate BBS proof.
The live tests verify private/secret rejected joins leave no local or canonical
ghost membership. Pre-2.5 client reply visibility through an old hub retains
that version's historical limits; the correction does not add a channel view.

The unmodified legacy trees came from commits `08e1a9d98edb` (1.3a, last tree
before the 1.9 version change), `143efec68b09` (2.0, last tree before 2.1),
`7a70ef225a` (2.3) and `8ed7eed5af` (2.7). The original 2.0 RC
snapshot `f7ba543df0` could not start due to its historical configuration
`fname` error; it was not patched or counted as a successful test.
The 2.0 runs exposed its TS-zero MODE parser defect and verified the
timestamp-free correction dialect, including founder ops and reconnects.

Version 1.3a remains in this validation matrix. It accepts AKILL but has no
RAKILL handler or automatic expiry: a timed ban expires on upgraded servers
while remaining on 1.3a until cleared locally or that server is upgraded.
The compatibility checks explicitly record this historical limit rather than
claiming remote removal works. Its queued closing ERROR text may also be lost;
KILL/S-line tests verify socket closure and absence from upgraded state instead.

The local FreeBSD JSexec was run with per-process ASLR disabled after the
debugger confirmed intermittent allocator stalls in JS_NewContext before any
script ran. This was a test-runtime workaround; legacy source was unchanged.

## Evidence and scope

The local source tree is `ircservices-5.1.24`. Its archive SHA256 is
`63c88b84ddcd12f3636ca9c69987a06e4055156ae3ebe2ff4f22205c66c2a365`.
Source references below are relative to that tree unless prefixed with
`load/ircd/`.

A read-only connection through `irc.synchro.net` confirmed that NickServ
belongs to `services.synchro.net`. Its VERSION reply identified
`ircservices-5.1.24`, build #8, compiled 2023-11-10. The selected IRC endpoint
was running IRCd 2.9, so production observations establish the deployed
services' identity and advertised commands, not the behavior of IRCd 2.10.
The deployed binary's local patches and configuration have not been inspected.

Live HELP observations:

- ChanServ advertises CLEAR EXCEPTIONS and CLEAR INVITES, as well as ordinary
  mode locking, clearing modes/status, INVITE, UNBAN and mass kicks.
- NickServ advertises registration, identification, kill protection, RECOVER,
  RELEASE and GHOST. HELP does not establish whether NSForceNickChange is set.
- MemoServ advertises notifications on logon, new memos and returning from AWAY.
- StatServ advertises server and user statistics.
- OperServ HELP denied the unprivileged probe. The operator supplied its
  command list, confirming AKILL, AKILLCHAN, SGLINE, SQLINE, SZLINE, session
  limits, clone kills, channel controls, GLOBAL, JUPE and administration.

Only informational requests were sent to production. No users, channels,
registrations, modes or bans were used as production test targets.

The manual probe in `tests/ircd/services-protocol-probe.sjs` exercises the
real JavaScript handlers with isolated in-memory state and the Bahamut
capability string. Its 16 output records describe observations, rather than
asserting that missing features should stay missing. Run with a configured
JSexec and the repository load path, for example:

```sh
jsexec -n -c /path/to/test/ctrl -i load tests/ircd/services-protocol-probe.sjs
```

## Protocol coverage

| Wire feature | IRCd 2.10 status | Services feature affected |
| --- | --- | --- |
| PASS, CAPAB, SERVER, SVINFO | Supported negotiation; services exempt from Synchronet CLOCK admission | Connection and reconnect |
| NICK introduction/rename, QUIT | Supported, including reserved service bots; services stamp lost | Bots, nickname holding/release; split recovery gap below |
| Numeric replies | Generic client routing exists | WHOIS, INFO, MOTD, STATS, TIME, VERSION, nickname warnings |
| PRIVMSG and private NOTICE | Supported | Service commands, replies, memos, news |
| NOTICE `$*.domain`, GLOBOPS | Supported | GLOBAL and operator notices |
| JOIN, PART, INVITE, KICK, KILL | Supported, including services authority through relays | Access enforcement, INVITE, GHOST, kill protection, sessions, clone kills |
| MODE `biklmnpstov` | Supported, including TS zero and complete services corrections | Ordinary MLOCK, access/status changes, bans, CLEAR |
| MODE `e` | Native support exists, but ordinary Bahamut traffic is filtered out | CLEAR EXCEPTIONS and exception tracking |
| MODE `I` | Missing; its argument can misalign later modes | Invite masks and CLEAR INVITES |
| MODE `j` | Missing; its argument can misalign later modes | Join rate MLOCK |
| MODE `rRcOMS` | Missing channel modes | Registration and extended MLOCK policies |
| TOPIC with saved setter/time, including empty text | Supported services override and forwarding | Topic retention and topic locks |
| SJOIN with members; user-prefixed SJOIN | Supported | Registered-channel status correction and membership |
| Empty-member SJOIN; timestamp replacement | Not implemented as Bahamut reset semantics | CLEAR USERS pre-reset; CSSetChannelTime |
| AKILL/RAKILL | Enforces bans, but identity/removal and lifetime are incomplete | AKILL, AKILLCHAN, SZLINE, optional clone autokills |
| SGLINE/UNSGLINE, SQLINE/UNSQLINE | No handlers or forwarding | Native realname bans and nickname reservations |
| SVSNICK | No handler or forwarding | Forced guest renames, including optional kill protection and SQLINE handling |
| SVSMODE `r`, `d`, `a` | Explicitly ignored | Identified state, services stamp, services-admin marker |
| SERVER/SQUIT, including JUPE | Supported with 2.10 authority/routing fixes | Network topology, services departure/restart, JUPE |
| AWAY and clearing AWAY | Forwarded, including reconnect state | MemoServ return-from-away notification |
| IPv4 NICKIP | Supported numeric representation | IP-based policy checks |
| IPv6 NICKIP | Bahamut's numeric IPv4 dialect has no representation; sent as zero | IPv6 visibility to this services protocol |

Shared output paths were traced through `send.c`, `actions.c`, `messages.c`,
`main.c`, `init.c`, the Bahamut module and its included helpers, and the
NickServ/ChanServ/OperServ/MemoServ/StatServ/misc modules. OperServ RAW can
emit arbitrary text; that is not a finite set of additional protocol features.

## Correctness repairs to do first

### 1. Consume unsupported mode arguments before filtering or applying modes

`load/ircd/exceptions.js` knows argument counts for `bekov` and `+l`, but not
Bahamut's `I` or `+j`. `load/ircd/channel.js` also ignores those letters
without consuming their parameters. Both filtering and batching need the
same dialect-aware argument schema.

Confirmed example, with Target and Owner in the channel:

```irc
:ChanServ MODE #audit 0 +jko 5:10 secret Target
```

The current handler sets the channel key to `5:10`, fails to op Target, and
relays a truncated request missing Target. A separate `+Iov` probe voices
Target instead of the intended Owner and fails the intended op grant.
Unsupported features must not corrupt supported operations in the same line.

A bounded repair can recognize and consume the Bahamut argument grammar
without implementing the missing policies. Strip an unsupported operation
with its own argument; reject ambiguous input when the grammar is unknown.
Test incoming MODE/SJOIN and outgoing mode filtering/batching together.

Sources: `modules/protocol/bahamut.c:new_chanmodes`,
`load/ircd/exceptions.js:IRC_exception_filter_modes/IRC_chanmode_batches`,
`load/ircd/channel.js:IRCClient_set_chanmode`.

### 2. Give services bans their own identity and removal rules

AKILL uses `isklined()` to decide whether a ban exists; RAKILL uses that same
enforcement lookup and `remove_kline()` to remove a matching entry. These
are wildcard matching operations, not exact ban identity checks.

Confirmed example: a local `*@*.test` K-line suppresses creation of a
services AKILL for `bad@bad.test`. A later `RAKILL bad.test bad` deletes the
broader local K-line. Other users covered by the local policy become unbanned.

Use exact canonical mask identity and services ownership/type for updates and
removal. Keep wildcard matching for deciding whether a client is banned.
Preserve local K-lines and independent, overlapping services entries.

The same work should address lifetime: incoming creation time and duration
are relayed but not retained or enforced. A one-second AKILL remains active
after its expiry unless another action removes it. REHASH clears the K-line
array as well. Define runtime-ban preservation and expiry independently of
services successfully sending every RAKILL.

Sources: `modules/protocol/bahamut.c:do_send_akill/do_cancel_akill`,
`load/ircd/server.js:AKILL/RAKILL`, `load/ircd/core.js:isklined/remove_kline`,
`load/ircd/config.js:KLine/Clear_Config_Globals`.

### 3. Bridge ordinary ban exceptions to the external dialect

Bahamut registers `+e` and its exception callbacks, but does not advertise
Synchronet's `CHANEXCEPT` extension. The IRCd currently drops incoming `+e`
from it and omits outgoing exception changes and reconnect lists. All three
paths were reproduced. Live ChanServ HELP advertises CLEAR EXCEPTIONS.

Distinguish ordinary ban exceptions from Synchronet's extended message-bypass
exceptions. Support plain `nick!user@host` exceptions for a known Bahamut peer
while retaining explicit negotiation for extended exception types. U-line
authority alone does not identify a peer's mode dialect.

Sources: `modules/protocol/banexcept.c`,
`modules/protocol/bahamut.c:init_modes/do_send_server`,
`load/ircd/exceptions.js:IRC_has_chanexcept`,
`load/ircd/channel.js:IRC_send_chanmode`,
`load/ircd/server.js:MODE/IRCClient_server_chan_info`.

## Additions with useful user-visible results

### Forced nick changes

The Bahamut module enables `PF_CHANGENICK` and emits
`SVSNICK <oldnick> <newnick> :<timestamp>`. The IRCd logs it as unknown and
does not relay it. When enabled, NSForceNickChange uses this instead of KILL.
SQLINE handling also uses it when SQlineKill is disabled. Help advertising
RECOVER alone does not prove that the deployed service uses forced renames.

This is a contained feature, but requires the owning server to perform the
normal nickname transition and broadcast the resulting NICK. Validate the
services source, route toward the user's owner, handle collisions and invalid
nicknames, update history, and avoid rerouting loops. Test the full services
rename followed by enforcer introduction, release and client notification.

Sources: `modules/protocol/svsnick.c`, `modules/nickserv/collide.c`,
`modules/operserv/sline.c`, `load/ircd/server.js:default/NICK`.

### Services identification and reconnect stamps

SVSMODE is silently ignored. The inspected senders use `r` for identified
state, `d <number>` for the services stamp, and `a` for services-admin status.
NICK introductions discard an incoming stamp; outgoing NICK always contains
zero. Thus services cannot receive its previously assigned stamp on reconnect
and use the intended stamp-based NickServ split recovery.

These need explicit fields and a defined external-services representation.
Synchronet already uses `r` for rejected-client operator notices and `d` for
debug notices. Feeding SVSMODE into its existing user-mode parser would apply
the wrong semantics. External identification must also remain distinct from
proof of a Synchronet BBS account used by embedded services.

Sources: `modules/protocol/bahamut.c:do_user_servicestamp_change/do_user_mode`,
`modules/nickserv/util.c:validate_user/set_identified`, `users.c:do_nick`,
`load/ircd/server.js:NICK/SVSMODE/IRCClient_server_nick_info`,
`load/ircd/user.js:USERMODE_CHAR`.

### Remaining channel policies and TLS presentation

| Mode | Bahamut meaning | Additional behavior needed |
| --- | --- | --- |
| `I` | Invite mask list | Storage, list replies, burst/relay and join exemption |
| `j` | Join rate limit | Parameter validation and actual per-channel rate enforcement |
| `r` | Registered channel | Services-controlled marker and lifecycle |
| `R` | Identified users only | Identified-state integration and join checks |
| `M` | Moderated for unidentified users | Identified-state integration and message checks |
| `O` | Operators only | Join checks |
| `c` | No color codes | Message checks |
| `S` | Secure clients only | Client transport presentation and join checks |

ChanServ can itself kick users for some locked `R`/`O` policies while services
is available; that does not supply native channel-mode enforcement. The
Bahamut `S` callback checks user mode `+S`. Synchronet currently sends neither
that marker nor its CLIENTTLS extension to this non-CLIENTTLS peer. A probe
with known TLS transport produced a NICK without `+S`. Consequently, a
ChanServ secure-only mode lock can reject a genuinely TLS-connected user.
Bridge from the existing first-party transport observations, preserving
unknown/conflicting states rather than treating them as secure.

Sources: `modules/protocol/bahamut.c:new_chanmodes/do_check_kick`,
`modules/chanserv/check.c:check_kick`, `load/ircd/client-security.js`,
`load/ircd/channel.js:MODE`, `load/ircd/unregistered.js:ISUPPORT`.

### SGLINE and SQLINE

The deployed OperServ lists both features. None of SGLINE/UNSGLINE or
SQLINE/UNSQLINE is recognized or relayed by the IRCd. Services can still
perform some reactive enforcement using KILL while connected; its SQLINE
rename path depends on the missing SVSNICK support and configuration.

Realname bans and nickname reservations need separate native stores,
enforcement, exact removal and a reconnect/rehash policy. Existing local
Q-lines are a possible starting point for SQLINE; keep their ownership
separate from services reservations. SZLINE already uses AKILL/RAKILL in this
module, with the ban lifecycle defects above. IPv6 visibility cannot be fixed
by passing an IP string to the old numeric-only NICKIP parser.

Sources: `modules/protocol/bahamut.c:do_send_sgline/do_send_sqline/do_send_szline`,
`modules/operserv/sline.c`, `load/ircd/core.js:IRCClient_check_nickname`,
`load/ircd/server.js:IRC_nick_ip/default`.

## Synchronization decisions to keep explicit

The Bahamut SJOIN helper emits an empty-member, older-TS SJOIN before CLEAR
USERS. The IRCd currently ignores it and does not forward it. The subsequent
explicit ban and KICK messages still perform the mass kick; this is a missing
reset step, not evidence that every CLEAR USERS operation fails.

CSSetChannelTime similarly asks to replace channel age with registration time.
The services timestamp protections from `6bd095d0` deliberately preserve
existing channel state. Implement a separately specified services reset/age
operation if needed; restoring generic older-TS wipes would reintroduce the
op-desynchronization problem.

Outgoing channel bursts use JOIN/MODE for services even when it advertises
SSJOIN. IRCServices creates a channel at its current time on a bare JOIN;
its MODE parser consumes but does not apply the timestamp to channel age.
An SSJOIN burst can convey the existing age and initial status together.
Test this with the actual services daemon before changing the established
burst dialect, including registered channels and CSSetChannelTime.

Sources: `modules/protocol/sjoin.c:sjoin_clear_users/sjoin_channel_create`,
`channels.c:chan_adduser/do_cmode`,
`load/ircd/server.js:SJOIN/IRCClient_server_chan_info`,
`load/ircd/channel.js:IRCClient_set_chanmode`.

## Suggested implementation order

1. Correct mode argument handling and services-ban identity/removal. These
   prevent existing commands from changing unintended state.
2. Bridge ordinary `+e`, including changes, removals and reconnect lists.
   This completes a feature already implemented natively and advertised live.
3. Add SVSNICK with routed ownership and full nickname lifecycle tests.
4. Design external identification/stamps and TLS mode presentation together
   with the mode-letter collisions and embedded-identity boundary.
5. Add invite masks and selected channel policies, then native S-lines and
   explicitly defined SJOIN/burst semantics.

OperServ's database management, operator/admin lists, session exception lists,
news storage and process controls are services-side features. They do not
each require a new IRCd command. Keep native additions tied to the wire
operations and enforcement responsibilities identified above.
