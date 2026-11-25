load({}, js.exec_dir + "../oauth/request.js").serve(js.exec_dir + "../oauth/", function discovery(oidc) {
	oidc.method(["GET","HEAD"]);
	var issuer = oidc.issuer;
	var metadata = {
		issuer: issuer,
		authorization_endpoint: issuer + "/oauth/auth.ssjs",
		token_endpoint: issuer + "/oauth/token.ssjs",
		userinfo_endpoint: issuer + "/oauth/userinfo.ssjs",
		jwks_uri: issuer + "/oauth/jwks.ssjs",
		response_types_supported: ["code"],
		grant_types_supported: ["authorization_code"],
		subject_types_supported: ["public"],
		scopes_supported: ["openid", "profile", "email"],
		id_token_signing_alg_values_supported: oidc.provider.signingKeys().map(function (k) { return k.alg; }),
		token_endpoint_auth_methods_supported: ["client_secret_post"],
		code_challenge_methods_supported: ["S256"]
	};
	if (oidc.dynamicRegistrationLimit()) metadata.registration_endpoint = issuer + "/oauth/client-registration.ssjs";
	oidc.json(metadata);
});
