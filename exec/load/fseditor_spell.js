/*
 * fseditor_spell.js - Spell checking engine for FSEditor
 *
 * Usage:  var spell = load({}, "fseditor_spell.js");
 *
 * Uses the same word lists as SlyEdit (dictionary_*.txt in sbbs/mods or
 * sbbs/ctrl): a base list for the language ("dictionary_en.txt") plus an
 * optional regional supplement ("dictionary_en-US-supplemental.txt").  The
 * language code "en-US" therefore loads both of those files.  Dropping a new
 * pair of files into ctrl/ (or mods/) makes a new language available.
 *
 * Words the user adds are kept in a per-user personal word list.
 */

"use strict";

var LANG_NAMES = {
	"en":    "English",
	"en-US": "English (American)",
	"en-GB": "English (British)",
	"en-CA": "English (Canadian)",
	"en-AU": "English (Australian)"
};

var words = null;			/* Loaded dictionary: "$word" -> true */
var extra = {};				/* Personal dictionary, same format */
var extra_count = 0;
var loaded_lang = null;		/* Language code currently loaded */
var personal_filename = null;
var load_error = "";

/* Two-letter words acceptable as half of a split ("inthe" -> "in the") */
var SHORT_WORDS = " am an as at be by do go he if in is it me my no of oh on or so to up us we ";

var LETTERS = "abcdefghijklmnopqrstuvwxyz'";

function lang_name(code)
{
	return LANG_NAMES[code] || code;
}

function search_dirs()
{
	var dirs = [];
	if(system.mods_dir)
		dirs.push(system.mods_dir);
	if(system.ctrl_dir)
		dirs.push(system.ctrl_dir);
	return dirs;
}

/* Find a dictionary file, mods dir first (so the sysop can override) */
function locate(name)
{
	var dirs = search_dirs();
	for(var i = 0; i < dirs.length; i++) {
		if(file_exists(dirs[i] + name))
			return dirs[i] + name;
	}
	return null;
}

function dict_files(code)
{
	var primary = code.split("-")[0];
	var files = [];
	var base = locate("dictionary_" + code + ".txt");
	if(!base)
		base = locate("dictionary_" + primary + ".txt");
	if(base)
		files.push(base);
	var supp = locate("dictionary_" + code + "-supplemental.txt");
	if(supp)
		files.push(supp);
	return files;
}

/* Returns an array of { code: "en-US", name: "English (American)" } */
function languages()
{
	var seen = {};
	var codes = [];
	var dirs = search_dirs();
	for(var d = 0; d < dirs.length; d++) {
		var list = directory(dirs[d] + "dictionary_*.txt");
		for(var i = 0; i < list.length; i++) {
			var m = file_getname(list[i]).match(/^dictionary_(.+?)(-supplemental)?\.txt$/i);
			if(m === null)
				continue;
			var code = m[1];
			/* A bare base list ("en") is only a language if nothing regional exists for it */
			if(!m[2])
				continue;
			if(!seen[code] && dict_files(code).length > 0) {
				seen[code] = true;
				codes.push(code);
			}
		}
	}
	if(codes.length == 0) {
		for(var d = 0; d < dirs.length; d++) {
			var list = directory(dirs[d] + "dictionary_*.txt");
			for(var i = 0; i < list.length; i++) {
				var m = file_getname(list[i]).match(/^dictionary_([^-]+)\.txt$/i);
				if(m !== null && !seen[m[1]]) {
					seen[m[1]] = true;
					codes.push(m[1]);
				}
			}
		}
	}
	codes.sort();
	var result = [];
	for(var i = 0; i < codes.length; i++)
		result.push({ code: codes[i], name: lang_name(codes[i]) });
	return result;
}

/* Reads one word list file into the "into" object.  Returns the word count. */
function read_list(path, into)
{
	var f = new File(path);
	if(!f.open("r"))
		return 0;
	var s = f.read();
	f.close();
	if(!s)
		return 0;
	/* Skip the Aspell/SCOWL copyright header, which ends with a line of "---" */
	if(s.indexOf("Custom wordlist generated") === 0) {
		var h = s.indexOf("\n---");
		if(h >= 0) {
			h = s.indexOf("\n", h + 1);
			s = (h >= 0) ? s.substr(h + 1) : "";
		}
	}
	var list = s.toLowerCase().replace(/\r/g, "").split("\n");
	var count = 0;
	for(var i = 0; i < list.length; i++) {
		if(list[i].length) {
			into["$" + list[i]] = true;
			count++;
		}
	}
	return count;
}

function load_personal(filename)
{
	extra = {};
	extra_count = 0;
	personal_filename = filename || null;
	if(!personal_filename || !file_exists(personal_filename))
		return;
	var f = new File(personal_filename);
	if(!f.open("r"))
		return;
	var list = f.readAll();
	f.close();
	for(var i = 0; i < list.length; i++) {
		var w = list[i].toLowerCase().replace(/^\s+|\s+$/g, "");
		if(w.length && !extra["$" + w]) {
			extra["$" + w] = true;
			extra_count++;
		}
	}
}

/*
 * Loads the dictionary for a language code (e.g. "en-US").
 * personal_file: path of the user's personal word list (or null).
 * Returns true on success; otherwise see load_error.
 */
function load_language(code, personal_file)
{
	load_error = "";
	var files = dict_files(code);
	if(files.length == 0) {
		load_error = "No dictionary found for " + lang_name(code);
		return false;
	}
	var w = {};
	var n = 0;
	for(var i = 0; i < files.length; i++)
		n += read_list(files[i], w);
	if(n < 1) {
		load_error = "Unable to read dictionary for " + lang_name(code);
		return false;
	}
	words = w;
	loaded_lang = code;
	load_personal(personal_file);
	return true;
}

function is_loaded(code)
{
	return words !== null && loaded_lang === code;
}

function is_known(w)
{
	var k = "$" + w;
	return words[k] === true || extra[k] === true;
}

/* Is this word spelled correctly?  (Any letter case) */
function check_word(w)
{
	var lw = w.toLowerCase();
	if(is_known(lw))
		return true;
	/* Possessives and contractions that aren't listed explicitly */
	var m = lw.match(/^(.+?)'(s|ll|d|re|ve|m)$/);
	if(m !== null && is_known(m[1]))
		return true;
	return false;
}

/*
 * Finds the misspelled words in one line of text.
 * o.skip_caps: ignore ALL-CAPS words (BBS, ASCII...)
 * o.ignore:    object of "$word" -> true of words to ignore
 * Returns an array of { start, end (exclusive), word }
 */
function find_errors(text, o)
{
	var errs = [];
	if(words === null)
		return errs;
	o = o || {};
	/* Curly apostrophes -> straight (same length, so offsets stay valid) */
	var t = text.replace(/[‘’]/g, "'");
	var tre = /\S+/g;
	var m;
	while((m = tre.exec(t)) !== null) {
		var tok = m[0];
		/* URLs, e-mail addresses, numbers, paths, e.g., foreign letters, markup... */
		if(/[0-9@\/\\_:#~<>=|*^{}]|[A-Za-z]\.[A-Za-z]|[À-῿⁰-￿]/.test(tok))
			continue;
		var wre = /[A-Za-z]+(?:'[A-Za-z]+)*/g;
		var wm;
		while((wm = wre.exec(tok)) !== null) {
			var wd = wm[0];
			if(wd.length < 2)
				continue;
			var allcaps = (wd == wd.toUpperCase());
			if(allcaps && o.skip_caps)
				continue;
			/* camelCase / iPhone style words */
			if(!allcaps && /[A-Z]/.test(wd.substr(1)))
				continue;
			if(check_word(wd))
				continue;
			if(o.ignore && o.ignore["$" + wd.toLowerCase()])
				continue;
			errs.push({ start: m.index + wm.index, end: m.index + wm.index + wd.length, word: wd });
		}
	}
	return errs;
}

/* Quoted text (" XX> blah", "> blah") and tear/origin lines aren't checked */
function is_skipped_line(text)
{
	return /^\s{0,2}[A-Za-z]{0,4}>/.test(text) || /^(---|\* Origin:)/.test(text);
}

function apply_case(orig, c)
{
	if(orig.length > 1 && orig == orig.toUpperCase())
		return c.toUpperCase();
	if(orig.charAt(0) != orig.charAt(0).toLowerCase())
		return c.charAt(0).toUpperCase() + c.substr(1);
	return c;
}

/* Calls fn(candidate, score) for every string one edit away from w */
function edits1(w, fn)
{
	var n = w.length;
	var i, j;
	for(i = 0; i < n - 1; i++)
		fn(w.substr(0, i) + w.charAt(i + 1) + w.charAt(i) + w.substr(i + 2), 0);
	for(i = 0; i < n; i++)
		for(j = 0; j < LETTERS.length; j++)
			if(LETTERS.charAt(j) != w.charAt(i))
				fn(w.substr(0, i) + LETTERS.charAt(j) + w.substr(i + 1), 1);
	for(i = 0; i < n; i++)
		fn(w.substr(0, i) + w.substr(i + 1), 2);
	for(i = 0; i <= n; i++)
		for(j = 0; j < LETTERS.length; j++)
			fn(w.substr(0, i) + LETTERS.charAt(j) + w.substr(i), 3);
}

/* Returns up to max suggested corrections for a misspelled word */
function suggest(word, max)
{
	if(words === null)
		return [];
	max = max || 9;
	var w = word.toLowerCase().replace(/[‘’]/g, "'");
	var cands = [];
	var seen = {};
	seen["$" + w] = true;

	function consider(c, score) {
		var k = "$" + c;
		if(seen[k])
			return;
		seen[k] = true;
		if(!check_word(c))
			return;
		if(c.charAt(0) != w.charAt(0))
			score += 10;
		cands.push({ w: c, s: score, i: cands.length });
	}

	edits1(w, consider);

	/* Run-together words: "alot" -> "a lot" */
	function part_ok(p) {
		if(p.length == 1)
			return (p == "a" || p == "i");
		if(p.length == 2)
			return SHORT_WORDS.indexOf(" " + p + " ") >= 0;
		return check_word(p);
	}
	for(var i = 1; i < w.length; i++) {
		var a = w.substr(0, i), b = w.substr(i);
		if(part_ok(a) && part_ok(b)) {
			var c = a + " " + b;
			if(!seen["$" + c]) {
				seen["$" + c] = true;
				cands.push({ w: c, s: 0.5, i: cands.length });
			}
		}
	}

	/* Nothing close?  Try two edits (slower, so only for short words and only when needed) */
	if(cands.length == 0 && w.length >= 3 && w.length <= 12) {
		var first = [];
		edits1(w, function(c) { first.push(c); });
		for(var k = 0; k < first.length; k++)
			edits1(first[k], function(c) { consider(c, 20); });
	}

	cands.sort(function(a, b) { return (a.s - b.s) || (a.i - b.i); });
	var result = [];
	for(var i = 0; i < cands.length && result.length < max; i++)
		result.push(apply_case(word, cands[i].w));
	return result;
}

/* Adds a word to the personal dictionary (and the user's file, if there is one) */
function add_personal(word)
{
	var lw = word.toLowerCase();
	if(extra["$" + lw])
		return;
	extra["$" + lw] = true;
	extra_count++;
	if(personal_filename) {
		var f = new File(personal_filename);
		if(f.open("a")) {
			f.writeln(lw);
			f.close();
		}
	}
}

function clear_personal()
{
	extra = {};
	extra_count = 0;
	if(personal_filename && file_exists(personal_filename))
		file_remove(personal_filename);
}

function personal_count()
{
	return extra_count;
}

this;
