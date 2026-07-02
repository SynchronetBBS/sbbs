# SyncDuke — Synchronet node status / listing integration (design)

**Date:** 2026-07-01
**Component:** `src/doors/syncduke`
**Reference implementation:** SyncDOOM (`src/doors/syncdoom/syncdoom.c`) via `src/doors/termgfx/sbbs_node.{c,h}`

## Goal

Bring SyncDOOM's in-game Synchronet node integration to SyncDuke: an in-game
**who's-online listing (Ctrl-U)**, a **node status broadcast** (so other users see
"playing SyncDuke …" in *their* who's-online), and a **neutral incoming
inter-node-message display**. Harmonizes SyncDuke with SyncDOOM.

## Scope

**In scope**
- **Ctrl-U** — non-blocking, multi-line who's-online banner over the top rows,
  auto-clearing (~9 s). No screen takeover; frame pacing untouched.
- **Node status broadcast** — `NODE_EXT` free-text ("playing SyncDuke", plus the
  current level when readable), written only on change; the flag is cleared on exit.
- **Incoming node-message receive** — poll `NODE_NMSG` ~1 Hz; display the message
  **verbatim** (word-wrapped) + a bell.

**Out of scope (deferred)**
- **Ctrl-P paging (sending)** — not fully worked out in SyncDOOM either; deferred
  along with its text-entry prompt and the waiting-room paging UI.

## Key decisions

- **`termgfx/sbbs_node.{c,h}` is reused as-is** — already linked into SyncDuke; no
  new dependency. Only `sbbs_my_node()` is used today; this adds the listing / status
  / receive calls.
- **Incoming messages are shown verbatim, with no "paging" label.** `NODE_NMSG` is a
  general inter-node message (a user's page, but also logoff/sysop notices), so the
  message text carries its own meaning. This matches SyncDOOM's *in-game* receive
  (its "page"/"paging" wording lives only in the deferred send-side UI).
- **Exit clears only the `NODE_EXT` flag**, not the `node.exb` record. Every reader
  (termgfx `sbbs_node.c`, and the BBS's `getnode.cpp`/`presence_lib.js`) shows the
  free-text only when the `NODE_EXT` flag is set; with the flag clear the stale
  `node.exb` bytes are ignored and overwritten on the next `set_ext`. (One reader to
  confirm before coding: `js_system.cpp` populates `node.vstatus` gated on
  `NODE_EXT` — expected, consistent with the other readers.)
- **The door owns the exit-clear** (via `atexit`, as SyncDOOM does), rather than a
  new `sbbs3` core change. `NODE_EXT` is transient door state; Synchronet otherwise
  only clears it at session end (`main.cpp:4705-4714`), not on return-to-menu, so a
  door that sets it must clear it. (A crash-safe alternative — clearing `NODE_EXT`
  in `external()` — was considered and set aside to keep this change door-local.)

## Architecture

New module **`syncduke_node.c`** (+ declarations in `syncduke.h`), matching
SyncDuke's file-per-concern layout (io/input/game/config/door/net/plat/stubs/log).
It owns all node state and BBS interaction; the sole shared touch-point is the
banner overlay, which `syncduke_io.c present()` paints.

Boundary: `syncduke_node.c` decides *what* to show and *when it's dirty*;
`syncduke_io.c present()` decides *when to repaint* and calls the draw.

## Components

1. **Init / teardown** — `syncduke_node_init(home)` calls `sbbs_node_init()` once at
   startup with the door's `-home`; everything gates on `sbbs_node_available()`.
   Registers an `atexit` that clears the `NODE_EXT` flag.
2. **Ctrl-U (0x15)** — added to `handle_key` beside Ctrl-S/T/O, but only sets a
   request flag (no BBS I/O in the input path). Swallowed — never reaches Duke.
3. **Who's-online build** — `sbbs_list_nodes(…, selfnode)` → banner rows: header
   `Who's online (N):`, one `alias  activity` row per node (`sbbs_node_ext` free-text
   if present, else `sbbs_action_str`), an `…and M more` overflow row, and a
   `No one else is online.` empty case. ~9 s expiry.
4. **Node status broadcast** — `sbbs_node_set_ext("playing SyncDuke" [+" (E#L#)"])`,
   written only when the text changes. Level appended when the engine state is
   readable via `syncduke_game.c`; otherwise just "playing SyncDuke". Cleared on exit.
5. **Incoming node-message receive** — `sbbs_recv_nmsg(my_node)` polled ~1 Hz; strip
   Ctrl-A/BEL, fold whitespace, word-wrap into the banner **verbatim** + bell.

## Banner rendering

- **State (in `syncduke_node.c`)**: `rows[]` + `until_ms` expiry + a monotonic
  **overlay signature** (like `sd_hud_signature`) that changes on any content or
  visibility change.
- **Draw**: after `present()` emits the game frame it calls
  `syncduke_node_draw(cols, rows)`, which emits absolute-positioned ANSI
  (`ESC[r;cH` + white-on-red SGR + text) for the top rows when the banner is live —
  the same "text cells over the frame" approach SyncDuke already uses for the stats
  strip / depth label / text-tier HUD, so it works over sixel, JXL, and text tiers.
- **Appear/expire repaint**: `present()` compares `syncduke_node_overlay_sig()` to the
  previous frame's; on change it forces a full repaint (`syncduke_have_last = 0` /
  `rt_invalidate`) so an appearing banner isn't swallowed by the identical-frame
  de-dupe, and on expiry the underlying image repaints and erases the strip — exactly
  how the existing HUD-line signature already defeats the de-dupe.

## Data flow

- **Input path**: Ctrl-U → set request flag only.
- **Main loop** (`syncduke_plat.c`, alongside `present()`): `syncduke_node_tick()`
  runs once per frame and (a) builds the who's-online banner if Ctrl-U was requested,
  (b) polls `sbbs_recv_nmsg` at ~1 Hz → banner + bell on a message, (c) refreshes
  node status on change. All BBS file I/O stays on the main loop, never the input path.
- `present()` then draws the banner and handles the repaint, per above.

## Error handling / non-Synchronet installs

All features gate on `sbbs_node_available()` (set when `sbbs_node_init()` finds
`node.dab` via `$SBBSCTRL`). On a non-Synchronet / DOOR32.SYS host with no BBS
context: Ctrl-U is a silent no-op; status broadcast and message-poll don't run. No
errors. Matches SyncDOOM.

## Testing

- **Unit** (`tests/`): extend the input tests to assert Ctrl-U (0x15) sets the
  request flag and is swallowed (never reaches Duke). If the who's-online
  build/word-wrap is factored to take an injected node list, unit-test it against a
  synthetic list; otherwise it's covered by the runtime test.
- **Runtime** (live BBS, `node.dab` present): Ctrl-U shows who's online; another
  node's who's-online shows "playing SyncDuke …"; a message sent to the SyncDuke node
  shows as a banner + bell; on door exit the status reverts (flag cleared) — verified
  from a second node's who's-online.
- **Build**: both doors build clean; SyncDuke input unit tests (kitty/evdev) still
  pass; uncrustify clean on new/edited files.

## Files touched

- `src/doors/syncduke/syncduke_node.c` (new) — node state, listing, status, receive,
  banner state + draw + signature.
- `src/doors/syncduke/syncduke.h` — declarations.
- `src/doors/syncduke/syncduke_input.c` — Ctrl-U (0x15) → request flag; swallow.
- `src/doors/syncduke/syncduke_io.c` — `present()` calls the banner draw + repaint on
  signature change.
- `src/doors/syncduke/syncduke_plat.c` — call `syncduke_node_tick()` per frame; init at startup.
- `src/doors/syncduke/CMakeLists.txt` — add `syncduke_node.c`.
- `src/doors/syncduke/tests/` — Ctrl-U input test.
