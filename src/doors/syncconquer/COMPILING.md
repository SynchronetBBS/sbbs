# Compiling SyncConquer

SyncConquer builds the Command & Conquer doors — Westwood's originals over
DOOR32.SYS — from a vendored [Vanilla Conquer](https://github.com/Vanilla-Conquer/Vanilla-Conquer)
subset (`vanilla/`) whose video/keyboard/audio backends are replaced by
termgfx-backed shims in `door/`. See `DESIGN.md` for the engine seam and
`PROVENANCE.md` for what is vendored and what is patched.

The project is `syncconquer`, and this **one tree builds two titles** — the
per-title naming from `DESIGN.md`:

| Binary | Title | Engine |
|--------|-------|--------|
| `syncalert` | Command & Conquer: Red Alert | `vanilla/redalert/` |
| `syncdawn` | Command & Conquer: Tiberian Dawn | `vanilla/tiberiandawn/` |

Both link the *same* `door/` engine seam, termgfx, and xpdev; they differ only
in the vendored engine they wrap and a `SYNCCONQUER_TD` compile definition that
selects the branding, MIX manifest, and data/log basename. **Both are built by
default** (`BUILD_VANILLATD=ON`); pass `ra` to `build.sh` / `build.bat` to build
Red Alert alone. Each gets its own xtrn bundle — `xtrn/syncalert/` and
`xtrn/syncdawn/` — and `jsexec deploy.js` installs whichever were built.

CMake pulls in three sub-projects automatically — the vendored engine
(`vanilla/`), `../termgfx` (sixel/JXL encoders, APC transport, audio manager),
and `../../xpdev` (portability wrappers). None need separate installation.

**Game data is not built and not shipped.** Each title needs the freeware MIX
files of its own game, and each resolves `<binary's own directory>/assets` by
default:

| Door | Needs | Lands in |
|------|-------|----------|
| `syncalert` | the freeware RA95 `REDALERT.MIX` + `MAIN.MIX` | `xtrn/syncalert/assets/` |
| `syncdawn` | the freeware C&C Gold MIX set (`CONQUER.MIX`, the `UPDATE*` patch mixes, the theater mixes, …) | `xtrn/syncdawn/assets/` |

`jsexec install-xtrn ../xtrn/syncalert` (or `../xtrn/syncdawn`) offers to
download them, or run that bundle's `fetch-assets.js` by hand — e.g. `jsexec
../xtrn/syncdawn/fetch-assets.js`.

## Common options

| Option | Default | Effect |
|--------|---------|--------|
| `BUILD_VANILLATD` | `OFF` for a raw `cmake`; **`ON`** as `build.sh` / `build.bat` invoke it | Also build the Tiberian Dawn engine and the `syncdawn` door |
| `WITHOUT_JPEG_XL` | `OFF` | Build without the JPEG-XL graphics tier (sixel/PPM/text only) |
| `CMAKE_BUILD_TYPE` | — | `Release` / `Debug` (single-config generators only) |

The `BUILD_VANILLATD` split is worth knowing: the CMake cache default is `OFF`
(Red Alert only), and it's the build scripts that turn it `ON`. So a **hand-run
`cmake` produces no `syncdawn` unless you pass `-DBUILD_VANILLATD=ON`** — the
`if(TARGET VanillaTD)` block in `CMakeLists.txt` is simply a no-op without it.

The vendored engine's *other* switches (`SDL2`, `OPENAL`, `BUILD_REMASTERTD`, …)
are pinned `FORCE` by `CMakeLists.txt` — the door build is headless by
construction and supplies its own backends. Don't set them by hand.
`BUILD_VANILLATD` is the deliberate exception: it's a plain (non-`FORCE`) cache
variable precisely so a build dir can opt Tiberian Dawn in or out.

Two optional libraries raise the fidelity tier; both are detected, and both are
skipped with a warning (not an error) when missing:

- **libjxl** — the JPEG-XL graphics tier for SyncTERM. Without it: sixel/PPM/text.
- **libsndfile** — OGG/Opus music compression. Without it: music ships as raw PCM.

---

## Linux / Unix-like (GCC or Clang)

Prerequisites: CMake ≥ 3.25, a C/C++ toolchain, and (optionally) system libjxl
and libsndfile, both found via `pkg-config`. On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake pkg-config libjxl-dev libsndfile1-dev
```

Then, from this directory:

```sh
./build.sh              # release build -> build/syncalert + build/syncdawn
./build.sh ra           # Red Alert only -- skip Tiberian Dawn
./build.sh debug        # Debug build
./build.sh clean        # remove the build tree
```

`build.sh` is a thin wrapper; the manual equivalent is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_VANILLATD=ON
cmake --build build -j$(nproc)
```

Building does **not** install the binaries. Run **`jsexec deploy.js`** afterwards
to copy `build/syncalert` into `xtrn/syncalert/` and `build/syncdawn` into
`xtrn/syncdawn/` (and, on a copy-style install, into the live install via
`$SBBSCTRL` or `SYNCALERT_DEST`). `deploy.js` skips SyncDawn when it wasn't
built.

---

## Windows (Visual Studio 2022 / MSVC)

Prerequisites: Visual Studio 2022 with the **Desktop development with C++**
workload (includes MSVC and a bundled CMake). All examples use the bundled
CMake; a standalone CMake works too.

Winsock comes transitively from the xpdev sub-build, so the base door needs no
extra setup. Two MSVC-specific adjustments are made by *our* `CMakeLists.txt`,
not by patching the vendored tree (see `PROVENANCE.md` § *Deliberate
non-patches*), and neither needs anything passed by hand:

- The vendored `make_icon()` is shadowed by a no-op (`cmake/BuildIcons.cmake`).
  Upstream hard-fails on a Windows checkout, where the `resources/` git symlink
  materializes as a plain text file; a console door wants no icon anyway.
- The engine links as a GUI app (`/subsystem:windows`). We re-link
  `/subsystem:console`, so `main()` gets valid std handles and the door's
  startup/fatal diagnostics actually reach the sysop.

For a one-command build, the **`build.bat`** helper runs the configure + build
below (classic-mode vcpkg for the optional tiers when present), leaving
`syncalert.exe` and `syncdawn.exe` under `build-msvc\Release\`:

```bat
build.bat            :: both titles (Win32 release)
build.bat ra         :: Red Alert only -- skip Tiberian Dawn
build.bat clean      :: delete the build tree, then build
```

Building does **not** install them — run **`jsexec deploy.js`** afterwards to
copy them into `xtrn\syncalert\` and `xtrn\syncdawn\` (and, on a copy-style
install, into the live install via `%SBBSCTRL%` or `SYNCALERT_DEST`). The manual
steps follow for full control.

> **Win32 (x86) only.** Unlike SyncDuke/SyncDOOM, this door has no x64 build, and
> `build.bat` refuses `x64` up front. The vendored engine's
> *Windows-without-SDL* path — the one the headless door selects — carries
> Win16-era 32-bit assumptions that upstream never compiles (its Windows build
> always defines `SDL_BUILD` and skips them): `wwmouse.cpp` hands `timeSetEvent`
> a `DWORD`-argument callback where `LPTIMECALLBACK` wants `DWORD_PTR`, and
> `winstub.cpp` assigns a `long(HWND,UINT,UINT,LONG)` to a `WNDPROC`. Both are
> hard MSVC errors at x64 and harmless at x86, where `DWORD`/`LONG` are
> pointer-width. Porting that path is an upstream job.
>
> The same never-compiled path is why Tiberian Dawn needed one vendored patch to
> build here at all: TD's `winstub.cpp` called the legacy `Winsock` object
> without Red Alert's `#ifndef WINSOCK_IPX` guard, and that object exists only in
> the `#ifndef _WIN32` half of `common/fakesock.h` — an
> `error C2065: 'Winsock': undeclared identifier` on MSVC. See `PROVENANCE.md`
> patch #32.

### Without the optional tiers (no dependencies)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -DBUILD_VANILLATD=ON
cmake --build build --config Release
```

Produces `build\Release\syncalert.exe` and `build\Release\syncdawn.exe` (sixel +
text graphics; music ships as raw PCM). Keep `-A Win32` — see the x64 note
above. Drop `-DBUILD_VANILLATD=ON` to build Red Alert alone.

### With the optional tiers (JPEG-XL graphics + OGG music, via vcpkg)

The optional libraries — **libjxl** (JPEG-XL graphics) and **libsndfile** (OGG
music compression) — must be MSVC-built to link into this MSVC door; the MinGW
builds under `3rdp` won't. Both are supplied through [vcpkg](https://vcpkg.io).
Two ways, both producing **self-contained** binaries (libjxl, libsndfile, and
their transitive deps — highway/brotli/lcms2 and Vorbis/Ogg/FLAC/Opus — linked
statically into each `.exe`; no DLLs to ship):

**A. Manifest mode (recommended).** The `vcpkg.json` in this directory pins
libjxl and libsndfile, and vcpkg installs them automatically when CMake is
configured with the vcpkg toolchain. One-time vcpkg setup:

```bat
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
```

Then configure + build (the first configure builds/downloads the libraries, which
can take a while; later builds are cached):

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -DBUILD_VANILLATD=ON ^
      -DCMAKE_TOOLCHAIN_FILE=%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
      -DVCPKG_TARGET_TRIPLET=x86-windows-static-md
cmake --build build --config Release
```

**B. Classic mode (manually managed vcpkg).** Install the libraries yourself,
then point CMake at the install tree. This is what `build.bat` does, probing
`C:\vcpkg\installed\<triplet>`:

```bat
vcpkg install libjxl:x86-windows-static-md libsndfile:x86-windows-static-md
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -DBUILD_VANILLATD=ON ^
      -DCMAKE_PREFIX_PATH=<vcpkg>\installed\x86-windows-static-md
cmake --build build --config Release
```

Either way, confirm `syncalert: JPEG XL graphics tier ENABLED` and
`termgfx: libsndfile … found -> OGG music compression` in the configure output.

Notes:

- The `-static-md` triplet links the libraries statically while keeping the
  default dynamic (`/MD`) CRT, so the triplet matches CMake's default MSVC
  runtime. Don't mix it with a `/MT` build.
- Both `syncalert.exe` and `syncdawn.exe` depend on the **C++ redistributable
  runtime** (`MSVCP140.dll`, `VCRUNTIME140.dll`, the UCRT — part of the standard
  Visual C++ Redistributable) regardless of the optional tiers, because the
  Vanilla Conquer engine and the bundled libADLMIDI music synth are C++ (and
  libjxl is too). Plan door distribution accordingly.
- Manifest mode installs both a Release and a Debug of each library; the CMake
  build selects the right one per `--config`, so Debug and Release both link
  cleanly.

---

## Output locations

| Build | Binaries |
|-------|----------|
| Linux/Unix (`build.sh`) | `build/syncalert`, `build/syncdawn` |
| Windows (`build.bat`) | `build-msvc\Release\syncalert.exe`, `build-msvc\Release\syncdawn.exe` |
| Windows (VS generator, manual) | `build\<Config>\syncalert.exe`, `build\<Config>\syncdawn.exe` |

(`syncdawn` only when Tiberian Dawn was enabled — the default for both scripts,
`-DBUILD_VANILLATD=ON` for a hand-run `cmake`.)

Use `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=<dir>` to redirect the binaries, or just
run `jsexec deploy.js`, which puts each where its door reads it —
`xtrn/syncalert/` and `xtrn/syncdawn/`, one level above the `assets/` dir each
engine loads its MIX files from.
