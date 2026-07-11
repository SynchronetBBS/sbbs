# SyncRetro M3 -- a second console: profiles, and a discovery cache

Status: **implemented 2026-07-11.** The NES door (`xtrn/syncnes`) renders,
selects the `pad` profile, and streams audio at the core's own 48 kHz; the
Intellivision is unchanged. Scope: milestone M3 (see [DESIGN.md](DESIGN.md) §15),
taken against **fceumm** (NES) as the second core.

M1/M2/M4 built a frontend that happens to have exactly one console wired into
it. This document specifies what has to become *data* for a second console to
work, what stays hand-written, and the one place M3 grows a component instead of
a parameter: the ROM discovery cache.

The core facts below are **measured**, not assumed -- see §2.

---

## 1. Goals / non-goals

Goals:

- A sysop can install NES as a second door (`xtrn/syncnes`) beside the existing
  Intellivision one, with no C change for a third console that is also a plain
  RetroPad machine.
- The Intellivision install keeps its behavior byte-for-byte, and does not move.
- The lobby names the console properly ("Nintendo Entertainment System" / "NES"),
  from one place that the C door and the JS lobby both read.
- Entering the lobby is fast on a ROM set of any size, over SMB.

Non-goals (deferred, §10): core options and save states (M5), the Zapper and
other pointer devices, per-core controller overlay art, turbo buttons, FDS
disk-swap keys, and a descriptor-derived help screen (§4 says why).

**Explicitly rejected: deriving the bindings from `SET_INPUT_DESCRIPTORS`.**
See §4. Recorded here so it is not re-litigated.

---

## 2. What we are given -- fceumm, measured

Built from libretro/libretro-fceumm at `0d610d9`, probed with a harness that
mirrors `retro_core.c` + `retro_env.c` (the probe is the M3 analogue of
`startprobe.c`; it prints system info, every env call, the AV info, and the
shape of the first 60 frames):

| | FCEUmm (NES) | FreeIntv (Intellivision) |
|---|---|---|
| `library_name` | `FCEUmm` | `freeintv` (lower-case!) |
| `valid_extensions` | `fds\|nes\|unf\|unif` | `int\|bin\|rom` |
| `need_fullpath` | **true** | true |
| pixel format | XRGB8888 | XRGB8888 |
| geometry | 256x240, aspect 1.219 | 352x224, aspect 1.571 (see below) |
| fps | **60.0998** (NTSC) / 50.0069 (PAL) | 60.0 |
| `sample_rate` | **48000** | 44100 |
| audio frames / video frame | 798.6 | 735 |
| ports | 4 (+ 4-player adapter, Zapper) | 2 |

Three of these matter, and one of them is a bug we would otherwise ship:

**`sample_rate` is 48000, and the door hardcoded 44100.** `SR_AUDIO_RATE` fed
both the Opus encode rate and the chunk-length arithmetic. On fceumm that would mis-pitch the audio *and* mis-size every chunk
against the FIFO cushion M4 depends on. §5 fixes it.

**`need_fullpath` is true, so no ROM-into-memory path is needed** -- but only
because we refuse an env call. fceumm declares `need_fullpath = true` in
`retro_get_system_info()` and *separately* registers a
`SET_CONTENT_INFO_OVERRIDE` (env 65) from `retro_set_environment()` offering
`need_fullpath = false` for the same extensions. `retro_env.c` returns false for
it (the `default:` arm), so the override never applies and `rc_core_load_game()`
hands the core a path, which it opens itself. **This is a decision, not an
accident: keep refusing it.** Honoring the override would oblige us to implement
`persistent_data` lifetime semantics for a memory buffer, to buy nothing.

**`.fds` needs a BIOS.** Famicom Disk System images require `disksys.rom` in the
system directory (`src/fds.c:688` fails the load without it). "NES needs no
BIOS" is true for `.nes`/`.unf`/`.unif` and false for `.fds`. So the default
whitelist is `.nes,.unf,.unif`, and `.fds` is opt-in for a sysop who supplies
`disksys.rom` -- the same legal status as `exec.bin`/`grom.bin` (§9).

**The geometry a core reports is not the console's resolution.** The NES's
256x240 is native. FreeIntv's 352x224 is not: the Intellivision's STIC draws a
160x96 active picture (the familiar 159x96 is that minus the column the
horizontal-delay register eats) inside an 8-pixel overscan border -- 176x112 --
and FreeIntv pixel-doubles it. Measured, not assumed: 240 frames into a
BIOS-backed 4-TRIS run, **0 of 19712 2x2 blocks are non-uniform**. (A game using
the STIC's H/V delay for fine scrolling could break the exact doubling; only
4-TRIS was checked.) This costs the renderer 4x the pixels for no information,
but it changes nothing -- the frame is resampled to terminal cells either way.
Do not "optimize" it by halving the frame: correctness would then depend on
every cartridge avoiding fine scrolling.

Two smaller notes. fceumm polls env 17 (`GET_VARIABLE_UPDATE`) and env 40 every
single frame; `retro_env.c`'s `default:` arm returns false and logs nothing, so
this costs nothing -- do not add a log line there. And the NTSC fps is not 60:
the pacer already reads `av.timing.fps` (`main.c:243`), so no change is needed,
but it is the first core to prove that path.

---

## 3. Architecture -- what becomes data, what stays code

**The console is CODE, not configuration.** Its identity does not vary from
install to install -- an NES is an NES on every BBS -- so it is not a sysop's
knob. Each console install is a *three-line* `lobby.js` that declares itself and
calls the shared lobby:

```
  xtrn/syncnes/lobby.js  --------- the WHOLE per-console definition
        |
        |  syncretro_lobby({ name, short, core, profile, ext, sizes, bios })
        v
  exec/load/syncretro_lobby.js  --- the lobby: picker, board, door command line
        |                             (shared: NOTHING here knows the console)
        +-- exec/load/syncretro_lib.js ---- discovery + cache + activity store
        |
        |  syncretro -core <core> -profile <pad|intv> -home ... <rom>
        v
  the C door  ---------------------- syncretro_profile.c picks the bind table
```

The C door learns its console from the **command line the lobby already builds**
(`-profile`), not from a config file it would have to keep in step. Run bare
from a shell with no `-profile`, it infers from the core's `library_name`.

`syncretro.ini` keeps only what is genuinely the sysop's: `[audio]`, `[disc]`,
and `[roms] dir=`/`exclude=`. A sysop hides a ROM; a sysop does not redefine
what an NES cartridge is.

### 3.1 The console profile (C)

New `syncretro_profile.c` / `.h`. A profile is four facts, not a vtable -- the
differences between a keypad console and a gamepad console turned out to be
small enough that function pointers would have been ceremony:

- `key` / `name` -- what `-profile` spells, and what the help screen shows.
- `analog` -- does this console read the ANALOG stick? Only the Intellivision
  does (its keypad digits are an angle, its paddle a swept disc). On every other
  console the analog device returns 0 outright, and the d-pad owns Left/Right.
- `arrow_port` -- which controller the ARROW KEYS drive. The Intellivision drives
  both hand controllers at once, so the arrows are player 2's disc (port 1).
  Everywhere else there is one player at one keyboard, so they are HIS d-pad
  (port 0). **This one is easy to miss**: the arrows were hardcoded to port 1,
  and a solo NES player pressing Up would have nudged a phantom second
  controller.
- the bind table (§4), selected in `syncretro_binds.c`.

Two profiles ship:

- **`pad`** -- a generic RetroPad. The default, and what NES uses.
- **`intv`** -- today's behavior, unchanged: keypad digits on the analog angle,
  the paddle disc sweep, Tab-swap, two hand controllers driven at once.

`syncretro_keypad.c` stops being called unconditionally from
`syncretro_input.c` and becomes the `intv` profile's hooks. Its header's own
comment ("the ONLY FreeIntv-specific policy in the door ... M3 generalizes this
into a per-core profile") is discharged by exactly this change -- the code does
not move, only its call site.

**Selection order**, resolved once at startup:

1. `-profile <name>` from the lobby, which knows which console it launched.
2. Otherwise, the core's `library_name`: `freeintv` -> `intv`; anything else ->
   `pad`.
3. Default `pad`.

**Compare the name case-insensitively.** FreeIntv reports its `library_name` as
`freeintv`, lower-case -- not the `FreeIntv` spelling used in its own repo, in
RetroArch's core list, and everywhere in our docs. PROVENANCE.md already records
the same casing trap for the `.so` filename. A case-sensitive match here fails
*silently*: the Intellivision falls through to `pad` and loses its keypad, with
no error anywhere. (The shipped syncivision install sets `profile = intv`
explicitly, so this bites only bare command-line runs -- which is to say, every
probe and test.)

**Sequencing constraint, easy to get wrong:** `sr_config_apply()` runs *before*
the core is opened, so the ini can only *record* the profile string. The profile
is resolved in `main()` after `rc_core_load_game()` (where `library_name` first
exists) and before the first `sr_input_pump()`. A profile that is still
unresolved when input arrives is a bug, not a fallback.

The upshot for console #3: ColecoVision, SMS, PC Engine and Genesis are plain
RetroPad machines. They boot and play on `pad` with **no C change at all** -- a
new profile is earned only by a console with a keypad or a paddle.

### 3.2 The install (JS)

Three files move to `exec/load/`, because they serve SEVERAL install dirs rather
than one door:

| was | now |
|---|---|
| `xtrn/syncivision/syncivision_lib.js` | `exec/load/syncretro_lib.js` -- discovery, cache, activity store |
| the body of `xtrn/syncivision/lobby.js` | `exec/load/syncretro_lobby.js` -- the picker, board and door command line |
| the body of `xtrn/syncivision/getcore.js` | `exec/load/syncretro_getcore.js` -- the buildbot downloader |

The rule this establishes, and which is worth stating out loud: **a lib serving
one door stays in the door dir; a lib serving several install dirs goes to
`exec/load/`.** `game_lobby.js` is already on the right side of that line;
`syncdoom_lib.js` and `syncduke_lib.js` correctly stay where they are.

What remains in a console's install dir is only what is *about that console*:
`lobby.js` (its spec), `getcore.js` (its core's name), `syncretro.ini` (the
sysop's knobs), `install-xtrn.ini`. `xtrn/syncivision` stays where it is;
`xtrn/syncnes` is new, and is 4 small files.

---

## 4. The binding table

`syncretro_binds.c` grows a second table. `sr_bind_lookup()` and
`sr_bind_help_line()` dispatch through the active profile, so the M2 invariant --
*one table drives both the key handler and the help screen, and they cannot
drift* -- survives intact.

The `pad` table:

| Key | RetroPad | Notes |
|---|---|---|
| `W A S D`, arrows | D-pad | both drive the active port |
| `Z` | `JOYPAD_B` (id 0) | NES *B*: the left button |
| `X` | `JOYPAD_A` (id 8) | NES *A*: the right button |
| `C` / `V` | `JOYPAD_Y` / `JOYPAD_X` | for later 4-button consoles |
| `Q` / `E` | `JOYPAD_L` / `JOYPAD_R` | for later 6-button consoles |
| Enter | `JOYPAD_START` (id 3) | |
| Bksp / Del | `JOYPAD_SELECT` (id 2) | same physical key as `intv`'s Clear |
| Tab | swap ports | NES port 2 is rare, but it is free |

Door keys are untouched: Space (pause), `?` (help), Ctrl-S/R/Q.

`Z`=B / `X`=A is not a guess. fceumm's `SET_INPUT_DESCRIPTORS` names them:
`port 0 id 0 : B`, `port 0 id 8 : A`. The physical NES layout puts B on the left
and A on the right, and `Z`/`X` sit that way on a QWERTY row.

**Why not derive the whole table from the descriptors?** Because they are a
*list of ids the core reads*, not a controller. They would have told us fceumm
reads `JOYPAD_A`; they would not have told us that FreeIntv's `JOYPAD_A` is the
Intellivision's *left* action button, and they say nothing at all about the
disc-angle keypad trick, which was reverse-engineered from `controller.c`. A
descriptor-driven help screen would be generic and wrong in exactly the cases
that cost the most to get right. Two hand-written profiles that are each correct
beat one derived profile that is merely plausible. (The descriptors *are* still
worth reading later, as a source for a per-game key hint -- see §10.)

---

## 5. Audio: the sample rate comes from the core

`SR_AUDIO_RATE` (a `#define` of 44100) becomes a runtime value read from
`core.av.timing.sample_rate`. Both consumers -- the Opus encode rate
(`syncretro_audio.c:97`) and the chunk frame count (`:127`) -- take it from
there.

This forces one sequencing change. `sr_audio_init()` runs at `main.c:139`,
*before* `rc_core_load_game()` at `:154` -- i.e. before any AV info exists. It
splits:

- `sr_audio_init()` keeps the config-only half (reads `syncretro.ini`, no I/O),
  still called at :139.
- `sr_audio_start(rate)` is new, called once after `load_game()`, and sizes the
  chunk buffer.

A zero, negative or absurd rate (outside 8000..192000) falls back to 44100 with
a logged warning -- a core that lies about its rate should be audible-but-wrong,
not silent.

Nothing else in M4 changes: the FIFO cushion is expressed in milliseconds, and
the cache-name ring (`s/0`..`s/3`, `s/z`) is rate-independent.

---

## 6. ROM discovery: the cache

**This is a live defect, not a NES-scale worry.** Discovery MD5s the *whole
file* for every candidate on every lobby entry (`syncivision_lib.js:222`). At
~200 Intellivision carts of <=64 KB that is ~12 MB and no one notices --
except over SMB, where the sysop already observes a startup delay on a remote
node reading `xtrn/syncivision`. The cost there is not the arithmetic, it is
**round trips**: one open + read + close per candidate, ~200 of them across the
wire. A full NES set is ~2,800 ROMs averaging ~256 KB, which turns a delay into
a hang.

So the cache ships **first, on its own**, against the Intellivision install --
where it can be felt and measured before any multi-core churn touches the file.

The warm path must do **zero file opens**:

1. One `directory()` glob per extension. (The file list; unavoidable.)
2. One `file_size()` / `file_date()` per candidate. A stat is a small fraction of
   an open+read+close, and it is what validates a cache entry.
3. One read of `<data_dir>/syncretro/roms.<id>.json`. **The only file opened on a
   warm run** -- 200 opens become 1.
4. Hash only candidates whose size *or* mtime does not match the cache. Steady
   state: none.

Cache entry: `{path, size, mtime, md5, title}`. The dedupe and dump-quality rules
(§6 of [LAUNCHER.md](LAUNCHER.md)) are unchanged and now run off cached hashes --
including the full-file (never 4 KB prefix) hash, for the reason recorded there.

**Concurrency.** `data_dir` is an SMB mount shared with a second host, which is
why `plays.jsonl` is append-only. The cache does **not** need that treatment: it
is *derived*, so a lost update costs a re-hash and never a wrong answer. Write
via temp-file + rename; treat any parse failure, or a missing file, as a cold
cache.

**Cold runs must say so.** A first run on a large set is genuinely slow, so the
lobby prints `Scanning ROMs (first run)...` with progress rather than a mystery
pause. Better still, the install step primes the cache once, so the first *player*
never pays for it.

---

## 7. The console spec -- `xtrn/<console>/lobby.js`

The whole of `xtrn/syncnes/lobby.js`, minus its comments:

```js
load("syncretro_lobby.js");

syncretro_lobby({
    dir:      js.exec_dir,
    name:     "Nintendo Entertainment System",
    short:    "NES",                     /* -> id "nes" */
    core:     "fceumm_libretro",
    profile:  "pad",
    ext:      ["nes", "unf", "unif"],    /* NOT .fds: it needs disksys.rom */
    min_size: 8 * 1024,
    max_size: 4 * 1024 * 1024,
    bios:     []                         /* an NES boots from the cartridge */
});
```

`name` and `short` drive the lobby header, the picker, the door's help title, and
a `console` field in `plays.jsonl` -- so one append-only log serves every console
and the board filters on it. A record with NO console field predates the field,
when the Intellivision was the only console: it IS an Intellivision play, and is
counted as one.

`id` is DERIVED from `short` (lower-cased, alphanumerics only) and names the
per-user save dir and the ROM cache. It is not a separate key, because a separate
key is a thing to get out of step. **`short` MUST stay `Intv` for the
Intellivision** (`id` = `intv`): change it and every existing player's SRAM and
save states are orphaned under `data/user/####/intv`.

The sysop's `syncretro.ini` keeps `[audio]`, `[disc]` and `[roms] dir=`/`exclude=`.
Adding `.fds` to `[roms] ext` is the documented escape hatch for a sysop who has
`disksys.rom`.

---

## 7a. Who's-online: name the cartridge

The sibling doors publish what the player is *actually doing* -- SyncDOOM's
who's-online line reads `playing SyncDOOM: DOOM2.WAD (MAP01)`, SyncDuke's carries
the E#L#. SyncRetro published nothing at all, so a player in the door showed up as
the BBS's generic "running external program". It now reads:

```
playing Astrosmash (Intellivision)
playing Super Mario Bros. (NES)
```

The lobby passes `-title` and `-console`, because it already knows both: it has
parsed `Astrosmash` out of `Astrosmash (1981) (Mattel).int` (title, year and
publisher, minus any dump marker), and it is the console's definition. The door
would otherwise have to re-implement that filename parsing in C. A door run bare
from a command line has neither, and falls back to the ROM's own filename and the
profile's name (`playing nestest (gamepad)`) -- ugly, never wrong.

Two details worth keeping:

- The console label is the LONG name when it fits a status column and the short
  one when it does not (`Intellivision`, but `NES` rather than "Nintendo
  Entertainment System"). One line in the lobby.
- `sbbs_node_init()` takes the door's **`-home`**, not `""`. The data dir comes
  from `$SBBSDATA` when the BBS sets it, but the fallback derives it by popping
  `user/####/<console>` off `-home` -- which is exactly the shape
  `sr_config_apply()` builds. Passing `""` (as the sibling doors do, relying on
  the env) makes a hand-run door silently publish nothing.

Unlike SyncDOOM and SyncDuke there is nothing to *update*: a cartridge, unlike a
map, does not change mid-session. So it is set once, after the ROM loads, and
cleared on exit.

---

## 8. Components

| File | Change |
|---|---|
| `syncretro_profile.c` / `.h` | **new** -- the profile struct, the table, selection |
| `syncretro_binds.c` / `.h` | second table (`pad`); lookup/help dispatch via profile |
| `syncretro_input.c` | keypad + disc calls go through the profile hooks |
| `syncretro_keypad.c` / `.h` | unchanged code; now reached as the `intv` hooks |
| `syncretro_audio.c` / `.h` | `SR_AUDIO_RATE` -> runtime; `sr_audio_start(rate)` |
| `syncretro_door.c` | `-profile`, `-title`, `-console`, parsed and stripped like `-core`; the who's-online status (§7a) |
| `retro_core.c` / `.h` | keep `system_info` (its `library_name` identifies the console) |
| `main.c` | resolve the profile after `load_game`; `sr_audio_start()`; publish the who's-online status; the help screen's closing paragraph follows the profile |
| `exec/load/syncretro_lib.js` | **moved+renamed** from `syncivision_lib.js`; rules as data; the cache |
| `exec/load/syncretro_lobby.js` | **new** -- the lobby itself, console-agnostic |
| `exec/load/syncretro_getcore.js` | **new** -- the core downloader, console-agnostic |
| `xtrn/syncivision/*` | `lobby.js` and `getcore.js` shrink to their console spec |
| `xtrn/syncnes/*` | **new** -- `lobby.js`, `getcore.js`, `syncretro.ini`, `install-xtrn.ini`, `.gitignore` |
| `PROVENANCE.md` | fceumm's pin + measured characteristics, as FreeIntv has |

`getcore.js` for NES fetches `fceumm_libretro.so` (libretro buildbot, or a source
build -- fceumm builds with a bare `make`, no configure) **and writes the
`[console]` block for the core it fetched**, so the sysop never hand-types the
name.

---

## 9. Error handling

- **Unknown `profile=`** -- warn to the door log, fall back to inference. Never
  fail the launch: a typo in an ini should not take the door down.
- **`.fds` with no `disksys.rom`** -- fceumm fails the load and says so. The
  lobby should not offer FDS images when the BIOS is absent, and `getcore.js`
  says why. This is the one NES BIOS trap, and it is silent otherwise.
- **Absurd `sample_rate`** -- clamp to 44100, log (§5).
- **Cold/torn/unparseable cache** -- treated as absent. Never fatal (§6).
- **A core we have never seen** -- `pad`, and it plays. That is the design.

---

## 10. Testing

C:

- `test_binds.c` gains `pad` cases, and a profile-selection test covering the
  precedence (ini beats inference) and the sequencing (unresolved profile at
  first pump = failure).
- The fceumm probe harness from §2 joins `~/tmp/syncretro_probes/`.

JS (`jsexec`, headless):

- NES discovery fixtures: extension gating, the size band, an empty `bios_md5`,
  and `.fds` excluded when `disksys.rom` is absent.
- Cache round trip: cold -> warm (zero opens) -> stale mtime -> torn file.
- The existing Intellivision fixtures must pass **unchanged** against the
  ini-driven rules. That is the regression gate for "the Intellivision install
  does not move."

Live:

- The cache, on VERT, against the existing Intellivision install -- the delay is
  the thing being fixed, so it is the thing to measure. Before/after, from a
  remote node.
- NES: boot a ROM, hear audio at 48 kHz, play with the `pad` bindings.

**No NES test ROM is committed.** `4-tris.rom` is committed for Intellivision,
but the NES tests do not get an equivalent: the ROM is fetched locally by whoever
runs them (`nestest.nes` is freely circulated, but not under a license that
settles redistribution). A test that cannot find its ROM **skips, and says it
skipped** -- it must not silently pass, and it must not fail a clean checkout.

---

## 11. Order of work -- and what was verified

1. **The discovery cache**, alone, on the Intellivision install. DONE, and
   measured on the live 198-cartridge set: **212 file opens -> 0**, 62 ms -> 13 ms
   locally (where an open is cheap; the point is the round trips a remote SMB node
   pays). (§6)
2. The lib move to `exec/load/`, console rules as data. DONE; the Intellivision
   discovery fixtures pass **unchanged**, which is the regression gate.
3. The C profile split. DONE: `pad` + `intv`, `intv` untouched. `test_binds`
   covers both tables and every selection path.
4. The audio sample rate. DONE, and demonstrated: the door logs
   `audio 48000 Hz, 100 ms chunks (4800 frames)` on fceumm and
   `audio 44100 Hz ... (4410 frames)` on FreeIntv.
5. `xtrn/syncnes`. DONE: `getcore.js` fetches fceumm from the libretro buildbot,
   and the door renders `nestest.nes` to sixel (its menu decodes cleanly out of a
   `SYNCRETRO_SIXELOUT` capture -- the M1 technique).

**Not yet done: a live play over a real terminal.** Everything above is headless.
The NES install has no cartridges (the sysop supplies them) and no `xtrn.ini`
entry yet.

---

## 12. Deferred, with reasons

- **Core options + save states** -- M5. Note that fceumm's sample rate is a *core
  option*: changing it mid-session would fire `SET_SYSTEM_AV_INFO`, which we
  refuse. Harmless while no core options are surfaced; M5 must handle it.
- **Turbo A/B, A+B, FDS disk-swap** -- real ids in fceumm's descriptors, and each
  wants a key. Not until someone asks.
- **The Zapper and the 4-player adapter** -- a light gun needs a pointer device;
  the same blocker as the deferred controller overlays.
- **A per-game key hint from the descriptors** -- the descriptors cannot found a
  bind table (§4), but they *can* label one. Combined with `startprobe.c`'s
  "which key starts this cartridge" sweep, that is the honest path to a per-game
  legend. Later.
- **A descriptor-derived help screen** -- rejected, §4.
