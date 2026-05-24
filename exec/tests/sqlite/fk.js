var db = new SQLite(":memory:");
db.exec("CREATE TABLE teams ("
      + " id INTEGER PRIMARY KEY,"
      + " name TEXT NOT NULL,"
      + " mgr_id INTEGER REFERENCES employees (id))");
db.exec("CREATE TABLE employees ("
      + " id INTEGER PRIMARY KEY,"
      + " name TEXT NOT NULL,"
      + " team_id INTEGER REFERENCES teams (id))");

var t = db.table.teams.new_row();
t.name = "engineering";
t.$insert();
var t2 = db.table.teams.new_row();
t2.name = "sales";
t2.$insert();

var e = db.table.employees.new_row();
e.name = "alice";
e.team_id = t.id;
e.$insert();

/* Basic single-column @col traversal. */
var fetched = db.table.employees.row(e.id);
var team = fetched['@team_id'];
if (!(team instanceof SQLiteRecord)) throw new Error("@team_id not a record");
if (team.name !== "engineering") throw new Error("@team_id wrong: " + team.name);
if (team.id !== t.id) throw new Error("@team_id id mismatch");

/* @col on null FK value -> null. */
var orphan = db.table.employees.new_row();
orphan.name = "bob";
/* team_id left undefined */
orphan.$insert();
var o = db.table.employees.row(orphan.id);
if (o['@team_id'] !== null) throw new Error("@team_id on undefined fk should be null");
o.team_id = null;
o.$update();
o = db.table.employees.row(orphan.id);
if (o['@team_id'] !== null) throw new Error("@team_id on null fk should be null");

/* @col on a non-FK column returns undefined (NOT throw). */
if (fetched['@name'] !== undefined)
	throw new Error("@name (no FK) should be undefined, got " + fetched['@name']);

/* No-caching: each access produces a fresh SQLiteRecord. */
var a = fetched['@team_id'];
var b = fetched['@team_id'];
if (a === b) throw new Error("@col should not cache (fresh record per access)");
/* But the data is the same. */
if (a.name !== b.name) throw new Error("@col data mismatch across calls");

/* Modifying the resolved record and update()ing it persists to the target table. */
team.name = "Engineering!";
team.$update();
if (db.query("SELECT name FROM teams WHERE id = ?", [t.id])[0].name !== "Engineering!")
	throw new Error("update on @col target didn't persist");

/* Chained navigation. */
db.run("UPDATE teams SET mgr_id = ? WHERE id = ?", [e.id, t.id]);
var t_fetched = db.table.teams.row(t.id);
var mgr = t_fetched['@mgr_id'];
if (!mgr || mgr.name !== "alice") throw new Error("chain hop 1: " + (mgr && mgr.name));
var mgr_team = mgr['@team_id'];
if (!mgr_team || mgr_team.id !== t.id) throw new Error("chain hop 2");

/* Composite FK throws on @col access. */
db.exec("CREATE TABLE cpk (a INTEGER, b INTEGER, n TEXT, PRIMARY KEY (a, b))");
db.exec("CREATE TABLE cref ("
      + " id INTEGER PRIMARY KEY,"
      + " a INTEGER, b INTEGER,"
      + " FOREIGN KEY (a, b) REFERENCES cpk (a, b))");
db.exec("INSERT INTO cpk VALUES (1, 2, 'hi')");
db.exec("INSERT INTO cref VALUES (1, 1, 2)");
var cr = db.table.cref.row(1);
var threw = false;
try { var _ = cr['@a']; } catch (e) { threw = true; }
if (!threw) throw new Error("composite FK @col should throw");

/* FK referencing a non-PK column also throws (current scope limit). */
db.exec("CREATE TABLE refs_uniq ("
      + " id INTEGER PRIMARY KEY,"
      + " uniq TEXT UNIQUE)");
db.exec("CREATE TABLE refs_to_uniq ("
      + " id INTEGER PRIMARY KEY,"
      + " ref TEXT REFERENCES refs_uniq (uniq))");
db.exec("INSERT INTO refs_uniq (uniq) VALUES ('x')");
db.exec("INSERT INTO refs_to_uniq (ref) VALUES ('x')");
var rr = db.table.refs_to_uniq.row(1);
threw = false;
try { var _ = rr['@ref']; } catch (e) { threw = true; }
if (!threw) throw new Error("non-PK-target FK @col should throw");

db.close();
