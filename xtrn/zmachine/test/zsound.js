// jsexec unit tests for zsound.js (@sound_effect player) -- and, through it,
// cterm_lib.js's audio layer (upload-once + re-Load-per-Queue replay fix).
// Simulates the terminal with a console stub (see the javascript skill /
// cterm_lib CTDA tests for the pattern): write() records the wire and answers
// the capability query; inkey() replays the canned bytes.
//
// Run: jsexec xtrn/zmachine/test/zsound.js

var fails = 0;
function ok(c, msg) { if (!c) { print("FAIL: " + msg); fails++; } }

// ---- console stub -----------------------------------------------------------
var wire = "", input = [];
var console = {
  cterm_version: 0,             // per-scenario
  ctrlkey_passthru: 0,
  clearkeybuffer: function () { input = []; },
  inkey: function (mode, timeout) { return input.length ? input.shift() : ""; },
  write: function (s) {
    wire += s;
    if (s.indexOf("SyncTERM:Q;libsndfile") >= 0 && console._audio_reply !== null)
      queue("\x1b[=7;100;" + console._audio_reply + "n");
    if (s === "\x1b[6n")        // query_fb's CPR terminator
      queue("\x1b[1;1R");
  },
  _audio_reply: null            // null = terminal ignores the query
};
function queue(s) { for (var i = 0; i < s.length; i++) input.push(s.charAt(i)); }

load(js.exec_dir + "../zsound.js");

// ---- fixture Blorb: two Snd resources (nums 3 and 4) ------------------------
function be32(n) {
  return String.fromCharCode((n >>> 24) & 255, (n >>> 16) & 255, (n >>> 8) & 255, n & 255);
}
function chunk(id, payload) {
  return id + be32(payload.length) + payload + (payload.length & 1 ? "\x00" : "");
}
// Even-length payloads: keeps the IFF pad byte out of the chunk, so the parsed
// resource length equals the chunk string length (the pad is not sound data).
var snd3 = chunk("FORM", "AIFF" + "snd-three-pcm3");    // an AIFF file IS a FORM chunk
var snd4 = chunk("FORM", "AIFF" + "snd-four-pcm42");
var ridxPayload = be32(2)
  + "Snd " + be32(3) + be32(0)    // offsets patched below
  + "Snd " + be32(4) + be32(0);
var ridx = chunk("RIdx", ridxPayload);
var off3 = 12 + ridx.length, off4 = off3 + snd3.length;
ridxPayload = be32(2) + "Snd " + be32(3) + be32(off3) + "Snd " + be32(4) + be32(off4);
ridx = chunk("RIdx", ridxPayload);
var body = ridx + snd3 + snd4;
var blorb = "FORM" + be32(4 + body.length) + "IFRS" + body;

var base = system.temp_dir + "zsound_test_" + Date.now();
var storyPath = base + ".z5", blbPath = base + ".blb";
function writeFile(path, data) {
  var f = new File(path);
  if (!f.open("wb")) throw new Error("cannot write " + path);
  f.write(data); f.close();
}
writeFile(blbPath, blorb);

function cleanup() {
  try { file_remove(blbPath); } catch (e) { }
}

try {
  // ---- Blorb index parse ----
  var idx = JSZM_zsBlorbSnds(blbPath);
  ok(idx !== null, "blorb parses");
  ok(idx && idx[3] && idx[3].off === off3 && idx[3].len === snd3.length, "Snd 3 offset/len (FORM incl. header)");
  ok(idx && idx[4] && idx[4].off === off4, "Snd 4 offset");
  ok(JSZM_zsBlorbSnds(base + ".nonexistent") === null, "missing file -> null");

  // ---- digital tier (audio APC + libsndfile) ----
  console.cterm_version = 1400; console._audio_reply = 1; wire = ""; input = [];
  var p = JSZM_makeZSound({ storyPath: storyPath, tag: "zmachine/test" });
  ok(p.tier === 2, "digital tier probed");
  ok(wire.indexOf("SyncTERM:Q;libsndfile") >= 0, "probe sent");

  wire = "";
  p.sound(1, 2, 0, 0);                       // high bleep
  ok(wire.indexOf("A;Synth") >= 0, "bleep is a Synth tone");
  ok(wire.indexOf("F=880") >= 0, "high bleep frequency");
  ok(wire.indexOf("A;Queue;C=3") >= 0, "bleep queues on the bleep channel");

  wire = "";
  p.sound(3, 2, 8, 0);                       // start sample 3 at full volume
  var st = wire.indexOf("C;S;zmachine/test/s3;");
  ok(st >= 0, "sample 3 stored");
  ok(wire.indexOf(base64_encode(snd3)) >= 0, "stored payload is the AIFF bytes");
  ok(wire.indexOf("A;Flush;C=2") >= 0 && wire.indexOf("A;Flush;C=2") < st, "channel flushed before play");
  ok(wire.indexOf("A;Load") > st, "loaded after store");
  ok(wire.indexOf("A;Queue;C=2") >= 0, "queued on the sample channel");
  ok(wire.indexOf("VL=100") >= 0, "volume 8 -> 100%");

  wire = "";
  p.sound(3, 2, 4, 0);                       // REPLAY: must re-Load, must NOT re-Store
  ok(wire.indexOf("C;S;zmachine/test/s3;") < 0, "replay does not re-upload");
  ok(wire.indexOf("A;Load") >= 0, "replay re-Loads (Queue empties the slot)");
  ok(wire.indexOf("A;Queue;C=2") >= 0, "replay queues");
  ok(wire.indexOf("VL=50") >= 0, "volume 4 -> 50%");

  wire = "";
  p.sound(3, 3, 0, 0);                       // stop
  ok(wire.indexOf("A;Flush;C=2") >= 0, "stop flushes the sample channel");
  ok(wire.indexOf("A;Queue") < 0, "stop queues nothing");

  wire = "";
  p.sound(4, 1, 0, 0);                       // prepare: upload only
  ok(wire.indexOf("C;S;zmachine/test/s4;") >= 0, "prepare uploads");
  ok(wire.indexOf("A;Queue") < 0, "prepare does not play");

  wire = "";
  p.sound(4, 2, 0, 255);                     // repeats 255 = loop forever
  ok(wire.indexOf("C;S;zmachine/test/s4;") < 0, "prepared sound not re-uploaded on start");
  ok(/A;Queue;C=2;[^\x1b]*;L\x1b/.test(wire), "repeats 255 queues looping");

  wire = "";
  p.sound(4, 2, 0, 3);                       // finite repeats: consecutive FIFO appends
  ok(wire.split("A;Queue;C=2").length - 1 === 3, "repeats 3 queues three times");

  wire = "";
  p.sound(4, 2, 8, 0, true);                 // chained start (issued inside a completion routine)
  ok(wire.indexOf("A;Flush") < 0, "chained start does not flush (appends behind current sound)");
  ok(wire.indexOf("A;Queue;C=2") >= 0, "chained start queues on the sample channel");

  wire = "";
  p.sound(99, 2, 0, 0);                      // unknown sound number
  ok(wire === "", "unknown sample is silent");

  wire = "";
  p.sound(0, 2, 0, 0);                       // number 0 = stop-all
  ok(wire.indexOf("A;Flush;C=2") >= 0, "sound 0 stops the sample channel");

  wire = "";
  p.stop();
  ok(wire.indexOf("A;Flush;C=2") >= 0 && wire.indexOf("A;Flush;C=3") >= 0, "stop() flushes both channels");

  // ---- missing .blb: sampled silent, bleeps still work ----
  wire = ""; input = [];
  var pnb = JSZM_makeZSound({ storyPath: system.temp_dir + "zsound_none_" + Date.now() + ".z3", tag: "zmachine/none" });
  wire = "";
  pnb.sound(3, 2, 8, 0);
  ok(wire === "", "no .blb -> sampled sound silent (no throw)");
  wire = "";
  pnb.sound(2, 2, 0, 0);
  ok(wire.indexOf("F=330") >= 0, "no .blb -> low bleep still tones");

  // ---- tone tier (audio APC, no libsndfile) ----
  console._audio_reply = 0; wire = ""; input = [];
  var pt = JSZM_makeZSound({ storyPath: storyPath, tag: "zmachine/test" });
  ok(pt.tier === 1, "tone tier probed");
  wire = "";
  pt.sound(1, 2, 0, 0);
  ok(wire.indexOf("A;Synth") >= 0, "tone tier bleeps via Synth");
  wire = "";
  pt.sound(3, 2, 8, 0);
  ok(wire === "", "tone tier: sampled sounds silent");

  // ---- BEL tier (not a cterm terminal: never queried, BEL bleeps) ----
  console.cterm_version = 0; console._audio_reply = null; wire = ""; input = [];
  var pb = JSZM_makeZSound({ storyPath: storyPath, tag: "zmachine/test" });
  ok(pb.tier === 0, "non-cterm -> BEL tier");
  ok(wire.indexOf("SyncTERM") < 0, "non-cterm terminal is never sent an APC/query");
  wire = "";
  pb.sound(1, 2, 0, 0);
  ok(wire === "\x07", "BEL tier bleep is a BEL");
  wire = "";
  pb.sound(3, 2, 8, 0);
  ok(wire === "", "BEL tier: sampled sounds silent");
  wire = "";
  pb.sound(3, 3, 0, 0);
  ok(wire === "", "BEL tier: stop emits nothing");
} finally {
  cleanup();
}

// ---- engine integration: @sound_effect operand decode through jszm run() ----
// Hand-assembled micro-stories: VAR 245 (0xF5) with the operand shapes the real
// games use (txd-verified), then QUIT (0xBA). Dictionary addr 0 = jszm's
// default-separators path, so no vocab table is needed.
load(js.exec_dir + "../quetzal.js");   // must precede jszm.js
load(js.exec_dir + "../jszm.js");

function mkStory(version, code) {
  var m = new Uint8Array(256), i;
  m[0] = version;
  m[7] = 0x50;                         // 0x06: initial PC = raw byte address (all non-v6 versions)
  for (i = 0; i < code.length; i++) m[0x50 + i] = code[i];
  return m;
}
function runStory(version, code, withHook, routine) {
  var m = mkStory(version, code), i;
  if (routine) {                       // optional completion routine at 0x80 (packed 0x20)
    m[0x80] = 0;                       // routine header: 0 locals
    for (i = 0; i < routine.length; i++) m[0x81 + i] = routine[i];
  }
  var g = new JSZM(m), calls = [];
  g.print = function () { }; g.highlight = function () { };
  g.updateStatusLine = function () { }; g.read = function () { return null; };
  if (withHook)
    g.sound = function (n, fx, vol, rep, chained) { calls.push([n, fx, vol, rep, !!chained]); };
  g.run();
  return { calls: calls, flags2: g.get(16) };
}
function callEq(c, e) {
  return c && c.length === 5 && c[0] === e[0] && c[1] === e[1] && c[2] === e[2]
      && c[3] === e[3] && c[4] === e[4];
}

var r3 = runStory(3, [
  0xF5, 0x57, 3, 2, 69,                // sound_effect 3, 2, 69   (Lurking Horror shape)
  0xF5, 0x7F, 1,                       // sound_effect 1          (bare bleep)
  0xF5, 0xFF,                          // sound_effect            (no operands: Beyond Zork)
  0xBA                                 // quit
], true);
ok(r3.calls.length === 3, "v3: three hook calls");
ok(callEq(r3.calls[0], [3, 2, 69, 0, false]), "v3: number/effect/volume decoded, no repeats");
ok(callEq(r3.calls[1], [1, 2, 0, 0, false]), "v3: bare bleep defaults effect=start");
ok(callEq(r3.calls[2], [1, 2, 0, 0, false]), "v3: zero-operand form bleeps (Beyond Zork)");

var r5 = runStory(5, [
  0xF5, 0x53, 3, 2, 0x03, 0x45,        // sound_effect 3, 2, $0345 (repeats 3, volume 0x45)
  0xF5, 0x53, 4, 3, 0xFF, 0x00,        // sound_effect 4, 3, $FF00 (stop; repeats 255)
  0xBA
], true);
ok(r5.calls.length === 2, "v5: two hook calls");
ok(callEq(r5.calls[0], [3, 2, 0x45, 3, false]), "v5: volume low byte / repeats high byte split");
ok(callEq(r5.calls[1], [4, 3, 0, 255, false]), "v5: repeats 255 (forever) decoded");
ok((r5.flags2 & 0x80) === 0x80, "v5: Flags2 bit7 set when sound hook present");

// Completion routine: runs immediately after a non-looping start; a sound the
// routine starts is flagged chained (the Sherlock ambient-resume/chime shape).
// Main: sound_effect 3, 2, $0008, routine@0x20-packed; routine: sound_effect 4.
var rc = runStory(5,
  [0xF5, 0x51, 3, 2, 0x00, 0x08, 0x20, 0xBA],
  true,
  [0xF5, 0x7F, 4, 0xB0]);             // sound_effect 4; rtrue
ok(rc.calls.length === 2, "routine: ran immediately after the start");
ok(callEq(rc.calls[0], [3, 2, 8, 0, false]), "routine: the triggering start is unchained");
ok(callEq(rc.calls[1], [4, 2, 0, 0, true]), "routine: its follow-on sound is chained");

// Looping start (repeats 255): never finishes, so the routine must NOT run --
// this is what stops Sherlock's default resume-routine recursing forever.
var rl = runStory(5,
  [0xF5, 0x51, 6, 2, 0xFF, 0x08, 0x20, 0xBA],
  true,
  [0xF5, 0x7F, 4, 0xB0]);
ok(rl.calls.length === 1, "routine: looping start fires no callback");
ok(callEq(rl.calls[0], [6, 2, 8, 255, false]), "routine: looping start itself still plays");

// Stop effect with a routine operand: no callback either (only starts finish).
var rs = runStory(5,
  [0xF5, 0x51, 3, 3, 0x00, 0x08, 0x20, 0xBA],
  true,
  [0xF5, 0x7F, 4, 0xB0]);
ok(rs.calls.length === 1, "routine: stop effect fires no callback");

var r5n = runStory(5, [0xF5, 0x7F, 1, 0xBA], false);       // no hook wired
ok((r5n.flags2 & 0x80) === 0, "v5: Flags2 bit7 clear without a sound hook");

var r3n = runStory(3, [0xF5, 0x57, 3, 2, 69, 0xBA], false);
ok(r3n.calls.length === 0, "v3: no hook -> silent no-op (no crash)");

if (fails) throw new Error(fails + " zsound test(s) failed");
print("ZSOUND OK");
