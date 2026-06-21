// Headless unit tests for the Quetzal codec (quetzal.js).
// Run: jsexec /share/sbbs/xtrn/zmachine/test/quetzal.js
load(js.exec_dir + "../quetzal.js");

var fails = 0;
function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }
function eq(a, b) {
  if (a.length !== b.length) return false;
  for (var i = 0; i < a.length; i++) if ((a[i] & 255) !== (b[i] & 255)) return false;
  return true;
}

// --- Task 1: IFF container round-trip ---
(function () {
  var form = quetzal.buildForm([
    { id: "IFhd", data: [1, 2, 3] },          // odd length -> must be padded
    { id: "CMem", data: [9, 8, 7, 6] }
  ]);
  ok(String.fromCharCode(form[0], form[1], form[2], form[3]) === "FORM", "starts with FORM");
  ok(String.fromCharCode(form[8], form[9], form[10], form[11]) === "IFZS", "form type IFZS");
  ok(eq(quetzal.getChunk(form, "IFhd"), [1, 2, 3]), "getChunk IFhd");
  ok(eq(quetzal.getChunk(form, "CMem"), [9, 8, 7, 6]), "getChunk CMem");
  ok(quetzal.getChunk(form, "Stks") === null, "missing chunk -> null");
  ok((form.length & 1) === 0, "form total length is even (padding)");

  var form2 = quetzal.addChunk(form, "Rcap", [65, 66, 67]);   // "ABC"
  ok(eq(quetzal.getChunk(form2, "Rcap"), [65, 66, 67]), "addChunk Rcap round-trips");
  ok(eq(quetzal.getChunk(form2, "IFhd"), [1, 2, 3]), "addChunk preserves existing chunks");
})();

// --- Task 2: CMem XOR-delta + RLE round-trip ---
(function () {
  function mkU8(arr) { var u = new Uint8Array(arr.length); for (var i = 0; i < arr.length; i++) u[i] = arr[i]; return u; }
  var orig = mkU8([10, 20, 30, 40, 50, 60, 70, 80]);

  // identical memory -> empty CMem; decode reproduces orig
  var same = mkU8([10, 20, 30, 40, 50, 60, 70, 80]);
  var enc0 = quetzal.cmemEncode(same, orig, 8);
  ok(enc0.length === 0, "no changes -> empty CMem (was " + enc0.length + ")");
  ok(eq(quetzal.cmemDecode(enc0, orig, 8), same), "empty CMem decodes to orig");

  // a couple of changes with unchanged runs between and trailing unchanged tail
  var mod = mkU8([10, 99, 30, 40, 50, 7, 70, 80]);   // changed at idx 1 and 5; idx 6,7 unchanged (trailing dropped)
  var enc = quetzal.cmemEncode(mod, orig, 8);
  ok(eq(quetzal.cmemDecode(enc, orig, 8), mod), "CMem round-trips with runs + trailing tail");
  ok(enc.length < 8, "CMem smaller than raw for sparse changes (was " + enc.length + ")");

  // long zero run > 256 unchanged bytes between two changes
  var n = 600, big = [], bigm = [];
  for (var i = 0; i < n; i++) { big.push(i & 255); bigm.push(i & 255); }
  bigm[0] = (big[0] ^ 0x5a) & 255; bigm[n - 1] = (big[n - 1] ^ 0x33) & 255;
  var bo = mkU8(big), bm = mkU8(bigm);
  ok(eq(quetzal.cmemDecode(quetzal.cmemEncode(bm, bo, n), bo, n), bm), "CMem handles >256 zero run");
})();

// --- Task 3: full write/read round-trip incl. Stks frame mapping ---
(function () {
  function mkU8(arr) { var u = new Uint8Array(arr.length); for (var i = 0; i < arr.length; i++) u[i] = arr[i]; return u; }
  function locals(arr) { var l = new Int16Array(arr.length); for (var i = 0; i < arr.length; i++) l[i] = arr[i]; return l; }
  function i16eq(a, b) { if (a.length !== b.length) return false; for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false; return true; }

  // 64-byte header + a little dynamic memory; put plausible store-bytes at the return pcs.
  var dynsize = 80;
  var orig = new Uint8Array(dynsize);
  orig[2] = 0x00; orig[3] = 0x2A;                 // release = 42
  var serial = [0x38, 0x31, 0x30, 0x37, 0x32, 0x36]; // "810726"
  for (var s = 0; s < 6; s++) orig[0x12 + s] = serial[s];
  orig[0x1C] = 0xAB; orig[0x1D] = 0xCD;           // checksum
  var mem = new Uint8Array(orig);                  // start equal
  mem[0x40] = 0x77; mem[0x41] = 0x05;              // a couple of dynamic changes
  mem[70] = 0x10;                                  // result-store byte for frame retA (var 0x10 = global)
  mem[60] = 0x05;                                  // result-store byte for frame retMain (local 5)

  // jszm state: main -> A -> B (B current). ds = B's eval stack.
  var ds = [111, 222];
  var cs = [
    { ds: [333], pc: 70, local: locals([1, 2]) },     // B's frame: returns into A at store-byte 70
    { ds: [444, 555], pc: 60, local: locals([7]) }    // A's frame: returns into main at store-byte 60
  ];
  var pc = 0x1234;

  var blob = quetzal.write({ mem: mem, orig: orig, dynsize: dynsize, pc: pc, cs: cs, ds: ds });
  ok(String.fromCharCode(blob[0], blob[1]) === "FO", "write produces a FORM");

  var r = quetzal.read(blob, orig, dynsize);
  ok(r !== null, "read succeeds");
  ok(r.release === 42, "release round-trips (was " + r.release + ")");
  ok(r.checksum === 0xABCD, "checksum round-trips");
  ok(eq(r.serial, serial), "serial round-trips");
  ok(r.pc === pc, "pc round-trips (was " + r.pc + ")");
  ok(eq(r.dyn, mem), "dynamic memory round-trips (CMem)");

  // stacks
  ok(eq(r.ds, ds), "active ds round-trips");
  ok(r.cs.length === 2, "two frames restored (was " + r.cs.length + ")");
  ok(r.cs[0].pc === 70 && i16eq(r.cs[0].local, cs[0].local) && eq(r.cs[0].ds, [333]), "frame 0 (B) round-trips");
  ok(r.cs[1].pc === 60 && i16eq(r.cs[1].local, cs[1].local) && eq(r.cs[1].ds, [444, 555]), "frame 1 (A) round-trips");

  // UMem read path
  var um = quetzal.buildForm([
    { id: "IFhd", data: quetzal.getChunk(blob, "IFhd") },
    { id: "UMem", data: (function () { var d = []; for (var i = 0; i < dynsize; i++) d.push(mem[i]); return d; })() },
    { id: "Stks", data: quetzal.getChunk(blob, "Stks") }
  ]);
  var ru = quetzal.read(um, orig, dynsize);
  ok(ru !== null && eq(ru.dyn, mem), "UMem save reads back");

  // The dummy/root frame's flags byte (Stks byte 3, after the 3-byte retpc) must
  // have the discard bit (0x10) CLEAR -- real interpreters (frotz) reject a save
  // whose initial frame sets it. Our own reader ignores the bit, so only this
  // explicit check (and a dfrotz restore) guards it.
  var stk = quetzal.getChunk(blob, "Stks");
  ok((stk[3] & 0x10) === 0, "dummy frame discard bit is clear (frotz-compatible, was " + (stk[3] & 0x10) + ")");
})();

if (fails) throw new Error(fails + " quetzal test(s) failed");
print("QUETZAL OK");
