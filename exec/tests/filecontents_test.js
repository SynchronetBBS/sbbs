// filecontents_test.js -- exercises the per-file-area content listing store.
// Run: jsexec exec/tests/filecontents_test.js
//
// Writes one data/dirs/<code>.contents file for the area holding the first ZIP
// it finds, and removes it again.
//
// SpiderMonkey 1.8.5: no let/const, no arrow functions, no Object.keys().

load("filecontents_lib.js");

var failures = 0;

function check(cond, what)
{
	if (!cond) {
		print("FAIL: " + what);
		failures++;
	} else
		print("ok: " + what);
}

// Find one file of each kind to work with.
function find(pattern)
{
	for (var code in file_area.dir) {
		var d = file_area.dir[code];
		var fb = new FileBase(d.code);
		if (!fb.open())
			continue;
		var names = fb.get_names();
		for (var j = 0; j < names.length; j++) {
			if (!pattern.test(names[j]))
				continue;
			var p = d.path + names[j];
			if (file_exists(p)) {
				fb.close();
				return p;
			}
		}
		fb.close();
	}
	return null;
}

var zip = find(/\.zip$/i);
var arc = find(/\.arc$/i);

check(zip !== null, "found a .zip to test with");
check(arc !== null, "found an .arc to test with");

/* ---- path to area mapping ---- */

check(filecontents_area(zip) !== undefined,
      "filecontents_area() resolves a file-base path");
check(filecontents_area("/tmp/definitely-not-in-a-file-area.zip") === undefined,
      "filecontents_area() returns undefined outside every file area");

/* ---- XAD date parsing ---- */

var d = filecontents_xaddate("1989-12-25 01:02:00 -0800");
check(d > 0, "filecontents_xaddate() parses an lsar timestamp (got " + d + ")");
check(filecontents_xaddate(undefined) === 0,
      "filecontents_xaddate() tolerates a missing date");

/* ---- extraction: libarchive path ---- */

var rec = filecontents_extract(zip);
check(rec.t === "archive", "zip record is typed archive");
check(rec.x === "libarchive", "zip extracted by libarchive (got " + rec.x + ")");
check(rec.err === undefined, "zip extracted without error");
check(rec.l !== undefined && rec.l.length > 0,
      "zip listing is non-empty (" + (rec.l ? rec.l.length : 0) + " entries)");
check(rec.sz === file_size(zip) && rec.mt === file_date(zip),
      "zip record carries the validity key");
if (rec.l && rec.l.length)
	check(rec.l[0].length === 3, "entries are [name, size, time] triples");

/* ---- extraction: external-tool fallback ---- */

if (arc !== null) {
	var arec = filecontents_extract(arc);
	check(arec.x === "lsar",
	      "arc fell back to lsar (got " + arec.x
	      + (arec.err ? ", err=" + arec.err : "") + ")");
	check(arec.err === undefined, "arc extracted without error");
	check(arec.l !== undefined && arec.l.length > 0,
	      "arc listing is non-empty (" + (arec.l ? arec.l.length : 0) + " entries)");
	if (arec.l && arec.l.length)
		check(typeof arec.l[0][0] === 'string' && arec.l[0][0].length > 0,
		      "arc entry has a name (" + (arec.l ? arec.l[0][0] : "") + ")");
}

/* ---- shell safety ---- */

check(filecontents_extract_external("/tmp/evil$(id).arc") === null,
      "external tool refused for a path containing $");
check(filecontents_extract_external("/tmp/evil`id`.arc") === null,
      "external tool refused for a path containing a backtick");

/* ---- store round trip ---- */

var code = filecontents_area(zip);
var store_file = filecontents_store(code);
var preexisting = file_exists(store_file);

check(filecontents_get(zip) === null, "nothing stored for the zip yet");
check(filecontents_put(zip, rec) === true, "filecontents_put() stored the record");
check(file_exists(store_file), "store file was created: " + store_file);

var got = filecontents_get(zip);
check(got !== null, "filecontents_get() returns the stored record");
if (got !== null) {
	check(got.l.length === rec.l.length, "round-tripped listing has the same length");
	check(got.x === rec.x, "round-tripped extractor survives");
}

/* ---- staleness ---- */

var stale = { t: "archive", sz: rec.sz + 1, mt: rec.mt, xv: 1, l: [] };
check(filecontents_current(stale, zip) === false,
      "a size mismatch invalidates a record");
stale = { t: "archive", sz: rec.sz, mt: rec.mt + 1, xv: 1, l: [] };
check(filecontents_current(stale, zip) === false,
      "an mtime mismatch invalidates a record");
stale = { t: "archive", sz: rec.sz, mt: rec.mt, xv: 0, err: "nope" };
check(filecontents_current(stale, zip) === false,
      "an old extractor version invalidates a stored failure");
stale = { t: "archive", sz: rec.sz, mt: rec.mt, xv: 0, l: [] };
check(filecontents_current(stale, zip) === true,
      "an old extractor version does NOT invalidate a successful listing");

/* ---- put outside a file area is a benign false ---- */

check(filecontents_put("/tmp/not-in-an-area.zip", rec) === false,
      "filecontents_put() declines a path outside every file area");

/* ---- cleanup ---- */

if (!preexisting && file_exists(store_file)) {
	file_remove(store_file);
	check(!file_exists(store_file), "store file removed again");
}

print("");
print(failures === 0 ? "PASS" : (failures + " FAILURE(S)"));
exit(failures === 0 ? 0 : 1);
