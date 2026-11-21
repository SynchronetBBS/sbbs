// Store, write side at its limits: the Inbox age limit, hiding sent mail
// by Message-ID, side files that cannot be saved, long calls done in
// slices, quota with Trash, and tags or mail the Inbox cannot take back

load(js.exec_dir + "kit.js");
var JsonFiles = Kit.lib("mailbox_files");
var Tags = Kit.lib("mailbox_tags");
var Store = Kit.lib("mailbox");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var BOB = 60002;
var CAROL = 60003;
var DAVE = 60004;
var DAY = 24 * 60 * 60;
var T0 = 1700000000;	// the injected clock

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

// The test's shared handle, reopened (an open handle can serve stale
// buffered bytes after the library wrote the base)
function fresh()
{
	shared.close();
	shared = Kit.email_base(S.shared_path);
	return shared;
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

function from_user(usernum, to_usernum, subject, id)
{
	return save(fresh(), { from_ext: String(usernum), from: "User " + usernum, to_ext: String(to_usernum),
		to: "User " + to_usernum, subject: subject, id: id });
}

// Hooks on the private base of a store: hooks[name] returns a value to
// use instead of the real call's, undefined to make the call; calls[name]
// counts the calls
var hooks = {};
var calls = { put: 0, remove: 0, purge: 0, move: 0 };

function hook(mb, method, name)
{
	var real = mb[method];
	mb[method] = function () {
		calls[name]++;
		if (hooks[name]) {
			var r = hooks[name].apply(mb, arguments);
			if (r !== undefined)
				return r;
		}
		return real.apply(mb, arguments);
	};
}

function open_store(usernum, options)
{
	var st = new Store({ usernum: usernum, shared_path: S.shared_path, data_dir: S.data_dir,
		options: options || {}, now: function () { return clock; } });
	st.open();
	st.private_base = function (create) {
		var had = this.pmb;
		var mb = Store.prototype.private_base.call(this, create);
		if (mb !== null && mb !== had) {
			hook(mb, "put_msg_header", "put");
			hook(mb, "remove_msg", "remove");
			hook(mb, "purge", "purge");
			hook(mb, "move_msg", "move");
		}
		return mb;
	};
	stores.push(st);
	return st;
}

function peek(path, fn)
{
	var mb = Kit.email_base(path);
	try {
		return fn(mb);
	} finally {
		mb.close();
	}
}

function private_path(usernum)
{
	return S.user_dir(usernum) + "mailbox";
}

function phdr(num)	// Alice's private header (unexpanded), null if gone
{
	return peek(private_path(ALICE), function (mb) {
		var h = mb.get_msg_header(false, num, false);
		if (h === null || h === undefined || h.number != num || (h.attr & D.MSG_DELETE))
			return null;
		return h;
	});
}

function shdr(num)
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

function one(res, msg)
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

function count_id(path, msgid)	// live messages with this Message-ID
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

function trash_count(st)
{
	return st.list("trash", { limit: 100 }).total;
}

try {
	shared = Kit.email_base(S.shared_path);
	var ajf = new JsonFiles(S.user_dir(ALICE), "mailbox.");
	// Alice's private base exists (a filed message)
	peek(private_path(ALICE), function (mb) {
		save(mb, { to_ext: String(ALICE), subject: "filed", tags: Tags.ARCHIVE });
	});
	var st = open_store(ALICE, { trash_days: 30, quota_bytes: 0 });
	// --- inbox_max_age(): the shared base's max_age in days, 0 for none
	Kit.eq(st.inbox_max_age(), 0, "a scratch base has no age limit");
	var real_shared = st.shared_base;
	st.shared_base = function () { return { max_age: 45 }; };
	Kit.eq(st.inbox_max_age(), 45, "the base's max_age");
	st.shared_base = function () { return { max_age: undefined }; };
	Kit.eq(st.inbox_max_age(), 0, "no number: no limit");
	st.shared_base = function () { throw new Error("/secret/path"); };
	var age_err = null;
	try {
		st.inbox_max_age();
	} catch (e) {
		age_err = e;
	}
	Kit.assert(age_err instanceof Error && age_err.message.indexOf("/secret") < 0,
		"a failure leaves as the generic error: " + age_err);
	delete st.shared_base;	// the prototype's again
	Kit.eq(st.shared_base, real_shared, "restored");

	var max_age = 90;
	st.inbox_max_age = function () { return max_age; };

	// --- The Inbox age limit: mail older than the shared base's max_age
	// is never put back there (mail maintenance would delete it)
	var OLD = to_alice("old", "<old@test>", { when_imported_time: T0 - 200 * DAY });
	var YOUNG = to_alice("young", "<young@test>", { when_imported_time: T0 - 10 * DAY });
	var told = one(st.remove(["s" + OLD]), "delete old");
	var res = st.restore([told]);
	Kit.eq(res, [{ id: told, ok: true, new_id: told, folder: "archive", expired: true }],
		"old mail restored to Archive");
	Kit.eq(ptags(num_of(told)).folder, Tags.ARCHIVE, "old mail tagged Archive");
	Kit.eq([ptags(num_of(told)).was, phdr(num_of(told)).when_imported_time], ["", T0 - 200 * DAY],
		"origin cleared, import time kept");
	res = st.move([told], "inbox");
	err(res, 0, "expired", "move of old mail to the Inbox");
	Kit.eq(res[0].message, "too old to return to the Inbox", "expired message");
	Kit.assert(phdr(num_of(told)) !== null, "old mail stays private");
	var tyoung = one(st.remove(["s" + YOUNG]), "delete young");
	var syoung = one(st.restore([tyoung]), "restore young");
	Kit.assert(syoung.charAt(0) == "s" && shdr(num_of(syoung)) !== null, "young mail back in the Inbox");
	var OLDSPAM = to_alice("old spam", "<oldspam@test>", { when_imported_time: T0 - 200 * DAY,
		attr: D.MSG_SPAM });
	var tspam = one(st.remove(["s" + OLDSPAM]), "delete old spam");
	Kit.eq(st.restore([tspam])[0], { id: tspam, ok: true, new_id: tspam, folder: "archive", expired: true },
		"old spam restored to Archive");

	// --- Old mail whose earlier move to the Inbox was interrupted after
	// the shared copy was made: the copy is removed, the journal entry
	// cleared, and the private message handled as old mail
	function interrupted_out(pid) {	// copy pid to the shared base, journaled
		var safter = fresh().last_msg;
		peek(private_path(ALICE), function (mb) {
			if (mb.copy_msg(num_of(pid), fresh(), { tags: "" }) === false)
				throw new Error("copy_msg back: " + mb.last_error);
		});
		var j = ajf.read("moving", {});
		var h = phdr(num_of(pid));
		j[pid] = { to: "s", msgid: h.id || "", imported: h.when_imported_time,
			written: h.when_written_time, after: safter, tags: "" };
		ajf.write("moving", j);
		var scopy = fresh().last_msg;
		Kit.eq([scopy > safter, shdr(scopy).id], [true, h.id], "shared copy made");
		return scopy;
	}
	function copies(msgid) {
		return count_id(private_path(ALICE), msgid) + count_id(S.shared_path, msgid);
	}
	var IR = to_alice("old, restore interrupted", "<oldir@test>", { when_imported_time: T0 - 200 * DAY });
	var tir = one(st.remove(["s" + IR]), "delete old (restore interrupted)");
	var sir = interrupted_out(tir);
	Kit.assert(ids(st.list("inbox", { limit: 100 })).indexOf("s" + sir) >= 0, "shared copy listed first");
	// The copy cannot be removed: that id fails, nothing changes
	st.open_shared = function () {
		Store.prototype.open_shared.call(this);
		this.smb.remove_msg = function () { return false; };
	};
	try {
		res = st.restore([tir]);
	} finally {
		delete st.open_shared;
	}
	err(res, 0, "failed", "shared copy not removable");
	Kit.assert(shdr(sir) !== null, "shared copy kept");
	Kit.eq(ptags(num_of(tir)).folder, Tags.TRASH, "still in Trash");
	Kit.eq(ajf.read("moving", {})[tir].copy, sir, "journal entry kept");
	res = st.restore([tir]);
	Kit.eq(res, [{ id: tir, ok: true, new_id: tir, folder: "archive", expired: true }],
		"old mail with a shared copy restored to Archive");
	Kit.eq(ptags(num_of(tir)).folder, Tags.ARCHIVE, "restored: in Archive");
	Kit.eq(shdr(sir), null, "restored: shared copy removed");
	Kit.assert(ids(st.list("inbox", { limit: 100 })).indexOf("s" + sir) < 0, "restored: copy not listed");
	Kit.eq(ajf.read("moving", {})[tir], undefined, "restored: journal entry cleared");
	Kit.eq(copies("<oldir@test>"), 1, "restored: one copy");
	var IM = to_alice("old, move interrupted", "<oldim@test>", { when_imported_time: T0 - 200 * DAY });
	var pim = one(st.move(["s" + IM], "archive"), "file old (move interrupted)");
	var sim = interrupted_out(pim);
	res = st.move([pim], "inbox");
	err(res, 0, "expired", "old mail with a shared copy to the Inbox");
	Kit.eq(ptags(num_of(pim)).folder, Tags.ARCHIVE, "moved: still in its folder");
	Kit.eq(shdr(sim), null, "moved: shared copy removed");
	Kit.assert(ids(st.list("inbox", { limit: 100 })).indexOf("s" + sim) < 0, "moved: copy not listed");
	Kit.eq(ajf.read("moving", {})[pim], undefined, "moved: journal entry cleared");
	Kit.eq(copies("<oldim@test>"), 1, "moved: one copy");

	max_age = 0;	// no limit
	Kit.assert(one(st.move([told], "inbox"), "no limit").charAt(0) == "s", "no limit: back in the Inbox");
	max_age = 90;

	// --- Mail held for a QWK network account cannot go back to the
	// Inbox: restore puts it in Archive
	var QWK = to_alice("qwk routed", "<qwk@test>", { to_net_type: D.NET_QWK, to_net_addr: "NODE" });
	var tqwk = one(st.remove(["s" + QWK]), "delete qwk-routed");
	Kit.eq(st.restore([tqwk])[0], { id: tqwk, ok: true, new_id: tqwk, folder: "archive" },
		"qwk-routed restored to Archive");
	Kit.eq(ptags(num_of(tqwk)).folder, Tags.ARCHIVE, "qwk-routed in Archive");

	// --- Tags set by other software on a shared message survive moves
	var TAGGED = to_alice("tagged", "<tagged@test>", { tags: "work urgent" });
	var ptag = one(st.move(["s" + TAGGED], "archive"), "file tagged");
	var tp = ptags(num_of(ptag));
	Kit.eq([tp.folder, tp.other], [Tags.ARCHIVE, ["work", "urgent"]], "foreign tags kept on the way in");
	var stag = one(st.move([ptag], "inbox"), "tagged back to the Inbox");
	Kit.eq(shdr(num_of(stag)).tags, "work urgent", "foreign tags kept on the way out");
	var ttag = one(st.remove([stag]), "delete tagged");
	tp = ptags(num_of(ttag));
	Kit.eq([tp.folder, tp.was, tp.other], [Tags.TRASH, "$Inbox", ["work", "urgent"]], "kept in Trash");

	// --- Sent mail is hidden by Message-ID: removing one recipient's
	// copy hides every copy, and a Sent copy removed for real does not
	// come back as an extra
	var M1 = from_user(ALICE, BOB, "to two people", "<multi@test>");
	var M2 = from_user(ALICE, CAROL, "to two people", "<multi@test>");
	var sent = ids(st.list("sent"));
	Kit.assert((sent.indexOf("s" + M1) >= 0) != (sent.indexOf("s" + M2) >= 0), "one copy listed");
	var shown = sent.indexOf("s" + M1) >= 0 ? "s" + M1 : "s" + M2;
	Kit.eq(one(st.remove([shown]), "hide the extra"), null, "hidden");
	sent = ids(st.list("sent"));
	Kit.assert(sent.indexOf("s" + M1) < 0 && sent.indexOf("s" + M2) < 0, "every copy hidden");
	Kit.assert(ajf.read("hidden", []).indexOf("m:<multi@test>") >= 0, "hidden by Message-ID");
	var PSENT;
	peek(private_path(ALICE), function (mb) {
		PSENT = save(mb, { from_ext: String(ALICE), from: "Alice", to: "Bob", subject: "copy kept",
			tags: Tags.SENT, id: "<kept@test>" });
	});
	var SX = from_user(ALICE, BOB, "copy kept", "<kept@test>");
	sent = ids(st.list("sent"));
	Kit.assert(sent.indexOf("p" + PSENT) >= 0 && sent.indexOf("s" + SX) < 0, "the Sent copy, not the extra");
	one(st.remove(["p" + PSENT]), "Sent copy to Trash");
	ok(st.empty_trash(), "Sent copy removed for real");
	Kit.assert(ids(st.list("sent")).indexOf("s" + SX) < 0, "no extra after the Sent copy is gone");
	// A message without a Message-ID is hidden by number
	var NOID = from_user(ALICE, BOB, "no id", null);
	var nh = fresh().get_msg_header(false, NOID, false);
	nh.id = undefined;
	Kit.assert(shared.put_msg_header(false, NOID, nh), "drop the Message-ID");
	Kit.assert(ids(st.list("sent")).indexOf("s" + NOID) >= 0, "no-id extra listed");
	one(st.remove(["s" + NOID]), "hide the no-id extra");
	Kit.assert(ajf.read("hidden", []).indexOf("s" + NOID) >= 0, "hidden by number");
	Kit.assert(ids(st.list("sent")).indexOf("s" + NOID) < 0, "no-id extra hidden");
	// An entry by number for a message that has a Message-ID (an older
	// hidden list) hides every copy too; entries for records that are
	// gone are dropped when the list is next written
	var L1 = from_user(ALICE, BOB, "legacy", "<legacy@test>");
	var L2 = from_user(ALICE, CAROL, "legacy", "<legacy@test>");
	var hl = ajf.read("hidden", []);
	hl.push("s" + L1, "s999999", "m:<gone@test>");
	ajf.write("hidden", hl);
	sent = ids(st.list("sent"));
	Kit.assert(sent.indexOf("s" + L1) < 0 && sent.indexOf("s" + L2) < 0, "legacy entry hides every copy");
	var H3 = from_user(ALICE, BOB, "another", "<another@test>");
	one(st.remove(["s" + H3]), "hide another");
	hl = ajf.read("hidden", []);
	Kit.assert(hl.indexOf("s999999") < 0 && hl.indexOf("m:<gone@test>") < 0, "stale entries dropped");
	Kit.assert(hl.indexOf("s" + L1) >= 0 && hl.indexOf("m:<multi@test>") >= 0, "live entries kept");
	// Hiding creates no private base
	var DX = from_user(DAVE, BOB, "from dave", "<dave@test>");
	var dst = open_store(DAVE);
	one(dst.remove(["s" + DX]), "Dave hides his extra");
	Kit.assert(!file_exists(private_path(DAVE) + ".shd"), "no private base created by hiding");

	// --- A side file that cannot be saved after the bases were changed:
	// the call still returns its results and does its bookkeeping
	var real_rename = file_rename;
	var real_remove = file_remove;
	var failing = [ajf.path("summary"), ajf.path("shared")];
	var SF = to_alice("save fails", "<savefails@test>");
	file_rename = function (from, to) { return failing.indexOf(to) >= 0 ? false : real_rename(from, to); };
	file_remove = function (p) { return failing.indexOf(p) >= 0 ? false : real_remove(p); };
	try {
		res = st.remove(["s" + SF]);
	} finally {
		file_rename = real_rename;
		file_remove = real_remove;
	}
	var tsf = one(res, "delete with the side files failing");
	Kit.eq(ptags(num_of(tsf)).folder, Tags.TRASH, "in Trash");
	Kit.eq(shdr(SF), null, "gone from the Inbox");
	Kit.eq(ajf.read("trash", {})[num_of(tsf)], clock, "Trash time recorded");
	Kit.assert(ids(st.list("trash", { limit: 100 })).indexOf(tsf) >= 0, "listed in Trash afterwards");
	Kit.assert(ids(st.list("inbox", { limit: 100 })).indexOf("s" + SF) < 0, "not listed in the Inbox");

	// --- A failure notice for a Sent copy whose $Failed cannot be written
	// does not fail the read
	var PF;
	peek(private_path(ALICE), function (mb) {
		PF = save(mb, { from_ext: String(ALICE), from: "Alice", to: "x@example.com", subject: "bounced",
			tags: Tags.SENT, id: "<bounced@test>" });
	});
	var NOTICE = to_alice("delivery failure", "<notice@test>", { attr: D.MSG_NOREPLY,
		reply_id: "<bounced@test>" });
	hooks.put = function () { throw new TypeError("simulated failure"); };
	var sl;
	try {
		sl = st.list("sent", { limit: 100 });
	} finally {
		hooks.put = null;
	}
	var item = null;
	for (var i = 0; i < sl.items.length; i++)
		if (sl.items[i].id == "p" + PF)
			item = sl.items[i];
	Kit.eq(item.delivery, { state: "failed", notice: "s" + NOTICE },
		"failed shown although $Failed was not written");

	// --- Long calls run in slices, one summary lock per slice
	ok(st.empty_trash(), "start with an empty Trash");
	var sst = open_store(ALICE, { trash_days: 30, quota_bytes: 0, slice_size: 2 });
	var five = [];
	for (i = 0; i < 5; i++)
		five.push("s" + to_alice("slice " + i, "<slice" + i + "@test>"));
	ok(sst.remove(five), "five to Trash");
	var summary_locks = 0;
	var real_update = sst.jf.update;
	sst.jf.update = function (name, fallback, fn) {
		if (name == "summary")
			summary_locks++;
		return real_update.call(this, name, fallback, fn);
	};
	calls.purge = 0;
	res = ok(sst.empty_trash(), "empty Trash in slices");
	Kit.eq(res.length, 5, "every Trash message removed");
	Kit.eq(summary_locks, 4, "one summary lock per slice of two, and one for the purge");
	Kit.eq(calls.purge, 1, "one purge for the whole call");
	Kit.eq(trash_count(sst), 0, "Trash empty");
	// A slice that cannot take the lock: the earlier slices stand, the
	// rest are reported
	five = [];
	for (i = 0; i < 5; i++)
		five.push("s" + to_alice("slice b" + i, "<sliceb" + i + "@test>"));
	ok(sst.remove(five), "five more to Trash");
	var runs = 0;
	var lock = sst.jf.lock_path("summary");
	var real_run = sst.run;
	sst.run = function () {
		if (++runs == 2)
			Kit.assert(file_mutex(lock, "held by test", 60), "take the summary lock");
		return real_run.apply(this, arguments);
	};
	sst.jf.timeout = 50;
	try {
		res = sst.empty_trash();
	} finally {
		file_remove(lock);
		sst.jf.timeout = 5000;
		delete sst.run;
	}
	Kit.eq(res.length, 5, "a result for every message");
	Kit.assert(res[0].ok && res[1].ok, "first slice done");
	for (i = 2; i < 5; i++)
		err(res, i, "conflict", "later slice " + i);
	Kit.eq(trash_count(sst), 3, "the rest still in Trash");
	ok(sst.empty_trash(), "empty the rest");
	// folder_delete and folder_rename in slices
	var fid = sst.folder_create("Sliced").id;
	five = [];
	for (i = 0; i < 5; i++)
		five.push("s" + to_alice("filed " + i, "<filed" + i + "@test>"));
	ok(sst.move(five, fid), "five filed");
	summary_locks = 0;
	var rn = sst.folder_rename(fid, "Sliced Two");
	Kit.eq([rn.name, rn.errors.length], ["Sliced Two", 0], "renamed in slices");
	Kit.eq(summary_locks, 3, "rename: one summary lock per slice");
	Kit.eq(sst.list(rn.id).total, 5, "every message renamed");
	summary_locks = 0;
	res = ok(sst.folder_delete(rn.id), "folder delete in slices");
	Kit.eq(res.length, 5, "every message to Trash");
	Kit.eq(summary_locks, 3, "delete: one summary lock per slice");
	Kit.eq(ajf.read("folders", []).indexOf(Tags.encode("Sliced Two")), -1, "folder gone");
	// purge_expired in slices
	calls.purge = 0;
	res = ok(sst.purge_expired(T0 + 60 * DAY), "expire in slices");
	Kit.eq(res.length, 5, "every expired message removed");
	Kit.eq(calls.purge, 1, "expiry: one purge");
	Kit.eq(trash_count(sst), 0, "Trash empty after expiry");
	sst.jf.update = real_update;
	delete sst.jf.update;
	// No private base: purge_expired writes nothing
	var est = open_store(CAROL);
	Kit.eq(est.purge_expired(), [], "nothing to expire");
	Kit.eq(directory(S.user_dir(CAROL) + "mailbox.*").length, 0, "no files for a user without a base");

	// --- Quota: no move is refused for it, and none removes Trash
	function body_of(n) {
		return new Array(n + 1).join("x") + "\r\n";
	}
	var Q1 = to_alice("quota trash 1", "<q1@test>");
	var Q2 = to_alice("quota trash 2", "<q2@test>");
	ok(st.remove(["s" + Q1, "s" + Q2]), "two in Trash");
	var used = st.usage().bytes;
	Kit.eq(st.list("trash").total, 2, "two in Trash");
	var BIG = save(fresh(), { to_ext: String(ALICE), to: "Alice", subject: "big", id: "<big@test>" },
		body_of(20000));
	var MID = save(fresh(), { to_ext: String(ALICE), to: "Alice", subject: "mid", id: "<mid@test>" },
		body_of(4000));
	var SMALL = to_alice("small", "<small@test>");
	var all = st.usage().bytes;	// the three in the Inbox count
	Kit.assert(all - used >= 24000, "usage counts the Inbox: " + used + " to " + all);
	var qst = open_store(ALICE, { quota_bytes: used + 256 });
	Kit.eq(qst.usage(), { bytes: all, quota: used + 256, full: true }, "over the quota with the Inbox");
	one(qst.move(["s" + BIG], "archive"), "a message larger than the room left is filed");
	Kit.eq(trash_count(qst), 2, "Trash kept");
	var quota = 256;
	// The move journal cannot be written: both ids are refused, and Trash
	// is as it was
	qst = open_store(ALICE, { quota_bytes: quota });
	var jlock = qst.jf.lock_path("moving");
	Kit.assert(file_mutex(jlock, "held by test", 60), "take the journal lock");
	qst.jf.timeout = 50;
	try {
		res = qst.move(["s" + SMALL, "s" + MID], "archive");
	} finally {
		file_remove(jlock);
		qst.jf.timeout = 5000;
	}
	err(res, 0, "conflict", "first: journal busy");
	err(res, 1, "conflict", "second: journal busy");
	Kit.eq(trash_count(qst), 2, "Trash kept after a journal refusal");
	Kit.assert(shdr(SMALL) !== null && shdr(MID) !== null, "both still in the Inbox");
	ok(qst.move(["s" + MID, "s" + SMALL], "archive"), "filed over the quota");
	Kit.eq(trash_count(qst), 2, "no Trash removed for a move");
	var after = qst.usage().bytes;
	Kit.assert(after >= all && after - all <= 3 * 256, "the moves changed the total by header blocks only: "
		+ all + " to " + after);
} finally {
	for (var k = 0; k < stores.length; k++)
		stores[k].close();
	if (shared)
		shared.close();
	S.cleanup();
}
