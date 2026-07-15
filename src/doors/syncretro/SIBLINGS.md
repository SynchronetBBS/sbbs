# SyncRetro -- candidate cores: what console to add next

Status: **survey, not a plan.** A map of libretro cores that could each become a
SyncRetro console for a *big* catalog win -- the ScummVM pattern generalized. It
commits to nothing; it exists so "what do we add next?" has a reasoned answer
instead of a guess. Companion to [SYNCDOS.md](SYNCDOS.md) and
[SYNCSCUMM.md](SYNCSCUMM.md); the fit criterion and the three-tier rule live in
[SYNCDOS.md](SYNCDOS.md) §1a and are assumed here.

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
   Chocolate Duke, 1oom, the DOOM door) is never hosted through a core.

A *big win* is a single core that clears all three for a **whole catalog**, ideally
one that is free to redistribute.

---

## Kind A -- reimplementation frameworks (bring the game data)

One core reimplements an engine/runtime; you supply the original games' data. The
ScummVM pattern. Cleanest tier-2 wins.

| Core | Unlocks | Fit | Notes |
|---|---|---|---|
| **ScummVM** | the 2D graphical-adventure catalog (100+ engines) | ideal: 320x200, point-and-click, slow | **sketched** -- [SYNCSCUMM.md](SYNCSCUMM.md). Ships freeware showcase games. |
| **EasyRPG** | RPG Maker 2000/2003 + EasyRPG games | ideal: 320x240, turn-based, keyboard | **The RPG twin of ScummVM.** Huge freeware catalog -- Japanese doujin RPGs, the horror canon (Ib, The Witch's House, Yume Nikki, OFF), mostly free. Content: `.ldb`/`.zip`/`.easyrpg`. Wrinkle: the **RTP** default-asset package is Enterbrain's and redistribution-restricted -- RTP-dependent games need it sysop-supplied (BIOS-like); self-contained games don't. Needs the *keyboard* half of the input bridge. |

AGS (Adventure Game Studio) is a huge catalog too, but it is reached **through
ScummVM's** AGS engine -- so it is already inside the ScummVM win, not a separate
console.

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

## Why this compounds -- one bridge, four universes

The expensive, shared frontend work is the **mouse/keyboard device bridge**
([SYNCDOS.md](SYNCDOS.md) §5) plus the M3 profile/discovery machinery. Once it
exists, each console below is just *a new dir + a `getcore.js` + a profile*. The
bridge amortizes across all of them:

- **TIC-80** -- gamepad-native; needs (almost) none of it.
- **EasyRPG** -- needs the *keyboard* half.
- **ScummVM** -- needs the *mouse* half.
- **`syncdos`** -- needs both.

So one piece of C opens four content universes.

---

## Ranking -- if we add one next

By catalog size x fit x low effort:

1. **TIC-80** -- huge free catalog, ideal fit, *lowest* effort (gamepad-native,
   arguably no bridge needed). The cheapest big win.
2. **EasyRPG** -- massive RPG catalog, ideal fit; costs the keyboard bridge and
   the RTP-asset handling.
3. **ScummVM** -- already sketched ([SYNCSCUMM.md](SYNCSCUMM.md)); costs the mouse
   bridge, ships its own free showcase.

`syncdos` (dosbox_pure) is the general fallback, not a "big win" -- it hosts the
DOS long tail one binary at a time, and every game is sysop-supplied
([SYNCDOS.md](SYNCDOS.md)).

---

## The rule this survey encodes

**Prefer a core that reimplements a whole engine/platform over one that emulates a
machine, and prefer a platform whose catalog is free to ship.** That ordering --
ScummVM/EasyRPG over dosbox, TIC-80 over PICO-8 -- is what turns "add a console"
from an open-ended catalog problem into a short, ranked list.
