# syncdoom multiplayer — design & as-built notes

Status: **built and working.** Co-op and deathmatch run over a Synchronet install (one or
more hosts that share the install), with registry-based discovery of joinable games. This
file began as the pre-build design and is kept as the rationale of record; the
configuration sections have been reconciled with what actually shipped.

> **Config source of truth:** the live, documented key list is
> `xtrn/syncdoom/syncdoom.example.ini` (installed as `syncdoom.ini` beside the door).
> Shipped `[net]` keys: `advertise`, `bind`, `port_low`/`port_high`, `max_games`,
> `max_players` (cap **4** = Doom's `MAXPLAYERS`), `idle_timeout`, `stale`,
> `allow_external`. Section name separators are **colons** — `[wadset:doom2]`,
> `[net:<hostname>]` — not dots.
> **Deferred / not built:** inter-BBS federation (trusted-peer public games —
> `public_advertise`, `federate`, `federation_secret`), `net_query` LAN-broadcast
> `discovery`, and LAN-address auto-detection.

## Foundation — the netcode is half-present

doomgeneric is derived from chocolate-doom but **stripped the net transport** to stay
single-player/portable. In this tree:

- The game loop (`d_loop.c`) is already net-aware — it calls chocolate's client/server
  API (`NET_CL_SendTiccmd`, `NET_CL_Run`, `NET_SV_Run`, `net_client_connected`).
- All `net_*.h` headers are present; the `.c` implementations were removed:
  `net_client.c`, `net_server.c`, `net_io.c`, `net_packet.c`, `net_query.c`,
  `net_loop.c`, `net_dedicated.c`, `net_structrw.c`, and a transport.

So MP is a **restore**, not a rewrite: pull those `.c` files at doomgeneric's
chocolate-doom base revision, and replace chocolate's SDL_net transport with one built
on **xpdev sockets** (`net_udp.c` over our `sockwrap`) so it stays SDL-free and
Windows-portable.

## Existing Doom MP code — what we reuse vs. reference only

There is **no drop-in, embeddable multiplayer *lobby*** to lift from the Doom world. Split
it into two layers:

- **Reuse — Chocolate's `net_query.c` (discovery primitive).** Vanilla/Chocolate has no
  lobby UI, but `net_query.c` is real **server discovery**: a **LAN-broadcast probe**
  (`-search`) and a **master-server query** (`-servers`), each returning running servers'
  metadata (players, map, mode, name). Its header is **already vendored** (`net_query.h`),
  and it's lockstep-compatible because it's part of the same net layer we're restoring — so
  it comes back with the rest of the net `.c` graft. It is *discovery*, not a menu, but
  it's the vanilla-correct data source a lobby sits on. Relevant to multi-host LANs: its
  broadcast mode can discover cross-host servers **without** the shared-file registry — a
  legitimate fallback/supplement where `data_dir` isn't actually shared. (We default to the
  file registry because enumerating many per-match UDP ports by broadcast is fiddly.)
- **Reference only — external launchers.** The actual "lobbies" of the Doom world are
  **standalone GUI launcher apps** that query a master server then exec the engine with the
  right args: **Doomseeker**, **IDE**, **ZDL** (ZDoom/GZDoom/Zandronum), **Odalaunch**
  (Odamex). Architecturally that **browse-list → launch-with-args** flow is exactly ours,
  so they're a good design reference — but they're Qt desktop apps tied to ZDoom-derived
  engines and their own protocols; **nothing liftable into a C text-mode BBS door**, and
  ZDoom-family netcode doesn't graft onto vanilla/Chocolate lockstep anyway.

Net: build the **lobby UI fresh** (a BBS text menu — no Doom-world equivalent embeds), but
get **discovery** from `net_query.c` and/or the `data/syncdoom/games/` file registry rather
than writing it from scratch.

## Topology — why lockstep works on a BBS

One **dedicated (headless) server** process coordinates a match; each player's door
connects as a **client** over UDP. On a single-host BBS that traffic is **loopback**
(127.0.0.1); on a multi-host BBS it crosses the LAN (see *Network topology* below). Either
way it's a short, fast hop — the players' high-latency BBS connections are not in this
path.

Doom is deterministic **lockstep**: each tic, every node submits its player's `ticcmd`
(inputs); the server distributes them; everyone simulates identically.

The enabler: **the door builds a ticcmd from the *current* key state every tic — it never
blocks on the user's connection.** Keystrokes update the held-key state asynchronously;
frames go out non-blocking/dropped. So a high-latency user has input lag *for themselves*
but does **not** stall the shared sim, which runs at localhost speed. MP feels like
single-player latency, per user, in a shared world.

## One dedicated server **per match** (on-demand)

Not a grid of "wad × mode" daemons — wad/mode/skill/maps are **properties of the match**,
chosen at creation.

1. **Create** — door spawns one dedicated server on a free localhost UDP port; the server
   writes and owns its registry entry.
2. **Join** — other players' doors connect to that server's port (everyone, incl. the
   creator, is a client).
3. **End/crash** — reap the process; its registry entry is removed (or pruned by heartbeat).

N concurrent matches = N server processes (each its own port). Headless servers are tiny
(just tic relay) so several at once is cheap; the lobby caps max concurrent games.

Dedicated > listen-server (a player hosting): the match **survives the creator dropping**.

Match-based (vanilla assigns player slots at game start; no mid-game join): gather in a
lobby → start → play to completion → server dies. Persistent "rooms" (a server that loops
back to a lobby between games) are a possible later layer; start with on-demand.

**No mid-game join — and what we do instead.** Doom's deterministic lockstep assigns player
slots at GAMESTART and simulates from tic 0 with no game-state transfer, so a player *cannot*
join a match already in progress (vanilla rejects it; there's no snapshot to sync a latecomer
from). The first live-player test surfaced the trap this creates: a creator sits alone in the
waiting room, presses Start not realizing they should wait, and lands in a one-player "co-op."
Mitigations (as built): the dedicated server **drops its registry entry the moment the match
goes in-progress** (so Browse only ever lists *joinable* games), the match **auto-starts when
full**, and the waiting room **refuses a manual Start while the controller is alone** in a
multi-player match (`num_players < 2 && max_players > 1`) — showing "waiting for another
player… (to play by yourself, pick Play single-player)" instead. An explicit 1-player match
(test/solo) still starts immediately.

## Server lifecycle & host-drop

Because the server is a **separate process from any player's door**, the creator
("host") leaving is just one client disconnecting — the match plays on. That only holds
if two things are true:

1. **The server must outlive the door that spawned it.** The creator's door launches the
   dedicated server, but the server must be **detached** from it — not a child that dies
   with its parent. On *nix: `fork()` + `setsid()` (double-fork) so it reparents to init
   and leaves the door's process group, so a SIGHUP / group-kill when the door exits
   doesn't take it down. On Windows: spawn `DETACHED_PROCESS` in its own job, not tied to
   the door. If we skip this, "host disconnects" silently becomes "game over" — the very
   thing the dedicated-server topology exists to prevent.

2. **A match must self-terminate when it empties**, or ports/processes leak. The server
   shuts down (and deletes its registry entry) when player count hits **zero**, or after a
   no-clients idle timeout. Optionally also end a match that drops below a configured floor
   (e.g. a DM with one player left). Vanilla assigns player slots at game start with **no
   mid-game join**, so a dropped player's slot just goes idle until the server decides to
   end the match — the remaining players keep playing in the meantime.

So: **host drop ≠ game over** by design, but it's load-bearing on (1) detaching the
server correctly and (2) the empty-match shutdown rule. Both belong to the dedicated
server, never to the creator's door.

## Game modes — already plumbed (vanilla)

- **Co-op** — default (`deathmatch=0`): shared maps, fight monsters together.
- **Deathmatch** — `-deathmatch` (DM 1.0: items + weapons respawn).
- **Altdeath** — `-altdeath` (DM 2.0: weapons stay).
- Modifiers: `-skill N`, `-warp <map>`, `-respawn`, `-fast`, `-nomonsters`.

The match creator picks these; the lobby passes them to the dedicated server.

## Cross-node discovery — a shared registry in `data/`

All nodes share `data_dir`, so games are discovered via the **filesystem**, with no
inter-door networking and no central matchmaker.

`<data_dir>/syncdoom/games/` — **one file per live match**, named
`<hostid>-<port>.ini`, e.g. `bbs-telnet-20001.ini`. The filename is **host-qualified**
because the registry dir is shared across hosts and two hosts can independently allocate
the same port from the `port_low`..`port_high` range — `<port>.ini` alone would collide. `<hostid>` + `<port>`
is unique BBS-wide (one server per port per host) and matches the `hostid`/`port` fields
inside:

```
host       = SysopHandle     ; creator's alias (display only)
wadset     = freedoom2       ; the [wadset:*] id; the joiner looks up the files locally
mode       = coop            ; coop | deathmatch | altdeath
addr       = 10.0.0.6        ; address joiners connect to (see Network topology)
port       = 20001           ; the dedicated server's UDP port
hostid     = node-name       ; which host the server runs on
players    = 2
maxplayers = 4
status     = lobby           ; lobby | playing
pid        = 12345           ; dedicated-server pid (liveness/cleanup)
heartbeat  = 1718563200      ; Unix epoch seconds; bumped every ~3s
```

The server is the **single writer** of its own entry; it writes exactly these fields (see
`mp_write_registry`). The map and skill aren't stored — they're the creator's command-line
choice at launch, not part of discovery; the joiner inherits the match by connecting, and
resolves WADs from the `wadset` id.

Flow:

1. **Create** → server writes/owns its file and heartbeats it.
2. **Browse** (any node) → door scans `games/*.ini`, lists the live ones (status=lobby,
   not full, fresh heartbeat) in a menu.
3. **Join** → door connects to `<addr>:<port>` (or `127.0.0.1:<port>` via the `hostid`
   loopback shortcut when it's on the same host); the **server** updates `players`/`status`.
4. **End/crash** → server deletes its file on clean exit; stale files pruned by heartbeat
   age (dead `pid` or heartbeat > ~15 s).

**Single-writer rule:** each match's server is the only writer of its own file; doors are
read-only listers. That keeps it race-free across nodes.

Alternatives considered: chocolate's `net_query` UDP LAN discovery (kept for the connect
handshake, but localhost broadcast across many per-match ports is fiddly for *listing*);
MQTT (`sbbs/syncdoom/games/#`) for push-style real-time lobby updates — nice later, but an
extra dependency the file registry doesn't need.

## Network topology — don't assume localhost

The `data_dir` registry is shared even when the BBS itself isn't single-host: a larger
system can be **split across several machines on a LAN** (separate telnet/web/door hosts)
that mount the same install over a network filesystem. There, discovery still works (every
node sees `games/*.ini`), but the **doom client and the dedicated server can be on
different machines**, so `127.0.0.1` is wrong. The registry must therefore carry a
**reachable address**, not just a port.

Address handling (as built):

- **Server bind** — the dedicated server binds the UDP port on the address from
  `[net] bind`, which **defaults to `[net] advertise`** when unset (listen where you tell
  peers to dial). With **both blank** it binds **`127.0.0.1`** — loopback only, so a fresh
  server is reachable from same-host clients and nothing else (the safe default; exposure is
  opt-in). Set `bind = 0.0.0.0` to listen on **all interfaces**, or a specific local IP to
  pin one NIC — needed when `advertise` is a public/NAT name this host can't bind directly.
  For cross-host play the sysop must also allow the UDP port range across the LAN firewall.
  **The game protocol has no authentication:** once the server listens on a routable address,
  any host that reaches the UDP port and speaks the right Doom version/WAD can join — gate it
  at the firewall (and see *Inter-BBS federation* for the planned allowlist).
- **Advertised `addr`** — what joiners connect to, written into the registry entry. The
  value comes straight from `[net] advertise` (optionally overridden per host via
  `[net:<hostname>]`, below). **Blank → `127.0.0.1`**, i.e. same-host play only; set it to
  this host's LAN IP or DNS name for cross-host play. There is **no auto-detection** — a
  shared install spanning hosts must set `advertise` per host (an unset value can't be
  guessed). The match creator shares the server's host, so it dials the server's **bind**
  address — its own local IP when `bind`/`advertise` is set, or `127.0.0.1` when the bind is
  unset or a wildcard (`0.0.0.0`). It can't blindly use `127.0.0.1`: a socket bound to a
  specific interface IP doesn't receive loopback datagrams. Browse-joiners dial the registry
  `addr`.

This is purely a config/address concern — the lockstep design and the file registry are
unchanged; only "where do I connect" gains a real address.

## Inter-BBS federation — trusted-peer public games *(design — not built)*

> **Not implemented yet.** This supersedes the earlier "open `scope = public`" sketch (let
> *any* Chocolate Doom client join): a narrower, safer model where a handful of **trusted,
> allowlisted BBSes** federate their game lists and play together. The only public-join
> feature that ships **today** is `[net] allow_external` — a lobby menu item to **manually**
> dial a server by `host:port` (no discovery, no auth, no port-forward help).

The goal: a user on BBS A can discover and join a co-op match hosted on BBS B. Two problems
to solve — **discovery** across systems and **access control** — plus one engine constraint
that frames the whole feature.

### The lockstep constraint — co-op first

Doom is **lockstep**: the sim advances only once every node's ticcmd for a tic has arrived
(35/sec). Single-host play hides this because the server and every door are co-located
(loopback/LAN), so the slow telnet link is *outside* the tic loop. Federation puts the Doom
client on BBS A and the dedicated server on BBS B with the **internet between them**, so
inter-BBS RTT enters the lockstep path and throttles the shared sim to the slowest peer.

- **Co-op** tolerates this well (Doom co-op is forgiving) — the target use case.
- **Deathmatch** is timing-sensitive and will feel laggy/rubber-bandy across the internet —
  allowed, but flagged "works, not competitive."

Pitch and ship federation as **inter-BBS co-op**.

### Discovery — finger-based registry federation

Every Synchronet system already runs a **finger** service, so expose the local game registry
as a finger response (e.g. `finger doom@peerbbs` returns the live `data/syncdoom/games/`
entries — host, mode, wadset, players, address, port). A lobby then **aggregates a configured
peer list** into its browse view: its own registry plus each peer's finger output. This is
**pull-based and decentralized** — no central server, no new transport, reusing infrastructure
every system already has. (The same data could ride the who's-online / active-user channel;
finger is the simplest first cut.)

### Connection & access control

A joining door on BBS A connects over the internet to BBS B's dedicated server, so the host
sysop must **port-forward** the UDP port range and publish a reachable address
(`[net] public_advertise`). Two layers keep out random/rogue clients — use either or both:

1. **Peer allowlist (baseline).** The sysop lists the trusted peer BBSes; the dedicated
   server drops UDP from any source not on the list. For peers with **static IPs** this alone
   is most of the value — a rogue client simply isn't on the list, and spoofing a full
   stateful Doom handshake from a forged source is impractical. No cryptography involved.
2. **Shared secret — for dynamic-IP peers / hardening.** A secret configured **out-of-band**
   in each side's `syncdoom.ini` (never in the repo) and **passed to `syncdoom` on the
   command line** — the server requires it, the joining client presents it, the connect
   handshake rejects a mismatch. This covers peers on **DDNS/dynamic addresses** (which an IP
   allowlist can't pin) and adds defense-in-depth. Cost: a small extension to the Chocolate
   connect handshake to carry and verify the token (the protocol is otherwise stock).

With static-IP peers, layer 1 suffices and the secret is optional; with dynamic-IP peers,
layer 2 is what gates access.

### WAD parity & versioning

Both ends need byte-identical WADs (IWAD + PWADs + DeHackEd). Commercial `doom2.wad` can't be
shared, so federated games realistically standardize on **Freedoom** — the one free, common
IWAD — and the registry's wadset id lets the joiner verify local files before connecting.
Federation also implies both sides run a **protocol-compatible syncdoom** (the Chocolate base
revision is pinned); a federation is, in effect, a version-coordinated group. There is **no
anti-cheat** (vanilla has none) — the *trust* in "trusted peers" is the mitigation.

### Config sketch (design)

```ini
[net]
public_advertise  = doom.mybbs.example         ; public host/IP joiners dial; UDP range port-forwarded
federate          = a.bbs.example, b.bbs.example:79   ; trusted peers (finger host[:port]); also the UDP allowlist
federation_secret =                            ; optional out-of-band shared secret (blank = allowlist only)
```

### Phasing

Federation is a **post-MVP** layer (NAT/forwarding, federation config, cross-internet lockstep
testing — not a quick add):

1. **Done / near-term:** single-host, then LAN multi-host (current).
2. **Federation MVP:** finger discovery + peer IP allowlist + co-op + Freedoom.
3. **Hardening:** the out-of-band command-line secret (for dynamic-IP peers) and any
   deathmatch lag tuning.

## `[net]` — networking & server config

Multiplayer config lives in a `[net]` section of the door's `syncdoom.ini` (alongside
`[video]`/`[input]` and `[wads]`/`[wadset:*]`). All keys are optional; an absent key keeps
the built-in default. These govern the dedicated-server lifecycle, address advertising,
port allocation, discovery, and per-match limits.

| Key | Meaning |
|-----|---------|
| `advertise` | Address joiners on **other** hosts dial, written into the registry `addr`. **Blank → `127.0.0.1`** (same-host play only). Set to this host's LAN IP / DNS name for cross-host play; override per host with `[net:<hostname>]`. No auto-detection. |
| `bind` | Local address the dedicated server's UDP socket **listens** on (vs. `advertise`, which is only what peers dial). **Blank inherits `advertise`** — usually the only net key you need. Both blank → `127.0.0.1` (loopback only; exposure is opt-in). Set `0.0.0.0` for all interfaces, or a specific local IP when `advertise` is a public/NAT name this host can't bind. No auth — firewall a routable bind. |
| `port_low` / `port_high` | UDP port range for per-match dedicated servers (free-port scan within it; one port per live match). Default `20000` / `20063`. |
| `max_games` | Cap on concurrent matches on this system (the lobby refuses Create past it). Default `8`. |
| `max_players` | Per-match player ceiling. **Hard cap 4** — Doom's `MAXPLAYERS` (4 player slots/colors/starts); the lobby clamps to 4 regardless. A set's/creator's `maxplayers` can't exceed it. Default `4`. |
| `idle_timeout` | Seconds an **empty** (zero-player) match waits before the dedicated server self-terminates and deletes its registry entry. Default `60`. |
| `stale` | Heartbeat age after which a game entry is pruned from the browse list (its server presumed dead). Default `30`. The server refreshes its `heartbeat` every ~3 s (fixed, not configurable). |
| `allow_external` | Show the lobby's "join an external server by `host:port`" option (manual cross-system join). Default `false`. |

**Deferred (not built):** `discovery` / `net_query` LAN-broadcast game discovery (discovery
is registry-only), and the inter-BBS federation keys `public_advertise` / `federate` /
`federation_secret` (see *Inter-BBS federation*).

#### Example

```ini
[net]
advertise    =                ; blank = 127.0.0.1 (same-host only); set LAN IP/DNS for cross-host
bind         =                ; blank inherits advertise; 0.0.0.0 = all interfaces; specific IP pins a NIC
port_low     = 20000          ; per-match dedicated-server UDP port range
port_high    = 20063
max_games    = 8              ; concurrent matches on this system
max_players  = 4              ; per-match ceiling (Doom MAXPLAYERS = 4)
idle_timeout = 60             ; empty match self-terminates after this many seconds
stale        = 30             ; prune a game entry after this heartbeat age
allow_external = false        ; offer manual "join by host:port" of an outside server
```

#### Shared config & per-host overrides

`syncdoom.ini` is a **single shared file** in the install (`xtrn/syncdoom/`), read by every
node on every host — even when hosts differ in OS (a Linux build and a Windows `.exe` both
read the same ini beside them). Most `[net]` keys are BBS-wide policy, so sharing is
correct: `port_low`/`port_high`, `max_games`, `max_players`, `stale`, `idle_timeout`, and
`allow_external`.

The inherently **per-host** value is `advertise` (and, if pinned, `bind`): a server on host A
must advertise A's address, one on host B must advertise B's. A single shared value can't
express that, so the **JS lobby** reads `[net]` then overlays a **hostname-keyed sub-section**
`[net:<hostname>]`, where `<hostname>` is Synchronet's `system.local_host_name` for the host
the lobby runs on. Keys in the matching sub-section win over the base `[net]`, keeping one
shared file that still carries per-host values. Because `bind` inherits `advertise`, a
LAN install usually only needs the per-host `advertise`:

```ini
[net]
advertise =                 ; blank = 127.0.0.1 (same-host only)

[net:bbs-telnet]            ; this host's LAN-facing address (bind inherits it)
advertise = 10.0.0.6
[net:bbs-web]               ; a second host sharing the install
advertise = 10.0.0.7
```

Because there is no auto-detection, **every host that hosts cross-host games needs an
`advertise` value** — a shared one (single-host BBS) or a `[net:<hostname>]` entry (a shared
install spanning hosts). A host left blank advertises `127.0.0.1` and is reachable only from
its own machine.

## Lobby — a JavaScript frontend (decided)

The lobby is **not** built into the C door. It is a **Synchronet JavaScript module** living
beside the binary in `xtrn/syncdoom/`, which spawns the `syncdoom` C binary to play. The
split falls exactly on the **engine / orchestration** line:

- **C door = engine**: rendering tiers, input, the net layer + UDP transport, and the
  headless `-dedicated` relay. Plus the small CLI contract below. Stays portable to any
  DOOR32.SYS BBS.
- **JS lobby = orchestration**: browse/create/join UI, the `data/syncdoom/games/` registry
  and discovery, reading `[net]`/`[wads]`/`[wadset:*]`, identity, and spawning servers.

### Why JS (what Synchronet gives for free)

- **Node model** — `system.node_list` / the node table / `bbs.node_action`: live cross-node
  awareness, no hand-rolled heartbeat scanning.
- **UI** — `frame.js` / `tree.js` / lightbar libraries + `console.*` make a slick
  server-browser trivial vs. hand-coding ANSI lightbars in C. (This replaces the earlier
  C-lobby "Option A/B/C" sketch entirely.)
- **Registry** — `File` + `JSON` with lock modes instead of C file locking.
- **Identity/time/auth native** — `user.alias`, `user.number`, time-left — no DOOR32 parse
  for the lobby part. **`system.host_name`** makes the per-host `advertise` override natural.
- **MQTT** (`MQTT` object) for real-time lobby push if ever wanted; **no** new dependency.
- No terminal probing for the lobby (the BBS already knows the term); fast iteration, no
  recompile; matches the Synchronet door-loader idiom.

Constraint: the lobby JS must be **SpiderMonkey 1.8.5-compatible** (no modern ES) like all
Synchronet server-side JS.

### The C CLI contract the lobby drives

The door exposes everything the lobby needs on the command line — **no dropfile required**
(the door reads only commtype, socket, and time from DOOR32.SYS, and all three are already
override-able):

| Arg | Purpose | Status |
|-----|---------|--------|
| `-connect <addr:port>` | Join a match as a client | done (passes through) |
| `-dedicated -port <n>` | Run as a headless match server | done |
| `-spawnserver -port <n>` | Daemonize a detached `-dedicated` server, then return (so JS can launch one synchronously) | to add (wraps `mp_spawn_server`) |
| `-name <handle>` | Set the network player name from the BBS handle | to add |
| `-s%H -t%T -l%R` + WAD args | Socket / time / rows / IWAD+PWADs | done |

### Spawning & locating

- **Locate the binary** via `js.exec_dir` — the lobby script and the `syncdoom` binary share
  `xtrn/syncdoom/`, so `js.exec_dir + "syncdoom"` needs no hardcoded path. The whole door
  (binary + WADs + config + lobby) is self-contained in one directory.
- **Play** — the lobby `bbs.exec()`s the binary with `-connect <addr:port> -name <alias>
  -s%H -t%T -l%R` + the match's WAD args (binary/untranslated I/O mode); the door runs the
  game and returns to the lobby.
- **Host** — the lobby allocates a free port (JS `Socket` bind-test, or the C
  `mp_alloc_port`), calls `syncdoom -spawnserver -port <n>` (C does the OS-correct
  `fork`+`setsid` detach and returns the pid), and writes the registry entry.
- **Cleanup** — servers self-terminate on idle and remove their own registry entry; the
  lobby prunes stale entries by heartbeat age. (See *Server lifecycle & host-drop*.)

### Portability note

A JS lobby makes **MP coordination Synchronet-native**, but the C door's
`-connect`/`-dedicated`/`-spawnserver` stay the portable interface, so the single-player
door remains universal and another BBS could script its own loader against the same
contract. **Portable engine, Synchronet-native lobby.**

### Reference (no existing real-time lobby to lift)

There is **no real-time game lobby in Synchronet to derive from**; these inform the *shape*
only: Inter-BBS door games (`ny2008`, FidoNet netmail — store-and-forward), `presence-
service.js`/`sbbsimsg` (presence, not match coordination), and the node table
(`getnodedat`/`putnodedat`, node-scoped not match-scoped).

## WADs

- **Single-player multi-WAD already works** — `-iwad <iwad> -file a.wad b.wad -merge c.wad`
  passes straight through to doomgeneric (`w_main.c`). Today the door just **forwards**
  those args (`syncdoom.c`); there is no door-side WAD list, directory, or config yet — the
  lobby introduces all of that.
- **In multiplayer every player must load the identical IWAD + PWAD set** — the sim is
  deterministic, so any mismatch desyncs/crashes. The lobby enforces it: the creator's WAD
  set becomes the match's, and joiners inherit the same args. Easy here — every node reads
  the same shared install's WAD dir.

### WAD selection & configuration (new — proposed)

#### Where the files live

doomgeneric strips Chocolate's WAD-search logic (its `DOOMWADDIR`/`DOOMWADPATH`/registry
block is all under `#if ORIGCODE`, which is **undefined**); it collapses to a compile-time
`FILES_DIR`, defined as `"."` (`config.h`). So today WAD resolution is just: an **absolute
/ already-existing path** (`D_FindWADByName` checks `M_FileExists` first), else the
**current working directory**. No env vars, no search list — it "works" only because the
sysop sets the door's startup dir and drops WADs there.

Proposed norm: **the door owns a WAD directory and passes absolute paths to doomgeneric**,
so resolution never depends on cwd (fragile across BBS launch contexts). The dir is
**sysop-curated and shared** across nodes via the install — users **never** supply their
own WADs (a desync hazard *and* a security one). IWADs are commercial and can't ship; the
sysop drops in their own. (Freedoom is the one freely-redistributable IWAD one could ship
or default to.)

#### `[wads]` — global WAD config

| Key | Meaning |
|-----|---------|
| `dir` | WAD directory. Default: the door's own program dir (same resolution `syncdoom.ini` uses). Relative (to the door dir) or absolute. |
| `default` | `<id>` of the `[wadset:*]` pre-selected (highlighted, ENTER-default) in the picker. |

**Deferred (not built):** `autoscan` / `default_iwad` (offer loose IWADs/PWADs with no
`[wadset:*]`) and `sort` (picker order). The picker lists the configured `[wadset:*]`
sections in ini order.

Deliberately **out of scope**: `allow_user_wads` (no — curated dir is the point; MP desync
+ security), and global player/mode caps (those belong in the net section, not `[wads]`).

#### `[wadset:*]` — one playable set per section

The picker lists **named, playable *sets*, not a raw file dump**: a valid Doom config is
*exactly one IWAD + zero-or-more PWADs* (a bare PWAD like SIGIL/MyHouse is unplayable
without its IWAD). All filenames resolve in `[wads] dir`.

| Key | Req? | Meaning |
|-----|------|---------|
| `iwad` | **yes** | The one base IWAD, e.g. `doom2.wad` → `-iwad doom2.wad`. |
| `name` | no | Display name in the picker (defaults to the section `<id>`). |
| `pwad` | no | Comma-separated PWADs loaded with `-file` (order significant). |
| `desc` | no | One-line description for the picker. |
| `modes` | no | Where the set may be used: `solo, coop, deathmatch, altdeath` (default: all). A DM-only pack sets `modes = deathmatch, altdeath`; subsumes a separate "solo allowed" flag. |
| `map` | no | Default starting map / `-warp` value (creator can override). |
| `maxplayers` | no | Default cap for the set, ≤ 4 (Doom `MAXPLAYERS`); creator can lower it. |
| `merge` | no | Comma-separated WADs merged into the IWAD via `-merge` (deutex-style; rare). |
| `enabled` | no | `false` hides a set without deleting the section (default `true`). |
| `note` | no | Caveat shown (with a keypress pause) before the door launches, e.g. "needs a source port; may not run on vanilla". |

`skill` is intentionally **not** a set key — it's a per-match choice at creation.

**WAD readme files:** if a `<wadname>.msg` file sits beside a WAD in the set (the IWAD, a
PWAD, or a merge WAD — e.g. `sigil.msg` next to `sigil.wad` in `[wads] dir`), the lobby
displays it — paged, with a pause — before launching. It's a long-form companion to `note`:
a map pack's readme/intro/credits. Sysop-curated like the WADs themselves; shown on Create,
Solo, and Join (each file at most once).

A selected set expands into the `-iwad`/`-file`/`-merge`/`-warp` args the door already
forwards (`syncdoom.c`), each as an **absolute path** (`[wads] dir` + filename), so this
layer is purely lobby-side — no change to the game launch path.

#### Example

```ini
[wads]
dir          = wads          ; relative to the door dir, or an absolute path

[wadset:doom2]
name         = Doom II: Hell on Earth
iwad         = doom2.wad
desc         = The classic 32-level campaign
modes        = solo, coop, deathmatch, altdeath

[wadset:sigil]
name         = SIGIL (John Romero)
iwad         = doom.wad
pwad         = sigil.wad
map          = E5M1
desc         = Romero's unofficial fifth episode
modes        = solo, coop

[wadset:dm-arena]
name         = Deathmatch Arena
iwad         = doom2.wad
pwad         = arena1.wad, arena2.wad
modes        = deathmatch, altdeath
maxplayers   = 4
```

#### Fallback (no `[wadset:*]` configured) — *(deferred)*

The auto-scan fallback (classify each file in `dir` by its 4-byte `IWAD`/`PWAD` header magic
and offer bare IWADs without explicit sections) is **not built**. Today a set must be
defined as a `[wadset:*]` section to appear in the picker.

#### Who prompts for a set

- **Create** and **Solo** — prompt (the set picker; Solo omits the MP params).
- **Join** — **never prompts**; reads `wadset =` from the registry, auto-loads exactly that
  set, and **verifies the files exist locally before connecting**, refusing with a clear
  message if a node is missing one. The browse list *displays* each game's set so you know
  what you're joining.
- **Solo writes nothing to the registry** — pure single-player, no server, no net layer; it
  shares only the WAD picker with Create.

## Build work & status

**C door (engine):**

1. Restore the chocolate-doom net `.c` files; transport via xpdev UDP (`sockwrap`) instead
   of SDL_net. — **done** (base `f1f8ecde`; `net_udp.c`).
2. Headless `-dedicated` server + lifecycle helpers (`mp_dedicated_main`, `mp_spawn_server`,
   `mp_alloc_port`, idle-timeout). — **done** (server validated: binds, runs).
3. CLI contract for the lobby: add `-spawnserver` and `-name`; confirm dropfile-less. — *to
   do* (small).
4. Rendering stays per-client (JXL/sixel/text tiers already built); the dedicated server is
   headless.

**JS lobby (orchestration, `xtrn/syncdoom/`, SpiderMonkey 1.8.5):**

5. Browse/create/join + waiting room (Synchronet UI libs); `data/syncdoom/games/` registry
   + discovery; read `[net]`/`[wads]`/`[wadset:*]`; port alloc + spawn via `-spawnserver`;
   locate binary via `js.exec_dir`; launch play via `bbs.exec` (no dropfile).

Bonuses that come for free with the net layer: in-game **chat** (`T` key), and players
dropping cleanly when their BBS time (`-t`) runs out.

## Open questions for review

- Player-slot caps per match — **resolved:** 4 (Doom `MAXPLAYERS`); the lobby clamps to it.
- Where the dedicated server binary comes from — a `-dedicated` mode of the same `syncdoom`
  executable (preferred: one binary), or a separate build target.
- Port allocation/recycling for concurrent matches (range + free-port scan).
- Stale-game GC cadence and who runs it (each door on browse, vs. a periodic sweep).
- Empty-match shutdown policy — zero players only, or a configurable floor for DM.
- LAN-address auto-detection — **resolved:** not done; cross-host installs set `[net]
  advertise` (or a per-host `[net:<hostname>]`) explicitly. Blank advertises `127.0.0.1`.
- Public/cross-BBS play — **resolved:** deferred as *Inter-BBS federation* (trusted-peer,
  finger discovery + IP allowlist; co-op first). The only shipped external-join path today is
  the manual `allow_external` host:port option.
- Whether the WAD-set auto-scan fallback is worth building for v1, or `[wadset:*]`
  sections in `syncdoom.ini` are required (config home decided: `syncdoom.ini`).
- Whether to expose persistent "rooms" later, and MQTT push for live lobby updates.
