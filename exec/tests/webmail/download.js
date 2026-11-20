// Download: serving one message part (webmail/lib/download.js), over a real
// Store and Session on scratch files

load(js.exec_dir + "kit.js");
var Download = Kit.wlib("download");
var Api = Kit.wlib("api");
var Config = Kit.wlib("config");
var UserData = Kit.wlib("userdata");
var Session = Kit.wlib("session");
var JsonFiles = Kit.lib("mailbox_files");
var Store = Kit.lib("mailbox");

var ALICE = 60001;
var BOB = 60002;
Config.warn = function () {};
var cfg = Config.load({ directory: Kit.exec_root() + "../webmail/" });

function fake_user(n, alias)
{
	return { number: n, alias: alias, name: alias, lang: "", settings: 0, is_guest: false,
		security: { restrictions: 0, password: "x", password_date: 1600000000 }, compare_ars: function () { return true; } };
}
var USERS = {};
USERS[ALICE] = fake_user(ALICE, "alice");
USERS[BOB] = fake_user(BOB, "bob");

var S = Kit.scratch();
var shared = null;

function fail(what)
{
	return function () {
		throw new Error("must not be called: " + what);
	};
}

function make_env(o)
{
	o = o || {};
	var ev = {
		method: o.method || "GET",
		config: o.config || cfg,
		secure: o.secure === undefined ? true : o.secure,
		cookie: o.cookie,
		made: 0,
		closed: 0,
		cookies: [],
		get_user: o.get_user || function (n) {
			return USERS.hasOwnProperty(n) ? USERS[n] : null;
		},
		userdata: o.userdata || function (n) {
			return new UserData(S.user_dir(n) + "webmail/", JsonFiles);
		},
		set_cookie: function (v, keep) {
			ev.cookies.push([v, keep]);
		}
	};
	var make = Api.context_factory({ config: ev.config, get_user: ev.get_user, userdata: ev.userdata,
		Store: Store, shared_path: S.shared_path, data_dir: S.data_dir });
	ev.make_ctx = o.make_ctx || function (n) {
		ev.made++;
		var ctx = make(n);
		var close = ctx.close;
		ctx.close = function () {
			ev.closed++;
			close.call(ctx);
		};
		return ctx;
	};
	return ev;
}

function sealed_env(o)
{
	o = o || {};
	o.make_ctx = fail("make_ctx");
	o.userdata = fail("userdata");
	o.get_user = fail("get_user");
	return make_env(o);
}

// A reply that records; fail_after: throw on that write (1-based)
function Reply(fail_after)
{
	this.st = null;
	this.headers = {};
	this.body = "";
	this.writes = [];
	this.tracked = [];
	this.fail_after = fail_after || 0;
}
Reply.prototype.status = function (s) {
	this.st = s;
};
Reply.prototype.header = function (k, v) {
	this.headers[k] = v;
};
Reply.prototype.write = function (d) {
	this.writes.push(d.length);
	if (this.fail_after && this.writes.length >= this.fail_after)
		throw new Error("client went away");
	this.body += d;
};
Reply.prototype.track = function (p) {
	this.tracked.push(p);
};

function serve(ev, query, fail_after)
{
	var r = new Reply(fail_after);
	Download.serve(ev, query, r);
	return r;
}

function text_reply(r, status, what)
{
	Kit.eq(r.st, status, what + ": status");
	Kit.eq(r.headers["Content-Type"], "text/plain; charset=utf-8", what + ": plain text");
	Kit.eq(r.headers["X-Content-Type-Options"], "nosniff", what + ": nosniff");
	Kit.assert(r.body.length > 0 && r.body.length < 100 && r.body.indexOf("/") < 0,
		what + ": a short fixed text: " + r.body);
}

var logged = [];
var real_log = Download.log;
Download.log = function (level, text) {
	logged.push(text);
};

try {
	// --- parameters
	Kit.eq(Download.params({ id: ["s1"], pid: ["2"], dl: ["0"] }), { id: "s1", pid: "2", dl: false });
	Kit.eq(Download.params({ id: "p12", pid: "1.2.3" }), { id: "p12", pid: "1.2.3", dl: true },
		"dl absent: download");
	Kit.eq(Download.params({ id: ["s1"], pid: ["raw"], dl: ["1"] }).pid, "raw");
	Kit.eq(Download.params({ id: ["s1"], pid: ["f3"] }).pid, "f3");
	var bad = [
		{ id: ["s1; rm"], pid: ["1"] }, { id: ["../x"], pid: ["1"] }, { id: ["s0"], pid: ["1"] },
		{ id: ["s1"], pid: ["../x"] }, { id: ["s1"], pid: ["1."] }, { id: ["s1"], pid: ["f0"] },
		{ id: ["s1"], pid: ["RAW"] }, { id: ["s1"], pid: ["1234567"] }, { id: ["s1"], pid: [""] },
		{ id: ["s1"], pid: ["1"], dl: ["2"] }, { id: ["s1"], pid: ["1"], dl: [""] },
		{ id: ["s1", "s2"], pid: ["1"] }, { id: [], pid: ["1"] }, { pid: ["1"] }, { id: ["s1"] },
		{ id: [5], pid: ["1"] }, null, "x"
	];
	for (var i = 0; i < bad.length; i++)
		Kit.eq(Download.params(bad[i]), null, "bad parameters " + JSON.stringify(bad[i]));

	// --- headers and type rules
	var h = Download.headers({ name: "a.png", type: "image/png" }, 10, false, false);
	Kit.eq(h, { "Content-Type": "image/png", "Content-Length": "10",
		"Content-Disposition": "inline; filename=\"a.png\"; filename*=UTF-8''a.png",
		"X-Content-Type-Options": "nosniff", "Content-Security-Policy": "sandbox",
		"Cross-Origin-Resource-Policy": "same-origin", "Cache-Control": "no-store" },
		"an image shown inline");
	var types = ["image/png", "image/jpeg", "image/gif", "image/webp"];
	for (i = 0; i < types.length; i++) {
		h = Download.headers({ name: "x", type: types[i] }, 1, false, false);
		Kit.eq(h["Content-Type"], types[i]);
		Kit.eq(h["Content-Disposition"].substr(0, 7), "inline;", types[i] + " inline with dl=0");
		h = Download.headers({ name: "x", type: types[i] }, 1, true, false);
		Kit.eq(h["Content-Type"], types[i]);
		Kit.eq(h["Content-Disposition"].substr(0, 11), "attachment;", types[i] + " attachment with dl=1");
	}
	var others = ["text/html", "image/svg+xml", "text/plain", "application/pdf", "image/bmp",
		"application/octet-stream", "", undefined];
	for (i = 0; i < others.length; i++) {
		h = Download.headers({ name: "x", type: others[i] }, 1, false, false);
		Kit.eq(h["Content-Type"], "application/octet-stream", others[i] + " sent as octet-stream");
		Kit.eq(h["Content-Disposition"].substr(0, 11), "attachment;", others[i] + " always attached");
	}
	h = Download.headers({ name: "m.eml", type: "image/png" }, 5, false, true);
	Kit.eq(h["Content-Type"], "message/rfc822", "raw");
	Kit.eq(h["Content-Disposition"].substr(0, 11), "attachment;", "raw always attached");

	// --- file names
	function disp(name)
	{
		return Download.disposition(name, false);
	}
	Kit.eq(disp("a\"b\".txt"), "attachment; filename=\"ab.txt\"; filename*=UTF-8''ab.txt", "quotes");
	Kit.eq(disp("../../etc/passwd"), "attachment; filename=\"....etcpasswd\"; filename*=UTF-8''....etcpasswd",
		"slashes");
	Kit.eq(disp("C:\\temp\\x.txt"), "attachment; filename=\"C:tempx.txt\"; filename*=UTF-8''C%3Atempx.txt",
		"backslashes");
	Kit.eq(disp("a\r\nSet-Cookie: x=1.txt"),
		"attachment; filename=\"aSet-Cookie: x=1.txt\"; filename*=UTF-8''aSet-Cookie%3A%20x%3D1.txt",
		"CR/LF removed");
	Kit.eq(disp("tab\there\x00\x7f\xC2\x85.txt"),
		"attachment; filename=\"tabhere.txt\"; filename*=UTF-8''tabhere.txt", "control characters");
	Kit.eq(disp("Caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC.txt"),
		"attachment; filename=\"Caf_ _.txt\"; filename*=UTF-8''Caf%C3%A9%20%E6%97%A5%E6%9C%AC.txt",
		"non-ASCII: RFC 5987 form");
	Kit.eq(disp("100% it's;ok.txt"),
		"attachment; filename=\"100_ it's;ok.txt\"; filename*=UTF-8''100%25%20it%27s%3Bok.txt",
		"percent, apostrophe and semicolon");
	Kit.eq(disp(""), "attachment; filename=\"download\"; filename*=UTF-8''download", "empty name");
	// Unicode format characters (as UTF-8 bytes) are removed: they can make
	// "gpj.exe" look like "exe.jpg"
	var FORMAT = ["\xE2\x80\x8B", "\xE2\x80\x8C", "\xE2\x80\x8D", "\xE2\x80\x8E", "\xE2\x80\x8F",
		"\xE2\x80\xAA", "\xE2\x80\xAB", "\xE2\x80\xAC", "\xE2\x80\xAD", "\xE2\x80\xAE",
		"\xE2\x81\xA6", "\xE2\x81\xA7", "\xE2\x81\xA8", "\xE2\x81\xA9", "\xEF\xBB\xBF"];
	for (i = 0; i < FORMAT.length; i++)
		Kit.eq(disp("a" + FORMAT[i] + "b.txt"), "attachment; filename=\"ab.txt\"; filename*=UTF-8''ab.txt",
			"format character " + escape(FORMAT[i]));
	Kit.eq(disp("x\xE2\x80\xAEgpj.exe"), "attachment; filename=\"xgpj.exe\"; filename*=UTF-8''xgpj.exe",
		"right-to-left override");
	// removing one can assemble another from the bytes around it
	Kit.eq(disp("a\xE2\x80\xE2\x80\x8B\xAEb.txt"),
		"attachment; filename=\"ab.txt\"; filename*=UTF-8''ab.txt", "an assembled override");
	Kit.eq(disp("a\xC2\xE2\x80\x8B\x85b.txt"),
		"attachment; filename=\"ab.txt\"; filename*=UTF-8''ab.txt", "an assembled C1 control");
	Kit.eq(disp("\xE2\x80\x90-dash.txt"), "attachment; filename=\"_-dash.txt\"; filename*=UTF-8''%E2%80%90-dash.txt",
		"a neighbor of the format range is kept");
	Kit.eq(disp("\"/\\\r\n"), "attachment; filename=\"download\"; filename*=UTF-8''download",
		"nothing left");
	Kit.eq(disp(undefined), "attachment; filename=\"download\"; filename*=UTF-8''download", "no name");
	Kit.eq(disp("  x.txt  "), "attachment; filename=\"x.txt\"; filename*=UTF-8''x.txt", "spaces trimmed");
	Kit.eq(Download.disposition("a.png", true), "inline; filename=\"a.png\"; filename*=UTF-8''a.png");
	// long names are cut at a character boundary
	var long_name = new Array(200).join("a") + "\xC3\xA9\xC3\xA9";	// 199 + 4 bytes
	var d = disp(long_name);
	Kit.assert(d.indexOf("filename*=UTF-8''" + new Array(200).join("a") + "\"") < 0, "cut");
	Kit.assert(/filename\*=UTF-8''a{199}$/.test(d), "the split character is dropped: " + d.substr(-30));
	d = disp(new Array(198).join("a") + "\xC3\xA9\xC3\xA9");	// 197 + 4: one whole character fits
	Kit.assert(/filename\*=UTF-8''a{197}%C3%A9$/.test(d), "a whole character kept: " + d.substr(-30));
	// a header value never holds CR, LF or a double quote inside the quoted name
	for (i = 0; i < 256; i++) {
		d = disp("x" + String.fromCharCode(i) + "y");
		Kit.assert(!/[\r\n\x00]/.test(d), "no control character for byte " + i);
		Kit.assert(d.split("\"").length === 3, "exactly one quoted string for byte " + i);
	}

	// --- serving, over the scratch bases
	shared = Kit.email_base(S.shared_path);
	var mixed = Kit.mime_fixtures.mixed();
	var big = Kit.mime_fixtures.big_text(200000);
	var hdr = mixed.hdr;
	hdr.to = "Alice";
	hdr.from = "Carol";
	hdr.to_ext = String(ALICE);
	hdr.subject = "Pictures";
	Kit.assert(shared.save_msg(hdr, mixed.body), "save mixed");
	var M = "s" + shared.last_msg;
	hdr = big.hdr;
	hdr.to = "Alice";
	hdr.from = "Dave";
	hdr.to_ext = String(ALICE);
	hdr.subject = "Big";
	Kit.assert(shared.save_msg(hdr, big.body), "save big");
	var BIG = "s" + shared.last_msg;
	Kit.assert(shared.save_msg({ to: "Bob", from: "Eve", to_ext: String(BOB), subject: "bob's" },
		"for bob\r\n"), "save bob's");
	var BOBS = "s" + shared.last_msg;
	shared.close();
	shared = null;

	var alice = Session.create(new UserData(S.user_dir(ALICE) + "webmail/", JsonFiles), ALICE, null,
		false, time(), cfg, sha256_calc("x\n1600000000", true));	// password and date

	// the part ids, from the store
	var st = new Store({ usernum: ALICE, shared_path: S.shared_path, data_dir: S.data_dir });
	st.open();
	var msg = st.get(M);
	var big_msg = st.get(BIG);
	st.close();
	var png_pid = null;
	var notes_pid = null;
	for (i = 0; i < msg.parts.length; i++) {
		if (msg.parts[i].type === "image/png")
			png_pid = msg.parts[i].pid;
		else if (msg.parts[i].name === "notes.txt")
			notes_pid = msg.parts[i].pid;
	}
	Kit.assert(png_pid !== null && notes_pid !== null, "fixture parts: " + JSON.stringify(msg.parts));

	// an image, inline: the exact bytes, and the temp file removed
	var ev = make_env({ cookie: alice });
	var r = serve(ev, { id: [M], pid: [png_pid], dl: ["0"] });
	Kit.eq(r.st, "200 OK", "image status");
	Kit.eq(r.headers["Content-Type"], "image/png");
	Kit.eq(r.headers["Content-Disposition"], "inline; filename=\"dot.png\"; filename*=UTF-8''dot.png");
	Kit.eq(r.headers["Content-Length"], String(mixed.png.length));
	Kit.eq(r.headers["Content-Security-Policy"], "sandbox");
	Kit.eq(r.headers["X-Content-Type-Options"], "nosniff");
	Kit.assert(r.body === mixed.png, "the image bytes");
	Kit.eq(r.tracked.length, 1, "the temp file was tracked");
	Kit.assert(!file_exists(r.tracked[0]), "the temp file was removed");
	Kit.eq([ev.made, ev.closed], [1, 1], "one mailbox, closed");
	// the caller's Api library is the one used
	var used = 0;
	var wrapped = { session_user: function (e, touch) {
		used++;
		return Api.session_user(e, touch);
	} };
	r = new Reply();
	Download.serve(make_env({ cookie: alice }), { id: [M], pid: [png_pid], dl: ["0"] }, r, wrapped);
	Kit.eq([r.st, used], ["200 OK", 1], "served with the given Api");
	// the same image as a download
	r = serve(make_env({ cookie: alice }), { id: [M], pid: [png_pid], dl: ["1"] });
	Kit.eq(r.headers["Content-Type"], "image/png");
	Kit.eq(r.headers["Content-Disposition"].substr(0, 11), "attachment;");
	// a text attachment: octet-stream, attached even with dl=0
	r = serve(make_env({ cookie: alice }), { id: [M], pid: [notes_pid], dl: ["0"] });
	Kit.eq(r.headers["Content-Type"], "application/octet-stream");
	Kit.eq(r.headers["Content-Disposition"], "attachment; filename=\"notes.txt\"; filename*=UTF-8''notes.txt");
	Kit.assert(r.body === mixed.notes, "the decoded text");
	// raw: the whole message, message/rfc822
	r = serve(make_env({ cookie: alice }), { id: [M], pid: ["raw"], dl: ["0"] });
	Kit.eq(r.st, "200 OK");
	Kit.eq(r.headers["Content-Type"], "message/rfc822");
	Kit.eq(r.headers["Content-Disposition"].substr(0, 11), "attachment;");
	Kit.assert(r.headers["Content-Disposition"].indexOf("Pictures.eml") > 0, "eml name");
	Kit.eq(r.headers["Content-Length"], String(r.body.length));
	Kit.assert(!file_exists(r.tracked[0]), "raw temp removed");
	Kit.eq([r.headers["X-Content-Type-Options"], r.headers["Content-Security-Policy"],
		r.headers["Cross-Origin-Resource-Policy"], r.headers["Cache-Control"]],
		["nosniff", "sandbox", "same-origin", "no-store"], "raw: the safe headers");
	Kit.assert(r.body.indexOf("Subject: ") >= 0 && r.body.indexOf("\r\n\r\n") > 0, "raw: header block and body");
	// raw of another user's message: refused, nothing sent
	r = serve(make_env({ cookie: alice }), { id: [BOBS], pid: ["raw"] });
	Kit.eq(r.st, "403 Forbidden", "raw of another user's message");
	Kit.eq(r.headers["Content-Type"], "text/plain; charset=utf-8", "raw refused: a plain error");

	// a large part goes out in 64 KB writes
	var big_pid = big_msg.parts.length > 0 ? big_msg.parts[0].pid : null;
	if (big_pid === null)
		big_pid = "1";
	r = serve(make_env({ cookie: alice }), { id: [BIG], pid: [big_pid] });
	Kit.eq(r.st, "200 OK", "big part");
	Kit.assert(r.writes.length >= 4, "several writes: " + r.writes.length);
	for (i = 0; i < r.writes.length; i++)
		Kit.assert(r.writes[i] <= Download.CHUNK, "write of at most 64 KB");
	Kit.eq(r.headers["Content-Length"], String(r.body.length), "length matches what was sent");

	// the client goes away mid-transfer: no throw, temp file removed, closed
	ev = make_env({ cookie: alice });
	logged = [];
	r = serve(ev, { id: [BIG], pid: [big_pid] }, 2);
	Kit.eq(r.st, "200 OK", "headers were already sent");
	Kit.eq(r.writes.length, 2, "stopped at the failing write");
	Kit.assert(!file_exists(r.tracked[0]), "temp removed after a failed write");
	Kit.eq(ev.closed, 1, "mailbox closed after a failed write");
	Kit.assert(logged.length === 1 && logged[0].indexOf("client went away") > 0, "logged");

	// a file shorter than it was when the length was sent: logged, the
	// reply ends early, nothing thrown, temp removed
	var real_part_file = Store.prototype.part_file;
	var shrink_path = null;
	Store.prototype.part_file = function (id, pid) {
		var info = real_part_file.call(this, id, pid);
		shrink_path = info.path;
		return info;
	};
	var real_read = Download.read;
	var reads = 0;
	Download.read = function (file, n) {
		if (file.name === shrink_path && ++reads > 1)
			return "";
		return real_read(file, n);
	};
	logged = [];
	try {
		r = serve(make_env({ cookie: alice }), { id: [BIG], pid: [big_pid] });
	} finally {
		Store.prototype.part_file = real_part_file;
		Download.read = real_read;
	}
	Kit.eq(r.st, "200 OK", "short read: headers sent");
	Kit.eq(r.headers.Connection, undefined, "no Connection header");
	Kit.assert(r.body.length < parseInt(r.headers["Content-Length"], 10), "fewer bytes than promised");
	Kit.assert(logged.length === 1 && logged[0].indexOf("short read") > 0, "short read logged: "
		+ JSON.stringify(logged));
	Kit.assert(!file_exists(shrink_path), "temp removed after a short read");

	// a file that grows while it is sent: never more bytes than
	// Content-Length, and the length is taken once
	Store.prototype.part_file = function (id, pid) {
		var info = real_part_file.call(this, id, pid);
		shrink_path = info.path;
		return info;
	};
	Download.read = function (file, n) {
		var c = real_read(file, n);
		return file.name === shrink_path ? c + "EXTRA" : c;
	};
	try {
		r = serve(make_env({ cookie: alice }), { id: [BIG], pid: [big_pid] });
	} finally {
		Store.prototype.part_file = real_part_file;
		Download.read = real_read;
	}
	Kit.eq(r.st, "200 OK", "growing file");
	Kit.eq(String(r.body.length), r.headers["Content-Length"], "exactly Content-Length bytes sent");

	// --- refusals
	var before = directory(system.temp_dir + "*").length;
	r = serve(sealed_env({ cookie: alice, method: "POST" }), { id: [M], pid: ["1"] });
	text_reply(r, "405 Method Not Allowed", "POST");
	Kit.eq(r.headers.Allow, "GET");
	r = serve(sealed_env({ cookie: alice, method: "HEAD" }), { id: [M], pid: ["1"] });
	Kit.eq(r.st, "405 Method Not Allowed", "HEAD");
	r = serve(sealed_env({ cookie: alice, secure: false }), { id: [M], pid: ["1"] });
	text_reply(r, "403 Forbidden", "not secure");
	r = serve(make_env({ cookie: alice, secure: false,
		config: Config.load({ directory: cfg.directory, require_tls: false }) }), { id: [M], pid: [png_pid] });
	Kit.eq(r.st, "200 OK", "require_tls false: served");
	r = serve(sealed_env({ cookie: alice }), { id: ["../x"], pid: ["1"] });
	text_reply(r, "400 Bad Request", "bad id: refused before the session is read");
	r = serve(sealed_env({ cookie: alice }), { id: [M], pid: ["1; rm"] });
	text_reply(r, "400 Bad Request", "bad pid");
	ev = make_env({ make_ctx: fail("make_ctx") });
	r = serve(ev, { id: [M], pid: [png_pid] });
	text_reply(r, "401 Unauthorized", "no cookie");
	Kit.eq(ev.cookies, [], "nothing to clear");
	ev = make_env({ cookie: "junk", make_ctx: fail("make_ctx") });
	r = serve(ev, { id: [M], pid: [png_pid] });
	text_reply(r, "401 Unauthorized", "bad cookie");
	Kit.eq(ev.cookies, [["", false]], "bad cookie cleared");
	r = serve(make_env({ cookie: alice }), { id: [M], pid: ["9"] });
	text_reply(r, "404 Not Found", "no such part");
	r = serve(make_env({ cookie: alice }), { id: ["s99999"], pid: ["1"] });
	text_reply(r, "404 Not Found", "no such message");
	r = serve(make_env({ cookie: alice }), { id: [BOBS], pid: ["1"] });
	text_reply(r, "403 Forbidden", "another user's message");
	r = serve(make_env({ cookie: alice }), { id: [M], pid: ["f1"] });
	text_reply(r, "404 Not Found", "no file attachment");
	logged = [];
	r = serve(make_env({ cookie: alice, make_ctx: function () {
		throw new Error("cannot open /secret/mail");
	} }), { id: [M], pid: [png_pid] });
	text_reply(r, "500 Internal Server Error", "a failure");
	Kit.assert(r.body.indexOf("secret") < 0, "no detail sent");
	Kit.assert(logged.length === 1 && logged[0].indexOf("/secret/mail") > 0, "detail logged");
	Kit.eq(directory(system.temp_dir + "*").length, before, "no temp file left behind");
} finally {
	Download.log = real_log;
	if (shared)
		shared.close();
	S.cleanup();
}
