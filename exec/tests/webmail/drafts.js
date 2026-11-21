// Drafts: messages being written, with chunked attachment uploads
// (webmail/lib/drafts.js)

load(js.exec_dir + "kit.js");
var Drafts = Kit.wlib("drafts");

var W = Kit.api_world();
var logged = [];
Drafts.log = function (level, text) {
	logged.push(text);
};

var DAY = 86400;
var NOW = time();
var DOMAINS = ["vert.example", "family.example"];
var make = W.Api.context_factory({ config: W.cfg,
	get_user: function (n) {
		return W.USERS.hasOwnProperty(n) ? W.USERS[n] : null;
	},
	userdata: function (n) {
		return new W.UserData(W.S.user_dir(n) + "webmail/", W.JsonFiles);
	},
	Store: W.Store, shared_path: W.S.shared_path, data_dir: W.S.data_dir });
var contexts = [];
W.add_user(60005, { alias: "carl" });
W.add_user(60006, { alias: "rita" });
W.add_user(60007, { alias: "sam" });
W.add_user(60008, { alias: "uma" });

// A context for usernum with config overrides o
function context(usernum, o)
{
	var ctx = make(usernum);
	contexts.push(ctx);
	var cfg = {};
	for (var k in W.cfg)
		cfg[k] = W.cfg[k];
	for (k in (o || {}))
		cfg[k] = o[k];
	ctx.config = cfg;
	ctx.domains = DOMAINS;
	ctx.clock = NOW;
	ctx.now = function () {
		return ctx.clock;
	};
	return ctx;
}

function read_file(path)
{
	var f = new File(path);
	Kit.assert(f.open("rb"), "open " + path);
	var t = f.read();
	f.close();
	return t;
}

function bytes(n, seed)
{
	var a = [];
	for (var i = 0; i < n; i++)
		a.push(String.fromCharCode((i * 7 + (seed || 0)) & 0xFF));
	return a.join("");
}

function names(dir)
{
	var list = directory(dir + "*");
	var out = [];
	for (var i = 0; i < list.length; i++)
		out.push(list[i].substr(dir.length));
	out.sort();
	return out;
}

// Every path under dir (recursively), relative to it
function tree(dir)
{
	var out = [];
	var list = directory(dir + "*");
	for (var i = 0; i < list.length; i++) {
		var p = list[i];
		var c = p.charAt(p.length - 1);
		out.push(p.substr(dir.length));
		if (c == "/" || c == "\\")
			out = out.concat(tree(p).map(function (s) {
				return p.substr(dir.length) + s;
			}));
	}
	out.sort();
	return out;
}

function chunk(upload, name, offset, total, data, type)
{
	return { upload: upload, name: name, type: type === undefined ? "application/pdf" : type, offset: offset, total: total,
		b64: data === "" ? "" : base64_encode(data) };
}

try {
	var A = context(W.ALICE, { max_attachment_bytes: 3000, max_draft_bytes: 100000 });
	var ROOT = A.userdata.dir + "drafts/";
	var before = tree(W.S.dir);

	// ---- ids are checked before any file is touched ----
	["../../x", "0123456789abcdef0", "0123456789ABCDEF", "0123456789abcde", "", null, 5, "0123456789abcde/"]
		.forEach(function (bad) {
			Kit.throws(function () { Drafts.get(A, bad); }, "invalid");
			Kit.throws(function () { Drafts.save(A, bad, { rev: 1 }); }, "invalid");
			Kit.throws(function () { Drafts.remove(A, bad); }, "invalid");
			Kit.throws(function () { Drafts.attach(A, bad, chunk("upload01", "x", 0, 1, "x")); }, "invalid");
			Kit.throws(function () { Drafts.detach(A, bad, "a1"); }, "invalid");
		});
	Kit.eq(tree(W.S.dir), before, "invalid ids touch nothing");
	Kit.throws(function () { Drafts.get(A, "0123456789abcdef"); }, "notfound");

	// ---- create, get, save ----
	var d = Drafts.create(A, { mode: "new" });
	Kit.assert(/^[0-9a-f]{16}$/.test(d.id), "draft id: " + d.id);
	Kit.eq([d.rev, d.mode, d.to, d.cc, d.bcc, d.subject, d.body, d.from_domain, d.priority, d.in_reply_to,
		d.attachments], [1, "new", "", "", "", "", "", "", "", "", []], "new draft fields");
	Kit.eq(names(ROOT + d.id + "/"), ["draft.json"], "draft directory");
	Kit.eq(Drafts.get(A, d.id), d, "get");
	Kit.throws(function () { Drafts.create(A, { mode: "draft" }); }, "invalid");
	Kit.throws(function () { Drafts.create(A, { mode: "new", source_id: "s1" }); }, "invalid");
	Kit.throws(function () { Drafts.create(A, { mode: "reply" }); }, "invalid");

	A.clock = NOW + 5;
	var s = Drafts.save(A, d.id, { rev: 1, to: "bob@x.org, Carol <c@y.org>", cc: "dave@z.org",
		bcc: "eve@w.org", subject: "Hi\r\nBcc: evil@example.org", body: "line 1\r\nline 2\ttab\r\n",
		from_domain: "Family.Example", priority: "high" });
	Kit.eq(s.rev, 2, "rev after save");
	Kit.eq(s.subject, "HiBcc: evil@example.org", "subject cleaned");
	Kit.eq(s.from_domain, "family.example", "from_domain lowercased");
	Kit.eq(s.updated, NOW + 5, "updated");
	Kit.eq(Drafts.get(A, d.id), s, "save/get round trip");
	// a partial save keeps the other fields
	s = Drafts.save(A, d.id, { rev: 2, body: "caf\xc3\xa9" });
	Kit.eq([s.rev, s.body, s.to, s.priority], [3, "caf\xc3\xa9", "bob@x.org, Carol <c@y.org>", "high"], "partial");
	// a stale revision: conflict with the current one, nothing written
	var stale = null;
	try {
		Drafts.save(A, d.id, { rev: 2, body: "other tab" });
	} catch (e) {
		stale = e;
	}
	Kit.eq(stale, { code: "conflict", message: stale.message, reason: "stale", rev: 3 }, "stale rev");
	Kit.eq(Drafts.get(A, d.id).body, "caf\xc3\xa9", "stale save wrote nothing");
	// field validation
	var bad_saves = [
		{ rev: 3, sender: "x" }, { body: "no rev" }, { rev: "3" }, { rev: 3, priority: "urgent" },
		{ rev: 3, from_domain: "evil.example" }, { rev: 3, to: "a@b.c\r\nBcc: x@y.z" },
		{ rev: 3, to: 5 }, { rev: 3, body: "bad \xc3\x28 utf-8" }, { rev: 3, body: "nul\x00" },
		{ rev: 3, subject: "\xff" }, { rev: 3, body: String.fromCharCode(0x100) },
		{ rev: 3, body: "overlong \xc0\xaf" }, { rev: 3, body: "surrogate \xed\xa0\x80" },
		{ rev: 3, body: "truncated \xe2\x82" }];
	bad_saves.forEach(function (f, i) {
		Kit.throws(function () { Drafts.save(A, d.id, f); }, "invalid");
	});
	Kit.throws(function () { Drafts.save(A, d.id, null); }, "invalid");
	var big_cfg = context(W.ALICE, { max_body_bytes: 10 });
	Kit.throws(function () { Drafts.save(big_cfg, d.id, { rev: 3, body: "12345678901" }); }, "toolarge");
	Kit.eq(Drafts.save(big_cfg, d.id, { rev: 3, body: "1234567890", from_domain: "" }).rev, 4, "at the limit");
	Kit.throws(function () {
		Drafts.save(A, d.id, { rev: 4, to: new Array(Drafts.MAX_ADDR_FIELD + 2).join("a") });
	}, "toolarge");
	Kit.eq(Drafts.get(A, d.id).rev, 4, "failed saves wrote nothing");

	// list, count, remove
	A.clock = NOW + 10;
	var d2 = Drafts.create(A, {});
	var l = Drafts.list(A);
	Kit.eq(l.map(function (x) { return x.id; }), [d2.id, d.id], "list newest first");
	Kit.eq(l[1].subject, "HiBcc: evil@example.org", "list subject");
	Kit.eq(Drafts.count(A), 2, "count");
	Kit.eq(Drafts.remove(A, d2.id), true, "remove");
	Kit.assert(!file_isdir(ROOT + d2.id), "draft directory removed");
	Kit.throws(function () { Drafts.get(A, d2.id); }, "notfound");
	Kit.throws(function () { Drafts.remove(A, d2.id); }, "notfound");

	// ---- attachments ----
	var dir = ROOT + d.id + "/";
	var data = bytes(1000, 1);
	// a hostile name: the display name is its base name; the file name is
	// the server's
	var r = Drafts.attach(A, d.id, chunk("upload01", "../../x", 0, 1000, data.substr(0, 400)));
	Kit.eq(r, { id: "a1", received: 400, complete: false }, "first chunk");
	Kit.eq(names(dir), ["a1", "draft.json"], "only server-chosen names in the draft directory");
	Kit.eq(Drafts.get(A, d.id).attachments, [{ id: "a1", name: "x", type: "application/pdf", size: 400,
		total: 1000, complete: false }], "attachment metadata");
	// wrong offset: conflict with the bytes received, file unchanged
	var e1 = null;
	try {
		Drafts.attach(A, d.id, chunk("upload01", "", 500, 1000, data.substr(500, 100)));
	} catch (e) {
		e1 = e;
	}
	Kit.eq([e1.code, e1.reason, e1.received], ["conflict", "offset", 400], "wrong offset");
	Kit.eq(read_file(dir + "a1"), data.substr(0, 400), "file unchanged after wrong offset");
	// the last chunk again (its response was lost): same received, no growth
	r = Drafts.attach(A, d.id, chunk("upload01", "../../x", 0, 1000, data.substr(0, 400)));
	Kit.eq(r, { id: "a1", received: 400, complete: false }, "repeated last chunk");
	Kit.eq(file_size(dir + "a1"), 400, "repeat not appended");
	// same offset and length, different bytes: not the retry
	Kit.throws(function () {
		Drafts.attach(A, d.id, chunk("upload01", "", 0, 1000, bytes(400, 9)));
	}, "conflict");
	// a different total on a later chunk
	Kit.throws(function () { Drafts.attach(A, d.id, chunk("upload01", "", 400, 999, data.substr(400, 100))); },
		"invalid");
	// a chunk past the declared total
	Kit.throws(function () { Drafts.attach(A, d.id, chunk("upload01", "", 400, 1000, bytes(700, 3))); },
		"invalid");
	Kit.eq(file_size(dir + "a1"), 400, "nothing appended by refused chunks");
	r = Drafts.attach(A, d.id, chunk("upload01", "", 400, 1000, data.substr(400, 300)));
	Kit.eq(r.received, 700, "second chunk");
	// a crash after an append, before draft.json recorded it: the file is
	// longer than received; the retried chunk is not appended twice
	var f = new File(dir + "a1");
	f.open("ab");
	f.write(data.substr(700, 100));
	f.close();
	r = Drafts.attach(A, d.id, chunk("upload01", "", 700, 1000, data.substr(700, 300)));
	Kit.eq(r, { id: "a1", received: 1000, complete: true }, "complete");
	Kit.eq(md5_calc(read_file(dir + "a1"), true), md5_calc(data, true), "file contents");
	// the final chunk again: acknowledged; anything else for it: conflict
	Kit.eq(Drafts.attach(A, d.id, chunk("upload01", "", 700, 1000, data.substr(700, 300))).received, 1000,
		"final chunk repeated");
	Kit.throws(function () { Drafts.attach(A, d.id, chunk("upload01", "", 1000, 1000, "z")); }, "conflict");

	// a retried FIRST chunk (no attachment id known to the client): the
	// upload key finds it
	var d3 = Drafts.create(A, {});
	var dir3 = ROOT + d3.id + "/";
	r = Drafts.attach(A, d3.id, chunk("first-retry", "a.txt", 0, 10, "0123456789", "text/plain"));
	Kit.eq(r, { id: "a1", received: 10, complete: true }, "small file in one chunk");
	r = Drafts.attach(A, d3.id, chunk("first-retry", "a.txt", 0, 10, "0123456789", "text/plain"));
	Kit.eq(r, { id: "a1", received: 10, complete: true }, "retried first chunk");
	// the same name again (another file): allowed, a new attachment
	r = Drafts.attach(A, d3.id, chunk("second-file", "a.txt", 0, 10, "0123456789", "text/plain"));
	Kit.eq(r.id, "a2", "duplicate names allowed");
	// an empty file
	r = Drafts.attach(A, d3.id, chunk("empty-file", "empty", 0, 0, ""));
	Kit.eq(r, { id: "a3", received: 0, complete: true }, "empty file");
	Kit.eq(file_size(dir3 + "a3"), 0, "empty file on disk");
	Kit.throws(function () { Drafts.attach(A, d3.id, chunk("empty-two", "e", 0, 5, "")); }, "invalid");
	// types and names
	var types = [["text/plain; charset=utf-8", "text/plain"], ["IMAGE/PNG", "image/png"],
		["text/html\r\nX: y", "application/octet-stream"], ["", "application/octet-stream"],
		["nonsense", "application/octet-stream"], [5, "application/octet-stream"],
		["a/b/c", "application/octet-stream"]];
	types.forEach(function (t, i) {
		var c = chunk("type-test-" + i, "C:\\temp\\file\r\n" + i + ".bin", 0, 1, "t", t[0]);
		var id = Drafts.attach(A, d3.id, c).id;
		var att = Drafts.get(A, d3.id).attachments.filter(function (a) { return a.id === id; })[0];
		Kit.eq(att.type, t[1], "type " + JSON.stringify(t[0]));
		Kit.eq(att.name, "file" + i + ".bin", "name base and cleaned");
		Drafts.detach(A, d3.id, id);
	});
	["..", ".", "", "a/", "dir\\..", "\r\n", null, "\xc3\x28"].forEach(function (n) {
		Kit.throws(function () { Drafts.attach(A, d3.id, chunk("bad-name-1", n, 0, 1, "x")); }, "invalid");
	});
	// base64 must be canonical
	["QR==", "QQ", "Q===", "QQ=a", "QQ= =", "QUJD\r\n", "!!!!", "QUJ=D", "QUI", " QUJD", "QUJD====", "=QUJ"]
		.forEach(function (b) {
			Kit.throws(function () {
				Drafts.attach(A, d3.id, { upload: "bad-b64-1", name: "n", type: "", offset: 0, total: 3, b64: b });
			}, "invalid");
		});
	// other bad chunks
	[{ upload: "short", name: "n", offset: 0, total: 1, b64: "eA==" },
		{ upload: "has a space", name: "n", offset: 0, total: 1, b64: "eA==" },
		{ upload: "good-key-1", name: "n", offset: -1, total: 1, b64: "eA==" },
		{ upload: "good-key-1", name: "n", offset: 0, total: 1.5, b64: "eA==" },
		{ upload: "good-key-1", name: "n", offset: 0, total: "1", b64: "eA==" },
		{ upload: "good-key-1", name: "n", offset: 0, total: 1, b64: 5 }, null]
		.forEach(function (c, i) {
			Kit.throws(function () { Drafts.attach(A, d3.id, c); }, "invalid");
		});
	// a new file must start at offset 0
	Kit.throws(function () { Drafts.attach(A, d3.id, chunk("late-start", "n", 5, 10, "x")); }, "conflict");
	// a chunk over 1500000 decoded bytes
	var huge = context(W.ALICE, { max_attachment_bytes: 5000000, max_draft_bytes: 50000000 });
	Kit.throws(function () {
		Drafts.attach(huge, d3.id, chunk("huge-chunk", "h", 0, 2000000, bytes(1500001)));
	}, "invalid");
	r = Drafts.attach(huge, d3.id, chunk("huge-chunk", "h", 0, 2000000, bytes(1500000)));
	Kit.eq(r.received, 1500000, "a chunk of exactly 1500000 bytes");
	Drafts.detach(huge, d3.id, r.id);
	Kit.eq(Drafts.get(A, d3.id).attachments.map(function (a) { return a.id; }), ["a1", "a2", "a3"],
		"detach removed the record");
	Kit.assert(!file_exists(dir3 + r.id), "detach removed the file");
	Kit.throws(function () { Drafts.detach(A, d3.id, "a99"); }, "notfound");
	Kit.throws(function () { Drafts.detach(A, d3.id, "../draft.json"); }, "invalid");
	Kit.throws(function () { Drafts.detach(A, d3.id, "draft.json"); }, "invalid");

	// ---- limits ----
	// max_attachment_bytes (3000): the draft d holds 1000; a declared
	// total over the rest is toolarge on the first chunk
	Kit.throws(function () { Drafts.attach(A, d.id, chunk("too-large-1", "t", 0, 2001, "x")); }, "toolarge");
	Kit.eq(Drafts.get(A, d.id).attachments.length, 1, "nothing recorded");
	Kit.eq(names(dir), ["a1", "draft.json"], "nothing written");
	// an understated total: caught when the actual bytes cross the limit
	Kit.throws(function () { Drafts.attach(A, d.id, chunk("too-large-2", "t", 0, 10, bytes(2500))); },
		"toolarge");
	// two uploads that each fit but not together: the bytes on disk count
	r = Drafts.attach(A, d.id, chunk("fits-one", "t", 0, 1500, bytes(100)));
	Kit.throws(function () { Drafts.attach(A, d.id, chunk("fits-two", "t", 0, 1500, bytes(100))); }, "toolarge");
	Drafts.detach(A, d.id, r.id);
	// max_draft_bytes: the user's draft files, on disk
	var Q = context(W.ALICE, { max_attachment_bytes: 100000, max_draft_bytes: 0 });
	var used = Drafts.usage(Q).bytes;
	Kit.assert(used > 1000, "usage counts files on disk: " + used);
	Q.config.max_draft_bytes = used + 500;
	Kit.throws(function () { Drafts.attach(Q, d3.id, chunk("quota-one", "q", 0, 600, bytes(10))); }, "quota");
	r = Drafts.attach(Q, d3.id, chunk("quota-two", "q", 0, 400, bytes(200)));
	Q.config.max_draft_bytes = Drafts.usage(Q).bytes + 100;
	Kit.throws(function () { Drafts.attach(Q, d3.id, chunk("quota-two", "q", 200, 400, bytes(200))); }, "quota");
	Kit.eq(Drafts.get(Q, d3.id).attachments.filter(function (a) { return a.id === r.id; })[0].size, 200,
		"quota refusal appended nothing");
	Drafts.detach(Q, d3.id, r.id);
	// save counts its text against max_draft_bytes too
	Q.config.max_draft_bytes = Drafts.usage(Q).bytes + 50;
	var qd = Drafts.get(Q, d3.id);
	var qe = null;
	try {
		Drafts.save(Q, d3.id, { rev: qd.rev, body: new Array(201).join("b") });
	} catch (e) {
		qe = e;
	}
	Kit.eq([qe.code, qe.reason], ["quota", "bytes"], "save over max_draft_bytes");
	Kit.eq(Drafts.get(Q, d3.id), qd, "a refused save changes nothing");
	Kit.eq(Drafts.save(Q, d3.id, { rev: qd.rev, body: "short" }).body, "short", "a save that fits");

	// ---- sending: claim, release, finish ----
	r = Drafts.attach(A, d3.id, chunk("incomplete", "i", 0, 10, "12345"));
	var ce = null;
	try {
		Drafts.claim(A, d3.id);
	} catch (e) {
		ce = e;
	}
	Kit.eq([ce.code, ce.reason], ["conflict", "incomplete"], "claim refuses an incomplete upload");
	Drafts.detach(A, d3.id, r.id);
	var snapshot = {};
	names(dir3).forEach(function (n) {
		snapshot[n] = read_file(dir3 + n);
	});
	var before_claim = Drafts.get(A, d3.id);
	var cl = Drafts.claim(A, d3.id);
	var sdir3 = ROOT + d3.id + ".sending/";
	Kit.eq(cl.dir, sdir3, "claimed directory");
	Kit.eq(cl.files.map(function (x) { return [x.id, x.name, x.size, x.path === sdir3 + x.id]; }),
		[["a1", "a.txt", 10, true], ["a2", "a.txt", 10, true], ["a3", "empty", 0, true]], "claim files");
	Kit.eq(read_file(cl.files[0].path), "0123456789", "a claimed file is readable");
	Kit.assert(!file_isdir(dir3), "the draft directory is gone while claimed");
	["get", "remove", "claim", "release_none"].forEach(function (m) {
		if (m === "release_none")
			return;
		Kit.throws(function () { Drafts[m](A, d3.id); }, "notfound");
	});
	Kit.throws(function () { Drafts.save(A, d3.id, { rev: before_claim.rev, body: "x" }); }, "notfound");
	Kit.throws(function () { Drafts.attach(A, d3.id, chunk("while-sent", "w", 0, 1, "w")); }, "notfound");
	Kit.throws(function () { Drafts.detach(A, d3.id, "a1"); }, "notfound");
	Kit.assert(Drafts.list(A).every(function (x) { return x.id !== d3.id; }), "not listed while claimed");
	Kit.eq(Drafts.release(A, d3.id), before_claim, "release returns the draft");
	Kit.eq(Drafts.get(A, d3.id), before_claim, "released draft as before");
	var after = {};
	names(dir3).forEach(function (n) {
		after[n] = read_file(dir3 + n);
	});
	Kit.eq(after, snapshot, "release restores every file byte for byte");
	Kit.assert(!file_isdir(sdir3), "no claimed directory after release");
	Kit.throws(function () { Drafts.release(A, d3.id); }, "notfound");
	Kit.throws(function () { Drafts.finish(A, d3.id); }, "notfound");
	// finish removes it
	var d4 = Drafts.create(A, {});
	Drafts.claim(A, d4.id);
	Kit.eq(Drafts.finish(A, d4.id), true, "finish");
	Kit.assert(!file_isdir(ROOT + d4.id + ".sending") && !file_isdir(ROOT + d4.id), "finish removed it");
	Kit.throws(function () { Drafts.get(A, d4.id); }, "notfound");

	// limits for the client
	Kit.eq(Drafts.limits(A), { max_drafts: 50, max_draft_bytes: 100000, max_attachment_bytes: 3000,
		max_body_bytes: 1048576, max_attachments: 100, max_chunk: 1500000 }, "limits");

	// ---- a draft.json without the shape of a draft is unreadable ----
	var shapes = [
		["attachments null", function (j) { j.attachments = [null]; }],
		["string rev", function (j) { j.rev = "1"; }],
		["attachment id path", function (j) { j.attachments[0].id = "../../secret"; }],
		["negative received", function (j) { j.attachments[0].received = -1; }],
		["received over total", function (j) { j.attachments[0].received = j.attachments[0].total + 1; }],
		["body not text", function (j) { j.body = 5; }],
		["next negative", function (j) { j.next = -1; }],
		["not an object", function (j) { return [1]; }]];
	var bad_ids = [];
	shapes.forEach(function (sh) {
		var bd = Drafts.create(A, {});
		Drafts.attach(A, bd.id, chunk("shape-upload", "s", 0, 2, "ab"));
		var bdir = ROOT + bd.id + "/";
		var j = JSON.parse(read_file(bdir + "draft.json"));
		var repl = sh[1](j);
		var x = new File(bdir + "draft.json");
		x.open("wb");
		x.write(JSON.stringify(repl === undefined ? j : repl));
		x.close();
		var ge = null;
		try {
			Drafts.get(A, bd.id);
		} catch (e) {
			ge = e;
		}
		Kit.eq([ge && ge.code, ge && ge.reason], ["notfound", "unreadable"], sh[0] + ": get");
		Kit.throws(function () { Drafts.save(A, bd.id, { rev: 1, body: "x" }); }, "notfound");
		Kit.throws(function () { Drafts.attach(A, bd.id, chunk("shape-upload", "s", 2, 2, "")); });
		Kit.throws(function () { Drafts.claim(A, bd.id); }, "notfound");
		Kit.assert(Drafts.list(A).every(function (y) { return y.id !== bd.id; }), sh[0] + ": not listed");
		bad_ids.push(bd.id);
	});
	Kit.assert(!file_exists(A.userdata.dir + "secret"), "no path from a record");
	var sw = Drafts.sweep(A, time() + 2 * DAY);	// every bad draft is old by then
	bad_ids.forEach(function (id) {
		Kit.assert(!file_isdir(ROOT + id), "sweep removed the unreadable draft " + id);
	});
	Kit.assert(sw.drafts >= bad_ids.length, "sweep counted them: " + JSON.stringify(sw));
	Kit.assert(file_isdir(dir), "a good draft with text survives that sweep");

	// ---- a claim older than an hour goes back to the drafts ----
	var R = context(60006);
	var RROOT = R.userdata.dir + "drafts/";
	R.clock = NOW;
	var rd = Drafts.create(R, {});
	Drafts.save(R, rd.id, { rev: 1, body: "do not lose me" });
	Drafts.claim(R, rd.id);
	Kit.eq(Drafts.sweep(R, NOW + 3000), { drafts: 0, files: 0, recovered: 0, stale_sends: [] },
		"a recent claim is left");
	Kit.assert(file_isdir(RROOT + rd.id + ".sending/"), "still claimed");
	Kit.eq(Drafts.sweep(R, NOW + 3700), { drafts: 0, files: 0, recovered: 1, stale_sends: [] },
		"an old claim is recovered");
	Kit.eq(Drafts.get(R, rd.id).body, "do not lose me", "recovered draft text");
	// a claim whose id is a draft directory again comes back under a new id
	Drafts.claim(R, rd.id);
	mkpath(RROOT + rd.id + "/");
	Kit.eq(Drafts.sweep(R, NOW + 3700).recovered, 1, "recovered under a new id");
	Kit.assert(!file_isdir(RROOT + rd.id + ".sending/"), "no claim stranded");
	var moved = Drafts.list(R).filter(function (x) { return x.id !== rd.id; });
	Kit.eq(moved.length, 1, "one draft");
	Kit.eq(Drafts.get(R, moved[0].id).body, "do not lose me", "its text kept");
	rmdir(RROOT + rd.id + "/");

	// ---- the send step's record lives with the claim ----
	var S6 = context(60007);
	var sroot = S6.userdata.dir + "drafts/";
	S6.clock = NOW;
	var sd = Drafts.create(S6, {});
	Drafts.save(S6, sd.id, { rev: 1, body: "record" });
	Kit.eq(Drafts.claimed(S6, sd.id), null, "not claimed");
	var scl = Drafts.claim(S6, sd.id, { send: { phase: "claimed", t: NOW } });
	var st = Drafts.claimed(S6, sd.id);
	Kit.eq([st.dir, st.send, st.claimed, st.draft.body], [scl.dir, { phase: "claimed", t: NOW }, NOW, "record"],
		"claimed() reads the claim and its record");
	Kit.eq(Drafts.set_send(S6, sd.id, { phase: "copy", t: NOW }), true, "set_send");
	Kit.eq(Drafts.claimed(S6, sd.id).send.phase, "copy", "record replaced");
	var tf = new File(scl.dir + "send.body");
	tf.open("wb");
	tf.write("built");
	tf.close();
	// sweep leaves a claim whose send got past "claimed" to the send step
	var called = [];
	var sw6 = Drafts.sweep(S6, NOW + 3700, { recover: function (id) {
		called.push(id);
		throw { code: "conflict", reason: "unknown" };	// logged, not thrown
	} });
	Kit.eq([sw6.recovered, sw6.stale_sends, called], [0, [sd.id], [sd.id]], "left to the send step");
	Kit.eq(logged.pop(), "drafts.sweep: recovery of " + sd.id + ": conflict (unknown)", "its failure logged");
	Kit.assert(file_isdir(sroot + sd.id + ".sending/"), "still claimed");
	// release: the send's files go; maybe_sent marks the draft
	var rel = Drafts.release(S6, sd.id, { maybe_sent: NOW + 5 });
	Kit.eq([rel.id, rel.maybe_sent, rel.body], [sd.id, NOW + 5, "record"], "released, marked maybe_sent");
	Kit.eq(names(sroot + sd.id + "/").filter(function (n) { return /^send/.test(n); }), [], "send files removed");
	var me = null;
	try {
		Drafts.claim(S6, sd.id);
	} catch (e) {
		me = e;
	}
	Kit.eq([me && me.code, me && me.reason], ["conflict", "maybe_sent"], "a maybe_sent draft needs confirmation");
	Drafts.claim(S6, sd.id, { confirm_resend: true });
	Kit.eq(Drafts.claimed(S6, sd.id).send, null, "claimed with no record");
	Drafts.release(S6, sd.id);
	// ownership: take() by compare-and-set; a stale owner is refused
	Drafts.claim(S6, sd.id, { confirm_resend: true, send: { sid: "s1", phase: "claimed", owner: "tok-a", beat: NOW } });
	var seen = Drafts.claimed(S6, sd.id).send;
	var tk = null;
	try {
		Drafts.take(S6, sd.id, seen, "tok-b", NOW + 10, 600);
	} catch (e) {
		tk = e;
	}
	Kit.eq([tk && tk.code, tk && tk.reason], ["conflict", "sending"], "no takeover inside the window");
	var taken = Drafts.take(S6, sd.id, seen, "tok-b", NOW + 601, 600);
	Kit.eq([taken.owner, taken.beat], ["tok-b", NOW + 601], "taken over after the window");
	tk = null;
	try {
		Drafts.take(S6, sd.id, seen, "tok-c", NOW + 5000, 600);	// seen is stale now
	} catch (e) {
		tk = e;
	}
	Kit.eq([tk && tk.code, tk && tk.reason], ["conflict", "taken"], "take needs the state last seen");
	function refused(fn, why) {
		var x = null;
		try {
			fn();
		} catch (e) {
			x = e;
		}
		Kit.eq([x && x.code, x && x.reason], ["conflict", "taken"], why);
	}
	refused(function () { Drafts.set_send(S6, sd.id, { sid: "s1", phase: "copy", owner: "tok-a" }, "tok-a"); },
		"a stale owner's set_send");
	refused(function () { Drafts.release(S6, sd.id, { token: "tok-a" }); }, "a stale owner's release");
	refused(function () { Drafts.finish(S6, sd.id, "tok-a"); }, "a stale owner's finish");
	refused(function () { Drafts.release(S6, sd.id); }, "a release without the token");
	Kit.eq(Drafts.claimed(S6, sd.id).send.owner, "tok-b", "the record is the new owner's");
	Kit.eq(Drafts.set_send(S6, sd.id, { sid: "s1", phase: "charging", owner: "tok-b", beat: NOW }, "tok-b"),
		true, "the owner writes");
	refused(function () { Drafts.set_send(S6, sd.id, { sid: "s1", phase: "delivered", owner: "tok-b" }, "tok-b"); },
		"a phase never moves backwards");
	Kit.eq(Drafts.claimed(S6, sd.id).send.phase, "charging", "still charging");
	Kit.eq(Drafts.release(S6, sd.id, { token: "tok-b" }).id, sd.id, "the owner releases");
	Drafts.claim(S6, sd.id, { confirm_resend: true, send: { sid: "s2", phase: "claimed", owner: "tok-d", beat: NOW } });
	Kit.eq(Drafts.finish(S6, sd.id, "tok-d"), true, "the owner finishes");
	sd = Drafts.create(S6, {});
	Drafts.save(S6, sd.id, { rev: 1, body: "record" });
	Drafts.claim(S6, sd.id, { confirm_resend: true });
	// release while the id is a draft directory again: a new id
	mkpath(sroot + sd.id + "/");
	var rel2 = Drafts.release(S6, sd.id);
	Kit.assert(rel2.id !== sd.id && /^[0-9a-f]{16}$/.test(rel2.id), "a new id: " + rel2.id);
	Kit.eq(Drafts.get(S6, rel2.id).body, "record", "the text kept");
	Kit.assert(!file_isdir(sroot + sd.id + ".sending/"), "no claim stranded");
	rmdir(sroot + sd.id + "/");

	// ---- release(): the mark is written before the rename, the send's
	// files are removed after it; a failure or a crash between any two
	// steps leaves the claim with its record, or the draft back marked ----
	var U = context(60008);
	var uroot = U.userdata.dir + "drafts/";
	U.clock = NOW;
	var DELIVERING = { sid: "u1", phase: "delivering", owner: "tok-u", beat: NOW };
	function claimed_with_body(body) {
		var x = Drafts.create(U, {});
		Drafts.save(U, x.id, { rev: 1, body: body });
		var c = Drafts.claim(U, x.id, { send: DELIVERING });
		var bf = new File(c.dir + "send.body");
		bf.open("wb");
		bf.write("built");
		bf.close();
		return x.id;
	}
	function draft_json(dir) {
		return JSON.parse(read_file(dir + "draft.json"));
	}
	function send_names(dir) {
		return names(dir).filter(function (n) { return /^send\./.test(n); });
	}
	function code_of(fn) {
		try {
			fn();
		} catch (e) {
			return e && e.code;
		}
		return null;
	}
	// (1) the rename fails (a file stands where the draft directory goes)
	var u1 = claimed_with_body("rename fails");
	var block = new File(uroot + u1);
	block.open("wb");
	block.close();
	Kit.eq(code_of(function () { Drafts.release(U, u1, { token: "tok-u", maybe_sent: NOW + 1 }); }), "failed",
		"a failed rename: failed");
	Kit.assert(file_isdir(uroot + u1 + ".sending/"), "rename failed: still claimed");
	Kit.eq(Drafts.claimed(U, u1).send.phase, "delivering", "rename failed: the record kept");
	Kit.eq(send_names(uroot + u1 + ".sending/"), ["send.body", "send.json"], "rename failed: the send's files kept");
	var dj = draft_json(uroot + u1 + ".sending/");
	Kit.eq([dj.maybe_sent, dj.claimed], [NOW + 1, undefined], "rename failed: the draft already marked");
	file_remove(uroot + u1);
	var back = Drafts.release(U, u1, { token: "tok-u", maybe_sent: NOW + 2 });
	Kit.eq([back.id, back.maybe_sent, back.body], [u1, NOW + 2, "rename fails"], "released on a second try, marked");
	Kit.eq(send_names(uroot + u1 + "/"), [], "then the send's files removed");
	// (2) a crash after the mark, before the rename
	var u2 = claimed_with_body("crash before rename");
	Drafts.hook = function (point) {
		if (point === "unclaim:written")
			throw new Error("simulated crash");
	};
	try {
		Kit.eq(code_of(function () { Drafts.release(U, u2, { token: "tok-u", maybe_sent: NOW + 3 }); }), "failed",
			"crash before the rename");
	} finally {
		Drafts.hook = null;
	}
	Kit.eq(Drafts.claimed(U, u2).send.phase, "delivering", "crash before the rename: the claim and record kept");
	Kit.eq(draft_json(uroot + u2 + ".sending/").maybe_sent, NOW + 3, "crash before the rename: already marked");
	// a sweep leaves it to the send step (it has a record), at the live window.
	// The mark rewrote draft.json without its claim time, so the sweep goes by
	// the claim's file dates (the real clock), not U's: measure from now.
	var called = [];
	var T2 = time();
	var swu = Drafts.sweep(U, T2 + 700, { live: 600, recover: function (id) {
		called.push(id);
	} });
	Kit.eq([swu.recovered, swu.stale_sends, called], [0, [u2], [u2]], "crash before the rename: left to recovery");
	Kit.eq(Drafts.release(U, u2, { token: "tok-u", maybe_sent: NOW + 4 }).maybe_sent, NOW + 4, "then released, marked");
	// (3) a crash after the rename, before the send's files are removed
	var u3 = claimed_with_body("crash after rename");
	Drafts.hook = function (point) {
		if (point === "unclaim:renamed")
			throw new Error("simulated crash");
	};
	try {
		code_of(function () { Drafts.release(U, u3, { token: "tok-u", maybe_sent: NOW + 5 }); });
	} finally {
		Drafts.hook = null;
	}
	Kit.assert(!file_isdir(uroot + u3 + ".sending/"), "crash after the rename: not claimed");
	var g3 = Drafts.get(U, u3);
	Kit.eq([g3.maybe_sent, g3.body], [NOW + 5, "crash after rename"], "crash after the rename: the draft back, marked");
	Kit.eq(send_names(uroot + u3 + "/"), ["send.body", "send.json"], "crash after the rename: the files left");
	Kit.eq(code_of(function () { Drafts.claim(U, u3); }), "conflict", "still needs confirmation to send");
	var c3 = Drafts.claim(U, u3, { confirm_resend: true, send: { sid: "u3", phase: "claimed", owner: "t3", beat: NOW } });
	Kit.eq(send_names(c3.dir), ["send.json"], "a new claim removes the old send's files");
	Kit.eq(Drafts.claimed(U, u3).send.sid, "u3", "with its own record");
	Drafts.release(U, u3, { token: "t3" });
	// (4) a claim with no readable record but the send's files: returned
	// marked maybe_sent after an hour (nothing is known of the send)
	var u4 = claimed_with_body("no record");
	var rj = new File(uroot + u4 + ".sending/send.json");
	rj.open("wb");
	rj.write("{ not json");
	rj.close();
	Kit.eq(Drafts.sweep(U, NOW + 700, { live: 600 }).stale_sends, [], "no record: not to recovery");
	var swr = Drafts.sweep(U, NOW + 3700, { live: 600 });
	Kit.eq([swr.recovered, swr.stale_sends], [1, []], "no record: given back after an hour");
	Kit.eq(Drafts.get(U, u4).maybe_sent, NOW + 3700, "no record: marked maybe_sent");
	// (5) the live window: a record in any phase is left to the send step
	// once its heartbeat is live old; without opts.live, after an hour
	var u5 = Drafts.create(U, {});
	Drafts.save(U, u5.id, { rev: 1, body: "claimed phase" });
	Drafts.claim(U, u5.id, { send: { sid: "u5", phase: "claimed", owner: "t5", beat: NOW } });
	Kit.eq(Drafts.sweep(U, NOW + 500, { live: 600 }).stale_sends, [], "inside the live window: left");
	Kit.eq(Drafts.sweep(U, NOW + 700).stale_sends, [], "no live given: an hour");
	Kit.eq(Drafts.sweep(U, NOW + 700, { live: 600 }).stale_sends, [u5.id], "phase claimed: to recovery");
	Kit.eq(Drafts.sweep(U, NOW + 3700, { live: 600 }).recovered, 1, "an hour, nothing saved: given back by sweep");
	// the three failures above were logged (and only they)
	var expected_logs = logged.filter(function (t) {
		return /^drafts\.release: (cannot rename |simulated crash)/.test(t);
	});
	Kit.eq(expected_logs.length, 3, "the failed releases logged");
	for (var li = logged.length - 1; li >= 0; li--)
		if (expected_logs.indexOf(logged[li]) >= 0)
			logged.splice(li, 1);

	// max_drafts: the 51st draft is quota
	var B = context(W.BOB);
	Kit.eq(B.config.max_drafts, 50, "default max_drafts");
	for (var i = 0; i < 50; i++)
		Drafts.create(B, {});
	Kit.throws(function () { Drafts.create(B, {}); }, "quota");
	Kit.eq(Drafts.count(B), 50, "still 50");

	// ---- errors carry no paths ----
	var lock = B.userdata.files.lock_path("drafts");
	Kit.assert(file_mutex(lock, "held by test", 60), "take the drafts lock");
	try {
		B.userdata.files.timeout = 100;
		var held = null;
		try {
			Drafts.list(B);
		} catch (e) {
			held = e;
		}
		Kit.eq([held.code, held.reason], ["conflict", "busy"], "a held lock is conflict, busy");
		Kit.assert(held.message.indexOf("/") < 0 && held.message.indexOf("\\") < 0, "no path: " + held.message);
	} finally {
		file_remove(lock);
		B.userdata.files.timeout = 5000;
	}

	// ---- sweep ----
	var C = context(60005);
	var CROOT = C.userdata.dir + "drafts/";
	var T = time() + 3 * DAY;	// "now" for the sweep: every file is old
	C.clock = T - 3 * DAY;
	var empty_old = Drafts.create(C, {});
	var text_old = Drafts.create(C, {});
	Drafts.save(C, text_old.id, { rev: 1, body: "keep me" });
	var upl_old = Drafts.create(C, {});	// an incomplete upload, untouched
	Drafts.save(C, upl_old.id, { rev: 1, subject: "has an old upload" });
	Drafts.attach(C, upl_old.id, chunk("old-upload", "o", 0, 100, bytes(10)));
	Drafts.attach(C, upl_old.id, chunk("done-upload", "d", 0, 5, bytes(5)));
	C.clock = T - 100;
	var empty_new = Drafts.create(C, {});
	var upl_new = Drafts.create(C, {});	// a fresh incomplete upload
	Drafts.attach(C, upl_new.id, chunk("new-upload", "n", 0, 100, bytes(10)));
	// leftovers: an orphan file old and young, a directory without a
	// draft.json old and young, and a directory that is not a draft's
	var od = CROOT + upl_old.id + "/";
	file_utime(od + "a1", T - 2 * DAY, T - 2 * DAY);
	var orphan_old = od + "draft.json.123-4-5.tmp";
	var orphan_new = od + "a7";
	[orphan_old, orphan_new].forEach(function (p) {
		var x = new File(p);
		x.open("wb");
		x.write("left over");
		x.close();
	});
	file_utime(orphan_old, T - 2 * DAY, T - 2 * DAY);
	file_utime(orphan_new, T - 100, T - 100);
	var broken_old = CROOT + "00000000000000aa/";
	var broken_new = CROOT + "00000000000000bb/";
	var other = CROOT + "not-a-draft/";
	[broken_old, broken_new, other].forEach(function (p) {
		mkpath(p);
		var x = new File(p + "junk");
		x.open("wb");
		x.write("junk");
		x.close();
	});
	file_utime(broken_old + "junk", T - 2 * DAY, T - 2 * DAY);
	file_utime(broken_old, T - 2 * DAY, T - 2 * DAY);
	file_utime(broken_new + "junk", T - 100, T - 100);
	file_utime(other + "junk", T - 2 * DAY, T - 2 * DAY);
	file_utime(other, T - 2 * DAY, T - 2 * DAY);
	var res = Drafts.sweep(C, T);
	Kit.eq(res, { drafts: 2, files: 2, recovered: 0, stale_sends: [] }, "sweep counts");
	var left = Drafts.list(C).map(function (x) { return x.id; }).sort();
	Kit.eq(left, [text_old.id, upl_old.id, empty_new.id, upl_new.id].sort(), "drafts kept");
	Kit.assert(!file_isdir(CROOT + empty_old.id), "empty stale draft removed");
	Kit.eq(Drafts.get(C, upl_old.id).attachments.map(function (a) { return a.name; }), ["d"],
		"stale incomplete upload removed, complete one kept");
	Kit.eq(names(od), ["a2", "a7", "draft.json"], "old orphan removed, young orphan and files kept");
	Kit.eq(Drafts.get(C, upl_new.id).attachments.length, 1, "fresh upload kept");
	Kit.assert(!file_isdir(broken_old), "stale directory without draft.json removed");
	Kit.assert(file_exists(broken_new + "junk"), "young directory without draft.json kept");
	Kit.assert(file_exists(other + "junk"), "a directory that is not a draft's is left alone");
	// a second sweep finds nothing more
	Kit.eq(Drafts.sweep(C, T), { drafts: 0, files: 0, recovered: 0, stale_sends: [] }, "second sweep");

	// ---- reply, reply all, forward (a real scratch mailbox) ----
	var D = W.D;
	var to_alice = function (h) {
		h.to = "alice";
		h.to_ext = String(W.ALICE);
		if (h.subject === undefined)
			h.subject = "Source";
		return h;
	};
	var n_inet = W.save(to_alice({ from: "Bob Jones", from_net_type: D.NET_INTERNET,
		from_net_addr: "bob@x.org", id: "<src@x.org>", forward_path: "<alice@family.example>",
		to_list: "Alice <alice@family.example>, Carol <carol@y.org>",
		cc_list: "dave@z.org, ALICE@Vert.Example, bob@x.org" }), "hello\r\n");
	var rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_inet });
	Kit.eq([rp.mode, rp.to, rp.cc, rp.from_domain, rp.in_reply_to, rp.reply_message_id],
		["reply", "Bob Jones <bob@x.org>", "", "family.example", "s" + n_inet, "<src@x.org>"], "reply fields");
	Kit.eq(rp.dropped_recipients, 0, "nothing dropped");
	var ra = Drafts.create(A, { mode: "replyall", source_id: "s" + n_inet });
	Kit.eq([ra.to, ra.cc, ra.from_domain], ["Bob Jones <bob@x.org>", "Carol <carol@y.org>, dave@z.org",
		"family.example"], "replyall fields");
	// a name that is not ASCII: the draft holds it as plain UTF-8 text in a
	// quoted string, never as an encoded word; the address parser reads it
	// back as that name, format() encodes it from its own bytes for the
	// header, and a reader decodes that to the same name
	var JW = "J\xc3\xbcrgen Wei\xc3\x9f";
	var SM = "S\xc3\xb8ren M\xc3\xb8ller";
	var n_utf = W.save(to_alice({ from: JW, from_net_type: D.NET_INTERNET, from_net_addr: "jw@x.de",
		auxattr: D.MSG_HFIELDS_UTF8, to_list: "alice@vert.example, =?UTF-8?B?" + base64_encode(SM) + "?= <sm@y.dk>" }),
		"hallo\r\n");
	ra = Drafts.create(A, { mode: "replyall", source_id: "s" + n_utf });
	Kit.eq([ra.to, ra.cc], ['"' + JW + '" <jw@x.de>', '"' + SM + '" <sm@y.dk>'], "non-ASCII names as text");
	Kit.assert(ra.to.indexOf("=?") < 0 && ra.cc.indexOf("=?") < 0, "no encoded words in a draft");
	var AddrLib = Kit.wlib("address");
	var back = AddrLib.parse_list(ra.to + ", " + ra.cc);
	Kit.eq([back[0].name, back[1].name], [JW, SM], "the parser reads the names back");
	var hdr = AddrLib.format(back[0].name, back[0].addr);
	Kit.assert(/^=\?UTF-8\?B\?/.test(hdr), "the header form is an encoded word: " + hdr);
	Kit.eq(base64_decode(/=\?UTF-8\?B\?([^?]*)\?=/.exec(hdr)[1]), JW, "which decodes to the name");
	// an encoded word that decodes to more recipients adds none: reply
	// all goes to the real addresses only (sender, To and CC as stored)
	var EVIL = "=?UTF-8?B?" + base64_encode("Alice <alice@x.example>, Mallory") + "?= <mallory@evil.example>";
	var n_evil = W.save(to_alice({ from: "M", from_net_type: D.NET_INTERNET, from_net_addr: "m@evil.example",
		to_list: "alice@vert.example, " + EVIL, cc_list: EVIL.replace("mallory@", "mal2@") }), "x\r\n");
	ra = Drafts.create(A, { mode: "replyall", source_id: "s" + n_evil });
	var got = AddrLib.parse_list(ra.to + ", " + ra.cc).map(function (e) { return e.addr; });
	Kit.eq(got, ["m@evil.example", "mallory@evil.example", "mal2@evil.example"], "reply all: only the real addresses");
	Kit.assert(ra.cc.indexOf("alice@x.example") > 0, "the decoded text stays a name: " + ra.cc);
	var n_rte = W.save(to_alice({ from: "M", from_net_type: D.NET_INTERNET, from_net_addr: "m@evil.example",
		replyto_list: EVIL }), "x\r\n");
	rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_rte });
	Kit.eq(AddrLib.parse_list(rp.to).map(function (e) { return e.addr; }), ["mallory@evil.example"],
		"reply to a crafted Reply-To: its one real address");
	// a name that only looks like an encoded word stays literal (quoted)
	Kit.eq(AddrLib.format("=?UTF-8?B?QWRtaW4=?=", "x@y.org"), '"=?UTF-8?B?QWRtaW4=?=" <x@y.org>', "literal stays literal");
	// mail sent to another of the user's own addresses: the reply is from
	// it, and reply all leaves it out
	A.addresses = ["alice@vert.example", "alice@family.example", "Alice.Smith@vert.example",
		"ally@family.example"];
	Kit.eq(rp.from_local, "", "sent to the default name: from_local empty");
	var n_ally = W.save(to_alice({ from: "Bob Jones", from_net_type: D.NET_INTERNET, from_net_addr: "bob@x.org",
		forward_path: "<Ally@Family.Example>", to_list: "ally@family.example, carol@y.org" }), "x\r\n");
	rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_ally });
	Kit.eq([rp.from_local, rp.from_domain], ["Ally", "family.example"], "reply from the address it was sent to");
	ra = Drafts.create(A, { mode: "replyall", source_id: "s" + n_ally });
	Kit.eq([ra.to, ra.cc], ["Bob Jones <bob@x.org>", "carol@y.org"], "reply all without the user's other address");
	var n_real = W.save(to_alice({ from: "Bob Jones", from_net_type: D.NET_INTERNET, from_net_addr: "bob@x.org",
		forward_path: "<alice.smith@vert.example>" }), "x\r\n");
	rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_real });
	Kit.eq([rp.from_local, rp.from_domain], ["alice.smith", ""], "a name at the default domain");
	var n_else = W.save(to_alice({ from: "Bob Jones", from_net_type: D.NET_INTERNET, from_net_addr: "bob@x.org",
		forward_path: "<list@family.example>" }), "x\r\n");
	rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_else });
	Kit.eq([rp.from_local, rp.from_domain], ["", "family.example"], "not the user's address: only its domain");
	// ... or to one the list does not offer but the server delivers to them
	A.own_address = function (a) {
		return a.toLowerCase() === "alice#shop@family.example";
	};
	var n_tag = W.save(to_alice({ from: "Shop", from_net_type: D.NET_INTERNET, from_net_addr: "shop@x.org",
		forward_path: "<alice#shop@family.example>", to_list: "alice#shop@family.example, carol@y.org" }), "x\r\n");
	var rt = Drafts.create(A, { mode: "replyall", source_id: "s" + n_tag });
	Kit.eq([rt.from_local, rt.from_domain, rt.cc], ["alice#shop", "family.example", "carol@y.org"],
		"reply from a tagged address of the user's, which reply all leaves out");
	rt = Drafts.save(A, rt.id, { rev: rt.rev, body: "kept" });
	Kit.eq(rt.from_local, "alice#shop", "kept by a save of something else");
	Kit.throws(function () {
		Drafts.save(A, rt.id, { rev: rt.rev, from_local: "alice#other" });
	}, "invalid");
	delete A.own_address;
	// saving a name: with the draft's domain it must be one of the addresses
	var sv = Drafts.save(A, rp.id, { rev: rp.rev, from_local: "ally" });
	Kit.eq([sv.from_local, sv.from_domain], ["ally", "family.example"], "a name saved");
	// ally is not at the default domain; mallory is not the user's name
	Kit.throws(function () {
		Drafts.save(A, sv.id, { rev: sv.rev, from_domain: "" });
	}, "invalid");
	Kit.throws(function () {
		Drafts.save(A, sv.id, { rev: sv.rev, from_local: "mallory" });
	}, "invalid");
	sv = Drafts.save(A, sv.id, { rev: sv.rev, from_local: "", from_domain: "" });
	Kit.eq([sv.from_local, sv.from_domain], ["", ""], "back to the default");
	delete A.addresses;
	// reply-to wins; mail sent to the default domain gives from_domain ""
	var n_rt = W.save(to_alice({ from: "List", from_net_type: D.NET_INTERNET, from_net_addr: "list@l.org",
		replyto_list: "list-reply@l.org", forward_path: "<alice@vert.example>" }), "x\r\n");
	rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_rt });
	Kit.eq([rp.to, rp.from_domain], ["list-reply@l.org", ""], "reply-to and default domain");
	// local mail: the sender's name; the user left out of reply all
	var n_local = W.save(to_alice({ from: "Bob Smith", from_ext: "60002" }), "local\r\n");
	ra = Drafts.create(A, { mode: "replyall", source_id: "s" + n_local });
	Kit.eq([ra.to, ra.cc, ra.from_domain], ["Bob Smith", "", ""], "local replyall");
	// FidoNet: a form the address parser reads back
	var n_fido = W.save(to_alice({ from: "John Doe", from_net_type: D.NET_FIDO, from_net_addr: "1:103/705" }),
		"fido\r\n");
	rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_fido });
	Kit.eq(rp.to, '"John Doe" <John.Doe@1:103/705>', "fido reply");
	// QWK: name@QWKID; a route cannot be written, so it is dropped
	var n_qwk = W.save(to_alice({ from: "Jane Roe", from_net_type: D.NET_QWK, from_net_addr: "VERT" }), "q\r\n");
	rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_qwk });
	Kit.eq([rp.to, rp.dropped_recipients], ['"Jane Roe" <Jane.Roe@VERT>', 0], "qwk reply");
	var n_route = W.save(to_alice({ from: "Jane Roe", from_net_type: D.NET_QWK, from_net_addr: "VERT/NIX" }),
		"q\r\n");
	rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_route });
	Kit.eq([rp.to, rp.dropped_recipients], ["", 1], "unwritable sender dropped and counted");
	// reply all: the user is removed from Reply-To too
	var n_rt2 = W.save(to_alice({ from: "Ann", from_net_type: D.NET_INTERNET, from_net_addr: "ann@a.org",
		replyto_list: "alice@vert.example, team@a.org", to_list: "alice@vert.example, zed@z.org" }), "r\r\n");
	ra = Drafts.create(A, { mode: "replyall", source_id: "s" + n_rt2 });
	Kit.eq([ra.to, ra.cc], ["team@a.org", "zed@z.org"], "replyall without the user in Reply-To");
	// a reply to one's own message goes to its recipients
	var n_own = W.save({ to: "bob", to_ext: String(W.BOB), from: "Alice Display", from_ext: String(W.ALICE),
		subject: "mine", to_list: "Bob <bob@x.org>, alice@vert.example", cc_list: "carol@y.org" }, "m\r\n");
	rp = Drafts.create(A, { mode: "reply", source_id: "s" + n_own });
	Kit.eq([rp.to, rp.cc], ["Bob <bob@x.org>", ""], "reply to own message");
	ra = Drafts.create(A, { mode: "replyall", source_id: "s" + n_own });
	Kit.eq([ra.to, ra.cc], ["Bob <bob@x.org>", "carol@y.org"], "replyall to own message");
	// a cc line over the limit loses whole entries from the end
	var many = function (tag) {
		var a = [];
		var label = new Array(61).join("d");
		for (var k = 0; k < 45; k++)
			a.push('"' + tag + " Person Number " + k + ' With A Longish Name" <' + tag + k
				+ new Array(41).join("x") + "@" + label + "." + label + ".example>");
		return a.join(", ");
	};
	var n_many = W.save(to_alice({ from: "M", from_net_type: D.NET_INTERNET, from_net_addr: "m@m.org",
		to_list: many("To"), cc_list: many("Cc") }), "many\r\n");
	ra = Drafts.create(A, { mode: "replyall", source_id: "s" + n_many });
	Kit.assert(ra.cc.length > 10000 && ra.cc.length <= Drafts.MAX_ADDR_FIELD, "cc kept and bounded: " + ra.cc.length);
	Kit.assert(ra.dropped_recipients > 0, "dropped counted: " + ra.dropped_recipients);
	var parsed = Kit.wlib("address").parse_list(ra.cc);
	Kit.eq(parsed.length + ra.dropped_recipients, 90, "whole entries only");
	// another user's message: the Store's refusal
	var n_bob = W.save({ to: "bob", to_ext: String(W.BOB), from: "X", subject: "for bob" }, "b\r\n");
	var count_before = Drafts.count(A);
	var store_e = null;
	try {
		A.store.get("s" + n_bob);
	} catch (e) {
		store_e = e;
	}
	var dr_e = null;
	try {
		Drafts.create(A, { mode: "reply", source_id: "s" + n_bob });
	} catch (e) {
		dr_e = e;
	}
	Kit.eq([dr_e.code, dr_e.message], ["forbidden", store_e.message], "the Store's code and message");
	try {
		A.store.get("s999999");
	} catch (e) {
		store_e = e;
	}
	try {
		Drafts.create(A, { mode: "forward", source_id: "s999999" });
	} catch (e) {
		dr_e = e;
	}
	Kit.eq([dr_e.code, dr_e.message], ["notfound", store_e.message], "a missing source: the Store's message");
	Kit.assert(dr_e.message !== "no such draft", "not the drafts text");
	Kit.throws(function () { Drafts.create(A, { mode: "forward", source_id: "s" + n_bob }); }, "forbidden");
	Kit.throws(function () { Drafts.create(A, { mode: "reply", source_id: "../x" }); }, "invalid");
	Kit.eq(Drafts.count(A), count_before, "no draft from a refused source");

	// forward: the attachments come along
	var mixed = Kit.mime_fixtures.mixed();
	var mh = to_alice(mixed.hdr);
	mh.from = "Sender";
	var n_mixed = W.save(mh, mixed.body);
	var F = context(W.ALICE, { max_attachment_bytes: 100000, max_draft_bytes: 10000000 });
	var fw = Drafts.create(F, { mode: "forward", source_id: "s" + n_mixed });
	Kit.eq(fw.to, "", "forward has no recipient");
	Kit.eq(fw.attachments.map(function (a) { return [a.name, a.type, a.complete]; }),
		[["dot.png", "image/png", true], ["notes.txt", "text/plain", true]], "forwarded attachments");
	var fdir = F.userdata.dir + "drafts/" + fw.id + "/";
	Kit.eq(md5_calc(read_file(fdir + fw.attachments[0].id), true), md5_calc(mixed.png, true), "png copied");
	Kit.eq(md5_calc(read_file(fdir + fw.attachments[1].id), true), md5_calc(mixed.notes, true),
		"notes copied");
	// all or nothing: over the per-message limit or the quota, no draft
	var my_temps = function () {
		return names(system.temp_dir).filter(function (n) {
			return n.indexOf("mailbox_part_" + W.ALICE + "_") === 0;
		});
	};
	var temp_before = my_temps();
	count_before = Drafts.count(F);
	F.config.max_attachment_bytes = mixed.png.length + 10;
	Kit.throws(function () { Drafts.create(F, { mode: "forward", source_id: "s" + n_mixed }); }, "toolarge");
	F.config.max_attachment_bytes = 100000;
	F.config.max_draft_bytes = Drafts.usage(F).bytes + mixed.png.length + 10;
	Kit.throws(function () { Drafts.create(F, { mode: "forward", source_id: "s" + n_mixed }); }, "quota");
	Kit.eq(Drafts.count(F), count_before, "no partial draft");
	Kit.eq(my_temps(), temp_before, "no temp file left behind");
	var leftovers = names(F.userdata.dir + "drafts/").filter(function (n) {
		return !file_exists(F.userdata.dir + "drafts/" + n + "draft.json");
	});
	Kit.eq(leftovers, [], "no draft directory without draft.json");

	// no failure was logged along the way
	Kit.eq(logged.filter(function (t) { return t.indexOf("drafts.") === 0; }), [], "no unexpected failures");
} finally {
	contexts.forEach(function (c) {
		try {
			c.close();
		} catch (e) {
		}
	});
	W.cleanup();
}
