var db = new SQLite(":memory:");
db.exec("CREATE TABLE users ("
      + " id INTEGER PRIMARY KEY,"
      + " name TEXT NOT NULL,"
      + " team_id INTEGER,"
      + " FOREIGN KEY (team_id) REFERENCES teams (id))");
db.exec("CREATE TABLE teams (id INTEGER PRIMARY KEY, name TEXT)");

/* db.table.X returns a SQLiteTable. */
var u = db.table.users;
if (!u) throw new Error("db.table.users undefined");
if (!(u instanceof SQLiteTable)) throw new Error("not SQLiteTable");

/* Schema introspection: name, columns, pk, foreign_keys. */
if (u.name !== "users") throw new Error("name: " + u.name);
if (u.columns.length !== 3) throw new Error("columns: " + u.columns.length);
if (u.columns[0].name !== "id" || !u.columns[0].pk)
	throw new Error("id col wrong");
if (!u.columns[1].notnull)
	throw new Error("name should be NOT NULL");
if (u.pk.length !== 1 || u.pk[0] !== "id")
	throw new Error("pk wrong: " + u.pk.join(","));

if (u.foreign_keys.length !== 1) throw new Error("fk count: " + u.foreign_keys.length);
var fk = u.foreign_keys[0];
if (fk.ncol !== 1) throw new Error("fk ncol: " + fk.ncol);
if (fk.from_col[0] !== "team_id") throw new Error("fk from_col: " + fk.from_col[0]);
if (fk.to_table !== "teams") throw new Error("fk to_table: " + fk.to_table);
if (fk.to_col[0] !== "id") throw new Error("fk to_col: " + fk.to_col[0]);

/* Case-insensitive lookup returns the same SQLiteTable. */
if (db.table.USERS !== db.table.users) throw new Error("case-insensitive identity broken");
if (db.table.Users !== db.table.users) throw new Error("mixed-case identity broken");

/* Bracket form works for any property name. */
if (db.table["users"] !== db.table.users) throw new Error("bracket form differs");

/* Missing table -> undefined (NOT throw). */
if (db.table.nope !== undefined)
	throw new Error("nope should be undefined, got " + db.table.nope);

/* Views are not exposed under db.table (v1 scope). */
db.exec("CREATE VIEW v_users AS SELECT * FROM users");
if (db.table.v_users !== undefined)
	throw new Error("view should be undefined, got " + db.table.v_users);

/* sqlite_* internal tables are real tables in sqlite_master but the
 * resolve hook restricts to type='table' which includes them; this just
 * documents the current behavior. */

/* Schema is cached: the SQLiteTable identity is stable. */
var first = db.table.users;
var second = db.table.users;
if (first !== second) throw new Error("table identity not cached");

/* Composite PK table. */
db.exec("CREATE TABLE cpk (a INTEGER, b INTEGER, name TEXT, PRIMARY KEY (a, b))");
var c = db.table.cpk;
if (c.pk.length !== 2) throw new Error("composite pk count: " + c.pk.length);
if (c.pk[0] !== "a" || c.pk[1] !== "b")
	throw new Error("composite pk order: " + c.pk.join(","));

/* Composite FK. */
db.exec("CREATE TABLE cfk ("
      + " a INTEGER, b INTEGER, name TEXT,"
      + " FOREIGN KEY (a, b) REFERENCES cpk (a, b))");
var cf = db.table.cfk;
if (cf.foreign_keys.length !== 1) throw new Error("cfk fk count");
if (cf.foreign_keys[0].ncol !== 2) throw new Error("composite fk ncol");
if (cf.foreign_keys[0].from_col.length !== 2 || cf.foreign_keys[0].to_col.length !== 2)
	throw new Error("composite fk arrays");

/* No-PK table. */
db.exec("CREATE TABLE nopk (x INTEGER, y INTEGER)");
if (db.table.nopk.pk.length !== 0) throw new Error("no-pk table should have empty pk");

db.close();
