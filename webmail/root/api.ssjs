// api.ssjs: the web client's JSON endpoint.  A thin shim: it gathers the
// request, hands it to Api.dispatch() (webmail/lib/api.js, where the rules
// and the tests are) and writes the reply.
//
// POST only (anything else: 405; OPTIONS never reaches this page: the web
// server answers it without CORS headers, so a cross-origin request with
// the X-Webmail header fails its preflight), a UTF-8 JSON body (not UTF-8
// or JSON: Api's top-level "invalid") of at most 256 KB, or, for a request
// of exactly one draft.attach or draft.save call, of at most what that
// call needs (Api.body_check, Api.body_allowance; larger: 413).  A body
// over 256 KB is held to that rule before it is parsed: the size against
// Content-Length and the method the body starts with, then the header,
// HTTPS and sign-in gates (Api.large_gate), so a refused one is never
// copied or parsed; dispatch checks the parsed request again.  The web
// server itself reads the whole body before this page runs, and gives no
// post_data for a body over 4 MiB (MAX_POST_LEN in websrvr.cpp).  The
// reply is application/json, never cached, with the session cookie
// ("webmail") set or cleared as Api asks.  Anything that goes wrong here is
// a fixed 500 reply and a log entry.
//
// The libraries are found as index.xjs finds them: config.js beside this
// directory (../lib/) or in the default place, then every library from
// the "directory" setting when it has them.  Each is loaded once per
// request, and only when needed: the mailbox library only by a call that
// opens the mailbox, shell.js only when the product name is used.

"use strict";

(function () {
	var COOKIE = "webmail";
	// The web server's own limit (MAX_POST_LEN in websrvr.cpp): no body
	// beyond it is ever accepted, so it is refused before anything loads
	var HOST_MAX_POST = 4 * 1048576;

	// Where config.js is read from: beside this directory, else the
	// default place
	function find_lib()
	{
		var c = [js.exec_dir + "../lib/", system.exec_dir + "../webmail/lib/"];
		for (var i = 0; i < c.length; i++)
			if (file_exists(c[i] + "config.js") && file_exists(c[i] + "api.js"))
				return c[i];
		return c[c.length - 1];
	}

	function send(status, body)
	{
		http_reply.status = status;
		http_reply.header["Content-Type"] = "application/json; charset=utf-8";
		http_reply.header["Cache-Control"] = "no-store";
		http_reply.header["X-Content-Type-Options"] = "nosniff";
		http_reply.header["Content-Length"] = String(body.length);
		write(body);
	}

	try {
		if (http_request.method !== "POST") {
			http_reply.header["Allow"] = "POST";
			send("405 Method Not Allowed", "{\"error\":\"method\"}");
			return;
		}
		var headers = http_request.header;
		var length = parseInt(headers["content-length"], 10);
		if (length > HOST_MAX_POST) {
			send("413 Payload Too Large", "{\"error\":\"toolarge\"}");
			return;
		}
		var lib = find_lib();
		var Config = load({}, lib + "config.js");
		var config = Config.load();
		if (file_exists(config.directory + "lib/api.js"))
			lib = config.directory + "lib/";
		var Api = load({}, lib + "api.js");
		// The size rule before anything else touches the body
		var body = http_request.post_data;
		var size = Api.body_check(config, length, body);
		var body_length = typeof body === "string" ? body.length : 0;
		if (!size.ok) {
			send("413 Payload Too Large", "{\"error\":\"toolarge\"}");
			return;
		}
		var Session = Api.Session;
		var UserData = null;
		// {system} and {host} in the product name, as the shell page does;
		// shell.js is loaded only when the name is used (not by the poll)
		var raw_name = config.product_name;
		var product_name = null;
		Object.defineProperty(config, "product_name", { enumerable: true, configurable: true,
			get: function () {
				if (product_name === null)
					product_name = load({}, lib + "shell.js").product_name(raw_name, headers.host);
				return product_name;
			},
			set: function (v) {
				product_name = v;
			} });
		var scheme = String(client.protocol).toLowerCase() === "https" ? "https" : "http";
		var secure = Session.is_secure(scheme, headers, config);
		var path = Api.base_path(http_request.virtual_path);
		var set = null;

		// One User object per user number for the request (the session
		// check and the mailbox both ask: each new one reads the record)
		var users = {};
		var get_user = function (n) {
			if (users.hasOwnProperty(n))
				return users[n];
			var u = null;
			try {
				u = new User(n);
				if (u.number !== n)
					u = null;
			} catch (e) {
				u = null;
			}
			users[n] = u;
			return u;
		};
		var userdata = function (n) {
			if (UserData === null)
				UserData = load({}, lib + "userdata.js");
			return new UserData(Api.userdata_dir(system.data_dir, n));
		};

		var env = {
			config: config,
			has_header: headers["x-webmail"] === "1",
			secure: secure,
			// every value sent: one per path the browser holds a cookie for
			cookie: http_request.cookie[COOKIE],
			accept_language: typeof headers["accept-language"] === "string"
				? headers["accept-language"] : "",
			client: { ip: http_request.remote_ip, agent: headers["user-agent"] },
			// read only when needed (session.get without a session, logout)
			webv4_user: function () {
				return load({}, lib + "webv4.js").session_user(http_request.cookie, system.data_dir);
			},
			// a session started from webv4 lasts only while its key is unchanged
			webv4_key: function (n) {
				return load({}, lib + "webv4.js").stored_key(n, system.data_dir);
			},
			find_user: function (name) {
				// the engine's lookup, the same one login() uses
				return system.find_login_id(name);
			},
			// The web server's login() counts and logs failed attempts
			login: function (name, password) {
				return login(name, password) ? user.number : 0;
			},
			get_user: get_user,
			userdata: userdata,
			make_ctx: Api.context_factory({ config: config, get_user: get_user,
				userdata: userdata, load_store: function () {
					return load({}, "mailbox.js");
				} }),
			set_cookie: function (value, keep) {
				set = { value: value, keep: keep };
			}
		};
		var result = null;
		// A large body: the gates first, so a refused one is never parsed
		if (size.method !== null)
			result = Api.large_gate(env, body);
		if (result === null) {
			// Valid UTF-8 and JSON, every string as UTF-8 bytes; null
			// (answered with the top-level "invalid") otherwise
			var request = Api.parse_body(body);
			body = null;
			result = Api.dispatch(env, request, { length: body_length });
			if (result.error === "toolarge") {
				send("413 Payload Too Large", "{\"error\":\"toolarge\"}");
				return;
			}
		}
		if (set !== null)
			http_reply.header["Set-Cookie"] = COOKIE + "=" + set.value
				+ Session.cookie_header(set.value, { secure: secure, keep: set.keep,
					config: config, path: path });
		send("200 OK", JSON.stringify(result));
	} catch (e) {
		log(LOG_ERR, "webmail api.ssjs: " + e + (e && e.fileName ? " (" + e.fileName + ":"
			+ e.lineNumber + ")" : ""));
		send("500 Internal Server Error", "{\"error\":\"internal\"}");
	}
})();
