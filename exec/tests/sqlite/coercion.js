var db = new SQLite(":memory:");
db.exec("CREATE TABLE t (i INTEGER, r REAL, s TEXT, n)");
db.run("INSERT INTO t VALUES (?, ?, ?, ?)", [42, 3.5, "hello", null]);

var sel = db.prepare("SELECT i, r, s, n FROM t");
var row = sel.step();
if (row === null) throw new Error("no row");

/* Arithmetic should go through valueOf, not toString. */
if (row.i + 10 !== 52) throw new Error("integer + number: expected 52, got " + (row.i + 10));
if (row.r * 2 !== 7) throw new Error("real * number: expected 7, got " + (row.r * 2));
if (row.i - 2 !== 40) throw new Error("integer - number: expected 40, got " + (row.i - 2));

/* Loose equality through valueOf. */
if (!(row.i == 42)) throw new Error("integer == 42 should be true");
if (!(row.r == 3.5)) throw new Error("real == 3.5 should be true");

/* Null-column detection: obj == null is always false in JS (spec quirk:
 * abstract equality only special-cases null vs undefined, not obj vs null).
 * Use .type or .value for null checks. */
if (row.n.type !== "null") throw new Error(".type should be 'null'");
if (row.n.value !== null) throw new Error(".value should be null");

/* String concat: when one operand is a string, + concatenates. */
if (("x=" + row.i) !== "x=42") throw new Error('string concat: expected "x=42", got "' + ("x=" + row.i) + '"');
if ((row.s + "!") !== "hello!") throw new Error("text + string concat");

/* Strict === against a primitive is false because Value is an object;
 * scripts must use .integer / .real / .text / .value for that. */
if (row.i === 42) throw new Error("strict === against primitive should be false (Value is an object)");
if (row.i.integer !== 42) throw new Error(".integer should give 42");

sel.close();
db.close();
