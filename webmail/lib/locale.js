// Locale: choosing a locale for a user and loading its strings.
//
// var Locale = load("locale.js");
// var name = Locale.pick(user_lang, accept_language, sysop_default,
//                        Locale.available(dir, mods_dir));
// var text = Locale.load(dir, mods_dir, name);   // {"section.key": "text"}
//
// A locale name is lowercase "ll" or "ll_cc" (letters and one underscore,
// as in webv4's lib/locale/): /^[a-z]{2,3}(_[a-z]{2,4})?$/.  Input such as
// "fr-CA" or "FR_ca" is normalized to "fr_ca" (lowercase, "-" to "_");
// anything else is not a locale name and is treated as "en_us".  A name is
// validated before it is used in a path.
//
// The strings are in sectioned ini files, "key = text":
//   <dir>/locale/en_us.ini        always read first (required)
//   <dir>/locale/<name>.ini       the chosen locale (optional)
//   <mods_dir>/locale/<name>.ini  the sysop's overlay (optional), holding
//                                 only the keys to change
// each overriding the one before.  <dir> and <mods_dir> end with a path
// separator.
//
// File.iniGetObject() is NOT used: it turns text that looks like a number or
// true/false into a number or boolean and treats quotes and ";" in values
// specially, so the files are parsed here instead.  File format:
//   - UTF-8; a leading byte order mark is ignored.  Values are returned as
//     UTF-8 byte strings, unchanged.
//   - blank lines and lines whose first non-blank character is ";" or "#"
//     are comments; a ";" or "#" later in a line is part of the value.
//   - "[section]" starts a section; the name is trimmed and lowercased.
//   - "key = text": split at the FIRST "=", so a value may contain "=".  The
//     key is trimmed and lowercased.  Spaces and tabs around the value are
//     removed (so a value cannot start or end with a space); nothing else
//     is changed: no quotes, no escapes, no type conversion.
//   - a section or key that is not lowercase letters, digits and underscores
//     is skipped, as is a key before any section or a line without "=".
// Placeholders such as {count} are left in the text for the caller.
//
// Plurals: a string that depends on a number may have a companion key with
// the suffix "_one" (label_trash_notice_one beside label_trash_notice).  The
// client uses the "_one" string when the number is exactly 1 and the plain
// key otherwise.  This library only stores the strings.  A translation may
// give the "_one" key any wording it needs; it must keep the same
// {placeholders} the plain key has, except one the singular does not use.
//
// The result of load() is a plain object: consumers must read it with
// hasOwnProperty (Object.prototype.hasOwnProperty.call(strings, key)),
// since a bare lookup of a missing key such as "constructor" finds an
// inherited function instead of undefined.
//
// A missing or unreadable file is skipped silently, except en_us.ini whose
// absence throws an Error (generic text) after logging the path through
// Locale.error(text) (which logs at LOG_ERR; a test may replace it).

"use strict";

var Locale = (function () {
	var NAME = /^[a-z]{2,3}(_[a-z]{2,4})?$/;
	var DEFAULT_NAME = "en_us";
	var MAX_HEADER = 1000;
	var MAX_ENTRIES = 20;
	var MAX_FILE = 1048576;

	function trim_blanks(s)
	{
		return s.replace(/^[ \t]+|[ \t]+$/g, "");
	}

	// ASCII-only lowercase (toLowerCase corrupts UTF-8 bytes)
	function lower(s)
	{
		return s.replace(/[A-Z]+/g, function (m) {
			return m.toLowerCase();
		});
	}

	// Normalized locale name, or 'fallback' (default "en_us") if it is not one
	function valid(name, fallback)
	{
		if (arguments.length < 2)
			fallback = DEFAULT_NAME;
		if (typeof name !== "string")
			return fallback;
		var n = lower(name).replace(/-/g, "_");
		return NAME.test(n) ? n : fallback;
	}

	function language(name)
	{
		return name.split("_")[0];
	}

	// The entry of 'available' for a (valid) wanted name: the exact name,
	// else the first with the same language; or null
	function match(wanted, available)
	{
		var i;
		for (i = 0; i < available.length; i++)
			if (available[i] === wanted)
				return wanted;
		var lang = language(wanted);
		for (i = 0; i < available.length; i++)
			if (language(available[i]) === lang)
				return available[i];
		return null;
	}

	// A locale name from a language tag such as "zh-Hant-TW", "es-419" or
	// "en-US-x-foo": the first subtag is the language; a two-letter second
	// subtag (or one after a four-letter script subtag) is the region;
	// anything else is dropped.  Returns null if the result is not a valid
	// name.
	function reduce(tag)
	{
		if (typeof tag !== "string")
			return null;
		var sub = lower(tag).split(/[-_]/);
		if (!/^[a-z]{2,3}$/.test(sub[0]))
			return null;
		var region = null;
		if (sub.length > 1 && /^[a-z]{2}$/.test(sub[1]))
			region = sub[1];
		else if (sub.length > 2 && /^[a-z]{4}$/.test(sub[1]) && /^[a-z]{2}$/.test(sub[2]))
			region = sub[2];
		else if (sub.length === 2 && /^[a-z]{3,4}$/.test(sub[1]))
			region = sub[1];	// a plain ll_cccc name
		return valid(region === null ? sub[0] : sub[0] + "_" + region, null);
	}

	// Locale names from an Accept-Language header, best first.  q=0 and
	// malformed entries are skipped; ties keep header order.
	function parse_accept(header)
	{
		var list = [];
		if (typeof header !== "string")
			return list;
		var entries = header.substr(0, MAX_HEADER).split(",", MAX_ENTRIES);
		for (var i = 0; i < entries.length; i++) {
			var parts = entries[i].split(";");
			var tag = reduce(trim_blanks(parts[0]));
			if (tag === null)
				continue;
			var q = 1;
			var ok = true;
			for (var j = 1; j < parts.length; j++) {
				var m = /^[ \t]*[qQ][ \t]*=[ \t]*([^ \t]*)[ \t]*$/.exec(parts[j]);
				if (m === null)
					continue;
				if (/^(0(\.[0-9]{0,3})?|1(\.0{0,3})?)$/.test(m[1]))
					q = parseFloat(m[1]);
				else
					ok = false;
			}
			if (ok && q > 0)
				list.push({ name: tag, q: q, order: i });
		}
		list.sort(function (a, b) {
			return (b.q - a.q) || (a.order - b.order);
		});
		return list.map(function (e) {
			return e.name;
		});
	}

	// The locale for a user: the BBS language setting, then the browser's
	// languages, then the sysop's default, then en_us.  Each wanted name
	// matches exactly or by language ("es" or "es_mx" match "es_ar").
	function pick(user_lang, accept_language, sysop_default, available)
	{
		if (!(available instanceof Array))
			available = [];
		var wanted = [];
		var u = reduce(user_lang);
		if (u !== null)
			wanted.push(u);
		wanted = wanted.concat(parse_accept(accept_language));
		var d = reduce(sysop_default);
		if (d !== null)
			wanted.push(d);
		for (var i = 0; i < wanted.length; i++) {
			var m = match(wanted[i], available);
			if (m !== null)
				return m;
		}
		return DEFAULT_NAME;
	}

	// Locale names that have an ini file in either directory, sorted
	function list_names(dir, found)
	{
		if (typeof dir !== "string" || dir === "")
			return;
		var files = directory(dir + "locale/*.ini");
		for (var i = 0; i < files.length; i++) {
			var m = /([^\/\\]*)\.ini$/.exec(files[i]);
			if (m !== null && NAME.test(m[1]))
				found[m[1]] = true;
		}
	}

	function available(dir, mods_dir)
	{
		var found = {};
		list_names(dir, found);
		list_names(mods_dir, found);
		var names = [];
		for (var n in found)
			names.push(n);
		return names.sort();
	}

	// Parse one ini file into 'into'.  Returns false if it cannot be read.
	function read_file(path, into)
	{
		var f = new File(path);
		if (!f.open("rb"))
			return false;
		var text = f.length <= MAX_FILE ? f.read(f.length) : undefined;
		f.close();
		if (typeof text !== "string")
			return false;
		if (text.substr(0, 3) === "\xef\xbb\xbf")
			text = text.substr(3);
		var lines = text.split("\n");
		var section = null;
		for (var i = 0; i < lines.length; i++) {
			var line = lines[i].replace(/\r$/, "");
			var t = trim_blanks(line);
			if (t === "" || t.charAt(0) === ';' || t.charAt(0) === '#')
				continue;
			if (t.charAt(0) === '[') {
				var s = /^\[([^\]]*)\]/.exec(t);
				if (s === null)
					section = null;
				else {
					section = lower(trim_blanks(s[1]));
					if (!/^[a-z0-9_]+$/.test(section))
						section = null;
				}
				continue;
			}
			var eq = line.indexOf('=');
			if (eq < 0 || section === null)
				continue;
			var key = lower(trim_blanks(line.substr(0, eq)));
			if (!/^[a-z0-9_]+$/.test(key))
				continue;
			into[section + "." + key] = trim_blanks(line.substr(eq + 1));
		}
		return true;
	}

	function load_strings(dir, mods_dir, name)
	{
		name = valid(name);
		var strings = {};
		var base = dir + "locale/" + DEFAULT_NAME + ".ini";
		if (!read_file(base, strings)) {
			Locale.error("webmail: cannot read " + base);
			throw new Error("The default locale strings are not available");
		}
		if (name !== DEFAULT_NAME)
			read_file(dir + "locale/" + name + ".ini", strings);
		if (typeof mods_dir === "string" && mods_dir !== "")
			read_file(mods_dir + "locale/" + name + ".ini", strings);
		return strings;
	}

	var Locale = {
		valid: valid,
		pick: pick,
		available: available,
		load: load_strings,
		error: function (text) {
			log(LOG_ERR, text);
		}
	};

	return Locale;
})();

Locale;
