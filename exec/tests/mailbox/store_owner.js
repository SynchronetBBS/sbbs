// Store, read side: ownership of list rows built from the facts cache.  A
// record whose 16-bit index field matches the user but whose header names
// another user (mail to user 125537 has 60001 there) is never listed and
// never cached; a warm Inbox page and search read no shared header.

load(js.exec_dir + "kit.js");
var JsonFiles = Kit.lib("mailbox_files");
var Store = Kit.lib("mailbox");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var CARL = 125537;	// 125537 & 0xFFFF == 60001: Alice's index field

var S = Kit.scratch();
var shared = null;
var stores = [];

function save(mb, hdr, body)
{
	if (hdr.from === undefined)
		hdr.from = "Sender";
	if (hdr.to === undefined)
		hdr.to = "Rcpt";
	if (!mb.save_msg(hdr, body === undefined ? "body\r\n" : body))
		throw new Error("save_msg: " + mb.last_error);
	return mb.last_msg;
}

function ids(list)
{
	var r = [];
	for (var i = 0; i < list.items.length; i++)
		r.push(list.items[i].id);
	return r;
}

// A Store whose shared header reads are counted
function open_store(usernum)
{
	var st = new Store({ usernum: usernum, alias: "u" + usernum, name: "User " + usernum,
		shared_path: S.shared_path, data_dir: S.data_dir });
	st.open();
	st.reads = 0;
	st.shared_header = function (num) {
		st.reads++;
		return Store.prototype.shared_header.call(this, num);
	};
	stores.push(st);
	return st;
}

function cache_text(usernum)
{
	var f = new File(new JsonFiles(S.user_dir(usernum), "mailbox.").path("shared"));
	if (!f.open("r"))
		return "";
	var s = f.read();
	f.close();
	return s;
}

try {
	shared = Kit.email_base(S.shared_path);
	var A1 = save(shared, { to_ext: String(ALICE), from: "Ann", subject: "for alice zz" });
	var A2 = save(shared, { to_ext: String(ALICE), from: "Ann", subject: "moving zz" });
	var C1 = save(shared, { to_ext: String(CARL), from: "Carl Only Sender",
		subject: "carl private subject zz" });

	var st = open_store(ALICE);
	Kit.eq(ids(st.list("inbox")), ["s" + A2, "s" + A1], "Alice's inbox");
	Kit.eq(ids(st.list("all", { search: "zz" })), ["s" + A2, "s" + A1], "Alice's search");
	Kit.eq(st.folders()[0].total, 2, "Alice's Inbox count");

	// Another user's list fields never reach Alice's cache file
	var text = cache_text(ALICE);
	Kit.assert(text.length > 0, "Alice's facts cache written");
	Kit.assert(text.indexOf("carl private subject") < 0, "no subject of Carl's in Alice's cache");
	Kit.assert(text.indexOf("Carl Only Sender") < 0, "no sender of Carl's in Alice's cache");

	// Warm: an Inbox page and a search read no shared header
	st.reads = 0;
	Kit.eq(ids(st.list("inbox")), ["s" + A2, "s" + A1], "warm inbox");
	Kit.eq(ids(st.list("all", { search: "zz" })), ["s" + A2, "s" + A1], "warm search");
	Kit.eq(st.reads, 0, "no shared header read when warm");
	Kit.assert(cache_text(ALICE).indexOf("carl private subject") < 0, "still not cached");
} finally {
	for (var k = 0; k < stores.length; k++)
		stores[k].close();
	if (shared)
		shared.close();
	S.cleanup();
}
