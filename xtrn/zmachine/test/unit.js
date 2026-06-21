// jsexec unit tests for the ES5 jszm helpers.
load(js.exec_dir + "../jszm.js");

var fails = 0;
function ok(c, msg) { if (!c) { print("FAIL: " + msg); fails++; } }

// Polyfills
ok(Math.imul(3, 4) === 12, "imul 3*4");
ok(Math.imul(0xffffffff, 5) === -5, "imul overflow");
ok(Math.trunc(3.7) === 3 && Math.trunc(-3.7) === -3, "trunc");

// bytesToString
ok(jszm_bytesToString(new Uint8Array([72, 73, 74]), 0, 3) === "HIJ", "bytesToString");

// Vocabulary collision safety
var m = {};
jszm_vocabSet(m, "constructor", 42);
ok(jszm_vocabGet(m, "constructor") === 42, "vocab set/get");
ok(jszm_vocabGet(m, "toString") === 0, "vocab miss is 0 (no proto leak)");

// Endianness via a JSZM instance (header byte0=3 so the ctor accepts it)
var hdr = new Uint8Array(64); hdr[0] = 3;
var g = new JSZM(hdr);
g.mem = new Uint8Array([0x12, 0x34, 0xFF, 0xFE, 0, 0]);
g.byteSwapped = false;
ok(g.getu(0) === 0x1234, "getu big-endian");
ok(g.get(0) === 0x1234, "get big-endian");
ok(g.get(2) === -2, "get signed big-endian");
g.byteSwapped = true;
ok(g.getu(0) === 0x3412, "getu little-endian");
g.byteSwapped = false; g.put(4, -2); ok(g.getu(4) === 0xFFFE, "put/getu round-trip BE");
g.byteSwapped = true; g.put(4, 0x1234); ok(g.mem[4] === 0x34 && g.mem[5] === 0x12, "put little-endian order");

if (fails) throw new Error(fails + " unit test(s) failed");
print("UNIT OK");
