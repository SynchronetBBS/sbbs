// Shared helpers for the web client library tests (exec/tests/webmail/*.js).
// Loads the mailbox kit (exec/tests/mailbox/kit.js) and adds to the same
// Kit object.  Run on its own (as the test runner does) this file only
// defines things.  A test loads it with:  load(js.exec_dir + "kit.js");
// js.exec_dir is the directory of the running test, <exec>/tests/webmail/.

"use strict";

load(js.exec_dir + "../mailbox/kit.js");	// defines the global Kit

(function () {
	var scratch = null;

	// Load webmail/lib/<name>.js from this worktree (the directory above
	// exec/, found from the test location) and return its namespace
	Kit.wlib = function (name) {
		var path = Kit.exec_root() + "../webmail/lib/" + name + ".js";
		if (!file_exists(path))
			throw new Error("Kit.wlib: no such library: " + path);
		return load({}, path);
	};

	// A scratch stand-in for data/user/####/webmail/, created if absent.
	// It lies inside a Kit.scratch() tree (data/user/<####>/webmail/), so
	// it goes away with that tree's cleanup().
	//   Kit.webmail_dir(usernum, scr)  inside the scratch tree 'scr' (the
	//                                  caller's, cleaned by scr.cleanup())
	//   Kit.webmail_dir(usernum)       inside one tree this kit makes on
	//                                  first use; remove it with
	//                                  Kit.webmail_cleanup()
	Kit.webmail_dir = function (usernum, scr) {
		if (!scr) {
			if (!scratch)
				scratch = Kit.scratch();
			scr = scratch;
		}
		var d = scr.user_dir(usernum) + "webmail/";
		mkpath(d);
		return d;
	};

	// Removes the tree made by Kit.webmail_dir(usernum) (no scratch given)
	Kit.webmail_cleanup = function () {
		if (scratch)
			scratch.cleanup();
		scratch = null;
	};

	// A world for driving Api.dispatch with a fake env over real Store,
	// Session and UserData objects on scratch files (as api.js's own test
	// does).  Returns W: the libraries (Api, Store, Session, UserData,
	// JsonFiles, D smbdefs, U userdefs), cfg (access_requirements set),
	// STRINGS (en_us), USERS (fake User objects; add_user(n, o)), the
	// user numbers ALICE, BOB, GUEST, DENIED (fails the access
	// requirements), S (a Kit.scratch() tree), shared (the scratch mail
	// base, opened by W.open_shared()), and:
	//   make_env(o)        an env; o: cookie, secure, has_header, config,
	//                      make_ctx, now; counts ev.made / ev.closed
	//   call(ev, m, args)  one call's result
	//   err(result, code)  asserts a failed result with the locale text
	//   save(hdr, body)    saves to the scratch mail base, returns number
	//   sign_in(usernum)   a session cookie made by a password sign-in
	//   cookie_for(usernum)  a session stored directly (any user, also
	//                      one that could not sign in)
	//   cleanup()          closes the base and removes the scratch tree
	Kit.api_world = function () {
		var W = {};
		var Api = W.Api = Kit.wlib("api");
		var Config = Kit.wlib("config");
		var UserData = W.UserData = Kit.wlib("userdata");
		W.Session = Kit.wlib("session");
		var JsonFiles = W.JsonFiles = Kit.lib("mailbox_files");
		W.Store = Kit.lib("mailbox");
		W.D = {};
		load(W.D, Kit.exec_root() + "load/smbdefs.js");
		var U = W.U = {};
		load(U, Kit.exec_root() + "load/userdefs.js");
		W.ALICE = 60001;
		W.BOB = 60002;
		W.GUEST = 60003;
		W.DENIED = 60004;
		var dir = Kit.exec_root() + "../webmail/";
		Config.warn = function () {};
		W.cfg = Config.load({ directory: dir, access_requirements: "LEVEL 10" });
		W.STRINGS = Kit.wlib("locale").load(dir + "lib/", dir + "mods/", "en_us");
		var USERS = W.USERS = {};
		W.add_user = function (n, o) {
			o = o || {};
			USERS[n] = {
				number: n,
				alias: o.alias || ("user" + n),
				name: o.name || ("User " + n),
				lang: "",
				settings: o.settings || 0,
				// as User.is_guest (user_is_guest()): the G restriction
				get is_guest() {
					return (this.security.restrictions & U.UFLAG_G) !== 0;
				},
				security: { restrictions: o.rest || 0, password: "pw" + n, password_date: 1600000000 },
				compare_ars: function (ars) {
					return o.ars_ok !== false;
				}
			};
		};
		W.add_user(W.ALICE, { alias: "alice" });
		W.add_user(W.BOB, { alias: "bob" });
		W.add_user(W.GUEST, { alias: "guest", rest: U.UFLAG_G });
		W.add_user(W.DENIED, { alias: "denied", ars_ok: false });
		var S = W.S = Kit.scratch();
		W.shared = null;
		W.open_shared = function () {
			if (W.shared !== null)
				W.shared.close();
			W.shared = Kit.email_base(S.shared_path);
			return W.shared;
		};
		W.save = function (hdr, body) {
			if (hdr.from === undefined)
				hdr.from = "Sender";
			if (hdr.to === undefined)
				hdr.to = "Rcpt";
			var mb = W.open_shared();	// fresh: the library writes the base too
			if (!mb.save_msg(hdr, body === undefined ? "body\r\n" : body))
				throw new Error("save_msg: " + mb.last_error);
			return mb.last_msg;
		};
		W.make_env = function (o) {
			o = o || {};
			var ev = {
				cookie: o.cookie,
				secure: o.secure === undefined ? true : o.secure,
				has_header: o.has_header === undefined ? true : o.has_header,
				config: o.config || W.cfg,
				accept_language: "",
				client: { ip: "192.0.2.1", agent: "Test/1.0" },
				webv4_user: function () {
					return 0;
				},
				find_user: function (name) {
					for (var n in USERS)
						if (USERS[n].alias === name)
							return USERS[n].number;
					return 0;
				},
				now: o.now,
				avatar_lib: null,
				made: 0,
				closed: 0,
				cookies: [],
				login: function (name, password) {
					for (var n in USERS)
						if (USERS[n].alias === name && USERS[n].security.password === password)
							return USERS[n].number;
					return 0;
				},
				get_user: function (n) {
					return USERS.hasOwnProperty(n) ? USERS[n] : null;
				},
				userdata: function (n) {
					return new UserData(S.user_dir(n) + "webmail/", JsonFiles);
				},
				set_cookie: function (value, keep) {
					ev.cookies.push([value, keep]);
				}
			};
			var make = o.make_ctx || Api.context_factory({ config: ev.config, get_user: ev.get_user,
				userdata: ev.userdata, Store: W.Store, shared_path: S.shared_path, data_dir: S.data_dir });
			ev.make_ctx = function (usernum) {
				ev.made++;
				var ctx = make(usernum);
				var close = ctx.close;
				ctx.close = function () {
					ev.closed++;
					close.call(ctx);
				};
				return ctx;
			};
			return ev;
		};
		// The user a cookie value was made for (sign_in, cookie_for): the
		// "as" a page started for that user sends
		var AS = {};
		W.as_of = function (ev) {
			var c = ev && ev.cookie;
			return typeof c === "string" && AS.hasOwnProperty(c) ? AS[c] : undefined;
		};
		W.call = function (ev, method, args) {
			var rq = { calls: [[method, args, "c"]] };
			if (W.as_of(ev) !== undefined)
				rq.as = W.as_of(ev);
			var r = Api.dispatch(ev, rq);
			Kit.assert(r.results instanceof Array && r.results.length === 1,
				method + ": one result: " + JSON.stringify(r));
			return r.results[0][1];
		};
		W.err = function (result, code, what) {
			Kit.eq(result.ok, false, (what || "") + " ok false: " + JSON.stringify(result));
			Kit.eq(result.error, code, (what || "") + " error code");
			Kit.eq(result.message, W.STRINGS["errors." + code], (what || "") + " localized message");
		};
		W.sign_in = function (usernum) {
			var ev = W.make_env();
			var u = USERS[usernum];
			var r = W.call(ev, "session.login", { user: u.alias, password: u.security.password });
			Kit.eq(r.ok, true, "sign in " + usernum + ": " + JSON.stringify(r));
			AS[ev.cookies[0][0]] = usernum;
			return ev.cookies[0][0];
		};
		W.cookie_for = function (usernum) {
			var ud = new UserData(S.user_dir(usernum) + "webmail/", JsonFiles);
			var c = W.Session.create(ud, usernum, { ip: "192.0.2.1", agent: "Test/1.0" }, false,
				time(), W.cfg, sha256_calc("pw" + usernum + "\n1600000000", true));
			AS[c] = usernum;
			return c;
		};
		W.cleanup = function () {
			if (W.shared !== null)
				W.shared.close();
			W.shared = null;
			S.cleanup();
		};
		return W;
	};
})();
