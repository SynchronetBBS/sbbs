// Store: the mailbox quota covers the Inbox.  usage() and overview(),
// mailbox.usage.ini (the private base's total, for the servers), moves
// and deletes at and over the quota, a Sent copy checked against all of
// the user's mail, and a store without a quota (as for a user with the W
// exemption)

load(js.exec_dir + "kit.js");
var Tags = Kit.lib("mailbox_tags");
var Store = Kit.lib("mailbox");
var JsonFiles = Kit.lib("mailbox_files");
var Summary = Kit.lib("mailbox_summary");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var BOB = 60002;
var CAROL = 60003;
var DAY = 24 * 60 * 60;

var S = Kit.scratch();
var shared = null;
var stores = [];
var temps = 0;
var clock = time();

function open_store(usernum, options)
{
	var st = new Store({ usernum: usernum, alias: "u" + usernum, name: "User " + usernum,
		shared_path: S.shared_path, data_dir: S.data_dir, options: options || {},
		now: function () { return clock; } });
	st.open();
	stores.push(st);
	return st;
}

function mail_to(usernum, subject, extra, body)
{
	if (shared !== null)
		shared.close();
	shared = Kit.email_base(S.shared_path);
	var hdr = { to: "User", to_ext: String(usernum), from: "Dave", subject: subject };
	for (var k in extra || {})
		hdr[k] = extra[k];
	if (!shared.save_msg(hdr, body || "body\r\n"))
		throw new Error("save_msg: " + shared.last_error);
	return shared.last_msg;
}

function body_file(len)
{
	var p = S.dir + "body" + (temps++) + ".txt";
	var f = new File(p);
	Kit.assert(f.open("wb"), "create " + p);
	f.write(new Array(len + 1).join("x") + "\r\n");
	f.close();
	return p;
}

function sent_header(n)
{
	return { to: "Bob", to_net_addr: "bob@example.org", from: "Alice", subject: "sent " + n,
		id: "<sent." + n + "@vert.example>" };
}

function one(res, msg)
{
	Kit.eq(res.length, 1, msg + ": one result");
	Kit.assert(res[0].ok, msg + ": " + JSON.stringify(res[0]));
	return res[0].new_id;
}

function sizes(st, folder)
{
	var total = 0;
	var items = st.list(folder, { limit: 100 }).items;
	for (var i = 0; i < items.length; i++)
		total += items[i].size;
	return total;
}

function usage_path(usernum)
{
	return S.user_dir(usernum) + "mailbox.usage.ini";
}

// The bytes value of the user's mailbox.usage.ini; null without the file
function recorded(usernum)
{
	var f = new File(usage_path(usernum));
	if (!f.open("r"))
		return null;
	var text = f.read();
	f.close();
	var m = text.match(/^bytes=(\d+)\r?\n$/);
	Kit.assert(m !== null, "mailbox.usage.ini is one bytes line: " + JSON.stringify(text));
	return parseInt(m[1], 10);
}

// The private base's total, from its headers (no cache)
function private_bytes(usernum)
{
	var dir = S.user_dir(usernum);
	var scratch = dir + "check/";
	mkpath(scratch);
	var mb = Kit.email_base(dir + "mailbox");
	try {
		return Summary.usage(mb, new JsonFiles(scratch, "mailbox."), { data_dir: S.data_dir }).bytes;
	} finally {
		mb.close();
		directory(scratch + "*").forEach(function (p) { file_remove(p); });
		rmdir(scratch);
	}
}

function in_step(st, usernum, what)
{
	var p = private_bytes(usernum);
	Kit.eq(recorded(usernum), p, "mailbox.usage.ini after " + what);
	Kit.eq(st.usage().bytes, p + sizes(st, "inbox") + sizes(st, "spam"), "usage() after " + what);
}

try {
	var st = open_store(ALICE);

	// --- usage(): the Inbox and Spam count; another user's mail does not
	Kit.eq(st.usage(), { bytes: 0, quota: system.mailbox_quota, full: false }, "nothing yet");
	var A1 = mail_to(ALICE, "one");
	var A2 = mail_to(ALICE, "two", null, new Array(3000).join("y") + "\r\n");
	var SP = mail_to(ALICE, "spam", { attr: D.MSG_SPAM });
	mail_to(BOB, "for bob", null, new Array(9000).join("z") + "\r\n");
	var inbox = sizes(st, "inbox");
	var spam = sizes(st, "spam");
	Kit.assert(inbox >= 512 + 3072 && spam >= 512, "sizes in whole blocks: " + inbox + ", " + spam);
	Kit.eq(st.usage().bytes, inbox + spam, "the Inbox and Spam count");
	Kit.eq(recorded(ALICE), null, "no mailbox.usage.ini without a private base");
	Kit.eq(directory(S.user_dir(ALICE) + "mailbox.usage*").length, 0, "nothing left behind");

	// A file attached to a message in the Inbox counts, as it does once
	// the message is filed
	var in_dir = S.data_dir + format("file/%04u.in/", ALICE);
	mkpath(in_dir);
	var af = new File(in_dir + "report.bin");
	Kit.assert(af.open("wb"), "create the attachment");
	af.write(new Array(5001).join("a"));
	af.close();
	var before_att = st.usage().bytes;
	var AT = mail_to(ALICE, "report.bin", { auxattr: D.MSG_FILEATTACH });
	var att = st.list("inbox").items[0];
	Kit.eq(att.id, "s" + AT, "the attached message is the newest");
	Kit.assert(att.size >= 5000 + 512, "its size counts the file: " + att.size);
	Kit.eq(st.usage().bytes - before_att, att.size, "usage counts the file");

	// --- overview(): folders() and usage() in one call
	var ov = st.overview();
	Kit.eq(ov.folders, st.folders(), "overview folders");
	Kit.eq(ov.usage, st.usage(), "overview usage");

	// --- mailbox.usage.ini follows every change to the private base
	var total = st.usage().bytes;
	var p1 = one(st.move(["s" + A1], "archive"), "file one");
	in_step(st, ALICE, "a move to a folder");
	Kit.assert(recorded(ALICE) >= 512, "the filed message is recorded: " + recorded(ALICE));
	var now = st.usage().bytes;
	Kit.assert(now >= total && now - total <= 256, "filing changes the total by a header block at most");
	var pat = one(st.move(["s" + AT], "archive"), "file the attached message");
	in_step(st, ALICE, "filing an attached message");
	Kit.assert(st.usage().bytes - now <= 256, "filing an attached message does not change the total");
	var p2 = one(st.remove(["s" + A2]), "delete two");
	in_step(st, ALICE, "a delete to Trash");
	one(st.set([p1], { read: true }), "mark read");
	in_step(st, ALICE, "a change of a flag");
	var sent1 = st.save_sent(sent_header(1), body_file(2000));
	in_step(st, ALICE, "a Sent copy");
	one(st.move([p1], "inbox"), "back to the Inbox");
	in_step(st, ALICE, "a move back to the Inbox");
	var with_trash = recorded(ALICE);
	clock += 40 * DAY;
	var purged = st.purge_expired();
	Kit.eq(purged.length, 1, "the Trash message expired");
	in_step(st, ALICE, "a Trash purge by age");
	Kit.assert(recorded(ALICE) < with_trash, "the purge lowered the recorded total");
	one(st.remove([sent1]), "Sent copy to Trash");
	Kit.eq(st.empty_trash().length, 1, "Trash emptied");
	in_step(st, ALICE, "emptying Trash");

	// A missing file is written again by usage(); a flag change does not
	// rewrite a file whose total is unchanged
	Kit.assert(file_remove(usage_path(ALICE)), "remove mailbox.usage.ini");
	st.usage();
	Kit.eq(recorded(ALICE), private_bytes(ALICE), "usage() writes a missing mailbox.usage.ini");
	var marker = new File(usage_path(ALICE));
	Kit.assert(marker.open("w"), "rewrite mailbox.usage.ini");
	marker.write("bytes=1\n");
	marker.close();
	one(st.set([pat], { read: true }), "mark read");
	Kit.eq(recorded(ALICE), 1, "not rewritten for an unchanged total");
	one(st.move([pat], "inbox"), "attached message back to the Inbox");
	Kit.eq(recorded(ALICE), private_bytes(ALICE), "rewritten when the total changes");
	// "*.tmp", not ".*": on Windows "mailbox.usage.ini.*" also matches the file itself
	Kit.eq(directory(S.user_dir(ALICE) + "mailbox.usage.ini.*.tmp").length, 0, "no temp files left");

	// --- Over the quota only because of the Inbox: deleting and filing
	// work, nothing is removed from Trash
	var bob = open_store(BOB, { quota_bytes: 1024 });
	var B1 = mail_to(BOB, "more for bob");
	var B2 = mail_to(BOB, "and more");
	var bu = bob.usage();
	Kit.assert(bu.bytes > 9000 && bu.full === true && bu.quota === 1024, "full by the Inbox alone: "
		+ JSON.stringify(bu));
	Kit.eq(recorded(BOB), null, "Bob has no private base yet");
	var bt = one(bob.remove(["s" + B1]), "delete over the quota");
	var bf = one(bob.move(["s" + B2], "archive"), "file over the quota");
	Kit.eq(bob.list("trash").total, 1, "the deleted message is in Trash");
	Kit.eq(bob.list("archive").total, 1, "the filed message is in Archive");
	Kit.eq(bob.usage().full, true, "still full: Trash counts");
	in_step(bob, BOB, "Bob's moves");

	// --- A Sent copy is checked against the Inbox too
	var used = bob.usage().bytes;
	var room = open_store(BOB, { quota_bytes: used + 1024 });
	Kit.eq(room.usage().full, false, "room for 1024 bytes");
	Kit.throws(function () {
		room.save_sent(sent_header(2), body_file(4000));
	}, "quota");
	Kit.eq(room.list("sent").total, 0, "no copy that would pass the quota");
	Kit.eq(room.list("trash").total, 1, "Trash kept when emptying it would not be enough");
	Kit.eq(room.usage().bytes, used, "nothing changed");
	var s3 = room.save_sent(sent_header(3), body_file(100));
	Kit.eq(room.list("sent").total, 1, "a copy that fits is kept");
	Kit.eq(room.list("trash").total, 1, "Trash kept when the copy fits");
	in_step(room, BOB, "Bob's Sent copy");
	// The private base alone is far under this quota: only with the Inbox
	// is there no room, and the oldest Trash goes to make it
	used = room.usage().bytes;
	var trash_size = sizes(room, "trash");
	Kit.assert(recorded(BOB) + 1024 < used, "most of Bob's mail is in the Inbox");
	var tight = open_store(BOB, { quota_bytes: used + 256 });
	var s4 = tight.save_sent(sent_header(4), body_file(100));
	Kit.eq(tight.list("trash").total, 0, "Trash removed to make room for a Sent copy");
	Kit.eq(tight.list("sent").total, 2, "the copy is kept");
	Kit.assert(tight.usage().bytes <= used + 256, "within the quota");
	Kit.eq(tight.usage().bytes, used - trash_size + tight.get(s4).size, "the total with the Inbox");
	in_step(tight, BOB, "making room");

	// --- No quota (as for a user with the W exemption): nothing refused,
	// no Trash removed, usage without a limit
	var carol = open_store(CAROL, { quota_bytes: 0 });
	var C1 = mail_to(CAROL, "c1", null, new Array(20000).join("c") + "\r\n");
	var C2 = mail_to(CAROL, "c2");
	var C3 = mail_to(CAROL, "c3");
	one(carol.remove(["s" + C2]), "Carol deletes");
	one(carol.move(["s" + C1], "archive"), "Carol files");
	carol.save_sent(sent_header(5), body_file(30000));
	Kit.eq(carol.list("trash").total, 1, "no Trash removed");
	Kit.eq(carol.list("sent").total, 1, "the Sent copy is kept");
	var cu = carol.usage();
	Kit.assert(cu.bytes > 50000 && cu.quota === 0 && cu.full === false, "usage without a limit: "
		+ JSON.stringify(cu));
	in_step(carol, CAROL, "Carol's changes");
	// The same mailbox under a small quota is full, and keeps no Sent copy
	var limited = open_store(CAROL, { quota_bytes: 4096 });
	Kit.eq(limited.usage().full, true, "full under a quota");
	Kit.throws(function () {
		limited.save_sent(sent_header(6), body_file(100));
	}, "quota");
	Kit.eq(limited.list("trash").total, 1, "Trash kept: removing it would not be enough");
} finally {
	for (var k = 0; k < stores.length; k++)
		stores[k].close();
	if (shared)
		shared.close();
	S.cleanup();
}
