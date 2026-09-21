# Test Drive

Accolade's 1987 sports-car racing classic, as a Synchronet external program
(door). The engine is a faithful reimplementation of the original DOS EGA
release, rendered to the caller's terminal as sixel graphics with PC-speaker
sound. Single player, keyboard only.

This directory (`xtrn/testdrive/`) is the installed door -- the `syncdrive`
binary, `syncdrive.ini`, the data-install helper, and the game data all live
here. The **source** lives in `src/doors/syncdrive/` of the Synchronet source
tree.

Two things you must deal with before your first player: the door needs a
**sixel-capable terminal**, and it needs the **original game files, which you
supply**. Both are covered below.

## Terminal requirements

Test Drive is a picture, so the door renders **sixel only**. **SyncTERM** is
the recommended and best-tested client; any other sixel-capable terminal
should also work.

## Installing

The bundled `install-xtrn.ini` lets the Synchronet installer register the door
for you -- no manual SCFG entry needed. Launch it any of these ways:

- **From the BBS, as a sysop** -- run **Auto-install New External Programs**
  (the `xtrn-setup` module), included by default in the **Operator**
  external-programs section. Find **Test Drive** in the list and install it.
- **SBBSCTRL (Windows)** -- **File -> Run -> Install External Programs**.
- **Command line** -- `jsexec install-xtrn ../xtrn/testdrive`.
- **Terminal sysop command** -- `;exec ?install-xtrn ../xtrn/testdrive`.

The installer registers the door, seeds `syncdrive.ini` from
`syncdrive.example.ini` (it never overwrites an existing one without asking),
and then offers -- prompted, and safe to decline -- to install the Test Drive
game data from a copy you've placed in this directory. Declining leaves the
door installed; you can supply the data at any time afterwards.

## Game data -- you must supply this

The door is only an engine; it needs the original 1987 DOS **EGA** release of
Test Drive, which is not shipped with it. Test Drive is abandonware, not
freeware -- the rights passed from Accolade through later publishers -- so
supplying a copy is the sysop's responsibility.

The exact release this door was verified against is the copy archived at
`https://archive.org/download/TestDrive_1987/Test%20Drive.zip`.

**Put your copy in this directory**, then let `getdata.js` sort it out. It
accepts any of three shapes:

- the loose game files dropped straight in;
- a `.zip` or other archive containing them; or
- an extracted game folder (a subdirectory holding the files).

It copies the files the door needs into place, verifies the result, and
**downloads nothing**. It's idempotent -- files already present are left
alone -- so re-running only fills in what's still missing:

```
jsexec ../xtrn/testdrive/getdata.js
```

If no complete copy is found, it says so and tells you where to put one; it
never fails the install outright.

## Controls

| Key(s) | Action |
|---|---|
| Up / Down | Accelerate / brake |
| Left / Right | Steer |
| A, Z | Shift up one gear, shift down one gear |
| Esc | Quit the current drive |
| Ctrl-P | Pause |
| F2 | Sound on/off |
| Ctrl-F | Toggle fit to screen / true aspect |
| Ctrl-S | Toggle the stats bar |
| Ctrl-Q | Quit the door |
| F1, Ctrl-K | Show the keys (the game waits until you press a key) |

Shifting takes one press per gear: let go of A or Z between shifts, and wait
for the knob to finish moving across the gear gate on the dashboard, since
shift, steering and throttle presses are ignored while it moves. The arrow
keys do not move the knob through the gate; that only happens with a joystick
in the original game.

The door starts out driving with the original key-repeat behavior. On a
terminal that reports key releases (SyncTERM, kitty), it switches to holding a
key down for as long as it's actually held, which feels closer to a real
control; a terminal that never reports a release keeps the original behavior
for the whole session.

## High scores

There is one high-score table for the whole BBS, shared by every node, kept in
`data/testdrive/SCORES`. Entries are named by the caller's own BBS alias,
which is filled in automatically and can't be changed at the entry screen.
The name field holds up to 15 characters; a longer alias is cut off there.

## `syncdrive.ini`

Door configuration file, read from this directory at launch. The installer
seeds it from `syncdrive.example.ini` (never overwriting an existing one).
Every key is optional.

- **`sixel_max`** -- the largest sixel image to send, in pixels. Leave it at
  `0` to let the terminal decide.
- **`[input] held_keys`** -- `auto` (default) drives with held keys on a
  terminal that reports key releases, falling back to the original key-repeat
  behavior everywhere else; `off` always uses the original behavior.
- **`[game] frame_rate`** -- the drawing rate of the original 1987 PC while
  driving. The gear-shift panel, traffic randomness, and the crash animation
  are all timed against it; leave it at the default (`8`) unless you have a
  specific reason to change it.
- **`[audio]`** -- `enabled` and `volume` control the PC-speaker sound.

## License & credits

Test Drive is (c) Accolade / Distinctive Software. This door's own code is
GPL-2.0. Full attribution and engine provenance ship with the source in
`src/doors/syncdrive/`.
