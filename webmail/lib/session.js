// Session: the web client's login sessions and their cookie.
//
// var Session = load("session.js");
//
// The cookie value is "<usernum>.<key>": the user number in plain digits
// (1 to 10 digits, no leading zero) and a key of 40 lowercase hexadecimal
// characters (160 bits).  Anything else is rejected before any file is
// touched; uppercase hex is rejected too, since keys are only ever issued
// in lowercase.
//
// The key comes from cryptlib's random number generator (the same one the
// servers use for TLS): an HMAC-SHA2 CryptContext gets a generated key,
// and its MAC over a fixed text is the output.  An HMAC under a random key
// is indistinguishable from random, so the first 20 of its 32 bytes are
// the session key.  cryptlib gathers its entropy from the operating system
// (/dev/urandom and system polling on Unix-likes, the CryptoAPI and system
// polling on Windows).  If CryptContext cannot be used, create() throws:
// it never falls back to a weaker source.
//
// Storage: sessions.json in the user's web client directory (through
// UserData, so the file is webmail.sessions.json), an object keyed by the
// hex SHA-256 hash of each session's key; the key itself is never stored,
// so the file does not yield usable cookies.  Each value is
//   { usernum: <the user number the session was issued for>,
//     created: <time_t>, seen: <time_t, last use, updated at most once a
//     minute>, keep: <boolean, "keep me signed in">,
//     client: { ip: <string, up to MAX_IP>, agent: <string, up to
//     MAX_AGENT> } [, stamp: <string given to create()>]
//     [, via: <string given to create(): how the session was started>] }
// client describes the device for a list of signed-in devices: control
// characters are removed and the strings truncated.  A user has at most
// MAX_SESSIONS sessions; creating one more removes the least recently
// seen.  create, destroy and a check that writes seen also remove the
// expired sessions (and any malformed entries) they find; an expired
// entry alone never makes check() write.
//
// Expiry: a session without keep is invalid once config.session_timeout
// seconds have passed since it was last seen, or 24 hours after it was
// created, whichever comes first.  One with keep is invalid once
// config.remember_days days have passed since it was created, however
// recently it was used.  Since seen is recorded at most once a minute, an
// idle session can expire up to a minute early.  Idle means no action by
// the user: requests the browser makes by itself (polling for new mail)
// call check() with { touch: false }, which never refreshes seen.
//
// Stamps: create() may store a stamp, a string the caller chooses (the web
// client passes a digest of the user's password and its date).  check()
// given { stamp }
// accepts a session only if its stored stamp is the same; a session stored
// without one matches only a check given none.  stamp may also be a
// function returning the stamp: it is called only after the key has
// matched a live session, so the caller can load what the stamp is made
// from (a user record) only for a value that is not forged.  A stamp
// function is given the session's via (a string, or undefined), so a
// caller can make the stamp of a session depend on how it was started.
// So changing the stamp (changing the password) ends every session made
// before.  A check given a stamp that does not match also removes that
// session (waiting at most 250 ms for the lock; if it is busy the session
// stays stored, unusable).
//
// Functions (now is a time_t, time() when omitted; config is Config.load()):
//
//   Session.parse(value)  { usernum, key } or null if value is not a valid
//                         cookie value.  Touches no file; never throws.
//   Session.create(userdata, usernum, client_info, keep [, now, config,
//                  stamp, via])
//                         stores a new session and returns its cookie
//                         value.  client_info is { ip, agent } or null.
//                         stamp, if a string, is stored (see Stamps); so
//                         is via, if it is 1 to 16 lowercase letters and
//                         digits (any other via throws an Error).
//                         Without config, expired sessions are not removed
//                         (the cap still applies).  Throws
//                         { code: "conflict" } if sessions.json stays
//                         locked, an Error if the random source fails or
//                         usernum is invalid, and whatever a file write
//                         throws.
//   Session.check(userdata, value, now, config [, { touch, stamp }])
//                         { usernum, keep } for a valid session, else null
//                         (also when the stamps differ: see Stamps).
//                         Never throws for a bad value.  Reads without the
//                         lock.  With touch (the default) and seen a minute
//                         old, it writes seen, waiting at most 250 ms for
//                         the lock; a lock conflict then does not fail a
//                         valid check, but any other error is rethrown.
//                         With touch false it writes only to remove a
//                         session whose stamp no longer matches.
//   Session.destroy(userdata, value [, now, config])
//                         removes the session; true if it existed.  A
//                         malformed value or unknown key is a no-op
//                         returning false.  Throws { code: "conflict" } if
//                         sessions.json stays locked, and rethrows other
//                         errors.
//   Session.destroy_all(userdata, usernum)
//                         removes every session in that user's file (a
//                         password change, "sign out everywhere"); returns
//                         the number removed.  No file is written when
//                         there are none.  Throws as destroy() does, and
//                         an Error for an invalid usernum.
//   Session.cookie_header(value, { secure, keep, config, path })
//                         the attributes to follow "<name>=<value>" in
//                         Set-Cookie (see the function).
//   Session.is_secure(scheme, headers, config)
//                         true if the request reached the client over TLS.
//
// The caller maps anything thrown to a fixed error for the browser and
// logs the detail; an exception's text never goes to the browser.
//
// Calling sequence for an HTTP request (the shim owns the cookie name and
// knows which directory belongs to which user):
//   1. var p = Session.parse(cookie_value); if p is null: not signed in.
//   2. Open the UserData for p.usernum
//      (system.data_dir + format("user/%04u/webmail/", p.usernum)) and
//      load user p.usernum, for its stamp (a digest of the
//      password and its date).
//   3. var s = Session.check(that_userdata, cookie_value, time(), config,
//      { touch: <false for background polling, else true>, stamp });
//      if s is null: not signed in (clear the cookie).
//   4. Confirm the account still exists, is not deleted or inactive, and
//      meets access_requirements: check() does not look at the user
//      record.
//   5. Act as that user.
// check() parses the value again and accepts it only if the stored session
// was issued for the user number in the value, so opening the wrong
// user's directory can yield null but never another user's session.
// A well-formed cookie for a user who has a sessions file costs a file
// read and one for a user who has none does not, so response timing can
// reveal which user numbers have signed in to the web client; user
// numbers are not secret.
// At sign-in: value = Session.create(ud, usernum, { ip, agent }, keep,
// time(), config, stamp), then Set-Cookie: <name>=<value> +
// cookie_header(value, { secure: Session.is_secure(...), keep, config,
// path }).  At sign-out:
// Session.destroy(ud, value, time(), config), then Set-Cookie: <name>= +
// cookie_header("", { secure, config, path }).  After a password change:
// Session.destroy_all(ud, usernum), then create a new session for the
// browser that made the change (with a stamp, the old sessions also stop
// matching by themselves).

"use strict";

var Session = (function () {
	var FILE = "sessions";
	var MAX_SESSIONS = 30;
	var MAX_IP = 64;
	var MAX_AGENT = 256;
	var SEEN_INTERVAL = 60;	// seconds between writes of seen
	var MAX_LIFETIME = 86400;	// seconds a session without keep can last
	var CHECK_TIMEOUT = 250;	// ms check() waits for the lock
	var DAY = 86400;
	var VIA_RE = /^[a-z0-9]{1,16}$/;
	var seq = 0;

	function now_or(now)
	{
		return typeof now === "number" && isFinite(now) ? now : time();
	}

	function to_hex(bytes)
	{
		var s = "";
		for (var i = 0; i < bytes.length; i++) {
			var c = bytes.charCodeAt(i);
			s += (c < 16 ? "0" : "") + c.toString(16);
		}
		return s;
	}

	// 40 lowercase hex characters from cryptlib's random number generator
	function new_key()
	{
		var c = new CryptContext(CryptContext.ALGO["HMAC-SHA2"]);
		c.generate_key();
		c.encrypt("webmail session " + (seq++));
		var mac = c.hashvalue;
		if (typeof mac !== "string" || mac.length < 20)
			throw new Error("session key generation failed");
		var key = to_hex(mac.substr(0, 20));
		if (!/^[0-9a-f]{40}$/.test(key))
			throw new Error("session key generation failed");
		return key;
	}

	function hash(key)
	{
		return sha256_calc(key, /* hex: */ true);
	}

	// Constant-time comparison: the time depends only on the length of b
	// (the stored hash), not on where a and b differ
	function same(a, b)
	{
		var diff = a.length ^ b.length;
		for (var i = 0; i < b.length; i++)
			diff |= (a.charCodeAt(i) | 0) ^ b.charCodeAt(i);
		return diff === 0;
	}

	function parse(value)
	{
		if (typeof value !== "string" || value.length > 51)
			return null;
		var m = /^([1-9][0-9]{0,9})\.([0-9a-f]{40})$/.exec(value);
		if (!m)
			return null;
		var usernum = parseInt(m[1], 10);
		if (usernum > 0xffffffff)
			return null;
		return { usernum: usernum, key: m[2] };
	}

	function valid_entry(e)
	{
		return e !== null && typeof e === "object"
			&& typeof e.usernum === "number"
			&& typeof e.created === "number" && isFinite(e.created)
			&& typeof e.seen === "number" && isFinite(e.seen)
			&& typeof e.keep === "boolean";
	}

	function expired(e, now, config)
	{
		if (!valid_entry(e))
			return true;
		if (!config)
			return false;
		if (e.keep)
			return now - e.created >= config.remember_days * DAY;
		return now - e.seen >= config.session_timeout
			|| now - e.created >= MAX_LIFETIME;
	}

	function sessions_of(obj)
	{
		if (obj === null || typeof obj !== "object" || obj instanceof Array)
			return {};
		return obj;
	}

	// The hash in sessions that matches h, or null; every stored hash is
	// compared, with no early exit
	function find(sessions, h)
	{
		var found = null;
		for (var k in sessions) {
			if (same(h, k))
				found = k;
		}
		return found;
	}

	// Removes expired and malformed entries; returns how many
	function prune(sessions, now, config)
	{
		var n = 0;
		for (var k in sessions) {
			if (expired(sessions[k], now, config)) {
				delete sessions[k];
				n++;
			}
		}
		return n;
	}

	function clean(v, max)
	{
		if (v === undefined || v === null)
			return "";
		return String(v).replace(/[\x00-\x1f\x7f]/g, "").substr(0, max);
	}

	function create(userdata, usernum, client_info, keep, now, config, stamp, via)
	{
		if (typeof usernum !== "number" || !(usernum >= 1 && usernum <= 0xffffffff)
			|| Math.floor(usernum) !== usernum)
			throw new Error("Session.create: invalid user number");
		if (via !== undefined && !(typeof via === "string" && VIA_RE.test(via)))
			throw new Error("Session.create: invalid via");
		now = now_or(now);
		var key = new_key();
		var h = hash(key);
		var ci = client_info || {};
		var entry = {
			usernum: usernum,
			created: now,
			seen: now,
			keep: keep === true,
			client: { ip: clean(ci.ip, MAX_IP), agent: clean(ci.agent, MAX_AGENT) }
		};
		if (typeof stamp === "string")
			entry.stamp = stamp;
		if (via !== undefined)
			entry.via = via;
		userdata.update(FILE, {}, function (obj) {
			var sessions = sessions_of(obj);
			prune(sessions, now, config);
			var keys = [];
			for (var k in sessions)
				keys.push(k);
			// Least recently seen first
			keys.sort(function (a, b) {
				var sa = sessions[a], sb = sessions[b];
				if (sa.seen !== sb.seen)
					return sa.seen - sb.seen;
				if (sa.created !== sb.created)
					return sa.created - sb.created;
				return a < b ? -1 : a > b ? 1 : 0;
			});
			for (var i = 0; keys.length - i >= MAX_SESSIONS; i++)
				delete sessions[keys[i]];
			sessions[h] = entry;
			return sessions;
		});
		return usernum + "." + key;
	}

	// The stamp stored with a session, or undefined
	function stamp_of(e)
	{
		return typeof e.stamp === "string" ? e.stamp : undefined;
	}

	// How a session was started (create()'s via), or undefined
	function via_of(e)
	{
		return typeof e.via === "string" && VIA_RE.test(e.via) ? e.via : undefined;
	}

	// Removes the session whose key hashes to h, after its stamp no longer
	// matched; a busy lock leaves it for later (it can never match again)
	function remove_stale(userdata, h, now, config)
	{
		try {
			userdata.update(FILE, {}, function (obj) {
				var cur = sessions_of(obj);
				prune(cur, now, config);
				var ck = find(cur, h);
				if (ck !== null)
					delete cur[ck];
				return cur;
			}, { timeout: CHECK_TIMEOUT });
		} catch (err) {
			if (!err || err.code !== "conflict")
				throw err;
		}
	}

	function check(userdata, value, now, config, options)
	{
		var p = parse(value);
		if (p === null)
			return null;
		now = now_or(now);
		var touch = !(options && options.touch === false);
		var sessions = sessions_of(userdata.read(FILE, null));
		var h = hash(p.key);
		var k = find(sessions, h);
		var e = k === null ? null : sessions[k];
		if (e === null || !valid_entry(e) || e.usernum !== p.usernum
			|| expired(e, now, config))
			return null;
		// The stamp is looked at only now, once the key has matched: a stamp
		// function (which may read a user record) never runs for a forged
		// value
		var stamp = options ? options.stamp : undefined;
		if (typeof stamp === "function")
			stamp = stamp(via_of(e));
		if (typeof stamp !== "string")
			stamp = undefined;
		if (stamp_of(e) !== stamp) {
			if (stamp !== undefined)
				remove_stale(userdata, h, now, config);
			return null;
		}
		var ok = true;
		if (touch && now - e.seen >= SEEN_INTERVAL) {
			try {
				userdata.update(FILE, {}, function (obj) {
					var cur = sessions_of(obj);
					prune(cur, now, config);
					var ck = find(cur, h);
					if (ck === null)
						ok = false;	// destroyed or expired meanwhile
					else if (now - cur[ck].seen >= SEEN_INTERVAL)
						cur[ck].seen = now;
					return cur;
				}, { timeout: CHECK_TIMEOUT });
			} catch (err) {
				if (!err || err.code !== "conflict")
					throw err;
				// Locked: the session stays valid; seen is written next time
			}
		}
		if (!ok)
			return null;
		return { usernum: e.usernum, keep: e.keep };
	}

	function destroy(userdata, value, now, config)
	{
		var p = parse(value);
		if (p === null)
			return false;
		var h = hash(p.key);
		var k = find(sessions_of(userdata.read(FILE, null)), h);
		if (k === null)
			return false;
		now = now_or(now);
		var removed = false;
		userdata.update(FILE, {}, function (obj) {
			var cur = sessions_of(obj);
			prune(cur, now, config);
			var ck = find(cur, h);
			if (ck !== null && valid_entry(cur[ck]) && cur[ck].usernum === p.usernum) {
				delete cur[ck];
				removed = true;
			}
			return cur;
		});
		return removed;
	}

	function destroy_all(userdata, usernum)
	{
		if (typeof usernum !== "number" || !(usernum >= 1 && usernum <= 0xffffffff)
			|| Math.floor(usernum) !== usernum)
			throw new Error("Session.destroy_all: invalid user number");
		var sessions = sessions_of(userdata.read(FILE, null));
		var k;
		var any = false;
		for (k in sessions) {
			any = true;
			break;
		}
		if (!any)
			return 0;
		var removed = 0;
		userdata.update(FILE, {}, function (obj) {
			var cur = sessions_of(obj);
			for (var ck in cur)
				removed++;
			return {};
		});
		return removed;
	}

	// "; Path=<path>; HttpOnly; Secure; SameSite=Lax", without
	// "; Secure" when opts.secure is not true, followed by
	// "; Max-Age=<remember_days in seconds>" when opts.keep is true.  An
	// empty value (clearing the cookie at sign-out) gets "; Max-Age=0"
	// instead.  path is the app's base path; it must start and end with
	// "/" and hold only printable ASCII other than space and ";",
	// otherwise "/" is used.
	//
	// Lax, not Strict: with Strict a browser leaves the cookie out of a
	// page load it does not count as started by this site, so the user
	// has to sign in again although the session is still good.  Seen with
	// Safari on iOS: the reload of the page itself came without the
	// cookie one second after an API request that carried it.  A link to
	// the page from elsewhere (a bookmark page, another app) is the same
	// case.  Lax still keeps the cookie out of every request another
	// site makes from its own page (forms that POST, images, frames,
	// scripts); it is sent only when the user is taken to a page here.
	function cookie_header(value, opts)
	{
		opts = opts || {};
		var path = opts.path;
		if (typeof path !== "string" || path.length > 256
			|| !/^\/(?:[\x21-\x3a\x3c-\x7e]*\/)?$/.test(path))
			path = "/";
		var s = "; Path=" + path + "; HttpOnly";
		if (opts.secure === true)
			s += "; Secure";
		s += "; SameSite=Lax";
		if (value === "")
			s += "; Max-Age=0";
		else if (opts.keep === true && opts.config)
			s += "; Max-Age=" + (opts.config.remember_days * DAY);
		return s;
	}

	// ASCII-only lowercase and trim (byte strings: see mailbox_tags.js)
	function ascii_lower(s)
	{
		return s.replace(/[A-Z]+/g, function (m) {
			return m.toLowerCase();
		});
	}

	function ascii_trim(s)
	{
		return s.replace(/^[ \t\r\n]+|[ \t\r\n]+$/g, "");
	}

	// True when scheme is https, or when config.tls_proxy_header names a
	// request header (any case) whose value, or the first element of a
	// comma-separated list, is https (any case, white space ignored).
	// Every header matching the name must say so.  An empty setting trusts
	// no header.
	function is_secure(scheme, headers, config)
	{
		if (typeof scheme === "string" && ascii_lower(ascii_trim(scheme)) === "https")
			return true;
		if (!config || typeof config.tls_proxy_header !== "string")
			return false;
		var name = ascii_lower(ascii_trim(config.tls_proxy_header)
			.replace(/[ \t]*:$/, ""));
		if (name === "" || headers === null || typeof headers !== "object")
			return false;
		var seen = false;
		for (var k in headers) {
			if (ascii_lower(String(k)) !== name)
				continue;
			var v = headers[k];
			if (typeof v !== "string")
				return false;
			if (ascii_lower(ascii_trim(v.split(",")[0])) !== "https")
				return false;
			seen = true;
		}
		return seen;
	}

	return {
		MAX_SESSIONS: MAX_SESSIONS,
		MAX_IP: MAX_IP,
		MAX_AGENT: MAX_AGENT,
		parse: parse,
		create: create,
		check: check,
		destroy: destroy,
		destroy_all: destroy_all,
		cookie_header: cookie_header,
		is_secure: is_secure
	};
})();

Session;
