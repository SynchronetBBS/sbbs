// Duration format parameters (@CODE:F@, :!, :A, :T, :B, :V, :C, :W, :D, :E,
// :S, :M), using the current user's banked minutes as the known input;
// restored before returning.

var failures = [];

function check(code, expect)
{
	var got = bbs.atcode(code);
	if (got !== expect)
		failures.push(format("@%s@ = '%s', expected '%s'", code, got, expect));
}

var formats = [ 'F', '!', 'A', 'T', 'B', 'V', 'C', 'W', 'D', 'E', 'S', 'M' ];
var tests = {
	0: [ "00:00:00", "0:00:00", "0", "00:00", "0", "0m", "0m"
		, "0 minutes", "0 minutes", "0 seconds", "0", "0" ],
	90: [ "01:30:00", "1:30:00", "1:30:00", "01:30", "1:30", "1h 30m", "1.5h"
		, "1 hour 30 minutes", "1.5 hours", "1 hour 30 minutes", "5400", "90" ],
	6000: [ "100:00:00", "100:00:00", "100:00:00", "100:00", "100:00", "4d 4h 0m", "4.2d"
		, "4 days 4 hours 0 minutes", "4.2 days", "4 days 4 hours 0 minutes", "360000", "6000" ],
};

var saved_minutes = user.security.minutes;
try {
	for (var minutes in tests) {
		user.security.minutes = Number(minutes);
		var expect = tests[minutes];
		for (var i = 0; i < formats.length; i++)
			check("TBANKED:" + formats[i], expect[i]);
		check("TBANKED", expect[formats.indexOf('T')]); // default
		check("MBANKED", expect[formats.indexOf('M')]); // default
	}
} finally {
	user.security.minutes = saved_minutes;
}

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
