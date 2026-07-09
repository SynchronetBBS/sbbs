# Provenance -- vendored artifacts & external content

SyncRetro vendors **no game-engine source**. It has exactly one vendored file
(`libretro.h`, MIT),
plus two classes of external runtime content (cores and console data) that are
NOT committed here.

## Vendored: `libretro.h`

The libretro API header, from **libretro-common**
(<https://github.com/libretro/libretro-common>, file `include/libretro.h`).

- **Status:** VENDORED.
- **License:** MIT (Copyright (C) 2010-2024 The RetroArch team), stated in the
  header's own comment block, which explicitly permits implementing a frontend
  against it. Preserve that upstream comment verbatim.
- **Pinning:**

  ```
  libretro.h  from libretro/libretro-common
    commit 76f431e4c9cdaeb317ef85851fcbcd618e4c4459  (2026-07-06)
    sha256 a11f7887be2af72bdf7f97dbd975f24df66922aaa6d5e6a7100f0bc1595fc1ae
    RETRO_API_VERSION 1
  ```

  That is the newest upstream commit touching `include/libretro.h`; its content
  is byte-identical to the `master` tip (`dfccc5330a8e`) as of the vendoring.

- **Updating:** re-copy from a fresh upstream commit and update the pin above;
  never hand-edit the committed copy (see [CLAUDE.md](CLAUDE.md)). Bumping it
  can change `RETRO_API_VERSION` -- `retro_core.c` rejects a core whose
  `retro_api_version()` doesn't match, so re-verify cores after a bump.

## External runtime content (NOT committed)

1. **libretro core `.so`** -- e.g. FreeIntv (Intellivision), fceumm (NES). GPL /
   freely redistributable; obtained from the libretro buildbot or built from the
   core's own source. Loaded at runtime via `-core <path>` / door config. May be
   bundled into the door's `xtrn/` dir (like `../syncdoom`'s fetched binary), but
   is not part of this source tree (`.gitignore` excludes `*_libretro.so`).

   **FreeIntv**, the first target, builds from source with no configure step:

   ```
   git clone --depth 1 https://github.com/libretro/FreeIntv.git
   cd FreeIntv && make            # -> freeintv_libretro.so
   ```

   Built and verified against this tree at upstream commit `428915b`
   (`library_version` "1.2  428915b"), `retro_api_version()` 1 -- matching the
   vendored `libretro.h`. Note the output is lower-case `freeintv_libretro.so`,
   not the `FreeIntv_libretro.so` spelling used in RetroArch's core list.

   Observed core characteristics (drive the frontend's converter + pacer):
   pixel format `XRGB8888`, geometry 352x224, aspect 1.5714, **60.0 fps**,
   44100 Hz, and `need_fullpath = 1` (so the frontend passes a ROM *path*, and
   never has to read the ROM into memory).

   **Frontend constraint discovered here:** FreeIntv's `retro_init()` passes the
   `GET_SYSTEM_DIRECTORY` answer straight to `fill_pathname_join()` without a
   NULL check, so a frontend that returns `false` (or `true` with a NULL
   pointer) **segfaults before `retro_load_game()` is ever called**. `retro_env.c`
   therefore always answers the directory queries with a valid path. Keep that
   guard when adding cores; it is cheaper than trusting each one to null-check.

   FreeIntv also ships **4-Tris** (`open-content/4-Tris/4-tris.rom`), a GPLv2
   homebrew game by Joe Zbiciak -- freely redistributable, and usable as a test
   ROM so development doesn't depend on commercial content. It still needs the
   BIOS below to run.

2. **Console system BIOS + game ROMs** -- e.g. Intellivision `exec.bin` /
   `grom.bin`, and game ROMs. **Commercial, copyrighted content.** SyncRetro
   ships none of it; a legally-owned copy is the sysop's to supply (same policy
   as `../syncmoo1`'s MoO1 LBX data). Installed into the BIOS/system dir handed
   to the core via `GET_SYSTEM_DIRECTORY`.

   FreeIntv requires **both**, and treats neither as optional
   (`firmware_opt = "false"` in its `.info`). Filenames are case-sensitive:

   | ROM           | File       | MD5                                |
   |---------------|------------|------------------------------------|
   | Executive ROM | `exec.bin` | `62e761035cb657903761800f4437b8af` |
   | Graphics ROM  | `grom.bin` | `0cd5946c6473e42e8e4c2137785e427f` |

   Without them the core loads and reports its AV info, but logs
   `Failed loading Executive BIOS` and cannot run a game.

## First target

- **Core:** FreeIntv (Intellivision), GPLv2+, from libretro.
- **Reference emulator (accuracy oracle, not vendored):** jzIntv (Joe Zbiciak).
