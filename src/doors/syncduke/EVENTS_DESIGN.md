# SyncDuke events.jsonl + shared game_lobby.js events layer (design)

**Date:** 2026-07-01
**Component:** `src/doors/syncduke`, `exec/load/game_lobby.js`, `xtrn/syncduke/`
**Reference:** SyncDOOM (`src/doors/syncdoom/syncdoom.c` event log; `xtrn/syncdoom/syncdoom_lib.js` activity feed)

## Goal

Give SyncDuke a door-written `events.jsonl` activity log and a lobby "recent
activity" view, mirroring SyncDOOM — and put the game-agnostic events logic in the
shared `exec/load/game_lobby.js` so both lobbies can reuse it.

## Scope

This is **Sub-project A** of the full "converge both lobbies on `game_lobby.js`"
goal (option C). It delivers SyncDuke events end-to-end and creates the shared
events layer, proven on SyncDuke, with **SyncDOOM untouched**.

**Sub-project B (separate spec, later):** migrate SyncDOOM's lobby onto
`game_lobby.js` — first its events feed (delete the dup from `syncdoom_lib.js`),
then its duplicated net/registry/paging/who's-online — so both lobbies truly
share the library. Not covered here.

**In scope (A)**
- SyncDuke **door** emits `events.jsonl` (`start` / `level` / `death`) via a
  `-eventlog <path>` arg.
- **`game_lobby.js`** gains generic events helpers (read / prune / feed + time utils).
- SyncDuke **lobby** passes `-eventlog`, adds a "recent activity" view + prune,
  using the shared helpers.

**Out of scope**
- Any change to SyncDOOM or `syncdoom_lib.js` (that is Sub-project B).
- Frag/deathmatch events (Duke co-op has no DM; N/A).

## Architecture

Three units, clean boundaries:

1. **Door (C) — new `syncduke_events.c`** (includes `duke3d.h`, like
   `syncduke_game.c`): owns the whole event system — the `-eventlog` path, a JSON
   writer, the `start`/`level`/`death` emitters, and a per-frame detector. Chosen
   over folding into `syncduke_game.c` (distinct concern, its own state/writer). The
   detector runs from `_nextpage()` (`syncduke_plat.c`).
2. **`game_lobby.js` (shared, generic):** gains game-agnostic events helpers — read
   and prune a JSONL log, and build a formatted activity feed from a caller-supplied
   per-event formatter, plus `ago()`/`mmss()` time utilities. No game specifics.
3. **SyncDuke lobby (JS):** passes `-eventlog`, supplies the SyncDuke event→text
   formatter, adds the activity view + prune, all via the shared helpers.

Boundary: the door *writes* the JSONL; `game_lobby.js` *reads/prunes/scaffolds* it
generically; the lobby *supplies the per-event wording*.

## Door event system (C)

- **`-eventlog <path>`** parsed in `syncduke_config.c` into a global (mirrors
  SyncDOOM's arg handling and its addition to the recognized-arg list). Empty ⇒
  logging off (a direct DOOR32 launch with no lobby simply emits nothing).
- **Writer:** a minimal JSON string escaper + `sd_ev_emit(json)` doing one
  append-mode line write (atomic, no lock) — exactly SyncDOOM's model (each node's
  door appends its own events; the lobby reads with `json_lines` `recover`).
- **Schema — mirrors SyncDOOM** so the shared reader parses both games uniformly:
  - `start`: `{"time":<unix>,"type":"start","node":N,"usernum":N,"user":"..","term":{"desc":"..","cols":N,"rows":N},"tier":"..","build":{"hash":"..","date":".."},"mode":"..","skill":N}` — emitted once, at the first frame after the tier + terminal size have resolved (guarded by a "started" flag).
  - `level`: `{"time":..,"type":"level","node":N,"user":"..","map":"E#L#","secs":N,"skill":N}` — on level change; `secs` = the *previous* level's elapsed seconds.
  - `death`: `{"time":..,"type":"death","node":N,"user":"..","map":"E#L#","secs":N}` — on the rising edge of `syncduke_player_dead()`.
- **Values** from existing engine state / accessors:
  - `map` = `"E%dL%d"` with `ud.volume_number+1`, `ud.level_number+1` (the +1 form the game itself displays).
  - `skill` = `ud.player_skill + 1`.
  - `mode` = `"co-op"` if `syncduke_net_role` is `master`/`join`, else `"single"`.
  - `build` = `GIT_HASH` / `GIT_DATE` (`git_hash.h`).
  - `tier` = the active tier's name (expose `sd_tier_name()` via an accessor, or reuse an existing tier-name export).
  - `node` = `sbbs_my_node()`; `user` = the door alias (`syncduke_door_alias()`); `usernum` from the `-home` path (as SyncDOOM derives it).
  - **elapsed `secs`** = a `totalclock` delta: capture `totalclock` at each level entry; on the next `level`/`death`, `secs = (totalclock - level_start)/120` (Build's 120 Hz tick). Confirm the tick rate/`totalclock` availability at implementation.
- **Detection (`syncduke_events_tick()`):** compare last `(volume,level)` to current → emit `level` on change (and on first entry, with `secs` 0); track `syncduke_player_dead()` low→high → emit `death`. All gated on `-eventlog` being set and the engine being in a real game (`MODE_GAME`, not demo — reuse the same guard as the node status). The `start` event fires once when the tier/term are known.
- **Engine-state access** lives in `syncduke_events.c` (it includes `duke3d.h`); the writer/path could be split out but is kept together for one cohesive module.

## `game_lobby.js` shared events API

Added to the object `game_lobby.js` returns (all game-agnostic):

- `read_events(path, max)` → array of parsed event objects, last `max`, via
  `json_lines.get(path, count, max_line_len, /*recover*/ true)`. Returns `[]` if the
  file is absent/empty.
- `prune_events(path, cap, keep)` → if the file has more than `cap` lines, rewrite it
  keeping the last `keep`. Rare, bounded (mirrors SyncDOOM's `sd_prune_events`).
- `event_feed(path, max, fmt)` → array of display strings for the last `max` events,
  where `fmt(event)` is the caller's per-event renderer; the lib does the read +
  iteration and skips events `fmt` returns falsy for.
- `ago(t)` → compact "just now / Nm ago / Nh ago / Nd ago" from a unix time.
- `mmss(secs)` → "M:SS" from a seconds count.

These are additive; no existing `game_lobby.js` export changes, so SyncDuke's current
lobby use is unaffected. (Sub-project B will point SyncDOOM's `sd_ago`/`sd_mmss`/
event-feed at these.)

## SyncDuke lobby (JS)

- `SD_EVENTS = backslash(system.data_dir + "syncduke/events.jsonl")` in
  `syncduke_lib.js` (mirroring `SD_GAMES`).
- `sd_cmd()` (`syncduke_lib.js`) appends `-eventlog "<SD_EVENTS>"` to the door command
  (mirroring SyncDOOM's `lobby.js` line 40; quote the path).
- A SyncDuke per-event formatter `sd_event_text(e)` — e.g. `start` → "joined",
  `level` → "reached E1L1", `death` → "died on E1L1", each suffixed with
  `gl.ago(e.time)`; unknown types → "" (skipped).
- A "recent activity" menu entry → `sd_show_activity()` printing
  `gl.event_feed(SD_EVENTS, N, sd_event_text)`.
- Prune on lobby entry: `gl.prune_events(SD_EVENTS, 2000, 1000)`.
- SpiderMonkey 1.8.5 compatible (no modern ES) — matches the existing lobby JS.

## Error handling / edge cases

- No `-eventlog` (direct launch / no lobby) ⇒ door writes nothing; lobby feed shows
  "no recent activity". No errors.
- Malformed / partial lines ⇒ `json_lines` `recover` skips them.
- Unbounded growth ⇒ `prune_events` caps it on lobby entry.
- Multiple nodes ⇒ each door appends its own events (append-atomic, no lock), as
  SyncDOOM does.

## Testing

- **`game_lobby.js` events API — automated (jsexec):** a headless test writes a temp
  `.jsonl`, then asserts `read_events` (count/order/recover on a bad line),
  `prune_events` (caps to `keep`), `event_feed` (formats + skips falsy), and
  `ago`/`mmss` formatting. `game_lobby.js` is UI-free by design, so this is genuine
  coverage for the shared code.
- **Door — build + runtime:** door builds clean; a live session appends `start`, then
  `level` on each level change, and `death` on death — verified by inspecting
  `data/syncduke/events.jsonl` and the lobby's activity view (runtime pass by the user,
  like the node features).
- **SyncDuke lobby — runtime:** the "recent activity" menu shows the formatted feed;
  prune bounds the file.

## Files touched

- `src/doors/syncduke/syncduke_events.c` (new) — path, JSON writer, start/level/death
  emit, per-frame detector.
- `src/doors/syncduke/syncduke.h` — declarations (`syncduke_events_tick`, init/emit hooks, `-eventlog` setter if needed).
- `src/doors/syncduke/syncduke_config.c` — parse `-eventlog`, store the path.
- `src/doors/syncduke/syncduke_plat.c` — call `syncduke_events_tick()` per frame.
- `src/doors/syncduke/syncduke_io.c` — expose a tier-name accessor if not already public.
- `src/doors/syncduke/CMakeLists.txt` — add `syncduke_events.c`.
- `exec/load/game_lobby.js` — add `read_events`/`prune_events`/`event_feed`/`ago`/`mmss`.
- `xtrn/syncduke/syncduke_lib.js` — `SD_EVENTS`, `-eventlog` in `sd_cmd`, `sd_event_text`.
- `xtrn/syncduke/lobby.js` — "recent activity" menu item + `sd_show_activity` + prune on entry.
- `exec/load/tests/` (or a jsexec test file) — `game_lobby.js` events-API test.
