// MimeWrite: the body of a message being sent, as the mail base stores it
// and the mail server sends it, for the web mail client.  A pure library:
// no web or session globals.
//
// var MimeWrite = load("mimewrite.js");
//
// How MIME reaches the mail server: the stored message's body is the MIME
// body (no top-level header lines in it).  Its MIME header lines are
// header fields of the stored message: mime_version and content_type
// (MsgBase.save_msg() stores them as RFC822HEADER lines
// "MIME-Version: 1.0" and "Content-Type: ...") and, for a single text
// part, a "Content-Transfer-Encoding: ..." RFC822HEADER line in
// field_list.  The mail server writes every RFC822HEADER line into the
// header it sends (mailsrvr.cpp sockmsgtxt()), adds no Content-Type of
// its own when the message has one, dot-stuffs the body and strips the
// white space at the end of every body line it sends.  The header fields
// for build()'s output are in its result's "fields".
//
// Interface:
//   MimeWrite.text(body)            body (a UTF-8 byte string) with every
//                                   line break as CRLF: CR LF, a bare CR
//                                   and a bare LF each become CR LF
//   MimeWrite.encoding(text)        "7bit", "8bit" or "quoted-printable"
//                                   for text from text() (see below)
//   MimeWrite.qp(text)              text as quoted-printable (CRLF lines)
//   MimeWrite.estimate(body, files) an upper bound of build()'s output
//                                   size in bytes, without reading a file
//   MimeWrite.build(body, files, out_path)
//                                   writes the MIME body to out_path;
//                                   returns {content_type, mime_version,
//                                   encoding, fields, size}
//   MimeWrite.param(name, value)    one header parameter, name="value",
//                                   the value safe (see "File names")
//
// body is the message text: a UTF-8 byte string (char codes 0-255).
// files is [{name, type, path, size}] (Drafts.claim()'s files); each
// file's bytes are read from path in pieces of 57 x 1024 bytes, encoded
// to base64 and written, so no attachment is ever held whole in memory.
//
// Without files: one text/plain; charset=utf-8 part.  With files:
// multipart/mixed with the text part first, then each file as
// "Content-Type: <type>; name=..." and "Content-Disposition: attachment;
// filename=...", base64 in lines of 76 characters.
//
// The text's transfer encoding (encoding()): quoted-printable when any
// line is longer than 998 bytes (the mail server cuts longer lines), ends
// in a space or tab (the mail server strips those), starts with "From "
// (mbox readers along the way would change it) or the text holds a control
// character other than tab, CR and LF (NUL included); else "8bit" when it
// has any byte above 0x7F, else "7bit".  Quoted-printable output lines
// are at most 76 characters; "=", control characters, bytes above 0x7E, a
// space or tab before a line break, the "F" of a line starting "From "
// and a "." starting a line are written as =XX.  format=flowed is not
// used: it depends on trailing spaces, which the mail server removes from
// every line it sends.
//
// The boundary is "=_wm" plus random hexadecimal digits.  "=_" never
// occurs in base64 or quoted-printable output; the text part, when sent
// as 7bit or 8bit, is checked and a new boundary chosen until it does not
// contain the boundary.
//
// File names: a name of printable ASCII without '"', '\' or "=?" is
// written as a quoted string as it is; any other name as RFC 2047 "B"
// encoded words (UTF-8) inside the quotes, which the readers in this tree
// (MsgBase.get_msg_parts() with mailbox_mime.js decode_words()) and common
// mail clients understand.  Control characters (C0, DEL, C1, U+2028/9 and
// the bidirectional controls) are removed first; an empty result is
// "attachment".  A type that is not a plain type/subtype, or is
// multipart/* or message/*, is application/octet-stream.  So nothing a client sends can add a header
// line or parameter.
//
// Errors are thrown as { code: "failed", message } (the detail logged
// through MimeWrite.log); out_path is removed on failure.

"use strict";

var MimeWrite = (function () {
	var PIECE = 57 * 1024;	// bytes read and encoded at a time (whole 76-column lines)
	var LINE = 76;
	var MAX_LINE = 998;
	var TYPE_RE = /^[a-z0-9][a-z0-9!#$&^_.+-]{0,63}\/[a-z0-9][a-z0-9!#$&^_.+-]{0,63}$/;
	var SAFE_NAME_RE = /^[\x20-\x7E]+$/;
	// C0, DEL, C1 (as UTF-8), U+2028/9 and the bidirectional controls
	var CONTROL_RE = /[\x00-\x1F\x7F]|\xC2[\x80-\x9F]|\xE2\x80[\xA8\xA9\x8E\x8F\xAA-\xAE]|\xE2\x81[\xA6-\xA9]|\xD8\x9C/g;
	var HEX = "0123456789ABCDEF";

	function failed(what)
	{
		MimeWrite.log(LOG_ERR, "mimewrite: " + what);
		return { code: "failed", message: "the message could not be built" };
	}

	function text(body)
	{
		return String(body).replace(/\r\n|\r|\n/g, "\r\n");
	}

	function lines_of(t)
	{
		return t.split("\r\n");
	}

	function encoding(t)
	{
		var high = false;
		var lines = lines_of(t);
		for (var i = 0; i < lines.length; i++) {
			var l = lines[i];
			if (l.length > MAX_LINE || /[ \t]$/.test(l) || l.substr(0, 5) == "From ")
				return "quoted-printable";
			if (/[\x00-\x08\x0A-\x1F\x7F]/.test(l))
				return "quoted-printable";
			if (!high && /[\x80-\xFF]/.test(l))
				high = true;
		}
		return high ? "8bit" : "7bit";
	}

	function hex(c)
	{
		return "=" + HEX.charAt((c >> 4) & 15) + HEX.charAt(c & 15);
	}

	// One line (no line break in it) as quoted-printable lines joined with
	// soft breaks
	function qp_line(l)
	{
		var out = [];
		var cur = "";
		for (var i = 0; i < l.length; i++) {
			var c = l.charCodeAt(i) & 0xFF;
			var tok;
			var last = i == l.length - 1;
			if (c == 0x3D || c < 0x20 && c != 0x09 || c > 0x7E
				|| ((c == 0x20 || c == 0x09) && last)
				|| (i == 0 && (c == 0x2E || (c == 0x46 && l.substr(0, 5) == "From "))))
				tok = hex(c);
			else
				tok = String.fromCharCode(c);
			// a soft break ("=") must fit on the line: at most 75 + "="
			if (cur.length + tok.length > LINE - 1) {
				out.push(cur + "=");
				cur = "";
				// a continuation line starting with "." or "From " is
				// harmless after a soft break only if encoded: done below
				if (tok == "." || (tok == "F" && l.substr(i, 5) == "From "))
					tok = hex(c);
			}
			cur += tok;
		}
		out.push(cur);
		return out.join("\r\n");
	}

	function qp(t)
	{
		var lines = lines_of(t);
		var out = [];
		for (var i = 0; i < lines.length; i++)
			out.push(qp_line(lines[i]));
		return out.join("\r\n");
	}

	function encoded_words(s)
	{
		var words = [];
		while (s.length > 0) {
			var n = Math.min(45, s.length);
			while (n < s.length && n > 0 && (s.charCodeAt(n) & 0xC0) == 0x80)
				n--;	// whole UTF-8 characters
			if (n == 0)
				n = Math.min(45, s.length);
			words.push("=?UTF-8?B?" + base64_encode(s.substr(0, n)) + "?=");
			s = s.substr(n);
		}
		return words.join(" ");
	}

	function clean_name(name)
	{
		var s = String(name === undefined || name === null ? "" : name);
		var out = "";
		for (var i = 0; i < s.length; i++) {
			var c = s.charCodeAt(i);
			out += String.fromCharCode(c > 0xFF ? 0x3F : c);	// not a byte string: '?'
		}
		out = out.replace(CONTROL_RE, "").replace(/^[ ]+|[ ]+$/g, "");
		return out === "" ? "attachment" : out;
	}

	function param(name, value)
	{
		var v = clean_name(value);
		if (SAFE_NAME_RE.test(v) && !/["\\]/.test(v) && v.indexOf("=?") < 0)
			return name + '="' + v + '"';
		return name + '="' + encoded_words(v) + '"';
	}

	function safe_type(type)
	{
		var t = String(type === undefined || type === null ? "" : type);
		// multipart/* and message/* would make a reader parse the
		// attachment's bytes as MIME structure
		if (!TYPE_RE.test(t) || /^(multipart|message)\//.test(t))
			return "application/octet-stream";
		return t;
	}

	function b64_size(n)
	{
		var chars = Math.ceil(n / 3) * 4;
		return chars + Math.ceil(chars / LINE) * 2;
	}

	function estimate(body, files)
	{
		var t = text(body);
		var size = (encoding(t) == "quoted-printable" ? t.length * 3 + Math.ceil(t.length / 75) * 3 : t.length) + 1024;
		for (var i = 0; i < (files || []).length; i++)
			size += b64_size(Math.max(0, files[i].size || 0)) + 1024;
		return size;
	}

	function new_boundary()
	{
		var s = "=_wm";
		for (var i = 0; i < 6; i++)
			s += format("%04x", random(0x10000));
		return s;
	}

	function Writer(path)
	{
		this.path = path;
		this.f = new File(path);
		if (!this.f.open("wb"))
			throw failed("cannot create " + path);
		this.size = 0;
	}

	Writer.prototype.put = function (s)
	{
		if (s.length > 0 && !this.f.write(s))
			throw failed("cannot write " + this.path);
		this.size += s.length;
	};

	Writer.prototype.close = function ()
	{
		this.f.close();
	};

	// One file as base64 lines, read in pieces
	function put_file(w, file)
	{
		var f = new File(file.path);
		if (!f.open("rb"))
			throw failed("cannot open attachment " + file.path);
		var total = 0;
		try {
			for (;;) {
				var chunk = f.read(PIECE);
				if (typeof chunk != "string" || chunk.length == 0)
					break;
				total += chunk.length;
				var enc = base64_encode(chunk);
				if (typeof enc != "string")
					throw failed("cannot encode attachment " + file.path);
				var out = "";
				for (var i = 0; i < enc.length; i += LINE)
					out += enc.substr(i, LINE) + "\r\n";
				w.put(out);
				if (chunk.length < PIECE)
					break;
			}
			if (f.error)
				throw failed("cannot read attachment " + file.path);
		} finally {
			f.close();
		}
		if (typeof file.size == "number" && total != file.size)
			throw failed("attachment " + file.path + " is " + total + " bytes, not " + file.size);
	}

	function build(body, files, out_path)
	{
		files = files || [];
		var t = text(body);
		var enc = encoding(t);
		var part_text = enc == "quoted-printable" ? qp(t) : t;
		var w = new Writer(out_path);
		var ok = false;
		var result;
		try {
			if (files.length == 0) {
				w.put(part_text);
				result = { content_type: "text/plain; charset=utf-8", encoding: enc };
			} else {
				var b;
				do {
					b = new_boundary();
				} while (part_text.indexOf(b) >= 0);
				w.put("This is a multi-part message in MIME format.\r\n\r\n--" + b + "\r\n"
					+ "Content-Type: text/plain; charset=utf-8\r\n"
					+ "Content-Transfer-Encoding: " + enc + "\r\n\r\n"
					+ part_text + "\r\n");
				for (var i = 0; i < files.length; i++) {
					var f = files[i];
					var type = safe_type(f.type);
					w.put("--" + b + "\r\n"
						+ "Content-Type: " + type + ";\r\n " + param("name", f.name) + "\r\n"
						+ "Content-Transfer-Encoding: base64\r\n"
						+ "Content-Disposition: attachment;\r\n " + param("filename", f.name) + "\r\n\r\n");
					put_file(w, f);
				}
				w.put("--" + b + "--\r\n");
				result = { content_type: "multipart/mixed; boundary=\"" + b + "\"", encoding: "7bit" };
			}
			ok = true;
		} finally {
			w.close();
			if (!ok)
				file_remove(out_path);
		}
		result.mime_version = "1.0";
		result.size = w.size;
		result.fields = [];
		if (files.length == 0 && enc != "7bit")
			result.fields.push({ type: 0xB0 /* RFC822HEADER */, data: "Content-Transfer-Encoding: " + enc });
		return result;
	}

	var MimeWrite = {
		PIECE: PIECE,
		text: text,
		encoding: encoding,
		qp: qp,
		estimate: estimate,
		param: param,
		safe_type: safe_type,
		build: build,
		log: function (level, s) {
			log(level, s);
		}
	};

	return MimeWrite;
})();

MimeWrite;
