# OpenDoors

OpenDoors 6.30 is a C/C++ toolkit for writing online software such as BBS
doors. It transparently interfaces with most BBS systems, displays output on
both local and remote screens, creates ANSI/AVATAR/RIP control sequences, and
provides a sysop interface. The 6.2x changes by Rob Swindell added TCP socket
(Telnet) and Door32.sys support.

## CMake builds

CMake 3.20 or newer can build the shared library, static library, examples,
and link smoke tests on Windows, Linux, and macOS:

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Unix-like systems the libraries are `libODoors.so`/`libODoors.a` (or the
macOS `.dylib` equivalent). With MSVC, the shared build produces
`ODoors62.dll` and `ODoorW.lib`; the static build produces
`ODoors-static.lib`. Consumers linking the Windows static library must define
`OD_WIN32_STATIC`. The `OpenDoors::Shared` and `OpenDoors::Static` CMake target
aliases are available when OpenDoors is included with `add_subdirectory()`.

## MSVC builds

The PowerShell wrapper uses separate build trees for each architecture:

```powershell
.\build-msvc.ps1 -Architecture x64 -Configuration Release
.\build-msvc.ps1 -Architecture x86 -Configuration Debug
```

Use `-Clean` to recreate an architecture's build tree. The script requires
CMake and a compatible Visual Studio/MSVC installation. The included
`odoors.props` property sheet points Visual Studio projects at these default
output locations; set `OpenDoorsLibraryDir` before importing it to override
the library directory.

## Optional xpdev examples

The `ex_ski` and `ex_vote` examples require Synchronet's xpdev target. CMake
uses an existing `xpdev` target first, then an explicit path, and finally
checks only the sibling directory `../xpdev`:

```sh
cmake -S . -B build -DOPENDOORS_XPDEV_DIR=/path/to/xpdev
```

If xpdev is unavailable, those two examples are skipped. The other examples,
both libraries, and the tests still build. With MSVC, pass the same location
as `-XpdevDirectory C:\path\to\xpdev`.

## Legacy builds

The existing GNU make, Windows make, and DOS build files remain available for
older toolchains. New builds should generally use CMake so generated binaries
stay outside the source tree.
