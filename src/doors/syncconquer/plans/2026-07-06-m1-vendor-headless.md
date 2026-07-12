# SyncConquer M1: Vendor & Headless Foundation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps
> use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vendor Vanilla Conquer into `src/doors/syncconquer/`, build the
Red Alert engine headless (no SDL, null backends) on Linux, and provide
the sysop asset-fetch helper — ending with `vanillara` running headless
against real freeware assets on this host.

**Architecture:** Vendored subset of Vanilla Conquer (redalert/ + common/
+ build system) at a pinned upstream commit with two small recorded local
patches; out-of-source CMake build via `build.sh`; assets fetched at
install time from the archive.org freeware CD image (nothing
EA-copyrighted enters the repo). This milestone is pure foundation — no
termgfx shims yet (M2).

**Tech Stack:** C++11 (vendored engine), CMake ≥3.25 (host has 3.31.6),
gcc 14 (verified), bsdtar/libarchive (ISO extraction, verified on host),
curl.

## Global Constraints

- Upstream: https://github.com/TheAssemblyArmada/Vanilla-Conquer pinned at
  commit `7f351daed0c19d7c4764dc4ebae1a70c7809ac1f` (verified head,
  2025-11-14).
- License: GPLv3 with EA additional terms — `License.txt` must be
  preserved verbatim in the vendored tree.
- NO EA-copyrighted assets (MIX files, ISOs) may be committed to the repo.
- Vendored code keeps upstream formatting — do NOT run uncrustify on
  anything under `vanilla/`; local patches match upstream file style.
- All spec decisions in `src/doors/syncconquer/DESIGN.md` (committed
  cc8f9d105b babies-22-karaoke) govern; deferred items in `DEFERRED.md`.
- House rules: commit messages wrapped ≤78 chars (verify with awk before
  committing — write msg to a file, `awk 'length > 78 {print}' file` must
  print nothing, commit with `git commit -F`); commit direct to master;
  check `git diff --cached` is empty before staging (shared index); NEVER
  `git push` without explicit user OK; end commit messages with
  `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- Facts already verified in a scratchpad trial build on this host
  (2026-07-06): headless configure/compile/link works ONLY with the
  wwkeyboard_null.cpp patch below; the binary runs and idles (it may
  ignore SIGTERM — always use `timeout -s KILL` in smoke tests).

---

### Task 1: Vendor Vanilla Conquer subset with local patches

**Files:**
- Create: `src/doors/syncconquer/vanilla/` — copied from upstream:
  `CMakeLists.txt`, `CMakePresets.json`, `License.txt`, `README.md`,
  `cmake/`, `common/`, `redalert/`, `resources/`
  (NOT vendored: `tiberiandawn/`, `tests/`, `tools/`, `scripts/`,
  `.github/` — excluded per DESIGN.md, guarded by patch 2)
- Create: `src/doors/syncconquer/vanilla/common/wwkeyboard_null.cpp`
- Modify: `src/doors/syncconquer/vanilla/common/CMakeLists.txt` (patch 1)
- Modify: `src/doors/syncconquer/vanilla/CMakeLists.txt` (patch 2)
- Create: `src/doors/syncconquer/PROVENANCE.md`
- Create: `src/doors/syncconquer/.gitignore`

**Interfaces:**
- Consumes: nothing (first task).
- Produces: a tree where
  `cmake -B build -DBUILD_VANILLATD=OFF -DSDL2=OFF -DOPENAL=OFF`
  configures and builds target `VanillaRA` → binary `build/vanillara`.
  Task 2's build.sh and all M2 shim work build on this tree.

- [ ] **Step 1: Clone upstream at the pinned commit**

```bash
cd /tmp/claude-1000/*/*/scratchpad 2>/dev/null || cd "$SCRATCHPAD"
git clone https://github.com/TheAssemblyArmada/Vanilla-Conquer vc-vendor
cd vc-vendor
git checkout 7f351daed0c19d7c4764dc4ebae1a70c7809ac1f
git rev-parse HEAD
```

Expected: final line prints exactly
`7f351daed0c19d7c4764dc4ebae1a70c7809ac1f`.
(A clone already exists at the session scratchpad `vc/` at this exact
commit — reusable if present, but discard its `build-headless/` and
`common/wwkeyboard_null.cpp` trial artifacts and `git checkout -- .` to
undo the trial CMakeLists edit before copying.)

- [ ] **Step 2: Copy the subset into the repo**

```bash
VEND=/home/rswindell/sbbs/src/doors/syncconquer/vanilla
mkdir -p "$VEND"
cd vc-vendor   # (or the pristine scratchpad clone)
cp -r CMakeLists.txt CMakePresets.json License.txt README.md \
      cmake common redalert resources "$VEND"/
```

Expected: `ls "$VEND"` shows the 8 entries; total size ≈ 15MB
(redalert 11M + common 3.4M + the rest).

- [ ] **Step 3: Apply patch 1 — null keyboard backend source selection**

In `vanilla/common/CMakeLists.txt` find the video backend selection block
and add `wwkeyboard_null.cpp` to the no-backend branch:

```cmake
# BEFORE (upstream):
else()
    list(APPEND COMMONV_SRC video_null.cpp)
endif()

# AFTER:
else()
    list(APPEND COMMONV_SRC video_null.cpp wwkeyboard_null.cpp)
endif()
```

Rationale (verified in trial build): upstream pairs each video backend
with a keyboard backend but the null/headless branch has no keyboard
source, so `vanillara` fails to link
(`undefined reference to CreateWWKeyboardClass()`).

- [ ] **Step 4: Add the null keyboard implementation**

Create `vanilla/common/wwkeyboard_null.cpp` with exactly:

```cpp
// Null keyboard backend: satisfies CreateWWKeyboardClass() for headless
// builds with no video backend (video_null). No input is ever produced.
// Local SyncConquer addition -- not upstream. See ../PROVENANCE.md.
#include "wwkeyboard.h"

class WWKeyboardClassNull : public WWKeyboardClass
{
public:
    virtual ~WWKeyboardClassNull()
    {
    }

    virtual void Fill_Buffer_From_System(void)
    {
    }

    virtual KeyASCIIType To_ASCII(unsigned short key)
    {
        return KA_NONE;
    }
};

WWKeyboardClass* CreateWWKeyboardClass(void)
{
    return new WWKeyboardClassNull;
}
```

(WWKeyboardClass has exactly two pure virtuals: `To_ASCII` and
`Fill_Buffer_From_System` — see `common/wwkeyboard.h:874,911`. The M2
terminal keyboard shim will follow this same subclass shape.)

- [ ] **Step 5: Apply patch 2 — guard non-vendored subdirectories**

In `vanilla/CMakeLists.txt`, the block near line 166:

```cmake
# BEFORE (upstream):
add_subdirectory(common)
add_subdirectory(tiberiandawn)
add_subdirectory(redalert)
add_subdirectory(tools)

# AFTER:
add_subdirectory(common)
if(BUILD_VANILLATD OR BUILD_REMASTERTD)
    add_subdirectory(tiberiandawn)
endif()
add_subdirectory(redalert)
if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()
```

Rationale (verified): upstream adds these unconditionally; without the
guards a subset configure fails with "add_subdirectory given source
'tiberiandawn' which is not an existing directory". The guards also mean
re-vendoring `tiberiandawn/` later (syncdawn) needs no CMake change.

- [ ] **Step 6: Write PROVENANCE.md**

Create `src/doors/syncconquer/PROVENANCE.md`:

```markdown
# Vendored source provenance

## vanilla/ — Vanilla Conquer

- Upstream: https://github.com/TheAssemblyArmada/Vanilla-Conquer
- Commit: 7f351daed0c19d7c4764dc4ebae1a70c7809ac1f (2025-11-14)
- License: GPLv3 with EA additional terms (vanilla/License.txt)
- Vendored subset: CMakeLists.txt, CMakePresets.json, License.txt,
  README.md, cmake/, common/, redalert/, resources/
- Not vendored: tiberiandawn/ (deferred syncdawn title), tests/, tools/,
  scripts/, .github/

## Local patches (keep this list complete)

1. `common/CMakeLists.txt`: add `wwkeyboard_null.cpp` to the
   no-video-backend source list (upstream has no keyboard backend for
   headless builds; vanillara otherwise fails to link).
2. `common/wwkeyboard_null.cpp`: NEW FILE — null keyboard backend
   (template for the M2 terminal keyboard shim).
3. `CMakeLists.txt`: guard `add_subdirectory(tiberiandawn)` with
   `BUILD_VANILLATD OR BUILD_REMASTERTD` and `add_subdirectory(tools)`
   with `BUILD_TOOLS`, so the vendored subset configures.

## Updating

Re-vendor by diffing upstream at a new commit against this subset,
re-applying the local patches above, and updating the commit hash here.
```

- [ ] **Step 7: Add .gitignore**

Create `src/doors/syncconquer/.gitignore`:

```
build/
```

- [ ] **Step 8: Verify the vendored tree builds headless**

```bash
cd /home/rswindell/sbbs/src/doors/syncconquer/vanilla
cmake -B build -DBUILD_VANILLATD=OFF -DSDL2=OFF -DOPENAL=OFF \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ls -la build/vanillara
```

Expected: configure summary lists SDL1/SDL2/OpenAL/VanillaTD under
"Disabled features"; build completes with `[100%] Built target VanillaRA`;
`build/vanillara` exists (~21MB Release). (This build/ directly under
vanilla/ is verification-only; Task 2's build.sh places the canonical
build at `syncconquer/build/`. It is removed in Step 9 after the smoke
run.)

- [ ] **Step 9: Smoke-run the binary (no assets), then clean up**

```bash
cd /home/rswindell/sbbs/src/doors/syncconquer/vanilla
timeout -s KILL 5 ./build/vanillara ; echo "exit=$?"
rm -rf build
```

Expected: prints `exit=137` (killed by timeout while alive =
startup, static init, and the null backends all function; the engine
idles without assets rather than crashing). Any other exit code or an
abort/assert message is a failure — investigate before proceeding
(crashes are always bugs per project policy).

- [ ] **Step 10: Commit the vendor import**

```bash
cd /home/rswindell/sbbs
git diff --cached --stat   # must be empty (shared index)
git add src/doors/syncconquer/vanilla \
        src/doors/syncconquer/PROVENANCE.md \
        src/doors/syncconquer/.gitignore
```

Write the commit message to a scratch file, verify wrap (awk check from
Global Constraints), then `git commit -F <file>`. Suggested subject:
`syncconquer: vendor Vanilla Conquer @ 7f351daed0 (redalert+common)`
Body: what was vendored, the pinned commit, the three local patches (by
their PROVENANCE.md numbers), and that headless build+link was verified
on Linux/gcc14.

### Task 2: build.sh

**Files:**
- Create: `src/doors/syncconquer/build.sh` (mode 0755)

**Interfaces:**
- Consumes: Task 1's `vanilla/` tree.
- Produces: `src/doors/syncconquer/build/vanillara` — the canonical build
  output path all later milestones (deploy.js, M2 door work) rely on.
  Usage: `./build.sh [debug|clean]`.

- [ ] **Step 1: Write build.sh**

Create `src/doors/syncconquer/build.sh` (follow syncduke/build.sh
conventions — read it first at `src/doors/syncduke/build.sh` and match
its argument handling and style; the content below is the required
behavior):

```bash
#!/bin/sh
# Build the SyncConquer engine (headless vanillara) out-of-source.
# Usage: ./build.sh [debug|clean]
set -e
cd "$(dirname "$0")"

BUILD_TYPE=Release
case "$1" in
debug)
	BUILD_TYPE=Debug
	;;
clean)
	rm -rf build
	exit 0
	;;
esac

cmake -B build -S vanilla \
	-DBUILD_VANILLATD=OFF \
	-DSDL2=OFF \
	-DOPENAL=OFF \
	-DCMAKE_BUILD_TYPE=$BUILD_TYPE
cmake --build build -j"$(nproc)"
ls -la build/vanillara
```

If syncduke's build.sh style differs materially (e.g. bash vs sh, echo
banners), match syncduke — the behavior above is what must be preserved.

- [ ] **Step 2: Run it**

```bash
cd /home/rswindell/sbbs/src/doors/syncconquer
chmod 755 build.sh
./build.sh
```

Expected: ends with the `ls -la` line showing `build/vanillara`.

- [ ] **Step 3: Verify clean + debug modes**

```bash
./build.sh clean && test ! -d build && echo CLEAN-OK
./build.sh debug && ls -la build/vanillara && echo DEBUG-OK
./build.sh clean && ./build.sh   # leave a Release build in place
```

Expected: `CLEAN-OK`, then `DEBUG-OK` (debug binary is larger, ~40MB+),
then a final Release build.

- [ ] **Step 4: Commit**

Stage `src/doors/syncconquer/build.sh` (after the shared-index
`git diff --cached` check), awk-verify the message, commit. Suggested
subject: `syncconquer: build.sh for the headless engine build`

### Task 3: fetch-assets.sh + headless run with real assets

**Files:**
- Create: `xtrn/syncalert/fetch-assets.sh` (mode 0755)

**Interfaces:**
- Consumes: Task 2's `build.sh` output (for the verification run).
- Produces: `xtrn/syncalert/assets/` containing `REDALERT.MIX` (25MB core
  game data) and `MAIN.MIX` (454MB movies+music container). M2's door
  launches the engine against this directory. Script is idempotent and
  is the sysop-facing asset installer per DESIGN.md.

Facts (verified 2026-07-06 against the actual download): archive.org item
`CCRedAlert`, file `CD1.iso` (Allied disc), size 583,286,784 bytes,
sha256 `9b30966b323643d9d544b7af21caacd923a46c2f7688639591e5b59b831da4d7`.
On-disc paths: `INSTALL/REDALERT.MIX`, `MAIN.MIX` (root). CD2 (Soviet) is
NOT needed: core MIX content is identical; it differs in campaign FMV
only (deferred with campaign mode).

- [ ] **Step 1: Write fetch-assets.sh**

Create `xtrn/syncalert/fetch-assets.sh`:

```bash
#!/bin/sh
# Fetch the Red Alert freeware assets for the SyncAlert door.
#
# Downloads the RA95 freeware CD image (released as freeware by EA,
# 2008-08-31) from archive.org, verifies its checksum, and extracts the
# two MIX archives the engine needs. Nothing is redistributed with
# Synchronet; the sysop fetches EA's freeware release directly.
#
# Usage: ./fetch-assets.sh [dest-dir]   (default: ./assets)
set -e
cd "$(dirname "$0")"

DEST="${1:-./assets}"
ISO_URL="https://archive.org/download/CCRedAlert/CD1.iso"
ISO_SHA256="9b30966b323643d9d544b7af21caacd923a46c2f7688639591e5b59b831da4d7"
ISO_FILE="ra95-cd1.iso"

if [ -f "$DEST/REDALERT.MIX" ] && [ -f "$DEST/MAIN.MIX" ]; then
	echo "Assets already present in $DEST -- nothing to do."
	exit 0
fi

command -v bsdtar >/dev/null || {
	echo "ERROR: bsdtar required (Debian: apt install libarchive-tools)" >&2
	exit 1
}

if [ ! -f "$ISO_FILE" ]; then
	echo "Downloading RA95 freeware CD1 (~556MB) from archive.org..."
	curl -L -o "$ISO_FILE.part" "$ISO_URL"
	mv "$ISO_FILE.part" "$ISO_FILE"
fi

echo "Verifying checksum..."
echo "$ISO_SHA256  $ISO_FILE" | sha256sum -c - || {
	echo "ERROR: checksum mismatch -- refusing to use $ISO_FILE" >&2
	exit 1
}

echo "Extracting MIX archives..."
mkdir -p "$DEST"
bsdtar -xf "$ISO_FILE" -C "$DEST" --strip-components=1 INSTALL/REDALERT.MIX
bsdtar -xf "$ISO_FILE" -C "$DEST" MAIN.MIX

ls -la "$DEST"
echo "Done. The ISO ($ISO_FILE) may be deleted to reclaim ~556MB."
```

- [ ] **Step 2: Run it**

```bash
cd /home/rswindell/sbbs/xtrn/syncalert
chmod 755 fetch-assets.sh
./fetch-assets.sh
```

(The session scratchpad already holds a verified `ra95-cd1.iso`; copying
it next to the script first skips the 556MB re-download:
`cp "$SCRATCHPAD/ra95-cd1.iso" .`)

Expected: checksum OK; `assets/` listing shows `REDALERT.MIX` (25046328
bytes) and `MAIN.MIX` (454605294 bytes). Run it a second time — expected:
`Assets already present ... nothing to do.`

- [ ] **Step 3: Headless run against real assets**

```bash
cd /home/rswindell/sbbs/xtrn/syncalert/assets
timeout -s KILL 15 /home/rswindell/sbbs/src/doors/syncconquer/build/vanillara ; echo "exit=$?"
ls -la
```

Expected: `exit=137` (engine alive the whole 15s — with functioning null
video it proceeds past asset loading into its intro/menu loop). Check
`ls` for files the engine created (e.g. a generated `redalert.ini` or
similar config) — their presence is positive evidence it got past MIX
loading. If the engine instead exits quickly or aborts, check whether it
wants lowercase filenames (`redalert.mix`/`main.mix` symlinks) or a
writable home — diagnose, fix in the script if needed (root-cause, don't
shrug: a crash is a bug per project policy). Record the observed startup
behavior in `src/doors/syncconquer/PROVENANCE.md` under a "## Headless
startup behavior (M1 observations)" section — M2's present loop and door
lifecycle depend on knowing it.

- [ ] **Step 4: Confirm no assets are committable**

```bash
cd /home/rswindell/sbbs && git status --porcelain xtrn/syncalert/
```

Expected: shows `fetch-assets.sh` only. If `assets/` or the ISO appear,
add `xtrn/syncalert/.gitignore` with:

```
assets/
*.iso
```

- [ ] **Step 5: Commit**

Stage `xtrn/syncalert/fetch-assets.sh` (+ `.gitignore` if created) and
any PROVENANCE.md update; awk-verify; commit. Suggested subject:
`syncalert: fetch-assets helper for the RA freeware CD`

---

## Self-review notes

- Spec coverage (M1 slice only): vendoring model ✓ (Task 1), headless
  seam proof ✓ (Tasks 1–2), fetch-assets posture incl. "nothing
  EA-copyrighted enters the repo" ✓ (Task 3), build.sh house pattern ✓
  (Task 2). Deliberately absent (later milestones): all termgfx/door/
  lobby/audio/mouse work (M2/M3), dirty-rect + image cache (M4).
- No placeholders: all file contents are complete; the only
  execution-time discoveries are explicitly framed as observations to
  record (engine startup behavior with assets), not code to invent.
- Type consistency: single interface surface — the `vanillara` binary
  path `src/doors/syncconquer/build/vanillara` (Task 2 produces, Task 3
  consumes) and `xtrn/syncalert/assets/` (Task 3 produces, M2 consumes).
