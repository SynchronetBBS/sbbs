// Webv4: who is signed in to the webv4 web interface, from its cookie.
//
// var Webv4 = load("webv4.js");
// var usernum = Webv4.session_user(http_request.cookie, system.data_dir);
//
// var key = Webv4.stored_key(usernum, system.data_dir);
// var ended = Webv4.end_session(http_request.cookie, system.data_dir, usernum);
//
// session_user is used only when the webmail option webv4_login is true,
// to sign a user in to the web client without the sign-in form.
// stored_key is the key of the user's webv4 session (or null): a web
// client session started that way lasts only while it is unchanged, so
// signing out of webv4 (or signing in to webv4 again elsewhere) signs the
// user out of the web client too.
// end_session is used only when the webmail option webv4_logout is true:
// signing out of the web client then signs the browser out of webv4 too,
// when its webv4 cookie is a valid session of the same user.
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
//     limits (session.js), and ends when this one does (api.js).
// Several values for the cookie are tried in order, as webv4 does; the
// first that matches wins.  The .web file is only read (opened "r"),
// never written, except that end_session removes it.  A missing file, a missing or empty key, or any problem
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

	// The webv4 session key stored for usernum, or null (no session, or
	// one webv4 would refuse); read through Webv4.read_key
	function stored_key(usernum, data_dir)
	{
		if (typeof usernum !== "number" || !(usernum >= 1 && usernum <= MAX_USERNUM)
			|| Math.floor(usernum) !== usernum)
			return null;
		return Webv4.read_key(data_dir + format("user/%04u.web", usernum));
	}

	// End the webv4 session of usernum that the browser's cookie belongs
	// to, as webv4's own sign-out does (auth.js, destroySession): its .web
	// file is removed.  Only a cookie that is a valid session of that user
	// counts; returns true when a session was ended.  Clearing the browser's
	// cookie is the caller's part (it has the reply).
	function end_session(cookies, data_dir, usernum)
	{
		if (typeof usernum !== "number" || session_user(cookies, data_dir) !== usernum)
			return false;
		return Webv4.remove_file(data_dir + format("user/%04u.web", usernum));
	}

	function remove_file(path)
	{
		return file_remove(path);
	}

	var Webv4 = {
		COOKIE: COOKIE,
		session_user: session_user,
		stored_key: stored_key,
		end_session: end_session,
		read_key: read_key,
		remove_file: remove_file
	};

	return Webv4;
})();

Webv4;
