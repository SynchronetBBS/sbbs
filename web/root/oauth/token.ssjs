load({}, js.exec_dir + "request.js").serve(js.exec_dir, function redeem(oidc) {
	oidc.method(["POST"]); oidc.formPost();
	if (oidc.param("grant_type") !== "authorization_code") oidc.fail("unsupported_grant_type");
	var id = oidc.param("client_id"), secret = oidc.param("client_secret"), code = oidc.param("code");
	var uri = oidc.param("redirect_uri"), verifier = oidc.param("code_verifier");
	if (!oidc.selectClient(id)) oidc.fail("invalid_client");
	var response = oidc.db.transaction(function () {
		var app = oidc.registered(id);
		if (!app || !oidc.provider.equal(oidc.provider.digest(secret), app.secret)) oidc.fail("invalid_client");
		var g = oidc.db.query("SELECT * FROM " + oidc.registry + "grants WHERE kind='code' AND client=? AND hash=? AND expires>?",
		    [id,oidc.provider.digest(code),time()])[0];
		if (!g || g.uri !== uri || !/^[A-Za-z0-9._~-]{43,128}$/.test(verifier) ||
		    !oidc.provider.equal(oidc.provider.url64(sha256_calc(verifier)), g.challenge) ||
		    g.sub !== oidc.provider.subject(g.num,false) || !oidc.eligible(new User(g.num),app.ars)) oidc.fail("invalid_grant");
		var access = oidc.provider.token(), now = time();
		if (oidc.ownerNumber) access = oidc.ownerNumber + "." + access;
		var claims = {iss:oidc.issuer,sub:g.sub,aud:id,exp:now+3600,iat:now,auth_time:g.auth_time};
		if (g.nonce !== null) claims.nonce = g.nonce;
		var signed = oidc.provider.sign(claims,oidc.provider.signingKey(false));
		oidc.db.run("UPDATE " + oidc.registry + "grants SET kind='access', hash=?, expires=? WHERE hash=?",
		    [oidc.provider.digest(access),now+3600,g.hash]);
		return {access_token:access,token_type:"Bearer",expires_in:3600,scope:g.scope,id_token:signed};
	});
	oidc.json(response);
});
