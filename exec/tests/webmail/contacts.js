// Contacts: the user's address book (webmail/lib/contacts.js)

load(js.exec_dir + "kit.js");
var Contacts = Kit.wlib("contacts");
var UserData = Kit.wlib("userdata");
var JsonFiles = Kit.lib("mailbox_files");

var S = Kit.scratch();
var logged = [];
Contacts.log = function (level, text) {
	logged.push(text);
};

function book(usernum)
{
	return new UserData(S.user_dir(usernum) + "webmail/", JsonFiles);
}

function fails(fn, code, reason)
{
	var caught = null;
	try {
		fn();
	} catch (e) {
		caught = e;
	}
	Kit.assert(caught !== null, "expected " + code);
	Kit.eq(caught.code, code, "code (" + JSON.stringify(caught) + ")");
	if (reason !== undefined)
		Kit.eq(caught.reason, reason, "reason");
	return caught;
}

function addrs_of(list)
{
	var out = [];
	for (var i = 0; i < list.length; i++)
		out.push(list[i].addr);
	return out;
}

try {
	var ud = book(60001);
	Kit.eq(Contacts.list(ud), [], "empty book");
	Kit.eq(Contacts.suggest(ud, "a"), [], "nothing to suggest");

	// --- set: create, change, validate
	var c1 = Contacts.set(ud, { name: "Ann Example", addrs: ["ann@example.org", "Ann@Example.org",
		"Ann <ann.work@example.com>"], note: "met at\r\nthe meetup" });
	Kit.eq(c1.id, "c1", "first id");
	Kit.eq(c1.addrs, ["ann@example.org", "ann.work@example.com"], "addresses parsed, case repeats dropped");
	Kit.assert(!/[\r\n]/.test(c1.note), "no control characters in the note");
	var c2 = Contacts.set(ud, { name: "bob", addrs: ["bob"] });
	Kit.eq(c2.addrs, ["bob"], "a local user name");
	var c3 = Contacts.set(ud, { name: "Fido Friend", addrs: ["Fido.Friend@1:103/705"] });
	Kit.eq(Contacts.list(ud).map(function (c) { return c.name; }), ["Ann Example", "bob", "Fido Friend"],
		"listed by name");
	var c1b = Contacts.set(ud, { id: "c1", name: "Ann E.", addrs: ["ann@example.org"] });
	Kit.eq([c1b.id, c1b.name, c1b.addrs], ["c1", "Ann E.", ["ann@example.org"]], "changed");
	fails(function () { Contacts.set(ud, { id: "c99", name: "x", addrs: ["x@example.org"] }); }, "notfound");
	fails(function () { Contacts.set(ud, { id: "../x", name: "x", addrs: ["x@example.org"] }); }, "invalid", "id");
	fails(function () { Contacts.set(ud, { name: "x", addrs: [] }); }, "invalid", "addrs");
	fails(function () { Contacts.set(ud, { name: "x" }); }, "invalid", "addrs");
	fails(function () { Contacts.set(ud, { name: "x", addrs: ["a@b.example, c@d.example"] }); }, "invalid", "addrs");
	fails(function () { Contacts.set(ud, { name: "x", addrs: ["bad\r\nBcc: e@x.example"] }); }, "invalid", "addrs");
	// the refusal names the address (its index in addrs), so the page can
	// say which line
	Kit.eq(fails(function () { Contacts.set(ud, { name: "x", addrs: ["ok@example.org", "ok2@example.org", "not <<valid"] }); },
		"invalid", "addrs").index, 2, "the bad address's index");
	Kit.eq(fails(function () { Contacts.set(ud, { name: "x", addrs: ["a, b@c.example"] }); }, "invalid", "addrs").index, 0,
		"index 0");
	Kit.eq(fails(function () { Contacts.set(ud, { name: "x", addrs: [] }); }, "invalid", "addrs").index, undefined,
		"no index for an empty list");
	fails(function () { Contacts.set(ud, { name: 5, addrs: ["x@example.org"] }); }, "invalid", "name");
	var eleven = [];
	for (var i = 0; i < 11; i++)
		eleven.push("p" + i + "@example.org");
	fails(function () { Contacts.set(ud, { name: "x", addrs: eleven }); }, "invalid", "addrs");
	var hostile = Contacts.set(ud, { name: "Evil\r\nBcc: x@evil.example\x00", addrs: ["e@example.org"] });
	Kit.assert(!/[\r\n\x00]/.test(hostile.name), "name cleaned: " + hostile.name);
	var longname = Contacts.set(ud, { name: new Array(200).join("n"), addrs: ["l@example.org"] });
	Kit.assert(longname.name.length <= 64, "name cut to 64 bytes");

	// --- remove
	Kit.eq(Contacts.remove(ud, hostile.id), true, "removed");
	fails(function () { Contacts.remove(ud, hostile.id); }, "notfound");
	fails(function () { Contacts.remove(ud, "x"); }, "invalid", "id");

	// --- collect: unknown addresses added once, case ignored
	var ud2 = book(60002);
	var entries = [{ name: "Zed", addr: "zed@example.org" }, { name: "", addr: "ZED@example.org" },
		{ name: "Amy", addr: "amy@example.org" }, { name: "x", addr: "a@@b" }, { name: "y", addr: "" },
		{ name: "Bad", addr: "b@example.org\r\nBcc: x@evil.example" }, null];
	Kit.eq(Contacts.collect(ud2, entries), 2, "two added");
	Kit.eq(Contacts.collect(ud2, entries), 0, "collect twice does not duplicate");
	Kit.eq(Contacts.list(ud2).length, 2, "two contacts");
	Kit.eq(Contacts.collect(ud2, [{ name: "Zed Again", addr: "Zed@Example.org" }]), 0, "known address");
	Kit.eq(Contacts.list(ud2)[1].name, "Zed", "a known contact keeps its name");
	Kit.eq(Contacts.collect(ud2, []), 0, "nothing to collect");
	Kit.eq(Contacts.collect(ud2, "x"), 0, "not a list");

	// --- suggest: prefix of name, a word of the name, or address; recency
	var ud3 = book(60003);
	Contacts.collect(ud3, [{ name: "Alice Smith", addr: "alice@example.org" }]);
	Contacts.collect(ud3, [{ name: "Alan Jones", addr: "aj@example.net" }]);
	Contacts.collect(ud3, [{ name: "Bob Alder", addr: "bob@example.com" }]);
	Kit.eq(addrs_of(Contacts.suggest(ud3, "al")), ["bob@example.com", "aj@example.net", "alice@example.org"],
		"most recently used first");
	Contacts.collect(ud3, [{ name: "", addr: "alice@example.org" }]);	// used again
	Kit.eq(addrs_of(Contacts.suggest(ud3, "AL")), ["alice@example.org", "bob@example.com", "aj@example.net"],
		"suggest orders by recency (case ignored)");
	Kit.eq(addrs_of(Contacts.suggest(ud3, "smi")), ["alice@example.org"], "a word of the name");
	Kit.eq(addrs_of(Contacts.suggest(ud3, "aj@")), ["aj@example.net"], "an address prefix");
	Kit.eq(addrs_of(Contacts.suggest(ud3, "zzz")), [], "no match");
	Kit.eq(Contacts.suggest(ud3, "al", 1).length, 1, "limit");
	Kit.eq(Contacts.suggest(ud3, "", 2).length, 2, "empty query: most recent");
	Kit.eq(Contacts.suggest(ud3, "al")[0], { id: "c1", name: "Alice Smith", addr: "alice@example.org" }, "shape");
	// another user's book is never consulted
	Kit.eq(Contacts.suggest(ud2, "al").length, 0, "own contacts only");

	// --- addresses that are names of every object's properties
	var ud6 = book(60006);
	Kit.eq(Contacts.collect(ud6, [{ name: "", addr: "constructor" }, { name: "", addr: "__proto__" },
		{ name: "", addr: "toString" }, { name: "", addr: "hasOwnProperty" }]), 4, "collected");
	Kit.eq(Contacts.collect(ud6, [{ name: "", addr: "constructor" }]), 0, "known");
	var c6 = Contacts.set(ud6, { name: "odd", addrs: ["constructor", "__proto__", "CONSTRUCTOR"] });
	Kit.eq(c6.addrs, ["constructor", "__proto__"], "kept once each");
	Kit.eq(addrs_of(Contacts.suggest(ud6, "cons")).length, 2, "suggested");

	// --- the limit of 500 contacts
	var ud4 = book(60004);
	var many = [];
	for (i = 0; i < 505; i++)
		many.push({ name: "N" + i, addr: "n" + i + "@example.org" });
	Kit.eq(Contacts.collect(ud4, many), 500, "collect stops at 500");
	fails(function () { Contacts.set(ud4, { name: "one more", addrs: ["x@example.org"] }); }, "toolarge");
	Contacts.remove(ud4, "c1");
	Kit.eq(Contacts.set(ud4, { name: "one more", addrs: ["x@example.org"] }).id, "c501", "room again; ids not reused");

	// --- an unusable file is an empty book, not an error
	var ud5 = book(60005);
	ud5.write("contacts", { v: 1, list: [null, { id: "../x", name: "x", addrs: [] }, { id: "c7", name: "ok",
		addrs: ["ok@example.org", 5] }] });
	ud5.write("contacts", { v: 1, list: [null, { id: "../x", name: "x", addrs: [] },
		{ id: "c6", name: "no address", addrs: [""] }, { id: "c7", name: "ok", addrs: ["ok@example.org", 5] }] });
	Kit.eq(Contacts.list(ud5), [{ id: "c7", name: "ok", addrs: ["ok@example.org"], note: "" }], "bad entries skipped");
	Kit.eq(Contacts.set(ud5, { name: "new", addrs: ["n@example.org"] }).id, "c8", "next id after the highest");
	ud5.write("contacts", "garbage");
	Kit.eq(Contacts.list(ud5), [], "garbage reads as empty");
	Kit.eq(logged, [], "nothing logged");
} finally {
	S.cleanup();
}
