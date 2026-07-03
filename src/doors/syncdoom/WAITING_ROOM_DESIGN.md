# SyncDOOM JS waiting room (lobby muster) — design

**Date:** 2026-07-02
**Component:** `xtrn/syncdoom/lobby.js`, `xtrn/syncdoom/syncdoom_lib.js` (JS lobby);
`exec/load/game_lobby.js` (shared muster primitives — additive). **No engine/netcode
(`src/doors/syncdoom/*.c`) change.**
**Reference:** the just-shipped SyncDuke waiting room (`xtrn/syncduke/lobby.js`
`sd_wait_room`, `syncduke_lib.js` `.claim` helpers) — this generalizes that pattern
from a 2-player claim to an N-player muster. SyncDOOM server model:
`src/doors/syncdoom/mp_server.c` (detached, self-daemonizing dedicated server) and
`xtrn/syncdoom/lobby.js` `sd_spawn_server`/`sd_create`/`sd_browse`.

## Goal

Replace SyncDOOM's create/join flow — where the creator and joiners **connect
immediately** to the dedicated server and wait at Doom's blank C netgame-sync screen
("a proper waiting room is still TODO" in the current `sd_create`) — with a **JS
lobby muster**: players assemble in a JS waiting room (who's-assembled list, Start /
Q / P, beep) and their game clients launch only once the muster completes, at which
point everyone connects and the game starts.

## Problem (current behavior)

`sd_create` spawns the detached server, then **immediately** `sd_play`s the creator's
client (connects, becomes controller, waits at Doom's sync screen). `sd_browse`
likewise launches a joiner's client immediately. So the "wait for players" experience
is the vanilla Doom netgame sync screen — no BBS who's-online, no page, no cancel, no
control over when to start. The creator must connect first (to win the controller slot
and carry `-skill`/`-deathmatch`), which is exactly what prevents a JS waiting room:
a connected client is a running door, and `bbs.exec` blocks the JS lobby.

## Key facts that shape the design

- **The wait must be deferred-connect.** A JS waiting room is only possible if the
  players' clients are **not connected** during the muster (a connected client blocks
  the lobby). So the muster is a lobby/registry-level gathering; clients launch only
  when it completes.
- **The creator orchestrates the launch → controller is unchanged.** Because the
  creator decides when the muster completes and fires the "go", it launches its own
  client **first** and connects first → it is the netgame controller, so its
  `-skill`/`-deathmatch` flags define the match exactly as today. **No NET_SV change.**
- **The server writes the registry only when given `-gamesdir`.** `mp_server.c:139`:
  "Without `-gamesdir` we don't register." So a muster-spawned server launched
  **without** `-gamesdir` writes/heartbeats/removes **no** registry entry — the lobby
  fully owns discovery during the muster, with zero server-side conflict. (The server
  otherwise runs normally: binds `-port`, forces `-maxplayers`, starts when that many
  connect.)
- **Start-early and auto-complete are the same code path.** The server auto-starts
  when `maxplayers` clients connect. If the creator spawns the server with
  `-maxplayers = <players assembled at "go">`, then "Start now with K" and "auto-start
  at the target T" are identical: K clients launch, K connect, the server starts.
- **The server self-daemonizes** (`mp_server.c:255` `fork()`+`setsid()`; Windows
  `CreateProcess(DETACHED_PROCESS)`), so `bbs.exec("syncdoom -spawnserver …")` returns
  immediately — JS never needs to background a process itself.

## Scope

**In scope**
- A **lobby-owned "mustering" registry entry** (SyncDuke-style, heartbeated) that
  `sd_browse` lists as joinable; the current server-written entry is unused on the
  muster path.
- **Waiter markers**: a joiner registers in the muster by dropping a per-node marker
  file so the creator can count *and* list who's assembled.
- **Creator waiting room** (`sd_muster_host`): who's-assembled list + "K / T", keys
  **S**tart (≥2), **Q** cancel, **P** page; auto-fires "go" at the target; beep on a
  new arrival.
- **Joiner waiting room** (`sd_muster_join`): register, then "waiting for host (K / T)",
  poll for "go"; **Q** leaves (removes its marker); beep + launch on "go".
- **"go" signal**: a marker carrying the server address; the creator spawns the server
  (with `-maxplayers = K`, **no `-gamesdir`**), writes "go", launches its client first.
- **Shared muster primitives** factored into `game_lobby.js` (marker + go + mustering-
  entry helpers) so this isn't a third copy of the coordination logic.

**Out of scope / deferred**
- **Any engine/netcode change.** The server is used as-is (minus `-gamesdir`).
- **Server "start with fewer after a grace".** If an assembled joiner's door fails to
  launch after "go", the server waits for K and the connected clients sit at Doom's C
  sync screen (the degraded case — see Risks). v1 accepts this rather than add a
  server-side auto-start-short.
- **Mid-game join** (vanilla Doom can't) and joining a game already past "go".
- **SyncDuke changes** — SyncDuke's 2-player claim stays; only the shared primitives
  it could later reuse are factored out.

## Architecture

Everything lives in the lobby + registry; the detached server just runs the game.

### 1. Muster coordination protocol

Files in the shared games dir (`data/syncdoom/games/`), all keyed by a stem
`<hostid>-<port>`:

- **Mustering entry `<stem>.ini`** (creator-lobby-owned, single writer). Written at
  create with `status:"mustering"`, `host`, `wadset`, `mode`, `addr`, `port`,
  `target`, `players` (assembled count), `maxplayers` (server max), and a `heartbeat`
  refreshed every ~20s so it stays visible during an indefinite muster (`gl.entry_age`
  honors `heartbeat`). `sd_browse` filters to `status == "mustering"`. The creator
  removes it when it fires "go" or cancels.
- **Waiter marker `<stem>.wait.<node>`** (joiner-owned, one per joining node; `.wait.*`
  ≠ `.ini`, so `gl.list_games` never parses it). Created when a joiner enters the
  muster; removed when the joiner leaves (Q) or launches on "go". The creator counts
  and lists them (`directory(<stem>.wait.*)`), reading each for the joiner's alias.
- **Go marker `<stem>.go`** (creator-owned). Written at "go" carrying the server
  `host:port` the joiners dial. Its existence is the launch signal; joiners poll it.

**The port is allocated up front** (`gl.alloc_port`) and written into the mustering
entry, so joiners know where they'll connect before "go" — the "go" marker only
*triggers* the launch.

**Ownership (one writer per file, as in SyncDuke):** creator writes the `.ini` and the
`.go`; each joiner writes only its own `.wait.<node>`. No shared-file races.

### 2. Creator waiting room (`sd_muster_host` in `lobby.js`)

Modeled on SyncDuke's `sd_wait_room` (reuses `nodesync`/Ctrl-P/panel/beep machinery):
- Header: "Hosting `<wadset> (<mode>)` — assembled **K / T**", then the **list of
  assembled players** (creator + each waiter marker's alias), and hints
  `\1hS\1n start   \1hQ\1n cancel   \1hP\1n page`.
- Each tick: `nodesync`; re-count waiter markers (`K = waiters + 1`); if a *new* waiter
  appeared, `console.beep()`; heartbeat the mustering entry every `SD_MUSTER_HEARTBEAT`
  (20s); repaint. Keys: **S** → if `K >= 2`, go; **Q** → cancel; **P** → page. Auto:
  when `K >= T`, go.
- **Go** (`sd_muster_go`): compute `K` = assembled count at this instant. Spawn the
  server with `-maxplayers K` and **no `-gamesdir`** (`sd_spawn_server` gains a "no
  registry" variant); write `<stem>.go` with the server address; remove the mustering
  entry; `beep`; then launch the creator's own client **first** (so it's controller)
  with the same `K` as its start threshold: `sd_play(connect, ["-players", K, "-skill",
  skill, (-deathmatch|-altdeath)…])`. (Today `sd_create` pairs the count as `-players n`
  on the client and `-maxplayers n` on the server; the muster substitutes the live `K`
  for both.) On the client's exit, sweep any leftover `.wait.*`/`.go` for the stem.

### 3. Joiner waiting room (`sd_muster_join` in `lobby.js`)

- On selecting a `mustering` game in `sd_browse`: drop `<stem>.wait.<node>` (contains
  the joiner's alias), then enter the wait loop.
- Each tick: `nodesync`; poll `<stem>.go`. If present → `beep`, remove our
  `.wait.<node>`, launch our client (`sd_play(server-addr, …)`) → connect. If the
  mustering entry has vanished with no `.go` (host cancelled) → "the host cancelled",
  remove our marker, return. Keys: **Q** → remove our marker, leave.
- Header: "Waiting for `<host>` to start — **K / T** assembled…", `\1hQ\1n leave`.

### 4. Launch / controller ordering

Creator writes `.go` then immediately launches its client; joiners see `.go` only on
their next poll tick (+ any mount lag) and launch after — so the creator connects
first and is the controller. Same reliability argument as SyncDuke's "master launches
on claim". Since the creator's client carries `-skill`/`-deathmatch`, the match
settings are unchanged from today.

### 5. Shared `game_lobby.js` primitives (additive; no behavior change to existing)

- `waiter_path(entryPath, node)` / `write_waiter(entryPath, node, alias)` /
  `list_waiters(entryPath)` (→ array of `{node, alias}`) / `remove_waiter(entryPath, node)`.
- `go_path(entryPath)` / `write_go(entryPath, addr)` / `read_go(entryPath)` (→ addr or
  null) / `clear_muster(entryPath)` (removes `.ini` + `.go` + all `.wait.*`).
- The mustering entry itself reuses `write_game`/`list_games`/`remove_game` (a
  `status:"mustering"` entry with a `heartbeat`), so no new entry mechanism.
- The wait-loop *structure* (panel + nodesync + Ctrl-P + inkey + heartbeat) is shared
  where clean; the per-door drawing stays door-side. Exact factoring is the plan's call
  — the goal is no third copy of the coordination logic.

## Data flow (happy path, K players)

1. Creator: `Create` → pick wadset/mode/**desired count T** → `gl.alloc_port` → write
   `<stem>.ini` `status:"mustering"` → `sd_muster_host`.
2. Joiners: `Browse` → pick the `mustering` game → drop `<stem>.wait.<node>` →
   `sd_muster_join` (waiting, polling `.go`).
3. Creator's room shows "K / T" + the list, beeping as waiters arrive. Creator **S**tarts
   (K≥2) or it auto-goes at T.
4. Go: creator spawns server `-maxplayers K` (no `-gamesdir`), writes `<stem>.go`
   (addr), removes `.ini`, launches its client first → controller.
5. Joiners see `.go` → beep, remove their marker, launch → connect. Server has K
   clients → starts. Everyone plays.
6. On any client exit, the creator sweeps leftover muster files for the stem.

**Joiner leaves:** Q in `sd_muster_join` → remove `.wait.<node>` → back to menu;
creator's next tick recounts K downward.
**Creator cancels:** Q in `sd_muster_host` → `clear_muster` (removes `.ini`; any
`.wait.*` are orphaned but harmless and swept next game / by stale age) → joiners see
the `.ini` gone with no `.go` → "host cancelled".

## Configuration

- Desired count **T**: prompted at create like today's "Number of players" (default 2,
  capped at the wadset's `maxplayers` / `[net] max_players`, ≤ Doom's 4).
- `[net]` reused as-is (advertise/bind/stale). `SD_MUSTER_HEARTBEAT = 20` (< `stale` 90).

## Testing

- **Headless (jsexec), model layer:** the `game_lobby.js` primitives — write/list/
  count/remove waiter markers under a temp games dir; `write_go`/`read_go`;
  `clear_muster` removes `.ini`+`.go`+all `.wait.*`; the mustering entry carries a
  fresh `heartbeat` and `sd_browse`'s filter includes `status:"mustering"`. Same
  harness style as SyncDuke's `test_waiting_room_lib.js`.
- **Live multi-node (required before commit):** (a) Create with T=3 → other nodes
  Browse + join → creator's list shows them assemble, count climbs, beeps; leave one →
  count drops; (b) **S**tart at K=2 (below T) → the two launch, connect, play; (c) let
  it auto-go at T=3 → three launch and play; (d) **Q** as host → joiners see "host
  cancelled"; (e) **Q** as joiner → removed from the host's list; (f) the degraded
  case: join, then hang up a joiner between "go" and connect → confirm only that match
  is affected (others unimpacted) and the host can abort its stuck client.

## Risks / open points

- **Bailed joiner after "go" (accepted for v1).** If an assembled joiner's door fails
  to launch/connect after "go", the server waits for K and the connected clients sit at
  Doom's C sync screen for that match; the creator aborts its client to recover. Adding
  a server-side "start with fewer after a grace" is deferred (it cuts against the
  no-netcode goal).
- **`sd_spawn_server` "no registry" variant.** The muster path must spawn the server
  **without** `-gamesdir` so it writes no competing registry entry; the existing
  `sd_spawn_server` always passes `-gamesdir`. This is a small lobby-side change (omit
  the flag on the muster path), not a server change.
- **Orphaned `.wait.*`/`.go` files.** Same class as SyncDuke's orphaned `.claim`:
  harmless without a live `.ini`, swept by the next game on that stem or an age sweep.
- **Mount attribute-cache lag (cross-host).** Waiter markers and the `.go` can lag
  ~15–60s across SMB (same lag `stale`/heartbeat already tolerate). Same-host is
  instant. The muster is host-driven and indefinite, so lag only delays.
- **Divergence note (house rule).** This gives SyncDOOM a JS waiting room with a real
  *joiner-side* wait (parked until "go"), which SyncDuke doesn't have (its joiner
  claims and launches immediately). Intentional — SyncDOOM musters N players; SyncDuke
  is a 2-player peer connect. The shared `game_lobby.js` primitives keep them from
  drifting further than the model requires.
