# SyncRetro launcher -- design

Status: designed 2026-07-09, approved, **not implemented**.
Scope: a JS lobby that discovers ROMs, lets the player pick one, launches the
door, and keeps a play-activity board. Replaces the one-`xtrn.ini`-entry-per-game
arrangement.

This is the door's *front end*, not part of the milestone sequence in
[DESIGN.md](DESIGN.md) §15. It ships whenever it is written.

---

## 1. Why

Today each game needs its own `[prog:...]` section in `ctrl/xtrn.ini`, hard-coding
one ROM path. With ~200 cartridges that does not scale, and it puts content
curation in the BBS's global config. One lobby entry (`cmd=?lobby`, as SyncDOOM
and SyncDuke already use) replaces all of them.

## 2. Scope

**In:** ROM discovery, a picker, launching the door, and a play-activity board
(who played what, when, for how long).

**Out, deliberately: in-game scores.** No libretro core reports a score; nothing
in the ABI has the concept. Reading Astrosmash's 40,000 points means knowing the
Intellivision RAM address that holds it and peeking it through
`retro_get_memory_data()` every frame -- a per-ROM, hand-authored address table.
That is the same shape as the PSG-synthesis idea rejected in
[M4_AUDIO.md](M4_AUDIO.md) §3: it works for exactly this core, by exploiting
FreeIntv's accidental exposure of the whole address space, and generalizes to
nothing. The activity board needs no game knowledge and works for every future
core.

**Out: multiplayer.** `exec/load/game_lobby.js` is a multiplayer model layer --
UDP port allocation, bind/advertise resolution, a file-based game registry,
muster/waiter files, paging other nodes. SyncRetro has no network play: libretro
netplay is a RetroArch feature, not a core one, and FreeIntv's two controllers
are two people at one keyboard. There is nothing to muster.

## 3. What we reuse from `game_lobby.js`

The bottom half, which is game-agnostic and UI-free:

`read_overlaid()` (INI with per-host `[net:<hostname>]`-style overlay),
`resolve_dir()`, `door_ars()` (so the picker honors the door's access
requirements), `attract_files()`, `live_nodes()` (the who's-online line), and the
`rpad`/`clip`/`ago`/`mmss` helpers.

Not the multiplayer half. See §2.

## 4. Files

| Path | Role |
|---|---|
| `xtrn/syncivision/lobby.js` | the launcher; symlinked into the install like SyncDOOM's |
| `xtrn/syncivision/syncivision_lib.js` | discovery, activity store, formatting -- **UI-free**, so it runs headless under `jsexec` and is testable |
| `<data_dir>/syncretro/plays.jsonl` | one JSON object per completed play |
| `xtrn/syncivision/tests/test_syncivision_lib.js` | `jsexec` tests, following `syncduke/tests/` |

The lib/lobby split mirrors `syncduke_lib.js` / `lobby.js` exactly. Per the
repo's directory rules, generated runtime state lives in `data_dir`, never in the
door directory.

**SpiderMonkey 1.8.5**: no `let`, no `const`, no arrow functions, no
`Array.prototype.includes`, no template literals.

## 5. The activity store

`plays.jsonl` is **append-only**, one line per completed play:

```json
{"t":1783000000,"user":1,"alias":"Digital Man","rom":"Utopia (1981) (Mattel).int","secs":1320}
```

No aggregate counts file. `<data_dir>` is an SMB mount shared with a second host,
so a read-modify-write of a counts file across two hosts is a lost-update race
waiting to happen. Each play appends one line under a brief exclusive
`File.open("a")`. Most Played, Your Last Game and per-title counts are all derived
by scanning, which at BBS scale is a few thousand lines. Rotation is a later
problem; do not pre-solve it.

**The lobby writes the log, not the door.** `bbs.exec()` returns when the door
exits -- normally, on carrier loss, or on a crash -- so the JS wrapper knows start
and end wall-clock time in every case. Putting this in the C door would duplicate
it for every future core and lose the record on a segfault. The door stays a game;
the lobby keeps the books.

## 6. Discovery

Rules, in order:

1. Whitelist `.int` / `.bin` / `.rom` (case-insensitive; configurable).
2. Reject anything outside 2 KB - 64 KB: the entire Intellivision cartridge range.
3. Reject `exec.bin` / `grom.bin` by name **and** by their known MD5s
   (`62e761035cb657903761800f4437b8af`, `0cd5946c6473e42e8e4c2137785e427f`), so a
   re-dropped ROM set cannot smuggle a BIOS image back into the picker. The BIOS
   now lives in the door directory (FreeIntv's system dir) and a `bios/`
   sub-directory, not in `roms/`.
4. Apply the sysop's `[roms] exclude=` globs from `syncretro.ini`.
5. **Dedupe by size + hash of the first 4 KB.** Over the SMB view a symlink is
   indistinguishable from a regular file, so `readlink` is useless: short aliases
   like `astrosmash.int` must collapse into `Astrosmash (1981) (Mattel).int` by
   content. Keep the more descriptive name.

Titles parse out of the No-Intro filename -- `Title (Year) (Publisher)` --
stripping `[!]`/`[a1]` dump markers, tolerating nested parens
(`Adventure (AD&D - Cloudy Mountain)`), and falling back to the bare filename when
the pattern does not match.

A sysop who wants a game gone can also just move or delete it.

## 7. Screen

Numbered, multi-column, following `exec/xtrn_sec.js`: it collapses to one column
below 80 columns (`console.screen_columns < 80`), prints item `i` and item `i+n`
side by side, and clips each cell. Derive the column count from
`console.screen_columns` rather than hardcoding two, so a 132-column terminal gets
three.

```
SyncRetro -- Intellivision                      3 nodes online, 1 playing
Most played:  1. Astrosmash (42)   2. Utopia (31)   3. Night Stalker (19)
              4. BurgerTime (12)   5. Tron Deadly Discs (9)

  1 | Advanced D&D (1982)            18 | Las Vegas Poker (1979)
  2 | Armor Battle (1978)            19 | Lock 'N' Chase (1982)
 ...
[1-207] play   [/] search   [N]ext  [P]rev   [Q]uit          page 1/6
```

**Numbering is alphabetical and never moves.** Typing `47` always launches the
same game, whatever the board says. The Most Played block sits above the list
rather than reordering it: a reshuffling list with 200 entries is hostile to
anyone who remembers a number. ~17 rows x 2 columns = 34 titles a page.

## 8. Launch

Follows SyncDOOM's shape (the door accepts both this and a DOOR32.SYS drop file):

```js
var cmd = SR_BINARY + " -s%H -t%T -name %a -core freeintv_libretro.so"
        + " -home " + home + ' "' + rom.path + '"';
bbs.exec(bbs.cmdstr(cmd), EX_NATIVE | EX_BIN, SR_DIR);
```

`home` is `<data_dir>/user/<nnnn>/intv`.

**The quotes matter.** `xtrn.cpp`'s `external()` splits the command line on bare
spaces, and every real Intellivision filename has them. A `"` (or `<>|;`) in the
line diverts the whole thing through `$SHELL -c`, which reassembles the argument
correctly. This is not a new risk: `cmdstr()`'s `QUOTED_STRING()` already quotes
`%a` for any lowercase specifier, so `-name %a` puts a `"` in the line and takes
the shell path on **every** SyncDOOM and SyncDuke launch today.

## 9. Error handling

- **Missing BIOS is detected up front.** Today the player learns about it by
  watching FreeIntv paint `LOAD EXEC FAIL`. The lobby checks for `exec.bin` and
  `grom.bin` before offering to launch, and says so plainly.
- **A ROM whose name contains `"`, `` ` ``, `$` or `\`** is listed but refuses to
  launch, telling the sysop to rename it. Those are the only characters that
  survive double-quoting as metacharacters. No ROM in a stock set has one; a
  sysop can drop in anything.
- ROM vanished between listing and launch -> rescan and report; never exec a
  stale path.
- Empty `roms/`, or everything excluded -> a clear message, not an empty menu.
- The play is logged whatever the door's exit status, including carrier loss.

## 10. Testing

`syncivision_lib.js` is UI-free, so it tests under `jsexec` the way
`syncduke/tests/test_waiting_room_lib.js` does:

- title parsing across the real filename shapes (nested parens, `[!]`, missing
  year, bare names);
- discovery filters: BIOS by hash, symlink dedupe by content, extension and size
  rejects, exclude globs;
- activity aggregation: top-N, per-user last game, a play whose door crashed;
- the quote-safety refusal.

Nothing here needs a terminal, which is the point of the lib/lobby split.

## 11. Deferred

- **Keypad overlays.** The Intellivision's keypad had a printed plastic overlay
  per game, and there is no on-screen acknowledgement when a key is pressed --
  confirmed live in Utopia, where a digit either builds something or silently
  does not. A per-game legend would help. See the notes on FreeIntv's 90-PNG
  `Assets/Overlays.zip`: sysop-supplied, never bundled (unlicensed Mattel art).
- Sorting the list by Most Played behind a key.
- Attract art / `lobby.msg`.
