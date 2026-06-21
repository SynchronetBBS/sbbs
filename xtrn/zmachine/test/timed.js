// Headless tests for timed input (jszm.js engine + door idle policy).
// Run: jsexec test/timed.js
load(js.exec_dir + "../quetzal.js");
load(js.exec_dir + "../jszm.js");

var fails = 0;
function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }

// Minimal v5 story: 64-byte header, code at 0x40, interrupt routine at 0x60
// (0 locals; `inc g16` to count firings; routineRet = 0xB1 rfalse / 0xB0 rtrue).
// globals @ 0x180 (g16@0x180, g17@0x182), static base @ 0x200.
function story(codeBytes, routineRet) {
  var m = new Uint8Array(0x300);
  m[0] = 5;                                  // version 5
  m[6] = 0x00; m[7] = 0x40;                  // initial PC = 0x40
  m[12] = 0x01; m[13] = 0x80;                // globals table @ 0x180
  m[14] = 0x02; m[15] = 0x00;                // static base @ 0x200
  var i;
  for (i = 0; i < codeBytes.length; i++) m[0x40 + i] = codeBytes[i];
  m[0x60] = 0x00;                            // routine: 0 locals
  m[0x61] = 0x95; m[0x62] = 0x10;            // inc g16
  m[0x63] = routineRet;                      // rfalse (keep waiting) / rtrue (abort)
  return m;
}

// JSZM with headless stubs; `script` feeds readTimedKey, `echo` records echoInput.
function engine(m, script, echo) {
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.readTimedKey = function () { return script.shift(); };
  if (echo) g.echoInput = function (x) { echo.push(x); };
  return g;
}

// --- read_char timed: 3 ticks then key 'g'(103); routine returns false ---
(function () {
  // read_char op0=1 op1=10 op2=0x18(routine); store -> g17(0x11); then quit.
  var code = [0xF6, 0x57, 0x01, 0x0A, 0x18, 0x11, 0xBA];
  var g = engine(story(code, 0xB1), [-1, -1, -1, 103]);
  try { g.run(); } catch (e) { ok(false, "read_char timed threw: " + e); }
  ok(g.getu(0x180) === 3, "read_char: routine fired 3x (was " + g.getu(0x180) + ")");
  ok(g.getu(0x182) === 103, "read_char: stored key 'g'=103 (was " + g.getu(0x182) + ")");
})();

// --- read_char timed abort: routine returns true on first tick -> store 0 ---
(function () {
  var code = [0xF6, 0x57, 0x01, 0x0A, 0x18, 0x11, 0xBA];
  var g = engine(story(code, 0xB0), [-1]);
  try { g.run(); } catch (e) { ok(false, "read_char abort threw: " + e); }
  ok(g.getu(0x180) === 1, "read_char abort: routine fired once");
  ok(g.getu(0x182) === 0, "read_char abort: stored terminator 0");
})();

// --- read_char disconnect: readTimedKey returns null -> run() unwinds, nothing stored ---
(function () {
  var code = [0xF6, 0x57, 0x01, 0x0A, 0x18, 0x11, 0xBA];
  var g = engine(story(code, 0xB1), [null]);   // immediate disconnect
  var threw = false;
  try { g.run(); } catch (e) { threw = true; }
  ok(!threw, "read_char disconnect: run() unwinds cleanly (no throw)");
  ok(g.getu(0x182) === 0, "read_char disconnect: nothing stored (g17 still 0)");
})();

// Story whose interrupt routine CLOBBERS the shared op vars: `storew 0x190 0 0`
// (VAR 3-operand -> sets op0/op1/op2) then `inc g16` then rfalse. A real interrupt
// routine executes such instructions; a timed loop that reads op1/op2 AFTER the
// routine ran would see garbage. (Routine @0x60 = packed 0x18.)
function clobberStory(code) {
  var m = new Uint8Array(0x300);
  m[0] = 5; m[6] = 0x00; m[7] = 0x40;        // v5, PC=0x40
  m[12] = 0x01; m[13] = 0x80;                // globals @ 0x180
  m[14] = 0x02; m[15] = 0x00;                // static base @ 0x200
  var i;
  for (i = 0; i < code.length; i++) m[0x40 + i] = code[i];
  var r = [0x00, 0xE1, 0x17, 0x01, 0x90, 0x00, 0x00, 0x95, 0x10, 0xB1], j;  // storew 0x190 0 0 ; inc g16 ; rfalse
  for (j = 0; j < r.length; j++) m[0x60 + j] = r[j];
  return m;
}

// --- read_char: interrupt routine clobbers op2; the loop must still fire the SAME
//     routine on every slice. (Catches the op1/op2-across-runInterrupt clobber bug
//     the inc-only routine above missed -- inc never touches op1/op2.) ---
(function () {
  var code = [0xF6, 0x57, 0x01, 0x0A, 0x18, 0x11, 0xBA];   // read_char op1=10 op2=routine(0x18) -> g17
  var g = engine(clobberStory(code), [-1, -1, 103], null); // 2 slices then key
  try { g.run(); } catch (e) { ok(false, "read_char clobber threw: " + e); }
  ok(g.getu(0x180) === 2, "read_char: routine fires both slices despite op2 clobber (was " + g.getu(0x180) + ")");
  ok(g.getu(0x182) === 103, "read_char clobber: stored key 103");
})();

// --- read (line): the timed line path captures time/routine as params, so it is
//     immune to the op clobber -- fires the routine on every slice regardless. ---
(function () {
  var code = [0xE4, 0x15, 0x01, 0x00, 0x00, 0x0A, 0x18, 0x11, 0xBA];   // aread op2=10 op3=routine(0x18)
  var m = clobberStory(code); m[0x100] = 10;
  var g = engine(m, [-1, -1, 13], []);                     // 2 slices then Enter
  try { g.run(); } catch (e) { ok(false, "read clobber threw: " + e); }
  ok(g.getu(0x180) === 2, "read: routine fires both slices despite op clobber (line path immune)");
})();

// --- read (aread) timed: 'g', tick, 'o', Enter; routine returns false ---
(function () {
  // aread op0=0x0100(text buf, large) op1=0(no parse) op2=10(time) op3=0x18(routine); store g17.
  var code = [0xE4, 0x15, 0x01, 0x00, 0x00, 0x0A, 0x18, 0x11, 0xBA];
  var m = story(code, 0xB1);
  m[0x100] = 10;                              // text-buffer max length
  var echo = [];
  var g = engine(m, [103, -1, 111, 13], echo);
  var prompts = 0; g.readPrompt = function () { prompts++; };
  try { g.run(); } catch (e) { ok(false, "read timed threw: " + e); }
  ok(g.getu(0x180) === 1, "read: routine fired once");
  ok(g.mem[0x101] === 2, "read: stored count 2 (was " + g.mem[0x101] + ")");
  ok(g.mem[0x102] === 103 && g.mem[0x103] === 111, "read: buffer holds 'go'");
  ok(g.getu(0x182) === 13, "read: stored terminator 13 (Return)");
  ok(echo.length === 3 && echo[0] === 103 && echo[1] === 111 && echo[2] === "\r\n",
     "read: echoed 'g','o' then a newline on Enter");
  ok(prompts === 1, "read: readPrompt fired once at the prompt");
})();

// --- read timed abort: 'g' then tick aborts (routine true) -> 'g' kept, terminator 0 ---
(function () {
  var code = [0xE4, 0x15, 0x01, 0x00, 0x00, 0x0A, 0x18, 0x11, 0xBA];
  var m = story(code, 0xB0); m[0x100] = 10;
  var g = engine(m, [103, -1], []);
  try { g.run(); } catch (e) { ok(false, "read abort threw: " + e); }
  ok(g.mem[0x101] === 1 && g.mem[0x102] === 103, "read abort: 'g' left in buffer");
  ok(g.getu(0x182) === 0, "read abort: stored terminator 0");
})();

// --- routine==0 falls back to the untimed path (self.read); readTimedKey NOT used ---
(function () {
  // aread op3=0 (no routine) -> untimed. self.read returns "go".
  var code = [0xE4, 0x15, 0x01, 0x00, 0x00, 0x0A, 0x00, 0x11, 0xBA];
  var m = story(code, 0xB1); m[0x100] = 10;
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; };
  var timedUsed = false;
  g.readTimedKey = function () { timedUsed = true; return null; };
  g.read = function () { return "go"; };
  try { g.run(); } catch (e) { ok(false, "fallback threw: " + e); }
  ok(!timedUsed, "routine==0 -> untimed path (readTimedKey not called)");
  ok(g.mem[0x101] === 2 && g.getu(0x182) === 13, "fallback filled buffer + stored 13");
})();

// --- header capability: Flags1 bit7 set in v4+ with readTimedKey, clear in v3 ---
(function () {
  var m = story([0xBA], 0xB1);                // just quit
  var g = engine(m, [], null);
  try { g.run(); } catch (e) {}
  ok((g.mem[1] & 0x80) !== 0, "v5: Flags1 bit7 set (timed input available)");

  var m3 = story([0xBA], 0xB1); m3[0] = 3;    // v3: no timed input
  var g3 = engine(m3, [], null);
  try { g3.run(); } catch (e) {}
  ok((g3.mem[1] & 0x80) === 0, "v3: Flags1 bit7 not set");
})();

// --- afterInterrupt: when a timed-read interrupt routine PRINTS, the engine calls
//     game.afterInterrupt(typedSoFar) so the door can repaint the status (live clock)
//     or re-show the in-progress input. Routine @0x60 here prints (new_line) then inc+rfalse. ---
(function () {
  function pstory(code) {
    var m = new Uint8Array(0x300);
    m[0] = 5; m[6] = 0x00; m[7] = 0x40; m[12] = 0x01; m[13] = 0x80; m[14] = 0x02; m[15] = 0x00;
    var i; for (i = 0; i < code.length; i++) m[0x40 + i] = code[i];
    var r = [0x00, 0xBB, 0x95, 0x10, 0xB1], j;   // 0 locals ; new_line (prints) ; inc g16 ; rfalse
    for (j = 0; j < r.length; j++) m[0x60 + j] = r[j];
    return m;
  }
  var code = [0xE4, 0x15, 0x01, 0x00, 0x00, 0x0A, 0x18, 0x11, 0xBA];   // aread op2=10 op3=routine(0x18)
  var m = pstory(code); m[0x100] = 10;
  var calls = [];
  var g = engine(m, [103, -1, 13], []);          // type 'g', tick (interrupt prints), Enter
  g.afterInterrupt = function (typed) { calls.push(typed); };
  try { g.run(); } catch (e) { ok(false, "afterInterrupt threw: " + e); }
  ok(calls.length === 1, "afterInterrupt called once when the interrupt printed (was " + calls.length + ")");
  ok(calls[0] === "g", "afterInterrupt received the in-progress input 'g' (was '" + calls[0] + "')");
})();

// --- door idle policy (pure jszm_idle_decision) ---
var JSZM_DOOR_NO_MAIN = true;
load(js.exec_dir + "../zmachine.js");
(function () {
  // jszm_idle_decision(idle, warn, max, warned) -> { warn, hangup }
  var d;
  d = jszm_idle_decision(10, 240, 300, false);  ok(!d.warn && !d.hangup, "idle 10s: silent");
  d = jszm_idle_decision(250, 240, 300, false); ok(d.warn && !d.hangup, "idle 250s: warn, no hangup");
  d = jszm_idle_decision(250, 240, 300, true);  ok(!d.warn && !d.hangup, "idle 250s already warned: silent");
  d = jszm_idle_decision(305, 240, 300, false); ok(d.hangup && d.warn, "idle 305s: hangup (warn also set)");
})();

// --- jszm_flush_resume codec scope: the flush is a TOP-LEVEL function, but quetzal is
//     load()ed inside jszm_door_main, so it must use the captured jszm_quetzal reference.
//     Regression for the silent "quetzal is not defined" that lost every disconnect's
//     resume slot (auto-resume never persisted on a live drop). ---
(function () {
  var orig = new Uint8Array(0x40);
  orig[2] = 0; orig[3] = 1;                              // release
  orig[0x1c] = 0x12; orig[0x1d] = 0x34;                  // checksum
  var i; for (i = 0; i < 6; i++) orig[0x12 + i] = i + 1; // serial
  var form = quetzal.write({ mem: orig, orig: orig, dynsize: 0x40, pc: 0x40, cs: [], ds: [] });
  var fp = "/tmp/zztest_flush.resume";
  try { if (file_exists(fp)) file_remove(fp); } catch (e) {}
  jszm_resume_done = false;
  jszm_resume_game = { checkpoint: form };
  jszm_resume_path = fp;
  jszm_resume_screen = "a screen recap";
  jszm_quetzal = null;                                   // codec unavailable -> must no-op (no throw, no write)
  jszm_flush_resume();
  ok(!file_exists(fp), "flush no-ops when codec unavailable (no ReferenceError, no slot written)");
  jszm_quetzal = quetzal;                                // codec captured -> writes the slot
  jszm_resume_upper = "1\n Your Compartment            17:26";   // a captured status bar
  jszm_flush_resume();
  ok(file_exists(fp), "flush writes the resume slot when jszm_quetzal is set");
  // the slot must carry BOTH the lower recap (Rcap) and the upper-window model (Ucap)
  var raw = (function () { var g = new File(fp); g.open("rb"); var b = g.readBin(1, g.length); g.close(); return b; })();
  var rcapBack = quetzal.getChunk(raw, "Rcap");
  var ucapBack = quetzal.getChunk(raw, "Ucap");
  ok(!!rcapBack, "slot carries the Rcap (lower-window recap) chunk");
  ok(!!ucapBack, "slot carries the Ucap (upper-window status) chunk");
  var ustr = ""; if (ucapBack) { for (i = 0; i < ucapBack.length; i++) ustr += String.fromCharCode(ucapBack[i] & 255); }
  ok(ustr === jszm_resume_upper, "Ucap round-trips the captured status bar verbatim");
  jszm_resume_upper = "";
  try { if (file_exists(fp)) file_remove(fp); } catch (e) {}
})();

// --- title resolution helpers (IFID construction + iFiction/cache parsing; no network) ---
(function () {
  var ids = jszm_zcodeIfids(9, "871008", "2B37", null);
  ok(ids.length === 2 && ids[0] === "ZCODE-9-871008-2B37" && ids[1] === "ZCODE-9-871008",
     "ZCODE IFIDs built with and without the checksum suffix");
  ids = jszm_zcodeIfids(9, "060321", "76BD", "e9fd3d87-dd2f-4005-b332-23557780b64e");
  ok(ids[0] === "E9FD3D87-DD2F-4005-B332-23557780B64E", "embedded UUID tried first, upper-cased");
  ok(jszm_zcodeIfids(1, "ab cd!", "0000", null)[1] === "ZCODE-1-ab-cd-",
     "non-alphanumeric serial bytes become hyphens");
  ok(jszm_parseIfdbTitle("<bibliographic><title>Border Zone</title></bibliographic><format><title>Story File</title></format>") === "Border Zone",
     "bibliographic <title> parsed, not the <format> one");
  ok(jszm_parseIfdbTitle("<bibliographic><title>A &amp; B &#67;</title></bibliographic>") === "A & B C",
     "XML entities decoded");
  ok(jszm_parseIfdbTitle("<x>no title here</x>") === "", "no bibliographic title -> empty");
  var c = jszm_parseTitleCache("# c\nborder_zone.z5 = Border Zone\n; c2\nunknown.z5 = \nzork1.z3=Zork I\n");
  ok(c["border_zone.z5"] === "Border Zone" && c["zork1.z3"] === "Zork I", "cache key=value lines parsed");
  ok(c.hasOwnProperty("unknown.z5") && c["unknown.z5"] === "", "empty value kept as a negative-cache entry");
})();

// --- chooser labels: disambiguate same-title files by release (then serial) ---
(function () {
  var L = jszm_labelStories([
    { title: "Zork III", ver: 3, rel: 17, serial: "840727" },
    { title: "Zork III", ver: 3, rel: 25, serial: "860811" },
    { title: "Border Zone", ver: 5, rel: 9, serial: "871008" },
    { title: "Twins", ver: 5, rel: 1, serial: "000001" },
    { title: "Twins", ver: 5, rel: 1, serial: "000002" }
  ]);
  ok(L[0].label === "Border Zone (v5)", "unique title gets no discriminator");
  ok(L[1].label === "Twins (v5, release 1 / 000001)" && L[2].label === "Twins (v5, release 1 / 000002)",
     "title+release tie falls through to the serial");
  ok(L[3].label === "Zork III (v3, release 17)" && L[4].label === "Zork III (v3, release 25)",
     "shared title disambiguated by release; sorted by title then release");
})();

if (fails) throw new Error(fails + " timed test(s) failed");
print("TIMED OK");
