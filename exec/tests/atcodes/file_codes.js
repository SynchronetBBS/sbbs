// @FILE_*@ codes expanded by bbs.expand_atcodes() for a FileBase file
// object and directory code, using the first file of the first directory
// that has any; skipped (passes) when no directory has files.

var failures = [];

function check(desc, got, expect)
{
	if (got !== expect)
		failures.push(format("%s = '%s', expected '%s'", desc, got, expect));
}

function first_file()
{
	for (var code in file_area.dir) {
		var fb = new FileBase(code);
		if (!fb.open())
			continue;
		var list = fb.get_list("*", FileBase.DETAIL.NORM);
		fb.close();
		if (list && list.length)
			return { code: code, file: list[0] };
	}
	return null;
}

var found = first_file();
if (found) {
	var f = found.file;
	var code = found.code;

	check("@FILE_NAME@", bbs.expand_atcodes("@FILE_NAME@", f, code), f.name);
	if (f.name.length < 20)
		check("@FILE_NAME-L20@ length"
			, bbs.expand_atcodes("@FILE_NAME-L20@", f, code).length, 20);
	check("@FILE_DIR_CODE@"
		, bbs.expand_atcodes("@FILE_DIR_CODE@", f, code).toLowerCase(), code.toLowerCase());
	check("@FILE_DESC@", bbs.expand_atcodes("@FILE_DESC@", f, code), f.desc || "");
	check("bbs.file_name after the call", bbs.file_name, "");

	var threw = false;
	try {
		bbs.expand_atcodes("@FILE_NAME@", f, "no_such_dir");
	} catch (e) {
		threw = true;
	}
	check("unknown directory code throws", threw, true);
}

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
