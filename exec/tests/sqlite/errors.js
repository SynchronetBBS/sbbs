var db = new SQLite(":memory:");
db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, email TEXT UNIQUE NOT NULL)");
db.run("INSERT INTO t (id, email) VALUES (?, ?)", [1, "alice@example.com"]);

/* The deliberate constraint violations below would otherwise produce
 * SQLite log noise via the default lprintf router. Suppress around the
 * expected-error region and restore. */
var prev_log = SQLite.set_log(null);

/* Trigger a UNIQUE constraint violation. Extended codes are enabled, so
 * error_code carries the specialized variant. */
var threw = false;
try {
	db.run("INSERT INTO t (id, email) VALUES (?, ?)", [2, "alice@example.com"]);
} catch (e) {
	threw = true;
}
if (!threw) throw new Error("UNIQUE constraint violation didn't throw");
if (db.error_code !== SQLite.CONSTRAINT_UNIQUE)
	throw new Error("expected CONSTRAINT_UNIQUE (" + SQLite.CONSTRAINT_UNIQUE
	    + "), got " + db.error_code);
/* Primary code is the low byte. */
if ((db.error_code & 0xFF) !== SQLite.CONSTRAINT)
	throw new Error("low byte should be primary CONSTRAINT");

/* Different constraint, different extended code. */
threw = false;
try { db.run("INSERT INTO t (id, email) VALUES (?, ?)", [3, null]); }
catch (e) { threw = true; }
if (!threw) throw new Error("NOT NULL violation didn't throw");
if (db.error_code !== SQLite.CONSTRAINT_NOTNULL)
	throw new Error("expected CONSTRAINT_NOTNULL, got " + db.error_code);

SQLite.set_log(prev_log);

/* SQLite.errstr(code) returns the English text for a code. */
if (typeof SQLite.errstr !== "function") throw new Error("SQLite.errstr should be a function");
var s = SQLite.errstr(SQLite.BUSY);
if (typeof s !== "string" || s.length === 0) throw new Error("errstr(BUSY) returned empty");
if (SQLite.errstr(SQLite.OK).length === 0) throw new Error("errstr(OK) returned empty");

db.close();
