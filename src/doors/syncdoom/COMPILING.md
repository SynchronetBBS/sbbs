# Compiling SyncDOOM

SyncDOOM is plain C. It links **xpdev** (Synchronet's cross-platform sockets,
recursive `mkdir`, and INI parsing) and **termgfx** (the shared terminal
graphics/audio library in `../termgfx`, built automatically by this build).
Building therefore requires a checkout of the Synchronet **source tree**, not
just a binary install — this directory lives at `src/doors/syncdoom/` within it.
The CMake build compiles its own minimal copy of xpdev (Synchronet's own
cryptography and audio back ends disabled), so the rest of the tree need not be
built first.

The build system is **CMake** (3.13+) on every platform. A C++ compiler is also
required (the bundled OPL/MIDI music synth, libADLMIDI, is C++).

Two features come from **optional** libraries — the rest of the door (sixel and
text graphics, digital sound effects, and OPL/MIDI music) builds with no external
dependencies:

| Feature (from termgfx) | Optional dependency | Linux / Unix | Windows (MSVC) |
|------------------------|---------------------|--------------|----------------|
| **JPEG-XL** graphics tier | `libjxl` | system, via `pkg-config` | vcpkg (static) |
| **OGG** music compression | `libsndfile` | system, via `pkg-config` | vcpkg (static) |

Both are **auto-detected**:

- **Without libjxl** the door still serves the sixel and text/block tiers (see
  the README). Turn it off explicitly with `-DWITHOUT_JPEG_XL=ON`.
- **Without libsndfile** music still plays — it is just shipped to the terminal
  as raw PCM WAV instead of compressed OGG/Vorbis (larger uploads, same sound).
  The FM synthesis itself is **libADLMIDI**, vendored under
  `../termgfx/libADLMIDI` and built in-tree, so no external MIDI library is ever
  needed. Digital sound effects need no optional dependency at all.

---

## Common options

Pass these to the `cmake` *configure* step on any platform:

- `-DWITHOUT_JPEG_XL=ON` — build without the JPEG-XL tier (sixel/text only).
- `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=<dir>` — write the binary straight to `<dir>`
  (e.g. your door's install directory) instead of the build tree.

---

## Linux / Unix-like (GCC or Clang)

Prerequisites:

- CMake 3.13+ and C **and** C++ compilers (GCC or Clang — the C++ compiler
  builds the bundled libADLMIDI music synth).
- For the JPEG-XL graphics tier: the libjxl development package (`libjxl-dev` /
  `libjxl-devel`) plus `pkg-config`.
- For OGG music compression: the libsndfile development package
  (`libsndfile1-dev` / `libsndfile-devel`) plus `pkg-config`.

On Debian/Ubuntu, for example:

```sh
sudo apt install cmake g++ pkg-config libjxl-dev libsndfile1-dev
```

Then:

```sh
cmake -B build
cmake --build build -j
```

Produces `build/syncdoom`. The configure output reports which optional tiers
were found:

- `syncdoom: JPEG XL graphics tier ENABLED` (else it falls back to sixel/text
  with a warning), and
- `termgfx: libsndfile … found -> OGG music compression` (else
  `termgfx: libsndfile NOT found -> music ships as raw PCM WAV`).

Or run the **`build.sh`** helper, which does the configure + build above
(`./build.sh`, plus `debug` / `clean` options), leaving the binary at
`build/syncdoom`. Building does **not** deploy — run **`jsexec deploy.js`** afterwards
to copy the binary next to the lobby in this tree's `xtrn/syncdoom/` (and, on a
copy-style install, into the live install located via `$SBBSCTRL` or
`SYNCDOOM_DEST=<dir>`). Keeping deploy separate lets you rebuild and test before
pushing a new binary to a running BBS; on a SYMLINK=1 in-place install the live
door already points at `build/syncdoom`, so `deploy.js` is a no-op there.

---

## Windows (Visual Studio 2022 / MSVC)

Prerequisites: Visual Studio 2022 with the **Desktop development with C++**
workload (includes MSVC and a bundled CMake). All examples use the bundled
CMake; a standalone CMake works too.

Winsock and `winmm` come transitively from the xpdev sub-build, so the base door
needs no extra setup.

For a one-command build, the **`build.bat`** helper runs the configure + build
below (classic-mode vcpkg for the JPEG-XL tier when present): `build.bat` (or
`build.bat x64`, plus a `clean` option), leaving `syncdoom.exe` under
`build-msvc\Release\`. Building does **not** install it — run **`jsexec deploy.js`**
afterwards to copy it into `xtrn\syncdoom\` (and, on a copy-style install, into
the live install via `%SBBSCTRL%` or `SYNCDOOM_DEST`). The manual steps follow
for full control.

### Without the JPEG-XL tier (no dependencies)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

Produces `build\Release\syncdoom.exe` (sixel + text tiers). Use `-A x64` for a
64-bit build.

### With the optional tiers (JPEG-XL graphics + OGG music, via vcpkg)

The optional libraries — **libjxl** (JPEG-XL graphics) and **libsndfile** (OGG
music compression) — must be MSVC-built to link into this MSVC door; the MinGW
builds under `3rdp` won't. Both are supplied through [vcpkg](https://vcpkg.io).
Two ways, both producing a single **self-contained** `syncdoom.exe` (libjxl,
libsndfile, and their transitive deps — highway/brotli/lcms2 and
Vorbis/Ogg/FLAC/Opus — linked statically; no DLLs to ship):

**A. Manifest mode (recommended).** The `vcpkg.json` in this directory pins
libjxl and libsndfile, and vcpkg installs them automatically when CMake is
configured with the vcpkg toolchain. One-time vcpkg setup:

```bat
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
```

Then configure + build (the first configure builds/downloads libjxl, which can
take a while; later builds are cached):

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 ^
      -DCMAKE_TOOLCHAIN_FILE=%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
      -DVCPKG_TARGET_TRIPLET=x86-windows-static-md
cmake --build build --config Release
```

**B. Classic mode (manually managed vcpkg).** Install the libraries yourself,
then point CMake at the install tree:

```bat
vcpkg install libjxl:x86-windows-static-md libsndfile:x86-windows-static-md
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 ^
      -DCMAKE_PREFIX_PATH=<vcpkg>\installed\x86-windows-static-md
cmake --build build --config Release
```

Either way, confirm `syncdoom: JPEG XL graphics tier ENABLED` in the configure
output. To verify the tier at runtime, force it: `syncdoom.exe -jxl 1 …` prints
`mode=jxl` in the startup banner.

**For a 64-bit build**, use `-A x64` and the `x64-windows-static-md` triplet
(manifest mode) or `…\installed\x64-windows-static-md` (classic mode).

Notes:

- The `-static-md` triplet links the libraries statically while keeping the
  default dynamic (`/MD`) CRT, so the triplet matches CMake's default MSVC
  runtime. Don't mix it with a `/MT` build.
- `syncdoom.exe` depends on the **C++ redistributable runtime** (`MSVCP140.dll`,
  `VCRUNTIME140.dll`, the UCRT — part of the standard Visual C++ Redistributable)
  regardless of the optional tiers, because the bundled libADLMIDI music synth is
  C++ (and libjxl is too). Plan door distribution accordingly.
- Manifest mode installs both a Release and a Debug of each library; the CMake
  build selects the right one per `--config`, so Debug and Release both link
  cleanly.

---

## Output locations

| Build | Binary |
|-------|--------|
| Linux/Unix | `build/syncdoom` |
| Windows (single-config / Ninja) | `build/syncdoom.exe` |
| Windows (VS generator) | `build/<Config>/syncdoom.exe` (e.g. `build/Release/syncdoom.exe`) |

Use `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=<dir>` to redirect the binary to your
door's install directory.
