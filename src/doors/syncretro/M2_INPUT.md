# SyncRetro M2 -- keyboard input, keypad, help & pause

Status: implemented 2026-07-09. The keypad's *semantics* remain unverified
end to end: nothing reads the controller word until a cartridge runs, and that
needs the Intellivision BIOS. `test_keypad.c` stands in for it (§7).
Scope: the input half of milestone M2 (see [DESIGN.md](DESIGN.md) §15).

M1 proved the frontend end to end but left the user with a partial RetroPad and
no way to reach the Intellivision's 12-key keypad -- which, on this console, is
not an accessory. Games *begin* with a keypad press (game variant, player
count), so without it most cartridges cannot be started at all.

This document specifies how a terminal keyboard drives all 12 keypad keys, and
how the door presents its own help and pause screens.

---

## 1. Goals / non-goals

Goals:

- Every Intellivision input reachable from a bare terminal keyboard, with no
  mouse, no graphics-tier dependency, and no per-ROM data.
- One binding table as the single source of truth, consumed by both the key
  handler and the on-screen help.
- Door-owned help and pause screens.

Non-goals (deferred, §8): the mouse-clickable 370x600 overlay art, the CP437
text tier, `SET_INPUT_DESCRIPTORS`-driven help for unknown cores, save-state
hotkeys.

---

## 2. What the core actually offers

Established by reading FreeIntv's `controller.c` / `libretro.c` at the pinned
revision (see [PROVENANCE.md](PROVENANCE.md) for the core pin). These are load-
bearing facts; re-verify them if the pin moves.

The core exposes **every keypad key through the RetroPad interface**, which our
`sr_pad_get()` synthesizes in full. There is no key we cannot reach:

| Intellivision key | What the core reads (`getControllerState`) |
|---|---|
| 1,2,3,4,6,7,8,9 | right-analog **angle**, via `keypadDirections[8]` |
| 5 | R3 |
| 0 | L3 |
| Clear | L2 |
| Enter | R2 |
| disc (16-way) | d-pad, and/or left-analog angle |
| left / right / top action button | B / A / Y |

The right-analog path (`controller.c:134-147`) divides each axis by 8192, takes
`atan2(Ry,Rx)+PI`, scales by 7/2PI, floors, subtracts 1, and indexes
`keypadDirections[] = {K_2,K_3,K_6,K_9,K_8,K_7,K_4,K_1}`. Simulating that
arithmetic over the eight compass vectors yields a clean, collision-free map --
the digits land on the 3x3 grid exactly where they sit on the physical keypad:

```
    (-1,-1) 1    ( 0,-1) 2    (+1,-1) 3
    (-1, 0) 4    [ R3 ]  5    (+1, 0) 6
    (-1,+1) 7    ( 0,+1) 8    (+1,+1) 9
```

Full-scale axis values (+-32767) are used so integer truncation cannot move a
vector across a bucket boundary. Because keypad bits and disc bits occupy
different parts of the controller word, **a digit may be held while steering** --
something the physical hardware could not do.

Two core features we deliberately leave unused, because the door supersedes both:

- **The mini-keypad** (`drawMiniKeypad`) blits a hardcoded 27x39 **1-bit
  monochrome** grid into the frame while L or R is held, and in that mode the
  d-pad stops driving the disc and instead walks a wraparound cursor over the 12
  keys, one tap per step, with a face button to commit. It costs up to four taps
  and a repaint to type a digit on a keyboard that has digit keys. We never
  assert L or R, so it never draws.
- **The core's pause and help** (`libretro.c:1219-1250`). RetroPad START toggles
  `paused` and paints `HELP - PRESS A`; A then paints a help screen describing
  the core's own RetroPad convention (`L/R - SHOW KEYPAD`, `X - LAST SELECTED
  KEYPAD BUTTON`, ...). After our remap every line of it is false, and it is
  drawn *into the framebuffer*, where the door cannot intercept it. We never
  assert START.

SELECT is no longer sent to the core. FreeIntv's SELECT swaps which physical
controller the RetroPad port feeds -- a workaround for a player with a single
controller. The door instead drives **both** RetroPad ports at once (the core
polls both -- verified at runtime), so both Intellivision hand controllers are
live simultaneously: a solo player uses whichever one the cart reads (no swap
dance), and two people can play on one keyboard. `Tab` is the door's OWN swap of
which core port each player's keys drive (`sr_pad_get()`'s read-time `g_swap`),
so it needs no core feature. See the two-controller note in syncretro_input.c.

Note the core polls each button id individually, because our `retro_env.c` does
not answer `RETRO_ENVIRONMENT_GET_INPUT_BITMASKS` and the core therefore leaves
`libretro_supports_bitmasks` false. If that ever changes, `sr_pad_get()` must
also answer `id == RETRO_DEVICE_ID_JOYPAD_MASK`.

---

## 3. The binding table

Bare keys play the game; Ctrl keys drive the door. Function keys are avoided --
too many terminals mangle them. The two controllers get two key groups:

| Player 1 (controller 0) | Player 2 (controller 1) | Action |
|---|---|---|
| `W` `A` `S` `D` | arrow keys | disc (16-way) |
| `Z` `X` `C` | `,` `.` `/` | action buttons |
| `1`-`9`, `0` | numeric keypad | keypad digits |
| `Backspace` (`0x08` **and** `0x7F`) | numpad `Del` | keypad Clear |
| `Enter` (`\r` and `\n`) | numpad `Enter` | keypad Enter |

Door keys (either player): `Tab` swap the two controllers · `Space` pause · `?`
key legend / help · `Ctrl-R` reset (`retro_reset()`) · `Ctrl-Q` quit.

**The arcade profile only:** `I` / `K`, action `SR_ACT_AXIS`, bound to the
RetroPad's right analog stick (up/down). MAME 2003-Plus reads a twin-stick
cabinet's second stick off that axis and nowhere else -- no button, no d-pad,
no core option reaches it (measured: `mame_remapping`, `input_interface`, and
`digital_joy_centering` each changed nothing). This binding table's Intellivision
and NES profiles have no analog stick to give it and do not carry these two
rows; see `syncretro_binds.c`'s `g_binds_arcade` table.

Player 2's arrows and numpad have no ASCII byte form, so they reach the door
only on the CSI / evdev / kitty paths. On a plain byte terminal the numpad is
indistinguishable from the number row, so there player 2's *keypad* falls back
to player 1's -- player 2's disc (arrows) and action buttons still work on every
terminal. `Tab`'s swap lets a solo player put their preferred key group on the
controller the cart actually reads.

**Not `Ctrl-H`.** An earlier draft offered it; `Ctrl-H` *is* `0x08`, which is
Backspace, which is keypad Clear. Help is `?`.

This frees `q` and `e`, which in M1 map to L and R and therefore *silently trip
the core's mini-keypad and hijack the d-pad*. That is fixed by this change.
`Enter` moves off START (START is no longer asserted at all).

It also fixes a latent M1 bug: the byte and kitty paths both folded case with
`c | 0x20`, which turns `'\r'` into `'-'` and `'\t'` into `')'`. START and
SELECT were therefore unreachable on every path. The fold is now alpha-only.

**Digit semantics.** The analog stick is a single vector, so at most one of
1,2,3,4,6,7,8,9 can be asserted at a time: last press wins, and releasing a
digit clears the vector only if it is the digit currently held. Keys 5, 0,
Clear and Enter are independent button bits and may combine freely with each
other, with a digit, and with the disc.

---

## 4. Components

Two new translation units; the split keeps FreeIntv-specific policy out of the
generic key parser, which M3 (multi-core) will need.

- **`syncretro_binds.c/.h`** -- owns the binding table as data, plus
  `sr_bind_lookup()` and `sr_bind_help_line()`. One table, two consumers: the
  key handler and the help screen cannot drift apart.
- **`syncretro_keypad.c/.h`** -- owns keypad state and analog synthesis:
  `sr_keypad_press(digit)`, `sr_keypad_release(digit)`, and the vector table.
  All FreeIntv-specific knowledge lives here, behind a comment naming the core;
  M3 generalizes it into a per-core profile.

Changed:

- **`syncretro_input.c`** -- `sr_pad_get()` gains an `RETRO_DEVICE_ANALOG` arm
  (index `RIGHT`, ids `X`/`Y`) delegating to `syncretro_keypad.c`; the key map
  is rewritten against `syncretro_binds.c`; `q`/`e`/`Enter` bindings removed.
- **`syncretro_door.c`** (or `main.c`, wherever the run loop lives) -- the pause
  loop and help display.

---

## 5. Data flow

Unchanged from M1 except at the two ends:

1. A key arrives on one of the three negotiated paths (evdev / kitty / byte),
   already normalized by `termgfx/keymode.h`.
2. `sr_bind_lookup()` resolves it to either a pad action, a keypad action, or a
   door action.
3. Pad actions set/clear bits in `g_joypad[]`, exactly as in M1. Keypad actions
   call into `syncretro_keypad.c`.
4. `retro_run()` polls; `sr_pad_get()` answers joypad bits from `g_joypad[]` and
   analog axes from the keypad module.

**Key-up on the byte path.** The byte path has no release event and M1 already
solves this with `sr_pad_tap()`'s auto-release timer. Digits reuse that
mechanism: a digit asserts its vector for the same dwell, then self-clears. On
the evdev and kitty paths, real key-up events drive `sr_keypad_release()`.

**Pause** stops calling `retro_run()`. On entry the door releases every pad bit
and the analog vector (otherwise a held disc direction sticks for the duration),
paints its overlay, and keeps servicing input, carrier detect, node status and
the BBS time limit. `Ctrl-Q` still quits from a paused state. On resume it
invalidates the frame cache so the next frame is a full repaint rather than a
diff against a frame the user never saw.

**Help** is drawn as plain ANSI text over a cleared screen, not into the
framebuffer. That works on every tier -- sixel, JXL, and the future CP437 text
tier -- and on clients with no graphics at all. Dismissing it triggers the same
full-repaint invalidation as resuming from pause.

**Reset** (`Ctrl-R`) releases all input first, then calls `retro_reset()`, then
invalidates the frame cache.

---

## 6. Error handling

- An unbound key is ignored silently; it must never reach the core.
- Keys other than the door actions are ignored while paused or while help is up.
- A digit released that is not the digit currently held clears nothing (guards
  against interleaved down/up on the evdev path).
- A carrier drop while paused or in help still runs the normal hangup path;
  `sr_door_hangup()` restores the key mode first, as in M1.
- `retro_reset()` on a core that never loaded a game is a no-op by contract; we
  do not special-case it.

---

## 7. Testing

- **`test_keypad.c`** (new unit test) -- replicate the five lines of arithmetic
  from `controller.c:134-147` and assert each of the 12 keys maps to the
  intended `K_*` bit, and that the eight digit vectors are collision-free. This
  is the test that catches an upstream change to the core's angle math, and it
  is the primary verification, because...
- ...**end-to-end keypad confirmation needs the BIOS.** Without `exec.bin` /
  `grom.bin` FreeIntv halts at its diagnostic screen and no cartridge reads the
  controller, so no keypad press can be observed changing anything. This is the
  one part of M2 that cannot be validated against the fake core or against
  `4-tris.rom` alone. It is a *verification* gap, not an implementation blocker.
- **`fakecore.c`** -- extend `FAKECORE_PADLOG` to log analog axes, confirming
  our vectors reach `input_state` with the right port/index/id. Covers the
  plumbing, not the semantics.
- **`fakterm.py`** -- pause enter/resume, help show/dismiss, `Ctrl-R`, `Ctrl-Q`
  from a paused state, carrier drop while paused, and that a held disc direction
  does not survive a pause.
- Uncrustify as the closing step, per this directory's CLAUDE.md. Re-apply the
  known `RC_RESOLVE` macro-indent fix by hand afterwards.

---

## 8. Deferred, with reasons

- **Mouse-clickable overlay art.** FreeIntv's `freeintv_multiscreen_overlay`
  core option composites a 370x600 per-ROM overlay beside the game and drives a
  hotspot keypad from a pointer device. `termgfx/sgrmouse.c` means we *could*
  feed it one. It needs core-options plumbing (M5) and a `RETRO_DEVICE_POINTER`
  mapping, and it is strictly a nice-to-have once §3 exists.

  The art: `libretro/FreeIntv`'s `Assets/Overlays.zip` holds 90 PNGs, exactly
  370x600, named by No-Intro ROM name -- i.e. already keyed to the core's
  `freeintv_overlays/<romname>.png` convention. **We must not bundle it**: the
  zip carries no license, FreeIntv's GPLv2+ covers its code and not the scanned
  Mattel art, and the repo disclaims provenance. Treat it exactly as the BIOS
  and the ROMs -- sysop-supplied, with a documented fetch. For browsing,
  <http://www.intvfunhouse.com/overlays/table.php> catalogs several hundred.

- **A text legend per game does not exist.** No machine-readable database maps
  the 12 keys to their per-game meanings; every public source is a scan or
  prose. Authoring one is open-ended content work, and §3 makes it unnecessary
  for play. Not planned.

- **CP437 text tier.** `termgfx/text.h` already has `rt_config()` with
  `RT_CP437`. But FreeIntv hands us a 352x224 frame (the STIC is 160x96; the
  rest is border), and 80x24 with half-blocks is an 80x48 pixel grid -- a ~2:1
  downsample of the playfield in 16 colors. Plausible for Astrosmash, likely
  illegible for text-heavy titles. Stays in M3, where the tier work lives.

- **`SET_INPUT_DESCRIPTORS`-driven help.** Useless for a core that sends
  descriptors: its strings describe *its* RetroPad convention, which our remap
  invalidates. It becomes the right fallback in M3, for cores we know nothing
  about. This is not a decision to never name a cabinet's buttons -- MAME
  2003-Plus is the case the reasoning above does not cover, because it sends no
  descriptors at all, and for that case `games.ini` is the hand-curated answer
  (see [GAMES_INI.md](GAMES_INI.md)).

- **Save-state hotkeys.** DESIGN.md §15 lists these under M2, but save states
  themselves are M5. Moving them there keeps M2 to one coherent change; M5 adds
  the hotkeys alongside the save UI. Amending DESIGN.md §15's M2 bullet to match
  is part of the implementation plan.
