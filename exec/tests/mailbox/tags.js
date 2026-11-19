// Tags: folder and flag encoding in message header tags

load(js.exec_dir + "kit.js");
var Tags = Kit.lib("mailbox_tags");

Kit.eq(Tags.decode(Tags.encode("Tax 100% \\ done")), "Tax 100% \\ done");
Kit.assert(Tags.encode("a b").indexOf(" ") < 0);
Kit.eq(Tags.TRASH, "$Trash");
Kit.eq(Tags.SENT, "$Sent");
Kit.eq(Tags.ARCHIVE, "$Archive");
Kit.eq(Tags.STARRED, "$Starred");
Kit.eq(Tags.WAS, "$Was:");
Kit.eq(Tags.FAILED, "$Failed");
Kit.eq(Tags.UNSENT, "$Unsent");
Kit.eq(Tags.encode("$Trash"), "%24Trash");	// cannot forge
Kit.eq(Tags.decode("%24Trash"), "$Trash");

// Encoding of specific characters
Kit.eq(Tags.encode("Receipts"), "Receipts");
Kit.eq(Tags.encode("a.b_c-d9"), "a.b_c-d9");
Kit.eq(Tags.encode("a b"), "a%20b");
Kit.eq(Tags.encode("50%"), "50%25");
Kit.eq(Tags.encode("a$b"), "a%24b");
Kit.eq(Tags.encode("a:b"), "a%3Ab");
Kit.eq(Tags.encode("a\\b"), "a%5Cb");
Kit.eq(Tags.encode("a\tb\x01"), "a%09b%01");
Kit.eq(Tags.encode("a/b!"), "a%2Fb%21");

// Non-ASCII (UTF-8 byte string) names are kept as they are
var utf8 = "Caf\xC3\xA9 \xE2\x82\xAC";	// "Cafe-acute Euro"
Kit.eq(Tags.encode(utf8), "Caf\xC3\xA9%20\xE2\x82\xAC");
Kit.eq(Tags.decode(Tags.encode(utf8)), utf8);
Kit.throws(function () { Tags.encode("x" + String.fromCharCode(0x20AC)); }, "invalid");

// Round trip of awkward names
var names = ["", "a", " ", "  lead", "trail  ", "%", "%41", "%%", "$", "$Sent", "a:b:c", "\\",
	"\\n", "tab\there", "ctl\x01\x1F\x7F", "\xFF\x80", "q\"u'o", "a,b;c", "a b c d e f"];
for (var i = 0; i < names.length; i++) {
	var t = Tags.encode(names[i]);
	Kit.assert(!/[ \t]/.test(t), "no whitespace in tag " + JSON.stringify(t));
	Kit.assert(t.charAt(0) != '$', "tag cannot start with $");
	Kit.eq(Tags.decode(t), names[i], "round trip " + JSON.stringify(names[i]));
}
// decode tolerates stray percent signs
Kit.eq(Tags.decode("100%"), "100%");
Kit.eq(Tags.decode("%zz"), "%zz");

// decode reverses only what encode produces; anything else stays literal
Kit.eq(Tags.decode("%5a"), "%5a", "lowercase hex is not an encode escape");
Kit.eq(Tags.decode("%5A"), "%5A", "%5A is a safe letter, never escaped by encode");
Kit.eq(Tags.decode("%41"), "%41", "safe byte is never escaped by encode");
Kit.eq(Tags.decode("%C3%A9"), "%C3%A9", "bytes >= 0x80 are never escaped");
Kit.eq(Tags.decode("%"), "%");
Kit.eq(Tags.decode("%2541"), "%41", "one level only: %25 then 41");
Kit.eq(Tags.decode("%20%24"), " $");
Kit.assert(Tags.decode("A") != Tags.decode("%41"), "A and %41 display differently");
Kit.assert(Tags.decode("%5a") != Tags.decode("%5A"), "%5a and %5A display differently");
// canonical
Kit.assert(Tags.canonical("Receipts"));
Kit.assert(Tags.canonical("a%20b"));
Kit.assert(Tags.canonical(Tags.encode(utf8)));
Kit.assert(Tags.canonical(""));
Kit.assert(!Tags.canonical("%41"));
Kit.assert(!Tags.canonical("%5a"));
Kit.assert(!Tags.canonical("%C3%A9"));
Kit.assert(!Tags.canonical("%"));
Kit.assert(!Tags.canonical("%zz"));
Kit.assert(Tags.canonical("%2541"), "the encoding of the name %41");
Kit.assert(!Tags.canonical("a b"), "space is not canonical");
Kit.assert(!Tags.canonical("$Sent"), "a raw $ tag is not a user-folder tag");

// byte 0xA0 is not whitespace (a-grave is C3 A0)
var grave = "Caf\xC3\xA0 x\xC3\xA0";
var gt = Tags.encode(grave);
Kit.eq(Tags.parse(gt + " $Starred").folder, gt, "0xA0 does not split a tag");
Kit.eq(Tags.parse(gt + " $Starred").other, []);
Kit.eq(Tags.format(Tags.parse(gt + " $Starred")), gt + " $Starred");
Kit.eq(Tags.parse("a\xA0b").folder, "a\xA0b");
Kit.eq(Tags.parse("x\xC3\xA0 y\xC3\xA0").folder, "x\xC3\xA0");
Kit.eq(Tags.parse("x\xC3\xA0 y\xC3\xA0").other, ["y\xC3\xA0"]);
Kit.eq(Tags.parse("a\x85b").folder, "a\x85b");
Kit.eq(Tags.parse("a\fb\vc\rd\ne").other.length, 4, "ASCII whitespace still separates");
Kit.eq(Tags.validate("Caf\xC3\xA0", ["Caf\xC3"]), null, "no trim of 0xA0 in existing");
Kit.eq(Tags.validate("Caf\xC3", ["Caf\xC3\xA0"]), null);
Kit.eq(Tags.validate("\xC3\xA0", []), null);
Kit.eq(Tags.validate("\xC3\xA0", ["\xC3"]), null);
Kit.eq(Tags.validate("\xC3\xA0", ["\xC3\xA0"]), "duplicate");
Kit.eq(Tags.validate("\xA0", []), null, "a lone 0xA0 is a name, not empty");
Kit.eq(Tags.validate(" \t\xC3\xA0 \n", ["\xC3\xA0"]), "duplicate", "ASCII whitespace still trimmed");
// bytes 0xC0-0xDE must not fold to 0xE0-0xFE
Kit.eq(Tags.validate("\xC3\x89", ["\xE3\x89"]), null, "0xC3 vs 0xE3 differ");
Kit.eq(Tags.validate("\xC0", ["\xE0"]), null, "0xC0 vs 0xE0 differ");
Kit.eq(Tags.validate("\xDE", ["\xFE"]), null, "0xDE vs 0xFE differ");
Kit.eq(Tags.validate("\xFF", ["\xFF"]), "duplicate");

// validate
Kit.eq(Tags.validate("  ", []), "empty");
Kit.eq(Tags.validate("", []), "empty");
Kit.eq(Tags.validate(new Array(66).join("x"), []), "toolong");
Kit.eq(Tags.validate(new Array(65).join("x"), []), null);	// 64 characters
Kit.eq(Tags.validate(new Array(66).join("x"), []), "toolong");	// 65 characters
Kit.eq(Tags.validate(new Array(33).join("\xC3\xA9"), []), null, "32 e-acute = 64 chars, 64 bytes");
Kit.eq(Tags.validate(new Array(65).join("\xC3\xA9"), []), null, "64 e-acute = 64 chars, 128 bytes");
Kit.eq(Tags.validate(new Array(66).join("\xC3\xA9"), []), "toolong");
Kit.eq(Tags.validate("family", ["Family"]), "duplicate");
Kit.eq(Tags.validate("FAMILY", ["family"]), "duplicate");
Kit.eq(Tags.validate("a\nb", []), "invalid", "control character inside a name");
Kit.eq(Tags.validate("a\tb", []), "invalid", "tab inside a name");
Kit.eq(Tags.validate("a\x07", []), "invalid", "BEL");
Kit.eq(Tags.validate("a\x7Fb", []), "invalid", "DEL");
Kit.eq(Tags.validate("\x00", []), "invalid", "NUL");
Kit.eq(Tags.validate(" a b\t", []), null, "ASCII whitespace at the edges is trimmed, not invalid");
// The tags of a shared message have no folder
Kit.eq(Tags.parse("work urgent $Starred", { shared: true }),
	{ folder: "", starred: true, was: "", failed: false, unsent: false, other: ["work", "urgent"] },
	"shared: every tag but the flags is other");
Kit.eq(Tags.parse("work urgent").folder, "work", "private: the first tag is the folder");
Kit.eq(Tags.validate("  family ", ["Family"]), "duplicate");
Kit.eq(Tags.validate("Caf\xC3\xA9", ["caf\xC3\x89"]), null, "no Unicode case folding");
Kit.eq(Tags.validate("Caf\xC3\xA9", ["caf\xC3\xA9"]), "duplicate");
Kit.eq(Tags.validate("work", ["Family"]), null);
Kit.eq(Tags.validate("inbox", []), "reserved");
var builtin = ["inbox", "starred", "sent", "drafts", "spam", "trash", "archive", "all"];
for (i = 0; i < builtin.length; i++) {
	Kit.eq(Tags.validate(builtin[i], []), "reserved", builtin[i]);
	Kit.eq(Tags.validate(builtin[i].toUpperCase(), []), "reserved", builtin[i]);
}
Kit.eq(Tags.validate("Inbox2", []), null);
Kit.eq(Tags.validate("a b", []), null, "spaces allowed");
Kit.eq(Tags.validate("a:b", []), null, "colon allowed (encoded)");
Kit.eq(Tags.validate("a\x01b", []), "invalid", "control character refused");
Kit.eq(Tags.validate("x" + String.fromCharCode(0x20AC), []), "invalid");
Kit.eq(Tags.validate("ok", undefined), null, "existing omitted");

// parse / format
var P = Tags.parse("$Starred Receipts $Was:$Inbox foreign");
Kit.assert(P.folder == "Receipts" && P.starred && P.was == "$Inbox");
Kit.assert(!P.failed && !P.unsent && P.other[0] == "foreign");
Kit.eq(Tags.parse(Tags.format(P)), P);

Kit.eq(Tags.parse(""), { folder: "", starred: false, was: "", failed: false, unsent: false, other: [] });
Kit.eq(Tags.parse(undefined), Tags.parse(""));
Kit.eq(Tags.parse(null), Tags.parse(""));
Kit.eq(Tags.parse("   \t "), Tags.parse(""));
Kit.eq(Tags.parse("  $Sent   $Failed  ").folder, "$Sent");
Kit.assert(Tags.parse("  $Sent   $Failed  ").failed);
Kit.eq(Tags.parse("$Trash $Was:Receipts").was, "Receipts");
Kit.eq(Tags.parse("$Sent $Unsent").unsent, true);
Kit.eq(Tags.parse("$Starred").folder, "");
Kit.eq(Tags.parse("$Weird mine $Other more"), { folder: "mine", starred: false, was: "", failed: false, unsent: false, other: ["$Weird", "$Other", "more"] });
Kit.eq(Tags.parse("$Weird").other, ["$Weird"], "unknown $ tag is not a folder");
Kit.eq(Tags.parse("$Archive Receipts").other, ["Receipts"], "first folder wins");

Kit.eq(Tags.format({}), "");
Kit.throws(function () { Tags.format({ folder: "two tags" }); }, "invalid");
Kit.throws(function () { Tags.format({ folder: "a\tb" }); }, "invalid");
Kit.throws(function () { Tags.format({ folder: "a\nb" }); }, "invalid");
Kit.throws(function () { Tags.format({ was: "$Inbox x" }); }, "invalid");
Kit.throws(function () { Tags.format({ was: "a\rb" }); }, "invalid");
Kit.eq(Tags.format({ folder: "a\xA0b", was: "c\xA0d" }), "a\xA0b $Was:c\xA0d", "0xA0 is not whitespace");
Kit.eq(Tags.format({ folder: "Receipts", starred: true, was: "$Inbox", failed: true, unsent: true, other: ["x", "$Y"] }),
	"Receipts $Starred $Was:$Inbox $Failed $Unsent x $Y");
Kit.eq(Tags.format({ other: ["x"], folder: "$Sent" }), "$Sent x");

// parse(format(x)) == x, including an encoded non-ASCII folder
var objs = [
	Tags.parse(""),
	Tags.parse("$Sent $Failed"),
	Tags.parse("$Trash $Starred $Was:" + Tags.encode(utf8) + " zzz $Qq"),
	{ folder: Tags.encode(utf8), starred: true, was: "$Inbox", failed: false, unsent: true, other: ["o1", "$o2"] }
];
for (i = 0; i < objs.length; i++)
	Kit.eq(Tags.parse(Tags.format(objs[i])), objs[i], "format round trip " + i);
Kit.eq(Tags.decode(Tags.parse(Tags.format(objs[3])).folder), utf8);
