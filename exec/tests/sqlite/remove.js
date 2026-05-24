var db = new SQLite(":memory:");
db.exec("CREATE TABLE u (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");

var r = db.table.u.new_row();
r.name = "alice";
r.$insert();
var id = r.id;

/* Basic remove: row gone, changes == 1. */
var n = r.$remove();
if (n !== 1) throw new Error("first remove changes: " + n);
if (db.query("SELECT COUNT(*) AS c FROM u WHERE id = ?", [id])[0].c !== 0)
	throw new Error("row still present after remove");

/* Second remove() is a no-op (row already gone): changes == 0. */
n = r.$remove();
if (n !== 0) throw new Error("second remove changes: " + n);

/* remove on new_row() record (never inserted) throws. */
var fresh = db.table.u.new_row();
fresh.name = "nope";
var threw = false;
try { fresh.$remove(); } catch (e) { threw = true; }
if (!threw) throw new Error("remove on never-inserted should throw");

/* remove on no-PK table throws. */
db.exec("CREATE TABLE np (a INTEGER, b INTEGER)");
var rn = db.table.np.new_row();
rn.a = 1; rn.b = 2;
rn.$insert();
threw = false;
try { rn.$remove(); } catch (e) { threw = true; }
if (!threw) throw new Error("remove on no-PK should throw");

/* Composite PK delete. */
db.exec("CREATE TABLE c ("
      + " a INTEGER, b INTEGER, note TEXT,"
      + " PRIMARY KEY (a, b))");
var rc = db.table.c.new_row();
rc.a = 1; rc.b = 2; rc.note = "x";
rc.$insert();
if (rc.$remove() !== 1) throw new Error("composite-PK remove changes");
if (db.query("SELECT COUNT(*) AS c FROM c")[0].c !== 0)
	throw new Error("composite-PK row still present");

db.close();
