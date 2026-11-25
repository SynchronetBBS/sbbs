load({}, js.exec_dir + "request.js").serve(js.exec_dir, function logout(oidc) {
	oidc.method(["GET","POST"]);
	if (http_request.method === "GET") return oidc.form(null);
	oidc.formPost(); oidc.validateForm();
	var server = oidc.serverSession();
	if (server !== undefined) server.logout();
	else {
		var s = oidc.validSession();
		oidc.db.run("DELETE FROM sessions WHERE hash=?", [oidc.provider.digest(oidc.cookie(oidc.sessionCookie))]);
		if (s && s.webkey && !oidc.web().end_session(http_request.cookie,system.data_dir,s.num))
			throw new Error("Could not end source session");
		oidc.setCookie(oidc.sessionCookie, "");
	}
	http_reply.header["Content-Type"] = "text/plain";
	write("Signed out of Synchronet.");
});
