# SyncRetro -- candidate cores: what console to add next

Status: **survey, not a plan.** A map of libretro cores that could each become a
SyncRetro console for a *big* catalog win -- the ScummVM pattern generalized. It
commits to nothing; it exists so "what do we add next?" has a reasoned answer
instead of a guess. Companion to [SYNCDOS.md](SYNCDOS.md) and
[SYNCSCUMM.md](SYNCSCUMM.md); the fit criterion and the three-tier rule live in
[SYNCDOS.md](SYNCDOS.md) §1a and are assumed here.

**Revised since first written:** the two Kind A candidates (ScummVM, EasyRPG)
were built as *native* doors rather than SyncRetro consoles, and are struck
below -- see [Kind A](#kind-a----reimplementation-frameworks-bring-the-game-data).

Every core below is a **hypothesis to probe** (the M3 discipline): its content
model, device support, and per-game geometry decide whether it fits, and none of
that is trusted until measured against the real `.so`.

---

## The bar a candidate has to clear

From [SYNCDOS.md](SYNCDOS.md) §1a, unchanged:

1. **Resolution** -- roughly 320x200 (doubles onto SyncTERM's 640x400 canvas).
   SVGA/3D overshoots and loses small text.
2. **Interaction** -- turn-based or slow real-time, generous targets. Twitch
   action fails even at low resolution.
3. **Tier** -- a game with a **native reimplementation door** (Vanilla Conquer,
   Chocolate Duke, 1oom, the DOOM door, and now ScummVM and EasyRPG) is never
   hosted through a core.

A *big win* is a single core that clears all three for a **whole catalog**, ideally
one that is free to redistribute.

---

## Kind A -- reimplementation frameworks (bring the game data)

One core reimplements an engine/runtime; you supply the original games' data. The
ScummVM pattern -- on paper the cleanest tier-2 win. In practice **both**
candidates resolved *past* tier 2 into tier 1: each framework was embedded
natively in its own door instead of being hosted through a core.

| Framework | Unlocks | Status | Notes |
|---|---|---|---|
| **ScummVM** | the 2D graphical-adventure catalog (100+ engines) | **superseded -- native door** | Built as [`../syncscumm`](../syncscumm/README.md), embedding ScummVM directly. Not a SyncRetro console: the tier rule forbids hosting through `scummvm_libretro` a catalog that already has a native door. The earlier core sketch, [SYNCSCUMM.md](SYNCSCUMM.md), is retained but superseded. |
| **EasyRPG** | RPG Maker 2000/2003 + EasyRPG games | **superseded -- native door** | Built as [`../syncrpg`](../syncrpg/README.md), embedding the EasyRPG Player directly. Same reason. The huge freeware catalog (Japanese doujin RPGs, the horror canon -- Ib, The Witch's House, Yume Nikki, OFF) and the **RTP** wrinkle (Enterbrain's default-asset package is redistribution-restricted, so RTP-dependent games need it sysop-supplied, BIOS-like) are both real -- but they are that door's concerns now, not core-hosting questions. |

AGS (Adventure Game Studio) is a huge catalog too, but it is reached **through
ScummVM's** AGS engine -- so it is already inside the `syncscumm` win, not a
separate console.

**What this leaves Kind A: nothing, today.** The category stays because the
*criterion* is still right -- another whole-engine framework with a free catalog
would belong here. But record the lesson these two taught: when a candidate is a
reimplementation **framework** rather than an emulator, it is usually also a good
candidate to embed **natively** as its own door, which is strictly better than
hosting it through a core. Check that before sketching a console.

---

## Kind B -- open fantasy consoles (the content is natively free)

One core runs an open platform whose entire catalog is freely shareable. Better
than Kind A in one way: **nothing to bring** -- a door could ship a whole library
with no licensing friction.

| Core | Unlocks | Fit | Notes |
|---|---|---|---|
| **TIC-80** | the open TIC-80 cartridge catalog | ideal AND lowest-effort | Open source; 240x136 / 16 colors; `.tic` carts, a large free community catalog. **Gamepad-native at tiny resolution** -- the closest of any candidate to the NES/Intellivision consoles we already ship, so it could nearly drop onto the existing `pad` profile with *no* mouse/keyboard bridge. Curate: some carts are twitch. |
| **LowRes NX** | its fantasy-console catalog | good, small | Open fantasy console, libretro core, free content. Smaller catalog than TIC-80. |
| **Uzebox** | open AVR game-console homebrew | good, small | Open hardware console; homebrew catalog, free. Small. |
| **Lutro** | Lua-framework games | niche | A libretro Lua game framework; modest catalog. |

**PICO-8 is the trap, not a win.** The famous fantasy console, but **commercial and
closed**: no official free core, carts not uniformly redistributable. Despite the
huge scene it is not a clean redistributable-catalog win the way TIC-80 is. Named
here so it is not reached for first.

---

## Free catalog, wrong genre -- do not be tempted

Big *free* catalogs that fail the **interaction** axis, not the resolution one:

- **OpenBOR** -- thousands of free community beat-'em-ups. Pure twitch action;
  wrong genre for a latency-quantized terminal.
- **NXEngine (Cave Story)** -- one genuinely great *freeware* game, but an action
  platformer (marginal on twitch), and a single-game core, not a catalog.

Recorded so the size of their catalogs does not read as "big win."

---

## What is already free, and the ONE piece of C that widens it

**Start from what SyncRetro already is.** The "one binary, many consoles"
property is not a future win -- it shipped in M3. The frontend hosts any libretro
core, and any **gamepad-driven** core is a new console with *zero new C*: a dir,
a `getcore.js`, and the existing `pad` profile. ColecoVision, SMS, Genesis and
PC-Engine already work that way, and **TIC-80 drops straight into that same free
bucket** (it is gamepad-native) -- which is exactly why it is the cheapest win
below.

The *one* limit on that free property is that the frontend today speaks only
**RetroPad**. Every mouse/keyboard-driven core is fenced off until one addition
exists -- the **mouse/keyboard device bridge** ([SYNCDOS.md](SYNCDOS.md) §5),
alongside the M3 profile/discovery machinery. That bridge does not *create* the
many-consoles property; it **extends the property you already have to the
mouse/keyboard class**, after which each of those cores is also just *a dir + a
`getcore.js` + a profile*:

- **TIC-80** -- gamepad-native; **already free today**, needs none of the bridge.
- **`syncdos`** -- needs both halves.

ScummVM and EasyRPG used to sit on that list, needing the mouse half and the
keyboard half respectively. Both became native doors instead, so neither is a
core to bridge to any more.

So the honest tally: the bridge is one piece of C, and today it opens **one**
universe -- the DOS long tail via `syncdos`. It was drafted when ScummVM and
EasyRPG were also on the far side of it, which made a much stronger case than
the one that actually remains. Judge it on `syncdos` alone, or on the next
mouse/keyboard core that clears the bar in §1a.

---

## Ranking -- if we add one next

By catalog size x fit x low effort:

1. **TIC-80** -- huge free catalog, ideal fit, *lowest* effort (gamepad-native,
   arguably no bridge needed). The cheapest big win, and now the only big one
   this survey still holds.
2. **LowRes NX** / **Uzebox** -- the same free-catalog shape as TIC-80 at a
   fraction of the size. Cheap, but small.

EasyRPG and ScummVM used to be #2 and #3. Both were built as native doors
instead ([`../syncrpg`](../syncrpg/README.md),
[`../syncscumm`](../syncscumm/README.md)) -- the tier rule working as intended,
so they leave this list rather than top it.

`syncdos` (dosbox_pure) is the general fallback, not a "big win" -- it hosts the
DOS long tail one binary at a time, and every game is sysop-supplied
([SYNCDOS.md](SYNCDOS.md)).

---

## The rule this survey encodes

**Prefer a core that reimplements a whole engine/platform over one that emulates a
machine, and prefer a platform whose catalog is free to ship.** That ordering --
ScummVM/EasyRPG over dosbox, TIC-80 over PICO-8 -- is what turns "add a console"
from an open-ended catalog problem into a short, ranked list.

**And the corollary its own top two candidates added:** when a reimplementation
is good enough to prefer, check whether it can be **embedded natively** as its
own door before sketching it as a console. Both framework candidates here could
be, and were -- one tier better than anything this survey could have added. A
survey that ranks cores has to be willing to conclude "not a core at all."
