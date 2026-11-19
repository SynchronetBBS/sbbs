// Locale: choosing a locale and loading its strings (webmail/lib/locale.js)

load(js.exec_dir + "kit.js");
var Locale = Kit.wlib("locale");

var logged = [];
Locale.error = function (text) {
	logged.push(text);
};

var scr = Kit.scratch();
try {
	var AV = ["en_us", "es_ar", "fr_fr", "pt"];

	// pick: the user's language, exact then by language prefix
	Kit.eq(Locale.pick("es", "", "en_us", ["en_us", "es_ar"]), "es_ar");
	Kit.eq(Locale.pick("es_ar", "", "en_us", ["en_us", "es_ar"]), "es_ar");
	Kit.eq(Locale.pick("es_mx", "", "en_us", ["en_us", "es_ar"]), "es_ar", "same language");
	Kit.eq(Locale.pick("ES-AR", "", "en_us", ["en_us", "es_ar"]), "es_ar", "normalized");
	Kit.eq(Locale.pick("fr_ca", "", "en_us", AV), "fr_fr");
	// exact wins over a prefix match listed earlier
	Kit.eq(Locale.pick("es_mx", "", "en_us", ["es_ar", "es_mx"]), "es_mx");

	// the browser's languages, in order of q
	Kit.eq(Locale.pick("", "fr-CA,fr;q=0.9,en;q=0.8", "en_us", ["en_us", "fr_fr"]), "fr_fr");
	Kit.eq(Locale.pick("", "de;q=0.5, es;q=0.9, fr;q=0.7", "en_us", AV), "es_ar");
	Kit.eq(Locale.pick("", "de, xx;q=0.9, pt-BR;q=0.8", "en_us", AV), "pt");
	// ties keep header order; a missing q is 1
	Kit.eq(Locale.pick("", "fr;q=0.8, es;q=0.8", "en_us", AV), "fr_fr");
	Kit.eq(Locale.pick("", "fr;q=0.8, es", "en_us", AV), "es_ar");
	// q=0 is ignored
	Kit.eq(Locale.pick("", "es;q=0, fr;q=0.1", "en_us", AV), "fr_fr");
	Kit.eq(Locale.pick("", "es;q=0", "pt", AV), "pt");
	// the user's BBS language wins over the browser
	Kit.eq(Locale.pick("pt", "es", "en_us", AV), "pt");
	// a user language with no match falls to the browser
	Kit.eq(Locale.pick("zz", "es", "en_us", AV), "es_ar");

	// no match: the sysop default, else en_us
	Kit.eq(Locale.pick("zz", "yy", "fr_fr", AV), "fr_fr");
	Kit.eq(Locale.pick("zz", "yy", "", AV), "en_us");
	Kit.eq(Locale.pick("zz", "yy", undefined, AV), "en_us");
	Kit.eq(Locale.pick(undefined, undefined, undefined, undefined), "en_us");
	Kit.eq(Locale.pick("", "", "../x", AV), "en_us", "bad default");
	Kit.eq(Locale.pick("", "", "FR-ca", AV), "fr_fr", "default normalized and matched");

	// malformed Accept-Language entries are skipped, never thrown
	[";;;", ",,,", "*", "es;q=abc", "es;q=", "es;q=2", "es;q=-1", ";q=1", "=", "e5",
	 "../x", "es_ar_x_y_z", "\x00\xff", "es;q=0.5;q=0.4;;", "q=1"].forEach(function (h) {
		var r = Locale.pick("", h, "en_us", AV);
		Kit.assert(typeof r === "string" && /^[a-z]{2,3}(_[a-z]{2,4})?$/.test(r), "header " + JSON.stringify(h));
	});
	Kit.eq(Locale.pick("", "es;q=abc, fr", "en_us", AV), "fr_fr");
	Kit.eq(Locale.pick("", ",,, ;, fr", "en_us", AV), "fr_fr");
	// not strings
	Kit.eq(Locale.pick(5, {}, null, AV), "en_us");
	// the header is capped at 1000 characters (one long tag, one entry)
	var longtag = "xx";
	while (longtag.length < 1000)
		longtag += "x";
	Kit.eq(Locale.pick("", longtag + ",fr", "en_us", AV), "en_us", "tag past the cap is cut off with the rest");
	Kit.eq(Locale.pick("", "fr," + longtag, "en_us", AV), "fr_fr", "text before the cap still counts");
	var pad = "";
	while (pad.length < 1000)
		pad += " ";
	Kit.eq(Locale.pick("", pad + "fr", "en_us", AV), "en_us", "cap on length");
	// and at 20 entries
	var many = "";
	for (var i = 0; i < 20; i++)
		many += "q,";
	Kit.eq(Locale.pick("", many + "fr", "en_us", AV), "en_us", "cap on entries");
	Kit.eq(Locale.pick("", many.substr(0, many.length - 2) + "fr", "en_us", AV), "fr_fr", "20th entry counts");

	// tags with more subtags are reduced
	var RAV = ["en_us", "zh_tw", "zh_cn", "es", "de_de", "fr_ca"];
	Kit.eq(Locale.pick("", "zh-Hant-TW", "en_us", RAV), "zh_tw");
	Kit.eq(Locale.pick("", "es-419", "en_us", RAV), "es");
	Kit.eq(Locale.pick("", "en-US-x-foo", "xx", RAV), "en_us");
	Kit.eq(Locale.pick("", "de-DE-1996", "en_us", RAV), "de_de");
	Kit.eq(Locale.pick("", "fr-Latn-CA;q=0.9", "en_us", RAV), "fr_ca");
	Kit.eq(Locale.pick("", "zh-Hans", "en_us", ["en_us", "zh_cn"]), "zh_cn", "language only, then prefix");
	Kit.eq(Locale.pick("zh-Hant-TW", "", "en_us", RAV), "zh_tw", "user language reduced too");
	Kit.eq(Locale.pick("", "es-419-x", "xx", ["es_ar"]), "es_ar");
	Kit.eq(Locale.pick("", "-Hant-TW, fr", "en_us", AV), "fr_fr");

	// the sysop default is matched against what is available
	Kit.eq(Locale.pick("", "", "es_mx", ["en_us", "es_ar"]), "es_ar");
	Kit.eq(Locale.pick("", "", "es_ar", ["en_us", "es_ar"]), "es_ar");
	Kit.eq(Locale.pick("", "", "de", ["en_us"]), "en_us");
	Kit.eq(Locale.pick("", "", "fr_fr", []), "en_us");
	Kit.eq(Locale.pick("", "", "fr_fr", undefined), "en_us");
	Kit.eq(Locale.pick("", "", "de", ["fr_fr"]), "en_us", "en_us even when not listed");
	["", "zz", "../x", "FR-ca", "es", "es_mx"].forEach(function (d) {
		var r = Locale.pick("zz", "yy", d, ["fr_fr", "es_ar"]);
		Kit.assert(r === "en_us" || r === "fr_fr" || r === "es_ar", "only available or en_us: " + r);
	});

	// name validation: anything not ll or ll_cc is en_us
	["../x", "en_us/../x", "EN-us", "", "e", "en_", "_us", "en_us_x", "english", "en us", "en\\us", "en.ini",
	 "12", "en_1", "en%2fus"].forEach(function (n) {
		Kit.eq(Locale.valid(n), "en_us", n);
	});
	Kit.eq(Locale.valid("fr-CA"), "fr_ca");
	Kit.eq(Locale.valid("FR_ca"), "fr_ca");
	Kit.eq(Locale.valid("fr"), "fr");
	Kit.eq(Locale.valid("fil_ph"), "fil_ph");
	Kit.eq(Locale.valid(undefined), "en_us");
	Kit.eq(Locale.valid("../x", null), null, "no fallback given");

	// scratch directories
	var dir = scr.dir + "wm/";
	var mods = scr.dir + "mods/";
	mkpath(dir + "locale");
	mkpath(mods + "locale");
	function put(path, text) {
		var f = new File(path);
		Kit.assert(f.open("wb"), "open " + path);
		f.write(text);
		f.close();
	}
	put(dir + "locale/en_us.ini", "[main]\nname = Webmail\ngreet = Hello\n\n[errors]\nnotfound = Not found\n");

	// available
	put(dir + "locale/es_ar.ini", "[main]\ngreet = Hola\n");
	put(mods + "locale/fr_fr.ini", "[main]\ngreet = Bonjour\n");
	put(mods + "locale/es_ar.ini", "[main]\nname = Correo\n");
	put(dir + "locale/Bad-Name.ini", "[main]\ngreet = x\n");
	put(dir + "locale/notes.txt", "x");
	put(dir + "locale/../x.ini", "x");
	Kit.eq(Locale.available(dir, mods), ["en_us", "es_ar", "fr_fr"], "es_ar is in both and listed once");
	Kit.eq(Locale.available(dir, scr.dir + "nonesuch/"), ["en_us", "es_ar"]);
	Kit.eq(Locale.available(scr.dir + "nonesuch/", mods), ["es_ar", "fr_fr"]);
	Kit.eq(Locale.available(dir), ["en_us", "es_ar"], "no mods dir");

	// load: en_us, then the language, then the overlay
	var s = Locale.load(dir, mods, "en_us");
	Kit.eq(s, { "main.name": "Webmail", "main.greet": "Hello", "errors.notfound": "Not found" });
	s = Locale.load(dir, mods, "es_ar");
	Kit.eq(s["main.greet"], "Hola", "language overrides");
	Kit.eq(s["main.name"], "Correo", "overlay overrides");
	Kit.eq(s["errors.notfound"], "Not found", "falls back to en_us");
	// a locale with only an overlay
	s = Locale.load(dir, mods, "fr_fr");
	Kit.eq(s["main.greet"], "Bonjour");
	Kit.eq(s["main.name"], "Webmail");
	// a locale with no file at all: English
	Kit.eq(Locale.load(dir, mods, "de_de"), Locale.load(dir, mods, "en_us"));
	// a bad name is en_us and never reaches a path
	put(scr.dir + "x.ini", "[main]\ngreet = LEAKED\n");
	Kit.eq(Locale.load(dir, mods, "../x")["main.greet"], "Hello");
	Kit.eq(Locale.load(dir, mods, "en_us/../x")["main.greet"], "Hello");
	Kit.eq(Locale.load(dir, mods, "EN-us")["main.greet"], "Hello");
	Kit.eq(Locale.load(dir, mods, undefined)["main.greet"], "Hello");
	// no mods dir
	Kit.eq(Locale.load(dir, undefined, "es_ar")["main.greet"], "Hola");
	Kit.eq(Locale.load(dir, "", "es_ar")["main.name"], "Webmail");

	// an overlay that changes one key changes only that key, adds none
	put(mods + "locale/en_us.ini", "[main]\nname = Our Mail\n");
	var a = Locale.load(dir, mods, "en_us");
	Kit.eq(a, { "main.name": "Our Mail", "main.greet": "Hello", "errors.notfound": "Not found" });
	Kit.eq(Object.keys(a).length, 3);

	// values are text exactly as written
	put(dir + "locale/xx_yy.ini",
		"; a comment\n# another\n\n[Main]\n"
		+ "one = 1\ntrue_val=true\nsemi = a ; b # c\neq = k=v=w\n"
		+ "quoted = \"hi\"\nback = a\\nb\\\\c\nspaced =   padded text   \n"
		+ "tab =\tx\t\nempty =\nbraces = {n} of {total}\n"
		+ "utf = caf\xc3\xa9 \xe2\x82\xac\nneg = -5\nflt = 1.50\n"
		+ "BadKey = skipped\nbad-key = skipped\nbad key = skipped\n = skipped\nnoequals\n"
		+ "  ; indented comment\n  [ other ]  \nk = v\n[bad section]\nz = 1\n[UPPER]\nq = 2\n"
		+ "crlf = line\r\n[main]\nlater = ok\n");
	s = Locale.load(dir, mods, "xx_yy");
	Kit.assert(s["main.one"] === "1", "1 stays text");
	Kit.assert(s["main.true_val"] === "true", "true stays text");
	Kit.assert(s["main.semi"] === "a ; b # c", "comment characters survive");
	Kit.assert(s["main.eq"] === "k=v=w", "equals survive");
	Kit.assert(s["main.quoted"] === "\"hi\"", "quotes are literal");
	Kit.assert(s["main.back"] === "a\\nb\\\\c", "no escapes");
	Kit.assert(s["main.spaced"] === "padded text", "outer spaces trimmed");
	Kit.assert(s["main.tab"] === "x", "tabs trimmed");
	Kit.assert(s["main.empty"] === "", "empty value");
	Kit.assert(s["main.braces"] === "{n} of {total}");
	Kit.assert(s["main.utf"] === "caf\xc3\xa9 \xe2\x82\xac", "UTF-8 bytes unchanged");
	Kit.assert(s["main.neg"] === "-5" && s["main.flt"] === "1.50", "numbers stay text");
	Kit.assert(s["upper.crlf"] === "line", "CRLF");
	Kit.assert(s["main.later"] === "ok", "section may reappear");
	Kit.assert(s["other.k"] === "v", "section name trimmed and lowercased");
	Kit.assert(s["upper.q"] === "2", "uppercase section lowercased");
	Kit.assert(s["main.badkey"] === "skipped", "key lowercased");
	var bad = ["main.BadKey", "main.bad-key", "main.bad key", "main.", "main.noequals", "bad section.z"];
	bad.forEach(function (k) {
		Kit.assert(s[k] === undefined, "skipped " + k);
	});
	for (var k in s)
		Kit.assert(typeof s[k] === "string", k + " is a string");
	// a UTF-8 byte order mark on the first line is ignored
	put(dir + "locale/bm_bm.ini", "\xef\xbb\xbf[main]\ngreet = Bom\n");
	Kit.eq(Locale.load(dir, mods, "bm_bm")["main.greet"], "Bom");

	// an unreadable or odd overlay is skipped without a throw
	mkpath(mods + "locale/dd_dd.ini");	// a directory
	Kit.eq(Locale.load(dir, mods, "dd_dd")["main.greet"], "Hello");

	// a missing en_us.ini throws a generic Error and logs the path
	logged.length = 0;
	var empty = scr.dir + "empty/";
	mkpath(empty + "locale");
	var thrown = null;
	try {
		Locale.load(empty, mods, "es_ar");
	} catch (e) {
		thrown = e;
	}
	Kit.assert(thrown instanceof Error, "throws an Error");
	Kit.assert(thrown.message.indexOf("/") < 0 && thrown.message.indexOf("\\") < 0, "no path in the message");
	Kit.eq(logged.length, 1);
	Kit.assert(logged[0].indexOf(empty + "locale/en_us.ini") >= 0, "path logged");

	// the real en_us.ini
	var real = Locale.load(Kit.exec_root() + "../webmail/lib/", Kit.exec_root() + "../webmail/mods/", "en_us");
	var codes = ["notfound", "forbidden", "invalid", "quota", "toolarge", "conflict", "failed", "expired",
		"auth", "https", "internal"];
	codes.forEach(function (c) {
		Kit.assert(typeof real["errors." + c] === "string" && real["errors." + c] !== "", "errors." + c);
	});
	["main", "folders", "list", "reader", "compose", "contacts", "settings", "errors"].forEach(function (sec) {
		var n = 0;
		for (var key in real)
			if (key.indexOf(sec + ".") === 0)
				n++;
		Kit.assert(n > 0, "section " + sec + " has keys");
	});
	for (var key in real) {
		Kit.assert(/^[a-z0-9_]+\.[a-z0-9_]+$/.test(key), "key name " + key);
		var v = real[key];
		Kit.assert(typeof v === "string" && v !== "", "value of " + key);
		Kit.assert(/^[\x20-\x7e]*$/.test(v), "ASCII only in " + key);
		Kit.assert(!/\s$|^\s/.test(v), "outer space in " + key);
		// placeholders are {name} with a plain name
		var ph = v.match(/[{}]/g) || [];
		var good = v.match(/\{[a-z_]+\}/g) || [];
		Kit.eq(ph.length, good.length * 2, "braces in " + key);
	}
	Kit.assert(real["errors.quota"].length > 5);

	// every string that was asked for is present
	var needed = ["list.label_unread_count", "list.label_move_to", "list.placeholder_move_filter",
		"list.button_delete_forever", "list.label_delete_forever_confirm", "list.label_marked_spam",
		"list.label_marked_not_spam", "list.label_starred", "list.label_unstarred", "list.label_archived",
		"list.label_restored", "list.label_restored_to_archive", "list.label_deleted_forever",
		"list.label_select_message", "list.label_select_all", "list.label_today", "list.label_yesterday",
		"list.placeholder_search", "list.button_clear_search", "reader.button_show_quoted",
		"reader.button_hide_quoted", "compose.label_send_failed", "compose.label_draft_failed",
		"folders.label_new_folder", "folders.label_rename_folder", "folders.label_delete_folder",
		"folders.label_delete_folder_confirm", "main.label_signed_in_as", "main.button_cancel",
		"main.label_offline", "main.label_retrying", "main.button_undo", "folders.label_trash_notice",
		"main.label_storage", "list.label_no_results", "list.label_selected", "list.label_moved_to_trash",
		"list.label_message_sent"];
	needed.forEach(function (k) {
		Kit.assert(Object.prototype.hasOwnProperty.call(real, k), "missing " + k);
	});
	Kit.eq(real["list.label_deleted_forever"], "Deleted permanently");
	Kit.assert(real["list.label_deleted"] === undefined, "old label_deleted gone");
	Kit.assert(real["main.error_signin_mismatch"] === undefined, "no lone error_ key");
	// the sign-in mismatch text lives once, under [errors]
	for (var rk in real)
		Kit.assert(!/^[a-z0-9_]+\.error_/.test(rk), "error_ prefix: " + rk);
	// offline / retrying / undo live once, in [main]
	["list.label_offline", "list.label_retrying", "list.button_undo"].forEach(function (k) {
		Kit.assert(real[k] === undefined, "duplicate " + k);
	});
	// every _one key has its base key, and uses the same placeholders
	var ones = 0;
	for (var ok in real) {
		if (!/_one$/.test(ok))
			continue;
		ones++;
		var base = ok.replace(/_one$/, "");
		Kit.assert(Object.prototype.hasOwnProperty.call(real, base), "base for " + ok);
	}
	Kit.assert(ones >= 2, "plural forms present");
	Kit.assert(real["folders.label_trash_notice_one"].indexOf("1 day") >= 0, "singular day");
	Kit.assert(real["folders.label_trash_notice"].indexOf("{days}") >= 0);
	// no two keys in different sections hold the same text under the same name
	var seen = {};
	for (var dk in real) {
		var nm = dk.split(".")[1];
		var sec = dk.split(".")[0];
		if (sec === "errors" || sec === "folders")
			continue;
		if (seen[nm] !== undefined && seen[nm].v === real[dk])
			Kit.assert(false, "duplicate " + nm + " in " + seen[nm].s + " and " + sec);
		seen[nm] = { s: sec, v: real[dk] };
	}
} finally {
	scr.cleanup();
}
