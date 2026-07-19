# Compiling SyncSCUMM

SyncSCUMM is ScummVM (a vendored, engine-pruned copy under `scummvm/`) plus a
Synchronet `OSystem` backend and door glue in `door/`, linked against the
shared `termgfx` and `xpdev` libraries. One binary, `scummvm`, plays every
bundled title; each title is its own `xtrn/sync<game>/` package that launches
the same binary (see `deploy.js`, `DESIGN.md`).

There are two build paths, because ScummVM has no single cross-platform build
system:

| Host | Driver | Script |
|------|--------|--------|
| Linux / *nix | ScummVM's `configure` + GNU `make` (the door's C libs via CMake first) | `build.sh` |
| Windows | MSVC via ScummVM's `create_project` generator + MSBuild | `build.bat` |

`DESIGN.md` covers the engine seam and the backend; `PROVENANCE.md` records
what is vendored, pruned, and patched.

## Windows (MSVC) — `build.bat`

```
build.bat            # Win32 Release -> build-msvc\Release\syncscumm.exe
build.bat clean      # delete the build tree + junction, then build
```

Win32 (x86) only — the same `x86-windows-static-md` vcpkg triplet the sibling
termgfx doors use (static libraries, dynamic `/MD` CRT, matching ScummVM's
Release `RuntimeLibrary`).

### Prerequisites

- **Visual Studio 2022** (any edition: Professional / Enterprise / Community /
  BuildTools) — provides MSBuild and a bundled CMake.
- **vcpkg** at `C:\vcpkg` (override with the `VCPKG_ROOT` environment
  variable). The media libraries are resolved in vcpkg *manifest* mode from
  `msvc/vcpkg.json`; `build.bat` copies a local `Directory.Build.props` /
  `Directory.Build.targets` next to the generated solution so vcpkg is wired in
  **per-solution** — no machine-wide `vcpkg integrate install`, which would
  leak vcpkg into your other MSBuild projects (e.g. the `sbbs3` tree). The
  first build compiles any missing deps from source (zlib, libmad, libpng,
  libjpeg-turbo, freetype, libjxl, libsndfile; ogg/vorbis/flac/opus are usually
  already present), reusing the vcpkg binary cache thereafter. `libjxl` and
  `libsndfile` drive the JPEG-XL graphics tier and the Opus/Ogg audio streaming,
  the same two libraries the sibling termgfx doors use.

### What `build.bat` does

1. **Junction.** `scummvm/backends/platform/synchronet` is a git symlink to
   `../../../door`; on a Windows checkout it materializes as a plain text file,
   so build.bat replaces it with a directory *junction* (and marks it
   `--skip-worktree` so git ignores the swap). This is what lets create_project
   and MSBuild see the door sources through the module path.
2. **create_project.exe.** Builds ScummVM's project generator (Debug — its own
   Release LTCG ICEs this toolset) if not already built.
3. **termgfx + xpdev.** Builds the door's C libraries as MSVC static libs via
   `door/CMakeLists.txt` (`termgfx.lib`, `xpdev_static.lib`, and termgfx's
   `ADLMIDI.lib`), the same libraries `build.sh` stages first. CMake is pointed
   at the classic vcpkg prefix (`VCPKG_ROOT\installed\x86-windows-static-md`) so
   termgfx compiles the **JPEG-XL** graphics tier (`WITH_JXL`, via libjxl) and
   the **Opus/Ogg audio** streaming (via libsndfile); absent those libs the door
   still builds, degraded to the sixel + raw-PCM tiers.
4. **Generate.** Runs the patched `create_project --synchronet` to emit
   `scummvm.sln` with the curated engines (scumm, sky, queen, lure, drascula,
   agi, sci)
   and *our* backend instead of SDL, with the SDL / OpenGL / network / cloud /
   MIDI-softfont / i18n / unused-video-codec features trimmed to a headless
   door (mirroring `build.sh`'s configure flags). The source path is passed
   **relatively** (`..\scummvm`) — an absolute path breaks create_project's
   per-object intermediate dir (`MSB3191`).
5. **Build.** MSBuild `scummvm.sln`, Release/Win32, with LTCG disabled
   (`WholeProgramOptimization=false`) to dodge the toolset ICE. The door's own
   `.lib`s and the Win32 system libs (ws2_32, winmm) are pulled in by
   `#pragma comment(lib, ...)` in `door/syncscumm.cpp` plus the `--library-dir`
   search path; the vcpkg libs auto-link.

Output: `build-msvc\Release\syncscumm.exe` (not installed). Run
`jsexec deploy.js` afterwards to copy it into the live `syncbass` / `syncqueen`
packages.

### The door's platform seam

Windows-vs-POSIX differences in the door glue (the monotonic clock, sleep,
`WSAStartup`, non-blocking `send()`/`recv()` on the DOOR32.SYS socket, the
`isatty`/exists/getpid dev helpers) live behind `door/sst_plat.{c,h}`, built
over xpdev (`sockwrap.h`, `genwrap.h`, `dirwrap.h`) — the same seam the sibling
termgfx doors ship (`syncmoo1_plat`, `syncretro_plat`). No other door file
carries an `#ifdef _WIN32`, except syncscumm.cpp's platform filesystem-factory
selection (ScummVM's `WindowsFilesystemFactory` vs `POSIXFilesystemFactory`).

## Linux / *nix — `build.sh`

```
./build.sh              # synchronet backend (default)
./build.sh null         # null backend (headless bring-up)
```

Needs a C/C++ toolchain, GNU make, CMake, and ScummVM's usual dependency
`-dev` packages (`libmad0-dev` is **required** — see the comment in
`build.sh`). Produces `build/scummvm`.
