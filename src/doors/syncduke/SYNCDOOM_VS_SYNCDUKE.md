# SyncDOOM vs. SyncDuke — feature comparison

Draft 2026-07-01. Compares the two Synchronet terminal-game doors that share
the `termgfx` render/transport library: **SyncDOOM** (the first port —
doomgeneric/Chocolate Doom) and **SyncDuke** (Duke Nukem 3D, vendored
Chocolate Duke3D / Build engine). Focus: deathmatch/multiplayer, the JS **lobby**
(Create/Join, cross-node discovery, node paging) and its **events-log activity
feed**, multi-map (WADset / GRP) support, Synchronet integration (node comms /
status), and the text-tier legibility fallback. Findings are evidence-backed with
`file:line` citations; both doors were surveyed at the current tree — including the
`xtrn/<door>/*.js` lobby, not just the C door.

## Summary matrix

| Capability | SyncDOOM | SyncDuke |
|---|---|---|
| Multiplayer transport | Full Doom netgame (co-op + deathmatch/altdeath), up to `MAXPLAYERS` | 2-player LAN UDP only (mmulti seam), co-op cadence |
| Deathmatch mode | Yes (`deathmatch`/`altdeath` reported & selectable) | Engine has Dukematch, **but the door never selects it** |
| Dedicated / detached server | Yes — `mp_server.c`, headless, self-terminating | No |
| JS lobby (Create/Join co-op, registry discovery, waiting room) | **Yes** — up to `MAXPLAYERS`, wadset picker | **Yes** — 2-player co-op, single GRP (`lobby.js`) |
| Lobby node paging (invite active nodes to a match) | Yes — shared `exec/load/game_lobby.js` | Yes — shared `exec/load/game_lobby.js` |
| Door activity/events log → lobby feed | **Yes** — door writes `events.jsonl` (frags/level-clears/deaths); lobby renders a recent-activity view | **No** — neither door nor lobby |
| Inter-BBS federation | Designed, **not built** | No |
| Multi-map selection | Engine passthrough (`-iwad/-file/-merge/-deh`); wadset picker is lobby-side | GRP dir configurable; multi-GRP scan exists but selection is a blocking `getch()`; no episode/map/usermap config |
| In-game who's-online (Ctrl-U, C door) | Yes — list + live banner | **No** (library supports it; door doesn't call it) |
| In-game paging (Ctrl-P) + page receipt (C door) | Yes — `sbbs_page_node`/`sbbs_recv_nmsg` | **No** in-game (the *lobby* pages nodes — see above) |
| In-game presence / who's-online free text | Yes — `NODE_EXT` status ("in the SyncDOOM waiting room", live game status) | **No** |
| DOOR32.SYS session handling | Yes | Yes |
| Per-node log tagging | Yes | Yes (`SBBSNNUM` → per-node debug log) |
| Text tier (block glyphs) | half-block + blocks&shades + quadrant + sextant | half-block + blocks&shades **only** (quadrant/sextant defined but unreachable) |
| Client charset detection (UTF-8 vs CP437) | Yes — `terminal.ini` / `-charset`; offers Unicode tiers on UTF-8 | **No** — hardcoded CP437; never uses a UTF-8/Unicode tier even on a UTF-8 terminal |
| half-block vs blocks&shades appearance | Nearly identical | Visibly different (blocks tier is coarser/shade-dithered) |
| Legible message overlays | **Strong** — Doom HUD/chat/pages redrawn as *real* ANSI chars over any tier | **Yes** — game quotes/chat redrawn as ANSI in text tiers (`6aeadc2978`); door overlays already ANSI; game *menus* still block-rendered |

**One-line takeaway:** Both doors are BBS-native: each ships a Synchronet JS
**lobby** (Create/Join co-op, registry-based cross-node discovery, a waiting room,
and node paging) built on the shared `exec/load/game_lobby.js`. SyncDOOM is the
more *mature* one — its matches scale to `MAXPLAYERS` with a dedicated server and
deathmatch modes, it has a wadset picker, a door-written **events log** driving a
lobby activity feed, and in-game who's-online/paging. SyncDuke's lobby is real but
its co-op is strictly **2-player LAN**, it has no events feed, and the C door
doesn't wire up the in-game who's-online/paging (which the shared `termgfx` library
already provides). The two are now at parity on the **legible text-tier message
overlay** (SyncDuke gained it in `6aeadc2978`) — game quotes/chat render as real
ANSI on block terminals in both.

---

## 1. Deathmatch / multiplayer

**SyncDOOM — full netgame, node-coordinated.**
- Engine netgame state is live: `g_game.c:118-121` (`int deathmatch; boolean netgame; playeringame[MAXPLAYERS]`).
- The door pumps client+server net loops every frame: `syncdoom.c:3371-3372, 3861-3864` (`NET_CL_Run(); NET_SV_Run();`).
- Modes distinguished and reported: `syncdoom.c:3601-3603` — `deathmatch >= 2 ? "altdeath" : "deathmatch"; … netgame ? "co-op" : "single"`.
- **Dedicated headless server** per match in `mp_server.c`: `mp_dedicated_main()` (:112), self-terminates when empty (:168-198); **detached spawn** survives the launching door (`fork`+`setsid` / `CreateProcess DETACHED_PROCESS`, :255-337).
- **Cross-node discovery** via a shared file registry: `mp_write_registry()` (:86-110) writes `<gamesdir>/<hostid>-<port>.ini` with host/wadset/mode/addr/port/players/status/heartbeat; entry dropped once the game is in progress so Browse only lists joinable games.
- Door CLI: `-dedicated`, `-spawnserver` (port alloc 20000-20063), `-connect`, `-name` (`syncdoom.c:3964-3999`). In-game chat (`T`, whisper-by-color).
- **Scoping caveat:** the browse/create/join **lobby is a separate Synchronet JS module**, not in the C door; the C side provides only the engine + CLI contract. Inter-BBS **federation** (trusted-peer public games) is designed but **not built** (`MULTIPLAYER.md:14-16, 228-235`).

**SyncDuke — 2-player LAN co-op *with* a real JS lobby; no dedicated server, no deathmatch, no >2 players.**
- **JS lobby exists** (`xtrn/syncduke/lobby.js` + `syncduke_lib.js`, on the shared
  `exec/load/game_lobby.js` as `gl`): **Create** hosts a match as master — allocates a
  port (`gl.alloc_port`), writes a registry entry (`sd_write_entry` → `gl.write_game`,
  `syncduke_lib.js:135`), optionally **pages the door's other active nodes** to invite
  them (`sd_prompt_page`/`gl.page_targets`/`gl.send_pages`, `lobby.js:56-76`), enters a
  waiting room, and clears the entry on exit. **Join** lists open registry games
  (`sd_list_open_games` → `gl.list_games`, `:161`) and dials one as player 1. **Solo**
  drops into Duke's own menus. So BBS-node coordination, a waiting room, and cross-node
  discovery **do exist** — via the lobby, the same model as SyncDOOM (the C door isn't
  registry-aware; the lobby writes the entry and hands the door its role — `syncduke_lib.js:9`).
- Transport in `syncduke_net.c`: *"minimal UDP transport for Duke3D's mmulti seam (LAN
  co-op) … exactly 2 players, same LAN"* (`:1-17`). Fills `initmultiplayers`/`getpacket`/
  `sendpacket` (`:198,218,244`); master/slave handshake `SD_HELLO`/`SD_WELCOME` magic
  `'DUK1'`; sets `numplayers=2` (`:105-114`). Built with `dummy_multi.c` replacing stock
  `mmulti.c` (`CMakeLists.txt:39`).
- **Lobby-driven config, not an ini section:** the door takes `-netrole master|join`/
  `-netport`/`-netpeer` — explicitly *"the lobby's path"* (`syncduke_net.c:67,74`,
  `syncduke_config.c:65,134-139`); env `SYNCDUKE_NET*` is the manual fallback. There's no
  `[net]`/`[coop]` ini section because the lobby owns the net config (`gl.read_overlaid`,
  `syncduke_lib.js:57-60`).
- **Deathmatch present in the engine but never selected:** `menues.c:4142-4143`
  (`"DUKEMATCH …"`), but the lobby and `syncduke_net.c` only set up a 2-player *co-op*
  connect list — nothing picks Dukematch.
- **Stale in-repo docs:** `README.md:19,189`, `DESIGN.md:24`, `PLAN.md:19` still say
  "single-player, no multiplayer" — predating both the net (`39aeecbc95`) and lobby layers.
- **Genuinely absent (vs SyncDOOM):** dedicated/detached server, deathmatch mode
  selection, >2 players, inter-BBS federation, and the door-written **events log / lobby
  activity feed** (see §3a).

---

## 2. Multi-map — WADset (Doom) vs. GRP (Duke)

**SyncDOOM — engine passthrough now; sysop wadset picker is lobby-side.**
- The C door **forwards** WAD args, no door-side list: `MULTIPLAYER.md:454-458` — *"the door just forwards those args … there is no door-side WAD list, directory, or config yet — the lobby introduces all of that."*
- Passthrough is real: `-iwad`, `-file`, `-merge`, `-deh` (`README.md:142-155`, allowlist `syncdoom.c:4229`); paths resolved absolute pre-`chdir`, case-insensitive. DeHackEd/BEX + NWT `-merge` compiled in.
- Sysop-configurable `[wads]`/`[wadset:*]` sections (iwad/pwad/name/modes/map/skill/maxplayers/merge…) are documented for the **JS lobby** (`MULTIPLAYER.md:481-554`); **no `[wadset` parsing in the C tree** — the C server only reads `-wadset`/`-wadname` to write the registry (`mp_server.c:144`). Auto-scan classification of loose WADs is **not built** (`MULTIPLAYER.md:556-560`).
- In MP, joiner resolves identical WADs from the registry `wadset` id and verifies before connect.

**SyncDuke — GRP dir configurable; multi-GRP scan is engine-native but door-unfriendly; no map/episode config.**
- GRP location configurable: `[grp] dir` (`syncduke_config.c:145`) and `-grpdir` (`:128-129`), exported as absolute `syncduke_grpdir` consumed by `findGRPToUse()`. Ships no game data (`README.md:156-163`).
- Engine scans `duke3d*.grp` (`game.c:8051`). Windows `findGRPToUse` used to
  **interactively prompt** on multiple matches via a **blocking console
  `getch()`** — which would hang a socket door (no host console the remote user
  can answer) and also fed a `char*` into an `int grpID` used as an OOB index.
  **Fixed:** the Windows branch now deterministically takes the first match
  (`grpID = 0`, logs a notice when >1 found), mirroring the *nix branch
  (`game.c:8170-8177`). There is still no door-friendly multi-GRP *selection* —
  isolate a specific GRP via the `[grp] dir` setting.
- Episode/level via the stock Duke "New Game" menu + GRP-version autodetect (`game.c:8253-8273`, probes `DUKESW.BIN`/`E4L11.MAP`…).
- **Absent:** ini keys for episode/volume/level/map; user-map (`.map`) support; per-session map selection; multiple configured GRP sets. `-map` is only anticipated (`syncduke_door.c:216`), not implemented.

**Comparison:** Both push actual map/mod handling down to their engines and neither has a *door-native* selection UI in C. SyncDOOM's answer is a designed lobby+ini wadset picker (partly implemented, in JS); SyncDuke has no equivalent and, worse, its multi-GRP path hits a blocking `getch()` that doesn't belong in a door — a single-GRP install is the only clean configuration today.

---

## 3. Synchronet integration — node comms / status listing

Both parse **DOOR32.SYS** (comm type, socket handle, user, time-left) and handle
client disconnect so the BBS reclaims the node. The divergence is everything
*above* the session:

**SyncDOOM — first-class node-social features (all in C via `termgfx/sbbs_node.c`).**
- Door-native `node.dab` access without SCFG: `sbbs_node_init(home)` (`syncdoom.c:4180`), gated by `sbbs_node_available()`.
- **Who's-online (Ctrl-U):** full-screen `draw_userlist`/`sbbs_list_nodes` (`syncdoom.c:3413-3440`) + non-blocking in-game banner (`:3731-3767`).
- **Paging (Ctrl-P):** `sbbs_page_node(...)` (`:3467`) → BEL-prefixed `<data>/msgs/n###.msg` + `NODE_NMSG`.
- **Page receipt:** `sbbs_recv_nmsg()` (`:3493,3791`), strips Ctrl-A codes.
- **Presence / who's-online free text:** `sbbs_node_set_ext("in the SyncDOOM waiting room")` (`:3855`), live game status with wad-set name (`:3508-3550`), cleared at exit (`:3729`).
- Hint row `"Ctrl-U who's online   Ctrl-P page a user"` shown only in BBS context (`:3301`).

**SyncDuke — node coordination lives in the *lobby*; the in-game C node-social API is unused.**
- **Lobby-level node coordination exists** (§1): the lobby lists active nodes and **pages
  them to join a co-op match** (`gl.page_targets`/`gl.send_pages`, `lobby.js:56-76`). So
  SyncDuke is not node-blind — it just does its node-social work in JS, at the lobby, not
  in the C door during play.
- The C door uses `sbbs_my_node()` **only** to tag the per-node debug log
  (`syncduke_config.c:189,205-217`) so concurrent co-op nodes don't collide; plus
  DOOR32.SYS parse (`syncduke_door.c:134-161`) and hangup-on-disconnect (`:46-52`).
- `termgfx/sbbs_node.c` provides the **entire** in-game toolkit (`sbbs_list_nodes`,
  `sbbs_page_node`, `sbbs_recv_nmsg`, `sbbs_node_set_ext`, `sbbs_username`,
  `sbbs_action_str`), but a tree-wide grep finds callers **only in `syncdoom.c`** — **no
  SyncDuke source calls them**.
- **Absent in SyncDuke (in-game):** who's-online (Ctrl-U), paging (Ctrl-P) + page receipt,
  and `NODE_EXT` presence *while playing*. This is the largest in-game parity gap and the
  cheapest to close, since the library work is done — note the lobby already covers the
  *pre-game* paging case.

### 3a. Door events log → lobby activity feed (SyncDOOM only)

SyncDOOM has a door→lobby **activity feed** that SyncDuke lacks entirely:

- The door **writes an append-only `events.jsonl`** when the lobby passes `-eventlog`:
  `syncdoom.c:3553-3590` (`g_eventlog` path; each event `fopen(...,"ab")` appended). The
  lobby launches with `-eventlog "<data>/syncdoom/events.jsonl"` (`lobby.js:40`).
- The lobby **renders a recent-activity view** from it — frags, level clears, and deaths:
  `syncdoom_lib.js:332-518` (`sd_recent_events`/`sd_event_feed`/`sd_event_text`/
  `sd_activity_cell`), shown in `lobby.js:592-597`, and **bounded** by `sd_prune_events(2000,1000)`
  on entry (`lobby.js:691`). `SD_EVENTS = <data>/syncdoom/events.jsonl` (`syncdoom_lib.js:20`).
- **SyncDuke:** no `-eventlog` in the door and no event references in `lobby.js`/
  `syncduke_lib.js` — no events file, no activity feed. A natural, low-cost lobby
  enhancement to mirror.

---

## 4. Text-tier support (legible messages on non-graphics terminals)

Both auto-select a block-glyph text tier when a terminal advertises neither
sixel nor JXL (e.g. conhost), so nobody gets a blank screen.

**SyncDOOM.**
- Four tiers JXL → Sixel → Text with auto-select (`syncdoom.c:229` enum; ladder `:2347-2370`). Text is the compatibility floor (`README.md:22-33`).
- `termgfx/text.c` renders RGB→ANSI blocks; modes `RT_HALF/SPACE/QUADRANT/SEXTANT/BLOCKS`, depths 24→3-bit, `RT_UTF8`/`RT_CP437` (`text.h:23-25`). `RT_BLOCKS` uses only CP437 block/shade glyphs that exist in legacy/conhost fonts (`text.h:17-22`).
- **Legible message overlays are a real, distinct feature:** in text mode the door suppresses Doom's rasterized message/chat and **redraws them as real terminal characters** over the block render, using cell-diff exclusion rects so the game never repaints over them (`syncdoom.c:854, 902-939`; API `text.h:36-45`). Door notices, page banners, who's-online banner all reuse this legible-character path.

**SyncDuke.**
- Tiers `SD_SIXEL/JXL/SIXEL_FULL/HALF/BLOCKS/QUADRANT/SEXTANT` (`syncduke_io.c:367`), `sd_is_text_tier(t)=t>=SD_HALF` (`:369`). Auto-select falls to `SD_HALF` when the probe shows no sixel/JXL (`:574-585`); 500 ms grace so conhost never sees a sixel frame (`:559-561`). Block render via `rt_render_frame` (`:429-434, 1027-1035`). Documented `README.md:34-38`.
- **Door overlays are legible ANSI text:** loading splash (`syncduke_door.c:71-73`), centered tier/depth label `syncduke_show_label()` (`syncduke_io.c:593-618`), live stats strip `syncduke_emit_overlay()` (`:795`).
- **Gap A — RESOLVED (`6aeadc2978`):** SyncDuke now redraws the game's own on-screen
  **quotes and multiplayer chat** as real ANSI characters in a text tier — `operatefta()`
  captures the strings (instead of rasterising the block font) and the door overlays them,
  cell-diff-excluded, over the block frame (matching SyncDOOM's HUD/chat approach). The
  same commit skips the two illegible exit "order" splashes in text tiers. Remaining vs
  SyncDOOM: Duke's **menus** (New Game, options, etc.) still block-render, and the door's
  own overlays (splash/tier label/stats) were already ANSI.

### 4a. Text-tier divergence — SyncDuke has no Unicode tier, and its two block tiers look different

Two concrete differences observed on-screen (conhost captures, `D:\OneDrive\Pictures\Screenshots 1\2026-07-01 (4)/(5).png` — (4) half-block, smoother; (5) blocks&shades, coarser/shade-dithered), both root-caused in code:

**Gap B — no UTF-8/Unicode text tier (SyncDuke always CP437).** SyncDuke only ever
exposes **half-block** and **blocks&shades**, even on a UTF-8 terminal, because:
- It **hardcodes `RT_CP437`** in the only `rt_config` call — `syncduke_io.c:449`:
  `rt_config(cols, rows, mode, syncduke_is_syncterm() ? RT_24BIT : RT_8BIT, RT_CP437)`.
  There is **no client-charset detection** anywhere in the door (no `terminal.ini`
  read, no `-charset`/`[video] charset` key — grep finds `RT_CP437` only here).
- The F4 cycle only appends `SD_HALF` and `SD_BLOCKS` to the available list
  (`syncduke_io.c:642-643`). `SD_QUADRANT`/`SD_SEXTANT` are defined in the enum
  (`:367`), named (`:553-554`), and handled in the render dispatch (`:1031-1032`),
  but are **never added to `avail[]` and never auto-selected** — dead code. The
  in-code comment "quadrant/sextant are UTF-8-only so they aren't offered on a
  CP437 SyncTERM" (`:625-626`) describes an intent that no code path can satisfy,
  since the charset is wired to CP437 unconditionally.

By contrast **SyncDOOM detects the charset and offers Unicode tiers:**
`read_terminal_ini()` sets `g_rt_charset = RT_UTF8|RT_CP437` from `<node>/terminal.ini`
(`syncdoom.c:2651-2707`), passes the real charset into `rt_config` (`:2103`), and
**conditionally builds `quadrant` + `sextant` states when `RT_UTF8`**
(`:2150-2154`); half-block uses the terminal's native glyph (0xDF CP437 / U+2580
UTF-8, `:2140-2143`). So a UTF-8 client of SyncDOOM gets the higher-resolution
quadrant/sextant tiers; a UTF-8 client of SyncDuke is stuck on CP437 half/blocks.

**Gap C — half-block vs blocks&shades diverge in SyncDuke but not in SyncDOOM.**
Both doors call the *same* `termgfx rt_render_frame`, so identical
`(cols,rows,mode,depth,charset)` + identical RGB would produce identical output.
The divergence therefore comes from **how each door configures/feeds the renderer**,
and the observation points at two config differences worth confirming as the cause:
- **Color depth on non-SyncTERM.** SyncDuke forces `RT_8BIT` on any non-SyncTERM
  client (`syncduke_io.c:449`); SyncDOOM derives `g_colors` (24-bit vs 8-bit) from
  `terminal.ini` (`syncdoom.c:2711-2713`). Coarser color quantization pushes the
  blocks&shades tier toward shade-glyph dithering (`░▒▓`), pulling it visually away
  from the clean 2-subpixel half-block — consistent with screenshot (5) vs (4).
- **Framebuffer downsample.** SyncDuke pre-packs a nearest-neighbour RGB downsample
  of the 320×200 fb to the cell grid (`syncduke_pack_rgb`, `:375-396`) before the
  block renderer runs; if SyncDOOM hands the renderer higher-res RGB and lets it do
  its own averaging, the two tiers would agree more closely. This is the likely
  second contributor and should be verified by A/B-configuring both doors with the
  same depth/charset on the same terminal.

*(Gap C is a reported-and-plausible root cause, not yet bisected; Gaps A and B are
confirmed in code.)*

---

## Where SyncDuke trails, and the cheapest paths to parity

1. **Node-social features (biggest win, lowest cost).** `termgfx/sbbs_node.c`
   already implements who's-online, paging, page-receipt, and `NODE_EXT`
   presence; SyncDOOM shows the wiring. SyncDuke just needs to call them
   (Ctrl-U / Ctrl-P handlers, an exit-time status clear, a presence string).
2. **Legible in-game message overlay — DONE (`6aeadc2978`).** Duke's quotes and
   multiplayer chat now redraw as real ANSI characters on text terminals (cell-diff
   exclusion + real-char draw), plus the illegible exit splashes are skipped. Only
   Duke's *menus* remain block-rendered — a further step if wanted.
2a. **Client charset detection + Unicode tiers (Gap B).** SyncDuke hardcodes
   `RT_CP437` and never reads the client charset, so its quadrant/sextant tiers
   are unreachable even on UTF-8 terminals. Port SyncDOOM's `terminal.ini`
   charset detection (and a `-charset`/`[video] charset` override), thread the
   real charset into `rt_config` instead of the literal `RT_CP437`, and add
   `SD_QUADRANT`/`SD_SEXTANT` to the F4 `avail[]` list when the charset is UTF-8.
   Cheap — the renderer already supports these modes.
2b. **half-block vs blocks&shades consistency (Gap C).** Investigate why the two
   block tiers diverge in SyncDuke but not SyncDOOM — start by matching
   SyncDOOM's color-depth selection (don't force `RT_8BIT` on non-SyncTERM) and
   the framebuffer→renderer feed (downsample resolution).
3. **Multi-GRP / map selection.** The blocking `getch()` GRP prompt is fixed
   (first match wins, no hang). Remaining: an `[grp]`/`[game]` ini key to *name*
   a specific GRP when several are present, plus episode/skill config keys.
   User-map (`.map`) support is a further step.
4. **Multiplayer maturity.** SyncDuke already has the lobby/registry, cross-node
   discovery, waiting room, and node paging (shared `game_lobby.js`). To match
   SyncDOOM's *depth* it would need: a dedicated/detached server, deathmatch
   (Dukematch) mode selection, and >2-player support — its co-op is 2-player LAN
   today.
5. **Events log / lobby activity feed.** Add a `-eventlog` door writer (pickups,
   deaths, level clears) + a lobby recent-activity view, mirroring SyncDOOM's
   `events.jsonl` (§3a). Low cost, high "someone's been playing" signal.
6. **Doc hygiene.** SyncDuke's README/DESIGN/PLAN still say "single-player, no
   multiplayer," contradicting the shipped `syncduke_net.c` LAN co-op **and its
   `lobby.js`**.

## Caveats / provenance

- The config **templates** `xtrn/<door>/{syncduke,syncdoom}.example.ini` ARE
  committed (tracked in git); the live per-BBS `syncduke.ini`/`syncdoom.ini` are
  per-install (untracked, seeded from the example by `install-xtrn`). Config keys
  cited come from those templates and the parsing code (`syncduke_config.c`). (An
  earlier draft wrongly said no example.ini was committed — it globbed only
  `src/doors/**`, but the templates live under `xtrn/`.)
- **Both doors ship a JavaScript lobby** (`xtrn/<door>/lobby.js` + `*_lib.js` on
  the shared `exec/load/game_lobby.js`); much node coordination lives there, not in
  the C door. SyncDOOM's wadset picker, events feed, and federation are variously
  JS, C, or **designed-not-built**; this report distinguishes engine/door C code
  from lobby-JS and from unbuilt design where the sources make it explicit. (An
  earlier draft wrongly called SyncDuke lobby-less by surveying only its C — the
  lobby is `xtrn/syncduke/lobby.js`.)
- `termgfx` is shared: several capabilities exist in the library but are wired
  in by SyncDOOM only — noted as "library provides / door doesn't call."
