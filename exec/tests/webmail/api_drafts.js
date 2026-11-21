// Api: the draft.*, contact.* and draft.send methods, driven through
// Api.dispatch with a fake env over real Drafts, Send, Contacts and Store
// objects on scratch files (send.inc's world: fake sender, fake address
// lookups and system values, a local recipient); argument checks,
// ownership, background requests, "as", send outcomes and conflict
// reasons, and recovery of an interrupted send by draft.list

load(js.exec_dir + "send.inc");
var Api = W.Api;
var err = W.err;
var ApiDrafts = Api.lib("drafts");
var ApiSend = Api.lib("send");
var quiet = function () {};
Api.log = quiet;
ApiDrafts.log = quiet;
ApiSend.log = quiet;
Api.lib("contacts").log = quiet;

var clock = NOW;
var ALICE_USER = fake_user();

// Alice's env: the send world's fakes go to the libraries through
// env.lib_ctx; o.user: fake_user options
function env_for(cookie, usernum, o)
{
	o = o || {};
	var ev = W.make_env({ cookie: cookie, config: o.config, make_ctx: o.make_ctx });
	ev.data_dir = W.S.data_dir;
	var extra = { domains: DOMAINS, now: function () {
		return clock;
	} };
	if (usernum === ALICE) {
		extra.user = o.user ? fake_user(o.user) : ALICE_USER;
		extra.resolvers = resolvers();
		extra.sys = { inet_addr: "vert.example", fido_addrs: ["1:103/705"], inet_cost: 0, fido_cost: 0,
			inet_alias: true, fido_alias: true,
			user_email: function (n) {
				return NAMES[n] + "@vert.example";
			},
			check_netmail_addr: function () {
				return false;
			},
			mailbox_full: function (n) {
				return o.full !== undefined && o.full.indexOf(n) >= 0;
			} };
	}
	ev.lib_ctx = extra;
	return ev;
}

var alice, bob;

function A(o)
{
	return env_for(alice, ALICE, o);
}

function B()
{
	return env_for(bob, BOB);
}

// One call with "as" (the page's user)
function call(ev, method, args, background)
{
	var rq = { calls: [[method, args, "c"]], as: W.as_of(ev) };
	if (background)
		rq.background = true;
	var r = Api.dispatch(ev, rq);
	Kit.assert(r.results instanceof Array && r.results.length === 1, method + ": one result");
	return r.results[0][1];
}

function ok(r, what)
{
	Kit.eq(r.ok, true, what + ": " + JSON.stringify(r));
	return r;
}

// A refusal with a reason: the reason's own text when it has one
function refused(r, code, reason, what)
{
	Kit.eq(r.ok, false, what + ": refused: " + JSON.stringify(r));
	Kit.eq(r.error, code, what + ": code");
	Kit.eq(r.reason, reason, what + ": reason");
	var key = "errors." + code + "_" + reason;
	Kit.eq(r.message, (W.STRINGS.hasOwnProperty(key) ? W.STRINGS[key] : W.STRINGS["errors." + code])
		.split("{count}").join(String(W.cfg.max_recipients)), what + ": text");
	Kit.assert(!/[\/\\]/.test(r.message), what + ": no path in the message");
	return r;
}

function new_api_draft(ev, fields)
{
	var d = ok(call(ev, "draft.create", { mode: "new" }), "draft.create").draft;
	if (fields) {
		var f = { id: d.id, rev: d.rev };
		for (var k in fields)
			f[k] = fields[k];
		d = ok(call(ev, "draft.save", f), "draft.save").draft;
	}
	return d;
}

try {
	alice = W.sign_in(ALICE);
	bob = W.sign_in(BOB);
	var r, d, i, ev;

	// --- draft.create: argument checks, the draft's shape
	err(call(A(), "draft.create", { mode: "draft" }), "invalid", "unknown mode");
	err(call(A(), "draft.create", { mode: "new", source_id: "s1" }), "invalid", "new with a source");
	err(call(A(), "draft.create", { mode: "reply" }), "invalid", "reply without a source");
	err(call(A(), "draft.create", { mode: "forward", source_id: "../x" }), "invalid", "bad source id");
	err(call(A(), "draft.create", {}), "invalid", "no mode");
	ev = A();
	d = ok(call(ev, "draft.create", { mode: "new" }), "new").draft;
	Kit.assert(/^[0-9a-f]{16}$/.test(d.id), "a draft id");
	Kit.eq(d.rev, 1, "rev 1");
	Kit.eq(ev.made, 0, "a new draft opens no mailbox");
	Kit.eq(call(A(), "draft.create", { mode: "new" }).dropped_recipients, undefined, "no dropped count on new");

	// --- draft.save / get: the round trip, stale rev with the current rev
	r = ok(call(A(), "draft.save", { id: d.id, rev: 1, to: "bob", subject: "Hi", body: "Line\r\n\"q\"",
		priority: "high" }), "save");
	Kit.eq([r.draft.rev, r.draft.to, r.draft.subject, r.draft.priority], [2, "bob", "Hi", "high"], "saved");
	r = ok(call(A(), "draft.get", { id: d.id }), "get");
	Kit.eq(r.draft.body, "Line\r\n\"q\"", "body read back");
	r = refused(call(A(), "draft.save", { id: d.id, rev: 1, subject: "old" }), "conflict", "stale", "stale rev");
	Kit.eq(r.rev, 2, "the conflict carries the current rev");
	Kit.eq(ok(call(A(), "draft.get", { id: d.id }), "get").draft.subject, "Hi", "nothing written");
	var bad_saves = [
		[{ id: d.id, subject: "x" }, "no rev"],
		[{ id: d.id, rev: -1 }, "negative rev"],
		[{ id: d.id, rev: 2, priority: "urgent" }, "priority"],
		[{ id: d.id, rev: 2, to: new Array(16386).join("a") }, "to over 16384 bytes"],
		[{ id: d.id, rev: 2, subject: new Array(4098).join("s") }, "subject over 4096"],
		[{ id: d.id, rev: 2, mode: "reply" }, "a field not saved"],
		[{ id: "../../etc/x", rev: 2 }, "id path"],
		[{ id: d.id + "0", rev: 2 }, "17-character id"],
		[{ id: d.id.toUpperCase(), rev: 2 }, "upper-case id"],
		[{ id: d.id, rev: 2, body: String.fromCharCode(0x263a) }, "a wide character"]];
	for (i = 0; i < bad_saves.length; i++)
		err(call(A(), "draft.save", bad_saves[i][0]), "invalid", "draft.save: " + bad_saves[i][1]);
	refused(call(A({}), "draft.save", { id: d.id, rev: 2, body: new Array(W.cfg.max_body_bytes + 2).join("b") }),
		"toolarge", "body", "body over max_body_bytes");

	// --- another user's draft cannot be reached
	err(call(B(), "draft.get", { id: d.id }), "notfound", "Bob: get Alice's draft");
	err(call(B(), "draft.save", { id: d.id, rev: 2, subject: "mine" }), "notfound", "Bob: save");
	err(call(B(), "draft.delete", { id: d.id }), "notfound", "Bob: delete");
	err(call(B(), "draft.attach", { id: d.id, upload: "bobupload1", name: "x", offset: 0, total: 1,
		b64: "eA==" }), "notfound", "Bob: attach");
	err(call(B(), "draft.send", { id: d.id, key: key() }), "notfound", "Bob: send");
	Kit.eq(ok(call(B(), "draft.list", {}), "Bob's list").drafts.length, 0, "Bob lists none of Alice's");
	Kit.eq(ok(call(A(), "draft.get", { id: d.id }), "get").draft.subject, "Hi", "untouched");

	// --- draft.attach / detach
	var data = "hello attachment";
	r = ok(call(A(), "draft.attach", { id: d.id, upload: "upload-key-1", name: "../../note.txt",
		type: "text/plain", offset: 0, total: data.length, b64: base64_encode(data.substr(0, 5)) }), "chunk 1");
	Kit.eq(r.attachment.received, 5, "received 5");
	Kit.eq(r.attachment.complete, false, "not complete");
	var att = r.attachment.id;
	Kit.assert(/^a[0-9]+$/.test(att), "attachment id " + att);
	r = refused(call(A(), "draft.attach", { id: d.id, upload: "upload-key-1", name: "x", offset: 3,
		total: data.length, b64: base64_encode("lo") }), "conflict", "offset", "wrong offset");
	Kit.eq(r.received, 5, "the conflict carries received");
	r = ok(call(A(), "draft.attach", { id: d.id, upload: "upload-key-1", name: "x", offset: 5,
		total: data.length, b64: base64_encode(data.substr(5)) }), "chunk 2");
	Kit.eq([r.attachment.received, r.attachment.complete], [data.length, true], "complete");
	r = ok(call(A(), "draft.get", { id: d.id }), "get");
	Kit.eq([r.draft.attachments[0].name, r.draft.attachments[0].size], ["note.txt", data.length], "stored");
	refused(call(A(), "draft.attach", { id: d.id, upload: "upload-key-2", name: "big", offset: 0,
		total: W.cfg.max_attachment_bytes + 1, b64: base64_encode("x") }), "toolarge", "total", "declared too large");
	var bad_chunks = [
		[{ upload: "short", name: "x", offset: 0, total: 1, b64: "eA==" }, "upload key too short"],
		[{ upload: "upload-key-3", name: "x", offset: -1, total: 1, b64: "eA==" }, "negative offset"],
		[{ upload: "upload-key-3", name: "x", offset: 0, total: 1.5, b64: "eA==" }, "fractional total"],
		[{ upload: "upload-key-3", offset: 0, total: 1, b64: "eA==" }, "no name"],
		[{ upload: "upload-key-3", name: "x", offset: 0, total: 1 }, "no data"],
		[{ upload: "upload-key-3", name: "x", offset: 0, total: 1, b64: new Array(2000002).join("A") },
			"chunk over 2000000 base64 characters"],
		[{ upload: "upload-key-3", name: new Array(1026).join("n"), offset: 0, total: 1, b64: "eA==" },
			"name over 1024"]];
	for (i = 0; i < bad_chunks.length; i++) {
		var a = bad_chunks[i][0];
		a.id = d.id;
		err(call(A(), "draft.attach", a), "invalid", "draft.attach: " + bad_chunks[i][1]);
	}
	err(call(A(), "draft.detach", { id: d.id, attachment: "../a1" }), "invalid", "bad attachment id");
	err(call(A(), "draft.detach", { id: d.id, attachment: "a99" }), "notfound", "no such attachment");
	r = ok(call(A(), "draft.detach", { id: d.id, attachment: att }), "detach");
	Kit.eq(r.draft.attachments.length, 0, "detached");

	// --- draft.list: rows, newest first; draft.delete
	var d2 = new_api_draft(A(), { subject: "second" });
	r = ok(call(A(), "draft.list", {}), "list");
	Kit.assert(r.drafts.length >= 3, "three drafts listed");
	Kit.eq(typeof r.swept, "number", "swept count");
	Kit.eq(typeof r.recovered, "number", "recovered count");
	ok(call(A(), "draft.delete", { id: d2.id }), "delete");
	err(call(A(), "draft.get", { id: d2.id }), "notfound", "deleted");
	err(call(A(), "draft.delete", { id: d2.id }), "notfound", "deleted twice");

	// --- writes need "as" and are refused in a background request
	r = Api.dispatch(A(), { calls: [["draft.save", { id: d.id, rev: 2, subject: "no as" }, 1]] });
	err(r.results[0][1], "invalid", "draft.save without as");
	var writes = [["draft.create", { mode: "new" }], ["draft.save", { id: d.id, rev: 2, subject: "bg" }],
		["draft.delete", { id: d.id }], ["draft.attach", { id: d.id, upload: "upload-bg-1", name: "x",
			offset: 0, total: 1, b64: "eA==" }], ["draft.detach", { id: d.id, attachment: "a1" }],
		["draft.send", { id: d.id, key: key() }], ["contact.set", { addrs: ["x@y.example"] }],
		["contact.delete", { id: "c1" }]];
	for (i = 0; i < writes.length; i++) {
		err(call(A(), writes[i][0], writes[i][1], true), "forbidden", writes[i][0] + " in the background");
		r = Api.dispatch(A(), { calls: [[writes[i][0], writes[i][1], 1]] });
		err(r.results[0][1], "invalid", writes[i][0] + " without as");
		err(W.call(W.make_env(), writes[i][0], writes[i][1]), "auth", writes[i][0] + " signed out");
	}
	Kit.eq(ok(call(A(), "draft.get", { id: d.id }), "get").draft.subject, "Hi", "nothing changed");
	var reads = [["draft.get", { id: d.id }], ["draft.list", {}], ["contact.list", {}],
		["contact.suggest", { q: "" }]];
	for (i = 0; i < reads.length; i++)
		ok(call(A(), reads[i][0], reads[i][1], true), reads[i][0] + " in the background");

	// --- draft.list sweeps (not in a background request): an empty draft
	// untouched for a day goes
	var empty = new_api_draft(A());
	clock = NOW + 2 * 86400;
	ok(call(A(), "draft.list", {}, true), "background list");
	ok(call(A(), "draft.get", { id: empty.id }), "a background list removed nothing");
	r = ok(call(A(), "draft.list", {}), "list");
	Kit.assert(r.swept >= 1, "swept " + r.swept);
	err(call(A(), "draft.get", { id: empty.id }), "notfound", "the stale empty draft removed");
	ok(call(A(), "draft.get", { id: d.id }), "a draft with text kept");

	// --- draft.create reply: from the user's own message; dropped_recipients
	var src_num = W.save({ to_ext: String(ALICE), to: "alice", from: "Carol Ext", subject: "Question",
		from_net_type: D.NET_INTERNET, from_net_addr: "carol@example.org" });
	ev = A();
	r = ok(call(ev, "draft.create", { mode: "reply", source_id: "s" + src_num }), "reply");
	Kit.eq(r.dropped_recipients, 0, "dropped_recipients beside the draft");
	Kit.eq(r.draft.dropped_recipients, undefined, "not inside the draft");
	Kit.assert(/carol@example\.org/.test(r.draft.to), "reply to the sender: " + r.draft.to);
	Kit.eq(r.draft.mode, "reply", "mode");
	Kit.eq(ev.made, 1, "a reply opens the mailbox");
	ok(call(A(), "draft.delete", { id: r.draft.id }), "delete the reply");
	var bob_num = W.save({ to_ext: String(BOB), to: "bob", from: "Eve", subject: "for bob" });
	err(call(A(), "draft.create", { mode: "reply", source_id: "s" + bob_num }), "forbidden",
		"a reply to another user's message");
	err(call(A(), "draft.create", { mode: "forward", source_id: "s" + bob_num }), "forbidden",
		"a forward of another user's message");

	// --- contacts: set, list, suggest (own contacts only), delete
	r = ok(call(A(), "contact.set", { name: "Carol\r\nX", addrs: ["carol@example.org", "Carol@Example.org"],
		note: "met" }), "contact.set");
	var cid = r.contact.id;
	Kit.assert(/^c[0-9]+$/.test(cid), "contact id");
	Kit.eq([r.contact.name, r.contact.addrs], ["CarolX", ["carol@example.org"]], "cleaned, once");
	Kit.eq(ok(call(A(), "contact.list", {}), "list").contacts.length, 1, "one contact");
	r = ok(call(A(), "contact.suggest", { q: "car" }), "suggest");
	Kit.eq(r.suggestions, [{ id: cid, name: "CarolX", addr: "carol@example.org" }], "suggested");
	Kit.eq(ok(call(B(), "contact.suggest", { q: "car" }), "Bob suggest").suggestions, [], "not to Bob");
	Kit.eq(ok(call(B(), "contact.suggest", { q: "alice" }), "Bob suggest").suggestions, [],
		"never the system's users");
	err(call(B(), "contact.delete", { id: cid }), "notfound", "Bob cannot delete Alice's contact");
	err(call(B(), "contact.set", { id: cid, addrs: ["x@y.example"] }), "notfound", "nor change it");
	var bad_contacts = [
		[{ addrs: [] }, "no address"], [{ addrs: "x@y.example" }, "not a list"],
		[{ addrs: ["a@b.example", "c@d.example", "e@f.example", "g@h.example", "i@j.example", "k@l.example",
			"m@n.example", "o@p.example", "q@r.example", "s@t.example", "u@v.example"] }, "11 addresses"],
		[{ addrs: [5] }, "a number"], [{ addrs: [["a@b.example"]] }, "a nested list"],
		[{ id: "x1", addrs: ["a@b.example"] }, "bad id"], [{ name: 5, addrs: ["a@b.example"] }, "name type"],
		[{}, "no addrs"]];
	for (i = 0; i < bad_contacts.length; i++)
		err(call(A(), "contact.set", bad_contacts[i][0]), "invalid", "contact.set: " + bad_contacts[i][1]);
	Kit.eq(refused(call(A(), "contact.set", { addrs: ["a@b.example, c@d.example"] }), "invalid", "addrs",
		"two addresses in one").index, 0, "the bad address's index");
	Kit.eq(refused(call(A(), "contact.set", { addrs: ["a@b.example", "x <<y"] }), "invalid", "addrs",
		"the second one bad").index, 1, "passed through as index");
	err(call(A(), "contact.suggest", { q: "a", limit: 51 }), "invalid", "suggest limit 51");
	err(call(A(), "contact.suggest", {}), "invalid", "suggest without q");
	ok(call(A(), "contact.delete", { id: cid }), "contact.delete");
	Kit.eq(ok(call(A(), "contact.list", {}), "list").contacts, [], "deleted");

	// --- draft.send end to end: one local recipient, one copy
	var sd = new_api_draft(A(), { to: "bob", subject: "Hello Bob", body: "Hi\r\n" });
	var k1 = key();
	var bob_before = store_for(BOB).list("inbox").total;
	ev = A();
	r = ok(call(ev, "draft.send", { id: sd.id, key: k1 }), "send");
	Kit.assert(/^p[0-9]+$/.test(r.id), "the Sent copy's id " + r.id);
	Kit.assert(/^<[0-9a-f]{24}\.[0-9A-F]+@vert\.example>$/.test(r.message_id), "Message-ID " + r.message_id);
	Kit.eq(r.warning, undefined, "no warning");
	var sent = r;
	var copies = with_id(shared(), sent.message_id);
	Kit.eq(copies.length, 1, "one copy");
	Kit.eq(copies[0].h.to_ext, String(BOB), "to Bob");
	Kit.eq(store_for(BOB).list("inbox").total, bob_before + 1, "in Bob's Inbox");
	// a sent draft is "not found" with reason "sent" (the completed-send
	// index), so a page holding it can say so instead of saving it again
	refused(call(A(), "draft.get", { id: sd.id }), "notfound", "sent", "the draft is gone: sent");
	refused(call(A(), "draft.save", { id: sd.id, rev: 1, body: "again" }), "notfound", "sent", "save of a sent draft");
	// a draft claimed by a send under way (another window): "sending",
	// not a plain notfound, so the page does not offer to keep it as new
	// (the forced claim's time is its files' time: the clock is real here)
	var clock_was = clock;
	clock = time();
	var busy_d = new_api_draft(A(), { subject: "being sent" });
	var droot = W.S.user_dir(W.ALICE) + "webmail/drafts/";
	Kit.assert(file_rename(droot + busy_d.id, droot + busy_d.id + ".sending"), "claim forced");
	refused(call(A(), "draft.get", { id: busy_d.id }), "notfound", "sending", "a draft being sent: get");
	refused(call(A(), "draft.save", { id: busy_d.id, rev: 1, body: "x" }), "notfound", "sending", "a draft being sent: save");
	var sg = call(A(), "draft.get", { id: busy_d.id });
	Kit.assert(typeof sg.age === "number" && sg.age >= 0 && sg.age < 600, "the claim's heartbeat age: " + JSON.stringify(sg));
	// the drafts lock busy just after the draft was found missing: still
	// "sending" (the claim is checked without the lock), never a plain
	// notfound that would invite saving it as a new draft
	(function () {
		var ev = A();
		var real = ev.userdata(ALICE);
		var takes = 0;
		var ud = { dir: real.dir, files: real.files, read: function () {
			return real.read.apply(real, arguments);
		}, write: function () {
			return real.write.apply(real, arguments);
		}, update: function (name) {
			if (name === "drafts" && ++takes > 1)
				throw { code: "conflict", message: "busy (test)" };
			return real.update.apply(real, arguments);
		} };
		ev.lib_ctx.userdata = ud;
		refused(call(ev, "draft.get", { id: busy_d.id }), "notfound", "sending", "lock busy after get: still sending");
	})();
	// a claim whose heartbeat is older than the live window: "stalled"
	clock += ApiSend.LIVE + 5;
	sg = call(A(), "draft.get", { id: busy_d.id });
	refused(sg, "notfound", "stalled", "an abandoned claim: stalled");
	Kit.assert(sg.age >= ApiSend.LIVE, "with its age: " + sg.age);
	refused(call(A(), "draft.save", { id: busy_d.id, rev: 1, body: "x" }), "notfound", "stalled", "save: stalled");
	clock = clock_was;
	Kit.assert(file_rename(droot + busy_d.id + ".sending", droot + busy_d.id), "claim undone");
	ok(call(A(), "draft.delete", { id: busy_d.id }), "cleaned up");
	var gone = new_api_draft(A(), { subject: "to delete" });
	ok(call(A(), "draft.delete", { id: gone.id }), "deleted");
	var g2 = call(A(), "draft.get", { id: gone.id });
	Kit.eq([g2.error, g2.reason], ["notfound", undefined], "a deleted draft: notfound, no reason");
	Kit.eq(ok(call(A(), "folder.list", {}), "folder.list").drafts, 1, "drafts count after the send");
	// the same key: the recorded outcome, nothing sent again
	r = ok(call(A(), "draft.send", { id: sd.id, key: k1 }), "send again");
	Kit.eq([r.id, r.message_id], [sent.id, sent.message_id], "the same outcome");
	Kit.eq(with_id(shared(), sent.message_id).length, 1, "still one copy");
	// the key with another draft
	var other = new_api_draft(A(), { to: "bob", body: "x" });
	refused(call(A(), "draft.send", { id: other.id, key: k1 }), "conflict", "key", "a used key");
	// key and id checks
	err(call(A(), "draft.send", { id: other.id, key: "short" }), "invalid", "key too short");
	err(call(A(), "draft.send", { id: other.id, key: "has spaces in it!!" }), "invalid", "key characters");
	err(call(A(), "draft.send", { id: other.id }), "invalid", "no key");
	err(call(A(), "draft.send", { id: other.id, key: key(), confirm_resend: "yes" }), "invalid",
		"confirm_resend type");
	// refusals carry their reasons
	var none = new_api_draft(A(), { subject: "nobody", body: "x" });
	refused(call(A(), "draft.send", { id: none.id, key: key() }), "invalid", "norecipients", "no recipients");
	var lots = [];
	for (i = 0; i < 51; i++)
		lots.push("r" + i + "@example.org");
	var many = new_api_draft(A(), { to: lots.join(", "), body: "x" });
	r = refused(call(A(), "draft.send", { id: many.id, key: key() }), "invalid", "toomany", "51 recipients");
	Kit.eq(r.message, W.STRINGS["errors.invalid_toomany"].replace("{count}", "50"), "the count in the text");
	Kit.eq(r.count, 50, "count: max_recipients");
	// the address parser's own cap (100 entries) names its own number
	lots = [];
	for (i = 0; i < 101; i++)
		lots.push("q" + i + "@example.org");
	var over = new_api_draft(A(), { to: lots.join(", "), body: "x" });
	r = call(A(), "draft.send", { id: over.id, key: key() });
	Kit.eq([r.error, r.reason, r.count], ["invalid", "toomany", 100], "101 entries: the parser's cap");
	Kit.eq(r.message, W.STRINGS["errors.invalid_toomany"].replace("{count}", "100"), "at most 100");
	var nolimit = {};
	for (var ck in W.cfg)
		nolimit[ck] = W.cfg[ck];
	nolimit.max_recipients = 0;
	r = call(A({ config: nolimit }), "draft.send", { id: over.id, key: key() });
	Kit.eq([r.reason, r.count], ["toomany", 100], "no recipient limit: still the parser's 100, never 0");
	Kit.assert(!/ 0 /.test(r.message), "never at most 0: " + r.message);
	// the address library's reasons, each with a text of its own
	var addr_cases = [["Name <a@b.example", "unbalanced"], ["a@b@c.example", "syntax"],
		["group: a@b.example;", "group"], ["nosuchuser", "unknown"]];
	for (i = 0; i < addr_cases.length; i++) {
		var ad = new_api_draft(A(), { to: "bob, " + addr_cases[i][0], body: "x" });
		r = refused(call(A(), "draft.send", { id: ad.id, key: key() }), "invalid", addr_cases[i][1],
			"address " + addr_cases[i][0]);
		Kit.eq(r.index, 1, addr_cases[i][1] + ": index among the entries");
		Kit.assert(W.STRINGS.hasOwnProperty("errors.invalid_" + addr_cases[i][1]), "a text of its own");
	}
	r = refused(call(A({ user: { deny: [0] } }), "draft.send", { id: other.id, key: key() }), "forbidden",
		"recipient", "a recipient the user may not send to");
	Kit.eq([r.index, r.entry], [0, "bob"], "which recipient");
	// A local recipient whose mailbox is full: refused, with a text of its own
	r = refused(call(A({ full: [BOB] }), "draft.send", { id: other.id, key: key() }), "forbidden",
		"mailbox_full", "a recipient with a full mailbox");
	Kit.eq([r.index, r.entry], [0, "bob"], "which recipient is full");
	Kit.eq(r.message, W.STRINGS["errors.forbidden_mailbox_full"], "the mailbox-full text");
	var unknown = new_api_draft(A(), { to: "nosuchuser", body: "x" });
	r = refused(call(A(), "draft.send", { id: unknown.id, key: key() }), "invalid", "unknown", "an unknown name");
	Kit.eq(r.index, 0, "index");
	// "sending": a second send while the first is delivering
	var during = null;
	ApiSend.hook = function (point) {
		if (point === "delivery" && during === null)
			during = call(A(), "draft.send", { id: other.id, key: key() });
	};
	try {
		r = ok(call(A(), "draft.send", { id: other.id, key: key() }), "the first send");
	} finally {
		ApiSend.hook = null;
	}
	refused(during, "conflict", "sending", "a second send meanwhile");
	Kit.eq(with_id(shared(), r.message_id).length, 1, "sent once");

	// --- maybe_sent: listed; sent again only with confirm_resend
	var ms = new_api_draft(A(), { to: "bob", body: "maybe" });
	var mctx = context();
	Drafts.claim(mctx, ms.id);
	Drafts.release(mctx, ms.id, { maybe_sent: clock });
	var row = null;
	r = ok(call(A(), "draft.list", {}), "list");
	for (i = 0; i < r.drafts.length; i++)
		if (r.drafts[i].id === ms.id)
			row = r.drafts[i];
	Kit.assert(row !== null && row.maybe_sent === clock, "the row carries maybe_sent: " + JSON.stringify(row));
	refused(call(A(), "draft.send", { id: ms.id, key: key() }), "conflict", "maybe_sent", "without confirmation");
	r = ok(call(A(), "draft.send", { id: ms.id, key: key(), confirm_resend: true }), "confirmed");
	Kit.eq(with_id(shared(), r.message_id).length, 1, "sent");

	// --- an interrupted send, completed by a later draft.list
	var cut = new_api_draft(A(), { to: "bob", body: "interrupted" });
	ApiSend.hook = function (point) {
		if (point === "saved") {
			ApiSend.hook = null;
			throw { crash: true };
		}
	};
	try {
		err(call(A(), "draft.send", { id: cut.id, key: key() }), "internal", "the process dies after the save");
	} finally {
		ApiSend.hook = null;
	}
	refused(call(A(), "draft.get", { id: cut.id }), "notfound", "sending", "claimed meanwhile");
	clock += 5 * 60;
	r = ok(call(A(), "draft.list", {}), "list soon after");
	Kit.eq(r.recovered, 0, "a claim inside the live window is left alone");
	clock += ApiSend.LIVE;
	r = ok(call(A(), "draft.list", {}, true), "background list");
	Kit.eq(r.recovered, 0, "no recovery in a background request");
	r = ok(call(A(), "draft.list", {}), "list later");
	Kit.eq(r.recovered, 1, "recovered by draft.list");
	for (i = 0; i < r.drafts.length; i++)
		Kit.assert(r.drafts[i].id !== cut.id, "not back in the drafts (it was sent)");
	var cut_body = 0;
	var bst = store_for(BOB);
	var bitems = bst.list("inbox", { limit: 100 }).items;
	for (i = 0; i < bitems.length; i++)
		if (/interrupted/.test(base64_decode(bst.get(bitems[i].id).text.b64)))
			cut_body++;
	Kit.eq(cut_body, 1, "Bob has the interrupted message once");
	Kit.eq(ok(call(A(), "draft.list", {}), "list again").recovered, 0, "nothing left to recover");

	// --- folder.list sweeps only for a claim that looks interrupted: its
	// record (else the claim) not written for the live window; a send
	// under way costs it no drafts lock
	Kit.eq(Api.SEND_LIVE, ApiSend.LIVE, "Api.SEND_LIVE is Send.LIVE");
	(function () {
		var clock_was = clock;
		clock = time();	// the claim's file dates are real
		var yd = new_api_draft(A(), { subject: "young claim" });
		var droot2 = W.S.user_dir(W.ALICE) + "webmail/drafts/";
		Kit.assert(file_rename(droot2 + yd.id, droot2 + yd.id + ".sending"), "claim forced");
		var ev = A();
		var real = ev.userdata(ALICE);
		var takes = 0;
		ev.lib_ctx.userdata = { dir: real.dir, files: real.files,
			read: function () { return real.read.apply(real, arguments); },
			write: function () { return real.write.apply(real, arguments); },
			update: function () { return real.update.apply(real, arguments); },
			locked: function (name) {
				if (name === "drafts")
					takes++;
				return real.locked.apply(real, arguments);
			} };
		ok(call(ev, "folder.list", {}), "folder.list with a young claim");
		Kit.eq(takes, 0, "a young claim: no sweep, no drafts lock");
		clock += ApiSend.LIVE + 5;
		ok(call(ev, "folder.list", {}), "folder.list with an old claim");
		Kit.assert(takes > 0, "an old claim: swept (" + takes + ")");
		clock = clock_was;
		Kit.assert(file_rename(droot2 + yd.id + ".sending", droot2 + yd.id) || !file_isdir(droot2 + yd.id + ".sending"),
			"claim undone");
		if (file_isdir(droot2 + yd.id))
			ok(call(A(), "draft.delete", { id: yd.id }), "cleaned up");
	})();

	// --- an interrupted send with a recipient missing (the Internet copy
	// saved, the local one not), its heartbeat 700 s old: completed by a
	// draft.list, and by a folder.list (an open mailbox finishes it on
	// its own); a background request does neither
	["draft.list", "folder.list"].forEach(function (method) {
		var subj = "missing recipient, " + method;
		var md = new_api_draft(A(), { to: "ext@example.org, bob", subject: subj, body: subj });
		ApiSend.hook = function (point, kind) {
			if (point === "delivery" && kind === "local") {
				ApiSend.hook = null;
				throw { crash: true };
			}
		};
		try {
			call(A(), "draft.send", { id: md.id, key: key() });
		} finally {
			ApiSend.hook = null;
		}
		refused(call(A(), "draft.get", { id: md.id }), "notfound", "sending", method + ": claimed");
		clock += 700;
		ok(call(A(), method, {}, true), method + ": background");
		refused(call(A(), "draft.get", { id: md.id }), "notfound", "stalled",
			method + ": not recovered by a background request");
		var res = ok(call(A(), method, {}), method + ": 700 s later");
		if (method === "draft.list")
			Kit.eq(res.recovered, 1, method + ": recovered");
		refused(call(A(), "draft.get", { id: md.id }), "notfound", "sent", method + ": the send completed");
		var copies = 0;
		var bs = store_for(BOB);
		var its = bs.list("inbox", { limit: 100 }).items;
		for (var j = 0; j < its.length; j++)
			if (base64_decode(bs.get(its[j].id).text.b64).indexOf(subj) >= 0)
				copies++;
		Kit.eq(copies, 1, method + ": Bob has it once");
	});

	// --- an interrupted send with nothing saved: back as maybe_sent
	var lost = new_api_draft(A(), { to: "bob", body: "lost" });
	ApiSend.hook = function (point) {
		if (point === "delivery") {
			ApiSend.hook = null;
			throw { crash: true };
		}
	};
	try {
		call(A(), "draft.send", { id: lost.id, key: key() });
	} finally {
		ApiSend.hook = null;
	}
	clock += 3700;
	r = ok(call(A(), "draft.list", {}), "list after the lost send");
	row = null;
	for (i = 0; i < r.drafts.length; i++)
		if (r.drafts[i].id === lost.id)
			row = r.drafts[i];
	Kit.assert(row !== null && row.maybe_sent > 0, "back with maybe_sent: " + JSON.stringify(row));
	Kit.eq(r.recovered, 1, "an unknown outcome resolved the claim: counted");

	// --- the bound on recoveries per list: three stale sends, two per call
	var stale_ids = [];
	for (i = 0; i < 3; i++) {
		var sdr = new_api_draft(A(), { to: "bob", body: "stale " + i });
		stale_ids.push(sdr.id);
		ApiSend.hook = function (point) {
			if (point === "saved") {
				ApiSend.hook = null;
				throw { crash: true };
			}
		};
		try {
			call(A(), "draft.send", { id: sdr.id, key: key() });
		} finally {
			ApiSend.hook = null;
		}
	}
	clock += 3700;
	// the mailbox cannot be opened: the list still comes back
	r = ok(call(A({ make_ctx: function () {
		throw new Error("simulated: the mail base cannot be opened");
	} }), "draft.list", {}), "list without a mailbox");
	Kit.eq(r.recovered, 0, "nothing recovered");
	Kit.assert(r.drafts instanceof Array && r.drafts.length > 0, "the drafts listed");
	Kit.eq(ok(call(A(), "folder.list", {}, true), "folder.list").drafts, r.drafts.length,
		"folder.list counts the drafts, not the claimed ones");
	Kit.eq(ok(call(A(), "draft.list", {}), "list").recovered, 2, "two recovered in one list");
	Kit.eq(ok(call(A(), "draft.list", {}), "list").recovered, 1, "the third in the next");
} finally {
	ApiSend.hook = null;
	for (var ci = 0; ci < contexts.length; ci++)
		contexts[ci].close();
	W.cleanup();
}
