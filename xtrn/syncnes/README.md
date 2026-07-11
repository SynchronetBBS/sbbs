# SyncNES

The **Nintendo Entertainment System** as a Synchronet external program (door):
cartridges played over the BBS, rendered to the terminal as sixel graphics with
the console's own audio streamed to SyncTERM.

This directory (`xtrn/syncnes/`) is the installed door — the `syncretro` binary,
the FCEUmm core, the lobby, `syncretro.ini`, and your cartridges. The **source**
lives in `src/doors/syncretro/` of the Synchronet source tree.

**One door, many consoles.** SyncNES is not its own program: it is the SyncRetro
door pointed at a different [libretro](https://www.libretro.com/) core. The same
binary runs `xtrn/syncivision` (Intellivision) from the same source, and a third
console is an install directory rather than a new program. What makes *this* one
an NES is `lobby.js` — it names the console, its core, its key bindings and what
a cartridge looks like, in about a dozen lines.

## Building the door binary

The `syncretro` binary is not produced by the normal Synchronet build; build it
separately, then deploy it here.

- A checkout of the **Synchronet source tree** — the build compiles against the
  in-tree `xpdev` and `termgfx` libraries, so a binary-only install is not enough.
- **CMake** 3.13+ and a C compiler (**GCC**/**Clang**, or **MSVC** on Windows).

```
cd src/doors/syncretro
./build.sh                 # Windows: build.bat
jsexec deploy.js           # installs the binary into EVERY console install dir
```

`deploy.js` finds each SyncRetro console (any `xtrn/*/lobby.js` that drives the
shared lobby) and puts the binary where that console's lobby looks for it, so
adding a console never means editing the deploy step.

## Installing into Synchronet

SyncNES ships an `install-xtrn.ini`, so the bundled installer registers it for
you — no manual SCFG entry needed:

- **From the BBS, as a sysop** — run **Auto-install New External Programs** (the
  `xtrn-setup` module), find **Nintendo Entertainment System**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/syncnes`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncnes`.

The installer seeds `syncretro.ini` from `syncretro.example.ini` and offers
(prompted) to download the **FCEUmm** core, which is GPLv2 and freely
redistributable, from the libretro buildbot. Decline it if you have no outbound
internet and drop the core in by hand; you can also run it later:
`jsexec ../xtrn/syncnes/getcore.js`.

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term` afterwards.

## Cartridges

**SyncNES ships no cartridges.** They are copyrighted content you supply. Drop
them into `roms/` and they appear in the lobby on the next entry.

- **Extensions**: `.nes`, `.unf`, `.unif`. Add others in `[roms] ext`.
- **Archives do not work.** A `.zip` is invisible to the lobby, and would be
  unreadable by the core anyway — it opens the ROM file itself. Extract them.
- **No BIOS is needed.** Unlike the Intellivision, an NES boots from the
  cartridge alone. The one exception is Famicom Disk System (`.fds`) images,
  which need `disksys.rom` in this directory; if you have it, drop it here and
  add `fds` to `[roms] ext`.
- Identical files under different names are collapsed to one entry, and multiple
  dumps of the same game (`[!]`, `[a1]`, …) collapse to the best-ranked dump.
  Hide anything you don't want with `[roms] exclude`.

## Playing

The lobby lists the cartridges, keeps a play-activity board (who played what,
when, for how long), and launches the door. In-game:

| Key | |
|---|---|
| `W A S D`, arrows, numpad | D-pad |
| `Z` / `X` | B / A |
| `Enter` / `Backspace` | Start / Select |
| `Tab` | swap the two controller ports (for a game that reads port 2) |
| `+` / `-` | volume (at 0 the door sends no audio at all) |
| `?` | the key list |
| `Space` | pause |
| `Ctrl-S` / `Ctrl-R` / `Ctrl-Q` | stats overlay / reset the console / quit |

A **SyncTERM**-class terminal gets graphics and sound; the door degrades rather
than failing on a terminal without them.

## Configuration

`syncretro.ini` holds this install's settings, fully commented in-file. The
console itself (name, core, key bindings, what a cartridge is) is **not** in
there — it is in `lobby.js`, because it doesn't vary from BBS to BBS.

- **`[video] aspect`** — the shape the picture is drawn at. A console's pixels
  are not square: the NES's 256×240 framebuffer was shown on a 4:3 television.
  `core` (default) uses what FCEUmm asks for (1.219, the 8:7 pixel-aspect
  convention, which is what every other libretro frontend does); `4:3` is what a
  television actually showed; `square` is the raw framebuffer, which looks a
  quarter too narrow. The door logs the value it settled on at startup.
- **`[audio]`** — the core's sound, streamed as Opus chunks. The sample rate is
  the core's own (48 kHz here), so there is nothing to set. `volume` is only the
  level a player *starts* at; he changes it with `+`/`-` in-game.
- **`[roms]`** — `dir` (default `roms`), `ext`, and `exclude` (case-insensitive
  substrings of filenames to hide).

Per-user save data (SRAM) is written under `data/user/<num>/nes/`, which
Synchronet auto-cleans with the user account.

## Legal

The libretro API is permissive and the **FCEUmm** core is GPLv2 — both are free
to redistribute. **Cartridges are not**, and none are shipped: sourcing
legally-owned copies is the sysop's responsibility. This door's own code is
GPL-2.0.
