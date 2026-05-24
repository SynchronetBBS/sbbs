var db = new SQLite(":memory:");
if (!(db instanceof SQLite)) throw new Error("not a SQLite instance");
if (db.path !== ":memory:") throw new Error("path property mismatch");
if (db.error_code !== 0) throw new Error("nonzero error_code on fresh db");

db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
if (db.error_code !== 0) throw new Error("CREATE TABLE error_code");

db.run("INSERT INTO t (name) VALUES (?)", ["alice"]);
if (db.last_insert_id !== 1) throw new Error("last_insert_id != 1");
if (db.changes !== 1) throw new Error("changes != 1");

var before = db.total_changes;
db.run("UPDATE t SET name = ? WHERE id = ?", ["alice2", 1]);
if (db.total_changes <= before) throw new Error("total_changes did not increase after UPDATE");

db.close();
