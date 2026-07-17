# SyncSCUMM M4 (Audio) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Play ScummVM's speech, sound effects, and music on SyncTERM by
streaming the mixer's PCM output over the door connection, leaving
non-audio terminals (Foot, xterm) exactly as they are today — silent,
subtitles auto-on.

**Architecture:** Three layers, landing in dependency order. (1) Extract
syncretro's proven PCM-streaming state machine out of the door and into
libtermgfx as a shared, stereo-capable module — a refactor guarded by
syncretro's existing tests, with syncretro reduced to a thin adapter.
(2) Wire SyncSCUMM's session layer (`sst_io.c`) to probe for audio caps,
parse the replies, expose a backlog, and feed the shared module. (3)
Replace ScummVM's `NullMixerManager` with a wall-clock-driven manager
that pulls mixed PCM and hands it to the session layer.

**Tech Stack:** C11 (termgfx, sst_io), C++ (ScummVM backend), CMake
(termgfx/syncretro), ScummVM's generated `config.mk` (syncscumm),
libsndfile (Ogg/Opus encode), SyncTERM audio APCs.

**Source of truth:** `docs/superpowers/specs/2026-07-16-syncscumm-m4-audio-design.md`,
including its **"Amendment 2026-07-16: transport is a shared termgfx
module"** section, which supersedes every mention of
`termgfx_audio_stream_chunk()` in the body of that spec.

## Global Constraints

- **Style is per-directory and they conflict — do not cross-contaminate:**
  - `src/doors/termgfx/*.{c,h}` — uncrustify-managed. Write to
    `src/uncrustify.cfg` from the start (tabs, 4-wide) and run
    `uncrustify -c ../../uncrustify.cfg --replace --no-backup <file>` as
    the closing step. Comments are `//`, including block banners.
    Header guard `TERMGFX_<NAME>_H_`. Public names `termgfx_<module>_<verb>`.
    Function definitions put the return type on its own line.
  - `src/doors/syncscumm/door/*.{c,cpp,h}` — **HAND-FORMATTED, NOT
    uncrustify-managed. Never run uncrustify here** (it churns 40+
    preexisting lines). Tabs; comments are `/* ... */` exclusively;
    file-statics `g_*`; exported `sst_io_*`; declarations at block top,
    column-aligned; `NULL` compared explicitly.
  - `src/doors/syncretro/*.{c,h}` — hand-formatted, `/* ... */`.
- **C11.** Pure C in `sst_io.c` (it compiles as `%.c` inside ScummVM's
  C++ tree): no `//`, no mid-block declarations.
- **Portability:** builds must stay clean under GCC/Clang. Never
  introduce reserved-namespace identifiers (leading `_` + uppercase, or
  any `__`).
- **Comment density is the house trait** in these files: explain *why*,
  cite the ported-from source as `file:line`, and narrate the live defect
  that motivated a branch. Match it.
- **Git:** commit directly on `master` in `~/sbbs` (no branching). The
  index is shared with the user — run `git status` repo-wide and check
  `git diff --cached` before every commit; **never `git commit --amend`**
  and never `reset --hard` / `checkout .` / `stash` in this tree.
  **Never `git push`** — the user pushes.
- **Commit messages wrap at 78 chars.** Verify with
  `awk 'length > 78 { print FILENAME ": " length($0) " chars: " $0 }' <msgfile>`
  (must print nothing), then `git commit -F <msgfile>`.
- **Never cite an unpushed commit by SHA** in a commit message or any
  durable text. Refer to prior work by what it did.
- **Validate before committing.** A commit requires a test run that
  actually exercised the change. Static reasoning is not validation.
- **Do not `make install`** or deploy binaries into the live `/sbbs`.
  SyncSCUMM's live binary is already a symlink to the build output, so
  `./build.sh` is live immediately — never claim "not deployed".
- **A live door session pins the cached binary** over the `/sbbs` CIFS
  mount: new launches run the OLD bytes until that session exits. Exit
  the door before concluding a fix failed.

### Load-bearing facts (verified against the tree during planning)

- `Audio::MixerImpl::mixCallback(byte *samples, uint len)` — **`len` is a
  BYTE count** (`scummvm/audio/mixer.cpp:331-350`: `memset(buf, 0, len)`
  then `len >>= 2` for stereo, with `assert(len % 4 == 0)`). The vendored
  `NullMixerManager::update()` passes a frame count — a latent upstream
  quirk. Our manager must pass `frames * 4`.
- `termgfx_audio_encode_ogg(const int16_t *pcm, size_t frames, int channels, int rate, double quality, uint8_t **out)`
  (`termgfx/audio.h:82-86`) **already takes `channels`** and already picks
  the Opus rate and resamples internally (`audio.c:379`). Syncretro passes
  a literal `1`.
- `termgfx_audio_pick_opus_rate()` is **`static` in `audio.c`, not public**
  (contra the design spec's wording). Do not try to call it; `encode_ogg`
  handles rate internally, so passing 22050 is correct.
- Audio caps reply: `ESC [ = 7 ; 100 ; {0,1} n`, parsed by
  `termgfx_audio_parse_caps(acc, len)` → 1 digital / 0 tone-only / -1 no
  reply yet. Idempotent over a growing buffer.
- Opus reply: `ESC [ = 7 ; 101 ; 32 ; 100 ; {0,1} n`, parsed by
  `termgfx_audio_parse_opus()`. **-1 means no reply — assume yes** (older
  SyncTERM never answers).
- Idle/underrun notification: `CSI = 7 ; <ch> ; 0 n`. It shares the `=7`
  prefix with the caps reply, so **feature id 100 must be excluded** when
  matching the channel form (`syncretro_input.c:889` uses `p[1] != 100`).
- **SyncTERM clamps a channel volume of 0 to -60 dB and bakes it into the
  channel's entry volume** — an as-designed behavior that cost SyncDuke a
  debugging session (`syncretro/M4_AUDIO.md:290-293`). Never spell a mute
  as `volume = 0`; stop sending instead.
- SyncTERM's channel mixer soft-clips (tanh knee); an already-mixed hot
  stream at 100% distorts. Proven fix: run the channel at ~70 (~ -3 dB).
- `A;Flush` **with** a fade (`O=`) only crossfades the HEAD buffer and
  leaves the rest of the FIFO playing. To actually stop, flush with **no
  fade** (`syncretro_audio.c`'s `sr_audio_stop_channel()` comment).
- **Backlog cap — a spec-vs-code unit mismatch, resolved in favor of the
  code.** The design says "if unshipped audio exceeds ~500 ms ... drop
  whole chunks". The module being extracted expresses the same policy in
  bytes and strikes: `48KB` of staged output on **two consecutive chunk
  boundaries** (~200 ms of real congestion, not a momentary burst). Keep
  the module's formulation. It measures the thing that actually matters —
  the door's staged-output depth, which is shared with the video path —
  whereas a millisecond figure would have to be inferred from it anyway.
  An instantaneous socket check is specifically wrong here: SyncDOOM tried
  that for SFX and reverted it, because the frame path keeps the socket
  busy essentially always.

---

## File Structure

**termgfx (new shared module — uncrustify style, `//` comments):**

- Create `src/doors/termgfx/audio_stream.h` — public API of the shared
  PCM-streaming module: the chunk accumulator + the stream state machine.
- Create `src/doors/termgfx/audio_stream.c` — implementation, extracted
  from `syncretro/syncretro_chunk.c` + `syncretro/syncretro_audio.c`.
- Modify `src/doors/termgfx/CMakeLists.txt:14-31` — add `audio_stream.c`
  to the `add_library` list (headers are not enumerated).

Naming note: `audio_mgr.h` already exports `termgfx_audio_stream_chunk()`
and `termgfx_audio_stream_stop()` (a different, non-underrun-aware FMV
path used by syncconquer). To avoid collision **the new module's prefix is
`termgfx_stream_`, not `termgfx_audio_stream_`.** The old functions stay
put; syncconquer is untouched by this plan.

**syncretro (becomes a thin adapter):**

- Delete `src/doors/syncretro/syncretro_chunk.{c,h}` (moves to termgfx).
- Rewrite `src/doors/syncretro/syncretro_audio.c` as an adapter over
  `termgfx_stream_*`; `syncretro_audio.h` keeps its existing `sr_audio_*`
  surface so no call site changes.
- Modify `src/doors/syncretro/CMakeLists.txt` — drop `syncretro_chunk.c`
  from the target; repoint the `test_chunk` test.
- Move `src/doors/syncretro/test_chunk.c` →
  `src/doors/termgfx/test/test_audio_stream.c` (see Task 2).

**syncscumm:**

- Modify `src/doors/syncscumm/door/CMakeLists.txt` — probe libsndfile,
  write `sndfile_libs.txt` (mirrors the existing `jxl_libs.txt` handoff).
- Modify `src/doors/syncscumm/build.sh` — consume `sndfile_libs.txt` into
  `config.mk`'s `LIBS`.
- Modify `src/doors/syncscumm/door/sst_io.{c,h}` — backlog query, audio
  probes, reply parsing, settle wait, stream feed, trace counters.
- Create `src/doors/syncscumm/door/audio_term.{h,cpp}` —
  `SyncscummMixerManager`.
- Modify `src/doors/syncscumm/door/syncscumm.cpp` — swap the mixer
  manager, tick it from `pollEvent()`.
- Modify `src/doors/syncscumm/door/video_term.cpp` — tick from present.
- Modify `src/doors/syncscumm/door/module.mk` — add `audio_term.o`.
- Create `src/doors/syncscumm/test/test_sst_io_audio.c` + append to
  `src/doors/syncscumm/test/unit_sst_io.sh`.
- Modify `src/doors/syncscumm/syncscumm.example.ini` — document the
  `[audio]` keys.

---

## Task 1: Give the chunk accumulator a channel count (in place)

Generalize syncretro's mono-only accumulator to N channels **before**
moving it, so its existing test guards the change. Syncretro keeps passing
1 and must stay bit-identical in behavior.

**Files:**
- Modify: `src/doors/syncretro/syncretro_chunk.h`
- Modify: `src/doors/syncretro/syncretro_chunk.c`
- Modify: `src/doors/syncretro/syncretro_audio.c` (call sites)
- Test: `src/doors/syncretro/test_chunk.c`

**Interfaces:**
- Produces: `sr_chunk_t` gains a `channels` field;
  `int sr_chunk_init(sr_chunk_t *c, size_t frames, int channels)`.
  All other signatures unchanged.

- [ ] **Step 1: Write the failing test**

Append to `src/doors/syncretro/test_chunk.c`, immediately before the
final `printf`/`return` (which the map places at `:125-126`). Note the
existing `stereo()` helper builds L == R; these cases need L != R, so
they build their own buffer.

```c
	/* --- stereo: both channels are kept, interleaved ------------------- */
	{
		sr_chunk_t c;
		int16_t    batch[4 * 2];
		size_t     used;
		int        i;

		/* Frame i = (100+i, 200+i): L and R differ, so a downmix that
		 * dropped the right channel would be caught. */
		for (i = 0; i < 4; i++) {
			batch[2 * i]     = (int16_t)(100 + i);
			batch[2 * i + 1] = (int16_t)(200 + i);
		}
		CHECK(sr_chunk_init(&c, 4, 2) == 1);
		used = sr_chunk_append(&c, batch, 4);
		CHECK(used == 4);              /* per-CHANNEL frames, not samples */
		CHECK(sr_chunk_full(&c) == 1);
		CHECK(c.buf[0] == 100 && c.buf[1] == 200);   /* frame 0: L, R */
		CHECK(c.buf[6] == 103 && c.buf[7] == 203);   /* frame 3: L, R */
		sr_chunk_free(&c);
	}

	/* A stereo chunk is silent only when BOTH channels are zero: a
	 * right-channel-only signal must defeat the silence path, which the
	 * mono scan (iterating len, not len*channels) would have missed. */
	{
		sr_chunk_t c;
		int16_t    batch[4 * 2];
		size_t     i;

		for (i = 0; i < 4 * 2; i++)
			batch[i] = 0;
		CHECK(sr_chunk_init(&c, 4, 2) == 1);
		sr_chunk_append(&c, batch, 4);
		CHECK(sr_chunk_is_silent(&c) == 1);
		sr_chunk_reset(&c);
		batch[7] = -1;                 /* last frame, RIGHT channel only */
		sr_chunk_append(&c, batch, 4);
		CHECK(sr_chunk_is_silent(&c) == 0);
		sr_chunk_free(&c);
	}

	/* Mono is unchanged: the left sample of each frame, as before. */
	{
		sr_chunk_t c;
		int16_t   *batch = stereo(4, 1234);

		CHECK(sr_chunk_init(&c, 4, 1) == 1);
		sr_chunk_append(&c, batch, 4);
		CHECK(c.buf[0] == 1234 && c.buf[3] == 1234);
		sr_chunk_free(&c);
		free(batch);
	}
```

Every existing `sr_chunk_init(&c, N)` call in this file must gain `, 1`.

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake -S . -B build -DSYNCRETRO_TESTS=ON && cmake --build build --target test_chunk
```
Expected: FAIL to compile — `too many arguments to function 'sr_chunk_init'`.

- [ ] **Step 3: Write the implementation**

`syncretro_chunk.h` — replace the accumulator block (`:13-22`):

```c
/* --- the chunk accumulator (test_chunk.c covers it) ------------------------
 * Holds `channels` interleaved samples per frame. Mono (channels == 1) keeps
 * the LEFT sample of each input frame: a libretro core like FreeIntv emits
 * Audio(c, c), so L == R and the downmix is lossless. A source with genuine
 * stereo content (ScummVM's mixer) must pass channels == 2 -- see
 * termgfx/audio_stream.h, where this accumulator now lives. */
typedef struct {
	int16_t *buf;   /* cap * channels interleaved samples */
	size_t cap;     /* FRAMES per chunk (not samples: see channels) */
	size_t len;     /* frames accumulated so far */
	int channels;   /* 1 = downmix to left, 2 = keep both */
} sr_chunk_t;

/* 1 = ok, 0 = OOM. `frames` is per-channel; `channels` is 1 or 2. */
int    sr_chunk_init(sr_chunk_t *c, size_t frames, int channels);
```

`syncretro_chunk.c` — the four mono assumptions:

```c
int sr_chunk_init(sr_chunk_t *c, size_t frames, int channels)
{
	if (channels < 1)
		channels = 1;
	c->channels = channels;
	c->len      = 0;
	/* Overflow guard: frames * channels must not wrap (test_chunk feeds
	 * SIZE_MAX to assert the failed-init contract). */
	if (frames > SIZE_MAX / ((size_t)channels * sizeof *c->buf)) {
		c->buf = NULL;
		c->cap = 0;
		return 0;
	}
	c->buf = malloc(frames * (size_t)channels * sizeof *c->buf);
	if (c->buf == NULL) {
		c->cap = 0;   /* every other entry point then no-ops instead of
		               * writing through a NULL buf */
		return 0;
	}
	c->cap = frames;
	return 1;
}

size_t sr_chunk_append(sr_chunk_t *c, const int16_t *pcm, size_t frames)
{
	size_t room = c->cap - c->len;
	size_t i;

	if (frames > room)
		frames = room;            /* stop at the boundary; caller re-appends */
	if (c->channels == 1) {
		for (i = 0; i < frames; i++)
			c->buf[c->len + i] = pcm[2 * i];   /* LEFT: L == R by contract */
	} else {
		/* Input is always interleaved stereo (both callers' contract), so
		 * this is a straight copy, not a conversion. */
		memcpy(c->buf + c->len * 2, pcm, frames * 2 * sizeof *c->buf);
	}
	c->len += frames;
	return frames;
}

int sr_chunk_is_silent(const sr_chunk_t *c)
{
	size_t i;

	if (!sr_chunk_full(c))
		return 0;
	for (i = 0; i < c->len * (size_t)c->channels; i++)
		if (c->buf[i] != 0)
			return 0;
	return 1;
}
```

`sr_chunk_free`/`sr_chunk_full`/`sr_chunk_reset` are unchanged, except
`sr_chunk_free` also clears `c->channels = 0`. Add `#include <stdint.h>`
for `SIZE_MAX` if not already present (it comes via the header).

`syncretro_audio.c` — three call sites:
- `:189` area: `sr_chunk_init(&g_chunk, frames)` → `sr_chunk_init(&g_chunk, frames, 1)`
- `:133` `sr_audio_encode()`: leave the literal `1` for now (it is the
  encoder's channel count and still correct for syncretro).
- `:145-146` silence fill: `for (i = 0; i < g_chunk.cap; i++)` →
  `for (i = 0; i < g_chunk.cap * (size_t)g_chunk.channels; i++)`.

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake --build build --target test_chunk && ctest --test-dir build -R chunk --output-on-failure
```
Expected: PASS, `ok: 0 failure(s)`.

- [ ] **Step 5: Build syncretro whole, to prove no call site was missed**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro && ./build.sh 2>&1 | tail -5
```
Expected: links clean, no warnings about `sr_chunk_init`.

- [ ] **Step 6: Commit**

```bash
cd /home/rswindell/sbbs && git status --porcelain src/doors/syncretro
git add src/doors/syncretro/syncretro_chunk.h src/doors/syncretro/syncretro_chunk.c \
        src/doors/syncretro/syncretro_audio.c src/doors/syncretro/test_chunk.c
```
Message (write to a file, run the 78-col awk check, `git commit -F`):
```
syncretro: teach the audio chunk accumulator a channel count

The accumulator hardcoded a mono downmix: it allocated one sample per
frame, kept only the left sample of each input frame, and scanned for
silence over frames rather than samples. That is lossless for libretro
cores, which emit Audio(c, c) -- but the accumulator is about to be
shared with a source that mixes genuine stereo, so the assumption has to
become a parameter.

sr_chunk_init() now takes a channel count. Mono keeps the existing
left-sample downmix, bit for bit; stereo copies both channels. The
silence scan covers every sample, so a right-channel-only signal no
longer reads as silent. Syncretro passes 1 and is unchanged.

test_chunk covers stereo interleaving, right-channel-only silence, and
the unchanged mono path.
```

---

## Task 2: Move the accumulator into termgfx

Move (not copy) the accumulator to the shared library, restyled to
termgfx conventions, with its test. Syncretro keeps building.

**Files:**
- Create: `src/doors/termgfx/audio_stream.h`
- Create: `src/doors/termgfx/audio_stream.c`
- Create: `src/doors/termgfx/test/CMakeLists.txt`
- Create: `src/doors/termgfx/test/test_audio_stream.c` (from `syncretro/test_chunk.c`)
- Delete: `src/doors/syncretro/syncretro_chunk.{c,h}`, `src/doors/syncretro/test_chunk.c`
- Modify: `src/doors/termgfx/CMakeLists.txt`, `src/doors/syncretro/CMakeLists.txt`
- Modify: `src/doors/syncretro/syncretro_audio.c` (include + type rename)

**Interfaces:**
- Produces (consumed by Tasks 3, 8):
```c
typedef struct {
	int16_t *buf;
	size_t   cap;
	size_t   len;
	int      channels;
} termgfx_chunk_t;

int    termgfx_chunk_init(termgfx_chunk_t *c, size_t frames, int channels);
void   termgfx_chunk_free(termgfx_chunk_t *c);
size_t termgfx_chunk_append(termgfx_chunk_t *c, const int16_t *pcm, size_t frames);
int    termgfx_chunk_full(const termgfx_chunk_t *c);
int    termgfx_chunk_is_silent(const termgfx_chunk_t *c);
void   termgfx_chunk_reset(termgfx_chunk_t *c);
```

- [ ] **Step 1: Create the header**

`src/doors/termgfx/audio_stream.h` — note `//` comments, `TERMGFX_AUDIO_STREAM_H_`
guard, and the banner-plus-rationale form the other termgfx headers use.
The stream state machine (Task 3) is appended to this same header.

```c
// audio_stream.h -- a continuous PCM stream -> SyncTERM's audio APC FIFO.
//
// Some doors have discrete sounds to play (a gunshot, a music track) and want
// audio_mgr.h. Others have one continuous mixed PCM stream and no useful
// per-sound boundary at all: a libretro core's output, ScummVM's mixer. This
// module is for those -- it cuts the stream into fixed-size chunks, Opus-encodes
// each, and feeds SyncTERM's channel FIFO (A;Queue appends, so consecutive
// chunks play back to back).
//
// WHY A CUSHION. Chunks are produced in real time and consumed in real time, so
// the FIFO's depth is whatever was established at the start and never grows on
// its own. A FIFO fed one chunk at a time from empty stays one deep and
// underruns on the first jitter. That is why playback waits for `prebuffer`
// chunks: those chunks ARE the latency budget and the entire jitter tolerance.
// On an underrun we re-prime rather than resume, for the same reason.
//
// This layer is I/O-free in the same sense as caps.h: the door supplies a put/
// flush/backlog vtable and feeds it the parsed capability replies. It holds
// state (unlike audio.h) but touches no socket.
#ifndef TERMGFX_AUDIO_STREAM_H_
#define TERMGFX_AUDIO_STREAM_H_

#include <stddef.h>
#include <stdint.h>

// ---- chunk accumulator ---------------------------------------------------

// Holds `channels` interleaved samples per frame. Mono (channels == 1) keeps the
// LEFT sample of each input frame: a libretro core like FreeIntv emits
// Audio(c, c), so L == R and the downmix is lossless and halves everything. A
// source with genuine stereo content (ScummVM's mixer) passes channels == 2.
// Input is ALWAYS interleaved stereo either way -- `channels` describes what we
// keep, not what we are handed.
typedef struct {
	int16_t *buf;        // cap * channels interleaved samples
	size_t   cap;        // FRAMES per chunk (not samples: see channels)
	size_t   len;        // frames accumulated so far
	int      channels;   // 1 = downmix to left, 2 = keep both
} termgfx_chunk_t;

// 1 = ok, 0 = OOM (leaves a safe struct whose every entry point no-ops).
int  termgfx_chunk_init(termgfx_chunk_t *c, size_t frames, int channels);
void termgfx_chunk_free(termgfx_chunk_t *c);

// Append from interleaved stereo; returns the per-channel frames CONSUMED,
// which stops at the chunk boundary. The caller resets and re-appends the
// remainder.
size_t termgfx_chunk_append(termgfx_chunk_t *c, const int16_t *pcm, size_t frames);
int    termgfx_chunk_full(const termgfx_chunk_t *c);

// 1 only when the chunk is FULL and every sample is zero. An empty or partial
// chunk is never "silent" -- silence is a property the encoder acts on.
int    termgfx_chunk_is_silent(const termgfx_chunk_t *c);
void   termgfx_chunk_reset(termgfx_chunk_t *c);

#endif // TERMGFX_AUDIO_STREAM_H_
```

- [ ] **Step 2: Create the implementation**

`src/doors/termgfx/audio_stream.c` — the Task 1 bodies, renamed
`sr_chunk_*` → `termgfx_chunk_*`, restyled (return type on its own line,
`//` comments).

```c
// audio_stream.c -- see audio_stream.h.
#include "audio_stream.h"

#include <stdlib.h>
#include <string.h>

// ---- chunk accumulator ---------------------------------------------------

int
termgfx_chunk_init(termgfx_chunk_t *c, size_t frames, int channels)
{
	if (channels < 1)
		channels = 1;
	c->channels = channels;
	c->len      = 0;
	// Overflow guard: frames * channels * sizeof must not wrap.
	if (frames > SIZE_MAX / ((size_t)channels * sizeof *c->buf)) {
		c->buf = NULL;
		c->cap = 0;
		return 0;
	}
	c->buf = malloc(frames * (size_t)channels * sizeof *c->buf);
	if (c->buf == NULL) {
		c->cap = 0;   // every other entry point then no-ops instead of
		              // writing through a NULL buf
		return 0;
	}
	c->cap = frames;
	return 1;
}

void
termgfx_chunk_free(termgfx_chunk_t *c)
{
	free(c->buf);
	c->buf      = NULL;
	c->cap      = c->len = 0;
	c->channels = 0;
}

size_t
termgfx_chunk_append(termgfx_chunk_t *c, const int16_t *pcm, size_t frames)
{
	size_t room = c->cap - c->len;
	size_t i;

	if (frames > room)
		frames = room;   // stop at the boundary; caller re-appends
	if (c->channels == 1) {
		for (i = 0; i < frames; i++)
			c->buf[c->len + i] = pcm[2 * i];   // LEFT: L == R by contract
	} else {
		memcpy(c->buf + c->len * 2, pcm, frames * 2 * sizeof *c->buf);
	}
	c->len += frames;
	return frames;
}

int
termgfx_chunk_full(const termgfx_chunk_t *c)
{
	return c->len >= c->cap;
}

int
termgfx_chunk_is_silent(const termgfx_chunk_t *c)
{
	size_t i;

	if (!termgfx_chunk_full(c))
		return 0;
	for (i = 0; i < c->len * (size_t)c->channels; i++)
		if (c->buf[i] != 0)
			return 0;
	return 1;
}

void
termgfx_chunk_reset(termgfx_chunk_t *c)
{
	c->len = 0;
}
```

Add `#include <stdint.h>` to `audio_stream.h` (already listed) — `SIZE_MAX`
comes from `<stdint.h>` via that header.

- [ ] **Step 3: Add to the library — one line**

`src/doors/termgfx/CMakeLists.txt`, in the `add_library(termgfx STATIC ...)`
list (`:14-31`), next to the other audio modules:

```cmake
    audio.c
    audio_mgr.c
    audio_midi.c
    audio_stream.c
```

- [ ] **Step 4: Move the test**

`git mv src/doors/syncretro/test_chunk.c src/doors/termgfx/test/test_audio_stream.c`,
then rename `sr_chunk_*` → `termgfx_chunk_*`, `sr_chunk_t` → `termgfx_chunk_t`,
and `#include "syncretro_chunk.h"` → `#include "audio_stream.h"`. Update the
banner:

```c
/* test_audio_stream.c -- the chunk accumulator: the part of the streaming
 * module with edge cases and no I/O. Everything downstream (encode, upload,
 * queue) is exercised by a door's fake terminal, not here. */
```

termgfx has no tests of its own today, so this creates the directory. Per
the house convention (`syncmoo1/tests/CMakeLists.txt:8-12`) the test
compiles the termgfx `.c` as a **source**, never linking the library —
that keeps it dependency-free (the library drags in C++/libADLMIDI) while
still testing the real implementation rather than a copy.

Create `src/doors/termgfx/test/CMakeLists.txt`:

```cmake
# Pure unit tests for termgfx's dependency-free modules. Compile the module
# .c as a SOURCE rather than linking the termgfx library: the library pulls in
# C++/libADLMIDI, and this target's whole value is having no such dependency.
# It still tests the real implementation, not a copy of it.
add_executable(test_audio_stream
    ${CMAKE_CURRENT_SOURCE_DIR}/../audio_stream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_audio_stream.c)

target_include_directories(test_audio_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(test_audio_stream PROPERTIES C_STANDARD 11)

# This test IS its assert()s/CHECKs. A Release configure puts -DNDEBUG in
# CMAKE_C_FLAGS_<CONFIG>, compiling every assert() to nothing and making the
# target exit 0 no matter what it "checks". Undefine it for this target only;
# target_compile_options lands after the per-config flags, so the -U wins.
target_compile_options(test_audio_stream PRIVATE -UNDEBUG)
add_test(NAME audio_stream COMMAND test_audio_stream)
```

Gate it in `src/doors/termgfx/CMakeLists.txt` (append at the end), matching
syncretro's opt-in shape so a door's `add_subdirectory(termgfx)` never
builds tests by default:

```cmake
# Opt-in unit tests (see test/CMakeLists.txt). OFF by default so a consuming
# door's add_subdirectory() of this library never builds them.
option(TERMGFX_TESTS "Build the libtermgfx unit tests" OFF)
if(TERMGFX_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
```

- [ ] **Step 5: Repoint syncretro**

`src/doors/syncretro/CMakeLists.txt`: remove `syncretro_chunk.c` from the
target's source list, and delete the `test_chunk` block at `:183-185`
(its test now lives in termgfx).

`src/doors/syncretro/syncretro_audio.c`: replace
`#include "syncretro_chunk.h"` with `#include "audio_stream.h"`, and
rename every `sr_chunk_t`/`sr_chunk_*` use to `termgfx_chunk_*`.
`git rm src/doors/syncretro/syncretro_chunk.c src/doors/syncretro/syncretro_chunk.h`.

- [ ] **Step 6: Run the moved test and rebuild syncretro**

```bash
cd /home/rswindell/sbbs/src/doors/termgfx
cmake -S . -B /tmp/termgfx-test -DTERMGFX_TESTS=ON && cmake --build /tmp/termgfx-test
ctest --test-dir /tmp/termgfx-test --output-on-failure
cd /home/rswindell/sbbs/src/doors/syncretro && ./build.sh 2>&1 | tail -5
```
Expected: `ok: 0 failure(s)`, 1/1 test passed; syncretro links clean.

- [ ] **Step 7: uncrustify the new termgfx files (and ONLY those)**

```bash
cd /home/rswindell/sbbs/src/doors/termgfx
uncrustify -c ../../uncrustify.cfg --replace --no-backup audio_stream.c audio_stream.h
cmake --build /tmp/termgfx-test && ctest --test-dir /tmp/termgfx-test --output-on-failure
```
Expected: still passes. **Do not uncrustify `test/test_audio_stream.c`**
(it came from a hand-formatted door and is test code), nor anything under
`syncretro/` or `syncscumm/`.

- [ ] **Step 8: Commit**

```
termgfx: host the streaming audio chunk accumulator

The accumulator that cuts a continuous PCM stream into fixed-size chunks
was written for syncretro, but nothing in it is libretro-shaped -- it is
pure arithmetic over a sample buffer, with no I/O, no config, and no
core. A second door now needs exactly it, which is the point at which it
should stop living in the first one.

Moves it to libtermgfx as audio_stream, restyled to this directory's
conventions, with its unit test. The test compiles the module as a source
rather than linking the library, per the house rule: the library pulls in
C++/libADLMIDI, and the test's value is having no such dependency.

termgfx had no tests of its own until now, so this adds the opt-in
TERMGFX_TESTS gate -- off by default, so a door's add_subdirectory() of
the library never builds them.
```

---

## Task 3a: Byte-capture harness — the extraction's guard

**Added mid-execution (2026-07-16, user-approved).** Task 3 originally
said to prove the extraction byte-identical using syncretro's
`fakecore.c` + `fakterm.py` harness, which `syncretro/M4_AUDIO.md` §9
describes. **That harness does not exist** — the doc is aspirational, and
the only `fakterm.py` in the tree belongs to syncmoo1. Syncretro's audio
state machine has no automated coverage at all today.

Task 3 moves a live door's working audio into a shared library and its
unit tests do not touch the state machine, so it needs a real guard. The
seam makes one cheap: the module reaches its door through exactly three
output functions and five config getters, all stubbable. And because the
adapter preserves the `sr_audio_*` surface, **the same harness runs
before and after the extraction** — capture golden output now, extract,
re-run, require an identical diff.

**Files:**
- Create: `src/doors/syncretro/test_audio_bytes.c`
- Modify: `src/doors/syncretro/CMakeLists.txt` (test target, under the
  existing `SYNCRETRO_TESTS` gate)

**Interfaces:**
- Consumes: `sr_audio_*` (`syncretro_audio.h`) — the surface Task 3 must
  preserve unchanged.
- Produces: a `test_audio_bytes` binary that writes a deterministic
  transcript of everything the module emits; and a golden transcript
  committed as the before/after reference.

- [ ] **Step 1: Write the harness**

`test_audio_bytes.c` stubs the door seam and drives the module through a
scripted session. Unlike the other tests it links the real `audio.c`
(needs libsndfile) — that is the point: the encoder is not what we are
testing, but its output must flow through the real call path.

Stub these (they are the module's entire door dependency):
`sr_out_put`, `sr_io_out_flush`, `sr_io_out_backlog`, and
`sr_config_audio_{enabled,quality,volume,chunk_ms,prebuffer}`.
`sr_out_put` appends to a growing buffer; `sr_io_out_backlog` returns a
value the scenario controls (so the two-strike backlog drop is reachable);
`sr_io_out_flush` records that it was called.

Scenario, in order — each step exercises a documented behavior:
1. `sr_audio_init()`, `sr_audio_start(44100)`, `sr_audio_probe()`.
2. `sr_audio_caps(1)` — digital tier: expect the channel volume, then the
   silence upload.
3. Feed a deterministic PCM ramp (NOT random, NOT silence — a ramp whose
   value derives from the frame index), enough to close well past
   `prebuffer` chunks, so both the PRIME hold-and-release and steady-state
   RUN are covered.
4. Feed a run of pure zeros long enough to close a chunk — the cached
   silence path (`A;Load`+`A;Queue`, no `C;S`).
5. Inject `sr_audio_underrun(2)` — expect a re-prime, not a single queue.
6. Drive `sr_io_out_backlog` over the threshold for two consecutive chunk
   boundaries — expect a drop.
7. `sr_audio_shutdown()` — expect `A;Flush` on the channel, no fade.

- [ ] **Step 2: Decide the comparison unit empirically — do not assume**

Ogg streams can carry a random bitstream serial number, which would make
raw bytes differ run-to-run and turn a raw diff into a false alarm. Find
out rather than guessing:

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
./build/test_audio_bytes > /tmp/cap1.txt
./build/test_audio_bytes > /tmp/cap2.txt
diff /tmp/cap1.txt /tmp/cap2.txt && echo "DETERMINISTIC" || echo "NOT deterministic"
```

- **If identical:** the transcript is the raw emitted bytes, hex-dumped.
  Strongest possible guard.
- **If not:** emit a NORMALIZED transcript instead — one line per emitted
  APC with its verb and parameters (`C;S name=s/0 bytes=1234`,
  `A;Load slot=0 file=s/0`, `A;Queue ch=2 slot=0 vol=100 loop=0`,
  `A;Update ch=2`, `A;Flush ch=2 fade=0`), recording each payload's
  LENGTH but not its content. This is the right unit anyway: the
  extraction changes call sequencing, not encoding, and the encoder is
  unchanged code either side.

State in the report which form you used and paste the determinism check.

- [ ] **Step 3: Capture the golden transcript**

```bash
./build/test_audio_bytes > test_audio_bytes.golden
```
Commit it. This file IS the contract Task 3 must not break.

- [ ] **Step 4: Make the test self-checking**

The binary compares its own output against the committed golden and exits
non-zero on mismatch, so `ctest` catches a regression with no manual diff:
`add_test(NAME audio_bytes COMMAND test_audio_bytes --check ${CMAKE_CURRENT_SOURCE_DIR}/test_audio_bytes.golden)`.
Verify it FAILS when the golden is corrupted — a golden test that cannot
fail is worse than no test.

- [ ] **Step 5: Commit**

```
syncretro: pin the audio stream's emitted bytes with a golden transcript

The audio state machine has no automated coverage: M4_AUDIO.md section 9
describes a fakecore/fakterm harness that was never built. That was
tolerable while the module had one consumer and a human ear on it. It is
not tolerable now -- the module is about to move into libtermgfx, and a
port that quietly dropped an Update re-arm or rotated a cache name
differently would sound fine on a short listen and fail on a long one.

The module's entire door dependency is three output calls and five config
getters, so a harness can stub them and drive a scripted session: prime,
run, cached silence, an injected underrun, a sustained backlog, shutdown.
It records what the module emits and compares it against a committed
transcript.

The point is the move that follows: the extracted module keeps this same
surface, so this harness runs unchanged on both sides of it and the
transcript has to match.
```

---

## Task 3: Extract the streaming state machine into termgfx

Move the PRIME/RUN machine, backlog policy, silence caching, blob path,
and telemetry into the shared module behind an injected config + I/O
vtable. Syncretro becomes an adapter with an unchanged `sr_audio_*` API.

**Files:**
- Modify: `src/doors/termgfx/audio_stream.{h,c}`
- Rewrite: `src/doors/syncretro/syncretro_audio.c`
- Modify: `src/doors/syncretro/CMakeLists.txt` (link deps unchanged)

**Interfaces:**
- Consumes: `termgfx_chunk_*` (Task 2); `termgfx/audio.h` builders.
- Produces (consumed by Task 8):

```c
// Door-supplied output seam. `ctx` is opaque to this module.
typedef struct {
	void   (*put)(void *ctx, const void *buf, size_t len);
	int    (*flush)(void *ctx);
	size_t (*backlog)(void *ctx);
	void    *ctx;
} termgfx_stream_io_t;

typedef struct {
	int         enabled;     // 0 = module never emits a byte
	double      quality;     // Opus VBR 0.01..1.0 (TERMGFX_MUSIC_QUALITY_DEFAULT)
	int         volume;      // channel volume 0..100; 0 = stop sending entirely
	int         chunk_ms;    // 50..250
	int         prebuffer;   // 2..8 chunks held before playback starts
	int         channels;    // 1 or 2 (what the accumulator keeps)
	int         rate;        // source sample rate, Hz
	int         ch;          // SyncTERM mixer channel (2..15)
	int         slot;        // patch slot 0..255
	const char *name;        // door name for diagnostics ("syncscumm")
	const char *cache_prefix;// cache-name prefix, e.g. "s" -> "s/0".."s/z"
} termgfx_stream_cfg_t;

typedef struct termgfx_stream termgfx_stream_t;

termgfx_stream_t *termgfx_stream_create(const termgfx_stream_cfg_t *cfg,
                                        const termgfx_stream_io_t *io);
void   termgfx_stream_destroy(termgfx_stream_t *s);
void   termgfx_stream_probe(termgfx_stream_t *s);
void   termgfx_stream_caps(termgfx_stream_t *s, int tier);
void   termgfx_stream_set_blob_ok(termgfx_stream_t *s, int ok);
int    termgfx_stream_blob_active(const termgfx_stream_t *s);
void   termgfx_stream_underrun(termgfx_stream_t *s, int ch);
void   termgfx_stream_pause(termgfx_stream_t *s, int on);
void   termgfx_stream_reset(termgfx_stream_t *s);
int    termgfx_stream_volume(const termgfx_stream_t *s);
int    termgfx_stream_muted(const termgfx_stream_t *s);
int    termgfx_stream_volume_step(termgfx_stream_t *s, int delta);
size_t termgfx_stream_feed(termgfx_stream_t *s, const int16_t *pcm, size_t frames);
void   termgfx_stream_stop(termgfx_stream_t *s);
void   termgfx_stream_stats(const termgfx_stream_t *s,
                            unsigned *chunks, unsigned *underruns, unsigned *dropped);
```

- [ ] **Step 1: Port the module**

Move the body of `syncretro_audio.c` into `audio_stream.c` essentially
verbatim, with these mechanical changes (all identified during planning —
the module has **no** libretro dependency, so nothing else moves):

1. File-statics become fields of `struct termgfx_stream` (`g_state`,
   `g_paused`, `g_chunk`, `g_apc`, `g_apc_cap`, `g_held[8]`,
   `g_held_len[8]`, `g_held_n`, `g_ring`, `g_have_silence`, `g_blob_ok`,
   `g_underruns`, `g_dropped`, `g_chunks`, `g_strikes`, `g_rate`,
   `g_enabled`, plus the copied cfg and io).
2. `sr_out_put(p, n)` → `s->io.put(s->io.ctx, p, n)`;
   `sr_io_out_flush()` → `s->io.flush(s->io.ctx)`;
   `sr_io_out_backlog()` → `s->io.backlog(s->io.ctx)`.
3. `sr_config_audio_*()` → `s->cfg.*`. **Clamp in create()**, once —
   syncretro clamped in config and read `chunk_ms` live at two sites; a
   by-value cfg removes that inconsistency (clamp `quality` to 0.01..1.0
   falling back to `TERMGFX_MUSIC_QUALITY_DEFAULT`, `volume` 0..100,
   `chunk_ms` 50..250, `prebuffer` 2..8, `channels` 1..2, `rate`
   8000..192000 with a diagnostic).
4. `SR_AUDIO_CH`/`SR_AUDIO_SLOT` → `s->cfg.ch` / `s->cfg.slot`.
   `SR_AUDIO_RING` stays a module `#define` (4).
5. `fprintf(stderr, "syncretro: ...")` → `fprintf(stderr, "%s: ...", s->cfg.name, ...)`.
6. The cache-name helper takes the prefix from cfg:
   `snprintf(out, outlen, "%s/%u", s->cfg.cache_prefix, idx % TERMGFX_STREAM_RING)`,
   and the silence name becomes `"%s/z"`.
7. `sr_audio_encode()`'s literal `1` channel count becomes
   `s->cfg.channels` — the one-token stereo fix.
8. The silence fill iterates `cap * channels`.
9. `termgfx_chunk_init(&s->chunk, frames, s->cfg.channels)` where
   `frames = cfg.rate * cfg.chunk_ms / 1000`.
10. `sr_audio_start(rate)` folds into `create()` — syncscumm knows its
    rate up front and syncretro can now defer `create()` until after
    `rc_core_load_game()`. The `SR_AS_WAIT_CAPS` state is preserved.
11. Restyle: `//` comments, return type on its own line, uncrustify.

Preserve verbatim (they are the hard-won parts):
- the `SR_AS_OFF/WAIT_CAPS/PRIME/RUN` machine and its re-prime-on-underrun,
- `#define TERMGFX_STREAM_BACKLOG_BYTES (48 * 1024)` /
  `TERMGFX_STREAM_BACKLOG_STRIKES 2` and the two-strike rationale comment
  (an instantaneous socket check was tried in SyncDOOM and reverted,
  because the frame path keeps the socket busy essentially always),
- the volume-0-means-stop-sending policy and its comment,
- `termgfx_stream_stop()`'s **no-fade** `A;Flush` and the comment
  explaining why (`O=` only crossfades the head buffer and leaves the FIFO
  playing).

Add `termgfx_stream_stats()` (new) so a door can put counters in its own
trace instead of only `stderr` at shutdown — syncscumm's design requires
chunks/dropped/underruns in the trace.

Carry a caution in the header for the paths the extraction inherits
untested: `pause`, `reset`, and the volume/mute entry points have **no
call sites in syncretro today** (implemented but unwired), so they are
untested-by-use.

- [ ] **Step 2: Rewrite syncretro_audio.c as an adapter**

Keep `syncretro_audio.h` byte-identical so no call site changes. The new
`syncretro_audio.c` holds one `static termgfx_stream_t *g_stream;` and:

```c
/* syncretro_audio.c -- syncretro's adapter onto termgfx's shared PCM
 * streaming module (termgfx/audio_stream.h). The state machine, cushion,
 * backlog policy, silence cache and blob path all live there now; what
 * remains here is the door's half: its output seam, its INI-sourced config,
 * and the rate the CORE reports (which is why creation waits for
 * rc_core_load_game() -- see sr_audio_start()). M4_AUDIO.md still describes
 * the design; the code moved, the reasoning did not. */

static void sr_stream_put(void *ctx, const void *buf, size_t len)
{
	(void)ctx;
	sr_out_put(buf, len);
}

static int sr_stream_flush(void *ctx)
{
	(void)ctx;
	return sr_io_out_flush();
}

static size_t sr_stream_backlog(void *ctx)
{
	(void)ctx;
	return sr_io_out_backlog();
}

static const termgfx_stream_io_t g_stream_io = {
	sr_stream_put, sr_stream_flush, sr_stream_backlog, NULL
};
```

`sr_audio_init()` records that config said yes (`g_enabled`); the real
`termgfx_stream_create()` happens in `sr_audio_start(rate)` with
`.channels = 1` (the cores emit mono duplicated) and
`.cache_prefix = "s"`, `.ch = SR_AUDIO_CH`, `.slot = SR_AUDIO_SLOT`,
`.name = "syncretro"`. Every other `sr_audio_*` becomes a one-line
forward. `SR_AUDIO_CH`/`SR_AUDIO_SLOT`/`SR_AUDIO_RATE_FALLBACK` stay in
`syncretro_audio.h`; `SR_AUDIO_RING` moves to termgfx.

- [ ] **Step 3: Build and run every syncretro test**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
cmake -S . -B build -DSYNCRETRO_TESTS=ON && cmake --build build 2>&1 | tail -5
ctest --test-dir build --output-on-failure
```
Expected: clean build; all tests pass.

- [ ] **Step 4: Prove the wire bytes are unchanged — the real guard**

The unit tests do not cover the state machine (syncretro covers it with
`fakecore.c` + `fakterm.py` byte-stream scenarios). This refactor's whole
risk is a behavior change in a live door, so capture the emitted APC
stream before and after and diff it.

```bash
cd /home/rswindell/sbbs/src/doors/syncretro
ls fakecore* fakterm* test/ 2>/dev/null   # locate the fake-terminal harness
```
Run the fake-terminal scenario that exercises audio against `master`'s
pre-Task-1 build and against the current build, capturing each session's
bytes to a file, and diff them. They must be **identical** modulo
timestamps. If the harness cannot be driven headlessly, say so and stop —
do not proceed to Task 4 on unverified extraction.

- [ ] **Step 5: uncrustify termgfx only, rebuild, re-test**

```bash
cd /home/rswindell/sbbs/src/doors/termgfx
uncrustify -c ../../uncrustify.cfg --replace --no-backup audio_stream.c audio_stream.h
cd ../syncretro && cmake --build build && ctest --test-dir build --output-on-failure
```

- [ ] **Step 6: Commit**

```
termgfx: share the PCM streaming module between doors

syncretro's audio module turned out to have no libretro in it at all: it
never includes libretro.h, never touches a core, and reaches the door
only through three output calls and five config values. It is a general
answer to "I have one continuous mixed PCM stream and a SyncTERM channel
FIFO" -- and ScummVM's mixer is about to ask the same question.

Moves the state machine, cushion, two-strike backlog policy, silence
cache, blob path and telemetry into libtermgfx behind an injected config
struct and a put/flush/backlog vtable. The encoder's channel count is now
cfg.channels rather than a hardcoded 1, so a genuinely stereo source
works. syncretro keeps its sr_audio_* surface and becomes an adapter: no
call site changed.

The alternative was a second copy in the second door. The reasoning that
made this code correct -- why the cushion exists, why an underrun
re-primes instead of resuming, why the backlog needs two strikes, why the
final flush must not fade -- is worth having in one place.
```

---

## Task 4: Syncretro live regression

**Files:** none (verification only).

The extraction touched a live, working door. The user runs this.

- [ ] **Step 1: Build and report**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro && ./build.sh 2>&1 | tail -5
```

- [ ] **Step 2: Ask the user for a syncretro live sound check**

The door is playable at `/sbbs/xtrn/syncivision`. Ask the user to launch
it on SyncTERM and confirm: sound plays, stays in sync, no stutter or
dropouts at startup, clean exit with no audio tail. Report exactly what
they say; **do not claim the extraction is verified without it.** If they
report a regression, fix before Task 5 — do not build SyncSCUMM's audio
on a broken shared module.

---

## Task 5: Bridge libsndfile into SyncSCUMM's link

Must land before any SyncSCUMM code references `termgfx_audio_*`. Today
`libtermgfx.a` contains `audio.c`, but SyncSCUMM's link never pulls that
object because nothing references it. The first reference pulls it in and
the link fails on libsndfile symbols — `build.sh` hand-transcribes flags
into ScummVM's `config.mk` and passes only `-lpthread -lm` (+ JXL).

**Files:**
- Modify: `src/doors/syncscumm/door/CMakeLists.txt`
- Modify: `src/doors/syncscumm/build.sh`
- Modify: `src/doors/syncscumm/test/unit_sst_io.sh`

**Interfaces:**
- Produces: `build/libs/sndfile_libs.txt`; `SYNCSCUMM_WITH_SNDFILE` define
  reaching both `%.c` and `%.cpp` rules.

- [ ] **Step 1: Write the failing check**

Prove the gap first, so the fix is verified rather than assumed:

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./build.sh 2>&1 | tail -3
cat > /tmp/snd_probe.c <<'EOF'
#include "audio.h"
#include <stdio.h>
int main(void) { printf("%d\n", termgfx_audio_have_ogg()); return 0; }
EOF
cc -o /tmp/snd_probe -I../termgfx /tmp/snd_probe.c \
   build/libs/termgfx/libtermgfx.a -lpthread -lm
```
Expected: **FAIL** — undefined references to `sf_open_virtual`/`sf_write_short`/
etc. (This is exactly what Task 9 would hit.) If it unexpectedly links,
libsndfile was statically absorbed or termgfx was built without it — check
`grep -i sndfile build/libs/CMakeCache.txt` and report before proceeding.

- [ ] **Step 2: Write sndfile_libs.txt from CMake**

`src/doors/syncscumm/door/CMakeLists.txt` — mirror the existing
`jxl_libs.txt` block at `:33-38`:

```cmake
# ScummVM's make links libtermgfx.a by hand (build.sh appends to config.mk),
# so every external the archive needs must be transcribed across that seam --
# CMake's PUBLIC link interface does not survive it. termgfx's Ogg/Opus encode
# (audio.c -> libsndfile) is pulled in the moment sst_io.c references any
# termgfx_audio_* symbol, so hand build.sh the flags the same way the JXL
# handoff above does.
get_target_property(_tg_libs termgfx INTERFACE_LINK_LIBRARIES)
set(_snd_libs "")
if(TARGET SndFile::sndfile)
    set(_snd_out "")
    get_target_property(_snd_loc SndFile::sndfile IMPORTED_LOCATION)
    if(_snd_loc)
        set(_snd_out "${_snd_loc}")
    else()
        set(_snd_out "-lsndfile")
    endif()
    set(_snd_libs "${_snd_out}")
endif()
file(WRITE ${CMAKE_BINARY_DIR}/sndfile_libs.txt "${_snd_libs}")
```

Do **not** touch the `WITHOUT_${_aud}` force-disable loop at `:8-11` —
those are xpdev's *local playback* backends (OSS/ALSA/PulseAudio). The
door encodes and the terminal plays; it has no business opening a sound
card. Leave them off.

- [ ] **Step 3: Consume it in build.sh**

`src/doors/syncscumm/build.sh` — after the JXL block (`:39-47`), same
shape, same absolute-path-vs-`-l` case split:

```sh
# libsndfile: termgfx's Ogg/Opus encode (M4 audio). Same seam as JXL above --
# LIBS, because ScummVM's make does the final link and never saw CMake's
# PUBLIC link interface. May be an absolute path or an -l token.
if [ -s "$HERE/build/libs/sndfile_libs.txt" ]; then
	SNDFILE_LIBS=$(cat "$HERE/build/libs/sndfile_libs.txt")
	echo "LIBS += $SNDFILE_LIBS" >> config.mk
	# DEFINES (not CXXFLAGS): Makefile.common folds DEFINES into CPPFLAGS,
	# which both the %.c and %.cpp generic rules pick up, so the plain-C
	# sst_io.o sees it too.
	echo "DEFINES += -DSYNCSCUMM_WITH_SNDFILE" >> config.mk
fi
```

- [ ] **Step 4: Mirror into the unit-test runner**

`src/doors/syncscumm/test/unit_sst_io.sh` — beside the `JXL_DEFINE`/`JXL_LIBS`
derivation (`:12-22`):

```sh
SNDFILE_LIBS=""
if [ -s "$DOOR/build/libs/sndfile_libs.txt" ]; then
	SNDFILE_LIBS=$(cat "$DOOR/build/libs/sndfile_libs.txt")
fi
```
and append `$SNDFILE_LIBS` to every `cc` line's link tail (after `$JXL_LIBS`).

- [ ] **Step 5: Verify the bridge closes the gap**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./build.sh 2>&1 | tail -3
cat build/libs/sndfile_libs.txt; echo
cc -o /tmp/snd_probe -I../termgfx /tmp/snd_probe.c \
   build/libs/termgfx/libtermgfx.a -lpthread -lm $(cat build/libs/sndfile_libs.txt)
/tmp/snd_probe
```
Expected: links; prints `1` (termgfx built with libsndfile). If it prints
`0`, libsndfile was not found at termgfx configure time — audio will be
silently unavailable; report that rather than proceeding.

- [ ] **Step 6: Run the existing suite (nothing should change yet)**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm
./test/unit_sst_io.sh && ./test/unit_sst_quant.sh
```
Expected: all pass.

- [ ] **Step 7: Commit**

```
syncscumm: carry libsndfile across the CMake-to-ScummVM link seam

The door's libraries are built by CMake but linked by ScummVM's make,
with build.sh hand-transcribing the flags into the generated config.mk.
CMake's PUBLIC link interface does not survive that seam, so libtermgfx's
externals have to be handed over explicitly -- as JXL already is.

Nothing referenced termgfx's audio module yet, so its libsndfile
dependency was never pulled in and the gap was invisible. The first
termgfx_audio_* call would have failed the link. Adds the sndfile_libs.txt
handoff alongside the JXL one, and mirrors it into the unit-test runner,
which links the same archive by hand.

The xpdev local-playback backends (OSS/ALSA/PulseAudio) stay force-
disabled: the door encodes, the terminal plays, and it has no business
opening a sound card.
```

---

## Task 6: Session layer — backlog query + audio probe + caps parsing

**Files:**
- Modify: `src/doors/syncscumm/door/sst_io.c`, `sst_io.h`
- Test: `src/doors/syncscumm/test/test_sst_io_audio.c` (create),
  `src/doors/syncscumm/test/unit_sst_io.sh`

**Interfaces:**
- Consumes: `termgfx_audio_query`, `termgfx_audio_parse_caps()`,
  `termgfx_audio_opus_query`, `termgfx_audio_parse_opus()` (`termgfx/audio.h`).
- Produces (consumed by Tasks 7, 8): `size_t sst_io_out_backlog(void);`
  and file-statics `g_audio_tier` (-1/0/1), `g_audio_done`, `g_opus_ok`.

- [ ] **Step 1: Write the failing test**

Create `src/doors/syncscumm/test/test_sst_io_audio.c` on the
`test_sst_io_nogfx.c` skeleton (fresh session per binary — sst_io keeps
file-static state with no reset).

```c
/* test_sst_io_audio.c -- the audio capability probe and its replies.
 *
 * Asserts (a) the startup burst solicits the audio caps query, (b) a
 * digital-tier reply resolves sst_io_audio_available() to 1 without eating
 * the settle window in real time, and (c) the two-stage Opus query is only
 * emitted once the caps reply has landed.
 *
 * sst_io keeps file-static session state with no reset, so exercising the
 * probe cleanly needs a fresh process. cc'd + run by unit_sst_io.sh.
 */
#include "sst_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int drain(int fd, char *buf, size_t cap)
{
	ssize_t n;
	size_t  tot = 0;

	while ((n = recv(fd, buf + tot, cap - 1 - tot, MSG_DONTWAIT)) > 0) {
		tot += (size_t)n;
		if (tot >= cap - 1)
			break;
	}
	buf[tot] = '\0';
	return (int)tot;
}

int main(void)
{
	int         sv[2];
	char        fdarg[32];
	char       *argv[2];
	static char out[65536];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = "syncscumm";
	argv[1] = fdarg;

	assert(sst_io_init(2, argv) == 1);
	assert(sst_io_active() == 1);
	sst_io_flush();
	drain(sv[0], out, sizeof out);

	/* (a) the burst solicits the audio caps query ... */
	assert(strstr(out, "libsndfile") != NULL);
	/* ... and NOT the Opus query, which is two-stage: it can only be asked
	 * once the terminal has confirmed a digital tier, because it queries a
	 * FORMAT that only a libsndfile-having terminal could decode. */
	assert(strstr(out, "32;100") == NULL);

	/* Inject a digital-audio caps reply (ESC[=7;100;1n) plus a JXL "no"
	 * (ESC[=1;0n), which latches the graphics settle flag so no test eats
	 * SST_GFX_SETTLE_MS of real time. */
	{
		const char *reply = "\x1b[=7;100;1n\x1b[=1;0n";
		assert(send(sv[0], reply, strlen(reply), 0) > 0);
		sst_io_pump();
	}

	/* (c) the caps reply triggers the Opus query. */
	sst_io_flush();
	drain(sv[0], out, sizeof out);
	assert(strstr(out, "32;100") != NULL);

	printf("SST_IO_AUDIO OK\n");
	return 0;
}
```

(The `sst_io_audio_available()` assertions land in Task 7, which is what
implements it. This task pins only the probe and the reply parsing.)

Append to `src/doors/syncscumm/test/unit_sst_io.sh`, before the
`mktemp`/`trap` block (its `cd` subshell ends the file):

```sh
# The audio probe needs a fresh sst_io session (file-static tier/settle
# state, no reset), so it's its own binary rather than another case above.
cc -o /tmp/test_sst_io_audio $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_audio.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_audio
```

- [ ] **Step 2: Run it to verify it fails**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./test/unit_sst_io.sh
```
Expected: FAIL at the first assert — the burst contains no audio query.

- [ ] **Step 3: Implement**

`sst_io.c` — add near the other parser statics (`:329-345` region):

```c
/* ---- audio capability state (M4) ----
 * g_audio_tier mirrors termgfx_audio_parse_caps(): -1 none/unknown (the
 * terminal never answered, i.e. no audio APC at all), 0 tone-only (audio
 * APC but no libsndfile, so A;Load is a no-op and a Queue would play an
 * empty slot), 1 digital. g_audio_done latches "the question is answered"
 * so sst_io_audio_available()'s bounded wait can exit the instant the
 * reply lands instead of burning its whole window -- the same trick
 * g_jxl_done plays for the graphics settle (see jxl_scan_feed()). */
static int g_audio_tier = -1;
static int g_audio_done;
static int g_opus_ok = 1;       /* -1 from parse_opus means "no reply": an
                                 * older SyncTERM that never answers, so
                                 * assume yes, as termgfx/audio.h:41-51 says */
static int g_opus_asked;
```

Add the caps query to the burst at `:719`, after `termgfx_query_jxl`. The
XTVERSION-first invariant is unaffected (it constrains only
XTVERSION-before-canvas), but say so:

```c
	out_puts(termgfx_query_jxl);
	/* Audio caps (SyncTERM:Q;libsndfile). Tail of the burst: nothing gates
	 * on its absence, and the XTVERSION-before-canvas invariant above
	 * constrains only those two. The Opus query is NOT sent here -- it is
	 * two-stage (audio_scan_feed(), below). */
	out_puts(termgfx_audio_query);
```

Add `audio_scan_feed()` beside `jxl_scan_feed()` — the house pattern for
`=`-marker replies, which don't survive the CSI dispatcher cleanly:

```c
/* Scan accumulated input for the audio capability replies. Mirrors
 * jxl_scan_feed(): termgfx_audio_parse_caps()/_parse_opus() have the same
 * idempotent-over-a-growing-buffer contract as termgfx_caps_parse_jxl().
 *
 * TWO-STAGE, deliberately: parse_caps only reports whether libsndfile is
 * present at all, not whether it decodes Ogg-Opus -- our codec, which older
 * distro libsndfile builds lack. So the Opus query goes out only once the
 * digital tier is confirmed, and a terminal that never answers it keeps the
 * historical assume-yes (termgfx/audio.h:41-51). */
static void audio_scan_feed(const uint8_t *buf, size_t n)
{
	int r;

	if (g_audio_done && g_opus_asked)
		return;
	acc_append(buf, n);   /* same accumulator as jxl_scan_feed() */
	if (!g_audio_done) {
		r = termgfx_audio_parse_caps(g_acc, (int)g_acc_len);
		if (r >= 0) {
			g_audio_tier = r;
			g_audio_done = 1;
			g_is_syncterm = 1;   /* only SyncTERM answers this */
			if (r >= 1 && !g_opus_asked) {
				out_puts(termgfx_audio_opus_query);
				sst_io_flush();
				g_opus_asked = 1;
			}
		}
	}
	if (g_opus_asked) {
		r = termgfx_audio_parse_opus(g_acc, (int)g_acc_len);
		if (r >= 0)
			g_opus_ok = r;
	}
}
```

Call it in `sst_io_pump()`'s consumer triple at `:813-815`:

```c
		sst_trace_in(buf, (int)n);
		jxl_scan_feed(buf, (size_t)n);
		audio_scan_feed(buf, (size_t)n);
		parse_bytes(buf, (int)n);
```

**Extend the `g_acc` shrink guard** at `:486-492`. Today it shrinks the
accumulator to a 256-byte tail once `g_jxl_done && g_probe_replied` —
which would truncate an audio reply still in flight:

```c
	/* Shrink once every scanner that reads g_acc has its answer. The audio
	 * conditions are new (M4): without them the accumulator could drop the
	 * tail an audio reply still needed. The Opus stage counts only if it was
	 * ever asked -- a tone-only or silent terminal never asks. */
	if (g_jxl_done && g_probe_replied && g_audio_done
	    && (!g_opus_asked || g_opus_seen)) {
```
(Add `static int g_opus_seen;`, set beside `g_opus_ok`.)

Add the backlog query (`sst_io.c` near `sst_io_frames_dropped()` at `:824`):

```c
/* Staged bytes not yet written to the socket. The streaming audio module
 * reads this to decide whether the link is genuinely congested (termgfx/
 * audio_stream.h's two-strike rule) -- NOT an instantaneous socket check,
 * which SyncDOOM tried for SFX and reverted, because the frame path keeps
 * the socket busy essentially always. Unlike syncretro's equivalent there is
 * no g_out_off to subtract: sst_io_flush() memmoves the tail down. */
size_t sst_io_out_backlog(void) { return g_out_len; }
```
and declare it in `sst_io.h` beside `sst_io_frames_dropped()`.

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./build.sh 2>&1 | tail -3 && ./test/unit_sst_io.sh
```
Expected: `SST_IO_AUDIO OK`, plus every pre-existing binary still passing
(`SST_IO_NOGFX OK`, canvas, gfxmax, xterm_ceiling, sixelmax_override).

- [ ] **Step 5: Commit**

This task's parsing and Task 7's settle wait are two halves of one
behavior, and Task 7's commit message covers both. Commit here only if
Task 7 will not follow immediately; otherwise leave the changes staged
and let Task 7 make the single commit. Either way, **check
`git diff --cached` first — the index is shared with the user.**

---

## Task 7: `sst_io_audio_available()` — a real bounded wait

The one-shot query with no re-entry. Every other bounded wait in this file
is poll-and-return, because present() gets called again by ScummVM's loop.
This one is called once, from `resolveSubtitles()` in `initBackend()`,
before the engine runs — so it needs an internal loop, a pattern the file
does not yet contain.

**Files:**
- Modify: `src/doors/syncscumm/door/sst_io.c` (`:826-830`), `sst_io.h` (`:37-43`)
- Test: `src/doors/syncscumm/test/test_sst_io_audio.c` (from Task 6),
  `src/doors/syncscumm/test/subtitles.sh` (verify unchanged)

**Interfaces:**
- Consumes: `g_audio_tier`, `g_audio_done`, `g_opus_ok` (Task 6).
- Produces: `sst_io_audio_available()` returning 1 only for a confirmed
  digital, Opus-capable tier.

- [ ] **Step 1: Extend the test**

Add to `test_sst_io_audio.c` before the final `printf` — the tone-only and
Opus-refused paths must both read as "no audio":

```c
	/* A tone-only terminal (libsndfile absent) is NOT audio for our
	 * purposes: A;Load is a no-op there, so a Queue would play an empty
	 * slot. Covered in its own binary -- see test_sst_io_audio_tone.c. */
```
and create `src/doors/syncscumm/test/test_sst_io_audio_tone.c`: the same
skeleton, injecting `"\x1b[=7;100;0n\x1b[=1;0n"`, asserting
`sst_io_audio_available() == 0` and that **no** Opus query was emitted
(`strstr(out, "32;100") == NULL`), printing `SST_IO_AUDIO_TONE OK`. Add
its `cc` block to `unit_sst_io.sh` with the fresh-session comment.

- [ ] **Step 2: Run to verify it fails**

Expected: `test_sst_io_audio_tone` fails — the stub returns 0 for the
right reason by accident, but the "no Opus query" assert catches a tier-0
terminal being asked. Confirm which assert fires before implementing.

- [ ] **Step 3: Implement**

Replace `sst_io.c:826-830`:

```c
/* Does this session have working audio? Drives the subtitles auto-decision
 * (door/syncscumm.cpp): no audio -> subtitles on.
 *
 * This is the ONE bounded wait in this file that must actually block. Every
 * other one (the JXL settle at sst_io_present(), the canvas hold) is a
 * poll-and-return: ScummVM's loop calls present() again, so "hold" means
 * "return and get asked again". Nobody re-asks this question -- it is called
 * once, from resolveSubtitles() in initBackend(), before the engine runs and
 * before a single frame is drawn. Returning "no audio" merely because the
 * reply had not landed yet would bake subtitles on for the whole session.
 *
 * So: pump until the answer latches or the window expires. Anchored on
 * g_probe_start_ms (init), NOT on first present -- unlike present()'s canvas
 * hold, which had to move its anchor because engine boot outlasts the grace.
 * Here we ARE inside boot, so init is the right anchor and the wait is
 * bounded by what is left of the window.
 *
 * Costs a few hundred ms once, at boot, on a terminal that never answers
 * (Foot, xterm) -- which is today's behavior anyway: the wait expires,
 * subtitles auto-on. A headless/capture session has no fd, never probes, and
 * returns 0 immediately.
 */
int sst_io_audio_available(void)
{
	if (!g_active || g_hangup)
		return 0;   /* headless/capture or dead: never any audio */
	while (!g_audio_done
	       && (int32_t)(now_ms() - g_probe_start_ms) < SST_GFX_SETTLE_MS) {
		sst_io_pump();
		if (g_hangup)
			return 0;
		if (g_audio_done)
			break;
		usleep(2000);   /* 2ms: the reply is one packet away, not a poll loop */
	}
	/* Tier 0 is audio-APC-capable but libsndfile-less: A;Load is a no-op
	 * there, so a Queue would play an empty slot -- silent, and worse than
	 * silent because subtitles would be off. Only tier 1 counts. g_opus_ok
	 * defaults to 1 for an older SyncTERM that never answers the Opus query
	 * (termgfx/audio.h:41-51). */
	return (g_audio_tier >= 1 && g_opus_ok) ? 1 : 0;
}
```

The Opus reply may land after `g_audio_done` latches. The window above
exits on caps, so `g_opus_ok` may still be its default 1 — correct by the
assume-yes rule, and Task 9's feed path re-reads it as replies arrive.
If the Opus reply says 0 later, `termgfx_stream_caps()` is never given a
digital tier (Task 9 gates on both), so nothing is ever queued.

`usleep` needs `<unistd.h>` — already included.

Update the `sst_io.h:37-43` contract comment to describe the real
behavior (drop the "M2 has no audio path / returns 0 for now" wording).

- [ ] **Step 4: Run to verify it passes**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./build.sh 2>&1 | tail -3 && ./test/unit_sst_io.sh
```
Expected: `SST_IO_AUDIO OK`, `SST_IO_AUDIO_TONE OK`, all pre-existing pass.

- [ ] **Step 5: Verify the subtitles test still passes**

`test/subtitles.sh:15,25` asserts the literal string
`"syncscumm: subtitles auto -> on (no audio this session)"` from a
headless PPM-dump run. Headless has no fd, `sst_io_init()` returns 0,
`g_active` stays 0, so the new early return keeps that true — but prove it:

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./test/subtitles.sh
```
Expected: passes, and the run does **not** take an extra 2s (the early
return must fire before the wait).

- [ ] **Step 6: Commit**

```
syncscumm: resolve the subtitles auto-decision from a real audio probe

The auto tier had no signal to read: sst_io_audio_available() was a
stub returning 0, so "auto" always meant subtitles on. It now asks the
terminal.

The probe rides the startup burst; the Opus query is two-stage, because
knowing libsndfile is present does not tell us it decodes Ogg-Opus --
older distro builds do not, and a terminal that never answers keeps the
historical assume-yes. A tone-only terminal reads as no-audio: A;Load is
a no-op without libsndfile, so a queue would play an empty slot.

Unlike every other bounded wait here, this one genuinely blocks. The
others are poll-and-return because ScummVM's loop calls present() again;
nobody re-asks this question -- it is answered once, during initBackend(),
before a frame is drawn, and answering "no" merely because the reply had
not arrived yet would bake subtitles on for the whole session. It pumps
until the answer latches or the window expires, and a terminal that never
answers costs a few hundred ms once, at boot, then behaves exactly as it
does today.

Also adds the staged-output backlog query the streaming module needs, and
extends the accumulator's shrink guard to cover the audio replies -- it
could otherwise have dropped a tail one was still waiting on.
```

---

## Task 8: Session layer — stream the PCM

**Files:**
- Modify: `src/doors/syncscumm/door/sst_io.{c,h}`
- Test: `src/doors/syncscumm/test/test_sst_io_audio.c`

**Interfaces:**
- Consumes: `termgfx_stream_*` (Task 3); `sst_io_out_backlog()` (Task 6).
- Produces (consumed by Task 9): `void sst_io_audio_stream(const int16_t *pcm, size_t frames);`
  `void sst_io_audio_stop(void);`

- [ ] **Step 1: Extend the test**

Add to `test_sst_io_audio.c`, after the availability assert:

```c
	/* Streamed PCM reaches the wire as audio APCs on a digital terminal. */
	{
		static int16_t pcm[22050 * 2];   /* 1s of 22050Hz stereo, non-silent */
		int            i;

		for (i = 0; i < 22050; i++) {
			pcm[2 * i]     = (int16_t)(i * 37);
			pcm[2 * i + 1] = (int16_t)(i * -37);
		}
		sst_io_audio_stream(pcm, 22050);
		sst_io_flush();
		drain(sv[0], out, sizeof out);
		/* prebuffer chunks are held, then shipped together: a 1s feed at
		 * 100ms chunks closes 10, well past the 3-chunk cushion. */
		assert(strstr(out, "A;") != NULL);
	}
```

And in `test_sst_io_audio_tone.c` (the silent-terminal guard — the
regression the design's Foot/xterm promise rests on):

```c
	/* The same PCM on a tone-only terminal must produce ZERO audio bytes. */
	{
		static int16_t pcm[22050 * 2];
		int            i;

		for (i = 0; i < 22050; i++)
			pcm[2 * i] = pcm[2 * i + 1] = (int16_t)(i * 37);
		drain(sv[0], out, sizeof out);   /* clear */
		sst_io_audio_stream(pcm, 22050);
		sst_io_flush();
		drain(sv[0], out, sizeof out);
		assert(strstr(out, "A;") == NULL);
	}
```

- [ ] **Step 2: Run to verify it fails**

Expected: link error — `sst_io_audio_stream` undefined.

- [ ] **Step 3: Implement**

`sst_io.c` — add `#include "audio_stream.h"` beside the other termgfx
includes, and:

```c
/* ---- streamed audio (M4) ----
 * The mixer's PCM goes out through termgfx's shared streaming module
 * (termgfx/audio_stream.h): cushion, backlog policy, silence cache, blob
 * path and underrun re-prime all live there. This file's job is the door's
 * half -- the output seam and the session's config. */
static termgfx_stream_t *g_stream;

static void sst_stream_put(void *ctx, const void *buf, size_t len)
{
	(void)ctx;
	out_put(buf, len);
}

static int sst_stream_flush(void *ctx)
{
	(void)ctx;
	sst_io_flush();
	return g_hangup ? 0 : 1;
}

static size_t sst_stream_backlog(void *ctx)
{
	(void)ctx;
	return sst_io_out_backlog();
}

static const termgfx_stream_io_t g_stream_io = {
	sst_stream_put, sst_stream_flush, sst_stream_backlog, NULL
};

/* Lazily create the stream on the first PCM, once the tier is known.
 * Deliberately not in sst_io_init(): the caps reply has not landed there,
 * and a stream created against an unknown tier would either drop the
 * cushion's first chunks or emit to a terminal that cannot play them. */
static void audio_stream_open(void)
{
	termgfx_stream_cfg_t cfg;

	memset(&cfg, 0, sizeof cfg);
	cfg.enabled      = 1;
	cfg.quality      = TERMGFX_MUSIC_QUALITY_DEFAULT;
	/* 70, not 100: SyncTERM's channel mixer soft-clips (tanh knee), and this
	 * stream is ALREADY mixed -- every voice, effect and music track summed
	 * by ScummVM. At 100 the sum distorts. ~70 is ~-3dB of headroom, the
	 * same fix syncconquer's FMV audio needed. NEVER spell a mute as 0 here:
	 * SyncTERM clamps a channel volume of 0 to -60dB and bakes it into the
	 * channel's entry volume (it cost SyncDuke a debugging session); the
	 * module stops sending instead. */
	cfg.volume       = 70;
	cfg.chunk_ms     = 100;
	cfg.prebuffer    = 3;      /* ~300ms: the latency budget AND the jitter
	                            * tolerance -- see audio_stream.h */
	cfg.channels     = 2;      /* ScummVM mixes genuine stereo */
	cfg.rate         = SST_AUDIO_RATE;
	cfg.ch           = 2;
	cfg.slot         = 0;
	cfg.name         = "syncscumm";
	cfg.cache_prefix = "s";
	g_stream = termgfx_stream_create(&cfg, &g_stream_io);
	if (g_stream == NULL)
		return;
	/* g_cterm_ver is already latched from the CTDA reply (sst_io.c:386-388,
	 * maj*1000+min); the JXL path next to it uses the same threshold to pick
	 * DrawJXLBlob. >= 1.329 means A;LoadBlob works, so chunks ship inline
	 * with no cache-file churn at all. */
	termgfx_stream_set_blob_ok(g_stream, g_cterm_ver >= TERMGFX_CTERM_VER_BLOB);
	termgfx_stream_caps(g_stream, g_audio_tier >= 1 && g_opus_ok ? 1 : 0);
}

void sst_io_audio_stream(const int16_t *pcm, size_t frames)
{
	if (!g_active || g_hangup || pcm == NULL || frames == 0)
		return;
	if (g_stream == NULL) {
		if (!g_audio_done)
			return;   /* tier unknown: drop, don't guess */
		audio_stream_open();
		if (g_stream == NULL)
			return;
	}
	termgfx_stream_feed(g_stream, pcm, frames);
}

void sst_io_audio_stop(void)
{
	if (g_stream == NULL)
		return;
	termgfx_stream_stop(g_stream);   /* A;Flush, no fade -- see audio_stream.h */
	termgfx_stream_destroy(g_stream);
	g_stream = NULL;
}
```

Add `#define SST_AUDIO_RATE 22050` beside the other `SST_*` constants
(`:866-867` region) with the design's rationale: retro-game audio has no
content above ~11kHz, so 22050 halves the PCM handling versus 48000, and
termgfx's encoder resamples to an Opus-legal rate itself.

Call `sst_io_audio_stop()` from `sst_io_shutdown()` **before** the
terminal restore, so no queued tail plays after the door hands the
terminal back (mirrors `syncretro/main.c:598`).

Declare both in `sst_io.h`.

- [ ] **Step 4: Run to verify it passes**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./build.sh 2>&1 | tail -3 && ./test/unit_sst_io.sh
```
Expected: all binaries pass, including the tone-terminal zero-bytes guard.

- [ ] **Step 5: Add the underrun hook**

`sst_io.c`'s `csi_final()` `case 'n'` (`:410-414`) — the idle notification
is a *runtime* event with no accumulator to scan, so unlike the caps
replies it belongs in the CSI dispatcher:

```c
		case 'n':   /* CTerm state report -- the Q;JXL and audio-caps replies
		             * are parsed by the raw-byte scanners (jxl_scan_feed(),
		             * audio_scan_feed()); what lands here is the runtime
		             * audio idle notification, which has no probe
		             * accumulator to scan. */
			g_probe_replied = 1;
			if (g_csi_len > 0 && g_csi_par[0] == '=') {
				int p[8], np;

				np = csi_params(p, 8);
				/* `CSI = 7 ; <ch> ; 0 n`: that channel's FIFO drained. The
				 * p[1] != 100 guard is load-bearing -- the caps reply shares
				 * the =7 prefix and feature id 100 is reserved for it. */
				if (np >= 3 && p[0] == 7 && p[1] != 100 && p[2] == 0
				    && g_stream != NULL)
					termgfx_stream_underrun(g_stream, p[1]);
			}
			return;
```

Verify `csi_params()` skips the leading `=` (the map says it does, at
`:357-371`) — if not, offset accordingly.

- [ ] **Step 6: Add the trace counters**

Extend `sst_trace()` (`:1400-1405`) with the stream's counters, per the
design's diagnosability requirement:

```c
	unsigned ch = 0, ur = 0, dr = 0;

	if (g_stream != NULL)
		termgfx_stream_stats(g_stream, &ch, &ur, &dr);
```
and append ` audio=%d/%u/%u/%u` (tier, chunks, underruns, dropped) to the
format. Note the existing `%-13s` outcome column already overruns on
`gated-inflight` (14 chars) — leave that alone, it is pre-existing.

- [ ] **Step 7: Run the suite + commit**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./build.sh 2>&1 | tail -3
./test/unit_sst_io.sh && ./test/unit_sst_quant.sh && ./test/subtitles.sh
```

```
syncscumm: stream the session's audio to SyncTERM

Wires the door's output seam onto termgfx's shared PCM streaming module
and hands it the session's shape: 22050Hz stereo, 100ms chunks, a
three-chunk cushion.

The channel runs at 70, not 100. This stream is already mixed -- every
voice, effect and music track summed by ScummVM -- and SyncTERM's channel
mixer soft-clips, so a hot pre-mixed stream at unity distorts. ~70 buys
about 3dB of headroom, the same fix the syncconquer FMV audio needed.

The stream is created on the first PCM rather than at init, because the
capability reply has not landed at init and a stream built against an
unknown tier would either throw away the cushion's first chunks or emit
to a terminal that cannot play them. A tone-only terminal produces zero
audio bytes, which the tests pin.

The idle notification goes through the CSI dispatcher rather than the
probe scanners: it is a runtime event with no accumulator to scan. Its
feature-id guard is load-bearing -- the caps reply shares the =7 prefix.
```

---

## Task 9: The mixer manager

**Files:**
- Create: `src/doors/syncscumm/door/audio_term.{h,cpp}`
- Modify: `src/doors/syncscumm/door/syncscumm.cpp`, `module.mk`,
  `video_term.cpp`

**Interfaces:**
- Consumes: `sst_io_audio_stream()` (Task 8).
- Produces: `class SyncscummMixerManager : public MixerManager` with
  `void init() override; void update(uint8 callbackPeriod = 10); void tick();`

- [ ] **Step 1: Write the header**

`src/doors/syncscumm/door/audio_term.h` — hand-formatted, `/* */`:

```c
/* audio_term.h -- ScummVM's mixer -> the door's audio stream.
 *
 * Replaces NullMixerManager. ScummVM sums every voice, sound effect and
 * synthesized music track (the AdLib/MT-32 emulators register on this same
 * mixer via playStream()) into ONE interleaved S16 stereo stream. That single
 * pull is the whole audio path: there is no useful per-sound boundary to hook,
 * and music arrives already rendered to PCM, so a separate MIDI path would
 * re-do work the engine just did.
 *
 * WALL-CLOCK DRIVEN. Each tick pulls exactly the samples real time is owed:
 * frames_due = elapsed_ms * rate / 1000 - frames_pulled. The game's own pacing
 * can then neither starve nor flood the stream, and audio cannot drift against
 * video, because both ride one clock. Pacing the pull off the game loop instead
 * is what made syncconquer's FMV audio drift ~4% and fall behind after a couple
 * of minutes.
 *
 * The manager is created unconditionally -- engines need a mixer object however
 * the terminal answered -- and whether the pulled PCM is streamed or discarded
 * is sst_io's decision, not ours.
 */
#ifndef SYNCSCUMM_AUDIO_TERM_H_
#define SYNCSCUMM_AUDIO_TERM_H_

#include "backends/mixer/mixer.h"

/* The mixer's rate. Retro adventure audio has no content above ~11kHz, so
 * 22050 halves the PCM we handle versus 48000 and costs nothing audible;
 * termgfx's encoder resamples to an Opus-legal rate itself. */
#define SYNCSCUMM_MIXER_RATE 22050

/* MixerManager's pure virtuals are exactly init()/suspendAudio()/resumeAudio()
 * (backends/mixer/mixer.h); isNullDevice() is virtual with a false default.
 * NOTE there is NO update() in the base -- that is NullMixerManager's own
 * invention, a call-counter that shoots the callback every Nth call. We are
 * wall-clock driven and have no period to honor, so tick() replaces it
 * outright. _mixer and _audioSuspended are the base's protected members;
 * don't shadow them. */
class SyncscummMixerManager : public MixerManager {
public:
	SyncscummMixerManager();
	virtual ~SyncscummMixerManager();

	void init() override;
	void suspendAudio() override;
	int  resumeAudio() override;
	bool isNullDevice() const override;

	/* Pull whatever real time is owed and ship it. Cheap and internally
	 * rate-limited by the wall clock, so calling it from more than one site
	 * is safe -- and necessary: see the call sites in syncscumm.cpp and
	 * video_term.cpp. */
	void tick();

private:
	uint32  _startMs;
	uint64  _framesPulled;
	int16  *_buf;
	size_t  _bufFrames;
};

/* The present path must tick too (see the tick() call site in
 * video_term.cpp), and it cannot get here through g_system:
 * getMixerManager() lives on ModularBackend, not OSystem. So the manager
 * publishes itself for that one caller. NULL until initBackend() runs. */
extern SyncscummMixerManager *g_syncscumm_mixer;

#endif /* SYNCSCUMM_AUDIO_TERM_H_ */
```

- [ ] **Step 2: Write the implementation**

`src/doors/syncscumm/door/audio_term.cpp`:

```cpp
/* audio_term.cpp -- see audio_term.h. */
#include "common/scummsys.h"
#include "audio_term.h"
#include "audio/mixer_intern.h"
#include "common/system.h"

extern "C" {
#include "sst_io.h"
}

/* One tick's ceiling. A pull is normally ~10-20ms of frames; this only bounds
 * a pathological gap (a long blocking load, a stopped debugger) so we ship one
 * bounded burst and re-anchor rather than allocating a multi-second pull that
 * would arrive as a wall of stale audio anyway. */
#define MAX_TICK_FRAMES (SYNCSCUMM_MIXER_RATE / 2)   /* 500ms */

SyncscummMixerManager *g_syncscumm_mixer;

SyncscummMixerManager::SyncscummMixerManager()
	: _startMs(0), _framesPulled(0), _buf(0), _bufFrames(0) {
}

SyncscummMixerManager::~SyncscummMixerManager() {
	free(_buf);
	_buf = 0;
}

void SyncscummMixerManager::init() {
	/* MixerImpl(uint sampleRate, bool stereo = true, uint outBufSize = 0) --
	 * same shape NullMixerManager::init() uses. Stereo: ScummVM mixes
	 * genuine stereo and we keep both channels. */
	_mixer = new Audio::MixerImpl(SYNCSCUMM_MIXER_RATE, true, 4096);
	assert(_mixer);
	_mixer->setReady(true);
	_bufFrames = MAX_TICK_FRAMES;
	_buf = (int16 *)calloc(_bufFrames * 2, sizeof(int16));   /* *2: stereo */
	_startMs = g_system->getMillis();
	_framesPulled = 0;
	g_syncscumm_mixer = this;
}

void SyncscummMixerManager::tick() {
	if (_audioSuspended || _mixer == 0 || _buf == 0)
		return;

	uint32 elapsed = g_system->getMillis() - _startMs;
	uint64 due = (uint64)elapsed * SYNCSCUMM_MIXER_RATE / 1000;
	if (due <= _framesPulled)
		return;

	uint64 frames = due - _framesPulled;
	if (frames > _bufFrames) {
		/* A gap this large means we were not running; shipping every missed
		 * frame would just queue stale audio behind real time. Ship one
		 * bounded burst and re-anchor the clock to now. */
		frames = _bufFrames;
		_framesPulled = due - frames;
	}

	/* mixCallback()'s `len` is a BYTE count, not a frame count: mixer.cpp
	 * does memset(buf, 0, len) and only then len >>= 2 for stereo, with an
	 * assert(len % 4 == 0). The vendored NullMixerManager passes a frame
	 * count here -- a latent upstream quirk this backend must not copy. */
	_mixer->mixCallback((byte *)_buf, (uint)(frames * 4));
	_framesPulled += frames;

	sst_io_audio_stream(_buf, (size_t)frames);
}

void SyncscummMixerManager::suspendAudio() {
	/* Stop pulling. Anything already queued in the terminal's FIFO plays
	 * out -- the cushion is by design, and a suspend is not a stop. */
	_audioSuspended = true;
}

int SyncscummMixerManager::resumeAudio() {
	if (!_audioSuspended)
		return -1;
	/* Re-anchor: real time advanced while suspended, but no PCM was
	 * produced for it. Without this, the first tick back would compute a
	 * huge frames_due and ship a burst of stale audio. */
	_startMs = g_system->getMillis();
	_framesPulled = 0;
	_audioSuspended = false;
	return 0;
}

bool SyncscummMixerManager::isNullDevice() const {
	return false;
}
```

- [ ] **Step 3: Swap it in**

`syncscumm.cpp` `initBackend()`:
```cpp
	_mixerManager = new SyncscummMixerManager();
	_mixerManager->init();
```
and `pollEvent()`:
```cpp
	((SyncscummMixerManager *)_mixerManager)->tick();
```
replacing the `NullMixerManager` cast/update. Add
`#include "audio_term.h"`, drop `backends/mixer/null/null-mixer.h`.

**Ordering:** `resolveSubtitles()` runs at the top of `initBackend()` and
calls `sst_io_audio_available()`, which blocks until the caps reply. That
must stay ahead of the mixer creation — it already is.

`module.mk`: add `audio_term.o` to `MODULE_OBJS`. The null-mixer line is
guarded `ifndef ENABLE_EVENTRECORDER` (upstream adds it there) — leave it.

- [ ] **Step 4: Tick from the present path**

`video_term.cpp`'s `updateScreen()` — the M2 lesson, verbatim in the
comment:

```cpp
	/* BASS's intro drives updateScreen() in a tight loop without ever
	 * pumping events, so a pollEvent-only tick would starve audio during
	 * the exact cutscene M4 exists for. tick() is cheap and internally
	 * rate-limited by the wall clock, so ticking from both sites is safe.
	 * Reached via the module's own pointer, not g_system: getMixerManager()
	 * is ModularBackend's, not OSystem's. */
	if (g_syncscumm_mixer != NULL)
		g_syncscumm_mixer->tick();
```
with `#include "audio_term.h"` added to `video_term.cpp`.

- [ ] **Step 5: Build + headless acceptance**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./build.sh 2>&1 | tail -5
./test/boot_bass.sh
./test/unit_sst_io.sh && ./test/subtitles.sh
```
Expected: BASS boots headless (~165 frames, the M1 acceptance figure);
headless is silent (no fd) and subtitles resolve on.

- [ ] **Step 6: Commit**

```
syncscumm: pull ScummVM's mixed audio and stream it

Replaces NullMixerManager with a real one. ScummVM sums every voice,
sound effect and synthesized music track into one interleaved stereo
stream -- the softsynths register on this same mixer -- so a single pull
covers every engine and every sound type. There is no useful per-sound
boundary to hook, and music arrives already rendered, so a separate MIDI
path would re-do work the engine just did.

The pull is driven by the wall clock, not the game loop: each tick takes
exactly the frames real time is owed. The game's pacing then cannot
starve or flood the stream, and audio cannot drift against video because
both ride one clock -- pacing off the game's own timer is what made the
syncconquer FMV audio run fast and fall behind.

Ticked from both pollEvent() and the present path, deliberately: BASS's
intro drives updateScreen() in a tight loop without pumping events, so a
pollEvent-only tick would starve audio during the exact cutscene this
milestone exists for.

The vendored NullMixerManager passes mixCallback() a frame count where it
wants a byte count. That is harmless when the pixels and samples are
discarded; it is not harmless here.
```

---

## Task 10: Sysop config + docs

**Files:**
- Modify: `src/doors/syncscumm/syncscumm.example.ini`
- Modify: `src/doors/syncscumm/DESIGN.md` (Audio path section)

- [ ] **Step 1: Document the keys**

Append to `syncscumm.example.ini`, matching its existing comment style:

```ini
[audio]
; Stream the game's sound to terminals that can play it (SyncTERM with
; libsndfile). Terminals that cannot are unaffected: they stay silent and
; turn subtitles on automatically. Default: true.
;enabled = true

; Opus encode quality, 0.01 to 1.0. Higher costs uplink bandwidth.
; Default: 0.15.
;quality = 0.15

; Channel volume, 0 to 100. Default 70 -- the stream is already mixed, and
; SyncTERM's mixer soft-clips a hot pre-mixed stream at 100. Raising this
; toward 100 risks distortion on loud scenes. To turn sound off, set
; enabled = false rather than volume = 0.
;volume = 70

; Milliseconds of audio per streamed chunk, 50 to 250. Default 100.
;chunk_ms = 100

; Chunks buffered before playback starts, 2 to 8. This is the latency
; budget AND the tolerance for a jittery link: 3 chunks at 100ms is about
; 300ms of each. Raise it on a link that stutters. Default: 3.
;prebuffer = 3
```

- [ ] **Step 2: Wire them — documented-but-ignored keys are a lie**

The ini text above promises behavior, so the keys must be read. Add to
`sst_io.c` beside `audio_stream_open()`, using the same xpdev
`iniReadFile()` pattern `resolveSubtitles()` already uses on
`syncscumm.ini` (relative to CWD, which is the door's `startup_dir`):

```c
/* Sysop audio settings. Same file and lookup as resolveSubtitles() in
 * syncscumm.cpp -- one syncscumm.ini, read from the door's startup_dir.
 * Missing file or missing key leaves every default in place; the module
 * clamps whatever it is handed, so a nonsense value cannot break a
 * session. Defaults are spelled in audio_stream_open(), not here, so
 * there is exactly one place to read them off. */
static void audio_read_ini(termgfx_stream_cfg_t *cfg)
{
	FILE      *f = fopen("syncscumm.ini", "r");
	str_list_t ini;

	if (f == NULL)
		return;
	ini = iniReadFile(f);
	fclose(f);
	cfg->enabled   = iniGetBool(ini, "audio", "enabled", cfg->enabled);
	cfg->quality   = iniGetFloat(ini, "audio", "quality", cfg->quality);
	cfg->volume    = iniGetInteger(ini, "audio", "volume", cfg->volume);
	cfg->chunk_ms  = iniGetInteger(ini, "audio", "chunk_ms", cfg->chunk_ms);
	cfg->prebuffer = iniGetInteger(ini, "audio", "prebuffer", cfg->prebuffer);
	strListFree(&ini);
}
```
called from `audio_stream_open()` after the defaults are set and before
`termgfx_stream_create()`. `sst_io.c` will need `ini_file.h` — note
`syncscumm.cpp:17-30` documents that xpdev's `ini_file.h` must be included
**before** `common/scummsys.h` poisons `strupr`/`printf`; `sst_io.c` is
plain C and includes no scummsys, so it has no such ordering problem, but
say so rather than leaving the next reader to wonder.

`enabled = false` must produce **zero** audio bytes: `cfg.enabled` already
gates the module (`termgfx_stream_create()` honors it), but assert it —
add a case to `test_sst_io_audio_tone.c`'s pattern in a tmpdir with an
ini, mirroring `unit_sst_io.sh`'s existing `sixelmax_override` block
(`mktemp -d`, `trap 'rm -rf' EXIT`, `printf` the ini, run in a subshell
`cd`).

- [ ] **Step 3: Update DESIGN.md**

Replace the "Audio path" section's M4-is-future wording with what shipped,
and point at `docs/superpowers/specs/2026-07-16-syncscumm-m4-audio-design.md`
plus its amendment.

- [ ] **Step 4: Verify + commit**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./build.sh 2>&1 | tail -3
./test/unit_sst_io.sh && ./test/subtitles.sh
```
Expected: all pass, including the new `enabled = false` zero-bytes case.

```
syncscumm: let the sysop tune the audio stream

Documents and reads the [audio] keys: enabled, quality, volume, chunk_ms
and prebuffer, from the same syncscumm.ini the subtitles precedence
already uses. The module clamps whatever it is handed, so a nonsense
value cannot break a session.

prebuffer is the one worth a sysop's attention: it is both the latency
budget and the tolerance for a jittery link, so a stuttering connection
wants it raised rather than the quality lowered.

Turning sound off is enabled = false, not volume = 0 -- SyncTERM clamps a
channel volume of 0 to -60dB and bakes it into the channel's entry
volume. enabled = false emits nothing at all, which is what a sysop who
turns sound off actually wants: no uplink cost, not a silent stream.
```

---

## Task 11: Live acceptance (user-driven)

**Files:** none. The milestone's acceptance bar, set by the user.

- [ ] **Step 1: Build and confirm the door is live**

```bash
cd /home/rswindell/sbbs/src/doors/syncscumm && ./build.sh 2>&1 | tail -3
```
The live binary is a symlink to the build output, so this **is** the
deploy. A live door session pins the old bytes — the user must exit any
running session first.

- [ ] **Step 2: Ask the user to run the acceptance pass**

Report each result verbatim; do not summarize a failure as a success.

1. **Beneath a Steel Sky on SyncTERM** — the comic intro's voice and
   music play, in sync with the picture, and the fades stay smooth. (The
   pacing interplay is the listen-for risk: M2's fades needed a palette-
   storm deadline, and audio now shares the uplink.)
2. **Flight of the Amazon Queen**, **Lure of the Temptress**, **Drascula**
   — a sound pass on each.
3. **Foot and xterm regression** — unchanged: subtitles on, no audio, and
   the intro still plays with Esc-to-skip working.
4. **Syncretro** — still sounds correct (Task 4 re-confirmation after the
   full batch).

- [ ] **Step 3: Diagnose from the trace if anything is off**

The trace gate is a touch file (`<SBBSDATA>/syncscumm/trace`) or
`SYNCSCUMM_TRACE=<path>`; **not an env var through the BBS** — `bbs.exec`
uses `execvp` with no shell and eats stderr, so a door launched from the
BBS can only be gated by the touch file. Each line now carries
`audio=<tier>/<chunks>/<underruns>/<dropped>`; `sst_trace_in()` already
dumps inbound replies, so the caps negotiation is visible with no extra
work. Underruns point at the cushion; drops at the backlog cap.

- [ ] **Step 4: Update the memory files**

Update `project_syncscumm.md` (M4 status, what shipped, traps found) and
add any new reference-worthy trap. Note the shared-module extraction in
whatever memory covers the termgfx library plan.

---

## Deferred / explicitly out of scope

- Any audio for Foot/xterm (no transport exists).
- In-door volume/mute hotkeys (SyncTERM's client-side volume serves).
  Note the extracted module carries syncretro's volume/mute/pause/reset
  entry points, which have **no call sites in either door** — they are
  untested-by-use.
- Automatic intro skipping on silent terminals (user chose: keep playing,
  rely on Esc).
- Sourcing floppy-version intro text for BASS's CD comic (the CD release
  has no text data for it — the developers replaced it with voice, so that
  sequence stays uncaptioned on silent terminals; not a swap-in).
- M3 (mouse input) stays gated on the quantizer perf fixes.
- Per-user save-path wiring (tracked separately; live play currently
  writes saves into the vendored `scummvm/` dir via the CWD fallback).
- Harmonizing syncconquer's `termgfx_audio_stream_chunk()` FMV path onto
  the new shared module. It works; leave it. Worth revisiting only if a
  third streaming consumer appears.
