var db = new SQLite(":memory:");
db.exec("CREATE TABLE u ("
      + " id INTEGER PRIMARY KEY,"
      + " name TEXT NOT NULL,"
      + " score INTEGER DEFAULT 0)");

var r = db.table.u.new_row();
r.name = "alice";
r.$insert();

/* Basic update. */
r.score = 42;
var n = r.$update();
if (n !== 1) throw new Error("update changes count: " + n);
var back = db.query("SELECT score FROM u WHERE id = ?", [r.id])[0].score;
if (back !== 42) throw new Error("score update didn't persist: " + back);

/* PK rename: WHERE uses the original PK, SET writes the new value. */
var orig_id = r.id;
r.id = 999;
r.name = "alice2";
r.$update();
var renamed = db.query("SELECT id, name FROM u WHERE id = ?", [999]);
if (renamed.length !== 1 || renamed[0].name !== "alice2")
	throw new Error("PK rename didn't take");
if (db.query("SELECT COUNT(*) AS c FROM u WHERE id = ?", [orig_id])[0].c !== 0)
	throw new Error("old PK row still present after rename");

/* Subsequent update uses the NEW pk value (orig_pk was refreshed). */
r.score = 7;
r.$update();
if (db.query("SELECT score FROM u WHERE id = ?", [999])[0].score !== 7)
	throw new Error("post-rename update broke");

/* update() on a new_row() record (never inserted) throws. */
var fresh = db.table.u.new_row();
fresh.name = "nope";
var threw = false;
try { fresh.$update(); } catch (e) { threw = true; }
if (!threw) throw new Error("update on never-inserted should throw");

/* update() on a no-PK table throws. */
db.exec("CREATE TABLE np (a INTEGER, b INTEGER)");
var rn = db.table.np.new_row();
rn.a = 1; rn.b = 2;
rn.$insert();
threw = false;
try { rn.$update(); } catch (e) { threw = true; }
if (!threw) throw new Error("update on no-PK table should throw");

/* Composite PK round-trip: update by (a, b) original values. */
db.exec("CREATE TABLE c ("
      + " a INTEGER, b INTEGER, note TEXT,"
      + " PRIMARY KEY (a, b))");
var rc = db.table.c.new_row();
rc.a = 1; rc.b = 2; rc.note = "x";
rc.$insert();
rc.note = "y";
rc.$update();
if (db.query("SELECT note FROM c WHERE a=? AND b=?", [1, 2])[0].note !== "y")
	throw new Error("composite-PK update");

db.close();
