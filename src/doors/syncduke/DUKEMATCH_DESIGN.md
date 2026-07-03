# SyncDuke dukematch (deathmatch) mode + frag activity (design)

**Date:** 2026-07-02
**Component:** `src/doors/syncduke` (door C), `xtrn/syncduke/` (lobby JS + config),
`exec/load/game_lobby.js` (shared feed — read-only reuse, no new API needed)
**Reference:** SyncDOOM frag model (`src/doors/syncdoom/syncdoom.c` `sd_event_frag` /
`sd_player_name`; `g_game.c` `sd_next_frag`), and SyncDuke's existing co-op path
(`syncduke_net.c`, `xtrn/syncduke/lobby.js` Create/Join, `syncduke_events.c`).

## Goal

Let two SyncDuke players play **dukematch** (Duke3D deathmatch), selectable from the
lobby's Create flow, and surface **frags** ("Duke fragged Rob") in the lobby's
recent-activity feed and live panel — reusing the existing co-op netcode and the
existing events/feed infrastructure unchanged wherever possible.

## Key engine facts (verified)

- **Mode is one flag.** Duke's command-line parser (`Game/src/game.c:7364`) reads
  `/c1` = Dukematch (spawn), `/c2` = Cooperative, `/c3` = Dukematch (no spawn),
  storing `ud.m_coop` = digit − 1 (0 = DM-spawn, 1 = co-op, 2 = DM-no-spawn). The
  lobby hardcodes `/c2` today.
- **Both peers pass matching launch flags; config is the shared source of truth.**
  Duke *does* have a start packet that broadcasts `m_coop`/respawn/`m_ffire`
  (`game.c:6403`, received `game.c:615`/`675`) — but it fires only from Duke's own
  in-game new-game menu (the `cheat_phase` path), **not** on the command-line warp
  launch the lobby uses. So consistency comes from the launch args, not an in-game
  exchange: the `/c` mode is taken from the shared registry entry (identical for both
  peers), and the `/m`//`/t` arena options come from `[dukematch]` in `syncduke.ini`
  — a file shared by every host of a multi-host BBS (exactly like `[net]`), so the
  options are identical across hosts with no per-peer negotiation. (A cross-host match
  between two *separate* installs with deliberately divergent `[dukematch]` files
  could differ — same caveat as any cross-host game — but a single BBS, one or many
  hosts, shares one config.)
- **Peer names already arrive.** When `numplayers > 1` (our handshake sets it to 2),
  the connect sequence sends packet **type 6 = names** to peers (`game.c:7932`),
  populating `ud.user_name[other]`. This already happens on the co-op path, so the
  raw material for "X fragged Y" is present with no netcode change.
- **Frags are already counted.** The deterministic lockstep simulation runs every
  player on both instances, so the global frag matrix `frags[killer][victim]`
  (`Game/src/global.c:199`) is identical on both. On a kill (`player.c:2639`):
  `frags[frag_ps][snum]++` for a cross-frag, else `ps[snum].fraggedself++` for a
  suicide. `ud.user_name[i]` names any player index; `ps[i].frag` is a per-player
  total.
- **Shareware maps suffice.** The shareware GRP ships no DM-only maps; deathmatch is
  played on the same E1L1–E1L6 maps co-op already offers. No level-list change.

Because each instance sees the same `frags[][]`, a door that emits **only its own
row** (`frags[myconnectindex][*]`) records each frag exactly once with no central
writer and no dedup — the same property SyncDOOM relies on.

## Scope

**In scope**
- **Lobby (JS):** Create prompts **game type** (Co-op / Dukematch) before the level
  picker; the registry entry gains a `mode` field; Join lists both game types with a
  **Mode** column; `sd_cmd` emits the correct `/c` flag; a `[dukematch]` config
  section supplies sub-mode + options.
- **Door (C):** `syncduke_events.c` emits a new `frag` event; `ev_mode()` reports
  `dukematch` / `dukematch-nospawn` / `co-op` / `single`; a small
  `syncduke_next_frag()` query added to `syncduke_game.c`.
- **Feed (JS):** `sd_event_text` in `syncduke_lib.js` gains a `frag` case. The
  shared `game_lobby.js` needs **no change** — it already renders whatever
  `sd_event_text` returns, so frags flow into the activity view and live panel for
  free.
- **Docs/config:** `syncduke.example.ini` `[dukematch]` block; `install-xtrn.ini`
  note; `SYNCDOOM_VS_SYNCDUKE.md` matrix refresh (dukematch → Yes).

**Out of scope (deferred / YAGNI, per review)**
- **Lobby end-of-match scoreboard.** Duke draws a live in-game frag bar
  (`displayfragbar`, `game.c:1780`) the player already sees; detecting "match end" in
  a respawn-forever DM means detecting a peer disconnect, which is fiddly. Frag
  events in the feed cover the social surface for v1.
- **Frag limit / time limit.** Matches run until a player quits, like co-op.
- **3+ players.** v1 stays strictly 2-player, like co-op.
- **New maps / full-game DM maps.** Shareware map pool only.

## Architecture

Four units, each with a clean boundary; the shared library is untouched.

### 1. Door mode — no new code, one flag (`xtrn/syncduke/syncduke_lib.js`)

`sd_cmd(role, port, peer, level, mode)` gains a trailing `mode` string
(`"coop"` | `"dm"` | `"dmnospawn"`, default `"coop"` for back-compat). It maps to the
`/c` flag and appends the `[dukematch]`-derived options for a DM master:

```
coop       -> /c2
dm         -> /c1                         (spawn: weapons/items respawn)
dmnospawn  -> /c3
```

Plus, for either DM sub-mode, options from config (below): `/m` when monsters are
off (`/m` = "monsters off" in the parser, `game.c:7417` — matches when the char
after `m` isn't `a`) and `/t` when item respawn is requested. Skill continues to come from the existing `[net] skill` default via
Duke's own default (no `/s` today; unchanged). There is no in-game start-packet
reconciliation on the warp-launch path (see "Key engine facts"), so **both** peers
pass matching flags: `/c` from the shared registry entry, and `/m`//`/t` from the
shared `syncduke.ini` `[dukematch]` — identical on every host of a BBS.

### 2. Lobby Create/Join UX (`xtrn/syncduke/lobby.js`)

- **`sd_pick_mode()`** — a small picker (mirrors `sd_pick_level`'s `console.uselect`
  style): "Co-op (play together)" / "Dukematch (deathmatch)". Returns
  `"coop"`/`"dm"`/`"dmnospawn"` or `null` to abort. When `[dukematch] submode =
  nospawn`, the Dukematch choice resolves to `"dmnospawn"`; otherwise `"dm"`. (One
  prompt, not two — sub-mode is a config default, consistent with how skill/port are
  config-driven, not prompted.)
- **`sd_create()`** — calls `sd_pick_mode()` first; abort on `null`. Threads `mode`
  into `sd_write_entry` (new `mode` field) and `sd_cmd`. The paging invite text and
  the "Game created" line say "co-op" or "dukematch" accordingly.
- **`sd_join()`** — the multi-game list gains a **Mode** column (`Co-op` /
  `Dukematch`); the single-game fast path names the mode. `sd_cmd`'s `mode` comes
  from the selected entry (`sel.mode`, defaulting to `"coop"` for pre-existing
  entries).
- Menu keys are **unchanged** (`C/J/P/L/H/Q`); `lobby.msg` art needs no new key. (A
  one-line "Create prompts for co-op or dukematch" hint in the art is a nice-to-have
  the sysop can add; nothing in code depends on it.)

### 3. Registry `mode` field (`xtrn/syncduke/syncduke_lib.js`)

`sd_write_entry(cfg, port, level, mode)` writes `mode: mode || "coop"`.
`sd_list_open_games` passes it through. A pre-existing entry without `mode` reads as
`"coop"` (back-compat: an in-flight co-op game created by an older lobby still lists
and joins correctly). The joiner's `sd_cmd` uses `entry.mode`.

### 4. Frag events (`syncduke_events.c` + `syncduke_game.c`)

**`syncduke_game.c`** — a new engine query beside the existing ones (it already
includes `duke3d.h`):

```c
/* Report the next not-yet-reported frag BY THIS PLAYER since the last call.
 * Returns 1 and sets *victim to the fragged player index, or 0 when none remain.
 * Diffs our own row of the global frags[][] matrix against a static shadow, so a
 * multi-frag tic drains one per call (loop until 0). Self-frags are NOT reported
 * here (they stay the existing 'death' event); this is cross-player kills only. */
int syncduke_next_frag(int *victim);
```

Implementation: a `static short seen[MAXPLAYERS]` shadow of
`frags[myconnectindex][j]`; scan `j != myconnectindex`; on `frags[me][j] > seen[j]`,
`seen[j]++`, `*victim = j`, return 1. Reset the shadow when a new game starts
(exposed via the existing `syncduke_events_tick` game-start edge, or lazily when
`myconnectindex`/game changes). Guard the whole thing on `ud.multimode > 1 &&
ud.coop != 1` (a real DM) so co-op and single-player never emit frags.

**`syncduke_events.c`:**
- `ev_mode()` becomes: `single` when no net role; else read `ud.coop` — `1` → `co-op`,
  `0` → `dukematch`, `2` → `dukematch-nospawn`. (Was hardcoded `co-op` for any net
  role.)
- New `ev_frag(int victim)`: emits
  `{"time",…,"type":"frag","node",…,"killer":"<my alias>","victim":"<ud.user_name[victim]>","map":"E#L#"}`
  — killer = `syncduke_door_alias()` (this door's player), victim named via
  `ud.user_name[victim]` (falls back to `"player N"` if empty, matching Duke's own
  fallback at `player.c:2662`). Reuses `ev_esc`/`ev_map`.
- `syncduke_events_tick()`: in the `in_game` branch, before/after the existing death
  edge, drain frags: `while (syncduke_next_frag(&v)) ev_frag(v);`. The existing
  `death` (suicide/PvE/level) and `start`/`level` events are unchanged; in DM the
  `level` event (map-change timer) simply won't fire much since DM stays on one map.

### 5. Feed rendering (`xtrn/syncduke/syncduke_lib.js`)

`sd_event_text(e)` gains, ahead of the existing cases:

```js
case "frag":
    return e.killer + " fragged " + e.victim;   // SyncDOOM-verbatim wording
```

No change to `game_lobby.js`, `sd_recent_events`, `sd_panel_cells`, or the lobby
draw loop — a `frag` is just another displayable event the existing feed/panel
render. `activity_max` / `activity_max_age` / `panel_rows` config all apply as-is.

## Config — `xtrn/syncduke/syncduke.example.ini`

New `[dukematch]` section (all optional; sane classic-DM defaults):

```ini
[dukematch]
; Dukematch sub-mode when a player picks Dukematch in the lobby's Create menu:
;   spawn   -- weapons/items respawn (the arcade default)   [/c1]
;   nospawn -- no respawns (grab-once)                      [/c3]
submode = spawn
; Monsters present during a dukematch. Classic dukematch is player-vs-player only
; (monsters off); set true for a monsters-in-the-arena variant.
monsters = false
; Respawn items/inventory over time (independent of weapon spawn). Default off.
respawn_items = false
```

`sd_load_config` reads `[dukematch]` into `cfg.dukematch` with these defaults via
`gl.apply_defaults` (same pattern as `cfg.net`). The lobby maps them to `/c`, `/m`,
`/t` in `sd_cmd`.

## Data flow (dukematch match)

1. Creator: `C` → `sd_pick_mode()` → Dukematch → `sd_pick_level()` → registry entry
   `{mode:"dm", level, …}` → optional page → `sd_cmd("master", port, null, level,
   "dm")` → door launched with `/v1 /l<n> /c1 [/m] [/t]`.
2. Joiner: `J` → sees the entry with Mode = Dukematch → `sd_cmd("join", …, level,
   sel.mode)` launches with the same `/c1 [/m] [/t]` (mode from the entry, arena
   options from the shared `[dukematch]` config). Handshake (`syncduke_net.c`) →
   `numplayers = 2` → connect sequence exchanges names (type 6). Both peers already
   hold matching game rules from their launch args — no in-game start packet needed.
3. In play: kills increment `frags[][]`; each door drains its own row →
   `ev_frag` appends `{type:"frag",killer,victim}` to `events.jsonl`. Duke's own
   in-frame frag bar shows the live score.
4. Lobby (any node): recent-activity view + live panel show "Duke fragged Rob"
   interleaved with starts/levels/deaths, bounded by the existing
   `activity_max`/`activity_max_age`.

## Testing

- **Headless (jsexec), no live BBS:** extend `xtrn/syncduke/tests/` (or the events
  test) to assert: `sd_cmd(…, "dm")` contains `/c1` and not `/c2`; `"dmnospawn"` →
  `/c3`; `"coop"`/undefined → `/c2` (back-compat); `[dukematch] monsters=false` adds
  `/m`, `respawn_items=true` adds `/t`; `sd_write_entry` stores `mode`; a mode-less
  entry reads back `"coop"`; `sd_event_text({type:"frag",killer:"A",victim:"B"})`
  === `"A fragged B"`. All runnable via `jsexec -f` against the lib with a stub
  `gl`, like the existing event tests.
- **C build:** compile clean under GCC/Clang first (the tree's portability floor),
  then MSVC; `uncrustify` (`src/uncrustify.cfg`) the touched C files. `syncduke_next_frag`
  is a pure query over engine globals — no new engine linkage beyond `duke3d.h`,
  which `syncduke_game.c` already pulls in.
- **Live 2-player validation (required before commit, per project rule):** two nodes,
  Create → Dukematch → Join; confirm both spawn into a DM (frag bar visible, monsters
  absent by default), trade frags, and that `events.jsonl` gains `frag` lines with
  correct killer/victim aliases and `mode:"dukematch"`, surfacing in the lobby feed.

## Risks / open points

- **Name availability timing.** `ud.user_name[other]` is filled by the type-6 packet
  during connect; a frag can't occur before players are in the level (well after
  connect), so the name is present. The `"player N"` fallback covers any gap.
- **Frag matrix reset between matches.** A door process is one match (BBS doors exit
  per session), so the shadow starts zeroed each launch; an explicit reset on the
  game-start edge guards a warp-to-new-level-without-exit case.
- **Suicide accounting.** `fraggedself` is deliberately left as the existing `death`
  event, not a frag, so the feed reads naturally ("Duke died" vs "X fragged Duke").
- **Divergence note (per house rule):** SyncDOOM keys frag emission off
  `sd_next_frag` over its own kill list; SyncDuke keys off the Duke `frags[][]`
  matrix row. Same contract (emit-own-only, no dedup), different engine source —
  intentional, documented here so the two doors' frag paths don't look accidentally
  different.
```
