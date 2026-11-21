// BlockTags: a user's blocked address tags.
//
// The mail server delivers mail for <name>#<tag>@<host> to the user of
// <name> unless the tag matches a line of data/user/<####>.smtpblock, a
// filter file like the .can files: one entry per line, compared without
// regard to case; ';' starts a comment line, text after a tab is a note,
// and an entry may be a pattern (a '*' wildcard, a trailing '~' or '^', a
// leading '!').  A sysop or user may have written the file by hand, so
// nothing here rewrites a line it was not asked to remove.
//
// var BlockTags = load({}, ".../blocktags.js");
// BlockTags.path(data_dir, usernum)    the user's file
// BlockTags.valid(tag)                 whether tag can be blocked here: 1 to
//                                      MAX_TAG of A-Z a-z 0-9 . _ + = -
// BlockTags.list(path)                 [{ text, pattern }], in file order:
//                                      each entry (comments and blank lines
//                                      left out), pattern true when it is
//                                      not a plain tag; [] without a file
// BlockTags.block(path, tag)           adds the tag (lowercased) unless a
//                                      plain entry already names it; throws
//                                      { code: "invalid", reason: "tag" } or
//                                      { code: "toolarge", reason: "tags" }
//                                      (MAX_ENTRIES entries)
// BlockTags.unblock(path, text)        removes every entry that reads text
//                                      (case ignored), a pattern too
// block() and unblock() return true when the file changed.  The file is
// replaced by rename, so the mail server never reads half of it; the
// caller holds a lock around the call (two requests of one user).
// Entries are byte strings as stored; the caller converts for display.

"use strict";

var BlockTags = (function () {
	var B = {};

	B.MAX_TAG = 64;
	B.MAX_ENTRIES = 500;

	B.path = function (data_dir, usernum) {
		return data_dir + "user/" + format("%04d", usernum) + ".smtpblock";
	};

	B.valid = function (tag) {
		return typeof tag == "string" && tag.length >= 1 && tag.length <= B.MAX_TAG
			&& /^[A-Za-z0-9._+=\-]+$/.test(tag);
	};

	function lower(s)
	{
		return s.replace(/[A-Z]+/g, function (m) {
			return m.toLowerCase();
		});
	}

	// The entry of a line as the matcher reads it ("" for a blank line or
	// a comment): up to the first tab, trailing white space removed
	function entry(line)
	{
		var s = line.replace(/[\r\n]+$/, "");
		var tab = s.indexOf("\t");
		if (tab >= 0)
			s = s.substr(0, tab);
		s = s.replace(/[ \t\r\n\f\v\xFF]+$/, "");
		return s.charAt(0) == ";" ? "" : s;
	}

	function is_pattern(e)
	{
		return e.charAt(0) == "!" || e.indexOf("*") >= 0 || /[~^]$/.test(e);
	}

	// The file as its lines, each with its line ending
	function lines(path)
	{
		if (!file_exists(path))
			return [];
		var f = new File(path);
		if (!f.open("rb"))
			throw new Error("BlockTags: error " + f.error + " opening " + path);
		var text = f.read();
		f.close();
		return text.match(/[^\n]*\n|[^\n]+$/g) || [];
	}

	function save(path, list)
	{
		var tmp = path + ".tmp";
		var f = new File(tmp);
		if (!f.open("wb"))
			throw new Error("BlockTags: error " + f.error + " creating " + tmp);
		var ok = f.write(list.join(""));
		f.close();
		if (!ok || !file_rename(tmp, path)) {
			file_remove(tmp);
			throw new Error("BlockTags: cannot write " + path);
		}
	}

	B.list = function (path) {
		var out = [];
		lines(path).forEach(function (l) {
			var e = entry(l);
			if (e !== "")
				out.push({ text: e, pattern: is_pattern(e) });
		});
		return out;
	};

	B.block = function (path, tag) {
		if (!B.valid(tag))
			throw { code: "invalid", message: "invalid tag", reason: "tag" };
		tag = lower(tag);
		var list = lines(path);
		var count = 0;
		for (var i = 0; i < list.length; i++) {
			var e = entry(list[i]);
			if (e === "")
				continue;
			count++;
			if (!is_pattern(e) && lower(e) === tag)
				return false;
		}
		if (count >= B.MAX_ENTRIES)
			throw { code: "toolarge", message: "too many blocked tags", reason: "tags" };
		var eol = list.length > 0 && /\r\n$/.test(list[0]) ? "\r\n" : "\n";
		if (list.length > 0 && !/\n$/.test(list[list.length - 1]))
			list[list.length - 1] += eol;
		list.push(tag + eol);
		save(path, list);
		return true;
	};

	B.unblock = function (path, text) {
		if (typeof text != "string" || text === "")
			return false;
		var want = lower(text);
		var list = lines(path);
		var kept = list.filter(function (l) {
			var e = entry(l);
			return e === "" || lower(e) !== want;
		});
		if (kept.length === list.length)
			return false;
		save(path, kept);
		return true;
	};

	return B;
})();

BlockTags;
