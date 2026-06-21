// Headless tests for the v6 render-mode heuristic. Run: jsexec test/v6mode.js
load(js.exec_dir + "../v6pics.js");
var fails = 0; function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }

// JSZM_v6Mode(bandRows, screenRows, graphicsAvailable) -> "text" | "cutscene" | "framed"
ok(JSZM_v6Mode(0, 24, true) === "text", "no pictures -> text");
ok(JSZM_v6Mode(10, 24, false) === "text", "graphics off -> text");
ok(JSZM_v6Mode(24, 24, true) === "cutscene", "picture fills the screen -> cutscene");
ok(JSZM_v6Mode(23, 24, true) === "cutscene", "picture (near) fills screen -> cutscene");
ok(JSZM_v6Mode(12, 24, true) === "framed", "picture occupies a top band, room for text -> framed");
ok(JSZM_v6Mode(19, 24, true) === "framed", "band leaves >=3 rows (status+2 text) -> framed");

// --- border-frame: a 4th classification, driven by an enclosing-frame descriptor ---
ok(JSZM_v6Mode(20, 24, true, { topPx: 64, bottomPx: 32, leftPx: 72, rightPx: 72 }) === "border-frame",
   "enclosing frame present -> border-frame");
ok(JSZM_v6Mode(20, 24, true, null) === "framed", "explicit null frame -> framed (Arthur unchanged)");
ok(JSZM_v6Mode(20, 24, true) === "framed", "omitted frame arg -> framed (Arthur unchanged)");
ok(JSZM_v6Mode(0, 24, true, null) === "text", "no band, no frame -> text (Arthur unchanged)");
ok(JSZM_v6Mode(24, 24, true, null) === "cutscene", "full band, no frame -> cutscene (Arthur unchanged)");

// --- JSZM_v6ScreenFrame(drawnPix, W, H, cellW, cellH) -> frame descriptor | null ---
// The DEFINING feature of a frame is a LEFT- and RIGHT-EDGE column below the top; a top band alone, or a
// banner with INTERIOR poles (Arthur), is not a frame.
// Top-band-only (Arthur intro): a band at the top, nothing at the sides -> NOT a frame.
ok(JSZM_v6ScreenFrame([{ dx: 0, dy: 0, w: 640, h: 192 }], 640, 400, 8, 16) === null,
   "top band only -> null (Arthur is not a border frame)");
// Arthur churchyard: a banner + "poles" that sit INSIDE the screen (x~164/476 on 640px), not at the edges
// -> NOT a frame (this is the key Arthur-regression guard for the edge-column topology).
ok(JSZM_v6ScreenFrame([
     { dx: 164, dy: 77, w: 312, h: 16 },   // banner (interior, not full width)
     { dx: 164, dy: 161, w: 36, h: 40 },   // left "pole" (interior, x=164 not at edge)
     { dx: 476, dy: 161, w: 36, h: 40 }    // right "pole" (interior, x=476+36=512 not at right edge)
   ], 640, 400, 8, 16) === null,
   "Arthur banner + interior poles -> null (poles are not edge columns)");
// Enclosing frame (Zork Zero): top strip + full-height LEFT/RIGHT edge columns, NO bottom strip -> a frame.
(function () {
  var dp = [
    { dx: 2,   dy: 2,  w: 640, h: 68  },   // top strip (full width, at the top edge)
    { dx: 2,   dy: 70, w: 72,  h: 332 },   // left-edge column (reaches the bottom; no separate bottom strip)
    { dx: 568, dy: 70, w: 74,  h: 332 }    // right-edge column
  ];
  var f = JSZM_v6ScreenFrame(dp, 640, 400, 8, 16);
  ok(f && f.topPx === 70 && f.bottomPx === 0 && f.leftPx === 74 && f.rightPx === 72,
     "ZZ top strip + edge columns, no bottom -> frame {top70,bottom0,left74,right72}");
})();
// A frame WITH a bottom strip (a hypothetical future game) reports its bottom thickness too.
(function () {
  var dp = [
    { dx: 0,   dy: 0,   w: 640, h: 68  },   // top strip
    { dx: 0,   dy: 380, w: 640, h: 20  },   // bottom strip
    { dx: 0,   dy: 68,  w: 72,  h: 312 },   // left column
    { dx: 568, dy: 68,  w: 72,  h: 312 }    // right column
  ];
  var f = JSZM_v6ScreenFrame(dp, 640, 400, 8, 16);
  ok(f && f.topPx === 68 && f.bottomPx === 20 && f.leftPx === 72 && f.rightPx === 72,
     "top+bottom+sides -> frame {68,20,72,72}");
})();
// Sliver decorations (2px edges) are ignored, so a top band with a thin edge line is still not a frame.
ok(JSZM_v6ScreenFrame([{ dx: 0, dy: 0, w: 640, h: 192 }, { dx: 0, dy: 398, w: 640, h: 2 }], 640, 400, 8, 16) === null,
   "thin 2px bottom edge does not make a frame");

// --- JSZM_v6FrameRect(frame, win0Xpx, win0Ypx, screenRows, screenCols, cellW, cellH) -> inner rect (1-based cells)
(function () {
  // Zork Zero @ posScale 2: frame {top70,bottom0,left74,right72}, win0 origin (44,40) native -> (88,80) device.
  var f = { topPx: 70, bottomPx: 0, leftPx: 74, rightPx: 72 };
  var r = JSZM_v6FrameRect(f, 88, 80, 24, 80, 8, 16);
  ok(r.winTop > r.topRows, "text starts strictly below the top border");
  ok(r.vpCol1 > 1, "left cols reserved for the left border");
  ok(r.vpCol1 + r.vpCols - 1 < 80, "right cols reserved for the right border");
  ok(r.winBot >= r.winTop && r.vpCols >= 1, "rectangle is non-degenerate");
  // exact values: top inset max(70,80)=80px -> winTop 6; bottom 0px -> 0 rows -> winBot 24;
  // left inset max(74,88)=88px -> vpCol1 12; right 72px -> 9 cols -> right edge col 71 -> vpCols 60
  ok(r.winTop === 6 && r.winBot === 24 && r.vpCol1 === 12 && r.vpCols === 60,
     "exact ZZ rect: winTop6 winBot24 vpCol1 12 vpCols60");
})();

// --- JSZM_v6WindowCols(winXpx, winWpx, fontWidth) -> { col1, cols } (1-based terminal columns) ---
(function () {
  // Journey win3 (left menu): X=1, W=128, font 8 -> cols 1..16
  var m = JSZM_v6WindowCols(1, 128, 8);
  ok(m.col1 === 1 && m.cols === 16, "win3 left panel: col1 1, cols 16");
  // Journey win0 (right narrative): X=145, W=480, font 8 -> col1 19, cols 60 (right edge col 78)
  var n = JSZM_v6WindowCols(145, 480, 8);
  ok(n.col1 === 19 && n.cols === 60, "win0 narrative: col1 19, cols 60");
  // Degenerate / zero width clamps to 1 col, never below col 1.
  var z = JSZM_v6WindowCols(0, 0, 8);
  ok(z.col1 === 1 && z.cols === 1, "zero window clamps to col1 1, cols 1");
})();

if (fails) { print(fails + " V6MODE FAILURE(S)"); exit(1); }
print("V6MODE OK");
