// Headless deterministic tests for the disconnect/resume engine behaviors.
// Run: jsexec /share/sbbs/xtrn/zmachine/test/resume.js
load(js.exec_dir + "../quetzal.js");
load(js.exec_dir + "../jszm.js");

// Any interactive story works for these mechanism tests; find one across the door's
// category subdirs (the game library is gitignored, so skip cleanly if none is present).
var STORY = (function () {
  var exts = ["*.z3", "*.z5", "*.z8"], i, f;
  for (i = 0; i < exts.length; i++) { f = directory(js.exec_dir + "../*/" + exts[i]); if (f.length) return f[0]; }
  return null;
})();
if (!STORY) { print("RESUME SKIPPED (no story files found under " + js.exec_dir + "../*/)"); exit(0); }

var fails = 0;
function ok(c, msg) { if (!c) { print("FAIL: " + msg); fails++; } }

function bytesEqual(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

function readStory(p) {
  var f = new File(p);
  if (!f.open("rb")) throw new Error("cannot open " + p);
  var bytes = f.readBin(1, f.length);
  f.close();
  return new Uint8Array(bytes);
}
var STORYBYTES = readStory(STORY);

// Build a JSZM driven by a scripted list of commands; read() returns null once
// the list is exhausted (which, post-Task-1, unwinds run() cleanly).
function makeGame(commands, resumeData) {
  var g = new JSZM(new Uint8Array(STORYBYTES));   // fresh copy of the story image
  g.print = function () {};
  g.highlight = function () {};
  g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; };
  var i = 0;
  g.reads = 0;
  g.read = function () {
    g.reads++;
    if (g.reads > 200) throw new Error("runaway: read() called " + g.reads + " times");
    return i < commands.length ? commands[i++] : null;
  };
  if (resumeData) g.resumeData = resumeData;
  return g;
}

// --- Task 1: null abort ---
(function () {
  var g = makeGame([]);            // read() returns null on the very first prompt
  var threw = null;
  try { g.run(); } catch (e) { threw = e; }
  ok(threw === null, "run() returns cleanly when read() === null (threw: " + threw + ")");
  ok(g.reads === 1, "read() called exactly once before abort (was " + g.reads + ")");
})();

// --- Task 2: checkpoint is a complete in-memory snapshot at the prompt ---
(function () {
  var g = makeGame([]);            // stop at the first prompt
  g.run();
  ok(g.checkpoint instanceof Uint8Array, "checkpoint is a Uint8Array");
  ok(g.checkpoint && g.checkpoint.length > 0, "checkpoint is non-empty (was " + (g.checkpoint ? g.checkpoint.length : "unset") + ")");
})();

// --- Task 3: resume lands at the saved prompt, not a fresh start ---
(function () {
  var g1 = makeGame([]);          var c1 = (g1.run(), g1.checkpoint);   // snapshot at READ #1
  var g2 = makeGame(["wait"]);    var c2 = (g2.run(), g2.checkpoint);   // snapshot at READ #2
  ok(!bytesEqual(c1, c2), "READ #1 and READ #2 snapshots differ (sanity)");

  var gr = makeGame([], c2);      gr.run();                            // resume from c2
  ok(bytesEqual(gr.checkpoint, c2), "resumed session's first checkpoint equals the saved one");
  ok(!bytesEqual(gr.checkpoint, c1), "resumed session did NOT start fresh");

  // Foreign/corrupt slot must not crash; engine starts fresh.
  var bad = new Uint8Array(c2); bad[2] ^= 0xFF; bad[3] ^= 0xFF;        // break the ZORKID
  var gb = makeGame([], bad);
  var threw = null;
  try { gb.run(); } catch (e) { threw = e; }
  ok(threw === null, "foreign/corrupt resumeData does not throw (threw: " + threw + ")");
  ok(bytesEqual(gb.checkpoint, c1), "rejected slot falls back to a fresh game");
})();

// --- Quetzal: save is a FORM, and a foreign story is rejected ---
(function () {
  var g = makeGame([]); g.run();                    // reach the first prompt
  var blob = g.checkpoint;
  ok(String.fromCharCode(blob[0], blob[1], blob[2], blob[3]) === "FORM", "checkpoint is a Quetzal FORM");

  // Corrupt the IFhd serial so the story-binding check rejects it.
  var foreign = new Uint8Array(blob);
  // find "IFhd" and flip a serial byte (IFhd data starts 8 bytes after the id)
  for (var i = 0; i + 4 <= foreign.length; i++) {
    if (String.fromCharCode(foreign[i], foreign[i + 1], foreign[i + 2], foreign[i + 3]) === "IFhd") {
      foreign[i + 8 + 2] ^= 0xFF; break;            // first serial byte
    }
  }
  var gb = makeGame([], foreign);
  var threw = null;
  try { gb.run(); } catch (e) { threw = e; }
  ok(threw === null, "foreign-story save does not throw (threw: " + threw + ")");
  ok(String.fromCharCode(gb.checkpoint[0], gb.checkpoint[1], gb.checkpoint[2], gb.checkpoint[3]) === "FORM",
     "rejected story falls back to a fresh game that still checkpoints as Quetzal");
})();

if (fails) throw new Error(fails + " resume test(s) failed");
print("RESUME OK");
