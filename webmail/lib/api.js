// Api: the web client's batched JSON calls, the trust boundary between a
// browser and a user's mail.
//
// var Api = load("api.js");
// var request = Api.parse_body(http_request.post_data);	// or null
// var reply = Api.dispatch(env, request);	// never throws
//
// Request text: Api.parse_body(bytes) is the one way a body becomes a
// request.  The body must be valid UTF-8 (no overlong forms, no encoded
// surrogates, nothing above U+10FFFF); it is decoded, parsed as JSON, and
// every string in the result, keys included, is encoded back to UTF-8
// bytes (surrogate pairs combined), so raw UTF-8 and \u escapes give the
// same byte strings.  A lone surrogate, nesting deeper than 32, invalid
// UTF-8 or invalid JSON give null, which dispatch answers with the
// top-level "invalid".
//
// request: { calls: [[method, args, call_id], ...], background }
//   at most 32 calls; each call an array of 1 to 3 elements.  args is an
//   object of named arguments (absent or null: none).  call_id is echoed
//   in the result when it is a number or a string of at most 40
//   characters, each 0x20 to 0x7E; else null.  background: true marks a
//   request the client made on its own, not for the user (see Session
//   below); it can only withhold refreshing the session, any other value
//   is ignored.
//
// reply:
//   { results: [[call_id, result], ...], state, signed_out }
//     one result per call, in order: {ok: true, ...method's fields} or
//     {ok: false, error: <code>, message: <text>}.  state is the mailbox's
//     change fingerprint (Store.state()) when a signed-in user's mailbox
//     was opened for this request, else "".  signed_out is true when the
//     request carried a session cookie but no value of it is a valid
//     session (expired, signed out elsewhere, password changed): the
//     calls needing a user still get "auth", and the client can say "you
//     were signed out" rather than show the wrong-password text.  It is
//     false when the request ends signed in (a session.login or webv4
//     sign-in in the same request).
//   { error: "invalid", message, results: [], state: "" }
//     the top-level error, when request is not an object with a "calls"
//     array of at most 32 arrays of 1 to 3 elements; no call runs.  Also
//     { error: "internal", ... } if the dispatcher itself fails.
//
// Error codes: notfound, forbidden, invalid, quota, toolarge, conflict,
// failed, expired, auth, https (the mailbox library's codes and this
// file's) and internal.  message is the text for the code from the
// request's locale strings ("errors.<code>"; in the "https" text {name} is
// the request's product name), or a built-in English text
// if the locale files cannot be read.  A thrown object whose code is one of
// the known codes passes through with that code; anything else is
// "internal", logged with its file name and line.  No exception text,
// server path or stack ever reaches the reply.
//
// Gates, in this order; each one gives its error to EVERY call of the
// request and stops anything later (no method runs, no file is read):
//   1. a malformed request             top-level "invalid"
//   2. env.has_header is not true      "forbidden" (the request lacks the
//                                      X-Webmail: 1 header a cross-site
//                                      form cannot send)
//   3. env.secure is not true while config.require_tls
//                                      "https"
//   then the session cookie is checked (see Session below); if that throws,
//   every call is "internal".  Then for each call:
//   4. unknown method                  "invalid"
//   5. the method needs a signed-in user and there is none
//                                      "auth"
//   6. an argument check fails         "invalid"
//   7. the method runs.
//
// env, supplied by the caller (api.ssjs, or a test):
//   config         Config.load()
//   has_header     boolean: the X-Webmail: 1 request header was present
//   secure         boolean: Session.is_secure(...)
//   cookie         the "webmail" cookie values (an array; a string is one
//                  value), or null/undefined
//   accept_language  the Accept-Language request header, or ""
//   client         { ip, agent } recorded with a new session
//   webv4_user()   returns the user signed in to webv4
//                  (Webv4.session_user()) or 0; called at most once per
//                  request, only when config.webv4_login is true and
//                  only by session.get without a session and by
//                  session.logout, so a poll never reads the .web file
//   now            optional time_t (tests); time() otherwise
//   find_user(name)  the user number the server's login() would look the
//                  name up as, or 0 (system.find_login_id(name), which
//                  calls the same lookup login() does)
//   login(user, password)
//                  the password check: the user number, or 0.  The shim
//                  wraps the web server's login() so failed attempts are
//                  counted by the server.
//   get_user(usernum)  the User object, or null
//   userdata(usernum)  the UserData of that user's web client directory
//   make_ctx(usernum)
//                  { usernum, alias, name, user, store (an opened Store),
//                    userdata, config, close() }.  Called at most once per
//                  dispatch, on the first call that needs the mailbox;
//                  close() is called once, at the end, in a finally.
//                  Api.context_factory() builds one.
//   set_cookie(value, keep)
//                  called to set the session cookie (value "" clears it);
//                  the last call of a dispatch is the one that counts
//
// Session: before the calls run, each cookie value (at most 8) is
// checked in turn with Session.parse -> env.userdata ->
// Session.check(ud, value, now, config, {touch, stamp}) -> the user record
// (Api.eligible); the first that passes is used.  stamp is a function
// Session calls only after the key matched a stored session; it loads the
// user record (env.get_user) and returns the digest of the user's
// password and password date (stamp_of), stored with every session made
// here, so changing the password in any way ends every web client
// session of that user, and a forged value never reads a user record.
// touch is false when every call of the request is "changes" (the
// background poll) or the request carries background: true (a refresh the
// client made on its own), so polling and what follows from it do not
// keep a session alive.  When values were sent but none passes, the
// cookie is cleared (set_cookie("")) and signed_out is set; a session
// whose user is no longer eligible is also destroyed.
// Api.session_user(env, touch) does the same for part.ssjs.
//
// Who may sign in (Api.eligible), for every sign-in path (password, webv4,
// and every later request): the user record exists, its number is at most
// 65535 (the mail index's limit), it is not deleted or inactive, is not a
// guest (User.is_guest, or the G restriction, which is how Synchronet
// marks a guest account: user_is_guest() in userdat.c), and meets
// config.access_requirements (User.compare_ars) when it is not empty.  A
// password sign-in also needs a stored password (the web server's login()
// accepts any password for an account without one).
// Before login() is called, the name is resolved as login() would
// resolve it (env.find_user); an account found that way and refused here
// never reaches login(), whose success has side effects (a logon recorded,
// the user record updated).  A name that does not resolve goes to login()
// and the account it signs in is checked afterwards.  Each refusal of a
// known account logs one notice "webmail: user #N refused: <reason>"; the
// reply is the same "auth" text in every case.
//
// Methods (Api.methods, a plain object that more methods are added to):
// each entry is { auth, ctx, args, run }.  auth: needs a signed-in user.  ctx
// (default: auth): needs the mailbox, so make_ctx is called first.  args:
// { <name>: { type: "string"|"int"|"boolean", required, max, min, test } };
// arguments not listed are invalid, "__proto__" included; strings are
// byte strings (no character above 0xFF) of at most max characters
// (default 1000); ints are finite whole numbers from min (default 0) to
// max; test(value) may refuse
// a string.  run(ctx, args, rq): ctx is the make_ctx() context or null;
// rq is this request's state ({ env, config, usernum, user, ... }); it
// returns an object whose fields go into the result beside ok: true.
//
//   session.get {no_webv4}
//                    -> { user ({number, alias, name, avatar} or null; alias
//                       and name as UTF-8, converted from CP437 when they
//                       are neither ASCII nor valid UTF-8; avatar see
//                       Avatars below), insecure,
//                       locale, strings (the locale strings, in the first
//                       session.get result of a request only), domains,
//                       limits {max_attachment_bytes, max_text_part_bytes,
//                       max_drafts}, poll_ms, powered_by (the system's
//                       version notice, "Synchronet BBS for Linux Version
//                       3.22": Api.powered_by(system.version_notice), white
//                       space runs made one space), product_name, brand,
//                       login_text, footer_links, fallback_url
//                       [, via: "webv4"] }
//                       With no web client session, config.webv4_login
//                       and env.webv4_user() an eligible user, it first
//                       signs that user in (a new session, keep false) and
//                       reports via "webv4".  It does not after a
//                       session.logout earlier in the same request, nor
//                       when no_webv4 is true (the browser signed out and
//                       asks not to be signed in again this way).
//                       brand, login_text and footer_links are sysop-
//                       supplied MARKUP (HTML); product_name and
//                       fallback_url are plain text; the client must treat
//                       them so.  No password, session key or setting with
//                       security weight (access_requirements, directory,
//                       tls_proxy_header, ...) is included.
//   session.login {user, password, keep}
//                    -> { user }.  user at most 64 characters and password
//                       at most 128, neither empty, else "auth" without
//                       asking login(); every refusal is "auth" with the
//                       same text.  A valid session the browser already
//                       had is destroyed.  Later calls of the request run
//                       as the new user.
//   session.logout   -> { webv4 }: true when the user is still signed in
//                       to webv4 (webv4_login on and env.webv4_user() is
//                       this user), so the next visit signs in again
//   folder.list      -> { folders, usage, purged, trash_days }:
//                       purge_expired() (at most once a day; purged is the
//                       number of messages it removed, 0 when nothing ran,
//                       nothing was due or it failed), then folders() and
//                       usage(), one Store; trash_days is the number of
//                       days a message stays in Trash (the Store's option,
//                       system.max_trash_age by default; 0: until removed
//                       by hand).  The spam age limit is not reported:
//                       scripts cannot read it.
//   message.list {folder, offset, limit, search}
//                    -> { total, items } (Store.list)
//   message.get {id, mark_read, head}
//                    -> { message } (Store.get, plus avatar: the sender's
//                       avatar, see Avatars below); with head true
//                       { item }: only the list item (Store.get with
//                       head), for marking a message read that the client
//                       already holds: no bodies, parts or avatar
//   changes {state}  -> { changed, state }: state is Store.state() now
//
// Avatars: the base64 data of a Synchronet avatar (10 x 6 character
// cells, two bytes each: a CP437 character and a PC text attribute),
// from exec/load/avatar_lib.js, or null.  The user's own: read(number,
// alias).  A sender's: as the terminal's message header picks it (see
// sender_avatar()).  The library's own rules apply: its "avatars"
// options (enabled, auto_identicon: a generated identicon when there is
// none), a user's disabled flag, and is_valid().  env.avatar_lib, when
// present, is the library to use (null: none); otherwise it is loaded
// once per request.  Any failure gives null, never a failed call.
//
// Argument patterns: message ids /^[sp][1-9][0-9]{0,9}$/ (as Store checks
// them); folder ids one of inbox, starred, sent, spam, trash, archive,
// all, or "u:" and a tag of 1 to 200 bytes with no white space (space, tab,
// CR, LF, FF, VT) that does not start with "$" (any tag Store.folders()
// can list, foreign ones included).
//
// Sending domains: Api.sending_domains() is the ONE place the list of
// domains a user may send from comes from.  For now it is the system's
// Internet address alone, until the sysop can configure a list.

"use strict";

var Api = (function () {
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

	var Session = sibling("session.js");
	var Locale = sibling("locale.js");
	var UD = {};
	load(UD, "userdefs.js");
	var MAX_COOKIES = 8;		// cookie values tried per request
	var MAX_DEPTH = 32;		// nesting of a request body

	var MAX_CALLS = 32;
	var MAX_CALL_ID = 40;
	var MAX_STRING = 1000;
	var MAX_CREDENTIAL = 262144;	// the request body limit; longer is "auth" below
	var MAX_USER = 64;
	var MAX_PASSWORD = 128;
	var MAX_TAG = 200;
	var MSG_ID_RE = /^[sp][1-9][0-9]{0,9}$/;
	var METHOD_RE = /^[a-z][a-z0-9_]{0,31}(\.[a-z][a-z0-9_]{0,31}){0,3}$/;
	var WS_RE = /[ \t\r\n\f\v]/;
	// Arrays, not objects: this engine's hasOwnProperty() is true for
	// "__proto__" on every object
	var FOLDERS = ["inbox", "starred", "sent", "spam", "trash", "archive", "all"];
	var CODES = ["notfound", "forbidden", "invalid", "quota", "toolarge", "conflict", "failed",
		"expired", "auth", "https"];
	// Only when the locale files cannot be read (the same as en_us.ini)
	var FALLBACK = {
		notfound: "That item could not be found. It may have been deleted.",
		forbidden: "You are not allowed to do that.",
		invalid: "Some of the information is not valid. Please check it and try again.",
		quota: "Your mailbox is full. Delete some messages to make room.",
		toolarge: "That is too large to send or save.",
		conflict: "Someone or something else changed this first. Please reload and try again.",
		failed: "That did not work. Please try again.",
		expired: "Your session has ended. Please sign in again.",
		auth: "The user name or password is not correct.",
		https: "{name} needs a secure connection. Please use the https address of this site.",
		internal: "Something went wrong on the server. Please try again in a moment."
	};
	var BRAND_KEYS = ["product_name", "brand", "login_text", "footer_links", "fallback_url"];

	function own(obj, key)
	{
		return Object.prototype.hasOwnProperty.call(obj, key);
	}

	function ascii_lower(s)
	{
		return s.replace(/[A-Z]+/g, function (m) {
			return m.toLowerCase();
		});
	}

	function now_of(env)
	{
		return typeof env.now === "number" && isFinite(env.now) ? env.now : time();
	}

	function describe(e)
	{
		var text;
		try {
			text = String(e);
			if (e !== null && typeof e === "object") {
				if (e.code !== undefined && !(e instanceof Error))
					text = "code " + e.code + ": " + e.message;
				if (e.fileName)
					text += " (" + e.fileName + ":" + e.lineNumber + ")";
			}
		} catch (x) {
			text = "(unprintable exception)";
		}
		return text;
	}

	function is_coded(e)
	{
		return e !== null && typeof e === "object" && !(e instanceof Error)
			&& typeof e.code === "string" && CODES.indexOf(e.code) >= 0;
	}

	// ---- Who may use the web client ----

	// Why u may not use the web client, or null if it may.  need_password:
	// also refuse an account without a stored password (password sign-in)
	function refusal(u, config, need_password)
	{
		try {
			if (u === null || typeof u !== "object" || typeof u.number !== "number" || !(u.number >= 1))
				return "no such user";
			// The mail index holds user numbers in 16 bits (the mailbox
			// library refuses a higher one)
			if (u.number > 0xFFFF)
				return "user number above 65535";
			if (u.settings & UD.USER_DELETED)
				return "deleted";
			if (u.settings & UD.USER_INACTIVE)
				return "inactive";
			if (u.is_guest === true || !u.security || (u.security.restrictions & UD.UFLAG_G))
				return "guest";
			var ars = config && typeof config.access_requirements === "string"
				? config.access_requirements : "";
			if (ars !== "" && u.compare_ars(ars) !== true)
				return "access requirements";
			if (need_password && (typeof u.security.password !== "string" || u.security.password === ""))
				return "no password";
			return null;
		} catch (e) {
			Api.log(LOG_ERR, "webmail api: eligibility check failed: " + describe(e));
			return "check failed";
		}
	}

	function eligible(u, config)
	{
		return refusal(u, config, false) === null;
	}

	// The session stamp of a user: the hex SHA-256 digest of the password
	// and the password date ("<password>\n<date>").  Most ways to change a
	// password (the User object's setter, the terminal server, the sysop's
	// user editor) leave the date alone, so the password itself must be in
	// it; only the digest is stored.  Any change ends every web client
	// session made before it.
	function stamp_of(u)
	{
		var pw = "";
		var date = "";
		if (u !== null && typeof u === "object" && u.security) {
			if (typeof u.security.password === "string")
				pw = u.security.password;
			if (typeof u.security.password_date === "number")
				date = String(u.security.password_date);
		}
		return sha256_calc(pw + "\n" + date, /* hex: */ true);
	}

	function user_of(env, usernum)
	{
		var u = env.get_user(usernum);
		if (u === null || typeof u !== "object" || u.number !== usernum)
			return null;
		return u;
	}

	// The cookie values of env.cookie (a string or an array of strings)
	function cookie_values(c)
	{
		var list = c instanceof Array ? c : [c];
		var out = [];
		for (var i = 0; i < list.length && out.length < MAX_COOKIES; i++)
			if (typeof list[i] === "string" && list[i] !== "")
				out.push(list[i]);
		return out;
	}

	// The signed-in user from env.cookie (a browser can send several values
	// of the cookie, for different paths: the first that is a valid session
	// of an eligible user wins): {usernum, user, cookie, clear}; clear is
	// true when values were sent but none is valid (the cookie should be
	// cleared and the user told they were signed out).  Throws what Session
	// throws.
	function session_user(env, touch)
	{
		var out = { usernum: 0, user: null, cookie: null, clear: false };
		var values = cookie_values(env.cookie);
		if (values.length === 0)
			return out;
		var now = now_of(env);
		for (var i = 0; i < values.length; i++) {
			var value = values[i];
			var p = Session.parse(value);
			if (p === null)
				continue;
			var ud = env.userdata(p.usernum);
			// The user record is loaded only once the key has matched a
			// stored session (Session calls the stamp function then), so a
			// forged value costs one sessions-file read and no user read
			var u = null;
			var s = Session.check(ud, value, now, env.config, { touch: touch !== false,
				stamp: (function (n) {
					return function () {
						u = user_of(env, n);
						return stamp_of(u);
					};
				})(p.usernum) });
			if (s === null)
				continue;
			if (!eligible(u, env.config)) {
				try {
					Session.destroy(ud, value, now, env.config);
				} catch (e) {
					Api.log(LOG_WARNING, "webmail api: user " + s.usernum
						+ ": removing the session of an ineligible user: " + describe(e));
				}
				continue;
			}
			out.usernum = s.usernum;
			out.user = u;
			out.cookie = value;
			return out;
		}
		out.clear = true;
		return out;
	}

	// ---- Locale ----

	function locale_dirs(config)
	{
		var d = config && typeof config.directory === "string" ? config.directory : "";
		return { dir: d + "lib/", mods: d + "mods/" };
	}

	function locale_name(rq)
	{
		if (rq.available === null) {
			var dirs = locale_dirs(rq.config);
			try {
				rq.available = Locale.available(dirs.dir, dirs.mods);
			} catch (e) {
				Api.log(LOG_ERR, "webmail api: listing locales: " + describe(e));
				rq.available = [];
			}
		}
		var lang = rq.user && typeof rq.user.lang === "string" ? rq.user.lang : undefined;
		return Locale.pick(lang, rq.env.accept_language, rq.config.locale, rq.available);
	}

	// The strings of the request's locale, or null if they cannot be read
	function strings(rq)
	{
		var name = locale_name(rq);
		if (!own(rq.loaded, name)) {
			var dirs = locale_dirs(rq.config);
			try {
				rq.loaded[name] = Locale.load(dirs.dir, dirs.mods, name);
			} catch (e) {
				Api.log(LOG_ERR, "webmail api: loading locale " + name + ": " + describe(e));
				rq.loaded[name] = null;
			}
		}
		return rq.loaded[name];
	}

	function text(rq, code)
	{
		var s = null;
		try {
			s = strings(rq);
		} catch (e) {
			Api.log(LOG_ERR, "webmail api: locale: " + describe(e));
		}
		var key = "errors." + code;
		var t;
		if (s !== null && typeof s === "object" && own(s, key) && typeof s[key] === "string")
			t = s[key];
		else
			t = CODES.indexOf(code) >= 0 ? FALLBACK[code] : FALLBACK.internal;
		// The product name, as the shell page writes it in its own texts
		// (taken literally)
		if (code === "https") {
			var name = rq.config && typeof rq.config.product_name === "string"
				&& rq.config.product_name !== "" ? rq.config.product_name : "Webmail";
			t = t.split("{name}").join(name);
		}
		return t;
	}

	function failure(rq, code)
	{
		return { ok: false, error: code, message: text(rq, code) };
	}

	// ---- Arguments ----

	var BYTES_ONLY = /[^\x00-\xff]/;

	function check_value(d, v)
	{
		switch (d.type) {
			case "string":
				return typeof v === "string"
					&& v.length <= (d.max === undefined ? MAX_STRING : d.max)
					&& !BYTES_ONLY.test(v)
					&& (d.test === undefined || d.test(v) === true);
			case "int":
				return typeof v === "number" && isFinite(v) && Math.floor(v) === v
					&& v >= (d.min === undefined ? 0 : d.min)
					&& v <= (d.max === undefined ? 0x7fffffff : d.max);
			case "boolean":
				return typeof v === "boolean";
		}
		return false;
	}

	// The checked arguments (a new object), or null if they are invalid
	function check_args(spec, args)
	{
		if (args === undefined || args === null)
			args = {};
		if (typeof args !== "object" || args instanceof Array)
			return null;
		var k;
		// "__proto__" first: this engine's hasOwnProperty() is true for it
		// on every object, so the test below would let it through
		if (Object.prototype.hasOwnProperty.call(args, "__proto__")
			&& Object.keys(args).indexOf("__proto__") >= 0)
			return null;
		for (k in args) {
			if (k === "__proto__" || (own(args, k) && !own(spec, k)))
				return null;
		}
		var out = {};
		for (k in spec) {
			if (!own(spec, k))
				continue;
			if (!own(args, k)) {
				if (spec[k].required)
					return null;
				continue;
			}
			if (!check_value(spec[k], args[k]))
				return null;
			out[k] = args[k];
		}
		return out;
	}

	function valid_folder(v)
	{
		if (FOLDERS.indexOf(v) >= 0)
			return true;
		if (v.substr(0, 2) !== "u:")
			return false;
		var tag = v.substr(2);
		return tag.length >= 1 && tag.length <= MAX_TAG && tag.charAt(0) !== '$' && !WS_RE.test(tag);
	}

	function valid_msg_id(v)
	{
		return MSG_ID_RE.test(v);
	}

	// ---- Sessions ----

	function set_cookie(rq, value, keep)
	{
		if (typeof rq.env.set_cookie === "function")
			rq.env.set_cookie(value, keep === true);
	}

	// A user record string as UTF-8: kept when ASCII or valid UTF-8, else
	// converted from CP437
	function utf8_of(v)
	{
		var s = v === undefined || v === null ? "" : String(v);
		if (str_is_ascii(s) || str_is_utf8(s))
			return s;
		return utf8_encode(s);
	}

	function user_info(rq)
	{
		if (rq.user === null)
			return null;
		return { number: rq.user.number, alias: utf8_of(rq.user.alias), name: utf8_of(rq.user.name),
			avatar: avatar_of(rq, rq.user.number, rq.user.alias, "", 0) };
	}

	// ---- Avatars (exec/load/avatar_lib.js) ----

	var MSG_ANONYMOUS = 1 << 5;	// smbdefs.js
	var NET_FIDO = 2;		// smbdefs.js
	var NET_QWK = 4;
	var QWK_ID = /^[A-Za-z][A-Za-z0-9_-]{1,7}$/;
	var BASE64 = /^[A-Za-z0-9+\/]+={0,2}$/;

	// The avatar library for this request (env.avatar_lib when given,
	// null for none), or null when it cannot be loaded
	function avatar_lib(rq)
	{
		if (rq.avatar_lib !== undefined)
			return rq.avatar_lib;
		var lib = null;
		if (rq.env.avatar_lib !== undefined)
			lib = rq.env.avatar_lib;
		else {
			try {
				lib = load({}, "avatar_lib.js");
			} catch (e) {
				Api.log(LOG_WARNING, "webmail api: loading avatar_lib.js: " + describe(e));
				lib = null;
			}
		}
		rq.avatar_lib = lib;
		return lib;
	}

	// The base64 avatar data that lib.read() gives for these arguments
	// (as the terminal's message header passes them, see
	// sender_avatar()), or null: avatars turned off, none, turned off by
	// its owner, or not a valid avatar.  Never throws.
	function avatar_of(rq, usernum, name, netaddr, bbsid)
	{
		var lib = avatar_lib(rq);
		if (lib === null || typeof lib !== "object")
			return null;
		try {
			if (lib.options && lib.options.enabled === false)
				return null;
			var obj = lib.read(usernum, name, netaddr, bbsid);
			if (!lib.is_enabled(obj) || typeof obj.data !== "string" || !BASE64.test(obj.data))
				return null;
			return lib.is_valid(base64_decode(obj.data)) ? obj.data : null;
		} catch (e) {
			Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": avatar: " + describe(e));
			return null;
		}
	}

	// The sender's avatar for message id, chosen as the terminal's message
	// header does (exec/showmsgavatar.js): none for an anonymous message,
	// else avatar_lib.draw(from_ext || ftn_bbsid, from, from_net), which
	// reads (that, from, from_net, that).  The header is the sender's to
	// write, so what it may steer is checked first: from_ext only as a
	// number; ftn_bbsid only as a QWK ID (a letter, then 1 to 7 letters,
	// digits, "_" or "-"); and a sender that is not FidoNet or QWK is
	// looked up by name and address only, as an Internet sender: no user
	// number or BBS ID, and an address that can name no network file (no
	// "/" or "\", and an "@").
	function sender_avatar(ctx, rq, id)
	{
		try {
			var h = ctx.store.resolve(id).header;
			var from = h.from;	// a non-NULL field first (lazy fields)
			if (h.attr & MSG_ANONYMOUS)
				return null;
			var ext = h.from_ext ? String(h.from_ext) : "";
			if (!/^[0-9]{1,10}$/.test(ext))
				ext = "";
			var bbsid = h.ftn_bbsid ? String(h.ftn_bbsid) : "";
			if (!QWK_ID.test(bbsid))
				bbsid = "";
			var type = h.from_net_type || 0;
			var net = type ? String(h.from_net_addr || "") : "";
			var first = ext !== "" ? ext : bbsid;
			if (type && type !== NET_FIDO && type !== NET_QWK) {
				first = "";
				net = net.replace(/[\/\\]/g, "_");
				if (net.indexOf("@") < 0)
					net += "@";
			}
			return avatar_of(rq, first, typeof from === "string" ? from : "", net, first);
		} catch (e) {
			Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": sender avatar: " + describe(e));
			return null;
		}
	}

	// Sign usernum in: a new session and its cookie; the session the
	// browser had (if any) is removed
	function start_session(rq, usernum, u, keep)
	{
		var env = rq.env;
		var now = now_of(env);
		var value = Session.create(env.userdata(usernum), usernum, env.client || null, keep === true,
			now, rq.config, stamp_of(u));
		if (rq.usernum > 0 && typeof rq.cookie === "string") {
			try {
				Session.destroy(env.userdata(rq.usernum), rq.cookie, now, rq.config);
			} catch (e) {
				Api.log(LOG_WARNING, "webmail api: user " + rq.usernum
					+ ": removing the replaced session: " + describe(e));
			}
		}
		rq.usernum = usernum;
		rq.user = u;
		rq.cookie = value;
		set_cookie(rq, value, keep);
	}

	// The user signed in to webv4 (env.webv4_user(), called at most once per
	// request and only when webv4_login is on), or 0
	function webv4_user(rq)
	{
		if (rq.config.webv4_login !== true || typeof rq.env.webv4_user !== "function")
			return 0;
		if (rq.webv4 === null) {
			var n = rq.env.webv4_user();
			rq.webv4 = typeof n === "number" && n >= 1 && Math.floor(n) === n ? n : 0;
		}
		return rq.webv4;
	}

	// Not after a session.logout in the same request, and not when the
	// call says the browser signed out here (no_webv4)
	function webv4_sign_in(rq, args)
	{
		if (rq.usernum !== 0 || rq.config.webv4_login !== true || rq.logged_out
			|| args.no_webv4 === true)
			return;
		var n = webv4_user(rq);
		if (n === 0)
			return;
		var u = user_of(rq.env, n);
		if (!eligible(u, rq.config))
			return;
		start_session(rq, n, u, false);
		rq.via = "webv4";
	}

	// ---- Methods ----

	function auth_error()
	{
		return { code: "auth", message: "sign-in refused" };
	}

	// The "powered by" text: a version notice with runs of white space
	// (ASCII only: these are byte strings) made one space
	function powered_by(notice)
	{
		return typeof notice === "string"
			? notice.replace(/[ \t\r\n\f\v]+/g, " ").replace(/^ | $/g, "") : "";
	}

	function sending_domains()
	{
		// THE single source of the domains a user may send from.  To be
		// replaced by the sysop's configured list (and the user's own
		// choice among them).  Until then: the system's Internet address
		// only.
		var a = typeof system.inet_addr === "string"
			? system.inet_addr.replace(/^[ \t]+|[ \t]+$/g, "") : "";
		return a === "" ? [] : [ascii_lower(a)];
	}

	function session_get(ctx, args, rq)
	{
		webv4_sign_in(rq, args);
		var s = strings(rq);
		if (s === null)
			throw new Error("the locale strings cannot be read");
		var config = rq.config;
		var out = { user: user_info(rq), insecure: rq.env.secure !== true, locale: locale_name(rq) };
		if (rq.via !== null)
			out.via = rq.via;
		if (!rq.strings_sent) {
			out.strings = s;
			rq.strings_sent = true;
		}
		out.domains = Api.sending_domains();
		out.limits = { max_attachment_bytes: config.max_attachment_bytes,
			max_text_part_bytes: config.max_text_part_bytes, max_drafts: config.max_drafts };
		out.poll_ms = config.poll_interval * 1000;
		out.powered_by = powered_by(system.version_notice);
		for (var i = 0; i < BRAND_KEYS.length; i++) {
			var v = config[BRAND_KEYS[i]];
			out[BRAND_KEYS[i]] = typeof v === "string" ? v : "";
		}
		return out;
	}

	function refuse_unless_eligible(rq, n, u)
	{
		var why = refusal(u, rq.config, true);
		if (why !== null) {
			Api.log(LOG_NOTICE, "webmail: user #" + n + " refused: " + why);
			throw auth_error();
		}
	}

	function session_login(ctx, args, rq)
	{
		var name = args.user;
		var password = args.password;
		if (name === "" || password === "" || name.length > MAX_USER || password.length > MAX_PASSWORD)
			throw auth_error();
		// An account the library would refuse never reaches login(), which
		// records a logon when it succeeds
		var found = typeof rq.env.find_user === "function" ? rq.env.find_user(name) : 0;
		if (typeof found === "number" && found >= 1)
			refuse_unless_eligible(rq, found, user_of(rq.env, found));
		var n = rq.env.login(name, password);
		if (typeof n !== "number" || !(n >= 1) || Math.floor(n) !== n)
			throw auth_error();
		var u = user_of(rq.env, n);
		refuse_unless_eligible(rq, n, u);
		start_session(rq, n, u, args.keep === true);
		return { user: user_info(rq) };
	}

	function session_logout(ctx, args, rq)
	{
		var n = rq.usernum;
		if (typeof rq.cookie === "string")
			Session.destroy(rq.env.userdata(n), rq.cookie, now_of(rq.env), rq.config);
		rq.usernum = 0;
		rq.user = null;
		rq.cookie = null;
		rq.logged_out = true;
		set_cookie(rq, "", false);
		return { webv4: n !== 0 && webv4_user(rq) === n };
	}

	function folder_list(ctx, args, rq)
	{
		var purged = 0;
		try {
			var res = ctx.store.purge_expired();
			if (res instanceof Array) {
				for (var i = 0; i < res.length; i++)
					if (res[i] && res[i].ok)
						purged++;
			}
		} catch (e) {
			Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": purge_expired: " + describe(e));
		}
		return { folders: ctx.store.folders(), usage: ctx.store.usage(), purged: purged,
			trash_days: ctx.store.options.trash_days };
	}

	function message_list(ctx, args, rq)
	{
		var opts = {};
		if (args.offset !== undefined)
			opts.offset = args.offset;
		if (args.limit !== undefined)
			opts.limit = args.limit;
		if (args.search !== undefined)
			opts.search = args.search;
		var list = ctx.store.list(args.folder, opts);
		return { total: list.total, items: list.items };
	}

	function message_get(ctx, args, rq)
	{
		if (args.head === true)
			return { item: ctx.store.get(args.id, { mark_read: args.mark_read === true, head: true }) };
		var msg = ctx.store.get(args.id, { mark_read: args.mark_read === true });
		msg.avatar = sender_avatar(ctx, rq, args.id);
		return { message: msg };
	}

	function changes(ctx, args, rq)
	{
		var s = ctx.store.state();
		rq.state = s;
		rq.state_fresh = true;
		return { changed: s !== args.state, state: s };
	}

	var methods = {
		"session.get": { auth: false, args: {
			no_webv4: { type: "boolean" }
		}, run: session_get },
		"session.login": { auth: false, args: {
			user: { type: "string", required: true, max: MAX_CREDENTIAL },
			password: { type: "string", required: true, max: MAX_CREDENTIAL },
			keep: { type: "boolean" }
		}, run: session_login },
		"session.logout": { auth: true, ctx: false, args: {}, run: session_logout },
		"folder.list": { auth: true, args: {}, run: folder_list },
		"message.list": { auth: true, args: {
			folder: { type: "string", required: true, max: MAX_TAG + 2, test: valid_folder },
			offset: { type: "int", min: 0, max: 10000000 },
			limit: { type: "int", min: 1, max: 100 },
			search: { type: "string" }
		}, run: message_list },
		"message.get": { auth: true, args: {
			id: { type: "string", required: true, max: 11, test: valid_msg_id },
			mark_read: { type: "boolean" },
			head: { type: "boolean" }
		}, run: message_get },
		"changes": { auth: true, args: {
			state: { type: "string", required: true }
		}, run: changes }
	};

	// ---- The request body ----

	// A string of UTF-16 code units as UTF-8 bytes (pairs combined); throws
	// URIError for a lone surrogate
	function to_utf8(s)
	{
		if (!/[^\x00-\x7f]/.test(s))
			return s;
		return unescape(encodeURIComponent(s));
	}

	// v with every string (keys too) converted by to_utf8; throws for a
	// lone surrogate or nesting deeper than MAX_DEPTH
	function convert(v, depth)
	{
		if (typeof v === "string")
			return to_utf8(v);
		if (v === null || typeof v !== "object")
			return v;
		if (depth >= MAX_DEPTH)
			throw new RangeError("nested too deep");
		var i;
		if (v instanceof Array) {
			var a = [];
			for (i = 0; i < v.length; i++)
				a.push(convert(v[i], depth + 1));
			return a;
		}
		var o = {};
		var keys = Object.keys(v);
		for (i = 0; i < keys.length; i++) {
			// defineProperty: assigning "__proto__" would set the prototype
			// instead of making a property (check_args then refuses it)
			Object.defineProperty(o, to_utf8(keys[i]), { value: convert(v[keys[i]], depth + 1),
				enumerable: true, writable: true, configurable: true });
		}
		return o;
	}

	// The request from a POST body (bytes, as the web server gives them), or
	// null if it is not valid UTF-8 or not JSON.  Text arrives two ways,
	// as raw UTF-8 and as \u escapes; both end up the same: the bytes are
	// decoded to UTF-16, parsed, and every string (keys too) is encoded
	// back to UTF-8 bytes, which is what the libraries work with.  A lone
	// surrogate (an escape that is half a pair) is refused.
	function parse_body(text)
	{
		if (typeof text !== "string" || text === "")
			return null;
		try {
			if (/[\x80-\xff]/.test(text)) {
				// str_is_utf8 refuses encoded surrogates (which this engine's
				// decodeURIComponent accepts) and overlong forms;
				// decodeURIComponent refuses what is above U+10FFFF
				if (/[^\x00-\xff]/.test(text) || !str_is_utf8(text))
					return null;
				text = decodeURIComponent(escape(text));
			}
			return convert(JSON.parse(text), 0);
		} catch (e) {
			return null;
		}
	}

	// ---- The dispatcher ----

	function well_formed(request)
	{
		if (request === null || typeof request !== "object" || request instanceof Array)
			return false;
		var calls = request.calls;
		if (!(calls instanceof Array) || calls.length > MAX_CALLS)
			return false;
		for (var i = 0; i < calls.length; i++) {
			if (!(calls[i] instanceof Array) || calls[i].length < 1 || calls[i].length > 3)
				return false;
		}
		return true;
	}

	function call_id(v)
	{
		if (typeof v === "string" && v.length <= MAX_CALL_ID && /^[\x20-\x7e]*$/.test(v))
			return v;
		if (typeof v === "number" && isFinite(v))
			return v;
		return null;
	}

	function get_ctx(rq)
	{
		if (!rq.ctx_tried) {
			rq.ctx_tried = true;
			try {
				rq.ctx = rq.env.make_ctx(rq.usernum);
				if (rq.ctx === null || typeof rq.ctx !== "object" || !rq.ctx.store)
					throw new Error("make_ctx returned no store");
			} catch (e) {
				rq.ctx_error = e;
			}
		}
		if (rq.ctx_error !== null)
			throw rq.ctx_error;
		if (rq.ctx.usernum !== rq.usernum)
			throw auth_error();	// the user changed within this request
		return rq.ctx;
	}

	function run_call(rq, call)
	{
		var name = call[0];
		// The name pattern also keeps out "__proto__", which this engine's
		// hasOwnProperty() reports as an own property of every object
		if (typeof name !== "string" || !METHOD_RE.test(name) || !own(methods, name))
			return failure(rq, "invalid");
		var m = methods[name];
		if (m === null || typeof m !== "object" || typeof m.run !== "function")
			return failure(rq, "invalid");
		if (m.auth && rq.usernum === 0)
			return failure(rq, "auth");
		var args = check_args(m.args || {}, call.length > 1 ? call[1] : undefined);
		if (args === null)
			return failure(rq, "invalid");
		try {
			var ctx = null;
			if (m.auth && m.ctx !== false) {
				ctx = get_ctx(rq);
				rq.state_fresh = false;
			}
			var out = m.run(ctx, args, rq);
			var result = { ok: true };
			if (out !== null && typeof out === "object") {
				for (var k in out) {
					if (own(out, k) && k !== "ok")
						result[k] = out[k];
				}
			}
			return result;
		} catch (e) {
			if (is_coded(e)) {
				if (e.code === "failed" || e.code === "conflict")
					Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": " + name + ": "
						+ describe(e));
				return failure(rq, e.code);
			}
			Api.log(LOG_ERR, "webmail api: user " + rq.usernum + ": " + name + ": " + describe(e));
			return failure(rq, "internal");
		}
	}

	function new_request(env)
	{
		return { env: env, config: env && env.config ? env.config : {}, usernum: 0, user: null,
			cookie: null, via: null, webv4: null, signed_out: false, logged_out: false,
			ctx: null, ctx_tried: false, ctx_error: null,
			available: null, loaded: {}, strings_sent: false, state: null, state_fresh: false,
			avatar_lib: undefined };
	}

	function only_changes(calls)
	{
		if (calls.length === 0)
			return false;
		for (var i = 0; i < calls.length; i++)
			if (calls[i][0] !== "changes")
				return false;
		return true;
	}

	function dispatch(env, request)
	{
		var rq = null;
		try {
			rq = new_request(env);
			if (!well_formed(request))
				return { error: "invalid", message: text(rq, "invalid"), results: [], state: "" };
			var calls = request.calls;
			var results = [];
			var i;
			var gate = null;
			if (env.has_header !== true)
				gate = "forbidden";
			else if (env.secure !== true && rq.config.require_tls !== false)
				gate = "https";
			if (gate === null) {
				try {
					var s = session_user(env, request.background !== true && !only_changes(calls));
					rq.usernum = s.usernum;
					rq.user = s.user;
					rq.cookie = s.cookie;
					if (s.clear) {
						set_cookie(rq, "", false);
						rq.signed_out = true;
					}
				} catch (e) {
					Api.log(LOG_ERR, "webmail api: checking the session: " + describe(e));
					gate = "internal";
				}
			}
			for (i = 0; i < calls.length; i++) {
				var id = call_id(calls[i].length > 2 ? calls[i][2] : null);
				results.push([id, gate !== null ? failure(rq, gate) : run_call(rq, calls[i])]);
			}
			var state = "";
			if (rq.ctx !== null && rq.ctx_error === null && rq.usernum !== 0
				&& rq.ctx.usernum === rq.usernum) {
				if (rq.state_fresh)
					state = rq.state;
				else {
					try {
						state = rq.ctx.store.state();
					} catch (e) {
						Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": state: " + describe(e));
					}
				}
			}
			// signed_out only when the request did not end signed in (a
			// sign-in later in the same request cancels it)
			return { results: results, state: state, signed_out: rq.signed_out && rq.usernum === 0 };
		} catch (e) {
			Api.log(LOG_ERR, "webmail api: dispatch: " + describe(e));
			var msg = FALLBACK.internal;
			try {
				msg = text(rq || new_request({}), "internal");
			} catch (x) {
			}
			return { error: "internal", message: msg, results: [], state: "" };
		} finally {
			if (rq !== null && rq.ctx !== null) {
				try {
					if (typeof rq.ctx.close === "function")
						rq.ctx.close();
					else if (rq.ctx.store && typeof rq.ctx.store.close === "function")
						rq.ctx.store.close();
				} catch (e) {
					Api.log(LOG_WARNING, "webmail api: closing the mailbox: " + describe(e));
				}
			}
		}
	}

	// ---- Helpers for the shims ----

	// A make_ctx for env: opts { config, get_user(usernum), userdata(usernum),
	// Store (the constructor) or load_store() (returns it; called once, on
	// the first make_ctx, so a request that opens no mailbox never loads
	// the mailbox library), shared_path (tests; default the "mail" base),
	// data_dir (tests; default system.data_dir) }
	function context_factory(opts)
	{
		var Store = opts.Store || null;
		return function (usernum) {
			var u = user_of(opts, usernum);
			if (u === null)
				throw auth_error();
			if (Store === null)
				Store = opts.load_store();
			var store = new Store({ usernum: usernum, alias: u.alias, name: u.name,
				shared_path: opts.shared_path, data_dir: opts.data_dir,
				options: { max_text_part_bytes: opts.config.max_text_part_bytes } });
			store.open();
			return { usernum: usernum, alias: u.alias, name: u.name, user: u, store: store,
				userdata: opts.userdata(usernum), config: opts.config,
				close: function () {
					store.close();
				} };
		};
	}

	// The user's web client directory
	function userdata_dir(data_dir, usernum)
	{
		return data_dir + format("user/%04u/webmail/", usernum);
	}

	// The app's base path for the cookie, from the request's virtual path
	// ("/mail/api.ssjs" -> "/mail/"); "/" when it is not usable
	function base_path(vpath)
	{
		if (typeof vpath !== "string")
			return "/";
		var i = vpath.lastIndexOf('/');
		if (i < 0)
			return "/";
		var p = vpath.substr(0, i + 1);
		if (p.length > 256 || !/^\/(?:[\x21-\x3a\x3c-\x7e]*\/)?$/.test(p))
			return "/";
		return p;
	}

	var Api = {
		// The libraries this one loaded, for callers that need them too
		Session: Session,
		Locale: Locale,
		methods: methods,
		dispatch: dispatch,
		parse_body: parse_body,
		session_user: session_user,
		eligible: eligible,
		sending_domains: sending_domains,
		powered_by: powered_by,
		context_factory: context_factory,
		userdata_dir: userdata_dir,
		base_path: base_path,
		log: function (level, text) {
			log(level, text);
		}
	};

	return Api;
})();

Api;
