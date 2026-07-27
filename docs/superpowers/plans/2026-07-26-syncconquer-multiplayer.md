# SyncConquer intra-BBS multiplayer — findings & plan sketch

**Status:** design not finished. Network play is **disabled in the shipped
door** until this is built (see "Interim state" below). This file exists so the
work can be picked up cold.

**Goal:** let callers on the same BBS — nodes that may be spread over more than
one host — play SyncAlert / SyncDawn against each other, using Vanilla
Conquer's own lockstep netcode.

**Source design:** `src/doors/syncconquer/DESIGN.md` ("Multiplayer (v1:
strictly intra-BBS)"). Federation between BBSes stays out of scope
(`DEFERRED.md`).

---

## The lesson: the game already has a lobby — what it lacks is discovery

The starting assumption should be that **no new lobby UI is needed**. Red
Alert and Tiberian Dawn each ship a real one in `netdlg.cpp`
(`Net_New_Dialog` / `Net_Join_Dialog`, reached through `Remote_Connect()`):
game list, player roster, side and colour picking, scenario and rules
selection, chat, and a start button. It also does the version and scenario CRC
checks that keep a lockstep game from desyncing. Reimplementing any of that
BBS-side would be work spent to end up somewhere worse.

What the engine cannot do inside a door is **find the other nodes**. That is a
transport limitation, not a UI one, and it is the whole of the problem:

- `UDPInterfaceClass::Open_Socket()` (`vanilla/common/wspudp.cpp:197`) binds
  `INADDR_ANY` on `PlanetWestwoodPortNumber` — one fixed global port,
  `1234` by default (`common/internet.cpp:24`). **Two door processes on one
  host collide on that bind**, so the second one fails. Since most BBSes run
  every node on a single host, this breaks the common case, not the exotic one.
- Peers are addressed by a bare 4-byte IPv4 address (`packet->Address + 4`),
  and every outbound datagram is sent to `PlanetWestwoodPortNumber` on the
  destination (`wspudp.cpp:468` Win32, `:575` POSIX). There is nowhere in an
  address to put a per-node port.
- Discovery is by broadcast — `255.255.255.255` plus each interface's
  broadcast address (`wspudp.cpp:180`, `:273`). That does not cross subnets, so
  it cannot reach a node on another host in the general case, and it cannot
  distinguish two nodes that would need different ports on one host.

So something outside the engine must hand each door a peer list of
**address *and* port**. That is the entire integration surface.

`PlanetWestwoodPortNumber` is a plain mutable global, which is the hook: the
door can set it per node before the socket opens.

### Two shapes for where the peer list comes from

1. **Door-side registry (C only) — recommended.** Each running door publishes
   an entry (node number, advertise address, port, alias) under
   `data/<game>/`, expiring it on exit, and the transport unicasts to every
   live peer instead of broadcasting. Nothing changes for the player: they
   enter the door, choose Multiplayer, and the game's own lobby lists what the
   other nodes announced. Configuration lives in the door's existing
   `syncalert.ini` / `syncdawn.ini`. The door can page other nodes from C — it
   already does node messages and status.
2. **BBS-side JS lobby**, as SyncDuke and SyncDOOM do it. Buys pre-launch
   visibility (see a game forming without spending a door session) and reuses
   `exec/load/game_lobby.js`, which **already implements** the `[net]`
   `bind`/`advertise` split, the per-host `[net:<hostname>]` overlay, port
   allocation from a range, the file registry, and an N-player muster. Costs a
   second UI and a second place for settings to live.

These are not exclusive: the registry is a file either way, so a JS lobby can
be layered over shape 1 later without redesigning it.

### Configuration (decided)

Two independent addresses, the split `game_lobby.js` already uses:

- `bind` — the local interface the door's socket accepts on.
- `advertise` — what peers on *other* hosts dial, recorded in the registry.

Both blank means loopback: same-host play only, and that is the **default**,
because most BBSes run all nodes on one host. A per-host override section
(keyed on the host name) lets one shared ini serve several hosts — the doors'
config files are typically on a shared mount.

### Work this implies

- Vendored transport patch (`common/wspudp.cpp`): per-node bind port, a
  configurable bind address, a port carried per peer rather than one global,
  and a seeded unicast peer list in place of the broadcast addresses. The
  6-byte packet address has 4 leading bytes (the IPX "network number") doing
  nothing — the obvious place to carry the port. Record every edit in
  `PROVENANCE.md`.
- Door-side peer registry + expiry, and the `[net]` config block.
- Whatever it takes to enter `Remote_Connect()` cleanly from the door and to
  survive a caller dropping mid-game (RA has drop handling; time-limit expiry
  is the BBS-specific case).
- Both titles: RA and TD have parallel `netdlg.cpp` / `ipxmgr.cpp` copies.

### Open questions

- Does a caller need to see a game forming *before* entering the door? That is
  the only real argument for shape 2, and it decides whether a JS lobby is in
  scope at all.
- Drop and time-limit policy: what a caller sees when their BBS time expires
  mid-match, and what the remaining players see.
- Cross-host determinism is assumed but unverified (`DEFERRED.md`); identical
  door version and asset set must be enforced at join.

---

## Interim state: network play is turned off in the door

Until the above exists, choosing Network in the multiplayer menu leads to the
engine's own LAN game-setup, which cannot find peers for the reasons above —
an option that can only disappoint. The button is therefore suppressed and the
menu offers **Skirmish** alone.

Skirmish is *inside* the multiplayer menu in both games, and it is the lone
caller's whole game, so the menu entry itself must stay — only the network
option goes.

Reversing this when the work lands is deleting one compile definition.

## Traps worth keeping

- **`grep` treats several vendored sources as binary.** `vanilla/redalert/`
  `netdlg.cpp` silently reports nothing without `-a`, which reads as "this
  function does not exist" when it plainly does. Use `grep -a` on `vanilla/`,
  and confirm a negative result some other way (`nm` on the built object) before
  believing it.
- **`USE_RA_AI` is already defined** (`vanilla/tiberiandawn/defines.h:56`), so
  TD's skirmish opponents are the Red Alert base-building AI. Checking only the
  CMake files says otherwise and is wrong.
- **Don't give each node its own `127.0.0.x` instead of its own port.** It is
  tempting, because the engine addresses peers by IP alone and that would need
  no transport patch at all — and on Linux it works, since the kernel routes
  all of `127.0.0.0/8` to loopback and any of those addresses binds with no
  setup. It is not portable: Windows assigns only `127.0.0.1` to the loopback
  interface, so binding `127.0.0.2` fails outright, and the BSDs (macOS)
  need an explicit `lo0` alias per address. The door ships on Windows.
  Per-node ports are the portable answer.
- **`NETWORKING=OFF` is not a free way to remove network play.** It also drops
  `WINSOCK_IPX`, which re-enables the legacy `Winsock` branches in both games'
  `winstub.cpp` — and that object exists only in `common/fakesock.h`, which is
  wrapped in `#ifndef _WIN32`. The Windows build would fail exactly as
  `PROVENANCE.md` patch #32 describes. Suppress the menu entry instead.
