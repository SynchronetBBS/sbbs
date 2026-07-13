# termgfx music: Opus transition plan

Status: **DONE (unconditional switch).** Executed 2026-07-01.
**Update 2026-07-12: capability gate added** — see below.

## Update 2026-07-12: Opus-support gate (the "accepted risk" closed)

The 2026-07-01 switch shipped with a documented accepted risk: a client whose
libsndfile lacks Opus (e.g. Ubuntu 22.04's libsndfile 1.0.31) got **silent**
music *and* a wasted upload, because the coarse feature-100 probe can't tell
Opus support apart from bare libsndfile presence. Phase 2 of the original plan
below flagged this as needing a SyncTERM-side change ("Coordinate with Deuce").

Deuce delivered it: SyncTERM commit `eaf14c525b` adds a
`Q;libsndfileFormat;<major>;<subtype>` query (feature **101**) that reports
whether the client's libsndfile decodes a specific container/subtype — for us
`32;100` (`SF_FORMAT_OGG>>16 ; SF_FORMAT_OPUS`), reply
`ESC[=7;101;32;100;{0,1}n`.

termgfx now uses it (a **simple gate**, per sysop direction — NOT the Vorbis
fallback of the original phase 3):

- `audio.c`: `termgfx_audio_opus_query` + `termgfx_audio_parse_opus()`.
- `audio_mgr.c`: a per-manager `opus_ok` (-1 unknown / 0 no / 1 yes). When the
  digital tier is confirmed, the Opus query goes out alongside the C;L query;
  the reply is parsed from the inbound stream (with a rolling window for a split
  feed). When `opus_ok == 0`, the three music entry points
  (`_music_play` / `_music` / `_music_async_submit`) suppress the track — no
  render, no upload — and mark it current so the door doesn't retry. SFX (raw
  WAV) are unaffected.
- **No regression:** `opus_ok == 1` behaves exactly as before, and a client that
  never answers feature-101 (older SyncTERM) stays `opus_ok == -1` = send, i.e.
  the current behavior. Only a client that *explicitly* reports no-Opus is gated.
- Still **silent** music for those clients (the accepted-risk stance stands);
  the gate just stops wasting the undecodable upload. A Vorbis fallback (working
  music instead of silence) remains the deliberately-deferred phase-3 option.

## What was actually done (2026-07-01)

Per sysop direction, the switch was made **unconditional** — NOT the
capability-gated coexistence this doc originally proposed. Rationale: all
SyncTERM builds that support the audio-APC file mechanism at all are taken to
support Opus decode, so no probe/negotiation (phase 2) and no per-codec cache
key are needed. Vorbis is dropped, not kept as a fallback.

- `audio.c`: encode format `SF_FORMAT_OGG | SF_FORMAT_VORBIS` → `... | SF_FORMAT_OPUS`.
- `audio_mgr.c`: per-manager `music_quality` (default **`TERMGFX_MUSIC_QUALITY_DEFAULT`
  = 0.15**) replaces the old compile-time `MUS_OGG_Q`; new setter
  `termgfx_audio_set_music_quality()`. **`MUS_RENDER_VER` stays 1** — see below.
- `syncduke_stubs.c`, `i_termmusic.c`: async-submit render rate `44100` → **48000**.
- Sysop-configurable quality: `syncXXX.ini [audio] music_quality` (0.0..1.0), read in
  each door's config path (`syncduke_config.c`, `syncdoom.c: read_syncdoom_ini`) and
  applied to the manager at init. Documented in both `*.example.ini`.
- Container/extension unchanged (`.ogg`; Ogg-Opus is still an Ogg file), so
  `xp_sndfile.c` decodes it with no change (it sniffs codec from content).

**No cache-version bump for the codec change (design correction).** An earlier
pass bumped `MUS_RENDER_VER` 1→2 to force fresh Opus everywhere, but that is
wrong: the cache key is **content identity**, and a client's existing Vorbis
file is the *same musical content* — it plays fine, so forcing every client to
discard it and re-transfer Opus is pure waste. `MUS_RENDER_VER` is reserved for
changes to the rendered PCM (bank/normalisation/render fixes) that make an old
file sound *wrong*; codec and quality are transport details and are deliberately
NOT in the key. Adopting Opus for *fresh* transfers, without disturbing working
caches, is done operationally instead: **clear only the door-side disk cache**
(`data/sync*/audio/*`) once. Then `termgfx_audio_music_play()`'s STATE 1
(client already has `<key>.ogg`) keeps existing users on their Vorbis with zero
re-transfer, while cold clients render/receive Opus. `music_quality` therefore
only affects the bytes a *fresh* client receives. The just-made experimental v2
files were removed from the door-side cache so fresh renders use the final
settings; existing clients keep playing their Vorbis (STATE 1) untouched.

**Measured finding + quality choice.** libsndfile maps the Opus
`SFC_SET_VBR_ENCODING_QUALITY` knob **~linearly to bitrate** (q0.06≈40, q0.15≈88,
q0.2≈114, q0.3≈164 kbps) and does NOT do Vorbis's content-adaptive VBR collapse.
On synthetic tones a swap at the old q0.3 looked ~4× *larger*; but on real OPL/FM
tracks Vorbis@q0.3 actually ran ~85–113 kbps, so Opus at a lower quality is a
genuine size win. Measured real door-side tracks: Vorbis ~100 kbps vs Opus@q0.06
~43 kbps (~2.3× smaller). At q0.06 the sysop reported the low end sounded a touch
thinner than Vorbis; the default was therefore set to **q0.15 (~88 kbps)** — Opus
at ~88 kbps out-resolves Vorbis-at-100 kbps while still landing ~20% smaller than
the old Vorbis files. Sysops can override per door via `[audio] music_quality`.

**Accepted risk (from the gate this doc originally flagged):** a client whose
libsndfile lacks Opus (e.g. Ubuntu 22.04 LTS's 1.0.31) gets **silent music**
(SFX still play — they're raw WAV). The door-side WAV fallback keys off the
*door's* libsndfile, not the client's, so it does not rescue such a client.

Below is the original planning analysis, retained for context.

---

Original draft 2026-07-01.

The music tier currently encodes rendered game music (OPL3 FM via libADLMIDI)
to **Ogg/Vorbis** through libsndfile, ships it over SyncTERM's `C;S` upload, and
caches it name-keyed on the player's disk. This note captures what it would take
to move to **Opus** (still in an Ogg container) and why it is gated on the
receiving end, not on our encoder.

Baseline as of commit `89052800b9` (size-4-kids): libsndfile links on MSVC via
vcpkg, so Ogg/Vorbis is live on Windows too. The `opus` codec lib is already
pulled in (vcpkg installed `opus:*-windows-static-md` transitively with
libsndfile), so nothing new needs vendoring to *encode* Opus.

## Why consider Opus

- Better compression than Vorbis at music bitrates: roughly **20–40% smaller at
  matched perceptual quality**. Vorbis already did the hard 10–15× cut over raw
  PCM, so this is an incremental win on an already-solved problem, not a
  transformation.
- The savings % is essentially **independent of sample rate** — compressed size
  is bitrate (bits/sec) × duration; the 48 kHz requirement (below) does not
  change the Opus-vs-Vorbis ratio. Confirm the real number for *our* content by
  A/B encoding a few Duke/Doom tracks before committing to the work.

## The gate: player-side decode support (NOT our encoder)

SyncTERM's decoder (`src/syncterm/xp_sndfile.c`) is **format-agnostic**: it
`sf_open`s the file, reads native S16 via `sf_readf_short`, and resamples to
stereo 44100 for playback. So it will play whatever the player's **libsndfile**
can decode. The gate is entirely:

> Does the player's libsndfile have the Opus codec compiled in?

- **Opus support landed in libsndfile 1.1.0 (2022)**, behind the `opus`
  external-lib. Vorbis has been in since **1.0.18 (2009)** — effectively
  universal.
- Deuce's bundled Windows SyncTERM (3rdp libsndfile 1.2.2 + opus) → fine.
- System-libsndfile SyncTERM builds are a mixed bag: e.g. **Ubuntu 22.04 LTS
  ships libsndfile 1.0.31 — no Opus at all**; Debian bookworm (1.2.0) has it. A
  meaningful slice of the installed base would get *silence* instead of
  degraded-but-working music if we switched the wire format unconditionally.

**Conclusion:** do NOT switch the default wholesale. Keep Vorbis as the
lowest-common-denominator and adopt Opus **capability-gated** — only when the
player advertises it.

### The negotiation gap

The current audio probe (`termgfx_audio_probe`) gets back
`ESC[=7;100;{0,1}n` — the `{0,1}` is a **boolean** "libsndfile present /
digital-SFX tier," not a codec list. So today the door cannot tell whether a
given player can decode Opus. Closing this needs a **SyncTERM-side change** so
the reply advertises Opus-decode capability (the `100` looks like a level field
that could be bumped, or add a distinct capability token). Coordinate with
Deuce.

## Container & rate facts (settled)

- **Container is unchanged.** Through libsndfile, Opus is only ever Ogg-framed:
  `SF_FORMAT_OGG | SF_FORMAT_OPUS` (RFC 7845 Ogg-Opus, `.opus`). Same Ogg paging
  as `SF_FORMAT_OGG | SF_FORMAT_VORBIS`; only the codec payload changes. No raw
  Opus / Matroska path in libsndfile.
- **Opus is inherently 48 kHz.** Render the *Opus* path at **48000**, not 44100:
  libADLMIDI already resamples from the OPL3 native 49716 Hz to whatever rate you
  request, so 48000 costs the *same* single resample as 44100 does today — and
  it lets libsndfile's Opus encoder take its native rate instead of resampling
  again internally.
- **SyncTERM playback is fixed at 44.1 kHz** (`XPBEEP_TARGET_RATE`), BUT the
  decoder resamples *any* source rate → 44100 (`resample_to_s16_stereo_44100`),
  so a 48 kHz Ogg-Opus (or Ogg-Vorbis) plays correctly — no wrong-pitch. The
  48k→44.1k resample on the client is unavoidable for Opus anyway (48k native
  vs 44.1k device).
- **Keep the Vorbis path at 44100.** Vorbis@44.1k → SyncTERM 44100→44100 no-op.
  Bumping Vorbis to 48k would only add a pointless client-side downsample.

## Cache invalidation

Music cache is **name-presence** (no MD5): a client holding `<track>_v1.ogg`
reports a hit and the door skips the upload; the door-side disk cache is
`<dir>/<key>.ogg`. The render-version tag lives in one place —
`audio_mgr.c: #define MUS_RENDER_VER` (baked into the key by `mus_key()` as
`<track>_v<n>`).

- A codec change alters the produced bytes, so it **must** bump
  `MUS_RENDER_VER` (1→2) or stale Vorbis-era files get served by name. The
  `.ogg` extension is shared by both codecs (Ogg-Opus is still an Ogg file), so
  the version tag — not the extension — is what disambiguates.
- **Capability-gated caveat:** if both codecs coexist across clients within one
  door build, a single global `_v2` can't distinguish the two payloads for the
  same track (the door-side disk cache is one file per key). That path needs the
  codec folded into the key (e.g. `_v2o` / `_v2v`, or use `.opus` vs `.ogg`).

## Non-issues

- **Encode time:** negligible. One-shot per track per render-version, then
  cached (door disk + client C;L). Opus at high complexity can be somewhat
  slower than Vorbis@q0.3, but it's a sub-second-to-few-seconds one-time cost and
  the complexity is tunable — not a decision factor.
- **Vendoring to encode:** none. opus is already present via vcpkg with
  libsndfile. The MinGW 3rdp libsndfile/opus stack remains SyncTERM-only (won't
  link into the MSVC doors — see the vcpkg commit).

## Proposed phasing

0. **Measure.** A/B encode a handful of representative Duke/Doom tracks to
   Vorbis@44.1k and Opus@48k; compare byte sizes and listen. Decide if the real
   savings justify the work. (No product change.)
1. **Encoder capability in termgfx.** Generalize the encode path to take a codec
   selector — e.g. `termgfx_audio_encode(codec, ...)` alongside the existing
   `_encode_ogg` — with Vorbis as the default. 48 kHz render only on the Opus
   branch. No behavior change yet (nothing selects Opus).
2. **SyncTERM capability advertisement (Deuce).** Extend the audio probe reply so
   the door can detect Opus-decode support.
3. **Capability-gated selection in the door.** Pick Opus when advertised, else
   Vorbis; codec-tagged cache keys (or `.opus`/`.ogg`); bump `MUS_RENDER_VER`.
   Preserve the raw-PCM WAV fallback for players with no libsndfile at all.

## Files in scope

- `termgfx/audio.c`, `audio.h` — codec-aware encode (`SF_FORMAT_OPUS` branch).
- `termgfx/audio_mgr.c` — `MUS_RENDER_VER` bump; `mus_key()` codec tag; per-codec
  render rate (48k for Opus); `.ogg`/`.opus` leaf.
- `syncduke/syncduke_stubs.c`, `syncdoom/i_termmusic.c` — the `termgfx_midi_render`
  / `termgfx_audio_music` rate literals (currently `44100`).
- `syncterm/xp_sndfile.c` — already format- and rate-agnostic; no change needed
  to *decode*.
- SyncTERM probe/caps (Deuce) — advertise Opus-decode (phase 2).
