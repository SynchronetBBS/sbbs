// Download: serving one part of a message (part.ssjs), and the rules for
// its response headers.
//
// var Download = load("download.js");
// Download.serve(env, query, reply [, Api]);	// never throws
//
// The request: GET ?id=<message id>&pid=<part id>&dl=<0|1>
//   id   /^[sp][1-9][0-9]{0,9}$/
//   pid  "raw" (the whole message), "f<n>" (a file attachment, n 1 to
//        999999) or a MIME part id: 1 to 20 numbers of 1 to 6 digits
//        joined by "." (as the mailbox library accepts them)
//   dl   "0" or "1"; absent means 1
// Each parameter must appear once; anything else is 400 before the
// session or the mailbox is looked at.
//
// Order of the checks: method not GET (405, Allow: GET); config.require_tls
// and not secure (403); parameters (400); the session cookie, checked as
// api.js does with touch true (401 when not signed in; an invalid cookie
// is cleared); then the mailbox (Store.part_file, or Store.raw_file for
// "raw"): 404 notfound, 403 forbidden, 400 invalid, 500 otherwise.  An
// error reply is a short fixed plain-text body; no server path or
// exception text is ever sent (the detail is logged).
//
// There is no X-Webmail header test here: a download or an image is a
// plain navigation or element request, which cannot carry one.  The
// session cookie is SameSite=Lax, so an image, frame or script element on
// another site's page arrives without it.  A link there that takes the
// user to a download URL does carry it: the file then goes to the user's
// own browser under the headers below, and the other site reads nothing.
// Every parameter is checked strictly.
//
// Response headers (Download.headers):
//   Content-Type             image/png, image/jpeg, image/gif or image/webp
//                            as they are; message/rfc822 for "raw"; any
//                            other type application/octet-stream
//   Content-Length           the size of the file sent
//   Content-Disposition      "inline" only for one of those four image types
//                            with dl=0; otherwise "attachment" (always for
//                            "raw" and every other type).  Both carry the
//                            file name (Download.disposition)
//   X-Content-Type-Options   nosniff
//   Content-Security-Policy  sandbox
//   Cross-Origin-Resource-Policy  same-origin
//   Cache-Control            no-store
// The file's size is read once; that value is Content-Length and the
// number of bytes sent, however the file changes meanwhile.  If it reads
// short, the reply ends early and that is logged.
//
// File names (Download.disposition): control characters (C0, DEL and the
// UTF-8 encoded C1 range), the Unicode format characters U+200B-U+200F,
// U+202A-U+202E, U+2066-U+2069 and U+FEFF (as UTF-8), double quotes,
// slashes and backslashes are removed, surrounding spaces trimmed, and
// the result cut to 200 bytes at a UTF-8 character boundary; an empty
// result is "download".  The name is
// sent twice: filename="..." with each run of non-ASCII bytes and each "%"
// replaced by "_" (for old clients), and filename*=UTF-8''... (RFC 5987:
// every byte outside ALPHA / DIGIT / !#$&+-.^_`|~ percent-encoded).
//
// The file is sent in 64 KB reads.  A temp file (temp true from the
// mailbox library) is deleted in a finally, also when writing fails (the
// client went away) or anything throws; reply.track(path) is called first
// so the shim can also delete it from js.on_exit() should the web server
// end the script without unwinding it.
//
// Api: the api.js library the caller already loaded (default: loaded from
// beside this file on first use).
// env: { method, config, secure, cookie, now (optional), userdata(usernum),
//   get_user(usernum), make_ctx(usernum), set_cookie(value, keep) } as for
//   Api.dispatch.
// reply: { status(text), header(name, value), write(bytes), track(path) }.

"use strict";

var Download = (function () {
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

	var Api = null;	// api.js: the caller's, or loaded on first use

	var CHUNK = 65536;
	var MAX_NAME = 200;
	var ID_RE = /^[sp][1-9][0-9]{0,9}$/;
	var PID_RE = /^(raw|f[1-9][0-9]{0,5}|[0-9]{1,6}(\.[0-9]{1,6}){0,19})$/;
	var INLINE_TYPES = ["image/png", "image/jpeg", "image/gif", "image/webp"];
	// Unicode format characters as UTF-8: U+200B-U+200F, U+202A-U+202E,
	// U+2066-U+2069 and U+FEFF (bidirectional overrides and the like, which
	// can make a name read differently from what it is)
	var FORMAT_RE = /\xE2\x80[\x8B-\x8F\xAA-\xAE]|\xE2\x81[\xA6-\xA9]|\xEF\xBB\xBF/g;
	var STATUS = {
		invalid: "400 Bad Request",
		auth: "401 Unauthorized",
		forbidden: "403 Forbidden",
		https: "403 Forbidden",
		notfound: "404 Not Found",
		method: "405 Method Not Allowed",
		internal: "500 Internal Server Error"
	};
	var TEXT = {
		invalid: "Bad request",
		auth: "Not signed in",
		forbidden: "Forbidden",
		https: "A secure (https) connection is required",
		notfound: "Not found",
		method: "Method not allowed",
		internal: "Server error"
	};

	// One parameter: a string, or an array of exactly one string
	function one(query, name)
	{
		if (query === null || typeof query !== "object"
			|| !Object.prototype.hasOwnProperty.call(query, name))
			return undefined;
		var v = query[name];
		if (v instanceof Array) {
			if (v.length !== 1)
				return null;
			v = v[0];
		}
		return typeof v === "string" ? v : null;
	}

	// {id, pid, dl (boolean)} or null
	function params(query)
	{
		var id = one(query, "id");
		var pid = one(query, "pid");
		var dl = one(query, "dl");
		if (typeof id !== "string" || !ID_RE.test(id))
			return null;
		if (typeof pid !== "string" || !PID_RE.test(pid))
			return null;
		if (dl === undefined)
			dl = "1";
		if (dl !== "0" && dl !== "1")
			return null;
		return { id: id, pid: pid, dl: dl === "1" };
	}

	// s cut to at most max bytes without splitting a UTF-8 sequence
	function cut(s, max)
	{
		if (s.length <= max)
			return s;
		s = s.substr(0, max);
		var i = s.length - 1;
		var n = 0;
		while (i >= 0 && n < 3 && (s.charCodeAt(i) & 0xC0) === 0x80) {
			i--;
			n++;
		}
		if (i >= 0) {
			var c = s.charCodeAt(i);
			var need = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
			if (need > n)
				s = s.substr(0, i);
		}
		return s;
	}

	function clean_name(name)
	{
		var s = typeof name === "string" ? name : "";
		// Repeated until nothing changes: removing one sequence can join the
		// bytes around it into another (E2 80 [E2 80 8B] AE is E2 80 AE)
		var prev;
		do {
			prev = s;
			s = s.replace(/[\x00-\x1f\x7f"\/\\]/g, "").replace(/\xC2[\x80-\x9F]/g, "");
			s = s.replace(FORMAT_RE, "");
		} while (s !== prev);
		s = s.replace(/^ +| +$/g, "");
		return cut(s, MAX_NAME).replace(/ +$/, "");
	}

	function hex2(c)
	{
		return (c < 16 ? "0" : "") + c.toString(16).toUpperCase();
	}

	function rfc5987(s)
	{
		var out = "";
		for (var i = 0; i < s.length; i++) {
			var ch = s.charAt(i);
			if (/[A-Za-z0-9!#$&+\-.\^_`|~]/.test(ch))
				out += ch;
			else
				out += "%" + hex2(s.charCodeAt(i) & 0xFF);
		}
		return out;
	}

	function disposition(name, inline)
	{
		var s = clean_name(name);
		if (s === "")
			s = "download";
		var ascii = s.replace(/[\x80-\xff]+/g, "_").replace(/%/g, "_");
		return (inline ? "inline" : "attachment") + "; filename=\"" + ascii + "\"; filename*=UTF-8''"
			+ rfc5987(s);
	}

	// info {name, type}, size (bytes), dl (boolean: download), raw (boolean)
	function headers(info, size, dl, raw)
	{
		var type;
		var inline = false;
		if (raw)
			type = "message/rfc822";
		else if (INLINE_TYPES.indexOf(info.type) >= 0) {
			type = info.type;
			inline = !dl;
		} else
			type = "application/octet-stream";
		return {
			"Content-Type": type,
			"Content-Length": String(size),
			"Content-Disposition": disposition(info.name, inline),
			"X-Content-Type-Options": "nosniff",
			"Content-Security-Policy": "sandbox",
			"Cross-Origin-Resource-Policy": "same-origin",
			"Cache-Control": "no-store"
		};
	}

	function describe(e)
	{
		try {
			var text = String(e);
			if (e !== null && typeof e === "object") {
				if (e.code !== undefined && !(e instanceof Error))
					text = "code " + e.code + ": " + e.message;
				if (e.fileName)
					text += " (" + e.fileName + ":" + e.lineNumber + ")";
			}
			return text;
		} catch (x) {
			return "(unprintable exception)";
		}
	}

	function plain(reply, what, extra)
	{
		reply.status(STATUS[what]);
		reply.header("Content-Type", "text/plain; charset=utf-8");
		reply.header("X-Content-Type-Options", "nosniff");
		reply.header("Cache-Control", "no-store");
		if (extra)
			for (var k in extra)
				reply.header(k, extra[k]);
		reply.write(TEXT[what]);
	}

	function code_of(e)
	{
		if (e !== null && typeof e === "object" && !(e instanceof Error) && typeof e.code === "string"
			&& ["invalid", "auth", "forbidden", "notfound"].indexOf(e.code) >= 0)
			return e.code;
		return "internal";
	}

	function serve(env, query, reply, given)
	{
		var ctx = null;
		var info = null;
		var file = null;
		var started = false;
		try {
			if ((given === undefined || given === null) && Api === null)
				Api = sibling("api.js");
			var A = given !== undefined && given !== null ? given : Api;
			if (env.method !== "GET") {
				plain(reply, "method", { "Allow": "GET" });
				return;
			}
			if (env.secure !== true && (!env.config || env.config.require_tls !== false)) {
				plain(reply, "https");
				return;
			}
			var p = params(query);
			if (p === null) {
				plain(reply, "invalid");
				return;
			}
			var s = A.session_user(env, true);
			if (s.clear && typeof env.set_cookie === "function")
				env.set_cookie("", false);
			if (s.usernum === 0) {
				plain(reply, "auth");
				return;
			}
			ctx = env.make_ctx(s.usernum);
			var raw = p.pid === "raw";
			info = raw ? ctx.store.raw_file(p.id) : ctx.store.part_file(p.id, p.pid);
			if (info.temp && typeof reply.track === "function")
				reply.track(info.path);
			file = new File(info.path);
			if (!file.open("rb"))
				throw new Error("cannot open " + info.path);
			var length = file.length;	// once: Content-Length and the copy
			var h = headers(info, length, p.dl, raw);
			reply.status("200 OK");
			for (var k in h)
				reply.header(k, h[k]);
			started = true;
			var left = length;
			while (left > 0) {
				var chunk = Download.read(file, Math.min(CHUNK, left));
				if (typeof chunk !== "string" || chunk.length === 0) {
					// Content-Length is already sent: the reply ends short
					Download.log(LOG_ERR, "webmail part: short read of " + info.path + ": "
						+ (length - left) + " of " + length + " bytes");
					break;
				}
				if (chunk.length > left)
					chunk = chunk.substr(0, left);	// never past Content-Length
				reply.write(chunk);
				left -= chunk.length;
			}
		} catch (e) {
			var code = code_of(e);
			Download.log(code === "internal" ? LOG_ERR : LOG_DEBUG, "webmail part: "
				+ (started ? "while sending: " : "") + describe(e));
			if (!started) {
				try {
					plain(reply, code);
				} catch (x) {
					Download.log(LOG_DEBUG, "webmail part: error reply: " + describe(x));
				}
			}
		} finally {
			if (file !== null && file.is_open)
				file.close();
			if (info !== null && info.temp === true)
				file_remove(info.path);
			if (ctx !== null) {
				try {
					ctx.close();
				} catch (e) {
					Download.log(LOG_WARNING, "webmail part: closing the mailbox: " + describe(e));
				}
			}
		}
	}

	var Download = {
		CHUNK: CHUNK,
		params: params,
		headers: headers,
		disposition: disposition,
		serve: serve,
		// One read of an open File (a test may replace it)
		read: function (file, n) {
			return file.read(n);
		},
		log: function (level, text) {
			log(level, text);
		}
	};

	return Download;
})();

Download;
