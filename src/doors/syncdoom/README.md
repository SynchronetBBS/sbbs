# syncdoom

Doom as a BBS door, rendered to the remote user's terminal — best on
**SyncTERM** (real graphics via its APC image-cache protocol), and runnable on
any **DOOR32.SYS**-compatible BBS.

It's a vendored fork of [doomgeneric](https://github.com/ozkl/doomgeneric) plus
the `doomgeneric_syncdoom.c` backend in this directory. See `CREDITS` for
attribution; GPL-2.0.

## Build

Linux:

```
make            # -> ./syncdoom   (needs libjxl: apt install libjxl-dev)
```

A Windows build (MSVC `.vcxproj`, linking xpdev) is planned.

## Running as a door

```
syncdoom %f -l%R -t%T -home <per-user-dir> -iwad <path-to-wad>
```

- `%f` — DOOR32.SYS drop-file path (supplies the socket handle + time left).
  Alternatively `-s<fd> -t<sec>` explicitly.
- `-l%R` — the user's screen line count (DOOR32.SYS has no rows field).
- `-home <dir>` — the user's storage dir (config + savegames), used verbatim.
- `-iwad <wad>` — the IWAD (e.g. freedoom1.wad). Resolved to absolute before
  the per-user chdir.

Single-player today. Co-op/deathmatch (a persistent dedicated server) is
planned via the restored Chocolate Doom net layer.

## Render tiers (by terminal capability)

1. **JXL** — SyncTERM ≥ 1.3 (default; ~42 KB/frame).
2. **PPM** — SyncTERM cache-APC without JXL (`-jxl 0`; heavy).
3. **Sixel** — many modern terminals (planned).
4. **CP437 / ANSI half-block** — universal BBS fallback (planned; lifted from doom-cli).
5. **ASCII** — last resort (planned).

## Keys

Arrows move/turn · Space fire · `e` use · `,`/`.` strafe · `]` run · `1`–`7`
weapons · Tab map · Esc menu · Enter select. **Cheats: type in UPPERCASE.**
`-kpsmooth <ms>` tunes the key-hold (key-up synthesis) threshold.
