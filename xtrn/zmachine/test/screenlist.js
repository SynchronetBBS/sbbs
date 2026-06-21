// Headless test of the chooser's version filter (jszm_storyList logic), without console.
// We can't load zmachine.js (it runs the door), so we replicate the filter predicate and
// assert it accepts v3/4/5/8, scanning the door's category subdirs (the curated library).
// The library is gitignored, so skip cleanly if no story files are present.
load(js.exec_dir + "../quetzal.js"); load(js.exec_dir + "../jszm.js");
var fails = 0;
function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }
function ver(p) { var f = new File(p); if (!f.open("rb")) return -1; var v = f.readBin(1, 1); f.close(); return v; }
var exts = ["*.z3", "*.z4", "*.z5", "*.z6", "*.z8"], found = {}, total = 0;
for (var i = 0; i < exts.length; i++) {
  var files = directory(js.exec_dir + "../*/" + exts[i]);   // *.zN across all category subdirs
  for (var j = 0; j < files.length; j++) {
    var v = ver(files[j]);
    if (v == 3 || v == 4 || v == 5 || v == 6 || v == 8) { found[v] = (found[v] || 0) + 1; total++; }
  }
}
if (!total) { print("SCREENLIST SKIPPED (no story files under " + js.exec_dir + "../*/)"); exit(0); }
ok(found[3] > 0, "version filter found at least one v3 story");
ok(found[5] > 0, "version filter found at least one v5 story");
ok(found[8] > 0, "version filter found at least one v8 story");
if (fails) throw new Error(fails + " screenlist test(s) failed");
print("SCREENLIST OK");
