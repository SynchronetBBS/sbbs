# Sixel palette subsetting -- measurement & validation results

Task 8 of the sixel-palette-subsetting plan
(`docs/superpowers/plans/2026-07-22-sixel-palette-subsetting.md`). This log
covers the two pieces that are fully automatable headlessly -- the shared
encoder's SyncTERM byte-identity proof (Step 1) and the cross-door README
sweep (Step 4) -- plus a pending template for the live per-title measurement
pass (Steps 2-3) that needs real ROMs/terminals and is out of scope here.

## Part A -- SyncTERM byte-identity proof (Step 1)

**Claim under test:** the live `termgfx/sixel.c` output for
`SIXEL_PAL_FULL`/`SIXEL_PAL_NONE` is byte-for-byte identical to the
pre-change encoder's `emit_palette=1`/`emit_palette=0` output, so doors left
on FULL/NONE (and SyncTERM, which never sees `USED`) are untouched by the
subsetting change (commit `6512285c1b`, "termgfx sixel: add SIXEL_PAL_USED to
emit only referenced registers").

### Setup

Pre-change source pulled from the parent of the commit that introduced
`SIXEL_PAL_USED`:

```
git show 6512285c1b~1:src/doors/termgfx/sixel.c > .../scratchpad/sixel_old.c
git show 6512285c1b~1:src/doors/termgfx/sixel.h > .../scratchpad/sixel_old.h
```

The old header's `sixel_encode()`/`sixel_encode_aspect()` signatures
(`int emit_palette`) are unchanged by the new tri-state, so a single harness
source compiles against either header unmodified. Two isolated build
directories were built so the two `sixel_encode` symbols (same name, two
translation units) never collide in one binary:

```
scratchpad/proveA/old/sixel.c   -- copy of the pre-change encoder
scratchpad/proveA/old/sixel.h   -- copy of the pre-change header
scratchpad/proveA/new/sixel.c   -- copy of the live encoder (post-6512285c1b)
scratchpad/proveA/new/sixel.h   -- copy of the live header
scratchpad/proveA/harness.c     -- shared harness (below)
```

Harness (`harness.c`): builds two deterministic indexed frames --
64x32 and a representative 320x200 -- with every one of the 256 palette
indices referenced somewhere in the frame (`idx[y*w+x] = (x*7 + y*13 +
x*y) & 0xFF`) and a synthetic 256-entry palette, then calls
`sixel_encode()` twice per frame: once with `emit_palette=1` (legacy
FULL / `SIXEL_PAL_FULL`) and once with `emit_palette=0` (legacy NONE /
`SIXEL_PAL_NONE`), writing each result to its own file. The literal `1`/`0`
values are used (not the new header's macros) so the exact same harness
source is valid whether it's compiled against the old header (no macros
defined) or the new one.

### Commands

```
cd scratchpad/proveA
gcc -Wall -O2 -I old harness.c old/sixel.c -o harness_old
gcc -Wall -O2 -I new harness.c new/sixel.c -o harness_new

mkdir -p out_old out_new
./harness_old out_old/out
./harness_new out_new/out

cmp out_old/out_64x32_full.bin   out_new/out_64x32_full.bin
cmp out_old/out_64x32_none.bin   out_new/out_64x32_none.bin
cmp out_old/out_320x200_full.bin out_new/out_320x200_full.bin
cmp out_old/out_320x200_none.bin out_new/out_320x200_none.bin
```

### Result

Both builds compiled clean (no warnings). All four `cmp` invocations
produced **no output** (byte-identical) and file sizes matched exactly
before comparing:

| Frame     | Mode (old emit_palette / new mode)   | Size (bytes) | `cmp` result |
|-----------|---------------------------------------|--------------|--------------|
| 64x32     | 1 / `SIXEL_PAL_FULL`                   | 17,472       | identical    |
| 64x32     | 0 / `SIXEL_PAL_NONE`                    | 13,812       | identical    |
| 320x200   | 1 / `SIXEL_PAL_FULL`                   | 339,283      | identical    |
| 320x200   | 0 / `SIXEL_PAL_NONE`                    | 335,623      | identical    |

Confirmed with a SHA-256 cross-check (old vs new hash equal for all four
files) as a second, independent proof beyond `cmp`'s silent exit status.

**Conclusion: PROVEN.** The `SIXEL_PAL_USED` addition does not alter a single
byte of the `FULL`/`NONE` output paths. SyncTERM (which only ever sees
`FULL`/`NONE` from every consuming door) and any door left on `FULL`/`NONE`
are unaffected by the subsetting change, exactly as the Scope guardrail in
the design doc requires.

## Part B -- cross-door README sweep (Step 4)

```
cd /home/rswindell/sbbs/src/doors
grep -n -i "palette.*once\|registers.*once\|sent once\|only.*SyncTERM\|SyncTERM.only\|all 256\|full palette\|dirty" \
     syncscumm/README.md syncconquer/README.md syncduke/README.md \
     syncdoom/README.md syncmoo1/README.md syncrpg/README.md
```

`syncconquer/README.md` does not exist (that door's docs are
`CLAUDE.md`/`COMPILING.md`/`DEFERRED.md`/`DESIGN.md`/`PROVENANCE.md`, no
top-level README) -- grep reported it missing; nothing to sweep there under
this task's file list.

| File : line | Matched text | Decision | Reason |
|---|---|---|---|
| `syncscumm/README.md:78` | "Frames are diffed to dirty rectangles and integer-scaled to the viewport" | **Left** | Generic frame-diffing claim, not a palette/register-count claim. Still true after the subsetting change -- SyncSCUMM still diffs frames to dirty rects; the change only affects *which registers* a dirty-rect image defines on register-resetting terminals, not whether/how dirtying happens. This is exactly the "still accurate, generic dirty-rect" example the sweep instructions call out to leave alone. |
| `syncdoom/README.md:54` | "It plays only on a SyncTERM that can decode audio files (silent otherwise)" | **Left** | False-positive match on `only.*SyncTERM` -- the sentence is about the JS lobby's `enter_sound` **audio** playback capability, unrelated to palette/sixel registers entirely. Not touched by this change. |

**Outcome: 0 READMEs changed.** No hit was a stale palette/register/dirty
claim; both were either already-accurate generic wording or an unrelated
audio-capability sentence caught by the pattern's breadth. No `git add` of
any door README is needed for this task's commit.

## PENDING -- live cross-title measurements (manual)

**Not performed here.** This section is a template only: Steps 2 and 3 of
the brief need real ROMs/games, a live SyncTERM session, and a live foot
session (a per-image, register-resetting terminal) -- none of which this
headless pass can drive. Fill in during the maintainer's live measurement
pass. The Task-7 live foot no-black-strips smoke test (SyncRetro
band-alignment, noted as not-performed in
`.superpowers/sdd/sixel-subset/task-7-report.md`) belongs to this same
manual pass and should be run alongside these captures.

### Cross-section (titles by characteristic -- pin exact ROMs/games available on this host)

- **SyncSCUMM** -- a fixed-palette talkie (e.g. BASS), a low-motion dialogue
  scene, and a high-motion / palette-cycling scene (the "comic storm"
  full-frame path).
- **SyncRetro** -- a clean low-color 2D game (EXACT mode, dirty-friendly), a
  scrolling platformer (motion + occasional new colors), and a
  NTSC/dither-filtered or >256-color core (forces CUBE + the
  `SR_DIRTY_FULL_PCT` diff-density fallback -- confirm it degrades
  gracefully to cheaper full frames, not worse).
- **SyncConquer** -- RA and TD (map scroll + sidebar + scattered unit
  motion).
- **SyncDuke / SyncDOOM** -- a demo/timedemo (full-frame-only path; confirm
  the non-SyncTERM full-frame palette shrinks with no visual change).
- **SyncMOO1** -- a mid-game galaxy/combat screen (full-frame-only, fixed
  palette; same confirmation as SyncDuke/SyncDOOM).

### Metrics to capture (before vs after, per title, per terminal)

Enable each door's opt-in trace facility (`sst_trace`, `sr_trace` +
`sr_trace_wire`) and the Ctrl-S overlay, drive the **same** input sequence
(demo/timedemo/recorded-input where available, else a fixed scripted input)
on both **SyncTERM** and **foot** (add xterm / Windows Terminal for extra
per-image-class coverage if time allows), and record:

| Title | Terminal | Build | bytes/frame mean | bytes/frame p95 | bytes/frame max | KB/s (steady-state) | full-vs-dirty frame ratio | dirty-rects/frame | fps | DSR pace latency proxy |
|---|---|---|---|---|---|---|---|---|---|---|
| _(fill in)_ | SyncTERM | before | | | | | | | | |
| _(fill in)_ | SyncTERM | after | | | | | | | | |
| _(fill in)_ | foot | before | | | | | | | | |
| _(fill in)_ | foot | after | | | | | | | | |

_(repeat the four-row block per title in the cross-section above)_

### Acceptance gate (apply per title, per terminal)

For each (title, terminal) pair:

- [ ] bytes/frame (mean **and** tail/p95/max) -- no worse than baseline
      within noise; **improves** on the optimized (non-SyncTERM) path.
- [ ] KB/s -- no worse than baseline within noise; improves on the
      optimized path.
- [ ] Latency (DSR pace proxy, where exposed) -- no worse than baseline;
      improves on the optimized path.
- [ ] fps -- holds or rises.
- [ ] dirty-frame share -- holds or rises (does not regress toward more
      full frames).
- [ ] Spot-checked frames show identical colors/geometry to baseline.
- [ ] On foot specifically: no partial-band black strips (SyncRetro
      band-alignment work) -- this is the Task-7 live smoke test, run here.

Any metric regressing halts and must be root-caused before Task 8 (and the
overall plan) is considered done.
