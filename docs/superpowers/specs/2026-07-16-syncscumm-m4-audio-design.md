# SyncSCUMM M4 — Audio for SyncTERM (design)

Date: 2026-07-16
Status: approved by user (design dialogue in session "syncscumm")
Predecessors: `2026-07-14-syncscumm-design.md` (door design),
M2 plan `../plans/2026-07-15-syncscumm-m2.md` (terminal video)

## Goal

Play ScummVM's audio — speech, sound effects, and music — on SyncTERM
terminals, streamed over the existing door connection. Terminals without
audio support (Foot, xterm, anything that never answers the audio
capability probe) remain exactly as they are today: silent, with
subtitles auto-enabled.

Acceptance bar (user-set): Beneath a Steel Sky (CD, voice-acted) as the
automated/headless target, **plus** a live sound pass on all the other
bundled freeware titles — Flight of the Amazon Queen, Lure of the
Temptress, and Drascula — before the milestone counts as done.

## Approach (user-approved: "Approach A")

Stream ScummVM's single mixed PCM output (the syncretro pattern), not
per-sound interception (syncduke pattern) and not a hybrid with a
separate music path. Rationale: ScummVM's engines stream decoded speech
and synthesize music (AdLib/MT-32 emulators) in-process; everything is
already summed into one mixer whose backend callback yields interleaved
signed-16-bit stereo PCM. One pull covers every engine and every sound
type; there is no useful per-sound boundary to hook, and music already
arrives as PCM so a separate MIDI path would re-implement work ScummVM
just did.

Key upstream facts this design relies on (verified in the vendored tree
and termgfx sources during design):

- `Audio::MixerImpl::mixCallback(byte *samples, uint len)` — **len is a
  BYTE count** (`audio/mixer.cpp:342` asserts `len % 4 == 0` for
  stereo). The M2 `NullMixerManager` passes a frame count here (a
  latent upstream quirk); the real backend must pass `frames * 4`.
- All softsynth music registers on the same mixer via `playStream()`
  (`audio/chip.cpp:145`, `audio/softsynth/mt32.cpp:344`), so the mixed
  pull includes it.
- `termgfx_audio_stream_chunk(m, pcm, bytes, bits, channels, rate,
  vol)` (`audio_mgr.h:180`) is an existing streaming entry point:
  Opus-encodes (via libsndfile) and ships `A;LoadBlob` + `A;Queue`
  inline when CTerm >= 1.329, else falls back to a rotating 32-name
  cache Store+Load. **This design originally specified it as M4's
  transport; that was corrected before planning — see the amendment
  below.**
- `A;Queue` appends to a channel FIFO, so consecutive chunks play
  gaplessly; `A;Update` arms a one-shot idle notification (CSI
  `=7;<ch>;0 n`) when the FIFO drains — underrun detection without
  polling.
- `termgfx_audio_tier(m)`: 1 = digital audio available, 0 = tone-only,
  -1 = none/unknown. Every manager entry point is a no-op below 1, so
  a non-audio terminal costs zero branches in door code.
- Opus-legal rates are {8000,12000,16000,24000,48000}; termgfx
  upsamples other rates on encode (`termgfx_audio_pick_opus_rate`).
- SyncTERM's channel mixer soft-clips (tanh knee); an already-mixed
  hot stream at 100% distorts. Proven fix (syncconquer FMV, commit
  d528495036): run the channel at ~70 (~ -3 dB).

## Amendment 2026-07-16: transport is a shared termgfx module

Fact-checking this design against the code before planning found that its
two transport requirements contradict each other, and the correction is
large enough to record here rather than bury in a plan.

**What was wrong.** The design specified shipping chunks through
`termgfx_audio_stream_chunk()` *and* arming `A;Update` for underrun
detection. `stream_chunk()` (`audio_mgr.c:1035`) cannot do the latter: it
emits only LoadBlob/Store+Load plus `A;Queue`, never `A;Update`; it
hardcodes the manager-internal `MUSIC_CH`; and nothing in `audio_mgr`
parses the `=7` idle reply. It also has no cushion state machine and no
backlog accounting — so three rows of this design's own error-handling
table (underrun, sustained saturation, cushion rebuild) were
unimplementable through it.

The root confusion: the design correctly cites syncretro for the
*approach* (pull one mixed PCM stream) but then borrowed syncconquer's
FMV *transport*. Syncretro's real transport is `syncretro_audio.c` — a
door-owned channel driven by the low-level `audio.h` builders, with a
PRIME/RUN cushion state machine, `A;Update` re-armed after every queue
(`syncretro_audio.c:72,103`), two-strike backlog dropping, cached-silence
dedup, a blob path, and mute. It parses the `=7` reply in its own input
layer (`syncretro_input.c:889`). That module already satisfies every
requirement here; `stream_chunk()` satisfies about half.

**Decision (user, "Approach 2").** Extract syncretro's streaming module
into libtermgfx as a shared component and have BOTH doors consume it,
rather than porting a second ~400-line copy into syncscumm. This is the
second consumer — the point at which extraction pays — and it matches the
standing "belongs in termgfx" call already recorded for
`sst_quant`/`syncretro_quant`.

**Consequences for the rest of this document:**

- Wherever this design says `termgfx_audio_stream_chunk()`, read "the
  extracted termgfx streaming module".
- The module must be **stereo-capable**: syncretro downmixes to mono
  (its chunk accumulator keeps only the left sample of each frame, which
  is lossless for its cores); ScummVM's mixer is stereo and must stay so.
- Syncretro's behavior must not regress. It is live and working, so the
  extraction is a refactor with syncretro's existing tests as the
  guard, landing BEFORE any syncscumm audio code.
- The door-specific seams syncretro's module reaches through (its output
  writer, backlog query, flush, and audio config) become explicit
  injected parameters of the shared module.
- `A;Update`/`=7` parsing is part of the extracted module's contract; the
  door feeds it inbound bytes (or the parsed channel), as syncscumm's
  session layer already does for the other probe replies.

## Architecture

### New component: `door/audio_term.{h,cpp}` — SyncscummMixerManager

Replaces `NullMixerManager` in `OSystem_Synchronet::initBackend()`.
Created unconditionally (engines need a mixer object regardless of
terminal capability); whether pulled PCM is streamed or discarded is the
session layer's decision.

- Owns an `Audio::MixerImpl` at **22050 Hz, stereo** (retro-game audio
  has no content above ~11 kHz; half the PCM handling of 48 kHz;
  termgfx upsamples to an Opus-legal rate at encode time).
- **Wall-clock-driven pull.** Each tick computes samples due from real
  elapsed time (`frames_due = elapsed_ms * 22050 / 1000 -
  frames_pulled`) and pulls exactly that many via `mixCallback(buf,
  frames * 4)`. The game's own pacing can neither starve nor flood the
  stream, and A/V can't drift (the same single-wall-clock lesson as
  syncconquer's `vqaaudio_termgfx.cpp:11-24`).
- **Ticked from BOTH `pollEvent()` and the present path.** M2 lesson:
  BASS's intro drives `updateScreen()` in a tight loop without pumping
  events; a pollEvent-only tick starves audio during the exact
  cutscene M4 exists for. The tick is cheap and internally
  rate-limited by the wall clock, so calling it from two sites is
  safe.
- Pulled PCM goes to the session layer through a new pure-C API
  (below); the manager knows nothing about terminals.

### Session layer additions: `sst_io.c`

- **Probes join the startup burst**: `termgfx_audio_probe` (libsndfile
  digital-vs-tone) and the Ogg-Opus format query, alongside the
  existing DA1/CTDA/XTVERSION/JXL probes. Inbound bytes already flow
  through `sst_io_pump()`'s parser; they are additionally fed to
  `termgfx_audio_feed()` so termgfx resolves the capability replies
  itself. (Note the reply ordering precedent from the ceiling work:
  outbound probe order determines reply order; audio probes have no
  ordering hazard because nothing gates on their absence, but they are
  sent after XTVERSION to keep the identification-first invariant.)
- **`sst_io_audio_stream(pcm, frames)`** — accumulates ~100 ms chunks
  (2205 frames) and hands each to `termgfx_audio_stream_chunk()` at
  channel volume **70**. Startup ships a 2-3 chunk cushion before
  steady state (those chunks ARE the latency budget, ~200-300 ms).
- **`sst_io_audio_available()`** — stops returning a hardcoded 0;
  returns `termgfx_audio_tier() >= 1`. Because `resolveSubtitles()`
  runs in `initBackend()` possibly before the probe reply lands, it
  gets a short bounded settle wait (same pattern as the existing
  JXL-reply settle window) — worst case a few hundred ms, once, at
  boot. Foot/xterm never answer, the wait expires, subtitles auto-on:
  today's behavior, unchanged.
- **Backlog cap**: if unshipped audio exceeds ~500 ms (the link truly
  can't carry audio + video), whole chunks are dropped — audio stays
  roughly synced to the picture rather than lagging ever further. Same
  philosophy as the video path's frame dropping; self-recovering.
- **Underrun handling**: `A;Update` armed after each queue; on the
  idle notification, rebuild the cushion immediately.
- **Shutdown**: `termgfx_audio_stream_stop()` (`A;Flush`) so no queued
  tail plays after exit. An Esc-skipped cutscene just drains the
  ~200-300 ms cushion naturally.
- **Trace diagnostics** (existing touch-file gate) gain counters:
  chunks shipped / dropped / underruns, so a live report is
  diagnosable from a trace replay.

### Subtitle interaction (already shipped in M2, now live)

The user > sysop > auto precedence is untouched. What changes is that
`auto` finally has a real signal: audio available → subtitles off;
silent terminal → subtitles on.

### Non-audio terminals & the intro (user decision)

The intro/comic keeps playing on Foot/xterm — no automatic skipping.
Esc-to-skip must work cleanly (verify, don't build). Known and
accepted: Beneath a Steel Sky's CD release has **no text data for the
comic intro** (the developers replaced floppy-era text with voice), so
that sequence stays uncaptioned on silent terminals; the other three
freeware titles carry text for essentially everything.

## Error handling

Guiding rule: audio is best-effort and must never stall or kill the
game.

| Condition | Behavior |
|---|---|
| Terminal never answers audio probe | tier stays -1; every audio call no-ops; subtitles auto-on |
| termgfx built without libsndfile | `termgfx_audio_have_ogg()` = 0; same as above |
| Chunk encode/alloc failure | drop the chunk, count it in trace, continue |
| Sustained link saturation | >~500 ms backlog → drop whole chunks, stay near-synced |
| Channel FIFO underrun | `A;Update` notify → rebuild cushion at once |
| Door exit | `A;Flush` the stream channel |

## Testing

1. **Canned-reply unit test** — new binary `test_sst_io_audio.c` in
   `unit_sst_io.sh` (fresh-session-per-binary convention): assert the
   startup burst contains the audio probes; canned digital-audio
   replies → `sst_io_audio_available()` == 1 and streamed PCM produces
   `A;LoadBlob`/`A;Queue` APC bytes; a silent session → available == 0
   and the same PCM produces no audio bytes.
2. **`subtitles.sh` extension** — auto now flips both ways with the
   audio caps reply; user/sysop overrides still win.
3. **Live acceptance** — Beneath a Steel Sky on SyncTERM: comic intro
   voice + music, in sync, fades still smooth (pacing interplay is the
   listen-for risk); then a sound pass on Amazon Queen, Lure of the
   Temptress, and Drascula; and a regression glance at Foot/xterm
   (unchanged: subtitles on, zero audio APC bytes on the wire).

## Out of scope for M4

- Any audio for Foot/xterm (no transport exists).
- In-door user volume control (SyncTERM's client-side volume serves).
- Automatic intro skipping on silent terminals (user chose option (b):
  keep playing, rely on Esc).
- Sourcing floppy-version intro text for the CD comic (not a swap-in).
- M3 (mouse input) remains gated on the quantizer perf fixes; the
  per-user save-path wiring is tracked separately.
