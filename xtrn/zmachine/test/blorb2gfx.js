// Headless tests for the offline pre-bake tool's pure functions. Run: jsexec test/blorb2gfx.js
load(js.exec_dir + "../tools/blorb2gfx.js");

var fails = 0;
function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }

// Build a minimal Blorb: FORM..IFRS, RIdx(2 Pict entries), a Rect chunk (#1 100x50), a PNG chunk (#2).
function be32(v) { return [(v >>> 24) & 255, (v >>> 16) & 255, (v >>> 8) & 255, v & 255]; }
function str4(s) { return [s.charCodeAt(0), s.charCodeAt(1), s.charCodeAt(2), s.charCodeAt(3)]; }
(function () {
  var bytes = [];
  function push(a) { for (var i = 0; i < a.length; i++) bytes.push(a[i] & 255); }
  // Header placeholder (FORM, len, IFRS) — len patched after.
  push(str4("FORM")); push(be32(0)); push(str4("IFRS"));
  // RIdx chunk: count=2, entry(Pict,1,offRect), entry(Pict,2,offPng) — offsets patched after.
  var ridxLenPos = bytes.length + 4;
  push(str4("RIdx")); push(be32(0)); // len patched
  var ridxStart = bytes.length;
  push(be32(2));
  var e1OffPos = bytes.length + 8; push(str4("Pict")); push(be32(1)); push(be32(0));
  var e2OffPos = bytes.length + 8; push(str4("Pict")); push(be32(2)); push(be32(0));
  var ridxLen = bytes.length - ridxStart;
  var rl = be32(ridxLen); bytes[ridxLenPos] = rl[0]; bytes[ridxLenPos + 1] = rl[1]; bytes[ridxLenPos + 2] = rl[2]; bytes[ridxLenPos + 3] = rl[3];
  // RelN chunk: release 27 (len 2, even — no pad byte).
  push(str4("RelN")); push(be32(2)); push([0, 27]);
  // APal chunk: adaptive pics 54, 170 (len 8).
  push(str4("APal")); push(be32(8)); push(be32(54)); push(be32(170));
  // BPal chunk: (scene 4, adaptive 54, variant 1005), (scene 4, adaptive 170, variant 1006) (len 24).
  push(str4("BPal")); push(be32(24));
  push(be32(4)); push(be32(54)); push(be32(1005));
  push(be32(4)); push(be32(170)); push(be32(1006));
  // Rect chunk for #1 at current offset
  var offRect = bytes.length;
  push(str4("Rect")); push(be32(8)); push(be32(100)); push(be32(50));
  // PNG chunk for #2 at current offset, 4 bytes of fake payload
  var offPng = bytes.length;
  push(str4("PNG ")); push(be32(4)); push([0xDE, 0xAD, 0xBE, 0xEF]);
  var o1 = be32(offRect); bytes[e1OffPos] = o1[0]; bytes[e1OffPos + 1] = o1[1]; bytes[e1OffPos + 2] = o1[2]; bytes[e1OffPos + 3] = o1[3];
  var o2 = be32(offPng); bytes[e2OffPos] = o2[0]; bytes[e2OffPos + 1] = o2[1]; bytes[e2OffPos + 2] = o2[2]; bytes[e2OffPos + 3] = o2[3];
  var formLen = be32(bytes.length - 8); bytes[4] = formLen[0]; bytes[5] = formLen[1]; bytes[6] = formLen[2]; bytes[7] = formLen[3];

  var u = new Uint8Array(bytes.length); for (var i = 0; i < bytes.length; i++) u[i] = bytes[i];
  var res = parseBlorb(u);
  ok(res && res.release === 27, "RelN parsed -> release 27");
  ok(res && res.pics.length === 2, "two Pict resources found");
  ok(res.pics[0].num === 1 && res.pics[0].fmt === "rect" && res.pics[0].w === 100 && res.pics[0].h === 50, "pic 1 is rect 100x50");
  ok(res.pics[1].num === 2 && res.pics[1].fmt === "png" && res.pics[1].dataLen === 4 && u[res.pics[1].dataOffset] === 0xDE, "pic 2 is png, dataOffset points at payload");
  ok(res.pics[0].dataOffset === 0 && res.pics[0].dataLen === 0, "rect carries no image payload (dataOffset/dataLen 0)");
  ok(res.apal && res.apal.length === 2 && res.apal[0] === 54 && res.apal[1] === 170, "APal parsed -> [54,170]");
  ok(res.bpal && res.bpal.length === 2, "BPal parsed -> 2 entries");
  ok(res.bpal[0].scene === 4 && res.bpal[0].adaptive === 54 && res.bpal[0].variant === 1005, "BPal entry 0 = (4,54,1005)");
  ok(res.bpal[1].scene === 4 && res.bpal[1].adaptive === 170 && res.bpal[1].variant === 1006, "BPal entry 1 = (4,170,1006)");
})();
(function () {
  ok(parseBlorb(new Uint8Array([1, 2, 3, 4])) === null, "non-FORM input -> null");
})();

// buildManifestText: slim manifest with a mask column and @ remap lines.
(function () {
  var entries = [
    { num: 1,  type: "rect",   w: 100, h: 50,  file: null,       mask: null      },
    { num: 2,  type: "bitmap", w: 292, h: 196, file: "2.ppm",    mask: null      },
    { num: 54, type: "bitmap", w: 314, h: 84,  file: "54.ppm",   mask: "54.pbm"  }
  ];
  var remap = [
    { scene: 4, adaptive: 54, variant: 1005 },
    { scene: 4, adaptive: 170, variant: 1006 }
  ];
  var txt = buildManifestText(3, 27, entries, remap);
  var expect =
    "# count=3  release=27\n" +
    "1  rect    100  50  -  -\n" +
    "2  bitmap  292  196  2.ppm  -\n" +
    "54  bitmap  314  84  54.ppm  54.pbm\n" +
    "@ 4 54 1005\n" +
    "@ 4 170 1006\n";
  ok(txt === expect, "buildManifestText emits mask column + @ remap lines (got:\n" + txt + ")");
})();

if (fails) { print(fails + " BLORB2GFX UNIT FAILURE(S)"); exit(1); }
print("BLORB2GFX UNIT OK");
