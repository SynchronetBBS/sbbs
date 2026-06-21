// test/v6surface.js — apcSurface emits the exact APC bytes the door emitted before extraction.
load(js.exec_dir + "../v6pics.js");
var fails = 0; function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }
var w = [];
var s = JSZM_makeApcSurface({
  write: function (x) { w.push(x); }, clientPrefix: "t", isCached: function () { return true; },
  readPpmBase64: function () { return "AAA="; }, readPbmBase64: function () { return "AAA="; },
  blackName: "t/.black.ppm", readBlackBase64: function () { return "AAA="; }
});
w.length = 0;
s.blit({ file: "5.ppm", mask: "5.pbm", useMask: true, srcX: 0, srcY: 2, srcW: 100, srcH: 50, dstX: 8, dstY: 16 });
var joined = w.join("");
ok(joined.indexOf("\x1b_SyncTERM:C;LoadPPM;B=0;t/5.ppm\x1b\\") >= 0, "LoadPPM emitted (cached -> no C;S)");
ok(joined.indexOf("C;LoadPBM;t/5.pbm") >= 0, "LoadPBM emitted");
ok(joined.indexOf("P;Paste;SX=0;SY=2;SW=100;SH=50;DX=8;DY=16;MBUF;MX=0;MY=2;B=0") >= 0, "Paste matches legacy format");
w.length = 0;
s.fillBlack({ dstX: 4, dstY: 6, w: 20, h: 10 });
ok(w.join("").indexOf("P;Paste;SX=0;SY=0;SW=20;SH=10;DX=4;DY=6;B=1") >= 0, "fillBlack matches legacy format");
if (fails) { print(fails + " V6SURFACE FAILURE(S)"); exit(1); }
print("V6SURFACE OK");
