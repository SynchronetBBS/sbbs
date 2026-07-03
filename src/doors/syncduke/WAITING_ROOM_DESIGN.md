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
- `sd_join` **claims** the game by dropping a separate `.claim` marker file (instead of
  silently removing the entry), so the master's room detects the commit.
- A waiting-room loop (`sd_wait_room`) reusing the existing panel/`nodesync`
  machinery: header + who's-online panel + elapsed timer; keys **Q** (cancel) and
  **P** (page nodes); indefinite wait.
- **C safety net** (`syncduke_net.c`): when a net role (`master` OR `join`) is set
  but the handshake fails, the door **exits cleanly** instead of dropping to
  single-player.
- Model helpers (`heartbeat` field, `.claim` read/write/clear) + headless tests.

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

The master owns a `.ini` **entry** file; the joiner signals with a separate `.claim`
**marker** file. Two files, **one writer each** — deliberately, so the master's
heartbeat re-writes and the joiner's claim never collide on the same file (a
single-file "flip the status" scheme has a real two-writer clobber race: the master's
next heartbeat would revert a joiner's just-written `joining` status back to
`waiting`, and the launch signal would be lost).

- **Entry `<stem>.ini`** (master-only writer). Created `status:"waiting"`, listed as
  joinable (`sd_list_open_games` already filters `status == "waiting"`). Because
  `gl.list_games` hides entries older than `[net] stale` (90s), an **indefinite** wait
  requires the master to **heartbeat** — re-write the entry every `SD_WAIT_HEARTBEAT`
  (20s) so its `heartbeat` field stays fresh and joiners keep seeing it. `gl.entry_age`
  already prefers a `heartbeat` field over file mtime, so `sd_write_entry` gains a
  `heartbeat: time()` field and re-calling it re-heartbeats.
- **Claim `<stem>.claim`** (joiner-only writer). `sd_claim_game(entry)` writes it beside
  `entry.file` (same stem, `.claim` extension — **not** `.ini`, so `gl.list_games`
  never parses it as a game), opened `"wx"` — **exclusive create** (`O_EXCL`): the first
  joiner wins atomically and any later joiner's open fails (returns false → "already
  claimed", bail without launching). Its existence is the signal. (`'x'` is the working
  exclusive flag; the legacy `'e'` char is deprecated/no-op in Synchronet.) The master
  clears any stale `.claim` for its stem when it creates a game (before the entry is
  listed), so a marker left by a prior game that reused the port can't be mistaken for a
  joiner.

**Ownership / lifecycle:**
- Master: creates the entry (`waiting`), heartbeats it, and is the sole remover —
  `sd_clear_game(entryPath)` deletes both the `.ini` and any `.claim` on cancel or via
  the `finally` after the door exits.
- Joiner: only writes the `.claim`; it never creates, heartbeats, or removes the entry.
- The master's room polls `sd_game_claimed(entryPath)` (`.claim` exists?) each tick and
  launches the door on the first claim. (Today one claim = ready; a future N-player
  build counts claims against `maxplayers` — a one-line predicate change, no UI
  rework.)

**Race handling (file coordination over a possibly-mounted registry):**
- *Master cancels just as a joiner claims:* the orphaned `.claim` is harmless — no
  `.ini` remains for it to point at, so nothing acts on it; the joiner's door finds no
  master and exits via the safety net. The `.claim` is tiny and can be swept later; the
  master also deletes any `.claim` in `sd_clear_game`.
- *Two joiners claim near-simultaneously:* the exclusive (`O_EXCL`) claim resolves it
  atomically — one `sd_claim_game` succeeds, the other returns false and bails cleanly
  in JS (no wasted door launch). `O_EXCL` is reliable on the same host (the common
  case); on the rare cross-host path where network-filesystem `O_EXCL` may not be
  atomic, the master accepting only one `HELLO` remains the backstop (the loser's door
  exits via the safety net).

### 2. JS waiting room (`sd_wait_room` in `lobby.js`)

Modeled on the existing `sd_lobby_wait` (same `console.ctrlkey_passthru = "-P"`,
`bbs.nodesync()` each tick for telegrams/interrupt, ~1/s repaint via
`console.inkey(K_UPPER|K_NOECHO, 1000)`), but its loop returns **why it ended** rather
than a menu key:

- **Static header (`sd_wait_bg`):** title, `Hosting E1L<n> <name> (<co-op|dukematch>)`,
  `Waiting for a player to join…`, and a hint line `\1hQ\1n cancel   \1hP\1n page nodes`.
  No countdown/timer (there's no timeout); the live panel below shows the room is alive.
  Redrawn on entry and (via a `bgfn` passed to `sd_draw_panel`) when the panel height
  changes — so the panel's height-change redraw restores THIS header, not the lobby art.
- **Body:** the existing who's-online + recent-activity panel (`sd_draw_panel(bgfn)`), so
  the host watches nodes and can tell when the joiner's node enters SyncDuke.
- **Each tick:** `nodesync`; `if (sd_game_claimed(entry)) return "joined"`; else heartbeat
  the entry if `SD_WAIT_HEARTBEAT` has elapsed; repaint the panel; `inkey`: `Q` → return
  `"cancel"`; `P` → run the existing page flow (`sd_prompt_page`/`sd_send_pages`) then keep
  waiting; other keys ignored.
- Returns `"joined"` or `"cancel"`.

`sd_create` becomes: pick level → pick mode → alloc port → **optionally page up
front** (keep existing pre-page) → write entry (`waiting`) → `sd_wait_room(entry, port,
lev, mode)`:
  - `"cancel"` → `sd_clear_game(entry)`; return to menu.
  - `"joined"` → `try { sd_play(sd_cmd("master", port, null, level, mode, dmopts)); }
    finally { sd_clear_game(entry); }` (unchanged launch, now gated on the claim).

`sd_join` becomes: pick a `waiting` game → `sd_claim_game(sel)` (drop the `.claim`) →
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
   host enters `sd_wait_room` (who's-online panel, Q/P), heartbeating the entry.
2. Joiner: `J` → picks the `waiting` game → `sd_claim_game` drops the `.claim` marker →
   joiner's door launches and starts dialing (retrying `HELLO`).
3. Host's room sees the `.claim` → launches the master door → both connect in ~1–2s →
   play. On exit, the host's `finally` (`sd_clear_game`) removes the entry + claim.

**Host cancels:** `Q` in the room → entry removed → back to menu. A later joiner sees
no `waiting` game.

**Joiner bails after claiming:** master door launches, no `HELLO` arrives → handshake
times out → **safety net exits** the door (not solo) → host back at the menu; entry
removed by `finally`.

## Testing

- **Headless (jsexec), model layer:** `sd_write_entry` writes a fresh `heartbeat`;
  `sd_claim_game(entry)` creates the `.claim` marker; `sd_game_claimed(path)` flips
  false→true across a claim; `sd_clear_game(path)` removes both files. Same harness
  style as the dukematch `test_dukematch_lib.js` (load the lib as a namespace object,
  stub `user`, write/read the registry under a temp game dir).
- **C safety net:** exercised by the existing socketpair/net harness pattern — a
  `master` role with no peer must **exit** (non-solo) rather than return to the game
  loop; a no-role launch must still reach single-player. If a unit harness is
  impractical, this is covered by the live test below and the code path is small.
- **Live 2-node (required before commit, per project rule):** (a) Create → the waiting
  room shows, who's-online updates, **Q** returns to menu, **P** pages, and the game
  stays joinable past 90s (heartbeat); (b) second node Joins → host's room auto-launches,
  both connect and play
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

- **Two-file coordination, not a status flip.** The joiner writes only its own
  `.claim` marker (derived from `entry.file` by swapping `.ini`→`.claim`); the master
  is the sole writer of the `.ini`. This sidesteps the two-writer clobber a shared
  status flip would have (the master's heartbeat would overwrite the joiner's flip).
- **Mount attribute-cache lag (cross-host).** The master's heartbeat and the joiner's
  `.claim` can take ~15–60s to be visible across SMB (same lag the existing `stale=90`
  already accommodates; heartbeat at 20s keeps margin). Same-host is instant — the
  common case. The wait is indefinite, so lag only delays, never breaks.
- **Safety-net exit cleanliness.** Exiting from `initmultiplayers` (deep in engine
  init) uses `_exit(0)` — the same clean exit the door's hangup path uses (skips the
  engine atexit, frees the node). The live test confirms the node is freed and the
  terminal returns to the lobby cleanly.
- **Divergence note (house rule):** this gives SyncDuke a *proper* JS waiting room
  that SyncDOOM still lacks (SyncDOOM leans on the engine's C sync screen; its JS
  waiting room is a standing TODO). Intentional and documented so the two doors' create
  flows are understood to differ.
