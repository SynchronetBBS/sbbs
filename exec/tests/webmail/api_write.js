// Api, write side: the calls that change mail (message.set, move, delete,
// undelete, trash.empty, folder.create/rename/delete) and message.original,
// driven through Api.dispatch with a fake env over a real Store on
// scratch files; the per-id result shape, argument checks that keep a bad
// call from reaching the mailbox, ownership, background requests,
// sign-in, folder-name reasons and the user number in every reply

load(js.exec_dir + "kit.js");
var W = Kit.api_world();
var Api = W.Api;
var D = W.D;
var ALICE = W.ALICE;
var BOB = W.BOB;
var make_env = W.make_env;
var call = W.call;
var err = W.err;
var save = W.save;

var real_log = Api.log;
Api.log = function () {};

function to_alice(subject, extra)
{
	var h = { to_ext: String(ALICE), to: "alice", from: "Carol", subject: subject };
	for (var k in extra || {})
		h[k] = extra[k];
	return "s" + save(h);
}

// A shared header from outside the library (fresh handle)
function shdr(id)
{
	return W.open_shared().get_msg_header(false, parseInt(id.substr(1), 10), false);
}

function ids_of(ev, folder)
{
	var r = call(ev, "message.list", { folder: folder, limit: 100 });
	Kit.eq(r.ok, true, "list " + folder + ": " + JSON.stringify(r));
	var out = [];
	for (var i = 0; i < r.items.length; i++)
		out.push(r.items[i].id);
	return out.sort();
}

// A successful write result: ok, a results array, a state token that is
// also the reply's
function written(reply, what)
{
	Kit.assert(reply.results instanceof Array && reply.results.length === 1, what + ": one call");
	var r = reply.results[0][1];
	Kit.eq(r.ok, true, what + ": " + JSON.stringify(r));
	Kit.eq(typeof r.state, "string", what + ": state in the result");
	Kit.assert(r.state !== "", what + ": a state token");
	Kit.eq(reply.state, r.state, what + ": the reply's state is the result's");
	return r;
}

function write(ev, method, args, what)
{
	return written(Api.dispatch(ev, { calls: [[method, args, 1]], as: W.as_of(ev) }), what || method);
}

try {
	var alice = W.sign_in(ALICE);
	var bob = W.sign_in(BOB);
	var A1 = to_alice("one");
	var A2 = to_alice("two");
	var A3 = to_alice("three");
	var A4 = to_alice("four");
	var AS = to_alice("spammy", { attr: D.MSG_SPAM });
	var B1 = "s" + save({ to_ext: String(BOB), to: "bob", from: "Eve", subject: "for bob" });
	var ev;
	var r;
	var i;

	// --- "expired" keeps the mailbox library's meaning (too old to return
	// to the Inbox); the session-ended text has a key of its own
	Kit.assert(/Inbox/.test(W.STRINGS["errors.expired"]), "errors.expired: " + W.STRINGS["errors.expired"]);
	Kit.assert(/sign in/i.test(W.STRINGS["errors.signed_out"]), "errors.signed_out");
	var api_src = new File(Kit.exec_root() + "../webmail/lib/api.js");
	Kit.assert(api_src.open("r"), "read api.js");
	var fallback = /expired: "([^"]*)"/.exec(api_src.read());
	api_src.close();
	Kit.eq(fallback !== null ? fallback[1] : null, W.STRINGS["errors.expired"], "the built-in text matches");

	// --- the user number in every reply
	ev = make_env({ cookie: alice });
	Kit.eq(Api.dispatch(ev, { calls: [["session.get", {}, 1]] }).usernum, ALICE, "usernum: signed in");
	Kit.eq(Api.dispatch(make_env(), { calls: [["session.get", {}, 1]] }).usernum, 0,
		"usernum: not signed in");
	Kit.eq(Api.dispatch(make_env({ cookie: alice }), { calls: "x" }).usernum, 0,
		"usernum: a malformed request");
	Kit.eq(Api.dispatch(make_env({ cookie: alice, has_header: false }),
		{ calls: [["folder.list", {}, 1]] }).usernum, 0, "usernum: a refused request");
	r = Api.dispatch(make_env({ cookie: alice }), { calls: [["session.login",
		{ user: "bob", password: "pw" + BOB }, 1]] });
	Kit.eq(r.usernum, BOB, "usernum: the user the request ended as");
	alice = W.sign_in(ALICE);	// that sign-in ended the session the request had
	r = Api.dispatch(make_env({ cookie: bob }), { calls: [["message.set", { ids: [A1], read: true }, 1]], as: BOB });
	Kit.eq(r.usernum, BOB, "usernum: a write");

	// --- "as": the page's user.  A mismatch refuses the whole request
	// (reads and writes) before anything runs; a write without it is
	// refused whole; it must be a positive integer
	ev = make_env({ cookie: alice });
	r = Api.dispatch(ev, { as: BOB, calls: [["folder.list", {}, 1], ["trash.empty", {}, 2],
		["folder.create", { name: "stale" }, 3]] });
	err(r.results[0][1], "stale", "as mismatch: a read");
	err(r.results[1][1], "stale", "as mismatch: trash.empty");
	err(r.results[2][1], "stale", "as mismatch: folder.create");
	Kit.eq(ev.made, 0, "as mismatch: no mailbox opened");
	Kit.eq(r.usernum, ALICE, "as mismatch: the reply names the real user");
	ev = make_env({ cookie: alice });
	r = Api.dispatch(ev, { calls: [["message.list", { folder: "inbox" }, 1], ["folder.create", { name: "noas" }, 2]] });
	err(r.results[0][1], "invalid", "a write without as: the read in it too");
	err(r.results[1][1], "invalid", "a write without as");
	Kit.eq(ev.made, 0, "a write without as: no mailbox opened");
	r = Api.dispatch(make_env({ cookie: alice }), { calls: [["folder.list", {}, 1]] });
	Kit.eq(r.results[0][1].ok, true, "a read without as");
	r = Api.dispatch(make_env({ cookie: alice }), { as: ALICE, calls: [["folder.list", {}, 1]] });
	Kit.eq(r.results[0][1].ok, true, "as matches");
	var bad_as = [0, -1, 1.5, "1", null, true, 1e12];
	for (i = 0; i < bad_as.length; i++) {
		r = Api.dispatch(make_env({ cookie: alice }), { as: bad_as[i], calls: [["folder.list", {}, 1]] });
		Kit.eq(r.error, "invalid", "as " + JSON.stringify(bad_as[i]) + ": the request is invalid");
		Kit.eq(r.results, [], "as " + JSON.stringify(bad_as[i]) + ": no call ran");
	}
	r = Api.dispatch(make_env(), { as: ALICE, calls: [["session.get", {}, 1]] });
	Kit.eq(r.results[0][1].ok, true, "as with no session: session.get still answers (signed out)");
	Kit.assert(!/folder/.test(String(ids_of(make_env({ cookie: alice }), "inbox"))), "nothing created");

	// --- ids: 1 to 200 valid message ids, no duplicates; anything else is
	// invalid for the whole call and never reaches the mailbox
	var many = [];
	for (i = 1; i <= 201; i++)
		many.push("s" + (100000 + i));
	var bad_ids = [[], many, ["../x"], [A1, "x1"], [A1, A1], "s1", [1], [null], [A1, ["s2"]],
		{ 0: A1, length: 1 }, ["s0"], ["s01"], ["p12345678901"], [String.fromCharCode(0x100)]];
	for (i = 0; i < bad_ids.length; i++) {
		ev = make_env({ cookie: alice });
		err(call(ev, "message.set", { ids: bad_ids[i], read: true }), "invalid", "bad ids #" + i);
		err(call(ev, "message.delete", { ids: bad_ids[i] }), "invalid", "bad ids (delete) #" + i);
		Kit.eq(ev.made, 0, "bad ids #" + i + ": the mailbox is not opened");
	}
	// moves between the bases are costly: move, delete and undelete take
	// at most 50 ids (set takes 200)
	var fifty = many.slice(0, 50);
	var fifty_one = many.slice(0, 51);
	var capped = ["message.move", "message.delete", "message.undelete"];
	for (i = 0; i < capped.length; i++) {
		var cargs = function (ids) {
			return capped[i] === "message.move" ? { ids: ids, folder: "archive" } : { ids: ids };
		};
		ev = make_env({ cookie: alice });
		err(call(ev, capped[i], cargs(fifty_one)), "invalid", capped[i] + ": 51 ids");
		Kit.eq(ev.made, 0, capped[i] + ": 51 ids: the mailbox is not opened");
		r = write(make_env({ cookie: alice }), capped[i], cargs(fifty), capped[i] + ": 50 ids");
		Kit.eq(r.results.length, 50, capped[i] + ": 50 results");
		Kit.eq(r.results[0], { id: many[0], error: "notfound" }, capped[i] + ": 50 ids, each notfound");
	}
	// move: only move targets (inbox, archive, a user folder)
	var not_targets = ["sent", "spam", "trash", "starred", "all", "drafts", "u:", "u:$Trash", "Inbox"];
	for (i = 0; i < not_targets.length; i++) {
		ev = make_env({ cookie: alice });
		err(call(ev, "message.move", { ids: [A1], folder: not_targets[i] }), "invalid",
			"move to " + not_targets[i]);
		Kit.eq(ev.made, 0, "move to " + not_targets[i] + ": the mailbox is not opened");
	}
	r = write(make_env({ cookie: alice }), "message.move", { ids: ["s999999"], folder: "inbox" }, "move to inbox");
	Kit.eq(r.results, [{ id: "s999999", error: "notfound" }], "inbox is a target");
	r = write(make_env({ cookie: alice }), "message.move", { ids: ["s999999"], folder: "archive" }, "to archive");
	Kit.eq(r.results, [{ id: "s999999", error: "notfound" }], "archive is a target");
	err(call(make_env({ cookie: alice }), "message.move", { ids: ["s999999"], folder: "u:nonesuch" }), "notfound",
		"a user folder id is a target (this one does not exist)");

	ev = make_env({ cookie: alice });
	err(call(ev, "message.delete", {}), "invalid", "ids required");
	err(call(ev, "message.set", { ids: [A1] }), "invalid", "set: none of read, starred, spam");
	err(call(ev, "message.set", { ids: [A1], read: 1 }), "invalid", "set: read not a boolean");
	err(call(ev, "message.set", { ids: [A1], read: true, junk: true }), "invalid", "set: unknown argument");
	err(call(ev, "message.move", { ids: [A1] }), "invalid", "move: folder required");
	err(call(ev, "message.move", { ids: [A1], folder: "u:$Trash" }), "invalid", "move: bad folder id");
	err(call(ev, "message.original", { id: "../x" }), "invalid", "original: bad id");
	err(call(ev, "folder.rename", { id: "inbox" + "x", name: "a" }), "invalid", "rename: bad folder id");
	err(call(ev, "trash.empty", { x: 1 }), "invalid", "trash.empty takes no arguments");
	Kit.eq(ev.made, 0, "invalid arguments: the mailbox is not opened");
	// 200 ids are accepted: each one that does not exist is notfound
	r = write(make_env({ cookie: alice }), "message.set", { ids: many.slice(0, 200), read: true }, "200 ids");
	Kit.eq(r.results.length, 200, "200 results");
	for (i = 0; i < 200; i++)
		Kit.eq(r.results[i], { id: many[i], error: "notfound" }, "200 ids: #" + i);

	// --- not signed in, a guest, a user who fails the access requirements:
	// "auth" for every write, the mailbox never opened
	var writes = [["message.set", { ids: [A1], read: true }], ["message.move", { ids: [A1], folder: "archive" }],
		["message.delete", { ids: [A1] }], ["message.undelete", { ids: ["p1"] }], ["trash.empty", {}],
		["folder.create", { name: "x" }], ["folder.rename", { id: "u:x", name: "y" }],
		["folder.delete", { id: "u:x" }], ["message.original", { id: A1 }]];
	var nobody = [undefined, W.cookie_for(W.GUEST), W.cookie_for(W.DENIED)];
	for (var n = 0; n < nobody.length; n++) {
		for (i = 0; i < writes.length; i++) {
			ev = make_env({ cookie: nobody[n] });
			err(call(ev, writes[i][0], writes[i][1]), "auth", "user #" + n + " " + writes[i][0]);
			Kit.eq(ev.made, 0, "user #" + n + " " + writes[i][0] + ": no mailbox");
		}
	}
	Kit.eq(shdr(A1).attr & D.MSG_READ, 0, "A1 still unread (bob's try was refused)");

	// --- another user's message: forbidden for that id
	r = Api.dispatch(make_env({ cookie: bob }), { calls: [["message.set", { ids: [A1], read: true }, 1]], as: BOB });
	Kit.eq(r.results[0][1].results, [{ id: A1, error: "forbidden" }], "bob sets alice's message");
	Kit.eq(shdr(A1).attr & D.MSG_READ, 0, "A1 still unread");
	err(call(make_env({ cookie: bob }), "message.original", { id: A1 }), "forbidden",
		"bob reads alice's original");

	// --- a background request cannot write (the client's own refreshes);
	// it can still read
	ev = make_env({ cookie: alice });
	r = Api.dispatch(ev, { background: true, as: ALICE, calls: [["message.set", { ids: [A1], read: true }, 1],
		["message.delete", { ids: [A2] }, 2], ["folder.create", { name: "bg" }, 3],
		["message.original", { id: A1 }, 4], ["message.list", { folder: "inbox" }, 5]] });
	err(r.results[0][1], "forbidden", "background set");
	err(r.results[1][1], "forbidden", "background delete");
	err(r.results[2][1], "forbidden", "background folder.create");
	Kit.eq(r.results[3][1].ok, true, "background original: a read");
	Kit.eq(r.results[4][1].ok, true, "background list");
	Kit.eq(shdr(A1).attr & D.MSG_READ, 0, "background: A1 not marked read");
	Kit.eq(ids_of(make_env({ cookie: alice }), "inbox"), [A1, A2, A3, A4].sort(), "background: nothing deleted");

	// --- message.set: read, starred, spam
	ev = make_env({ cookie: alice });
	r = write(ev, "message.set", { ids: [A1, A2], read: true }, "set read");
	Kit.eq(r.results, [{ id: A1, ok: true, new_id: A1 }, { id: A2, ok: true, new_id: A2 }], "set read results");
	Kit.eq(shdr(A1).attr & D.MSG_READ, D.MSG_READ, "A1 read");
	Kit.eq(ev.made, 1, "one mailbox per request");
	r = write(make_env({ cookie: alice }), "message.set", { ids: [A1], read: false, starred: true }, "set star");
	Kit.eq([shdr(A1).attr & D.MSG_READ, shdr(A1).attr & D.MSG_VALIDATED], [0, D.MSG_VALIDATED], "A1 unread, starred");
	// "Not spam": a Spam message goes back to the Inbox listing
	ev = make_env({ cookie: alice });
	Kit.eq(ids_of(ev, "spam"), [AS], "AS in Spam");
	r = write(ev, "message.set", { ids: [AS], spam: false }, "not spam");
	Kit.eq(r.results, [{ id: AS, ok: true, new_id: AS }], "not spam result");
	Kit.eq(ids_of(ev, "spam"), [], "Spam empty");
	Kit.eq(ids_of(ev, "inbox"), [A1, A2, A3, A4, AS].sort(), "AS in the Inbox");
	write(ev, "message.set", { ids: [AS], spam: true }, "mark as spam");
	Kit.eq(ids_of(ev, "spam"), [AS], "AS back in Spam");

	// --- delete: one stale id and one valid id
	ev = make_env({ cookie: alice });
	r = write(ev, "message.delete", { ids: [A2] }, "delete A2");
	var P2 = r.results[0].new_id;
	Kit.assert(/^p[0-9]+$/.test(P2), "A2 now private: " + P2);
	r = write(make_env({ cookie: alice }), "message.delete", { ids: [A2, A3] }, "delete stale + valid");
	Kit.eq(r.results[0], { id: A2, error: "notfound" }, "the stale id: notfound");
	Kit.eq(r.results[1].id, A3, "the valid id");
	Kit.eq(r.results[1].ok, true, "the valid id: ok");
	var P3 = r.results[1].new_id;
	Kit.eq(ids_of(make_env({ cookie: alice }), "trash"), [P2, P3].sort(), "both in Trash");
	// another user's id in a selection: forbidden for that one only
	r = write(make_env({ cookie: alice }), "message.delete", { ids: [B1] }, "delete bob's");
	Kit.eq(r.results, [{ id: B1, error: "forbidden" }], "bob's message: forbidden");

	// --- undelete: back to the Inbox
	r = write(make_env({ cookie: alice }), "message.undelete", { ids: [P3] }, "undelete");
	Kit.eq(r.results[0].ok, true, "undeleted");
	var A3b = r.results[0].new_id;
	Kit.assert(/^s[0-9]+$/.test(A3b), "back in the shared base: " + A3b);
	Kit.assert(ids_of(make_env({ cookie: alice }), "inbox").indexOf(A3b) >= 0, "in the Inbox again");

	// --- folders: create (names with odd characters), reasons, move,
	// rename, delete
	ev = make_env({ cookie: alice });
	var NAME = "Tax 100% \\ done";
	r = write(ev, "folder.create", { name: NAME }, "folder.create");
	Kit.eq(r.folder.name, NAME, "the folder's name");
	var F = r.folder.id;
	Kit.assert(/^u:/.test(F), "a user folder id: " + F);
	var reasons = [["   ", "empty"], ["tax 100% \\ DONE", "duplicate"], ["Inbox", "reserved"],
		[new Array(66).join("x"), "toolong"], ["a\x01b", "invalid"]];
	for (i = 0; i < reasons.length; i++) {
		r = call(ev, "folder.create", { name: reasons[i][0] });
		err(r, "invalid", "folder name " + reasons[i][1]);
		Kit.eq(r.reason, reasons[i][1], "the reason " + reasons[i][1]);
	}
	// an overlong name reaches the mailbox and comes back "toolong"; past
	// 4096 bytes it is refused before (no reason)
	r = call(ev, "folder.create", { name: new Array(4097).join("x") });
	err(r, "invalid", "a 4096-byte name");
	Kit.eq(r.reason, "toolong", "a 4096-byte name: toolong");
	r = call(ev, "folder.create", { name: new Array(4098).join("x") });
	err(r, "invalid", "a 4097-byte name");
	Kit.eq(r.reason, undefined, "a 4097-byte name: refused by the API, no reason");
	r = call(ev, "folder.rename", { id: F, name: new Array(1001).join("x") });
	Kit.eq(r.reason, "toolong", "rename to a long name: toolong");
	r = call(ev, "folder.create", { name: "a\xE2\x80\xAEb" });
	Kit.eq(r.reason, "invalid", "a bidirectional control: invalid");
	r = call(ev, "message.move", { ids: [A4], folder: "u:nonesuch" });
	err(r, "notfound", "move to a folder that does not exist");
	Kit.eq(r.reason, undefined, "no reason on other errors");
	r = write(ev, "message.move", { ids: [A4], folder: F }, "move to the folder");
	var P4 = r.results[0].new_id;
	Kit.assert(/^p[0-9]+$/.test(P4), "A4 now private: " + P4);
	Kit.eq(ids_of(ev, F), [P4], "A4 in the folder");
	Kit.assert(ids_of(ev, "inbox").indexOf(A4) < 0, "A4 left the Inbox");
	r = write(ev, "folder.rename", { id: F, name: "Renamed" }, "folder.rename");
	Kit.eq(r.folder.name, "Renamed", "renamed");
	Kit.eq(r.results, [], "no per-message errors");
	var F2 = r.folder.id;
	Kit.eq(ids_of(ev, F2), [P4], "the message went with it");
	write(ev, "folder.create", { name: "Other" }, "a second folder");
	r = call(ev, "folder.rename", { id: F2, name: "other" });
	err(r, "invalid", "rename to a duplicate");
	Kit.eq(r.reason, "duplicate", "rename: duplicate");
	r = write(ev, "folder.delete", { id: F2 }, "folder.delete");
	Kit.eq(r.results, [{ id: P4, ok: true, new_id: P4 }], "folder.delete: its message to Trash");
	Kit.assert(ids_of(ev, "trash").indexOf(P4) >= 0, "P4 in Trash");
	err(call(ev, "folder.delete", { id: F2 }), "notfound", "the folder is gone");

	// --- trash.empty
	ev = make_env({ cookie: alice });
	var before = ids_of(ev, "trash");
	Kit.assert(before.length >= 2, "something in Trash");
	r = write(ev, "trash.empty", {}, "trash.empty");
	var emptied = [];
	for (i = 0; i < r.results.length; i++) {
		Kit.eq(r.results[i].ok, true, "emptied #" + i);
		Kit.eq(r.results[i].new_id, null, "emptied #" + i + ": gone");
		emptied.push(r.results[i].id);
	}
	Kit.eq(emptied.sort(), before, "every Trash message reported");
	Kit.eq(ids_of(ev, "trash"), [], "Trash empty");

	// --- Sent rows: a failed delivery carries the failure notice's id
	var pmb = Kit.email_base(W.S.user_dir(ALICE) + "mailbox");
	try {
		if (!pmb.save_msg({ from_ext: String(ALICE), from: "alice", to: "x@example.com", subject: "bounced",
			tags: "$Sent", id: "<bounced@test>" }, "body\r\n"))
			throw new Error("save sent copy: " + pmb.last_error);
		var PB = "p" + pmb.last_msg;
	} finally {
		pmb.close();
	}
	var NOTICE = to_alice("Undeliverable", { attr: D.MSG_NOREPLY, reply_id: "<bounced@test>" });
	r = call(make_env({ cookie: alice }), "message.list", { folder: "sent" });
	var row = null;
	for (i = 0; i < r.items.length; i++)
		if (r.items[i].id === PB)
			row = r.items[i];
	Kit.eq(row.delivery, { state: "failed", notice: NOTICE }, "the Sent row names the notice");

	// --- message.original
	r = call(make_env({ cookie: alice }), "message.original", { id: A1 });
	Kit.eq(r.ok, true, "original");
	Kit.eq(typeof r.original.rfc822, "string", "rfc822 block");
	Kit.assert(r.original.rfc822.indexOf("Subject: one") >= 0, "the subject in the block");
	Kit.assert(r.original.stored instanceof Array, "stored lines");
	Kit.eq(r.original.external_files, false, "no file attachments");
	err(call(make_env({ cookie: alice }), "message.original", { id: "s999999" }), "notfound", "original: none");

	// --- the mail base's age limit in folder.list (0 here: no limit set)
	r = call(make_env({ cookie: alice }), "folder.list", {});
	Kit.eq(r.inbox_days, 0, "inbox_days: none");
	var real_age = W.Store.prototype.inbox_max_age;
	try {
		W.Store.prototype.inbox_max_age = function () {
			return 30;
		};
		Kit.eq(call(make_env({ cookie: alice }), "folder.list", {}).inbox_days, 30, "inbox_days: 30");
		W.Store.prototype.inbox_max_age = function () {
			throw new Error("/secret/path");
		};
		r = call(make_env({ cookie: alice }), "folder.list", {});
		Kit.eq([r.ok, r.inbox_days], [true, 0], "inbox_days: unreadable is 0, the call still works");
	} finally {
		W.Store.prototype.inbox_max_age = real_age;
	}

	// --- a per-id code the API does not know is reported as "failed"; a
	// throwing write is the existing whole-call error
	var fake = { usernum: ALICE, store: {
		set: function () {
			return [{ id: A1, ok: false, error: "weird", message: "/secret/path" },
				{ id: A2, ok: true, new_id: A2, junk: "/secret" }];
		},
		remove: function () {
			throw { code: "conflict", message: "busy" };
		},
		state: function () {
			return "st";
		},
		close: function () {}
	} };
	ev = make_env({ cookie: alice, make_ctx: function () {
		return fake;
	} });
	r = write(ev, "message.set", { ids: [A1, A2], read: true }, "fake set");
	Kit.eq(r.results, [{ id: A1, error: "failed" }, { id: A2, ok: true, new_id: A2 }],
		"unknown code made failed; only known fields kept");
	err(call(ev, "message.delete", { ids: [A1] }), "conflict", "a thrown conflict");
} finally {
	Api.log = real_log;
	W.cleanup();
}
