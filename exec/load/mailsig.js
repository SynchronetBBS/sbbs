// Mail signature for the mailbox library, with fallback to the user's
// general signature.
//
// var MailSig = load("mailsig.js");
//
// The mail signature is <data_dir>user/####.mail.sig (the terminal server's
// per-area naming, "mail" standing in for a sub-board code) and is stored as
// UTF-8. When it does not exist the user's general signature
// <data_dir>user/####.sig is used; the terminal's editors write that one,
// normally in CP437, with Ctrl-A attribute codes. The general file is never
// written here.
//
// String contract: text is a UTF-8 byte string (each char code 0-255) in
// and out, with "\n" line breaks. A char code above 0xFF throws
// { code: "invalid" }. The 2000 character limit counts UTF-8 characters
// (bytes that are not continuation bytes 0x80-0xBF), not bytes. Files are
// read and written in binary mode; lines are written separated by "\r\n"
// (as the terminal's editors write ####.sig).
//
// Errors are thrown as { code, message }: "invalid" for a bad argument,
// "failed" (with a fixed message; the detail is logged) when the file
// cannot be written or removed.

"use strict";

var MailSig = (function () {
	// Include user signatures in e-mail msgs: MM_EMAILSIG in
	// src/sbbs3/sbbsdefs.h (exec/load/sbbsdefs.js), a bit of
	// msg_area.settings. Defined here so this library loads no sibling.
	var MM_EMAILSIG = (1 << 17);
	var MAX_LENGTH = 2000;
	var seq = 0;
	var MAX_USERNUM = 0xFFFFFFFF;

	function invalid(message)
	{
		return { code: "invalid", message: message };
	}

	// A file that could not be written or removed: the detail (it names a
	// server path) is logged, the caller gets a fixed message
	function failed(detail)
	{
		log(LOG_ERR, "mailsig: " + detail);
		return { code: "failed", message: "the signature could not be saved" };
	}

	function slash(p)
	{
		var c = p.charAt(p.length - 1);
		return (c == '/' || c == '\\') ? p : p + '/';
	}

	function data_path(data_dir)
	{
		if (data_dir === undefined || data_dir === null)
			data_dir = system.data_dir;
		if (typeof data_dir != "string" || data_dir === "")
			throw invalid("bad data directory");
		return slash(data_dir);
	}

	function user_path(usernum, data_dir, ext)
	{
		if (typeof usernum != "number" || !isFinite(usernum)
		    || Math.floor(usernum) != usernum || usernum < 1 || usernum > MAX_USERNUM)
			throw invalid("bad user number");
		return data_path(data_dir) + format("user/%04u", usernum) + ext;
	}

	function read_file(path)
	{
		if (!file_exists(path))
			return null;
		var f = new File(path);
		if (!f.open("rb"))
			return null;
		var s = f.read();
		f.close();
		return (s === undefined || s === null) ? "" : s;
	}

	function normalize_eol(s)	// CRLF, CR and LF all become LF
	{
		return s.replace(/\r\n?/g, "\n");
	}

	function char_count(s)	// UTF-8 characters: bytes other than 0x80-0xBF
	{
		var n = 0;
		for (var i = 0; i < s.length; i++) {
			var c = s.charCodeAt(i);
			if (c < 0x80 || c > 0xBF)
				n++;
		}
		return n;
	}

	function is_byte_string(s)
	{
		for (var i = 0; i < s.length; i++) {
			if (s.charCodeAt(i) > 0xFF)
				return false;
		}
		return true;
	}

	// The general signature as UTF-8 text with LF line breaks
	function general_text(raw)
	{
		// A Ctrl-A immediately followed by ESC leaves the rest of that
		// ANSI sequence as text (strip_ctrl_a eats the ESC).
		raw = strip_ansi(strip_ctrl_a(raw));
		if (!str_is_ascii(raw) && !str_is_utf8(raw))
			raw = utf8_encode(raw);	// CP437
		return normalize_eol(raw);
	}

	var M = {
		MAX_LENGTH: MAX_LENGTH
	};

	// { text, source } where source is "mail", "general" or "none"
	M.get = function (usernum, data_dir)
	{
		var raw = read_file(user_path(usernum, data_dir, ".mail.sig"));
		if (raw !== null)
			return { text: normalize_eol(raw), source: "mail" };
		raw = read_file(user_path(usernum, data_dir, ".sig"));
		if (raw !== null)
			return { text: general_text(raw), source: "general" };
		return { text: "", source: "none" };
	};

	// text: writes the mail signature ("" means no signature on mail);
	// null: removes it, so the general signature applies again.
	M.set = function (usernum, text, data_dir)
	{
		var path = user_path(usernum, data_dir, ".mail.sig");
		if (text === null) {
			if (file_exists(path) && !file_remove(path))
				throw failed("cannot remove " + path);
			return;
		}
		if (typeof text != "string")
			throw invalid("signature must be a string or null");
		if (!is_byte_string(text))
			throw invalid("signature is not a UTF-8 byte string");
		// validity first: stray continuation bytes would count as no characters
		if (!str_is_ascii(text) && !str_is_utf8(text))
			throw invalid("signature is not valid UTF-8");
		if (char_count(text) > MAX_LENGTH)
			throw invalid("signature is longer than " + MAX_LENGTH + " characters");

		var out = normalize_eol(text).replace(/\n/g, "\r\n");
		var tmp = path + "." + system.timer + "-" + random(0x7fffffff) + "-" + (seq++) + ".tmp";
		var f = new File(tmp);
		if (!f.open("wb"))
			throw failed("cannot create " + tmp);
		var ok = out.length === 0 || f.write(out);
		f.close();
		if (!ok || file_size(tmp) != out.length) {
			file_remove(tmp);
			throw failed("cannot write " + tmp);
		}
		if (!file_rename(tmp, path)) {
			// rename does not replace an existing file on Windows
			file_remove(path);
			if (!file_rename(tmp, path))
				throw failed("cannot replace " + path + "; new signature kept in " + tmp);
		}
	};

	// Is the sysop option "Include Signatures in E-mail" on?
	M.enabled = function ()
	{
		return (msg_area.settings & MM_EMAILSIG) != 0;
	};

	return M;
})();

MailSig;
