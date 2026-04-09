var args = [ 1, 1.1, true, "one", -1, 1e9, null, undefined ];

var test = {
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
	"%7$s %s":	"null undefined",
	"%8$s %s":	"undefined %s",
	"%9$s":		"%9$s",
};

for (var i in test) {
	var result = format.apply(null, [i].concat(args));
	if (result != test[i])
		throw new Error("format(" + i + ") = '" + result + "' instead of '" + test[i] + "'");
}

// %*s width-from-argument tests (double values must work like ints)
var star_tests = [
	{ fmt: "%*s",  args: [5, "hi"],  expected: "   hi",  desc: "int width" },
	{ fmt: "%*s",  args: [5.0, "hi"],  expected: "   hi",  desc: "double width (whole number)" },
	{ fmt: "%*s",  args: [80 * 0.25 - 1, ""],  expected: format("%19s", ""),  desc: "computed double width" },
	{ fmt: "%*d",  args: [4, 42],  expected: "  42",  desc: "int width with %d" },
	{ fmt: "%*d",  args: [4.0, 42],  expected: "  42",  desc: "double width with %d" },
	{ fmt: "%-*s", args: [5, "hi"],  expected: "hi   ",  desc: "left-aligned int width" },
	{ fmt: "%-*s", args: [5.0, "hi"],  expected: "hi   ",  desc: "left-aligned double width" },
];

for (var i = 0; i < star_tests.length; i++) {
	var t = star_tests[i];
	var result = format.apply(null, [t.fmt].concat(t.args));
	if (result != t.expected)
		throw new Error("format(" + t.desc + ") = '" + result + "' instead of '" + t.expected + "'");
}
