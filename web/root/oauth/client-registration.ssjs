// OpenID Connect Dynamic Client Registration for confidential web clients.
load({}, js.exec_dir + "request.js").serve(js.exec_dir, function registerClient(oidc) {
	oidc.method(["POST"]);
	var limit = oidc.dynamicRegistrationLimit();
	if (!limit) oidc.fail("registration_disabled", "403 Forbidden");
	if (!/^application\/json(?:\s*;|$)/i.test(http_request.header["content-type"] || ""))
		oidc.fail("invalid_request", "415 Unsupported Media Type");
	function invalid(field) {
		oidc.fail("invalid_client_metadata", "400 Bad Request", "Invalid or unsupported " + field + ".");
	}
	var body = http_request.post_data;
	if (!body || http_request.query_string) oidc.fail("invalid_request");
	if (body.length > 16384) oidc.fail("invalid_request", "413 Content Too Large");
	var data;
	try {
		data = JSON.parse(decodeURIComponent(escape(body)));
	} catch (_) {
		invalid("JSON metadata");
	}
	if (!data || typeof data !== "object" || Array.isArray(data)) invalid("JSON metadata");
	var uris = data.redirect_uris;
	if (!Array.isArray(uris) || !uris.length || uris.length > 16 || uris.some(function (uri, i) {
		if (!oidc.provider.validCallback(uri) || uris.indexOf(uri) !== i ||
		    /[^A-Za-z0-9._~:/?@!$&'()*+,;=%-]|%(?![a-fA-F0-9]{2})/.test(uri)) return true;
		var authority = uri.split("/")[2].split(":"), host = authority[0].replace(/\.$/, "");
		return (authority.length > 1 && !(Number(authority[1]) > 0 && Number(authority[1]) <= 65535)) ||
		    host.length > 253 || host.split(".").some(function (label) {
			    return !/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$/.test(label);
		    });
	})) oidc.fail("invalid_redirect_uri", "400 Bad Request", "Supply one to sixteen distinct, exact HTTPS callback URIs.");

	var metadata = {
		redirect_uris:uris, application_type:"web", grant_types:["authorization_code"], response_types:["code"],
		token_endpoint_auth_method:"client_secret_post", subject_type:"public", id_token_signed_response_alg:"RS256"
	};
	Object.keys(metadata).forEach(function (key) {
		if (key !== "redirect_uris" && data[key] !== undefined && JSON.stringify(data[key]) !== JSON.stringify(metadata[key]))
			invalid(key);
	});
	["id_token_encrypted_response_alg", "id_token_encrypted_response_enc", "userinfo_signed_response_alg",
	    "userinfo_encrypted_response_alg", "userinfo_encrypted_response_enc", "default_max_age", "default_acr_values",
	    "request_uris", "request_object_signing_alg", "request_object_encryption_alg", "request_object_encryption_enc",
	    "sector_identifier_uri"].forEach(function (key) {
		if (data[key] !== undefined) invalid(key);
	});
	if (data.require_auth_time !== undefined) {
		if (typeof data.require_auth_time !== "boolean") invalid("require_auth_time");
		metadata.require_auth_time = data.require_auth_time;
	}
	var scope = data.scope === undefined ? "openid profile email" : data.scope;
	if (typeof scope !== "string") invalid("scope");
	var scopes = scope.split(" ");
	if (scopes.indexOf("openid") < 0 || scopes.some(function (s) { return ["openid","profile","email"].indexOf(s) < 0; }))
		invalid("scope");
	metadata.scope = scope;
	var name = data.client_name === undefined ? uris[0].split("/")[2] : data.client_name;
	if (typeof name !== "string" || !name.trim() || name.length > 256 || /[\x00-\x1f\x7f]/.test(name))
		invalid("client_name");
	try {
		// The SSJS JSON and HTML writers consume UTF-8 bytes.
		metadata.client_name = unescape(encodeURIComponent(name));
	} catch (_) {
		invalid("client_name");
	}

	oidc.db.transaction(function () {
		oidc.db.exec("CREATE TABLE IF NOT EXISTS dynamic_clients (" +
		    "id TEXT PRIMARY KEY REFERENCES clients(id) ON DELETE CASCADE, name TEXT NOT NULL, scope TEXT NOT NULL, issued INTEGER NOT NULL);" +
		    "CREATE INDEX IF NOT EXISTS dynamic_clients_issued ON dynamic_clients(issued)");
		var now = time();
		if (oidc.db.query("SELECT count(*) AS n FROM dynamic_clients WHERE issued>?", [now-3600])[0].n >= limit) {
			http_reply.header["Retry-After"] = "3600";
			oidc.fail("temporarily_unavailable", "429 Too Many Requests");
		}
		var id = "dyn~" + oidc.provider.token(), secret = oidc.provider.token();
		oidc.db.run("INSERT INTO clients(id,secret,ars) VALUES(?,?,'')", [id,oidc.provider.digest(secret)]);
		uris.forEach(function (uri) { oidc.db.run("INSERT INTO callbacks(client,uri) VALUES(?,?)", [id,uri]); });
		oidc.db.run("INSERT INTO dynamic_clients(id,name,scope,issued) VALUES(?,?,?,?)", [id,metadata.client_name,scope,now]);
		metadata.client_id = id;
		metadata.client_secret = secret;
		metadata.client_id_issued_at = now;
		metadata.client_secret_expires_at = 0;
	});
	http_reply.status = "201 Created";
	oidc.json(metadata);
});
