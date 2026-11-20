// Store: Sent copies for a sending client (save_sent, mark_sent,
// remove_sent), the replied attribute (set_replied) and a Sent copy's BCC
// list (get().headers.bcc)

load(js.exec_dir + "kit.js");
var Tags = Kit.lib("mailbox_tags");
var Store = Kit.lib("mailbox");
var JsonFiles = Kit.lib("mailbox_files");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var BOB = 60002;

var S = Kit.scratch();
var shared = null;
var stores = [];
var temps = [];

function open_store(usernum, options)
{
	var st = new Store({ usernum: usernum, alias: "u" + usernum, name: "User " + usernum,
		shared_path: S.shared_path, data_dir: S.data_dir, options: options || {} });
	st.open();
	stores.push(st);
	return st;
}

function fresh_shared()
{
	if (shared !== null)
		shared.close();
	shared = Kit.email_base(S.shared_path);
	return shared;
}

function save_shared(hdr, body)
{
	var mb = fresh_shared();
	if (hdr.from === undefined)
		hdr.from = "Carol";
	if (hdr.to === undefined)
		hdr.to = "Alice";
	if (!mb.save_msg(hdr, body || "body\r\n"))
		throw new Error("save_msg: " + mb.last_error);
	return mb.last_msg;
}

function body_file(text)
{
	var p = S.dir + "body" + temps.length + ".txt";
	var f = new File(p);
	Kit.assert(f.open("wb"), "create " + p);
	f.write(text);
	f.close();
	temps.push(p);
	return p;
}

function phdr(usernum, num)
{
	var mb = Kit.email_base(S.user_dir(usernum) + "mailbox");
	try {
		var h = mb.get_msg_header(false, num, false);
		if (h === null || h === undefined || h.number != num || (h.attr & D.MSG_DELETE))
			return null;
		return { tags: h.tags || "", from_ext: h.from_ext, to_ext: h.to_ext, attr: h.attr, id: h.id,
			to_list: h.to_list, subject: h.subject };
	} finally {
		mb.close();
	}
}

function ids(page)
{
	var out = [];
	for (var i = 0; i < page.items.length; i++)
		out.push(page.items[i].id);
	return out;
}

function item(page, id)
{
	for (var i = 0; i < page.items.length; i++)
		if (page.items[i].id === id)
			return page.items[i];
	return null;
}

function sent_header(extra)
{
	var h = { to: "Bob", to_list: "Bob <bob@example.org>, Carl <carl@example.org>",
		from: "Alice", subject: "Hello", id: "<5F00AA01.7@vert.example>", mime_version: "1.0",
		content_type: "text/plain; charset=utf-8", auxattr: D.MSG_HFIELDS_UTF8,
		field_list: [{ type: D.RFC822HEADER, data: "Content-Transfer-Encoding: 8bit" },
			{ type: D.RFC822HEADER, data: "Bcc: Dee <dee@example.org>" }] };
	for (var k in extra || {})
		h[k] = extra[k];
	return h;
}

try {
	fresh_shared();
	var st = open_store(ALICE);

	// --- save_sent: creates the private base, tags $Sent $Unsent
	Kit.assert(!file_exists(S.user_dir(ALICE) + "mailbox.shd"), "no private base yet");
	var input = sent_header({ to_ext: String(ALICE), from_ext: "999" });
	var before = JSON.stringify(input);
	var id = st.save_sent(input, body_file("Hi Bob \xc3\xa9\r\n"));
	Kit.eq(JSON.stringify(input), before, "the caller's header is not changed");
	Kit.assert(/^p[1-9][0-9]*$/.test(id), "a private id: " + id);
	var num = parseInt(id.substr(1), 10);
	var h = phdr(ALICE, num);
	Kit.eq(h.tags, "$Sent $Unsent", "tags");
	Kit.eq(h.from_ext, String(ALICE), "from_ext is the user");
	Kit.eq(h.to_ext, undefined, "no to_ext");
	Kit.assert(h.attr & D.MSG_READ, "marked read");
	Kit.eq(h.id, "<5F00AA01.7@vert.example>", "Message-ID kept");

	var page = st.list("sent");
	Kit.eq(ids(page), [id], "listed in Sent");
	Kit.eq(item(page, id).delivery, { state: "unsent" }, "shown as not sent");
	Kit.eq(st.list("inbox").total, 0, "not in the Inbox");

	var g = st.get(id);
	Kit.eq(g.headers.bcc, "Dee <dee@example.org>", "BCC kept in the Sent copy");
	Kit.eq(g.headers.to, "Bob <bob@example.org>, Carl <carl@example.org>", "To list");
	Kit.eq(base64_decode(g.text.b64), "Hi Bob \xc3\xa9\r\n", "body read back");
	Kit.eq(g.folder, "sent", "in Sent");

	// A Sent copy cannot be moved to the Inbox (it has no to_ext)
	var mv = st.move([id], "inbox");
	Kit.eq(mv[0].ok, false, "a Sent copy cannot go to the Inbox");

	// --- mark_sent: $Unsent removed, idempotent
	Kit.eq(st.mark_sent(id), true, "mark_sent");
	Kit.eq(phdr(ALICE, num).tags, "$Sent", "tags after mark_sent");
	Kit.eq(item(st.list("sent"), id).delivery, { state: "sent" }, "shown as sent");
	Kit.eq(st.mark_sent(id), true, "mark_sent again");
	Kit.eq(phdr(ALICE, num).tags, "$Sent", "unchanged");

	// remove_sent refuses a delivered copy
	Kit.throws(function () { st.remove_sent(id); }, "invalid");
	Kit.assert(phdr(ALICE, num) !== null, "a delivered copy is kept");

	// --- mark_unknown: an unsent copy whose outcome is unknown
	var idu = st.save_sent(sent_header({ id: "<5F00AA09.1@vert.example>" }), body_file("u\r\n"));
	Kit.eq(st.mark_unknown(idu), true, "mark_unknown");
	Kit.eq(phdr(ALICE, parseInt(idu.substr(1), 10)).tags, "$Sent $Unsent $Unknown", "tags");
	Kit.eq(item(st.list("sent"), idu).delivery, { state: "unknown" }, "listed as unknown");
	Kit.throws(function () { st.mark_unknown(id); }, "invalid");	// delivered: no
	Kit.eq(st.mark_sent(idu), true, "mark_sent");
	Kit.eq(phdr(ALICE, parseInt(idu.substr(1), 10)).tags, "$Sent", "both flags cleared");
	Kit.throws(function () { st.mark_unknown(idu); }, "invalid");

	// --- unsent: false gives $Sent alone
	var id2 = st.save_sent(sent_header({ id: "<5F00AA02.8@vert.example>" }), body_file("two\r\n"),
		{ unsent: false });
	Kit.eq(phdr(ALICE, parseInt(id2.substr(1), 10)).tags, "$Sent", "unsent false");

	// --- remove_sent removes an unsent copy for real
	var id3 = st.save_sent(sent_header({ id: "<5F00AA03.9@vert.example>" }), body_file("three\r\n"));
	Kit.eq(st.remove_sent(id3), true, "remove_sent");
	Kit.eq(phdr(ALICE, parseInt(id3.substr(1), 10)), null, "removed");
	Kit.eq(ids(st.list("sent")).indexOf(id3), -1, "not listed");
	Kit.throws(function () { st.remove_sent(id3); }, "notfound");
	Kit.throws(function () { st.mark_sent(id3); }, "notfound");

	// Shared ids and bad ids are refused
	var s1 = save_shared({ to_ext: String(ALICE), subject: "to alice" });
	Kit.throws(function () { st.remove_sent("s" + s1); }, "invalid");
	Kit.throws(function () { st.mark_sent("s" + s1); }, "invalid");
	Kit.throws(function () { st.remove_sent("x1"); }, "invalid");
	Kit.throws(function () { st.save_sent(null, body_file("x")); }, "invalid");
	Kit.throws(function () { st.save_sent(sent_header(), S.dir + "no-such-file"); }, "invalid");

	// get(): bcc is "" for a shared message
	Kit.eq(st.get("s" + s1).headers.bcc, "", "no bcc on shared mail");
	// ... and for a private message that is not a Sent copy
	var arch = st.save_sent(sent_header({ id: "<arch.1@vert.example>" }), body_file("arch\r\n"));
	Kit.eq(st.move([arch], "archive")[0].ok, true, "filed in Archive");
	Kit.eq(st.get(arch).headers.bcc, "", "no bcc off a $Sent message");
	var trashed = st.remove([id2])[0].new_id;
	Kit.eq(st.get(trashed).headers.bcc, "Dee <dee@example.org>", "bcc in Trash from Sent");
	Kit.eq(st.restore([trashed])[0].ok, true, "back to Sent");

	// --- find_sent: $Sent copies by Message-ID
	Kit.eq(st.find_sent("<5F00AA02.8@vert.example>"), [id2], "found by Message-ID");
	Kit.eq(st.find_sent("<arch.1@vert.example>"), [], "not a $Sent message");
	Kit.eq(st.find_sent("<none@vert.example>"), [], "none");
	Kit.throws(function () { st.find_sent(""); }, "invalid");
	Kit.eq(open_store(BOB + 1000).find_sent("<x@y>"), [], "no private base");

	// --- a failed save leaves nothing and says nothing about paths
	var sizes = phdr(ALICE, num) !== null;
	var nul = S.dir + "nul.txt";
	var nf = new File(nul);
	nf.open("wb");
	nf.write("a\x00b");
	nf.close();
	temps.push(nul);
	var count = st.list("sent").total;
	var caught = null;
	try {
		st.save_sent(sent_header({ id: "<x.1@vert.example>" }), nul);
	} catch (e) {
		caught = e;
	}
	Kit.assert(caught !== null && caught.code === "failed", "a body with a NUL fails: " + JSON.stringify(caught));
	Kit.assert(caught.message.indexOf(S.dir) < 0, "no path in the message");
	Kit.eq(st.list("sent").total, count, "nothing saved");

	// --- quota: the oldest Trash makes room; else quota and nothing saved
	var small = open_store(BOB, { quota_bytes: 4096 });
	var big = "";
	for (var i = 0; i < 40; i++)
		big += "0123456789012345678901234567890123456789012345678901234567890123456789\r\n";
	var bid = small.save_sent(sent_header({ to: "Alice", id: "<b.1@vert.example>" }), body_file(big));
	Kit.assert(small.usage().bytes <= 4096, "under quota: " + small.usage().bytes);
	var rm = small.remove([bid]);
	Kit.eq(rm[0].ok, true, "to Trash");
	var bid2 = small.save_sent(sent_header({ to: "Alice", id: "<b.2@vert.example>" }), body_file(big));
	Kit.eq(small.list("trash").total, 0, "Trash emptied to make room");
	Kit.eq(ids(small.list("sent")), [bid2], "the new copy saved");
	var huge = big + big + big;
	Kit.throws(function () {
		small.save_sent(sent_header({ to: "Alice", id: "<b.3@vert.example>" }), body_file(huge));
	}, "quota");
	Kit.eq(small.list("sent").total, 1, "nothing saved over quota");

	// --- set_replied: the user's own mail only
	var page0 = st.list("inbox");
	Kit.eq(item(page0, "s" + s1).replied, false, "not replied yet");
	Kit.eq(st.set_replied("s" + s1), true, "set_replied on shared mail");
	Kit.eq(item(st.list("inbox"), "s" + s1).replied, true, "replied");
	Kit.eq(st.set_replied("s" + s1), true, "again");
	Kit.eq(st.set_replied(id), true, "set_replied on a private message");
	Kit.eq(item(st.list("sent"), id).replied, true, "private replied");
	var to_bob = save_shared({ to_ext: String(BOB), to: "Bob", subject: "to bob" });
	Kit.throws(function () { st.set_replied("s" + to_bob); }, "forbidden");
	var from_alice = save_shared({ to_ext: String(BOB), to: "Bob", from_ext: String(ALICE),
		from: "Alice", subject: "from alice" });
	Kit.throws(function () { st.set_replied("s" + from_alice); }, "forbidden");
	fresh_shared();
	var bh = shared.get_msg_header(false, to_bob, false);
	Kit.assert(!(bh.attr & D.MSG_REPLIED), "Bob's message untouched");

	// --- a busy summary lock: conflict, nothing saved
	var jf = new JsonFiles(S.user_dir(ALICE), "mailbox.");
	var n_before = st.list("sent").total;
	var lock = jf.lock_path("summary");
	var lf = new File(lock);
	Kit.assert(lf.open("wb"), "create the lock file " + lock);
	lf.write(String(time()));
	lf.close();
	try {
		jf.timeout = 100;
		var held = null;
		var st2 = new Store({ usernum: ALICE, shared_path: S.shared_path, data_dir: S.data_dir });
		st2.open();
		stores.push(st2);
		st2.jf.timeout = 100;
		try {
			st2.save_sent(sent_header({ id: "<c.1@vert.example>" }), body_file("busy\r\n"));
		} catch (e) {
			held = e;
		}
		Kit.assert(held !== null && held.code === "conflict", "busy: conflict: " + JSON.stringify(held));
	} finally {
		file_remove(lock);
	}
	Kit.eq(st.list("sent").total, n_before, "nothing saved while busy");
} finally {
	for (var si = 0; si < stores.length; si++)
		stores[si].close();
	if (shared)
		shared.close();
	S.cleanup();
}
