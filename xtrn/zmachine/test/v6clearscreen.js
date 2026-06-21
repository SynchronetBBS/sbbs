// Regression: erase_window -1/-2 must black the FULL pixel screen even when drawnPix is empty
// (the text->graphics return after Arthur's F6 mode). Partial erases (>=0) must NOT. jsexec test/v6clearscreen.js
load(js.exec_dir + "../v6pics.js");
var fails = 0; function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }

function mkBridge() {
  var writes = [];
  var manifest = { count: 1, release: 1, pics: { 5: { w: 100, h: 50, type: "bitmap", file: "5.ppm" } }, adaptive: {}, remap: {} };
  var b = JSZM_makePictureBridge({
    write: function (s) { writes.push(s); },
    capable: true, manifest: manifest, clientPrefix: "t",
    readPpmBase64: function () { return "AAA="; }, readPbmBase64: function () { return ""; },
    isCached: function () { return true; }, scale: 2,
    screenWidthPx: 640, screenHeightPx: 384,
    originOf: function () { return { x: 0, y: 0 }; },
    fontHeight: 16, fontWidth: 8,
    blackName: "black.ppm", readBlackBase64: function () { return "AAA="; }
  });
  b._writes = writes;
  return b;
}
function fullScreenBlacks(writes) {
  var n = 0, i;
  for (i = 0; i < writes.length; i++)
    if (/Paste;SX=0;SY=0;SW=640;SH=384;DX=0;DY=0;B=1/.test(writes[i])) n++;
  return n;
}

// 1) STARTUP: erase_window -1 before ANY picture was ever drawn -> NO full-screen black
//    (nothing stale to clear; must not force the screen-sized black upload early).
(function () {
  var b = mkBridge();
  b.clearRegion(-1);
  ok(fullScreenBlacks(b._writes) === 0, "erase_window -1 at startup (no prior draw) does NOT full-screen-black");
})();

// 2) F6 TEXT->GRAPHICS RETURN: a picture was drawn earlier, the screen is now empty (F6's own erase
//    dropped it), and erase_window -1 fires again -> ONE full-screen black so the return is clean.
(function () {
  var b = mkBridge();
  b.drawPicture(5, 10, 10, 0);   // graphics used -> everDrew
  b.clearRegion(-1);             // F6 entry: per-pic blit empties drawnPix (no full-screen black)
  var before = b._writes.length;
  b.clearRegion(-1);             // F6 return: drawnPix empty BUT everDrew -> full-screen black
  ok(fullScreenBlacks(b._writes.slice(before)) === 1, "empty-screen erase_window -1 after graphics blacks the full screen");
})();

// 3) erase_window -1 with pictures PRESENT does not full-screen-black (the per-picture black-blits cover
//    the screen, as before); it still drops all tracked pictures.
(function () {
  var b = mkBridge();
  b.drawPicture(5, 10, 10, 0);
  var before = b._writes.length;
  b.clearRegion(-1);
  ok(fullScreenBlacks(b._writes.slice(before)) === 0, "erase_window -1 with pics present relies on per-pic blits, no full-screen black");
  ok(b.screenPicCalls().length === 0, "erase_window -1 drops all tracked pictures");
})();

// 4) PARTIAL erase (window >=0) must NOT full-screen-black (normal map/scene toggle path)
(function () {
  var b = mkBridge();
  b.drawPicture(5, 10, 10, 2);
  var before = b._writes.length;
  b.clearRegion(2);
  ok(fullScreenBlacks(b._writes.slice(before)) === 0, "partial erase (window 2) does NOT full-screen-black");
})();

if (fails) { print(fails + " V6CLEARSCREEN FAILURE(S)"); exit(1); }
print("V6CLEARSCREEN OK");
