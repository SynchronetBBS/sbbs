// Api: the batched JSON calls (webmail/lib/api.js), driven with a fake
// env over real Store, Session and UserData objects on scratch files

load(js.exec_dir + "kit.js");
var Api = Kit.wlib("api");
var Config = Kit.wlib("config");
var UserData = Kit.wlib("userdata");
var Session = Kit.wlib("session");
var JsonFiles = Kit.lib("mailbox_files");
var Store = Kit.lib("mailbox");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");
var U = {};
load(U, Kit.exec_root() + "load/userdefs.js");

var ALICE = 60001;
var BOB = 60002;
var GUEST = 60003;
var DENIED = 60004;	// fails access_requirements
var GONE = 60005;	// deleted
var IDLE = 60006;	// inactive
var NOPASS = 60007;	// no password stored
var GUEST_FLAG = 60008;	// is_guest false but the G restriction set
var HIGH = 70001;	// above 65535: the mail index cannot hold it

var WEBMAIL_DIR = Kit.exec_root() + "../webmail/";
Config.warn = function () {};
var cfg = Config.load({ directory: WEBMAIL_DIR, access_requirements: "LEVEL 10",
	poll_interval: 30, max_drafts: 7, product_name: "Mail <b>", brand: "<i>B</i>",
	login_text: "Hello", footer_links: "<a href=x>x</a>", fallback_url: "/old" });
var cfg_plain = Config.load({ directory: WEBMAIL_DIR, require_tls: false });
var cfg_webv4 = Config.load({ directory: WEBMAIL_DIR, webv4_login: true,
	access_requirements: "LEVEL 10" });

var STRINGS = (function () {
	var Locale = Kit.wlib("locale");
	return Locale.load(WEBMAIL_DIR + "lib/", WEBMAIL_DIR + "mods/", "en_us");
})();

var USERS = {};
var ars_calls = [];
function add_user(n, o)
{
	o = o || {};
	USERS[n] = {
		number: n,
		alias: o.alias || ("user" + n),
		name: o.name || ("User " + n),
		lang: o.lang || "",
		settings: o.settings || 0,
		is_guest: o.guest === true,
		security: { restrictions: o.rest || 0, password: o.password === undefined ? "pw" + n : o.password,
			password_date: 1600000000 },
		compare_ars: function (ars) {
			ars_calls.push([n, ars]);
			return o.ars_ok !== false;
		}
	};
}
add_user(ALICE, { alias: "alice", name: "Alice A" });
add_user(BOB, { alias: "bob" });
add_user(GUEST, { alias: "guest", guest: true, rest: U.UFLAG_G });
add_user(DENIED, { alias: "denied", ars_ok: false });
add_user(GONE, { alias: "gone", settings: U.USER_DELETED });
add_user(IDLE, { alias: "idle", settings: U.USER_INACTIVE });
add_user(NOPASS, { alias: "nopass", password: "" });
add_user(GUEST_FLAG, { alias: "gflag", rest: U.UFLAG_G });
add_user(HIGH, { alias: "high" });

var S = Kit.scratch();
var shared = null;

function fail(what)
{
	return function () {
		throw new Error("must not be called: " + what);
	};
}

// The production context factory over the scratch bases, counting
function counting_factory(ev, config)
{
	var make = Api.context_factory({ config: config, get_user: ev.get_user,
		userdata: ev.userdata, Store: Store, shared_path: S.shared_path, data_dir: S.data_dir });
	return function (usernum) {
		ev.made++;
		var ctx = make(usernum);
		var close = ctx.close;
		ctx.close = function () {
			ev.closed++;
			close.call(ctx);
		};
		return ctx;
	};
}

function make_env(o)
{
	o = o || {};
	var ev = {
		cookie: o.cookie,
		secure: o.secure === undefined ? true : o.secure,
		has_header: o.has_header === undefined ? true : o.has_header,
		config: o.config || cfg,
		accept_language: o.accept_language || "",
		client: { ip: "192.0.2.1", agent: "Test/1.0" },
		webv4_calls: 0,
		webv4_user: o.webv4_fn || function () {
			ev.webv4_calls++;
			return o.webv4_user || 0;
		},
		finds: [],
		find_user: o.find_user || function (name) {
			ev.finds.push(name);
			for (var n in USERS)
				if (USERS[n].alias === name)
					return USERS[n].number;
			return 0;
		},
		now: o.now,
		// the avatar library: none unless a test gives one (so no test
		// reads a real user directory)
		avatar_lib: o.avatar_lib === undefined ? null : o.avatar_lib,
		made: 0,
		closed: 0,
		cookies: [],
		logins: [],
		login: o.login || function (name, password) {
			ev.logins.push(name);
			for (var n in USERS)
				// as the server's login(): an empty stored password accepts any
				if (USERS[n].alias === name && (USERS[n].security.password === ""
					|| USERS[n].security.password === password))
					return USERS[n].number;
			return 0;
		},
		get_user: o.get_user || function (n) {
			return USERS.hasOwnProperty(n) ? USERS[n] : null;
		},
		userdata: o.userdata || function (n) {
			return new UserData(S.user_dir(n) + "webmail/", JsonFiles);
		},
		set_cookie: function (value, keep) {
			ev.cookies.push([value, keep]);
		}
	};
	ev.make_ctx = o.make_ctx || counting_factory(ev, ev.config);
	return ev;
}

// An env whose every file-touching hook fails the test
function sealed_env(o)
{
	o = o || {};
	o.make_ctx = fail("make_ctx");
	o.userdata = fail("userdata");
	o.get_user = fail("get_user");
	o.login = fail("login");
	o.find_user = fail("find_user");
	o.webv4_fn = fail("webv4_user");
	return make_env(o);
}

function call(ev, method, args, id)
{
	var r = Api.dispatch(ev, { calls: [[method, args, id === undefined ? "c" : id]] });
	Kit.assert(r.results instanceof Array && r.results.length === 1,
		method + ": one result: " + JSON.stringify(r));
	return r.results[0][1];
}

// name: the product name expected in the "https" text
function err(result, code, what, name)
{
	var text = STRINGS["errors." + code];
	if (name !== undefined)
		text = text.split("{name}").join(name);
	Kit.eq(result.ok, false, (what || "") + " ok false: " + JSON.stringify(result));
	Kit.eq(result.error, code, (what || "") + " error code");
	Kit.eq(result.message, text, (what || "") + " localized message");
}

function save(hdr, body)
{
	if (hdr.from === undefined)
		hdr.from = "Sender";
	if (hdr.to === undefined)
		hdr.to = "Rcpt";
	if (!shared.save_msg(hdr, body === undefined ? "body\r\n" : body))
		throw new Error("save_msg: " + shared.last_error);
	return shared.last_msg;
}

// A signed-in cookie for usernum, made through a password sign-in
function sign_in(usernum, keep)
{
	var ev = make_env();
	var u = USERS[usernum];
	var r = call(ev, "session.login", { user: u.alias, password: u.security.password,
		keep: keep === true });
	Kit.eq(r.ok, true, "sign in " + usernum + ": " + JSON.stringify(r));
	Kit.eq(ev.cookies.length, 1, "one cookie set");
	return ev.cookies[0][0];
}

function session_file(usernum)
{
	return S.user_dir(usernum) + "webmail/webmail.sessions.json";
}

function read_json(path)
{
	var f = new File(path);
	if (!f.open("r"))
		return null;
	var t = f.read();
	f.close();
	return JSON.parse(t);
}

// Every file under the scratch tree, with size and date
function snapshot()
{
	var out = [];
	(function walk(dir) {
		var list = directory(dir + "*");
		for (var i = 0; i < list.length; i++) {
			var p = list[i];
			var c = p.charAt(p.length - 1);
			if (c == '/' || c == '\\')
				walk(p);
			else
				out.push(p + ":" + file_size(p) + ":" + file_date(p));
		}
	})(S.dir);
	return out.sort().join("\n");
}

var real_log = Api.log;
var logged = [];
Api.log = function (level, text) {
	logged.push(text);
};

try {
	shared = Kit.email_base(S.shared_path);
	var A1 = save({ to_ext: String(ALICE), from: "Carol", subject: "Hello Alice" });
	var A2 = save({ to_ext: String(ALICE), from: "Dave", subject: "Second" });
	var B1 = save({ to_ext: String(BOB), from: "Eve", subject: "for bob" });
	shared.close();
	shared = Kit.email_base(S.shared_path);

	var alice = sign_in(ALICE);
	Kit.assert(Session.parse(alice) !== null, "a session cookie value");
	var bob = sign_in(BOB);

	// --- malformed requests: one top-level invalid; nothing touched
	var before = snapshot();
	var bad_requests = [null, undefined, "x", 5, [], {}, { calls: "x" }, { calls: {} },
		{ calls: [5] }, { calls: [[]] }, { calls: [[1, 2, 3, 4]] }, { calls: [null] },
		{ calls: [{ 0: "session.get" }] }];
	var many = [];
	for (var i = 0; i < 33; i++)
		many.push(["session.get", {}, i]);
	bad_requests.push({ calls: many });
	for (i = 0; i < bad_requests.length; i++) {
		var ev = sealed_env({ cookie: alice });
		var r = Api.dispatch(ev, bad_requests[i]);
		Kit.eq(r.error, "invalid", "top-level invalid #" + i);
		Kit.eq(r.message, STRINGS["errors.invalid"], "top-level message #" + i);
		Kit.eq(r.results, [], "no results #" + i);
		Kit.eq(r.state, "", "no state #" + i);
		Kit.eq(ev.cookies, [], "no cookie change #" + i);
	}
	// 32 calls are allowed
	r = Api.dispatch(make_env(), { calls: many.slice(0, 32) });
	Kit.eq(r.results.length, 32, "32 calls");
	Kit.eq(r.error, undefined, "32 calls: no top-level error");
	Kit.eq(snapshot(), before, "no file touched by malformed requests");

	// --- call ids: a string of at most 40 characters or a number; else null
	r = Api.dispatch(make_env(), { calls: [["nonesuch", {}, "abc"], ["nonesuch", {}, 7],
		["nonesuch", {}, new Array(42).join("x")], ["nonesuch", {}, new Array(41).join("x")],
		["nonesuch", {}, {}], ["nonesuch", {}, null], ["nonesuch"], ["nonesuch", {}, -1.5],
		["nonesuch", {}, "caf\xC3\xA9"], ["nonesuch", {}, String.fromCharCode(0x100)],
		["nonesuch", {}, "a\tb"], ["nonesuch", {}, "a\x7f"], ["nonesuch", {}, " ~"]] });
	var ids = [];
	for (i = 0; i < r.results.length; i++)
		ids.push(r.results[i][0]);
	Kit.eq(ids, ["abc", 7, null, new Array(41).join("x"), null, null, null, -1.5, null, null, null, null,
		" ~"], "call ids");

	// --- forbidden: no X-Webmail header; hides https and everything after
	before = snapshot();
	ev = sealed_env({ cookie: alice, has_header: false, secure: false });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1], ["message.list", { folder: "inbox" }, 2],
		["nonesuch", {}, 3], ["message.get", { id: "../x" }, 4]] });
	Kit.eq(r.results.length, 4);
	for (i = 0; i < 4; i++) {
		Kit.eq(r.results[i][0], i + 1, "forbidden: id echoed");
		err(r.results[i][1], "forbidden", "forbidden #" + i);
	}
	Kit.eq(r.state, "", "forbidden: no state");

	// --- https: not secure while require_tls; hides unknown methods and auth
	ev = sealed_env({ cookie: alice, secure: false });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1], ["nonesuch", {}, 2],
		["message.list", { folder: "inbox" }, 3], ["session.login", { user: "alice", password: "x" }, 4]] });
	for (i = 0; i < 4; i++)
		err(r.results[i][1], "https", "https #" + i, cfg.product_name);
	Kit.eq(r.results[0][1].message, "Mail <b> needs a secure connection. Please use the https address of this site.",
		"https: the product name in the text, literally");
	// {system} and {host} are filled in by the caller (api.ssjs); a name
	// holding "$&" or "{name}" is taken literally
	var cfg_odd = Config.load({ directory: WEBMAIL_DIR, product_name: "A $& {name} B" });
	Kit.eq(call(sealed_env({ config: cfg_odd, secure: false }), "session.get", {}).message,
		"A $& {name} B needs a secure connection. Please use the https address of this site.",
		"https: an odd product name taken literally");
	Kit.eq(ev.cookies, [], "https: cookie left alone");
	Kit.eq(snapshot(), before, "no file touched by forbidden or https requests");

	// --- require_tls false: calls proceed, session.get says insecure
	ev = make_env({ config: cfg_plain, secure: false, cookie: alice });
	var g = call(ev, "session.get", {});
	Kit.eq(g.ok, true, "insecure session.get works");
	Kit.eq(g.insecure, true, "insecure:true");
	Kit.eq(g.user.number, ALICE, "signed in");
	g = call(make_env({ cookie: alice }), "session.get", {});
	Kit.eq(g.insecure, false, "secure: insecure false");

	// --- unknown methods: invalid, before the auth check
	ev = sealed_env();
	var unknown = ["nonesuch", "constructor", "__proto__", "toString", "hasOwnProperty",
		"session", "", 5, null];
	for (i = 0; i < unknown.length; i++)
		err(call(ev, unknown[i], {}), "invalid", "unknown method " + JSON.stringify(unknown[i]));

	// --- auth: methods needing a user, without one (no cookie); before
	// argument checks
	ev = sealed_env();
	err(call(ev, "message.list", { folder: "inbox" }), "auth", "unauthenticated message.list");
	err(call(ev, "message.get", { id: "../x" }), "auth", "auth hides a bad argument");
	err(call(ev, "folder.list", {}), "auth");
	err(call(ev, "changes", { state: "x" }), "auth");
	err(call(ev, "session.logout", {}), "auth");

	// --- argument checks (signed in, but no store opened for a bad call)
	ev = make_env({ cookie: alice, make_ctx: fail("make_ctx") });
	var bad_args = [
		["message.list", { folder: "inbox", extra: 1 }, "extra argument"],
		["message.list", {}, "missing folder"],
		["message.list", { folder: 5 }, "folder type"],
		["message.list", { folder: "nonesuch" }, "unknown folder"],
		["message.list", { folder: "u:" }, "empty tag"],
		["message.list", { folder: "u:$Sent" }, "reserved tag"],
		["message.list", { folder: "u:a b" }, "white space in a tag"],
		["message.list", { folder: "u:a\tb" }, "tab in a tag"],
		["message.list", { folder: "u:" + new Array(202).join("x") }, "201-byte tag"],
		["message.list", { folder: "INBOX" }, "case"],
		["message.list", { folder: "inbox", offset: -1 }, "negative offset"],
		["message.list", { folder: "inbox", offset: 1.5 }, "fractional offset"],
		["message.list", { folder: "inbox", offset: 10000001 }, "offset too large"],
		["message.list", { folder: "inbox", offset: "1" }, "offset as a string"],
		["message.list", { folder: "inbox", limit: 0 }, "limit 0"],
		["message.list", { folder: "inbox", limit: 101 }, "limit 101"],
		["message.list", { folder: "inbox", search: new Array(1002).join("x") }, "1001-char search"],
		["message.list", { folder: "inbox", search: String.fromCharCode(0x100) }, "a character above 0xFF"],
		["message.list", { folder: "inbox", search: null }, "null search"],
		["message.list", [ "inbox" ], "arguments as an array"],
		["message.list", "inbox", "arguments as a string"],
		["message.get", { id: "s1; rm" }, "id with a command"],
		["message.get", { id: "../x" }, "id with a path"],
		["message.get", { id: "s0" }, "id s0"],
		["message.get", { id: "s01" }, "id with a leading zero"],
		["message.get", { id: "x1" }, "id kind"],
		["message.get", { id: "s12345678901" }, "id too long"],
		["message.get", { id: "s1", mark_read: 1 }, "mark_read not a boolean"],
		["message.get", { id: 1 }, "id a number"],
		["changes", {}, "missing state"],
		["changes", { state: new Array(1002).join("x") }, "state too long"],
		["folder.list", { x: 1 }, "folder.list takes no arguments"],
		["session.logout", { all: true }, "logout takes no arguments"]
	];
	for (i = 0; i < bad_args.length; i++)
		err(call(ev, bad_args[i][0], bad_args[i][1]), "invalid", bad_args[i][2]);
	Kit.eq(ev.made, 0, "no context made for invalid calls");
	// Unauthenticated methods check their arguments too
	err(call(sealed_env(), "session.get", { x: 1 }), "invalid", "session.get extra");
	err(call(sealed_env(), "session.login", { user: "a", password: "b", keep: "yes" }), "invalid",
		"keep must be a boolean");
	err(call(sealed_env(), "session.login", { user: 5, password: "b" }), "invalid", "user type");

	// --- valid edge arguments reach the store
	ev = make_env({ cookie: alice });
	r = Api.dispatch(ev, { calls: [
		["message.list", { folder: "inbox", offset: 10000000, limit: 100 }, 1],
		["message.list", { folder: "inbox", limit: 1, search: "" }, 2],
		["message.list", { folder: "u:" + new Array(201).join("x") }, 3],
		["message.list", { folder: "u:Caf\xC3\xA0" }, 4],
		["message.list", { folder: "all" }, 5],
		["message.list", { folder: "all", search: "hello" }, 6]] });
	Kit.eq(r.results[0][1], { ok: true, total: 2, items: [] }, "offset at the maximum");
	Kit.eq(r.results[1][1].items.length, 1, "limit 1");
	err(r.results[2][1], "notfound", "a 200-byte tag passes validation; no such folder");
	err(r.results[3][1], "notfound", "a UTF-8 tag with byte 0xA0 passes validation");
	err(r.results[4][1], "invalid", "all without a search: the store refuses");
	Kit.eq(r.results[5][1].total, 1, "all with a search");
	Kit.eq(ev.made, 1, "one context for six calls");
	Kit.eq(ev.closed, 1, "closed once");

	// --- a batch of three, in order, one context
	ev = make_env({ cookie: alice });
	r = Api.dispatch(ev, { calls: [["session.get", {}, "a"], ["folder.list", {}, "b"],
		["message.list", { folder: "inbox" }, "c"]] });
	Kit.eq(r.results.length, 3);
	Kit.eq([r.results[0][0], r.results[1][0], r.results[2][0]], ["a", "b", "c"], "order");
	Kit.eq(r.results[0][1].user, { number: ALICE, alias: "alice", name: "Alice A", avatar: null },
		"session.get user");
	var fl = r.results[1][1];
	Kit.eq(fl.ok, true, "folder.list ok");
	Kit.eq(fl.folders[0].id, "inbox", "folders");
	Kit.eq(fl.folders[0].total, 2, "inbox total");
	Kit.eq(fl.usage, { bytes: 0, quota: system.mailbox_quota }, "usage");
	Kit.eq(fl.purged, 0, "no private base: nothing purged");
	Kit.eq(fl.purge, undefined, "no purge object");
	Kit.eq(fl.trash_days, system.max_trash_age, "trash_days: the system's Trash age");
	Kit.eq(fl.inbox_limit, null, "inbox_limit: a user object without mail counts");

	// --- folder.list inbox_limit: the mail server's count (not the Inbox
	// folder's total, which is 2 here) against Max Messages Waiting
	var limit_of = function (waiting, limit, exemptions) {
		var saved_stats = USERS[ALICE].stats;
		var saved_exempt = USERS[ALICE].security.exemptions;
		USERS[ALICE].stats = { mail_waiting: waiting };
		USERS[ALICE].security.exemptions = exemptions || 0;
		try {
			var lev = make_env({ cookie: alice });
			if (limit !== undefined)
				lev.max_msgs_waiting = limit;
			var lr = Api.dispatch(lev, { calls: [["folder.list", {}, 1]] }).results[0][1];
			Kit.eq(lr.ok, true, "folder.list ok");
			Kit.eq(lr.folders[0].total, 2, "the Inbox folder's own total");
			return lr.inbox_limit;
		} finally {
			USERS[ALICE].stats = saved_stats;
			USERS[ALICE].security.exemptions = saved_exempt;
		}
	};
	Kit.eq(limit_of(79, 100), { count: 79, limit: 100, level: "ok" }, "under 80%: nothing to show");
	Kit.eq(limit_of(80, 100), { count: 80, limit: 100, level: "near" }, "at 80%: near");
	Kit.eq(limit_of(100, 100), { count: 100, limit: 100, level: "near" }, "at the limit: mail still accepted");
	Kit.eq(limit_of(101, 100), { count: 101, limit: 100, level: "over" }, "over the limit: mail refused");
	Kit.eq(limit_of(3, 3).level, "near", "a small limit, at it");
	Kit.eq(limit_of(4, 3).level, "over", "a small limit, over it");
	Kit.eq(limit_of(500, 0), null, "limit 0: no limit");
	Kit.eq(limit_of(500, 100, U.UEXEMPT_MAIL_WAITING), null, "the W exemption: not limited");
	Kit.eq(limit_of(500, 100, U.UEXEMPT_MAIL_WAITING >> 1).level, "over", "another exemption flag does not exempt");
	Kit.eq(limit_of(0, undefined), system.max_msgs_waiting > 0
		? { count: 0, limit: system.max_msgs_waiting, level: "ok" } : null, "the system's limit by default");
	var items = r.results[2][1].items;
	Kit.eq(items.length, 2, "two messages");
	Kit.eq(items[0].id, "s" + A2, "newest first");
	Kit.eq(ev.made, 1, "make_ctx once per request");
	Kit.eq(ev.closed, 1, "ctx closed once");
	// the top-level state is the store's
	var st = new Store({ usernum: ALICE, shared_path: S.shared_path, data_dir: S.data_dir });
	st.open();
	Kit.eq(r.state, st.state(), "top-level state is the store's state");
	st.close();
	// not signed in: ""
	r = Api.dispatch(make_env(), { calls: [["session.get", {}, 1]] });
	Kit.eq(r.state, "", "no user: empty state");
	Kit.eq(r.results[0][1].user, null, "no user");

	// --- the other user's message is not reachable
	err(call(make_env({ cookie: alice }), "message.get", { id: "s" + B1 }), "forbidden",
		"another user's message");
	err(call(make_env({ cookie: alice }), "message.get", { id: "s999" }), "notfound");

	// --- message.get
	var m = call(make_env({ cookie: alice }), "message.get", { id: "s" + A1, mark_read: true });
	Kit.eq(m.ok, true, "message.get ok");
	Kit.eq(m.message.subject, "Hello Alice");
	Kit.eq(m.message.read, true, "marked read");
	// head: mark read and return only the list item (no headers, bodies,
	// parts or avatar; the avatar library is not touched)
	var avatar_reads = 0;
	var counting_avatars = { options: {}, read: function () {
		avatar_reads++;
		return null;
	}, is_enabled: function () { return false; }, is_valid: function () { return false; } };
	var hd = call(make_env({ cookie: alice, avatar_lib: counting_avatars }), "message.get",
		{ id: "s" + A2, mark_read: true, head: true });
	Kit.eq(hd.ok, true, "head: ok " + JSON.stringify(hd));
	Kit.eq(hd.message, undefined, "head: no message");
	Kit.eq(hd.item.id, "s" + A2, "head: the item");
	Kit.eq(hd.item.subject, "Second", "head: the item's subject");
	Kit.eq(hd.item.read, true, "head: marked read");
	Kit.eq([hd.item.headers, hd.item.text, hd.item.html, hd.item.parts, hd.item.avatar],
		[undefined, undefined, undefined, undefined, undefined], "head: nothing but the item");
	Kit.eq(avatar_reads, 0, "head: no avatar lookup");
	// without avatar: true no avatar is computed, and none is in the reply
	var plain = call(make_env({ cookie: alice, avatar_lib: counting_avatars }), "message.get",
		{ id: "s" + A2, mark_read: false });
	Kit.eq([plain.ok, plain.message.avatar, avatar_reads], [true, undefined, 0], "not asked: no avatar lookup");
	err(call(make_env({ cookie: alice }), "message.get", { id: "s" + A2, avatar: 1 }), "invalid",
		"avatar must be a boolean");
	err(call(make_env({ cookie: alice }), "message.get", { id: "s" + A2, head: "yes" }), "invalid",
		"head must be a boolean");

	// --- changes: false for the current state, true after new mail
	ev = make_env({ cookie: alice });
	r = Api.dispatch(ev, { calls: [["changes", { state: "" }, 1]] });
	var cur = r.results[0][1].state;
	Kit.eq(r.results[0][1].changed, true, "empty state differs");
	Kit.eq(r.state, cur, "top-level state equals changes' state");
	Kit.eq(call(make_env({ cookie: alice }), "changes", { state: cur }),
		{ ok: true, changed: false, state: cur }, "unchanged");
	save({ to_ext: String(BOB), from: "X", subject: "bob again" });
	var c2 = call(make_env({ cookie: alice }), "changes", { state: cur });
	// another user's mail may or may not move the fingerprint; Alice's must
	var A3 = save({ to_ext: String(ALICE), from: "Zed", subject: "new one" });
	c2 = call(make_env({ cookie: alice }), "changes", { state: cur });
	Kit.eq(c2.changed, true, "new mail for this user");
	Kit.assert(c2.state !== cur, "new state");

	// --- UTF-8 subject and sender come back byte for byte through JSON
	var SUBJ = "Caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80 \xE6\x97\xA5\xE6\x9C\xAC";
	var FROM = "J\xC3\xBCrgen M\xC3\xBCller";
	var A4 = save({ to_ext: String(ALICE), from: FROM, subject: SUBJ });
	r = Api.dispatch(make_env({ cookie: alice }), { calls: [["message.list", { folder: "inbox" }, 1]] });
	var back = JSON.parse(JSON.stringify(r));
	var it = back.results[0][1].items[0];
	Kit.eq(it.id, "s" + A4, "the UTF-8 message is newest");
	Kit.assert(it.subject === SUBJ, "subject byte-identical: " + escape(it.subject));
	Kit.assert(it.from === FROM, "from byte-identical: " + escape(it.from));
	for (i = 0; i < it.subject.length; i++)
		Kit.assert(it.subject.charCodeAt(i) <= 0xFF, "byte string");

	// --- a method that throws: internal for that call only; no detail leaks
	Api.methods["test.throw"] = { auth: false, args: {}, run: function () {
		throw new Error("cannot open /secret/path/mail.shd");
	} };
	Api.methods["test.coded"] = { auth: false, args: { code: { type: "string" } },
		run: function (ctx, args) {
			throw { code: args.code, message: "/secret/path detail" };
		} };
	Api.methods["test.string"] = { auth: false, args: {}, run: function () {
		throw "/secret/path as a string";
	} };
	logged = [];
	r = Api.dispatch(make_env(), { calls: [["test.throw", {}, 1], ["session.get", {}, 2],
		["test.coded", { code: "quota" }, 3], ["test.coded", { code: "weird" }, 4],
		["test.coded", { code: "constructor" }, 5], ["test.string", {}, 6],
		["test.coded", { code: "expired" }, 7], ["test.coded", { code: "failed" }, 8]] });
	err(r.results[0][1], "internal", "thrown Error");
	Kit.eq(r.results[1][1].ok, true, "the next call still runs");
	err(r.results[2][1], "quota", "coded error passes with the localized text");
	err(r.results[3][1], "internal", "unknown code");
	err(r.results[4][1], "internal", "inherited-name code");
	err(r.results[5][1], "internal", "thrown string");
	err(r.results[6][1], "expired");
	err(r.results[7][1], "failed");
	Kit.assert(JSON.stringify(r).indexOf("secret") < 0, "no detail in the result");
	var hit = false;
	for (i = 0; i < logged.length; i++)
		if (logged[i].indexOf("/secret/path/mail.shd") >= 0 && logged[i].indexOf("api.js") >= 0)
			hit = true;
	Kit.assert(hit, "the detail and the file name and line are logged: " + JSON.stringify(logged));
	delete Api.methods["test.throw"];
	delete Api.methods["test.coded"];
	delete Api.methods["test.string"];

	// --- make_ctx failing: internal for each call that needs it, made once
	var tries = 0;
	ev = make_env({ cookie: alice, make_ctx: function () {
		tries++;
		throw new Error("open failed at /secret");
	} });
	r = Api.dispatch(ev, { calls: [["folder.list", {}, 1], ["message.list", { folder: "inbox" }, 2],
		["session.get", {}, 3]] });
	err(r.results[0][1], "internal");
	err(r.results[1][1], "internal");
	Kit.eq(r.results[2][1].ok, true, "a call without the store still runs");
	Kit.eq(tries, 1, "make_ctx tried once");
	Kit.eq(r.state, "", "no state without a context");

	// --- session.login refusals: auth, same text for every reason
	var refusals = [
		[{ user: "alice", password: "wrong" }, "wrong password"],
		[{ user: "nobody", password: "x" }, "no such user"],
		[{ user: "nopass", password: "" }, "empty password"],
		[{ user: "", password: "x" }, "empty user"],
		[{ user: new Array(66).join("a"), password: "x" }, "65-character user"],
		[{ user: "alice", password: new Array(130).join("a") }, "129-character password"],
		[{ user: new Array(2000).join("a"), password: "x" }, "very long user"]
	];
	for (i = 0; i < refusals.length; i++) {
		ev = make_env();
		err(call(ev, "session.login", refusals[i][0]), "auth", "refused: " + refusals[i][1]);
		Kit.eq(ev.cookies, [], "no cookie: " + refusals[i][1]);
	}
	// Accounts the library refuses never reach the server's login(), and
	// each refusal is logged once with the user number and the reason
	var refused_accounts = [
		["guest", GUEST, "guest"],
		["gflag", GUEST_FLAG, "guest"],
		["denied", DENIED, "access requirements"],
		["gone", GONE, "deleted"],
		["idle", IDLE, "inactive"],
		["nopass", NOPASS, "no password"]
	];
	for (i = 0; i < refused_accounts.length; i++) {
		ev = make_env({ login: fail("login for " + refused_accounts[i][0]) });
		logged = [];
		err(call(ev, "session.login", { user: refused_accounts[i][0], password: "pw" + refused_accounts[i][1] }),
			"auth", "refused before login(): " + refused_accounts[i][0]);
		Kit.eq(ev.cookies, [], "no cookie: " + refused_accounts[i][0]);
		Kit.eq(logged, ["webmail: user #" + refused_accounts[i][1] + " refused: " + refused_accounts[i][2]],
			"one notice for " + refused_accounts[i][0]);
	}
	// A name the library cannot resolve goes to login(), and the account
	// login() found is still checked afterwards
	ev = make_env({ find_user: function () { return 0; }, login: function () { return GUEST; } });
	logged = [];
	err(call(ev, "session.login", { user: "Guest Real Name", password: "x" }), "auth",
		"refused after login()");
	Kit.eq(logged, ["webmail: user #" + GUEST + " refused: guest"], "logged after login()");
	Kit.eq(ev.cookies, [], "no cookie after the post-check");
	// The shim resolves names with the engine's own lookup, the one login()
	// itself uses
	Kit.eq(typeof system.find_login_id, "function", "system.find_login_id exists");
	Kit.assert(typeof Api.find_login_id === "undefined", "no lookup of the library's own");
	// Long names never reach the server's login()
	ev = make_env();
	call(ev, "session.login", { user: new Array(66).join("a"), password: "x" });
	call(ev, "session.login", { user: "alice", password: new Array(130).join("a") });
	Kit.eq(ev.logins, [], "over-long credentials not passed on");
	// 64 and 128 are allowed through to login()
	ev = make_env();
	call(ev, "session.login", { user: new Array(65).join("a"), password: new Array(129).join("a") });
	Kit.eq(ev.logins.length, 1, "64/128 reach login()");
	// access_requirements is tested with the configured string
	ars_calls = [];
	call(make_env(), "session.login", { user: "denied", password: "pw" + DENIED });
	Kit.eq(ars_calls, [[DENIED, "LEVEL 10"]], "compare_ars called with the requirement");
	ars_calls = [];
	call(make_env({ config: cfg_plain }), "session.login", { user: "bob", password: "pw" + BOB });
	Kit.eq(ars_calls, [], "no requirement: compare_ars not called");
	// login() throwing is internal, not a crash
	err(call(make_env({ login: function () { throw new Error("x"); } }), "session.login",
		{ user: "alice", password: "pw" + ALICE }), "internal");

	// --- session.login success: cookie set, then the session works
	ev = make_env();
	r = Api.dispatch(ev, { calls: [["session.login", { user: "alice", password: "pw" + ALICE, keep: true }, 1],
		["folder.list", {}, 2]] });
	Kit.eq(r.results[0][1], { ok: true, user: { number: ALICE, alias: "alice", name: "Alice A", avatar: null } },
		"login result");
	Kit.eq(r.results[1][1].ok, true, "a call after login in the same batch is signed in");
	Kit.eq(ev.cookies.length, 1, "cookie set once");
	Kit.eq(ev.cookies[0][1], true, "keep passed to the cookie");
	var v = ev.cookies[0][0];
	Kit.eq(Session.parse(v).usernum, ALICE, "a session cookie for alice");
	Kit.assert(r.state !== "", "state after sign-in");
	var sess = read_json(session_file(ALICE));
	var keep_seen = false;
	for (var k in sess)
		if (sess[k].keep === true && sess[k].client.agent === "Test/1.0")
			keep_seen = true;
	Kit.assert(keep_seen, "the session was stored with keep and the client");
	Kit.eq(call(make_env({ cookie: v }), "session.get", {}).user.number, ALICE, "cookie works");
	// The result never carries a password or key
	Kit.assert(JSON.stringify(r).indexOf("pw" + ALICE) < 0, "no password in the result");
	Kit.assert(JSON.stringify(r).indexOf(Session.parse(v).key) < 0, "no key in the result");
	// Signing in again replaces the session the browser had
	ev = make_env({ cookie: v });
	call(ev, "session.login", { user: "bob", password: "pw" + BOB });
	Kit.eq(Session.check(new UserData(S.user_dir(ALICE) + "webmail/", JsonFiles), v, time(), cfg), null,
		"the previous session is gone");
	Kit.eq(Session.parse(ev.cookies[0][0]).usernum, BOB, "new cookie for bob");

	// --- invalid cookies: not signed in, cookie cleared
	var bad_cookies = ["junk", ALICE + "." + new Array(41).join("a"), "1.x"];
	for (i = 0; i < bad_cookies.length; i++) {
		ev = make_env({ cookie: bad_cookies[i] });
		g = call(ev, "session.get", {});
		Kit.eq(g.user, null, "bad cookie not signed in");
		Kit.eq(ev.cookies, [["", false]], "bad cookie cleared");
	}
	ev = make_env({ cookie: "" });
	call(ev, "session.get", {});
	Kit.eq(ev.cookies, [], "no cookie: nothing cleared");

	// --- a session whose user is no longer eligible is destroyed
	var tmp = sign_in(BOB);
	USERS[BOB].security.restrictions = U.UFLAG_G;
	ev = make_env({ cookie: tmp });
	Kit.eq(call(ev, "session.get", {}).user, null, "now a guest: not signed in");
	Kit.eq(ev.cookies, [["", false]], "cookie cleared");
	USERS[BOB].security.restrictions = 0;
	Kit.eq(call(make_env({ cookie: tmp }), "session.get", {}).user, null, "the session was destroyed");
	USERS[BOB].settings = U.USER_DELETED;
	tmp = (function () {
		USERS[BOB].settings = 0;
		var t = sign_in(BOB);
		USERS[BOB].settings = U.USER_DELETED;
		return t;
	})();
	Kit.eq(call(make_env({ cookie: tmp }), "session.get", {}).user, null, "deleted: not signed in");
	USERS[BOB].settings = 0;
	var saved_bob = USERS[BOB];
	tmp = sign_in(BOB);
	delete USERS[BOB];
	Kit.eq(call(make_env({ cookie: tmp }), "session.get", {}).user, null, "no user record");
	USERS[BOB] = saved_bob;

	// --- touch: a request of only "changes" does not refresh the session
	var T0 = time();
	ev = make_env({ now: T0 });
	call(ev, "session.login", { user: "bob", password: "pw" + BOB });
	var bv = ev.cookies[0][0];
	function seen_of(value)
	{
		var all = read_json(session_file(BOB));
		var h = sha256_calc(Session.parse(value).key, true);
		return all[h].seen;
	}
	Kit.eq(seen_of(bv), T0);
	call(make_env({ cookie: bv, now: T0 + 120 }), "changes", { state: "" });
	Kit.eq(seen_of(bv), T0, "changes alone: seen not refreshed");
	Api.dispatch(make_env({ cookie: bv, now: T0 + 120 }), { calls: [["changes", { state: "" }, 1],
		["changes", { state: "" }, 2]] });
	Kit.eq(seen_of(bv), T0, "two changes: seen not refreshed");
	Api.dispatch(make_env({ cookie: bv, now: T0 + 120 }), { calls: [["changes", { state: "" }, 1],
		["session.get", {}, 2]] });
	Kit.eq(seen_of(bv), T0 + 120, "any other call refreshes seen");
	// background: true (an automatic refresh) never refreshes it either
	r = Api.dispatch(make_env({ cookie: bv, now: T0 + 240 }), { background: true,
		calls: [["folder.list", {}, 1], ["message.list", { folder: "inbox" }, 2]] });
	Kit.eq([r.results[0][1].ok, r.results[1][1].ok], [true, true], "background request answered");
	Kit.eq(seen_of(bv), T0 + 120, "background: seen not refreshed");
	// only true withholds the touch; any other value is no flag
	Api.dispatch(make_env({ cookie: bv, now: T0 + 300 }), { background: "yes", calls: [["folder.list", {}, 1]] });
	Kit.eq(seen_of(bv), T0 + 300, "background other than true: seen refreshed");
	Api.dispatch(make_env({ cookie: bv, now: T0 + 360 }), { background: false, calls: [["folder.list", {}, 1]] });
	Kit.eq(seen_of(bv), T0 + 360, "background false: seen refreshed");
	Api.dispatch(make_env({ cookie: bv, now: T0 + 420 }), { calls: [["folder.list", {}, 1]] });
	Kit.eq(seen_of(bv), T0 + 420, "without the flag: seen refreshed");

	// --- session.logout
	tmp = sign_in(BOB);
	ev = make_env({ cookie: tmp });
	r = Api.dispatch(ev, { calls: [["session.logout", {}, 1], ["folder.list", {}, 2]] });
	Kit.eq(r.results[0][1], { ok: true, webv4: false }, "logout");
	err(r.results[1][1], "auth", "signed out for the rest of the batch");
	Kit.eq(ev.cookies, [["", false]], "cookie cleared");
	Kit.eq(ev.made, 0, "logout opens no store");
	Kit.eq(r.state, "", "no state after logout");
	Kit.eq(call(make_env({ cookie: tmp }), "session.get", {}).user, null, "the session is gone");
	// Logging out while the webv4 session is still valid says so
	tmp = sign_in(BOB);
	ev = make_env({ cookie: tmp, config: cfg_webv4, webv4_user: BOB });
	r = call(ev, "session.logout", {});
	Kit.eq(r, { ok: true, webv4: true }, "logout: still signed in to webv4");
	Kit.eq(ev.webv4_calls, 1, "the webv4 cookie is read once, at logout");
	tmp = sign_in(BOB);
	r = call(make_env({ cookie: tmp, webv4_user: BOB }), "session.logout", {});
	Kit.eq(r.webv4, false, "webv4_login off: false");

	// --- webv4 sign-in through session.get
	ev = make_env({ config: cfg_webv4, webv4_user: ALICE });
	g = call(ev, "session.get", {});
	Kit.eq(g.user, { number: ALICE, alias: "alice", name: "Alice A", avatar: null }, "signed in from webv4");
	Kit.eq(g.via, "webv4", "via webv4");
	Kit.eq(ev.cookies.length, 1, "a web client cookie is set");
	Kit.eq(ev.cookies[0][1], false, "not kept");
	var wv = ev.cookies[0][0];
	g = call(make_env({ cookie: wv }), "session.get", {});
	Kit.eq(g.user.number, ALICE, "the new session works on its own");
	Kit.eq(g.via, undefined, "an existing session is not via webv4");
	// an existing web client session wins over the webv4 cookie
	ev = make_env({ config: cfg_webv4, webv4_user: BOB, cookie: wv });
	g = call(ev, "session.get", {});
	Kit.eq(g.user.number, ALICE, "own session first");
	Kit.eq(ev.cookies, [], "no new cookie");
	// webv4_login false: ignored
	ev = make_env({ webv4_user: ALICE });
	g = call(ev, "session.get", {});
	Kit.eq(g.user, null, "webv4_login off");
	Kit.eq(ev.cookies, [], "no cookie");
	// refused users are not signed in this way
	var refused = [GUEST, GUEST_FLAG, DENIED, GONE, IDLE, 99999];
	for (i = 0; i < refused.length; i++) {
		ev = make_env({ config: cfg_webv4, webv4_user: refused[i] });
		g = call(ev, "session.get", {});
		Kit.eq(g.user, null, "webv4 user refused: " + refused[i]);
		Kit.eq(ev.cookies, [], "no cookie for " + refused[i]);
	}
	// the webv4 session file is read only where it is needed
	ev = make_env({ config: cfg_webv4, webv4_user: ALICE, cookie: alice });
	Api.dispatch(ev, { calls: [["folder.list", {}, 1], ["changes", { state: "" }, 2],
		["message.list", { folder: "inbox" }, 3], ["session.get", {}, 4]] });
	Kit.eq(ev.webv4_calls, 0, "not read when a web client session exists");
	ev = make_env({ config: cfg_webv4, webv4_user: ALICE });
	Api.dispatch(ev, { calls: [["folder.list", {}, 1], ["changes", { state: "" }, 2]] });
	Kit.eq(ev.webv4_calls, 0, "not read by calls that cannot sign in");
	ev = make_env({ webv4_user: ALICE });
	call(ev, "session.get", {});
	Kit.eq(ev.webv4_calls, 0, "not read when webv4_login is off");
	ev = make_env({ config: cfg_webv4, webv4_user: 0 });
	Api.dispatch(ev, { calls: [["session.get", {}, 1], ["session.get", {}, 2]] });
	Kit.eq(ev.webv4_calls, 1, "read at most once per request");
	// only session.get signs in from webv4
	err(call(make_env({ config: cfg_webv4, webv4_user: ALICE }), "folder.list", {}), "auth",
		"folder.list does not sign in from webv4");
	// a webv4 user plus a failing make_ctx etc: the gates still apply
	err(call(sealed_env({ config: cfg_webv4, webv4_user: ALICE, has_header: false }), "session.get", {}),
		"forbidden", "webv4 path: header still required");
	err(call(sealed_env({ config: cfg_webv4, webv4_user: ALICE, secure: false }), "session.get", {}),
		"https", "webv4 path: TLS still required", cfg_webv4.product_name);
	// A sign-out is not undone in the same request: logout then get
	// reports no user, clears the cookie once and stores no session
	tmp = sign_in(BOB);
	var bob_before = Object.keys(read_json(session_file(BOB))).length;
	ev = make_env({ cookie: tmp, config: cfg_webv4, webv4_user: BOB });
	r = Api.dispatch(ev, { calls: [["session.logout", {}, 1], ["session.get", {}, 2]] });
	Kit.eq(r.results[0][1], { ok: true, webv4: true }, "logout + get: logout");
	Kit.eq(r.results[1][1].user, null, "logout + get: no user");
	Kit.eq(r.results[1][1].via, undefined, "logout + get: not via webv4");
	Kit.eq(ev.cookies, [["", false]], "logout + get: one cookie clear");
	Kit.eq(r.signed_out, false, "logout + get: not reported as signed out");
	Kit.eq(Object.keys(read_json(session_file(BOB))).length, bob_before - 1,
		"logout + get: the session removed, none stored");
	// An expired or forged web client session with a valid webv4 session
	// signs in again
	ev = make_env({ cookie: tmp, config: cfg_webv4, webv4_user: BOB });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1]] });
	Kit.eq(r.results[0][1].user.number, BOB, "ended session + webv4: signed in again");
	Kit.eq(r.results[0][1].via, "webv4", "ended session + webv4: via webv4");
	Kit.eq(r.signed_out, false, "ended session + webv4: not signed out");
	Kit.eq(ev.cookies.length, 2, "ended session + webv4: cleared, then set");
	Kit.eq(ev.cookies[1][0] !== "", true, "ended session + webv4: the new cookie is last");
	// no_webv4: the browser signed out here; no webv4 sign-in for this call
	ev = make_env({ config: cfg_webv4, webv4_user: ALICE });
	g = call(ev, "session.get", { no_webv4: true });
	Kit.eq(g.user, null, "no_webv4: no user");
	Kit.eq(ev.cookies, [], "no_webv4: no cookie");
	Kit.eq(ev.webv4_calls, 0, "no_webv4: the webv4 session is not read");
	g = call(make_env({ config: cfg_webv4, webv4_user: ALICE }), "session.get", { no_webv4: false });
	Kit.eq(g.via, "webv4", "no_webv4 false: signs in");
	g = call(make_env({ config: cfg_webv4, webv4_user: BOB, cookie: wv }), "session.get", { no_webv4: true });
	Kit.eq(g.user.number, ALICE, "no_webv4 leaves an existing session alone");
	err(call(sealed_env(), "session.get", { no_webv4: 1 }), "invalid", "no_webv4 must be a boolean");

	// --- session.get contents
	ev = make_env({ cookie: alice });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1], ["session.get", {}, 2]] });
	g = r.results[0][1];
	Kit.eq(g.domains, Kit.wlib("address").domains_for(ALICE), "domains: Address.domains_for of the user");
	ev = make_env({ cookie: alice });
	ev.lib_ctx = { domains: ["vert.example", "family.example"] };
	Kit.eq(call(ev, "session.get", {}).domains, ["vert.example", "family.example"],
		"domains: the same source as settings.get's from_domains");
	Kit.eq(call(make_env(), "session.get", {}).domains, [], "domains: none when signed out");
	Kit.eq(g.limits, { max_attachment_bytes: cfg.max_attachment_bytes,
		max_text_part_bytes: cfg.max_text_part_bytes, max_drafts: 7,
		max_draft_bytes: cfg.max_draft_bytes, max_body_bytes: cfg.max_body_bytes,
		max_attachments: 100, max_chunk: 1500000, max_recipients: cfg.max_recipients,
		max_message_bytes: cfg.max_message_bytes,
		max_attach_total: Math.min(cfg.max_attachment_bytes, Kit.wlib("config").attachment_allowance(cfg.max_message_bytes)) },
		"limits");
	Kit.eq(g.poll_ms, 30000, "poll_ms");
	Kit.eq([g.product_name, g.brand, g.login_text, g.footer_links, g.fallback_url],
		["Mail <b>", "<i>B</i>", "Hello", "<a href=x>x</a>", "/old"], "brand keys as given");
	Kit.eq(g.powered_by, Api.powered_by(system.version_notice), "powered_by: the system's version notice");
	Kit.assert(/Synchronet/.test(g.powered_by) && !/\s\s/.test(g.powered_by) && g.powered_by === g.powered_by.trim(),
		"powered_by: runs of spaces collapsed: " + g.powered_by);
	Kit.eq(Api.powered_by("Synchronet BBS for Linux  Version 3.22"), "Synchronet BBS for Linux Version 3.22",
		"powered_by: two spaces");
	Kit.eq(Api.powered_by(" A\t B \r\n"), "A B", "powered_by: tabs, line breaks, ends");
	Kit.eq(Api.powered_by(undefined), "", "powered_by: none");
	Kit.eq(call(make_env(), "session.get", {}).powered_by, g.powered_by, "powered_by also when signed out");
	Kit.eq(g.locale, "en_us");
	Kit.eq(g.strings["errors.auth"], STRINGS["errors.auth"], "strings");
	Kit.eq(r.results[1][1].strings, undefined, "strings once per request");
	Kit.eq(r.results[1][1].locale, "en_us", "the second still names the locale");
	var text = JSON.stringify(r);
	Kit.assert(text.indexOf("pw" + ALICE) < 0, "no password");
	Kit.assert(text.indexOf(Session.parse(alice).key) < 0, "no session key");
	Kit.assert(text.indexOf("access_requirements") < 0 && text.indexOf("LEVEL 10") < 0,
		"no access requirement");
	Kit.assert(text.indexOf("tls_proxy_header") < 0 && text.indexOf("directory") < 0,
		"no security settings");
	Kit.eq(Api.sending_domains(1), Kit.wlib("address").domains_for(1), "sending_domains: Address.domains_for");

	// --- localized errors: the request's locale
	var LS = Kit.scratch();
	try {
		mkpath(LS.dir + "lib/locale");
		mkpath(LS.dir + "mods/locale");
		var src = new File(WEBMAIL_DIR + "lib/locale/en_us.ini");
		src.open("rb");
		var en = src.read();
		src.close();
		var f = new File(LS.dir + "lib/locale/en_us.ini");
		f.open("wb");
		f.write(en);
		f.close();
		f = new File(LS.dir + "lib/locale/fr_fr.ini");
		f.open("wb");
		f.write("[errors]\nauth = Refus\xC3\xA9\ninvalid = Invalide\n");
		f.close();
		var cfg_fr = Config.load({ directory: LS.dir });
		ev = make_env({ config: cfg_fr, accept_language: "fr-FR, en;q=0.5" });
		r = Api.dispatch(ev, { calls: [["nonesuch", {}, 1], ["folder.list", {}, 2],
			["session.get", {}, 3]] });
		Kit.eq(r.results[0][1].message, "Invalide", "localized invalid");
		Kit.eq(r.results[1][1].message, "Refus\xC3\xA9", "localized auth");
		Kit.eq(r.results[2][1].locale, "fr_fr");
		Kit.eq(r.results[2][1].strings["errors.auth"], "Refus\xC3\xA9");
		Kit.eq(r.results[2][1].strings["errors.notfound"], STRINGS["errors.notfound"], "English fallback");
		r = Api.dispatch(make_env({ config: cfg_fr, accept_language: "fr" }), null);
		Kit.eq(r.message, "Invalide", "top-level error localized");
		// the signed-in user's BBS language comes first
		USERS[ALICE].lang = "fr";
		g = call(make_env({ config: cfg_fr, cookie: alice, accept_language: "en" }), "session.get", {});
		Kit.eq(g.locale, "fr_fr", "user's language");
		USERS[ALICE].lang = "";
		// no locale files at all: a built-in English text, never an exception
		var cfg_none = Config.load({ directory: LS.dir + "nonesuch/" });
		r = Api.dispatch(make_env({ config: cfg_none }), { calls: [["nonesuch", {}, 1]] });
		Kit.eq(r.results[0][1].error, "invalid", "no locale files: still a result");
		Kit.assert(typeof r.results[0][1].message === "string" && r.results[0][1].message !== "",
			"a fallback text");
		Kit.assert(r.results[0][1].message.indexOf(LS.dir) < 0, "no path in the fallback");
		g = call(make_env({ config: cfg_none }), "session.get", {});
		Kit.eq(g.ok, false, "session.get without strings fails");
		Kit.eq(g.error, "internal", "session.get without strings: internal");
	} finally {
		LS.cleanup();
	}

	// --- folder.list after a private base exists: purge runs once a day
	ev = make_env({ cookie: alice });
	r = Api.dispatch(ev, { calls: [["message.list", { folder: "inbox" }, 1]] });
	var st2 = new Store({ usernum: ALICE, shared_path: S.shared_path, data_dir: S.data_dir });
	st2.open();
	st2.move(["s" + A3], "archive");
	st2.close();
	fl = call(make_env({ cookie: alice }), "folder.list", {});
	Kit.eq(fl.purged, 0, "purge ran, nothing due");
	Kit.assert(fl.usage.bytes > 0, "usage counts the private base");
	fl = call(make_env({ cookie: alice }), "folder.list", {});
	Kit.eq(fl.purged, 0, "not twice a day");

	// --- request text: raw UTF-8 and \u escapes give the same byte strings
	function body_of(search)
	{
		return '{"calls":[["message.list",{"folder":"inbox","search":"' + search + '"},"x"]]}';
	}
	function search_of(text)
	{
		var req = Api.parse_body(text);
		Kit.assert(req !== null, "parsed: " + escape(text));
		return req.calls[0][1].search;
	}
	var forms = [
		["\xC3\xA9", "\\u00e9", "e-acute"],
		["\xE2\x82\xAC", "\\u20AC", "euro sign"],
		["\xF0\x9F\x98\x80", "\\ud83d\\ude00", "emoji (a surrogate pair)"],
		["a\xC3\xA9b", "a\\u00E9b", "mixed with ASCII"]
	];
	for (i = 0; i < forms.length; i++) {
		var raw_form = search_of(body_of(forms[i][0]));
		var esc_form = search_of(body_of(forms[i][1]));
		Kit.assert(raw_form === forms[i][0], forms[i][2] + ": raw form kept as bytes: " + escape(raw_form));
		Kit.assert(esc_form === forms[i][0], forms[i][2] + ": escaped form gives the same bytes: "
			+ escape(esc_form));
	}
	// keys are converted too, and ASCII bodies pass unchanged
	var kreq = Api.parse_body('{"calls":[["x",{"\\u00e9":1, "a":"plain"},1]]}');
	Kit.assert(kreq.calls[0][1].hasOwnProperty("\xC3\xA9"), "an escaped key becomes UTF-8 bytes");
	Kit.eq(kreq.calls[0][1].a, "plain");
	var bad_bodies = [
		[body_of("\\ud83d"), "a lone high surrogate"],
		[body_of("\\ude00x"), "a lone low surrogate"],
		[body_of("\\ude00\\ud83d"), "surrogates in the wrong order"],
		[body_of("\xC3"), "a truncated sequence"],
		[body_of("\xFF"), "an invalid byte"],
		[body_of("\xC0\xAF"), "an overlong slash"],
		[body_of("\xE0\x80\x80"), "an overlong NUL"],
		[body_of("\xED\xA0\x80"), "an encoded surrogate"],
		[body_of("\xF4\x90\x80\x80"), "above U+10FFFF"],
		["{x", "not JSON"],
		["", "empty"],
		[null, "not a string"],
		[new Array(100).join("[") + new Array(100).join("]"), "nested too deep"]
	];
	for (i = 0; i < bad_bodies.length; i++)
		Kit.eq(Api.parse_body(bad_bodies[i][0]), null, "refused: " + bad_bodies[i][1]);
	r = Api.dispatch(make_env(), Api.parse_body(body_of("\\ud83d")));
	Kit.eq(r.error, "invalid", "a refused body is the top-level invalid");
	// a large non-ASCII body completes in linear time (measured: about
	// 15 ms; the bound only catches a quadratic conversion)
	var chunk = new Array(1001).join("\xC3\xA9\xE2\x82\xAC");	// 5 KB
	var big_search = "";
	for (i = 0; i < 40; i++)
		big_search += chunk;	// 200 KB
	var t0 = Date.now();
	Kit.assert(search_of(body_of(big_search)) === big_search, "200 KB body converted");
	var big_ms = Date.now() - t0;
	Kit.assert(big_ms < 10000, "200 KB body in " + big_ms + " ms");

	// --- an argument named __proto__ is an unknown argument
	r = Api.dispatch(make_env({ cookie: alice, make_ctx: fail("make_ctx") }),
		Api.parse_body('{"calls":[["message.list",{"folder":"inbox","__proto__":{"x":1}},1],'
		+ '["session.get",{"__proto__":null},2]]}'));
	err(r.results[0][1], "invalid", "__proto__ argument");
	err(r.results[1][1], "invalid", "__proto__ argument of a method with none");
	var pobj = { folder: "inbox" };
	Object.defineProperty(pobj, "__proto__", { value: 1, enumerable: true, writable: true,
		configurable: true });
	err(call(make_env({ cookie: alice, make_ctx: fail("make_ctx") }), "message.list", pobj), "invalid",
		"own __proto__ property");

	// --- several cookie values: the first valid one is used
	ev = make_env({ cookie: ["junk", ALICE + "." + new Array(41).join("a"), alice] });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1]] });
	Kit.eq(r.results[0][1].user.number, ALICE, "a later valid value signs in");
	Kit.eq(ev.cookies, [], "not cleared when one value is valid");
	Kit.eq(r.signed_out, false, "not signed out");
	ev = make_env({ cookie: ["junk", ALICE + "." + new Array(41).join("a")] });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1], ["folder.list", {}, 2]] });
	Kit.eq(r.results[0][1].user, null, "no valid value");
	err(r.results[1][1], "auth", "auth text unchanged");
	Kit.eq(ev.cookies, [["", false]], "cleared when none is valid");
	Kit.eq(r.signed_out, true, "signed_out when a cookie was sent but none is valid");
	r = Api.dispatch(make_env(), { calls: [["folder.list", {}, 1]] });
	Kit.eq(r.signed_out, false, "no cookie: not signed out, just not signed in");
	r = Api.dispatch(make_env({ cookie: alice }), { calls: [["folder.list", {}, 1]] });
	Kit.eq(r.signed_out, false, "valid cookie");

	// --- a password change ends the user's sessions
	tmp = sign_in(BOB);
	Kit.eq(call(make_env({ cookie: tmp }), "session.get", {}).user.number, BOB, "signed in");
	USERS[BOB].security.password_date++;
	ev = make_env({ cookie: tmp });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1]] });
	Kit.eq(r.results[0][1].user, null, "signed out by the password change");
	Kit.eq(r.signed_out, true, "reported as signed out");
	Kit.eq(ev.cookies, [["", false]], "cookie cleared");
	tmp = sign_in(BOB);
	Kit.eq(call(make_env({ cookie: tmp }), "session.get", {}).user.number, BOB,
		"a new sign-in works with the new password date");
	// the password alone changing (most ways to change it leave the date)
	Kit.eq(call(make_env({ cookie: tmp }), "session.get", {}).user.number, BOB, "nothing changed: kept");
	var stored_bob = read_json(session_file(BOB));
	var stamps = [];
	for (k in stored_bob)
		stamps.push(stored_bob[k].stamp);
	for (i = 0; i < stamps.length; i++) {
		Kit.assert(/^[0-9a-f]{64}$/.test(stamps[i]), "the stamp is a SHA-256 digest: " + stamps[i]);
		Kit.assert(stamps[i].indexOf(USERS[BOB].security.password) < 0, "the stamp holds no password");
	}
	Kit.assert(JSON.stringify(stored_bob).indexOf(USERS[BOB].security.password) < 0,
		"the sessions file holds no password");
	USERS[BOB].security.password = "newpw";
	ev = make_env({ cookie: tmp });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1]] });
	Kit.eq(r.results[0][1].user, null, "a new password with the same date ends the session");
	Kit.eq(r.signed_out, true, "reported as signed out");
	USERS[BOB].security.password = "pw" + BOB;
	tmp = sign_in(BOB);

	// --- forged cookie values never load a user record
	var user_reads = 0;
	// the real key with its first character changed (to "e" when it is "f")
	var real_key = Session.parse(tmp).key;
	var forged_key = (real_key.charAt(0) == "f" ? "e" : "f") + real_key.substr(1);
	Kit.assert(forged_key !== real_key, "the forged key differs");
	ev = make_env({ cookie: ["60001.0123456789abcdef0123456789abcdef01234567",
		"60002." + forged_key, "70000.0123456789abcdef0123456789abcdef01234567"],
		get_user: function (n) {
			user_reads++;
			return USERS.hasOwnProperty(n) ? USERS[n] : null;
		} });
	r = Api.dispatch(ev, { calls: [["changes", { state: "" }, 1]] });
	err(r.results[0][1], "auth", "forged values: not signed in");
	Kit.eq(user_reads, 0, "no user record read for a forged value");
	user_reads = 0;
	ev = make_env({ cookie: tmp, get_user: function (n) {
		user_reads++;
		return USERS.hasOwnProperty(n) ? USERS[n] : null;
	} });
	Kit.eq(call(ev, "session.get", {}).user.number, BOB, "a valid value");
	Kit.eq(user_reads, 1, "the user record is read once, after the key matched");

	// --- signed_out is false when the request ends signed in
	ev = make_env({ cookie: "junk" });
	r = Api.dispatch(ev, { calls: [["session.login", { user: "bob", password: "pw" + BOB }, 1]] });
	Kit.eq(r.results[0][1].ok, true, "signed in");
	Kit.eq(r.signed_out, false, "a sign-in in the same request: not signed out");
	ev = make_env({ cookie: "junk", config: cfg_webv4, webv4_user: ALICE });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1]] });
	Kit.eq(r.results[0][1].via, "webv4", "signed in from webv4");
	Kit.eq(r.signed_out, false, "a webv4 sign-in in the same request: not signed out");

	// --- names are sent as UTF-8
	add_user(60010, { alias: "M\x81ller", name: "J\xC3\xBCrgen" });	// CP437 alias, UTF-8 name
	ev = make_env({ config: cfg_webv4, webv4_user: 60010 });
	g = call(ev, "session.get", {});
	Kit.assert(g.user.alias === "M\xC3\xBCller", "a CP437 alias converted: " + escape(g.user.alias));
	Kit.assert(g.user.name === "J\xC3\xBCrgen", "a UTF-8 name kept: " + escape(g.user.name));

	// --- avatars: the library over a scratch user directory
	var AV_CELL = "A\x1f";
	var av_bin = "";
	for (var ai = 0; ai < 60; ai++)
		av_bin += AV_CELL;
	var AV_DATA = base64_encode(av_bin);
	function avatar_lib(options) {
		var lib = Kit.lib("avatar_lib");
		lib.localuser_fname = function (n) {
			return S.data_dir + "user/" + format("%04u.ini", n);
		};
		lib.options = options || {};
		return lib;
	}
	function write_avatar(n, obj) {
		var f = new File(S.data_dir + "user/" + format("%04u.ini", n));
		Kit.assert(f.open("w+"), "write avatar ini");
		f.iniSetObject("avatar", obj);
		f.close();
	}
	function identicon_of(name) {
		return base64_encode(load({}, "identicon.js").identicon(name).BIN);
	}
	write_avatar(ALICE, { data: AV_DATA, disabled: false });
	var M_BOB = save({ to: "alice", to_ext: String(ALICE), from: "bob", from_ext: String(BOB),
		subject: "from bob (local, no avatar file)" });
	var M_ALICE = save({ to: "alice", to_ext: String(ALICE), from: "alice", from_ext: String(ALICE),
		subject: "from alice herself" });
	var M_NET = save({ to: "alice", to_ext: String(ALICE), from: "Net Person", from_net_type: 5,
		from_net_addr: "net.person@example.com", subject: "from the Internet" });
	var M_ANON = save({ to: "alice", to_ext: String(ALICE), from: "bob", from_ext: String(BOB),
		attr: 1 << 5, subject: "anonymous" });
	function sender_avatar(lib, id) {
		var g = call(make_env({ cookie: alice, avatar_lib: lib }), "message.get", { id: "s" + id, avatar: true });
		Kit.eq(g.ok, true, "message.get with avatars: " + JSON.stringify(g).substr(0, 200));
		// the same through head (marking a kept message read)
		var hg = call(make_env({ cookie: alice, avatar_lib: lib }), "message.get",
			{ id: "s" + id, head: true, avatar: true });
		Kit.eq(hg.avatar, g.message.avatar, "head with avatar: the same avatar");
		return g.message.avatar;
	}
	var lib = avatar_lib();
	g = call(make_env({ cookie: alice, avatar_lib: lib }), "session.get", {});
	Kit.eq(g.user.avatar, AV_DATA, "session.get: the user's own avatar");
	Kit.eq(sender_avatar(lib, M_ALICE), AV_DATA, "local sender with an avatar: by user number");
	Kit.eq(sender_avatar(lib, M_BOB), identicon_of("bob"), "local sender without one: the identicon of the name");
	Kit.eq(sender_avatar(lib, M_NET), identicon_of("Net Person"), "Internet sender: the identicon of the name");
	Kit.eq(sender_avatar(lib, M_ANON), null, "anonymous sender: none");
	Kit.eq(sender_avatar(avatar_lib({ auto_identicon: false }), M_NET), null, "no identicons: none");
	Kit.eq(sender_avatar(avatar_lib({ enabled: false }), M_ALICE), null, "avatars turned off: none");
	g = call(make_env({ cookie: alice, avatar_lib: avatar_lib({ enabled: false }) }), "session.get", {});
	Kit.eq(g.user.avatar, null, "avatars turned off: no user avatar");
	write_avatar(ALICE, { data: AV_DATA, disabled: true });
	Kit.eq(call(make_env({ cookie: alice, avatar_lib: avatar_lib() }), "session.get", {}).user.avatar, null,
		"the user turned theirs off");
	write_avatar(ALICE, { data: base64_encode("short"), disabled: false });
	Kit.eq(sender_avatar(avatar_lib(), M_ALICE), null, "invalid data: none");
	var blink = "";
	for (ai = 0; ai < 60; ai++)
		blink += "A\x9f";
	write_avatar(ALICE, { data: base64_encode(blink), disabled: false });
	Kit.eq(sender_avatar(avatar_lib(), M_ALICE), null, "a blinking attribute: invalid, none");
	// hostile headers cannot steer the library to another avatar file
	write_avatar(5, { data: AV_DATA, disabled: false });
	var M_NUMID = save({ to: "alice", to_ext: String(ALICE), from: "Numeric Id", from_net_type: 5,
		from_net_addr: "numeric@example.com", ftn_bbsid: "5", subject: "numeric ftn_bbsid" });
	Kit.eq(sender_avatar(avatar_lib(), M_NUMID), identicon_of("Numeric Id"),
		"an Internet sender with ftn_bbsid 5: not local user 5's avatar");
	var qlib = function (opts) {
		var l = avatar_lib(opts);
		var orig = l.netuser_fname;
		l.netuser_fname = function (addr) {
			var f = orig.call(l, addr);
			return f ? S.data_dir + f.substr(system.data_dir.length) : f;
		};
		return l;
	};
	mkpath(S.data_dir + "qnet");
	var AV2_BIN = "";
	for (ai = 0; ai < 60; ai++)
		AV2_BIN += "B\x2e";
	var AV2 = base64_encode(AV2_BIN);
	var qf = new File(S.data_dir + "qnet/VERT.avatars.ini");
	Kit.assert(qf.open("w+"), "write the network's avatar file");
	qf.iniSetObject("Mallory", { data: AV2, disabled: false });
	qf.close();
	var M_SLASH = save({ to: "alice", to_ext: String(ALICE), from: "Mallory", from_net_type: 5,
		from_net_addr: "mallory@evil.example/VERT", subject: "address ending in /VERT" });
	Kit.eq(sender_avatar(qlib(), M_SLASH), identicon_of("Mallory"),
		"an Internet address ending in /VERT does not read that network's avatar file");
	var M_BARE = save({ to: "alice", to_ext: String(ALICE), from: "Mallory", from_net_type: 5,
		from_net_addr: "VERT", subject: "Internet address that looks like a QWK ID" });
	Kit.eq(sender_avatar(qlib(), M_BARE), identicon_of("Mallory"),
		"an Internet address 'VERT' is not looked up as a QWK network");
	var M_QWK = save({ to: "alice", to_ext: String(ALICE), from: "Mallory", from_net_type: 4,
		from_net_addr: "VERT", subject: "a real QWK sender" });
	Kit.eq(sender_avatar(qlib(), M_QWK), AV2, "a QWK sender from VERT: that network's avatar (control)");
	var M_QID = save({ to: "alice", to_ext: String(ALICE), from: "Mallory", from_net_type: 4,
		from_net_addr: "VERT/HUB", ftn_bbsid: "VERT", subject: "a QWK sender with a route" });
	Kit.eq(sender_avatar(qlib(), M_QID), AV2, "a QWK sender with a route and BBS ID (control)");

	var broken = avatar_lib();
	broken.read = function () {
		throw new Error("boom");
	};
	Kit.eq(sender_avatar(broken, M_NET), null, "a library that throws: none, the call still works");
	Kit.eq(sender_avatar(null, M_NET), null, "no library: none");
	Kit.eq(call(make_env({ cookie: alice, avatar_lib: broken }), "session.get", {}).user.avatar, null,
		"session.get with a library that throws");

	// --- base_path for the cookie
	Kit.eq(Api.base_path("/mail/api.ssjs"), "/mail/");
	Kit.eq(Api.base_path("/api.ssjs"), "/");
	Kit.eq(Api.base_path("/a/b/part.ssjs"), "/a/b/");
	Kit.eq(Api.base_path(""), "/");
	Kit.eq(Api.base_path(undefined), "/");
	Kit.eq(Api.base_path("/a b/api.ssjs"), "/", "space: not a usable path");
	Kit.eq(Api.base_path("/a;b/api.ssjs"), "/", "semicolon");
	Kit.eq(Api.base_path("relative/api.ssjs"), "/");

	// --- eligibility
	Kit.eq(Api.eligible(USERS[ALICE], cfg), true);
	Kit.eq(Api.eligible(null, cfg), false);
	Kit.eq(Api.eligible(USERS[GUEST], cfg), false);
	Kit.eq(Api.eligible(USERS[GUEST_FLAG], cfg), false);
	Kit.eq(Api.eligible(USERS[DENIED], cfg), false);
	Kit.eq(Api.eligible(USERS[GONE], cfg), false);
	Kit.eq(Api.eligible(USERS[IDLE], cfg), false);
	Kit.eq(Api.eligible({ number: 0 }, cfg), false);
	Kit.eq(Api.eligible(USERS[HIGH], cfg), false, "a user number above 65535");

	// --- A user number above 65535 ends as not signed in, never as an
	// error from the mailbox library: no password or webv4 sign-in, and a
	// stored session of such a user is refused and removed
	err(call(make_env(), "session.login", { user: "high", password: "pw" + HIGH }), "auth",
		"password sign-in above 65535");
	ev = make_env({ config: cfg_webv4, webv4_user: HIGH });
	Kit.eq(call(ev, "session.get", {}).user, null, "no webv4 sign-in above 65535");
	Kit.eq(ev.cookies, [], "no cookie above 65535");
	var high_ud = new UserData(S.user_dir(HIGH) + "webmail/", JsonFiles);
	var high_cookie = Session.create(high_ud, HIGH, { ip: "192.0.2.1", agent: "Test/1.0" }, false,
		time(), cfg, sha256_calc("pw" + HIGH + "\n1600000000", true));
	ev = make_env({ cookie: high_cookie });
	r = Api.dispatch(ev, { calls: [["session.get", {}, 1], ["folder.list", {}, 2]] });
	Kit.eq(r.results[0][1].user, null, "a stored session above 65535: no user");
	err(r.results[1][1], "auth", "a stored session above 65535: not signed in");
	Kit.eq([r.signed_out, ev.made], [true, 0], "signed out, no mailbox opened");
	Kit.eq(ev.cookies, [["", false]], "the cookie cleared");

	// --- context_factory with load_store: the mailbox library is loaded
	// only by the first request that opens a mailbox, and once
	var loads = 0;
	ev = make_env({ cookie: alice });
	ev.make_ctx = Api.context_factory({ config: cfg, get_user: ev.get_user, userdata: ev.userdata,
		shared_path: S.shared_path, data_dir: S.data_dir, load_store: function () {
			loads++;
			return Store;
		} });
	Api.dispatch(ev, { calls: [["session.get", {}, 1]] });
	Kit.eq(loads, 0, "session.get loads no mailbox library");
	r = Api.dispatch(ev, { calls: [["folder.list", {}, 1], ["message.list", { folder: "inbox" }, 2]] });
	Kit.eq([r.results[0][1].ok, r.results[1][1].ok], [true, true], "load_store: calls answered");
	Api.dispatch(ev, { calls: [["changes", { state: "" }, 1]] });
	Kit.eq(loads, 1, "load_store called once");
	Kit.eq([typeof Api.Session.check, typeof Api.Locale.load], ["function", "function"],
		"Api passes on Session and Locale");
	// the user record bits Api uses without loading userdefs.js
	var UDEFS = {};
	load(UDEFS, "userdefs.js");
	for (var udk in Api.UD)
		Kit.eq(Api.UD[udk], UDEFS[udk], "Api.UD." + udk + " as userdefs.js defines it");
} finally {
	Api.log = real_log;
	if (shared)
		shared.close();
	S.cleanup();
}
