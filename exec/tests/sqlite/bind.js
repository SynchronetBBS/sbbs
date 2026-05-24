var db = new SQLite(":memory:");
db.exec("CREATE TABLE t (a INTEGER, b TEXT)");

db.run("INSERT INTO t VALUES (?, ?)", [1, "one"]);
db.run("INSERT INTO t VALUES (:a, :b)", { ":a": 2, ":b": "two" });
db.run("INSERT INTO t VALUES (:a, :b)", { a: 3, b: "three" });

var rows = db.query("SELECT a, b FROM t ORDER BY a");
if (rows.length !== 3) throw new Error("expected 3 rows, got " + rows.length);
if (rows[0].a !== 1 || rows[0].b !== "one") throw new Error("row 0 mismatch");
if (rows[1].a !== 2 || rows[1].b !== "two") throw new Error("row 1 mismatch");
if (rows[2].a !== 3 || rows[2].b !== "three") throw new Error("row 2 mismatch");

var s = db.prepare("SELECT a FROM t WHERE b = :name");
s.bind({ name: "two" });
var row = s.step();
if (row === null) throw new Error("no row from named bind");
if (row.a.integer !== 2) throw new Error("named bind matched wrong row");
s.close();

db.close();
