var db = new SQLite(":memory:");
db.exec("CREATE TABLE t (a INTEGER, b VARCHAR(50), c REAL, d BLOB, e DATETIME)");
db.run("INSERT INTO t VALUES (?, ?, ?, ?, ?)", [1, "x", 1.5, null, "2026-05-12"]);

/* Declared types should come back verbatim from the CREATE TABLE. */
var stmt = db.prepare("SELECT a, b, c, d, e FROM t");
var types = stmt.declared_types;
if (types.length !== 5) throw new Error("expected 5 entries");
if (types[0] !== "INTEGER") throw new Error("a: " + types[0]);
if (types[1] !== "VARCHAR(50)") throw new Error("b: " + types[1]);
if (types[2] !== "REAL") throw new Error("c: " + types[2]);
if (types[3] !== "BLOB") throw new Error("d: " + types[3]);
if (types[4] !== "DATETIME") throw new Error("e: " + types[4]);
stmt.close();

/* Computed/expression columns have no declared type - null. */
var s2 = db.prepare("SELECT 1 AS one, a + 1 FROM t");
var t2 = s2.declared_types;
if (t2[0] !== null) throw new Error("literal should have null decltype");
if (t2[1] !== null) throw new Error("expression should have null decltype");
s2.close();

/* Aggregate also null. */
var s3 = db.prepare("SELECT COUNT(*) FROM t");
if (s3.declared_types[0] !== null) throw new Error("aggregate should have null decltype");
s3.close();

db.close();
