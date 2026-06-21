// test/v6sixel.js — run: jsexec test/v6sixel.js
load(js.exec_dir + "../v6sixel.js");
var fails = 0; function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }
function bytesOf(str) { var a = []; for (var i = 0; i < str.length; i++) a.push(str.charCodeAt(i) & 255); return a; }

// P6 PPM: 2x1, red then green, with a comment line in the header
(function () {
  var hdr = "P6\n# a comment\n2 1\n255\n";
  var raster = [255,0,0, 0,255,0];
  var ppm = JSZM_sixelParsePPM(bytesOf(hdr).concat(raster));
  ok(ppm && ppm.w === 2 && ppm.h === 1, "PPM dims 2x1");
  ok(ppm.rgb[0] === 255 && ppm.rgb[1] === 0 && ppm.rgb[2] === 0, "PPM px0 red");
  ok(ppm.rgb[3] === 0 && ppm.rgb[4] === 255 && ppm.rgb[5] === 0, "PPM px1 green");
})();

// P4 PBM: width 10 (padded to 16 = 2 bytes/row), row0 = 0b11000000 0b00000000 -> cols 0,1 opaque
(function () {
  var hdr = "P4\n10 1\n";
  var bytes = bytesOf(hdr).concat([0xC0, 0x00]);
  var pbm = JSZM_sixelParsePBM(bytes);
  ok(pbm && pbm.w === 10 && pbm.stride === 2, "PBM 10w stride 2");
  ok(JSZM_sixelMaskOpaque(pbm, 0, 0) === true, "PBM col0 opaque");
  ok(JSZM_sixelMaskOpaque(pbm, 1, 0) === true, "PBM col1 opaque");
  ok(JSZM_sixelMaskOpaque(pbm, 2, 0) === false, "PBM col2 transparent");
  ok(JSZM_sixelMaskOpaque(null, 5, 0) === true, "null mask = all opaque");
})();

// Encode a 2x1 red/green image, no mask, no pad -> P2=1 header, 2 registers, one band.
(function () {
  var ppm = { w: 2, h: 1, rgb: [255,0,0, 0,255,0] };
  var s = JSZM_sixelEncode(ppm, null, 0, 0, 2, 1, 0);
  ok(s.indexOf("\x1bP0;1;0q") === 0, "DCS header P2=1");
  ok(s.indexOf("\"1;1;2;1") >= 0, "raster attrs 2x1");
  ok(s.indexOf("#0;2;100;0;0") >= 0, "register 0 = red (0-100 scale)");
  ok(s.indexOf("#1;2;0;100;0") >= 0, "register 1 = green");
  ok(s.charAt(s.length-2) === "\x1b" && s.charAt(s.length-1) === "\\", "ends with ST");
  ok(s.indexOf("#0") >= 0, "red column present");
})();

// Masked pixel is skipped: 2x1, both red, mask makes col1 transparent -> only 1 column of red set.
(function () {
  var ppm = { w: 2, h: 1, rgb: [255,0,0, 255,0,0] };
  var pbm = { w: 2, h: 1, stride: 1, bits: [0x80] };   // col0 opaque, col1 transparent
  var s = JSZM_sixelEncode(ppm, pbm, 0, 0, 2, 1, 0);
  ok(s.indexOf("#0;2;100;0;0") >= 0, "register red present");
  ok(s.indexOf("!2@") < 0, "transparent col not painted as a run of 2");
})();

// topPad shifts content down: raster height = pad + imageH.
(function () {
  var ppm = { w: 1, h: 1, rgb: [0,0,255] };
  var s = JSZM_sixelEncode(ppm, null, 0, 0, 1, 1, 3);
  ok(s.indexOf("\"1;1;1;4") >= 0, "raster height = topPad(3)+1");
})();

// Black band: solid black w x h.
(function () {
  var s = JSZM_sixelBlackBand(4, 6, 0);
  ok(s.indexOf("\x1bP0;1;0q") === 0, "black band DCS header");
  ok(s.indexOf("#0;2;0;0;0") >= 0, "black register");
  ok(s.indexOf("\"1;1;4;6") >= 0, "black band 4x6");
})();

(function () {
  var w = [];
  var sfc = JSZM_makeSixelSurface({
    write: function (x) { w.push(x); }, cellW: 8, cellH: 16,
    loadPPM: function () { return { w: 2, h: 2, rgb: [255,0,0,255,0,0, 255,0,0,255,0,0] }; },
    loadPBM: function () { return null; }
  });
  w.length = 0;
  sfc.blit({ file: "5.ppm", mask: null, useMask: false, srcX: 0, srcY: 0, srcW: 2, srcH: 2, dstX: 16, dstY: 48 });
  var j = w.join("");
  ok(j.indexOf("\x1b[4;3H") >= 0, "cursor positioned to floor cell row4 col3 (dy48/16+1, dx16/8+1)");
  ok(j.indexOf("\x1bP0;1;0q") >= 0, "sixel DCS emitted");
  ok(j.indexOf("\x1b7") >= 0 || j.indexOf("\x1b[s") >= 0, "cursor saved");
  ok(j.indexOf("\x1b8") >= 0 || j.indexOf("\x1b[u") >= 0, "cursor restored");
  w.length = 0;
  sfc.fillBlack({ dstX: 0, dstY: 0, w: 16, h: 6 });
  ok(w.join("").indexOf("#0;2;0;0;0") >= 0, "fillBlack emits black sixel");
})();

// Resample + device-cell scaling: a 2x2 image up to a 2x-larger cell renders at 4x4 device px.
(function () {
  var ppm = { w: 2, h: 2, rgb: [255,0,0,255,0,0, 255,0,0,255,0,0] };
  var rs = JSZM_sixelResample(ppm, null, 0, 0, 2, 2, 4, 4);
  ok(rs.ppm.w === 4 && rs.ppm.h === 4, "resample 2x2 -> 4x4 dims");
  ok(rs.ppm.rgb.length === 4 * 4 * 3, "resample rgb length");
  ok(rs.ppm.rgb[0] === 255 && rs.ppm.rgb[1] === 0 && rs.ppm.rgb[2] === 0, "resample px red");
  var w = [];
  var sfc = JSZM_makeSixelSurface({ write: function (x) { w.push(x); }, cellW: 8, cellH: 16, devCellW: 16, devCellH: 32,
    loadPPM: function () { return { w: 2, h: 2, rgb: [255,0,0,255,0,0, 255,0,0,255,0,0] }; }, loadPBM: function () { return null; } });
  w.length = 0;
  sfc.blit({ file: "x", mask: null, useMask: false, srcX: 0, srcY: 0, srcW: 2, srcH: 2, dstX: 0, dstY: 0 });
  ok(w.join("").indexOf("\"1;1;4;4") >= 0, "blit resamples 2x2 logical -> 4x4 device (devCell 2x)");
})();

// leftPad: transparent leading columns widen the raster + shift content right (sub-cell horizontal offset).
(function () {
  var s = JSZM_sixelEncode({ w: 1, h: 1, rgb: [255,0,0] }, null, 0, 0, 1, 1, 0, 3);
  ok(s.indexOf("\"1;1;4;1") >= 0, "encode leftPad: raster width = leftPad(3)+sw(1)=4");
  var bb = JSZM_sixelBlackBand(2, 6, 0, 2);
  ok(bb.indexOf("\"1;1;4;6") >= 0, "black band leftPad: width = leftPad(2)+w(2)=4");
})();

// Global device positioning: adjacent door rects tile exactly at the device level, so clears fully cover a
// wide image with no sub-pixel seam. devCell 10x20, logical cell 8x16: door 80 -> device 100 = cell-aligned.
(function () {
  var w = [];
  var sfc = JSZM_makeSixelSurface({ write: function (x) { w.push(x); }, cellW: 8, cellH: 16, devCellW: 10, devCellH: 20 });
  sfc.fillBlack({ dstX: 0,  dstY: 0, w: 80, h: 16 });   // door [0..80)   -> device [0..100), col 1, width 100
  sfc.fillBlack({ dstX: 80, dstY: 0, w: 80, h: 16 });   // door [80..160) -> device [100..200), col 11
  var s = w.join("");
  ok(s.indexOf("\x1b[1;1H") >= 0, "first clear positioned at col 1");
  ok(s.indexOf("\"1;1;100;") >= 0, "first clear raster width = 100 device px (80 door * 10/8)");
  ok(s.indexOf("\x1b[1;11H") >= 0, "second clear abuts at col 11 (device 100 = first clear's right edge -> exact tiling)");
})();

if (fails) { print(fails + " V6SIXEL FAILURE(S)"); exit(1); }
print("V6SIXEL OK");
