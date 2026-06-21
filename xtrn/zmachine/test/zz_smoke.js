// Headless smoke test: run the jszm engine on Zork Zero with stubbed I/O.
// Catches engine crashes / missing early assets before a live launch. jsexec test/zz_smoke.js
var BASE = js.exec_dir + "../";   // door dir (this script lives in test/)
load(BASE + "quetzal.js");
load(BASE + "jszm.js");

var dir = BASE + "fantasy/zork0-r393-s890714.gfx";
var mf = { count: 0, release: 0, pics: {} };
(function () {
  var f = new File(dir + "/manifest"); if (!f.open("r")) { print("NO MANIFEST"); return; }
  var line;
  while ((line = f.readln()) !== null) {
    if (line.charAt(0) === "#") { var c = line.match(/count=(\d+)/), r = line.match(/release=(\d+)/); if (c) mf.count = +c[1]; if (r) mf.release = +r[1]; continue; }
    if (line.charAt(0) === "@" || !line.length) continue;
    var p = line.replace(/^\s+/, "").split(/\s+/);
    if (p.length >= 4 && /^\d+$/.test(p[0])) mf.pics[+p[0]] = { w: +p[2], h: +p[3] };
  }
  f.close();
})();
print("manifest: count=" + mf.count + " release=" + mf.release + " pics=" + (function(){var n=0,k;for(k in mf.pics)n++;return n;})());

var f = new File(BASE + "fantasy/zork0-r393-s890714.z6");
f.open("rb"); var bytes = new Uint8Array(f.readBin(1, f.length)); f.close();

var game = new JSZM(bytes);
var out = [], pics = [], reads = 0, keys = 0;
var script = ["", "", "look", "wait", "quit", "y"];

function dumpWindows(tag) {
  print("=== game.windows @ " + tag + " (Y X Ysize Xsize | Ycur Xcur | style font fontsize) ===");
  var w, ws = game.windows;
  if (!ws) { print("  (no game.windows)"); return; }
  for (w = 0; w < 8; w++) {
    var a = ws[w]; if (!a) { continue; }
    var y = a[0]&0xffff, x = a[1]&0xffff, ys = a[2]&0xffff, xs = a[3]&0xffff;
    if (!(y||x||ys||xs)) continue;   // skip empty windows
    print("  win"+w+": Y="+y+" X="+x+" Ysize="+ys+" Xsize="+xs+" | Ycur="+(a[4]&0xffff)+" Xcur="+(a[5]&0xffff)+" | style="+a[10]+" font="+a[12]+" fontsize="+(a[13]&0xffff));
  }
}
game.print = function (t) { out.push(t); };
game.read = function () { reads++; if (reads > 25) throw new Error("READ RUNAWAY"); if (reads === 2) dumpWindows("first Banquet Hall prompt"); var c = (reads - 1 < script.length) ? script[reads - 1] : "quit"; out.push("\n{READ#" + reads + " <- '" + c + "'}\n"); return c; };
game.readKey = function () { keys++; if (keys > 800) throw new Error("READKEY RUNAWAY"); return String.fromCharCode(13); };
game.updateStatusLine = function () {};
game.screen = function () {};
game.setCursor = function () {};
game.getCursor = function () { return { row: 1, col: 1 }; };
game.setColour = function () {};
game.setTextStyle = function () {};
game.bufferMode = function () {};
game.eraseWindow = function () {};
game.eraseLine = function () {};
game.scrollWindow = function () {};
game.windowChanged = function () {};
game.windowOrigin = function () { return { x: 0, y: 0 }; };
game.soundEffect = function () {};
game.mouseWindow = function () {};
game.drawPicture = function (pic, x, y, win) { pics.push({ op: "draw", pic: pic, x: x, y: y, win: win }); };
game.erasePicture = function (pic, x, y, win) { pics.push({ op: "erase", pic: pic, x: x, y: y, win: win }); };
game.pictureInfo = function (pic) {
  if (pic === 0) return { count: mf.count, release: mf.release };
  if (mf.pics[pic]) return { width: mf.pics[pic].w, height: mf.pics[pic].h };
  return null;
};

var err = null;
try { game.run(); } catch (e) { err = e; }

var text = out.join("");
print("=== TEXT (first 2500 chars) ===");
print(text.substr(0, 2500));
print("=== PICTURE OPS (first 50 of " + pics.length + ") ===");
for (var i = 0; i < pics.length && i < 50; i++) print(JSON.stringify(pics[i]));
// any picture the game drew that we DIDN'T bake?
var missing = {};
for (i = 0; i < pics.length; i++) { var pn = pics[i].pic; if (pn !== 0 && !mf.pics[pn]) missing[pn] = (missing[pn] || 0) + 1; }
var miss = []; for (var m in missing) miss.push(m + "(x" + missing[m] + ")");
print("reads=" + reads + " keys=" + keys + " picOps=" + pics.length + " missingAssets=" + (miss.length ? miss.join(",") : "none"));
if (err) { print("=== EXCEPTION ==="); print(err.toString()); if (err.stack) print(err.stack); }
else print("=== ran to QUIT cleanly ===");
