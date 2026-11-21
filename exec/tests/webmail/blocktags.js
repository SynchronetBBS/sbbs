// BlockTags: a user's blocked address tags (webmail/lib/blocktags.js)

load(js.exec_dir + "kit.js");
var BlockTags = Kit.wlib("blocktags");

var S = Kit.scratch();
try {
	var path = BlockTags.path(S.data_dir, 7);
	Kit.eq(path, S.data_dir + "user/0007.smtpblock", "path");
	mkpath(S.data_dir + "user");

	function raw() {
		var f = new File(path);
		if (!f.open("rb"))
			return null;
		var s = f.read();
		f.close();
		return s;
	}
	function put(text) {
		var f = new File(path);
		Kit.assert(f.open("wb"), "create");
		f.write(text);
		f.close();
	}
	function texts() {
		return BlockTags.list(path).map(function (e) { return e.text + (e.pattern ? "*P" : ""); });
	}
	function refused(fn, code, reason, what) {
		var e = null;
		try { fn(); } catch (x) { e = x; }
		Kit.assert(e !== null && e.code === code && e.reason === reason, what + ": " + JSON.stringify(e));
	}

	// --- valid tags
	["shop", "A.b_c+d=e-9", new Array(65).join("x")].forEach(function (t) {
		Kit.assert(BlockTags.valid(t), "valid: " + t);
	});
	["", new Array(66).join("x"), "a b", "a#b", "a@b", "!a", ";a", "a*", "a~", "a^", "a\tb", "a\nb", "caf\xe9", 5, null]
		.forEach(function (t) {
			Kit.assert(!BlockTags.valid(t), "not valid: " + JSON.stringify(t));
		});

	// --- no file
	Kit.eq(BlockTags.list(path), [], "no file: empty list");
	Kit.eq(BlockTags.unblock(path, "x"), false, "no file: nothing to remove");
	Kit.eq(file_exists(path), false, "unblock creates no file");

	// --- block: appended, lowercased; a second time changes nothing
	Kit.eq(BlockTags.block(path, "Shop"), true, "block");
	Kit.eq(raw(), "shop\n", "stored lowercased, one line");
	Kit.eq(BlockTags.block(path, "SHOP"), false, "already blocked");
	Kit.eq(raw(), "shop\n", "unchanged");
	Kit.eq(BlockTags.block(path, "news"), true, "a second tag");
	Kit.eq(texts(), ["shop", "news"], "listed in file order");
	Kit.assert(system.findstr(path, "NEWS") === true && system.findstr(path, "other") === false,
		"the mail server's matcher finds a blocked tag, not another");
	refused(function () { BlockTags.block(path, "a b"); }, "invalid", "tag", "an invalid tag");
	Kit.eq(texts(), ["shop", "news"], "a refusal writes nothing");
	Kit.eq(file_exists(path + ".tmp"), false, "no temporary file left");

	// --- a file written by hand: comments, notes, patterns, CRLF, no
	// final line ending; nothing but the line asked for is touched
	put("; my blocked tags\r\nOld\tspam since 2020\r\n\r\njunk*\r\n!keep\r\nads~\r\nlast");
	Kit.eq(texts(), ["Old", "junk**P", "!keep*P", "ads~*P", "last"], "entries: no comments, blank lines or notes; patterns marked");
	Kit.eq(BlockTags.block(path, "old"), false, "a plain entry in another case is the same tag");
	Kit.eq(BlockTags.block(path, "junk"), true, "a pattern does not count as the plain tag");
	Kit.eq(raw(), "; my blocked tags\r\nOld\tspam since 2020\r\n\r\njunk*\r\n!keep\r\nads~\r\nlast\r\njunk\r\n",
		"appended with the file's line ending, the unfinished last line completed");
	Kit.eq(BlockTags.unblock(path, "OLD"), true, "unblock ignores case");
	Kit.eq(BlockTags.unblock(path, "junk*"), true, "a pattern is removed by its text");
	Kit.eq(BlockTags.unblock(path, "nothere"), false, "not listed: unchanged");
	Kit.eq(BlockTags.unblock(path, ""), false, "empty: unchanged");
	Kit.eq(raw(), "; my blocked tags\r\n\r\n!keep\r\nads~\r\nlast\r\njunk\r\n", "the other lines are as they were");

	// --- the cap counts entries, not comments
	var many = "; note\n";
	for (var i = 0; i < BlockTags.MAX_ENTRIES - 1; i++)
		many += "t" + i + "\n";
	put(many);
	Kit.eq(BlockTags.block(path, "onemore"), true, "the last entry allowed");
	refused(function () { BlockTags.block(path, "toomany"); }, "toolarge", "tags", "the list is full");
	Kit.eq(BlockTags.block(path, "t5"), false, "full, but already blocked: no refusal");
	Kit.eq(BlockTags.unblock(path, "t5"), true, "a full list can be shortened");
	Kit.eq(BlockTags.block(path, "toomany"), true, "then there is room");
} finally {
	S.cleanup();
}
