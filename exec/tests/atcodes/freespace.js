// @FREESPACE@ and its unit variants, compared against the free space in
// bytes. Free space changes while the test runs, so allow some slack.

var failures = [];

var bytes = dir_freespace(system.temp_dir, 1);
var slack = 64 * 1024 * 1024;

function check_near(code, unit)
{
	var got = bbs.atcode(code);
	var expect = Math.floor(bytes / unit);
	var tolerance = Math.max(1, Math.ceil(slack / unit));
	if (!/^[0-9]+$/.test(got) || Math.abs(Number(got) - expect) > tolerance)
		failures.push("@" + code + "@ = '" + got + "', expected about " + expect);
}

check_near("FREESPACE", 1);
check_near("FREESPACE:B", 1);
check_near("FREESPACEK", 1024);
check_near("FREESPACE:K", 1024);
check_near("FREESPACEM", 1024 * 1024);
check_near("FREESPACEG", 1024 * 1024 * 1024);
check_near("FREESPACET", 1024 * 1024 * 1024 * 1024);

var verbal = bbs.atcode("FREESPACE:V");
var match = /^([0-9]+\.[0-9])([KMGTP]?)$/.exec(verbal);
if (!match)
	failures.push(format("@FREESPACE:V@ = '%s', not an estimate", verbal));
else {
	var value = Number(match[1]) * Math.pow(1024, " KMGTP".indexOf(match[2] || ' '));
	if (Math.abs(value - bytes) > Math.max(slack, bytes * 0.05))
		failures.push("@FREESPACE:V@ = '" + verbal + "', expected about " + bytes + " bytes");
}

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
