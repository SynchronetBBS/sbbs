// Api: the request body size per kind of request (draft.attach and
// draft.save may be larger than every other request), checked before the
// body is parsed (Api.body_check, Api.large_gate: what api.ssjs runs) and
// again on the parsed request (Api.dispatch's meta.length)

load(js.exec_dir + "kit.js");
var W = Kit.api_world();
var Api = W.Api;
var ALICE = W.ALICE;
var BOB = W.BOB;
var err = W.err;
var cfg = W.cfg;
Api.log = function () {};
Api.lib("drafts").log = function () {};

var BASE = Api.BASE_BODY;
var ATTACH = Api.body_allowance(cfg, "draft.attach");
var SAVE = Api.body_allowance(cfg, "draft.save");

function pad(n, ch)
{
	var s = ch || "A";
	while (s.length * 2 <= n)
		s += s;
	return s + s.substr(0, n - s.length);
}

// A body as the client writes it: calls first, as last
function body_of(calls, as)
{
	return JSON.stringify({ calls: calls, as: as });
}

// A body of exactly n bytes: one call of method whose string argument
// "pad" is grown to fit
function sized(method, n, extra_calls)
{
	var calls = [[method, { pad: "" }, 1]].concat(extra_calls || []);
	var base = body_of(calls, ALICE).length;
	calls[0][1].pad = pad(n - base);
	var b = body_of(calls, ALICE);
	Kit.eq(b.length, n, "sized body");
	return b;
}

function env(cookie, o)
{
	var ev = W.make_env({ cookie: cookie, has_header: o && o.has_header, secure: o && o.secure });
	ev.lib_ctx = { domains: ["vert.example"] };
	return ev;
}

try {
	var alice = W.sign_in(ALICE);
	var r, b, c;

	// --- the allowances: computed from the drafts limits, within the host
	Kit.eq(BASE, 262144, "every request: 256 KB");
	Kit.eq(ATTACH, 4 * Math.ceil(1500000 / 3) + 6 * (1024 + 255) + 16384, "draft.attach allowance");
	Kit.eq(ATTACH, 2024058, "draft.attach: 2,024,058 bytes");
	Kit.eq(SAVE, 2 * (cfg.max_body_bytes + 3 * 16384 + 4096 + 253 + 4) + 16384, "draft.save allowance");
	Kit.eq(Api.body_allowance(cfg, "message.list"), BASE, "any other method: the base");
	Kit.eq(Api.max_body(cfg), Math.max(ATTACH, SAVE), "the largest");
	Kit.eq(Api.HOST_MAX_POST, 4194304, "the web server's MAX_POST_LEN");
	Kit.eq(Api.body_allowance({ max_body_bytes: 100 * 1048576 }, "draft.save"), Api.HOST_MAX_POST,
		"never more than the web server gives a script");
	Kit.eq(Api.body_allowance({ max_body_bytes: 0 }, "draft.save"), BASE, "never less than the base");
	var websrvr = new File(Kit.exec_root() + "../src/sbbs3/websrvr.cpp");
	if (websrvr.open("r")) {
		var m = /#define MAX_POST_LEN\s+\((\d+) \* 1048576\)/.exec(websrvr.read());
		websrvr.close();
		Kit.assert(m !== null, "MAX_POST_LEN found");
		Kit.eq(parseInt(m[1], 10) * 1048576, Api.HOST_MAX_POST, "HOST_MAX_POST is MAX_POST_LEN");
	}

	// --- before parsing: Content-Length and the body's first call
	Kit.eq(Api.body_check(cfg, NaN, "{}"), { ok: true, method: null }, "a small body");
	b = sized("message.list", BASE);
	Kit.eq(Api.body_check(cfg, BASE, b), { ok: true, method: null }, "any method up to 256 KB");
	b = sized("message.list", BASE + 1);
	Kit.eq(Api.body_check(cfg, b.length, b).ok, false, "any other method over 256 KB");
	b = sized("draft.get", 2000000);
	Kit.eq(Api.body_check(cfg, b.length, b).ok, false, "a 2 MB draft.get");
	b = sized("draft.attach", 2000000);
	Kit.eq(Api.body_check(cfg, b.length, b), { ok: true, method: "draft.attach" }, "a 2 MB draft.attach");
	b = sized("draft.attach", ATTACH);
	Kit.eq(Api.body_check(cfg, b.length, b).ok, true, "draft.attach at its allowance");
	b = sized("draft.attach", ATTACH + 1);
	Kit.eq(Api.body_check(cfg, b.length, b).ok, false, "draft.attach one byte over");
	b = sized("draft.save", SAVE);
	Kit.eq(Api.body_check(cfg, b.length, b), { ok: true, method: "draft.save" }, "draft.save at its allowance");
	b = sized("draft.save", SAVE + 1);
	Kit.eq(Api.body_check(cfg, b.length, b).ok, false, "draft.save one byte over");
	b = JSON.stringify({ calls: [["message.list", { folder: "inbox" }, 1], ["draft.attach", { pad: pad(2000000) }, 2]] });
	Kit.eq(Api.body_check(cfg, b.length, b).ok, false, "draft.attach not first");
	b = JSON.stringify({ as: ALICE, calls: [["draft.attach", { pad: pad(2000000) }, 1]] });
	Kit.eq(Api.body_check(cfg, b.length, b).ok, false, "another order: held to the base");
	Kit.eq(Api.body_check(cfg, Api.max_body(cfg) + 1, "{}").ok, false, "declared over the largest");
	Kit.eq(Api.body_check(cfg, 5000000, undefined).ok, false, "a body the web server kept (over 4 MiB)");
	Kit.eq(Api.body_check(cfg, NaN, undefined).ok, true, "no body at all");

	// --- after parsing: exactly one call of that method
	b = sized("draft.attach", 2000000, [["folder.list", {}, 2]]);
	Kit.eq(Api.body_check(cfg, b.length, b).ok, true, "a second call passes the first look");
	r = Api.dispatch(env(alice), Api.parse_body(b), { length: b.length });
	Kit.eq(r.error, "toolarge", "draft.attach plus another call: refused");
	Kit.eq(r.results, [], "no call ran");
	b = sized("draft.attach", 2000000, [["draft.attach", {}, 2]]);
	r = Api.dispatch(env(alice), Api.parse_body(b), { length: b.length });
	Kit.eq(r.error, "toolarge", "two draft.attach calls: refused");
	r = Api.dispatch(env(alice), { calls: [["folder.list", {}, 1]], as: ALICE }, { length: BASE + 1 });
	Kit.eq(r.error, "toolarge", "one other call in a large body: refused");
	r = Api.dispatch(env(alice), { calls: [["folder.list", {}, 1], ["draft.list", {}, 2]], as: ALICE },
		{ length: BASE });
	Kit.eq(r.results.length, 2, "up to the base, any batch");
	r = Api.dispatch(env(alice), { calls: [["draft.save", {}, 1]], as: ALICE }, { length: SAVE + 1 });
	Kit.eq(r.error, "toolarge", "draft.save over its allowance");

	// --- a real 1,500,000-byte chunk end to end
	var d = W.call(env(alice), "draft.create", { mode: "new" }).draft;
	var chunk = pad(1500000, "\x01\xfe\x7f");
	b = body_of([["draft.attach", { id: d.id, upload: "big-upload-1", name: "big.bin",
		type: "application/octet-stream", offset: 0, total: 1500001, b64: base64_encode(chunk) }, 7]], ALICE);
	Kit.assert(b.length > 2000000 && b.length <= ATTACH, "a full chunk's body: " + b.length);
	c = Api.body_check(cfg, b.length, b);
	Kit.eq(c, { ok: true, method: "draft.attach" }, "accepted before parsing");
	var ev = env(alice);
	Kit.eq(Api.large_gate(ev, b), null, "passes the gates");
	r = Api.dispatch(ev, Api.parse_body(b), { length: b.length });
	var res = r.results[0][1];
	Kit.eq(res.ok, true, "stored: " + JSON.stringify(res).substr(0, 200));
	Kit.eq([res.attachment.received, res.attachment.complete], [1500000, false], "1,500,000 bytes received");
	var got = W.call(env(alice), "draft.get", { id: d.id }).draft;
	Kit.eq(got.attachments[0].size, 1500000, "on disk");
	// a full draft.save near its allowance
	var text = pad(cfg.max_body_bytes, "a\"\n\\");
	b = body_of([["draft.save", { id: d.id, rev: got.rev, body: text, to: pad(16384, "b"),
		subject: pad(4096, "s") }, 8]], ALICE);
	// as a browser writes it: this engine's JSON.stringify writes a line
	// break as \u000a, a browser's as \n
	b = b.split("\\u000a").join("\\n");
	Kit.assert(b.length > BASE && b.length <= SAVE, "a full save's body: " + b.length);
	Kit.eq(Api.body_check(cfg, b.length, b).method, "draft.save", "draft.save accepted");
	r = Api.dispatch(env(alice), Api.parse_body(b), { length: b.length });
	Kit.eq(r.results[0][1].ok, true, "saved: " + JSON.stringify(r.results[0][1]).substr(0, 200));
	Kit.eq(r.results[0][1].draft.body.length, cfg.max_body_bytes, "the whole body");

	// --- the gates before parsing: as cheap as for an ordinary request
	b = sized("draft.attach", 2000000);
	r = Api.large_gate(env(alice, { has_header: false }), b);
	Kit.eq([r.error, r.message], ["forbidden", W.STRINGS["errors.forbidden"]], "no X-Webmail header");
	r = Api.large_gate(env(alice, { secure: false }), b);
	Kit.eq(r.error, "https", "not secure");
	ev = env(undefined);
	r = Api.large_gate(ev, b);
	Kit.eq([r.error, r.usernum], ["auth", 0], "not signed in");
	Kit.eq(ev.made, 0, "no mailbox opened");
	ev = env("1." + ALICE + ".ffffffffffffffffffffffffffffffff");
	r = Api.large_gate(ev, b);
	Kit.eq([r.error, r.signed_out], ["auth", true], "a cookie that is no session");
	Kit.eq(ev.cookies, [["", false]], "the cookie cleared, as dispatch does");
	ev = env(undefined);
	Api.large_gate(ev, b);
	Kit.eq(ev.cookies, [], "no cookie: nothing to clear");
	var bob = W.sign_in(BOB);
	r = Api.large_gate(env(bob), b);
	Kit.eq([r.error, r.usernum], ["stale", BOB], "a body for another user (as at its end)");
	Kit.eq(Api.large_gate(env(alice), b), null, "the right user");
	Kit.eq(Api.large_gate(env(alice), JSON.stringify({ calls: [["draft.attach", {}, 1]] })), null,
		"no as at the end: left to dispatch");
	// every shape rpc.js request() writes (calls first, background when
	// set, as last: pinned in client.js): the method is read from the
	// start and "as" from the end
	var shapes = [{ calls: [["draft.attach", { pad: pad(2000000) }, 1]], as: BOB },
		{ calls: [["draft.attach", { pad: pad(2000000) }, 1]], background: true, as: BOB }];
	for (var si = 0; si < shapes.length; si++) {
		var sb = JSON.stringify(shapes[si]);
		Kit.eq(Api.body_check(cfg, sb.length, sb), { ok: true, method: "draft.attach" }, "client shape " + si + ": the method");
		r = Api.large_gate(env(alice), sb);
		Kit.eq([r && r.error, r && r.usernum], ["stale", ALICE], "client shape " + si + ": as read from the end");
	}

	// --- max_body_bytes above what a request can carry: the largest that
	// fits, one warning
	var Config = Kit.wlib("config");
	var warned = [];
	Config.warn = function (t) {
		warned.push(t);
	};
	var fit = Config.MAX_BODY_FIT;
	Kit.eq(Api.body_allowance({ max_body_bytes: fit }, "draft.save") <= Api.HOST_MAX_POST, true,
		"MAX_BODY_FIT's draft.save fits the web server's limit");
	Kit.assert(2 * (fit + 1 + 3 * 16384 + 4096 + 253 + 4) + 16384 > Api.HOST_MAX_POST,
		"one byte more would not");
	Kit.eq(Api.body_allowance({ max_body_bytes: fit + 1 }, "draft.save"), Api.HOST_MAX_POST,
		"(clamped there)");
	var big = Config.load({ max_body_bytes: 5000000 });
	Kit.eq(big.max_body_bytes, fit, "configured above: the largest that fits");
	Kit.eq(warned.length, 1, "one warning: " + warned.join(" | "));
	Kit.assert(/max_body_bytes/.test(warned[0]), "naming the option");
	warned = [];
	Kit.eq(Config.load({ max_body_bytes: fit }).max_body_bytes, fit, "at the limit: kept");
	Kit.eq(warned.length, 0, "no warning");
	// a draft of that size can be saved
	var fcfg = {};
	for (var fk in cfg)
		fcfg[fk] = cfg[fk];
	fcfg.max_body_bytes = fit;
	var fev = W.make_env({ cookie: alice, config: fcfg });
	fev.lib_ctx = { domains: ["vert.example"] };
	var fd = W.call(fev, "draft.create", { mode: "new" }).draft;
	b = body_of([["draft.save", { id: fd.id, rev: fd.rev, body: pad(fit, "a\"\n\\"),
		to: pad(16384, "b"), cc: pad(16384, "c"), bcc: pad(16384, "d"), subject: pad(4096, "s"),
		from_domain: "", priority: "high" }, 9]], ALICE).split("\\u000a").join("\\n");
	Kit.assert(b.length <= Api.HOST_MAX_POST, "the fullest save fits 4 MiB: " + b.length);
	Kit.eq(Api.body_check(fcfg, b.length, b).method, "draft.save", "accepted");

	// --- credentials: a limit of their own, no longer the body's
	ev = env();
	err(W.call(ev, "session.login", { user: "alice", password: pad(8192) }), "auth", "8192: auth");
	err(W.call(ev, "session.login", { user: "alice", password: pad(8193) }), "invalid", "8193: invalid");
	err(W.call(ev, "session.login", { user: pad(8193), password: "x" }), "invalid", "user 8193: invalid");
} finally {
	W.cleanup();
}
