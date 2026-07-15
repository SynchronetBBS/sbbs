# SyncRetro -- `syncscumm` sketch: adventures via the ScummVM core

Status: **sketch. Nothing here is built.** Companion to
[SYNCDOS.md](SYNCDOS.md); read that first for the shared machinery (the console
model, the probe-first discipline, and especially the mouse/keyboard input
bridge, which `syncscumm` *shares* rather than duplicates). Where this doc
asserts a `scummvm_libretro` characteristic, treat it as a hypothesis to verify
against the real core (§3), the same way [M3_MULTICORE.md](M3_MULTICORE.md) §2
measured fceumm.

`syncscumm` is a third SyncRetro console -- same one binary, a different core --
for the classic graphical-adventure catalog, hosted by **ScummVM**
(`scummvm_libretro`) rather than dosbox_pure.

---

## 1. Where this sits -- the three-tier rule

SyncRetro content has a preference order, and `syncscumm` exists to occupy the
middle of it:

1. **Native reimplementation door** (best, where one exists and is mature):
   Vanilla Conquer -> C&C / Red Alert (`syncconquer` / `syncalert`), Chocolate
   Duke -> Duke3D (`syncduke`), 1oom -> MOO1 (`syncmoo1`), plus the DOOM door.
   Clean input/render/audio, no emulation overhead. A game with a native door is
   **never** hosted through a core.
2. **Purpose-fit libretro core** -- ScummVM for adventures. A reimplementation
   *framework*, not an emulator (§2), so it keeps most of tier 1's cleanliness
   while covering a whole genre through one core.
3. **dosbox_pure** (`syncdos`) -- the DOS long tail with neither of the above
   ([SYNCDOS.md](SYNCDOS.md)).

The line between tiers 2 and 3 is the point of this doc: the Sierra and LucasArts
adventures that landed in the `syncdos` "good-fit" list actually belong **here**,
because ScummVM reimplements their engines and dosbox would only emulate the DOS
around them.

---

## 2. What ScummVM is -- a framework of engine reimplementations

Not an emulator. Classic adventures shipped their logic as **scripts + assets**
run by an interpreter (SCUMM bytecode, Sierra's SCI/AGI p-machine, ...).
ScummVM **reimplements those interpreters** clean-room and runs the original
game's data files directly -- no CPU or DOS emulation. Architecturally it is a
common platform/backend layer (graphics, audio, input, save, GUI) plus a large
set of **engine plugins**, each reimplementing one original runtime. As of 2025
that is **100+ engines and 325+ named games** (and, via the Adventure Game Studio,
Wintermute and Director engines, effectively thousands more indie/CD-ROM titles).

The libretro core wraps the whole framework, so **one core gives every engine**.
That is the structural win over `syncdos`: dosbox hosts one DOS binary per game;
`scummvm_libretro` hosts an entire genre. And because it runs *data*, it can run
the Amiga or FM-Towns cut of a game as readily as the DOS one.

See [SYNCSCUMM_ENGINES.md](SYNCSCUMM_ENGINES.md) for the engine tour and which
ones clear the terminal fit bar (§6).

---

## 3. The probe -- task zero, gates everything

Build the M3-style probe against the real `scummvm_libretro.so` and record:

| Fact | Gates |
|---|---|
| `library_name`, content model (the **`.scummvm` hook file**, §4) | the console spec + launch |
| the **`scummvm.zip` datafile bundle** it needs (core system files) | install / `getcore.js` |
| **which input devices it reads** -- `RETRO_DEVICE_MOUSE` / `POINTER` / `KEYBOARD` | the shared input bridge ([SYNCDOS.md](SYNCDOS.md) §5) |
| **per-game geometry** -- it is NOT one resolution (§6) | the render fit / curation |
| `sample_rate`, fps | the audio-rate path M3 already generalized |
| game-compatibility gaps vs standalone ScummVM | which games we bless |

Until the probe answers the device and content rows, §4-§6 are proposals.

---

## 4. Content -- `.scummvm` hook files, and a free showcase

ScummVM content is a **game directory** (the data files), addressed by a small
**`.scummvm` hook file** the core loads as its playlist target -- this is also how
a specific game launches with **no launcher menu**, the clean equivalent of
`syncdos`'s bundled `[autoexec]`. So discovery keys on the `.scummvm` descriptor +
its data folder, not on a single cartridge-shaped file: the same shape problem
`syncdos` §4 describes, solved by the descriptor.

**The door advantage tier 3 cannot match: a legally redistributable showcase.**
Every `syncdos` game is sysop-supplied commercial content. ScummVM **officially
distributes 11 freeware games** -- Beneath a Steel Sky, Flight of the Amazon
Queen, Lure of the Temptress, Dreamweb, Drascula, Soltys, Sfinx, The Griffon
Legend, Nippon Safes Inc., Mystery House, Broken Sword 2.5. So `syncscumm` can
ship a game a player tries **immediately, with nothing supplied by the sysop**.
Lead with **Beneath a Steel Sky**: 320x200 cyberpunk point-and-click, Dave Gibbons
(*Watchmen*) art, Revolution/Charles Cecil -- the exact opposite of MOO2's "own a
commercial SVGA game or see nothing." Commercial adventures remain sysop-supplied,
same legal footing as the Intellivision BIOS.

---

## 5. Input -- the SAME bridge as `syncdos`, and its best case

ScummVM is mouse-and-keyboard point-and-click, so it needs the very
`RETRO_DEVICE_MOUSE` / `KEYBOARD` frontend bridge `syncdos` needs
([SYNCDOS.md](SYNCDOS.md) §5). **This is not duplicated work** -- the bridge is
written once in `retro_bridge.c` / `syncretro_input.c` and both consoles inherit
it. A `scummvm` profile selects its bind table; the device plumbing is shared.

And this is the bridge's **best case**, the opposite end from MOO2: 320x200,
slow-paced, generous click hotspots, zero timing pressure. Cell-quantized mouse
and per-frame encode latency -- fatal to a flight sim, awkward for MOO2's dense
SVGA UI -- are entirely tolerable pointing at a big inventory icon in a room that
is not going anywhere.

---

## 6. Rendering -- pixel tier, and curate by native resolution

These are graphical bitmap adventures, so it is the pixel tier; the text tier
([SYNCDOS.md](SYNCDOS.md) §6) does not apply. The catch is that ScummVM's catalog
is **not one resolution**:

- **320x200 (the classics)** -- SCUMM, Sierra AGI/SCI0-1, Sky, Kyra, Gob, AGOS,
  Sherlock, most of the freeware set. These double to the 640x400 canvas exactly
  -- the C&C sweet spot -- and are the fit.
- **640x480 SVGA / FMV** -- Sierra SCI32, Broken Sword's FMV, Director CD-ROMs,
  Mohawk/Myst. Overshoot the canvas and lose small text, the MOO2 problem.
- **3D** (GrimE: Grim Fandango, Escape from Monkey Island; Stark; Myst III) --
  a different renderer entirely; out of scope.

So the console **curates to the 320x200 engines**. The probe's per-game geometry
row (§3) is what a discovery filter would read to keep an SVGA/3D title out of the
picker, rather than a hand-maintained blocklist.

---

## 7. Order of work (when/if built)

1. **The probe (§3).** Its device + content answers decide §4-§6.
2. **The shared input bridge** -- build it once for whichever of `syncdos` /
   `syncscumm` is done first; the other inherits it. Adventures are the gentler
   target to prove it against.
3. **Content + install** -- the `.scummvm` descriptor discovery, the `scummvm.zip`
   datafile bundle in `getcore.js`, and the bundled freeware showcase.
4. **Resolution curation** -- the 320x200 filter (§6).

---

## 8. Deferred / rejected, with reasons

- **Hosting any of these adventures through `syncdos`/dosbox** -- rejected;
  tier 2 beats tier 3 for a reimplemented engine (§1), the same rule that keeps
  Red Alert out of a `syncdos` list.
- **SVGA / FMV / 3D ScummVM games** -- out of the fit band (§6); curated out, not
  supported-and-ugly.
- **A separate mouse bridge for `syncscumm`** -- rejected; it is the one
  `syncdos` builds (§5).
- **RetroArch's scanner for content discovery** -- the core's own docs warn against
  it (third-party DB, not ScummVM's detection). Use `.scummvm` hook files (§4).
