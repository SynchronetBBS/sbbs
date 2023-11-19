// Localized/customized support for JavaScript user-visible text (i.e. strings not contained in text.dat)
// Customized text strings go in the [JS] section of ctrl/text.ini
// Localized (translated to non-default locale) text strings go in the [JS] section of ctrl/text.<lang>.ini

"use strict";

var gettext_cache = {};

function gettext(orig) {
	function get_text_from_ini(ini_fname, orig)	{
		var f = new File(ini_fname);
		if(!f.open("r"))
			return undefined;
		var text = f.iniGetValue("js", orig);
		f.close();
		return text;
	}
	if (gettext_cache[orig] !== undefined)
		return gettext_cache[orig];
	var text;
	if (user.lang)
		text = get_text_from_ini("text." + user.lang + ".ini", orig);
	if (text === undefined)
		text = get_text_from_ini("text.ini", orig);
	if (text === undefined)
		text = orig;
	return gettext_cache[orig] = text;
}

this;
