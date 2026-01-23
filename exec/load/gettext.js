// Localized/customized support for JavaScript user-visible text (i.e. strings not contained in text.dat)
// Customized text strings go in the [scriptfilename.js] or [JS] sections of ctrl/[charset/]text.ini
// Localized (translated to non-default locale) text strings go in the equivalent sections of ctrl/[charset/]text.<lang>.ini

// For strings that contain control characters,
// an optional 'key' is typically used/specified to look-up those text strings.

"use strict";

var gettext_cache = {};

function gettext(orig, key) {
	function get_text_from_ini(ini_fname, orig, key){
		var f = new File(ini_fname);
		if(!f.open("r"))
			return undefined;
		var text = f.iniGetValue(js.exec_file, key || orig);
		if (text === undefined)
			text = f.iniGetValue("js", key || orig);
		f.close();
		return text;
	}
	if (gettext_cache[key || orig] !== undefined)
		return gettext_cache[key || orig];
	var text;
	var charset = console.charset.toLowerCase();
	if (user.lang)
		text = get_text_from_ini(charset + "/text." + user.lang + ".ini", orig, key);
	if (text === undefined)
		text = get_text_from_ini(charset + "/text.ini", orig, key);
	if (text === undefined && user.lang)
		text = get_text_from_ini("text." + user.lang + ".ini", orig, key);
	if (text === undefined)
		text = get_text_from_ini("text.ini", orig, key);
	if (text === undefined)
		text = orig;
	if (key === undefined && orig.length > 0 && orig.indexOf(':') == orig.length - 1)
		log(LOG_WARNING, "gettext() called with a value ending in colon: '" + orig + "'");
	return gettext_cache[key || orig] = text;
}

this;
