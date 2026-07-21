# Compiling SyncRPG

SyncRPG is the EasyRPG Player (a vendored copy under `easyrpg/`, with liblcf
and inih beside it in `easyrpg/lib/`) run through a `BaseUi` backend of our own
(`door/ui_term.cpp`, class `BaseUi_termgfx`), linked against the shared
`termgfx` and `xpdev` libraries. One binary — `syncrpg` (`syncrpg.exe` on
Windows) — plays any RPG Maker 2000/2003 project it is pointed at; v1 ships
Yume Nikki.

Unlike SyncSCUMM, the vendored engine here builds with **CMake on every
platform**, so both hosts run the *same* two-stage build and differ only in the
toolchain driving it:

| Host | Driver | Script |
|------|--------|--------|
| Linux / \*nix | CMake + GNU make/Ninja, system `-dev` packages | `build.sh` |
| Windows | CMake + Ninja under MSVC, dependencies from vcpkg | `build.bat` |

Both scripts run the same two stages:

1. **Stage 1** — the door's C libraries (`termgfx`, `xpdev`, and the two-file
   vendored `inih`) via `door/CMakeLists.txt`.
2. **Stage 2** — the vendored EasyRPG Player as a CMake **OBJECT** library
   (`add_subdirectory(easyrpg)`, its own unmodified `CMakeLists.txt`) plus the
   door's own sources, linked against Stage 1's archives — via the top-level
   `CMakeLists.txt`.

`PROVENANCE.md` records what is vendored, pruned, and (not) patched;
`docs/superpowers/specs/2026-07-19-syncrpg-door-design.md` covers the
architecture.

## Windows (MSVC) — `build.bat`

```
build.bat            # Win32 Release -> build-msvc\Release\syncrpg.exe
build.bat clean      # delete the build tree, then build
```

Win32 (x86) only — the same `x86-windows-static-md` vcpkg triplet the sibling
termgfx doors use (static libraries, dynamic `/MD` CRT).

### Prerequisites

- **Visual Studio 2022** (any edition: Professional / Enterprise / Community /
  BuildTools) with the **"C++ CMake tools for Windows"** component, which
  supplies both `cmake.exe` and `ninja.exe`. `build.bat` enters the VS
  developer environment itself — run it from a plain `cmd` prompt, not from a
  Developer Command Prompt.
- **vcpkg** at `C:\vcpkg` (override with the `VCPKG_ROOT` environment
  variable). `build.bat` runs `vcpkg install` for the dependency list below in
  *classic* mode and points CMake at `scripts\buildsystems\vcpkg.cmake`. The
  first build compiles whatever is missing from source and reuses the vcpkg
  binary cache thereafter; budget an hour or so for that first run, almost all
  of it **ICU**.

### Dependencies (vcpkg, `x86-windows-static-md`)

| Package | Needed by | Required? |
|---------|-----------|-----------|
| `fmt`, `libpng`, `pixman`, `zlib` | EasyRPG Player core | **yes** |
| `expat`, `icu` | liblcf (XML, and encoding detection for non-Latin-1 game data — Yume Nikki's project data is Shift-JIS) | **yes** / strongly recommended |
| `sdl2` | EasyRPG's `PLAYER_TARGET_PLATFORM` (see below) | **yes** |
| `freetype`, `harfbuzz` | EasyRPG font rendering / text shaping | yes (default ON) |
| `nlohmann-json` | EasyRPG JSON support | optional |
| `mpg123`, `libsndfile`, `libvorbis`, `opusfile`, `speexdsp`, `libsamplerate` | EasyRPG audio decoders / resampler | optional |
| `libjxl` | **termgfx** — the JPEG-XL graphics tier | optional (falls back to sixel) |
| `libsndfile` | **termgfx** — Opus/Ogg music streaming | optional (falls back to raw PCM) |

`lhasa` (running games out of `.lzh` archives) has no vcpkg port, so
`build.bat` passes `-DPLAYER_WITH_LHASA=OFF`. Installed door packages ship
extracted project directories, so nothing is lost.

### What `build.bat` does

1. **Developer environment.** Locates `VsDevCmd.bat` and calls it with
   `-arch=x86 -host_arch=x64` — the x64-*hosted* cross compiler targeting x86.
   The 32-bit hosted compiler runs out of its ~3 GB address space optimizing
   the larger EasyRPG/liblcf translation units under a parallel build, the same
   reason SyncSCUMM's `Directory.Build.props` sets
   `PreferredToolArchitecture=x64`.
2. **vcpkg.** `vcpkg install --triplet x86-windows-static-md <the list above>`.
3. **Stage 1.** Configures and builds `door/CMakeLists.txt` with the Ninja
   generator into `build-msvc\libs`, producing `termgfx\termgfx.lib`,
   `xpdev_static.lib`, `ADLMIDI.lib` and `inih.lib`.
4. **Stage 2.** Configures and builds the top-level `CMakeLists.txt` into
   `build-msvc\rpg`, with the same `-D` flags `build.sh` passes plus the vcpkg
   toolchain, and links `syncrpg.exe`.
5. **Stage.** Copies the binary to `build-msvc\Release\syncrpg.exe`, where
   `exec/load/door_deploy.js` (shared by every door) looks for it.

Output: `build-msvc\Release\syncrpg.exe` (not installed). Run
`jsexec deploy.js` afterwards to copy it into the installed SyncRPG package
(`deploy.js` and `jsexec` are Synchronet-only; on another BBS, copy the binary
into the game directory yourself).

The binary is large — about 45 MB — because everything except the CRT is
statically linked, and ICU's character-set data is most of it. That is the
cost of the encoding support liblcf needs to read a Japanese-authored RM2k/2k3
project; it costs disk, not memory-resident footprint.

### Windows-specific build wiring

Everything below is why the Windows build is not simply "run `build.sh`'s CMake
lines with `cl`". Each item is a real trap, in the order it bites:

- **`VsDevCmd.bat` overwrites `VCPKG_ROOT`.** It points the variable at Visual
  Studio's own bundled vcpkg (`VC\vcpkg\`), which has none of our ports.
  Because the CMake vcpkg toolchain resolves its root from that environment
  variable, a build that sets `VCPKG_ROOT` *before* entering the developer
  environment silently configures against the wrong prefix: nothing fails,
  `libjxl`/`libsndfile` just come up NOTFOUND and the door quietly degrades to
  the sixel + raw-PCM tiers. `build.bat` stashes any caller-supplied value and
  restores it **after** the `call`.
- **The Ninja generator, not a Visual Studio solution.** EasyRPG's
  `PlayerConfigureWindows.cmake` states outright that multi-config generators
  are unsupported (its find modules assume one configuration) and strips
  `CMAKE_CONFIGURATION_TYPES` down to the current build type. Ninja is
  single-config, so `-DCMAKE_BUILD_TYPE=Release` is the whole story and the
  binary lands directly in the build dir — hence the copy in step 5.
- **The `/MD` CRT has to be forced.** The vcpkg triplet is static libraries
  over the *dynamic* CRT, and every TU in the link must agree. EasyRPG's
  `PlayerConfigureWindows.cmake` derives `CMAKE_MSVC_RUNTIME_LIBRARY` from
  `VCPKG_CRT_LINKAGE`, which is undefined when its `CMakeLists.txt` is
  `add_subdirectory`'d rather than top-level — so its generator expression
  collapses to plain `MultiThreaded` (`/MT`) and the link ends in ~76
  unresolved `__imp__*` externals from *every* vcpkg archive. The top-level
  `CMakeLists.txt` sets the cache entry (with `FORCE`) before
  `add_subdirectory(easyrpg)`.
- **`/Zc:__cplusplus` on the door's own TUs.** Without it MSVC reports
  `__cplusplus` as `199711L` even under `-std:c++17`, and xpdev's `gen_defs.h`
  (reached from `dirwrap.h`) gates its `HAS_STDINT_H` on exactly that macro:
  it concludes the compiler has no `<stdint.h>`, rolls its own
  `int8_t`/`int32_t`/`uint32_t`, and they collide with the real ones the C++
  standard headers already pulled in (`C2371`). The sibling SyncConquer MSVC
  build uses the same flag.
- **`src/platform/windows/` and two `resources/windows/` files were
  un-pruned.** The vendoring pass removed every platform backend except `sdl/`,
  but EasyRPG's `CMakeLists.txt` compiles `src/registry.cpp` +
  `src/platform/windows/{utils,midiout_device_win32}.*` unconditionally on
  `WIN32`, and `src/player.cpp` / `src/audio_generic_midiout.cpp` reference
  them; the same `WIN32` branch `configure_file`s
  `resources/windows/player.rc.in` at *configure* time, so that template must
  exist even though the target it belongs to (upstream's own `Player.exe`) is
  never built here. Restored verbatim from the pinned upstream commit — see
  `PROVENANCE.md`. Neither `PLAYER_BUILD_EXECUTABLE` nor `PLAYER_CONSOLE_PORT`
  can be used to skip that block: both are plain `set()`s inside EasyRPG's
  `CMakeLists.txt`, not cache options, so a `-D` on the command line is
  overwritten.
- **`winmm`, and why native MIDI stays off.** `midiout_device_win32.cpp` is
  compiled into the object library whether or not native MIDI is enabled, so
  its `midiOut*` references must resolve — the top-level `CMakeLists.txt` links
  `winmm` on `WIN32` for that reason alone. `PLAYER_WITH_NATIVE_MIDI` is
  **OFF**: a door must never play the game's MIDI out of the *BBS host's* sound
  card. Music reaches the caller's terminal through termgfx, from EasyRPG's own
  built-in **fmmidi** sequencer.
- **Win32 system libraries must be restated.** Stage 2 links Stage 1's archives
  by absolute path, so the system libraries `xpdev/CMakeLists.txt` declares on
  its CMake target (`iphlpapi ws2_32 winmm netapi32 ole32 uuid`) do not survive
  that seam. The top-level `CMakeLists.txt` names them again under `if(WIN32)`.
- **The JPEG-XL closure must be restated too.** For the same reason, and
  because there is no pkg-config on this path to read `libjxl.pc`'s
  `Requires.private`, `door/CMakeLists.txt` resolves libjxl's companion static
  archives (`jxl_cms`, `hwy`, `brotli{enc,dec,common}`, `lcms2`) explicitly and
  records them in `jxl_libs.txt` for `build.bat` to transcribe. termgfx is
  libjxl's only consumer, so nothing else in the link would bring them in.
- **`/FI` vs `-include`.** The UI substitution force-includes `door/ui_term.h`
  into exactly one engine source (`easyrpg/src/baseui.cpp`) so that
  `BaseUi::CreateUi` builds our class instead of `Sdl2Ui` — a build-wiring
  change with no vendored edit (see `PROVENANCE.md`). MSVC spells that flag
  `/FI<path>`, one token with no separator, where GCC/Clang want
  `-include <path>`.
- **SDL2 is a build-time dependency that is never used.** EasyRPG selects its
  UI through `PLAYER_TARGET_PLATFORM`, which has no "none" option; the only
  non-SDL choice is `libretro`, whose support directory the vendoring pass
  pruned. So the build asks for `SDL2` and compiles `sdl2_ui.cpp` into the
  object library, where it goes unreferenced — nothing constructs an `Sdl2Ui`,
  so no window is ever opened. This mirrors `build.sh`.

### The door's platform seam

Windows-vs-POSIX differences in the door glue (the monotonic clock, sleep,
`WSAStartup`, non-blocking `send()`/`recv()` on the DOOR32.SYS socket, the
`isatty`/exists/getpid dev helpers) live in the shared `../termgfx` library
behind `sst_plat.{c,h}`, built over xpdev — the same seam every sibling termgfx
door uses. SyncRPG's own five `door/*.cpp` files carry **no** `#ifdef _WIN32`
and no POSIX-only headers at all; the only platform-dependent thing
`door/syncrpg.cpp` does is build the save path with xpdev's `PATH_DELIM` /
`IS_PATH_DELIM` and create it with `mkpath()`.

Remember that on Windows a DOOR32.SYS handle is a Winsock `SOCKET`, which the
CRT's `read()`/`write()` cannot touch — so a Windows install **must** use
comm type **2** (socket) I/O. See the README.

## Linux / \*nix — `build.sh`

```
./build.sh              # -> build/syncrpg
```

Needs a C/C++ toolchain, CMake, and the EasyRPG/liblcf dependency `-dev`
packages (`libpng`, `pixman-1`, `fmt`, `freetype`, `harfbuzz`, `expat`, `icu`,
`nlohmann-json`, plus the optional audio decoders), and — for the door's own
tiers — `libjxl` and `libsndfile`.

Two \*nix-only wrinkles, both handled by `build.sh`:

- **inih** is hand-compiled with `cc` from the two vendored files, because
  Debian ships no inih dev package. (`build.bat` gets the same archive from the
  Stage 1 CMake `inih` target instead, MSVC having no `cc` to call.)
- **`-DPLAYER_WITH_NATIVE_MIDI=OFF`** keeps EasyRPG's CMake from wiring in
  `src/platform/linux/midiout_device_alsa.cpp`, which the vendoring pass pruned
  (and which the door would not want anyway — same reasoning as the Windows
  `winmm` note above).
