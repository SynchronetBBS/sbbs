# SyncDuke multiplayer waiting room + clean-exit safety net (design)

**Date:** 2026-07-02
**Component:** `xtrn/syncduke/lobby.js`, `xtrn/syncduke/syncduke_lib.js` (JS lobby);
`src/doors/syncduke/syncduke_net.c` (door). Reuses `exec/load/game_lobby.js`
(`gl.*` registry + panel helpers) unchanged.
**Reference:** SyncDuke's existing `sd_lobby_wait` live panel + `sd_create`/`sd_join`
(`lobby.js`); the peer master/join handshake in `syncduke_net.c`. Contrast: SyncDOOM
has no purpose-built waiting room (it falls through to the Doom engine's stock C
netgame-sync screen; "a proper waiting room is still TODO" in its `sd_create`).

## Goal

Replace the SyncDuke master's silent, blocking "Loading Duke Nukem 3D…" wait — which
hangs for 60s and then **warps the host into a solo game** if nobody joins — with an
interactive **JS waiting room** the host can quit/page from, and a door **safety net**
so a failed multiplayer connect exits cleanly instead of falling back to single
player. Applies identically to co-op and dukematch.

## Problem (current behavior)

`sd_create` writes the registry entry and **immediately** launches the door as
master (`sd_play(sd_cmd("master", …))`). The door blocks in `sd_net_handshake`
(`syncduke_net.c`, 60s), showing Duke's raw "Loading…" screen with no BBS UI — no
quit, no who's-online, no paging, no indication anything is waiting. On timeout the
handshake returns 0 → `initmultiplayers` sets `numplayers = 1` → the door warps into
the chosen level **single-player**. The host who wanted a multiplayer game silently
ends up alone.

## Key facts that shape the design

- **The wait can live in JS, not C.** The joiner's door retries `HELLO` every 100ms
  for 60s (`syncduke_net.c` `sd_net_handshake` join branch), and the master's UDP
  **port is written into the registry entry at create time** — so the joiner knows
  where to dial before the master's door exists. The master therefore does **not**
  need to launch first; it can keep the JS lobby running (the waiting room) and spawn
  the door only once a joiner has committed. The doors then connect in ~1–2s and
  Duke's "Loading…" screen merely flashes by. (This is why SyncDuke can do in JS what
  SyncDOOM leaves to the engine's C sync screen: SyncDOOM launches door-first, so its
  wait is necessarily inside the running C engine with the JS lobby blocked in
  `bbs.exec`.)
- **The 2-player cap is in the transport, not the wait.** `syncduke_net.c`'s
  single master↔one-joiner handshake, `getpacket`'s "peer 0 or peer 1" sender ID, and
  `sd_net_become`'s hardcoded `numplayers = 2` are the limit; the engine itself is
  `MAXPLAYERS = 16`. So the waiting room and registry are designed **player-count
  aware** (carry a target count, default 2) so a future N-player transport upgrade
  needs no waiting-room redesign — but building that transport is **out of scope**.

## Scope

**In scope**
- `sd_create` enters a **JS waiting room** instead of launching the door immediately;
  it launches the door only when a joiner commits, and removes the entry on cancel/exit.
- `sd_join` **claims** the game (registry `status: waiting → joining`) instead of
  silently removing the entry, so the master's room detects the commit.
- A waiting-room loop (`sd_wait_room`) reusing the existing panel/`nodesync`
  machinery: header + who's-online panel + elapsed timer; keys **Q** (cancel) and
  **P** (page nodes); indefinite wait.
- **C safety net** (`syncduke_net.c`): when a net role (`master` OR `join`) is set
  but the handshake fails, the door **exits cleanly** instead of dropping to
  single-player.
- Model helpers + headless tests for the registry status transitions.

**Out of scope (YAGNI / deferred)**
- Actual >2-player support (multi-peer transport, N-way packet routing). The design
  only avoids *baking in* the 2-player assumption; it does not implement N-player.
- Any wait **timeout / countdown** — the host waits indefinitely (per decision); the
  only exits are a joiner arriving or the host pressing Q.
- A waiting-room "start single-player" key — SyncDuke's main menu already has
  **P = play solo**; the waiting room never needs its own solo path.
- Any change to `game_lobby.js` or the netcode transport/handshake protocol itself
  (only the *fall-back-to-solo* branch changes, in `syncduke_net.c`).

## Architecture

Three units with clean boundaries.

### 1. Registry coordination protocol (the contract)

The registry entry (`gl.write_game`/`list_games`/`remove_game`) already carries
`status`, `players`, `maxplayers`, `port`, `mode`, `level`, …. The status lifecycle
becomes:

- **`waiting`** — master created it, sitting in the waiting room. Listed as joinable
  (`sd_list_open_games` already filters `status == "waiting"`).
- **`joining`** — a joiner has claimed it and is launching/dialing. No longer listed
  as joinable (the existing filter drops it), so a second node can't also claim it.

Transitions and **ownership**:
- The **master owns the entry's existence**: it creates it (`waiting`) and is the only
  party that **removes** it (on cancel, or via the `finally` after the door exits).
- The **joiner only mutates status** `waiting → joining` (a "claim"); it never creates
  or removes the entry. A `sd_claim_game(entry)` helper re-writes the same entry file
  (stem derived from `entry.file`) with `status:"joining"` — no `game_lobby.js` change.
- The master's room polls the entry each tick and launches the door when it observes
  `status == "joining"` (today: one joiner = ready; the launch predicate is factored
  so a future N-player build changes it to "claims ≥ maxplayers−1" without touching the
  UI).

**Race handling (file coordination over a possibly-mounted registry):**
- *Master cancels between the joiner's read and its claim-write:* the joiner's
  `sd_claim_game` may recreate the file as `joining`. Harmless: it isn't listed
  (not `waiting`), the joiner's door then finds no master and exits via the safety
  net, and the orphaned entry is reaped by the existing `stale` age filter in
  `gl.list_games`. The joiner re-checks `file_exists(entry.file)` immediately before
  claiming to shrink the window.
- *Two joiners claim near-simultaneously (same-host, sub-second):* the master launches
  on the first `joining` it sees and stops advertising; the second joiner's door fails
  to connect (the master accepts one `HELLO`) and exits via the safety net. Rare,
  self-healing. (Same class of race the current `remove_game`-to-claim already has.)

### 2. JS waiting room (`sd_wait_room` in `lobby.js`)

Modeled on the existing `sd_lobby_wait` (same `console.ctrlkey_passthru = "-P"`,
`bbs.nodesync()` each tick for telegrams/interrupt, ~1/s repaint via
`console.inkey(K_UPPER|K_NOECHO, 1000)`), but its loop returns **why it ended** rather
than a menu key:

- **Header:** `Waiting for a player to join E1L<n> <name> (<co-op|dukematch>)…` plus an
  **elapsed** timer `m:ss` (counts **up** from room entry — informational, not a
  countdown) and a hint line: `\1hQ\1n cancel   \1hP\1n page nodes`.
- **Body:** the existing who's-online + recent-activity panel (`sd_draw_panel`), so the
  host watches nodes and can tell when the joiner's node enters SyncDuke.
- **Each tick:** `nodesync`; re-read this entry (`gl.list_games` filtered to our
  `port`, or a direct read of `entry.file`); if `status == "joining"` → return
  `"joined"`. `inkey`: `Q` → return `"cancel"`; `P` → run the existing page flow
  (`sd_prompt_page`/`sd_send_pages`) then keep waiting; other keys ignored.
- Returns `"joined"` or `"cancel"`.

`sd_create` becomes: pick level → pick mode → alloc port → **optionally page up
front** (keep existing pre-page) → write entry (`waiting`) → `sd_wait_room(entry)`:
  - `"cancel"` → `gl.remove_game(entry)`; return to menu.
  - `"joined"` → `try { sd_play(sd_cmd("master", port, null, level, mode, dmopts)); }
    finally { gl.remove_game(entry); }` (unchanged launch, now gated on the commit).

`sd_join` becomes: pick a `waiting` game → `sd_claim_game(sel)` (status→`joining`) →
`sd_play(sd_cmd("join", …, sel.mode||"coop", cfg.dukematch))` (unchanged launch). It
no longer calls `gl.remove_game` (the master owns removal).

### 3. C safety net (`syncduke_net.c`)

`initmultiplayers` currently: default `numplayers = 1`; if role is `master`/`join`,
call `sd_net_handshake`, which on failure returns 0 and leaves `numplayers = 1` →
solo. Change: **when a net role is configured and the handshake fails, exit the door
cleanly** (log the reason + flush/close, freeing the node) instead of continuing at
`numplayers = 1`. A launch with **no** net role (the `P = play solo` menu path) is
unaffected and still plays single-player. This removes the silent-solo fallback for
both roles: a master whose committed joiner bails, and a joiner whose master vanished,
both exit back to the lobby rather than warping into a lone game.

(Timeout tuning: because the master now launches only after the joiner is already
dialing, a normal connect is ~1–2s; the existing 60s handshake window still covers a
slow cross-host connect. Shortening the master's post-commit timeout is a possible
follow-up but not required.)

## Data flow

**Happy path (2-player, either mode).**
1. Host: `C` → level → mode → port allocated → entry written `status:"waiting"` →
   host enters `sd_wait_room` (who's-online panel, elapsed timer, Q/P).
2. Joiner: `J` → picks the `waiting` game → `sd_claim_game` sets `status:"joining"` →
   joiner's door launches and starts dialing (retrying `HELLO`).
3. Host's room sees `joining` → launches the master door → both connect in ~1–2s →
   play. On exit, the host's `finally` removes the entry.

**Host cancels:** `Q` in the room → entry removed → back to menu. A later joiner sees
no `waiting` game.

**Joiner bails after claiming:** master door launches, no `HELLO` arrives → handshake
times out → **safety net exits** the door (not solo) → host back at the menu; entry
removed by `finally`.

## Testing

- **Headless (jsexec), model layer:** `sd_claim_game(entry)` produces an entry with
  `status:"joining"` at the same file; a `sd_game_ready(entry)`-style predicate returns
  true iff `status == "joining"`; `sd_list_open_games` still excludes a `joining`
  entry. Same harness style as the dukematch `test_dukematch_lib.js` (load the lib as
  a namespace object, stub `user`, write/read the registry under a temp game dir).
- **C safety net:** exercised by the existing socketpair/net harness pattern — a
  `master` role with no peer must **exit** (non-solo) rather than return to the game
  loop; a no-role launch must still reach single-player. If a unit harness is
  impractical, this is covered by the live test below and the code path is small.
- **Live 2-node (required before commit, per project rule):** (a) Create → the waiting
  room shows, who's-online updates, elapsed advances, **Q** returns to menu, **P**
  pages; (b) second node Joins → host's room auto-launches, both connect and play
  (co-op *and* dukematch); (c) Join then immediately hang up the joiner before connect
  → the master exits to the lobby, **not** into a solo game.

## Integration with the in-flight dukematch work

The dukematch feature (mode prompt, frag events, docs) is implemented and review-clean
but **uncommitted**, pending exactly the live 2-player test this waiting room unblocks.
This design **builds on** those uncommitted `lobby.js`/`syncduke_lib.js` changes
(`sd_create`/`sd_join` already thread `mode`/`cfg.dukematch`). Plan of record: build
the waiting room on top, run **one** combined live session that validates create →
wait → join → play for both co-op and dukematch plus the frag feed and the safety
net, then commit the whole set (dukematch commits, then the waiting-room commit).

## Risks / open points

- **Registry mutation by the joiner.** The joiner now writes the master's entry file
  (status flip). Deriving the stem from `entry.file` must match the master's
  `sd_entry_name(port)` naming; the plan verifies the round-trip in a headless test.
- **Mount attribute-cache lag (cross-host).** A joiner's `joining` flip can take
  ~15–60s to be visible to the master over SMB (same lag the existing `stale=90`
  already accommodates). Same-host is instant — the common case. Acceptable; the wait
  is indefinite so lag only delays, never breaks.
- **Safety-net exit cleanliness.** Exiting from `initmultiplayers` (deep in engine
  init) must free the node without corrupting terminal state; the plan picks the exact
  clean-exit call (e.g. the door's existing hangup/flush path) and validates the node
  is freed.
- **Divergence note (house rule):** this gives SyncDuke a *proper* JS waiting room
  that SyncDOOM still lacks (SyncDOOM leans on the engine's C sync screen; its JS
  waiting room is a standing TODO). Intentional and documented so the two doors' create
  flows are understood to differ.
