/* sqlite_example.js
 *
 * A short, correct reference for using the SQLite JS bindings.
 *
 * THE GOLDEN RULE
 *
 *   Any value that comes from a user, a config file, a network peer, or
 *   anywhere outside the script itself MUST be passed to SQLite as a
 *   BOUND PARAMETER. Never paste it into the SQL string.
 *
 *   Correct:   db.run("INSERT INTO t (name) VALUES (?)", [name]);
 *   Wrong:     db.run("INSERT INTO t (name) VALUES ('" + name + "')");
 *
 *   The "wrong" form looks fine until 'name' is something like
 *       '); DROP TABLE t; --
 *   at which point your data is gone. With a bound parameter the value is
 *   never interpreted as SQL, no matter what characters it contains.
 *
 * Run via:   jsexec sqlite_example.js
 */

var dbpath = system.temp_path + "sqlite_example.db";
var db = new SQLite(dbpath);

/* ---- Multi-process friendliness -------------------------------------- */
/* The default busy_timeout (5s) handles brief contention. For high-
 * concurrency multi-process workloads, enable WAL mode once after
 * creating the database - readers don't block writers in WAL. */
db.exec("PRAGMA journal_mode = WAL");

/* ---- Schema setup ---------------------------------------------------- */
/* exec() takes a literal SQL string only. It accepts no parameters by
 * design, and is intended for DDL like this. Do not build it with + from
 * user input. */
db.exec(
	"CREATE TABLE IF NOT EXISTS scores (" +
	" id    INTEGER PRIMARY KEY," +
	" name  TEXT    NOT NULL," +
	" score INTEGER NOT NULL," +
	" stamp INTEGER NOT NULL)"
);

/* ---- Writing data: parameterized ------------------------------------- */
/* Positional placeholders (?) bound from an array. */
function record_score(name, score) {
	db.run(
		"INSERT INTO scores (name, score, stamp) VALUES (?, ?, ?)",
		[name, score, time()]
	);
}

/* Named placeholders (:name) bound from an object. Equivalent in effect;
 * use whichever reads better at the call site. */
function record_score_named(name, score) {
	db.run(
		"INSERT INTO scores (name, score, stamp) VALUES (:name, :score, :t)",
		{ ":name": name, ":score": score, ":t": time() }
	);
}

/* Wrap multiple writes in a transaction. The function is called between
 * BEGIN IMMEDIATE and COMMIT; if it throws, everything is ROLLED BACK. */
db.transaction(function () {
	record_score("Alice",        12345);
	record_score("'Bobby Tables", 23456);  /* quote-safe via binding */
	record_score_named("Carol",  9999);
});

/* ---- Reading data: parameterized too --------------------------------- */
/* query() does prepare + bind + step-all + finalize in one call.
 * Returns an array of plain row objects keyed by column name. */
var threshold = 10000;
var winners = db.query(
	"SELECT name, score FROM scores WHERE score >= ? ORDER BY score DESC",
	[threshold]
);
writeln("Scores at or above " + threshold + ":");
for (var i = 0; i < winners.length; i++)
	writeln("  " + winners[i].name + " = " + winners[i].score);

/* ---- Streaming large result sets ------------------------------------- */
/* For result sets you don't want to buffer in memory, prepare a statement
 * and step through it. Each step() returns a SQLiteRow whose columns are
 * SQLiteValue objects: row[i] or row["col_name"] both work.
 *
 * Note: each row[x] access creates a fresh SQLiteValue instance, so
 * `row[0] !== row[0]`. If you need to reference the same Value object
 * twice (e.g. for identity checks), assign it to a local first. The
 * column data is identical regardless. */
var stmt = db.prepare("SELECT name, score FROM scores ORDER BY score DESC");
var row, rank = 1;
while ((row = stmt.step()) !== null) {
	/* Use the typed accessor that matches the column's storage. */
	writeln(rank + ". " + row.name.text + " (" + row.score.integer + ")");
	rank++;
}
stmt.close();

/* ---- Re-using a prepared statement ----------------------------------- */
/* When the same SQL runs many times with different values, prepare it
 * once and reset() between executions. Avoids re-parsing on every call. */
var lookup = db.prepare("SELECT score FROM scores WHERE name = ?");
function score_for(name) {
	lookup.reset();
	lookup.bind([name]);
	var r = lookup.step();
	return r === null ? null : r.score.integer;
}
writeln("Alice's score: " + score_for("Alice"));
writeln("Nobody's score: " + score_for("nobody"));
lookup.close();

/* ---- LIKE / GLOB pattern escaping ------------------------------------ */
/* Bound parameters in regular SQL contexts are literal - no escaping
 * needed. The ONE exception is LIKE / GLOB pattern values, where '%' and
 * '_' (or '*' and '?' for GLOB) are wildcard metacharacters interpreted
 * by SQLite. Use SQLite.escape_like() ONLY when the user's literal text
 * is going into a LIKE/GLOB pattern, and pair it with ESCAPE in the SQL.
 *
 * DO NOT use escape_like() as a general 'safe text' helper. Binding
 * already handles SQL injection. Misapplying it inserts literal
 * backslashes into your data. */
db.exec("CREATE TABLE log (msg TEXT)");
db.run("INSERT INTO log VALUES (?)", ["100% Complete"]);
db.run("INSERT INTO log VALUES (?)", ["100 percent (no symbol)"]);
db.run("INSERT INTO log VALUES (?)", ["100% Complete!"]);

/* Prefix match for the user's literal "100% Complete": */
var needle = "100% Complete";
var hits = db.query(
	"SELECT msg FROM log WHERE msg LIKE ? ESCAPE '\\'",
	[SQLite.escape_like(needle) + "%"]
);
writeln('Prefix-match for literal "' + needle + '":');
for (var i = 0; i < hits.length; i++)
	writeln("  " + hits[i].msg);
/* Matches "100% Complete" and "100% Complete!" - NOT "100 percent...",
 * because the % in the needle is escaped to mean literal %. */

/* ---- Schema introspection -------------------------------------------- */
/* Useful when writing generic tools that don't know the schema. */
writeln("Tables: " + db.tables().join(", "));
writeln("Columns in scores:");
var cols = db.columns("scores");
for (var i = 0; i < cols.length; i++) {
	writeln("  " + cols[i].name + " " + cols[i].type
	    + (cols[i].notnull ? " NOT NULL" : "")
	    + (cols[i].pk ? " (pk=" + cols[i].pk + ")" : ""));
}
if (!db.has_table("logs"))
	db.exec("CREATE TABLE logs (msg TEXT)");

/* ---- Debugging with expanded_sql ------------------------------------- */
/* When a query doesn't return what you expected, .expanded_sql shows the
 * actual SQL after binding. Don't feed it back into prepare() - it's for
 * logging only. */
var debug = db.prepare("SELECT name FROM scores WHERE score > ? AND name LIKE ?");
debug.bind([5000, "%"]);
writeln("Would run: " + debug.expanded_sql);
debug.close();

/* ---- Online backup --------------------------------------------------- */
/* sqlite3_backup_* atomically copies a live database, safe even while
 * other processes are writing to it (a raw cp/file_copy can race). */
var backup_path = system.temp_path + "sqlite_example_backup.db";
db.backup(backup_path);
writeln("Backed up to " + backup_path);
file_remove(backup_path);

/* ---- Table-scoped accessor: db.table.<name> -------------------------- */
/* For the common "read a row, change a field, write it back" pattern, a
 * SQLiteTable object exposes the schema and gives you SQLiteRecord
 * instances that know how to insert/update/remove themselves. The path
 * of least resistance still goes through bound parameters - column
 * names come from the cached schema, not from your strings.
 *
 * db.table.<name> is case-insensitive; bracket form works for names
 * that aren't valid JS identifiers (db.table["with-dash"]). Tables that
 * don't exist resolve to undefined. The schema is loaded once per table
 * and cached for the connection's lifetime - reopen the connection if
 * you need to see DDL changes. */
var Scores = db.table.scores;
writeln("Table.name = " + Scores.name + "; pk = " + Scores.pk.join(","));

/* new_row() evaluates DEFAULT expressions (including CURRENT_TIMESTAMP)
 * and returns a SQLiteRecord with those values filled in. Columns with
 * no default are left undefined. */
var r = Scores.new_row();
r.name  = "Dave";
r.score = 31337;
r.stamp = time();
r.$insert();         /* INSERT INTO scores (defined cols); backfills the
                     * auto-assigned rowid into r.id. */
writeln("Inserted with id = " + r.id);

/* row(pk) loads a single row by primary key, or returns null. */
var found = Scores.row(r.id);
writeln("Loaded: " + found.name + " = " + found.score);

/* Mutate and update() - WHERE uses the *original* PK captured at row()
 * time, so changing the PK column is a valid rename. */
found.score = found.score + 1;
var changed = found.$update();
writeln(changed + " row updated to score = " + found.score);

/* select(where_sql, params) returns an array of SQLiteRecord. where_sql
 * may be null for "all rows"; if it has placeholders, params is
 * required (same rule as query() / run()). */
var winners = Scores.select("score > ? ORDER BY score DESC", [10000]);
writeln("Records with score > 10000:");
for (var i = 0; i < winners.length; i++)
	writeln("  " + winners[i].name + " = " + winners[i].score);

/* @col: lazy foreign-key traversal. If a column is a single-column FK,
 * `record['@<col>']` returns a fresh SQLiteRecord on the referenced
 * table. Each access is a fresh fetch - assign to a local if you want
 * a stable reference. Composite FKs throw with a clear message; manual
 * joins via db.query() are the path for those. */
db.exec("CREATE TABLE teams (id INTEGER PRIMARY KEY, name TEXT)");
db.exec("CREATE TABLE players ("
      + " id INTEGER PRIMARY KEY,"
      + " name TEXT NOT NULL,"
      + " team_id INTEGER REFERENCES teams (id))");
db.run("INSERT INTO teams (id, name) VALUES (?, ?)", [1, "Engineering"]);
db.run("INSERT INTO players (name, team_id) VALUES (?, ?)", ["Alice", 1]);

var alice = db.table.players.row(1);
writeln(alice.name + " plays for " + alice['@team_id'].name);

/* toObject() takes a snapshot suitable for binding into other queries
 * or external serialization. Undefined columns are skipped; null is
 * preserved as a real value. */
var snap = alice.$toObject();
writeln("As object: " + JSON.stringify(snap));

/* remove() deletes by original PK. Returns the changes count. */
var stragglers = Scores.select("score < ?", [100]);
for (var i = 0; i < stragglers.length; i++)
	stragglers[i].$remove();

db.close();
file_remove(dbpath);
