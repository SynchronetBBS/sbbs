# SyncRetro Suspend and Resume Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A player's game survives the end of a session — including a dropped
carrier — and resumes where they left off, on any machine that is theirs alone.

**Architecture:** The JS lobby decides whether a snapshot is permitted (console
capability, sysop switch, and which cabinet this player chose) and expresses
that single decision by passing `-state <key8>` to the door, or omitting it.
The C door obeys: with `-state` it snapshots at the one exit path every session
end converges on, and restores after the game loads; without it, it does
neither. The door never infers anything from where `-home` points.

**Tech Stack:** C99 (libretro `retro_serialize`/`retro_unserialize`, xpdev,
`src/hash/md5.c`, CMake + ctest), SpiderMonkey 1.8.5 JavaScript
(`userprops.js`, `md5_calc`).

**Spec:** `docs/superpowers/specs/2026-08-02-syncretro-save-restore-design.md`

## Global Constraints

- **JS is SpiderMonkey 1.8.5**: no `let`, no `const`, no arrow functions, no
  template literals, no `Array.prototype.includes`, no `Object.assign`.
- **C is portable across GCC, Clang and MSVC**: no reserved identifiers, no
  `goto` past an initializer, every non-void path returns, no POSIX
  `realpath()`/`access()` — use xpdev `FULLPATH()`, `fexist()`, `mkpath()`.
- **C indentation is TABS.** Run
  `uncrustify -c src/uncrustify.cfg --no-backup <files>` as the closing step of
  every task touching C.
- **The door must never inspect `-home`'s value to decide anything.** `-home` is
  where files go, not a signal. Permission comes from `-state` alone.
- **Both halves must compute the same `key8`.** The recipe is fixed in Task 1
  and pinned by a golden value both halves assert.
- **`data/` is generated runtime state; `ctrl/` is config; `exec/` is code
  only.** Address directories via `system.*_dir`, never a hardcoded path.
- Commit messages wrap at **78 columns** (`awk 'length > 78' <msgfile>` must
  print nothing) and end with:
  `Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>`
- **Commit directly to `master`.** No branch. Do not push.
- **`/home/rswindell/sbbs` has a SHARED git index.** Before every commit run
  `git diff --cached --name-only` and confirm only your files are listed. Never
  `git add -A` or `git add .`; never `--amend`, `reset --hard`, `checkout .`
  or `stash`.

## Live-system constraints — read before touching anything

This repository **is** a running BBS. Three specific traps:

- **`./build.sh` deploys.** `src/doors/syncretro/build/syncretro` is symlinked
  as the live door binary for every console. Build and test **only** into
  `build-test/`: `cmake --build build-test -j8 && (cd build-test && ctest)`.
  This has already caused one outage in this project.
- **`/share/sbbs/exec/load` is a directory symlink** to `exec/load` in this
  checkout, and two packages' `lobby.js` are symlinks into it. Edits to those
  files are live the instant they are saved. Keep every file syntactically
  valid at each save; verify with `jsexec` before committing.
- **Never touch anything under `/share/sbbs`.** Deploying is the owner's.

---

## File Structure

**Created:**
- `src/doors/syncretro/syncretro_state.c` / `.h` — key derivation, snapshot
  path, write and restore. One responsibility, testable without a core.
- `src/doors/syncretro/test_statekey.c` — key recipe, pinned to a golden value
- `exec/tests/syncretro_state_test.js` — the JS half, pinned to the SAME golden
  value

**Modified:**
- `src/doors/syncretro/syncretro_door.c` — parse `-state`
- `src/doors/syncretro/main.c` — restore after load, snapshot at `done:`
- `src/doors/syncretro/syncretro.h` — the state module's contract
- `src/doors/syncretro/CMakeLists.txt` — `syncretro_state.c`, `md5.c`, the test
- `exec/load/syncretro_lib.js` — core-hash cache, key derivation, snapshot list
- `exec/load/syncretro_lobby.js` — `[state]` section, picker marks, cabinet
  toggle, `-state` emission
- `xtrn/{syncivision,syncnes,syncarcade}/syncretro.ini` — `save_state`,
  `auto_resume`
- `src/doors/syncretro/GAMES_INI.md` — the per-romset `save_state` key

---

### Task 1: The key recipe, pinned by a golden value

This task exists first and alone because **the two halves computing different
keys is the failure this whole design is exposed to**: the lobby would mark a
cartridge as resumable and the door would then decline to resume it, with no
error anywhere. Fixing the recipe and pinning it to a constant that both halves
assert makes any future drift a test failure instead of a silent one.

**Files:**
- Create: `src/doors/syncretro/syncretro_state.c`, `syncretro_state.h`
- Create: `src/doors/syncretro/test_statekey.c`
- Create: `exec/tests/syncretro_state_test.js`
- Modify: `src/doors/syncretro/CMakeLists.txt`

**Interfaces:**
- Produces: `void sr_state_key(char out[9], const char *core_md5, const char *rom_md5, const char *opts);`
  — writes 8 lowercase hex digits plus NUL. `opts` is the resolved options
  already flattened (see the recipe below); all three inputs are ASCII.
- Produces: `syncretro_state_key(core_md5, rom_md5, opts)` in
  `exec/load/syncretro_lib.js`, returning the same 8 characters.

**THE RECIPE — implement exactly, both halves:**

1. Flatten the resolved `[options]` to `name=value` pairs, sorted by name
   ascending (byte order, not locale), joined with `\n`, no trailing newline.
   No options → the empty string.
2. Build the input string: `core_md5 + "\n" + rom_md5 + "\n" + opts`.
   Both md5s are 32 lowercase hex characters.
3. Take the MD5 of that string; `key8` is its first 8 hex digits, lowercase.

- [ ] **Step 1: Write the failing C test**

Create `src/doors/syncretro/test_statekey.c`:

```c
/* test_statekey.c -- the snapshot staleness key. The lobby computes it to
 * decide which cartridges show as suspended; the door computes or is handed it
 * to name the file. If the two ever disagree the lobby marks games the door
 * then refuses to resume, and nothing reports an error -- so both halves pin
 * the same golden value here and in exec/tests/syncretro_state_test.js.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro_state.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK_STR(got, want) \
		do { \
			const char *g_ = (got); \
			if (g_ == NULL || strcmp(g_, (want)) != 0) { \
				printf("FAIL %s:%d: got \"%s\", want \"%s\"\n", \
					   __FILE__, __LINE__, g_ ? g_ : "(null)", (want)); \
				failures++; \
			} \
		} while (0)

/* Fixed inputs. THE SAME THREE STRINGS AND THE SAME EXPECTED KEY appear in
 * exec/tests/syncretro_state_test.js -- change one, change both. */
#define CORE_MD5 "0123456789abcdef0123456789abcdef"
#define ROM_MD5  "fedcba9876543210fedcba9876543210"
#define OPTS     "mame2003-plus_skip_disclaimer=enabled\n" \
                 "mame2003-plus_skip_warnings=enabled"

static void test_golden(void)
{
	char key[9];

	sr_state_key(key, CORE_MD5, ROM_MD5, OPTS);
	/* Golden. Fill this in from the first run (Step 3) and never adjust it to
	 * match a changed implementation -- if it moves, the recipe changed and
	 * every existing snapshot on every install just became unreachable. */
	CHECK_STR(key, "REPLACE_ME");
}

/* Each input participates: change any one and the key must move. */
static void test_inputs_matter(void)
{
	char base[9], other[9];

	sr_state_key(base, CORE_MD5, ROM_MD5, OPTS);

	sr_state_key(other, "ffffffffffffffffffffffffffffffff", ROM_MD5, OPTS);
	if (strcmp(base, other) == 0) {
		printf("FAIL %s:%d: core hash does not affect the key\n",
		       __FILE__, __LINE__);
		failures++;
	}
	sr_state_key(other, CORE_MD5, "ffffffffffffffffffffffffffffffff", OPTS);
	if (strcmp(base, other) == 0) {
		printf("FAIL %s:%d: rom hash does not affect the key\n",
		       __FILE__, __LINE__);
		failures++;
	}
	sr_state_key(other, CORE_MD5, ROM_MD5, "");
	if (strcmp(base, other) == 0) {
		printf("FAIL %s:%d: options do not affect the key\n",
		       __FILE__, __LINE__);
		failures++;
	}
}

/* Shape: 8 lowercase hex digits, NUL-terminated. The filename depends on it. */
static void test_shape(void)
{
	char key[9];
	int  i;

	sr_state_key(key, CORE_MD5, ROM_MD5, "");
	if (strlen(key) != 8) {
		printf("FAIL %s:%d: length %u, want 8\n", __FILE__, __LINE__,
		       (unsigned)strlen(key));
		failures++;
	}
	for (i = 0; key[i] != '\0'; i++) {
		if (!((key[i] >= '0' && key[i] <= '9')
		      || (key[i] >= 'a' && key[i] <= 'f'))) {
			printf("FAIL %s:%d: '%c' is not lowercase hex\n",
			       __FILE__, __LINE__, key[i]);
			failures++;
			break;
		}
	}
}

int main(void)
{
	test_golden();
	test_inputs_matter();
	test_shape();
	printf("%s: %d failure(s)\n", failures ? "FAILED" : "ok", failures);
	return failures != 0;
}
```

- [ ] **Step 2: Write the header and implementation**

`src/doors/syncretro/syncretro_state.h`:

```c
/* syncretro_state.h -- the suspend/resume snapshot: its staleness key, its
 * path, and the write/restore pair.
 *
 * A libretro save-state blob carries NO version stamp, so restoring one into a
 * core, romset or option set it was not taken from feeds the emulator garbage.
 * Everything here exists to make that unreachable rather than unlikely: the key
 * derives from all three, and it is carried in the FILENAME so the lobby can
 * tell a live snapshot from a stale one with a single directory read.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#ifndef SYNCRETRO_STATE_H_
#define SYNCRETRO_STATE_H_

#include <stddef.h>

/* 8 lowercase hex digits + NUL. `opts` is the resolved [options] flattened to
 * sorted "name=value" lines joined with '\n'. exec/load/syncretro_lib.js's
 * syncretro_state_key() implements the identical recipe; the two are pinned to
 * one golden value by test_statekey.c and exec/tests/syncretro_state_test.js. */
void sr_state_key(char out[9], const char *core_md5, const char *rom_md5,
                  const char *opts);

#endif /* SYNCRETRO_STATE_H_ */
```

`src/doors/syncretro/syncretro_state.c`:

```c
/* syncretro_state.c -- see syncretro_state.h.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro_state.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "md5.h"   /* src/hash, already on termgfx's PUBLIC include path */

void sr_state_key(char out[9], const char *core_md5, const char *rom_md5,
                  const char *opts)
{
	BYTE   digest[MD5_DIGEST_SIZE];
	char * buf;
	size_t n;
	int    i;

	if (core_md5 == NULL)
		core_md5 = "";
	if (rom_md5 == NULL)
		rom_md5 = "";
	if (opts == NULL)
		opts = "";

	n   = strlen(core_md5) + 1 + strlen(rom_md5) + 1 + strlen(opts) + 1;
	buf = malloc(n);
	if (buf == NULL) {          /* no key, no snapshot -- never a wrong key */
		out[0] = '\0';
		return;
	}
	snprintf(buf, n, "%s\n%s\n%s", core_md5, rom_md5, opts);
	MD5_calc(digest, buf, strlen(buf));
	free(buf);

	for (i = 0; i < 4; i++)
		snprintf(out + i * 2, 3, "%02x", digest[i]);
	out[8] = '\0';
}
```

- [ ] **Step 3: Register in CMake and capture the golden value**

Add `syncretro_state.c` to the `syncretro` target's sources, and add the test
after the `test_config` block:

```cmake
    # test_statekey -- the snapshot staleness key. md5.c is compiled in
    # directly (one dependency-free TU) rather than add_subdirectory()ing the
    # hash lib, the same way termgfx compiles in fnv1a.c; termgfx already puts
    # src/hash on the include path.
    add_executable(test_statekey test_statekey.c syncretro_state.c
                   ${CMAKE_CURRENT_SOURCE_DIR}/../../hash/md5.c)
    target_include_directories(test_statekey PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                               ${CMAKE_CURRENT_SOURCE_DIR}/../../hash
                               ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
    add_test(NAME statekey COMMAND test_statekey)
```

Add the same two source files to the main `syncretro` executable.

Then build and run it once to obtain the golden value:

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake -S . -B build-test -DSYNCRETRO_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-test --target test_statekey -j8
./build-test/test_statekey
```

Expected: it FAILS, printing `got "<8 hex digits>", want "REPLACE_ME"`. Put the
printed value into `test_statekey.c` in place of `REPLACE_ME`, rebuild, and
confirm it passes. **Record that value — Task 2 needs the identical constant.**

- [ ] **Step 4: Write the JS half and pin it to the same value**

Add to `exec/load/syncretro_lib.js`:

```js
// The snapshot staleness key. A libretro save-state blob carries no version
// stamp, so a snapshot must not be restored into a core, romset or option set
// it was not taken from. This derives an 8-hex-digit key from all three, and it
// goes in the FILENAME so the picker can tell live from stale with one
// directory read instead of a probe per cartridge.
//
// src/doors/syncretro/syncretro_state.c's sr_state_key() implements the
// IDENTICAL recipe, and both are pinned to one golden value by
// exec/tests/syncretro_state_test.js and test_statekey.c. If these two ever
// disagree, the picker marks games the door then refuses to resume and nothing
// reports an error -- which is the whole reason for the golden value.
//
// `opts` is the resolved [options] flattened to sorted "name=value" lines
// joined with newlines. md5_calc() returns base64 unless asked for hex.
function syncretro_state_key(core_md5, rom_md5, opts)
{
	var s = String(core_md5 || "") + "\n"
	    + String(rom_md5 || "") + "\n"
	    + String(opts || "");

	return md5_calc(s, true).substr(0, 8).toLowerCase();
}

// The [options] section, flattened the way syncretro_state_key() requires:
// "name=value" per option, sorted by name in byte order, joined with newlines.
function syncretro_state_opts(options)
{
	var names = [];
	var out = [];
	var k, i;

	if (!options)
		return "";
	for (k in options)
		names.push(String(k));
	names.sort();
	for (i = 0; i < names.length; i++)
		out.push(names[i] + "=" + String(options[names[i]]));
	return out.join("\n");
}
```

Create `exec/tests/syncretro_state_test.js`:

```js
// syncretro_state_test.js -- the JS half of the snapshot staleness key.
// THE GOLDEN VALUE AND THE THREE INPUTS BELOW ARE THE SAME ONES IN
// src/doors/syncretro/test_statekey.c. Change one, change both -- and if the
// value ever moves, every existing snapshot on every install becomes
// unreachable, so it is not a number to adjust casually.
//
// Run: jsexec exec/tests/syncretro_state_test.js

load("syncretro_lib.js");

var failures = 0;

function check(cond, what)
{
	if (!cond) {
		print("FAIL: " + what);
		failures++;
	}
}

function check_str(got, want, what)
{
	if (String(got) !== String(want)) {
		print("FAIL: " + what + ": got \"" + got + "\", want \"" + want + "\"");
		failures++;
	}
}

var CORE_MD5 = "0123456789abcdef0123456789abcdef";
var ROM_MD5  = "fedcba9876543210fedcba9876543210";
var OPTS     = "mame2003-plus_skip_disclaimer=enabled\n"
    + "mame2003-plus_skip_warnings=enabled";

// The golden value, shared with the C half.
check_str(syncretro_state_key(CORE_MD5, ROM_MD5, OPTS), "REPLACE_ME",
          "golden key matches the C half");

// Every input participates.
check(syncretro_state_key("ffffffffffffffffffffffffffffffff", ROM_MD5, OPTS)
      !== syncretro_state_key(CORE_MD5, ROM_MD5, OPTS), "core hash matters");
check(syncretro_state_key(CORE_MD5, "ffffffffffffffffffffffffffffffff", OPTS)
      !== syncretro_state_key(CORE_MD5, ROM_MD5, OPTS), "rom hash matters");
check(syncretro_state_key(CORE_MD5, ROM_MD5, "")
      !== syncretro_state_key(CORE_MD5, ROM_MD5, OPTS), "options matter");

// Shape: 8 lowercase hex digits.
var k = syncretro_state_key(CORE_MD5, ROM_MD5, "");
check(k.length === 8, "key is 8 characters");
check(/^[0-9a-f]{8}$/.test(k), "key is lowercase hex");

// Flattening is order-independent in input and sorted in output, because two
// installs listing the same options differently must produce the same key.
check_str(syncretro_state_opts({ b: "2", a: "1" }), "a=1\nb=2",
          "options sort by name");
check_str(syncretro_state_opts({}), "", "no options is the empty string");
check_str(syncretro_state_opts(null), "", "absent options is the empty string");

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
```

Replace `REPLACE_ME` with the value recorded in Step 3.

- [ ] **Step 5: Run both halves**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro && cmake --build build-test -j8 \
  && (cd build-test && ctest -R statekey --output-on-failure)
cd /home/rswindell/sbbs && jsexec exec/tests/syncretro_state_test.js
```

Expected: both pass. **If the JS value differs from the C one, do not change
either golden value to match — find the difference in the recipe.** The usual
culprits are `md5_calc()`'s base64-versus-hex default and a trailing newline in
the flattened options.

- [ ] **Step 6: Format and commit**

```bash
cd /home/rswindell/sbbs
uncrustify -c src/uncrustify.cfg --no-backup \
    src/doors/syncretro/syncretro_state.c src/doors/syncretro/syncretro_state.h \
    src/doors/syncretro/test_statekey.c
git add src/doors/syncretro/syncretro_state.c \
        src/doors/syncretro/syncretro_state.h \
        src/doors/syncretro/test_statekey.c \
        src/doors/syncretro/CMakeLists.txt \
        exec/load/syncretro_lib.js exec/tests/syncretro_state_test.js
git commit   # message per Global Constraints
```

---

### Task 2: The door writes and restores a snapshot

**Files:**
- Modify: `src/doors/syncretro/syncretro_state.c` / `.h` (path, write, restore)
- Modify: `src/doors/syncretro/syncretro_door.c` (parse `-state`)
- Modify: `src/doors/syncretro/main.c` (restore after load, snapshot at `done:`)
- Modify: `src/doors/syncretro/syncretro.h` (`sr_door_state_key()`)

**Interfaces:**
- Consumes: `sr_state_key()` from Task 1.
- Produces:
  - `const char *sr_door_state_key(void);` — the `-state` argument, or NULL
    when it was not given. `"auto"` is resolved before this returns.
  - `int sr_state_path(char *out, size_t max, const char *home, const char *rom_path, const char *key8);`
    — builds `<home>/<rom-basename-without-extension>.<key8>.state`. Returns 0
    on success.
  - `int sr_state_save(rc_core_t *core, const char *path);` — 0 on success.
  - `int sr_state_load(rc_core_t *core, const char *path);` — 0 on success,
    negative on a failure the caller should report and recover from.

- [ ] **Step 1: Parse `-state` in `syncretro_door.c`**

Alongside the existing flag parsing (`-home`, `-core`, `-option`, …), add:

```c
		} else if (strcmp(argv[i], "-state") == 0 && i + 1 < argc) {
			/* The lobby's decision, arriving as one flag: present means
			 * snapshots are permitted and this is the key to name the file
			 * with; absent means neither write nor look for one. The door
			 * forms no opinion of its own -- in particular it never inspects
			 * -home to guess whether this player is on a private machine. */
			snprintf(g_state_key, sizeof g_state_key, "%s", argv[++i]);
```

with `static char g_state_key[16];` beside the other door statics, and:

```c
const char *sr_door_state_key(void)
{
	return g_state_key[0] != '\0' ? g_state_key : NULL;
}
```

Add the declaration to `syncretro.h` beside `sr_door_home()`, and a `-state`
line to the usage text at `syncretro_door.c`'s help output:

```
"  -state <key>       permit save states; <key> names the file (\"auto\" to derive)\n"
```

- [ ] **Step 2: Add path, save and restore to `syncretro_state.c`**

```c
/* <home>/<rom-basename-sans-extension>.<key8>.state
 *
 * The key is in the NAME, not inside the file: that is what lets the lobby tell
 * a live snapshot from a stale one with a single directory read rather than a
 * probe per cartridge, and it means a snapshot for a core or option set that no
 * longer applies simply never matches. */
int sr_state_path(char *out, size_t max, const char *home,
                  const char *rom_path, const char *key8)
{
	const char *base;
	const char *dot;
	char        stem[256];
	size_t      n;

	if (out == NULL || max == 0 || home == NULL || rom_path == NULL
	    || key8 == NULL || key8[0] == '\0')
		return -1;

	base = strrchr(rom_path, '/');
#ifdef _WIN32
	{
		const char *bs = strrchr(rom_path, '\\');

		if (bs != NULL && (base == NULL || bs > base))
			base = bs;
	}
#endif
	base = base != NULL ? base + 1 : rom_path;

	dot = strrchr(base, '.');
	n   = dot != NULL ? (size_t)(dot - base) : strlen(base);
	if (n >= sizeof stem)
		n = sizeof stem - 1;
	memcpy(stem, base, n);
	stem[n] = '\0';

	snprintf(out, max, "%s/%s.%s.state", home, stem, key8);
	return 0;
}

int sr_state_save(rc_core_t *core, const char *path)
{
	size_t size;
	void * buf;
	FILE * f;
	int    rc = 0;

	if (core == NULL || core->serialize_size == NULL || core->serialize == NULL)
		return -1;
	size = core->serialize_size();
	if (size == 0)          /* the core does not support save states at all */
		return -1;
	buf = malloc(size);
	if (buf == NULL)
		return -1;
	if (!core->serialize(buf, size)) {
		free(buf);
		return -1;
	}
	f = fopen(path, "wb");
	if (f == NULL) {
		free(buf);
		return -1;
	}
	if (fwrite(buf, 1, size, f) != size)
		rc = -1;
	if (fclose(f) != 0)
		rc = -1;
	free(buf);
	if (rc != 0)
		remove(path);   /* a truncated snapshot is worse than none */
	return rc;
}

int sr_state_load(rc_core_t *core, const char *path)
{
	size_t size;
	size_t got;
	void * buf;
	FILE * f;
	int    ok;

	if (core == NULL || core->unserialize == NULL)
		return -1;
	f = fopen(path, "rb");
	if (f == NULL)
		return -1;      /* no snapshot is the normal case, not an error */
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	size = (size_t)ftell(f);
	rewind(f);
	buf = size > 0 ? malloc(size) : NULL;
	if (buf == NULL) {
		fclose(f);
		return -1;
	}
	got = fread(buf, 1, size, f);
	fclose(f);
	if (got != size) {
		free(buf);
		return -1;
	}
	ok = core->unserialize(buf, size);
	free(buf);
	return ok ? 0 : -1;
}
```

Declare all three in `syncretro_state.h`, which needs `#include "retro_core.h"`
for `rc_core_t`.

- [ ] **Step 3: Wire restore into `main.c`, after the game loads**

Immediately after the `rc_core_load_game()` success path and before the frame
loop:

```c
	/* Resume, if the lobby permitted it and a snapshot for THIS core, romset
	 * and option set exists. A failure here is not fatal: the blob is dropped
	 * and the game starts from power-on, which is better than continuing into a
	 * half-restored machine. */
	if (sr_door_state_key() != NULL && sr_config_save_dir() != NULL
	    && sr_config_rom_path() != NULL) {
		char path[PATH_MAX];

		if (sr_state_path(path, sizeof path, sr_config_save_dir(),
		                  sr_config_rom_path(), sr_door_state_key()) == 0
		    && fexist(path)) {
			if (sr_state_load(&core, path) == 0) {
				fprintf(stderr, "syncretro: resumed from %s\n", path);
			} else {
				fprintf(stderr, "syncretro: %s did not restore -- "
				        "starting fresh\n", path);
				remove(path);
				sr_io_toast("Saved game could not be restored");
			}
		}
	}
```

- [ ] **Step 4: Wire the snapshot into `main.c`'s `done:` label**

At `done:`, **before** `rc_core_close(&core)` — the core must still be loaded:

```c
done:
	/* Suspend. This is the one place every session end converges on --
	 * sr_door_should_exit() covers carrier loss, Ctrl-Q, the DOOR32 time limit
	 * and the idle timeout -- so a dropped connection suspends the game exactly
	 * as cleanly as quitting does. Non-fatal: a failure costs the suspend, not
	 * the session. */
	if (sr_door_state_key() != NULL && sr_config_save_dir() != NULL
	    && sr_config_rom_path() != NULL) {
		char path[PATH_MAX];

		if (sr_state_path(path, sizeof path, sr_config_save_dir(),
		                  sr_config_rom_path(), sr_door_state_key()) == 0) {
			if (sr_state_save(&core, path) != 0)
				fprintf(stderr, "syncretro: could not write %s\n", path);
		}
	}
	sr_audio_shutdown();
```

Note the ordering: the snapshot is taken before `sr_audio_shutdown()` and
`sr_io_leave()` only in the sense that it must precede `rc_core_close()`;
placing it first at `done:` keeps the core's lifetime obvious.

- [ ] **Step 5: Build and verify nothing regressed**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake --build build-test -j8 && (cd build-test && ctest --output-on-failure)
```

Expected: all tests pass, including `statekey` and `config`.

**Do NOT run `./build.sh`.**

- [ ] **Step 6: Verify the round trip against a real core**

This is the step no unit test covers, because it needs a loaded core. The tree
vendors `freeintv_libretro.so` and `4-tris.rom` for exactly this kind of check.

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
mkdir -p /tmp/claude-1000/srstate && rm -f /tmp/claude-1000/srstate/*.state
./build-test/syncretro -core ./freeintv_libretro.so -rom ./4-tris.rom \
    -home /tmp/claude-1000/srstate -state deadbeef -stdio < /dev/null
ls -l /tmp/claude-1000/srstate/
```

Expected: a file `4-tris.deadbeef.state` exists and is non-empty. Run the same
command again and confirm the log line `syncretro: resumed from …` appears.
Record both outcomes in your report; if the door cannot run headless in this
environment, say so explicitly rather than claiming the round trip is verified.

- [ ] **Step 7: Format and commit**

```bash
cd /home/rswindell/sbbs
uncrustify -c src/uncrustify.cfg --no-backup \
    src/doors/syncretro/syncretro_state.c src/doors/syncretro/syncretro_state.h \
    src/doors/syncretro/syncretro_door.c src/doors/syncretro/main.c
git add src/doors/syncretro/syncretro_state.c \
        src/doors/syncretro/syncretro_state.h \
        src/doors/syncretro/syncretro_door.c src/doors/syncretro/main.c \
        src/doors/syncretro/syncretro.h
git commit   # message per Global Constraints
```

---

### Task 3: The lobby decides, and passes `-state`

**Files:**
- Modify: `exec/load/syncretro_lib.js` (core-hash cache, snapshot listing)
- Modify: `exec/load/syncretro_lobby.js` (`[state]`, the decision, `-state`)
- Modify: `exec/tests/syncretro_state_test.js` (extend)

**Interfaces:**
- Consumes: `syncretro_state_key()`, `syncretro_state_opts()` (Task 1); the
  door's `-state` contract (Task 2).
- Produces, with these exact names and signatures:
  - `syncretro_state_list(home)` → `{ "<rom stem>": {key, path} }`, from ONE
    `directory()` call.
  - `syncretro_state_marked(list, rom_name, key8)` → boolean.
  - `syncretro_state_sweep(list, roms, current_keys)` → count of stale snapshot
    files deleted.
  - `syncretro_core_md5(core_path, cache_path)` → 32 hex chars, cached on the
    core file's size + mtime.
  - `syncretro_lobby_state_key(rom)` → the `key8` string when a snapshot is
    permitted for this ROM, or `""` when it is not.
  - `syncretro_lobby_games_save_state(rom_name)` → `"true"`, `"false"` or `""`
    (no per-romset override).
- **Also required, and easy to miss:** the ROM objects that discovery returns
  do **not** currently carry their hash — `syncretro_lib.js` computes `full` for
  dedupe and then discards it (the `out.push({...})` around line 703 lists
  `path, name, title, year, publisher, tags, size` and no hash). Step 3a adds
  it. Without that, `syncretro_lobby_state_key()` has no ROM hash to key on.

- [ ] **Step 1: Write the failing test**

Append to `exec/tests/syncretro_state_test.js`:

```js
// The snapshot directory listing: one read, parsed into stem -> key.
var dir = system.temp_dir + "syncretro_statetest/";
var f;

mkpath(dir);
f = new File(dir + "Astrosmash.a1b2c3d4.state"); f.open("wb"); f.write("x"); f.close();
f = new File(dir + "Utopia.99887766.state");     f.open("wb"); f.write("x"); f.close();
f = new File(dir + "notasnapshot.txt");          f.open("wb"); f.write("x"); f.close();

var list = syncretro_state_list(dir);
check_str(list["Astrosmash"].key, "a1b2c3d4", "listing reads the key from the name");
check_str(list["Utopia"].key, "99887766", "listing finds the second snapshot");
check(list["notasnapshot"] === undefined, "non-.state files are ignored");
check(list["Astrosmash"].path.indexOf("Astrosmash.a1b2c3d4.state") >= 0,
      "listing carries the full path");

// A cartridge is suspended only when the key matches what would restore NOW.
check(syncretro_state_marked(list, "Astrosmash.int", "a1b2c3d4") === true,
      "matching key marks the cartridge");
check(syncretro_state_marked(list, "Astrosmash.int", "ffffffff") === false,
      "a stale key does not mark the cartridge");
check(syncretro_state_marked(list, "Nothing.int", "a1b2c3d4") === false,
      "a cartridge with no snapshot is not marked");
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cd /home/rswindell/sbbs && jsexec exec/tests/syncretro_state_test.js
```

Expected: FAIL — `syncretro_state_list is not defined`.

- [ ] **Step 3: Implement the listing in `syncretro_lib.js`**

```js
// Every snapshot in one directory read.
//
// The picker needs to know which cartridges have a resumable game, and the
// naive way to answer -- probe for a snapshot per cartridge -- is a file
// operation per ROM on every lobby entry, which is the cost the ROM hash cache
// exists to avoid (LAUNCHER.md sec 6). Because the key is in the NAME, one
// directory() call answers it for every cartridge at once.
//
// Returns { "<rom stem>": { key: "<key8>", path: "<full path>" } }.
function syncretro_state_list(home)
{
	var out = {};
	var files, i, base, m;

	if (!home)
		return out;
	files = directory(backslash(home) + "*.state");
	for (i = 0; i < files.length; i++) {
		base = file_getname(files[i]);
		m = /^(.+)\.([0-9a-f]{8})\.state$/.exec(base);
		if (m)
			out[m[1]] = { key: m[2], path: files[i] };
	}
	return out;
}

// Is this cartridge resumable RIGHT NOW? Only when a snapshot exists whose key
// matches the one the door would use, so a core upgrade unmarks everything
// rather than offering a restore that would feed the emulator garbage.
function syncretro_state_marked(list, rom_name, key8)
{
	var stem = String(rom_name).replace(/\.[^.]*$/, "");

	return !!(key8 && list && list[stem] && list[stem].key === key8);
}
```

- [ ] **Step 3a: Carry the ROM hash on the ROM object**

In `syncretro_lib.js`'s discovery pass, the object literal pushed for each
cartridge (around line 703) currently ends `tags: parsed.tags, size: size`. The
full-file md5 is already in scope as `full`. Add it:

```js
			out.push({
				path:      path,
				name:      name,
				title:     parsed.title,
				year:      parsed.year,
				publisher: parsed.publisher,
				tags:      parsed.tags,
				size:      size,
				/* The full-file hash discovery already computed for dedupe.
				 * It is one of the three inputs to the snapshot key, and
				 * recomputing it in the lobby would re-read every cartridge --
				 * exactly what the cache above exists to prevent. */
				md5:       full
			});
```

Add one assertion to `exec/tests/syncretro_state_test.js` that a discovered ROM
object carries a 32-hex-character `md5`, so this cannot silently regress.

- [ ] **Step 4: Add the core-hash cache**

```js
// The core binary's hash, cached on size + mtime.
//
// It is one of the three inputs to the snapshot key, and hashing it on every
// lobby entry would be a real cost rather than a notional one: the MAME
// 2003-Plus core is multi-megabyte. Same argument and same shape as the ROM
// hash cache above. Derived, so a lost update costs a re-hash and never a
// wrong answer; a torn, stale or missing cache reads as cold.
function syncretro_core_md5(core_path, cache_path)
{
	var cache = null;
	var size, date, f, tmp, hash;

	if (!core_path || !file_exists(core_path))
		return "";
	size = file_size(core_path);
	date = file_date(core_path);

	f = new File(cache_path);
	if (f.open("r")) {
		try { cache = JSON.parse(f.readAll().join("\n")); }
		catch (e) { cache = null; }
		f.close();
	}
	if (cache && cache.path === core_path && cache.size === size
	    && cache.date === date && /^[0-9a-f]{32}$/.test(String(cache.md5)))
		return String(cache.md5);

	hash = syncretro_file_md5(core_path, 0);
	if (!hash)
		return "";

	/* temp + rename, like the ROM cache: a torn write must read as cold rather
	 * than as a wrong hash, which would silently orphan every snapshot. */
	tmp = cache_path + ".tmp";
	f = new File(tmp);
	if (f.open("w")) {
		f.write(JSON.stringify({ path: core_path, size: size, date: date,
		                         md5: hash }));
		f.close();
		file_rename(tmp, cache_path);
	}
	return hash;
}
```

The cache path is `system.data_dir + "syncretro/core." + <id> + ".json"`,
beside the existing `roms.<id>.json`.

- [ ] **Step 4a: Delete stale snapshots during the same pass**

The spec requires that a snapshot matching no current cartridge-and-key is
deleted while the lobby is already listing the directory, so a core upgrade
cleans up after itself rather than accumulating dead blobs forever.

```js
// Delete snapshots that can no longer be restored.
//
// A snapshot is dead when its stem names no cartridge we can see, or when its
// key is not the key that cartridge would restore with now -- which is what
// happens to every snapshot on the console when the core is upgraded. Deleting
// them here costs nothing: the directory has just been read for the picker's
// marks, so this is the same pass.
//
// `current_keys` maps a ROM name to the key8 that ROM would use now.
function syncretro_state_sweep(list, roms, current_keys)
{
	var stems = {};
	var i, stem, n = 0;

	for (i = 0; i < roms.length; i++) {
		stem = String(roms[i].name).replace(/\.[^.]*$/, "");
		stems[stem] = current_keys[roms[i].name] || "";
	}
	for (stem in list) {
		if (stems[stem] !== undefined && stems[stem] === list[stem].key)
			continue;
		if (file_remove(list[stem].path))
			n++;
	}
	return n;
}
```

Add test coverage: a snapshot whose key is stale is deleted; a snapshot whose
stem matches no cartridge is deleted; a live one is kept.

- [ ] **Step 5: Read `[state]` and make the decision in `syncretro_lobby.js`**

Add `state` to the sections `syncretro_lobby_ini()` merges (it currently
merges `console`, `roms`, `lobby`, `text`, `idle`), then:

```js
/* The whole suspend/resume decision, in one place, expressed as the key the
 * door is handed -- or "" for "not permitted", which the caller turns into an
 * absent -state flag.
 *
 * Three things must all hold, and each is owned by whoever knows the answer:
 * the console/romset capability (ours), the sysop's auto_resume switch, and
 * this player's cabinet. The door is told the outcome and never infers it. */
function syncretro_lobby_state_key(rom)
{
	/* The merged ini and the core hash are both resolved once in
	 * syncretro_lobby_init() and kept in module-scope vars beside
	 * syncretro_lobby_con / syncretro_lobby_rules -- declare
	 * `syncretro_lobby_ini_cache` and `syncretro_lobby_core_md5` there and
	 * assign them in init, rather than re-reading per cartridge. */
	var ini = syncretro_lobby_ini_cache;
	var cap, per_rom;

	/* The sysop's switch. */
	if (String(ini.state.auto_resume || "true").toLowerCase() === "false")
		return "";
	/* No snapshots on a shared cabinet, ever: a restore there would roll back
	 * the machine's high-score table to whenever the snapshot was taken. */
	if (syncretro_lobby_con.shared_saves && !syncretro_lobby_private())
		return "";
	/* The capability claim: per-console default, per-romset override. */
	cap = String(ini.console.save_state || "false").toLowerCase() === "true";
	per_rom = syncretro_lobby_games_save_state(rom.name);   /* "", "true", "false" */
	if (per_rom !== "")
		cap = per_rom === "true";
	if (!cap)
		return "";

	return syncretro_state_key(syncretro_lobby_core_md5,
	                           rom.md5,
	                           syncretro_state_opts(ini.options));
}

/* The per-romset override, from games.ini with games.local.ini over the top --
 * the same two-file pair and the same precedence the picker's display titles
 * already use. Returns "" when neither file mentions save_state for this
 * romset, which leaves the [console] default in force. */
function syncretro_lobby_games_save_state(rom_name)
{
	var romset = String(rom_name).replace(/\.[^.]*$/, "").toLowerCase();
	var rows = syncretro_lobby_games;      /* already loaded for display titles */
	var v;

	if (!rows || !rows[romset] || rows[romset].save_state === undefined)
		return "";
	v = String(rows[romset].save_state).toLowerCase();
	return (v === "true" || v === "false") ? v : "";
}
```

`syncretro_lobby_ini()` must also merge an `options` section for
`syncretro_state_opts(ini.options)` to have anything to flatten — it currently
merges `console`, `roms`, `lobby`, `text` and `idle`. Add `options` and
`state` together.

- [ ] **Step 6: Emit `-state` on the command line**

In `syncretro_lobby_play()`, beside the existing `-home`:

```js
	var state_key = syncretro_lobby_state_key(rom);

	cmd = syncretro_lobby_binary
	    + ...
	    + (state_key ? ' -state ' + state_key : '')
	    + ' -home "' + home + '" "'
	    + backslash(syncretro_lobby_rules.dir) + rom.name + '"';
```

`-state <key8>` is 15 characters; `syncretro_lobby_check_cmdlen()` already
guards the 260-byte limit and will report if a long cartridge name plus this
flag crosses it.

- [ ] **Step 7: Run the tests**

```bash
cd /home/rswindell/sbbs && jsexec exec/tests/syncretro_state_test.js \
  && jsexec exec/tests/syncretro_config_test.js
```

Expected: both `ok: 0 failures`.

- [ ] **Step 8: Commit**

---

### Task 4: The cabinet toggle

**Files:**
- Modify: `exec/load/syncretro_lobby.js` (preference, display, key handling)
- Modify: `exec/tests/syncretro_state_test.js` (extend)

**Interfaces:**
- Consumes: `syncretro_lobby_state_key()` from Task 3.
- Produces: `syncretro_lobby_private()` — true when this player has chosen a
  private cabinet on this console.

- [ ] **Step 1: Write the failing test**

```js
// The cabinet preference. Stored via the stock userprops.js, in
// data/user/<####>.ini under [syncretro], NOT inside data/user/<####>/<id>/ --
// that directory IS the private -home handed to the core, and it is what a
// sysop deletes to reclaim space, which must not silently change a preference.
check_str(syncretro_cabinet_key("arcade"), "cabinet.arcade",
          "preference key is per console");
check(syncretro_cabinet_default() === "public",
      "an absent preference means the shared cabinet");
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cd /home/rswindell/sbbs && jsexec exec/tests/syncretro_state_test.js
```

Expected: FAIL — `syncretro_cabinet_key is not defined`.

- [ ] **Step 3: Implement the preference**

```js
load("userprops.js");

/* One key per console, in the stock per-user properties file. userprops.js's
 * filename() is data_dir + "user/%04u.ini" -- a SIBLING of data/user/<####>/,
 * deliberately: the per-console directory under it is the private -home the
 * core is handed, and it is what a sysop deletes to reclaim space from a
 * dormant player. A preference living there would sit among files an emulator
 * writes, and would vanish when their saves were discarded. */
function syncretro_cabinet_key(id)
{
	return "cabinet." + String(id);
}

/* Absent, unreadable, or a guest all mean PUBLIC. That is the safe default: a
 * player whose preference is lost competes on the shared table rather than
 * practising alone while believing their scores count. */
function syncretro_cabinet_default()
{
	return "public";
}

function syncretro_lobby_private()
{
	var v;

	if (!syncretro_lobby_con.shared_saves)
		return true;    /* already a private machine; nothing to choose */
	v = userprops.get("syncretro", syncretro_cabinet_key(syncretro_lobby_con.id),
	                  syncretro_cabinet_default());
	return String(v).toLowerCase() === "private";
}
```

- [ ] **Step 4: Draw the cabinet line and handle its key**

On the picker, only when `syncretro_lobby_con.shared_saves` is true:

```
Cabinet:  [Public]  Private        (P to switch)
          High scores shared with everyone.  No saved games.
```

```
Cabinet:   Public  [Private]       (P to switch)
           Your own machine.  Scores are yours; games resume where you left off.
```

Guests get the line **without** the `(P to switch)` hint and the key does
nothing for them, because `userprops.set()` silently no-ops under `UFLAG_G` —
offering a toggle that cannot persist is worse than not offering it.

Toggling redraws the picker, because the suspended marks change with the
cabinet: a private cabinet's snapshots are invisible from the public one.

- [ ] **Step 5: Run the tests, verify the lobby still parses**

```bash
cd /home/rswindell/sbbs && jsexec exec/tests/syncretro_state_test.js \
  && jsexec xtrn/syncarcade/test_shipped_ini.js
```

- [ ] **Step 6: Commit**

---

### Task 5: Ship the configuration

**Files:**
- Modify: `xtrn/syncivision/syncretro.ini`, `xtrn/syncnes/syncretro.ini`,
  `xtrn/syncarcade/syncretro.ini`
- Modify: `src/doors/syncretro/GAMES_INI.md`

**Interfaces:** consumes the key names read in Tasks 3 and 4.

- [ ] **Step 1: Add `save_state` to each `[console]` section**

`syncivision` and `syncnes` get `true`; `syncarcade` gets `false`. Each with a
comment explaining the claim it makes — for the arcade, that save-state support
in the MAME 2003-Plus lineage is per-driver across some 5000 drivers, that a
partially-supporting driver restores a subtly broken machine which fails as a
wrong-looking game rather than as an error, and that nothing here has been
verified so nothing here claims to be.

- [ ] **Step 2: Add a `[state]` section to all three**

```ini
[state]
; Whether this install offers suspend and resume at all. Nothing to do with
; whether a core CAN save state -- that claim is [console] save_state, and is
; ours to make. This one is yours: turning it off disables the feature
; install-wide without asserting anything about the core.
auto_resume = true
```

- [ ] **Step 3: Document the per-romset override in `GAMES_INI.md`**

A new key beside `boot_frames`, `button.*`, `stick2` and `analog_rest`:
`save_state = true|false`, overriding `[console] save_state` for one romset.
State the rule already recorded in §14: a section in `games.ini` means someone
here has run that cabinet, so a bulk verification tool runs against that file
and its verdicts cover only cabinets that have been played.

- [ ] **Step 4: Verify the shipped files still resolve**

```bash
cd /home/rswindell/sbbs
for p in syncivision syncnes syncarcade; do jsexec xtrn/$p/test_shipped_ini.js; done
```

Expected: three × `ok: 0 failures`.

- [ ] **Step 5: Commit**

---

### Task 6: Documentation and final verification

**Files:**
- Modify: `xtrn/{syncivision,syncnes,syncarcade}/README.md`
- Modify: `src/doors/syncretro/README.md` (the `-state` flag)
- Modify: `src/doors/syncretro/DESIGN.md` (where the state module sits)

- [ ] **Step 1: Document the player-facing behavior in each package README**

For the two cartridge consoles: games resume where you left off, including
after a dropped connection; `Ctrl-R` starts over. For the arcade: the cabinet
choice, what each costs, and that saved games are off until a romset is
verified — with `games.local.ini` named as where a sysop may enable one at
their own risk.

- [ ] **Step 2: Document `-state` in `src/doors/syncretro/README.md`**

Beside `-home`, including that its ABSENCE is what disables snapshots and that
the door never infers permission from `-home`.

- [ ] **Step 3: Full verification sweep**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake --build build-test -j8 && (cd build-test && ctest --output-on-failure)
cd /home/rswindell/sbbs
jsexec exec/tests/syncretro_state_test.js
jsexec exec/tests/syncretro_config_test.js
jsexec xtrn/syncivision/test_shipped_ini.js
jsexec xtrn/syncnes/test_shipped_ini.js
jsexec xtrn/syncarcade/test_shipped_ini.js
```

Expected: every ctest passes; all five JS suites print `0 failures`.

- [ ] **Step 4: Confirm the door forms no opinion from `-home`**

```bash
cd /home/rswindell/sbbs
grep -n "save_dir\|sr_door_home" src/doors/syncretro/syncretro_state.c \
    src/doors/syncretro/main.c | grep -i "state\|snapshot"
```

Every hit must be a *use* of the directory as a destination. Any comparison,
substring test or pattern match against the path is the design error this plan
exists to avoid — report it rather than fixing it silently.

- [ ] **Step 5: Commit**

---

## After the plan

Do **not** push. Do **not** run `./build.sh`. Report what was built, and state
explicitly whether the real-core round trip in Task 2 Step 6 was verified or
could not be run in this environment.

The MAME romset verification tool is **not** part of this plan. The arcade
ships with `save_state` off for every romset; enabling them later is a data
change to `games.ini`, not a code change.
