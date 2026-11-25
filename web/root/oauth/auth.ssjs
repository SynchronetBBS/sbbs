load({}, js.exec_dir + "request.js").serve(js.exec_dir, function authorize(oidc) {
	oidc.method(["GET","POST"]);
	var post = http_request.method === "POST";
	var server = oidc.serverSession();
	if (post) {
		oidc.formPost();
		if (server === undefined) oidc.validateForm();
		else if (http_request.query.username !== undefined || http_request.query.password !== undefined)
			oidc.fail("invalid_request");
	}
	var r = {client_id:oidc.param("client_id"), redirect_uri:oidc.param("redirect_uri"), state:oidc.param("state", true)};
	var app = oidc.selectClient(r.client_id) && oidc.registered(r.client_id);
	if (!app || !oidc.db.query("SELECT 1 FROM " + oidc.registry + "callbacks WHERE client=? AND uri=?", [app.id,r.redirect_uri]).length)
		oidc.fail("invalid_request");
	r.response_type = oidc.param("response_type");
	r.scope = oidc.param("scope");
	r.nonce = oidc.param("nonce", true);
	r.code_challenge_method = oidc.param("code_challenge_method", true);
	r.code_challenge = oidc.param("code_challenge", true);
	r.prompt = oidc.param("prompt", true);
	if (r.response_type !== "code") return redirect(r, {error:"unsupported_response_type"});
	var scopes = r.scope.split(" ");
	if (scopes.indexOf("openid") < 0 || scopes.some(function (s) { return ["openid","profile","email"].indexOf(s) < 0; }))
		return redirect(r, {error:"invalid_scope"});
	if (app.scope && scopes.some(function (s) { return app.scope.split(" ").indexOf(s) < 0; }))
		return redirect(r, {error:"invalid_scope"});
	if (r.code_challenge_method !== "S256" || !/^[A-Za-z0-9_-]{43}$/.test(r.code_challenge || "") ||
	    (r.prompt && r.prompt !== "none" && r.prompt !== "login"))
		return redirect(r, {error:"invalid_request"});

	var consentApp = app.name ? app : null;
	var decision = post && consentApp && oidc.param("decision", true);
	if (post && consentApp) {
		if (server !== undefined) oidc.validateForm();
		if (decision === "deny") return redirect(r, {error:"access_denied"});
		if (decision !== "allow") oidc.fail("invalid_request");
	}
	var s, u, source, authTime;
	if (server !== undefined) {
		authTime = server.auth_time;
		if (authTime === null || r.prompt === "login") {
			if (r.prompt === "none") return redirect(r, {error:"login_required"});
			// Resume at the provider after the requested sign-in has completed.
			// The forced login has then been satisfied, so omit prompt on return.
			var parts = Object.keys(r).filter(function (k) {
				return k !== "prompt" && r[k] !== null;
			}).map(function (k) { return k + "=" + encodeURIComponent(r[k]); });
			return server.request_login("/oauth/auth.ssjs?" + parts.join("&"), r.prompt === "login");
		}
		u = user;
	} else if (post && (!consentApp || http_request.query.username !== undefined)) {
		if (r.prompt === "none") return redirect(r, {error:"login_required"});
		var name = oidc.param("username"), password = oidc.param("password", true) || "";
		if (!login(name, password, false)) return oidc.form(r, "Sign-in failed.", consentApp);
		u = user;
	} else if (r.prompt !== "login") {
		s = oidc.validSession();
		if (s) u = new User(s.num);
		else if ((source = oidc.webIdentity())) u = new User(source.num);
	}
	if (server === undefined && !u) {
		if (r.prompt === "none") return redirect(r, {error:"login_required"});
		return oidc.form(r, null, consentApp);
	}
	if (!oidc.eligible(u, app.ars)) return redirect(r, {error:"access_denied"});

	if (consentApp && !decision) {
		if (r.prompt === "none") return redirect(r, {error:"consent_required"});
		return oidc.form(r, null, consentApp, true);
	}

	var code = oidc.provider.token(), raw, now = time();
	oidc.db.transaction(function () {
		var sub = oidc.provider.subject(u.number, true);
		oidc.db.run("DELETE FROM " + oidc.registry + "grants WHERE expires<=?", [now]);
		if (server === undefined) {
			authTime = s ? s.created : now;
			raw = oidc.saveSession(u, sub, source, s, now);
		}
		oidc.db.run("INSERT INTO " + oidc.registry + "grants VALUES(?,?,?,?,?,?,?,?,?,?,?)", [oidc.provider.digest(code),"code",app.id,r.redirect_uri,
		    r.code_challenge,r.scope,r.nonce,u.number,sub,authTime,now+600]);
	});
	if (raw) oidc.setCookie(oidc.sessionCookie, raw);
	redirect(r, {code:code});

	function redirect(r, fields)
	{
		if (r.state !== null) fields.state = r.state;
		var parts = Object.keys(fields).map(function (k) { return k + "=" + encodeURIComponent(fields[k]); });
		http_reply.status = "302 Found";
		http_reply.header.Location = r.redirect_uri + (r.redirect_uri.indexOf("?") < 0 ? "?" : "&") + parts.join("&");
	}
});
