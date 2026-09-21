# syncdrive

**Test Drive** (Accolade, 1987) -- the sports-car racing classic -- rendered
as a Synchronet / SyncTERM BBS door via
[kylofon/test-drive-sdl3](https://github.com/kylofon/test-drive-sdl3)
(`tdport/`), a faithful C reimplementation of the original DOS EGA release
(`TDEGA.EXE`), and the shared `../termgfx` rendering library -- the same
architecture as `../syncmoo1` and `../syncconquer`. Single player, keyboard
only.

See [DESIGN.md](DESIGN.md) for the full design (architecture, the host
backend, input, high scores, BBS integration), [PROVENANCE.md](PROVENANCE.md)
for exactly what was vendored from upstream and how, and [CLAUDE.md](CLAUDE.md)
for the ours-vs-vendored coding contract before touching any source in this
directory.

The installed package (binary, `syncdrive.ini`, the data-install helper) lives
in `xtrn/syncdrive/`; see [its README](../../../xtrn/syncdrive/README.md) for
the sysop-facing side (installing, supplying the game data, controls).

## Build and test

### Linux / Unix

```
./build.sh            # configure, build, run unit tests
./build/syncdrive --check --game-dir=game
```

### Windows (MSVC)

```
build.bat            :: Win32 release + unit tests
build.bat clean      :: wipe the build tree first, then build
build-msvc\Release\syncdrive.exe --check --game-dir=game
```

The binary lands in `build-msvc\Release\`. Needs Visual Studio 2022
(`build.bat` finds the bundled CMake if none is on `PATH`) and, for sound,
libsndfile from a classic-mode vcpkg prefix at
`C:\vcpkg\installed\x86-windows-static-md` -- `vcpkg install
libsndfile:x86-windows-static-md`. It is not optional in the way the siblings'
JPEG-XL tier is: the speaker PCM is generated in-process, but termgfx streams
it to SyncTERM as Ogg/Opus, and without libsndfile that encoder is a stub and
every chunk is dropped, so the door runs silent. Absent the prefix, `build.bat`
warns and builds on. No libjxl is looked for -- this door serves the sixel tier
only. As on *nix, building does not deploy, and `jsexec deploy.js` installs
what was built.

**Win32 (x86) is the one supported Windows target.** A Win32 door runs on both
a Win32 and a Win64 Synchronet host -- the DOOR32.SYS comm handle is
32-bit-significant and crosses the process-bitness boundary fine -- so one
Win32 binary covers every Windows BBS. The code is 64-bit-clean, so
`cmake -A x64` still compiles, but no x64 binary is shipped or tested.

The high-score lock test needs a second process contending for the lock and
uses `fork()`, so it is built and run on *nix only; the other five unit tests
run on both platforms.

Dev game data lives in `game/` (gitignored). Fetch it from
https://archive.org/download/TestDrive_1987/Test%20Drive.zip; TDEGA.EXE md5
cdf8d1a767d559db559a0b829e1bb3b0.

### Headless smoke test

```
cd game
SYNCDRIVE_SIXELOUT=/path/to/syncdrive.six \
    SYNCDRIVE_KEYS='wait:4000,enter,wait:3000,enter,wait:3000,enter,wait:3000,quit' \
    timeout 60 ../build/syncdrive --game-dir=. --data-dir=/path/to/data
```

Split the sixel capture on DCS ... ST and convert each with
`convert sixel:<file> <png>` to look at the frames. This script runs the
Accolade logo, the "Accolade presents" card with the car reveal, and the
car-select spec screen (a car with its stats table, around frame 150); the
last `enter` selects that car, so the final frames show it driving off before
the script quits.

## Deploy

```
jsexec deploy.js
```

Installs the freshly-built binary into the live `xtrn/syncdrive/` package (or
the in-tree bundle when there is no live install), flat, as
`ctrl/xtrn.ini`'s direct launch requires. Building never touches a live
install; deploying is a separate, explicit step, so a fresh build can be run
and tested before it replaces the binary in front of players.
