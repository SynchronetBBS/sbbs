# Working in termgfx (shared door graphics/audio library)

`termgfx` is the shared library the game doors (SyncDuke, SyncDOOM,
SyncConquer) link for sixel/JXL/text rendering, the SyncTERM APC image and
**audio** transport, cap-probing, and `sbbs_node`. A change here can affect
**every** consuming door — check the others before altering shared behavior,
and keep changes bit-identical for doors you're not intentionally touching.

## Our code — house style (uncrustify)

Everything at the top level of this directory (`*.{c,h}`, `CMakeLists.txt`,
the `*.md` docs) is our code. For C changes, follow
[../../uncrustify.cfg](../../uncrustify.cfg) — write to it from the start
(tabs, brace placement, spacing) and run it as the closing step:

```
uncrustify -c ../../uncrustify.cfg --replace --no-backup <file>...
```

## Vendored code — keep upstream intact

`libADLMIDI/` is a vendored snapshot (the OPL3 MIDI synth). **Don't run
uncrustify on it** and keep edits minimal/surgical — reformatting destroys
its diffability against upstream.

## Before touching audio — read the README's Audio section

The audio pipeline (`audio_mgr` over `audio`) has non-obvious gain staging and
a decode pitfall that each cost a full debugging pass. **Read the
"Audio (SyncTERM APC) — gain model & hard-won lessons" section of
[README.md](README.md) before changing volumes, the music path, or how a door
feeds decoded audio in.** The one-line versions:

- **`A;Volume` is absolute and replaces SyncTERM's −12 dB channel base** (not
  multiplied), and the mixer soft-clips the summed output — so music queued at
  100% sits ~12 dB above SFX and a single hot stream can distort by itself.
  Don't chase that with volume cuts; keep headroom.
- **A persistent-state codec (IMA-ADPCM/AUD) streamed through a chunked ring
  buffer desyncs at block boundaries and drifts to DC** — decode whole-file
  from one contiguous buffer instead (these doors accumulate the whole track
  anyway). See [../syncconquer/PROVENANCE.md](../syncconquer/PROVENANCE.md)
  patch 8 for the real case.
- **Isolate audio artifacts by layer** (source vs decode vs encode): check DC
  vs AC and zero-crossing rate, envelope the track in thirds for progressive
  drift, and get ground truth from an independent reference decoder
  (`ffmpeg -c:a adpcm_ima_ws`). Don't guess.
