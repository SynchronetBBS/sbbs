// Reading a message's parts, for the mailbox library.
//
// var MimeRead = load("mailbox_mime.js");
// MimeRead.model(parts);	// {text_id, html_id, attachments: [{pid, name, type, size, cid, inline}]}
// MimeRead.file_parts(header, data_dir);	// [{pid, name, type, size, path}]
// MimeRead.part_name(part);	// a part's download file name
// MimeRead.safe_name(name, type);	// sanitize a UTF-8 file name
// MimeRead.safe_type(type);	// a usable media type, else application/octet-stream
// MimeRead.decode_words(text, raw);	// RFC 2047 encoded-words to UTF-8
//
// model() takes the array MsgBase.get_msg_parts() returns. The bodies are
// the first text/plain and the first text/html part whose disposition is
// not "attachment"; every other part is an attachment, inline when it has
// a content id. Anything that is not an array gives an empty model.
//
// Part names: get_msg_parts() reports them as found in the message (only
// the name= and filename= parameters; an RFC 2231 filename*= parameter is
// not extracted, so such a part has no name here). They are decoded and
// then sanitized for use as a download file name:
// - RFC 2047 encoded-words (B and Q) in utf-8, us-ascii, iso-8859-1 and
//   windows-1252 are decoded; whitespace between two encoded-words is
//   dropped; a word in another charset, or one that does not decode to
//   valid text in its charset, is kept as the encoded text. Text outside
//   encoded-words is kept if it is valid UTF-8, else read as windows-1252.
// - Then: base name only (after the last / or \), control characters
//   (C0, DEL and C1), the Unicode format characters U+200B-U+200F,
//   U+202A-U+202E, U+2066-U+2069 and U+FEFF, and \ / : * ? " < > |
//   removed; leading whitespace and dots and trailing whitespace and dots
//   removed; at most 200 bytes (cut on a UTF-8 character boundary);
//   "attachment" plus an extension guessed from the type when nothing is
//   left; "_" prefixed when the part before the first dot is a Windows
//   device name (CON, PRN, AUX, NUL, COM1-COM9, LPT1-LPT9, any case).
// The name is for display and Content-Disposition only: no path on the
// server is ever built from it.
//
// File attachments (auxattr MSG_FILEATTACH): the subject holds the file
// names (Summary.attach_names: space separated, double quotes allowed,
// base names only, "", "." and ".." skipped), pid "f1", "f2", ... in that
// order. The files are in data_dir/file/<to_ext>.in/, or
// data_dir/file/<from_ext>.out/ for a message with no recipient user
// number. path is null for a name that cannot be a file there (it holds
// ':' or a control character, or the header names no user); size is -1
// when the file is missing (or path is null). name is the base name as
// UTF-8 (converted from CP437 unless auxattr has MSG_HFIELDS_UTF8),
// sanitized as above; path uses the name as stored. type is guessed from
// the extension.

"use strict";

var MimeRead = (function () {
	var HERE = (function () {
		var f = String(new Error().fileName || "");
		var i = Math.max(f.lastIndexOf('/'), f.lastIndexOf('\\'));
		return i >= 0 ? f.substr(0, i + 1) : "";
	})();

	function sibling(name)
	{
		var path = HERE + name;
		if (HERE === "" || !file_exists(path))
			path = name;
		return load({}, path);
	}

	var Summary = sibling("mailbox_summary.js");

	var MSG_FILEATTACH = (1 << 1);	// auxattr (smbdefs.js)
	var MSG_HFIELDS_UTF8 = (1 << 13);	// auxattr (smbdefs.js)
	var MAX_NAME = 200;
	// UTF-8 of the format characters U+200B-U+200F, U+202A-U+202E,
	// U+2066-U+2069 (zero-width, direction marks, embeddings, overrides and
	// isolates: they can disguise an extension) and U+FEFF
	var FORMAT_RE = /\xE2\x80[\x8B-\x8F\xAA-\xAE]|\xE2\x81[\xA6-\xA9]|\xEF\xBB\xBF/g;
	// Windows device names (the name before its first dot, any case)
	var DEVICE_RE = /^(con|prn|aux|nul|com[1-9]|lpt[1-9])$/i;
	var DEFAULT_TYPE = "application/octet-stream";

	// Extension (lower case) to media type, and back. Types that can run
	// as a page when opened (HTML, SVG, XML) are not guessed.
	var TYPES = {
		txt: "text/plain", log: "text/plain", asc: "text/plain", diz: "text/plain",
		csv: "text/csv",
		jpg: "image/jpeg", jpeg: "image/jpeg", png: "image/png", gif: "image/gif",
		webp: "image/webp", bmp: "image/bmp",
		pdf: "application/pdf", zip: "application/zip", gz: "application/gzip",
		"7z": "application/x-7z-compressed", rar: "application/vnd.rar",
		mp3: "audio/mpeg", ogg: "audio/ogg", wav: "audio/wav", mp4: "video/mp4",
		eml: "message/rfc822"
	};
	var EXTS = {
		"text/plain": ".txt", "text/html": ".html", "text/csv": ".csv",
		"image/jpeg": ".jpg", "image/png": ".png", "image/gif": ".gif", "image/webp": ".webp",
		"image/bmp": ".bmp", "application/pdf": ".pdf", "application/zip": ".zip",
		"application/gzip": ".gz", "audio/mpeg": ".mp3", "audio/ogg": ".ogg", "audio/wav": ".wav",
		"video/mp4": ".mp4", "message/rfc822": ".eml"
	};

	// windows-1252 0x80-0x9F as code points (0 = undefined)
	var CP1252 = [0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
		0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
		0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
		0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178];

	// Single-byte text to UTF-8; cp1252: map 0x80-0x9F as windows-1252
	function bytes_to_utf8(s, cp1252)
	{
		var out = "";
		for (var i = 0; i < s.length; i++) {
			var c = s.charCodeAt(i) & 0xFF;
			if (c < 0x80)
				out += String.fromCharCode(c);
			else {
				if (cp1252 && c < 0xA0)
					c = CP1252[c - 0x80] || 0xFFFD;
				out += utf8_encode(c);
			}
		}
		return out;
	}

	// Decoded bytes of one encoded-word in a charset to UTF-8, or null
	function charset_to_utf8(charset, bytes)
	{
		switch (charset) {
			case "utf-8":
			case "utf8":
				return str_is_utf8(bytes) ? bytes : null;
			case "us-ascii":
			case "ascii":
				return str_is_ascii(bytes) ? bytes : null;
			case "iso-8859-1":
			case "latin1":
			case "latin-1":
				return bytes_to_utf8(bytes, false);
			case "windows-1252":
			case "cp1252":
				return bytes_to_utf8(bytes, true);
		}
		return null;
	}

	// One encoded-word's charset (lower case, RFC 2231 language suffix
	// dropped) and decoded bytes; null if its encoded text is malformed
	function word_bytes(charset, enc, data)
	{
		var bytes;
		charset = charset.toLowerCase();
		var star = charset.indexOf('*');
		if (star >= 0)
			charset = charset.substr(0, star);
		if (enc == 'b' || enc == 'B') {
			if (!/^[A-Za-z0-9+\/]*=*$/.test(data))
				return null;
			bytes = base64_decode(data);
			if (typeof bytes != "string")
				return null;
		} else {
			bytes = data.replace(/_/g, " ").replace(/=([0-9A-Fa-f]{2})/g, function (all, hex) {
				return String.fromCharCode(parseInt(hex, 16));
			});
		}
		return { charset: charset, bytes: bytes };
	}

	function raw_to_utf8(s)
	{
		return str_is_utf8(s) ? s : bytes_to_utf8(s, true);
	}

	// RFC 2047 encoded-words in 'text' decoded to UTF-8; raw (optional)
	// converts the text between them (default: kept if valid UTF-8, else
	// read as windows-1252). Adjacent words (only whitespace between them,
	// which is dropped) in the same charset are joined before their bytes
	// are converted, so a character split across words decodes; a run
	// that does not convert is kept as its encoded text.
	function decode_words(text, raw)
	{
		if (typeof text != "string")
			return "";
		raw = raw || raw_to_utf8;
		var re = /=\?([^?\s]+)\?([BbQq])\?([^?\s]*)\?=/g;
		var out = "";
		var last = 0;
		var run = null;	// {charset, bytes, src}: the pending adjacent words
		var m;
		function flush() {	// true if a run was pending and decoded
			if (run === null)
				return false;
			var dec = charset_to_utf8(run.charset, run.bytes);
			out += dec !== null ? dec : raw(run.src);
			run = null;
			return dec !== null;
		}
		while ((m = re.exec(text)) !== null) {
			var gap = text.substring(last, m.index);
			var w = word_bytes(m[1], m[2], m[3]);
			var adjacent = run !== null && /^[ \t\r\n]*$/.test(gap);
			if (adjacent && w !== null && w.charset === run.charset) {
				run.bytes += w.bytes;
				run.src += gap + m[0];
			} else {
				// whitespace between two decoded words is dropped
				if (!flush() || !adjacent || w === null)
					out += raw(gap);
				if (w === null)
					out += raw(m[0]);
				else
					run = { charset: w.charset, bytes: w.bytes, src: m[0] };
			}
			last = re.lastIndex;
		}
		flush();
		return out + raw(text.substr(last));
	}

	function safe_type(type)
	{
		if (typeof type == "string" && /^[a-z0-9][a-z0-9!#$&^_.+-]*\/[a-z0-9][a-z0-9!#$&^_.+-]*$/.test(type)
			&& type.length <= 127)
			return type;
		return DEFAULT_TYPE;
	}

	function type_of_name(name)
	{
		var dot = name.lastIndexOf('.');
		if (dot < 0)
			return DEFAULT_TYPE;
		var ext = name.substr(dot + 1).toLowerCase();
		return TYPES.hasOwnProperty(ext) ? TYPES[ext] : DEFAULT_TYPE;
	}

	// Sanitize a UTF-8 name for use as a download file name
	function safe_name(name, type)
	{
		var s = typeof name == "string" ? name : "";
		s = s.substr(Math.max(s.lastIndexOf('/'), s.lastIndexOf('\\')) + 1);
		s = s.replace(/[\x00-\x1F\x7F:*?"<>|]/g, "").replace(/\xC2[\x80-\x9F]/g, "");
		s = s.replace(FORMAT_RE, "");
		s = s.replace(/^[ \t]+/, "").replace(/^\.+/, "").replace(/[ \t.]+$/, "");
		if (s.length > MAX_NAME) {
			var cut = MAX_NAME;
			while (cut > 0 && (s.charCodeAt(cut) & 0xC0) == 0x80)
				cut--;
			s = s.substr(0, cut).replace(/[ \t.]+$/, "");
		}
		if (s === "")
			s = "attachment" + (EXTS.hasOwnProperty(type) ? EXTS[type] : "");
		if (DEVICE_RE.test(s.split(".")[0].replace(/[ \t]+$/, "")))
			s = "_" + s;
		return s;
	}

	function part_name(part)
	{
		return safe_name(decode_words(String(part.name || "")), safe_type(part.content_type));
	}

	function model(parts)
	{
		var m = { text_id: null, html_id: null, attachments: [] };
		if (!(parts instanceof Array))
			return m;
		for (var i = 0; i < parts.length; i++) {
			var p = parts[i];
			if (p === null || typeof p != "object" || typeof p.id != "string")
				continue;
			var type = safe_type(p.content_type);
			if (p.disposition !== "attachment") {
				if (type == "text/plain" && m.text_id === null) {
					m.text_id = p.id;
					continue;
				}
				if (type == "text/html" && m.html_id === null) {
					m.html_id = p.id;
					continue;
				}
			}
			var cid = typeof p.content_id == "string" ? p.content_id : "";
			m.attachments.push({ pid: p.id, name: part_name(p), type: type,
				size: typeof p.size == "number" ? p.size : 0, cid: cid, inline: cid !== "" });
		}
		return m;
	}

	// The directory holding a file-attached message's files; null if the
	// header names no user
	function attach_dir(hdr, data_dir)
	{
		var to = parseInt(hdr.to_ext, 10);
		if (to > 0)
			return data_dir + format("file/%04u.in/", to);
		var from = parseInt(hdr.from_ext, 10);
		if (from > 0)
			return data_dir + format("file/%04u.out/", from);
		return null;
	}

	function file_parts(hdr, data_dir)
	{
		var out = [];
		if (hdr === null || typeof hdr != "object")
			return out;
		var subject = hdr.subject;	// a non-NULL field first (lazy fields)
		var auxattr = hdr.auxattr;
		if (!(auxattr & MSG_FILEATTACH))
			return out;
		var dir = attach_dir(hdr, data_dir);
		var names = Summary.attach_names(subject || "");
		for (var i = 0; i < names.length; i++) {
			var n = names[i];
			var shown = (auxattr & MSG_HFIELDS_UTF8) ? n : utf8_encode(n);
			if (!str_is_utf8(shown))
				shown = raw_to_utf8(shown);
			var path = null;
			if (dir !== null && !/[\x00-\x1F\x7F:\/\\]/.test(n))
				path = dir + n;
			var size = -1;
			if (path !== null && file_exists(path) && !file_isdir(path))
				size = file_size(path);
			var name = safe_name(shown, DEFAULT_TYPE);
			out.push({ pid: "f" + (i + 1), name: name, type: type_of_name(n), size: size, path: path });
		}
		return out;
	}

	return {
		model: model,
		file_parts: file_parts,
		part_name: part_name,
		safe_name: safe_name,
		safe_type: safe_type,
		decode_words: decode_words
	};
})();

MimeRead;
