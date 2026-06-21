// Headless tests for the v6 door picture runtime (v6pics.js). Run: jsexec test/v6pic.js
load(js.exec_dir + "../v6pics.js");

var fails = 0;
function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }

// The protocol emit now lives in JSZM_makeApcSurface; the bridge routes through opts.surface. Build the bridge
// the way the door does -- construct the APC surface from the I/O opts and pass it as surface: -- so these byte
// assertions exercise the EXPLICIT surface path (not just the bridge's back-compat fallback). The surface-owned
// keys are lifted out of opts into the surface; the geometry/manifest keys stay on the bridge. Assertions below
// are unchanged: identical bytes are the proof of byte-identity across the extraction.
function mkBridge(opts) {
  var sfc = JSZM_makeApcSurface({
    write: opts.write, clientPrefix: opts.clientPrefix, isCached: opts.isCached,
    readPpmBase64: opts.readPpmBase64, readPbmBase64: opts.readPbmBase64,
    blackName: opts.blackName, readBlackBase64: opts.readBlackBase64
  });
  return JSZM_makePictureBridge({
    surface: sfc, capable: opts.capable, manifest: opts.manifest, scale: opts.scale,
    originOf: opts.originOf, fontHeight: opts.fontHeight, fontWidth: opts.fontWidth,
    screenWidthPx: opts.screenWidthPx, screenHeightPx: opts.screenHeightPx
  });
}

// --- JSZM_loadManifest: write a temp manifest, load it back ---
(function () {
  var dir = js.exec_dir + "tmp/v6pic_mf";
  if (!file_isdir(dir)) mkdir(dir);
  var f = new File(dir + "/manifest"); if (!f.open("w")) { print("FAIL: could not write temp manifest"); exit(1); }
  f.write("# count=2  release=7\n1  rect    100  50  -\n2  bitmap  292  196  2.ppm\n");
  f.close();

  var m = JSZM_loadManifest(dir);
  ok(m && m.count === 2 && m.release === 7, "header count/release parsed");
  ok(m && m.pics[1] && m.pics[1].type === "rect" && m.pics[1].w === 100 && m.pics[1].h === 50 && m.pics[1].file === null, "rect line parsed");
  ok(m && m.pics[2] && m.pics[2].type === "bitmap" && m.pics[2].w === 292 && m.pics[2].h === 196 && m.pics[2].file === "2.ppm", "bitmap line parsed");

  ok(JSZM_loadManifest(js.exec_dir + "tmp/does_not_exist_v6") === null, "missing dir -> null");
})();

// --- JSZM_loadManifest: mask column + @ remap lines ---
(function () {
  var dir = js.exec_dir + "tmp/v6pic_mf2";
  if (!file_isdir(dir)) mkdir(dir);
  var f = new File(dir + "/manifest"); if (!f.open("w")) { print("FAIL: could not write temp manifest 2"); exit(1); }
  f.write("# count=3  release=27\n" +
          "4  bitmap  130  72  4.ppm  -\n" +
          "54  bitmap  314  84  54.ppm  54.pbm\n" +
          "1005  bitmap  314  84  1005.ppm  1005.pbm\n" +
          "@ 4 54 1005\n@ 4 170 1006\n");
  f.close();
  var m = JSZM_loadManifest(dir);
  ok(m && m.pics[4] && m.pics[4].mask === null, "mask column '-' -> null");
  ok(m && m.pics[54] && m.pics[54].mask === "54.pbm", "mask column filename parsed");
  ok(m && m.remap && m.remap[4] && m.remap[4][54] === 1005, "@ remap (4,54,1005) parsed");
  ok(m && m.remap[4][170] === 1006, "@ remap (4,170,1006) parsed");
  ok(m && m.adaptive && m.adaptive[54] === true && m.adaptive[170] === true, "adaptive set derived from remap adaptivePic");
  ok(m && !m.adaptive[4], "scene pic 4 not marked adaptive");
})();

// --- JSZM_makePictureBridge ---
(function () {
  var manifest = { count: 2, release: 7, pics: {
    1: { type: "rect",   w: 100, h: 50,  file: null    },
    2: { type: "bitmap", w: 292, h: 196, file: "2.ppm" }
  } };
  var emitted = [];
  function freshBridge(capable) {
    emitted = [];
    return mkBridge({
      write: function (s) { emitted.push(s); },
      capable: capable,
      manifest: manifest,
      clientPrefix: "zmachine/arthur",
      readPpmBase64: function (file) { return "B64(" + file + ")"; },
      originOf: function (win) { return { x: 174, y: 40 }; }
    });
  }

  // graphicsAvailable gating
  ok(freshBridge(true).graphicsAvailable() === true, "graphicsAvailable true when capable + manifest");
  ok(freshBridge(false).graphicsAvailable() === false, "graphicsAvailable false when not capable");
  var nullB = mkBridge({ write: function () {}, capable: true, manifest: null,
      clientPrefix: "x", readPpmBase64: function () { return ""; }, originOf: function () { return { x: 0, y: 0 }; } });
  ok(nullB.graphicsAvailable() === false, "graphicsAvailable false when no manifest");
  ok(nullB.pictureInfo(0) === null && nullB.pictureInfo(2) === null, "pictureInfo null when no manifest");

  // pictureInfo
  var b = freshBridge(true);
  var c0 = b.pictureInfo(0);
  ok(c0 && c0.count === 2 && c0.release === 7, "pictureInfo(0) = catalogue count/release");
  var i2 = b.pictureInfo(2);
  ok(i2 && i2.width === 292 && i2.height === 196, "pictureInfo(2) = bitmap dims");
  ok(b.pictureInfo(99) === null, "pictureInfo(unknown) = null");

  // drawPicture bitmap: upload + LoadPPM + Paste, DX/DY = origin + (x,y)
  b = freshBridge(true);
  b.drawPicture(2, 10, 5, 0);   // x=10,y=5 ; origin (174,40) -> DX=184, DY=45
  ok(emitted.length === 3, "first draw emits upload+LoadPPM+Paste (got " + emitted.length + ")");
  ok(emitted[0].indexOf("C;S;zmachine/arthur/2.ppm;B64(2.ppm)") >= 0, "upload uses client path + base64");
  ok(emitted[1].indexOf("C;LoadPPM;B=0;zmachine/arthur/2.ppm") >= 0, "LoadPPM into buffer 0");
  ok(emitted[2].indexOf("P;Paste;SX=0;SY=0;SW=292;SH=196;DX=184;DY=45;B=0") >= 0, "Paste at origin+offset");

  // second draw of same picture: NO re-upload (upload-once), still LoadPPM + Paste
  b.drawPicture(2, 0, 0, 0);
  ok(emitted.length === 5, "second draw of same pic skips upload (2 more emits, total 5; got " + emitted.length + ")");
  ok(emitted[3].indexOf("C;LoadPPM") >= 0 && emitted[4].indexOf("P;Paste") >= 0, "second draw still LoadPPM+Paste");

  // rect picture: no-op
  b = freshBridge(true);
  b.drawPicture(1, 0, 0, 0);
  ok(emitted.length === 0, "drawPicture(rect) emits nothing");

  // erasePicture: best-effort no-op, must not throw or emit
  b = freshBridge(true);
  b.erasePicture(2, 0, 0, 0);
  ok(emitted.length === 0, "erasePicture is a no-op (emits no APC)");

  // missing asset: readPpmBase64 -> "" must emit nothing (no stale buffer-0 paste), and retry later
  (function () {
    var em2 = [], ready = false;
    var mb = mkBridge({
      write: function (s) { em2.push(s); }, capable: true, manifest: manifest,
      clientPrefix: "zmachine/arthur",
      readPpmBase64: function (f) { return ready ? "B64(" + f + ")" : ""; },
      originOf: function () { return { x: 0, y: 0 }; }
    });
    mb.drawPicture(2, 0, 0, 0);
    ok(em2.length === 0, "missing asset draws nothing (got " + em2.length + ")");
    ready = true; em2 = [];
    mb.drawPicture(2, 0, 0, 0);
    ok(em2.length === 3 && em2[0].indexOf("C;S;") >= 0, "after asset appears, draw uploads+loads+pastes (retry worked)");
  })();

  // isCached (client-cache skip): a cached file is NOT re-uploaded (no C;S) but is still LoadPPM'd + pasted;
  // an uncached file IS uploaded.
  (function () {
    var mf = { count: 1, release: 7, pics: { 2: { type: "bitmap", w: 292, h: 196, file: "2.ppm", mask: "2.pbm" } },
               adaptive: {}, remap: {} };
    var em = [], cachedSet = { "2.ppm": true };   // client holds the PPM, NOT the mask
    var cb = mkBridge({
      write: function (s) { em.push(s); }, capable: true, manifest: mf, clientPrefix: "z",
      readPpmBase64: function (f) { return "PPM(" + f + ")"; },
      readPbmBase64: function (f) { return "PBM(" + f + ")"; },
      isCached: function (f) { return !!cachedSet[f]; },
      originOf: function () { return { x: 0, y: 0 }; }
    });
    cb.drawPicture(2, 0, 0, 0);
    var txt = em.join("\n");
    ok(txt.indexOf("C;S;z/2.ppm") < 0, "cached PPM not re-uploaded (no C;S)");
    ok(txt.indexOf("C;LoadPPM;B=0;z/2.ppm") >= 0, "cached PPM still LoadPPM'd from client cache");
    ok(/P;Paste/.test(txt), "cached PPM still pasted");
    ok(txt.indexOf("C;S;z/2.pbm;PBM(2.pbm)") >= 0, "uncached mask IS uploaded");
    ok(txt.indexOf("C;LoadPBM;z/2.pbm") >= 0, "uncached mask LoadPBM'd before paste");
  })();
})();


// --- bandRows(): bottom-most drawn-picture cell-row (ceil((dy+h)/fontHeight)), 0 when none ---
(function () {
  var manifest = { count: 1, release: 0, pics: { 2: { type: "bitmap", w: 292, h: 196, file: "2.ppm" } } };
  var b = mkBridge({
    write: function () {}, capable: true, manifest: manifest, clientPrefix: "z",
    readPpmBase64: function () { return "B"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16
  });
  ok(b.bandRows() === 0, "bandRows 0 when nothing drawn");
  b.drawPicture(2, 175, 95, 0);                 // dy=95, h=196 -> bottom px 291 -> ceil(291/16)=19
  ok(b.bandRows() === 19, "bandRows = bottom cell-row of the drawn picture (got " + b.bandRows() + ")");
})();

// --- full-width background replacement: a new wide background drawn into a window replaces a prior
//     wide background there, so a stale background can't inflate the inferred band. (Arthur redraws the
//     scene into window 7 over the dismissed map's background but never erases window 7; the map bg
//     used to linger and push the status bar down one row.) Small overlays coexist untouched. ---
(function () {
  var manifest = { count: 3, release: 0, pics: {
    137: { type: "bitmap", w: 320, h: 96, file: "137.ppm" },   // map background (640px wide @2x)
    54:  { type: "bitmap", w: 314, h: 84, file: "54.ppm"  },   // scene background (628px wide @2x)
    6:   { type: "bitmap", w: 21,  h: 21, file: "6.ppm"   }    // small overlay (torque, 42px @2x)
  }, adaptive: {}, remap: {} };
  var b = mkBridge({
    write: function () {}, capable: true, manifest: manifest, clientPrefix: "z",
    readPpmBase64: function () { return "B"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, scale: 2, screenWidthPx: 640
  });
  b.drawPicture(137, 0, 2, 7);                  // map bg, dy=2, sh=192 -> bottom 194 -> ceil/16=13
  ok(b.bandRows() === 13, "map bg alone -> band 13 (got " + b.bandRows() + ")");
  b.drawPicture(54, 0, 10, 7);                  // scene bg over it, dy=10, sh=168 -> bottom 178 -> 12
  ok(b.bandRows() === 12, "new wide bg drops stale wide bg in same window -> band 12 (got " + b.bandRows() + ")");
  b.drawPicture(137, 0, 2, 7);                  // back to map -> scene bg dropped, band 13 again
  ok(b.bandRows() === 13, "replacement is symmetric (map again -> band 13; got " + b.bandRows() + ")");
  // a small overlay is never wide -> it neither triggers removal nor gets removed by a later wide bg
  b.drawPicture(6, 0, 200, 7);                  // dy=200, sh=42 -> bottom 242 -> ceil/16=16
  ok(b.bandRows() === 16, "small overlay coexists, its extent counts (got " + b.bandRows() + ")");
  b.drawPicture(54, 0, 10, 7);                  // redraw scene bg: drops map bg, KEEPS the overlay
  ok(b.bandRows() === 16, "redrawing a wide bg keeps the small overlay (band 16; got " + b.bandRows() + ")");
  // a small overlay in a DIFFERENT window never removes another window's background
  b.drawPicture(6, 0, 0, 2);
  ok(b.bandRows() === 16, "overlay in window 2 leaves window 7's bg intact (got " + b.bandRows() + ")");
})();

// --- a LARGE overlay must not evict the wider BACKGROUND it sits inside (intro Merlin cutscene chop) ---
(function () {
  var manifest = { count: 2, release: 0, pics: {
    2: { type: "bitmap", w: 292, h: 196, file: "2.ppm" },   // celtic frame: 584px @2x (a background, 0.91 of 640)
    3: { type: "bitmap", w: 240, h: 150, file: "3.ppm" }    // Merlin overlay: 480px @2x (0.75 of 640 -> NOT a bg)
  }, adaptive: {}, remap: {} };
  var b = mkBridge({
    write: function () {}, capable: true, manifest: manifest, clientPrefix: "z",
    readPpmBase64: function () { return "B"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, scale: 2, screenWidthPx: 640
  });
  b.drawPicture(2, 0, 0, 0);                    // frame: sh=392 -> bottom 392 -> ceil/16 = 25 (fills the screen)
  ok(b.bandRows() === 25, "cutscene frame -> band 25 (got " + b.bandRows() + ")");
  b.drawPicture(3, 0, 0, 0);                    // Merlin inside it (480px < 0.8*640) must NOT evict the frame
  ok(b.bandRows() === 25, "a narrower overlay does NOT evict the wider frame -> band stays 25 (got " + b.bandRows() + ") -- else a 25-line cutscene flips to framed mode and the frame's bottom is chopped");
})();

// --- clearRegion(win): black-blit each tracked pixel rect (upload+load black once, B=1) ---
(function () {
  var manifest = { count: 2, release: 0, pics: {
    2: { type: "bitmap", w: 292, h: 196, file: "2.ppm" },
    3: { type: "bitmap", w: 100, h: 50,  file: "3.ppm" }
  } };
  var em = [];
  var b = mkBridge({
    write: function (s) { em.push(s); }, capable: true, manifest: manifest, clientPrefix: "z",
    readPpmBase64: function () { return "B"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, blackName: "z/black", readBlackBase64: function () { return "BLK"; }
  });
  b.drawPicture(2, 175, 95, 0);                 // pixel rect (175,95,292,196) in win 0
  em = [];
  b.clearRegion(-1);                            // whole screen
  ok(em.length === 3, "clearRegion emits upload+load+paste first time (got " + em.length + ")");
  ok(em[0].indexOf("C;S;z/black;BLK") >= 0, "uploads the black source once");
  ok(em[1].indexOf("C;LoadPPM;B=1;z/black") >= 0, "loads black into buffer 1");
  ok(em[2].indexOf("P;Paste;SX=0;SY=0;SW=292;SH=196;DX=175;DY=95;B=1") >= 0, "black-pastes the rect from B=1");
  em = [];
  b.clearRegion(-1);
  ok(em.length === 0, "clearRegion clears its tracking (second call no-op)");
  b.drawPicture(2, 0, 0, 0); b.drawPicture(3, 10, 10, 0);
  em = [];
  b.clearRegion(-1);
  var ups = 0, k; for (k = 0; k < em.length; k++) if (em[k].indexOf("C;S;") >= 0) ups++;
  ok(ups === 0, "black NOT re-uploaded on a later clear (cached)");
  var pastes = 0; for (k = 0; k < em.length; k++) if (em[k].indexOf("P;Paste") >= 0) pastes++;
  ok(pastes === 2, "two pictures -> two black pastes (got " + pastes + ")");
  var b2 = mkBridge({
    write: function (s) {}, capable: true, manifest: manifest, clientPrefix: "z",
    readPpmBase64: function () { return "B"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, blackName: "z/black", readBlackBase64: function () { return "BLK"; }
  });
  b2.drawPicture(2, 0, 0, 5);
  b2.clearRegion(0);
  ok(b2.bandRows() > 0, "clearRegion(non-matching win) keeps the rect");
})();

// --- clearBandStrip(xLo,xHi): black-blit cross-window picture pixels by column (#map-remnant) ---
(function () {
  var manifest = { count: 2, release: 0, pics: {
    137: { type: "bitmap", w: 640, h: 96, file: "137.ppm" },   // full-width map background
    9:   { type: "bitmap", w: 20,  h: 20, file: "9.ppm"   }     // small overlay
  } };
  var em = [];
  function mk() { em = []; return mkBridge({
    write: function (s) { em.push(s); }, capable: true, manifest: manifest, clientPrefix: "z",
    readPpmBase64: function () { return "B"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, blackName: "z/black", readBlackBase64: function () { return "BLK"; } }); }
  function pastes(arr) { var p = [], k; for (k = 0; k < arr.length; k++) if (arr[k].indexOf("P;Paste") >= 0) p.push(arr[k]); return p; }

  var b = mk();
  b.drawPicture(137, 0, 0, 7);              // full-width bg tagged WINDOW 7, rect (0,0,640,96)
  em = [];
  b.clearBandStrip(0, 28);                  // erase a left-panel-width column (x 0..28)
  var p = pastes(em);
  ok(p.length === 1, "clearBandStrip black-pastes the overlapping cross-window picture (got " + p.length + ")");
  ok(p[0].indexOf("SW=28;SH=96;DX=0;DY=0;B=1") >= 0, "clips the paste to column [0,28) at full picture height (got " + p[0] + ")");
  ok(b.bandRows() > 0, "a partial-width clear keeps the picture tracked");

  var b2 = mk();
  b2.drawPicture(137, 0, 0, 7);
  em = [];
  b2.clearBandStrip(0, 640);                // covers the whole picture width -> dropped
  ok(b2.bandRows() === 0, "a full-width clearBandStrip drops the tracking");

  var b3 = mk();
  b3.drawPicture(9, 300, 0, 2);             // small overlay at x 300..320, tagged win 2
  em = [];
  b3.clearBandStrip(0, 28);                 // no column overlap
  ok(pastes(em).length === 0, "clearBandStrip skips pictures outside the column");
})();

// --- adaptive-palette substitution + transparency mask ---
(function () {
  var manifest = { count: 3, release: 27,
    pics: {
      4:    { type: "bitmap", w: 130, h: 72, file: "4.ppm",    mask: null       },
      54:   { type: "bitmap", w: 314, h: 84, file: "54.ppm",   mask: "54.pbm"   },
      1005: { type: "bitmap", w: 314, h: 84, file: "1005.ppm", mask: "1005.pbm" }
    },
    adaptive: { 54: true },
    remap: { 4: { 54: 1005 } }
  };
  var emitted = [];
  function bridge() {
    emitted = [];
    return mkBridge({
      write: function (s) { emitted.push(s); },
      capable: true, manifest: manifest, clientPrefix: "zmachine/arthur",
      readPpmBase64: function (fn) { return "PPM(" + fn + ")"; },
      readPbmBase64: function (fn) { return "PBM(" + fn + ")"; },
      originOf: function () { return { x: 0, y: 0 }; }
    });
  }

  // Scene pic 4 (non-adaptive) then frame pic 54 (adaptive) -> variant 1005, drawn with its mask.
  var b = bridge();
  b.drawPicture(4, 0, 0, 1);
  var sceneTxt = emitted.join("\n");
  ok(sceneTxt.indexOf("C;LoadPPM;B=0;zmachine/arthur/4.ppm") >= 0, "scene pic 4 PPM loaded");
  ok(sceneTxt.indexOf("MBUF") < 0, "scene pic 4 (no mask) not pasted with MBUF");
  var n0 = emitted.length;
  b.drawPicture(54, 0, 0, 1);
  var post = emitted.slice(n0).join("\n");
  ok(post.indexOf("zmachine/arthur/1005.ppm") >= 0, "adaptive 54 after scene 4 -> variant 1005 PPM");
  ok(post.indexOf("zmachine/arthur/54.ppm") < 0, "original 54.ppm not used when variant resolves");
  ok(post.indexOf("C;S;zmachine/arthur/1005.pbm;PBM(1005.pbm)") >= 0, "variant mask uploaded once");
  ok(post.indexOf("C;LoadPBM;zmachine/arthur/1005.pbm") >= 0, "variant mask LoadPBM before paste");
  ok(/P;Paste;[^\n]*;MBUF;/.test(post), "variant pasted with MBUF");

  // Adaptive draw with no scene context -> falls back to the adaptive picture itself.
  b = bridge();
  b.drawPicture(54, 0, 0, 1);
  var fb = emitted.join("\n");
  ok(fb.indexOf("zmachine/arthur/54.ppm") >= 0, "adaptive with no scene -> original 54.ppm fallback");
  ok(fb.indexOf("1005") < 0, "no variant substituted without scene context");
  ok(/P;Paste;[^\n]*;MBUF;/.test(fb), "fallback 54 still pasted with its own mask (MBUF)");

  // Mask PBM uploaded only once across repeated draws.
  b = bridge();
  b.drawPicture(4, 0, 0, 1); b.drawPicture(54, 0, 0, 1);
  b.drawPicture(4, 0, 0, 1); b.drawPicture(54, 0, 0, 1);
  var uploads = emitted.join("\n").split("C;S;zmachine/arthur/1005.pbm").length - 1;
  ok(uploads === 1, "variant mask PBM uploaded exactly once across draws (got " + uploads + ")");
})();

// --- #114 adaptive RE-BLIT: a new scene palette recolours on-screen adaptive pictures ---
(function () {
  var manifest = { count: 3, release: 27,
    pics: {
      4:    { type: "bitmap", w: 130, h: 72, file: "4.ppm"    },   // scene/room A (non-adaptive)
      7:    { type: "bitmap", w: 130, h: 72, file: "7.ppm"    },   // scene/room B (non-adaptive)
      54:   { type: "bitmap", w: 314, h: 84, file: "54.ppm"   },   // adaptive banner frame
      1005: { type: "bitmap", w: 314, h: 84, file: "1005.ppm" },   // 54 under scene 4
      1007: { type: "bitmap", w: 314, h: 84, file: "1007.ppm" }    // 54 under scene 7
    },
    adaptive: { 54: true },
    remap: { 4: { 54: 1005 }, 7: { 54: 1007 } }
  };
  var em = [];
  var b = mkBridge({
    write: function (s) { em.push(s); }, capable: true, manifest: manifest, clientPrefix: "z",
    readPpmBase64: function () { return "B"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, blackName: "z/black", readBlackBase64: function () { return "BLK"; }
  });
  function loaded(file) { var k; for (k = 0; k < em.length; k++) if (em[k].indexOf("C;LoadPPM;B=0;z/" + file) >= 0) return true; return false; }

  b.drawPicture(4, 0, 0, 7);                 // enter room A
  b.drawPicture(54, 0, 0, 7);                // banner -> scene-4 variant 1005
  ok(loaded("1005.ppm"), "banner first drawn as the scene-4 variant 1005");

  em = [];
  b.drawPicture(7, 0, 0, 7);                 // move to room B (new palette) -> re-blit the on-screen banner
  ok(loaded("1007.ppm"), "a new scene palette re-blits the on-screen banner as variant 1007");
  ok(!loaded("1005.ppm"), "the stale scene-4 variant 1005 is not re-blitted");

  // redrawing the SAME scene must not re-blit (no spurious repaint)
  em = [];
  b.drawPicture(7, 0, 0, 7);
  ok(!loaded("1007.ppm") && !loaded("1005.ppm"), "re-entering the same scene does not re-blit the banner");

  // a scene with no remap entry leaves the banner as-is (no variant to switch to)
  em = [];
  b.drawPicture(99, 0, 0, 7);                // 99 not in remap
  ok(em.length === 0 || (!loaded("1005.ppm") && !loaded("1007.ppm")), "a scene without a remap entry does not re-blit");
})();

// --- #114 map overlay: a full-width cover (the map bg) HIDES the banner -> NO re-blit; a cleared cover is ignored ---
(function () {
  var manifest = { count: 7, release: 27,
    pics: {
      4:    { type: "bitmap", w: 130, h: 72, file: "4.ppm"    },   // room scene A (small, never covers the frame)
      7:    { type: "bitmap", w: 130, h: 72, file: "7.ppm"    },   // room scene B (small)
      137:  { type: "bitmap", w: 320, h: 96, file: "137.ppm"  },   // full-width map background (640@2x; CONTAINS the frame)
      54:   { type: "bitmap", w: 314, h: 84, file: "54.ppm"   },   // adaptive banner frame (628@2x)
      1005: { type: "bitmap", w: 314, h: 84, file: "1005.ppm" },   // 54 under scene 4
      1007: { type: "bitmap", w: 314, h: 84, file: "1007.ppm" },   // 54 under scene 7
      1147: { type: "bitmap", w: 314, h: 84, file: "1147.ppm" }    // 54 under map 137
    },
    adaptive: { 54: true },
    remap: { 4: { 54: 1005 }, 7: { 54: 1007 }, 137: { 54: 1147 } }
  };
  var em = [];
  var b = mkBridge({
    write: function (s) { em.push(s); }, capable: true, manifest: manifest, clientPrefix: "z",
    screenWidthPx: 640, screenHeightPx: 400, scale: 2,
    readPpmBase64: function () { return "B"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, blackName: "z/black", readBlackBase64: function () { return "BLK"; }
  });
  function loaded(file) { var k; for (k = 0; k < em.length; k++) if (em[k].indexOf("C;LoadPPM;B=0;z/" + file) >= 0) return true; return false; }

  b.drawPicture(4, 0, 0, 7);                 // room A
  b.drawPicture(54, 0, 0, 7);                // banner -> scene-4 variant 1005

  // the map background (full-width, geometrically contains the frame) must NOT re-blit the banner on top of itself
  em = [];
  b.drawPicture(137, 0, 0, 7);
  ok(loaded("137.ppm"), "map background drawn");
  ok(!loaded("1147.ppm"), "banner NOT re-blitted over the full-width map cover (it is hidden behind it)");

  // once the band is erased (map dismissed), the lingering map bg is 'cleared' -> ignored as a cover -> banner recolours
  em = [];
  b.clearBandStrip(0, 320, 0, 200);          // partial erase marks the lingering map bg cleared (kept, not dropped)
  b.drawPicture(7, 0, 0, 7);                 // re-enter a room (scene B, new palette)
  ok(loaded("1007.ppm"), "after the map is erased, a new scene recolours the banner (cleared cover ignored)");

  // Ctrl-R / resume replay (screenPicCalls) must OMIT the blanked map background -- otherwise a forced redraw
  // repaints the dismissed map scroll behind the room's banner + scene.
  var replay = b.screenPicCalls().join(" ");
  ok(replay.indexOf("137,") < 0, "Ctrl-R/resume replay omits the blanked (cleared) map background");
  ok(replay.indexOf("54,") >= 0, "Ctrl-R/resume replay still includes the recoloured banner frame");
})();

// --- #115 screenPicCalls() + adaptive-aware eviction (resume/Ctrl-R single source of truth) ---
(function () {
  var manifest = { count: 3, release: 0,
    pics: {
      40:   { type: "bitmap", w: 320, h: 80, file: "40.ppm"  },   // wide room background A (non-adaptive)
      41:   { type: "bitmap", w: 320, h: 80, file: "41.ppm"  },   // wide room background B (non-adaptive)
      54:   { type: "bitmap", w: 320, h: 84, file: "54.ppm"  },   // adaptive banner frame (wide)
      1005: { type: "bitmap", w: 320, h: 84, file: "1005.ppm" },
      1007: { type: "bitmap", w: 320, h: 84, file: "1007.ppm" }
    },
    adaptive: { 54: true },
    remap: { 40: { 54: 1005 }, 41: { 54: 1007 } }
  };
  function mk() { return mkBridge({
    write: function () {}, capable: true, manifest: manifest, clientPrefix: "z", screenWidthPx: 320,
    readPpmBase64: function () { return "B"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, blackName: "z/black", readBlackBase64: function () { return "BLK"; } }); }

  // screenPicCalls reports the ORIGINAL pic/x/y/win (not the resolved variant), in draw order.
  var b = mk();
  b.drawPicture(40, 0, 0, 7);                 // room bg A
  b.drawPicture(54, 0, 4, 7);                 // banner -> variant 1005 (but reported as 54)
  var calls = b.screenPicCalls();
  ok(calls.length === 2, "screenPicCalls lists both on-screen pictures (got " + calls.length + ")");
  ok(calls.join(";").indexOf("54,0,4,7") >= 0, "banner reported by ORIGINAL pic number 54 (re-resolved on replay)");
  ok(calls.join(";").indexOf("1005") < 0, "the resolved variant is NOT what gets captured");

  // a new WIDE non-adaptive bg evicts the old non-adaptive bg, but NOT the adaptive banner
  b.drawPicture(41, 0, 0, 7);                 // room bg B (wide) -- evicts 40, keeps 54
  calls = b.screenPicCalls().join(";");
  ok(calls.indexOf("40,") < 0, "wide non-adaptive bg evicts the prior non-adaptive bg (40 gone)");
  ok(calls.indexOf("54,0,4,7") >= 0, "the adaptive banner is NOT evicted by a wide room bg");
  ok(calls.indexOf("41,0,0,7") >= 0, "the new room bg is tracked");

  // re-blitting the adaptive banner (when the scene changed to 41) must not evict the room bg 41
  ok(calls.indexOf("41,0,0,7") >= 0 && b.screenPicCalls().join(";").indexOf("54,") >= 0,
     "scene change re-blits the banner without evicting the room background");
})();

// --- bridge scale: pictureInfo ×scale, Paste SW/SH ×scale ---
(function () {
  var manifest = { count: 1, release: 27, pics: { 5: { type: "bitmap", w: 130, h: 72, file: "5.ppm", mask: null } }, adaptive: {}, remap: {} };
  var emitted = [];
  var b = mkBridge({
    write: function (s) { emitted.push(s); }, capable: true, manifest: manifest, clientPrefix: "z",
    readPpmBase64: function () { return "P"; }, originOf: function () { return { x: 0, y: 0 }; }, scale: 2
  });
  var pi = b.pictureInfo(5);
  ok(pi && pi.width === 260 && pi.height === 144, "pictureInfo returns native×scale (260x144)");
  b.drawPicture(5, 0, 0, 0);
  ok(emitted.join("\n").indexOf("SW=260;SH=144") >= 0, "Paste uses scaled SW/SH");
})();

// --- screen-edge clip: a paste whose destination overruns the screen right/bottom edge is trimmed
//     (SyncTERM drops an over-edge paste wholesale -- Arthur's full-width map background lands at DX=2,
//     SW=640 on a 640px screen = 642, which vanished entirely until clipped). ---
(function () {
  var manifest = { count: 2, release: 0, pics: {
    7: { type: "bitmap", w: 320, h: 96,  file: "7.ppm", mask: null },   // full-width bg (640x192 @2x)
    8: { type: "bitmap", w: 130, h: 72,  file: "8.ppm", mask: null }    // small overlay
  }, adaptive: {}, remap: {} };
  var emitted = [];
  var b = mkBridge({
    write: function (s) { emitted.push(s); }, capable: true, manifest: manifest, clientPrefix: "z",
    readPpmBase64: function () { return "P"; }, originOf: function () { return { x: 1, y: 1 }; },
    scale: 2, screenWidthPx: 640, screenHeightPx: 400
  });
  // bg drawn at raw (1,1): origin(1,1)+ (1,1) -> DX=2 DY=2, SW=640 SH=192 -> right edge 642 > 640: trim SW to 638
  b.drawPicture(7, 1, 1, 0);
  var txt = emitted.join("\n");
  ok(/DX=2;DY=2;/.test(txt), "bg pasted at DX=2 DY=2 (got " + txt.replace(/\n/g, " | ") + ")");
  ok(txt.indexOf("SW=638;SH=192;") >= 0, "over-edge width trimmed 640->638 to fit the screen");
  ok(txt.indexOf("SW=640;") < 0, "no paste overruns the screen width");
  // a fully-on-screen picture is NOT trimmed
  emitted = [];
  b.drawPicture(8, 0, 0, 0);     // origin(1,1)+(0,0) -> DX=1 DY=1, SW=260 SH=144 -> 261 <= 640: unchanged
  ok(emitted.join("\n").indexOf("SW=260;SH=144;") >= 0, "in-bounds picture pasted untrimmed");
})();

// --- map top-edge clip: window-2 content is clipped below a wide bg's scaled top edge (protects the map
//     scroll's 1-row top edge from the pic-147 fill-blocks); reset by the clears ---
(function () {
  var manifest = { count: 3, release: 1, pics: {
    137: { type: "bitmap", w: 320, h: 96, file: "137.ppm" },   // wide map background (640@2x)
    147: { type: "bitmap", w: 8,  h: 7,  file: "147.ppm" },    // window-2 fill block at the top
    9:   { type: "bitmap", w: 13, h: 13, file: "9.ppm" }       // window-2 content lower down
  }, adaptive: {}, remap: {} };
  var em = [];
  var b = mkBridge({
    write: function (s) { em.push(s); }, capable: true, manifest: manifest, clientPrefix: "z",
    screenWidthPx: 640, screenHeightPx: 400, scale: 2,
    readPpmBase64: function () { return "P"; }, originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, blackName: "z/black", readBlackBase64: function () { return "BLK"; }
  });
  function lastPaste() { var k; for (k = em.length - 1; k >= 0; k--) if (em[k].indexOf("P;Paste") >= 0) return em[k]; return ""; }

  em = []; b.drawPicture(137, 0, 2, 7);                 // wide bg at DY=2 -> clip line = 2 + scale(2) = 4
  ok(lastPaste().indexOf("DY=2;") >= 0, "wide map bg drawn at DY=2 (establishes the top-edge clip line)");

  em = []; b.drawPicture(147, 0, 3, 2);                 // win-2 block at DY=3 (in the scaled top edge) -> clipped to 4
  var pc = lastPaste();
  ok(pc.indexOf("DY=4;") >= 0, "win-2 content above the clip line is pushed below the scroll top edge (DY=4)");
  ok(pc.indexOf("SY=1;") >= 0 && pc.indexOf("SH=13;") >= 0, "clip advances the source row and shrinks the height");

  em = []; b.drawPicture(9, 0, 20, 2);                  // win-2 content well below -> untouched
  ok(lastPaste().indexOf("DY=20;") >= 0, "win-2 content below the clip line is left unclipped");

  b.clearBandStrip(0, 640, 0, 200);                     // map dismissed -> clip line reset
  em = []; b.drawPicture(147, 0, 3, 2);
  ok(lastPaste().indexOf("DY=3;") >= 0, "clip line reset by clearBandStrip (no stale clip after the map is dismissed)");
})();

if (fails) { print(fails + " V6PIC UNIT FAILURE(S)"); exit(1); }
print("V6PIC UNIT OK");
