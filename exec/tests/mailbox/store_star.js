// Store: a star is the MSG_VALIDATED attribute (what the IMAP service shows
// as \Flagged), in the header and the index, in either base.  The Starred
// folder and counts read it from the index (shared) or the summary
// (private) on every call, with no header read for cached rows; it moves
// with the message between the bases.

load(js.exec_dir + "kit.js");
var Tags = Kit.lib("mailbox_tags");
var Store = Kit.lib("mailbox");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var V = D.MSG_VALIDATED;

var S = Kit.scratch();
var shared = null;
var stores = [];

function save(mb, hdr)
{
	if (hdr.from === undefined)
		hdr.from = "Sender";
	if (hdr.to === undefined)
		hdr.to = "Alice";
	if (!mb.save_msg(hdr, "body\r\n"))
		throw new Error("save_msg: " + mb.last_error);
	return mb.last_msg;
}

function to_alice(subject, extra)
{
	var hdr = { to_ext: String(ALICE), subject: subject };
	for (var k in extra || {})
		hdr[k] = extra[k];
	return save(shared, hdr);
}

// A Store whose shared header reads are counted
function open_store()
{
	var st = new Store({ usernum: ALICE, alias: "alice", name: "Alice",
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

function fresh_shared()
{
	shared.close();
	shared = Kit.email_base(S.shared_path);
	return shared;
}

// {hdr, idx} attributes of a live message of base path (or the shared
// base), null if gone
function attrs(path, num)
{
	var mb = path === null ? fresh_shared() : Kit.email_base(path);
	try {
		var h = mb.get_msg_header(false, num, false);
		if (h === null || h.number != num || (h.attr & D.MSG_DELETE))
			return null;
		var x = mb.get_msg_index(false, num);
		return { hdr: h.attr, idx: x.attr, tags: h.tags || "" };
	} finally {
		if (path !== null)
			mb.close();
	}
}

function starred_bit(a)
{
	return a !== null && (a.hdr & V) != 0 && (a.idx & V) != 0;
}

function ids(list)
{
	var r = [];
	for (var i = 0; i < list.items.length; i++)
		r.push(list.items[i].id);
	return r;
}

function item(list, id)
{
	for (var i = 0; i < list.items.length; i++)
		if (list.items[i].id == id)
			return list.items[i];
	return null;
}

function folder(st, id)
{
	var f = st.folders();
	for (var i = 0; i < f.length; i++)
		if (f[i].id == id)
			return f[i];
	return null;
}

function ok(res, what)
{
	Kit.assert(res instanceof Array && res.length > 0, what + ": results");
	for (var i = 0; i < res.length; i++)
		Kit.assert(res[i].ok === true, what + ": " + JSON.stringify(res[i]));
	return res[0].new_id;
}

function num_of(id)
{
	return parseInt(id.substr(1), 10);
}

try {
	shared = Kit.email_base(S.shared_path);
	var A1 = to_alice("plain one");
	var A2 = to_alice("plain two");
	var IMAP = to_alice("flagged by imap");
	var pdir = S.user_dir(ALICE) + "mailbox";
	var st = open_store();
	var P1 = num_of(ok(st.move(["s" + A2], "archive"), "file one"));
	var PRIV = S.data_dir + format("user/%04u/mailbox", ALICE);
	Kit.eq(pdir, PRIV, "the private base path");

	// Warm everything once
	st.folders();
	st.list("inbox");
	st.list("starred");
	Kit.eq(folder(st, "starred").total, 0, "nothing starred at first");
	Kit.eq(ids(st.list("starred")), [], "empty Starred folder at first");

	// --- Star and unstar an Inbox message: the bit in header and index;
	// the Starred folder and counts follow at once, no header read
	ok(st.set(["s" + A1], { starred: true }), "star s" + A1);
	Kit.assert(starred_bit(attrs(null, A1)), "shared: MSG_VALIDATED in header and index");
	Kit.eq(attrs(null, A1).tags, "", "shared: no tag written");
	st.reads = 0;
	Kit.eq(ids(st.list("starred")), ["s" + A1], "starred list at once");
	Kit.eq(folder(st, "starred").total, 1, "starred count at once");
	Kit.eq(item(st.list("inbox"), "s" + A1).starred, true, "inbox row starred");
	Kit.eq(st.reads, 0, "no shared header read for cached rows");
	ok(st.set(["s" + A1], { starred: false }), "unstar s" + A1);
	Kit.eq((attrs(null, A1).hdr | attrs(null, A1).idx) & V, 0, "shared: bit cleared in header and index");
	st.reads = 0;
	Kit.eq(ids(st.list("starred")), [], "unstarred at once");
	Kit.eq(item(st.list("inbox"), "s" + A1).starred, false, "inbox row unstarred");
	Kit.eq(st.reads, 0, "no shared header read after unstar");

	// --- A private message: the bit in its header and index, the summary
	// carries it
	ok(st.set(["p" + P1], { starred: true }), "star p" + P1);
	Kit.assert(starred_bit(attrs(PRIV, P1)), "private: MSG_VALIDATED in header and index");
	Kit.eq(attrs(PRIV, P1).tags, Tags.ARCHIVE, "private: tags unchanged");
	Kit.assert(ids(st.list("starred")).indexOf("p" + P1) >= 0, "private star listed");
	Kit.eq(item(st.list("archive"), "p" + P1).starred, true, "archive row starred");
	ok(st.set(["p" + P1], { starred: false }), "unstar p" + P1);
	Kit.eq(attrs(PRIV, P1).hdr & V, 0, "private: bit cleared");
	Kit.assert(ids(st.list("starred")).indexOf("p" + P1) < 0, "private unstarred");

	// --- Flagged outside the library (as an IMAP client would): starred
	// at once, from the index, no header read
	st.list("inbox");
	var h = fresh_shared().get_msg_header(false, IMAP, false);
	h.attr |= V;
	if (!shared.put_msg_header(false, IMAP, h))
		throw new Error("put_msg_header: " + shared.last_error);
	st.reads = 0;
	Kit.assert(ids(st.list("starred")).indexOf("s" + IMAP) >= 0, "flagged elsewhere: starred");
	Kit.eq(item(st.list("inbox"), "s" + IMAP).starred, true, "flagged elsewhere: row starred");
	Kit.eq(st.reads, 0, "flagged elsewhere: no header read");
	Kit.eq(st.get("s" + IMAP).starred, true, "flagged elsewhere: get() says starred");

	// --- A private message moved between private folders keeps its star
	var pmb = Kit.email_base(PRIV);
	var P4 = save(pmb, { tags: Tags.ARCHIVE, subject: "private starred" });
	pmb.close();
	ok(st.set(["p" + P4], { starred: true }), "star p" + P4);
	st.folder_create("Keep");
	ok(st.move(["p" + P4], "u:Keep"), "move private starred");
	var pa = attrs(PRIV, P4);
	Kit.eq([pa.tags, (pa.hdr & V) != 0], ["Keep", true], "private move(): tags rewritten, attribute kept");
	Kit.assert(ids(st.list("starred")).indexOf("p" + P4) >= 0, "private moved message starred");

	// --- A move in both directions, to Trash and back, keeps the star
	ok(st.set(["s" + A1], { starred: true }), "star again");
	var p1 = ok(st.move(["s" + A1], "u:Keep"), "Inbox to folder");
	Kit.assert(starred_bit(attrs(PRIV, num_of(p1))), "to the private base: starred");
	Kit.assert(ids(st.list("starred")).indexOf(p1) >= 0, "listed starred in the private base");
	var s1 = ok(st.move([p1], "inbox"), "folder to Inbox");
	Kit.assert(starred_bit(attrs(null, num_of(s1))), "back in the shared base: starred");
	Kit.eq(attrs(null, num_of(s1)).tags, "", "no tag in the shared base");
	Kit.assert(ids(st.list("starred")).indexOf(s1) >= 0, "listed starred in the Inbox");
	var t1 = ok(st.remove([s1]), "to Trash");
	Kit.assert(starred_bit(attrs(PRIV, num_of(t1))), "in Trash: the bit kept");
	Kit.assert(ids(st.list("starred")).indexOf(t1) < 0, "Trash is not listed as starred");
	Kit.eq(item(st.list("trash"), t1).starred, false, "a Trash row shows no star");
	var r1 = ok(st.restore([t1]), "restore");
	Kit.assert(starred_bit(attrs(null, num_of(r1))), "restored: starred");
	Kit.assert(ids(st.list("starred")).indexOf(r1) >= 0, "restored: listed starred");
	var t2 = ok(st.remove(["p" + P4]), "private to Trash");
	ok(st.restore([t2]), "private restore");
	Kit.assert(starred_bit(attrs(PRIV, P4)), "private Trash and back: starred");

	// --- The facts cache holds no star
	var facts = Kit.lib("mailbox_files");
	var cache = new facts(S.user_dir(ALICE), "mailbox.").read("shared", null);
	Kit.assert(cache && cache.v === 4, "facts cache version 4");
	for (var n in cache.recs)
		Kit.eq(cache.recs[n].length, 8, "fact #" + n + ": no star element");
} finally {
	for (var i = 0; i < stores.length; i++)
		stores[i].close();
	if (shared)
		shared.close();
	S.cleanup();
}
