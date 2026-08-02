# SyncRetro Configuration Consolidation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Collapse a SyncRetro console package's three config conventions into
one — shipped `syncretro.ini`, sysop `syncretro.local.ini`, layered key by key —
and make the shipped file the console's single declaration.

**Architecture:** Both halves of the door (the C binary and the JS lobby) gain a
two-layer ini read that mirrors the one `syncretro_games.c` already uses for
`games.ini` / `games.local.ini`: read the shipped file, read the sysop's over the
top, resolve each key at the same scope with the local file winning. `console.ini`
and `syncretro.example.ini` are absorbed into the shipped `syncretro.ini`, along
with the per-console spec object currently embedded in each package's `lobby.js`,
which collapses to two lines.

**Tech Stack:** C99 (xpdev `ini_file.h`, CMake + ctest), SpiderMonkey 1.8.5
JavaScript (Synchronet `File.iniGetObject`), Synchronet `install-xtrn.js`.

**Spec:** `docs/superpowers/specs/2026-08-02-syncretro-config-consolidation-design.md`

## Global Constraints

- **JS is SpiderMonkey 1.8.5**: no `let`, no `const`, no arrow functions, no
  template literals, no `Array.prototype.includes`, no `Object.assign`.
- **C is portable across GCC, Clang and MSVC**: no reserved identifiers (no
  leading `_` + uppercase, no `__`), no `goto` past an initializer, every
  non-void path returns, no POSIX `realpath()`/`access()` — use xpdev
  `FULLPATH()`, `fexist()`, `mkpath()`.
- **C indentation is TABS**, per `src/doors/syncretro/CLAUDE.md`. Run
  `uncrustify -c src/uncrustify.cfg --no-backup <files>` as the closing step of
  every task that touches C.
- **Resolution rule, everywhere**: the local file wins **at the same scope**. A
  local root key overrides a shipped root key; a local root key does **not**
  reach past a shipped section key. Specificity is decided before locality.
  (Identical to `syncretro_games.c:126-145`.)
- **`[options]` merges by name**: a name present in the local file wins, names
  present only in the shipped file survive. Removing a shipped option is
  deliberately not expressible.
- **The shipped file is tracked; `*.local.ini` is never tracked** — the repo root
  `.gitignore` already ignores `*.local.ini` tree-wide. Do not add per-package
  ignore rules for it.
- **No `docs/v322_new.md` entry.** SyncRetro did not ship in v3.21, so the
  release notes cover its debut, not a change to a scheme no released version
  had.
- **Commit directly to `master`.** Do not create a branch. Do not push — the user
  pushes.
- Every commit message wraps at **78 columns** and ends with the trailer:
  `Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>`
  Verify before committing:
  `awk 'length > 78 {print}' <msgfile>` must print nothing.

## Preserve, do not fix

`syncretro_door.c:292` pins command-line `-option` values during argument
parsing, which runs **before** `sr_config_apply()` (`main.c:418`) reads the ini.
`sr_option_apply()` (`retro_options.c:118-123`) loops every pin with no `break`,
so the **last** matching pin wins — meaning an ini-pinned option currently
overrides a hand-typed `-option`, contrary to what the `[options]` comment
claims. That is pre-existing. This plan appends base pins then local pins,
leaving that relative order untouched. **Do not fix it here**; it is a separate
change.

---

## File Structure

**Created:**
- `src/doors/syncretro/test_config.c` — unit tests for the C two-layer read
- `xtrn/syncivision/syncretro.ini` — shipped, tracked
- `xtrn/syncnes/syncretro.ini` — shipped, tracked
- `xtrn/syncarcade/syncretro.ini` — shipped, tracked

**Modified:**
- `src/doors/syncretro/syncretro_config.c` — one two-layer reader replaces
  `sr_config_read_ini()` + `sr_config_read_console_ini()`
- `src/doors/syncretro/syncretro.h` — declare `sr_config_read()`
- `src/doors/syncretro/CMakeLists.txt` — register `test_config`
- `exec/load/syncretro_lobby.js` — two-layer read; `dir` defaults to
  `js.exec_dir`
- `exec/load/syncretro_lib.js` — `syncretro_console()` / `syncretro_rules()`
  become the compiled floor only
- `xtrn/{syncivision,syncnes,syncarcade}/lobby.js` — collapse to two lines
- `xtrn/{syncivision,syncnes,syncarcade}/install-xtrn.ini` — drop `[copy:]`
- `xtrn/{syncivision,syncnes,syncarcade}/.gitignore` — drop `/syncretro.ini`
- `xtrn/{syncivision,syncnes,syncarcade}/README.md` — the one rule + migration
- `src/doors/syncretro/GAMES_INI.md` — §14 restated

**Deleted:**
- `xtrn/{syncivision,syncnes,syncarcade}/console.ini`
- `xtrn/{syncivision,syncnes,syncarcade}/syncretro.example.ini`

---

### Task 1: Two-layer ini read in the door (C)

**Files:**
- Modify: `src/doors/syncretro/syncretro_config.c:194-300` (replaces
  `sr_config_read_console_ini()` and `sr_config_read_ini()`)
- Modify: `src/doors/syncretro/syncretro.h` (declare `sr_config_read()`)
- Modify: `src/doors/syncretro/CMakeLists.txt:239-246` (register `test_config`)
- Test: `src/doors/syncretro/test_config.c`

**Interfaces:**
- Produces: `void sr_config_read(const char *dir);` — reads `<dir>/syncretro.ini`
  then `<dir>/syncretro.local.ini` and populates every existing
  `sr_config_*()` getter. Replaces the two static readers. Called by
  `sr_config_apply()` with the launch directory.
- Consumes: nothing from other tasks.
- Unchanged getters that later tasks and existing callers rely on:
  `sr_config_console_name()`, `sr_config_profile()`, `sr_config_audio_enabled()`,
  `sr_config_audio_quality()`, `sr_config_audio_volume()`,
  `sr_config_audio_chunk_ms()`, `sr_config_audio_prebuffer()`,
  `sr_config_dirty_rect()`, `sr_config_pace_depth()`,
  `sr_config_palette_subset()`, `sr_config_aspect_mode()`,
  `sr_config_idle_timeout()`, `sr_config_idle_warn()`, `sr_config_dirty_log()`,
  `sr_config_input_device()`, `sr_config_disc_rotate()`.
- **Not** produced here: a `shared_saves` getter. The door does not consume that
  key in this change; only the lobby does. It is added by the save/restore spec.

- [ ] **Step 1: Write the failing test**

Create `src/doors/syncretro/test_config.c`:

```c
/* test_config.c -- syncretro.ini is shipped and syncretro.local.ini is the
 * sysop's overlay, read over the top of it. Every key the door has must resolve
 * through both, at the same scope, with the local file winning. See
 * docs/superpowers/specs/2026-08-02-syncretro-config-consolidation-design.md.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro.h"
#include "retro_core.h"   /* rc_core_t, for the rc_core_load_game() stub */
#include "dirwrap.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) \
		do { \
			if (!(cond)) { \
				printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
				failures++; \
			} \
		} while (0)

#define CHECK_STR(got, want) \
		do { \
			const char *g_ = (got); \
			if (g_ == NULL || strcmp(g_, (want)) != 0) { \
				printf("FAIL %s:%d: got \"%s\", want \"%s\"\n", \
					   __FILE__, __LINE__, g_ ? g_ : "(null)", (want)); \
				failures++; \
			} \
		} while (0)

/* Every door module syncretro_config.c calls into, stubbed so this test needs
 * neither a libretro core nor termgfx: the claim under test is the ini
 * resolution, not what the pinned options later do. sr_config_apply() is never
 * called here, but the linker still has to resolve what it references. */
#define STUB_PINS 32
static char stub_pin[STUB_PINS][160];
static int  stub_npin;

int sr_option_pin(const char *kv)
{
	if (stub_npin >= STUB_PINS)
		return -1;
	snprintf(stub_pin[stub_npin], sizeof stub_pin[0], "%s", kv);
	stub_npin++;
	return 0;
}

const char *rc_core_ext(void)
{
	return ".so";
}

int rc_core_load_game(rc_core_t *c, const char *rom_path)
{
	(void)c;
	(void)rom_path;
	return 0;
}

const char *sr_door_home(void)
{
	return NULL;
}

const char *sr_door_core_path(void)
{
	return NULL;
}

const char *sr_door_rom_path(void)
{
	return NULL;
}

void sr_io_set_aspect(double aspect)
{
	(void)aspect;
}

/* The LAST pin recorded for `key`, or NULL. Mirrors sr_option_apply()'s
 * last-wins loop (retro_options.c:118-123), which is what makes appending the
 * local file's options after the shipped file's the correct merge. */
static const char *last_pin(const char *key)
{
	size_t      n = strlen(key);
	const char *hit = NULL;
	int         i;

	for (i = 0; i < stub_npin; i++) {
		if (strncmp(stub_pin[i], key, n) == 0 && stub_pin[i][n] == '=')
			hit = stub_pin[i] + n + 1;
	}
	return hit;
}

#define BASE_DIR  "cfgfx_base"    /* shipped file only */
#define BOTH_DIR  "cfgfx_both"    /* shipped + sysop overlay */
#define EMPTY_DIR "cfgfx_empty"   /* neither file */

static void write_fixtures(void)
{
	FILE *f;

	mkpath(BASE_DIR);
	f = fopen(BASE_DIR "/syncretro.ini", "w");
	fputs("[console]\n"
	      "name    = Intellivision\n"
	      "short   = Intv\n"
	      "core    = freeintv_libretro\n"
	      "profile = intv\n"
	      "\n"
	      "[video]\n"
	      "aspect     = 4:3\n"
	      "dirty_rect = true\n"
	      "pace_depth = 0\n"
	      "\n"
	      "[audio]\n"
	      "enabled = true\n"
	      "volume  = 80\n"
	      "\n"
	      "[idle]\n"
	      "timeout = 15m\n"
	      "warn    = 60\n"
	      "\n"
	      "[disc]\n"
	      "rotate = brickout\n"
	      "\n"
	      "[options]\n"
	      "freeintv_pixel_perfect = enabled\n"
	      "freeintv_control       = keypad\n", f);
	fclose(f);

	mkpath(BOTH_DIR);
	f = fopen(BOTH_DIR "/syncretro.ini", "w");
	fputs("[console]\n"
	      "name    = Intellivision\n"
	      "short   = Intv\n"
	      "profile = intv\n"
	      "\n"
	      "[video]\n"
	      "aspect     = 4:3\n"
	      "dirty_rect = true\n"
	      "\n"
	      "[audio]\n"
	      "volume = 80\n"
	      "\n"
	      "[idle]\n"
	      "timeout = 15m\n"
	      "\n"
	      "[options]\n"
	      "freeintv_pixel_perfect = enabled\n"
	      "freeintv_control       = keypad\n", f);
	fclose(f);

	/* The sysop's overlay: only what differs. */
	f = fopen(BOTH_DIR "/syncretro.local.ini", "w");
	fputs("[video]\n"
	      "aspect = square\n"
	      "\n"
	      "[audio]\n"
	      "volume = 40\n"
	      "\n"
	      "[idle]\n"
	      "timeout = 0\n"
	      "\n"
	      "[debug]\n"
	      "dirty_log = true\n"
	      "\n"
	      "[options]\n"
	      "freeintv_control = disc\n", f);
	fclose(f);

	mkpath(EMPTY_DIR);
}

/* The shipped file alone: every key resolves from it. */
static void test_base_only(void)
{
	stub_npin = 0;
	sr_config_read(BASE_DIR);

	CHECK_STR(sr_config_console_name(), "Intellivision");
	CHECK_STR(sr_config_profile(), "intv");
	CHECK_STR(sr_config_aspect_mode(), "4:3");
	CHECK(sr_config_audio_volume() == 80);
	CHECK(sr_config_idle_timeout() == 900);
	CHECK(sr_config_idle_warn() == 60);
	CHECK(sr_config_dirty_log() == 0);
	CHECK_STR(last_pin("freeintv_control"), "keypad");
}

/* The overlay: a local key wins, a shipped-only key survives, a local-only
 * section arrives, and [options] merges by name rather than replacing. */
static void test_local_overlay(void)
{
	stub_npin = 0;
	sr_config_read(BOTH_DIR);

	CHECK_STR(sr_config_aspect_mode(), "square");          /* local wins */
	CHECK(sr_config_audio_volume() == 40);                 /* local wins */
	CHECK(sr_config_idle_timeout() == 0);                  /* local wins, 0 is a value */
	CHECK(sr_config_dirty_rect() == 1);                    /* shipped survives */
	CHECK_STR(sr_config_console_name(), "Intellivision");  /* shipped survives */
	CHECK(sr_config_dirty_log() == 1);                     /* local-only section */
	CHECK_STR(last_pin("freeintv_control"), "disc");       /* local wins */
	CHECK_STR(last_pin("freeintv_pixel_perfect"), "enabled");  /* shipped survives */
}

/* Neither file: the compiled floor, and the door still starts. */
static void test_no_files(void)
{
	stub_npin = 0;
	sr_config_read(EMPTY_DIR);

	CHECK(sr_config_audio_volume() == 100);
	CHECK(sr_config_dirty_rect() == 1);
	CHECK(sr_config_idle_timeout() == 600);
	CHECK(sr_config_idle_warn() == 60);
	CHECK(sr_config_console_name() == NULL);
	CHECK(stub_npin == 0);
}

/* An overlay that is a byte-for-byte copy of the shipped file resolves
 * identically to no overlay at all. This is the property that makes the
 * migration in the spec safe: a sysop renames their whole config to
 * syncretro.local.ini and nothing changes. */
static void test_full_copy_equivalence(void)
{
	char aspect[64];
	int  volume;

	stub_npin = 0;
	sr_config_read(BASE_DIR);
	snprintf(aspect, sizeof aspect, "%s", sr_config_aspect_mode());
	volume = sr_config_audio_volume();

	mkpath("cfgfx_copy");
	{
		FILE *src = fopen(BASE_DIR "/syncretro.ini", "r");
		FILE *dst = fopen("cfgfx_copy/syncretro.local.ini", "w");
		int   c;

		CHECK(src != NULL && dst != NULL);
		if (src != NULL && dst != NULL) {
			while ((c = fgetc(src)) != EOF)
				fputc(c, dst);
		}
		if (src != NULL)
			fclose(src);
		if (dst != NULL)
			fclose(dst);
	}

	stub_npin = 0;
	sr_config_read("cfgfx_copy");
	CHECK_STR(sr_config_aspect_mode(), aspect);
	CHECK(sr_config_audio_volume() == volume);
}

int main(void)
{
	write_fixtures();
	test_base_only();
	test_local_overlay();
	test_no_files();
	test_full_copy_equivalence();

	printf("%s: %d failure(s)\n", failures ? "FAILED" : "ok", failures);
	return failures != 0;
}
```

- [ ] **Step 2: Register the test in CMake**

Add to `src/doors/syncretro/CMakeLists.txt` immediately after the `test_games`
block (line 246):

```cmake
    # test_config -- the shipped ini + the sysop's overlay. Links xpdev for the
    # ini reader and stubs the option/core seams, so it needs neither a core nor
    # termgfx. termgfx's include dir only: audio_mgr.h supplies the quality
    # default the reader falls back to.
    add_executable(test_config test_config.c syncretro_config.c)
    target_include_directories(test_config PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                               ${CMAKE_CURRENT_SOURCE_DIR}/../termgfx
                               ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
    target_link_libraries(test_config xpdev)
    add_test(NAME config COMMAND test_config)
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake -S . -B build-test -DSYNCRETRO_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-test --target test_config -j4
```

Expected: **compile/link failure** — `sr_config_read` is not declared or
defined.

- [ ] **Step 4: Declare the reader in `syncretro.h`**

Replace the `console.ini` comment block above `sr_config_console_name()`
(`syncretro.h:82-88`) with:

```c
/* Read <dir>/syncretro.ini, then <dir>/syncretro.local.ini over the top of it,
 * and populate every sr_config_*() getter below. Both files are optional and
 * every key has a compiled default, so a door directory with neither behaves
 * exactly as one with both at their defaults.
 *
 * syncretro.ini is SHIPPED and syncretro.local.ini is the SYSOP'S: a pull
 * overwrites the first and never the second, so the second is the one to edit.
 * A key present in the local file wins at the SAME SCOPE -- the same rule
 * games.ini/games.local.ini already uses (syncretro_games.c). */
void        sr_config_read(const char *dir);

/* [console] -- what console this install IS, or NULL when the key is absent.
 * Read by the lobby from the same file, which is what keeps these off a command
 * line Windows truncates at 260 bytes. -console / -profile still override, for
 * a door run by hand. */
const char *sr_config_console_name(void);
const char *sr_config_profile(void);
const char *sr_config_rom_path(void);
```

- [ ] **Step 5: Replace both readers in `syncretro_config.c`**

Delete `sr_config_read_console_ini()` (`syncretro_config.c:194-216`) and
`sr_config_read_ini()` (`:217-300`) entirely, and put this in their place:

```c
/* The shipped file and the sysop's overlay resolve as if the local file's lines
 * were APPENDED to the shipped one: a key present in the local file wins at the
 * SAME SCOPE. So a local [video] key overrides the shipped [video] key -- but a
 * local root key does not reach past a shipped section key, because specificity
 * is decided before locality. Same rule, and the same reason, as
 * games.ini/games.local.ini; see sr_games_str() in syncretro_games.c. */
static str_list_t sr_config_read_one(const char *dir, const char *name)
{
	char       path[PATH_MAX];
	str_list_t ini;
	FILE *     f;

	snprintf(path, sizeof path, "%s/%s", dir, name);
	f = fopen(path, "r");
	if (f == NULL)
		return NULL;
	ini = iniReadFile(f);
	fclose(f);
	return ini;
}

static const char *sr_cfg_str(str_list_t base, str_list_t local,
                              const char *section, const char *key,
                              const char *deflt, char *val)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetString(local, section, key, deflt, val);
	if (base != NULL)
		return iniGetString(base, section, key, deflt, val);
	snprintf(val, INI_MAX_VALUE_LEN, "%s", deflt);
	return val;
}

static int sr_cfg_bool(str_list_t base, str_list_t local, const char *section,
                       const char *key, int deflt)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetBool(local, section, key, deflt);
	if (base != NULL)
		return iniGetBool(base, section, key, deflt);
	return deflt;
}

static long sr_cfg_int(str_list_t base, str_list_t local, const char *section,
                       const char *key, long deflt)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetInteger(local, section, key, deflt);
	if (base != NULL)
		return iniGetInteger(base, section, key, deflt);
	return deflt;
}

static double sr_cfg_float(str_list_t base, str_list_t local,
                           const char *section, const char *key, double deflt)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetFloat(local, section, key, deflt);
	if (base != NULL)
		return iniGetFloat(base, section, key, deflt);
	return deflt;
}

static double sr_cfg_duration(str_list_t base, str_list_t local,
                              const char *section, const char *key,
                              double deflt)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetDuration(local, section, key, deflt);
	if (base != NULL)
		return iniGetDuration(base, section, key, deflt);
	return deflt;
}

/* [options] merges BY NAME: a name in the local file wins, names only in the
 * shipped file survive. sr_option_apply() keeps the LAST matching pin
 * (retro_options.c), so appending the local file's options after the shipped
 * file's is what makes the local one win -- the order here is load-bearing. */
static void sr_config_pin_options(str_list_t ini)
{
	named_string_t **opts;
	char             kv[INI_MAX_VALUE_LEN * 2];
	size_t           i;

	if (ini == NULL)
		return;
	opts = iniGetNamedStringList(ini, "options");
	if (opts == NULL)
		return;
	for (i = 0; opts[i] != NULL; i++) {
		snprintf(kv, sizeof kv, "%s=%s", opts[i]->name,
		         opts[i]->value != NULL ? opts[i]->value : "");
		sr_option_pin(kv);
	}
	iniFreeNamedStringList(opts);
}

void sr_config_read(const char *dir)
{
	str_list_t base;
	str_list_t local;
	char       val[INI_MAX_VALUE_LEN];

	if (dir == NULL)
		dir = ".";
	base  = sr_config_read_one(dir, "syncretro.ini");
	local = sr_config_read_one(dir, "syncretro.local.ini");

	sr_cfg_str(base, local, "console", "name", "", val);
	snprintf(g_con_name, sizeof g_con_name, "%s", val);
	sr_cfg_str(base, local, "console", "short", "", val);
	snprintf(g_con_short, sizeof g_con_short, "%s", val);
	sr_cfg_str(base, local, "console", "core", "", val);
	snprintf(g_con_core, sizeof g_con_core, "%s", val);
	sr_cfg_str(base, local, "console", "profile", "", val);
	snprintf(g_con_profile, sizeof g_con_profile, "%s", val);

	sr_cfg_str(base, local, "disc", "rotate", "", val);
	snprintf(g_disc_rotate, sizeof g_disc_rotate, "%s", val);
	sr_cfg_str(base, local, "video", "aspect", "core", val);
	snprintf(g_aspect, sizeof g_aspect, "%s", val);

	g_audio_enabled   = sr_cfg_bool(base, local, "audio", "enabled", TRUE);
	g_audio_quality   = sr_cfg_float(base, local, "audio", "quality",
	                                 TERMGFX_MUSIC_QUALITY_DEFAULT);
	g_audio_volume    = (int)sr_cfg_int(base, local, "audio", "volume", 100);
	g_audio_chunk_ms  = (int)sr_cfg_int(base, local, "audio", "chunk_ms", 100);
	g_audio_prebuffer = (int)sr_cfg_int(base, local, "audio", "prebuffer", 3);

	g_dirty_rect     = sr_cfg_bool(base, local, "video", "dirty_rect", TRUE);
	/* pace_depth -- how many frames may be in flight at once. 0 (the default)
	 * lets the AIMD pacer choose from the measured round-trip. Pinning it to 1
	 * is a diagnostic: on the sixel tier a terminal draws a frame
	 * progressively, so a second frame arriving mid-draw composites two frames
	 * on screen -- which reads as tearing on scrolling content. */
	g_pace_depth     = (int)sr_cfg_int(base, local, "video", "pace_depth", 0);
	g_palette_subset = sr_cfg_bool(base, local, "video", "palette_subset", TRUE);

	/* iniGetDuration() so "15m"/"900"/"1h" all work, and so a bare number means
	 * SECONDS here exactly as it does in the lobby. */
	g_idle_timeout = (unsigned)sr_cfg_duration(base, local, "idle", "timeout",
	                                           SR_IDLE_DEFAULT);
	g_idle_warn    = (unsigned)sr_cfg_duration(base, local, "idle", "warn", 60);

	g_dirty_log    = sr_cfg_bool(base, local, "debug", "dirty_log", FALSE);
	/* [input] device -- a RETRO_DEVICE id to hand the core for both ports. 0
	 * (the default) means say NOTHING, leaving whatever the core chose: a wrong
	 * device id silently rewires every button. */
	g_input_device = (int)sr_cfg_int(base, local, "input", "device", 0);

	sr_config_pin_options(base);
	sr_config_pin_options(local);

	strListFree(&base);
	strListFree(&local);
}
```

Then, in `sr_config_apply()`, replace the two calls

```c
	sr_config_read_ini();
	sr_config_read_console_ini();
```

with

```c
	sr_config_read(cwd);
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake --build build-test --target test_config -j4 && (cd build-test && ctest -R config --output-on-failure)
```

Expected: `ok: 0 failure(s)` and `100% tests passed`.

- [ ] **Step 7: Run the whole suite and build the door**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake --build build-test -j4 && (cd build-test && ctest --output-on-failure)
./build.sh
```

Expected: every test passes; `[build] Built: .../build/syncretro`.

- [ ] **Step 8: Format and commit**

```bash
cd /home/rswindell/sbbs
uncrustify -c src/uncrustify.cfg --no-backup \
    src/doors/syncretro/syncretro_config.c \
    src/doors/syncretro/test_config.c
git add src/doors/syncretro/syncretro_config.c src/doors/syncretro/syncretro.h \
        src/doors/syncretro/test_config.c src/doors/syncretro/CMakeLists.txt
git commit -F <(printf '%s\n' \
'syncretro: read syncretro.ini with syncretro.local.ini over the top' '' \
'The door read a shipped console.ini and a sysop-copied syncretro.ini' \
'through two separate readers, so a key added upstream never reached an' \
'existing install. One reader now layers a shipped file and the sysop'"'"'s' \
'overlay, resolving each key at the same scope with the local file' \
'winning -- the rule games.ini/games.local.ini already uses.' '' \
'[options] merges by name, relying on sr_option_apply() keeping the last' \
'matching pin, so the shipped options are pinned before the local ones.' '' \
'Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>')
```

---

### Task 2: Two-layer ini read in the lobby (JS)

**Files:**
- Modify: `exec/load/syncretro_lobby.js:145-235` (replaces
  `syncretro_lobby_console_ini()` and the `syncretro.ini` read inside
  `syncretro_lobby_init()`)
- Test: `exec/tests/syncretro_config_test.js`

**Interfaces:**
- Consumes: nothing from Task 1 (the two halves read the same files
  independently; there is no shared code between C and JS).
- Produces: `syncretro_lobby_ini(dir)` — returns a plain object
  `{console:{}, roms:{}, lobby:{}, text:{}, idle:{}}` where each property is the
  shipped section overlaid with the sysop's. Task 3 consumes it.

- [ ] **Step 1: Write the failing test**

Create `exec/tests/syncretro_config_test.js`:

```js
// syncretro_config_test.js -- the lobby's half of the shipped-ini + sysop-overlay
// read. Run: jsexec exec/tests/syncretro_config_test.js
//
// SpiderMonkey 1.8.5: no let/const, no arrow functions, no template literals.

load("syncretro_lobby.js");

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

var dir = system.temp_dir + "syncretro_cfgtest/";
var f;

mkpath(dir);

f = new File(dir + "syncretro.ini");
f.open("w");
f.write("[console]\n"
    + "name = Intellivision\n"
    + "short = Intv\n"
    + "core = freeintv_libretro\n"
    + "profile = intv\n"
    + "shared_saves = false\n"
    + "\n"
    + "[roms]\n"
    + "dir = roms\n"
    + "ext = int, bin, rom\n"
    + "min_size = 2048\n"
    + "max_size = 65536\n"
    + "\n"
    + "[lobby]\n"
    + "enter_sound = ding.mid\n"
    + "\n"
    + "[idle]\n"
    + "timeout = 15m\n");
f.close();

f = new File(dir + "syncretro.local.ini");
f.open("w");
f.write("[console]\n"
    + "short = Intellivision\n"
    + "\n"
    + "[roms]\n"
    + "exclude = BIOS\n"
    + "\n"
    + "[lobby]\n"
    + "enter_sound = \n"
    + "\n"
    + "[text]\n"
    + "header = mine.asc\n");
f.close();

var ini = syncretro_lobby_ini(dir);

// A shipped key with no local twin survives.
check_str(ini.console.name, "Intellivision", "console.name survives");
check_str(ini.console.core, "freeintv_libretro", "console.core survives");
check_str(ini.roms.ext, "int, bin, rom", "roms.ext survives");
check_str(ini.idle.timeout, "15m", "idle.timeout survives");

// A local key wins at the same scope.
check_str(ini.console.short, "Intellivision", "console.short overridden");

// A local-only key arrives.
check_str(ini.roms.exclude, "BIOS", "roms.exclude arrives");

// A local-only SECTION arrives.
check_str(ini.text.header, "mine.asc", "text.header arrives");

// A key present but EMPTY in the local file is a real override in [lobby] and
// [text] ("draw nothing"), not an absent key.
check(ini.lobby.enter_sound === "", "empty enter_sound overrides");

// Neither file: every section is an object, never null.
var empty = system.temp_dir + "syncretro_cfgtest_empty/";
mkpath(empty);
var none = syncretro_lobby_ini(empty);
check(none.console && typeof none.console === "object", "console object when no files");
check(none.roms && typeof none.roms === "object", "roms object when no files");
check(none.lobby && typeof none.lobby === "object", "lobby object when no files");
check(none.text && typeof none.text === "object", "text object when no files");
check(none.idle && typeof none.idle === "object", "idle object when no files");

// A full copy as the overlay resolves identically to no overlay at all -- the
// property that makes the spec's migration safe.
var copydir = system.temp_dir + "syncretro_cfgtest_copy/";
mkpath(copydir);
file_copy(dir + "syncretro.ini", copydir + "syncretro.ini");
file_copy(dir + "syncretro.ini", copydir + "syncretro.local.ini");
var copied = syncretro_lobby_ini(copydir);
check_str(copied.console.short, "Intv", "full-copy overlay: console.short");
check_str(copied.roms.ext, "int, bin, rom", "full-copy overlay: roms.ext");

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /home/rswindell/sbbs && jsexec exec/tests/syncretro_config_test.js
```

Expected: FAIL — `syncretro_lobby_ini is not defined`.

- [ ] **Step 3: Write the reader**

In `exec/load/syncretro_lobby.js`, replace `syncretro_lobby_console_ini()`
(lines 145-188) with:

```js
/* One section, shipped then overlaid. `blanks` keeps a key that is present but
 * EMPTY -- a real override in [lobby] and [text] ("draw nothing", "no display
 * file"), where iniGetObject would otherwise drop it and make it
 * indistinguishable from an absent key. */
function syncretro_lobby_ini_section(base, local, section, blanks)
{
	var out = {};
	var b, l, k;

	b = base ? base.iniGetObject(section, false, blanks) : null;
	l = local ? local.iniGetObject(section, false, blanks) : null;
	if (b) {
		for (k in b)
			out[k] = b[k];
	}
	if (l) {
		for (k in l)
			out[k] = l[k];
	}
	return out;
}

/* syncretro.ini is SHIPPED and read first; syncretro.local.ini is the SYSOP'S
 * and read over the top, winning key by key. The shipped file is also read by
 * the DOOR, which is the whole point of it being a file: these are the facts
 * both halves need, and they used to travel from here to the door on the
 * command line. Synchronet assembles that line into a 260-byte buffer on
 * Windows and truncates it there in silence, so a long cartridge name pushed
 * the ROM argument -- the last thing on the line -- off the end, and the door
 * reported "(no ROM)" for content the BBS had just logged the full path of.
 *
 * Every section is an object, never null: a caller may index it without
 * guarding. Both files are optional. */
function syncretro_lobby_ini(dir)
{
	var base  = new File(backslash(dir) + "syncretro.ini");
	var local = new File(backslash(dir) + "syncretro.local.ini");
	var out;

	if (!base.open("r"))
		base = null;
	if (!local.open("r"))
		local = null;

	out = {
		console: syncretro_lobby_ini_section(base, local, "console", false),
		roms:    syncretro_lobby_ini_section(base, local, "roms", false),
		lobby:   syncretro_lobby_ini_section(base, local, "lobby", true),
		text:    syncretro_lobby_ini_section(base, local, "text", true),
		idle:    syncretro_lobby_ini_section(base, local, "idle", false)
	};

	if (base)
		base.close();
	if (local)
		local.close();
	return out;
}
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd /home/rswindell/sbbs && jsexec exec/tests/syncretro_config_test.js
```

Expected: `ok: 0 failures`, exit 0.

- [ ] **Step 5: Commit**

```bash
cd /home/rswindell/sbbs
git add exec/load/syncretro_lobby.js exec/tests/syncretro_config_test.js
git commit -F <(printf '%s\n' \
'syncretro: lobby reads syncretro.ini with syncretro.local.ini over it' '' \
'The lobby'"'"'s half of the same two-layer read the door now does: the' \
'shipped file first, the sysop'"'"'s overlay over the top, winning key by' \
'key. [lobby] and [text] keep the blanks-preserving read, where a key' \
'present but empty is a real override rather than an absent key.' '' \
'Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>')
```

---

### Task 3: The console and roms spec move into the ini contract

**Files:**
- Modify: `exec/load/syncretro_lobby.js` (`syncretro_lobby_init()`, and the
  `syncretro_lobby()` entry point's `dir` default)
- Modify: `exec/load/syncretro_lib.js:271-299` (`syncretro_rules()`),
  `:343-369` (`syncretro_console()`)
- Test: `exec/tests/syncretro_config_test.js` (extend)

**Interfaces:**
- Consumes: `syncretro_lobby_ini(dir)` from Task 2, returning
  `{console, roms, lobby, text, idle}`.
- Produces: `syncretro_console(spec)` and `syncretro_rules(spec)` keep their
  existing signatures and return shapes — `{name, short, profile, core, id,
  shared_saves}` and `{dir, ext, exclude, min_size, max_size, bios_md5,
  bios_names, bios_words}` — but are now fed the merged ini object rather than a
  per-package literal. Tasks 4-6 rely on `syncretro_lobby()` working with **no
  argument at all**.

- [ ] **Step 1: Write the failing test**

Append to `exec/tests/syncretro_config_test.js`, before the final `print`:

```js
// The console and rules objects now come from the ini, not a JS literal.
var con = syncretro_console(ini.console);
check_str(con.name, "Intellivision", "console name from ini");
check_str(con.short, "Intellivision", "console short from ini (local wins)");
check_str(con.profile, "intv", "console profile from ini");
check_str(con.core, "freeintv_libretro", "console core from ini");
check_str(con.id, "intellivision", "console id derived from short");
check(con.shared_saves === false, "shared_saves false from ini");

var rules = syncretro_rules(ini.roms);
check_str(rules.dir, "roms", "rules dir from ini");
check_str(rules.ext.join(","), "int,bin,rom", "rules ext parsed from ini");
check_str(rules.exclude.join(","), "BIOS", "rules exclude from ini");
check(rules.min_size === 2048, "rules min_size from ini");
check(rules.max_size === 65536, "rules max_size from ini");

// shared_saves is a string in an ini, not a boolean. "true" must mean true and
// "false" must mean false -- the naive truthiness test makes both true.
check(syncretro_console({ shared_saves: "true" }).shared_saves === true,
      "shared_saves \"true\" is true");
check(syncretro_console({ shared_saves: "false" }).shared_saves === false,
      "shared_saves \"false\" is false");

// dir_name was the spec's key for the roms sub-directory; the ini spells it
// `dir`, which is what [roms] already used. Both must reach rules.dir.
check_str(syncretro_rules({ dir: "carts" }).dir, "carts", "roms dir key");
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /home/rswindell/sbbs && jsexec exec/tests/syncretro_config_test.js
```

Expected: FAIL on `shared_saves "false" is false` (an ini value is the string
`"false"`, which is truthy) and on `roms dir key` (`syncretro_rules()` reads
`spec.dir_name`, not `spec.dir`).

- [ ] **Step 3: Teach the two constructors the ini's spelling**

In `exec/load/syncretro_lib.js`, replace the `shared_saves` assignment in
`syncretro_console()`:

```js
		// Where the core's saves go. Default: PER-USER, which is right for a
		// cartridge console -- one player's battery save is not another's.
		// `shared_saves = true` points every player at one directory instead,
		// which is right for an ARCADE cabinet: the high-score table is the
		// whole point of the machine, and there is no per-player save to keep
		// apart. See the home path in syncretro_lobby.js.
		//
		// An ini value is a STRING, so the naive truthiness test makes "false"
		// true. Compare the text.
		if (spec.shared_saves !== undefined && spec.shared_saves !== null) {
			c.shared_saves = String(spec.shared_saves).toLowerCase() === "true"
			    || String(spec.shared_saves) === "1";
		}
```

And in `syncretro_rules()`, accept the ini's key name:

```js
	if (spec.dir)
		r.dir = String(spec.dir);
	else if (spec.dir_name)
		r.dir = String(spec.dir_name);
```

- [ ] **Step 4: Feed the merged ini into `syncretro_lobby_init()`**

In `exec/load/syncretro_lobby.js`, replace the body of `syncretro_lobby_init()`
down to the end of the old `syncretro.ini` read (the block ending with the
`if (ini) { ... }` at line 235) with:

```js
function syncretro_lobby_init(spec)
{
	var ini;
	var target, sep, sub, exe, cname, bpfx, cpfx;

	if (!spec)
		spec = {};
	syncretro_lobby_dir = backslash(spec.dir || js.exec_dir);
	ini = syncretro_lobby_ini(syncretro_lobby_dir);

	syncretro_lobby_con   = syncretro_console(ini.console);
	syncretro_lobby_rules = syncretro_rules(ini.roms);
	syncretro_lobby_bios  = syncretro_list(ini.console.bios);
	/* How the door gets the player's connection. Default: a SOCKET (Synchronet
	 * hands the door one end of a loopback socketpair and pumps it). `stdio`
	 * instead has Synchronet fork the door on a raw pty (EX_STDIO|EX_BIN) and
	 * relay it -- the same shape Mystic uses on *nix, and so the way to
	 * exercise the door's -stdio path against a real session. */
	syncretro_lobby_stdio = String(ini.console.stdio).toLowerCase() === "true";

	syncretro_lobby_cfg = { lobby: ini.lobby, text: ini.text, idle: ini.idle };
```

Leave everything after that point in the function unchanged.

- [ ] **Step 5: Default `dir` in the entry point**

Find the exported `syncretro_lobby(spec)` function and make the argument
optional, so a package's `lobby.js` needs none:

```js
/* The whole of a console package's lobby.js is `syncretro_lobby()`: what the
 * console IS now lives in the shipped syncretro.ini beside it, read by this
 * function and by the door. `spec` remains only for a door run from an
 * unusual directory (`{dir: "/some/where/"}`). */
function syncretro_lobby(spec)
{
	syncretro_lobby_init(spec || {});
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cd /home/rswindell/sbbs && jsexec exec/tests/syncretro_config_test.js
```

Expected: `ok: 0 failures`, exit 0.

- [ ] **Step 7: Commit**

```bash
cd /home/rswindell/sbbs
git add exec/load/syncretro_lobby.js exec/load/syncretro_lib.js \
        exec/tests/syncretro_config_test.js
git commit -F <(printf '%s\n' \
'syncretro: build the console and rom rules from the ini, not a literal' '' \
'What a console is -- its name, core, profile, cartridge extensions and' \
'size band -- was declared in each package'"'"'s lobby.js and, for four of' \
'those keys, declared a second time in console.ini. It now comes from the' \
'shipped syncretro.ini alone, which both halves of the door read.' '' \
'shared_saves is compared as text: an ini value is a string, and the' \
'truthiness test made "false" true.' '' \
'Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>')
```

---

### Task 4: Migrate the syncivision package

**Files:**
- Create: `xtrn/syncivision/syncretro.ini`
- Delete: `xtrn/syncivision/console.ini`,
  `xtrn/syncivision/syncretro.example.ini`
- Modify: `xtrn/syncivision/lobby.js`,
  `xtrn/syncivision/install-xtrn.ini:56`, `xtrn/syncivision/.gitignore`
- Test: `xtrn/syncivision/test_lobby_headless.js` (existing)

**Interfaces:**
- Consumes: `syncretro_lobby()` with no argument (Task 3);
  `syncretro_lobby_ini()` section names `console`, `roms`, `lobby`, `text`,
  `idle` (Task 2).
- Produces: the shipped-ini layout every later package task copies.

**The `[console]` section for this package** — every value taken from the files
being deleted, unchanged:

| key | value | from |
|---|---|---|
| `name` | `Intellivision` | `console.ini`, `lobby.js:20` |
| `short` | `Intv` | `console.ini`, `lobby.js:24` |
| `core` | `freeintv_libretro` | `console.ini`, `lobby.js:25` |
| `profile` | `intv` | `console.ini`, `lobby.js:26` |
| `shared_saves` | `false` | `syncretro_lib.js:346` default |
| `bios` | `exec.bin, grom.bin` | `lobby.js:37` |

**The `[roms]` section:**

| key | value | from |
|---|---|---|
| `dir` | `roms` | `syncretro_rules()` default |
| `ext` | `int, bin, rom` | `lobby.js:30` |
| `min_size` | `2048` | `lobby.js:31` (`2 * 1024`) |
| `max_size` | `65536` | `lobby.js:32` (`64 * 1024`) |
| `bios_names` | `exec.bin, grom.bin` | `lobby.js:38` |
| `bios_words` | `bios` | `lobby.js:50` |

- [ ] **Step 1: Write the failing test**

Add to `xtrn/syncivision/test_lobby_headless.js`, after its existing stubs and
before its final result print:

```js
// The shipped syncretro.ini must reproduce exactly what the deleted lobby.js
// spec and console.ini declared. These are the values a player's save
// directory, cartridge picker and key bindings all derive from, so a typo here
// is silent: the door still runs, against the wrong console.
var shipped = syncretro_lobby_ini(js.exec_dir);
var con = syncretro_console(shipped.console);
var rules = syncretro_rules(shipped.roms);

check_str(con.name, "Intellivision", "shipped ini: name");
check_str(con.short, "Intv", "shipped ini: short");
check_str(con.id, "intv", "shipped ini: id (names the save dir)");
check_str(con.core, "freeintv_libretro", "shipped ini: core");
check_str(con.profile, "intv", "shipped ini: profile");
check(con.shared_saves === false, "shipped ini: shared_saves false");
check_str(rules.ext.join(","), "int,bin,rom", "shipped ini: ext");
check(rules.min_size === 2048, "shipped ini: min_size");
check(rules.max_size === 65536, "shipped ini: max_size");
check_str(rules.bios_names.join(","), "exec.bin,grom.bin", "shipped ini: bios_names");
check_str(rules.bios_words.join(","), "bios", "shipped ini: bios_words");
```

If `test_lobby_headless.js` has no `check`/`check_str` helpers, add the same two
functions used in `exec/tests/syncretro_config_test.js` (Task 2, Step 1).

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /home/rswindell/sbbs && jsexec xtrn/syncivision/test_lobby_headless.js
```

Expected: FAIL on every `shipped ini:` check — there is no `syncretro.ini`.

- [ ] **Step 3: Write the shipped `xtrn/syncivision/syncretro.ini`**

Create it with the header below, then the sections from the two tables above,
then `[video]`, `[audio]`, `[idle]`, `[lobby]`, `[text]`, `[disc]`, `[input]`,
`[debug]` and `[options]` carried over from `syncretro.example.ini` **with their
values and comments unchanged**.

Move each comment to sit beside the key it explains. The comments in
`lobby.js` that explain a console fact — why `profile = intv` means the keypad
rides the disc angle, why the Intellivision needs `exec.bin`/`grom.bin`, why the
BIOS-name and BIOS-word nets exist — move **verbatim** into this file beside
those keys, converted from `//` and `/* */` to `;`. Nothing is rewritten and
nothing is dropped; a comment that explained a value follows that value here.

Header:

```ini
; SyncRetro -- Intellivision.  SHIPPED WITH THE DOOR.
;
; EDIT syncretro.local.ini, NOT THIS FILE. This one is ours and an upgrade will
; replace it. Yours sits beside it, is read second, and wins key by key -- so it
; holds only what you changed, and every default and new setting added upstream
; keeps arriving underneath it.
;
; Read by BOTH halves of the door: the lobby (lobby.js, via
; exec/load/syncretro_lobby.js) and the native binary. That is the whole reason
; it is a file rather than either one's code -- these facts used to travel from
; the lobby to the door on the COMMAND LINE, which Synchronet assembles into a
; 260-byte buffer on Windows and truncates there without a word, silently
; cutting the ROM argument off the end of a long cartridge name.
;
; Display names for individual cartridges live in games.ini, beside this file,
; with your own in games.local.ini.
;
; THIS FILE IS AUTHORITATIVE. The door and the lobby both carry compiled
; defaults for every setting, but those exist only so a damaged install still
; starts -- what a working install uses is what is written here, and what you
; write over it.
```

The `[console]` section additionally carries this note, because the merge gave
up a guarantee the deleted `console.ini` enforced structurally — it had no
override file at all, so an install could not redefine which console it is:

```ini
[console]
; WHAT THIS CONSOLE IS. `name`, `short`, `core` and `profile` are facts about
; what we shipped: overriding them in syncretro.local.ini is possible and is
; almost certainly a mistake -- `short` in particular derives the id that names
; your players' save directory, so changing it strands every save they have.
; Keys added here later may be claims about what the core can do rather than
; facts about the package, and those ARE yours to correct; each says so.
```

- [ ] **Step 4: Collapse `xtrn/syncivision/lobby.js`**

Replace the whole file with:

```js
/* SyncRetro -- Intellivision lobby.
 *
 * What this console IS -- its name, core, key-binding profile, what a cartridge
 * looks like, which BIOS images it needs -- lives in syncretro.ini beside this
 * file, which the native door reads too. Your own settings go in
 * syncretro.local.ini.
 */
load("syncretro_lobby.js");

syncretro_lobby();
```

- [ ] **Step 5: Delete the two obsolete files and the installer step**

```bash
cd /home/rswindell/sbbs
git rm xtrn/syncivision/console.ini xtrn/syncivision/syncretro.example.ini
```

In `xtrn/syncivision/install-xtrn.ini`, delete the `[copy:syncretro.example.ini]`
block at line 56 **and** the comment block above it that explains the seeding
(the paragraph beginning "Seed the live configuration"). Nothing replaces them:
the shipped file arrives with the package.

In `xtrn/syncivision/.gitignore`, delete the `/syncretro.ini` line and the
comment block above it that claims the live `syncretro.ini` "is intentionally
NOT ignored here" — both are now wrong, and they contradicted each other.

- [ ] **Step 6: Run the test to verify it passes**

```bash
cd /home/rswindell/sbbs && jsexec xtrn/syncivision/test_lobby_headless.js
```

Expected: 0 failures.

- [ ] **Step 7: Verify the package has no stale references**

```bash
cd /home/rswindell/sbbs
grep -rn "console\.ini\|syncretro\.example\.ini" xtrn/syncivision/ || echo "clean"
```

Expected: `clean`. Any hit is a doc or installer line that still names a deleted
file; fix it before committing.

Then check that no key was dropped in the carry-over. The deleted files are
`git rm`'d but not yet committed, so `HEAD` still has them:

```bash
cd /home/rswindell/sbbs
PKG=syncivision
keys() { grep -ohE '^[a-zA-Z_][a-zA-Z0-9_.]*[ 	]*=' | sed 's/[ 	=]*$//' | sort -u; }
{ git show HEAD:xtrn/$PKG/syncretro.example.ini
  git show HEAD:xtrn/$PKG/console.ini; } | keys > old.keys
keys < xtrn/$PKG/syncretro.ini > new.keys
comm -23 old.keys new.keys
rm -f old.keys new.keys
```

Expected: **no output** — every key the two deleted files declared appears in the
shipped `syncretro.ini`. Any name printed is a setting that was silently lost;
add it back before committing.

- [ ] **Step 8: Commit**

```bash
cd /home/rswindell/sbbs
git add xtrn/syncivision/
git commit -F <(printf '%s\n' \
'syncivision: one shipped syncretro.ini declares the console' '' \
'console.ini, the installer-copied syncretro.example.ini and the spec' \
'object in lobby.js all described the same console, the first two' \
'overlapping on four keys. One shipped syncretro.ini now carries all of' \
'it, with the sysop'"'"'s syncretro.local.ini read over the top, and lobby.js' \
'is two lines.' '' \
'Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>')
```

---

### Task 5: Migrate the syncnes package

**Files:**
- Create: `xtrn/syncnes/syncretro.ini`
- Delete: `xtrn/syncnes/console.ini`, `xtrn/syncnes/syncretro.example.ini`
- Modify: `xtrn/syncnes/lobby.js`, `xtrn/syncnes/install-xtrn.ini:53`,
  `xtrn/syncnes/.gitignore`
- Test: `xtrn/syncnes/test_shipped_ini.js` (new)

**Interfaces:**
- Consumes: `syncretro_lobby_ini()`, `syncretro_console()`, `syncretro_rules()`.
- Produces: nothing later tasks depend on.

**The `[console]` section:**

| key | value | from |
|---|---|---|
| `name` | `Nintendo Entertainment System` | `console.ini`, `lobby.js:20` |
| `short` | `NES` | `console.ini`, `lobby.js:21` |
| `core` | `fceumm_libretro` | `console.ini`, `lobby.js:22` |
| `profile` | `pad` | `console.ini`, `lobby.js:25` |
| `shared_saves` | `false` | `syncretro_lib.js:346` default |
| `bios` | *(empty)* | `lobby.js:42` |

**The `[roms]` section:**

| key | value | from |
|---|---|---|
| `dir` | `roms` | `syncretro_rules()` default |
| `ext` | `nes, unf, unif` | `lobby.js:33` |
| `min_size` | `8192` | `lobby.js:36` (`8 * 1024`) |
| `max_size` | `4194304` | `lobby.js:37` (`4 * 1024 * 1024`) |

- [ ] **Step 1: Write the failing test**

Create `xtrn/syncnes/test_shipped_ini.js`:

```js
// test_shipped_ini.js -- the shipped syncretro.ini must reproduce exactly what
// the deleted lobby.js spec and console.ini declared. `id` in particular names
// the per-user save directory, so a typo silently strands every player's saves.
// Run: jsexec xtrn/syncnes/test_shipped_ini.js

load("syncretro_lobby.js");

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

var shipped = syncretro_lobby_ini(js.exec_dir);
var con = syncretro_console(shipped.console);
var rules = syncretro_rules(shipped.roms);

check_str(con.name, "Nintendo Entertainment System", "name");
check_str(con.short, "NES", "short");
check_str(con.id, "nes", "id (names the save dir)");
check_str(con.core, "fceumm_libretro", "core");
check_str(con.profile, "pad", "profile");
check(con.shared_saves === false, "shared_saves false");
check_str(rules.ext.join(","), "nes,unf,unif", "ext");
check(rules.min_size === 8192, "min_size");
check(rules.max_size === 4194304, "max_size");

// .fds is DELIBERATELY absent: an FDS image needs disksys.rom, which is
// copyrighted content the sysop supplies. Without it fceumm fails the load and
// the player just sees a door that will not start.
check(rules.ext.join(",").indexOf("fds") < 0, "fds not enabled by default");

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /home/rswindell/sbbs && jsexec xtrn/syncnes/test_shipped_ini.js
```

Expected: FAIL on every check — there is no `syncretro.ini`.

- [ ] **Step 3: Write the shipped `xtrn/syncnes/syncretro.ini`**

Use `xtrn/syncivision/syncretro.ini` -- created by the previous task and in the
tree now -- as the structural model: read it first, then write this one with the
same header, section order and comment placement, substituting this package's
values from the two tables above and "Nintendo Entertainment System" for
"Intellivision" in the header.

Carry `[video]`, `[audio]`, `[idle]`, `[lobby]`, `[text]`, `[input]`, `[debug]`
and `[options]` over from `xtrn/syncnes/syncretro.example.ini` with their values
and comments unchanged. Note this package's `syncretro.example.ini` carries the
full `[video] aspect` explanation that the other two cross-reference — keep it
here, and keep the cross-references pointing at this file under its new name.

Move each `lobby.js` comment that explains a console fact — why a NES controller
needs no console-specific C, why `.fds` is excluded pending `disksys.rom`, the
8 KB-to-4 MB cartridge range — **verbatim** into this file beside the key it
explains, converted from `//` and `/* */` to `;`.

- [ ] **Step 4: Collapse `xtrn/syncnes/lobby.js`**

Replace the whole file with:

```js
/* SyncRetro -- Nintendo Entertainment System lobby.
 *
 * What this console IS -- its name, core, key-binding profile and what a
 * cartridge looks like -- lives in syncretro.ini beside this file, which the
 * native door reads too. Your own settings go in syncretro.local.ini.
 */
load("syncretro_lobby.js");

syncretro_lobby();
```

- [ ] **Step 5: Delete the two obsolete files and the installer step**

```bash
cd /home/rswindell/sbbs
git rm xtrn/syncnes/console.ini xtrn/syncnes/syncretro.example.ini
```

In `xtrn/syncnes/install-xtrn.ini`, delete the `[copy:syncretro.example.ini]`
block at line 53 and the comment block above it that explains the seeding.

In `xtrn/syncnes/.gitignore`, delete the `/syncretro.ini` line at line 32, the
comment above it, and the contradicting comment block at lines 10-12 that claims
the live `syncretro.ini` "is intentionally NOT ignored here". Also correct the
file's opening comment (line 4), which lists `syncretro.ini` among the
version-controlled files — that is now true, and the sentence should say so
rather than describe it as the seeded copy. Leave the `disksys.rom` line, which
still refers to a sysop-supplied BIOS, but update its cross-reference from "if
you enable .fds in syncretro.ini" to `syncretro.local.ini`.

- [ ] **Step 6: Run the test to verify it passes**

```bash
cd /home/rswindell/sbbs && jsexec xtrn/syncnes/test_shipped_ini.js
```

Expected: `ok: 0 failures`.

- [ ] **Step 7: Verify the package has no stale references**

```bash
cd /home/rswindell/sbbs
grep -rn "console\.ini\|syncretro\.example\.ini" xtrn/syncnes/ || echo "clean"
```

Expected: `clean`.

Then check that no key was dropped in the carry-over. The deleted files are
`git rm`'d but not yet committed, so `HEAD` still has them:

```bash
cd /home/rswindell/sbbs
PKG=syncnes
keys() { grep -ohE '^[a-zA-Z_][a-zA-Z0-9_.]*[ 	]*=' | sed 's/[ 	=]*$//' | sort -u; }
{ git show HEAD:xtrn/$PKG/syncretro.example.ini
  git show HEAD:xtrn/$PKG/console.ini; } | keys > old.keys
keys < xtrn/$PKG/syncretro.ini > new.keys
comm -23 old.keys new.keys
rm -f old.keys new.keys
```

Expected: **no output** — every key the two deleted files declared appears in the
shipped `syncretro.ini`. Any name printed is a setting that was silently lost;
add it back before committing.

- [ ] **Step 8: Commit**

```bash
cd /home/rswindell/sbbs
git add xtrn/syncnes/
git commit -F <(printf '%s\n' \
'syncnes: one shipped syncretro.ini declares the console' '' \
'console.ini, the installer-copied syncretro.example.ini and the spec' \
'object in lobby.js all described the same console. One shipped' \
'syncretro.ini now carries all of it, with the sysop'"'"'s' \
'syncretro.local.ini read over the top, and lobby.js is two lines.' '' \
'Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>')
```

---

### Task 6: Migrate the syncarcade package

**Files:**
- Create: `xtrn/syncarcade/syncretro.ini`
- Delete: `xtrn/syncarcade/console.ini`,
  `xtrn/syncarcade/syncretro.example.ini`
- Modify: `xtrn/syncarcade/lobby.js`, `xtrn/syncarcade/install-xtrn.ini:64`,
  `xtrn/syncarcade/.gitignore`
- Test: `xtrn/syncarcade/test_shipped_ini.js` (new)

**Interfaces:**
- Consumes: `syncretro_lobby_ini()`, `syncretro_console()`, `syncretro_rules()`.
- Produces: nothing later tasks depend on.

**The `[console]` section:**

| key | value | from |
|---|---|---|
| `name` | `Arcade` | `console.ini`, `lobby.js:25` |
| `short` | `Arcade` | `console.ini`, `lobby.js:26` |
| `core` | `mame2003_plus_libretro` | `console.ini`, `lobby.js:27` |
| `profile` | `arcade` | `console.ini`, `lobby.js:38` |
| `shared_saves` | `true` | `lobby.js:79` — **the only package where this is true** |
| `bios` | *(empty)* | `lobby.js:62` |

**The `[roms]` section:**

| key | value | from |
|---|---|---|
| `dir` | `roms` | `syncretro_rules()` default |
| `ext` | `zip` | `lobby.js:48` |
| `min_size` | `1024` | `lobby.js:55` |
| `max_size` | `67108864` | `lobby.js:56` (`64 * 1024 * 1024`) |

- [ ] **Step 1: Write the failing test**

Create `xtrn/syncarcade/test_shipped_ini.js`:

```js
// test_shipped_ini.js -- the shipped syncretro.ini must reproduce exactly what
// the deleted lobby.js spec and console.ini declared. shared_saves is the one
// that matters most here: false would give every player a private cabinet and
// silently empty the machine's high-score table.
// Run: jsexec xtrn/syncarcade/test_shipped_ini.js

load("syncretro_lobby.js");

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

var shipped = syncretro_lobby_ini(js.exec_dir);
var con = syncretro_console(shipped.console);
var rules = syncretro_rules(shipped.roms);

check_str(con.name, "Arcade", "name");
check_str(con.short, "Arcade", "short");
check_str(con.id, "arcade", "id (names the shared save dir)");
check_str(con.core, "mame2003_plus_libretro", "core");
check_str(con.profile, "arcade", "profile");
check(con.shared_saves === true, "shared_saves TRUE: one cabinet, one table");
check_str(rules.ext.join(","), "zip", "ext");
check(rules.min_size === 1024, "min_size");
check(rules.max_size === 67108864, "max_size");

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /home/rswindell/sbbs && jsexec xtrn/syncarcade/test_shipped_ini.js
```

Expected: FAIL on every check — there is no `syncretro.ini`.

- [ ] **Step 3: Write the shipped `xtrn/syncarcade/syncretro.ini`**

Use `xtrn/syncivision/syncretro.ini` -- created two tasks ago and in the tree
now -- as the structural model: read it first, then write this one with the same
header, section order and comment placement, substituting this package's values
from the two tables above and "Arcade (MAME 2003-Plus)" for "Intellivision" in
the header.

Carry `[video]`, `[audio]`, `[idle]`, `[lobby]`, `[text]`, `[input]`, `[debug]`
and `[options]` over from `xtrn/syncarcade/syncretro.example.ini` with their
values and comments unchanged — including the `[video] aspect` block's
explanation of why it is deliberately unset here (an upright cabinet is already
tall, and MAME reports a usable per-game aspect of its own), and the `[options]`
block's two pinned options.

Move each `lobby.js` comment that explains a console fact — why the `arcade`
profile exists when it binds what `pad` binds, why a romset's FILENAME IS DATA
and must not be renamed, the measured 1.8 KB-to-15 MB size band across 272
romsets, why there is no BIOS to reject — **verbatim** into this file beside the
key it explains, converted from `//` and `/* */` to `;`.

`shared_saves = true` needs its comment carried over too: the high-score table
is the whole point of the machine, so every player is pointed at one save
directory instead of their own.

- [ ] **Step 4: Collapse `xtrn/syncarcade/lobby.js`**

Replace the whole file with:

```js
/* SyncRetro -- Arcade lobby (MAME 2003-Plus).
 *
 * What this cabinet IS -- its core, key-binding profile, what a romset looks
 * like, and that every player shares one save directory so the high-score table
 * is the machine's -- lives in syncretro.ini beside this file, which the native
 * door reads too. Your own settings go in syncretro.local.ini; per-cabinet
 * button labels go in games.local.ini.
 */
load("syncretro_lobby.js");

syncretro_lobby();
```

- [ ] **Step 5: Delete the two obsolete files and the installer step**

```bash
cd /home/rswindell/sbbs
git rm xtrn/syncarcade/console.ini xtrn/syncarcade/syncretro.example.ini
```

In `xtrn/syncarcade/install-xtrn.ini`, delete the
`[copy:syncretro.example.ini]` block at line 64 and the comment block above it
("Seed the live configuration…").

In `xtrn/syncarcade/.gitignore`, delete the `/syncretro.ini` line and its
comment. Keep the `games.local.ini` comment block — it is still correct and now
describes the same rule the whole package follows.

- [ ] **Step 6: Run the test to verify it passes**

```bash
cd /home/rswindell/sbbs && jsexec xtrn/syncarcade/test_shipped_ini.js
```

Expected: `ok: 0 failures`.

- [ ] **Step 7: Verify the package has no stale references**

```bash
cd /home/rswindell/sbbs
grep -rn "console\.ini\|syncretro\.example\.ini" xtrn/syncarcade/ || echo "clean"
```

Expected: `clean`.

Then check that no key was dropped in the carry-over. The deleted files are
`git rm`'d but not yet committed, so `HEAD` still has them:

```bash
cd /home/rswindell/sbbs
PKG=syncarcade
keys() { grep -ohE '^[a-zA-Z_][a-zA-Z0-9_.]*[ 	]*=' | sed 's/[ 	=]*$//' | sort -u; }
{ git show HEAD:xtrn/$PKG/syncretro.example.ini
  git show HEAD:xtrn/$PKG/console.ini; } | keys > old.keys
keys < xtrn/$PKG/syncretro.ini > new.keys
comm -23 old.keys new.keys
rm -f old.keys new.keys
```

Expected: **no output**. This package's `[options]` pins two core options, and
they must survive the carry-over — a name printed here is a pinned option that
was silently lost.

- [ ] **Step 8: Commit**

```bash
cd /home/rswindell/sbbs
git add xtrn/syncarcade/
git commit -F <(printf '%s\n' \
'syncarcade: one shipped syncretro.ini declares the cabinet' '' \
'console.ini, the installer-copied syncretro.example.ini and the spec' \
'object in lobby.js all described the same cabinet. One shipped' \
'syncretro.ini now carries all of it -- including shared_saves, which' \
'points every player at one save directory so the high-score table is the' \
'machine'"'"'s -- with the sysop'"'"'s syncretro.local.ini read over the top.' '' \
'Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>')
```

---

### Task 7: Documentation and final verification

**Files:**
- Modify: `xtrn/syncivision/README.md`, `xtrn/syncnes/README.md`,
  `xtrn/syncarcade/README.md`
- Modify: `src/doors/syncretro/GAMES_INI.md` (§14)
- Modify: `src/doors/syncretro/README.md`, `src/doors/syncretro/DESIGN.md`
  (only where they name `console.ini` or `syncretro.example.ini`)

**Interfaces:**
- Consumes: the finished behavior from Tasks 1-6.
- Produces: nothing.

- [ ] **Step 1: Find every remaining reference**

```bash
cd /home/rswindell/sbbs
grep -rn "console\.ini\|syncretro\.example\.ini" \
    src/doors/syncretro/ exec/load/syncretro_*.js xtrn/sync{ivision,nes,arcade}/
```

Expected: a list of doc lines only — no code hits. Every hit is fixed in this
task. If a hit is in a `.c`, `.h` or `.js` file, an earlier task missed it; fix
it there and re-run its tests.

- [ ] **Step 2: Add the one rule to each package README**

In each of the three `xtrn/*/README.md`, add this section immediately after the
installation instructions, with the console's own name substituted:

```markdown
## Configuring it

Two files, and only the second is yours:

- **`syncretro.ini`** — ours. Shipped with the door, documented in place, and
  replaced by every upgrade. It declares what this console is and what every
  setting defaults to. **Do not edit it**; your changes would be lost, and a
  `git pull` would refuse to run until you put them back.
- **`syncretro.local.ini`** — yours. Create it beside `syncretro.ini` and put in
  only the settings you want to change. It is read second and wins key by key,
  so everything you did not mention keeps following our defaults — including
  settings added in later releases.

Per-game display names and button labels work the same way: `games.ini` is ours,
`games.local.ini` is yours.

### Upgrading from an earlier release

If you already have a `syncretro.ini` you edited, **rename it to
`syncretro.local.ini` before you update**. It keeps working exactly as it did —
a full config read as an overlay simply wins on every key — and you can trim it
down to just your own changes whenever you like. If you update without renaming
it, your settings are replaced by ours without warning.
```

- [ ] **Step 3: Restate `GAMES_INI.md` §14**

Rewrite §14's opening so it presents the games pair as one instance of the
package-wide rule rather than a local exception. Keep the section's existing
resolution examples and its warning about section-versus-root scope. The
sentence that must appear:

> `games.ini` is ours and `games.local.ini` is yours, exactly as `syncretro.ini`
> is ours and `syncretro.local.ini` is yours. Every configuration file in this
> door works that way: the shipped file holds the documented defaults, your
> `.local` twin holds only what you changed, and yours is read second.

- [ ] **Step 4: Fix the remaining doc references**

Update every hit from Step 1 in `src/doors/syncretro/README.md`,
`DESIGN.md` and any other doc, replacing:

- "console.ini" → "syncretro.ini `[console]`"
- "syncretro.example.ini" → "syncretro.ini" (and drop any sentence about the
  installer copying it)
- "NOT syncretro.ini: that one is the SYSOP's" → the sysop's file is
  `syncretro.local.ini`

`DESIGN.md:151` and `:302-303` describe `GET_SAVE_DIRECTORY` and the BIOS dir and
do **not** need changing — they name directories, not config files. Read before
editing.

- [ ] **Step 5: Full verification sweep**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake --build build-test -j4 && (cd build-test && ctest --output-on-failure)
./build.sh
cd /home/rswindell/sbbs
jsexec exec/tests/syncretro_config_test.js
jsexec xtrn/syncivision/test_lobby_headless.js
jsexec xtrn/syncnes/test_shipped_ini.js
jsexec xtrn/syncarcade/test_shipped_ini.js
grep -rn "console\.ini\|syncretro\.example\.ini" \
    src/doors/syncretro/ exec/load/ xtrn/sync{ivision,nes,arcade}/ || echo "clean"
git status --short xtrn/sync{ivision,nes,arcade}/
```

Expected: every ctest passes; the door builds; all four JS tests print
`0 failures`; the grep prints `clean`; `git status` shows no untracked
`console.ini` or `syncretro.example.ini` left behind.

- [ ] **Step 6: Confirm the shipped files are tracked and the overlays are not**

```bash
cd /home/rswindell/sbbs
git ls-files xtrn/sync{ivision,nes,arcade}/syncretro.ini
printf 'x\n' > xtrn/syncnes/syncretro.local.ini
git status --short xtrn/syncnes/ | grep syncretro.local.ini && echo "BAD: overlay is visible to git" || echo "ok: overlay ignored"
rm xtrn/syncnes/syncretro.local.ini
```

Expected: three tracked `syncretro.ini` paths listed, and
`ok: overlay ignored` — the root `.gitignore`'s tree-wide `*.local.ini` rule
covers it with no per-package line.

- [ ] **Step 7: Commit**

```bash
cd /home/rswindell/sbbs
git add xtrn/sync{ivision,nes,arcade}/README.md src/doors/syncretro/
git commit -F <(printf '%s\n' \
'syncretro: document the shipped-ini and sysop-overlay rule' '' \
'One rule across the door: the shipped file holds the documented' \
'defaults, the sysop'"'"'s .local twin holds only what changed, and the twin' \
'is read second. GAMES_INI.md section 14 now presents the games pair as' \
'an instance of that rule rather than a local exception, and each' \
'package README carries the rule and the rename an existing install needs' \
'before upgrading.' '' \
'Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>')
```

---

## After the plan

Do **not** push. Report what was built and let the user push.

The follow-on spec — per-user save/restore — depends on this landing: it adds a
`[console] save_state` key with a per-romset override in `games.local.ini`, and
reads `shared_saves` from the same `[console]` section in both halves of the
door.
