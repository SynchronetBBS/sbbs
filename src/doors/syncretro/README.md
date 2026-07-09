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

> **Status: design + skeleton.** This directory currently holds the design
> ([DESIGN.md](DESIGN.md)) and a compileable-once-`libretro.h`-is-vendored code
> skeleton (honest `TODO(M1)` stubs). It does not play anything yet. See
> DESIGN.md sec 15 for the milestones; M1 is a video-only Intellivision slice.

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

## Launching (once M1 lands)

```
syncretro <path>/door32.sys -name "<user>" -home <dir> -core <core.so> <rom>
```

DOOR32.SYS supplies the socket + time left; the door probes terminal geometry
at connect. See DESIGN.md sec 11 for the full option list.

## Constraints

Software-rendered cores only (no GL/Vulkan -- correct for a terminal). Audio is
deferred past M1 (streaming console PCM to a terminal is an open problem, see
DESIGN.md sec 8). Legacy 2D consoles are the sweet spot -- their tiny, low-frame
-rate output suits terminal rendering well.

## License

Frontend glue: GPL-2.0 (matching the door family). libretro API: permissive.
Cores: their own licenses (FreeIntv GPLv2+). BIOS/ROMs: their rights holders'.
