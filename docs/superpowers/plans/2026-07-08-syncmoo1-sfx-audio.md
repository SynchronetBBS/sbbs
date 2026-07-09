# syncmoo1 SFX Audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the syncmoo1 door sound effects over SyncTERM's audio APC by implementing 1oom's `hw_audio_sfx_*` contract on termgfx's audio manager.

**Architecture:** A new door module `syncmoo1_audio.c` hashes each raw VOC sample 1oom registers into a content-addressed cache leaf (`s_<fnv1a8>`), queues it, and Stores it once the terminal's audio tier is known. termgfx gains a name-addressed SFX pair (`_store` / `_play_named`) alongside its existing id-addressed one, so SyncDuke and SyncDOOM are untouched.

**Tech Stack:** C (C11), CMake, termgfx audio manager (`audio_mgr.c` over the SyncTERM audio APC), vendored 1oom `hw` backend seam.

See the spec: [../specs/2026-07-08-syncmoo1-sfx-audio-design.md](../specs/2026-07-08-syncmoo1-sfx-audio-design.md)

## Global Constraints

- **Never edit the vendored `1oom/` tree.** All door logic lives in `hw_sbbs.c` + `syncmoo1_*.c` (`src/doors/syncmoo1/CLAUDE.md`).
- **syncmoo1 house style is 4-space indent, NOT tabs.** Do not run uncrustify over `syncmoo1_*.c`/`hw_sbbs.c`.
- **termgfx house style IS uncrustify tabs.** Run `uncrustify -c ../../uncrustify.cfg --replace --no-backup <file>` as the closing step of every termgfx C change.
- **termgfx is shared** with SyncDOOM and SyncDuke (`termgfx/CLAUDE.md`). Every change here is additive; existing symbols keep byte-for-byte behavior. After any termgfx edit, both other doors must still build.
- **Do not use `fmt_sfx_convert()`.** termgfx's `sfx_cache_file_once()` already transcodes VOC. Raw LBX bytes go straight to termgfx.
- **`hw_audio_sfx_batch_start(max)` is a capacity hint, not a reset.** It must never clear the slot table.
- **Tests must run their asserts.** `tests/CMakeLists.txt` passes `-UNDEBUG` because the Release build defines `NDEBUG`, which would compile every `assert()` to nothing.
- **Commit messages wrap at 78 columns.** Verify with `awk 'length > 78' <msgfile>` printing nothing.
- Build with `./build.sh` from the door directory. Run unit tests with `./build/tests/<name>`.

---

## File Structure

| File | Responsibility |
|---|---|
| `src/doors/termgfx/hash.h` / `hash.c` | **Create.** `termgfx_fnv1a()` — one home for the hash both other doors copy. |
| `src/doors/termgfx/audio_mgr.c` | **Modify.** Extract `sfx_store_bytes()`; add name-addressed store/play + their bookkeeping. |
| `src/doors/termgfx/audio_mgr.h` | **Modify.** Declare the two new functions. |
| `src/doors/termgfx/CMakeLists.txt` | **Modify.** Add `hash.c`. |
| `src/doors/syncmoo1/syncmoo1_audio.c` / `.h` | **Create.** 1oom `hw_audio_sfx_*` contract over termgfx. Owns the slot table, pending-store queue, volume. |
| `src/doors/syncmoo1/hw_sbbs.c` | **Modify.** `hw_audio_sfx_*` stubs become forwarders. |
| `src/doors/syncmoo1/syncmoo1_io.c` | **Modify.** Create the audio manager, set cache prefix, probe at terminal enter. |
| `src/doors/syncmoo1/syncmoo1_input.c` | **Modify.** Feed inbound bytes to `termgfx_audio_feed()`. |
| `src/doors/syncmoo1/syncmoo1.h` | **Modify.** Declare the cross-module audio entry points. |
| `src/doors/syncmoo1/CMakeLists.txt` | **Modify.** Add `syncmoo1_audio.c`. |
| `src/doors/syncmoo1/tests/test_audio.c` | **Create.** Unit tests for the pure parts. |
| `src/doors/syncmoo1/tests/CMakeLists.txt` | **Modify.** Add the `test_audio` target. |

---

### Task 1: `termgfx_fnv1a()` + its test target

Gives the content hash one home. `syncdoom/i_termmusic.c`'s `term_hash` and `syncduke`'s copy are left alone — this only stops syncmoo1 becoming the third.

**Files:**
- Create: `src/doors/termgfx/hash.h`, `src/doors/termgfx/hash.c`
- Modify: `src/doors/termgfx/CMakeLists.txt`
- Create: `src/doors/syncmoo1/tests/test_audio.c`
- Modify: `src/doors/syncmoo1/tests/CMakeLists.txt`

**Interfaces:**
- Produces: `uint32_t termgfx_fnv1a(const void *data, size_t len);`
- Consumes: nothing.

- [ ] **Step 1: Write the failing test.** Create `src/doors/syncmoo1/tests/test_audio.c`:

```c
#include <assert.h>
#include <string.h>
#include "hash.h"   /* termgfx: termgfx_fnv1a */

int main(void)
{
    /* Canonical FNV-1a 32-bit vectors (offset basis 0x811c9dc5, prime 0x01000193). */
    assert(termgfx_fnv1a("", 0)          == 0x811c9dc5u);
    assert(termgfx_fnv1a("a", 1)         == 0xe40c292cu);
    assert(termgfx_fnv1a("foobar", 6)    == 0xbf9cf968u);

    /* Binary-safe: embedded NULs are hashed, not treated as terminators. */
    assert(termgfx_fnv1a("a\0b", 3) != termgfx_fnv1a("a\0c", 3));

    /* Same bytes -> same hash (the property content-addressing depends on). */
    {
        const char v[] = "Creative Voice File\x1a";
        assert(termgfx_fnv1a(v, sizeof v - 1) == termgfx_fnv1a(v, sizeof v - 1));
    }
    return 0;
}
```

- [ ] **Step 2: Add the test target.** In `src/doors/syncmoo1/tests/CMakeLists.txt`, append:

```cmake
# Pure unit tests for the audio module's content hash + helpers. Same rules as
# test_map: dependency-free SOURCES (never the termgfx library, which drags in
# C++/libADLMIDI), and -UNDEBUG so the assert()s actually run.
add_executable(test_audio
    ${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx/hash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_audio.c)

target_include_directories(test_audio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx)

set_target_properties(test_audio PROPERTIES C_STANDARD 11)
target_compile_options(test_audio PRIVATE -UNDEBUG)
add_test(NAME test_audio COMMAND test_audio)
```

- [ ] **Step 3: Run it to verify it fails.**

Run: `cd src/doors/syncmoo1 && ./build.sh 2>&1 | grep -E 'error|hash.h'`
Expected: FAIL — `fatal error: hash.h: No such file or directory`.

- [ ] **Step 4: Write the implementation.** Create `src/doors/termgfx/hash.h`:

```c
#ifndef TERMGFX_HASH_H_
#define TERMGFX_HASH_H_

#include <stddef.h>
#include <stdint.h>

// 32-bit FNV-1a over `len` bytes. Binary-safe (embedded NULs are data).
//
// This is the hash the doors already content-address their SyncTERM client-cache
// names with: syncdoom/i_termmusic.c builds "d_%08x" from it, and syncduke does
// the same for its MIDI tracks. It lives here so a third door does not copy it
// again. Not a security hash -- it is a cache key: identical bytes must yield
// identical names, across sessions and across hosts.
uint32_t termgfx_fnv1a(const void *data, size_t len);

#endif // TERMGFX_HASH_H_
```

Create `src/doors/termgfx/hash.c`:

```c
#include "hash.h"

uint32_t
termgfx_fnv1a(const void *data, size_t len)
{
	const uint8_t *p = (const uint8_t *)data;
	uint32_t       h = 0x811c9dc5u;          // FNV offset basis
	size_t         i;

	for (i = 0; i < len; ++i) {
		h ^= p[i];
		h *= 0x01000193u;                    // FNV prime
	}
	return h;
}
```

- [ ] **Step 5: Add `hash.c` to the termgfx library.** In `src/doors/termgfx/CMakeLists.txt`, add `hash.c` to the `add_library(termgfx STATIC ...)` list, on the line after `geometry.c`.

- [ ] **Step 6: Run the test to verify it passes.**

Run: `cd src/doors/syncmoo1 && ./build.sh >/dev/null && ./build/tests/test_audio && echo PASS`
Expected: `PASS` (exit 0).

- [ ] **Step 7: Verify the test actually asserts.** Temporarily break the prime (`0x01000194u`), rebuild, run.

Run: `./build/tests/test_audio; echo "exit=$?"`
Expected: nonzero exit with an `Assertion ... failed` message. **Restore the correct prime and re-run to PASS before committing.**

- [ ] **Step 8: Verify the shared library's other consumers still build.**

Run: `cd src/doors && for d in syncdoom syncduke syncmoo1; do (cd $d && ./build.sh >/dev/null 2>&1 && echo "$d OK") || echo "$d FAIL"; done`
Expected: three `OK` lines.

- [ ] **Step 9: Style + commit.**

```bash
cd src/doors/termgfx && uncrustify -c ../../uncrustify.cfg --replace --no-backup hash.c hash.h
cd /home/rswindell/sbbs
git add src/doors/termgfx/hash.c src/doors/termgfx/hash.h
git commit -F - -- src/doors/termgfx/hash.c src/doors/termgfx/hash.h \
  src/doors/termgfx/CMakeLists.txt src/doors/syncmoo1/tests/test_audio.c \
  src/doors/syncmoo1/tests/CMakeLists.txt <<'EOF'
termgfx: add termgfx_fnv1a() -- one home for the cache-name hash

syncdoom (i_termmusic.c's term_hash) and syncduke each carry their own
32-bit FNV-1a to content-address SyncTERM client-cache names as
"d_<hash8>".  syncmoo1 needs the same for its SFX, so give the hash a
single home rather than a third copy.

Additive: no existing symbol or behaviour changes, and syncdoom and
syncduke both still build.  The existing copies are left in place; they
can adopt this when convenient.

Tested against the canonical FNV-1a vectors and for binary safety
(embedded NULs hashed as data), with -UNDEBUG on the test target so the
assert()s actually run.
EOF
```

---

### Task 2: Name-addressed SFX in termgfx

Today the SFX cache leaf is `snprintf(leaf, "%d", id)` and the Store happens lazily inside `termgfx_audio_sfx_file()` on first *play*, guarded by `sfx_up[id]`. Neither eager Store nor a caller-supplied name is expressible. Add both, without touching the id path.

**Files:**
- Modify: `src/doors/termgfx/audio_mgr.c`
- Modify: `src/doors/termgfx/audio_mgr.h`

**Interfaces:**
- Consumes: `termgfx_fnv1a()` (Task 1) — indirectly; callers pass the leaf.
- Produces:
  - `void termgfx_audio_sfx_store(termgfx_audio_t *m, const char *leaf, const void *filedata, size_t filelen);`
  - `void termgfx_audio_sfx_play_named(termgfx_audio_t *m, const char *leaf, int vol, int pan);`

- [ ] **Step 1: Extract the store body, keeping the id path identical.** In `audio_mgr.c`, replace the whole of `sfx_cache_file_once()` (currently at ~line 470) with:

```c
// Transcode + Store one sample under cache name `fn`. Returns its exact play
// time in ms (0 = unknown). No "already stored" bookkeeping -- the id path and
// the name path each own theirs.
static int sfx_store_bytes(termgfx_audio_t *m, const char *fn,
                           const void *filedata, size_t filelen)
{
	uint8_t *pcm = NULL;
	int      rate = 0;
	size_t   n;
	int      dur = 0;

	n = termgfx_audio_voc_to_pcm(filedata, filelen, &pcm, &rate);
	if (n > 0) {
		send_buf(m, termgfx_audio_cache_pcm(&m->buf, &m->cap, fn, pcm, n, 8, 1, rate),
		         TERMGFX_AUDIO_PRIO);
		if (rate > 0)   // exact play time (8-bit mono PCM) for the busy tracking
			dur = (int)((uint64_t)n * 1000u / (uint64_t)rate);
		free(pcm);
	}
	else {
		send_buf(m, termgfx_audio_cache_file(&m->buf, &m->cap, fn, filedata, filelen),
		         TERMGFX_AUDIO_PRIO);
		dur = sfx_file_dur_ms(filedata, filelen);
	}
	return dur;
}

static void sfx_cache_file_once(termgfx_audio_t *m, int id,
                                const void *filedata, size_t filelen, const char *fn)
{
	if (m->sfx_up[id])
		return;
	m->sfx_dur[id] = sfx_store_bytes(m, fn, filedata, filelen);
	m->sfx_up[id] = 1;
}
```

- [ ] **Step 2: Verify the id path is unchanged.**

Run: `cd src/doors && for d in syncdoom syncduke; do (cd $d && ./build.sh >/dev/null 2>&1 && echo "$d OK") || echo "$d FAIL"; done`
Expected: two `OK` lines. (Behavior is unchanged: `sfx_cache_file_once` still sets `sfx_dur[id]` and `sfx_up[id]` exactly as before.)

- [ ] **Step 3: Add the named bookkeeping.** In `audio_mgr.c`, add near the other `#define`s (after `#define KEYMAX`):

```c
#define NAMEDMAX     64              // distinct name-addressed SFX per session
#define NAMEDLEN     16              // leaf: "s_" + 8 hex + NUL, rounded up
```

and inside `struct termgfx_audio_s` (next to `sfx_up` / `sfx_dur`):

```c
	char named[NAMEDMAX][NAMEDLEN];        // name-addressed SFX Stored this session
	int  named_dur[NAMEDMAX];              // their exact play time (ms), 0 = unknown
	int  named_count;
```

- [ ] **Step 4: Add the lookup helper.** In `audio_mgr.c`, above `termgfx_audio_sfx_file()`:

```c
// Index of `leaf` among the name-addressed samples Stored this session, or -1.
static int named_find(const termgfx_audio_t *m, const char *leaf)
{
	int i;

	for (i = 0; i < m->named_count; ++i) {
		if (strcmp(m->named[i], leaf) == 0)
			return i;
	}
	return -1;
}
```

- [ ] **Step 5: Add the two public functions.** In `audio_mgr.c`, after `termgfx_audio_sfx_file()`:

```c
// ---- name-addressed SFX ---------------------------------------------------
// The id-addressed pair above keys the client-cache name on the caller's id
// ("<prefix>/sfx/<id>") and Stores lazily, on first play. These two key it on a
// caller-supplied leaf -- a content hash, e.g. "s_<fnv1a8>" -- and split Store
// from dispatch, so a door can upload eagerly while the engine is still loading.
//
// A content-addressed name is also what makes a cross-session upload skip sound:
// name-presence then means "the right bytes". (The C;L cache-list skip is still
// music-only; widening it to sfx/* is a separate change.)

void termgfx_audio_sfx_store(termgfx_audio_t *m, const char *leaf,
                             const void *filedata, size_t filelen)
{
	char fn[48];
	int  i;

	if (m == NULL || m->tier < 1 || leaf == NULL || leaf[0] == '\0' || filelen == 0)
		return;
	if (named_find(m, leaf) >= 0)          // already Stored this session
		return;
	if (m->named_count >= NAMEDMAX)
		return;                            // table full: this sample stays silent

	cache_name(m, "sfx", leaf, fn, sizeof(fn));
	i = m->named_count;
	strncpy(m->named[i], leaf, NAMEDLEN - 1);
	m->named[i][NAMEDLEN - 1] = '\0';
	m->named_dur[i] = sfx_store_bytes(m, fn, filedata, filelen);
	m->named_count = i + 1;
}

void termgfx_audio_sfx_play_named(termgfx_audio_t *m, const char *leaf,
                                  int vol, int pan)
{
	char fn[48];
	int  i;

	if (m == NULL || m->tier < 1 || leaf == NULL || leaf[0] == '\0')
		return;
	i = named_find(m, leaf);
	if (i < 0)                             // never Stored -> nothing to Load
		return;
	cache_name(m, "sfx", leaf, fn, sizeof(fn));
	sfx_dispatch(m, fn, vol, pan, m->named_dur[i]);
}
```

- [ ] **Step 6: Declare them.** In `audio_mgr.h`, after the `termgfx_audio_sfx_file()` declaration:

```c
// Name-addressed SFX. `leaf` is a caller-supplied cache-name component -- use a
// content hash (see hash.h) so identical bytes reuse one cache entry. Unlike the
// id-addressed pair above, Store and dispatch are separate: _store uploads once
// (no sound), _play_named dispatches an already-Stored leaf. Both no-op below
// tier 1, so a terminal without the audio APC costs nothing and stays silent.
void termgfx_audio_sfx_store(termgfx_audio_t *m, const char *leaf,
                             const void *filedata, size_t filelen);
void termgfx_audio_sfx_play_named(termgfx_audio_t *m, const char *leaf,
                                  int vol, int pan);
```

- [ ] **Step 7: Build all three doors.**

Run: `cd src/doors && for d in syncdoom syncduke syncmoo1; do (cd $d && ./build.sh >/dev/null 2>&1 && echo "$d OK") || echo "$d FAIL"; done`
Expected: three `OK` lines.

- [ ] **Step 8: Style + commit.**

```bash
cd src/doors/termgfx && uncrustify -c ../../uncrustify.cfg --replace --no-backup audio_mgr.c audio_mgr.h
cd /home/rswindell/sbbs
git commit -F - -- src/doors/termgfx/audio_mgr.c src/doors/termgfx/audio_mgr.h <<'EOF'
termgfx: add name-addressed SFX (store + play_named)

The SFX pair keys its client-cache name on the caller's id
("<prefix>/sfx/<id>", audio_mgr.c's snprintf(leaf, "%d", id)) and Stores
lazily, inside the play call, guarded by sfx_up[id].  Neither an eager
Store nor a caller-supplied name is expressible, and syncmoo1 needs both:
1oom registers every sample up front, before the audio tier is even
known, and content-addressed names are what let a later cross-session
upload skip be correct.

Add termgfx_audio_sfx_store() and termgfx_audio_sfx_play_named(), keyed
on a caller-supplied leaf with their own "Stored this session" table.
The transcode-and-Store body is extracted to sfx_store_bytes() and shared
with the id path, whose sfx_up[]/sfx_dur[] behaviour is unchanged.

Additive: SyncDOOM and SyncDuke keep the id path byte-for-byte and both
still build.
EOF
```

---

### Task 3: `syncmoo1_audio` — the pure parts, test-first

The door module's testable core: leaf naming, the volume map, the slot table, and dedupe. No engine or socket involved. `g_mgr == NULL` (no terminal) must be a safe no-op, which is what lets the unit test drive it directly.

**Files:**
- Create: `src/doors/syncmoo1/syncmoo1_audio.c`, `src/doors/syncmoo1/syncmoo1_audio.h`
- Modify: `src/doors/syncmoo1/tests/test_audio.c`, `src/doors/syncmoo1/tests/CMakeLists.txt`
- Modify: `src/doors/syncmoo1/CMakeLists.txt`

**Interfaces:**
- Consumes: `termgfx_fnv1a()` (Task 1); `termgfx_audio_sfx_store/_play_named/_stop_all/_tier` (Task 2).
- Produces (used by Task 4):
  - `void sm_audio_leaf(const void *data, size_t len, char out[16]);`
  - `int  sm_audio_vol(int moo_vol);`
  - `void sm_audio_attach(termgfx_audio_t *m);`
  - `int  sm_audio_batch_start(int max);`
  - `int  sm_audio_init(int index, const uint8_t *data, uint32_t len);`
  - `void sm_audio_play(int index);`
  - `void sm_audio_stop(void);`
  - `void sm_audio_release(int index);`
  - `void sm_audio_set_volume(int moo_vol);`
  - `void sm_audio_pump(void);`
  - `const char *sm_audio_slot(int index);` (test accessor)
  - `int sm_audio_pending(void);` (test accessor)

- [ ] **Step 1: Write the failing test.** Append to `src/doors/syncmoo1/tests/test_audio.c`, before `return 0;`:

```c
    /* --- syncmoo1_audio: leaf naming ------------------------------------ */
    {
        char leaf[16];
        sm_audio_leaf("foobar", 6, leaf);
        assert(strcmp(leaf, "s_bf9cf968") == 0);   /* "s_" + %08x of FNV-1a */
    }

    /* --- volume map: 1oom's 0..128 -> termgfx's 0..100 ------------------- */
    assert(sm_audio_vol(0)   == 0);
    assert(sm_audio_vol(128) == 100);
    assert(sm_audio_vol(64)  == 50);
    assert(sm_audio_vol(-5)  == 0);     /* clamped */
    assert(sm_audio_vol(999) == 100);   /* clamped */

    /* --- slot table + dedupe (no terminal attached: all Stores no-op) ---- */
    {
        static const char a[] = "sample-A";
        static const char b[] = "sample-B";
        char leaf_a[16], leaf_b[16];

        sm_audio_attach(NULL);                 /* no manager -> silent, still tracks */
        sm_audio_leaf(a, sizeof a - 1, leaf_a);
        sm_audio_leaf(b, sizeof b - 1, leaf_b);

        assert(sm_audio_batch_start(41) == 0);
        assert(sm_audio_init(0, (const uint8_t *)a, sizeof a - 1) == 0);
        assert(sm_audio_init(7, (const uint8_t *)b, sizeof b - 1) == 0);
        assert(strcmp(sm_audio_slot(0), leaf_a) == 0);
        assert(strcmp(sm_audio_slot(7), leaf_b) == 0);
        assert(sm_audio_pending() == 2);

        /* Identical bytes at another index: same leaf, queued ONCE. */
        assert(sm_audio_init(9, (const uint8_t *)a, sizeof a - 1) == 0);
        assert(strcmp(sm_audio_slot(9), leaf_a) == 0);
        assert(sm_audio_pending() == 2);

        /* batch_start is a CAPACITY HINT, not a reset: the intro calls it
           again after ui_late_init() registered the gameplay set. Clearing
           here would silence every gameplay sound. */
        assert(sm_audio_batch_start(44) == 0);
        assert(strcmp(sm_audio_slot(0), leaf_a) == 0);
        assert(sm_audio_pending() == 2);

        /* Out-of-range indices are no-ops, never writes. */
        assert(sm_audio_init(-1, (const uint8_t *)a, sizeof a - 1) == 0);
        assert(sm_audio_init(100000, (const uint8_t *)a, sizeof a - 1) == 0);
        assert(sm_audio_slot(-1) == NULL);
        assert(sm_audio_slot(100000) == NULL);

        /* release() clears the slot, not the client's cached sample. */
        sm_audio_release(7);
        assert(sm_audio_slot(7) == NULL);

        /* play()/stop() with no manager are no-ops (must not crash). */
        sm_audio_play(0);
        sm_audio_play(7);
        sm_audio_stop();
    }
```

Add `#include "syncmoo1_audio.h"` to the test's includes.

- [ ] **Step 2: Add the module's sources to the test target.** In `src/doors/syncmoo1/tests/CMakeLists.txt`, add to `add_executable(test_audio ...)`:

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../syncmoo1_audio.c
```

and add `${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx` to its `target_include_directories` (already present from Task 1).

- [ ] **Step 3: Run it to verify it fails.**

Run: `cd src/doors/syncmoo1 && ./build.sh 2>&1 | grep -E 'syncmoo1_audio.h|error' | head -3`
Expected: FAIL — `fatal error: syncmoo1_audio.h: No such file or directory`.

- [ ] **Step 4: Write the header.** Create `src/doors/syncmoo1/syncmoo1_audio.h`:

```c
/* syncmoo1_audio.h -- 1oom's hw_audio_sfx_* contract over termgfx's audio
 * manager. hw_sbbs.c forwards the engine's hooks here, the way hw_video_*
 * forwards to syncmoo1_io.c; nothing else in the door knows about audio.
 *
 * Design: docs/superpowers/specs/2026-07-08-syncmoo1-sfx-audio-design.md
 */
#ifndef SYNCMOO1_AUDIO_H
#define SYNCMOO1_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#include "audio_mgr.h"   /* termgfx: termgfx_audio_t */

/* Cache leaf for `len` bytes: "s_" + 8 lower-case hex of FNV-1a. `out` must be
 * >= 16 bytes. Content-addressed, so identical samples share one cache entry
 * and the name never depends on 1oom's index numbering. */
void sm_audio_leaf(const void *data, size_t len, char out[16]);

/* 1oom reports SFX volume as 0..128; termgfx wants 0..100. Clamped. */
int sm_audio_vol(int moo_vol);

/* Adopt the session's audio manager (may be NULL -- then everything below is a
 * silent no-op, which is exactly what a terminal with no audio APC gets). */
void sm_audio_attach(termgfx_audio_t *m);

/* The 1oom hw_audio_sfx_* contract. Indices are 1oom's global SFX ids. */
int  sm_audio_batch_start(int max);   /* capacity hint. NEVER clears slots. */
int  sm_audio_init(int index, const uint8_t *data, uint32_t len);
void sm_audio_play(int index);
void sm_audio_stop(void);
void sm_audio_release(int index);
void sm_audio_set_volume(int moo_vol);

/* Drain the pending-store queue once the terminal's audio tier is known.
 * Called from hw_event_handle(); cheap no-op once drained or below tier 1. */
void sm_audio_pump(void);

/* Test accessors: the leaf registered at `index` (NULL if none/out of range),
 * and how many distinct samples are still queued for Store. */
const char *sm_audio_slot(int index);
int         sm_audio_pending(void);

#endif /* SYNCMOO1_AUDIO_H */
```

- [ ] **Step 5: Write the implementation.** Create `src/doors/syncmoo1/syncmoo1_audio.c`:

```c
/* syncmoo1_audio.c -- see syncmoo1_audio.h.
 *
 * Two facts from the engine shape this module, both verified in 1oom's source
 * rather than assumed:
 *
 *   - hw_audio_sfx_batch_start(max) is a CAPACITY HINT, not a reset. SDL's
 *     parallel backend only sizes a table with it (hwsdl_audio.c:433) and its
 *     serial backend ignores it entirely. ui_late_init() registers the 41
 *     gameplay sounds BEFORE the intro registers its three, so a backend that
 *     cleared here would silence the game the moment the intro ran.
 *
 *   - The audio tier is UNKNOWN while the engine registers sounds.
 *     ui_late_init() calls batch_start right after hw_video_init(), before the
 *     first present and so before the capability probe is even sent. Every
 *     termgfx Store is dropped below tier 1. So samples are copied into a
 *     pending queue here and drained from sm_audio_pump() once the tier reply
 *     lands -- inside the engine's own "Preparing sounds" pause.
 *
 * We never call fmt_sfx_convert(): termgfx's Store path already transcodes VOC.
 * The raw LBX bytes go straight through.
 */
#include "syncmoo1_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"   /* termgfx: termgfx_fnv1a */

/* 1oom's global SFX id space: NUM_SOUNDS (0x29) gameplay sounds, plus the
 * intro's three at NUM_SOUNDS+0..2 (uiintro.c). Sized generously and
 * range-checked rather than assuming the intro keeps using exactly three. */
#define SM_SFX_MAX      256
#define SM_LEAF_LEN     16
#define SM_PENDING_MAX  64

struct sm_pending {
    char     leaf[SM_LEAF_LEN];
    uint8_t *data;
    size_t   len;
};

static termgfx_audio_t   *g_mgr;
static char               g_slot[SM_SFX_MAX][SM_LEAF_LEN];
static struct sm_pending  g_pending[SM_PENDING_MAX];
static int                g_pending_n;
static int                g_vol = 128;   /* 1oom's default full scale */

void sm_audio_leaf(const void *data, size_t len, char out[16])
{
    snprintf(out, SM_LEAF_LEN, "s_%08x", termgfx_fnv1a(data, len));
}

int sm_audio_vol(int moo_vol)
{
    if (moo_vol <= 0)
        return 0;
    if (moo_vol >= 128)
        return 100;
    return (moo_vol * 100) / 128;
}

void sm_audio_attach(termgfx_audio_t *m)
{
    g_mgr = m;
}

static int sm_audio_valid(int index)
{
    return index >= 0 && index < SM_SFX_MAX;
}

int sm_audio_batch_start(int max)
{
    (void)max;   /* capacity hint only -- see the file header. Never clears. */
    return 0;    /* 0 = "prepared synchronously": our work is a hash, not a decode */
}

/* Queue `len` bytes under `leaf` unless that leaf is already queued. Returns 0
 * whether or not it queued: a full queue or a failed copy costs that one sample
 * its sound, never the session. */
static int sm_audio_queue(const char *leaf, const uint8_t *data, size_t len)
{
    int i;

    for (i = 0; i < g_pending_n; ++i) {
        if (strcmp(g_pending[i].leaf, leaf) == 0)
            return 0;   /* dedupe: same bytes registered at another index */
    }
    if (g_pending_n >= SM_PENDING_MAX)
        return 0;

    g_pending[g_pending_n].data = (uint8_t *)malloc(len);
    if (g_pending[g_pending_n].data == NULL)
        return 0;
    memcpy(g_pending[g_pending_n].data, data, len);
    g_pending[g_pending_n].len = len;
    snprintf(g_pending[g_pending_n].leaf, SM_LEAF_LEN, "%s", leaf);
    ++g_pending_n;
    return 0;
}

int sm_audio_init(int index, const uint8_t *data, uint32_t len)
{
    char leaf[SM_LEAF_LEN];

    if (!sm_audio_valid(index) || data == NULL || len == 0)
        return 0;   /* the engine already logs a bad index (uisound.c) */

    sm_audio_leaf(data, len, leaf);
    snprintf(g_slot[index], SM_LEAF_LEN, "%s", leaf);

    /* Copy the bytes: 1oom frees the intro's items after use, and the whole
     * set is ~440 KB -- a tenth of one sixel frame's working memory. */
    return sm_audio_queue(leaf, data, len);
}

void sm_audio_pump(void)
{
    int i;

    if (g_pending_n == 0 || g_mgr == NULL || termgfx_audio_tier(g_mgr) < 1)
        return;

    for (i = 0; i < g_pending_n; ++i) {
        termgfx_audio_sfx_store(g_mgr, g_pending[i].leaf,
                                g_pending[i].data, g_pending[i].len);
        free(g_pending[i].data);
        g_pending[i].data = NULL;
    }
    g_pending_n = 0;
}

void sm_audio_play(int index)
{
    if (g_mgr == NULL || !sm_audio_valid(index) || g_slot[index][0] == '\0')
        return;
    termgfx_audio_sfx_play_named(g_mgr, g_slot[index], sm_audio_vol(g_vol), 0);
}

void sm_audio_stop(void)
{
    if (g_mgr != NULL)
        termgfx_audio_sfx_stop_all(g_mgr);
}

void sm_audio_release(int index)
{
    if (sm_audio_valid(index))
        g_slot[index][0] = '\0';   /* the client keeps the cached sample */
}

void sm_audio_set_volume(int moo_vol)
{
    g_vol = moo_vol;
}

const char *sm_audio_slot(int index)
{
    if (!sm_audio_valid(index) || g_slot[index][0] == '\0')
        return NULL;
    return g_slot[index];
}

int sm_audio_pending(void)
{
    return g_pending_n;
}
```

- [ ] **Step 6: Add the module to the door build.** In `src/doors/syncmoo1/CMakeLists.txt`, add `syncmoo1_audio.c` to `add_executable(syncmoo1 ...)`, after `syncmoo1_config.c`.

- [ ] **Step 7: Run the test to verify it passes.**

Run: `cd src/doors/syncmoo1 && ./build.sh >/dev/null && ./build/tests/test_audio && echo PASS`
Expected: `PASS`.

- [ ] **Step 8: Verify the batch_start guard actually asserts.** Temporarily make `sm_audio_batch_start()` clear the table (`memset(g_slot, 0, sizeof g_slot);`), rebuild, run.

Run: `./build/tests/test_audio; echo "exit=$?"`
Expected: nonzero, failing on `strcmp(sm_audio_slot(0), leaf_a) == 0` after the second `batch_start`. **Remove the memset and re-run to PASS before committing.**

- [ ] **Step 9: Commit.**

```bash
cd /home/rswindell/sbbs
git add src/doors/syncmoo1/syncmoo1_audio.c src/doors/syncmoo1/syncmoo1_audio.h
git commit -F - -- src/doors/syncmoo1/syncmoo1_audio.c src/doors/syncmoo1/syncmoo1_audio.h \
  src/doors/syncmoo1/CMakeLists.txt src/doors/syncmoo1/tests/test_audio.c \
  src/doors/syncmoo1/tests/CMakeLists.txt <<'EOF'
syncmoo1: add syncmoo1_audio -- content-addressed SFX over termgfx

The module 1oom's hw_audio_sfx_* hooks will forward to.  Samples are
content-addressed as s_<fnv1a8>, so identical bytes in INTROSND.LBX and
SOUNDFX.LBX share one client-cache entry and the name never depends on
1oom's index numbering.

Two engine facts shape it, both read from the source:

  - batch_start(max) is a capacity hint, not a reset.  ui_late_init()
    registers the 41 gameplay sounds before the intro registers its
    three, so clearing here would silence the game once the intro ran.
    There is a test for exactly that.

  - The audio tier is unknown while the engine registers sounds, so
    every Store would be dropped.  Samples are copied into a pending
    queue and drained by sm_audio_pump() once the tier is known.

No manager attached (a terminal with no audio APC) is a silent no-op
throughout, which is also what lets the unit test drive the module with
no socket.  Tests run with -UNDEBUG so the assert()s actually execute.
EOF
```

---

### Task 4: Wire it into the door, and prove it on the wire

**Files:**
- Modify: `src/doors/syncmoo1/hw_sbbs.c` (the `hw_audio_sfx_*` stubs, `hw_event_handle`)
- Modify: `src/doors/syncmoo1/syncmoo1_io.c` (create manager, cache prefix, probe)
- Modify: `src/doors/syncmoo1/syncmoo1_input.c` (feed inbound bytes)
- Modify: `src/doors/syncmoo1/syncmoo1.h` (declare `sm_io_audio()`)

**Interfaces:**
- Consumes: everything from Task 3; `termgfx_audio_create/_probe/_feed/_set_cache_prefix`.
- Produces: `termgfx_audio_t *sm_io_audio(void);`

- [ ] **Step 1: Create the manager in `syncmoo1_io.c`.** Add `#include "audio_mgr.h"` and `#include "syncmoo1_audio.h"` to its includes, then near the other module statics:

```c
/* One audio manager per session. Its emit callback stages bytes through the
 * same out-buffer as the sixel stream, so audio and video share one ordered
 * write path (mirrors syncduke_io.c's sd_audio_emit). */
static termgfx_audio_t *g_audio;

static void sm_audio_emit(void *ctx, const void *buf, size_t len, int stream)
{
    (void)ctx;
    (void)stream;
    sm_out_put(buf, len);
}

termgfx_audio_t *sm_io_audio(void)
{
    return g_audio;
}
```

- [ ] **Step 2: Probe at terminal enter.** In `sm_io_enter()`, immediately after `sm_out_puts(termgfx_term_enter);`:

```c
    /* Audio: create the manager and probe before any sample is registered.
     * The reply lands ~50 ms later; sm_audio_pump() drains the pending Stores
     * when it does. Cache prefix "moo1" -> client-cache names moo1/sfx/s_... */
    g_audio = termgfx_audio_create(sm_audio_emit, NULL);
    termgfx_audio_set_cache_prefix(g_audio, "moo1");
    sm_audio_attach(g_audio);
    termgfx_audio_probe(g_audio);
```

- [ ] **Step 3: Declare the accessor.** In `syncmoo1.h`, after `int sm_io_get_fd(void);`:

```c
/* The session's termgfx audio manager (NULL before sm_io_enter()). syncmoo1_
 * input.c feeds it inbound bytes so it can resolve the capability probe. */
termgfx_audio_t *sm_io_audio(void);
```

and add `#include "audio_mgr.h"` to `syncmoo1.h`'s includes.

- [ ] **Step 4: Feed inbound bytes.** In `syncmoo1_input.c`'s `sm_input_pump()`, right after the existing `sm_io_wiredump_in(buf, (size_t)n);` line:

```c
        {   /* Resolve the audio capability probe (SyncTERM replies with an
             * APC the manager parses); harmless for every other byte. */
            termgfx_audio_t *am = sm_io_audio();
            if (am != NULL)
                termgfx_audio_feed(am, buf, (int)n);
        }
```

Add `#include "audio_mgr.h"` to its includes.

- [ ] **Step 5: Forward the engine's hooks.** In `hw_sbbs.c`, add `#include "syncmoo1_audio.h"`, then replace the five SFX stubs:

```c
int hw_audio_sfx_batch_start(int sfx_index_max)
{
    return sm_audio_batch_start(sfx_index_max);
}

int hw_audio_sfx_batch_end(void)
{
    return 0;
}

int hw_audio_sfx_init(int sfx_index, const uint8_t *data, uint32_t len)
{
    return sm_audio_init(sfx_index, data, len);
}

void hw_audio_sfx_release(int sfx_index)
{
    sm_audio_release(sfx_index);
}

void hw_audio_sfx_play(int sfx_index)
{
    sm_audio_play(sfx_index);
}

void hw_audio_sfx_stop(void)
{
    sm_audio_stop();
}

bool hw_audio_sfx_volume(int volume)
{
    sm_audio_set_volume(volume);
    return true;
}
```

- [ ] **Step 6: Drain the queue from the event pump.** In `hw_sbbs.c`'s `hw_event_handle()`, before the `sm_input_pump()` call:

```c
    sm_audio_pump();   /* Stores the queued samples once the tier reply lands */
```

- [ ] **Step 7: Build and run the unit tests.**

Run: `cd src/doors/syncmoo1 && ./build.sh 2>&1 | grep -E 'error|Built:' | tail -1 && ./build/tests/test_map && ./build/tests/test_audio && echo "unit PASS"`
Expected: `Built:` then `unit PASS`.

- [ ] **Step 8: Prove it on the wire — SyncTERM-like terminal.** The harness lives in the session scratchpad; if absent, drive the door over a socketpair the same way. Capture with the door's own wire dump.

```bash
cd src/doors/syncmoo1
rm -rf /tmp/moo_audio && mkdir -p /tmp/moo_audio/data
SBBSDATA=/tmp/moo_audio/data SBBSNNUM=3 \
  python3 <scratchpad>/harness2.py /tmp/moo_audio 9 syncterm '\x1b[4;400;640t' '\x1b[25;80R' >/dev/null 2>&1
python3 tools/wiredump.py /tmp/moo_audio/data/syncmoo1/syncmoo1_n3.wire | head -20
```

Then assert on the capture with a throwaway script:

```bash
python3 - /tmp/moo_audio/data/syncmoo1/syncmoo1_n3.wire <<'EOF'
import sys, re
d = open(sys.argv[1], 'rb').read()
stores = re.findall(rb'SyncTERM:C;S;([^;\x1b]+)', d)
loads  = re.findall(rb'SyncTERM:A;Load;S=\d+;([^\x1b]+)', d)
print("Store APCs:", len(stores), "distinct:", len(set(stores)), "| Load APCs:", len(loads))
for x in sorted(set(stores))[:5]: print("  ", x.decode())
assert len(stores) == len(set(stores)), "a sample was Stored twice"
assert all(x.startswith(b'moo1/sfx/s_') for x in stores), "bad Store name"
assert all(x.startswith(b'moo1/sfx/s_') for x in loads), "bad Load name"
assert set(loads) <= set(stores), "played a leaf that was never Stored"
EOF
```

The exact APC strings, from `termgfx/audio.c`: Store is
`ESC _ SyncTERM:C;S;<file>;` (audio.c:111); play/Load is
`ESC _ SyncTERM:A;Load;S=<slot>;<file> ST` (audio.c:496). `C;L` is the
cache-*list* query, NOT a play -- do not grep for it here.

Expected: every Store and Load name matches `moo1/sfx/s_<8 hex>`, no name is
Stored twice, and nothing is Loaded that was never Stored.

- [ ] **Step 9: Prove silence on a terminal with no audio APC.** Run the same capture in the harness's `foot` mode (answers DA1 only, never the audio-capability reply).

Note what is NOT zero: `termgfx_audio_probe()` emits its query
(`ESC _ SyncTERM:Q;libsndfile ST`, audio.c:79) unconditionally, so that one APC
reaches every terminal. It is inert on a terminal that ignores APCs.

```bash
python3 - /tmp/moo_foot/data/syncmoo1/syncmoo1_n3.wire <<'EOF'
import sys, re
d = open(sys.argv[1], 'rb').read()
assert len(re.findall(rb'SyncTERM:C;S;', d))       == 0, "Stored with no audio tier"
assert len(re.findall(rb'SyncTERM:A;Load', d))     == 0, "Loaded with no audio tier"
assert len(re.findall(rb'SyncTERM:A;Queue', d))    == 0, "Queued with no audio tier"
assert len(re.findall(rb'SyncTERM:Q;libsndfile', d)) == 1, "probe sent exactly once"
EOF
```

Expected: no Store/Load/Queue, exactly one probe. The tier never reaches 1, so
`sm_audio_pump()` never drains and `sm_audio_play()` no-ops.

- [ ] **Step 10: Verify no Store precedes the tier reply.**

The capability reply is a CSI, not an APC: SyncTERM answers the `Q;libsndfile`
query with `ESC[=7;100;{0,1}n` (see `syncduke_input.c:1505`'s comment, and
`termgfx_audio_parse_caps`).

Run: over the wire dump's `<ms> <I|O> <len>` records, assert the timestamp of the
first outbound `SyncTERM:C;S;` is greater than that of the inbound
`ESC[=7;100;` reply.
Expected: true. (A Store before the reply is dropped by termgfx's `tier < 1`
guard, so this catches a regression where the queue drains too early.)

- [ ] **Step 11: Listen to it.** Launch the door from the BBS in SyncTERM and confirm the intro stings and UI blips are audible, and that a second launch is audibly identical (the client now serves the samples from its own on-disk cache).

- [ ] **Step 12: Commit.**

```bash
cd /home/rswindell/sbbs
git commit -F - -- src/doors/syncmoo1/hw_sbbs.c src/doors/syncmoo1/syncmoo1_io.c \
  src/doors/syncmoo1/syncmoo1_input.c src/doors/syncmoo1/syncmoo1.h <<'EOF'
syncmoo1: wire SFX audio into the 1oom hw backend

hw_audio_sfx_* forward to syncmoo1_audio, the way hw_video_* forward to
syncmoo1_io.  The audio manager is created and probed at terminal enter,
inbound bytes are fed to it from the input pump so it can resolve the
capability reply, and hw_event_handle() drains the pending-store queue
once the tier is known -- inside the engine's own "Preparing sounds"
pause.

Samples are Stored under moo1/sfx/s_<fnv1a8>, once each per session.
A terminal without the audio APC (foot, xterm) never reaches tier 1, so
nothing is uploaded and nothing plays: silent, not broken.

Verified on the wire: every Store name is moo1/sfx/s_<hex>, no sample is
Stored twice, no Store precedes the capability reply, and a DA1-only
terminal receives zero audio bytes.  Confirmed audible in SyncTERM.
EOF
```

---

## Self-Review

**Spec coverage.** termgfx additions (spec "termgfx additions") → Tasks 1–2. Door module + state (spec "Door module") → Task 3. Data flow steps 1–8 (spec "Data flow") → Task 3 (steps 1,2,3,5,6,7,8) and Task 4 step 6 (step 4, the drain). Wiring (spec "Wiring") → Task 4 steps 1–4. Error handling (spec "Error handling") → Task 3's NULL-manager and range-check tests, Task 4 step 9. Testing (spec "Testing") → Task 1 step 1, Task 3 step 1, Task 4 steps 8–10. Non-goals hold: no music hook is touched, `fmt_sfx_convert()` is never called, the `C;L` glob is unchanged, and neither other door's source is modified.

**Placeholder scan.** No TBD/TODO. Every code step carries its code. The one non-literal is `<scratchpad>/harness2.py` in Task 4 step 8, which is a session-local path, with the fallback ("drive the door over a socketpair the same way") stated inline.

**Type consistency.** `sm_audio_leaf(const void*, size_t, char[16])`, `sm_audio_vol(int)->int`, `sm_audio_batch_start(int)->int`, `sm_audio_init(int, const uint8_t*, uint32_t)->int`, `sm_audio_slot(int)->const char*`, `sm_audio_pending(void)->int` are used identically in the header (Task 3 step 4), the implementation (step 5), the test (step 1) and the forwarders (Task 4 step 5). `hw_audio_sfx_init`'s `uint32_t len` matches `1oom/src/hw.h:67`. `termgfx_audio_sfx_store/_play_named` signatures match between Task 2's header, Task 2's body, and Task 3's calls. `SM_LEAF_LEN` (16) matches the `char out[16]` in the public signature and termgfx's `NAMEDLEN` (16).
