# termio extraction — golden baseline manifest (Task 2)

Captured against `src/doors/syncscumm` at commit `106ff50c3a` ("termgfx: add
termgfx_termio.h, the shared terminal-I/O API") — the state right after Task 1
added the (unused) shared header but before any code moves into it. This is
the "before" snapshot Task 6's refactor must reproduce byte-for-byte.

Large binaries (PPM frame set, wire capture) are **not** committed — they live
in scratch, which survives the session but is gitignored:
`.superpowers/sdd/termio-baseline/` (repo-relative). Only this manifest +
sha256s are committed, so Task 6 has an exact, reproducible recipe plus a
checksum to diff against once it regenerates the same captures.

## 1. Build

```
cd src/doors/syncscumm
./build.sh
```

Result: **clean build**, no errors/warnings of note in the log. Output
binary: `src/doors/syncscumm/build/syncscumm`
(sha256 `02a446c90184fc94608001bcd4848258ba4baa9c79d4fed91c9873c51adb143b`,
18157032 bytes).

## 2. Boot suite — `test/boot_*.sh`

Every script was run individually (`test/boot_queen.sh` needs >2 min wall
clock for its talkie+floppy passes, so it was run standalone rather than in
a 2-minute-capped loop). All 9 passed:

| Script | Result |
|---|---|
| `test/boot_bass_pkg.sh` | `BOOT-BASS-PKG OK (165 frames)` |
| `test/boot_bass.sh` | `BOOT-BASS OK (165 frames)` |
| `test/boot_betrayedalliance.sh` | `BOOT-BETRAYEDALLIANCE OK (500 frames)` |
| `test/boot_cascadequest.sh` | `BOOT-CASCADEQUEST OK (500 frames)` |
| `test/boot_drascula.sh` | `BOOT-DRASCULA OK (detected drascula:drascula)` |
| `test/boot_lure.sh` | `BOOT-LURE OK (440 frames)` |
| `test/boot_queen.sh` | `BOOT-QUEEN OK (talkie, 197 frames)` + `BOOT-QUEEN OK (floppy, 197 frames)` |
| `test/boot_sixel.sh` | `BOOT-SIXEL OK (3317760 bytes)` |
| `test/boot_spacequest0.sh` | `BOOT-SPACEQUEST0 OK (500 frames)` |

**9/9 green.**

## 3. Unit binaries — `test/unit_sst_io.sh`, `test/unit_sst_quant.sh`

- `test/unit_sst_quant.sh` — **PASS** (`SST_QUANT OK (256 colors)`).
- `test/unit_sst_io.sh` — **fails to link**, at its very first `cc`
  invocation (`test_sst_io.c` + `door/sst_io.c` + the two static libs), with
  `undefined reference to 'sst_plat_write'` / `sst_plat_now_ms` /
  `sst_plat_net_init` / `sst_plat_isatty` / `sst_plat_sock_setup` /
  `sst_plat_file_exists` / `sst_plat_getpid` / `sst_plat_read` /
  `sst_plat_sleep_ms`. Because the script has `set -e`, it aborts there —
  **none** of the ~20 `test_sst_io_*` / `test_sst_mouse` / `test_sst_input*` /
  `test_sst_datadir` drivers it lists ever run.

  **Root cause (pre-existing, unrelated to Task 1):** `door/sst_plat.c` was
  split out of `sst_io.c` as its own translation unit in commit
  `745c1c542b` ("syncscumm: add the Win32/MSVC build (create_project +
  vcpkg)"). `door/module.mk` was updated to compile `sst_plat.o` into the
  real ScummVM/door build, but `test/unit_sst_io.sh`'s 20 standalone `cc`
  lines (each just `test_sst_io.c`/`test_sst_mouse.c`/... + `sst_io.c` + the
  two static libs) were never updated to also compile `sst_plat.c`. The
  script itself was last touched by commit `02b58d3a51`, which predates the
  `sst_plat.c` split — confirmed via `git log` on both files. Task 1 (commit
  `106ff50c3a`, header-only) touched neither file and is not implicated.

  **Diagnostic-only verification (no repo file modified):** a scratch copy
  of the script, with `"$DOOR/door/sst_plat.c"` inserted into all 20 `cc`
  lines, was placed temporarily inside `test/` (so its own `$0`-relative
  path resolution worked), run, then deleted (`git status` confirms the
  working tree is clean). With that one object added, **all 20 drivers
  compiled and passed**: `SST_IO OK`, `SST_IO_NOGFX OK`,
  `SST_IO_CANVAS OK (Ph=1200)`, `SST_IO_PRESENT_PENDING OK`, three
  `SST_IO_GFXMAX ... OK` lines, `SST_IO_XTERM_CEILING OK`, two
  `SST_IO_BOTTOM_DIRTY ... OK` lines, `SST_IO_AUDIO OK`,
  `SST_IO_AUDIO_TONE OK`, `SST_IO_AUDIO_UNDERRUN OK`,
  `SST_IO_AUDIO_BACKLOG OK`, `SST_IO_AUDIO_STATIC OK`,
  `SST_IO_SIXELMAX_OVERRIDE OK`, `SST_IO_AUDIO_INI_OFF OK`,
  `SST_IO_AUDIO_INI_TUNE OK (20 chunks queued)`,
  `SST_IO_AUDIO_INI_HEADROOM OK (decoded peak 16778 at headroom = 50)`,
  `SST_DATADIR OK` (`test_sst_mouse`/`test_sst_input`/`test_sst_input_evdev`
  are silent-on-success; the script's `set -e` reaching the final
  `SST_DATADIR OK` line proves all three passed too).

  **Conclusion:** the underlying `sst_io.c` unit-test *logic* is green at
  HEAD; only the test script's build recipe is stale (missing one object on
  its link lines). This is a genuine pre-existing defect in
  `test/unit_sst_io.sh`, reported here per instructions rather than fixed
  (no test/source files were modified for this task — verified clean via
  `git status`). **Recommend a follow-up task add `"$DOOR/door/sst_plat.c"`
  to `test/unit_sst_io.sh`'s 20 `cc` invocations.**

## 4. Golden render baseline (PRIMARY) — PPM frames

Scenario: BASS boot, `SYNCSCUMM_DUMP`, mirroring `test/boot_bass.sh` exactly
(game copy without `sky.cpt`, `SYNCSCUMM_DATA` search-set override,
`timeout 20`):

```
cd src/doors/syncscumm
DOOR=$PWD
GAMECOPY=$(mktemp -d)/game; mkdir -p "$GAMECOPY"
cp test/games/bass/sky.dsk test/games/bass/sky.dnr "$GAMECOPY/"
SYNCSCUMM_DATA="$DOOR/scummvm/dists/engine-data" \
SYNCSCUMM_DUMP=<dump-dir> timeout 20 "$DOOR/build/syncscumm" \
  --path="$GAMECOPY" -c "$(mktemp)" sky
```

Run twice, independently, same binary. Both produced **165 `frameNNNNN.ppm`
files** (`P6 320 200 255` headers), and `diff -rq` between the two run
directories reported **zero differences** — byte-identical.

Saved at `.superpowers/sdd/termio-baseline/ppm/` (165 files, ~31 MB) with a
per-file + aggregate checksum manifest at
`.superpowers/sdd/termio-baseline/ppm-frames.sha256`:

- Aggregate sha256 (sha256sum of the sorted `sha256sum frame*.ppm` listing):
  `b2cbf878aa719d1023d3bc207fb3d100080e23f4f173cf3f7435af7e672e9ccb`

Boot log saved at `.superpowers/sdd/termio-baseline/boot.log` (the two runs'
logs differed only in the `mktemp`-generated scratch `-c` config path — not
a behavioral difference).

**This is the primary behavior gate for Task 6**, alongside the boot/unit
test results above.

## 5. Wire-bytes baseline (SUPPLEMENTARY) — reproducibility tested

Mechanism: `SYNCSCUMM_SIXELOUT=<path>` (mirroring `test/boot_sixel.sh`
exactly — BASS, `SYNCSCUMM_DATA` engine-data override, `timeout 20`, no game
copy needed since `--path` points at the fixture directly). Note this is
`sst_io.c`'s `g_capture` mode: `sst_io_present()` encodes every present()
call straight to sixel and `fwrite()`s it to the capture file with **no
pacing/backpressure/dedupe and no audio interleave** — a simplified,
video-only proxy for the real wire path, not the literal
`out_put()`/`sst_io_flush()` production bytes (those require a real
tty/socket peer to ever be written, per `sst_io_init()`'s control flow: the
`SYNCSCUMM_SIXELOUT` capture branch returns before the wire-byte
`g_tee`/wirecap block ever runs).

```
cd src/doors/syncscumm
SYNCSCUMM_DATA="$PWD/scummvm/dists/engine-data" \
SYNCSCUMM_SIXELOUT=<out.six> timeout 20 ./build/syncscumm \
  --path=test/games/bass -c "$(mktemp)" sky
```

Run twice, independently, same binary:

- Run 1: `wire_run1.six`, 3317760 bytes
- Run 2: `wire_run2.six`, 3317760 bytes
- `cmp` reported **no differences** — byte-identical.
- sha256 (both runs):
  `1b45d3ea3acd5a1fc7200cd391018ee2f487d48af923237805b2068e4c6b26c2`

**Result: deterministic.** Saved as
`.superpowers/sdd/termio-baseline/wire.six` (one canonical copy; the second
run's file was deleted after the diff, since it was byte-identical). This
is kept as an **additional** golden baseline per the task instructions, but
because it bypasses pacing/backpressure/audio-FIFO interleave, it is
secondary evidence — the PPM-frame diff plus the boot/unit test results
remain the authoritative behavior gate for Task 6.

## 6. File index (scratch, not committed)

```
.superpowers/sdd/termio-baseline/
├── ppm/                    165 frameNNNNN.ppm (run 1; run 2 verified
│                           byte-identical then discarded)
├── ppm-frames.sha256       per-file + reproducible via sha256sum -c
├── boot.log                SYNCSCUMM_DUMP run's stdout+stderr
├── wire.six                3317760-byte SYNCSCUMM_SIXELOUT capture
└── wire-boot.log           SYNCSCUMM_SIXELOUT run's stdout+stderr
```

## 7. Reproduction recipe for Task 6

1. `cd src/doors/syncscumm && ./build.sh`
2. `for t in test/boot_*.sh; do "$t"; done` (run `boot_queen.sh` standalone
   if wall-clock-limited) — expect the 9 `BOOT-* OK` lines in section 2.
3. Regenerate the PPM set with the exact command in section 4, `diff -rq`
   against `.superpowers/sdd/termio-baseline/ppm/` (or verify per-file with
   `sha256sum -c ppm-frames.sha256` from inside the new dump dir after
   renaming to match).
4. Regenerate the wire capture with the exact command in section 5, `cmp`
   against `.superpowers/sdd/termio-baseline/wire.six` /
   check its sha256 against section 5's value.
5. `test/unit_sst_io.sh` is expected to still fail to link, identically,
   until the pre-existing `sst_plat.c` link gap (section 3) is separately
   fixed — that failure is NOT something the termio extraction should be
   blamed for introducing or fixing incidentally.
