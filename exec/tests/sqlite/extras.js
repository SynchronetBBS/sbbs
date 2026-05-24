var db = new SQLite(":memory:");
db.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
db.run("INSERT INTO users (name) VALUES (?)", ["alice"]);

/* has_table */
if (!db.has_table("users")) throw new Error("has_table(users) should be true");
if (db.has_table("nope")) throw new Error("has_table(nope) should be false");

/* has_table injection safety: a malicious table-name shouldn't drop anything. */
var dropped = false;
try {
	db.has_table("users; DROP TABLE users; --");
} catch (e) {
	/* fine if it throws */
}
if (!db.has_table("users")) throw new Error("users table was dropped!");

/* expanded_sql shows bound values. */
var stmt = db.prepare("SELECT * FROM users WHERE id = ? AND name = ?");
stmt.bind([42, "alice"]);
var exp = stmt.expanded_sql;
if (exp.indexOf("42") < 0) throw new Error("expanded_sql missing 42: " + exp);
if (exp.indexOf("'alice'") < 0) throw new Error("expanded_sql missing 'alice': " + exp);
stmt.close();

/* backup: write to a temp file, open the backup, verify data round-trips. */
var path = system.temp_path + "test_sqlite_backup_" + Date.now() + ".db";
db.backup(path);

var db2 = new SQLite(path);
var rows = db2.query("SELECT name FROM users");
if (rows.length !== 1 || rows[0].name !== "alice") throw new Error("backup data missing");
db2.close();
file_remove(path);

/* SQLite.escape_like: escapes %, _, and the escape char so a user's literal
 * text can be inserted into a LIKE pattern. */
if (typeof SQLite.escape_like !== "function") throw new Error("SQLite.escape_like missing");
if (SQLite.escape_like("100% Complete") !== "100\\% Complete")
	throw new Error("escape_like: got " + SQLite.escape_like("100% Complete"));
if (SQLite.escape_like("a_b") !== "a\\_b") throw new Error("escape_like underscore");
if (SQLite.escape_like("path\\file") !== "path\\\\file") throw new Error("escape_like backslash");
/* Custom escape char. */
if (SQLite.escape_like("a%b", "!") !== "a!%b") throw new Error("escape_like custom escape char");

/* End-to-end: literal '%' as a LIKE pattern fragment only matches itself. */
var db2 = new SQLite(":memory:");
db2.exec("CREATE TABLE log (msg TEXT)");
db2.run("INSERT INTO log VALUES (?)", ["100% Complete"]);
db2.run("INSERT INTO log VALUES (?)", ["100 percent done"]);
var hits = db2.query("SELECT msg FROM log WHERE msg LIKE ? ESCAPE '\\'",
    [SQLite.escape_like("100% C") + "%"]);
if (hits.length !== 1) throw new Error("escape_like LIKE pattern: expected 1 hit, got " + hits.length);
if (hits[0].msg !== "100% Complete") throw new Error("escape_like matched wrong row");
db2.close();

db.close();
