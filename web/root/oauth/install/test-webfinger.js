// Run in a Synchronet JS runtime from the source checkout.
"use strict";
var endpoint = js.exec_dir + "webfinger.ssjs";
var issuerFile = js.exec_dir + "../issuer.js";
var optionsFile = "finger_options.js";
var ns = "https://synchro.net/ns/finger/";
var oidc = "http://openid.net/specs/connect/1.0/issuer";
var profile = "http://webfinger.net/rel/profile-page";
var canonical = "acct:Test%20%25%2F%C3%89@bbs.example";
var alternate = canonical.replace("bbs.example", "login.example");
var acct = {number: 42, alias: "Test %/\x90", name: "Real Name", handle: "Handle",
	email: "local@bbs.example", netmail: "private@example.test", location: "Montr\x82al",
	age: 23, gender: "F", settings: 0};
var finger, web, serverHost = "login.example", port = 443, hasPlan = true;

function check(ok, label) {
	if (!ok) throw new Error(label);
}
function options(module) {
	if (module === "oidc") return {https_port:port};
	return module === "fingerservice" ? finger : web;
}
function request(resource, rel, method) {
	var q = {}, body = "", reply = {header: {}};
	if (resource !== undefined)
		q.resource = Array.isArray(resource) ? resource : [resource];
	if (rel !== undefined) q.rel = rel;
	var scope = {
		http_request: {method: method || "GET", query: q}, http_reply: reply,
		js: {exec_dir:"/.well-known/"},
		system: {ctrl_dir: "/ctrl/", data_dir: "/data/", host_name: serverHost, inet_addr: "bbs.example",
			matchuser: function (name) { return name === acct.alias ? 42 : 0; }},
		User: function () { return acct; },
		file_exists: function (p) { return hasPlan && p === "/data/user/0042.plan"; },
		load: function (file, name) {
			if (file === "finger_options.js")
				return load({load: function (_, module) { return options(module); }}, optionsFile);
			if (name === "/.well-known/../oauth/issuer.js") return load({}, issuerFile);
			if (file === "modopts.js") return options(name);
			throw new Error("Unexpected load: " + file);
		},
		require: function () {}, USER_DELETED: 1, USER_INACTIVE: 64,
		write: function (s) { body += s; }, writeln: function (s) { body += s + "\n"; }
	};
	load(scope, endpoint);
	reply.body = body;
	if (reply.status === "200 OK" && body)
		reply.doc = JSON.parse(decodeURIComponent(escape(body)));
	return reply;
}
var r = request(alternate);
check(r.doc.subject === canonical && r.doc.aliases.join() === alternate, "canonical and alternate account URIs");
check(r.header["Access-Control-Allow-Origin"] === "*" && r.header["Content-Type"] === "application/jrd+json", "headers");
check(r.doc.links.length === 1 && r.doc.links[0].rel === oidc && r.doc.links[0].href === "https://bbs.example", "primary domain issuer");
var props = r.doc.properties;
check(props[ns+"location"] === "Montr\u00e9al" && props[ns+"age"] === "23" && props[ns+"gender"] === "F"
	&& props[ns+"real-name"] === acct.name && props[ns+"email"] === acct.email && props[ns+"handle"] === acct.handle, "profile fields and encoding");
check(r.body.indexOf(acct.netmail) < 0, "private address");
["age", "gender", "location", "real-name"].forEach(function (field) {
	finger = {};
	finger["include_" + field.replace("-", "_")] = false;
	var p = request(canonical).doc.properties;
	check(!(ns + field in p) && Object.keys(p).length === 5, "visibility for " + field);
});
finger = {include_real_name: false, include_location: false, include_age: false, include_gender: false};
web = {profile_url: "https://www.example/users/{alias}", plan_url: "https://www.example/u/{number}/plans/{number4}"};
r = request(canonical, [profile, ns+"plan"]);
check(Object.keys(r.doc.properties).length === 2 && r.body.indexOf("Real Name") < 0, "hidden profile");
check(r.doc.links.length === 2 && r.doc.links[0].href === "https://www.example/users/Test%20%25%2F%C3%89"
	&& r.doc.links[1].href === "https://www.example/u/42/plans/0042", "link expansion and rel filtering");
check(request(canonical, ["self"]).doc.links.length === 0, "unknown relation");
hasPlan = false;
check(request(canonical, [ns+"plan"]).doc.links.length === 0, "missing plan");
web = {profile_url: "https://example/{unknown}"};
check(request(canonical, [profile]).doc.links.length === 0, "invalid template");
port = 8443;
check(request(canonical).doc.links[0].href === "https://bbs.example:8443", "non-default TLS port");
port = 443;
serverHost = "bbs.example";
r = request(canonical);
check(!r.doc.aliases && r.doc.links[0].href === "https://bbs.example", "standalone host");
check(request(canonical, undefined, "HEAD").body === "", "HEAD");
check(request(canonical, undefined, "POST").status === "405 Method Not Allowed", "POST");
[undefined, [canonical, canonical], "acct:@bbs.example", "acct:a%b@bbs.example", "acct:%00@bbs.example", "acct:a@b@c"].forEach(function (bad) {
	check(request(bad).status === "400 Bad Request", "malformed resource");
});
[alternate, "acct:Unknown@bbs.example", "https://bbs.example/profile"].forEach(function (unknown) {
	check(request(unknown).status === "404 Not Found", "unknown resource");
});
[1, 64].forEach(function (flags) {
	acct.settings = flags;
	check(request(canonical).status === "404 Not Found", "unavailable account");
});
print("Passed WebFinger tests");
