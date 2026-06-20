# Compiling SyncDOOM

SyncDOOM is plain C and links one library, **xpdev** (Synchronet's
cross-platform sockets, recursive `mkdir`, and INI parsing). Building therefore
requires a checkout of the Synchronet **source tree**, not just a binary install
— this directory lives at `src/doors/syncdoom/` within it. The CMake build
compiles its own minimal copy of xpdev (cryptography and audio back ends
disabled), so the rest of the tree need not be built first.

The build system is **CMake** (3.13+) on every platform.

| Platform | Toolchain | JPEG-XL tier |
|----------|-----------|--------------|
| Linux / Unix-like | GCC or Clang | system `libjxl` via `pkg-config` |
| Windows | Visual Studio 2022 (MSVC) | MSVC-built `libjxl` via vcpkg |

The JPEG-XL tier is **optional** everywhere — without it the door still serves
the sixel and text/block rendering tiers (see the README). It can be turned off
explicitly with `-DWITHOUT_JPEG_XL=ON`.

---

## Common options

Pass these to the `cmake` *configure* step on any platform:

- `-DWITHOUT_JPEG_XL=ON` — build without the JPEG-XL tier (sixel/text only).
- `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=<dir>` — write the binary straight to `<dir>`
  (e.g. your door's install directory) instead of the build tree.

---

## Linux / Unix-like (GCC or Clang)

Prerequisites:

- CMake 3.13+ and a C compiler (GCC or Clang).
- For the JPEG-XL tier: the libjxl development package (`libjxl-dev` /
  `libjxl-devel`) plus `pkg-config`.

```sh
cmake -B build
cmake --build build -j
```

Produces `build/syncdoom`. If libjxl is found you'll see
`syncdoom: JPEG XL graphics tier ENABLED` in the configure output; otherwise the
build falls back to sixel/text with a warning.

Or run the **`build.sh`** helper, which does the configure + build above and then
copies the binary next to the lobby in this tree's `xtrn/syncdoom/` (`./build.sh`,
plus `debug` / `clean` options) — the same place `build.bat` installs the `.exe`.
For an in-place install that is the live door directory.

---

## Windows (Visual Studio 2022 / MSVC)

Prerequisites: Visual Studio 2022 with the **Desktop development with C++**
workload (includes MSVC and a bundled CMake). All examples use the bundled
CMake; a standalone CMake works too.

Winsock and `winmm` come transitively from the xpdev sub-build, so the base door
needs no extra setup.

For a one-command build, the **`build.bat`** helper runs the configure + build
below (classic-mode vcpkg for the JPEG-XL tier when present) and copies
`syncdoom.exe` into `xtrn\syncdoom\`: `build.bat` (or `build.bat x64`, plus a
`clean` option). The manual steps follow for full control.

### Without the JPEG-XL tier (no dependencies)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

Produces `build\Release\syncdoom.exe` (sixel + text tiers). Use `-A x64` for a
64-bit build.

### With the JPEG-XL tier (via vcpkg)

The bundled `3rdp` libjxl is MinGW-built and won't link with MSVC, so an
MSVC-built static libjxl is supplied through [vcpkg](https://vcpkg.io). Two ways,
both producing a single **self-contained** `syncdoom.exe` (libjxl + its
highway/brotli/lcms2 deps linked statically; no JXL DLLs to ship):

**A. Manifest mode (recommended).** The `vcpkg.json` in this directory pins
libjxl, and vcpkg installs it automatically when CMake is configured with the
vcpkg toolchain. One-time vcpkg setup:

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

**B. Classic mode (manually managed vcpkg).** Install libjxl yourself, then
point CMake at the install tree:

```bat
vcpkg install libjxl:x86-windows-static-md
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

- The `-static-md` triplet links libjxl statically while keeping the default
  dynamic (`/MD`) CRT, so the triplet matches CMake's default MSVC runtime.
  Don't mix it with a `/MT` build.
- libjxl is C++, so a **JXL-enabled** `syncdoom.exe` depends on the C++
  redistributable runtime (`MSVCP140.dll`, `VCRUNTIME140.dll`, the UCRT) — part
  of the standard Visual C++ Redistributable. The non-JXL build needs only the C
  runtime. Plan door distribution accordingly.
- Manifest mode installs both a Release and a Debug libjxl; the CMake build
  selects the right one per `--config`, so Debug and Release both link cleanly.

---

## Output locations

| Build | Binary |
|-------|--------|
| Linux/Unix | `build/syncdoom` |
| Windows (single-config / Ninja) | `build/syncdoom.exe` |
| Windows (VS generator) | `build/<Config>/syncdoom.exe` (e.g. `build/Release/syncdoom.exe`) |

Use `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=<dir>` to redirect the binary to your
door's install directory.
