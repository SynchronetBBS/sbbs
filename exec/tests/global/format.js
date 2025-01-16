const args = [ 1, 1.1, true, "one", -1, 1e9, null ];

const test = {
	"%s":		"1",
	"%d":		"1",
	"%i":		"1",
	"%u":		"1",
	"%02d":		"01",
	"%02x":		"01",
	"%02u":		"01",
	"%02i":		"01",
	"%d %.2f":	"1 1.10",
	"%d %d %d":	"1 1 1",
	"%4$s":		"one",
	"%4$.2s":	"on",
	"%4$5s":	"  one",
	"%4$-5s":	"one  ",
	"%5$d":		"-1",
	"%5$x":		"ffffffff",
	"%6$u":		"1000000000",
	"%7$u":		"0",
	"%7$s":		"null",
	"%7$s %s":	"null %s",
	"%8$s":		"%8$s",
};

for (var i in test) {
	var result = format.apply(null, [i].concat(args));
	if (result != test[i])
		throw new Error("format(" + i + ") = '" + result + "' instead of '" + test[i] + "'");
}
