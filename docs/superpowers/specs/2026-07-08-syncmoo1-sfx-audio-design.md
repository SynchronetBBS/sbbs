# syncmoo1 SFX audio (M2, step 1)

Date: 2026-07-08
Status: approved, not yet implemented

Give the syncmoo1 door sound effects over SyncTERM's audio APC, by
implementing 1oom's `hw_audio_sfx_*` contract on top of termgfx's audio
manager. Music is explicitly out of scope (see Follow-on milestones).

## Background

`DESIGN.md` §11 defers audio to M2. Every `hw_audio_*` hook in `hw_sbbs.c`
is currently an empty stub returning success, which is why the engine's
`Preparing sounds, this may take a while...` completes in 0 ms.

Facts established by reading the sources, not assumed:

- **The engine hands us raw LBX items.** `hw_audio_sfx_init(i, data, len)`
  receives a Creative VOC straight out of `SOUNDFX.LBX` / `INTROSND.LBX`.
  Measured: 41 items / 345 KB / 11 kHz 8-bit mono / 31.6 s of audio, plus
  6 items / 98 KB in `INTROSND.LBX`.
- **We do not convert.** termgfx's `sfx_cache_file_once()`
  (`audio_mgr.c:468`) transcodes VOC to 8-bit WAV and ships anything else
  verbatim. `fmt_sfx_convert()` is therefore unused, which keeps us clear
  of the resample/gain staging that `termgfx/CLAUDE.md` warns about.
- **The SFX index space is global and non-overlapping.** `uiclassic.c:527`
  registers `0x00..0x28` (`NUM_SOUNDS` = 41) from `SOUNDFX.LBX` inside
  `ui_late_init()`. The intro registers three more from `INTROSND.LBX` at
  ids *above* that range: `uiintro.c:23-25` defines
  `SFX_ID_1/3/5 = NUM_SOUNDS + 0/1/2` (41/42/43). They do not collide.
- **`batch_start(max)` is a capacity hint, not a reset.** SDL's parallel
  variant allocates a table of `sfx_index_max` (`hwsdl_audio.c:433-438`);
  its serial variant does nothing (line 524). Neither discards previously
  registered sounds. `ui_late_init()` (`game.c:725`) runs before the intro,
  so a backend that treated `batch_start` as "clear" would silently drop
  every gameplay sound when the intro began.
- **termgfx names SFX by id, and Stores lazily.** `audio_mgr.c:447-449`
  builds the cache leaf as `snprintf(leaf, "%d", id)` giving
  `"<prefix>/sfx/<id>"`; the Store happens inside
  `termgfx_audio_sfx_file()` on first *play*, guarded per session by
  `sfx_up[id]`.
- **SyncTERM persists Stored files on disk, per BBS, keyed by name.**

Note what this does *not* imply: because the index ranges are disjoint,
id-keyed cache names would be correct today. Content-addressing is chosen
for three other reasons — it dedupes a sample present in both LBX files, it
does not depend on 1oom's index numbering staying stable across engine
updates, and it is the precondition for step 3's cross-session upload skip,
which is only sound when a name's presence guarantees its bytes.

- **The tier is unknown when the engine registers sounds.**
  `hw_audio_sfx_batch_start()` is called from `ui_late_init()` right after
  `hw_video_init()`, before the first present and therefore before the
  audio-capability probe is even sent. `tier == -1` at that moment, and
  every `tier < 1` guard (`audio_mgr.c:444,500,521`) drops the Store. A
  naive eager upload would upload nothing.

- **Music already solved naming.** termgfx's music API takes a
  caller-supplied name, and both doors content-address it with 32-bit
  FNV-1a: `snprintf(s->name, "d_%08x", term_hash(...))`
  (`syncdoom/i_termmusic.c:149`, and the same scheme in
  `syncduke/syncduke_stubs.c:331`). SFX were left id-keyed. This spec
  extends the existing scheme sideways to SFX rather than inventing one.

## Non-goals

- Music (`hw_audio_music_*` stays stubbed).
- Migrating SyncDOOM / SyncDuke to the new API.
- Widening termgfx's `C;L` cache-list skip to `sfx/*`.
- Any use of `fmt_sfx_convert()` / `fmt_mus_convert_xmid()`.

## Design

### termgfx additions (additive; existing callers unchanged)

```c
uint32_t termgfx_fnv1a(const void *data, size_t len);

void termgfx_audio_sfx_store(termgfx_audio_t *m, const char *leaf,
                             const void *filedata, size_t filelen);
void termgfx_audio_sfx_play_named(termgfx_audio_t *m, const char *leaf,
                                  int vol, int pan);
```

`termgfx_fnv1a()` gives the hash a single home; it is currently copied in
`syncdoom` (`term_hash`) and `syncduke`. Those copies are left alone here
— this only stops syncmoo1 becoming the third.

`_store` is the eager half that today's API cannot express: Store once, no
dispatch. `_play_named` dispatches by name. Both reuse the existing
`cache_name(m, "sfx", leaf, ...)` and `sfx_cache_file_once()`. The only new
state is per-name "already stored" bookkeeping for named callers, replacing
the `sfx_up[id]` array on that path; `termgfx_audio_sfx*(id, ...)` keep
their `sfx_up[]` behaviour byte-for-byte, so SyncDuke and SyncDOOM do not
move.

Leaf format: `s_<hash8>` (e.g. `s_1a2b3c4d`). The `s_` is a legibility tag
("sfx"), matching the convention the other doors use -- SyncDOOM's `d_<hash8>`
(`i_termmusic.c:149`) and SyncDuke's `<trackname>_<hash>`, whose comment states
the purpose plainly: "the hash keeps it stable across sessions and
collision-free across GRPs; the name just makes the cache files readable."

The prefix is NOT what keeps the sfx and music namespaces apart: `cache_name()`
(`audio_mgr.c:187`) already separates them by sub-directory, `<prefix>/sfx/<leaf>`
versus `<prefix>/music/<key>`. Nothing can alias across them.

The leaf must be under 16 bytes including its NUL (termgfx rejects longer ones
outright rather than truncating them into its fixed-width table). `s_%08x` is
10 characters, leaving room for the version tag described below.

### Door module

New `syncmoo1_audio.c` / `.h`, whose only job is 1oom's `hw_audio_sfx_*`
contract over termgfx. `hw_sbbs.c`'s stubs become thin forwarders, matching
how `hw_video_*` forwards to `syncmoo1_io.c`. No other door module learns
about audio.

State:

- `slot[SM_SFX_MAX]` — 1oom index -> leaf name registered at that index.
  Sized `NUM_SOUNDS + 3` (44) at minimum; the implementation should size it
  from termgfx's `KEYMAX` (1024) and range-check, rather than assume the
  intro keeps using exactly three ids above `NUM_SOUNDS`.
- a pending-store queue of `(leaf, bytes)` — samples not yet Stored.
- a set of leaves already Stored this session.
- the current SFX volume (0..128 as 1oom reports it).

### Data flow

1. `hw_audio_sfx_batch_start(max)` is a capacity hint: range-check `max`
   and return 0 (prepared synchronously — our work is a hash, not a
   decode). It must **not** clear `slot[]`: the engine calls it again for
   the intro's ids after `ui_late_init()` has already registered the
   gameplay set, and clearing would silence the game.
2. `hw_audio_sfx_init(i, voc, len)` computes `s_<fnv1a(voc,len)>`, sets
   `slot[i]`, and if that leaf is neither queued nor already Stored, copies
   the bytes into the queue. The copy is deliberate: 1oom frees the intro's
   items after use, and the whole set is ~440 KB.
3. `hw_audio_sfx_batch_end()` returns immediately.
4. `hw_event_handle()` (which already drives `sm_input_pump()`) drains the
   queue the first time `termgfx_audio_tier() >= 1` — one
   `termgfx_audio_sfx_store()` per distinct leaf. This lands ~50 ms in,
   inside the engine's own "Preparing sounds" window.
5. `hw_audio_sfx_play(i)` -> `termgfx_audio_sfx_play_named(m, slot[i],
   vol, 0)`. Unknown index or empty slot is a no-op.
6. `hw_audio_sfx_volume(v)` stores `v` (0..128); dispatch scales to
   termgfx's 0..100. 1oom sets a global SFX level, not a per-sound one.
7. `hw_audio_sfx_stop()` -> `termgfx_audio_sfx_stop_all()`.
8. `hw_audio_sfx_release(i)` clears `slot[i]` only. The client keeps the
   cached sample; that is the point.

`slot[]` persists across batches, matching the engine's global index space.
A sample present in both LBX files hashes to one leaf and uploads once. The
scheme needs no knowledge of 1oom's batch structure or id numbering.

### Wiring

`termgfx_audio_create(emit, ctx)` with an `emit` that forwards to
`sm_out_put()` (mirroring `syncduke_io.c:82`'s `sd_audio_emit`).
`termgfx_audio_set_cache_prefix(m, "moo1")` -> cache names `moo1/sfx/s_...`.
`termgfx_audio_probe()` at terminal enter; inbound bytes fed through
`termgfx_audio_feed()` from `sm_input_pump()`.

### Error handling

Everything degrades to silence, never to an error path:

- Tier stays -1 (Foot, xterm, anything without the audio APC): `slot[]`
  fills, the queue never drains, `play` no-ops, and **nothing is Stored,
  Loaded or Queued**. One inert APC goes out either way:
  `termgfx_audio_probe()` emits `ESC _ SyncTERM:Q;libsndfile ST`
  (`audio.c:79`) unconditionally, and a terminal that ignores APCs ignores
  it.
- A failed allocation in the queue drops that sample only; its `play`
  becomes a no-op.
- Unknown/out-of-range index: no-op (the engine already logs it —
  `uisound.c:19`).

## Testing

Unit (`tests/test_audio.c`, built like `test_map`: dependency-free sources,
and `-UNDEBUG` on the target so the asserts actually run):

- `termgfx_fnv1a()` against known FNV-1a 32-bit vectors.
- leaf formatting: `s_<hash8>`, lower-case hex, zero-padded.
- volume map: 0 -> 0, 128 -> 100, monotonic, clamped.
- `batch_start` does **not** clear `slot[]`: register a gameplay id, call
  `batch_start` again as the intro does, and assert the first id still
  resolves to its leaf. (This guards the exact bug the design review
  caught.)
- dedupe: identical bytes registered at two indices queue once and yield
  the same leaf.

Wire (drive the door against the fake terminal, assert on the
`syncmoo1_n<node>.wire` capture via `tools/wiredump.py`):

- zero `C;S` Store APCs before the tier reply arrives;
- exactly one Store per distinct sample hash, none duplicated;
- each play APC names the leaf its index was registered with, and ids
  registered before an intervening `batch_start` still play;
- a terminal answering DA1 only receives no Store/Load/Queue APC, and
  exactly one `Q;libsndfile` probe.

## Follow-on milestones (not this spec)

**Step 2 — content-address SyncDOOM's and SyncDuke's SFX names.** Both
currently use `<prefix>/sfx/<id>`. Beyond preparing step 3, this fixes a
latent bug they have today: within one session, `sfx_up[id]` means a
WAD/GRP swap (SyncDuke's add-on picker, SyncDOOM's wadset) keeps playing
the first content Stored under that id — verify before fixing. One commit
per door, checked with a wire capture.

**Step 3 — widen termgfx's `C;L` cache-list skip from `music/*` to
`sfx/*`, and version-tag the SFX leaf first.**

Two things must land together here, and the second is easy to miss. termgfx
does not ship our VOC verbatim: `sfx_store_bytes()` runs
`termgfx_audio_voc_to_pcm()` and Stores a transcoded 8-bit WAV. So `s_<hash>`
names the hash of the *source* bytes, not of the bytes actually cached. If the
transcode ever changes, the name does not, and the client keeps serving audio
rendered by the old code.

That is exactly the hazard music already solved: `mus_key()` appends a render
version, `<name>_v<MUS_RENDER_VER>` (`audio_mgr.c:747`), so the cache name
changes when the renderer does. Today the SFX exposure is harmless, because
every session re-Stores and the Store overwrites. It becomes real the instant
name-presence starts meaning "skip the upload". So step 3 must give the SFX
leaf its own transcode-version tag -- `s_<hash8>_v<n>`, 13 characters, within
the 16-byte leaf bound termgfx enforces -- before, or in the same change as,
widening the glob. Without it the skip serves audio rendered by a transcode
that no longer exists, forever, and no one notices.

The glob widening itself is only safe once **step 2** lands. The skip is
name-presence, so it is correct exactly when the name is content-addressed --
the argument `audio_mgr.c:658-660` already makes for music. Today SFX are
re-Stored every session: `freedoom1.wad` alone is 69 sounds / 1.37 MB, Duke is
larger (~450 ids), syncmoo1 ~440 KB. With id-keyed names a skip would serve the
wrong sounds after a WAD swap (`DOOM2.WAD`, `myhouse.wad` and `freedoom1.wad`
all sit in `xtrn/syncdoom/wads/`). Step 3 must not precede step 2.

So step 3 has two preconditions, not one: content-addressed names in the other
two doors (step 2), and a transcode-version tag on the SFX leaf (above).

Until step 3 lands, syncmoo1 re-uploads its ~440 KB once per session. That
is accepted: it is correct, it is an order of magnitude below the sixel
stream, and it lands during a pause the engine already takes.
