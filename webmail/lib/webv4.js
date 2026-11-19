// Webv4: who is signed in to the webv4 web interface, from its cookie.
//
// var Webv4 = load("webv4.js");
// var usernum = Webv4.session_user(http_request.cookie, system.data_dir);
//
// Used only when the webmail option webv4_login is true, to sign a user in
// to the web client without the sign-in form.
//
// This follows webv4's own rule (webv4/lib/auth.js, validateSession and
// getSession) and nothing more:
//   - the cookie is named "synchronet".  The name is fixed in webv4's code
//     (setCookie, validateSession, destroySession), not a setting, so it is
//     fixed here too (Webv4.COOKIE).
//   - its value is "<usernum>,<key>": the user number in plain digits and a
//     key of word characters.  Here the user number must also be 1 to
//     2147483647 with no leading zero (webv4 never issues one) and the key
//     at most 1024 characters (webv4's are 512); both are checked before
//     any path is built from the number.
//   - the key must equal the "key" value stored in
//     data_dir + format("user/%04u.web", usernum), which webv4 reads with
//     File.iniGetObject(); a key that is not a string there (iniGetObject
//     turns an all-digit value into a number) is refused, as webv4 refuses
//     it.  Keys are compared in constant time.
//   - there is NO age test.  webv4 does not expire a session: validateSession
//     accepts a cookie whenever its key matches the stored one; its timeout
//     setting only decides when a new logon is recorded and the CSRF token
//     regenerated, and is the cookie's browser lifetime.  So a webv4
//     session lasts until the user signs out of webv4 (which deletes the
//     .web file) or signs in again elsewhere (which replaces the key).
//     The web client's own session, created from this one, has its own
//     limits (session.js).
// Several values for the cookie are tried in order, as webv4 does; the
// first that matches wins.  The .web file is only read (opened "r"),
// never written.  A missing file, a missing or empty key, or any problem
// reading or parsing the file gives 0.
//
// Whether that user may use the web client (not a guest, not deleted,
// access_requirements) is NOT decided here; api.js applies the same
// refusals as for a password sign-in.
//
// Webv4.read_key(path) reads the stored key (a string) or returns null; a
// test may replace it to see which files are looked up.

"use strict";

var Webv4 = (function () {
	var COOKIE = "synchronet";
	var VALUE_RE = /^([1-9][0-9]{0,9}),([A-Za-z0-9_]{1,1024})$/;
	var MAX_VALUES = 16;
	var MAX_USERNUM = 0x7fffffff;

	// Constant-time comparison: the time depends only on the length of b
	// (the stored key), not on where a and b differ
	function same(a, b)
	{
		var diff = a.length ^ b.length;
		for (var i = 0; i < b.length; i++)
			diff |= (a.charCodeAt(i) | 0) ^ b.charCodeAt(i);
		return diff === 0;
	}

	function read_key(path)
	{
		try {
			if (!file_exists(path))
				return null;
			var f = new File(path);
			if (!f.open("r"))
				return null;
			var obj;
			try {
				obj = f.iniGetObject();
			} finally {
				f.close();
			}
			if (obj === null || typeof obj !== "object" || typeof obj.key !== "string"
				|| obj.key === "")
				return null;
			return obj.key;
		} catch (e) {
			return null;
		}
	}

	function session_user(cookies, data_dir)
	{
		if (cookies === null || typeof cookies !== "object"
			|| !Object.prototype.hasOwnProperty.call(cookies, COOKIE))
			return 0;
		var list = cookies[COOKIE];
		if (typeof list === "string")
			list = [list];
		if (!(list instanceof Array))
			return 0;
		for (var i = 0; i < list.length && i < MAX_VALUES; i++) {
			var v = list[i];
			if (typeof v !== "string")
				continue;
			var m = VALUE_RE.exec(v);
			if (m === null)
				continue;
			var usernum = parseInt(m[1], 10);
			if (!(usernum >= 1 && usernum <= MAX_USERNUM))
				continue;
			var key = Webv4.read_key(data_dir + format("user/%04u.web", usernum));
			if (typeof key === "string" && key !== "" && same(m[2], key))
				return usernum;
		}
		return 0;
	}

	var Webv4 = {
		COOKIE: COOKIE,
		session_user: session_user,
		read_key: read_key
	};

	return Webv4;
})();

Webv4;
