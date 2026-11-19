// Shell: the logic of the app's shell page (webmail/root/index.xjs), kept
// here so it can be tested without a web server.
//
// var Shell = load("shell.js");
// var boot = Shell.boot(env);          // the session.get result
// var json = Shell.embed(boot);        // for <script type="application/json">
// var url = Shell.asset(root_dir, "css/style.css");   // "v/<n>/css/style.css"
// var headers = Shell.headers(config, secure);        // { name: value }
//
// Shell.boot(env [, Api])
//   env is the env Api.dispatch takes (see api.js); it is not changed.
//   Api is the api.js library the caller already loaded (default: loaded
//   from beside this file on first use).  Returns what Api.dispatch
//   returns for a request of one session.get call: { ok: true, user (or
//   null), insecure, locale, strings, domains, limits, poll_ms,
//   powered_by, product_name, brand, login_text, footer_links,
//   fallback_url [, via] }, plus signed_out (the reply's flag: the browser
//   sent a session cookie that is no longer valid).  The shell page is the
//   app's own document, fetched by a navigation that cannot carry the
//   X-Webmail header, so the dispatch is told the header was present.
//   When the call fails the result is { ok: false, error, message,
//   signed_out, locale, strings } (Shell.strings: strings null when they
//   cannot be read either).  The caller must already have answered a
//   request that needs HTTPS and is not secure (Shell.needs_https), since
//   session.get would fail with "https" then.
//
// Shell.needs_https(config, secure)
//   true when config.require_tls is on and the request is not secure.
//
// Shell.strings(config, accept_language [, Locale])
//   { locale, strings } for a page written without a session (the "needs
//   HTTPS" page): the locale the browser prefers among those installed,
//   else config.locale; strings is null when they cannot be read.  Locale
//   is the locale.js library the caller already has (Api.Locale; default:
//   loaded from beside this file on first use).
//
// Shell.host(host_header [, sys])
//   the request's host name for {host}: the Host header without its port,
//   lowercased, when it is a plain host name (letters, digits, hyphens and
//   dots; labels neither empty nor starting or ending with a hyphen; at
//   most 253 characters).  The header is the client's to choose, so
//   anything else (an IPv6 literal, a path, markup, white space) is
//   refused, and so is a missing header: then sys.inet_addr (trimmed,
//   lowercased), or the BBS name when that is empty.  sys is
//   { name, inet_addr } (default: the system object).
//
// Shell.product_name(setting [, host_header [, sys]])
//   the product_name setting for this request: each "{system}" replaced
//   by the BBS name (sys.name, CP437 converted to UTF-8 unless it is
//   ASCII or valid UTF-8) and each "{host}" by Shell.host(host_header,
//   sys).  Nothing else in the text is touched (another "{" stays as it
//   is).  The caller puts the result in the config for the request, so
//   the page title, the header, session.get and the {name} in messages
//   all carry the same name; it is plain text, escaped where written.
//
// Shell.text(strings, key, vars)
//   strings[key] when it is a string of its own, else the built-in English
//   text of one of the texts the page itself writes (the unsupported-
//   browser, needs-HTTPS, JavaScript-required, fallback-link and reload
//   texts), else the key itself, with
//   each {name} replaced by vars[name] when vars has it (taken literally).
//
// Shell.html(text)
//   text with &, <, >, " and ' and every character above 0xFF written as
//   character references, for an element's text or a quoted attribute
//   value.  The web server's write() keeps only the low byte of a
//   character above 0xFF (U+013C would go out as "<"), so none may reach
//   it; UTF-8 byte strings (characters 0x80 to 0xFF) are left as they are.
//
// Shell.lang(locale)
//   the value for <html lang>: "en_us" -> "en-US", "de" -> "de"; "en" for
//   anything that is not a locale name.
//
// Shell.embed(obj)
//   JSON.stringify(obj) with every <, >, &, U+2028 and U+2029 written as a
//   \uXXXX escape, so the text cannot end the script element, start a
//   comment or character reference, or break a script parser; JSON.parse
//   of the result gives back the same object.  U+2028 and U+2029 are
//   escaped both as characters and in their UTF-8 byte form (the form
//   they have in a UTF-8 byte string), and every other character above
//   0xFF is written as \uXXXX too (see Shell.html); other bytes are left
//   as they are.
//
// Shell.version(root_dir)
//   the newest modification time among the files under root_dir's js/
//   and css/ directories (sub-directories included), or 0 when there are
//   none: one version for all the app's scripts and stylesheets.
//
// Shell.asset(root_dir, rel_path [, version])
//   "v/" + version + "/" + rel_path (version defaults to
//   Shell.version(root_dir)).  The version is in the PATH, not a query, so
//   the modules that app.js imports by relative URL get the same version
//   and a changed file anywhere under js/ or css/ gives every one of them
//   a new URL; webctrl.ini's Rewrite rule serves v/<digits>/js/... and
//   v/<digits>/css/... from js/ and css/, so a page loaded before an
//   upgrade keeps working.  root_dir ends with a path separator; rel_path
//   uses "/".
//
// Shell.headers(config, secure)
//   the response headers of the shell page: Content-Security-Policy,
//   X-Content-Type-Options, Referrer-Policy, Cache-Control, and
//   Strict-Transport-Security only when secure is true and
//   config.hsts_max_age is above 0 (it pins the whole host name to HTTPS,
//   which is the sysop's decision).

"use strict";

var Shell = (function () {
	var HERE = (function () {
		var f = String(new Error().fileName || "");
		var i = Math.max(f.lastIndexOf('/'), f.lastIndexOf('\\'));
		return i >= 0 ? f.substr(0, i + 1) : "";
	})();

	var LOCALE_NAME = /^([a-z]{2,3})(?:_([a-z]{2,4}))?$/;

	var CSP = "default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
		+ "img-src 'self' data: blob: https:; connect-src 'self'; frame-src 'self' about:; "
		+ "form-action 'self'; base-uri 'none'; frame-ancestors 'none'";

	var api = null;
	var locale = null;

	// The page's own texts, for when no locale file can be read (the same
	// as en_us.ini)
	var BUILTIN = {
		"main.label_unsupported_browser": "{name} could not start. Please reload the page. "
			+ "If this keeps happening, your browser may be out of date.",
		"main.label_needs_https": "{name} needs a secure connection. Please use the https address "
			+ "of this site.",
		"main.label_javascript_required": "{name} could not start because JavaScript is turned off. "
			+ "Please turn it on and reload the page.",
		"main.label_fallback": "Open the simple mail page",
		"main.button_reload": "Reload the page"
	};

	function sibling(name)
	{
		var path = HERE + name;
		if (HERE === "" || !file_exists(path))
			path = name;
		return load({}, path);
	}

	function Api()
	{
		if (api === null)
			api = sibling("api.js");
		return api;
	}

	function Locale()
	{
		if (locale === null)
			locale = api !== null && api.Locale ? api.Locale : sibling("locale.js");
		return locale;
	}

	function own(obj, key)
	{
		return obj !== null && typeof obj === "object" && Object.prototype.hasOwnProperty.call(obj, key);
	}

	function boot(env, given)
	{
		if (given !== undefined && given !== null)
			api = given;
		var e = {};
		for (var k in env) {
			if (own(env, k))
				e[k] = env[k];
		}
		e.has_header = true;
		var reply = Api().dispatch(e, { calls: [["session.get", null, 1]] });
		var signed_out = reply.signed_out === true;
		var r;
		if (reply.results instanceof Array && reply.results.length === 1)
			r = reply.results[0][1];
		else
			r = { ok: false, error: reply.error || "internal", message: reply.message || "" };
		r.signed_out = signed_out;
		if (r.ok !== true) {
			// The sign-in form shown with the error needs its texts
			var s = strings(env.config, env.accept_language);
			r.locale = s.locale;
			r.strings = s.strings;
		}
		return r;
	}

	function needs_https(config, secure)
	{
		return config.require_tls === true && secure !== true;
	}

	function strings(config, accept_language, given)
	{
		var L = given !== undefined && given !== null ? given : Locale();
		var dir = config.directory + "lib/";
		var mods = config.directory + "mods/";
		var name;
		var s = null;
		try {
			name = L.pick(undefined, typeof accept_language === "string" ? accept_language : "",
				config.locale, L.available(dir, mods));
			s = L.load(dir, mods, name);
		} catch (e) {
			log(LOG_ERR, "webmail shell: loading the locale strings: " + e);
			if (name === undefined)
				name = "en_us";
		}
		return { locale: name, strings: s };
	}

	var HOST_RE = /^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9-]*[a-z0-9])?)*$/;

	function utf8_of(v)
	{
		var s = v === undefined || v === null ? "" : String(v);
		if (str_is_ascii(s) || str_is_utf8(s))
			return s;
		return utf8_encode(s);
	}

	function sys_of(sys)
	{
		return sys !== null && typeof sys === "object" ? sys : system;
	}

	function host(header, sys)
	{
		sys = sys_of(sys);
		if (typeof header === "string") {
			var h = header.replace(/:[0-9]{1,5}$/, "").toLowerCase();
			if (h.length <= 253 && HOST_RE.test(h))
				return h;
		}
		var a = typeof sys.inet_addr === "string" ? sys.inet_addr.replace(/^[ \t]+|[ \t]+$/g, "") : "";
		if (a !== "")
			return a.toLowerCase();
		return utf8_of(sys.name);
	}

	function product_name(setting, header, sys)
	{
		sys = sys_of(sys);
		return String(setting).replace(/\{(system|host)\}/g, function (m, which) {
			return which === "system" ? utf8_of(sys.name) : host(header, sys);
		});
	}

	function text(s, key, vars)
	{
		var t = own(s, key) && typeof s[key] === "string" ? s[key]
			: (own(BUILTIN, key) ? BUILTIN[key] : key);
		if (vars === null || typeof vars !== "object")
			return t;
		return t.replace(/\{([a-z0-9_]+)\}/g, function (m, name) {
			return own(vars, name) ? String(vars[name]) : m;
		});
	}

	// A class from character codes (see EMBED_RE): above 0xFF
	var WIDE = "[" + String.fromCharCode(0x100) + "-" + String.fromCharCode(0xFFFF) + "]";
	var HTML_RE = new RegExp("[&<>\"']|" + WIDE, "g");

	function html(t)
	{
		return String(t).replace(HTML_RE, function (c) {
			return "&#" + c.charCodeAt(0) + ";";
		});
	}

	function lang(name)
	{
		var m = LOCALE_NAME.exec(typeof name === "string" ? name : "");
		if (!m)
			return "en";
		return m[2] ? m[1] + "-" + m[2].toUpperCase() : m[1];
	}

	// The characters embed() escapes.  Built from character codes: this
	// engine reads a \u escape above 0xFF in source as its UTF-8 bytes, so
	// a character class written with that escape would match single bytes
	// of other characters.  Strings here are UTF-8 byte strings, so the
	// UTF-8 forms of U+2028 and U+2029 (E2 80 A8, E2 80 A9) are escaped as
	// well.
	var EMBED_RE = new RegExp("[<>&]|" + WIDE + "|\xE2\x80[\xA8\xA9]", "g");

	function embed(obj)
	{
		return JSON.stringify(obj).replace(EMBED_RE, function (c) {
			if (c.length === 3)
				return c.charCodeAt(2) === 0xA8 ? "\\u2028" : "\\u2029";
			var h = c.charCodeAt(0).toString(16);
			while (h.length < 4)
				h = "0" + h;
			return "\\u" + h;
		});
	}

	function newest(dir)
	{
		var t = 0;
		var list = directory(dir + "*");
		for (var i = 0; i < list.length; i++) {
			var p = list[i];
			var c = p.charAt(p.length - 1);
			var d = (c === '/' || c === '\\') ? newest(p) : file_date(p);
			if (typeof d === "number" && d > t)
				t = d;
		}
		return t;
	}

	function version(root_dir)
	{
		return Math.max(newest(root_dir + "js/"), newest(root_dir + "css/"));
	}

	function asset(root_dir, rel_path, v)
	{
		if (typeof v !== "number")
			v = version(root_dir);
		return "v/" + v + "/" + rel_path;
	}

	function headers(config, secure)
	{
		var h = {
			"Content-Security-Policy": CSP,
			"X-Content-Type-Options": "nosniff",
			"Referrer-Policy": "no-referrer",
			"Cache-Control": "no-store"
		};
		var age = config ? config.hsts_max_age : 0;
		if (secure === true && typeof age === "number" && age > 0)
			h["Strict-Transport-Security"] = "max-age=" + age;
		return h;
	}

	return {
		boot: boot,
		needs_https: needs_https,
		host: host,
		product_name: product_name,
		strings: strings,
		text: text,
		html: html,
		lang: lang,
		embed: embed,
		version: version,
		asset: asset,
		headers: headers
	};
})();

Shell;
