var db = new SQLite(":memory:");
db.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL, score REAL DEFAULT 0)");
db.exec("CREATE TABLE scores (uid INTEGER, val INTEGER, PRIMARY KEY (uid, val))");
db.exec("CREATE VIEW v_top AS SELECT name FROM users ORDER BY score DESC");

/* tables() lists user tables only - no views, no sqlite_* internals. */
var t = db.tables();
if (t.length !== 2) throw new Error("expected 2 tables, got " + t.length);
if (t[0] !== "scores") throw new Error("alphabetical order, scores first");
if (t[1] !== "users") throw new Error("alphabetical order, users second");

/* columns() returns descriptors. */
var cols = db.columns("users");
if (cols.length !== 3) throw new Error("users should have 3 columns");

if (cols[0].cid !== 0) throw new Error("cid 0 wrong");
if (cols[0].name !== "id") throw new Error("col 0 name");
if (cols[0].type !== "INTEGER") throw new Error("col 0 type");
if (cols[0].pk !== 1) throw new Error("col 0 should be pk=1");

if (cols[1].name !== "name") throw new Error("col 1 name");
if (cols[1].notnull !== true) throw new Error("col 1 should be NOT NULL");
if (cols[1].pk !== 0) throw new Error("col 1 should not be pk");

if (cols[2].name !== "score") throw new Error("col 2 name");
if (cols[2].dflt_value !== "0") throw new Error("col 2 default: " + cols[2].dflt_value);

/* Compound primary key: pk is the 1-based index. */
var sc = db.columns("scores");
if (sc[0].pk !== 1) throw new Error("scores.uid should be pk=1");
if (sc[1].pk !== 2) throw new Error("scores.val should be pk=2");

/* Nonexistent table: empty array, not an error. */
var none = db.columns("does_not_exist");
if (none.length !== 0) throw new Error("nonexistent table should give empty array");

/* Injection safety: the table-name argument is bound, not interpolated. */
var threw_or_empty = false;
try {
	var r = db.columns("users; DROP TABLE users; --");
	if (r.length === 0) threw_or_empty = true;
} catch (e) {
	threw_or_empty = true;
}
if (!threw_or_empty) throw new Error("malicious table-name arg should not work");
/* And verify users still exists. */
if (db.columns("users").length !== 3) throw new Error("users table was dropped!");

db.close();
