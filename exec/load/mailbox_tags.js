// Folder and flag encoding in message header tags, for the mailbox library.
//
// var Tags = load("mailbox_tags.js");
//
// A message's tags are one string (the header's "tags" field), tags
// separated by single spaces. Reserved tags start with '$' and are matched
// case-sensitively: $Sent $Archive $Trash (folders), $Starred, $Was:<tag>
// (the folder a Trash message came from), $Failed and $Unsent (Sent copies).
// A user folder's tag is its name's UTF-8 bytes with every byte except
// A-Z a-z 0-9 . _ - and bytes >= 0x80 written as %XX (uppercase hex), so a
// user tag never contains a space and never begins with '$'.
//
// String contract: folder names are UTF-8 byte strings (each char code
// 0-255), the form callers already hold. encode() takes one and decode()
// returns one. Bytes >= 0x80 are kept as they are. A char code above 0xFF
// throws { code: "invalid" } from encode().
// Nothing here uses \s, trim(), toLowerCase() or toUpperCase(): in this
// engine they treat byte 0xA0 as whitespace and fold bytes 0xC0-0xDE to
// 0xE0-0xFE, which would corrupt UTF-8 bytes. Whitespace is ASCII only.
// validate() counts characters (UTF-8 bytes that are not continuation
// bytes 0x80-0xBF), and compares names case-insensitively for ASCII
// letters only (no Unicode case folding).

"use strict";

var Tags = (function () {
	var T = {
		SENT: "$Sent",
		ARCHIVE: "$Archive",
		TRASH: "$Trash",
		STARRED: "$Starred",
		WAS: "$Was:",
		FAILED: "$Failed",
		UNSENT: "$Unsent",
		MAX_NAME_LENGTH: 64
	};
	var BUILTIN = ["inbox", "starred", "sent", "drafts", "spam", "trash", "archive", "all"];
	var HEX = "0123456789ABCDEF";

	function invalid()
	{
		return { code: "invalid", message: "folder name is not a UTF-8 byte string" };
	}

	function lower(s)	// ASCII-only lower-casing
	{
		return s.replace(/[A-Z]/g, function (c) {
			return String.fromCharCode(c.charCodeAt(0) + 32);
		});
	}

	var WS = "[ \\t\\r\\n\\f\\v]";
	var WS_RUN = new RegExp(WS + "+");
	var WS_ANY = new RegExp(WS);
	var WS_EDGES = new RegExp("^" + WS + "+|" + WS + "+$", "g");

	function trim(s)	// ASCII-whitespace-only trim
	{
		return s.replace(WS_EDGES, "");
	}

	function safe_byte(c)	// bytes encode() writes as they are
	{
		return (c >= 0x30 && c <= 0x39) || (c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A)
			|| c == 0x2E || c == 0x5F || c == 0x2D || c >= 0x80;
	}

	function has_wide(s)
	{
		for (var i = 0; i < s.length; i++)
			if (s.charCodeAt(i) > 0xFF)
				return true;
		return false;
	}

	T.encode = function (name)
	{
		name = String(name);
		var out = "";
		for (var i = 0; i < name.length; i++) {
			var c = name.charCodeAt(i);
			if (c > 0xFF)
				throw invalid();
			if (safe_byte(c))
				out += name.charAt(i);
			else
				out += '%' + HEX.charAt(c >> 4) + HEX.charAt(c & 15);
		}
		return out;
	};

	// Reverses only what encode() produces: %XX with uppercase hex for a
	// byte below 0x80 that encode() escapes. Any other '%' stays literal.
	T.decode = function (tag)
	{
		return String(tag).replace(/%([0-9A-F]{2})/g, function (m, h) {
			var c = parseInt(h, 16);
			if (c >= 0x80 || safe_byte(c))
				return m;
			return String.fromCharCode(c);
		});
	};

	// True if tag is exactly what encode() would write for its decoded name
	T.canonical = function (tag)
	{
		tag = String(tag);
		if (has_wide(tag))
			return false;
		return T.encode(T.decode(tag)) === tag;
	};

	// Returns null or "empty", "invalid" (a char code above 0xFF, or a
	// control character: below 0x20 or 0x7F, after the ASCII whitespace at
	// the edges is trimmed), "toolong", "reserved", "duplicate"
	T.validate = function (name, existing_names)
	{
		name = trim(String(name));
		if (name.length < 1)
			return "empty";
		if (has_wide(name))
			return "invalid";
		var chars = 0;
		for (var i = 0; i < name.length; i++) {
			var c = name.charCodeAt(i);
			if (c < 0x20 || c == 0x7F)
				return "invalid";	// a control character
			if (c < 0x80 || c > 0xBF)
				chars++;
		}
		if (chars > T.MAX_NAME_LENGTH)
			return "toolong";
		var key = lower(name);
		if (BUILTIN.indexOf(key) >= 0)
			return "reserved";
		if (existing_names) {
			for (i = 0; i < existing_names.length; i++)
				if (lower(trim(String(existing_names[i]))) == key)
					return "duplicate";
		}
		return null;
	};

	function is_folder_tag(t)
	{
		return t == T.SENT || t == T.ARCHIVE || t == T.TRASH || t.charAt(0) != '$';
	}

	// opts.shared: the tags of a shared (Inbox) message, which has no
	// folder: every tag that is not a reserved flag goes to "other"
	T.parse = function (tags_string, opts)
	{
		var shared = !!(opts && opts.shared);
		var r = { folder: "", starred: false, was: "", failed: false, unsent: false, other: [] };
		if (tags_string === undefined || tags_string === null)
			return r;
		var list = String(tags_string).split(WS_RUN);
		for (var i = 0; i < list.length; i++) {
			var t = list[i];
			if (t === "")
				continue;
			if (t == T.STARRED && !r.starred)
				r.starred = true;
			else if (t == T.FAILED && !r.failed)
				r.failed = true;
			else if (t == T.UNSENT && !r.unsent)
				r.unsent = true;
			else if (t.substr(0, T.WAS.length) == T.WAS && t.length > T.WAS.length && r.was === "")
				r.was = t.substr(T.WAS.length);
			else if (!shared && r.folder === "" && is_folder_tag(t))
				r.folder = t;
			else
				r.other.push(t);
		}
		return r;
	};

	T.format = function (obj)
	{
		if ((obj.folder && WS_ANY.test(obj.folder)) || (obj.was && WS_ANY.test(obj.was)))
			throw { code: "invalid", message: "folder tag is not a single tag" };
		var list = [];
		if (obj.folder)
			list.push(obj.folder);
		if (obj.starred)
			list.push(T.STARRED);
		if (obj.was)
			list.push(T.WAS + obj.was);
		if (obj.failed)
			list.push(T.FAILED);
		if (obj.unsent)
			list.push(T.UNSENT);
		if (obj.other)
			for (var i = 0; i < obj.other.length; i++)
				list.push(obj.other[i]);
		return list.join(' ');
	};

	return T;
})();

Tags;
