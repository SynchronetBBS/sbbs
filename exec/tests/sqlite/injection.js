var db = new SQLite(":memory:");
db.exec("CREATE TABLE t (k TEXT)");

/* exec() must refuse a params argument by design. */
var threw = false;
try { db.exec("SELECT 1", [1]); } catch (e) { threw = true; }
if (!threw) throw new Error("exec() should reject params argument");

/* query() with placeholders but no params must throw. */
threw = false;
try { db.query("SELECT * FROM t WHERE k = ?"); } catch (e) { threw = true; }
if (!threw) throw new Error("query() with placeholder but no params should throw");

/* No escape/quote helpers exposed. */
if (typeof db.escape !== "undefined") throw new Error("db.escape should not exist");
if (typeof db.quote !== "undefined") throw new Error("db.quote should not exist");

/* A bound text value must never be re-parsed as SQL. */
db.run("INSERT INTO t (k) VALUES (?)", ["'); DROP TABLE t; --"]);
var rows = db.query("SELECT k FROM t");
if (rows.length !== 1) throw new Error("injection-shaped string was interpreted as SQL");
if (rows[0].k !== "'); DROP TABLE t; --") throw new Error("not stored verbatim");
db.close();
