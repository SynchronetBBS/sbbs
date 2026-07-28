# games.ini -- per-cabinet facts the core will not tell us

Status: implemented. The reader (`syncretro_games.c`), the help-screen
integration (`syncretro_binds.c`), the lobby's title lookup
(`syncretro_lobby.js`), and the `probe_core -hold` measurement tool described
in sec 8 are all in the tree; sec 10 lists the tests that pin the contract.
Scope: the arcade console (`xtrn/syncarcade`), its lobby, and the door's help
screen. See [M2_INPUT.md](M2_INPUT.md) §3 for the binding table this labels, and
[DESIGN.md](DESIGN.md) §15 for where the milestones sit.

This replaces `names.json` with `games.ini`, and widens it from a display-title
map into the small table of per-cabinet facts that neither the door nor the
lobby can discover at runtime.

---

## 1. Goals / non-goals

Goals:

- Tell a player which key **fires** on the cabinet they are actually playing.
- Show the second-stick keys **only** on cabinets that have a second stick.
- Keep the display titles the picker already depends on.
- Cost nothing to the cartridge consoles, which ship no such file.

Non-goals:

- **Per-game core options.** `syncretro.ini`'s `[options]` is per-console and
  stays that way. A per-game override is a different feature with a different
  consumer, and nothing here needs it.
- **Guessing.** A romset whose controls have not been measured gets no control
  entry. See §8.
- **Replacing `SET_INPUT_DESCRIPTORS` handling in general.** This is a
  hand-curated table for one core that sends none. A core that *does* send them
  is out of scope, and remains the M3 fallback [M2_INPUT.md](M2_INPUT.md) §10
  describes.

## 2. The problem

MAME 2003-Plus reports *how many* buttons a driver wants -- "Supports 6 distinct
button controls" for Street Fighter II, 0 for Pac-Man -- but sends **no**
`SET_INPUT_DESCRIPTORS`. So the door knows a cabinet has buttons and cannot know
what any of them *is*. `syncretro_binds.c` says so at the arcade table, and
numbers them 1-6 in RetroPad id order because that is the most it can honestly
claim.

That numbering is not MAME's. Measured against the real core and real romsets:

| game | fires on | which the arcade table calls |
|---|---|---|
| Battlezone (`bzone`) | RetroPad **Y** = key `C` | "button 3" |
| Centipede (`centiped`) | RetroPad **B** = key `Z` | "button 1" |

Two games, two different ids, neither matching the door's numbering. A player is
told "buttons 1 and 2" and has to hunt. There is no rule to infer -- the driver
decides -- so the mapping has to be recorded per game or not known at all.

[M2_INPUT.md](M2_INPUT.md) §10 rejected descriptor-driven help on the grounds
that a core's descriptor strings describe *its* RetroPad convention, which the
door's remap invalidates. That objection does not apply here: there are no
strings to be invalidated, because this core sends none. A hand-curated table is
not a worse version of the descriptors -- it is the only version.

A second, newer reason: the arcade profile now binds `I` / `K` to the RetroPad's
right stick, because MAME puts a twin-stick cabinet's second stick there and
nowhere else. The binding is per profile, so the help screen offers those keys
on every cabinet -- including the ~46 that have one stick. The door cannot tell
which is which. This file is how it learns.

## 3. Why a file the door reads, and not an argument

The lobby cannot pass this on the command line. The BBS assembles a door's
command line into `xtrn.cpp`'s `fullcmdline[MAX_PATH + 1]` -- 260 usable
characters -- and **truncates it silently**. One arcade game's line already runs
to roughly 240. This is the same constraint that put `[options]` in
`syncretro.ini` rather than on the command line, and it is not negotiable.

So the door opens the file itself. It already has both things it needs: its own
directory, and the ROM path whose basename is the key.

## 4. Format

An ini, not JSON, for one reason: **both consumers already parse it.** The door
reads ini through xpdev (`iniGetNamedStringList()`, as `syncretro_config.c`
does); the lobby reads ini through `File.iniGetObject()`. JSON is native only on
the lobby side -- in the door it would mean shipping a parser, new C that has to
build clean on MSVC and GCC/Clang, to read what is a flat key/value table.

An ini also degrades better. A stray comma makes `JSON.parse()` throw, and
`syncretro_lobby.js` can only respond by discarding **the whole file**; the ini
reader yields nothing for a bad line and keeps the rest.

```ini
; games.ini -- what MAME 2003-Plus never tells the frontend.
; Keyed by ROMSET NAME (the zip's basename). Everything but `name` is optional.

[bzone]
name     = Battlezone
button.Y = Fire
stick2   = Right tread

[centiped]
name     = Centipede
button.B = Fire

[pacman]
name = Pac-Man
```

| key | read by | meaning |
|---|---|---|
| `name` | lobby | display title for the picker |
| `button.<id>` | door | what RetroPad `<id>` is on this cabinet. `<id>` is a RetroPad button name: `B` `A` `Y` `X` `L` `R` |
| `stick2` | door | this cabinet has a second stick; the value labels it |
| `boot_frames` | door | frames of power-on self-test to run before the player is shown anything (sec 13). Also valid at the **root**, where it is the install-wide default |

### The root section

A key written **before any `[section]`** applies to every romset, and a section's
own key overrides it. Only `boot_frames` uses this: a button label is a fact
about one cabinet and can have no sensible default, but a warm-up is a policy
for the install, and the romsets that most need it are exactly the ones nobody
has written a section for.

```ini
boot_frames = 900          ; install-wide: every cabinet skips its self-test

[sf2]
name        = Street Fighter II
boot_frames = 1200         ; ...this one's runs longer
```

Comments in the shipped file carry **no relative-time language** -- no "today",
"now", "currently", "as before". The file outlives the change that introduced
it, and a sysop reading it cold has no reference point for when "today" was.
Temporal framing belongs in this document or a commit message.

### Why ids and not keys

`button.Y = Fire`, not `fire = C`. Which *key* reaches RetroPad Y is the binding
table's business, and the table can change -- it did when `I` / `K` were added.
Data that named keys would rot silently the next time it does. The file records
what the door cannot derive; everything derivable stays derived.

## 5. Consumers

**Lobby** (`syncretro_lobby.js`, `syncretro_lib.js`). Reads `name` only.
`syncretro_names_set()` keeps its contract -- a flat `romset -> title` map, keys
lower-cased -- and only its source changes, from `JSON.parse` to
`File.iniGetObject()`. `syncretro_parse_title()` is untouched. The `_comment`
key convention disappears with the JSON: an ini has real comments, so
`syncretro_names_set()` no longer needs to skip keys beginning with `_`.

**Door.** Reads the single section matching its ROM's basename, and feeds
exactly one thing: the help screen. It does not read `name` -- the lobby already
passes the parsed title on the command line.

The binds table stays the single source of truth for *which key does what* --
the M2 invariant that keeps input and help from drifting. `games.ini` supplies
**labels only**. Concretely, `sr_bind_help_line()` gains a per-game override on
its `desc`.

## 6. Help-screen rendering

Before this change, the arcade help groups keys: `Z X` is one row reading
"buttons 1 and 2". A
group cannot be relabelled as a unit when only one of its keys has a label, so:

> **When a game supplies any `button.*` label, the button rows render one line
> per key. When it supplies none, they stay grouped.**

A key whose id has no label is omitted rather than shown unlabelled -- on a
one-button cabinet the other five do nothing, and listing them is the confusion
this file exists to remove. The `stick2` line appears only when `stick2` is set.

That omission makes labelling a game an **all-or-nothing contract**: a section
that labels any button is asserting that the buttons it does *not* label do
nothing on that cabinet. Half-labelling a six-button game hides four working
buttons, which is a worse failure than the vague numbering this replaces. A
curator who has measured only some buttons of a multi-button game leaves the
section at `name` alone until the rest are measured. §10 pins this rule; the
shipped file must not violate it.

Battlezone, fully populated:

```
W A S D <-> arrows   joystick
C                    Fire
I K                  Right tread
Bksp                 INSERT COIN
Enter                1-player start (after a coin)
```

Pac-Man, title only, is unchanged from the current grouped rendering.

## 7. Loading and failure

Three states must never block play: a missing file, a mistyped section name,
and a genuinely unmeasured cabinet all have to fall back to the generic
numbering rather than fail. The original design generalized that into "don't
speak either" -- on the reasoning that a quiet fallback was the safer default
-- and so those three states, plus a working section, produced identical
stderr. That conflated two different requirements: **don't fail** (true, and
unchanged below) and **don't say what happened** (which only made the feature
undiagnosable -- a missing file, a typo'd section, and an honestly-unmeasured
cabinet were impossible to tell apart from outside the source).

The fix is one informational line per load, on the module's existing
`fprintf(stderr, "syncretro: ...")` channel -- already routed to the server
log on POSIX and to `syncretro.log` on Windows, and already far from the only
line the door writes there:

| situation | reported line |
|---|---|
| file loads, section found | `syncretro: games.ini: [<romset>] <N> button(s)[, stick2 "<label>"]` |
| file loads, no section for this romset | `syncretro: games.ini: no section for "<romset>"` |
| no `games.ini` in the launch dir | `syncretro: games.ini: no file in <dir>` |

| situation | behaviour |
|---|---|
| no `games.ini` | picker lists raw romset names; help uses the generic numbering. Non-fatal; reported (above). |
| file present, no section for this romset | as above, for that game only; reported |
| section present, no `button.*` | title only; generic numbering; reported as 0 buttons |
| unknown id in `button.*` (e.g. `button.Z`) | that key ignored, one `LOG_WARNING`; rest of the section applies |
| malformed line | the ini reader yields nothing for it; there is no whole-file failure mode |

The door must still never treat a missing or empty file as an error -- an
arcade install with no `games.ini` is a working install, and stays one. What
changed is that the door reports which of the three silent states it found,
instead of leaving them indistinguishable from each other and from success.

## 8. Filling it in

The file is worth exactly what has been measured into it, and measuring it needs
a tool change: **`probe_core` gains a `-hold <id>` option** that holds one
RetroPad id from a given frame, so a captured frame can be diffed against an
idle capture to show whether that id does anything. The option was prototyped
against a scratch copy of `probe_core.c` to establish the two rows in §2;
landing it in the committed `probe_core.c` is part of this work, and is the
smaller half of it.

`-coin` already exists and is a prerequisite: a cabinet ignores every button
until a credit is in and the game has started, so a probe run that skips it
measures the attract mode and reports that nothing does anything. Two of the
four games in the first measurement pass returned exactly that -- an
inconclusive run is not evidence of a dead button.

**Absent beats guessed.** A romset whose fire button has not been measured gets
`name` and nothing else, and falls back to the generic numbering. A wrong label
is worse than a vague one -- the same reasoning that made the arcade table
number its buttons instead of naming them.

The ~48 other installed romsets are a measurement pass, not a guessing pass, and
can land incrementally after the code does.

## 9. Migration

**Hard break.** `names.json` is no longer read. The shipped bundle gains a
`games.ini` carrying every title the shipped `names.json` had; a sysop who
hand-added titles re-adds them, and the README says so.

Chosen over a compatibility path because the shipped file is the bulk of the
data, custom entries are rare, and a loader that reads two files in priority
order is a permanent cost to avoid a one-time one.

## 10. Testing

| test | pins |
|---|---|
| `test_games` (new) | lookup of a present game, an absent game, a section with no buttons, an unknown id, and a missing file |
| `test_binds` (extend) | the §6 ungrouping rule: labels present -> one line per key; labels absent -> the existing grouped rows, unchanged; an unlabelled id omitted when its section labels anything |

The lobby half has no JS harness in this tree, so it is verified by hand with
`jsexec` reading the shipped file back through `File.iniGetObject()`.

## 11. Risks

- **The measurement pass stalls.** Then the file ships mostly titles and the
  help screen is no better than it is for those games -- but no worse, and the
  two payoffs for Battlezone are already real.
- **A driver's fire button differs between romset revisions** (a parent and its
  clone). Keyed by romset name, so each gets its own section; the risk is only
  that a clone silently inherits nothing. Acceptable: it falls back.
- **Scope creep into a general metadata file.** Resisted in §1. The moment this
  grows per-game core options it needs a different design, because those have a
  different consumer and a different lifetime.

## 12. Known limitation: the `stick2` asymmetry

`button.*` and `stick2` treat the same absence in opposite senses. That is by
design, not an inconsistency to fix.

For `button.*`, absent means "unmeasured," and the door falls back to the
generic numbering (§6) rather than guess -- a wrong label is worse than a vague
one (§8).

For `stick2`, absent means the inverse: "this cabinet has one stick," and the
door suppresses the `I` / `K` help line rather than falling back to a vague
label for a control that is not there. Sections with one stick vastly outnumber
the twin-stick ones, so the fallback that is right for a sparse field (buttons)
would be noise for this one.

The honest reason `I` / `K` can only ever label half a control: the arcade
profile binds the RetroPad's right stick's **Y axis only**
(`SR_AXIS_RIGHT_Y_NEG` / `SR_AXIS_RIGHT_Y_POS`, see `syncretro_binds.h`) -- it
does not bind the X axis. A genuinely 8-way twin-stick cabinet -- Robotron,
Crazy Climber -- steers its second stick in two dimensions, so a keyboard
reaching only its Y axis was only ever half a control, and the blanket help
line this file replaced ("2nd stick up / down (twin-stick cabinets)" on every
cabinet) oversold what a player could do with it.

`stick2` alone does not set the section's labelled flag (`sr_games_labelled()`
looks only at `button.*`), so adding `stick2` to a cabinet never triggers the
all-or-nothing button rule in §6 -- a curator can record the second stick's
label on a cabinet whose buttons remain unmeasured, or the reverse, and the two
facts do not interact.

`stick2` should be measured for twin-stick cabinets during the measurement pass
(§8), the same as the buttons.

## 13. `boot_frames`: the power-on self-test

A cabinet does not start playable. A driver spends its first seconds on a RAM
and ROM check, a colour bar, a licence screen -- and the door runs all of it at
the emulated 60 fps, because `sr_pace_to_rate()` paces the core to its native
rate whether or not the player can act on what is on screen. They can't --
nothing on a coin-op responds to input before a credit is in.

`boot_frames = N` runs the first N frames **unpaced** -- back-to-back
`retro_run()` with the core's video and audio discarded
(`sr_bridge_set_warmup()`) -- then arms the pacer and starts drawing. Measured
against MAME 2003-Plus:

| | |
|---|---|
| emulation speed, this core | 68-149x realtime standalone; ~47x inside the door |
| a 900-frame (15 s) warm-up | ~0.3 s of CPU |
| `retro_load_game()`, which this does **not** touch | 4-142 ms depending on romset |

### Why not a save state

`retro_serialize()` after the boot, cached and restored on later launches, is
the obvious alternative, and MAME 2003-Plus supports it -- measured on seven
romsets, all round-tripping byte-identically at 32-324 KB in under a
millisecond, with no `SET_SERIALIZATION_QUIRKS` declared. It was still the wrong
trade here:

- It **cannot** speed up loading the ROM. `retro_unserialize()` requires a
  loaded game, so the zip decode and ROM decrypt happen either way. A save state
  only ever skips *emulated* time -- the same thing the warm-up skips, for
  tenths of a second less.
- The blob carries no version stamp, so the cache has to be keyed on the core
  binary, the romset **and** the resolved option values, and a miss on any of
  those restores garbage into the emulator.
- Save-state support in this lineage is **per-driver** across some 5000 drivers.
  A driver with partial support can restore a subtly broken machine, which
  fails as a wrong-looking game rather than as an error.
- The core loads NVRAM at start and writes it at unload. A boot snapshot
  restored on every launch can roll back or clobber high scores -- for an arcade
  door, the state players care most about.

The warm-up has none of these properties because it is not a shortcut: the same
instructions execute on the same emulated cycle counts, self-tests and NVRAM
load included. MAME's timing is in cycles, not host time, so the driver cannot
tell. Only the waiting is gone.

### Choosing a value

Unlike `button.*`, this needs no per-romset measurement to be useful. Warming
up **past** a short boot only lands the player further into the attract loop --
where they would have been sitting anyway -- so one install-wide root value is a
complete answer, and a section's own key is for the outliers. `900` (15
emulated seconds) covers the self-test of every romset measured; `sf2`, one of
the longest, finishes between frames 600 and 900.

Values are clamped to `SR_BOOT_FRAMES_MAX` (18000 frames, five emulated
minutes) with a warning. The warm-up is real CPU spent on every node with the
player looking at a static screen, so a slipped digit must not be obeyed.

### What the player sees

The door's own "Loading SyncRetro..." splash, which is already up: the warm-up
sits between `rc_core_load_game()` and the first `sr_io_present()`, so no new
screen is needed. A session limit (`-t`) or a dropped carrier still ends the
door mid-warm-up -- the loop polls `sr_door_should_exit()` every frame.
