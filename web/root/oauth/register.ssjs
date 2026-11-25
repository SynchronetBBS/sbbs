load({}, js.exec_dir + "request.js").serve(js.exec_dir, function register(oidc) {
	oidc.method(["GET", "POST"]);
	var post = http_request.method === "POST", action;
	if (post) {
		oidc.formPost();
		oidc.validateForm();
		action = oidc.param("action");
	}
	var server = oidc.serverSession(), owner, session, identity;
	if (server !== undefined) {
		if (action === "login") oidc.fail("invalid_request");
		if (server.auth_time === null) return server.request_login("/oauth/register.ssjs", false);
		owner = user;
	} else if (action === "login") {
		if (!login(oidc.param("username"), oidc.param("password"), false) || !oidc.eligible(user, ""))
			return loginForm("Sign-in failed.");
		var raw = oidc.db.transaction(function () {
			return oidc.saveSession(user, oidc.provider.subject(user.number, true), null, null, time());
		});
		oidc.setCookie(oidc.sessionCookie, raw);
		return back();
	} else {
		session = oidc.validSession();
		if (session) owner = new User(session.num);
		else if ((identity = oidc.webIdentity())) owner = new User(identity.num);
	}
	if (!owner) return loginForm(post ? "Sign in again to manage your applications." : null);
	if (!oidc.eligible(owner, "")) oidc.fail("access_denied", "403 Forbidden");

	var name = "", uri = "", message, id, secret;
	if (action === "create") {
		name = oidc.param("name").trim();
		uri = oidc.param("redirect_uri");
		if (!name || name.length > 128) message = "Enter an application name of up to 128 characters.";
		else if (!oidc.provider.validCallback(uri)) message = "Enter an exact HTTPS callback URL without a fragment.";
	}
	var create = action === "create" && !message;
	var attached = oidc.useOwner(owner.number, create);
	if (create && !attached) oidc.fail("access_denied", "403 Forbidden");
	if (post && !message) {
		oidc.db.transaction(function () {
			if (create) {
				if (oidc.db.query("SELECT 1 FROM apps.clients WHERE name=?", [name]).length) {
					message = "You already have an application with this name.";
					return;
				}
				id = "user:" + owner.number + ":" + oidc.provider.token();
				secret = oidc.provider.token();
				oidc.db.run("INSERT INTO apps.clients(id,secret,ars,name) VALUES(?,?,'',?)", [id,oidc.provider.digest(secret),name]);
				oidc.db.run("INSERT INTO apps.callbacks(client,uri) VALUES(?,?)", [id,uri]);
			} else {
				if (action !== "rotate" && action !== "remove") oidc.fail("invalid_request");
				id = oidc.param("client_id");
				if (!attached) oidc.fail("unknown_client", "404 Not Found");
				if (action === "rotate") {
					secret = oidc.provider.token();
					oidc.db.run("UPDATE apps.clients SET secret=? WHERE id=?", [oidc.provider.digest(secret),id]);
				} else oidc.db.run("DELETE FROM apps.clients WHERE id=?", [id]);
				if (!oidc.db.changes) oidc.fail("unknown_client", "404 Not Found");
			}
		});
		if (action === "remove") return back();
	}
	var csrf = oidc.formToken();
	var rows = attached ? oidc.db.query("SELECT id,name,uri FROM apps.clients JOIN apps.callbacks ON client=id ORDER BY name") : [];
	oidc.page("Your OpenID applications");
	if (message) write('<p role="alert">' + oidc.html(message) + '</p>');
	if (secret) {
		write('<h2>Client credentials</h2><p>Copy the secret now. It is shown only once.</p><dl><dt>Client ID</dt><dd><code>' +
		    oidc.html(id) + '</code></dd><dt>Client secret</dt><dd><code>' + secret + '</code></dd></dl>');
		name = uri = "";
	}
	rows.forEach(function (app) {
		write('<section><h2>' + oidc.html(app.name) + '</h2><dl><dt>Client ID</dt><dd><code>' + oidc.html(app.id) +
		    '</code></dd><dt>Callback URL</dt><dd><code>' + oidc.html(app.uri) + '</code></dd></dl>');
		startForm(csrf);
		write('<input type="hidden" name="client_id" value="' + oidc.html(app.id) +
		    '"><button name="action" value="rotate">Replace secret</button> <button name="action" value="remove">Remove application</button></form></section>');
	});
	write('<h2>Register an application</h2>');
	startForm(csrf);
	write('<p><label>Application name <input name="name" maxlength="128" value="' + oidc.html(name) +
	    '" required></label></p><p><label>HTTPS callback URL <input type="url" name="redirect_uri" value="' + oidc.html(uri) +
	    '" required></label></p><button name="action" value="create">Register application</button></form></main></html>');

	function startForm(token) {
		write('<form action="/oauth/register.ssjs" method="post"><input type="hidden" name="csrf" value="' + token + '">');
	}
	function loginForm(error) {
		var token = oidc.formToken();
		oidc.page("Sign in to register an application");
		if (error) write('<p role="alert">' + oidc.html(error) + '</p>');
		startForm(token);
		oidc.passwordFields();
		write('<button name="action" value="login">Sign in</button></form></main></html>');
	}
	function back() {
		http_reply.status = "303 See Other";
		http_reply.header.Location = "/oauth/register.ssjs";
	}
});
