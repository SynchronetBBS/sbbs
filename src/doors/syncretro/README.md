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

> **Status: M1 (video), M2 (input) and M4 (audio) complete.** The frontend loads
> a core, loads a ROM, renders frames as sixel, maps terminal keys onto a
> RetroPad -- including all twelve Intellivision keypad keys -- and streams the
> core's audio to SyncTERM as Opus chunks. Enter/probe/leave and teardown are
> correct on every exit path. See DESIGN.md sec 15 for the remaining milestones.
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

See [Keys](#keys) below for the full key list.

How well *holding* a direction works depends on your terminal, because a plain
terminal never reports a key being released:

| Terminal | Key-up | Feel |
|---|---|---|
| SyncTERM | physical key reports (CTDA cap 8) | exact; layout-independent (WASD works on AZERTY) |
| kitty, foot, ... | kitty keyboard protocol | exact |
| anything else | none -- a short auto-release timer | a held direction stutters once before it flows |

The door negotiates the best available path at connect and restores the
terminal's key mode on exit.

## Keys

| Key | Action |
|---|---|
| `W A S D` / arrows | disc (16-way) |
| `Z X C` | action buttons |
| `1 - 9, 0` | keypad digits |
| `Backspace` | keypad Clear |
| `Enter` | keypad Enter |
| `Tab` | swap left/right controller |
| `Space` | pause / resume |
| `?` | this help |
| `Ctrl-R` | reset the console |
| `Ctrl-Q` | quit |

Press `?` in-game to see this same list, drawn as the door's own help screen.

## Audio

Requires a SyncTERM build with **libsndfile**; without it the door is silent by
design and sends no audio APCs at all, not even the capability query --
`[audio] enabled = false` behaves the same way. On a capable terminal the
core's audio streams as 100 ms mono Opus chunks (roughly 1.2-2.0 KB each, about
20 KB/s sustained) over one of SyncTERM's audio-mixer channels, with a short
prebuffer cushion against link jitter and silence replayed from a single
cached sample at essentially no bandwidth cost. A game that is briefly silent
on its own -- Astrosmash and BurgerTime both do this on their title screens --
sends no audio traffic until it actually makes a sound; that is expected, not
a stuck stream. On exit the door logs a summary line:

```
syncretro: audio N chunks, N underrun(s), N drop(s)
```

Tune it in `syncretro.ini`'s `[audio]` section (see
[M4_AUDIO.md](M4_AUDIO.md) sec 5.1 for the reasoning behind each default):

| Key | Default | Clamp | Meaning |
|---|---|---|---|
| `enabled` | `true` | -- | `false` emits no audio APCs at all (not the same as `volume = 0`) |
| `quality` | `0.15` | 0.01 .. 1.0, else default | Opus VBR quality; out-of-range (including NaN) falls back to the default rather than clamping |
| `volume` | `100` | 0 .. 100 | channel base volume |
| `chunk_ms` | `100` | 50 .. 250 | chunk size; below 50 the Ogg headers dominate the stream |
| `prebuffer` | `3` | 2 .. 8 | chunks held before playback starts, i.e. 200..800 ms of cushion |

## Constraints

Software-rendered cores only (no GL/Vulkan -- correct for a terminal). Legacy
2D consoles are the sweet spot -- their tiny, low-frame-rate output suits
terminal rendering well.

Frames are truecolor, and sixel is a 256-color format, so each frame is quantized
([`syncretro_quant.c`](syncretro_quant.c)). Legacy consoles use far fewer than
256 colors, so the reduction is normally **exact** and the palette is stable
enough that the color registers are sent once.

## License

Frontend glue: GPL-2.0 (matching the door family). libretro API: permissive.
Cores: their own licenses (FreeIntv GPLv2+). BIOS/ROMs: their rights holders'.
