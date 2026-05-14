// Basic FileBase round-trip: open, add, get, get_list, get_size, hash,
// remove, close. Exercises the core API surface against an ad-hoc
// (is_path) file base so the test doesn't touch the live install's
// file areas.
//
// For an is_path FileBase, getfilepath() resolves filenames relative
// to system.temp_dir (see filedat.c:getfilepath when !dirnum_is_valid),
// so the real file we add() must live there.

mkpath(system.temp_dir);

var stem = "test_filebase_basic_" + Date.now();
var fbpath = system.temp_dir + stem;
var FBEXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash", ".ixt", ".ixb"];

/* Short name to avoid the filebase index's filename-normalization
 * (smb_fileidxname truncates long names). The FileBase itself is in a
 * unique temp path, so collisions across concurrent runs are still
 * impossible. */
var FNAME = "tfb.dat";
var FPATH = system.temp_dir + FNAME;
var CONTENT = "FileBase basic test payload\n";

function cleanup() {
	FBEXTENSIONS.forEach(function (ext) {
		try { file_remove(fbpath + ext); } catch (e) { /* ignore */ }
	});
	try { file_remove(FPATH); } catch (e) { /* ignore */ }
}

try {
	/* Create the on-disk file the FileBase will reference. */
	var f = new File(FPATH);
	if (!f.open("w"))
		throw new Error("could not create " + FPATH + ": " + f.error);
	f.write(CONTENT);
	f.close();

	var fb = new FileBase(fbpath, /* is_path: */ true);
	if (!fb.open())
		throw new Error("FileBase open failed: " + fb.last_error);

	var added = fb.add({
		name: FNAME,
		desc: "basic FileBase test",
		from: "Tester",
		cost: CONTENT.length
	});
	if (!added)
		throw new Error("FileBase.add failed: " + fb.last_error);

	var meta = fb.get(FNAME);
	if (meta === null)
		throw new Error("FileBase.get returned null for " + FNAME + ": " + fb.last_error);
	if (meta.name != FNAME)
		throw new Error("get().name = " + JSON.stringify(meta.name) + " (expected " + JSON.stringify(FNAME) + ")");
	if (meta.desc != "basic FileBase test")
		throw new Error("get().desc = " + JSON.stringify(meta.desc));

	var list = fb.get_list();
	if (!list || list.length != 1)
		throw new Error("get_list() length = " + (list ? list.length : "<null>") + " (expected 1)");
	if (list[0].name != FNAME)
		throw new Error("get_list()[0].name = " + JSON.stringify(list[0].name));

	/* Compare to the actual on-disk size, not CONTENT.length: File.write() in
 * text mode does \n -> \r\n on Windows, so the on-disk size differs from
 * the JS string length. */
var actual_size = file_size(FPATH);
var size = fb.get_size(FNAME);
if (size != actual_size)
	throw new Error("get_size = " + size + " (expected " + actual_size + ")");

	var hashes = fb.hash(FNAME);
	if (hashes === null)
		throw new Error("hash() returned null: " + fb.last_error);
	if (typeof hashes.md5 !== "string" || hashes.md5.length != 32)
		throw new Error("hash().md5 not a 32-char string: " + JSON.stringify(hashes.md5));

	if (!fb.remove(FNAME, /* delete: */ true))
		throw new Error("remove() failed: " + fb.last_error);

	if (file_exists(FPATH))
		throw new Error("on-disk file not deleted by remove(name, true)");

	var list2 = fb.get_list();
	if (list2 && list2.length != 0)
		throw new Error("get_list() after remove length = " + list2.length + " (expected 0)");

	fb.close();
} finally {
	cleanup();
}
