// Store, read side: folders, lists, search, ownership, delivery status
// and the change fingerprint

load(js.exec_dir + "kit.js");
var JsonFiles = Kit.lib("mailbox_files");
var Tags = Kit.lib("mailbox_tags");
var Store = Kit.lib("mailbox");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var BOB = 60002;
// Mail addressed to user 125537 has 60001 (Alice) in the index's 16-bit
// user number field (125537 & 0xFFFF == 60001); its header names Carl
var CARL = 125537;

var S = Kit.scratch();
var shared = null;
var priv = null;
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

function inet(hdr, addr)	// a recipient on the Internet (no user number)
{
	hdr.to_net_type = D.NET_INTERNET;
	hdr.to_net_addr = addr;
	return hdr;
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

function folder(folders, id)
{
	for (var i = 0; i < folders.length; i++)
		if (folders[i].id == id)
			return folders[i];
	return null;
}

function open_store(usernum)
{
	var st = new Store({ usernum: usernum, alias: "u" + usernum, name: "User " + usernum,
		shared_path: S.shared_path, data_dir: S.data_dir });
	st.open();
	stores.push(st);
	return st;
}

try {
	shared = Kit.email_base(S.shared_path);
	// Every subject holds "zz" so a search for it matches everything
	var A_read = save(shared, { to_ext: String(ALICE), from: "Carol", subject: "Hello Alice zz",
		attr: D.MSG_READ });
	var A_star = save(shared, { to_ext: String(ALICE), from: "Dave", subject: "starred zz",
		attr: D.MSG_VALIDATED });
	var B_mail = save(shared, { to_ext: String(BOB), from: "Eve", subject: "secret for bob zz" });
	var A_spam = save(shared, { to_ext: String(ALICE), from: "Spammer", subject: "Buy now zz",
		attr: D.MSG_SPAM });
	var A_del = save(shared, { to_ext: String(ALICE), subject: "gone zz" });
	if (!shared.remove_msg(A_del))
		throw new Error("remove_msg: " + shared.last_error);
	var E_extra = save(shared, { from_ext: String(ALICE), from: "Alice", to_ext: String(BOB),
		to: "Bob", subject: "terminal sent zz", id: "<e@test>" });
	var E_hidden = save(shared, { from_ext: String(ALICE), from: "Alice", to_ext: String(BOB),
		to: "Bob", subject: "hidden sent zz", id: "<h@test>" });
	var Q1 = save(shared, inet({ from_ext: String(ALICE), from: "Alice", subject: "queued zz",
		id: "<q@test>", delivery_attempts: 2 }, "x@example.com"));
	var Q2 = save(shared, inet({ from_ext: String(ALICE), from: "Alice", subject: "queued zz",
		id: "<q@test>", delivery_attempts: 3 }, "y@example.com"));
	var Q3 = save(shared, inet({ from_ext: String(ALICE), from: "Alice", subject: "queued zz",
		id: "<q@test>", netattr: D.NETMSG_SENT, delivery_attempts: 1 }, "z@example.com"));
	var notice = save(shared, { to_ext: String(ALICE), from: "Mail Delivery Subsystem",
		subject: "Undeliverable zz", attr: D.MSG_NOREPLY, reply_id: "<f@test>" });
	// Bob's mail that must never show up for Alice
	var B_net = save(shared, inet({ from_ext: String(BOB), from: "Bob", subject: "bob out zz",
		id: "<b@test>" }, "w@example.com"));
	var B_notice = save(shared, { to_ext: String(BOB), from: "Mail Delivery Subsystem",
		subject: "bob bounce zz", attr: D.MSG_NOREPLY, reply_id: "<s@test>" });
	var bobs = ["s" + B_mail, "s" + B_net, "s" + B_notice];
	// Carl's mail, in Alice's index slot
	var C_in = save(shared, { to_ext: String(CARL), from: "Frank", subject: "carl inbox zz" });
	var C_out = save(shared, inet({ from_ext: String(CARL), from: "Carl", subject: "carl sent zz",
		id: "<c@test>" }, "v@example.com"));
	var carls = ["s" + C_in, "s" + C_out];
	bobs = bobs.concat(carls);	// none of these may show for Alice

	// Alice's private base and side files
	var adir = S.user_dir(ALICE);
	var jf = new JsonFiles(adir, "mailbox.");
	priv = Kit.email_base(adir + "mailbox");
	function psave(tags, subject, id, attr) {
		var h = inet({ from_ext: String(ALICE), from: "Alice", subject: subject, tags: tags },
			"r@example.com");
		if (id)
			h.id = id;
		if (attr)
			h.attr = attr;
		return save(priv, h);
	}
	var P_q = psave(Tags.SENT, "copy q zz", "<q@test>", D.MSG_READ);
	var P_f = psave(Tags.SENT, "copy f zz", "<f@test>", D.MSG_READ);
	var P_s = psave(Tags.SENT, "copy s zz", "<s@test>", D.MSG_READ);
	var P_u = psave(Tags.SENT + " " + Tags.UNSENT, "copy u zz", "<u@test>", D.MSG_READ);
	var P_a1 = psave(Tags.ARCHIVE, "archived zz");
	var P_a2 = psave(Tags.ARCHIVE, "archived starred zz", null, D.MSG_READ | D.MSG_VALIDATED);
	var P_t1 = psave(Tags.TRASH + " " + Tags.WAS + Tags.ARCHIVE, "trashed zz");
	var P_t2 = psave(Tags.TRASH, "trashed starred zz", null, D.MSG_VALIDATED);
	var P_r = psave(Tags.encode("Receipts"), "receipt zz");
	var P_x = psave(Tags.encode("Tax Stuff"), "taxes zz");
	priv.close();
	priv = null;
	jf.write("folders", [Tags.encode("Receipts"), Tags.encode("Empty")]);
	jf.write("hidden", ["s" + E_hidden]);

	var st = open_store(ALICE);

	// Folders, counts and order
	var F = st.folders();
	Kit.eq(F.map(function (f) { return f.id; }), ["inbox", "starred", "sent", "spam", "trash",
		"archive", "u:Receipts", "u:Empty", "u:" + Tags.encode("Tax Stuff")], "folder order");
	Kit.eq(folder(F, "inbox"), { id: "inbox", name: "", kind: "builtin", total: 3, unread: 2 }, "inbox");
	Kit.eq([folder(F, "spam").total, folder(F, "spam").unread], [1, 1], "spam");
	Kit.eq([folder(F, "starred").total, folder(F, "starred").unread], [2, 1], "starred");
	Kit.eq([folder(F, "sent").total, folder(F, "sent").unread], [5, 0], "sent");
	Kit.eq([folder(F, "trash").total, folder(F, "archive").total], [2, 2], "trash, archive");
	Kit.eq(folder(F, "u:Receipts"), { id: "u:Receipts", name: "Receipts", kind: "user", total: 1, unread: 1 },
		"user folder");
	Kit.eq(folder(F, "u:Empty").total, 0, "empty user folder");
	Kit.eq(folder(F, "u:" + Tags.encode("Tax Stuff")).name, "Tax Stuff", "tag-only folder name");

	// Membership
	Kit.eq(ids(st.list("inbox")), ["s" + notice, "s" + A_star, "s" + A_read], "inbox list");
	Kit.eq(ids(st.list("spam")), ["s" + A_spam], "spam list");
	Kit.eq(ids(st.list("starred")).sort(), ["p" + P_a2, "s" + A_star].sort(), "starred list");
	Kit.eq(ids(st.list("archive")), ["p" + P_a2, "p" + P_a1], "archive list");
	Kit.eq(ids(st.list("trash")), ["p" + P_t2, "p" + P_t1], "trash list");
	Kit.eq(ids(st.list("u:Receipts")), ["p" + P_r], "user folder list");
	Kit.eq(ids(st.list("u:Empty")), [], "empty folder list");
	Kit.throws(function () { st.list("u:Nope"); }, "notfound");
	Kit.throws(function () { st.list("bogus"); }, "invalid");
	Kit.throws(function () { st.list("all"); }, "invalid");

	var I = st.list("inbox").items;
	var a = I[2];
	Kit.eq([a.from, a.to, a.subject, a.read, a.replied, a.starred, a.folder], ["Carol", "Rcpt",
		"Hello Alice zz", true, false, false, "inbox"], "inbox item fields");
	Kit.assert(typeof a.date == "number" && typeof a.arrived == "number" && typeof a.size == "number"
		&& a.size > 0 && a.priority === "" && a.attach === false, "inbox item types");
	Kit.assert(I[1].starred === true, "shared starred flag");

	// Sent, extras and delivery status
	var sent = st.list("sent");
	Kit.eq(sent.total, 5, "sent total");
	Kit.eq(ids(sent).sort(), ["p" + P_f, "p" + P_q, "p" + P_s, "p" + P_u, "s" + E_extra].sort(),
		"sent membership (hidden extra and copied ones excluded)");
	Kit.eq(item(sent, "p" + P_q).delivery, { state: "queued", waiting: 2, attempts: 3 }, "queued");
	Kit.eq(item(sent, "p" + P_s).delivery, { state: "sent" }, "sent (Bob's notice ignored)");
	Kit.eq(item(sent, "p" + P_f).delivery, { state: "failed", notice: "s" + notice }, "failed by notice");
	// the notice's id costs no header read: a warm Sent page re-reads only
	// the outbound copies still queued (P_q's two)
	var sent_reads = 0;
	st.shared_header = function (num) {
		sent_reads++;
		return Store.prototype.shared_header.call(this, num);
	};
	Kit.eq(item(st.list("sent"), "p" + P_f).delivery.notice, "s" + notice, "warm: the notice id");
	Kit.eq(sent_reads, 2, "warm Sent page: only the two queued copies re-read");
	delete st.shared_header;
	Kit.eq(item(sent, "p" + P_u).delivery, { state: "unsent" }, "unsent");
	Kit.eq([item(sent, "s" + E_extra).extra, item(sent, "s" + E_extra).delivery.state], [true, "sent"],
		"extra");
	Kit.assert(!item(sent, "p" + P_q).extra, "copy is not extra");
	priv = Kit.email_base(adir + "mailbox");
	Kit.assert(Tags.parse(priv.get_msg_header(false, P_f, false).tags).failed, "$Failed added");
	priv.close();
	priv = null;
	if (!shared.remove_msg(notice))
		throw new Error("remove notice: " + shared.last_error);
	Kit.eq(item(st.list("sent"), "p" + P_f).delivery, { state: "failed" }, "failed after notice removed");
	Kit.eq(st.folders()[0].total, 2, "inbox after notice removed");

	// Paging
	var pg = st.list("sent", { offset: 1, limit: 2 });
	Kit.eq([pg.total, pg.items.length], [5, 2], "page");
	Kit.eq(ids(pg), ids(st.list("sent")).slice(1, 3), "page slice");
	Kit.eq(st.list("sent", { offset: 5 }).items.length, 0, "past the end");

	// Search
	Kit.eq(ids(st.list("all", { search: "HELLO" })), ["s" + A_read], "search all, case");
	Kit.eq(ids(st.list("inbox", { search: "dave" })), ["s" + A_star], "search from");
	Kit.eq(st.list("all", { search: "secret" }).total, 0, "search miss (Bob's)");
	Kit.eq(st.list("all", { search: "Buy now" }).total, 0, "all excludes spam");
	Kit.eq(st.list("all", { search: "trashed" }).total, 0, "all excludes trash");
	Kit.eq(st.list("spam", { search: "buy" }).total, 1, "search spam");
	Kit.eq(st.list("all", { search: "example.com" }).total, 0, "from_addr is the sender's");
	var all = st.list("all", { search: "zz", limit: 100 });
	// inbox 2 + sent 5 + archive 2 + Receipts 1 + Tax Stuff 1
	Kit.eq(all.total, 11, "search all total");

	// Nothing of Bob's, anywhere
	var lists = [all, st.list("inbox"), st.list("spam"), st.list("starred"), st.list("sent"),
		st.list("trash"), st.list("archive")];
	for (var i = 0; i < lists.length; i++)
		for (var j = 0; j < bobs.length; j++)
			Kit.assert(ids(lists[i]).indexOf(bobs[j]) < 0, "Bob's " + bobs[j] + " in list " + i);

	// resolve: ownership and ids
	var r = st.resolve("s" + A_read);
	Kit.eq([r.kind, r.number, r.role, r.header.subject], ["s", A_read, "to", "Hello Alice zz"], "resolve");
	Kit.eq(st.resolve("s" + E_extra).role, "from", "resolve sent extra");
	Kit.eq(st.resolve("p" + P_r).kind, "p", "resolve private");
	Kit.throws(function () { st.resolve("s" + B_mail); }, "forbidden");
	Kit.throws(function () { st.resolve("s" + B_net); }, "forbidden");
	Kit.throws(function () { st.resolve("s999999"); }, "notfound");
	Kit.throws(function () { st.resolve("s" + A_del); }, "notfound");
	Kit.throws(function () { st.resolve("p999999"); }, "notfound");
	var bad = ["x1", "s0", "s01", "s", "p-1", "s1 ", "S1", "s12345678901", 5, null];
	for (i = 0; i < bad.length; i++)
		Kit.throws(function () { st.resolve(bad[i]); }, "invalid");

	// A user with no private base and no user directory: reads may create
	// the directory and derived caches, never the base, folders or hidden
	var bst = open_store(BOB);
	var bob_reads = 0;
	bst.shared_header = function (num) {
		bob_reads++;
		return Store.prototype.shared_header.call(this, num);
	};
	var BF = bst.folders();
	Kit.eq([folder(BF, "inbox").total, folder(BF, "archive").total, folder(BF, "trash").total,
		folder(BF, "sent").total, BF.length], [4, 0, 0, 1, 6], "Bob's folders");
	Kit.eq(bst.list("archive").total, 0, "Bob's archive");
	Kit.eq(item(bst.list("sent"), "s" + B_net).delivery, { state: "queued", waiting: 1, attempts: 0 },
		"Bob's queued extra");
	var bstate = bst.state();
	Kit.assert(typeof bstate == "string" && bstate.length > 0, "Bob's state");
	for (i = 0; i < BF.length; i++)
		bst.list(BF[i].id);
	bst.state();
	var bdir = S.data_dir + "user/" + format("%04u", BOB) + "/";
	var bad_files = ["mailbox.shd", "mailbox.sid", "mailbox.sdt", "mailbox.sha", "mailbox.sda",
		"mailbox.hash", "mailbox.ini", "mailbox.folders.json", "mailbox.hidden.json"];
	for (i = 0; i < bad_files.length; i++)
		Kit.assert(!file_exists(bdir + bad_files[i]), "read created " + bad_files[i]);
	Kit.assert(bob_reads > 0, "Bob's first reads counted");
	bob_reads = 0;
	BF = bst.folders();
	Kit.eq(bob_reads, 0, "second folders() reads no shared headers");
	Kit.eq([folder(BF, "archive").total, folder(BF, "trash").total], [0, 0], "Bob's private folders");
	Kit.throws(function () { bst.resolve("s" + A_read); }, "forbidden");

	// Records whose index field matches Alice but whose header names Carl
	// (not listed for her: see "bobs" above) cannot be opened by her
	Kit.eq([shared.get_msg_index(false, C_in).to, shared.get_msg_index(false, C_out).from],
		[ALICE, ALICE], "Carl's mail has Alice's number in the index");
	Kit.throws(function () { st.resolve("s" + C_in); }, "forbidden");
	Kit.throws(function () { st.resolve("s" + C_out); }, "forbidden");
	Kit.eq(st.folders().length, F.length, "Carl's mail adds no folder");
	// A user number above 65535 does not fit the index: refused
	Kit.throws(function () { open_store(CARL); }, "invalid");
	Kit.throws(function () { open_store(65536); }, "invalid");
	Kit.eq(open_store(65535).usernum, 65535, "65535 accepted");

	// state(): changes with Alice's mail only
	var s1 = st.state();
	Kit.eq(st.state(), s1, "state stable");
	save(shared, { to_ext: String(BOB), subject: "for bob" });
	Kit.eq(st.state(), s1, "state after mail to Bob");
	save(shared, { to_ext: String(ALICE), subject: "for alice" });
	var s2 = st.state();
	Kit.assert(s2 != s1, "state after mail to Alice");
	priv = Kit.email_base(adir + "mailbox");
	save(priv, { tags: Tags.ARCHIVE, subject: "filed" });
	priv.close();
	priv = null;
	Kit.assert(st.state() != s2, "state after a private save");

	// A shared header's tags changed by the library: forget_shared()
	var s3 = st.state();
	var h = shared.get_msg_header(false, A_read, false);
	h.tags = "work";
	if (!shared.put_msg_header(false, A_read, h))
		throw new Error("put_msg_header: " + shared.last_error);
	st.forget_shared([A_read]);
	Kit.assert(st.state() != s3, "state after forget_shared");
	st.list("inbox");
	Kit.eq(jf.read("shared", null).recs[A_read][7][0], "work", "fact refreshed after forget_shared");

	// A cached fact whose time no longer matches the index is discarded
	var cache = jf.read("shared", null);
	Kit.assert(cache && cache.recs[A_star] instanceof Array, "fact cached");
	cache.recs[A_star][0] = 1;	// time
	cache.recs[A_star][7][4] = "stale";	// the row's subject
	jf.write("shared", cache);
	Kit.eq(jf.read("shared", null).recs[A_star][7][4], "stale", "stale fact written");
	Kit.eq(item(st.list("inbox"), "s" + A_star).subject, "starred zz", "stale fact discarded");

	// Lost update: one session reads facts, another retags the message and
	// calls forget_shared() before the first saves; the stale fact must not
	// be saved over the invalidation
	var st2 = open_store(ALICE);
	var N = save(shared, { to_ext: String(ALICE), subject: "star me later" });
	var raced = false;
	st.read_fact = function (rec) {
		var f = Store.prototype.read_fact.call(this, rec);
		if (rec.number == N && !raced) {
			raced = true;
			var nh = shared.get_msg_header(false, N, false);
			nh.tags = "work";
			if (!shared.put_msg_header(false, N, nh))
				throw new Error("put_msg_header: " + shared.last_error);
			st2.forget_shared([N]);
		}
		return f;
	};
	st.folders();
	Kit.assert(raced, "race ran");
	delete st.read_fact;
	st.list("inbox");
	Kit.eq(jf.read("shared", null).recs[N][7][0], "work", "no lost update");

	// limit is clamped to 100
	for (i = 0; i < 101; i++)
		save(shared, { to_ext: String(ALICE), subject: "bulk " + i });
	var big = st.list("inbox", { limit: 1000 });
	Kit.eq([big.total, big.items.length], [105, 100], "limit clamp");

	// Mail to oneself is in Inbox and in Sent, once in "all"
	var self = save(shared, { to_ext: String(ALICE), from_ext: String(ALICE), from: "Alice",
		to: "Alice", subject: "note to self", id: "<self@test>" });
	Kit.assert(ids(st.list("inbox")).indexOf("s" + self) >= 0, "self in inbox");
	var ss = item(st.list("sent"), "s" + self);
	Kit.assert(ss !== null && ss.extra === true, "self in sent as extra");
	Kit.eq(ids(st.list("all", { search: "note to self" })), ["s" + self], "self once in all");

	// $Failed cannot be written (summary lock held): the read still works,
	// reports failed, and the tag is added on a later call
	priv = Kit.email_base(adir + "mailbox");
	var P_g = psave(Tags.SENT, "copy g", "<g@test>", D.MSG_READ);
	priv.close();
	priv = null;
	Kit.eq(item(st.list("sent"), "p" + P_g).delivery, { state: "sent" }, "g before notice");
	var notice_g0 = save(shared, { to_ext: String(ALICE), from: "Mail Delivery Subsystem",
		subject: "bounce g", attr: D.MSG_NOREPLY, reply_id: "<g@test>" });
	var notice_g = save(shared, { to_ext: String(ALICE), from: "Mail Delivery Subsystem",
		subject: "bounce g again", attr: D.MSG_NOREPLY, reply_id: "<g@test>" });
	Kit.assert(notice_g > notice_g0, "the second notice is newer");
	var lock = st.jf.lock_path("summary");
	Kit.assert(file_mutex(lock, "held by test", 60), "take summary lock");
	st.jf.timeout = 200;
	try {
		Kit.eq(item(st.list("sent"), "p" + P_g).delivery, { state: "failed", notice: "s" + notice_g },
			"failed while locked: the newest notice");
	} finally {
		file_remove(lock);
	}
	priv = Kit.email_base(adir + "mailbox");
	Kit.assert(!Tags.parse(priv.get_msg_header(false, P_g, false).tags).failed, "not tagged while locked");
	priv.close();
	priv = null;
	Kit.eq(item(st.list("sent"), "p" + P_g).delivery, { state: "failed", notice: "s" + notice_g },
		"failed after unlock");
	if (!shared.remove_msg(notice_g))
		throw new Error("remove notice g: " + shared.last_error);
	Kit.eq(item(st.list("sent"), "p" + P_g).delivery, { state: "failed", notice: "s" + notice_g0 },
		"the newest notice deleted: the older one");
	priv = Kit.email_base(adir + "mailbox");
	Kit.assert(Tags.parse(priv.get_msg_header(false, P_g, false).tags).failed, "tagged after unlock");
	priv.close();
	priv = null;
} finally {
	for (var k = 0; k < stores.length; k++)
		stores[k].close();
	if (priv)
		priv.close();
	if (shared)
		shared.close();
	S.cleanup();
}
