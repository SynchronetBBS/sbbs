# syncdoom multiplayer — design (not built yet)

Status: **scoped, not implemented.** This is the plan to review before building. Co-op
and deathmatch over a single Synchronet host, plus how nodes discover joinable games.

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
the same port from `port_range` — `<port>.ini` alone would collide. `<hostid>` + `<port>`
is unique BBS-wide (one server per port per host) and matches the `hostid`/`port` fields
inside:

```
host       = SysopHandle
wad        = freedoom2.wad +myhouse.wad
mode       = coop            ; coop | deathmatch | altdeath
skill      = 3
map        = MAP01
addr       = 10.0.0.6        ; address joiners connect to (see Network topology)
port       = 20001           ; the dedicated server's UDP port
hostid     = node-name       ; which host the server runs on (loopback shortcut)
scope      = lan             ; lan | public  (who may join)
players    = 2
maxplayers = 4
status     = lobby           ; lobby | playing | done
pid        = 12345           ; dedicated-server pid (liveness/cleanup)
heartbeat  = 2026-06-16T...  ; bumped every few seconds
```

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

Address handling:

- **Server bind** — the dedicated server binds the UDP port on all interfaces (or a
  sysop-configured bind address), so both same-host (loopback) and cross-host (LAN) clients
  can reach it. The sysop must allow the UDP port range across the LAN firewall.
- **Advertised `addr`** — what joiners connect to, written into the registry entry.
  Resolution order: (1) explicit sysop config in `syncdoom.ini` `[net] advertise =` — wins,
  and is required when auto-detection guesses wrong (multi-NIC) or to pin one interface;
  (2) the host's primary LAN address, auto-detected; (3) `127.0.0.1` as the single-host
  fallback. **Never silently hardcode loopback.**
- **`hostid` / loopback shortcut** — the entry records which host the server runs on (e.g.
  system hostname). A joining door on that same host may substitute `127.0.0.1` for a
  faster local hop; a door elsewhere uses `addr`. Optimization, not required for
  correctness.

This is purely a config/address concern — the lockstep design and the file registry are
unchanged; only "where do I connect" gains a real address.

## Opening a match to public (non-BBS) clients — optional

The restored net layer speaks **Chocolate Doom's wire protocol** (we keep its packet
format and only swap the transport), so in principle an external Chocolate Doom client
could join a match — not only BBS doors. That's the `scope = public` flag:

- `scope = lan` (default) — only this system's doors may join; bind/advertise stay on the
  LAN.
- `scope = public` — the sysop advertises a **public address** (`[net] public_advertise =`)
  and **port-forwards** the UDP port(s); the lobby exposes the match to outside clients.

Caveats that keep this opt-in and later-stage: NAT/port-forwarding setup, no BBS
authentication or time-accounting for outside clients, version/WAD-set matching enforced
purely by the protocol (mismatch desyncs), and the usual public-server abuse/cheating
exposure. Ship LAN-only first; public is a deliberate sysop choice, per match.

## `[net]` — networking & server config

Multiplayer config lives in a `[net]` section of the door's `syncdoom.ini` (alongside the
existing `[video]`/`[input]` and the new `[wads]`/`[wadset.*]`). All keys are optional; an
absent key keeps the built-in default. These govern the dedicated-server lifecycle, address
advertising, port allocation, discovery, and limits referenced throughout this doc.

#### Addressing

| Key | Meaning |
|-----|---------|
| `bind` | Address the dedicated server binds. Default: **all interfaces** (so both loopback and LAN clients reach it). Pin to one interface on a multi-NIC host. |
| `advertise` | LAN address joiners connect to, written into the registry `addr`. Default: **auto-detected** primary LAN address, falling back to loopback on a single-host install. **Required** when auto-detection guesses wrong (multi-NIC) or to pin an interface on a multi-host BBS. |
| `public_advertise` | Public address advertised for `scope = public` matches; the sysop **port-forwards** the UDP port(s) to it. Only consulted when `allow_public = true`. |

#### Ports & concurrency

| Key | Meaning |
|-----|---------|
| `port_range` | UDP port range for per-match dedicated servers, e.g. `20000-20063` (free-port scan within it; one port per live match). |
| `max_games` | Cap on concurrent matches (bounds server processes/ports; the lobby refuses Create past it). |
| `max_players` | Global ceiling on players per match, ≤ 8 (`NET_MAXPLAYERS`). A set's/creator's `maxplayers` can't exceed this. |

#### Public access

| Key | Meaning |
|-----|---------|
| `allow_public` | Master gate for `scope = public`. Default `false` — the lobby hides the public option entirely. Set `true` only after `public_advertise` + port-forwarding are in place. |

#### Discovery & liveness

| Key | Meaning |
|-----|---------|
| `discovery` | Game-list source: `registry` (shared-file, default) \| `query` (`net_query` LAN broadcast — for installs where `data_dir` isn't shared) \| `both`. |
| `heartbeat` | Seconds between a server bumping its registry entry's `heartbeat` (default ~5). |
| `stale` | Heartbeat age (or dead `pid`) after which a game entry is pruned from the list (default ~15). |
| `idle_timeout` | Seconds an **empty** (zero-player) match waits before the dedicated server self-terminates and deletes its registry entry. Implements the empty-match shutdown rule. |

#### Example

```ini
[net]
bind         =                ; blank = all interfaces (default)
advertise    =                ; blank = auto-detect LAN addr; set on multi-NIC/host
port_range   = 20000-20063    ; per-match dedicated-server UDP ports
max_games    = 8              ; concurrent matches
max_players  = 8              ; per-match ceiling (<= NET_MAXPLAYERS)
discovery    = registry       ; registry | query | both
heartbeat    = 5              ; server registry-entry refresh, seconds
stale        = 15             ; prune a game entry after this heartbeat age
idle_timeout = 60             ; empty match self-terminates after this many seconds

allow_public     = false      ; gate scope=public; needs public_advertise + forwarding
public_advertise =            ; public address for scope=public matches
```

#### Shared config & per-host overrides

`syncdoom.ini` is a **single shared file** in the install (`xtrn/syncdoom/`), read by every
node on every host — even when hosts differ in OS (a Linux build and a Windows `.exe` both
read the same ini beside them). Most `[net]` keys are BBS-wide policy, so sharing is
correct: `port_range`, `max_games`, `max_players`, `allow_public`, `discovery`,
`heartbeat`, `stale`, `idle_timeout`, and `bind = all-interfaces`.

The one inherently **per-host** value is `advertise` (and, rarely, a pinned `bind`): a
server on host A must advertise A's address, one on host B must advertise B's. That's why
`advertise` **defaults to auto-detect** — with the shared ini left blank, each host's server
advertises its *own* LAN address at runtime, so the shared file works untouched. Note the
registry `addr` is the **advertised** address, not the raw bind result: `bind =
all-interfaces` is a wildcard a joiner can't connect to, so the server writes the explicit/
auto-detected concrete address instead.

The shared file only needs help when you must **override** `advertise` per host (multi-NIC,
where auto-detect picks the wrong interface). A single value can't express that, so the
door reads `[net]` then overlays a **hostname-keyed sub-section** `[net.<hostname>]` (it
knows its own `gethostname()`) — keeping one shared file that still carries per-host values:

```ini
[net]
advertise =                 ; blank = auto-detect (works for most hosts)

[net.bbs-telnet]            ; this host has two NICs; pin the right one
advertise = 10.0.0.6
[net.bbs-web]
advertise = 10.0.0.7
```

(A per-host environment variable could serve the same role, but the hostname sub-section
keeps all config in the one shared ini.)

## Where the lobby lives — and what exists to build on

The lobby is **new code, part of the `syncdoom` door itself** — a pre-game text menu
(create / browse-and-join) shown before the game launches, reading and writing the
`data/syncdoom/games/` registry. It is not a separate program.

There is **no existing real-time game lobby in Synchronet to derive from**. The closest
BBS-native idioms, for reference rather than reuse:

- **Inter-BBS door games** (e.g. the bundled `ny2008`) coordinate across *systems* via
  FidoNet-style **netmail file exchange** (`INTERBBS.CFG`) — store-and-forward, turn-based,
  not real-time. Wrong model for lockstep, but it's the established "doors talking to each
  other" pattern.
- **`presence-service.js` / `sbbsimsg`** — tracks who's online locally and inter-BBS via
  instant messaging; a model for *presence/announce*, not match coordination.
- Synchronet's **node table** (`getnodedat`/`putnodedat`) — live per-node status another
  node can read; conceptually similar to our heartbeat registry, but node-scoped, not
  match-scoped.

So we build the lobby fresh against the file registry; these inform the *shape*
(heartbeat liveness, single-writer, file-based discovery) but none drop in.

## Lobby UI — proposed options

The lobby is the text screen shown **before** the game launches, so it runs before any
graphics tier and must work on the same broad terminal range (plain ANSI/CP437, not just
SyncTERM). Two views are needed no matter what — **browse/join** and **create** — plus a
**waiting room** between joining and match start. The design fork is the browse model.

### Option A — Classic prompt-driven list (works on any terminal)

A plain list plus a command prompt; no cursor addressing, no redraw loop.

```
                     S Y N C D O O M  --  N E T W O R K   G A M E S
 --------------------------------------------------------------------------
  #  Host        Mode        WAD              Map    Players  Status
 --------------------------------------------------------------------------
  1  RoboCop     Co-op       DOOM2            MAP01    2/4    Lobby
  2  Carcosa     Deathmatch  DOOM2 +myhouse   MAP07    3/4    Playing
  3  Sigil       Altdeath    SIGIL            E5M1     1/8    Lobby
 --------------------------------------------------------------------------
  [#] Join   [C]reate   [S]olo   [R]efresh   [Q]uit
  Command: _
```

- Pro: trivial to build, no terminal-capability assumptions, classic door feel.
- Con: no live updates without a manual refresh; least slick.

### Option B — Full-screen lightbar browser (server-browser model)

Highlighted row, arrow-key navigation, ENTER joins, auto-refreshes from the heartbeat
registry every few seconds. Scales to a wide terminal (reuses the renderer's geometry
probe).

```
+========================= SYNCDOOM -- NETWORK GAMES =========================+
|  Host        Mode        WAD              Map    Players  Status            |
| -------------------------------------------------------------------------- |
| >RoboCop     Co-op       DOOM2            MAP01    2/4    Lobby           < |
|  Carcosa     Deathmatch  DOOM2 +myhouse   MAP07    3/4    Playing           |
|  Sigil       Altdeath    SIGIL            E5M1     1/8    Lobby             |
|                                                                            |
+============================================================================+
|  up/dn select   ENTER join   C create   S solo   Q quit                    |
+============================================================================+
             refreshing every 3s . 4 players online . 58m left
```

- Pro: matches the Doomseeker/IDE mental model, live counts/status, modern feel.
- Con: needs cursor addressing + a redraw loop; must degrade to Option A on a dumb
  terminal.

### Option C — Auto / no lobby (args decide, sysop-baked)

No browse menu: the SCFG command line / dropfile carries the game params and the door
**joins a matching live game if one exists, else creates it**, going straight to the
waiting room. Good for a sysop exposing one fixed game as its own door entry (e.g. a
"Co-op DOOM2" menu command).

- Pro: zero UI, fastest to a game.
- Con: no choice, no visibility, can't browse multiple games.

### Shared pieces (under A or B alike)

- **Create flow** — a short guided sequence: WAD set -> mode -> skill -> starting map ->
  max players -> scope (lan/public). A few prompts, or a mini lightbar form.
- **Waiting room** — after create/join, before start: lists joined players + their nodes,
  a ready/start control (creator starts, or auto-start when full / on a countdown), and a
  chat line.

### Recommendation

**B as the primary browse UI, with A as the automatic fallback** when the terminal can't
do cursor addressing (detected via the same probe path the renderer uses), and **C exposed
as a per-menu SCFG option** for curated single-game door entries. All three share the same
list/registry code; the create flow and waiting room are identical regardless of browse
model.

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
| `dir` | WAD directory. Default: the door's own program dir (same resolution `syncdoom.ini` uses). Relative (to the door dir) or absolute; may be a comma-list for multiple search dirs. |
| `default` | `<id>` of the pre-selected `[wadset.*]` in the picker; the set **Solo** uses when not prompting. |
| `autoscan` | Offer loose IWADs found by scanning `dir` even without a matching `[wadset.*]`. `true`/`false`. |
| `default_iwad` | IWAD paired with **auto-scanned standalone PWADs** (which otherwise can't be made playable). Only relevant when `autoscan = true`. |
| `sort` | Picker order: `config` (ini section order) or `name` (alphabetical). |

Deliberately **out of scope**: `allow_user_wads` (no — curated dir is the point; MP desync
+ security), and global player/mode caps (those belong in the net section, not `[wads]`).

#### `[wadset.*]` — one playable set per section

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
| `maxplayers` | no | Default/recommended cap, ≤ 8 (`NET_MAXPLAYERS`); creator can override. |
| `merge` | no | Comma-separated WADs merged into the IWAD via `-merge` (deutex-style; rare). |
| `enabled` | no | `false` hides a set without deleting the section (default `true`). |
| `note` | no | Caveat shown before launch (e.g. "needs a source port; may not run on vanilla"). |

`skill` is intentionally **not** a set key — it's a per-match choice at creation.

A selected set expands into the `-iwad`/`-file`/`-merge`/`-warp` args the door already
forwards (`syncdoom.c`), each as an **absolute path** (`[wads] dir` + filename), so this
layer is purely lobby-side — no change to the game launch path.

#### Example

```ini
[wads]
dir          = wads          ; relative to the door dir, or an absolute path
default      = doom2         ; pre-selected set; Solo uses it when not prompting
autoscan     = true          ; offer loose IWADs found in dir w/o a [wadset.*]
default_iwad = freedoom2.wad ; IWAD paired with auto-scanned standalone PWADs
sort         = config        ; picker order: config | name

[wadset.doom2]
name         = Doom II: Hell on Earth
iwad         = doom2.wad
desc         = The classic 32-level campaign
modes        = solo, coop, deathmatch, altdeath

[wadset.sigil]
name         = SIGIL (John Romero)
iwad         = doom.wad
pwad         = sigil.wad
map          = E5M1
desc         = Romero's unofficial fifth episode
modes        = solo, coop

[wadset.dm-arena]
name         = Deathmatch Arena
iwad         = doom2.wad
pwad         = arena1.wad, arena2.wad
modes        = deathmatch, altdeath
maxplayers   = 8
```

#### Fallback (no `[wadset.*]` configured)

With `autoscan = true`, scan `dir` and classify each file by its 4-byte header magic
(`IWAD` vs `PWAD`), offering every IWAD as a standalone set (and loose PWADs atop
`default_iwad`). Anything beyond bare IWADs wants explicit `[wadset.*]` entries; zero-config
still gets "pick an IWAD and play."

#### Who prompts for a set

- **Create** and **Solo** — prompt (the set picker; Solo omits the MP params).
- **Join** — **never prompts**; reads `wad =` from the registry, auto-loads exactly that
  set, and **verifies the files exist locally before connecting**, refusing with a clear
  message if a node is missing one. The browse list *displays* each game's set so you know
  what you're joining.
- **Solo writes nothing to the registry** — pure single-player, no server, no net layer; it
  shares only the WAD picker with Create.

## Build work (the bulk = restoring the net layer)

1. Restore the chocolate-doom net `.c` files at doomgeneric's base revision; transport via
   xpdev UDP (`sockwrap`) instead of SDL_net.
2. Dedicated-server lifecycle: spawn on create, reap on end/empty/timeout, cap concurrency.
3. Lobby: create/join menu + the `data/syncdoom/games/` registry + arg plumbing.
4. Rendering stays per-client (the tiers already built: JXL/sixel/text); the dedicated
   server is headless.

Bonuses that come for free with the net layer: in-game **chat** (`T` key), and players
dropping cleanly when their BBS time (`-t` / DOOR32.SYS line 9) runs out.

## Open questions for review

- Player-slot caps per match (vanilla `NET_MAXPLAYERS` is 8; co-op/DM cap?).
- Where the dedicated server binary comes from — a `-dedicated` mode of the same `syncdoom`
  executable (preferred: one binary), or a separate build target.
- Port allocation/recycling for concurrent matches (range + free-port scan).
- Stale-game GC cadence and who runs it (each door on browse, vs. a periodic sweep).
- Empty-match shutdown policy — zero players only, or a configurable floor for DM.
- LAN-address auto-detection: how reliably can the server pick its own routable address on
  a multi-NIC host, vs. always requiring `[net] advertise =` on multi-host installs.
- Whether `scope = public` ships at all in v1, or stays a documented-but-unbuilt option.
- Whether the WAD-set auto-scan fallback is worth building for v1, or `[wadset.*]`
  sections in `syncdoom.ini` are required (config home decided: `syncdoom.ini`).
- Whether to expose persistent "rooms" later, and MQTT push for live lobby updates.
