// filecontents_test.js -- exercises the per-file-area content listing store.
// Run: jsexec exec/tests/filecontents_test.js
//
// Writes one data/dirs/<code>.contents file for the area holding the first ZIP
// it finds, and removes it again.
//
// SpiderMonkey 1.8.5: no let/const, no arrow functions, no Object.keys().

var contents = load({}, "filecontents_lib.js");

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

check(contents.area(zip) !== undefined,
      "contents.area() resolves a file-base path");
check(contents.area("/tmp/definitely-not-in-a-file-area.zip") === undefined,
      "contents.area() returns undefined outside every file area");

/* ---- XAD date parsing ---- */

var d = contents.xaddate("1989-12-25 01:02:00 -0800");
check(d > 0, "contents.xaddate() parses an lsar timestamp (got " + d + ")");
check(contents.xaddate(undefined) === 0,
      "contents.xaddate() tolerates a missing date");

/* ---- extraction: libarchive path ---- */

var rec = contents.extract(zip);
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
	var arec = contents.extract(arc);
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

check(contents.extract_external("/tmp/evil$(id).arc") === null,
      "external tool refused for a path containing $");
check(contents.extract_external("/tmp/evil`id`.arc") === null,
      "external tool refused for a path containing a backtick");

/* ---- store round trip ---- */

var code = contents.area(zip);
var store_file = contents.store_path(code);
var preexisting = file_exists(store_file);

/* Not "nothing is stored yet": the store is live and may already hold this
   area, so test the absent case on a name that cannot be there. */
check(contents.get(file_getname(zip) + ".not-a-real-file") === null,
      "contents.get() returns null for a file with no record");
check(contents.put(zip, rec) === true, "contents.put() stored the record");
check(file_exists(store_file), "store file was created: " + store_file);

var got = contents.get(zip);
check(got !== null, "contents.get() returns the stored record");
if (got !== null) {
	check(got.l.length === rec.l.length, "round-tripped listing has the same length");
	check(got.x === rec.x, "round-tripped extractor survives");
}

/* ---- staleness ---- */

var stale = { t: "archive", sz: rec.sz + 1, mt: rec.mt, xv: 1, l: [] };
check(contents.current(stale, zip) === false,
      "a size mismatch invalidates a record");
stale = { t: "archive", sz: rec.sz, mt: rec.mt + 1, xv: 1, l: [] };
check(contents.current(stale, zip) === false,
      "an mtime mismatch invalidates a record");
stale = { t: "archive", sz: rec.sz, mt: rec.mt, xv: 0, err: "nope" };
check(contents.current(stale, zip) === false,
      "an old extractor version invalidates a stored failure");
stale = { t: "archive", sz: rec.sz, mt: rec.mt, xv: 0, l: [] };
check(contents.current(stale, zip) === true,
      "an old extractor version does NOT invalidate a successful listing");

/* ---- per-host extractor capability ---- */

var tool = contents.external();
check(tool.indexOf("lsar/") === 0,
      "contents.external() finds lsar and its version (got '" + tool + "')");

var noext = { t: "archive", sz: rec.sz, mt: rec.mt, xv: 1,
              err: "Unrecognized archive format", ext: "" };
check(contents.current(noext, zip) === false,
      "a failure recorded by a host with no external tool is not trusted here");

var sameext = { t: "archive", sz: rec.sz, mt: rec.mt, xv: 1,
                err: "Unrecognized archive format", ext: tool };
check(contents.current(sameext, zip) === true,
      "a failure recorded with the same tool this host has is trusted");

var otherext = { t: "archive", sz: rec.sz, mt: rec.mt, xv: 1,
                 err: "Unrecognized archive format", ext: "someothertool" };
check(contents.current(otherext, zip) === false,
      "a failure recorded with a different tool is re-examined");

/* ---- put outside a file area is a benign false ---- */

check(contents.put("/tmp/not-in-an-area.zip", rec) === false,
      "contents.put() declines a path outside every file area");

/* ---- cleanup ---- */

if (!preexisting && file_exists(store_file)) {
	file_remove(store_file);
	check(!file_exists(store_file), "store file removed again");
}

print("");
print(failures === 0 ? "PASS" : (failures + " FAILURE(S)"));
exit(failures === 0 ? 0 : 1);
