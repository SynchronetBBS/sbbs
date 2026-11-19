// part.ssjs: one part of a message, for viewing or download.  A thin shim:
// the rules (parameters, sign-in, headers, file names, streaming) and their
// tests are in webmail/lib/download.js; this file gathers the request and
// writes the reply.
//
// GET ?id=<message id>&pid=<part id>&dl=<0|1>
//
// The libraries are found as index.xjs finds them (see api.ssjs) and each
// is loaded once.

"use strict";

// Temp files to remove when the script ends, also if the web server ends
// it early (the client went away): js.on_exit runs then too
var webmail_part_temp = [];
js.on_exit("for (var i = 0; i < webmail_part_temp.length; i++) file_remove(webmail_part_temp[i]);");

(function () {
	var COOKIE = "webmail";
	var headers_sent = false;

	// Where config.js is read from: beside this directory, else the
	// default place
	function find_lib()
	{
		var c = [js.exec_dir + "../lib/", system.exec_dir + "../webmail/lib/"];
		for (var i = 0; i < c.length; i++)
			if (file_exists(c[i] + "config.js") && file_exists(c[i] + "download.js"))
				return c[i];
		return c[c.length - 1];
	}

	try {
		var lib = find_lib();
		var Config = load({}, lib + "config.js");
		var config = Config.load();
		if (file_exists(config.directory + "lib/download.js"))
			lib = config.directory + "lib/";
		var Api = load({}, lib + "api.js");
		var Session = Api.Session;
		var UserData = load({}, lib + "userdata.js");
		var Download = load({}, lib + "download.js");

		var headers = http_request.header;
		var scheme = String(client.protocol).toLowerCase() === "https" ? "https" : "http";
		var secure = Session.is_secure(scheme, headers, config);
		var path = Api.base_path(http_request.virtual_path);

		var get_user = function (n) {
			try {
				var u = new User(n);
				return u.number === n ? u : null;
			} catch (e) {
				return null;
			}
		};
		var userdata = function (n) {
			return new UserData(Api.userdata_dir(system.data_dir, n));
		};

		var env = {
			method: http_request.method,
			config: config,
			secure: secure,
			cookie: http_request.cookie[COOKIE],	// every value sent
			get_user: get_user,
			userdata: userdata,
			make_ctx: Api.context_factory({ config: config, get_user: get_user,
				userdata: userdata, load_store: function () {
					return load({}, "mailbox.js");
				} }),
			set_cookie: function (value, keep) {
				http_reply.header["Set-Cookie"] = COOKIE + "=" + value
					+ Session.cookie_header(value, { secure: secure, keep: keep,
						config: config, path: path });
			}
		};
		var reply = {
			status: function (s) {
				http_reply.status = s;
			},
			header: function (name, value) {
				http_reply.header[name] = value;
			},
			write: function (bytes) {
				headers_sent = true;
				write(bytes);
			},
			track: function (p) {
				webmail_part_temp.push(p);
			}
		};
		Download.serve(env, http_request.query, reply, Api);
	} catch (e) {
		log(LOG_ERR, "webmail part.ssjs: " + e + (e && e.fileName ? " (" + e.fileName + ":"
			+ e.lineNumber + ")" : ""));
		if (!headers_sent) {
			http_reply.status = "500 Internal Server Error";
			http_reply.header["Content-Type"] = "text/plain; charset=utf-8";
			http_reply.header["Cache-Control"] = "no-store";
			http_reply.header["X-Content-Type-Options"] = "nosniff";
			write("Server error");
		}
	}
})();
