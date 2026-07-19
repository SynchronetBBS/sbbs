# SyncSCUMM M5 — Multi-title deployment + Amazon Queen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring up a second playable ScummVM adventure (Flight of the Amazon Queen) end-to-end on a per-title `xtrn/<game>` deployment model, with the door serving the Talkie build to audio sessions and the Floppy build to no-audio sessions; then re-home BASS onto the same model. Fixes per-user saves along the way.

**Architecture:** One binary (`src/doors/syncscumm/build/scummvm`) symlinked into each per-title package `xtrn/sync<game>/` (the syncmoo1/syncretro pattern, no lobby). Each package fetches BOTH freeware builds into `talkie/` and `floppy/` subdirs. The door, in `main()` before ScummVM's game detection, probes audio availability (the same check behind subtitles-auto) and rewrites `--path` to the matching variant. Per-user saves via `--savepath`. The door is otherwise game-agnostic — the game is chosen by the trailing target arg.

**Tech Stack:** C (`sst_io.c` selection helper), C++ (`syncscumm.cpp` main), POSIX sh (getdata/boot scripts), Synchronet `install-xtrn.ini` / `install-xtrn.js`, ScummVM CLI (`--path`, `--savepath`, `-c`, target arg).

## Global Constraints

- **`syncscumm/door/*` is HAND-FORMATTED — do NOT run uncrustify.** Match local style, minimal churn. `strcmp(...)==0`. US spelling. No reserved identifiers. Test seams under `#ifdef SST_TEST` only.
- **`xtrn/sync<game>/` packages are repo-tracked** and mirror `xtrn/syncmoo1/` exactly in structure/section style. Game data + the deployed binary are gitignored.
- **Binary deploy is a symlink** from the build output into each `xtrn/sync<game>/scummvm`, mirroring `src/doors/syncretro/deploy.js`. One binary serves every title.
- **Data fetch verifies the OFFICIAL sha256** (ScummVM publishes `<zip>.sha256` next to each), never a hand-computed hash. Idempotent. Both builds unzip into `talkie/` and `floppy/` subdirs.
- **Per-user saves via `--savepath=%juser/%4/<gameid>/saves`** on the door cmd (per-user, overrides the CWD fallback that leaked saves into the vendored tree). Same `gameid` for both variants, so saves are shared across them.
- **Both titles have both freeware builds:** Queen — `FOTAQ_Talkie-1.0.zip` + `FOTAQ_Floppy.zip`; BASS — `bass-cd-1.2.zip` + `BASS-Floppy-1.3.zip`, all under `https://downloads.scummvm.org/frs/extras/<Title>/`.
- Commit on `master` (do NOT branch). Do NOT push (the user pushes).
- All curated titles render at 320×200 — no geometry work.

---

### Task 1: Door — Talkie/Floppy data-set selection

Add the one door-code change: pick the game-data variant from audio availability, before ScummVM detects the game. Factor the path choice into a testable C helper in `sst_io.c` (drives off a directory-exists check), call it from `main()`.

**Files:**
- Modify: `src/doors/syncscumm/door/sst_io.c` (new helper `sst_select_datadir`), `door/sst_io.h` (declare it).
- Modify: `src/doors/syncscumm/door/syncscumm.cpp` `main()` (call it, rewrite `--path`).
- Test: `src/doors/syncscumm/test/test_sst_datadir.c` (new; wired into `test/unit_sst_io.sh`).

**Interfaces:**
- Consumes: `sst_io_audio_available()` (already exists, `sst_io.h`).
- Produces:
  ```c
  /* Choose the game-data directory for this session: <base>/talkie when audio is
   * available, else <base>/floppy. Falls back to the other variant, then to base
   * itself, based on which directories actually exist (so a title packaged with
   * only one build, or a flat --path, still runs). Writes the chosen path into
   * buf and returns buf. audio is nonzero when the session can play digital audio
   * (caller passes sst_io_audio_available()). */
  const char *sst_select_datadir(const char *base, int audio, char *buf, size_t bufsz);
  ```

- [ ] **Step 1: Write the failing test**

`test/test_sst_datadir.c` — build a temp dir tree and assert selection + fallback. Use the `SST_TEST` build (no session needed; the helper only stats dirs):

```c
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "sst_io.h"

static void mkd(const char *p) { mkdir(p, 0777); }

int main(void) {
    char tmpl[] = "/tmp/sst_dd_XXXXXX";
    char *base = mkdtemp(tmpl);
    assert(base);
    char t[512], f[512], buf[600];
    snprintf(t, sizeof t, "%s/talkie", base);
    snprintf(f, sizeof f, "%s/floppy", base);

    /* both present: audio -> talkie, no-audio -> floppy */
    mkd(t); mkd(f);
    assert(strcmp(sst_select_datadir(base, 1, buf, sizeof buf), t) == 0);
    assert(strcmp(sst_select_datadir(base, 0, buf, sizeof buf), f) == 0);

    /* only talkie present: no-audio falls back to talkie */
    rmdir(f);
    assert(strcmp(sst_select_datadir(base, 0, buf, sizeof buf), t) == 0);

    /* neither present: falls back to base itself (flat --path) */
    rmdir(t);
    assert(strcmp(sst_select_datadir(base, 1, buf, sizeof buf), base) == 0);

    rmdir(base);
    return 0;
}
```

- [ ] **Step 2: Run to verify it fails**

Add a `test_sst_datadir` block to `test/unit_sst_io.sh` (mirror the `test_sst_io_canvas` block; `-DSST_TEST`), then:
Run: `cd src/doors/syncscumm && ./build.sh >/dev/null 2>&1; sh test/unit_sst_io.sh 2>&1 | tail -8`
Expected: FAIL — `sst_select_datadir` undefined.

- [ ] **Step 3: Implement the helper**

In `sst_io.c` (uses `<sys/stat.h>`; guard the trailing slash; prefer `variant`, fall back to the other, then base):

```c
static int sst_isdir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

const char *sst_select_datadir(const char *base, int audio, char *buf, size_t bufsz)
{
    const char *first  = audio ? "talkie" : "floppy";
    const char *second = audio ? "floppy" : "talkie";
    char        cand[600];

    snprintf(cand, sizeof cand, "%s/%s", base, first);
    if (sst_isdir(cand)) { snprintf(buf, bufsz, "%s", cand); return buf; }
    snprintf(cand, sizeof cand, "%s/%s", base, second);
    if (sst_isdir(cand)) { snprintf(buf, bufsz, "%s", cand); return buf; }
    snprintf(buf, bufsz, "%s", base);   /* flat --path / single-build package */
    return buf;
}
```
Declare it in `sst_io.h` (with the doc comment above).

- [ ] **Step 4: Wire it into `main()`**

In `syncscumm.cpp` `main()`, after `filteredArgv` is built and before `scummvm_main()`, rewrite the `--path=` entry:

```cpp
    // Talkie/Floppy: pick the game-data variant from this session's audio
    // availability before scummvm_main() detects the game from --path. Same
    // determination that drives subtitles-auto (sst_io_audio_available()): a
    // session that can play speech gets the Talkie build, one that cannot gets
    // the Floppy build (guaranteed on-screen text). See sst_select_datadir().
    static char pathArg[640];
    for (int i = 1; i < filteredArgc; i++) {
        if (strncmp(filteredArgv[i], "--path=", 7) != 0)
            continue;
        char chosen[600];
        sst_select_datadir(filteredArgv[i] + 7, sst_io_audio_available() != 0,
                           chosen, sizeof chosen);
        snprintf(pathArg, sizeof pathArg, "--path=%s", chosen);
        filteredArgv[i] = pathArg;
        break;
    }
```
(`<string.h>`/`<cstdio>` are already included via the door's headers; add if the build complains. `sst_io.h` is already included in the file's `extern "C"` block.)

- [ ] **Step 5: Run tests + build**

Run: `cd src/doors/syncscumm && ./build.sh 2>&1 | tail -6 && sh test/unit_sst_io.sh 2>&1 | tail -8`
Expected: clean build; `test_sst_datadir` passes; full suite passes.

- [ ] **Step 6: Commit** (no uncrustify — hand-formatted door)

```bash
cd src/doors/syncscumm
git add door/sst_io.c door/sst_io.h door/syncscumm.cpp test/test_sst_datadir.c test/unit_sst_io.sh
git commit -F - <<'EOF'
syncscumm: pick the Talkie or Floppy data set by audio availability

Before ScummVM detects the game from --path, rewrite --path to the
talkie/ or floppy/ variant of the title's data dir: talkie when the
session can play digital audio, floppy (guaranteed on-screen text) when
it cannot -- the same determination that drives subtitles-auto. Falls
back to whichever build is present, then to a flat --path, so a
single-build package still runs. Path choice is a pure, unit-tested
helper (sst_select_datadir).
EOF
```

---

### Task 2: `xtrn/syncqueen/` package

The repo-tracked Queen package, mirroring `xtrn/syncmoo1/`. Ships config + fetch tooling; the binary and data are deployed/fetched (gitignored).

**Files (all Create):**
- `xtrn/syncqueen/install-xtrn.ini`
- `xtrn/syncqueen/scummvm.example.ini`
- `xtrn/syncqueen/getdata.sh`
- `xtrn/syncqueen/.gitignore`
- `xtrn/syncqueen/README.md`

**Interfaces:** Consumes Task 1's data-set selection (the `cmd`'s `--path` is the base dir; the door appends `talkie/`|`floppy/`).

- [ ] **Step 1: `install-xtrn.ini`** — mirror `xtrn/syncmoo1/install-xtrn.ini`'s sections and comment style. The `[prog:...]` entry:

```ini
[prog:SYNCQUEEN]
name          = Flight of the Amazon Queen
cmd           = scummvm%. %f --path=. --savepath=%juser/%4/queen/saves -c %juser/%4/queen/scummvm.ini queen
type          = XTRN_DOOR32
settings      = XTRN_NATIVE | XTRN_BIN | XTRN_MULTIUSER | XTRN_NODISPLAY
execution_ars = ANSI
note          = Install the Flight of the Amazon Queen door (native, DOOR32.SYS, per-user saves).
```
`--path=.` is the package dir (startup_dir defaults to it); the door rewrites it to `./talkie` or `./floppy`. Add a `[copy:scummvm.example.ini]` → `scummvm.ini` and an `[exec:getdata.sh]` (or `getdata.js` wrapper) section modeled on syncmoo1's, plus the header comments (adjusted: FOTAQ freeware IS downloaded, unlike MoO1's sysop-supplied data). Note the `SYNCSCUMM_DATA` engine-data env in the door's own launch (the door sets it, or the cmd/ini does — match how BASS's live entry supplies engine-data; if BASS relied on `SYNCSCUMM_DATA` env, document it here).

- [ ] **Step 2: `scummvm.example.ini`** — a `[queen]` domain template:

```ini
[scummvm]
gui_return_to_launcher_at_exit=false

[queen]
description=Flight of the Amazon Queen
gameid=queen
```
(No `path=`/`savepath=` here — the door sets `--path` per variant and `--savepath` is on the cmd. Keep it minimal; ScummVM fills the rest.)

- [ ] **Step 3: `getdata.sh`** — fetch BOTH builds into `talkie/`/`floppy/`, verifying the official sha256. Mirror `test/fetch_bass.sh`, extended:

```sh
#!/bin/sh
# Fetch the freeware Flight of the Amazon Queen data (both builds) into this
# door directory: talkie/ (speech) and floppy/ (text). Idempotent. Verifies
# each zip against ScummVM's published .sha256.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BASE="https://downloads.scummvm.org/frs/extras/Flight%20of%20the%20Amazon%20Queen"
fetch() {  # <zip> <destdir> <marker-file>
  [ -f "$2/$3" ] && { echo "$2: present"; return; }
  mkdir -p "$2"
  curl -sfL -o "$2/dl.zip" "$BASE/$1"
  curl -sfL "$BASE/$1.sha256" | awk '{print $1}' | \
    (read h; echo "$h  $2/dl.zip" | sha256sum -c -)
  unzip -o -q -j "$2/dl.zip" -d "$2"
  rm "$2/dl.zip"
}
fetch "FOTAQ_Talkie-1.0.zip" "$HERE/talkie" "queen.1c"
fetch "FOTAQ_Floppy.zip"     "$HERE/floppy" "queen.1"
ls "$HERE/talkie" "$HERE/floppy"
```
(Verify the marker filenames — `queen.1c` talkie / `queen.1` floppy — during implementation by listing the unzip; adjust if ScummVM's zip layout differs. The `.sha256` file format is `<hash>  <name>`; take field 1.)

- [ ] **Step 4: `.gitignore`** — ignore the fetched data + deployed binary:
```
/talkie/
/floppy/
/scummvm
/scummvm.exe
/scummvm.ini
/*.log
```

- [ ] **Step 5: `README.md`** — one-screen install note matching `xtrn/syncnes/README.md`'s tone: what the door is, `./build.sh` in `src/doors/syncscumm`, `jsexec install-xtrn ../xtrn/syncqueen`, that `getdata.sh` downloads the freeware data, per-user saves.

- [ ] **Step 6: Fetch + sanity check**

Run: `sh xtrn/syncqueen/getdata.sh && ls xtrn/syncqueen/talkie xtrn/syncqueen/floppy`
Expected: both dirs populated; sha256 verifies. (This downloads ~45 MB total; idempotent on re-run.)

- [ ] **Step 7: Commit** (tracked files only — data is gitignored)

```bash
cd /home/rswindell/sbbs
git add xtrn/syncqueen/install-xtrn.ini xtrn/syncqueen/scummvm.example.ini xtrn/syncqueen/getdata.sh xtrn/syncqueen/.gitignore xtrn/syncqueen/README.md
git commit -m "syncscumm: add the Flight of the Amazon Queen door package"
```

---

### Task 3: Deploy the binary + `test/boot_queen.sh`

Symlink the built binary into the package, and add a headless boot test for both variants.

**Files:**
- Create/modify: `src/doors/syncscumm/deploy.js` (symlink `build/scummvm` into each live `xtrn/sync<game>/`; model on `src/doors/syncretro/deploy.js`).
- Create: `src/doors/syncscumm/test/boot_queen.sh`.

- [ ] **Step 1: `deploy.js`** — after `build.sh`, symlink `js.exec_dir + "build/scummvm"` into each SyncSCUMM live `xtrn/sync*/scummvm`. Follow syncretro's `deploy.js` structure (LIVE `xtrn/` resolution via `system.ctrl_dir + "../xtrn/"`, the symlink-timestamp caveat it documents). Enumerate the SyncSCUMM packages (`syncqueen`, `syncbass`) — a small explicit list, or by a marker file.

- [ ] **Step 2: `boot_queen.sh`** — mirror `test/boot_bass.sh`, run for BOTH variants:

```sh
#!/bin/sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/scummvm"
PKG="$DOOR/../../xtrn/syncqueen"
[ -f "$PKG/talkie/queen.1c" ] || { echo "FAIL: run xtrn/syncqueen/getdata.sh first"; exit 1; }
for V in talkie floppy; do
  DUMP=$(mktemp -d)
  SYNCSCUMM_DATA="$DOOR/scummvm/dists/engine-data" \
  SYNCSCUMM_DUMP="$DUMP" timeout 25 "$BIN" --path="$PKG/$V" queen \
    > "$DUMP/boot.log" 2>&1 || true
  N=$(ls "$DUMP" | grep -c '^frame[0-9]*\.ppm$' || true)
  [ "$N" -ge 5 ] || { echo "FAIL($V): only $N frames"; cat "$DUMP/boot.log"; exit 1; }
  head -c 15 "$DUMP/$(ls "$DUMP" | grep '^frame' | sort | head -1)" | \
    grep -q "P6 320 200" || { echo "FAIL($V): bad PPM header"; exit 1; }
  echo "BOOT-QUEEN OK ($V, $N frames)"
  rm -rf "$DUMP"
done
```
(Adjust the `queen.1c`/`queen.1` marker + the `--path` to match the real unzip layout from Task 2.)

- [ ] **Step 3: Run the boot test**

Run: `cd src/doors/syncscumm && ./build.sh >/dev/null 2>&1; sh test/boot_queen.sh`
Expected: `BOOT-QUEEN OK (talkie, …)` and `BOOT-QUEEN OK (floppy, …)`.

- [ ] **Step 4: Commit**

```bash
cd /home/rswindell/sbbs
git add src/doors/syncscumm/deploy.js src/doors/syncscumm/test/boot_queen.sh
git commit -m "syncscumm: deploy.js symlinks the binary; boot_queen tests both builds"
```

---

### Task 4: Install Queen live + live acceptance (user-run gate)

Not a code task; establishes the live door and is the milestone's play-test.

- [ ] Deploy: `cd src/doors/syncscumm && ./build.sh && jsexec deploy.js`, then `jsexec ../../exec/install-xtrn.js ../xtrn/syncqueen` (or the documented install path), and `sh xtrn/syncqueen/getdata.sh`. Confirm the live `xtrn/syncqueen/` has the `scummvm` symlink + `talkie/`+`floppy/` data + `scummvm.ini`.
- [ ] **SyncTERM (audio):** launch Queen — intro, click-to-walk, verb/inventory, F5 panel, **voice plays** (Talkie selected). Save, then **confirm the save file is under `data/user/<####>/queen/saves/`**, not the package or vendored tree.
- [ ] **WT-sixel or a no-audio session** (or `[audio] enabled=false`): launch Queen — confirm **Floppy** is selected and **dialogue shows as text** (intro + early dialogue), click-to-walk + F5 + save all work.
- [ ] **Save compatibility:** load the audio-session save on the no-audio session (and vice-versa) — same `queen` gameid + savepath, must load cleanly. Flag if rejected.
- [ ] Update project memory with the Queen result + any per-title quirks found.

---

### Task 5: `xtrn/syncbass/` package + re-home BASS

Mirror the Queen package for BASS (both builds), and move the live BASS entry onto it.

**Files:**
- Create: `xtrn/syncbass/{install-xtrn.ini,scummvm.example.ini,getdata.sh,.gitignore,README.md}` (mirror Task 2; `[sky]` domain; `getdata.sh` fetches `bass-cd-1.2.zip`→`talkie/` + `BASS-Floppy-1.3.zip`→`floppy/` with official sha256; `cmd` target `sky`, `--savepath=%juser/%4/sky/saves`, `-c %juser/%4/sky/scummvm.ini`).
- Modify: the live `/sbbs/ctrl/xtrn.ini` `SYNCSCUMM_BASS` entry (via the announced live-edit path) — repoint `startup_dir` to the `xtrn/syncbass` package, `cmd` to the package form (base `--path=.` + `--savepath` + selection-driven variant). Keep `name=Beneath a Steel Sky`.

- [ ] **Step 1:** create the `xtrn/syncbass/` package (mirror Task 2, BASS specifics). Run `sh xtrn/syncbass/getdata.sh`; confirm `talkie/` (CD) + `floppy/` populate + sha256 verifies.
- [ ] **Step 2:** `sh src/doors/syncscumm/test/boot_bass.sh` still passes (dev fixture untouched); add a `boot_sky_pkg` variant check if cheap, else rely on boot_queen's coverage of the selection path.
- [ ] **Step 3:** commit the package:
  ```bash
  cd /home/rswindell/sbbs
  git add xtrn/syncbass/
  git commit -m "syncscumm: re-home Beneath a Steel Sky into an xtrn/syncbass package"
  ```
- [ ] **Step 4 (announced live edit):** repoint the live `SYNCSCUMM_BASS` xtrn.ini entry onto the package (announce what/why before editing per the live-config-edit rule). Deploy the binary symlink into `xtrn/syncbass` (`jsexec deploy.js`).
- [ ] **Step 5 (user-run):** BASS still launches and plays from the re-homed package; **audio session → CD/talkie voice; no-audio session → Floppy text (comic intro now readable)**; saves land in `data/user/<####>/sky/saves/`. Regression-confirm BASS play is unchanged otherwise.

---

## Self-Review notes (for the executor)

- **Spec coverage:** Task 1 = the door selection; Task 2/3 = Queen package + deploy + boot; Task 4 = Queen live acceptance (audio/no-audio, save-compat); Task 5 = BASS re-home (incl. the comic-intro fix). Per-user saves = `--savepath` on every package cmd (Global Constraints + Tasks 2/5). Engine-data sharing = `SYNCSCUMM_DATA` (Task 2 Step 1).
- **Unknowns to resolve during implementation (verify, don't guess):** the exact unzip layout + marker filenames of each freeware zip (`queen.1c`/`queen.1`, BASS `sky.dsk`/floppy); how the door receives `SYNCSCUMM_DATA` in the live launch (env vs. door default — match BASS's current working entry); and `install-xtrn.js`'s exact section semantics for `[copy:]`/`[exec:]` (mirror syncmoo1). Each is a small verification against a real file/existing package, not a design choice.
- **No door behavior beyond Task 1** — everything else is packaging, data, and config.
