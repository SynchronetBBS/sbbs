// FileBase.update() semantics:
//
// - a property absent from the file object means "leave it alone", so the
//   stored extended description and auxdata survive an update that doesn't
//   mention them (an empty string still clears either one)
// - changed text is written to new data blocks in place: the record keeps its
//   position in the index instead of being removed and re-added
// - readd_always still removes and re-adds, which is what makes the file date
//   and list as newly imported
//
// Re-adding is observed through the physical order of the index file, since
// that is the only externally visible difference: a removed-and-re-added
// record is appended to the .sid, an updated one stays put.
//
// As in basic.js, an is_path FileBase resolves filenames relative to
// system.temp_dir (filedat.c:getfilepath when !dirnum_is_valid), so the files
// we add have to live there.

mkpath(system.temp_dir);

var stem = "test_filebase_update_" + Date.now();
var fbpath = system.temp_dir + stem;
var FBEXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash", ".ixt", ".ixb"];
var NAMES = ["tfu1.dat", "tfu2.dat", "tfu3.dat"];
var EXTDESC = "stored extended description";
var AUXDATA = '{"v":1,"stored":"listing"}';

function cleanup() {
	FBEXTENSIONS.forEach(function (ext) {
		try { file_remove(fbpath + ext); } catch (e) { /* ignore */ }
	});
	NAMES.forEach(function (name) {
		try { file_remove(system.temp_dir + name); } catch (e) { /* ignore */ }
	});
	try { file_remove(system.temp_dir + "tfu9.dat"); } catch (e) { /* ignore */ }
}

function open_base() {
	var fb = new FileBase(fbpath, /* is_path: */ true);
	if (!fb.open())
		throw new Error("FileBase open failed: " + fb.last_error);
	return fb;
}

// Names in the order the index file physically stores them.
function index_order() {
	var f = new File(fbpath + ".sid");
	if (!f.open("rb"))
		throw new Error("could not read index: " + f.error);
	var raw = f.read();
	f.close();
	var found = raw.match(/tfu[0-9]\.dat/g);
	return found === null ? [] : found;
}

function seed() {
	cleanup();
	NAMES.forEach(function (name) {
		var f = new File(system.temp_dir + name);
		if (!f.open("w"))
			throw new Error("could not create " + name + ": " + f.error);
		f.write("payload for " + name + "\n");
		f.close();
	});
	var fb = open_base();
	if (!fb.add({ name: NAMES[0], desc: "first", extdesc: EXTDESC, auxdata: AUXDATA, added: 1000000000 }))
		throw new Error("add failed: " + fb.last_error);
	if (!fb.add({ name: NAMES[1], desc: "second" }))
		throw new Error("add failed: " + fb.last_error);
	if (!fb.add({ name: NAMES[2], desc: "third" }))
		throw new Error("add failed: " + fb.last_error);
	fb.close();
}

function stored(name) {
	var fb = open_base();
	var file = fb.get(name === undefined ? NAMES[0] : name, FileBase.DETAIL.AUXDATA);
	fb.close();
	if (file === null)
		throw new Error("get() returned null");
	return file;
}

// truncsp() and the CRLF the base appends make an exact compare awkward.
function text_is(got, want)
{
	return got !== undefined && got.replace(/[\s]*$/, '') == want;
}

function check(what, got, want) {
	if (String(got) != String(want))
		throw new Error(what + ": got " + JSON.stringify(got) + ", expected " + JSON.stringify(want));
}

try {
	/* An update that mentions only the description leaves the rest alone. */
	seed();
	var fb = open_base();
	if (!fb.update(NAMES[0], { desc: "first (edited)" }))
		throw new Error("update failed: " + fb.last_error);
	fb.close();
	var file = stored();
	check("desc", file.desc, "first (edited)");
	if (!text_is(file.extdesc, EXTDESC))
		throw new Error("desc-only update lost the extended description: " + JSON.stringify(file.extdesc));
	if (!text_is(file.auxdata, AUXDATA))
		throw new Error("desc-only update lost the auxdata: " + JSON.stringify(file.auxdata));
	check("index order after desc-only update", index_order().join(), NAMES.join());

	/* The pattern used by rehashfiles.js and friends: read the list at the
	   default detail level, which carries neither text, and write it back. */
	seed();
	fb = open_base();
	var list = fb.get_list();
	for (var i = 0; i < list.length; i++)
		if (list[i].name == NAMES[0] && !fb.update(list[i].name, list[i]))
			throw new Error("update failed: " + fb.last_error);
	fb.close();
	file = stored();
	if (!text_is(file.extdesc, EXTDESC))
		throw new Error("get_list() round-trip lost the extended description");
	if (!text_is(file.auxdata, AUXDATA))
		throw new Error("get_list() round-trip lost the auxdata");

	/* Handing back identical auxdata is not a change. */
	seed();
	fb = open_base();
	fb.update(NAMES[0], { desc: "first", auxdata: AUXDATA });
	fb.close();
	if (!text_is(stored().auxdata, AUXDATA))
		throw new Error("identical auxdata was not preserved");
	check("index order after no-op auxdata update", index_order().join(), NAMES.join());

	/* Changed text is written in place: the record keeps its index position. */
	seed();
	fb = open_base();
	fb.update(NAMES[0], { desc: "first", extdesc: "a longer extended description than before" });
	fb.close();
	file = stored();
	if (!text_is(file.extdesc, "a longer extended description than before"))
		throw new Error("extended description not written: " + JSON.stringify(file.extdesc));
	if (!text_is(file.auxdata, AUXDATA))
		throw new Error("changing the extended description lost the auxdata");
	check("index order after text change", index_order().join(), NAMES.join());

	/* An empty string clears, and clears only what was named. */
	seed();
	fb = open_base();
	fb.update(NAMES[0], { desc: "first", auxdata: "" });
	fb.close();
	file = stored();
	if (file.auxdata !== undefined)
		throw new Error('auxdata: "" did not clear it: ' + JSON.stringify(file.auxdata));
	if (!text_is(file.extdesc, EXTDESC))
		throw new Error('auxdata: "" also cleared the extended description');

	/* readd_always re-adds: the record moves to the end of the index and dates
	   as newly imported, while its stored text comes along. */
	seed();
	fb = open_base();
	var before = fb.get(NAMES[0], FileBase.DETAIL.NORM).added;
	fb.update(NAMES[0], { desc: "first" }, /* use_diz_always: */ false, /* readd_always: */ true);
	fb.close();
	file = stored();
	check("index order after readd_always", index_order().join(), [NAMES[1], NAMES[2], NAMES[0]].join());
	if (!text_is(file.extdesc, EXTDESC))
		throw new Error("readd_always lost the extended description");
	if (!text_is(file.auxdata, AUXDATA))
		throw new Error("readd_always lost the auxdata");
	if (!(file.added > before))
		throw new Error("readd_always did not update the added date: " + before + " -> " + file.added);

	/* A file object carrying no header-field property at all is a partial
	   update, not an error: auxdata alone, or a header value alone. */
	seed();
	fb = open_base();
	if (!fb.update(NAMES[0], { auxdata: '{"only":"auxdata"}' }))
		throw new Error("update with an auxdata-only object failed: " + fb.last_error);
	if (!fb.update(NAMES[1], { cost: 42 }))
		throw new Error("update with a cost-only object failed: " + fb.last_error);
	fb.close();
	file = stored();
	if (!text_is(file.auxdata, '{"only":"auxdata"}'))
		throw new Error("auxdata-only update did not store: " + JSON.stringify(file.auxdata));
	check("desc survived an auxdata-only update", file.desc, "first");
	check("cost-only update", stored(NAMES[1]).cost, 42);

	/* Renaming while the text changes keeps working, and the index follows. */
	seed();
	fb = open_base();
	if (!fb.update(NAMES[0], { name: "tfu9.dat", desc: "first", extdesc: "renamed and rewritten" }))
		throw new Error("rename update failed: " + fb.last_error);
	fb.close();
	fb = open_base();
	if (fb.get("tfu9.dat") === null)
		throw new Error("file not found under its new name");
	if (fb.get(NAMES[0]) !== null)
		throw new Error("file still found under its old name");
	fb.close();

	/* Repeatedly growing and shrinking the stored text must leave every record
	   readable, which is what exercises the data-block allocation. */
	seed();
	for (var pass = 0; pass < 8; pass++) {
		fb = open_base();
		for (i = 0; i < NAMES.length; i++) {
			var pad = "";
			var len = ((pass * 3 + i) % 5) * 400;
			while (pad.length < len)
				pad += "0123456789";
			fb.update(NAMES[i], { desc: "file " + i, extdesc: len ? "pass " + pass + " " + pad : "",
				auxdata: len ? '{"pass":' + pass + '}' : "" });
		}
		fb.close();
	}
	for (i = 0; i < NAMES.length; i++) {
		file = stored(NAMES[i]);
		var expect = ((7 * 3 + i) % 5) * 400;
		if (expect > 0 && (file.extdesc === undefined || file.extdesc.indexOf("pass 7 ") !== 0))
			throw new Error(NAMES[i] + ": stale or missing text after churn: " + JSON.stringify(String(file.extdesc).substr(0, 24)));
		if (expect === 0 && file.extdesc !== undefined)
			throw new Error(NAMES[i] + ": text not cleared by churn");
	}
} finally {
	cleanup();
}
