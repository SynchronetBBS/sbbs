// Config: the web client's settings.
//
// var Config = load("config.js");
// var cfg = Config.load([raw_options]);
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

"use strict";

var Config = (function () {
	var NUMBERS = {
		session_timeout: 7200,
		remember_days: 30,
		poll_interval: 60,
		max_attachment_bytes: 26214400,
		max_text_part_bytes: 1048576,
		max_drafts: 50,
		max_draft_bytes: 104857600,
		hsts_max_age: 0
	};
	var MINIMUMS = {
		poll_interval: 15,
		session_timeout: 60,
		remember_days: 1
	};
	var BOOLEANS = {
		webv4_login: false,
		require_tls: true
	};
	var STRINGS = {
		access_requirements: "",
		tls_proxy_header: "",
		locale: "en_us",
		product_name: "{system} Webmail",	// {system} and {host}: see Shell.product_name
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

	function load_config(raw)
	{
		if (arguments.length < 1)
			raw = load("modopts.js", "webmail");
		if (raw === null || typeof raw !== "object")
			raw = {};
		var cfg = {};
		var k;
		for (k in NUMBERS)
			cfg[k] = to_number(k, raw[k], NUMBERS[k]);
		for (k in BOOLEANS)
			cfg[k] = to_boolean(k, raw[k], BOOLEANS[k]);
		for (k in STRINGS)
			cfg[k] = to_string(k, raw[k], STRINGS[k]);
		cfg.fallback_url = to_url(raw.fallback_url);
		cfg.directory = to_directory(raw.directory);
		return cfg;
	}

	var Config = {
		load: load_config,
		warn: function (text) {
			log(LOG_WARNING, text);
		}
	};

	return Config;
})();

Config;
