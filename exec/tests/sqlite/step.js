var db = new SQLite(":memory:");
db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
db.run("INSERT INTO t (name) VALUES (?)", ["alpha"]);
db.run("INSERT INTO t (name) VALUES (?)", ["beta"]);
db.run("INSERT INTO t (name) VALUES (?)", ["gamma"]);

var stmt = db.prepare("SELECT id, name FROM t ORDER BY id");
if (!(stmt instanceof SQLiteStatement)) throw new Error("not SQLiteStatement");
if (stmt.column_count !== 2) throw new Error("column_count != 2");
if (stmt.column_names[0] !== "id") throw new Error("column 0 not 'id'");
if (stmt.column_names[1] !== "name") throw new Error("column 1 not 'name'");

var names = [];
var row;
while ((row = stmt.step()) !== null) {
	if (!(row instanceof SQLiteRow)) throw new Error("not SQLiteRow");
	names.push(row[1].text);
}
if (names.length !== 3) throw new Error("expected 3 rows");
if (names[0] !== "alpha" || names[2] !== "gamma") throw new Error("name order wrong");

stmt.reset();
var r1 = stmt.step();
/* Row's $-prefixed static properties should give real numbers / arrays,
 * not undefined. This regressed silently once when the tinyids went
 * outside int8 range. */
if (r1.$length !== 2) throw new Error("row.$length: expected 2, got " + r1.$length);
if (r1.$column_names[0] !== "id") throw new Error("$column_names[0]");
if (r1.$column_names[1] !== "name") throw new Error("$column_names[1]");
if (r1.$valid !== true) throw new Error("$valid on live row should be true");

/* Values are eager snapshots, so we can grab them now and they outlive
 * the next step(). The Row itself, however, is still a live view. */
var v1 = r1[1];        /* this fires the eager fetch */
var name1 = r1.name;   /* and again */
stmt.step();           /* advances the statement; r1 (Row) is now stale */

if (r1.$valid !== false) throw new Error("$valid should be false after step()");

if (v1.text !== "alpha") throw new Error("eager Value did not survive step(): got " + v1.text);
if (name1.text !== "alpha") throw new Error("eager Value by column-name did not survive step()");

var threw = false;
try { var _ = r1[0]; } catch (e) { threw = true; }
if (!threw) throw new Error("stale Row did not throw on access after advance");

stmt.close();
/* Values survive even after the statement is closed. */
if (v1.text !== "alpha") throw new Error("Value did not survive stmt.close()");
db.close();
/* And after the connection is closed. */
if (v1.text !== "alpha") throw new Error("Value did not survive db.close()");
