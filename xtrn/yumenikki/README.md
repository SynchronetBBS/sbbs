# SyncRPG (Yume Nikki)

**Yume Nikki** — Kikiyama's freeware surreal-exploration game (2004–2007, RPG
Maker 2003) — as a Synchronet external program (door): play as Madotsuki, who
cannot leave her room but, in her dreams, wanders a vast and unsettling world
collecting reality-bending "Effects." There is no combat and no stated goal;
it is pure atmosphere and exploration, rendered to the terminal as sixel /
JPEG-XL graphics with sound streamed to SyncTERM.

This directory (`xtrn/yumenikki/`) is the installed door — the `syncrpg` binary,
the game data (`yumenikki/`), and the fetched-data helper live here. The
**source** (the vendored EasyRPG Player engine plus the Synchronet backend that
drives it) lives in `src/doors/syncrpg/` of the Synchronet source tree; that one
binary can play any RPG Maker 2000/2003 game — this directory just points it at
Yume Nikki. See `src/doors/syncrpg/README.md` for the door's full, BBS-agnostic
documentation (DOOR32.SYS contract, terminal tiers, architecture).

> **Note:** Yume Nikki contains occasional flashing imagery. Players sensitive
> to flashing graphics should take care.

## Building the door binary

The `syncrpg` binary is not produced by the normal Synchronet build; build it
separately, then deploy it here. It compiles against the in-tree `xpdev` and
`termgfx` libraries and the vendored EasyRPG tree, so a binary-only install is
not enough — you need a checkout of the **Synchronet source tree**, **CMake**,
and a C++ compiler (**GCC**/**Clang**, or **MSVC** on Windows).

```
cd src/doors/syncrpg
./build.sh                 # Windows: build.bat
jsexec deploy.js           # copies the binary into xtrn/yumenikki/
```

`deploy.js` copies the built binary in as `xtrn/yumenikki/syncrpg`
(`syncrpg.exe` on Windows).

## Installing into Synchronet

SyncRPG ships an `install-xtrn.ini`, so the bundled installer registers it for
you — no manual SCFG entry needed:

- **From the BBS, as a sysop** — run **Auto-install New External Programs**
  (the `xtrn-setup` module), find **Yume Nikki**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/yumenikki`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/yumenikki`.

It registers Yume Nikki under **Games → RPG** as a native DOOR32.SYS door
(binary socket I/O, multi-node), with per-user saves. The installer also offers
to fetch the game data for you (see below).

## Game data

The game data is **not** bundled or redistributed. `getdata.js` fetches it on
demand — it downloads the official Playism English localization of **Yume Nikki
0.10a** (freeware) from a stable community archive over HTTPS and **sha256-
verifies** it before extracting:

```
jsexec ../xtrn/yumenikki/getdata.js
```

(The `install-xtrn` step above offers to run this for you.) It writes the
`yumenikki/` game tree (~79 MB) into this directory, which is where the door's
command line expects it. Re-running it is idempotent — it skips the download if
the data is already present.

## Playing

Yume Nikki is **keyboard only** — RPG Maker 2000/2003 games have no mouse
control:

| Key | Action |
|-----|--------|
| Arrow keys | Move |
| Enter / Z / Space | Confirm / interact |
| Esc / X | Menu / cancel |
| Numpad 9 | Pinch cheek (wake from the dream) |
| Numpad 1 / 3 | Effect actions (for Effects that have them) |
| Numpad 5 | Drop the equipped Effect (in the Room of Doors) |
| Ctrl-S | Toggle the door status bar |
| Ctrl-Q | Quit the door |

**Saving:** Yume Nikki has no menu save — interact with the **desk** in
Madotsuki's starting bedroom to save. On the title screen, **Dream Diary** loads
your save (it is greyed out until a save exists). Saves are per-user, kept in
`data/user/<####>/yumenikki/`, so two nodes never share one save slot.

Requires a graphics-capable ANSI terminal — **SyncTERM** (sixel or JPEG-XL) is
the reference client.
