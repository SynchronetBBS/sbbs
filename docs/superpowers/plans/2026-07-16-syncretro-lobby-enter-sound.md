# SyncRetro Lobby enter_sound Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Play a short, sysop-supplied sound when a player enters a SyncRetro console lobby (SyncNES, SyncIvision), matching SyncDuke and SyncDOOM.

**Architecture:** The mechanism already exists and is already shared. `exec/load/game_lobby.js` exposes `enter_sound(door_dir, cfg)`, which reads `cfg.lobby.enter_sound`, probes the terminal, and plays the file. `exec/load/syncretro_lobby.js:44` already loads `game_lobby.js` as `syncretro_lobby_gl`. So this is one call, one extra ini section read, docs in two example inis, and stubs so the headless harness survives the feature being used.

**Tech Stack:** Synchronet JavaScript (SpiderMonkey 1.8.5), `jsexec`, `exec/load/game_lobby.js`, `exec/load/cterm_lib.js` (SyncTERM audio APCs).

**Spec:** `docs/superpowers/specs/2026-07-16-syncretro-lobby-enter-sound-design.md`

## Global Constraints

- **SpiderMonkey 1.8.5 only.** No `let`/`const`, no arrow functions, no `Array.prototype.forEach` on host objects, no getters/setters, no trailing commas. `var` and `function` only.
- **Match the file's existing style.** These files use tabs for indentation and `/* ... */` block comments in the shared `exec/load/` modules. Follow what is already there; do not reformat.
- **Nothing audio ships.** Do not add, fetch, or commit any sound file. Do not add `.gitignore` entries for one either.
- **Commit directly on `master`.** Do not create a branch. Do not `git push`. Do not `git commit --amend` (the index is shared with the user).
- **Before every commit, run `git diff --cached --stat`** and confirm only your own paths are staged.
- **Commit messages wrap at 78 columns.** Verify with `awk 'length > 78' <msgfile>` printing nothing, and commit with `git commit -F <msgfile>`.
- **The headless test reads the LIVE install** (`/sbbs/xtrn/syncivision/`) and refuses to run while `/sbbs/data/syncretro/plays.jsonl` exists. Save and restore it around every run (Task 1, Step 2 shows the exact incantation). It is the user's real activity log — never leave it deleted.

---

### Task 1: The lobby plays a configured enter_sound

**Files:**
- Modify: `exec/load/syncretro_lobby.js:52` (module var list), `:70-83` (ini read), `:328` (call site — the line `filter = roms;` immediately after `syncretro_lobby_number(roms);`)
- Test: `xtrn/syncivision/test_lobby_headless.js` (`fake_console` at `:32`, plus a new test section at the end)

**Interfaces:**
- Consumes: `syncretro_lobby_gl.enter_sound(door_dir, cfg)` from `exec/load/game_lobby.js:621` — returns nothing; no-op unless `cfg.lobby.enter_sound` is set AND the terminal reports libsndfile.
- Produces: `syncretro_lobby_cfg` — a module-scope object of shape `{ lobby: { enter_sound: <string|undefined> } }`. Always an object with a `lobby` object, never null, even when `syncretro.ini` is absent or unreadable. Task 2 documents the key that populates it.

- [ ] **Step 1: Write the failing test**

In `xtrn/syncivision/test_lobby_headless.js`, add a `wrote` capture buffer beside the existing `out`/`keys`/`launched`/`logged` declarations near the top:

```js
var wrote = [];     // raw bytes the lobby wrote to the "terminal" (audio APCs land here)
```

Add this section at the end of the file, immediately BEFORE the `if (LIVE_PLAYS !== "" && file_exists(LIVE_PLAYS))` cleanup block:

```js
writeln("8. no entry sound reaches a terminal that cannot decode audio");
out = []; launched = []; wrote = [];
keys = ["Q"];
try {
	load(js.exec_dir + "lobby.js");
	check(true, "entering the lobby did not throw with the audio probe stubbed");
} catch (e) {
	check(false, "threw: " + e);
}
// The stub console answers the libsndfile probe with silence, so cterm_lib.js
// reports "no audio" and enter_sound must upload and queue nothing. This is the
// guard that keeps the harness honest once a sysop sets [lobby] enter_sound: it
// asserts the lobby stays SILENT here, not merely that it stays quiet because
// nothing is configured.
var w8 = wrote.join("");
check(w8.indexOf("C;S;") < 0, "uploaded no audio to the terminal");
check(w8.indexOf("A;Queue") < 0, "queued no audio on any channel");
check(launched.length === 0, "played no door");
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cd /sbbs
LIVE=/sbbs/data/syncretro/plays.jsonl
SAVE=$(mktemp)                  # a scratch path; do NOT hardcode one under /tmp
restore() { rm -f "$LIVE"; [ -s "$SAVE" ] && cp "$SAVE" "$LIVE"; }
trap restore EXIT INT TERM      # restore even if jsexec dies mid-run
cp "$LIVE" "$SAVE" && rm -f "$LIVE"
jsexec /sbbs/xtrn/syncivision/test_lobby_headless.js 2>&1 | tail -12
restore; cmp -s "$SAVE" "$LIVE" && echo "--- live plays.jsonl restored intact"
```

`plays.jsonl` is the user's real activity log. Confirm the `cmp` line prints
before moving on; if it does not, stop and restore it by hand from `$SAVE`.

Expected: section 8 runs but `wrote` is never populated, because `fake_console` has no `write` — the assertions pass vacuously and prove nothing. This is the "test fails to test anything" state that Step 3 fixes. If instead it FAILS with `console.write is not a function`, that is also acceptable and Step 3 fixes it the same way.

- [ ] **Step 3: Stub the three console calls cterm_lib.js needs**

In `xtrn/syncivision/test_lobby_headless.js`, in `fake_console`, add three members. `cterm_lib.js` runs its capability probe at LOAD time (it touches `console` at its line 39 and calls `console.inkey` at its line 65), so merely loading it needs these:

```js
	// game_lobby.js's enter_sound loads cterm_lib.js, which probes the terminal
	// for libsndfile at load time. Stub what that probe touches. inkey returning
	// "" ends query_fb's read loop immediately, so the probe reports "no audio"
	// and the lobby stays silent -- no hang, no throw. Without these the harness
	// throws the first time a sysop sets [lobby] enter_sound in the live ini,
	// i.e. it would pass only until the feature is actually used.
	write:   function (s) { wrote.push(String(s)); },
	inkey:   function () { return ""; },
	clearkeybuffer: function () { },
```

- [ ] **Step 4: Run the test to verify it passes**

Use the same save/restore incantation as Step 2.

Expected: `ok: 0 failures`, and section 8 reports `uploaded no audio to the terminal` / `queued no audio on any channel`.

- [ ] **Step 5: Read the [lobby] section from syncretro.ini**

In `exec/load/syncretro_lobby.js`, add `syncretro_lobby_cfg` to the module var list at line 52:

```js
var syncretro_lobby_dir, syncretro_lobby_con, syncretro_lobby_rules, syncretro_lobby_bios, syncretro_lobby_binary, syncretro_lobby_core, syncretro_lobby_stdio, syncretro_lobby_cfg;
```

Then replace the ini-read block (currently lines 70-83) with:

```js
	/* The sysop's half of syncretro.ini. The console's half is the spec above:
	 * a sysop hides a ROM or moves the roms dir; a sysop does not redefine what
	 * an NES cartridge is. [lobby] is game_lobby.js's shape ({lobby:{...}}), so
	 * it hands straight to enter_sound the way SyncDuke and SyncDOOM hand it
	 * theirs -- always an object, so no caller has to null-check it. */
	syncretro_lobby_cfg = { lobby: {} };
	f = new File(syncretro_lobby_dir + "syncretro.ini");
	if (f.open("r")) {
		ini = f.iniGetObject("roms");
		syncretro_lobby_cfg.lobby = f.iniGetObject("lobby") || {};
		f.close();
		if (ini) {
			if (ini.dir)
				syncretro_lobby_rules.dir = String(ini.dir);
			if (ini.exclude != null)
				syncretro_lobby_rules.exclude = syncretro_list(ini.exclude);
		}
	}
```

Note both `iniGetObject` calls happen BEFORE `f.close()`.

- [ ] **Step 6: Call enter_sound on lobby entry**

In `exec/load/syncretro_lobby.js`, in `syncretro_lobby()`, find:

```js
	syncretro_lobby_number(roms);

	filter = roms;
	page   = 0;
```

and insert the call between them:

```js
	syncretro_lobby_number(roms);

	/* One-shot entry sound, if the sysop configured one -- the same helper and the
	 * same [lobby] enter_sound key SyncDuke and SyncDOOM use. Silent unless the
	 * terminal can decode audio files, and silent when no key is set (it returns
	 * before cterm_lib.js is even loaded). After the BIOS and cartridge guards
	 * above, so a fanfare never plays into an error message. */
	syncretro_lobby_gl.enter_sound(syncretro_lobby_dir, syncretro_lobby_cfg);

	filter = roms;
	page   = 0;
```

- [ ] **Step 7: Run the test to verify it still passes**

Use the same save/restore incantation as Step 2.

Expected: `ok: 0 failures`. Section 8 now exercises the real call path: `enter_sound` runs, finds no `[lobby] enter_sound` in the live ini, and returns before loading `cterm_lib.js`.

- [ ] **Step 8: Verify the SpiderMonkey 1.8.5 parse of both files**

```bash
cd /sbbs && for f in /home/rswindell/sbbs/exec/load/syncretro_lobby.js /home/rswindell/sbbs/xtrn/syncivision/test_lobby_headless.js; do
jsexec -r "var f=new File('$f'); f.open('r'); var s=f.read(); f.close(); try { new Function(s); print('OK   '+'$f'); } catch(e) { print('FAIL '+'$f'+': '+e); }" 2>&1 | grep -E '^(OK|FAIL)'
done
```

Expected: `OK` for both.

- [ ] **Step 9: Commit**

```bash
cd /home/rswindell/sbbs
git diff --cached --stat        # must be empty before staging
MSG=$(mktemp)   # scratch path, not a hardcoded /tmp file
cat > "$MSG" <<'EOF'
syncretro: play a sysop-supplied sound on lobby entry

SyncDuke and SyncDOOM play a one-shot sound when a player enters the lobby,
via game_lobby.js's shared enter_sound(); the SyncRetro consoles did not.
syncretro_lobby.js already loads game_lobby.js, so this is the same one-line
call the siblings make, plus the [lobby] section read that feeds it.

Nothing ships: silent until a sysop sets [lobby] enter_sound, and silent on a
terminal that cannot decode audio files.

The headless harness gains write/inkey/clearkeybuffer stubs. cterm_lib.js
probes the terminal at load time, so without them the harness would throw the
first time a sysop actually set the key -- passing only until the feature was
used. It now asserts the lobby uploads and queues no audio when the probe
reports none.
EOF
awk 'length > 78 { print "OVERLONG: " $0 }' "$MSG"    # must print nothing
git add exec/load/syncretro_lobby.js xtrn/syncivision/test_lobby_headless.js
git commit -F "$MSG"
```

---

### Task 2: Document `[lobby] enter_sound` in both consoles' example inis

**Files:**
- Modify: `xtrn/syncnes/syncretro.example.ini` (header line 12; new section at end of file)
- Modify: `xtrn/syncivision/syncretro.example.ini` (same two edits)

**Interfaces:**
- Consumes: `syncretro_lobby_cfg.lobby.enter_sound` from Task 1 — the key documented here is exactly the key Task 1 reads.
- Produces: nothing consumed by later tasks.

There is no test for this task; it is documentation in a template file. `[copy:]` seeds a sysop's live `syncretro.ini` from it and will not overwrite an existing one, so an installed board keeps its config.

- [ ] **Step 1: Correct the header's summary of who reads what**

Both files carry this line (line 12 of `xtrn/syncnes/syncretro.example.ini`):

```
; The door reads [audio] and [video]; the lobby reads [roms].
```

Replace it in BOTH files with:

```
; The door reads [audio] and [video]; the lobby reads [roms] and [lobby].
```

- [ ] **Step 2: Append the [lobby] section to `xtrn/syncnes/syncretro.example.ini`**

Add at the END of the file:

```ini

[lobby]
; enter_sound -- play a one-shot sound once when a player enters the lobby, on a
; SyncTERM that can decode audio files (silent on any other terminal). A path to a
; WAV/OGG/FLAC/VOC file you supply (relative to this door's dir, or absolute) -- or
; a wildcard (e.g. sounds/*.wav) from which one match is chosen at random each
; entry. Prefer OGG or WAV: those decode everywhere, while MP3 needs a newer
; libsndfile on the player's end. Blank/unset = no entry sound.
;
; NOTHING ships here (don't redistribute copyrighted sound). SyncDuke and SyncDOOM
; suggest extracting a sound from your own GRP or WAD; there is no equivalent for a
; cartridge. A console's sound is GENERATED by its sound chip while the game runs
; -- the NES's APU, the Intellivision's AY-3-8910 -- so it exists nowhere in the ROM
; as data, and no tool can pull it out. Supply your own file.
;enter_sound = sounds/*.wav
```

- [ ] **Step 3: Append the same section to `xtrn/syncivision/syncretro.example.ini`**

Identical text to Step 2. Both consoles read the same shared lobby, so the key, the wording, and the caveat are the same; only the ini they live in differs.

- [ ] **Step 4: Verify neither template grew a live config or an asset**

```bash
cd /home/rswindell/sbbs
git status --porcelain xtrn/syncnes xtrn/syncivision
```

Expected: exactly two modified `syncretro.example.ini` paths. No `syncretro.ini`, no sound files, no new directories. (The live `syncretro.ini` is deliberately untracked; do not add it.)

- [ ] **Step 5: Commit**

```bash
cd /home/rswindell/sbbs
git diff --cached --stat        # must be empty before staging
MSG=$(mktemp)   # scratch path, not a hardcoded /tmp file
cat > "$MSG" <<'EOF'
syncretro: document [lobby] enter_sound in the console templates

The key the lobby now reads, in the two example inis a sysop actually edits,
mirroring SyncDuke's wording.

With one caveat the siblings don't need: SyncDuke can tell you to extract a
quip from your own GRP and SyncDOOM a blast from your own WAD, but a cartridge
has nothing to extract. A console's sound is generated by its sound chip as the
game runs -- the NES's APU, the Intellivision's AY-3-8910 -- so it is nowhere
in the ROM as data. Saying so beats leaving a sysop hunting for a ripper that
cannot exist.
EOF
awk 'length > 78 { print "OVERLONG: " $0 }' "$MSG"    # must print nothing
git add xtrn/syncnes/syncretro.example.ini xtrn/syncivision/syncretro.example.ini
git commit -F "$MSG"
```

---

## Live verification (user, after both tasks)

Automated tests cover the silent paths only. The happy path — a sound actually
reaching a terminal — is deliberately not automated, because the only way to drive
it through the harness is to write a `[lobby]` key into the live `syncretro.ini`,
and a test must not edit a sysop's config. So the user checks it:

1. Put a short WAV in `/sbbs/xtrn/syncnes/sounds/`.
2. Set `enter_sound = sounds/*.wav` under a `[lobby]` section in
   `/sbbs/xtrn/syncnes/syncretro.ini` (the LIVE ini, not the template).
3. Enter the SyncNES lobby from SyncTERM — the sound plays once, on entry.
4. Enter from a terminal without audio support (or an older SyncTERM) — the lobby
   paints normally and silently.

## Self-review notes

- **Spec coverage:** the `[lobby]` decision, the call site after the guards, the
  no-built-in decision (nothing to implement — it is the absence of code), the
  untracked-assets rule, the inherited error handling, and the test stubs each map
  to a step above. The spec's "Out of scope" items produce no tasks by design.
- **Type consistency:** `syncretro_lobby_cfg` is declared (Task 1 Step 5), always
  `{ lobby: {} }` or better, and consumed (Task 1 Step 6) and documented (Task 2)
  under exactly that name and shape.
