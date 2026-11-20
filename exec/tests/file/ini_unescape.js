// "key : value" ini values are C string literals (c_unescape_str()); one
// that ends in a lone backslash must end there, not read past the end of
// the string (the escape handling once stepped over the terminator). The
// same applies to system.findstr() on a filter file whose last line ends
// in a backslash. Scratch files below system.temp_dir.

var path = system.temp_dir + format("ini_unescape_%u_%u", time(), random(1000000));
var failures = [];

function check(desc, got, expect)
{
	if (got !== expect)
		failures.push(desc + " = " + JSON.stringify(got) + ", expected " + JSON.stringify(expect));
}

function write_text(name, text)
{
	var f = new File(name);
	if (!f.open("wb"))
		throw new Error("Error " + f.error + " creating " + f.name);
	f.write(text);
	f.close();
}

mkpath(system.temp_dir);
try {
	write_text(path + ".ini", "[s]\nplain : abc\\x41\ntrailing : abc\\\nquoted : \"x\\ty\\\"\nkey\\:value\nlast : end\\");
	var f = new File(path + ".ini");
	if (!f.open("r"))
		throw new Error("Error " + f.error + " opening " + f.name);
	check("escape", f.iniGetValue("s", "plain"), "abcA");
	check("trailing backslash", f.iniGetValue("s", "trailing"), "abc");
	check("trailing backslash in quotes", f.iniGetValue("s", "quoted"), "x\ty");
	// The key is unescaped first, in the same line buffer: reading on past
	// its end would shift the value over by one character.
	check("key ending in a backslash", f.iniGetValue("s", "key"), "value");
	check("trailing backslash at end of file", f.iniGetValue("s", "last"), "end");
	f.close();

	write_text(path + ".can", "example.com\nexa\\");
	check("findstr: entry before", system.findstr(path + ".can", "example.com"), true);
	check("findstr: line ending in a backslash", system.findstr(path + ".can", "exa"), true);
	check("findstr: no other match", system.findstr(path + ".can", "other.test"), false);
}
finally {
	file_remove(path + ".ini");
	file_remove(path + ".can");
}

if (failures.length > 0)
	throw new Error("c_unescape test failures:\n  " + failures.join("\n  "));
