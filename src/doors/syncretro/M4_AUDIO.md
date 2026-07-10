# SyncRetro M4 -- streaming the core's audio to the terminal

Status: implemented 2026-07-09, and heard. Verified against FreeIntv playing
4-TRIS: 100 ms Opus chunks cycling the `s/0`..`s/3` cache ring, silent chunks
replaying a once-uploaded `s/z`, zero underruns and zero drops.
Scope: milestone M4 (see [DESIGN.md](DESIGN.md) §8 and §15).

M1 shipped video and discarded the core's PCM. This document specifies how that
PCM reaches the player, as a chunked Opus stream over SyncTERM's audio APC.

---

## 1. Goals / non-goals

Goals:

- The core's audio, as the core mixes it, playing in the terminal.
- Constant, bounded latency (~300 ms) that does not grow over a session.
- Silence costs almost nothing, and never introduces an attack delay on the
  first sound after a quiet passage.
- Degrade to silence -- never to garbage -- on a terminal that cannot decode.

Non-goals (deferred, §9): adaptive buffer depth, stereo, a mute hotkey, and
per-core audio profiles.

**Explicitly rejected: PSG register synthesis.** See §3. It is recorded here so
it is not re-litigated.

---

## 2. What we are given

**The core** (`FreeIntv`, at the pinned revision):

- `AUDIO_FREQUENCY` = 44100. `retro_run()` emits `44100/60 = 735` frames per
  video frame through `retro_audio_sample_batch`.
- The stream is **mono duplicated to stereo**: `libretro.c` ends with
  `Audio(c, c)`. Downmixing to mono is lossless and halves everything.
- The PSG is box-filtered from its native ~224 kHz down to 44.1 kHz (`c +=
  PSGBuffer[j++]; c = c / l`), and the **Intellivoice speech chip is mixed into
  the same stream** (`c = (c + ivoiceBuffer[...]) / 2`).

**The terminal** (SyncTERM's audio APC, `src/conio/cterm.adoc` §"Audio APCs"):

- 256 patch slots of S16 stereo 44100 PCM; 16 mixer channels, of which 2..15 are
  APC-usable and lazy-open at -12 dB.
- `Queue` **moves a slot's buffer onto a channel's FIFO** and empties the slot.
  Consecutive queues on one channel therefore play back to back: this FIFO is
  the live audio ring that §8 of DESIGN.md says termgfx lacks. It is not ours,
  it is the terminal's.
- `A;Update;C=n` arms a one-shot `CSI = 7 ; n ; 0 n` emitted when that channel
  transitions running -> stopped. That is an underrun signal, delivered without
  polling.
- `A;Load` decodes through **libsndfile**. A terminal without it silently
  ignores every Load. `termgfx_audio_parse_caps()` already reports this
  (tier 1 = libsndfile present).
- `C;S` writes the upload to `~/.cache/syncterm/<bbs>/<name>` with
  `fopen(fn, "wb")` -- **it overwrites by name**, and the file persists across
  sessions.

---

## 3. Why not PSG synthesis

The Intellivision's AY-3-8914 is memory-mapped at `$01F0-$01FF`, and FreeIntv
returns its whole 64K `Memory[]` from `retro_get_memory_data(SYSTEM_RAM)`. So a
frontend *can* poll the sound registers every frame and re-synthesize the
chiptune with `A;Synth` (`SQ` waveform, `T=<n>p` gives zero-crossing-aligned
looping) plus `A;Volume` ramps for envelopes. Near-zero bandwidth, near-zero
latency. It was seriously considered and rejected for two independent reasons:

1. **It is deaf to the Intellivoice.** The SP0256 speech chip leaves no trace in
   the PSG registers, so Space Spartans, B-17 Bomber, Bomb Squad and Tron: Solar
   Sailer would render mute where they should be talking. There is no recovering
   that from `$01F0-$01FF`.
2. **It does not generalize, and it rests on a core bug.** A survey of nine
   era-mate cores found *none* that expose their sound-chip registers through
   the public libretro ABI. Vectrex's AY sits behind a 6522 VIA in a private
   struct; ColecoVision's SN76489 is on Z80 I/O port space, which structurally
   cannot appear in a memory map; the NES's 2A03, the 2600's TIA and the
   Odyssey2's VDC all keep register state private. atari800 and prosystem export
   a full 64K buffer that *looks* like FreeIntv's, but their write dispatchers
   divert the hardware pages before storage (`MEMORY_writemap[0xd2] =
   POKEY_PutByte`), so those addresses are permanently stale. FreeIntv works
   only because it routes real memory-mapped I/O through the same flat array it
   exports -- of a piece with its declaring `unsigned int Memory[0x10000]`
   (4 bytes/slot) while `retro_get_memory_size()` reports `0x10000`.

Lesser losses, for completeness: the four envelope shapes (`A;Volume` ramps only
approximate them), the noise generator's timbre at its true period, the exact
16-entry logarithmic volume table, and any sub-frame register writes that 60 Hz
polling would flatten.

---

## 4. Measurements

All measured on this host, not estimated. Chunk figures are one 100 ms mono
chunk; "b64" is the on-wire size after the APC's base64.

**Ogg container overhead sets a floor on chunk size** (libopus, 64 kbit/s, sine):

| chunk | ogg | b64 | effective |
|---|---|---|---|
| 17 ms (one video frame) | 752 B | 1002 B | 354 kbit/s |
| 50 ms | 949 B | 1265 B | 152 kbit/s |
| 100 ms | 1470 B | 1960 B | 118 kbit/s |
| 250 ms | 2683 B | 3577 B | 86 kbit/s |
| 500 ms | 4965 B | 6620 B | 79 kbit/s |

There is a fixed ~650-byte header tax per chunk, so per-frame chunking is out:
the headers would outweigh the audio.

**Quality**, encoded through the same libsndfile path termgfx uses, on a **square
wave** (the PSG's actual output, and the coder's worst case):

| VBR quality | ogg | b64 | sustained (10 chunks/s) |
|---|---|---|---|
| 0.06 | 1219 B | 1625 B | 15.9 KB/s (130 kbit/s) |
| **0.15** | **1481 B** | **1974 B** | **19.3 KB/s (158 kbit/s)** |
| 0.30 | 2129 B | 2838 B | 27.7 KB/s (227 kbit/s) |
| 0.50 | 2882 B | 3842 B | 37.5 KB/s (307 kbit/s)  |

**The default is `TERMGFX_MUSIC_QUALITY_DEFAULT` (0.15)** -- the value SyncDuke
and SyncDOOM already use for music, and, independently, the value this
measurement lands on. Use the constant, not a literal. q=0.06 (~40 kbit/s)
audibly rings on square waves; q=0.30 costs 40% more bytes for material that is
mostly square waves anyway.

**Silence is nearly free:** a 100 ms all-zero chunk encodes to ~187 B (headers,
essentially) against 1481 B for a tone. That figure is from `libopus` via
`ffmpeg`; libsndfile's Opus writer may differ by tens of bytes, which changes
nothing about the decision.

**Encode cost is negligible:** linear resample 44100->48000 plus the libsndfile
Opus encode of one 100 ms mono chunk takes **0.37 ms mean, 0.35 ms best** (50
iterations, square wave) against a 16.7 ms frame budget. **So the encode runs
inline on the game thread.** `audio_mgr.c`'s worker-thread pattern
(`mus_wake` / `mus_job_pending` / xpdev `threadwrap`) is NOT needed and must not
be copied here; if a future core's audio makes this false, that is the seam to
reach for.

---

## 5. Architecture

One new translation unit, `syncretro_audio.c`, between `retro_bridge.c`'s
existing `sr_audio_feed()` and termgfx's audio builders. Nothing else changes
shape: the APC bytes go out through the same staged `sr_out_put()` as sixel.

- **`syncretro_audio.c` / `.h`** -- chunk accumulator, silence detection, the
  Opus encode call, the cache-name ring, the prebuffer and underrun state
  machine, and the drop counter.

Changed:

- **`retro_bridge.c`** -- `sr_audio_feed()` stops discarding: it downmixes to
  mono and hands the samples to the accumulator.
- **`syncretro_io.c`** -- the audio capability probe joins the existing enter/
  probe handshake; the `CSI = 7 ; ch ; 0 n` underrun report is routed to the
  audio module, alongside the existing `CSI = 7 ; 100 ; ...` libsndfile reply.
- **`syncretro_input.c`** -- the CSI `n` arm already handles `CSI = ... n`
  reports for the JXL cap; it gains the audio-state report.
- **`main.c`** -- flush the audio channel on the way out.
- **`syncretro_config.c`** -- read `syncretro.ini` (§5.1). This is the door's
  first INI; the file is optional and every key has a default, so a door dir
  without one behaves exactly as today.

### 5.1 Configuration -- `syncretro.ini`

Follows `../syncduke/syncduke_config.c` exactly: `fopen()` the INI from the
launch directory, `iniReadFile()`, `iniGet*()` with a default, clamp, free.
Absent file or absent key means the default. A command-line flag, if one is ever
added, takes precedence over the INI, as `-charset` does in SyncDuke.

```ini
[audio]
enabled  = true   ; false = emit no audio APCs at all (not volume 0)
quality  = 0.15   ; Opus VBR quality; outside 0.01 .. 1.0 falls back to the
                  ; default rather than clamping to the nearest bound -- an
                  ; out-of-range quality is a typo, and this also rejects the
                  ; NaN that atof("nan") would otherwise sail past a pair of
                  ; one-sided comparisons
                  ; default = TERMGFX_MUSIC_QUALITY_DEFAULT
volume   = 100    ; channel base volume 0..100; see the volume-0 note in sec 8
chunk_ms = 100    ; clamped 50..250; below 50 the Ogg headers dominate (sec 4)
prebuffer = 3     ; chunks held before playback; clamped 2..8 -> 200..800 ms
```

`quality`, `chunk_ms` and `prebuffer` are exposed because §4's measurements make
their trade-offs legible to a sysop, and because the fixed-depth choice in §6 is
the thing most likely to need tuning against a real link before the adaptive
version in §10 is worth building. Every value is clamped on read: a sysop must
not be able to configure the door into a state the measurements say is broken
(e.g. `chunk_ms = 17`, where headers cost 3x the audio).

**Fixed resources.** Channel **2** (first APC-usable). Slot **0**. Cache names
`s/0` .. `s/3` (a 4-entry ring) plus `s/z` for silence. The ring only needs to
outlive the in-flight `C;S` -> `A;Load` pair, and APCs are processed in stream
order, so 4 is generous; it exists so a name is never reused while its Load is
still upstream, and it bounds the player's on-disk cache at four small files
instead of one per chunk forever.

---

## 6. Data flow

1. `sr_audio_feed(pcm, frames)` -- take the left channel only (L == R by
   construction), append into the accumulator.
2. At **4410 mono frames (100 ms)** the chunk closes.
3. **Silent chunk** (every sample zero): emit `A;Load;S=0;s/z` + `A;Queue;C=2;S=0`.
   No `C;S`, no encode. `s/z` is uploaded once at startup.
4. **Otherwise:** `termgfx_audio_encode_ogg(pcm, 4410, 1, 44100, quality, &out)`
   -- it resamples 44100->48000 internally -- then `C;S` under `s/<ring>`, then
   `A;Load;S=0;s/<ring>`, then `A;Queue;C=2;S=0`.
5. **Prebuffer.** Hold the first three chunks (300 ms) and queue them together;
   only then does the stream run, queueing each new chunk as it closes. The
   cushion exists *only* because it was established up front: chunks are produced
   in real time and consumed in real time, so a FIFO fed one chunk at a time from
   empty stays one deep forever and underruns on the first jitter. Those three
   chunks are the entire latency budget and the entire jitter tolerance.
6. **Underrun.** `A;Update;C=2` is armed after every Queue. A
   `CSI = 7 ; 2 ; 0 n` means the FIFO drained. Count it, and **re-prime**: hold
   the next three chunks and queue them together, rather than dribbling single
   chunks into an empty FIFO, which would underrun again on the next jitter.

---

## 7. Backpressure -- drop at the input, never at the output

A chunk is dropped only under **sustained** congestion: when the door's staged
output backlog exceeds `SR_AUDIO_BACKLOG_BYTES` (48 KB) at **two consecutive
chunk boundaries** (~200 ms). The drop happens **at the input, before encoding**;
a chunk already queued to the terminal is never dropped.

The distinction matters. `syncdoom/i_termsound.c:80` records an *instantaneous*
socket-busy check being tried for SFX and reverted:

> "This burst dedup is the ONLY drop: an output-backpressure drop was tried here
> and removed -- it randomly ate one-time sounds during combat, since frames keep
> the socket busy most of the time; the channel voice-stealing in termgfx bounds
> audio latency instead."

The socket is busy with sixel essentially always, so socket depth is not a signal
about audio. SyncDOOM's *other* mechanism -- collapsing an identical SFX
re-triggered within 40 ms -- has no analogue here: a PCM stream has no discrete
triggers to coalesce.

The prebuffer hold list is bounded by `prebuffer` (clamped 2..8). It is the only
place chunks queue door-side, and it drains in a single burst when playback
starts.

The core produces 44100 samples/sec against the wall clock whether or not we can
ship them. If we fall behind, the only two options are to let latency grow
without bound, or to discard old samples and stay aligned with the picture. We
discard: a stall costs one audible 100 ms gap, not permanent drift.

Audio is small next to video (≈2 KB per 100 ms against ≈8-9 KB per sixel frame),
so it is not expected to starve the frame path; the drop counter is how we would
learn otherwise.

---

## 8. Error handling

- **No audio APC / no libsndfile** (`termgfx_audio_tier() < 1`): the module is
  disabled at probe time and emits **zero** audio bytes. Not a degraded stream --
  silence. A Load that the terminal ignores would leave a Queue playing an empty
  slot.
- **`termgfx_audio_have_ogg()` == 0** (door built without libsndfile): same --
  disabled. We do not fall back to raw WAV; at ~940 kbit/s after base64 it would
  fight the frame path for no benefit.
- **Encode failure** (returns 0): drop that chunk, bump the counter, continue.
  One 100 ms gap is always preferable to a stalled game loop.
- **Underrun**: re-prime (§6.6). Never treated as fatal.
- **Pause / help / reset**: pause stops calling `retro_run()`, so no PCM is
  produced and the FIFO drains -- which fires the underrun report. The module
  must treat "no audio produced" as an expected quiescent state and re-prime on
  resume rather than logging a spurious underrun. `Ctrl-R` flushes the channel.
- **Teardown**: `A;Flush;C=2;O=50` (50 ms fade) before the terminal restore, on
  every exit path including `sr_door_hangup()`. A dropped carrier needs no flush
  -- the terminal is gone -- but the call must be harmless in that case.
- **Volume 0**: SyncTERM clamps a volume of 0 to -60 dB, and a channel's entry
  volume bakes that level in -- an as-designed behavior that cost SyncDuke a
  debugging session. `[audio] enabled = false` disables the module; "silent"
  must not be spelled as `volume = 0`.

---

## 9. Testing

- **`test_audio.c`** (new unit test): the chunk accumulator. A 735-frame batch
  does not close a 4410-frame chunk; six of them close one with 0 remainder;
  batches that straddle a boundary carry the remainder correctly; an all-zero
  chunk is detected as silent and one non-zero sample anywhere defeats that.
- **`fakecore.c`**: emit a known square wave through `audio_batch`, so the door
  has something deterministic to encode. Gate on an env var so existing scenarios
  stay silent.
- **`fakterm.py`** scenarios:
  - a terminal that never answers the audio probe receives **zero** `A;` APCs;
  - a tier-1 terminal receives `C;S` -> `A;Load` -> `A;Queue` in that order;
  - the cache name cycles `s/0..s/3` and repeats;
  - a silent stretch emits `A;Load`+`A;Queue` and **no** `C;S`;
  - exactly three chunks are queued before the stream is declared running;
  - an injected `CSI = 7 ; 2 ; 0 n` triggers a re-prime, not a single queue;
  - `Ctrl-Q` emits `A;Flush;C=2` before the terminal restore;
  - a `syncretro.ini` with `[audio] enabled = false` emits **zero** audio APCs on
    a tier-1 terminal, and one with an out-of-range `chunk_ms = 17` is clamped to
    50 rather than honored.
- Uncrustify as the closing step, per this directory's CLAUDE.md.

**What cannot be tested here:** that it *sounds right*. The host is headless
(no sound device). Every automated check above verifies the byte stream, never
the audio. Live SyncTERM listening is the acceptance gate, and the first thing
to listen for is Intellivoice speech in Space Spartans -- the one thing the
rejected PSG design could never have produced.

---

## 10. Deferred, with reasons

- **Adaptive buffer depth.** `termgfx/pace.c` already does AIMD depth control
  against DSR-ACK for frames, and audio has the same shape (an invisible queue,
  an ack-ish signal, a cost to running shallow). But it would be a second control
  loop competing with the first for one socket. Build the fixed depth, learn from
  the drop and underrun counters, then decide.
- **Stereo.** The core emits mono duplicated. Nothing to carry until a core does.
- **A mute / volume hotkey.** The binding table has room; no demand yet.
- **Per-core audio profiles.** Rejected for M4 (§3), and the survey says there is
  nothing to generalize *to*. If M3 brings a core whose audio needs different
  handling, `syncretro_audio.c` is the seam.
