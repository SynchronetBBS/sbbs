var db = new SQLite(":memory:");
db.exec("CREATE TABLE u ("
      + " id INTEGER PRIMARY KEY,"
      + " name TEXT NOT NULL,"
      + " score INTEGER DEFAULT 0)");

for (var i = 0; i < 5; i++) {
	var r = db.table.u.new_row();
	r.name  = "user" + i;
	r.score = i * 10;
	r.$insert();
}

/* row(pk) round-trips: load by PK, returns SQLiteRecord. */
var u = db.table.u.row(3);
if (!(u instanceof SQLiteRecord)) throw new Error("row() should return SQLiteRecord");
if (u.id !== 3) throw new Error("row() loaded wrong id");
if (u.name !== "user2") throw new Error("row() name: " + u.name);
if (u.score !== 20) throw new Error("row() score: " + u.score);

/* row(pk) on missing PK returns null. */
if (db.table.u.row(9999) !== null)
	throw new Error("row() missing should be null");

/* Loaded record can be updated: full round-trip. */
u.score = 999;
u.$update();
if (db.query("SELECT score FROM u WHERE id = ?", [3])[0].score !== 999)
	throw new Error("loaded-record update failed");

/* Loaded record can be removed. */
u = db.table.u.row(3);
if (u.$remove() !== 1) throw new Error("row+remove changes");
if (db.table.u.row(3) !== null) throw new Error("row should be gone");

/* select() with WHERE + positional params. */
var hits = db.table.u.select("score >= ?", [20]);
if (hits.length !== 2) throw new Error("select score>=20 expected 2, got " + hits.length);
for (var i = 0; i < hits.length; i++) {
	if (!(hits[i] instanceof SQLiteRecord)) throw new Error("select element type");
	if (hits[i].score < 20) throw new Error("select didn't filter");
}

/* select() with null WHERE = all rows. */
var all = db.table.u.select(null);
if (all.length !== 4) throw new Error("select all expected 4, got " + all.length);  /* one was removed */

/* select() with named params. */
var nh = db.table.u.select("name = :n", { n: "user1" });
if (nh.length !== 1 || nh[0].name !== "user1") throw new Error("named-param select");

/* select() with no params arg but the WHERE has placeholders should throw. */
var threw = false;
try { db.table.u.select("score = ?"); } catch (e) { threw = true; }
if (!threw) throw new Error("missing params should throw");

/* select() returns records that can be updated/removed. */
var found = db.table.u.select("name = ?", ["user1"]);
found[0].score = 12345;
found[0].$update();
if (db.query("SELECT score FROM u WHERE name = ?", ["user1"])[0].score !== 12345)
	throw new Error("select+update round-trip");

/* select() with ORDER BY in the WHERE fragment (it's a free-form fragment
 * appended after WHERE, but ORDER BY etc. attach naturally to a WHERE-less
 * select if we just pass it without "1=1"). Use a real WHERE for clarity. */
var ordered = db.table.u.select("score > ? ORDER BY score DESC", [0]);
if (ordered.length < 2) throw new Error("ordered select size");
if (ordered[0].score < ordered[ordered.length - 1].score)
	throw new Error("ORDER BY DESC didn't sort");

/* select() WHERE matches no rows -> empty array. */
var nada = db.table.u.select("score < ?", [-1]);
if (!Array.isArray(nada) || nada.length !== 0) throw new Error("empty select");

/* Composite-PK row(). */
db.exec("CREATE TABLE c (a INTEGER, b INTEGER, note TEXT, PRIMARY KEY (a, b))");
var rc = db.table.c.new_row();
rc.a = 1; rc.b = 2; rc.note = "hi";
rc.$insert();
var got = db.table.c.row(1, 2);
if (!got || got.note !== "hi") throw new Error("composite row()");
/* Wrong arg count for composite PK. */
threw = false;
try { db.table.c.row(1); } catch (e) { threw = true; }
if (!threw) throw new Error("composite row() with too few args should throw");

/* row() on a no-PK table throws. */
db.exec("CREATE TABLE np (a INTEGER, b INTEGER)");
threw = false;
try { db.table.np.row(1); } catch (e) { threw = true; }
if (!threw) throw new Error("row() on no-PK should throw");

/* select() still works on no-PK tables (no orig_pk captured; mutation
 * methods would fail but the read path is fine). */
db.exec("INSERT INTO np VALUES (1, 2)");
var nps = db.table.np.select(null);
if (nps.length !== 1) throw new Error("select on no-PK table");

db.close();
