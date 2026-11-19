// Shell: the shell page's logic (webmail/lib/shell.js): the boot data,
// the embedding of it in the page, versioned asset paths and the
// response headers

load(js.exec_dir + "kit.js");
var Shell = Kit.wlib("shell");
var Config = Kit.wlib("config");

var WEBMAIL_DIR = Kit.exec_root() + "../webmail/";
Config.warn = function () {};
function cfg_default()
{
	return Config.load({});
}
var cfg = Config.load({ directory: WEBMAIL_DIR, product_name: "Family Mail",
	brand: "<b>Ours</b>", fallback_url: "https://example.com/old" });

function own(o, k)
{
	return Object.prototype.hasOwnProperty.call(o, k);
}

function fail(what)
{
	return function () {
		throw new Error("must not be called: " + what);
	};
}

function make_env(o)
{
	o = o || {};
	var e = {
		config: o.config || cfg,
		has_header: false,	// a page load never carries X-Webmail
		secure: o.secure === undefined ? true : o.secure,
		cookie: o.cookie,
		accept_language: o.accept_language || "",
		client: { ip: "192.0.2.1", agent: "Test/1.0" },
		webv4_user: function () {
			return 0;
		},
		find_user: fail("find_user"),
		login: fail("login"),
		get_user: fail("get_user"),
		userdata: fail("userdata"),
		make_ctx: fail("make_ctx"),
		set_cookie: function (v) {
			e.cookie_set = v;
		}
	};
	return e;
}

// --- boot: the session.get result, without a session
var ev = make_env();
var b = Shell.boot(ev);
Kit.eq(b.ok, true, "boot ok");
Kit.eq(b.user, null, "no user");
Kit.eq(b.signed_out, false, "not signed out");
Kit.eq(b.insecure, false, "secure request");
Kit.eq(b.locale, "en_us", "locale");
Kit.eq(b.product_name, "Family Mail", "product name");
Kit.eq(b.brand, "<b>Ours</b>", "brand markup passed as is");
Kit.eq(b.fallback_url, "https://example.com/old", "fallback url");
Kit.eq(b.poll_ms, cfg.poll_interval * 1000, "poll_ms");
Kit.assert(typeof b.powered_by === "string" && b.powered_by.indexOf("Synchronet") === 0
	&& !/\s\s/.test(b.powered_by), "boot: powered_by from the version notice: " + b.powered_by);
Kit.assert(typeof b.limits === "object" && b.limits.max_drafts === cfg.max_drafts, "limits");
Kit.assert(b.domains instanceof Array, "domains");
Kit.assert(typeof b.strings === "object" && b.strings !== null, "strings sent");
Kit.eq(b.strings["main.button_sign_in"], "Sign in", "a string");
Kit.eq(ev.has_header, false, "the caller's env is not changed");
Kit.eq(ev.cookie_set, undefined, "no cookie set");

// An insecure request where TLS is not required
var plain = Config.load({ directory: WEBMAIL_DIR, require_tls: false });
b = Shell.boot(make_env({ config: plain, secure: false }));
Kit.eq(b.ok, true, "plain http allowed");
Kit.eq(b.insecure, true, "insecure flagged");

// A cookie that is not a valid session: signed_out, and the cookie cleared
ev = make_env({ cookie: ["not-a-session"] });
b = Shell.boot(ev);
Kit.eq(b.ok, true, "boot ok with a stale cookie");
Kit.eq(b.user, null, "no user with a stale cookie");
Kit.eq(b.signed_out, true, "signed out reported");
Kit.eq(ev.cookie_set, "", "stale cookie cleared");

// --- needs_https
Kit.eq(Shell.needs_https(cfg, false), true, "default config, plain http");
Kit.eq(Shell.needs_https(cfg, true), false, "default config, https");
Kit.eq(Shell.needs_https(plain, false), false, "require_tls off");

// --- strings and text, for the page written without a session
var s = Shell.strings(cfg, "");
Kit.eq(s.locale, "en_us", "default locale");
Kit.eq(Shell.text(s.strings, "main.label_needs_https").indexOf("secure connection") >= 0, true,
	"needs https text");
Kit.eq(Shell.text(s.strings, "main.no_such_key"), "main.no_such_key", "missing key gives the key");
Kit.eq(Shell.text(s.strings, "constructor"), "constructor", "inherited name is not a string");
Kit.eq(Shell.text(null, "a.b"), "a.b", "no strings");
// {name}: the product name, substituted here (before any escaping)
var NAME_KEYS = ["main.label_needs_https", "main.label_unsupported_browser", "main.label_javascript_required"];
for (var i = 0; i < NAME_KEYS.length; i++) {
	Kit.assert(s.strings[NAME_KEYS[i]].indexOf("{name}") >= 0, NAME_KEYS[i] + " uses {name}");
	var txt = Shell.text(s.strings, NAME_KEYS[i], { name: "Our <Mail>" });
	Kit.assert(txt.indexOf("Our <Mail>") >= 0, NAME_KEYS[i] + " gets the name");
	Kit.eq(txt.indexOf("{name}"), -1, NAME_KEYS[i] + " has no placeholder left");
}
Kit.eq(Shell.text({ "a.b": "x {name} {other} {name}" }, "a.b", { name: "N" }), "x N {other} N",
	"every {name}; unknown placeholders kept");
Kit.eq(Shell.text({ "a.b": "x {name}" }, "a.b", { name: "$&$1" }), "x $&$1", "replacement taken literally");
// the caller's Locale library is the one used
var fake_locale = { available: function () { return ["xx"]; }, pick: function () { return "xx"; },
	load: function (d, m, name) { return { "a.b": "from " + name }; } };
Kit.eq(Shell.strings(cfg, "", fake_locale), { locale: "xx", strings: { "a.b": "from xx" } },
	"strings with the given Locale");
// The page's own texts have built-in English for when no locale file can
// be read: never a raw key on the page
var PAGE_KEYS = NAME_KEYS.concat(["main.label_fallback", "main.button_reload"]);
for (i = 0; i < PAGE_KEYS.length; i++) {
	var builtin = Shell.text(null, PAGE_KEYS[i], { name: "Mail" });
	Kit.assert(builtin !== PAGE_KEYS[i] && builtin.indexOf("{name}") < 0, PAGE_KEYS[i] + " built in: " + builtin);
	Kit.eq(builtin, Shell.text(s.strings, PAGE_KEYS[i], { name: "Mail" }), PAGE_KEYS[i] + " built in as in en_us");
}

// --- boot with the caller's Api library (the shell page passes the one
// it loaded); a failed boot still carries the locale strings, so the
// sign-in form is never shown with raw keys
var dispatched = 0;
var fake_api = {
	dispatch: function (e, req) {
		dispatched++;
		Kit.eq(e.has_header, true, "fake dispatch: header assumed");
		return { error: "internal", message: "Broken", results: [], state: "" };
	}
};
b = Shell.boot(make_env(), fake_api);
Kit.eq(dispatched, 1, "the given Api is used");
Kit.eq([b.ok, b.error, b.message, b.signed_out], [false, "internal", "Broken", false], "failed boot");
Kit.eq(b.locale, "en_us", "failed boot: the locale");
Kit.eq(b.strings["main.button_sign_in"], "Sign in", "failed boot: the strings");
var failing_call = {
	dispatch: function () {
		return { results: [[1, { ok: false, error: "internal", message: "Oops" }]], state: "",
			signed_out: true };
	}
};
b = Shell.boot(make_env(), failing_call);
Kit.eq([b.ok, b.error, b.signed_out], [false, "internal", true], "failed session.get");
Kit.eq(b.strings["main.button_sign_in"], "Sign in", "failed session.get: the strings");

// --- host and product_name: {system} and {host}, filled per request
var SYS = { name: "Scratch BBS", inet_addr: "bbs.example.org" };
Kit.eq(Shell.host("Mail.Example.com:8443", SYS), "mail.example.com", "port stripped, lowercased");
Kit.eq(Shell.host("mail.example.com", SYS), "mail.example.com", "plain host");
Kit.eq(Shell.host("localhost", SYS), "localhost", "single label");
Kit.eq(Shell.host("127.0.0.1:80", SYS), "127.0.0.1", "IPv4 literal is letters, digits and dots");
var long_host = new Array(61).join("abcd.") + "com";	// 303 characters
var hostile = ["<script>", "a b", "evil.com/x", "[::1]", "[::1]:80", long_host, "", undefined, null,
	".example.com", "example.com.", "-a.example.com", "a-.example.com", "a..b", "a.b:", "a.b:x",
	"a_b.example", "a.b\r\nX: y", "caf\xC3\xA9.example", 5];
for (var i = 0; i < hostile.length; i++)
	Kit.eq(Shell.host(hostile[i], SYS), "bbs.example.org", "falls back for " + JSON.stringify(hostile[i]));
Kit.eq(Shell.host("<script>", { name: "Scratch BBS", inet_addr: " Mail.Example.ORG " }), "mail.example.org",
	"system address trimmed and lowercased");
Kit.eq(Shell.host("<script>", { name: "Scratch BBS", inet_addr: "" }), "Scratch BBS",
	"no system address: the BBS name");
Kit.eq(Shell.host(new Array(254).join("a"), SYS), new Array(254).join("a"), "253 characters accepted");
Kit.eq(Shell.host(new Array(255).join("a"), SYS), "bbs.example.org", "254 characters refused");
Kit.eq(Shell.host(new Array(64).join("a") + ".com", SYS), new Array(64).join("a") + ".com", "long label kept");

Kit.eq(Shell.product_name("{system} Webmail", "x.example", SYS), "Scratch BBS Webmail", "the default");
Kit.eq(Shell.product_name(cfg_default().product_name, "", SYS), "Scratch BBS Webmail", "the stock default");
Kit.eq(Shell.product_name("{host} Webmail", "Mail.Example.com:8443", SYS), "mail.example.com Webmail", "{host}");
Kit.eq(Shell.product_name("{host} Webmail", "<script>", SYS), "bbs.example.org Webmail", "{host} hostile");
Kit.eq(Shell.product_name("Family Mail", "<script>", SYS), "Family Mail", "no placeholder: unchanged");
Kit.eq(Shell.product_name("Mail {other} {Host} {system", "h.example", SYS), "Mail {other} {Host} {system",
	"other braces kept literally");
Kit.eq(Shell.product_name("{system}{system} {host}", "h.example", SYS), "Scratch BBSScratch BBS h.example",
	"every occurrence");
Kit.eq(Shell.product_name("{system}", "", { name: "$& $1", inet_addr: "" }), "$& $1", "replacement taken literally");
// system.name is CP437: converted to UTF-8 unless ASCII or valid UTF-8
Kit.eq(Shell.product_name("{system}", "", { name: "Caf\x82", inet_addr: "" }), "Caf\xC3\xA9", "CP437 converted");
Kit.eq(Shell.product_name("{system}", "", { name: "Caf\xC3\xA9", inet_addr: "" }), "Caf\xC3\xA9", "UTF-8 kept");
Kit.eq(Shell.host("<script>", { name: "Caf\x82", inet_addr: "" }), "Caf\xC3\xA9", "fallback name converted too");
Kit.eq(Shell.html(Shell.product_name("{system}", "", { name: "A<B>&", inet_addr: "" })), "A&#60;B&#62;&#38;",
	"escaped where written");
// With no system object, the real one is used
Kit.eq(Shell.product_name("{system}", ""), Shell.product_name("{system}", "", system), "default system object");

// --- html escaping and lang
Kit.eq(Shell.html("<a href=\"x\">&'</a>"), "&#60;a href=&#34;x&#34;&#62;&#38;&#39;&#60;/a&#62;", "html");
Kit.eq(Shell.html("caf\xC3\xA9"), "caf\xC3\xA9", "UTF-8 bytes unchanged");
// The web server's write() keeps only the low byte of a character above
// 0xFF, so U+013C would go out as "<": such characters are references
var WIDE = String.fromCharCode(0x13C) + String.fromCharCode(0x2028) + String.fromCharCode(0xFFFF);
Kit.eq(Shell.html("a" + WIDE + "b"), "a&#316;&#8232;&#65535;b", "characters above 0xFF as references");
Kit.eq(Shell.lang("en_us"), "en-US", "lang en_us");
Kit.eq(Shell.lang("de"), "de", "lang de");
Kit.eq(Shell.lang("../x"), "en", "lang junk");
Kit.eq(Shell.lang(undefined), "en", "lang none");

// --- embed
// U+2028 and U+2029 from character codes: this engine reads a \u escape
// above 0xFF in source as UTF-8 bytes
var LS = String.fromCharCode(0x2028);
var PS = String.fromCharCode(0x2029);
var LS8 = "\xE2\x80\xA8";	// the same two as UTF-8 bytes
var PS8 = "\xE2\x80\xA9";
var nasty = { a: "</script><b>&", c: "<!-- x -->", d: LS + PS, d8: LS8 + PS8, e: "&amp;",
	f: "caf\xC3\xA9 \xE2\x80\x94 \xC2\xA9\xC2\xA8", g: ["<", { h: ">" }] };
var json = Shell.embed(nasty);
var left = new RegExp("[<>&" + LS + PS + "]|" + LS8 + "|" + PS8);
Kit.eq(left.test(json), false, "no <, >, &, U+2028 or U+2029 left");
Kit.eq(json.indexOf("</script"), -1, "cannot end the script element");
Kit.eq(json.indexOf("<!--"), -1, "cannot open a comment");
Kit.assert(json.indexOf("\\u003c/script\\u003e") >= 0, "< and > escaped");
Kit.assert(json.indexOf("\\u0026") >= 0, "& escaped");
Kit.assert(json.indexOf("\"d\":\"\\u2028\\u2029\"") >= 0, "U+2028 and U+2029 escaped");
Kit.assert(json.indexOf("\"d8\":\"\\u2028\\u2029\"") >= 0, "their UTF-8 forms escaped");
Kit.assert(json.indexOf("caf\xC3\xA9 \xE2\x80\x94 \xC2\xA9\xC2\xA8") >= 0, "other UTF-8 bytes unchanged");
var back = JSON.parse(json);
Kit.eq(back.a, nasty.a, "round trip a");
Kit.eq(back.c, nasty.c, "round trip c");
Kit.eq(back.d, nasty.d, "round trip d");
Kit.eq(back.d8, LS + PS, "UTF-8 form parses as the characters (as a browser reads it)");
Kit.eq(back.f, nasty.f, "round trip f");
Kit.eq(back.g, nasty.g, "round trip g");
Kit.eq(JSON.parse(Shell.embed({ a: "</script><b>&" })).a, "</script><b>&", "markup and an ampersand round trip");
Kit.eq(Shell.embed({ a: "</script><b>&" }).indexOf("<"), -1, "markup and an ampersand: no < left");
var wide = Shell.embed({ w: "a" + WIDE + "b", u: "caf\xC3\xA9" });
Kit.eq(/[^\x00-\xFF]/.test(wide), false, "no character above 0xFF left in embed");
Kit.assert(wide.indexOf("a\\u013c\\u2028\\uffffb") >= 0, "escaped as \\uXXXX: " + wide);
Kit.eq(JSON.parse(wide).w, "a" + WIDE + "b", "wide characters round trip");
Kit.eq(JSON.parse(wide).u, "caf\xC3\xA9", "bytes round trip");

// --- version and asset: one version for every script and stylesheet (the
// newest modification time among the files under js/ and css/), in the
// path, so modules imported by relative URL share it
var scr = Kit.scratch();
var dir = scr.dir + "root/";
function put(rel, mtime)
{
	var f = new File(dir + rel);
	Kit.assert(f.open("w"), "create " + rel);
	f.write("x");
	f.close();
	Kit.assert(file_utime(dir + rel, mtime, mtime), "set mtime " + rel);
}
try {
	mkpath(dir);
	Kit.eq(Shell.version(dir), 0, "no js/ or css/: version 0");
	mkpath(dir + "css");
	mkpath(dir + "js/sub");
	put("css/style.css", 1700000000);
	put("js/app.js", 1700000050);
	put("js/rpc.js", 1700000020);
	put("js/sub/deep.js", 1700000010);
	put("index.xjs", 1800000000);	// outside js/ and css/: not counted
	Kit.eq(Shell.version(dir), 1700000050, "newest of js/ and css/");
	var u1 = Shell.asset(dir, "css/style.css");
	Kit.eq(u1, "v/1700000050/css/style.css", "versioned path");
	Kit.eq(Shell.asset(dir, "js/app.js", 42), "v/42/js/app.js", "given version");
	put("js/rpc.js", 1700000100);	// touching one module
	Kit.eq(Shell.version(dir), 1700000100, "a touched module raises the version");
	var u2 = Shell.asset(dir, "css/style.css");
	Kit.assert(u1 !== u2, "every asset URL changes");
	Kit.eq(u2, "v/1700000100/css/style.css", "new version");
	put("js/sub/deep.js", 1700000200);
	Kit.eq(Shell.version(dir), 1700000200, "files in sub-directories count");
	put("css/custom.css", 1700000300);
	Kit.eq(Shell.version(dir), 1700000300, "custom.css counts");
} finally {
	scr.cleanup();
}

// --- headers
var POLICY = "default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
	+ "img-src 'self' data: blob: https:; connect-src 'self'; frame-src 'self' about:; "
	+ "form-action 'self'; base-uri 'none'; frame-ancestors 'none'";
var h = Shell.headers(cfg, true);
Kit.eq(h["Content-Security-Policy"], POLICY, "policy");
Kit.eq(h["X-Content-Type-Options"], "nosniff", "nosniff");
Kit.eq(h["Referrer-Policy"], "no-referrer", "referrer policy");
Kit.eq(h["Cache-Control"], "no-store", "no-store");
Kit.eq(cfg.hsts_max_age, 0, "hsts off by default");
Kit.eq(own(h, "Strict-Transport-Security"), false, "no HSTS by default, secure");
Kit.eq(own(Shell.headers(cfg, false), "Strict-Transport-Security"), false, "no HSTS by default, insecure");
var hsts = Config.load({ directory: WEBMAIL_DIR, hsts_max_age: 31536000 });
Kit.eq(Shell.headers(hsts, true)["Strict-Transport-Security"], "max-age=31536000", "HSTS when set and secure");
Kit.eq(own(Shell.headers(hsts, false), "Strict-Transport-Security"), false, "no HSTS over http");
var keys = [];
for (var k in Shell.headers(hsts, true))
	keys.push(k);
Kit.eq(keys.length, 5, "exactly five headers with HSTS");
