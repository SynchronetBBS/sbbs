// Byte-count format parameters (@CODE:B@, :K, :M, :G, :V), using the
// current user's credits as the known input; restored before returning.

var failures = [];

function check(code, expect)
{
	var got = bbs.atcode(code);
	if (got !== expect)
		failures.push(format("@%s@ = '%s', expected '%s'", code, got, expect));
}

var tests = [
	// bytes,       B,             K,          M,         G,       V
	[ 0,            "0",           "0",        "0.0",     "0.00",  "0" ],
	[ 1023,         "1023",        "0",        "0.0",     "0.00",  "1023" ],
	[ 1536,         "1536",        "1",        "0.0",     "0.00",  "1.5K" ],
	[ 1572864,      "1572864",     "1536",     "1.5",     "0.00",  "1.5M" ],
	[ 10737418240,  "10737418240", "10485760", "10240.0", "10.00", "10.0G" ],
	[ 1649267441664, "1649267441664", "1610612736", "1572864.0", "1536.00", "1.5T" ],
];

var saved_credits = user.security.credits;
try {
	for (var i = 0; i < tests.length; i++) {
		var t = tests[i];
		user.security.credits = t[0];
		check("CREDITS", t[1]);  // default is bytes
		check("CREDITS:B", t[1]);
		check("CREDITS:K", t[2]);
		check("CREDITS:M", t[3]);
		check("CREDITS:G", t[4]);
		check("CREDITS:V", t[5]);
	}
} finally {
	user.security.credits = saved_credits;
}

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
