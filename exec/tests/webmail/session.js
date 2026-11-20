// Session: login sessions and their cookies (webmail/lib/session.js)

load(js.exec_dir + "kit.js");
var Session = Kit.wlib("session");
var Config = Kit.wlib("config");
var UserData = Kit.wlib("userdata");
var JsonFiles = Kit.lib("mailbox_files");	// this worktree's copy

var T0 = 1700000000;	// the injected clock
var DAY = 86400;
var cfg = Config.load({ session_timeout: 7200, remember_days: 30 });
var CLIENT = { ip: "192.0.2.1", agent: "Test/1.0" };

// A UserData that counts reads and updates (writes)
function Counting(ud)
{
	this.ud = ud;
	this.reads = 0;
	this.updates = 0;
}
Counting.prototype.read = function (name, fallback)
{
	this.reads++;
	return this.ud.read(name, fallback);
};
Counting.prototype.update = function (name, fallback, fn, options)
{
	this.updates++;
	this.options = options;
	return this.ud.update(name, fallback, fn, options);
};

function key_of(value)
{
	return value.substr(value.indexOf('.') + 1);
}

function count(obj)
{
	var n = 0;
	for (var k in obj)
		n++;
	return n;
}

var scr = Kit.scratch();
try {
	var dir = Kit.webmail_dir(60001, scr);
	var ud = new UserData(dir, JsonFiles);

	// --- parse
	Kit.eq(Session.parse("60001." + "0123456789abcdef0123456789abcdef01234567"),
		{ usernum: 60001, key: "0123456789abcdef0123456789abcdef01234567" });
	var bad_values = ["x", "1.", "0.abc", "1.2.3", "../1.abc", "1e3.abc", "", ".", "1",
		"01." + "0123456789abcdef0123456789abcdef01234567",
		"-1." + "0123456789abcdef0123456789abcdef01234567",
		"1 ." + "0123456789abcdef0123456789abcdef01234567",
		"1." + "0123456789ABCDEF0123456789abcdef01234567",	// uppercase
		"1." + "0123456789abcdef0123456789abcdef0123456",	// 39
		"1." + "0123456789abcdef0123456789abcdef012345678",	// 41
		"1." + "0123456789abcdef0123456789abcdef01234567\n",
		"12345678901." + "0123456789abcdef0123456789abcdef01234567",
		null, undefined, 5, {}];
	var i;
	for (i = 0; i < bad_values.length; i++)
		Kit.assert(Session.parse(bad_values[i]) === null, "parse rejects " + JSON.stringify(bad_values[i]));

	// --- create then check
	var v1 = Session.create(ud, 60001, CLIENT, false, T0, cfg);
	Kit.assert(/^60001\.[0-9a-f]{40}$/.test(v1), "cookie value format: " + v1);
	Kit.eq(Session.check(ud, v1, T0, cfg), { usernum: 60001, keep: false });

	// The stored file never contains the key; it holds the key's hash
	var text = (function () {
		var f = new File(dir + "webmail.sessions.json");
		Kit.assert(f.open("r"), "sessions file exists");
		var t = f.read();
		f.close();
		return t;
	})();
	Kit.assert(text.indexOf(key_of(v1)) < 0, "key not stored");
	var stored = ud.read("sessions", null);
	Kit.eq(count(stored), 1, "one session stored");
	for (var h in stored) {
		Kit.eq(h, sha256_calc(key_of(v1), true), "keyed by the key's hash");
		Kit.eq(stored[h], { usernum: 60001, created: T0, seen: T0, keep: false,
			client: { ip: "192.0.2.1", agent: "Test/1.0" } });
	}

	// Keys differ between sessions
	var v2 = Session.create(ud, 60001, CLIENT, true, T0, cfg);
	Kit.assert(key_of(v1) !== key_of(v2), "distinct keys");

	// A different key returns null
	var other = "60001." + (key_of(v1).charAt(0) == "a" ? "b" : "a") + key_of(v1).substr(1);
	Kit.assert(Session.check(ud, other, T0, cfg) === null, "different key");
	// The right key with another user number returns null
	Kit.assert(Session.check(ud, "60002." + key_of(v1), T0, cfg) === null, "other usernum");
	// Uppercase, 39 and 41 characters
	Kit.assert(Session.check(ud, "60001." + key_of(v1).toUpperCase(), T0, cfg) === null, "uppercase key");
	Kit.assert(Session.check(ud, "60001." + key_of(v1).substr(1), T0, cfg) === null, "39 chars");
	Kit.assert(Session.check(ud, v1 + "0", T0, cfg) === null, "41 chars");

	// Two sessions coexist; destroying one leaves the other valid
	Kit.eq(Session.check(ud, v2, T0, cfg), { usernum: 60001, keep: true });
	Kit.assert(Session.destroy(ud, v1) === true, "destroy v1");
	Kit.assert(Session.check(ud, v1, T0, cfg) === null, "v1 gone");
	Kit.eq(Session.check(ud, v2, T0, cfg), { usernum: 60001, keep: true });
	Kit.assert(Session.destroy(ud, v1) === false, "destroy again: nothing");
	Session.destroy(ud, v2);
	Kit.eq(ud.read("sessions", null), {}, "all destroyed");

	// --- Malformed values touch no file (fresh directory, counted access)
	var dir2 = Kit.webmail_dir(60002, scr);
	var cu = new Counting(new UserData(dir2, JsonFiles));
	for (i = 0; i < bad_values.length; i++) {
		Kit.assert(Session.check(cu, bad_values[i], T0, cfg) === null, "check " + JSON.stringify(bad_values[i]));
		Kit.assert(Session.destroy(cu, bad_values[i]) === false, "destroy " + JSON.stringify(bad_values[i]));
	}
	Kit.eq(cu.reads + cu.updates, 0, "no file access for malformed values");
	Kit.eq(directory(dir2 + "*").length, 0, "no file created");
	// A well-formed value for a user with no sessions file creates nothing
	Kit.assert(Session.check(cu, "60002." + key_of(v1), T0, cfg) === null, "no sessions");
	Kit.assert(Session.destroy(cu, "60002." + key_of(v1)) === false, "destroy, no sessions");
	Kit.eq(cu.updates, 0, "no update without sessions");
	Kit.eq(directory(dir2 + "*").length, 0, "still no file");

	// --- Expiry without keep: exactly session_timeout idle seconds
	var TO = cfg.session_timeout;
	var a = Session.create(ud, 60001, CLIENT, false, T0, cfg);
	Kit.eq(Session.check(ud, a, T0 + TO - 1, cfg), { usernum: 60001, keep: false }, "idle TO-1");
	// seen is now T0+TO-1; idle measured from there
	var S = T0 + TO - 1;
	Kit.eq(Session.check(ud, a, S + TO - 1, cfg), { usernum: 60001, keep: false }, "idle TO-1 again");
	S = S + TO - 1;
	Kit.assert(Session.check(ud, a, S + TO, cfg) === null, "idle TO: expired");
	// check alone does not write to remove it; the next create does
	Kit.eq(count(ud.read("sessions", null)), 1, "expired session left by check");
	Session.destroy(ud, Session.create(ud, 60001, CLIENT, false, S + TO, cfg));
	Kit.eq(ud.read("sessions", null), {}, "expired session removed by create");

	// --- Expiry with keep: remember_days after creation, regardless of use
	var RD = cfg.remember_days * DAY;
	var k = Session.create(ud, 60001, CLIENT, true, T0, cfg);
	Kit.eq(Session.check(ud, k, T0 + TO * 3, cfg), { usernum: 60001, keep: true }, "keep survives idle");
	Kit.eq(Session.check(ud, k, T0 + RD - 1, cfg), { usernum: 60001, keep: true }, "keep RD-1");
	Kit.assert(Session.check(ud, k, T0 + RD, cfg) === null, "keep RD: expired");
	Session.destroy(ud, Session.create(ud, 60001, CLIENT, false, T0 + RD, cfg));
	Kit.eq(ud.read("sessions", null), {}, "expired keep session removed");

	// --- Without keep: also ends 24 hours after creation, however used
	var n24 = Session.create(ud, 60001, CLIENT, false, T0, cfg);
	var k24 = Session.create(ud, 60001, CLIENT, true, T0, cfg);
	var t;
	for (t = T0 + 60; t < T0 + DAY; t += 60) {
		if (Session.check(ud, n24, t, cfg) === null)
			break;
	}
	Kit.eq(t, T0 + DAY, "used every minute: valid until 24 hours");
	Kit.assert(Session.check(ud, n24, T0 + DAY - 1, cfg) !== null, "24h - 1");
	Kit.assert(Session.check(ud, n24, T0 + DAY, cfg) === null, "24h: expired");
	Kit.eq(Session.check(ud, k24, T0 + DAY, cfg), { usernum: 60001, keep: true }, "keep: no 24h limit");
	ud.write("sessions", {});

	// --- touch: false returns the session without refreshing seen
	var cq = new Counting(ud);
	var q = Session.create(ud, 60001, CLIENT, false, T0, cfg);
	var qh = sha256_calc(key_of(q), true);
	for (t = T0 + 60; t < T0 + TO; t += 600) {
		Kit.eq(Session.check(cq, q, t, cfg, { touch: false }), { usernum: 60001, keep: false },
			"touch false at " + (t - T0));
	}
	Kit.eq(cq.updates, 0, "touch false writes nothing");
	Kit.eq(ud.read("sessions", null)[qh].seen, T0, "seen not refreshed");
	Kit.assert(Session.check(cq, q, T0 + TO, cfg, { touch: false }) === null,
		"polling does not extend the idle time");
	Kit.eq(cq.updates, 0, "touch false writes nothing, expired either");
	// The default is touch: true
	var q2 = Session.create(ud, 60001, CLIENT, false, T0, cfg);
	Kit.assert(Session.check(cq, q2, T0 + 60, cfg, {}) !== null, "touch default");
	Kit.eq(cq.updates, 1, "default touches");
	ud.write("sessions", {});

	// --- Expired sessions are removed on the next create, or on a check
	// that writes seen anyway
	var e1 = Session.create(ud, 60001, CLIENT, false, T0, cfg);
	var e2 = Session.create(ud, 60001, CLIENT, false, T0 + TO, cfg);	// removes e1
	Kit.eq(count(ud.read("sessions", null)), 1, "create removed the expired one");
	var e3 = Session.create(ud, 60001, CLIENT, true, T0 + 2 * TO - 10, cfg);
	Kit.eq(count(ud.read("sessions", null)), 2, "e2 not yet expired");
	// e3 was seen 10 seconds ago (no seen write due): the expired e2 is
	// left alone
	Kit.eq(Session.check(ud, e3, T0 + 2 * TO, cfg), { usernum: 60001, keep: true });
	Kit.eq(count(ud.read("sessions", null)), 2, "no write for an expired entry alone");
	// A minute later seen is due, and e2 goes with that write
	Kit.eq(Session.check(ud, e3, T0 + 2 * TO + 50, cfg), { usernum: 60001, keep: true });
	var left = ud.read("sessions", null);
	Kit.eq(count(left), 1, "check removed the expired one");
	Kit.assert(left[sha256_calc(key_of(e3), true)] !== undefined, "e3 remains");
	Session.destroy(ud, e3);
	Kit.assert(Session.check(ud, e1, T0, cfg) === null && Session.check(ud, e2, T0 + TO, cfg) === null,
		"removed sessions stay invalid");

	// --- seen is written at most once a minute; no lock when nothing to write
	var cw = new Counting(ud);
	var s = Session.create(cw, 60001, CLIENT, false, T0, cfg);
	Kit.eq(cw.updates, 1, "create writes once");
	cw.updates = 0;
	Kit.assert(Session.check(cw, s, T0, cfg) !== null, "check at T0");
	Kit.assert(Session.check(cw, s, T0 + 30, cfg) !== null, "check at +30");
	Kit.assert(Session.check(cw, s, T0 + 59, cfg) !== null, "check at +59");
	Kit.eq(cw.updates, 0, "no write within a minute");
	Kit.assert(Session.check(cw, s, T0 + 60, cfg) !== null, "check at +60");
	Kit.eq(cw.updates, 1, "one write after a minute");
	Kit.eq(ud.read("sessions", null)[sha256_calc(key_of(s), true)].seen, T0 + 60, "seen updated");
	Kit.assert(Session.check(cw, s, T0 + 100, cfg) !== null, "check at +100");
	Kit.eq(cw.updates, 1, "no second write within the minute");

	// --- A lock held on sessions.json does not fail a valid check
	var lock = ud.files.lock_path("sessions");
	Kit.assert(file_mutex(lock, "held by test", 60), "take the sessions lock");
	try {
		// With the default (5 second) lock timeout, check gives up quickly
		var started = system.timer;
		Kit.eq(Session.check(cw, s, T0 + 200, cfg), { usernum: 60001, keep: false }, "check while locked");
		var took = system.timer - started;
		// well under the 5 second default (the short wait is 250 ms)
		Kit.assert(took < 3, "check while locked took " + took + " s");
		Kit.eq(cw.options, { timeout: 250 }, "short timeout for the seen write");
		Kit.eq(ud.files.timeout, 5000, "UserData timeout restored");
		ud.files.timeout = 100;
		// create cannot store a session without the lock
		Kit.throws(function () {
			Session.create(ud, 60001, CLIENT, false, T0 + 200, cfg);
		}, "conflict");
	} finally {
		file_remove(lock);
		ud.files.timeout = 5000;
	}
	Kit.eq(ud.read("sessions", null)[sha256_calc(key_of(s), true)].seen, T0 + 60, "seen unchanged while locked");
	Kit.assert(Session.check(cw, s, T0 + 200, cfg) !== null, "check after unlock");
	Kit.eq(ud.read("sessions", null)[sha256_calc(key_of(s), true)].seen, T0 + 200, "seen written after unlock");
	Session.destroy(ud, s);

	// --- Per-user cap: the oldest by last seen goes
	var vals = [];
	for (i = 0; i < Session.MAX_SESSIONS; i++)
		vals.push(Session.create(ud, 60001, CLIENT, true, T0 + 1000 - i, cfg));	// vals[last] seen earliest
	Kit.eq(count(ud.read("sessions", null)), Session.MAX_SESSIONS, "at the cap");
	var oldest = vals[vals.length - 1];
	var extra = Session.create(ud, 60001, CLIENT, true, T0 + 2000, cfg);
	Kit.eq(count(ud.read("sessions", null)), Session.MAX_SESSIONS, "still at the cap");
	Kit.assert(Session.check(ud, oldest, T0 + 2000, cfg) === null, "oldest removed");
	Kit.assert(Session.check(ud, vals[0], T0 + 2000, cfg) !== null, "newest kept");
	Kit.assert(Session.check(ud, extra, T0 + 2000, cfg) !== null, "new one valid");
	ud.write("sessions", {});

	// --- destroy_all removes every session of the user
	Kit.eq(Session.destroy_all(ud, 60001), 0, "nothing to remove");
	var all = [];
	for (i = 0; i < 3; i++)
		all.push(Session.create(ud, 60001, CLIENT, i == 0, T0, cfg));
	Kit.eq(Session.destroy_all(ud, 60001), 3, "three removed");
	for (i = 0; i < all.length; i++)
		Kit.assert(Session.check(ud, all[i], T0, cfg) === null, "destroyed " + i);
	Kit.eq(ud.read("sessions", null), {}, "file empty");
	var cd = new Counting(new UserData(dir2, JsonFiles));
	Kit.eq(Session.destroy_all(cd, 60002), 0, "no sessions file");
	Kit.eq(cd.updates, 0, "no write without sessions");
	Kit.eq(directory(dir2 + "*").length, 0, "no file created by destroy_all");
	Kit.throws(function () {
		Session.destroy_all(ud, 0);
	});

	// --- Client description: truncated, control characters removed
	var c = Session.create(ud, 60001, { ip: "198.51.100.7\r\n", agent: "Bad\x00\x1b[2JAgent\x7f" + new Array(600).join("x") }, false, T0, cfg);
	var rec = ud.read("sessions", null)[sha256_calc(key_of(c), true)];
	Kit.eq(rec.client.ip, "198.51.100.7");
	Kit.assert(rec.client.agent.indexOf("Bad[2JAgentx") === 0, "agent cleaned: " + rec.client.agent.substr(0, 20));
	Kit.assert(rec.client.agent.length <= Session.MAX_AGENT, "agent truncated");
	Session.create(ud, 60001, null, false, T0, cfg);	// no client info
	ud.write("sessions", {});

	// --- A damaged sessions file yields nothing and is replaced on create
	ud.write("sessions", [1, 2]);
	Kit.assert(Session.check(ud, c, T0, cfg) === null, "array file");
	ud.write("sessions", { x: 5, y: null });
	Kit.assert(Session.check(ud, c, T0, cfg) === null, "junk entries");
	var d = Session.create(ud, 60001, CLIENT, false, T0, cfg);
	Kit.eq(count(ud.read("sessions", null)), 1, "junk entries removed on create");
	Kit.assert(Session.check(ud, d, T0, cfg) !== null, "valid after repair");
	ud.write("sessions", {});

	// --- cookie_header
	Kit.eq(Session.cookie_header(v1, { secure: true, keep: false, config: cfg, path: "/webmail/" }),
		"; Path=/webmail/; HttpOnly; Secure; SameSite=Lax");
	Kit.eq(Session.cookie_header(v1, { secure: false, keep: false, config: cfg, path: "/" }),
		"; Path=/; HttpOnly; SameSite=Lax");
	Kit.eq(Session.cookie_header(v1, { secure: true, keep: true, config: cfg, path: "/webmail/" }),
		"; Path=/webmail/; HttpOnly; Secure; SameSite=Lax; Max-Age=" + (30 * DAY));
	// Clearing the cookie (empty value): Max-Age=0
	Kit.eq(Session.cookie_header("", { secure: true, keep: true, config: cfg, path: "/webmail/" }),
		"; Path=/webmail/; HttpOnly; Secure; SameSite=Lax; Max-Age=0");
	// Invalid paths fall back to "/"
	var bad_paths = ["webmail/", "/webmail", "/a b/", "/a;b/", "/a\r\nSet-Cookie: x/", "", null, undefined, 7, "/a\x7f/"];
	for (i = 0; i < bad_paths.length; i++)
		Kit.eq(Session.cookie_header(v1, { secure: true, keep: false, config: cfg, path: bad_paths[i] }),
			"; Path=/; HttpOnly; Secure; SameSite=Lax", "bad path " + JSON.stringify(bad_paths[i]));

	// --- is_secure
	var pcfg = Config.load({ tls_proxy_header: "X-Forwarded-Proto" });
	Kit.assert(Session.is_secure("http", {}, cfg) === false, "http, no header");
	Kit.assert(Session.is_secure("https", {}, cfg) === true, "https");
	Kit.assert(Session.is_secure("HTTPS", null, cfg) === true, "HTTPS");
	Kit.assert(Session.is_secure("http", { "x-forwarded-proto": "https" }, pcfg) === true, "proxy header");
	Kit.assert(Session.is_secure("http", { "X-FORWARDED-PROTO": " HTTPS\t" }, pcfg) === true, "case and space");
	Kit.assert(Session.is_secure("http", { "x-forwarded-proto": "https" }, cfg) === false, "setting empty");
	Kit.assert(Session.is_secure("http", { "x-forwarded-proto": "http" }, pcfg) === false, "proxy says http");
	Kit.assert(Session.is_secure("http", { "x-forwarded-proto": "https, http" }, pcfg) === true, "first is https");
	Kit.assert(Session.is_secure("http", { "x-forwarded-proto": "http, https" }, pcfg) === false, "first is http");
	Kit.assert(Session.is_secure("http", { "x-forwarded-proto": "httpsx" }, pcfg) === false, "not https");
	Kit.assert(Session.is_secure("http", { "x-other": "https" }, pcfg) === false, "other header");
	Kit.assert(Session.is_secure("http", null, pcfg) === false, "no headers");
	Kit.assert(Session.is_secure(undefined, undefined, pcfg) === false, "nothing");
	var ccfg = Config.load({ tls_proxy_header: "X-Forwarded-Proto:" });
	Kit.assert(Session.is_secure("http", { "x-forwarded-proto": "https" }, ccfg) === true, "trailing colon");
	Kit.assert(Session.is_secure("http", { "x-forwarded-proto": "http" }, ccfg) === false, "colon, http");

	// --- stamps: a session made with a stamp is valid only with that stamp
	var sdir = Kit.webmail_dir(60009, scr);
	var sud = new UserData(sdir, JsonFiles);
	var sv = Session.create(sud, 60009, CLIENT, false, T0, cfg, "1700000000");
	Kit.eq(Session.check(sud, sv, T0, cfg, { touch: false, stamp: "1700000000" }),
		{ usernum: 60009, keep: false }, "same stamp");
	var stored = sud.read("sessions");
	var hk = sha256_calc(key_of(sv), true);
	Kit.eq(stored[hk].stamp, "1700000000", "the stamp is stored");
	Kit.eq(Session.check(sud, sv, T0, cfg, { touch: false }), null, "stamped session, no stamp given");
	Kit.assert(sud.read("sessions")[hk] !== undefined, "a check without a stamp removes nothing");
	Kit.eq(Session.check(sud, sv, T0, cfg, { touch: false, stamp: "1700000001" }), null,
		"another stamp (the password changed)");
	Kit.eq(sud.read("sessions")[hk], undefined, "a stamp mismatch removes the session");
	// a session stored without a stamp matches only when none is given
	var nv = Session.create(sud, 60009, CLIENT, false, T0, cfg);
	Kit.eq(Session.check(sud, nv, T0, cfg, { touch: false }), { usernum: 60009, keep: false },
		"no stamp either side");
	Kit.eq(Session.check(sud, nv, T0, cfg, { touch: false, stamp: "1" }), null, "stamp given, none stored");
	// the stamp may be a function, called only once the key has matched
	var fv = Session.create(sud, 60009, CLIENT, false, T0, cfg, "s1");
	var calls = 0;
	var stamp_fn = function () {
		calls++;
		return "s1";
	};
	Kit.eq(Session.check(sud, fv, T0, cfg, { touch: false, stamp: stamp_fn }),
		{ usernum: 60009, keep: false }, "a stamp function");
	Kit.eq(calls, 1, "called once for a matching key");
	calls = 0;
	Kit.eq(Session.check(sud, "60009.0123456789abcdef0123456789abcdef01234567", T0, cfg,
		{ touch: false, stamp: stamp_fn }), null, "unknown key");
	Kit.eq(Session.check(sud, "junk", T0, cfg, { touch: false, stamp: stamp_fn }), null, "malformed");
	Kit.eq(calls, 0, "not called for an unknown or malformed key");
	Kit.eq(Session.check(sud, fv, T0, cfg, { touch: false, stamp: function () {
		return "s2";
	} }), null, "a stamp function giving another stamp");
	Kit.eq(Session.check(sud, fv, T0, cfg, { touch: false, stamp: stamp_fn }), null,
		"that mismatch removed the session");
	// a stamp that is not a string is not stored
	var xv = Session.create(sud, 60009, CLIENT, false, T0, cfg, 5);
	Kit.eq(Session.check(sud, xv, T0, cfg, { touch: false }), { usernum: 60009, keep: false },
		"a non-string stamp counts as none");
} finally {
	scr.cleanup();
}
