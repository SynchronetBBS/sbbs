// The public issuer comes from BBS configuration, independently of the request.
"use strict";
(function (domain, options) {
	if (!domain) throw new Error("Missing BBS Internet address");
	var port = options && options.https_port;
	if (port == null) port = 443;
	if (typeof port !== "number" || port < 1 || port > 65535 || port !== Math.floor(port))
		throw new Error("Invalid [oidc] https_port");
	return "https://" + domain.toLowerCase() + (port === 443 ? "" : ":" + port);
});
