# Coding contract for src/doors/syncretro

SyncRetro is a libretro **frontend** door: it hosts an external, compiled
libretro core and renders it through `../termgfx`. See [DESIGN.md](DESIGN.md)
for the architecture and [PROVENANCE.md](PROVENANCE.md) for the one vendored
artifact.

## Ours vs. vendored -- the line is different here

Unlike the sibling doors (`../syncmoo1`, `../syncdoom`, `../syncduke`,
`../syncconquer`), SyncRetro vendors **no game-engine source tree**. The engine
is a libretro core `.so` loaded at runtime; we never see or compile its source.
So:

- **Everything in this directory is ours** -- `main.c`, `retro_*.c`,
  `syncretro_*.c`/`.h`, the build files, and the docs.
- **The one vendored file is `libretro.h`** -- the MIT-licensed libretro API
  header (from libretro-common). It is committed, pinned, and **never
  hand-edited**; re-vendor from upstream per PROVENANCE.md rather than patching
  the committed copy.
- **Cores are never patched** -- by construction, since we only ever load a
  compiled `.so`. If a core misbehaves, the fix belongs in our frontend
  (`retro_env.c` policy, `retro_bridge.c` conversion), not in the core.

## House style: TABS (uncrustify), from the start

New C in this directory uses the repo's **tab** indentation (the
[../../uncrustify.cfg](../../uncrustify.cfg) house style), and should be run
through uncrustify as the closing step of a change. This is a deliberate
divergence from `../syncmoo1`, which drifted to 4-space and flagged it as a
known deviation -- do **not** repeat that here. Match the existing tabbed files
in this directory.

## Portability

Frontend C compiles under GCC/Clang (primary) **and** MSVC. The Windows build
is Win32 (x86), configured by `build.bat` / installed by `deploy.js` (via
jsexec, shared with the *nix build), mirroring the sibling doors. Two -- and only two -- files carry a Windows-vs-POSIX
`#ifdef`; keep it that way when adding code:

- **`syncretro_plat.c` / `.h`** -- the whole clock / sleep / socket / stderr
  seam, over xpdev (`genwrap.h` clock+sleep, `sockwrap.h` sockets with the
  WSA->errno normalization). Every other `syncretro_*.c` / `main.c` file routes
  its monotonic clock, sleep, and non-blocking descriptor I/O through this and
  stays `#ifdef`-free. A Winsock `SOCKET` is not a CRT fd, so the seam uses
  `send`/`recv`/`ioctlsocket` on Windows and `read`/`write`/`fcntl` on POSIX.
- **`retro_core.c`** -- the core-load seam (`dlopen`/`dlsym`/`dlclose` vs
  `LoadLibrary`/`GetProcAddress`/`FreeLibrary`). It is NOT in the plat seam
  because xpdev's `xp_dlopen()` mangles the name (`lib%s.so` / `%s.dll`, version
  suffixes) and so cannot load a core by the explicit path we hand it -- the
  shim loads the path verbatim. A core is a `.dll` on Windows, `.so` on *nix.

Path/existence/canonicalization goes through xpdev (`FULLPATH()`, `fexist()`,
`mkpath()`), never POSIX `realpath()`/`access()` (MSVC has neither). Otherwise
follow the repo-wide C/C++ portability rules in the top-level
[../../../CLAUDE.md](../../../CLAUDE.md) (no reserved identifiers, no `goto` past
initializers, return from non-void paths, etc.). A clean MSVC build is not full
verification -- GCC/Clang are stricter; see the top-level file.
