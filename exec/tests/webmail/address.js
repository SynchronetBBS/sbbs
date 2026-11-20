// Address: addresses, sender identity and header safety
// (webmail/lib/address.js)

load(js.exec_dir + "kit.js");
var Address = Kit.wlib("address");

var WIDE = String.fromCharCode(0x100);	// a char code above 0xFF

var SB = {};
load(SB, Kit.exec_root() + "load/sbbsdefs.js");
var UD = {};
load(UD, Kit.exec_root() + "load/userdefs.js");
var SMB = {};
load(SMB, Kit.exec_root() + "load/smbdefs.js");

// No control character, line break or NUL in anything returned
function no_controls(s, what)
{
	Kit.assert(typeof s === "string", what + ": a string");
	// C0, DEL, C1, U+2028/U+2029 and the bidirectional controls U+061C,
	// U+200E, U+200F, U+202A-U+202E, U+2066-U+2069 (as UTF-8)
	Kit.assert(!/[\x00-\x1F\x7F]|\xC2[\x80-\x9F]|\xD8\x9C|\xE2\x80[\x8E\x8F\xA8-\xAE]|\xE2\x81[\xA6-\xA9]/.test(s),
		what + ": no control characters: " + JSON.stringify(s));
}

function code_of(fn)
{
	try {
		fn();
	} catch (e) {
		return e && e.code;
	}
	return null;
}

function reason_of(fn)
{
	try {
		fn();
	} catch (e) {
		return e && e.reason;
	}
	return null;
}

// The constants copied from the stock definition files
Kit.eq(Address.NET_NONE, SMB.NET_NONE, "NET_NONE");
Kit.eq(Address.NET_UNKNOWN, SMB.NET_UNKNOWN, "NET_UNKNOWN");
Kit.eq(Address.NET_FIDO, SMB.NET_FIDO, "NET_FIDO");
Kit.eq(Address.NET_QWK, SMB.NET_QWK, "NET_QWK");
Kit.eq(Address.NET_INTERNET, SMB.NET_INTERNET, "NET_INTERNET");
Kit.eq(Address.NMAIL_ALIAS, SB.NMAIL_ALIAS, "NMAIL_ALIAS");
Kit.eq(Address.MM_REALNAME, SB.MM_REALNAME, "MM_REALNAME");
Kit.eq(Address.U_NAME, SB.U_NAME, "U_NAME");
Kit.eq(Address.UREST_REAL_NAME, UD.UREST_REAL_NAME, "UREST_REAL_NAME");
Kit.eq(Address.USER_DELETED, UD.USER_DELETED, "USER_DELETED");
Kit.eq(Address.USER_INACTIVE, UD.USER_INACTIVE, "USER_INACTIVE");

// ---------------------------------------------------------------------
// The plan's step 1 (domains as amended: the list and the user's
// can_send_from_domain are passed in)

Kit.eq(Address.parse_list('"Doe, Jane" <j@x.org>, bob@y.com; Al'),
	[{ name: "Doe, Jane", addr: "j@x.org" }, { name: "", addr: "bob@y.com" },
		{ name: "", addr: "Al" }]);
Kit.throws(function () {
	Address.parse_list("a@b, <>");
}, "invalid");
var D = Address.domains(["vert.example", "Family.example", "*.wild.example",
	"family.example", "other.example", "!neg.example", ""], function (d) {
	return d !== "other.example";
});
Kit.eq(D, ["vert.example", "family.example"]);
Kit.eq(Address.from_for("rob", "family.example", D), "rob@family.example");
Kit.throws(function () {
	Address.from_for("rob", "evil.example", D);
}, "forbidden");
Kit.eq(Address.reply_domain("family.example", "vert.example", D), "family.example");
Kit.eq(Address.reply_domain("", "vert.example", D), "vert.example");
Kit.assert(Address.is_me("ROB@Family.Example", "rob", D), "is_me");
Kit.eq(Address.clean("Hi\r\nBcc: x@y.z\tthere", 255), "HiBcc: x@y.zthere");
Kit.eq(Address.clean("  a  ", 255), "a");

var rob = {
	number: 7, alias: "rob", name: "Rob S",
	security: { restrictions: 0 }
};
var NOALIAS = { inet_alias: false, fido_alias: false };
var ALIASES = { inet_alias: true, fido_alias: true };
Kit.eq(Address.identity(rob, "internet", { display_name: "The Sysop" }, NOALIAS),
	{ from: "The Sysop", from_ext: "7" });
Kit.eq(Address.identity(rob, "local", { display_name: "The Sysop" }, NOALIAS),
	{ from: "rob", from_ext: "7" });
Kit.eq(Address.identity(rob, "internet", { display_name: "" }, NOALIAS),
	{ from: "Rob S", from_ext: "7" });

// ---------------------------------------------------------------------
// clean()

Kit.eq(Address.clean(undefined, 10), "", "undefined");
Kit.eq(Address.clean(null, 10), "", "null");
Kit.eq(Address.clean(42, 10), "42", "number");
Kit.eq(Address.clean("a\x00b\x07c\x1Fd\x7Fe", 255), "abcde", "C0 and DEL removed");
Kit.eq(Address.clean("a\xC2\x85b\xC2\x9Fc", 255), "abc", "C1 controls (UTF-8) removed");
Kit.eq(Address.clean("a\xE2\x80\xA8b\xE2\x80\xA9c", 255), "abc", "U+2028/U+2029 removed");
Kit.eq(Address.clean("a\xE2\x80\xAEb\xE2\x81\xA6c\xD8\x9Cd", 255), "abcd", "bidi controls removed");
Kit.eq(Address.clean("\r\nSubject\r\n", 255), "Subject", "edges");
// No line break survives in any form
var hostile = "x\r\nBcc: evil@example.org\n\rTo: a@b\x00\x0B\x0C\xC2\x85";
no_controls(Address.clean(hostile, 255), "clean hostile");
Kit.eq(Address.clean(hostile, 255), "xBcc: evil@example.orgTo: a@b", "hostile");
// Byte limits, never splitting a UTF-8 sequence
Kit.eq(Address.clean("abcdef", 3), "abc", "ASCII cut");
Kit.eq(Address.clean("ab\xC3\xA9", 3), "ab", "2-byte char not split");
Kit.eq(Address.clean("ab\xC3\xA9", 4), "ab\xC3\xA9", "2-byte char fits");
Kit.eq(Address.clean("a\xE2\x82\xAC", 3), "a", "3-byte char not split");
Kit.eq(Address.clean("\xF0\x9F\x98\x80\xF0\x9F\x98\x80", 7), "\xF0\x9F\x98\x80", "4-byte char not split");
Kit.eq(Address.clean("ab   cd", 3), "ab", "trailing space after the cut trimmed");
var long_subject = "";
for (var i = 0; i < 300; i++)
	long_subject += "\xC3\xA9";
var c = Address.clean(long_subject, Address.MAX_SUBJECT);
Kit.eq(c.length, 254, "subject 255 bytes: 127 two-byte chars");
// Invalid UTF-8: each maximal ill-formed subsequence becomes U+FFFD
var FFFD = "\xEF\xBF\xBD";
Kit.eq(Address.clean("a\xA0b", 255), "a" + FFFD + "b", "lone 0xA0 byte");
Kit.eq(Address.clean("\xA0a\xA0", 255), FFFD + "a" + FFFD, "0xA0 is not white space");
Kit.eq(Address.clean("a\xC2\xA0b", 255), "a\xC2\xA0b", "NBSP kept (valid UTF-8)");
Kit.eq(Address.clean("\xC2\xA0a\xC2\xA0", 255), "\xC2\xA0a\xC2\xA0", "NBSP not trimmed");
Kit.eq(Address.clean("a\xC3b", 255), "a" + FFFD + "b", "truncated sequence");
Kit.eq(Address.clean("a\xE2\x82b", 255), "a" + FFFD + "b", "truncated 3-byte: one FFFD");
Kit.eq(Address.clean("\xC0\xAF", 255), FFFD + FFFD, "overlong");
Kit.eq(Address.clean("\xED\xA0\x80", 255), FFFD + FFFD + FFFD, "surrogate");
Kit.eq(Address.clean("\xF4\x90\x80\x80", 255), FFFD + FFFD + FFFD + FFFD, "above U+10FFFF");
Kit.eq(Address.clean("\xFF\xFE", 255), FFFD + FFFD, "FF FE");
Kit.eq(Address.clean("a" + WIDE + "b", 255), "a" + FFFD + "b", "a char code above 0xFF");
Kit.eq(Address.clean("\xC3\xA9t\xC3\xA9", 255), "\xC3\xA9t\xC3\xA9", "valid UTF-8 unchanged");
Kit.eq(Address.clean("abc"), "abc", "no limit given");
// Toupper/tolower are not applied: bytes 0xC0-0xDE stay
Kit.eq(Address.clean("\xC3\x80", 255), "\xC3\x80", "no case folding");

// ---------------------------------------------------------------------
// parse_list()

Kit.eq(Address.parse_list(""), [], "empty");
Kit.eq(Address.parse_list(undefined), [], "undefined");
Kit.eq(Address.parse_list(" , ;; ,"), [], "only separators");
Kit.eq(Address.parse_list("a@b.c,"), [{ name: "", addr: "a@b.c" }], "trailing separator");
Kit.eq(Address.parse_list("a@b.c;"), [{ name: "", addr: "a@b.c" }], "trailing semicolon");
Kit.eq(Address.parse_list("a@b.c,,  ,d@e.f"),
	[{ name: "", addr: "a@b.c" }, { name: "", addr: "d@e.f" }], "empty entries skipped");
Kit.eq(Address.parse_list("  Jane   Doe   <j@x.org>  "), [{ name: "Jane Doe", addr: "j@x.org" }],
	"extra white space");
Kit.eq(Address.parse_list("\tJane\tDoe\t<j@x.org>"), [{ name: "Jane Doe", addr: "j@x.org" }],
	"tabs are white space");
Kit.eq(Address.parse_list("<j@x.org>"), [{ name: "", addr: "j@x.org" }], "angle only");
Kit.eq(Address.parse_list("< j@x.org >"), [{ name: "", addr: "j@x.org" }], "spaces inside angles");
Kit.eq(Address.parse_list("j@x.org (Jane Doe)"), [{ name: "Jane Doe", addr: "j@x.org" }], "comment");
Kit.eq(Address.parse_list("(Jane) j@x.org"), [{ name: "Jane", addr: "j@x.org" }], "leading comment");
Kit.eq(Address.parse_list("Jane <j@x.org> (work)"), [{ name: "Jane", addr: "j@x.org" }],
	"a comment does not replace a name");
Kit.eq(Address.parse_list('"Jane \\"JJ\\" Doe" <j@x.org>'), [{ name: 'Jane "JJ" Doe', addr: "j@x.org" }],
	"escaped quotes");
Kit.eq(Address.parse_list('"a\\\\b" <j@x.org>'), [{ name: "a\\b", addr: "j@x.org" }], "escaped backslash");
Kit.eq(Address.parse_list('"Doe; Jane" <j@x.org>; "x,y" <k@x.org>'),
	[{ name: "Doe; Jane", addr: "j@x.org" }, { name: "x,y", addr: "k@x.org" }], "separators in quotes");
Kit.eq(Address.parse_list("Dr. Seuss <s@x.org>"), [{ name: "Dr. Seuss", addr: "s@x.org" }], "dot in name");
Kit.eq(Address.parse_list("Rob Swindell"), [{ name: "", addr: "Rob Swindell" }], "local name");
Kit.eq(Address.parse_list("  Rob   Swindell "), [{ name: "", addr: "Rob Swindell" }], "local name spaces");
Kit.eq(Address.parse_list("1:103/705"), [{ name: "", addr: "1:103/705" }], "bare FidoNet");
Kit.eq(Address.parse_list("1:103/705.1"), [{ name: "", addr: "1:103/705.1" }], "bare FidoNet point");
Kit.eq(Address.parse_list("Rob <rob@1:103/705>"), [{ name: "Rob", addr: "rob@1:103/705" }], "FidoNet");
Kit.eq(Address.parse_list("rob@VERT"), [{ name: "", addr: "rob@VERT" }], "QWK");
Kit.eq(Address.parse_list("rob@VERT/NIX"), [{ name: "", addr: "rob@VERT/NIX" }], "QWK route");
Kit.eq(Address.parse_list("J\xC3\xA9r\xC3\xB4me <j@x.org>"), [{ name: "J\xC3\xA9r\xC3\xB4me", addr: "j@x.org" }],
	"UTF-8 name");
Kit.eq(Address.parse_list("a+tag@x.org, o'brien@x.org"),
	[{ name: "", addr: "a+tag@x.org" }, { name: "", addr: "o'brien@x.org" }], "atext");
// Names are limited to 64 bytes, without splitting a character
var long_name = "";
for (i = 0; i < 40; i++)
	long_name += "\xC3\xA9";
var p = Address.parse_list('"' + long_name + '" <j@x.org>');
Kit.eq(p[0].name.length, 64, "name cut to 64 bytes");
// The plan's example of an invalid entry
Kit.eq(reason_of(function () {
	Address.parse_list("a@b, <>");
}), "syntax", "empty angle");

// Hostile and malformed input: every one refused
var BAD = [
	["a@x.org\r\nBcc: e@evil.org", "control"],
	["\"Jane\r\nBcc: e@evil.org\" <j@x.org>", "control"],
	["Jane\n <j@x.org>", "control"],
	["j@x.org\r", "control"],
	["\r\n", "control"],
	["a\x00b@x.org", "control"],
	["Ja\x7Fne <j@x.org>", "control"],
	["Ja\xC2\x85ne <j@x.org>", "control"],
	["Ja\xE2\x80\xA8ne <j@x.org>", "control"],
	["Ja\xE2\x80\xAEne <j@x.org>", "control"],
	["J\xA0ne <j@x.org>", "utf8"],
	["J\xC3 <j@x.org>", "utf8"],
	["J" + WIDE + " <j@x.org>", "utf8"],
	["j\xC3\xA9@x.org", "syntax"],
	["\"Jane <j@x.org>", "unbalanced"],
	["Jane <j@x.org", "unbalanced"],
	["Jane j@x.org>", "unbalanced"],
	["Jane (x <j@x.org>", "unbalanced"],
	["Jane <<j@x.org>>", "syntax"],
	["Jane <j@x.org> <k@x.org>", "syntax"],
	["Jane <j@x.org> Doe", "syntax"],
	["a@b@x.org", "syntax"],
	["<a@b@x.org>", "syntax"],
	["<@relay.org:j@x.org>", "route"],
	["<@a.org,@b.org:j@x.org>", "route"],
	["friends: a@x.org, b@x.org;", "group"],
	["undisclosed-recipients:;", "group"],
	["Jane: <j@x.org>", "group"],
	["\"a b\"@x.org", "syntax"],
	["a b@x.org", "syntax"],
	["@x.org", "syntax"],
	["a@", "syntax"],
	["j@[192.0.2.1]", "syntax"],
	["Jane \"Doe\" j@x.org", "syntax"],
	["Jane@ <j@x.org>", "syntax"],
	["a\\b <j@x.org>", "syntax"],
	["((nested)) j@x.org", "syntax"],
	["a@b.c (x\r\ny)", "control"]
];
for (i = 0; i < BAD.length; i++) {
	var what = "bad " + i + " " + JSON.stringify(BAD[i][0]);
	Kit.eq(code_of(function () {
		Address.parse_list(BAD[i][0]);
	}), "invalid", what);
	Kit.eq(reason_of(function () {
		Address.parse_list(BAD[i][0]);
	}), BAD[i][1], what + " reason");
}
// The error names the first bad entry (its position, and its text
// without control characters)
try {
	Address.parse_list("a@x.org, good@x.org, bad\r\nBcc: e@evil.org, worse@");
	Kit.assert(false, "should throw");
} catch (e) {
	Kit.eq(e.code, "invalid", "first bad: code");
	Kit.eq(e.index, 2, "first bad: index");
	no_controls(e.message, "error message");
	no_controls(e.entry, "error entry");
	Kit.assert(e.message.indexOf("badBcc") >= 0, "message names the entry: " + e.message);
}
// Address length: 254 bytes in all, 64 in the local part; refused, not cut
var local64 = "";
for (i = 0; i < 64; i++)
	local64 += "a";
var host = "";
while (host.length < 240)
	host += "abcdefghi.";
host += "org";
Kit.eq(Address.parse_list(local64 + "@x.org")[0].addr, local64 + "@x.org", "local part 64");
Kit.eq(reason_of(function () {
	Address.parse_list(local64 + "a@x.org");
}), "toolong", "local part 65");
var addr254 = local64 + "@" + host.substr(0, 254 - 65);
Kit.eq(addr254.length, 254, "addr254 is 254");
Kit.eq(Address.parse_list(addr254)[0].addr, addr254, "254 accepted");
Kit.eq(reason_of(function () {
	Address.parse_list(addr254 + "x");
}), "toolong", "255 refused");
var name254 = "";
while (name254.length < 255)
	name254 += "Bob ";
Kit.eq(reason_of(function () {
	Address.parse_list(name254);
}), "toolong", "local name over 254");
// Entry count and input length
var many = [];
for (i = 0; i < Address.MAX_ENTRIES; i++)
	many.push("u" + i + "@x.org");
Kit.eq(Address.parse_list(many.join(",")).length, Address.MAX_ENTRIES, "100 entries");
many.push("one@more.org");
Kit.eq(reason_of(function () {
	Address.parse_list(many.join(","));
}), "toomany", "101 entries");
var huge = "";
while (huge.length <= Address.MAX_INPUT)
	huge += ",,,,,,,,,,";
Kit.eq(reason_of(function () {
	Address.parse_list(huge);
}), "toolong", "input over the limit");

// ---------------------------------------------------------------------
// format() and format_list()

Kit.eq(Address.format("", "a@b.c"), "a@b.c", "no name");
Kit.eq(Address.format(undefined, "a@b.c"), "a@b.c", "undefined name");
Kit.eq(Address.format("Jane Doe", "j@x.org"), "Jane Doe <j@x.org>", "atext name");
Kit.eq(Address.format('Jane "JJ" Doe', "j@x.org"), '"Jane \\"JJ\\" Doe" <j@x.org>', "quotes escaped");
Kit.eq(Address.format("a\\b", "j@x.org"), '"a\\\\b" <j@x.org>', "backslash escaped");
Kit.eq(Address.format("Doe, Jane", "j@x.org"), '"Doe, Jane" <j@x.org>', "comma quoted");
Kit.eq(Address.format("Dr. Seuss", "s@x.org"), '"Dr. Seuss" <s@x.org>', "dot quoted");
Kit.eq(Address.format("<x@evil.org>", "j@x.org"), '"<x@evil.org>" <j@x.org>', "angles quoted");
Kit.eq(Address.format("J\xC3\xA9r\xC3\xB4me", "j@x.org"),
	"=?UTF-8?B?" + base64_encode("J\xC3\xA9r\xC3\xB4me") + "?= <j@x.org>", "encoded word");
Kit.eq(Address.format("Jane\r\nBcc: e@evil.org", "j@x.org"), '"JaneBcc: e@evil.org" <j@x.org>',
	"line break removed from a name");
var f = Address.format(long_name + long_name, "j@x.org");
no_controls(f, "long encoded name");
var words = f.substr(0, f.lastIndexOf(" <")).split(" ");
Kit.assert(words.length > 1, "several encoded words");
var decoded = "";
for (i = 0; i < words.length; i++) {
	Kit.assert(words[i].length <= 75, "encoded word at most 75 characters");
	var m = /^=\?UTF-8\?B\?([A-Za-z0-9+\/=]+)\?=$/.exec(words[i]);
	Kit.assert(m !== null, "encoded word form: " + words[i]);
	var part = base64_decode(m[1]);
	Kit.eq(Address.clean(part), part, "each word is whole UTF-8");
	decoded += part;
}
Kit.eq(decoded, long_name.substr(0, 64), "decoded name is the name cut to 64 bytes");
Kit.eq(code_of(function () {
	Address.format("Jane", "j@x.org\r\nBcc: e@evil.org");
}), "invalid", "address with a line break");
Kit.eq(code_of(function () {
	Address.format("Jane", "a b@x.org");
}), "invalid", "address with a space");
Kit.eq(code_of(function () {
	Address.format("Jane", "Rob Swindell");
}), "invalid", "not an address");
Kit.eq(code_of(function () {
	Address.format("Jane", "a@b@x.org");
}), "invalid", "two @");
Kit.eq(Address.format_list([{ name: "Jane", addr: "j@x.org" }, { name: "", addr: "k@x.org" },
	{ name: "Doe, J", addr: "d@x.org" }]), 'Jane <j@x.org>, k@x.org, "Doe, J" <d@x.org>', "format_list");
Kit.eq(Address.format_list([]), "", "format_list empty");

// ---------------------------------------------------------------------
// resolve() and system_resolvers(), with a fake system

var LABEL64 = "";
for (i = 0; i < 64; i++)
	LABEL64 += "x";

var FAKE_USERS = {
	1: { alias: "Rob", name: "Rob Swindell" },
	7: { alias: "jane", name: "Jane Doe" },
	5: { alias: "vertbob", name: "Vert Bob" },
	6: { alias: "vert", name: "Vert Sysop" },
	8: { alias: "", name: "Gone User", settings: UD.USER_DELETED },	// name.idx slot blanked
	9: { alias: "Al", name: "" },
	10: { alias: "idle", name: "Idle User", settings: UD.USER_INACTIVE }
};
// new User(n) for the fake system: throws for a number with no record
function FakeRecord(n)
{
	if (!FAKE_USERS[n])
		throw new Error("Invalid user number: " + n);
	this.number = n;
	this.settings = FAKE_USERS[n].settings || 0;
}
var fake_msg_area = { settings: 0, inet_netmail_settings: 0, fido_netmail_settings: 0 };
function alnum(s)
{
	return String(s).replace(/[^A-Za-z0-9]/g, "").toLowerCase();
}
var fake_system = {
	lastuser: 10,
	operator: "The Op",
	qwk_id: "MYBBS",
	inet_domain_list: ["mybbs.com", "Family.example", "*.wild.example"],
	calls: [],
	username: function (n) {
		if (n < 1 || n > 10)
			return "";
		if (!FAKE_USERS[n] || FAKE_USERS[n].alias === "")
			return "DELETED USER";
		return FAKE_USERS[n].alias;
	},
	matchuser: function (name, sysop_alias) {
		this.calls.push(["matchuser", name, sysop_alias]);
		var n = String(name);
		if (sysop_alias !== false && /^(sysop|postmaster|mybbs)$/i.test(n))
			return 1;
		n = n.split("@")[0];
		for (var u in FAKE_USERS)
			if (FAKE_USERS[u].alias !== "" && alnum(FAKE_USERS[u].alias) === alnum(n))
				return Number(u);
		return 0;
	},
	matchuserdata: function (field, data, match_del) {
		this.calls.push(["matchuserdata", field, data, match_del]);
		for (var u in FAKE_USERS)
			if (FAKE_USERS[u].alias !== "" && FAKE_USERS[u].name.toLowerCase() === String(data).toLowerCase())
				return Number(u);
		return 0;
	},
	alias: function (s) {
		var map = { info: "jane", fwd: "x@elsewhere.org", num: "7",
			"jane@family.example": "rob@outside.org", "janey": "jane" };
		return map.hasOwnProperty(s) ? map[s] : s;
	},
	findstr: function (list, host) {
		for (var i = 0; i < list.length; i++)
			if (list[i].toLowerCase() === String(host).toLowerCase())
				return true;
		return false;
	}
};
var R = Address.system_resolvers({ system: fake_system, msg_area: fake_msg_area, User: FakeRecord });

// The terminal's rule for a local name or number
Kit.eq(R.matchuser("jane"), 7, "alias");
Kit.eq(R.matchuser("JANE"), 7, "alias, case");
Kit.eq(R.matchuser("7"), 7, "number");
Kit.eq(R.matchuser("07"), 7, "number, leading zero");
Kit.eq(R.matchuser("8"), 0, "number of a deleted user");
Kit.eq(R.matchuser("11"), 0, "number past the last user");
Kit.eq(R.matchuser("0"), 0, "number 0");
Kit.eq(R.matchuser("#7"), 0, "#7 is not a number");
Kit.eq(R.matchuser("7abc"), 0, "7abc is not a number");
Kit.eq(R.matchuser("sysop"), 1, "sysop");
Kit.eq(R.matchuser("Postmaster"), 1, "postmaster");
Kit.eq(R.matchuser("MYBBS"), 1, "QWK id");
Kit.eq(R.matchuser("Gone User"), 0, "deleted user's name");
Kit.eq(R.matchuser("idle"), 0, "inactive user (the terminal says Unknown user)");
Kit.eq(R.matchuser("10"), 0, "inactive user by number");
Kit.eq(R.matchuser("vert"), 6, "alias vert");
Kit.eq(R.matchuser("Jane Doe"), 0, "real name, MM_REALNAME off");
fake_msg_area.settings = SB.MM_REALNAME;
Kit.eq(R.matchuser("Jane Doe"), 7, "real name, MM_REALNAME on");
Kit.eq(R.matchuser("Gone User"), 0, "deleted user's real name");
fake_msg_area.settings = 0;
Kit.eq(R.matchuser(""), 0, "empty");
Kit.eq(R.own.username(7), "jane", "username");
Kit.eq(R.own.username(8), "", "username of a deleted user");
// A sysop alias (or anything) matched to a user number with no user there
var NO_SYSOP = {};
for (var key in fake_system)
	NO_SYSOP[key] = fake_system[key];
NO_SYSOP.username = function (n) {
	return n === 1 ? "" : fake_system.username(n);
};
var R2 = Address.system_resolvers({ system: NO_SYSOP, msg_area: fake_msg_area, User: FakeRecord });
Kit.eq(R2.matchuser("sysop"), 0, "sysop alias, no user 1");
Kit.eq(R2.matchuser("1"), 0, "number 1, no user 1");
Kit.eq(code_of(function () {
	Address.resolve({ name: "", addr: "1" }, R2.matchuser, netaddr_type, R2.own);
}), "invalid", "1 with no user 1 is not a FidoNet node");

// The mail server's rule for the local part at one of the system's domains
Kit.assert(R.own.is_own_domain("MYBBS.COM"), "own domain");
Kit.assert(R.own.is_own_domain("family.example"), "own additional domain");
Kit.assert(!R.own.is_own_domain("evil.example"), "foreign domain");
function lu(local, host)
{
	return R.own.local_user(local, local + "@" + (host || "mybbs.com"));
}
Kit.eq(lu("jane"), 7, "local part: alias");
Kit.eq(lu("JANE"), 7, "local part: alias, case");
Kit.eq(lu("Jane#news"), 0, "local part: a tag is left to the mail server");
Kit.eq(lu("VERT!bob"), 0, "local part: bang route is left to the mail server");
Kit.eq(lu("vert!bob"), 0, "local part: bang route, lower case");
Kit.eq(lu(".jane"), 0, "local part: leading non-alphanumeric");
Kit.eq(lu("_jane"), 0, "local part: leading underscore");
Kit.eq(lu("info"), 0, "local part: alias.cfg maps it");
Kit.eq(lu("janey"), 0, "local part: alias.cfg maps it, even to a user");
Kit.eq(lu("num"), 0, "local part: alias.cfg to a number");
Kit.eq(lu("7"), 0, "local part: a number");
Kit.eq(lu("fwd"), 0, "local part: alias.cfg off-system");
Kit.eq(lu("jane", "family.example"), 0, "full address mapped by alias.cfg");
Kit.eq(lu("jane", "Family.example"), 7, "full-address alias matches as the server's table does (fake: exact)");
Kit.eq(lu("sysop"), 0, "local part: sysop alias");
Kit.eq(lu("PostMaster"), 0, "local part: postmaster");
Kit.eq(lu("mybbs"), 0, "local part: QWK id");
Kit.eq(lu("idle"), 0, "local part: inactive user");
Kit.eq(lu("The.Op"), 1, "local part: sysop name");
Kit.eq(lu("the.op"), 1, "local part: sysop name, case");
Kit.eq(lu("jane.doe"), 0, "local part: real name, MM_REALNAME off");
fake_msg_area.settings = SB.MM_REALNAME;
Kit.eq(lu("jane.doe"), 7, "local part: real name, MM_REALNAME on");
fake_msg_area.settings = 0;
Kit.eq(R2.own.local_user("The.Op", "The.Op@mybbs.com"), 0, "local part: sysop name, no user 1");
Kit.eq(R.own.local_user(""), 0, "local part: empty");

function resolve(addr, name, own)
{
	return Address.resolve({ name: name || "", addr: addr }, R.matchuser, netaddr_type,
		own === undefined ? R.own : own);
}
Kit.eq(resolve("jane"), { kind: "local", usernum: 7, name: "jane" }, "local by alias");
Kit.eq(resolve("Jane", "Someone Else"), { kind: "local", usernum: 7, name: "jane" },
	"a local recipient's name is the system's");
Kit.eq(resolve("7"), { kind: "local", usernum: 7, name: "jane" }, "local by number");
Kit.eq(resolve("sysop"), { kind: "local", usernum: 1, name: "Rob" }, "local sysop");
Kit.eq(resolve("jane@MyBBS.com"), { kind: "local", usernum: 7, name: "jane" }, "own domain: local");
Kit.eq(resolve("jane@Family.example"), { kind: "local", usernum: 7, name: "jane" },
	"own additional domain");
Kit.eq(resolve("info@family.example"),
	{ kind: "internet", addr: "info@family.example", name: "", net_type: SMB.NET_INTERNET },
	"own domain, alias.cfg: left to the mail server");
Kit.eq(resolve("VERT!bob@mybbs.com").kind, "internet", "VERT!bob is never user vertbob");
Kit.eq(resolve("vertbob@mybbs.com").usernum, 5, "vertbob");
Kit.eq(resolve("nobody@mybbs.com"),
	{ kind: "internet", addr: "nobody@mybbs.com", name: "", net_type: SMB.NET_INTERNET },
	"own domain, no such user: left to the mail server");
Kit.eq(resolve("jane@mybbs.com", "", null),
	{ kind: "internet", addr: "jane@mybbs.com", name: "", net_type: SMB.NET_INTERNET },
	"without own: Internet");
Kit.eq(resolve("jane@other.org", "J"),
	{ kind: "internet", addr: "jane@other.org", name: "J", net_type: SMB.NET_INTERNET }, "Internet");
Kit.eq(resolve("rob@1:103/705", "Rob"),
	{ kind: "fido", addr: "rob@1:103/705", name: "Rob", net_type: SMB.NET_FIDO }, "FidoNet");
Kit.eq(resolve("1:103/705"),
	{ kind: "fido", addr: "1:103/705", name: "", net_type: SMB.NET_FIDO }, "bare FidoNet");
Kit.eq(resolve("rob@VERT"), { kind: "qwk", addr: "rob@VERT", name: "", net_type: SMB.NET_QWK }, "QWK");
Kit.eq(resolve("rob@1:103/705.1").kind, "fido", "FidoNet point");
Kit.eq(resolve("rob@01:103/0705.0").addr, "rob@1:103/705", "FidoNet address in the stored form");
Kit.eq(resolve("1:103/705.0").addr, "1:103/705", "bare FidoNet address in the stored form");
Kit.eq(resolve("a@" + LABEL64.substr(1) + ".example").kind, "internet", "63-byte label");
Kit.eq(resolve("a@x-y.example").kind, "internet", "hyphen inside a label");
var bad_resolve = [
	["nobody", "unknown"],
	["Gone User", "unknown"],
	["a@.c", "unknown"],
	["", "syntax"],
	["a@b.c\r\nBcc: e@evil.org", "control"],
	["a@b@c.org", "syntax"],
	["a b@c.org", "syntax"],
	["idle", "unknown"],
	["10", "unknown"],
	["705", "unknown"],
	["2", "unknown"],
	["99999", "unknown"],
	["103/705", "unknown"],
	["705.0", "unknown"],
	["1:103", "syntax"],
	["rob@705", "unknown"],
	["rob@103/705", "unknown"],
	["rob@1.2", "unknown"],
	["a@127.0.0.1:6379", "unknown"],
	["a@host:25", "unknown"],
	["a@host.example:25", "unknown"],
	["a@host/x", "unknown"],
	["a@host.example/x", "unknown"],
	["a@[1.2.3.4]", "syntax"],
	["a@localhost", "unknown"],
	["a@127.0.0.1", "unknown"],
	["a@10.1.2.3", "unknown"],
	["a@-x.example", "unknown"],
	["a@x-.example", "unknown"],
	["a@x..example", "unknown"],
	["a@x.example.", "unknown"],
	// The last label must begin with a letter (no numeric-looking hosts)
	["a@0x7f.1", "unknown"],
	["a@example.123", "unknown"],
	["a@x.1a", "unknown"],
	["a@x.-a", "unknown"],
	["a@" + LABEL64 + ".example", "unknown"],
	["rob@TOOLONGID", "unknown"],
	["rob@1VERT", "unknown"],
	["rob@VERT/NIX", "unknown"]
];
for (i = 0; i < bad_resolve.length; i++) {
	(function (a) {
		Kit.eq(code_of(function () {
			resolve(a[0]);
		}), "invalid", "resolve " + JSON.stringify(a[0]));
		Kit.eq(reason_of(function () {
			resolve(a[0]);
		}), a[1], "resolve reason " + JSON.stringify(a[0]));
	})(bad_resolve[i]);
}
Kit.eq(code_of(function () {
	Address.resolve(null, R.matchuser, netaddr_type, R.own);
}), "invalid", "resolve null");
// Top-level labels that begin with a letter, incl. an IDN ("xn--") one
["a@b.c", "a@x.co", "a@x.xn--p1ai", "a@x.c0m"].forEach(function (addr) {
	Kit.eq(resolve(addr).kind, "internet", "resolve " + addr);
	Kit.eq(Address.format("", addr), addr, "format " + addr);
});
["a@0x7f.1", "a@example.123"].forEach(function (addr) {
	Kit.eq(code_of(function () { Address.format("", addr); }), "invalid", "format " + addr);
});
// Bidirectional controls are removed from names (like other controls)
Kit.eq(Address.clean("a\xE2\x80\xAEb\xD8\x9Cc\xE2\x81\xA6d\xE2\x80\x8Fe", 255), "abcde", "bidi controls removed");
no_controls(Address.format("x\xE2\x80\xAEy", "a@b.org"), "format name with a bidi override");
// A network type the web client does not send to
Kit.eq(code_of(function () {
	Address.resolve({ name: "", addr: "a@b.org" }, R.matchuser, function () {
		return 3;
	});
}), "invalid", "other net type");
// Every parsed entry resolves or throws invalid; nothing else escapes
var mixed = Address.parse_list('jane, "X" <x@other.org>, 1:103/705, rob@VERT');
var kinds = [];
for (i = 0; i < mixed.length; i++)
	kinds.push(Address.resolve(mixed[i], R.matchuser, netaddr_type, R.own).kind);
Kit.eq(kinds, ["local", "internet", "fido", "qwk"], "parsed then resolved");

// ---------------------------------------------------------------------
// domains_for(): the real objects, here faked

function FakeUser(n)
{
	if (n > 3)
		throw new Error("Invalid user number: " + n);
	this.number = n;
	this.can_send_from_domain = function (d) {
		if (n === 3)
			throw new Error("broken");
		return n === 1 || d === "mybbs.com";
	};
}
var fake_env = { system: fake_system, User: FakeUser };	// not "env": a jsexec global
Kit.eq(Address.domains_for(1, fake_env), ["mybbs.com", "family.example"], "domains_for sysop");
Kit.eq(Address.domains_for(2, fake_env), ["mybbs.com"], "domains_for user");
Kit.eq(Address.domains_for(3, fake_env), [], "domains_for: a throwing method");
Kit.eq(Address.domains_for(99, fake_env), [], "domains_for: past the last user");
Kit.eq(Address.domains_for(0, fake_env), [], "domains_for 0");
Kit.eq(Address.domains_for(-1, fake_env), [], "domains_for -1");
Kit.eq(Address.domains_for(1.5, fake_env), [], "domains_for 1.5");
Kit.eq(Address.domains_for("1", fake_env), [], "domains_for a string");
Kit.eq(Address.domains_for(null, fake_env), [], "domains_for null");

// ---------------------------------------------------------------------
// alias_table(), own.deliver_user(), addresses_for(), from_pick(): the
// addresses the mail server delivers to a user are the ones the user may
// send from

var long_name = "";
for (i = 0; i < 130; i++)
	long_name += "n";
var tbl = Address.alias_table("; a comment\r\n\r\n  rob\t1  \r\nnovalue\r\nnovalue2   \r\n;x y\r\n"
	+ "*@old.example   *@new.example\n" + long_name + " 5\nlast  Jane Doe");
Kit.eq(tbl.slice(0, 2), [{ name: "rob", value: "1" }, { name: "*@old.example", value: "*@new.example" }],
	"alias_table: comments, lines without a value, a pattern");
// a line over 127 bytes is read in pieces, as the mail server reads it
Kit.eq(tbl[2], { name: long_name.substr(127), value: "5" }, "alias_table: the tail of a long line");
Kit.eq(tbl[3], { name: "last", value: "Jane Doe" }, "alias_table: no final line break, a value with a space");
Kit.eq(Address.alias_table(""), [], "alias_table: empty");
Kit.eq(Address.alias_table(undefined), [], "alias_table: undefined");

var ALIAS_TEXT = "; test aliases\n"
	+ "rob        1\n"
	+ "info       jane\n"
	+ "jane@family.example   rob@outside.org\n"
	+ "janey      jane\n"
	+ "fwd        x@elsewhere.org\n"
	+ "news       sub:news\n"
	+ "*@old.example   1\n"
	+ "boss@family.example  Rob\n"
	+ "rs@mybbs.com   Rob Swindell\n"
	+ "num        7\n"
	+ "moved@mybbs.com   jane@mybbs.com\n"
	+ "nobody     ghost\n";
function MailUser(n)
{
	if (!FAKE_USERS[n])
		throw new Error("Invalid user number: " + n);
	this.number = n;
	this.settings = FAKE_USERS[n].settings || 0;
	this.name = FAKE_USERS[n].name;
	this.email = FAKE_USERS[n].alias + "@mybbs.com";
	this.netmail = FAKE_USERS[n].netmail || "";
	this.can_send_from_domain = function (d) {
		return n === 1 || d === "mybbs.com";
	};
}
var real_area = { settings: Address.MM_REALNAME, inet_netmail_settings: 0, fido_netmail_settings: 0 };
var mail_env = { system: fake_system, msg_area: real_area, User: MailUser, alias_text: ALIAS_TEXT };
var RD = Address.system_resolvers(mail_env).own;
Kit.eq(RD.deliver_user("jane@mybbs.com"), 7, "deliver: an alias");
Kit.eq(RD.deliver_user("Jane.Doe@mybbs.com"), 7, "deliver: a real name, dotted");
Kit.eq(RD.deliver_user("rob@mybbs.com"), 1, "deliver: alias.cfg gives a user number");
Kit.eq(RD.deliver_user("num@mybbs.com"), 7, "deliver: another number");
Kit.eq(RD.deliver_user("7@mybbs.com"), 0, "deliver: a number that no alias gave is a name");
Kit.eq(RD.deliver_user("info@mybbs.com"), 7, "deliver: alias.cfg gives an alias");
Kit.eq(RD.deliver_user("INFO@mybbs.com"), 7, "deliver: alias.cfg names ignore case");
Kit.eq(RD.deliver_user("boss@family.example"), 1, "deliver: a whole-address entry");
Kit.eq(RD.deliver_user("boss@mybbs.com"), 0, "deliver: that entry is for its domain only");
Kit.eq(RD.deliver_user("rs@mybbs.com"), 1, "deliver: alias.cfg gives a real name");
Kit.eq(RD.deliver_user("moved@mybbs.com"), 7, "deliver: alias.cfg gives an address here");
Kit.eq(RD.deliver_user("jane@family.example"), 0, "deliver: forwarded to another system");
Kit.eq(RD.deliver_user("fwd@mybbs.com"), 0, "deliver: a name forwarded elsewhere");
Kit.eq(RD.deliver_user("news@mybbs.com"), 0, "deliver: a sub-board");
Kit.eq(RD.deliver_user("x@old.example"), 1, "deliver: a pattern entry, as the server reads it");
Kit.eq(RD.deliver_user("jane@other.example"), 0, "deliver: a domain that is not the system's");
Kit.eq(RD.deliver_user("nobody@mybbs.com"), 0, "deliver: an alias to no user");
Kit.eq(RD.deliver_user("sysop@mybbs.com"), 0, "deliver: a sysop alias is not certain");
Kit.eq(RD.deliver_user("qwk!jane@mybbs.com"), 0, "deliver: QWK routing");
Kit.eq(RD.deliver_user("jane#tag@mybbs.com"), 7, "deliver: a tag goes to the user of the name");
Kit.eq(RD.deliver_user("Jane.Doe#shop@mybbs.com"), 7, "deliver: a tag on a real name");
Kit.eq(RD.deliver_user("info#tag@mybbs.com"), 0, "deliver: no tag on an alias.cfg name");
Kit.eq(RD.deliver_user("The.Op#tag@mybbs.com"), 0, "deliver: no tag on the sysop's name");
Kit.eq(RD.deliver_user("#tag@mybbs.com"), 0, "deliver: a tag alone");
// a tag the user blocked is refused by the server
var blocking = { lastuser: 10, operator: "", qwk_id: "MYBBS", inet_domain_list: fake_system.inet_domain_list,
	data_dir: "/data/", username: fake_system.username, calls: [],
	matchuser: fake_system.matchuser, matchuserdata: fake_system.matchuserdata,
	findstr: function (list, text) {
		if (typeof list === "string")
			return list === "/data/user/0007.smtpblock" && text === "spam";
		return fake_system.findstr(list, text);
	} };
var RB = Address.system_resolvers({ system: blocking, msg_area: real_area, User: MailUser, alias_text: "" }).own;
Kit.eq(RB.deliver_user("jane#shop@mybbs.com"), 7, "deliver: a tag that is not blocked");
Kit.eq(RB.deliver_user("jane#spam@mybbs.com"), 0, "deliver: a blocked tag");
Kit.eq(RD.deliver_user(".jane@mybbs.com"), 0, "deliver: not starting with a letter or digit");
Kit.eq(RD.deliver_user("idle@mybbs.com"), 0, "deliver: an inactive user");
Kit.eq(RD.deliver_user("jane"), 0, "deliver: no domain");
Kit.eq(RD.deliver_user(undefined), 0, "deliver: undefined");

// addresses_for(): the alias, the user's own Internet address when the
// server delivers it to them, the real name; never the alias.cfg names
FAKE_USERS[1].netmail = "boss@family.example";
FAKE_USERS[7].netmail = "rob@mybbs.com";		// someone else's
FAKE_USERS[9].netmail = "al@elsewhere.org";	// not here
Kit.eq(Address.addresses_for(1, mail_env), ["Rob@mybbs.com", "Rob@family.example", "boss@family.example",
	"Rob.Swindell@mybbs.com", "Rob.Swindell@family.example"],
	"addresses_for: the alias, the Internet address, the real name");
Kit.eq(Address.addresses_for(7, mail_env), ["jane@mybbs.com", "Jane.Doe@mybbs.com"],
	"addresses_for: an Internet address that is another user's is not offered");
var no_real = { system: fake_system, msg_area: fake_msg_area, User: MailUser, alias_text: ALIAS_TEXT };
Kit.eq(Address.addresses_for(1, no_real), ["Rob@mybbs.com", "Rob@family.example", "boss@family.example"],
	"addresses_for: no real names when the system takes no mail for them");
Kit.eq(Address.addresses_for(9, mail_env), ["Al@mybbs.com"], "addresses_for: a blank real name, an outside address");
Kit.eq(Address.addresses_for(7, { system: fake_system, msg_area: real_area, User: MailUser, alias_text: ALIAS_TEXT,
	domains: [] }), [], "addresses_for: no domain, no address");
Kit.eq(Address.addresses_for(99, mail_env), [], "addresses_for: past the last user");
Kit.eq(Address.addresses_for("1", mail_env), [], "addresses_for: a string");
FAKE_USERS[7].netmail = "";

// own_address(): any address the server delivers to the user, at a domain
// the user may send from
Kit.eq(Address.own_address(7, "info@MYBBS.com", mail_env), "info@mybbs.com", "own_address: an alias.cfg name");
Kit.eq(Address.own_address(7, "jane#shop@mybbs.com", mail_env), "jane#shop@mybbs.com", "own_address: a tag");
Kit.eq(Address.own_address(7, "Jane.Doe@mybbs.com", mail_env), "Jane.Doe@mybbs.com", "own_address: the real name");
Kit.eq(Address.own_address(7, "rob@mybbs.com", mail_env), null, "own_address: another user's");
Kit.eq(Address.own_address(7, "jane@family.example", mail_env), null, "own_address: a domain the user may not use");
Kit.eq(Address.own_address(1, "boss@family.example", mail_env), "boss@family.example", "own_address: a whole-address entry");
Kit.eq(Address.own_address(1, "x@old.example", mail_env), null, "own_address: a domain that is not the system's");
Kit.eq(Address.own_address(7, "qwk!jane@mybbs.com", mail_env), null, "own_address: QWK routing");
Kit.eq(Address.own_address(7, "jane", mail_env), null, "own_address: no domain");
Kit.eq(Address.own_address(99, "jane@mybbs.com", mail_env), null, "own_address: past the last user");
Kit.eq(Address.own_address("7", "jane@mybbs.com", mail_env), null, "own_address: a string for the number");
Kit.eq(Address.own_address(7, undefined, mail_env), null, "own_address: undefined");

var MINE = ["Rob@mybbs.com", "Rob@family.example", "boss@family.example", "rs@mybbs.com"];
Kit.eq(Address.from_pick("", "", MINE), "Rob@mybbs.com", "from_pick: the default");
Kit.eq(Address.from_pick("", "FAMILY.example", MINE), "Rob@family.example", "from_pick: default name, a domain");
Kit.eq(Address.from_pick("BOSS", "family.example", MINE), "boss@family.example", "from_pick: a name and a domain");
Kit.eq(Address.from_pick("rs", "", MINE), "rs@mybbs.com", "from_pick: a name at the default domain");
Kit.eq(Address.from_pick("boss", "", MINE), null, "from_pick: a name that is not at that domain");
Kit.eq(Address.from_pick("eve", "family.example", MINE), null, "from_pick: not the user's");
Kit.eq(Address.from_pick("", "", []), null, "from_pick: no addresses");
Kit.eq(Address.from_pick("", "", undefined), null, "from_pick: no list");
// not in the list: the address itself when the check allows it
var asked = [];
var allow = function (a) {
	asked.push(a);
	return a === "rob#shop@family.example";
};
Kit.eq(Address.from_pick("rob#shop", "Family.Example", MINE, allow), "rob#shop@family.example", "from_pick: allowed by the check");
Kit.eq(Address.from_pick("eve", "", MINE, allow), null, "from_pick: refused by the check");
Kit.eq(asked, ["rob#shop@family.example", "eve@mybbs.com"], "from_pick: the check gets the whole address");
Kit.eq(Address.from_pick("boss", "family.example", MINE, function () {
	throw new Error("not asked");
}), "boss@family.example", "from_pick: an entry of the list needs no check");
Kit.eq(Address.domains(undefined, function () {
	return true;
}), [], "domains: no list");
Kit.eq(Address.domains(["a.org", "b\r\n.org", "c.org"], function () {
	return true;
}), ["a.org", "c.org"], "domains: an entry with a line break is not a host name");
Kit.eq(Address.domains(["a.org", "b.org"], function (d) {
	if (d === "a.org")
		throw new Error("x");
	return true;
}), ["b.org"], "domains: a throwing test counts as no");

// ---------------------------------------------------------------------
// from_for(), reply_domain(), is_me()

Kit.eq(Address.from_for("Rob.Swindell", "vert.example", D), "Rob.Swindell@vert.example", "dotted");
Kit.eq(Address.from_for("rob", "VERT.Example", D), "rob@vert.example", "domain case");
Kit.eq(Address.from_for('"a!b"', "vert.example", D), '"a!b"@vert.example', "quoted (usermailaddr)");
Kit.eq(code_of(function () {
	Address.from_for("rob\r\nBcc: e@evil.org", "vert.example", D);
}), "invalid", "local part with a line break");
Kit.eq(code_of(function () {
	Address.from_for("", "vert.example", D);
}), "invalid", "empty local part");
Kit.eq(code_of(function () {
	Address.from_for("a b", "vert.example", D);
}), "invalid", "space in local part");
Kit.eq(code_of(function () {
	Address.from_for("a@b", "vert.example", D);
}), "invalid", "@ in local part");
Kit.eq(code_of(function () {
	Address.from_for('"a"b"', "vert.example", D);
}), "invalid", "quote in a quoted local part");
Kit.eq(code_of(function () {
	Address.from_for(local64 + "a", "vert.example", D);
}), "invalid", "local part over 64");
Kit.eq(code_of(function () {
	Address.from_for("rob", "vert.example\r\n", D);
}), "forbidden", "domain with a line break");
Kit.eq(code_of(function () {
	Address.from_for("rob", "*.wild.example", D);
}), "forbidden", "wildcard");
Kit.eq(code_of(function () {
	Address.from_for("rob", "vert.example", []);
}), "forbidden", "empty list");

Kit.eq(Address.reply_domain("FAMILY.example", "vert.example", D), "family.example", "case");
Kit.eq(Address.reply_domain("jane@family.example", "vert.example", D), "family.example", "an address");
Kit.eq(Address.reply_domain("evil.example", "vert.example", D), "vert.example", "foreign");
Kit.eq(Address.reply_domain(undefined, "vert.example", D), "vert.example", "undefined");
Kit.eq(Address.reply_domain("family.example\r\n", "vert.example", D), "vert.example", "line break");
Kit.eq(code_of(function () {
	Address.reply_domain("evil.example", "vert.example\r\nBcc: e@evil.org", D);
}), "invalid", "default domain with a line break");
Kit.eq(code_of(function () {
	Address.reply_domain("x", "", D);
}), "invalid", "empty default domain");
Kit.eq(code_of(function () {
	Address.reply_domain("x", "host:25", D);
}), "invalid", "default domain with a port");

Kit.assert(Address.is_me("rob@vert.example", "rob", D), "is_me plain");
Kit.assert(!Address.is_me("rob@evil.example", "rob", D), "is_me foreign domain");
Kit.assert(!Address.is_me("rob@vert.example.evil.org", "rob", D), "is_me suffix domain");
Kit.assert(!Address.is_me("robert@vert.example", "rob", D), "is_me other user");
Kit.assert(Address.is_me("Rob.Swindell@vert.example", "Rob Swindell", D), "is_me dotted");
Kit.assert(Address.is_me("rob#news@vert.example", "rob", D), "is_me tag");
Kit.assert(!Address.is_me("rob\r\n@vert.example", "rob", D), "is_me line break");
Kit.assert(!Address.is_me("", "rob", D), "is_me empty");
Kit.assert(!Address.is_me(undefined, "rob", D), "is_me undefined");
Kit.assert(!Address.is_me("rob@vert.example", "", D), "is_me empty alias");
Kit.assert(!Address.is_me("@vert.example", "...", D), "is_me no alphanumerics");
Kit.assert(!Address.is_me("Jane <rob@vert.example>", "rob", D), "is_me takes a bare address");

// ---------------------------------------------------------------------
// identity()

Kit.eq(Address.identity(rob, "internet", { display_name: "" }, ALIASES).from, "rob", "inet aliases");
Kit.eq(Address.identity(rob, "internet", {}, NOALIAS).from, "Rob S", "no display name key");
Kit.eq(Address.identity(rob, "internet", undefined, NOALIAS).from, "Rob S", "no settings");
Kit.eq(Address.identity(rob, "internet", { display_name: "  \t " }, NOALIAS).from, "Rob S",
	"blank display name");
var restricted = { number: 7, alias: "rob", name: "Rob S", security: { restrictions: UD.UREST_REAL_NAME } };
Kit.eq(Address.identity(restricted, "internet", {}, NOALIAS).from, "rob", "UREST_REAL_NAME");
Kit.eq(Address.identity(restricted, "fido", {}, NOALIAS).from, "rob", "UREST_REAL_NAME fido");
Kit.eq(Address.identity(rob, "fido", { display_name: "The Sysop" }, NOALIAS),
	{ from: "Rob S", from_ext: "7" }, "fido: real name, never the display name");
Kit.eq(Address.identity(rob, "fido", {}, ALIASES).from, "rob", "fido aliases");
Kit.eq(Address.identity(rob, "fido", {}, { inet_alias: true, fido_alias: false }).from, "Rob S",
	"fido follows the FidoNet setting, not the Internet one");
Kit.eq(Address.identity(rob, "qwk", { display_name: "The Sysop" }, NOALIAS),
	{ from: "rob", from_ext: "7" }, "qwk: alias");
Kit.eq(Address.identity(rob, "local", {}, ALIASES).from, "rob", "local: alias");
Kit.eq(Address.identity({ number: 9, alias: "Al", name: "", security: { restrictions: 0 } },
	"internet", {}, NOALIAS).from, "Al", "no real name: the alias");
var id = Address.identity(rob, "internet", { display_name: "Evil\r\nBcc: e@evil.org" }, NOALIAS);
Kit.eq(id.from, "EvilBcc: e@evil.org", "display name with a line break");
Kit.eq(Address.identity(rob, "internet", { display_name: 'The "Boss" \\o/' }, NOALIAS).from,
	"The Boss o/", "quote and backslash removed (the mail server quotes the name unescaped)");
Kit.eq(Address.identity(rob, "internet", { display_name: long_name + long_name }, NOALIAS).from,
	long_name.substr(0, 64), "display name cut to 64 bytes");
Kit.eq(Address.identity(rob, "internet", { display_name: "\xA0x" }, NOALIAS).from, FFFD + "x",
	"invalid UTF-8 display name");
Kit.eq(code_of(function () {
	Address.identity(rob, "smtp", {}, NOALIAS);
}), "invalid", "unknown kind");
// The name goes inside the mail server's quotes unescaped: nothing in it
// may read as an address or an encoded word
var spoof = Address.identity(rob, "internet", { display_name:
	"Disp\r\nBcc: x@y.example \"Quote\" <spoof@example.org> =?UTF-8?B?Um9i?=" }, NOALIAS).from;
Kit.assert(!/[<>"\\\r\n]|=\?|\?=/.test(spoof), "no address or encoded word in the From name: " + spoof);
Kit.eq(Address.display_name("a=?=?b?=?=c <x>"), "abc x", "markers removed until none is left");
Kit.eq(Address.display_name("=?UTF-8?B?Um9i?="), "UTF-8?B?Um9i", "an encoded word is broken");
// FidoNet addresses come back in the stored form
Kit.eq(Address.fido_canonical("01:0103/706.0"), "1:103/706", "leading zeros and point 0");
Kit.eq(Address.fido_canonical("1:103/705.2"), "1:103/705.2", "a point kept");
Kit.eq(Address.fido_canonical("x"), "x", "not an address");
var real = Address.identity(rob, "internet", {});	// the system's msg_area settings
Kit.assert(real.from === "rob" || real.from === "Rob S", "real settings: " + real.from);

// ---------------------------------------------------------------------

// format(): a name that looks like an encoded word is quoted, so a reader
// never decodes it into a different name and address
Kit.eq(Address.format("=?UTF-8?B?RXZpbCA8ZUBldmlsLm9yZz4=?=", "j@x.org"),
	'"=?UTF-8?B?RXZpbCA8ZUBldmlsLm9yZz4=?=" <j@x.org>', "encoded-word-looking name quoted");
Kit.eq(Address.format("a=?b", "j@x.org"), '"a=?b" <j@x.org>', "=? anywhere");
Kit.eq(Address.format("a=b?c", "j@x.org"), "a=b?c <j@x.org>", "= and ? apart are atext");
// format() is for Internet recipients: a plain host name only
Kit.eq(code_of(function () {
	Address.format("Rob", "rob@1:103/705");
}), "invalid", "format: FidoNet");
Kit.eq(code_of(function () {
	Address.format("Rob", "a@127.0.0.1:6379");
}), "invalid", "format: host and port");
Kit.eq(code_of(function () {
	Address.format("Rob", "rob@VERT");
}), "invalid", "format: no dot");

// The input-length error has no entry text
try {
	Address.parse_list(huge);
	Kit.assert(false, "should throw");
} catch (e) {
	Kit.eq(e.reason, "toolong", "huge: reason");
	Kit.eq(e.index, -1, "huge: index");
	Kit.eq(e.entry, "", "huge: entry");
	Kit.assert(e.message.indexOf("entry") < 0, "huge: message without an entry: " + e.message);
}

// Trimming is linear: long runs of white space inside text
var spaces = "";
while (spaces.length < 200000)
	spaces += "          ";
var t0 = Date.now();
Kit.eq(Address.clean("a" + spaces + "b", 1000000).length, spaces.length + 2, "inner spaces kept");
Kit.eq(Address.clean("a" + spaces + "\x01"), "a", "trailing spaces before a control");
Kit.eq(Address.clean(spaces + "a" + spaces, 300000), "a", "both edges");
Kit.eq(Address.format("a" + spaces.substr(0, 60) + "b", "j@x.org"), "a b <j@x.org>", "format squeezes");
Kit.assert(Date.now() - t0 < 5000, "white space handling took " + (Date.now() - t0) + " ms");

// Fuzz: every public function's string arguments with control characters
// mixed in; nothing returned (and no error text) has one
var NASTY = ["\r", "\n", "\r\n", "\x00", "\x0B", "\x7F", "\xC2\x85", "\xC2\x9B",
	"\xE2\x80\xA8", "\xE2\x80\xAE", "\t", "\xA0", "\xC3"];
var SEEDS = ["jane", "Jane Doe <j@x.org>", "a@b.org", "rob@1:103/705", "rob@VERT", "Rob S",
	"vert.example", "family.example", "x@mybbs.com", "7", "sysop", "1:103/705"];
function variants(seed)
{
	var out = [];
	for (var a = 0; a < NASTY.length; a++) {
		out.push(NASTY[a] + seed, seed + NASTY[a], seed.substr(0, 2) + NASTY[a] + seed.substr(2));
	}
	return out;
}
var checked = 0;
function check_value(v, what)
{
	if (typeof v === "string") {
		no_controls(v, what);
		checked++;
	} else if (v !== null && typeof v === "object") {
		for (var k in v)
			check_value(v[k], what + "." + k);
	}
}
function probe(what, fn)
{
	var r;
	try {
		r = fn();
	} catch (e) {
		if (!e || typeof e.code !== "string")
			throw new Error(what + ": threw a non-library error: " + e);
		check_value(e, what + " error");
		return;
	}
	check_value(r, what);
}
var fuzz_user = { number: 7, alias: "rob", name: "Rob S", security: { restrictions: 0 } };
for (var si = 0; si < SEEDS.length; si++) {
	var vs = variants(SEEDS[si]);
	for (var vi = 0; vi < vs.length; vi++) {
		var v = vs[vi];
		var w = JSON.stringify(v);
		probe("clean " + w, function () { return Address.clean(v, 64); });
		probe("parse_list " + w, function () { return Address.parse_list(v + ", " + v); });
		probe("resolve " + w, function () {
			return Address.resolve({ name: v, addr: v }, R.matchuser, netaddr_type, R.own);
		});
		probe("resolve name " + w, function () {
			return Address.resolve({ name: v, addr: "a@b.org" }, R.matchuser, netaddr_type, R.own);
		});
		probe("matchuser " + w, function () { return R.matchuser(v); });
		probe("local_user " + w, function () { return R.own.local_user(v, v + "@mybbs.com"); });
		probe("format " + w, function () { return Address.format(v, "a@b.org"); });
		probe("format addr " + w, function () { return Address.format("x", v); });
		probe("format_list " + w, function () {
			return Address.format_list([{ name: v, addr: "a@b.org" }, { name: "y", addr: v }]);
		});
		probe("domains " + w, function () {
			return Address.domains([v, "ok.example"], function () { return true; });
		});
		probe("domains_for " + w, function () {
			return Address.domains_for(1, { system: { inet_domain_list: [v, SEEDS[si]] }, User: FakeUser });
		});
		probe("from_for " + w, function () { return Address.from_for(v, "vert.example", D); });
		probe("from_for domain " + w, function () { return Address.from_for("rob", v, D); });
		probe("reply_domain " + w, function () { return Address.reply_domain(v, "vert.example", D); });
		probe("reply_domain default " + w, function () { return Address.reply_domain("x", v, D); });
		probe("is_me " + w, function () { return Address.is_me(v, v, D); });
		var kinds4 = ["internet", "local", "fido", "qwk"];
		for (var ki = 0; ki < kinds4.length; ki++) {
			var kk = kinds4[ki];
			probe("identity " + kk + " " + w, function () {
				return Address.identity({ number: 7, alias: v, name: v, security: { restrictions: 0 } },
					kk, { display_name: v }, NOALIAS);
			});
			probe("identity name " + kk + " " + w, function () {
				return Address.identity({ number: 7, alias: v, name: "", security: { restrictions: 0 } },
					kk, {}, ALIASES);
			});
		}
		probe("identity kind " + w, function () { return Address.identity(fuzz_user, v, {}, NOALIAS); });
	}
}
Kit.assert(checked > 1000, "fuzz checked " + checked + " strings");
