// Config: the web client's settings.
//
// var Config = load("config.js");
// var cfg = Config.load([raw_options [, max_inactivity]]);
//
// With no argument the options come from load("modopts.js", "webmail"):
// the root section of ctrl/modopts/webmail.ini if that file exists (a
// [webmail] section in ctrl/modopts.ini is then NOT read), else the
// [webmail] section of ctrl/modopts.ini; it returns an object, or
// null/undefined when nothing is configured.  modopts converts a value
// that looks like a number or true/false, but leaves "yes"/"no"/"on"/"off"
// and negative numbers as text, so every value is converted again here.
// A caller (a test) may pass the raw options object instead.
//
// max_inactivity is the web server's idle limit in seconds; when it is not
// passed, server.max_inactivity is read (undefined under jsexec and in a
// web server older than v3.22).  It sets the default poll_interval: half
// the limit, rounded down, at least 15 and at most 60 (min(60, max(15,
// floor(limit / 2)))), so an idle page's check does not arrive just as
// the server closes the kept-alive connection, and a long idle limit never
// makes mail checks rarer than once a minute; 60 when the limit is not a
// positive number.  A configured poll_interval always
// wins (and keeps its minimum).
//
// The result has every key below, with the default when the option is
// absent or invalid; unknown keys are ignored:
//   numbers (whole, 0 or more; a negative or non-numeric value is invalid;
//     poll_interval is at least 15, session_timeout at least 60 and
//     remember_days at least 1)
//   booleans (true/false, yes/no, on/off, 1/0, any case)
//   strings (a value modopts turned into a number or boolean is used in
//     its string form)
//   fallback_url: an http:// or https:// URL or a relative path; any
//     other value (another scheme such as javascript:, a scheme-relative
//     //host URL, white space or control characters) is unusable
//   directory: a string; a relative one is resolved against
//     system.exec_dir; the result ends with a path separator
//
// A value that is present (not empty) but unusable gets the default and one
// warning naming the key, through Config.warn(text) (which logs at
// LOG_WARNING; a test may replace it).
//
// max_attachment_bytes, when not set, is Config.attachment_allowance() of
// max_message_bytes (the most a message may carry as files at all), so it
// is never a limit that cannot bind; 26214400 when max_message_bytes is 0.
//
// Config.attachment_allowance(max_message_bytes): the largest file, in
// bytes, whose message (with no text) MimeWrite.estimate() puts at or
// under max_message_bytes.  The same arithmetic as the estimate: a file
// of n bytes is c = ceil(n / 3) * 4 base64 characters plus a line break
// (2 bytes) every 76 characters, plus 1024 bytes for its part's header,
// plus 1024 for the (empty) text part: c + 2 * ceil(c / 76) + 2048.  A
// test compares it with MimeWrite.estimate() over a table of sizes; the
// browser checks files with a copy of the same formula (js/compose.js).
//
// max_body_bytes above MAX_BODY_FIT (the largest message text a draft.save
// request can carry within the web server's 4 MiB request limit, by the
// API's arithmetic: see Api.body_allowance) gets MAX_BODY_FIT and one
// warning, so a draft the limit allows can always be saved.

"use strict";

var Config = (function () {
	var NUMBERS = {
		session_timeout: 7200,
		remember_days: 30,
		poll_interval: 60,
		max_attachment_bytes: 26214400,
		max_text_part_bytes: 1048576,
		max_drafts: 50,
		max_draft_bytes: 104857600,	// all of a user's drafts on disk: attachments and text
		max_body_bytes: 1048576,
		max_recipients: 50,
		max_message_bytes: 20971520,	// a message being sent, as stored (attachments encoded)
		hsts_max_age: 0
	};
	// Api.body_allowance("draft.save") = 2 * (max_body_bytes + 3 * 16384
	// + 4096 + 253 + 4) + 16384, at most 4 MiB (MAX_POST_LEN in
	// websrvr.cpp); a test pins this to the API's figure
	var MAX_BODY_FIT = Math.floor((4 * 1048576 - 16384) / 2) - (3 * 16384 + 4096 + 253 + 4);
	var MINIMUMS = {
		poll_interval: 15,
		session_timeout: 60,
		remember_days: 1
	};
	var BOOLEANS = {
		webv4_login: false,
		webv4_logout: false,
		require_tls: true
	};
	var STRINGS = {
		access_requirements: "",
		tls_proxy_header: "",
		locale: "en_us",
		product_name: "{system} WebMail",	// {system} and {host}: see Shell.product_name
		brand: "",
		login_text: "",
		footer_links: ""
	};

	function present(v)
	{
		return v !== undefined && v !== null && v !== "";
	}

	function bad(key)
	{
		Config.warn("webmail option " + key + " has an unusable value; using the default");
	}

	function trim(v)
	{
		return v.replace(/^[ \t\r\n]+|[ \t\r\n]+$/g, "");
	}

	function to_number(key, v, def)
	{
		if (!present(v))
			return def;
		var n;
		if (typeof v === "string") {
			v = trim(v);
			if (/^[0-9]{1,15}$/.test(v))
				n = parseInt(v, 10);
		} else if (typeof v === "number" && isFinite(v) && v >= 0 && Math.floor(v) === v)
			n = v;
		if (n === undefined) {
			bad(key);
			return def;
		}
		if (MINIMUMS[key] !== undefined && n < MINIMUMS[key]) {
			Config.warn("webmail option " + key + " is below its minimum of " + MINIMUMS[key] + "; using the minimum");
			return MINIMUMS[key];
		}
		return n;
	}

	function to_boolean(key, v, def)
	{
		if (!present(v))
			return def;
		if (typeof v === "boolean")
			return v;
		if (typeof v === "number") {
			if (v === 1)
				return true;
			if (v === 0)
				return false;
		} else if (typeof v === "string") {
			v = trim(v).toLowerCase();
			if (v == "true" || v == "yes" || v == "on" || v == "1")
				return true;
			if (v == "false" || v == "no" || v == "off" || v == "0")
				return false;
		}
		bad(key);
		return def;
	}

	function to_string(key, v, def)
	{
		if (!present(v))
			return def;
		if (typeof v === "string")
			return v;
		if (typeof v === "number" || typeof v === "boolean")
			return String(v);
		bad(key);
		return def;
	}

	// fallback_url: an http:// or https:// URL, or a relative path (no
	// scheme, not scheme-relative); anything else is unusable, so a sysop
	// typo can never put a javascript: or data: link on the page
	function to_url(v)
	{
		var s = to_string("fallback_url", v, "");
		if (s === "")
			return "";
		s = trim(s);
		var absolute = /^https?:\/\/[^\/\\]/i.test(s);
		var relative = !/^[A-Za-z][A-Za-z0-9+.\-]*:/.test(s) && !/^[\/\\]{2}/.test(s);
		if (s === "" || /[\x00-\x20\x7f]/.test(s) || !(absolute || relative)) {
			bad("fallback_url");
			return "";
		}
		return s;
	}

	function slash(p)
	{
		var c = p.charAt(p.length - 1);
		return (c == '/' || c == '\\') ? p : p + '/';
	}

	function to_directory(v)
	{
		var def = system.exec_dir + "../webmail/";
		if (!present(v))
			return def;
		if (typeof v !== "string") {
			bad("directory");
			return def;
		}
		v = trim(v);
		if (v === "")
			return def;
		if (!/^([\/\\]|[A-Za-z]:)/.test(v))
			v = system.exec_dir + v;
		return slash(v);
	}

	// The bytes MimeWrite.estimate() gives a message of one file of n
	// bytes and no text
	function encoded(n)
	{
		var c = Math.ceil(n / 3) * 4;
		return c + 2 * Math.ceil(c / 76) + 2048;
	}

	function attachment_allowance(max_bytes)
	{
		if (!(max_bytes > 2048))
			return 0;
		var lo = 0;
		var hi = max_bytes;
		while (lo < hi) {
			var mid = Math.floor((lo + hi + 1) / 2);
			if (encoded(mid) <= max_bytes)
				lo = mid;
			else
				hi = mid - 1;
		}
		return lo;
	}

	// The default poll_interval for a web server idle limit of
	// max_inactivity seconds
	function poll_default(max_inactivity)
	{
		if (typeof max_inactivity !== "number" || !isFinite(max_inactivity) || max_inactivity <= 0)
			return NUMBERS.poll_interval;
		return Math.min(NUMBERS.poll_interval, Math.max(MINIMUMS.poll_interval, Math.floor(max_inactivity / 2)));
	}

	function load_config(raw, max_inactivity)
	{
		if (arguments.length < 1)
			raw = load("modopts.js", "webmail");
		if (arguments.length < 2)
			max_inactivity = js.global.server ? js.global.server.max_inactivity : undefined;
		if (raw === null || typeof raw !== "object")
			raw = {};
		var cfg = {};
		var k;
		for (k in NUMBERS)
			cfg[k] = to_number(k, raw[k], k === "poll_interval" ? poll_default(max_inactivity) : NUMBERS[k]);
		for (k in BOOLEANS)
			cfg[k] = to_boolean(k, raw[k], BOOLEANS[k]);
		for (k in STRINGS)
			cfg[k] = to_string(k, raw[k], STRINGS[k]);
		if (cfg.max_body_bytes > MAX_BODY_FIT) {
			Config.warn("webmail option max_body_bytes is above " + MAX_BODY_FIT
				+ ", the most a request can carry; using " + MAX_BODY_FIT);
			cfg.max_body_bytes = MAX_BODY_FIT;
		}
		if (!present(raw.max_attachment_bytes) && cfg.max_message_bytes > 0)
			cfg.max_attachment_bytes = attachment_allowance(cfg.max_message_bytes);
		cfg.fallback_url = to_url(raw.fallback_url);
		cfg.directory = to_directory(raw.directory);
		return cfg;
	}

	var Config = {
		MAX_BODY_FIT: MAX_BODY_FIT,
		attachment_allowance: attachment_allowance,
		load: load_config,
		warn: function (text) {
			log(LOG_WARNING, text);
		}
	};

	return Config;
})();

Config;
