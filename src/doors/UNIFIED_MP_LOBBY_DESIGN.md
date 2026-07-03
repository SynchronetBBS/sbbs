# Unified `[M]ultiplayer` Lobby Entry — Design

**Date:** 2026-07-03
**Applies to:** SyncDOOM (`xtrn/syncdoom/`) and SyncDuke (`xtrn/syncduke/`), plus
the shared model layer `exec/load/game_lobby.js`.

## Goal

Replace the two separate lobby menu keys **`[J]`oin** and **`[C]`reate** with a
single **`[M]ultiplayer`** option in both doors. Selecting it runs a short,
mostly-yes/no flow that funnels the common case — a handful of users who want to
play *together* — into one game, while still allowing (but not encouraging) a
user to start a second, separate game. This reduces menu clutter and makes the
majority multiplayer path simpler.

## Motivation

A typical BBS has fewer than four users online at once, so more than one
simultaneously-*played* multiplayer game is rare. Today a user must decide up
front whether to Join or Create, and two users arriving together can each Create
their own 1-player game instead of playing together. `[M]` removes that up-front
choice and, via a short setup-window lock, nudges the second arrival into joining
the game the first is forming.

## Global constraints

- **SpiderMonkey 1.8.5** — no `let`/`const`/arrow/template-literals/`for..of`/
  `.includes`/`.find`; the doors' JS must stay ES5-in-SM185.
- **Both doors stay behaviorally aligned** ("more similar than different"). The
  `[M]` flow, prompts, and mutex behave identically; only the underlying
  join/create internals differ (SyncDOOM N-player muster vs SyncDuke 2-player
  `.claim`) and are **not** changed by this work.
- **`game_lobby.js` changes are additive** (new `gl.*` helpers only).
- **Cross-host** (e.g. Vertrauen): the lock file lives in the shared games dir,
  like the registry entries, so it coordinates across hosts.

## Menu change

Both lobby menus currently expose (SyncDOOM): `J`oin, `C`reate, `P` single-
player, `L`og, `H`elp, `Q`uit, and `E` external (SyncDOOM only, gated — see
below). SyncDuke is the same minus `E`.

- Remove `J` and `C`; add **`M`ultiplayer** (dispatches to a new
  `sd_multiplayer()` in each door's `lobby.js`).
- **`P`/`L`/`H`/`Q` are unchanged.**
- The menu is rendered from each door's `lobby.msg` display file, so that file is
  updated to list `M` in place of `J`/`C`.

## The `[M]` flow

On select, count **waiting** games with the existing `gl.list_games` (see
"In-progress games" below):

- **0 games** → prompt: *"No multiplayer games are waiting. Create one? `[Y/N]`"*.
  - `Y` → create flow (below). `N` → back to menu.
- **1 game** → prompt: *"Join `<host>`'s game (`<wadset>`, `<mode>`, `n/max`)?
  `[Y/N]`"*.
  - `Y` → the door's existing join. `N` → *"Create one? `[Y/N]`"* (→ create flow
    or menu).
- **≥2 games** → show the numbered list, then *"Join which? `[1-N]`, or `[N]`o to
  skip"*.
  - a number → join that game. skip → *"Create one? `[Y/N]`"* (→ create flow or
    menu).

All prompts are single-key. The multi-choice "Yes/No/Create-new" is deliberately
avoided; the "create a second game while one exists" path is the rarest, so it's
reached as a *second* Y/N rather than a first-class option.

The join and create actions call each door's **existing** functions unchanged
(SyncDOOM `sd_browse`/`sd_create`; SyncDuke `sd_join`/`sd_create`) — this work is
the entry flow around them, not a rewrite of them.

## Creation mutex (setup-window lock)

To close the "both see 0 games, both create" race and funnel the second user into
joining, the create path is guarded by a short-lived lock using the built-in
`file_mutex(path [, text=hostname] [, max_age])` (atomic create; returns `true`
if acquired, `false` if held; a lock older than `max_age` seconds is treated as
stale and overwritten — so an abandoned lock self-reaps with no heartbeat code).

- **Lock file:** `<gamesdir>/creating.<door>.lock` (shared games dir; cross-host).
- **`max_age`:** ~**120 s** — comfortably longer than the interactive create
  prompts, so a legitimately in-progress create can't have its lock stolen, while
  a creator who hangs up mid-setup frees it within two minutes. (No heartbeat:
  `file_mutex` reaps on file age.)
- **Acquire at confirm-create**, *before* the WAD/mode/etc. picks (this is what
  closes the race).
- **Release** (remove the lock file) the instant the game entry is **registered**
  in the games dir — i.e. the lock spans picks→register only, **never** the
  waiting-room wait. From then on the game is discoverable and other users join
  it.
- Also release on abort/cancel of the create (`Q`/hangup) so it doesn't wait for
  the stale timeout.

### Waiter poll (second user)

If `file_mutex` returns `false` at confirm-create (someone is mid-setup), show
*"A multiplayer game is being set up — please wait…"* and **poll** every ~1.5 s:

1. **A game now appears** in the registry → drop the user into the §"1 game"/
   "≥2 games" **join prompt** (the funnel: they join instead of duplicating;
   "Create one? `[Y/N]`" is still reachable there for the deliberate second game).
2. **Lock freed *and* still no game** (creator bailed before registering) →
   acquire the lock and proceed with the create flow.
3. Otherwise keep waiting.
4. `Q` aborts to the menu at any time.

## In-progress games don't count

No new filtering is needed. `gl.list_games` only returns games still **gathering**
players; a game is removed from the registry at launch (the muster clears its
entry on "go"; the SyncDuke `.claim` clears on join), so an in-progress match
never appears in the count or the list.

## External-by-address (SyncDOOM only)

SyncDOOM's `sd_join_external()` is gated by `[net] allow_external` (default
**false**); SyncDuke has no external-join.

- **`allow_external` off (default):** external appears nowhere in the `[M]` flow.
- **`allow_external` on:** after the user declines to join a listed game *and*
  declines to create, append one final prompt — *"Join an external server by
  address? `[Y/N]`"* — then run `sd_join_external()`.

## Code layout

- **`game_lobby.js`** (shared, additive): new helpers —
  - `gl.mutex_path(cfg, door)` → the `creating.<door>.lock` path in the games dir.
  - `gl.acquire_create_lock(cfg, door)` → `file_mutex(...)` with the standard
    `max_age`; returns bool.
  - `gl.release_create_lock(cfg, door)` → remove the lock file (idempotent).
  - (`gl.list_games` is reused for the count/list; no change.)
- **Each door's `lobby.js`**: a new `sd_multiplayer()` dispatcher implementing the
  flow above and the waiter poll, calling the door's existing join/create
  functions. The `[M]` key routes to it; `[J]`/`[C]` keys and their handlers are
  removed.
- **Each door's `lobby.msg`**: menu text updated (`M` replaces `J`/`C`).

## Testing

- **Headless (`jsexec`):** the mutex + count-branching logic is testable against a
  temp games dir — fabricate 0/1/2 waiting entries and assert the chosen branch;
  run two processes contending on `file_mutex` on the same path to prove only one
  acquires (race closed) and the loser polls.
- **Manual (live door):** the interactive prompts, the waiter "please wait…" →
  auto-join transition, and the cross-host case stay manual live-tests, as now.

## Non-goals / out of scope

- **Not** unifying the two waiting-room *models* (SyncDOOM N-player muster vs
  SyncDuke 2-player `.claim`). `[M]` is an entry flow above them; both keep their
  current internals. (Unifying the models remains a separate, deferred item.)
- **Not** changing the in-game/netcode paths, the dedicated-server spawn, or the
  registry format.
- **Not** adding external-by-address to SyncDuke.
