// Headless smoke for Journey (v6, r83). Runs the jszm engine with stubbed I/O; dumps window geometry +
// picture ops so we can see Journey's layout before touching renderer code. jsexec test/journey_smoke.js
var BASE = js.exec_dir + "../";
load(BASE + "quetzal.js");
load(BASE + "jszm.js");

var fails = 0; function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }

var dir = BASE + "fantasy/journey.gfx";
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

var f = new File(BASE + "fantasy/journey.z6");
f.open("rb"); var bytes = new Uint8Array(f.readBin(1, f.length)); f.close();

var game = new JSZM(bytes);
// Report a realistic SyncTERM v6 geometry so the engine takes its graphical layout path.
game.screenRows = 24; game.screenCols = 80; game.screenWidthPx = 640; game.screenHeightPx = 384; game.fontWidth = 8; game.fontHeight = 16;

var out = [], pics = [], reads = 0, keys = 0, dumped = 0;
var script = ["", "", "", "", "", "", "quit", "y"];   // Journey is menu-driven; blank entries advance the intro/menus

function dumpWindows(tag) {
  print("=== game.windows @ " + tag + " (win: X Y | Xsize Ysize | Xcur Ycur | font fontsize) ===");
  var w, ws = game.windows; if (!ws) { print("  (no game.windows)"); return; }
  for (w = 0; w < 8; w++) { var a = ws[w]; if (!a) continue; var y=a[0]&0xffff,x=a[1]&0xffff,ys=a[2]&0xffff,xs=a[3]&0xffff;
    if (!(y||x||ys||xs)) continue;
    print("  win"+w+": X="+x+" Y="+y+" | Xsize="+xs+" Ysize="+ys+" | Xcur="+(a[5]&0xffff)+" Ycur="+(a[4]&0xffff)+" | font="+a[12]+" fontsize="+(a[13]&0xffff)); }
}
// Track window 1 ops: Journey positions menu text via set_window(1) + set_cursor pixel positions.
// This confirms the menu window (win1) is exercised with cursor positioning.
var winOut = { 0: "", 1: "", 3: "" }, curw = 0, win1ScreenCalls = 0, win1CursorCalls = 0;
game.screen = function (w) { curw = w; if (w === 1) win1ScreenCalls++; };
game.print = function (t) { out.push(t); if (winOut[curw] === undefined) winOut[curw] = ""; winOut[curw] += t; };
game.read = function () { reads++; if (reads > 30) throw new Error("READ RUNAWAY"); if (dumped < 2 && pics.length) { dumpWindows("prompt#"+reads); dumped++; }
  var c = (reads - 1 < script.length) ? script[reads - 1] : "quit"; out.push("\n{READ#" + reads + " <- '" + c + "'}\n"); return c; };
game.readKey = function () { keys++; if (keys > 800) throw new Error("READKEY RUNAWAY"); return String.fromCharCode(13); };
game.updateStatusLine = function () {}; game.eraseWindow = function () {}; game.eraseLine = function () {}; game.scrollWindow = function () {};
game.setCursor = function () { if (curw === 1) win1CursorCalls++; };
game.getCursor = function () { return { row: 1, col: 1 }; }; game.setColour = function () {}; game.setTextStyle = function () {};
game.bufferMode = function () {};
game.windowChanged = function () {}; game.windowOrigin = function () { return { x: 0, y: 0 }; }; game.soundEffect = function () {}; game.mouseWindow = function () {};
game.graphicsAvailable = function () { return true; };
game.drawPicture = function (pic, x, y, win) { pics.push({ op: "draw", pic: pic, x: x>32767?x-65536:x, y: y>32767?y-65536:y, win: win }); };
game.erasePicture = function (pic, x, y, win) { pics.push({ op: "erase", pic: pic, x: x, y: y, win: win }); };
game.pictureInfo = function (pic) {
  if (pic === 0) return { count: mf.count, release: mf.release };
  if (mf.pics[pic]) return { width: mf.pics[pic].w, height: mf.pics[pic].h };
  return null;
};

var err = null;
try { game.run(); } catch (e) { err = e; }
if (!dumped) dumpWindows("final");

var text = out.join("");
print("=== TEXT (first 2000 chars) ===");
print(text.substr(0, 2000));
print("=== PICTURE OPS (first 50 of " + pics.length + ") ===");
for (var i = 0; i < pics.length && i < 50; i++) print(JSON.stringify(pics[i]));
var missing = {};
for (i = 0; i < pics.length; i++) { var pn = pics[i].pic; if (pn !== 0 && !mf.pics[pn]) missing[pn] = (missing[pn] || 0) + 1; }
var miss = []; for (var m in missing) miss.push(m + "(x" + missing[m] + ")");
print("reads=" + reads + " keys=" + keys + " picOps=" + pics.length + " missingAssets=" + (miss.length ? miss.join(",") : "none"));
print("win1 screen calls=" + win1ScreenCalls + " win1 cursor calls=" + win1CursorCalls);
ok(win1ScreenCalls > 0, "set_window(1) called -- Journey menu window exercised");
ok(win1CursorCalls > 0, "set_cursor in win1 -- menu text is positioned (the bottom grid)");
if (err) { print("=== EXCEPTION ==="); print(err.toString()); if (err.stack) print(err.stack); }
else print("=== ran to QUIT cleanly ===");
if (fails) print(fails + " SMOKE FAILURE(S)"); else print("SMOKE OK");
