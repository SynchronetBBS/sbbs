var db = new SQLite(":memory:");
db.exec("CREATE TABLE t (i INTEGER, r REAL, t TEXT, b BLOB, n)");

var ab = new ArrayBuffer(4);
var u8 = new Uint8Array(ab);
u8[0] = 0xDE; u8[1] = 0xAD; u8[2] = 0xBE; u8[3] = 0xEF;

var ins = db.prepare("INSERT INTO t VALUES (?, ?, ?, ?, ?)");
ins.bind([42, 3.14, "héllo", ab, null]);
if (ins.step() !== null) throw new Error("insert returned a row");
ins.close();

var sel = db.prepare("SELECT i, r, t, b, n FROM t");
var row = sel.step();
if (row === null) throw new Error("no row from SELECT");

if (row.i.type !== "integer") throw new Error("i.type != integer");
if (row.i.integer !== 42) throw new Error("i.integer != 42");
if (row.r.type !== "real") throw new Error("r.type != real");
if (Math.abs(row.r.real - 3.14) > 1e-12) throw new Error("r.real not 3.14");
if (row.t.type !== "text") throw new Error("t.type != text");
if (row.t.text !== "héllo") throw new Error("text round-trip failed");
if (row.b.type !== "blob") throw new Error("b.type != blob");
if (row.b.bytes !== 4) throw new Error("blob bytes != 4");

var blob = row.b.blob;
if (!(blob instanceof ArrayBuffer)) throw new Error("b.blob not ArrayBuffer");
var bu8 = new Uint8Array(blob);
if (bu8[0] !== 0xDE || bu8[1] !== 0xAD || bu8[2] !== 0xBE || bu8[3] !== 0xEF)
	throw new Error("blob bytes round-trip failed");

if (row.n.type !== "null") throw new Error("n.type != null");
if (row.n.value !== null) throw new Error("n.value != null");

sel.close();
db.close();
