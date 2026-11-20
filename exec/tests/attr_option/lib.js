// attr_option.js: color options given as attribute letters, as in attr.ini

load(js.exec_dir + "../../load/attr_option.js");

var failures = [];

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

check("letters", attr_option("WH4"), "\x01n\x01W\x01H\x014");
check("lowercase", attr_option("v"), "\x01n\x01v");
check("stops at an unknown letter", attr_option("WHZ4"), "\x01n\x01W\x01H");
check("empty", attr_option(""), "");
check("Ctrl-A codes passed through (as set before letters)", attr_option("\x01n\x01h\x01w\x014"), "\x01n\x01h\x01w\x014");
check("no letters", attr_option("Z"), "");
check("not a string", attr_option(undefined), "");
check("a number (modopts.js converts all-digit values)", attr_option(4), "\x01n\x014");
check("options converted", attr_options({ name_attr: "BH", line_fmt: "WH", other: 1 })
	, { name_attr: "\x01n\x01B\x01H", line_fmt: "WH", other: 1 });

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
