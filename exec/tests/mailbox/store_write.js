// Store, write side: flags, moves, Trash (delete, restore, empty,
// expiry), folders, quota, ownership and interrupted moves

load(js.exec_dir + "kit.js");
var JsonFiles = Kit.lib("mailbox_files");
var Tags = Kit.lib("mailbox_tags");
var Store = Kit.lib("mailbox");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var BOB = 60002;
var DAY = 24 * 60 * 60;
var T0 = 1700000000;	// the injected clock for moves to Trash

var S = Kit.scratch();
var shared = null;
var stores = [];
var clock = T0;

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

function to_alice(subject, id, extra)
{
	var h = { to_ext: String(ALICE), to: "Alice", from: "Carol", subject: subject };
	if (id)
		h.id = id;
	for (var k in extra || {})
		h[k] = extra[k];
	return save(fresh(), h, "body of " + subject + "\r\n");
}

function open_store(usernum, options)
{
	var st = new Store({ usernum: usernum, alias: "u" + usernum, name: "User " + usernum,
		shared_path: S.shared_path, data_dir: S.data_dir, options: options || {},
		now: function () { return clock; } });
	st.open();
	stores.push(st);
	return st;
}

// Open a base by path for a look from outside the library
function peek(path, fn)
{
	var mb = Kit.email_base(path);
	try {
		return fn(mb);
	} finally {
		mb.close();
	}
}

function alice_path()
{
	return S.user_dir(ALICE) + "mailbox";
}

function phdr(num)	// Alice's private header (unexpanded), null if gone
{
	return peek(alice_path(), function (mb) {
		var h = mb.get_msg_header(false, num, false);
		if (h === null || h === undefined || h.number != num || (h.attr & D.MSG_DELETE))
			return null;
		return h;
	});
}

// The test's shared handle, reopened: an open handle can serve stale
// buffered index and header bytes after the library wrote the base
function fresh()
{
	shared.close();
	shared = Kit.email_base(S.shared_path);
	return shared;
}

function shdr(num)	// a shared header (unexpanded), null if gone
{
	var h = fresh().get_msg_header(false, num, false);
	if (h === null || h === undefined || h.number != num || (h.attr & D.MSG_DELETE))
		return null;
	return h;
}

function ptags(num)
{
	var h = phdr(num);
	return h === null ? null : Tags.parse(h.tags);
}

function num_of(id)
{
	return parseInt(id.substr(1), 10);
}

function ids(list)
{
	var r = [];
	for (var i = 0; i < list.items.length; i++)
		r.push(list.items[i].id);
	return r;
}

function ok(res, msg)
{
	for (var i = 0; i < res.length; i++)
		if (!res[i].ok)
			throw new Error((msg || "") + ": " + JSON.stringify(res[i]));
	return res;
}

function one(res, msg)	// the new id of a one-id successful call
{
	Kit.eq(res.length, 1, (msg || "") + " result count");
	ok(res, msg);
	return res[0].new_id;
}

function err(res, i, code, msg)
{
	Kit.assert(res[i].ok === false && res[i].error === code, (msg || "") + ": expected " + code
		+ " got " + JSON.stringify(res[i]));
}

// Private messages with this Message-ID
function count_id(path, msgid)
{
	return peek(path, function (mb) {
		var idx = mb.get_index() || [];
		var n = 0;
		for (var i = 0; i < idx.length; i++) {
			if (idx[i].attr & D.MSG_DELETE)
				continue;
			var h = mb.get_msg_header(false, idx[i].number, false);
			if (h !== null && h.id == msgid)
				n++;
		}
		return n;
	});
}

function live_msgs(path)
{
	return peek(path, function (mb) {
		var idx = mb.get_index() || [];
		var n = 0;
		for (var i = 0; i < idx.length; i++)
			if (!(idx[i].attr & D.MSG_DELETE))
				n++;
		return n;
	});
}

function total_msgs(path)
{
	return peek(path, function (mb) { return mb.total_msgs; });
}

try {
	shared = Kit.email_base(S.shared_path);
	var adir = S.user_dir(ALICE);
	var ajf = new JsonFiles(adir, "mailbox.");

	// --- Shared mail
	var A1 = to_alice("first", "<a1@test>");
	var A2 = to_alice("second", "<a2@test>");
	var A3 = to_alice("third", "<a3@test>");
	var A4 = to_alice("fourth", "<a4@test>");
	var A5 = to_alice("fifth", "<a5@test>");
	var AS = to_alice("spammy", "<as@test>", { attr: D.MSG_SPAM });
	var B1 = save(shared, { to_ext: String(BOB), to: "Bob", subject: "bob's", id: "<b1@test>" });
	var EX = save(shared, { from_ext: String(ALICE), from: "Alice", to_ext: String(BOB), to: "Bob",
		subject: "sent from the terminal", id: "<ex@test>" });

	// --- Alice's private base: Sent copy, a folder, and filler (so a full
	// summary rebuild would read many headers)
	var PS, PR, i;
	peek(alice_path(), function (mb) {
		PS = save(mb, { from_ext: String(ALICE), from: "Alice", to: "x@example.com",
			to_net_type: D.NET_INTERNET, to_net_addr: "x@example.com", subject: "a sent copy",
			tags: Tags.SENT, id: "<ps@test>" });
		PR = save(mb, { to_ext: String(ALICE), subject: "a receipt", tags: "Receipts" });
		for (i = 0; i < 25; i++)
			save(mb, { to_ext: String(ALICE), subject: "filler " + i, tags: Tags.ARCHIVE });
	});
	ajf.write("folders", ["Receipts"]);

	// --- Bob's private base, with more messages than Alice's, so a
	// "p<number>" exists there and not in Alice's base
	var bdir = S.user_dir(BOB);
	var PB;
	peek(bdir + "mailbox", function (mb) {
		for (i = 0; i < 60; i++)
			PB = save(mb, { to_ext: String(BOB), subject: "bob private " + i, tags: Tags.ARCHIVE });
	});
	Kit.assert(phdr(PB) === null, "Bob's last private number is not Alice's");

	var st = open_store(ALICE, { trash_days: 30, quota_bytes: 0 });

	// Count private header reads (a full summary rebuild reads every one),
	// and let a test intercept writes to the private base: a hook returns
	// a value to use instead of the real call's, undefined to make the call
	var preads = 0;
	var hooks = {};
	function hook(mb, method, name) {
		var real = mb[method];
		mb[method] = function () {
			if (hooks[name]) {
				var r = hooks[name].apply(mb, arguments);
				if (r !== undefined)
					return r;
			}
			return real.apply(mb, arguments);
		};
	}
	st.private_base = function (create) {
		var had = this.pmb;
		var mb = Store.prototype.private_base.call(this, create);
		if (mb !== null && mb !== had) {
			var real = mb.get_msg_header;
			mb.get_msg_header = function () {
				preads++;
				return real.apply(mb, arguments);
			};
			hook(mb, "put_msg_header", "put");
			hook(mb, "remove_msg", "remove");
			hook(mb, "purge", "purge");
			hook(mb, "move_msg", "move");
		}
		return mb;
	};
	// Make the shared base's move_msg do something else, once
	function shared_move_once(fn) {
		st.open_shared = function () {
			Store.prototype.open_shared.call(this);
			var mb = this.smb;
			var real = mb.move_msg;
			mb.move_msg = function () {
				if (fn !== null) {
					var f = fn;
					fn = null;
					return f.apply(mb, arguments);
				}
				return real.apply(mb, arguments);
			};
		};
	}
	// A journal entry as Store writes one before a move between bases
	function journal(source_id, h, after, tags) {
		var j = ajf.read("moving", {});
		j[source_id] = { to: source_id.charAt(0) == "s" ? "p" : "s", msgid: h.id || "",
			imported: h.when_imported_time, written: h.when_written_time, after: after, tags: tags };
		ajf.write("moving", j);
	}
	function last_msg(path) {
		return peek(path, function (mb) { return mb.last_msg; });
	}
	function generic(r, msg) {
		Kit.assert(typeof r.message == "string" && r.message.indexOf(S.dir) < 0
			&& r.message.indexOf("Error") < 0, msg + ": message not generic: " + r.message);
	}
	st.folders();	// warm the summary

	// --- Move Inbox -> folder: body intact, gone from the shared base
	var body1 = shared.get_msg_body(false, A1);
	preads = 0;
	var p1 = one(st.move(["s" + A1], "u:Receipts"), "move to folder");
	Kit.assert(preads <= 2, "move to folder: no summary rebuild (" + preads + " reads)");
	Kit.assert(p1.charAt(0) == "p", "moved to private");
	Kit.assert(shdr(A1) === null, "gone from shared");
	Kit.eq(ptags(num_of(p1)).folder, "Receipts", "tagged");
	var pbody = peek(alice_path(), function (mb) { return mb.get_msg_body(false, num_of(p1)); });
	Kit.eq(md5_calc(pbody, true), md5_calc(body1, true), "body md5");
	Kit.assert(ids(st.list("u:Receipts")).indexOf(p1) >= 0, "listed in folder");
	Kit.assert(ids(st.list("inbox")).indexOf("s" + A1) < 0, "not in inbox");

	// --- And back to the Inbox
	preads = 0;
	var s1 = one(st.move([p1], "inbox"), "move to inbox");
	Kit.assert(preads <= 2, "move to inbox: no summary rebuild (" + preads + " reads)");
	Kit.assert(s1.charAt(0) == "s", "back in shared");
	Kit.assert(phdr(num_of(p1)) === null, "gone from private");
	Kit.eq(shdr(num_of(s1)).subject, "first", "shared copy");
	Kit.eq(md5_calc(fresh().get_msg_body(false, num_of(s1)), true), md5_calc(body1, true), "body md5 back");
	Kit.assert(ids(st.list("inbox")).indexOf(s1) >= 0, "listed in inbox");

	// --- set: flags persist
	ok(st.set([s1, "p" + PR], { read: true, starred: true }), "set read+starred");
	Kit.assert(shdr(num_of(s1)).attr & D.MSG_READ, "shared read");
	Kit.assert(Tags.parse(shdr(num_of(s1)).tags).starred, "shared starred");
	Kit.assert(phdr(PR).attr & D.MSG_READ, "private read");
	Kit.assert(ptags(PR).starred, "private starred");
	Kit.assert(ids(st.list("starred")).indexOf(s1) >= 0, "starred list sees the shared star");
	ok(st.set([s1], { read: false, starred: false }), "unset");
	Kit.assert(!(shdr(num_of(s1)).attr & D.MSG_READ), "shared unread");
	Kit.assert(!Tags.parse(shdr(num_of(s1)).tags).starred, "shared unstarred");
	Kit.assert(ids(st.list("starred")).indexOf(s1) < 0, "unstarred in list");
	ok(st.set(["s" + AS], { spam: false }), "not spam");
	Kit.assert(!(shdr(AS).attr & D.MSG_SPAM), "spam cleared");
	Kit.assert(ids(st.list("inbox")).indexOf("s" + AS) >= 0, "not-spam in inbox");
	ok(st.set(["s" + AS], { spam: true }), "spam");
	Kit.assert(ids(st.list("spam")).indexOf("s" + AS) >= 0, "spam again");
	err(st.set(["p" + PR], { spam: true }), 0, "invalid", "spam on private");

	// --- Another user's mail: forbidden or notfound, nothing changes
	var b1_before = JSON.stringify(shdr(B1));
	var res = st.set(["s" + B1], { read: true, starred: true });
	err(res, 0, "forbidden", "set Bob's");
	err(st.move(["s" + B1], "u:Receipts"), 0, "forbidden", "move Bob's");
	err(st.remove(["s" + B1]), 0, "forbidden", "remove Bob's");
	err(st.restore(["s" + B1]), 0, "forbidden", "restore Bob's");
	Kit.eq(JSON.stringify(shdr(B1)), b1_before, "Bob's header unchanged");
	err(st.set(["p" + PB], { read: true }), 0, "notfound", "Bob's private number");
	err(st.move(["p" + PB], "inbox"), 0, "notfound", "move Bob's private number");
	err(st.remove(["p" + PB]), 0, "notfound", "remove Bob's private number");
	Kit.eq(peek(bdir + "mailbox", function (mb) {
		return mb.get_msg_header(false, PB, false).attr;
	}), 0, "Bob's private message unchanged");
	// A sent extra: hide only
	err(st.set(["s" + EX], { starred: true }), 0, "forbidden", "star an extra");
	err(st.set(["s" + EX], { read: true }), 0, "forbidden", "read an extra");
	err(st.move(["s" + EX], "u:Receipts"), 0, "forbidden", "move an extra");
	var ex_before = JSON.stringify(shdr(EX));
	Kit.eq(one(st.remove(["s" + EX]), "hide extra"), null, "hidden extra new_id");
	Kit.eq(JSON.stringify(shdr(EX)), ex_before, "extra's header unchanged");
	Kit.eq(ajf.read("hidden", []), ["m:<ex@test>"], "hidden list (by Message-ID)");
	Kit.assert(ids(st.list("sent")).indexOf("s" + EX) < 0, "extra hidden from Sent");

	// --- Bad arguments
	Kit.throws(function () { st.move(["s" + A2], "trash"); }, "invalid");
	Kit.throws(function () { st.move(["s" + A2], "sent"); }, "invalid");
	Kit.throws(function () { st.move(["s" + A2], "starred"); }, "invalid");
	Kit.throws(function () { st.move(["s" + A2], "spam"); }, "invalid");
	Kit.throws(function () { st.move(["s" + A2], "u:Nope"); }, "notfound");
	Kit.throws(function () { st.move("s" + A2, "archive"); }, "invalid");
	err(st.move(["x1"], "archive"), 0, "invalid", "bad id");

	// --- Delete and restore: from the Inbox, a folder, Sent and Spam
	clock = T0;
	var t1 = one(st.remove([s1]), "delete from inbox");
	var tp = ptags(num_of(t1));
	Kit.eq([tp.folder, tp.was], [Tags.TRASH, "$Inbox"], "trash from inbox");
	Kit.assert(shdr(num_of(s1)) === null, "inbox original gone");
	Kit.assert(ids(st.list("trash")).indexOf(t1) >= 0, "in trash list");
	var s1b = one(st.restore([t1]), "restore to inbox");
	Kit.assert(s1b.charAt(0) == "s" && shdr(num_of(s1b)).subject == "first", "restored to inbox");
	Kit.assert(phdr(num_of(t1)) === null, "trash copy gone");

	Kit.eq(one(st.remove(["p" + PR]), "delete from folder"), "p" + PR, "retagged in place");
	Kit.eq([ptags(PR).folder, ptags(PR).was, ptags(PR).starred], [Tags.TRASH, "Receipts", true],
		"trash from folder");
	one(st.restore(["p" + PR]), "restore to folder");
	Kit.eq([ptags(PR).folder, ptags(PR).was, ptags(PR).starred], ["Receipts", "", true],
		"back in folder, star kept");

	one(st.remove(["p" + PS]), "delete from sent");
	Kit.eq(ptags(PS).was, Tags.SENT, "trash from sent");
	one(st.restore(["p" + PS]), "restore to sent");
	Kit.eq(ptags(PS).folder, Tags.SENT, "back in sent");
	Kit.assert(ids(st.list("sent")).indexOf("p" + PS) >= 0, "listed in sent");

	var ts = one(st.remove(["s" + AS]), "delete from spam");
	Kit.eq(ptags(num_of(ts)).was, "$Spam", "trash from spam");
	var ss = one(st.restore([ts]), "restore to spam");
	Kit.assert(shdr(num_of(ss)).attr & D.MSG_SPAM, "restored as spam");
	Kit.assert(ids(st.list("spam")).indexOf(ss) >= 0, "listed in spam");
	err(st.restore(["p" + PR]), 0, "invalid", "restore of a message not in Trash");

	// --- Delete twice removes for real; a gone id is notfound while the
	// rest of the call applies
	var t2 = one(st.remove(["s" + A2]), "delete A2");
	var before_total = total_msgs(alice_path());
	res = st.remove([t2, "s" + A3, "s999999"]);
	Kit.eq(res[0], { id: t2, ok: true, new_id: null }, "removed for real");
	Kit.assert(res[1].ok && res[1].new_id.charAt(0) == "p", "other id still applies");
	err(res, 2, "notfound", "missing id");
	Kit.assert(phdr(num_of(t2)) === null, "really gone");
	Kit.eq(total_msgs(alice_path()), live_msgs(alice_path()), "purged: no flagged records left");
	Kit.assert(total_msgs(alice_path()) < before_total, "purge freed records");
	res = st.remove([t2, "s" + A4]);
	err(res, 0, "notfound", "second real removal");
	Kit.assert(res[1].ok, "rest of the call");
	var t4 = res[1].new_id;
	res = st.move(["s" + A2, "s" + A5], "archive");
	err(res, 0, "notfound", "move of a moved message");
	Kit.assert(res[1].ok, "rest of the move");
	var pa5 = res[1].new_id;

	// --- move_msg fails once and copies nothing; the
	// message stays, and the retry moves it normally
	var A6 = to_alice("sixth", "<a6@test>");
	shared_move_once(function () { return false; });
	res = st.move(["s" + A6], "u:Receipts");
	Kit.assert(res[0].ok === false && res[0].error == "failed", "failed move reported");
	generic(res[0], "failed move");
	Kit.assert(shdr(A6) !== null, "still in shared");
	Kit.assert(ids(st.list("inbox")).indexOf("s" + A6) >= 0, "still in inbox");
	Kit.eq(count_id(alice_path(), "<a6@test>"), 0, "no private copy");
	delete st.open_shared;
	var pa6 = one(st.move(["s" + A6], "u:Receipts"), "retry after a failed move");
	Kit.eq(count_id(alice_path(), "<a6@test>"), 1, "one private copy after retry");

	// move_msg copies, then returns false (source state unknown): the
	// stray copy is in the summary and the usage, and the retry reuses it
	var A9 = to_alice("ninth", "<a9@test>");
	var u9 = st.usage().bytes;
	shared_move_once(function (num, dest, changes) {
		if (this.copy_msg(num, dest, changes) === false)
			throw new Error("copy_msg: " + this.last_error);
		return false;
	});
	err(st.move(["s" + A9], "u:Receipts"), 0, "failed", "copied, then false");
	delete st.open_shared;
	Kit.eq(count_id(alice_path(), "<a9@test>"), 1, "the stray copy exists");
	var stray = null;
	var rl = st.list("u:Receipts", { limit: 100 }).items;
	for (i = 0; i < rl.length; i++)
		if (rl[i].subject == "ninth")
			stray = rl[i].id;
	Kit.assert(stray !== null, "stray copy listed (summary sees it)");
	Kit.assert(st.usage().bytes > u9, "usage counts the stray copy");
	Kit.eq(one(st.move(["s" + A9], "u:Receipts"), "retry reuses the copy"), stray, "same copy");
	Kit.eq(count_id(alice_path(), "<a9@test>"), 1, "one private copy after retry (stray)");
	Kit.assert(shdr(A9) === null, "shared original removed (stray)");
	Kit.eq(ajf.read("moving", {})["s" + A9], undefined, "journal entry cleared");

	// Killed between the copy and the removal: journal entry, private
	// copy and shared original all exist; the move reuses the copy
	var A7 = to_alice("seventh", "<a7@test>");
	var after = last_msg(alice_path());
	peek(alice_path(), function (mb) {
		if (fresh().copy_msg(A7, mb, { tags: "Receipts" }) === false)
			throw new Error("copy_msg: " + shared.last_error);
	});
	journal("s" + A7, shdr(A7), after, "Receipts");
	var pa7 = one(st.move(["s" + A7], "u:Receipts"), "move after kill");
	Kit.eq(count_id(alice_path(), "<a7@test>"), 1, "one private copy");
	Kit.assert(shdr(A7) === null, "shared original removed");
	Kit.eq(ptags(num_of(pa7)).folder, "Receipts", "copy tagged");
	// The same, when the repeated operation is a delete (copy retagged)
	var A10 = to_alice("tenth", "<a10@test>");
	after = last_msg(alice_path());
	peek(alice_path(), function (mb) {
		if (fresh().copy_msg(A10, mb, { tags: Tags.ARCHIVE }) === false)
			throw new Error("copy_msg: " + shared.last_error);
	});
	journal("s" + A10, shdr(A10), after, Tags.ARCHIVE);
	var pa10 = one(st.remove(["s" + A10]), "delete after kill");
	Kit.eq(count_id(alice_path(), "<a10@test>"), 1, "one private copy (delete)");
	Kit.assert(shdr(A10) === null, "shared original removed (delete)");
	Kit.eq([ptags(num_of(pa10)).folder, ptags(num_of(pa10)).was], [Tags.TRASH, "$Inbox"],
		"retagged to Trash");
	// And on the way back: a shared copy and the private original
	var A8 = to_alice("eighth", "<a8@test>");
	var pa8 = one(st.move(["s" + A8], "archive"), "file A8");
	after = fresh().last_msg;
	peek(alice_path(), function (mb) {
		if (mb.copy_msg(num_of(pa8), fresh(), { tags: "" }) === false)
			throw new Error("copy_msg back: " + mb.last_error);
	});
	journal(pa8, phdr(num_of(pa8)), after, "");
	var sa8 = one(st.move([pa8], "inbox"), "move back after kill");
	Kit.eq(count_id(alice_path(), "<a8@test>"), 0, "private original removed");
	function live_shared(msgid) {
		var n = 0;
		var sidx = fresh().get_index();
		for (var j = 0; j < sidx.length; j++)
			if (!(sidx[j].attr & D.MSG_DELETE) && shdr(sidx[j].number).id == msgid)
				n++;
		return n;
	}
	Kit.eq(live_shared("<a8@test>"), 1, "one shared copy");
	Kit.assert(shdr(num_of(sa8)) !== null, "result names the live shared copy");
	Kit.eq(ajf.read("moving", {})[pa8], undefined, "journal entry cleared (back)");

	// A journal entry whose source is gone (the move finished) is dropped
	journal("s" + A8, { id: "<a8@test>", when_imported_time: 1, when_written_time: 1 }, 0, "");
	st.set([sa8], { read: true });
	Kit.eq(ajf.read("moving", {})["s" + A8], undefined, "stale journal entry dropped");

	// Look-alikes WITHOUT a journal entry are never reused: one message
	// delivered twice in the same second
	var D1 = to_alice("delivered twice", "<dup@test>");
	var D2 = fresh().copy_msg(D1, shared);
	Kit.assert(D2 !== false, "second delivery");
	var pd1 = one(st.move(["s" + D1], "u:Receipts"), "file the first delivery");
	var pd2 = one(st.remove(["s" + D2]), "delete the second delivery");
	Kit.assert(pd2 != pd1, "a new Trash copy");
	Kit.eq(ptags(num_of(pd1)).folder, "Receipts", "filed copy still filed");
	Kit.eq(ptags(num_of(pd2)).folder, Tags.TRASH, "the second alone in Trash");
	Kit.assert(shdr(D2) === null, "second delivery moved");
	one(st.remove([pd2]), "remove the second for real");
	Kit.eq(ptags(num_of(pd1)).folder, "Receipts", "filed copy survives");
	// and the other way: restoring does not swallow a twin in the Inbox
	var D3 = to_alice("twins", "<twin@test>");
	var D4 = fresh().copy_msg(D3, shared);
	var pt4 = one(st.remove(["s" + D4]), "trash a twin");
	var sd4 = one(st.restore([pt4]), "restore the twin");
	Kit.assert(sd4 != "s" + D3 && shdr(D3) !== null, "the other twin untouched");
	Kit.eq(live_shared("<twin@test>"), 2, "both twins in the Inbox");

	// --- Folders: create, rename (retags everything, Trash origins too),
	// delete (to Trash), restore recreates
	Kit.throws(function () { st.folder_create("receipts"); }, "invalid");
	Kit.throws(function () { st.folder_create("Inbox"); }, "invalid");
	Kit.throws(function () { st.folder_create("  "); }, "invalid");
	var f = st.folder_create("Tax Stuff");
	Kit.eq(f, { id: "u:" + Tags.encode("Tax Stuff"), name: "Tax Stuff" }, "created");
	Kit.eq(ajf.read("folders", []), ["Receipts", Tags.encode("Tax Stuff")], "folders.json");
	var tax = ok(st.move([pa5, pa6], f.id), "file to Tax");
	var tax1 = tax[0].new_id;
	var tax2 = tax[1].new_id;
	one(st.remove([tax2]), "trash one from Tax");
	Kit.eq(ptags(num_of(tax2)).was, Tags.encode("Tax Stuff"), "was Tax");
	preads = 0;
	var r2 = st.folder_rename(f.id, "Taxes 2026");
	Kit.assert(preads <= 4, "rename: no summary rebuild (" + preads + " reads)");
	var newtag = Tags.encode("Taxes 2026");
	Kit.eq(r2, { id: "u:" + newtag, name: "Taxes 2026", errors: [] }, "renamed");
	Kit.eq(ptags(num_of(tax1)).folder, newtag, "message retagged");
	Kit.eq(ptags(num_of(tax2)).was, newtag, "trash origin retagged");
	Kit.eq(ajf.read("folders", []), ["Receipts", newtag], "folders.json renamed");
	Kit.throws(function () { st.folder_rename("u:" + newtag, "Receipts"); }, "invalid");
	Kit.throws(function () { st.folder_rename("u:Nope", "X"); }, "notfound");
	Kit.eq(st.folder_rename("u:" + newtag, "taxes 2026").name, "taxes 2026", "case-only rename");
	newtag = Tags.encode("taxes 2026");
	res = ok(st.folder_delete("u:" + newtag), "folder delete");
	Kit.eq(res.length, 1, "one message trashed by the folder delete");
	Kit.eq([ptags(num_of(tax1)).folder, ptags(num_of(tax1)).was], [Tags.TRASH, newtag], "to Trash");
	Kit.eq(ajf.read("folders", []), ["Receipts"], "folder gone");
	Kit.throws(function () { st.list("u:" + newtag); }, "notfound");
	one(st.restore([tax1]), "restore into a deleted folder");
	Kit.eq(ptags(num_of(tax1)).folder, newtag, "restored tag");
	Kit.eq(ajf.read("folders", []), ["Receipts", newtag], "folder recreated");

	// --- Expiry: measured from when the message was put in Trash
	clock = T0;
	var e1 = one(st.remove(["p" + PR]), "trash at T0");
	clock = T0 + 10 * DAY;
	var e2 = one(st.remove([tax1]), "trash at T0+10d");
	var trash_now = ids(st.list("trash", { limit: 100 }));
	var tj = ajf.read("trash", {});
	Kit.eq([tj[num_of(e1)], tj[num_of(e2)]], [T0, T0 + 10 * DAY], "trash times recorded");
	// A Trash message with no record is treated as trashed when first seen
	var orphan = t4;
	Kit.assert(trash_now.indexOf(orphan) >= 0, "orphan in trash");
	delete tj[num_of(orphan)];
	ajf.write("trash", tj);
	res = st.purge_expired(T0 + 35 * DAY);
	Kit.assert(res instanceof Array, "expiry ran");
	var gone = [];
	for (i = 0; i < res.length; i++) {
		Kit.assert(res[i].ok, "expired ok");
		gone.push(res[i].id);
	}
	Kit.assert(gone.indexOf(e1) >= 0, "30+ days in Trash expired");
	Kit.assert(gone.indexOf(e2) < 0, "25 days in Trash kept");
	Kit.assert(gone.indexOf(orphan) < 0, "unrecorded kept");
	Kit.eq(ajf.read("trash", {})[num_of(orphan)], T0 + 35 * DAY, "unrecorded now recorded");
	Kit.assert(ajf.read("trash", {})[num_of(e1)] === undefined, "expired entry pruned");
	Kit.eq(st.purge_expired(T0 + 35 * DAY + DAY - 1), null, "at most once a day");
	Kit.assert(phdr(num_of(e2)) !== null, "kept by the skipped run");
	res = st.purge_expired(T0 + 41 * DAY);
	Kit.assert(res instanceof Array && res.length >= 1, "next day runs");
	Kit.assert(phdr(num_of(e2)) === null, "e2 expired on the next day's run");

	// --- File attachments: deleted with the message, only in the user's
	// own incoming directory
	var adir_in = S.data_dir + format("file/%04u.in/", ALICE);
	var bdir_in = S.data_dir + format("file/%04u.in/", BOB);
	mkpath(adir_in);
	mkpath(bdir_in);
	function touch(path, n) {
		var fh = new File(path);
		if (!fh.open("wb"))
			throw new Error("create " + path);
		fh.write(new Array(n + 1).join("x"));
		fh.close();
	}
	touch(adir_in + "pic.jpg", 1000);
	touch(adir_in + "my doc.txt", 10);
	touch(bdir_in + "secret.txt", 10);
	var FA = to_alice("pic.jpg \"my doc.txt\" ../" + format("%04u", BOB) + ".in/secret.txt", "<fa@test>",
		{ auxattr: D.MSG_FILEATTACH });
	var tf = one(st.remove(["s" + FA]), "trash file-attached");
	Kit.assert(file_exists(adir_in + "pic.jpg"), "file kept while in Trash");
	// remove_msg fails: the message and its files both stay
	hooks.remove = function () { return false; };
	try {
		res = st.remove([tf]);
	} finally {
		hooks.remove = null;
	}
	err(res, 0, "failed", "remove_msg failed");
	generic(res[0], "remove_msg failed");
	Kit.assert(file_exists(adir_in + "pic.jpg") && file_exists(adir_in + "my doc.txt"),
		"files kept when the message could not be removed");
	Kit.eq(ptags(num_of(tf)).folder, Tags.TRASH, "still in Trash");
	one(st.remove([tf]), "remove file-attached");
	Kit.assert(!file_exists(adir_in + "pic.jpg") && !file_exists(adir_in + "my doc.txt"),
		"attachments deleted");
	Kit.assert(file_exists(bdir_in + "secret.txt"), "never outside the user's directory");

	// --- purge() fails: the removal still succeeds (flagged, freed later)
	var TP = to_alice("purge fails", "<tp@test>");
	var tpp = one(st.remove(["s" + TP]), "trash for the purge failure");
	hooks.purge = function () { return false; };
	try {
		one(st.remove([tpp]), "removed with purge failing");
	} finally {
		hooks.purge = null;
	}
	Kit.assert(phdr(num_of(tpp)) === null, "removed although not purged");
	Kit.assert(total_msgs(alice_path()) > live_msgs(alice_path()), "flagged record left");

	// --- empty_trash purges
	var T1 = to_alice("trash me 1", "<t1@test>");
	var T2 = to_alice("trash me 2", "<t2@test>");
	ok(st.remove(["s" + T1, "s" + T2]), "fill trash");
	var in_trash = st.list("trash").total;
	Kit.assert(in_trash >= 2, "trash has messages");
	var u_before = st.usage().bytes;
	before_total = total_msgs(alice_path());
	preads = 0;
	res = ok(st.empty_trash(), "empty trash");
	Kit.assert(preads <= in_trash + 1, "empty_trash: no summary rebuild (" + preads + " reads)");
	Kit.eq(res.length, in_trash, "every trash message removed");
	Kit.eq(total_msgs(alice_path()), live_msgs(alice_path()), "purged, not just flagged");
	Kit.assert(total_msgs(alice_path()) <= before_total - in_trash, "records freed");
	Kit.eq(st.list("trash").total, 0, "trash empty");
	Kit.assert(st.usage().bytes < u_before, "usage dropped");
	Kit.eq(st.empty_trash(), [], "empty trash again");

	// --- Quota
	var q = st.usage();
	Kit.eq(q.quota, 0, "quota option");
	var Q1 = to_alice("quota trash old", "<q1@test>");
	var Q2 = to_alice("quota trash new", "<q2@test>");
	clock = T0 + 100 * DAY;
	var qt1 = one(st.remove(["s" + Q1]), "trash q1");
	var qt2 = one(st.remove(["s" + Q2]), "trash q2");
	var QM = to_alice("quota move", "<qm@test>");
	var base = st.usage().bytes;	// includes the two Trash messages
	var tsize = 512;	// a small message: one header block, one data block
	// Room for the incoming message once the oldest Trash is removed
	var qst = open_store(ALICE, { trash_days: 30, quota_bytes: base + tsize - 1 });
	var qm = one(qst.move(["s" + QM], "archive"), "move with oldest Trash removed");
	Kit.assert(phdr(num_of(qt1)) === null, "oldest Trash removed");
	Kit.assert(phdr(num_of(qt2)) !== null, "newer Trash kept");
	Kit.assert(qst.usage().bytes <= base + tsize - 1, "within quota");
	// No room even with Trash empty: quota, Trash kept, message stays
	var QN = to_alice("quota no room", "<qn@test>");
	var qst2 = open_store(ALICE, { trash_days: 30, quota_bytes: qst.usage().bytes - tsize });
	err(qst2.move(["s" + QN], "archive"), 0, "quota", "over quota");
	Kit.assert(shdr(QN) !== null, "still in the Inbox");
	Kit.eq(qst2.list("trash").total, 1, "Trash kept when emptying it would not be enough");
	// Deleting is never refused for quota (it is how a user gets under it)
	var QR = to_alice("quota delete", "<qr@test>");
	var qr = one(qst2.remove(["s" + QR]), "delete over quota");
	Kit.eq(ptags(num_of(qr)).folder, Tags.TRASH, "in Trash over quota");
	// After room is made the same move succeeds
	one(qst2.move([qm], "inbox"), "make room");
	one(qst2.move(["s" + QN], "archive"), "same move succeeds");

	// --- Summary lock held: conflict for every id, nothing written
	var QC = to_alice("conflict", "<qc@test>");
	one(st.move(["s" + to_alice("receipt 2", "<rc@test>")], "u:Receipts"), "file a receipt");
	var lock = st.jf.lock_path("summary");
	Kit.assert(file_mutex(lock, "held by test", 60), "take summary lock");
	st.jf.timeout = 200;
	try {
		res = st.remove(["s" + QC, "p" + PS]);
		err(res, 0, "conflict", "locked remove");
		err(res, 1, "conflict", "locked remove (private)");
		Kit.throws(function () { st.folder_rename("u:Receipts", "Bills"); }, "conflict");
	} finally {
		file_remove(lock);
		st.jf.timeout = 5000;
	}
	Kit.assert(shdr(QC) !== null, "still in the Inbox after a conflict");
	Kit.eq(ptags(PS).folder, Tags.SENT, "private untouched after a conflict");
	Kit.eq(ajf.read("folders", [])[0], "Receipts", "folders untouched after a conflict");

	// --- forget_shared cannot take its lock after a shared write: the
	// facts cache is deleted, so no stale star survives
	Kit.assert(file_exists(ajf.path("shared")), "facts cache exists");
	lock = st.jf.lock_path("shared");
	Kit.assert(file_mutex(lock, "held by test", 60), "take facts lock");
	st.jf.timeout = 200;
	try {
		one(st.set(["s" + QC], { starred: true }), "star while facts are locked");
	} finally {
		file_remove(lock);
		st.jf.timeout = 5000;
	}
	Kit.assert(!file_exists(ajf.path("shared")), "facts cache deleted");
	Kit.assert(ids(st.list("starred")).indexOf("s" + QC) >= 0, "star seen");

	// --- A rename interrupted on its second message: what was retagged is
	// recorded, the rest keeps the old tag, both folders list correctly
	var bills = st.folder_create("Bills").id;
	ok(st.move(["s" + to_alice("bill 1", "<bill1@test>"), "s" + to_alice("bill 2", "<bill2@test>")],
		bills), "file two bills");
	var puts = 0;
	hooks.put = function () {
		if (++puts == 2)
			throw new TypeError("simulated failure");
		return undefined;
	};
	try {
		preads = 0;
		var rb = st.folder_rename(bills, "Paid");
	} finally {
		hooks.put = null;
	}
	Kit.eq([rb.id, rb.name, rb.errors.length], ["u:Paid", "Paid", 1], "partial rename result");
	Kit.eq(rb.errors[0].error, "failed", "per-message failure");
	generic(rb.errors[0], "unexpected exception");
	Kit.eq(st.list("u:Paid").total, 1, "renamed message listed");
	Kit.eq(st.list(bills).total, 1, "the other keeps the old folder");
	Kit.assert(preads < 10, "no summary rebuild after a partial rename (" + preads + " reads)");
	// folders.json cannot be updated: the messages are renamed anyway
	lock = st.jf.lock_path("folders");
	Kit.assert(file_mutex(lock, "held by test", 60), "take folders lock");
	st.jf.timeout = 100;
	try {
		rb = st.folder_rename("u:Paid", "Settled");
	} finally {
		file_remove(lock);
		st.jf.timeout = 5000;
	}
	Kit.eq(rb.id, "u:Settled", "renamed without folders.json");
	Kit.eq(st.list("u:Settled").total, 1, "message carries the new tag");
	Kit.assert(ajf.read("folders", []).indexOf("Paid") >= 0, "folders.json unchanged");

	// --- The journal never mistakes a same-second twin for the
	// copy of an interrupted move
	function count_subject(path, subject) {
		return peek(path, function (mb) {
			var idx = mb.get_index() || [];
			var n = 0;
			for (var j = 0; j < idx.length; j++) {
				if (idx[j].attr & D.MSG_DELETE)
					continue;
				var h = mb.get_msg_header(false, idx[j].number, false);
				if (h !== null && h.subject == subject)
					n++;
			}
			return n;
		});
	}
	function twin(subject, msgid) {
		var a = to_alice(subject, msgid);
		var b = fresh().copy_msg(a, shared);
		Kit.assert(b !== false, "twin of " + subject);
		return [a, b];
	}
	ok(st.empty_trash(), "start the twin cases with an empty Trash");
	// The first try copies nothing; the twin is filed; the retry must
	// not take the filed twin
	var TA = twin("twin a", "<twa@test>");
	shared_move_once(function () { return false; });
	err(st.remove(["s" + TA[0]]), 0, "failed", "no copy: first try fails");
	delete st.open_shared;
	var fa = one(st.move(["s" + TA[1]], "u:Receipts"), "no copy: file the twin");
	var ta = one(st.remove(["s" + TA[0]]), "no copy: retry");
	Kit.assert(ta != fa, "no copy: a new Trash copy");
	Kit.eq(ptags(num_of(fa)).folder, "Receipts", "no copy: filed twin still filed");
	Kit.eq(ptags(num_of(ta)).folder, Tags.TRASH, "no copy: retried one in Trash");
	Kit.assert(shdr(TA[0]) === null && shdr(TA[1]) === null, "no copy: both moved");
	ok(st.empty_trash(), "no copy: empty Trash");
	Kit.eq(count_id(alice_path(), "<twa@test>"), 1, "no copy: the filed twin survives empty_trash");
	Kit.eq(ptags(num_of(fa)).folder, "Receipts", "no copy: and is still filed");
	// Killed after the copy; the twin is filed; the retry takes the
	// stray copy, not the newer twin
	var TB = twin("twin b", "<twb@test>");
	var trash_tags = Tags.format({ folder: Tags.TRASH, was: "$Inbox" });
	after = last_msg(alice_path());
	peek(alice_path(), function (mb) {
		if (fresh().copy_msg(TB[0], mb, { tags: trash_tags }) === false)
			throw new Error("copy_msg: " + shared.last_error);
	});
	journal("s" + TB[0], shdr(TB[0]), after, trash_tags);
	var strayb = "p" + (after + 1);
	var fb = one(st.move(["s" + TB[1]], "u:Receipts"), "stray copy: file the twin");
	Kit.eq(one(st.remove(["s" + TB[0]]), "stray copy: retry"), strayb, "stray copy: the retry takes the stray copy");
	Kit.eq(ptags(num_of(fb)).folder, "Receipts", "stray copy: filed twin still filed");
	Kit.eq(count_id(alice_path(), "<twb@test>"), 2, "stray copy: one copy of each delivery");
	ok(st.empty_trash(), "stray copy: empty Trash");
	Kit.eq(count_id(alice_path(), "<twb@test>"), 1, "stray copy: the filed twin survives empty_trash");
	// Restore direction, the first try copies nothing
	var RA = twin("twin ra", "<twra@test>");
	var rta = ok(st.remove(["s" + RA[0], "s" + RA[1]]), "restore, no copy: twins to Trash");
	hooks.move = function () {
		hooks.move = null;
		return false;
	};
	err(st.restore([rta[0].new_id]), 0, "failed", "restore, no copy: first restore fails");
	hooks.move = null;
	var sya = one(st.restore([rta[1].new_id]), "restore, no copy: restore the twin");
	var sza = one(st.restore([rta[0].new_id]), "restore, no copy: retry");
	Kit.assert(sza != sya && shdr(num_of(sya)) !== null && shdr(num_of(sza)) !== null,
		"restore, no copy: two shared copies");
	Kit.eq(live_shared("<twra@test>"), 2, "restore, no copy: both twins in the Inbox");
	Kit.eq(count_id(alice_path(), "<twra@test>"), 0, "restore, no copy: none left in Trash");
	// Restore direction, killed after the copy
	var RB = twin("twin rb", "<twrb@test>");
	var rtb = ok(st.remove(["s" + RB[0], "s" + RB[1]]), "restore, stray copy: twins to Trash");
	after = fresh().last_msg;
	peek(alice_path(), function (mb) {
		if (mb.copy_msg(num_of(rtb[0].new_id), fresh(), { tags: "" }) === false)
			throw new Error("copy_msg back: " + mb.last_error);
	});
	journal(rtb[0].new_id, phdr(num_of(rtb[0].new_id)), after, "");
	var strayrb = "s" + (after + 1);
	var syb = one(st.restore([rtb[1].new_id]), "restore, stray copy: restore the twin");
	Kit.assert(syb != strayrb, "restore, stray copy: the twin gets its own copy");
	Kit.eq(one(st.restore([rtb[0].new_id]), "restore, stray copy: retry"), strayrb, "restore, stray copy: the retry takes the stray");
	Kit.eq(live_shared("<twrb@test>"), 2, "restore, stray copy: both twins in the Inbox");
	Kit.eq(count_id(alice_path(), "<twrb@test>"), 0, "restore, stray copy: none left in Trash");
	// No Message-ID: an interrupted move retried leaves one copy
	var NM = to_alice("no message id", null);
	var nmh = fresh().get_msg_header(false, NM, false);
	delete nmh.id;	// save_msg stored a generated one: drop it
	nmh.id = undefined;
	if (!shared.put_msg_header(false, NM, nmh))
		throw new Error("put_msg_header: " + shared.last_error);
	Kit.eq(shdr(NM).id, undefined, "no stored Message-ID");
	after = last_msg(alice_path());
	peek(alice_path(), function (mb) {
		if (fresh().copy_msg(NM, mb, { tags: "Receipts" }) === false)
			throw new Error("copy_msg: " + shared.last_error);
	});
	journal("s" + NM, shdr(NM), after, "Receipts");
	Kit.eq(one(st.move(["s" + NM], "u:Receipts"), "no-id retry"), "p" + (after + 1), "no-id copy reused");
	Kit.eq(count_subject(alice_path(), "no message id"), 1, "one copy without a Message-ID");
	Kit.assert(shdr(NM) === null, "no-id original removed");
	Kit.eq(ajf.read("moving", {}), {}, "journal empty");

	// --- An unsettled journal entry never lets a same-call twin
	// be taken for its copy. Count journal writes, and let a test make
	// exactly one of them fail as a busy lock would.
	var moving_writes = 0;
	var moving_busy_once = false;
	var real_update = st.jf.update;
	st.jf.update = function (name, fallback, fn) {
		if (name == "moving") {
			moving_writes++;
			if (moving_busy_once) {
				moving_busy_once = false;
				throw { code: "conflict", message: "locked: (test)" };
			}
		}
		return real_update.call(this, name, fallback, fn);
	};
	function trash_and_check(a, filed, label) {
		var t = one(st.remove(["s" + a]), label + ": delete the first");
		Kit.assert(t != filed, label + ": its own Trash copy");
		Kit.eq(ptags(num_of(filed)).folder, "Receipts", label + ": filed twin still filed");
		ok(st.empty_trash(), label + ": empty Trash");
		Kit.eq(ptags(num_of(filed)).folder, "Receipts", label + ": filed twin survives empty_trash");
	}
	ok(st.empty_trash(), "start the same-call twin cases with an empty Trash");
	// move_msg throws for the first, having copied nothing
	var THROWN = twin("twin e1", "<twe1@test>");
	shared_move_once(function () { throw new Error("simulated move_msg exception"); });
	res = st.move(["s" + THROWN[0], "s" + THROWN[1]], "u:Receipts");
	delete st.open_shared;
	err(res, 0, "failed", "move throws: first throws");
	Kit.assert(res[1].ok, "move throws: twin filed in the same call");
	Kit.assert(shdr(THROWN[0]) !== null, "move throws: first still in the Inbox");
	trash_and_check(THROWN[0], res[1].new_id, "move throws");
	Kit.eq(count_id(alice_path(), "<twe1@test>"), 1, "move throws: one copy left");
	// move_msg returns false (no copy) and the settle cannot be
	// written: no further cross-base move in that call
	var UNSAVED = twin("twin e2", "<twe2@test>");
	shared_move_once(function () {
		moving_busy_once = true;
		return false;
	});
	res = st.move(["s" + UNSAVED[0], "s" + UNSAVED[1]], "u:Receipts");
	delete st.open_shared;
	moving_busy_once = false;
	err(res, 0, "failed", "settle not saved: first fails");
	err(res, 1, "conflict", "settle not saved: no move after an unsaved settle");
	Kit.assert(shdr(UNSAVED[0]) !== null && shdr(UNSAVED[1]) !== null, "settle not saved: both still in the Inbox");
	var fe2 = one(st.move(["s" + UNSAVED[1]], "u:Receipts"), "settle not saved: file the twin");
	trash_and_check(UNSAVED[0], fe2, "settle not saved");
	Kit.eq(count_id(alice_path(), "<twe2@test>"), 1, "settle not saved: one copy left");
	// The same two in the restore direction: two twins in Trash, restored
	// in one call
	var RTHROWN = twin("twin r1", "<twr1@test>");
	var rt1 = ok(st.remove(["s" + RTHROWN[0], "s" + RTHROWN[1]]), "restore throws: twins to Trash");
	hooks.move = function () {
		hooks.move = null;
		throw new Error("simulated move_msg exception");
	};
	res = st.restore([rt1[0].new_id, rt1[1].new_id]);
	hooks.move = null;
	err(res, 0, "failed", "restore throws: first throws");
	Kit.assert(res[1].ok, "restore throws: twin restored in the same call");
	var rz1 = one(st.restore([rt1[0].new_id]), "restore throws: retry");
	Kit.assert(rz1 != res[1].new_id, "restore throws: its own shared copy");
	Kit.eq(live_shared("<twr1@test>"), 2, "restore throws: both twins in the Inbox");
	var RUNSAVED = twin("twin r2", "<twr2@test>");
	var rt2 = ok(st.remove(["s" + RUNSAVED[0], "s" + RUNSAVED[1]]), "restore settle not saved: twins to Trash");
	hooks.move = function () {
		hooks.move = null;
		moving_busy_once = true;
		return false;
	};
	res = st.restore([rt2[0].new_id, rt2[1].new_id]);
	hooks.move = null;
	moving_busy_once = false;
	err(res, 0, "failed", "restore settle not saved: first fails");
	err(res, 1, "conflict", "restore settle not saved: no move after an unsaved settle");
	var ry2 = one(st.restore([rt2[1].new_id]), "restore settle not saved: restore the twin");
	var rz2 = one(st.restore([rt2[0].new_id]), "restore settle not saved: retry");
	Kit.assert(rz2 != ry2, "restore settle not saved: its own shared copy");
	Kit.eq(live_shared("<twr2@test>"), 2, "restore settle not saved: both twins in the Inbox");
	Kit.eq(count_id(alice_path(), "<twr2@test>"), 0, "restore settle not saved: none left in Trash");
	// The journal locked for the whole call: cross-base moves get
	// conflict and add nothing; flags and retags still work
	var CX = to_alice("journal locked x", "<cx@test>");
	var CY = to_alice("journal locked y", "<cy@test>");
	var plast = last_msg(alice_path());
	lock = st.jf.lock_path("moving");
	Kit.assert(file_mutex(lock, "held by test", 60), "take journal lock");
	st.jf.timeout = 50;
	try {
		res = st.move(["s" + CX, "s" + CY], "u:Receipts");
		err(res, 0, "conflict", "locked journal: first");
		err(res, 1, "conflict", "locked journal: second");
		ok(st.set(["s" + CX, "p" + PS], { read: true }), "flags with the journal locked");
		one(st.move([fe2], "archive"), "retag with the journal locked");
		one(st.move([fe2], "u:Receipts"), "retag back");
		ok(st.empty_trash(), "empty Trash with the journal locked");
	} finally {
		file_remove(lock);
		st.jf.timeout = 5000;
	}
	Kit.eq(last_msg(alice_path()), plast, "nothing added to the private base");
	Kit.assert(shdr(CX) !== null && shdr(CY) !== null, "both still in the Inbox");
	Kit.assert(shdr(CX).attr & D.MSG_READ, "flag set");
	// A number pinned by one entry is never pinned for another
	// (twins, one entry pinned to the stray copy, a second unpinned entry
	// whose after + 1 is that same copy)
	var DD = twin("twin dd", "<twdd@test>");
	after = last_msg(alice_path());
	peek(alice_path(), function (mb) {
		if (fresh().copy_msg(DD[0], mb, { tags: "Receipts" }) === false)
			throw new Error("copy_msg: " + shared.last_error);
	});
	journal("s" + DD[0], shdr(DD[0]), after, "Receipts");
	journal("s" + DD[1], shdr(DD[1]), after, "Receipts");
	var jd = ajf.read("moving", {});
	jd["s" + DD[0]].copy = after + 1;
	ajf.write("moving", jd);
	var fd1 = one(st.move(["s" + DD[1]], "u:Receipts"), "pinned copy: move the second twin");
	Kit.assert(fd1 != "p" + (after + 1), "pinned copy: the second twin gets its own copy");
	Kit.eq(one(st.move(["s" + DD[0]], "u:Receipts"), "pinned copy: move the first"), "p" + (after + 1),
		"pinned copy: the first reuses its pinned copy");
	Kit.eq(count_id(alice_path(), "<twdd@test>"), 2, "pinned copy: one copy of each twin");

	// No journal write when nothing in it changes
	moving_writes = 0;
	one(st.set(["s" + CX], { starred: true }), "star (no journal write)");
	ok(st.empty_trash(), "empty Trash (no journal write)");
	st.folder_rename("u:Receipts", "Receipts Two");
	st.folder_rename("u:" + Tags.encode("Receipts Two"), "Receipts");
	Kit.eq(moving_writes, 0, "no journal write without a move");
	st.jf.update = real_update;
	delete st.jf.update;

	// --- Rename where every message fails and the folders file
	// stays locked keeps the old id; deleting an empty folder whose
	// folders file stays locked is a conflict
	var allfail = st.folder_create("All Fail").id;
	one(st.move(["s" + to_alice("all fail", "<af@test>")], allfail), "file for all-fail");
	var emptyf = st.folder_create("Empty Folder").id;
	hooks.put = function () { throw new TypeError("simulated failure"); };
	lock = st.jf.lock_path("folders");
	Kit.assert(file_mutex(lock, "held by test", 60), "take folders lock (all fail)");
	st.jf.timeout = 100;
	try {
		rb = st.folder_rename(allfail, "Never");
		Kit.throws(function () { st.folder_delete(emptyf); }, "conflict");
	} finally {
		hooks.put = null;
		file_remove(lock);
		st.jf.timeout = 5000;
	}
	Kit.eq([rb.id, rb.errors.length], [allfail, 1], "all failed: the old id");
	Kit.eq(st.list(allfail).total, 1, "message still in the old folder");
	Kit.assert(st.list(emptyf).total === 0, "empty folder still exists");

	// --- update_folders retries with a short lock timeout
	lock = st.jf.lock_path("folders");
	Kit.assert(file_mutex(lock, "held by test", 60), "take folders lock (timeout)");
	var t_start = system.timer;
	try {
		rb = st.folder_rename(allfail, "Renamed Slowly");
	} finally {
		file_remove(lock);
	}
	Kit.assert(system.timer - t_start < 4.5, "folders retries are short (" + (system.timer - t_start) + " s)");
	Kit.eq(rb.id, "u:" + Tags.encode("Renamed Slowly"), "renamed with folders.json locked");

	// --- No message leaving Store names a server path, whatever
	// lock is held
	var seen = [];
	function collect(fn) {
		try {
			var r = fn();
			var list = r instanceof Array ? r : (r && r.errors instanceof Array ? r.errors : []);
			for (var j = 0; j < list.length; j++)
				if (list[j] && list[j].message !== undefined)
					seen.push(String(list[j].message));
		} catch (e) {
			seen.push(String(e && e.message !== undefined ? e.message : e));
		}
	}
	var names = ["summary", "moving", "folders", "trash", "hidden", "shared", "state"];
	for (i = 0; i < names.length; i++) {
		lock = st.jf.lock_path(names[i]);
		Kit.assert(file_mutex(lock, "held by test", 60), "take " + names[i] + " lock");
		st.jf.timeout = 50;
		try {
			var mi = to_alice("audit " + i, "<audit" + i + "@test>");
			collect(function () { return st.remove(["s" + mi]); });
			collect(function () { return st.set(["s" + TB[1], "p" + PS], { starred: true, read: true }); });
			collect(function () { return st.remove(["s" + EX]); });
			collect(function () { return st.folder_create("Audit " + i); });
			collect(function () { return st.folder_rename("u:Receipts", "Audit R" + i); });
			collect(function () { return st.folder_rename("u:" + Tags.encode("Audit R" + i), "Receipts"); });
			collect(function () { return st.folder_delete("u:" + Tags.encode("Audit " + i)); });
			collect(function () { return st.purge_expired(T0 + (200 + i) * DAY); });
			collect(function () { return st.empty_trash(); });
			collect(function () { return st.folders(); });
			collect(function () { return st.list("inbox"); });
			collect(function () { return st.state(); });
			collect(function () { return st.usage(); });
			collect(function () { return st.get(sya, { mark_read: true }); });
		} finally {
			file_remove(lock);
			st.jf.timeout = 5000;
		}
	}
	// The journal lock taken in the middle of a call (after its reconcile):
	// the per-id conflict must not name the lock file either
	var mj = to_alice("audit journal", "<auditj@test>");
	var jlock = st.jf.lock_path("moving");
	hooks.put = function () {
		hooks.put = null;
		if (!file_mutex(jlock, "held by test", 60))
			throw new Error("cannot take the journal lock");
		return undefined;
	};
	st.jf.timeout = 50;
	try {
		res = st.remove(["p" + PS, "s" + mj]);
	} finally {
		hooks.put = null;
		file_remove(jlock);
		st.jf.timeout = 5000;
	}
	err(res, 1, "conflict", "journal busy mid-call");
	seen.push(res[1].message);
	var bad_store = new Store({ usernum: ALICE, shared_path: S.dir + "no/such/dir/base",
		data_dir: S.data_dir });
	collect(function () { bad_store.open(); });
	Kit.assert(seen.length > 0, "audit collected messages");
	for (i = 0; i < seen.length; i++)
		Kit.assert(seen[i].indexOf(S.dir) < 0 && seen[i].indexOf("locked:") < 0,
			"server path in a message: " + seen[i]);
} finally {
	for (var k = 0; k < stores.length; k++)
		stores[k].close();
	if (shared)
		shared.close();
	S.cleanup();
}
