# SyncRetro

Play **legacy game consoles** (Intellivision, NES, ColecoVision, ...) as a
Synchronet / SyncTERM **door**, by hosting a [libretro](https://www.libretro.com/)
core and rendering it to the terminal through the shared
[`../termgfx`](../termgfx) library -- the same graphics/audio/pacing stack as
`../syncdoom`, `../syncduke`, `../syncmoo1`.

**One door, many consoles:** SyncRetro is a minimal libretro *frontend*. It
vendors no emulator -- you point it at a libretro core `.so` and it drives the
core's run loop, converting each video frame to sixel/JXL and mapping BBS input
to a RetroPad. Swap the core, get a different console.

> **Status: M1 (video-only vertical slice) complete.** The frontend loads a core,
> loads a ROM, renders frames as sixel, and maps terminal keys onto a RetroPad,
> with correct enter/probe/leave and teardown. Audio is accepted and discarded.
> See DESIGN.md sec 15 for the remaining milestones.
>
> To actually *play* Intellivision you still need the BIOS (below). Without it
> FreeIntv loads the cartridge, halts its CPU, and draws its own "PUT GROM/EXEC IN
> SYSTEM DIRECTORY" screen -- which the door renders correctly, and which is a
> useful way to confirm your install before you have the BIOS.

## What you supply

1. **`libretro.h`** -- the API header, vendored into this dir before building
   (see [PROVENANCE.md](PROVENANCE.md)).
2. **A libretro core `.so`** -- e.g. `FreeIntv_libretro.so` (Intellivision). GPL /
   freely redistributable.
3. **Console BIOS + game ROMs** -- **commercial, copyrighted content SyncRetro
   does not ship.** A legally-owned copy is yours to supply (same as MoO1's data
   in `../syncmoo1`).

## Building

```sh
./build.sh            # Release -> build/syncretro
./build.sh debug
./build.sh clean
```

Links `../termgfx` + `xpdev`; `dlopen`s the core at runtime. `libjxl` is
optional (absence degrades to sixel/text). `./deploy.sh` installs the binary
into the door's `xtrn` dir (a no-op on a symlink install).

## Installing

The door's own directory is the install root. A typical Intellivision install
(`xtrn/syncivision`) looks like:

```
xtrn/syncivision/
    syncretro                 the door binary
    freeintv_libretro.so      the core
    exec.bin  grom.bin        the BIOS you supply (case-sensitive names)
    roms/                     cartridges
        4-tris.rom
```

The BIOS is found in the door's own directory (its start-up directory in SCFG),
and cartridges are looked up in `roms/` -- so a bare ROM name is enough. Override
the BIOS location with `$SYNCRETRO_SYSTEM` if you keep it elsewhere.

## Launching

```
syncretro <path>/door32.sys -name "<user>" -home <dir> -core freeintv_libretro.so 4-tris.rom
```

DOOR32.SYS supplies the socket + time left; the door probes terminal geometry at
connect. `-home` is the per-user sandbox that SRAM and save states land in. Run
`syncretro -help` for the full option list.

### Controls

Arrows or WASD are the d-pad; `Space`/`Z` = A, `X` = B, `C` = X, `V` = Y, `Q`/`E`
= L/R, `Enter` = Start, `Tab` = Select, `Ctrl-Q` quits.

How well *holding* a direction works depends on your terminal, because a plain
terminal never reports a key being released:

| Terminal | Key-up | Feel |
|---|---|---|
| SyncTERM | physical key reports (CTDA cap 8) | exact; layout-independent (WASD works on AZERTY) |
| kitty, foot, ... | kitty keyboard protocol | exact |
| anything else | none -- a short auto-release timer | a held direction stutters once before it flows |

The door negotiates the best available path at connect and restores the
terminal's key mode on exit.

## Constraints

Software-rendered cores only (no GL/Vulkan -- correct for a terminal). Audio is
deferred past M1 (streaming console PCM to a terminal is an open problem, see
DESIGN.md sec 8). Legacy 2D consoles are the sweet spot -- their tiny, low-frame
-rate output suits terminal rendering well.

Frames are truecolor, and sixel is a 256-color format, so each frame is quantized
([`syncretro_quant.c`](syncretro_quant.c)). Legacy consoles use far fewer than
256 colors, so the reduction is normally **exact** and the palette is stable
enough that the color registers are sent once.

## License

Frontend glue: GPL-2.0 (matching the door family). libretro API: permissive.
Cores: their own licenses (FreeIntv GPLv2+). BIOS/ROMs: their rights holders'.
