# Compiling SyncRetro

SyncRetro is a libretro **frontend**: it vendors no game engine, and the
emulator itself — the libretro **core** — is never compiled here. Building
produces one small binary that hosts *any* core, so the same `syncretro` runs
SyncArcade (MAME 2003-Plus), SyncNES (FCEUmm) and SyncIvision (FreeIntv). The
core is a separate, per-platform artifact installed by `getcore.js` (see
[The core is not built here](#the-core-is-not-built-here)).

| Host | Driver | Script | Output |
|------|--------|--------|--------|
| Linux / \*BSD / macOS | CMake + the native toolchain | `build.sh` | `build/syncretro` |
| Windows | CMake + MSBuild (Visual Studio 2022, Win32) | `build.bat` | `build-msvc\Release\syncretro.exe` |

The binary links `../termgfx` (sixel/JXL encode, APC transport, audio
streaming) and `../../xpdev` (sockets, `.ini` parsing); both are built as
CMake sub-targets from this tree, so a binary-only Synchronet install is not
enough — you need the source tree. `DESIGN.md` covers the architecture and
`PROVENANCE.md` the one vendored file (`libretro.h`).

---

## Linux / Unix-like (GCC or Clang)

### Prerequisites

- **CMake** 3.13+ and **C *and* C++** compilers (GCC or Clang) — termgfx is
  C++.
- For the **JPEG-XL** graphics tier: `libjxl` development package plus
  `pkg-config`. Optional — without it the door still serves the sixel and
  text/block tiers.
- For **Opus/Ogg** audio streaming: `libsndfile` development package plus
  `pkg-config`. Optional in the build sense, but a door built without it cannot
  stream the core's audio.

On Debian/Ubuntu:

```sh
sudo apt install cmake g++ pkg-config libjxl-dev libsndfile1-dev
```

No libretro package is needed: `libretro.h` is vendored, and cores are loaded
at run time with `dlopen()`.

### Build

```sh
./build.sh              # Release -> build/syncretro
./build.sh debug        # Debug build
./build.sh clean        # delete the build tree, then build
```

`build.sh` is a thin wrapper — the equivalent by hand is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The configure step reports which optional tiers it found; check these lines
rather than discovering a missing library at run time:

```
-- termgfx: libsndfile 1.2.2 (pkg-config) found -> OGG music compression
-- syncretro: JPEG XL graphics tier ENABLED
```

(Absent libjxl the second becomes a warning and the JXL tier is dropped.)
A `Package 'portaudio-2.0' ... not found` note from the xpdev sub-build is
expected and harmless — this tree pins xpdev's audio and crypto backends off,
because a door never plays sound locally.

### Configure options

Pass to the `cmake` *configure* step:

- `-DWITHOUT_JPEG_XL=ON` — build without the JPEG-XL tier (sixel/text only).
- `-DSYNCRETRO_TESTS=ON` — also build the unit tests (see below).
- `-DSYNCRETRO_PROBE=ON` — also build `probe_core`, the core diagnostic.

### Install the binary — `deploy.js`

Building deliberately touches no install. Deploy with the shared jsexec
script, which finds **every** SyncRetro console install and copies the binary
into each:

```
jsexec deploy.js
```

```
[deploy] target: linux-x64/  (syncretro)
[deploy] console: /sbbs/xtrn/syncarcade/
[deploy] Deployed: /sbbs/xtrn/syncarcade/linux-x64/syncretro
[deploy] console: /sbbs/xtrn/syncnes/
...
```

On a \*nix host the binary lands in an `<os>-<arch>` sub-directory
(`linux-x64`, `freebsd-x64`, `darwin-arm64`, …), not at the door root: every
\*nix build is named `syncretro`, so a shared install serving two hosts would
otherwise have them collide. Windows stays flat. `deploy.js` is a jsexec
script rather than a shell/batch pair precisely so it derives that directory
with the same code the lobby does — see "Installing" in the README.

### The core is not built here

The libretro core is a **separate, platform-specific** binary, downloaded from
the libretro buildbot by each console's `getcore.js` — nothing in this tree
compiles or redistributes one:

```
jsexec ../xtrn/syncarcade/getcore.js       # run against the LIVE install
```

```
SyncRetro: installing the MAME 2003-Plus (arcade) core
  (mame2003_plus_libretro.so) into <install>/xtrn/syncarcade/linux-x64/
```

It installs into the same per-target sub-directory `deploy.js` uses. **Run it
once per platform**: a console first installed from Windows has only the
`.dll` at the door root, and a \*nix host of that same shared install needs its
own `.so` (and vice versa). ROMs, BIOS files and the core's system directory
are platform-independent and stay at the door root, shared by both.

### Verify a core without a terminal — `probe_core`

`probe_core` loads a core through the door's *real* `retro_core.c` and
`retro_options.c`, prints what the core actually does, and exits. It opens no
terminal and needs no BBS, which makes it the smoke test for a freshly
installed core on a new platform:

```sh
cmake -S . -B build -DSYNCRETRO_PROBE=ON && cmake --build build --target probe_core
cd <door dir> && <build>/probe_core linux-x64/mame2003_plus_libretro.so roms/pacman.zip
```

It reports the geometry, fps, sample rate and pixel format the door will read,
every core option and environment call, the distinct-colour count (under 256
the sixel quantizer is exact), and an ASCII thumbnail of a real frame — so a
successful run proves the core loaded, found its ROM, and produced video.

### Tests

```sh
cmake -S . -B build -DSYNCRETRO_TESTS=ON && cmake --build build -j
ctest --test-dir build
```

```
100% tests passed, 0 tests failed out of 5
```

No core is linked into any of them. They cover the keypad digit map, the
controller bindings, the sixel quantizer's colour-register map, the dirty-rect
tracker, and `test_audio_bytes` — a golden transcript of the audio APC bytes,
which is what keeps the shared termgfx audio module honest.

---

## Windows (MSVC, Win32)

```bat
build.bat             :: Win32/Release -> build-msvc\Release\syncretro.exe
build.bat clean
```

Needs Visual Studio 2022 (any edition) and, for the JXL tier, libjxl from a
classic-mode vcpkg prefix (`vcpkg install libjxl:x86-windows-static-md`).
Then `jsexec deploy.js` and the console's `getcore.js`, exactly as above —
both scripts are shared by the two platforms.

**Win32 is the one supported Windows target**, deliberately: a Win32 door runs
on both a Win32 and a future Win64 Synchronet host, and it must load a Win32
core `.dll`. See the README for the rest of the Windows-specific behavior
(hidden console, the per-node log file).
