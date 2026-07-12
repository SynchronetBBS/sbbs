# JSZM Z-Machine Door — Sysop Manual

A Synchronet **external program (door)** that plays **Z-machine** interactive-fiction
story files (Infocom classics and modern IF) directly in the terminal server, using
the public-domain **jszm** interpreter ported to Synchronet's JavaScript
(SpiderMonkey 1.8.5).

Supported story-file versions: **v3, v4, v5, and v8** (`.z3` / `.z5` / `.z8`).

- `jszm.js` — the Z-machine interpreter engine (a library).
- `quetzal.js` — the Quetzal save-file codec (a library).
- `zmachine.js` — the Synchronet door front-end (this is what you run).

---

## 1. Requirements

- A current **Synchronet** release with the **SpiderMonkey 1.8.5** JavaScript engine
  and **TypedArray** (`Uint8Array`/`Int16Array`) support.
- No external dependencies beyond stock Synchronet libraries (all from `exec/load/`):
  `sbbsdefs.js`, `userdefs.js`, `cga_defs.js`, `frame.js`, and `http.js` (the last is
  used to fetch game titles from IFDB — see [§5](#5-game-titles-ifdb)).
- One or more **story files** (`.z3`/`.z5`/`.z8`, versions 3/4/5/8), organized into
  one or more subdirectories (see [§2](#2-installation)).
- **Outbound HTTPS** is optional but recommended — used once per game to look up a
  proper title from IFDB. Without it, the door falls back to filename-based titles.

---

## 2. Installation

### Quick Start (Recommended)

Run the Synchronet installer from your install directory:

```bash
jsexec install-xtrn ../xtrn/zmachine
```

This registers the door in SCFG and runs the **games provisioning step**, which
offers to install additional story files alongside the bundled Zork trilogy (Zork
I/II/III, included with MIT license). You can also re-run the provisioning step
anytime to fetch more games:

```bash
jsexec ../xtrn/zmachine/getgames.js
```

The provisioner installs tested story files (including Arthur with version-6 graphics
support) into appropriate category subdirectories. **ImageMagick** (`convert`) is
required only if you select v6 graphics games; standard v3/4/5/8 text games need no
external tools. Only Zork I/II/III are **bundled** with the door (MIT-licensed); every
other catalog entry is **downloaded on request**, one prompt at a time. Many of those
are the classic **commercial Infocom** titles — each carries a reminder, and ensuring
you have the right to use them is your responsibility. See [§5](#5-game-titles-ifdb)
for adding your own games.

### Manual Installation (Alternative)

If you prefer to set up the door manually:

1. Put the door files in their own directory under your install's `xtrn/`, e.g.
   `xtrn/zmachine/`:

   ```
   xtrn/zmachine/jszm.js        <- the interpreter engine
   xtrn/zmachine/quetzal.js     <- the save codec
   xtrn/zmachine/zmachine.js    <- the door (run this)
   xtrn/zmachine/games.ini      <- curated game catalog (for provisioning)
   xtrn/zmachine/getgames.js    <- story-file provisioner script
   ```

2. Put your story files into **subdirectories** of the door directory. Each
   subdirectory that contains at least one story file becomes a **category** in the
   menu (named after the directory). Organize them however you like — by genre, era,
   author, etc.:

   ```
   xtrn/zmachine/fantasy/zork1.z3
   xtrn/zmachine/fantasy/curses.z5
   xtrn/zmachine/science-fiction/seastalker.z3
   xtrn/zmachine/horror/anchor.z8
   ```

   With **no command-line arguments**, the door auto-discovers every subdirectory
   holding story files and offers them as categories (alphabetically). A single
   subdirectory works fine too — you don't have to use multiple categories.

3. Configure the door in SCFG (see [§3](#3-configuring-the-door-in-scfg)).

The engine and door are pure JavaScript; nothing needs to be compiled.

---

## 3. Configuring the door in SCFG

Run `scfg` → **External Programs** → pick (or create) a program section → **add** a
program:

| Field | Value |
|-------|-------|
| **Name** | `Interactive Fiction` (whatever you like) |
| **Internal Code** | `ZMACHINE` (or anything unique) |
| **Start-up Directory** | `xtrn/zmachine/` |
| **Command Line** | `?zmachine.js` |
| **Multiple Concurrent Users** | `Yes` |
| **BBS Drop File Type** | `None` |

The leading **`?`** is required — it tells the terminal server to run the file as a
**JavaScript module**. With the start-up directory set to `xtrn/zmachine/`, the bare
`?zmachine.js` resolves correctly and the door finds its libraries and game
subdirectories beside itself.

> Prefer a full path if you don't want to rely on the start-up directory:
> `?/sbbs/xtrn/zmachine/zmachine.js` (adjust to your install root).

### Command-line options

| Command line | Behavior |
|--------------|----------|
| `?zmachine.js` | **Auto-discover** every game subdirectory as a category |
| `?zmachine.js <dir> [<dir> …]` | Use the named directories as categories, **in the given order** (relative paths resolve against the door's directory) |
| `?zmachine.js <path-to-story>.z5` | Play that one story file directly (no menu) |

So you can leave the default (`?zmachine.js`) and just arrange subdirectories, or pin
a specific category order with explicit arguments, or make a menu entry that launches
one game directly.

(From the sysop console you can test it the same way: `;exec ?/sbbs/xtrn/zmachine/zmachine.js`.)

---

## 4. The chooser

- If there is **more than one** category, the player first sees a **Select a
  Category** menu (each category shows its game count). One category goes straight to
  the game list.
- The **Select a Game** menu lists the category's titles; for multiple categories the
  category name is shown in the heading (e.g. *Select a Game - Fantasy*).
- After a game **ends** (quit/win/death) the player returns to the game menu to play
  again or pick another; **Quit** there goes back to the category menu (or exits the
  door if there's only one category).
- Both menus **default to the last selection**, so replaying or browsing is quick.
- A disconnect mid-game exits the door (and the game is saved for resume — see
  [§7](#7-saved-games)).

### Optional intro & credits

If a file named **`intro.msg`** exists in the door directory it is displayed at
startup (Synchronet Ctrl-A color codes and ANSI/CP437 are honored). A short
attribution line is always shown after it, then a single *[Hit a key]* leads into the
menu.

---

## 5. Game titles (IFDB)

Story files carry no title metadata, so the door derives a proper title from each
file's **Treaty-of-Babel IFID** and looks it up at **IFDB** (ifdb.org). The first
time a category is opened, titles are resolved and cached in a **`titles.ini`** file
inside that category's directory (`filename = title`), so subsequent visits are
instant. If a game can't be reached/found (offline, or not in IFDB), the door falls
back to a prettified filename and you can hand-edit `titles.ini` to set any title.

---

## 6. The player experience

### Display modes (adaptive)

- **Line scrolling (default):** game text scrolls normally; for v3 games the status
  (room, score/moves) is folded into the input prompt, e.g.
  `[ West of House  Score: 0  Moves: 5 ] >`. Preserves terminal **scrollback**.
- **ANSI status bar (v4+):** a redrawn status line on the top row with the narrative
  scrolling natively below — used by v4/5/8 games that manage their own status.
- **ANSI full-screen frame:** for a game that opens a multi-row **upper window**
  (e.g. *Seastalker*'s sonar), auto-engaged via the stock `frame.js`. Uses cursor
  addressing, so scrollback isn't available while active.

Real-time games (e.g. *Border Zone*) are supported via **timed input** (the on-screen
clock advances while you think). v5+ **color/text styles** and **Unicode** output are
honored on capable terminals. PETSCII / non-ANSI terminals always get line-scrolling.

### Sound

The `@sound_effect` opcode plays in one of three tiers, picked automatically per
session:

- **SyncTERM (with audio support):** the standard high/low **bleeps** (used by
  Arthur, Zork Zero, Journey, Beyond Zork) play as synthesized tones, and the
  **sampled sounds** of *The Lurking Horror* and *Sherlock* play digitally. The
  door reads each sound from the game's Blorb (`<story>.blb` beside the story
  file — `getgames.js` downloads these automatically for the two sampled-sound
  games) and uploads it to SyncTERM's per-BBS cache on first play.
- **Older SyncTERM (no sound-file decoding):** bleeps play as tones; sampled
  sounds are silent.
- **All other terminals:** bleeps map to an ASCII BEL; sampled sounds are silent.

No configuration is needed; a missing `.blb` just means that game plays silently.

### In-game commands (typed at the game prompt, intercepted by the door)

| Command | Effect |
|---------|--------|
| `#display` | Toggle line-scrolling ↔ ANSI |
| `#ansi` | Force ANSI |
| `#scroll` | Force line-scrolling |
| `#split` / `#unsplit` | **Diagnostic:** open/close a test upper window |

---

## 7. Saved games

Two independent mechanisms:

**Manual save/restore** — the in-game `SAVE` and `RESTORE` verbs. Saves are
per-user/per-story in **standard Quetzal** format:

```
data/user/<NNNN>.jszm.<story>.save
```

e.g. `data/user/0001.jszm.zork1.save`. Because it's Quetzal (the same FORM/IFZS format
Frotz and others use), the saved game is portable in principle (the door adds one
custom screen-recap chunk that other interpreters ignore).

**Automatic resume on disconnect** — if a player drops carrier, idles out, or is
interrupted mid-game, the door checkpoints the game and stashes it as:

```
data/user/<NNNN>.jszm.<story>.resume
```

The next time that player selects the same game, it **resumes** right where they left
off (the last screen is reprinted). A normal game-end (quit/win/death) clears the
resume slot so the next visit starts fresh.

---

## 8. Terminal compatibility

| Terminal | Result |
|----------|--------|
| ANSI (SyncTERM, most modern clients) | Full feature set; either display mode, color, Unicode |
| PETSCII (C64/128) | Line-scrolling; status on its own line |
| Plain ASCII / dumb | Line-scrolling |

The door detects this with `console.term_supports(USER_ANSI)` / `USER_PETSCII` at
startup and picks defaults accordingly, wrapping to the negotiated screen width.

---

## 9. Limitations

- **v6 graphics need a pre-baked picture cache.** `getgames.js` bakes `<story>.gfx`
  from the game's Blorb at install time (requires ImageMagick); without it, a v6
  game still plays, in text mode.
- **Sound completion routines run at queue time**, not at true playback end.
  Chained sounds (*Sherlock*'s ambient-loop resume and Big Ben hour chimes)
  queue behind the playing sound and play in the right order; only a routine's
  *other* side effects (flag updates) land slightly early.
- **Transcript/scripting to file** (the in-game `SCRIPT` verb) is not implemented.

---

## 10. Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| Door exits immediately, nothing displayed | Run without the `?` prefix, or a bare name not found in the start-up dir. Use `?zmachine.js` with the start-up dir set to `xtrn/zmachine/`, or a full path. |
| "No Z-machine story files found" | No subdirectory holds a `.z3`/`.z5`/`.z8` file (or the wrong dir args were passed). Add story files to a subdirectory. |
| A game you added doesn't appear | It isn't a supported story version, or it's not in a scanned subdirectory. |
| Titles show as filenames instead of real names | IFDB lookup failed (no outbound HTTPS, or the game isn't in IFDB). Edit the category's `titles.ini` to set titles by hand. |
| A first-time category open is slow | First visit resolves titles from IFDB and caches them; later visits are instant. |
| Garbled / mis-wrapped text | Confirm the user's terminal width/type is detected correctly; the door wraps to `console.screen_columns`. |
| RESTORE finds nothing | `RESTORE` loads a **manual SAVE** (`.save`), which is separate from auto-resume (`.resume`); make sure a SAVE exists for that game. |

---

## 11. Credits & license

- **jszm** Z-machine interpreter — public domain, by **zzo38**. This Synchronet build
  is an ES5 / SpiderMonkey-1.8.5 port.
- **Quetzal** save format — by **Martin Frost**; the **Z-Machine Standards Document**
  by **Graham Nelson**; the Z-machine itself created by **Infocom**.
- **Synchronet door & ES5 port** — Rob Swindell (Digital Man).
- Story files are copyright their respective authors; the door includes none. Host
  only files you are entitled to distribute.

The door and engine port are in the public domain.
