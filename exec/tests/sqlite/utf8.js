var db = new SQLite(":memory:");
db.exec("CREATE TABLE t (s TEXT)");

/* Write via bindUtf8: the JS string is treated as proper Unicode and
 * stored as portable UTF-8 in the DB. Read back via .utf8 to recover
 * the same Unicode string. */
var stmt = db.prepare("INSERT INTO t (s) VALUES (?)");
stmt.bindUtf8(1, "héllo Ω 🌍");
if (stmt.step() !== null) throw new Error("insert returned a row");
stmt.close();

var sel = db.prepare("SELECT s FROM t");
var row = sel.step();
if (row === null) throw new Error("no row from SELECT");
if (row.s.utf8 !== "héllo Ω 🌍") throw new Error(".utf8 round-trip failed: got " + row.s.utf8);

/* .text reads the same column as Synchronet narrow bytes - same column
 * read two different ways should differ for non-ASCII content. */
if (row.s.text === row.s.utf8) throw new Error(".text and .utf8 should differ for non-ASCII data");

/* toString defaults to the Synchronet narrow form (matches .text). */
if (("" + row.s) !== row.s.text) throw new Error("toString did not match .text");

sel.close();
db.close();
