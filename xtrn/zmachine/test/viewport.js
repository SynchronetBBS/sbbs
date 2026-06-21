// Headless tests for v6 scaled-viewport geometry. Run: jsexec test/viewport.js
load(js.exec_dir + "../viewport.js");
var fails = 0;
function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }

// 80x25 (640x400), native 320x200: 2x fills the display exactly.
var a = JSZM_v6Viewport(640, 80, 8, 25, 16, 320, 200);
ok(a.scale === 2 && a.widthPx === 640 && a.cols === 80 && a.offsetPx === 0, "80x25/320: scale 2, 640px/80col, no offset");

// 80x24 (640x384): 2x of 200=400 overflows 384 by 16px (<= ~1.5-row tolerance) -> still 2x (bottom crop).
var b = JSZM_v6Viewport(640, 80, 8, 24, 16, 320, 200);
ok(b.scale === 2 && b.cols === 80, "80x24/320: scale 2 with bottom crop");

// Short 80-col terminal (12 rows): width still fits 2x, so stay 2x (scale is width-driven; the short
// height clips at the bottom). A scaled-down 40-col screen would just garble Arthur's fixed 640-space layout.
var c = JSZM_v6Viewport(640, 80, 8, 12, 16, 320, 200);
ok(c.scale === 2 && c.widthPx === 640 && c.cols === 80 && c.offsetCols === 0, "short 80-col: stay 2x (width-driven), bottom clips");

// Narrow terminal (60 cols = 480px): floor(480/320)=1 -> native-center.
var d = JSZM_v6Viewport(480, 60, 8, 25, 16, 320, 200);
ok(d.scale === 1 && d.widthPx === 320 && d.cols === 40 && d.offsetCols === 10, "narrow term: 1x, 40 cols, offset 10");

// No native dims (no graphics) -> default 320x200.
var e = JSZM_v6Viewport(640, 80, 8, 25, 16, 0, 0);
ok(e.scale === 2 && e.cols === 80, "no native dims -> default 320x200, scale 2");

if (fails) { print(fails + " VIEWPORT UNIT FAILURE(S)"); exit(1); }
print("VIEWPORT UNIT OK");
