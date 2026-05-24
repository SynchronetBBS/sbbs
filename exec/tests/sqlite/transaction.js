var db = new SQLite(":memory:");
db.exec("CREATE TABLE t (k TEXT)");

db.transaction(function () {
	db.run("INSERT INTO t (k) VALUES (?)", ["commit-row"]);
});
var rows = db.query("SELECT k FROM t");
if (rows.length !== 1) throw new Error("commit path didn't persist 1 row");

var threw = false;
try {
	db.transaction(function () {
		db.run("INSERT INTO t (k) VALUES (?)", ["rolled-back"]);
		throw new Error("boom");
	});
} catch (e) {
	threw = true;
}
if (!threw) throw new Error("transaction did not propagate the thrown error");

rows = db.query("SELECT k FROM t");
if (rows.length !== 1) throw new Error("rollback did not discard the inserted row");
db.close();
