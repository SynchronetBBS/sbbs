# SyncRPG provenance

## Vendored engine: EasyRPG Player

- Upstream: https://github.com/EasyRPG/Player.git
- Pinned commit: 0582a5eb8a442824c8aad0495443824b3c8afb08
  (2026-07-14, master branch as of scouting)
- Fetched: `git clone` of the upstream repo, checked out at the pinned SHA
- License: GPLv3 (see easyrpg/COPYING; a verbatim copy of the GNU GPL v3)

## No vendored edits for the door UI backend (build-wiring only)

The door runs EasyRPG headless through its own `BaseUi` subclass
(`door/ui_term.{cpp,h}`, class `BaseUi_termgfx`) instead of the SDL2
`Sdl2Ui`. This substitution required **no edit to the vendored `easyrpg/`
tree**. EasyRPG already selects its UI at compile time in
`src/baseui.cpp`: `USE_SDL==N` picks an SdlUi, otherwise the `PLAYER_UI`
macro names the class `BaseUi::CreateUi` instantiates (the same hook the
upstream libretro/3ds/switch ports use). `syncrpg/CMakeLists.txt`
force-includes `door/ui_term.h` into just that one engine source (a
per-source `-include`, via `set_source_files_properties(... TARGET_DIRECTORY
EasyRPG_Player ...)`); the header does `#undef USE_SDL` +
`#define PLAYER_UI BaseUi_termgfx`, so `CreateUi` returns ours. A
source-level `#undef` overrides the build's `-DUSE_SDL=2` regardless of
`-D`/`-U` ordering on the compiler line, so the whole change lives in the
door's own files and CMake wiring. The SDL sources still compile into the
object library but go unreferenced (nothing constructs an `Sdl2Ui`, so no
display is ever opened). Should this ever need to become a genuine vendored
patch, it would be recorded in "Local patches" below instead.

## Local patches (keep this list complete)

Direct edits to vendored `easyrpg/` sources. Each is tagged in-code with a
`syncrpg door: ... PROVENANCE local patch #N` comment (grep
`PROVENANCE local patch #` under `easyrpg/` to find them all), and a re-vendor
must re-apply them by hand. Everything else about this door is build-wiring or
lives in `door/` -- see the section above, and keep it that way: this list is
short on purpose.

1. **`easyrpg/src/scene_logo.cpp`** (the `#include` at the top, and the
   `Text::AlignRight` draw in `Scene_Logo::DrawTextOnLogo()`) -- the startup
   splash. Stock draws EasyRPG's own version left-aligned and
   `WEBSITE_ADDRESS` (`easyrpg.org`) right-aligned on a single row at y=215;
   nothing on it identified the DOOR, or which build of it a caller was
   running. The right slot now draws `syncrpg_version_line()`
   (`door/syncrpg_version.h` -- the door's version, git hash and build date
   from the generated `git_hash.h`), while EasyRPG's version keeps the left
   slot. The two share the row because there is no second one to be had: it
   already runs to y=231 of a 240px screen. `easyrpg.org` is still shown in
   the engine's own Settings > About window (`window_about.cpp`), so no
   attribution is lost. This is drawn by the engine rather than composited
   door-side because the splash bitmap is assembled inside `Scene_Logo` and
   never crosses the `BaseUi` seam as text.

## Vendored library: liblcf

EasyRPG Player reads/writes RPG Maker 2000/2003 project data (.lmt/.ldb/
.lmu/.lsd) through liblcf, a separate EasyRPG project. Upstream's own
`lib/liblcf/.gitignore` marks that path "legacy, used for
PLAYER_BUILD_LIBLCF" -- liblcf is NOT a git submodule of Player (there is
no liblcf entry in Player's `.gitmodules`, which lists only
`builds/libretro/libretro-common`); it is fetched/cloned into `lib/liblcf`
independently and Player's CMakeLists.txt builds it in-tree when
`PLAYER_BUILD_LIBLCF` is set. This vendored copy follows that same layout.

- Upstream: https://github.com/EasyRPG/liblcf.git
- Pinned commit: 666e6c023696d4a45a67dd9ba879dbff7b0f69f3
  (2026-05-20)
- License: MIT (see easyrpg/lib/liblcf/COPYING)

## Vendored library: inih (INI parser)

liblcf's `src/inireader.cpp` parses .ini-style RTP/config data via inih's
C API (`#include <ini.h>`), resolved at CMake configure time through
`find_package(inih REQUIRED)` / `builds/cmake/Modules/Findinih.cmake`.
Upstream normally expects inih to be preinstalled as a system library
(pkg-config `libinih`); rather than depend on a system package, the two
inih source files are vendored directly.

- Upstream: https://github.com/benhoyt/inih.git
- Pinned commit: 577ae2dee1f0d9c2d11c7f10375c1715f3d6940c
  (2026-01-31)
- License: BSD 3-Clause ("New BSD", see easyrpg/lib/inih/LICENSE.txt)
- Vendored files: `ini.c`, `ini.h` (the C library only -- liblcf does not
  use inih's C++ `INIReader` wrapper, so `cpp/INIReader.{h,cpp}` was not
  vendored), placed at `easyrpg/lib/inih/` alongside `easyrpg/lib/liblcf/`.

## Pruning (deletions only -- no upstream file is modified)

### EasyRPG Player (`easyrpg/`)

- `src/platform/`: all platform backends removed except `sdl/`, which is
  kept as the `ui_term` reference backend, and `windows/`, which the MSVC
  build requires (see below). Removed: `3ds`, `amigaos4`, `android`,
  `emscripten`, `libretro`, `linux`, `macos`, `morphos`, `opendingux`,
  `ps4`, `psp`, `psvita`, `switch`, `wii`, `wiiu`.
  `src/platform/clock.h` (a shared, non-platform-specific header) is kept.
  `src/platform/` now contains `clock.h`, `sdl/` and `windows/`.

  `windows/` was initially pruned with the rest and then **restored
  verbatim** from the pinned upstream commit when the Win32/MSVC build was
  added (see `COMPILING.md`): Player's own `CMakeLists.txt` compiles
  `src/registry.cpp` plus `src/platform/windows/utils.{cpp,h}` and
  `src/platform/windows/midiout_device_win32.{cpp,h}` unconditionally on
  `WIN32`, and `src/player.cpp` (`WindowsUtils::InitMiniDumpWriter()`) and
  `src/audio_generic_midiout.cpp` (`Win32MidiOutDevice`) reference them, so
  a Windows build cannot be done without them. The four files are upstream's,
  unmodified; the door never *uses* the Win32 MIDI-out device, because
  `PLAYER_WITH_NATIVE_MIDI` is OFF on both platforms (a door must not play
  game music out of the BBS host's sound card).
- `resources/`: removed (per-platform icons/installers, ~14 MB; no game
  or engine-runtime data) **except** `resources/windows/player.rc.in` and
  `resources/windows/player.manifest`, restored verbatim (as with
  `src/platform/windows/` above) when the Win32/MSVC build was added:
  Player's `CMakeLists.txt` `configure_file()`s that `.rc.in` at *configure*
  time on `WIN32`, so a Windows configure fails outright without it. The
  200 KB `resources/windows/player.ico` it names stays pruned -- the
  generated `.rc` belongs to upstream's own `Player.exe` target, which this
  door never builds (`build.bat` asks for `--target syncrpg`).
- `tests/`, `bench/`: removed (upstream's unit tests and micro-benchmark
  harness; not needed to build/run the engine).
- `docs/`, `.github/`: removed (documentation and CI workflow files).
- `builds/`: all removed except `builds/cmake/` (the CMake support
  modules `CMakeLists.txt` itself pulls in via
  `list(APPEND CMAKE_MODULE_PATH ...builds/cmake/Modules)` --
  `PlayerConfigureWindows`, `PlayerFindPackage`, `PlayerBuildType`,
  `PlayerMisc`, and the `Findinih.cmake`-style modules). Removed:
  `builds/android`, `builds/flatpak`, `builds/libretro`, `builds/macos`,
  `builds/opendingux`, `builds/snap`, `builds/make-dist.sh`,
  `builds/release-helper.sh`.
- `.git/`, `build-sdl2/`, `build-cfg/`: the scouting clone's own VCS
  metadata and CMake configure/build output dirs. `build-cfg/` was an
  untracked leftover CMake cache from earlier scouting (not part of
  upstream), removed along with the rest.
- No bundled game data of any kind is vendored -- `src/generated/*.h`
  (embedded fallback bitmap fonts: Baekmuk, ttyp0, WenQuanYi, Shinonome,
  RMG2000) and `src/external/dr_wav.h` (a vendored WAV-decoder header)
  are engine-internal assets/dependencies used to render UI text and
  decode audio, not RPG Maker project/game content, and are kept as part
  of `src/` (the engine).

### liblcf (`easyrpg/lib/liblcf/`)

Applying the same tests/docs/packaging pruning philosophy as the rest of
this door's vendored trees (see SyncSCUMM's PROVENANCE.md), beyond what
Player's own prune list calls out by name:

- `tests/`: removed (liblcf's own unit tests, including a vendored copy
  of doctest.h).
- `generator/`: removed (a Python code generator that regenerates the
  `src/generated/` and `src/lcf/` reader/writer sources from CSV
  templates; the already-generated sources are vendored, so the
  generator itself is a maintenance-only tool, not a build dependency).
- `bench/`, `mime/`, `tools/`: removed (micro-benchmarks, MIME data, and
  CLI helper tools -- none are linked into the library).
- `builds/`: kept only `builds/cmake/` (needed by Player's
  `CMAKE_MODULE_PATH` append at build-configure time). Removed:
  `builds/autoconf`, `builds/Doxyfile`, `builds/release-helper.sh`,
  `builds/sources2buildsystem.pl`.

## No bundled game data

This door ships no RPG Maker 2000/2003 game/project files (no `.lmt`,
`.ldb`, `.lmu`, `.lsd`, no RTP graphics/audio/fonts belonging to a
specific title). Test/playable content is the sysop's/user's own,
fetched separately at door-configuration time (see the door's own
`.gitignore`, which excludes `test/games/`).

## Vendored size

`easyrpg/` is approximately 11 MB (an initial size estimate of ~6.3 MB
made during scouting undercounted the embedded fallback-font headers in
`src/generated/` (~3 MB) and the full liblcf engine sources needed for
buildability; nothing outside the KEEP list above is included).
