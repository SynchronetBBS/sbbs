load({}, js.exec_dir + "request.js").serve(js.exec_dir, function jwks(oidc) {
	oidc.method(["GET","HEAD"]);
	oidc.json({keys:oidc.provider.signingKeys().map(oidc.provider.jwk)});
});
