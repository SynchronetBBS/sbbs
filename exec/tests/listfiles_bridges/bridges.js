// bbs.batch_add_file(), bbs.remove_file() and bbs.move_file() against a
// throwaway file base directory with the internal code LISTFILES_TEST (the
// tests are skipped when there's no such directory). bbs.view_file_info()
// is interactive and isn't tested here.

var CODE = "listfiles_test";
var failures = [];

function check(desc, got, expect)
{
	if (got !== expect)
		failures.push(format("%s = '%s', expected '%s'", desc, got, expect));
}

function throws(func)
{
	try {
		func();
	} catch (e) {
		return true;
	}
	return false;
}

var name = format("lft%05u.txt", Date.now() % 100000);
var path = file_area.dir[CODE].path + name;
var file = new File(path);
if (!file.open("w"))
	throw new Error("Can't create " + path);
file.writeln("listfiles bridge test");
file.close();

var fb = new FileBase(CODE);
if (!fb.open())
	throw new Error("Can't open file base " + CODE);
var added = fb.add({ name: name, desc: "listfiles bridge test", cost: 0 });
fb.close();
if (!added)
	throw new Error("Can't add " + name);

try {
	var queued = bbs.batch_dnload_total;
	check("batch_add_file()", bbs.batch_add_file(CODE, name), true);
	check("batch_dnload_total", bbs.batch_dnload_total, queued + 1);
	bbs.batch_remove(false, name);

	check("batch_add_file() nonexistent", bbs.batch_add_file(CODE, "nonexistent.zzz"), false);
	check("move_file() to its own directory", bbs.move_file(CODE, name, CODE), false);

	check("remove_file()", bbs.remove_file(CODE, name, true, false), true);
	fb.open();
	check("still in the file base", fb.get(name) === null, true);
	fb.close();
	check("still on disk", file_exists(path), false);

	check("batch_add_file() bad dir throws", throws(function () { bbs.batch_add_file("no_such_dir", name); }), true);
	check("remove_file() bad dir throws", throws(function () { bbs.remove_file("no_such_dir", name); }), true);
	check("move_file() bad dest throws", throws(function () { bbs.move_file(CODE, name, "no_such_dir"); }), true);
	check("view_file_info() bad dir throws", throws(function () { bbs.view_file_info("no_such_dir", name); }), true);
} finally {
	if (file_exists(path)) {
		fb.open();
		fb.remove(name, /* delete: */ true);
		fb.close();
	}
}

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
