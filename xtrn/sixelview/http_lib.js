/* http_lib.js
 *
 * HTTP for SixelView's mini browser (web libraries): URL handling, safe encoding of form data, and a
 * request function that does GET and POST under tight limits.  Kept apart from the rest so that it can
 * grow (other methods, cookies ...) without touching the viewer.
 *
 * The rules this file enforces, whoever calls it:
 *	- only http and https URLs, with no control characters or user names and passwords, and spaces and
 *	  non-ASCII characters percent-encoded, so nothing typed or found in a page can add a header or a second
 *	  request;
 *	- every address, including each redirect, must pass the caller's allowed(url) test;
 *	- only GET and POST; a POST that is redirected is carried on as a GET with no body (as browsers do for
 *	  301, 302 and 303) and 307/308 are refused, so form data is never re-sent to a place the user didn't see;
 *	- limits on the size of the answer and on the time taken;
 *	- no shell, no files named from the data: everything goes through Synchronet's own sockets;
 *	- the BBS's name (the "Via" header Synchronet's HTTPRequest would add) is not sent.
 *
 * Tab size is 4.
 */

var HTTP_MAX_URL = 2048;

/* Resolve ref against base (both absolute URLs or, for local files, plain paths) */
function html_url_resolve(base, ref)
{
	ref = String(ref).replace(/^\s+|\s+$/g, "");
	if (/^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(ref))
		return ref;
	var m = /^([a-zA-Z][a-zA-Z0-9+.-]*:)?(\/\/[^\/?#]*)?([^?#]*)(\?[^#]*)?(#.*)?$/.exec(base);
	if (!m)
		return ref;
	var scheme = m[1] || "", auth = m[2] || "", path = m[3] || "";
	if (ref.substr(0, 2) == "//")
		return scheme + ref;
	var rm = /^([^?#]*)(\?[^#]*)?(#.*)?$/.exec(ref);
	var rpath = rm[1], rq = rm[2] || "", rf = rm[3] || "";
	if (!rpath.length)
		return scheme + auth + path + (rm[2] !== undefined ? rq : (m[4] || "")) + rf;
	if (rpath.charAt(0) != "/")
		rpath = path.replace(/[^\/]*$/, "") + rpath;
	var segs = rpath.split("/"), out = [];
	for (var i = 0; i < segs.length; i++) {
		if (segs[i] == "..")
			out.length > 1 && out.pop();
		else if (segs[i] != ".")
			out.push(segs[i]);
	}
	var joined = out.join("/");
	if (joined.charAt(0) != "/" && auth)
		joined = "/" + joined;
	return scheme + auth + joined + rq + rf;
}

/* A URL this library will talk to: http or https, a plain host name or address (optionally with a port).  Spaces
 * and non-ASCII characters are percent-encoded; anything with a control character is refused.  Returns it without
 * any #fragment, or null. */
function http_clean_url(url)
{
	url = String(url).replace(/#.*$/, "");
	if (/[\x00-\x1f\x7f]/.test(url))
		return null;
	url = url.replace(/[\x20\x80-\xff]/g, function (c) { return "%" + ("0" + c.charCodeAt(0).toString(16).toUpperCase()).slice(-2); });
	if (url.length > HTTP_MAX_URL || /[^\x21-\x7e]/.test(url))
		return null;
	if (!/^https?:\/\/[A-Za-z0-9][A-Za-z0-9.\-]*(:[0-9]{1,5})?([\/?].*)?$/i.test(url))
		return null;
	return url;
}

/* Percent-encode a string of bytes for a form (spaces become +) */
function http_encode(s)
{
	var out = [];
	s = String(s);
	for (var i = 0; i < s.length; i++) {
		var c = s.charCodeAt(i) & 0xff;
		if ((c >= 0x30 && c <= 0x39) || (c >= 0x41 && c <= 0x5a) || (c >= 0x61 && c <= 0x7a) || c == 0x2d || c == 0x5f || c == 0x2e || c == 0x7e)
			out.push(String.fromCharCode(c));
		else if (c == 0x20)
			out.push("+");
		else
			out.push("%" + (c < 16 ? "0" : "") + c.toString(16).toUpperCase());
	}
	return out.join("");
}

/* application/x-www-form-urlencoded body from [[name, value], ...] (byte strings) */
function http_form_body(pairs)
{
	var parts = [];
	for (var i = 0; i < pairs.length; i++)
		parts.push(http_encode(pairs[i][0]) + "=" + http_encode(pairs[i][1]));
	return parts.join("&");
}

/* The address for a GET form: action without its own query or fragment, plus the form data */
function http_get_url(action, body)
{
	return action.replace(/[?#].*$/, "") + (body.length ? "?" + body : "");
}

/* A cached answer: "<final url>\n<body>" in file; null if there isn't one younger than ttl seconds */
function http_cache_read(file, ttl)
{
	if (!file_exists(file) || time() - file_date(file) >= ttl)
		return null;
	var f = new File(file);
	if (!f.open("rb"))
		return null;
	var s = f.read();
	f.close();
	var nl = s.indexOf("\n");
	return nl < 0 ? null : { url: s.substr(0, nl), body: s.substr(nl + 1) };
}

function http_cache_write(file, url, body)
{
	var f = new File(file);
	if (f.open("wb")) {
		f.write(url + "\n" + body);
		f.close();
	}
}

/* Make a request.
 *	o.method	"GET" (default) or "POST"
 *	o.url		the address
 *	o.body		the data for a POST (already encoded; see http_form_body)
 *	o.ctype		its content type (default application/x-www-form-urlencoded)
 *	o.allowed	function(url): true if that address may be used (checked for every redirect too)
 *	o.maxbytes	largest answer (default 1 MB)
 *	o.timeout	seconds for the whole thing (default 45)
 *	o.types		regular expression the answer's content type must match, if given
 * Returns { code, url (where it ended up), ctype, body } for a 200 answer, or { error: "reason" }. */
function http_request(o)
{
	var method = o.method || "GET";
	if (method != "GET" && method != "POST")
		return { error: "Unsupported request" };
	var maxbytes = o.maxbytes || 1048576, deadline = system.timer + (o.timeout || 45);
	var cur = http_clean_url(o.url), body = o.body, ctype = o.ctype || "application/x-www-form-urlencoded";
	if (method == "POST" && (typeof body != "string" || body.length > 65536 || /[^\x21-\x7e]/.test(body)))
		return { error: "Bad form data" };
	if (ctype.length > 100 || /[^\x20-\x7e]/.test(ctype))
		return { error: "Bad request" };
	load("http.js");
	for (var hop = 0; hop < 6; hop++) {
		if (!cur)
			return { error: "That address can't be used" };
		if (o.allowed && !o.allowed(cur))
			return { error: "That address isn't allowed in this library" };
		var req = new HTTPRequest(undefined, undefined, undefined, 20);
		req.follow_redirects = 0;
		try {
			if (method == "POST")
				req.SetupPost(cur, undefined, undefined, body, ctype);
			else
				req.SetupGet(cur);
			req.request_headers = req.request_headers.filter(function (h) { return !/^Via:/i.test(h); });
			req.SendRequest();
			req.ReadStatus();
			req.ReadHeaders();
			var code = req.response_code;
			var hp = req.response_headers_parsed || {};
			if (code >= 300 && code < 400) {
				req.sock.close();
				if (code == 307 || code == 308)
					return { error: "The site wants the form sent again somewhere else" };
				var loc = hp.location;
				if (!loc || !loc.length)
					return { error: "Bad redirect" };
				cur = http_clean_url(html_url_resolve(cur, loc[0]));
				method = "GET";
				body = undefined;
				continue;
			}
			if (code != 200) {
				req.sock.close();
				return { error: "The site answered " + code };
			}
			var ct = hp["content-type"] && hp["content-type"].length ? hp["content-type"][0] : "";
			if (o.types && !o.types.test(ct)) {
				req.sock.close();
				return { error: "The site sent something that can't be shown" };
			}
			if (req.contentlength && req.contentlength > maxbytes) {
				req.sock.close();
				return { error: "The answer is too big" };
			}
			var parts = [], total = 0, ch;
			while ((ch = req.sock.recv(16384, 15)) !== null && ch !== "" && ch !== undefined) {
				parts.push(String(ch));
				total += ch.length;
				if (total > maxbytes || system.timer > deadline) {
					req.sock.close();
					return { error: total > maxbytes ? "The answer is too big" : "The site took too long" };
				}
			}
			req.sock.close();
			return { code: 200, url: cur, ctype: ct, body: parts.join("") };
		}
		catch (e) {
			/* the address can carry what the user searched for, so it is not logged */
			log(LOG_WARNING, "http_lib: " + String(cur).replace(/[?#].*$/, "") + ": " + String(e).replace(/[?#]\S*/g, ""));
			try { req.sock.close(); } catch (e2) {}
			return { error: "Unable to reach the site" };
		}
	}
	return { error: "Too many redirects" };
}
