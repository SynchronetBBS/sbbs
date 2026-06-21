// v6pics.js — v6 door picture runtime: slim-manifest loader + APC picture-blit bridge.
// Loaded by zmachine.js (like quetzal.js). All I/O is injected into the bridge so this file is
// unit-testable headless (test/v6pic.js). ES5 / SpiderMonkey 1.8.5.

// JSZM_loadManifest(dir) -> { count, release, pics:{<num>:{type,w,h,file,mask}}, adaptive:{<num>:true}, remap:{<scene>:{<adaptive>:<variant>}} } | null
// Parses <dir>/manifest (slim format from tools/blorb2gfx.js). Returns null if absent/invalid.
function JSZM_loadManifest(dir) {
  var f = new File(dir + "/manifest");
  if (!f.exists || !f.open("r")) return null;
  var out = { count: 0, release: 0, pics: {}, adaptive: {}, remap: {} }, line;
  while ((line = f.readln()) !== null && line !== undefined) {
    line = ("" + line).replace(/^\s+|\s+$/g, "");
    if (line === "") continue;
    if (line.charAt(0) === "#") {
      var mC = line.match(/count=(\d+)/), mR = line.match(/release=(\d+)/);
      if (mC) out.count = parseInt(mC[1], 10);
      if (mR) out.release = parseInt(mR[1], 10);
      continue;
    }
    if (line.charAt(0) === "@") {                  // @ <scene> <adaptive> <variant>
      var rt = line.substring(1).replace(/^\s+/, "").split(/\s+/);
      if (rt.length >= 3) {
        var sc = parseInt(rt[0], 10), ap = parseInt(rt[1], 10), vp = parseInt(rt[2], 10);
        if (!out.remap[sc]) out.remap[sc] = {};
        out.remap[sc][ap] = vp;
        out.adaptive[ap] = true;
      }
      continue;
    }
    var t = line.split(/\s+/);            // <num> <type> <w> <h> <file> [<mask>]
    if (t.length < 5) continue;
    var num = parseInt(t[0], 10);
    out.pics[num] = {
      type: t[1], w: parseInt(t[2], 10), h: parseInt(t[3], 10),
      file: (t[4] === "-") ? null : t[4],
      mask: (t.length >= 6 && t[5] !== "-") ? t[5] : null
    };
  }
  f.close();
  return out;
}

// JSZM_v6LoadLayout(dir) -> { authoredWidth, borderFrame, windowedText } : per-game v6 layout overrides from the
// re-bake-safe `<dir>/layout` sidecar (hand-authored; survives a Blorb re-bake, unlike the manifest).
//   authoredWidth=<px>  the coordinate-space width the game lays out in. A native-authored game (Zork
//                       Zero) positions pictures/windows in 320-space and ignores the reported (scaled)
//                       screen size; the door scales picture POSITIONS by posScale = deviceWidth/authoredWidth.
//                       Absent -> device width -> posScale 1 (Arthur, scaled-authored, unchanged).
//   borderFrame=1       OPT IN to enclosing-border-frame layout (Zork Zero). Border-frame detection is
//                       ENTIRELY skipped unless this is set, so Arthur (no sidecar) can never be mis-classified
//                       as a frame by an ornate full-screen border picture -> zero regression.
//   windowedText=1      OPT IN to multi-window text layout (Journey): the renderer mirrors the game's own
//                       declared text windows (win0 narrative inset + a left menu side-panel) instead of the
//                       single-column story path. Absent -> single-column (Arthur/Zork Zero unchanged).
function JSZM_v6LoadLayout(dir) {
  var out = { authoredWidth: 0, borderFrame: false, windowedText: false }, m;
  var f = new File(dir + "/layout");
  if (f.exists && f.open("r")) {
    var line;
    while ((line = f.readln()) !== null && line !== undefined) {
      line = "" + line;
      if ((m = line.match(/^\s*authoredWidth\s*=\s*(\d+)/))) out.authoredWidth = parseInt(m[1], 10);
      else if ((m = line.match(/^\s*borderFrame\s*=\s*(\d+)/))) out.borderFrame = (parseInt(m[1], 10) !== 0);
      else if ((m = line.match(/^\s*windowedText\s*=\s*(\d+)/))) out.windowedText = (parseInt(m[1], 10) !== 0);
    }
    f.close();
  }
  return out;
}

// JSZM_v6ScreenFrame(drawnPix, screenW, screenH, cellW, cellH) -> {topPx,bottomPx,leftPx,rightPx} | null
// Classify the drawn pictures as an ENCLOSING border frame (Zork Zero) vs a top band with interior
// decoration (Arthur). The DEFINING feature of a frame is a side column touching BOTH the left and right
// screen edges below the top -- Zork Zero's frame is a top strip plus full-height LEFT/RIGHT edge columns
// (and NO bottom strip), whereas Arthur's banner + "poles" sit well INSIDE the screen (x~164/476 on a
// 640px screen), not at the edges, so it is not a frame. drawnPix are DEVICE pixel rects {dx,dy,w,h};
// slivers (w<8 or h<4) are ignored and full-width strips never count as side columns. Returns the per-edge
// border thickness in pixels (bottomPx 0 when there is no bottom strip).
function JSZM_v6ScreenFrame(drawnPix, screenW, screenH, cellW, cellH) {
  var tolY = cellH, tolX = cellW, i, p, top = 0, bottom = screenH, left = 0, right = screenW;
  var maxColW = Math.floor(screenW / 3);          // a real side column is NARROW (a vertical bar), not a wide background
  var minColH = Math.floor(screenH / 2);          // ...and TALL (spans much of the height), unlike a short edge icon
  for (i = 0; i < drawnPix.length; i++) {
    p = drawnPix[i];
    if (p.w < 8 || p.h < 4) continue;
    if (p.dy <= tolY && p.dy + p.h > top) top = p.dy + p.h;               // top-edge cover (top strip / column tops)
    if (p.w >= screenW - tolX) {                                          // a full-width strip
      if (p.dy + p.h >= screenH - tolY && p.dy > tolY && p.dy < bottom) bottom = p.dy;   // optional bottom strip (below the very top)
      continue;                                                          // full-width strips are never side columns
    }
    if (p.w > maxColW || p.h < minColH) continue;                        // too wide / too short to be a side column (excludes wide ornate borders + small icons)
    if (p.dy + p.h <= tolY) continue;                                    // sits entirely in the top sliver -> not a side column
    if (p.dx <= tolX && p.dx + p.w > left) left = p.dx + p.w;             // left-edge column
    if (p.dx + p.w >= screenW - tolX && p.dx < right) right = p.dx;       // right-edge column
  }
  if (!(left > 0 && right < screenW)) return null;                       // need BOTH edge columns -> else top band / interior decoration
  return { topPx: top, bottomPx: (bottom < screenH) ? (screenH - bottom) : 0, leftPx: left, rightPx: screenW - right };
}

// JSZM_v6FrameRect(frame, win0Xpx, win0Ypx, screenRows, screenCols, cellW, cellH) -> inner text rectangle
// in 1-based CELLS {winTop,winBot,vpCol1,vpCols,topRows,bottomRows}. Insets snap OUTWARD to whole cells so
// text sits strictly inside the pixel border (any sub-cell remainder is covered by the border art on top).
// All pixel inputs are already SCALED; top/left use the LARGER of the border thickness and win0's origin.
function JSZM_v6FrameRect(frame, win0Xpx, win0Ypx, screenRows, screenCols, cellW, cellH) {
  var topPx = Math.max(frame.topPx, win0Ypx || 0);
  var leftPx = Math.max(frame.leftPx, win0Xpx || 0);
  var winTop = Math.ceil(topPx / cellH) + 1;                 // first row whose top edge is at/below the border
  var vpCol1 = Math.ceil(leftPx / cellW) + 1;
  var bottomRows = Math.ceil(frame.bottomPx / cellH);
  var rightCols = Math.ceil(frame.rightPx / cellW);
  var winBot = screenRows - bottomRows;
  var vpCols = (screenCols - rightCols) - vpCol1 + 1;
  if (winBot < winTop) winBot = winTop;
  if (vpCols < 1) vpCols = 1;
  return { winTop: winTop, winBot: winBot, vpCol1: vpCol1, vpCols: vpCols, topRows: winTop - 1, bottomRows: bottomRows };
}

// JSZM_v6WindowCols(winXpx, winWpx, fontWidth) -> { col1, cols } : map a v6 window's DEVICE-pixel left edge +
// width to 1-based terminal cell columns. col1 = floor(winX / fontWidth) + 1; cols = floor(winW / fontWidth)
// (whole cells inside the window). Clamped so col1 >= 1 and cols >= 1. Used to place windowed-text regions
// (Journey's win0 narrative inset + win3 menu panel) from the game's own declared window geometry.
function JSZM_v6WindowCols(winXpx, winWpx, fontWidth) {
  var fw = fontWidth > 0 ? fontWidth : 8;
  var col1 = Math.floor((winXpx > 0 ? winXpx : 0) / fw) + 1;
  var cols = Math.floor((winWpx > 0 ? winWpx : 0) / fw);
  if (col1 < 1) col1 = 1;
  if (cols < 1) cols = 1;
  return { col1: col1, cols: cols };
}

// JSZM_v6Mode(bandRows, screenRows, graphicsAvailable, frame) -> "text"|"cutscene"|"framed"|"border-frame"
// T1 heuristic: no graphics/pictures -> text; a picture band that (nearly) fills the screen with no
// room for text -> cutscene; a top band leaving >= 3 rows (status + 2 narrative) -> framed.
// `frame` (optional) is the enclosing-border descriptor from JSZM_v6ScreenFrame, or null/undefined.
// border-frame is checked BEFORE the band heuristic and is the ONLY new branch: when `frame` is falsy
// (Arthur, and every pre-existing 3-arg call site) the outcome is identical to before.
function JSZM_v6Mode(bandRows, screenRows, graphicsAvailable, frame) {
  if (!graphicsAvailable) return "text";
  if (frame) return "border-frame";
  if (bandRows <= 0) return "text";
  if (bandRows >= screenRows - 2) return "cutscene";
  return "framed";
}

// JSZM_makeApcSurface(opts) -> a SyncTERM-APC pixel surface. Byte-identical to the pre-extraction inline
// emit in JSZM_makePictureBridge: it owns the upload-once bookkeeping (uploaded/maskUploaded/blackLoaded)
// and emits the exact C;S / C;LoadPPM / C;LoadPBM / P;Paste protocol strings the door emitted before.
// opts: { write, clientPrefix, isCached, readPpmBase64, readPbmBase64, blackName, readBlackBase64 }.
//   blit({ file, mask, useMask, srcX, srcY, srcW, srcH, dstX, dstY })  // paint one image sub-rect -> bool drawn
//   fillBlack({ dstX, dstY, w, h })                                     // paint a solid-black rect
//   setForceUpload(v) / forgetUploads()                                // debug-reupload plumbing
function JSZM_makeApcSurface(opts) {
  function apc(body) { return "\x1b_SyncTERM:" + body + "\x1b\\"; }
  var uploaded = {}, maskUploaded = {}, blackLoaded = false, forceUpload = false;
  function ensureUploaded(file) {
    if (uploaded[file]) return true;
    if (!forceUpload && opts.isCached && opts.isCached(file)) { uploaded[file] = true; return true; }  // client already holds it (MD5-matched) -> no C;S
    var b64 = opts.readPpmBase64(file); if (!b64) return false;       // asset missing -> draw nothing; retry next time
    opts.write(apc("C;S;" + opts.clientPrefix + "/" + file + ";" + b64)); uploaded[file] = true; return true;
  }
  function ensureMask(mask) {
    if (maskUploaded[mask]) return true;
    if (!forceUpload && opts.isCached && opts.isCached(mask)) { maskUploaded[mask] = true; return true; }
    var mb64 = opts.readPbmBase64(mask); if (!mb64) return false;
    opts.write(apc("C;S;" + opts.clientPrefix + "/" + mask + ";" + mb64)); maskUploaded[mask] = true; return true;
  }
  function ensureBlack() {                       // upload + load the black source into buffer 1, once
    if (blackLoaded || !opts.blackName) return false;
    opts.write(apc("C;S;" + opts.blackName + ";" + (opts.readBlackBase64 ? opts.readBlackBase64() : "")));
    opts.write(apc("C;LoadPPM;B=1;" + opts.blackName)); blackLoaded = true; return true;
  }
  return {
    kind: "apc",
    blit: function (b) {
      if (!ensureUploaded(b.file)) return false;
      opts.write(apc("C;LoadPPM;B=0;" + opts.clientPrefix + "/" + b.file));   // buffer 0 is shared -> reload before each paste
      // Transparency mask (if any): upload once, then LoadPBM before this paste (single shared mask buffer).
      var useMask = false;
      if (b.useMask && b.mask && opts.readPbmBase64 && ensureMask(b.mask)) { opts.write(apc("C;LoadPBM;" + opts.clientPrefix + "/" + b.mask)); useMask = true; }
      opts.write(apc("P;Paste;SX=" + b.srcX + ";SY=" + b.srcY + ";SW=" + b.srcW + ";SH=" + b.srcH + ";DX=" + b.dstX + ";DY=" + b.dstY +
                     (useMask ? ";MBUF;MX=" + b.srcX + ";MY=" + b.srcY : "") + ";B=0"));
      return true;
    },
    fillBlack: function (r) {
      ensureBlack();
      opts.write(apc("P;Paste;SX=0;SY=0;SW=" + r.w + ";SH=" + r.h + ";DX=" + r.dstX + ";DY=" + r.dstY + ";B=1"));
    },
    canFillBlack: function () { return !!opts.blackName; },   // a black source exists -> clears can black-blit
    setForceUpload: function (v) { forceUpload = !!v; },
    forgetUploads: function () { uploaded = {}; maskUploaded = {}; blackLoaded = false; }
  };
}

// JSZM_makePictureBridge(opts) -> { graphicsAvailable, pictureInfo, drawPicture, erasePicture, bandRows, clearRegion }
// opts: { surface, capable, manifest, scale, originOf, fontHeight, fontWidth, screenWidthPx, screenHeightPx }.
// The protocol emit lives in opts.surface (JSZM_makeApcSurface). For back-compat, if opts.surface is absent
// the bridge builds an APC surface from the legacy I/O opts (write/clientPrefix/isCached/readPpmBase64/
// readPbmBase64/blackName/readBlackBase64), so existing callers keep working byte-identically.
function JSZM_makePictureBridge(opts) {
  var surface = opts.surface || JSZM_makeApcSurface(opts);
  var manifest = opts.manifest;
  var drawnPix = [];            // pixel rects of pictures drawn this screen: {win, dx, dy, w, h}
  var everDrew = false;         // a picture has been blitted at least once this session -> a later empty-screen
                                // erase_window -1 (Arthur's F6 text->graphics return) must full-screen-black
  var lastScenePic = -1;        // last NON-adaptive picture drawn -> selects the adaptive-palette context
  var reblitting = false;       // guard while re-blitting on-screen adaptive pictures (avoids re-entrancy)
  var scale = opts.scale > 1 ? opts.scale : 1;     // integer render scale (assets are supplied pre-scaled)
  var wideBgTopClip = -1;       // screen Y below which OTHER-window content must stay, to protect a wide background's scaled top edge; -1 = inactive
  var wideBgWin = -1;           // the window the active wide background was drawn into (content in OTHER windows is what gets top-clipped)

  function graphicsAvailable() { return !!(opts.capable && manifest); }
  function canBlack() { return surface.canFillBlack ? surface.canFillBlack() : true; }   // surface has a black source -> clears may black-blit

  function pictureInfo(pic) {
    if (!manifest) return null;
    if (pic === 0) return { count: manifest.count, release: manifest.release };
    var p = manifest.pics[pic];
    return p ? { width: p.w * scale, height: p.h * scale } : null;
  }

  function drawPicture(pic, x, y, win) {
    if (!manifest) return;
    var drawNum = pic;
    var isAdaptive = !!(manifest.adaptive && manifest.adaptive[pic]);   // listed in @APal -> recolours with the scene
    // Adaptive-palette substitution: an adaptive picture borrows a pre-rendered variant keyed by the last
    // non-adaptive (scene) picture. A non-adaptive picture sets that palette context -- and, because DOS recolours
    // already-drawn adaptive graphics for free via the shared hardware palette, a NEW palette context re-blits the
    // on-screen adaptive pictures (the banner frame/poles) with their new variants (our RGB model has no shared
    // palette, so we re-paint). See the Blorb-Infocom-extension "Current Palette" rule.
    var triggerReblit = false;
    if (isAdaptive) {
      var rm = manifest.remap && manifest.remap[lastScenePic];
      if (rm && rm[pic]) drawNum = rm[pic];
    } else {
      var prevScene = lastScenePic;
      lastScenePic = pic;
      // Recolour the on-screen adaptive pictures for the new palette -- but DEFER it to after this picture is
      // pushed into drawnPix (below), so reblitAdaptive's coverage test can see it. A full-band background that
      // covers the banner frame (the map's pic 137) must SUPPRESS the re-blit (the banner is hidden behind it),
      // not paint the banner on top of the map.
      triggerReblit = (!reblitting && pic !== prevScene && manifest.remap && manifest.remap[pic]);
    }
    var p = manifest.pics[drawNum];
    if (!p || p.type !== "bitmap" || !p.file) return;   // rects / unknown carry no bitmap
    // v6 draw_picture coords are SIGNED (Z-spec): a picture taller/wider than the window is centered
    // to a negative offset (Arthur does this for full-screen art). Sign-extend, then clip the part
    // that falls off the top/left (advancing the source rect) so the picture renders instead of
    // landing off-screen at an unsigned ~65533.
    var sx = (x > 32767) ? x - 65536 : (x || 0);
    var sy = (y > 32767) ? y - 65536 : (y || 0);
    // Position scale: a native-authored game (Zork Zero) lays out in 320-space, so its window origins +
    // draw coords are scaled to device by posScale; a scaled-authored game (Arthur, posScale 1) is 1:1.
    // The horizontal centring offset is in DEVICE space (added after scaling), so originOf returns the
    // PURE native window origin and the offset is applied here. (posScale 1 -> identical to before.)
    var ps = opts.posScale > 1 ? opts.posScale : 1;
    var o = opts.originOf(win) || { x: 0, y: 0 };
    var dx = ((o.x || 0) + sx) * ps + (opts.offsetPx || 0), dy = ((o.y || 0) + sy) * ps;
    var sw = p.w * scale, sh = p.h * scale, srcx = 0, srcy = 0;
    if (dx < 0) { srcx = -dx; sw += dx; dx = 0; }
    if (dy < 0) { srcy = -dy; sh += dy; dy = 0; }
    // Clip the paste to the screen's right/bottom edge (the left/top clip above already handles negatives).
    // A picture pasted past the screen edge (Arthur's full-width map background lands at DX=2, SW=640 on a
    // 640px screen -> 642) is dropped wholesale by SyncTERM rather than clipped, so trim the source rect.
    // The boundary is the DISPLAY edge, not the content width: when the content is centred in a wider
    // terminal (originOf adds a horizontal offset), dx already includes that offset, so clipping at the
    // bare content width would wrongly cut the right of every centred picture. opts.clipRightPx/clipBottomPx
    // carry the full display extent (= content + both margins); they default to screenWidthPx/HeightPx
    // (offset 0 -> identical to before, e.g. 80-col SyncTERM).
    var clipR = opts.clipRightPx || opts.screenWidthPx, clipB = opts.clipBottomPx || opts.screenHeightPx;
    if (clipR > 0 && dx + sw > clipR) sw = clipR - dx;
    if (clipB > 0 && dy + sh > clipB) sh = clipB - dy;
    // Top-edge protection for a windowed background. A WIDE (full-width), non-adaptive background drawn into
    // its own window (e.g. a framed map/scroll) carries a thin decorative top edge. Content the game overlays
    // from ANOTHER window, positioned to just clear that edge on the native screen, keeps only ~1px clearance
    // once we scale the background up -- so it bites into the now-thicker edge and leaves notches. The bg sets
    // a clip line one scaled edge-row below its top; later content in a DIFFERENT window is top-clipped to it
    // so the edge stays intact (the clipped slice is the same backdrop the content was covering). Same-window
    // content (e.g. a cutscene drawn over its own frame) is left alone. (Reset by the clears.)
    var bgWide = !isAdaptive && opts.screenWidthPx > 0 && (p.w * scale) >= opts.screenWidthPx * 0.8;
    if (bgWide) {
      wideBgTopClip = dy + scale; wideBgWin = win;
    } else if (wideBgTopClip >= 0 && win !== wideBgWin && dy < wideBgTopClip) {
      var clipT = wideBgTopClip - dy;
      srcy += clipT; sh -= clipT; dy = wideBgTopClip;
    }
    if (sw <= 0 || sh <= 0) return;                     // entirely off-screen
    // Emit the actual pixels via the surface (APC by default). The surface owns upload-once + the wire
    // format; it returns false if the asset is missing (no stale buffer-0 paste) -> draw nothing, retry next
    // time, and do NOT push/evict (the picture never landed). useMask is gated on p.mask alone -- the surface
    // decides whether it can actually load the mask PBM.
    if (!surface.blit({ file: p.file, mask: p.mask, useMask: !!p.mask,
        srcX: srcx, srcY: srcy, srcW: sw, srcH: sh, dstX: dx, dstY: dy })) return;
    // A new full-width BACKGROUND drawn into a window replaces a prior full-width background there
    // (e.g. Arthur draws the scene into window 7 over the dismissed map's background but never erases
    // window 7 -- the stale map bg would otherwise linger in drawnPix and inflate the inferred band).
    // "Background" means it spans MOST of the screen width (>= 0.8). A large OVERLAY is not a background and
    // must not evict the background it sits inside: the intro's Merlin cutscene draws pic 3 (480px, 0.75 of a
    // 640px screen) INSIDE the celtic frame pic 2 (584px) -- at the old 0.5 threshold the Merlin counted as a
    // background and evicted the frame, collapsing bandRows so a 25-line cutscene flipped to framed mode and
    // the frame's bottom was overwritten by the text band (chopped). Small overlays (torque, frame poles) are
    // well under the threshold and coexist. ADAPTIVE pictures (the banner frame/poles) are PERSISTENT
    // foreground, not stale backgrounds: a non-adaptive scene bg must not evict them, and the adaptive frame
    // re-blitting must not evict the room background -- so gate both ends on non-adaptive.
    var WIDE = (opts.screenWidthPx || 0) * 0.8;
    if (sw >= WIDE && WIDE > 0 && !isAdaptive) {
      var keep = [], q, e;
      for (q = 0; q < drawnPix.length; q++) {
        e = drawnPix[q];
        if (!e.adaptive && e.win === win && e.w >= WIDE && dx < e.dx + e.w && dx + sw > e.dx && dy < e.dy + e.h && dy + sh > e.dy) continue;  // stale overlapping background -> drop
        keep.push(e);
      }
      drawnPix = keep;
    }
    drawnPix.push({ win: win, dx: dx, dy: dy, w: sw, h: sh, pic: pic, x: x, y: y, adaptive: isAdaptive, variant: drawNum, cleared: false });
    everDrew = true;
    if (triggerReblit) reblitAdaptive();   // now that this picture is in drawnPix, the coverage test below sees it
  }

  // Recolour the on-screen adaptive pictures (banner frame + poles) for the current palette context
  // (lastScenePic). DOS/Amiga do this for free via the shared hardware palette when a room reloads it; we
  // re-paint each tracked adaptive picture, recomputing its variant from the new lastScenePic. Only pictures
  // whose variant actually CHANGES are re-painted (so re-entering the same palette, or a room that draws several
  // scene pictures resolving to the same variant, costs no extra wire traffic). drawnPix is the single source of
  // truth (and is pruned by the clears), so cleared adaptive pictures are never replayed.
  // Is adaptive picture `a` fully hidden behind a freshly-drawn (not-yet-erased) non-adaptive picture? Used so the
  // banner frame is NOT re-painted over a full-band background that covers it (the map's pic 137). A cover whose
  // pixels have since been erased (cleared) does NOT count: after the map is dismissed the stale 137 lingers in
  // drawnPix but is blanked, so back in the room the banner must recolour normally.
  function coveredByFresh(a) {
    var i, f;
    for (i = 0; i < drawnPix.length; i++) {
      f = drawnPix[i];
      if (f.adaptive || f.cleared) continue;
      if (f.dx <= a.dx && f.dx + f.w >= a.dx + a.w && f.dy <= a.dy && f.dy + f.h >= a.dy + a.h) return true;
    }
    return false;
  }
  function reblitAdaptive() {
    var rm = manifest.remap && manifest.remap[lastScenePic], todo = [], keep = [], i, a, nv;
    for (i = 0; i < drawnPix.length; i++) {
      a = drawnPix[i];
      if (a.adaptive && !a.cleared) { nv = (rm && rm[a.pic]) ? rm[a.pic] : a.pic; if (nv !== a.variant && !coveredByFresh(a)) { todo.push(a); continue; } }   // a CLEARED adaptive picture (e.g. the room banner frame blanked by the map toggle's clearBandStrip but kept in drawnPix because no single strip fully covers it) must NOT be re-painted -- otherwise a palette-context change from a map marker resurrects the frame as a ghost at its stale room-view position. Matches the forced-redraw/resume guard below + the line-324 invariant.
      keep.push(a);                  // non-adaptive, adaptive w/ unchanged variant, or one hidden behind a cover -> leave in place
    }
    if (!todo.length) return;        // nothing recolours -> no repaint
    drawnPix = keep;                 // drop the stale-variant rects; the re-blit below re-adds them with new variants
    reblitting = true;
    for (i = 0; i < todo.length; i++) drawPicture(todo[i].pic, todo[i].x, todo[i].y, todo[i].win);
    reblitting = false;
  }

  function erasePicture(pic, x, y, win) { return; }   // pixel-clear handled by clearRegion (later task); @erase_picture is rare

  function bandRows() {          // bottom-most cell-row any drawn picture reaches (0 if none)
    var fh = opts.fontHeight || 16, max = 0, i, bot;
    for (i = 0; i < drawnPix.length; i++) {
      if (drawnPix[i].w < 8) continue;   // skip thin sliver decorations (2px frame edges) -- they shouldn't inflate the text-exclusion band (T2)
      bot = Math.ceil((drawnPix[i].dy + drawnPix[i].h) / fh);
      if (bot > max) max = bot;
    }
    return max;
  }

  function screenFrame() {       // enclosing-border descriptor from this screen's drawn pictures, or null
    var cw = opts.fontWidth || 8, ch = opts.fontHeight || 16;
    return JSZM_v6ScreenFrame(drawnPix, opts.screenWidthPx || 0, opts.screenHeightPx || 0, cw, ch);
  }

  function clearRegion(win) {                     // pixel black-blit each picture in window `win` (win<0 = all)
    wideBgTopClip = -1; wideBgWin = -1;
    // erase_window -1/-2 = clear the ENTIRE screen. The per-picture black-blits below only cover what's tracked
    // in drawnPix, which works while pictures are on screen -- but after a text-only stretch (Arthur's F6 full-
    // screen text mode) the prior erase_window -1 already black-blitted and DROPPED every picture, so drawnPix is
    // empty and the loop below clears nothing. SyncTERM's ESC[2J (console.clear) does NOT clear the APC pixel
    // layer, so the stale text-mode pixels then show through every region the returning (non-full-screen) pictures
    // don't opaquely cover (the black band above the room picture; the gaps in the map). When drawnPix is empty
    // but graphics have already been used this session (everDrew), black the whole pixel screen explicitly so a
    // text->graphics return composites onto a clean black field. GATED so it does NOT fire on the very first
    // erase_window -1 at startup (before any picture, nothing stale to clear, and it would force the screen-sized
    // black upload early); and normal scene/map toggles erase PARTIAL windows (2/5/6), never -1 -- so the common
    // paths are untouched and this only changes the exact buggy case.
    if (win < 0 && everDrew && !drawnPix.length && canBlack() && opts.screenWidthPx > 0 && opts.screenHeightPx > 0) {
      surface.fillBlack({ dstX: 0, dstY: 0, w: opts.screenWidthPx, h: opts.screenHeightPx });
    }
    if (!drawnPix.length || !canBlack()) { drawnPix = (win < 0) ? [] : drawnPix; return; }
    var any = false, i;
    for (i = 0; i < drawnPix.length; i++) if (win < 0 || drawnPix[i].win === win) { any = true; break; }
    if (!any) return;
    var keep = [], r;
    for (i = 0; i < drawnPix.length; i++) {
      r = drawnPix[i];
      if (win < 0 || r.win === win)
        surface.fillBlack({ dstX: r.dx, dstY: r.dy, w: r.w, h: r.h });
      else keep.push(r);
    }
    drawnPix = keep;
  }

  function clearBandStrip(xLo, xHi, yLo, yHi) {   // black-blit the slice of EVERY drawn picture that falls in the
    // erased window's pixel RECTANGLE [xLo,xHi) x [yLo,yHi) -- regardless of which window tagged it. A picture
    // drawn into one window can overlap another (Arthur's full-width map background pic 137 is tagged window 7,
    // yet the map dismiss erases windows 2/5/6 which together span the band); clearRegion only clears same-tagged
    // pictures, so 137 lingered as a coloured remnant. The Y bound is essential: Arthur clears its parser-reply
    // window 3 ("I beg your pardon?") on an empty command, and that window is ZERO-HEIGHT below the screen
    // (y=401,h=0) but spans the band's columns -- without the Y test we black-blitted the room scene out of the
    // band. yLo/yHi default to unbounded (back-compat).
    wideBgTopClip = -1; wideBgWin = -1;
    if (!drawnPix.length || !canBlack()) return;
    if (yLo === undefined) { yLo = -2000000; yHi = 2000000; }
    var i, e, any = false;
    for (i = 0; i < drawnPix.length; i++) { e = drawnPix[i]; if (xHi > e.dx && xLo < e.dx + e.w && yHi > e.dy && yLo < e.dy + e.h) { any = true; break; } }
    if (!any) return;
    var keep = [], bx, bw;
    for (i = 0; i < drawnPix.length; i++) {
      e = drawnPix[i];
      if (xHi <= e.dx || xLo >= e.dx + e.w || yHi <= e.dy || yLo >= e.dy + e.h) { keep.push(e); continue; }   // no rect overlap -> untouched
      bx = (e.dx > xLo) ? e.dx : xLo;
      bw = ((e.dx + e.w < xHi) ? e.dx + e.w : xHi) - bx;
      if (bw > 0) surface.fillBlack({ dstX: bx, dstY: e.dy, w: bw, h: e.h });
      e.cleared = true;     // pixels blanked -> reblitAdaptive's coverage test no longer treats it as a (now-blank) cover
      if (xLo <= e.dx && xHi >= e.dx + e.w) continue;   // fully cleared across its width -> drop the tracking
      keep.push(e);                                      // partially cleared -> keep (a later full redraw rebuilds)
    }
    drawnPix = keep;
  }

  return {
    graphicsAvailable: graphicsAvailable,
    pictureInfo: pictureInfo,
    drawPicture: drawPicture,
    erasePicture: erasePicture,
    bandRows: bandRows,
    screenFrame: screenFrame,
    clearRegion: clearRegion,
    clearBandStrip: clearBandStrip,
    // The ORIGINAL draw_picture calls (pic,x,y,win) for every picture currently on screen, in draw order --
    // the bridge's drawnPix is the single source of truth (maintained through draws, adaptive re-blits,
    // evictions and clears), so this is what the door replays for a forced redraw (Ctrl-R) and captures for
    // the disconnect/resume slot. Replaying these re-resolves each adaptive variant from the scene context,
    // so the recoloured banner survives a redraw/resume instead of reverting to a stale or missing variant.
    screenPicCalls: function() {
      var out = [], i, e;
      for (i = 0; i < drawnPix.length; i++) {
        e = drawnPix[i];
        if (e.cleared) continue;   // its pixels were blanked (e.g. the dismissed map background lingering in drawnPix) -- not visible, so a forced redraw / resume must NOT repaint it
        out.push(e.pic + "," + e.x + "," + e.y + "," + e.win);
      }
      return out;
    },
    // Drop this screen's drawn-rect tracking WITHOUT a black-blit (the caller already cleared the screen) and
    // WITHOUT forgetting cached uploads -- so a forced redraw / on-top reblit can replay draw_picture cleanly,
    // re-using the client-side cache, instead of piling duplicate rects onto drawnPix on every redraw.
    forgetScreen: function() { drawnPix = []; lastScenePic = -1; wideBgTopClip = -1; wideBgWin = -1; },
    // Debug/refresh: drop the per-session "already uploaded" tracking and (with setForceUpload) re-send each
    // file's C;S even if the client claims it cached -- so a forced redraw re-uploads the current screen's
    // PPMs/PBMs from scratch (diagnoses a missing/corrupt client-side cache file). Both delegate to the surface,
    // which owns the upload bookkeeping.
    forgetUploads: function() { if (surface && surface.forgetUploads) surface.forgetUploads(); },
    setForceUpload: function(v) { if (surface && surface.setForceUpload) surface.setForceUpload(v); }
  };
}
