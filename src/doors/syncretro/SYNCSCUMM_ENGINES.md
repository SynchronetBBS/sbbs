# `syncscumm` -- the ScummVM engine tour, as a curation guide

> **The console this was written for was superseded** -- ScummVM ships as the
> native [`../syncscumm`](../syncscumm/README.md) door, not as a SyncRetro
> console ([SYNCSCUMM.md](SYNCSCUMM.md) banner). **The engine tour below is
> unaffected and still current:** which engine families clear the terminal bar is
> a property of ScummVM's catalog, not of how we host it, so this remains the
> curation guide for the door. Only the `scummvm_libretro`-specific framing
> (§"one core") is stale.

Companion to [SYNCSCUMM.md](SYNCSCUMM.md). ScummVM is **100+ engines / 325+ named
games** (2025), and via AGS/Wintermute/Director effectively thousands more. This
doc is not an inventory -- the ScummVM wiki is -- it is a **fit guide**: which
engine families clear the terminal bar (320x200 -> the 640x400 canvas, gentle
input) and which do not.

**The list is not the source of truth; the probe is.** Per-game geometry
([SYNCSCUMM.md](SYNCSCUMM.md) §3) is read at load time, and a discovery filter
keeps SVGA/3D titles out of the picker by that number, not by this doc. Resolutions
below are stated at engine-family granularity and a specific title can differ --
so the buckets guide curation, they do not adjudicate a single game.

---

## The fit band -- 2D, ~320x200, point-and-click or parser

These double cleanly to the canvas and their pace/hotspots suit a terminal. This
is where `syncscumm` lives.

| Engine | Publisher | Representative games | Note |
|---|---|---|---|
| **SCUMM** | LucasArts | Maniac Mansion, Zak McKracken, Loom, Monkey Island 1-2, Indy 3/4, Day of the Tentacle, Sam & Max, Full Throttle, The Dig | the founding engine; also Humongous kids' games (Putt-Putt, Freddi Fish) |
| **AGI** | Sierra (early) | King's Quest 1-3, Space Quest 1-2, LSL 1, Police Quest 1, Manhunter | 16-color parser era; tiny + fits |
| **SCI (SCI0/1/1.1)** | Sierra | King's Quest 4-6 (DOS), Space Quest 3-5, Quest for Glory 1-4, Gabriel Knight 1, Freddy Pharkas | the 320x200 SCI split -- see the miss band for SCI32 |
| **Sky** | Revolution | **Beneath a Steel Sky** (freeware) | the lead showcase ([SYNCSCUMM.md](SYNCSCUMM.md) §4) |
| **Kyra** | Westwood | Legend of Kyrandia 1-3, Hand of Fate, Malcolm's Revenge, Lands of Lore 1 | |
| **AGOS** | Adventure Soft | Simon the Sorcerer 1-2, Elvira 1-2, Waxworks | Feeble Files is higher-res (miss) |
| **Gob** | Coktel Vision | Gobliiins 1-3, Ween, Woodruff, Bargon Attack | |
| **Cine / Cruise** | Delphine | Future Wars, Operation Stealth / Cruise for a Corpse | |
| **Queen** | -- | **Flight of the Amazon Queen** (freeware) | |
| **Lure** | Revolution | **Lure of the Temptress** (freeware) | |
| **Drascula** | -- | **Drascula** (freeware) | |
| **Saga** | -- | Inherit the Earth, I Have No Mouth and I Must Scream | |
| **Sherlock** | Sierra | The Case of the Serrated Scalpel | Rose Tattoo is 640x480 (miss) |
| **Tinsel / MADE / Parallaction / Toltecs / Chewy / Supernova / Prince** | various | Discworld 1, Return to Zork, Nippon Safes (freeware), 3 Skulls of the Toltecs | the long 2D tail; most fit, check per title |
| **Xeen / Griffon** | -- | Might & Magic 4-5 (World of Xeen), **The Griffon Legend** (freeware) | non-adventure but 320x200 and turn/slow -- fit |

The freeware subset (bold) is the shippable showcase: it needs nothing from the
sysop.

---

## The miss band -- SVGA, FMV, or 3D

Right catalog, wrong tier for a terminal -- curated out, not supported-and-ugly.

- **SCI32** (Sierra SCI2/2.1/3) -- King's Quest 7, Phantasmagoria, Gabriel Knight
  2, Torin's Passage, Shivers. **640x480**, the MOO2 wall. This is why "Sierra"
  splits: AGI/SCI0-1 fit, SCI32 does not.
- **Groovie** -- The 7th Guest, The 11th Hour. FMV-driven, higher res.
- **Sword1 / Sword2** -- Broken Sword 1-2. 640x480 + FMV. (Sword25 / Broken Sword
  2.5, a freeware fan game, is the exception worth checking.)
- **Mohawk** -- Myst, Riven, Myst III, the Living Books CD-ROMs. Panoramic/high-res.
- **ZVision** -- Zork Nemesis, Zork Grand Inquisitor. 640x480 panoramic.
- **Bladerunner** (Westwood) -- 640x480 with pseudo-3D actors.
- **Cryomni3d / Pegasus / Nancy / Buried / Trecision / mTropolis / Director** --
  90s CD-ROM multimedia and panoramic adventures; mostly 640x480+, some their own
  renderer.
- **GrimE / Stark** -- Grim Fandango, Escape from Monkey Island / The Longest
  Journey. **True 3D** (the old ResidualVM merge); a different renderer entirely,
  out of scope.
- **Freescape** -- Driller, Castle Master. Early 3D vector; niche, and not the
  point-and-click model.

---

## The wildcard -- AGS (Adventure Game Studio)

ScummVM absorbed the **AGS** engine, which alone represents *thousands* of games
-- commercial (Blackwell series, Gemini Rue, Primordia, Technobabylon, Unavowed,
Kathy Rain) and a vast freeware scene. Resolution varies wildly (many are
320x200/320x240 and fit; plenty are 640x480+ and miss), so AGS is exactly the case
the **geometry filter, not this list**, must adjudicate -- there are too many
titles, changing too often, to bucket by hand. Some AGS games are also freeware and
could widen the shippable showcase, but each needs its own license check before
bundling.

---

## The one-core payoff, restated

Every engine above is reached through the **same** `scummvm_libretro.so` -- one
core, one console, the whole 2D-adventure genre. That is the structural reason
`syncscumm` earns its place in tier 2 above `syncdos`: dosbox would host each of
these as a separate DOS binary; ScummVM hosts the lot, and runs the Amiga or
FM-Towns cut just as well as the DOS one.
