load({}, js.exec_dir + "request.js").serve(js.exec_dir, function userinfo(oidc) {
	oidc.method(["GET","POST"]);
	var m = /^Bearer ((?:([1-9][0-9]{0,8})\.)?[A-Za-z0-9_-]{43})$/i.exec(http_request.header.authorization || "");
	var g = m && (!m[2] || oidc.useOwner(Number(m[2]), false)) &&
	    oidc.db.query("SELECT g.*, c.ars FROM " + oidc.registry + "grants g JOIN " + oidc.registry + "clients c ON c.id=g.client " +
	    "WHERE c.enabled=1 AND g.kind='access' AND g.hash=? AND g.expires>?", [oidc.provider.digest(m[1]),time()])[0];
	var u = g && new User(g.num);
	if (!g || g.sub !== oidc.provider.subject(g.num,false) || !oidc.eligible(u,g.ars)) {
		http_reply.header["WWW-Authenticate"] = 'Bearer error="invalid_token"';
		oidc.fail("invalid_token","401 Unauthorized");
	}
	var out = {sub:g.sub}, scopes = g.scope.split(" ");
	if (scopes.indexOf("profile") >= 0) {
		out.preferred_username = u.alias;
		if (u.name) out.name = u.name;
	}
	if (scopes.indexOf("email") >= 0 && /^[^\s@]+@[^\s@]+$/.test(u.netmail)) {
		out.email = u.netmail;
		out.email_verified = false;
	}
	oidc.json(out);
});
