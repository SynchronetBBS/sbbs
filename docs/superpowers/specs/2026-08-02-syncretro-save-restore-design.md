# SyncRetro suspend and resume — design

Date: 2026-08-02
Status: implemented 2026-08-02
Depends on: `2026-08-02-syncretro-config-consolidation-design.md` (implemented)

## Problem

A player asked for SyncArcade to let them save and restore a game. The
objection to doing it naively is real: an arcade cabinet's NVRAM holds the
machine's high-score table, every player on the BBS writes to the same copy,
and a snapshot restored later rolls that table back to whenever the snapshot
was taken — erasing scores other players set in between.

That objection is about *degree*, not kind. The shared cabinet already loses
scores, on any install. MAME loads NVRAM at start and writes it at unload
(`GAMES_INI.md`), so two players on two nodes both read the same NVRAM at
launch and both write it at exit: last one out wins, and the first player's
high score is gone. No save/restore involved, and no network filesystem
needed — two nodes on one machine are enough. Suspend/resume widens a
window that is already open, from "another player's session" to "however long
I leave my snapshot sitting".

So the fix is not to police snapshots. It is to give a player the choice of
which machine they are sitting at, and let suspend/resume follow from that.

### What exists today

Nothing. No part of the tree calls `retro_serialize()` or
`retro_unserialize()`. `GAMES_INI.md` §"Why not a save state" evaluated the
mechanism for a different purpose (skipping boot time) and measured MAME
2003-Plus round-tripping **32-324 KB in under a millisecond across seven
romsets, with no `SET_SERIALIZATION_QUIRKS` declared** — so the mechanism is
known-good on the core that matters. It was rejected there for reasons that do
not apply here, and it recorded this one, which does: *"A boot snapshot
restored on every launch can roll back or clobber high scores."*

What does exist is the storage isolation the feature needs.
`syncretro_lobby_play()` picks the door's `-home` from the console's
`shared_saves` flag: one directory for every player on an arcade console,
`data/user/<####>/<id>/` on every other. Two of the three console packages are
already private per user; only `xtrn/syncarcade` is shared.

## Goals

- A player's game survives the end of a session — including the end they did
  not choose, a dropped carrier — and resumes where they left off.
- A player decides whether they are competing on the shared machine or playing
  their own, and that decision is visible before they start a game.
- The shared high-score table cannot be rolled back by a restore. Structurally,
  not by policy.
- A snapshot is never restored into an emulator it does not match.
- No new per-lobby-entry I/O. The picker already scans a directory; this must
  not add a probe per cartridge.

## Non-goals

- Manual save slots. Auto-suspend on exit and auto-resume on launch only.
- Periodic autosave. The exit path covers every ordinary session end (§2).
- Sharing or transferring snapshots between users.
- Bounding snapshot growth. Deliberately deferred; see "Deferred".
- **Verifying which MAME romsets snapshot reliably.** A separate deliverable;
  the arcade ships with every romset's `save_state` off, and turning them on
  later is a data change (§7).
- Anything for the sibling doors (`syncscumm`, `syncrpg`, …). They are separate
  programs, not SyncRetro consoles.

## Design

### 1. Two cabinets

A console whose `[console] shared_saves` is false is already a private machine
per user. The arcade's toggle gives a player that same isolation on demand: it
swaps `-home` between `data/syncretro/<id>/shared` and
`data/user/<####>/<id>/` — the identical path the cartridge consoles already
use.

| | shared cabinet | private cabinet |
|---|---|---|
| high scores | the machine's, shared with everyone | the player's own |
| suspend/resume | **no** | yes |
| NVRAM at first entry | whatever the machine holds | empty, a virgin machine |

Suspend/resume is therefore a property of being on a private machine, and the
high-score table cannot be rolled back because the shared cabinet never writes
a snapshot at all.

`Ctrl-R` remains "start over": it calls `retro_reset()`, which restarts the
machine without wiping NVRAM, so a player keeps their own accumulated scores.
It composes correctly with auto-suspend — reset, then quit, and the snapshot
written on the way out is a fresh machine.

### 2. When the snapshot is written

At `main.c`'s `done:` label, before `rc_core_close()`.

That is the single point every exit converges on. `sr_door_should_exit()`
(`syncretro_door.c:121`) returns true on carrier loss, `Ctrl-Q`, the DOOR32
time limit and the idle timeout, and each breaks the frame loop into `done:`,
which already tears down audio and restores the terminal. So a snapshot there
catches every ordinary session end **including a dropped carrier**, which is
the case a BBS player cares most about.

Only a hard kill or a crash escapes it. That is acceptable and needs no
periodic autosave: the cost of missing one is a game that starts fresh.

Restore happens after `rc_core_load_game()` and before the frame loop.
`retro_unserialize()` requires a loaded game, so the ROM decode happens either
way — a snapshot skips emulated time, never load time.

### 3. Naming the snapshot, and the staleness key

One file per ROM, in the door's `-home`:

    <rom-basename>.<key8>.state

`key8` is the first 8 hex digits of a hash over three things: the core
binary's hash, the ROM's hash, and the resolved `[options]` sorted by name.
`GAMES_INI.md` is explicit that a snapshot blob carries **no version stamp**,
so a cache must be keyed on core, romset and options, and that a miss on any of
them restores garbage into the emulator.

Putting the key in the **filename** rather than inside the file or a sidecar is
what keeps the lobby cheap. One `directory()` call yields existence and
validity together, so the lobby can tell a live snapshot from a stale one with
no per-cartridge I/O, and it already holds every input to compute the current
key:

- the **ROM's hash** is the full-file md5 in `data/syncretro/roms.<id>.json`,
  which discovery already caches per cartridge (`LAUNCHER.md` §6);
- the **core binary's hash** is cached the same way and for the same reason —
  keyed on the core file's size and mtime, so a warm run reads no bytes of it.
  Hashing it per lobby entry would be a real cost rather than a notional one —
  the MAME 2003-Plus core is multi-megabyte, and re-reading it on every entry is
  precisely the pattern the ROM cache exists to avoid, the more so on an install
  reached over a network filesystem;
- the **resolved options** come from `syncretro.ini` + `syncretro.local.ini`,
  which the lobby reads anyway.

After a core upgrade every key changes, so nothing matches, nothing is marked,
and no stale blob can be restored — the failure mode becomes unreachable rather
than merely unlikely. A player loses that suspend silently. The alternative is
keeping the stale file in order to report it, and a mid-game restore into a
subtly-broken machine is worse than a lost suspend.

### 4. The lobby

**Marking.** One `directory()` call per lobby entry lists the user's
snapshots; names are matched in memory against the cartridge list the picker
already has. A cartridge is marked suspended only when a snapshot's key matches
what would restore right now. A snapshot whose key matches no current cartridge
is garbage and is deleted during that same pass, so a core upgrade cleans up
after itself.

**The cabinet toggle** sits on the picker, keyed, and appears only where
`shared_saves` is true. It states which machine the player is on and what that
costs, because a player needs to know before starting a game — discovering it
afterwards is how someone loses a high-score run they believed counted.

```
Cabinet:  [Public]  Private        (C to switch)
          Scores count.  Games cannot be saved here.
```

```
Cabinet:   Public  [Private]       (C to switch)
           Scores are private.  Games save and resume.
```

`C`, not `P`: `P` is the lobby's existing Prev-page hotkey
(`syncretro_lobby.js`'s paging block in `syncretro_lobby()`), and binding it
to the cabinet toggle too would have silently broken paging on every
shared-saves console.

Both description lines read as a **rule**, not a status. "No saved games."
parses on a fast read as "none saved yet" rather than "you cannot save
here", and that misreading costs a player the game they believed was
saved.

### 5. Where the preference lives

`data/user/<####>.ini`, section `[syncretro]`, one key per console, via the
stock `exec/load/userprops.js`:

```ini
[syncretro]
	cabinet.arcade = private
```

`userprops.js`'s `filename()` resolves to `system.data_dir + "user/%04u.ini"`
— a **sibling** of `data/user/<####>/`, not a file inside it. That matters
twice: the per-console directory *is* the private `-home` handed to the core as
`GET_SAVE_DIRECTORY`, so a preference stored there would sit among files an
emulator writes; and it is the directory a sysop deletes to reclaim space from
a dormant player, which would otherwise silently flip that player's cabinet
back to public as a side effect of discarding their saves.

The section name follows the stock convention — modules use a section named
after themselves (`[minesweeper]`, `[logon]`, `[fseditor]`).

**Guests cannot hold a preference.** `userprops.js` short-circuits on
`UFLAG_G`: `set()` returns true without writing and `get()` returns the
default. A guest therefore always lands on the public cabinet, which is correct
rather than a limitation — a guest account is shared, so a private cabinet
keyed to it would be private in name only. The picker shows the cabinet line to
a guest without the key hint, rather than offering a toggle that does nothing.

### 6. How the door is told, and what it must never do

**The door must never inspect `-home` to decide anything.** `-home` is a
destination, not a signal. Inferring the cabinet from the path would couple the
door to the lobby's path conventions, misbehave for a door run by hand with an
arbitrary `-home`, and break silently the moment a sysop moves `data_dir`.

Permission is explicit, on the command line:

| | |
|---|---|
| `-state <key8>` present | snapshots permitted; name the file with this key, in `-home` |
| `-state` absent | no snapshot written, none looked for |

The lobby decides, because it is the only half that knows all three inputs —
the capability claim, the sysop's switch, and this player's cabinet — and it
expresses the whole decision as one flag. On the shared cabinet it omits
`-state`, and the door has no way to form an opinion.

`syncretro_door.c`'s usage text once also promised a bare `-state auto` that
derives the key by itself, for a door run by hand. It does not, and it
cannot where the flag is read: `sr_door_state_key()` is populated during
argv parsing, before `sr_config_apply()`, `rc_core_open()` and option
resolution run, so none of the three key inputs (core hash, ROM hash, the
resolved options) exist yet. Deriving the key there would mean restructuring
door setup ordering the rest of `main.c` depends on, for a path the lobby
never takes. A bare `-state auto` still works exactly as any other
`-state <key>` does — it permits snapshots and names the file with the
literal key `auto`, producing an inert `<rom>.auto.state` the lobby never
looks for — so nothing breaks; only the promise that it derives anything is
gone.

Passing the key rather than having the door recompute it also avoids re-hashing
a ROM the discovery cache already hashed, and costs about 14 characters of a
command line Synchronet truncates at 260 bytes on Windows.

The door keeps one veto of its own, and it is a runtime fact rather than a
path: a core reporting `retro_serialize_size() == 0` gets no snapshot whatever
it was told.

### 7. Two switches, because they are two different claims

**`[console] save_state`** — the capability claim: *this core and driver
snapshot reliably.* Ours to state, shipped in `syncretro.ini`, with a
per-romset override in `games.ini` / `games.local.ini`.

- `syncnes`, `syncivision`: `true`. One core, one driver, reliable.
- `syncarcade`: **`false`**, for every romset, at ship. Save-state support in
  the MAME 2003-Plus lineage is **per-driver across some 5000 drivers**, and a
  partially-supporting driver restores a subtly broken machine that fails as a
  wrong-looking game rather than as an error. Nothing here has been verified,
  so nothing here claims to be.

**What the arcade gets at ship, and why that is still the feature that was
asked for.** The request was for saved games *and* a way out of the shared
high-score table. The cabinet toggle delivers the second on its own: on a
private cabinet the NVRAM is the player's, so their scores persist and are
their own, with no snapshot involved. Suspend/resume then arrives per romset
as entries are verified, which is a **data** change to `games.ini` and not a
code change — so deferring verification locks in nothing and costs no rework.
A sysop who wants to enable a romset before anyone has verified it can do so in
`games.local.ini`, which is what that file is for.

**Verification, when it happens, is bounded by `games.ini`.** A tool that
decides `save_state` in bulk runs against the sections already in that file,
because a section there means someone has actually run that cabinet through the
door on a terminal (`GAMES_INI.md` §14). Probing a whole romset collection
would produce verdicts for games nobody can play well anyway, and shipping
`save_state = true` for them would assert an endorsement the project has not
made.

The probe itself is a separate deliverable and deliberately not specified here.
The one thing worth recording so it is not got wrong later: the obvious test is
the wrong one. `retro_serialize()` -> `retro_unserialize()` -> `retro_serialize()`
inside one live process proves almost nothing, because the machine is already
in the state being restored — a driver that ignores half its state still
round-trips byte-identically, which is exactly what `GAMES_INI.md`'s
seven-romset measurement shows. What matters is a **cold** restore into a
freshly loaded machine, compared by video output over a long run, with a
control that runs the continuous path twice to prove the core is deterministic
before any verdict is trusted.

**`[state] auto_resume`** — the sysop's switch, shipped `true` in
`syncretro.ini` and overridable in `syncretro.local.ini` like every other
setting. Turning it off disables suspend/resume install-wide. `[state]` is a
new section, and since the lobby is what acts on it, `syncretro_lobby_ini()`
gains it alongside the five it already merges.

They are separate because they are owned by different people and mean different
things. With one key, a sysop who simply does not want resume on their NES
would have to write `save_state = false`, recording a false claim about the
core that whoever reads or copies that file later inherits.

Suspend/resume happens when capability **and** enablement **and** a private
`-home` all hold — each decided by whoever knows the answer: us for the driver,
the sysop for the policy, the player for the cabinet.

### 8. Failure handling

Every failure is non-fatal and logged, matching the door's existing config
philosophy ("a door that cannot create its save dir should still let the player
play").

| failure | behavior |
|---|---|
| snapshot write fails | log; the session ends normally, the suspend is lost |
| `retro_unserialize()` fails | discard the blob, toast the player once, start the game fresh |
| key mismatch | never reaches restore; not marked, not loaded, deleted by the lobby |
| `retro_serialize_size() == 0` | no snapshot; nothing is marked for that console |
| preference file missing or unreadable | **public** cabinet |

The default on a lost preference is public deliberately: a player whose
preference is gone lands on the shared cabinet competing for high scores,
rather than practising alone while believing their scores count.

## Testing

The risk worth the most test effort is **the two halves computing the same
key** — the lobby computes it for the picker, the door writes the file with it,
and a divergence marks games the door then refuses to resume. A test computes
the key on both sides from fixed inputs and asserts they match.

Beyond that:

- a snapshot round-trips through write and restore;
- a changed core hash unmarks the cartridge and deletes the stale file;
- the shared cabinet writes no snapshot;
- `save_state = false` writes none; `auto_resume = false` writes none;
- a guest gets the public cabinet and no toggle;
- the picker's marking costs exactly one directory read regardless of cartridge
  count.

## Deferred

**Snapshot growth is unbounded.** One file per ROM per user, at the 32-324 KB
`GAMES_INI.md` measured, with no cap and no expiry. Stale snapshots self-clean
on a core upgrade, but a player rotating through many games on a stable install
accumulates indefinitely — order 5 MB for fifty arcade games on one console for
one player, all under `data/user/`. A retention cap (count or bytes, sysop-set)
is the obvious answer if it ever bites. Recorded rather than built.

## Correction to the consolidation design

`2026-08-02-syncretro-config-consolidation-design.md`'s follow-on section
predicts that this feature would read `shared_saves` from `[console]` **in both
halves of the door**. It does not. Because the lobby expresses its whole
decision as the presence or absence of `-state`, only the lobby reads
`shared_saves`, exactly as it does today. The door needs no `shared_saves`
getter — which is also what keeps §6's rule enforceable.
