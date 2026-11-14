// @-code display modifiers: case, alignment, width, truncation, zero-padding
// and thousands separators. Uses the current user's note and credits as
// known inputs; both are restored before returning.

var failures = [];

function check(code, expect)
{
	var got = bbs.atcode(code);
	if (got !== expect)
		failures.push(format("@%s@ = '%s', expected '%s'", code, got, expect));
}

function pad(str, len, align)
{
	str = str.substr(0, len);
	var fill = len - str.length;
	switch (align) {
		case 'L':
			return str + format("%*s", fill, "");
		case 'R':
			return format("%*s", fill, "") + str;
		case 'C':
			var left = Math.floor(fill / 2);
			return format("%*s", left, "") + str + format("%*s", fill - left, "");
	}
	return str;
}

check("BBS", system.name);
check("SYSOP", system.operator);
check("USER", user.alias);
check("ALIAS", user.alias);
check("USERNUM", String(user.number));

var saved_note = user.note;
var saved_credits = user.security.credits;
try {
	var note = "Hello World";
	user.note = note;
	check("NOTE", note);

	// Case
	check("NOTE|U", note.toUpperCase());
	check("NOTE-U", note.toUpperCase());
	check("NOTE|D", note.toLowerCase());
	check("NOTE-D", note.toLowerCase());

	// Default width is the length of the @-code itself, including the @s
	check("NOTE-L", pad(note, "@NOTE-L@".length, 'L'));
	check("NOTE|L", pad(note, "@NOTE|L@".length, 'L'));
	check("NOTE-R", pad(note, "@NOTE-R@".length, 'R'));

	// Explicit width
	check("NOTE-L20", pad(note, 20, 'L'));
	check("NOTE|L20", pad(note, 20, 'L'));
	check("NOTE-R20", pad(note, 20, 'R'));
	check("NOTE|R20", pad(note, 20, 'R'));
	check("NOTE-C20", pad(note, 20, 'C'));
	check("NOTE|C20", pad(note, 20, 'C'));
	check("NOTE-L5", note.substr(0, 5));
	check("NOTE|UL20", pad(note.toUpperCase(), 20, 'L'));

	// No truncation
	check("NOTE->", note);

	// Zero-padding
	check("USERNUM-Z6", format("%06u", user.number));
	check("USERNUM|Z6", format("%06u", user.number));

	// Thousands separators
	user.security.credits = 1234567;
	check("CREDITS", "1234567");
	check("CREDITS|T", "1,234,567");
	check("CREDITS-T", "1,234,567");
	check("CREDITS:B|T", "1,234,567");
	check("CREDITS|TR12", "   1,234,567");
	user.security.credits = 999;
	check("CREDITS|T", "999");
} finally {
	user.note = saved_note;
	user.security.credits = saved_credits;
}

if (failures.length)
	throw new Error(failures.length + " failed:\r\n" + failures.join("\r\n"));
