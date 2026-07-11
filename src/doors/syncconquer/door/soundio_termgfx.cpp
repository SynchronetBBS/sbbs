// soundio_termgfx.cpp -- NEW_VIDEO_BUILD audio backend: implements the
// SoundImp_* seam (common/soundio_imp.h) that common/soundio_common.cpp (the
// vendored, unmodified game-facing audio.h implementation -- Play_Sample,
// File_Stream_Sample_Vol, and the Westwood-ADPCM/SOS AUD decode via
// Sample_Copy()/Audio_Unzap()) calls into. Mirrors soundio_openal.cpp's
// shape (its "contract"/reference, per the task brief) but hands already-
// engine-decoded PCM to termgfx's audio_mgr (door_io.c's g_audio, reached
// via door_io_audio()) instead of a real OpenAL device.
//
// Why this file does NOT touch vanilla/: every byte we receive here has
// already been decompressed by the engine's own AUD/ADPCM path (soundio_
// common.cpp's Sample_Copy()/Audio_Unzap()) -- reused as-is, per DESIGN.md.
// SoundImp_* is a small, symmetric seam (13 free functions); soundio_
// openal.cpp needs nothing from vanilla/ beyond it, and neither do we.
//
// *** HISTORICAL FINDING, NOW FIXED (read before changing this file's
// SFX/music split -- full writeup in task-4-report.md's Task 4/4b/4c
// sections): a File_Stream_Vol() stream (the ONLY caller in redalert/ is
// ThemeClass::Play_Song -- i.e. Theme/score music) used to deliver AT MOST
// its first ~8KB ring block's worth of decoded PCM through SoundImp_Buffer_
// Sample_Data() and then go permanently silent: Maintenance_Callback()'s
// per-tracker `MoreSource` flag latched false after that first block and
// was never reset, even though the ring kept refilling `QueueBuffer` behind
// it unused. That was a property of the VENDORED file, not this backend --
// soundio_openal.cpp saw the exact same truncation, it just wasn't as
// noticeable under real hardware playback timing. Fixed upstream-side in
// vanilla/common/soundio_common.cpp's Maintenance_Callback(): a short
// Sample_Copy() now re-arms `MoreSource` (gated on `IsScore`, so a stale
// recycled tracker can't re-arm for a plain SFX) when QueueBuffer/
// FilePending show more ring data is queued, instead of unconditionally
// latching it off (see that file's local-patches note in PROVENANCE.md).
//
// That alone raised the accumulated PCM per Theme but a SEPARATE bug still
// capped it around ~4.3s: SoundImp_Sample_Status() below has TWO
// independent pollers -- soundio_common.cpp's own Maintenance_Callback()
// (its internal reap check) and redalert/theme.cpp's ThemeClass::
// Still_Playing() (via the public Sample_Status()) -- and the original
// generation-counter implementation re-snapshotted its baseline on every
// single check, so whichever caller polled second in a given tick saw
// "nothing new" even though data genuinely arrived, because the first
// caller already consumed the edge. That raced the Theme into restarting
// roughly every 40ms, each restart re-decoding the file from byte 0 --
// which looked like a fixed ~4.3s ring/decode ceiling but was actually a
// polling race. Fixed by replacing the edge detector with an idempotent
// duration model (see SoundImp_Sample_Status()): every caller gets the
// same answer, no consumption, no race. Verified end to end (M2 Task 4c):
// THEME_INTRO now decodes and commits its full ~205s Track_Length (9.5MB+
// PCM), and with termgfx/audio.c up-sampling non-Opus-legal rates (Red
// Alert's AUD tracks are natively 22050 Hz) before encoding, the result
// lands in the door-side OGG disk cache instead of always hitting the
// raw-PCM WAV fallback.
//
// M2 Task 4d: the fixes above got the FULL track committed, but only ever
// at Stop_Sample()/Shutdown_Sample() -- so in a live session it sat fully
// decoded in the accumulator, uncommitted, for the Theme's entire ~205-
// 217s life: the user heard Start_Sample()'s initial sub-threshold
// stinger, then silence, until a theme change or door exit finally shipped
// it. SoundImp_Sample_Status() (already polled every tick) now also
// detects the stream going idle -- no new SoundImp_Buffer_Sample_Data()
// call for SA_IDLE_MS while the accumulator already holds more than
// SA_MUSIC_THRESHOLD_MS -- and commits right then, once per stream
// (`idle_committed`); see that function's own comment for the full design.
// The looping music upload (and warm-disk-cache / C;L-skip paths) now
// engage within a few seconds of theme start instead of at exit.
//
// M2 Task 4e: Task 4d's idle-commit ships only the ~7s partial snippet
// covered above -- the FULL track still only lands at Stop_Sample()/
// Shutdown_Sample() (theme change or door exit), so a session that never
// changes theme hears that ~7s loop for its entire life. Two additions,
// gated behind a per-stream `full_committed` latch so neither ever fires
// twice for the same Start_Sample()..Stop_Sample() stream:
//   (a) THEME FINGERPRINT: SoundImp_Buffer_Sample_Data() hashes (FNV-1a)
//       the first SA_FP_BYTES of a stream's post-Start_Sample PCM, once,
//       as soon as that many bytes have arrived -- a stable identity for
//       "which theme this is" that doesn't depend on ring-block chunking
//       (the SAME file bytes hash the same regardless of how many ticks
//       it took to deliver them).
//   (b) FULL-TRACK SWAP: a SECOND, longer idle gate (SA_FULL_IDLE_MS,
//       well above the SA_IDLE_MS prime gate and above the inter-burst
//       gaps Task 4d's own evidence measured) fires once the stream has
//       gone quiet for good and something has accumulated since the
//       prime -- ships the FULL accumulation as a new content-hash
//       track. termgfx_audio_music()/_music_play() already replace
//       (Flush+Load+Queue) whatever's on the music channel for a
//       different name (see audio_mgr.c's mus_ship()), so this swap
//       needs no explicit stop call. The generated name is then
//       persisted to a tiny flat file keyed by the fingerprint (sa_map_
//       write()/sa_map_read(), "<cache>/syncalert_theme_<fp>.map", one
//       line: the "d_<hash>" name, plus a duration field added by M2 Task
//       4g below) -- no databases, just content-addressed lookup, matching
//       the SFX/music id scheme already used throughout this file.
//   On a LATER session, the SAME prime check first looks up the just-
//   computed fingerprint in that map; a hit calls termgfx_audio_music_
//   play() with the remembered name, and a non-RENDER result (the
//   client's own cache or the door-side OGG disk cache still has it --
//   verified via the mgr's own API, not a direct file-presence check,
//   so this needs no access to audio_mgr's private cache-dir string)
//   means the FULL track is already playing -- the ~7s partial commit
//   is skipped entirely and the stream is marked full_committed right
//   away. A stale/missing mapping (RENDER) degrades gracefully to the
//   normal cold Task-4d/4e path (prime, then the later full swap
//   re-renders and re-persists). This file resolves its own copy of
//   the "audio cache dir" (SYNCALERT_AUDIO_CACHE env var, else "./
//   audio-cache" -- the same default door_io.c falls back to) purely so
//   ITS OWN map files land in the same place session to session; it
//   does not need to (and does not try to) match door_io.c's own
//   resolution byte-for-byte when a -audiocache CLI flag is used
//   instead of the env var, since the warm lookup above never touches
//   the filesystem directly -- only the mgr API's own return code.
//
// M2 Task 4f: two blocking fixes to the 4e design above.
//   Fix 1 (prime was destructive): the cold prime commit above used to call
//       the ordinary sa_commit(), which zeroes the accumulator -- so the
//       LATER full-track swap only ever saw bytes that arrived AFTER the
//       prime, missing the first ~7s of a real track (measured: 209.96s
//       shipped vs. the true 217.07s). Fixed with sa_commit_peek(): ships
//       the prime WITHOUT resetting the accumulator, so it stays anchored
//       at byte 0 and the full-track swap (also made non-destructive, see
//       Fix 2) genuinely covers the entire track.
//   Fix 2 (premature-swap poisoning): the SA_FULL_IDLE_MS gate is a
//       heuristic ("quiet for 5s" == "done"), and a long enough main-loop
//       stall (mission load, MP connect, slow disk) can satisfy it mid-
//       decode -- shipping (and, before this fix, permanently persisting)
//       a truncated "full" track. The swap's own commit is now ALSO non-
//       destructive (sa_commit_music_named() without resetting len), and
//       SoundImp_Buffer_Sample_Data() un-poisons full_committed the moment
//       more data arrives afterward -- proof the swap was premature, since
//       a genuinely finished stream never calls this again. The eventual
//       confirmed end of stream (Stop_Sample()/Shutdown_Sample(), or the
//       engine's own duration-reap Stop_Sample() call) then ships the TRUE
//       complete accumulation (byte 0 onward) under a fresh content hash
//       and REWRITES the fingerprint map via sa_commit_final() -- so a
//       stall can no longer permanently wedge a theme's warm-cache entry
//       to a truncation; it self-heals within the same session.
//   Minor 3 (warm-hit accumulation waste): after a WARM fingerprint-cache
//       hit (full track already playing from cache), a new `warm_hit` flag
//       makes SoundImp_Buffer_Sample_Data() skip sa_append() entirely --
//       this stream's own PCM will never be used, so there's no reason to
//       realloc/memcpy the ~9.5MB a full THEME_INTRO decode produces.
//       Deliberately distinct from the COLD full_committed case (Fix 2
//       needs THAT path to keep accumulating).
//   Minor 4 (Stop-path map loss): Stop_Sample() firing between the prime
//       and the full-track-swap gate used to ship the remainder (now, post
//       Fix 1, the FULL track so far) without ever writing the map --
//       silently losing the warm-cache benefit for the next session.
//       sa_commit_final() (Stop_Sample()/Shutdown_Sample()'s new shared
//       commit path) now writes the map there too whenever the shipped
//       accumulation cleared SA_MUSIC_THRESHOLD_MS -- the same helper also
//       carries Fix 2's rewrite-on-correction above.
//
// M2 Task 4g: two review-found regressions in Task 4f's minors above, both
// confined to this file.
//   Fix 1 (warm_hit froze the duration model): Minor 3's early return in
//       SoundImp_Buffer_Sample_Data() skipped recv_gen++/total_bytes+=/
//       last_recv_ms along with sa_append() -- so once a warm fingerprint-
//       cache hit engaged, the wall-clock "still playing" budget SoundImp_
//       Sample_Status() computes from those counters froze at whatever had
//       accumulated by the hit (~16s for THEME_INTRO's prime). Once real
//       wall time caught up to that frozen budget, Sample_Status() started
//       reporting false even though the cached track was still audibly
//       playing, Maintenance_Callback() reaped it as stopped, and ThemeClass
//       restarted the theme -- an audible stinger + full decode restart
//       every ~16s for the rest of EVERY warm session (the looping music
//       itself survived via the same-name warm re-hit each time, masking
//       the churn as "just a stutter" rather than a full restart cycle).
//       Fixed by still running the three counter updates in the warm_hit
//       branch; only sa_append() (the actually-wasted realloc/memcpy) is
//       skipped, exactly as Minor 3 intended.
//   Fix 2 (map write not gated on decode-quiet evidence): sa_commit_final()
//       wrote the fingerprint map on ANY Stop_Sample()/Shutdown_Sample()
//       past SA_MUSIC_THRESHOLD_MS -- including an early theme change (the
//       user backs out at t=10s of a track that takes minutes to fully
//       decode), which persisted that ~10s truncation as the theme's
//       permanent warm-cache entry. Worse, Fix 1's warm-hit accumulation
//       skip means the truncated file plays straight from the door-side OGG
//       disk cache on every later warm session with no further decode at
//       all -- Task 4f's self-heal (Buffer_Sample_Data()'s un-poison, which
//       needs MORE post-swap data to arrive in the SAME cold stream) never
//       gets a chance to run again. Fixed per the reviewer's direction: the
//       map write in sa_commit_final() now fires only when the stream has
//       been idle (no SoundImp_Buffer_Sample_Data() call) for at least
//       SA_FULL_IDLE_MS at the moment of the final commit -- a genuinely
//       finished decode (running roughly 5x real-time) is always quiet well
//       past that by the time Stop/Shutdown fires, while an early theme-
//       change Stop has a recent delivery and skips the write; the music
//       itself still ships unconditionally for the transient listen, only
//       the map persistence is withheld. Hardening also added per the
//       reviewer: the map line now carries the track's duration (ms)
//       alongside its name, and every write site (this one and the
//       full-track-swap gate in SoundImp_Sample_Status()) goes through the
//       new sa_map_write_if_longer(), which reads the existing entry first
//       and refuses to replace a longer, known-good entry with a shorter
//       one -- protects the "stall-then-quit" corner: a full, correct entry
//       recorded on one visit to a theme must survive a LATER, unrelated
//       short revisit (idle the whole time, so the quiet gate above is
//       satisfied) that would otherwise persist its own much-shorter
//       accumulation over it.
//
// This file's pragmatic v1 (matches the task brief's own "if true streaming
// can't map cleanly ... implement the pragmatic v1" guidance): commit
// whatever a tracker accumulated as a one-shot SFX (content-hash id, fire-
// and-forget). Streamed Themes instead reach termgfx's looping music path
// through the idle-prime / full-swap / final-commit gates above, each of
// which applies SA_MUSIC_THRESHOLD_MS itself.
//
// M2 LIVE-TEST FINDING 2: sa_commit() originally routed by duration ALONE
// (>= SA_MUSIC_THRESHOLD_MS -> music) as an escape hatch from the era when
// Stop_Sample() was the only commit point for a stream's accumulation. Once
// the stream gates above became the real music path, that hatch was pure
// misroute: every one-shot clip longer than ~1.5s -- EVA voice lines
// ("Establishing battlefield control..."), long SFX -- went through the
// LOOPING music channel instead of the SFX pool. Each such clip hard-
// flushed the playing theme (music kept cutting out), looped forever until
// the next long clip replaced it (the "voice repeats"), and re-set the
// shared music-channel volume to its own positional volume (the "volume
// goes up and down"). Live evidence: voice-length (1.5-2.8s) OGGs in the
// music-only disk cache. sa_commit() only ever ships PRE-Start_Sample
// accumulation -- by construction a one-shot clip's whole body (or a short
// sub-threshold stream remainder via sa_commit_final()'s fallback tail) --
// so it now routes SFX unconditionally.
//
// Identity for both paths is a content hash of the decoded PCM (FNV-1a),
// not any engine-side name/enum -- nothing on this seam exposes one, so a
// given clip's audio bytes ARE its identity (stable within one process).
//
// Panning: Play_Sample_Handle (soundio_common.cpp) accepts a `panloc`
// parameter but never forwards it to ANY SoundImp_* call -- confirmed true
// of soundio_openal.cpp too (grep finds zero uses of `panloc` outside the
// four Play_Sample*() signatures). This is a pre-existing upstream gap, not
// a regression: this backend has no way to do real stereo SFX positioning
// either, so every SFX plays centered (pan=0).
#include "soundio_imp.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   /* sa_now_ms() -- SoundImp_Sample_Status()'s duration model */
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>   /* sa_mkpath() -- M2 Task 4e theme fingerprint cache dir */
#endif

extern "C" {
#include "audio_mgr.h"   /* termgfx: audio_mgr.c is a plain-C translation unit */
}

#include "door_io.h"     /* door_io_audio() -- the manager door_io.c owns/creates */

// One accumulator per engine "voice" (Audio_Init() creates MAX_SAMPLE_
// TRACKERS=5 of these up front and reuses them for the life of the process).
struct SampleTrackerTypeImp {
	uint8_t *pcm;   // growable accumulated decoded PCM
	size_t len;
	size_t cap;
	int bits;           // 8 or 16 (SoundImp_Init_Sample/_Set_Sample_Attributes)
	int channels;       // 1 or 2
	int rate;           // Hz
	int vol_pct;        // last SoundImp_Set_Sample_Volume(), scaled to 0..100
	bool as_music;      // last commit went through the music (looping) path, not SFX
	uint32_t recv_gen;    // bumped on every SoundImp_Buffer_Sample_Data() call
	uint32_t status_gen;  // recv_gen snapshot taken ONCE at Start_Sample() -- see
	                      // SoundImp_Sample_Status()'s duration model below for why this
	                      // is no longer re-snapshotted on every check.
	uint64_t total_bytes; // cumulative bytes delivered since Start_Sample() (duration model)
	uint64_t start_ms;    // wall clock (sa_now_ms()) at Start_Sample() (duration model)
	uint64_t last_recv_ms; // wall clock of the most recent SoundImp_Buffer_Sample_Data()
	                       // call since Start_Sample() -- SoundImp_Sample_Status()'s
	                       // stream-idle detector (prompt music commit)
	bool idle_committed;     // stream-idle commit already fired for this Start_Sample()
	                         // ... Stop_Sample() span -- guards against re-committing on
	                         // every later poll while the stream stays idle
	// -- M2 Task 4e (theme fingerprint / full-track cache) --
	uint32_t fp;             // FNV-1a fingerprint of the first SA_FP_BYTES of this
	                         // stream's post-Start_Sample PCM ("which theme is this")
	bool fp_done;            // fp has been computed for the CURRENT stream
	bool full_committed;     // the full-track swap already fired (cold path) OR was
	                         // bypassed by a warm fingerprint-cache hit -- either way,
	                         // no more commits for this Start_Sample()..Stop_Sample()
	                         // stream (see sa_commit()'s own guard) UNLESS Buffer_
	                         // Sample_Data() un-poisons it again (M2 Task 4f Fix 2)
	bool warm_hit;           // M2 Task 4f Minor 3: a WARM fingerprint-cache hit fired
	                         // (full track already playing from cache) -- distinct from
	                         // full_committed: a cold full-track swap sets full_committed
	                         // WITHOUT this, and Fix 2 needs that path to keep
	                         // accumulating so a premature swap can be detected/undone.
	                         // Only a true warm_hit means this stream's own PCM is
	                         // provably never used, so Buffer_Sample_Data() can skip
	                         // accumulating it altogether.
};

// Minimum accumulated audio for a STREAM to route as looping music rather
// than a one-shot SFX. Applied only by the stream-side commit gates (the
// idle-prime/full-swap checks in SoundImp_Sample_Status() and sa_commit_
// final()) -- never to pre-Start_Sample one-shot accumulation, which always
// ships as SFX regardless of length (see sa_commit() / the file header's
// LIVE-TEST FINDING 2 note).
#define SA_MUSIC_THRESHOLD_MS 1500

// M2 LIVE-TEST FINDING 4: vanilla's File_Stream_Sample_Vol() now decodes
// SCORES whole-file (the ring-streamed path desynced the IMA-ADPCM decoder
// across ring-block boundaries -- predictor drift to DC, ~12 dB of music
// lost over a track), so a score's ENTIRE decoded PCM arrives before
// SoundImp_Start_Sample(). That is indistinguishable at this seam from a
// long one-shot SFX/voice purely by "arrived before Start", so split on
// DURATION: anything at least this long is a whole-file score (RA scores run
// 200-375s) and must ship as looping music + drive the duration model;
// anything shorter is an SFX/voice (all well under 10s) and ships one-shot.
// The gap between the two (10s..200s) is empty, so the threshold is safe.
#define SA_SCORE_THRESHOLD_MS 30000

// Stream-idle gap (see SoundImp_Sample_Status()'s idle-detect commit) --
// how long a music-routed stream must go without a new SoundImp_Buffer_
// Sample_Data() call before we treat the initial decode burst as "done for
// now" and ship it, rather than waiting for Stop_Sample()/Shutdown_Sample().
#define SA_IDLE_MS 500

// M2 Task 4e: a SECOND, much longer idle gap -- well above SA_IDLE_MS and
// above the inter-burst gaps Task 4d's own evidence measured -- meaning the
// stream has gone quiet for good (fully decoded), not just between ticks.
// Triggers the full-track swap (see SoundImp_Sample_Status()).
#define SA_FULL_IDLE_MS 5000

// Bytes of decoded PCM hashed for the theme fingerprint (see the file
// header's M2 Task 4e section) -- well under SA_MUSIC_THRESHOLD_MS's worth
// of audio at any real sample rate, so it's always available by the time
// the prime idle-detect fires.
#define SA_FP_BYTES 8192

// Max length (incl. NUL) of a persisted/looked-up music track name -- our
// own "d_%08x" content-hash names are always 10 bytes; generous headroom
// costs nothing.
#define SA_MUSIC_NAME_MAX 32

// M2 LIVE-TEST FINDING 3 (superseded by finding 4): the "music distorts a
// lot / gets softer and louder" report was first attributed to gain staging
// (music sitting ~12 dB above SFX in SyncTERM's mixer, into the tanh
// soft-clip) and worked around by scaling the music percent down 1/4. That
// was wrong: the real cause was finding 4 -- the score's IMA-ADPCM decode
// desynced across ring-block boundaries (predictor drift to DC, music lost
// into clipping), which the whole-file decode in vanilla's File_Stream_
// Sample_Vol() now fixes at the source (clean, 0% clipping). The 1/4 scale
// only made the music too quiet vs. SyncDuke/SyncDOOM, so it was reverted:
// music now ships at the engine's own ScoreVolume, matching those doors.
// (The music-vs-SFX mixer balance -- shared by all termgfx doors -- is a
// separate, non-distortion follow-up, not addressed here.)

// ---- content-addressed identity ---------------------------------------------
// Neither termgfx_audio_sfx()'s `id` (0..1023 -- the C;S upload-once cache
// key, audio_mgr.c: "<prefix>/sfx/<id>", `sfx_up[id]`) nor the music path's
// cache name have an engine-side identity to draw on (see file header), so a
// clip's OWN decoded PCM bytes become its identity: hash it. For SFX, map
// the hash to a small stable slot, open-addressed so two different sounds
// never silently collide onto the same id.
#define SA_SFX_SLOTS 1024

static uint32_t g_sfx_hash[SA_SFX_SLOTS];
static bool     g_sfx_used[SA_SFX_SLOTS];

static uint32_t sa_fnv1a(const void *data, size_t len)
{
	const uint8_t *p = static_cast<const uint8_t *>(data);
	uint32_t       h = 2166136261u;
	size_t         i;

	for (i = 0; i < len; i++) {
		h ^= p[i];
		h *= 16777619u;
	}
	return h;
}

static int sa_sfx_id_for(uint32_t hash)
{
	int start = static_cast<int>(hash % SA_SFX_SLOTS);
	int i;

	for (i = 0; i < SA_SFX_SLOTS; i++) {
		int slot = (start + i) % SA_SFX_SLOTS;
		if (!g_sfx_used[slot] || g_sfx_hash[slot] == hash) {
			g_sfx_used[slot] = true;
			g_sfx_hash[slot] = hash;
			return slot;
		}
	}
	return start;   // >1024 distinct sounds this session (won't happen for C&C's SFX set):
	                // degrade gracefully by reusing a slot rather than dropping the sound.
}

// ---- accumulator ------------------------------------------------------------
static void sa_append(SampleTrackerTypeImp *st, const void *data, size_t n)
{
	if (n == 0)
		return;
	if (st->len + n > st->cap) {
		size_t   ncap = st->cap ? st->cap * 2 : 16384;
		uint8_t *nb;
		while (ncap < st->len + n)
			ncap *= 2;
		nb = static_cast<uint8_t *>(realloc(st->pcm, ncap));
		if (nb == NULL)
			return;   // OOM: drop the overflow -- nothing sane to do here
		st->pcm = nb;
		st->cap = ncap;
	}
	memcpy(st->pcm + st->len, data, n);
	st->len += n;
}

// Monotonic wall clock in milliseconds -- same pattern as door_io.c's
// door_now_ms() / termgfx/audio_mgr.c's sfx_now_ms() (both local statics,
// not shared helpers, so this follows suit rather than reaching across
// modules for one function).
static uint64_t sa_now_ms(void)
{
#ifdef _WIN32
	static LARGE_INTEGER freq;
	LARGE_INTEGER        c;
	if (freq.QuadPart == 0)
		QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&c);
	return static_cast<uint64_t>(c.QuadPart) * 1000ULL / static_cast<uint64_t>(freq.QuadPart);
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return static_cast<uint64_t>(t.tv_sec) * 1000u + static_cast<uint64_t>(t.tv_nsec) / 1000000u;
#endif
}

static uint32_t sa_bytes_to_ms(const SampleTrackerTypeImp *st, uint64_t bytes)
{
	uint64_t bytes_per_sec = static_cast<uint64_t>(st->rate > 0 ? st->rate : 22050)
	                         * static_cast<uint64_t>(st->channels > 0 ? st->channels : 1)
	                         * static_cast<uint64_t>(st->bits >= 16 ? 2 : 1);
	if (bytes_per_sec == 0)
		return 0;
	return static_cast<uint32_t>((bytes * 1000u) / bytes_per_sec);
}

static uint32_t sa_duration_ms(const SampleTrackerTypeImp *st)
{
	return sa_bytes_to_ms(st, st->len);
}

// ---- theme fingerprint cache (M2 Task 4e) ------------------------------------
// This file's OWN copy of "where does session-persistent stuff live" -- NOT a
// call into audio_mgr.c (music_cache_dir is private to that translation unit,
// no getter exists, and adding one isn't needed: the warm-session lookup in
// SoundImp_Sample_Status() confirms cache freshness via termgfx_audio_music_
// play()'s own return code, never by reading the disk cache directly). Same
// env var / default door_io.c falls back to when no -audiocache CLI flag is
// given, so the two coincide in that (common, and this task's tested) case;
// when a CLI-only override is used instead, this still works correctly --
// it only needs to find the SAME directory as ITS OWN prior session, not
// audio_mgr's -- see the file header for the full argument.
static void sa_theme_cache_dir(char *dir, size_t sz)
{
	const char *d = getenv("SYNCALERT_AUDIO_CACHE");
	if (d == NULL || d[0] == '\0')
		d = "./audio-cache";
	snprintf(dir, sz, "%s", d);
}

// Create `dir` (and any missing parents) -- standalone copy of door_io.c's
// door_mkpath() (private/static to that file, not exported), same walk-and-
// mkdir-each-segment shape.
static void sa_mkpath(const char *dir)
{
	char   tmp[300];
	size_t i, n;

	n = snprintf(tmp, sizeof tmp, "%s", dir);
	if (n == 0 || n >= sizeof tmp)
		return;
	for (i = 1; i < n; i++) {
		if (tmp[i] != '/' && tmp[i] != '\\')
			continue;
		tmp[i] = '\0';
#ifdef _WIN32
		(void)CreateDirectoryA(tmp, NULL);
#else
		(void)mkdir(tmp, 0755);
#endif
		tmp[i] = '/';
	}
#ifdef _WIN32
	(void)CreateDirectoryA(tmp, NULL);
#else
	(void)mkdir(tmp, 0755);
#endif
}

static void sa_map_path(uint32_t fp, char *path, size_t sz)
{
	char dir[256];

	sa_theme_cache_dir(dir, sizeof dir);
	snprintf(path, sz, "%s/syncalert_theme_%08x.map", dir, fp);
}

// One line, "<d_hash-name> <duration_ms>" -- the "d_<hash>" name a prior
// session persisted for this fingerprint, plus (M2 Task 4g) the duration (ms)
// of the track that name pointed at, so a would-be writer can tell whether
// its own candidate is at least as complete before clobbering a known-good
// entry (see sa_map_write_if_longer()). `duration_ms_out` may be NULL for a
// caller (the warm-prime lookup) that only wants the name. A line written by
// an OLDER build without the duration field parses fine too -- the missing
// second field just reads back as 0, which sa_map_write_if_longer() treats
// as "unknown, always allow the overwrite" rather than failing. Returns false
// if there's no mapping (cold) or the file/name is empty.
static bool sa_map_read(uint32_t fp, char *name, size_t namesz, uint32_t *duration_ms_out)
{
	char          path[300];
	char          line[SA_MUSIC_NAME_MAX + 32];
	FILE *        f;
	size_t        n;
	unsigned long duration_ms = 0;

	if (duration_ms_out != NULL)
		*duration_ms_out = 0;
	sa_map_path(fp, path, sizeof path);
	f = fopen(path, "rb");
	if (f == NULL)
		return false;
	if (fgets(line, sizeof line, f) == NULL) {
		fclose(f);
		return false;
	}
	fclose(f);
	n = strlen(line);
	while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
		line[--n] = '\0';
	sscanf(line, "%*s %lu", &duration_ms);   // absent/unparsable -> stays 0
	snprintf(name, namesz, "%s", line);
	n = strcspn(name, " \t");   // truncate at the duration field, if present
	name[n] = '\0';
	if (duration_ms_out != NULL)
		*duration_ms_out = static_cast<uint32_t>(duration_ms);
	return name[0] != '\0';
}

// Best-effort atomic write (temp file + rename, same shape as audio_mgr.c's
// own mus_write_file()) -- a lost race or a failed write just means the next
// session's prime falls back to the normal cold path, nothing corrupts.
// `duration_ms` (M2 Task 4g) is persisted alongside the name -- see
// sa_map_read()'s comment and sa_map_write_if_longer(), the guarded wrapper
// every real call site below uses instead of calling this directly.
static void sa_map_write(uint32_t fp, const char *name, uint32_t duration_ms)
{
	char  dir[256];
	char  path[300];
	char  tmp[340];
	FILE *f;

	sa_theme_cache_dir(dir, sizeof dir);
	sa_mkpath(dir);
	snprintf(path, sizeof path, "%s/syncalert_theme_%08x.map", dir, fp);
	snprintf(tmp, sizeof tmp, "%s.tmp.%llu", path,
	         static_cast<unsigned long long>(sa_now_ms()));
	f = fopen(tmp, "wb");
	if (f == NULL)
		return;
	if (fprintf(f, "%s %lu\n", name, static_cast<unsigned long>(duration_ms)) < 0
	    || fclose(f) != 0) {
		remove(tmp);
		return;
	}
#ifdef _WIN32
	remove(path);   // Windows rename() won't replace an existing target
#endif
	if (rename(tmp, path) != 0)
		remove(tmp);
}

// M2 Task 4g hardening (reviewer-specified): never let a shorter candidate
// clobber a known-good longer entry. Protects the "stall-then-quit" corner --
// a full, correct entry from an earlier visit to this theme must survive a
// LATER, unrelated short visit (e.g. the player re-enters the same theme and
// quits again within seconds, genuinely idle the whole time so the quiet gate
// in sa_commit_final()/the swap gate is satisfied) that would otherwise
// persist its own much shorter accumulation over it. A missing/unreadable
// existing entry (cold, or an older duration-less line -> reads back as 0)
// always allows the write.
static void sa_map_write_if_longer(uint32_t fp, const char *name, uint32_t duration_ms)
{
	char     existing_name[SA_MUSIC_NAME_MAX];
	uint32_t existing_ms = 0;

	if (sa_map_read(fp, existing_name, sizeof existing_name, &existing_ms)
	    && existing_ms > duration_ms)
		return;
	sa_map_write(fp, name, duration_ms);
}

// One-shot: fire-and-forget onto termgfx's pooled SFX channels.
static void sa_commit_sfx(SampleTrackerTypeImp *st)
{
	termgfx_audio_t *m = door_io_audio();
	uint32_t         hash;
	int              id;

	if (m == NULL || st->len == 0)
		return;
	hash = sa_fnv1a(st->pcm, st->len);
	id   = sa_sfx_id_for(hash);
	termgfx_audio_sfx(m, id, st->pcm, st->len, st->bits, st->channels, st->rate,
	                  st->vol_pct, 0 /* pan: never reaches this seam, see file header */);
}

// Looping music: content-hash identity, cache-checked before rendering -- a
// track already seen this session (or shipped to this client before, when
// the door-side disk cache from a PRIOR session is warm) skips straight to
// playing without re-encoding. termgfx_audio_music_async_submit() isn't a
// fit here (audio_mgr.c: it always MIDI-renders via termgfx_midi_render();
// our PCM is already-decoded AUD data, no MIDI synthesis involved), so this
// uses the synchronous render path. `name_out` (M2 Task 4e) optionally gets
// the generated "d_<hash>" name back, e.g. so the full-track swap in
// SoundImp_Sample_Status() can persist it to the fingerprint map -- NULL/0
// is fine for a plain commit that doesn't need it.
static void sa_commit_music_named(SampleTrackerTypeImp *st, char *name_out, size_t name_out_sz)
{
	termgfx_audio_t *m = door_io_audio();
	uint32_t         hash;
	char             name[SA_MUSIC_NAME_MAX];

	if (name_out != NULL && name_out_sz > 0)
		name_out[0] = '\0';
	if (m == NULL || st->len == 0)
		return;
	hash = sa_fnv1a(st->pcm, st->len);
	snprintf(name, sizeof name, "d_%08x", hash);
	// loop=1: the terminal loops the score forever, while RA's theme manager is
	// told when the song "ended" by SoundImp_Sample_Status()'s own duration model.
	if (termgfx_audio_music_play(m, name, st->vol_pct, 1) == TERMGFX_MUSIC_RENDER)
		termgfx_audio_music(m, name, st->pcm, st->len, st->bits, st->channels,
		                    st->rate, st->vol_pct, 1);
	if (name_out != NULL && name_out_sz > 0)
		snprintf(name_out, name_out_sz, "%s", name);
}

static void sa_commit_music(SampleTrackerTypeImp *st)
{
	sa_commit_music_named(st, NULL, 0);
}

// One-shot commit: everything this ever ships is PRE-Start_Sample
// accumulation -- a one-shot clip's whole body (SoundImp_Start_Sample()'s
// call site) or a sub-threshold stream remainder (sa_commit_final()'s
// fallback tail) -- so it routes to the SFX pool unconditionally, whatever
// its length (M2 live-test finding 2; see the file header). Streams reach
// the music path only through the idle-prime/full-swap/final gates. This is
// the ORIGINAL, fully-destructive commit -- M2 Task 4f deliberately does not
// touch its reset behavior.
static void sa_commit(SampleTrackerTypeImp *st)
{
	if (st->len == 0)
		return;
	if (st->full_committed) {
		// M2 Task 4e/4f: a warm fingerprint-cache hit, or a cold full-
		// track swap that was never un-poisoned (see Buffer_Sample_
		// Data()), already finished this stream's music -- silently
		// discard anything left in the accumulator. Never true for a
		// one-shot SFX (full_committed is only ever set inside the
		// idle-detect block, which never runs for one).
		st->len = 0;
		return;
	}
	st->as_music = false;
	sa_commit_sfx(st);
	st->len = 0;
}

// M2 Task 4f Fix 1: commit whatever has accumulated WITHOUT resetting the
// accumulator -- used only by the cold-path prime commit in SoundImp_
// Sample_Status() (the SA_IDLE_MS gate) so accumulation stays anchored at
// byte 0. Before this fix the prime used the ordinary destructive
// sa_commit(), so the LATER full-track swap only ever saw bytes that
// arrived AFTER the prime -- measured missing the first ~7.1s of a real
// track (209.96s shipped vs. the true 217.07s). The full-track swap's own
// commit is likewise non-destructive now (see SoundImp_Sample_Status()) so
// that Fix 2's premature-swap correction has the true byte-0 history to
// work with; only Start_Sample()'s SFX-before-Start commit and the
// confirmed-end-of-stream commit (sa_commit_final(), below) actually end a
// stream's accumulation.
static void sa_commit_peek(SampleTrackerTypeImp *st)
{
	if (st->len == 0 || st->full_committed)
		return;
	st->as_music = sa_duration_ms(st) >= SA_MUSIC_THRESHOLD_MS;
	if (st->as_music)
		sa_commit_music(st);
	else
		sa_commit_sfx(st);
	// Deliberately no st->len = 0 here -- see the comment above.
}

// M2 Task 4f: the confirmed-end-of-stream commit -- SoundImp_Stop_Sample()
// and SoundImp_Shutdown_Sample() both call this instead of the plain
// sa_commit(). Two things it adds over sa_commit():
//   Fix 4: when the shipped accumulation cleared SA_MUSIC_THRESHOLD_MS (a
//       real music track, not a short SFX-routed clip), persist/rewrite the
//       fingerprint map here too -- Task 4e only ever wrote the map from
//       the full-track-swap gate, so a session whose Stop_Sample() fired
//       BETWEEN the prime and that gate shipped real music (now, after
//       Fix 1, the full track so far) but silently never remembered it for
//       next time.
//   Fix 2 (the other half): if Buffer_Sample_Data() un-poisoned
//       full_committed because data kept arriving after a premature full-
//       track swap, this is where the TRUE complete track (byte 0 onward,
//       intact because neither the prime nor the swap ever reset len --
//       see sa_commit_peek() and the swap site) finally gets shipped under
//       a fresh content hash and the map gets REWRITTEN via the existing
//       atomic tmp+rename in sa_map_write() -- overwriting the swap's
//       stale, truncated entry so a warm session never plays the
//       truncation again.
// M2 Task 4g Fix 2: Task 4f's version above wrote the map on ANY Stop past
// SA_MUSIC_THRESHOLD_MS -- including an early theme change (user backs out
// of the menu at t=10s of a track that takes minutes to fully decode), which
// persisted THAT truncation as the theme's permanent warm-cache entry; Fix
// 1's accumulation-skip on a warm hit means the truncated file's own OGG is
// never re-rendered, so there is no RENDER path left to self-heal it -- a
// warm session just plays the truncation forever. Gate the map write itself
// on decode-quiet evidence: only write when the stream has been idle for
// >= SA_FULL_IDLE_MS at the moment of this final commit. A genuinely
// finished decode (running ~5x real-time) is always quiet well past that by
// the time Stop_Sample()/Shutdown_Sample() fires; an early theme-change Stop
// instead has a recent SoundImp_Buffer_Sample_Data() delivery, so the write
// is skipped -- the music still ships for the transient listen (sa_commit_
// music_named() below is unconditional), only the map persistence is
// withheld. The write itself now also goes through sa_map_write_if_longer()
// (see its own comment) so a later short revisit to an already-fully-mapped
// theme can't clobber a known-good longer entry.
// SFX call sites are unaffected: a plain SFX's duration is under the
// threshold, so it falls straight through to the ordinary sa_commit() tail
// below, byte-for-byte the original Stop/Shutdown reset behavior.
static void sa_commit_final(SampleTrackerTypeImp *st)
{
	char     name[SA_MUSIC_NAME_MAX];
	uint32_t duration_ms;
	bool     quiet;

	if (st->len == 0 || st->full_committed || sa_duration_ms(st) < SA_MUSIC_THRESHOLD_MS) {
		sa_commit(st);
		return;
	}
	duration_ms = sa_duration_ms(st);
	quiet = (sa_now_ms() - st->last_recv_ms) >= SA_FULL_IDLE_MS;
	sa_commit_music_named(st, name, sizeof name);
	st->as_music = true;
	st->len = 0;
	if (quiet && st->fp_done && name[0] != '\0')
		sa_map_write_if_longer(st->fp, name, duration_ms);
}

// ---- SoundImp_* (common/soundio_imp.h) -------------------------------------

bool SoundImp_Init(int bits_per_sample, bool stereo, int rate, bool reverse_channels)
{
	(void)bits_per_sample;
	(void)stereo;
	(void)rate;
	(void)reverse_channels;
	return door_io_audio() != NULL;   // door_io_init() already created the manager
}

void SoundImp_Shutdown(void)
{
	// Nothing global to tear down here -- door_io.c's door_term_restore()
	// already stops the music/SFX channels before the process exits.
}

SampleTrackerTypeImp *SoundImp_Init_Sample(int bits_per_sample, bool stereo, int rate)
{
	SampleTrackerTypeImp *st = static_cast<SampleTrackerTypeImp *>(calloc(1, sizeof(*st)));
	if (st == NULL)
		return NULL;
	st->bits     = bits_per_sample;
	st->channels = stereo ? 2 : 1;
	st->rate     = rate;
	return st;
}

void SoundImp_Shutdown_Sample(SampleTrackerTypeImp *st)
{
	if (st == NULL)
		return;
	// M2 Task 4f: sa_commit_final(), not the plain sa_commit() -- the door
	// exiting mid-track is exactly the case Fix 1/Fix 2 target (a session
	// that never idled long enough to swap, or whose swap was premature),
	// so this may well be shipping (and mapping) the TRUE full track, not
	// just "something not yet committed" as a defensive no-op.
	sa_commit_final(st);
	free(st->pcm);
	free(st);
}

void SoundImp_Set_Sample_Attributes(SampleTrackerTypeImp *st, int bits_per_sample, bool stereo, int rate)
{
	st->bits     = bits_per_sample;
	st->channels = stereo ? 2 : 1;
	st->rate     = rate;
}

void SoundImp_Set_Sample_Volume(SampleTrackerTypeImp *st, unsigned int volume)
{
	int pct = static_cast<int>((static_cast<uint64_t>(volume) * 100u) / 65536u);
	if (pct > 100)
		pct = 100;
	st->vol_pct = pct;
	// Live volume passthrough for an already-committed, currently-looping
	// track (Options.ScoreVolume slider, Fade_Sample's Reducer decay at a
	// theme transition). One-shot SFX have no such live-update concept in
	// termgfx's fire-and-forget API, so this is a no-op for those.
	if (st->as_music)
		termgfx_audio_music_volume(door_io_audio(), pct);
}

// No real device/context to (un)suspend -- termgfx ships fully-rendered
// audio to the client independently of anything we'd pause locally.
void SoundImp_PauseSound(void) { }

bool SoundImp_ResumeSound(void)
{
	return true;   // must return true, or soundio_common.cpp's Start_Primary_Sound_Buffer()
	               // (and hence every Play_Sample_Handle()) refuses to proceed at all.
}

// We have no hardware ring to throttle against -- always report plenty of
// room so soundio_common.cpp's copy loops hand us everything they have
// available right away (proven harmless for SFX; irrelevant to the
// truncation finding above, which reproduces regardless of this value --
// see task-4-report.md).
int SoundImp_Get_Sample_Free_Buffer_Count(SampleTrackerTypeImp *st)
{
	(void)st;
	return 64;
}

void SoundImp_Buffer_Sample_Data(SampleTrackerTypeImp *st, const void *data, size_t datalen)
{
	// M2 Task 4f Minor 3 / M2 Task 4g Fix 1: a WARM fingerprint-cache hit
	// means the full track is already playing from the door-side disk
	// cache / client cache -- this stream's own PCM will never be used for
	// anything, so skip the realloc/memcpy (sa_append()) entirely (a full
	// THEME_INTRO decode is ~9.5MB of PCM). BUT the duration-model counters
	// below (recv_gen/total_bytes/last_recv_ms) still have to advance on
	// EVERY call, warm or cold -- Task 4f's early return skipped those too,
	// which froze SoundImp_Sample_Status()'s wall-clock "still playing"
	// budget at whatever had accumulated by the warm hit (~16s for
	// THEME_INTRO's prime): once real wall time caught up to that frozen
	// budget, Sample_Status() started reporting false even though the
	// cached track was still audibly playing, Maintenance_Callback() reaped
	// it as stopped, and ThemeClass restarted the theme -- an audible
	// stinger + full decode restart every ~16s for the rest of every warm
	// session. Fixed by still returning immediately (this stream's own PCM
	// is genuinely never used, and the full_committed un-poison / fp-hash
	// logic below only makes sense for the COLD path -- see the struct's
	// warm_hit comment), but only AFTER the three counters below run.
	if (st->warm_hit) {
		st->recv_gen++;
		st->total_bytes += datalen;
		st->last_recv_ms = sa_now_ms();
		return;
	}
	sa_append(st, data, datalen);
	st->recv_gen++;
	st->total_bytes += datalen;
	st->last_recv_ms = sa_now_ms();
	if (st->full_committed) {
		// M2 Task 4f Fix 2: reaching here means a COLD full-track swap
		// already fired (warm_hit would have returned above otherwise)
		// and MORE data just arrived anyway -- proof the SA_FULL_IDLE_MS
		// gate misfired mid-decode (a long main-loop stall, not a
		// genuinely finished stream: a real end-of-stream never calls
		// this again). Un-poison: let the next confirmed end of stream
		// (Stop_Sample()/Shutdown_Sample(), or the engine's own
		// duration-reap Stop_Sample() call) ship the TRUE complete
		// track and rewrite the fingerprint map, via sa_commit_final(),
		// instead of sa_commit()'s guard discarding it forever.
		st->full_committed = false;
	}
	// M2 Task 4e: hash the stream's identity once enough of it has arrived.
	// `len` is 0 at the start of every stream (Start_Sample()'s sa_commit()
	// call already cleared whatever preceded it -- SFX or a prior stream),
	// so this always hashes exactly the first SA_FP_BYTES of THIS stream's
	// PCM, regardless of how many ring blocks it took to get there. A no-op
	// for a one-shot SFX's own pre-Start_Sample accumulation (fp/fp_done get
	// reset right after by Start_Sample(), before any stream data exists).
	if (!st->fp_done && st->len >= SA_FP_BYTES) {
		st->fp = sa_fnv1a(st->pcm, SA_FP_BYTES);
		st->fp_done = true;
	}
}

// A one-shot SFX's entire clip is already accumulated by the time this
// fires (see the file header's finding), so committing here ships it in
// full. A streamed Theme instead gets only its first ring block by this
// point -- soundio_common.cpp's Maintenance_Callback() keeps delivering
// more via SoundImp_Buffer_Sample_Data() on later ticks. Reset the
// duration-model baseline (`total_bytes` starts over at whatever's about
// to be committed; `start_ms` anchors the wall clock) and snapshot
// `status_gen` ONCE here -- see SoundImp_Sample_Status() for why it is
// never re-snapshotted again after this.

// Deferred music-channel stop state (see SoundImp_Music_Housekeep below). Both
// Start_Sample (cancel) and Stop_Sample (arm) touch it, so declared above both.
static int      sa_music_stop_pending;
static uint32_t sa_music_stop_at;

void SoundImp_Start_Sample(SampleTrackerTypeImp *st)
{
	st->total_bytes = st->len;
	st->start_ms = sa_now_ms();
	st->last_recv_ms = st->start_ms;
	st->idle_committed = false;
	st->full_committed = false;
	st->warm_hit = false;      // M2 Task 4f Minor 3: fresh stream, not warm yet
	// M2 finding 4: a whole-file score has its ENTIRE track accumulated here
	// (delivered before Start). Don't ship it as a one-shot SFX and don't wipe
	// the fingerprint the delivery already computed -- leave it accumulated so
	// the idle-commit gate in SoundImp_Sample_Status() routes it as looping
	// music, with total_bytes (captured above) keeping the theme "playing" for
	// its full length. A short pre-Start accumulation is an SFX/voice (or a
	// legacy streamed stinger) and ships one-shot here, exactly as before.
	if (sa_duration_ms(st) < SA_SCORE_THRESHOLD_MS) {
		st->fp = 0;            // M2 Task 4e: fresh stream, fresh identity
		st->fp_done = false;
		sa_commit(st);
	} else {
		// A new SCORE (music) stream is starting -- typically a theme CHANGE
		// that just did Stop_Sample(old). Cancel any pending music-channel stop
		// so the new theme replaces the channel seamlessly (the deferred stop is
		// only for a real Theme.Stop() with no new theme following).
		sa_music_stop_pending = 0;
	}
	st->status_gen = st->recv_gen;
}

// No real per-voice playback clock (SyncTERM plays audio out-of-band from
// us, asynchronously). Two independent callers poll this: soundio_common.
// cpp's own Maintenance_Callback() (its internal reap check) AND redalert/
// theme.cpp's ThemeClass::Still_Playing() (via the public Sample_Status()).
//
// The original implementation compared `recv_gen` (bumped on every
// SoundImp_Buffer_Sample_Data() call) against a `status_gen` baseline that
// got RE-snapshotted to recv_gen on every single check -- an edge detector.
// That works for exactly one caller, but with two independent callers each
// consuming (and resetting) the same shared edge, whichever one polls
// SECOND in a given tick sees "nothing new" even though data genuinely
// arrived, because the FIRST caller already ate the edge. Confirmed by
// tracing (M2 Task 4c): this produced Theme restarts roughly every 40ms --
// far too fast to be the vendored SOS/ADPCM ring machinery itself (which,
// once soundio_common.cpp's MoreSource re-arm and File_Stream_Preload's
// ring bookkeeping are correct, keeps well ahead) -- and each restart
// re-decodes the file from byte 0, which is what made the accumulated
// track look "stuck" at a few seconds no matter how long the harness idled.
//
// Fix: replace the racy edge with an idempotent duration model. `status_
// gen` is now a WRITE-ONCE baseline taken only in Start_Sample() (never
// touched here), so `recv_gen != status_gen` answers a stable question --
// "has ANY data arrived since this clip started?" -- the same for every
// caller, this tick or any later one; no consumption, no race.
//
// A one-shot SFX never gets a SoundImp_Buffer_Sample_Data() call after
// Start_Sample() (its whole clip is already accumulated beforehand, see
// the file header's finding), so `recv_gen` never advances past the
// Start_Sample() baseline and this keeps returning false on the very
// first check, byte-for-byte the same prompt reap timing as the original
// edge detector's first (and, for an SFX, only meaningful) check.
//
// Once a clip HAS delivered at least one post-Start_Sample chunk --
// i.e. it is actually streaming, not a one-shot -- switch to the duration
// model: report "still playing" until wall-clock time since Start_Sample()
// catches up to the wall-clock duration of everything accumulated so far
// (`total_bytes`, extended by every later SoundImp_Buffer_Sample_Data()
// call). Because decode here is CPU-bound, not real-time-paced (see
// SoundImp_Get_Sample_Free_Buffer_Count() below), total_bytes routinely
// grows far faster than wall-clock playback would consume it, so this
// comfortably bridges the gaps between ticks that produce no new data
// without misreporting end-of-stream.
//
// *** M2 Task 4d: stream-idle commit (prompt music playback) ***
// sa_commit() otherwise only runs at Start_Sample() (the first ring block,
// almost always under SA_MUSIC_THRESHOLD_MS), Stop_Sample(), or Shutdown_
// Sample() -- so in a live session a streamed Theme's full decode sat in
// the accumulator, uncommitted, until the theme changed or the door exited
// (~205-217s for THEME_INTRO): the user heard the initial sub-threshold
// stinger from Start_Sample(), then silence, and the music C;S upload
// (plus any disk-cache render) didn't happen until minutes later.
//
// Fix: this poller (already run every tick by both callers above) also
// watches for the stream going quiet. Once (a) at least SA_IDLE_MS has
// passed since the last SoundImp_Buffer_Sample_Data() call -- decode has
// paused, not merely between ticks -- and (b) the accumulator already
// holds >= SA_MUSIC_THRESHOLD_MS of audio -- enough to route as music, not
// a stinger -- commit early. Because decode races far ahead of wall-clock
// playback (see above), this fires within a few seconds of Start_Sample(),
// not at the end of the ~205s track: the looping upload (and warm-disk-
// cache / C;L-skip paths) engage at session start instead of at exit.
//
// `idle_committed` guards this to exactly ONCE per Start_Sample()..Stop_
// Sample() stream: without it, every later tick where the accumulator
// stays empty-then-idle again (e.g. a second decode burst arrives, crosses
// the threshold, then goes quiet again) would fire another sa_commit(),
// each one hashing to a DIFFERENT content id and uploading a separate
// "music" clip mid-track instead of one continuous loop. Start_Sample()
// resets the flag, so each new stream gets its own single early commit.
//
// This is provably inert for a one-shot SFX: the `recv_gen == status_gen`
// guard above already returns before this runs for any tracker with zero
// post-Start_Sample deliveries -- true of every SFX by construction (its
// whole clip arrives before Start_Sample(), nothing arrives after).
//
// sa_commit() only resets `len`/the accumulator -- `total_bytes`/
// `start_ms` (the "still playing" wall-clock model below) are untouched,
// so committing here doesn't change what this function reports next: any
// data that keeps arriving after the idle commit still accumulates and
// still gets shipped by the eventual Stop_Sample()/Shutdown_Sample()
// commit, same as before this fix.
//
// *** M2 Task 4e: warm-cache prime + full-track swap ***
// Two more checks, both still gated behind the SAME "has data genuinely
// streamed" guard above (so still provably inert for a one-shot SFX):
//
// 1. WARM PRIME (replaces the plain sa_commit() prime call above when a
//    fingerprint match exists): by the time the SA_IDLE_MS/SA_MUSIC_
//    THRESHOLD_MS prime gate is satisfied, fp_done is already true (over
//    a second of audio at any real sample rate is always >= SA_FP_BYTES).
//    Look the fingerprint up in the persisted map; a hit calls termgfx_
//    audio_music_play() with the remembered name -- a non-RENDER result
//    means the client's own cache or the door-side OGG disk cache (from
//    a PRIOR session) already has the FULL track, so it just started
//    playing, no render, no partial commit. The ~7s partial this stream
//    would otherwise have shipped is discarded outright (st->len = 0)
//    and full_committed latches true immediately -- this stream is done,
//    full length, from ~SA_IDLE_MS in. A miss (no mapping, or the mgr
//    itself reports RENDER -- a stale/evicted cache) falls through to
//    the ORIGINAL Task 4d cold prime unchanged.
//
// 2. FULL-TRACK SWAP (cold path only -- full_committed is already true
//    after a warm hit, so this never re-fires for one): once the stream
//    goes quiet again for SA_FULL_IDLE_MS (well above both SA_IDLE_MS and
//    the inter-burst gaps Task 4d's own evidence measured) with more
//    audio accumulated since the prime, decode is genuinely done -- ship
//    the FULL accumulation as a new content-hash track (sa_commit_music_
//    named(), which internally calls termgfx_audio_music_play()/_music()
//    exactly like a normal music commit; those already replace whatever
//    is on the music channel for a different name, so no explicit stop
//    is needed) and persist name-by-fingerprint so the NEXT session's
//    warm prime (above) can find it. Fires at most once per stream
//    either way (full_committed latches immediately after) UNLESS Fix 2
//    below un-poisons it again.
//
// *** M2 Task 4f Fix 1/2: non-destructive prime + swap, premature-swap
// correction ***
// Both the cold prime (1, above) and the full-track swap (2, above) now
// ship via a commit that does NOT reset the accumulator (sa_commit_peek()
// for the prime; the swap inlines the same non-reset shape) -- see each
// call site's own comment for why. Practically, this means `st->len` keeps
// growing from byte 0 for the ENTIRE life of a stream, right up until a
// confirmed end of stream (sa_commit_final(), from Stop_Sample()/Shutdown_
// Sample()) finally clears it. Two consequences:
//   - The full-track swap genuinely covers the whole track (byte 0 onward),
//     not just what arrived after the prime -- Fix 1's actual bug.
//   - If SA_FULL_IDLE_MS's "quiet for 5s" heuristic misfires mid-decode
//     (Fix 2's target: a long enough main-loop stall -- mission load, MP
//     connect, slow disk -- can look identical to "done" for 5s), more data
//     arriving after the swap already fired is unambiguous proof it was
//     premature (see SoundImp_Buffer_Sample_Data()'s un-poison check,
//     which flips full_committed back to false). The next confirmed end of
//     stream then ships the TRUE complete accumulation under a fresh
//     content hash and REWRITES the fingerprint map (sa_commit_final()),
//     so a stall can no longer wedge a warm session onto a truncated
//     track permanently -- it self-heals within the same session.
bool SoundImp_Sample_Status(SampleTrackerTypeImp *st)
{
	// M2 finding 4: a whole-file score delivers all its PCM before Start, so
	// recv_gen == status_gen holds even though it is a long track that must
	// keep "playing" for its full duration. Bail early only for genuinely
	// short content (SFX/voice) that also streamed nothing after Start.
	if (st->recv_gen == st->status_gen
	    && sa_bytes_to_ms(st, st->total_bytes) < SA_SCORE_THRESHOLD_MS)
		return false;
	if (!st->idle_committed
	    && (sa_now_ms() - st->last_recv_ms) >= SA_IDLE_MS
	    && sa_duration_ms(st) >= SA_MUSIC_THRESHOLD_MS) {
		char cached_name[SA_MUSIC_NAME_MAX];
		bool warm = false;

		// finding 4: a whole-file score fingerprints during pre-Start
		// delivery, but recompute defensively if it hadn't reached SA_FP_BYTES.
		if (!st->fp_done && st->len >= SA_FP_BYTES) {
			st->fp = sa_fnv1a(st->pcm, SA_FP_BYTES);
			st->fp_done = true;
		}
		if (st->fp_done && sa_map_read(st->fp, cached_name, sizeof cached_name, NULL)) {
			termgfx_audio_t *m = door_io_audio();
			if (m != NULL
			    && termgfx_audio_music_play(m, cached_name, st->vol_pct, 1)
			    != TERMGFX_MUSIC_RENDER) {
				st->as_music = true;
				st->len = 0;
				st->full_committed = true;
				st->warm_hit = true;   // M2 Task 4f Minor 3: stop accumulating --
				                       // see SoundImp_Buffer_Sample_Data()
				warm = true;
			}
		}
		if (!warm)
			sa_commit_peek(st);   // M2 Task 4f Fix 1: cold, non-destructive prime --
		                          // see sa_commit_peek()'s own comment
		st->idle_committed = true;
	}
	if (st->idle_committed && !st->full_committed
	    && (sa_now_ms() - st->last_recv_ms) >= SA_FULL_IDLE_MS
	    && st->len > 0) {
		char     name[SA_MUSIC_NAME_MAX];
		uint32_t duration_ms = sa_duration_ms(st);

		sa_commit_music_named(st, name, sizeof name);
		st->as_music = true;
		// M2 Task 4f Fix 2: deliberately NOT st->len = 0 here (Task 4e used
		// to reset it) -- keep the accumulator anchored at byte 0 so that if
		// this swap turns out to be premature (SA_FULL_IDLE_MS misfiring
		// mid-decode during a stall -- more data arrives afterward; see
		// SoundImp_Buffer_Sample_Data()'s un-poison check), the eventual
		// confirmed end-of-stream commit (sa_commit_final(), from Stop_
		// Sample()/Shutdown_Sample()) ships the TRUE complete track, not
		// just the post-swap remainder, and rewrites the map below with the
		// corrected name. M2 Task 4g: sa_map_write_if_longer(), not a plain
		// write -- this gate already IS quiet by construction (that's what
		// just fired it), but a later short revisit to this same theme must
		// not be able to clobber a longer entry a PRIOR stream already
		// established (the "stall-then-quit" corner; see that helper).
		if (st->fp_done && name[0] != '\0')
			sa_map_write_if_longer(st->fp, name, duration_ms);
		st->full_committed = true;
	}
	return (sa_now_ms() - st->start_ms) < sa_bytes_to_ms(st, st->total_bytes);
}

// Deferred music-channel stop. The score loops CLIENT-SIDE (termgfx uploaded it
// once), so resetting our tracker doesn't silence it -- we must send a stop to
// the music channel. But a theme CHANGE also does Stop_Sample(old) right before
// Start_Sample(new), and the new track's Flush+Load replaces the channel, so an
// immediate stop there would leave a silence gap. So: arm a pending stop on
// Stop_Sample of a music tracker, CANCEL it if a new score stream starts
// (SoundImp_Start_Sample), and otherwise fire it a beat later here. Net: a real
// Theme.Stop() (the Sound Controls "stop" button, THEME_QUIET) goes quiet; a
// theme change stays seamless. Called each door tick (door_io_pump).
extern "C" void SoundImp_Music_Housekeep(void)
{
	if (sa_music_stop_pending && (sa_now_ms() - sa_music_stop_at) > 120) {
		termgfx_audio_music_stop(door_io_audio());
		sa_music_stop_pending = 0;
	}
}

void SoundImp_Stop_Sample(SampleTrackerTypeImp *st)
{
	int was_music = st->as_music;

	// M2 Task 4f: sa_commit_final(), not the plain sa_commit() this used to
	// call -- this IS a confirmed end of stream (an explicit switch/fade
	// cutting the tracker short, Stop_Sample() before Start_Sample(), or
	// the engine's own duration-reap Stop_Sample() call), so whatever's
	// left in the accumulator (now, after Fix 1/2, potentially the TRUE
	// full track from byte 0 -- not just a short remainder) ships here, and
	// the fingerprint map gets written/rewritten if it cleared
	// SA_MUSIC_THRESHOLD_MS (Fix 4/Fix 2's correction; see sa_commit_
	// final()'s own comment). A plain SFX still falls straight through to
	// the ordinary sa_commit() tail inside it -- byte-for-byte the original
	// reset behavior, untouched by this task.
	sa_commit_final(st);
	st->as_music = false;
	// Reset the generation/duration-model state too, not just as_music.
	// soundio_common.cpp pools a fixed MAX_SAMPLE_TRACKERS set of these and
	// hands the same index back out to a later, unrelated Play_Sample_
	// Handle() call; Start_Sample() already re-baselines all of this before
	// SoundImp_Sample_Status() is ever consulted for the new stream, so this
	// is currently benign -- but Play_Sample_Handle()'s own "already
	// playing?" pre-check calls Sample_Status() BEFORE that re-baseline, and
	// leaving a stopped stream's stale recv_gen/status_gen/total_bytes/
	// start_ms lying around invites a false "still playing" read from that
	// check. recv_gen == status_gen (both zeroed) means Sample_Status()
	// reports "not playing" immediately, same as a fresh tracker.
	st->recv_gen       = 0;
	st->status_gen     = 0;
	st->total_bytes    = 0;
	st->start_ms       = 0;
	st->last_recv_ms   = 0;
	st->idle_committed = false;
	st->fp             = 0;    // M2 Task 4e
	st->fp_done        = false;
	st->full_committed = false;
	st->warm_hit       = false; // M2 Task 4f Minor 3

	// Arm the deferred client-side music-channel stop (see the note above
	// SoundImp_Music_Housekeep). Only for a music tracker: SFX are one-shot and
	// need no channel stop.
	if (was_music) {
		sa_music_stop_pending = 1;
		sa_music_stop_at       = sa_now_ms();
	}
}
