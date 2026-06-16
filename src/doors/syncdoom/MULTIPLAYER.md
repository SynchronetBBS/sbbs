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

## Topology — why lockstep works on a BBS

Every player's door runs on the **same host** as the BBS, so the netcode is **localhost
UDP** (127.0.0.1). One **dedicated (headless) server** process coordinates a match; each
player's door connects as a **client**.

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

## Game modes — already plumbed (vanilla)

- **Co-op** — default (`deathmatch=0`): shared maps, fight monsters together.
- **Deathmatch** — `-deathmatch` (DM 1.0: items + weapons respawn).
- **Altdeath** — `-altdeath` (DM 2.0: weapons stay).
- Modifiers: `-skill N`, `-warp <map>`, `-respawn`, `-fast`, `-nomonsters`.

The match creator picks these; the lobby passes them to the dedicated server.

## Cross-node discovery — a shared registry in `data/`

All nodes share `data_dir`, so games are discovered via the **filesystem**, with no
inter-door networking and no central matchmaker.

`<data_dir>/syncdoom/games/` — **one file per live match**, e.g. `20001.ini`:

```
host       = Digital Man
wad        = freedoom2.wad +myhouse.wad
mode       = coop            ; coop | deathmatch | altdeath
skill      = 3
map        = MAP01
port       = 20001           ; the dedicated server's localhost UDP port
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
3. **Join** → door connects to `127.0.0.1:<port>`; the **server** updates `players`/
   `status`.
4. **End/crash** → server deletes its file on clean exit; stale files pruned by heartbeat
   age (dead `pid` or heartbeat > ~15 s).

**Single-writer rule:** each match's server is the only writer of its own file; doors are
read-only listers. That keeps it race-free across nodes.

Alternatives considered: chocolate's `net_query` UDP LAN discovery (kept for the connect
handshake, but localhost broadcast across many per-match ports is fiddly for *listing*);
MQTT (`sbbs/syncdoom/games/#`) for push-style real-time lobby updates — nice later, but an
extra dependency the file registry doesn't need.

## WADs

- **Single-player multi-WAD already works** — `-iwad <iwad> -file a.wad b.wad -merge c.wad`
  passes straight through to doomgeneric (`w_main.c`).
- **In multiplayer every player must load the identical IWAD + PWAD set** — the sim is
  deterministic, so any mismatch desyncs/crashes. The lobby enforces it: the creator's WAD
  set becomes the match's, and joiners inherit the same args. Easy here — all doors read
  the same shared `xtrn/syncdoom/` WAD dir on the one host.

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
- Whether to expose persistent "rooms" later, and MQTT push for live lobby updates.
