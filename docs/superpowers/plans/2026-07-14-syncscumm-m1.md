# SyncSCUMM M1 Implementation Plan — vendor + backend skeleton + headless BASS boot

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vendor ScummVM v2026.3.0 (pruned to the curated engine set), register a
Synchronet backend skeleton in its build, and boot Beneath a Steel Sky headless to
dumped PPM frames — proven by a repeatable test script.

**Architecture:** ScummVM's `OSystem` is its published porting API. Our backend
(`OSystem_Synchronet`) starts as a null-backend clone whose graphics manager keeps a
real 8-bit framebuffer + palette and dumps PPM frames when `SYNCSCUMM_DUMP` is set.
All our C++ lives in `src/doors/syncscumm/door/`; a directory symlink at
`scummvm/backends/platform/synchronet` plus one `configure` case registers it.

**Tech Stack:** ScummVM v2026.3.0 (GPLv2+, C++), its `configure`/make build
(out-of-tree), gcc, bash test rigs. No termgfx in M1 (that starts at M2).

## Global Constraints

- Spec: `docs/superpowers/specs/2026-07-14-syncscumm-design.md`.
- ScummVM pin: tag `v2026.3.0`, commit `fed42f2068dcafc6aafa1c28c77e4c88def74b66`.
  Tarball `https://github.com/scummvm/scummvm/archive/refs/tags/v2026.3.0.tar.gz`,
  sha256 `54ec34519be9edb24f952afda9deb9a49d5ace35c3539feaa654a09fb08ce81a`.
- Curated engines, exactly: `scumm,sky,queen,lure,drascula`.
- Canonical configure flags:
  `--backend=<null|synchronet> --disable-all-engines
  --enable-engine=scumm,sky,queen,lure,drascula --disable-detection-full`.
- BASS fixture: `https://downloads.scummvm.org/frs/extras/Beneath%20a%20Steel%20Sky/bass-cd-1.2.zip`,
  sha256 `53209b9400eab6fd7fa71518b2f357c8de75cfeaa5ba57024575ab79cc974593`.
  Never committed to git (freeware to *distribute*, still not repo content).
- `door/` C++ follows **ScummVM backend style** (tabs, their naming; it compiles
  inside ScummVM's build against ScummVM headers). Do not uncrustify vendored code.
- Never introduce reserved-namespace identifiers (no leading `_` + capital, no `__`).
- Git: work directly on master (house rule); **path-scoped `git add` only; never
  `git reset --hard` / `git checkout .` / `git stash`** (shared working tree, see
  memory `no-destructive-git-in-shared-tree`); run repo-wide `git status --short`
  before every commit and touch nothing you don't recognize; commit-message lines
  wrap at 78 chars (verify with awk before committing); NO pushes.
- All shell snippets assume `DOOR=/home/rswindell/sbbs/src/doors/syncscumm`.

---

### Task 1: Vendor pruned ScummVM v2026.3.0

**Files:**
- Create: `src/doors/syncscumm/scummvm/` (vendored tree, pruned)
- Create: `src/doors/syncscumm/PROVENANCE.md`
- Create: `src/doors/syncscumm/.gitignore`

**Interfaces:**
- Produces: a vendored tree where
  `scummvm/configure --backend=null --disable-all-engines
  --enable-engine=scumm,sky,queen,lure,drascula --disable-detection-full`
  succeeds from an out-of-tree build dir. Later tasks rely on the paths
  `scummvm/configure`, `scummvm/backends/platform/`, `scummvm/dists/engine-data/`.

- [ ] **Step 1: Fetch and verify the tarball**

```bash
DOOR=/home/rswindell/sbbs/src/doors/syncscumm
mkdir -p "$DOOR" /tmp/syncscumm-vendor && cd /tmp/syncscumm-vendor
curl -sL -o scummvm.tar.gz \
  "https://github.com/scummvm/scummvm/archive/refs/tags/v2026.3.0.tar.gz"
echo "54ec34519be9edb24f952afda9deb9a49d5ace35c3539feaa654a09fb08ce81a  scummvm.tar.gz" \
  | sha256sum -c -
```

Expected: `scummvm.tar.gz: OK` (mismatch = STOP, wrong upstream bits).

- [ ] **Step 2: Extract and prune**

```bash
cd /tmp/syncscumm-vendor && tar xzf scummvm.tar.gz
mv scummvm-2026.3.0 scummvm && cd scummvm
# engines: keep the curated five (engines/ root files are core -- keep them)
for e in engines/*/; do case "$e" in
  engines/scumm/|engines/sky/|engines/queen/|engines/lure/|engines/drascula/) ;;
  *) rm -rf "$e";; esac; done
# platform backends: keep null only (synchronet shim arrives in Task 3)
for b in backends/platform/*/; do case "$b" in
  backends/platform/null/) ;; *) rm -rf "$b";; esac; done
# dists: keep only engine-data, and only the curated engines' files
find dists -mindepth 1 -maxdepth 1 ! -name engine-data -exec rm -rf {} +
find dists/engine-data -type f \
  ! -name sky.cpt ! -name lure.dat ! -name drascula.dat ! -name queen.tbl \
  ! -name README -delete
rm -rf devtools doc po test
mv /tmp/syncscumm-vendor/scummvm "$DOOR/scummvm"
du -sh "$DOOR/scummvm"
```

Expected: tree lands at `$DOOR/scummvm`, roughly 110-120M.

- [ ] **Step 3: Verify configure runs out-of-tree (this is the task's test)**

```bash
mkdir -p /tmp/syncscumm-vendor/verify && cd /tmp/syncscumm-vendor/verify
"$DOOR/scummvm/configure" --backend=null --disable-all-engines \
  --enable-engine=scumm,sky,queen,lure,drascula --disable-detection-full \
  2>&1 | tail -2
```

Expected output ends with:
```
Creating config.h
Creating config.mk
```
(Validated on this host 2026-07-14, both pristine and pruned trees.)

- [ ] **Step 4: Write PROVENANCE.md**

Create `src/doors/syncscumm/PROVENANCE.md`:

```markdown
# SyncSCUMM provenance

## Vendored engine: ScummVM

- Upstream: https://github.com/scummvm/scummvm
- Version: tag v2026.3.0, commit fed42f2068dcafc6aafa1c28c77e4c88def74b66
- Fetched: GitHub release tarball, sha256
  54ec34519be9edb24f952afda9deb9a49d5ace35c3539feaa654a09fb08ce81a
- License: GPLv2+ (see scummvm/COPYING; compatible with this repo's use)

## Pruning (deletions only -- no upstream file is modified)

- engines/: all engine subdirectories except scumm, sky, queen, lure,
  drascula (engines/ root sources kept -- they are core).
- backends/platform/: all except null.
- dists/: all except engine-data, itself pruned to the curated engines'
  runtime files (sky.cpt, lure.dat, drascula.dat, queen.tbl, README).
- devtools/, doc/, po/, test/: removed.

Re-adding an engine = restore its engines/<id>/ dir and any
dists/engine-data file from the pinned tarball, add it to the configure
--enable-engine list in build.sh, and door-test the title(s).

## Local additions to the vendored tree

- backends/platform/synchronet -- a directory SYMLINK to ../../../door
  (all code we author lives outside the vendored tree in door/).
- configure: one added case in the backend switch (search "synchronet)").
  This is the only modified upstream file.
```

- [ ] **Step 5: Write .gitignore**

Create `src/doors/syncscumm/.gitignore`:

```
/build/
/test/games/
*.log
```

- [ ] **Step 6: Commit (path-scoped)**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
# ^ recognize every modified file before proceeding (concurrent sessions!)
git add src/doors/syncscumm/scummvm src/doors/syncscumm/PROVENANCE.md \
        src/doors/syncscumm/.gitignore
git commit -m "syncscumm: vendor ScummVM v2026.3.0 (curated engines)" \
  -m "Pruned to engines scumm/sky/queen/lure/drascula per the approved design
(docs/superpowers/specs/2026-07-14-syncscumm-design.md); deletions only,
no upstream file modified.  Pin, prune list, and re-add recipe recorded
in PROVENANCE.md."
```

---

### Task 2: build.sh — baseline null-backend build

**Files:**
- Create: `src/doors/syncscumm/build.sh`

**Interfaces:**
- Consumes: Task 1's vendored tree.
- Produces: `./build.sh [null|synchronet]` (default synchronet, falls back to
  null until Task 3 exists) producing `$DOOR/build/scummvm`. Task 3/4 rebuild
  with it; the boot test invokes `$DOOR/build/scummvm`.

- [ ] **Step 1: Write build.sh**

Create `src/doors/syncscumm/build.sh` (mode 755):

```bash
#!/bin/sh
# SyncSCUMM build: out-of-tree ScummVM build with the curated engine set.
# Usage: ./build.sh [null|synchronet]   (backend; default: synchronet)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BACKEND="${1:-synchronet}"
mkdir -p "$HERE/build"
cd "$HERE/build"
"$HERE/scummvm/configure" --backend="$BACKEND" --disable-all-engines \
  --enable-engine=scumm,sky,queen,lure,drascula --disable-detection-full
make -j"$(nproc)"
ls -la "$HERE/build/scummvm"
```

- [ ] **Step 2: Run it against the null backend (synchronet doesn't exist yet)**

```bash
cd $DOOR && ./build.sh null 2>&1 | tail -3
```

Expected: `LINK scummvm` then the `ls -la` line (~45MB binary).
(Validated on this host: full build ≈ 2 min at -j.)

- [ ] **Step 3: Smoke the binary**

```bash
$DOOR/build/scummvm --version | head -1
```

Expected: `ScummVM 2026.3.0 (<build date/time>)`.

- [ ] **Step 4: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/build.sh
git commit -m "syncscumm: out-of-tree build script (curated engine set)"
```

---

### Task 3: Register the synchronet backend (skeleton = null-equivalent)

**Files:**
- Modify: `src/doors/syncscumm/scummvm/configure` (the backend `case`, next to
  the existing `null)` arm at ~line 4340)
- Create: `src/doors/syncscumm/scummvm/backends/platform/synchronet` (symlink)
- Create: `src/doors/syncscumm/door/module.mk`
- Create: `src/doors/syncscumm/door/syncscumm.cpp`

**Interfaces:**
- Consumes: Task 2's `build.sh`.
- Produces: `./build.sh` (synchronet default) builds; `OSystem_Synchronet`
  composed exactly like `OSystem_NULL` (ModularMixerBackend +
  ModularGraphicsBackend + EventSource, POSIX FS factory, default
  saves/timer/events, null mixer). Task 4 replaces its graphics manager.
  Define guard: `USE_SYNCHRONET_DRIVER`.

- [ ] **Step 1: Add the configure case**

In `scummvm/configure`, find the `null)` arm of the backend switch (search for
`append_var DEFINES "-DUSE_NULL_DRIVER"`) and add a sibling arm directly below
it:

```sh
	synchronet)
		append_var DEFINES "-DUSE_SYNCHRONET_DRIVER"
		_text_console=yes
		;;
```

(configure line 4411 `append_var MODULES "backends/platform/$_backend"` wires
the module directory automatically — nothing else to touch.)

- [ ] **Step 2: Create the shim symlink**

```bash
ln -s ../../../door $DOOR/scummvm/backends/platform/synchronet
ls -l $DOOR/scummvm/backends/platform/synchronet
```

Expected: `synchronet -> ../../../door`.

- [ ] **Step 3: Write door/module.mk**

Create `src/doors/syncscumm/door/module.mk` (the null backend's pattern —
`MODULE` must be the in-tree path, i.e. the symlink):

```make
MODULE := backends/platform/synchronet

MODULE_OBJS := \
	syncscumm.o

# We don't use rules.mk but rather manually update OBJS and MODULE_DIRS.
MODULE_OBJS := $(addprefix $(MODULE)/, $(MODULE_OBJS))
OBJS := $(MODULE_OBJS) $(OBJS)
MODULE_DIRS += $(sort $(dir $(MODULE_OBJS)))
```

- [ ] **Step 4: Write door/syncscumm.cpp**

Create `src/doors/syncscumm/door/syncscumm.cpp` — the `OSystem_NULL`
composition under our name (ScummVM style: tabs). This is a POSIX-only door;
no Win32/Amiga branches:

```cpp
/* SyncSCUMM -- ScummVM as a Synchronet door.
 * M1 skeleton: OSystem_Synchronet is a null-equivalent backend; the
 * frame-dump graphics manager (video_dump.cpp) is swapped in by
 * initBackend(). Terminal I/O via libtermgfx arrives in M2+.
 * GPLv2+, like the ScummVM tree this compiles into.
 */

#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_stdout
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr
#define FORBIDDEN_SYMBOL_EXCEPTION_fputs
#define FORBIDDEN_SYMBOL_EXCEPTION_exit
#define FORBIDDEN_SYMBOL_EXCEPTION_time_h
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "common/scummsys.h"

#if defined(USE_SYNCHRONET_DRIVER)

#include "common/events.h"
#include "backends/modular-backend.h"
#include "backends/mutex/null/null-mutex.h"
#include "backends/saves/default/default-saves.h"
#include "backends/timer/default/default-timer.h"
#include "backends/events/default/default-events.h"
#include "backends/mixer/null/null-mixer.h"
#include "backends/graphics/null/null-graphics.h"
#include "backends/fs/posix/posix-fs-factory.h"
#include "base/main.h"

class OSystem_Synchronet : public ModularMixerBackend, public ModularGraphicsBackend, Common::EventSource {
public:
	OSystem_Synchronet();
	virtual ~OSystem_Synchronet() {}

	void initBackend() override;
	bool pollEvent(Common::Event &event) override;
	Common::MutexInternal *createMutex() override;
	uint32 getMillis(bool skipRecord = false) override;
	void delayMillis(uint msecs) override;
	void getTimeAndDate(TimeDate &td, bool skipRecord = false) const override;
	void quit() override;
	void logMessage(LogMessageType::Type type, const char *message) override;

private:
	timeval _startTime;
};

OSystem_Synchronet::OSystem_Synchronet() {
	_fsFactory = new POSIXFilesystemFactory();
}

void OSystem_Synchronet::initBackend() {
	gettimeofday(&_startTime, 0);
	_savefileManager = new DefaultSaveFileManager();
	_timerManager = new DefaultTimerManager();
	_eventManager = new DefaultEventManager(this);
	_mixerManager = new NullMixerManager();
	_mixerManager->init();
	_graphicsManager = new NullGraphicsManager();	// Task 4 swaps this
	BaseBackend::initBackend();
}

bool OSystem_Synchronet::pollEvent(Common::Event &event) {
	((DefaultTimerManager *)getTimerManager())->checkTimers();
	((NullMixerManager *)_mixerManager)->update(1);
	return false;
}

Common::MutexInternal *OSystem_Synchronet::createMutex() {
	return new NullMutexInternal();
}

uint32 OSystem_Synchronet::getMillis(bool skipRecord) {
	timeval now;
	gettimeofday(&now, 0);
	return (uint32)((now.tv_sec - _startTime.tv_sec) * 1000 +
		(now.tv_usec - _startTime.tv_usec) / 1000);
}

void OSystem_Synchronet::delayMillis(uint msecs) {
	usleep(msecs * 1000);
}

void OSystem_Synchronet::getTimeAndDate(TimeDate &td, bool skipRecord) const {
	time_t curTime = time(0);
	struct tm t = *localtime(&curTime);
	td.tm_sec = t.tm_sec;
	td.tm_min = t.tm_min;
	td.tm_hour = t.tm_hour;
	td.tm_mday = t.tm_mday;
	td.tm_mon = t.tm_mon;
	td.tm_year = t.tm_year;
	td.tm_wday = t.tm_wday;
}

void OSystem_Synchronet::quit() {
	destroy();
	exit(0);
}

void OSystem_Synchronet::logMessage(LogMessageType::Type type, const char *message) {
	FILE *output = (type == LogMessageType::kInfo || type == LogMessageType::kDebug)
		? stdout : stderr;
	fputs(message, output);
	fflush(output);
}

int main(int argc, char *argv[]) {
	g_system = new OSystem_Synchronet();
	assert(g_system);
	int res = scummvm_main(argc, argv);
	g_system->destroy();
	return res;
}

#endif /* USE_SYNCHRONET_DRIVER */
```

NOTE for the implementer: `OSystem_NULL` (scummvm/backends/platform/null/
null.cpp) is the reference for every method here. If a signature above fails
to compile against v2026.3.0 headers, match null.cpp's current signature —
null.cpp is authoritative, this listing is its transcription.

- [ ] **Step 5: Build with the synchronet backend**

```bash
cd $DOOR && rm -rf build && ./build.sh 2>&1 | tail -3
$DOOR/build/scummvm --version | head -1
```

Expected: link succeeds; `ScummVM 2026.3.0 (...)`.

- [ ] **Step 6: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/door src/doors/syncscumm/scummvm/configure \
        src/doors/syncscumm/scummvm/backends/platform/synchronet
git commit -m "syncscumm: register Synchronet backend (null-equivalent skeleton)" \
  -m "One configure case + a directory symlink into door/, where all our
C++ lives (the only vendor-tree touches; PROVENANCE.md documents both).
OSystem_Synchronet mirrors OSystem_NULL pending the M1 frame-dump
graphics manager."
```

---

### Task 4: Frame-dump graphics manager + headless BASS boot test (TDD)

**Files:**
- Create: `src/doors/syncscumm/test/fetch_bass.sh`
- Create: `src/doors/syncscumm/test/boot_bass.sh`
- Create: `src/doors/syncscumm/door/video_dump.h`
- Create: `src/doors/syncscumm/door/video_dump.cpp`
- Modify: `src/doors/syncscumm/door/syncscumm.cpp` (swap graphics manager)
- Modify: `src/doors/syncscumm/door/module.mk` (add video_dump.o)

**Interfaces:**
- Consumes: Task 3's backend; `SyncscummDumpGraphicsManager` replaces
  `NullGraphicsManager` in `OSystem_Synchronet::initBackend()`.
- Produces: env contract used by all future headless rigs:
  `SYNCSCUMM_DUMP=<dir>` = write `frame%04d.ppm` (P6, one per changed
  `updateScreen()`, capped at 500). Test entry: `test/boot_bass.sh` (exit 0 =
  M1 acceptance).

- [ ] **Step 1: Write the fixture fetcher**

Create `src/doors/syncscumm/test/fetch_bass.sh` (mode 755):

```bash
#!/bin/sh
# Fetch the freeware Beneath a Steel Sky CD data into test/games/bass/
# (gitignored). Idempotent.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DEST="$HERE/games/bass"
[ -f "$DEST/sky.dsk" ] && { echo "bass: already present"; exit 0; }
mkdir -p "$DEST"
curl -sL -o "$DEST/bass.zip" \
  "https://downloads.scummvm.org/frs/extras/Beneath%20a%20Steel%20Sky/bass-cd-1.2.zip"
echo "53209b9400eab6fd7fa71518b2f357c8de75cfeaa5ba57024575ab79cc974593  $DEST/bass.zip" \
  | sha256sum -c -
unzip -o -q -j "$DEST/bass.zip" -d "$DEST"
rm "$DEST/bass.zip"
ls "$DEST"
```

- [ ] **Step 2: Write the boot test**

Create `src/doors/syncscumm/test/boot_bass.sh` (mode 755):

```bash
#!/bin/sh
# M1 acceptance: boot BASS headless, assert PPM frames come out.
# Exit 0 = pass. Requires test/games/bass (run fetch_bass.sh once).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/scummvm"
GAME="$HERE/games/bass"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$GAME/sky.dsk" ] || { echo "FAIL: run test/fetch_bass.sh first"; exit 1; }
DUMP=$(mktemp -d)
INI=$(mktemp)
trap 'rm -rf "$DUMP" "$INI"' EXIT
SYNCSCUMM_DUMP="$DUMP" timeout 20 "$BIN" --path="$GAME" \
  --extrapath="$DOOR/scummvm/dists/engine-data" -c "$INI" sky \
  > "$DUMP/boot.log" 2>&1 || true   # timeout kill (124/143) is the normal end
FRAMES=$(ls "$DUMP" | grep -c '^frame[0-9]*\.ppm$' || true)
[ "$FRAMES" -ge 5 ] || { echo "FAIL: only $FRAMES frames dumped"; exit 1; }
FIRST="$DUMP/$(ls "$DUMP" | grep '^frame[0-9]*\.ppm$' | sort | head -1)"
HDR=$(head -c 32 "$FIRST" | tr '\n' ' ')
case "$HDR" in "P6 320 200 255 "*) ;; *) echo "FAIL: bad PPM header: $HDR"; exit 1;; esac
# at least one frame must have non-black content (intro art)
for f in "$DUMP"/frame*.ppm; do
	if [ "$(tail -c +16 "$f" | tr -d '\0' | wc -c)" -gt 1000 ]; then
		echo "BOOT-BASS OK ($FRAMES frames)"; exit 0
	fi
done
echo "FAIL: all frames black"; exit 1
```

- [ ] **Step 3: Run the test — it must FAIL (no dump manager yet)**

```bash
$DOOR/test/fetch_bass.sh && $DOOR/test/boot_bass.sh; echo "exit=$?"
```

Expected: `FAIL: only 0 frames dumped`, `exit=1`.

- [ ] **Step 4: Write door/video_dump.h**

```cpp
/* SyncSCUMM M1: a graphics manager that KEEPS the pixels.
 * Maintains the 8-bit game surface + palette; when the SYNCSCUMM_DUMP
 * environment variable names a directory, each changed updateScreen()
 * writes frame%04d.ppm there (capped). M2 replaces the PPM writer with
 * the termgfx dirty-rect encoder -- the surface/palette bookkeeping here
 * is the part that carries forward.
 */
#ifndef SYNCSCUMM_VIDEO_DUMP_H_
#define SYNCSCUMM_VIDEO_DUMP_H_

#include "backends/graphics/null/null-graphics.h"
#include "graphics/surface.h"

class SyncscummDumpGraphicsManager : public NullGraphicsManager {
public:
	SyncscummDumpGraphicsManager();
	~SyncscummDumpGraphicsManager() override;

	void initSize(uint width, uint height, const Graphics::PixelFormat *format = NULL) override;
	int16 getHeight() const override { return _height; }
	int16 getWidth() const override { return _width; }
	void setPalette(const byte *colors, uint start, uint num) override;
	void grabPalette(byte *colors, uint start, uint num) const override;
	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) override;
	Graphics::Surface *lockScreen() override;
	void unlockScreen() override;
	void updateScreen() override;

private:
	void dumpFrame();

	Graphics::Surface _screen;
	byte _palette[256 * 3];
	uint _width, _height;
	bool _dirty;
	int _frameNo;
	const char *_dumpDir;	// getenv("SYNCSCUMM_DUMP"); NULL = no dumps
};

#endif /* SYNCSCUMM_VIDEO_DUMP_H_ */
```

- [ ] **Step 5: Write door/video_dump.cpp**

```cpp
#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "common/scummsys.h"

#if defined(USE_SYNCHRONET_DRIVER)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_dump.h"

#define SYNCSCUMM_DUMP_CAP 500

SyncscummDumpGraphicsManager::SyncscummDumpGraphicsManager()
	: _width(0), _height(0), _dirty(false), _frameNo(0) {
	memset(_palette, 0, sizeof(_palette));
	_dumpDir = getenv("SYNCSCUMM_DUMP");
}

SyncscummDumpGraphicsManager::~SyncscummDumpGraphicsManager() {
	_screen.free();
}

void SyncscummDumpGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	_width = width;
	_height = height;
	_screen.create(width, height, Graphics::PixelFormat::createFormatCLUT8());
	_dirty = true;
}

void SyncscummDumpGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	memcpy(_palette + start * 3, colors, num * 3);
	_dirty = true;
}

void SyncscummDumpGraphicsManager::grabPalette(byte *colors, uint start, uint num) const {
	memcpy(colors, _palette + start * 3, num * 3);
}

void SyncscummDumpGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	if (!_screen.getPixels())
		return;
	const byte *src = (const byte *)buf;
	for (int row = 0; row < h; row++)
		memcpy(_screen.getBasePtr(x, y + row), src + row * pitch, w);
	_dirty = true;
}

Graphics::Surface *SyncscummDumpGraphicsManager::lockScreen() {
	return &_screen;
}

void SyncscummDumpGraphicsManager::unlockScreen() {
	_dirty = true;
}

void SyncscummDumpGraphicsManager::updateScreen() {
	if (!_dirty)
		return;
	_dirty = false;
	dumpFrame();
}

void SyncscummDumpGraphicsManager::dumpFrame() {
	if (!_dumpDir || _frameNo >= SYNCSCUMM_DUMP_CAP || !_screen.getPixels())
		return;
	char path[1024];
	snprintf(path, sizeof(path), "%s/frame%04d.ppm", _dumpDir, _frameNo);
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return;
	fprintf(fp, "P6\n%u %u\n255\n", _width, _height);
	const byte *px = (const byte *)_screen.getPixels();
	for (uint i = 0; i < _width * _height; i++)
		fwrite(_palette + px[i] * 3, 1, 3, fp);
	fclose(fp);
	_frameNo++;
}

#endif /* USE_SYNCHRONET_DRIVER */
```

- [ ] **Step 6: Wire it in**

In `door/module.mk`, extend the object list:

```make
MODULE_OBJS := \
	syncscumm.o \
	video_dump.o
```

In `door/syncscumm.cpp`: add `#include "video_dump.h"` after the
null-graphics include, and in `initBackend()` replace

```cpp
	_graphicsManager = new NullGraphicsManager();	// Task 4 swaps this
```
with
```cpp
	_graphicsManager = new SyncscummDumpGraphicsManager();
```

- [ ] **Step 7: Rebuild and run the test — it must PASS**

```bash
cd $DOOR && ./build.sh 2>&1 | tail -2 && ./test/boot_bass.sh
```

Expected: `BOOT-BASS OK (<N> frames)` with N ≥ 5. Eyeball one frame if
curious: any `frame*.ppm` from a re-run with SYNCSCUMM_DUMP pointed at a kept
dir shows the BASS intro art.

- [ ] **Step 8: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/door src/doors/syncscumm/test
git commit -m "syncscumm: M1 frame-dump graphics manager + headless BASS boot test" \
  -m "The backend now keeps the 8-bit surface + palette (the bookkeeping M2's
termgfx encoder consumes) and, with SYNCSCUMM_DUMP set, writes PPM
frames.  test/boot_bass.sh is the M1 acceptance: boot the freeware
Beneath a Steel Sky data (test/fetch_bass.sh, gitignored) headless and
assert real 320x200 frames emerge."
```

---

### Task 5: DESIGN.md + M1 wrap

**Files:**
- Create: `src/doors/syncscumm/DESIGN.md`

**Interfaces:**
- Consumes: the committed spec.
- Produces: the in-tree design doc the siblings carry (readers land here, not
  in docs/superpowers/).

- [ ] **Step 1: Write DESIGN.md**

Copy `docs/superpowers/specs/2026-07-14-syncscumm-design.md` verbatim to
`src/doors/syncscumm/DESIGN.md`, then edit the two header lines to:

```markdown
# SyncSCUMM — ScummVM point-and-click adventures as a Synchronet door

Status: M1 complete (vendor + backend skeleton + headless BASS boot)
Date: 2026-07-14
```

- [ ] **Step 2: Verify the full M1 acceptance one last time**

```bash
cd $DOOR && ./test/boot_bass.sh && ./build.sh null >/dev/null 2>&1 && \
  echo BOTH-BACKENDS-BUILD
```

Expected: `BOOT-BASS OK (...)` then `BOTH-BACKENDS-BUILD` (null stays healthy
for upstream-parity debugging). Re-run `./build.sh` afterward so the working
binary is the synchronet one.

- [ ] **Step 3: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/DESIGN.md
git commit -m "syncscumm: in-tree design doc (M1 status)"
```
