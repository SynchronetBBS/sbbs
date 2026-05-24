var db = new SQLite(":memory:");
db.exec("CREATE TABLE t ("
      + " id INTEGER PRIMARY KEY,"
      + " title TEXT DEFAULT 'untitled',"
      + " score INTEGER DEFAULT 0,"
      + " ratio REAL DEFAULT 1.5,"
      + " stamp TEXT DEFAULT CURRENT_TIMESTAMP,"
      + " note TEXT NOT NULL,"
      + " bonus REAL"
      + ")");

var r = db.table.t.new_row();
if (!(r instanceof SQLiteRecord)) throw new Error("not SQLiteRecord");

/* Defaults are evaluated by SQLite. */
if (r.title !== "untitled") throw new Error("title default: " + r.title);
if (r.score !== 0) throw new Error("score default: " + r.score);
if (r.ratio !== 1.5) throw new Error("ratio default: " + r.ratio);
if (typeof r.stamp !== "string" || r.stamp.length < 10)
	throw new Error("stamp default: " + r.stamp);

/* No default => undefined (NOT null — null is a real explicit value). */
if (r.id !== undefined) throw new Error("id (PK no default) should be undefined");
if (r.note !== undefined) throw new Error("note should be undefined");
if (r.bonus !== undefined) throw new Error("bonus should be undefined");

/* Writes land. */
r.note  = "hello";
r.bonus = 3.14;
if (r.note !== "hello") throw new Error("note write");
if (r.bonus !== 3.14)   throw new Error("bonus write");

/* null is distinct from undefined. */
r.bonus = null;
if (r.bonus !== null) throw new Error("null write didn't stick");

/* Write to bogus column throws. */
var threw = false;
try { r.nonexistent = 1; } catch (e) { threw = true; }
if (!threw) throw new Error("write to nonexistent column should throw");

/* Read of bogus column returns undefined (NOT throw — prototype lookup
 * needs to fall through). */
if (r.bogus !== undefined) throw new Error("read of bogus should be undefined");

/* Methods reachable via prototype. */
if (typeof r.$insert !== "function")   throw new Error("$insert method missing");
if (typeof r.$update !== "function")   throw new Error("$update method missing");
if (typeof r.$remove !== "function")   throw new Error("$remove method missing");
if (typeof r.$toObject !== "function") throw new Error("$toObject method missing");

/* CURRENT_TIMESTAMP re-evaluates per new_row(). */
var a = db.table.t.new_row().stamp;
mswait(1100);
var b = db.table.t.new_row().stamp;
if (a === b) throw new Error("CURRENT_TIMESTAMP should re-evaluate");

/* No-defaults table also works (all columns start undefined). */
db.exec("CREATE TABLE p (x INTEGER, y INTEGER)");
var rp = db.table.p.new_row();
if (rp.x !== undefined || rp.y !== undefined)
	throw new Error("no-defaults columns should be undefined");

/* toObject(): plain JS object with only defined columns; null preserved. */
var rt = db.table.t.new_row();
rt.note  = "hi";
rt.bonus = null;
var snap = rt.$toObject();
if (snap instanceof SQLiteRecord) throw new Error("toObject should be plain object");
if (snap.title !== "untitled") throw new Error("snap.title");
if (snap.score !== 0)          throw new Error("snap.score");
if (snap.note  !== "hi")       throw new Error("snap.note");
if ('bonus' in snap === false || snap.bonus !== null)
	throw new Error("explicit null bonus should be in snapshot as null");
if ('id' in snap)              throw new Error("undefined id should not appear");
/* Mutating the snapshot doesn't affect the record. */
snap.note = "changed";
if (rt.note !== "hi") throw new Error("snapshot mutation leaked back");

/* toObject() result is directly bindable into queries that reference
 * any subset of its keys — keys without a matching placeholder are
 * silently skipped. */
db.run("INSERT INTO t (note, title) VALUES (:note, :title)", rt.$toObject());
var got = db.query("SELECT note, title FROM t WHERE note = ?", ["hi"])[0];
if (got.note !== "hi" || got.title !== "untitled")
	throw new Error("bound snapshot insert");

db.close();
