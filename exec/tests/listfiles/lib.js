// listfiles_lib.js: options merging, header/footer menu lookup order,
// filespec widening, module-local @-codes and pagination

load("listfiles_lib.js");

var failures = [];

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

check("listfiles_options", listfiles_options({a:1, b:2}, {b:3}, null, {c:4}), {a:1, b:3, c:4});
check("listfiles_options undefined", listfiles_options({a:1}, undefined, {a:2}, undefined), {a:2});

check("listfiles_menu_candidates"
	, listfiles_menu_candidates("UPLOADS", 3, "head")
	, ["listfiles_uploads_head", "listfiles_lib3_head", "listfiles_head"]);
check("listfiles_menu_candidates tail"
	, listfiles_menu_candidates("main", 1, "tail")
	, ["listfiles_main_tail", "listfiles_lib1_tail", "listfiles_tail"]);

check("widen 12+ with ext", listfiles_widen_filespec("abcdefghijkl.zip"), "abcdefghijkl*.zip");
check("widen 12+ no ext", listfiles_widen_filespec("abcdefghijklmn"), "abcdefghijklmn*");
check("widen short", listfiles_widen_filespec("short.zip"), "short.zip");
check("widen wildcard", listfiles_widen_filespec("abcdefghijkl*.zip"), "abcdefghijkl*.zip");
check("widen empty", listfiles_widen_filespec(""), "");

check("local codes", listfiles_local_codes("[@LETTER@][@SIZE-R7@]", {letter:"C", size:"1.5M"}), "[C][   1.5M]");
check("local -L", listfiles_local_codes("@FLAGS-L3@|", {flags:"+"}), "+  |");
check("local |L", listfiles_local_codes("@TAG|L2@|", {tag:"*"}), "* |");
check("local -C", listfiles_local_codes("@LETTER-C3@", {letter:"A"}), " A ");
check("local truncates", listfiles_local_codes("@SIZE-R3@", {size:"12345"}), "123");
check("local missing", listfiles_local_codes("@MISSING@", {missing:"-"}), "-");
check("local undefined value", listfiles_local_codes("[@TAG@]", {}), "[]");
check("unknown left alone", listfiles_local_codes("@FILE_NAME-L12@ @LETTER@", {letter:"B"}), "@FILE_NAME-L12@ B");

check("local name/desc/attrs"
	, listfiles_local_codes("@NAME_ATTR@@NAME@@LETTER_ATTR@@LETTER@@SIZE_ATTR@@SIZE-R6@@DESC_ATTR@@DESC@"
		, {name:"a.zip", name_attr:"<n>", letter:"A", letter_attr:"<l>", size:"10K", size_attr:"<s>"
		, desc:"An archive", desc_attr:"<d>"})
	, "<n>a.zip<l>A<s>   10K<d>An archive");

check("fname short", listfiles_format_filename("readme.txt", 12), "readme  .txt");
check("fname long", listfiles_format_filename("averyverylongname.zip", 12), "averyver.zip");
check("fname no ext", listfiles_format_filename("noextensionlongname", 12), "noextensionl");
check("fname no ext short", listfiles_format_filename("noext", 12), "noext       ");
check("fname long ext", listfiles_format_filename("x.verylongextension", 12), "x.verylongex");
check("fname exact", listfiles_format_filename("abcdefgh.zip", 12), "abcdefgh.zip");

check("attr 0x07", listfiles_attr_str(0x07), "\x01n\x01w");
check("attr 0x0F", listfiles_attr_str(0x0F), "\x01n\x01w\x01h");
check("attr 0x1E", listfiles_attr_str(0x1E), "\x01n\x01y\x01h\x014");
check("attr 0x8C", listfiles_attr_str(0x8C), "\x01n\x01r\x01h\x01i");

check("paginate", listfiles_paginate([1, 1, 1, 1, 1], 2)
	, [{first:0, count:2}, {first:2, count:2}, {first:4, count:1}]);
check("paginate tall", listfiles_paginate([1, 5, 1], 3)
	, [{first:0, count:1}, {first:1, count:1}, {first:2, count:1}]);
check("paginate mixed", listfiles_paginate([2, 1, 3, 1], 4)
	, [{first:0, count:2}, {first:2, count:2}]);
check("paginate empty", listfiles_paginate([], 10), []);

var d = listfiles_defaults();
var keys = ["line_fmt", "line_fmt_nobatch", "extdesc_marker", "missing_marker"
	, "tagged_marker", "untagged_marker", "box_top_left", "box_top_right"
	, "box_bottom_left", "box_bottom_right", "box_horizontal", "box_vertical"
	, "short_hdr_underline", "selected_item_attr", "lightbar", "more_fmt"];
for (var i = 0; i < keys.length; i++) {
	if (d[keys[i]] === undefined)
		failures.push("listfiles_defaults() lacks " + keys[i]);
}

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
