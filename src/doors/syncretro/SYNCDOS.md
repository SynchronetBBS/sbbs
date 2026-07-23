# SyncRetro -- `syncdos` sketch: a DOS console via dosbox_pure

Status: **sketch. Nothing here is built.** This is the written-up form of a
design conversation, not a milestone. It commits to no schedule and to no code;
its job is to record the shape of a DOS console, the decisions already made, and
-- above all -- the facts that must be **measured against the real core** before
any of it is real. Where this doc asserts a dosbox_pure characteristic, treat it
as a hypothesis to verify (§2), the same way [M3_MULTICORE.md](M3_MULTICORE.md)
§2 measured fceumm before trusting it.

The feature is a DOS *platform* console; any one game is just content in its
library. Master of Orion II gets named throughout as the **SVGA stress-test
case** -- 640x480, dense mouse-driven UI -- because it is near the *hard* edge of
what a terminal can render, not because it is the showcase. The showcase is the
opposite: 320x200, slow-paced, generous-target games (§1a). Do not read MOO2 as
"the easy first game"; it is the "can we even do this?" game.

---

## 1. What `syncdos` is -- and is not

It is **not a new binary.** SyncRetro is one frontend (`syncretro`) that hosts
any libretro core; a "console" is a directory under `xtrn/` with a `lobby.js`
spec, a `getcore.js`, and config -- exactly the model M3 established. So there is
no `syncmoo2` and no `syncdos` executable. There is a `syncdos` **console dir**,
identical in shape to `xtrn/syncivision` and `xtrn/syncnes`:

```
load("syncretro_lobby.js");
syncretro_lobby({
    dir:      js.exec_dir,
    name:     "MS-DOS",
    short:    "DOS",                 /* -> id "dos" */
    core:     "dosbox_pure_libretro",
    profile:  "dos",                 /* NEW profile -- see §5 */
    ext:      [ ... DOS content shapes -- see §4 ],
    ...
});
```

**Named for the platform, not the emulator.** `syncivision` is the Intellivision
(its core happens to be `freeintv`); `syncnes` is the NES (core `fceumm`). By that
convention the DOS console is `syncdos`, with `dosbox_pure` as the core
`getcore.js` fetches -- not `syncdosbox`, which would name the emulator the way
"syncfceumm" would have named the NES. This keeps the row consistent in
[../../../docs/door_graphics_audio_matrix.md](../../../docs/door_graphics_audio_matrix.md).

A game is then one item in the console's library, discovered by the lobby's ROM
picker like any `.nes`/`.int` cartridge. Nothing about the console is game-specific.

---

## 1a. What content fits -- and the tier above this one

"It's just content" is true at the *plumbing* layer -- no per-game C -- and says
nothing about whether a game is *playable* over a terminal. Two axes decide that,
and a game must pass **both**:

1. **Resolution -- 320x200 (VGA mode 13h).** It pixel-doubles to exactly 640x400,
   which *is* SyncTERM's default 80x25 canvas: clean 2x integer scale, no resample
   blur, small text survives. This is the C&C sweet spot (its DOS original is
   320x200). **640x480 SVGA overshoots** the canvas and loses small text -- which
   is where MOO2 lives, and why it is the stress-test, not the showcase.
2. **Interaction -- latency-tolerant, coarse-mouse-friendly.** The terminal path
   adds frame-pacing + encode latency and quantizes the mouse to cells. Turn-based
   and slow real-time (C&C's select-and-command) with generous targets pass;
   real-time *twitch* (flight sims, action) fails even at 320x200.

The good-fit quadrant is **320x200 + turn-based-or-slow + big click targets**:
Dune II, X-COM, Civilization I, Master of Magic, and the like. MOO2 fails axis 1;
a flight sim fails axis 2; neither is a good candidate.

**The three-tier rule (curation, not just fit).** Even a game that passes both
axes belongs here only if nothing better hosts it:

1. **Native reimplementation door** -- Vanilla Conquer -> C&C / Red Alert
   (`syncconquer` / `syncalert`), Chocolate Duke -> Duke3D, 1oom -> MOO1, the DOOM
   door, ScummVM -> the adventure catalog ([`../syncscumm`](../syncscumm/README.md)),
   EasyRPG -> RPG Maker 2000/2003 ([`../syncrpg`](../syncrpg/README.md)). Cleanest
   integration; a game with a native door is **never** run under a core. (This is
   why C&C and Red Alert are *not* `syncdos` candidates despite fitting perfectly
   -- we reimplemented them.)
2. **Purpose-fit libretro core** -- a core that reimplements a whole engine or
   platform rather than emulating a machine. **Currently empty:** ScummVM was the
   candidate ([SYNCSCUMM.md](SYNCSCUMM.md)), but it was built natively as a door
   instead and moved up to tier 1. The tier stays as a slot for the next such
   core.
3. **dosbox_pure (`syncdos`, this doc)** -- the DOS long tail with neither of the
   above.

So the genuine `syncdos` list is what survives all three cuts: 320x200,
latency-tolerant, and un-reimplemented. The Sierra/LucasArts adventures that look
like they belong here are gone to tier 1, in the native
[`../syncscumm`](../syncscumm/README.md) door; C&C/RA are tier 1; MOO2 stays, but
as the SVGA stress-test.

---

## 2. Why dosbox_pure, and what must be measured first

**Mainline DOSBox was never an option.** SyncRetro can only load a libretro core,
so the choice is among the *libretro DOSBox cores* -- `dosbox_pure`,
`dosbox_core`, `dosbox-svn`. `dosbox_pure` is the pick because its model matches
the door: it mounts a game **ZIP/directory directly** (zero `dosbox.conf`, zero
`MOUNT` commands), has a built-in executable **start menu**, implements libretro
**save states** (relevant to M5), and ships a **built-in General MIDI synth**
(no soundfont needed; MT-32 works if the sysop supplies ROMs). The other two are
closer ports of stock DOSBox and still expect manual mounting -- the opposite of
what this console wants.

The trade-off: dosbox_pure is built on the **DOSBox 0.74** base, so raw
emulation accuracy/performance is "classic DOSBox," a step behind DOSBox-Staging
/ DOSBox-X. For a preservation door that is a fine trade. **Sound is not a
concern**: AdLib/OPL2/OPL3, Sound Blaster/Pro/16 (FM + PCM), Gravis Ultrasound,
Tandy and PC speaker are all inherited from the 0.74 base, and whatever the
emulated card produces leaves the core as one mixed PCM stream -- SyncRetro's
audio bridge forwards it exactly as it forwards fceumm's or FreeIntv's.

**The M3-style probe is task zero, and it gates everything below.** Build a probe
that mirrors `retro_core.c` + `retro_env.c` (the analogue of the fceumm harness)
and record, for the *actual* `.so`:

| Fact | Why it gates something |
|---|---|
| `library_name`, `valid_extensions`, `need_fullpath` | console spec + content gating (§4) |
| geometry / fps / `sample_rate` | renderer + the audio-rate fix M3 already generalized |
| **Does it implement `retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)` over the low 1 MB?** | the entire text tier (§7) lives or dies here |
| **Which input devices does it read** -- `RETRO_DEVICE_MOUSE` / `KEYBOARD` / `POINTER`? | the input bridge (§6) |
| Its core-option strings (start-menu / auto-start behavior) | the exe-launch policy (§3) |

Until that probe is run, §6 and §7 are *proposals*, not commitments. Do not write
either before the probe answers its row of the table.

---

## 3. Launching a specific executable -- never show the picker

dosbox_pure's start menu appears only when content is **ambiguous** (multiple
executables, no instruction). A door must never expose it. Three ways out, in the
order we'd rely on them:

1. **Bundle a `dosbox.conf` with an `[autoexec]` in the content** -- the
   deterministic way. dosbox_pure runs the `[autoexec]` verbatim on boot:
   ```
   [autoexec]
   C:
   CD \MOO2
   ORION2.EXE
   EXIT
   ```
   This skips the menu, runs exactly the named `.exe`/`.com`/`.bat`, and (with
   `EXIT`) ends the DOS session when the game quits. It is **per-game and
   self-contained** -- the packaging says how to launch, so the lobby stays
   DOS-agnostic. This is the primary mechanism.
2. **Single-executable content auto-runs** with no menu and no config at all. The
   picker is only ever the multi-exe case.
3. **The frontend forces auto-start** via a dosbox_pure core option (SyncRetro
   *is* the frontend; `retro_env.c` sets core options). Secondary, because it
   depends on getting the exact option string right -- which §2's probe must
   supply. Do not hardcode an option name from memory.

---

## 4. Content discovery -- DOS "ROMs" are not cartridges

A cartridge console has one file per game. A dosbox_pure "ROM" is a game
**directory**, a **`.zip`/`.dosz`**, a CD **`.iso`/`.cue`**, a floppy **`.img`**,
or a bare **`.exe`**. So the lobby's discovery (today: glob by extension, size-band,
full-file MD5, dedupe -- see [LAUNCHER.md](LAUNCHER.md) §6) needs to learn DOS
content **shapes**:

- A game is often a *directory* or an *archive*, not a single stat-able file, so
  the size-band and whole-file-hash identity rules need a per-shape answer (hash
  the archive; for a directory, a manifest hash).
- One content item may bundle its own `dosbox.conf` (§3), which is part of its
  identity, not a separate ROM.
- The discovery **cache** (M3 §6) still applies -- warm entry keyed by
  size+mtime, cold path says so -- but the "candidate" is now a shape, not a file.

This is **generic to the console**, not to MOO2, and it is the JS half of the work.

---

## 5. Input: a `dos` profile is not enough -- the frontend needs mouse+keyboard

SyncRetro feeds cores a **RetroPad** today. Every `RETRO_DEVICE_*` we handle in
our own code (`syncretro_input.c`, `syncretro_binds.c`, `syncretro_keypad.c`) is
`JOYPAD` or `ANALOG`; `MOUSE`/`KEYBOARD`/`POINTER` appear only in the vendored
`libretro.h`. A DOS strategy/adventure game is mouse-and-keyboard driven, so a
new `dos` profile in the M3 profile system is necessary but **not sufficient** --
the profile selects a bind table, but there is no mouse/keyboard *device* for it
to bind to yet.

So the real work is a **device bridge in the frontend**: expose terminal SGR
mouse and key input to the core as `RETRO_DEVICE_MOUSE` (relative motion +
buttons) and `RETRO_DEVICE_KEYBOARD`, via `input_state` in `retro_bridge.c`. Two
things make this tractable and worthwhile:

- We **already ingest** SGR mouse bytes -- `syncretro_input.c:466` skips the SGR
  `<` marker while parsing the input stream. The terminal-side plumbing to
  *receive* mouse events is partly there; what is missing is *exposing* them as a
  libretro device rather than folding everything into the pad cache.
- It is written **once and reused**. Unlike SyncMOO1, where mouse handling lived
  in a per-engine `hw_sbbs.c` backend, here the mouse/keyboard bridge is written
  against the standard libretro device API and **every** keyboard/mouse core
  inherits it -- MOO2, any DOS game, point-and-click adventures. It is
  console-level, not per-title.

Gated on §2 confirming dosbox_pure actually reads those devices.

---

## 6. Rendering tier: pixels by default, text when it can

For a graphics-mode game the frame is pixels, full stop -- the existing pixel tier
(sixel/JXL, resampled to terminal cells) handles it, no change. A 320x200 game
doubles onto the canvas cleanly (§1a); a 640x480 SVGA game (MOO2) overshoots it
and is the hard case, which is the whole reason MOO2 is the stress-test. The
interesting addition is a **text tier** for the large
text-mode slice of the DOS catalog (utilities, roguelikes, text adventures, TUIs,
text-mode door-style games), where real terminal text/ANSI is dramatically
better than pixel-blitting an 80x25 screen: crisp, tiny bandwidth, works on
**every** terminal including non-sixel ones, and selectable/scrollable.

### 6.1 The mechanism, and its one hard dependency

The DOS API a program uses is a **red herring**: INT 21h stdout (AH=09h/02h/40h),
INT 10h BIOS screen I/O, and direct `B800:0000` writes all land in the same place
-- emulated video memory, which DOSBox rasterizes to the framebuffer. By the time
output crosses the libretro video boundary it is already pixels, whichever
service produced it.

But the text *information* still lives in emulated RAM, and **libretro can expose
RAM**: `retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)` is in our pinned
`libretro.h` (line ~518). We do **not** resolve that entry point in
`retro_core.c` today -- SyncRetro never needed it. If dosbox_pure implements it
over the low megabyte (§2's gating question), the frontend can read, each tick:

- the **BIOS Data Area** (physical `0x400+`): video mode (`0x449`), columns
  (`0x44A`), rows-1 (`0x484`), cursor (`0x450`) -- "are we in text mode, and what
  geometry/cursor".
- the **text buffer** at `0xB8000`: 2 bytes/cell, character code + attribute.

Each cell is a real **CP437 codepoint + DOS attribute**, mapping cleanly onto
what termgfx already emits (CP437 -> UTF-8, attribute byte -> Ctrl-A color).
Reading codes directly recovers the intended characters **even under a
custom-loaded font**, because the program wrote codepoints; only the glyph
bitmaps changed.

**Fallbacks, if §2 says the core exposes no RAM:** (a) fork dosbox_pure to
surface the text buffer -- **rejected by default**, it violates this door's
"never patch a core" rule ([CLAUDE.md](CLAUDE.md)) and changes our maintenance
posture; or (b) reconstruct the grid by **glyph-matching** the framebuffer
against the ROM font -- no patch, but works only for the stock font and breaks on
custom fonts or SVGA text. Prefer the memory read; it is both cleaner and more
robust.

**This helps text modes only.** A graphics-mode game has no character grid to
read, so MOO2 gains nothing here -- it stays on the pixel tier. Recorded so the
text tier is not mis-sold as a MOO2 feature.

### 6.2 The text-tier output model -- changed-rects, promoted to scrollback

We poll a **memory snapshot** (read `0xB8000` each tick), not the program's
output stream, so the mechanism is inherently a **changed-rects cell-differ**:
diff this tick's 80x25 grid against the last and emit cursor-addressing + text
for the cells that changed. That is the text-cell analogue of the pixel
dirty-rect tier, and it is correct for full-screen TUIs.

It is **wrong** for line-at-a-time programs, and the fix is the point of this
section. A line-oriented program writes at the cursor and lets the BIOS **scroll
the screen up** at the bottom row -- which copies rows 1..24 over 0..23. To a
naive differ that is "25 rows changed," so it repaints the whole window in place
and emits **no newline**: nothing flows into the terminal's scrollback, and the
BBS user loses the natural, selectable, scrollable line history that is the
entire reason to have a text tier.

So layer **scroll/clear detection** on top of the differ, and decide **per tick**
from the shape of the diff:

- **Upward shift** -- new rows `0..(24-k)` equal old rows `k..24`? The screen
  scrolled up by `k` lines; the only genuinely new content is the `k` exposed
  bottom rows. Emit `k x (newline + bottom row)` and let the *terminal* scroll,
  pushing old lines into real **scrollback**. This is the scrollback-logical /
  teletype model for line-oriented output.
- **Scattered cell changes**, no whole-screen shift -- absolute-addressed
  dirty-cell **repaint**; the viewport stays put. Full-screen TUI.
- **Whole grid blanked** -- emit a terminal **clear**, don't diff 2000 cells.

The **BIOS cursor** (`0x450`) is the classifier's second signal: a streaming
program pins the cursor near the bottom row and marches it; a random-access TUI
jumps it around. Cursor trajectory + shift-detection together classify reliably.

**Honest limits** (it is heuristic reconstruction, not a faithful output tap):

- **Polling loses intra-tick ordering.** Ten lines written between two polls are
  recoverable *if* the shift-by-10 test matches; but a tick that *also* cleared
  and drew a menu collapses to one final state we can't disentangle -- that burst
  degrades to a full repaint (correct-looking, no scrollback for those lines).
- **Windowed/partial scrolls stay TUI, correctly.** A game scrolling a sub-rect
  (a pane inside a frame, INT 10h AH=06/07 with a window) won't match a
  full-screen shift and *shouldn't* -- leaving it in repaint mode is right.
- **Heuristic is the ceiling for a stock core.** Authoritative stream-vs-screen
  semantics need observing the output ops (hook INT 10h/21h or the CRTC scroll
  inside the emulator) -- i.e. forking the core, which we won't. In practice
  scroll detection is quite good for the common cases, which are most of what
  text-mode DOS does.

---

## 7. Multiplayer / DOS SHARE -- not supported, and why

Classic DOS multinode doors (LORD, TradeWars) achieved "multiplayer" by multiple
node instances sharing **data files on disk**, coordinated by DOS file sharing:
`SHARE.EXE`, deny-mode opens, and INT 21h AH=5Ch **record locking**. dosbox_pure
does not deliver this, structurally:

- **Each instance is an isolated emulated PC** -- it mounts *its* content as C:;
  two instances do not share a DOS filesystem with any arbitration between them.
- **The record locking those games rely on is effectively a stub in the 0.74
  base.** dosbox_pure reports SHARE present enough that games don't bail, but INT
  21h 5Ch is not translated to real host locks -- there was no contention to
  arbitrate in a single-player emulator. (DOSBox-X did more real-DOS/SHARE work,
  but it isn't a libretro core.)
- Mounting the *same host directory* into several instances therefore races: you
  get the sharing without the locking, which is worse than neither.

**libretro netplay is the wrong model.** dosbox_pure's netplay is **lock-step
input mirroring of one emulated machine** (two-players-one-console), not
independent nodes sharing data files, and it is fragile for DOS besides.

**What could work is frontend serialization.** Push coordination up to the lobby,
where SyncRetro already does node work (the wait-room / node-status machinery
behind SyncDuke and the Duke/DOOM waiting rooms): enforce **single-writer access
to a shared host game dir** -- one node in the door's data at a time -- and mount
it into each instance. Viable for **turn-based** doors ("one player acts on the
shared world, then the next"). **Not** viable for safe *concurrent* real-time
multinode, because the emulator won't arbitrate the writes and we'd be
serializing them externally. Deferred, and honestly out of scope until someone
wants a specific turn-based DOS door.

---

## 8. Legal -- games are sysop-supplied, like the Intellivision BIOS

DOS games are non-free and are **never committed**, the same status as
`exec.bin`/`grom.bin` for FreeIntv and `disksys.rom` for the FDS (M3 §9). The
sysop supplies the content; `getcore.js` fetches only the (freely
redistributable) `dosbox_pure` core. MOO2, specifically, is commercial software
the sysop must own and provide. No game data, and no BIOS/game images, in the
repo.

---

## 9. Order of work (when/if this is built)

1. **The probe (§2).** Task zero. Its memory-interface and input-device answers
   decide whether §6/§7 and §5 are even possible as described. Nothing else
   starts first.
2. **Content discovery for DOS shapes (§4)** -- JS, in the lobby/lib. Independent
   of the C work; testable headless with fixtures.
3. **The mouse/keyboard device bridge (§5)** -- C, in `retro_bridge.c` /
   `syncretro_input.c`, plus a `dos` profile. The real unlock for the whole
   DOS/point-and-click category; makes MOO2 actually playable.
4. **The text tier (§6)** -- optional, and only if the probe green-lights the RAM
   read. A parallel render tier chosen per-frame from the video mode; big payoff
   for text-mode DOS, zero help for graphics games like MOO2.

Steps 3 and 4 are independent; a graphics game needs only 3, a text-mode program
benefits most from 4.

---

## 10. Deferred / rejected, with reasons

- **Forking dosbox_pure to tap INT 10h/21h or the text buffer** -- rejected;
  violates "never patch a core." The stock-core memory read (§6.1) is the
  supported path; heuristic scroll detection (§6.2) is the accepted ceiling.
- **Concurrent real-time multinode via DOS SHARE** -- not supported (§7).
  Turn-based via lobby serialization is the only door-side multiplayer, and only
  on request.
- **`syncdosbox` as the name** -- rejected for `syncdos`; name the platform, not
  the emulator (§1).
- **Hardcoding dosbox_pure core-option names from memory** -- rejected; read them
  from the probe (§2, §3).
