# SyncIvision

The **Mattel Intellivision** as a Synchronet external program (door): cartridges
played over the BBS, rendered to the terminal as sixel graphics with the
console's own audio — including the Intellivoice — streamed to SyncTERM.

This directory (`xtrn/syncivision/`) is the installed door — the `syncretro`
binary, the FreeIntv core, the lobby, `syncretro.ini`, the console BIOS, and your
cartridges. The **source** lives in `src/doors/syncretro/` of the Synchronet
source tree.

**One door, many consoles.** SyncIvision is not its own program: it is the
SyncRetro door pointed at a [libretro](https://www.libretro.com/) core. The same
binary runs `xtrn/syncnes` (NES) from the same source. What makes *this* one an
Intellivision is `lobby.js` — it names the console, its core, its key bindings
and what a cartridge looks like, in about a dozen lines.

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

## Installing into Synchronet

SyncIvision ships an `install-xtrn.ini`, so the bundled installer registers it
for you — no manual SCFG entry needed:

- **From the BBS, as a sysop** — run **Auto-install New External Programs** (the
  `xtrn-setup` module), find **Intellivision**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/syncivision`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncivision`.

The installer seeds `syncretro.ini` from `syncretro.example.ini` and offers
(prompted) to download the **FreeIntv** core, which is GPLv2+ and freely
redistributable, from the libretro buildbot. You can also run it later:
`jsexec ../xtrn/syncivision/getcore.js`.

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term` afterwards.

## BIOS and cartridges

**Neither is shipped** — both are copyrighted content you supply.

- **The console BIOS is required.** Drop `exec.bin` (8192 bytes) and `grom.bin`
  (2048 bytes) into **this** directory. Without them FreeIntv loads a cartridge,
  halts, and paints its own "PUT GROM/EXEC IN SYSTEM DIRECTORY" screen — the
  lobby checks first and says so plainly instead.
- **Cartridges** go in `roms/` (`.int`, `.bin`, `.rom`) and appear in the lobby
  on the next entry. A BIOS image is rejected from the picker by name *and* by
  content, so a re-dropped ROM set can't smuggle one in as a cartridge.
- Identical files under different names collapse to one entry, and multiple dumps
  of the same game (`[!]`, `[a1]`, …) collapse to the best-ranked dump. Two
  genuinely different ports of the same title (the two Pac-Mans) both survive.
  Hide anything you don't want with `[roms] exclude`.

## Playing

The lobby lists the cartridges, keeps a play-activity board, and launches the
door. The Intellivision's hand controller is a 12-key **keypad**, a 16-direction
**disc**, and three action buttons — and **both controllers are live at once**,
so two people can share one keyboard:

| Key | Player 1 | Player 2 |
|---|---|---|
| Disc | `W A S D` | arrows |
| Action buttons | `Z X C` | `,` `.` `/` |
| Keypad digits | number row | numeric keypad |
| Keypad Clear / Enter | `Backspace` / `Enter` | numpad `Del` / `Enter` |

| | |
|---|---|
| `Tab` | swap which controller your keys drive |
| `+` / `-` | volume (at 0 the door sends no audio at all) |
| `?` | the key list |
| `Space` | pause |
| `Ctrl-S` / `Ctrl-R` / `Ctrl-Q` | stats overlay / reset the console / quit |

**Most games start on a keypad digit, not an action button** — and many
cartridges (both Pac-Man ports, for instance) read only the *right* controller.
If nothing responds, press `Tab` and try again.

A **SyncTERM**-class terminal gets graphics and sound; the door degrades rather
than failing on a terminal without them.

## Configuration

`syncretro.ini` holds this install's settings, fully commented in-file. The
console itself (name, core, key bindings, what a cartridge is) is **not** in
there — it is in `lobby.js`, because it doesn't vary from BBS to BBS.

- **`[disc] rotate`** — cartridges whose paddle follows disc *rotation* rather
  than a held direction (Brickout! and friends). On these, holding Left/Right
  sweeps the disc so the paddle travels smoothly; everywhere else a held
  direction pins the disc, exactly as the hardware does.
- **`[video] aspect`** — the shape the picture is drawn at. `core` (the default)
  uses what FreeIntv asks for, which for this console is also its raw framebuffer
  shape; `4:3`, `square` or a number override it.
- **`[audio]`** — the core's sound, streamed as Opus chunks at the core's own
  rate (44.1 kHz here). `volume` is only the level a player *starts* at; he
  changes it with `+`/`-` in-game.
- **`[roms]`** — `dir` (default `roms`), `ext`, and `exclude`.

Per-user save data is written under `data/user/<num>/intv/`, which Synchronet
auto-cleans with the user account.

## Legal

The libretro API is permissive and the **FreeIntv** core is GPLv2+ — both are
free to redistribute. **The BIOS and cartridges are not**, and none are shipped:
sourcing legally-owned copies is the sysop's responsibility. This door's own code
is GPL-2.0.
