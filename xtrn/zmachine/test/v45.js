// Headless unit tests for v4/5/8 foundation (jszm.js). Run: jsexec .../test/v45.js
load(js.exec_dir + "../quetzal.js");
load(js.exec_dir + "../jszm.js");

var fails = 0;
function ok(c, m) { if (!c) { print("FAIL: " + m); fails++; } }

// Minimal valid header (64 bytes), version v, object-table at otAddr.
function hdr(v, otAddr) {
  var m = new Uint8Array(512);
  m[0] = v;
  m[10] = (otAddr >> 8) & 255; m[11] = otAddr & 255;   // 0x0A object table
  m[12] = 0; m[13] = 64;                                 // 0x0C globals (anywhere valid)
  m[14] = 1; m[15] = 0;                                  // 0x0E static base (PURBOT) = 256
  m[6] = 0; m[7] = 64;                                   // 0x06 initial pc
  return m;
}

// --- Task 1: version acceptance + params ---
(function () {
  ok(JSZM.versionParams(3).addrShift === 1 && JSZM.versionParams(3).objEntry === 9, "v3 params");
  ok(JSZM.versionParams(5).addrShift === 2 && JSZM.versionParams(5).objEntry === 14, "v5 params");
  ok(JSZM.versionParams(8).addrShift === 3 && JSZM.versionParams(8).objEntry === 14, "v8 params");
  ok(JSZM.versionParams(2) === null, "v2 -> null (unsupported)");
  ok(JSZM.versionParams(6) !== null, "v6 -> params (now supported)");

  var g = new JSZM(hdr(5, 100));
  ok(g.version === 5 && g.vp.objEntry === 14, "ctor sets version + vp for v5");
  var threw = false; try { new JSZM(hdr(6, 100)); } catch (e) { threw = true; }
  ok(!threw, "ctor accepts v6");
  var threw3 = false; try { new JSZM(hdr(3, 100)); } catch (e) { threw3 = true; }
  ok(!threw3, "ctor still accepts v3");
})();

// Build a story image with one object table holding `nobj` objects.
// fields[o] = {attrs:[bytes], parent, sibling, child, prop}. v: version.
function withObjects(v, fields) {
  var vp = JSZM.versionParams(v), ot = 100, dp = vp.defProps * 2;
  var m = hdr(v, ot);
  var base = ot + dp;                                  // first object entry (1-based math uses objectsAdj)
  for (var o = 1; o < fields.length; o++) {
    var f = fields[o], a = base + (o - 1) * vp.objEntry, i;
    for (i = 0; i < vp.attrBytes; i++) m[a + i] = (f.attrs && f.attrs[i]) || 0;
    var fo = a + vp.attrBytes;
    function wf(off, val) { if (vp.objNumWidth === 1) m[off] = val & 255; else { m[off] = (val >> 8) & 255; m[off + 1] = val & 255; } }
    wf(fo, f.parent); wf(fo + vp.objNumWidth, f.sibling); wf(fo + 2 * vp.objNumWidth, f.child);
    var po = fo + 3 * vp.objNumWidth; m[po] = (f.prop >> 8) & 255; m[po + 1] = f.prop & 255;
  }
  return new JSZM(m);
}

// --- Task 2: object-tree accessors (v3 and v5) ---
[3, 5].forEach(function (v) {
  var g = withObjects(v, [null,
    { attrs: [], parent: 0, sibling: 2, child: 3, prop: 0x1234 },   // obj 1
    { attrs: [], parent: 1, sibling: 0, child: 0, prop: 0x5678 }    // obj 2
  ]);
  g.mem = g.memInit; g.objects = g.getu(10) - 2 + g.vp.objectsAdj;   // mimic init() base
  ok(g.objField(1, 0) === 0 && g.objField(1, 1) === 2 && g.objField(1, 2) === 3, "v" + v + " obj1 parent/sibling/child");
  ok(g.objField(2, 0) === 1, "v" + v + " obj2 parent");
  ok(g.objPropTable(1) === 0x1234 && g.objPropTable(2) === 0x5678, "v" + v + " prop-table addrs");
  g.objSetField(1, 1, 9);                                            // set sibling
  ok(g.objField(1, 1) === 9, "v" + v + " objSetField sibling");
});

// --- Task 3: attribute accessors (v3: 0..31, v5: 0..47) ---
[ {v:3, hi:31}, {v:5, hi:47} ].forEach(function (t) {
  var g = withObjects(t.v, [null, { parent:0, sibling:0, child:0, prop:0x100 }]);
  g.mem = new Uint8Array(g.memInit); g.objects = g.getu(10) - 2 + g.vp.objectsAdj;
  ok(g.getAttr(1, 0) === 0, "v" + t.v + " attr 0 clear initially");
  g.setAttr(1, 0); g.setAttr(1, t.hi); g.setAttr(1, 7);
  ok(g.getAttr(1, 0) === 1 && g.getAttr(1, t.hi) === 1 && g.getAttr(1, 7) === 1, "v" + t.v + " set attrs 0/7/hi");
  ok(g.getAttr(1, 1) === 0, "v" + t.v + " attr 1 still clear");
  g.clearAttr(1, 7);
  ok(g.getAttr(1, 7) === 0 && g.getAttr(1, 0) === 1, "v" + t.v + " clearAttr 7 leaves 0 set");
});

// --- Task 4: property header decode (v3 + v5 forms) ---
(function () {
  // v3: build a prop table with name-len 0, then prop#5 size 2, prop#3 size 1, terminator 0.
  var g3 = withObjects(3, [null, { parent:0,sibling:0,child:0, prop:200 }]);
  var m3 = new Uint8Array(g3.memInit);
  m3[200] = 0;                       // short-name length (0 words)
  m3[201] = ((2 - 1) << 5) | 5;      // size 2, prop 5
  m3[202] = 0xAA; m3[203] = 0xBB;
  m3[204] = ((1 - 1) << 5) | 3;      // size 1, prop 3
  m3[205] = 0xCC;
  m3[206] = 0;                       // terminator
  g3.mem = m3; g3.objects = g3.getu(10) - 2 + g3.vp.objectsAdj;
  var z = g3.firstProp(1); var h = g3.propHeader(z);
  ok(h[0] === 5 && h[1] === 2 && h[2] === 202 && h[3] === 204, "v3 first prop = #5 size2");
  var h2 = g3.propHeader(204);
  ok(h2[0] === 3 && h2[1] === 1 && h2[2] === 205, "v3 next prop = #3 size1");

  // v5: 2-byte header form, then 1-byte form.
  var g5 = withObjects(5, [null, { parent:0,sibling:0,child:0, prop:200 }]);
  var m5 = new Uint8Array(g5.memInit);
  m5[200] = 0;                       // short-name length
  m5[201] = 0x80 | 40;               // 2-byte header, prop 40
  m5[202] = 0x80 | 3;                // size byte: low6 = 3 (top bit ignored by &63)
  m5[203] = 1; m5[204] = 2; m5[205] = 3;
  m5[206] = 0x40 | 7;                // 1-byte header, size bit set -> size 2, prop 7
  m5[207] = 9; m5[208] = 8;
  m5[209] = 0;                       // terminator
  g5.mem = m5; g5.objects = g5.getu(10) - 2 + g5.vp.objectsAdj;
  var z5 = g5.firstProp(1); var p = g5.propHeader(z5);
  ok(p[0] === 40 && p[1] === 3 && p[2] === 203 && p[3] === 206, "v5 first prop = #40 size3 (2-byte hdr)");
  var p2 = g5.propHeader(206);
  ok(p2[0] === 7 && p2[1] === 2 && p2[2] === 207, "v5 next prop = #7 size2 (1-byte hdr)");
})();

// --- Task 5: packed-address unpack per version ---
(function () {
  var g4 = new JSZM(hdr(4, 100)); ok(g4.unpack(0x1000) === 0x4000, "v4 unpack x4");
  var g5 = new JSZM(hdr(5, 100)); ok(g5.unpack(0x1000) === 0x4000, "v5 unpack x4");
  var g8 = new JSZM(hdr(8, 100)); ok(g8.unpack(0x1000) === 0x8000, "v8 unpack x8");
  var g3 = new JSZM(hdr(3, 100)); ok(g3.unpack(0x1000) === 0x2000, "v3 unpack x2");
})();

// --- Task 6: czech.z5 loads, object/property reads sane, v4+ header fields written ---
(function () {
  function readStory(p) { var f = new File(p); if (!f.open("rb")) throw new Error("open " + p); var b = f.readBin(1, f.length); f.close(); return new Uint8Array(b); }
  var g = new JSZM(readStory(js.exec_dir + "stories/czech.z5"));
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; };
  g.read = function () { return null; };
  try { g.run(); } catch (e) {}                 // init() ran (header written) before any unimplemented opcode
  ok(g.version === 5 && g.vp.objEntry === 14, "czech.z5 is v5");
  ok(g.objects > 0 && g.objPropTable(1) > 0 && g.objPropTable(1) < g.mem.length, "object 1 prop-table addr in range");
  ok(g.mem[0x1f] === 1, "header: interpreter version byte written");
  ok(g.mem[0x20] > 0 && g.mem[0x21] > 0, "header: screen rows/cols written");
})();

// --- Plan2 T1: EXT dispatch via log_shift / art_shift ---
(function () {
  // Synthetic v5 story exercising EXT:2 (log_shift) and EXT:3 (art_shift).
  // Memory layout: header at 0, globals at 64 (word addr 32 stored at 0x0C), code at 128.
  // Globals: g0=0x10 (var 16), g1=0x11 (var 17), g2=0x12 (var 18), g3=0x13 (var 19).
  var m = new Uint8Array(512);
  m[0] = 5;                           // version 5
  m[6] = 0; m[7] = 128;              // initial PC = 128
  m[10] = 0; m[11] = 200;            // object table (unused but must be in range)
  m[12] = 0; m[13] = 64;             // globals base = 64
  m[14] = 1; m[15] = 0;              // static base = 256

  // Code at 128:
  // EXT:2 log_shift(8, 4) -> g0  (8 << 4 = 128 = 0x80)
  // Types: op0=small(01), op1=small(01), op2=omit(11), op3=omit(11) -> 0x5F
  var p = 128;
  m[p++] = 0xBE; m[p++] = 0x02; m[p++] = 0x5F; m[p++] = 8; m[p++] = 4; m[p++] = 0x10;  // store -> g0

  // EXT:2 log_shift(8, -1) -> g1  (8 >>> 1 = 4) [op0=small(01), op1=word(00) = 0xFFFF = -1]
  // Types: op0=small(01), op1=word(00), op2=omit(11), op3=omit(11) -> 0x4F
  m[p++] = 0xBE; m[p++] = 0x02; m[p++] = 0x4F; m[p++] = 8; m[p++] = 0xFF; m[p++] = 0xFF; m[p++] = 0x11;

  // EXT:3 art_shift(-4, -1) -> g2  (-4 >> 1 = -2, sign-preserved)
  // op0 as word 0xFFFC (-4), op1 as word 0xFFFF (-1)
  // Types: op0=word(00), op1=word(00), op2=omit(11), op3=omit(11) -> 0x0F
  m[p++] = 0xBE; m[p++] = 0x03; m[p++] = 0x0F; m[p++] = 0xFF; m[p++] = 0xFC; m[p++] = 0xFF; m[p++] = 0xFF; m[p++] = 0x12;

  // EXT:3 art_shift(1, 2) -> g3  (1 << 2 = 4)
  m[p++] = 0xBE; m[p++] = 0x03; m[p++] = 0x5F; m[p++] = 1; m[p++] = 2; m[p++] = 0x13;

  // QUIT (0xBA = 186, 0OP)
  m[p++] = 0xBA;

  var g = new JSZM(m);
  g.print = function (t) {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () {}; g.read = function () { return null; };
  var err = null; try { g.run(); } catch (e) { err = String(e); }
  ok(!err, "EXT synthetic story ran without error (err=" + err + ")");
  // globals base 64; each global is 2 bytes: g0 at 64, g1 at 66, g2 at 68, g3 at 70
  var g0 = g.getu(64), g1 = g.getu(66);
  var g2s = g.get(68);  // signed for art_shift result
  var g3 = g.getu(70);
  ok(g0 === 128, "log_shift(8,4)=128 (got " + g0 + ")");
  ok(g1 === 4, "log_shift(8,-1)=4 [logical right] (got " + g1 + ")");
  ok(g2s === -2, "art_shift(-4,-1)=-2 [signed right] (got " + g2s + ")");
  ok(g3 === 4, "art_shift(1,2)=4 [left] (got " + g3 + ")");

  // Also verify czech.z5 still reaches its banner (EXT dispatch doesn't break normal ops).
  function rd(p){var f=new File(p);f.open("rb");var b=f.readBin(1,f.length);f.close();return new Uint8Array(b);}
  var cg = new JSZM(rd(js.exec_dir + "stories/czech.z5"));
  var cout = ""; cg.print = function (t) { cout += t; }; cg.highlight = function () {}; cg.updateStatusLine = function () {};
  cg.restarted = function () { this.seed = 1; }; cg.read = function () { return null; };
  try { cg.run(); } catch (e) {}
  ok(cout.indexOf("CZECH") >= 0, "czech.z5 still reaches its banner after EXT dispatch added");
})();

// --- Plan2 T3: call family unblocks czech further ---
(function () {
  function rd(p){var f=new File(p);f.open("rb");var b=f.readBin(1,f.length);f.close();return new Uint8Array(b);}
  var g = new JSZM(rd(js.exec_dir + "stories/czech.z5"));
  var out = ""; g.print = function (t) { out += t; }; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  try { g.run(); } catch (e) {}
  // czech's banner already contains "[brackets]", so a bare "[" proves nothing.
  // The call family is what lets czech get PAST the banner into its actual test
  // sections (each routine call exercises call_*). Gate on a deep marker ([70],
  // the Arithmetic section) only reachable once the call family works.
  ok(out.indexOf("Arithmetic ops [70]") >= 0, "czech.z5 reaches Arithmetic [70] test section (call family working)");
})();

// --- Plan2 T4: Quetzal Stks round-trips discard/args ---
(function () {
  function mkU8(arr) { var u = new Uint8Array(arr.length); for (var i = 0; i < arr.length; i++) u[i] = arr[i]; return u; }
  function loc(arr) { var l = new Int16Array(arr.length); for (var i = 0; i < arr.length; i++) l[i] = arr[i]; return l; }
  var orig = mkU8([0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80,
                   0, 0, 0x38, 0x31, 0x30, 0x37, 0x32, 0x36, 0, 0, 0, 0, 1, 2, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
  var mem = new Uint8Array(orig);
  mem[70] = 0x10; mem[60] = 0x05;
  var ds = [11, 22];
  var cs = [
    { ds: [33], pc: 70, local: loc([1]), discard: true,  args: 2 },
    { ds: [44], pc: 60, local: loc([7]), discard: false, args: 1 }
  ];
  var blob = quetzal.write({ mem: mem, orig: orig, dynsize: 80, pc: 0x99, cs: cs, ds: ds });
  var r = quetzal.read(blob, orig, 80);
  ok(r !== null, "Stks discard/args: read succeeded");
  ok(r.cs.length === 2, "Stks discard/args: two frames (was " + r.cs.length + ")");
  ok(r.cs[0].discard === true && r.cs[1].discard === false,
    "discard flags round-trip (got " + r.cs[0].discard + "/" + r.cs[1].discard + ")");
  ok(r.cs[0].args === 2 && r.cs[1].args === 1,
    "args round-trip (got " + r.cs[0].args + "/" + r.cs[1].args + ")");
})();

// --- Plan2 T5: table/text opcodes (scan_table, copy_table, print_table, encode_text) ---
(function () {
  // Synthetic v5 story exercising the table opcodes via real bytecode.
  var m = new Uint8Array(2048);
  m[0] = 5; m[6] = 0; m[7] = 128; m[10] = 0; m[11] = 200; m[12] = 0; m[13] = 64; m[14] = 2; m[15] = 0;
  // table of 3 words at 300: 0x0011, 0x0022, 0x0033
  m[300] = 0; m[301] = 0x11; m[302] = 0; m[303] = 0x22; m[304] = 0; m[305] = 0x33;
  m[320] = 65; m[321] = 66; m[322] = 67; m[323] = 68;   // 'AB','CD' for a 2x2 print_table
  var p = 128;
  // scan_table 0x22 in table@300 count 3 form 0x82 -> g0; branch-on-found (offset 2 = continue)
  m[p++] = 0xF7; m[p++] = 0x05; m[p++] = 0; m[p++] = 0x22; m[p++] = 1; m[p++] = 44; m[p++] = 3; m[p++] = 0x82; m[p++] = 0x10; m[p++] = 0xC2;
  // copy_table 300 -> 310 size 6 (forward, non-overlapping)
  m[p++] = 0xFD; m[p++] = 0x07; m[p++] = 1; m[p++] = 44; m[p++] = 1; m[p++] = 54; m[p++] = 6;
  // print_table 320 width 2 height 2  -> "AB\nCD"
  m[p++] = 0xFE; m[p++] = 0x17; m[p++] = 1; m[p++] = 64; m[p++] = 2; m[p++] = 2;
  m[p++] = 0xBA;   // quit
  var g = new JSZM(m); var out = "";
  g.print = function (t) { out += t; }; g.read = function () { return null; }; g.restarted = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  var err = null; try { g.run(); } catch (e) { err = String(e); }
  ok(!err, "table-opcode story ran clean (err=" + err + ")");
  ok(g.getu(64) === 302, "scan_table: found 0x22 at entry addr 302 (got " + g.getu(64) + ")");
  ok(g.getu(310) === 0x11 && g.getu(312) === 0x22 && g.getu(314) === 0x33, "copy_table: 3 words copied forward");
  ok(out === "AB\nCD", "print_table: 2x2 rectangle = 'AB\\nCD' (got " + JSON.stringify(out) + ")");

  // scan_table miss: store 0, no branch.
  var m2 = new Uint8Array(2048);
  m2[0] = 5; m2[6] = 0; m2[7] = 128; m2[10] = 0; m2[11] = 200; m2[12] = 0; m2[13] = 64; m2[14] = 2; m2[15] = 0;
  m2[300] = 0; m2[301] = 0x11;
  var q = 128;
  // scan for 0x99 (absent) in 1-entry table; branch-on-true offset 2 must NOT be taken -> fall through to a marker store
  m2[q++] = 0xF7; m2[q++] = 0x05; m2[q++] = 0; m2[q++] = 0x99; m2[q++] = 1; m2[q++] = 44; m2[q++] = 1; m2[q++] = 0x82; m2[q++] = 0x10; m2[q++] = 0xC2;
  m2[q++] = 0xBA;
  var g2 = new JSZM(m2); g2.print = function () {}; g2.read = function () { return null; }; g2.restarted = function () {};
  try { g2.run(); } catch (e) {}
  ok(g2.getu(64) === 0, "scan_table miss: stores 0 (got " + g2.getu(64) + ")");

  // copy_table zero-fill (second==0), via bytecode: copy_table 330, 0, 3
  var m4 = new Uint8Array(2048);
  m4[0] = 5; m4[6] = 0; m4[7] = 128; m4[10] = 0; m4[11] = 200; m4[12] = 0; m4[13] = 64; m4[14] = 2; m4[15] = 0;
  m4[330] = 1; m4[331] = 2; m4[332] = 3;
  var r = 128;
  // copy_table first=330 second=0 size=3 : types large,small,small,omit = 00 01 01 11 = 0x17
  m4[r++] = 0xFD; m4[r++] = 0x17; m4[r++] = 1; m4[r++] = 74; m4[r++] = 0; m4[r++] = 3;
  m4[r++] = 0xBA;
  var g4 = new JSZM(m4); g4.print = function () {}; g4.read = function () { return null; }; g4.restarted = function () {};
  try { g4.run(); } catch (e) {}
  ok(g4.mem[330] === 0 && g4.mem[331] === 0 && g4.mem[332] === 0, "copy_table zero-fill (second==0)");

  // encode_text round-trips through getText (dictionary-word form).
  var ge = new JSZM(m); ge.mem = new Uint8Array(m);
  ge.mem[500] = 110; ge.mem[501] = 111; ge.mem[502] = 114; ge.mem[503] = 116; ge.mem[504] = 104;  // 'north'
  ge.encodeText(500, 5, 0, 410);
  ok(ge.getText(410) === "north", "encode_text: 'north' encodes to a 9-Z-char word that decodes back (got '" + ge.getText(410) + "')");
})();

// --- Plan2 T5: v5 read layout + tokenise (text byte1=count, chars from byte2, store term char) ---
(function () {
  var m = new Uint8Array(2048);
  m[0] = 5; m[6] = 0; m[7] = 128; m[8] = 1; m[9] = 0x90;   // dict at 400
  m[10] = 0; m[11] = 200; m[12] = 0; m[13] = 64; m[14] = 2; m[15] = 0;
  var d = 400; m[d++] = 0; m[d++] = 6; m[d++] = 0; m[d++] = 2;  // 0 seps, entry len 6, 2 entries
  var gt = new JSZM(m); gt.mem = m;
  m[500] = 103; m[501] = 111; gt.encodeText(500, 2, 0, 404);                                   // 'go'
  m[510] = 110; m[511] = 111; m[512] = 114; m[513] = 116; m[514] = 104; gt.encodeText(510, 5, 0, 410); // 'north'
  m[600] = 20; m[650] = 5;        // text buffer (max 20), parse buffer (max 5 tokens)
  var p = 128;
  // read text=600 parse=650 -> store term char in g0
  m[p++] = 0xE4; m[p++] = 0x0F; m[p++] = 2; m[p++] = 88; m[p++] = 2; m[p++] = 138; m[p++] = 0x10;
  m[p++] = 0xBA;
  var g = new JSZM(m); g.print = function () {}; g.restarted = function () {};
  g.read = function () { return "go north"; };
  var err = null; try { g.run(); } catch (e) { err = String(e); }
  ok(!err, "v5 read story ran clean (err=" + err + ")");
  ok(g.mem[601] === 8, "v5 read: text byte1 = char count 8 (got " + g.mem[601] + ")");
  var chars = ""; for (var i = 0; i < g.mem[601]; i++) chars += String.fromCharCode(g.mem[602 + i]);
  ok(chars === "go north", "v5 read: chars stored from byte 2 (got '" + chars + "')");
  ok(g.getu(64) === 13, "v5 read: stores terminating char 13 (got " + g.getu(64) + ")");
  ok(g.mem[651] === 2, "v5 read: parse buffer has 2 tokens (got " + g.mem[651] + ")");
  ok(g.getu(652) === 404 && g.mem[654] === 2 && g.mem[655] === 2, "v5 read: token0 = 'go' @404 len2 pos2");
  ok(g.getu(656) === 410 && g.mem[658] === 5 && g.mem[659] === 5, "v5 read: token1 = 'north' @410 len5 pos5");
})();

// --- Plan2 T5: read_char + screen no-ops + EXT font/unicode stubs ---
(function () {
  var m = new Uint8Array(1024);
  m[0] = 5; m[6] = 0; m[7] = 128; m[10] = 0; m[11] = 200; m[12] = 0; m[13] = 64; m[14] = 2; m[15] = 0;
  var p = 128;
  // read_char 1 -> g0
  m[p++] = 0xF6; m[p++] = 0x7F; m[p++] = 1; m[p++] = 0x10;
  // screen no-ops: split_window(1), set_window(0), set_cursor(2,1), set_text_style(0),
  // buffer_mode(1), output_stream(1), erase_window(-1) -- each must consume + not crash.
  m[p++] = 0xEA; m[p++] = 0x7F; m[p++] = 1;                         // split_window 1 (VAR 234)
  m[p++] = 0xEB; m[p++] = 0x7F; m[p++] = 0;                         // set_window 0  (VAR 235)
  m[p++] = 0xEF; m[p++] = 0x5F; m[p++] = 2; m[p++] = 1;             // set_cursor 2 1 (VAR 239)
  m[p++] = 0xF1; m[p++] = 0x7F; m[p++] = 0;                         // set_text_style 0 (VAR 241)
  m[p++] = 0xF2; m[p++] = 0x7F; m[p++] = 1;                         // buffer_mode 1 (VAR 242)
  m[p++] = 0xF3; m[p++] = 0x7F; m[p++] = 1;                         // output_stream 1 (VAR 243)
  // EXT stubs
  m[p++] = 0xBE; m[p++] = 4; m[p++] = 0x7F; m[p++] = 4; m[p++] = 0x11;        // set_font 4 -> g1
  m[p++] = 0xBE; m[p++] = 11; m[p++] = 0x7F; m[p++] = 0x41;                   // print_unicode 'A'
  m[p++] = 0xBE; m[p++] = 12; m[p++] = 0x7F; m[p++] = 0x41; m[p++] = 0x12;    // check_unicode 0x41 -> g2
  m[p++] = 0xBE; m[p++] = 12; m[p++] = 0x3F; m[p++] = 0x01; m[p++] = 0x50; m[p++] = 0x13; // check_unicode 0x150 -> g3
  m[p++] = 0xBA;
  var g = new JSZM(m); var out = "";
  g.print = function (t) { out += t; }; g.restarted = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.read = function () { return "x"; };
  var err = null; try { g.run(); } catch (e) { err = String(e); }
  ok(!err, "read_char/screen/EXT story ran clean (err=" + err + ")");
  ok(g.getu(64) === 120, "read_char: stores ZSCII of keypress 'x'=120 (got " + g.getu(64) + ")");
  ok(g.getu(66) === 1, "set_font(4): reports previous font 1 (got " + g.getu(66) + ")");
  ok(out === "A", "print_unicode: emits the codepoint (got " + JSON.stringify(out) + ")");
  ok(g.getu(68) === 3, "check_unicode(0x41): 3 for Latin-1 (got " + g.getu(68) + ")");
  ok(g.getu(70) === 0, "check_unicode(0x150): 0 above Latin-1 (got " + g.getu(70) + ")");
})();

// --- Plan3 T1: engine screen opcodes call host hooks ---
(function () {
  // A v5 routine: set_cursor 3,5 ; erase_window -1 ; erase_line 1 ; buffer_mode 0 ;
  // get_cursor (table) ; quit. We assert each host hook fired with the right args,
  // and get_cursor wrote what the hook returned.
  // Build the bytecode by hand: header (v5) + a main routine at 0x40.
  var m = new Uint8Array(0x200);
  m[0] = 5;                          // version 5
  m[6] = 0x00; m[7] = 0x40;          // initial PC = 0x40 (v1-5: byte addr of the FIRST INSTRUCTION,
                                     //   NOT a routine header -- there is no locals byte here)
  m[14] = 0x00; m[15] = 0x40;        // static mem base (dynamic = 0..0x3f)
  var p = 0x40;
  function emitVAR(op, types, ops) {  // 0xE0|op? no -- VAR form is 0xC0|op for 2OP-as-VAR; use 0xE0 for true VAR
    m[p++] = 0xE0 | op;               // VAR opcode (op = low 5/6 bits); 0xE0..0xFF
    m[p++] = types;                   // one type byte (2 bits per operand; 3=omitted)
    for (var i = 0; i < ops.length; i++) { m[p++] = (ops[i] >> 8) & 0xff; m[p++] = ops[i] & 0xff; }
  }
  // set_cursor (VAR:239 -> op = 239-224 = 15): 0xEF ; two large-constant operands 3,5
  emitVAR(15, 0x0f, [3, 5]);         // types: 00 00 11 11 -> op1=large,op2=large,om,om = 0x0F
  // erase_window (VAR:237 -> 13): operand -1 (0xFFFF)
  emitVAR(13, 0x3f, [0xFFFF]);       // types: 00 11 11 11 = 0x3F (one large const)
  // erase_line (VAR:238 -> 14): operand 1
  emitVAR(14, 0x3f, [1]);
  // buffer_mode (VAR:242 -> 18): operand 0
  emitVAR(18, 0x3f, [0]);
  // get_cursor (VAR:240 -> 16): table at 0x10
  emitVAR(16, 0x3f, [0x10]);
  // quit (0OP:186)
  m[p++] = 0xBA;

  var g = new JSZM(m);
  var calls = [];
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.setCursor   = function (r, c) { calls.push(["setCursor", r, c]); };
  g.eraseWindow = function (w)    { calls.push(["eraseWindow", w]); };
  g.eraseLine   = function (v)    { calls.push(["eraseLine", v]); };
  g.bufferMode  = function (f)    { calls.push(["bufferMode", f]); };
  g.getCursor   = function ()     { calls.push(["getCursor"]); return { row: 7, col: 9 }; };
  try { g.run(); } catch (e) {}
  function find(name) { for (var i = 0; i < calls.length; i++) if (calls[i][0] === name) return calls[i]; return null; }
  ok(find("setCursor") && find("setCursor")[1] === 3 && find("setCursor")[2] === 5, "set_cursor hook got row=3,col=5");
  ok(find("eraseWindow") && find("eraseWindow")[1] === -1, "erase_window hook got -1");
  ok(find("eraseLine") && find("eraseLine")[1] === 1, "erase_line hook got 1");
  ok(find("bufferMode") && find("bufferMode")[1] === 0, "buffer_mode hook got 0");
  ok(find("getCursor"), "get_cursor hook fired");
  ok(g.getu(0x10) === 7 && g.getu(0x12) === 9, "get_cursor wrote hook's row,col into the table");
})();

// --- Plan3 T2: output_stream 3 redirects print into a memory table ---
(function () {
  var m = new Uint8Array(0x300);
  m[0] = 5; m[6] = 0x00; m[7] = 0x40; m[14] = 0x01; m[15] = 0x00;  // v5, PC=0x40 (first instr), static base 0x100
  var p = 0x40;
  function emitVAR(op, types, ops) {
    m[p++] = 0xE0 | op; m[p++] = types;
    for (var i = 0; i < ops.length; i++) { m[p++] = (ops[i] >> 8) & 0xff; m[p++] = ops[i] & 0xff; }
  }
  // output_stream 3, table @ 0x80  (VAR:243 -> op = 243-224 = 19): 0xF3
  emitVAR(19, 0x0f, [3, 0x80]);
  // print "Hi" -- use print_char twice (VAR:229 -> op 5): 'H'(72), 'i'(105)
  emitVAR(5, 0x3f, [72]);
  emitVAR(5, 0x3f, [105]);
  // output_stream -3 (disable): operand 0xFFFD (-3)
  emitVAR(19, 0x3f, [0xFFFD]);
  // print_char '!' (33) -- now goes to screen, not the table
  emitVAR(5, 0x3f, [33]);
  m[p++] = 0xBA;  // quit

  var out = "";
  var g = new JSZM(m);
  g.print = function (t) { out += t; }; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  try { g.run(); } catch (e) {}
  ok(g.getu(0x80) === 2, "stream-3 table length word = 2 (was " + g.getu(0x80) + ")");
  ok(g.mem[0x82] === 72 && g.mem[0x83] === 105, "stream-3 table holds 'Hi'");
  ok(out.indexOf("Hi") < 0, "screen did NOT get the redirected text");
  ok(out.indexOf("!") >= 0, "screen DID get the post-disable '!'");
})();

// --- #112: output_stream -1 deselects stream 1 (the screen) in v3+ ---
// Arthur (v6) echoes the just-typed command to the transcript with the screen stream
// turned OFF (output_stream -1) so it is NOT shown again on screen; a correct interpreter
// must suppress those characters. jszm formerly treated stream 1 as "always on".
(function () {
  var m = new Uint8Array(0x100);
  m[0] = 5; m[6] = 0x00; m[7] = 0x40; m[14] = 0x00; m[15] = 0x80;   // v5, PC=0x40, static base 0x80
  var p = 0x40;
  function emitVAR(op, types, ops) {
    m[p++] = 0xE0 | op; m[p++] = types;
    for (var i = 0; i < ops.length; i++) { m[p++] = (ops[i] >> 8) & 0xff; m[p++] = ops[i] & 0xff; }
  }
  emitVAR(5, 0x3f, [62]);          // print_char '>' (62) -- visible (prompt)
  emitVAR(19, 0x3f, [0xFFFF]);     // output_stream -1 (disable screen)
  emitVAR(5, 0x3f, [88]);          // print_char 'X' (88) -- screen OFF -> must be suppressed
  emitVAR(19, 0x3f, [1]);          // output_stream 1 (re-enable screen)
  emitVAR(5, 0x3f, [89]);          // print_char 'Y' (89) -- visible again
  m[p++] = 0xBA;                   // quit

  var out = "";
  var g = new JSZM(m);
  g.print = function (t) { out += t; }; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  try { g.run(); } catch (e) {}
  ok(out.indexOf("X") < 0, "output_stream -1: 'X' suppressed while screen deselected (got " + JSON.stringify(out) + ")");
  ok(out.indexOf(">") >= 0 && out.indexOf("Y") >= 0, "screen output before/after the off-window still shown");
})();

// --- Text/Unicode T1: extended ZSCII maps to Unicode via the default table ---
(function () {
  // A v5 story whose only string (printed at startup) is the single 10-bit ZSCII
  // value 170 -- the default Unicode table maps ZSCII 170 -> U+00E9 ('e-acute').
  // Z-chars: A2-shift(5), escape(6), then two 5-bit halves of 170 = 0b0101010 ...
  // 170 = 0x0AA = 10 bits: 00101 01010 -> top5=5, low5=10.
  var m = new Uint8Array(0x100);
  m[0] = 5; m[6] = 0x00; m[7] = 0x40; m[14] = 0x00; m[15] = 0x40;  // v5, PC=0x40
  // print (0OP:178 = 0xB2) of an inline string, then quit (0xBA).
  m[0x40] = 0xB2;
  // string at 0x41: one word holding Z-chars [5, 6, 5] then a word [10, 5(pad), 5(pad)] w/ end bit.
  // word1 = (5<<10)|(6<<5)|5 = 0x14C5 ; word2 = (10<<10)|(5<<5)|5 |0x8000 = 0xA8A5
  m[0x41] = 0x14; m[0x42] = 0xC5;
  m[0x43] = 0xA8; m[0x44] = 0xA5;
  m[0x45] = 0xBA;   // quit
  var out = "";
  var g = new JSZM(m);
  g.print = function (t) { out += t; }; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  try { g.run(); } catch (e) {}
  ok(out.charCodeAt(0) === 0x00E9, "ZSCII 170 -> U+00E9 (was " + out.charCodeAt(0).toString(16) + ")");
})();

// --- Text/Unicode T2: custom alphabet table (header 0x34) ---
(function () {
  // v5 story with a custom alphabet whose A0[0] (Z-char 6) is 'Q' instead of 'a'.
  var m = new Uint8Array(0x200);
  m[0] = 5; m[6] = 0x00; m[7] = 0x40; m[14] = 0x00; m[15] = 0x40;
  m[0x34] = 0x00; m[0x35] = 0x60;          // header 0x34 -> alphabet table at 0x60
  // 78-byte alphabet table: A0 = 'Q' then 'bcdef...' ; A1/A2 = the defaults (only A0[0] matters here)
  var alpha = "Qbcdefghijklmnopqrstuvwxyz" +              // A0 (26)
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ" +              // A1 (26)
              "\0\0" + "0123456789.,!?_#'\"/\\-:()";       // A2 (26): [0],[1] placeholders
  for (var i = 0; i < 78; i++) m[0x60 + i] = alpha.charCodeAt(i);
  // print inline string of Z-char 6 (A0[0]) -> should be 'Q'. word = (6<<10)|(5<<5)|5 |0x8000.
  m[0x40] = 0xB2;                          // print
  var word = (6 << 10) | (5 << 5) | 5 | 0x8000;
  m[0x41] = (word >> 8) & 0xff; m[0x42] = word & 0xff;
  m[0x43] = 0xBA;                          // quit
  var out = "";
  var g = new JSZM(m);
  g.print = function (t) { out += t; }; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  try { g.run(); } catch (e) {}
  ok(out.charAt(0) === "Q", "custom alphabet A0[0] decodes to 'Q' (was '" + out.charAt(0) + "')");
})();

// --- Text/Unicode T3: print_unicode emits the code point; check_unicode uses the hook ---
(function () {
  var m = new Uint8Array(0x100);
  m[0] = 5; m[6] = 0x00; m[7] = 0x40; m[14] = 0x00; m[15] = 0x40;
  var p = 0x40;
  // print_unicode (EXT 11) of U+00E9: 0xBE 0x0B <types> <operand>
  m[p++] = 0xBE; m[p++] = 0x0B; m[p++] = 0x3F; m[p++] = 0x00; m[p++] = 0xE9;   // one large const 0x00E9
  // check_unicode (EXT 12) of U+00E9 -> store to global var 16 (0x10) at PC start of routine? store byte follows.
  m[p++] = 0xBE; m[p++] = 0x0C; m[p++] = 0x3F; m[p++] = 0x00; m[p++] = 0xE9; m[p++] = 0x10;  // store result in var 0x10
  m[p++] = 0xBA;   // quit
  var out = "";
  var g = new JSZM(m);
  g.print = function (t) { out += t; }; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  var asked = -1; g.checkUnicode = function (cp) { asked = cp; return 3; };
  try { g.run(); } catch (e) {}
  ok(out.charCodeAt(0) === 0x00E9, "print_unicode emitted U+00E9 (was " + out.charCodeAt(0).toString(16) + ")");
  ok(asked === 0x00E9, "check_unicode invoked the host hook with U+00E9 (was " + asked.toString(16) + ")");
})();

// --- Colour T1: set_colour / set_text_style call the host hooks ---
(function () {
  // v5 routine: set_colour 3 6 ; set_text_style 6 ; quit. Assert the hooks fire
  // with the raw operands (door interprets them; engine just forwards).
  var m = new Uint8Array(0x100);
  m[0] = 5; m[6] = 0x00; m[7] = 0x40; m[14] = 0x00; m[15] = 0x40;   // v5, PC=0x40
  var p = 0x40;
  // set_colour (2OP:27) long form, small/small constants: opcode 0x00|27=0x1B, ops 3,6
  m[p++] = 0x1B; m[p++] = 3; m[p++] = 6;
  // set_text_style (VAR:241 -> op 17): 0xF1, one small-const operand (types 0x7F), value 6
  m[p++] = 0xF1; m[p++] = 0x7F; m[p++] = 6;
  m[p++] = 0xBA;   // quit
  var g = new JSZM(m), calls = [];
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  g.setColour    = function (fg, bg) { calls.push(["setColour", fg, bg]); };
  g.setTextStyle = function (s)      { calls.push(["setTextStyle", s]); };
  try { g.run(); } catch (e) {}
  function find(n) { for (var i = 0; i < calls.length; i++) if (calls[i][0] === n) return calls[i]; return null; }
  ok(find("setColour") && find("setColour")[1] === 3 && find("setColour")[2] === 6, "set_colour hook got fg=3,bg=6");
  ok(find("setTextStyle") && find("setTextStyle")[1] === 6, "set_text_style hook got bitmask 6");
})();


// --- get_prop sign-extension (Border Zone camera "rewound" bug): a 2-byte property
//     holding 0xFFFF must read back as signed -1 (not 65535), so a subsequent je/jl/jg
//     against a sign-extended constant on the stack compares correctly. czech doesn't
//     cover this (no >=0x8000 property compared signed via the stack). Story: obj1 prop5
//     = 0xFFFF; get_prop obj1 prop5 -> sp; je sp #-1 ? g16=1 : g16=0. Assert g16==1.
(function () {
  var m = new Uint8Array(0x200);
  m[0] = 5;                              // v5
  m[6] = 0x00; m[7] = 0x40;             // PC = 0x40
  m[10] = 0x00; m[11] = 0x60;           // object table @ 0x60
  m[12] = 0x01; m[13] = 0x40;           // globals @ 0x140 (g16 @ 0x140)
  m[14] = 0x01; m[15] = 0x60;           // static base @ 0x160
  // object 1 entry: this.objects = (getu(10)-2)+114 = 0x5E+114 = 0xD0; objAddr(1)=0xDE.
  var oe = 0xDE;
  m[oe + 12] = 0x00; m[oe + 13] = 0xF0; // prop-table addr (objAddr+attrBytes6+3*width2=0xEA) @ 0xF0
  // property table @ 0xF0: no short name, prop #5 (2 bytes) = 0xFFFF, terminator
  m[0xF0] = 0x00; m[0xF1] = 0x45; m[0xF2] = 0xFF; m[0xF3] = 0xFF; m[0xF4] = 0x00;
  // code @ 0x40
  var p = 0x40;
  m[p++] = 0x11; m[p++] = 0x01; m[p++] = 0x05; m[p++] = 0x00;                         // get_prop obj1 prop5 -> sp
  m[p++] = 0xC1; m[p++] = 0x8F; m[p++] = 0x00; m[p++] = 0xFF; m[p++] = 0xFF; m[p++] = 0xC6; // je sp #-1 ?(true)->0x4E
  m[p++] = 0x0D; m[p++] = 0x10; m[p++] = 0x00; m[p++] = 0xBA;                         // 0x4A: store g16=0; quit
  m[p++] = 0x0D; m[p++] = 0x10; m[p++] = 0x01; m[p++] = 0xBA;                         // 0x4E: store g16=1; quit
  var g = new JSZM(m);
  g.print = function () {}; g.highlight = function () {}; g.updateStatusLine = function () {};
  g.restarted = function () { this.seed = 1; }; g.read = function () { return null; };
  try { g.run(); } catch (e) { ok(false, "get_prop signext threw: " + e); }
  ok(g.getu(0x140) === 1, "get_prop 2-byte 0xFFFF reads as signed -1 (je sp,#-1 -> equal); g16=" + g.getu(0x140));
})();

if (fails) throw new Error(fails + " v45 test(s) failed");
print("V45 OK");
