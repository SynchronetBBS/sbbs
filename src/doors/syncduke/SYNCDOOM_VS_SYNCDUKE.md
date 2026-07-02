# SyncDOOM vs. SyncDuke — feature comparison

Revised 2026-07-02 (first draft 2026-07-01). Compares the two Synchronet
terminal-game doors that share the `termgfx` render/transport library and the
`exec/load/game_lobby.js` lobby model layer: **SyncDOOM** (the first port —
doomgeneric/Chocolate Doom) and **SyncDuke** (Duke Nukem 3D, vendored Chocolate
Duke3D / Build engine). Focus: deathmatch/multiplayer, the JS **lobby**
(Create/Join, cross-node discovery, node paging, the live who's-online + activity
panel), the door-written **events-log activity feed**, multi-map (WADset / GRP)
support, in-game Synchronet integration (node comms / status), and the text-tier
legibility fallback. Findings are evidence-backed with `file:line` citations; both
doors were surveyed at the current tree — including the `xtrn/<door>/*.js` lobby and
the shared `exec/load/game_lobby.js`, not just the C door.

> **What changed since the first draft:** SyncDuke has since gained (a) in-game
> node-social features — Ctrl-U who's-online, a NODE_EXT presence string while
> playing, and in-game receipt of inter-node messages; (b) a door-written
> `events.jsonl` + lobby activity feed; (c) client charset detection and the UTF-8
> quadrant/sextant text tiers; and (d) — jointly with SyncDOOM — the shared live
> lobby panel and a configurable `[lobby]` activity feed, after SyncDOOM's lobby was
> migrated onto `game_lobby.js`. The matrix and sections below reflect that; the
> remaining SyncDuke gaps are narrower than the first draft's.

## Summary matrix

| Capability | SyncDOOM | SyncDuke |
|---|---|---|
| Multiplayer transport | Full Doom netgame (co-op + deathmatch/altdeath), up to `MAXPLAYERS` | 2-player LAN UDP only (mmulti seam), co-op cadence |
| Deathmatch mode | Yes (`deathmatch`/`altdeath` reported & selectable) | Engine has Dukematch, **but the door never selects it** |
| Dedicated / detached server | Yes — `mp_server.c`, headless, self-terminating | No |
| JS lobby (Create/Join co-op, registry discovery, waiting room) | Yes — up to `MAXPLAYERS`, wadset picker | Yes — 2-player co-op, single GRP |
| Shared lobby model (`exec/load/game_lobby.js`) | Yes — thin door layer over `gl` | Yes — thin door layer over `gl` (both converged) |
| Lobby node paging (invite active nodes to a match) | Yes — `gl.page_targets`/`gl.send_pages` | Yes — `gl.page_targets`/`gl.send_pages` |
| Live who's-online + recent-activity **lobby panel** | Yes — shared `gl.panel_cells`, opt-in `[lobby] live` | **Yes** — shared `gl.panel_cells`, opt-in `[lobby] live` |
| Door activity/events log → lobby feed | Yes — `events.jsonl` (frags/level-clears/deaths) | **Yes** — `events.jsonl` (starts/level-reaches/deaths) |
| Configurable feed (`[lobby] activity_max`/`activity_max_age`/`panel_rows`) | Yes — shared `gl` accessors | Yes — shared `gl` accessors |
| Inter-BBS federation | Designed, **not built** | No |
| Multi-map selection | Engine passthrough (`-iwad/-file/-merge/-deh`); wadset picker is lobby-side | GRP dir configurable; single-GRP install is the clean config; no episode/map/usermap config |
| In-game who's-online (Ctrl-U, C door) | Yes — list + live banner | **Yes** — `syncduke_node.c` (Ctrl-U → banner) |
| In-game presence / who's-online free text | Yes — `NODE_EXT` status | **Yes** — `NODE_EXT` status while playing, cleared on exit |
| In-game inter-node message **receipt** | Yes — `sbbs_recv_nmsg` | **Yes** — `sbbs_recv_nmsg`, shown in the banner |
| In-game **outbound** paging (Ctrl-P page-a-user) | Yes — `sbbs_page_node` | **No** in-game (the *lobby* pages nodes pre-game) |
| DOOR32.SYS session handling | Yes | Yes |
| Per-node log tagging | Yes | Yes (`SBBSNNUM` → per-node debug log) |
| Text tiers | half-block + blocks&shades + quadrant + sextant | **half-block + blocks&shades + quadrant + sextant** |
| Client charset detection (UTF-8 vs CP437) | Yes — `terminal.ini` / `-charset` | **Yes** — `terminal.ini` / `-charset` / `[video] charset` |
| half-block vs blocks&shades appearance | Nearly identical | Can still differ (forces `RT_8BIT` on non-SyncTERM — Gap C) |
| Legible message overlays | Strong — Doom HUD/chat/pages redrawn as real ANSI over any tier | Yes — game quotes/chat redrawn as ANSI in text tiers; menus still block-rendered |

**One-line takeaway:** The two doors are now close to parity on BBS-integration and
presentation. Both are thin door-specific layers over the shared
`exec/load/game_lobby.js` (Create/Join co-op, registry discovery, a waiting room,
node paging, a **live who's-online + recent-activity panel**, and a door-written
**events feed** — all configurable via `[lobby]`). SyncDuke has caught up on the
in-game node-social features (Ctrl-U who's-online, NODE_EXT presence, inter-node
message receipt), the events feed, and the UTF-8 text tiers + charset detection —
all of which the first draft flagged as gaps. What still sets **SyncDOOM** apart is
multiplayer *depth*: matches scale to `MAXPLAYERS` with a dedicated headless server
and selectable deathmatch/altdeath, versus SyncDuke's strictly **2-player LAN co-op**.
Smaller remaining SyncDuke gaps: no in-game *outbound* Ctrl-P paging, no multi-GRP /
map / episode selection config, and a block-tier color-depth divergence (Gap C).

---

## 1. Deathmatch / multiplayer

**SyncDOOM — full netgame, node-coordinated.**
- Engine netgame state is live: `g_game.c:118-121` (`int deathmatch; boolean netgame; playeringame[MAXPLAYERS]`).
- The door pumps client+server net loops every frame: `syncdoom.c` (`NET_CL_Run(); NET_SV_Run();`).
- Modes distinguished and reported (`deathmatch >= 2 ? "altdeath" : "deathmatch"; … netgame ? "co-op" : "single"`).
- **Dedicated headless server** per match in `mp_server.c`: `mp_dedicated_main()`, self-terminates when empty; **detached spawn** survives the launching door (`fork`+`setsid` / `CreateProcess DETACHED_PROCESS`).
- **Cross-node discovery** via a shared file registry: `mp_write_registry()` writes `<gamesdir>/<hostid>-<port>.ini` with host/wadset/mode/addr/port/players/status/heartbeat; entry dropped once the game is in progress so Browse only lists joinable games.
- Door CLI: `-dedicated`, `-spawnserver` (port alloc 20000-20063), `-connect`, `-name`. In-game chat (`T`, whisper-by-color).
- **Scoping caveat:** the browse/create/join **lobby is a Synchronet JS module**, not in the C door; the C side provides the engine + CLI contract. Inter-BBS **federation** (trusted-peer public games) is designed but **not built** (`MULTIPLAYER.md`).

**SyncDuke — 2-player LAN co-op with a real JS lobby; no dedicated server, no deathmatch, no >2 players.**
- **JS lobby** (`xtrn/syncduke/lobby.js` + `syncduke_lib.js`, over `gl`): **Create** hosts a
  match as master — allocates a port (`gl.alloc_port`), writes a registry entry
  (`sd_write_entry` → `gl.write_game`), optionally **pages the door's other active nodes**
  (`sd_prompt_page`/`gl.page_targets`/`gl.send_pages`), enters a waiting room, and clears
  the entry on exit. **Join** lists open registry games (`sd_list_open_games` →
  `gl.list_games`) and dials one as player 1. **Solo** drops into Duke's own menus. So
  BBS-node coordination, a waiting room, and cross-node discovery exist — the same model
  as SyncDOOM (the C door isn't registry-aware; the lobby writes the entry and hands the
  door its role).
- Transport in `syncduke_net.c`: minimal UDP for Duke3D's `mmulti` seam (LAN co-op),
  exactly 2 players, same LAN. Fills `initmultiplayers`/`getpacket`/`sendpacket`;
  master/slave handshake `SD_HELLO`/`SD_WELCOME` magic `'DUK1'`; `numplayers = 2`. Built
  with `dummy_multi.c` replacing stock `mmulti.c` (`CMakeLists.txt`).
- **Lobby-driven config:** the door takes `-netrole master|join` / `-netport` / `-netpeer`
  (`syncduke_net.c:70`, `syncduke_config.c`); env `SYNCDUKE_NET*` is the manual fallback.
  The lobby owns the net config (`gl.read_overlaid`, `syncduke_lib.js`).
- **Deathmatch present in the engine but never selected** (`menues.c` `"DUKEMATCH …"`);
  the lobby + `syncduke_net.c` only set up a 2-player co-op connect list.
- **Genuinely absent (vs SyncDOOM):** dedicated/detached server, deathmatch mode
  selection, >2 players, and inter-BBS federation. (The events log and in-game
  node-social features the first draft listed here are now present — see §3/§3a.)

---

## 2. Multi-map — WADset (Doom) vs. GRP (Duke)

**SyncDOOM — engine passthrough; sysop wadset picker is lobby-side.**
- The C door **forwards** WAD args, no door-side list: `-iwad`, `-file`, `-merge`, `-deh`
  (allowlist in `syncdoom.c`); paths resolved absolute pre-`chdir`, case-insensitive.
  DeHackEd/BEX + NWT `-merge` compiled in.
- Sysop-configurable `[wads]`/`[wadset:*]` sections are for the **JS lobby**
  (`MULTIPLAYER.md`); the C server only reads `-wadset`/`-wadname` to write the registry.
  Auto-scan classification of loose WADs is **not built**.
- In MP, the joiner resolves identical WADs from the registry `wadset` id and verifies
  before connect.

**SyncDuke — GRP dir configurable; no map/episode config.**
- GRP location configurable: `[grp] dir` and `-grpdir` (`syncduke_config.c`), exported as
  absolute `syncduke_grpdir` consumed by `findGRPToUse()`. Ships no game data.
- Engine scans `duke3d*.grp` (`game.c`). The Windows `findGRPToUse` used to
  interactively prompt on multiple matches via a blocking console `getch()` (which would
  hang a socket door) — **fixed**: it now deterministically takes the first match (logs a
  notice when >1 found), mirroring the *nix branch. There is still no door-friendly
  multi-GRP *selection*; isolate a specific GRP via `[grp] dir`.
- Episode/level via the stock Duke "New Game" menu + GRP-version autodetect.
- **Absent:** ini keys for episode/volume/level/map; user-map (`.map`) support;
  per-session map selection; multiple configured GRP sets.

**Comparison:** Both push map/mod handling down to their engines and neither has a
*door-native* selection UI in C. SyncDOOM's answer is a designed lobby+ini wadset picker
(partly implemented, in JS); SyncDuke has no equivalent — a single-GRP install is the
clean configuration today.

---

## 3. Synchronet integration — in-game node comms / status

Both parse **DOOR32.SYS** (comm type, socket handle, user, time-left) and handle client
disconnect so the BBS reclaims the node. Above the session, SyncDuke has closed most of
the first draft's gap — both doors now call the shared `termgfx/sbbs_node.c` toolkit
in-game.

**SyncDOOM — first-class node-social features (in C via `termgfx/sbbs_node.c`).**
- Door-native `node.dab` access without SCFG: `sbbs_node_init(home)`, gated by
  `sbbs_node_available()`.
- **Who's-online (Ctrl-U):** full-screen `draw_userlist`/`sbbs_list_nodes` + a
  non-blocking in-game banner.
- **Outbound paging (Ctrl-P):** `sbbs_page_node(...)` → BEL-prefixed
  `<data>/msgs/n###.msg` + `NODE_NMSG`.
- **Page receipt:** `sbbs_recv_nmsg()`, strips Ctrl-A codes.
- **Presence / who's-online free text:** `sbbs_node_set_ext("in the SyncDOOM waiting
  room")`, live game status with wad-set name, cleared at exit.
- Hint row `"Ctrl-U who's online   Ctrl-P page a user"` shown only in BBS context.

**SyncDuke — now wires up the in-game toolkit too (`syncduke_node.c`, `syncduke_input.c`).**
- **Who's-online (Ctrl-U):** `syncduke_input.c:702` maps `0x15` →
  `syncduke_node_userlist_request()`; the next `syncduke_node_tick()` builds a
  cross-tier who's-online **banner** (`syncduke_node_draw`, drawn from
  `syncduke_io.c:1104` via `syncduke_node_overlay_sig()` de-dupe). It calls
  `sbbs_list_nodes`/`sbbs_username`/`sbbs_action_str` (`syncduke_node.c:121-137`).
- **Presence (NODE_EXT while playing):** `syncduke_node.c:101-103` broadcasts
  `syncduke_game_status()` (`syncduke_game.c:42`, e.g. "playing SyncDuke (Nukem 3D)
  (E1L2)") via `sbbs_node_set_ext`, only when it changes; cleared on exit (`:78`).
- **In-game message receipt:** `sbbs_recv_nmsg` (`syncduke_node.c:156`) polls for
  inter-node messages and surfaces them in the banner overlay.
- The C door also uses `sbbs_my_node()` to tag the per-node debug log
  (`syncduke_config.c`) so concurrent co-op nodes don't collide, plus DOOR32.SYS parse
  and hangup-on-disconnect (`syncduke_door.c`).
- **Remaining in-game gap:** no **outbound** Ctrl-P page-a-user — SyncDuke shows incoming
  messages but has no `sbbs_page_node` call to *send* one from inside the game. (The lobby
  covers the pre-game paging case via `gl.send_pages`.) This is now the narrow node-social
  gap; the first draft's larger "no who's-online / no presence" gap is closed.

### 3a. Door events log → lobby activity feed (both doors)

Both doors write an append-only `events.jsonl` when the lobby passes `-eventlog`, and
both lobbies render a recent-activity feed from it via the **shared** `game_lobby.js`
events layer (`gl.recent_events` / `gl.event_feed` / `gl.prune_events`).

- **SyncDOOM:** the door appends frags / level-clears / deaths (`syncdoom.c`); the lobby
  launches with `-eventlog "<data>/syncdoom/events.jsonl"` and renders the feed
  (`syncdoom_lib.js:270-305` — `sd_event_text`/`sd_recent_events`/`sd_panel_cells`, thin
  wrappers over `gl`), shown in the full-screen `A` view and the bottom panel.
- **SyncDuke:** `syncduke_events.c` appends **start / level (reached) / death** events
  (`ev_emit`, `syncduke_events_tick`); the lobby launches with `-eventlog` (`sd_cmd`,
  `syncduke_lib.js:117`), reads `SD_EVENTS` (`:35`), formats via `sd_event_text`
  (plain body — the display path adds the age), and shows them in the `L`og view
  (`lobby.js:174-177`) and the bottom panel. Bounded on entry by
  `gl.prune_events(SD_EVENTS, 2000, 1000)` (`lobby.js:270`).
- Event *taxonomy* is door-specific and stays door-side (Doom filters out its
  start/end/player-death records; Duke surfaces all of start/level/death). The raw read,
  filtering-by-displayability, formatting, oversample-fill, and pruning are shared in
  `gl`.

### 3b. Shared lobby infrastructure (`exec/load/game_lobby.js`)

Both lobbies are now thin door-specific layers over one shared, UI-free model library
(SyncDOOM's lobby was migrated onto it; SyncDuke was built on it). `gl` owns:
- `[net]` config with per-host `[net:<hostname>]` overlay + defaults (`read_overlaid`,
  `apply_defaults`); bind/advertise/creator-connect address resolution; UDP port
  allocation (`alloc_port`).
- The file-based game registry (`list_games`/`write_game`/`remove_game`, heartbeat-or-mtime
  staleness + reaping).
- Node paging (`door_ars`/`page_targets`/`send_pages`).
- The **live who's-online + recent-activity panel** composer (`live_nodes`/`node_cell`/
  `blank_cell`/`activity_cell`/`panel_cells`) — UI-free cell strings; each door's lobby
  bottom-anchors them and repaints ~1/s. Players of the door's marker sort first / render
  green.
- The **events layer** (`read_events`/`recent_events`/`event_feed`/`prune_events`) and
  small string/time helpers (`trim`/`plain`/`clip`/`rpad`/`lpad`/`mmss`/`ago`).
- **Feed configuration accessors:** `activity_max(cfg)` (feed count, default 18),
  `activity_max_age(cfg)` (hide activity older than this), `panel_rows(cfg)` (panel
  height, default 6), plus `parse_duration(str)` — because JS `File.iniGetValue` has no
  native duration parsing, `parse_duration` reads a bare number as **days** (fractions
  OK, e.g. `0.5`) or an `h`/`d`/`w` suffix.
- The `[lobby]` keys are shared by both doors and documented in each
  `xtrn/<door>/<door>.example.ini`: `live` (enable the panel), `activity_max`,
  `activity_max_age`, `panel_rows`. The examples ship `live = true`.

---

## 4. Text-tier support (legible output on non-graphics terminals)

Both auto-select a block-glyph text tier when a terminal advertises neither sixel nor JXL
(e.g. conhost), so nobody gets a blank screen, and both now detect the client charset and
offer the higher-resolution Unicode tiers on UTF-8 terminals.

**SyncDOOM.**
- Four tiers JXL → Sixel → Text with auto-select; text is the compatibility floor.
- `termgfx/text.c` renders RGB→ANSI blocks; modes `RT_HALF/SPACE/QUADRANT/SEXTANT/BLOCKS`,
  depths 24→3-bit, `RT_UTF8`/`RT_CP437`. `RT_BLOCKS` uses only CP437 block/shade glyphs
  present in legacy/conhost fonts.
- `read_terminal_ini()` sets the charset from `<node>/terminal.ini`, passes it into
  `rt_config`, and conditionally builds quadrant + sextant states on `RT_UTF8`.
- **Legible message overlays:** in text mode the door suppresses Doom's rasterized
  message/chat and **redraws them as real terminal characters** over the block render,
  using cell-diff exclusion rects. Door notices, page banners, who's-online banner all
  reuse this path.

**SyncDuke.**
- Tiers `SD_SIXEL/JXL/SIXEL_FULL/HALF/BLOCKS/QUADRANT/SEXTANT` (`syncduke_io.c:368`),
  `sd_is_text_tier(t) = t >= SD_HALF`. Auto-select falls to `SD_HALF` when the probe shows
  no sixel/JXL; 500 ms grace so conhost never sees a sixel frame. Block render via
  `rt_render_frame`.
- **Client charset detection + Unicode tiers (was Gap B — RESOLVED, `525034f712`).**
  `syncduke_config.c:103-229` reads the charset with precedence `-charset` arg →
  `[video] charset` → `<node>/terminal.ini` `chars` (mirrors SyncDOOM's
  `read_terminal_ini`), exposed as `syncduke_term_is_utf8()`. `rt_config` now threads the
  real charset (`syncduke_io.c:452-453`: `… syncduke_term_is_utf8() ? RT_UTF8 : RT_CP437`),
  and `SD_QUADRANT`/`SD_SEXTANT` are added to the F4 `avail[]` list when the client is
  UTF-8 (`:728-731`). So a UTF-8 client of SyncDuke now gets the quadrant/sextant tiers,
  same as SyncDOOM.
- **Legible message overlays (was Gap A — RESOLVED, `6aeadc2978`).** In a text tier
  `operatefta()` captures the game's on-screen **quotes / multiplayer chat** (instead of
  rasterizing the block font) and the door redraws them as real ANSI characters,
  cell-diff-excluded, over the block frame; the illegible exit "order" splashes are
  skipped. Door overlays (loading splash, tier/depth label, live stats strip) were already
  ANSI. Remaining vs SyncDOOM: Duke's **menus** (New Game, options) still block-render.

### 4a. Gap C — half-block vs blocks&shades appearance (SyncDuke only, partially open)

Both doors call the same `termgfx rt_render_frame`, so identical
`(cols,rows,mode,depth,charset)` + identical RGB produce identical output. Any divergence
comes from how each door configures/feeds the renderer. With Gap B resolved (charset now
matches), the remaining suspect is **color depth**:
- SyncDuke forces `RT_8BIT` on any non-SyncTERM client (`syncduke_io.c:452`), while
  SyncDOOM derives 24-bit vs 8-bit from `terminal.ini`. Coarser quantization pushes the
  blocks&shades tier toward shade-glyph dithering (`░▒▓`), pulling it away from the clean
  2-subpixel half-block.
- A secondary suspect is the framebuffer downsample: SyncDuke pre-packs a nearest-neighbour
  RGB downsample of the 320×200 fb to the cell grid (`syncduke_pack_rgb`) before the block
  renderer runs.

*(Gap C is reported-and-plausible, not yet bisected. The cheap first step is to stop
forcing `RT_8BIT` on non-SyncTERM and derive depth from the detected terminal, as SyncDOOM
does.)*

---

## Where SyncDuke still trails, and the cheapest paths to parity

1. **In-game outbound paging (Ctrl-P).** SyncDuke shows *incoming* inter-node messages
   in-game and pages nodes from the *lobby*, but has no in-game `sbbs_page_node` call to
   *send* a page to a chosen node while playing. `termgfx/sbbs_node.c` already provides it
   (SyncDOOM shows the wiring); a Ctrl-P handler + target prompt would close it.
2. **Block-tier color-depth consistency (Gap C).** Stop forcing `RT_8BIT` on non-SyncTERM;
   derive depth from the detected terminal like SyncDOOM, and verify the two block tiers
   then match.
3. **Multi-GRP / map selection.** The blocking `getch()` GRP prompt is fixed (first match
   wins). Remaining: an `[grp]`/`[game]` ini key to *name* a specific GRP when several are
   present, plus episode/skill config keys; user-map (`.map`) support is a further step.
4. **Multiplayer maturity.** SyncDuke has the lobby/registry, cross-node discovery, waiting
   room, node paging, live panel, and events feed. To match SyncDOOM's *depth* it would
   need a dedicated/detached server, deathmatch (Dukematch) selection, and >2-player
   support — its co-op is 2-player LAN today.
5. **Doc hygiene.** Re-check SyncDuke's `README.md`/`DESIGN.md`/`PLAN.md` for lingering
   "single-player, no multiplayer" language that predates the net + lobby layers.

**Already closed since the first draft:** in-game who's-online (Ctrl-U) + banner, NODE_EXT
presence while playing, in-game message receipt, the door events log + lobby activity feed,
client charset detection + UTF-8 quadrant/sextant tiers, and (jointly) the shared live
lobby panel + configurable `[lobby]` feed.

## Caveats / provenance

- The config **templates** `xtrn/<door>/{syncduke,syncdoom}.example.ini` ARE committed
  (tracked in git); the live per-BBS `syncduke.ini`/`syncdoom.ini` are per-install
  (untracked, seeded from the example by `install-xtrn`). Config keys cited come from those
  templates and the parsing code.
- **Both doors ship a JavaScript lobby** (`xtrn/<door>/lobby.js` + `*_lib.js`) that are now
  thin layers over the shared `exec/load/game_lobby.js`; much node coordination, the live
  panel, and the events feed live there, not in the C door. SyncDOOM's wadset picker and
  federation are variously JS or **designed-not-built**; this report distinguishes
  engine/door C code from lobby-JS from unbuilt design where the sources make it explicit.
- `termgfx` (`sbbs_node.c`, the encoders, the text tiers) is shared C; `game_lobby.js` is
  the shared JS model layer. Both doors now call into both, so most "library provides /
  door doesn't call" gaps from the first draft are closed.
