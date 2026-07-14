# Synchronet door graphics & audio support matrix

A cross-implementation comparison of the graphics and audio capabilities of the
Synchronet game doors that push rich media over the terminal, and the two
JavaScript doors that do so without the shared C library. It exists to make
support gaps visible and to explain *why* a mechanism is present in one door but
absent in another — the answer is almost always one of two axes (see
[Why the gaps](#why-the-gaps)).

**Doors covered**

| Door | Game | Location | Runtime |
|------|------|----------|---------|
| SyncConquer (`syncalert`) | Command & Conquer: Red Alert | `src/doors/syncconquer` | C + [`termgfx`](../src/doors/termgfx) |
| SyncDuke | Duke Nukem 3D | `src/doors/syncduke` | C + `termgfx` |
| SyncDOOM | Doom | `src/doors/syncdoom` | C + `termgfx` |
| SyncRetro | libretro frontend (NES, Intellivision…) | `src/doors/syncretro` | C + `termgfx` |
| SyncMOO1 | Master of Orion (1oom) | `src/doors/syncmoo1` | C + `termgfx` |
| zmachine (JSZM) | Z-machine interactive fiction | `xtrn/zmachine` | JavaScript (SpiderMonkey 1.8.5) |
| minesweeper | Minesweeper | `xtrn/minesweeper` | JavaScript (SpiderMonkey 1.8.5) |

All of the rich-media transports are **SyncTERM/CTerm-proprietary APC** verbs
(sixel is the one broadly-standard exception). No third-party terminal
implements JPEG XL, the audio API, or the blob verbs, so on any non-SyncTERM
client every door degrades to sixel (if the terminal reports it) or to ANSI/text.

---

## Graphics matrix

| Door | Native render | Delivered | Graphics tiers (default **bold**) | Color depth | Frame model | Frame rate | Transport |
|------|---------------|-----------|-----------------------------------|-------------|-------------|-----------|-----------|
| **SyncConquer** | 640×400, 8-bit paletted | 640×400 (1:1, no scale) | **JXL**, sixel (full-res), PPM (opt-in), text | sixel 256-color; JXL/PPM 24-bit RGB | per-frame full-screen re-encode, whole-frame dedupe | change-driven (no cap) | full-frame APC; JXL/PPM **inline blob** ≥1.329 |
| **SyncDuke** | 320×200 → ×2 | 640×400 (client-scaled: sixel `pan/pad=2`; JXL `ZX/ZY=2` ≥1.332) | **JXL**, sixel (half-res), text (half/blocks) | sixel 256-color; JXL 24-bit RGB | per-frame full-screen re-encode | **30 fps** cap | full-frame APC; JXL inline blob ≥1.329, native-res + `ZX/ZY` ≥1.332 |
| **SyncDOOM** | 320×200 → ×2 | 640×400 (client-scaled: sixel `pan/pad=2`; JXL/PPM `ZX/ZY=2` ≥1.332) | **JXL**, PPM (opt-in, LAN), sixel (half-res), text (half/quad/sextant) | sixel 256-color; JXL/PPM 24-bit RGB | per-frame full-screen re-encode | 35 fps sim, **~13 fps** delivered | full-frame APC; JXL/PPM inline blob ≥1.329, native-res + `ZX/ZY` ≥1.332 |
| **SyncRetro** | core-defined (e.g. NES 256×240) | core geometry, may resize | **sixel only** (JXL *probed* but detect-only — no present path yet, planned M3) | sixel 256-color (truecolor quantized; legacy consoles <256 colors, so usually exact) | per-frame full-screen re-encode | paced to **core fps** (~50/60 Hz) | raw DECSIXEL |
| **SyncMOO1** | 320×200 (sixel lossless) | 320×200+ | **sixel only** | sixel 256-color | per-frame full-screen re-encode | change-driven (turn-based) | raw DECSIXEL |
| **zmachine** | text; v6 games add images | image sub-rects | APC copy-buffer pixels **>** JS-encoded sixel **>** `[picture #N]` text — *v6 games only* | 16-color art baked to 24-bit RGB PPM (gamma 0.55 pre-corrected) | **upload-once + blit** (`P;Paste` sub-rects), integer NN scale ≤2× | n/a (static, event-driven) | `C;LoadPPM`/`LoadPBM` + `P;Paste`; per-BBS cache, MD5-negotiated |
| **minesweeper** | text; pixel tiles when able | 16×16-px cell tiles | APC copy-buffer pixels, else 16-color CP437 text | true-color PPM + 1-bit PBM mask; CP437 art for splashes | **upload-once + blit** (`P;Paste` 16×16 tiles) | n/a (event/mouse-driven) | `C;LoadPPM`/`LoadPBM` + `P;Paste`; `C;S`/`C;L` cache w/ CRC |

Notes:
- **JXL is the default tier on all three of Conquer/Duke/DOOM** when the client
  advertises it (`sa_auto_tier()`/equivalent picks JXL first); the doors differ
  only in what their *sixel fallback* looks like.
- **Only SyncConquer renders natively at 640×400** (no upscale) — its 6–8px UI
  fonts can't survive half-res, which is why its **sixel fallback stays
  full-res** rather than the half-res `pan/pad=2` doubling Duke/DOOM use (both
  natively 320×200, so the client integer-doubles their sixel back to 640×400
  losslessly at ~½ the bytes — *on a client that scales*; see
  [Client-side scaling](#client-side-scaling--who-performs-the-upscale)).
  SyncRetro's sixel is also full-res, but for a
  different reason: it up-samples each core frame by arbitrary non-integer
  ratios, which the client's integer doubling can't express.
- **The sixel tier never fills the full 640×400, and Ctrl-F (Aspect↔Fill) is
  visible on SyncTERM.** Two mechanisms shrink the sixel image below the canvas:
  (1) a **bottom-row reserve** — the sixel tier leaves the last text row free
  (16px) as a scroll-guard against a bottom-touching sixel scrolling on
  terminals that ignore DECSDM `?80l`, so the fit height drops to **384**; and
  (2) a **whole-band clamp** — a sixel band is 6 pixels tall (each sixel char =
  6 stacked scanlines) and a *partial* final band garbles under SyncTERM's
  decoder (SF syncterm #258), so the emitted **height** is rounded down to a
  multiple of 6 (`eh -= eh % 6`, door_io.c:1689). On the common 640×400 canvas
  the reserve already gives 384 (itself ÷6, so the band clamp is a no-op there),
  and the two fit modes then diverge — visibly, and with a real legibility
  trade-off on SyncTERM's ~640×400 canvas:
  - **Aspect** (the sticky default when unset) preserves the 640×400 (1.6) ratio
    inside 640×384 → **614×384**, centered with ~13px side margins. Correct
    proportions, but it **downscales the 640-wide framebuffer to 614** — a ~4%
    horizontal shrink that blurs RA's small native UI fonts (the COPYRIGHT line
    and menu labels go noticeably softer/less legible).
  - **Fill** (Ctrl-F) uses the full **640×384** — the height still squishes 400→
    384, but the width stays **1:1 with the 640-wide framebuffer**, so text stays
    crisp at the cost of a ~4% too-wide aspect (the wordmark reads slightly
    fatter). For SyncConquer's native-res UI many users prefer Fill precisely
    because 1:1 horizontal keeps the small fonts readable.

  So on SyncTERM the toggle is *not* a no-op: the bottom-row reserve makes Aspect
  614 wide (narrower than the 640 canvas), which is what Fill stretches into. The
  fit preference is **sticky per-user** (`syncalert.fill` flag under `-home`), so
  which state you start a session in is whatever you last chose — not necessarily
  the Aspect default. (A fit change from Fill→Aspect also used to leave the old
  wider frame's pixels bleeding through the newly-exposed side margins, because
  the shrink wasn't cleared — fixed by having `door_fit_toggle()` request a screen
  clear the way `door_tier_cycle()` already does.)

  The **JXL/PPM** tiers have neither constraint and fill the full 640×400
  (positioned by absolute APC pixel offset, not sixel bands / text cursor) — which
  is why the "Delivered" column reads 640×400 for the default tier; the sixel
  *fallback* is 614×384 (Aspect) / 640×384 (Fill).
  - The same whole-band height clamp is applied by **SyncDuke, SyncDOOM and
    SyncMOO1** (`sxh -= sxh % 6` / `h -= h % 6`) — for the half-res doors it's the
    native 320×200-scale height that's rounded before the client doubles it.
    **SyncRetro is the exception**: it does not pre-clamp and relies on the
    encoder padding a partial final band (`bands = (h+5)/6`), so a
    non-band-multiple core height can still exercise the #258 partial-band path.
- **The three full-screen doors hide the client's status line to reclaim a 25th
  row.** SyncTERM is an 80×**25** terminal; when its status line is visible it
  consumes the 25th row, leaving 24 usable (640×**384** of drawable canvas). On
  entry each door emits DECSSDT `Ps=0` (`termgfx_term_status_off`) so the status
  row is freed and the full 80×25 / 640×**400** canvas is available; the canvas
  probe then reports the reclaimed size and the game fills it (SyncConquer 640×400
  1:1; Duke/DOOM their ×2 320×200). A carried DECRQSS query captures the pre-door
  setting so each door's terminal-restore puts it back on exit. **SyncConquer,
  SyncDuke and SyncDOOM all do this** (default-on); the two JS doors and the
  sixel-only doors don't reserve a full-screen canvas and leave the status line
  alone. (Note SyncTERM's default font uses **non-square pixels** that map a
  640×400 grid to a ~4:3 display — so filling 640×400 there already yields the
  DOS-authentic aspect; a square-pixel font mode like "LCD 80×25" instead shows
  640×400 as 8:5.)
- **PPM tier** is a genuine tier only in **SyncDOOM** (opt-in, LAN/localhost —
  uncompressed) and, opt-in, SyncConquer. termgfx ships no PPM *encoder*; the
  door emits the P6 bytes itself.
- **JXL is a shipping tier in only three doors** (SyncConquer, SyncDuke,
  SyncDOOM) — and it's the *default* tier in all three when the client
  advertises it. Duke and DOOM feed it a ×2-upscaled 320×200 frame; SyncConquer
  a native 640×400 one. Both compress well.
- **Two doors are sixel-only: SyncRetro and SyncMOO1** — but for opposite
  reasons, which the "Analyzed & rejected" section below spells out. SyncRetro's
  JXL tier is **not built yet** (explicitly deferred to milestone M3 — "JXL is
  detect-only", `syncretro_io.c:542`); it would benefit. SyncMOO1's JXL tier was
  **analyzed and rejected** — measured on real frames, lossless JXL is ~5×
  *larger* than sixel for MoO1's indexed pixel art, so it stays sixel-only by
  choice. Both still *probe* for JXL and link `libjxl`; neither consumes the
  reply. SyncMOO1's text/block tier is also deferred, so a non-sixel terminal
  gets **no picture** there (SyncRetro likewise has no text tier).
- The two **JS doors do not re-encode a full frame per frame.** They use
  SyncTERM's **copy-buffer** model: upload a PPM/PBM once (client-cached) and
  blit sub-rectangles with `P;Paste`. zmachine additionally carries its **own
  pure-ES5 PPM→sixel encoder** for non-SyncTERM sixel terminals.

### Analyzed & rejected (a "no" is not always a "not yet")

Some tiers are absent because they were tried or measured and found *not worth
it* for that door — a deliberate decision, distinct from a tier that is merely
unbuilt. Recording the reason keeps the same analysis from being re-run, and
stops "door X lacks tier Y" from reading as a gap when it is actually a choice.

| Door | Tier considered | Verdict | Reason |
|------|-----------------|---------|--------|
| **SyncMOO1** | JXL | **Rejected** | Measured on real frames: **lossless JXL is ~5× *larger* than sixel** for MoO1's indexed pixel art. Only *lossy* JXL beats sixel on size, and it rings the UI text. So the door probes + links `libjxl` but stays sixel-only by design (`syncmoo1/README.md`, DESIGN.md §11). |
| **SyncConquer** | half-res sixel (`pan/pad=2`) | **Rejected** | Its native UI has 6–8 px fonts that don't survive the client's 2× integer doubling — legibility loss. Keeps full-res sixel instead. (Contrast Duke/DOOM, whose 320×200 art doubles losslessly.) |
| **SyncRetro** | half-res sixel (`pan/pad=2`) | **Inapplicable** | Not a value judgment: Retro up-samples cores by arbitrary, per-axis non-integer ratios, which the client's integer-only doubling cannot express. Full-res encode is the only correct option. |

Contrast with genuinely *deferred* (unbuilt, would help) tiers: **SyncRetro
JXL** (M3) and **text/block fallbacks** in SyncRetro and SyncMOO1.

---

## Canvas fit

Once a tier is chosen, the door must size the game frame into the terminal's
graphics canvas — which rarely matches the frame's native aspect (a maximized
Windows Terminal reports something like 2700×1440; SyncTERM reports ~640×400).
There are two shared mechanics plus a per-door *policy* on top.

**Shared mechanic 1 — bounded stretch (`termgfx_geom_fit_ex`).** The fit finds
the largest aspect-preserving rectangle that fits the canvas, then, per axis,
**snaps a thin leftover bar to the full canvas if it's ≤ `max_stretch_pct`% of
the image**, else letterboxes:

```c
if (vw > w && (vw - w) * 100 <= w * max_stretch_pct)  w = vw;   // eat a thin bar
if (vh > h && (vh - h) * 100 <= h * max_stretch_pct)  h = vh;
```

So a small mismatch (a 2–5% sliver that reads as a rendering glitch) fills
cleanly, while a real mismatch letterboxes — with distortion **hard-capped at
`max_stretch_pct`**. `max_stretch_pct == 0` means true aspect, bars kept full.

**Shared mechanic 2 — the sixel bottom-row reserve + `DOOR_SCALE_MAX`.** The
sixel tier leaves the last text row free (scroll-guard, see the Graphics notes)
and clamps emitted width to a bandwidth cap. These shrink/offset the sixel image
independently of the fit policy, which is why the sixel tier can show margins or
a slight vertical squish the JXL/PPM tiers don't.

**Per-door policy:**

| Door | Policy | `max_stretch_pct` | Manual toggle | Notes |
|------|--------|:---:|:---:|-------|
| **SyncDuke** | bounded stretch | **8** | — | `termgfx_geom_fit` (wrapper defaults to 8); fills near-fits, letterboxes the rest |
| **SyncDOOM** | bounded stretch | **8** | — | same (`syncdoom.c` fit call); `-scaling off` forces native 640×400 instead |
| **SyncMOO1** | bounded stretch | **8** | — | `SM_GEOM_STRETCH_PCT`; then widens a height-limited fit back to native 2× |
| **SyncConquer** | **pure aspect + manual Fill** | **0** | **Ctrl-F** | opts *out* of the shared 8%; see below |
| **SyncRetro** | **PAR-correct aspect** | — | — | stretches each frame to the *core's* real display ratio (e.g. 256×240 shown as 4:3), not the frame's pixel ratio |

**Why SyncConquer is the exception.** The 8% bounded stretch is the right
default for the half-res game doors (320×200 upscaled ×2): it uses the screen
without ever grossly distorting the game. SyncConquer instead renders a **native
640×400** UI with 6–8px fonts, where two things the 8% policy can't give matter:

- **Aspect** (`max_stretch_pct = 0`, the default): true proportions, but on an
  off-aspect canvas it *down*-scales the 640-wide frame (e.g. to 614), softening
  the tiny fonts.
- **Fill** (Ctrl-F, sticky per-user via `syncalert.fill`): stretches to the full
  canvas. On a ~640-wide canvas this keeps the width **1:1**, so the fonts stay
  crisp, at the cost of aspect (the classic width-1:1-vs-true-ratio trade-off).

The effect is subtle on SyncTERM (canvas ≈ native, so Aspect ≈ Fill apart from
the bottom-row reserve) and dramatic on Windows Terminal (a wide, off-aspect
canvas, where Fill visibly stretches). Because it's an orthogonal axis to the
tier — and its ideal value depends on the *client's* canvas aspect, which the
door knows at probe time — an auto aspect-vs-fill default (rather than a manual
toggle) is a natural future refinement, and would suit the other doors too if
they ever want it. See the Graphics notes for the Fill-centering and
margin-clear details.

---

## Client-side scaling — who performs the upscale

A door that renders at 320×200 and displays at 640×400 can either **upscale the
frame itself** and ship the big image, or **ship the small image** and have the
terminal enlarge it. The second is strictly cheaper — a 2× frame costs roughly
2.4–2.8× the bytes to encode at any quality setting — and, because every upscale
in this chain is nearest-neighbor pixel replication, the picture is *identical*
either way. There is no quality argument, only a compatibility one: **can this
client scale, and along which axes?**

Two independent mechanisms, one per tier.

### Sixel: the `pan;pad` raster attribute

DECSIXEL's raster attribute carries a pixel *aspect ratio* (`pan;pad`). Reading
it as a **scale factor** is a cterm extension, and support fragments three ways:

| Terminal | Sixel scaling | Consequence for a half-encoded sixel |
|----------|---------------|--------------------------------------|
| **SyncTERM/cterm** | **both axes** — `pan`/`pad` are integer pixel scales | encode at ½×½, terminal restores full size (~½ the bytes) |
| **foot**, **Contour**, **Windows Terminal** | **vertical only** (the DEC aspect reading) | half-height encode is restored; half-**width** is not |
| **xterm 390**, **WezTerm** | **none** — renders at the encoded size | a half-encoded sixel appears half-size (needs a full 1:1 encode) |

Hence the doors' sixel policies: `pan=2` (vertical halving) is sent broadly,
while `pad=2` (horizontal) goes **only** to SyncTERM, because a strict-DEC client
would draw a half-width image. See `syncduke_io.c` (`vsc`/`hsc`) and SyncDOOM's
`emit_frame_sixel()`.

> **Careful when observing this: the full-res sixel tier is a *sticky per-user*
> preference, not a per-session default.** It starts off (`g_sixel_fullres = 0`),
> but the F4 tier cycle **persists** whatever you land on — SyncDOOM to
> `[video] sixel_fullres` in the user's `syncdoom.ini`, SyncDuke to a
> `syncduke.fullres` touch-file rewritten on exit (both under
> `data/user/<n>/{doom,duke}/`). So cycling tiers once to test leaves the door
> starting in **sixel-full** on every later session, on every terminal — and a
> full-res encode (`vsc = hsc = 1`) never exercises the `pan=2` path at all, on any
> client. It's easy to conclude from a live session that full-res is the default
> for non-SyncTERM; it isn't. Clear the pref to see the real default.

**The doors no longer guess: they measure.** All three (SyncDuke, SyncDOOM,
SyncMOO1) run `termgfx_sixel_vscale_probe()` once at startup against any
non-SyncTERM sixel terminal — it draws the same sliver twice, `pan=1` then
`pan=2`, and compares how far the cursor advanced. A terminal that honors the
raster pan advances twice as far. That is self-calibrating (no cell-height
assumption) and tests the *behavior* rather than sniffing a name that rots, so
one policy now serves every client:

| | vertical (`pan`) | horizontal (`pad`) |
|---|:---:|:---:|
| SyncTERM | 2 | 2 |
| probe says it scales (foot, Contour, WT) | 2 | 1 |
| probe says it doesn't (xterm, WezTerm) | 1 | 1 |

No answer within the window reads as "does not scale" — the safe direction, since
a full 1:1 encode is correct on every terminal and merely fatter.

> **The probe's one blind spot.** It measures the *space* the image occupies, not
> whether the content was *stretched* to fill it. A terminal that honored `pan` by
> reserving a taller box and letterboxing the pixels inside would advance the cursor
> exactly as far as one that scales, and would be misread as scaling. Nothing on the
> wire distinguishes them — the cursor position is the only thing a terminal ever
> reports back about a sixel — so any probe over this protocol inherits the hole. No
> characterized terminal behaves this way, and the failure is loud (a half-size
> picture in a full-size box) with an immediate remedy: **F4 → full-res**, which
> encodes 1:1 and is correct regardless of what the terminal does with `pan`. That is
> exactly why the manual override earns its keep alongside the auto-detection.

**Never encode below native.** Whatever the terminal can scale, the door caps the
factor at what keeps the *encode* at or above the game's native frame
(`termgfx_geom_sixel_scale()`, long SyncMOO1's rule, now shared). The encode is
what carries the pixels; the display is encode × factor. Let the encode fall under
native and you discard rows the game actually drew, and the terminal then replicates
them back to the right *size* — right-sized, permanently blurrier. It answers per
axis, and the axes genuinely disagree: fitting 320×200 into 640×384 gives `pad=2`
(320 encoded columns *is* native) but `pan=1` (384/2 = 192 < 200). SyncDuke and
SyncDOOM previously hardcoded 2 on both axes and silently dropped 8 of the 200 rows
there. Big canvases — where sixel actually lives — are unaffected: their half-res
encode sits comfortably above native, so it stays lossless *and* cheap.

**What full-res (F4) is now for.** Not correctness — the probe and the guard handle
that. It is (a) the player's explicit bytes-for-exactness call, since a half-res
encode quantizes the scale to 2×2 blocks while a 1:1 encode lands on the fitted
rectangle pixel-exactly, and (b) the backstop when the probe is fooled. It stays a
sticky per-user preference, and is offered as an F4 stop **only where it is a
different picture** — on a terminal that scales neither axis the only possible
encode is 1:1, so a second stop would be the same image under another name.

### JXL/PPM: the graphics-APC `ZX`/`ZY` zoom (CTerm ≥ 1.332)

The cached/blob image path had no scaling at all until CTerm 1.332 added `ZX`/`ZY`
integer zoom factors (SourceForge syncterm feature-request #136), alongside
`FX`/`FY` mirroring and negative `DX`/`DY` destinations. A door now sends its
**native** frame plus `ZX=2;ZY=2` instead of a pre-upscaled one — the ~60%
bandwidth reduction the request was filed for.

The zoom is **integer-only, and SyncTERM has no resampler** (its only scaling
anywhere — sixel raster attributes included — is whole-pixel replication). So the
door can delegate the upscale only when the fitted size is an *exact* multiple of
the native frame; `termgfx_geom_zoom()` makes that call, and any other fit falls
back to the door-side resample it always did. In practice:

| Canvas | Fit | Zoom |
|--------|-----|------|
| 640×400 (80×25, 80×50) | 640×400 | **ZX=2 ZY=2** — encode 320×200 |
| 640×480 (80×30, 80×60), 1056×400 (132×25) | 640×400 (letterboxed) | **ZX=2 ZY=2** |
| 640×384 (status line left on) | 640×384 | none — door resamples |
| 640×392 (80×28), 640×350 (EGA) | 640×392 / 560×350 | none — door resamples |
| 320×200 (40×25) | 320×200 | 1× (already native) |

Two consequences worth internalizing. The factor is always **2×** — a 4× fit would
need a 1280-wide canvas and SyncTERM's widest is 1056. And **keeping the status
line costs the optimization entirely** (640×384 isn't a multiple of 200), which
is now a load-bearing reason for the doors to hide it, not just a cosmetic one.

---

## Audio matrix

| Door | Audio? | Transport | SFX source → wire | Music source → synth → codec/rate | Streaming | MIDI (OPL3) |
|------|--------|-----------|-------------------|-----------------------------------|-----------|-------------|
| **SyncConquer** | Yes | SyncTERM audio APC | AUD/ADPCM → PCM **22050 Hz** → Opus, upload-once content-hashed cache (1024 slots), L/R pan | Klepacki AUD **22050 Hz** → Opus, door-side disk cache + client `C;L` skip | **Yes — FMV/VQA cutscene**: 22050 Hz **16-bit mono**, 16 KB (~0.37 s) PCM chunks, A/V-synced; uses **`A;LoadBlob`** ≥1.329 | No (digital tracks) |
| **SyncDuke** | Yes (SyncTERM ≥1.10) | SyncTERM audio APC | Creative **VOC**, 8-bit unsigned, upload-once cache | OPL/MIDI → **OPL3** (libADLMIDI) → Opus **48000 Hz**, worker thread, 3-D positional pan | No (cached) | **Yes** (MIDI source) |
| **SyncDOOM** | Yes (SyncTERM ≥1.10) | SyncTERM audio APC | Doom **DMX** lumps, 8-bit unsigned mono, lump-native rate | MUS/MIDI → **OPL3** (libADLMIDI) → Opus **48000 Hz**, ship-once client cache | No (cached) | **Yes** (MUS source) |
| **SyncRetro** | Yes (needs libsndfile) | SyncTERM audio APC | — (no discrete SFX; game audio only) | — | **Yes — live game audio**: core-mixed PCM → **mono Opus 100 ms chunks** (~1.2–2.0 KB, ~20 KB/s); rate = **core's** (44100/48000), not constant | No |
| **SyncMOO1** | Yes | SyncTERM audio APC | LBX-wrapped **VOC**, upload-once, played by name | 40-track **XMI** → MIDI → **OPL3** (libADLMIDI) → Ogg **48000 Hz**, content-addressed disk cache | No (cached) | **Yes** (XMI source) |
| **zmachine** | Yes (3-tier) | SyncTERM audio APC via `cterm_lib.js` | Blorb `Snd ` **AIFF** resources → `A;Load` (client libsndfile decodes), channel 2 (one-at-a-time, voice-steal) | — (no music track concept) | — (bleeps: `A;Synth` tones 330/880 Hz, ch 3) | No |
| **minesweeper** | **No** | — | — | — | No | No |

Notes:
- **Two doors stream** (the `A;LoadBlob`/`stream_chunk` path): **SyncRetro**
  (continuous live core audio) and **SyncConquer** (VQA cutscenes). Everyone else
  uploads-once and caches — better for **repeated** SFX/music, since the client
  replays a cached slot with no payload.
- **OPL3 FM music** (libADLMIDI) is used by Duke (MIDI), DOOM (MUS) and MOO1
  (XMI). SyncConquer and SyncRetro use **digital** audio only, no OPL.
- **Music encodes at 48 kHz** (Duke/DOOM/MOO1, the Opus-legal rate); native
  digital content is **22050 Hz** (SyncConquer) or the **core rate**
  (SyncRetro). SFX is **8-bit unsigned** for the Build/Doom doors (VOC/DMX).
- **zmachine's three audio tiers** (`zsound.js`): `2 digital` (samples + tones),
  `1 tone` (tones only, samples silent), `0 bel` (ASCII BEL only). It's the only
  door that gracefully falls all the way back to the terminal bell.
- **minesweeper has no audio at all** — despite `.bin` files named `boom`/`mine`,
  those are CP437 *explosion art*, not sound.

The underlying termgfx audio wire model (shared by all five C doors): 256 patch
slots (0 = music, 1–255 transient SFX), channels 2–15 (music = 2, one-shot SFX
pool = 3–10 voice-stealing, looping/ambient = 11–15), WAV-wrapped 8/16-bit PCM
uploaded and decoded client-side by libsndfile (Opus/OGG/FLAC/WAV/VOC, MP3 on
libsndfile ≥1.1.0).

---

## Input & control matrix

Input divides the same way rendering does: the five C doors negotiate raw
terminal input protocols through `termgfx`, while the two JS doors ride
Synchronet's `console` input layer (which does its own mouse/key handling).

### Mouse

| Door | Mouse? | Modes requested | Granularity | Used for |
|------|--------|-----------------|-------------|----------|
| **SyncConquer** | Yes | `?1003h` any-motion + `?1006h` SGR + **`?1016h` SGR-Pixels** | **pixel** where 1016 is honored, else cell (auto-detected from out-of-grid coords) | unit select/move, screen edge-scroll, music-volume slider drag |
| **SyncDuke** | Yes | `?1003h` + `?1006h` SGR | cell → mapped to a turn rate | mouse-look steer (Ctrl-O cycles off/steer/follow) |
| **SyncDOOM** | Yes | `?1003h` + `?1006h` SGR | cell → turn rate | mouse-look steer (Ctrl-O cycles off/steer/follow) |
| **SyncRetro** | **No** | — | — | the libretro pad is keyboard-mapped; no pointer |
| **SyncMOO1** | Yes | `?1003h` + `?1006h` SGR + **`?1016h` SGR-Pixels** | cell / pixel | MoO1 UI clicks (turn-based, so no motion pacing) |
| **zmachine** | via `console` | Synchronet `console.mouse` | cell | menu / map clicks (v6) |
| **minesweeper** | via `console` | `console.mouse` (mode 1003) | cell | reveal / flag a cell |

The **pixel-level (SGR-Pixels, DEC 1016)** nuance the doors have to cope with:
**xterm and Windows Terminal report pixel coordinates; SyncTERM currently does
not** (it acknowledges `?1016h` but keeps sending 1-based text cells). So the two
doors that ask for 1016 (Conquer, MoO1) auto-detect it — a reported coordinate
past the text grid proves pixels are live — and fall back to cell coords on
SyncTERM. Duke/DOOM don't bother requesting 1016: a cell is fine for mapping a
pointer column to a turn rate.

### Keyboard protocol

The C doors negotiate a real key-up protocol so a held key = continuous motion
(hold-to-move / hold-to-turn); without one, a terminal only sends key-*down* and
the door has to synthesize releases on a timer.

| Door | Kitty keyboard (`CSI?u`) | evdev (SyncTERM physical keys) | Fallback |
|------|--------------------------|-------------------------------|----------|
| **SyncConquer** | Yes | Yes | byte (synthesized releases) |
| **SyncDuke** | Yes | Yes | byte |
| **SyncDOOM** | Yes | Yes | byte |
| **SyncRetro** | Yes | Yes | byte |
| **SyncMOO1** | **No** | **No** | **byte only** — turn-based, so it never needs key-up |
| **zmachine** | via `console` | — | `console.getkey` (BBS input layer) |
| **minesweeper** | via `console` | — | `console.getkey` |

Both negotiations live in the shared `termgfx/keymode.*`; a door emits the
queries in its startup probe and the reply flips it into hold-to-move mode. MOO1
deliberately opts out (nothing to hold in a 4X turn game). The JS doors don't see
raw protocols at all — Synchronet's console gives them decoded keys.

### Door hotkeys

Keys the *door shim* intercepts (distinct from keys forwarded to the game). All
are live in-session; the C doors read them off the raw byte/evdev/kitty stream,
the JS doors as typed commands.

| Action | Conquer | Duke | DOOM | Retro | MOO1 | zmachine | mines |
|--------|:-------:|:----:|:----:|:-----:|:----:|:--------:|:-----:|
| Cycle graphics tier | `F4` | `F4` | `F4` | — | — | — *(auto)* | — |
| Frame-pipeline depth | — | `Ctrl-T` | `Ctrl-T` | — | — | — | — |
| Stats overlay | `Ctrl-S` | `Ctrl-S` | `Ctrl-S` | `Ctrl-S` | — | — | — |
| Mouse steer toggle | — | `Ctrl-O` | `Ctrl-O` | — | — | — | — |
| Music/SFX volume | `+` `-` `=` | *(in-game menu)* | *(in-game menu)* | `+` `-` `=` | — | — | — |
| Display-fit toggle | `Ctrl-F` | — | — | — | — | — | — |
| Who's online | `Ctrl-U` | `Ctrl-U` | `Ctrl-U` | — | — | — | — |
| Page a node | `Ctrl-P` | `Ctrl-P` | `Ctrl-P` | — | — | — | — |
| Redraw / console reset | — | — | — | `Ctrl-R` | — | `Ctrl-R` | — |
| Emergency quit | `Ctrl-Q` | *(game)* | *(game)* | `Ctrl-Q` | *(game)* | *(game)* | *(game)* |

Notes:
- **Pipeline-depth (`Ctrl-T`) exists only in Duke/DOOM**, the two doors with the
  AIMD DSR-ACK frame-pacer. Conquer (change-driven), Retro (paced to core fps)
  and MOO1 (turn-based) have no in-flight depth to tune.
- **Door-level volume hotkeys exist only in Conquer and Retro** (both `+`/`-`,
  with `=` as an unshifted alias for `+`). Duke and DOOM route volume through
  their engines' own sound-setup
  menus instead, so the door doesn't shadow those keys.
- **The node overlay (`Ctrl-U` who's-online, `Ctrl-P` page) is a full-screen-door
  feature** (Conquer/Duke/DOOM). Retro/MOO1 and the JS doors don't carry it.
- **MOO1 exposes no door hotkeys at all** — sixel-only and turn-based, it has no
  tier to cycle, no pacing to tune, and no overlay, so every key goes to 1oom.
- **zmachine's "hotkeys" are typed `#` commands** rather than control keys,
  fitting its parser-driven interaction. They toggle the display *layout*, not
  the graphics tier: `#ansi` forces full-screen ANSI (framed, with a status line
  and the game's split windows), `#scroll` forces a plain line-scrolling
  transcript, and `#display` toggles between them. The picture tier itself (APC
  copy-buffer > sixel > text) is auto-negotiated, with no user toggle.

---

## Capability detection & version gating

Every door probes the terminal before selecting a tier — the mechanisms are the
same, whether in C (`termgfx/caps.c`) or JS (`exec/load/cterm_lib.js`):

| Probe | Query | Answer identifies |
|-------|-------|-------------------|
| Sixel | DA1 (`ESC[c`) attribute 4 | pixel-ops capable terminal |
| JPEG XL | `ESC_SyncTERM:Q;JXL ST` → `ESC[=1;{0,1}-n` | JXL support (and SyncTERM peer) |
| Audio / libsndfile | `ESC_SyncTERM:Q;libsndfile ST` → `ESC[=7;100;{0,1}n` | `1` digital / `0` tone-only |
| CTerm version | DA1 → `ESC[=67;84;101;114;109;MAJ;MIN…c` ("Cterm") | `MAJ*1000+MIN` (e.g. 1.329 → 1329) |

Version thresholds that gate features (named in `termgfx/caps.h`, mirrored in
`cterm_lib.js`):

| Version | Feature |
|---------|---------|
| 1002 | APC PPM pixel-media verbs (`DrawPPM`) — `TERMGFX_CTERM_VER_PPM` |
| 1189 | Sixel |
| 1207 | CTerm Device Attributes (CTDA) |
| 1316 | Copy buffers (the `LoadPPM`/`P;Paste` path the JS doors use) |
| 1318 | JPEG XL |
| 1329 | APC **blob** verbs — `A;LoadBlob`, `Draw*Blob`, `Load*Blob` — `TERMGFX_CTERM_VER_BLOB` |
| 1332 | APC **integer zoom** `ZX`/`ZY` (plus `FX`/`FY` mirroring, negative `DX`/`DY`) — `TERMGFX_CTERM_VER_ZOOM` |

**Blob verbs (≥1.329)** let audio and JXL/PPM frames ship *inline* with no cache
file. On the non-blob cache path the C doors salt the frame cache filename with
the door PID (`termgfx_session_salt()`) so two SyncTERM windows of one
dialing-entry — which share `~/.cache/syncterm/<bbs>/` — don't collide
(SourceForge syncterm #256).

**Frame pacing** (the five C doors, `termgfx/pace.c`): send a frame + `ESC[6n`,
wait for the DSR reply, and settle an AIMD in-flight pipeline depth around a
~40 ms RTT baseline. Delivered fps ≈ pipeline depth ÷ round-trip, which is why
SyncDOOM's 35 fps sim delivers ~13 fps over a real link. The JS doors have no
such loop — their graphics are static/event-driven, so no per-frame pacing is
needed.

---

## Why the gaps

Almost every difference above reduces to **two independent axes**:

### Axis 1 — runtime: compiled C + termgfx vs. interpreted ES5 JS

The five C doors link `termgfx`, which does per-frame pixel quantization, sixel/
JXL/OPL3/Opus encoding, DSR-ACK pacing, and threaded music rendering in native
code. The two JS doors run under **SpiderMonkey 1.8.5** (ES5 only, no compiled
inner loop). Consequences:

- **No real-time full-frame video in JS.** A JS door cannot re-encode a
  640×400 frame ~30×/sec — the quantize+encode inner loop would be far too slow
  interpreted. So the JS doors avoid a per-frame pipeline entirely and use the
  **copy-buffer** model: upload art once (client-cached), then blit sub-rects.
  That's perfect for a board game or an IF illustration, and impossible to scale
  to a running 3-D engine.
- **JS leans on the client's persistent cache** for both graphics and audio
  (`C;L` MD5/CRC negotiation, `audio_prefetch`) precisely because re-sending
  payloads each turn is cheap enough interpreted only when it happens rarely.
- **Everything JS *does* do it does through `cterm_lib.js`**, the shared JS
  wrapper around the very same SyncTERM APC verbs termgfx emits — so a JS door
  and a C door reach the identical terminal features; they just drive them at
  different rates and granularities.

### Axis 2 — media model: continuous engine vs. discrete scene

- **Continuous engines** (Duke, DOOM, Retro, and Conquer's tactical view) produce
  a new pixel frame every tick → they all need per-frame encode + DSR-ACK pacing.
  Where a smaller frame pays off over a link they add a JXL tier (Conquer, Duke
  and DOOM default to it when the client has it; Retro's is still on the
  roadmap), and the two with a *continuous,
  unique* audio stream ship it via the blob/stream path (Retro live audio,
  Conquer FMV).
- **Turn/scene-driven** experiences (MOO1, zmachine, minesweeper) only redraw on
  input, so they never need a frame cap or streaming, and a static
  upload-once/change-driven model suffices. MOO1 is the interesting hinge: it's a
  C+termgfx door but *turn-based*, so it keeps the full pixel pipeline yet paces
  change-driven like the JS doors.

Individual consequences that fall out of these axes:

- **Audio streaming exists in exactly two doors** (SyncRetro live game audio,
  SyncConquer FMV) because only those have a *continuous, unique* audio stream.
  A repeated SFX or a loopable music track is better served by the upload-once
  cache, so the SFX/music doors deliberately don't stream.
- **OPL3/FM music only where the source is a MIDI-family lump** (Duke MIDI, DOOM
  MUS, MOO1 XMI). RA and libretro cores already carry digital audio, so there's
  nothing to synthesize.
- **minesweeper has no audio** because Minesweeper has no sound design — not a
  capability gap, a content one.
- **zmachine graphics only for Z-machine v6 games**, audio only for Blorb titles
  with `Snd ` resources — the format simply doesn't carry media otherwise.

---

## Gaps & opportunities this surfaced

- **One sixel-only door with a real JXL opportunity: SyncRetro.** Its JXL
  present path is plumbed (probe + `libjxl`) but explicitly deferred to M3; a JXL
  tier (smaller frames over a remote link) is a genuine win there. **Not
  SyncMOO1** — that door's JXL was analyzed and rejected (JXL *loses* to sixel on
  its indexed pixel art; see "Analyzed & rejected"). A **text fallback is missing
  in both**, though — a non-sixel terminal currently gets *no picture* from
  either, which is a real gap independent of JXL.
- **Tone tier is effectively silent at the manager layer.** `termgfx`'s
  `A;Synth` tone builder exists, but `audio_mgr.c` never calls it, so a `tier 0`
  (audio-APC-but-no-libsndfile) client gets nothing from the C doors — where
  zmachine's `zsound.js` *does* fall back to synth tones (and then BEL). The C
  doors could adopt the same graceful degradation.
- **The blob transport is audio+JXL/PPM only.** The JS doors' copy-buffer path
  (`LoadPPM`/`P;Paste`) has no blob equivalent, and none is needed (their images
  are cached-and-reused, not transient) — worth noting so it isn't mistaken for
  an omission.
- **zmachine `@sound_effect` completion routines run at queue time** (the
  immediate-callback interpreter shortcut): a sound started inside the routine
  queues onto the same channel behind the current one, so chains (Sherlock's
  ambient-loop resume; the recursive Big Ben hour chimes) play in order
  client-side — plain same-channel Queues, no playback-end notification
  needed. Residual gap: a routine's *game-state* side effects land at queue
  time rather than true playback end; event-accurate timing would need the
  playback-state reporting the audio APCs have always had (`A;Update` one-shot
  idle notify, `A;Wait`, the `CSI =7;<ch> n` state query).
- **SyncRetro has no networked multiplayer** (deferred); it drives two local
  players on one keyboard only. Duke/DOOM/Conquer all have UDP netplay.
- **SyncMOO1 arrow-key menu navigation doesn't work** (mouse/hotkeys only) — a
  usability gap distinct from graphics/audio.

---

*Maintenance note: regenerate the quantitative cells from the sources when a
door's tier set, resolution, sample rate, or fps cap changes — the numbers here
are read from `src/doors/*/` and `xtrn/{zmachine,minesweeper}/` as of the CTerm
1.329 blob-media work.*
