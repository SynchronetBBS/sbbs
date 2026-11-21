// Send: sending a saved draft (webmail/lib/send.js) into a scratch mail
// base, with a fake sender, fake address lookups and fake system values
// (send.inc); crashes and recovery are in send_recovery.js

load(js.exec_dir + "send.inc");

try {
	// --- The main case: Internet To, local CC, local BCC, 3 MB attachment
	var A = context();
	var big = bytes(3 * 1024 * 1024 + 17, 5);
	var body = "Hello all\r\n\xc3\xa9t\xc3\xa9\r\n.\r\nFrom here\r\n";
	var id = new_draft(A, { to: "Ext Person <ext@example.org>", cc: "bob", bcc: "carl",
		subject: "Report \xe2\x82\xac", body: body, from_domain: "family.example", priority: "high" },
		[{ name: "report.bin", type: "application/octet-stream", data: big }]);
	var k1 = key();
	var r = Send.send(A, id, k1);
	Kit.eq(r.ok, true, "sent");
	Kit.assert(/^p[0-9]+$/.test(r.id), "Sent copy id " + r.id);
	Kit.assert(/^<[0-9a-f]{24}\.[0-9A-F]{8}@family\.example>$/.test(r.message_id), "Message-ID form " + r.message_id);
	Kit.eq(r.warning, undefined, "no warning");
	var copies = with_id(shared(), r.message_id);
	Kit.eq(copies.length, 3, "three delivery copies");
	var inet = null;
	var to_bob = null;
	var to_carl = null;
	for (var i = 0; i < copies.length; i++) {
		var h = copies[i].h;
		var json = JSON.stringify(h);
		if (h.to_net_type == D.NET_INTERNET)
			inet = copies[i];
		else if (h.to_ext == String(BOB))
			to_bob = copies[i];
		else if (h.to_ext == String(CARL))
			to_carl = copies[i];
		if (h.to_ext != String(CARL))
			Kit.assert(!/carl/i.test(json), "no copy but carl's mentions the BCC: " + json);
		else
			Kit.assert(!/carl/i.test(JSON.stringify([h.to_list, h.cc_list])), "BCC not in carl's lists");
		Kit.eq(h.priority, 2, "priority high");
		Kit.assert(h.auxattr & D.MSG_HFIELDS_UTF8, "UTF-8 header fields");
		Kit.eq(h.subject, "Report \xe2\x82\xac", "subject");
		Kit.eq(h.from_ext, String(ALICE), "from_ext");
		Kit.eq(h.to_list, "Ext Person <ext@example.org>", "to_list");
		Kit.eq(h.cc_list, "bob <bob@vert.example>", "cc_list");
	}
	Kit.assert(inet && to_bob && to_carl, "one copy each");
	Kit.eq(inet.h.to_net_addr, "ext@example.org", "Internet address");
	Kit.eq(inet.h.from_net_addr, "alice@family.example", "the chosen From address");
	Kit.eq(inet.h.reverse_path, "alice@family.example", "reverse path");
	Kit.eq(inet.h.from_net_type, D.NET_INTERNET, "from network type");
	Kit.eq(inet.h.from, "alice", "Internet From name (alias setting)");
	Kit.assert(inet.h.netattr & D.NETMSG_KILLSENT, "Internet copy: kill when sent");
	Kit.assert(!(to_bob.h.netattr & D.NETMSG_KILLSENT), "local copy kept");
	Kit.eq(to_bob.h.from, "alice", "local From is the alias");
	Kit.eq(to_bob.h.from_net_type, undefined, "local copy: no sender network");
	// The Sent copy
	var items = sent_items(A);
	Kit.eq(items.length, 1, "one Sent item");
	Kit.eq(items[0].id, r.id, "the Sent copy");
	Kit.eq(items[0].priority, "high", "listed high");
	Kit.eq(items[0].delivery.state, "queued", "queued while the Internet copy waits");
	var g = A.store.get(r.id);
	Kit.assert(g.headers.bcc.indexOf("carl") >= 0, "the Sent copy lists the BCC: " + g.headers.bcc);
	Kit.eq(g.headers.message_id, r.message_id, "same Message-ID");
	Kit.eq(strip_end(base64_decode(g.text.b64)), strip_end(body), "Sent text read back");
	Kit.eq(g.parts.length, 1, "one attachment");
	var pf = A.store.part_file(r.id, g.parts[0].pid);
	Kit.eq(md5_calc(read_file(pf.path), true), md5_calc(big, true), "attachment md5 (Sent copy)");
	file_remove(pf.path);
	// Bob's copy through Bob's Store
	var bst = store_for(BOB);
	var bg = bst.get("s" + to_bob.num);
	Kit.eq(strip_end(base64_decode(bg.text.b64)), strip_end(body), "Bob's text read back");
	var bpf = bst.part_file("s" + to_bob.num, bg.parts[0].pid);
	Kit.eq(md5_calc(read_file(bpf.path), true), md5_calc(big, true), "attachment md5 (Bob)");
	file_remove(bpf.path);
	Kit.eq(bg.headers.from, "alice", "Bob sees the alias");
	// Statistics, the draft
	Kit.eq(A.user.adjusted, [], "no cost charged");
	Kit.eq(A.user.sent, [[3, false]], "sent_email(3) once");
	Kit.assert(!draft_exists(A, id), "the draft is gone");
	// The recipients, BCC included, are in the sender's own contacts
	var book = Contacts.list(A.userdata);
	var addrs = [];
	for (i = 0; i < book.length; i++)
		addrs = addrs.concat(book[i].addrs);
	addrs.sort();
	Kit.eq(addrs, ["bob", "carl", "ext@example.org"], "recipients collected");
	Kit.eq(Contacts.suggest(A.userdata, "ext")[0].name, "Ext Person", "with their names");
	Kit.eq(directory(W.S.user_dir(ALICE) + "webmail/drafts/*").length, 0, "no claimed draft left");
	// Delivered: the Internet copy gone (killed when sent) -> sent
	remove_shared(inet.num);
	Kit.eq(sent_items(A)[0].delivery.state, "sent", "sent once the Internet copy is gone");
	// A name that is not ASCII, as a draft holds it (plain UTF-8 in a
	// quoted string): the delivery copy's To line carries it as encoded
	// words a reader decodes to that name, and the contact gets the name
	var JW = "J\xc3\xbcrgen Wei\xc3\x9f";
	var jid = new_draft(A, { to: '"' + JW + '" <jw@example.de>', subject: "Umlaut", body: "x\r\n" });
	var jr = Send.send(A, jid, key());
	var jc = with_id(shared(), jr.message_id);
	Kit.eq(jc.length, 1, "one copy");
	var jm = /^=\?UTF-8\?B\?([^?]*)\?= <jw@example\.de>$/.exec(jc[0].h.to_list);
	Kit.assert(jm !== null, "To line with an encoded word: " + jc[0].h.to_list);
	Kit.eq(base64_decode(jm[1]), JW, "which decodes to the name");
	Kit.eq(Contacts.suggest(A.userdata, "jw@")[0].name, JW, "the contact has the name as text");
	remove_shared(jc[0].num);

	// --- Idempotency: the same key returns the outcome, sends nothing
	var before = shared().length;
	Kit.eq(Send.send(A, id, k1), r, "the same key: the recorded outcome");
	Kit.eq(shared().length, before, "nothing sent again");
	Kit.eq(Send.send(A, id, key()), r, "another key for the sent draft: its outcome");
	Kit.eq(shared().length, before, "nothing sent again");
	fails(function () { Send.send(A, id, "short"); }, "invalid", "key");
	fails(function () { Send.send(A, id, "bad key with spaces!!"); }, "invalid", "key");

	// --- Both To recipients in every copy's to_list; the BCC in none
	var B = context();
	var id2 = new_draft(B, { to: "ext@example.org, bob", bcc: "Zed <zed@bcc.example>", subject: "two",
		body: "x" });
	var r2 = Send.send(B, id2, key());
	var c2 = with_id(shared(), r2.message_id);
	Kit.eq(c2.length, 3, "three copies");
	for (i = 0; i < c2.length; i++) {
		Kit.eq(c2[i].h.to_list, "ext@example.org, bob <bob@vert.example>", "both To recipients");
		Kit.eq(c2[i].h.cc_list, undefined, "no CC list");
		if (c2[i].h.to_net_addr !== "zed@bcc.example")
			Kit.assert(JSON.stringify(c2[i].h).indexOf("zed@") < 0, "BCC not mentioned");
	}

	// --- A chosen From name: the address, when it is one of the user's
	var CN = context();
	CN.addresses = ["alice@vert.example", "alice@family.example", "ally@family.example"];
	var idn = new_draft(CN, { to: "ext@example.org", body: "x", from_domain: "family.example", from_local: "ally" });
	Send.send(CN, idn, key());
	var cn = shared();
	Kit.eq(cn[cn.length - 1].h.from_net_addr, "ally@family.example", "the chosen From name and domain");
	// ... and not once it no longer is (alias.cfg changed since the save)
	var idn2 = new_draft(CN, { to: "ext@example.org", body: "x", from_domain: "family.example", from_local: "ally" });
	CN.addresses = ["alice@vert.example", "alice@family.example"];
	nothing_saved(CN, function () { Send.send(CN, idn2, key()); }, "forbidden", "domain");

	// --- From domain outside the allowed list: forbidden, nothing saved
	var C = context({ domains: DOMAINS.concat(["evil.example"]) });
	var id3 = new_draft(C, { to: "ext@example.org", body: "x", from_domain: "evil.example" });
	C.domains = DOMAINS;
	nothing_saved(C, function () { Send.send(C, id3, key()); }, "forbidden", "domain");
	Kit.assert(draft_exists(C, id3), "draft kept after a refusal");

	// --- No recipients: invalid
	var id4 = new_draft(A, { subject: "nobody", body: "x" });
	nothing_saved(A, function () { Send.send(A, id4, key()); }, "invalid", "norecipients");
	Kit.assert(draft_exists(A, id4), "draft kept");

	// --- can_send_mail false for Internet mail: forbidden, nothing saved
	var E = context({ user: { deny: [D.NET_INTERNET] } });
	var id5 = new_draft(E, { to: "bob, ext@example.org", body: "x" });
	var e5 = nothing_saved(E, function () { Send.send(E, id5, key()); }, "forbidden", "recipient");
	Kit.eq(e5.index, 1, "names the second recipient");
	Kit.eq(e5.entry, "ext@example.org", "entry");

	// --- A local recipient whose mailbox is full (Max Messages Waiting):
	// the whole send is refused, nothing saved; only local recipients are
	// asked about
	var asked = [];
	var MF = context({ sys: { mailbox_full: function (n) {
		asked.push(n);
		return n === CARL;
	} } });
	var idm = new_draft(MF, { to: "bob, ext@example.org", cc: "carl", body: "x" });
	var em = nothing_saved(MF, function () { Send.send(MF, idm, key()); }, "forbidden", "mailbox_full");
	Kit.eq([em.index, em.entry], [2, "carl"], "names the full recipient");
	Kit.eq(asked, [BOB, CARL], "asked for the local recipients only, in order");
	Kit.assert(draft_exists(MF, idm), "draft kept after a full mailbox");
	// A check that cannot be made refuses the send too
	var MX = context({ sys: { mailbox_full: function (n) {
		throw new Error("simulated lookup failure");
	} } });
	var idx = new_draft(MX, { to: "bob", body: "x" });
	nothing_saved(MX, function () { Send.send(MX, idx, key()); }, "failed");

	// --- Cost: 10 per Internet recipient, two of them
	var F = context({ sys: { inet_cost: 10 }, user: { credits: 15 } });
	var id6 = new_draft(F, { to: "a@example.org, b@example.org", body: "x" });
	nothing_saved(F, function () { Send.send(F, id6, key()); }, "forbidden", "credits");
	F.user.security.credits = 25;
	var r6 = Send.send(F, id6, key());
	Kit.eq(r6.ok, true, "sent with 25 credits");
	Kit.eq(F.user.adjusted, [-20], "adjust_credits(-20) once");
	Kit.eq(F.user.sent, [[2, false]], "sent_email once");
	// exempt: no charge
	var F2 = context({ sys: { inet_cost: 10 }, user: { credits: 0, exemptions: 1 << 18 } });
	Send.send(F2, new_draft(F2, { to: "a@example.org", body: "x" }), key());
	Kit.eq(F2.user.adjusted, [], "exempt: nothing charged");

	// --- A reply: reply_id is the source's Message-ID; the source replied
	var src = W.save({ to: "Alice", to_ext: String(ALICE), from: "Ext", from_net_type: D.NET_INTERNET,
		from_net_addr: "ext@example.org", subject: "question", id: "<orig.1@example.org>" });
	var R = context();
	var rd = Drafts.create(R, { mode: "reply", source_id: "s" + src });
	rd = Drafts.save(R, rd.id, { rev: rd.rev, body: "answer" });
	var r7 = Send.send(R, rd.id, key());
	var c7 = with_id(shared(), r7.message_id);
	Kit.eq(c7.length, 1, "one copy");
	Kit.eq(c7[0].h.reply_id, "<orig.1@example.org>", "reply_id");
	Kit.eq(R.store.get(r7.id).headers.reply_id, "<orig.1@example.org>", "Sent copy reply_id");
	var inbox = R.store.list("inbox").items;
	for (i = 0; i < inbox.length; i++)
		if (inbox[i].id === "s" + src)
			Kit.eq(inbox[i].replied, true, "source marked replied");

	// --- FidoNet: refused with an attachment; plain text without
	var G = context();
	var id8 = new_draft(G, { to: "John Doe <John.Doe@1:103/705>", body: "x" },
		[{ name: "a.txt", type: "text/plain", data: "abc" }]);
	nothing_saved(G, function () { Send.send(G, id8, key()); }, "invalid", "attachments");
	var fbody = "Hi John\r\nCaf\xc3\xa9\r\n";
	var id9 = new_draft(G, { to: "John Doe <John.Doe@1:103/705>", body: fbody, subject: "fido" });
	var r9 = Send.send(G, id9, key());
	var c9 = with_id(shared(), r9.message_id);
	Kit.eq(c9.length, 1, "one FidoNet copy");
	Kit.eq(c9[0].h.to_net_type, D.NET_FIDO, "FidoNet network type");
	Kit.eq(c9[0].h.to, "John Doe", "FidoNet name");
	Kit.eq(c9[0].h.to_net_addr, "1:103/705", "FidoNet address");
	Kit.eq(c9[0].h.content_type, undefined, "plain text: no Content-Type");
	Kit.eq(c9[0].h.ftn_charset, "UTF-8 4", "marked UTF-8");
	Kit.eq(c9[0].h.to_list, undefined, "no To list with a FidoNet address");
	Kit.assert(c9[0].h.netattr & D.NETMSG_KILLSENT, "FidoNet copy: kill when sent");
	var fmb = Kit.email_base(W.S.shared_path);
	var ftext = fmb.get_msg_body(false, c9[0].num);
	fmb.close();
	Kit.eq(strip_end(ftext), strip_end(fbody), "FidoNet text");
	Kit.assert(G.store.get(r9.id).headers.to.indexOf("(1:103/705)") >= 0, "Sent copy names the FidoNet address");
	// a FidoNet address without a name is refused, as at the terminal
	var id9b = new_draft(G, { to: "1:103/705", body: "x" });
	nothing_saved(G, function () { Send.send(G, id9b, key()); }, "invalid", "fidoname");
	// no FidoNet address configured
	var G2 = context({ sys: { fido_addrs: [] } });
	nothing_saved(G2, function () {
		Send.send(G2, new_draft(G2, { to: "Jo <Jo@1:103/705>", body: "x" }), key());
	}, "forbidden", "fido");

	// --- QWK: refused unless system.check_netmail_addr() finds a route;
	// attachments refused for QWK as for FidoNet
	var Q = context();
	var asked_qwk = [];
	Q.sys.check_netmail_addr = function (a) {
		asked_qwk.push(a);
		return false;
	};
	nothing_saved(Q, function () {
		Send.send(Q, new_draft(Q, { to: "someone@VERT", body: "x" }), key());
	}, "forbidden", "qwk");
	Kit.eq(asked_qwk, ["someone@VERT"], "the route was asked about");
	var Q2 = context({ sys: { check_netmail_addr: function () { return true; } } });
	nothing_saved(Q2, function () {
		Send.send(Q2, new_draft(Q2, { to: "someone@VERT", body: "x" },
			[{ name: "a.txt", type: "text/plain", data: "abc" }]), key());
	}, "invalid", "attachments");

	// --- No room for the Sent copy: sent anyway, warning nocopy
	var H = context();
	H.collect = null;	// no contacts
	H.store.save_sent = function () {
		throw { code: "quota", message: "the mailbox is full" };
	};
	var r10 = Send.send(H, new_draft(H, { to: "bob", body: "x" }), key());
	Kit.eq(r10.warning, "nocopy", "nocopy");
	Kit.eq(r10.id, null, "no Sent id");
	Kit.eq(with_id(shared(), r10.message_id).length, 1, "delivered");

	// --- a delivery save that fails leaves no Sent copy
	// and the draft intact
	var I = context();
	var id11 = new_draft(I, { to: "ext@example.org, bob", subject: "fail", body: "keep me" },
		[{ name: "k.bin", type: "application/octet-stream", data: bytes(1000, 1) }]);
	var before11 = shared().length;
	var sent11 = I.store.list("sent").total;
	var k11 = key();
	Send.hook = function (point, kind) {
		if (point == "delivery" && kind == "local")
			throw new Error("simulated delivery failure");
	};
	try {
		fails(function () { Send.send(I, id11, k11); }, "failed");
	} finally {
		Send.hook = null;
	}
	Kit.eq(shared().length, before11, "the Internet copy already saved was removed");
	Kit.eq(I.store.list("sent").total, sent11, "no Sent copy");
	var back = Drafts.get(I, id11);
	Kit.eq([back.to, back.subject, back.body, back.attachments.length],
		["ext@example.org, bob", "fail", "keep me", 1], "the draft is intact");
	var r11 = Send.send(I, id11, k11);	// the same key may try again
	Kit.eq(r11.ok, true, "a retry after a failure sends");
	Kit.eq(with_id(shared(), r11.message_id).length, 2, "two copies, once");

	// --- A failure after the delivery save still reports success
	var J = context({ user: { adjust_throws: true }, sys: { inet_cost: 1 }, });
	J.user.security.credits = 5;
	J.store.mark_sent = function () {
		throw new Error("simulated mark_sent failure");
	};
	var logs = logged.length;
	var r12 = Send.send(J, new_draft(J, { to: "ext@example.org", body: "x" }), key());
	Kit.eq(r12.ok, true, "success despite step 5 failures");
	Kit.assert(logged.slice(logs).join("\n").indexOf("after sending") >= 0, "the failures are logged");
	Kit.eq(sent_item(J, r12.id).delivery.state, "queued", "delivered");

	// --- no line break in the subject; display names
	var L = context({ settings: { display_name: "Bob" } });
	var r16 = Send.send(L, new_draft(L, { to: "ext@example.org, bob", subject: "Hi\r\nBcc: evil@example.org",
		body: "x" }), key());
	var c16 = with_id(shared(), r16.message_id);
	for (i = 0; i < c16.length; i++) {
		Kit.assert(!/[\r\n]/.test(c16[i].h.subject), "no line break in the subject");
		if (c16[i].h.to_net_type == D.NET_INTERNET)
			Kit.eq(c16[i].h.from, "Bob", "the display name for Internet mail");
		else
			Kit.eq(c16[i].h.from, "alice", "the local copy carries the sender's own name");
	}

	// --- Own-domain address delivered locally; duplicates sent once
	var M = context();
	var r17 = Send.send(M, new_draft(M, { to: "bob@vert.example", body: "x" }), key());
	var c17 = with_id(shared(), r17.message_id);
	Kit.eq(c17.length, 1, "one copy");
	Kit.eq([c17[0].h.to_ext, c17[0].h.to_net_type], [String(BOB), undefined], "local delivery");
	var r18 = Send.send(M, new_draft(M, { to: "bob, Bob <bob@vert.example>", cc: "BOB, ext@example.org",
		bcc: "EXT@example.org, bob" }), key());
	var c18 = with_id(shared(), r18.message_id);
	Kit.eq(c18.length, 2, "each destination once");
	for (i = 0; i < c18.length; i++) {
		Kit.eq(c18[i].h.to_list, "bob <bob@vert.example>", "To as first given");
		Kit.eq(c18[i].h.cc_list, "ext@example.org", "CC as first given");
	}
	Kit.eq(M.user.sent[M.user.sent.length - 1], [2, false], "counted once each");

	// --- A local recipient whose address cannot be written: left out of
	// the lists, the send goes on
	var X = context({ sys: { user_email: function (n) { return '"bo!b"@vert.example'; } } });
	var r20 = Send.send(X, new_draft(X, { to: "bob", cc: "ext@example.org", body: "x" }), key());
	var c20 = with_id(shared(), r20.message_id);
	Kit.eq(c20.length, 2, "both delivered");
	for (i = 0; i < c20.length; i++)
		Kit.eq([c20[i].h.to_list, c20[i].h.cc_list], [undefined, "ext@example.org"], "bob left out of the lists");
	Kit.assert(X.store.get(r20.id).headers.to.indexOf("bob") >= 0, "the Sent copy names bob");

	// --- A reply whose source Message-ID is over 900 bytes: no reply_id
	var long_id = "<" + new Array(950).join("x") + "@example.org>";
	var src2 = W.save({ to: "Alice", to_ext: String(ALICE), from: "Ext", from_net_type: D.NET_INTERNET,
		from_net_addr: "ext@example.org", subject: "long id", id: long_id });
	var R2 = context();
	var rd2 = Drafts.create(R2, { mode: "reply", source_id: "s" + src2 });
	Kit.eq(rd2.reply_message_id, long_id, "the long Message-ID read back intact");
	rd2 = Drafts.save(R2, rd2.id, { rev: rd2.rev, body: "answer" });
	var r21 = Send.send(R2, rd2.id, key());
	Kit.eq(with_id(shared(), r21.message_id)[0].h.reply_id, undefined, "an overlong reply_id is left out");

	// --- A long recipient list: warning, no lists in the copies
	var many = ['"A@b \\"q\\" =?x?= <z>" <weird@example.org>'];
	for (i = 1; i < 30; i++)
		many.push("Recipient Number " + i + " <recipient" + i + "@example.org>");
	var N = context();
	var r19 = Send.send(N, new_draft(N, { to: many.join(", "), body: "x" }), key());
	Kit.eq(r19.warning, "longlist", "longlist");
	var c19 = with_id(shared(), r19.message_id);
	Kit.eq(c19.length, 30, "30 copies");
	Kit.eq([c19[0].h.to_list, c19[0].h.cc_list], [undefined, undefined], "no lists");
	// the mail server then prints  "to" <address>  (or "to" alone when it
	// holds '@'): the name may not carry a quote, bracket, '@' or marker
	for (i = 0; i < c19.length; i++) {
		if (c19[i].h.to_net_addr === "weird@example.org")
			Kit.assert(!/["<>@]|=\?|\?=/.test(c19[i].h.to) && c19[i].h.to !== "", "a safe To name: " + c19[i].h.to);
	}
	Kit.assert(N.store.get(r19.id).headers.to.indexOf("recipient29@") >= 0, "the Sent copy keeps them");

	// --- Limits: recipients and size
	var O = context({ config: { max_recipients: 2 } });
	nothing_saved(O, function () {
		Send.send(O, new_draft(O, { to: "a@example.org, b@example.org", cc: "bob", body: "x" }), key());
	}, "invalid", "toomany");
	var P = context({ config: { max_message_bytes: 2000 } });
	var id20 = new_draft(P, { to: "bob", body: "x" }, [{ name: "b", type: "application/octet-stream",
		data: bytes(3000, 2) }]);
	nothing_saved(P, function () { Send.send(P, id20, key()); }, "toolarge");
	Kit.assert(draft_exists(P, id20), "draft kept");

	// --- A second send of a claimed draft is refused
	var T = context();
	var id21 = new_draft(T, { to: "bob", body: "x" });
	var k21 = key();
	Send.hook = function (point) {
		if (point == "delivery")
			fails(function () { Send.send(T, id21, key()); }, "conflict", "sending");
	};
	try {
		Send.send(T, id21, k21);
	} finally {
		Send.hook = null;
	}

	// --- One log line per send, without addresses or subjects
	var lines = 0;
	for (i = 0; i < logged.length; i++) {
		if (/ sent mail: /.test(logged[i])) {
			lines++;
			Kit.assert(!/@|Report|crash/.test(logged[i]), "no address or subject logged: " + logged[i]);
		}
	}
	Kit.assert(lines >= 12, "a log line per send: " + lines);
	if (argv.indexOf("-v") >= 0)
		print(logged.join("\n"));
	Kit.eq(directory(system.temp_dir + "webmail_send_*").length, 0, "no temp file left");
} finally {
	Send.hook = null;
	for (var ci = 0; ci < contexts.length; ci++)
		contexts[ci].close();
	W.cleanup();
}
