# SynchronetIRCd 2.4 synchronization

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

Untrusted older leaves receive ordinary JOIN/MODE corrections. Timestamped
MODE uses TS zero, so an older local channel age cannot block hub authority.
An old leaf can retain a different local channel age; its assertions cannot
replace an established hub channel. Its initial server MODE/TOPIC burst is
accepted only for channels it first introduces, while all members are routed
through that leaf. That exemption ends at BURST 0 or when another route joins.
Established channels receive hub state after the leaf's burst completes.
Legacy topic delivery uses a per-channel, per-leaf presentation counter to
outpace timestamps received from that leaf, without changing network time.

Mixed networks retain limitations in the old implementation. In particular,
old daemons discard empty TOPIC commands, so a topic clear cannot reliably
reach their clients. Old hubs also retain their original timestamp merging
and same-second topic behavior; full convergence and cached timestamp handoff
require upgrading the hubs on the affected path. Compatibility keeps links
and supported operations working; it cannot repair code executing on old
peers. Use `tests/ircd/rolling.js --legacy-source=/previous/exec/` with a
separate previous-version source tree to test actual mixed processes.

## Clock admission

`CLOCK ROLE HUB|LEAF` tells the receiver how the sender's configuration
classifies that receiver. This assertion cannot grant hub trust: the local
configuration supplies that independently. Only mutual hub links compare
their clocks. `CLOCK <nonce>` requests a fresh wall-clock sample, answered by
`CLOCK <nonce> <seconds.milliseconds>`. Each direction sends `CLOCK READY`
after its check. Neither direction sends its state burst before reciprocal
readiness. A hub withholds readiness from a returning leaf until incoming
hub mapping bursts have completed.

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

Hidden channel snapshots and topics are withheld from untrusted leaves
without members. A later JOIN and burst completion trigger catch-up snapshots.

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
