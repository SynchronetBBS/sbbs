# SyncSCUMM M5 — Multi-title deployment + Flight of the Amazon Queen (design)

Date: 2026-07-18
Status: approved by user (design dialogue in session "syncscumm")
Predecessors: `2026-07-14-syncscumm-design.md` (door design), M2 video, M3 input
(`2026-07-17-syncscumm-m3-input-design.md`), M4 audio
(`2026-07-16-syncscumm-m4-audio-design.md`). BASS (sky) is now fully playable
and pushed.

## Goal

Establish the per-title deployment model the door was designed for, and use it
to bring up a **second** playable ScummVM adventure end-to-end: **Flight of the
Amazon Queen** (FOTAQ). Along the way, fix the two things BASS's ad-hoc bring-up
left behind: saves leaking into the vendored `scummvm/` tree, and BASS not
living in a clean `xtrn/<game>` package.

Acceptance bar (user-set): FOTAQ playable on **SyncTERM** and **Windows
Terminal (sixel)** — intro plays, click-to-walk + verb/inventory work, the F5
control panel + save/load work — with saves landing in the player's **per-user**
directory, not the shared tree. On an **audio** session the Talkie (speech)
version plays voice; on a **no-audio** session (terminal can't do audio, or the
player/sysop disabled it) the **Floppy** (text/captions) version is served so the
dialogue is always readable.

## Approach (user-approved)

One door binary, one package per title — the syncretro/syncmoo1 pattern, **minus
the lobby**. syncretro uses a lobby because a *console* holds many ROMs to pick
from; a ScummVM adventure is a single title, so each `xtrn/<game>` dir **is** the
game and launches straight into it. Decisions the user locked:

1. **Naming:** `xtrn/syncqueen` (and `xtrn/syncbass`), matching the `sync*` door
   family.
2. **Sequence:** stand up **Queen first** to prove the model end-to-end, **then
   re-home BASS** to `xtrn/syncbass` in the same milestone.
3. **Per-user saves:** folded in now — multi-title is exactly when a shared
   savepath becomes wrong, and it kills the leak-into-`scummvm/` at the same
   time.

The door is **already game-agnostic**: nothing hardcodes "sky". The game is
chosen by the trailing target argument (`sky` → `queen`), the startup-dir's
`scummvm.ini`, and `--path` to the game data, with shared engine-data via
`SYNCSCUMM_DATA`. So M5 is mostly **packaging + data + config + play-testing** —
the **one** door-code addition is the Talkie/Floppy data-set selection (below),
driven by the audio-availability check the door already makes for subtitles-auto.
(All curated titles render at 320×200, matching the door's fixed `SST_FB_W/H`, so
no geometry work.)

## The deployment model (mirrors `xtrn/syncmoo1`)

Each title is a repo-tracked package `xtrn/sync<game>/` containing only tracked
config + tooling; the binary and game data are runtime artifacts placed into the
same dir at deploy time.

Tracked in the repo:
- `install-xtrn.ini` — installer metadata for `install-xtrn.js`, including the
  `cmd` and (defaulted) `startup_dir`.
- `scummvm.example.ini` — the ScummVM config template for this title (the
  `[<gameid>]` game domain: target engine, description, and the per-user
  `savepath`), copied to the live/per-user location at install.
- `getdata.sh` (or `getdata.js`, matching syncmoo1) — fetches the title's
  freeware data into the package dir; idempotent; pins the download's sha256.
- `.gitignore` — ignores the fetched game data and the deployed binary.
- `README.md` — one-screen install note (matches the syncnes/syncmoo1 packages).

Placed at deploy time (gitignored):
- `scummvm` — the door binary, **symlinked** from
  `src/doors/syncscumm/build/scummvm` (one binary serves every title, exactly
  like syncretro symlinks its single binary into each `xtrn/<console>`).
- the fetched game data (e.g. FOTAQ's files).

Launch: `cmd = scummvm%. %f --path=<game-data-base> -c
%juser/%4/<gameid>/scummvm.ini <target>`, `startup_dir` = the package dir. The
`--path` given is the title's data **base** dir; the door rewrites it to the
`talkie/` or `floppy/` variant per the selection below. `%f` is the DOOR32.SYS
drop file;
`%juser/%4/<gameid>/` is the player's per-user dir (`data/user/<####>/<gameid>/`)
— the same specifier syncmoo1 and the current BASS entry already use. Engine-data
is reached via `SYNCSCUMM_DATA` pointing at the shared
`src/doors/syncscumm/scummvm/dists/engine-data` (which already ships
`queen.tbl`, `sky.cpt`, `lure.dat`, `drascula.dat`), so it is never duplicated
per title.

## Per-user saves

ScummVM writes saves to the game domain's `savepath`; unset, it falls back to
CWD (the bug that put `SKY-VM.000` in the vendored tree). Set `savepath` in each
title's per-user `scummvm.ini` (`-c %juser/%4/<gameid>/scummvm.ini`) to that same
per-user directory, so saves land in `data/user/<####>/<gameid>/` and never in
the shared package or vendored tree. This is the long-deferred per-user-save item
(`install-xtrn.ini:47` TODO), resolved for every title at once by making it part
of the package template.

Open sub-point for the plan: whether the per-user `scummvm.ini` is
materialized/seeded on first launch (a small door-side or install-time step) vs.
assumed present — the plan pins the mechanism, following how the current BASS
`-c %juser/%4/scumm/scummvm.ini` entry expects it.

## Flight of the Amazon Queen specifics

- Data: **both** English builds, from
  `https://downloads.scummvm.org/frs/extras/Flight%20of%20the%20Amazon%20Queen/`,
  unzipped into `talkie/` and `floppy/` subdirs of the package's game-data dir:
  - **`FOTAQ_Talkie-1.0.zip`** (verified 200, ~36.5 MB) — speech.
  - **`FOTAQ_Floppy.zip`** (verified 200) — text/captions.
  ScummVM publishes a `.zip.sha256` next to each, so `getdata.sh` fetches and
  verifies against the **official** sha256 (no hand-computed hash), mirroring
  `fetch_bass.sh`'s verify step.
- Config: `scummvm.example.ini` with a `[queen]` domain (`gameid=queen`,
  `savepath=` → per-user); `path=` is set by the door's per-session Talkie/Floppy
  selection, not fixed here. Subtitles left to the door's existing auto/sysop/user
  resolution (auto → on for the Floppy/no-audio session anyway).
- Engine + engine-data (`queen.tbl`) already compiled/shipped — no build change.

### Talkie vs Floppy selection (user decision)

Each title ships **both** data sets, and the door picks which to load **per
session, at startup**, from the same audio-availability determination it already
makes for subtitles-auto:

- **Audio session** (terminal supports the digital audio tier AND the player/
  sysop hasn't disabled it) → the **Talkie** data set: the player *hears* the
  voice acting.
- **No-audio session** (terminal can't play audio, `[audio] enabled = false`, or
  the codec probe is denied) → the **Floppy** data set: dialogue is on-screen
  **text**, always. The Floppy build is text-native, so — unlike relying on a
  Talkie's subtitles — the dialogue is guaranteed complete, sidestepping the
  M4/BASS trap where the CD comic intro had speech but no subtitle text and a
  no-audio player got it neither heard nor seen.

Mechanism: the decision must precede ScummVM's game detection (which consumes
`--path`), so it lives in the door's `main()` — after `sst_io_init()`, before
`scummvm_main()` — where the door already builds its own argv. The door runs the
existing bounded audio-capability probe (the same one behind
`sst_io_audio_available()`), then points `--path` at the title's `talkie/` or
`floppy/` subdirectory. The launcher passes the title's data **base** dir; the
door appends the chosen variant. `[audio] enabled` (and the terminal's real
capability) thus drive the data set and subtitles from one determination.

Both variants share the **same gameid** (`queen`), so a player's per-user save
directory (`data/user/<####>/queen/`) is shared across them and a save made on
an audio session loads on a later no-audio session (and vice-versa) — a
save-compatibility point the plan verifies in testing.

Graceful fallback: the door prefers the variant matching the audio state but
uses whichever build is actually present, so a title packaged with only one
build (a future freeware release, or a sysop who fetched just one) still runs —
it just doesn't get the split. Both titles shipped in M5 (Queen and BASS) have
both freeware builds, so both split.

## BASS re-home

Create `xtrn/syncbass/` mirroring `xtrn/syncqueen/`, and — since BASS also has
both freeware builds (**`bass-cd-1.2.zip`** CD/talkie + **`BASS-Floppy-1.3.zip`**)
— give BASS the **same Talkie/Floppy split**. Its `getdata.sh` fetches both into
`talkie/` and `floppy/`; `scummvm.example.ini` has the `[sky]` domain. This is a
bonus win: it **fixes the BASS comic-intro no-audio gap** (the CD intro's
speech-without-subtitles) by serving no-audio players the text-native Floppy
build instead of the gappy Talkie subtitles. Repoint the live `SYNCSCUMM_BASS`
xtrn entry off the ad-hoc `startup_dir=…/scummvm/` + `--path=../test/games/bass`
onto the package, with the per-user savepath. Leave the `test/games/bass` +
`test/boot_bass.sh` dev fixtures as they are — the headless CI/boot harness,
independent of the deployed package.

## Testing

- `xtrn/syncqueen/getdata.sh` idempotent-fetches both builds; a
  `test/boot_queen.sh` (mirroring `boot_bass.sh`: run the binary headless with
  `SYNCSCUMM_DUMP`, assert ≥5 non-black 320×200 PPM frames) proves Queen boots +
  renders through the real engine — run for **both** the `talkie/` and `floppy/`
  paths so each data set is known-good.
- **Selection logic:** an automated/headless check that the door loads the
  **Talkie** path when audio is available and the **Floppy** path when it is not
  (drive both by the audio-availability determination — e.g. `[audio] enabled =
  false` forces the Floppy path). This is the one new door behavior, so it gets a
  real test, not just a live glance.
- Live acceptance (user-run): FOTAQ on SyncTERM (audio) and WT-sixel — intro,
  click-to-walk, verb/inventory, F5 panel, **save then load** (confirm the save
  file appears under `data/user/<####>/queen/`, not the package/vendored tree),
  clean exit. On the audio session confirm **voice plays** (Talkie); on a
  no-audio session confirm **dialogue shows as text** (Floppy).
- **Save compatibility across variants:** save on an audio (Talkie) session, then
  load it on a no-audio (Floppy) session (and vice-versa) — same gameid `queen`,
  same per-user savepath, so the save must load cleanly. Flag if the engine
  rejects a cross-variant save.
- Regression: BASS still launches and plays from its re-homed `xtrn/syncbass`
  package, saves now landing per-user.

## Out of scope for M5

- The remaining curated titles (Lure, Drascula) — same pattern, follow-on
  milestones once Queen proves it.
- Any lobby/picker (explicitly not wanted).
- Commercial/sysop-supplied SCUMM titles.
- Door-code/engine changes beyond the Talkie/Floppy data-set selection — the
  selection logic is the one planned door change; anything else (a title-specific
  door bug Queen surfaces) is handled as a defect within M5 but isn't planned
  work.
- The runtime-state `.gitignore` for the vendored `scummvm/` tree and the
  quantizer-stability gate (separate deferred items).
