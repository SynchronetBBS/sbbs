// Address: recipient addresses, the sender's identity and header safety,
// for the web mail client.
//
// var Address = load("address.js");
//
// A pure library: no web or session globals, nothing written.  Only
// system_resolvers(), domains_for() and identity() without its last
// argument read the system's objects (system, msg_area, User), and each
// takes them as an argument so a test can pass fakes.
//
// String contract: text is UTF-8 BYTE strings (char codes 0-255), the form
// the web server and the mailbox library use.  Nothing here uses \s,
// trim(), toLowerCase() or toUpperCase() (in this engine they treat byte
// 0xA0 as white space and fold bytes 0xC0-0xDE); white space is ASCII
// space and tab only and case folding is ASCII only, as in mailbox_tags.js.
//
// Header safety: nothing returned contains a control character: C0
// (0x00-0x1F, so no CR, LF, NUL or tab), DEL, C1 (U+0080-U+009F as UTF-8),
// U+2028/U+2029, or the bidirectional controls U+061C, U+200E, U+200F,
// U+202A-U+202E, U+2066-U+2069.  clean() removes them from free text;
// parse_list(), resolve(), format() and from_for() refuse an address
// containing one, since removing it would change the address.
//
// Invalid UTF-8 (deterministic): clean() replaces each maximal ill-formed
// subsequence (the W3C/WHATWG decoder rule: a lead byte with the valid
// continuation bytes that follow it, or one stray byte) with U+FFFD
// (EF BF BD), and any char code above 0xFF likewise; overlong forms,
// surrogates and values above U+10FFFF are ill-formed.  parse_list()
// refuses an entry with invalid UTF-8 (reason "utf8").  Addresses are
// printable ASCII only.
//
// Limits, in bytes, never splitting a UTF-8 sequence: subject 255, a name
// 64, an address 254 (local part 64), a file name 128 (MAX_* below).
// Names and free text are cut (clean); an address over its limit is
// refused (reason "toolong").  parse_list() takes at most MAX_ENTRIES
// (100) non-empty entries and MAX_INPUT (16384) bytes of input.
//
// Errors are thrown as { code, message }: code "invalid" or "forbidden".
// parse_list() and resolve() add  reason  (one of: syntax, control, utf8,
// unbalanced, group, route, toolong, toomany, unknown),  index  (the
// entry's position among the non-empty entries, from 0; -1 for the input
// as a whole) and  entry  (the entry's text through clean(), 64 bytes).
// The message names the entry, cleaned the same way.
//
// Interface:
//   Address.clean(text [, max_len])     text without control characters,
//                                       invalid UTF-8 replaced, trimmed,
//                                       cut to max_len bytes
//   Address.parse_list(string)          [{name, addr}]
//   Address.resolve(entry, matchuser_fn, netaddr_type_fn [, own])
//                                       {kind:"local", usernum, name} or
//                                       {kind:"internet"|"fido"|"qwk",
//                                       addr, name, net_type}
//   Address.system_resolvers([env])     {matchuser, netaddr_type, own}
//                                       for resolve(), from the system
//   Address.format(name, addr)          one mailbox for a To/CC header line
//   Address.format_list(entries)        mailboxes joined with ", "
//   Address.domains(list, can_send_fn)  the From domains a user may use
//   Address.domains_for(usernum [, env])  the same from the system objects
//   Address.from_for(local_part, domain, allowed)  the From address
//   Address.reply_domain(to_domain, default_domain, allowed)
//   Address.is_me(addr, alias, allowed)
//   Address.identity(user, kind, settings [, sys])  {from, from_ext}
//   Address.fido_canonical(text)        zone:net/node[.point] as stored
//   Address.display_name(text)          a name safe inside the mail
//                                       server's quoted From/To form
//
// Each function is described where it is defined.

"use strict";

var Address = (function () {
	var A = {
		MAX_SUBJECT: 255,
		MAX_NAME: 64,
		MAX_ADDR: 254,
		MAX_LOCAL: 64,
		MAX_FILENAME: 128,
		MAX_ENTRIES: 100,
		MAX_INPUT: 16384,
		// smbdefs.js
		NET_NONE: 0,
		NET_UNKNOWN: 1,
		NET_FIDO: 2,
		NET_QWK: 4,
		NET_INTERNET: 5,
		// sbbsdefs.js
		NMAIL_ALIAS: (1 << 4),
		MM_REALNAME: (1 << 16),
		U_NAME: 2,
		// userdefs.js
		UREST_REAL_NAME: (1 << 14),
		USER_DELETED: (1 << 0),
		USER_INACTIVE: (1 << 6)
	};
	var FFFD = "\xEF\xBF\xBD";

	// ---------------------------------------------------------------
	// Byte-string helpers (ASCII-only white space and case)

	function lower(s)
	{
		return s.replace(/[A-Z]/g, function (c) {
			return String.fromCharCode(c.charCodeAt(0) + 32);
		});
	}

	// Linear: a regular expression anchored at the end ([ \t]+$) would try
	// every run of white space inside the text
	function trim(s)
	{
		var a = 0, b = s.length;
		while (a < b && (s.charCodeAt(a) == 0x20 || s.charCodeAt(a) == 0x09))
			a++;
		while (b > a && (s.charCodeAt(b - 1) == 0x20 || s.charCodeAt(b - 1) == 0x09))
			b--;
		return (a == 0 && b == s.length) ? s : s.substring(a, b);
	}

	function squeeze(s)	// runs of space and tab to one space, trimmed
	{
		return trim(s.replace(/[ \t]+/g, " "));
	}

	function str(v)
	{
		return (v === undefined || v === null) ? "" : String(v);
	}

	// C0 (except tab, which is white space to the parser), DEL
	var C0 = /[\x00-\x08\x0A-\x1F\x7F]/;
	// C1, U+2028/U+2029 and the bidirectional controls, as UTF-8
	var CONTROL_UTF8 = /\xC2[\x80-\x9F]|\xD8\x9C|\xE2\x80[\x8E\x8F\xA8-\xAE]|\xE2\x81[\xA6-\xA9]/;
	var CONTROL_UTF8_G = new RegExp(CONTROL_UTF8.source, "g");

	// Length of the valid UTF-8 sequence at s[i], or -n for an ill-formed
	// one whose maximal subpart is n bytes (n >= 1)
	function utf8_seq(s, i)
	{
		var c = s.charCodeAt(i);
		if (c < 0x80)
			return 1;
		var need, lo = 0x80, hi = 0xBF;
		if (c >= 0xC2 && c <= 0xDF)
			need = 1;
		else if (c >= 0xE0 && c <= 0xEF) {
			need = 2;
			if (c == 0xE0)
				lo = 0xA0;
			else if (c == 0xED)
				hi = 0x9F;
		} else if (c >= 0xF0 && c <= 0xF4) {
			need = 3;
			if (c == 0xF0)
				lo = 0x90;
			else if (c == 0xF4)
				hi = 0x8F;
		} else
			return -1;	// a continuation byte, C0, C1, F5-FF or a wide char
		for (var k = 1; k <= need; k++) {
			if (i + k >= s.length)
				return -k;
			var b = s.charCodeAt(i + k);
			if (b < lo || b > hi)
				return -k;
			lo = 0x80;
			hi = 0xBF;
		}
		return need + 1;
	}

	function utf8_valid(s)
	{
		for (var i = 0; i < s.length;) {
			var n = utf8_seq(s, i);
			if (n < 0)
				return false;
			i += n;
		}
		return true;
	}

	function utf8_fix(s)
	{
		var out = "";
		var start = 0;
		for (var i = 0; i < s.length;) {
			var n = utf8_seq(s, i);
			if (n > 0) {
				i += n;
				continue;
			}
			out += s.substring(start, i) + FFFD;
			i += -n;
			start = i;
		}
		return out + s.substring(start);
	}

	// Cut a valid UTF-8 string to at most max bytes at a character boundary
	function cut(s, max)
	{
		if (s.length <= max)
			return s;
		var end = max;
		while (end > 0) {
			var c = s.charCodeAt(end);
			if (c < 0x80 || c > 0xBF)
				break;
			end--;
		}
		return s.substr(0, end);
	}

	// clean(text [, max_len]): text as a string (undefined and null give
	// ""), invalid UTF-8 replaced with U+FFFD, every control character
	// removed (see the top of this file), ASCII white space trimmed, cut to
	// max_len bytes without splitting a character and trimmed again.
	// Without max_len, not cut.  Free text bound for a header (subject,
	// names, file names) passes through this.
	A.clean = function (text, max_len)
	{
		var s = utf8_fix(str(text));
		s = s.replace(/[\x00-\x1F\x7F]/g, "").replace(CONTROL_UTF8_G, "");
		s = trim(s);
		if (typeof max_len === "number" && max_len >= 0)
			s = trim(cut(s, max_len));
		return s;
	};

	// ---------------------------------------------------------------
	// Address syntax

	// Local part: RFC 5322 atext and dots, ASCII only.  Host: letters,
	// digits, '.', '-' (a domain or QWK id), and ':' '/' for FidoNet and
	// QWK routes ("1:103/705", "VERT/NIX").  No quoted local parts, domain
	// literals, comments or white space: one written form per address.
	var LOCAL_RE = /^[A-Za-z0-9!#$%&'*+\-\/=?\^_`{|}~.]+$/;
	var HOST_RE = /^[A-Za-z0-9.\-:\/]+$/;
	var HOSTNAME_RE = /^[A-Za-z0-9.\-]+$/;
	// FidoNet: zone:net/node[.point] only (smb_netaddr_type also takes
	// "net/node", "node" and "node.point" relative to the system's own
	// address; those are refused here)
	var FIDO_RE = /^[0-9]+:[0-9]+\/[0-9]+(\.[0-9]+)?$/;
	// QWK: one QWK id (1-8 letters and digits, a letter first); routes
	// ("VERT/NIX") are refused
	var QWKID_RE = /^[A-Za-z][A-Za-z0-9]{0,7}$/;
	var LABEL_RE = /^[A-Za-z0-9]([A-Za-z0-9\-]{0,61}[A-Za-z0-9])?$/;

	// An Internet mail domain: labels of letters, digits and hyphens (not
	// at either end), 1-63 bytes each, at least two labels, 253 bytes in
	// all, the last label beginning with a letter (so no IP address or
	// other numeric-looking host, e.g. "0x7f.1"; "xn--" labels are fine).
	// No ':' (the mail sender would take "host:port" as a port), no '/',
	// no IP literal.
	function plain_host(h)
	{
		if (h.length < 1 || h.length > 253)
			return false;
		var labels = h.split(".");
		if (labels.length < 2)
			return false;
		for (var i = 0; i < labels.length; i++) {
			if (!LABEL_RE.test(labels[i]))
				return false;
		}
		return /^[A-Za-z]/.test(labels[labels.length - 1]);
	}
	var ATEXT_PHRASE_RE = /^[A-Za-z0-9!#$%&'*+\-\/=?\^_`{|}~ ]+$/;

	function fail(code, reason, message, index, entry)
	{
		var e = { code: code, message: message, reason: reason };
		if (index === -1) {	// the input as a whole
			e.index = -1;
			e.entry = "";
		} else if (index !== undefined) {
			e.index = index;
			e.entry = A.clean(entry, A.MAX_NAME);
			e.message += ": entry " + (index + 1) + " \"" + e.entry + "\"";
		}
		return e;
	}

	// null if addr (an address with '@') is acceptable, else a reason
	function addr_problem(addr)
	{
		if (C0.test(addr) || addr.indexOf("\t") >= 0 || CONTROL_UTF8.test(addr))
			return "control";
		if (!/^[\x21-\x7E]+$/.test(addr))
			return "syntax";
		var at = addr.indexOf("@");
		if (at < 0 || addr.indexOf("@", at + 1) >= 0)
			return "syntax";
		var local = addr.substr(0, at);
		var host = addr.substr(at + 1);
		if (local === "" || host === "" || !LOCAL_RE.test(local) || !HOST_RE.test(host))
			return "syntax";
		if (addr.length > A.MAX_ADDR || local.length > A.MAX_LOCAL)
			return "toolong";
		return null;
	}

	// null if text (an entry with no '@': a local name or number, or a
	// bare FidoNet address) is acceptable, else a reason
	function bare_problem(text)
	{
		if (C0.test(text) || CONTROL_UTF8.test(text))
			return "control";
		if (!utf8_valid(text))
			return "utf8";
		if (text === "" || /[\t<>"()\[\]\\,;@]/.test(text))
			return "syntax";
		if (text.indexOf(":") >= 0 && !FIDO_RE.test(text))
			return /^[0-9:\/.]+$/.test(text) ? "syntax" : "group";
		if (text.length > A.MAX_ADDR)
			return "toolong";
		return null;
	}

	// Split at ',' and ';' outside quotes, angle brackets and comments.
	// An unterminated quote, angle or comment runs to the end of the input
	// (the entry parser then reports it).
	function split_entries(s)
	{
		var list = [];
		var cur = "";
		var quote = false, angle = false, paren = 0;
		for (var i = 0; i < s.length; i++) {
			var ch = s.charAt(i);
			if ((quote || paren) && ch == "\\" && i + 1 < s.length) {
				cur += ch + s.charAt(++i);
				continue;
			}
			if (quote) {
				if (ch == '"')
					quote = false;
			} else if (paren) {
				if (ch == "(")
					paren++;
				else if (ch == ")")
					paren--;
			} else if (ch == '"')
				quote = true;
			else if (ch == "(")
				paren++;
			else if (ch == "<")
				angle = true;
			else if (ch == ">")
				angle = false;
			else if (!angle && (ch == "," || ch == ";")) {
				list.push(cur);
				cur = "";
				continue;
			}
			cur += ch;
		}
		list.push(cur);
		return list;
	}

	// One non-empty entry to {name, addr}, or a thrown error
	function parse_entry(raw, index)
	{
		function bad(reason, what)
		{
			return fail("invalid", reason, what, index, raw);
		}
		if (C0.test(raw) || CONTROL_UTF8.test(raw))
			throw bad("control", "control character in an address");
		if (!utf8_valid(raw))
			throw bad("utf8", "invalid UTF-8 in an address");

		var tokens = [];	// {quoted, text} before any angle address
		var comment = null;
		var addr = null;	// the text inside <>, if any
		var after_angle = false;
		var i = 0, ch, t;
		while (i < raw.length) {
			ch = raw.charAt(i);
			if (ch == " " || ch == "\t") {
				i++;
				continue;
			}
			if (ch == '"') {
				if (after_angle)
					throw bad("syntax", "text after the address");
				t = "";
				for (i++; i < raw.length && raw.charAt(i) != '"'; i++) {
					if (raw.charAt(i) == "\\" && i + 1 < raw.length)
						i++;
					t += raw.charAt(i);
				}
				if (i >= raw.length)
					throw bad("unbalanced", "unbalanced quote");
				i++;
				if (i < raw.length && raw.charAt(i) != " " && raw.charAt(i) != "\t"
					&& raw.charAt(i) != "<" && raw.charAt(i) != "(")
					throw bad("syntax", "quoted text joined to other text");
				tokens.push({ quoted: true, text: t });
				continue;
			}
			if (ch == "(") {
				t = "";
				for (i++; i < raw.length && raw.charAt(i) != ")"; i++) {
					if (raw.charAt(i) == "(")
						throw bad("syntax", "nested comment");
					if (raw.charAt(i) == "\\" && i + 1 < raw.length)
						i++;
					t += raw.charAt(i);
				}
				if (i >= raw.length)
					throw bad("unbalanced", "unbalanced parenthesis");
				i++;
				if (comment === null)
					comment = t;
				continue;
			}
			if (ch == "<") {
				if (addr !== null)
					throw bad("syntax", "more than one address");
				t = "";
				for (i++; i < raw.length && raw.charAt(i) != ">"; i++) {
					if (raw.charAt(i) == "<")
						throw bad("syntax", "nested angle bracket");
					t += raw.charAt(i);
				}
				if (i >= raw.length)
					throw bad("unbalanced", "unbalanced angle bracket");
				i++;
				addr = trim(t);
				after_angle = true;
				continue;
			}
			if (ch == ">" || ch == ")")
				throw bad("unbalanced", "unbalanced " + (ch == ">" ? "angle bracket" : "parenthesis"));
			if (after_angle)
				throw bad("syntax", "text after the address");
			t = "";
			while (i < raw.length && " \t\"(<>)".indexOf(raw.charAt(i)) < 0)
				t += raw.charAt(i++);
			if (i < raw.length && raw.charAt(i) == '"')
				throw bad("syntax", "quoted text joined to other text");
			tokens.push({ quoted: false, text: t });
		}

		var name, k;
		if (addr !== null) {
			// Display name and <address>
			for (k = 0; k < tokens.length; k++) {
				if (tokens[k].quoted)
					continue;
				if (tokens[k].text.indexOf(":") >= 0)
					throw bad("group", "group syntax");
				if (/[@\[\]\\]/.test(tokens[k].text))
					throw bad("syntax", "special character in a name");
			}
			if (addr.charAt(0) == "@")
				throw bad("route", "source route");
			if (addr === "")
				throw bad("syntax", "empty address");
			var p = addr_problem(addr);
			if (p !== null)
				throw bad(p, "invalid address");
			var parts = [];
			for (k = 0; k < tokens.length; k++)
				parts.push(tokens[k].text);
			name = parts.join(" ");
			if (name === "" && comment !== null)
				name = comment;
		} else {
			// A bare address or local name, perhaps with a comment
			var words = [];
			for (k = 0; k < tokens.length; k++) {
				if (tokens[k].quoted)
					throw bad("syntax", "quoted text without an address");
				words.push(tokens[k].text);
			}
			if (words.length > 1)
				for (k = 0; k < words.length; k++)
					if (words[k].indexOf(":") >= 0)
						throw bad("group", "group syntax");
			var text = words.join(" ");
			if (text.indexOf("@") >= 0) {
				if (words.length > 1)
					throw bad("syntax", "white space in an address");
				p = addr_problem(text);
			} else
				p = bare_problem(text);
			if (p !== null)
				throw bad(p, "invalid address");
			addr = text;
			name = comment === null ? "" : comment;
		}
		return { name: A.clean(squeeze(name), A.MAX_NAME), addr: addr };
	}

	// parse_list(string): the recipients typed in one To, CC or BCC field,
	// as [{name, addr}] in order.  Entries are separated by ',' or ';'
	// outside quotes, angle brackets and comments; empty entries (also a
	// trailing separator) are skipped.  An entry is one of:
	//   Name <addr>   "Quoted, \"Name\"" <addr>   <addr>
	//   addr   addr (comment)   (comment) addr     (a comment is the name
	//                                              when there is no other)
	//   a local user's name, alias or number, or a bare FidoNet address
	//   ("1:103/705"): any text without '@', specials or ':' (except in a
	//   FidoNet address); runs of white space become one space
	// addr is returned as typed (case kept).  name is through clean() (64
	// bytes); "" when absent.  Refused (thrown invalid, naming the first
	// bad entry): control characters, invalid UTF-8, unbalanced quotes,
	// angles or parentheses, nested comments, group syntax ("name: ...;"),
	// source routes ("<@host:a@b>"), quoted local parts, domain literals,
	// more than one '@', white space or non-ASCII in an address, an
	// address over 254 bytes (local part 64), a local name over 254 bytes,
	// more than 100 entries, more than 16384 bytes of input.
	A.parse_list = function (string)
	{
		var s = str(string);
		if (s.length > A.MAX_INPUT)
			throw fail("invalid", "toolong", "the list of recipients is too long", -1);
		var raw = split_entries(s);
		var out = [];
		for (var i = 0; i < raw.length; i++) {
			if (trim(raw[i]) === "")
				continue;
			if (out.length >= A.MAX_ENTRIES)
				throw fail("invalid", "toomany", "too many recipients", out.length, raw[i]);
			out.push(parse_entry(raw[i], out.length));
		}
		return out;
	};

	// ---------------------------------------------------------------
	// Recipients

	// resolve(entry, matchuser_fn, netaddr_type_fn [, own]): what kind of
	// recipient an entry from parse_list() is.
	//   matchuser_fn(text)    a local user's number for a name, alias or
	//                         number as the terminal's mail command takes
	//                         it, or 0 (system_resolvers().matchuser)
	//   netaddr_type_fn(addr) the stock netaddr_type() global
	//   own (optional)        { is_own_domain(host), local_user(local_part,
	//                         address), username(usernum) }
	//                         (system_resolvers().own);
	//                         without it, every address with '@' is
	//                         network mail and a local name is returned as
	//                         typed
	// In order:
	//   1. no '@' and matchuser_fn gives a user: {kind:"local", usernum,
	//      name}; name is own.username(usernum) (the name the terminal
	//      writes as the recipient), never the typed display name.  A
	//      number whose own.username() is empty is no user.  All-digit
	//      text that is no user is refused (never a FidoNet node).
	//   2. an Internet address at one of the system's own domains whose
	//      local part own.local_user() maps to a user: that user, as in 1
	//      (so mail between local users is not sent through SMTP); any
	//      other stays Internet mail and the mail server applies its own
	//      rules (alias.cfg, routing, tags, mail processors, unknown user)
	//   3. by netaddr_type_fn(addr), with the forms accepted here:
	//        NET_INTERNET  local@domain, the domain a plain host name (see
	//                      plain_host: no port, path, IP address or
	//                      single label)
	//        NET_FIDO      zone:net/node[.point], or name@ that
	//        NET_QWK       name@QWKID (one id, 1-8 characters, a letter
	//                      first; no route)
	//      {kind:"internet"|"fido"|"qwk", addr, name, net_type}
	// Anything else throws invalid ("unknown": no such local user and not
	// a network address).  The entry's address is checked again here, so a
	// hand-made entry with a control character is refused too.
	A.resolve = function (entry, matchuser_fn, netaddr_type_fn, own)
	{
		if (entry === null || typeof entry !== "object")
			throw fail("invalid", "syntax", "no address", 0, "");
		var addr = str(entry.addr);
		var name = A.clean(squeeze(str(entry.name)), A.MAX_NAME);
		var has_at = addr.indexOf("@") >= 0;
		var p = has_at ? addr_problem(addr) : bare_problem(addr);
		if (p !== null)
			throw fail("invalid", p, "invalid address", 0, addr);

		function unknown()
		{
			return fail("invalid", "unknown", "unknown user or address", 0, addr);
		}

		// A user number from a lookup, or 0; with own.username, a number
		// whose name is empty (no such user, deleted) is no user
		function local(n)
		{
			if (typeof n !== "number" || !(n > 0))
				return null;
			var uname = addr;
			if (own && own.username) {
				uname = A.clean(own.username(n), A.MAX_NAME);
				if (uname === "")
					return null;
			}
			return { kind: "local", usernum: n, name: uname };
		}

		var r;
		if (!has_at) {
			r = local(matchuser_fn(addr));
			if (r !== null)
				return r;
			if (/^[0-9]+$/.test(addr))	// a number that is no user
				throw unknown();
		}
		var type = netaddr_type_fn(addr);
		var at = addr.lastIndexOf("@");
		var host = at >= 0 ? addr.substr(at + 1) : addr;
		var kind = null;
		if (type === A.NET_INTERNET && at >= 0 && plain_host(host)) {
			if (own && own.is_own_domain(host)) {
				r = local(own.local_user(addr.substr(0, at), addr));
				if (r !== null)
					return r;
			}
			kind = "internet";
		} else if (type === A.NET_FIDO && FIDO_RE.test(host))
			kind = "fido";
		else if (type === A.NET_QWK && at >= 0 && QWKID_RE.test(host))
			kind = "qwk";
		if (kind === null)
			throw unknown();
		if (kind === "fido")	// as the save stores it (smb_faddrtoa)
			addr = (at >= 0 ? addr.substr(0, at + 1) : "") + A.fido_canonical(host);
		return { kind: kind, addr: addr, name: name, net_type: type };
	};

	// fido_canonical(text): a FidoNet address zone:net/node[.point] in the
	// form the mail base stores it: numbers without leading zeros, the
	// point only when it is not 0; the text unchanged when it is not one
	A.fido_canonical = function (text)
	{
		var m = String(text).match(/^[ \t]*([0-9]+):([0-9]+)\/([0-9]+)(?:\.([0-9]+))?[ \t]*$/);
		if (!m)
			return String(text);
		var out = parseInt(m[1], 10) + ":" + parseInt(m[2], 10) + "/" + parseInt(m[3], 10);
		var point = m[4] === undefined ? 0 : parseInt(m[4], 10);
		return point ? out + "." + point : out;
	};

	// system_resolvers([env]): the functions resolve() takes, from the
	// system's objects.  env = { system, msg_area, User } (defaults: the
	// globals), for tests.  Returns { matchuser, netaddr_type, own }.
	//
	// Every user number found is checked: its name (system.username) is
	// not empty or "DELETED USER", and its record (new User(n)) can be
	// read and is neither deleted nor inactive.  The terminal refuses an
	// inactive or deleted recipient as "Unknown user" (email.cpp
	// sbbs_t::email), so they are no user here.
	//
	// matchuser(text): the terminal's rule for "E-mail (User name or
	// number)" (exec/load/shell_lib.js send_email(), which calls
	// bbs.finduser(), system.matchuser(), then system.matchuserdata(U_NAME)
	// when the system allows mail to real names):
	//   - all digits: that user number, if it is 1 to system.lastuser
	//     (sbbs_t::finduser, data.cpp).  The terminal takes atoi() of any
	//     text, so "7abc" is user 7 there; here only all-digit text is a
	//     number, and "#7" is not a number in either.
	//   - system.matchuser(text): aliases, comparing letters and digits
	//     only, case-insensitively; deleted users are skipped; "sysop",
	//     "postmaster" and the system's QWK id are user 1 (userdat.c
	//     matchuser_ex/matchusername), if there is a user 1.
	//   - with MM_REALNAME in msg_area.settings: the real name
	//     (system.matchuserdata(U_NAME, text), deleted users skipped).
	//   The terminal's interactive partial match ("Do you mean ...?") has
	//   no equivalent here.
	// own.local_user(local_part, address): a user only for a local part
	// the mail server is certain to deliver to that user, else 0 (the
	// address then stays Internet mail and the mail server applies its own
	// rules: mailsrvr.cpp RCPT TO handling and smtp_resolve_user).  0 when
	// the local part
	//   - is empty, starts with a character that is not a letter or digit
	//     (the server skips those), or contains '!' (QWK routing: the
	//     server cuts there) or '#' (a tag, which the server may block);
	//   - is a sysop alias: "sysop", "postmaster" or the QWK id (whether
	//     the server takes them depends on a mail server option scripts
	//     cannot see);
	//   - or the whole address is changed by alias.cfg (system.alias()),
	//     each checked as the server does before any user lookup.
	// Otherwise, as the server: system.matchuser(local_part, false)
	// (aliases, letters and digits compared); the sysop's name
	// (system.operator, '.' read as space): user 1; with MM_REALNAME, the
	// real name as written and with '.' as space.
	// own.is_own_domain(host): system.findstr(system.inet_domain_list,
	// host) (the system's Internet address and its additional domains).
	// own.username(usernum): system.username(), "" for no usable user.
	A.system_resolvers = function (env)
	{
		env = env || {};
		var sys = env.system || system;
		var ma = env.msg_area || msg_area;
		var U = env.User || User;

		function usable(n)
		{
			if (typeof n !== "number" || !(n > 0))
				return false;
			var u = str(sys.username(n));
			if (u === "" || u === "DELETED USER")
				return false;
			try {
				var rec = new U(n);
				return (rec.settings & (A.USER_DELETED | A.USER_INACTIVE)) == 0;
			} catch (e) {
				return false;
			}
		}

		function checked(n)
		{
			return usable(n) ? n : 0;
		}

		function username(n)
		{
			return usable(n) ? str(sys.username(n)) : "";
		}

		function realnames()
		{
			return (ma.settings & A.MM_REALNAME) != 0;
		}

		function matchuser(text)
		{
			text = squeeze(str(text));
			if (text === "")
				return 0;
			if (/^[0-9]+$/.test(text)) {
				var num = parseInt(text, 10);
				return num <= sys.lastuser ? checked(num) : 0;
			}
			var n = sys.matchuser(text);
			if (!(n > 0) && realnames())
				n = sys.matchuserdata(A.U_NAME, text, false);
			return checked(n);
		}

		function is_sysop_alias(name)
		{
			var k = alnum_key(name);
			return k === "sysop" || k === "postmaster" || k === alnum_key(str(sys.qwk_id));
		}

		function local_user(local_part, address)
		{
			var name = str(local_part);
			if (!/^[A-Za-z0-9]/.test(name) || /[!#]/.test(name) || is_sysop_alias(name))
				return 0;
			if (address !== undefined && str(sys.alias(str(address))) !== str(address))
				return 0;
			if (str(sys.alias(name)) !== name)
				return 0;
			var n = sys.matchuser(name, false);
			if (n > 0)
				return checked(n);
			var spaced = name.replace(/\./g, " ");
			var op = lower(str(sys.operator));
			if (op !== "" && (lower(name) === op || lower(spaced) === op))
				return checked(1);
			if (realnames()) {
				n = sys.matchuserdata(A.U_NAME, name, false);
				if (!(n > 0))
					n = sys.matchuserdata(A.U_NAME, spaced, false);
				return checked(n);
			}
			return 0;
		}

		return {
			matchuser: matchuser,
			netaddr_type: netaddr_type,
			own: {
				is_own_domain: function (host) {
					return !!sys.findstr(sys.inet_domain_list, str(host));
				},
				local_user: local_user,
				username: username
			}
		};
	};

	// ---------------------------------------------------------------
	// Header lines

	// An RFC 2047 "B" encoding of a UTF-8 name: encoded words of at most
	// 75 characters (45 bytes of text each), each holding whole characters
	function encode_words(s)
	{
		var words = [];
		while (s.length > 0) {
			var part = cut(s, 45);
			if (part === "")	// cannot happen for valid UTF-8; be safe
				part = s.substr(0, 1);
			words.push("=?UTF-8?B?" + base64_encode(part) + "?=");
			s = s.substr(part.length);
		}
		return words.join(" ");
	}

	// format(name, addr): one mailbox for a To or CC header line.  name
	// goes through clean() (64 bytes); without one the result is addr.
	// An ASCII name of atext and spaces without "=?" is written as is;
	// any other ASCII name as a quoted string ('"' and '\' escaped), so a
	// name that looks like an encoded word is never decoded by a reader
	// into a different name and address.  A name with any non-ASCII byte
	// is written as RFC 2047 "B" encoded words that this function makes
	// from the name's own bytes (UTF-8); nothing a user types is ever
	// written out as a raw encoded word.  addr must have '@', pass the
	// same checks as parse_list() and have a plain host name for its
	// domain (Internet recipients only), else invalid is thrown.  Used by
	// the send task for the to_list and cc_list header fields, which the
	// mail server writes out unchanged.
	A.format = function (name, addr)
	{
		addr = str(addr);
		var p = addr.indexOf("@") >= 0 ? addr_problem(addr) : "syntax";
		if (p === null && !plain_host(addr.substr(addr.indexOf("@") + 1)))
			p = "syntax";
		if (p !== null)
			throw fail("invalid", p, "invalid address", 0, addr);
		name = A.clean(squeeze(str(name)), A.MAX_NAME);
		if (name === "")
			return addr;
		if (/[\x80-\xFF]/.test(name))
			return encode_words(name) + " <" + addr + ">";
		if (ATEXT_PHRASE_RE.test(name) && name.indexOf("=?") < 0)
			return name + " <" + addr + ">";
		return '"' + name.replace(/[\\"]/g, "\\$&") + '" <' + addr + ">";
	};

	// format_list(entries): format() of each {name, addr}, joined with ", "
	// (one line: the result is never folded)
	A.format_list = function (entries)
	{
		var out = [];
		for (var i = 0; i < entries.length; i++)
			out.push(A.format(entries[i].name, entries[i].addr));
		return out.join(", ");
	};

	// ---------------------------------------------------------------
	// The sender

	// domains(list, can_send_fn): the From domains a user may use.  list is
	// system.inet_domain_list (the system's Internet address first, then
	// the additional domains, which may include patterns and repeats);
	// can_send_fn(domain) is the user's can_send_from_domain.  Returns the
	// entries that are plain host names (letters, digits, '.', '-'; so no
	// pattern) and for which can_send_fn returns true (a throw counts as
	// false), ASCII-lowercased, in order, without duplicates.
	A.domains = function (list, can_send_fn)
	{
		var out = [];
		if (!list || typeof list.length !== "number")
			return out;
		for (var i = 0; i < list.length; i++) {
			if (typeof list[i] !== "string" || !HOSTNAME_RE.test(list[i]))
				continue;
			var d = lower(list[i]);
			if (out.indexOf(d) >= 0)
				continue;
			var ok = false;
			try {
				ok = can_send_fn(list[i]) === true;
			} catch (e) {
				ok = false;
			}
			if (ok)
				out.push(d);
		}
		return out;
	};

	// domains_for(usernum [, env]): domains() for user usernum from the
	// system objects: system.inet_domain_list and
	// new User(usernum).can_send_from_domain.  env = { system, User } for
	// tests.  [] for anything but a positive integer number, a user that
	// cannot be read (new User() throws past the last user), or any other
	// failure; never throws.
	A.domains_for = function (usernum, env)
	{
		try {
			if (typeof usernum !== "number" || usernum < 1 || Math.floor(usernum) !== usernum)
				return [];
			env = env || {};
			var sys = env.system || system;
			var U = env.User || User;
			var u = new U(usernum);
			return A.domains(sys.inet_domain_list, function (d) {
				return u.can_send_from_domain(d);
			});
		} catch (e) {
			return [];
		}
	};

	function in_allowed(domain, allowed)
	{
		if (!allowed || typeof allowed.length !== "number")
			return false;
		for (var i = 0; i < allowed.length; i++)
			if (typeof allowed[i] === "string" && lower(allowed[i]) === domain)
				return true;
		return false;
	}

	// from_for(local_part, domain, allowed): local_part + "@" + domain
	// (lowercased).  local_part is the part before '@' of the address the
	// system assigns the user (user.email), never the raw alias: atext and
	// dots, or a quoted string of printable ASCII without '"' or '\' (as
	// usermailaddr() writes a name with '!'); at most 64 bytes; else
	// invalid.  domain not in allowed (from domains()): forbidden.
	A.from_for = function (local_part, domain, allowed)
	{
		var lp = str(local_part);
		var ok = LOCAL_RE.test(lp) || /^"[\x20\x21\x23-\x5B\x5D-\x7E]+"$/.test(lp);
		if (!ok || lp.length > A.MAX_LOCAL)
			throw fail("invalid", "syntax", "invalid sender address");
		var d = lower(str(domain));
		if (!HOSTNAME_RE.test(d) || !in_allowed(d, allowed))
			throw fail("forbidden", "domain", "sending from this domain is not allowed");
		return lp + "@" + d;
	};

	// reply_domain(to_domain, default_domain, allowed): the From domain for
	// a reply to mail sent to to_domain (a domain, or an address whose
	// domain is used): to_domain lowercased when it is in allowed, else
	// default_domain lowercased (which must be a host name: letters,
	// digits, '.', '-'; else invalid is thrown)
	A.reply_domain = function (to_domain, default_domain, allowed)
	{
		var d = str(to_domain);
		var at = d.lastIndexOf("@");
		if (at >= 0)
			d = d.substr(at + 1);
		d = lower(d);
		var def = lower(str(default_domain));
		if (!HOSTNAME_RE.test(def))
			throw fail("invalid", "syntax", "invalid default domain");
		if (HOSTNAME_RE.test(d) && in_allowed(d, allowed))
			return d;
		return def;
	};

	// Letters and digits only, ASCII-lowercased: the mail server compares
	// local parts to aliases this way (userdat.c matchusername)
	function alnum_key(s)
	{
		return lower(s.replace(/[^A-Za-z0-9]/g, ""));
	}

	// is_me(addr, alias, allowed): true when addr (a bare address) is one
	// the mail server would deliver to the user with this alias: its domain
	// is in allowed and its local part, without a "#tag", matches alias
	// comparing letters and digits only, case-insensitively (as the mail
	// server matches; so "Rob.Swindell" is "Rob Swindell").  Used to leave
	// the user out of a reply-all.
	A.is_me = function (addr, alias, allowed)
	{
		addr = str(addr);
		if (addr_problem(addr) !== null)
			return false;
		var at = addr.indexOf("@");
		if (!in_allowed(lower(addr.substr(at + 1)), allowed))
			return false;
		var local = addr.substr(0, at);
		if (!/^[A-Za-z0-9]/.test(local) || local.indexOf("!") >= 0)
			return false;	// the mail server skips or routes these
		var tag = local.indexOf("#");
		if (tag >= 0)
			local = local.substr(0, tag);
		var key = alnum_key(str(alias));
		return key !== "" && alnum_key(local) === key;
	};

	// The mail server writes a From or To name inside quotes without
	// escaping (userdat.c smtp_netmailaddr, mailsrvr.cpp sockmsgtxt), so
	// '"' and '\' are removed from it, and so are '<' and '>' and the
	// encoded-word markers "=?" and "?=" (repeated until none is left), so
	// no reader can take part of the name for an address or decode it into
	// one.  display_name(s) is this, through clean() (64 bytes).
	function header_name(s)
	{
		var t = str(s).replace(/["\\<>]/g, "");
		var prev;
		do {
			prev = t;
			t = t.replace(/=\?|\?=/g, "");
		} while (t !== prev);
		return A.clean(squeeze(t), A.MAX_NAME);
	}

	A.display_name = header_name;

	// identity(user, kind, settings [, sys]): the sender fields for mail to
	// a recipient of this kind ("local", "internet", "fido", "qwk").
	//   user      a User or the like: number, alias, name,
	//             security.restrictions
	//   settings  the user's web mail settings; display_name is used only
	//             for "internet"
	//   sys       { inet_alias, fido_alias }: whether the Internet and the
	//             FidoNet NetMail "use aliases" settings are on (default:
	//             NMAIL_ALIAS in msg_area.inet_netmail_settings and
	//             msg_area.fido_netmail_settings)
	// from, as the terminal writes it:
	//   internet  the display name if not blank, else the alias when
	//             inet_alias is on or the user has the 'O' restriction
	//             (UREST_REAL_NAME), else the real name (netmail.cpp
	//             inetmail(), the same rule as user.email in js_user.cpp)
	//   fido      the alias when fido_alias is on or the user has 'O', else
	//             the real name (netmail.cpp netmail())
	//   qwk       the alias (netmail.cpp qnetmail())
	//   local     the alias (email.cpp email())
	// A blank real name falls back to the alias (the terminal would send
	// an empty name).  For "internet", '"', '\', '<', '>', "=?" and "?="
	// are removed (see header_name).  from goes through clean() (64 bytes).  from_ext is
	// the user's number as a string.  Another kind throws invalid.
	A.identity = function (user, kind, settings, sys)
	{
		if (!sys)
			sys = {
				inet_alias: (msg_area.inet_netmail_settings & A.NMAIL_ALIAS) != 0,
				fido_alias: (msg_area.fido_netmail_settings & A.NMAIL_ALIAS) != 0
			};
		var rest = (user.security && user.security.restrictions) || 0;
		var alias = str(user.alias);

		function system_name(use_alias)
		{
			if (use_alias || (rest & A.UREST_REAL_NAME) || squeeze(str(user.name)) === "")
				return alias;
			return str(user.name);
		}

		var from;
		if (kind === "internet") {
			from = header_name(settings && settings.display_name);
			if (from === "")
				from = header_name(system_name(sys.inet_alias));
		} else if (kind === "fido")
			from = A.clean(system_name(sys.fido_alias), A.MAX_NAME);
		else if (kind === "qwk" || kind === "local")
			from = A.clean(alias, A.MAX_NAME);
		else
			throw fail("invalid", "kind", "unknown kind of recipient");
		return { from: from, from_ext: String(user.number) };
	};

	return A;
})();

Address;
